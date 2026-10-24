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

typedef struct {
    char function[10];      /* Current function (DCV, ACV, OHMS, CURR, DIODE) */
    unsigned char range_mode;         /* 0=AUTO, 1-7=Manual range */
    unsigned char filter_count;       /* Filter count 1-100 */
    unsigned char trigger_mode;       /* 0=FREE, 1=SINGLE, 2=EXT */
    unsigned char digits;            /* Display digits (4-5) */
    float nullval;         /* Null/REL value for relative measurements */
    unsigned char calculation_mode;  /* 0=NONE, 1=AVG, 2=SCALE, 3=DBM, 4=DBR */
    float scale_factor;    /* Scaling factor for calculations */
    float scale_offset;    /* Scaling offset for calculations */
    unsigned char avg_count;         /* Number of readings for averaging */
    float dbm_reference;   /* dBm reference resistance (default 600) */
    float dbr_reference;   /* dBr reference value */
    float min_value;       /* Minimum measured value */
    float max_value;       /* Maximum measured value */
    
    unsigned int filter_enabled:1;     /* Digital filter */
    unsigned int null_enabled:1;      /* NULL/REL */
    unsigned int data_format:1;       /* 0=ASCII, 1=Scientific notation */
    unsigned int auto_zero:1;
    unsigned int beeper_enabled:1;
    unsigned int front_panel_lock:1;
    unsigned int high_speed_mode:1;
    unsigned int statistics_enabled:1; /* Track min/max */
    unsigned int lf_termination:1;    /* terminate with LF, not CRLF */
    unsigned int reserved:7;
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

/* PS5010 slot settings, sent by Apply. Stored raw in PROFILES.DAT: the layout is
   the v3.7 one, with bits that never held a setting renamed. */
typedef struct {
    float voltage1;             /* VPOS, 0-32 V */
    float current_limit1;       /* IPOS, 0.05 A steps; max per compartment */
    float voltage2;             /* VNEG magnitude, 0-32 V */
    float current_limit2;       /* INEG */
    float logic_voltage;        /* VLOG, 4.50-5.50 V */
    float logic_current_limit;  /* ILOG, 0.1-3.0 A in 0.1 A steps */
    float unused_f[3];          /* no PS5010 command: tracking ratio, OVP levels */
    unsigned char unused_c[4];  /* no PS5010 command: tracking/display modes */

    unsigned int fsout_enabled:1;   /* FSOUT: + and - floating supplies switch together */
    unsigned int unused_out2:1;     /* was a CH2 output flag; the supply has no such switch */
    unsigned int logic_enabled:1;   /* LSOUT */
    unsigned int high_power:1;      /* in the high-power compartment (was OVP, always 0) */
    unsigned int unused_b:4;
    unsigned int srq_enabled:1;     /* RQS */
    unsigned int unused_lf:1;
    unsigned int pri_enabled:1;     /* regulation-change SRQs, manual p. 3-8..3-10 */
    unsigned int nri_enabled:1;
    unsigned int lri_enabled:1;
    unsigned int user_enabled:1;    /* USER: SRQ on the INST ID button */
    unsigned int dt_enabled:1;      /* DT SET: new settings wait for GET */
    unsigned int reserved:1;
} ps5010_config;

typedef struct {
    char function[8];           /* FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, etc. */
    char channel[4];            /* A, B, AB, BA, etc. */
    float gate_time;            /* Gate time in seconds */
    int averaging;              /* -1=auto, 1=no avg, >1=avg count */
    char coupling_a[4];         /* AC, DC */
    char coupling_b[4];         /* AC, DC */
    char impedance_a[4];        /* HI, LO */
    char impedance_b[4];        /* HI, LO */
    char attenuation_a[4];      /* X1, X10 */
    char attenuation_b[4];      /* X1, X10 */
    char slope_a[4];            /* POS, NEG */
    char slope_b[4];            /* POS, NEG */
    float level_a;              /* Trigger level Channel A */
    float level_b;              /* Trigger level Channel B */
    long overflow_count;        /* Extended range overflow counter */
    float last_measurement;     /* Last measurement result */
    
    unsigned int filter_enabled:1;         /* Input filter */
    unsigned int auto_trigger:1;
    unsigned int overflow_enabled:1;       /* Overflow indication */
    unsigned int preset_enabled:1;
    unsigned int srq_enabled:1;
    unsigned int measurement_complete:1;
    unsigned int lf_termination:1;         /* terminate with LF, not CRLF */
    unsigned int reserved:9;
} dc5009_config;

typedef struct {
    char function[8];           /* FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, etc. */
    char channel[4];            /* A, B, AB, BA, etc. */
    float gate_time;            /* Gate time in seconds */
    int averaging;              /* -1=auto, 1=no avg, >1=avg count */
    char coupling_a[4];         /* AC, DC */
    char coupling_b[4];         /* AC, DC */
    char impedance_a[4];        /* HI, LO */
    char impedance_b[4];        /* HI, LO */
    char attenuation_a[4];      /* X1, X10 */
    char attenuation_b[4];      /* X1, X10 */
    char slope_a[4];            /* POS, NEG */
    char slope_b[4];            /* POS, NEG */
    float level_a;              /* Trigger level Channel A */
    float level_b;              /* Trigger level Channel B */
    long overflow_count;        /* Extended range overflow counter */
    float last_measurement;     /* Last measurement result */
    
    unsigned int filter_enabled:1;         /* Input filter */
    unsigned int auto_trigger:1;
    unsigned int overflow_enabled:1;       /* Overflow indication */
    unsigned int preset_enabled:1;
    unsigned int srq_enabled:1;
    unsigned int measurement_complete:1;
    unsigned int rise_fall_enabled:1;      /* DC5010 only */
    unsigned int burst_mode:1;             /* DC5010 only */
    unsigned int lf_termination:1;         /* terminate with LF, not CRLF */
    unsigned int reserved:7;
} dc5010_config;

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
