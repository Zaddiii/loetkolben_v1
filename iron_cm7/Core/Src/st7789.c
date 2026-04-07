#include "st7789.h"

#include "display.h"
#include "main.h"
#include "peripherals.h"

#include <stddef.h>
#include <string.h>

enum
{
  ST7789_WIDTH = 320U,
  ST7789_HEIGHT = 170U,
  ST7789_X_OFFSET = 0U,
  ST7789_Y_OFFSET = 35U,
  ST7789_SPI_TIMEOUT_MS = 50U,
  ST7789_CMD_SWRESET = 0x01U,
  ST7789_CMD_SLPOUT = 0x11U,
  ST7789_CMD_COLMOD = 0x3AU,
  ST7789_CMD_MADCTL = 0x36U,
  ST7789_CMD_INVON = 0x21U,
  ST7789_CMD_CASET = 0x2AU,
  ST7789_CMD_RASET = 0x2BU,
  ST7789_CMD_RAMWR = 0x2CU,
  ST7789_CMD_DISPON = 0x29U,
  ST7789_MADCTL_LANDSCAPE = 0x60U,
  ST7789_COLOR_BLACK = 0x0000U,
  ST7789_COLOR_WHITE = 0xFFFFU,
  ST7789_COLOR_AMBER = 0xFD20U,
  ST7789_COLOR_CYAN = 0x07FFU,
  ST7789_LINE_MARGIN_X = 10U,
  ST7789_LINE_MARGIN_Y = 10U,
  ST7789_LINE_PITCH = 38U,
  ST7789_CHAR_WIDTH = 4U,
  ST7789_CHAR_HEIGHT = 5U,
  ST7789_CHAR_SCALE = 2U,
  ST7789_CHAR_SPACING = 2U,
  ST7789_FILL_CHUNK_PIXELS = 64U
};

enum
{
  ST7789_ERROR_NONE = 0U,
  ST7789_ERROR_SPI = 1U,
  ST7789_ERROR_INIT = 2U,
  ST7789_ERROR_ARGUMENT = 3U
};

static St7789Context st7789_context;

static void St7789_Select(void)
{
  HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, GPIO_PIN_RESET);
}

static void St7789_Deselect(void)
{
  HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef St7789_WriteCommand(uint8_t command)
{
  HAL_StatusTypeDef status;

  HAL_GPIO_WritePin(DISPLAY_DC_GPIO_Port, DISPLAY_DC_Pin, GPIO_PIN_RESET);
  St7789_Select();
  status = HAL_SPI_Transmit(&hspi2, &command, 1U, ST7789_SPI_TIMEOUT_MS);
  St7789_Deselect();
  return status;
}

static HAL_StatusTypeDef St7789_WriteData(const uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_OK;
  }

  HAL_GPIO_WritePin(DISPLAY_DC_GPIO_Port, DISPLAY_DC_Pin, GPIO_PIN_SET);
  St7789_Select();
  status = HAL_SPI_Transmit(&hspi2, (uint8_t *)data, length, ST7789_SPI_TIMEOUT_MS);
  St7789_Deselect();
  return status;
}

static bool St7789_SetAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
  uint8_t address_data[4];
  uint16_t x_end = (uint16_t)(x + width - 1U + ST7789_X_OFFSET);
  uint16_t y_end = (uint16_t)(y + height - 1U + ST7789_Y_OFFSET);

  x = (uint16_t)(x + ST7789_X_OFFSET);
  y = (uint16_t)(y + ST7789_Y_OFFSET);

  address_data[0] = (uint8_t)(x >> 8);
  address_data[1] = (uint8_t)(x & 0xFFU);
  address_data[2] = (uint8_t)(x_end >> 8);
  address_data[3] = (uint8_t)(x_end & 0xFFU);

  if ((St7789_WriteCommand(ST7789_CMD_CASET) != HAL_OK) || (St7789_WriteData(address_data, sizeof(address_data)) != HAL_OK))
  {
    return false;
  }

  address_data[0] = (uint8_t)(y >> 8);
  address_data[1] = (uint8_t)(y & 0xFFU);
  address_data[2] = (uint8_t)(y_end >> 8);
  address_data[3] = (uint8_t)(y_end & 0xFFU);

  if ((St7789_WriteCommand(ST7789_CMD_RASET) != HAL_OK) || (St7789_WriteData(address_data, sizeof(address_data)) != HAL_OK))
  {
    return false;
  }

  return St7789_WriteCommand(ST7789_CMD_RAMWR) == HAL_OK;
}

