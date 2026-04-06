#ifndef HEATER_H
#define HEATER_H

#include <stdint.h>

typedef struct
{
  uint32_t last_measurement_tick_ms;
  uint16_t latest_samples[8];
  uint16_t filtered_raw;
  uint16_t heater_current_raw;
  uint16_t buckboost_voltage_raw;
  uint16_t buckboost_ref_raw;
  uint16_t pwm_permille;
  uint16_t tip_temp_cdeg;
  uint16_t target_temp_cdeg;
  uint16_t power_watt_x10;
  uint16_t external_tip_temp_cdeg;
  uint16_t simulated_tip_temp_cdeg;
  uint16_t simulated_target_temp_cdeg;
  uint16_t simulated_ambient_temp_cdeg;
  uint16_t simulated_power_watt_x10;
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
void Heater_Control_SetPwmPermille(uint16_t pwm_permille);
void Heater_Control_ForceOff(void);
const HeaterControlContext *Heater_Control_GetContext(void);

#endif