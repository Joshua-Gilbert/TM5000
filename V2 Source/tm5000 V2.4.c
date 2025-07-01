/*
 * TM5000 GPIB Control System for Gridcase 1520
 * Version 2.4
 *Compile with: wcl -ml -0 -bt=dos -os -d0 tm5000F.c ieeeio_w.c 
 *
 * Changes in v2.2:
 * - Implemented per-module data storage for separate trace display
 * - Reduced font size and spacing for better screen fit
 * - Each module now maintains its own data buffer
 * - Graph displays true multi-channel traces in different colors
 * - Optimized text positioning to prevent overlap
 * - Added module-specific data management functions
 *
 *Changes in v2.3
 * - Complete font with scientific symbols
 * - Automatic mV scale switching at high zoom
 * - Enhanced graphics utilizing 640x400 mode
 *  
 *Changes in v2.4
 * - Menu option for configuring continuous measurements 
 * - Sample rate selection with presets and custom input
 * - Load_data works with variable sample rates
 * - Continous Measurements interval runs off DOS timer ticks at 0x0040:006C (BIOS data area) instead of clock()
 * - Module functions menu and menu reorganization
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

/* Global IEEE handles - DUAL HANDLES */
int ieee_out = -1;  /* Handle for writing to GPIB */
int ieee_in = -1;   /* Handle for reading from GPIB */

/* Function prototypes for DOS-specific functions */
#ifdef __WATCOMC__
#define outportb(port, value) outp(port, value)
#define inportb(port) inp(port)
#else
void outportb(unsigned int port, unsigned char value);
unsigned char inportb(unsigned int port);
#endif
void delay(unsigned int milliseconds);

/* Console functions not in OpenWatcom */
void clrscr(void);
void gotoxy(int x, int y);

/* Implementation of console functions */
void clrscr(void) {
    union REGS regs;
    regs.w.ax = 0x0600;  /* Scroll window up */
    regs.h.bh = 0x07;    /* White on black */
    regs.h.ch = 0;       /* Top row */
    regs.h.cl = 0;       /* Left column */
    regs.h.dh = 24;      /* Bottom row */
    regs.h.dl = 79;      /* Right column */
    int86(0x10, &regs, &regs);
    
    /* Reset cursor to home */
    gotoxy(1, 1);
}
void textattr(unsigned char attr) {
    /* Set text attribute using BIOS */
    union REGS regs;
    regs.h.ah = 0x09;    /* Write character and attribute */
    regs.h.bl = attr;    /* Attribute */
    regs.w.cx = 0;       /* Don't write any characters */
    regs.h.bh = 0;       /* Video page 0 */
    int86(0x10, &regs, &regs);
}
/* Clear to end of line */
void clreol(void) {
    union REGS regs;
    int x, y;
    
    /* Get current cursor position */
    regs.h.ah = 0x03;
    regs.h.bh = 0;
    int86(0x10, &regs, &regs);
    x = regs.h.dl;
    y = regs.h.dh;
    
    /* Clear from cursor to end of line */
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
/* More Function prototypes*/
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


/* IOTech 488 GPIB Constants */
#define T10s    13         /* 10 seconds timeout */
#define T1s     11         /* 1 second timeout */
#define T300ms  10         /* 300 milliseconds timeout */

/* CGA Graphics Constants */
#define CGA_MEMORY      0xB8000000L
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   200
#define LPT1_BASE       0x378
#define LPT1_STATUS     (LPT1_BASE + 1)
#define LPT1_CONTROL    (LPT1_BASE + 2)
/* For enhanced graph */
#define GRAPH_LEFT      35    /* Reduced from 40 */
#define GRAPH_RIGHT     260   /* Reduced from 279 */
#define GRAPH_TOP       16    /* Reduced from 20 */
#define GRAPH_BOTTOM    175   /* Reduced from 180 */
#define GRAPH_WIDTH     (GRAPH_RIGHT - GRAPH_LEFT)
#define GRAPH_HEIGHT    (GRAPH_BOTTOM - GRAPH_TOP)

/* Module Types */
#define MOD_NONE     0
#define MOD_DC5009   1
#define MOD_DM5010   2
#define MOD_DM5120   3
#define MOD_PS5004   4
#define MOD_PS5010   5
#define MOD_DC5010   6
#define MOD_FG5010   7

/* Mouse Support Constants */
#define MOUSE_INT       0x33
#define MOUSE_RESET     0x00
#define MOUSE_SHOW      0x01
#define MOUSE_HIDE      0x02
#define MOUSE_STATUS    0x03
#define MOUSE_SET_POS   0x04

/* Global error flag for IOTech driver */
int gpib_error = 0;

/* Data Structures */
#pragma pack(1)
typedef struct {
    unsigned char module_type;
    unsigned char slot_number;
    unsigned char enabled;
    unsigned char gpib_address;
    float last_reading;
    char description[20];
    /* NEW FIELDS FOR V2.2 */
    float far *module_data;      /* Per-module data buffer */
    unsigned int module_data_count;  /* Count for this module */
    unsigned int module_data_size;   /* Size allocated */
} tm5000_module;
#pragma pack()

typedef struct {
    tm5000_module modules[8];
    float far *data_buffer;
    unsigned int buffer_size;
    unsigned int data_count;
    int sample_rate;
    int gpib_devices[8];
} measurement_system;

/* Mouse state structure */
typedef struct {
    int present;
    int x, y;
    int left_button;
    int right_button;
} mouse_state;

/* Control Panel State */
typedef struct {
    int running;              /* 0=stopped, 1=running */
    int sample_rate_ms;       /* Delay between samples in milliseconds */
    int selected_rate;        /* Index of selected rate preset */
    char custom_rate[10];     /* Custom rate string */
    int use_custom;           /* 0=use preset, 1=use custom */
    /* REMOVED: int panel_needs_redraw; */
} control_panel_state;

/* Graph scaling structure */
typedef struct {
    float min_value;
    float max_value;
    float scale_factor;
    int auto_scale;
    int grid_divisions;
    float pan_offset;
    float zoom_factor;
} graph_scale;

/* Module trace info for multi-channel display */
typedef struct {
    int slot;
    int enabled;
    unsigned char color;
    char label[20];
    float *data;
    int data_count;
} trace_info;

/* DM5120 Configuration Structure */
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
} dm5120_config;

/* PS5004 Configuration Structure */
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

/* Global Variables */
measurement_system *g_system = NULL;
unsigned char far *video_mem = (unsigned char far *)CGA_MEMORY;
char g_error_msg[80] = "";
dm5120_config g_dm5120_config[8];  /* Global DM5120 configurations */
ps5004_config g_ps5004_config[8];  /* Global PS5004 configurations */
mouse_state g_mouse = {0, 0, 0, 0, 0}; /*Global Mouse State*/
graph_scale g_graph_scale = {-10.0, 10.0, 1.0, 1, 10, 0.0, 1.0}; /* Global graph */
trace_info g_traces[8];
control_panel_state g_control_panel = {0, 500, 2, "500", 0};

/* Sample rate presets (in milliseconds) */
int sample_rate_presets[] = {100, 250, 500, 1000, 2000, 5000, 10000};
char *sample_rate_labels[] = {"100ms", "250ms", "500ms", "1s", "2s", "5s", "10s"};
#define NUM_RATE_PRESETS 7


/* Standard ASCII characters 0x20-0x7F (96 characters) */
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

/* Extended Scientific Symbols - separate array */
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

/* Function Prototypes */
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
void init_dm5120_config(int slot);
float read_dm5120_voltage(int address);
void test_dm5120_comm_debug(int address);
void gpib_terminal_mode(void);
void drain_input_buffer(void);
int command_has_response(const char *cmd);
int allocate_module_buffer(int slot, unsigned int size);
void free_module_buffer(int slot);
void store_module_data(int slot, float value);
void clear_module_data(int slot);

/* PS5004 specific functions */
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

/* rawmode() is provided by ieeeio_w.c */
extern void rawmode(int handle);

/* Character mapping function */
int get_font_index(char c) {
    /* Direct ASCII mapping for standard characters */
    if (c >= 0x20 && c <= 0x7F) {
        return c - 0x20;
    }
    
    /* Extended scientific symbols - return negative to indicate scientific_font array */
    /* The negative value indicates which array to use */
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

/* BEGINNING MOUSE FUNCTIONS */
/* Initialize mouse driver */
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
/* Show mouse cursor */
void show_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SHOW;
    int86(MOUSE_INT, &regs, &regs);
}

/* Hide mouse cursor */
void hide_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_HIDE;
    int86(MOUSE_INT, &regs, &regs);
}

/* Get mouse status */
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

/* Set mouse position */
void set_mouse_pos(int x, int y) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SET_POS;
    regs.w.cx = x * 8;  /* Convert from text to pixel coordinates */
    regs.w.dx = y * 8;
    int86(MOUSE_INT, &regs, &regs);
}

/* Check if mouse clicked in a region */
int mouse_in_region(int x1, int y1, int x2, int y2) {
    get_mouse_status();
    return (g_mouse.x >= x1 && g_mouse.x <= x2 && 
            g_mouse.y >= y1 && g_mouse.y <= y2 &&
            g_mouse.left_button);
}

/* Wait for mouse click or key press */
int wait_for_input(void) {
    int old_left = 0;
    
    show_mouse();
    
    while (1) {
        /* Check keyboard */
        if (kbhit()) {
            hide_mouse();
            return getch();
        }
        
        /* Check mouse */
        if (g_mouse.present) {
            get_mouse_status();
            
            /* Detect click (button release) */
            if (old_left && !g_mouse.left_button) {
                hide_mouse();
                return -1;  /* Mouse click indicator */
            }
            
            old_left = g_mouse.left_button;
        }
        
        delay(10);
    }
}

/* END OF MOUSE FUNCTIONS */

