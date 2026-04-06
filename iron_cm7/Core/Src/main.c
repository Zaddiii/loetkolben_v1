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
#include "calibration.h"
#include "heater.h"
#include "mcp9808.h"
#include "max31856.h"
#include "peripherals.h"
#include "storage.h"
#include "ui.h"
#include <string.h>
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
volatile uint32_t g_debug_active_fault_flags = 0U;
volatile uint32_t g_debug_warning_flags = 0U;
volatile uint32_t g_debug_heater_measurement_ready = 0U;
volatile uint32_t g_debug_heater_filtered_raw = 0U;
volatile uint32_t g_debug_heater_current_raw = 0U;
volatile uint32_t g_debug_buckboost_voltage_raw = 0U;
volatile uint32_t g_debug_buckboost_ref_raw = 0U;
volatile uint32_t g_debug_heater_sample_count = 0U;
volatile uint32_t g_debug_heater_enable_level = 0U;
volatile uint32_t g_debug_tip_clamp_level = 0U;
volatile uint32_t g_debug_heater_pwm_compare = 0U;
volatile uint32_t g_debug_tip_temp_cdeg = 0U;
volatile uint32_t g_debug_target_temp_cdeg = 0U;
volatile int32_t g_debug_ambient_temp_cdeg = 0;
volatile uint32_t g_debug_power_watt_x10 = 0U;
volatile uint32_t g_debug_external_tip_temp_cdeg = 0U;
volatile int32_t g_debug_external_ambient_temp_cdeg = 0;
volatile uint32_t g_debug_calibration_valid = 0U;
volatile uint32_t g_debug_ambient_sensor_ready = 0U;
volatile uint32_t g_debug_external_sensor_ready = 0U;
volatile uint32_t g_debug_sim_tip_temp_cdeg = 0U;
volatile uint32_t g_debug_sim_target_temp_cdeg = 0U;
volatile uint32_t g_debug_sim_power_watt_x10 = 0U;
volatile uint32_t g_debug_ui_screen = 0U;
volatile uint32_t g_debug_ui_menu_item = 0U;
volatile uint32_t g_debug_ui_save_pending = 0U;

