#include "app.h"

#include "calibration.h"
#include "heater.h"
#include "ina238.h"
#include "main.h"
#include "mcp9808.h"
#include "safety_guard.h"
#include "storage.h"

#include <string.h>

enum
{
  STATION_TIP_OVERTEMP_LIMIT_CDEG = 4500U,
  STATION_AMBIENT_OVERTEMP_LIMIT_CDEG = 800U,
  STATION_AMBIENT_SENSOR_FAULT_TIMEOUT_MS = 5000U,
  STATION_RUNSTATE_DEBOUNCE_MS = 500U,
  STATION_HEATUP_ENTER_BAND_CDEG = 90U,
  STATION_HEATUP_EXIT_BAND_CDEG = 20U,
  STATION_COOLDOWN_ENTER_BAND_CDEG = 120U,
  STATION_COOLDOWN_EXIT_BAND_CDEG = 40U,
  STATION_STANDBY_TARGET_CDEG = 1800U
};

static StationContext station_context;
static uint32_t station_forced_fault_flags;
static uint32_t station_ambient_sensor_fault_since_ms;
static uint32_t station_pending_runstate_since_ms;
static uint8_t station_simulated_docked;
static StationState station_pending_state;
static StationOperatingMode station_pending_operating_mode;

static uint32_t Station_FilterCriticalFaults(uint32_t fault_flags)
{
  return fault_flags & STATION_FAULT_CRITICAL_MASK;
}

static uint32_t Station_MapIgnoredFaultsToWarnings(uint32_t fault_flags)
{
  uint32_t warning_flags = 0U;

  if ((fault_flags & STATION_FAULT_BUCKBOOST) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_BUCKBOOST_IGNORED;
  }

  if ((fault_flags & STATION_FAULT_OPAMP_PGOOD) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_OPAMP_PGOOD_IGNORED;
  }

  if ((fault_flags & STATION_FAULT_TIP_OVERTEMP) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_TIP_OVERTEMP_IGNORED;
  }

  if ((fault_flags & STATION_FAULT_AMBIENT_OVERTEMP) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_AMBIENT_OVERTEMP_IGNORED;
  }

  if ((fault_flags & STATION_FAULT_AMBIENT_SENSOR) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_AMBIENT_SENSOR_IGNORED;
  }

  if ((fault_flags & STATION_FAULT_INJECTED) != 0U)
  {
    warning_flags |= STATION_WARNING_FAULT_INJECTED_IGNORED;
  }

  return warning_flags;
}

static uint8_t Station_ReadDockInput(void)
{
#if IRON_VIRTUAL_DOCK_INPUT
  return station_simulated_docked;
#else
  return HAL_GPIO_ReadPin(DOCK_CHECK_GPIO_Port, DOCK_CHECK_Pin) == DOCK_CHECK_DOCKED_LEVEL ? 1U : 0U;
#endif
}

static void Station_UpdateControlTarget(const HeaterControlContext *heater)
{
  uint16_t effective_target_temp_cdeg;

  station_context.docked = Station_ReadDockInput();
  station_context.standby_active = 0U;

  if (heater == NULL)
  {
    station_context.effective_target_temp_cdeg = STATION_STANDBY_TARGET_CDEG;
    return;
  }

  effective_target_temp_cdeg = heater->target_temp_cdeg;

  if (station_context.docked != 0U)
  {
    effective_target_temp_cdeg = STATION_STANDBY_TARGET_CDEG;
    station_context.standby_active = 1U;
  }

  station_context.effective_target_temp_cdeg = effective_target_temp_cdeg;
  Heater_Control_SetEffectiveTargetTempCdeg(effective_target_temp_cdeg);
}

