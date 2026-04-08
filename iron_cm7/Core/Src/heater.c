#include "heater.h"

#include "calibration.h"
#include "ina238.h"
#include "main.h"
#include "mcp9808.h"
#include "max31856.h"
#include "peripherals.h"
#include "safety_guard.h"

enum
{
  HEATER_MEASUREMENT_PERIOD_MS = 20U,
  HEATER_ADC_POLL_TIMEOUT_MS = 5U,
  HEATER_MEAS_FAIL_RECOVERY_THRESHOLD = 10U,
  HEATER_SWITCH_OFF_DELAY_US = 15U,
  HEATER_CLAMP_SETTLE_US = 20U,
  HEATER_RESTORE_DELAY_US = 10U,
  HEATER_ADC_SAMPLE_COUNT = 8U,
  HEATER_PWM_MAX_PERMILLE = 1000U,
  HEATER_DEFAULT_TARGET_TEMP_CDEG = 3200U,
  HEATER_DEFAULT_AMBIENT_TEMP_CDEG = 250U,
  HEATER_SIMULATED_AMBIENT_TEMP_CDEG = 250U,
  HEATER_SIMULATED_TARGET_TEMP_CDEG = 3200U,
  HEATER_SIMULATED_MIN_TEMP_CDEG = 200U,
  HEATER_SIMULATED_MAX_TEMP_CDEG = 4500U,
  HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG = 0U,
  HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG = 800U,
  HEATER_SIMULATED_MAX_POWER_WATT_X10 = 900U,
  HEATER_SIMULATED_READY_BAND_CDEG = 50U,
  HEATER_SIMULATED_LOAD_NOMINAL_PERMILLE = 1000U,
  HEATER_SIMULATED_LOAD_MIN_PERMILLE = 250U,
  HEATER_SIMULATED_LOAD_MAX_PERMILLE = 2500U,
  HEATER_SIMULATED_MAX_HEAT_STEP_NUMERATOR = 24000U,
  HEATER_SIMULATED_LOSS_NUMERATOR = 1400U,
  HEATER_SIMULATED_LOSS_DENOMINATOR = 220000U,
  HEATER_SIMULATED_AMBIENT_HEAT_NUMERATOR = 1U,
  HEATER_SIMULATED_AMBIENT_HEAT_DENOMINATOR = 30U,
  HEATER_SIMULATED_AMBIENT_COOLING_DIVISOR = 8U,
  HEATER_SIMULATED_EXTERNAL_LAG_DIVISOR = 5U,
  HEATER_SIMULATED_PWM_SLEW_STEP = 40U,
  HEATER_SIMULATED_MIN_HOLD_PWM_PERMILLE = 80U,
  HEATER_SIMULATED_INTEGRATOR_MAX = 420U,
  HEATER_SIMULATED_INTEGRATOR_MIN = -180U,
  HEATER_AUX_ADC_FULL_SCALE_RAW = 4095U,
  HEATER_AUX_CURRENT_FULL_SCALE_MA = 8000U,
  HEATER_AUX_VBUS_FULL_SCALE_MV = 30000U,
  HEATER_INTERNAL_ADC_FULL_SCALE_RAW = 65535U,
  HEATER_TEST_LINEAR_TIP_MAX_CDEG = 5000U,
  HEATER_BUCKBOOST_TARGET_MV = 24000U,
  HEATER_BUCKBOOST_REF_FULL_SCALE_MV = 26000U,
  HEATER_MEAS_ERR_ADC1_START = (1U << 0),
  HEATER_MEAS_ERR_ADC1_TIMEOUT = (1U << 1),
  HEATER_MEAS_ERR_ADC1_STOP = (1U << 2),
  HEATER_MEAS_ERR_ADC2_READ = (1U << 3)
};

static HeaterControlContext heater_context;
static uint8_t heater_dwt_delay_ready;
static int32_t heater_control_integrator_permille;
static uint16_t heater_test_ambient_temp_cdeg;
static uint32_t heater_measurement_fail_streak;