/* Allocate data buffer for a specific module */
int allocate_module_buffer(int slot, unsigned int size) {
    if (slot < 0 || slot >= 8) return 0;
    
    /* Free existing buffer if any */
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
    }
    
    /* Allocate new buffer */
    g_system->modules[slot].module_data = (float far *)_fmalloc(size * sizeof(float));
    if (g_system->modules[slot].module_data) {
        g_system->modules[slot].module_data_size = size;
        g_system->modules[slot].module_data_count = 0;
        return 1;
    }
    
    return 0;
}

/* Free module data buffer */
void free_module_buffer(int slot) {
    if (slot < 0 || slot >= 8) return;
    
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
        g_system->modules[slot].module_data = NULL;
        g_system->modules[slot].module_data_size = 0;
        g_system->modules[slot].module_data_count = 0;
    }
}

/* Store data point for a module */
void store_module_data(int slot, float value) {
    if (slot < 0 || slot >= 8) return;
    if (!g_system->modules[slot].module_data) return;
    
    if (g_system->modules[slot].module_data_count < g_system->modules[slot].module_data_size) {
        g_system->modules[slot].module_data[g_system->modules[slot].module_data_count++] = value;
    }
}

/* Clear module data */
void clear_module_data(int slot) {
    if (slot < 0 || slot >= 8) return;
    g_system->modules[slot].module_data_count = 0;
}

/* Modified IEEE write function for dual handles */
int ieee_write(const char *str) {
    if (ieee_out < 0) return -1;
    return write(ieee_out, str, strlen(str));
}

/* Modified IEEE read function for dual handles */
int ieee_read(char *buffer, int maxlen) {
    int bytes_read;
    
    if (ieee_in < 0) return -1;
    
    bytes_read = read(ieee_in, buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else if (bytes_read < 0) {
        /* Check errno for specific errors */
        if (errno == 5) {  /* General failure - often means no data */
            buffer[0] = '\0';
            return 0;  /* Return 0 to indicate no data, not error */
        }
        /* For other errors, return the error */
        return bytes_read;
    }
    
    return bytes_read;
}

/* Check for GPIB service request */
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

/* IOTech 488 Helper Functions - Modified for dual handles */
void gpib_write(int address, char *command) {
    char cmd_buffer[256];
    
    /* Check SRQ status first for DM5120 */
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 8 && 
            g_system->modules[slot].module_type == MOD_DM5120) {
            gpib_check_srq(address);
            delay(50);
        }
    }
    
    /* Format command with proper termination */
    sprintf(cmd_buffer, "output %d;%s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    /* Extra delay for DM5120 */
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 8 && 
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
    
    /* Format ENTER command with proper termination */
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    /* Small delay before reading */
    delay(50);
    
    /* Read the response */
    bytes_read = ieee_read(buffer, maxlen);
    return bytes_read;
}