static void Station_UpdateRunState(const HeaterControlContext *heater, uint32_t now_ms)
{
  StationState desired_state;
  StationOperatingMode desired_operating_mode;
  uint16_t effective_target_temp_cdeg;

  if (heater == NULL)
  {
    station_context.state = STATION_STATE_IDLE;
    station_context.operating_mode = STATION_OPERATING_MODE_IDLE;
    return;
  }

  if (Calibration_IsBringUpSessionActive())
  {
    station_context.state = STATION_STATE_CALIBRATION;
    station_context.operating_mode = STATION_OPERATING_MODE_CALIBRATION;
    station_pending_state = station_context.state;
    station_pending_operating_mode = station_context.operating_mode;
    station_pending_runstate_since_ms = now_ms;
    return;
  }

  if (station_context.calibration_valid == 0U)
  {
    station_context.state = (station_context.docked != 0U) ? STATION_STATE_STANDBY : STATION_STATE_NO_CALIBRATION;
    station_context.operating_mode = (station_context.docked != 0U) ? STATION_OPERATING_MODE_STANDBY : STATION_OPERATING_MODE_IDLE;
    station_pending_state = station_context.state;
    station_pending_operating_mode = station_context.operating_mode;
    station_pending_runstate_since_ms = now_ms;
    return;
  }

  if (station_context.docked != 0U)
  {
    desired_state = STATION_STATE_STANDBY;

    effective_target_temp_cdeg = heater->effective_target_temp_cdeg;

    if ((station_context.operating_mode == STATION_OPERATING_MODE_HEATUP) ||
        (station_context.operating_mode == STATION_OPERATING_MODE_STANDBY))
    {
      if (heater->tip_temp_cdeg + STATION_HEATUP_EXIT_BAND_CDEG < effective_target_temp_cdeg)
      {
        desired_operating_mode = STATION_OPERATING_MODE_HEATUP;
      }
      else
      {
        desired_operating_mode = STATION_OPERATING_MODE_STANDBY;
      }
    }
    else if (station_context.operating_mode == STATION_OPERATING_MODE_COOLDOWN)
    {
      if (heater->tip_temp_cdeg > effective_target_temp_cdeg + STATION_COOLDOWN_EXIT_BAND_CDEG)
      {
        desired_operating_mode = STATION_OPERATING_MODE_COOLDOWN;
      }
      else
      {
        desired_operating_mode = STATION_OPERATING_MODE_STANDBY;
      }
    }
    else
    {
      if (heater->tip_temp_cdeg + STATION_HEATUP_ENTER_BAND_CDEG < effective_target_temp_cdeg)
      {
        desired_operating_mode = STATION_OPERATING_MODE_HEATUP;
      }
      else if (heater->tip_temp_cdeg > effective_target_temp_cdeg + STATION_COOLDOWN_ENTER_BAND_CDEG)
      {
        desired_operating_mode = STATION_OPERATING_MODE_COOLDOWN;
      }
      else
      {
        desired_operating_mode = STATION_OPERATING_MODE_STANDBY;
      }
    }
  }
  else
  {
    effective_target_temp_cdeg = heater->effective_target_temp_cdeg;

    if ((station_context.operating_mode == STATION_OPERATING_MODE_HEATUP) ||
        (station_context.operating_mode == STATION_OPERATING_MODE_IDLE))
    {
      if (heater->tip_temp_cdeg + STATION_HEATUP_EXIT_BAND_CDEG < effective_target_temp_cdeg)
      {
        desired_state = STATION_STATE_HEATING;
        desired_operating_mode = STATION_OPERATING_MODE_HEATUP;
      }
      else
      {
        desired_state = STATION_STATE_READY;
        desired_operating_mode = STATION_OPERATING_MODE_HOLD;
      }
    }
    else if (station_context.operating_mode == STATION_OPERATING_MODE_COOLDOWN)
    {
      if (heater->tip_temp_cdeg > effective_target_temp_cdeg + STATION_COOLDOWN_EXIT_BAND_CDEG)
      {
        desired_state = STATION_STATE_IDLE;
        desired_operating_mode = STATION_OPERATING_MODE_COOLDOWN;
      }
      else
      {
        desired_state = STATION_STATE_READY;
        desired_operating_mode = STATION_OPERATING_MODE_HOLD;
      }
    }
    else
    {
      if (heater->tip_temp_cdeg + STATION_HEATUP_ENTER_BAND_CDEG < effective_target_temp_cdeg)
      {
        desired_state = STATION_STATE_HEATING;
        desired_operating_mode = STATION_OPERATING_MODE_HEATUP;
      }
      else if (heater->tip_temp_cdeg > effective_target_temp_cdeg + STATION_COOLDOWN_ENTER_BAND_CDEG)
      {
        desired_state = STATION_STATE_IDLE;
        desired_operating_mode = STATION_OPERATING_MODE_COOLDOWN;
      }
      else
      {
        desired_state = STATION_STATE_READY;
        desired_operating_mode = STATION_OPERATING_MODE_HOLD;
      }
    }
  }

  if ((station_context.state == STATION_STATE_BOOT) ||
      ((desired_state == station_context.state) && (desired_operating_mode == station_context.operating_mode)))
  {
    station_context.state = desired_state;
    station_context.operating_mode = desired_operating_mode;
    station_pending_state = desired_state;
    station_pending_operating_mode = desired_operating_mode;
    station_pending_runstate_since_ms = now_ms;
    return;
  }

  if ((desired_state != station_pending_state) || (desired_operating_mode != station_pending_operating_mode))
  {
    station_pending_state = desired_state;
    station_pending_operating_mode = desired_operating_mode;
    station_pending_runstate_since_ms = now_ms;
    return;
  }

  if ((now_ms - station_pending_runstate_since_ms) >= STATION_RUNSTATE_DEBOUNCE_MS)
  {
    station_context.state = desired_state;
    station_context.operating_mode = desired_operating_mode;
  }
}

