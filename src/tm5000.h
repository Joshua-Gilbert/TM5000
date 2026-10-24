/*
 * TM5000 GPIB Control System for the Gridcase 1520: shared types, globals, prototypes.
 * C89, 16-bit large model (int is 2 bytes); sources are split per module to fit 64 KB segments.
 */

#ifndef TM5000_H
#define TM5000_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <math.h>
#include <time.h>
#include <malloc.h>
#include <i86.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <errno.h>

/* Version information */
#define TM5000_VERSION "3.7"
#define TM5000_VERSION_MAJOR 3
#define TM5000_VERSION_MINOR 7

/* Per-slot sample buffer limits */
#define MAX_SAMPLES_PER_MODULE 1024
/* The monitor pools ten slots' worth of samples among the modules it reads.
   POOLED_MAX caps one module: 8192 doubles = one whole 64 KB segment (alloc_samples). */
#define POOL_SAMPLES   (10L * MAX_SAMPLES_PER_MODULE)
#define POOLED_MAX     8192u
#define MIN_BUFFER_SIZE 10
#define MAX_BUFFER_SIZE MAX_SAMPLES_PER_MODULE

/* DM5120 specific hardware limits */
#define DM5120_MAX_BUFFER_SIZE 500
#define DM5120_MIN_BUFFER_SIZE 10
#define DM5120_DEFAULT_BUFFER_SIZE 250

#define GPIB_BUFFER_SIZE 128

/* GPIB timeout codes */
#define T10s    13         /* 10 seconds timeout */
#define T1s     11         /* 1 second timeout */
#define T300ms  10         /* 300 milliseconds timeout */

/* Graphics definitions */
#define CGA_MEMORY      0xB8000000L
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   200
/* Fixed wait after most bus calls, needed under Driver488. TM5000G builds
 * modules.c and module_funcs.c with -DGRIDGPIB: GRIDGPIB calls return only
 * once the bus transaction is complete, so the waits compile out. */
#ifdef GRIDGPIB
#define GPIB_PACE(ms) ((void)0)
#else
#define GPIB_PACE(ms) delay(ms)
#endif

#define LPT1_BASE       0x378
#define LPT1_STATUS     (LPT1_BASE + 1)
#define LPT1_CONTROL    (LPT1_BASE + 2)
#define GRAPH_LEFT      35    
#define GRAPH_RIGHT     260   
#define GRAPH_TOP       16    
#define GRAPH_BOTTOM    175   
#define GRAPH_WIDTH     (GRAPH_RIGHT - GRAPH_LEFT)
#define GRAPH_HEIGHT    (GRAPH_BOTTOM - GRAPH_TOP)

/* Module type definitions */
#define MOD_NONE     0
#define MOD_DC5009   1
#define MOD_DM5010   2
#define MOD_DM5120   3
#define MOD_PS5004   4
#define MOD_PS5010   5
#define MOD_DC5010   6
#define MOD_FG5010   7

/* Unit types for traces */
#define UNIT_VOLTAGE    0
#define UNIT_FREQUENCY  1
#define UNIT_DB         2
#define UNIT_DERIVATIVE 3    /* V/s for differentiation */
#define UNIT_CURRENT    4    /* A, mA, µA for current measurements */
#define UNIT_RESISTANCE 5    /* Ω, mΩ, µΩ for resistance measurements */
#define UNIT_POWER      6    /* Power spectrum units (V²/Hz) */
#define UNIT_TIME       7    /* s, ms, us, ns (counter period/width/interval) */
#define UNIT_COUNT      8    /* dimensionless: counts, ratios */

/* Mouse definitions */
#define MOUSE_INT       0x33
#define MOUSE_RESET     0x00
#define MOUSE_SHOW      0x01
#define MOUSE_HIDE      0x02
#define MOUSE_STATUS    0x03
#define MOUSE_SET_POS   0x04

/* Compiler compatibility */
#ifdef __WATCOMC__
#define outportb(port, value) outp(port, value)
#define inportb(port) inp(port)
#else
void outportb(unsigned int port, unsigned char value);
unsigned char inportb(unsigned int port);
#endif

/* Global handles */
extern int ieee_out;
extern int ieee_in;
extern int gpib_error;