static void Heater_ApplyPwm(uint16_t pwm_permille);
static uint16_t Heater_ComputeControlPwmPermille(void);
static void Heater_UpdateBuckBoostReference(uint16_t pwm_permille);
static void Heater_UpdateScaledAuxiliaryTelemetry(void);
static uint8_t Heater_ReadAuxiliaryMeasurements(void);
static void Heater_HandleMeasurementFailure(uint16_t error_flag);
static void Heater_UpdateDerivedTelemetry(uint32_t now_ms);
static uint16_t Heater_ClampTargetTempCdeg(uint16_t target_temp_cdeg);

#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
static uint16_t heater_simulated_raw = 1200U;
static int16_t heater_simulated_step = 11;
static int32_t heater_simulated_integrator_permille = 0;

static uint16_t Heater_SimulateControllerPwmPermille(void)
{
  int32_t tip_temp_cdeg = (int32_t)heater_context.simulated_tip_temp_cdeg;
  int32_t target_temp_cdeg = (int32_t)heater_context.effective_target_temp_cdeg;
  int32_t ambient_temp_cdeg = (int32_t)heater_context.simulated_ambient_temp_cdeg;
  int32_t error_cdeg = target_temp_cdeg - tip_temp_cdeg;
  int32_t requested_pwm_permille;

  if (target_temp_cdeg <= (ambient_temp_cdeg + 50))
  {
    heater_simulated_integrator_permille = 0;
    return 0U;
  }

  if (error_cdeg >= 0)
  {
    heater_simulated_integrator_permille += error_cdeg / 35;
  }
  else
  {
    heater_simulated_integrator_permille += error_cdeg / 60;
  }

  if (heater_simulated_integrator_permille > HEATER_SIMULATED_INTEGRATOR_MAX)
  {
    heater_simulated_integrator_permille = HEATER_SIMULATED_INTEGRATOR_MAX;
  }

  if (heater_simulated_integrator_permille < HEATER_SIMULATED_INTEGRATOR_MIN)
  {
    heater_simulated_integrator_permille = HEATER_SIMULATED_INTEGRATOR_MIN;
  }

  requested_pwm_permille = (error_cdeg * 2) / 3;
  requested_pwm_permille += heater_simulated_integrator_permille;

  if (error_cdeg > 0)
  {
    requested_pwm_permille += HEATER_SIMULATED_MIN_HOLD_PWM_PERMILLE;
  }

  if (error_cdeg < -(int32_t)HEATER_SIMULATED_READY_BAND_CDEG)
  {
    requested_pwm_permille = 0;
  }

  if (requested_pwm_permille < 0)
  {
    requested_pwm_permille = 0;
  }

  if (requested_pwm_permille > HEATER_PWM_MAX_PERMILLE)
  {
    requested_pwm_permille = HEATER_PWM_MAX_PERMILLE;
  }

  return (uint16_t)requested_pwm_permille;
}

static uint16_t Heater_SimulatePwmRamp(uint16_t current_pwm_permille, uint16_t requested_pwm_permille)
{
  if (current_pwm_permille < requested_pwm_permille)
  {
    uint16_t ramped_pwm = (uint16_t)(current_pwm_permille + HEATER_SIMULATED_PWM_SLEW_STEP);

    if (ramped_pwm > requested_pwm_permille)
    {
      ramped_pwm = requested_pwm_permille;
    }

    return ramped_pwm;
  }

  if (current_pwm_permille > requested_pwm_permille)
  {
    if (current_pwm_permille > (requested_pwm_permille + HEATER_SIMULATED_PWM_SLEW_STEP))
    {
      return (uint16_t)(current_pwm_permille - HEATER_SIMULATED_PWM_SLEW_STEP);
    }

    return requested_pwm_permille;
  }

  return current_pwm_permille;
}