static bool St7789_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t rgb565)
{
  uint8_t pixel_chunk[ST7789_FILL_CHUNK_PIXELS * 2U];
  uint32_t pixel_count;
  uint32_t remaining;
  uint16_t pixel_index;

  if ((width == 0U) || (height == 0U))
  {
    return true;
  }

  if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT))
  {
    return false;
  }

  if ((x + width) > ST7789_WIDTH)
  {
    width = (uint16_t)(ST7789_WIDTH - x);
  }

  if ((y + height) > ST7789_HEIGHT)
  {
    height = (uint16_t)(ST7789_HEIGHT - y);
  }

  for (pixel_index = 0U; pixel_index < ST7789_FILL_CHUNK_PIXELS; ++pixel_index)
  {
    pixel_chunk[pixel_index * 2U] = (uint8_t)(rgb565 >> 8);
    pixel_chunk[pixel_index * 2U + 1U] = (uint8_t)(rgb565 & 0xFFU);
  }

  pixel_count = (uint32_t)width * (uint32_t)height;
  remaining = pixel_count;

  if (!St7789_SetAddressWindow(x, y, width, height))
  {
    return false;
  }

  while (remaining > 0U)
  {
    uint16_t chunk_pixels = (remaining > ST7789_FILL_CHUNK_PIXELS) ? ST7789_FILL_CHUNK_PIXELS : (uint16_t)remaining;

    if (St7789_WriteData(pixel_chunk, (uint16_t)(chunk_pixels * 2U)) != HAL_OK)
    {
      return false;
    }

    remaining -= chunk_pixels;
  }

  return true;
}

