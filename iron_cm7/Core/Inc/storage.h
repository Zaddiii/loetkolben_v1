#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "calibration.h"

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t flags;
  uint16_t target_temp_cdeg;
  uint8_t calibration_valid;
  uint8_t point_count;
  struct
  {
    uint16_t tip_temp_cdeg;
    uint16_t internal_raw;
    uint8_t valid;
    uint8_t reserved[3];
  } points[CALIBRATION_MAX_POINTS];
  uint32_t crc32;
} StorageImage;

typedef struct
{
  uint8_t initialized;
  uint8_t flash_data_valid;
  uint8_t last_error;
  uint8_t dirty;
  uint32_t last_crc32;
  StorageImage image;
} StorageContext;

bool Storage_Init(void);
bool Storage_SaveTargetTemp(uint16_t target_temp_cdeg);
bool Storage_SaveCalibrationTable(const CalibrationTable *table);
const StorageContext *Storage_GetContext(void);

#endif