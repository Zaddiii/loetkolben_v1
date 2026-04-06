#include "ui.h"

#include "app.h"
#include "calibration.h"
#include "display.h"
#include "heater.h"
#include "main.h"
#include "peripherals.h"
#include "storage.h"

#include <string.h>

enum
{
  UI_ENCODER_COUNTS_PER_DETENT = 4,
  UI_TARGET_STEP_CDEG = 50U,
  UI_TARGET_MIN_CDEG = 200U,
  UI_TARGET_MAX_CDEG = 4500U,
  UI_BUTTON_LONG_PRESS_MS = 1200U,
  UI_SAVE_DELAY_MS = 1500U
};

static UiContext ui_context;
static uint16_t ui_last_encoder_counter;
static int16_t ui_encoder_accumulator;
static GPIO_PinState ui_last_button_level;
static uint32_t ui_button_press_start_ms;
static uint8_t ui_button_long_press_handled;

static void Ui_PublishEvent(UiEvent event_id, uint16_t event_value)
{
  ui_context.last_event = (uint8_t)event_id;
  ui_context.event_value = event_value;
  ui_context.event_counter++;
}

static void Ui_SetTargetTemp(uint16_t target_temp_cdeg, uint32_t now_ms)
{
  if (target_temp_cdeg < UI_TARGET_MIN_CDEG)
  {
    target_temp_cdeg = UI_TARGET_MIN_CDEG;
  }

  if (target_temp_cdeg > UI_TARGET_MAX_CDEG)
  {
    target_temp_cdeg = UI_TARGET_MAX_CDEG;
  }

  if (ui_context.target_temp_cdeg == target_temp_cdeg)
  {
    return;
  }

  ui_context.target_temp_cdeg = target_temp_cdeg;
  ui_context.last_target_change_ms = now_ms;
  ui_context.save_pending = 1U;
  Heater_Control_SetTargetTempCdeg(target_temp_cdeg);
  Ui_PublishEvent(UI_EVENT_TARGET_CHANGED, target_temp_cdeg);
}

static void Ui_HandleEncoderDetents(int16_t detents, uint32_t now_ms)
{
  if (detents == 0)
  {
    return;
  }

  if (ui_context.screen == UI_SCREEN_MAIN)
  {
    int32_t next_target = (int32_t)ui_context.target_temp_cdeg + ((int32_t)detents * (int32_t)UI_TARGET_STEP_CDEG);
    Ui_SetTargetTemp((uint16_t)next_target, now_ms);
    return;
  }

  {
    int32_t menu_index = (int32_t)ui_context.selected_menu_item + detents;

    while (menu_index < 0)
    {
      menu_index += UI_MENU_COUNT;
    }

    ui_context.selected_menu_item = (uint8_t)(menu_index % UI_MENU_COUNT);
    Ui_PublishEvent(UI_EVENT_MENU_SELECTION_CHANGED, ui_context.selected_menu_item);
  }
}

static void Ui_EnterMenu(void)
{
  ui_context.screen = UI_SCREEN_MENU;
  ui_context.selected_menu_item = UI_MENU_START_CALIBRATION;
  Ui_PublishEvent(UI_EVENT_MENU_ENTERED, ui_context.selected_menu_item);
}

static void Ui_ExitMenu(void)
{
  ui_context.screen = UI_SCREEN_MAIN;
  Ui_PublishEvent(UI_EVENT_MENU_EXITED, 0U);
}

static void Ui_ExecuteMenuAction(void)
{
  const HeaterControlContext *heater = Heater_Control_GetContext();
  uint8_t ack_result;

  switch ((UiMenuItem)ui_context.selected_menu_item)
  {
    case UI_MENU_START_CALIBRATION:
      if (Calibration_IsBringUpSessionActive())
      {
        if (Calibration_CaptureBringUpPoint())
        {
          Ui_PublishEvent(UI_EVENT_CALIBRATION_POINT_CAPTURED, Calibration_GetSessionContext()->stored_point_count);
          return;
        }

        Ui_PublishEvent(UI_EVENT_CALIBRATION_REJECTED, heater->external_sensor_ready);
        break;
      }

      if (heater->external_sensor_ready != 0U)
      {
        if (Calibration_StartBringUpSession(HAL_GetTick(), heater->external_tip_temp_cdeg, heater->filtered_raw))
        {
          Ui_PublishEvent(UI_EVENT_CALIBRATION_STARTED, 0U);
          Ui_ExitMenu();
          return;
        }
      }

      Ui_PublishEvent(UI_EVENT_CALIBRATION_REJECTED, heater->external_sensor_ready);
      break;

    case UI_MENU_STREAM_TOGGLE:
      ui_context.stream_enabled = (uint8_t)!ui_context.stream_enabled;
      Ui_PublishEvent(UI_EVENT_STREAM_TOGGLED, ui_context.stream_enabled);
      break;

    case UI_MENU_DOCK_TOGGLE:
      Station_App_SetDocked((uint8_t)(Station_App_GetContext()->docked == 0U));
      Ui_PublishEvent(UI_EVENT_DOCK_TOGGLED, Station_App_GetContext()->docked);
      break;

    case UI_MENU_SCREEN_CYCLE:
      Ui_PublishEvent(UI_EVENT_SCREEN_CHANGED, (uint16_t)Display_CyclePage());
      break;

    case UI_MENU_FAULT_ACK:
      ack_result = Station_App_AcknowledgeFaults(STATION_FAULT_INJECTED);
      Ui_PublishEvent(UI_EVENT_FAULT_ACK_RESULT, ack_result);
      break;

    case UI_MENU_EXIT:
      Ui_ExitMenu();
      break;

    default:
      break;
  }
}

