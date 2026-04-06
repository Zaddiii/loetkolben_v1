#include "ina238.h"

#include "heater.h"
#include "main.h"
#include "peripherals.h"

enum
{
  INA238_I2C_ADDRESS = (0x40U << 1),
  INA238_REG_CONFIG = 0x00U,
  INA238_REG_ADC_CONFIG = 0x01U,
  INA238_REG_SHUNT_CAL = 0x02U,
  INA238_REG_VSHUNT = 0x04U,
  INA238_REG_VBUS = 0x05U,
  INA238_REG_CURRENT = 0x07U,
  INA238_REG_POWER = 0x08U,
  INA238_REG_DIAG_ALERT = 0x0BU,
  INA238_REG_MANUFACTURER_ID = 0x3EU,
  INA238_REG_DEVICE_ID = 0x3FU,
  INA238_RETRY_PERIOD_MS = 1000U,
  INA238_POLL_PERIOD_MS = 100U,
  INA238_I2C_TIMEOUT_MS = 20U,
  INA238_CONFIG_DEFAULT = 0x0000U,
  INA238_ADC_CONFIG_DEFAULT = 0xFB6AU,
  INA238_SHUNT_CAL_DEFAULT = 0x0800U,
  INA238_BUS_VOLTAGE_LSB_UV = 3125U,
  INA238_CURRENT_LSB_UA = 1000U,
  INA238_POWER_LSB_UW = 20000U
};

enum
{
  INA238_ERROR_NONE = 0U,
  INA238_ERROR_I2C_READ = 1U,
  INA238_ERROR_I2C_WRITE = 2U,
  INA238_ERROR_NOT_INITIALIZED = 3U
};

static Ina238Context ina238_context;

static bool Ina238_ReadRegister16(uint8_t reg, uint16_t *value);
static bool Ina238_WriteRegister16(uint8_t reg, uint16_t value);

static bool Ina238_Configure(void)
{
  uint16_t ignored_value;

  if (!Ina238_WriteRegister16(INA238_REG_CONFIG, INA238_CONFIG_DEFAULT))
  {
    ina238_context.initialized = 0U;
    ina238_context.last_error = INA238_ERROR_I2C_WRITE;
    ina238_context.last_hal_status = (uint8_t)hi2c1.ErrorCode;
    return false;
  }

  if (!Ina238_WriteRegister16(INA238_REG_ADC_CONFIG, INA238_ADC_CONFIG_DEFAULT))
  {
    ina238_context.initialized = 0U;
    ina238_context.last_error = INA238_ERROR_I2C_WRITE;
    ina238_context.last_hal_status = (uint8_t)hi2c1.ErrorCode;
    return false;
  }

  if (!Ina238_WriteRegister16(INA238_REG_SHUNT_CAL, INA238_SHUNT_CAL_DEFAULT))
  {
    ina238_context.initialized = 0U;
    ina238_context.last_error = INA238_ERROR_I2C_WRITE;
    ina238_context.last_hal_status = (uint8_t)hi2c1.ErrorCode;
    return false;
  }

  (void)Ina238_ReadRegister16(INA238_REG_MANUFACTURER_ID, &ina238_context.manufacturer_id);
  (void)Ina238_ReadRegister16(INA238_REG_DEVICE_ID, &ina238_context.device_id);
  (void)Ina238_ReadRegister16(INA238_REG_CONFIG, &ignored_value);

  ina238_context.initialized = 1U;
  ina238_context.data_ready = 0U;
  ina238_context.last_error = INA238_ERROR_NONE;
  ina238_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
}