static uint32_t Station_ReadHardwareFaultInputs(void)
{
  uint32_t fault_flags = 0;

#if !IRON_VIRTUAL_FAULT_INPUTS
  if (HAL_GPIO_ReadPin(OVERCURRENT_GPIO_Port, OVERCURRENT_Pin) == OVERCURRENT_FAULT_ACTIVE_LEVEL)
  {
    fault_flags |= STATION_FAULT_OVERCURRENT;
  }

  if (HAL_GPIO_ReadPin(BUCKBOOST_FAULT_GPIO_Port, BUCKBOOST_FAULT_Pin) == BUCKBOOST_FAULT_ACTIVE_LEVEL)
  {
    fault_flags |= STATION_FAULT_BUCKBOOST;
  }

  if (HAL_GPIO_ReadPin(OPAMP_PGOOD_GPIO_Port, OPAMP_PGOOD_Pin) != OPAMP_PGOOD_GOOD_LEVEL)
  {
    fault_flags |= STATION_FAULT_OPAMP_PGOOD;
  }
#endif

  return fault_flags;
}

static uint32_t Station_EvaluateDerivedFaults(const HeaterControlContext *heater, uint32_t now_ms)
{
  uint32_t fault_flags = 0U;

  if (heater == NULL)
  {
    return 0U;
  }

  if (heater->measurement_ready != 0U)
  {
    if (heater->tip_temp_cdeg >= STATION_TIP_OVERTEMP_LIMIT_CDEG)
    {
      fault_flags |= STATION_FAULT_TIP_OVERTEMP;
    }

    if ((heater->ambient_sensor_ready != 0U) && (heater->ambient_temp_cdeg >= (int16_t)STATION_AMBIENT_OVERTEMP_LIMIT_CDEG))
    {
      fault_flags |= STATION_FAULT_AMBIENT_OVERTEMP;
    }
  }

#if !IRON_VIRTUAL_MCP9808
  if (heater->ambient_sensor_ready == 0U)
  {
    if (station_ambient_sensor_fault_since_ms == 0U)
    {
      station_ambient_sensor_fault_since_ms = now_ms;
    }
    else if ((now_ms - station_ambient_sensor_fault_since_ms) >= STATION_AMBIENT_SENSOR_FAULT_TIMEOUT_MS)
    {
      fault_flags |= STATION_FAULT_AMBIENT_SENSOR;
    }
  }
  else
  {
    station_ambient_sensor_fault_since_ms = 0U;
  }
#else
  (void)now_ms;
#endif

  return fault_flags;
}

