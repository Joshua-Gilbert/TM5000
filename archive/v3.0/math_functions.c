/*
 * TM5000 GPIB Control System - Mathematical Functions Module
 * Version 3.0
 * Complete implementation extracted from TM5000L.c
 */

#include "tm5000.h"
#include "math_functions.h"
#include <math.h>

/* FFT - Fast Fourier Transform */
void perform_fft(void) {
    int slot, i, j, k;
    int N, pow2;
    float far *real_data;
    float far *imag_data;
    float far *magnitude;
    float sample_rate;
    float freq_resolution;
    float twiddle_real, twiddle_imag;
    float temp_real, temp_imag;
    float max_mag = 0.0;
    int target_slot = -1;
    float window;
    int le, le2;
    float ur, ui, sr, si;
    int ip;
    int dft_size;
    float sum_real, sum_imag;
    float angle, c, s;
    
    clrscr();
    printf("\n\nFFT - Frequency Analysis\n");
    printf("========================\n\n");
    
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
    
    /* Find largest power of 2 that fits */
    N = 1;
    pow2 = 0;
    while (N < g_system->modules[slot].module_data_count && N < 512) {
        N *= 2;
        pow2++;
    }
    N /= 2;  /* Use largest power of 2 that fits */
    pow2--;
    
    printf("\nProcessing %d samples (2^%d)...\n", N, pow2);
    
    sample_rate = 1000.0 / g_control_panel.sample_rate_ms;  /* Hz */
    freq_resolution = sample_rate / N;
    
    printf("Sample rate: %.2f Hz\n", sample_rate);
    printf("Frequency resolution: %.3f Hz\n", freq_resolution);
    
    /* Allocate memory for FFT */
    real_data = (float far *)_fmalloc(N * sizeof(float));
    imag_data = (float far *)_fmalloc(N * sizeof(float));
    magnitude = (float far *)_fmalloc((N/2) * sizeof(float));
    
    if (!real_data || !imag_data || !magnitude) {
        printf("\nInsufficient memory for FFT!\n");
        if (real_data) _ffree(real_data);
        if (imag_data) _ffree(imag_data);
        if (magnitude) _ffree(magnitude);
        printf("Press any key...");
        getch();
        return;
    }
    
    /* Apply Hamming window to reduce spectral leakage */
    printf("\nApplying Hamming window...\n");
    for (i = 0; i < N; i++) {
        if (g_has_287) {
            window = 0.54 - 0.46 * cos(2.0 * 3.14159 * i / (N - 1));
        } else {
            /* Approximation for systems without math coprocessor */
            window = 0.54 - 0.46 * (1.0 - 2.0 * (float)i / (float)(N-1));
        }
        real_data[i] = g_system->modules[slot].module_data[i] * window;
        imag_data[i] = 0.0;
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
    
    /* Calculate magnitude spectrum */
    if (g_has_287) {
        for (i = 0; i < N/2; i++) {
            magnitude[i] = sqrt(real_data[i] * real_data[i] + 
                               imag_data[i] * imag_data[i]) / (N/2);
            if (magnitude[i] > max_mag) max_mag = magnitude[i];
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
        allocate_module_buffer(target_slot, N/2);
    }
    
    for (i = 0; i < N/2 && i < g_system->modules[target_slot].module_data_size; i++) {
        g_system->modules[target_slot].module_data[i] = magnitude[i];
    }
    g_system->modules[target_slot].module_data_count = N/2;
    
    printf("\nFFT Complete!\n");
    printf("DC component: %.3f\n", magnitude[0]);
    printf("Max magnitude: %.3f at %.1f Hz\n", max_mag, 
           freq_resolution * (max_mag > 0 ? 1 : 0));
    
    /* Clean up memory */
    _ffree(real_data);
    _ffree(imag_data);
    _ffree(magnitude);
    
    printf("\nResults stored in slot %d\n", target_slot);
    printf("Note: X-axis now represents frequency (Hz)\n");
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
    
    printf("\nDifferentiation complete!\n");
    printf("Units changed from V to V/s\n");
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
    
    /* Trapezoidal rule integration */
    sum = 0.0;
    result_data[0] = 0.0;  /* Start integration at zero */
    
    if (g_has_287) {
        for (i = 1; i < count; i++) {
            sum += (source_data[i] + source_data[i-1]) * 0.5 * dt;
            result_data[i] = sum;
        }
    } else {
        for (i = 1; i < count; i++) {
            sum += source_data[i] * dt;
            result_data[i] = sum;
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
    
    /* Apply moving average filter */
    for (i = 0; i < count; i++) {
        sum = 0.0;
        
        /* Determine window bounds with edge handling */
        start = i - half_window;
        end = i + half_window;
        
        if (start < 0) start = 0;
        if (end >= count) end = count - 1;
        
        /* Calculate average within window */
        for (j = start; j <= end; j++) {
            sum += source_data[j];
        }
        
        result_data[i] = sum / (end - start + 1);
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nSmoothing complete!\n");
    printf("Noise reduction applied with %d-point moving average\n", window_size);
    printf("\nPress any key to continue...");
    getch();
}