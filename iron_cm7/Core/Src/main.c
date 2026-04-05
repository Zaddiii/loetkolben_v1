/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
#include "heater.h"
#include "peripherals.h"
#include <stdarg.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
//#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* USER CODE BEGIN PV */

volatile uint32_t g_debug_heartbeat = 0U;
volatile uint32_t g_debug_loop_counter = 0U;
volatile uint32_t g_debug_station_state = 0U;
volatile uint32_t g_debug_fault_flags = 0U;
volatile uint32_t g_debug_heater_measurement_ready = 0U;
volatile uint32_t g_debug_heater_filtered_raw = 0U;
volatile uint32_t g_debug_heater_sample_count = 0U;
volatile uint32_t g_debug_heater_enable_level = 0U;
volatile uint32_t g_debug_tip_clamp_level = 0U;
volatile uint32_t g_debug_heater_pwm_compare = 0U;

static uint32_t debug_last_heartbeat_ms = 0U;
static uint32_t debug_last_status_report_ms = 0U;
static uint32_t debug_last_logged_state = 0xFFFFFFFFU;
static uint32_t debug_last_logged_fault_flags = 0xFFFFFFFFU;
static uint32_t debug_last_logged_measurement_ready = 0xFFFFFFFFU;
static GPIO_PinState debug_last_button_level = GPIO_PIN_RESET;
static uint8_t debug_fault_button_latched = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void MX_GPIO_Init(void);
static void MX_StationSafeState_Init(void);
static void MX_BringUpBoard_Init(void);
static void Debug_Log(const char *format, ...);
static void Debug_UpdateHeartbeat(uint32_t now_ms);
static void Debug_HandleFaultButton(void);
static void Debug_ReportChanges(void);
static void Debug_ReportPeriodicStatus(uint32_t now_ms);
static const char *Debug_GetStationStateName(StationState state);
static void Debug_FormatFaultFlags(uint32_t fault_flags, char *buffer, size_t buffer_size);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
    Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
  HSEM notification */
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /*Take HSEM */
  HAL_HSEM_FastTake(HSEM_ID_0);
  /*Release HSEM in order to notify the CPU2(CM4)*/
  HAL_HSEM_Release(HSEM_ID_0,0);
  /* wait until CPU2 wakes up from stop mode */
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
    Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
  /* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */

  MX_GPIO_Init();
  MX_StationSafeState_Init();
  MX_BringUpBoard_Init();
  MX_StationPeripherals_Init();
  Station_App_Init();
  Debug_Log("iron_cm7 booted\r\n");
#if IRON_SIMULATION_MODE
  Debug_Log("SIMULATION MODE active: fault inputs are virtual, calibration is auto-valid, ADC values are synthetic\r\n");
#endif
  Debug_Log("LED1 heartbeat active, USART3 debug active, USER button injects a fault\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_ms = HAL_GetTick();

    Station_App_Tick(now_ms);

    {
      const StationContext *station = Station_App_GetContext();
      const HeaterControlContext *heater = Heater_Control_GetContext();

      ++g_debug_loop_counter;
      g_debug_heartbeat = now_ms;
      g_debug_station_state = (uint32_t)station->state;
      g_debug_fault_flags = station->fault_flags;
      g_debug_heater_measurement_ready = heater->measurement_ready;
      g_debug_heater_filtered_raw = heater->filtered_raw;
      g_debug_heater_sample_count = heater->sample_count;
      g_debug_heater_enable_level = (uint32_t)HAL_GPIO_ReadPin(HEATER_EN_GPIO_Port, HEATER_EN_Pin);
      g_debug_tip_clamp_level = (uint32_t)HAL_GPIO_ReadPin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin);
      g_debug_heater_pwm_compare = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1);
    }

    Debug_UpdateHeartbeat(now_ms);
    Debug_HandleFaultButton();
    Debug_ReportChanges();
    Debug_ReportPeriodicStatus(now_ms);

    HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, HEATER_EN_Pin, HEATER_EN_INACTIVE_LEVEL);
  HAL_GPIO_WritePin(GPIOF, INA300_LATCH_Pin, INA300_LATCH_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOF, TIP_CHECK_Pin, TIP_CHECK_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOF, TIP_CLAMP_Pin, TIP_CLAMP_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOB, DISPLAY_DC_Pin, DISPLAY_DC_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOB, LED1_Pin, LED1_OFF_LEVEL);
  HAL_GPIO_WritePin(GPIOD, DISPLAY_RESET_Pin, DISPLAY_RESET_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOD, DISPLAY_CS_Pin, DISPLAY_CS_SAFE_LEVEL);
  HAL_GPIO_WritePin(GPIOG, MAX31856_CS_Pin, MAX31856_CS_SAFE_LEVEL);

  GPIO_InitStruct.Pin = HEATER_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = INA300_LATCH_Pin | TIP_CHECK_Pin | TIP_CLAMP_Pin;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DISPLAY_DC_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED1_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DISPLAY_RESET_Pin | DISPLAY_CS_Pin;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = MAX31856_CS_Pin;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = ENCODER_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BUCKBOOST_FAULT_Pin;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = INA238_ALERT_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = USER_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DOCK_CHECK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = OVERCURRENT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = HUSB238_INT_Pin;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = OPAMP_PGOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = MAX31856_DRDY_Pin | MAX31856_FAULT_Pin;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