static uint32_t Station_EvaluateWarnings(const HeaterControlContext *heater)
{
  const Mcp9808Context *ambient_sensor = Mcp9808_GetContext();
  const Ina238Context *ina238 = Ina238_GetContext();
  uint32_t warning_flags = 0U;

  if (ambient_sensor->last_error != 0U)
  {
    warning_flags |= STATION_WARNING_MCP9808_ERROR;
  }

  if (ina238->last_error != 0U)
  {
    warning_flags |= STATION_WARNING_INA238_ERROR;
  }

  if (ina238->alert_active != 0U)
  {
    warning_flags |= STATION_WARNING_INA238_ALERT;
  }

#if !IRON_VIRTUAL_MCP9808
  if ((heater != NULL) && (heater->ambient_sensor_ready == 0U))
  {
    warning_flags |= STATION_WARNING_AMBIENT_SENSOR_MISSING;
  }
#else
  (void)heater;
#endif

  return warning_flags;
}

static void Station_UpdateFaultStatus(const HeaterControlContext *heater, uint32_t now_ms)
{
  uint32_t all_fault_flags = Station_ReadHardwareFaultInputs();
  uint32_t active_fault_flags;

  /*
   * Startup robustness: while heater output is still inactive, external power
   * path status lines can be transient. Do not latch BUCKBOOST/OPAMP faults
   * before the control loop actually drives heater power.
   */
  if ((heater != NULL) && (heater->pwm_permille == 0U))
  {
    all_fault_flags &= ~(STATION_FAULT_BUCKBOOST | STATION_FAULT_OPAMP_PGOOD);
  }

  all_fault_flags |= Station_EvaluateDerivedFaults(heater, now_ms);
  all_fault_flags |= station_forced_fault_flags;
  active_fault_flags = Station_FilterCriticalFaults(all_fault_flags);

  station_context.active_fault_flags = active_fault_flags;
  station_context.warning_flags = Station_EvaluateWarnings(heater);
  station_context.warning_flags |= Station_MapIgnoredFaultsToWarnings(all_fault_flags & ~STATION_FAULT_CRITICAL_MASK);

  if (active_fault_flags != 0U)
  {
    station_context.fault_flags |= active_fault_flags;
    station_context.fault_ack_pending = 1U;
  }
}

void Station_App_Init(void)
{
  const HeaterControlContext *heater;
  const StorageContext *storage;
  CalibrationTable persisted_table;
  uint8_t index;

  station_context.state = STATION_STATE_BOOT;
  station_context.fault_flags = 0;
  station_context.active_fault_flags = 0;
  station_context.warning_flags = 0;
  station_context.last_tick_ms = 0;
  station_context.operating_mode = STATION_OPERATING_MODE_IDLE;
  station_context.effective_target_temp_cdeg = STATION_STANDBY_TARGET_CDEG;
  station_context.fault_ack_pending = 0U;
  station_context.docked = 0U;
  station_context.standby_active = 0U;
  station_forced_fault_flags = 0U;
  station_ambient_sensor_fault_since_ms = 0U;
  station_pending_runstate_since_ms = 0U;
  station_simulated_docked = 0U;
  station_pending_state = STATION_STATE_BOOT;
  station_pending_operating_mode = STATION_OPERATING_MODE_IDLE;

  Heater_Control_Init();
  (void)Storage_Init();
  storage = Storage_GetContext();

  Heater_Control_SetTargetTempCdeg(storage->image.target_temp_cdeg);

  if ((storage->image.calibration_valid != 0U) && (storage->image.point_count <= CALIBRATION_MAX_POINTS))
  {
    memset(&persisted_table, 0, sizeof(persisted_table));
    persisted_table.signature = Calibration_GetActiveTable()->signature;
    persisted_table.valid = storage->image.calibration_valid;
    persisted_table.point_count = storage->image.point_count;

    for (index = 0U; index < storage->image.point_count; ++index)
    {
      persisted_table.points[index].tip_temp_cdeg = storage->image.points[index].tip_temp_cdeg;
      persisted_table.points[index].internal_raw = storage->image.points[index].internal_raw;
      persisted_table.points[index].valid = storage->image.points[index].valid;
    }

    (void)Calibration_LoadPersistedTable(&persisted_table);
  }

  heater = Heater_Control_GetContext();
  Station_UpdateControlTarget(heater);
  station_context.calibration_valid = heater->calibration_valid;

  Station_UpdateFaultStatus(heater, 0U);

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    station_context.operating_mode = STATION_OPERATING_MODE_FAULT;
    Station_SafeShutdown();
    return;
  }

  Station_UpdateRunState(heater, 0U);
}

