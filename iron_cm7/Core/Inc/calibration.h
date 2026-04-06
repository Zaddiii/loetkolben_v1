#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#define CALIBRATION_MAX_POINTS 8U

typedef struct
{
  uint16_t tip_temp_cdeg;
  uint16_t internal_raw;
  uint8_t valid;
} CalibrationPoint;

typedef struct
{
  uint32_t signature;
  uint8_t point_count;
  uint8_t valid;
  CalibrationPoint points[CALIBRATION_MAX_POINTS];
} CalibrationTable;

typedef struct
{
  uint32_t stable_time_ms;
  uint16_t target_temp_cdeg;
  uint16_t last_external_tip_temp_cdeg;
  uint16_t averaged_external_tip_temp_cdeg;
  uint16_t averaged_internal_raw;
  uint8_t active;
  uint8_t stable;
  uint8_t capture_ready;
  uint8_t next_target_index;
  uint8_t stored_point_count;
} CalibrationSessionContext;

void Calibration_Init(void);
bool Calibration_HasValidTable(void);
bool Calibration_ConvertRawToTipCdeg(uint16_t raw_value, int16_t ambient_temp_cdeg, uint16_t *tip_temp_cdeg);
bool Calibration_BeginSession(void);
bool Calibration_StorePoint(uint16_t external_tip_temp_cdeg, uint16_t internal_raw);
bool Calibration_FinalizeSession(void);
bool Calibration_StartBringUpSession(uint32_t now_ms, uint16_t external_tip_temp_cdeg, uint16_t internal_raw);
void Calibration_ProcessBringUp(uint32_t now_ms, uint16_t external_tip_temp_cdeg, uint16_t internal_raw, uint8_t external_sensor_ready);
bool Calibration_CaptureBringUpPoint(void);
bool Calibration_FinalizeBringUpSession(void);
void Calibration_CancelBringUpSession(void);
bool Calibration_IsBringUpSessionActive(void);
bool Calibration_LoadPersistedTable(const CalibrationTable *table);
const CalibrationTable *Calibration_GetActiveTable(void);
const CalibrationTable *Calibration_GetPendingTable(void);
const CalibrationSessionContext *Calibration_GetSessionContext(void);

#endif