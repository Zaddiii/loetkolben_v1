#include "debug_console.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "main.h"
#include "peripherals.h"
#include "calibration.h"
#include "display.h"
#include "fan.h"
#include "heater.h"
#include "ina238.h"
#include "mcp9808.h"
#include "max31856.h"
#include "st7789.h"
#include "storage.h"
#include "ui.h"
#include "app.h"

/* Global debug variables */
volatile uint32_t g_debug_heartbeat = 0U;
volatile uint32_t g_debug_loop_counter = 0U;
volatile uint32_t g_debug_station_state = 0U;
volatile uint32_t g_debug_station_mode = 0U;
volatile uint32_t g_debug_station_docked = 0U;
volatile uint32_t g_debug_fault_flags = 0U;
volatile uint32_t g_debug_active_fault_flags = 0U;
volatile uint32_t g_debug_warning_flags = 0U;
volatile uint32_t g_debug_heater_measurement_ready = 0U;
volatile uint32_t g_debug_heater_filtered_raw = 0U;
volatile uint32_t g_debug_heater_current_raw = 0U;
volatile uint32_t g_debug_heater_current_ma = 0U;
volatile uint32_t g_debug_buckboost_voltage_raw = 0U;
volatile uint32_t g_debug_buckboost_voltage_mv = 0U;
volatile uint32_t g_debug_buckboost_ref_raw = 0U;
volatile uint32_t g_debug_buckboost_target_mv = 0U;
volatile uint32_t g_debug_heater_sample_count = 0U;
volatile uint32_t g_debug_heater_enable_level = 0U;
volatile uint32_t g_debug_tip_clamp_level = 0U;
volatile uint32_t g_debug_heater_pwm_compare = 0U;
volatile uint32_t g_debug_tip_temp_cdeg = 0U;
volatile uint32_t g_debug_target_temp_cdeg = 0U;
volatile uint32_t g_debug_effective_target_temp_cdeg = 0U;
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
volatile uint32_t g_debug_fan_pwm_permille = 0U;
volatile uint32_t g_debug_fan_tach_rpm = 0U;
volatile uint32_t g_debug_display_version = 0U;

/* Static debug variables */
static uint32_t debug_last_heartbeat_ms = 0U;
static uint32_t debug_last_status_report_ms = 0U;
static uint32_t debug_last_logged_state = 0xFFFFFFFFU;
static uint32_t debug_last_logged_mode = 0xFFFFFFFFU;
static uint32_t debug_last_logged_docked = 0xFFFFFFFFU;
static uint32_t debug_last_logged_fault_flags = 0xFFFFFFFFU;
static uint32_t debug_last_logged_warning_flags = 0xFFFFFFFFU;
static uint32_t debug_last_logged_measurement_ready = 0xFFFFFFFFU;
static uint32_t debug_last_ui_event_counter = 0U;
static uint32_t debug_button_press_start_ms = 0U;
static uint32_t debug_status_stream_period_ms = 1000U;
static uint32_t debug_encoder_live_period_ms = 100U;
static uint32_t debug_last_encoder_live_report_ms = 0U;
static char debug_console_rx_buffer[128];
static uint8_t debug_console_rx_length = 0U;
static uint8_t debug_console_discard_until_newline = 0U;
static uint8_t debug_console_discard_escape_sequence = 0U;
static GPIO_PinState debug_last_button_level = GPIO_PIN_RESET;
static uint8_t debug_button_long_press_handled = 0U;
static uint8_t debug_status_stream_enabled = 0U;
static uint8_t debug_encoder_live_enabled = 0U;
static uint8_t debug_measurement_log_enabled = 0U;
static uint32_t debug_measurement_rise_count = 0U;
static uint32_t debug_measurement_fall_count = 0U;
static uint32_t debug_measurement_missed_period_count = 0U;
static uint32_t debug_measurement_fast_period_count = 0U;
static uint32_t debug_measurement_last_rise_ms = 0U;

/* Forward declarations */
static void Debug_WriteRaw(const char *text, uint16_t length);
static void Debug_Log(const char *format, ...);
static void Debug_UpdateHeartbeat(uint32_t now_ms);
static void Debug_HandleFaultButton(void);
static void Debug_HandleShortButtonPress(void);
static void Debug_HandleLongButtonPress(void);
static void Debug_PollConsole(void);
static void Debug_ProcessConsoleCommand(const char *command);
static void Debug_NormalizeConsoleCommand(char *command);
static void Debug_PrintConsolePrompt(void);
static uint8_t Debug_IsConsoleInputActive(void);
static void Debug_ReportUiEvents(void);
static void Debug_ReportChanges(void);
static void Debug_ReportPeriodicStatus(uint32_t now_ms, uint8_t force);
static void Debug_ReportSimulationStatus(void);
static void Debug_ReportInaStatus(void);
static void Debug_ReportScreen(void);
static void Debug_ReportEncoderStatus(void);
static void Debug_ReportEncoderLive(uint32_t now_ms, uint8_t force);
static const char *Debug_GetStationStateName(StationState state);
static const char *Debug_GetStationOperatingModeName(StationOperatingMode mode);
static void Debug_FormatFaultFlags(uint32_t fault_flags, char *buffer, size_t buffer_size);
static void Debug_FormatWarningFlags(uint32_t warning_flags, char *buffer, size_t buffer_size);
static uint32_t Debug_GetTim3KernelClockHz(void);

static uint32_t Debug_GetTim3KernelClockHz(void)
{
  RCC_ClkInitTypeDef clock_config;
  uint32_t flash_latency;
  uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();

  HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

  if (clock_config.APB1CLKDivider == RCC_HCLK_DIV1)
  {
    return pclk1_hz;
  }

  return pclk1_hz * 2U;
}

