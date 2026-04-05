#ifndef HEATER_H
#define HEATER_H

#include <stdint.h>

typedef struct
{
  uint32_t last_measurement_tick_ms;
  uint16_t latest_samples[8];
  uint16_t filtered_raw;
  uint16_t pwm_permille;
  uint8_t sample_count;
  uint8_t measurement_ready;
} HeaterControlContext;

void Heater_Control_Init(void);
void Heater_Control_Tick(uint32_t now_ms);
void Heater_Control_SetPwmPermille(uint16_t pwm_permille);
void Heater_Control_ForceOff(void);
const HeaterControlContext *Heater_Control_GetContext(void);

#endif