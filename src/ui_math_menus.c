/*
 * TM5000 GPIB Control System - Mathematical Functions UI Menus
 * Version 3.5
 * User interface menus for advanced mathematical analysis functions
 * 
 * This module provides CGA-optimized menu interfaces for:
 * - Dual-trace operations (add, subtract, multiply, divide)
 * - Digital filtering (low-pass, high-pass, band-pass)
 * - Curve fitting (linear, polynomial, exponential)
 * - Correlation analysis and signal processing
 * 
 * Memory optimized for DOS 16-bit environment with minimal overhead.
 */

#include "tm5000.h"
#include "math_functions.h"
#include "ui.h"

/* Dual-trace operations menu */
void dual_trace_operations_menu(void) {
    int choice;
    int done = 0;
    int trace1, trace2, result_slot;
    int operation, result;
    char *operation_names[] = {
        "Add", "Subtract", "Multiply", "Divide", 
        "Average", "Minimum", "Maximum", "Difference"
    };
    
    while (!done) {
        clrscr();
        printf("Dual-Trace Operations\n");
        printf("=====================\n\n");
        
        printf("Available traces:\n");
        printf("-----------------\n");
        {
            int i, has_traces = 0;
            for (i = 0; i < 10; i++) {
                if (g_system->modules[i].enabled && 
                    g_system->modules[i].module_data && 
                    g_system->modules[i].module_data_count > 0) {
                    printf("Slot %d: %s - %u samples\n", 
                           i, g_system->modules[i].description,
                           g_system->modules[i].module_data_count);
                    has_traces = 1;
                }
            }
            
            if (!has_traces) {
                printf("No trace data available. Run measurements first.\n\n");
                printf("Press any key to return...");
                getch();
                return;
            }
        }
        
        printf("\nOperations:\n");
        printf("-----------\n");
        printf("1. Add traces (A + B)\n");
        printf("2. Subtract traces (A - B)\n");
        printf("3. Multiply traces (A * B)\n");
        printf("4. Divide traces (A / B)\n");
        printf("5. Average traces ((A + B) / 2)\n");
        printf("6. Minimum values (MIN(A, B))\n");
        printf("7. Maximum values (MAX(A, B))\n");
        printf("8. Absolute difference (|A - B|)\n");
        printf("0. Return to Math Menu\n\n");
        
        printf("Choice: ");
        choice = getch();
        
        if (choice >= '1' && choice <= '8') {
            operation = choice - '1';
            
            clrscr();
            printf("Dual-Trace Operation: %s\n", operation_names[operation]);
            printf("=========================%s\n", 
                   (operation == 0) ? "=" : (operation == 1) ? "=====" : 
                   (operation == 2) ? "======" : (operation == 3) ? "====" :
                   (operation == 4) ? "=====" : (operation == 5) ? "=======" :
                   (operation == 6) ? "=======" : "==========");
            
            printf("\nEnter first trace slot (0-9): ");
            scanf("%d", &trace1);
            
            printf("Enter second trace slot (0-9): ");
            scanf("%d", &trace2);
            
            printf("Enter result slot (0-9): ");
            scanf("%d", &result_slot);
            
            if (trace1 >= 0 && trace1 < 10 && trace2 >= 0 && trace2 < 10 && 
                result_slot >= 0 && result_slot < 10) {
                
                result = perform_dual_trace_operation(trace1, trace2, operation, result_slot);
                
                printf("\n");
                switch (result) {
                    case MATH_SUCCESS:
                        printf("Operation completed successfully!\n");
                        printf("Result stored in slot %d\n", result_slot);
                        
                        /* Set up trace for display */
                        g_traces[result_slot].enabled = 1;
                        g_traces[result_slot].slot = result_slot;
                        g_traces[result_slot].color = 0x0C; /* Light red for computed data */
                        g_traces[result_slot].unit_type = UNIT_VOLTAGE;
                        g_traces[result_slot].x_scale = 1.0;
                        g_traces[result_slot].x_offset = 0.0;
                        strcpy(g_traces[result_slot].label, g_system->modules[result_slot].description);
                        g_traces[result_slot].data = g_system->modules[result_slot].module_data;
                        g_traces[result_slot].data_count = g_system->modules[result_slot].module_data_count;
                        break;
                    case MATH_ERROR_INVALID_TRACE:
                        printf("Error: Invalid trace slot specified\n");
                        break;
                    case MATH_ERROR_NO_DATA:
                        printf("Error: No data in specified traces\n");
                        break;
                    case MATH_ERROR_MEMORY:
                        printf("Error: Insufficient memory\n");
                        break;
                    default:
                        printf("Error: Operation failed (code %d)\n", result);
                        break;
                }
            } else {
                printf("\nError: Invalid slot numbers\n");
            }
            
            printf("\nPress any key to continue...");
            getch();
        } else if (choice == '0' || choice == 27) {
            done = 1;
        }
    }
}

