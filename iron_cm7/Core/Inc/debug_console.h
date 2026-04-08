#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdint.h>

void Debug_Console_Init(void);
void Debug_Console_Tick(uint32_t now_ms);

extern volatile uint32_t g_debug_heartbeat;
extern volatile uint32_t g_debug_loop_counter;
extern volatile uint32_t g_debug_station_state;
extern volatile uint32_t g_debug_station_mode;
extern volatile uint32_t g_debug_station_docked;
extern volatile uint32_t g_debug_fault_flags;
extern volatile uint32_t g_debug_active_fault_flags;
extern volatile uint32_t g_debug_warning_flags;
extern volatile uint32_t g_debug_heater_measurement_ready;
extern volatile uint32_t g_debug_heater_filtered_raw;
extern volatile uint32_t g_debug_heater_current_raw;
extern volatile uint32_t g_debug_heater_current_ma;
extern volatile uint32_t g_debug_buckboost_voltage_raw;
extern volatile uint32_t g_debug_buckboost_voltage_mv;
extern volatile uint32_t g_debug_buckboost_ref_raw;
extern volatile uint32_t g_debug_buckboost_target_mv;
extern volatile uint32_t g_debug_heater_sample_count;
extern volatile uint32_t g_debug_heater_enable_level;
extern volatile uint32_t g_debug_tip_clamp_level;
extern volatile uint32_t g_debug_heater_pwm_compare;
extern volatile uint32_t g_debug_tip_temp_cdeg;
extern volatile uint32_t g_debug_target_temp_cdeg;
extern volatile uint32_t g_debug_effective_target_temp_cdeg;
extern volatile int32_t g_debug_ambient_temp_cdeg;
extern volatile uint32_t g_debug_power_watt_x10;
extern volatile uint32_t g_debug_external_tip_temp_cdeg;
extern volatile int32_t g_debug_external_ambient_temp_cdeg;
extern volatile uint32_t g_debug_calibration_valid;
extern volatile uint32_t g_debug_ambient_sensor_ready;
extern volatile uint32_t g_debug_external_sensor_ready;
extern volatile uint32_t g_debug_sim_tip_temp_cdeg;
extern volatile uint32_t g_debug_sim_target_temp_cdeg;
extern volatile uint32_t g_debug_sim_power_watt_x10;
extern volatile uint32_t g_debug_ui_screen;
extern volatile uint32_t g_debug_ui_menu_item;
extern volatile uint32_t g_debug_ui_save_pending;
extern volatile uint32_t g_debug_fan_pwm_permille;
extern volatile uint32_t g_debug_fan_tach_rpm;
extern volatile uint32_t g_debug_display_version;

#endif
