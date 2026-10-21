/*
 * TM5000 GPIB Control System: entry point, FPU probe, and global definitions.
 */

#include "tm5000.h"
#include "gpib.h"
#include "graphics.h"
#include "ui.h"
#include "data.h"

int ieee_out = -1;  /* Driver488 \DEV\IEEEOUT handle; stays -1 under GRIDGPIB */
int ieee_in = -1;   /* Driver488 \DEV\IEEEIN handle; stays -1 under GRIDGPIB */
int gpib_error = 0;

measurement_system *g_system = NULL;
unsigned char far *video_mem = (unsigned char far *)CGA_MEMORY;
char g_error_msg[64] = "";
/* Per-slot tables (configs, g_traces) are far to keep DGROUP under 64 KB. */
dm5120_config __far g_dm5120_config[10];
dm5010_config __far g_dm5010_config[10];
ps5004_config __far g_ps5004_config[10];
ps5010_config __far g_ps5010_config[10];
dc5009_config __far g_dc5009_config[10];
dc5010_config __far g_dc5010_config[10];
fg5010_config __far g_fg5010_config[10];
/* Positional initializers below follow the struct field order in tm5000.h. */
mouse_state g_mouse = {0, 0, 0, 0, 0, 0};
graph_scale g_graph_scale = {-1000.0, 1000.0, 1.0, 0.0, 1.0, 10, 0, 0, 1, 0};
trace_info __far g_traces[10];
control_panel_state g_control_panel = {"500", 500, 2, 0xFF, 0, 0, 1, 0};
fft_config g_fft_config = {0.0, 256, 128, 1, 0, 1, 1, 1, 0};
int g_has_287 = 0;  /* set by the FPU probe in main() */

/* Sample-interval presets in ms, with matching menu labels */
int __far sample_rate_presets[] = {100, 250, 500, 1000, 2000, 5000, 10000};
char __far *sample_rate_labels[] = {"100ms", "250ms", "500ms", "1s", "2s", "5s", "10s"};


/* Allocates the system struct plus a far buffer of `points` samples; NULL on failure. */
measurement_system* init_measurement_system(unsigned int points) {
    measurement_system *sys = (measurement_system *)malloc(sizeof(measurement_system));
    int i;
    
    if (!sys) return NULL;
    
    memset(sys, 0, sizeof(measurement_system));
    sys->buffer_size = points;
    sys->data_buffer = (double far *)_fmalloc(points * sizeof(double));
    
    if (!sys->data_buffer) {
        free(sys);
        return NULL;
    }
    
    sys->data_count = 0;
    sys->sample_rate = 10;
    
    for (i = 0; i < 10; i++) {
        sys->modules[i].enabled = 0;
        sys->modules[i].module_type = MOD_NONE;
        sys->modules[i].is_result = 0;
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

void delay(unsigned int milliseconds) {
    clock_t start_time = clock();
    clock_t delay_ticks = (clock_t)milliseconds * CLOCKS_PER_SEC / 1000;
    
    while ((clock() - start_time) < delay_ticks) {
        /* Busy wait */
    }
}

void cleanup(void) {
    int i;

    /* Return instruments to local before the driver closes. GRIDGPIB drops REN for
       the whole bus; under Driver488 each instrument slot gets LOCAL. */
    if (!gpib_release_bus()) {
        for (i = 0; i < 10; i++) {
            if (g_system && g_system->modules[i].enabled &&
                !g_system->modules[i].is_result && g_system->modules[i].gpib_address) {
                gpib_local(g_system->modules[i].gpib_address);   /* skip FFT/math result slots */
            }
        }
    }

    /* Reset the mouse driver so the pointer is hidden and our settings dropped on exit */
    if (g_mouse.present) {
        union REGS r;
        r.x.ax = MOUSE_RESET;
        int86(MOUSE_INT, &r, &r);
    }

    if (ieee_out >= 0) {
        delay(200);  /* let the driver settle before closing handles, else it errors */
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
    printf("TM5000 GPIB Control System v" TM5000_VERSION "\n");
    printf("GPIB driver: %s\n", gpib_driver_name);
    printf("(C) 2025 - For Gridcase 1520\n\n");
    g_has_287 = 0;
    control_word = 0;   /* without an FPU FNSTCW stores nothing; stack garbage must not read as 0x037F */

    _asm {
        ; Without a 287 a waiting FPU instruction can hang, so only no-wait
        ; (FN*) forms belong before the probe result is known.
        fnclex

        fninit

        ; FNINIT leaves the control word at 0x037F only if an FPU is present
        fnstcw  control_word
        mov     ax, control_word
        cmp     ax, 0x037F
        jne     no_287

        ; Exercise the FPU once; the compare result itself is unused
        fld1
        fldz
        fcompp
        fstsw   status_word
        mov     ax, status_word
        sahf

        ; Reached without hanging: FPU present
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
        gpib_driver_help();
        cleanup();
        return 1;
    }
    if (init_mouse()) {
        printf("Mouse support enabled.\n");
    } else {
        printf("No mouse detected - keyboard only mode.\n");
    }
    printf("\nSystem initialized successfully.\n");
    printf("%s", gpib_driver_note);
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
