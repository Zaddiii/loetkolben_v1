#include "fan.h"

#include <stddef.h>

#include "peripherals.h"

enum
{
  FAN_PWM_MAX_PERMILLE = 1000U,
  FAN_AMBIENT_STAGE1_CDEG = 300U,
  FAN_AMBIENT_STAGE2_CDEG = 400U,
  FAN_AMBIENT_STAGE3_CDEG = 500U,
  FAN_STAGE1_PWM_PERMILLE = 300U,
  FAN_STAGE2_PWM_PERMILLE = 500U,
  FAN_STAGE3_PWM_PERMILLE = 1000U,
  FAN_RAMP_STEP_PERMILLE = 60U,
  FAN_MAX_RPM = 15600U,
  FAN_TACHO_PULSES_PER_REV = 2U,
  FAN_TACHO_MIN_PERIOD_US = 1500U,
  FAN_TACHO_MIN_ACTIVE_PWM_PERMILLE = 200U,
  FAN_TACHO_MIN_ALLOWED_RPM = 300U,
  FAN_TACHO_TOLERANCE_PERCENT = 30U,
  FAN_TACH_TIMER_HZ = 1000000U,
  FAN_TACH_CALIBRATION_PPM_DEFAULT = 1003200U, /* +0.32%: MCU clock runs 0.32% fast, corrects tach reading */
  FAN_TACH_CALIBRATION_PPM_MIN = 950000U,
  FAN_TACH_CALIBRATION_PPM_MAX = 1050000U,
  FAN_TACH_RAW_HISTORY_LEN = 32U
};

static FanContext fan_context;

/* Shared between TIM17 ISR and Fan_Tick. Written by ISR only. */
static volatile uint32_t fan_tach_period_us;      /* µs between last two rising edges */
static volatile uint32_t fan_tach_last_edge_ms;   /* HAL_GetTick() at last captured edge */
static volatile uint32_t fan_tach_valid_edge_count;
static volatile uint32_t fan_tach_rejected_edge_count;
static volatile uint32_t fan_tach_period_sum_us_total;   /* accumulated valid periods [µs] */
static volatile uint32_t fan_tach_period_count_total;    /* accumulated valid period count */
static volatile uint32_t fan_tach_raw_period_ticks[FAN_TACH_RAW_HISTORY_LEN];
static volatile uint32_t fan_tach_raw_write_index;
static volatile uint32_t fan_tach_raw_count;

/* ISR-internal state (only accessed from capture callback) */
static uint32_t fan_tach_prev_raw;
static uint8_t fan_tach_prev_valid;
static volatile uint32_t fan_tach_calibration_ppm = FAN_TACH_CALIBRATION_PPM_DEFAULT;
static uint32_t fan_tach_period_sum_snapshot_us;

static uint32_t Fan_GetCalibratedTimerHz(void)
{
  uint64_t scaled_hz = ((uint64_t)FAN_TACH_TIMER_HZ * fan_tach_calibration_ppm) + 500000ULL;
  return (uint32_t)(scaled_hz / 1000000ULL);
}

uint32_t Fan_GetTachTimerClockHz(void)
{
  RCC_ClkInitTypeDef clock_config;
  uint32_t flash_latency;
  uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();

  HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

  if (clock_config.APB2CLKDivider == RCC_HCLK_DIV1)
  {
    return pclk2_hz;
  }

  /* TIM17 is on APB2; with APB prescaler != 1 timer kernel clock is 2x PCLK on this config. */
  return pclk2_hz * 2U;
}

uint32_t Fan_GetTachTickHz(void)
{
  return Fan_GetTachTimerClockHz() / (htim17.Init.Prescaler + 1U);
}

uint16_t Fan_GetTachPrescaler(void)
{
  return (uint16_t)htim17.Init.Prescaler;
}

