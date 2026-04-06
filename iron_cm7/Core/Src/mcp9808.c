#include "mcp9808.h"

#include "main.h"
#include "peripherals.h"

enum
{
  MCP9808_I2C_ADDRESS = (0x18U << 1),
  MCP9808_REG_AMBIENT_TEMP = 0x05U,
  MCP9808_REG_MANUFACTURER_ID = 0x06U,
  MCP9808_REG_DEVICE_ID = 0x07U,
  MCP9808_EXPECTED_MANUFACTURER_ID = 0x0054U,
  MCP9808_EXPECTED_DEVICE_ID = 0x0400U,
  MCP9808_RETRY_PERIOD_MS = 1000U,
  MCP9808_POLL_PERIOD_MS = 100U,
  MCP9808_I2C_TIMEOUT_MS = 20U,
  MCP9808_DEFAULT_AMBIENT_TEMP_CDEG = 250
};

enum
{
  MCP9808_ERROR_NONE = 0U,
  MCP9808_ERROR_I2C_READ = 1U,
  MCP9808_ERROR_MANUFACTURER_ID = 2U,
  MCP9808_ERROR_DEVICE_ID = 3U,
  MCP9808_ERROR_NOT_INITIALIZED = 4U
};

static Mcp9808Context mcp9808_context;

static bool Mcp9808_ReadRegister16(uint8_t reg, uint16_t *value);
static bool Mcp9808_Probe(void)
{
  uint16_t manufacturer_id;
  uint16_t device_id;

  mcp9808_context.last_hal_status = (uint8_t)HAL_OK;

  if (!Mcp9808_ReadRegister16(MCP9808_REG_MANUFACTURER_ID, &manufacturer_id))
  {
    mcp9808_context.initialized = 0U;
    mcp9808_context.last_error = MCP9808_ERROR_I2C_READ;
    mcp9808_context.last_hal_status = (uint8_t)hi2c3.ErrorCode;
    return false;
  }

  mcp9808_context.last_manufacturer_id = manufacturer_id;

  if (manufacturer_id != MCP9808_EXPECTED_MANUFACTURER_ID)
  {
    mcp9808_context.initialized = 0U;
    mcp9808_context.last_error = MCP9808_ERROR_MANUFACTURER_ID;
    return false;
  }

  if (!Mcp9808_ReadRegister16(MCP9808_REG_DEVICE_ID, &device_id))
  {
    mcp9808_context.initialized = 0U;
    mcp9808_context.last_error = MCP9808_ERROR_I2C_READ;
    mcp9808_context.last_hal_status = (uint8_t)hi2c3.ErrorCode;
    return false;
  }

  mcp9808_context.last_device_id = device_id;

  if (device_id != MCP9808_EXPECTED_DEVICE_ID)
  {
    mcp9808_context.initialized = 0U;
    mcp9808_context.last_error = MCP9808_ERROR_DEVICE_ID;
    return false;
  }

  mcp9808_context.initialized = 1U;
  mcp9808_context.last_error = MCP9808_ERROR_NONE;
  mcp9808_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
}

static bool Mcp9808_ReadRegister16(uint8_t reg, uint16_t *value)
{
  uint8_t raw[2];

  if (HAL_I2C_Mem_Read(&hi2c3,
                       MCP9808_I2C_ADDRESS,
                       reg,
                       I2C_MEMADD_SIZE_8BIT,
                       raw,
                       sizeof(raw),
                       MCP9808_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  *value = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
  return true;
}

static int16_t Mcp9808_DecodeAmbientTempCdeg(uint16_t raw_value)
{
  int32_t temp_cdeg = ((int32_t)(raw_value & 0x0FFFU) * 100) / 16;

  if ((raw_value & 0x1000U) != 0U)
  {
    temp_cdeg -= 25600;
  }

  return (int16_t)temp_cdeg;
}

bool Mcp9808_Init(void)
{
  mcp9808_context.last_update_tick_ms = 0U;
  mcp9808_context.last_init_attempt_tick_ms = HAL_GetTick();
  mcp9808_context.ambient_temp_cdeg = MCP9808_DEFAULT_AMBIENT_TEMP_CDEG;
  mcp9808_context.last_manufacturer_id = 0U;
  mcp9808_context.last_device_id = 0U;
  mcp9808_context.initialized = 0U;
  mcp9808_context.data_ready = 0U;
  mcp9808_context.last_error = MCP9808_ERROR_NOT_INITIALIZED;
  mcp9808_context.last_hal_status = 0U;

#if IRON_VIRTUAL_MCP9808
  mcp9808_context.initialized = 1U;
  mcp9808_context.data_ready = 1U;
  mcp9808_context.last_error = MCP9808_ERROR_NONE;
  return true;
#else
  return Mcp9808_Probe();
#endif
}

bool Mcp9808_Poll(uint32_t now_ms)
{
#if IRON_VIRTUAL_MCP9808
  mcp9808_context.last_update_tick_ms = now_ms;
  mcp9808_context.data_ready = 1U;
  return true;
#else
  uint16_t ambient_raw;

  if (mcp9808_context.initialized == 0U)
  {
    if ((now_ms - mcp9808_context.last_init_attempt_tick_ms) >= MCP9808_RETRY_PERIOD_MS)
    {
      mcp9808_context.last_init_attempt_tick_ms = now_ms;
      (void)Mcp9808_Probe();
    }

    mcp9808_context.last_error = MCP9808_ERROR_NOT_INITIALIZED;
    return false;
  }

  if ((now_ms - mcp9808_context.last_update_tick_ms) < MCP9808_POLL_PERIOD_MS)
  {
    return mcp9808_context.data_ready != 0U;
  }

  if (!Mcp9808_ReadRegister16(MCP9808_REG_AMBIENT_TEMP, &ambient_raw))
  {
    mcp9808_context.data_ready = 0U;
    mcp9808_context.initialized = 0U;
    mcp9808_context.last_error = MCP9808_ERROR_I2C_READ;
    mcp9808_context.last_hal_status = (uint8_t)hi2c3.ErrorCode;
    return false;
  }

  mcp9808_context.last_update_tick_ms = now_ms;
  mcp9808_context.ambient_temp_cdeg = Mcp9808_DecodeAmbientTempCdeg(ambient_raw);
  mcp9808_context.data_ready = 1U;
  mcp9808_context.last_error = MCP9808_ERROR_NONE;
  mcp9808_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
#endif
}

const Mcp9808Context *Mcp9808_GetContext(void)
{
  return &mcp9808_context;
}