static void Heater_UpdateSimulationModel(void)
{
  int32_t tip_temp_cdeg = (int32_t)heater_context.simulated_tip_temp_cdeg;
  int32_t ambient_temp_cdeg = (int32_t)heater_context.simulated_ambient_temp_cdeg;
  int32_t ambient_base_temp_cdeg = (int32_t)heater_context.simulated_ambient_base_temp_cdeg;
  int32_t external_tip_temp_cdeg = (int32_t)heater_context.simulated_external_tip_temp_cdeg;
  int32_t load_permille = (int32_t)heater_context.simulated_thermal_load_permille;
  int32_t tip_delta_cdeg = tip_temp_cdeg - ambient_temp_cdeg;
  int32_t heat_step_cdeg;
  int32_t loss_step_cdeg;
  int32_t ambient_rise_cdeg;
  uint16_t requested_pwm_permille;

  heater_context.simulated_target_temp_cdeg = heater_context.target_temp_cdeg;
  heater_context.simulated_target_temp_cdeg = heater_context.effective_target_temp_cdeg;
  requested_pwm_permille = Heater_SimulateControllerPwmPermille();
  heater_context.simulated_requested_pwm_permille = requested_pwm_permille;
  heater_context.pwm_permille = Heater_SimulatePwmRamp(heater_context.pwm_permille, requested_pwm_permille);
  Heater_UpdateBuckBoostReference(heater_context.pwm_permille);
  Heater_ApplyPwm(heater_context.pwm_permille);

  heat_step_cdeg = ((int32_t)heater_context.pwm_permille * (int32_t)HEATER_SIMULATED_MAX_HEAT_STEP_NUMERATOR) / load_permille;

  if (tip_delta_cdeg < 0)
  {
    tip_delta_cdeg = 0;
  }

  loss_step_cdeg = (tip_delta_cdeg * ((int32_t)HEATER_SIMULATED_LOSS_NUMERATOR + load_permille)) / HEATER_SIMULATED_LOSS_DENOMINATOR;
  tip_temp_cdeg += heat_step_cdeg - loss_step_cdeg;

  if (tip_temp_cdeg < (int32_t)heater_context.simulated_ambient_base_temp_cdeg)
  {
    tip_temp_cdeg = (int32_t)heater_context.simulated_ambient_base_temp_cdeg;
  }

  if (tip_temp_cdeg > HEATER_SIMULATED_MAX_TEMP_CDEG)
  {
    tip_temp_cdeg = HEATER_SIMULATED_MAX_TEMP_CDEG;
  }

  ambient_rise_cdeg = ((int32_t)heater_context.simulated_power_watt_x10 * (int32_t)HEATER_SIMULATED_AMBIENT_HEAT_NUMERATOR) /
                      HEATER_SIMULATED_AMBIENT_HEAT_DENOMINATOR;
  ambient_rise_cdeg -= (ambient_temp_cdeg - ambient_base_temp_cdeg) / HEATER_SIMULATED_AMBIENT_COOLING_DIVISOR;
  ambient_temp_cdeg += ambient_rise_cdeg;

  if (ambient_temp_cdeg < HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG;
  }

  if (ambient_temp_cdeg > HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG;
  }

  heater_context.simulated_power_watt_x10 = (uint16_t)((uint32_t)HEATER_SIMULATED_MAX_POWER_WATT_X10 * heater_context.pwm_permille / HEATER_PWM_MAX_PERMILLE);
  external_tip_temp_cdeg += (tip_temp_cdeg - external_tip_temp_cdeg) / HEATER_SIMULATED_EXTERNAL_LAG_DIVISOR;

  if (external_tip_temp_cdeg < ambient_temp_cdeg)
  {
    external_tip_temp_cdeg = ambient_temp_cdeg;
  }

  heater_context.simulated_tip_temp_cdeg = (uint16_t)tip_temp_cdeg;
  heater_context.simulated_ambient_temp_cdeg = (uint16_t)ambient_temp_cdeg;
  heater_context.simulated_external_tip_temp_cdeg = (uint16_t)external_tip_temp_cdeg;
}
#endif