#pragma pack(1)
typedef struct {
    double far *module_data;     /* double keeps full counter resolution */
    char description[12];
    double last_reading;
    unsigned int module_data_count;
    unsigned int module_data_size;   /* samples allocated */
    unsigned char module_type;
    unsigned char slot_number;
    unsigned char gpib_address;
    unsigned char enabled:1;
    unsigned char is_result:1;   /* computed trace (FFT/math): no GPIB, exempt from phantom cleanup */
    unsigned char time_ok:1;     /* every stored sample has a measurement-time stamp */
    unsigned char reserved:5;
    /* Time stamps, and averaging once the buffer fills */
    unsigned long far *sample_t; /* measurement clock (BIOS ticks) of each sample (mean of its group) */
    unsigned long t_origin;      /* measurement clock at the run's first reading */
    double acc_v, acc_t;         /* readings waiting to be averaged into the next sample */
    unsigned int acc_n;
    unsigned char avg_shift;     /* each stored sample = mean of 2^avg_shift readings */
    unsigned char src_measured;  /* computed trace: src_seconds came from time stamps */
    float src_seconds;           /* computed trace (FFT): duration of the measurement used */
} tm5000_module;
#pragma pack()

typedef struct {
    tm5000_module modules[10];
    int gpib_devices[10];
    double far *data_buffer;
    unsigned int buffer_size;
    unsigned int data_count;
    int sample_rate;
} measurement_system;

typedef struct {
    int x, y;
    unsigned char present:1;
    unsigned char left_button:1;
    unsigned char right_button:1;
    unsigned char reserved:5;
} mouse_state;

typedef struct {
    char custom_rate[8];
    int sample_rate_ms;       /* delay between samples, ms */
    int selected_rate;        /* index into sample_rate_presets */
    int monitor_mask;         /* bit per slot to monitor */
    unsigned char running:1;
    unsigned char use_custom:1; /* 0=preset, 1=custom_rate */
    unsigned char monitor_all:1; /* 0=use monitor_mask, 1=all enabled modules */
    unsigned char auto_rate:1;   /* 1=AUTO: fastest cycle the instruments allow */
    unsigned char reserved:4;
} control_panel_state;

typedef struct {
    float custom_sample_rate;
    int input_points;      /* power of 2, 64..8192 */
    int output_points;     /* power of 2, 64..8192 (≤ input) */
    int window_type;       /* 0=Rectangular, 1=Hamming, 2=Hanning, 3=Blackman */
    int output_format;     /* 0=dB, 1=Linear magnitude, 2=Power */
    unsigned char zero_pad:1;      /* pad to next power of 2 */
    unsigned char dc_remove:1;     /* remove DC before FFT */
    unsigned char peak_centering:1; /* center output on highest peak */
    unsigned char reserved:5;
} fft_config;

typedef struct {
    float min_value;
    float max_value;
    float scale_factor;
    float pan_offset;
    float zoom_factor;
    int grid_divisions;
    int sample_start;        /* first sample index displayed */
    int sample_count;        /* samples displayed (0 = all) */
    unsigned char auto_scale:1;
    unsigned char reserved:7;
} graph_scale;

typedef struct {
    char label[12];
    double *data;                /* points at the slot's module_data, not owned */
    float x_scale;               /* frequency traces: Hz per sample */
    float x_offset;              /* X-axis offset (for centering) */
    int data_count;
    int slot;
    int unit_type;               /* UNIT_* */
    unsigned char color;
    unsigned char enabled:1;
    unsigned char reserved:7;
} trace_info;

/* The *_config structs below are written raw into PROFILES.DAT (config_profiles.h),
   as are control_panel_state, fft_config and graph_scale above: any layout change
   needs a PROFILE_VERSION bump. */
