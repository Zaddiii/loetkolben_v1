#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

enum
{
  DISPLAY_LINE_COUNT = 4,
  DISPLAY_LINE_LENGTH = 32
};

typedef enum
{
  DISPLAY_PAGE_MAIN = 0,
  DISPLAY_PAGE_CALIBRATION,
  DISPLAY_PAGE_COUNT
} DisplayPage;

typedef struct
{
  uint32_t last_refresh_ms;
  uint32_t version;
  uint8_t page;
  char lines[DISPLAY_LINE_COUNT][DISPLAY_LINE_LENGTH];
} DisplayContext;

void Display_Init(void);
void Display_Tick(uint32_t now_ms);
void Display_SetPage(DisplayPage page);
DisplayPage Display_CyclePage(void);
const char *Display_GetPageName(DisplayPage page);
const DisplayContext *Display_GetContext(void);

#endif