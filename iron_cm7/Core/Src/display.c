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
  DISPLAY_REFRESH_PERIOD_MS = 16U,
  DISPLAY_HW_RETRY_PERIOD_MS = 5000U,
  DISPLAY_MAIN_TIP_UPDATE_MS = 120U,
  DISPLAY_MAIN_SET_UPDATE_MS = 250U,
  DISPLAY_COLOR_BLACK = 0x0000U,
  DISPLAY_COLOR_HEADER_BLUE = 0x001F,
  DISPLAY_COLOR_HEADER_TEXT = 0xFD20U,
  DISPLAY_COLOR_TIP_TEXT = 0xFFFFU,
  DISPLAY_COLOR_SET_TEXT = 0x07FFU
};

static DisplayContext display_context;
static uint32_t display_last_hw_init_attempt_ms;
static uint32_t display_last_tip_draw_ms;
static uint32_t display_last_set_draw_ms;
static uint8_t display_main_layout_drawn;
static int16_t display_tip_shown_deg;
static int16_t display_set_shown_deg;

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
    case DISPLAY_PAGE_MAIN:
      return "MAIN";

    case DISPLAY_PAGE_CALIBRATION:
      return "CAL";

    default:
      return "UNK";
  }
}

static uint8_t Display_SetLine(uint8_t index, const char *text)
{
  if ((index >= DISPLAY_LINE_COUNT) || (text == NULL))
  {
    return 0U;
  }

  if (display_context.lines[index][0] != '\0' && strncmp(display_context.lines[index], text, DISPLAY_LINE_LENGTH - 1U) == 0)
  {
    return 0U;
  }

  (void)snprintf(display_context.lines[index], DISPLAY_LINE_LENGTH, "%s", text);
  return 1U;
}

void Display_Init(void)
{
  memset(&display_context, 0, sizeof(display_context));
  display_context.page = DISPLAY_PAGE_MAIN;
  display_last_hw_init_attempt_ms = HAL_GetTick();
  display_last_tip_draw_ms = 0U;
  display_last_set_draw_ms = 0U;
  display_main_layout_drawn = 0U;
  display_tip_shown_deg = -32768;
  display_set_shown_deg = -32768;
  (void)St7789_Init();
}

