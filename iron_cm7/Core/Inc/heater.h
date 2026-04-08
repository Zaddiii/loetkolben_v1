#ifndef HEATER_H
#define HEATER_H

#include <stdint.h>

/**
 * Measurement metadata: sequence tracking, freshness, and error flags.
 * Enables explicit control-path decisions based on measurement age/validity.
 */
typedef struct
{
  uint32_t sequence_id;        ///< Monotonic sequence number per completed measurement cycle
  uint32_t timestamp_ms;       ///< HAL_GetTick() when measurement completed
  uint8_t  valid;              ///< 1 if measurement passed all sanity checks, 0 otherwise
  uint16_t age_ms;             ///< Age of measurement relative to now [ms], clamped to 65535
  uint16_t error_flags;        ///< Bitmask: ADC errors, sensor errors, timeout errors, etc.
} MeasurementMetadata;

/**
 * Heater control context: sensor readings, control outputs, simulation state.
 */
typedef struct
{
  /* Measurement metadata (Phase 3) */
  MeasurementMetadata measurement;

  uint32_t last_measurement_tick_ms;
  uint16_t latest_samples[8];
  uint16_t filtered_raw;
  uint16_t heater_current_raw;
  uint16_t heater_current_ma;
  uint16_t buckboost_voltage_raw;
  uint16_t buckboost_voltage_mv;
  uint16_t buckboost_ref_raw;
  uint16_t buckboost_target_mv;
  uint16_t pwm_permille;
  uint16_t tip_temp_cdeg;
  uint16_t target_temp_cdeg;
  uint16_t effective_target_temp_cdeg;
  uint16_t power_watt_x10;
  uint16_t external_tip_temp_cdeg;
  uint16_t simulated_tip_temp_cdeg;
  uint16_t simulated_target_temp_cdeg;
  uint16_t simulated_ambient_temp_cdeg;
  uint16_t simulated_ambient_base_temp_cdeg;
  uint16_t simulated_external_tip_temp_cdeg;
  uint16_t simulated_power_watt_x10;
  uint16_t simulated_requested_pwm_permille;
  uint16_t simulated_thermal_load_permille;
  int16_t ambient_temp_cdeg;
  int16_t external_ambient_temp_cdeg;
  uint8_t sample_count;
  uint8_t measurement_ready;
  uint8_t calibration_valid;
  uint8_t ambient_sensor_ready;
  uint8_t external_sensor_ready;
} HeaterControlContext;

void Heater_Control_Init(void);
void Heater_Control_Tick(uint32_t now_ms);
void Heater_Control_SetTargetTempCdeg(uint16_t target_temp_cdeg);
void Heater_Control_SetEffectiveTargetTempCdeg(uint16_t target_temp_cdeg);
void Heater_Control_SetPwmPermille(uint16_t pwm_permille);
void Heater_Control_ForceOff(void);
void Heater_Simulation_Reset(void);
void Heater_Simulation_SetAmbientTempCdeg(uint16_t ambient_temp_cdeg);
void Heater_Simulation_SetThermalLoadPermille(uint16_t thermal_load_permille);
void Heater_Simulation_SetTipTempCdeg(uint16_t tip_temp_cdeg);
void Heater_Test_SetAmbientTempCdeg(uint16_t ambient_temp_cdeg);
uint16_t Heater_Test_GetAmbientTempCdeg(void);
const HeaterControlContext *Heater_Control_GetContext(void);

#endif