static bool Ina238_ReadRegister16(uint8_t reg, uint16_t *value)
{
  uint8_t raw[2];

  if (HAL_I2C_Mem_Read(&hi2c1,
                       INA238_I2C_ADDRESS,
                       reg,
                       I2C_MEMADD_SIZE_8BIT,
                       raw,
                       sizeof(raw),
                       INA238_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  *value = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
  return true;
}

static bool Ina238_WriteRegister16(uint8_t reg, uint16_t value)
{
  uint8_t raw[2];

  raw[0] = (uint8_t)(value >> 8);
  raw[1] = (uint8_t)(value & 0xFFU);

  return HAL_I2C_Mem_Write(&hi2c1,
                           INA238_I2C_ADDRESS,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           raw,
                           sizeof(raw),
                           INA238_I2C_TIMEOUT_MS) == HAL_OK;
}

bool Ina238_Init(void)
{
  ina238_context.last_update_tick_ms = 0U;
  ina238_context.last_init_attempt_tick_ms = HAL_GetTick();
  ina238_context.shunt_voltage_raw = 0U;
  ina238_context.bus_voltage_raw = 0U;
  ina238_context.current_raw = 0U;
  ina238_context.power_raw = 0U;
  ina238_context.bus_voltage_mv = 0U;
  ina238_context.current_ma = 0U;
  ina238_context.power_mw = 0U;
  ina238_context.manufacturer_id = 0U;
  ina238_context.device_id = 0U;
  ina238_context.initialized = 0U;
  ina238_context.data_ready = 0U;
  ina238_context.alert_active = 0U;
  ina238_context.last_error = INA238_ERROR_NOT_INITIALIZED;
  ina238_context.last_hal_status = 0U;

#if IRON_VIRTUAL_INA238
  ina238_context.initialized = 1U;
  ina238_context.data_ready = 1U;
  ina238_context.last_error = INA238_ERROR_NONE;
  return true;
#else
  return Ina238_Configure();
#endif
}

bool Ina238_Poll(uint32_t now_ms)
{
#if IRON_VIRTUAL_INA238
  const HeaterControlContext *heater = Heater_Control_GetContext();

  ina238_context.last_update_tick_ms = now_ms;
  ina238_context.data_ready = 1U;
  ina238_context.alert_active = 0U;
  ina238_context.bus_voltage_mv = heater->buckboost_voltage_mv;
  ina238_context.current_ma = heater->heater_current_ma;
  ina238_context.power_mw = (uint16_t)(((uint32_t)ina238_context.bus_voltage_mv * (uint32_t)ina238_context.current_ma) / 1000U);
  ina238_context.bus_voltage_raw = (uint16_t)(((uint32_t)ina238_context.bus_voltage_mv * 1000U) / INA238_BUS_VOLTAGE_LSB_UV);
  ina238_context.current_raw = (uint16_t)(((uint32_t)ina238_context.current_ma * 1000U) / INA238_CURRENT_LSB_UA);
  ina238_context.power_raw = (uint16_t)(((uint32_t)ina238_context.power_mw * 1000U) / INA238_POWER_LSB_UW);
  ina238_context.last_error = INA238_ERROR_NONE;
  ina238_context.last_hal_status = (uint8_t)HAL_OK;
  return true;
#else
  uint16_t diag_alert_reg;

  if (ina238_context.initialized == 0U)
  {
    if ((now_ms - ina238_context.last_init_attempt_tick_ms) >= INA238_RETRY_PERIOD_MS)
    {
      ina238_context.last_init_attempt_tick_ms = now_ms;
      (void)Ina238_Configure();
    }

    ina238_context.last_error = INA238_ERROR_NOT_INITIALIZED;
    return false;
  }

  if ((now_ms - ina238_context.last_update_tick_ms) < INA238_POLL_PERIOD_MS)
  {
    return ina238_context.data_ready != 0U;
  }

  if (!Ina238_ReadRegister16(INA238_REG_VSHUNT, &ina238_context.shunt_voltage_raw) ||
      !Ina238_ReadRegister16(INA238_REG_VBUS, &ina238_context.bus_voltage_raw) ||
      !Ina238_ReadRegister16(INA238_REG_CURRENT, &ina238_context.current_raw) ||
      !Ina238_ReadRegister16(INA238_REG_POWER, &ina238_context.power_raw) ||
      !Ina238_ReadRegister16(INA238_REG_DIAG_ALERT, &diag_alert_reg))
  {
    ina238_context.initialized = 0U;
    ina238_context.data_ready = 0U;
    ina238_context.last_error = INA238_ERROR_I2C_READ;
    ina238_context.last_hal_status = (uint8_t)hi2c1.ErrorCode;
    return false;
  }

  ina238_context.last_update_tick_ms = now_ms;
  ina238_context.data_ready = 1U;
  ina238_context.alert_active = (HAL_GPIO_ReadPin(INA238_ALERT_GPIO_Port, INA238_ALERT_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  ina238_context.bus_voltage_mv = (uint16_t)(((uint32_t)ina238_context.bus_voltage_raw * INA238_BUS_VOLTAGE_LSB_UV) / 1000U);
  ina238_context.current_ma = (uint16_t)(((uint32_t)ina238_context.current_raw * INA238_CURRENT_LSB_UA) / 1000U);
  ina238_context.power_mw = (uint16_t)(((uint32_t)ina238_context.power_raw * INA238_POWER_LSB_UW) / 1000U);
  ina238_context.last_error = INA238_ERROR_NONE;
  ina238_context.last_hal_status = (uint8_t)HAL_OK;
  (void)diag_alert_reg;
  return true;
#endif
}

const Ina238Context *Ina238_GetContext(void)
{
  return &ina238_context;
}