static uint32_t debug_last_heartbeat_ms = 0U;
static uint32_t debug_last_status_report_ms = 0U;
static uint32_t debug_last_logged_state = 0xFFFFFFFFU;
static uint32_t debug_last_logged_fault_flags = 0xFFFFFFFFU;
static uint32_t debug_last_logged_warning_flags = 0xFFFFFFFFU;
static uint32_t debug_last_logged_measurement_ready = 0xFFFFFFFFU;
static uint32_t debug_last_ui_event_counter = 0U;
static uint32_t debug_button_press_start_ms = 0U;
static uint32_t debug_status_stream_period_ms = 1000U;
static char debug_console_rx_buffer[48];
static uint8_t debug_console_rx_length = 0U;
static GPIO_PinState debug_last_button_level = GPIO_PIN_RESET;
static uint8_t debug_button_long_press_handled = 0U;
static uint8_t debug_status_stream_enabled = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void MX_GPIO_Init(void);
static void MX_StationSafeState_Init(void);
static void MX_BringUpBoard_Init(void);
static void Debug_Log(const char *format, ...);
static void Debug_UpdateHeartbeat(uint32_t now_ms);
static void Debug_HandleFaultButton(void);
static void Debug_HandleShortButtonPress(void);
static void Debug_HandleLongButtonPress(void);
static void Debug_PollConsole(void);
static void Debug_ProcessConsoleCommand(const char *command);
static void Debug_PrintConsolePrompt(void);
static void Debug_ReportUiEvents(void);
static void Debug_ReportChanges(void);
static void Debug_ReportPeriodicStatus(uint32_t now_ms, uint8_t force);
static const char *Debug_GetStationStateName(StationState state);
static void Debug_FormatFaultFlags(uint32_t fault_flags, char *buffer, size_t buffer_size);
static void Debug_FormatWarningFlags(uint32_t warning_flags, char *buffer, size_t buffer_size);

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
  Ui_Init(debug_status_stream_enabled);
  Debug_Log("iron_cm7 booted\r\n");
  Debug_Log("bring-up profile=%u flags: fault_virtual=%u cal_virtual=%u adc_virtual=%u aux_virtual=%u mcp9808_virtual=%u max31856_virtual=%u\r\n",
            (unsigned int)IRON_BRINGUP_PROFILE,
            (unsigned int)IRON_VIRTUAL_FAULT_INPUTS,
            (unsigned int)IRON_VIRTUAL_CALIBRATION,
            (unsigned int)IRON_VIRTUAL_INTERNAL_ADC,
            (unsigned int)IRON_VIRTUAL_AUX_ADC,
            (unsigned int)IRON_VIRTUAL_MCP9808,
            (unsigned int)IRON_VIRTUAL_MAX31856);
  Debug_Log("LED1 heartbeat active, USART3 debug active, USER button short=calibration long=fault-ack-or-inject\r\n");
  Debug_Log("UART commands: help, status, stream, stream on, stream off, stream <ms>, profile, target, target <degC>, storage, fault, fault ack, fault inject, ui\r\n");
  Debug_Log("status stream default=off\r\n");
  Debug_Log("storage flash_valid=%u err=%u target=%u.%uC cal_valid=%u pts=%u\r\n",
            (unsigned int)Storage_GetContext()->flash_data_valid,
            (unsigned int)Storage_GetContext()->last_error,
            (unsigned int)(Storage_GetContext()->image.target_temp_cdeg / 10U),
            (unsigned int)(Storage_GetContext()->image.target_temp_cdeg % 10U),
            (unsigned int)Storage_GetContext()->image.calibration_valid,
            (unsigned int)Storage_GetContext()->image.point_count);
  Debug_Log("calibration table valid=%u points=%u, MCP9808 init=%u err=%u hal=%u man=0x%04X dev=0x%04X, MAX31856 init=%u err=%u hal=%u\r\n",
            (unsigned int)Calibration_HasValidTable(),
            (unsigned int)Calibration_GetActiveTable()->point_count,
            (unsigned int)Mcp9808_GetContext()->initialized,
            (unsigned int)Mcp9808_GetContext()->last_error,
            (unsigned int)Mcp9808_GetContext()->last_hal_status,
            (unsigned int)Mcp9808_GetContext()->last_manufacturer_id,
            (unsigned int)Mcp9808_GetContext()->last_device_id,
            (unsigned int)Max31856_GetContext()->initialized,
            (unsigned int)Max31856_GetContext()->last_error,
            (unsigned int)Max31856_GetContext()->last_hal_status);
  Debug_Log("MAX31856 diag: err=%u hal=%u\r\n",
            (unsigned int)Max31856_GetContext()->last_error,
            (unsigned int)Max31856_GetContext()->last_hal_status);
  Debug_PrintConsolePrompt();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_ms = HAL_GetTick();

    Station_App_Tick(now_ms);
    Ui_Tick(now_ms);

    {
      const HeaterControlContext *heater = Heater_Control_GetContext();
      Calibration_ProcessBringUp(now_ms,
                                 heater->external_tip_temp_cdeg,
                                 heater->filtered_raw,
                                 heater->external_sensor_ready);
    }

    {
      const StationContext *station = Station_App_GetContext();
      const HeaterControlContext *heater = Heater_Control_GetContext();
      const UiContext *ui = Ui_GetContext();

      ++g_debug_loop_counter;
      g_debug_heartbeat = now_ms;
      g_debug_station_state = (uint32_t)station->state;
      g_debug_fault_flags = station->fault_flags;
      g_debug_active_fault_flags = station->active_fault_flags;
      g_debug_warning_flags = station->warning_flags;
      g_debug_heater_measurement_ready = heater->measurement_ready;
      g_debug_heater_filtered_raw = heater->filtered_raw;
      g_debug_heater_current_raw = heater->heater_current_raw;
      g_debug_buckboost_voltage_raw = heater->buckboost_voltage_raw;
      g_debug_buckboost_ref_raw = heater->buckboost_ref_raw;
      g_debug_heater_sample_count = heater->sample_count;
      g_debug_heater_enable_level = (uint32_t)HAL_GPIO_ReadPin(HEATER_EN_GPIO_Port, HEATER_EN_Pin);
      g_debug_tip_clamp_level = (uint32_t)HAL_GPIO_ReadPin(TIP_CLAMP_GPIO_Port, TIP_CLAMP_Pin);
      g_debug_heater_pwm_compare = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1);
      g_debug_tip_temp_cdeg = heater->tip_temp_cdeg;
      g_debug_target_temp_cdeg = heater->target_temp_cdeg;
      g_debug_ambient_temp_cdeg = heater->ambient_temp_cdeg;
      g_debug_power_watt_x10 = heater->power_watt_x10;
      g_debug_external_tip_temp_cdeg = heater->external_tip_temp_cdeg;
      g_debug_external_ambient_temp_cdeg = heater->external_ambient_temp_cdeg;
      g_debug_calibration_valid = heater->calibration_valid;
      g_debug_ambient_sensor_ready = heater->ambient_sensor_ready;
      g_debug_external_sensor_ready = heater->external_sensor_ready;
      g_debug_sim_tip_temp_cdeg = heater->simulated_tip_temp_cdeg;
      g_debug_sim_target_temp_cdeg = heater->simulated_target_temp_cdeg;
      g_debug_sim_power_watt_x10 = heater->simulated_power_watt_x10;
      g_debug_ui_screen = (uint32_t)ui->screen;
      g_debug_ui_menu_item = (uint32_t)ui->selected_menu_item;
      g_debug_ui_save_pending = (uint32_t)ui->save_pending;
    }

    Debug_UpdateHeartbeat(now_ms);
    Debug_HandleFaultButton();
    Debug_PollConsole();
    Debug_ReportUiEvents();
    Debug_ReportChanges();
    Debug_ReportPeriodicStatus(now_ms, 0U);

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
  char buffer[256];
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
    buffer[length - 1] = '\r';
    buffer[length] = '\0';
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
  uint32_t now_ms = HAL_GetTick();

  if ((button_level == GPIO_PIN_SET) && (debug_last_button_level == GPIO_PIN_RESET))
  {
    debug_button_press_start_ms = now_ms;
    debug_button_long_press_handled = 0U;
  }

  if ((button_level == GPIO_PIN_SET) && (debug_last_button_level == GPIO_PIN_SET) &&
      (debug_button_long_press_handled == 0U) &&
      ((now_ms - debug_button_press_start_ms) >= 1500U))
  {
    debug_button_long_press_handled = 1U;
    Debug_HandleLongButtonPress();
  }

  if ((button_level == GPIO_PIN_RESET) && (debug_last_button_level == GPIO_PIN_SET) &&
      (debug_button_long_press_handled == 0U))
  {
    Debug_HandleShortButtonPress();
  }

  debug_last_button_level = button_level;
}

