/*
 * TM5000 GPIB Control System - Mathematical Functions Module
 * Version 3.2
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
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
    float max_db = -120.0;
    int peak_index = 0;
    float db_value;
    float far *db_data;
    int center_position, shift_amount, source_index;
    
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
    if (N > 0) {
        freq_resolution = sample_rate / N;
    } else {
        freq_resolution = 1.0;  /* Fallback to prevent division by zero */
    }
    
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
    
    /* Initialize FPU for optimal precision */
    init_fpu_precision();
    
    for (i = 0; i < N; i++) {
        if (g_has_287) {
            /* Use 287-safe cosine with proper range reduction */
            double angle = 2.0 * 3.14159265358979323846 * i / (N - 1);
            window = 0.54 - 0.46 * cos_287_safe(angle);
        } else {
            /* Improved approximation using Taylor series */
            double t = 2.0 * (double)i / (double)(N - 1) - 1.0;  /* Normalize to [-1,1] */
            double t2 = t * t;
            /* cos(πt) ≈ 1 - (πt)²/2 + (πt)⁴/24 for |t| < 1 */
            double pit = 3.14159265358979323846 * t;
            double pit2 = pit * pit;
            double cos_approx = 1.0 - pit2 / 2.0 + pit2 * pit2 / 24.0;
            window = 0.54 - 0.46 * cos_approx;
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
    
    /* Convert magnitude to dB and find peak for centering */
    max_db = -120.0;  /* Start with very low dB value */
    peak_index = 0;
    
    /* Allocate temporary array for dB conversion */
    db_data = (float far *)_fmalloc((N/2) * sizeof(float));
    if (!db_data) {
        printf("\nInsufficient memory for dB conversion!\n");
        /* Fallback: store magnitude directly */
        for (i = 0; i < N/2 && i < g_system->modules[target_slot].module_data_size; i++) {
            g_system->modules[target_slot].module_data[i] = magnitude[i];
        }
        g_system->modules[target_slot].module_data_count = N/2;
        return;
    }
    
    /* Convert to dB and find peak */
    for (i = 0; i < N/2; i++) {
        /* Convert magnitude to dB: 20*log10(magnitude) */
        if (magnitude[i] > 0.0) {
            if (g_has_287) {
                /* Use 287 coprocessor for accurate log calculation */
                db_value = 20.0 * log10(magnitude[i]);
            } else {
                /* Software approximation for log10 */
                float temp = magnitude[i];
                int exp = 0;
                while (temp >= 10.0) { temp /= 10.0; exp++; }
                while (temp < 1.0) { temp *= 10.0; exp--; }
                /* log10(temp) ≈ (temp-1)/temp for temp near 1 */
                db_value = 20.0 * (exp + (temp - 1.0) / temp);
            }
        } else {
            db_value = -120.0;  /* Very low value for zero magnitude */
        }
        
        db_data[i] = db_value;
        
        /* Track peak for centering (skip DC component at i=0) */
        if (i > 0 && db_value > max_db) {
            max_db = db_value;
            peak_index = i;
        }
    }
    
    /* Center the FFT data around the peak */
    center_position = (N/2) / 2;  /* Center of our array */
    shift_amount = center_position - peak_index;
    
    /* Copy data with circular shift to center peak */
    for (i = 0; i < N/2 && i < g_system->modules[target_slot].module_data_size; i++) {
        source_index = (i - shift_amount + N/2) % (N/2);
        g_system->modules[target_slot].module_data[i] = db_data[source_index];
    }
    g_system->modules[target_slot].module_data_count = N/2;
    
    /* Clean up temporary array */
    _ffree(db_data);
    
    /* Set up trace for dB display with centered frequency scale */
    if (target_slot >= 0 && target_slot < 10) {
        g_traces[target_slot].unit_type = UNIT_DB;  /* Y-axis in dB */
        g_traces[target_slot].x_scale = freq_resolution;  /* Hz per sample */
        /* X-axis now represents frequency with peak at center */
        g_traces[target_slot].x_offset = -center_position * freq_resolution;  /* Offset to show negative frequencies */
        strcpy(g_traces[target_slot].label, "FFT (dB)");
        
        /* CRITICAL: Connect trace data to module data */
        g_traces[target_slot].data = g_system->modules[target_slot].module_data;
        g_traces[target_slot].data_count = g_system->modules[target_slot].module_data_count;
        g_traces[target_slot].enabled = 1;
    }
    
    printf("\nFFT Complete!\n");
    printf("Peak: %.1f dB at %.1f Hz (centered)\n", max_db, peak_index * freq_resolution);
    
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