typedef struct {
    char function[12];      /* Current function (DCV, ACV, OHMS, etc.) */
    int range_mode;         /* 0=AUTO, 1-7=Manual range */
    int filter_value;       /* Filter value 1-99 */
    int trigger_source;     /* 0=TALK, 1=EXT (DM5120 trigger source) */
    int trigger_mode;       /* 0=CONT, 1=ONE (DM5120 trigger mode) */
    int digits;            /* Number of digits (3-6) */
    float nullval;         /* Null value for relative measurements */
    int buffer_size;       /* Number of samples to buffer */
    float min_value;       /* Minimum value tracked */
    float max_value;       /* Maximum value tracked */
    int sample_count;      /* Samples taken since last clear */
    float sample_rate;     /* Internal sample rate (if supported) */
    
    /* Async buffer fill tracking */
    int buffer_state;      /* 0=idle, 1=filling, 2=half, 3=full */
    unsigned long buffer_start_time; /* tick count when fill started */
    int samples_ready;     /* Number of samples currently in buffer */

    unsigned int filter_enabled:1;
    unsigned int null_enabled:1;
    unsigned int data_format:1;       /* 0=ASCII, 1=Scientific notation */
    unsigned int buffer_enabled:1;    /* Use internal buffering */
    unsigned int min_max_enabled:1;
    unsigned int burst_mode:1;        /* 0=Normal, 1=Burst sampling */
    unsigned int lf_termination:1;    /* terminate with LF, not CRLF */
    /* 0 = ON, the factory state, so profiles saved before these bits read as ON */
    unsigned int autocal_off:1;
    unsigned int intfilt_off:1;
    unsigned int reserved:7;
} dm5120_config;

/* DM5010: Tek Codes & Formats settings (manual pp. 3-3/3-4). Same layout as the
   v3.7 struct; renamed fields reuse ones that held 0 (or, inverted, 1) in old
   profiles, and dm5010_fix_legacy_config converts the rest. */
typedef struct {
    char function[10];      /* DCV, ACV, ACDC, OHMS or DIODE */
    unsigned char range_mode;         /* 0=AUTO, 1-6 = full-scale index (modules.c table) */
    unsigned char filter_count;       /* unused */
    unsigned char trigger_mode;       /* 0=MODE RUN, 1=MODE TRIG */
    unsigned char digits;            /* unused: DIGIT is high_speed_mode */
    float nullval;         /* NULL offset, V or ohms */
    unsigned char calculation_mode;  /* dB: 0=off, 3=CALC DBM, 4=CALC DBR */
    float scale_factor;    /* RATIO A, not 0 */
    float scale_offset;    /* RATIO B */
    unsigned char avg_count;         /* AVE N, 1-255 here */
    float dbm_reference;   /* unused: dBm is fixed at 1 mW in 600 ohm (p. 3-7) */
    float dbr_reference;   /* DBR ref, not 0 */
    float limit_hi;        /* LIMITS first argument (upper button) */
    float limit_lo;        /* LIMITS second argument */

    unsigned int filter_enabled:1;     /* CALC AVE */
    unsigned int null_enabled:1;
    unsigned int source_front:1;      /* 1 = SOURCE FRONT, the power-on state */
    unsigned int rqs_on:1;            /* 1 = RQS ON, the power-on state */
    unsigned int unused_bit:1;
    unsigned int lfr_on:1;
    unsigned int high_speed_mode:1;   /* DIGIT 3.5 */
    unsigned int statistics_enabled:1; /* Track min/max */
    unsigned int lf_termination:1;    /* terminate with LF, not CRLF */
    unsigned int ratio_on:1;          /* CALC RATIO */
    unsigned int cmpr_on:1;           /* CALC CMPR */
    unsigned int dt_trig:1;           /* DT TRIG */
    unsigned int monitor_on:1;        /* MONITOR ON */
    unsigned int opc_on:1;
    unsigned int over_on:1;
    unsigned int user_on:1;
} dm5010_config;

typedef struct {
    float voltage;          /* Output voltage setting (0-20V) */
    float current_limit;    /* Current limit (10-305mA) */
    int display_mode;       /* 0=Voltage, 1=Current, 2=Current Limit */
    
    unsigned int output_enabled:1;
    unsigned int vri_enabled:1;        /* Voltage regulation interrupt */
    unsigned int cri_enabled:1;        /* Current regulation interrupt */
    unsigned int uri_enabled:1;        /* Unregulated interrupt */
    unsigned int dt_enabled:1;         /* Device trigger */
    unsigned int user_enabled:1;       /* INST ID button */
    unsigned int rqs_enabled:1;        /* Service requests */
    unsigned int lf_termination:1;     /* terminate with LF, not CRLF */
    unsigned int reserved:8;
} ps5004_config;

