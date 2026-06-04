/*
 * TM5000 GPIB Control System for Gridcase 1520
 * Version 3.3 - Enhanced Stability
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
#define TM5000_VERSION "3.3"
#define TM5000_VERSION_MAJOR 3
#define TM5000_VERSION_MINOR 3

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

/* Structure definitions */
#pragma pack(1)
typedef struct {
    unsigned char module_type;
    unsigned char slot_number;
    unsigned char enabled;
    unsigned char gpib_address;
    float last_reading;
    char description[20];
    float far *module_data;      /* Per-module data buffer */
    unsigned int module_data_count;  /* Count for this module */
    unsigned int module_data_size;   /* Size allocated */
} tm5000_module;
#pragma pack()

typedef struct {
    tm5000_module modules[10];
    float far *data_buffer;
    unsigned int buffer_size;
    unsigned int data_count;
    int sample_rate;
    int gpib_devices[10];
} measurement_system;

typedef struct {
    int present;
    int x, y;
    int left_button;
    int right_button;
} mouse_state;

typedef struct {
    int running;              /* 0=stopped, 1=running */
    int sample_rate_ms;       /* Delay between samples in milliseconds */
    int selected_rate;        /* Index of selected rate preset */
    char custom_rate[10];     /* Custom rate string */
    int use_custom;           /* 0=use preset, 1=use custom */
    int monitor_mask;         /* Bit mask of which modules to monitor */
    int monitor_all;          /* 0=selective, 1=monitor all enabled modules */
} control_panel_state;

/* FFT Configuration structure */
typedef struct {
    int input_points;      /* 64, 128, 256, 512, 1024 */
    int output_points;     /* 32, 64, 128, 256, 512, 1024 (≤ input) */
    int window_type;       /* 0=Rectangular, 1=Hamming, 2=Hanning, 3=Blackman */
    int zero_pad;          /* Auto-pad to next power of 2 */
    int dc_remove;         /* Remove DC component before FFT */
    int output_format;     /* 0=dB, 1=Linear magnitude, 2=Power */
    int peak_centering;    /* Center FFT output on highest peak */
    float custom_sample_rate; /* Override auto-detection (0.0 = auto) */
} fft_config;

typedef struct {
    float min_value;
    float max_value;
    float scale_factor;
    int auto_scale;
    int grid_divisions;
    float pan_offset;
    float zoom_factor;
    int sample_start;     /* Starting sample index for display */
    int sample_count;     /* Number of samples to display (0 = all) */
} graph_scale;

typedef struct {
    int slot;
    int enabled;
    unsigned char color;
    char label[20];
    float *data;
    int data_count;
    int unit_type;     /* 0=Voltage, 1=Frequency, 2=dB */
    float x_scale;     /* For frequency traces: Hz per sample */
    float x_offset;    /* X-axis offset (for centering) */
} trace_info;

typedef struct {
    char function[20];      /* Current function (DCV, ACV, OHMS, etc.) */
    int range_mode;         /* 0=AUTO, 1-7=Manual range */
    int filter_enabled;     /* Filter ON/OFF */
    int filter_value;       /* Filter value 1-99 */
    int trigger_mode;       /* CONT, ONE, TALK, EXT */
    int digits;            /* Number of digits (3-6) */
    float nullval;         /* Null value for relative measurements */
    int null_enabled;      /* NULL function ON/OFF */
    int data_format;       /* 0=ASCII, 1=Scientific notation */
	int buffer_enabled;    /* Use internal buffering */
	int buffer_size;       /* Number of samples to buffer */
	int min_max_enabled;   /* Track min/max values */
	float min_value;       /* Minimum value tracked */
	float max_value;       /* Maximum value tracked */
	int sample_count;      /* Samples taken since last clear */
	int burst_mode;        /* 0=Normal, 1=Burst sampling */
	float sample_rate;     /* Internal sample rate (if supported) */
	int lf_termination;    /* Use LF instead of CRLF for instruments showing "LF" */
} dm5120_config;

