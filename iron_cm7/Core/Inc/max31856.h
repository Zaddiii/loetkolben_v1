#ifndef MAX31856_H
#define MAX31856_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t last_update_tick_ms;
  uint16_t thermocouple_temp_cdeg;
  int16_t cold_junction_temp_cdeg;
  uint8_t fault_flags;
  uint8_t initialized;
  uint8_t data_ready;
} Max31856Context;

bool Max31856_Init(void);
bool Max31856_Poll(uint32_t now_ms);
const Max31856Context *Max31856_GetContext(void);

#endif