static void Debug_HandleShortButtonPress(void)
{
  const HeaterControlContext *heater = Heater_Control_GetContext();
  const CalibrationSessionContext *session = Calibration_GetSessionContext();

  if (Calibration_IsBringUpSessionActive())
  {
    uint16_t captured_external_tip_temp_cdeg = session->averaged_external_tip_temp_cdeg;
    uint16_t captured_internal_raw = session->averaged_internal_raw;

    if (Calibration_CaptureBringUpPoint())
    {
      const CalibrationSessionContext *updated_session = Calibration_GetSessionContext();

      Debug_Log("calibration point stored: ext=%u.%uC raw=%u points=%u next=%u.%uC\r\n",
                (unsigned int)(captured_external_tip_temp_cdeg / 10U),
                (unsigned int)(captured_external_tip_temp_cdeg % 10U),
                (unsigned int)captured_internal_raw,
                (unsigned int)updated_session->stored_point_count,
                (unsigned int)(updated_session->target_temp_cdeg / 10U),
                (unsigned int)(updated_session->target_temp_cdeg % 10U));
    }
    else
    {
      Debug_Log("calibration capture rejected: ext_ok=%u stable=%u hold=%lums target=%u.%uC ext=%u.%uC\r\n",
                (unsigned int)heater->external_sensor_ready,
                (unsigned int)session->capture_ready,
                (unsigned long)session->stable_time_ms,
                (unsigned int)(session->target_temp_cdeg / 10U),
                (unsigned int)(session->target_temp_cdeg % 10U),
                (unsigned int)(session->last_external_tip_temp_cdeg / 10U),
                (unsigned int)(session->last_external_tip_temp_cdeg % 10U));
    }

    return;
  }

  if (heater->external_sensor_ready == 0U)
  {
    Debug_Log("calibration start rejected: MAX31856 not ready\r\n");
    return;
  }

  if (Calibration_StartBringUpSession(HAL_GetTick(), heater->external_tip_temp_cdeg, heater->filtered_raw))
  {
    const CalibrationSessionContext *started_session = Calibration_GetSessionContext();

    Debug_Log("calibration session started: ambient=%u.%uC raw=%u next=%u.%uC\r\n",
              (unsigned int)(heater->external_tip_temp_cdeg / 10U),
              (unsigned int)(heater->external_tip_temp_cdeg % 10U),
              (unsigned int)heater->filtered_raw,
              (unsigned int)(started_session->target_temp_cdeg / 10U),
              (unsigned int)(started_session->target_temp_cdeg % 10U));
  }
  else
  {
    Debug_Log("calibration start failed\r\n");
  }
}