static void Heater_UpdateDerivedTelemetry(uint32_t now_ms)
{
  const Mcp9808Context *ambient_sensor = Mcp9808_GetContext();
  const Max31856Context *max31856 = Max31856_GetContext();
#if !(IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC) && !IRON_VIRTUAL_CALIBRATION
  uint16_t tip_temp_cdeg;
#endif
  int16_t ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;

  if (Mcp9808_Poll(now_ms))
  {
    heater_context.ambient_sensor_ready = ambient_sensor->data_ready;
    ambient_temp_cdeg = ambient_sensor->ambient_temp_cdeg;
  }
  else
  {
    heater_context.ambient_sensor_ready = 0U;
  }

#if IRON_VIRTUAL_MCP9808
  ambient_temp_cdeg = (int16_t)heater_test_ambient_temp_cdeg;
  heater_context.ambient_sensor_ready = 1U;
#endif

  if (Max31856_Poll(now_ms))
  {
    heater_context.external_sensor_ready = max31856->data_ready;
    heater_context.external_tip_temp_cdeg = max31856->thermocouple_temp_cdeg;
    heater_context.external_ambient_temp_cdeg = max31856->cold_junction_temp_cdeg;
  }
  else
  {
    heater_context.external_sensor_ready = 0U;
    heater_context.external_tip_temp_cdeg = 0U;
    heater_context.external_ambient_temp_cdeg = ambient_temp_cdeg;
  }

  heater_context.calibration_valid = Calibration_HasValidTable() ? 1U : 0U;
  Heater_UpdateScaledAuxiliaryTelemetry();

#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  heater_context.tip_temp_cdeg = heater_context.simulated_tip_temp_cdeg;
  heater_context.ambient_temp_cdeg = heater_context.simulated_ambient_temp_cdeg;
  heater_context.external_tip_temp_cdeg = heater_context.simulated_external_tip_temp_cdeg;
  heater_context.external_ambient_temp_cdeg = (int16_t)heater_context.simulated_ambient_temp_cdeg;
#else
  heater_context.ambient_temp_cdeg = ambient_temp_cdeg;

#if IRON_VIRTUAL_CALIBRATION
  heater_context.tip_temp_cdeg = (uint16_t)(((uint32_t)heater_context.filtered_raw * HEATER_TEST_LINEAR_TIP_MAX_CDEG) /
                                            HEATER_INTERNAL_ADC_FULL_SCALE_RAW);
#else
  if (Calibration_ConvertRawToTipCdeg(heater_context.filtered_raw, ambient_temp_cdeg, &tip_temp_cdeg))
  {
    heater_context.tip_temp_cdeg = tip_temp_cdeg;
  }
  else
  {
    heater_context.tip_temp_cdeg = 0U;
  }
#endif
#endif

  heater_context.power_watt_x10 = (uint16_t)(((uint32_t)heater_context.heater_current_ma * (uint32_t)heater_context.buckboost_voltage_mv) / 100000U);
  if (heater_context.power_watt_x10 > 999U)
  {
    heater_context.power_watt_x10 = 999U;
  }

  if (Ina238_Poll(now_ms))
  {
  }
}

static void Heater_TimerWaitUs(uint16_t wait_us)
{
  uint32_t start_cycles;
  uint32_t target_cycles;

  if (wait_us == 0U)
  {
    return;
  }

  if (heater_dwt_delay_ready == 0U)
  {
    __HAL_TIM_SET_COUNTER(&htim7, 0U);
    while (__HAL_TIM_GET_COUNTER(&htim7) < wait_us)
    {
    }

    return;
  }

  start_cycles = DWT->CYCCNT;
  target_cycles = (SystemCoreClock / 1000000U) * (uint32_t)wait_us;

  while ((DWT->CYCCNT - start_cycles) < target_cycles)
  {
  }
}

static uint16_t Heater_ClampTargetTempCdeg(uint16_t target_temp_cdeg)
{
  if (target_temp_cdeg < HEATER_SIMULATED_MIN_TEMP_CDEG)
  {
    return HEATER_SIMULATED_MIN_TEMP_CDEG;
  }

  if (target_temp_cdeg > HEATER_SIMULATED_MAX_TEMP_CDEG)
  {
    return HEATER_SIMULATED_MAX_TEMP_CDEG;
  }

  return target_temp_cdeg;
}

