#include "fan.h"

#include <stddef.h>

enum
{
  FAN_PWM_MAX_PERMILLE = 1000U,
  FAN_MIN_ACTIVE_PWM_PERMILLE = 180U,
  FAN_COOLDOWN_BOOST_PERMILLE = 180U,
  FAN_FAULT_PWM_PERMILLE = 900U,
  FAN_RAMP_STEP_PERMILLE = 60U,
  FAN_MAX_RPM = 7200U,
  FAN_AMBIENT_MARGIN_ENABLE_CDEG = 120U,
  FAN_STANDBY_MARGIN_CDEG = 80U
};

static FanContext fan_context;

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
  fan_context.pwm_permille = 0U;
  fan_context.requested_pwm_permille = 0U;
  fan_context.tach_rpm = 0U;
  fan_context.enabled = 0U;
  fan_context.cooling_request = 0U;
}

void Fan_Tick(uint32_t now_ms, const StationContext *station, const HeaterControlContext *heater)
{
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

  if (station->state == STATION_STATE_FAULT)
  {
    requested_pwm_permille = FAN_FAULT_PWM_PERMILLE;
    fan_context.cooling_request = 1U;
  }
  else if (heater->tip_temp_cdeg > (uint16_t)(heater->ambient_temp_cdeg + FAN_AMBIENT_MARGIN_ENABLE_CDEG))
  {
    int32_t temp_margin_cdeg = (int32_t)heater->tip_temp_cdeg - (int32_t)heater->ambient_temp_cdeg;

    requested_pwm_permille = (uint16_t)(temp_margin_cdeg / 5);
    if (requested_pwm_permille > 0U)
    {
      requested_pwm_permille = (uint16_t)(requested_pwm_permille + FAN_MIN_ACTIVE_PWM_PERMILLE);
    }

    if ((station->operating_mode == STATION_OPERATING_MODE_COOLDOWN) ||
        (station->operating_mode == STATION_OPERATING_MODE_STANDBY))
    {
      requested_pwm_permille = (uint16_t)(requested_pwm_permille + FAN_COOLDOWN_BOOST_PERMILLE);
      fan_context.cooling_request = 1U;
    }
  }

  if ((station->standby_active != 0U) &&
      (heater->tip_temp_cdeg <= (heater->effective_target_temp_cdeg + FAN_STANDBY_MARGIN_CDEG)))
  {
    requested_pwm_permille = 0U;
    fan_context.cooling_request = 0U;
  }

  requested_pwm_permille = Fan_ClampPwm(requested_pwm_permille);
  fan_context.requested_pwm_permille = requested_pwm_permille;
  fan_context.pwm_permille = Fan_SlewPwm(fan_context.pwm_permille, requested_pwm_permille);
  fan_context.enabled = fan_context.pwm_permille != 0U;
  target_rpm = (uint16_t)(((uint32_t)fan_context.pwm_permille * FAN_MAX_RPM) / FAN_PWM_MAX_PERMILLE);

  if (fan_context.tach_rpm < target_rpm)
  {
    fan_context.tach_rpm = (uint16_t)(fan_context.tach_rpm + ((target_rpm - fan_context.tach_rpm + 3U) / 4U));
  }
  else if (fan_context.tach_rpm > target_rpm)
  {
    fan_context.tach_rpm = (uint16_t)(fan_context.tach_rpm - ((fan_context.tach_rpm - target_rpm + 3U) / 4U));
  }
}

const FanContext *Fan_GetContext(void)
{
  return &fan_context;
}