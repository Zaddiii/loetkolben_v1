#include "heater.h"

#include "main.h"
#include "peripherals.h"

enum
{
  HEATER_MEASUREMENT_PERIOD_MS = 20U,
  HEATER_SWITCH_OFF_DELAY_US = 15U,
  HEATER_CLAMP_SETTLE_US = 20U,
  HEATER_RESTORE_DELAY_US = 8U,
  HEATER_ADC_SAMPLE_COUNT = 8U,
  HEATER_PWM_MAX_PERMILLE = 1000U
};

static HeaterControlContext heater_context;

#if IRON_SIMULATION_MODE
static uint16_t heater_simulated_raw = 1200U;
static int16_t heater_simulated_step = 11;
#endif

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

#if IRON_SIMULATION_MODE
static void Heater_FillSimulatedSamples(void)
{
  static const int16_t sample_offsets[HEATER_ADC_SAMPLE_COUNT] = {-9, -5, -2, 0, 2, 4, 7, 11};
  uint8_t index;

  heater_simulated_raw = (uint16_t)((int32_t)heater_simulated_raw + heater_simulated_step);

  if ((heater_simulated_raw >= 2200U) || (heater_simulated_raw <= 900U))
  {
    heater_simulated_step = (int16_t)(-heater_simulated_step);
    heater_simulated_raw = (uint16_t)((int32_t)heater_simulated_raw + heater_simulated_step);
  }

  for (index = 0; index < HEATER_ADC_SAMPLE_COUNT; ++index)
  {
    heater_context.latest_samples[index] = (uint16_t)((int32_t)heater_simulated_raw + sample_offsets[index]);
  }
}
#endif

static void Heater_PerformMeasurementSequence(void)
{
#if !IRON_SIMULATION_MODE
  uint8_t index;
#endif

  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  Heater_TimerWaitUs(HEATER_SWITCH_OFF_DELAY_US);

  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_MEASURE_LEVEL);
  Heater_TimerWaitUs(HEATER_CLAMP_SETTLE_US);

#if IRON_SIMULATION_MODE
  Heater_FillSimulatedSamples();
#else
  for (index = 0; index < HEATER_ADC_SAMPLE_COUNT; ++index)
  {
    heater_context.latest_samples[index] = 0U;
  }
#endif

  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
  Heater_TimerWaitUs(HEATER_RESTORE_DELAY_US);

  heater_context.filtered_raw = Heater_FilterSamples(heater_context.latest_samples, HEATER_ADC_SAMPLE_COUNT);
  heater_context.sample_count = HEATER_ADC_SAMPLE_COUNT;
  heater_context.measurement_ready = 1U;

  /* Heating stays disabled until the control path and hardware polarities are fully verified. */
  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
}

void Heater_Control_Init(void)
{
  heater_context.last_measurement_tick_ms = 0U;
  heater_context.filtered_raw = 0U;
  heater_context.pwm_permille = 0U;
  heater_context.sample_count = 0U;
  heater_context.measurement_ready = 0U;

  if (HAL_TIM_Base_Start(&htim7) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  Heater_Control_ForceOff();
#if IRON_SIMULATION_MODE
  Heater_Control_SetPwmPermille(IRON_SIMULATION_DEFAULT_PWM_PERMILLE);
#else
  Heater_ApplyPwm(0U);
#endif
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

void Heater_Control_SetPwmPermille(uint16_t pwm_permille)
{
  if (pwm_permille > HEATER_PWM_MAX_PERMILLE)
  {
    pwm_permille = HEATER_PWM_MAX_PERMILLE;
  }

  heater_context.pwm_permille = pwm_permille;
  Heater_ApplyPwm(pwm_permille);
}

void Heater_Control_ForceOff(void)
{
  heater_context.pwm_permille = 0U;
  Heater_ApplyPwm(0U);
  HAL_GPIO_WritePin(HEATER_EN_GPIO_Port, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  HAL_GPIO_WritePin(TIP_CHECK_GPIO_Port, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
  HAL_GPIO_WritePin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
}

const HeaterControlContext *Heater_Control_GetContext(void)
{
  return &heater_context;
}