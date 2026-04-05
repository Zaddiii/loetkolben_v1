#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum
{
  STATION_STATE_BOOT = 0,
  STATION_STATE_IDLE,
  STATION_STATE_NO_CALIBRATION,
  STATION_STATE_FAULT
} StationState;

typedef struct
{
  StationState state;
  uint32_t fault_flags;
  uint32_t last_tick_ms;
  uint8_t calibration_valid;
} StationContext;

enum
{
  STATION_FAULT_OVERCURRENT = (1UL << 0),
  STATION_FAULT_BUCKBOOST = (1UL << 1),
  STATION_FAULT_OPAMP_PGOOD = (1UL << 2),
  STATION_FAULT_INJECTED = (1UL << 31)
};

void Station_App_Init(void);
void Station_App_Tick(uint32_t now_ms);
void Station_SafeShutdown(void);
void Station_App_RequestFault(uint32_t fault_mask);
const StationContext *Station_App_GetContext(void);

#endif