typedef struct {
    /* Positive/Negative Floating Supplies - Constant Voltage Mode: 0 to ±32V */
    float voltage1;             /* Positive supply voltage (0 to +32V) */
    float current_limit1;       /* Current limit Ch1: 50mA-750mA (1.6A @ ≤15V) */
    float voltage2;             /* Negative supply voltage (0 to -32V) */
    float current_limit2;       /* Current limit Ch2: 50mA-750mA (1.6A @ ≤15V) */
    
    /* Logic Supply - Constant Voltage Mode: 4.5-5.5V */
    float logic_voltage;        /* Logic supply voltage (4.5-5.5V) */
    float logic_current_limit;  /* Logic current limit (100mA-3.0A) */
    float tracking_ratio;       /* Ratio for asymmetric tracking */
    float ovp_level1;           /* OVP threshold channel 1 */
    float ovp_level2;           /* OVP threshold channel 2 */
    
    unsigned char tracking_mode;          /* 0=Independent, 1=Series, 2=Parallel */
    unsigned char display_channel;        /* 0=Ch1, 1=Ch2, 2=Logic */
    unsigned char display_mode;           /* 0=Voltage, 1=Current */
    unsigned char error_queue_size;       /* Number of errors in queue */
    
    unsigned int output1_enabled:1;
    unsigned int output2_enabled:1;
    unsigned int logic_enabled:1;
    unsigned int ovp_enabled:1;            /* Over voltage protection */
    unsigned int cv_mode1:1;               /* Ch1 constant voltage */
    unsigned int cc_mode1:1;               /* Ch1 constant current */
    unsigned int cv_mode2:1;               /* Ch2 constant voltage */
    unsigned int cc_mode2:1;               /* Ch2 constant current */
    unsigned int srq_enabled:1;
    unsigned int lf_termination:1;         /* terminate with LF, not CRLF */
    unsigned int reserved:6;
} ps5010_config;

/* DC5009 and DC5010 share one layout, the same size as before. Fields a model
   lacks keep their defaults. */
typedef struct {
    char function[8];           /* header: FREQ PER WID RAT TIME TOT EVE TMAN PROB RISE FALL */
    char channel[4];            /* its argument: A B/A AB BA A+B A-B A&B, or "" */
    float gate_time;            /* unused: neither counter has a gate time */
    int averaging;              /* -1 = auto (~0.3 s), else n for AVE 1.E+n */
    char coupling_a[4];         /* AC, DC */
    char coupling_b[4];
    char impedance_a[4];        /* DC5010 termination: HI (1 Mohm) or LO (50 ohm) */
    char impedance_b[4];
    char attenuation_a[4];      /* "1" or "5" (X1 / X5) */
    char attenuation_b[4];
    char slope_a[4];            /* POS, NEG */
    char slope_b[4];
    float level_a;              /* programmed trigger level, V */
    float level_b;
    long overflow_count;        /* TOT/TMAN counter overflows seen (OVER ON) */
    float last_measurement;     /* last reading: sets the expected measurement time */

    unsigned int filter_enabled:1;         /* DC5010 FIL ON (20 MHz) */
    unsigned int auto_trigger:1;           /* AUTO levels (DC5009 both channels, DC5010 A) */
    unsigned int overflow_enabled:1;       /* OVER ON */
    unsigned int preset_enabled:1;         /* PRE ON: /16 prescaler on channel A */
    unsigned int srq_enabled:1;            /* RQS ON */
    unsigned int opc_enabled:1;            /* OPC ON */
    unsigned int lf_termination:1;         /* terminate with LF, not CRLF */
    unsigned int null_enabled:1;           /* DC5010 NULL ON */
    unsigned int fptrig:1;                 /* DC5009 levels from the front-panel knobs */
    unsigned int rear_a:1;                 /* DC5009 SOUR REAR, channel A */
    unsigned int rear_b:1;
    unsigned int user_enabled:1;           /* USER ON */
    unsigned int dt_mode:2;                /* DT OFF / TRIG / GATE */
    unsigned int b_level_swap:1;           /* DC5010: channel B in the other level mode than A */
    unsigned int reserved:1;
} dc5009_config;

typedef dc5009_config dc5010_config;
unsigned dc50xx_timeout_ms(int slot);   /* modules.c; gpib_gg.c's counter read timeout */