int gpib_read_float(int address, float *value) {
    char buffer[256];
    char cmd_buffer[80];
    
    /* Format ENTER command */
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    /* Read response into buffer */
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        /* Try various formats */
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
    
    /* Use REMOTE command with proper termination */
    sprintf(cmd_buffer, "remote %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);  /* Increased delay for DM5120 */
}

void gpib_local(int address) {
    char cmd_buffer[80];
    
    /* Use LOCAL command */
    sprintf(cmd_buffer, "local %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(50);
}

void gpib_clear(int address) {
    char cmd_buffer[80];
    
    /* Use CLEAR command - but be careful with DM5120 */
    sprintf(cmd_buffer, "clear %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);
}

/* Enhanced GPIB communication functions for DM5120 */
void gpib_write_dm5120(int address, char *command) {
    char cmd_buffer[256];
    
    /* Poll status first */
    gpib_check_srq(address);
    delay(50);
    
    /* Format command with space after semicolon and proper termination */
    sprintf(cmd_buffer, "output %d; %s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    /* Extended delay for DM5120 */
    delay(200);
}

int gpib_read_dm5120(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    
    /* Use ENTER command */
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    /* Wait for response */
    delay(100);
    
    /* Read response */
    bytes_read = ieee_read(buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

/* Read float value from DM5120 */
int gpib_read_float_dm5120(int address, float *value) {
    char buffer[256];
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "enter %d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        /* Try various DM5120 formats */
        if (sscanf(buffer, "NDCV%e", value) == 1) return 1;
        if (sscanf(buffer, "DCV%e", value) == 1) return 1;
        if (sscanf(buffer, "NACV%e", value) == 1) return 1;
        if (sscanf(buffer, "ACV%e", value) == 1) return 1;
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
    }
    
    return 0;
}

/* Put DM5120 in remote mode */
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
    /* First abort any pending operations */
    sprintf(cmd_buffer, "abort\r\n");
    ieee_write(cmd_buffer);
    delay(100);
    
    /* Then clear */
    sprintf(cmd_buffer, "clear %d\r\n", address);
    ieee_write(cmd_buffer);
    delay(200);
}

/* Check if a command generates a response that must be read */
int command_has_response(const char *cmd) {
    /* Commands that ALWAYS generate responses */
    if (strncasecmp(cmd, "hello", 5) == 0) return 1;
    if (strncasecmp(cmd, "status", 6) == 0) return 1;
    if (strncasecmp(cmd, "enter", 5) == 0) return 1;
    if (strncasecmp(cmd, "spoll", 5) == 0) return 1;
    
    /* FILL command might generate response */
    if (strncasecmp(cmd, "fill", 4) == 0) return 1;
    
    /* Commands that typically DON'T generate responses */
    if (strncasecmp(cmd, "output", 6) == 0) return 0;
    if (strncasecmp(cmd, "remote", 6) == 0) return 0;
    if (strncasecmp(cmd, "local", 5) == 0) return 0;
    if (strncasecmp(cmd, "clear", 5) == 0) return 0;
    if (strncasecmp(cmd, "reset", 5) == 0) return 0;  /* Usually no response */
    
    /* BREAK should be sent via IOCTL, not as command */
    if (strncasecmp(cmd, "break", 5) == 0) return 0;
    
    /* Default: assume no response */
    return 0;
}

/* Drain any pending data from input buffer */
void drain_input_buffer(void) {
    char buffer[256];
    int bytes_read;
    int total_drained = 0;
    
    /* Set non-blocking read if possible, or use small timeout */
    while ((bytes_read = read(ieee_in, buffer, sizeof(buffer))) > 0) {
        total_drained += bytes_read;
        delay(10);
    }
    
    if (total_drained > 0) {
        printf("Drained %d bytes from input buffer\n", total_drained);
    }
}

/* Check for IOTech errors */
void check_gpib_error(void) {
    char status[256];
    int bytes_read;
    
    /* Clear buffer */
    memset(status, 0, sizeof(status));
    
    /* Get status - STATUS command ALWAYS generates a response */
    ieee_write("status\r\n");
    delay(50);
    
    /* MUST read the response to avoid SEQUENCE error */
    bytes_read = ieee_read(status, sizeof(status));
    
    if (bytes_read > 0) {
        printf("GPIB Status: %s\n", status);
        
        /* Check if status contains error indication */
        if (strstr(status, "ERROR") || strstr(status, "Error")) {
            strcpy(g_error_msg, status);
            gpib_error = 1;
            
            /* If SEQUENCE error, try to clear it */
            if (strstr(status, "SEQUENCE")) {
                printf("Attempting to clear SEQUENCE error...\n");
                
                /* Send abort to clear the error */
                ieee_write("abort\r\n");
                delay(100);
                
                /* Get status again */
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

/* Initialize DM5120 configuration */
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
}

/* Initialize PS5004 configuration */
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

/* PS5004 Command Functions */
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
        /* 1=Voltage regulation, 2=Current regulation, 3=Unregulated */
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
    
    /* Send SEND command to get next DVM reading */
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        /* PS5004 returns scientific notation */
        if (sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}

/* Test PS5004 communication */
void test_ps5004_comm(int address) {
    char buffer[256];
    float value;
    
    printf("\nTesting PS5004 at address %d...\n", address);
    
    /* Put device in remote mode */
    printf("Setting REMOTE mode...\n");
    gpib_remote(address);
    delay(200);
    
    /* Try to get ID */
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("ID Response: %s\n", buffer);
    } else {
        printf("No ID response\n");
    }
    
    /* Initialize */
    printf("\nInitializing PS5004...\n");
    ps5004_init(address);
    
    /* Query all settings */
    printf("\nQuerying all settings...\n");
    gpib_write(address, "SET?");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Settings: %s\n", buffer);
    }
    
    /* Set voltage to 5V */
    printf("\nSetting voltage to 5.0V...\n");
    ps5004_set_voltage(address, 5.0);
    
    /* Query voltage */
    gpib_write(address, "VOLTAGE?");
    delay(50);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Voltage setting: %s\n", buffer);
    }
    
    /* Set current limit to 100mA */
    printf("\nSetting current limit to 100mA...\n");
    ps5004_set_current(address, 0.1);
    
    /* Query current */
    gpib_write(address, "CURRENT?");
    delay(50);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("Current limit: %s\n", buffer);
    }
    
    /* Test display modes */
    printf("\nTesting display modes...\n");
    ps5004_set_display(address, "VOLTAGE");
    delay(100);
    ps5004_set_display(address, "CURRENT");
    delay(100);
    ps5004_set_display(address, "CLIMIT");
    delay(100);
    ps5004_set_display(address, "VOLTAGE");
    
    /* Test reading */
    printf("\nReading voltage (output is OFF)...\n");
    value = ps5004_read_value(address);
    printf("Voltage reading: %.4f V\n", value);
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* Configure PS5004 menu */
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
                
                /* Put in remote mode first */
                gpib_remote(address);
                delay(200);
                
                /* Apply all settings */
                ps5004_set_voltage(address, cfg->voltage);
                ps5004_set_current(address, cfg->current_limit);
                
                /* Set display mode */
                switch(cfg->display_mode) {
                    case 0: ps5004_set_display(address, "VOLTAGE"); break;
                    case 1: ps5004_set_display(address, "CURRENT"); break;
                    case 2: ps5004_set_display(address, "CLIMIT"); break;
                }
                
                /* Set interrupts */
                gpib_write(address, cfg->vri_enabled ? "VRI ON" : "VRI OFF");
                delay(50);
                gpib_write(address, cfg->cri_enabled ? "CRI ON" : "CRI OFF");
                delay(50);
                gpib_write(address, cfg->uri_enabled ? "URI ON" : "URI OFF");
                delay(50);
                
                /* Set other features */
                gpib_write(address, cfg->dt_enabled ? "DT ON" : "DT OFF");
                delay(50);
                gpib_write(address, cfg->user_enabled ? "USER ON" : "USER OFF");
                delay(50);
                gpib_write(address, cfg->rqs_enabled ? "RQS ON" : "RQS OFF");
                delay(50);
                
                /* Set output state last */
                ps5004_set_output(address, cfg->output_enabled);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'D':  /* Test measurement */
                printf("\n\nTesting measurement...\n");
                
                /* Make sure display is set appropriately */
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
                /* Reset our configuration to match */
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

/* DM5120 Command Functions */
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

/* Test DM5120 communication */
void test_dm5120_comm(int address) {
    char buffer[256];
    char status[256];
    float value;
    int i, srq_status;
    
    printf("\nTesting DM5120 at address %d...\n", address);
    
    /* Check driver status first */
    ieee_write("status\r\n");
    delay(50);
    ieee_read(status, sizeof(status));
    printf("Driver status: %s\n", status);
    
    /* Put device in remote mode */
    printf("Setting REMOTE mode...\n");
    gpib_remote_dm5120(address);
    delay(200);
    
    /* Check SRQ status */
    printf("Checking SRQ status...\n");
    srq_status = gpib_check_srq(address);
    printf("SRQ status: %d\n", srq_status);
    
    /* Clear and abort */
    printf("Sending ABORT command...\n");
    ieee_write("abort\r\n");
    delay(200);
    
    /* Try to initialize the DM5120 */
    printf("\nSending INIT command...\n");
    gpib_write_dm5120(address, "INIT");
    delay(300);
    
    /* Set to DC Volts, Auto range */
    printf("Setting DCV function...\n");
    gpib_write_dm5120(address, "FUNCT DCV");
    delay(100);
    
    printf("Setting AUTO range...\n");
    gpib_write_dm5120(address, "RANGE AUTO");
    delay(100);
    
    /* Set data format to scientific notation */
    printf("Setting data format ON (scientific)...\n");
    gpib_write_dm5120(address, "DATFOR ON");
    delay(100);
    
    /* Try different read methods */
    printf("\nAttempting measurement reads:\n");
    
    /* Method 1: voltage? command */
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
    
    /* Method 2: Execute measurement */
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
    
    /* Method 3: READ? query */
    printf("\n3. READ? query:\n");
    gpib_write_dm5120(address, "READ?");
    delay(300);
    
    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        printf("   Raw response: '%s'\n", buffer);
    } else {
        printf("   No response\n");
    }
    
    /* Query current settings */
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

/* Enhanced read_dm5120 function */
float read_dm5120_enhanced(int address, int slot) {
    float value = 0.0;
    int retry;
    dm5120_config *cfg = &g_dm5120_config[slot];
    char buffer[256];
    int srq_status;
    
    /* Check SRQ status first */
    srq_status = gpib_check_srq(address);
    if (srq_status & 0x40) {  /* Device requesting service */
        delay(100);
    }
    
    /* First, try the voltage? command */
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
    
    /* If voltage? didn't work, try standard commands */
    if (cfg->trigger_mode == 0) {  /* CONT mode */
        gpib_write_dm5120(address, "X");
        delay(300);
    } else {
        gpib_write_dm5120(address, "SEND");
        delay(400);
    }
    
    /* Try to read the value */
    if (gpib_read_float_dm5120(address, &value)) {
        return value;
    }
    
    /* If that failed, try alternate method */
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

/* Wrapper for compatibility */
float read_dm5120(int address) {
    int i;
    for (i = 0; i < 8; i++) {
        if (g_system->modules[i].enabled && 
            g_system->modules[i].gpib_address == address &&
            g_system->modules[i].module_type == MOD_DM5120) {
            return read_dm5120_enhanced(address, i);
        }
    }
    return read_dm5120_enhanced(address, 0);
}

/* Simple voltage reading */
float read_dm5120_voltage(int address) {
    char buffer[256];
    float value = 0.0;
    
    /* Check SRQ first */
    gpib_check_srq(address);
    delay(50);
    
    /* Use the voltage? command */
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

/* Configure DM5120 menu */
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
        printf("8. Query Current Status\n");
        printf("9. Apply Settings to DM5120\n");
        printf("A. Test Measurement\n");
        printf("B. Run Communication Test\n");
        printf("0. Return to Module Config\n");
        
        printf("\nSelect option: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':  /* Function */
                printf("\n\nFunctions:\n");
                printf("1. DCV (DC Voltage)\n");
                printf("2. ACV (AC Voltage)\n");
                printf("3. OHMS (Resistance)\n");
                printf("4. DCA (DC Current)\n");
                printf("5. ACA (AC Current)\n");
                printf("6. ACVDB (AC Voltage in dB)\n");
                printf("7. ACADB (AC Current in dB)\n");
                printf("8. OHMSCOMP (Ohms with compensation)\n");
                printf("Select: ");
                temp_int = getch() - '0';
                
                switch(temp_int) {
                    case 1: strcpy(cfg->function, "DCV"); break;
                    case 2: strcpy(cfg->function, "ACV"); break;
                    case 3: strcpy(cfg->function, "OHMS"); break;
                    case 4: strcpy(cfg->function, "DCA"); break;
                    case 5: strcpy(cfg->function, "ACA"); break;
                    case 6: strcpy(cfg->function, "ACVDB"); break;
                    case 7: strcpy(cfg->function, "ACADB"); break;
                    case 8: strcpy(cfg->function, "OHMSCOMP"); break;
                }
                break;
                
            case '2':  /* Range */
                printf("\n\nRange (0=AUTO, 1-7=Manual): ");
                scanf("%d", &cfg->range_mode);
                break;
                
            case '3':  /* Filter */
                printf("\n\nFilter ON/OFF (1/0): ");
                scanf("%d", &cfg->filter_enabled);
                if (cfg->filter_enabled) {
                    printf("Filter value (1-99): ");
                    scanf("%d", &cfg->filter_value);
                }
                break;
                
            case '4':  /* Trigger */
                printf("\n\nTrigger modes:\n");
                printf("0. CONT (Continuous)\n");
                printf("1. ONE (Single)\n");
                printf("2. TALK\n");
                printf("3. EXT (External)\n");
                printf("Select: ");
                scanf("%d", &cfg->trigger_mode);
                break;
                
            case '5':  /* Digits */
                printf("\n\nNumber of digits (3-6): ");
                scanf("%d", &temp_int);
                if (temp_int >= 3 && temp_int <= 6) {
                    cfg->digits = temp_int;
                }
                break;
                
            case '6':  /* NULL */
                printf("\n\nNULL function ON/OFF (1/0): ");
                scanf("%d", &cfg->null_enabled);
                if (cfg->null_enabled) {
                    printf("Use ACQUIRE (1) or enter value (0)? ");
                    scanf("%d", &temp_int);
                    if (temp_int == 0) {
                        printf("NULL value: ");
                        scanf("%f", &cfg->nullval);
                    }
                }
                break;
                
            case '7':  /* Data format */
                printf("\n\nData format - Scientific (1) or NR3 (0): ");
                scanf("%d", &cfg->data_format);
                break;
                
            case '8':  /* Query status */
                printf("\n\nQuerying DM5120 status...\n");
                
                gpib_write_dm5120(address, "FUNCT?");
                delay(50);
                if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
                    printf("Function: %s\n", buffer);
                }
                
                gpib_write_dm5120(address, "RANGE?");
                delay(50);
                if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
                    printf("Range: %s\n", buffer);
                }
                
                gpib_write_dm5120(address, "FILTER?");
                delay(50);
                if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
                    printf("Filter: %s\n", buffer);
                }
                
                gpib_write_dm5120(address, "TRIGGER?");
                delay(50);
                if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
                    printf("Trigger: %s\n", buffer);
                }
                
                gpib_write_dm5120(address, "ERROR?");
                delay(50);
                if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
                    printf("Error: %s\n", buffer);
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case '9':  /* Apply settings */
                printf("\n\nApplying settings to DM5120...\n");
                
                /* Put in remote mode first */
                gpib_remote_dm5120(address);
                delay(200);
                
                /* Initialize */
                gpib_write_dm5120(address, "INIT");
                delay(300);
                
                /* Apply all settings */
                dm5120_set_function(address, cfg->function);
                dm5120_set_range(address, cfg->range_mode);
                dm5120_set_filter(address, cfg->filter_enabled, cfg->filter_value);
                
                switch(cfg->trigger_mode) {
                    case 0: dm5120_set_trigger(address, "CONT"); break;
                    case 1: dm5120_set_trigger(address, "ONE"); break;
                    case 2: dm5120_set_trigger(address, "TALK"); break;
                    case 3: dm5120_set_trigger(address, "EXT"); break;
                }
                
                dm5120_set_digits(address, cfg->digits);
                dm5120_set_null(address, cfg->null_enabled, cfg->nullval);
                dm5120_set_data_format(address, cfg->data_format);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'A':  /* Test measurement */
                printf("\n\nTesting measurement...\n");
                
                printf("Using voltage? command: ");
                temp_float = read_dm5120_voltage(address);
                printf("%.6f V\n", temp_float);
                
                printf("Using configured method: ");
                temp_float = read_dm5120_enhanced(address, slot);
                printf("%.6f V\n", temp_float);
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'B':  /* Run communication test */
                test_dm5120_comm(address);
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

/* CGA Graphics Functions */
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

void draw_waveform_far(float far *data, int count, unsigned char color) {
    /* This is now handled by the enhanced graph_display function */
    /* Keep this stub for compatibility */
    graph_display();
}

/* Parallel Port Functions */
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

/* Module Configuration */
void configure_modules(void) {
    int slot, type, addr, i;
    int addr_conflict;
    char buffer[80];
    char status[256];
    
    clrscr();
    printf("Module Configuration\n");
    printf("===================\n\n");
    
    for (slot = 0; slot < 8; slot++) {
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
    
    if (slot >= 0 && slot < 8) {
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
        
        if (type >= 0 && type <= 7) {
            g_system->modules[slot].module_type = type;
            g_system->modules[slot].slot_number = slot;
            
            if (type != MOD_NONE) {
                do {
                    printf("\nEnter GPIB address (1-30, default=%d): ", 10 + slot);
                    fflush(stdin);
                    fgets(buffer, sizeof(buffer), stdin);
                    
                    if (sscanf(buffer, "%d", &addr) == 1) {
                        if (addr >= 1 && addr <= 30) {
                            addr_conflict = 0;
                            for (i = 0; i < 8; i++) {
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
                        addr = 10 + slot;
                        addr_conflict = 0;
                        for (i = 0; i < 8; i++) {
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
                    init_dm5120_config(slot);
                    test_dm5120_comm(g_system->modules[slot].gpib_address);
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
                
                /* Allocate data buffer for this module */
                if (!allocate_module_buffer(slot, 250)) {  /* 250 points per module */
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

/* Measurement Functions */
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

/* Power Supply Control Functions */
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

/* Function Generator Control */
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
    
    for (i = 0; i < 8; i++) {
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
                    value = read_dm5010(g_system->modules[i].gpib_address);
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
                    
                case MOD_PS5004:
                    ps_cfg = &g_ps5004_config[i];
                    printf("\n");
                    printf("  Settings: %.4fV, %.0fmA limit, Output %s\n",
                           ps_cfg->voltage, ps_cfg->current_limit * 1000,
                           ps_cfg->output_enabled ? "ON" : "OFF");
                    
                    /* Read based on display mode */
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
                    
                    /* Get regulation status */
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

/* Continuous monitor settings menu */
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
                /* Start monitoring */
                continuous_monitor();
                break;
                
            case 27: /* ESC */
                done = 1;
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
    
    /* Ensure module buffers are allocated */
    for (i = 0; i < 8; i++) {
        if (g_system->modules[i].enabled) {
            active_modules++;
            if (!g_system->modules[i].module_data) {
                if (!allocate_module_buffer(i, 1000)) {
                    printf("WARNING: Failed to allocate buffer for slot %d!\n", i);
                }
            }
            /* Clear any old data */
            clear_module_data(i);
        }
    }
    
    /* Clear global data */
    g_system->data_count = 0;
    
    /* Calculate ticks needed for sample rate (18.2 ticks per second) */
    ticks_per_sample = (g_control_panel.sample_rate_ms * 182L) / 10000L;
    if (ticks_per_sample < 1) ticks_per_sample = 1;
    
    clrscr();
    printf("Continuous Monitor - Press SPACE to start/stop, ESC to exit\n");
    printf("Sample rate: %d ms (%lu ticks), %d active modules\n", 
           g_control_panel.sample_rate_ms, ticks_per_sample, active_modules);
    printf("Commands: C=Clear data\n");
    printf("============================================================\n\n");
    
    /* Get initial tick count from BIOS data area */
    last_tick_count = *((unsigned long far *)0x0040006CL);
    
    while (!done) {
        current_time = time(NULL);
        gotoxy(1, 5);
        printf("Time: %ld sec  Samples: %u  Status: %-8s  ", 
               current_time - start_time, 
               g_system->data_count,
               g_control_panel.running ? "RUNNING" : "STOPPED");
        
        /* Show measurement overhead if running */
        if (measurement_ticks > 0 && g_control_panel.running) {
            printf("Overhead: %lu ms  ", (measurement_ticks * 10000L) / 182L);
        }
        printf("\n\n");
        
        /* Get current tick count */
        current_tick_count = *((unsigned long far *)0x0040006CL);
        
        /* Check if it's time for a sample, accounting for measurement time */
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
        
        /* Display/measure all modules */
        samples_taken = 0;
        for (i = 0; i < 8; i++) {
            if (g_system->modules[i].enabled) {
                /* Get type string */
                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: strcpy(type_str, "DC5009"); break;
                    case MOD_DM5010: strcpy(type_str, "DM5010"); break;
                    case MOD_DM5120: strcpy(type_str, "DM5120"); break;
                    case MOD_PS5004: strcpy(type_str, "PS5004"); break;
                    case MOD_PS5010: strcpy(type_str, "PS5010"); break;
                    case MOD_DC5010: strcpy(type_str, "DC5010"); break;
                    case MOD_FG5010: strcpy(type_str, "FG5010"); break;
                    default: strcpy(type_str, "Unknown"); break;
                }
                
                printf("S%d %-6s[%2d]:", i, type_str, g_system->modules[i].gpib_address);
                
                if (g_control_panel.running && need_sample) {
                    /* Take measurement */
                    switch(g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            value = read_dc5009(g_system->modules[i].gpib_address);
                            printf("%12.6f MHz   ", value / 1e6);
                            break;
                            
                        case MOD_DM5010:
                            value = read_dm5010(g_system->modules[i].gpib_address);
                            printf("%12.4f V     ", value);
                            break;
                            
                        case MOD_DM5120:
                            value = read_dm5120_voltage(g_system->modules[i].gpib_address);
                            if (value == 0.0) {
                                value = read_dm5120(g_system->modules[i].gpib_address);
                            }
                            printf("%12.6f V     ", value);
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
                            
                        default:
                            value = 0.0;
                            printf("Not implemented       ");
                    }
                    
                    /* Store the reading in BOTH places */
                    g_system->modules[i].last_reading = value;
                    
                    /* Store in module-specific buffer */
                    store_module_data(i, value);
                    
                    /* ALSO store in global buffer (for first module or average) */
                    if (samples_taken == 0 && g_system->data_count < g_system->buffer_size) {
                        g_system->data_buffer[g_system->data_count] = value;
                    }
                    
                    samples_taken++;
                } else {
                    /* Just display last value or status */
                    if (g_control_panel.running) {
                        printf("%12.6f       ", g_system->modules[i].last_reading);
                    } else {
                        printf(" [Press SPACE]        ");
                    }
                }
                
                printf("\n");
            }
        }
        
        /* After all modules sampled, update counters and calculate overhead */
        if (need_sample && g_control_panel.running && samples_taken > 0) {
            /* Increment global count */
            if (g_system->data_count < g_system->buffer_size) {
                g_system->data_count++;
            }
            
            /* Calculate measurement overhead */
            tick_end = *((unsigned long far *)0x0040006CL);
            measurement_ticks = tick_end - tick_start;
            
            need_sample = 0;
        }
        
        /* Check for keyboard input */
        if (kbhit()) {
            key = getch();
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case ' ':
                    g_control_panel.running = !g_control_panel.running;
                    if (g_control_panel.running) {
                        /* Reset tick counter and measurement overhead when starting */
                        last_tick_count = *((unsigned long far *)0x0040006CL);
                        measurement_ticks = 0;
                    }
                    break;
                    
                case 'C':
                    /* Clear all data */
                    for (i = 0; i < 8; i++) {
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
        
        /* Small delay to prevent CPU hogging on 286 */
        delay(20);
    }
    
    /* Summary */
    printf("\n\nMonitoring complete.\n");
    printf("Total samples: %u\n", g_system->data_count);
    if (g_system->data_count > 0) {
        printf("First value: %.6f\n", g_system->data_buffer[0]);
        printf("Last value: %.6f\n", g_system->data_buffer[g_system->data_count-1]);
    }
    printf("\nPress any key to continue...");
    getch();
}

/* Display Graph Functionality */
/* Color mapping for module types */
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

/* Draw text in graphics mode */
void draw_text(int x, int y, char *text, unsigned char color) {
    draw_text_scaled(x, y, text, color, 1, 1);
}

/* New function to draw scaled text */
void draw_text_scaled(int x, int y, char *text, unsigned char color, int scale_x, int scale_y) {
    int i, j, sx, sy;
    unsigned char font_byte;
    int char_index;
    
    while (*text) {
        char_index = get_font_index(*text);
        
        /* Draw each character using appropriate font array */
        for (i = 0; i < 7; i++) {
            if (char_index >= 0) {
                /* Positive index means use enhanced_font array */
                font_byte = enhanced_font[char_index][i];
            } else {
                /* Negative index means use scientific_font array */
                /* Convert negative index to array position: -1 becomes 0, -2 becomes 1, etc */
                font_byte = scientific_font[(-char_index) - 1][i];
            }
            
            for (j = 0; j < 5; j++) {
                if (font_byte & (0x10 >> j)) {
                    /* Draw scaled pixels */
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

/* Draw vertical grid line with label */
void draw_grid_vline(int x, float value, unsigned char color) {
    int y;
    char label[10];
    int int_val = (int)value;
    
    /* Draw dashed line */
    for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 4) {
        plot_pixel(x, y, color);
    }
    
    /* Draw simple integer label */
    sprintf(label, "%d", int_val);
    draw_text(x - 6, GRAPH_BOTTOM + 3, label, color);
}

/* Draw horizontal grid line with label */
void draw_grid_hline(int y, float value, unsigned char color) {
    int x;
    char label[10];
    
    /* Draw dashed line */
    for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 4) {
        plot_pixel(x, y, color);
    }
    
    /* Format value properly - always show one decimal place for consistency */
    sprintf(label, "%.1f", value);
    
    /* Draw label aligned to the left */
    draw_text(2, y - 3, label, color);
}

/* Updated draw_grid function with dynamic X-axis scaling */
void draw_grid(void) {
    int i, x, y;
    float value;
    int pos;
    float y_range;
    char label[20];
    int use_millivolts = 0;
    float scale_multiplier = 1.0;
    char *unit_label = "V";
    int max_samples = 0;
    int j;
    
    /* Find maximum sample count across all traces */
    for (i = 0; i < 8; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > max_samples) {
            max_samples = g_traces[i].data_count;
        }
    }
    
    /* Calculate Y range */
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    /* Determine if we should use millivolts */
    if (y_range < 0.1) {
        use_millivolts = 1;
        scale_multiplier = 1000.0;
        unit_label = "mV";
    }
    
    /* Draw border */
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    /* Draw unit label */
    draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    
    /* Draw horizontal grid lines (Y-axis) */
    for (i = 0; i <= 5; i++) {
        value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            /* Grid line */
            if (i % 2 == 0) {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 3) {
                    plot_pixel(x, pos, 1);
                    plot_pixel(x+1, pos, 1);
                }
            } else {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 6) {
                    plot_pixel(x, pos, 1);
                }
            }
            
            /* Y-axis label */
            if (use_millivolts) {
                sprintf(label, "%.1f", value * scale_multiplier);
            } else {
                if (y_range < 1.0) {
                    sprintf(label, "%.3f", value);
                } else if (y_range < 10.0) {
                    sprintf(label, "%.2f", value);
                } else {
                    sprintf(label, "%.1f", value);
                }
            }
            draw_text(2, pos - 3, label, 2);
        }
    }
    
    /* Draw vertical grid lines with DYNAMIC X-axis labels */
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        /* Grid line */
        if (i % 2 == 0) {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 3) {
                plot_pixel(pos, y, 1);
                plot_pixel(pos, y+1, 1);
            }
        } else {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 6) {
                plot_pixel(pos, y, 1);
            }
        }
        
        /* X-axis label - scale based on actual sample count */
        if (max_samples > 0) {
            int sample_num = (max_samples - 1) * i / 5;
            sprintf(label, "%d", sample_num);
        } else {
            sprintf(label, "%d", i * 20);  /* Default if no data */
        }
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    /* Draw "Time (samples)" label */
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, GRAPH_BOTTOM + 10, "Time (samples)", 1);
}

void draw_legend(void) {
    int i, y_pos = 2;
    char label[30];
    int legend_x = GRAPH_RIGHT + 5;
    int active = 0;
    char *module_name;
    
    /* Count active traces first */
    for (i = 0; i < 8; i++) {
        if (g_traces[i].enabled) active++;
    }
    
    if (active == 0) return;
    
    /* Draw legend box */
    draw_line(legend_x - 2, y_pos - 2, legend_x + 60, y_pos - 2, 1);
    draw_line(legend_x - 2, y_pos + active * 8, legend_x + 60, y_pos + active * 8, 1);
    draw_line(legend_x - 2, y_pos - 2, legend_x - 2, y_pos + active * 8, 1);
    draw_line(legend_x + 60, y_pos - 2, legend_x + 60, y_pos + active * 8, 1);
    
    /* Compact legend with enhanced formatting */
    for (i = 0; i < 8; i++) {
        if (g_traces[i].enabled) {
            /* Draw color swatch with border */
            int x, y;
            for (y = 0; y < 5; y++) {
                for (x = 0; x < 8; x++) {
                    plot_pixel(legend_x + x, y_pos + y, g_traces[i].color);
                }
            }
            /* Draw border around swatch */
            draw_line(legend_x - 1, y_pos - 1, legend_x + 8, y_pos - 1, 3);
            draw_line(legend_x - 1, y_pos + 5, legend_x + 8, y_pos + 5, 3);
            draw_line(legend_x - 1, y_pos - 1, legend_x - 1, y_pos + 5, 3);
            draw_line(legend_x + 8, y_pos - 1, legend_x + 8, y_pos + 5, 3);
            
            /* Get module name */
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
            
            /* Draw label with module info */
            sprintf(label, "S%d:%s", i, module_name);
            draw_text(legend_x + 10, y_pos, label, 3);
            
            /* Show last value if available */
            if (g_system->modules[i].last_reading != 0.0) {
                /* Use appropriate units */
                if (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) {
                    sprintf(label, "%.1fmV", g_system->modules[i].last_reading * 1000);
                } else {
                    sprintf(label, "%.3fV", g_system->modules[i].last_reading);
                }
                draw_text(legend_x + 10, y_pos + 7, label, 1);
            }
            
            y_pos += 16;  /* More spacing for two-line display */
            
            if (y_pos > GRAPH_BOTTOM - 20) break;
        }
    }
}

/* Auto-scale graph based on data */
void auto_scale_graph(void) {
    int i, j;
    float min_val = 1e30, max_val = -1e30;
    int found_data = 0;
    float range, margin;
    
    /* Find min/max across all enabled traces */
    for (i = 0; i < 8; i++) {
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
    
    /* Calculate range */
    range = max_val - min_val;
    
    /* If range is too small, expand it */
    if (range < 0.1) {
        float center = (max_val + min_val) / 2.0;
        min_val = center - 0.5;
        max_val = center + 0.5;
        range = 1.0;
    }
    
    /* Add 10% margin */
    margin = range * 0.1;
    g_graph_scale.min_value = min_val - margin;
    g_graph_scale.max_value = max_val + margin;
    
    /* Round to nice numbers if possible */
    if (g_graph_scale.min_value > 0 && g_graph_scale.min_value < 1.0) {
        g_graph_scale.min_value = 0.0;
    }
}

/* Get Y coordinate for a value */
int value_to_y(float value) {
    float normalized = (value - g_graph_scale.min_value) / 
                      (g_graph_scale.max_value - g_graph_scale.min_value);
    return GRAPH_BOTTOM - (int)(normalized * GRAPH_HEIGHT);
}

/* Get value for a Y coordinate */
float y_to_value(int y) {
    float normalized = (float)(GRAPH_BOTTOM - y) / GRAPH_HEIGHT;
    return g_graph_scale.min_value + normalized * 
           (g_graph_scale.max_value - g_graph_scale.min_value);
}

/* Draw crosshair cursor */
void draw_crosshair(int x, int y, unsigned char color) {
    int i;
    
    /* Vertical line */
    for (i = GRAPH_TOP; i <= GRAPH_BOTTOM; i += 2) {
        if (i != y) plot_pixel(x, i, color);
    }
    
    /* Horizontal line */
    for (i = GRAPH_LEFT; i <= GRAPH_RIGHT; i += 2) {
        if (i != x) plot_pixel(i, y, color);
    }
}

/* Get interpolated value at X position for a trace */
float get_value_at_x(trace_info *trace, int x) {
    float x_normalized;
    float sample_pos;
    int sample_index;
    float fraction;
    
    /* Handle edge cases */
    if (trace->data_count <= 1) {
        return trace->data[0];
    }
    
    /* Normalize X position to 0-1 range */
    x_normalized = (float)(x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
    if (x_normalized < 0.0) x_normalized = 0.0;
    if (x_normalized > 1.0) x_normalized = 1.0;
    
    /* Map to sample position */
    sample_pos = x_normalized * (trace->data_count - 1);
    sample_index = (int)sample_pos;
    fraction = sample_pos - sample_index;
    
    /* Bounds checking */
    if (sample_index >= trace->data_count - 1) {
        return trace->data[trace->data_count - 1];
    }
    
    /* Linear interpolation */
    return trace->data[sample_index] * (1.0 - fraction) + 
           trace->data[sample_index + 1] * fraction;
}

/* Draw value readout box */
void draw_readout(int x, int y, char *text) {
    int text_len = strlen(text) * 6;
    int box_x = x + 5;
    int box_y = y - 30;
    int i, j;
    char enhanced_text[80];
    
    /* Adjust position if too close to edge */
    if (box_x + text_len + 8 > SCREEN_WIDTH) {
        box_x = x - text_len - 12;
    }
    if (box_y < 0) box_y = y + 5;
    
    /* Clear background with larger box */
    for (i = 0; i < 16; i++) {
        for (j = 0; j < text_len + 8; j++) {
            plot_pixel(box_x + j, box_y + i, 0);
        }
    }
    
    /* Draw enhanced border with double lines */
    /* Top */
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y, 3);
        plot_pixel(box_x + j, box_y + 1, 1);
    }
    /* Bottom */
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y + 15, 3);
        plot_pixel(box_x + j, box_y + 14, 1);
    }
    /* Left */
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x, box_y + i, 3);
        plot_pixel(box_x + 1, box_y + i, 1);
    }
    /* Right */
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x + text_len + 7, box_y + i, 3);
        plot_pixel(box_x + text_len + 6, box_y + i, 1);
    }
    
    /* Format text with units */
    strcpy(enhanced_text, text);
    
    /* Check if we need to add units */
    if (strstr(text, ":") && !strstr(text, "V") && !strstr(text, "mV")) {
        /* Add appropriate unit */
        if (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) {
            strcat(enhanced_text, " mV");
        } else {
            strcat(enhanced_text, " V");
        }
    }
    
    /* Draw text with slight shadow effect */
    draw_text(box_x + 5, box_y + 5, enhanced_text, 1);  /* Shadow */
    draw_text(box_x + 4, box_y + 4, enhanced_text, 3);  /* Main text */
}

/* ADDITION: New functions for enhanced graphics capabilities */

/* Draw anti-aliased line (simulated with patterns) */
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int e2;
    int intensity;
    
    while (1) {
        plot_pixel(x1, y1, color);
        
        /* Add anti-aliasing effect with adjacent pixels */
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

/* Draw filled rectangle */
void draw_filled_rect(int x1, int y1, int x2, int y2, unsigned char color) {
    int x, y;
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            plot_pixel(x, y, color);
        }
    }
}

/* Draw gradient rectangle (vertical) */
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

/* Updated graph display function */
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
    
    /* Check if ANY module has data */
    for (i = 0; i < 8; i++) {
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
    
    /* Initialize traces with MODULE-SPECIFIC data */
    for (i = 0; i < 8; i++) {
        g_traces[i].enabled = 0;
        if (g_system->modules[i].enabled) {
            g_traces[i].slot = i;
            g_traces[i].color = get_module_color(g_system->modules[i].module_type);
            
            /* Get module type string */
            switch(g_system->modules[i].module_type) {
                case MOD_DC5009: strcpy(g_traces[i].label, "DC5009"); break;
                case MOD_DM5010: strcpy(g_traces[i].label, "DM5010"); break;
                case MOD_DM5120: strcpy(g_traces[i].label, "DM5120"); break;
                case MOD_PS5004: strcpy(g_traces[i].label, "PS5004"); break;
                case MOD_PS5010: strcpy(g_traces[i].label, "PS5010"); break;
                case MOD_DC5010: strcpy(g_traces[i].label, "DC5010"); break;
                case MOD_FG5010: strcpy(g_traces[i].label, "FG5010"); break;
                default: strcpy(g_traces[i].label, "Unknown"); break;
            }
            
            /* Use MODULE-SPECIFIC data if available */
            if (g_system->modules[i].module_data && 
                g_system->modules[i].module_data_count > 0) {
                g_traces[i].data = g_system->modules[i].module_data;
                g_traces[i].data_count = g_system->modules[i].module_data_count;
                g_traces[i].enabled = 1;
                active_traces++;
            }
        }
    }
    
    /* Initialize graphics mode */
    init_graphics();
    
    /* Auto-scale if enabled */
    if (g_graph_scale.auto_scale) {
        auto_scale_graph();
    }
    
    /* Main display loop */
    while (!done) {
        if (need_redraw) {
            /* Clear screen */
            _fmemset(video_mem, 0, 16384);
            
            /* Draw grid with dynamic scaling */
            draw_grid_dynamic(max_samples);
            
            /* Draw all enabled traces */
            for (i = 0; i < 8; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 1) {
                    float x_scale;
                    int x_pos1, x_pos2, y_pos1, y_pos2;
                    
                    /* Calculate proper scaling */
                    x_scale = (float)GRAPH_WIDTH / (float)(g_traces[i].data_count - 1);
                    
                    /* Draw waveform for this specific module */
                    for (j = 0; j < g_traces[i].data_count - 1; j++) {
                        /* Calculate X positions with proper scaling */
                        x_pos1 = GRAPH_LEFT + (int)(j * x_scale);
                        x_pos2 = GRAPH_LEFT + (int)((j + 1) * x_scale);
                        
                        /* Ensure we stay within graph bounds */
                        if (x_pos1 < GRAPH_LEFT) x_pos1 = GRAPH_LEFT;
                        if (x_pos2 > GRAPH_RIGHT) x_pos2 = GRAPH_RIGHT;
                        
                        /* Get Y positions */
                        y_pos1 = value_to_y(g_traces[i].data[j]);
                        y_pos2 = value_to_y(g_traces[i].data[j + 1]);
                        
                        /* Clip to graph area */
                        if (y_pos1 < GRAPH_TOP) y_pos1 = GRAPH_TOP;
                        if (y_pos1 > GRAPH_BOTTOM) y_pos1 = GRAPH_BOTTOM;
                        if (y_pos2 < GRAPH_TOP) y_pos2 = GRAPH_TOP;
                        if (y_pos2 > GRAPH_BOTTOM) y_pos2 = GRAPH_BOTTOM;
                        
                        /* Only draw if within bounds */
                        if (x_pos1 >= GRAPH_LEFT && x_pos2 <= GRAPH_RIGHT) {
                            draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                        }
                    }
                }
            }
            
            /* Draw legend */
            draw_legend();
            
            /* Draw status bar */
            draw_gradient_rect(0, 185, SCREEN_WIDTH-1, 199, 1, 2);
            
            /* Draw instructions */
            draw_text(2, 188, "A:Auto", 3);
            draw_text(45, 188, "+/-:Zoom", 3);
            draw_text(95, 188, "M:Mouse", 3);
            draw_text(140, 188, "C:Clear", 3);
            draw_text(185, 188, "ESC:Exit", 3);
            
            /* Show current scale range */
            sprintf(readout, "%.2f-%.2f%s", 
                    g_graph_scale.min_value * ((g_graph_scale.max_value - g_graph_scale.min_value < 0.1) ? 1000 : 1),
                    g_graph_scale.max_value * ((g_graph_scale.max_value - g_graph_scale.min_value < 0.1) ? 1000 : 1),
                    (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) ? "mV" : "V");
            draw_text(240, 188, readout, 3);
            
            need_redraw = 0;
        }
        
        /* Handle mouse if present */
        if (g_mouse.present) {
            get_mouse_status();
            
            /* Convert mouse coordinates to graphics coordinates */
            x = g_mouse.x * 320 / 80;
            y = g_mouse.y * 200 / 25;
            
            /* If mouse moved and is in graph area */
            if ((x != old_x || y != old_y) && 
                x >= GRAPH_LEFT && x <= GRAPH_RIGHT &&
                y >= GRAPH_TOP && y <= GRAPH_BOTTOM) {
                
                /* Erase old crosshair */
                if (old_x >= 0 && mouse_visible) {
                    draw_crosshair(old_x, old_y, 0);
                    need_redraw = 1;
                }
                
                /* Draw new crosshair */
                draw_crosshair(x, y, 2);
                
                /* Find nearest trace and show value */
                for (i = 0; i < 8; i++) {
                    if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                        value = get_value_at_x(&g_traces[i], x);
                        sprintf(readout, "S%d:%.3f", i, value);
                        draw_readout(x, y, readout);
                        break;
                    }
                }
                
                old_x = x;
                old_y = y;
                mouse_visible = 1;
            }
        }
        
        /* Handle keyboard */
        if (kbhit()) {
            key = getch();
            
            /* Clear any mouse cursor before processing keys */
            if (mouse_visible && old_x >= 0) {
                draw_crosshair(old_x, old_y, 0);
                mouse_visible = 0;
                need_redraw = 1;
            }
            
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case 'A':  /* Auto-scale */
                    g_graph_scale.auto_scale = 1;
                    auto_scale_graph();
                    need_redraw = 1;
                    break;
                    
                case '+':  /* Zoom in */
                    {
                        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        float range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 0.8;
                        g_graph_scale.min_value = center - range;
                        g_graph_scale.max_value = center + range;
                        g_graph_scale.auto_scale = 0;
                        need_redraw = 1;
                    }
                    break;
                    
                case '-':  /* Zoom out */
                    {
                        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        float range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 1.25;
                        g_graph_scale.min_value = center - range;
                        g_graph_scale.max_value = center + range;
                        g_graph_scale.auto_scale = 0;
                        need_redraw = 1;
                    }
                    break;
                    
                case 0:  /* Extended key */
                    key = getch();
                    switch(key) {
                        case 72:  /* Up arrow */
                            {
                                float shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                                g_graph_scale.min_value += shift;
                                g_graph_scale.max_value += shift;
                                g_graph_scale.auto_scale = 0;
                                need_redraw = 1;
                            }
                            break;
                            
                        case 80:  /* Down arrow */
                            {
                                float shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                                g_graph_scale.min_value -= shift;
                                g_graph_scale.max_value -= shift;
                                g_graph_scale.auto_scale = 0;
                                need_redraw = 1;
                            }
                            break;
                    }
                    break;
                    
                case 'M':  /* Toggle mouse */
                    if (g_mouse.present) {
                        mouse_visible = !mouse_visible;
                        if (!mouse_visible && old_x >= 0) {
                            draw_crosshair(old_x, old_y, 0);
                            need_redraw = 1;
                        }
                    }
                    break;
                    
                case 'C':  /* Clear data */
                    for (i = 0; i < 8; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    text_mode();
                    printf("\nAll data cleared. Press any key...");
                    getch();
                    done = 1;
                    break;
            }
        }
        
        delay(10);
    }
    
    /* Return to text mode */
    text_mode();
}

/* Add this new function for dynamic grid drawing */
void draw_grid_dynamic(int max_samples) {
    int i, x, y;
    float value;
    int pos;
    float y_range;
    char label[20];
    int use_millivolts = 0;
    float scale_multiplier = 1.0;
    char *unit_label = "V";
    
    /* Calculate Y range */
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    /* Determine if we should use millivolts */
    if (y_range < 0.1) {
        use_millivolts = 1;
        scale_multiplier = 1000.0;
        unit_label = "mV";
    }
    
    /* Draw border */
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    /* Draw unit label */
    draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    
    /* Draw horizontal grid lines (Y-axis) */
    for (i = 0; i <= 5; i++) {
        value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            /* Grid line */
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
            
            /* Y-axis label */
            if (use_millivolts) {
                sprintf(label, "%.1f", value * scale_multiplier);
            } else {
                if (y_range < 1.0) {
                    sprintf(label, "%.3f", value);
                } else if (y_range < 10.0) {
                    sprintf(label, "%.2f", value);
                } else {
                    sprintf(label, "%.1f", value);
                }
            }
            draw_text(2, pos - 3, label, 2);
        }
    }
    
    /* Draw vertical grid lines with DYNAMIC X-axis labels */
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        /* Grid line */
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
        
        /* X-axis label - scale based on actual sample count */
        if (max_samples > 0) {
            int sample_num = (max_samples - 1) * i / 5;
            sprintf(label, "%d", sample_num);
        } else {
            sprintf(label, "%d", i * 20);
        }
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    /* Draw "Time (samples)" label */
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, GRAPH_BOTTOM + 10, "Time (samples)", 1);
}
/* Finished Graphics */


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
    
    /* ADDITION: Print sampling configuration */
    print_string("\r\nSampling Configuration:\r\n");
    print_string("----------------------\r\n");
    
    /* Format sample rate with appropriate units */
    if (g_control_panel.sample_rate_ms < 1000) {
        sprintf(rate_str, "%d ms", g_control_panel.sample_rate_ms);
    } else {
        sprintf(rate_str, "%.1f seconds", g_control_panel.sample_rate_ms / 1000.0);
    }
    sprintf(buffer, "Sample Rate: %s\r\n", rate_str);
    print_string(buffer);
    
    /* Calculate and print samples per second/minute */
    if (g_control_panel.sample_rate_ms > 0) {
        samples_per_second = 1000.0 / g_control_panel.sample_rate_ms;
        if (samples_per_second >= 1.0) {
            sprintf(buffer, "Frequency: %.2f samples/second\r\n", samples_per_second);
        } else {
            sprintf(buffer, "Frequency: %.2f samples/minute\r\n", samples_per_second * 60);
        }
        print_string(buffer);
    }
    
     /* Print whether custom or preset rate was used */
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
    
    for (i = 0; i < 8; i++) {
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

void save_data(void) {
    FILE *fp;
    char filename[80];
    int i;
    
    clrscr();
    printf("Save Data\n");
    printf("=========\n\n");
    
    printf("Enter filename: ");
    scanf("%s", filename);
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot create file\n");
        getch();
        return;
    }
    
    fprintf(fp, "TM5000 Data File v1.2\n");
    fprintf(fp, "Samples: %u\n", g_system->data_count);
    
    /* Add sample rate information */
    fprintf(fp, "SampleRateMs: %d\n", g_control_panel.sample_rate_ms);
    fprintf(fp, "RateType: %s\n", g_control_panel.use_custom ? "Custom" : "Preset");
    if (!g_control_panel.use_custom) {
        fprintf(fp, "PresetIndex: %d\n", g_control_panel.selected_rate);
    }
    
    fprintf(fp, "Modules:\n");
    
    for (i = 0; i < 8; i++) {
        if (g_system->modules[i].enabled) {
            fprintf(fp, "%d,%d,%d,%s\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description);
        }
    }
    
    fprintf(fp, "Data:\n");
    for (i = 0; i < g_system->data_count; i++) {
        fprintf(fp, "%.6f\n", g_system->data_buffer[i]);
    }
    
    fclose(fp);
    printf("Data saved successfully.\n");
    printf("Press any key to continue...");
    getch();
}

void load_data(void) {
    FILE *fp;
    char filename[80];
    char line[100];
    char desc[20];
    int slot, type, addr;
    long file_pos;  /* DECLARE HERE, not later */
    
    clrscr();
    printf("Load Data\n");
    printf("=========\n\n");
    
    printf("Enter filename: ");
    scanf("%s", filename);
    
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file\n");
        getch();
        return;
    }
    
    fgets(line, sizeof(line), fp);  /* Read header "TM5000 Data File..." */
    fscanf(fp, "Samples: %u\n", &g_system->data_count);
    
    /* Try to read sample rate info (may not exist in older files) */
    /* Save current file position in case we need to rewind */
    file_pos = ftell(fp);  /* ASSIGN HERE, not declare */
    
    if (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "SampleRateMs: %d", &g_control_panel.sample_rate_ms) == 1) {
            /* Successfully read sample rate, now read rate type */
            if (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "Custom")) {
                    g_control_panel.use_custom = 1;
                    sprintf(g_control_panel.custom_rate, "%d", g_control_panel.sample_rate_ms);
                } else {
                    g_control_panel.use_custom = 0;
                    /* Try to read preset index */
                    if (fgets(line, sizeof(line), fp)) {
                        if (sscanf(line, "PresetIndex: %d", &g_control_panel.selected_rate) == 1) {
                            /* Validate preset index */
                            if (g_control_panel.selected_rate < 0 || 
                                g_control_panel.selected_rate >= NUM_RATE_PRESETS) {
                                g_control_panel.selected_rate = 2; /* Default to 500ms */
                            }
                        }
                    }
                }
            }
            /* Read the "Modules:" line */
            fgets(line, sizeof(line), fp);
        } else {
            /* Old file format - rewind to saved position */
            fseek(fp, file_pos, SEEK_SET);
            /* Set default sample rate for old files */
            g_control_panel.sample_rate_ms = 500;
            g_control_panel.selected_rate = 2;
            g_control_panel.use_custom = 0;
            sprintf(g_control_panel.custom_rate, "500");
            /* Read the "Modules:" line */
            fgets(line, sizeof(line), fp);
        }
    }
    
    /* Clear all modules first */
    for (slot = 0; slot < 8; slot++) {
        g_system->modules[slot].enabled = 0;
        g_system->modules[slot].module_type = MOD_NONE;
        g_system->modules[slot].gpib_address = 0;
    }
    
    /* Read module configuration */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Data:", 5) == 0) break;
        
        if (sscanf(line, "%d,%d,%d,%[^\n]", &slot, &type, &addr, desc) == 4) {
            g_system->modules[slot].module_type = type;
            g_system->modules[slot].gpib_address = addr;
            g_system->modules[slot].enabled = 1;
            strcpy(g_system->modules[slot].description, desc);
        }
        else if (sscanf(line, "%d,%d,%d", &slot, &type, &addr) == 3) {
            g_system->modules[slot].module_type = type;
            g_system->modules[slot].gpib_address = addr;
            g_system->modules[slot].enabled = 1;
            
            switch(type) {
                case MOD_DC5009: strcpy(g_system->modules[slot].description, "DC5009 Counter"); break;
                case MOD_DM5010: strcpy(g_system->modules[slot].description, "DM5010 DMM"); break;
                case MOD_DM5120: strcpy(g_system->modules[slot].description, "DM5120 DMM"); break;
                case MOD_PS5004: strcpy(g_system->modules[slot].description, "PS5004 Power"); break;
                case MOD_PS5010: strcpy(g_system->modules[slot].description, "PS5010 Power"); break;
                case MOD_DC5010: strcpy(g_system->modules[slot].description, "DC5010 Counter"); break;
                case MOD_FG5010: strcpy(g_system->modules[slot].description, "FG5010 Func Gen"); break;
                default: strcpy(g_system->modules[slot].description, "Unknown"); break;
            }
        }
    }
    
    /* Read data values */
    g_system->data_count = 0;
    while (fscanf(fp, "%f", &g_system->data_buffer[g_system->data_count]) == 1) {
        g_system->data_count++;
        if (g_system->data_count >= g_system->buffer_size) break;
    }
    
    fclose(fp);
    
    /* Update the control panel state */
	/* Sample rate is already loaded directly from file */
	/* No update needed */
    
    /* Display what was loaded */
    printf("Data loaded successfully.\n");
    printf("Samples: %u\n", g_system->data_count);
    printf("Sample rate: %d ms", g_control_panel.sample_rate_ms);
    if (g_control_panel.use_custom) {
        printf(" (custom)\n");
    } else {
        printf(" (preset: %s)\n", sample_rate_labels[g_control_panel.selected_rate]);
    }
    
    printf("Press any key to continue...");
    getch();
}

