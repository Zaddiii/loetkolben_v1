#ifndef UI_H
#define UI_H

#include <stdint.h>

typedef enum
{
  UI_SCREEN_MAIN = 0,
  UI_SCREEN_MENU
} UiScreen;

typedef enum
{
  UI_MENU_START_CALIBRATION = 0,
  UI_MENU_EXIT,
  UI_MENU_COUNT
} UiMenuItem;

typedef enum
{
  UI_EVENT_NONE = 0,
  UI_EVENT_TARGET_CHANGED,
  UI_EVENT_TARGET_SAVED,
  UI_EVENT_MENU_ENTERED,
  UI_EVENT_MENU_SELECTION_CHANGED,
  UI_EVENT_MENU_EXITED,
  UI_EVENT_CALIBRATION_STARTED,
  UI_EVENT_CALIBRATION_POINT_CAPTURED,
  UI_EVENT_CALIBRATION_FINALIZED,
  UI_EVENT_CALIBRATION_FINALIZE_REJECTED,
  UI_EVENT_CALIBRATION_REJECTED,
  UI_EVENT_STREAM_TOGGLED,
  UI_EVENT_DOCK_TOGGLED,
  UI_EVENT_SCREEN_CHANGED,
  UI_EVENT_FAULT_CLEAR_REQUESTED,
  UI_EVENT_FAULT_ACK_RESULT
} UiEvent;

typedef struct
{
  UiScreen screen;
  uint8_t selected_menu_item;
  uint8_t save_pending;
  uint8_t last_storage_result;
  uint8_t last_event;
  uint8_t stream_enabled;
  uint16_t target_temp_cdeg;
  uint16_t event_value;
  uint32_t last_target_change_ms;
  uint32_t event_counter;
} UiContext;

void Ui_Init(uint8_t status_stream_enabled);
void Ui_Tick(uint32_t now_ms);
void Ui_SetStreamEnabled(uint8_t enabled);
const UiContext *Ui_GetContext(void);
const char *Ui_GetMenuItemName(UiMenuItem item);

#endif