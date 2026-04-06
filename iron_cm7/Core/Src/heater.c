#include "heater.h"

#include "calibration.h"
#include "main.h"
#include "mcp9808.h"
#include "max31856.h"
#include "peripherals.h"

enum
{
  HEATER_MEASUREMENT_PERIOD_MS = 20U,
  HEATER_SWITCH_OFF_DELAY_US = 15U,
  HEATER_CLAMP_SETTLE_US = 20U,
  HEATER_RESTORE_DELAY_US = 8U,
  HEATER_ADC_SAMPLE_COUNT = 8U,
  HEATER_PWM_MAX_PERMILLE = 1000U,
  HEATER_DEFAULT_TARGET_TEMP_CDEG = 3200U,
  HEATER_DEFAULT_AMBIENT_TEMP_CDEG = 250U,
  HEATER_SIMULATED_AMBIENT_TEMP_CDEG = 250U,
  HEATER_SIMULATED_TARGET_TEMP_CDEG = 3200U,
  HEATER_SIMULATED_MIN_TEMP_CDEG = 200U,
  HEATER_SIMULATED_MAX_TEMP_CDEG = 4500U,
  HEATER_SIMULATED_MAX_POWER_WATT_X10 = 900U,
  HEATER_SIMULATED_READY_BAND_CDEG = 50U
};

static HeaterControlContext heater_context;

static void Heater_ApplyPwm(uint16_t pwm_permille);
static void Heater_UpdateBuckBoostReference(uint16_t pwm_permille);
#if !(IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC)
static uint16_t Heater_ComputePseudoPowerWattX10(uint16_t current_raw, uint16_t voltage_raw);
static uint16_t Heater_ReadSingleAdcSample(ADC_HandleTypeDef *hadc);
#endif
static void Heater_ReadAuxiliaryMeasurements(void);
static void Heater_UpdateDerivedTelemetry(uint32_t now_ms);

#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
static uint16_t heater_simulated_raw = 1200U;
static int16_t heater_simulated_step = 11;

static uint16_t Heater_SimulatePwmPermilleFromTemperature(uint16_t tip_temp_cdeg, uint16_t target_temp_cdeg)
{
  int32_t error_cdeg = (int32_t)target_temp_cdeg - (int32_t)tip_temp_cdeg;

  if (error_cdeg <= 0)
  {
    return 120U;
  }

  if (error_cdeg >= 1200)
  {
    return 900U;
  }

  if (error_cdeg >= 500)
  {
    return 720U;
  }

  return (uint16_t)(180U + ((uint32_t)error_cdeg * 540U) / 500U);
}

static void Heater_UpdateSimulationModel(void)
{
  int32_t error_cdeg;
  int32_t next_tip_temp_cdeg;

  heater_context.simulated_target_temp_cdeg = heater_context.target_temp_cdeg;
  heater_context.simulated_ambient_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.pwm_permille = Heater_SimulatePwmPermilleFromTemperature(
      heater_context.simulated_tip_temp_cdeg,
      heater_context.simulated_target_temp_cdeg);
  Heater_UpdateBuckBoostReference(heater_context.pwm_permille);
  Heater_ApplyPwm(heater_context.pwm_permille);

  error_cdeg = (int32_t)heater_context.simulated_target_temp_cdeg - (int32_t)heater_context.simulated_tip_temp_cdeg;

  if (error_cdeg > (int32_t)HEATER_SIMULATED_READY_BAND_CDEG)
  {
    next_tip_temp_cdeg = (int32_t)heater_context.simulated_tip_temp_cdeg + (error_cdeg / 7);
  }
  else if (error_cdeg < -(int32_t)HEATER_SIMULATED_READY_BAND_CDEG)
  {
    next_tip_temp_cdeg = (int32_t)heater_context.simulated_tip_temp_cdeg + (error_cdeg / 10);
  }
  else
  {
    next_tip_temp_cdeg = (int32_t)heater_context.simulated_target_temp_cdeg - (heater_simulated_step * 2);
  }

  if (next_tip_temp_cdeg < HEATER_SIMULATED_MIN_TEMP_CDEG)
  {
    next_tip_temp_cdeg = HEATER_SIMULATED_MIN_TEMP_CDEG;
  }

  if (next_tip_temp_cdeg > HEATER_SIMULATED_MAX_TEMP_CDEG)
  {
    next_tip_temp_cdeg = HEATER_SIMULATED_MAX_TEMP_CDEG;
  }

  heater_context.simulated_tip_temp_cdeg = (uint16_t)next_tip_temp_cdeg;
  heater_context.simulated_power_watt_x10 = (uint16_t)((uint32_t)HEATER_SIMULATED_MAX_POWER_WATT_X10 * heater_context.pwm_permille / HEATER_PWM_MAX_PERMILLE);
}
#endif

