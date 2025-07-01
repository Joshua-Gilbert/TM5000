/*
 * TM5000 GPIB Control System - Mathematical Functions Module
 * Version 3.4
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Version update
 * 3.3 - Version update  
 * 3.4 - Removed FFT buffer size constraints for 1024-sample optimization
 */

#include "tm5000.h"
#include "math_functions.h"
#include <math.h>

/* 287-optimized precision routines per scientific programming guide */

/* Kahan summation accumulator for high-precision accumulation */
typedef struct {
    double sum;
    double compensation;
} kahan_accumulator;

void kahan_init(kahan_accumulator *acc) {
    acc->sum = 0.0;
    acc->compensation = 0.0;
}

void kahan_add(kahan_accumulator *acc, double value) {
    double y = value - acc->compensation;
    double t = acc->sum + y;
    
    /* Extract lost low-order bits */
    acc->compensation = (t - acc->sum) - y;
    acc->sum = t;
}

/* 287-safe cosine with range reduction */
double cos_287_safe(double x) {
    const double PI = 3.14159265358979323846;
    const double PIOVER4 = 0.78539816339744830962;
    double reduced = fmod(x, 2.0 * PI);
    
    if (reduced < 0.0) reduced += 2.0 * PI;
    
    /* Reduce to first quadrant for 287 range limitations */
    if (reduced > PIOVER4 && reduced <= 3.0 * PIOVER4) {
        return sin(PI / 2.0 - reduced);
    } else if (reduced > 3.0 * PIOVER4 && reduced <= 5.0 * PIOVER4) {
        return -cos(reduced - PI);
    } else if (reduced > 5.0 * PIOVER4 && reduced <= 7.0 * PIOVER4) {
        return -sin(reduced - 3.0 * PI / 2.0);
    } else {
        return cos(reduced);
    }
}

/* Initialize FPU to optimal state for precision */
void init_fpu_precision(void) {
    if (g_has_287) {
        unsigned short control_word;
        
        _asm {
            finit                   /* Initialize FPU */
            fstcw   control_word    /* Get control word */
        }
        
        /* Set to extended precision (64-bit) and round to nearest */
        control_word &= ~0x0300;  /* Clear precision bits */
        control_word |= 0x0300;   /* Set extended precision */
        
        _asm {
            fldcw control_word
            fclex               /* Clear exceptions */
        }
    }
}

/* Window function generator using 287-optimized trigonometry */
void generate_window_function(float far *window, int N, int window_type) {
    int i;
    double angle, cos_val;
    const double PI = 3.14159265358979323846;
    
    switch(window_type) {
        case 0: /* Rectangular */
            for (i = 0; i < N; i++) {
                window[i] = 1.0f;
            }
            break;
            
        case 1: /* Hamming */
            for (i = 0; i < N; i++) {
                angle = 2.0 * PI * i / (N - 1);
                cos_val = cos_287_safe(angle);
                window[i] = 0.54f - 0.46f * (float)cos_val;
            }
            break;
            
        case 2: /* Hanning */
            for (i = 0; i < N; i++) {
                angle = 2.0 * PI * i / (N - 1);
                cos_val = cos_287_safe(angle);
                window[i] = 0.5f * (1.0f - (float)cos_val);
            }
            break;
            
        case 3: /* Blackman */
            for (i = 0; i < N; i++) {
                angle = 2.0 * PI * i / (N - 1);
                cos_val = cos_287_safe(angle);
                window[i] = 0.42f - 0.5f * (float)cos_val + 
                           0.08f * (float)cos_287_safe(2.0 * angle);
            }
            break;
    }
}

