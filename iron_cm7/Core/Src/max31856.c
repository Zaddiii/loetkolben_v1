#include "max31856.h"

#include "main.h"
#include "peripherals.h"

enum
{
  MAX31856_REG_CR0 = 0x00U,
  MAX31856_REG_CR1 = 0x01U,
  MAX31856_REG_MASK = 0x02U,
  MAX31856_REG_CJTH = 0x0AU,
  MAX31856_REG_CJTL = 0x0BU,
  MAX31856_REG_LTCBH = 0x0CU,
  MAX31856_REG_LTCBM = 0x0DU,
  MAX31856_REG_LTCBL = 0x0EU,
  MAX31856_REG_SR = 0x0FU,
  MAX31856_CR0_AUTOCONVERT = 0x80U,
  MAX31856_CR0_50HZ_FILTER = 0x01U,
  MAX31856_CR1_TYPE_K = 0x03U,
  MAX31856_POLL_PERIOD_MS = 100U
};

static Max31856Context max31856_context;

static HAL_StatusTypeDef Max31856_WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t tx_data[2] = {(uint8_t)(reg | 0x80U), value};
  HAL_StatusTypeDef status;

  HAL_GPIO_WritePin(MAX31856_CS_GPIO_Port, MAX31856_CS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&hspi1, tx_data, 2U, 20U);
  HAL_GPIO_WritePin(MAX31856_CS_GPIO_Port, MAX31856_CS_Pin, GPIO_PIN_SET);

  return status;
}

static HAL_StatusTypeDef Max31856_ReadRegisters(uint8_t reg, uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef status;
  uint8_t address = (uint8_t)(reg & 0x7FU);

  HAL_GPIO_WritePin(MAX31856_CS_GPIO_Port, MAX31856_CS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&hspi1, &address, 1U, 20U);

  if (status == HAL_OK)
  {
    status = HAL_SPI_Receive(&hspi1, data, length, 20U);
  }

  HAL_GPIO_WritePin(MAX31856_CS_GPIO_Port, MAX31856_CS_Pin, GPIO_PIN_SET);
  return status;
}

static int16_t Max31856_DecodeColdJunctionCdeg(const uint8_t *raw_data)
{
  int16_t raw_value = (int16_t)(((uint16_t)raw_data[0] << 8) | raw_data[1]);

  raw_value >>= 2;
  return (int16_t)((raw_value * 100) / 64);
}

static int32_t Max31856_DecodeThermocoupleCdeg(const uint8_t *raw_data)
{
  int32_t raw_value = ((int32_t)raw_data[0] << 16) | ((int32_t)raw_data[1] << 8) | raw_data[2];

  raw_value >>= 5;

  if ((raw_value & (1L << 18)) != 0)
  {
    raw_value |= ~((1L << 19) - 1L);
  }

  return (raw_value * 100) / 128;
}

bool Max31856_Init(void)
{
  max31856_context.last_update_tick_ms = 0U;
  max31856_context.thermocouple_temp_cdeg = 0U;
  max31856_context.cold_junction_temp_cdeg = 250;
  max31856_context.fault_flags = 0U;
  max31856_context.data_ready = 0U;

#if IRON_SIMULATION_MODE
  max31856_context.initialized = 1U;
  max31856_context.data_ready = 1U;
  max31856_context.thermocouple_temp_cdeg = 3200U;
  return true;
#else
  if (Max31856_WriteRegister(MAX31856_REG_CR0, MAX31856_CR0_AUTOCONVERT | MAX31856_CR0_50HZ_FILTER) != HAL_OK)
  {
    max31856_context.initialized = 0U;
    return false;
  }

  if (Max31856_WriteRegister(MAX31856_REG_CR1, MAX31856_CR1_TYPE_K) != HAL_OK)
  {
    max31856_context.initialized = 0U;
    return false;
  }

  if (Max31856_WriteRegister(MAX31856_REG_MASK, 0x00U) != HAL_OK)
  {
    max31856_context.initialized = 0U;
    return false;
  }

  max31856_context.initialized = 1U;
  return true;
#endif
}

bool Max31856_Poll(uint32_t now_ms)
{
#if IRON_SIMULATION_MODE
  max31856_context.last_update_tick_ms = now_ms;
  max31856_context.data_ready = 1U;
  max31856_context.fault_flags = 0U;
  return true;
#else
  uint8_t thermocouple_regs[3];
  uint8_t cold_junction_regs[2];
  uint8_t status_reg;

  if (max31856_context.initialized == 0U)
  {
    return false;
  }

  if ((now_ms - max31856_context.last_update_tick_ms) < MAX31856_POLL_PERIOD_MS)
  {
    return max31856_context.data_ready != 0U;
  }

  if (Max31856_ReadRegisters(MAX31856_REG_LTCBH, thermocouple_regs, sizeof(thermocouple_regs)) != HAL_OK)
  {
    return false;
  }

  if (Max31856_ReadRegisters(MAX31856_REG_CJTH, cold_junction_regs, sizeof(cold_junction_regs)) != HAL_OK)
  {
    return false;
  }

  if (Max31856_ReadRegisters(MAX31856_REG_SR, &status_reg, 1U) != HAL_OK)
  {
    return false;
  }

  max31856_context.last_update_tick_ms = now_ms;
  max31856_context.thermocouple_temp_cdeg = (uint16_t)Max31856_DecodeThermocoupleCdeg(thermocouple_regs);
  max31856_context.cold_junction_temp_cdeg = Max31856_DecodeColdJunctionCdeg(cold_junction_regs);
  max31856_context.fault_flags = status_reg;
  max31856_context.data_ready = 1U;
  return true;
#endif
}

const Max31856Context *Max31856_GetContext(void)
{
  return &max31856_context;
}