static void Heater_UpdateDerivedTelemetry(uint32_t now_ms)
{
  const Mcp9808Context *ambient_sensor = Mcp9808_GetContext();
  const Max31856Context *max31856 = Max31856_GetContext();
#if !(IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC)
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

#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  heater_context.tip_temp_cdeg = heater_context.simulated_tip_temp_cdeg;
  heater_context.ambient_temp_cdeg = heater_context.simulated_ambient_temp_cdeg;
  heater_context.power_watt_x10 = heater_context.simulated_power_watt_x10;
#else
  heater_context.ambient_temp_cdeg = ambient_temp_cdeg;

  if (Calibration_ConvertRawToTipCdeg(heater_context.filtered_raw, ambient_temp_cdeg, &tip_temp_cdeg))
  {
    heater_context.tip_temp_cdeg = tip_temp_cdeg;
  }
  else
  {
    heater_context.tip_temp_cdeg = 0U;
  }

  heater_context.power_watt_x10 = Heater_ComputePseudoPowerWattX10(
      heater_context.heater_current_raw,
      heater_context.buckboost_voltage_raw);
#endif
}

static void Heater_TimerWaitUs(uint16_t wait_us)
{
  __HAL_TIM_SET_COUNTER(&htim7, 0U);

  while (__HAL_TIM_GET_COUNTER(&htim7) < wait_us)
  {
  }
}

static uint16_t Heater_FilterSamples(const uint16_t *samples, uint8_t count)
{
  uint32_t sum = 0;
  uint16_t min_value = 0xFFFFU;
  uint16_t max_value = 0U;
  uint8_t index;

  for (index = 0; index < count; ++index)
  {
    uint16_t value = samples[index];
    sum += value;

    if (value < min_value)
    {
      min_value = value;
    }

    if (value > max_value)
    {
      max_value = value;
    }
  }

  if (count > 2U)
  {
    sum -= min_value;
    sum -= max_value;
    return (uint16_t)(sum / (uint32_t)(count - 2U));
  }

  return (uint16_t)(sum / count);
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
  uint32_t dac_value = ((uint32_t)pwm_permille * 4095U) / HEATER_PWM_MAX_PERMILLE;

  heater_context.buckboost_ref_raw = (uint16_t)dac_value;

  if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value) != HAL_OK)
  {
    Error_Handler();
  }
}

#if !(IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC)
static uint16_t Heater_ComputePseudoPowerWattX10(uint16_t current_raw, uint16_t voltage_raw)
{
  uint32_t scaled_power = ((uint32_t)current_raw * (uint32_t)voltage_raw) / 12000U;

  if (scaled_power > 999U)
  {
    scaled_power = 999U;
  }

  return (uint16_t)scaled_power;
}

static uint16_t Heater_ReadSingleAdcSample(ADC_HandleTypeDef *hadc)
{
  uint32_t value;

  if (HAL_ADC_Start(hadc) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_PollForConversion(hadc, 2U) != HAL_OK)
  {
    Error_Handler();
  }

  value = HAL_ADC_GetValue(hadc);

  if (HAL_ADC_Stop(hadc) != HAL_OK)
  {
    Error_Handler();
  }

  return (uint16_t)value;
}
#endif