/* Digital filter configuration and application menu */
void digital_filter_menu(void) {
    int choice;
    int done = 0;
    int trace_slot, result;
    filter_config config;
    
    /* Initialize default filter configuration */
    config.filter_type = FILTER_TYPE_LOWPASS;
    config.cutoff_freq = 100.0;
    config.bandwidth = 50.0;
    config.order = 2;
    config.sample_rate = 1000.0;
    config.gain = 1.0;
    config.window_size = 5;
    
    while (!done) {
        clrscr();
        printf("Digital Filtering\n");
        printf("=================\n\n");
        
        printf("Current configuration:\n");
        printf("----------------------\n");
        printf("Filter type: %s\n", 
               (config.filter_type == FILTER_TYPE_LOWPASS) ? "Low-pass" :
               (config.filter_type == FILTER_TYPE_HIGHPASS) ? "High-pass" :
               (config.filter_type == FILTER_TYPE_BANDPASS) ? "Band-pass" :
               (config.filter_type == FILTER_TYPE_MOVING_AVG) ? "Moving Average" : "Unknown");
        
        if (config.filter_type != FILTER_TYPE_MOVING_AVG) {
            printf("Cutoff freq: %.1f Hz\n", config.cutoff_freq);
            printf("Sample rate: %.1f Hz\n", config.sample_rate);
            printf("Filter order: %d\n", config.order);
            if (config.filter_type == FILTER_TYPE_BANDPASS) {
                printf("Bandwidth: %.1f Hz\n", config.bandwidth);
            }
        } else {
            printf("Window size: %d samples\n", config.window_size);
        }
        
        printf("\nOptions:\n");
        printf("--------\n");
        printf("1. Configure Low-pass Filter\n");
        printf("2. Configure High-pass Filter\n");
        printf("3. Configure Band-pass Filter\n");
        printf("4. Configure Moving Average\n");
        printf("5. Apply Filter to Trace\n");
        printf("6. Set Sample Rate\n");
        printf("0. Return to Math Menu\n\n");
        
        printf("Choice: ");
        choice = getch();
        
        switch (choice) {
            case '1':
                config.filter_type = FILTER_TYPE_LOWPASS;
                printf("\n\nLow-pass Filter Configuration\n");
                printf("Enter cutoff frequency (Hz): ");
                scanf("%f", &config.cutoff_freq);
                printf("Enter filter order (1-4): ");
                scanf("%d", &config.order);
                if (config.order < 1) config.order = 1;
                if (config.order > 4) config.order = 4;
                break;
                
            case '2':
                config.filter_type = FILTER_TYPE_HIGHPASS;
                printf("\n\nHigh-pass Filter Configuration\n");
                printf("Enter cutoff frequency (Hz): ");
                scanf("%f", &config.cutoff_freq);
                printf("Enter filter order (1-4): ");
                scanf("%d", &config.order);
                if (config.order < 1) config.order = 1;
                if (config.order > 4) config.order = 4;
                break;
                
            case '3':
                config.filter_type = FILTER_TYPE_BANDPASS;
                printf("\n\nBand-pass Filter Configuration\n");
                printf("Enter center frequency (Hz): ");
                scanf("%f", &config.cutoff_freq);
                printf("Enter bandwidth (Hz): ");
                scanf("%f", &config.bandwidth);
                printf("Enter filter order (1-4): ");
                scanf("%d", &config.order);
                if (config.order < 1) config.order = 1;
                if (config.order > 4) config.order = 4;
                break;
                
            case '4':
                config.filter_type = FILTER_TYPE_MOVING_AVG;
                printf("\n\nMoving Average Filter Configuration\n");
                printf("Enter window size (3-21, odd numbers): ");
                scanf("%d", &config.window_size);
                if (config.window_size < 3) config.window_size = 3;
                if (config.window_size > 21) config.window_size = 21;
                if (config.window_size % 2 == 0) config.window_size++; /* Make odd */
                break;
                
            case '5':
                printf("\n\nApply Filter\n");
                printf("Enter trace slot to filter (0-9): ");
                scanf("%d", &trace_slot);
                
                if (trace_slot >= 0 && trace_slot < 10) {
                    result = apply_digital_filter(trace_slot, &config);
                    
                    printf("\n");
                    switch (result) {
                        case MATH_SUCCESS:
                            printf("Filter applied successfully!\n");
                            printf("Trace %d has been filtered in-place\n", trace_slot);
                            break;
                        case MATH_ERROR_INVALID_TRACE:
                            printf("Error: Invalid trace slot\n");
                            break;
                        case MATH_ERROR_NO_DATA:
                            printf("Error: No data in trace\n");
                            break;
                        case MATH_ERROR_INVALID_CONFIG:
                            printf("Error: Invalid filter configuration\n");
                            break;
                        default:
                            printf("Error: Filter operation failed\n");
                            break;
                    }
                } else {
                    printf("\nError: Invalid trace slot\n");
                }
                
                printf("Press any key to continue...");
                getch();
                break;
                
            case '6':
                printf("\n\nSample Rate Configuration\n");
                printf("Current sample rate: %.1f Hz\n", config.sample_rate);
                printf("Enter new sample rate (Hz): ");
                scanf("%f", &config.sample_rate);
                if (config.sample_rate <= 0.0) config.sample_rate = 1000.0;
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Curve fitting menu */
void curve_fitting_menu(void) {
    int choice;
    int done = 0;
    int trace_slot;
    curve_fit_result result;
    double *y_data, *x_data;
    int count, i, fit_result;
    
    while (!done) {
        clrscr();
        printf("Curve Fitting\n");
        printf("=============\n\n");
        
        printf("Available fitting methods:\n");
        printf("--------------------------\n");
        printf("1. Linear Regression (y = a + bx)\n");
        printf("2. Polynomial Fitting (order 2-3)\n");
        printf("3. Exponential Fitting (y = a*e^bx)\n");
        printf("0. Return to Math Menu\n\n");
        
        printf("Choice: ");
        choice = getch();
        
        switch (choice) {
            case '1':
            case '2':
            case '3': {
                int method = choice - '0';   /* 1=linear, 2=polynomial, 3=exponential */
                int order = 2;
                printf("\n\n%s\n",
                       (method == 1) ? "Linear Regression (y = a + bx)" :
                       (method == 2) ? "Polynomial Fitting" : "Exponential Fitting (y = a*e^bx)");
                printf("Enter trace slot for Y data (0-9): ");
                scanf("%d", &trace_slot);
                if (method == 2) {
                    printf("Polynomial order (2-3): ");
                    scanf("%d", &order);
                    if (order < 2) order = 2;
                    if (order > 3) order = 3;
                }

                if (trace_slot >= 0 && trace_slot < 10 &&
                    g_system->modules[trace_slot].enabled &&
                    g_system->modules[trace_slot].module_data &&
                    g_system->modules[trace_slot].module_data_count > 1) {

                    y_data = g_system->modules[trace_slot].module_data;
                    count = g_system->modules[trace_slot].module_data_count;

                    /* Generate X data as sample indices */
                    x_data = (double *)malloc(count * sizeof(double));
                    if (x_data) {
                        for (i = 0; i < count; i++) {
                            x_data[i] = (double)i;
                        }

                        if (method == 1) {
                            fit_result = fit_linear_regression(x_data, y_data, count, &result);
                        } else if (method == 2) {
                            fit_result = fit_polynomial(x_data, y_data, count, order, &result);
                        } else {
                            fit_result = fit_exponential(x_data, y_data, count, &result);
                        }

                        printf("\n");
                        if (fit_result == MATH_SUCCESS) {
                            printf("Curve Fit Results:\n");
                            printf("------------------\n");
                            printf("Equation: %s\n", get_equation_text(result.equation_index));
                            printf("Correlation (R^2): %.4f\n", result.correlation);
                            printf("RMS Error: %.6f\n", result.rms_error);
                            printf("Points used: %d\n", result.points_used);

                            if (result.correlation > 0.9) {
                                printf("Fit quality: Excellent\n");
                            } else if (result.correlation > 0.8) {
                                printf("Fit quality: Good\n");
                            } else if (result.correlation > 0.6) {
                                printf("Fit quality: Fair\n");
                            } else {
                                printf("Fit quality: Poor\n");
                            }
                        } else {
                            printf("Error: curve fitting failed\n");
                            if (method == 3) {
                                printf("(exponential fit needs all-positive Y values)\n");
                            } else {
                                printf("(need at least order+1 data points)\n");
                            }
                        }

                        free(x_data);
                    } else {
                        printf("\nError: Insufficient memory\n");
                    }
                } else {
                    printf("\nError: Invalid trace or insufficient data\n");
                }

                printf("\nPress any key to continue...");
                getch();
                break;
            }
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Correlation analysis menu */
void correlation_analysis_menu(void) {
    int choice;
    int done = 0;
    int trace1, trace2, corr_result;
    correlation_result result;
    
    while (!done) {
        clrscr();
        printf("Correlation Analysis\n");
        printf("====================\n\n");
        
        printf("Available analysis:\n");
        printf("-------------------\n");
        printf("1. Pearson Correlation Coefficient\n");
        printf("2. Cross-correlation (best alignment lag)\n");
        printf("3. Phase Shift / Delay Analysis\n");
        printf("0. Return to Math Menu\n\n");
        
        printf("Choice: ");
        choice = getch();
        
        switch (choice) {
            case '1':
                printf("\n\nPearson Correlation Analysis\n");
                printf("Enter first trace slot (0-9): ");
                scanf("%d", &trace1);
                
                printf("Enter second trace slot (0-9): ");
                scanf("%d", &trace2);
                
                if (trace1 >= 0 && trace1 < 10 && trace2 >= 0 && trace2 < 10) {
                    corr_result = calculate_correlation(trace1, trace2, &result);
                    
                    printf("\n");
                    if (corr_result == MATH_SUCCESS) {
                        printf("Correlation Analysis Results:\n");
                        printf("-----------------------------\n");
                        printf("Correlation coefficient: %.4f\n", result.correlation_coefficient);
                        printf("Covariance: %.6f\n", result.covariance);
                        
                        printf("\nInterpretation:\n");
                        if (fabs(result.correlation_coefficient) > 0.9) {
                            printf("Very strong %s correlation\n", 
                                   (result.correlation_coefficient > 0) ? "positive" : "negative");
                        } else if (fabs(result.correlation_coefficient) > 0.7) {
                            printf("Strong %s correlation\n",
                                   (result.correlation_coefficient > 0) ? "positive" : "negative");
                        } else if (fabs(result.correlation_coefficient) > 0.5) {
                            printf("Moderate %s correlation\n",
                                   (result.correlation_coefficient > 0) ? "positive" : "negative");
                        } else if (fabs(result.correlation_coefficient) > 0.3) {
                            printf("Weak %s correlation\n",
                                   (result.correlation_coefficient > 0) ? "positive" : "negative");
                        } else {
                            printf("Very weak or no correlation\n");
                        }
                    } else {
                        printf("Error: Correlation calculation failed\n");
                    }
                } else {
                    printf("\nError: Invalid trace slots\n");
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case '2': {  /* Cross-correlation */
                int count1, count2, count, lag, best_lag = 0;
                double *d1, *d2, *corr, best = -2.0;
                printf("\n\nCross-Correlation\n");
                printf("Enter first trace slot (0-9): ");
                scanf("%d", &trace1);
                printf("Enter second trace slot (0-9): ");
                scanf("%d", &trace2);

                if (trace1 >= 0 && trace1 < 10 && trace2 >= 0 && trace2 < 10 &&
                    g_system->modules[trace1].enabled && g_system->modules[trace2].enabled &&
                    g_system->modules[trace1].module_data && g_system->modules[trace2].module_data) {

                    d1 = g_system->modules[trace1].module_data;
                    d2 = g_system->modules[trace2].module_data;
                    count1 = g_system->modules[trace1].module_data_count;
                    count2 = g_system->modules[trace2].module_data_count;
                    count = (count1 < count2) ? count1 : count2;
                    corr = (double *)malloc(count * sizeof(double));

                    if (count >= 2 && corr) {
                        printf("Computing cross-correlation (%d points)...\n", count);  /* O(n^2) - can be slow (review #12) */
                        if (calculate_cross_correlation(d1, d2, count, corr) == MATH_SUCCESS) {
                            /* scan only the reliable lag range (tail is zeroed) (review) */
                            for (lag = 0; lag <= count / 2; lag++) {
                                if (corr[lag] > best) { best = corr[lag]; best_lag = lag; }
                            }
                            printf("\nCross-Correlation Results:\n");
                            printf("--------------------------\n");
                            printf("Zero-lag correlation: %.4f\n", corr[0]);
                            printf("Peak correlation:     %.4f\n", best);
                            printf("Best alignment lag:   %d samples\n", best_lag);
                            if (best_lag == 0) {
                                printf("(traces are best aligned with no shift)\n");
                            } else {
                                printf("(trace %d leads trace %d by %d samples)\n", trace1, trace2, best_lag);
                            }
                        } else {
                            printf("\nError: cross-correlation failed\n");
                        }
                    } else {
                        printf("\nError: insufficient data or memory\n");
                    }
                    if (corr) free(corr);
                } else {
                    printf("\nError: Invalid trace slots\n");
                }

                printf("\nPress any key to continue...");
                getch();
                break;
            }

            case '3': {  /* Phase shift / delay */
                float sample_rate;
                int lag;
                printf("\n\nPhase Shift / Delay Analysis\n");
                printf("Enter first (reference) trace slot (0-9): ");
                scanf("%d", &trace1);
                printf("Enter second trace slot (0-9): ");
                scanf("%d", &trace2);

                sample_rate = (g_control_panel.sample_rate_ms > 0)
                              ? (1000.0 / g_control_panel.sample_rate_ms) : 1.0;
                printf("Computing phase/delay (cross-correlation)...\n");  /* O(n^2) - can be slow (review #12) */
                lag = calculate_phase_shift(trace1, trace2, sample_rate);

                printf("\n");
                if (lag >= 0) {
                    printf("Phase / Delay Results:\n");
                    printf("----------------------\n");
                    printf("Best alignment lag: %d samples\n", lag);
                    printf("Sample rate:        %.2f Hz\n", sample_rate);
                    printf("Time delay:         %.4f s\n", (sample_rate > 0.0) ? (lag / sample_rate) : 0.0);
                    if (lag == 0) {
                        printf("(no detectable delay between the traces)\n");
                    } else {
                        printf("(trace %d is delayed relative to trace %d)\n", trace2, trace1);
                    }
                } else {
                    printf("Error: phase analysis failed (invalid traces or <2 samples)\n");
                }

                printf("\nPress any key to continue...");
                getch();
                break;
            }
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}