static void Ui_HandleShortPress(void)
{
  if (ui_context.screen == UI_SCREEN_MAIN)
  {
    Ui_EnterMenu();
    return;
  }

  Ui_ExecuteMenuAction();
}

static void Ui_HandleLongPress(void)
{
  if (Calibration_IsBringUpSessionActive())
  {
    if (Calibration_FinalizeBringUpSession())
    {
      (void)Storage_SaveCalibrationTable(Calibration_GetActiveTable());
      Ui_PublishEvent(UI_EVENT_CALIBRATION_FINALIZED, Calibration_GetActiveTable()->point_count);
    }
    else
    {
      Ui_PublishEvent(UI_EVENT_CALIBRATION_FINALIZE_REJECTED, Calibration_GetSessionContext()->stored_point_count);
    }
    return;
  }

  if (Station_App_GetContext()->state == STATION_STATE_FAULT)
  {
    Ui_PublishEvent(UI_EVENT_FAULT_ACK_RESULT, Station_App_AcknowledgeFaults(STATION_FAULT_INJECTED));
    return;
  }

  if (ui_context.screen == UI_SCREEN_MENU)
  {
    Ui_ExitMenu();
  }
}

void Ui_Init(uint8_t status_stream_enabled)
{
  memset(&ui_context, 0, sizeof(ui_context));
  ui_context.screen = UI_SCREEN_MAIN;
  ui_context.selected_menu_item = UI_MENU_START_CALIBRATION;
  ui_context.target_temp_cdeg = Heater_Control_GetContext()->target_temp_cdeg;
  ui_context.stream_enabled = status_stream_enabled;

  if (HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL) != HAL_OK)
  {
    Error_Handler();
  }

  ui_last_encoder_counter = (uint16_t)__HAL_TIM_GET_COUNTER(&htim5);
  ui_last_button_level = HAL_GPIO_ReadPin(ENCODER_SW_GPIO_Port, ENCODER_SW_Pin);
}

void Ui_Tick(uint32_t now_ms)
{
  uint16_t encoder_counter = (uint16_t)__HAL_TIM_GET_COUNTER(&htim5);
  int16_t encoder_delta = (int16_t)(encoder_counter - ui_last_encoder_counter);
  GPIO_PinState button_level = HAL_GPIO_ReadPin(ENCODER_SW_GPIO_Port, ENCODER_SW_Pin);

  ui_last_encoder_counter = encoder_counter;
  ui_context.target_temp_cdeg = Heater_Control_GetContext()->target_temp_cdeg;

  if (encoder_delta != 0)
  {
    ui_encoder_accumulator = (int16_t)(ui_encoder_accumulator + encoder_delta);

    while (ui_encoder_accumulator >= UI_ENCODER_COUNTS_PER_DETENT)
    {
      ui_encoder_accumulator = (int16_t)(ui_encoder_accumulator - UI_ENCODER_COUNTS_PER_DETENT);
      Ui_HandleEncoderDetents(1, now_ms);
    }

    while (ui_encoder_accumulator <= -UI_ENCODER_COUNTS_PER_DETENT)
    {
      ui_encoder_accumulator = (int16_t)(ui_encoder_accumulator + UI_ENCODER_COUNTS_PER_DETENT);
      Ui_HandleEncoderDetents(-1, now_ms);
    }
  }

  if ((button_level == ENCODER_SW_ACTIVE_LEVEL) && (ui_last_button_level != ENCODER_SW_ACTIVE_LEVEL))
  {
    ui_button_press_start_ms = now_ms;
    ui_button_long_press_handled = 0U;
  }

  if ((button_level == ENCODER_SW_ACTIVE_LEVEL) && (ui_last_button_level == ENCODER_SW_ACTIVE_LEVEL) &&
      (ui_button_long_press_handled == 0U) &&
      ((now_ms - ui_button_press_start_ms) >= UI_BUTTON_LONG_PRESS_MS))
  {
    ui_button_long_press_handled = 1U;
    Ui_HandleLongPress();
  }

  if ((button_level != ENCODER_SW_ACTIVE_LEVEL) && (ui_last_button_level == ENCODER_SW_ACTIVE_LEVEL) &&
      (ui_button_long_press_handled == 0U))
  {
    Ui_HandleShortPress();
  }

  ui_last_button_level = button_level;

  if ((ui_context.save_pending != 0U) && ((now_ms - ui_context.last_target_change_ms) >= UI_SAVE_DELAY_MS))
  {
    ui_context.last_storage_result = Storage_SaveTargetTemp(ui_context.target_temp_cdeg) ? 1U : 0U;
    ui_context.save_pending = 0U;
    Ui_PublishEvent(UI_EVENT_TARGET_SAVED, ui_context.target_temp_cdeg);
  }
}

void Ui_SetStreamEnabled(uint8_t enabled)
{
  ui_context.stream_enabled = enabled != 0U;
}

const UiContext *Ui_GetContext(void)
{
  return &ui_context;
}

const char *Ui_GetMenuItemName(UiMenuItem item)
{
  switch (item)
  {
    case UI_MENU_START_CALIBRATION:
      return "START_CAL";

    case UI_MENU_STREAM_TOGGLE:
      return "STREAM";

    case UI_MENU_DOCK_TOGGLE:
      return "DOCK";

    case UI_MENU_SCREEN_CYCLE:
      return "SCREEN";

    case UI_MENU_FAULT_ACK:
      return "FAULT_ACK";

    case UI_MENU_EXIT:
      return "EXIT";

    default:
      return "UNKNOWN";
  }
}