static void MX_StationSafeState_Init(void)
{
  Station_SafeShutdown();
}

static void MX_BringUpBoard_Init(void)
{
  debug_last_button_level = HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);
}

static void Debug_Log(const char *format, ...)
{
  char buffer[160];
  va_list args;
  int length;

  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }

  if (length >= (int)sizeof(buffer))
  {
    length = (int)sizeof(buffer) - 1;
  }

  (void)HAL_UART_Transmit(&huart3, (uint8_t *)buffer, (uint16_t)length, 100U);
}

static void Debug_UpdateHeartbeat(uint32_t now_ms)
{
  if ((now_ms - debug_last_heartbeat_ms) < 500U)
  {
    return;
  }

  debug_last_heartbeat_ms = now_ms;
  HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

static void Debug_HandleFaultButton(void)
{
  GPIO_PinState button_level = HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);

  if ((button_level == GPIO_PIN_SET) && (debug_last_button_level == GPIO_PIN_RESET) && (debug_fault_button_latched == 0U))
  {
    debug_fault_button_latched = 1U;
    Debug_Log("USER button pressed -> injecting station fault\r\n");
    Station_App_RequestFault(STATION_FAULT_INJECTED);
  }

  debug_last_button_level = button_level;
}

static void Debug_ReportChanges(void)
{
  const StationContext *station = Station_App_GetContext();
  const HeaterControlContext *heater = Heater_Control_GetContext();
  char fault_text[96];

  if ((uint32_t)station->state != debug_last_logged_state)
  {
    debug_last_logged_state = (uint32_t)station->state;
    Debug_Log("station state=%s (%lu)\r\n",
              Debug_GetStationStateName(station->state),
              debug_last_logged_state);
  }

  if (station->fault_flags != debug_last_logged_fault_flags)
  {
    debug_last_logged_fault_flags = station->fault_flags;
    Debug_FormatFaultFlags(debug_last_logged_fault_flags, fault_text, sizeof(fault_text));
    Debug_Log("fault flags=0x%08lX [%s]\r\n", debug_last_logged_fault_flags, fault_text);
  }

  if (heater->measurement_ready != debug_last_logged_measurement_ready)
  {
    debug_last_logged_measurement_ready = heater->measurement_ready;
    Debug_Log("measurement ready=%u samples=%u raw=%u\r\n",
              (unsigned int)heater->measurement_ready,
              (unsigned int)heater->sample_count,
              (unsigned int)heater->filtered_raw);
  }
}

static void Debug_ReportPeriodicStatus(uint32_t now_ms)
{
  const StationContext *station = Station_App_GetContext();
  const HeaterControlContext *heater = Heater_Control_GetContext();
  char fault_text[96];

  if ((now_ms - debug_last_status_report_ms) < 1000U)
  {
    return;
  }

  debug_last_status_report_ms = now_ms;
  Debug_FormatFaultFlags(station->fault_flags, fault_text, sizeof(fault_text));

  Debug_Log("status t=%lu state=%s faults=%s heater_en=%lu tip_clamp=%lu pwm_cmp=%lu raw=%u samples=%u ready=%u\r\n",
            now_ms,
            Debug_GetStationStateName(station->state),
            fault_text,
            g_debug_heater_enable_level,
            g_debug_tip_clamp_level,
            g_debug_heater_pwm_compare,
            (unsigned int)heater->filtered_raw,
            (unsigned int)heater->sample_count,
            (unsigned int)heater->measurement_ready);
}

static const char *Debug_GetStationStateName(StationState state)
{
  switch (state)
  {
    case STATION_STATE_BOOT:
      return "BOOT";

    case STATION_STATE_IDLE:
      return "IDLE";

    case STATION_STATE_NO_CALIBRATION:
      return "NO_CAL";

    case STATION_STATE_FAULT:
      return "FAULT";

    default:
      return "UNKNOWN";
  }
}

static void Debug_FormatFaultFlags(uint32_t fault_flags, char *buffer, size_t buffer_size)
{
  size_t written = 0U;
  int append_length;

  if ((buffer == NULL) || (buffer_size == 0U))
  {
    return;
  }

  if (fault_flags == 0U)
  {
    (void)snprintf(buffer, buffer_size, "none");
    return;
  }

  buffer[0] = '\0';

  if ((fault_flags & STATION_FAULT_OVERCURRENT) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sOVERCURRENT", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((fault_flags & STATION_FAULT_BUCKBOOST) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sBUCKBOOST", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((fault_flags & STATION_FAULT_OPAMP_PGOOD) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sOPAMP_PGOOD", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((fault_flags & STATION_FAULT_INJECTED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sINJECTED", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if (written == 0U)
  {
    (void)snprintf(buffer, buffer_size, "0x%08lX", fault_flags);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
