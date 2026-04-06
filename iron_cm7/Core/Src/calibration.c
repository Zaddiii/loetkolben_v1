#include "calibration.h"

#include "main.h"

#include <string.h>

enum
{
  CALIBRATION_SIGNATURE = 0x43414C31UL,
  CALIBRATION_REFERENCE_AMBIENT_CDEG = 250,
  CALIBRATION_MIN_FINAL_POINTS = 3U,
  CALIBRATION_MAX_TIP_TEMP_CDEG = 4500U,
  CALIBRATION_STABILITY_BAND_CDEG = 20U,
  CALIBRATION_STABILITY_HOLD_MS = 2000U,
  CALIBRATION_STABILITY_DELTA_LIMIT_CDEG = 5U
};

static CalibrationTable calibration_active_table;
static CalibrationTable calibration_pending_table;
static CalibrationSessionContext calibration_session;
static uint32_t calibration_session_stable_since_ms;
static uint32_t calibration_session_external_sum;
static uint32_t calibration_session_raw_sum;
static uint16_t calibration_session_sample_count;

static const uint16_t calibration_targets_cdeg[CALIBRATION_MAX_POINTS - 1U] = {
    1000U,
    1500U,
    2000U,
    2500U,
    3000U,
    3500U,
    4000U};

static void Calibration_ResetSessionState(void)
{
  memset(&calibration_session, 0, sizeof(calibration_session));
  calibration_session_stable_since_ms = 0U;
  calibration_session_external_sum = 0U;
  calibration_session_raw_sum = 0U;
  calibration_session_sample_count = 0U;
}

static uint16_t Calibration_GetTargetForIndex(uint8_t target_index)
{
  if (target_index >= (uint8_t)(sizeof(calibration_targets_cdeg) / sizeof(calibration_targets_cdeg[0])))
  {
    return 0U;
  }

  return calibration_targets_cdeg[target_index];
}

static void Calibration_AdvanceToNextTarget(void)
{
  calibration_session.next_target_index++;
  calibration_session.target_temp_cdeg = Calibration_GetTargetForIndex(calibration_session.next_target_index);
  calibration_session.stable = 0U;
  calibration_session.capture_ready = 0U;
  calibration_session.stable_time_ms = 0U;
  calibration_session.averaged_external_tip_temp_cdeg = 0U;
  calibration_session.averaged_internal_raw = 0U;
  calibration_session_stable_since_ms = 0U;
  calibration_session_external_sum = 0U;
  calibration_session_raw_sum = 0U;
  calibration_session_sample_count = 0U;
}

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
#if IRON_VIRTUAL_CALIBRATION
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
  Calibration_ResetSessionState();
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

bool Calibration_StartBringUpSession(uint32_t now_ms, uint16_t external_tip_temp_cdeg, uint16_t internal_raw)
{
  if (!Calibration_BeginSession())
  {
    return false;
  }

  Calibration_ResetSessionState();
  calibration_session.active = 1U;
  calibration_session.last_external_tip_temp_cdeg = external_tip_temp_cdeg;

  if (!Calibration_StorePoint(external_tip_temp_cdeg, internal_raw))
  {
    Calibration_ResetSessionState();
    return false;
  }

  calibration_session.stored_point_count = calibration_pending_table.point_count;
  calibration_session.next_target_index = 0U;
  calibration_session.target_temp_cdeg = Calibration_GetTargetForIndex(0U);
  calibration_session_stable_since_ms = now_ms;
  return true;
}