void Display_Tick(uint32_t now_ms)
{
  const HeaterControlContext *heater = Heater_Control_GetContext();
  const UiContext *ui = Ui_GetContext();
  const CalibrationSessionContext *session = Calibration_GetSessionContext();
  char line_buffer[DISPLAY_LINE_LENGTH];
  uint8_t changed_mask = 0U;

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

  if ((DisplayPage)display_context.page == DISPLAY_PAGE_MAIN || ui->screen == UI_SCREEN_MAIN)
  {
    if (ui->screen == UI_SCREEN_MENU)
    {
      display_main_layout_drawn = 0U;
      (void)snprintf(line_buffer, sizeof(line_buffer), "MENU");
      if (Display_SetLine(0U, line_buffer) != 0U)
      {
        changed_mask |= (uint8_t)(1U << 0U);
      }
      (void)snprintf(line_buffer, sizeof(line_buffer), "%s", Ui_GetMenuItemName((UiMenuItem)ui->selected_menu_item));
      if (Display_SetLine(1U, line_buffer) != 0U)
      {
        changed_mask |= (uint8_t)(1U << 1U);
      }
      (void)snprintf(line_buffer, sizeof(line_buffer), "");
      if (Display_SetLine(2U, line_buffer) != 0U)
      {
        changed_mask |= (uint8_t)(1U << 2U);
      }
      (void)snprintf(line_buffer, sizeof(line_buffer), "");
      if (Display_SetLine(3U, line_buffer) != 0U)
      {
        changed_mask |= (uint8_t)(1U << 3U);
      }
    }
    else
    {
      int16_t tip_actual_deg = (int16_t)(heater->tip_temp_cdeg / 10U);
      int16_t set_actual_deg = (int16_t)(ui->target_temp_cdeg / 10U);
      uint8_t tip_due = 0U;
      uint8_t set_due = 0U;

      if ((display_main_layout_drawn == 0U) && (St7789_GetContext()->initialized != 0U))
      {
        (void)St7789_ClearArea(0U, 0U, 320U, 170U, DISPLAY_COLOR_BLACK);
        (void)St7789_ClearArea(0U, 0U, 320U, 28U, DISPLAY_COLOR_HEADER_BLUE);
        (void)St7789_DrawText(12U, 8U, "SOLDERING IRON", DISPLAY_COLOR_HEADER_TEXT, DISPLAY_COLOR_HEADER_BLUE, 2U);
        display_main_layout_drawn = 1U;
        display_tip_shown_deg = -32768;
        display_set_shown_deg = -32768;
        display_last_tip_draw_ms = 0U;
        display_last_set_draw_ms = 0U;
      }

      if (display_tip_shown_deg == -32768)
      {
        display_tip_shown_deg = tip_actual_deg;
      }

      if ((now_ms - display_last_tip_draw_ms) >= DISPLAY_MAIN_TIP_UPDATE_MS)
      {
        display_last_tip_draw_ms = now_ms;
        tip_due = 1U;

        if (tip_actual_deg > display_tip_shown_deg)
        {
          display_tip_shown_deg++;
        }
        else if (tip_actual_deg < display_tip_shown_deg)
        {
          display_tip_shown_deg--;
        }
      }

      if (((now_ms - display_last_set_draw_ms) >= DISPLAY_MAIN_SET_UPDATE_MS) || (set_actual_deg != display_set_shown_deg))
      {
        display_last_set_draw_ms = now_ms;
        display_set_shown_deg = set_actual_deg;
        set_due = 1U;
      }

      if ((St7789_GetContext()->initialized != 0U) && (tip_due != 0U))
      {
        (void)St7789_ClearArea(8U, 42U, 304U, 66U, DISPLAY_COLOR_BLACK);
        (void)snprintf(line_buffer, sizeof(line_buffer), "%dC", (int)display_tip_shown_deg);
        (void)St7789_DrawText(28U, 52U, line_buffer, DISPLAY_COLOR_TIP_TEXT, DISPLAY_COLOR_BLACK, 6U);
      }

      if ((St7789_GetContext()->initialized != 0U) && (set_due != 0U))
      {
        (void)St7789_ClearArea(8U, 118U, 304U, 38U, DISPLAY_COLOR_BLACK);
        (void)snprintf(line_buffer, sizeof(line_buffer), "SET %dC", (int)display_set_shown_deg);
        (void)St7789_DrawText(18U, 124U, line_buffer, DISPLAY_COLOR_SET_TEXT, DISPLAY_COLOR_BLACK, 3U);
      }

      display_context.version++;
      return;
    }
  }
  else if ((DisplayPage)display_context.page == DISPLAY_PAGE_CALIBRATION || session->active != 0U)
  {
    display_main_layout_drawn = 0U;
    (void)snprintf(line_buffer, sizeof(line_buffer), "CALIBRATION");
    if (Display_SetLine(0U, line_buffer) != 0U)
    {
      changed_mask |= (uint8_t)(1U << 0U);
    }
    (void)snprintf(line_buffer, sizeof(line_buffer), "NEXT %u.%uC", (unsigned int)(session->target_temp_cdeg / 10U), (unsigned int)(session->target_temp_cdeg % 10U));
    if (Display_SetLine(1U, line_buffer) != 0U)
    {
      changed_mask |= (uint8_t)(1U << 1U);
    }
    (void)snprintf(line_buffer, sizeof(line_buffer), "HOLD %lums", (unsigned long)session->stable_time_ms);
    if (Display_SetLine(2U, line_buffer) != 0U)
    {
      changed_mask |= (uint8_t)(1U << 2U);
    }
    (void)snprintf(line_buffer, sizeof(line_buffer), "%u POINTS", (unsigned int)(session->active ? session->stored_point_count : Calibration_GetActiveTable()->point_count));
    if (Display_SetLine(3U, line_buffer) != 0U)
    {
      changed_mask |= (uint8_t)(1U << 3U);
    }
  }

  if (changed_mask != 0U)
  {
    if (St7789_GetContext()->initialized != 0U)
    {
      uint8_t line_index;

      for (line_index = 0U; line_index < DISPLAY_LINE_COUNT; ++line_index)
      {
        if ((changed_mask & (uint8_t)(1U << line_index)) != 0U)
        {
          (void)St7789_DrawTextLine(line_index, display_context.lines[line_index]);
        }
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