static void St7789_GetGlyphRows(char character, uint8_t rows[ST7789_CHAR_HEIGHT])
{
  uint8_t index;

  for (index = 0U; index < ST7789_CHAR_HEIGHT; ++index)
  {
    rows[index] = 0x00U;
  }

  switch (character)
  {
    case '0': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case '1': rows[0] = 0x2U; rows[1] = 0x6U; rows[2] = 0x2U; rows[3] = 0x2U; rows[4] = 0x7U; break;
    case '2': rows[0] = 0xEU; rows[1] = 0x1U; rows[2] = 0x6U; rows[3] = 0x8U; rows[4] = 0xFU; break;
    case '3': rows[0] = 0xEU; rows[1] = 0x1U; rows[2] = 0x6U; rows[3] = 0x1U; rows[4] = 0xEU; break;
    case '4': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0xFU; rows[3] = 0x1U; rows[4] = 0x1U; break;
    case '5': rows[0] = 0xFU; rows[1] = 0x8U; rows[2] = 0xEU; rows[3] = 0x1U; rows[4] = 0xEU; break;
    case '6': rows[0] = 0x6U; rows[1] = 0x8U; rows[2] = 0xEU; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case '7': rows[0] = 0xFU; rows[1] = 0x1U; rows[2] = 0x2U; rows[3] = 0x4U; rows[4] = 0x4U; break;
    case '8': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x6U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case '9': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x7U; rows[3] = 0x1U; rows[4] = 0x6U; break;
    case 'A': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0xFU; rows[3] = 0x9U; rows[4] = 0x9U; break;
    case 'B': rows[0] = 0xEU; rows[1] = 0x9U; rows[2] = 0xEU; rows[3] = 0x9U; rows[4] = 0xEU; break;
    case 'C': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x8U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case 'D': rows[0] = 0xEU; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0x9U; rows[4] = 0xEU; break;
    case 'E': rows[0] = 0xFU; rows[1] = 0x8U; rows[2] = 0xEU; rows[3] = 0x8U; rows[4] = 0xFU; break;
    case 'F': rows[0] = 0xFU; rows[1] = 0x8U; rows[2] = 0xEU; rows[3] = 0x8U; rows[4] = 0x8U; break;
    case 'G': rows[0] = 0x7U; rows[1] = 0x8U; rows[2] = 0xBU; rows[3] = 0x9U; rows[4] = 0x7U; break;
    case 'H': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0xFU; rows[3] = 0x9U; rows[4] = 0x9U; break;
    case 'I': rows[0] = 0x7U; rows[1] = 0x2U; rows[2] = 0x2U; rows[3] = 0x2U; rows[4] = 0x7U; break;
    case 'J': rows[0] = 0x1U; rows[1] = 0x1U; rows[2] = 0x1U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case 'K': rows[0] = 0x9U; rows[1] = 0xAU; rows[2] = 0xCU; rows[3] = 0xAU; rows[4] = 0x9U; break;
    case 'L': rows[0] = 0x8U; rows[1] = 0x8U; rows[2] = 0x8U; rows[3] = 0x8U; rows[4] = 0xFU; break;
    case 'M': rows[0] = 0x9U; rows[1] = 0xFU; rows[2] = 0xFU; rows[3] = 0x9U; rows[4] = 0x9U; break;
    case 'N': rows[0] = 0x9U; rows[1] = 0xDU; rows[2] = 0xFU; rows[3] = 0xBU; rows[4] = 0x9U; break;
    case 'O': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case 'P': rows[0] = 0xEU; rows[1] = 0x9U; rows[2] = 0xEU; rows[3] = 0x8U; rows[4] = 0x8U; break;
    case 'Q': rows[0] = 0x6U; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0xBU; rows[4] = 0x7U; break;
    case 'R': rows[0] = 0xEU; rows[1] = 0x9U; rows[2] = 0xEU; rows[3] = 0xAU; rows[4] = 0x9U; break;
    case 'S': rows[0] = 0x7U; rows[1] = 0x8U; rows[2] = 0x6U; rows[3] = 0x1U; rows[4] = 0xEU; break;
    case 'T': rows[0] = 0xFU; rows[1] = 0x2U; rows[2] = 0x2U; rows[3] = 0x2U; rows[4] = 0x2U; break;
    case 'U': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0x9U; rows[4] = 0x6U; break;
    case 'V': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0x9U; rows[3] = 0x6U; rows[4] = 0x6U; break;
    case 'W': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0xFU; rows[3] = 0xFU; rows[4] = 0x9U; break;
    case 'X': rows[0] = 0x9U; rows[1] = 0x6U; rows[2] = 0x6U; rows[3] = 0x6U; rows[4] = 0x9U; break;
    case 'Y': rows[0] = 0x9U; rows[1] = 0x9U; rows[2] = 0x6U; rows[3] = 0x2U; rows[4] = 0x2U; break;
    case 'Z': rows[0] = 0xFU; rows[1] = 0x1U; rows[2] = 0x6U; rows[3] = 0x8U; rows[4] = 0xFU; break;
    case '.': rows[0] = 0x0U; rows[1] = 0x0U; rows[2] = 0x0U; rows[3] = 0x0U; rows[4] = 0x2U; break;
    case '%': rows[0] = 0x9U; rows[1] = 0x1U; rows[2] = 0x2U; rows[3] = 0x4U; rows[4] = 0x9U; break;
    case '-': rows[0] = 0x0U; rows[1] = 0x0U; rows[2] = 0xFU; rows[3] = 0x0U; rows[4] = 0x0U; break;
    case ':': rows[0] = 0x0U; rows[1] = 0x2U; rows[2] = 0x0U; rows[3] = 0x2U; rows[4] = 0x0U; break;
    case '/': rows[0] = 0x1U; rows[1] = 0x1U; rows[2] = 0x2U; rows[3] = 0x4U; rows[4] = 0x4U; break;
    default: break;
  }
}

