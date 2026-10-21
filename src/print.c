/*
 * TM5000 GPIB Control System - Print Module
 * Version 3.4
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Version update
 * 3.3 - Version update  
 * 3.4 - Added UNIT_POWER support for FFT power spectrum printing
 *     - Fixed print menu with toggle-based custom header control
 *     - Eliminated input buffering bug causing immediate printing
 */

#include "print.h"
#include "graphics.h"
#include "data.h"  /* For get_graph_units(), get_frequency_units(), get_db_units() functions */

/* Global variables for custom header control */
int g_use_custom_header = 0;  /* 0 = default header, 1 = custom header */
char g_custom_header_text[64] = "TM5000 HIGH-PRECISION MEASUREMENT GRAPH";

/* Send a byte to the parallel port (LPT1).
 * Grid-OS: the port address comes from the BIOS (0040:0008), not a fixed
 * 378h, and BUSY is waited out for up to 10 s (a PostScript printer holds
 * BUSY while it works) instead of ~1000 loop passes, after which bytes were
 * silently dropped.  A real timeout sets g_lpt_error and stops the job.   */
int g_lpt_error = 0;

static unsigned lpt_base(void) {
    unsigned b = *(unsigned far *)0x00400008L;
    return b ? b : LPT1_BASE;
}

void lpt_send_byte(unsigned char data) {
    unsigned base = lpt_base();
    unsigned long t0;
    
    if (g_lpt_error) return;
    if (!(inportb(base + 1) & 0x80)) {               /* BUSY */
        t0 = *(unsigned long far *)0x0040006CL;
        while (!(inportb(base + 1) & 0x80)) {
            if (*(unsigned long far *)0x0040006CL - t0 > 182UL) {
                g_lpt_error = 1;                     /* printer not ready */
                return;
            }
        }
    }
    outportb(base, data);
    outportb(base + 2, 0x0D);                        /* strobe */
    outportb(base + 2, 0x0C);
}

/* Grid-OS: print exactly what the graph screen shows - rows 0-185 of the
 * CGA 320x200 four-level screen (graph, labels; not the key-help footer) as
 * a PostScript grey image with a white background, the caption under it.
 * Returns 0 when sent, 1 if the printer was not ready.                  */
int print_screen_ps(const char *caption) {
    static const char hx[] = "0123456789ABCDEF";
    char line[168];
    char hdr[96];
    unsigned char far *row;
    unsigned char b;
    int y, x, n;
    time_t now;
    struct tm *tmv;
    
    g_lpt_error = 0;
    time(&now);
    tmv = localtime(&now);
    sprintf(hdr, "(%s   %04d-%02d-%02d %02d:%02d:%02d) show\r\n",
            (g_use_custom_header && g_custom_header_text[0]) ? g_custom_header_text : "TM5000G graph",
            tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday,
            tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
    
    print_string("%!PS-Adobe-3.0\r\n%%Title: TM5000G screen\r\n%%Pages: 1\r\n%%EndComments\r\n");
    print_string("%%Page: 1 1\r\n/Times-Roman findfont 12 scalefont setfont\r\n36 750 moveto\r\n");
    print_string(hdr);
    /* 540 pt wide; 186 rows x 1.6875 pt keeps the screen's 1.6:1 shape */
    print_string("gsave 36 420 translate 540 313.875 scale\r\n/ln 80 string def\r\n");
    print_string("320 186 2 [320 0 0 -186 0 186] {currentfile ln readhexstring pop} image\r\n");
    for (y = 0; y < 186 && !g_lpt_error; y++) {
        row = video_mem + ((y & 1) ? 8192 : 0) + (y >> 1) * 80;
        n = 0;
        for (x = 0; x < 80; x++) {
            b = (unsigned char)~row[x];          /* lit pixel = dark ink */
            line[n++] = hx[b >> 4];
            line[n++] = hx[b & 15];
        }
        line[n++] = '\r'; line[n++] = '\n'; line[n] = '\0';
        print_string(line);
    }
    print_string("grestore\r\n");
    if (caption && caption[0]) {
        sprintf(hdr, "36 400 moveto (%.80s) show\r\n", caption);
        print_string(hdr);
    }
    print_string("showpage\r\n%%EOF\r\n");
    lpt_send_byte(0x04);                         /* end of job */
    return g_lpt_error;
}

/* Send a string to the parallel port */
void print_string(char *str) {
    while (*str) {
        lpt_send_byte(*str++);
    }
}

/* Map an internal (uppercase) unit string to its PostScript-friendly glyph form.
   Single source of truth shared by get_print_units and get_trace_print_units. */
static char *ps_unit_of(char *u) {
    if (strcmp(u, "UV")   == 0) return "uV";   /* For PostScript /mu glyphshow support */
    if (strcmp(u, "MV")   == 0) return "mV";
    if (strcmp(u, "V")    == 0) return "V";
    if (strcmp(u, "HZ")   == 0) return "Hz";
    if (strcmp(u, "KHZ")  == 0) return "kHz";
    if (strcmp(u, "MHZ")  == 0) return "MHz";
    if (strcmp(u, "GHZ")  == 0) return "GHz";
    if (strcmp(u, "DB")   == 0) return "dB";
    if (strcmp(u, "UV/S") == 0) return "uV/s";
    if (strcmp(u, "MV/S") == 0) return "mV/s";
    if (strcmp(u, "V/S")  == 0) return "V/s";
    if (strcmp(u, "UA")   == 0) return "uA";
    if (strcmp(u, "MA")   == 0) return "mA";
    if (strcmp(u, "A")    == 0) return "A";
    if (strcmp(u, "UO")   == 0) return "uO";   /* uOhm */
    if (strcmp(u, "MO")   == 0) return "mO";   /* mOhm */
    if (strcmp(u, "O")    == 0) return "O";    /* Ohm */
    return u;  /* power spectrum (^2/Hz) and anything else: pass through */
}

/* Print-specific unit conversion for PostScript glyphs - supports FFT and counter units */
void get_print_units(double range, char **unit_str, double *scale_factor, int *decimal_places, char **postscript_unit) {
    float sf;   /* the graphics unit helpers work in float; factors are exact */
    get_graph_units((float)range, unit_str, &sf, decimal_places);
    *scale_factor = sf;
    *postscript_unit = ps_unit_of(*unit_str);
}

/* Enhanced unit detection for trace-specific printing */
void get_trace_print_units(int trace_idx, double range, char **unit_str, double *scale_factor, int *decimal_places, char **postscript_unit) {
    float sf;
    /* Check trace unit type first */
    if (trace_idx >= 0 && trace_idx < 10 && g_traces[trace_idx].enabled) {
        get_units_for_type(g_traces[trace_idx].unit_type, (float)range, unit_str, &sf, decimal_places);
    } else {
        /* Fallback to standard units */
        get_graph_units((float)range, unit_str, &sf, decimal_places);
    }
    *scale_factor = sf;
    
    /* Convert to print-friendly PostScript units */
    *postscript_unit = ps_unit_of(*unit_str);
}


/* ---- Grid-OS: offset axis for small changes on big values ------------
 * Same rule as the screen graph: when the plotted span is under 2% of the
 * values (a 10 MHz standard moving a few Hz), the axis is labelled with
 * signed offsets from a round reference value, printed with the graph.  */
static int print_offset_ref(double mn, double mx, double scale, double *ref) {
    double vmax = fabs(mn), p;
    if (fabs(mx) > vmax) vmax = fabs(mx);
    if (!(mx > mn) || (mx - mn) >= 0.02 * vmax || vmax * scale < 100.0) return 0;
    p = pow(10.0, ceil(log10(mx - mn)));
    *ref = floor((mn + mx) / 2.0 / p + 0.5) * p;
    return 1;
}

/* decimals needed to print a grid step such as 0.25, 2, 0.005 exactly */
static int step_dec(double step) {
    int d;
    double sv;
    if (step <= 0.0) return 0;
    d = (int)ceil(-log10(step) - 1e-9);
    if (d < 0) d = 0;
    sv = step * pow(10.0, d);
    if (fabs(sv - floor(sv + 0.5)) > 1e-4 * sv && d < 9) d++;
    return d > 9 ? 9 : d;
}

/* decimals so a range or per-division value keeps 3 significant digits
   (a 2 Hz range printed as "0 Hz" per division with the unit's 0 decimals) */
static int sig_dec(int dec, double v) {
    int d;
    v = fabs(v);
    if (v <= 0.0) return dec < 0 ? 0 : dec;
    d = 2 - (int)floor(log10(v));
    if (d < dec) d = dec;
    if (d < 0) d = 0;
    return d > 9 ? 9 : d;
}

/* decimals for a legend value: at least the unit's own, enough to show
   changes of 'res' (already scaled to the unit), at most 9 */
static int val_dec(int dec, double res) {
    int d;
    if (res <= 0.0) return dec;
    d = (int)ceil(-log10(res) - 1e-9);
    if (d < dec) d = dec;
    if (d < 0) d = 0;
    return d > 9 ? 9 : d;
}

/* reference value as text in the unit that suits its size, with enough
   decimals to match one grid step */
static void ref_text(char *buf, int trace, double ref, double step, char **unit) {
    char *u;
    double sc;
    int d, d2;
    get_trace_print_units(trace, fabs(ref), &u, &sc, &d, unit);
    d2 = step_dec(step * sc);
    if (d2 > d) d = d2;
    sprintf(buf, "%.*f", d, ref * sc);
}

/* ---- Grid-OS: run duration and x-axis time for printouts --------------
 * From the samples' own time stamps when they have them (continuous
 * monitor runs); otherwise samples x interval x averaging, in long/double
 * arithmetic (the old int product overflowed: 1023 x 275 ms printed as
 * 19.2 s).  *measured says which one it is.                              */
static double run_seconds(int max_samples, int *measured, int *avg) {
    long sp;
    int i, sh = 0;
    for (i = 0; i < 10; i++)
        if (g_traces[i].enabled && g_traces[i].data_count == max_samples &&
            g_system->modules[i].avg_shift > sh)
            sh = g_system->modules[i].avg_shift;
    *avg = 1 << sh;
    /* a computed spectrum (FFT): the duration of the measurement it came from */
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count == max_samples &&
            g_system->modules[i].is_result && g_system->modules[i].src_seconds > 0.0f) {
            *measured = g_system->modules[i].src_measured;
            *avg = 1;
            return g_system->modules[i].src_seconds;
        }
    }
    if (time_axis_slot(max_samples, &sp) >= 0) {
        *measured = 1;
        return sp / 1000.0;
    }
    *measured = 0;
    return (max_samples > 1) ?
        (double)(max_samples - 1) * g_control_panel.sample_rate_ms * (1 << sh) / 1000.0 : 0.0;
}

