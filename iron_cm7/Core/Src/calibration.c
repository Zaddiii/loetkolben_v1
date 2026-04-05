#include "calibration.h"

#include "main.h"

#include <string.h>

enum
{
  CALIBRATION_SIGNATURE = 0x43414C31UL,
  CALIBRATION_REFERENCE_AMBIENT_CDEG = 250,
  CALIBRATION_MIN_FINAL_POINTS = 3U,
  CALIBRATION_MAX_TIP_TEMP_CDEG = 4500U
};

static CalibrationTable calibration_active_table;
static CalibrationTable calibration_pending_table;

static void Calibration_LoadDefaultTable(void)
{
  static const CalibrationPoint default_points[CALIBRATION_MAX_POINTS] = {
      {250U, 420U, 1U},
      {1000U, 1160U, 1U},
      {1500U, 1660U, 1U},
      {2000U, 2180U, 1U},
      {2500U, 2720U, 1U},
      {3000U, 3290U, 1U},
      {3500U, 3890U, 1U},
      {4000U, 4520U, 1U}};

  memcpy(calibration_active_table.points, default_points, sizeof(default_points));
  calibration_active_table.signature = CALIBRATION_SIGNATURE;
  calibration_active_table.point_count = CALIBRATION_MAX_POINTS;
#if IRON_SIMULATION_MODE
  calibration_active_table.valid = 1U;
#else
  calibration_active_table.valid = 0U;
#endif
}

static void Calibration_ResetPendingTable(void)
{
  memset(&calibration_pending_table, 0, sizeof(calibration_pending_table));
  calibration_pending_table.signature = CALIBRATION_SIGNATURE;
}

void Calibration_Init(void)
{
  memset(&calibration_active_table, 0, sizeof(calibration_active_table));
  Calibration_LoadDefaultTable();
  Calibration_ResetPendingTable();
}

bool Calibration_HasValidTable(void)
{
  return calibration_active_table.valid != 0U;
}

bool Calibration_ConvertRawToTipCdeg(uint16_t raw_value, int16_t ambient_temp_cdeg, uint16_t *tip_temp_cdeg)
{
  const CalibrationTable *table = &calibration_active_table;
  uint8_t index;
  int32_t compensated_temp_cdeg;

  if ((tip_temp_cdeg == NULL) || (table->valid == 0U) || (table->point_count == 0U))
  {
    return false;
  }

  if (raw_value <= table->points[0].internal_raw)
  {
    compensated_temp_cdeg = table->points[0].tip_temp_cdeg;
  }
  else if (raw_value >= table->points[table->point_count - 1U].internal_raw)
  {
    compensated_temp_cdeg = table->points[table->point_count - 1U].tip_temp_cdeg;
  }
  else
  {
    compensated_temp_cdeg = table->points[0].tip_temp_cdeg;

    for (index = 1U; index < table->point_count; ++index)
    {
      const CalibrationPoint *lower = &table->points[index - 1U];
      const CalibrationPoint *upper = &table->points[index];

      if (raw_value <= upper->internal_raw)
      {
        uint32_t raw_span = (uint32_t)upper->internal_raw - (uint32_t)lower->internal_raw;
        uint32_t temp_span = (uint32_t)upper->tip_temp_cdeg - (uint32_t)lower->tip_temp_cdeg;
        uint32_t raw_offset = (uint32_t)raw_value - (uint32_t)lower->internal_raw;

        compensated_temp_cdeg = (int32_t)lower->tip_temp_cdeg + (int32_t)((temp_span * raw_offset) / raw_span);
        break;
      }
    }
  }

  compensated_temp_cdeg += (int32_t)ambient_temp_cdeg - CALIBRATION_REFERENCE_AMBIENT_CDEG;

  if (compensated_temp_cdeg < 0)
  {
    compensated_temp_cdeg = 0;
  }

  if (compensated_temp_cdeg > CALIBRATION_MAX_TIP_TEMP_CDEG)
  {
    compensated_temp_cdeg = CALIBRATION_MAX_TIP_TEMP_CDEG;
  }

  *tip_temp_cdeg = (uint16_t)compensated_temp_cdeg;
  return true;
}

bool Calibration_BeginSession(void)
{
  Calibration_ResetPendingTable();
  return true;
}

bool Calibration_StorePoint(uint16_t external_tip_temp_cdeg, uint16_t internal_raw)
{
  uint8_t insert_index;
  uint8_t index;

  if (calibration_pending_table.point_count >= CALIBRATION_MAX_POINTS)
  {
    return false;
  }

  insert_index = calibration_pending_table.point_count;

  for (index = 0U; index < calibration_pending_table.point_count; ++index)
  {
    if (external_tip_temp_cdeg < calibration_pending_table.points[index].tip_temp_cdeg)
    {
      insert_index = index;
      break;
    }
  }

  for (index = calibration_pending_table.point_count; index > insert_index; --index)
  {
    calibration_pending_table.points[index] = calibration_pending_table.points[index - 1U];
  }

  calibration_pending_table.points[insert_index].tip_temp_cdeg = external_tip_temp_cdeg;
  calibration_pending_table.points[insert_index].internal_raw = internal_raw;
  calibration_pending_table.points[insert_index].valid = 1U;
  calibration_pending_table.point_count++;

  return true;
}

bool Calibration_FinalizeSession(void)
{
  uint8_t index;

  if (calibration_pending_table.point_count < CALIBRATION_MIN_FINAL_POINTS)
  {
    return false;
  }

  for (index = 1U; index < calibration_pending_table.point_count; ++index)
  {
    if (calibration_pending_table.points[index].internal_raw <= calibration_pending_table.points[index - 1U].internal_raw)
    {
      return false;
    }
  }

  calibration_pending_table.valid = 1U;
  calibration_active_table = calibration_pending_table;
  return true;
}

const CalibrationTable *Calibration_GetActiveTable(void)
{
  return &calibration_active_table;
}

const CalibrationTable *Calibration_GetPendingTable(void)
{
  return &calibration_pending_table;
}