/* FFT Configuration Menu */
int fft_configuration_menu(void) {
    int choice;
    int done = 0;
    int temp_value;
    float temp_float;
    char *window_names[] = {"Rectangular", "Hamming", "Hanning", "Blackman"};
    char *format_names[] = {"dB Magnitude", "Linear Magnitude", "Power Spectrum"};
    int valid_sizes[] = {64, 128, 256, 512, 1024};
    int num_sizes = 5;
    int i, size_index;
    
    while (!done) {
        clrscr();
        printf("\n\nFFT Configuration\n");
        printf("=================\n\n");
        
        printf("Current Settings:\n");
        printf("  Input points: %d\n", g_fft_config.input_points);
        printf("  Output points: %d\n", g_fft_config.output_points);
        printf("  Window: %s\n", window_names[g_fft_config.window_type]);
        printf("  Zero padding: %s\n", g_fft_config.zero_pad ? "Yes" : "No");
        printf("  Remove DC: %s\n", g_fft_config.dc_remove ? "Yes" : "No");
        printf("  Output format: %s\n", format_names[g_fft_config.output_format]);
        printf("  Peak centering: %s\n", g_fft_config.peak_centering ? "Yes" : "No");
        if (g_fft_config.custom_sample_rate > 0.0) {
            printf("  Sample rate: %.2f Hz (custom)\n", g_fft_config.custom_sample_rate);
        } else {
            printf("  Sample rate: Auto-detect\n");
        }
        
        printf("\nMemory usage: ~%dKB working space\n", 
               (g_fft_config.input_points * 8 + 1023) / 1024);
        
        printf("\nOptions:\n");
        printf("1. Input Size [64|128|256|512|1024]\n");
        printf("2. Output Resolution [32|64|128|256|512|1024]\n");
        printf("3. Window Function\n");
        printf("4. Processing Options\n");
        printf("5. Sample Rate\n");
        printf("6. Execute FFT\n");
        printf("0. Return to Math Menu\n\n");
        
        printf("Choice: ");
        choice = getch();
        
        switch(choice) {
            case '1':
                clrscr();
                printf("\nSelect Input Size:\n");
                for (i = 0; i < num_sizes; i++) {
                    printf("%d. %d points\n", i+1, valid_sizes[i]);
                }
                printf("Choice (1-%d): ", num_sizes);
                scanf("%d", &temp_value);
                if (temp_value >= 1 && temp_value <= num_sizes) {
                    g_fft_config.input_points = valid_sizes[temp_value - 1];
                    /* Ensure output <= input */
                    if (g_fft_config.output_points > g_fft_config.input_points) {
                        g_fft_config.output_points = g_fft_config.input_points;
                    }
                }
                break;
                
            case '2':
                clrscr();
                printf("\nSelect Output Resolution:\n");
                for (i = 0; i < num_sizes; i++) {
                    if (valid_sizes[i] <= g_fft_config.input_points) {
                        printf("%d. %d points\n", i+1, valid_sizes[i]);
                    }
                }
                printf("Choice: ");
                scanf("%d", &temp_value);
                if (temp_value >= 1 && temp_value <= num_sizes) {
                    int new_size = valid_sizes[temp_value - 1];
                    if (new_size <= g_fft_config.input_points) {
                        g_fft_config.output_points = new_size;
                    }
                }
                break;
                
            case '3':
                clrscr();
                printf("\nSelect Window Function:\n");
                printf("1. Rectangular (no windowing)\n");
                printf("2. Hamming (good general purpose)\n");
                printf("3. Hanning (smooth frequency response)\n");
                printf("4. Blackman (excellent sidelobe suppression)\n");
                printf("Choice (1-4): ");
                scanf("%d", &temp_value);
                if (temp_value >= 1 && temp_value <= 4) {
                    g_fft_config.window_type = temp_value - 1;
                }
                break;
                
            case '4':
                clrscr();
                printf("\nProcessing Options:\n");
                printf("1. Zero padding: %s\n", g_fft_config.zero_pad ? "ON" : "OFF");
                printf("2. Remove DC: %s\n", g_fft_config.dc_remove ? "ON" : "OFF");
                printf("3. Output format: %s\n", format_names[g_fft_config.output_format]);
                printf("4. Peak centering: %s\n", g_fft_config.peak_centering ? "ON" : "OFF");
                printf("\nToggle option (1-4): ");
                scanf("%d", &temp_value);
                switch(temp_value) {
                    case 1: g_fft_config.zero_pad = !g_fft_config.zero_pad; break;
                    case 2: g_fft_config.dc_remove = !g_fft_config.dc_remove; break;
                    case 3: 
                        g_fft_config.output_format = (g_fft_config.output_format + 1) % 3;
                        break;
                    case 4: g_fft_config.peak_centering = !g_fft_config.peak_centering; break;
                }
                break;
                
            case '5':
                clrscr();
                printf("\nSample Rate Configuration:\n");
                printf("Current: ");
                if (g_fft_config.custom_sample_rate > 0.0) {
                    printf("%.2f Hz (custom)\n", g_fft_config.custom_sample_rate);
                } else {
                    printf("Auto-detect\n");
                }
                printf("\n1. Auto-detect from measurement settings\n");
                printf("2. Enter custom sample rate\n");
                printf("Choice: ");
                scanf("%d", &temp_value);
                if (temp_value == 1) {
                    g_fft_config.custom_sample_rate = 0.0;
                } else if (temp_value == 2) {
                    printf("Enter sample rate (Hz): ");
                    scanf("%f", &temp_float);
                    if (temp_float > 0.0) {
                        g_fft_config.custom_sample_rate = temp_float;
                    }
                }
                break;
                
            case '6':
                return 1;  /* Execute FFT */
                
            case '0':
            case 27:  /* ESC */
                return 0;  /* Cancel */
        }
    }
    
    return 0;
}