uint32_t Fan_DebugCopyRawPeriods(uint32_t *out_period_ticks, uint32_t max_count)
{
  uint32_t count;
  uint32_t start;
  uint32_t i;

  if ((out_period_ticks == NULL) || (max_count == 0U))
  {
    return 0U;
  }

  __disable_irq();

  count = fan_tach_raw_count;
  if (count > FAN_TACH_RAW_HISTORY_LEN)
  {
    count = FAN_TACH_RAW_HISTORY_LEN;
  }
  if (count > max_count)
  {
    count = max_count;
  }

  start = (fan_tach_raw_write_index + FAN_TACH_RAW_HISTORY_LEN - count) % FAN_TACH_RAW_HISTORY_LEN;
  for (i = 0U; i < count; ++i)
  {
    out_period_ticks[i] = fan_tach_raw_period_ticks[(start + i) % FAN_TACH_RAW_HISTORY_LEN];
  }

  __enable_irq();

  return count;
}

static void Fan_ApplyPwm(uint16_t pwm_permille)
{
  uint32_t compare_value = ((uint32_t)(__HAL_TIM_GET_AUTORELOAD(&htim15) + 1U) * pwm_permille) / FAN_PWM_MAX_PERMILLE;

  if (compare_value > __HAL_TIM_GET_AUTORELOAD(&htim15))
  {
    compare_value = __HAL_TIM_GET_AUTORELOAD(&htim15);
  }

  __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, compare_value);
}

static uint16_t Fan_ClampPwm(uint16_t pwm_permille)
{
  if (pwm_permille > FAN_PWM_MAX_PERMILLE)
  {
    return FAN_PWM_MAX_PERMILLE;
  }

  return pwm_permille;
}

static uint16_t Fan_SlewPwm(uint16_t current_pwm_permille, uint16_t requested_pwm_permille)
{
  if (current_pwm_permille < requested_pwm_permille)
  {
    uint16_t next_pwm = (uint16_t)(current_pwm_permille + FAN_RAMP_STEP_PERMILLE);

    if (next_pwm > requested_pwm_permille)
    {
      next_pwm = requested_pwm_permille;
    }

    return next_pwm;
  }

  if (current_pwm_permille > requested_pwm_permille)
  {
    if (current_pwm_permille > (requested_pwm_permille + FAN_RAMP_STEP_PERMILLE))
    {
      return (uint16_t)(current_pwm_permille - FAN_RAMP_STEP_PERMILLE);
    }

    return requested_pwm_permille;
  }

  return current_pwm_permille;
}

