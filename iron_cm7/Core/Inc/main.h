/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void SystemClock_Config(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

#define INA300_LATCH_Pin GPIO_PIN_6
#define INA300_LATCH_GPIO_Port GPIOF
#define OPAMP_PGOOD_Pin GPIO_PIN_7
#define OPAMP_PGOOD_GPIO_Port GPIOF
#define TIP_CHECK_Pin GPIO_PIN_9
#define TIP_CHECK_GPIO_Port GPIOF
#define TIP_CLAMP_Pin GPIO_PIN_10
#define TIP_CLAMP_GPIO_Port GPIOF
#define THERMO_INT_ADC_Pin GPIO_PIN_11
#define THERMO_INT_ADC_GPIO_Port GPIOF

#define HEATER_EN_Pin GPIO_PIN_2
#define HEATER_EN_GPIO_Port GPIOE
#define OVERCURRENT_Pin GPIO_PIN_3
#define OVERCURRENT_GPIO_Port GPIOE
#define HUSB238_INT_Pin GPIO_PIN_15
#define HUSB238_INT_GPIO_Port GPIOE

#define ENCODER_SW_Pin GPIO_PIN_2
#define ENCODER_SW_GPIO_Port GPIOA
#define ENCODER_SW_ACTIVE_LEVEL GPIO_PIN_RESET
#define ENCODER_A_Pin GPIO_PIN_0
#define ENCODER_A_GPIO_Port GPIOA
#define ENCODER_B_Pin GPIO_PIN_1
#define ENCODER_B_GPIO_Port GPIOA
#define BUCKBOOST_REF_Pin GPIO_PIN_4
#define BUCKBOOST_REF_GPIO_Port GPIOA
#define BUCKBOOST_FAULT_Pin GPIO_PIN_5
#define BUCKBOOST_FAULT_GPIO_Port GPIOA
#define ADC_HEATER_CURRENT_Pin GPIO_PIN_6
#define ADC_HEATER_CURRENT_GPIO_Port GPIOA
#define DISPLAY_BACKLIGHT_PWM_Pin GPIO_PIN_7
#define DISPLAY_BACKLIGHT_PWM_GPIO_Port GPIOA
#define MCP9808_SCL_Pin GPIO_PIN_8
#define MCP9808_SCL_GPIO_Port GPIOA

#define INA238_ALERT_Pin GPIO_PIN_5
#define INA238_ALERT_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_0
#define LED1_GPIO_Port GPIOB
#define INA238_SCL_Pin GPIO_PIN_6
#define INA238_SCL_GPIO_Port GPIOB
#define INA238_SDA_Pin GPIO_PIN_7
#define INA238_SDA_GPIO_Port GPIOB
#define DISPLAY_SPI_SCK_Pin GPIO_PIN_13
#define DISPLAY_SPI_SCK_GPIO_Port GPIOB
#define DISPLAY_DC_Pin GPIO_PIN_14
#define DISPLAY_DC_GPIO_Port GPIOB
#define DISPLAY_SPI_MOSI_Pin GPIO_PIN_15
#define DISPLAY_SPI_MOSI_GPIO_Port GPIOB

#define ADC_BUCKBOOST_VOLTAGE_Pin GPIO_PIN_4
#define ADC_BUCKBOOST_VOLTAGE_GPIO_Port GPIOC
#define MCP9808_SDA_Pin GPIO_PIN_9
#define MCP9808_SDA_GPIO_Port GPIOC

#define MAX31856_MOSI_Pin GPIO_PIN_7
#define MAX31856_MOSI_GPIO_Port GPIOD
#define DEBUG_TX_Pin GPIO_PIN_8
#define DEBUG_TX_GPIO_Port GPIOD
#define DEBUG_RX_Pin GPIO_PIN_9
#define DEBUG_RX_GPIO_Port GPIOD
#define DISPLAY_RESET_Pin GPIO_PIN_8
#define DISPLAY_RESET_GPIO_Port GPIOD
#define DISPLAY_CS_Pin GPIO_PIN_9
#define DISPLAY_CS_GPIO_Port GPIOD
#define DOCK_CHECK_Pin GPIO_PIN_10
#define DOCK_CHECK_GPIO_Port GPIOD

#define USER_BUTTON_Pin GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC

#define FAN_PWM_Pin GPIO_PIN_5
#define FAN_PWM_GPIO_Port GPIOE
#define HEATER_PWM_Pin GPIO_PIN_9
#define HEATER_PWM_GPIO_Port GPIOE
#define MAX31856_CS_Pin GPIO_PIN_10
#define MAX31856_CS_GPIO_Port GPIOG
#define MAX31856_MISO_Pin GPIO_PIN_9
#define MAX31856_MISO_GPIO_Port GPIOG
#define MAX31856_SCK_Pin GPIO_PIN_11
#define MAX31856_SCK_GPIO_Port GPIOG
#define MAX31856_DRDY_Pin GPIO_PIN_12
#define MAX31856_DRDY_GPIO_Port GPIOG
#define MAX31856_FAULT_Pin GPIO_PIN_13
#define MAX31856_FAULT_GPIO_Port GPIOG

#define HEATER_EN_INACTIVE_LEVEL GPIO_PIN_RESET
#define HEATER_EN_ACTIVE_LEVEL GPIO_PIN_SET

#define IRON_BRINGUP_PROFILE_SENSORLESS 1U
#define IRON_BRINGUP_PROFILE_REAL_SENSORS 2U

#define IRON_BRINGUP_PROFILE IRON_BRINGUP_PROFILE_SENSORLESS

#if (IRON_BRINGUP_PROFILE == IRON_BRINGUP_PROFILE_SENSORLESS)
#define IRON_VIRTUAL_FAULT_INPUTS 1U
#define IRON_VIRTUAL_CALIBRATION 1U
#define IRON_VIRTUAL_INTERNAL_ADC 1U
#define IRON_VIRTUAL_AUX_ADC 1U
#define IRON_VIRTUAL_INA238 1U
#define IRON_VIRTUAL_MAX31856 1U
#define IRON_VIRTUAL_MCP9808 1U
#define IRON_VIRTUAL_DOCK_INPUT 1U
#elif (IRON_BRINGUP_PROFILE == IRON_BRINGUP_PROFILE_REAL_SENSORS)
#define IRON_VIRTUAL_FAULT_INPUTS 1U
#define IRON_VIRTUAL_CALIBRATION 0U
#define IRON_VIRTUAL_INTERNAL_ADC 0U
#define IRON_VIRTUAL_AUX_ADC 0U
#define IRON_VIRTUAL_INA238 0U
#define IRON_VIRTUAL_MAX31856 0U
#define IRON_VIRTUAL_MCP9808 0U
#define IRON_VIRTUAL_DOCK_INPUT 1U
#else
#error Unsupported IRON_BRINGUP_PROFILE value
#endif

#define IRON_SIMULATION_DEFAULT_PWM_PERMILLE 250U
#define OVERCURRENT_FAULT_ACTIVE_LEVEL GPIO_PIN_SET
#define BUCKBOOST_FAULT_ACTIVE_LEVEL GPIO_PIN_SET
#define OPAMP_PGOOD_GOOD_LEVEL GPIO_PIN_SET
#define DOCK_CHECK_DOCKED_LEVEL GPIO_PIN_SET
#define TIP_CHECK_SAFE_LEVEL GPIO_PIN_RESET
#define TIP_CLAMP_SAFE_LEVEL GPIO_PIN_SET
#define TIP_CLAMP_MEASURE_LEVEL GPIO_PIN_RESET
#define INA300_LATCH_SAFE_LEVEL GPIO_PIN_RESET
#define DISPLAY_DC_SAFE_LEVEL GPIO_PIN_RESET
#define DISPLAY_RESET_SAFE_LEVEL GPIO_PIN_SET
#define DISPLAY_CS_SAFE_LEVEL GPIO_PIN_SET
#define MAX31856_CS_SAFE_LEVEL GPIO_PIN_SET
#define LED1_ON_LEVEL GPIO_PIN_SET
#define LED1_OFF_LEVEL GPIO_PIN_RESET

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