static uint16_t Heater_ComputeControlPwmPermille(void)
{
  int32_t tip_temp_cdeg = (int32_t)heater_context.tip_temp_cdeg;
  int32_t target_temp_cdeg = (int32_t)heater_context.effective_target_temp_cdeg;
  int32_t ambient_temp_cdeg = (int32_t)heater_context.ambient_temp_cdeg;
  int32_t error_cdeg = target_temp_cdeg - tip_temp_cdeg;
  int32_t requested_pwm_permille;

  if (target_temp_cdeg <= (ambient_temp_cdeg + 50))
  {
    heater_control_integrator_permille = 0;
    return 0U;
  }

  if (error_cdeg >= 0)
  {
    heater_control_integrator_permille += error_cdeg / 35;
  }
  else
  {
    heater_control_integrator_permille += error_cdeg / 60;
  }

  if (heater_control_integrator_permille > HEATER_SIMULATED_INTEGRATOR_MAX)
  {
    heater_control_integrator_permille = HEATER_SIMULATED_INTEGRATOR_MAX;
  }

  if (heater_control_integrator_permille < HEATER_SIMULATED_INTEGRATOR_MIN)
  {
    heater_control_integrator_permille = HEATER_SIMULATED_INTEGRATOR_MIN;
  }

  requested_pwm_permille = (error_cdeg * 2) / 3;
  requested_pwm_permille += heater_control_integrator_permille;

  if (error_cdeg > 0)
  {
    requested_pwm_permille += HEATER_SIMULATED_MIN_HOLD_PWM_PERMILLE;
  }

  if (error_cdeg < -(int32_t)HEATER_SIMULATED_READY_BAND_CDEG)
  {
    requested_pwm_permille = 0;
  }

  if (requested_pwm_permille < 0)
  {
    requested_pwm_permille = 0;
  }

  if (requested_pwm_permille > HEATER_PWM_MAX_PERMILLE)
  {
    requested_pwm_permille = HEATER_PWM_MAX_PERMILLE;
  }

  return (uint16_t)requested_pwm_permille;
}

static void Heater_ApplyPwm(uint16_t pwm_permille)
{
  uint32_t compare_value = ((uint32_t)(__HAL_TIM_GET_AUTORELOAD(&htim1) + 1U) * pwm_permille) / HEATER_PWM_MAX_PERMILLE;

  if (compare_value > __HAL_TIM_GET_AUTORELOAD(&htim1))
  {
    compare_value = __HAL_TIM_GET_AUTORELOAD(&htim1);
  }

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_value);
}

static void Heater_UpdateBuckBoostReference(uint16_t pwm_permille)
{
  uint32_t dac_value;

  heater_context.buckboost_target_mv = (pwm_permille == 0U) ? 0U : HEATER_BUCKBOOST_TARGET_MV;
  dac_value = ((uint32_t)heater_context.buckboost_target_mv * 4095U) / HEATER_BUCKBOOST_REF_FULL_SCALE_MV;

  if (dac_value > 4095U)
  {
    dac_value = 4095U;
  }

  heater_context.buckboost_ref_raw = (uint16_t)dac_value;

  if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value) != HAL_OK)
  {
    Error_Handler();
  }
}

static void Heater_UpdateScaledAuxiliaryTelemetry(void)
{
  heater_context.heater_current_ma = (uint16_t)(((uint32_t)heater_context.heater_current_raw * HEATER_AUX_CURRENT_FULL_SCALE_MA) / HEATER_AUX_ADC_FULL_SCALE_RAW);
  heater_context.buckboost_voltage_mv = (uint16_t)(((uint32_t)heater_context.buckboost_voltage_raw * HEATER_AUX_VBUS_FULL_SCALE_MV) / HEATER_AUX_ADC_FULL_SCALE_RAW);
}

static uint8_t Heater_ReadAuxiliaryMeasurements(void)
{
#if IRON_VIRTUAL_AUX_ADC
  heater_context.heater_current_raw = (uint16_t)(700U + ((uint32_t)heater_context.pwm_permille * 11U) / 10U);
  heater_context.buckboost_voltage_raw = (uint16_t)(2400U + ((uint32_t)heater_context.pwm_permille * 9U) / 10U);
  return 1U;
#else
  if (HAL_ADC_Start(&hadc2) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_PollForConversion(&hadc2, HEATER_ADC_POLL_TIMEOUT_MS) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc2);
    return 0U;
  }
  heater_context.heater_current_raw = (uint16_t)HAL_ADC_GetValue(&hadc2);

  if (HAL_ADC_PollForConversion(&hadc2, HEATER_ADC_POLL_TIMEOUT_MS) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc2);
    return 0U;
  }
  heater_context.buckboost_voltage_raw = (uint16_t)HAL_ADC_GetValue(&hadc2);

  if (HAL_ADC_Stop(&hadc2) != HAL_OK)
  {
    return 0U;
  }

  return 1U;
#endif
}

