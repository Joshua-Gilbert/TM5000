/*
 * TM5000 GPIB Control System for Gridcase 1520
 * Version 3.5 - Data Management Foundation with Configuration Profiles
 * Main header file with common definitions
 * C89 compliant
 *
 * Changes in v3.0:
 * - Split monolithic source into logical modules for DOS segment size limits
 * - Improved maintainability and expansion capability
 * - Preserved all v2.9 functionality
 * - Enhanced modular design for future expansion
 * - Fixed graph display crashes by adding comprehensive divide-by-zero protection
 * - Implemented proper engineering scale logic for grid lines and range display
 * - Added per-division calculation with 1-2-5 engineering increment rounding
 * - Fixed unit switching based on per-division values (V/mV/µV) for instrument-grade scaling
 * - Enhanced initial range from ±10V to ±1000V for full meter capability coverage
 * - Fixed LF termination support for all DM5120 GPIB functions (remote, local, clear)
 * - Improved bottom menu readability with better spacing and simplified text
 * - Added clean fixed-format slot readout (S[n][sample]:XX.XX V) for consistency
 * - Maintained ultra-precision 1µV zoom capability for DM5120 high-precision measurements
 * - Fixed grid line labels to properly match range display units during zoom operations
 * - Enhanced zoom behavior to properly switch units when per-division < 0.1V (V→mV→µV)
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
#define TM5000_VERSION "3.5"
#define TM5000_VERSION_MAJOR 3
#define TM5000_VERSION_MINOR 5

/* Buffer size constants - v3.4 enhanced capacity */
#define MAX_SAMPLES_PER_MODULE 1024
#define MIN_BUFFER_SIZE 10
#define MAX_BUFFER_SIZE MAX_SAMPLES_PER_MODULE

/* DM5120 specific hardware limits */
#define DM5120_MAX_BUFFER_SIZE 500
#define DM5120_MIN_BUFFER_SIZE 10
#define DM5120_DEFAULT_BUFFER_SIZE 250

/* GPIB buffer optimization - v3.4 memory optimization */
#define GPIB_BUFFER_SIZE 128

/* GPIB timeout definitions */
#define T10s    13         /* 10 seconds timeout */
#define T1s     11         /* 1 second timeout */
#define T300ms  10         /* 300 milliseconds timeout */

/* Graphics definitions */
#define CGA_MEMORY      0xB8000000L
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   200
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
extern int ieee_out;  /* Handle for writing to GPIB */
extern int ieee_in;   /* Handle for reading from GPIB */
extern int gpib_error;

/* Structure definitions - optimized member ordering and bit fields */
#pragma pack(1)
typedef struct {
    float far *module_data;      /* 4 bytes - far pointer (keep for compatibility) */
    char description[12];        /* 12 bytes - reduced size */
    float last_reading;          /* 4 bytes */
    unsigned int module_data_count;  /* 4 bytes - Count for this module */
    unsigned int module_data_size;   /* 4 bytes - Size allocated */
    unsigned char module_type;   /* 1 byte */
    unsigned char slot_number;   /* 1 byte */
    unsigned char gpib_address;  /* 1 byte */
    unsigned char enabled:1;     /* 1 bit - pack boolean flags */
    unsigned char reserved:7;    /* 7 bits - reserved for future flags */
} tm5000_module;
#pragma pack()

typedef struct {
    tm5000_module modules[10];   /* 10 * 35 = 350 bytes - largest member first */
    int gpib_devices[10];        /* 40 bytes - second largest array */
    float far *data_buffer;      /* 4 bytes - far pointer */
    unsigned int buffer_size;    /* 4 bytes */
    unsigned int data_count;     /* 4 bytes */
    int sample_rate;             /* 4 bytes */
} measurement_system;

typedef struct {
    int x, y;                    /* 8 bytes - largest members first */
    unsigned char present:1;     /* 1 bit - pack boolean flags */
    unsigned char left_button:1; /* 1 bit */
    unsigned char right_button:1;/* 1 bit */
    unsigned char reserved:5;    /* 5 bits - reserved for future flags */
} mouse_state;

typedef struct {
    char custom_rate[8];      /* 8 bytes - reduced size */
    int sample_rate_ms;       /* 4 bytes - Delay between samples in milliseconds */
    int selected_rate;        /* 4 bytes - Index of selected rate preset */
    int monitor_mask;         /* 4 bytes - Bit mask of which modules to monitor */
    unsigned char running:1;  /* 1 bit - pack boolean flags */
    unsigned char use_custom:1; /* 1 bit - 0=use preset, 1=use custom */
    unsigned char monitor_all:1; /* 1 bit - 0=selective, 1=monitor all enabled modules */
    unsigned char reserved:5; /* 5 bits - reserved for future flags */
} control_panel_state;

