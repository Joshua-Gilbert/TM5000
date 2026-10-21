/*
 * TM5000 math menu operations: FFT, differentiation, integration, smoothing.
 * Results go to the first free slot (or overwrite the source) as is_result traces.
 */

#include "tm5000.h"
#include "math_functions.h"
#include <math.h>

/* 286/287 assembly routines */
extern void init_fpu_287_(void);
extern void fft_butterfly_asm_(float far *real_data, float far *imag_data, 
                               int k, int ip, float ur, float ui);
extern void fft_bitreverse_asm_(float far *real_data, float far *imag_data, int N);
extern double fast_sqrt_287_simple_(double x);
extern double fast_mult_287_(double a, double b);
extern double fast_div_287_(double a, double b);

extern void fast_memcpy_286(void *dest, void *src, unsigned int count);
extern void fast_fmemcpy_286(void far *dest, void far *src, unsigned int count);

/* Kahan (compensated) summation accumulator */
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

/* Gives a result slot room for count samples. A big-enough buffer is kept: an in-place
   operation is still reading it. Returns 1 on success, 0 if out of memory. */
int ensure_result_buffer(int slot, unsigned int count) {
    if (g_system->modules[slot].module_data &&
        g_system->modules[slot].module_data_size >= count) {
        return 1;
    }
    return allocate_module_buffer(slot, count);
}