static void Heater_ReadAuxiliaryMeasurements(void)
{
#if IRON_VIRTUAL_AUX_ADC
  heater_context.heater_current_raw = (uint16_t)(700U + ((uint32_t)heater_context.pwm_permille * 11U) / 10U);
  heater_context.buckboost_voltage_raw = (uint16_t)(2400U + ((uint32_t)heater_context.pwm_permille * 9U) / 10U);
#else
  if (HAL_ADC_Start(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_PollForConversion(&hadc2, 2U) != HAL_OK)
  {
    Error_Handler();
  }
  heater_context.heater_current_raw = (uint16_t)HAL_ADC_GetValue(&hadc2);

  if (HAL_ADC_PollForConversion(&hadc2, 2U) != HAL_OK)
  {
    Error_Handler();
  }
  heater_context.buckboost_voltage_raw = (uint16_t)HAL_ADC_GetValue(&hadc2);

  if (HAL_ADC_Stop(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

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

static void Heater_PerformMeasurementSequence(void)
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
    heater_context.latest_samples[index] = Heater_ReadSingleAdcSample(&hadc1);
  }
#endif

  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
  Heater_TimerWaitUs(HEATER_RESTORE_DELAY_US);

  heater_context.filtered_raw = Heater_FilterSamples(heater_context.latest_samples, HEATER_ADC_SAMPLE_COUNT);
  Heater_ReadAuxiliaryMeasurements();
  Heater_UpdateDerivedTelemetry(HAL_GetTick());
  heater_context.sample_count = HEATER_ADC_SAMPLE_COUNT;
  heater_context.measurement_ready = 1U;

  /* Heating stays disabled until the control path and hardware polarities are fully verified. */
  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
}

void Heater_Control_Init(void)
{
  heater_context.last_measurement_tick_ms = 0U;
  heater_context.filtered_raw = 0U;
  heater_context.heater_current_raw = 0U;
  heater_context.buckboost_voltage_raw = 0U;
  heater_context.buckboost_ref_raw = 0U;
  heater_context.pwm_permille = 0U;
  heater_context.tip_temp_cdeg = 0U;
  heater_context.target_temp_cdeg = HEATER_DEFAULT_TARGET_TEMP_CDEG;
  heater_context.power_watt_x10 = 0U;
  heater_context.external_tip_temp_cdeg = 0U;
  heater_context.simulated_tip_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_target_temp_cdeg = HEATER_SIMULATED_TARGET_TEMP_CDEG;
  heater_context.simulated_ambient_temp_cdeg = HEATER_SIMULATED_AMBIENT_TEMP_CDEG;
  heater_context.simulated_power_watt_x10 = 0U;
  heater_context.ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;
  heater_context.external_ambient_temp_cdeg = HEATER_DEFAULT_AMBIENT_TEMP_CDEG;
  heater_context.sample_count = 0U;
  heater_context.measurement_ready = 0U;
  heater_context.calibration_valid = 0U;
  heater_context.ambient_sensor_ready = 0U;
  heater_context.external_sensor_ready = 0U;

  Calibration_Init();
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

  Heater_Control_ForceOff();
#if IRON_VIRTUAL_INTERNAL_ADC || IRON_VIRTUAL_AUX_ADC
  Heater_Control_SetPwmPermille(IRON_SIMULATION_DEFAULT_PWM_PERMILLE);
#else
  Heater_ApplyPwm(0U);
#endif

  Heater_UpdateDerivedTelemetry(HAL_GetTick());
}

void Heater_Control_Tick(uint32_t now_ms)
{
  if ((now_ms - heater_context.last_measurement_tick_ms) < HEATER_MEASUREMENT_PERIOD_MS)
  {
    return;
  }

  heater_context.last_measurement_tick_ms = now_ms;
  Heater_PerformMeasurementSequence();
}

void Heater_Control_SetTargetTempCdeg(uint16_t target_temp_cdeg)
{
  if (target_temp_cdeg < HEATER_SIMULATED_MIN_TEMP_CDEG)
  {
    target_temp_cdeg = HEATER_SIMULATED_MIN_TEMP_CDEG;
  }

  if (target_temp_cdeg > HEATER_SIMULATED_MAX_TEMP_CDEG)
  {
    target_temp_cdeg = HEATER_SIMULATED_MAX_TEMP_CDEG;
  }

  heater_context.target_temp_cdeg = target_temp_cdeg;
  heater_context.simulated_target_temp_cdeg = target_temp_cdeg;
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
  Heater_UpdateBuckBoostReference(0U);
  Heater_ApplyPwm(0U);
  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  HAL_GPIO_WritePin(TIP_CHECK_GPIO_Port, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
}

const HeaterControlContext *Heater_Control_GetContext(void)
{
  return &heater_context;
}