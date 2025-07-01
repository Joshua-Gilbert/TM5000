/*
 * TM5000 GPIB Control System - Module Support
 * Version 3.1
 * Header file for instrument module functions
 */

#ifndef MODULES_H
#define MODULES_H

#include "tm5000.h"

/* Module configuration functions */
void configure_modules(void);
void module_selection_menu(void);
void display_trace_selection_menu(void);
void sync_traces_with_modules(void);

/* DM5120 functions */
void init_dm5120_config(int slot);
void init_dm5120_config_enhanced(int slot);
void configure_dm5120_advanced(int slot);
void dm5120_set_function(int address, char *function);
void dm5120_set_range(int address, int range);
void dm5120_set_filter(int address, int enabled, int value);
void dm5120_set_trigger(int address, char *mode);
void dm5120_set_digits(int address, int digits);
void dm5120_set_null(int address, int enabled, float value);
void dm5120_set_data_format(int address, int on);
void dm5120_enable_buffering(int address, int buffer_size);
void dm5120_start_buffer_sequence(int address);
int dm5120_get_buffer_data(int address, float far *buffer, int max_samples);
float read_dm5120_buffered(int address, int slot);
float read_dm5120_enhanced(int address, int slot);
float read_dm5120(int address);
float read_dm5120_voltage(int address);
void dm5120_clear_statistics(int slot);
void test_dm5120_comm(int address);
void test_dm5120_comm_debug(int address);

/* DM5010 functions */
void init_dm5010_config(int slot);
void configure_dm5010_advanced(int slot);
void dm5010_set_function(int address, char *function);
void dm5010_set_range(int address, char *function, float range);
void dm5010_set_filter(int address, int enabled, int count);
void dm5010_set_trigger(int address, char *mode);
void dm5010_set_autozero(int address, int enabled);
void dm5010_set_null(int address, int enabled, float value);
void dm5010_set_calculation(int address, int mode, float factor, float offset);
void dm5010_beeper(int address, int enabled);
void dm5010_lock_front_panel(int address, int locked);
float read_dm5010_enhanced(int address, int slot);
void test_dm5010_comm(int address);

/* PS5004 functions */
void init_ps5004_config(int slot);
void configure_ps5004_advanced(int slot);
void ps5004_init(int address);
void ps5004_set_voltage(int address, float voltage);
void ps5004_set_current(int address, float current);
void ps5004_set_output(int address, int on);
void ps5004_set_display(int address, char *mode);
int ps5004_get_regulation_status(int address);
float ps5004_read_value(int address);
void test_ps5004_comm(int address);

/* PS5010 functions */
void init_ps5010_config(int slot);
void configure_ps5010_advanced(int slot);
void ps5010_init(int address);
void ps5010_set_voltage(int address, int channel, float voltage);
void ps5010_set_current(int address, int channel, float current);
void ps5010_set_output(int address, int channel, int on);
void ps5010_set_tracking_voltage(int address, float voltage);
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat);
int ps5010_get_settings(int address, char *buffer, int maxlen);
int ps5010_get_error(int address);
void ps5010_set_interrupts(int address, int pri_on, int nri_on, int lri_on);
void ps5010_set_srq(int address, int on);
float read_ps5010(int address, int slot);
void test_ps5010_comm(int address);

/* Measurement functions */
/* DC5009 functions */
void init_dc5009_config(int slot);
void configure_dc5009_advanced(int slot);
void dc5009_set_function(int address, char *function, char *channel);
void dc5009_set_coupling(int address, char channel, char *coupling);
void dc5009_set_impedance(int address, char channel, char *impedance);
void dc5009_set_attenuation(int address, char channel, char *attenuation);
void dc5009_set_slope(int address, char channel, char *slope);
void dc5009_set_level(int address, char channel, float level);
void dc5009_set_filter(int address, int enabled);
void dc5009_set_gate_time(int address, float gate_time);
void dc5009_set_averaging(int address, int count);
void dc5009_auto_trigger(int address);
void dc5009_start_measurement(int address);
void dc5009_stop_measurement(int address);
float dc5009_read_measurement(int address);
int dc5009_check_overflow(int address);
void dc5009_clear_overflow(int address);
void dc5009_query_function(int address, char *buffer);
void dc5009_query_id(int address, char *buffer);
int dc5009_query_error(int address);
void dc5009_set_preset(int address, int enabled);
void dc5009_manual_timing(int address);
void dc5009_set_srq(int address, int enabled);
unsigned char dc5009_get_status_byte(int address);
double dc5009_read_extended_range(int address);
void test_dc5009_comm(int address);

/* DC5010 functions */
void init_dc5010_config(int slot);
void configure_dc5010_advanced(int slot);
void dc5010_set_function(int address, char *function, char *channel);
void dc5010_set_coupling(int address, char channel, char *coupling);
void dc5010_set_impedance(int address, char channel, char *impedance);
void dc5010_set_attenuation(int address, char channel, char *attenuation);
void dc5010_set_slope(int address, char channel, char *slope);
void dc5010_set_level(int address, char channel, float level);
void dc5010_set_filter(int address, int enabled);
void dc5010_set_gate_time(int address, float gate_time);
void dc5010_set_averaging(int address, int count);
void dc5010_auto_trigger(int address);
void dc5010_start_measurement(int address);
void dc5010_stop_measurement(int address);
float dc5010_read_measurement(int address);
int dc5010_check_overflow(int address);
void dc5010_clear_overflow(int address);
void dc5010_set_burst_mode(int address, int enabled);
void dc5010_measure_rise_time(int address);
void dc5010_measure_fall_time(int address);
void dc5010_query_function(int address, char *buffer);
void dc5010_query_id(int address, char *buffer);
int dc5010_query_error(int address);
void dc5010_set_preset(int address, int enabled);
void dc5010_manual_timing(int address);
void dc5010_totalize_sum(int address);
void dc5010_totalize_diff(int address);
void dc5010_propagation_delay(int address);
void dc5010_set_srq(int address, int enabled);
unsigned char dc5010_get_status_byte(int address);
double dc5010_read_extended_range(int address);
void test_dc5010_comm(int address);

/* Measurement functions */
void single_measurement(void);
void continuous_monitor(void);
void continuous_monitor_enhanced(void);
void module_functions_menu(void);

#endif /* MODULES_H */