typedef struct {
    char function[20];      /* Current function (DCV, ACV, OHMS, CURR, DIODE) */
    int range_mode;         /* 0=AUTO, 1-7=Manual range */
    int filter_enabled;     /* Digital filter ON/OFF */
    int filter_count;       /* Filter count 1-100 */
    int trigger_mode;       /* 0=FREE, 1=SINGLE, 2=EXT */
    int digits;            /* Display digits (4-5) */
    float nullval;         /* Null/REL value for relative measurements */
    int null_enabled;      /* NULL/REL function ON/OFF */
    int data_format;       /* 0=ASCII, 1=Scientific notation */
    int auto_zero;         /* Auto-zero ON/OFF */
    int calculation_mode;  /* 0=NONE, 1=AVG, 2=SCALE, 3=DBM, 4=DBR */
    float scale_factor;    /* Scaling factor for calculations */
    float scale_offset;    /* Scaling offset for calculations */
    int avg_count;         /* Number of readings for averaging */
    float dbm_reference;   /* dBm reference resistance (default 600) */
    float dbr_reference;   /* dBr reference value */
    int beeper_enabled;    /* Beeper ON/OFF */
    int front_panel_lock;  /* Front panel lockout */
    int high_speed_mode;   /* High speed acquisition mode */
    float min_value;       /* Minimum measured value */
    float max_value;       /* Maximum measured value */
    int statistics_enabled; /* Track min/max statistics */
    int lf_termination;    /* Use LF instead of CRLF for instruments showing "LF" */
} dm5010_config;

typedef struct {
    float voltage;          /* Output voltage setting (0-20V) */
    float current_limit;    /* Current limit (10-305mA) */
    int output_enabled;     /* Output ON/OFF */
    int display_mode;       /* 0=Voltage, 1=Current, 2=Current Limit */
    int vri_enabled;        /* Voltage regulation interrupt */
    int cri_enabled;        /* Current regulation interrupt */
    int uri_enabled;        /* Unregulated interrupt */
    int dt_enabled;         /* Device trigger */
    int user_enabled;       /* INST ID button */
    int rqs_enabled;        /* Service requests */
    int lf_termination;     /* Use LF instead of CRLF for instruments showing "LF" */
} ps5004_config;

typedef struct {
    float voltage1;             /* Output voltage channel 1 (0-40V) */
    float current_limit1;       /* Current limit channel 1 (0-0.5A) */
    int output1_enabled;        /* Channel 1 output ON/OFF */
    
    float voltage2;             /* Output voltage channel 2 (0-40V) */
    float current_limit2;       /* Current limit channel 2 (0-0.5A) */
    int output2_enabled;        /* Channel 2 output ON/OFF */
    
    float logic_voltage;        /* Logic supply voltage (4.5-5.5V typ) */
    float logic_current_limit;  /* Logic current limit (0-2A) */
    int logic_enabled;          /* Logic output ON/OFF */
    
    int tracking_mode;          /* 0=Independent, 1=Series, 2=Parallel */
    float tracking_ratio;       /* Ratio for asymmetric tracking */
    
    int display_channel;        /* 0=Ch1, 1=Ch2, 2=Logic */
    int display_mode;           /* 0=Voltage, 1=Current */
    
    int ovp_enabled;            /* Over voltage protection */
    float ovp_level1;           /* OVP threshold channel 1 */
    float ovp_level2;           /* OVP threshold channel 2 */
    
    int cv_mode1;               /* Channel 1 in constant voltage mode */
    int cc_mode1;               /* Channel 1 in constant current mode */
    int cv_mode2;               /* Channel 2 in constant voltage mode */
    int cc_mode2;               /* Channel 2 in constant current mode */
    
    int srq_enabled;            /* Enable service requests */
    int error_queue_size;       /* Number of errors in queue */
    int lf_termination;         /* Use LF instead of CRLF for instruments showing "LF" */
} ps5010_config;

typedef struct {
    char function[20];          /* FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, etc. */
    char channel[10];           /* A, B, AB, BA, etc. */
    float gate_time;            /* Gate time in seconds */
    int averaging;              /* -1=auto, 1=no avg, >1=avg count */
    char coupling_a[10];        /* AC, DC */
    char coupling_b[10];        /* AC, DC */
    char impedance_a[10];       /* HI, LO */
    char impedance_b[10];       /* HI, LO */
    char attenuation_a[10];     /* X1, X10 */
    char attenuation_b[10];     /* X1, X10 */
    char slope_a[10];           /* POS, NEG */
    char slope_b[10];           /* POS, NEG */
    float level_a;              /* Trigger level Channel A */
    float level_b;              /* Trigger level Channel B */
    int filter_enabled;         /* Input filter ON/OFF */
    int auto_trigger;           /* Auto trigger mode */
    int overflow_enabled;       /* Overflow indication */
    int preset_enabled;         /* Preset function */
    int srq_enabled;            /* Service request */
    long overflow_count;        /* Extended range overflow counter */
    float last_measurement;     /* Last measurement result */
    int measurement_complete;   /* Measurement status flag */
    int lf_termination;         /* Use LF instead of CRLF for instruments showing "LF" */
} dc5009_config;