void Calibration_ProcessBringUp(uint32_t now_ms, uint16_t external_tip_temp_cdeg, uint16_t internal_raw, uint8_t external_sensor_ready)
{
  uint16_t previous_external_tip_temp_cdeg = calibration_session.last_external_tip_temp_cdeg;
  uint16_t target_temp_cdeg;
  uint16_t temp_delta_cdeg;

  if ((calibration_session.active == 0U) || (external_sensor_ready == 0U))
  {
    calibration_session.stable = 0U;
    calibration_session.capture_ready = 0U;
    calibration_session.stable_time_ms = 0U;
    calibration_session_stable_since_ms = 0U;
    calibration_session_external_sum = 0U;
    calibration_session_raw_sum = 0U;
    calibration_session_sample_count = 0U;
    return;
  }

  target_temp_cdeg = calibration_session.target_temp_cdeg;

  if (target_temp_cdeg == 0U)
  {
    calibration_session.capture_ready = 1U;
    calibration_session.stable = 1U;
    calibration_session.stable_time_ms = CALIBRATION_STABILITY_HOLD_MS;
    return;
  }

  temp_delta_cdeg = (external_tip_temp_cdeg > target_temp_cdeg)
                        ? (external_tip_temp_cdeg - target_temp_cdeg)
                        : (target_temp_cdeg - external_tip_temp_cdeg);

  if ((temp_delta_cdeg > CALIBRATION_STABILITY_BAND_CDEG) ||
      ((calibration_session_stable_since_ms != 0U) &&
     (((external_tip_temp_cdeg > previous_external_tip_temp_cdeg)
       ? (external_tip_temp_cdeg - previous_external_tip_temp_cdeg)
       : (previous_external_tip_temp_cdeg - external_tip_temp_cdeg)) > CALIBRATION_STABILITY_DELTA_LIMIT_CDEG)))
  {
    calibration_session.stable = 0U;
    calibration_session.capture_ready = 0U;
    calibration_session.stable_time_ms = 0U;
    calibration_session_stable_since_ms = now_ms;
    calibration_session_external_sum = 0U;
    calibration_session_raw_sum = 0U;
    calibration_session_sample_count = 0U;
    calibration_session.last_external_tip_temp_cdeg = external_tip_temp_cdeg;
    return;
  }

  if (calibration_session_sample_count == 0U)
  {
    calibration_session_stable_since_ms = now_ms;
  }

  calibration_session_external_sum += external_tip_temp_cdeg;
  calibration_session_raw_sum += internal_raw;
  calibration_session_sample_count++;
  calibration_session.stable = 1U;
  calibration_session.stable_time_ms = now_ms - calibration_session_stable_since_ms;

  if (calibration_session_sample_count != 0U)
  {
    calibration_session.averaged_external_tip_temp_cdeg = (uint16_t)(calibration_session_external_sum / calibration_session_sample_count);
    calibration_session.averaged_internal_raw = (uint16_t)(calibration_session_raw_sum / calibration_session_sample_count);
  }

  if (calibration_session.stable_time_ms >= CALIBRATION_STABILITY_HOLD_MS)
  {
    calibration_session.capture_ready = 1U;
  }

  calibration_session.last_external_tip_temp_cdeg = external_tip_temp_cdeg;
}

bool Calibration_CaptureBringUpPoint(void)
{
  if ((calibration_session.active == 0U) || (calibration_session.capture_ready == 0U))
  {
    return false;
  }

  if ((calibration_session.averaged_external_tip_temp_cdeg == 0U) || (calibration_session_sample_count == 0U))
  {
    return false;
  }

  if (!Calibration_StorePoint(calibration_session.averaged_external_tip_temp_cdeg, calibration_session.averaged_internal_raw))
  {
    return false;
  }

  calibration_session.stored_point_count = calibration_pending_table.point_count;
  Calibration_AdvanceToNextTarget();
  return true;
}

bool Calibration_FinalizeBringUpSession(void)
{
  bool finalized;

  if (calibration_session.active == 0U)
  {
    return false;
  }

  finalized = Calibration_FinalizeSession();
  Calibration_ResetSessionState();
  return finalized;
}

void Calibration_CancelBringUpSession(void)
{
  Calibration_ResetPendingTable();
  Calibration_ResetSessionState();
}

bool Calibration_IsBringUpSessionActive(void)
{
  return calibration_session.active != 0U;
}

bool Calibration_LoadPersistedTable(const CalibrationTable *table)
{
  if ((table == NULL) || (table->signature != CALIBRATION_SIGNATURE) || (table->point_count > CALIBRATION_MAX_POINTS))
  {
    return false;
  }

  calibration_active_table = *table;
  return calibration_active_table.valid != 0U;
}

const CalibrationTable *Calibration_GetActiveTable(void)
{
  return &calibration_active_table;
}

const CalibrationTable *Calibration_GetPendingTable(void)
{
  return &calibration_pending_table;
}

const CalibrationSessionContext *Calibration_GetSessionContext(void)
{
  return &calibration_session;
}