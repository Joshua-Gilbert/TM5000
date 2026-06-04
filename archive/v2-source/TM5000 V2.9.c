/*
 * TM5000 GPIB Control System for Gridcase 1520
 * Version 2.9
 * Compile with: wcl -ml -0 -bt=dos -os -d0 tm5000L.c ieeeio_w.c 
 * C89 compliant
 *
 *
 * Changes in v2.9:
 * - Implemented full Tektronix DM5010 multimeter support
 * - Added DM5010 configuration structure with advanced features
 * - Created comprehensive DM5010 GPIB communication functions
 * - Added DM5010 command functions for all major operations
 * - Implemented enhanced DM5010 measurement with retry logic and statistics
 * - Added DM5010 communication testing and validation functions
 * - Created DM5010 advanced configuration menu
 * - Integrated DM5010 into continuous monitoring and single measurements
 * - Set default GPIB address 15 for DM5010 modules
 * - Added support for: DC/AC voltage, resistance, current, diode test
 * - Implemented: filtering, triggering, auto-zero, calculations (avg/scale/dBm/dBr)
 * - Added statistics tracking and error handling for DM5010
 * - Fixed Brother PostScript print Min/Max legend display preserving correct sign values
 * - Enhanced zoom capability to 1µV minimum scale (was limited to 100µV)
 * - Added ultra-precise 1µV increments for zoom ranges under 10µV total span
 * - Expanded module support from 8 slots (0-7) to 10 slots (0-9)
 * - Updated all arrays, loops, and keyboard handling for 10-module operation
 * - Number keys 0-9 now select traces, ALT+0-9 toggle trace visibility
 * - Optimized continuous monitoring function for reduced overhead:
 * - Reduced display update frequency (every 5th iteration vs every iteration)
 * - Adaptive delay timing: 5ms when sampling, 15ms when idle (was fixed 20ms)
 * - Improved CPU efficiency while maintaining measurement accuracy
 * - Fixed Brother PostScript print function missing max_samples variable declaration
 * - Resolved DOS segment size limit by removing HPGL plotter and PCL functions (640+ lines)
 * - Reduced file size from 370KB to 343KB for DOS compatibility
 * - Fixed graph display only showing slot 0 by moving selected_trace init after sync
 * - Created separate display_trace_selection_menu() for graph vs monitoring contexts
 * - Clarified menu terminology: "Select Traces to Display" vs "Module Selection for Monitoring"
 * - Enhanced Brother PostScript custom header prompt with clearer instructions
 */

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

int ieee_out = -1;  /* Handle for writing to GPIB */
int ieee_in = -1;   /* Handle for reading from GPIB */

#ifdef __WATCOMC__
#define outportb(port, value) outp(port, value)
#define inportb(port) inp(port)
#else
void outportb(unsigned int port, unsigned char value);
unsigned char inportb(unsigned int port);
#endif
void delay(unsigned int milliseconds);

void clrscr(void);
void gotoxy(int x, int y);

void clrscr(void) {
    union REGS regs;
    regs.w.ax = 0x0600;  /* Scroll window up */
    regs.h.bh = 0x07;    /* White on black */
    regs.h.ch = 0;       /* Top row */
    regs.h.cl = 0;       /* Left column */
    regs.h.dh = 24;      /* Bottom row */
    regs.h.dl = 79;      /* Right column */
    int86(0x10, &regs, &regs);
    
    gotoxy(1, 1);
}
void textattr(unsigned char attr) {
    union REGS regs;
    regs.h.ah = 0x09;    /* Write character and attribute */
    regs.h.bl = attr;    /* Attribute */
    regs.w.cx = 0;       /* Don't write any characters */
    regs.h.bh = 0;       /* Video page 0 */
    int86(0x10, &regs, &regs);
}
void clreol(void) {
    union REGS regs;
    int x, y;
    
    regs.h.ah = 0x03;
    regs.h.bh = 0;
    int86(0x10, &regs, &regs);
    x = regs.h.dl;
    y = regs.h.dh;
    
    regs.h.ah = 0x09;
    regs.h.al = ' ';
    regs.h.bh = 0;
    regs.h.bl = 0x07;  /* Normal attribute */
    regs.w.cx = 80 - x;
    int86(0x10, &regs, &regs);
}
void gotoxy(int x, int y) {
    union REGS regs;
    regs.h.ah = 0x02;    /* Set cursor position */
    regs.h.bh = 0;       /* Video page 0 */
    regs.h.dh = y - 1;   /* Row (0-based) */
    regs.h.dl = x - 1;   /* Column (0-based) */
    int86(0x10, &regs, &regs);
}
void draw_text_scaled(int x, int y, char *text, unsigned char color, int scale_x, int scale_y);
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color);
void draw_filled_rect(int x1, int y1, int x2, int y2, unsigned char color);
void draw_gradient_rect(int x1, int y1, int x2, int y2, unsigned char color1, unsigned char color2);
void update_sample_rate(void);
void textattr(unsigned char attr);
void clreol(void);
void file_operations_menu(void);
void measurement_menu(void);
void draw_grid_dynamic(int max_samples);
void math_functions_menu(void);
void perform_fft(void);
void calculate_statistics(void);
void perform_differentiation(void);
void perform_integration(void);
void perform_smoothing(void);
void auto_scale_graph(void);
int value_to_y(float value);
float y_to_value(int y);
void draw_readout(int x, int y, char *text);
void draw_frequency_grid(int fft_samples);
void draw_legend_enhanced(int *is_fft_trace);
void print_graph_text(void);
void print_graph_menu(void);
void print_graph_postscript(void);
void graph_config_menu(void);

void init_dm5120_config_enhanced(int slot);
void dm5120_enable_buffering(int address, int buffer_size);
void dm5120_start_buffer_sequence(int address);
int dm5120_get_buffer_data(int address, float far *buffer, int max_samples);
float read_dm5120_buffered(int address, int slot);
float read_dm5120_enhanced(int address, int slot);
void dm5120_clear_statistics(int slot);

void init_ps5010_config(int slot);
void configure_ps5010_advanced(int slot);
void ps5010_init(int address);
void ps5010_set_voltage(int address, int channel, float voltage);
void ps5010_set_current(int address, int channel, float current);
void ps5010_set_output(int address, int channel, int on);
void ps5010_set_tracking_voltage(int address, float voltage);
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat);
int ps5010_get_settings(int address, char *buffer, int maxlen);
int ps5010_get_error(int address);
void ps5010_set_interrupts(int address, int pri_on, int nri_on, int lri_on);
void ps5010_set_srq(int address, int on);
float read_ps5010(int address, int slot);
void test_ps5010_comm(int address);

void module_selection_menu(void);
void display_trace_selection_menu(void);
void continuous_monitor_enhanced(void);
void sync_traces_with_modules(void);

#define T10s    13         /* 10 seconds timeout */
#define T1s     11         /* 1 second timeout */
#define T300ms  10         /* 300 milliseconds timeout */

#define CGA_MEMORY      0xB8000000L
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   200
#define LPT1_BASE       0x378
#define LPT1_STATUS     (LPT1_BASE + 1)
#define LPT1_CONTROL    (LPT1_BASE + 2)
#define GRAPH_LEFT      35    /* Reduced from 40 */
#define GRAPH_RIGHT     260   /* Reduced from 279 */
#define GRAPH_TOP       16    /* Reduced from 20 */
#define GRAPH_BOTTOM    175   /* Reduced from 180 */
#define GRAPH_WIDTH     (GRAPH_RIGHT - GRAPH_LEFT)
#define GRAPH_HEIGHT    (GRAPH_BOTTOM - GRAPH_TOP)

#define MOD_NONE     0
#define MOD_DC5009   1
#define MOD_DM5010   2
#define MOD_DM5120   3
#define MOD_PS5004   4
#define MOD_PS5010   5
#define MOD_DC5010   6
#define MOD_FG5010   7

#define MOUSE_INT       0x33
#define MOUSE_RESET     0x00
#define MOUSE_SHOW      0x01
#define MOUSE_HIDE      0x02
#define MOUSE_STATUS    0x03
#define MOUSE_SET_POS   0x04

int gpib_error = 0;

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
} ps5010_config;

measurement_system *g_system = NULL;
unsigned char far *video_mem = (unsigned char far *)CGA_MEMORY;
char g_error_msg[80] = "";
dm5120_config g_dm5120_config[10];  /* Global DM5120 configurations */
dm5010_config g_dm5010_config[10];  /* Global DM5010 configurations */
ps5004_config g_ps5004_config[10];  /* Global PS5004 configurations */
ps5010_config g_ps5010_config[10];  /* Global PS5010 configurations - NEW */
mouse_state g_mouse = {0, 0, 0, 0, 0}; /*Global Mouse State*/
graph_scale g_graph_scale = {-10.0, 10.0, 1.0, 1, 10, 0.0, 1.0, 0, 0}; /* Global graph */
trace_info g_traces[10];
control_panel_state g_control_panel = {0, 500, 2, "500", 0, 0xFF, 1};
int g_has_287 = 0;  /* Math coprocessor flag */

int sample_rate_presets[] = {100, 250, 500, 1000, 2000, 5000, 10000};
char *sample_rate_labels[] = {"100ms", "250ms", "500ms", "1s", "2s", "5s", "10s"};
#define NUM_RATE_PRESETS 7


