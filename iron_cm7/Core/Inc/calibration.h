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

void Calibration_Init(void);
bool Calibration_HasValidTable(void);
bool Calibration_ConvertRawToTipCdeg(uint16_t raw_value, int16_t ambient_temp_cdeg, uint16_t *tip_temp_cdeg);
bool Calibration_BeginSession(void);
bool Calibration_StorePoint(uint16_t external_tip_temp_cdeg, uint16_t internal_raw);
bool Calibration_FinalizeSession(void);
const CalibrationTable *Calibration_GetActiveTable(void);
const CalibrationTable *Calibration_GetPendingTable(void);

#endif