#if IRON_VIRTUAL_INTERNAL_ADC
static void Heater_FillSimulatedSamples(void)
{
  static const int16_t sample_offsets[HEATER_ADC_SAMPLE_COUNT] = {-9, -5, -2, 0, 2, 4, 7, 11};
  uint8_t index;

  Heater_UpdateSimulationModel();
  heater_simulated_raw = (uint16_t)(400U + ((uint32_t)heater_context.simulated_tip_temp_cdeg * 3U) / 2U);
  heater_simulated_raw = (uint16_t)((int32_t)heater_simulated_raw + heater_simulated_step);
  heater_simulated_step = (int16_t)(-heater_simulated_step);

  for (index = 0; index < HEATER_ADC_SAMPLE_COUNT; ++index)
  {
    heater_context.latest_samples[index] = (uint16_t)((int32_t)heater_simulated_raw + sample_offsets[index]);
  }
}
#endif

static uint8_t Heater_PerformMeasurementSequence(void)
{
#if !IRON_VIRTUAL_INTERNAL_ADC
  uint8_t index;
#endif

  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  Heater_TimerWaitUs(HEATER_SWITCH_OFF_DELAY_US);

  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_MEASURE_LEVEL);
  Heater_TimerWaitUs(HEATER_CLAMP_SETTLE_US);

#if IRON_VIRTUAL_INTERNAL_ADC
  Heater_FillSimulatedSamples();
#else
  for (index = 0; index < HEATER_ADC_SAMPLE_COUNT; ++index)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      Heater_HandleMeasurementFailure(HEATER_MEAS_ERR_ADC1_START);
      return 0U;
    }

    if (HAL_ADC_PollForConversion(&hadc1, HEATER_ADC_POLL_TIMEOUT_MS) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      Heater_HandleMeasurementFailure(HEATER_MEAS_ERR_ADC1_TIMEOUT);
      return 0U;
    }

    heater_context.latest_samples[index] = (uint16_t)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
      Heater_HandleMeasurementFailure(HEATER_MEAS_ERR_ADC1_STOP);
      return 0U;
    }
  }
#endif

  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
  Heater_TimerWaitUs(HEATER_RESTORE_DELAY_US);

  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_ACTIVE_LEVEL);

  heater_context.filtered_raw = heater_context.latest_samples[HEATER_ADC_SAMPLE_COUNT - 1U];
  if (Heater_ReadAuxiliaryMeasurements() == 0U)
  {
    Heater_HandleMeasurementFailure(HEATER_MEAS_ERR_ADC2_READ);
    return 0U;
  }
  Heater_UpdateDerivedTelemetry(HAL_GetTick());
  heater_context.sample_count = HEATER_ADC_SAMPLE_COUNT;

  /* Phase 3: Set measurement metadata after successful cycle */
  heater_context.measurement.sequence_id++;
  heater_context.measurement.timestamp_ms = HAL_GetTick();
  heater_context.measurement.valid = 1U;
  heater_context.measurement.age_ms = 0U;
  heater_context.measurement.error_flags = 0U;

  heater_context.measurement_ready = 1U;
  heater_measurement_fail_streak = 0U;
  return 1U;
}

void Heater_Control_Init(void)
{
  heater_context.last_measurement_tick_ms = 0U;
  heater_context.filtered_raw = 0U;
  heater_context.heater_current_raw = 0U;
  heater_context.heater_current_ma = 0U;
  heater_context.buckboost_voltage_raw = 0U;
  heater_context.buckboost_voltage_mv = 0U;
  heater_context.buckboost_ref_raw = 0U;
  heater_context.buckboost_target_mv = 0U;
  heater_context.pwm_permille = 0U;
  heater_context.tip_temp_cdeg = 0U;
  heater_context.target_temp_cdeg = HEATER_DEFAULT_TARGET_TEMP_CDEG;
  heater_context.effective_target_temp_cdeg = HEATER_DEFAULT_TARGET_TEMP_CDEG;
  heater_context.power_watt_x10 = 0U;
  heater_context.external_tip_temp_cdeg = 0U;
  heater_context.simulated_tip_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_target_temp_cdeg = HEATER_SIMULATED_TARGET_TEMP_CDEG;
  heater_context.simulated_ambient_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_ambient_base_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_external_tip_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_power_watt_x10 = 0U;
  heater_context.simulated_requested_pwm_permille = 0U;
  heater_context.simulated_thermal_load_permille = HEATER_SIMULATED_LOAD_NOMINAL_PERMILLE;
  heater_context.ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;
  heater_context.external_ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;
  heater_context.sample_count = 0U;
  heater_context.measurement_ready = 0U;
  heater_context.calibration_valid = 0U;
  heater_context.ambient_sensor_ready = 0U;
  heater_context.external_sensor_ready = 0U;

  /* Phase 3: Initialize measurement metadata */
  heater_context.measurement.sequence_id = 0U;
  heater_context.measurement.timestamp_ms = 0U;
  heater_context.measurement.valid = 0U;
  heater_context.measurement.age_ms = 65535U;
  heater_context.measurement.error_flags = 0U;

  heater_dwt_delay_ready = 0U;
  heater_control_integrator_permille = 0;
  heater_test_ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;
  heater_measurement_fail_streak = 0U;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0U;

  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U)
  {
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U)
    {
      heater_dwt_delay_ready = 1U;
    }
  }

  Calibration_Init();
  (void)Ina238_Init();
  (void)Mcp9808_Init();
  (void)Max31856_Init();

  if (HAL_TIM_Base_Start(&htim7) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_TIM_MOE_ENABLE(&htim1);

  Heater_Control_ForceOff();
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  Heater_Control_SetPwmPermille(IRON_SIMULATION_DEFAULT_PWM_PERMILLE);
  Heater_Simulation_Reset();
#else
  Heater_ApplyPwm(0U);
#endif

  Heater_UpdateDerivedTelemetry(HAL_GetTick());
}

