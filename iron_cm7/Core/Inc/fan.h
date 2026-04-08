#ifndef FAN_H
#define FAN_H

#include <stdint.h>

#include "app.h"
#include "heater.h"

typedef struct
{
  uint32_t last_tick_ms;
  uint32_t last_tach_sample_ms;
  uint32_t last_tach_pulse_count;
  uint16_t pwm_permille;
  uint16_t requested_pwm_permille;
  uint16_t tach_rpm;
  uint16_t target_rpm;
  uint16_t tach_error_rpm;
  uint32_t tach_pulses_window;
  uint32_t tach_window_ms;
  uint32_t tach_signal_hz_x100;
  uint32_t tach_valid_edges;
  uint32_t tach_rejected_edges;
  uint32_t tach_calibration_ppm;
  uint8_t enabled;
  uint8_t cooling_request;
  uint8_t tach_ok;
} FanContext;

void Fan_Init(void);
void Fan_Tick(uint32_t now_ms, const StationContext *station, const HeaterControlContext *heater);
const FanContext *Fan_GetContext(void);
void Fan_SetRpmCalibrationPpm(uint32_t calibration_ppm);
uint32_t Fan_GetRpmCalibrationPpm(void);
uint32_t Fan_GetTachTimerClockHz(void);
uint32_t Fan_GetTachTickHz(void);
uint16_t Fan_GetTachPrescaler(void);
uint32_t Fan_DebugCopyRawPeriods(uint32_t *out_period_ticks, uint32_t max_count);

#endif