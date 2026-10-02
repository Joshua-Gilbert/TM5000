/*
 * TM5000 GPIB Control System - Mathematical Functions Header
 * Version 3.5
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Version update
 * 3.3 - Version update
 * 3.4 - Fixed FFT peak centering and added UNIT_POWER support
 * 3.5 - Enhanced math with dual-trace operations, statistics, and filtering
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

/* Statistics Configuration - optimized member ordering (largest first) */
typedef struct {
    float update_rate;                /* 4 bytes - largest member first */
    int window_size;                  /* 4 bytes - For rolling statistics (10-1000) */
    int histogram_bins;               /* 4 bytes - Number of histogram bins (10-100) */
    unsigned char enable_mean:1;      /* Bit fields - pack all flags into 1 byte */
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

/* Digital Filter Configuration - optimized member ordering (largest first) */
#pragma pack(1)
typedef struct {
    float cutoff_freq;                /* 4 bytes - Cutoff frequency (Hz) */
    float bandwidth;                  /* 4 bytes - For bandpass/bandstop filters */
    float sample_rate;                /* 4 bytes - Sampling rate (Hz) */
    float gain;                       /* 4 bytes - Filter gain */
    int filter_type;                  /* 4 bytes - Filter type constant */
    int order;                        /* 4 bytes - Filter order (1-4) */
    int window_size;                  /* 4 bytes - For moving average filter */
} filter_config;
#pragma pack()

/* Curve Fitting Types */
#define FIT_TYPE_LINEAR        0
#define FIT_TYPE_POLYNOMIAL    1
#define FIT_TYPE_EXPONENTIAL   2
#define FIT_TYPE_LOGARITHMIC   3
#define FIT_TYPE_POWER         4

/* Curve Fitting Results - optimized member ordering (largest first) */
#pragma pack(1)
typedef struct {
    float coefficients[6];            /* 24 bytes - largest member first */
    float correlation;                /* 4 bytes - R-squared value */
    float rms_error;                  /* 4 bytes - RMS error of fit */
    int fit_type;                     /* 4 bytes */
    int points_used;                  /* 4 bytes - Number of data points used */
    unsigned char equation_index;     /* 1 byte - Index into equation text table */
} curve_fit_result;
#pragma pack()

/* Statistics Results - optimized member ordering (largest first) */
#pragma pack(1)
typedef struct {
    time_t calculation_time;         /* 4 bytes - largest first */
    float mean;                      /* 4 bytes */
    float rms;                       /* 4 bytes */
    float std_dev;                   /* 4 bytes */
    float min_value;                 /* 4 bytes */
    float max_value;                 /* 4 bytes */
    float peak_to_peak;              /* 4 bytes */
    float median;                    /* 4 bytes */
    float mode;                      /* 4 bytes */
    int sample_count;                /* 4 bytes - int after floats */
} statistics_result;
#pragma pack()

/* Correlation Analysis Results */
#pragma pack(1)
typedef struct {
    float correlation_coefficient;    /* Pearson correlation (-1 to +1) */
    float covariance;                /* Covariance between traces */
    float phase_shift;               /* Phase shift in samples */
    float time_delay;                /* Time delay in seconds */
    int lag_samples;                 /* Lag for maximum correlation */
} correlation_result;
#pragma pack()

/* Global statistics configuration */
extern statistics_config g_stats_config;

/* Original mathematical function prototypes */
void perform_fft(void);
int fft_configuration_menu(void);
void execute_fft_with_config(void);
void generate_window_function(float far *window, int N, int window_type);
void perform_differentiation(void);  
void perform_integration(void);
void perform_smoothing(void);

/* Enhanced Math Functions (v3.5) */

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
/* Removed redundant rolling statistics function */
int get_statistics_result(int trace_slot, statistics_result *result);

/* Advanced Statistics */
int calculate_histogram(float *data, int count, float *bins, int bin_count);
int calculate_median(float *data, int count);
int calculate_mode(float *data, int count);
int find_peaks(float *data, int count, int *peak_indices, int max_peaks);
int calculate_frequency_analysis(float *data, int count, float sample_rate);

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

/* Memory Management for Rolling Statistics */
void cleanup_math_buffers(void);

/* Equation text retrieval */
const char* get_equation_text(unsigned char equation_index);

/* Validation and Error Handling */
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