void Heater_Control_Tick(uint32_t now_ms)
{
  uint16_t requested_pwm_permille;

  /* Phase 3: Update measurement age relative to now [ms] */
  if (heater_context.measurement.valid)
  {
    uint32_t age_ms = now_ms - heater_context.measurement.timestamp_ms;
    /* Clamp to uint16_t max to avoid wraparound at very old measurements */
    heater_context.measurement.age_ms = (age_ms > 65535U) ? 65535U : (uint16_t)age_ms;
  }

  if ((now_ms - heater_context.last_measurement_tick_ms) < HEATER_MEASUREMENT_PERIOD_MS)
  {
    return;
  }

  heater_context.last_measurement_tick_ms = now_ms;
  if (Heater_PerformMeasurementSequence() == 0U)
  {
    return;
  }

#if !(IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC)
  requested_pwm_permille = Heater_ComputeControlPwmPermille();
  Heater_Control_SetPwmPermille(requested_pwm_permille);
#endif
}

void Heater_Control_SetTargetTempCdeg(uint16_t target_temp_cdeg)
{
  target_temp_cdeg = Heater_ClampTargetTempCdeg(target_temp_cdeg);

  heater_context.target_temp_cdeg = target_temp_cdeg;
  heater_context.effective_target_temp_cdeg = target_temp_cdeg;
  heater_context.simulated_target_temp_cdeg = heater_context.effective_target_temp_cdeg;
}

void Heater_Control_SetEffectiveTargetTempCdeg(uint16_t target_temp_cdeg)
{
  heater_context.effective_target_temp_cdeg = Heater_ClampTargetTempCdeg(target_temp_cdeg);
  heater_context.simulated_target_temp_cdeg = heater_context.effective_target_temp_cdeg;
}

void Heater_Control_SetPwmPermille(uint16_t pwm_permille)
{
  if (pwm_permille > HEATER_PWM_MAX_PERMILLE)
  {
    pwm_permille = HEATER_PWM_MAX_PERMILLE;
  }

  heater_context.pwm_permille = pwm_permille;
  Heater_UpdateBuckBoostReference(pwm_permille);
  Heater_ApplyPwm(pwm_permille);
}

