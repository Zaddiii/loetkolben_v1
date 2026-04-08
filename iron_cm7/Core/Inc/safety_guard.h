#ifndef SAFETY_GUARD_H
#define SAFETY_GUARD_H

#include <stdint.h>

/**
 * @file safety_guard.h
 * @brief Central arbitration for safety-critical outputs (HEATER_EN, TIP_CLAMP, PWM).
 *
 * Purpose:
 * This module is the single authority for setting the heating element enable state,
 * tip clamp state, and heater PWM duty cycle. All other modules must route their
 * requests through this interface rather than writing directly to GPIO/Timer registers.
 *
 * Invariants enforced:
 * - HEATER_EN may only be active if there is a fresh valid measurement.
 * - TIP_CLAMP must be in SAFE state whenever HEATER_EN is active.
 * - No PWM output shall be applied if HEATER_EN is disabled.
 * - Safe state on startup: HEATER_EN inactive, TIP_CLAMP safe, PWM = 0.
 * - On any safety fault: immediate Safe-Off (HEATER_EN inactive, TIP_CLAMP safe).
 */

/**
 * @brief Clamp states for tip protection.
 */
typedef enum
{
  SAFETY_CLAMP_SAFE = 0U,     /**< Clamp in safe state (measurement mode or heater off) */
  SAFETY_CLAMP_HOLD = 1U,     /**< Clamp in hold state (measurement window, heater may be off) */
  SAFETY_CLAMP_ERROR = 0xFFU  /**< Invalid/error state */
} SafetyClampState;

/**
 * @brief Output enable states.
 */
typedef enum
{
  SAFETY_OUTPUT_INACTIVE = 0U,
  SAFETY_OUTPUT_ACTIVE = 1U
} SafetyOutputState;

/**
 * @brief Safety context and status information.
 */
typedef struct
{
  uint8_t heater_en_active;                /**< Current HEATER_EN state (0=off, 1=on) */
  uint8_t heater_en_requested;             /**< Requested HEATER_EN state from control */
  uint8_t heater_en_allowed;               /**< Guard permission (0=no, 1=yes) */

  SafetyClampState clamp_state;            /**< Current clamp state */
  SafetyClampState clamp_requested;        /**< Requested clamp state */

  uint16_t pwm_permille_active;            /**< Current PWM output (permille) */
  uint16_t pwm_permille_requested;         /**< Requested PWM from control (permille) */

  uint32_t measurement_sequence_id;        /**< Expected measurement sequence ID for freshness */
  uint32_t measurement_timestamp_ms;       /**< Timestamp of last valid measurement */
  uint32_t measurement_age_ms;             /**< Age of current measurement [ms] */
  uint8_t measurement_is_fresh;            /**< Is current measurement fresh enough? (0=no, 1=yes) */

  uint32_t fault_flags;                    /**< Safety-relevant fault flags (bit-encoded) */
  uint32_t warning_flags;                  /**< Safety-relevant warnings (bit-encoded) */

  uint32_t safe_off_count;                 /**< Counter: how many times did we force Safe-Off? */
  uint32_t invariant_violation_count;      /**< Counter: how many times did we detect invariant violation? */
  uint32_t last_safe_off_reason_ms;        /**< Timestamp of last Safe-Off trigger */

} SafetyGuardContext;

/**
 * @brief Initialize safety guard to safe default state.
 *
 * Must be called once during firmware boot, before the main loop.
 * Sets all outputs to safe state and clears all counters.
 */
void Safety_Guard_Init(void);

/**
 * @brief Get current safety guard context (read-only).
 *
 * @return Pointer to SafetyGuardContext (const)
 */
const SafetyGuardContext *Safety_Guard_GetContext(void);

/**
 * @brief Main safety guard function: request and arbitrate outputs.
 *
 * Called from control/application logic to request a state change.
 * This function:
 * 1. Checks invariants (measurement freshness, state coherence, timeouts).
 * 2. Logs invariant violations if found.
 * 3. Applies safety rules (e.g., no EN without fresh measurement).
 * 4. Writes actual GPIO/PWM outputs only if safe.
 * 5. If unsafe, forces Safe-Off and increments fault counters.
 *
 * @param heater_en_request   Requested heater enable (0=off, 1=on)
 * @param clamp_request       Requested clamp state (SAFE or HOLD)
 * @param pwm_request_permille Requested PWM duty cycle [0..1000]
 * @param measurement_sequence_id Current measurement sequence ID (for age tracking)
 * @param measurement_timestamp_ms Timestamp of most recent measurement [ms]
 * @param measurement_valid   Is the measurement valid? (0=no, 1=yes)
 * @param now_ms             Current time [ms] for age calculation
 *
 * @return 0 if request was accepted and applied, 1 if safety override triggered
 */
uint8_t Safety_Guard_SetOutputs(
    uint8_t heater_en_request,
    SafetyClampState clamp_request,
    uint16_t pwm_request_permille,
    uint32_t measurement_sequence_id,
    uint32_t measurement_timestamp_ms,
    uint8_t measurement_valid,
    uint32_t now_ms
);

/**
 * @brief Force immediate safe state (heater off, clamp safe, pwm off).
 *
 * Used during system faults, emergency stops, or explicit application requests.
 * Generates a log entry with reason timestamp.
 *
 * @param reason_code Optional reason code or event timestamp [32-bit]
 */
void Safety_Guard_ForceSafeOff(uint32_t reason_code);

/**
 * @brief Tick function for periodic guard checks (called every ~20ms or with measurements).
 *
 * Optional: updates internal freshness tracking and can auto-expire stale measurements.
 *
 * @param now_ms Current time [ms]
 */
void Safety_Guard_Tick(uint32_t now_ms);

/**
 * @brief Report safety guard status for diagnostics.
 *
 * Intended for console output or logging subsystem.
 */
void Safety_Guard_ReportStatus(void);

#endif /* SAFETY_GUARD_H */