void display_error(char *msg) {
    strcpy(g_error_msg, msg);
    gotoxy(1, 24);
    printf("Error: %s", msg);
}

/* Send custom GPIB command */
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
    for (i = 0; i < 8; i++) {
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

/* Debug version of test_dm5120_comm */
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

/* Interactive GPIB terminal mode */
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
    
    /* Clear any pending data first */
    drain_input_buffer();
    
    /* Get initial status - MUST read response */
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Initial Status: %s\n", response);
    }
    
    /* Flush stdin */
    while (getchar() != '\n');
    
    while (!done) {
        printf("\nGPIB> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }
        
        /* Remove newline */
        command[strcspn(command, "\r\n")] = 0;
        
        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }
        
        if (strlen(command) == 0) {
            continue;
        }
        
        /* Special handling for BREAK - use IOCTL like KYBDCTRL */
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
        
        /* Normal command processing */
        printf("Sending: '%s\\r\\n'\n", command);
        strcat(command, "\r\n");
        ieee_write(command);
        
        /* Check if this command expects a response */
        if (command_has_response(command)) {
            printf("Reading response...\n");
            delay(100);
            
            bytes_read = ieee_read(response, sizeof(response)-1);
            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("Response (%d bytes): '%s'\n", bytes_read, response);
                
                /* Hex dump for ENTER commands */
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
            /* Command doesn't expect response, but check for errors */
            delay(50);
            
            /* Do a non-blocking read to see if there's an error */
            bytes_read = read(ieee_in, response, sizeof(response)-1);
            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("Unexpected response (possible error): %s", response);
            }
        }
        
        delay(50);
    }
}

