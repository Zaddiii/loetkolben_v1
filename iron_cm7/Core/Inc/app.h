#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum
{
  STATION_STATE_BOOT = 0,
  STATION_STATE_IDLE,
  STATION_STATE_HEATING,
  STATION_STATE_READY,
  STATION_STATE_CALIBRATION,
  STATION_STATE_NO_CALIBRATION,
  STATION_STATE_STANDBY,
  STATION_STATE_FAULT
} StationState;

typedef enum
{
  STATION_OPERATING_MODE_IDLE = 0,
  STATION_OPERATING_MODE_HEATUP,
  STATION_OPERATING_MODE_HOLD,
  STATION_OPERATING_MODE_COOLDOWN,
  STATION_OPERATING_MODE_STANDBY,
  STATION_OPERATING_MODE_CALIBRATION,
  STATION_OPERATING_MODE_FAULT
} StationOperatingMode;

typedef struct
{
  StationState state;
  StationOperatingMode operating_mode;
  uint32_t fault_flags;
  uint32_t active_fault_flags;
  uint32_t warning_flags;
  uint32_t last_tick_ms;
  uint16_t effective_target_temp_cdeg;
  uint8_t calibration_valid;
  uint8_t fault_ack_pending;
  uint8_t docked;
  uint8_t standby_active;
} StationContext;

enum
{
  STATION_FAULT_OVERCURRENT = (1UL << 0),
  STATION_FAULT_BUCKBOOST = (1UL << 1),
  STATION_FAULT_OPAMP_PGOOD = (1UL << 2),
  STATION_FAULT_TIP_OVERTEMP = (1UL << 3),
  STATION_FAULT_AMBIENT_OVERTEMP = (1UL << 4),
  STATION_FAULT_AMBIENT_SENSOR = (1UL << 5),
  STATION_FAULT_FAN_TACH = (1UL << 6),
  STATION_FAULT_INJECTED = (1UL << 31)
};

enum
{
  STATION_FAULT_CRITICAL_MASK =
      (STATION_FAULT_OVERCURRENT | STATION_FAULT_TIP_OVERTEMP | STATION_FAULT_AMBIENT_OVERTEMP | STATION_FAULT_FAN_TACH),
  STATION_FAULT_ACK_MASK =
      (STATION_FAULT_OVERCURRENT | STATION_FAULT_TIP_OVERTEMP | STATION_FAULT_AMBIENT_OVERTEMP | STATION_FAULT_FAN_TACH)
};

enum
{
  STATION_WARNING_MCP9808_ERROR = (1UL << 0),
  STATION_WARNING_AMBIENT_SENSOR_MISSING = (1UL << 1),
  STATION_WARNING_INA238_ERROR = (1UL << 2),
  STATION_WARNING_INA238_ALERT = (1UL << 3),
  STATION_WARNING_FAULT_BUCKBOOST_IGNORED = (1UL << 4),
  STATION_WARNING_FAULT_OPAMP_PGOOD_IGNORED = (1UL << 5),
  STATION_WARNING_FAULT_TIP_OVERTEMP_IGNORED = (1UL << 6),
  STATION_WARNING_FAULT_AMBIENT_OVERTEMP_IGNORED = (1UL << 7),
  STATION_WARNING_FAULT_AMBIENT_SENSOR_IGNORED = (1UL << 8),
  STATION_WARNING_FAULT_INJECTED_IGNORED = (1UL << 9)
};

void Station_App_Init(void);
void Station_App_Tick(uint32_t now_ms);
void Station_SafeShutdown(void);
void Station_App_RequestFault(uint32_t fault_mask);
void Station_App_ClearFault(uint32_t fault_mask);
uint8_t Station_App_AcknowledgeFaults(uint32_t clearable_fault_mask);
void Station_App_SetDocked(uint8_t docked);
const StationContext *Station_App_GetContext(void);

#endif