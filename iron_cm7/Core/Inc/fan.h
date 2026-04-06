#ifndef FAN_H
#define FAN_H

#include <stdint.h>

#include "app.h"
#include "heater.h"

typedef struct
{
  uint32_t last_tick_ms;
  uint16_t pwm_permille;
  uint16_t requested_pwm_permille;
  uint16_t tach_rpm;
  uint8_t enabled;
  uint8_t cooling_request;
} FanContext;

void Fan_Init(void);
void Fan_Tick(uint32_t now_ms, const StationContext *station, const HeaterControlContext *heater);
const FanContext *Fan_GetContext(void);

#endif