/* the FFT result slot being printed (longest trace is a spectrum), or -1 */
static int spectrum_slot(int max_samples) {
    int i;
    for (i = 0; i < 10; i++)
        if (g_traces[i].enabled && g_traces[i].data_count == max_samples &&
            g_system->modules[i].is_result && g_system->modules[i].src_seconds > 0.0f)
            return i;
    return -1;
}

/* x label k of n for the printed axis: time when the X:TIME view is on and
   the data has stamps, else the sample number */
static void x_label(char *buf, int max_samples, int k, int n) {
    long sp, t;
    int ts = g_xaxis_time ? time_axis_slot(max_samples, &sp) : -1;
    int idx = (max_samples > 0) ? (int)((long)(max_samples - 1) * k / n) : 0;
    if (ts >= 0 && (t = sample_time_ms(ts, idx)) >= 0) fmt_time_label(buf, t, sp);
    else sprintf(buf, "%d", (int)((long)max_samples * k / n));
}

static char *x_title(int max_samples) {
    static char t[24];
    long sp;
    if (g_xaxis_time && time_axis_slot(max_samples, &sp) >= 0) {
        sprintf(t, "Time (%s)", sp < 100000L ? "s" : sp < 6000000L ? "min:s" : "h:min");
        return t;
    }
    return "Sample Number";
}

/* Print options menu */
/* Grid-OS: what the graph's P key prints - the print menu sets it (the
   last kind printed from the menu, or option 5) */
int g_print_mode = PRINT_MODE_POSTSCRIPT;

static const char *print_mode_name(int m) {
    return m == PRINT_MODE_TEXT ? "Text graph" :
           m == PRINT_MODE_SCREEN ? "Screen copy" : "PostScript plot";
}

/* P from the graph, text and PostScript modes (screen copy is done by the
   graph itself, which still has the picture on screen) */
void print_graph_selected(void) {
    if (g_print_mode == PRINT_MODE_TEXT) print_graph_text();
    else print_graph_postscript();
}

void print_graph_menu(void) {
    int choice;
    char input_buffer[80];
    
    while (1) {
        clrscr();
        printf("Print Graph Options\n");
        printf("===================\n\n");
        printf("Custom Header: %s\n", g_use_custom_header ? "ON" : "OFF");
        if (g_use_custom_header) {
            printf("Header Text: %s\n\n", g_custom_header_text);
        } else {
            printf("Header Text: TM5000 HIGH-PRECISION MEASUREMENT GRAPH\n\n");
        }
        printf("1. Text-based graph (80 column)\n");
        printf("2. Brother PostScript (HL5370DL)\n");
        printf("3. Toggle custom header ON/OFF\n");
        printf("4. Set custom header text\n");
        printf("5. P key in the graph prints: %s\n", print_mode_name(g_print_mode));
        printf("0. Cancel\n\n");
        printf("Select option: ");
        
        choice = getch();
        
        switch(choice) {
            case '1':
                g_print_mode = PRINT_MODE_TEXT;
                print_graph_text();
                return;
            case '2':
                g_print_mode = PRINT_MODE_POSTSCRIPT;
                print_graph_postscript();
                return;
            case '5':
                g_print_mode = (g_print_mode == PRINT_MODE_POSTSCRIPT) ? PRINT_MODE_TEXT :
                               (g_print_mode == PRINT_MODE_TEXT) ? PRINT_MODE_SCREEN :
                                                                   PRINT_MODE_POSTSCRIPT;
                break;
            case '3':
                g_use_custom_header = !g_use_custom_header;
                break;
            case '4':
                printf("\n\nEnter custom header text: ");
                fflush(stdout);
                /* Clear input buffer first */
                while (kbhit()) getch();
                /* Get the input line */
                if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
                    /* Remove newline */
                    input_buffer[strcspn(input_buffer, "\n")] = '\0';
                    if (strlen(input_buffer) > 0) {
                        strcpy(g_custom_header_text, input_buffer);
                        g_use_custom_header = 1;  /* Automatically enable custom header */
                    }
                }
                break;
            case '0':
            default:
                return;
        }
    }
}