static bool St7789_DrawCharacter(uint16_t x, uint16_t y, char character, uint16_t fg_color, uint16_t bg_color, uint8_t scale)
{
  uint8_t rows[ST7789_CHAR_HEIGHT];
  uint8_t row;
  uint8_t column;

  if (scale == 0U)
  {
    return false;
  }

  if (character >= 'a' && character <= 'z')
  {
    character = (char)(character - ('a' - 'A'));
  }

  St7789_GetGlyphRows(character, rows);

  if (!St7789_FillRect(x,
                       y,
                       ST7789_CHAR_WIDTH * scale,
                       ST7789_CHAR_HEIGHT * scale,
                       bg_color))
  {
    return false;
  }

  for (row = 0U; row < ST7789_CHAR_HEIGHT; ++row)
  {
    for (column = 0U; column < ST7789_CHAR_WIDTH; ++column)
    {
      if ((rows[row] & (1U << (ST7789_CHAR_WIDTH - 1U - column))) != 0U)
      {
        if (!St7789_FillRect((uint16_t)(x + column * scale),
                             (uint16_t)(y + row * scale),
                             scale,
                             scale,
                             fg_color))
        {
          return false;
        }
      }
    }
  }

  return true;
}

bool St7789_Init(void)
{
  HAL_StatusTypeDef pwm_status;
  uint8_t pwm_start_failed;
  uint8_t data;

  memset(&st7789_context, 0, sizeof(st7789_context));
  st7789_context.width = ST7789_WIDTH;
  st7789_context.height = ST7789_HEIGHT;
  pwm_start_failed = 0U;

  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(120U);

  pwm_status = HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  if ((pwm_status != HAL_OK) && (pwm_status != HAL_BUSY))
  {
    // Continue display init even if PWM start fails so SPI diagnostics remain possible.
    pwm_start_failed = 1U;
    st7789_context.last_hal_status = (uint8_t)pwm_status;
  }

  if (pwm_start_failed == 0U)
  {
    St7789_SetBacklightPermille(800U);
  }

  if (St7789_WriteCommand(ST7789_CMD_SWRESET) != HAL_OK)
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  HAL_Delay(150U);

  if (St7789_WriteCommand(ST7789_CMD_SLPOUT) != HAL_OK)
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  HAL_Delay(120U);

  data = 0x55U;
  if ((St7789_WriteCommand(ST7789_CMD_COLMOD) != HAL_OK) || (St7789_WriteData(&data, 1U) != HAL_OK))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  data = ST7789_MADCTL_LANDSCAPE;
  if ((St7789_WriteCommand(ST7789_CMD_MADCTL) != HAL_OK) || (St7789_WriteData(&data, 1U) != HAL_OK))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  if ((St7789_WriteCommand(ST7789_CMD_INVON) != HAL_OK) || (St7789_WriteCommand(ST7789_CMD_DISPON) != HAL_OK))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  HAL_Delay(20U);
  st7789_context.initialized = 1U;
  st7789_context.last_error = ST7789_ERROR_NONE;
  if (pwm_start_failed == 0U)
  {
    st7789_context.last_hal_status = (uint8_t)HAL_OK;
  }
  return St7789_FillScreen(ST7789_COLOR_BLACK);
}

bool St7789_FillScreen(uint16_t rgb565)
{
  if (st7789_context.initialized == 0U)
  {
    st7789_context.last_error = ST7789_ERROR_INIT;
    return false;
  }

  if (!St7789_FillRect(0U, 0U, ST7789_WIDTH, ST7789_HEIGHT, rgb565))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  return true;
}