/* FFT Configuration structure - optimized member ordering and bit fields */
typedef struct {
    float custom_sample_rate; /* 4 bytes - largest member first */
    int input_points;      /* 4 bytes - 64, 128, 256, 512, 1024 */
    int output_points;     /* 4 bytes - 32, 64, 128, 256, 512, 1024 (≤ input) */
    int window_type;       /* 4 bytes - 0=Rectangular, 1=Hamming, 2=Hanning, 3=Blackman */
    int output_format;     /* 4 bytes - 0=dB, 1=Linear magnitude, 2=Power */
    unsigned char zero_pad:1;      /* 1 bit - Auto-pad to next power of 2 */
    unsigned char dc_remove:1;     /* 1 bit - Remove DC component before FFT */
    unsigned char peak_centering:1; /* 1 bit - Center FFT output on highest peak */
    unsigned char reserved:5;      /* 5 bits - reserved for future flags */
} fft_config;

typedef struct {
    float min_value;         /* 4 bytes - largest members first */
    float max_value;         /* 4 bytes */
    float scale_factor;      /* 4 bytes */
    float pan_offset;        /* 4 bytes */
    float zoom_factor;       /* 4 bytes */
    int grid_divisions;      /* 4 bytes */
    int sample_start;        /* 4 bytes - Starting sample index for display */
    int sample_count;        /* 4 bytes - Number of samples to display (0 = all) */
    unsigned char auto_scale:1; /* 1 bit - pack boolean flags */
    unsigned char reserved:7;   /* 7 bits - reserved for future flags */
} graph_scale;

typedef struct {
    char label[12];              /* 12 bytes - reduced size */
    float *data;                 /* 4 bytes - pointer (keep for compatibility) */
    float x_scale;               /* 4 bytes - For frequency traces: Hz per sample */
    float x_offset;              /* 4 bytes - X-axis offset (for centering) */
    int data_count;              /* 4 bytes */
    int slot;                    /* 4 bytes */
    int unit_type;               /* 4 bytes - 0=Voltage, 1=Frequency, 2=dB */
    unsigned char color;         /* 1 byte */
    unsigned char enabled:1;     /* 1 bit - pack boolean flags */
    unsigned char reserved:7;    /* 7 bits - reserved */
} trace_info;

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
    
    /* Buffer state tracking for async operations */
    int buffer_state;      /* 0=idle, 1=filling, 2=half, 3=full */
    unsigned long buffer_start_time; /* When buffer fill started (tick count) */
    int samples_ready;     /* Number of samples currently in buffer */
    
    /* Bit-packed boolean flags to save memory */
    unsigned int filter_enabled:1;     /* Filter ON/OFF */
    unsigned int null_enabled:1;      /* NULL function ON/OFF */
    unsigned int data_format:1;       /* 0=ASCII, 1=Scientific notation */
    unsigned int buffer_enabled:1;    /* Use internal buffering */
    unsigned int min_max_enabled:1;   /* Track min/max values */
    unsigned int burst_mode:1;        /* 0=Normal, 1=Burst sampling */
    unsigned int lf_termination:1;    /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:9;          /* Reserved for future use */
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
    
    /* Bit-packed boolean flags to save memory */
    unsigned int filter_enabled:1;     /* Digital filter ON/OFF */
    unsigned int null_enabled:1;      /* NULL/REL function ON/OFF */
    unsigned int data_format:1;       /* 0=ASCII, 1=Scientific notation */
    unsigned int auto_zero:1;         /* Auto-zero ON/OFF */
    unsigned int beeper_enabled:1;    /* Beeper ON/OFF */
    unsigned int front_panel_lock:1;  /* Front panel lockout */
    unsigned int high_speed_mode:1;   /* High speed acquisition mode */
    unsigned int statistics_enabled:1; /* Track min/max statistics */
    unsigned int lf_termination:1;    /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:7;          /* Reserved for future use */
} dm5010_config;