unsigned char enhanced_font[][7] = {
    /* 0x20 space */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x21 ! */     {0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00},
    /* 0x22 " */     {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x23 # */     {0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00},
    /* 0x24 $ */     {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    /* 0x25 % */     {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
    /* 0x26 & */     {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D},
    /* 0x27 ' */     {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x28 ( */     {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    /* 0x29 ) */     {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    /* 0x2A * */     {0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00},
    /* 0x2B + */     {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    /* 0x2C , */     {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08},
    /* 0x2D - */     {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    /* 0x2E . */     {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
    /* 0x2F / */     {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
    /* 0x30 0 */     {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    /* 0x31 1 */     {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 0x32 2 */     {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    /* 0x33 3 */     {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    /* 0x34 4 */     {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    /* 0x35 5 */     {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    /* 0x36 6 */     {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    /* 0x37 7 */     {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04},
    /* 0x38 8 */     {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    /* 0x39 9 */     {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    /* 0x3A : */     {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
    /* 0x3B ; */     {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08},
    /* 0x3C < */     {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},
    /* 0x3D = */     {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
    /* 0x3E > */     {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
    /* 0x3F ? */     {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},
    /* 0x40 @ */     {0x0E, 0x11, 0x01, 0x0D, 0x15, 0x15, 0x0E},
    /* 0x41 A */     {0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11},
    /* 0x42 B */     {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    /* 0x43 C */     {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    /* 0x44 D */     {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    /* 0x45 E */     {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    /* 0x46 F */     {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    /* 0x47 G */     {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    /* 0x48 H */     {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 0x49 I */     {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 0x4A J */     {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    /* 0x4B K */     {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    /* 0x4C L */     {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    /* 0x4D M */     {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    /* 0x4E N */     {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},
    /* 0x4F O */     {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x50 P */     {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    /* 0x51 Q */     {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    /* 0x52 R */     {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    /* 0x53 S */     {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
    /* 0x54 T */     {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    /* 0x55 U */     {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x56 V */     {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    /* 0x57 W */     {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    /* 0x58 X */     {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    /* 0x59 Y */     {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04},
    /* 0x5A Z */     {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    /* 0x5B [ */     {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
    /* 0x5C \ */     {0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00},
    /* 0x5D ] */     {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
    /* 0x5E ^ */     {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00},
    /* 0x5F _ */     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
    /* 0x60 ` */     {0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x61 a */     {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F},
    /* 0x62 b */     {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1E},
    /* 0x63 c */     {0x00, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x0E},
    /* 0x64 d */     {0x01, 0x01, 0x0D, 0x13, 0x11, 0x11, 0x0F},
    /* 0x65 e */     {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E},
    /* 0x66 f */     {0x06, 0x09, 0x08, 0x1C, 0x08, 0x08, 0x08},
    /* 0x67 g */     {0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E},
    /* 0x68 h */     {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11},
    /* 0x69 i */     {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E},
    /* 0x6A j */     {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C},
    /* 0x6B k */     {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12},
    /* 0x6C l */     {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 0x6D m */     {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15},
    /* 0x6E n */     {0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11},
    /* 0x6F o */     {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E},
    /* 0x70 p */     {0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10},
    /* 0x71 q */     {0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x01},
    /* 0x72 r */     {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10},
    /* 0x73 s */     {0x00, 0x00, 0x0E, 0x10, 0x0E, 0x01, 0x1E},
    /* 0x74 t */     {0x08, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x06},
    /* 0x75 u */     {0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0F},
    /* 0x76 v */     {0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04},
    /* 0x77 w */     {0x00, 0x00, 0x11, 0x15, 0x15, 0x15, 0x0A},
    /* 0x78 x */     {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11},
    /* 0x79 y */     {0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E},
    /* 0x7A z */     {0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F},
    /* 0x7B { */     {0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02},
    /* 0x7C | */     {0x04, 0x04, 0x04, 0x00, 0x04, 0x04, 0x04},
    /* 0x7D } */     {0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08},
    /* 0x7E ~ */     {0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00},
    /* 0x7F DEL */   {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

unsigned char scientific_font[][7] = {
    /* 0x00 µ (mu) */ {0x00, 0x00, 0x11, 0x11, 0x11, 0x1E, 0x10},
    /* 0x01 Ω (omega) */ {0x0E, 0x11, 0x11, 0x11, 0x0A, 0x1B, 0x00},
    /* 0x02 π (pi) */ {0x00, 0x00, 0x1F, 0x0A, 0x0A, 0x0A, 0x0A},
    /* 0x03 Σ (sigma) */ {0x1F, 0x10, 0x08, 0x04, 0x08, 0x10, 0x1F},
    /* 0x04 Δ (delta) */ {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x1F},
    /* 0x05 √ (sqrt) */ {0x07, 0x04, 0x04, 0x14, 0x08, 0x00, 0x00},
    /* 0x06 ∞ (infinity) */ {0x00, 0x00, 0x0A, 0x15, 0x15, 0x0A, 0x00},
    /* 0x07 ± (plus/minus) */ {0x04, 0x04, 0x1F, 0x04, 0x04, 0x1F, 0x00},
    /* 0x08 ≈ (approx) */ {0x00, 0x08, 0x15, 0x02, 0x08, 0x15, 0x02},
    /* 0x09 ° (degree) */ {0x04, 0x0A, 0x04, 0x00, 0x00, 0x00, 0x00},
    /* 0x0A ² (squared) */ {0x0C, 0x02, 0x04, 0x08, 0x0E, 0x00, 0x00},
    /* 0x0B ³ (cubed) */ {0x0E, 0x02, 0x04, 0x02, 0x0E, 0x00, 0x00},
    /* 0x0C × (multiply) */ {0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00},
    /* 0x0D ÷ (divide) */ {0x00, 0x04, 0x00, 0x1F, 0x00, 0x04, 0x00},
    /* 0x0E ≤ (less/equal) */ {0x04, 0x08, 0x10, 0x08, 0x04, 0x1F, 0x00},
    /* 0x0F ≥ (greater/equal) */ {0x10, 0x08, 0x04, 0x08, 0x10, 0x1F, 0x00}
};

int init_gpib_system(void);
void init_graphics(void);
void text_mode(void);
void cleanup(void);
void module_functions_menu(void);
void main_menu(void);
void configure_modules(void);
void single_measurement(void);
void continuous_monitor(void);
void graph_display(void);
void print_report(void);
void save_data(void);
void load_data(void);
void display_error(char *msg);
void plot_pixel(int x, int y, unsigned char color);
void draw_line(int x1, int y1, int x2, int y2, unsigned char color);
void draw_waveform_far(float far *data, int count, unsigned char color);
void lpt_send_byte(unsigned char data);
void print_string(char *str);
void test_dm5120_comm(int address);
void send_custom_command(void);
void configure_dm5120_advanced(int slot);
void configure_dm5010_advanced(int slot);
void init_dm5120_config(int slot);
void init_dm5010_config(int slot);
void gpib_write_dm5010(int address, char *command);
int gpib_read_dm5010(int address, char *buffer, int maxlen);
int gpib_read_float_dm5010(int address, float *value);
void dm5010_set_function(int address, char *function);
void dm5010_set_range(int address, char *function, float range);
void dm5010_set_filter(int address, int enabled, int count);
void dm5010_set_trigger(int address, char *mode);
void dm5010_set_autozero(int address, int enabled);
void dm5010_set_null(int address, int enabled, float value);
void dm5010_set_calculation(int address, int mode, float factor, float offset);
void dm5010_beeper(int address, int enabled);
void dm5010_lock_front_panel(int address, int locked);
void test_dm5010_comm(int address);
float read_dm5010_enhanced(int address, int slot);
float read_dm5120_voltage(int address);
void test_dm5120_comm_debug(int address);
void gpib_terminal_mode(void);
void drain_input_buffer(void);
int command_has_response(const char *cmd);
int allocate_module_buffer(int slot, unsigned int size);
void free_module_buffer(int slot);
void store_module_data(int slot, float value);
void clear_module_data(int slot);


void init_ps5004_config(int slot);
void configure_ps5004_advanced(int slot);
void ps5004_set_voltage(int address, float voltage);
void ps5004_set_current(int address, float current);
void ps5004_set_output(int address, int on);
void ps5004_set_display(int address, char *mode);
int ps5004_get_regulation_status(int address);
float ps5004_read_value(int address);
void ps5004_init(int address);
void test_ps5004_comm(int address);


extern void rawmode(int handle);

int get_font_index(char c) {
    if (c >= 0x20 && c <= 0x7F) {
        return c - 0x20;
    }
    
    switch(c) {
        case 'µ': return -1;   /* Index 0 in scientific_font */
        case 'Ω': return -2;   /* Index 1 in scientific_font */
        case 'π': return -3;   /* Index 2 in scientific_font */
        case 'Σ': return -4;   /* Index 3 in scientific_font */
        case 'Δ': return -5;   /* Index 4 in scientific_font */
        case '√': return -6;   /* Index 5 in scientific_font */
        case '∞': return -7;   /* Index 6 in scientific_font */
        case '±': return -8;   /* Index 7 in scientific_font */
        case '≈': return -9;   /* Index 8 in scientific_font */
        case '°': return -10;  /* Index 9 in scientific_font */
        case '²': return -11;  /* Index 10 in scientific_font */
        case '³': return -12;  /* Index 11 in scientific_font */
        case '×': return -13;  /* Index 12 in scientific_font */
        case '÷': return -14;  /* Index 13 in scientific_font */
        case '≤': return -15;  /* Index 14 in scientific_font */
        case '≥': return -16;  /* Index 15 in scientific_font */
        default: return 0;     /* space for unknown */
    }
}

int init_mouse(void) {
    union REGS regs;
    
    regs.w.ax = MOUSE_RESET;
    int86(MOUSE_INT, &regs, &regs);
    
    if (regs.w.ax == 0xFFFF) {
        g_mouse.present = 1;
        return 1;  /* Mouse present */
    }
    
    g_mouse.present = 0;
    return 0;  /* No mouse */
}
void show_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SHOW;
    int86(MOUSE_INT, &regs, &regs);
}

void hide_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_HIDE;
    int86(MOUSE_INT, &regs, &regs);
}

void get_mouse_status(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_STATUS;
    int86(MOUSE_INT, &regs, &regs);
    
    g_mouse.x = regs.w.cx / 8;  /* Convert to text coordinates */
    g_mouse.y = regs.w.dx / 8;
    g_mouse.left_button = regs.w.bx & 1;
    g_mouse.right_button = (regs.w.bx & 2) >> 1;
}

void set_mouse_pos(int x, int y) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SET_POS;
    regs.w.cx = x * 8;  /* Convert from text to pixel coordinates */
    regs.w.dx = y * 8;
    int86(MOUSE_INT, &regs, &regs);
}

int mouse_in_region(int x1, int y1, int x2, int y2) {
    get_mouse_status();
    return (g_mouse.x >= x1 && g_mouse.x <= x2 && 
            g_mouse.y >= y1 && g_mouse.y <= y2 &&
            g_mouse.left_button);
}

int wait_for_input(void) {
    int old_left = 0;
    
    show_mouse();
    
    while (1) {
        if (kbhit()) {
            hide_mouse();
            return getch();
        }
        
        if (g_mouse.present) {
            get_mouse_status();
            
            if (old_left && !g_mouse.left_button) {
                hide_mouse();
                return -1;  /* Mouse click indicator */
            }
            
            old_left = g_mouse.left_button;
        }
        
        delay(10);
    }
}


int allocate_module_buffer(int slot, unsigned int size) {
    if (slot < 0 || slot >= 8) return 0;
    
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
    }
    
    g_system->modules[slot].module_data = (float far *)_fmalloc(size * sizeof(float));
    if (g_system->modules[slot].module_data) {
        g_system->modules[slot].module_data_size = size;
        g_system->modules[slot].module_data_count = 0;
        return 1;
    }
    
    return 0;
}

void free_module_buffer(int slot) {
    if (slot < 0 || slot >= 8) return;
    
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
        g_system->modules[slot].module_data = NULL;
        g_system->modules[slot].module_data_size = 0;
        g_system->modules[slot].module_data_count = 0;
    }
}

void store_module_data(int slot, float value) {
    if (slot < 0 || slot >= 8) return;
    if (!g_system->modules[slot].module_data) return;
    
    if (g_system->modules[slot].module_data_count < g_system->modules[slot].module_data_size) {
        g_system->modules[slot].module_data[g_system->modules[slot].module_data_count++] = value;
    }
}

void clear_module_data(int slot) {
    if (slot < 0 || slot >= 8) return;
    g_system->modules[slot].module_data_count = 0;
}

int ieee_write(const char *str) {
    if (ieee_out < 0) return -1;
    return write(ieee_out, str, strlen(str));
}

int ieee_read(char *buffer, int maxlen) {
    int bytes_read;
    
    if (ieee_in < 0) return -1;
    
    bytes_read = read(ieee_in, buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else if (bytes_read < 0) {
        if (errno == 5) {  /* General failure - often means no data */
            buffer[0] = '\0';
            return 0;  /* Return 0 to indicate no data, not error */
        }
        return bytes_read;
    }
    
    return bytes_read;
}

int gpib_check_srq(int address) {
    char cmd[80];
    char response[10];
    int status;
    
    sprintf(cmd, "spoll %d\r\n", address);
    ieee_write(cmd);
    delay(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        status = atoi(response);
        return status;
    }
    return 0;
}

void gpib_write(int address, char *command) {
    char cmd_buffer[256];
    
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 10 && 
            g_system->modules[slot].module_type == MOD_DM5120) {
            gpib_check_srq(address);
            delay(50);
        }
    }
    
    sprintf(cmd_buffer, "output %d;%s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 10 && 
            g_system->modules[slot].module_type == MOD_DM5120) {
            delay(100);
        } else {
            delay(50);
        }
    } else {
        delay(50);
    }
}

int gpib_read(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(50);
    
    bytes_read = ieee_read(buffer, maxlen);
    return bytes_read;
}

int gpib_read_float(int address, float *value) {
    char buffer[256];
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%*[^+-]%f", value) == 1) {
            return 1;
        }
        if (sscanf(buffer, "%f", value) == 1) {
            return 1;
        }
        if (sscanf(buffer, "%e", value) == 1) {
            return 1;
        }
    }
    
    return 0;
}

void gpib_remote(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "remote %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);  /* Increased delay for DM5120 */
}

void gpib_local(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "local %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(50);
}

void gpib_clear(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "clear %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);
}

void gpib_write_dm5120(int address, char *command) {
    char cmd_buffer[256];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    gpib_check_srq(address);
    delay(50);
    
    sprintf(cmd_buffer, "output %d; %s%s", address, command, termination);
    ieee_write(cmd_buffer);
    
    delay(200);
}

int gpib_read_dm5120(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "enter %d%s", address, termination);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    bytes_read = ieee_read(buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

int gpib_read_float_dm5120(int address, float *value) {
    char buffer[256];
    char cmd_buffer[80];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "enter %d%s", address, termination);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "NDCV%e", value) == 1) return 1;
        if (sscanf(buffer, "DCV%e", value) == 1) return 1;
        if (sscanf(buffer, "NACV%e", value) == 1) return 1;
        if (sscanf(buffer, "ACV%e", value) == 1) return 1;
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
    }
    
    return 0;
}

void gpib_remote_dm5120(int address) {
    char cmd_buffer[80];
    sprintf(cmd_buffer, "remote %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(200);  /* Extended delay for DM5120 */
}

void gpib_local_dm5120(int address) {
    char cmd_buffer[80];
    sprintf(cmd_buffer, "local %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);
}

void gpib_clear_dm5120(int address) {
    char cmd_buffer[80];
    sprintf(cmd_buffer, "abort\r\n");
    ieee_write(cmd_buffer);
    delay(100);
    
    sprintf(cmd_buffer, "clear %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(200);
}

int command_has_response(const char *cmd) {
    if (strncasecmp(cmd, "hello", 5) == 0) return 1;
    if (strncasecmp(cmd, "status", 6) == 0) return 1;
    if (strncasecmp(cmd, "enter", 5) == 0) return 1;
    if (strncasecmp(cmd, "spoll", 5) == 0) return 1;
    
    if (strncasecmp(cmd, "fill", 4) == 0) return 1;
    
    if (strncasecmp(cmd, "output", 6) == 0) return 0;
    if (strncasecmp(cmd, "remote", 6) == 0) return 0;
    if (strncasecmp(cmd, "local", 5) == 0) return 0;
    if (strncasecmp(cmd, "clear", 5) == 0) return 0;
    if (strncasecmp(cmd, "reset", 5) == 0) return 0;  /* Usually no response */
    
    if (strncasecmp(cmd, "break", 5) == 0) return 0;
    
    return 0;
}

void drain_input_buffer(void) {
    char buffer[256];
    int bytes_read;
    int total_drained = 0;
    
    while ((bytes_read = read(ieee_in, buffer, sizeof(buffer))) > 0) {
        total_drained += bytes_read;
        delay(10);
    }
    
    if (total_drained > 0) {
        printf("Drained %d bytes from input buffer\n", total_drained);
    }
}

void check_gpib_error(void) {
    char status[256];
    int bytes_read;
    
    memset(status, 0, sizeof(status));
    
    ieee_write("status\r\n");
    delay(50);
    
    bytes_read = ieee_read(status, sizeof(status));
    
    if (bytes_read > 0) {
        printf("GPIB Status: %s\n", status);
        
        if (strstr(status, "ERROR") || strstr(status, "Error")) {
            strcpy(g_error_msg, status);
            gpib_error = 1;
            
            if (strstr(status, "SEQUENCE")) {
                printf("Attempting to clear SEQUENCE error...\n");
                
                ieee_write("abort\r\n");
                delay(100);
                
                ieee_write("status\r\n");
                delay(50);
                bytes_read = ieee_read(status, sizeof(status));
                if (bytes_read > 0) {
                    printf("Status after abort: %s\n", status);
                    if (!strstr(status, "ERROR")) {
                        gpib_error = 0;  /* Error cleared */
                    }
                }
            }
        } else {
            gpib_error = 0;
        }
    } else {
        printf("Warning: No status response received\n");
        gpib_error = 1;
        strcpy(g_error_msg, "No status response");
    }
}

void init_dm5120_config(int slot) {
    strcpy(g_dm5120_config[slot].function, "DCV");
    g_dm5120_config[slot].range_mode = 0;  /* AUTO */
    g_dm5120_config[slot].filter_enabled = 0;
    g_dm5120_config[slot].filter_value = 10;
    g_dm5120_config[slot].trigger_mode = 0;  /* CONT */
    g_dm5120_config[slot].digits = 5;
    g_dm5120_config[slot].nullval = 0.0;
    g_dm5120_config[slot].null_enabled = 0;
    g_dm5120_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5120_config[slot].lf_termination = 0;  /* Default to CRLF */
}

void init_dm5120_config_enhanced(int slot) {
    init_dm5120_config(slot);  /* Call original init first */
    
    g_dm5120_config[slot].buffer_enabled = 0;
    g_dm5120_config[slot].buffer_size = 100;  /* Conservative default */
    g_dm5120_config[slot].min_max_enabled = 0;
    g_dm5120_config[slot].min_value = 1e30;
    g_dm5120_config[slot].max_value = -1e30;
    g_dm5120_config[slot].sample_count = 0;
    g_dm5120_config[slot].burst_mode = 0;
    g_dm5120_config[slot].sample_rate = 10.0;  /* 10 samples/sec default */
}

void init_dm5010_config(int slot) {
    strcpy(g_dm5010_config[slot].function, "DCV");
    g_dm5010_config[slot].range_mode = 0;  /* AUTO */
    g_dm5010_config[slot].filter_enabled = 0;
    g_dm5010_config[slot].filter_count = 1;
    g_dm5010_config[slot].trigger_mode = 0;  /* FREE */
    g_dm5010_config[slot].digits = 5;  /* 4.5 digits */
    g_dm5010_config[slot].nullval = 0.0;
    g_dm5010_config[slot].null_enabled = 0;
    g_dm5010_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5010_config[slot].auto_zero = 1;  /* ON */
    g_dm5010_config[slot].calculation_mode = 0;  /* NONE */
    g_dm5010_config[slot].scale_factor = 1.0;
    g_dm5010_config[slot].scale_offset = 0.0;
    g_dm5010_config[slot].avg_count = 10;
    g_dm5010_config[slot].dbm_reference = 600.0;  /* 600 ohm */
    g_dm5010_config[slot].dbr_reference = 0.0;
    g_dm5010_config[slot].beeper_enabled = 1;
    g_dm5010_config[slot].front_panel_lock = 0;
    g_dm5010_config[slot].high_speed_mode = 0;
    g_dm5010_config[slot].min_value = 1e30;
    g_dm5010_config[slot].max_value = -1e30;
    g_dm5010_config[slot].statistics_enabled = 0;
}

void dm5120_enable_buffering(int address, int buffer_size) {
    char cmd[50];
    
    gpib_write_dm5120(address, "CLEAR");
    delay(100);
    
    sprintf(cmd, "SAMPLES %d", buffer_size);
    gpib_write_dm5120(address, cmd);
    delay(50);
    
    gpib_write_dm5120(address, "TRIGGER INT");
    delay(50);
    
    gpib_write_dm5120(address, "DATFOR ON");  /* Scientific notation */
    delay(50);
}

void dm5120_start_buffer_sequence(int address) {
    gpib_write_dm5120(address, "INIT:CONT ON");
    delay(100);
    
    gpib_write_dm5120(address, "INIT");
    delay(50);
}

int dm5120_get_buffer_data(int address, float far *buffer, int max_samples) {
    char response[1024];
    char *token;
    int count = 0;
    float value;
    
    gpib_write_dm5120(address, "FETC?");
    delay(200);  /* Allow time for data formatting */
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        token = strtok(response, ",");
        while (token != NULL && count < max_samples) {
            if (sscanf(token, "%e", &value) == 1) {
                buffer[count++] = value;
            }
            token = strtok(NULL, ",");
        }
    }
    
    return count;
}

float read_dm5120_buffered(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float value = 0.0;
    float buffer[10];
    int samples;
    
    if (cfg->buffer_enabled && cfg->burst_mode) {
        dm5120_enable_buffering(address, 10);
        dm5120_start_buffer_sequence(address);
        delay(100);  /* Wait for samples */
        
        samples = dm5120_get_buffer_data(address, buffer, 10);
        
        if (samples > 0) {
            int i;
            value = 0.0;
            for (i = 0; i < samples; i++) {
                value += buffer[i];
                
                if (cfg->min_max_enabled) {
                    if (buffer[i] < cfg->min_value) cfg->min_value = buffer[i];
                    if (buffer[i] > cfg->max_value) cfg->max_value = buffer[i];
                }
            }
            value /= samples;
            cfg->sample_count += samples;
        }
    } else {
        value = read_dm5120_enhanced(address, slot);
        
        if (cfg->min_max_enabled) {
            if (value < cfg->min_value) cfg->min_value = value;
            if (value > cfg->max_value) cfg->max_value = value;
            cfg->sample_count++;
        }
    }
    
    return value;
}

void dm5120_clear_statistics(int slot) {
    g_dm5120_config[slot].min_value = 1e30;
    g_dm5120_config[slot].max_value = -1e30;
    g_dm5120_config[slot].sample_count = 0;
}

void init_ps5004_config(int slot) {
    g_ps5004_config[slot].voltage = 0.0;
    g_ps5004_config[slot].current_limit = 0.1;  /* 100mA default */
    g_ps5004_config[slot].output_enabled = 0;   /* OFF */
    g_ps5004_config[slot].display_mode = 0;     /* Voltage */
    g_ps5004_config[slot].vri_enabled = 0;      /* OFF */
    g_ps5004_config[slot].cri_enabled = 0;      /* OFF */
    g_ps5004_config[slot].uri_enabled = 0;      /* OFF */
    g_ps5004_config[slot].dt_enabled = 0;       /* OFF */
    g_ps5004_config[slot].user_enabled = 0;     /* OFF */
    g_ps5004_config[slot].rqs_enabled = 1;      /* ON */
}

void init_ps5010_config(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    
    cfg->voltage1 = 0.0;
    cfg->current_limit1 = 0.1;  /* 100mA default */
    cfg->output1_enabled = 0;
    
    cfg->voltage2 = 0.0;
    cfg->current_limit2 = 0.1;  /* 100mA default */
    cfg->output2_enabled = 0;
    
    cfg->logic_voltage = 5.0;
    cfg->logic_current_limit = 1.0;  /* 1A default */
    cfg->logic_enabled = 0;
    
    cfg->tracking_mode = 0;  /* Independent */
    cfg->tracking_ratio = 1.0;  /* 1:1 tracking */
    
    cfg->display_channel = 0;  /* Show channel 1 */
    cfg->display_mode = 0;     /* Show voltage */
    
    cfg->ovp_enabled = 0;
    cfg->ovp_level1 = 42.0;  /* Just above max */
    cfg->ovp_level2 = 42.0;
    
    cfg->cv_mode1 = 1;
    cfg->cc_mode1 = 0;
    cfg->cv_mode2 = 1;
    cfg->cc_mode2 = 0;
    
    cfg->srq_enabled = 1;
    cfg->error_queue_size = 0;
}

void ps5004_init(int address) {
    gpib_write(address, "INIT");
    delay(100);
}

void ps5004_set_voltage(int address, float voltage) {
    char cmd[50];
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 20.0) voltage = 20.0;
    sprintf(cmd, "VOLTAGE %.4f", voltage);
    gpib_write(address, cmd);
    delay(50);
}

void ps5004_set_current(int address, float current) {
    char cmd[50];
    if (current < 0.01) current = 0.01;    /* 10mA minimum */
    if (current > 0.305) current = 0.305;  /* 305mA maximum */
    sprintf(cmd, "CURRENT %.3f", current);
    gpib_write(address, cmd);
    delay(50);
}

void ps5004_set_output(int address, int on) {
    if (on) {
        gpib_write(address, "OUTPUT ON");
    } else {
        gpib_write(address, "OUTPUT OFF");
    }
    delay(50);
}

void ps5004_set_display(int address, char *mode) {
    char cmd[50];
    sprintf(cmd, "DISPLAY %s", mode);
    gpib_write(address, cmd);
    delay(50);
}

int ps5004_get_regulation_status(int address) {
    char buffer[80];
    int status = 0;
    
    gpib_write(address, "REGULATION?");
    delay(50);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (strstr(buffer, "REGULATION 1")) status = 1;
        else if (strstr(buffer, "REGULATION 2")) status = 2;
        else if (strstr(buffer, "REGULATION 3")) status = 3;
        else status = atoi(buffer);
    }
    
    return status;
}

float ps5004_read_value(int address) {
    char buffer[256];
    float value = 0.0;
    
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}

void test_ps5004_comm(int address) {
    char buffer[256];
    float value;
    
    printf("\nTesting PS5004 at address %d...\n", address);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote(address);
    delay(200);
    
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("ID Response: %s\n", buffer);
    } else {
        printf("No ID response\n");
    }
    
    printf("\nInitializing PS5004...\n");
    ps5004_init(address);
    
    printf("\nQuerying all settings...\n");
    gpib_write(address, "SET?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Settings: %s\n", buffer);
    }
    
    printf("\nSetting voltage to 5.0V...\n");
    ps5004_set_voltage(address, 5.0);
    
    gpib_write(address, "VOLTAGE?");
    delay(50);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Voltage setting: %s\n", buffer);
    }
    
    printf("\nSetting current limit to 100mA...\n");
    ps5004_set_current(address, 0.1);
    
    gpib_write(address, "CURRENT?");
    delay(50);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Current limit: %s\n", buffer);
    }
    
    printf("\nTesting display modes...\n");
    ps5004_set_display(address, "VOLTAGE");
    delay(100);
    ps5004_set_display(address, "CURRENT");
    delay(100);
    ps5004_set_display(address, "CLIMIT");
    delay(100);
    ps5004_set_display(address, "VOLTAGE");
    
    printf("\nReading voltage (output is OFF)...\n");
    value = ps5004_read_value(address);
    printf("Voltage reading: %.4f V\n", value);
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

void configure_ps5004_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    ps5004_config *cfg = &g_ps5004_config[slot];
    char buffer[80];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("PS5004 Power Supply Configuration - Slot %d\n", slot);
        printf("=====================================\n\n");
        
        printf("1. Voltage: %.4f V\n", cfg->voltage);
        printf("2. Current Limit: %.3f A (%.0f mA)\n", 
               cfg->current_limit, cfg->current_limit * 1000);
        printf("3. Output: %s\n", cfg->output_enabled ? "ON" : "OFF");
        printf("4. Display: ");
        switch(cfg->display_mode) {
            case 0: printf("VOLTAGE\n"); break;
            case 1: printf("CURRENT\n"); break;
            case 2: printf("CURRENT LIMIT\n"); break;
        }
        printf("5. Voltage Regulation Interrupt (VRI): %s\n", 
               cfg->vri_enabled ? "ON" : "OFF");
        printf("6. Current Regulation Interrupt (CRI): %s\n", 
               cfg->cri_enabled ? "ON" : "OFF");
        printf("7. Unregulated Interrupt (URI): %s\n", 
               cfg->uri_enabled ? "ON" : "OFF");
        printf("8. Device Trigger (DT): %s\n", 
               cfg->dt_enabled ? "ON" : "OFF");
        printf("9. User Button (INST ID): %s\n", 
               cfg->user_enabled ? "ON" : "OFF");
        printf("A. Service Requests (RQS): %s\n", 
               cfg->rqs_enabled ? "ON" : "OFF");
        printf("B. Query Current Status\n");
        printf("C. Apply Settings to PS5004\n");
        printf("D. Test Measurement\n");
        printf("E. Run Communication Test\n");
        printf("F. Initialize PS5004 (INIT command)\n");
        printf("0. Return to Module Config\n");
        
        printf("\nSelect option: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':  /* Voltage */
                printf("\n\nEnter voltage (0-20V): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 20.0) {
                    cfg->voltage = temp_float;
                }
                break;
                
            case '2':  /* Current limit */
                printf("\n\nEnter current limit in mA (10-305): ");
                scanf("%f", &temp_float);
                if (temp_float >= 10.0 && temp_float <= 305.0) {
                    cfg->current_limit = temp_float / 1000.0;  /* Convert to A */
                }
                break;
                
            case '3':  /* Output */
                cfg->output_enabled = !cfg->output_enabled;
                break;
                
            case '4':  /* Display mode */
                printf("\n\nDisplay modes:\n");
                printf("0. VOLTAGE\n");
                printf("1. CURRENT\n");
                printf("2. CURRENT LIMIT\n");
                printf("Select: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->display_mode = temp_int;
                }
                break;
                
            case '5':  /* VRI */
                cfg->vri_enabled = !cfg->vri_enabled;
                break;
                
            case '6':  /* CRI */
                cfg->cri_enabled = !cfg->cri_enabled;
                break;
                
            case '7':  /* URI */
                cfg->uri_enabled = !cfg->uri_enabled;
                break;
                
            case '8':  /* DT */
                cfg->dt_enabled = !cfg->dt_enabled;
                break;
                
            case '9':  /* User button */
                cfg->user_enabled = !cfg->user_enabled;
                break;
                
            case 'A':  /* RQS */
                cfg->rqs_enabled = !cfg->rqs_enabled;
                break;
                
            case 'B':  /* Query status */
                printf("\n\nQuerying PS5004 status...\n");
                
                gpib_write(address, "SET?");
                delay(100);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("All settings:\n%s\n", buffer);
                }
                
                gpib_write(address, "REGULATION?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("\nRegulation status: %s", buffer);
                    temp_int = ps5004_get_regulation_status(address);
                    switch(temp_int) {
                        case 1: printf(" (Voltage regulation)\n"); break;
                        case 2: printf(" (Current regulation)\n"); break;
                        case 3: printf(" (Unregulated)\n"); break;
                    }
                }
                
                gpib_write(address, "ERROR?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Error status: %s\n", buffer);
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'C':  /* Apply settings */
                printf("\n\nApplying settings to PS5004...\n");
                
                gpib_remote(address);
                delay(200);
                
                ps5004_set_voltage(address, cfg->voltage);
                ps5004_set_current(address, cfg->current_limit);
                
                switch(cfg->display_mode) {
                    case 0: ps5004_set_display(address, "VOLTAGE"); break;
                    case 1: ps5004_set_display(address, "CURRENT"); break;
                    case 2: ps5004_set_display(address, "CLIMIT"); break;
                }
                
                gpib_write(address, cfg->vri_enabled ? "VRI ON" : "VRI OFF");
                delay(50);
                gpib_write(address, cfg->cri_enabled ? "CRI ON" : "CRI OFF");
                delay(50);
                gpib_write(address, cfg->uri_enabled ? "URI ON" : "URI OFF");
                delay(50);
                
                gpib_write(address, cfg->dt_enabled ? "DT ON" : "DT OFF");
                delay(50);
                gpib_write(address, cfg->user_enabled ? "USER ON" : "USER OFF");
                delay(50);
                gpib_write(address, cfg->rqs_enabled ? "RQS ON" : "RQS OFF");
                delay(50);
                
                ps5004_set_output(address, cfg->output_enabled);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'D':  /* Test measurement */
                printf("\n\nTesting measurement...\n");
                
                ps5004_set_display(address, "VOLTAGE");
                delay(100);
                
                printf("Voltage reading: ");
                temp_float = ps5004_read_value(address);
                printf("%.4f V\n", temp_float);
                
                ps5004_set_display(address, "CURRENT");
                delay(100);
                
                printf("Current reading: ");
                temp_float = ps5004_read_value(address);
                printf("%.3f A (%.1f mA)\n", temp_float, temp_float * 1000);
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'E':  /* Run communication test */
                test_ps5004_comm(address);
                break;
                
            case 'F':  /* Initialize */
                printf("\n\nInitializing PS5004...\n");
                ps5004_init(address);
                init_ps5004_config(slot);
                printf("PS5004 initialized to power-on state.\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

void ps5010_init(int address) {
    gpib_write(address, "INIT");
    delay(500);  /* PS5010 needs time for reset */
}

void ps5010_set_voltage(int address, int channel, float voltage) {
    char cmd[50];
    
    switch(channel) {
        case 1:  /* Positive floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 40.0) voltage = 40.0;
            sprintf(cmd, "VPOS %.1f", voltage);
            break;
        case 2:  /* Negative floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 40.0) voltage = 40.0;
            sprintf(cmd, "VNEG %.1f", voltage);
            break;
        case 3:  /* Logic supply */
            if (voltage < 4.5) voltage = 4.5;
            if (voltage > 5.5) voltage = 5.5;
            sprintf(cmd, "VLOG %.1f", voltage);
            break;
        default:
            return;  /* Invalid channel */
    }
    
    gpib_write(address, cmd);
    delay(50);
}

void ps5010_set_current(int address, int channel, float current) {
    char cmd[50];
    
    switch(channel) {
        case 1:  /* Positive supply */
            if (current < 0.0) current = 0.0;
            if (current > 0.5) current = 0.5;
            sprintf(cmd, "IPOS %.3f", current);
            break;
        case 2:  /* Negative supply */
            if (current < 0.0) current = 0.0;
            if (current > 0.5) current = 0.5;
            sprintf(cmd, "INEG %.3f", current);
            break;
        case 3:  /* Logic supply */
            if (current < 0.0) current = 0.0;
            if (current > 2.0) current = 2.0;
            sprintf(cmd, "ILOG %.3f", current);
            break;
        default:
            return;
    }
    
    gpib_write(address, cmd);
    delay(50);
}

void ps5010_set_output(int address, int channel, int on) {
    char cmd[50];
    
    switch(channel) {
        case 0:  /* All outputs */
            sprintf(cmd, "OUT %s", on ? "ON" : "OFF");
            break;
        case 1:  /* Floating supplies only */
            sprintf(cmd, "FSOUT %s", on ? "ON" : "OFF");
            break;
        case 2:  /* Logic supply only */
            sprintf(cmd, "LSOUT %s", on ? "ON" : "OFF");
            break;
        default:
            return;
    }
    
    gpib_write(address, cmd);
    delay(50);
}

void ps5010_set_tracking_voltage(int address, float voltage) {
    char cmd[50];
    
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 40.0) voltage = 40.0;
    
    sprintf(cmd, "VTRA %.1f", voltage);
    gpib_write(address, cmd);
    delay(50);
}
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat) {
    char buffer[256];
    
    gpib_write(address, "REG?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        /* REG returns <neg>,<pos>,<logic> where:
           1 = voltage regulation
           2 = current regulation
           3 = unregulated */
        if (sscanf(buffer, "%d,%d,%d", neg_stat, pos_stat, log_stat) == 3) {
            return 1;  /* Success */
        }
    }
    
    return 0;  /* Failed */
}
int ps5010_get_settings(int address, char *buffer, int maxlen) {
    gpib_write(address, "SET?");
    delay(100);
    return gpib_read(address, buffer, maxlen);
}

int ps5010_get_error(int address) {
    char buffer[256];
    int error_code = 0;
    
    gpib_write(address, "ERR?");
    delay(50);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        sscanf(buffer, "%d", &error_code);
    }
    
    return error_code;
}

void ps5010_set_interrupts(int address, int pri_on, int nri_on, int lri_on) {
    gpib_write(address, pri_on ? "PRI ON" : "PRI OFF");
    delay(50);
    gpib_write(address, nri_on ? "NRI ON" : "NRI OFF");
    delay(50);
    gpib_write(address, lri_on ? "LRI ON" : "LRI OFF");
    delay(50);
}

void ps5010_set_srq(int address, int on) {
    gpib_write(address, on ? "RQS ON" : "RQS OFF");
    delay(50);
}

//* Read PS5010 value - no direct measurement commands */
float read_ps5010(int address, int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    int neg_stat, pos_stat, log_stat;
    
    
    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat)) {
        cfg->cv_mode1 = (pos_stat == 1);
        cfg->cc_mode1 = (pos_stat == 2);
        cfg->cv_mode2 = (neg_stat == 1);
        cfg->cc_mode2 = (neg_stat == 2);
        
        return 0.0;
    }
    
    return 0.0;
}

void test_ps5010_comm(int address) {
    char buffer[256];
    int neg_stat, pos_stat, log_stat;
    
    printf("\nTesting PS5010 at address %d...\n", address);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote(address);
    delay(200);
    
    printf("\nResetting PS5010...\n");
    ps5010_init(address);
    
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("ID Response: %s\n", buffer);
    } else {
        printf("No ID response\n");
    }
    
    printf("\nSetting test voltages...\n");
    ps5010_set_voltage(address, 1, 5.0);   /* VPOS 5.0 */
    ps5010_set_voltage(address, 2, 12.0);  /* VNEG 12.0 */
    ps5010_set_current(address, 1, 0.1);   /* IPOS 0.1 */
    ps5010_set_current(address, 2, 0.1);   /* INEG 0.1 */
    ps5010_set_voltage(address, 3, 5.0);   /* VLOG 5.0 */
    ps5010_set_current(address, 3, 1.0);   /* ILOG 1.0 */
    
    printf("\nQuerying settings...\n");
    if (ps5010_get_settings(address, buffer, sizeof(buffer)) > 0) {
        printf("Settings: %s\n", buffer);
    }
    
    printf("\nReading regulation status...\n");
    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat)) {
        printf("Negative supply: %s\n", 
               neg_stat == 1 ? "Voltage regulation (CV)" : 
               (neg_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
        printf("Positive supply: %s\n",
               pos_stat == 1 ? "Voltage regulation (CV)" : 
               (pos_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
        printf("Logic supply: %s\n",
               log_stat == 1 ? "Voltage regulation (CV)" : 
               (log_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
    }
    
    printf("\nTesting VTRA (tracking voltage) command...\n");
    ps5010_set_tracking_voltage(address, 10.0);
    delay(200);
    printf("Set both POS and NEG to 10V using VTRA\n");
    
    printf("\nChecking error status...\n");
    gpib_write(address, "ERR?");
    delay(50);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        int error_code = atoi(buffer);
        if (error_code == 0) {
            printf("No errors\n");
        } else {
            printf("Error code: %d\n", error_code);
        }
    }
    
    printf("\nTesting output control...\n");
    ps5010_set_output(address, 0, 0);  /* All outputs OFF */
    printf("All outputs OFF\n");
    delay(500);
    
    ps5010_set_output(address, 1, 1);  /* Floating supplies ON */
    printf("Floating supplies ON\n");
    delay(500);
    
    ps5010_set_output(address, 2, 1);  /* Logic supply ON */
    printf("Logic supply ON\n");
    
    printf("\nFinal settings check...\n");
    if (ps5010_get_settings(address, buffer, sizeof(buffer)) > 0) {
        printf("Settings: %s\n", buffer);
    }
    
    printf("\nNote: PS5010 cannot measure output voltage/current.\n");
    printf("It can only report regulation status (CV/CC/UR).\n");
    printf("Use an external DMM to verify actual output values.\n");
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

void configure_ps5010_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    ps5010_config *cfg = &g_ps5010_config[slot];
    char buffer[80];
    float temp_float;
    int temp_int, channel;
    
    while (!done) {
        clrscr();
        printf("PS5010 Dual Power Supply Configuration - Slot %d\n", slot);
        printf("============================================\n\n");
        
        printf("Channel 1:\n");
        printf("1. Voltage: %.3f V    Current Limit: %.3f A    Output: %s\n",
               cfg->voltage1, cfg->current_limit1, 
               cfg->output1_enabled ? "ON" : "OFF");
        
        printf("\nChannel 2:\n");
        printf("2. Voltage: %.3f V    Current Limit: %.3f A    Output: %s\n",
               cfg->voltage2, cfg->current_limit2,
               cfg->output2_enabled ? "ON" : "OFF");
        
        printf("\nLogic Supply:\n");
        printf("3. Voltage: %.3f V    Current Limit: %.3f A    Output: %s\n",
               cfg->logic_voltage, cfg->logic_current_limit,
               cfg->logic_enabled ? "ON" : "OFF");
        
        printf("\nTracking Mode:\n");
        printf("4. Mode: ");
        switch(cfg->tracking_mode) {
            case 0: printf("Independent\n"); break;
            case 1: printf("Series (outputs track together)\n"); break;
            case 2: printf("Parallel (current sharing)\n"); break;
        }
        
        printf("\nDisplay:\n");
        printf("5. Channel: %s    Mode: %s\n",
               cfg->display_channel == 0 ? "CH1" : 
               (cfg->display_channel == 1 ? "CH2" : "Logic"),
               cfg->display_mode == 0 ? "Voltage" : "Current");
        
        printf("\nActions:\n");
        printf("6. Apply All Settings\n");
        printf("7. Read Regulation Status\n");  /* Changed from "Read All Channels" */
        printf("8. Test VTRA Command\n");       /* Changed from "Test Tracking Mode" */
        printf("9. Run Communication Test\n");
        printf("0. Return to Module Config\n");
        
        printf("\nSelect option: ");
        choice = getch();
        
        switch(choice) {
            case '1':  /* Channel 1 settings */
                printf("\n\nChannel 1 Configuration:\n");
                printf("V)oltage, C)urrent limit, O)utput toggle: ");
                choice = toupper(getch());
                
                switch(choice) {
                    case 'V':
                        printf("\nEnter voltage (0-40V): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 40.0) {
                            cfg->voltage1 = temp_float;
                        }
                        break;
                    case 'C':
                        printf("\nEnter current limit in mA (0-500): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 500.0) {
                            cfg->current_limit1 = temp_float / 1000.0;
                        }
                        break;
                    case 'O':
                        cfg->output1_enabled = !cfg->output1_enabled;
                        break;
                }
                break;
                
            case '2':  /* Channel 2 settings */
                printf("\n\nChannel 2 Configuration:\n");
                printf("V)oltage, C)urrent limit, O)utput toggle: ");
                choice = toupper(getch());
                
                switch(choice) {
                    case 'V':
                        printf("\nEnter voltage (0-40V): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 40.0) {
                            cfg->voltage2 = temp_float;
                        }
                        break;
                    case 'C':
                        printf("\nEnter current limit in mA (0-500): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 500.0) {
                            cfg->current_limit2 = temp_float / 1000.0;
                        }
                        break;
                    case 'O':
                        cfg->output2_enabled = !cfg->output2_enabled;
                        break;
                }
                break;
                
            case '3':  /* Logic supply settings */
                printf("\n\nLogic Supply Configuration:\n");
                printf("V)oltage, C)urrent limit, O)utput toggle: ");
                choice = toupper(getch());
                
                switch(choice) {
                    case 'V':
                        printf("\nEnter voltage (4.5-5.5V): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 4.5 && temp_float <= 5.5) {
                            cfg->logic_voltage = temp_float;
                        }
                        break;
                    case 'C':
                        printf("\nEnter current limit in A (0-2): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 2.0) {
                            cfg->logic_current_limit = temp_float;
                        }
                        break;
                    case 'O':
                        cfg->logic_enabled = !cfg->logic_enabled;
                        break;
                }
                break;
                
            case '4':  /* Tracking mode */
                printf("\n\nTracking modes:\n");
                printf("0. Independent (no tracking)\n");
                printf("1. Series (voltages track together)\n");
                printf("2. Parallel (current sharing)\n");
                printf("Select: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->tracking_mode = temp_int;
                }
                break;
                
            case '5':  /* Display settings */
                printf("\n\nDisplay channel (0=CH1, 1=CH2, 2=Logic): ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->display_channel = temp_int;
                }
                
                printf("Display mode (0=Voltage, 1=Current): ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 1) {
                    cfg->display_mode = temp_int;
                }
                break;
                
            case '6':  /* Apply all settings */
                printf("\n\nApplying settings to PS5010...\n");
                
                gpib_remote(address);
                delay(200);
                
                ps5010_set_voltage(address, 1, cfg->voltage1);
                ps5010_set_voltage(address, 2, cfg->voltage2);
                ps5010_set_voltage(address, 3, cfg->logic_voltage);
                
                ps5010_set_current(address, 1, cfg->current_limit1);
                ps5010_set_current(address, 2, cfg->current_limit2);
                ps5010_set_current(address, 3, cfg->logic_current_limit);
                
                
                ps5010_set_output(address, 1, cfg->output1_enabled);
                ps5010_set_output(address, 2, cfg->output2_enabled);
                ps5010_set_output(address, 3, cfg->logic_enabled);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '7':  /* Read all channels */
                printf("\n\nReading regulation status...\n");
                
                {
                    int neg_stat, pos_stat, log_stat;
                    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat)) {
                        printf("\nChannel 1 (POS): ");
                        switch(pos_stat) {
                            case 1: printf("Voltage regulation mode\n"); break;
                            case 2: printf("Current regulation mode\n"); break;
                            case 3: printf("Unregulated\n"); break;
                            default: printf("Unknown\n");
                        }
                        
                        printf("Channel 2 (NEG): ");
                        switch(neg_stat) {
                            case 1: printf("Voltage regulation mode\n"); break;
                            case 2: printf("Current regulation mode\n"); break;
                            case 3: printf("Unregulated\n"); break;
                            default: printf("Unknown\n");
                        }
                        
                        printf("Logic supply: ");
                        switch(log_stat) {
                            case 1: printf("Voltage regulation mode\n"); break;
                            case 2: printf("Current regulation mode\n"); break;
                            case 3: printf("Unregulated\n"); break;
                            default: printf("Unknown\n");
                        }
                        
                        printf("\nNote: PS5010 cannot measure actual voltage/current values.\n");
                        printf("Only regulation status is available via GPIB.\n");
                    } else {
                        printf("Failed to read regulation status\n");
                    }
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case '8':  /* Test tracking mode */
                printf("\n\nTesting tracking voltage command...\n");
                printf("Note: Hardware tracking mode is set by front panel switch.\n");
                printf("VTRA command sets both POS and NEG to same voltage.\n\n");
                
                printf("Setting both supplies to 10V using VTRA...\n");
                ps5010_set_tracking_voltage(address, 10.0);
                delay(200);
                
                printf("Command sent. Check front panel displays.\n");
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case '9':  /* Communication test */
                test_ps5010_comm(address);
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

void dm5120_set_function(int address, char *function) {
    char cmd[50];
    sprintf(cmd, "FUNCT %s", function);
    gpib_write_dm5120(address, cmd);
    delay(100);
}

void dm5120_set_range(int address, int range) {
    char cmd[50];
    if (range == 0) {
        gpib_write_dm5120(address, "RANGE AUTO");
    } else {
        sprintf(cmd, "RANGE %d", range);
        gpib_write_dm5120(address, cmd);
    }
    delay(50);
}

void dm5120_set_filter(int address, int enabled, int value) {
    char cmd[50];
    
    if (enabled) {
        gpib_write_dm5120(address, "FILTER ON");
        delay(50);
        if (value > 0 && value <= 99) {
            sprintf(cmd, "FILTERVAL %d", value);
            gpib_write_dm5120(address, cmd);
            delay(50);
        }
    } else {
        gpib_write_dm5120(address, "FILTER OFF");
    }
}

void dm5120_set_trigger(int address, char *mode) {
    char cmd[50];
    sprintf(cmd, "TRIGGER %s", mode);
    gpib_write_dm5120(address, cmd);
    delay(50);
}

void dm5120_set_digits(int address, int digits) {
    char cmd[50];
    if (digits >= 3 && digits <= 6) {
        sprintf(cmd, "DIGIT %d", digits);
        gpib_write_dm5120(address, cmd);
        delay(50);
    }
}

void dm5120_set_null(int address, int enabled, float value) {
    char cmd[50];
    
    if (enabled) {
        gpib_write_dm5120(address, "NULL ON");
        delay(50);
        sprintf(cmd, "NULLVAL ACQUIRE");
        gpib_write_dm5120(address, cmd);
        delay(50);
    } else {
        gpib_write_dm5120(address, "NULL OFF");
    }
}

void dm5120_set_data_format(int address, int on) {
    if (on) {
        gpib_write_dm5120(address, "DATFOR ON");
    } else {
        gpib_write_dm5120(address, "DATFOR OFF");
    }
    delay(50);
}

void test_dm5120_comm(int address) {
    char buffer[256];
    char status[256];
    float value;
    int i, srq_status;
    
    printf("\nTesting DM5120 at address %d...\n", address);
    
    ieee_write("status\r\n");
    delay(50);
    ieee_read(status, sizeof(status));
    printf("Driver status: %s\n", status);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote_dm5120(address);
    delay(200);
    
    printf("Checking SRQ status...\n");
    srq_status = gpib_check_srq(address);
    printf("SRQ status: %d\n", srq_status);
    
    printf("Sending ABORT command...\n");
    ieee_write("abort\r\n");
    delay(200);
    
    printf("\nSending INIT command...\n");
    gpib_write_dm5120(address, "INIT");
    delay(300);
    
    printf("Setting DCV function...\n");
    gpib_write_dm5120(address, "FUNCT DCV");
    delay(100);
    
    printf("Setting AUTO range...\n");
    gpib_write_dm5120(address, "RANGE AUTO");
    delay(100);
    
    printf("Setting data format ON (scientific)...\n");
    gpib_write_dm5120(address, "DATFOR ON");
    delay(100);
    
    printf("\nAttempting measurement reads:\n");
    
    printf("\n1. Using voltage? command:\n");
    gpib_write_dm5120(address, "voltage?");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Raw response: '%s'\n", buffer);
        if (sscanf(buffer, "%f", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n2. Simple execute (X command):\n");
    gpib_write_dm5120(address, "X");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Raw response: '%s'\n", buffer);
        if (sscanf(buffer, "%e", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n3. READ? query:\n");
    gpib_write_dm5120(address, "read?");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Raw response: '%s'\n", buffer);
    } else {
        printf("   No response\n");
    }
    
    printf("\nQuerying current settings:\n");
    
    gpib_write_dm5120(address, "FUNCT?");
    delay(50);
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Function: %s\n", buffer);
    }
    
    gpib_write_dm5120(address, "RANGE?");
    delay(50);
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Range: %s\n", buffer);
    }
    
    gpib_write_dm5120(address, "ERROR?");
    delay(50);
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Error status: %s\n", buffer);
    }
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

float read_dm5120_enhanced(int address, int slot) {
    float value = 0.0;
    int retry;
    dm5120_config *cfg = &g_dm5120_config[slot];
    char buffer[256];
    int srq_status;
    
    srq_status = gpib_check_srq(address);
    if (srq_status & 0x40) {  /* Device requesting service */
        delay(100);
    }
    
    gpib_write_dm5120(address, "voltage?");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        }
        if (sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
    }
    
    if (cfg->trigger_mode == 0) {  /* CONT mode */
        gpib_write_dm5120(address, "X");
        delay(300);
    } else {
        gpib_write_dm5120(address, "SEND");
        delay(400);
    }
    
    if (gpib_read_float_dm5120(address, &value)) {
        return value;
    }
    
    for (retry = 0; retry < 3; retry++) {
        char buffer[256];
        
        gpib_write_dm5120(address, "X");
        delay(300);
        
        if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
            if (sscanf(buffer, "NDCV%e", &value) == 1) return value;
            if (sscanf(buffer, "DCV%e", &value) == 1) return value;
            if (sscanf(buffer, "NACV%e", &value) == 1) return value;
            if (sscanf(buffer, "ACV%e", &value) == 1) return value;
            if (sscanf(buffer, "%e", &value) == 1) return value;
            if (sscanf(buffer, "%f", &value) == 1) return value;
        }
        
        delay(100);
    }
    
    printf("\nDM5120: Failed to read after 3 attempts\n");
    return 0.0;
}

float read_dm5120(int address) {
    int i;
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && 
            g_system->modules[i].gpib_address == address &&
            g_system->modules[i].module_type == MOD_DM5120) {
            return read_dm5120_enhanced(address, i);
        }
    }
    return read_dm5120_enhanced(address, 0);
}

float read_dm5120_voltage(int address) {
    char buffer[256];
    float value = 0.0;
    
    gpib_check_srq(address);
    delay(50);
    
    gpib_write_dm5120(address, "voltage?");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        }
        if (sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}

void configure_dm5120_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dm5120_config *cfg = &g_dm5120_config[slot];
    char buffer[80];
    int temp_int;
    float temp_float;
    
    while (!done) {
        clrscr();
        printf("DM5120 Advanced Configuration - Slot %d\n", slot);
        printf("================================\n\n");
        
        printf("1. Function: %s\n", cfg->function);
        printf("2. Range: %s\n", cfg->range_mode == 0 ? "AUTO" : "Manual");
        printf("3. Filter: %s (Value: %d)\n", 
               cfg->filter_enabled ? "ON" : "OFF", cfg->filter_value);
        printf("4. Trigger: ");
        switch(cfg->trigger_mode) {
            case 0: printf("CONT\n"); break;
            case 1: printf("ONE\n"); break;
            case 2: printf("TALK\n"); break;
            case 3: printf("EXT\n"); break;
        }
        printf("5. Digits: %d\n", cfg->digits);
        printf("6. NULL: %s (Value: %.6f)\n", 
               cfg->null_enabled ? "ON" : "OFF", cfg->nullval);
        printf("7. Data Format: %s\n", cfg->data_format ? "Scientific" : "NR3");
        
        printf("8. Buffering: %s (Size: %d)\n", 
               cfg->buffer_enabled ? "ON" : "OFF", cfg->buffer_size);
        printf("9. Min/Max Tracking: %s", cfg->min_max_enabled ? "ON" : "OFF");
        if (cfg->min_max_enabled && cfg->sample_count > 0) {
            printf(" (Min: %.6f, Max: %.6f, Samples: %d)", 
                   cfg->min_value, cfg->max_value, cfg->sample_count);
        }
        printf("\n");
        printf("A. Burst Mode: %s (Rate: %.1f samples/sec)\n", 
               cfg->burst_mode ? "ON" : "OFF", cfg->sample_rate);
        
        printf("\nB. Query Current Status\n");
        printf("C. Apply Settings to DM5120\n");
        printf("D. Test Measurement\n");
        printf("E. Run Communication Test\n");
        printf("F. Clear Min/Max Statistics\n");
        printf("0. Return to Module Config\n");
        
        printf("\nSelect option: ");
        choice = toupper(getch());
        
        switch(choice) {
            
            case '8':  /* Buffering */
                cfg->buffer_enabled = !cfg->buffer_enabled;
                if (cfg->buffer_enabled) {
                    printf("\n\nEnter buffer size (10-1000): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 10 && temp_int <= 1000) {
                        cfg->buffer_size = temp_int;
                    }
                }
                break;
                
            case '9':  /* Min/Max tracking */
                cfg->min_max_enabled = !cfg->min_max_enabled;
                if (cfg->min_max_enabled) {
                    dm5120_clear_statistics(slot);
                    printf("\n\nMin/Max tracking enabled. Statistics cleared.\n");
                    printf("Press any key to continue...");
                    getch();
                }
                break;
                
            case 'A':  /* Burst mode */
                cfg->burst_mode = !cfg->burst_mode;
                if (cfg->burst_mode) {
                    printf("\n\nEnter internal sample rate (1-100 samples/sec): ");
                    scanf("%f", &temp_float);
                    if (temp_float >= 1.0 && temp_float <= 100.0) {
                        cfg->sample_rate = temp_float;
                    }
                }
                break;
                
                
            case 'F':  /* Clear statistics */
                dm5120_clear_statistics(slot);
                printf("\n\nMin/Max statistics cleared.\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

void init_graphics(void) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x04;
    int86(0x10, &regs, &regs);
    
    outportb(0x3D9, 0x30);
}

void text_mode(void) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
}

void plot_pixel(int x, int y, unsigned char color) {
    unsigned int offset;
    unsigned char shift, mask;
    
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
        return;
    
    offset = (y >> 1) * 80 + (x >> 2);
    if (y & 1) offset += 8192;
    
    shift = 6 - ((x & 3) << 1);
    mask = ~(3 << shift);
    
    video_mem[offset] = (video_mem[offset] & mask) | ((color & 3) << shift);
}
void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int e2;
    
    while (1) {
        plot_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int e2;
    int intensity;
    
    while (1) {
        plot_pixel(x1, y1, color);
        
        if (dx > dy) {
            if (y1 > 0) plot_pixel(x1, y1-1, (color > 1) ? color-1 : 1);
            if (y1 < SCREEN_HEIGHT-1) plot_pixel(x1, y1+1, (color > 1) ? color-1 : 1);
        } else {
            if (x1 > 0) plot_pixel(x1-1, y1, (color > 1) ? color-1 : 1);
            if (x1 < SCREEN_WIDTH-1) plot_pixel(x1+1, y1, (color > 1) ? color-1 : 1);
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}
void draw_gradient_rect(int x1, int y1, int x2, int y2, unsigned char color1, unsigned char color2) {
    int x, y;
    int height = y2 - y1;
    
    for (y = y1; y <= y2; y++) {
        unsigned char color = ((y - y1) < height/2) ? color1 : color2;
        for (x = x1; x <= x2; x++) {
            plot_pixel(x, y, color);
        }
    }
}
void draw_waveform_far(float far *data, int count, unsigned char color) {
    graph_display();
}

void lpt_send_byte(unsigned char data) {
    int timeout = 1000;
    
    while (!(inportb(LPT1_STATUS) & 0x80) && timeout--);
    
    if (timeout > 0) {
        outportb(LPT1_BASE, data);
        outportb(LPT1_CONTROL, 0x0D);
        outportb(LPT1_CONTROL, 0x0C);
    }
}

void print_string(char *str) {
    while (*str) {
        lpt_send_byte(*str++);
    }
}

void configure_modules(void) {
    int slot, type, addr, i;
    int addr_conflict;
    char buffer[80];
    char status[256];
    
    clrscr();
    printf("Module Configuration\n");
    printf("===================\n\n");
    
    for (slot = 0; slot < 10; slot++) {
        printf("Slot %d - ", slot);
        
        if (g_system->modules[slot].enabled) {
            switch(g_system->modules[slot].module_type) {
                case MOD_DC5009: printf("DC5009 Counter"); break;
                case MOD_DM5010: printf("DM5010 DMM"); break;
                case MOD_DM5120: printf("DM5120 DMM"); break;
                case MOD_PS5004: printf("PS5004 Power Supply"); break;
                case MOD_PS5010: printf("PS5010 Power Supply"); break;
                case MOD_DC5010: printf("DC5010 Counter"); break;
                case MOD_FG5010: printf("FG5010 Function Gen"); break;
            }
            printf(" (GPIB: %d)", g_system->modules[slot].gpib_address);
        } else {
            printf("Empty");
        }
        printf("\n");
    }
    
    printf("\nEnter slot to configure (0-7, or 9 to exit): ");
    scanf("%d", &slot);
    
    if (slot >= 0 && slot < 10) {
        printf("\nModule types:\n");
        printf("0 = Empty\n");
        printf("1 = DC5009 Counter/Timer\n");
        printf("2 = DM5010 DMM\n");
        printf("3 = DM5120 DMM\n");
        printf("4 = PS5004 Power Supply\n");
        printf("5 = PS5010 Power Supply\n");
        printf("6 = DC5010 Counter\n");
        printf("7 = FG5010 Function Generator\n");
        printf("\nSelect type: ");
        scanf("%d", &type);
        
        if (type >= 0 && type <= 9) {
            g_system->modules[slot].module_type = type;
            g_system->modules[slot].slot_number = slot;
            
            if (type != MOD_NONE) {
                int default_addr;
                switch(type) {
                    case MOD_DM5010:
                        default_addr = 15;
                        break;
                    case MOD_DM5120:
                        default_addr = 16;
                        break;
                    case MOD_PS5010:
                        default_addr = 22;
                        break;
                    case MOD_PS5004:
                        default_addr = 13;
                        break;
                    default:
                        default_addr = 10 + slot;
                        break;
                }
                
                do {
                    printf("\nEnter GPIB address (1-30, default=%d): ", default_addr);
                    fflush(stdin);
                    fgets(buffer, sizeof(buffer), stdin);
                    
                    if (sscanf(buffer, "%d", &addr) == 1) {
                        if (addr >= 1 && addr <= 30) {
                            addr_conflict = 0;
                            for (i = 0; i < 10; i++) {
                                if (i != slot && g_system->modules[i].enabled &&
                                    g_system->modules[i].gpib_address == addr) {
                                    printf("Error: Address %d already used by slot %d!\n", addr, i);
                                    addr_conflict = 1;
                                    break;
                                }
                            }
                            
                            if (!addr_conflict) {
                                g_system->modules[slot].gpib_address = addr;
                                break;
                            }
                        } else {
                            printf("Invalid address! Must be between 1 and 30.\n");
                        }
                    } else {
                        switch(type) {
                            case MOD_DM5010:
                                addr = 15;
                                break;
                            case MOD_DM5120:
                                addr = 16;
                                break;
                            case MOD_PS5010:
                                addr = 22;
                                break;
                            case MOD_PS5004:
                                addr = 13;
                                break;
                            default:
                                addr = 10 + slot;  /* Default formula for other modules */
                                break;
                        }
                        
                        addr_conflict = 0;
                        for (i = 0; i < 10; i++) {
                            if (i != slot && g_system->modules[i].enabled &&
                                g_system->modules[i].gpib_address == addr) {
                                printf("Default address %d is in use. Please enter a different address.\n", addr);
                                addr_conflict = 1;
                                break;
                            }
                        }
                        
                        if (!addr_conflict) {
                            g_system->modules[slot].gpib_address = addr;
                            break;
                        }
                    }
                } while (1);
                
                g_system->modules[slot].enabled = 1;
                
                switch(type) {
                    case MOD_DC5009: 
                        strcpy(g_system->modules[slot].description, "DC5009 Counter"); 
                        break;
                    case MOD_DM5010: 
                        strcpy(g_system->modules[slot].description, "DM5010 DMM"); 
                        break;
                    case MOD_DM5120: 
                        strcpy(g_system->modules[slot].description, "DM5120 DMM"); 
                        break;
                    case MOD_PS5004: 
                        strcpy(g_system->modules[slot].description, "PS5004 Power"); 
                        break;
                    case MOD_PS5010: 
                        strcpy(g_system->modules[slot].description, "PS5010 Power"); 
                        break;
                    case MOD_DC5010: 
                        strcpy(g_system->modules[slot].description, "DC5010 Counter"); 
                        break;
                    case MOD_FG5010: 
                        strcpy(g_system->modules[slot].description, "FG5010 Func Gen"); 
                        break;
                }
                
                printf("Initializing device at GPIB address %d...\n", 
                       g_system->modules[slot].gpib_address);
                
                gpib_remote(g_system->modules[slot].gpib_address);
                delay(200);
                
                if (type == MOD_DM5120) {
                    delay(300);
                    
                    printf("\nUse Line Feed termination? (Y/N): ");
                    fflush(stdout);
                    {
                        char input_char = getch();
                        printf("%c\n", input_char);
                        if (input_char == 'Y' || input_char == 'y') {
                            g_dm5120_config[slot].lf_termination = 1;
                            printf("Line Feed termination enabled.\n");
                        } else {
                            g_dm5120_config[slot].lf_termination = 0;
                            printf("Standard CRLF termination enabled.\n");
                        }
                    }
                    
                    init_dm5120_config(slot);
                    test_dm5120_comm(g_system->modules[slot].gpib_address);
                } else if (type == MOD_DM5010) {
                    delay(300);
                    init_dm5010_config(slot);
                    printf("\nPress any key to test DM5010 communication...");
                    getch();
                    test_dm5010_comm(g_system->modules[slot].gpib_address);
                } else if (type == MOD_PS5004) {
                    delay(300);
                    init_ps5004_config(slot);
                    printf("\nPress any key to test PS5004 communication...");
                    getch();
                    test_ps5004_comm(g_system->modules[slot].gpib_address);
                } else {
                    gpib_clear(g_system->modules[slot].gpib_address);
                }
                
                check_gpib_error();
                
                if (gpib_error) {
                    printf("Warning: Could not verify device initialization\n");
                    gpib_error = 0;
                }
                
                if (!allocate_module_buffer(slot, 1000)) {  /* 1000 points per module */
                    printf("Warning: Could not allocate data buffer for module\n");
                }
                
                printf("Module configured successfully\n");
            } else {
                g_system->modules[slot].enabled = 0;
                g_system->modules[slot].gpib_address = 0;
            }
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
}

float read_dc5009(int address) {
    char buffer[50];
    float value;
    
    gpib_write(address, "FUNC FREQ");
    gpib_write(address, "CHAN A");
    gpib_write(address, "INIT");
    delay(100);
    gpib_write(address, "FETCH?");
    
    if (gpib_read_float(address, &value) == 1) {
        return value;
    }
    return 0.0;
}

float read_dm5010(int address) {
    char buffer[50];
    float value;
    
    gpib_write(address, "DCV");
    gpib_write(address, "AUTO");
    delay(50);
    
    if (gpib_read_float(address, &value) == 1) {
        return value;
    }
    return 0.0;
}

float read_dm5010_enhanced(int address, int slot) {
    char buffer[256];
    float value = 0.0;
    int attempts = 0;
    int success = 0;
    
    gpib_check_srq(address);
    delay(50);
    
    while (attempts < 3 && !success) {
        attempts++;
        
        gpib_write_dm5010(address, "VAL?");
        delay(100);
        
        if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
            if (sscanf(buffer, "%e", &value) == 1) {
                success = 1;
                break;
            }
        }
        
        if (!success) {
            gpib_write_dm5010(address, "READ?");
            delay(150);
            
            if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
                if (sscanf(buffer, "%e", &value) == 1) {
                    success = 1;
                    break;
                }
            }
        }
        
        if (!success && g_dm5010_config[slot].trigger_mode == 1) {
            gpib_write_dm5010(address, "INIT");
            delay(100);
            gpib_write_dm5010(address, "FETC?");
            delay(100);
            
            if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
                if (sscanf(buffer, "%e", &value) == 1) {
                    success = 1;
                    break;
                }
            }
        }
        
        delay(100);  /* Wait before retry */
    }
    
    if (success && g_dm5010_config[slot].statistics_enabled) {
        if (value < g_dm5010_config[slot].min_value) {
            g_dm5010_config[slot].min_value = value;
        }
        if (value > g_dm5010_config[slot].max_value) {
            g_dm5010_config[slot].max_value = value;
        }
    }
    
    return success ? value : 0.0;
}

void set_ps5004_voltage(int address, float voltage) {
    ps5004_set_voltage(address, voltage);
}

void set_ps5010_output(int address, int channel, float voltage, float current) {
    char cmd[50];
    sprintf(cmd, "INST:SEL OUT%d", channel);
    gpib_write(address, cmd);
    sprintf(cmd, "VOLT %.3f", voltage);
    gpib_write(address, cmd);
    sprintf(cmd, "CURR %.3f", current);
    gpib_write(address, cmd);
}

void set_fg5010_frequency(int address, float frequency) {
    char cmd[50];
    sprintf(cmd, "FREQ %.6f", frequency);
    gpib_write(address, cmd);
}

void set_fg5010_amplitude(int address, float amplitude) {
    char cmd[50];
    sprintf(cmd, "AMPL %.3f", amplitude);
    gpib_write(address, cmd);
}
void single_measurement(void) {
    int i;
    float value;
    ps5004_config *ps_cfg;
    
    clrscr();
    printf("Single Measurement\n");
    printf("==================\n\n");
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            printf("Slot %d [GPIB:%d] (%s): ", 
                   i, g_system->modules[i].gpib_address, 
                   g_system->modules[i].description);
            
            switch(g_system->modules[i].module_type) {
                case MOD_DC5009:
                case MOD_DC5010:
                    value = read_dc5009(g_system->modules[i].gpib_address);
                    printf("%.6f MHz\n", value / 1e6);
                    break;
                    
                case MOD_DM5010:
                    value = read_dm5010_enhanced(g_system->modules[i].gpib_address, i);
                    printf("%.4f V\n", value);
                    break;
                    
                case MOD_DM5120:
                    printf("\n  Reading DM5120 (voltage?)...");
                    fflush(stdout);
                    value = read_dm5120_voltage(g_system->modules[i].gpib_address);
                    if (value == 0.0) {
                        printf("\n  Trying alternate method...");
                        value = read_dm5120_enhanced(g_system->modules[i].gpib_address, i);
                    }
                    printf("\n  Result: %.6f V\n", value);
                    break;
                
				case MOD_PS5010:
					{
						ps5010_config *ps_cfg = &g_ps5010_config[i];
						int neg_stat, pos_stat, log_stat;
						printf("\n");
						printf("  Settings: POS %.1fV/%.0fmA, NEG %.1fV/%.0fmA\n",
						       ps_cfg->voltage1, ps_cfg->current_limit1 * 1000,
						       ps_cfg->voltage2, ps_cfg->current_limit2 * 1000);
						printf("  Logic: %.1fV/%.1fA\n",
						       ps_cfg->logic_voltage, ps_cfg->logic_current_limit);
        
						if (ps5010_read_regulation(g_system->modules[i].gpib_address, 
                                  &neg_stat, &pos_stat, &log_stat)) {
							printf("  Status: POS=%s, NEG=%s, LOG=%s\n",
									pos_stat == 1 ? "CV" : (pos_stat == 2 ? "CC" : "UR"),
									neg_stat == 1 ? "CV" : (neg_stat == 2 ? "CC" : "UR"),
									log_stat == 1 ? "CV" : (log_stat == 2 ? "CC" : "UR"));
						}
						value = 0.0;  /* No measurement capability */
					}
					break;
                
				case MOD_PS5004:
                    ps_cfg = &g_ps5004_config[i];
                    printf("\n");
                    printf("  Settings: %.4fV, %.0fmA limit, Output %s\n",
                           ps_cfg->voltage, ps_cfg->current_limit * 1000,
                           ps_cfg->output_enabled ? "ON" : "OFF");
                    
                    switch(ps_cfg->display_mode) {
                        case 0:  /* Voltage */
                            ps5004_set_display(g_system->modules[i].gpib_address, "VOLTAGE");
                            delay(100);
                            value = ps5004_read_value(g_system->modules[i].gpib_address);
                            printf("  Output Voltage: %.4f V\n", value);
                            break;
                            
                        case 1:  /* Current */
                            ps5004_set_display(g_system->modules[i].gpib_address, "CURRENT");
                            delay(100);
                            value = ps5004_read_value(g_system->modules[i].gpib_address);
                            printf("  Output Current: %.3f A (%.1f mA)\n", 
                                   value, value * 1000);
                            break;
                            
                        case 2:  /* Current Limit */
                            ps5004_set_display(g_system->modules[i].gpib_address, "CLIMIT");
                            delay(100);
                            value = ps5004_read_value(g_system->modules[i].gpib_address);
                            printf("  Current Limit: %.3f A (%.0f mA)\n", 
                                   value, value * 1000);
                            break;
                    }
                    
                    i = ps5004_get_regulation_status(g_system->modules[i].gpib_address);
                    printf("  Regulation: ");
                    switch(i) {
                        case 1: printf("Voltage (CV)\n"); break;
                        case 2: printf("Current (CC)\n"); break;
                        case 3: printf("Unregulated\n"); break;
                        default: printf("Unknown\n");
                    }
                    break;
                    
                default:
                    printf("Not implemented\n");
            }
            
            g_system->modules[i].last_reading = value;
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
}

void continuous_monitor_setup(void) {
    int done = 0;
    int choice;
    char buffer[20];
    
    while (!done) {
        clrscr();
        printf("Continuous Measurement Settings\n");
        printf("===========================\n\n");
        
        printf("Current Settings:\n");
        printf("-----------------\n");
        printf("1. Sample Rate: %d ms", g_control_panel.sample_rate_ms);
        if (g_control_panel.use_custom) {
            printf(" (custom)\n");
        } else {
            printf(" (preset %d)\n", g_control_panel.selected_rate + 1);
        }
        
        printf("2. Auto-start: %s\n", g_control_panel.running ? "Yes" : "No");
        
        printf("\nPreset Rates:\n");
        printf("3. 100ms    4. 250ms    5. 500ms\n");
        printf("6. 1 sec    7. 2 sec    8. 5 sec    9. 10 sec\n");
        
        printf("\nC. Custom rate\n");
		printf("M. Module selection (currently: %s)\n", 
			g_control_panel.monitor_all ? "ALL" : "Selective");
		printf("S. Start monitoring\n");
		printf("ESC. Return to main menu\n");
        
        printf("\nSelect option: ");
        choice = getch();
        
        switch(choice) {
            case '1':
            case '2':
                g_control_panel.running = !g_control_panel.running;
                break;
                
            case '3':
                g_control_panel.sample_rate_ms = 100;
                g_control_panel.selected_rate = 0;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "100");
                break;
                
            case '4':
                g_control_panel.sample_rate_ms = 250;
                g_control_panel.selected_rate = 1;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "250");
                break;
                
            case '5':
                g_control_panel.sample_rate_ms = 500;
                g_control_panel.selected_rate = 2;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "500");
                break;
                
            case '6':
                g_control_panel.sample_rate_ms = 1000;
                g_control_panel.selected_rate = 3;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "1000");
                break;
                
            case '7':
                g_control_panel.sample_rate_ms = 2000;
                g_control_panel.selected_rate = 4;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "2000");
                break;
                
            case '8':
                g_control_panel.sample_rate_ms = 5000;
                g_control_panel.selected_rate = 5;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "5000");
                break;
                
            case '9':
                g_control_panel.sample_rate_ms = 10000;
                g_control_panel.selected_rate = 6;
                g_control_panel.use_custom = 0;
                strcpy(g_control_panel.custom_rate, "10000");
                break;
                
            case 'c':
            case 'C':
                printf("\n\nEnter custom rate (ms, 10-60000): ");
                scanf("%s", buffer);
                while (getchar() != '\n');  /* Clear input buffer */
                {
                    int custom_val = atoi(buffer);
                    if (custom_val >= 10 && custom_val <= 60000) {
                        g_control_panel.sample_rate_ms = custom_val;
                        sprintf(g_control_panel.custom_rate, "%d", custom_val);
                        g_control_panel.use_custom = 1;
                    } else {
                        printf("Invalid rate. Press any key...");
                        getch();
                    }
                }
                break;
                
            case 's':
            case 'S':
                continuous_monitor();
                break;
                
            case 27: /* ESC */
                done = 1;
                break;
			
			case 'm':
			case 'M':
				module_selection_menu();
				break;
        }
    }
}

void continuous_monitor(void) {
    int i, done = 0;
    float value;
    time_t start_time = time(NULL);
    time_t current_time;
    unsigned long last_tick_count = 0;
    unsigned long current_tick_count;
    unsigned long tick_start, tick_end;
    unsigned long ticks_per_sample;
    unsigned long measurement_ticks = 0;
    ps5004_config *ps_cfg;
    char type_str[20];
    int key;
    int need_sample = 0;
    int samples_taken = 0;
    int active_modules = 0;
    int should_monitor;
    int display_update_counter = 0;  /* Reduce display update frequency */
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            active_modules++;
            if (!g_system->modules[i].module_data) {
                if (!allocate_module_buffer(i, 1000)) {
                    printf("WARNING: Failed to allocate buffer for slot %d!\n", i);
                }
            }
            clear_module_data(i);
        }
    }
    
    g_system->data_count = 0;
    
    ticks_per_sample = (g_control_panel.sample_rate_ms * 182L) / 10000L;
    if (ticks_per_sample < 1) ticks_per_sample = 1;
    
    clrscr();
    printf("Continuous Monitor - Press SPACE to start/stop, ESC to exit\n");
    printf("Sample rate: %d ms (%lu ticks), %d active modules\n", 
           g_control_panel.sample_rate_ms, ticks_per_sample, active_modules);
    printf("Commands: C=Clear data\n");
    printf("============================================================\n\n");
    
    last_tick_count = *((unsigned long far *)0x0040006CL);
    
    while (!done) {
        current_time = time(NULL);
        gotoxy(1, 5);
        printf("Time: %ld sec  Samples: %u  Status: %-8s  ", 
               current_time - start_time, 
               g_system->data_count,
               g_control_panel.running ? "RUNNING" : "STOPPED");
        
        if (++display_update_counter >= 10) {  /* Update every 10th iteration */
            display_update_counter = 0;
            if (measurement_ticks > 0 && g_control_panel.running) {
                printf("OH:%lu ", (measurement_ticks * 10000L) / 182L);
            }
        }
        printf("\n\n");
        
        current_tick_count = *((unsigned long far *)0x0040006CL);
        
        if (g_control_panel.running) {
            unsigned long adjusted_ticks = ticks_per_sample;
            if (measurement_ticks > 0 && measurement_ticks < ticks_per_sample) {
                adjusted_ticks = ticks_per_sample - measurement_ticks;
            }
            
            if ((current_tick_count - last_tick_count) >= adjusted_ticks) {
                need_sample = 1;
                last_tick_count = current_tick_count;
                tick_start = current_tick_count;
            }
        }
        
        samples_taken = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                should_monitor = g_control_panel.monitor_all || 
                                (g_control_panel.monitor_mask & (1 << i));
                
                if (!should_monitor && g_control_panel.running) {
                    continue;  /* Skip to next module */
                }
                
                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: strcpy(type_str, "DC5009"); break;
                    case MOD_DM5010: strcpy(type_str, "DM5010"); break;
                    case MOD_DM5120: 
                        strcpy(type_str, "DM5120");
                        if (g_dm5120_config[i].buffer_enabled) {
                            strcat(type_str, "*");  /* * indicates buffering active */
                        }
                        break;
                    case MOD_PS5004: strcpy(type_str, "PS5004"); break;
                    case MOD_PS5010: strcpy(type_str, "PS5010"); break;
                    case MOD_DC5010: strcpy(type_str, "DC5010"); break;
                    case MOD_FG5010: strcpy(type_str, "FG5010"); break;
                    default: strcpy(type_str, "Unknown"); break;
                }
                
                if (!should_monitor && !g_control_panel.monitor_all) {
                    strcat(type_str, "-");  /* - indicates not monitored */
                }
                
                printf("S%d %-6s[%2d]:", i, type_str, g_system->modules[i].gpib_address);
                
                if (g_control_panel.running && need_sample && should_monitor) {
                    switch(g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            value = read_dc5009(g_system->modules[i].gpib_address);
                            printf("%12.6f MHz   ", value / 1e6);
                            break;
                            
                        case MOD_DM5010:
                            value = read_dm5010_enhanced(g_system->modules[i].gpib_address, i);
                            printf("%12.4f V     ", value);
                            break;
                            
                        case MOD_DM5120:
                            {
                                dm5120_config *cfg = &g_dm5120_config[i];
                                
                                if (cfg->buffer_enabled && cfg->burst_mode) {
                                    float buffer[100];
                                    int samples_read;
                                    int j;
                                    
                                    dm5120_enable_buffering(g_system->modules[i].gpib_address, 
                                                          cfg->buffer_size);
                                    dm5120_start_buffer_sequence(g_system->modules[i].gpib_address);
                                    delay(100);  /* Wait for samples */
                                    
                                    samples_read = dm5120_get_buffer_data(
                                        g_system->modules[i].gpib_address, 
                                        buffer, 
                                        cfg->buffer_size);
                                    
                                    if (samples_read > 0) {
                                        float time_per_sample = 1000.0 / cfg->sample_rate; /* ms */
                                        
                                        for (j = 0; j < samples_read; j++) {
                                            store_module_data(i, buffer[j]);
                                            
                                            if (cfg->min_max_enabled) {
                                                if (buffer[j] < cfg->min_value) 
                                                    cfg->min_value = buffer[j];
                                                if (buffer[j] > cfg->max_value) 
                                                    cfg->max_value = buffer[j];
                                            }
                                            
                                            value = buffer[j];
                                        }
                                        
                                        cfg->sample_count += samples_read;
                                        
                                        if (g_system->data_count + samples_read - 1 < g_system->buffer_size) {
                                            g_system->data_count += samples_read - 1;
                                        }
                                        
                                        printf("%12.6f V [%d samples] ", 
                                               value, samples_read);
                                    } else {
                                        value = read_dm5120_voltage(g_system->modules[i].gpib_address);
                                        printf("%12.6f V     ", value);
                                    }
                                } else {
                                    value = read_dm5120_voltage(g_system->modules[i].gpib_address);
                                    if (value == 0.0) {
                                        value = read_dm5120(g_system->modules[i].gpib_address);
                                    }
                                    printf("%12.6f V     ", value);
                                    
                                    if (cfg->min_max_enabled) {
                                        if (value < cfg->min_value) cfg->min_value = value;
                                        if (value > cfg->max_value) cfg->max_value = value;
                                        cfg->sample_count++;
                                    }
                                }
                                
                                g_system->modules[i].last_reading = value;
                            }
                            break;
                            
                        case MOD_PS5004:
                            ps_cfg = &g_ps5004_config[i];
                            if (ps_cfg->display_mode == 1) {
                                ps5004_set_display(g_system->modules[i].gpib_address, "CURRENT");
                                delay(50);
                                value = ps5004_read_value(g_system->modules[i].gpib_address);
                                printf("%12.1f mA    ", value * 1000);
                            } else {
                                ps5004_set_display(g_system->modules[i].gpib_address, "VOLTAGE");
                                delay(50);
                                value = ps5004_read_value(g_system->modules[i].gpib_address);
                                printf("%12.4f V     ", value);
                            }
                            break;
                            
                        case MOD_PS5010:  
                            {
                                ps5010_config *ps_cfg = &g_ps5010_config[i];
                                int neg_stat, pos_stat, log_stat;
                                
                                if (ps5010_read_regulation(g_system->modules[i].gpib_address,
                                                          &neg_stat, &pos_stat, &log_stat)) {
                                    printf("P:%s N:%s L:%s     ",
                                           pos_stat == 1 ? "CV" : (pos_stat == 2 ? "CC" : "UR"),
                                           neg_stat == 1 ? "CV" : (neg_stat == 2 ? "CC" : "UR"),
                                           log_stat == 1 ? "CV" : (log_stat == 2 ? "CC" : "UR"));
                                    value = 0.0;  /* No actual measurement */
                                } else {
                                    printf("No status         ");
                                    value = 0.0;
                                }
                            }
                            break;
                            
                        default:
                            value = 0.0;
                            printf("Not implemented       ");
                    } 
                    
                    g_system->modules[i].last_reading = value;
                    
                    store_module_data(i, value);
                    
                    if (samples_taken == 0 && g_system->data_count < g_system->buffer_size) {
                        g_system->data_buffer[g_system->data_count] = value;
                    }
                    
                    samples_taken++;
                } else {
                    if (g_control_panel.running) {
                        if (should_monitor) {
                            printf("%12.6f       ", g_system->modules[i].last_reading);
                        } else {
                            printf(" [Not Selected]    ");
                        }
                    } else {
                        printf(" [Press SPACE]        ");
                    }
                }
                
                printf("\n");
            } 
        } 
        
        if (need_sample && g_control_panel.running && samples_taken > 0) {
            if (g_system->data_count < g_system->buffer_size) {
                g_system->data_count++;
            }
            
            tick_end = *((unsigned long far *)0x0040006CL);
            measurement_ticks = tick_end - tick_start;
            
            need_sample = 0;
        }
        
        if (kbhit()) {
            key = getch();
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case ' ':
                    g_control_panel.running = !g_control_panel.running;
                    if (g_control_panel.running) {
                        last_tick_count = *((unsigned long far *)0x0040006CL);
                        measurement_ticks = 0;
                    }
                    break;
                    
                case 'C':
                    for (i = 0; i < 10; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    gotoxy(1, 22);
                    printf("*** All data cleared ***");
                    delay(500);
                    gotoxy(1, 22);
                    printf("                        ");
                    break;
            } 
        }
        
        if (g_control_panel.running && samples_taken > 0) {
            delay(5);  /* Shorter delay when actively sampling */
        } else {
            delay(15);  /* Longer delay when idle to save CPU */
        }
    } 
    
    printf("\n\nMonitoring complete.\n");
    printf("Total samples: %u\n", g_system->data_count);
    if (g_system->data_count > 0) {
        printf("First value: %.6f\n", g_system->data_buffer[0]);
        printf("Last value: %.6f\n", g_system->data_buffer[g_system->data_count-1]);
    }
    printf("\nPress any key to continue...");
    getch();
}
void module_selection_menu(void) {
    int done = 0;
    int i, key;
    char status;
    
    while (!done) {
        clrscr();
        printf("\n\nModule Selection for Monitoring\n");
        printf("===============================\n\n");
        
        printf("Select which modules to monitor:\n");
        printf("(Press number to toggle, A for All, N for None, ESC to save)\n\n");
        
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                status = (g_control_panel.monitor_mask & (1 << i)) ? '*' : ' ';
                printf("%d. [%c] Slot %d: %s (GPIB %d)\n", 
                       i, status, i, 
                       g_system->modules[i].description,
                       g_system->modules[i].gpib_address);
            }
        }
        
        printf("\nCurrent mode: %s\n", 
               g_control_panel.monitor_all ? "Monitor ALL enabled modules" : "Selective monitoring");
        
        key = getch();
        
        if (key >= '0' && key <= '9') {
            i = key - '0';
            if (g_system->modules[i].enabled) {
                g_control_panel.monitor_mask ^= (1 << i);
                g_control_panel.monitor_all = 0;  /* Switch to selective mode */
            }
        } else if (toupper(key) == 'A') {
            g_control_panel.monitor_mask = 0xFF;
            g_control_panel.monitor_all = 1;
        } else if (toupper(key) == 'N') {
            g_control_panel.monitor_mask = 0;
            g_control_panel.monitor_all = 0;
        } else if (key == 27) {  /* ESC */
            done = 1;
        }
    }
}

void display_trace_selection_menu(void) {
    int done = 0;
    int i, key;
    char status;
    
    while (!done) {
        clrscr();
        printf("\n\nSelect Traces to Display in Graph\n");
        printf("=================================\n\n");
        
        printf("Select which traces to show in graph:\n");
        printf("(Press number to toggle, A for All, N for None, ESC to save)\n\n");
        
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                status = g_traces[i].enabled ? '*' : ' ';
                printf("%d. [%c] Slot %d: %s (%d samples)\n", 
                       i, status, i, 
                       g_traces[i].label,
                       g_traces[i].data_count);
            }
        }
        
        printf("\nActive traces: ");
        {
            int active_count = 0;
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled) {
                    if (active_count > 0) printf(", ");
                    printf("%d", i);
                    active_count++;
                }
            }
            if (active_count == 0) printf("None");
        }
        printf("\n");
        
        key = getch();
        
        if (key >= '0' && key <= '9') {
            i = key - '0';
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                g_traces[i].enabled = !g_traces[i].enabled;
            }
        } else if (toupper(key) == 'A') {
            for (i = 0; i < 10; i++) {
                if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                    g_traces[i].enabled = 1;
                }
            }
        } else if (toupper(key) == 'N') {
            for (i = 0; i < 10; i++) {
                g_traces[i].enabled = 0;
            }
        } else if (key == 27) {  /* ESC */
            done = 1;
        }
    }
}

void continuous_monitor_enhanced(void) {
    continuous_monitor();
}

unsigned char get_module_color(int module_type) {
    switch(module_type) {
        case MOD_DC5009: return 1;  /* Cyan */
        case MOD_DM5010: return 2;  /* Magenta */
        case MOD_DM5120: return 3;  /* White */
        case MOD_PS5004: return 1;  /* Cyan */
        case MOD_PS5010: return 2;  /* Magenta */
        case MOD_DC5010: return 3;  /* White */
        case MOD_FG5010: return 1;  /* Cyan */
        default: return 3;          /* White */
    }
}

void draw_text(int x, int y, char *text, unsigned char color) {
    draw_text_scaled(x, y, text, color, 1, 1);
}

void draw_text_scaled(int x, int y, char *text, unsigned char color, int scale_x, int scale_y) {
    int i, j, sx, sy;
    unsigned char font_byte;
    int char_index;
    
    while (*text) {
        char_index = get_font_index(*text);
        
        for (i = 0; i < 7; i++) {
            if (char_index >= 0) {
                font_byte = enhanced_font[char_index][i];
            } else {
                font_byte = scientific_font[(-char_index) - 1][i];
            }
            
            for (j = 0; j < 5; j++) {
                if (font_byte & (0x10 >> j)) {
                    for (sy = 0; sy < scale_y; sy++) {
                        for (sx = 0; sx < scale_x; sx++) {
                            plot_pixel(x + j*scale_x + sx, y + i*scale_y + sy, color);
                        }
                    }
                }
            }
        }
        
        x += 6 * scale_x;  /* Character width + spacing */
        text++;
        
        if (x > SCREEN_WIDTH - 6*scale_x) break;
    }
}

void draw_grid_vline(int x, float value, unsigned char color) {
    int y;
    char label[10];
    int int_val = (int)value;
    
    for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 4) {
        plot_pixel(x, y, color);
    }
    
    sprintf(label, "%d", int_val);
    draw_text(x - 6, GRAPH_BOTTOM + 3, label, color);
}

void draw_grid_hline(int y, float value, unsigned char color) {
    int x;
    char label[10];
    
    for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 4) {
        plot_pixel(x, y, color);
    }
    
    sprintf(label, "%.1f", value);
    
    draw_text(2, y - 3, label, color);
}

void get_graph_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 0.000001) {
        *unit_str = "uV";
        *scale_factor = 1000000.0;
        *decimal_places = 1;
    } else if (range < 0.001) {
        *unit_str = "uV";
        *scale_factor = 1000000.0;
        *decimal_places = 0;
    } else if (range < 0.01) {
        *unit_str = "mV";
        *scale_factor = 1000.0;
        *decimal_places = 3;  /* Reduced from 4 for cleaner display */
    } else if (range < 0.1) {
        *unit_str = "mV";
        *scale_factor = 1000.0;
        *decimal_places = 2;  /* Reduced from 3 for cleaner display */
    } else if (range < 1.0) {
        *unit_str = "mV";
        *scale_factor = 1000.0;
        *decimal_places = 1;
    } else if (range < 10.0) {
        *unit_str = "mV";
        *scale_factor = 1000.0;
        *decimal_places = 0;
    } else {
        *unit_str = "V";
        *scale_factor = 1.0;
        *decimal_places = 2;  /* Reduced from 3 for cleaner display */
    }
}

void graph_display(void) {
    int done = 0;
    int i, j, x, y, old_x = -1, old_y = -1;
    int key;
    char readout[80];
    float value;
    int mouse_visible = 0;
    int need_redraw = 1;
    int active_traces = 0;
    int any_data = 0;
    int max_samples = 0;
    int cursor_x = -1;        
    int selected_trace = -1;  
    int cursor_visible = 0;
    float x_normalized;       
    int sample_num;          
    int slot;                
    float center, range, shift;  
    float x_scale;           
    int x_pos1, x_pos2, y_pos1, y_pos2;  
    int old_cursor_x = -1;   
    char *unit_str;          
    int current_sample;      
    float sample_to_x_scale; 
    int keyboard_mode = 1;   
    float y_scale;           
    float normalized_y;      
    int readout_needs_update = 1;  
    int last_readout_x = -1;       
    int shift_pressed = 0;   
    int is_fft_trace[8];     
    float freq_scale;        
    char *freq_unit_str;     
    float freq_multiplier;   
    int should_monitor;      
    char *display_unit_str;      
    float display_scale_factor;  
    int display_decimal_places;  
    float y_range;
    
    for (i = 0; i < 10; i++) {
        is_fft_trace[i] = 0;
        if (g_system->modules[i].enabled && 
            strcmp(g_system->modules[i].description, "FFT Result") == 0) {
            is_fft_trace[i] = 1;
        }
    }
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && 
            g_system->modules[i].module_data && 
            g_system->modules[i].module_data_count > 0) {
            any_data = 1;
            if (g_system->modules[i].module_data_count > max_samples) {
                max_samples = g_system->modules[i].module_data_count;
            }
        }
    }
    
    if (!any_data && g_system->data_count == 0) {
        printf("No data to display. Run measurements first.\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    sync_traces_with_modules();
    
    if (selected_trace == -1) {
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                selected_trace = i;
                break;
            }
        }
    }
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled) {
            active_traces++;
        }
    }
    
    if (selected_trace >= 0 && selected_trace < 10 && g_traces[selected_trace].enabled) {
        current_sample = g_traces[selected_trace].data_count / 2;
        if (g_has_287) {
            sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
            cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
        } else {
            sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
            cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
        }
    } else {
        cursor_x = GRAPH_LEFT + GRAPH_WIDTH / 2;
    }
    
    init_graphics();
    
    if (g_graph_scale.auto_scale) {
        auto_scale_graph();
    }
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
    
    if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
        float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
        float max_freq = sample_rate / 2.0;
        
        if (max_freq >= 1000000.0) {
            freq_unit_str = "MHz";
            freq_multiplier = 1000000.0;
        } else if (max_freq >= 1000.0) {
            freq_unit_str = "kHz";
            freq_multiplier = 1000.0;
        } else {
            freq_unit_str = "Hz";
            freq_multiplier = 1.0;
        }
        unit_str = freq_unit_str;
    } else {
        unit_str = display_unit_str;
    }
    
    if (g_has_287) {
        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
    }
    
    while (!done) {
        if (need_redraw) {
            _fmemset(video_mem, 0, 16384);
            
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                draw_frequency_grid(g_traces[selected_trace].data_count);
            } else {
                draw_grid_dynamic(max_samples);
            }
            
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 1) {
                    int decimation_factor = 1;
                    int effective_samples;
                    int start_sample = g_graph_scale.sample_start;
                    int display_count;
                    
                    if (g_graph_scale.sample_count == 0) {
                        start_sample = 0;
                        display_count = g_traces[i].data_count;
                    } else {
                        display_count = g_graph_scale.sample_count;
                        if (start_sample >= g_traces[i].data_count) {
                            start_sample = 0;
                        }
                        if (start_sample + display_count > g_traces[i].data_count) {
                            display_count = g_traces[i].data_count - start_sample;
                        }
                    }
                    
                    effective_samples = display_count;
                    
                    if (display_count > GRAPH_WIDTH) {
                        if (g_has_287) {
                            decimation_factor = (display_count + GRAPH_WIDTH - 1) / GRAPH_WIDTH;
                        } else {
                            decimation_factor = display_count / GRAPH_WIDTH;
                            if (decimation_factor < 1) decimation_factor = 1;
                        }
                        effective_samples = display_count / decimation_factor;
                    }
                    
                    if (g_has_287) {
                        x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                    } else {
                        x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                    }
                    
                    for (j = 0; j < effective_samples - 1; j++) {
                        int sample_idx1 = start_sample + (j * decimation_factor);
                        int sample_idx2 = start_sample + ((j + 1) * decimation_factor);
                        
                        if (sample_idx1 >= g_traces[i].data_count) sample_idx1 = g_traces[i].data_count - 1;
                        if (sample_idx2 >= g_traces[i].data_count) sample_idx2 = g_traces[i].data_count - 1;
                        if (g_has_287) {
                            x_pos1 = GRAPH_LEFT + (int)(j * x_scale);
                            x_pos2 = GRAPH_LEFT + (int)((j + 1) * x_scale);
                        } else {
                            x_pos1 = GRAPH_LEFT + (int)((float)j * x_scale);
                            x_pos2 = GRAPH_LEFT + (int)((float)(j + 1) * x_scale);
                        }
                        
                        if (x_pos1 < GRAPH_LEFT) x_pos1 = GRAPH_LEFT;
                        if (x_pos2 > GRAPH_RIGHT) x_pos2 = GRAPH_RIGHT;
                        
                        if (g_has_287) {
                            normalized_y = (g_traces[i].data[sample_idx1] - g_graph_scale.min_value) * y_scale;
                            y_pos1 = GRAPH_BOTTOM - (int)normalized_y;
                            
                            normalized_y = (g_traces[i].data[sample_idx2] - g_graph_scale.min_value) * y_scale;
                            y_pos2 = GRAPH_BOTTOM - (int)normalized_y;
                        } else {
                            y_pos1 = value_to_y(g_traces[i].data[sample_idx1]);
                            y_pos2 = value_to_y(g_traces[i].data[sample_idx2]);
                        }
                        
                        if (y_pos1 < GRAPH_TOP) y_pos1 = GRAPH_TOP;
                        if (y_pos1 > GRAPH_BOTTOM) y_pos1 = GRAPH_BOTTOM;
                        if (y_pos2 < GRAPH_TOP) y_pos2 = GRAPH_TOP;
                        if (y_pos2 > GRAPH_BOTTOM) y_pos2 = GRAPH_BOTTOM;
                        
                        if (x_pos1 >= GRAPH_LEFT && x_pos2 <= GRAPH_RIGHT) {
                            if (i == selected_trace) {
                                draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                                if (y_pos1 > GRAPH_TOP && y_pos1 < GRAPH_BOTTOM) {
                                    draw_line_aa(x_pos1, y_pos1-1, x_pos2, y_pos2-1, g_traces[i].color);
                                }
                            } else {
                                draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                            }
                        }
                    }
                }
            }
            
            draw_legend_enhanced(is_fft_trace);
            
            draw_gradient_rect(0, 185, SCREEN_WIDTH-1, 199, 1, 2);
            
            draw_text(2, 188, "A:Auto +/-:Zoom H:Help/Config", 3);
            draw_text(2, 195, "0-9:Select ALT+#:Toggle ESC:Exit", 3);
            
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                float max_freq = sample_rate / 2.0;
                sprintf(readout, "0-%.1f%s", max_freq / freq_multiplier, freq_unit_str);
            } else {
                if (display_decimal_places == 0) {
                    sprintf(readout, "%.0f-%.0f%s", 
                            g_graph_scale.min_value * display_scale_factor,
                            g_graph_scale.max_value * display_scale_factor,
                            display_unit_str);
                } else {
                    sprintf(readout, "%.*f-%.*f%s", 
                            display_decimal_places, g_graph_scale.min_value * display_scale_factor,
                            display_decimal_places, g_graph_scale.max_value * display_scale_factor,
                            display_unit_str);
                }
            }
            draw_text(240, 188, readout, 3);
            
            if (g_has_287) {
                draw_text(240, 195, "287", 2);
            }
            
            if (selected_trace >= 0 && g_traces[selected_trace].enabled && 
                g_traces[selected_trace].data_count >= 1000) {
                draw_text(255, 195, "1K", 2);
            }
            
            if (g_graph_scale.sample_count > 0) {
                char range_str[20];
                sprintf(range_str, "R:%d-%d", 
                        g_graph_scale.sample_start + 1, 
                        g_graph_scale.sample_start + g_graph_scale.sample_count);
                draw_text(270, 195, range_str, 6);  /* Yellow for range indicator */
            }
            
            if (selected_trace >= 0 && g_system->modules[selected_trace].module_type == MOD_DM5120) {
                dm5120_config *cfg = &g_dm5120_config[selected_trace];
                if (cfg->buffer_enabled && cfg->burst_mode) {
                    sprintf(readout, "BUF@%.0fHz", cfg->sample_rate);
                    draw_text(280, 195, readout, 1);
                }
            }
            
            need_redraw = 0;
            cursor_visible = 1;
        }
        
        if (cursor_x != old_cursor_x && cursor_visible) {
            need_redraw = 1;
            old_cursor_x = cursor_x;
            readout_needs_update = 1;
            continue;
        }
        
        if (cursor_visible && cursor_x >= GRAPH_LEFT && cursor_x <= GRAPH_RIGHT && 
            readout_needs_update && !need_redraw) {
            
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 2) {
                plot_pixel(cursor_x, y, 3);
            }
            
            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                if (keyboard_mode && g_has_287) {
                    sample_num = current_sample;
                } else {
                    float effective_rate = 1.0;
                    if (g_system->modules[selected_trace].module_type == MOD_DM5120) {
                        dm5120_config *cfg = &g_dm5120_config[selected_trace];
                        if (cfg->buffer_enabled && cfg->burst_mode) {
                            effective_rate = cfg->sample_rate / (1000.0 / g_control_panel.sample_rate_ms);
                        }
                    }
                    
                    x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                    if (effective_rate > 1.0) {
                        sample_num = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) * effective_rate + 0.5);
                    } else {
                        sample_num = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                    }
                }
                
                if (sample_num < 0) sample_num = 0;
                if (sample_num >= g_traces[selected_trace].data_count) {
                    sample_num = g_traces[selected_trace].data_count - 1;
                }
                
                if (keyboard_mode && sample_num >= 0 && sample_num < g_traces[selected_trace].data_count) {
                    value = g_traces[selected_trace].data[sample_num];
                } else {
                    value = g_traces[selected_trace].data[sample_num];
                }
                
                if (is_fft_trace[selected_trace]) {
                    float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                    float freq = (sample_rate / 2.0) * sample_num / (g_traces[selected_trace].data_count - 1);
                    sprintf(readout, "S%d[%.1f%s]:%.3f", selected_trace, 
                            freq / freq_multiplier, freq_unit_str, value);
                } else {
                    if (display_decimal_places == 0) {
                        sprintf(readout, "S%d[%d]:%.0f %s", selected_trace, sample_num, 
                                value * display_scale_factor, display_unit_str);
                    } else {
                        sprintf(readout, "S%d[%d]:%.*f %s", selected_trace, sample_num, 
                                display_decimal_places, value * display_scale_factor, display_unit_str);
                    }
                }
                
                {
                    int readout_x = cursor_x - 30;
                    if (readout_x < GRAPH_LEFT) readout_x = GRAPH_LEFT;
                    if (readout_x > GRAPH_RIGHT - 60) readout_x = GRAPH_RIGHT - 60;
                    draw_text(readout_x, GRAPH_TOP + 20, readout, 3);
                }
            }
            
            readout_needs_update = 0;
            last_readout_x = cursor_x;
        }
        
        if (g_mouse.present && mouse_visible) {
            get_mouse_status();
            
            x = g_mouse.x * 320 / 80;
            y = g_mouse.y * 200 / 25;
            
            if ((x != old_x || y != old_y) && 
                x >= GRAPH_LEFT && x <= GRAPH_RIGHT &&
                y >= GRAPH_TOP && y <= GRAPH_BOTTOM) {
                
                keyboard_mode = 0;
                cursor_x = x;
                cursor_visible = 1;
                
                old_x = x;
                old_y = y;
            }
        }
        
        if (kbhit()) {
            key = getch();
            
            if (key >= 128 && key <= 137) {  /* ALT+0 through ALT+9 */
                slot = key - 128;  /* Convert to slot number 0-9 */
                
                if (slot < 10) {
                    if (g_system->modules[slot].enabled && 
                        g_system->modules[slot].module_data &&
                        g_system->modules[slot].module_data_count > 0) {
                        g_traces[slot].enabled = !g_traces[slot].enabled;
                        
                        active_traces = 0;
                        for (i = 0; i < 10; i++) {
                            if (g_traces[i].enabled) active_traces++;
                        }
                        
                        if (!g_traces[selected_trace].enabled) {
                            selected_trace = -1;
                            for (i = 0; i < 10; i++) {
                                if (g_traces[i].enabled) {
                                    selected_trace = i;
                                    break;
                                }
                            }
                        }
                        
                        need_redraw = 1;
                    }
                }
                continue;
            }
            
            if (key >= '0' && key <= '9') {
                slot = key - '0';
                
                if (g_traces[slot].enabled) {
                    selected_trace = slot;
                    cursor_visible = 1;
                    keyboard_mode = 1;
                    
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    
                    if (is_fft_trace[selected_trace]) {
                        float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                        float max_freq = sample_rate / 2.0;
                        
                        if (max_freq >= 1000000.0) {
                            freq_unit_str = "MHz";
                            freq_multiplier = 1000000.0;
                        } else if (max_freq >= 1000.0) {
                            freq_unit_str = "kHz";
                            freq_multiplier = 1000.0;
                        } else {
                            freq_unit_str = "Hz";
                            freq_multiplier = 1.0;
                        }
                        unit_str = freq_unit_str;
                    } else {
                        unit_str = display_unit_str;
                    }
                    
                    if (g_has_287) {
                        x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                        current_sample = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                        sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                    } else {
                        current_sample = g_traces[selected_trace].data_count / 2;
                        sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                    }
                    
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    
                    need_redraw = 1;
                }
                continue;
            }
            
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case 'A':  /* Auto-scale */
                    g_graph_scale.auto_scale = 1;
                    auto_scale_graph();
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case 'R':  /* Manual redraw */
                    need_redraw = 1;
                    cursor_visible = 1;
                    if (selected_trace >= 0 && g_traces[selected_trace].enabled && keyboard_mode) {
                        x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                        current_sample = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                        if (current_sample < 0) current_sample = 0;
                        if (current_sample >= g_traces[selected_trace].data_count) {
                            current_sample = g_traces[selected_trace].data_count - 1;
                        }
                    }
                    break;
                    
                case '+':  /* Zoom in */
                    if (g_has_287) {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) * 0.5;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.4;
                    } else {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 0.8;
                    }
                    
                    if (range < 0.000010) {  /* Less than 10µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 1.0) range_uv = 1.0;  /* Minimum 1µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000050) {  /* Less than 50µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.005) {  /* Less than 5mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
                        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    else if (range < 0.050) {  /* Less than 50mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
                        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    
                    g_graph_scale.min_value = center - range;
                    g_graph_scale.max_value = center + range;
                    g_graph_scale.auto_scale = 0;
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case '-':  /* Zoom out */
                    if (g_has_287) {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) * 0.5;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.625;
                    } else {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 1.25;
                    }
                    
                    if (range < 0.000010) {  /* Less than 10µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 1.0) range_uv = 1.0;  /* Minimum 1µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000050) {  /* Less than 50µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.005) {  /* Less than 5mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
                        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    else if (range < 0.050) {  /* Less than 50mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
                        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    
                    g_graph_scale.min_value = center - range;
                    g_graph_scale.max_value = center + range;
                    g_graph_scale.auto_scale = 0;
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case 0:  /* Extended key */
                    key = getch();
                    switch(key) {
                        case 72:  /* Up arrow - pan up */
                            shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                            g_graph_scale.min_value += shift;
                            g_graph_scale.max_value += shift;
                            g_graph_scale.auto_scale = 0;
                            if (g_has_287) {
                                y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                            }
                            need_redraw = 1;
                            break;
                            
                        case 80:  /* Down arrow - pan down */
                            shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                            g_graph_scale.min_value -= shift;
                            g_graph_scale.max_value -= shift;
                            g_graph_scale.auto_scale = 0;
                            if (g_has_287) {
                                y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                            }
                            need_redraw = 1;
                            break;
                            
                        case 75:  /* Left arrow - move one sample left */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                if (current_sample > 0) {
                                    current_sample--;
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                    }
                                    cursor_visible = 1;
                                }
                            }
                            break;
                            
                        case 77:  /* Right arrow - move one sample right */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                if (current_sample < g_traces[selected_trace].data_count - 1) {
                                    current_sample++;
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                    }
                                    cursor_visible = 1;
                                }
                            }
                            break;
                    }
                    break;
                    
                case 'M':  /* Toggle mouse */
                    if (g_mouse.present) {
                        mouse_visible = !mouse_visible;
                        if (!mouse_visible) {
                            keyboard_mode = 1;
                        }
                    }
                    break;
                    
                case 'C':  /* Clear data */
                    for (i = 0; i < 10; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    text_mode();
                    printf("\nAll data cleared. Press any key...");
                    getch();
                    done = 1;
                    break;
                
                case 'P':  /* Print graph */
                    text_mode();
                    print_graph_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
                    
                case 'H':  /* Help/Config menu */
                case 'h':
                    text_mode();
                    graph_config_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
            }
        }
        
        delay(10);
    }
    
    text_mode();
}
void print_graph_menu(void) {
    int choice;
    
    clrscr();
    printf("Print Graph Options\n");
    printf("===================\n\n");
    printf("1. Text-based graph (80 column)\n");
    printf("2. Brother PostScript (HL5370DL)\n");
    printf("0. Cancel\n\n");
    printf("Select option: ");
    
    choice = getch();
    
    switch(choice) {
        case '1':
            print_graph_text();
            break;
        case '2':
            print_graph_postscript();
            break;
        default:
            break;
    }
}