bool St7789_ClearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t rgb565)
{
  if (st7789_context.initialized == 0U)
  {
    st7789_context.last_error = ST7789_ERROR_INIT;
    return false;
  }

  if (!St7789_FillRect(x, y, width, height, rgb565))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  st7789_context.last_error = ST7789_ERROR_NONE;
  st7789_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
}

bool St7789_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color, uint8_t scale)
{
  size_t text_index;
  size_t text_length;

  if ((text == NULL) || (scale == 0U))
  {
    st7789_context.last_error = ST7789_ERROR_ARGUMENT;
    return false;
  }

  if (st7789_context.initialized == 0U)
  {
    return false;
  }

  text_length = strlen(text);
  for (text_index = 0U; text_index < text_length; ++text_index)
  {
    if (!St7789_DrawCharacter(x, y, text[text_index], fg_color, bg_color, scale))
    {
      st7789_context.last_error = ST7789_ERROR_SPI;
      st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
      return false;
    }

    x = (uint16_t)(x + ST7789_CHAR_WIDTH * scale + ST7789_CHAR_SPACING);
    if ((x + ST7789_CHAR_WIDTH * scale) >= ST7789_WIDTH)
    {
      break;
    }
  }

  st7789_context.last_error = ST7789_ERROR_NONE;
  st7789_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
}

bool St7789_DrawTextLine(uint8_t line_index, const char *text)
{
  uint16_t y;
  uint16_t x;
  uint16_t background_color;
  uint16_t foreground_color;
  size_t text_index;
  size_t text_length;

  if ((line_index >= DISPLAY_LINE_COUNT) || (text == NULL))
  {
    st7789_context.last_error = ST7789_ERROR_ARGUMENT;
    return false;
  }

  if (st7789_context.initialized == 0U)
  {
    return false;
  }

  y = (uint16_t)(ST7789_LINE_MARGIN_Y + (uint16_t)line_index * ST7789_LINE_PITCH);
  background_color = ST7789_COLOR_BLACK;
  foreground_color = (line_index == 0U) ? ST7789_COLOR_AMBER : ST7789_COLOR_CYAN;

  if (!St7789_FillRect(0U, y - 6U, ST7789_WIDTH, (uint16_t)(ST7789_LINE_PITCH - 4U), background_color))
  {
    st7789_context.last_error = ST7789_ERROR_SPI;
    st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
    return false;
  }

  x = ST7789_LINE_MARGIN_X;
  text_length = strlen(text);
  if (text_length > (DISPLAY_LINE_LENGTH - 1U))
  {
    text_length = DISPLAY_LINE_LENGTH - 1U;
  }

  for (text_index = 0U; text_index < text_length; ++text_index)
  {
    if (!St7789_DrawCharacter(x, y, text[text_index], foreground_color, background_color, ST7789_CHAR_SCALE))
    {
      st7789_context.last_error = ST7789_ERROR_SPI;
      st7789_context.last_hal_status = (uint8_t)hspi2.ErrorCode;
      return false;
    }

    x = (uint16_t)(x + ST7789_CHAR_WIDTH * ST7789_CHAR_SCALE + ST7789_CHAR_SPACING);
    if ((x + ST7789_CHAR_WIDTH * ST7789_CHAR_SCALE) >= ST7789_WIDTH)
    {
      break;
    }
  }

  st7789_context.last_error = ST7789_ERROR_NONE;
  st7789_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
}

void St7789_SetBacklightPermille(uint16_t backlight_permille)
{
  uint32_t compare_value;
  uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim3);

  if (backlight_permille > 1000U)
  {
    backlight_permille = 1000U;
  }

  compare_value = ((period + 1U) * backlight_permille) / 1000U;
  if (compare_value > period)
  {
    compare_value = period;
  }

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, compare_value);
  st7789_context.backlight_permille = backlight_permille;
}

const St7789Context *St7789_GetContext(void)
{
  return &st7789_context;
}