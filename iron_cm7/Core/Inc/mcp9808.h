#ifndef MCP9808_H
#define MCP9808_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t last_update_tick_ms;
  uint32_t last_init_attempt_tick_ms;
  int16_t ambient_temp_cdeg;
  uint16_t last_manufacturer_id;
  uint16_t last_device_id;
  uint8_t initialized;
  uint8_t data_ready;
  uint8_t last_error;
  uint8_t last_hal_status;
} Mcp9808Context;

bool Mcp9808_Init(void);
bool Mcp9808_Poll(uint32_t now_ms);
const Mcp9808Context *Mcp9808_GetContext(void);

#endif