typedef struct {
    char waveform[12];
    char trigger_source[10];    /* INT, EXT, MAN */
    char trigger_slope[4];      /* POS, NEG */
    char mod_type[4];           /* AM, FM */
    float frequency;            /* Hz (0.1 Hz to 15 MHz) */
    float amplitude;            /* Vpp (0.01 to 20 Vpp) */
    float offset;               /* DC offset, V (-10 to +10) */
    float duty_cycle;           /* square/pulse, % (1 to 99) */
    float start_freq;           /* Sweep start frequency */
    float stop_freq;            /* Sweep stop frequency */
    float sweep_time;           /* s (0.1 to 999) */
    float trigger_level;        /* External trigger level, V (-5 to +5) */
    float phase;                /* degrees (0-360) */
    float mod_freq;             /* Modulation frequency */
    float mod_depth;            /* Modulation depth (%) */
    float burst_period;         /* Burst repetition period */
    int burst_count;            /* cycles per burst */
    int sweep_type;             /* 0=Linear, 1=Log */
    int units_freq;             /* 0=Hz, 1=kHz, 2=MHz */
    int units_time;             /* 0=s, 1=ms, 2=us */

    unsigned int output_enabled:1;
    unsigned int sweep_enabled:1;
    unsigned int sync_enabled:1;       /* Sync output */
    unsigned int invert_enabled:1;
    unsigned int modulation_enabled:1; /* AM/FM modulation */
    unsigned int burst_enabled:1;
    unsigned int lf_termination:1;     /* terminate with LF, not CRLF */
    unsigned int reserved:9;
} fg5010_config;

extern measurement_system *g_system;
extern unsigned char far *video_mem;
extern char g_error_msg[64];
extern dm5120_config __far g_dm5120_config[10];
extern dm5010_config __far g_dm5010_config[10];
extern ps5004_config __far g_ps5004_config[10];
extern ps5010_config __far g_ps5010_config[10];
extern dc5009_config __far g_dc5009_config[10];
extern dc5010_config __far g_dc5010_config[10];
extern fg5010_config __far g_fg5010_config[10];
extern mouse_state g_mouse;
extern graph_scale g_graph_scale;
extern trace_info __far g_traces[10];
extern control_panel_state g_control_panel;
extern fft_config g_fft_config;
extern int g_has_287;

/* Sample rate presets */
extern int __far sample_rate_presets[];
extern char __far *sample_rate_labels[];
#define NUM_RATE_PRESETS 7

/* Function prototypes from other modules */

/* From gpib.c */
int init_gpib_system(void);
int ieee_write(const char *str);
int ieee_read(char *buffer, int maxlen);
int gpib_write(int address, char *command);
int gpib_read(int address, char *buffer, int maxlen);
int gpib_read_float(int address, float *value);
void gpib_remote(int address);
void gpib_local(int address);
void gpib_clear(int address);
int command_has_response(const char *cmd);
void drain_input_buffer(void);

/* From graphics.c */
void init_graphics(void);
void text_mode(void);
void plot_pixel(int x, int y, unsigned char color);
void draw_line(int x1, int y1, int x2, int y2, unsigned char color);
void clrscr(void);
void gotoxy(int x, int y);
void textattr(unsigned char attr);
void clreol(void);

/* From ui.c */
void main_menu(void);
void display_error(char *msg);
int wait_for_input(void);

/* From modules.c */
void configure_modules(void);
void single_measurement(void);
void continuous_monitor(void);

/* DM5120 Buffer Functions */
float dm5120_get_buffer_average(int address);
int dm5120_get_buffer_count(int address);
float dm5120_get_buffer_min(int address);
float dm5120_get_buffer_max(int address);
void dm5120_set_buffer_size(int address, int size);
int dm5120_query_buffer_size(int address);
float dm5120_read_one_stored(int address);
int dm5120_read_all_stored(int address, float far *buffer, int max_samples);

/* From data.c */
int allocate_module_buffer(int slot, unsigned int size);
void free_module_buffer(int slot);
void store_module_data(int slot, double value);
void clear_module_data(int slot);
void save_data(void);
void load_data(void);

/* From print.c */
void print_report(void);
void print_graph_menu(void);

/* From main.c */
void delay(unsigned int milliseconds);
void cleanup(void);

#endif /* TM5000_H */
