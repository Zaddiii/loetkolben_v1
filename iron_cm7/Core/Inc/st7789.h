#ifndef ST7789_H
#define ST7789_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t width;
  uint16_t height;
  uint16_t backlight_permille;
  uint8_t initialized;
  uint8_t last_error;
  uint8_t last_hal_status;
} St7789Context;

bool St7789_Init(void);
bool St7789_FillScreen(uint16_t rgb565);
bool St7789_DrawTextLine(uint8_t line_index, const char *text);
void St7789_SetBacklightPermille(uint16_t backlight_permille);
const St7789Context *St7789_GetContext(void);

#endif