static void Debug_HandleLongButtonPress(void)
{
  const CalibrationSessionContext *session = Calibration_GetSessionContext();
  const StationContext *station = Station_App_GetContext();

  if (Calibration_IsBringUpSessionActive())
  {
    if (Calibration_FinalizeBringUpSession())
    {
      (void)Storage_SaveCalibrationTable(Calibration_GetActiveTable());
      Debug_Log("calibration finalized: points=%u valid=%u\r\n",
                (unsigned int)Calibration_GetActiveTable()->point_count,
                (unsigned int)Calibration_HasValidTable());
    }
    else
    {
      Debug_Log("calibration finalize rejected: pending_points=%u minimum=3\r\n",
                (unsigned int)session->stored_point_count);
    }

    return;
  }

  if (station->state == STATION_STATE_FAULT)
  {
    if (Station_App_AcknowledgeFaults(STATION_FAULT_INJECTED) != 0U)
    {
      Debug_Log("fault acknowledged\r\n");
    }
    else
    {
      Debug_Log("fault acknowledge rejected: active=0x%08lX latched=0x%08lX\r\n",
                station->active_fault_flags,
                station->fault_flags);
    }

    return;
  }

  Debug_Log("USER button long press -> injecting station fault\r\n");
  Station_App_RequestFault(STATION_FAULT_INJECTED);
}

static void Debug_PollConsole(void)
{
  uint8_t ch;

  while (HAL_UART_Receive(&huart3, &ch, 1U, 0U) == HAL_OK)
  {
    if ((ch == '\r') || (ch == '\n'))
    {
      if (debug_console_rx_length > 0U)
      {
        debug_console_rx_buffer[debug_console_rx_length] = '\0';
        Debug_ProcessConsoleCommand(debug_console_rx_buffer);
        debug_console_rx_length = 0U;
      }

      continue;
    }

    if (debug_console_rx_length < (uint8_t)(sizeof(debug_console_rx_buffer) - 1U))
    {
      debug_console_rx_buffer[debug_console_rx_length++] = (char)ch;
    }
    else
    {
      debug_console_rx_length = 0U;
      Debug_Log("console: command too long\r\n");
      Debug_PrintConsolePrompt();
    }
  }
}