/* Memory allocation */
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
    
    for (i = 0; i < 8; i++) {
        sys->modules[i].enabled = 0;
        sys->modules[i].module_type = MOD_NONE;
        sys->modules[i].slot_number = i;
        sys->modules[i].gpib_address = 0;
        sys->modules[i].last_reading = 0.0;
        sys->modules[i].description[0] = '\0';
        sys->gpib_devices[i] = -1;
        /* ADD THESE THREE LINES HERE FOR V2.2: */
        sys->modules[i].module_data = NULL;
        sys->modules[i].module_data_count = 0;
        sys->modules[i].module_data_size = 0;
    }
    
    return sys;
}

/* GPIB System Initialization - Modified to handle Personal488 quirks */
int init_gpib_system(void) {
    int i, bytes_read;
    char response[256];
    
    /* Open dual handles for Personal488 compatibility */
    printf("Opening IEEE device handles...\n");
    
    ieee_out = open("\\dev\\ieeeout", O_WRONLY | O_BINARY);
    if (ieee_out < 0) {
        printf("Cannot open \\DEV\\IEEEOUT\n");
        return -1;
    }
    
    /* Add delay before opening input device */
    printf("Waiting before opening input device...\n");
    delay(400);  /* 400ms delay to let driver settle */
    
    ieee_in = open("\\dev\\ieeein", O_RDONLY | O_BINARY);
    if (ieee_in < 0) {
        printf("Cannot open \\DEV\\IEEEIN\n");
        close(ieee_out);
        return -1;
    }
    
    /* Set raw mode on handles */
    printf("Setting raw mode...\n");
    rawmode(ieee_out);
    rawmode(ieee_in);  /* May fail on Personal488, that's OK */
    
    /* Send BREAK using IOCTL to reset the driver state */
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
            /* Continue anyway - might not be fatal */
        } else {
            printf("  IOCTL BREAK succeeded\n");
        }
    }
    delay(200);
    
    /* After BREAK, the driver should be in a clean state */
    /* Now we can start issuing commands */
    
    /* Get driver revision - this command ALWAYS generates a response */
    printf("Getting driver info...\n");
    ieee_write("hello\r\n");
    delay(100);
    
    /* Now we MUST read the response to stay in sync */
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("IOTech Driver: %s", response);
    } else {
        printf("Warning: No hello response\n");
        /* If no response, driver might be in bad state */
        /* Try to recover with abort */
        ieee_write("abort\r\n");
        delay(100);
    }
    
    /* Reset the system - but be careful about responses */
    printf("Resetting IEEE system...\n");
    ieee_write("reset\r\n");
    delay(500);  /* Extended delay for reset */
    
    /* Reset command might generate a response on some systems */
    /* Try to read it but don't fail if there isn't one */
    bytes_read = read(ieee_in, response, sizeof(response));
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        printf("Reset response: %s", response);
    }
    /* If no response or error, that's OK - reset often doesn't respond */
    
    /* Clear any pending errors by getting status */
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Status after reset: %s", response);
    }
    
    /* Set FILL ERROR mode to get proper error messages */
    printf("Setting FILL ERROR mode...\n");
    ieee_write("fill error\r\n");
    delay(100);
    
    /* FILL command generates a response that must be read */
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Fill response: %s", response);
    }
    
    /* Final status check */
    printf("Final status check...\n");
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Status: %s", response);
        
        /* Check for errors in status */
        if (strstr(response, "ERROR") || strstr(response, "SEQUENCE")) {
            printf("Note: Status shows errors - attempting to clear...\n");
            
            /* Try abort to clear any errors */
            ieee_write("abort\r\n");
            delay(100);
            
            /* Get status again */
            ieee_write("status\r\n");
            delay(100);
            bytes_read = ieee_read(response, sizeof(response));
            if (bytes_read > 0) {
                printf("Status after abort: %s", response);
            }
        }
    }
    
    /* Initialize module structures */
    for (i = 0; i < 8; i++) {
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
    int menu_line_1, menu_line_2, menu_line_3, menu_line_4, menu_line_5, menu_line_0;
    
    while (!done) {
        clrscr();  /* Clear screen */
        
        /* Force complete screen clear in case clrscr() isn't working properly */
        for (i = 1; i <= 25; i++) {
            gotoxy(1, i);
            clreol();  /* Clear to end of line */
        }
        
        printf("Module Functions Menu\n");
        printf("====================\n\n");
        
        /* Display configured modules */
        printf("Configured Modules:\n");
        printf("------------------\n");
        found = 0;
        line_count = 6;  /* Track current line for mouse regions */
        
        for (i = 0; i < 8; i++) {
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
        
        /* Menu starts after module list */
        line_count += 2;  /* Skip blank line and "Functions:" */
        printf("\n\nFunctions:\n");
        printf("----------\n\n");
        line_count += 2;  /* Account for header and blank line */
        
        /* Set menu item positions for mouse */
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
        printf("4. Test Module Communication\n\n");
        line_count += 2;
        
        menu_line_5 = line_count;
        printf("5. GPIB Terminal Mode\n\n");
        line_count += 2;
        
        menu_line_0 = line_count + 1;
        printf("\n0. Return to Main Menu\n");
        
        printf("\n\nEnter choice: ");
        
        show_mouse();
        
        /* Get input with mouse support */
        choice = 0;
        if (g_mouse.present) {
            while (choice == 0) {
                if (kbhit()) {
                    hide_mouse();
                    choice = getch();
                    break;
                }
                
                /* Check mouse clicks on menu items */
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
                
                for (i = 0; i < 8; i++) {
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
                
                if (slot >= 0 && slot < 8 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_DM5120) {
                    configure_dm5120_advanced(slot);
                } else {
                    printf("Invalid slot or not a DM5120.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                }
                break;
                
            case '3':
                found = 0;
                clrscr();
                printf("PS5004 Power Supply Modules:\n");
                printf("============================\n\n");
                
                for (i = 0; i < 8; i++) {
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
                
                if (slot >= 0 && slot < 8 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_PS5004) {
                    configure_ps5004_advanced(slot);
                } else {
                    printf("Invalid slot or not a PS5004.\n");
                    printf("\nPress any key to continue...");
                    wait_for_input();
                }
                break;
                
            case '4':
                printf("\n\nEnter GPIB address to test: ");
                scanf("%d", &i);
                printf("\nSelect test type:\n");
                printf("1. DM5120 communication test\n");
                printf("2. PS5004 communication test\n");
                printf("3. Generic GPIB test\n");
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
                        gpib_terminal_mode();
                        break;
                }
                break;
                
            case '5':
                gpib_terminal_mode();
                break;
                
            case '0':
            case 27:  /* ESC key */
                done = 1;
                break;
        }
    }
}

/* File Operations Submenu */
/* Updated File Operations Menu */
void file_operations_menu(void) {
    int choice;
    int done = 0;
    int i;
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        
        /* Clear top lines */
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          File Operations\n");
        printf("          ===============\n\n");
        
        printf("  1. Save Data to File\n\n");
        
        printf("  2. Load Data from File\n\n");
        
        printf("  3. Print Report\n\n");
        
        printf("  0. Return to Main Menu\n\n");
        
        printf("  ============================================\n");
        printf("  Data in memory: %u samples\n", g_system->data_count);
        printf("  ============================================\n\n");
        
        printf("  Enter choice: ");
        
        choice = getch();
        
        switch(choice) {
            case '1':
                save_data();
                break;
                
            case '2':
                load_data();
                break;
                
            case '3':
                print_report();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Updated Measurement Menu */
void measurement_menu(void) {
    int choice;
    int done = 0;
    int i, count;
    
    while (!done) {
        clrscr();
        gotoxy(1, 1);
        
        /* Clear top lines */
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          Measurement Operations\n");
        printf("          =====================\n\n");
        
        /* Count modules */
        count = 0;
        for (i = 0; i < 8; i++) {
            if (g_system->modules[i].enabled) count++;
        }
        
        printf("  1. Single Measurement\n\n");
        
        printf("  2. Continuous Measurement\n\n");
        
        printf("  3. Graph Display\n\n");
        
        printf("  0. Return to Main Menu\n\n");
        
        printf("  ============================================\n");
        printf("  Configured modules: ");
        count = 0;
        for (i = 0; i < 8; i++) {
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
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Updated Main Menu with better screen clearing and mouse support */
void main_menu(void) {
    int choice;
    int done = 0;
    int i, active_count;
    
    /* Clear screen before entering loop */
    clrscr();
    delay(50);
    
    while (!done) {
        /* Clear screen and force cursor to top */
        clrscr();
        gotoxy(1, 1);
        
        /* Extra clearing for stubborn text */
        for (i = 0; i < 3; i++) {
            printf("                                                                      \n");
        }
        gotoxy(1, 1);
        
        printf("\n\n");
        printf("          TM5000 CONTROL SYSTEM v2.4\n");
        printf("          ==========================\n\n");
        
        printf("  1. Configure Modules\n\n");
        
        printf("  2. Measurement Operations\n\n");
        
        printf("  3. Module Functions\n\n");
        
        printf("  4. File Operations\n\n");
        
        printf("  0. Exit\n\n");
        
        /* Extended decorative line */
        printf("  ============================================\n");
        printf("  Active Modules: ");
        active_count = 0;
        for (i = 0; i < 8; i++) {
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
        
        /* Get input with mouse support */
        choice = 0;
        if (g_mouse.present) {
            while (choice == 0) {
                if (kbhit()) {
                    hide_mouse();
                    choice = getch();
                    break;
                }
                
                /* Check mouse clicks */
                get_mouse_status();
                if (g_mouse.left_button) {
                    /* Wait for button release */
                    while (g_mouse.left_button) {
                        get_mouse_status();
                        delay(10);
                    }
                    
                    /* Check which line was clicked */
                    if (g_mouse.y == 6) { choice = '1'; break; }
                    if (g_mouse.y == 8) { choice = '2'; break; }
                    if (g_mouse.y == 10) { choice = '3'; break; }
                    if (g_mouse.y == 12) { choice = '4'; break; }
                    if (g_mouse.y == 14) { choice = '0'; break; }
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

/* Fix the transition in main() */
/* In main(), update the transition to main menu: */

/* Cleanup */
void cleanup(void) {
    int i;
    
    /* Take devices out of remote mode */
    for (i = 0; i < 8; i++) {
        if (g_system && g_system->modules[i].enabled) {
            gpib_local(g_system->modules[i].gpib_address);
        }
    }
    /* Free module data buffers */
    for (i = 0; i < 8; i++) {
        free_module_buffer(i);
    }
    /* Close dual handles */
    if (ieee_out >= 0) close(ieee_out);
    if (ieee_in >= 0) close(ieee_in);
    
    /* Free memory */
    if (g_system) {
        if (g_system->data_buffer)
            _ffree(g_system->data_buffer);
        free(g_system);
    }
}

/* Main Program */
int main(void) {
    int i;  /* Declare i at the beginning of main() */
	
	srand((unsigned)time(NULL));
    
    clrscr();
    printf("TM5000 GPIB Control System v2.4\n");
    printf("For Personal488 Driver V2.2 August 1989\n");
    printf("(C) 2025 - For Gridcase 1520\n\n");
    
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
    /* Initialize mouse support */
    if (init_mouse()) {
        printf("Mouse support enabled.\n");
    } else {
        printf("No mouse detected - keyboard only mode.\n");
    }
    printf("\nSystem initialized successfully.\n");
    printf("Note: Personal488 enforces strict command/response pairing\n");
    printf("Press any key to continue...");
    getch();
    
    /* Clear screen thoroughly before entering main menu */
    clrscr();
    gotoxy(1, 1);
    
    /* Print blank lines to overwrite any residual text */
    for (i = 0; i < 5; i++) {
        printf("                                                                      \n");
    }
    
    /* Small delay to ensure screen is cleared */
    delay(100);
    
    main_menu();
    
    cleanup();
    
    clrscr();
    printf("Thank you for using TM5000 Control System.\n");
    
    return 0;
}