static void Debug_WriteRaw(const char *text, uint16_t length)
{
  if ((text == NULL) || (length == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart3, (uint8_t *)text, length, 100U);
}

static void Debug_Log(const char *format, ...)
{
  char buffer[384];
  va_list args;
  int length;
  uint8_t input_in_progress = (debug_console_rx_length > 0U) ? 1U : 0U;

  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }

  if (length >= (int)sizeof(buffer))
  {
    length = (int)sizeof(buffer) - 3;
    buffer[length++] = '\r';
    buffer[length++] = '\n';
    buffer[length] = '\0';
  }

  if (input_in_progress != 0U)
  {
    static const char line_break[] = "\r\n";

    Debug_WriteRaw(line_break, (uint16_t)(sizeof(line_break) - 1U));
  }

  Debug_WriteRaw(buffer, (uint16_t)length);

  if (input_in_progress != 0U)
  {
    static const char prompt[] = "> ";

    Debug_WriteRaw(prompt, (uint16_t)(sizeof(prompt) - 1U));
    Debug_WriteRaw(debug_console_rx_buffer, debug_console_rx_length);
  }
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
    if (Station_App_AcknowledgeFaults(STATION_FAULT_ACK_MASK) != 0U)
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
    if (debug_console_discard_escape_sequence != 0U)
    {
      if ((ch >= 0x40U) && (ch <= 0x7EU))
      {
        debug_console_discard_escape_sequence = 0U;
      }
      continue;
    }

    if (ch == 0x1BU)
    {
      debug_console_discard_escape_sequence = 1U;
      continue;
    }

    if ((ch == '\r') || (ch == '\n'))
    {
      static const char line_break[] = "\r\n";

      Debug_WriteRaw(line_break, (uint16_t)(sizeof(line_break) - 1U));

      if (debug_console_discard_until_newline != 0U)
      {
        debug_console_discard_until_newline = 0U;
        debug_console_rx_length = 0U;
        Debug_Log("console: command too long\r\n");
        Debug_PrintConsolePrompt();
        continue;
      }

      if (debug_console_rx_length > 0U)
      {
        char command_buffer[sizeof(debug_console_rx_buffer)];

        debug_console_rx_buffer[debug_console_rx_length] = '\0';
        Debug_NormalizeConsoleCommand(debug_console_rx_buffer);
        (void)snprintf(command_buffer, sizeof(command_buffer), "%s", debug_console_rx_buffer);
        debug_console_rx_length = 0U;
        Debug_ProcessConsoleCommand(command_buffer);
      }
      else
      {
        Debug_PrintConsolePrompt();
      }

      continue;
    }

    if ((ch == '\b') || (ch == 0x7FU))
    {
      if (debug_console_rx_length > 0U)
      {
        debug_console_rx_length--;
      }
      continue;
    }

    if (debug_console_discard_until_newline != 0U)
    {
      continue;
    }

    if (debug_console_rx_length < (uint8_t)(sizeof(debug_console_rx_buffer) - 1U))
    {
      debug_console_rx_buffer[debug_console_rx_length++] = (char)ch;
    }
    else
    {
      debug_console_rx_length = 0U;
      debug_console_discard_until_newline = 1U;
    }
  }
}

static void Debug_NormalizeConsoleCommand(char *command)
{
  size_t read_index;
  size_t write_index = 0U;
  size_t start_index = 0U;
  size_t end_index;
  uint8_t previous_was_space = 1U;

  if (command == NULL)
  {
    return;
  }

  for (read_index = 0U; command[read_index] != '\0'; ++read_index)
  {
    unsigned char current = (unsigned char)command[read_index];

    if (isspace(current) != 0)
    {
      if (previous_was_space == 0U)
      {
        command[write_index++] = ' ';
        previous_was_space = 1U;
      }
      continue;
    }

    if (current < 0x20U)
    {
      continue;
    }

    command[write_index++] = (char)tolower(current);
    previous_was_space = 0U;
  }

  command[write_index] = '\0';

  while ((command[start_index] != '\0') && isspace((unsigned char)command[start_index]))
  {
    ++start_index;
  }

  end_index = strlen(command);
  while ((end_index > start_index) && isspace((unsigned char)command[end_index - 1U]))
  {
    --end_index;
  }

  if (start_index > 0U)
  {
    memmove(command, command + start_index, end_index - start_index);
  }

  command[end_index - start_index] = '\0';
}

static void Debug_PrintConsolePrompt(void)
{
  static const char prompt[] = "> ";

  Debug_WriteRaw(prompt, (uint16_t)(sizeof(prompt) - 1U));
}

