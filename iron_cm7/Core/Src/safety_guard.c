#include "safety_guard.h"

#include "main.h"
#include "peripherals.h"

#include <string.h>

/**
 * Measurement freshness timeout [ms].
 * If measurement is older than this, it's considered stale.
 */
#define SAFETY_GUARD_MEASUREMENT_TIMEOUT_MS (100U)

/**
 * Global safety guard context.
 */
static SafetyGuardContext safety_context;

/**
 * Internal: apply actual GPIO/PWM outputs to hardware.
 * Only called from Safety_Guard_SetOutputs() after safety checks pass.
 */
static void Safety_Guard_ApplyOutputs(
    uint8_t heater_en,
    SafetyClampState clamp,
    uint16_t pwm_permille)
{
  /* HEATER_EN: active high */
  HAL_GPIO_WritePin(
      HEATER_EN_GPIO_Port,
      HEATER_EN_Pin,
      heater_en ? HEATER_EN_ACTIVE_LEVEL : HEATER_EN_INACTIVE_LEVEL);

  /* TIP_CLAMP: may be open-drain or push-pull, depends on polarity define */
  HAL_GPIO_WritePin(
      TIP_CLAMP_GPIO_Port,
      TIP_CLAMP_Pin,
      (clamp == SAFETY_CLAMP_SAFE) ? TIP_CLAMP_SAFE_LEVEL : TIP_CLAMP_MEASURE_LEVEL);

  /* PWM: set compare value for TIM1 CH1 (heater) */
  uint32_t compare_value = ((__HAL_TIM_GET_AUTORELOAD(&htim1) + 1U) * (uint32_t)pwm_permille) / 1000U;
  if (compare_value > __HAL_TIM_GET_AUTORELOAD(&htim1))
  {
    compare_value = __HAL_TIM_GET_AUTORELOAD(&htim1);
  }
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_value);
}

/**
 * Internal: check invariants.
 * Returns non-zero (bitmask) if any invariant is violated.
 */
static uint32_t Safety_Guard_CheckInvariants(
    uint8_t heater_en_request,
    SafetyClampState clamp_request,
    uint16_t pwm_request,
    uint8_t measurement_valid,
    uint8_t measurement_is_fresh)
{
  uint32_t violations = 0U;

  /* Invariant 1: No EN without valid fresh measurement */
  if (heater_en_request != 0U)
  {
    if (measurement_valid == 0U)
    {
      violations |= 0x01U;  /* INV_EN_NEEDS_VALID_MEAS */
    }

    if (measurement_is_fresh == 0U)
    {
      violations |= 0x02U;  /* INV_EN_NEEDS_FRESH_MEAS */
    }
  }

  /* Invariant 2: EN must imply clamp in SAFE or valid measurement window */
  if (heater_en_request != 0U && clamp_request != SAFETY_CLAMP_SAFE)
  {
    violations |= 0x04U;  /* INV_EN_REQUIRES_CLAMP_SAFE */
  }

  /* Invariant 3: PWM > 0 only if EN is requested */
  if (pwm_request > 0U && heater_en_request == 0U)
  {
    violations |= 0x08U;  /* INV_PWM_NEEDS_EN */
  }

  /* Invariant 4: No extreme PWM values */
  if (pwm_request > 1000U)
  {
    violations |= 0x10U;  /* INV_PWM_OUT_OF_RANGE */
  }

  return violations;
}

void Safety_Guard_Init(void)
{
  memset(&safety_context, 0, sizeof(safety_context));
  safety_context.clamp_state = SAFETY_CLAMP_SAFE;
  safety_context.clamp_requested = SAFETY_CLAMP_SAFE;
  safety_context.measurement_is_fresh = 0U;

  /* Apply safe default state */
  Safety_Guard_ApplyOutputs(0U, SAFETY_CLAMP_SAFE, 0U);
}

const SafetyGuardContext *Safety_Guard_GetContext(void)
{
  return &safety_context;
}

uint8_t Safety_Guard_SetOutputs(
    uint8_t heater_en_request,
    SafetyClampState clamp_request,
    uint16_t pwm_request_permille,
    uint32_t measurement_sequence_id,
    uint32_t measurement_timestamp_ms,
    uint8_t measurement_valid,
    uint32_t now_ms)
{
  uint32_t violations;
  uint32_t measurement_age_ms;
  uint8_t override_triggered = 0U;

  /* Update context with request */
  safety_context.heater_en_requested = heater_en_request;
  safety_context.clamp_requested = clamp_request;
  safety_context.pwm_permille_requested = pwm_request_permille;

  /* Calculate measurement age and freshness */
  if (measurement_valid != 0U && now_ms >= measurement_timestamp_ms)
  {
    measurement_age_ms = now_ms - measurement_timestamp_ms;
    safety_context.measurement_age_ms = measurement_age_ms;
    safety_context.measurement_is_fresh = (measurement_age_ms < SAFETY_GUARD_MEASUREMENT_TIMEOUT_MS) ? 1U : 0U;
  }
  else
  {
    safety_context.measurement_is_fresh = 0U;
    safety_context.measurement_age_ms = 0xFFFFFFFFU;
  }

  safety_context.measurement_timestamp_ms = measurement_timestamp_ms;
  safety_context.measurement_sequence_id = measurement_sequence_id;

  /* Check invariants */
  violations = Safety_Guard_CheckInvariants(
      heater_en_request,
      clamp_request,
      pwm_request_permille,
      measurement_valid,
      safety_context.measurement_is_fresh);

  if (violations != 0U)
  {
    safety_context.invariant_violation_count++;

    /* Log violations (optional, would call Debug_Log if console is active) */
    /* For now, just track the counter. */

    /* Force safe state */
    Safety_Guard_ForceSafeOff(violations);
    override_triggered = 1U;
  }
  else
  {
    /* All invariants pass: apply the requested state */
    safety_context.heater_en_active = heater_en_request;
    safety_context.heater_en_allowed = 1U;
    safety_context.clamp_state = clamp_request;
    safety_context.pwm_permille_active = pwm_request_permille;

    Safety_Guard_ApplyOutputs(heater_en_request, clamp_request, pwm_request_permille);
  }

  return override_triggered;
}

void Safety_Guard_ForceSafeOff(uint32_t reason_code)
{
  safety_context.heater_en_active = 0U;
  safety_context.heater_en_allowed = 0U;
  safety_context.clamp_state = SAFETY_CLAMP_SAFE;
  safety_context.pwm_permille_active = 0U;
  safety_context.safe_off_count++;
  safety_context.last_safe_off_reason_ms = reason_code;

  Safety_Guard_ApplyOutputs(0U, SAFETY_CLAMP_SAFE, 0U);
}

void Safety_Guard_Tick(uint32_t now_ms)
{
  /* Optional: periodic checks, watchdog resets, etc. */
  (void)now_ms;

  /* For now, this is a placeholder for future per-cycle invariant re-checks */
}

void Safety_Guard_ReportStatus(void)
{
  /* Optional: generate status report for console/logger.
     Intended to be called from Debug_Log infrastructure or health command. */

  /* For now, this is a placeholder. */
}