void print_graph_text(void) {
    int i, j, x, y;
    char line[81];
    char plot_chars[80];
    float value, y_range;
    int y_pos;
    int samples_to_print;
    char label[40];
    int trace_count = 0;
    int max_samples = 0;
    float line_value;  
    char plot_char;
    char *unit_str;
    float scale_factor;
    int decimal_places;
    float min_val, max_val;
    int graph_lines = 50;  /* Increased from 40 for better resolution */
    
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
    
    min_val = g_graph_scale.min_value;
    max_val = g_graph_scale.max_value;
    y_range = max_val - min_val;
    
    get_graph_units(y_range, &unit_str, &scale_factor, &decimal_places);
    
    if (unit_str[0] == 'u' && unit_str[1] == 'V') {
        unit_str = "uV";  /* Use 'u' instead of µ for ASCII compatibility */
    }
    
    lpt_send_byte(0x1B); lpt_send_byte(0x40); /* Reset */
    lpt_send_byte(0x0D); /* CR */
    lpt_send_byte(0x0A); /* LF */
    
    print_string("TM5000 HIGH-PRECISION MEASUREMENT GRAPH\r\n");
    print_string("=======================================\r\n");
    
    printf("\nEnter custom header (or press Enter for none): ");
    fflush(stdout);
    {
        char custom_header[256];
        if (fgets(custom_header, sizeof(custom_header), stdin) && custom_header[0] != '\n') {
            custom_header[strcspn(custom_header, "\n")] = '\0';
            print_string(custom_header);
            print_string("\r\n");
        }
    }
    print_string("\r\n");
    
    if (decimal_places == 0) {
        sprintf(label, "Y Scale: %.0f to %.0f %s\r\n", 
                min_val * scale_factor,
                max_val * scale_factor,
                unit_str);
    } else if (decimal_places >= 4) {
        sprintf(label, "Y Scale: %.*f to %.*f %s\r\n", 
                decimal_places, min_val * scale_factor,
                decimal_places, max_val * scale_factor,
                unit_str);
    } else {
        sprintf(label, "Y Scale: %.*f to %.*f %s\r\n", 
                decimal_places, min_val * scale_factor,
                decimal_places, max_val * scale_factor,
                unit_str);
    }
    print_string(label);
    
    {
        char *range_unit_str;
        float range_scale_factor;
        int range_decimal_places;
        
        get_graph_units(y_range, &range_unit_str, &range_scale_factor, &range_decimal_places);
        
        if (range_unit_str[0] == 'u' && range_unit_str[1] == 'V') {
            range_unit_str = "uV";
        }
        
        if (range_decimal_places == 0) {
            sprintf(label, "Range: %.0f %s\r\n", 
                    y_range * range_scale_factor, range_unit_str);
        } else {
            sprintf(label, "Range: %.*f %s\r\n", 
                    range_decimal_places, y_range * range_scale_factor, range_unit_str);
        }
    }
    print_string(label);
    
    sprintf(label, "Sample Rate: %d ms, Total Samples: %d\r\n", 
            g_control_panel.sample_rate_ms, max_samples);
    print_string(label);
    
    print_string("Measurement Statistics:\r\n");
    print_string("----------------------\r\n");
    {
        time_t current_time;
        struct tm *time_info;
        float total_time_sec = (max_samples * g_control_panel.sample_rate_ms) / 1000.0;
        int active_traces = 0;
        
        {
            int i;
            for (i = 0; i < 10; i++) {
                if (g_traces[i].data_count > 0) active_traces++;
            }
        }
        
        sprintf(label, "Measurement Duration: %.2f seconds\r\n", total_time_sec);
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
        
        sprintf(label, "Zoom Factor: %.2fx, Auto Scale: %s\r\n",
                g_graph_scale.zoom_factor,
                g_graph_scale.auto_scale ? "ON" : "OFF");
        print_string(label);
    }
    print_string("\r\n");
    
    sprintf(label, "Graph Resolution: %.2e %s per line\r\n\r\n", 
            (y_range * scale_factor) / (float)(graph_lines - 1), unit_str);
    print_string(label);
    
    for (y = 0; y < graph_lines; y++) {
        for (x = 0; x < 100; x++) {
            line[x] = ' ';
            plot_chars[x] = ' ';
        }
        line[80] = '\0';
        
        line_value = max_val - (y * y_range / (float)(graph_lines - 1));
        
        line[0] = '|';
        
        if (y % 5 == 0) {
            if (decimal_places == 0) {
                sprintf(label, "%7.0f", line_value * scale_factor);
            } else if (decimal_places >= 4) {
                sprintf(label, "%*.*f", 7 + decimal_places, decimal_places, 
                        line_value * scale_factor);
            } else {
                sprintf(label, "%7.*f", decimal_places, line_value * scale_factor);
            }
            
            for (j = 0; j < 7 + decimal_places && j < 12 && label[j]; j++) {
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
                    j = (x * g_traces[i].data_count) / 65;
                    if (j >= g_traces[i].data_count) j = g_traces[i].data_count - 1;
                    
                    value = g_traces[i].data[j];
                    
                    y_pos = (int)((float)(graph_lines - 1) * (max_val - value) / y_range + 0.5);
                    
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
    for (x = 1; x <= 6; x++) {
        sprintf(label, "%10d", (max_samples * x) / 6);
        print_string(label);
    }
    print_string("\r\n");
    print_string("                              Sample Number\r\n\r\n");
    
    print_string("Legend:\r\n");
    print_string("-------\r\n");
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            char symbol;
            float last_val, min_trace, max_trace;
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
                float last_scale, min_scale, max_scale;
                int last_dec, min_dec, max_dec;
                float trace_range = max_trace - min_trace;
                
                get_graph_units(fabs(last_val), &last_unit, &last_scale, &last_dec);
                get_graph_units(fabs(min_trace), &min_unit, &min_scale, &min_dec);
                get_graph_units(fabs(max_trace), &max_unit, &max_scale, &max_dec);
                
                if (last_unit[0] == 'u' && last_unit[1] == 'V') last_unit = "uV";
                if (min_unit[0] == 'u' && min_unit[1] == 'V') min_unit = "uV";
                if (max_unit[0] == 'u' && max_unit[1] == 'V') max_unit = "uV";
                
                sprintf(label, "      Last: %.*f %s, Min: %.*f %s, Max: %.*f %s\r\n",
                        last_dec, last_val * last_scale, last_unit,
                        min_dec, min_trace * min_scale, min_unit,
                        max_dec, max_trace * max_scale, max_unit);
                print_string(label);
                
                {
                    char *range_unit, *avg_unit, *rms_unit;
                    float range_scale, avg_scale, rms_scale;
                    int range_dec, avg_dec, rms_dec;
                    float avg_val = 0.0, rms_val = 0.0;
                    
                    for (k = 0; k < g_traces[i].data_count; k++) {
                        avg_val += g_traces[i].data[k];
                        rms_val += g_traces[i].data[k] * g_traces[i].data[k];
                    }
                    avg_val /= g_traces[i].data_count;
                    rms_val = sqrt(rms_val / g_traces[i].data_count);
                    
                    get_graph_units(trace_range, &range_unit, &range_scale, &range_dec);
                    get_graph_units(fabs(avg_val), &avg_unit, &avg_scale, &avg_dec);
                    get_graph_units(rms_val, &rms_unit, &rms_scale, &rms_dec);
                    
                    if (range_unit[0] == 'u' && range_unit[1] == 'V') range_unit = "uV";
                    if (avg_unit[0] == 'u' && avg_unit[1] == 'V') avg_unit = "uV";
                    if (rms_unit[0] == 'u' && rms_unit[1] == 'V') rms_unit = "uV";
                    
                    sprintf(label, "      Range: %.*f %s, Avg: %.*f %s, RMS: %.*f %s\r\n",
                            range_dec, trace_range * range_scale, range_unit,
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
    float value, y_range;
    char label[100];
    int trace_count = 0;
    float x_scale, y_scale;
    float page_width = 540.0;   
    float page_height = 720.0;  
    float graph_x = 72.0;       
    float graph_y = 144.0;      
    float graph_width = 468.0;  
    float graph_height = 432.0; 
    float y_pos, x_pos;         
    int sample_num;             
    char *unit_str;
    float scale_factor;
    int decimal_places;
    float min_val, max_val;
    int max_samples = 0;
    
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
    
    min_val = g_graph_scale.min_value;
    max_val = g_graph_scale.max_value;
    y_range = max_val - min_val;
    
    get_graph_units(y_range, &unit_str, &scale_factor, &decimal_places);
    
    print_string("%!PS-Adobe-3.0\r\n");
    print_string("%%Title: TM5000 High-Precision Measurement Graph\r\n");
    print_string("%%Creator: TM5000 Control System v2.9\r\n");
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
    print_string("gsave\r\n");
    
    print_string("72 720 moveto\r\n");
    print_string("(TM5000 HIGH-PRECISION MEASUREMENT GRAPH) show\r\n");
    
    printf("\n=== CUSTOM HEADER ENTRY ===\n");
    printf("Enter custom header text (up to 255 characters):\n");
    printf("Press Enter for no custom header, or type your text: ");
    fflush(stdout);
    {
        char custom_header[256];
        if (fgets(custom_header, sizeof(custom_header), stdin) && custom_header[0] != '\n') {
            custom_header[strcspn(custom_header, "\n")] = '\0';
            
            print_string("72 705 moveto\r\n");  /* Position below main title */
            print_string("(");
            print_string(custom_header);
            print_string(") show\r\n");
        }
    }
    
    print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
    print_string("72 700 moveto\r\n");
    if (decimal_places == 0) {
        sprintf(label, "(Scale: %.0f to %.0f ) show ", 
                min_val * scale_factor,
                max_val * scale_factor);
    } else {
        sprintf(label, "(Scale: %.*f to %.*f ) show ", 
                decimal_places, min_val * scale_factor,
                decimal_places, max_val * scale_factor);
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
        float range_scale_factor;
        int range_decimal_places;
        
        get_graph_units(y_range, &range_unit_str, &range_scale_factor, &range_decimal_places);
        
        if (range_decimal_places == 0) {
            sprintf(label, "(Range: %.0f ) show ", y_range * range_scale_factor);
        } else {
            sprintf(label, "(Range: %.*f ) show ", range_decimal_places, y_range * range_scale_factor);
        }
        print_string(label);
        if (strcmp(range_unit_str, "uV") == 0) {
            print_string("( ) show /mu glyphshow (V, Sample Rate: ) show ");
        } else {
            sprintf(label, "(%s, Sample Rate: ) show ", range_unit_str);
            print_string(label);
        }
    }
    sprintf(label, "(%d ms) show\r\n", g_control_panel.sample_rate_ms);
    print_string(label);
    
    print_string("72 670 moveto\r\n");
    print_string("(Measurement Statistics:) show\r\n");
    {
        time_t current_time;
        struct tm *time_info;
        float total_time_sec = (max_samples * g_control_panel.sample_rate_ms) / 1000.0;
        int active_traces = 0;
        
        {
            int i;
            for (i = 0; i < 10; i++) {
                if (g_traces[i].data_count > 0) active_traces++;
            }
        }
        
        print_string("72 655 moveto\r\n");
        sprintf(label, "(Duration: %.2f sec, Active Traces: %d, Data Points: %d) show\r\n", 
                total_time_sec, active_traces, max_samples);
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
            sprintf(label, "(Sample Range: %d to %d, Zoom: %.2fx, Auto Scale: %s) show\r\n",
                    g_graph_scale.sample_start + 1,
                    g_graph_scale.sample_start + g_graph_scale.sample_count,
                    g_graph_scale.zoom_factor,
                    g_graph_scale.auto_scale ? "ON" : "OFF");
        } else {
            sprintf(label, "(Sample Range: All (1 to %d), Zoom: %.2fx, Auto Scale: %s) show\r\n", 
                    max_samples,
                    g_graph_scale.zoom_factor,
                    g_graph_scale.auto_scale ? "ON" : "OFF");
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
        float y_label_value;
        y_pos = graph_y + (graph_height * i / 10);
        y_label_value = min_val + (y_range * i / 10);
        
        sprintf(label, "%.1f %.1f moveto\r\n", graph_x - 45, y_pos - 3);
        print_string(label);
        if (decimal_places == 0) {
            sprintf(label, "(%.0f) show\r\n", y_label_value * scale_factor);
        } else {
            sprintf(label, "(%.*f) show\r\n", decimal_places, y_label_value * scale_factor);
        }
        print_string(label);
    }
    
    for (i = 0; i <= 10; i++) {
        x_pos = graph_x + (graph_width * i / 10);
        sample_num = 0;
        
        for (j = 0; j < 10; j++) {
            if (g_traces[j].enabled && g_traces[j].data_count > sample_num) {
                sample_num = g_traces[j].data_count;
            }
        }
        sample_num = (sample_num * i) / 10;
        
        if (i % 2 == 0) {  /* Label every other tick for clarity */
            sprintf(label, "%.1f %.1f moveto\r\n", x_pos - 10, graph_y - 15);
            print_string(label);
            sprintf(label, "(%d) show\r\n", sample_num);
            print_string(label);
        }
    }
    
    print_string("/Times-Roman findfont 10 scalefont setfont\r\n");
    sprintf(label, "%.1f %.1f moveto\r\n", graph_x + graph_width/2 - 30, graph_y - 30);
    print_string(label);
    print_string("(Sample Number) show\r\n");
    
    print_string("gsave\r\n");
    sprintf(label, "%.1f %.1f translate\r\n", graph_x - 60, graph_y + graph_height/2);
    print_string(label);
    print_string("90 rotate\r\n");
    print_string("0 0 moveto\r\n");
    print_string("(Measurement [) show ");
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
            
            x_scale = graph_width / (float)(g_traces[i].data_count - 1);
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
            float min_trace, max_trace;
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
                float min_scale, max_scale, range_scale;
                int min_dec, max_dec, range_dec;
                float trace_range = max_trace - min_trace;
                
                get_graph_units(fabs(min_trace), &min_unit, &min_scale, &min_dec);
                get_graph_units(fabs(max_trace), &max_unit, &max_scale, &max_dec);
                get_graph_units(trace_range, &range_unit, &range_scale, &range_dec);
                
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
                if (strcmp(max_unit, "uV") == 0) {
                    print_string("( ) show /mu glyphshow (V, Range: ) show ");
                } else {
                    sprintf(label, "(%s, Range: ) show ", max_unit);
                    print_string(label);
                }
                
                sprintf(label, "(%.*f) show ", range_dec, trace_range * range_scale);
                print_string(label);
                if (strcmp(range_unit, "uV") == 0) {
                    print_string("( ) show /mu glyphshow (V) show\r\n");
                } else {
                    sprintf(label, "(%s) show\r\n", range_unit);
                    print_string(label);
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
void print_report(void) {
    char buffer[100];
    int i;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char *type_name;
    float min, max, sum, val;
    float samples_per_second;
    char rate_str[50];
    
    clrscr();
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
                sample_rate_labels[g_control_panel.selected_rate]);
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
    
    if (g_system->data_count > 0) {
        min = g_system->data_buffer[0];
        max = g_system->data_buffer[0];
        sum = 0;
        
        for (i = 0; i < g_system->data_count; i++) {
            val = g_system->data_buffer[i];
            if (val < min) min = val;
            if (val > max) max = val;
            sum += val;
        }
        
        print_string("\r\nMeasurement Statistics:\r\n");
        print_string("----------------------\r\n");
        sprintf(buffer, "Samples: %u\r\n", g_system->data_count);
        print_string(buffer);
		 if (g_control_panel.sample_rate_ms > 0 && g_system->data_count > 1) {
            float duration_ms = (g_system->data_count - 1) * g_control_panel.sample_rate_ms;
            if (duration_ms < 60000) {
                sprintf(buffer, "Duration: %.1f seconds\r\n", duration_ms / 1000.0);
            } else {
                sprintf(buffer, "Duration: %.1f minutes\r\n", duration_ms / 60000.0);
            }
            print_string(buffer);
        }
        sprintf(buffer, "Minimum: %.6f\r\n", min);
        print_string(buffer);
        sprintf(buffer, "Maximum: %.6f\r\n", max);
        print_string(buffer);
        sprintf(buffer, "Average: %.6f\r\n", sum / g_system->data_count);
        print_string(buffer);
    }
    
    lpt_send_byte(0x0C);
    
    printf("Report printed successfully.\n");
    printf("Press any key to continue...");
    getch();
}

void graph_config_menu(void) {
    int done = 0;
    int choice;
    char input[20];
    int i;
    static int sample_start = 0;      /* Start sample index */
    static int sample_count = 0;      /* 0 = all samples */
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        printf("=====================================\n");
        printf("    GRAPH DISPLAY CONFIGURATION\n");
        printf("=====================================\n\n");
        
        printf("1. Display Range Settings\n");
        printf("2. Sample Range Selection\n");
        printf("3. Select Traces to Display\n");
        printf("4. Keyboard Shortcuts Help\n");
        printf("5. Display Options\n");
        printf("6. Return to Graph\n\n");
        
        printf("Current Settings:\n");
        printf("-----------------\n");
        printf("Y-Range: %.3f%s to %.3f%s\n", 
               g_graph_scale.min_value * 1000.0, "mV",
               g_graph_scale.max_value * 1000.0, "mV");
        printf("Auto-scale: %s\n", g_graph_scale.auto_scale ? "ON" : "OFF");
        
        if (sample_count == 0) {
            printf("Sample Range: ALL samples\n");
        } else {
            printf("Sample Range: %d to %d (%d samples)\n", 
                   sample_start + 1, sample_start + sample_count, sample_count);
        }
        
        printf("Active Modules: ");
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                printf("%d ", i);
            }
        }
        printf("\n\nSelect option (1-6): ");
        
        choice = getch();
        
        switch (choice) {
            case '1':  /* Range settings */
                clrscr();
                printf("Y-AXIS RANGE SETTINGS\n");
                printf("---------------------\n\n");
                printf("Current: %.3fmV to %.3fmV\n\n", 
                       g_graph_scale.min_value * 1000.0,
                       g_graph_scale.max_value * 1000.0);
                printf("Set Min Value (mV): ");
                if (scanf("%s", input) == 1) {
                    g_graph_scale.min_value = atof(input) / 1000.0;
                    g_graph_scale.auto_scale = 0;
                }
                printf("Set Max Value (mV): ");
                if (scanf("%s", input) == 1) {
                    g_graph_scale.max_value = atof(input) / 1000.0;
                    g_graph_scale.auto_scale = 0;
                }
                break;
                
            case '2':  /* Sample range selection */
                clrscr();
                printf("SAMPLE RANGE SELECTION\n");
                printf("----------------------\n\n");
                printf("A - Display ALL samples\n");
                printf("C - Custom range (1-1000)\n\n");
                printf("Select option: ");
                
                choice = getch();
                if (choice == 'A' || choice == 'a') {
                    sample_start = 0;
                    sample_count = 0;  /* 0 means all */
                    printf("\nSet to display ALL samples\n");
                } else if (choice == 'C' || choice == 'c') {
                    printf("\n\nEnter starting sample (1-1000): ");
                    if (scanf("%s", input) == 1) {
                        sample_start = atoi(input) - 1;  /* Convert to 0-based */
                        if (sample_start < 0) sample_start = 0;
                        if (sample_start >= 1000) sample_start = 999;
                    }
                    printf("Enter number of samples to display (1-1000): ");
                    if (scanf("%s", input) == 1) {
                        sample_count = atoi(input);
                        if (sample_count < 1) sample_count = 1;
                        if (sample_count > 1000) sample_count = 1000;
                        if (sample_start + sample_count > 1000) {
                            sample_count = 1000 - sample_start;
                        }
                    }
                    printf("\nSet to display samples %d to %d\n", 
                           sample_start + 1, sample_start + sample_count);
                }
                printf("\nPress any key...");
                getch();
                break;
                
            case '3':  /* Display trace selection */
                display_trace_selection_menu();
                break;
                
            case '4':  /* Help */
                clrscr();
                printf("GRAPH KEYBOARD SHORTCUTS\n");
                printf("========================\n\n");
                printf("Navigation:\n");
                printf("  Arrow Keys   - Move cursor through samples\n");
                printf("  0-7         - Select trace/module\n");
                printf("  ALT+0-7   - Toggle trace visibility\n\n");
                printf("Display Control:\n");
                printf("  A           - Auto-scale\n");
                printf("  +/-         - Zoom in/out\n");
                printf("  R           - Redraw screen\n");
                printf("  M           - Toggle mouse mode\n\n");
                printf("Functions:\n");
                printf("  P           - Print graph\n");
                printf("  C           - Clear all data\n");
                printf("  H           - This help/config menu\n");
                printf("  ESC         - Exit graph display\n\n");
                printf("Status Indicators:\n");
                printf("  287         - Math coprocessor active\n");
                printf("  1K          - 1000 samples loaded\n");
                printf("  Range       - Showing sample subset\n\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '5':  /* Display options */
                clrscr();
                printf("DISPLAY OPTIONS\n");
                printf("---------------\n\n");
                printf("Grid divisions: %d\n", g_graph_scale.grid_divisions);
                printf("Enter new value (5-20): ");
                if (scanf("%s", input) == 1) {
                    int divs = atoi(input);
                    if (divs >= 5 && divs <= 20) {
                        g_graph_scale.grid_divisions = divs;
                    }
                }
                break;
                
            case '6':  /* Exit */
            case 27:   /* ESC */
                done = 1;
                break;
        }
    }
    
    g_graph_scale.sample_start = sample_start;
    g_graph_scale.sample_count = sample_count;
}

void save_data(void) {
    FILE *fp;
    char filename[80];
    int i, j;
    int total_module_samples = 0;
    
    clrscr();
    printf("Save Data\n");
    printf("=========\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".tm5");
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot create file\n");
        getch();
        return;
    }
    
    fprintf(fp, "TM5000 Data File v2.0\n");
    fprintf(fp, "FileFormat: ModuleData\n");  /* New format indicator */
    
    fprintf(fp, "GlobalSamples: %u\n", g_system->data_count);
    fprintf(fp, "SampleRateMs: %d\n", g_control_panel.sample_rate_ms);
    fprintf(fp, "RateType: %s\n", g_control_panel.use_custom ? "Custom" : "Preset");
    if (!g_control_panel.use_custom) {
        fprintf(fp, "PresetIndex: %d\n", g_control_panel.selected_rate);
    }
    
    fprintf(fp, "Modules:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            fprintf(fp, "%d,%d,%d,%s,%u\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description,
                    g_system->modules[i].module_data_count);
            total_module_samples += g_system->modules[i].module_data_count;
        }
    }
    
    fprintf(fp, "GlobalData:\n");
    for (i = 0; i < g_system->data_count; i++) {
        fprintf(fp, "%.6e\n", g_system->data_buffer[i]);
    }
    
    fprintf(fp, "ModuleData:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && 
            g_system->modules[i].module_data &&
            g_system->modules[i].module_data_count > 0) {
            
            fprintf(fp, "Slot%d:%u\n", i, g_system->modules[i].module_data_count);
            
            for (j = 0; j < g_system->modules[i].module_data_count; j++) {
                fprintf(fp, "%.6e\n", g_system->modules[i].module_data[j]);
            }
        }
    }
    
    fprintf(fp, "EndOfFile\n");
    fclose(fp);
    
    printf("\nData saved successfully!\n");
    printf("File: %s\n", filename);
    printf("Global samples: %u\n", g_system->data_count);
    printf("Total module samples: %d\n", total_module_samples);
    printf("\nPress any key to continue...");
    getch();
}

void load_data(void) {
    FILE *fp;
    char filename[80];
    char line[100];
    char desc[20];
    int slot, type, addr;
    long file_pos;  
    unsigned int count;
    int i, j;
    int is_new_format = 0;
    unsigned int module_count;
    float value;
    int total_loaded = 0;
    
    clrscr();
    printf("Load Data\n");
    printf("=========\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".tm5");
    
    fp = fopen(filename, "r");
    if (!fp) {
        filename[strlen(filename)-4] = '\0';
        fp = fopen(filename, "r");
        if (!fp) {
            printf("Error: Cannot open file '%s'\n", filename);
            getch();
            return;
        }
    }
    
    fgets(line, sizeof(line), fp);
    
    if (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "FileFormat: ModuleData")) {
            is_new_format = 1;
        } else {
            rewind(fp);
            fgets(line, sizeof(line), fp);  /* Skip header again */
        }
    }
    
    for (i = 0; i < 10; i++) {
        clear_module_data(i);
        g_system->modules[i].enabled = 0;
    }
    g_system->data_count = 0;
    
    if (is_new_format) {
        printf("Loading enhanced format file...\n");
        
        fscanf(fp, "GlobalSamples: %u\n", &g_system->data_count);
        
        fscanf(fp, "SampleRateMs: %d\n", &g_control_panel.sample_rate_ms);
        fgets(line, sizeof(line), fp);  /* RateType line */
        if (strstr(line, "Custom")) {
            g_control_panel.use_custom = 1;
            sprintf(g_control_panel.custom_rate, "%d", g_control_panel.sample_rate_ms);
        } else {
            g_control_panel.use_custom = 0;
            fscanf(fp, "PresetIndex: %d\n", &g_control_panel.selected_rate);
        }
        
        fgets(line, sizeof(line), fp);  /* "Modules:" */
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "GlobalData:", 11) == 0) break;
            
            if (sscanf(line, "%d,%d,%d,%[^,],%u", &slot, &type, &addr, desc, &count) == 5) {
                if (slot >= 0 && slot < 10) {
                    g_system->modules[slot].module_type = type;
                    g_system->modules[slot].gpib_address = addr;
                    g_system->modules[slot].enabled = 1;
                    strcpy(g_system->modules[slot].description, desc);
                    
                    if (count > 0) {
                        allocate_module_buffer(slot, count + 100);  /* Extra space */
                    }
                }
            }
        }
        
        for (i = 0; i < g_system->data_count && i < g_system->buffer_size; i++) {
            if (fscanf(fp, "%f", &g_system->data_buffer[i]) != 1) break;
        }
        
        fgets(line, sizeof(line), fp);  /* Skip to ModuleData: */
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "EndOfFile", 9) == 0) break;
            
            if (sscanf(line, "Slot%d:%u", &slot, &module_count) == 2) {
                if (slot >= 0 && slot < 10 && g_system->modules[slot].enabled) {
                    printf("Loading %u samples for slot %d...\n", module_count, slot);
                    
                    for (j = 0; j < module_count; j++) {
                        if (fscanf(fp, "%f", &value) == 1) {
                            store_module_data(slot, value);
                            total_loaded++;
                        } else {
                            break;
                        }
                    }
                    
                    if (g_system->modules[slot].module_data_count > 0) {
                        g_system->modules[slot].last_reading = 
                            g_system->modules[slot].module_data[
                                g_system->modules[slot].module_data_count - 1];
                    }
                }
            }
        }
    } else {
        printf("Loading legacy format file...\n");
        
        fscanf(fp, "Samples: %u\n", &g_system->data_count);
        
        file_pos = ftell(fp);
        if (fscanf(fp, "SampleRateMs: %d\n", &g_control_panel.sample_rate_ms) != 1) {
            fseek(fp, file_pos, SEEK_SET);
            g_control_panel.sample_rate_ms = 500;  /* Default */
        }
        
        fgets(line, sizeof(line), fp);  /* Skip "Modules:" */
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Data:", 5) == 0) break;
            
            if (sscanf(line, "%d,%d,%d,%[^\n]", &slot, &type, &addr, desc) >= 3) {
                g_system->modules[slot].module_type = type;
                g_system->modules[slot].gpib_address = addr;
                g_system->modules[slot].enabled = 1;
                if (strlen(desc) > 0) {
                    strcpy(g_system->modules[slot].description, desc);
                }
            }
        }
        
        g_system->data_count = 0;
        while (fscanf(fp, "%f", &g_system->data_buffer[g_system->data_count]) == 1) {
            g_system->data_count++;
            if (g_system->data_count >= g_system->buffer_size) break;
        }
        
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                allocate_module_buffer(i, g_system->data_count);
                for (j = 0; j < g_system->data_count; j++) {
                    store_module_data(i, g_system->data_buffer[j]);
                }
                printf("Copied %u samples to slot %d for compatibility\n", 
                       g_system->data_count, i);
                break;
            }
        }
    }
    
    fclose(fp);
    
    printf("\nData loaded successfully!\n");
    printf("File format: %s\n", is_new_format ? "Enhanced (v2.0)" : "Legacy");
    printf("Global samples: %u\n", g_system->data_count);
    printf("Sample rate: %d ms\n", g_control_panel.sample_rate_ms);
    printf("\nModule data loaded:\n");
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && g_system->modules[i].module_data_count > 0) {
            printf("  Slot %d: %u samples\n", i, g_system->modules[i].module_data_count);
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
    
    sync_traces_with_modules();
}