/* Cosine with explicit reduction to [0, 2*PI) and quadrant folding before libm sin/cos. */
double cos_287_safe(double x) {
    const double PI = 3.14159265358979323846;
    const double PIOVER4 = 0.78539816339744830962;
    double reduced = fmod(x, 2.0 * PI);
    
    if (reduced < 0.0) reduced += 2.0 * PI;
    
    /* Fold so sin/cos see |arg| <= PI/4 (the last range, > 7*PI/4, is not folded) */
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

/* Reset the 287 and select 64-bit extended precision (finit leaves round-to-nearest). */
void init_fpu_precision(void) {
    if (g_has_287) {
        unsigned short control_word;

        _asm {
            finit
            fstcw   control_word
        }

        control_word &= ~0x0300;  /* PC bits 8-9 */
        control_word |= 0x0300;   /* 11b = 64-bit extended */
        
        _asm {
            fldcw control_word
            fclex
        }
    }
}

/* window_type indexes window_names[]: 0 rect, 1 Hamming, 2 Hanning, 3 Blackman */
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

/* Returns 1 to run the FFT, 0 on cancel. */
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

void perform_fft(void) {
    if (!fft_configuration_menu()) {
        return;
    }

    execute_fft_with_config();
}

/* FFT of a user-chosen slot using g_fft_config. */
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
    float out_bin_hz;   /* Hz per displayed output bin */

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
        /* log2 of configured size (already a power of 2) */
        int temp = N;
        pow2 = 0;
        while (temp > 1) {
            temp >>= 1;
            pow2++;
        }
    }

    /* No 287: the DFT transforms only the first dft_size samples, so window just those */
    dft_size = N > 64 ? 64 : N;
    if (!g_has_287 && actual_input_size > dft_size) {
        actual_input_size = dft_size;
    }

    printf("\nConfiguration: %d input -> %d FFT -> %d output points\n", 
           actual_input_size, N, g_fft_config.output_points);
    
    if (g_fft_config.custom_sample_rate > 0.0) {
        sample_rate = g_fft_config.custom_sample_rate;
        printf("Sample rate: %.2f Hz (custom)\n", sample_rate);
    } else {
        sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
        printf("Sample rate: %.2f Hz (auto-detect)\n", sample_rate);
    }
    
    freq_resolution = sample_rate / N;
    out_bin_hz = freq_resolution;  /* reset below by each spectrum path */
    printf("Frequency resolution: %.3f Hz\n", freq_resolution);
    
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
    
    printf("\nApplying %s window...\n", window_names[g_fft_config.window_type]);

    init_fpu_precision();

    /* Scrub NaN (x != x) and clip extremes; this modifies the source slot in place */
    printf("Validating input data...\n");
    for (i = 0; i < actual_input_size; i++) {
        if (g_system->modules[slot].module_data[i] != g_system->modules[slot].module_data[i]) {
            printf("Warning: NaN detected in input data at sample %d, replacing with 0\n", i);
            g_system->modules[slot].module_data[i] = 0.0;
        }
        if (g_system->modules[slot].module_data[i] > 1e30 || 
            g_system->modules[slot].module_data[i] < -1e30) {
            printf("Warning: Extreme value detected at sample %d, clipping\n", i);
            if (g_system->modules[slot].module_data[i] > 0) {
                g_system->modules[slot].module_data[i] = 1e6;
            } else {
                g_system->modules[slot].module_data[i] = -1e6;
            }
        }
    }
    
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
        printf("Using 287 coprocessor with C implementation...\n");
        
        /* Bit-reversal permutation (C; the asm FFT routines above are not used) */
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
        
        /* Radix-2 butterflies */
        for (i = 1; i <= pow2; i++) {
            double angle;
            le = 1 << i;
            le2 = le / 2;
            ur = 1.0;
            ui = 0.0;
            
            /* Per-stage twiddle step (angle <= PI, so the guard never fires) */
            angle = 3.14159 / le2;
            if (angle > 1000.0 || angle < -1000.0) {
                printf("Warning: Extreme twiddle angle detected, using approximation\n");
                sr = 1.0 - angle*angle/2.0;  /* cos approximation */
                si = -angle;                  /* sin approximation */
            } else {
                sr = cos(angle);
                si = -sin(angle);
            }
            
            for (j = 0; j < le2; j++) {
                for (k = j; k < N; k += le) {
                    ip = k + le2;
                    
                    if (ip < N && k < N) {
                        temp_real = real_data[ip] * ur - imag_data[ip] * ui;
                        temp_imag = real_data[ip] * ui + imag_data[ip] * ur;
                        
                        real_data[ip] = real_data[k] - temp_real;
                        imag_data[ip] = imag_data[k] - temp_imag;
                        real_data[k] += temp_real;
                        imag_data[k] += temp_imag;
                    }
                }
                
                /* Rotate twiddle by one step */
                temp_real = ur;
                ur = temp_real * sr - ui * si;
                ui = temp_real * si + ui * sr;
                
                /* Recompute directly if the recurrence diverges */
                if (ur > 1e10 || ur < -1e10 || ui > 1e10 || ui < -1e10) {
                    printf("Warning: Twiddle factor overflow, resetting\n");
                    ur = cos(3.14159 * j / le2);
                    ui = -sin(3.14159 * j / le2);
                }
            }
            
            /* Progress indication for large FFTs */
            if (pow2 > 8 && (i % 2) == 0) {
                printf("Stage %d/%d completed...\n", i, pow2);
            }
        }
    } else {
        /* No 287: coarse DFT, at most 64 points */
        printf("Software mode (this may take a while)...\n");

        for (k = 0; k < dft_size/2; k++) {
            sum_real = 0.0;
            sum_imag = 0.0;

            /* Rotate a unit phasor by the bin's step: one cos/sin pair per bin */
            angle = -2.0 * 3.14159265 * k / dft_size;
            c = cos(angle);
            s = sin(angle);
            ur = 1.0;
            ui = 0.0;

            for (j = 0; j < dft_size; j++) {
                sum_real += real_data[j] * ur;
                sum_imag += real_data[j] * ui;

                temp_real = ur;
                ur = temp_real * c - ui * s;
                ui = temp_real * s + ui * c;
            }
            
            magnitude[k] = sqrt(sum_real * sum_real + sum_imag * sum_imag);
            if (magnitude[k] > max_mag) max_mag = magnitude[k];
        }
        N = dft_size;
    }

    printf("Calculating %s...\n", format_names[g_fft_config.output_format]);
    
    if (g_has_287) {
        int half = N / 2;                  /* number of meaningful bins for real input */
        int op = g_fft_config.output_points;
        int b, b0, b1;
        float m;

        /* Map the full 0..Nyquist span onto op bins, each taking the peak of its
           source group so narrow lines survive; x axis scaled to match. */
        out_bin_hz = (op > 0) ? (freq_resolution * (float)half / (float)op) : freq_resolution;

        for (i = 0; i < op; i++) {
            b0 = (int)(((long)i * half) / op);
            b1 = (int)((((long)i + 1) * half) / op);
            if (b1 <= b0) b1 = b0 + 1;
            if (b1 > half) b1 = half;

            mag_linear = 0.0;
            for (b = b0; b < b1; b++) {
                m = sqrt(real_data[b] * real_data[b] +
                         imag_data[b] * imag_data[b]) / half;   /* /(N/2): sine amplitude */
                if (m > mag_linear) mag_linear = m;  /* peak-preserving */
            }

            switch(g_fft_config.output_format) {
                case 0: /* dB Magnitude */
                    if (mag_linear > 1e-12) {  /* low threshold so low-amplitude bins still display */
                        magnitude[i] = 20.0 * log10(mag_linear);
                    } else {
                        magnitude[i] = -240.0;  /* = 20*log10(1e-12) */
                    }
                    if (i > 0 && magnitude[i] > max_mag) {  /* skip DC for peak finding */
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;

                case 1: /* Linear Magnitude */
                    magnitude[i] = mag_linear;
                    if (i > 0 && magnitude[i] > max_mag) {
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;

                case 2: /* Power Spectrum */
                    magnitude[i] = mag_linear * mag_linear;
                    if (i > 0 && magnitude[i] > max_mag) {
                        max_mag = magnitude[i];
                        peak_index = i;
                    }
                    break;
            }
        }
    } else {
        /* No 287: the DFT above left unnormalized linear magnitudes in the first N/2
           bins (N is now dft_size) and the rest unwritten: normalize, format, zero the tail. */
        int valid = N / 2;
        float best = -1e30;
        out_bin_hz = (N > 0) ? (sample_rate / N) : freq_resolution;
        printf("(no 287: FFT is approximate, %d-point)\n", N);
        for (i = 0; i < g_fft_config.output_points; i++) {
            float lin = (i < valid) ? (magnitude[i] / valid) : 0.0;
            switch (g_fft_config.output_format) {
                case 0:  magnitude[i] = (lin > 1e-12) ? (20.0 * log10(lin)) : -240.0; break;
                case 2:  magnitude[i] = lin * lin; break;
                default: magnitude[i] = lin; break;
            }
            if (i > 0 && i < valid && magnitude[i] > best) { best = magnitude[i]; peak_index = i; }
        }
        max_mag = best;
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
    }

    /* The source samples are already copied out, so the target may be reallocated */
    if (!ensure_result_buffer(target_slot, g_fft_config.output_points)) {
        printf("\nInsufficient memory for FFT result!\n");
        _ffree(real_data);
        _ffree(imag_data);
        _ffree(magnitude);
        _ffree(window_data);
        printf("Press any key...");
        getch();
        return;
    }

    /* is_result marks a computed trace (fresh slot or overwritten source) so
       continuous monitoring won't re-read it. */
    g_system->modules[target_slot].enabled = 1;
    g_system->modules[target_slot].module_type = MOD_NONE;
    g_system->modules[target_slot].is_result = 1;
    strcpy(g_system->modules[target_slot].description, "FFT Result");
    g_system->modules[target_slot].gpib_address = 0;  /* No GPIB for computed data */

    /* Apply peak centering if enabled */
    if (g_fft_config.peak_centering && peak_index > 0) {
        int center_position = g_fft_config.output_points / 2;
        int shift_amount = center_position - peak_index;
        int source_index;
        
        printf("Centering peak (index %d) to center position (%d)...\n", peak_index, center_position);
        
        /* Store results with circular shift to center peak */
        for (i = 0; i < g_fft_config.output_points; i++) {
            source_index = (i - shift_amount);
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
                g_traces[target_slot].unit_type = UNIT_POWER;
                strcpy(g_traces[target_slot].label, "FFT (Power)");
                break;
        }
        
        g_traces[target_slot].x_scale = out_bin_hz;

        if (g_fft_config.peak_centering && peak_index > 0) {
            /* Centered mode: peak frequency becomes the center reference */
            float peak_frequency = peak_index * out_bin_hz;
            int center_position = g_fft_config.output_points / 2;
            g_traces[target_slot].x_offset = peak_frequency - (center_position * out_bin_hz);
        } else {
            /* Normal mode: start at 0 Hz */
            g_traces[target_slot].x_offset = 0.0;
        }
        
        /* Trace points at the module buffer (no copy) */
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
           peak_index * out_bin_hz);
    
    _ffree(real_data);
    _ffree(imag_data);
    _ffree(magnitude);
    _ffree(window_data);
    
    printf("\nResults stored in slot %d\n", target_slot);
    printf("Frequency resolution: %.3f Hz per point\n", out_bin_hz);
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
    float prev, cur;

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
    }

    if (!ensure_result_buffer(target_slot, count)) {
        printf("\nInsufficient memory for result!\n");
        printf("Press any key...");
        getch();
        return;
    }
    result_data = g_system->modules[target_slot].module_data;

    g_system->modules[target_slot].enabled = 1;
    g_system->modules[target_slot].module_type = MOD_NONE;
    g_system->modules[target_slot].is_result = 1;
    strcpy(g_system->modules[target_slot].description, "Derivative");
    g_system->modules[target_slot].gpib_address = 0;

    if (g_has_287) {
        /* Central difference, one-sided at the ends. result_data aliases source_data
           when overwriting, so source_data[i-1] comes from prev. */
        scale_factor = 1.0 / dt;

        prev = source_data[0];
        result_data[0] = (source_data[1] - prev) * scale_factor;

        for (i = 1; i < count - 1; i++) {
            cur = source_data[i];
            result_data[i] = (source_data[i+1] - prev) * 0.5 * scale_factor;
            prev = cur;
        }

        result_data[count-1] = (source_data[count-1] - prev) * scale_factor;
    } else {
        /* No 287: forward difference (reads i and i+1 before writing i) */
        for (i = 0; i < count - 1; i++) {
            result_data[i] = (source_data[i+1] - source_data[i]) / dt;
        }
        result_data[count-1] = result_data[count-2];  /* Duplicate last */
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    if (target_slot >= 0 && target_slot < 10) {
        g_traces[target_slot].unit_type = UNIT_DERIVATIVE;  /* V/s */
        g_traces[target_slot].x_scale = 1.0;  /* x axis in samples */
        g_traces[target_slot].x_offset = 0.0;
        strcpy(g_traces[target_slot].label, "dV/dt");

        g_traces[target_slot].data = g_system->modules[target_slot].module_data;
        g_traces[target_slot].data_count = g_system->modules[target_slot].module_data_count;
        g_traces[target_slot].enabled = 1;
        g_traces[target_slot].slot = target_slot;
        g_traces[target_slot].color = 0x0E;  /* yellow */
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
    float prev, cur;
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
    }

    if (!ensure_result_buffer(target_slot, count)) {
        printf("\nInsufficient memory for result!\n");
        printf("Press any key...");
        getch();
        return;
    }
    result_data = g_system->modules[target_slot].module_data;

    g_system->modules[target_slot].enabled = 1;
    g_system->modules[target_slot].module_type = MOD_NONE;
    g_system->modules[target_slot].is_result = 1;
    strcpy(g_system->modules[target_slot].description, "Integral");
    g_system->modules[target_slot].gpib_address = 0;

    /* Running integral, Kahan-summed; trapezoidal with a 287, rectangle rule without.
       result_data aliases source_data when overwriting, so the trapezoid takes
       source_data[i-1] from prev. */
    prev = source_data[0];
    result_data[0] = 0.0;

    kahan_init(&acc);

    init_fpu_precision();

    if (g_has_287) {
        for (i = 1; i < count; i++) {
            cur = source_data[i];
            kahan_add(&acc, (double)(cur + prev) * 0.5 * dt);
            prev = cur;
            result_data[i] = (float)acc.sum;
        }
    } else {
        for (i = 1; i < count; i++) {
            double rectangle_area = (double)source_data[i] * dt;
            kahan_add(&acc, rectangle_area);
            result_data[i] = (float)acc.sum;
        }
    }
    
    g_system->modules[target_slot].module_data_count = count;

    if (target_slot >= 0 && target_slot < 10) {
        g_traces[target_slot].unit_type = UNIT_VOLTAGE;  /* values are V*s; no such unit, so the axis reads V */
        g_traces[target_slot].x_scale = 1.0;  /* x axis in samples */
        g_traces[target_slot].x_offset = 0.0;
        strcpy(g_traces[target_slot].label, "Integral");

        g_traces[target_slot].data = g_system->modules[target_slot].module_data;
        g_traces[target_slot].data_count = g_system->modules[target_slot].module_data_count;
        g_traces[target_slot].enabled = 1;
        g_traces[target_slot].slot = target_slot;
        g_traces[target_slot].color = 0x0E;  /* yellow */
    }

    printf("\nIntegration complete!\n");
    printf("Result is in V*s (the graph axis is labelled V)\n");
    printf("Final value: %.6f V*s\n", result_data[count-1]);
    printf("\nPress any key to continue...");
    getch();
}

#define SMOOTH_HISTORY 11   /* > largest half window (21 / 2) */

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
    float history[SMOOTH_HISTORY];
    
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
    }

    if (!ensure_result_buffer(target_slot, count)) {
        printf("\nInsufficient memory for result!\n");
        printf("Press any key...");
        getch();
        return;
    }
    result_data = g_system->modules[target_slot].module_data;

    g_system->modules[target_slot].enabled = 1;
    g_system->modules[target_slot].module_type = MOD_NONE;
    g_system->modules[target_slot].is_result = 1;
    strcpy(g_system->modules[target_slot].description, "Smoothed");
    g_system->modules[target_slot].gpib_address = 0;

    /* Centered moving average, Kahan-summed; the window shrinks at the edges.
       result_data aliases source_data when overwriting, so samples before i come
       from history[], which holds the last SMOOTH_HISTORY source values. */
    init_fpu_precision();

    for (i = 0; i < count; i++) {
        kahan_init(&window_acc);

        start = i - half_window;
        end = i + half_window;

        if (start < 0) start = 0;
        if (end >= count) end = count - 1;

        for (j = start; j <= end; j++) {
            kahan_add(&window_acc, (double)(j < i ? history[j % SMOOTH_HISTORY]
                                                  : source_data[j]));
        }

        history[i % SMOOTH_HISTORY] = source_data[i];
        result_data[i] = (float)(window_acc.sum / (end - start + 1));
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nSmoothing complete!\n");
    printf("Noise reduction applied with %d-point moving average\n", window_size);
    printf("\nPress any key to continue...");
    getch();
}
