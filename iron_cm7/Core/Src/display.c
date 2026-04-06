#include "display.h"

#include "app.h"
#include "calibration.h"
#include "fan.h"
#include "heater.h"
#include "main.h"
#include "st7789.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

enum
{
  DISPLAY_REFRESH_PERIOD_MS = 150U,
  DISPLAY_HW_RETRY_PERIOD_MS = 2000U
};

static DisplayContext display_context;
static uint32_t display_last_hw_init_attempt_ms;

static const char *Display_GetOperatingModeName(StationOperatingMode mode)
{
  switch (mode)
  {
    case STATION_OPERATING_MODE_IDLE:
      return "IDLE";

    case STATION_OPERATING_MODE_HEATUP:
      return "HEAT";

    case STATION_OPERATING_MODE_HOLD:
      return "HOLD";

    case STATION_OPERATING_MODE_COOLDOWN:
      return "COOL";

    case STATION_OPERATING_MODE_STANDBY:
      return "STBY";

    case STATION_OPERATING_MODE_CALIBRATION:
      return "CAL";

    case STATION_OPERATING_MODE_FAULT:
      return "FAULT";

    default:
      return "UNK";
  }
}

static const char *Display_GetScreenName(UiScreen screen)
{
  return (screen == UI_SCREEN_MAIN) ? "MAIN" : "MENU";
}

const char *Display_GetPageName(DisplayPage page)
{
  switch (page)
  {
    case DISPLAY_PAGE_OVERVIEW:
      return "OVERVIEW";

    case DISPLAY_PAGE_THERMAL:
      return "THERMAL";

    case DISPLAY_PAGE_CONTROL:
      return "CONTROL";

    case DISPLAY_PAGE_CALIBRATION:
      return "CAL";

    default:
      return "UNKNOWN";
  }
}

static uint8_t Display_SetLine(uint8_t index, const char *text)
{
  if ((index >= DISPLAY_LINE_COUNT) || (text == NULL))
  {
    return 0U;
  }

  if (strncmp(display_context.lines[index], text, DISPLAY_LINE_LENGTH) == 0)
  {
    return 0U;
  }

  (void)snprintf(display_context.lines[index], DISPLAY_LINE_LENGTH, "%s", text);
  return 1U;
}

void Display_Init(void)
{
  memset(&display_context, 0, sizeof(display_context));
  display_context.page = DISPLAY_PAGE_OVERVIEW;
  display_last_hw_init_attempt_ms = HAL_GetTick();
  (void)St7789_Init();
}