void sync_traces_with_modules(void) {
    int i;
    int user_enabled[8];  /* Save user trace visibility preferences */
    int first_sync = 1;   /* Check if this is the first sync */
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].data != NULL) {
            first_sync = 0;
            break;
        }
    }
    
    if (!first_sync) {
        for (i = 0; i < 10; i++) {
            user_enabled[i] = g_traces[i].enabled;
        }
    }
    
    for (i = 0; i < 10; i++) {
        g_traces[i].data = NULL;
        g_traces[i].data_count = 0;
        if (first_sync) {
            g_traces[i].enabled = 0;
        }
    }
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            g_traces[i].slot = i;
            g_traces[i].color = get_module_color(g_system->modules[i].module_type);
            strcpy(g_traces[i].label, g_system->modules[i].description);
            
            if (g_system->modules[i].module_data && 
                g_system->modules[i].module_data_count > 0) {
                g_traces[i].data = g_system->modules[i].module_data;
                g_traces[i].data_count = g_system->modules[i].module_data_count;
                
                if (first_sync) {
                    g_traces[i].enabled = 1;
                } else {
                    g_traces[i].enabled = user_enabled[i];
                }
            } else {
                g_traces[i].enabled = 0;
            }
        } else {
            g_traces[i].enabled = 0;
        }
    }
}