/* FFT - Fast Fourier Transform with configuration */
void perform_fft(void) {
    /* Show configuration menu first */
    if (!fft_configuration_menu()) {
        return;  /* User cancelled */
    }
    
    /* Execute FFT with current configuration */
    execute_fft_with_config();
}

/* Core FFT execution with configuration parameters */
void execute_fft_with_config(void) {
    int slot, i, j, k;
    int N, pow2;
    float far *real_data;
    float far *imag_data;
    float far *magnitude;
    float far *window_data;
    float sample_rate;
    float freq_resolution;
    float twiddle_real, twiddle_imag;
    float temp_real, temp_imag;
    float max_mag = 0.0;
    int target_slot = -1;
    int le, le2;
    float ur, ui, sr, si;
    int ip;
    int dft_size;
    float sum_real, sum_imag;
    float angle, c, s;
    float max_db = -120.0;
    int peak_index = 0;
    float db_value;
    float far *db_data;
    int center_position, shift_amount, source_index;
    float dc_sum = 0.0;
    int actual_input_size;
    char *window_names[] = {"Rectangular", "Hamming", "Hanning", "Blackman"};
    char *format_names[] = {"dB Magnitude", "Linear Magnitude", "Power Spectrum"};
    float mag_linear;
    
    clrscr();
    printf("\n\nFFT Execution\n");
    printf("=============\n\n");
    
    printf("Select source slot (0-9): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 9 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    /* Use configured input size, limited by available data */
    actual_input_size = g_system->modules[slot].module_data_count;
    if (actual_input_size > g_fft_config.input_points) {
        actual_input_size = g_fft_config.input_points;
    }
    
    /* Determine FFT size (next power of 2 if zero padding enabled) */
    N = g_fft_config.input_points;
    if (g_fft_config.zero_pad) {
        /* Find next power of 2 >= input_points */
        int temp = N;
        pow2 = 0;
        while (temp > 1) {
            temp >>= 1;
            pow2++;
        }
        if ((1 << pow2) < N) pow2++;
        N = 1 << pow2;
    } else {
        /* Find power of 2 for configured size */
        int temp = N;
        pow2 = 0;
        while (temp > 1) {
            temp >>= 1;
            pow2++;
        }
    }
    
    printf("\nConfiguration: %d input -> %d FFT -> %d output points\n", 
           actual_input_size, N, g_fft_config.output_points);
    
    /* Sample rate calculation */
    if (g_fft_config.custom_sample_rate > 0.0) {
        sample_rate = g_fft_config.custom_sample_rate;
        printf("Sample rate: %.2f Hz (custom)\n", sample_rate);
    } else {
        sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
        printf("Sample rate: %.2f Hz (auto-detect)\n", sample_rate);
    }
    
    freq_resolution = sample_rate / N;
    printf("Frequency resolution: %.3f Hz\n", freq_resolution);
    
    /* Allocate memory for FFT */
    real_data = (float far *)_fmalloc(N * sizeof(float));
    imag_data = (float far *)_fmalloc(N * sizeof(float));
    magnitude = (float far *)_fmalloc(g_fft_config.output_points * sizeof(float));
    window_data = (float far *)_fmalloc(actual_input_size * sizeof(float));
    
    if (!real_data || !imag_data || !magnitude || !window_data) {
        printf("\nInsufficient memory for FFT!\n");
        if (real_data) _ffree(real_data);
        if (imag_data) _ffree(imag_data);
        if (magnitude) _ffree(magnitude);
        if (window_data) _ffree(window_data);
        printf("Press any key...");
        getch();
        return;
    }
    
    /* Generate window function */
    printf("\nApplying %s window...\n", window_names[g_fft_config.window_type]);
    
    /* Initialize FPU for optimal precision */
    init_fpu_precision();
    
    /* Generate window coefficients */
    generate_window_function(window_data, actual_input_size, g_fft_config.window_type);
    
    /* Copy input data with windowing and zero padding */
    for (i = 0; i < N; i++) {
        if (i < actual_input_size) {
            real_data[i] = g_system->modules[slot].module_data[i] * window_data[i];
        } else {
            real_data[i] = 0.0;  /* Zero padding */
        }
        imag_data[i] = 0.0;
    }
    
    /* Remove DC component if requested */
    if (g_fft_config.dc_remove) {
        printf("Removing DC component...\n");
        dc_sum = 0.0;
        for (i = 0; i < actual_input_size; i++) {
            dc_sum += real_data[i];
        }
        dc_sum /= actual_input_size;
        for (i = 0; i < actual_input_size; i++) {
            real_data[i] -= dc_sum;
        }
    }
    
    printf("Performing FFT...\n");
    
    if (g_has_287) {
        printf("Using 287 coprocessor...\n");
        
        /* Bit-reversal permutation */
        j = 0;
        for (i = 0; i < N - 1; i++) {
            if (i < j) {
                temp_real = real_data[i];
                real_data[i] = real_data[j];
                real_data[j] = temp_real;
                
                temp_imag = imag_data[i];
                imag_data[i] = imag_data[j];
                imag_data[j] = temp_imag;
            }
            k = N / 2;
            while (k <= j) {
                j -= k;
                k /= 2;
            }
            j += k;
        }
        
        /* Butterfly operations */
        for (i = 1; i <= pow2; i++) {
            le = 1 << i;
            le2 = le / 2;
            ur = 1.0;
            ui = 0.0;
            sr = cos(3.14159 / le2);
            si = -sin(3.14159 / le2);
            
            for (j = 0; j < le2; j++) {
                for (k = j; k < N; k += le) {
                    ip = k + le2;
                    
                    temp_real = real_data[ip] * ur - imag_data[ip] * ui;
                    temp_imag = real_data[ip] * ui + imag_data[ip] * ur;
                    
                    real_data[ip] = real_data[k] - temp_real;
                    imag_data[ip] = imag_data[k] - temp_imag;
                    real_data[k] += temp_real;
                    imag_data[k] += temp_imag;
                }
                
                temp_real = ur;
                ur = temp_real * sr - ui * si;
                ui = temp_real * si + ui * sr;
            }
        }
    } else {
        /* Software DFT for systems without math coprocessor */
        printf("Software mode (this may take a while)...\n");
        
        dft_size = N > 64 ? 64 : N;
        
        for (k = 0; k < dft_size/2; k++) {
            sum_real = 0.0;
            sum_imag = 0.0;
            
            for (j = 0; j < dft_size; j++) {
                angle = -2.0 * 3.14159 * k * j / dft_size;
                c = 1.0 - angle*angle/2.0;  /* cos approximation */
                s = angle;                   /* sin approximation */
                
                sum_real += real_data[j] * c;
                sum_imag += real_data[j] * s;
            }
            
            magnitude[k] = sqrt(sum_real * sum_real + sum_imag * sum_imag);
            if (magnitude[k] > max_mag) max_mag = magnitude[k];
        }
        N = dft_size;  /* Adjust size for software mode */
    }
    
    /* Calculate spectrum based on output format */
    printf("Calculating %s...\n", format_names[g_fft_config.output_format]);
    
    if (g_has_287) {
        for (i = 0; i < g_fft_config.output_points && i < N/2; i++) {
            mag_linear = sqrt(real_data[i] * real_data[i] + 
                             imag_data[i] * imag_data[i]) / (N/2);
            
            switch(g_fft_config.output_format) {
                case 0: /* dB Magnitude */
                    if (mag_linear > 1e-10) {
                        magnitude[i] = 20.0 * log10(mag_linear);
                    } else {
                        magnitude[i] = -200.0;  /* Very low dB value */
                    }
                    /* For dB, find peak in dB domain for consistent comparison */
                    if (i > 0 && magnitude[i] > max_mag) {  /* Skip DC for peak finding */
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;
                    
                case 1: /* Linear Magnitude */
                    magnitude[i] = mag_linear;
                    if (i > 0 && magnitude[i] > max_mag) {  /* Skip DC for peak finding */
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;
                    
                case 2: /* Power Spectrum */
                    magnitude[i] = mag_linear * mag_linear;
                    if (i > 0 && magnitude[i] > max_mag) {  /* Skip DC for peak finding */
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;
            }
        }
    }
    
    /* Find available slot for results */
    for (i = 0; i < 10; i++) {
        if (!g_system->modules[i].enabled) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot < 0) {
        target_slot = slot;
        printf("\nOverwriting source data with FFT results...\n");
    } else {
        printf("\nStoring FFT in slot %d...\n", target_slot);
        g_system->modules[target_slot].enabled = 1;
        g_system->modules[target_slot].module_type = MOD_NONE;
        strcpy(g_system->modules[target_slot].description, "FFT Result");
        g_system->modules[target_slot].gpib_address = 0;  /* No GPIB for computed data */
    }
    
    /* Allocate buffer and store results */
    if (!g_system->modules[target_slot].module_data) {
        allocate_module_buffer(target_slot, g_fft_config.output_points);
    }
    
    /* Apply peak centering if enabled */
    if (g_fft_config.peak_centering && peak_index > 0) {
        int center_position = g_fft_config.output_points / 2;
        int shift_amount = peak_index - center_position;
        int source_index;
        
        printf("Centering peak (index %d) to center position (%d)...\n", peak_index, center_position);
        
        /* Store results with circular shift to center peak */
        for (i = 0; i < g_fft_config.output_points; i++) {
            source_index = (i + shift_amount);
            if (source_index >= g_fft_config.output_points) {
                source_index -= g_fft_config.output_points;
            } else if (source_index < 0) {
                source_index += g_fft_config.output_points;
            }
            g_system->modules[target_slot].module_data[i] = magnitude[source_index];
        }
    } else {
        /* Store results directly - no centering */
        for (i = 0; i < g_fft_config.output_points; i++) {
            g_system->modules[target_slot].module_data[i] = magnitude[i];
        }
    }
    g_system->modules[target_slot].module_data_count = g_fft_config.output_points;
    
    /* Set up trace for display */
    if (target_slot >= 0 && target_slot < 10) {
        /* Set appropriate unit type based on output format */
        switch(g_fft_config.output_format) {
            case 0: /* dB Magnitude */
                g_traces[target_slot].unit_type = UNIT_DB;
                strcpy(g_traces[target_slot].label, "FFT (dB)");
                break;
            case 1: /* Linear Magnitude */
                g_traces[target_slot].unit_type = UNIT_VOLTAGE;
                strcpy(g_traces[target_slot].label, "FFT (Linear)");
                break;
            case 2: /* Power Spectrum */
                g_traces[target_slot].unit_type = UNIT_POWER;  /* Use power spectrum units */
                strcpy(g_traces[target_slot].label, "FFT (Power)");
                break;
        }
        
        g_traces[target_slot].x_scale = freq_resolution;  /* Hz per sample */
        
        /* Set frequency offset based on centering mode */
        if (g_fft_config.peak_centering && peak_index > 0) {
            /* Centered mode: peak frequency becomes the center reference */
            float peak_frequency = peak_index * freq_resolution;
            int center_position = g_fft_config.output_points / 2;
            g_traces[target_slot].x_offset = peak_frequency - (center_position * freq_resolution);
        } else {
            /* Normal mode: start at 0 Hz */
            g_traces[target_slot].x_offset = 0.0;
        }
        
        /* CRITICAL: Connect trace data to module data */
        g_traces[target_slot].data = g_system->modules[target_slot].module_data;
        g_traces[target_slot].data_count = g_system->modules[target_slot].module_data_count;
        g_traces[target_slot].enabled = 1;
    }
    
    printf("\nFFT Complete!\n");
    printf("Configuration: %d->%d->%d points, %s window\n", 
           actual_input_size, N, g_fft_config.output_points, 
           window_names[g_fft_config.window_type]);
    printf("Peak: %.2f %s at %.1f Hz\n", max_mag,
           (g_fft_config.output_format == 0) ? "dB" : 
           (g_fft_config.output_format == 1) ? "V" : "V²",
           peak_index * freq_resolution);
    
    /* Clean up memory */
    _ffree(real_data);
    _ffree(imag_data);
    _ffree(magnitude);
    _ffree(window_data);
    
    printf("\nResults stored in slot %d\n", target_slot);
    printf("Frequency resolution: %.3f Hz per point\n", freq_resolution);
    printf("\nPress any key to continue...");
    getch();
}

/* Differentiation - Calculate dV/dt */
void perform_differentiation(void) {
    int slot, i;
    int target_slot = -1;
    float far *source_data;
    float far *result_data;
    int count;
    float dt;
    float scale_factor;
    
    clrscr();
    printf("\n\nDifferentiation (dV/dt)\n");
    printf("=======================\n\n");
    
    printf("Select source slot (0-9): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 9 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count < 2) {
        printf("\nInvalid slot or insufficient data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    source_data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    dt = g_control_panel.sample_rate_ms / 1000.0;  /* Convert to seconds */
    
    printf("\nDifferentiating %d samples...\n", count);
    printf("Time step: %.3f seconds\n", dt);
    
    /* Find available slot for results */
    for (i = 0; i < 10; i++) {
        if (!g_system->modules[i].enabled) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot < 0) {
        target_slot = slot;
        printf("\nOverwriting source data...\n");
    } else {
        printf("\nStoring result in slot %d...\n", target_slot);
        g_system->modules[target_slot].enabled = 1;
        g_system->modules[target_slot].module_type = MOD_NONE;
        strcpy(g_system->modules[target_slot].description, "dV/dt");
        g_system->modules[target_slot].gpib_address = 0;
    }
    
    if (!g_system->modules[target_slot].module_data) {
        allocate_module_buffer(target_slot, count);
    }
    result_data = g_system->modules[target_slot].module_data;
    
    if (g_has_287) {
        /* Use central difference method for better accuracy */
        scale_factor = 1.0 / dt;
        
        /* Forward difference for first point */
        result_data[0] = (source_data[1] - source_data[0]) * scale_factor;
        
        /* Central difference for interior points */
        for (i = 1; i < count - 1; i++) {
            result_data[i] = (source_data[i+1] - source_data[i-1]) * 0.5 * scale_factor;
        }
        
        /* Backward difference for last point */
        result_data[count-1] = (source_data[count-1] - source_data[count-2]) * scale_factor;
    } else {
        /* Simple forward difference for software mode */
        for (i = 0; i < count - 1; i++) {
            result_data[i] = (source_data[i+1] - source_data[i]) / dt;
        }
        result_data[count-1] = result_data[count-2];  /* Duplicate last */
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    /* Set up trace for display with proper derivative units */
    if (target_slot >= 0 && target_slot < 10) {
        g_traces[target_slot].unit_type = UNIT_DERIVATIVE;  /* V/s units */
        g_traces[target_slot].x_scale = 1.0;  /* Sample per point */
        g_traces[target_slot].x_offset = 0.0;  /* Start at sample 0 */
        strcpy(g_traces[target_slot].label, "dV/dt");
        
        /* CRITICAL: Connect trace data to module data */
        g_traces[target_slot].data = g_system->modules[target_slot].module_data;
        g_traces[target_slot].data_count = g_system->modules[target_slot].module_data_count;
        g_traces[target_slot].enabled = 1;
        g_traces[target_slot].slot = target_slot;
        g_traces[target_slot].color = 0x0E;  /* Yellow color for derivative */
    }
    
    printf("\nDifferentiation complete!\n");
    printf("Units changed from V to V/s\n");
    printf("Results stored in slot %d with proper V/s units\n", target_slot);
    printf("\nPress any key to continue...");
    getch();
}

/* Integration - Calculate cumulative integral */
void perform_integration(void) {
    int slot, i;
    int target_slot = -1;
    float far *source_data;
    float far *result_data;
    int count;
    float dt;
    float sum;
    kahan_accumulator acc;
    
    clrscr();
    printf("\n\nIntegration\n");
    printf("===========\n\n");
    
    printf("Select source slot (0-9): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 9 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count < 2) {
        printf("\nInvalid slot or insufficient data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    source_data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    dt = g_control_panel.sample_rate_ms / 1000.0;  /* Convert to seconds */
    
    printf("\nIntegrating %d samples...\n", count);
    printf("Time step: %.3f seconds\n", dt);
    
    /* Find available slot for results */
    for (i = 0; i < 10; i++) {
        if (!g_system->modules[i].enabled) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot < 0) {
        target_slot = slot;
        printf("\nOverwriting source data...\n");
    } else {
        printf("\nStoring result in slot %d...\n", target_slot);
        g_system->modules[target_slot].enabled = 1;
        g_system->modules[target_slot].module_type = MOD_NONE;
        strcpy(g_system->modules[target_slot].description, "Integral");
        g_system->modules[target_slot].gpib_address = 0;
    }
    
    if (!g_system->modules[target_slot].module_data) {
        allocate_module_buffer(target_slot, count);
    }
    result_data = g_system->modules[target_slot].module_data;
    
    /* Trapezoidal rule integration with Kahan summation for precision */
    result_data[0] = 0.0;  /* Start integration at zero */
    
    kahan_init(&acc);
    
    init_fpu_precision();  /* Set optimal FPU state */
    
    if (g_has_287) {
        /* High-precision trapezoidal rule with compensated summation */
        for (i = 1; i < count; i++) {
            double trapezoid_area = (double)(source_data[i] + source_data[i-1]) * 0.5 * dt;
            kahan_add(&acc, trapezoid_area);
            result_data[i] = (float)acc.sum;
        }
    } else {
        /* Software mode with Kahan summation */
        for (i = 1; i < count; i++) {
            double rectangle_area = (double)source_data[i] * dt;
            kahan_add(&acc, rectangle_area);
            result_data[i] = (float)acc.sum;
        }
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nIntegration complete!\n");
    printf("Units changed from V to V*s\n");
    printf("Final value: %.6f V*s\n", result_data[count-1]);
    printf("\nPress any key to continue...");
    getch();
}

/* Smoothing - Moving average filter */
void perform_smoothing(void) {
    int slot, i, j;
    int target_slot = -1;
    float far *source_data;
    float far *result_data;
    int count;
    int window_size;
    float sum;
    int half_window;
    int start, end;
    kahan_accumulator window_acc;
    
    clrscr();
    printf("\n\nSmoothing Filter\n");
    printf("================\n\n");
    
    printf("Select source slot (0-9): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 9 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count < 3) {
        printf("\nInvalid slot or insufficient data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    printf("Enter window size (3-21, odd numbers only): ");
    scanf("%d", &window_size);
    
    if (window_size < 3 || window_size > 21 || window_size % 2 == 0) {
        printf("\nInvalid window size!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    source_data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    half_window = window_size / 2;
    
    printf("\nSmoothing %d samples with window size %d...\n", count, window_size);
    
    /* Find available slot for results */
    for (i = 0; i < 10; i++) {
        if (!g_system->modules[i].enabled) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot < 0) {
        target_slot = slot;
        printf("\nOverwriting source data...\n");
    } else {
        printf("\nStoring result in slot %d...\n", target_slot);
        g_system->modules[target_slot].enabled = 1;
        g_system->modules[target_slot].module_type = MOD_NONE;
        strcpy(g_system->modules[target_slot].description, "Smoothed");
        g_system->modules[target_slot].gpib_address = 0;
    }
    
    if (!g_system->modules[target_slot].module_data) {
        allocate_module_buffer(target_slot, count);
    }
    result_data = g_system->modules[target_slot].module_data;
    
    /* Apply moving average filter with Kahan summation for precision */
    init_fpu_precision();  /* Set optimal FPU state */
    
    for (i = 0; i < count; i++) {
        kahan_init(&window_acc);
        
        /* Determine window bounds with edge handling */
        start = i - half_window;
        end = i + half_window;
        
        if (start < 0) start = 0;
        if (end >= count) end = count - 1;
        
        /* Calculate average within window using compensated summation */
        for (j = start; j <= end; j++) {
            kahan_add(&window_acc, (double)source_data[j]);
        }
        
        result_data[i] = (float)(window_acc.sum / (end - start + 1));
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nSmoothing complete!\n");
    printf("Noise reduction applied with %d-point moving average\n", window_size);
    printf("\nPress any key to continue...");
    getch();
}
