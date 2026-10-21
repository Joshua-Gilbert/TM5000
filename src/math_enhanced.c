/*
 * Trace math library: dual-trace arithmetic, statistics, first-order filters,
 * curve fitting, and correlation. Most functions return MATH_* status codes.
 */

#include "math_functions.h"
#include "modules.h"
#include <dos.h>
#include <malloc.h>

/* Borland-style far heap prototypes; unused (code uses malloc / _fmalloc) */
void far * farmalloc(unsigned long size);
void farfree(void far *ptr);

statistics_config g_stats_config = {10.0, 100, 20, 1, 1, 1, 1, 0, 0, 0, 0};

#define ROLLING_BUFFER_SIZE 1000    /* max rolling-stats window, samples */
#define MAX_TRACES 10

/* Indexed by curve_fit_result.equation_index; the last entry is the fallback. */
static const char* __far equation_texts[] = {
    "Linear: y = a + bx",           /* 0 */
    "Quadratic: y = a + bx + cx²",  /* 1 */
    "Cubic: y = a + bx + cx² + dx³", /* 2 */
    "Exponential: y = ae^(bx)",     /* 3 */
    "Power: y = ax^b",              /* 4 */
    "Logarithmic: y = a + b*ln(x)", /* 5 */
    "Unknown equation type"         /* 6 - fallback */
};

const char* get_equation_text(unsigned char equation_index) {
    if (equation_index >= sizeof(equation_texts) / sizeof(equation_texts[0])) {
        return equation_texts[6];
    }
    return equation_texts[equation_index];
}

/* result_slot = trace1 <operation> trace2, element-wise over the shorter trace. */
int perform_dual_trace_operation(int trace1, int trace2, int operation, int result_slot) {
    int i, count;
    float value1, value2, result_value;

    if (trace1 < 0 || trace1 >= 10 || trace2 < 0 || trace2 >= 10 || 
        result_slot < 0 || result_slot >= 10) {
        return MATH_ERROR_INVALID_TRACE;
    }
    
    if (validate_trace_data(trace1) != MATH_SUCCESS ||
        validate_trace_data(trace2) != MATH_SUCCESS) {
        return MATH_ERROR_NO_DATA;
    }

    count = (g_system->modules[trace1].module_data_count < g_system->modules[trace2].module_data_count) ?
            g_system->modules[trace1].module_data_count : g_system->modules[trace2].module_data_count;
    
    if (count == 0) {
        return MATH_ERROR_NO_DATA;
    }
    
    /* Reuse a big-enough buffer: reallocating would free a source when result_slot is one */
    if (!g_system->modules[result_slot].module_data ||
        g_system->modules[result_slot].module_data_size < (unsigned int)count) {
        if (allocate_module_buffer(result_slot, count) == 0) {
            return MATH_ERROR_MEMORY;
        }
    }

    for (i = 0; i < count; i++) {
        value1 = g_system->modules[trace1].module_data[i];
        value2 = g_system->modules[trace2].module_data[i];
        
        switch (operation) {
            case TRACE_OP_ADD:
                result_value = value1 + value2;
                break;
            case TRACE_OP_SUBTRACT:
                result_value = value1 - value2;
                break;
            case TRACE_OP_MULTIPLY:
                result_value = value1 * value2;
                break;
            case TRACE_OP_DIVIDE:
                if (fabs(value2) < 1e-10) {
                    result_value = (value1 >= 0) ? 1e10 : -1e10; /* saturate rather than fail the whole trace */
                } else {
                    result_value = value1 / value2;
                }
                break;
            case TRACE_OP_AVERAGE:
                result_value = (value1 + value2) / 2.0;
                break;
            case TRACE_OP_MIN:
                result_value = (value1 < value2) ? value1 : value2;
                break;
            case TRACE_OP_MAX:
                result_value = (value1 > value2) ? value1 : value2;
                break;
            case TRACE_OP_DIFF:
                result_value = fabs(value1 - value2);
                break;
            default:
                return MATH_ERROR_INVALID_PARAMS;
        }
        
        g_system->modules[result_slot].module_data[i] = result_value;
    }
    
    g_system->modules[result_slot].module_data_count = count;
    g_system->modules[result_slot].enabled = 1;
    g_system->modules[result_slot].module_type = MOD_NONE;
    g_system->modules[result_slot].is_result = 1;  /* computed trace - exempt from phantom cleanup */
    
    switch (operation) {
        case TRACE_OP_ADD:
            sprintf(g_system->modules[result_slot].description, "S%d + S%d", trace1, trace2);
            break;
        case TRACE_OP_SUBTRACT:
            sprintf(g_system->modules[result_slot].description, "S%d - S%d", trace1, trace2);
            break;
        case TRACE_OP_MULTIPLY:
            sprintf(g_system->modules[result_slot].description, "S%d * S%d", trace1, trace2);
            break;
        case TRACE_OP_DIVIDE:
            sprintf(g_system->modules[result_slot].description, "S%d / S%d", trace1, trace2);
            break;
        case TRACE_OP_AVERAGE:
            sprintf(g_system->modules[result_slot].description, "AVG(S%d,S%d)", trace1, trace2);
            break;
        case TRACE_OP_MIN:
            sprintf(g_system->modules[result_slot].description, "MIN(S%d,S%d)", trace1, trace2);
            break;
        case TRACE_OP_MAX:
            sprintf(g_system->modules[result_slot].description, "MAX(S%d,S%d)", trace1, trace2);
            break;
        case TRACE_OP_DIFF:
            sprintf(g_system->modules[result_slot].description, "DIFF(S%d,S%d)", trace1, trace2);
            break;
    }
    
    return MATH_SUCCESS;
}