typedef struct {
    float voltage;          /* Output voltage setting (0-20V) */
    float current_limit;    /* Current limit (10-305mA) */
    int display_mode;       /* 0=Voltage, 1=Current, 2=Current Limit */
    
    /* Bit-packed boolean flags to save memory */
    unsigned int output_enabled:1;     /* Output ON/OFF */
    unsigned int vri_enabled:1;        /* Voltage regulation interrupt */
    unsigned int cri_enabled:1;        /* Current regulation interrupt */
    unsigned int uri_enabled:1;        /* Unregulated interrupt */
    unsigned int dt_enabled:1;         /* Device trigger */
    unsigned int user_enabled:1;       /* INST ID button */
    unsigned int rqs_enabled:1;        /* Service requests */
    unsigned int lf_termination:1;     /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:8;           /* Reserved for future use */
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
    
    /* Bit-packed boolean flags to save memory */
    unsigned int output1_enabled:1;        /* Channel 1 output ON/OFF */
    unsigned int output2_enabled:1;        /* Channel 2 output ON/OFF */
    unsigned int logic_enabled:1;          /* Logic output ON/OFF */
    unsigned int ovp_enabled:1;            /* Over voltage protection */
    unsigned int cv_mode1:1;               /* Channel 1 in constant voltage mode */
    unsigned int cc_mode1:1;               /* Channel 1 in constant current mode */
    unsigned int cv_mode2:1;               /* Channel 2 in constant voltage mode */
    unsigned int cc_mode2:1;               /* Channel 2 in constant current mode */
    unsigned int srq_enabled:1;            /* Enable service requests */
    unsigned int lf_termination:1;         /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:6;               /* Reserved for future use */
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
    
    /* Bit-packed boolean flags to save memory */
    unsigned int filter_enabled:1;         /* Input filter ON/OFF */
    unsigned int auto_trigger:1;           /* Auto trigger mode */
    unsigned int overflow_enabled:1;       /* Overflow indication */
    unsigned int preset_enabled:1;         /* Preset function */
    unsigned int srq_enabled:1;            /* Service request */
    unsigned int measurement_complete:1;   /* Measurement status flag */
    unsigned int lf_termination:1;         /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:9;               /* Reserved for future use */
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
    
    /* Bit-packed boolean flags to save memory */
    unsigned int filter_enabled:1;         /* Input filter ON/OFF */
    unsigned int auto_trigger:1;           /* Auto trigger mode */
    unsigned int overflow_enabled:1;       /* Overflow indication */
    unsigned int preset_enabled:1;         /* Preset function */
    unsigned int srq_enabled:1;            /* Service request */
    unsigned int measurement_complete:1;   /* Measurement status flag */
    unsigned int rise_fall_enabled:1;      /* Rise/fall time measurements (DC5010 specific) */
    unsigned int burst_mode:1;             /* Burst measurement mode (DC5010 specific) */
    unsigned int lf_termination:1;         /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:7;               /* Reserved for future use */
} dc5010_config;

typedef struct {
    char waveform[12];          /* 12 bytes - largest string first */
    char trigger_source[10];    /* 10 bytes - INT, EXT, MAN */
    char trigger_slope[4];      /* 4 bytes - POS, NEG */
    char mod_type[4];           /* 4 bytes - AM, FM */
    float frequency;            /* 4 bytes - Output frequency in Hz (0.1 Hz to 15 MHz) */
    float amplitude;            /* 4 bytes - Output amplitude in Vpp (0.01V to 20Vpp) */
    float offset;               /* 4 bytes - DC offset in V (-10V to +10V) */
    float duty_cycle;           /* 4 bytes - For square/pulse wave (1% to 99%) */
    float start_freq;           /* 4 bytes - Sweep start frequency */
    float stop_freq;            /* 4 bytes - Sweep stop frequency */
    float sweep_time;           /* 4 bytes - Sweep duration in seconds (0.1s to 999s) */
    float trigger_level;        /* 4 bytes - External trigger level (-5V to +5V) */
    float phase;                /* 4 bytes - Phase offset in degrees (0-360) */
    float mod_freq;             /* 4 bytes - Modulation frequency */
    float mod_depth;            /* 4 bytes - Modulation depth (%) */
    float burst_period;         /* 4 bytes - Burst repetition period */
    int burst_count;            /* 4 bytes - Number of cycles per burst */
    int sweep_type;             /* 4 bytes - 0=Linear, 1=Log */
    int units_freq;             /* 4 bytes - 0=Hz, 1=kHz, 2=MHz */
    int units_time;             /* 4 bytes - 0=s, 1=ms, 2=us */
    
    /* Bit-packed boolean flags to save memory */
    unsigned int output_enabled:1;     /* Output ON/OFF */
    unsigned int sweep_enabled:1;      /* Frequency sweep mode */
    unsigned int sync_enabled:1;       /* Sync output ON/OFF */
    unsigned int invert_enabled:1;     /* Invert output waveform */
    unsigned int modulation_enabled:1; /* AM/FM modulation */
    unsigned int burst_enabled:1;      /* Burst mode */
    unsigned int lf_termination:1;     /* Use LF instead of CRLF for instruments showing "LF" */
    unsigned int reserved:9;           /* Reserved for future use */
} fg5010_config;

/* Global variables (extern declarations) */
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
void gpib_write(int address, char *command);
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
void store_module_data(int slot, float value);
void clear_module_data(int slot);
void save_data(void);
void load_data(void);

/* From print.c */
void print_report(void);
void print_graph_menu(void);

/* Utility functions */
void delay(unsigned int milliseconds);
void cleanup(void);

#endif /* TM5000_H */