void save_settings(void) {
    FILE *fp;
    char filename[80];
    int i;
    
    clrscr();
    printf("Save Configuration Settings\n");
    printf("===========================\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".cfg");
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot create file\n");
        getch();
        return;
    }
    
    fprintf(fp, "TM5000 Configuration File v1.0\n");
    
    fprintf(fp, "[ControlPanel]\n");
    fprintf(fp, "SampleRateMs=%d\n", g_control_panel.sample_rate_ms);
    fprintf(fp, "UseCustom=%d\n", g_control_panel.use_custom);
    fprintf(fp, "SelectedRate=%d\n", g_control_panel.selected_rate);
    fprintf(fp, "CustomRate=%s\n", g_control_panel.custom_rate);
    fprintf(fp, "MonitorMask=%d\n", g_control_panel.monitor_mask);
    fprintf(fp, "MonitorAll=%d\n", g_control_panel.monitor_all);
    
    fprintf(fp, "\n[Modules]\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            fprintf(fp, "Module%d=%d,%d,%s\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description);
        }
    }
    
    fprintf(fp, "\n[GraphScale]\n");
    fprintf(fp, "MinValue=%f\n", g_graph_scale.min_value);
    fprintf(fp, "MaxValue=%f\n", g_graph_scale.max_value);
    fprintf(fp, "AutoScale=%d\n", g_graph_scale.auto_scale);
    fprintf(fp, "ZoomFactor=%f\n", g_graph_scale.zoom_factor);
    
    fclose(fp);
    
    printf("\nSettings saved to %s\n", filename);
    printf("Press any key to continue...");
    getch();
}