static void Debug_PrintConsolePrompt(void)
{
  Debug_Log("> ");
}

static void Debug_ProcessConsoleCommand(const char *command)
{
  unsigned int target_deg_c;

  if ((command == NULL) || (command[0] == '\0'))
  {
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "help") == 0)
  {
    Debug_Log("commands: help | status | stream | stream on | stream off | stream <ms> | profile | target | target <degC> | storage | fault | fault ack | fault inject | ui\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "status") == 0)
  {
    debug_last_status_report_ms = 0U;
    Debug_ReportPeriodicStatus(HAL_GetTick(), 1U);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "stream") == 0)
  {
    Debug_Log("stream=%s period=%lums\r\n",
              debug_status_stream_enabled ? "on" : "off",
              (unsigned long)debug_status_stream_period_ms);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "stream on") == 0)
  {
    debug_status_stream_enabled = 1U;
    Ui_SetStreamEnabled(1U);
    debug_last_status_report_ms = 0U;
    Debug_Log("stream enabled period=%lums\r\n", (unsigned long)debug_status_stream_period_ms);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "stream off") == 0)
  {
    debug_status_stream_enabled = 0U;
    Ui_SetStreamEnabled(0U);
    Debug_Log("stream disabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  {
    unsigned int stream_period_ms;

    if (sscanf(command, "stream %u", &stream_period_ms) == 1)
    {
      if (stream_period_ms < 100U)
      {
        stream_period_ms = 100U;
      }

      debug_status_stream_period_ms = stream_period_ms;
      debug_status_stream_enabled = 1U;
      Ui_SetStreamEnabled(1U);
      debug_last_status_report_ms = 0U;
      Debug_Log("stream enabled period=%ums\r\n", stream_period_ms);
      Debug_PrintConsolePrompt();
      return;
    }
  }

  if (strcmp(command, "profile") == 0)
  {
    Debug_Log("profile=%u fault_virtual=%u cal_virtual=%u adc_virtual=%u aux_virtual=%u mcp9808_virtual=%u max31856_virtual=%u\r\n",
              (unsigned int)IRON_BRINGUP_PROFILE,
              (unsigned int)IRON_VIRTUAL_FAULT_INPUTS,
              (unsigned int)IRON_VIRTUAL_CALIBRATION,
              (unsigned int)IRON_VIRTUAL_INTERNAL_ADC,
              (unsigned int)IRON_VIRTUAL_AUX_ADC,
              (unsigned int)IRON_VIRTUAL_MCP9808,
              (unsigned int)IRON_VIRTUAL_MAX31856);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "target") == 0)
  {
    const HeaterControlContext *heater = Heater_Control_GetContext();

    Debug_Log("target=%u.%uC\r\n",
              (unsigned int)(heater->target_temp_cdeg / 10U),
              (unsigned int)(heater->target_temp_cdeg % 10U));
    Debug_PrintConsolePrompt();
    return;
  }

  if (sscanf(command, "target %u", &target_deg_c) == 1)
  {
    Heater_Control_SetTargetTempCdeg((uint16_t)(target_deg_c * 10U));
    (void)Storage_SaveTargetTemp((uint16_t)(target_deg_c * 10U));
    Debug_Log("target set to %u.0C\r\n", target_deg_c);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "storage") == 0)
  {
    const StorageContext *storage = Storage_GetContext();

    Debug_Log("storage: flash_valid=%u err=%u dirty=%u crc=0x%08lX target=%u.%uC cal_valid=%u pts=%u\r\n",
              (unsigned int)storage->flash_data_valid,
              (unsigned int)storage->last_error,
              (unsigned int)storage->dirty,
              storage->last_crc32,
              (unsigned int)(storage->image.target_temp_cdeg / 10U),
              (unsigned int)(storage->image.target_temp_cdeg % 10U),
              (unsigned int)storage->image.calibration_valid,
              (unsigned int)storage->image.point_count);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fault") == 0)
  {
    const StationContext *station = Station_App_GetContext();
    char active_fault_text[96];
    char latched_fault_text[96];
    char warning_text[96];

    Debug_FormatFaultFlags(station->active_fault_flags, active_fault_text, sizeof(active_fault_text));
    Debug_FormatFaultFlags(station->fault_flags, latched_fault_text, sizeof(latched_fault_text));
    Debug_FormatWarningFlags(station->warning_flags, warning_text, sizeof(warning_text));
    Debug_Log("fault: active=0x%08lX [%s] latched=0x%08lX [%s] warnings=0x%08lX [%s] ack_pending=%u\r\n",
              station->active_fault_flags,
              active_fault_text,
              station->fault_flags,
              latched_fault_text,
              station->warning_flags,
              warning_text,
              (unsigned int)station->fault_ack_pending);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fault ack") == 0)
  {
    if (Station_App_AcknowledgeFaults(STATION_FAULT_INJECTED) != 0U)
    {
      const StationContext *updated_station = Station_App_GetContext();

      Debug_Log("fault acknowledged: state=%s active=0x%08lX latched=0x%08lX\r\n",
                Debug_GetStationStateName(updated_station->state),
                updated_station->active_fault_flags,
                updated_station->fault_flags);
    }
    else
    {
      const StationContext *updated_station = Station_App_GetContext();

      Debug_Log("fault acknowledge rejected: active=0x%08lX latched=0x%08lX\r\n",
                updated_station->active_fault_flags,
                updated_station->fault_flags);
    }

    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fault inject") == 0)
  {
    Station_App_RequestFault(STATION_FAULT_INJECTED);
    Debug_Log("fault injected\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "ui") == 0)
  {
    const UiContext *ui = Ui_GetContext();

    Debug_Log("ui: screen=%s menu=%s save_pending=%u target=%u.%uC stream=%u\r\n",
              (ui->screen == UI_SCREEN_MAIN) ? "MAIN" : "MENU",
              Ui_GetMenuItemName((UiMenuItem)ui->selected_menu_item),
              (unsigned int)ui->save_pending,
              (unsigned int)(ui->target_temp_cdeg / 10U),
              (unsigned int)(ui->target_temp_cdeg % 10U),
              (unsigned int)ui->stream_enabled);
    Debug_PrintConsolePrompt();
    return;
  }

  Debug_Log("unknown command: %s\r\n", command);
  Debug_PrintConsolePrompt();
}

static void Debug_ReportUiEvents(void)
{
  const UiContext *ui = Ui_GetContext();

  if (ui->event_counter == debug_last_ui_event_counter)
  {
    return;
  }

  debug_last_ui_event_counter = ui->event_counter;

  switch ((UiEvent)ui->last_event)
  {
    case UI_EVENT_TARGET_CHANGED:
      Debug_Log("ui target changed: %u.%uC\r\n",
                (unsigned int)(ui->event_value / 10U),
                (unsigned int)(ui->event_value % 10U));
      break;

    case UI_EVENT_TARGET_SAVED:
      Debug_Log("ui target saved: %u.%uC result=%u\r\n",
                (unsigned int)(ui->event_value / 10U),
                (unsigned int)(ui->event_value % 10U),
                (unsigned int)ui->last_storage_result);
      break;

    case UI_EVENT_MENU_ENTERED:
      Debug_Log("ui menu entered: %s\r\n", Ui_GetMenuItemName((UiMenuItem)ui->event_value));
      break;

    case UI_EVENT_MENU_SELECTION_CHANGED:
      Debug_Log("ui menu select: %s\r\n", Ui_GetMenuItemName((UiMenuItem)ui->event_value));
      break;

    case UI_EVENT_MENU_EXITED:
      Debug_Log("ui menu exited\r\n");
      break;

    case UI_EVENT_CALIBRATION_STARTED:
      Debug_Log("ui calibration started\r\n");
      break;

    case UI_EVENT_CALIBRATION_REJECTED:
      Debug_Log("ui calibration rejected: ext_ok=%u\r\n", (unsigned int)ui->event_value);
      break;

    case UI_EVENT_STREAM_TOGGLED:
      debug_status_stream_enabled = ui->stream_enabled;
      Debug_Log("ui stream %s\r\n", ui->stream_enabled ? "enabled" : "disabled");
      break;

    case UI_EVENT_FAULT_CLEAR_REQUESTED:
      Debug_Log("ui fault clear requested\r\n");
      break;

    default:
      break;
  }
}

static void Debug_ReportChanges(void)
{
  const StationContext *station = Station_App_GetContext();
  const HeaterControlContext *heater = Heater_Control_GetContext();
  char fault_text[96];
  char warning_text[96];
  long ambient_whole;
  long ambient_frac;

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
    Debug_Log("fault flags=0x%08lX [%s] active=0x%08lX ack=%u\r\n",
              debug_last_logged_fault_flags,
              fault_text,
              station->active_fault_flags,
              (unsigned int)station->fault_ack_pending);
  }

  if (station->warning_flags != debug_last_logged_warning_flags)
  {
    debug_last_logged_warning_flags = station->warning_flags;
    Debug_FormatWarningFlags(debug_last_logged_warning_flags, warning_text, sizeof(warning_text));
    Debug_Log("warning flags=0x%08lX [%s]\r\n", debug_last_logged_warning_flags, warning_text);
  }

  if (heater->measurement_ready != debug_last_logged_measurement_ready)
  {
    debug_last_logged_measurement_ready = heater->measurement_ready;
    ambient_whole = (long)(heater->ambient_temp_cdeg / 10);
    ambient_frac = (long)(heater->ambient_temp_cdeg >= 0 ? (heater->ambient_temp_cdeg % 10) : -(heater->ambient_temp_cdeg % 10));
    Debug_Log("measurement ready=%u n=%u raw=%u current=%u vbus=%u vref=%u tip=%u.%uC tgt=%u.%uC amb=%ld.%ldC ext=%u.%uC cal=%u amb_ok=%u ext_ok=%u pwr=%u.%uW\r\n",
              (unsigned int)heater->measurement_ready,
              (unsigned int)heater->sample_count,
              (unsigned int)heater->filtered_raw,
              (unsigned int)heater->heater_current_raw,
              (unsigned int)heater->buckboost_voltage_raw,
              (unsigned int)heater->buckboost_ref_raw,
              (unsigned int)(heater->tip_temp_cdeg / 10U),
              (unsigned int)(heater->tip_temp_cdeg % 10U),
              (unsigned int)(heater->target_temp_cdeg / 10U),
              (unsigned int)(heater->target_temp_cdeg % 10U),
              ambient_whole,
              ambient_frac,
              (unsigned int)(heater->external_tip_temp_cdeg / 10U),
              (unsigned int)(heater->external_tip_temp_cdeg % 10U),
              (unsigned int)heater->calibration_valid,
              (unsigned int)heater->ambient_sensor_ready,
              (unsigned int)heater->external_sensor_ready,
              (unsigned int)(heater->power_watt_x10 / 10U),
              (unsigned int)(heater->power_watt_x10 % 10U));
  }
}

static void Debug_ReportPeriodicStatus(uint32_t now_ms, uint8_t force)
{
  const StationContext *station = Station_App_GetContext();
  const HeaterControlContext *heater = Heater_Control_GetContext();
  const CalibrationSessionContext *session = Calibration_GetSessionContext();
  const Mcp9808Context *mcp9808 = Mcp9808_GetContext();
  const Max31856Context *max31856 = Max31856_GetContext();
  char fault_text[96];
  char warning_text[96];

  if ((force == 0U) && (debug_status_stream_enabled == 0U))
  {
    return;
  }

  if ((force == 0U) && ((now_ms - debug_last_status_report_ms) < debug_status_stream_period_ms))
  {
    return;
  }

  debug_last_status_report_ms = now_ms;
  Debug_FormatFaultFlags(station->fault_flags, fault_text, sizeof(fault_text));
  Debug_FormatWarningFlags(station->warning_flags, warning_text, sizeof(warning_text));

  Debug_Log("status t=%lu state=%s faults=%s active=0x%08lX warns=%s ack=%u en=%lu clamp=%lu pwm=%lu raw=%u cur=%u vbus=%u vref=%u tip=%u.%uC tgt=%u.%uC ext=%u.%uC cal=%u amb_ok=%u ext_ok=%u mcp_init=%u mcp_err=%u mcp_hal=%u max_init=%u max_err=%u max_hal=%u cal_mode=%u cal_target=%u.%uC cal_stable=%u hold=%lums pts=%u pwr=%u.%uW n=%u ready=%u\r\n",
            now_ms,
            Debug_GetStationStateName(station->state),
            fault_text,
            station->active_fault_flags,
            warning_text,
            (unsigned int)station->fault_ack_pending,
            g_debug_heater_enable_level,
            g_debug_tip_clamp_level,
            g_debug_heater_pwm_compare,
            (unsigned int)heater->filtered_raw,
            (unsigned int)heater->heater_current_raw,
            (unsigned int)heater->buckboost_voltage_raw,
            (unsigned int)heater->buckboost_ref_raw,
            (unsigned int)(heater->tip_temp_cdeg / 10U),
            (unsigned int)(heater->tip_temp_cdeg % 10U),
            (unsigned int)(heater->target_temp_cdeg / 10U),
            (unsigned int)(heater->target_temp_cdeg % 10U),
            (unsigned int)(heater->external_tip_temp_cdeg / 10U),
            (unsigned int)(heater->external_tip_temp_cdeg % 10U),
            (unsigned int)heater->calibration_valid,
            (unsigned int)heater->ambient_sensor_ready,
            (unsigned int)heater->external_sensor_ready,
            (unsigned int)mcp9808->initialized,
            (unsigned int)mcp9808->last_error,
            (unsigned int)mcp9808->last_hal_status,
            (unsigned int)max31856->initialized,
            (unsigned int)max31856->last_error,
            (unsigned int)max31856->last_hal_status,
            (unsigned int)session->active,
            (unsigned int)(session->target_temp_cdeg / 10U),
            (unsigned int)(session->target_temp_cdeg % 10U),
            (unsigned int)session->capture_ready,
            (unsigned long)session->stable_time_ms,
            (unsigned int)session->stored_point_count,
            (unsigned int)(heater->power_watt_x10 / 10U),
            (unsigned int)(heater->power_watt_x10 % 10U),
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

    case STATION_STATE_HEATING:
      return "HEATING";

    case STATION_STATE_READY:
      return "READY";

    case STATION_STATE_CALIBRATION:
      return "CAL";

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

  if ((fault_flags & STATION_FAULT_TIP_OVERTEMP) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sTIP_OVERTEMP", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((fault_flags & STATION_FAULT_AMBIENT_OVERTEMP) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sAMBIENT_OVERTEMP", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((fault_flags & STATION_FAULT_AMBIENT_SENSOR) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sAMBIENT_SENSOR", (written > 0U) ? "|" : "");
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

static void Debug_FormatWarningFlags(uint32_t warning_flags, char *buffer, size_t buffer_size)
{
  size_t written = 0U;
  int append_length;

  if ((buffer == NULL) || (buffer_size == 0U))
  {
    return;
  }

  if (warning_flags == 0U)
  {
    (void)snprintf(buffer, buffer_size, "none");
    return;
  }

  buffer[0] = '\0';

  if ((warning_flags & STATION_WARNING_MCP9808_ERROR) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sMCP9808_ERROR", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_AMBIENT_SENSOR_MISSING) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sAMBIENT_SENSOR_MISSING", (written > 0U) ? "|" : "");
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
    (void)snprintf(buffer, buffer_size, "0x%08lX", warning_flags);
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
