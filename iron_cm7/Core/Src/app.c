#include "app.h"

#include "calibration.h"
#include "heater.h"
#include "main.h"

static StationContext station_context;
static uint32_t station_forced_fault_flags;

static void Station_UpdateFaultInputs(void)
{
  uint32_t fault_flags = 0;

#if !IRON_SIMULATION_MODE
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

  fault_flags |= station_forced_fault_flags;
  station_context.fault_flags = fault_flags;
}

void Station_App_Init(void)
{
  const HeaterControlContext *heater;

  station_context.state = STATION_STATE_BOOT;
  station_context.fault_flags = 0;
  station_context.last_tick_ms = 0;
  station_forced_fault_flags = 0U;

  Heater_Control_Init();
  heater = Heater_Control_GetContext();
  station_context.calibration_valid = heater->calibration_valid;

  Station_UpdateFaultInputs();

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
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
  Station_UpdateFaultInputs();

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
    return;
  }

  if (station_context.calibration_valid == 0U)
  {
    station_context.state = STATION_STATE_NO_CALIBRATION;
    Heater_Control_Tick(now_ms);
    return;
  }

  Heater_Control_Tick(now_ms);
  heater = Heater_Control_GetContext();
  station_context.calibration_valid = heater->calibration_valid;

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
  Station_UpdateFaultInputs();

  if (station_context.fault_flags != 0U)
  {
    station_context.state = STATION_STATE_FAULT;
    Station_SafeShutdown();
  }
}

const StationContext *Station_App_GetContext(void)
{
  return &station_context;
}