void load_settings(void) {
    FILE *fp;
    char filename[80];
    char line[256];
    char key[50], value[200];
    int slot, type, addr;
    char desc[50];
    
    clrscr();
    printf("Load Configuration Settings\n");
    printf("===========================\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".cfg");
    
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file '%s'\n", filename);
        getch();
        return;
    }
    
    fgets(line, sizeof(line), fp);
    if (!strstr(line, "TM5000 Configuration")) {
        printf("Error: Invalid configuration file\n");
        fclose(fp);
        getch();
        return;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        if (strcmp(line, "[ControlPanel]") == 0) {
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    fseek(fp, -strlen(line), SEEK_CUR);
                    break;
                }
                if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                    if (strcmp(key, "SampleRateMs") == 0) {
                        g_control_panel.sample_rate_ms = atoi(value);
                    } else if (strcmp(key, "UseCustom") == 0) {
                        g_control_panel.use_custom = atoi(value);
                    } else if (strcmp(key, "SelectedRate") == 0) {
                        g_control_panel.selected_rate = atoi(value);
                    } else if (strcmp(key, "CustomRate") == 0) {
                        strcpy(g_control_panel.custom_rate, value);
                    } else if (strcmp(key, "MonitorMask") == 0) {
                        g_control_panel.monitor_mask = atoi(value);
                    } else if (strcmp(key, "MonitorAll") == 0) {
                        g_control_panel.monitor_all = atoi(value);
                    }
                }
            }
        } else if (strcmp(line, "[Modules]") == 0) {
            for (slot = 0; slot < 10; slot++) {
                g_system->modules[slot].enabled = 0;
            }
            
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    fseek(fp, -strlen(line), SEEK_CUR);
                    break;
                }
                if (sscanf(line, "Module%d=%d,%d,%[^\n]", &slot, &type, &addr, desc) == 4) {
                    if (slot >= 0 && slot < 10) {
                        g_system->modules[slot].module_type = type;
                        g_system->modules[slot].gpib_address = addr;
                        g_system->modules[slot].enabled = 1;
                        strcpy(g_system->modules[slot].description, desc);
                        g_system->modules[slot].slot_number = slot;
                    }
                }
            }
        } else if (strcmp(line, "[GraphScale]") == 0) {
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') break;
                if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                    if (strcmp(key, "MinValue") == 0) {
                        g_graph_scale.min_value = atof(value);
                    } else if (strcmp(key, "MaxValue") == 0) {
                        g_graph_scale.max_value = atof(value);
                    } else if (strcmp(key, "AutoScale") == 0) {
                        g_graph_scale.auto_scale = atoi(value);
                    } else if (strcmp(key, "ZoomFactor") == 0) {
                        g_graph_scale.zoom_factor = atof(value);
                    }
                }
            }
        }
    }
    
    fclose(fp);
    
    printf("\nSettings loaded successfully!\n");
    printf("Sample rate: %d ms\n", g_control_panel.sample_rate_ms);
    printf("\nModules configured:\n");
    for (slot = 0; slot < 10; slot++) {
        if (g_system->modules[slot].enabled) {
            printf("  Slot %d: %s (GPIB %d)\n", 
                   slot, 
                   g_system->modules[slot].description,
                   g_system->modules[slot].gpib_address);
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
}


void display_error(char *msg) {
    strcpy(g_error_msg, msg);
    gotoxy(1, 24);
    printf("Error: %s", msg);
}

void send_custom_command(void) {
    int slot, i;
    char command[256];
    char response[256];
    int found = 0;
    int done = 0;
    
    clrscr();
    printf("Send Custom GPIB Command\n");
    printf("========================\n\n");
    
    printf("Active modules:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            printf("  Slot %d: %s (GPIB %d)\n", 
                   i, g_system->modules[i].description,
                   g_system->modules[i].gpib_address);
            found = 1;
        }
    }
    
    if (!found) {
        printf("\nNo modules configured!\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    printf("\nEnter slot number (0-7, or 9 to exit): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7) {
        return;
    }
    
    if (!g_system->modules[slot].enabled) {
        printf("Slot %d is not configured!\n", slot);
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    printf("\nSending commands to %s at GPIB address %d\n", 
           g_system->modules[slot].description,
           g_system->modules[slot].gpib_address);
    printf("Type 'EXIT' to return to main menu\n");
    printf("\nCommon test commands:\n");
    printf("  DM5120:  voltage?, X, INIT, FUNCT?, RANGE?\n");
    printf("           FUNCT DCV, RANGE AUTO, SEND, READ?\n");
    printf("  PS5004:  ID?, SET?, VOLTAGE?, CURRENT?, REGULATION?\n");
    printf("           VOLTAGE 5.0, CURRENT 0.1, OUTPUT ON/OFF\n");
    printf("  General: *IDN?, *RST, *CLS, ERROR?\n");
    printf("----------------------------------------\n\n");
    
    while (getchar() != '\n');
    
    while (!done) {
        printf("\nCommand> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }
        
        command[strcspn(command, "\r\n")] = 0;
        
        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }
        
        if (strlen(command) == 0) {
            continue;
        }
        
        printf("Sending: '%s'\n", command);
        
        ieee_write("fill off\r\n");
        delay(50);
        
        gpib_write(g_system->modules[slot].gpib_address, command);
        
        if (strchr(command, '?') != NULL || 
            strcasecmp(command, "X") == 0 ||
            strcasecmp(command, "SEND") == 0 ||
            strncasecmp(command, "F", 1) == 0) {
            
            printf("Reading response...\n");
            delay(200);
            
            memset(response, 0, sizeof(response));
            
            gpib_read(g_system->modules[slot].gpib_address, response, sizeof(response));
            
            if (strlen(response) > 0) {
                printf("Response: '%s'\n", response);
                
                printf("Hex dump: ");
                for (i = 0; i < strlen(response) && i < 32; i++) {
                    printf("%02X ", (unsigned char)response[i]);
                }
                printf("\n");
            } else {
                printf("No response received\n");
            }
        } else {
            printf("Command sent (no response expected)\n");
        }
        
        check_gpib_error();
        if (gpib_error) {
            printf("GPIB Error: %s\n", g_error_msg);
            gpib_error = 0;
        }
    }
}

void test_dm5120_comm_debug(int address) {
    char buffer[256];
    char cmd[256];
    int bytes_read;
    int i;
    
    printf("\nDEBUG: Testing DM5120 at address %d\n", address);
    printf("Trying to match KYBDCTRL exactly...\n\n");
    
    printf("Step 1: REMOTE %d\n", address);
    sprintf(cmd, "remote %d\r\n", address);
    ieee_write(cmd);
    delay(200);
    
    printf("\nMethod 1: With space after semicolon\n");
    printf("Sending: OUTPUT %d; voltage?\n", address);
    sprintf(cmd, "output %d; voltage?\r\n", address);
    ieee_write(cmd);
    delay(300);
    
    printf("Sending: ENTER %d\n", address);
    sprintf(cmd, "enter %d\r\n", address);
    ieee_write(cmd);
    
    bytes_read = ieee_read(buffer, 255);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Response (%d bytes): '%s'\n", bytes_read, buffer);
        
        printf("Hex: ");
        for (i = 0; i < bytes_read && i < 20; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
    } else {
        printf("No response\n");
    }
    
    printf("\nMethod 2: No space after semicolon\n");
    printf("Sending: OUTPUT %d;voltage?\n", address);
    sprintf(cmd, "output %d;voltage?\r\n", address);
    ieee_write(cmd);
    delay(300);
    
    printf("Sending: ENTER %d\n", address);
    sprintf(cmd, "enter %d\r\n", address);
    ieee_write(cmd);
    
    bytes_read = ieee_read(buffer, 255);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Response (%d bytes): '%s'\n", bytes_read, buffer);
    } else {
        printf("No response\n");
    }
    
    printf("\nChecking GPIB error status:\n");
    ieee_write("status\r\n");
    delay(50);
    ieee_read(buffer, sizeof(buffer));
    printf("Status: %s\n", buffer);
    
    printf("\nPress any key to continue...");
    getch();
}