typedef struct {
    char function[20];          /* FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, etc. */
    char channel[10];           /* A, B, AB, BA, etc. */
    float gate_time;            /* Gate time in seconds */
    int averaging;              /* -1=auto, 1=no avg, >1=avg count */
    char coupling_a[10];        /* AC, DC */
    char coupling_b[10];        /* AC, DC */
    char impedance_a[10];       /* HI, LO */
    char impedance_b[10];       /* HI, LO */
    char attenuation_a[10];     /* X1, X10 */
    char attenuation_b[10];     /* X1, X10 */
    char slope_a[10];           /* POS, NEG */
    char slope_b[10];           /* POS, NEG */
    float level_a;              /* Trigger level Channel A */
    float level_b;              /* Trigger level Channel B */
    int filter_enabled;         /* Input filter ON/OFF */
    int auto_trigger;           /* Auto trigger mode */
    int overflow_enabled;       /* Overflow indication */
    int preset_enabled;         /* Preset function */
    int srq_enabled;            /* Service request */
    long overflow_count;        /* Extended range overflow counter */
    float last_measurement;     /* Last measurement result */
    int measurement_complete;   /* Measurement status flag */
    int rise_fall_enabled;      /* Rise/fall time measurements (DC5010 specific) */
    int burst_mode;             /* Burst measurement mode (DC5010 specific) */
    int lf_termination;         /* Use LF instead of CRLF for instruments showing "LF" */
} dc5010_config;

typedef struct {
    float frequency;            /* Output frequency in Hz (0.1 Hz to 15 MHz) */
    float amplitude;            /* Output amplitude in Vpp (0.01V to 20Vpp) */
    float offset;               /* DC offset in V (-10V to +10V) */
    char waveform[12];          /* SINE, SQUARE, TRIANGLE, RAMP, PULSE, NOISE */
    int output_enabled;         /* Output ON/OFF */
    float duty_cycle;           /* For square/pulse wave (1% to 99%) */
    int sweep_enabled;          /* Frequency sweep mode */
    float start_freq;           /* Sweep start frequency */
    float stop_freq;            /* Sweep stop frequency */
    float sweep_time;           /* Sweep duration in seconds (0.1s to 999s) */
    int sweep_type;             /* 0=Linear, 1=Log */
    char trigger_source[10];    /* INT, EXT, MAN */
    char trigger_slope[4];      /* POS, NEG */
    float trigger_level;        /* External trigger level (-5V to +5V) */
    int sync_enabled;           /* Sync output ON/OFF */
    int invert_enabled;         /* Invert output waveform */
    float phase;                /* Phase offset in degrees (0-360) */
    int modulation_enabled;     /* AM/FM modulation */
    char mod_type[4];           /* AM, FM */
    float mod_freq;             /* Modulation frequency */
    float mod_depth;            /* Modulation depth (%) */
    int burst_enabled;          /* Burst mode */
    int burst_count;            /* Number of cycles per burst */
    float burst_period;         /* Burst repetition period */
    int units_freq;             /* 0=Hz, 1=kHz, 2=MHz */
    int units_time;             /* 0=s, 1=ms, 2=us */
    int lf_termination;         /* Use LF instead of CRLF for instruments showing "LF" */
} fg5010_config;

/* Global variables (extern declarations) */
extern measurement_system *g_system;
extern unsigned char far *video_mem;
extern char g_error_msg[80];
extern dm5120_config g_dm5120_config[10];
extern dm5010_config g_dm5010_config[10];
extern ps5004_config g_ps5004_config[10];
extern ps5010_config g_ps5010_config[10];
extern dc5009_config g_dc5009_config[10];
extern dc5010_config g_dc5010_config[10];
extern fg5010_config g_fg5010_config[10];
extern mouse_state g_mouse;
extern graph_scale g_graph_scale;
extern trace_info g_traces[10];
extern control_panel_state g_control_panel;
extern fft_config g_fft_config;
extern int g_has_287;

/* Sample rate presets */
extern int sample_rate_presets[];
extern char *sample_rate_labels[];
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