void print_graph_text(void) {
    int i, j, x, y;
    char line[81];
    char plot_chars[80];
    double value, y_range;
    int y_pos;
    int samples_to_print;
    char label[40];
    int trace_count = 0;
    int max_samples = 0;
    int dominant_trace = -1;
    double line_value;  
    char plot_char;
    char *unit_str;
    double scale_factor;
    int decimal_places;
    double min_val, max_val;
    int graph_lines = 51;  /* 50 intervals: a label every 5 lines = 10 divisions */
    int off_mode = 0, lbl_dec = 0;
    double off_ref = 0.0;
    
    g_lpt_error = 0;
    printf("\nPrinting text graph...\n");
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            trace_count++;
            if (g_traces[i].data_count > max_samples) {
                max_samples = g_traces[i].data_count;
            }
        }
    }
    
    if (trace_count == 0) {
        printf("No data to print!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    graph_limits(&min_val, &max_val);   /* full-precision axis limits */
    y_range = max_val - min_val;
    
    {
        char *postscript_unit;
        
        /* Find the dominant trace to determine units - prioritize FFT traces */
        /* Check for FFT traces first */
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_DB) {
                dominant_trace = i;
                break;  /* FFT trace takes precedence for Y-axis units */
            }
        }
        
        /* Check for frequency traces (power spectrum) if no dB FFT found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_FREQUENCY) {
                    dominant_trace = i;
                    break;  /* Power spectrum trace takes precedence */
                }
            }
        }
        
        /* Check for derivative traces if no FFT found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_DERIVATIVE) {
                    dominant_trace = i;
                    break;  /* Derivative trace takes precedence over current/voltage */
                }
            }
        }
        
        /* Check for current traces if no derivative found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_CURRENT) {
                    dominant_trace = i;
                    break;  /* Current trace takes precedence over voltage */
                }
            }
        }
        
        /* Check for resistance traces if no current found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_RESISTANCE) {
                    dominant_trace = i;
                    break;  /* Resistance trace takes precedence over voltage */
                }
            }
        }
        
        /* If no special trace found, use first enabled trace */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                    dominant_trace = i;
                    break;  /* Use first enabled trace for units */
                }
            }
        }
        
        if (dominant_trace >= 0) {
            get_trace_print_units(dominant_trace, y_range, &unit_str, &scale_factor, &decimal_places, &postscript_unit);
        } else {
            get_print_units(y_range, &unit_str, &scale_factor, &decimal_places, &postscript_unit);
        }
        unit_str = postscript_unit;  /* Use print-friendly units */
    }
    if (!(dominant_trace >= 0 && g_traces[dominant_trace].unit_type == UNIT_DB) &&
        print_offset_ref(min_val, max_val, scale_factor, &off_ref)) {
        off_mode = 1;
        lbl_dec = step_dec(y_range / 10.0 * scale_factor);
    }
    
    lpt_send_byte(0x1B); lpt_send_byte(0x40); /* Reset */
    lpt_send_byte(0x0D); /* CR */
    lpt_send_byte(0x0A); /* LF */
    
    /* Print header based on global setting */
    if (g_use_custom_header && strlen(g_custom_header_text) > 0) {
        print_string(g_custom_header_text);
        print_string("\r\n");
        print_string("=======================================\r\n");
    } else {
        print_string("TM5000 HIGH-PRECISION MEASUREMENT GRAPH\r\n");
        print_string("=======================================\r\n");
    }
    print_string("\r\n");
    
    {   /* Grid-OS: enough decimals to tell the limits apart */
        int sd = decimal_places, s2 = step_dec(y_range / 10.0 * scale_factor);
        if (s2 > sd) sd = s2;
        sprintf(label, "Y Scale: %.*f to %.*f %s\r\n", 
                sd, min_val * scale_factor, sd, max_val * scale_factor, unit_str);
    }
    print_string(label);
    if (off_mode) {
        char rs[24], *ru;
        ref_text(rs, dominant_trace, off_ref, y_range / 10.0, &ru);
        sprintf(label, "Y axis: offset from %s %s, in %s\r\n", rs, ru, unit_str);
        print_string(label);
    }
    
    {
        char *range_unit_str;
        double range_scale_factor;
        int range_decimal_places;
        
        {
            char *range_postscript_unit;
            get_print_units(y_range, &range_unit_str, &range_scale_factor, &range_decimal_places, &range_postscript_unit);
            range_unit_str = range_postscript_unit;  /* Use print-friendly units */
        }
        
        /* Calculate range exactly as displayed on grid lines (10 divisions) */
        sprintf(label, "Total Scale: %.*f %s (%.*f %s per division)\r\n", 
                sig_dec(range_decimal_places, y_range * range_scale_factor),
                y_range * range_scale_factor, range_unit_str,
                sig_dec(range_decimal_places, y_range / 10.0 * range_scale_factor),
                (y_range / 10.0) * range_scale_factor, range_unit_str);
    }
    print_string(label);
    
    {
        int meas, avg;
        double secs = run_seconds(max_samples, &meas, &avg);
        int sp = spectrum_slot(max_samples);
        if (sp >= 0)
            sprintf(label, "Spectrum: %d bins of %.4g Hz, from %.1f s of measurement%s\r\n",
                    max_samples, (double)g_traces[sp].x_scale, secs, meas ? " (measured)" : "");
        else if (max_samples > 1 && secs > 0.0)
            sprintf(label, "Sample interval: %.0f ms %s, Samples: %d\r\n",
                    secs * 1000.0 / (max_samples - 1), meas ? "(measured)" : "(set)", max_samples);
        else
            sprintf(label, "Sample Rate: %d ms, Samples: %d\r\n", g_control_panel.sample_rate_ms, max_samples);
        print_string(label);
        if (avg > 1) {
            sprintf(label, "Averaging: each sample is the mean of %d readings (buffer full)\r\n", avg);
            print_string(label);
        }
    }
    
    print_string("Measurement Statistics:\r\n");
    print_string("----------------------\r\n");
    {
        time_t current_time;
        struct tm *time_info;
        int meas_t, avg_t;
        double total_time_sec = run_seconds(max_samples, &meas_t, &avg_t);
        int active_traces = 0;
        
        {
            int i;
            for (i = 0; i < 10; i++) {
                if (g_traces[i].data_count > 0) active_traces++;
            }
        }
        
        sprintf(label, "Measurement Duration: %.1f seconds%s\r\n", total_time_sec,
                meas_t ? " (measured)" : "");
        print_string(label);
        sprintf(label, "Active Traces: %d\r\n", active_traces);
        print_string(label);
        sprintf(label, "Data Points per Trace: %d\r\n", max_samples);
        print_string(label);
        
        time(&current_time);
        time_info = localtime(&current_time);
        sprintf(label, "Print Time: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                time_info->tm_year + 1900, time_info->tm_mon + 1, time_info->tm_mday,
                time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        print_string(label);
        
        if (g_graph_scale.sample_count > 0) {
            sprintf(label, "Sample Range: %d to %d (showing %d samples)\r\n",
                    g_graph_scale.sample_start + 1,
                    g_graph_scale.sample_start + g_graph_scale.sample_count,
                    g_graph_scale.sample_count);
        } else {
            sprintf(label, "Sample Range: All samples (1 to %d)\r\n", max_samples);
        }
        print_string(label);
        
        {
            double per_div = (max_val - min_val) / 10.0;   /* double limits */
            char *per_div_unit;
            double per_div_scale;
            int per_div_decimal;
            char *per_div_ps_unit;
            get_print_units(fabs(per_div), &per_div_unit, &per_div_scale, &per_div_decimal, &per_div_ps_unit);
            sprintf(label, "Units per div: %.*f %s, Auto Scale: %s\r\n",
                    sig_dec(per_div_decimal, per_div * per_div_scale),
                    per_div * per_div_scale, per_div_ps_unit,
                    g_graph_scale.auto_scale ? "ON" : "OFF");
        }
        print_string(label);
    }
    print_string("\r\n");
    
    sprintf(label, "Graph Resolution: %.2e %s per line\r\n\r\n", 
            (y_range * scale_factor) / (double)(graph_lines - 1), unit_str);
    print_string(label);
    
    for (y = 0; y < graph_lines; y++) {
        for (x = 0; x < 80; x++) {       /* (was 100: overran both buffers) */
            line[x] = ' ';
            plot_chars[x] = ' ';
        }
        line[80] = '\0';
        
        line_value = max_val - (y * y_range / (double)(graph_lines - 1));
        
        line[0] = '|';
        
        if (y % 5 == 0) {
            if (off_mode) {
                {
                    double dv = (line_value - off_ref) * scale_factor;
                    if (fabs(dv) < 1e-3 * y_range * scale_factor) dv = 0.0;  /* no "-0.0" */
                    sprintf(label, "%+9.*f", lbl_dec, dv);
                }
            } else if (decimal_places == 0) {
                sprintf(label, "%7.0f", line_value * scale_factor);
            } else if (decimal_places >= 4) {
                sprintf(label, "%*.*f", 7 + decimal_places, decimal_places, 
                        line_value * scale_factor);
            } else {
                sprintf(label, "%7.*f", decimal_places, line_value * scale_factor);
            }
            
            for (j = 0; j < (off_mode ? 11 : 7 + decimal_places) && j < 12 && label[j]; j++) {
                line[j] = label[j];
            }
            line[j] = '-';
            line[j+1] = '|';
        } else {
            line[7] = '|';
        }
        
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                samples_to_print = g_traces[i].data_count;
                
                for (x = 0; x < 65; x++) {
                    j = (int)((long)x * g_traces[i].data_count / 65);
                    if (j >= g_traces[i].data_count) j = g_traces[i].data_count - 1;
                    
                    value = g_traces[i].data[j];
                    
                    y_pos = (int)((double)(graph_lines - 1) * (max_val - value) / y_range + 0.5);
                    
                    if (y_pos == y && x < 65) {
                        switch(i) {
                            case 0: plot_char = '*'; break;
                            case 1: plot_char = '+'; break;
                            case 2: plot_char = 'o'; break;
                            case 3: plot_char = '#'; break;
                            case 4: plot_char = '@'; break;
                            case 5: plot_char = 'x'; break;
                            case 6: plot_char = '%'; break;
                            case 7: plot_char = '&'; break;
                            default: plot_char = '*'; break;
                        }
                        if (plot_chars[x] == ' ') {
                            plot_chars[x] = plot_char;
                        }
                    }
                }
            }
        }
        
        for (x = 0; x < 65; x++) {
            line[x + 13] = plot_chars[x];  /* Increased offset for wider labels */
        }
        
        print_string(line);
        print_string("\r\n");
    }
    
    print_string("            +");  /* Adjusted spacing */
    for (x = 0; x < 65; x++) {
        lpt_send_byte('-');
    }
    print_string("\r\n");
    
    print_string("            0");  /* Adjusted spacing */
    
    /* Check if we have FFT traces for frequency X-axis */
    {
        int has_fft_trace = 0;
        int fft_trace_idx = -1;
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == 2) {
                has_fft_trace = 1;
                fft_trace_idx = i;
                break;
            }
        }
        
        if (has_fft_trace && fft_trace_idx >= 0) {
            /* FFT frequency X-axis */
            double sample_rate = 1000.0 / g_control_panel.sample_rate_ms;  /* Hz */
            double max_freq = sample_rate / 2.0;  /* Nyquist frequency */
            char *freq_unit;
            double freq_scale;
            
            if (max_freq >= 1000000.0) {
                freq_unit = "MHz";
                freq_scale = 1000000.0;
            } else if (max_freq >= 1000.0) {
                freq_unit = "kHz";
                freq_scale = 1000.0;
            } else {
                freq_unit = "Hz";
                freq_scale = 1.0;
            }
            
            for (x = 1; x <= 6; x++) {
                double freq_val = (max_freq * x) / (6.0 * freq_scale);
                sprintf(label, "%9.1f", freq_val);
                print_string(label);
            }
            print_string("\r\n");
            sprintf(label, "                              Frequency (%s)\r\n\r\n", freq_unit);
            print_string(label);
        } else {
            /* Standard sample number X-axis */
            for (x = 1; x <= 6; x++) {
                char xl[16];
                x_label(xl, max_samples, x, 6);
                sprintf(label, "%10s", xl);
                print_string(label);
            }
            print_string("\r\n");
            sprintf(label, "                              %s\r\n\r\n", x_title(max_samples));
            print_string(label);
        }
    }
    
    print_string("Legend:\r\n");
    print_string("-------\r\n");
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            char symbol;
            double last_val, min_trace, max_trace;
            int k;
            
            switch(i) {
                case 0: symbol = '*'; break;
                case 1: symbol = '+'; break;
                case 2: symbol = 'o'; break;
                case 3: symbol = '#'; break;
                case 4: symbol = '@'; break;
                case 5: symbol = 'x'; break;
                case 6: symbol = '%'; break;
                case 7: symbol = '&'; break;
                default: symbol = '*'; break;
            }
            
            min_trace = max_trace = g_traces[i].data[0];
            for (k = 1; k < g_traces[i].data_count; k++) {
                if (g_traces[i].data[k] < min_trace) min_trace = g_traces[i].data[k];
                if (g_traces[i].data[k] > max_trace) max_trace = g_traces[i].data[k];
            }
            
            last_val = g_system->modules[i].last_reading;
            
            sprintf(label, "  %c = Slot %d: %s (%d samples)\r\n", 
                    symbol, i, g_traces[i].label, g_traces[i].data_count);
            print_string(label);
            
            {
                char *last_unit, *min_unit, *max_unit;
                double last_scale, min_scale, max_scale;
                int last_dec, min_dec, max_dec;
                double trace_range = max_trace - min_trace;
                
                {
                    char *last_ps_unit, *min_ps_unit, *max_ps_unit;
                    get_trace_print_units(i, fabs(last_val), &last_unit, &last_scale, &last_dec, &last_ps_unit);
                    get_trace_print_units(i, fabs(min_trace), &min_unit, &min_scale, &min_dec, &min_ps_unit);
                    get_trace_print_units(i, fabs(max_trace), &max_unit, &max_scale, &max_dec, &max_ps_unit);
                    last_unit = last_ps_unit;
                    min_unit = min_ps_unit;
                    max_unit = max_ps_unit;
                }
                
                {   /* Grid-OS: enough decimals to see the trace's own spread */
                    double res = (trace_range > 0.0 ? trace_range : y_range) / 100.0;
                    last_dec = val_dec(last_dec, res * last_scale);
                    min_dec = val_dec(min_dec, res * min_scale);
                    max_dec = val_dec(max_dec, res * max_scale);
                }
                sprintf(label, "      Last: %.*f %s, Min: %.*f %s, Max: %.*f %s\r\n",
                        last_dec, last_val * last_scale, last_unit,
                        min_dec, min_trace * min_scale, min_unit,
                        max_dec, max_trace * max_scale, max_unit);
                print_string(label);
                
                {
                    char *range_unit, *avg_unit, *rms_unit;
                    double range_scale, avg_scale, rms_scale;
                    int range_dec, avg_dec, rms_dec;
                    double avg_val = 0.0, rms_val = 0.0;
                    
                    for (k = 0; k < g_traces[i].data_count; k++) {
                        avg_val += g_traces[i].data[k];
                        rms_val += g_traces[i].data[k] * g_traces[i].data[k];
                    }
                    avg_val /= g_traces[i].data_count;
                    rms_val = sqrt(rms_val / g_traces[i].data_count);
                    
                    {
                        char *range_ps_unit, *avg_ps_unit, *rms_ps_unit;
                        get_trace_print_units(i, trace_range, &range_unit, &range_scale, &range_dec, &range_ps_unit);
                        get_trace_print_units(i, fabs(avg_val), &avg_unit, &avg_scale, &avg_dec, &avg_ps_unit);
                        get_trace_print_units(i, rms_val, &rms_unit, &rms_scale, &rms_dec, &rms_ps_unit);
                        range_unit = range_ps_unit;
                        avg_unit = avg_ps_unit;
                        rms_unit = rms_ps_unit;
                    }
                    
                    {
                        double res = (trace_range > 0.0 ? trace_range : y_range) / 100.0;
                        avg_dec = val_dec(avg_dec, res * avg_scale);
                        rms_dec = val_dec(rms_dec, res * rms_scale);
                    }
                    sprintf(label, "      Average: %.*f %s, RMS: %.*f %s\r\n",
                            avg_dec, avg_val * avg_scale, avg_unit,
                            rms_dec, rms_val * rms_scale, rms_unit);
                    print_string(label);
                }
            }
        }
    }
    
    print_string("\r\nGraph Settings:\r\n");
    print_string("---------------\r\n");
    sprintf(label, "Zoom Factor: %.2f\r\n", g_graph_scale.zoom_factor);
    print_string(label);
    sprintf(label, "Auto Scale: %s\r\n", g_graph_scale.auto_scale ? "ON" : "OFF");
    print_string(label);
    
    if (g_system->modules[0].module_type == MOD_DM5120) {
        print_string("\r\nDM5120 Precision Info:\r\n");
        sprintf(label, "Configured digits: %d\r\n", g_dm5120_config[0].digits);
        print_string(label);
    }
    
    lpt_send_byte(0x0C);
    
    printf("High-precision text graph printed successfully.\n");
    printf("Press any key to continue...");
    getch();
}