void gpib_write_dm5010(int address, char *command) {
    char cmd_buffer[256];
    
    gpib_check_srq(address);
    delay(50);
    
    sprintf(cmd_buffer, "output %d; %s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    delay(100);
}

int gpib_read_dm5010(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    bytes_read = ieee_read(buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

int gpib_read_float_dm5010(int address, float *value) {
    char buffer[256];
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
        if (sscanf(buffer, "%eV", value) == 1) return 1;
        if (sscanf(buffer, "%eA", value) == 1) return 1;
        if (sscanf(buffer, "%eR", value) == 1) return 1;
    }
    
    return 0;
}

void dm5010_set_function(int address, char *function) {
    char cmd[50];
    sprintf(cmd, "CONF:%s", function);
    gpib_write_dm5010(address, cmd);
}

void dm5010_set_range(int address, char *function, float range) {
    char cmd[100];
    if (range == 0.0) {
        sprintf(cmd, "CONF:%s AUTO", function);
    } else {
        sprintf(cmd, "CONF:%s %e", function, range);
    }
    gpib_write_dm5010(address, cmd);
}

void dm5010_set_filter(int address, int enabled, int count) {
    char cmd[50];
    if (enabled) {
        sprintf(cmd, "AVER:STAT ON");
        gpib_write_dm5010(address, cmd);
        sprintf(cmd, "AVER:COUN %d", count);
        gpib_write_dm5010(address, cmd);
    } else {
        sprintf(cmd, "AVER:STAT OFF");
        gpib_write_dm5010(address, cmd);
    }
}

void dm5010_set_trigger(int address, char *mode) {
    char cmd[50];
    sprintf(cmd, "TRIG:SOUR %s", mode);
    gpib_write_dm5010(address, cmd);
}

void dm5010_set_autozero(int address, int enabled) {
    char cmd[50];
    sprintf(cmd, "ZERO:AUTO %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}

void dm5010_set_null(int address, int enabled, float value) {
    char cmd[50];
    if (enabled) {
        sprintf(cmd, "CALC:NULL:OFFS %e", value);
        gpib_write_dm5010(address, cmd);
        sprintf(cmd, "CALC:NULL:STAT ON");
        gpib_write_dm5010(address, cmd);
    } else {
        sprintf(cmd, "CALC:NULL:STAT OFF");
        gpib_write_dm5010(address, cmd);
    }
}

void dm5010_set_calculation(int address, int mode, float factor, float offset) {
    char cmd[100];
    switch(mode) {
        case 1: /* Average */
            sprintf(cmd, "CALC:AVER:STAT ON");
            break;
        case 2: /* Scale */
            sprintf(cmd, "CALC:SCAL:GAIN %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(cmd, "CALC:SCAL:OFFS %e", offset);
            gpib_write_dm5010(address, cmd);
            sprintf(cmd, "CALC:SCAL:STAT ON");
            break;
        case 3: /* dBm */
            sprintf(cmd, "CALC:DBM:REF %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(cmd, "CALC:DBM:STAT ON");
            break;
        case 4: /* dBr */
            sprintf(cmd, "CALC:DB:REF %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(cmd, "CALC:DB:STAT ON");
            break;
        default: /* None */
            sprintf(cmd, "CALC:STAT OFF");
            break;
    }
    gpib_write_dm5010(address, cmd);
}

void dm5010_beeper(int address, int enabled) {
    char cmd[50];
    sprintf(cmd, "SYST:BEEP:STAT %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}

void dm5010_lock_front_panel(int address, int locked) {
    char cmd[50];
    sprintf(cmd, "SYST:LOCK %s", locked ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}

void test_dm5010_comm(int address) {
    char buffer[256];
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DM5010 Communication Test ===\n");
    printf("Testing DM5010 at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write_dm5010(address, "*IDN?");
    delay(100);
    if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
        printf("   ID: %s\n", buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing reset...\n");
    gpib_write_dm5010(address, "*RST");
    delay(500);
    gpib_write_dm5010(address, "CONF:VOLT:DC");
    delay(100);
    printf("   Reset and configured for DC voltage\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function configuration...\n");
    gpib_write_dm5010(address, "CONF:VOLT:DC AUTO,MAX");
    delay(100);
    gpib_write_dm5010(address, "FUNC?");
    delay(100);
    if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
        printf("   Current function: %s\n", buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing measurement...\n");
    gpib_write_dm5010(address, "READ?");
    delay(200);
    if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", &value) == 1) {
            printf("   Measurement: %.6e V\n", value);
            success_count++;
        } else {
            printf("   Measurement parse failed: %s\n", buffer);
        }
    } else {
        printf("   Measurement failed!\n");
    }
    test_count++;
    
    printf("5. Testing status...\n");
    gpib_write_dm5010(address, "*STB?");
    delay(100);
    if (gpib_read_dm5010(address, buffer, sizeof(buffer)) > 0) {
        printf("   Status byte: %s\n", buffer);
        success_count++;
    } else {
        printf("   Status query failed!\n");
    }
    test_count++;
    
    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/%d\n", success_count, test_count);
    if (success_count == test_count) {
        printf("DM5010 communication: EXCELLENT\n");
    } else if (success_count >= test_count * 0.8) {
        printf("DM5010 communication: GOOD\n");
    } else if (success_count >= test_count * 0.5) {
        printf("DM5010 communication: MARGINAL\n");
    } else {
        printf("DM5010 communication: POOR - Check connections!\n");
    }
    
    printf("\nPress any key to continue...");
    getch();
}

void configure_dm5010_advanced(int slot) {
    int choice, done = 0;
    char input[50];
    
    while (!done) {
        clrscr();
        printf("DM5010 Advanced Configuration - Slot %d\n", slot);
        printf("=====================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Function: %s\n", g_dm5010_config[slot].function);
        printf("  Range Mode: %s\n", g_dm5010_config[slot].range_mode == 0 ? "AUTO" : "MANUAL");
        printf("  Filter: %s (%d count)\n", 
               g_dm5010_config[slot].filter_enabled ? "ON" : "OFF",
               g_dm5010_config[slot].filter_count);
        printf("  Trigger: %s\n", 
               g_dm5010_config[slot].trigger_mode == 0 ? "FREE" : 
               g_dm5010_config[slot].trigger_mode == 1 ? "SINGLE" : "EXT");
        printf("  Digits: %d\n", g_dm5010_config[slot].digits);
        printf("  Auto Zero: %s\n", g_dm5010_config[slot].auto_zero ? "ON" : "OFF");
        printf("  Calculation: %s\n",
               g_dm5010_config[slot].calculation_mode == 0 ? "NONE" :
               g_dm5010_config[slot].calculation_mode == 1 ? "AVERAGE" :
               g_dm5010_config[slot].calculation_mode == 2 ? "SCALE" :
               g_dm5010_config[slot].calculation_mode == 3 ? "dBm" : "dBr");
        printf("  Statistics: %s\n", g_dm5010_config[slot].statistics_enabled ? "ON" : "OFF");
        
        printf("\nOptions:\n");
        printf("1. Set Function (DCV/ACV/OHMS/CURR/DIODE)\n");
        printf("2. Set Range Mode\n");
        printf("3. Configure Filter\n");
        printf("4. Set Trigger Mode\n");
        printf("5. Set Display Digits\n");
        printf("6. Configure Auto Zero\n");
        printf("7. Set Calculation Mode\n");
        printf("8. Configure Statistics\n");
        printf("9. Reset to Defaults\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        scanf("%d", &choice);
        
        switch(choice) {
            case 1:
                printf("Enter function (DCV/ACV/OHMS/CURR/DIODE): ");
                scanf("%s", input);
                strcpy(g_dm5010_config[slot].function, input);
                dm5010_set_function(g_system->modules[slot].gpib_address, input);
                break;
                
            case 2:
                printf("Range mode (0=AUTO, 1=MANUAL): ");
                scanf("%d", &g_dm5010_config[slot].range_mode);
                break;
                
            case 3:
                printf("Enable filter (0=OFF, 1=ON): ");
                scanf("%d", &g_dm5010_config[slot].filter_enabled);
                if (g_dm5010_config[slot].filter_enabled) {
                    printf("Filter count (1-100): ");
                    scanf("%d", &g_dm5010_config[slot].filter_count);
                }
                dm5010_set_filter(g_system->modules[slot].gpib_address,
                                 g_dm5010_config[slot].filter_enabled,
                                 g_dm5010_config[slot].filter_count);
                break;
                
            case 4:
                printf("Trigger mode (0=FREE, 1=SINGLE, 2=EXT): ");
                scanf("%d", &g_dm5010_config[slot].trigger_mode);
                if (g_dm5010_config[slot].trigger_mode == 0) {
                    dm5010_set_trigger(g_system->modules[slot].gpib_address, "IMM");
                } else if (g_dm5010_config[slot].trigger_mode == 1) {
                    dm5010_set_trigger(g_system->modules[slot].gpib_address, "BUS");
                } else {
                    dm5010_set_trigger(g_system->modules[slot].gpib_address, "EXT");
                }
                break;
                
            case 5:
                printf("Display digits (4-5): ");
                scanf("%d", &g_dm5010_config[slot].digits);
                break;
                
            case 6:
                printf("Auto zero (0=OFF, 1=ON): ");
                scanf("%d", &g_dm5010_config[slot].auto_zero);
                dm5010_set_autozero(g_system->modules[slot].gpib_address,
                                   g_dm5010_config[slot].auto_zero);
                break;
                
            case 7:
                printf("Calculation mode (0=NONE, 1=AVG, 2=SCALE, 3=dBm, 4=dBr): ");
                scanf("%d", &g_dm5010_config[slot].calculation_mode);
                if (g_dm5010_config[slot].calculation_mode == 2) {
                    printf("Scale factor: ");
                    scanf("%f", &g_dm5010_config[slot].scale_factor);
                    printf("Scale offset: ");
                    scanf("%f", &g_dm5010_config[slot].scale_offset);
                }
                dm5010_set_calculation(g_system->modules[slot].gpib_address,
                                      g_dm5010_config[slot].calculation_mode,
                                      g_dm5010_config[slot].scale_factor,
                                      g_dm5010_config[slot].scale_offset);
                break;
                
            case 8:
                printf("Enable statistics (0=OFF, 1=ON): ");
                scanf("%d", &g_dm5010_config[slot].statistics_enabled);
                if (g_dm5010_config[slot].statistics_enabled) {
                    g_dm5010_config[slot].min_value = 1e30;
                    g_dm5010_config[slot].max_value = -1e30;
                }
                break;
                
            case 9:
                init_dm5010_config(slot);
                printf("Configuration reset to defaults.\n");
                getch();
                break;
                
            case 0:
                done = 1;
                break;
                
            default:
                printf("Invalid choice!\n");
                getch();
                break;
        }
    }
}

void gpib_terminal_mode(void) {
    char command[256];
    char response[256];
    int done = 0;
    int bytes_read;
    int i;
    union REGS regs;
    struct SREGS segs;
    
    clrscr();
    printf("GPIB Terminal Mode - Personal488\n");
    printf("=================================\n\n");
    printf("Enter commands EXACTLY as in KYBDCTRL\n");
    printf("Examples:\n");
    printf("  REMOTE 16\n");
    printf("  OUTPUT 16; voltage?\n");
    printf("  ENTER 16\n");
    printf("  STATUS\n");
    printf("Type 'EXIT' to return\n\n");
    printf("Note: BREAK is sent via IOCTL, not as a command\n\n");
    
    drain_input_buffer();
    
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Initial Status: %s\n", response);
    }
    
    while (getchar() != '\n');
    
    while (!done) {
        printf("\nGPIB> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }
        
        command[strcspn(command, "\r\n")] = 0;
        
        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }
        
        if (strlen(command) == 0) {
            continue;
        }
        
        if (strncasecmp(command, "BREAK", 5) == 0) {
            printf("Sending BREAK via IOCTL...\n");
            
            strcpy(response, "BREAK");
            regs.x.ax = 0x4403;     /* IOCTL write */
            regs.x.bx = ieee_out;   /* File handle */
            regs.x.cx = 5;          /* Length of "BREAK" */
            regs.x.dx = (unsigned)response;
            segread(&segs);         /* Get current segment registers */
            
            intdosx(&regs, &regs, &segs);
            
            if (regs.x.cflag) {
                printf("IOCTL BREAK failed, error: %d\n", regs.x.ax);
            } else {
                printf("IOCTL BREAK succeeded\n");
            }
            delay(200);
            continue;
        }
        
        printf("Sending: '%s\\r\\n'\n", command);
        strcat(command, "\r\n");
        ieee_write(command);
        
        if (command_has_response(command)) {
            printf("Reading response...\n");
            delay(100);
            
            bytes_read = ieee_read(response, sizeof(response)-1);
            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("Response (%d bytes): '%s'\n", bytes_read, response);
                
                if (strncasecmp(command, "ENTER", 5) == 0) {
                    printf("Hex: ");
                    for (i = 0; i < bytes_read && i < 32; i++) {
                        printf("%02X ", (unsigned char)response[i]);
                        if (response[i] == '\r') printf("[CR] ");
                        if (response[i] == '\n') printf("[LF] ");
                    }
                    printf("\n");
                }
            } else {
                printf("No response received (may indicate error)\n");
            }
        } else {
            delay(50);
            
            bytes_read = read(ieee_in, response, sizeof(response)-1);
            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("Unexpected response (possible error): %s", response);
            }
        }
        
        delay(50);
    }
}

measurement_system* init_measurement_system(unsigned int points) {
    measurement_system *sys = (measurement_system *)malloc(sizeof(measurement_system));
    int i;
    
    if (!sys) return NULL;
    
    memset(sys, 0, sizeof(measurement_system));
    sys->buffer_size = points;
    sys->data_buffer = (float far *)_fmalloc(points * sizeof(float));
    
    if (!sys->data_buffer) {
        free(sys);
        return NULL;
    }
    
    sys->data_count = 0;
    sys->sample_rate = 10;
    
    for (i = 0; i < 10; i++) {
        sys->modules[i].enabled = 0;
        sys->modules[i].module_type = MOD_NONE;
        sys->modules[i].slot_number = i;
        sys->modules[i].gpib_address = 0;
        sys->modules[i].last_reading = 0.0;
        sys->modules[i].description[0] = '\0';
        sys->gpib_devices[i] = -1;
        sys->modules[i].module_data = NULL;
        sys->modules[i].module_data_count = 0;
        sys->modules[i].module_data_size = 0;
    }
    
    return sys;
}

int init_gpib_system(void) {
    int i, bytes_read;
    char response[256];
    
    printf("Opening IEEE device handles...\n");
    
    ieee_out = open("\\dev\\ieeeout", O_WRONLY | O_BINARY);
    if (ieee_out < 0) {
        printf("Cannot open \\DEV\\IEEEOUT\n");
        return -1;
    }
    
    printf("Waiting before opening input device...\n");
    delay(400);  /* 400ms delay to let driver settle */
    
    ieee_in = open("\\dev\\ieeein", O_RDONLY | O_BINARY);
    if (ieee_in < 0) {
        printf("Cannot open \\DEV\\IEEEIN\n");
        close(ieee_out);
        return -1;
    }
    
    printf("Setting raw mode...\n");
    rawmode(ieee_out);
    rawmode(ieee_in);  /* May fail on Personal488, that's OK */
    
    printf("Sending BREAK via IOCTL...\n");
    {
        union REGS regs;
        struct SREGS segs;
        char break_cmd[] = "BREAK";
        
        regs.x.ax = 0x4403;     /* IOCTL write */
        regs.x.bx = ieee_out;   /* File handle */
        regs.x.cx = 5;          /* Length of "BREAK" */
        regs.x.dx = (unsigned)break_cmd;
        segread(&segs);         /* Get current segment registers */
        
        intdosx(&regs, &regs, &segs);
        
        if (regs.x.cflag) {
            printf("  IOCTL BREAK failed, error code: %d\n", regs.x.ax);
        } else {
            printf("  IOCTL BREAK succeeded\n");
        }
    }
    delay(200);
    
    
    printf("Getting driver info...\n");
    ieee_write("hello\r\n");
    delay(100);
    
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("IOTech Driver: %s", response);
    } else {
        printf("Warning: No hello response\n");
        ieee_write("abort\r\n");
        delay(100);
    }
    
    printf("Resetting IEEE system...\n");
    ieee_write("reset\r\n");
    delay(500);  /* Extended delay for reset */
    
    bytes_read = read(ieee_in, response, sizeof(response));
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        printf("Reset response: %s", response);
    }
    
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Status after reset: %s", response);
    }
    
    printf("Setting FILL ERROR mode...\n");
    ieee_write("fill error\r\n");
    delay(100);
    
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Fill response: %s", response);
    }
    
    printf("Final status check...\n");
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Status: %s", response);
        
        if (strstr(response, "ERROR") || strstr(response, "SEQUENCE")) {
            printf("Note: Status shows errors - attempting to clear...\n");
            
            ieee_write("abort\r\n");
            delay(100);
            
            ieee_write("status\r\n");
            delay(100);
            bytes_read = ieee_read(response, sizeof(response));
            if (bytes_read > 0) {
                printf("Status after abort: %s", response);
            }
        }
    }
    
    for (i = 0; i < 10; i++) {
        g_system->modules[i].enabled = 0;
        g_system->modules[i].gpib_address = 0;
        g_system->gpib_devices[i] = -1;
    }
    
    return 0;
}

void module_functions_menu(void) {
    int choice;
    int done = 0;
    int i, found;
    int slot;
    int line_count;
    int menu_line_1, menu_line_2, menu_line_3, menu_line_4, menu_line_5, menu_line_6, menu_line_0;
    
    while (!done) {
        clrscr();  /* Clear screen */
        
        for (i = 1; i <= 25; i++) {
            gotoxy(1, i);
            clreol();  /* Clear to end of line */
        }
        
        printf("Module Functions Menu\n");
        printf("====================\n\n");
        
        printf("Configured Modules:\n");
        printf("------------------\n");
        found = 0;
        line_count = 6;  /* Track current line for mouse regions */
        
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                printf("  Slot %d: ", i);
                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: printf("DC5009 Counter"); break;
                    case MOD_DM5010: printf("DM5010 DMM"); break;
                    case MOD_DM5120: printf("DM5120 DMM"); break;
                    case MOD_PS5004: printf("PS5004 Power Supply"); break;
                    case MOD_PS5010: printf("PS5010 Power Supply"); break;
                    case MOD_DC5010: printf("DC5010 Counter"); break;
                    case MOD_FG5010: printf("FG5010 Function Gen"); break;
                }
                printf(" (GPIB %d)\n", g_system->modules[i].gpib_address);
                found = 1;
                line_count++;
            }
        }
        if (!found) {
            printf("  No modules configured\n");
            line_count++;
        }
        
        line_count += 2;  /* Skip blank line and "Functions:" */
        printf("\n\nFunctions:\n");
        printf("----------\n\n");
        line_count += 2;  /* Account for header and blank line */
        
        menu_line_1 = line_count;
        printf("1. Send Custom GPIB Command\n\n");
        line_count += 2;
        
        menu_line_2 = line_count;
        printf("2. DM5120 Advanced Configuration\n\n");
        line_count += 2;
        
        menu_line_3 = line_count;
        printf("3. PS5004 Advanced Configuration\n\n");
        line_count += 2;
        
        menu_line_4 = line_count;
        printf("4. PS5010 Advanced Configuration\n\n");
        line_count += 2;
        
        menu_line_5 = line_count;
        printf("5. Test Module Communication\n\n");
        line_count += 2;
        
        menu_line_6 = line_count;
        printf("6. GPIB Terminal Mode\n\n");
        line_count += 2;
        
        menu_line_0 = line_count + 1;
        printf("\n0. Return to Main Menu\n");
        
        printf("\n\nEnter choice: ");
        
        show_mouse();
        
        choice = 0;
        if (g_mouse.present) {
            while (choice == 0) {
                if (kbhit()) {
                    hide_mouse();
                    choice = getch();
                    break;
                }
                
                if (mouse_in_region(1, menu_line_1, 40, menu_line_1)) { 
                    choice = '1'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_2, 40, menu_line_2)) { 
                    choice = '2'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_3, 40, menu_line_3)) { 
                    choice = '3'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_4, 40, menu_line_4)) { 
                    choice = '4'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_5, 40, menu_line_5)) { 
                    choice = '5'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_6, 40, menu_line_6)) { 
                    choice = '6'; 
                    break; 
                }
                if (mouse_in_region(1, menu_line_0, 40, menu_line_0)) { 
                    choice = '0'; 
                    break; 
                }
                
                delay(10);
            }
            hide_mouse();
        } else {
            choice = getch();
        }
        
        switch(choice) {
            case '1':
                send_custom_command();
                break;
                
            case '2':
                found = 0;
                clrscr();
                printf("DM5120 Modules:\n");
                printf("===============\n\n");
                
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && 
                        g_system->modules[i].module_type == MOD_DM5120) {
                        printf("Slot %d: DM5120 (GPIB %d)\n", 
                               i, g_system->modules[i].gpib_address);
                        found = 1;
                    }
                }
                
                if (!found) {
                    printf("No DM5120 modules configured.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                    break;
                }
                
                printf("\nEnter slot number: ");
                scanf("%d", &slot);
                
                if (slot >= 0 && slot < 10 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_DM5120) {
                    configure_dm5120_advanced(slot);
                } else {
                    printf("Invalid slot or not a DM5120.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                }
                break;
                
            case '3':  /* PS5004 Configuration */
                found = 0;
                clrscr();
                printf("PS5004 Power Supply Modules:\n");
                printf("============================\n\n");
                
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && 
                        g_system->modules[i].module_type == MOD_PS5004) {
                        printf("Slot %d: PS5004 (GPIB %d)\n", 
                               i, g_system->modules[i].gpib_address);
                        found = 1;
                    }
                }
                
                if (!found) {
                    printf("No PS5004 modules configured.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                    break;
                }
                
                printf("\nEnter slot number: ");
                scanf("%d", &slot);
                
                if (slot >= 0 && slot < 10 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_PS5004) {
                    configure_ps5004_advanced(slot);
                } else {
                    printf("Invalid slot or not a PS5004.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                }
                break;
                
            case '4':  /* PS5010 Configuration */
                found = 0;
                clrscr();
                printf("PS5010 Dual Power Supply Modules:\n");
                printf("=================================\n\n");
                
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && 
                        g_system->modules[i].module_type == MOD_PS5010) {
                        printf("Slot %d: PS5010 (GPIB %d)\n", 
                               i, g_system->modules[i].gpib_address);
                        found = 1;
                    }
                }
                
                if (!found) {
                    printf("No PS5010 modules configured.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                    break;
                }
                
                printf("\nEnter slot number: ");
                scanf("%d", &slot);
                
                if (slot >= 0 && slot < 10 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_PS5010) {
                    configure_ps5010_advanced(slot);
                } else {
                    printf("Invalid slot or not a PS5010.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                }
                break;
                
            case '5':  /* Test Module Communication */
                printf("\n\nEnter GPIB address to test: ");
                scanf("%d", &i);
                printf("\nSelect test type:\n");
                printf("1. DM5120 communication test\n");
                printf("2. PS5004 communication test\n");
                printf("3. PS5010 communication test\n");
                printf("4. Generic GPIB test\n");
                printf("\nChoice: ");
                choice = getch();
                
                switch(choice) {
                    case '1':
                        test_dm5120_comm_debug(i);
                        break;
                    case '2':
                        test_ps5004_comm(i);
                        break;
                    case '3':
                        test_ps5010_comm(i);
                        break;
                    case '4':
                        gpib_terminal_mode();
                        break;
                }
                break;
                
            case '6':  /* GPIB Terminal Mode */
                gpib_terminal_mode();
                break;
                
            case '0':
            case 27:  /* ESC key */
                done = 1;
                break;
        }
    }
}

void file_operations_menu(void) {
    int choice;
    int done = 0;
    int i;
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          File Operations\n");
        printf("          ===============\n\n");
        
        printf("  1. Save Measurement Data\n\n");
        
        printf("  2. Load Measurement Data\n\n");
        
        printf("  3. Save Configuration Settings\n\n");
        
        printf("  4. Load Configuration Settings\n\n");
        
        printf("  5. Print Report\n\n");
        
        printf("  0. Return to Main Menu\n\n");
        
        printf("  ============================================\n");
        printf("  Data in memory: %u global samples\n", g_system->data_count);
        
        {
            int total_module_samples = 0;
            for (i = 0; i < 10; i++) {
                if (g_system->modules[i].enabled && 
                    g_system->modules[i].module_data_count > 0) {
                    total_module_samples += g_system->modules[i].module_data_count;
                }
            }
            printf("  Module samples: %d total\n", total_module_samples);
        }
        printf("  ============================================\n\n");
        
        printf("  Enter choice: ");
        
        show_mouse();
        
        choice = 0;
        if (g_mouse.present) {
            while (choice == 0) {
                if (kbhit()) {
                    hide_mouse();
                    choice = getch();
                    break;
                }
                
                get_mouse_status();
                if (g_mouse.left_button) {
                    while (g_mouse.left_button) {
                        get_mouse_status();
                        delay(10);
                    }
                    
                    if (g_mouse.y == 6) { choice = '1'; break; }   /* Save Data */
                    if (g_mouse.y == 8) { choice = '2'; break; }   /* Load Data */
                    if (g_mouse.y == 10) { choice = '3'; break; }  /* Save Settings */
                    if (g_mouse.y == 12) { choice = '4'; break; }  /* Load Settings */
                    if (g_mouse.y == 14) { choice = '5'; break; }  /* Print Report */
                    if (g_mouse.y == 16) { choice = '0'; break; }  /* Return */
                }
                
                delay(10);
            }
            hide_mouse();
        } else {
            choice = getch();
        }
        
        switch(choice) {
            case '1':
                save_data();
                break;
                
            case '2':
                load_data();
                break;
                
            case '3':
                save_settings();
                break;
                
            case '4':
                load_settings();
                break;
                
            case '5':
                print_report();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

void measurement_menu(void) {
    int choice;
    int done = 0;
    int i, count;
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          Measurement Operations\n");
        printf("          =====================\n\n");
        
        count = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) count++;
        }
        
        printf("  1. Single Measurement\n\n");
        
        printf("  2. Continuous Measurement\n\n");
        
        printf("  3. Graph Display\n\n");
        
        printf("  4. Math Functions");
        if (g_has_287) {
            printf(" (287 available)\n\n");
        } else {
            printf(" (software mode)\n\n");
        }
        
        printf("  0. Return to Main Menu\n\n");
        
        printf("  ============================================\n");
        printf("  Configured modules: ");
        count = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                if (count > 0) printf(", ");
                printf("S%d", i);
                count++;
            }
        }
        if (count == 0) {
            printf("None");
        }
        printf("\n");
        printf("  Current samples: %u\n", g_system->data_count);
        printf("  ============================================\n\n");
        
        printf("  Enter choice: ");
        
        choice = getch();
        
        switch(choice) {
            case '1':
                if (count == 0) {
                    printf("\n\n  No modules configured!");
                    printf("\n  Press any key...");
                    getch();
                } else {
                    single_measurement();
                }
                break;
                
            case '2':
                if (count == 0) {
                    printf("\n\n  No modules configured!");
                    printf("\n  Press any key...");
                    getch();
                } else {
                    continuous_monitor_setup();
                }
                break;
                
            case '3':
                graph_display();
                break;
                
            case '4':
                math_functions_menu();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

void math_functions_menu(void) {
    int choice;
    int done = 0;
    int i, j;
    int source_slot = -1;
    int has_data = 0;
    
    sync_traces_with_modules();
    
    while (!done) {
        clrscr();
        
        printf("\n\n");
        printf("          Math Functions\n");
        printf("          ==============\n\n");
        
        if (g_has_287) {
            printf("  287 Coprocessor: ENABLED - Fast calculations\n\n");
        } else {
            printf("  287 Coprocessor: NOT FOUND - Software mode\n\n");
        }
        
        printf("  Available data:\n");
        printf("  ---------------\n");
        has_data = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && 
                g_system->modules[i].module_data && 
                g_system->modules[i].module_data_count > 0) {
                printf("  Slot %d: %s - %u samples\n", 
                       i, g_system->modules[i].description,
                       g_system->modules[i].module_data_count);
                has_data = 1;
                if (source_slot < 0) source_slot = i;  /* Auto-select first */
            }
        }
        
        if (!has_data) {
            printf("  No data available. Run measurements first.\n");
        }
        
        printf("\n  Functions:\n");
        printf("  ----------\n\n");
        
        printf("  1. FFT (Frequency Analysis)\n\n");
        
        printf("  2. Statistics (Min/Max/Avg/StdDev)\n\n");
        
        printf("  3. Differentiate (dV/dt)\n\n");
        
        printf("  4. Integrate\n\n");
        
        printf("  5. Smooth (Moving Average)\n\n");
        
        printf("  0. Return\n\n");
        
        printf("  Enter choice: ");
        
        choice = getch();
        
        if (!has_data && choice >= '1' && choice <= '5') {
            printf("\n\n  No data to process!");
            printf("\n  Press any key...");
            getch();
            continue;
        }
        
        switch(choice) {
            case '1':  /* FFT */
                perform_fft();
                break;
                
            case '2':  /* Statistics */
                calculate_statistics();
                break;
                
            case '3':  /* Differentiate */
                perform_differentiation();
                break;
                
            case '4':  /* Integrate */
                perform_integration();
                break;
                
            case '5':  /* Smooth */
                perform_smoothing();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

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
    
    printf("Select source slot (0-7): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
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
    
    printf("\nApplying Hamming window...\n");
    for (i = 0; i < N; i++) {
        if (g_has_287) {
            window = 0.54 - 0.46 * cos(2.0 * 3.14159 * i / (N - 1));
        } else {
            window = 0.54 - 0.46 * (1.0 - 2.0 * (float)i / (float)(N-1));
        }
        real_data[i] = g_system->modules[slot].module_data[i] * window;
        imag_data[i] = 0.0;
    }
    
    printf("Performing FFT...\n");
    
    if (g_has_287) {
        printf("Using 287 coprocessor...\n");
        
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
        printf("Software mode (this may take a while)...\n");
        
        dft_size = N > 64 ? 64 : N;
        
        for (k = 0; k < dft_size/2; k++) {
            sum_real = 0.0;
            sum_imag = 0.0;
            
            for (j = 0; j < dft_size; j++) {
                angle = -2.0 * 3.14159 * k * j / dft_size;
                c = 1.0 - angle*angle/2.0;
                s = angle;
                
                sum_real += real_data[j] * c;
                sum_imag += real_data[j] * s;
            }
            
            magnitude[k] = sqrt(sum_real * sum_real + sum_imag * sum_imag);
            if (magnitude[k] > max_mag) max_mag = magnitude[k];
        }
        N = dft_size;  /* Adjust size */
    }
    
    if (g_has_287) {
        for (i = 0; i < N/2; i++) {
            magnitude[i] = sqrt(real_data[i] * real_data[i] + 
                               imag_data[i] * imag_data[i]) / (N/2);
            if (magnitude[i] > max_mag) max_mag = magnitude[i];
        }
    }
    
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
    
    _ffree(real_data);
    _ffree(imag_data);
    _ffree(magnitude);
    
    printf("\nResults stored in slot %d\n", target_slot);
    printf("Note: X-axis now represents frequency (Hz)\n");
    printf("\nPress any key to continue...");
    getch();
}
void calculate_statistics(void) {
    int slot, i;
    float min, max, sum, avg, sum_sq, std_dev;
    float far *data;
    int count;
    
    clrscr();
    printf("\n\nStatistics Calculation\n");
    printf("======================\n\n");
    
    printf("Select source slot (0-7): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    printf("\nCalculating statistics for %d samples...\n", count);
    
    min = max = data[0];
    sum = 0.0;
    sum_sq = 0.0;
    
    if (g_has_287) {
        for (i = 0; i < count; i++) {
            float val = data[i];
            if (val < min) min = val;
            if (val > max) max = val;
            sum += val;
            sum_sq += val * val;
        }
        avg = sum / count;
        std_dev = sqrt((sum_sq - sum * sum / count) / (count - 1));
    } else {
        for (i = 0; i < count; i++) {
            float val = data[i];
            if (val < min) min = val;
            if (val > max) max = val;
            sum += val;
        }
        avg = sum / count;
        
        sum_sq = 0.0;
        for (i = 0; i < count; i++) {
            float diff = data[i] - avg;
            sum_sq += diff * diff;
        }
        std_dev = sqrt(sum_sq / (count - 1));
    }
    
    printf("\nResults:\n");
    printf("--------\n");
    printf("Samples:   %d\n", count);
    printf("Minimum:   %.6f\n", min);
    printf("Maximum:   %.6f\n", max);
    printf("Average:   %.6f\n", avg);
    printf("Std Dev:   %.6f\n", std_dev);
    printf("Range:     %.6f\n", max - min);
    printf("RMS:       %.6f\n", sqrt(sum_sq / count));
    
    printf("\nPress any key to continue...");
    getch();
}
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
    
    printf("Select source slot (0-7): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7 || !g_system->modules[slot].enabled ||
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
        scale_factor = 1.0 / dt;
        
        result_data[0] = (source_data[1] - source_data[0]) * scale_factor;
        
        for (i = 1; i < count - 1; i++) {
            result_data[i] = (source_data[i+1] - source_data[i-1]) * 0.5 * scale_factor;
        }
        
        result_data[count-1] = (source_data[count-1] - source_data[count-2]) * scale_factor;
    } else {
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

void perform_integration(void) {
    int slot, i;
    int target_slot = -1;
    float far *source_data;
    float far *result_data;
    int count;
    float dt;
    float sum = 0.0;
    
    clrscr();
    printf("\n\nIntegration\n");
    printf("===========\n\n");
    
    printf("Select source slot (0-7): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    source_data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    dt = g_control_panel.sample_rate_ms / 1000.0;  /* Convert to seconds */
    
    printf("\nIntegrating %d samples...\n", count);
    printf("Time step: %.3f seconds\n", dt);
    
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
    
    result_data[0] = 0.0;  /* Start at zero */
    
    if (g_has_287) {
        for (i = 1; i < count; i++) {
            sum += (source_data[i] + source_data[i-1]) * 0.5 * dt;
            result_data[i] = sum;
        }
    } else {
        for (i = 1; i < count; i++) {
            sum += source_data[i-1] * dt;
            result_data[i] = sum;
        }
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nIntegration complete!\n");
    printf("Final integral value: %.6f V*s\n", sum);
    printf("Units changed from V to V*s\n");
    printf("\nPress any key to continue...");
    getch();
}
void perform_smoothing(void) {
    int slot, i, j;
    int target_slot = -1;
    float far *source_data;
    float far *result_data;
    int count;
    int window_size;
    float sum;
    int half_window;
    
    clrscr();
    printf("\n\nSmoothing (Moving Average)\n");
    printf("==========================\n\n");
    
    printf("Select source slot (0-7): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 7 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    source_data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    printf("\nEnter window size (3-21, odd numbers): ");
    scanf("%d", &window_size);
    
    if (window_size < 3) window_size = 3;
    if (window_size > 21) window_size = 21;
    if (window_size % 2 == 0) window_size++;  /* Make odd */
    
    half_window = window_size / 2;
    
    printf("\nSmoothing %d samples with window size %d...\n", count, window_size);
    
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
    
    for (i = 0; i < count; i++) {
        sum = 0.0;
        j = 0;
        
        if (g_has_287) {
            int start = i - half_window;
            int end = i + half_window;
            
            if (start < 0) start = 0;
            if (end >= count) end = count - 1;
            
            for (j = start; j <= end; j++) {
                sum += source_data[j];
            }
            result_data[i] = sum / (end - start + 1);
        } else {
            if (i < half_window) {
                for (j = 0; j <= i + half_window && j < count; j++) {
                    sum += source_data[j];
                }
                result_data[i] = sum / (j);
            } else if (i >= count - half_window) {
                for (j = i - half_window; j < count; j++) {
                    sum += source_data[j];
                }
                result_data[i] = sum / (count - (i - half_window));
            } else {
                for (j = i - half_window; j <= i + half_window; j++) {
                    sum += source_data[j];
                }
                result_data[i] = sum / window_size;
            }
        }
    }
    
    g_system->modules[target_slot].module_data_count = count;
    
    printf("\nSmoothing complete!\n");
    printf("Noise reduction applied\n");
    printf("\nPress any key to continue...");
    getch();
}
void draw_frequency_grid(int fft_samples) {
    int i, x, y;
    int pos;
    char label[20];
    float sample_rate;
    float max_freq;
    float freq;
    char *freq_unit;
    float freq_scale;
    float y_range;
    char *unit_label = "Mag";  /* Magnitude for Y axis */
    
    sample_rate = 1000.0 / g_control_panel.sample_rate_ms;  /* Hz */
    max_freq = sample_rate / 2.0;  /* Nyquist frequency */
    
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
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    
    for (i = 0; i <= 5; i++) {
        float value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            if (i % 2 == 0) {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 3) {
                    plot_pixel(x, pos, 1);
                    if (x+1 <= GRAPH_RIGHT) plot_pixel(x+1, pos, 1);
                }
            } else {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 6) {
                    plot_pixel(x, pos, 1);
                }
            }
            
            if (value < 0.01) {
                sprintf(label, "%.1e", value);
            } else if (value < 1.0) {
                sprintf(label, "%.3f", value);
            } else {
                sprintf(label, "%.1f", value);
            }
            draw_text(2, pos - 3, label, 2);
        }
    }
    
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        if (i % 2 == 0) {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 3) {
                plot_pixel(pos, y, 1);
                if (y+1 <= GRAPH_BOTTOM) plot_pixel(pos, y+1, 1);
            }
        } else {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 6) {
                plot_pixel(pos, y, 1);
            }
        }
        
        freq = max_freq * i / 5.0;
        sprintf(label, "%.1f", freq / freq_scale);
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    sprintf(label, "Frequency (%s)", freq_unit);
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, GRAPH_BOTTOM + 10, label, 1);
}

void auto_scale_graph(void) {
    int i, j;
    float min_val = 1e30, max_val = -1e30;
    int found_data = 0;
    float range, margin;
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            for (j = 0; j < g_traces[i].data_count; j++) {
                if (g_traces[i].data[j] < min_val) min_val = g_traces[i].data[j];
                if (g_traces[i].data[j] > max_val) max_val = g_traces[i].data[j];
                found_data = 1;
            }
        }
    }
    
    if (!found_data) {
        g_graph_scale.min_value = 0.0;
        g_graph_scale.max_value = 10.0;
        return;
    }
    
    range = max_val - min_val;
    
    if (range < 0.1) {
        float center = (max_val + min_val) / 2.0;
        min_val = center - 0.5;
        max_val = center + 0.5;
        range = 1.0;
    }
    
    margin = range * 0.1;
    g_graph_scale.min_value = min_val - margin;
    g_graph_scale.max_value = max_val + margin;
    
    range = g_graph_scale.max_value - g_graph_scale.min_value;
    if (range < 0.000050) {  /* Less than 50µV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_uv = range * 1000000.0;  /* Convert to µV */
        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
        range = range_uv / 1000000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    else if (range < 0.005) {  /* Less than 5mV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_mv = range * 1000.0;  /* Convert to mV */
        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
        range = range_mv / 1000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    else if (range < 0.050) {  /* Less than 50mV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_mv = range * 1000.0;  /* Convert to mV */
        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
        range = range_mv / 1000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    
    if (g_graph_scale.min_value > 0 && g_graph_scale.min_value < 1.0) {
        g_graph_scale.min_value = 0.0;
    }
}
int value_to_y(float value) {
    float normalized = (value - g_graph_scale.min_value) / 
                      (g_graph_scale.max_value - g_graph_scale.min_value);
    return GRAPH_BOTTOM - (int)(normalized * GRAPH_HEIGHT);
}

float y_to_value(int y) {
    float normalized = (float)(GRAPH_BOTTOM - y) / GRAPH_HEIGHT;
    return g_graph_scale.min_value + normalized * 
           (g_graph_scale.max_value - g_graph_scale.min_value);
}
float get_value_at_x(trace_info *trace, int x) {
    float x_normalized;
    float sample_pos;
    int sample_index;
    float fraction;
    
    if (trace->data_count <= 1) {
        return trace->data[0];
    }
    
    x_normalized = (float)(x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
    if (x_normalized < 0.0) x_normalized = 0.0;
    if (x_normalized > 1.0) x_normalized = 1.0;
    
    sample_pos = x_normalized * (trace->data_count - 1);
    sample_index = (int)sample_pos;
    fraction = sample_pos - sample_index;
    
    if (sample_index >= trace->data_count - 1) {
        return trace->data[trace->data_count - 1];
    }
    
    return trace->data[sample_index] * (1.0 - fraction) + 
           trace->data[sample_index + 1] * fraction;
}
void draw_readout(int x, int y, char *text) {
    int text_len = strlen(text) * 6;
    int box_x = x + 5;
    int box_y = y - 30;
    int i, j;
    char enhanced_text[80];
    
    if (box_x + text_len + 8 > SCREEN_WIDTH) {
        box_x = x - text_len - 12;
    }
    if (box_y < 0) box_y = y + 5;
    
    for (i = 0; i < 16; i++) {
        for (j = 0; j < text_len + 8; j++) {
            plot_pixel(box_x + j, box_y + i, 0);
        }
    }
    
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y, 3);
        plot_pixel(box_x + j, box_y + 1, 1);
    }
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y + 15, 3);
        plot_pixel(box_x + j, box_y + 14, 1);
    }
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x, box_y + i, 3);
        plot_pixel(box_x + 1, box_y + i, 1);
    }
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x + text_len + 7, box_y + i, 3);
        plot_pixel(box_x + text_len + 6, box_y + i, 1);
    }
    
    strcpy(enhanced_text, text);
    
    if (strstr(text, ":") && !strstr(text, "V") && !strstr(text, "mV")) {
        if (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) {
            strcat(enhanced_text, " mV");
        } else {
            strcat(enhanced_text, " V");
        }
    }
    
    draw_text(box_x + 5, box_y + 5, enhanced_text, 1);  /* Shadow */
    draw_text(box_x + 4, box_y + 4, enhanced_text, 3);  /* Main text */
}
void draw_legend_enhanced(int *is_fft_trace) {
    int i, y_pos = 2;
    char label[40];
    int legend_x = GRAPH_RIGHT + 5;
    int active = 0;
    char *module_name;
    int box_height;
    int x, y;  /* Declare loop variables at function scope for C89 */
    float sample_rate, freq_res;
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled) active++;
    }
    
    if (active == 0) return;
    
    box_height = active * 18;  /* Increased spacing */
    
    draw_line(legend_x - 2, y_pos - 2, legend_x + 60, y_pos - 2, 1);
    draw_line(legend_x - 2, y_pos + box_height, legend_x + 60, y_pos + box_height, 1);
    draw_line(legend_x - 2, y_pos - 2, legend_x - 2, y_pos + box_height, 1);
    draw_line(legend_x + 60, y_pos - 2, legend_x + 60, y_pos + box_height, 1);
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled) {
            for (y = 0; y < 5; y++) {
                for (x = 0; x < 10; x++) {
                    plot_pixel(legend_x + x, y_pos + y, g_traces[i].color);
                }
            }
            draw_line(legend_x - 1, y_pos - 1, legend_x + 8, y_pos - 1, 3);
            draw_line(legend_x - 1, y_pos + 5, legend_x + 8, y_pos + 5, 3);
            draw_line(legend_x - 1, y_pos - 1, legend_x - 1, y_pos + 5, 3);
            draw_line(legend_x + 8, y_pos - 1, legend_x + 8, y_pos + 5, 3);
            
            if (is_fft_trace[i]) {
                sprintf(label, "S%d:FFT", i);
            } else {
                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: module_name = "DC5009"; break;
                    case MOD_DM5010: module_name = "DM5010"; break;
                    case MOD_DM5120: module_name = "DM5120"; break;
                    case MOD_PS5004: module_name = "PS5004"; break;
                    case MOD_PS5010: module_name = "PS5010"; break;
                    case MOD_DC5010: module_name = "DC5010"; break;
                    case MOD_FG5010: module_name = "FG5010"; break;
                    default: module_name = "Unknown"; break;
                }
                sprintf(label, "S%d:%s", i, module_name);
            }
            
            draw_text(legend_x + 10, y_pos, label, 3);
            
            if (g_system->modules[i].last_reading != 0.0 || g_traces[i].data_count > 0) {
                if (is_fft_trace[i]) {
                    sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                    freq_res = sample_rate / (g_traces[i].data_count * 2);
                    sprintf(label, "DC:%.3f", g_traces[i].data[0]);
                } else {
                    if (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) {
                        sprintf(label, "%.1fmV", g_system->modules[i].last_reading * 1000);
                    } else {
                        sprintf(label, "%.3fV", g_system->modules[i].last_reading);
                    }
                }
                draw_text(legend_x + 10, y_pos + 8, label, 1);
            }
            
            sprintf(label, "%d smp", g_traces[i].data_count);
            draw_text(legend_x + 40, y_pos + 8, label, 2);
            
            y_pos += 18;  /* Increased spacing */
            
            if (y_pos > GRAPH_BOTTOM - 20) break;
        }
    }
}
void draw_grid_dynamic(int max_samples) {
    int i, x, y;
    float value;
    int pos;
    float y_range;
    char label[20];
    int use_millivolts = 0;
    int use_microvolts = 0;
    float scale_multiplier = 1.0;
    char *unit_label = "V";
    int decimal_places = 3;
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    get_graph_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    
    if (unit_label[0] == 'u' && unit_label[1] == 'V') {
        use_microvolts = 1;
        unit_label = "uV";  /* Use 'u' for display compatibility */
    } else if (unit_label[0] == 'm' && unit_label[1] == 'V') {
        use_millivolts = 1;
    }
    
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    
    for (i = 0; i <= 5; i++) {
        value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            if (i % 2 == 0) {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 3) {
                    plot_pixel(x, pos, 1);
                    if (x+1 <= GRAPH_RIGHT) plot_pixel(x+1, pos, 1);
                }
            } else {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 6) {
                    plot_pixel(x, pos, 1);
                }
            }
            
            if (decimal_places == 0) {
                sprintf(label, "%.0f", value * scale_multiplier);
            } else {
                sprintf(label, "%.*f", decimal_places, value * scale_multiplier);
            }
            draw_text(2, pos - 3, label, 2);
        }
    }
    
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        if (i % 2 == 0) {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 3) {
                plot_pixel(pos, y, 1);
                if (y+1 <= GRAPH_BOTTOM) plot_pixel(pos, y+1, 1);
            }
        } else {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 6) {
                plot_pixel(pos, y, 1);
            }
        }
        
        if (max_samples > 0) {
            int sample_num = (max_samples - 1) * i / 5;
            sprintf(label, "%d", sample_num);
        } else {
            sprintf(label, "%d", i * 20);
        }
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, GRAPH_BOTTOM + 10, "Time (samples)", 1);
}

