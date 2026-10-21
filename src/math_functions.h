/*
 * TM5000 trace math: FFT, calculus, dual-trace ops, statistics, filters, curve fits, correlation.
 */

#ifndef MATH_FUNCTIONS_H
#define MATH_FUNCTIONS_H

#include "tm5000.h"

/* Dual-Trace Operation Types */
#define TRACE_OP_ADD      0
#define TRACE_OP_SUBTRACT 1
#define TRACE_OP_MULTIPLY 2
#define TRACE_OP_DIVIDE   3
#define TRACE_OP_AVERAGE  4
#define TRACE_OP_MIN      5
#define TRACE_OP_MAX      6
#define TRACE_OP_DIFF     7

typedef struct {
    float update_rate;
    int window_size;                  /* rolling statistics (10-1000) */
    int histogram_bins;               /* 10-100 */
    unsigned char enable_mean:1;
    unsigned char enable_rms:1;
    unsigned char enable_std_dev:1;
    unsigned char enable_min_max:1;
    unsigned char enable_peak_detect:1;
    unsigned char enable_frequency:1;
    unsigned char rolling_stats:1;
    unsigned char enable_histogram:1;
} statistics_config;

/* Digital Filter Types */
#define FILTER_TYPE_LOWPASS   0
#define FILTER_TYPE_HIGHPASS  1
#define FILTER_TYPE_BANDPASS  2
#define FILTER_TYPE_BANDSTOP  3
#define FILTER_TYPE_MOVING_AVG 4

#pragma pack(1)
typedef struct {
    float cutoff_freq;                /* Hz */
    float bandwidth;                  /* bandpass/bandstop only */
    float sample_rate;                /* Hz */
    float gain;
    int filter_type;                  /* FILTER_TYPE_* */
    int order;                        /* 1-4 */
    int window_size;                  /* moving average only */
} filter_config;
#pragma pack()

/* Curve Fitting Types */
#define FIT_TYPE_LINEAR        0
#define FIT_TYPE_POLYNOMIAL    1
#define FIT_TYPE_EXPONENTIAL   2
#define FIT_TYPE_LOGARITHMIC   3
#define FIT_TYPE_POWER         4

#pragma pack(1)
typedef struct {
    float coefficients[6];
    float correlation;                /* R-squared */
    float rms_error;
    int fit_type;                     /* FIT_TYPE_* */
    int points_used;
    unsigned char equation_index;     /* see get_equation_text() */
} curve_fit_result;
#pragma pack()

#pragma pack(1)
typedef struct {
    time_t calculation_time;
    float mean;
    float rms;
    float std_dev;
    float min_value;
    float max_value;
    float peak_to_peak;
    float median;
    float mode;
    int sample_count;
} statistics_result;
#pragma pack()

#pragma pack(1)
typedef struct {
    float correlation_coefficient;    /* Pearson (-1 to +1) */
    float covariance;
    float phase_shift;               /* samples */
    float time_delay;                /* s */
    int lag_samples;                 /* lag of maximum correlation */
} correlation_result;
#pragma pack()

extern statistics_config g_stats_config;

/* FFT and calculus (math_functions.c) */
void perform_fft(void);
int fft_configuration_menu(void);
void execute_fft_with_config(void);
void generate_window_function(float far *window, int N, int window_type);
void perform_differentiation(void);  
void perform_integration(void);
void perform_smoothing(void);

/* Dual-trace and analysis math (math_enhanced.c) */

/* Dual-Trace Operations */
int perform_dual_trace_operation(int trace1, int trace2, int operation, int result_slot);
int dual_trace_add(int trace1, int trace2, int result_slot);
int dual_trace_subtract(int trace1, int trace2, int result_slot);
int dual_trace_multiply(int trace1, int trace2, int result_slot);
int dual_trace_divide(int trace1, int trace2, int result_slot);
int dual_trace_average(int trace1, int trace2, int result_slot);
int dual_trace_min(int trace1, int trace2, int result_slot);
int dual_trace_max(int trace1, int trace2, int result_slot);