void Heater_Control_ForceOff(void)
{
  heater_context.pwm_permille = 0U;
  heater_context.simulated_power_watt_x10 = 0U;
  heater_control_integrator_permille = 0;
  Heater_UpdateBuckBoostReference(0U);
  Heater_ApplyPwm(0U);
  /* Safety-critical outputs now routed through Safety_Guard */
  Safety_Guard_ForceSafeOff(HAL_GetTick());
  /* Legacy TIP_CHECK (non-critical) still set locally for compatibility */
  HAL_GPIO_WritePin(TIP_CHECK_GPIO_Port, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
}

void Heater_Simulation_Reset(void)
{
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  heater_simulated_integrator_permille = 0;
  heater_context.simulated_tip_temp_cdeg = heater_context.simulated_ambient_base_temp_cdeg;
  heater_context.simulated_external_tip_temp_cdeg = heater_context.simulated_ambient_base_temp_cdeg;
  heater_context.simulated_ambient_temp_cdeg = heater_context.simulated_ambient_base_temp_cdeg;
  heater_context.simulated_power_watt_x10 = 0U;
  heater_context.simulated_requested_pwm_permille = 0U;
  Heater_Control_SetPwmPermille(0U);
#endif
}

void Heater_Simulation_SetAmbientTempCdeg(uint16_t ambient_temp_cdeg)
{
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  if (ambient_temp_cdeg < HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG;
  }

  if (ambient_temp_cdeg > HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG;
  }

  heater_context.simulated_ambient_base_temp_cdeg = ambient_temp_cdeg;
  if (heater_context.simulated_ambient_temp_cdeg < ambient_temp_cdeg)
  {
    heater_context.simulated_ambient_temp_cdeg = ambient_temp_cdeg;
  }

  if (heater_context.simulated_tip_temp_cdeg < ambient_temp_cdeg)
  {
    heater_context.simulated_tip_temp_cdeg = ambient_temp_cdeg;
  }

  if (heater_context.simulated_external_tip_temp_cdeg < ambient_temp_cdeg)
  {
    heater_context.simulated_external_tip_temp_cdeg = ambient_temp_cdeg;
  }
#else
  (void)ambient_temp_cdeg;
#endif
}

void Heater_Simulation_SetThermalLoadPermille(uint16_t thermal_load_permille)
{
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  if (thermal_load_permille < HEATER_SIMULATED_LOAD_MIN_PERMILLE)
  {
    thermal_load_permille = HEATER_SIMULATED_LOAD_MIN_PERMILLE;
  }

  if (thermal_load_permille > HEATER_SIMULATED_LOAD_MAX_PERMILLE)
  {
    thermal_load_permille = HEATER_SIMULATED_LOAD_MAX_PERMILLE;
  }

  heater_context.simulated_thermal_load_permille = thermal_load_permille;
#else
  (void)thermal_load_permille;
#endif
}

void Heater_Test_SetAmbientTempCdeg(uint16_t ambient_temp_cdeg)
{
  if (ambient_temp_cdeg < HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MIN_AMBIENT_TEMP_CDEG;
  }

  if (ambient_temp_cdeg > HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG)
  {
    ambient_temp_cdeg = HEATER_SIMULATED_MAX_AMBIENT_TEMP_CDEG;
  }

  heater_test_ambient_temp_cdeg = ambient_temp_cdeg;
}

uint16_t Heater_Test_GetAmbientTempCdeg(void)
{
  return heater_test_ambient_temp_cdeg;
}

void Heater_Simulation_SetTipTempCdeg(uint16_t tip_temp_cdeg)
{
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  if (tip_temp_cdeg < heater_context.simulated_ambient_base_temp_cdeg)
  {
    tip_temp_cdeg = heater_context.simulated_ambient_base_temp_cdeg;
  }

  if (tip_temp_cdeg > HEATER_SIMULATED_MAX_TEMP_CDEG)
  {
    tip_temp_cdeg = HEATER_SIMULATED_MAX_TEMP_CDEG;
  }

  heater_context.simulated_tip_temp_cdeg = tip_temp_cdeg;
  heater_context.simulated_external_tip_temp_cdeg = tip_temp_cdeg;
#else
  (void)tip_temp_cdeg;
#endif
}

const HeaterControlContext *Heater_Control_GetContext(void)
{
  return &heater_context;
}

static void Heater_HandleMeasurementFailure(uint16_t error_flag)
{
  heater_context.measurement_ready = 0U;
  heater_context.measurement.valid = 0U;
  heater_context.measurement.error_flags |= error_flag;
  heater_measurement_fail_streak++;

  if (heater_measurement_fail_streak >= HEATER_MEAS_FAIL_RECOVERY_THRESHOLD)
  {
    /* Recover from rare long-run ADC stuck states observed on bench. */
    (void)HAL_ADC_Stop(&hadc1);
    (void)HAL_ADC_Stop(&hadc2);
    (void)HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
    (void)HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
    heater_measurement_fail_streak = 0U;
  }

  Heater_Control_ForceOff();
}