static uint8_t Debug_IsConsoleInputActive(void)
{
  if (debug_console_rx_length > 0U)
  {
    return 1U;
  }

  if ((debug_console_discard_until_newline != 0U) || (debug_console_discard_escape_sequence != 0U))
  {
    return 1U;
  }

  return 0U;
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
    Debug_Log("commands: help | ping | version | health | status | stream | stream on | stream off | stream <ms> | profile | target | target <degC> | amb | amb <degC> | storage | fault | fault ack | fault inject | ui | enc | enc live | enc live on | enc live off | enc live <ms> | cal | cal start | cal point | cal finish | cal cancel | sim | sim ambient <degC> | sim load <percent> | sim tip <degC> | sim reset | dock | dock on | dock off | dock toggle | fan | fan cal | fan cal <ppm> | fan cal reset | meas | meas on | meas off | meas stats | meas reset | ina | screen | screen next | screen <page>\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "ping") == 0)
  {
    Debug_Log("pong t=%lu loop=%lu\r\n",
              (unsigned long)HAL_GetTick(),
              (unsigned long)g_debug_loop_counter);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "version") == 0)
  {
    Debug_Log("version iron_cm7 build=%s %s profile=%u git=workspace\r\n",
              __DATE__,
              __TIME__,
              (unsigned int)IRON_BRINGUP_PROFILE);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "health") == 0)
  {
    const StationContext *station = Station_App_GetContext();
    const HeaterControlContext *heater = Heater_Control_GetContext();

    Debug_Log("health state=%s mode=%s faults=0x%08lX warns=0x%08lX meas=%u stream=%u disp=%u ina=%u amb=%u ext=%u\r\n",
              Debug_GetStationStateName(station->state),
              Debug_GetStationOperatingModeName(station->operating_mode),
              station->fault_flags,
              station->warning_flags,
              (unsigned int)heater->measurement_ready,
              (unsigned int)debug_status_stream_enabled,
              (unsigned int)St7789_GetContext()->initialized,
              (unsigned int)Ina238_GetContext()->initialized,
              (unsigned int)heater->ambient_sensor_ready,
              (unsigned int)heater->external_sensor_ready);
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
    Debug_Log("profile=%u fault_virtual=%u cal_virtual=%u adc_virtual=%u aux_virtual=%u ina_virtual=%u mcp9808_virtual=%u max31856_virtual=%u dock_virtual=%u\r\n",
              (unsigned int)IRON_BRINGUP_PROFILE,
              (unsigned int)IRON_VIRTUAL_FAULT_INPUTS,
              (unsigned int)IRON_VIRTUAL_CALIBRATION,
              (unsigned int)IRON_VIRTUAL_INTERNAL_ADC,
              (unsigned int)IRON_VIRTUAL_AUX_ADC,
              (unsigned int)IRON_VIRTUAL_INA238,
              (unsigned int)IRON_VIRTUAL_MCP9808,
              (unsigned int)IRON_VIRTUAL_MAX31856,
              (unsigned int)IRON_VIRTUAL_DOCK_INPUT);
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

  if (strcmp(command, "amb") == 0)
  {
    uint16_t ambient_cdeg = Heater_Test_GetAmbientTempCdeg();

    Debug_Log("ambient=%u.%uC (fan test threshold=40.0C -> 50%% pwm)\r\n",
              (unsigned int)(ambient_cdeg / 10U),
              (unsigned int)(ambient_cdeg % 10U));
    Debug_PrintConsolePrompt();
    return;
  }

  {
    unsigned int ambient_deg_c;

    if (sscanf(command, "amb %u", &ambient_deg_c) == 1)
    {
      Heater_Test_SetAmbientTempCdeg((uint16_t)(ambient_deg_c * 10U));
      Debug_Log("ambient set to %u.0C\r\n", ambient_deg_c);
      Debug_PrintConsolePrompt();
      return;
    }
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
    if (Station_App_AcknowledgeFaults(STATION_FAULT_ACK_MASK) != 0U)
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

    Debug_Log("ui: screen=%s menu=%s page=%s save_pending=%u target=%u.%uC stream=%u\r\n",
              (ui->screen == UI_SCREEN_MAIN) ? "MAIN" : "MENU",
              Ui_GetMenuItemName((UiMenuItem)ui->selected_menu_item),
              Display_GetPageName((DisplayPage)Display_GetContext()->page),
              (unsigned int)ui->save_pending,
              (unsigned int)(ui->target_temp_cdeg / 10U),
              (unsigned int)(ui->target_temp_cdeg % 10U),
              (unsigned int)ui->stream_enabled);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "cal") == 0)
  {
    const CalibrationSessionContext *session = Calibration_GetSessionContext();

    Debug_Log("cal: active=%u valid=%u pts=%u next=%u.%uC stable=%u hold=%lums cap=%u ext=%u.%uC raw=%u\r\n",
              (unsigned int)session->active,
              (unsigned int)Calibration_HasValidTable(),
              (unsigned int)(session->active ? session->stored_point_count : Calibration_GetActiveTable()->point_count),
              (unsigned int)(session->target_temp_cdeg / 10U),
              (unsigned int)(session->target_temp_cdeg % 10U),
              (unsigned int)session->stable,
              (unsigned long)session->stable_time_ms,
              (unsigned int)session->capture_ready,
              (unsigned int)(session->averaged_external_tip_temp_cdeg / 10U),
              (unsigned int)(session->averaged_external_tip_temp_cdeg % 10U),
              (unsigned int)session->averaged_internal_raw);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "cal start") == 0)
  {
    const HeaterControlContext *heater = Heater_Control_GetContext();

    if (Calibration_StartBringUpSession(HAL_GetTick(), heater->external_tip_temp_cdeg, heater->filtered_raw))
    {
      Debug_Log("calibration session started\r\n");
    }
    else
    {
      Debug_Log("calibration start failed\r\n");
    }

    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "cal point") == 0)
  {
    if (Calibration_CaptureBringUpPoint())
    {
      Debug_Log("calibration point captured pts=%u next=%u.%uC\r\n",
                (unsigned int)Calibration_GetSessionContext()->stored_point_count,
                (unsigned int)(Calibration_GetSessionContext()->target_temp_cdeg / 10U),
                (unsigned int)(Calibration_GetSessionContext()->target_temp_cdeg % 10U));
    }
    else
    {
      Debug_Log("calibration point capture rejected\r\n");
    }

    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "cal finish") == 0)
  {
    if (Calibration_FinalizeBringUpSession())
    {
      (void)Storage_SaveCalibrationTable(Calibration_GetActiveTable());
      Debug_Log("calibration finalized pts=%u valid=%u\r\n",
                (unsigned int)Calibration_GetActiveTable()->point_count,
                (unsigned int)Calibration_HasValidTable());
    }
    else
    {
      Debug_Log("calibration finalize rejected\r\n");
    }

    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "cal cancel") == 0)
  {
    Calibration_CancelBringUpSession();
    Debug_Log("calibration canceled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "dock") == 0)
  {
    const StationContext *station = Station_App_GetContext();

    Debug_Log("dock=%s standby=%u eff_target=%u.%uC\r\n",
              station->docked ? "on" : "off",
              (unsigned int)station->standby_active,
              (unsigned int)(station->effective_target_temp_cdeg / 10U),
              (unsigned int)(station->effective_target_temp_cdeg % 10U));
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "dock on") == 0)
  {
    Station_App_SetDocked(1U);
    Debug_Log("dock enabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "dock off") == 0)
  {
    Station_App_SetDocked(0U);
    Debug_Log("dock disabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "dock toggle") == 0)
  {
    Station_App_SetDocked((uint8_t)(Station_App_GetContext()->docked == 0U));
    Debug_Log("dock toggled -> %s\r\n", Station_App_GetContext()->docked ? "on" : "off");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fan cal") == 0)
  {
    Debug_Log("fan cal=%luppm\r\n", (unsigned long)Fan_GetRpmCalibrationPpm());
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fan cal reset") == 0)
  {
    Fan_SetRpmCalibrationPpm(1000000U);
    Debug_Log("fan calibration reset to %luppm\r\n", (unsigned long)Fan_GetRpmCalibrationPpm());
    Debug_PrintConsolePrompt();
    return;
  }

  {
    unsigned int fan_calibration_ppm;

    if (sscanf(command, "fan cal %u", &fan_calibration_ppm) == 1)
    {
      Fan_SetRpmCalibrationPpm((uint32_t)fan_calibration_ppm);
      Debug_Log("fan calibration set to %luppm\r\n", (unsigned long)Fan_GetRpmCalibrationPpm());
      Debug_PrintConsolePrompt();
      return;
    }
  }

  if (strcmp(command, "fan") == 0)
  {
    const FanContext *fan = Fan_GetContext();

    Debug_Log("fan: duty_target=%u%% rpm_now=%u rpm_target=%u rpm_err=%u freq=%lu.%02luHz period_us=%lu age_ms=%lu valid=%lu rej=%lu cal=%luppm enabled=%u cooling_req=%u\r\n",
              (unsigned int)(fan->requested_pwm_permille / 10U),
              (unsigned int)fan->tach_rpm,
              (unsigned int)fan->target_rpm,
              (unsigned int)fan->tach_error_rpm,
              (unsigned long)(fan->tach_signal_hz_x100 / 100U),
              (unsigned long)(fan->tach_signal_hz_x100 % 100U),
              (unsigned long)fan->tach_pulses_window,
              (unsigned long)fan->tach_window_ms,
              (unsigned long)fan->tach_valid_edges,
              (unsigned long)fan->tach_rejected_edges,
              (unsigned long)fan->tach_calibration_ppm,
              (unsigned int)fan->enabled,
              (unsigned int)fan->cooling_request);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fan diag") == 0)
  {
    uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();
    uint32_t timclk_hz = Fan_GetTachTimerClockHz();
    uint32_t tick_hz = Fan_GetTachTickHz();
    uint16_t psc = Fan_GetTachPrescaler();
    uint32_t cal_ppm = Fan_GetRpmCalibrationPpm();

    Debug_Log("fan diag: pclk2=%luHz tim17_clk=%luHz prescaler=%u tick_hz=%luHz cal=%luppm formula=f_hz=(tick_hz*edges/sum_ticks)\r\n",
              (unsigned long)pclk2_hz,
              (unsigned long)timclk_hz,
              (unsigned int)psc,
              (unsigned long)tick_hz,
              (unsigned long)cal_ppm);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "fan raw") == 0)
  {
    uint32_t raw_ticks[16];
    uint32_t raw_count;
    uint32_t i;
    uint64_t sum_ticks = 0ULL;
    uint64_t freq_hz_x100 = 0ULL;
    uint32_t tick_hz = Fan_GetTachTickHz();

    raw_count = Fan_DebugCopyRawPeriods(raw_ticks, 16U);
    if (raw_count == 0U)
    {
      Debug_Log("fan raw: no samples\r\n");
      Debug_PrintConsolePrompt();
      return;
    }

    Debug_Log("fan raw: count=%lu ticks=", (unsigned long)raw_count);
    for (i = 0U; i < raw_count; ++i)
    {
      sum_ticks += (uint64_t)raw_ticks[i];
      Debug_Log("%lu%s", (unsigned long)raw_ticks[i], (i + 1U < raw_count) ? "," : "");
    }

    if (sum_ticks > 0ULL)
    {
      freq_hz_x100 = (((uint64_t)tick_hz * 100ULL * (uint64_t)raw_count) + (sum_ticks / 2ULL)) / sum_ticks;
    }

    Debug_Log(" -> sum=%lu tick_hz=%lu f=%lu.%02luHz\r\n",
              (unsigned long)sum_ticks,
              (unsigned long)tick_hz,
              (unsigned long)(freq_hz_x100 / 100ULL),
              (unsigned long)(freq_hz_x100 % 100ULL));
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "refclk") == 0)
  {
    uint32_t timclk_hz = Debug_GetTim3KernelClockHz();
    uint32_t psc = htim3.Init.Prescaler + 1U;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;
    uint64_t f_hz_x100 = (((uint64_t)timclk_hz * 100ULL) + ((uint64_t)psc * (uint64_t)arr / 2ULL)) /
                         ((uint64_t)psc * (uint64_t)arr);

    Debug_Log("refclk: pin=PA7(TIM3_CH2) tim3_clk=%luHz psc=%lu arr=%lu duty=%lu/%lu expected=%lu.%02luHz\r\n",
              (unsigned long)timclk_hz,
              (unsigned long)(psc - 1U),
              (unsigned long)(arr - 1U),
              (unsigned long)__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_2),
              (unsigned long)(arr - 1U),
              (unsigned long)(f_hz_x100 / 100ULL),
              (unsigned long)(f_hz_x100 % 100ULL));
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "refclk on") == 0)
  {
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK)
    {
      Debug_Log("refclk: start failed\r\n");
      Debug_PrintConsolePrompt();
      return;
    }

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, arr / 2U);
    Debug_Log("refclk: enabled on PA7 (TIM3_CH2), 50%% duty\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "refclk off") == 0)
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
    Debug_Log("refclk: output disabled (duty=0)\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if ((strcmp(command, "meas") == 0) || (strcmp(command, "meas stats") == 0))
  {
    Debug_Log("meas: stream=%s rise=%lu fall=%lu missed=%lu fast=%lu\r\n",
              debug_measurement_log_enabled ? "on" : "off",
              (unsigned long)debug_measurement_rise_count,
              (unsigned long)debug_measurement_fall_count,
              (unsigned long)debug_measurement_missed_period_count,
              (unsigned long)debug_measurement_fast_period_count);
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "meas on") == 0)
  {
    debug_measurement_log_enabled = 1U;
    Debug_Log("measurement stream enabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "meas off") == 0)
  {
    debug_measurement_log_enabled = 0U;
    Debug_Log("measurement stream disabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "meas reset") == 0)
  {
    debug_measurement_rise_count = 0U;
    debug_measurement_fall_count = 0U;
    debug_measurement_missed_period_count = 0U;
    debug_measurement_fast_period_count = 0U;
    debug_measurement_last_rise_ms = 0U;
    Debug_Log("measurement stats reset\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "ina") == 0)
  {
    Debug_ReportInaStatus();
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "screen") == 0)
  {
    Debug_ReportScreen();
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "screen next") == 0)
  {
    Debug_Log("screen page=%s\r\n", Display_GetPageName(Display_CyclePage()));
    Debug_ReportScreen();
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "screen cal") == 0)
  {
    Display_SetPage(DISPLAY_PAGE_CALIBRATION);
    Debug_ReportScreen();
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "sim") == 0)
  {
    Debug_ReportSimulationStatus();
    Debug_PrintConsolePrompt();
    return;
  }

  {
    unsigned int sim_ambient_deg_c;

    if (sscanf(command, "sim ambient %u", &sim_ambient_deg_c) == 1)
    {
      Heater_Test_SetAmbientTempCdeg((uint16_t)(sim_ambient_deg_c * 10U));
      Heater_Simulation_SetAmbientTempCdeg((uint16_t)(sim_ambient_deg_c * 10U));
      Debug_ReportSimulationStatus();
      Debug_PrintConsolePrompt();
      return;
    }
  }

  {
    unsigned int sim_load_percent;

    if (sscanf(command, "sim load %u", &sim_load_percent) == 1)
    {
      Heater_Simulation_SetThermalLoadPermille((uint16_t)(sim_load_percent * 10U));
      Debug_ReportSimulationStatus();
      Debug_PrintConsolePrompt();
      return;
    }
  }

  {
    unsigned int sim_tip_deg_c;

    if (sscanf(command, "sim tip %u", &sim_tip_deg_c) == 1)
    {
      Heater_Simulation_SetTipTempCdeg((uint16_t)(sim_tip_deg_c * 10U));
      Debug_ReportSimulationStatus();
      Debug_PrintConsolePrompt();
      return;
    }
  }

  if (strcmp(command, "sim reset") == 0)
  {
    Heater_Simulation_Reset();
    Debug_ReportSimulationStatus();
    Debug_PrintConsolePrompt();
    return;
  }

  if ((strcmp(command, "encoder") == 0) || (strcmp(command, "enc") == 0))
  {
    Debug_ReportEncoderStatus();
    Debug_PrintConsolePrompt();
    return;
  }

  if (strcmp(command, "enc live") == 0)
  {
    Debug_Log("enc live=%s period=%lums\r\n",
              debug_encoder_live_enabled ? "on" : "off",
              (unsigned long)debug_encoder_live_period_ms);
    Debug_ReportEncoderLive(HAL_GetTick(), 1U);
    Debug_PrintConsolePrompt();
    return;
  }

  if ((strcmp(command, "enc live on") == 0) || (strcmp(command, "enc liveon") == 0) || (strcmp(command, "encliveon") == 0))
  {
    debug_encoder_live_enabled = 1U;
    debug_last_encoder_live_report_ms = 0U;
    Debug_Log("enc live enabled period=%lums\r\n", (unsigned long)debug_encoder_live_period_ms);
    Debug_PrintConsolePrompt();
    return;
  }

  if ((strcmp(command, "enc live off") == 0) || (strcmp(command, "enc liveoff") == 0) || (strcmp(command, "encliveoff") == 0))
  {
    debug_encoder_live_enabled = 0U;
    Debug_Log("enc live disabled\r\n");
    Debug_PrintConsolePrompt();
    return;
  }

  {
    unsigned int encoder_live_period_ms;

    if (sscanf(command, "enc live %u", &encoder_live_period_ms) == 1)
    {
      if (encoder_live_period_ms < 20U)
      {
        encoder_live_period_ms = 20U;
      }

      debug_encoder_live_period_ms = encoder_live_period_ms;
      debug_encoder_live_enabled = 1U;
      debug_last_encoder_live_report_ms = 0U;
      Debug_Log("enc live enabled period=%ums\r\n", encoder_live_period_ms);
      Debug_PrintConsolePrompt();
      return;
    }
  }

  Debug_Log("unknown command: %s\r\n", command);
  Debug_PrintConsolePrompt();
}

static void Debug_ReportSimulationStatus(void)
{
  const HeaterControlContext *heater = Heater_Control_GetContext();

  Debug_Log("sim: amb=%u.%uC base=%u.%uC tip=%u.%uC ext=%u.%uC set=%u.%uC eff=%u.%uC pwm=%u req=%u load=%u.%u%% pwr=%u.%uW\r\n",
            (unsigned int)(heater->simulated_ambient_temp_cdeg / 10U),
            (unsigned int)(heater->simulated_ambient_temp_cdeg % 10U),
            (unsigned int)(heater->simulated_ambient_base_temp_cdeg / 10U),
            (unsigned int)(heater->simulated_ambient_base_temp_cdeg % 10U),
            (unsigned int)(heater->simulated_tip_temp_cdeg / 10U),
            (unsigned int)(heater->simulated_tip_temp_cdeg % 10U),
            (unsigned int)(heater->simulated_external_tip_temp_cdeg / 10U),
            (unsigned int)(heater->simulated_external_tip_temp_cdeg % 10U),
            (unsigned int)(heater->target_temp_cdeg / 10U),
            (unsigned int)(heater->target_temp_cdeg % 10U),
            (unsigned int)(heater->effective_target_temp_cdeg / 10U),
            (unsigned int)(heater->effective_target_temp_cdeg % 10U),
            (unsigned int)heater->pwm_permille,
            (unsigned int)heater->simulated_requested_pwm_permille,
            (unsigned int)(heater->simulated_thermal_load_permille / 10U),
            (unsigned int)(heater->simulated_thermal_load_permille % 10U),
            (unsigned int)(heater->simulated_power_watt_x10 / 10U),
            (unsigned int)(heater->simulated_power_watt_x10 % 10U));
}

static void Debug_ReportInaStatus(void)
{
  const Ina238Context *ina238 = Ina238_GetContext();

  Debug_Log("ina: init=%u ready=%u err=%u hal=%u alert=%u man=0x%04X dev=0x%04X vbus=%umV current=%umA power=%umW raw_v=%u raw_i=%u raw_p=%u\r\n",
            (unsigned int)ina238->initialized,
            (unsigned int)ina238->data_ready,
            (unsigned int)ina238->last_error,
            (unsigned int)ina238->last_hal_status,
            (unsigned int)ina238->alert_active,
            (unsigned int)ina238->manufacturer_id,
            (unsigned int)ina238->device_id,
            (unsigned int)ina238->bus_voltage_mv,
            (unsigned int)ina238->current_ma,
            (unsigned int)ina238->power_mw,
            (unsigned int)ina238->bus_voltage_raw,
            (unsigned int)ina238->current_raw,
            (unsigned int)ina238->power_raw);
}

static void Debug_ReportScreen(void)
{
  const DisplayContext *display = Display_GetContext();
  const St7789Context *st7789 = St7789_GetContext();

  Debug_Log("screen[%lu] hw=%u err=%u hal=%u bl=%u page=%s: %s | %s | %s | %s\r\n",
            (unsigned long)display->version,
            (unsigned int)st7789->initialized,
            (unsigned int)st7789->last_error,
            (unsigned int)st7789->last_hal_status,
            (unsigned int)st7789->backlight_permille,
            Display_GetPageName((DisplayPage)display->page),
            display->lines[0],
            display->lines[1],
            display->lines[2],
            display->lines[3]);
}

static void Debug_ReportEncoderStatus(void)
{
  GPIO_PinState enc_a_state = HAL_GPIO_ReadPin(ENCODER_A_GPIO_Port, ENCODER_A_Pin);
  GPIO_PinState enc_b_state = HAL_GPIO_ReadPin(ENCODER_B_GPIO_Port, ENCODER_B_Pin);
  TIM_TypeDef *encoder_tim = htim5.Instance;
  uint32_t tim_cnt = encoder_tim->CNT;
  uint32_t tim_cr1 = encoder_tim->CR1;
  uint32_t tim_cen = (tim_cr1 & TIM_CR1_CEN) ? 1U : 0U;
  uint32_t tim_icr1 = encoder_tim->CCR1;
  uint32_t tim_icr2 = encoder_tim->CCR2;
  uint32_t enc_a_raw = ((ENCODER_A_GPIO_Port->IDR & ENCODER_A_Pin) != 0U) ? 1U : 0U;
  uint32_t enc_b_raw = ((ENCODER_B_GPIO_Port->IDR & ENCODER_B_Pin) != 0U) ? 1U : 0U;

  Debug_Log("encoder: a=%u b=%u tim=%08lX cnt=%u run=%u ic1=%u ic2=%u a_raw=%u b_raw=%u\r\n",
            (unsigned int)enc_a_state,
            (unsigned int)enc_b_state,
            (unsigned long)encoder_tim,
            (unsigned int)tim_cnt,
            (unsigned int)tim_cen,
            (unsigned int)tim_icr1,
            (unsigned int)tim_icr2,
            (unsigned int)enc_a_raw,
            (unsigned int)enc_b_raw);
}

static void Debug_ReportEncoderLive(uint32_t now_ms, uint8_t force)
{
  const UiContext *ui = Ui_GetContext();
  TIM_TypeDef *encoder_tim = htim5.Instance;
  uint32_t enc_a_raw = ((ENCODER_A_GPIO_Port->IDR & ENCODER_A_Pin) != 0U) ? 1U : 0U;
  uint32_t enc_b_raw = ((ENCODER_B_GPIO_Port->IDR & ENCODER_B_Pin) != 0U) ? 1U : 0U;
  uint32_t sw_raw = (HAL_GPIO_ReadPin(ENCODER_SW_GPIO_Port, ENCODER_SW_Pin) == ENCODER_SW_ACTIVE_LEVEL) ? 1U : 0U;
  uint32_t cnt = (encoder_tim != NULL) ? encoder_tim->CNT : 0U;
  uint32_t cen = (encoder_tim != NULL) ? ((encoder_tim->CR1 & TIM_CR1_CEN) ? 1U : 0U) : 0U;

  if ((force == 0U) && (debug_encoder_live_enabled == 0U))
  {
    return;
  }

  if ((force == 0U) && (Debug_IsConsoleInputActive() != 0U))
  {
    return;
  }

  if ((force == 0U) && ((now_ms - debug_last_encoder_live_report_ms) < debug_encoder_live_period_ms))
  {
    return;
  }

  debug_last_encoder_live_report_ms = now_ms;

  Debug_Log("enc_live t=%lu a=%u b=%u sw=%u tim_cnt=%lu run=%lu ui_evt=%lu set=%u.%uC screen=%u\r\n",
            (unsigned long)now_ms,
            (unsigned int)enc_a_raw,
            (unsigned int)enc_b_raw,
            (unsigned int)sw_raw,
            (unsigned long)cnt,
            (unsigned long)cen,
            (unsigned long)ui->event_counter,
            (unsigned int)(ui->target_temp_cdeg / 10U),
            (unsigned int)(ui->target_temp_cdeg % 10U),
            (unsigned int)ui->screen);
}

static void Debug_ReportUiEvents(void)
{
  const UiContext *ui = Ui_GetContext();

  if (Debug_IsConsoleInputActive() != 0U)
  {
    return;
  }

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

    case UI_EVENT_CALIBRATION_POINT_CAPTURED:
      Debug_Log("ui calibration point captured: pts=%u\r\n", (unsigned int)ui->event_value);
      break;

    case UI_EVENT_CALIBRATION_FINALIZED:
      Debug_Log("ui calibration finalized: pts=%u\r\n", (unsigned int)ui->event_value);
      break;

    case UI_EVENT_CALIBRATION_FINALIZE_REJECTED:
      Debug_Log("ui calibration finalize rejected: pts=%u\r\n", (unsigned int)ui->event_value);
      break;

    case UI_EVENT_CALIBRATION_REJECTED:
      Debug_Log("ui calibration rejected: ext_ok=%u\r\n", (unsigned int)ui->event_value);
      break;

    case UI_EVENT_STREAM_TOGGLED:
      debug_status_stream_enabled = ui->stream_enabled;
      Debug_Log("ui stream %s\r\n", ui->stream_enabled ? "enabled" : "disabled");
      break;

    case UI_EVENT_DOCK_TOGGLED:
      Debug_Log("ui dock %s\r\n", ui->event_value ? "enabled" : "disabled");
      break;

    case UI_EVENT_SCREEN_CHANGED:
      Debug_Log("ui screen page=%s\r\n", Display_GetPageName((DisplayPage)ui->event_value));
      break;

    case UI_EVENT_FAULT_CLEAR_REQUESTED:
      Debug_Log("ui fault clear requested\r\n");
      break;

    case UI_EVENT_FAULT_ACK_RESULT:
      Debug_Log("ui fault ack %s\r\n", ui->event_value ? "accepted" : "rejected");
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

  if (Debug_IsConsoleInputActive() != 0U)
  {
    return;
  }

  if ((uint32_t)station->state != debug_last_logged_state)
  {
    debug_last_logged_state = (uint32_t)station->state;
    Debug_Log("station state=%s (%lu)\r\n",
              Debug_GetStationStateName(station->state),
              debug_last_logged_state);
  }

  if ((uint32_t)station->operating_mode != debug_last_logged_mode)
  {
    debug_last_logged_mode = (uint32_t)station->operating_mode;
    Debug_Log("station mode=%s (%lu)\r\n",
              Debug_GetStationOperatingModeName(station->operating_mode),
              debug_last_logged_mode);
  }

  if ((uint32_t)station->docked != debug_last_logged_docked)
  {
    debug_last_logged_docked = (uint32_t)station->docked;
    Debug_Log("dock state=%s eff_target=%u.%uC\r\n",
              station->docked ? "DOCKED" : "UNDOCKED",
              (unsigned int)(station->effective_target_temp_cdeg / 10U),
              (unsigned int)(station->effective_target_temp_cdeg % 10U));
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
    uint32_t now_ms = HAL_GetTick();

    debug_last_logged_measurement_ready = heater->measurement_ready;

    if (heater->measurement_ready != 0U)
    {
      uint32_t period_ms = (debug_measurement_last_rise_ms != 0U) ? (now_ms - debug_measurement_last_rise_ms) : 0U;

      debug_measurement_rise_count++;
      if ((debug_measurement_last_rise_ms != 0U) && (period_ms > 35U))
      {
        debug_measurement_missed_period_count++;
      }
      if ((debug_measurement_last_rise_ms != 0U) && (period_ms < 15U))
      {
        debug_measurement_fast_period_count++;
      }
      debug_measurement_last_rise_ms = now_ms;
    }
    else
    {
      debug_measurement_fall_count++;
    }

    if (debug_measurement_log_enabled == 0U)
    {
      return;
    }

    ambient_whole = (long)(heater->ambient_temp_cdeg / 10);
    ambient_frac = (long)(heater->ambient_temp_cdeg >= 0 ? (heater->ambient_temp_cdeg % 10) : -(heater->ambient_temp_cdeg % 10));
    Debug_Log("measurement ready=%u n=%u raw=%u current=%u/%umA vbus=%u/%umV vref=%u tgt_v=%umV tip=%u.%uC tgt=%u.%uC amb=%ld.%ldC ext=%u.%uC cal=%u amb_ok=%u ext_ok=%u pwr=%u.%uW\r\n",
              (unsigned int)heater->measurement_ready,
              (unsigned int)heater->sample_count,
              (unsigned int)heater->filtered_raw,
              (unsigned int)heater->heater_current_raw,
              (unsigned int)heater->heater_current_ma,
              (unsigned int)heater->buckboost_voltage_raw,
              (unsigned int)heater->buckboost_voltage_mv,
              (unsigned int)heater->buckboost_ref_raw,
              (unsigned int)heater->buckboost_target_mv,
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
  const Ina238Context *ina238 = Ina238_GetContext();
  const Mcp9808Context *mcp9808 = Mcp9808_GetContext();
  const Max31856Context *max31856 = Max31856_GetContext();
  char fault_text[96];
  char warning_text[96];

  if ((force == 0U) && (Debug_IsConsoleInputActive() != 0U))
  {
    return;
  }

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

  Debug_Log("status t=%lu state=%s mode=%s dock=%u faults=%s active=0x%08lX warns=%s ack=%u en=%lu clamp=%lu pwm=%lu fan=%u rpm=%u sim_req=%u sim_load=%u\r\n",
            now_ms,
            Debug_GetStationStateName(station->state),
            Debug_GetStationOperatingModeName(station->operating_mode),
            (unsigned int)station->docked,
            fault_text,
            station->active_fault_flags,
            warning_text,
            (unsigned int)station->fault_ack_pending,
            g_debug_heater_enable_level,
            g_debug_tip_clamp_level,
            g_debug_heater_pwm_compare,
            (unsigned int)Fan_GetContext()->pwm_permille,
            (unsigned int)Fan_GetContext()->tach_rpm,
            (unsigned int)heater->simulated_requested_pwm_permille,
            (unsigned int)heater->simulated_thermal_load_permille);
  Debug_Log("status therm raw=%u cur=%u/%umA vbus=%u/%umV vref=%u tgt_v=%umV tip=%u.%uC set=%u.%uC eff=%u.%uC amb=%u.%uC ext=%u.%uC pwr=%u.%uW n=%u ready=%u m_valid=%u m_age=%u m_err=0x%04X\r\n",
            (unsigned int)heater->filtered_raw,
            (unsigned int)heater->heater_current_raw,
            (unsigned int)heater->heater_current_ma,
            (unsigned int)heater->buckboost_voltage_raw,
            (unsigned int)heater->buckboost_voltage_mv,
            (unsigned int)heater->buckboost_ref_raw,
            (unsigned int)heater->buckboost_target_mv,
            (unsigned int)(heater->tip_temp_cdeg / 10U),
            (unsigned int)(heater->tip_temp_cdeg % 10U),
            (unsigned int)(heater->target_temp_cdeg / 10U),
            (unsigned int)(heater->target_temp_cdeg % 10U),
            (unsigned int)(heater->effective_target_temp_cdeg / 10U),
            (unsigned int)(heater->effective_target_temp_cdeg % 10U),
            (unsigned int)(heater->ambient_temp_cdeg / 10),
            (unsigned int)((heater->ambient_temp_cdeg >= 0) ? (heater->ambient_temp_cdeg % 10) : -(heater->ambient_temp_cdeg % 10)),
            (unsigned int)(heater->external_tip_temp_cdeg / 10U),
            (unsigned int)(heater->external_tip_temp_cdeg % 10U),
            (unsigned int)(heater->power_watt_x10 / 10U),
            (unsigned int)(heater->power_watt_x10 % 10U),
            (unsigned int)heater->sample_count,
            (unsigned int)heater->measurement_ready,
            (unsigned int)heater->measurement.valid,
            (unsigned int)heater->measurement.age_ms,
            (unsigned int)heater->measurement.error_flags);
  Debug_Log("status sens cal=%u amb_ok=%u ext_ok=%u ina_init=%u ina_err=%u ina_alert=%u ina_pwr=%umW mcp_init=%u mcp_err=%u mcp_hal=%u max_init=%u max_err=%u max_hal=%u cal_mode=%u cal_target=%u.%uC cal_stable=%u hold=%lums pts=%u\r\n",
            (unsigned int)heater->calibration_valid,
            (unsigned int)heater->ambient_sensor_ready,
            (unsigned int)heater->external_sensor_ready,
            (unsigned int)ina238->initialized,
            (unsigned int)ina238->last_error,
            (unsigned int)ina238->alert_active,
            (unsigned int)ina238->power_mw,
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
            (unsigned int)session->stored_point_count);
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

    case STATION_STATE_STANDBY:
      return "STBY";

    case STATION_STATE_FAULT:
      return "FAULT";

    default:
      return "UNKNOWN";
  }
}

static const char *Debug_GetStationOperatingModeName(StationOperatingMode mode)
{
  switch (mode)
  {
    case STATION_OPERATING_MODE_IDLE:
      return "IDLE";

    case STATION_OPERATING_MODE_HEATUP:
      return "HEATUP";

    case STATION_OPERATING_MODE_HOLD:
      return "HOLD";

    case STATION_OPERATING_MODE_COOLDOWN:
      return "COOLDOWN";

    case STATION_OPERATING_MODE_STANDBY:
      return "STANDBY";

    case STATION_OPERATING_MODE_CALIBRATION:
      return "CAL";

    case STATION_OPERATING_MODE_FAULT:
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

  if ((warning_flags & STATION_WARNING_INA238_ERROR) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sINA238_ERROR", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_INA238_ALERT) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sINA238_ALERT", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_BUCKBOOST_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_BUCKBOOST_IGN", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_OPAMP_PGOOD_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_OPAMP_IGN", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_TIP_OVERTEMP_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_TIP_OT_IGN", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_AMBIENT_OVERTEMP_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_AMB_OT_IGN", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_AMBIENT_SENSOR_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_AMB_SENSOR_IGN", (written > 0U) ? "|" : "");
    if (append_length > 0)
    {
      written += (size_t)append_length;
      if (written >= buffer_size)
      {
        written = buffer_size - 1U;
      }
    }
  }

  if ((warning_flags & STATION_WARNING_FAULT_INJECTED_IGNORED) != 0U)
  {
    append_length = snprintf(buffer + written, buffer_size - written, "%sFAULT_INJECTED_IGN", (written > 0U) ? "|" : "");
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

void Debug_Console_Init(void)
{
  debug_last_button_level = HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);
}

void Debug_Console_Tick(uint32_t now_ms)
{
  Debug_UpdateHeartbeat(now_ms);
  Debug_HandleFaultButton();
  Debug_PollConsole();
  Debug_ReportUiEvents();
  Debug_ReportChanges();
  Debug_ReportPeriodicStatus(now_ms, 0U);
  Debug_ReportEncoderLive(now_ms, 0U);
}