/* Real-time Statistics */
int calculate_realtime_statistics(int trace_slot, statistics_config *config, statistics_result *result);
int calculate_basic_statistics(float *data, int count, statistics_result *result);
int calculate_rolling_statistics(int trace_slot, int window_size);
int get_statistics_result(int trace_slot, statistics_result *result);

/* Distribution and peak statistics */
int calculate_histogram(float *data, int count, float *bins, int bin_count);
int calculate_median(float *data, int count, float *result);
int calculate_mode(float *data, int count, float *result);
int find_peaks(float *data, int count, int *peak_indices, int max_peaks);
int calculate_frequency_analysis(float *data, int count, float sample_rate, float *result);

/* Digital Filtering */
int apply_digital_filter(int trace_slot, filter_config *config);
int design_lowpass_filter(filter_config *config, float *coefficients);
int design_highpass_filter(filter_config *config, float *coefficients);
int design_bandpass_filter(filter_config *config, float *coefficients);
int apply_moving_average_filter(float *data, int count, int window_size);
int apply_iir_filter(float *data, int count, float *coefficients, int order);

/* Curve Fitting */
int perform_curve_fitting(int trace_slot, int fit_type, curve_fit_result *result);
int fit_linear_regression(float *x_data, float *y_data, int count, curve_fit_result *result);
int fit_polynomial(float *x_data, float *y_data, int count, int order, curve_fit_result *result);
int fit_exponential(float *x_data, float *y_data, int count, curve_fit_result *result);
int calculate_fit_quality(float *y_data, float *y_fitted, int count, curve_fit_result *result);

/* Correlation Analysis */
int calculate_correlation(int trace1, int trace2, correlation_result *result);
int calculate_cross_correlation(float *data1, float *data2, int count, float *correlation);
int find_best_lag(float *correlation, int count, int *best_lag);
int calculate_phase_shift(int trace1, int trace2, float sample_rate);

/* Signal Processing Utilities */
int remove_dc_offset(float *data, int count);
int normalize_signal(float *data, int count, float target_range);
int apply_window_function(float *data, int count, int window_type);
int interpolate_missing_data(float *data, int count, int *missing_indices, int missing_count);
int decimate_signal(float *input, int input_count, float *output, int factor);

/* Measurement Analysis */
int analyze_signal_quality(float *data, int count, float sample_rate);
int detect_signal_anomalies(float *data, int count, int *anomaly_indices, int max_anomalies);
int calculate_snr(float *signal, int signal_count, float *noise, int noise_count);
int measure_rise_time(float *data, int count, float sample_rate);
int measure_fall_time(float *data, int count, float sample_rate);

/* Menu and User Interface Functions */
void dual_trace_operations_menu(void);
void statistics_configuration_menu(void);
void digital_filter_menu(void);
void curve_fitting_menu(void);
void correlation_analysis_menu(void);
void advanced_analysis_menu(void);

/* Configuration and Settings */
int save_math_settings(void);
int load_math_settings(void);
int reset_math_settings(void);

void cleanup_math_buffers(void);

const char* get_equation_text(unsigned char equation_index);

/* Validation */
int validate_trace_data(int trace_slot);
int validate_statistics_config(statistics_config *config);
int validate_filter_config(filter_config *config);

/* Math Function Error Codes */
#define MATH_SUCCESS                0
#define MATH_ERROR_INVALID_TRACE    1
#define MATH_ERROR_NO_DATA          2
#define MATH_ERROR_MEMORY           3
#define MATH_ERROR_INVALID_CONFIG   4
#define MATH_ERROR_DIVISION_BY_ZERO 5
#define MATH_ERROR_INVALID_PARAMS   6
#define MATH_ERROR_CONVERGENCE      7
#define MATH_ERROR_BUFFER_OVERFLOW  8

#endif /* MATH_FUNCTIONS_H */
