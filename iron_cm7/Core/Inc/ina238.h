#ifndef INA238_H
#define INA238_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t last_update_tick_ms;
  uint32_t last_init_attempt_tick_ms;
  uint16_t shunt_voltage_raw;
  uint16_t bus_voltage_raw;
  uint16_t current_raw;
  uint16_t power_raw;
  uint16_t bus_voltage_mv;
  uint16_t current_ma;
  uint16_t power_mw;
  uint16_t manufacturer_id;
  uint16_t device_id;
  uint8_t initialized;
  uint8_t data_ready;
  uint8_t alert_active;
  uint8_t last_error;
  uint8_t last_hal_status;
} Ina238Context;

bool Ina238_Init(void);
bool Ina238_Poll(uint32_t now_ms);
const Ina238Context *Ina238_GetContext(void);

#endif