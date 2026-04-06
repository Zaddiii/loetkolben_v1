#include "app.h"

#include "calibration.h"
#include "heater.h"
#include "main.h"
#include "mcp9808.h"
#include "storage.h"

#include <string.h>

enum
{
  STATION_TIP_OVERTEMP_LIMIT_CDEG = 4500U,
  STATION_AMBIENT_OVERTEMP_LIMIT_CDEG = 800U,
  STATION_AMBIENT_SENSOR_FAULT_TIMEOUT_MS = 5000U
};

static StationContext station_context;
static uint32_t station_forced_fault_flags;
static uint32_t station_ambient_sensor_fault_since_ms;

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
  uint32_t warning_flags = 0U;

  if (ambient_sensor->last_error != 0U)
  {
    warning_flags |= STATION_WARNING_MCP9808_ERROR;
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
  uint32_t active_fault_flags = Station_ReadHardwareFaultInputs();

  active_fault_flags |= Station_EvaluateDerivedFaults(heater, now_ms);
  active_fault_flags |= station_forced_fault_flags;

  station_context.active_fault_flags = active_fault_flags;
  station_context.warning_flags = Station_EvaluateWarnings(heater);

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
  station_context.fault_ack_pending = 0U;
  station_forced_fault_flags = 0U;
  station_ambient_sensor_fault_since_ms = 0U;

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
  station_context.calibration_valid = heater->calibration_valid;

  Station_UpdateFaultStatus(heater, 0U);

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
    return;
  }

  if (Calibration_IsBringUpSessionActive())
  {
    station_context.state = STATION_STATE_CALIBRATION;
    return;
  }

  if (station_context.calibration_valid == 0U)
  {
    station_context.state = STATION_STATE_NO_CALIBRATION;
  }
  else
  {
    station_context.state = STATION_STATE_IDLE;
  }
}

void Station_App_Tick(uint32_t now_ms)
{
  const HeaterControlContext *heater;

  if ((now_ms - station_context.last_tick_ms) < 10U)
  {
    return;
  }

  station_context.last_tick_ms = now_ms;
  Heater_Control_Tick(now_ms);
  heater = Heater_Control_GetContext();
  station_context.calibration_valid = heater->calibration_valid;
  Station_UpdateFaultStatus(heater, now_ms);

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
    return;
  }

  if (Calibration_IsBringUpSessionActive())
  {
    station_context.state = STATION_STATE_CALIBRATION;
    return;
  }

  if (station_context.calibration_valid == 0U)
  {
    station_context.state = STATION_STATE_NO_CALIBRATION;
    return;
  }

  if (heater->tip_temp_cdeg + 50U < heater->target_temp_cdeg)
  {
    station_context.state = STATION_STATE_HEATING;
  }
  else if (heater->tip_temp_cdeg > heater->target_temp_cdeg + 80U)
  {
    station_context.state = STATION_STATE_HEATING;
  }
  else
  {
    station_context.state = STATION_STATE_READY;
  }
}

void Station_SafeShutdown(void)
{
  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  HAL_GPIO_WritePin(TIP_CHECK_GPIO_Port, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
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
  station_forced_fault_flags &= ~clearable_fault_mask;
  Station_UpdateFaultStatus(Heater_Control_GetContext(), HAL_GetTick());

  if (station_context.active_fault_flags != 0U)
  {
    return 0U;
  }

  station_context.fault_flags = 0U;
  station_context.fault_ack_pending = 0U;
  return 1U;
}

const StationContext *Station_App_GetContext(void)
{
  return &station_context;
}