void Fan_Init(void)
{
  fan_context.last_tick_ms = 0U;
  fan_context.last_tach_sample_ms = 0U;
  fan_context.last_tach_pulse_count = 0U;
  fan_context.pwm_permille = 0U;
  fan_context.requested_pwm_permille = 0U;
  fan_context.tach_rpm = 0U;
  fan_context.target_rpm = 0U;
  fan_context.tach_error_rpm = 0U;
  fan_context.tach_pulses_window = 0U;
  fan_context.tach_window_ms = 0U;
  fan_context.tach_signal_hz_x100 = 0U;
  fan_context.tach_valid_edges = 0U;
  fan_context.tach_rejected_edges = 0U;
  fan_context.tach_calibration_ppm = FAN_TACH_CALIBRATION_PPM_DEFAULT;
  fan_context.enabled = 0U;
  fan_context.cooling_request = 0U;
  fan_context.tach_ok = 1U;
  fan_context.last_tach_sample_ms = 0U;
  fan_context.last_tach_pulse_count = 0U;

  fan_tach_period_us = 0U;
  fan_tach_last_edge_ms = 0U;
  fan_tach_valid_edge_count = 0U;
  fan_tach_rejected_edge_count = 0U;
  fan_tach_period_sum_us_total = 0U;
  fan_tach_period_count_total = 0U;
  fan_tach_prev_raw = 0U;
  fan_tach_prev_valid = 0U;
  fan_tach_calibration_ppm = FAN_TACH_CALIBRATION_PPM_DEFAULT;
  fan_tach_period_sum_snapshot_us = 0U;
  fan_tach_raw_write_index = 0U;
  fan_tach_raw_count = 0U;

  if (HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_IC_Start_IT(&htim17, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  Fan_ApplyPwm(0U);
}

void Fan_Tick(uint32_t now_ms, const StationContext *station, const HeaterControlContext *heater)
{
  uint32_t period_us;
  uint32_t period_sum_us_total;
  uint32_t period_count_total;
  uint32_t period_sum_us_window;
  uint32_t period_count_window;
  uint32_t window_ms;
  uint32_t last_edge_ms;
  uint16_t requested_pwm_permille = 0U;
  uint16_t target_rpm;

  if ((station == NULL) || (heater == NULL))
  {
    return;
  }

  if ((now_ms - fan_context.last_tick_ms) < 20U)
  {
    return;
  }

  fan_context.last_tick_ms = now_ms;
  fan_context.cooling_request = 0U;

  if ((uint16_t)heater->ambient_temp_cdeg >= FAN_AMBIENT_STAGE3_CDEG)
  {
    requested_pwm_permille = FAN_STAGE3_PWM_PERMILLE;
    fan_context.cooling_request = 1U;
  }
  else if ((uint16_t)heater->ambient_temp_cdeg >= FAN_AMBIENT_STAGE2_CDEG)
  {
    requested_pwm_permille = FAN_STAGE2_PWM_PERMILLE;
    fan_context.cooling_request = 1U;
  }
  else if ((uint16_t)heater->ambient_temp_cdeg >= FAN_AMBIENT_STAGE1_CDEG)
  {
    requested_pwm_permille = FAN_STAGE1_PWM_PERMILLE;
    fan_context.cooling_request = 1U;
  }

  requested_pwm_permille = Fan_ClampPwm(requested_pwm_permille);
  fan_context.requested_pwm_permille = requested_pwm_permille;
  fan_context.pwm_permille = Fan_SlewPwm(fan_context.pwm_permille, requested_pwm_permille);
  Fan_ApplyPwm(fan_context.pwm_permille);
  fan_context.enabled = fan_context.pwm_permille != 0U;
  target_rpm = (uint16_t)(((uint32_t)fan_context.pwm_permille * FAN_MAX_RPM) / FAN_PWM_MAX_PERMILLE);
  fan_context.target_rpm = target_rpm;

  /*
   * Robust tach measurement:
   * - ISR accumulates many valid periods.
   * - Fan_Tick computes average period over a window (>=200 ms or >=32 edges).
   * This avoids single-edge jitter and pulse asymmetry bias.
   */
  period_sum_us_total = fan_tach_period_sum_us_total;
  period_count_total = fan_tach_period_count_total;
  last_edge_ms = fan_tach_last_edge_ms;

  if ((last_edge_ms == 0U) || ((now_ms - last_edge_ms) > 500U))
  {
    fan_context.tach_rpm = 0U;
    fan_context.tach_signal_hz_x100 = 0U;
    fan_context.tach_pulses_window = 0U;
    fan_context.tach_window_ms = 0U;
    fan_context.last_tach_sample_ms = now_ms;
    fan_context.last_tach_pulse_count = period_count_total;
    fan_tach_period_sum_snapshot_us = period_sum_us_total;
  }
  else
  {
    if (fan_context.last_tach_sample_ms == 0U)
    {
      fan_context.last_tach_sample_ms = now_ms;
      fan_context.last_tach_pulse_count = period_count_total;
      fan_tach_period_sum_snapshot_us = period_sum_us_total;
    }

    period_count_window = period_count_total - fan_context.last_tach_pulse_count;
    period_sum_us_window = period_sum_us_total - fan_tach_period_sum_snapshot_us;
    window_ms = now_ms - fan_context.last_tach_sample_ms;

    fan_context.tach_window_ms = window_ms;

    if ((period_count_window > 0U) &&
        (period_sum_us_window > 0U) &&
        ((window_ms >= 200U) || (period_count_window >= 32U)))
    {
      uint64_t hz_x100;
      uint64_t rpm;
      uint32_t timer_hz = Fan_GetCalibratedTimerHz();

      hz_x100 = (((uint64_t)timer_hz * 100ULL * (uint64_t)period_count_window) +
                 ((uint64_t)period_sum_us_window / 2ULL)) /
                (uint64_t)period_sum_us_window;
      fan_context.tach_signal_hz_x100 = (hz_x100 > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (uint32_t)hz_x100;

      rpm = (((uint64_t)timer_hz * 60ULL * (uint64_t)period_count_window) +
             ((uint64_t)period_sum_us_window * (uint64_t)FAN_TACHO_PULSES_PER_REV / 2ULL)) /
            ((uint64_t)period_sum_us_window * (uint64_t)FAN_TACHO_PULSES_PER_REV);
      fan_context.tach_rpm = (rpm > 0xFFFFULL) ? 0xFFFFU : (uint16_t)rpm;

      period_us = (period_sum_us_window + (period_count_window / 2U)) / period_count_window;
      fan_tach_period_us = period_us;
      fan_context.tach_pulses_window = period_us;

      fan_context.last_tach_sample_ms = now_ms;
      fan_context.last_tach_pulse_count = period_count_total;
      fan_tach_period_sum_snapshot_us = period_sum_us_total;
    }
  }

  fan_context.tach_valid_edges = fan_tach_valid_edge_count;
  fan_context.tach_rejected_edges = fan_tach_rejected_edge_count;
  fan_context.tach_calibration_ppm = fan_tach_calibration_ppm;

  if (fan_context.tach_rpm > fan_context.target_rpm)
  {
    fan_context.tach_error_rpm = (uint16_t)(fan_context.tach_rpm - fan_context.target_rpm);
  }
  else
  {
    fan_context.tach_error_rpm = (uint16_t)(fan_context.target_rpm - fan_context.tach_rpm);
  }

  if ((fan_context.pwm_permille < FAN_TACHO_MIN_ACTIVE_PWM_PERMILLE) || (fan_context.target_rpm < FAN_TACHO_MIN_ALLOWED_RPM))
  {
    fan_context.tach_ok = 1U;
  }
  else
  {
    uint32_t allowed_error_rpm = ((uint32_t)fan_context.target_rpm * FAN_TACHO_TOLERANCE_PERCENT) / 100U;

    if (allowed_error_rpm < FAN_TACHO_MIN_ALLOWED_RPM)
    {
      allowed_error_rpm = FAN_TACHO_MIN_ALLOWED_RPM;
    }

    fan_context.tach_ok = ((uint32_t)fan_context.tach_error_rpm <= allowed_error_rpm) ? 1U : 0U;
  }
}


const FanContext *Fan_GetContext(void)
{
  return &fan_context;
}

/* Called from TIM17_IRQHandler via HAL_TIM_IRQHandler.
   Measures time between consecutive rising edges (period in µs at 1 MHz timer
   clock) and stores the result in fan_tach_period_us.  IRQ rate equals the
   tach frequency (typically 250-350 Hz in bench tests) - completely safe. */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint32_t capture_now;
  uint32_t period;

  if ((htim == NULL) || (htim->Instance != TIM17) || (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
  {
    return;
  }

  capture_now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

  if (fan_tach_prev_valid != 0U)
  {
    /* Handle 16-bit counter wrap-around */
    if (capture_now >= fan_tach_prev_raw)
    {
      period = capture_now - fan_tach_prev_raw;
    }
    else
    {
      period = (0x10000U + capture_now) - fan_tach_prev_raw;
    }

    if (period >= FAN_TACHO_MIN_PERIOD_US)
    {
      fan_tach_period_us = period;
      fan_tach_period_sum_us_total += period;
      fan_tach_period_count_total++;
      fan_tach_raw_period_ticks[fan_tach_raw_write_index] = period;
      fan_tach_raw_write_index = (fan_tach_raw_write_index + 1U) % FAN_TACH_RAW_HISTORY_LEN;
      if (fan_tach_raw_count < FAN_TACH_RAW_HISTORY_LEN)
      {
        fan_tach_raw_count++;
      }
      fan_tach_last_edge_ms = HAL_GetTick();
      fan_tach_valid_edge_count++;
    }
    else
    {
      fan_tach_rejected_edge_count++;
    }
  }

  fan_tach_prev_raw = capture_now;
  fan_tach_prev_valid = 1U;
}

void Fan_SetRpmCalibrationPpm(uint32_t calibration_ppm)
{
  if (calibration_ppm < FAN_TACH_CALIBRATION_PPM_MIN)
  {
    calibration_ppm = FAN_TACH_CALIBRATION_PPM_MIN;
  }
  else if (calibration_ppm > FAN_TACH_CALIBRATION_PPM_MAX)
  {
    calibration_ppm = FAN_TACH_CALIBRATION_PPM_MAX;
  }

  fan_tach_calibration_ppm = calibration_ppm;
}

uint32_t Fan_GetRpmCalibrationPpm(void)
{
  return fan_tach_calibration_ppm;
}