void print_graph_postscript(void) {
    int i, j, x, y;
    double value, y_range;
    char label[80];
    int trace_count = 0;
    int dominant_trace = -1;
    double x_scale, y_scale;
    double page_width = 540.0;   
    double page_height = 720.0;  
    double graph_x = 72.0;       
    double graph_y = 144.0;      
    double graph_width = 468.0;  
    double graph_height = 432.0; 
    double y_pos, x_pos;         
    int sample_num;             
    char *unit_str;
    double scale_factor;
    int decimal_places;
    double min_val, max_val;
    int max_samples = 0;
    int off_mode = 0, lbl_dec = 0;
    double off_ref = 0.0;
    
    g_lpt_error = 0;
    printf("\nPrinting to Brother HL5370DW (PostScript)...\n");
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            trace_count++;
            if (g_traces[i].data_count > max_samples) {
                max_samples = g_traces[i].data_count;
            }
        }
    }
    
    if (trace_count == 0) {
        printf("No data to print!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    graph_limits(&min_val, &max_val);   /* full-precision axis limits */
    y_range = max_val - min_val;
    
    {
        char *postscript_unit;
        
        /* Find the dominant trace to determine units - prioritize FFT traces */
        /* Check for FFT traces first */
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_DB) {
                dominant_trace = i;
                break;  /* FFT trace takes precedence for Y-axis units */
            }
        }
        
        /* Check for frequency traces (power spectrum) if no dB FFT found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_FREQUENCY) {
                    dominant_trace = i;
                    break;  /* Power spectrum trace takes precedence */
                }
            }
        }
        
        /* Check for derivative traces if no FFT found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_DERIVATIVE) {
                    dominant_trace = i;
                    break;  /* Derivative trace takes precedence over current/voltage */
                }
            }
        }
        
        /* Check for current traces if no derivative found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_CURRENT) {
                    dominant_trace = i;
                    break;  /* Current trace takes precedence over voltage */
                }
            }
        }
        
        /* Check for resistance traces if no current found */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0 && g_traces[i].unit_type == UNIT_RESISTANCE) {
                    dominant_trace = i;
                    break;  /* Resistance trace takes precedence over voltage */
                }
            }
        }
        
        /* If no special trace found, use first enabled trace */
        if (dominant_trace == -1) {
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                    dominant_trace = i;
                    break;  /* Use first enabled trace for units */
                }
            }
        }
        
        if (dominant_trace >= 0) {
            get_trace_print_units(dominant_trace, y_range, &unit_str, &scale_factor, &decimal_places, &postscript_unit);
        } else {
            get_print_units(y_range, &unit_str, &scale_factor, &decimal_places, &postscript_unit);
        }
        unit_str = postscript_unit;  /* Use print-friendly units */
    }
    if (!(dominant_trace >= 0 && g_traces[dominant_trace].unit_type == UNIT_DB) &&
        print_offset_ref(min_val, max_val, scale_factor, &off_ref)) {
        off_mode = 1;
        lbl_dec = step_dec(y_range / 10.0 * scale_factor);
    }
    
    print_string("%!PS-Adobe-3.0\r\n");
    print_string("%%Title: TM5000 High-Precision Measurement Graph\r\n");
    print_string("%%Creator: TM5000 Control System v3.2\r\n");
    print_string("%%BoundingBox: 0 0 612 792\r\n");
    print_string("%%DocumentNeededResources: font Times-Roman\r\n");
    print_string("%%Pages: 1\r\n");
    print_string("%%EndComments\r\n\r\n");
    
    print_string("%%BeginProlog\r\n");
    print_string("% Ensure mu glyph is available\r\n");
    print_string("/mu { % fallback if glyphshow fails\r\n");
    print_string("  { /mu glyphshow } stopped {\r\n");
    print_string("    pop (u) show  % fallback to 'u' if mu not available\r\n");
    print_string("  } if\r\n");
    print_string("} bind def\r\n");
    print_string("%%EndProlog\r\n\r\n");
    
    print_string("/Times-Roman findfont 16 scalefont setfont\r\n");
    
    print_string("%%Page: 1 1\r\n");
    /* Set graph title based on global setting */
    {
        char graph_title[GPIB_BUFFER_SIZE];
        if (g_use_custom_header && strlen(g_custom_header_text) > 0) {
            strcpy(graph_title, g_custom_header_text);
        } else {
            strcpy(graph_title, "TM5000 HIGH-PRECISION MEASUREMENT GRAPH");
        }
        
        print_string("gsave\r\n");
        
        print_string("72 720 moveto\r\n");
        print_string("(");
        print_string(graph_title);
        print_string(") show\r\n");
    }
    
    print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
    print_string("72 705 moveto\r\n");
    {   /* Grid-OS: enough decimals to tell the limits apart */
        int sd = decimal_places, s2 = step_dec(y_range / 10.0 * scale_factor);
        if (s2 > sd) sd = s2;
        sprintf(label, "(Scale: %.*f to %.*f ) show ", 
                sd, min_val * scale_factor, sd, max_val * scale_factor);
    }
    print_string(label);
    if (strcmp(unit_str, "uV") == 0) {
        print_string("( ) show\r\n");
        print_string("/mu glyphshow\r\n");  /* Direct glyph access */
        print_string("(V) show\r\n");
    } else {
        sprintf(label, "(%s) show\r\n", unit_str);
        print_string(label);
    }
    
    print_string("72 685 moveto\r\n");
    {
        char *range_unit_str;
        double range_scale_factor;
        int range_decimal_places;
        
        {
            char *range_postscript_unit;
            if (dominant_trace >= 0) {
                get_trace_print_units(dominant_trace, y_range, &range_unit_str, &range_scale_factor, &range_decimal_places, &range_postscript_unit);
            } else {
                get_print_units(y_range, &range_unit_str, &range_scale_factor, &range_decimal_places, &range_postscript_unit);
            }
            range_unit_str = range_postscript_unit;
        }
        
        sprintf(label, "(Range: %.*f ) show ",
                sig_dec(range_decimal_places, y_range * range_scale_factor),
                y_range * range_scale_factor);
        print_string(label);
        if (strcmp(range_unit_str, "uV") == 0) {
            print_string("( ) show /mu glyphshow (V, Interval: ) show ");
        } else {
            sprintf(label, "(%s, Interval: ) show ", range_unit_str);
            print_string(label);
        }
    }
    {
        int meas, avg;
        double secs = run_seconds(max_samples, &meas, &avg);
        int sp = spectrum_slot(max_samples);
        if (sp >= 0)
            sprintf(label, "(%.4g Hz/bin) show\r\n", (double)g_traces[sp].x_scale);
        else if (max_samples > 1 && secs > 0.0)
            sprintf(label, "(%.0f ms%s%s) show\r\n", secs * 1000.0 / (max_samples - 1),
                    meas ? " measured" : "", avg > 1 ? "" : "");
        else
            sprintf(label, "(%d ms) show\r\n", g_control_panel.sample_rate_ms);
        print_string(label);
        if (avg > 1) {
            print_string("350 685 moveto\r\n");
            sprintf(label, "(Averaging: each sample = mean of %d readings) show\r\n", avg);
            print_string(label);
        }
    }
    
    print_string("72 670 moveto\r\n");
    print_string("(Measurement Statistics:) show\r\n");
    {
        time_t current_time;
        struct tm *time_info;
        double total_time_sec = 0.0;
        int actual_samples = 0;
        int active_traces = 0;
        
        /* Use actual sample count from enabled traces, not max_samples */
        {
            int i;
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > actual_samples) {
                    actual_samples = g_traces[i].data_count;
                }
                if (g_traces[i].data_count > 0) active_traces++;
            }
        }
        
        if (actual_samples > 0) {
            int meas_t, avg_t;
            total_time_sec = run_seconds(actual_samples, &meas_t, &avg_t);
        }
        
        print_string("72 655 moveto\r\n");
        if (actual_samples > 0) {
            sprintf(label, "(Duration: %.1f sec, Active Traces: %d, Data Points: %d) show\r\n", 
                    total_time_sec, active_traces, actual_samples);
        } else {
            sprintf(label, "(Duration: 0.00 sec, Active Traces: %d, Data Points: 0) show\r\n", 
                    active_traces);
        }
        print_string(label);
        
        time(&current_time);
        time_info = localtime(&current_time);
        print_string("72 640 moveto\r\n");
        sprintf(label, "(Print Time: %04d-%02d-%02d %02d:%02d:%02d) show\r\n",
                time_info->tm_year + 1900, time_info->tm_mon + 1, time_info->tm_mday,
                time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        print_string(label);
        
        print_string("72 625 moveto\r\n");
        if (g_graph_scale.sample_count > 0) {
            {
                double per_div = (max_val - min_val) / 10.0;
                char *per_div_unit;
                double per_div_scale;
                int per_div_decimal;
                char *per_div_ps_unit;
                if (dominant_trace >= 0) {
                    get_trace_print_units(dominant_trace, fabs(per_div), &per_div_unit, &per_div_scale, &per_div_decimal, &per_div_ps_unit);
                } else {
                    get_print_units(fabs(per_div), &per_div_unit, &per_div_scale, &per_div_decimal, &per_div_ps_unit);
                }
                sprintf(label, "(Sample Range: %d to %d, Units/div: %.*f %s, Auto Scale: %s) show\r\n",
                        g_graph_scale.sample_start + 1,
                        g_graph_scale.sample_start + g_graph_scale.sample_count,
                        sig_dec(per_div_decimal, per_div * per_div_scale),
                        per_div * per_div_scale, per_div_ps_unit,
                        g_graph_scale.auto_scale ? "ON" : "OFF");
            }
        } else {
            {
                double per_div = (max_val - min_val) / 10.0;
                char *per_div_unit;
                double per_div_scale;
                int per_div_decimal;
                char *per_div_ps_unit;
                if (dominant_trace >= 0) {
                    get_trace_print_units(dominant_trace, fabs(per_div), &per_div_unit, &per_div_scale, &per_div_decimal, &per_div_ps_unit);
                } else {
                    get_print_units(fabs(per_div), &per_div_unit, &per_div_scale, &per_div_decimal, &per_div_ps_unit);
                }
                sprintf(label, "(Sample Range: All (1 to %d), Units/div: %.*f %s, Auto Scale: %s) show\r\n", 
                        actual_samples,
                        sig_dec(per_div_decimal, per_div * per_div_scale),
                        per_div * per_div_scale, per_div_ps_unit,
                        g_graph_scale.auto_scale ? "ON" : "OFF");
            }
        }
        print_string(label);
    }
    
    print_string("1 setlinewidth\r\n");
    print_string("newpath\r\n");
    sprintf(label, "%.1f %.1f moveto\r\n", graph_x, graph_y);
    print_string(label);
    sprintf(label, "%.1f %.1f lineto\r\n", graph_x + graph_width, graph_y);
    print_string(label);
    sprintf(label, "%.1f %.1f lineto\r\n", graph_x + graph_width, graph_y + graph_height);
    print_string(label);
    sprintf(label, "%.1f %.1f lineto\r\n", graph_x, graph_y + graph_height);
    print_string(label);
    print_string("closepath stroke\r\n");
    
    print_string("0.25 setlinewidth\r\n");
    print_string("[1 2] 0 setdash\r\n");  /* Fine dashed lines */
    
    for (i = 1; i < 10; i++) {
        y_pos = graph_y + (graph_height * i / 10);
        sprintf(label, "newpath %.1f %.1f moveto %.1f %.1f lineto stroke\r\n",
                graph_x, y_pos, graph_x + graph_width, y_pos);
        print_string(label);
    }
    
    for (i = 1; i < 10; i++) {
        x_pos = graph_x + (graph_width * i / 10);
        sprintf(label, "newpath %.1f %.1f moveto %.1f %.1f lineto stroke\r\n",
                x_pos, graph_y, x_pos, graph_y + graph_height);
        print_string(label);
    }
    
    print_string("[] 0 setdash\r\n");  /* Solid lines for traces */
    
    print_string("/Times-Roman findfont 8 scalefont setfont\r\n");
    
    for (i = 0; i <= 10; i++) {
        double y_label_value;
        y_pos = graph_y + (graph_height * i / 10);
        y_label_value = min_val + (y_range * i / 10);
        
        sprintf(label, "%.1f %.1f moveto\r\n", graph_x - 45, y_pos - 3);
        print_string(label);
        if (off_mode) {
            double dv = (y_label_value - off_ref) * scale_factor;
            if (fabs(dv) < 1e-3 * y_range * scale_factor) dv = 0.0;  /* no "-0.0" */
            sprintf(label, "(%+.*f) show\r\n", lbl_dec, dv);
        } else if (decimal_places == 0) {
            sprintf(label, "(%.0f) show\r\n", y_label_value * scale_factor);
        } else {
            sprintf(label, "(%.*f) show\r\n", decimal_places, y_label_value * scale_factor);
        }
        print_string(label);
    }
    
    if (off_mode) {     /* Grid-OS: the reference the offset labels count from */
        char rs[24], *ru;
        ref_text(rs, dominant_trace, off_ref, y_range / 10.0, &ru);
        print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
        sprintf(label, "%.1f %.1f moveto\r\n", graph_x, graph_y + graph_height + 6);
        print_string(label);
        sprintf(label, "(Axis = offset from reference %s ) show ", rs);
        print_string(label);
        if (strcmp(ru, "uV") == 0) {
            print_string("/mu glyphshow (V) show\r\n");
        } else {
            sprintf(label, "(%s) show\r\n", ru);
            print_string(label);
        }
        print_string("/Times-Roman findfont 8 scalefont setfont\r\n");
    }
    
    /* Check if we have FFT traces for frequency X-axis */
    {
        int has_fft_trace = 0;
        int fft_trace_idx = -1;
        for (j = 0; j < 10; j++) {
            if (g_traces[j].enabled && g_traces[j].data_count > 0 && g_traces[j].unit_type == 2) {
                has_fft_trace = 1;
                fft_trace_idx = j;
                break;
            }
        }
        
        if (has_fft_trace && fft_trace_idx >= 0) {
            /* FFT frequency X-axis */
            double sample_rate = 1000.0 / g_control_panel.sample_rate_ms;  /* Hz */
            double max_freq = sample_rate / 2.0;  /* Nyquist frequency */
            char *freq_unit;
            double freq_scale;
            
            if (max_freq >= 1000000.0) {
                freq_unit = "MHz";
                freq_scale = 1000000.0;
            } else if (max_freq >= 1000.0) {
                freq_unit = "kHz";
                freq_scale = 1000.0;
            } else {
                freq_unit = "Hz";
                freq_scale = 1.0;
            }
            
            for (i = 0; i <= 10; i++) {
                double freq_val;
                x_pos = graph_x + (graph_width * i / 10);
                freq_val = (max_freq * i) / (10.0 * freq_scale);
                
                if (i % 2 == 0) {  /* Label every other tick for clarity */
                    sprintf(label, "%.1f %.1f moveto\r\n", x_pos - 15, graph_y - 15);
                    print_string(label);
                    sprintf(label, "(%.1f) show\r\n", freq_val);
                    print_string(label);
                }
            }
            
            print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
            sprintf(label, "%.1f %.1f moveto\r\n", graph_x + graph_width/2 - 30, graph_y - 30);
            print_string(label);
            sprintf(label, "(Frequency [%s]) show\r\n", freq_unit);
            print_string(label);
        } else {
            /* Standard sample number X-axis */
            for (i = 0; i <= 10; i++) {
                x_pos = graph_x + (graph_width * i / 10);
                sample_num = 0;
                
                for (j = 0; j < 10; j++) {
                    if (g_traces[j].enabled && g_traces[j].data_count > sample_num) {
                        sample_num = g_traces[j].data_count;
                    }
                }
                sample_num = (int)((long)sample_num * i / 10);
                
                if (i % 2 == 0) {  /* Label every other tick for clarity */
                    char xl[16];
                    x_label(xl, max_samples, i, 10);
                    sprintf(label, "%.1f %.1f moveto\r\n", x_pos - 10, graph_y - 15);
                    print_string(label);
                    sprintf(label, "(%s) show\r\n", xl);
                    print_string(label);
                }
            }
            
            print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
            sprintf(label, "%.1f %.1f moveto\r\n", graph_x + graph_width/2 - 30, graph_y - 30);
            print_string(label);
            sprintf(label, "(%s) show\r\n", x_title(max_samples));
            print_string(label);
        }
    }
    
    print_string("gsave\r\n");
    sprintf(label, "%.1f %.1f translate\r\n", graph_x - 60, graph_y + graph_height/2);
    print_string(label);
    print_string("90 rotate\r\n");
    print_string("0 0 moveto\r\n");
    
    /* Determine Y-axis label based on trace type */
    {
        int dominant_trace = -1;
        char *y_label = "Measurement";
        
        for (j = 0; j < 10; j++) {
            if (g_traces[j].enabled && g_traces[j].data_count > 0) {
                dominant_trace = j;
                break;
            }
        }
        
        if (dominant_trace >= 0) {
            switch (g_traces[dominant_trace].unit_type) {
                case 1:  /* Frequency units */
                    y_label = "Frequency";
                    break;
                case 2:  /* dB units for FFT */
                    y_label = "Magnitude";
                    break;
                default: /* Voltage units */
                    y_label = "Measurement";
                    break;
            }
        }
        
        sprintf(label, off_mode ? "(%s offset [) show " : "(%s [) show ", y_label);
        print_string(label);
    }
    
    if (strcmp(unit_str, "uV") == 0) {
        print_string("/mu glyphshow (V]) show\r\n");
    } else {
        sprintf(label, "(%s]) show\r\n", unit_str);
        print_string(label);
    }
    print_string("grestore\r\n");
    
    print_string("1 setlinewidth\r\n");
    print_string("1 setlinejoin\r\n");  /* Round line joins */
    print_string("1 setlinecap\r\n");   /* Round line caps */
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            switch(i) {
                case 0: print_string("0 0 0 setrgbcolor\r\n"); break;      
                case 1: print_string("1 0 0 setrgbcolor\r\n"); break;      
                case 2: print_string("0 0 1 setrgbcolor\r\n"); break;      
                case 3: print_string("0 0.5 0 setrgbcolor\r\n"); break;    
                case 4: print_string("0.5 0 0.5 setrgbcolor\r\n"); break;  
                case 5: print_string("0 0.5 0.5 setrgbcolor\r\n"); break;  
                case 6: print_string("0.5 0.5 0 setrgbcolor\r\n"); break;  
                case 7: print_string("0.5 0.5 0.5 setrgbcolor\r\n"); break;
            }
            
            print_string("newpath\r\n");
            
            x_scale = graph_width / (double)(g_traces[i].data_count - 1);
            y_scale = graph_height / y_range;
            
            value = g_traces[i].data[0];
            sprintf(label, "%.2f %.2f moveto\r\n",
                    graph_x,
                    graph_y + (value - min_val) * y_scale);
            print_string(label);
            
            for (j = 1; j < g_traces[i].data_count; j++) {
                value = g_traces[i].data[j];
                sprintf(label, "%.2f %.2f lineto\r\n",
                        graph_x + j * x_scale,
                        graph_y + (value - min_val) * y_scale);
                print_string(label);
                
                if (j % 100 == 0) {
                    print_string("stroke\r\n");
                    sprintf(label, "%.2f %.2f moveto\r\n",
                            graph_x + j * x_scale,
                            graph_y + (value - min_val) * y_scale);
                    print_string(label);
                }
            }
            
            print_string("stroke\r\n");
        }
    }
    
    print_string("0 0 0 setrgbcolor\r\n");  
    print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
    
    print_string("72 120 moveto\r\n");
    print_string("(Legend:) show\r\n");
    
    y = 100;
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            double min_trace, max_trace;
            int k;
            
            min_trace = max_trace = g_traces[i].data[0];
            for (k = 1; k < g_traces[i].data_count; k++) {
                if (g_traces[i].data[k] < min_trace) min_trace = g_traces[i].data[k];
                if (g_traces[i].data[k] > max_trace) max_trace = g_traces[i].data[k];
            }
            
            switch(i) {
                case 0: print_string("0 0 0 setrgbcolor\r\n"); break;
                case 1: print_string("1 0 0 setrgbcolor\r\n"); break;
                case 2: print_string("0 0 1 setrgbcolor\r\n"); break;
                case 3: print_string("0 0.5 0 setrgbcolor\r\n"); break;
                case 4: print_string("0.5 0 0.5 setrgbcolor\r\n"); break;
                case 5: print_string("0 0.5 0.5 setrgbcolor\r\n"); break;
                case 6: print_string("0.5 0.5 0 setrgbcolor\r\n"); break;
                case 7: print_string("0.5 0.5 0.5 setrgbcolor\r\n"); break;
            }
            
            print_string("2 setlinewidth\r\n");
            sprintf(label, "newpath 90 %d moveto 110 %d lineto stroke\r\n", y + 3, y + 3);
            print_string(label);
            print_string("1 setlinewidth\r\n");
            
            print_string("0 0 0 setrgbcolor\r\n");
            sprintf(label, "120 %d moveto\r\n", y);
            print_string(label);
            sprintf(label, "(Slot %d: %s - %d samples) show\r\n", 
                    i, g_traces[i].label, g_traces[i].data_count);
            print_string(label);
            
            print_string("/Times-Roman findfont 8 scalefont setfont\r\n");
            sprintf(label, "120 %d moveto\r\n", y - 10);
            print_string(label);
            {
                char *min_unit, *max_unit, *range_unit;
                double min_scale, max_scale, range_scale;
                int min_dec, max_dec, range_dec;
                double trace_range = max_trace - min_trace;
                
                {
                    char *min_ps_unit, *max_ps_unit, *range_ps_unit;
                    get_trace_print_units(i, fabs(min_trace), &min_unit, &min_scale, &min_dec, &min_ps_unit);
                    get_trace_print_units(i, fabs(max_trace), &max_unit, &max_scale, &max_dec, &max_ps_unit);
                    get_trace_print_units(i, trace_range, &range_unit, &range_scale, &range_dec, &range_ps_unit);
                    min_unit = min_ps_unit;
                    max_unit = max_ps_unit;
                    range_unit = range_ps_unit;
                }
                
                {   /* Grid-OS: enough decimals to see the trace's own spread */
                    double res = (trace_range > 0.0 ? trace_range : y_range) / 100.0;
                    min_dec = val_dec(min_dec, res * min_scale);
                    max_dec = val_dec(max_dec, res * max_scale);
                }
                print_string("(  Min: ) show ");
                sprintf(label, "(%.*f) show ", min_dec, min_trace * min_scale);
                print_string(label);
                if (strcmp(min_unit, "uV") == 0) {
                    print_string("( ) show /mu glyphshow (V, Max: ) show ");
                } else {
                    sprintf(label, "(%s, Max: ) show ", min_unit);
                    print_string(label);
                }
                
                sprintf(label, "(%.*f) show ", max_dec, max_trace * max_scale);
                print_string(label);
                /* Calculate and display average instead of range */
                {
                    double avg_val = 0.0;
                    int k;
                    char *avg_unit;
                    double avg_scale;
                    int avg_dec;
                    char *avg_ps_unit;
                    
                    for (k = 0; k < g_traces[i].data_count; k++) {
                        avg_val += g_traces[i].data[k];
                    }
                    avg_val /= g_traces[i].data_count;
                    
                    get_trace_print_units(i, fabs(avg_val), &avg_unit, &avg_scale, &avg_dec, &avg_ps_unit);
                    avg_unit = avg_ps_unit;
                    avg_dec = val_dec(avg_dec, (trace_range > 0.0 ? trace_range : y_range) / 100.0 * avg_scale);
                    
                    if (strcmp(max_unit, "uV") == 0) {
                        print_string("( ) show /mu glyphshow (V, Avg: ) show ");
                    } else {
                        sprintf(label, "(%s, Avg: ) show ", max_unit);
                        print_string(label);
                    }
                    
                    sprintf(label, "(%.*f) show ", avg_dec, avg_val * avg_scale);
                    print_string(label);
                    if (strcmp(avg_unit, "uV") == 0) {
                        print_string("( ) show /mu glyphshow (V) show\r\n");
                    } else {
                        sprintf(label, "(%s) show\r\n", avg_unit);
                        print_string(label);
                    }
                }
            }
            print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
            
            y -= 20;
        }
    }
    
    print_string("grestore\r\n");
    print_string("showpage\r\n");
    print_string("%%EOF\r\n");
    
    lpt_send_byte(0x04);
    
    printf("High-precision PostScript graph printed successfully.\n");
    printf("Press any key to continue...");
    getch();
}