void main_menu(void) {
    int choice;
    int done = 0;
    int i, active_count;
    
    clrscr();
    delay(50);
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          TM5000 CONTROL SYSTEM v2.9\n");
        printf("          ==========================\n\n");
        
        printf("  1. Configure Modules\n\n");
        
        printf("  2. Measurement Operations\n\n");
        
        printf("  3. Module Functions\n\n");
        
        printf("  4. File Operations\n\n");
        
        printf("  0. Exit\n\n");
        
        printf("  ============================================\n");
        printf("  Active Modules: ");
        active_count = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                if (active_count > 0) printf(", ");
                printf("S%d", i);
                active_count++;
            }
        }
        if (active_count == 0) {
            printf("None");
        }
        printf("\n");
        
        printf("  Memory: %u KB  ", (unsigned)(_memavl() / 1024));
        printf("Data: %u/%u", g_system->data_count, g_system->buffer_size);
        if (g_mouse.present) {
            printf("  Mouse: ON");
        }
        printf("\n");
        printf("  ============================================\n\n");
        
        printf("  Enter choice: ");
        
        show_mouse();
        
        choice = 0;
        if (g_mouse.present) {
            while (choice == 0) {
                if (kbhit()) {
                    hide_mouse();
                    choice = getch();
                    break;
                }
                
                get_mouse_status();
                if (g_mouse.left_button) {
                    while (g_mouse.left_button) {
                        get_mouse_status();
                        delay(10);
                    }
                    
                    if (g_mouse.y == 7) { choice = '1'; break; }   /* Configure Modules (shown on line 6) */
					if (g_mouse.y == 9) { choice = '2'; break; }   /* Measurement Operations (shown on line 8) */
					if (g_mouse.y == 11) { choice = '3'; break; }  /* Module Functions (shown on line 10) */
					if (g_mouse.y == 13) { choice = '4'; break; }  /* File Operations (shown on line 12) */
					if (g_mouse.y == 15) { choice = '0'; break; }  /* Exit (shown on line 14) */
                }
                
                delay(10);
            }
            hide_mouse();
        } else {
            choice = getch();
        }
        
        switch(choice) {
            case '1': 
                configure_modules(); 
                break;
                
            case '2': 
                measurement_menu();
                break;
                
            case '3': 
                module_functions_menu();
                break;
                
            case '4': 
                file_operations_menu();
                break;
                
            case '0':
            case 'x':
            case 'X':
            case 27:  /* ESC key */
                done = 1;
                break;
        }
    }
}

void cleanup(void) {
    int i;
    
    for (i = 0; i < 10; i++) {
        if (g_system && g_system->modules[i].enabled) {
            gpib_local(g_system->modules[i].gpib_address);
        }
    }
    for (i = 0; i < 10; i++) {
        free_module_buffer(i);
    }
    if (ieee_out >= 0) close(ieee_out);
    if (ieee_in >= 0) close(ieee_in);
    
    if (g_system) {
        if (g_system->data_buffer)
            _ffree(g_system->data_buffer);
        free(g_system);
    }
}

int main(void) {
    int i;
    unsigned int control_word;
    unsigned int status_word;
    
    srand((unsigned)time(NULL));
    
    clrscr();
    printf("TM5000 GPIB Control System v2.9\n");
    printf("For Personal488 Driver V2.2 August 1989\n");
    printf("(C) 2025 - For Gridcase 1520\n\n");
    g_has_287 = 0;  /* Default to no FPU */
    
    _asm {
        ; Clear any pending FPU exceptions
        fclex                       
        
        ; Initialize the FPU
        fninit                      
        
        ; Store initial control word (should be 0x037F after FNINIT)
        fnstcw  control_word        
        mov     ax, control_word
        cmp     ax, 0x037F          ; Check for expected init value
        jne     no_287              
        
        ; Try a simple FPU operation
        fld1                        ; Load 1.0 onto FPU stack
        fldz                        ; Load 0.0 onto FPU stack
        fcompp                      ; Compare and pop both
        fstsw   status_word         ; Store status word
        mov     ax, status_word
        sahf                        ; Store AH into flags
        
        ; If we got here without hanging, FPU exists
        mov     g_has_287, 1        
        
        no_287:
    }
    
    if (g_has_287) {
        printf("Math coprocessor (80287) detected - enhanced calculations enabled\n");
        printf("High sample rate (1000 samples) with optimized display\n");
    } else {
        printf("No math coprocessor detected - using software floating point\n");
        printf("Extended sampling (1000 samples) with automatic decimation\n");
    }
	printf("'I' Initial Data Sequence Driver Error\n");
    g_system = init_measurement_system(1000);
    if (!g_system) {
        printf("Error: Insufficient memory\n");
        return 1;
    }
    
    if (init_gpib_system() < 0) {
        printf("Error: GPIB initialization failed\n");
        printf("\nMake sure:\n");
        printf("1. DRVR488.EXE is loaded\n");
        printf("2. Program is compiled in real mode (-mc flag)\n");
        printf("3. Personal488 hardware is connected\n");
        cleanup();
        return 1;
    }
    if (init_mouse()) {
        printf("Mouse support enabled.\n");
    } else {
        printf("No mouse detected - keyboard only mode.\n");
    }
    printf("\nSystem initialized successfully.\n");
    printf("Note: Personal488 enforces strict command/response pairing\n");
    printf("Press any key to continue...");
    getch();
    
    clrscr();
    gotoxy(1, 1);
    
    for (i = 0; i < 5; i++) {
        printf("                                                                      \n");
    }
    
    delay(100);
    
    main_menu();
    
    cleanup();
    
    clrscr();
    
	printf("Thank you for using TM5000 Control System.\n");
    
    return 0;
}

