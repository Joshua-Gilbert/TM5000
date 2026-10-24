/*
 * TM5000 instrument modules: slot configuration and per-instrument drivers.
 */

#ifndef MODULES_H
#define MODULES_H

#include "tm5000.h"

/* Module configuration functions */
void configure_modules(void);
void auto_detect_modules(void);
void validate_enabled_modules(void);
int module_is_result(int slot);   /* is_result set, or MOD_NONE slot that holds data */
void display_trace_selection_menu(void);
void sync_traces_with_modules(void);
void forget_trace_slot(int slot);

/* DM5120 functions */
void init_dm5120_config(int slot);
void init_dm5120_config_enhanced(int slot);
void configure_dm5120_advanced(int slot);
void dm5120_fix_legacy_config(int slot);
void dm5120_set_function(int address, char *function);
void dm5120_set_range(int address, int range);
void dm5120_set_filter(int address, int enabled, int value);
void dm5120_set_trigger(int address, char *source, char *mode);
void dm5120_set_digits(int address, int digits);
void dm5120_set_null(int address, int enabled, float value);
void dm5120_set_data_format(int address, int on);
void dm5120_set_autocal(int address, int on);
void dm5120_set_intfilt(int address, int on);
int dm5120_calculate_delay(int slot, int operation_type, int sample_count);
int dm5120_parse_value(const char *reply, float *value);
const char *dm5120_error_text(int code);
int dm5120_query_error(int address);
const char *dm5120_unit(int slot);

void dm5120_enable_buffering(int address, int slot, int buffer_size);

/* DM5120 buffer queries */
float dm5120_get_buffer_average(int address);
int dm5120_get_buffer_count(int address);
float dm5120_get_buffer_min(int address);
float dm5120_get_buffer_max(int address);
void dm5120_set_buffer_size(int address, int size);
int dm5120_query_buffer_size(int address);
/* DM5120 timing */
float dm5120_get_measurement_rate(int slot, int trigger_mode);
int dm5120_calculate_measurement_time(int slot, int operation_type, int sample_count);
#define DM5120_STOINT_BAD_SLOT (-6)
#define DM5120_STOINT_TOO_SHORT (-7)
int dm5120_validate_stoint_settings(int slot, int interval_ms);
int dm5120_min_stoint_ms(int slot);
int dm5120_stoint_for_slot(int slot);
int dm5120_set_storage_interval(int address, int slot, int interval_ms);

/* DM5120 SRQ events */
void dm5120_configure_srq_events(int address, int enable_full, int enable_half, int enable_rdy, int enable_opc);
void dm5120_configure_srq_events_smart(int address, int buffer_size, int is_circular, int expected_time_ms);
int dm5120_check_buffer_status(int address);
int dm5120_get_buffer_data_enhanced(int address, int slot, float far *buffer, int max_samples);

/* DM5120 validation, diagnostics, and readout */
int dm5120_validate_configuration(int slot);
void dm5120_timing_diagnostics(int slot);
int dm5120_query_storage_interval(int address);
int dm5120_get_buffer_data(int address, int slot, float far *buffer, int max_samples);
int dm5120_fill_buffer_complete(int address, int slot, float far *buffer, int buffer_size);
int dm5120_get_multiple_samples(int address, int slot, float far *buffer, int num_samples);
float read_dm5120_buffered(int address, int slot);
float read_dm5120_enhanced(int address, int slot);
float read_dm5120_voltage(int address);
void dm5120_clear_statistics(int slot);
void test_dm5120_comm(int address);
void test_dm5120_comm_debug(int address);

/* DM5120 async buffer fill */
void dm5120_start_buffer_async(int address, int slot);
int dm5120_check_buffer_async(int address, int slot);
void dm5120_reset_buffer_async(int slot);

/* DM5010 functions */
void init_dm5010_config(int slot);
void configure_dm5010_advanced(int slot);
void dm5010_fix_legacy_config(int slot);
#define DM5010_READ_NONE 0      /* no reply or no number */
#define DM5010_READ_OK   1
#define DM5010_READ_OVER 2      /* +-1.E+99 */
int dm5010_read(int address, int slot, double *value);
const char *dm5010_unit(int slot);
void dm5010_format(int slot, int r, double v, char *out);
void test_dm5010_comm(int address);

/* PS5004 functions */
#define PS5004_NFLAGS 7     /* ps5004_flag(): OUT VRI CRI URI DT USER RQS */
#define PS5004_OUT    0
#define PS5004_DT     4
#define PS5004_RQS    6
void init_ps5004_config(int slot);
void configure_ps5004_advanced(int slot);
int ps5004_flag(const ps5004_config *cfg, int i);
void ps5004_set_flag(ps5004_config *cfg, int i, int on);
void ps5004_send_flag(int address, int i, int on);
float ps5004_round_voltage(float v);
float ps5004_round_current(float a);
int ps5004_query_block(int address, const char *query, char **reply);
char *ps5004_query(int address, const char *query);
int ps5004_parse_number(const char *s, float *value);
int ps5004_read_value(int address, float *value);
void ps5004_select_display(int slot, int mode);
void ps5004_forget_display(int slot);
int ps5004_display_shown(int address);
int ps5004_measure(int slot, int mode, float *value);
void ps5004_init(int address);
void ps5004_set_voltage(int address, float voltage);
void ps5004_set_current(int address, float current);
void ps5004_set_output(int address, int on);
int ps5004_get_regulation_status(int address);
const char *ps5004_regulation_name(int status);
int ps5004_show_errors(int address, int print);
void ps5004_release_dt(int address);
void ps5004_clear_srq(int address);
void ps5004_trigger(int address);
int ps5004_parse_settings(char *s, ps5004_config *cfg);
int ps5004_compare(const ps5004_config *now, const ps5004_config *want);
void ps5004_print_items(const char *s);
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
void test_ps5010_comm(int address);

/* DC5009 / DC5010 counters: one driver for both models */
void init_dc5009_config(int slot);
void init_dc5010_config(int slot);
void configure_dc5009_advanced(int slot);
void configure_dc5010_advanced(int slot);
void dc50xx_test(int slot);
int dc50xx_read(int slot, double *value);       /* 1 reading, 0 none ready, -1 no reply */
int dc50xx_single(int slot, int phase, double *value);
void dc50xx_sync_function(int slot);
void dc50xx_format(char *buf, int slot, double value);   /* buf >= 20 */
int dc50xx_trace_unit(int slot);

/* FG5010 functions */
void init_fg5010_config(int slot);
void fg5010_fix_config(int slot);
void configure_fg5010_advanced(int slot);
int test_fg5010_comm(int address);  /* 0 = pass, -1 = fail, 1 = declined (not run) */
int fg5010_state_text(int address, char *out);  /* monitor line; no printf */

/* Measurement functions */
void single_measurement(void);
void continuous_monitor(void);
void continuous_monitor_enhanced(void);
void module_functions_menu(void);

#endif /* MODULES_H */