void Display_Tick(uint32_t now_ms)
{
  const StationContext *station = Station_App_GetContext();
  const HeaterControlContext *heater = Heater_Control_GetContext();
  const FanContext *fan = Fan_GetContext();
  const UiContext *ui = Ui_GetContext();
  const CalibrationSessionContext *session = Calibration_GetSessionContext();
  char line_buffer[DISPLAY_LINE_LENGTH];
  uint8_t changed = 0U;

  if ((now_ms - display_context.last_refresh_ms) < DISPLAY_REFRESH_PERIOD_MS)
  {
    if ((St7789_GetContext()->initialized == 0U) && ((now_ms - display_last_hw_init_attempt_ms) >= DISPLAY_HW_RETRY_PERIOD_MS))
    {
      display_last_hw_init_attempt_ms = now_ms;
      (void)St7789_Init();
    }

    return;
  }

  display_context.last_refresh_ms = now_ms;

  if ((St7789_GetContext()->initialized == 0U) && ((now_ms - display_last_hw_init_attempt_ms) >= DISPLAY_HW_RETRY_PERIOD_MS))
  {
    display_last_hw_init_attempt_ms = now_ms;
    (void)St7789_Init();
  }

  if (station->fault_flags != 0U)
  {
    (void)snprintf(line_buffer, sizeof(line_buffer), "FAULT %s", Display_GetOperatingModeName(station->operating_mode));
    changed |= Display_SetLine(0U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "TIP %u.%uC FAN %u%%",
                   (unsigned int)(heater->tip_temp_cdeg / 10U),
                   (unsigned int)(heater->tip_temp_cdeg % 10U),
                   (unsigned int)(fan->pwm_permille / 10U));
    changed |= Display_SetLine(1U, line_buffer);
    (void)snprintf(line_buffer, sizeof(line_buffer), "LAT %08lX", station->fault_flags);
    changed |= Display_SetLine(2U, line_buffer);
    (void)snprintf(line_buffer, sizeof(line_buffer), "ACT %08lX", station->active_fault_flags);
    changed |= Display_SetLine(3U, line_buffer);
  }
  else if ((DisplayPage)display_context.page == DISPLAY_PAGE_THERMAL)
  {
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "THERM %s %s",
                   Display_GetScreenName(ui->screen),
                   station->docked ? "DOCK" : "RUN");
    changed |= Display_SetLine(0U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "TIP %u.%u AMB %d.%d",
                   (unsigned int)(heater->tip_temp_cdeg / 10U),
                   (unsigned int)(heater->tip_temp_cdeg % 10U),
                   (int)(heater->ambient_temp_cdeg / 10),
                   (int)((heater->ambient_temp_cdeg >= 0) ? (heater->ambient_temp_cdeg % 10) : -(heater->ambient_temp_cdeg % 10)));
    changed |= Display_SetLine(1U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "EXT %u.%u RAW %u",
                   (unsigned int)(heater->external_tip_temp_cdeg / 10U),
                   (unsigned int)(heater->external_tip_temp_cdeg % 10U),
                   (unsigned int)heater->filtered_raw);
    changed |= Display_SetLine(2U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "PWR %u.%uW PWM %u",
                   (unsigned int)(heater->power_watt_x10 / 10U),
                   (unsigned int)(heater->power_watt_x10 % 10U),
                   (unsigned int)heater->pwm_permille);
    changed |= Display_SetLine(3U, line_buffer);
  }
  else if ((DisplayPage)display_context.page == DISPLAY_PAGE_CONTROL)
  {
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "CTRL %s %s",
                   Display_GetOperatingModeName(station->operating_mode),
                   station->standby_active ? "STBY" : "LIVE");
    changed |= Display_SetLine(0U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "SET %u.%u EFF %u.%u",
                   (unsigned int)(heater->target_temp_cdeg / 10U),
                   (unsigned int)(heater->target_temp_cdeg % 10U),
                   (unsigned int)(heater->effective_target_temp_cdeg / 10U),
                   (unsigned int)(heater->effective_target_temp_cdeg % 10U));
    changed |= Display_SetLine(1U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "FAN %u%% RPM %u",
                   (unsigned int)(fan->pwm_permille / 10U),
                   (unsigned int)fan->tach_rpm);
    changed |= Display_SetLine(2U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "LOAD %u.%u REQ %u",
                   (unsigned int)(heater->simulated_thermal_load_permille / 10U),
                   (unsigned int)(heater->simulated_thermal_load_permille % 10U),
                   (unsigned int)heater->simulated_requested_pwm_permille);
    changed |= Display_SetLine(3U, line_buffer);
  }
  else if (((DisplayPage)display_context.page == DISPLAY_PAGE_CALIBRATION) || (session->active != 0U))
  {
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "CAL %s %uPTS",
                   session->active ? "ACTIVE" : "TABLE",
                   (unsigned int)(session->active ? session->stored_point_count : Calibration_GetActiveTable()->point_count));
    changed |= Display_SetLine(0U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "VALID %u NEXT %u.%u",
                   (unsigned int)Calibration_HasValidTable(),
                   (unsigned int)(session->target_temp_cdeg / 10U),
                   (unsigned int)(session->target_temp_cdeg % 10U));
    changed |= Display_SetLine(1U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "HOLD %lums CAP %u",
                   (unsigned long)session->stable_time_ms,
                   (unsigned int)session->capture_ready);
    changed |= Display_SetLine(2U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "EXT %u.%u RAW %u",
                   (unsigned int)(session->averaged_external_tip_temp_cdeg / 10U),
                   (unsigned int)(session->averaged_external_tip_temp_cdeg % 10U),
                   (unsigned int)session->averaged_internal_raw);
    changed |= Display_SetLine(3U, line_buffer);
  }
  else
  {
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "%s %s %s",
                   Display_GetScreenName(ui->screen),
                   Display_GetOperatingModeName(station->operating_mode),
                   station->docked ? "DOCK" : "RUN");
    changed |= Display_SetLine(0U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "TIP %u.%uC EFF %u.%uC",
                   (unsigned int)(heater->tip_temp_cdeg / 10U),
                   (unsigned int)(heater->tip_temp_cdeg % 10U),
                   (unsigned int)(heater->effective_target_temp_cdeg / 10U),
                   (unsigned int)(heater->effective_target_temp_cdeg % 10U));
    changed |= Display_SetLine(1U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "SET %u.%u FAN %u%%",
                   (unsigned int)(heater->target_temp_cdeg / 10U),
                   (unsigned int)(heater->target_temp_cdeg % 10U),
                   (unsigned int)(fan->pwm_permille / 10U));
    changed |= Display_SetLine(2U, line_buffer);
    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "%s %s",
                   Ui_GetMenuItemName((UiMenuItem)ui->selected_menu_item),
                   fan->cooling_request ? "COOL" : "OK");
    changed |= Display_SetLine(3U, line_buffer);
  }

  if (changed != 0U)
  {
    if (St7789_GetContext()->initialized != 0U)
    {
      uint8_t line_index;

      for (line_index = 0U; line_index < DISPLAY_LINE_COUNT; ++line_index)
      {
        (void)St7789_DrawTextLine(line_index, display_context.lines[line_index]);
      }
    }

    display_context.version++;
  }
}

void Display_SetPage(DisplayPage page)
{
  if (page >= DISPLAY_PAGE_COUNT)
  {
    return;
  }

  display_context.page = (uint8_t)page;
}

DisplayPage Display_CyclePage(void)
{
  display_context.page = (uint8_t)((display_context.page + 1U) % DISPLAY_PAGE_COUNT);
  return (DisplayPage)display_context.page;
}

const DisplayContext *Display_GetContext(void)
{
  return &display_context;
}