int calculate_realtime_statistics(int trace_slot, statistics_config *config, statistics_result *result) {
    float *data;
    int count;

    if (validate_trace_data(trace_slot) != MATH_SUCCESS || !config || !result) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    data = g_system->modules[trace_slot].module_data;
    count = g_system->modules[trace_slot].module_data_count;
    
    if (calculate_basic_statistics(data, count, result) != MATH_SUCCESS) {
        return MATH_ERROR_MEMORY;
    }

    if (config->enable_histogram) {
        /* histogram not implemented */
    }

    if (config->rolling_stats && config->window_size > 0) {
        calculate_rolling_statistics(trace_slot, config->window_size);
    }

    result->calculation_time = time(NULL);

    return MATH_SUCCESS;
}

/* Mean, min/max, peak-to-peak, RMS, std dev (population). */
int calculate_basic_statistics(float *data, int count, statistics_result *result) {
    int i;
    float sum = 0.0, sum_squares = 0.0;
    float variance;
    
    if (!data || !result || count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    memset(result, 0, sizeof(statistics_result));
    result->sample_count = count;
    result->min_value = data[0];
    result->max_value = data[0];
    
    for (i = 0; i < count; i++) {
        sum += data[i];
        sum_squares += data[i] * data[i];
        
        if (data[i] < result->min_value) {
            result->min_value = data[i];
        }
        if (data[i] > result->max_value) {
            result->max_value = data[i];
        }
    }
    
    result->mean = sum / count;
    result->peak_to_peak = result->max_value - result->min_value;
    
    result->rms = sqrt(sum_squares / count);

    variance = (sum_squares / count) - (result->mean * result->mean);
    result->std_dev = (variance > 0) ? sqrt(variance) : 0.0;

    /* Mean stands in for median/mode: a true median needs a 4 KB sort buffer on the stack */
    result->median = result->mean;
    result->mode = result->mean;
    
    return MATH_SUCCESS;
}

/* Filters the trace in place. */
int apply_digital_filter(int trace_slot, filter_config *config) {
    float *data;
    int count, pass;
    float coefficients[16];

    if (validate_trace_data(trace_slot) != MATH_SUCCESS || !config) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    if (validate_filter_config(config) != MATH_SUCCESS) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    data = g_system->modules[trace_slot].module_data;
    count = g_system->modules[trace_slot].module_data_count;
    
    /* The designers emit one section (1st order, bandpass 2nd); order N cascades N passes. */
    switch (config->filter_type) {
        case FILTER_TYPE_LOWPASS:
            design_lowpass_filter(config, coefficients);
            for (pass = 0; pass < config->order; pass++) {
                apply_iir_filter(data, count, coefficients, 1);
            }
            break;

        case FILTER_TYPE_HIGHPASS:
            design_highpass_filter(config, coefficients);
            for (pass = 0; pass < config->order; pass++) {
                apply_iir_filter(data, count, coefficients, 1);
            }
            break;

        case FILTER_TYPE_BANDPASS:
            design_bandpass_filter(config, coefficients);
            for (pass = 0; pass < config->order; pass++) {
                apply_iir_filter(data, count, coefficients, 2);
            }
            break;
            
        case FILTER_TYPE_MOVING_AVG:
            apply_moving_average_filter(data, count, config->window_size);
            break;
            
        default:
            return MATH_ERROR_INVALID_CONFIG;
    }
    
    return MATH_SUCCESS;
}

/* First-order lowpass. */
int design_lowpass_filter(filter_config *config, float *coefficients) {
    float omega, alpha;

    if (!config || !coefficients) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    omega = 2.0 * 3.14159 * config->cutoff_freq / config->sample_rate;
    alpha = omega / (omega + 1.0);
    
    /* y[n] = alpha*x[n] + (1-alpha)*y[n-1] */
    coefficients[0] = alpha;          /* b0 */
    coefficients[1] = 0.0;            /* b1 */
    coefficients[2] = 1.0;            /* a0 */
    coefficients[3] = -(1.0 - alpha); /* a1 */
    
    return MATH_SUCCESS;
}

/* First-order highpass. */
int design_highpass_filter(filter_config *config, float *coefficients) {
    float omega, alpha;

    if (!config || !coefficients) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    omega = 2.0 * 3.14159 * config->cutoff_freq / config->sample_rate;
    alpha = 1.0 / (omega + 1.0);

    /* RC highpass: y[n] = alpha*(x[n] - x[n-1]) + alpha*y[n-1] */
    coefficients[0] = alpha;          /* b0 */
    coefficients[1] = -alpha;         /* b1 */
    coefficients[2] = 1.0;            /* a0 */
    coefficients[3] = -alpha;         /* a1 */
    
    return MATH_SUCCESS;
}

/* Band-pass: product of a first-order highpass at f-bw/2 and lowpass at f+bw/2,
   written as one second-order section. */
int design_bandpass_filter(filter_config *config, float *coefficients) {
    float center_freq, bandwidth;
    float low_cutoff, high_cutoff;
    float omega_low, omega_high;
    float alpha_low, alpha_high;
    
    if (!config || !coefficients) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    center_freq = config->cutoff_freq;
    bandwidth = config->bandwidth;
    
    low_cutoff = center_freq - bandwidth / 2.0;
    high_cutoff = center_freq + bandwidth / 2.0;
    
    /* Clamp to (0, Nyquist) */
    if (low_cutoff <= 0.0) low_cutoff = 1.0;
    if (high_cutoff >= config->sample_rate / 2.0) {
        high_cutoff = config->sample_rate / 2.0 - 1.0;
    }
    
    omega_low = 2.0 * 3.14159 * low_cutoff / config->sample_rate;
    omega_high = 2.0 * 3.14159 * high_cutoff / config->sample_rate;
    
    alpha_low = 1.0 / (omega_low + 1.0);      /* High-pass component */
    alpha_high = omega_high / (omega_high + 1.0); /* Low-pass component */
    
    /* b0..b2, a0..a2 */
    coefficients[0] = alpha_low * alpha_high;           /* b0 */
    coefficients[1] = -alpha_low * alpha_high;          /* b1 */
    coefficients[2] = 0.0;                              /* b2 */
    coefficients[3] = 1.0;                              /* a0 */
    /* Poles: highpass at alpha_low, lowpass at 1-alpha_high */
    coefficients[4] = -(alpha_low + 1.0 - alpha_high); /* a1 */
    coefficients[5] = alpha_low * (1.0 - alpha_high);  /* a2 */
    
    /* Zero the unused tail of the 16-entry array */
    {
        int i;
        for (i = 6; i < 16; i++) {
            coefficients[i] = 0.0;
        }
    }
    
    return MATH_SUCCESS;
}

/* Trailing (causal) moving average, in place; the window is shorter for the first samples. */
int apply_moving_average_filter(float *data, int count, int window_size) {
    int i, j;
    float sum, *filtered_data;
    int start_idx, end_idx, actual_window;

    if (!data || count <= 0 || window_size <= 0 || window_size > count) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    filtered_data = (float *)malloc(count * sizeof(float));
    if (!filtered_data) {
        return MATH_ERROR_MEMORY;
    }

    for (i = 0; i < count; i++) {
        sum = 0.0;
        start_idx = (i >= window_size) ? i - window_size + 1 : 0;
        end_idx = i;
        actual_window = end_idx - start_idx + 1;
        
        for (j = start_idx; j <= end_idx; j++) {
            sum += data[j];
        }
        
        filtered_data[i] = sum / actual_window;
    }
    
    memcpy(data, filtered_data, count * sizeof(float));
    free(filtered_data);
    
    return MATH_SUCCESS;
}

/* In-place IIR, coefficients laid out [b0..b_order, a0..a_order] with a0 = 1:
   y[n] = sum b_j*x[n-j] - sum(j>=1) a_j*y[n-j]. */
int apply_iir_filter(float *data, int count, float *coefficients, int order) {
    int i, j;
    float *x_history, *y_history;
    float output;

    if (!data || !coefficients || count <= 0 || order <= 0 || order > 8) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    x_history = (float *)calloc(order + 1, sizeof(float));
    y_history = (float *)calloc(order + 1, sizeof(float));
    
    if (!x_history || !y_history) {
        if (x_history) free(x_history);
        if (y_history) free(y_history);
        return MATH_ERROR_MEMORY;
    }
    
    for (i = 0; i < count; i++) {
        for (j = order; j > 0; j--) {
            x_history[j] = x_history[j - 1];
        }
        x_history[0] = data[i];

        output = 0.0;

        /* Feed-forward (numerator) */
        for (j = 0; j <= order; j++) {
            output += coefficients[j] * x_history[j];
        }

        /* Feedback (denominator); y_history[0] is y[n-1] here */
        for (j = 1; j <= order; j++) {
            if (order + 1 + j < 16) {
                output -= coefficients[order + 1 + j] * y_history[j - 1];
            }
        }
        
        for (j = order; j > 0; j--) {
            y_history[j] = y_history[j - 1];
        }
        y_history[0] = output;
        
        data[i] = output;
    }
    
    free(x_history);
    free(y_history);
    
    return MATH_SUCCESS;
}

/* Least-squares y = a + bx; correlation receives R^2. */
int fit_linear_regression(float *x_data, float *y_data, int count, curve_fit_result *result) {
    int i;
    float sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    float mean_x, mean_y, slope, intercept;
    float ss_tot = 0.0, ss_res = 0.0, y_pred;

    if (!x_data || !y_data || !result || count < 2) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    memset(result, 0, sizeof(curve_fit_result));
    result->fit_type = FIT_TYPE_LINEAR;
    result->points_used = count;

    for (i = 0; i < count; i++) {
        sum_x += x_data[i];
        sum_y += y_data[i];
        sum_xy += x_data[i] * y_data[i];
        sum_x2 += x_data[i] * x_data[i];
    }
    
    mean_x = sum_x / count;
    mean_y = sum_y / count;
    
    {
        float denominator = sum_x2 - (sum_x * sum_x) / count;
        if (fabs(denominator) < 1e-10) {
            return MATH_ERROR_DIVISION_BY_ZERO;
        }
        
        slope = (sum_xy - (sum_x * sum_y) / count) / denominator;
        intercept = mean_y - slope * mean_x;
    }
    
    result->coefficients[0] = intercept;  /* a */
    result->coefficients[1] = slope;      /* b */

    for (i = 0; i < count; i++) {
        y_pred = intercept + slope * x_data[i];
        ss_res += (y_data[i] - y_pred) * (y_data[i] - y_pred);
        ss_tot += (y_data[i] - mean_y) * (y_data[i] - mean_y);
    }
    
    if (ss_tot > 1e-10) {
        result->correlation = 1.0 - (ss_res / ss_tot);
    } else {
        result->correlation = 1.0;
    }
    
    result->rms_error = sqrt(ss_res / count);

    result->equation_index = 0; /* Linear */

    return MATH_SUCCESS;
}

/* Least-squares polynomial fit, order <= 3, via normal equations + Gaussian elimination. */
int fit_polynomial(float *x_data, float *y_data, int count, int order, curve_fit_result *result) {
    int i, j, k;
    float matrix[4][5]; /* augmented normal matrix, order <= 3 */
    float temp;
    float coeffs[4];
    float x_mean, x_scale, inv_scale;   /* X mapped to [-1,1] for conditioning */

    if (!x_data || !y_data || !result || count < order + 1 || order > 3) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    memset(result, 0, sizeof(curve_fit_result));
    result->fit_type = FIT_TYPE_POLYNOMIAL;
    result->points_used = count;

    /* Raw indices make sum(x^6) ~1e18 at count~1000, wrecking the float matrix. Scaling X
       leaves R^2/RMS unchanged; coefficients are in the scaled domain (not displayed). */
    {
        float x_min = x_data[0], x_max = x_data[0];
        for (k = 0; k < count; k++) {
            if (x_data[k] < x_min) x_min = x_data[k];
            if (x_data[k] > x_max) x_max = x_data[k];
        }
        x_mean = (x_min + x_max) * 0.5;
        x_scale = (x_max - x_min) * 0.5;
        if (x_scale < 1e-10) x_scale = 1.0;   /* all X equal: avoid divide-by-zero */
        inv_scale = 1.0 / x_scale;
    }

    /* Normal equations, e.g. 2nd order:
       [sum(x^0) sum(x^1) sum(x^2)] [a0]   [sum(y*x^0)]
       [sum(x^1) sum(x^2) sum(x^3)] [a1] = [sum(y*x^1)]
       [sum(x^2) sum(x^3) sum(x^4)] [a2]   [sum(y*x^2)] */
    
    for (i = 0; i <= order; i++) {
        for (j = 0; j <= order; j++) {
            matrix[i][j] = 0.0;
            
            /* sum of x^(i+j) */
            for (k = 0; k < count; k++) {
                float xk = (x_data[k] - x_mean) * inv_scale;
                float x_power = 1.0;
                int p;
                for (p = 0; p < i + j; p++) {
                    x_power *= xk;
                }
                matrix[i][j] += x_power;
            }
        }
        
        /* RHS: sum of y*x^i */
        matrix[i][order + 1] = 0.0;
        for (k = 0; k < count; k++) {
            float xk = (x_data[k] - x_mean) * inv_scale;
            float x_power = 1.0;
            int p;
            for (p = 0; p < i; p++) {
                x_power *= xk;
            }
            matrix[i][order + 1] += y_data[k] * x_power;
        }
    }
    
    /* Gaussian elimination with partial pivoting */
    for (i = 0; i <= order; i++) {
        int pivot_row = i;
        for (j = i + 1; j <= order; j++) {
            if (fabs(matrix[j][i]) > fabs(matrix[pivot_row][i])) {
                pivot_row = j;
            }
        }
        
        if (pivot_row != i) {
            for (k = 0; k <= order + 1; k++) {
                temp = matrix[i][k];
                matrix[i][k] = matrix[pivot_row][k];
                matrix[pivot_row][k] = temp;
            }
        }
        
        if (fabs(matrix[i][i]) < 1e-10) {   /* singular */
            return MATH_ERROR_CONVERGENCE;
        }
        
        for (j = i + 1; j <= order; j++) {
            float factor = matrix[j][i] / matrix[i][i];
            for (k = i; k <= order + 1; k++) {
                matrix[j][k] -= factor * matrix[i][k];
            }
        }
    }
    
    /* Back substitution */
    for (i = order; i >= 0; i--) {
        coeffs[i] = matrix[i][order + 1];
        for (j = i + 1; j <= order; j++) {
            coeffs[i] -= matrix[i][j] * coeffs[j];
        }
        coeffs[i] /= matrix[i][i];

        if (i < 6) {
            result->coefficients[i] = coeffs[i];
        }
    }
    
    /* R^2 and RMS error */
    {
        float ss_res = 0.0, ss_tot = 0.0;
        float y_mean = 0.0;
        float y_pred, x_power;

        for (i = 0; i < count; i++) {
            y_mean += y_data[i];
        }
        y_mean /= count;

        for (i = 0; i < count; i++) {
            /* predict on the same scaled X as the fit */
            float xk = (x_data[i] - x_mean) * inv_scale;
            y_pred = 0.0;
            x_power = 1.0;
            for (j = 0; j <= order; j++) {
                y_pred += coeffs[j] * x_power;
                x_power *= xk;
            }

            ss_res += (y_data[i] - y_pred) * (y_data[i] - y_pred);
            ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
        }
        
        result->correlation = (ss_tot > 1e-10) ? (1.0 - ss_res / ss_tot) : 1.0;
        result->rms_error = sqrt(ss_res / count);
    }
    
    if (order == 1) {
        result->equation_index = 0; /* Linear */
    } else if (order == 2) {
        result->equation_index = 1; /* Quadratic */
    } else if (order == 3) {
        result->equation_index = 2; /* Cubic */
    } else {
        result->equation_index = 6; /* Unknown */
    }
    
    return MATH_SUCCESS;
}

/* y = a * exp(b * x), via linear regression on ln(y); R^2 is computed on the original y. */
int fit_exponential(float *x_data, float *y_data, int count, curve_fit_result *result) {
    float *ln_y_data;
    int i;
    int fit_result;
    curve_fit_result linear_result;
    
    if (!x_data || !y_data || !result || count < 3) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    /* ln(y) needs y > 0 */
    for (i = 0; i < count; i++) {
        if (y_data[i] <= 0.0) {
            return MATH_ERROR_INVALID_PARAMS;
        }
    }
    
    ln_y_data = (float *)malloc(count * sizeof(float));
    if (!ln_y_data) {
        return MATH_ERROR_MEMORY;
    }
    
    /* ln(y) = ln(a) + b*x */
    for (i = 0; i < count; i++) {
        ln_y_data[i] = log(y_data[i]);
    }
    
    fit_result = fit_linear_regression(x_data, ln_y_data, count, &linear_result);
    
    free(ln_y_data);
    
    if (fit_result != MATH_SUCCESS) {
        return fit_result;
    }
    
    memset(result, 0, sizeof(curve_fit_result));
    result->fit_type = FIT_TYPE_EXPONENTIAL;
    result->points_used = count;

    /* a = exp(intercept), b = slope */
    result->coefficients[0] = exp(linear_result.coefficients[0]); /* a */
    result->coefficients[1] = linear_result.coefficients[1];      /* b */
    
    {
        float ss_res = 0.0, ss_tot = 0.0;
        float y_mean = 0.0;
        float y_pred;

        for (i = 0; i < count; i++) {
            y_mean += y_data[i];
        }
        y_mean /= count;

        for (i = 0; i < count; i++) {
            y_pred = result->coefficients[0] * exp(result->coefficients[1] * x_data[i]);
            ss_res += (y_data[i] - y_pred) * (y_data[i] - y_pred);
            ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
        }
        
        result->correlation = (ss_tot > 1e-10) ? (1.0 - ss_res / ss_tot) : 1.0;
        result->rms_error = sqrt(ss_res / count);
    }
    
    result->equation_index = 3; /* Exponential */
    
    return MATH_SUCCESS;
}

/* R^2 and RMS error of y_fitted against y_data. */
int calculate_fit_quality(float *y_data, float *y_fitted, int count, curve_fit_result *result) {
    int i;
    float ss_res = 0.0, ss_tot = 0.0;
    float y_mean = 0.0;

    if (!y_data || !y_fitted || !result || count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    for (i = 0; i < count; i++) {
        y_mean += y_data[i];
    }
    y_mean /= count;

    for (i = 0; i < count; i++) {
        ss_res += (y_data[i] - y_fitted[i]) * (y_data[i] - y_fitted[i]);
        ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
    }

    result->correlation = (ss_tot > 1e-10) ? (1.0 - ss_res / ss_tot) : 1.0;
    result->rms_error = sqrt(ss_res / count);
    result->points_used = count;
    
    return MATH_SUCCESS;
}

/* Pearson coefficient and sample covariance of two traces over the shorter length. */
int calculate_correlation(int trace1, int trace2, correlation_result *result) {
    float *data1, *data2;
    int count1, count2, count;
    int i;
    float sum1 = 0.0, sum2 = 0.0, sum12 = 0.0, sum1_sq = 0.0, sum2_sq = 0.0;
    float mean1, mean2, num, den1, den2;
    
    if (!result) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    if (validate_trace_data(trace1) != MATH_SUCCESS ||
        validate_trace_data(trace2) != MATH_SUCCESS) {
        return MATH_ERROR_NO_DATA;
    }

    memset(result, 0, sizeof(correlation_result));
    
    data1 = g_system->modules[trace1].module_data;
    data2 = g_system->modules[trace2].module_data;
    count1 = g_system->modules[trace1].module_data_count;
    count2 = g_system->modules[trace2].module_data_count;
    
    count = (count1 < count2) ? count1 : count2;

    if (count < 2) {
        return MATH_ERROR_NO_DATA;
    }

    for (i = 0; i < count; i++) {
        sum1 += data1[i];
        sum2 += data2[i];
        sum12 += data1[i] * data2[i];
        sum1_sq += data1[i] * data1[i];
        sum2_sq += data2[i] * data2[i];
    }
    
    mean1 = sum1 / count;
    mean2 = sum2 / count;
    
    num = sum12 - (sum1 * sum2) / count;
    den1 = sum1_sq - (sum1 * sum1) / count;
    den2 = sum2_sq - (sum2 * sum2) / count;
    
    if (den1 > 1e-10 && den2 > 1e-10) {
        result->correlation_coefficient = num / sqrt(den1 * den2);
    } else {
        result->correlation_coefficient = 0.0;
    }
    
    result->covariance = num / (count - 1);

    return MATH_SUCCESS;
}

/* Normalized cross-correlation r[lag], lag 0..count/2, into caller's correlation[count];
   lags past count/2 are zeroed (too little overlap). Each lag is normalized by its own
   overlap energy so r stays in [-1,1] without small-lag bias; double accumulators avoid
   overflow on long traces. r[0] is Pearson. O(count^2). */
int calculate_cross_correlation(float *data1, float *data2, int count, float *correlation) {
    int i, lag, max_lag;
    double mean1 = 0.0, mean2 = 0.0, sxy, sxx, syy;
    float *c1, *c2;

    if (!data1 || !data2 || !correlation || count < 2) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    c1 = (float *)malloc(count * sizeof(float));
    c2 = (float *)malloc(count * sizeof(float));
    if (!c1 || !c2) {
        if (c1) free(c1);
        if (c2) free(c2);
        return MATH_ERROR_MEMORY;
    }

    for (i = 0; i < count; i++) { mean1 += data1[i]; mean2 += data2[i]; }
    mean1 /= count;
    mean2 /= count;
    for (i = 0; i < count; i++) {
        c1[i] = (float)(data1[i] - mean1);
        c2[i] = (float)(data2[i] - mean2);
    }

    max_lag = count / 2;   /* keep the overlap >= half the record for a reliable estimate */
    for (lag = 0; lag < count; lag++) {
        if (lag > max_lag) {            /* too little overlap to be meaningful */
            correlation[lag] = 0.0;
            continue;
        }
        sxy = 0.0; sxx = 0.0; syy = 0.0;
        for (i = 0; i + lag < count; i++) {
            sxy += (double)c1[i] * c2[i + lag];
            sxx += (double)c1[i] * c1[i];
            syy += (double)c2[i + lag] * c2[i + lag];
        }
        if (sxx > 1e-20 && syy > 1e-20) {
            correlation[lag] = (float)(sxy / sqrt(sxx * syy));
        } else {
            correlation[lag] = 0.0;     /* constant overlap window */
        }
    }

    free(c1);
    free(c2);
    return MATH_SUCCESS;
}

/* Lag in samples (>= 0) of peak cross-correlation of trace2 vs trace1, or -1 on error.
   The caller converts the lag to time/phase. */
int calculate_phase_shift(int trace1, int trace2, float sample_rate) {
    float *data1, *data2, *corr;
    int count, count1, count2, lag, best_lag = 0;
    float best = -2.0;

    (void)sample_rate;  /* lag is returned in samples; caller scales by sample_rate */

    if (validate_trace_data(trace1) != MATH_SUCCESS ||
        validate_trace_data(trace2) != MATH_SUCCESS) {
        return -1;
    }

    data1 = g_system->modules[trace1].module_data;
    data2 = g_system->modules[trace2].module_data;
    count1 = g_system->modules[trace1].module_data_count;
    count2 = g_system->modules[trace2].module_data_count;
    count = (count1 < count2) ? count1 : count2;
    if (count < 2) {
        return -1;
    }

    corr = (float *)malloc(count * sizeof(float));
    if (!corr) {
        return -1;
    }

    if (calculate_cross_correlation(data1, data2, count, corr) != MATH_SUCCESS) {
        free(corr);
        return -1;
    }

    /* Stop at count/2: the zeroed tail would win for anti-correlated inputs. */
    for (lag = 0; lag <= count / 2; lag++) {
        if (corr[lag] > best) {
            best = corr[lag];
            best_lag = lag;
        }
    }

    free(corr);
    return best_lag;
}

int validate_trace_data(int trace_slot) {
    if (trace_slot < 0 || trace_slot >= 10) {
        return MATH_ERROR_INVALID_TRACE;
    }
    
    if (!g_system->modules[trace_slot].enabled) {
        return MATH_ERROR_INVALID_TRACE;
    }
    
    if (!g_system->modules[trace_slot].module_data) {
        return MATH_ERROR_NO_DATA;
    }
    
    if (g_system->modules[trace_slot].module_data_count == 0) {
        return MATH_ERROR_NO_DATA;
    }
    
    return MATH_SUCCESS;
}

int validate_filter_config(filter_config *config) {
    if (!config) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    if (config->filter_type < 0 || config->filter_type > FILTER_TYPE_MOVING_AVG) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    if (config->cutoff_freq <= 0.0 || config->sample_rate <= 0.0) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    if (config->cutoff_freq >= config->sample_rate / 2.0) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    if (config->order < 1 || config->order > 8) {
        return MATH_ERROR_INVALID_CONFIG;
    }
    
    return MATH_SUCCESS;
}

int remove_dc_offset(float *data, int count) {
    int i;
    float sum = 0.0, mean;

    if (!data || count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / count;

    for (i = 0; i < count; i++) {
        data[i] -= mean;
    }
    
    return MATH_SUCCESS;
}

/* Local maxima above mean + 2 sigma; returns the number found (<= max_peaks). */
int find_peaks(float *data, int count, int *peak_indices, int max_peaks) {
    int i, peak_count = 0;
    float threshold;
    float sum = 0.0, mean, std_dev = 0.0;

    if (!data || !peak_indices || count < 3 || max_peaks <= 0) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / count;
    
    for (i = 0; i < count; i++) {
        std_dev += (data[i] - mean) * (data[i] - mean);
    }
    std_dev = sqrt(std_dev / count);
    
    threshold = mean + 2.0 * std_dev;

    for (i = 1; i < count - 1 && peak_count < max_peaks; i++) {
        if (data[i] > data[i-1] && data[i] > data[i+1] && data[i] > threshold) {
            peak_indices[peak_count] = i;
            peak_count++;
        }
    }
    
    return peak_count;
}

/* Stats over the last window_size samples; the result is computed and discarded. */
int calculate_rolling_statistics(int trace_slot, int window_size) {
    float *data;
    int count, start_index, samples_to_use;
    statistics_result result;
    
    if (trace_slot < 0 || trace_slot >= MAX_TRACES) {
        return MATH_ERROR_INVALID_TRACE;
    }
    
    if (window_size < 10 || window_size > ROLLING_BUFFER_SIZE) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    if (validate_trace_data(trace_slot) != MATH_SUCCESS) {
        return MATH_ERROR_NO_DATA;
    }

    data = g_system->modules[trace_slot].module_data;
    count = g_system->modules[trace_slot].module_data_count;

    samples_to_use = (count < window_size) ? count : window_size;
    start_index = count - samples_to_use;

    if (samples_to_use > 0) {
        calculate_basic_statistics(data + start_index, samples_to_use, &result);
    }
    
    return MATH_SUCCESS;
}

/* Recomputes basic statistics for the whole trace; nothing is cached. */
int get_statistics_result(int trace_slot, statistics_result *result) {
    if (trace_slot < 0 || trace_slot >= MAX_TRACES || !result || 
        validate_trace_data(trace_slot) != MATH_SUCCESS) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    return calculate_basic_statistics(g_system->modules[trace_slot].module_data,
                                    g_system->modules[trace_slot].module_data_count, 
                                    result);
}

/* Counts per equal-width bin over [min, max]; the max value lands in the last bin. */
int calculate_histogram(float *data, int count, float *bins, int bin_count) {
    int i, bin_index;
    float min_val, max_val, range, bin_width;

    if (!data || !bins || count <= 0 || bin_count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    min_val = max_val = data[0];
    for (i = 1; i < count; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    
    range = max_val - min_val;
    if (range == 0.0) {
        /* all values equal */
        bins[0] = (float)count;
        for (i = 1; i < bin_count; i++) {
            bins[i] = 0.0;
        }
        return MATH_SUCCESS;
    }
    
    bin_width = range / bin_count;
    
    for (i = 0; i < bin_count; i++) {
        bins[i] = 0.0;
    }

    for (i = 0; i < count; i++) {
        bin_index = (int)((data[i] - min_val) / bin_width);
        if (bin_index >= bin_count) bin_index = bin_count - 1;
        if (bin_index < 0) bin_index = 0;
        bins[bin_index] += 1.0;
    }
    
    return MATH_SUCCESS;
}

/* Median into *result; count <= 1024 (4 KB stack copy, bubble sort). */
int calculate_median(float *data, int count, float *result) {
    float sorted_data[1024];
    int i, j;
    float median;

    if (!data || !result || count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    if (count > 1024) {
        return MATH_ERROR_BUFFER_OVERFLOW;
    }
    
    memcpy(sorted_data, data, count * sizeof(float));

    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (sorted_data[j] > sorted_data[j + 1]) {
                float temp = sorted_data[j];
                sorted_data[j] = sorted_data[j + 1];
                sorted_data[j + 1] = temp;
            }
        }
    }
    
    if (count % 2 == 0) {
        median = (sorted_data[count/2 - 1] + sorted_data[count/2]) / 2.0;
    } else {
        median = sorted_data[count/2];
    }
    
    *result = median;
    return MATH_SUCCESS;
}

/* Most frequent value (within tolerance) into *result. O(count^2). */
int calculate_mode(float *data, int count, float *result) {
    float tolerance = 0.001;
    float mode_value;
    int max_frequency = 1;
    int i, j, frequency;

    if (!data || !result || count <= 0) {
        return MATH_ERROR_INVALID_PARAMS;
    }

    mode_value = data[0];

    for (i = 0; i < count; i++) {
        frequency = 1;
        for (j = i + 1; j < count; j++) {
            if (fabs(data[i] - data[j]) <= tolerance) {
                frequency++;
            }
        }
        
        if (frequency > max_frequency) {
            max_frequency = frequency;
            mode_value = data[i];
        }
    }
    
    *result = mode_value;
    return MATH_SUCCESS;
}

/* Dominant frequency (Hz) estimated from zero crossings, into *result. */
int calculate_frequency_analysis(float *data, int count, float sample_rate, float *result) {
    int zero_crossings = 0;
    int i;
    float dominant_freq;

    if (!data || !result || count <= 0 || sample_rate <= 0.0) {
        return MATH_ERROR_INVALID_PARAMS;
    }
    
    for (i = 1; i < count; i++) {
        if ((data[i-1] >= 0.0 && data[i] < 0.0) || 
            (data[i-1] < 0.0 && data[i] >= 0.0)) {
            zero_crossings++;
        }
    }
    
    dominant_freq = (zero_crossings * sample_rate) / (2.0 * count);  /* two crossings per cycle */

    *result = dominant_freq;
    return MATH_SUCCESS;
}