void Station_App_Tick(uint32_t now_ms)
{
  const HeaterControlContext *heater;

  if ((now_ms - station_context.last_tick_ms) < 10U)
  {
    return;
  }

  station_context.last_tick_ms = now_ms;

  /* Evaluate safety-critical faults before any heater control update. */
  Station_UpdateFaultStatus(Heater_Control_GetContext(), now_ms);

  /* In FAULT state keep hard-safe outputs stable and skip heater sequencing. */
  if ((station_context.state == STATION_STATE_FAULT) ||
      (station_context.fault_flags != 0U) ||
      (station_context.active_fault_flags != 0U))
  {
    station_context.state = STATION_STATE_FAULT;
    station_context.operating_mode = STATION_OPERATING_MODE_FAULT;
    Station_SafeShutdown();
    return;
  }

  Station_UpdateControlTarget(Heater_Control_GetContext());
  Heater_Control_Tick(now_ms);
  heater = Heater_Control_GetContext();
  Station_UpdateControlTarget(heater);
  station_context.calibration_valid = heater->calibration_valid;
  Station_UpdateFaultStatus(heater, now_ms);

  if (station_context.active_fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    station_context.operating_mode = STATION_OPERATING_MODE_FAULT;
    Station_SafeShutdown();
    return;
  }

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    station_context.operating_mode = STATION_OPERATING_MODE_FAULT;
    Station_SafeShutdown();
    return;
  }

  Station_UpdateRunState(heater, now_ms);
}

void Station_SafeShutdown(void)
{
  /* Route safety-critical outputs through Safety_Guard */
  Safety_Guard_ForceSafeOff(HAL_GetTick());
  
  /* Legacy TIP_CHECK (non-critical monitoring) still set locally for compatibility */
  HAL_GPIO_WritePin(TIP_CHECK_GPIO_Port, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
}

void Station_App_RequestFault(uint32_t fault_mask)
{
  station_forced_fault_flags |= fault_mask;
  Station_UpdateFaultStatus(Heater_Control_GetContext(), HAL_GetTick());

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
  }
}

void Station_App_ClearFault(uint32_t fault_mask)
{
  station_forced_fault_flags &= ~fault_mask;
  Station_UpdateFaultStatus(Heater_Control_GetContext(), HAL_GetTick());
}

uint8_t Station_App_AcknowledgeFaults(uint32_t clearable_fault_mask)
{
  clearable_fault_mask &= STATION_FAULT_ACK_MASK;
  station_forced_fault_flags &= ~clearable_fault_mask;
  Station_UpdateFaultStatus(Heater_Control_GetContext(), HAL_GetTick());

  if (station_context.active_fault_flags != 0U)
  {
    return 0U;
  }

  station_context.fault_flags = 0U;
  station_context.fault_ack_pending = 0U;
  station_context.state = STATION_STATE_IDLE;
  station_context.operating_mode = STATION_OPERATING_MODE_IDLE;
  station_pending_state = STATION_STATE_IDLE;
  station_pending_operating_mode = STATION_OPERATING_MODE_IDLE;
  station_pending_runstate_since_ms = HAL_GetTick();
  return 1U;
}

void Station_App_SetDocked(uint8_t docked)
{
  station_simulated_docked = docked != 0U;
  station_context.docked = station_simulated_docked;
}

const StationContext *Station_App_GetContext(void)
{
  return &station_context;
}