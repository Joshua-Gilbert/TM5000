/*
 * TM5000 GPIB Control System for Gridcase 1520
 * Version 3.4 - Main Program with 1024-Sample Buffer Upgrade
 * C89 compliant
 *
 * Main program entry point and system initialization
 *
 * Recent Fixes and Improvements:
 * - Fixed graph display crashes with comprehensive divide-by-zero protection
 * - Implemented range-based unit switching (V/MV/UV) for proper zoom behavior
 * - Fixed grid line alignment and unit display for instrument-grade scaling
 * - Enhanced initial measurement range from ±10V to ±1000V for full meter coverage
 * - Fixed LF termination support for all DM5120 GPIB communication functions
 * - Improved bottom menu readability with better spacing and simplified text format
 * - Added clean slot readout format (S[n][sample]:XX.XX V) for consistent display
 * - Fixed grid line units to show only in top-left corner when in MV/UV modes
 * - Maintained ultra-precision 1µV zoom capability for DM5120 measurements
 * - Enhanced zoom operations to properly recalculate units based on range size
 * - Fixed unit switching display issue: CGA font only supports uppercase (V/MV/UV not v/mv/µv)
 * - Converted ALL graphics mode text to uppercase for CGA compatibility (menus, units, labels)
 * - Fixed Brother printer scale display issues with new get_print_units() function
 * - Preserved PostScript µ glyph support while maintaining uppercase graphics units
 * - Added extensible print unit system for future units (mA, etc.)
 * - Fixed data loading crashes by adding comprehensive safety checks for NaN/infinite values
 * - Added graph scale validation after data loading to prevent display corruption
 * - Fixed Brother printer legend to show trace average instead of redundant range information
 * - Replaced obsolete "Zoom" with "Units per div" in printer output for clarity
 * - Fixed printed scale range to match actual grid line values exactly (10-division calculation)
 * - Fixed continuous monitor sample count display lag (was showing count-1 during sampling)
 * - Fixed floating-point precision errors in zoom operations causing 401µV instead of 400µV
 * - Added periodic engineering scale snapping every 5 zoom operations to clean precision errors
 * - Maintains zoom flexibility while ensuring clean values (400µV not 401µV) periodically
 * - Enhanced cursor readout: shows "sample#:voltage" format with scaled values but no units
 * - Simplified legend display: active traces show "S[n]:[MODULE]" bright, inactive show "[MODULE]" dark
 * - Removed legend box and sample counts to eliminate graph overlap and improve clarity
 * - CRITICAL FIX: Fixed buffer overflow in sync_traces_with_modules() causing data loading corruption
 * - Fixed array bounds error accessing user_enabled[8] with 10 elements, eliminating "measurement zero" errors
 * - Fixed is_fft_trace[8] buffer overflow in graph display (changed to [10] for all 10 slots)
 * - Fixed slot validation limits: math functions and module commands now accept slots 0-9 (was 0-7)
 * - Improved module command interface: now uses ESC key to exit instead of negative number input
 * - Enhanced precision cleanup: only corrects actual floating-point drift, preserves user zoom choices
 * - Detects when units per division don't match due to accumulation errors (401µV→400µV)
 * - Added mouse support to measurement operations menu for improved usability
 * - Enhanced graph config menu: direct voltage per division setting (e.g. 2.5V, 500mV, 1uV)
 * - Extended zoom range: now supports ultra-precision down to 1µV per division (was 2µV limit)
 * - Users can set any custom voltage scale instead of relying only on zoom keys
 * - All buffer overflow and "measurement zero" corruption issues resolved in v3.0 codebase
 */

#include "tm5000.h"
#include "gpib.h"
#include "graphics.h"
#include "ui.h"
#include "data.h"

/* Global variable definitions */
int ieee_out = -1;  /* Handle for writing to GPIB */
int ieee_in = -1;   /* Handle for reading from GPIB */
int gpib_error = 0;

measurement_system *g_system = NULL;
unsigned char far *video_mem = (unsigned char far *)CGA_MEMORY;
char g_error_msg[80] = "";
dm5120_config g_dm5120_config[10];  /* Global DM5120 configurations */
dm5010_config g_dm5010_config[10];  /* Global DM5010 configurations */
ps5004_config g_ps5004_config[10];  /* Global PS5004 configurations */
ps5010_config g_ps5010_config[10];  /* Global PS5010 configurations */
dc5009_config g_dc5009_config[10];  /* Global DC5009 configurations */
dc5010_config g_dc5010_config[10];  /* Global DC5010 configurations */
fg5010_config g_fg5010_config[10];  /* Global FG5010 configurations */
mouse_state g_mouse = {0, 0, 0, 0, 0}; /* Global Mouse State */
graph_scale g_graph_scale = {-1000.0, 1000.0, 1.0, 1, 10, 0.0, 1.0, 0, 0}; /* Global graph - wide range to top of meter capability */
trace_info g_traces[10];
control_panel_state g_control_panel = {0, 500, 2, "500", 0, 0xFF, 1};
fft_config g_fft_config = {256, 128, 1, 1, 1, 0, 1, 0.0};  /* Default FFT configuration */
int g_has_287 = 0;  /* Math coprocessor flag */

/* Sample rate presets */
int sample_rate_presets[] = {100, 250, 500, 1000, 2000, 5000, 10000};
char *sample_rate_labels[] = {"100ms", "250ms", "500ms", "1s", "2s", "5s", "10s"};

/* Scientific font data is defined in graphics.c to avoid duplicate symbols */

/* Forward declarations for functions needed here */
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

/* Delay function */
void delay(unsigned int milliseconds) {
    clock_t start_time = clock();
    clock_t delay_ticks = (clock_t)milliseconds * CLOCKS_PER_SEC / 1000;
    
    while ((clock() - start_time) < delay_ticks) {
        /* Busy wait */
    }
}

/* Cleanup function */
void cleanup(void) {
    int i;
    
    /* Put all modules in local mode first */
    for (i = 0; i < 10; i++) {
        if (g_system && g_system->modules[i].enabled) {
            gpib_local(g_system->modules[i].gpib_address);
        }
    }
    
    /* Simple v2.9 style cleanup */
    if (ieee_out >= 0) {
        delay(100);  /* Brief delay before closing handles */
    }
    
    /* Free module buffers */
    for (i = 0; i < 10; i++) {
        free_module_buffer(i);
    }
    
    /* Close GPIB handles */
    if (ieee_out >= 0) close(ieee_out);
    if (ieee_in >= 0) close(ieee_in);
    
    /* Free system memory */
    if (g_system) {
        if (g_system->data_buffer)
            _ffree(g_system->data_buffer);
        free(g_system);
    }
}

/* Main program entry point */
int main(void) {
    int i;
    unsigned int control_word;
    unsigned int status_word;
    
    srand((unsigned)time(NULL));
    
    clrscr();
    printf("TM5000 GPIB Control System v" TM5000_VERSION "\n");
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
        printf("High sample rate (1024 samples) with optimized display\n");
    } else {
        printf("No math coprocessor detected - using software floating point\n");
        printf("Extended sampling (1024 samples) with automatic decimation\n");
    }
    g_system = init_measurement_system(MAX_SAMPLES_PER_MODULE);
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