/* Print measurement report */
void print_report(void) {
    char buffer[80];
    int i;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char *type_name;
    double min, max, sum, val;
    double samples_per_second;
    char rate_str[50];
    
    clrscr();
    g_lpt_error = 0;
    printf("Printing report...\n");
    
    print_string("\x1B" "E");
    print_string("\x1B&l0O");
    print_string("\x1B(s0p10h12v0s0b3T");
    
    print_string("TM5000 MEASUREMENT REPORT\r\n");
    print_string("========================\r\n");
    sprintf(buffer, "Date: %02d/%02d/%04d  Time: %02d:%02d:%02d\r\n",
            t->tm_mon + 1, t->tm_mday, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec);
    print_string(buffer);
    
    print_string("\r\nSampling Configuration:\r\n");
    print_string("----------------------\r\n");
    
    if (g_control_panel.sample_rate_ms < 1000) {
        sprintf(rate_str, "%d ms", g_control_panel.sample_rate_ms);
    } else {
        sprintf(rate_str, "%.1f seconds", g_control_panel.sample_rate_ms / 1000.0);
    }
    sprintf(buffer, "Sample Rate: %s\r\n", rate_str);
    print_string(buffer);
    
    if (g_control_panel.sample_rate_ms > 0) {
        samples_per_second = 1000.0 / g_control_panel.sample_rate_ms;
        if (samples_per_second >= 1.0) {
            sprintf(buffer, "Frequency: %.2f samples/second\r\n", samples_per_second);
        } else {
            sprintf(buffer, "Frequency: %.2f samples/minute\r\n", samples_per_second * 60);
        }
        print_string(buffer);
    }
    
    if (g_control_panel.use_custom) {
        print_string("Rate Type: Custom\r\n");
    } else {
        sprintf(buffer, "Rate Type: Preset #%d (%s)\r\n", 
                g_control_panel.selected_rate + 1, 
                (char far *)sample_rate_labels[g_control_panel.selected_rate]);
        print_string(buffer);
    }
    
    print_string("\r\nModule Configuration:\r\n");
    print_string("--------------------\r\n");
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            type_name = "Unknown";
            switch(g_system->modules[i].module_type) {
                case MOD_DC5009: type_name = "DC5009"; break;
                case MOD_DM5010: type_name = "DM5010"; break;
                case MOD_DM5120: type_name = "DM5120"; break;
                case MOD_PS5004: type_name = "PS5004"; break;
                case MOD_PS5010: type_name = "PS5010"; break;
                case MOD_DC5010: type_name = "DC5010"; break;
                case MOD_FG5010: type_name = "FG5010"; break;
            }
            
            sprintf(buffer, "Slot %d: %s, GPIB %d, Last: %.6f\r\n",
                    i, type_name,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].last_reading);
            print_string(buffer);
        }
    }
    
    /* Grid-OS: statistics per slot from its own buffer (the old global
       buffer stopped at 1024 readings and only held the first slot) */
    print_string("\r\nMeasurement Statistics:\r\n");
    print_string("----------------------\r\n");
    for (i = 0; i < 10; i++) {
        tm5000_module *m = &g_system->modules[i];
        unsigned int k;
        long t;
        if (!m->enabled || !m->module_data || m->module_data_count == 0) continue;
        min = max = m->module_data[0];
        sum = 0.0;
        for (k = 0; k < m->module_data_count; k++) {
            val = m->module_data[k];
            if (val < min) min = val;
            if (val > max) max = val;
            sum += val;
        }
        sprintf(buffer, "Slot %d: %u samples", i, m->module_data_count);
        print_string(buffer);
        if (m->avg_shift) {
            sprintf(buffer, " (each the mean of %u readings)", 1u << m->avg_shift);
            print_string(buffer);
        }
        print_string("\r\n");
        t = sample_time_ms(i, m->module_data_count - 1);
        if (t < 0 && m->is_result && m->src_seconds > 0.0f)
            t = (long)(m->src_seconds * 1000.0f);      /* FFT: source measurement */
        if (t >= 0) {
            if (t < 60000L) sprintf(buffer, "  Duration: %.1f seconds (measured)\r\n", t / 1000.0);
            else            sprintf(buffer, "  Duration: %.1f minutes (measured)\r\n", t / 60000.0);
            print_string(buffer);
        }
        sprintf(buffer, "  Minimum: %.10g\r\n", min);  print_string(buffer);
        sprintf(buffer, "  Maximum: %.10g\r\n", max);  print_string(buffer);
        sprintf(buffer, "  Average: %.10g\r\n", sum / m->module_data_count);  print_string(buffer);
    }
    
    lpt_send_byte(0x0C);
    
    printf("Report printed successfully.\n");
    printf("Press any key to continue...");
    getch();
}