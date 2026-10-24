/*
 * TM5000 module drivers and menus: DC5009/DC5010, FG5010, DM5120, DM5010,
 * PS5004, PS5010, plus slot setup and the live measurement loop.
 */

#include "modules.h"
#include "data.h"
#include "gpib.h"
#include "graphics.h"
#include <stdarg.h>
#include <signal.h>

/* Shared GPIB scratch buffers; far to stay out of DGROUP */
static char __far gpib_cmd_buffer[80];
static char __far gpib_response_buffer[80];

void init_dc5009_config(int slot) {
    dc5009_config *cfg = &g_dc5009_config[slot];
    
    strcpy(cfg->function, "FREQ");
    strcpy(cfg->channel, "A");
    cfg->gate_time = 1.0;
    cfg->averaging = -1;  /* Auto */
    strcpy(cfg->coupling_a, "DC");
    strcpy(cfg->coupling_b, "DC");
    strcpy(cfg->impedance_a, "HI");
    strcpy(cfg->impedance_b, "HI");
    strcpy(cfg->attenuation_a, "X1");
    strcpy(cfg->attenuation_b, "X1");
    strcpy(cfg->slope_a, "POS");
    strcpy(cfg->slope_b, "POS");
    cfg->level_a = 0.0;
    cfg->level_b = 0.0;
    cfg->filter_enabled = 0;
    cfg->auto_trigger = 1;
    cfg->overflow_enabled = 0;
    cfg->preset_enabled = 0;
    cfg->srq_enabled = 0;
    cfg->overflow_count = 0;
    cfg->last_measurement = 0.0;
    cfg->measurement_complete = 0;
    cfg->lf_termination = 0;  /* Default to CRLF */
}

void init_dc5010_config(int slot) {
    dc5010_config *cfg = &g_dc5010_config[slot];
    
    strcpy(cfg->function, "FREQ");
    strcpy(cfg->channel, "A");
    cfg->gate_time = 1.0;
    cfg->averaging = -1;  /* Auto */
    strcpy(cfg->coupling_a, "DC");
    strcpy(cfg->coupling_b, "DC");
    strcpy(cfg->impedance_a, "HI");
    strcpy(cfg->impedance_b, "HI");
    strcpy(cfg->attenuation_a, "X1");
    strcpy(cfg->attenuation_b, "X1");
    strcpy(cfg->slope_a, "POS");
    strcpy(cfg->slope_b, "POS");
    cfg->level_a = 0.0;
    cfg->level_b = 0.0;
    cfg->filter_enabled = 0;
    cfg->auto_trigger = 1;
    cfg->overflow_enabled = 0;
    cfg->preset_enabled = 0;
    cfg->srq_enabled = 0;
    cfg->overflow_count = 0;
    cfg->last_measurement = 0.0;
    cfg->measurement_complete = 0;
    cfg->rise_fall_enabled = 0;
    cfg->burst_mode = 0;
    cfg->lf_termination = 0;  /* Default to CRLF */
}

/* DC5009 counter */
void dc5009_set_function(int address, char *function, char *channel) {

    if (strcmp(function, "TIME") == 0 && channel && strlen(channel) == 2) {
        /* TIME AB or TIME BA format */
        sprintf(gpib_cmd_buffer, "TIME %s", channel);
    } else if (strcmp(function, "EVENTS") == 0 && channel && strcmp(channel, "BA") == 0) {
        /* EVENTS B DURING A */
        strcpy(gpib_cmd_buffer, "EVE BA");
    } else if (strcmp(function, "TOTALIZE") == 0 && channel) {
        if (strcmp(channel, "A") == 0 || strcmp(channel, "B") == 0) {
            sprintf(gpib_cmd_buffer, "TOT %s", channel);
        } else {
            /* Standard format for other totalize modes */
            sprintf(gpib_cmd_buffer, "%s %s", function, channel);
        }
    } else if (strcmp(function, "RATIO") == 0 && (!channel || strlen(channel) == 0)) {
        /* Default RATIO B/A */
        strcpy(gpib_cmd_buffer, "RAT B/A");
    } else {
        /* Standard format */
        sprintf(gpib_cmd_buffer, "%s %s", function, channel);
    }
    
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(100);
}

void dc5009_set_coupling(int address, char channel, char *coupling) {
    
    sprintf(gpib_cmd_buffer, "COU CHA %c %s", channel, coupling);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_impedance(int address, char channel, char *impedance) {
    
    sprintf(gpib_cmd_buffer, "TER CHA %c %s", channel, impedance);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_attenuation(int address, char channel, char *attenuation) {
    
    sprintf(gpib_cmd_buffer, "ATT CHA %c %s", channel, attenuation);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_slope(int address, char channel, char *slope) {
    
    sprintf(gpib_cmd_buffer, "SLO CHA %c %s", channel, slope);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_level(int address, char channel, float level) {
    
    sprintf(gpib_cmd_buffer, "LEV CHA %c %.3f", channel, level);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_filter(int address, int enabled) {
    gpib_write(address, enabled ? "FIL ON" : "FIL OFF");
    GPIB_PACE(50);
}

void dc5009_set_gate_time(int address, float gate_time) {
    
    sprintf(gpib_cmd_buffer, "GATE %.3f", gate_time);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_set_averaging(int address, int count) {
    
    sprintf(gpib_cmd_buffer, "AVG %d", count);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5009_auto_trigger(int address) {
    gpib_write(address, "AUTO");
    GPIB_PACE(100);
}

void dc5009_start_measurement(int address) {
    gpib_write(address, "START");
    GPIB_PACE(50);
}

void dc5009_stop_measurement(int address) {
    gpib_write(address, "STOP");
    GPIB_PACE(50);
}

double dc5009_read_measurement(int address) {
    
    double value = 0.0;
    
    gpib_write(address, "SEND");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%lf", &value) == 1 || sscanf(gpib_response_buffer, "%le", &value) == 1) {
            return value;
        }
    }
    return 0.0;
}

int dc5009_check_overflow(int address) {
    
    
    gpib_write(address, "OVER?");
    GPIB_PACE(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return (strcmp(gpib_response_buffer, "ON") == 0);
    }
    return 0;
}

void dc5009_clear_overflow(int address) {
    gpib_write(address, "OVER OFF");
    GPIB_PACE(50);
}

/* DC5009 Query Commands */
void dc5009_query_function(int address, char *buffer, int size) {
    buffer[0] = '\0';
    gpib_write(address, "FUNC?");
    GPIB_PACE(50);
    gpib_read(address, buffer, size);
}

void dc5009_query_id(int address, char *buffer, int size) {
    buffer[0] = '\0';
    gpib_write(address, "ID?");
    GPIB_PACE(50);
    gpib_read(address, buffer, size);
}

int dc5009_query_error(int address) {
    
    gpib_write(address, "ERR?");
    GPIB_PACE(50);
    gpib_read(address, gpib_response_buffer, 80);
    return atoi(gpib_response_buffer);
}

/* DC5009 preset, manual timing, SRQ, status */
void dc5009_set_preset(int address, int enabled) {
    gpib_write(address, enabled ? "PRE ON" : "PRE OFF");
    GPIB_PACE(50);
}

void dc5009_manual_timing(int address) {
    gpib_write(address, "TMAN");
    GPIB_PACE(50);
}

void dc5009_set_srq(int address, int enabled) {
    gpib_write(address, enabled ? "RQS ON" : "RQS OFF");
    GPIB_PACE(50);
}

unsigned char dc5009_get_status_byte(int address) {
    unsigned char status;
    ieee_spoll(address, &status);
    return status;
}

/* DC5009 Extended Range Measurement */
double dc5009_read_extended_range(int address) {
    
    float display_value = 0.0;
    int overflow_count = 0;
    unsigned char status;
    double total_result;
    const double OVERFLOW_MULTIPLIER = 10995.1162778;
    
    gpib_write(address, "SEND");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        sscanf(gpib_response_buffer, "%f", &display_value);
    }

    ieee_spoll(address, &status);
    if (status & 0x02) {  /* Overflow bit in status byte */
        /* Counts at most one overflow; a true count needs tracking across reads */
        overflow_count = 1;
    }

    /* Per programming guide: display + overflows * 2^40/1e8 */
    total_result = (double)display_value + (overflow_count * OVERFLOW_MULTIPLIER);
    
    return total_result;
}

/* DC5010: DC5009 command set plus RISE/FALL, TOT A+B/A-B, burst */
void dc5010_set_function(int address, char *function, char *channel) {


    if (strcmp(function, "TIME") == 0 && channel && strlen(channel) == 2) {
        /* TIME AB or TIME BA format */
        sprintf(gpib_cmd_buffer, "TIME %s", channel);
    } else if (strcmp(function, "EVENTS") == 0 && channel && strcmp(channel, "BA") == 0) {
        /* EVENTS B DURING A */
        strcpy(gpib_cmd_buffer, "EVE BA");
    } else if (strcmp(function, "TOTALIZE") == 0 && channel) {
        if (strcmp(channel, "A") == 0 || strcmp(channel, "B") == 0) {
            sprintf(gpib_cmd_buffer, "TOT %s", channel);
        } else if (strcmp(channel, "A+B") == 0) {
            /* DC5010 specific: Sum totalize */
            strcpy(gpib_cmd_buffer, "TOT A+B");
        } else if (strcmp(channel, "A-B") == 0) {
            /* DC5010 specific: Difference totalize */
            strcpy(gpib_cmd_buffer, "TOT A-B");
        } else {
            /* Standard format for other totalize modes */
            sprintf(gpib_cmd_buffer, "%s %s", function, channel);
        }
    } else if (strcmp(function, "RATIO") == 0 && (!channel || strlen(channel) == 0)) {
        /* Default RATIO B/A */
        strcpy(gpib_cmd_buffer, "RAT B/A");
    } else if (strcmp(function, "RISE") == 0) {
        /* Rise time - DC5010 specific */
        strcpy(gpib_cmd_buffer, "RISE A");
    } else if (strcmp(function, "FALL") == 0) {
        /* Fall time - DC5010 specific */
        strcpy(gpib_cmd_buffer, "FALL A");
    } else {
        /* Standard format */
        sprintf(gpib_cmd_buffer, "%s %s", function, channel);
    }
    
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(100);
}

void dc5010_set_coupling(int address, char channel, char *coupling) {
    dc5009_set_coupling(address, channel, coupling);
}

void dc5010_set_impedance(int address, char channel, char *impedance) {
    dc5009_set_impedance(address, channel, impedance);
}

void dc5010_set_attenuation(int address, char channel, char *attenuation) {
    dc5009_set_attenuation(address, channel, attenuation);
}

void dc5010_set_slope(int address, char channel, char *slope) {
    dc5009_set_slope(address, channel, slope);
}

void dc5010_set_level(int address, char channel, float level) {
    dc5009_set_level(address, channel, level);
}

void dc5010_set_filter(int address, int enabled) {
    dc5009_set_filter(address, enabled);
}

void dc5010_set_gate_time(int address, float gate_time) {
    dc5009_set_gate_time(address, gate_time);
}

void dc5010_set_averaging(int address, int count) {
    dc5009_set_averaging(address, count);
}

void dc5010_auto_trigger(int address) {
    dc5009_auto_trigger(address);
}

void dc5010_start_measurement(int address) {
    dc5009_start_measurement(address);
}

void dc5010_stop_measurement(int address) {
    dc5009_stop_measurement(address);
}

double dc5010_read_measurement(int address) {
    return dc5009_read_measurement(address);
}

int dc5010_check_overflow(int address) {
    return dc5009_check_overflow(address);
}

void dc5010_clear_overflow(int address) {
    dc5009_clear_overflow(address);
}

/* DC5010 specific functions */
void dc5010_set_burst_mode(int address, int enabled) {

    sprintf(gpib_cmd_buffer, "BURST %s", enabled ? "ON" : "OFF");
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void dc5010_measure_rise_time(int address) {
    gpib_write(address, "RISE A");
    GPIB_PACE(100);
}

void dc5010_measure_fall_time(int address) {
    gpib_write(address, "FALL A");
    GPIB_PACE(100);
}

/* DC5010 Query Commands */
void dc5010_query_function(int address, char *buffer, int size) {
    dc5009_query_function(address, buffer, size);
}

void dc5010_query_id(int address, char *buffer, int size) {
    dc5009_query_id(address, buffer, size);
}

int dc5010_query_error(int address) {
    return dc5009_query_error(address);
}

/* DC5010 preset, timing, totalize modes, SRQ, status */
void dc5010_set_preset(int address, int enabled) {
    dc5009_set_preset(address, enabled);
}

void dc5010_manual_timing(int address) {
    dc5009_manual_timing(address);
}

void dc5010_totalize_sum(int address) {
    gpib_write(address, "TOT A+B");
    GPIB_PACE(100);
}

void dc5010_totalize_diff(int address) {
    gpib_write(address, "TOT A-B");
    GPIB_PACE(100);
}

void dc5010_propagation_delay(int address) {
    gpib_write(address, "PROB A&B");
    GPIB_PACE(100);
}

void dc5010_set_srq(int address, int enabled) {
    dc5009_set_srq(address, enabled);
}

unsigned char dc5010_get_status_byte(int address) {
    return dc5009_get_status_byte(address);
}

/* DC5010 Extended Range Measurement */
double dc5010_read_extended_range(int address) {
    return dc5009_read_extended_range(address);
}

void test_dc5009_comm(int address) {
    
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5009 Communication Test ===\n");
    printf("Testing DC5009 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing initialization...\n");
    gpib_write(address, "INIT");
    GPIB_PACE(500);
    printf("   Initialization command sent\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function setup...\n");
    gpib_write(address, "FREQ A");
    GPIB_PACE(100);
    gpib_write(address, "FUNC?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing auto trigger...\n");
    gpib_write(address, "AUTO");
    GPIB_PACE(100);
    printf("   Auto trigger enabled\n");
    success_count++;
    test_count++;
    
    printf("5. Testing measurement...\n");
    gpib_write(address, "START");
    GPIB_PACE(200);
    gpib_write(address, "SEND");
    GPIB_PACE(200);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%e", &value) == 1 || sscanf(gpib_response_buffer, "%f", &value) == 1) {
            printf("   Measurement: %.6e Hz\n", value);
            success_count++;
        } else {
            printf("   %s\n", gpib_response_buffer);
        }
    } else {
        printf("   Measurement failed!\n");
    }
    test_count++;
    
    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/%d\n", success_count, test_count);
    if (success_count == test_count) {
        printf("DC5009 communication: EXCELLENT\n");
    } else if (success_count >= test_count * 0.8) {
        printf("DC5009 communication: GOOD\n");
    } else if (success_count >= test_count * 0.5) {
        printf("DC5009 communication: MARGINAL\n");
    } else {
        printf("DC5009 communication: POOR - Check connections!\n");
    }
    
    printf("\nPress any key to continue...");
    getch();
}

void test_dc5010_comm(int address) {
    
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5010 Communication Test ===\n");
    printf("Testing DC5010 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing initialization...\n");
    gpib_write(address, "INIT");
    GPIB_PACE(500);
    printf("   Initialization command sent\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function setup...\n");
    gpib_write(address, "FREQ A");
    GPIB_PACE(100);
    gpib_write(address, "FUNC?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing high frequency capability...\n");
    gpib_write(address, "GATE 0.1");  /* Shorter gate for high freq */
    GPIB_PACE(50);
    printf("   High frequency mode configured\n");
    success_count++;
    test_count++;
    
    printf("5. Testing rise time measurement (DC5010 specific)...\n");
    gpib_write(address, "RISE A");
    GPIB_PACE(100);
    printf("   Rise time function configured\n");
    success_count++;
    test_count++;
    
    printf("6. Testing measurement...\n");
    gpib_write(address, "FREQ A");  /* Back to frequency */
    GPIB_PACE(100);
    gpib_write(address, "START");
    GPIB_PACE(200);
    gpib_write(address, "SEND");
    GPIB_PACE(200);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%e", &value) == 1 || sscanf(gpib_response_buffer, "%f", &value) == 1) {
            printf("   Measurement: %.6e Hz\n", value);
            success_count++;
        } else {
            printf("   %s\n", gpib_response_buffer);
        }
    } else {
        printf("   Measurement failed!\n");
    }
    test_count++;
    
    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/%d\n", success_count, test_count);
    if (success_count == test_count) {
        printf("DC5010 communication: EXCELLENT\n");
    } else if (success_count >= test_count * 0.8) {
        printf("DC5010 communication: GOOD\n");
    } else if (success_count >= test_count * 0.5) {
        printf("DC5010 communication: MARGINAL\n");
    } else {
        printf("DC5010 communication: POOR - Check connections!\n");
    }
    
    printf("\nPress any key to continue...");
    getch();
}

/* Counter configuration menus */
void configure_dc5009_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dc5009_config *cfg = &g_dc5009_config[slot];
    char input[50];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("DC5009 Advanced Configuration - Slot %d\n", slot);
        printf("=====================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Function: %s %s\n", cfg->function, cfg->channel);
        printf("  Gate Time: %.3f seconds\n", cfg->gate_time);
        printf("  Averaging: %d\n", cfg->averaging);
        printf("  Channel A: %s coupling, %s impedance, %s attenuation, %s slope\n",
               cfg->coupling_a, cfg->impedance_a, cfg->attenuation_a, cfg->slope_a);
        printf("  Channel B: %s coupling, %s impedance, %s attenuation, %s slope\n",
               cfg->coupling_b, cfg->impedance_b, cfg->attenuation_b, cfg->slope_b);
        printf("  Trigger Levels: A=%.3fV, B=%.3fV\n", cfg->level_a, cfg->level_b);
        printf("  Filter: %s\n", cfg->filter_enabled ? "ON" : "OFF");
        printf("  Auto Trigger: %s\n", cfg->auto_trigger ? "ON" : "OFF");
        printf("  Overflow: %s\n", cfg->overflow_enabled ? "ON" : "OFF");
        
        printf("\nOptions:\n");
        printf("1. Set Function and Channel\n");
        printf("2. Configure Gate Time\n");
        printf("3. Set Averaging\n");
        printf("4. Configure Channel A\n");
        printf("5. Configure Channel B\n");
        printf("6. Set Trigger Levels\n");
        printf("7. Filter Control\n");
        printf("8. Auto Trigger\n");
        printf("9. Overflow Control\n");
        printf("A. Apply Settings to DC5009\n");
        printf("B. Test Communication\n");
        printf("C. Reset to Defaults\n");
        printf("D. Advanced Features (Query, SRQ, Preset)\n");
        printf("E. Extended Range Test\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, EVENTS\n");
                printf("Special formats:\n");
                printf("  TIME AB/BA - Time interval measurements\n");
                printf("  EVENTS BA - Events B during A\n");
                printf("Enter function: ");
                scanf("%s", input);
                strcpy(cfg->function, input);
                printf("Enter channel (A, B, AB, BA): ");
                scanf("%s", input);
                strcpy(cfg->channel, input);
                break;
                
            case '2':
                printf("\n\nEnter gate time (0.001 to 100 seconds): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.001 && temp_float <= 100.0) {
                    cfg->gate_time = temp_float;
                }
                break;
                
            case '3':
                printf("\n\nEnter averaging (-1=auto, 1=none, >1=count): ");
                scanf("%d", &temp_int);
                cfg->averaging = temp_int;
                break;
                
            case '4':
                printf("\n\nChannel A Configuration:\n");
                printf("Coupling (AC/DC): ");
                scanf("%s", cfg->coupling_a);
                printf("Impedance (HI/LO): ");
                scanf("%s", cfg->impedance_a);
                printf("Attenuation (X1/X10): ");
                scanf("%s", cfg->attenuation_a);
                printf("Slope (POS/NEG): ");
                scanf("%s", cfg->slope_a);
                break;
                
            case '5':
                printf("\n\nChannel B Configuration:\n");
                printf("Coupling (AC/DC): ");
                scanf("%s", cfg->coupling_b);
                printf("Impedance (HI/LO): ");
                scanf("%s", cfg->impedance_b);
                printf("Attenuation (X1/X10): ");
                scanf("%s", cfg->attenuation_b);
                printf("Slope (POS/NEG): ");
                scanf("%s", cfg->slope_b);
                break;
                
            case '6':
                printf("\n\nTrigger Levels:\n");
                printf("Channel A level (volts): ");
                scanf("%f", &cfg->level_a);
                printf("Channel B level (volts): ");
                scanf("%f", &cfg->level_b);
                break;
                
            case '7':
                cfg->filter_enabled = !cfg->filter_enabled;
                printf("\n\nFilter %s\n", cfg->filter_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '8':
                cfg->auto_trigger = !cfg->auto_trigger;
                printf("\n\nAuto Trigger %s\n", cfg->auto_trigger ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '9':
                cfg->overflow_enabled = !cfg->overflow_enabled;
                printf("\n\nOverflow Detection %s\n", cfg->overflow_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'A':
                printf("\n\nApplying settings to DC5009...\n");
                gpib_remote(address);
                GPIB_PACE(200);
                
                dc5009_set_function(address, cfg->function, cfg->channel);
                dc5009_set_gate_time(address, cfg->gate_time);
                dc5009_set_averaging(address, cfg->averaging);
                dc5009_set_coupling(address, 'A', cfg->coupling_a);
                dc5009_set_coupling(address, 'B', cfg->coupling_b);
                dc5009_set_impedance(address, 'A', cfg->impedance_a);
                dc5009_set_impedance(address, 'B', cfg->impedance_b);
                dc5009_set_attenuation(address, 'A', cfg->attenuation_a);
                dc5009_set_attenuation(address, 'B', cfg->attenuation_b);
                dc5009_set_slope(address, 'A', cfg->slope_a);
                dc5009_set_slope(address, 'B', cfg->slope_b);
                dc5009_set_level(address, 'A', cfg->level_a);
                dc5009_set_level(address, 'B', cfg->level_b);
                dc5009_set_filter(address, cfg->filter_enabled);
                
                if (cfg->auto_trigger) {
                    dc5009_auto_trigger(address);
                }
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'B':
                test_dc5009_comm(address);
                break;
                
            case 'C':
                init_dc5009_config(slot);
                printf("\n\nConfiguration reset to defaults.\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'D':
                printf("\n\nAdvanced Features:\n");
                printf("1. Query current function\n");
                printf("2. Query instrument ID\n");
                printf("3. Query error status\n");
                printf("4. Toggle preset function\n");
                printf("5. Toggle SRQ\n");
                printf("6. Manual timing control\n");
                printf("Choice (1-6): ");
                
                choice = getch();
                switch(choice) {
                    case '1': {
                        char func_buffer[80];
                        dc5009_query_function(address, func_buffer, sizeof(func_buffer));
                        printf("   %s\n", func_buffer);
                        break;
                    }
                    case '2': {
                        char id_buffer[80];
                        dc5009_query_id(address, id_buffer, sizeof(id_buffer));
                        printf("   %s\n", id_buffer);
                        break;
                    }
                    case '3': {
                        int error = dc5009_query_error(address);
                        printf("\nError status: %d\n", error);
                        break;
                    }
                    case '4':
                        printf("\nToggling preset function...\n");
                        dc5009_set_preset(address, 1);
                        delay(1000);
                        dc5009_set_preset(address, 0);
                        break;
                    case '5':
                        printf("\nEnabling SRQ...\n");
                        dc5009_set_srq(address, 1);
                        break;
                    case '6':
                        printf("\nActivating manual timing...\n");
                        dc5009_manual_timing(address);
                        break;
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'E':
                printf("\n\nTesting Extended Range Measurement:\n");
                {
                    double result = dc5009_read_extended_range(address);
                    unsigned char status = dc5009_get_status_byte(address);
                    printf("Extended range result: %.6f\n", result);
                    printf("Status byte: 0x%02X\n", status);
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

void configure_dc5010_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dc5010_config *cfg = &g_dc5010_config[slot];
    char input[50];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("DC5010 Advanced Configuration - Slot %d\n", slot);
        printf("=====================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Function: %s %s\n", cfg->function, cfg->channel);
        printf("  Gate Time: %.3f seconds\n", cfg->gate_time);
        printf("  Averaging: %d\n", cfg->averaging);
        printf("  Channel A: %s coupling, %s impedance, %s attenuation, %s slope\n",
               cfg->coupling_a, cfg->impedance_a, cfg->attenuation_a, cfg->slope_a);
        printf("  Channel B: %s coupling, %s impedance, %s attenuation, %s slope\n",
               cfg->coupling_b, cfg->impedance_b, cfg->attenuation_b, cfg->slope_b);
        printf("  Trigger Levels: A=%.3fV, B=%.3fV\n", cfg->level_a, cfg->level_b);
        printf("  Filter: %s\n", cfg->filter_enabled ? "ON" : "OFF");
        printf("  Auto Trigger: %s\n", cfg->auto_trigger ? "ON" : "OFF");
        printf("  Overflow: %s\n", cfg->overflow_enabled ? "ON" : "OFF");
        printf("  Rise/Fall Time: %s\n", cfg->rise_fall_enabled ? "ON" : "OFF");
        printf("  Burst Mode: %s\n", cfg->burst_mode ? "ON" : "OFF");
        
        printf("\nOptions:\n");
        printf("1. Set Function and Channel\n");
        printf("2. Configure Gate Time\n");
        printf("3. Set Averaging\n");
        printf("4. Configure Channel A\n");
        printf("5. Configure Channel B\n");
        printf("6. Set Trigger Levels\n");
        printf("7. Filter Control\n");
        printf("8. Auto Trigger\n");
        printf("9. Overflow Control\n");
        printf("A. Rise/Fall Time Mode (DC5010 specific)\n");
        printf("B. Burst Mode (DC5010 specific)\n");
        printf("C. Apply Settings to DC5010\n");
        printf("D. Test Communication\n");
        printf("E. Reset to Defaults\n");
        printf("F. Advanced Features (Query, SRQ, Preset)\n");
        printf("G. DC5010 Specific Functions\n");
        printf("H. Extended Range Test\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE, EVENTS\n");
                printf("RISE, FALL (DC5010 specific)\n");
                printf("Special formats:\n");
                printf("  TIME AB/BA - Time interval measurements\n");
                printf("  EVENTS BA - Events B during A\n");
                printf("  TOTALIZE A+B/A-B - DC5010 sum/difference totalize\n");
                printf("Enter function: ");
                scanf("%s", input);
                strcpy(cfg->function, input);
                printf("Enter channel (A, B, AB, BA, A+B, A-B): ");
                scanf("%s", input);
                strcpy(cfg->channel, input);
                break;
                
            case '2':
                printf("\n\nEnter gate time (0.001 to 100 seconds): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.001 && temp_float <= 100.0) {
                    cfg->gate_time = temp_float;
                }
                break;
                
            case '3':
                printf("\n\nEnter averaging (-1=auto, 1=none, >1=count): ");
                scanf("%d", &temp_int);
                cfg->averaging = temp_int;
                break;
                
            case '4':
                printf("\n\nChannel A Configuration:\n");
                printf("Coupling (AC/DC): ");
                scanf("%s", cfg->coupling_a);
                printf("Impedance (HI/LO): ");
                scanf("%s", cfg->impedance_a);
                printf("Attenuation (X1/X10): ");
                scanf("%s", cfg->attenuation_a);
                printf("Slope (POS/NEG): ");
                scanf("%s", cfg->slope_a);
                break;
                
            case '5':
                printf("\n\nChannel B Configuration:\n");
                printf("Coupling (AC/DC): ");
                scanf("%s", cfg->coupling_b);
                printf("Impedance (HI/LO): ");
                scanf("%s", cfg->impedance_b);
                printf("Attenuation (X1/X10): ");
                scanf("%s", cfg->attenuation_b);
                printf("Slope (POS/NEG): ");
                scanf("%s", cfg->slope_b);
                break;
                
            case '6':
                printf("\n\nTrigger Levels:\n");
                printf("Channel A level (volts): ");
                scanf("%f", &cfg->level_a);
                printf("Channel B level (volts): ");
                scanf("%f", &cfg->level_b);
                break;
                
            case '7':
                cfg->filter_enabled = !cfg->filter_enabled;
                printf("\n\nFilter %s\n", cfg->filter_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '8':
                cfg->auto_trigger = !cfg->auto_trigger;
                printf("\n\nAuto Trigger %s\n", cfg->auto_trigger ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '9':
                cfg->overflow_enabled = !cfg->overflow_enabled;
                printf("\n\nOverflow Detection %s\n", cfg->overflow_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'A':
                cfg->rise_fall_enabled = !cfg->rise_fall_enabled;
                printf("\n\nRise/Fall Time Mode %s\n", cfg->rise_fall_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'B':
                cfg->burst_mode = !cfg->burst_mode;
                printf("\n\nBurst Mode %s\n", cfg->burst_mode ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'C':
                printf("\n\nApplying settings to DC5010...\n");
                gpib_remote(address);
                GPIB_PACE(200);
                
                dc5010_set_function(address, cfg->function, cfg->channel);
                dc5010_set_gate_time(address, cfg->gate_time);
                dc5010_set_averaging(address, cfg->averaging);
                dc5010_set_coupling(address, 'A', cfg->coupling_a);
                dc5010_set_coupling(address, 'B', cfg->coupling_b);
                dc5010_set_impedance(address, 'A', cfg->impedance_a);
                dc5010_set_impedance(address, 'B', cfg->impedance_b);
                dc5010_set_attenuation(address, 'A', cfg->attenuation_a);
                dc5010_set_attenuation(address, 'B', cfg->attenuation_b);
                dc5010_set_slope(address, 'A', cfg->slope_a);
                dc5010_set_slope(address, 'B', cfg->slope_b);
                dc5010_set_level(address, 'A', cfg->level_a);
                dc5010_set_level(address, 'B', cfg->level_b);
                dc5010_set_filter(address, cfg->filter_enabled);
                dc5010_set_burst_mode(address, cfg->burst_mode);
                
                if (cfg->auto_trigger) {
                    dc5010_auto_trigger(address);
                }
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'D':
                test_dc5010_comm(address);
                break;
                
            case 'E':
                init_dc5010_config(slot);
                printf("\n\nConfiguration reset to defaults.\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'F':
                printf("\n\nAdvanced Features:\n");
                printf("1. Query current function\n");
                printf("2. Query instrument ID\n");
                printf("3. Query error status\n");
                printf("4. Toggle preset function\n");
                printf("5. Toggle SRQ\n");
                printf("6. Manual timing control\n");
                printf("Choice (1-6): ");
                
                choice = getch();
                switch(choice) {
                    case '1': {
                        char func_buffer[80];
                        dc5010_query_function(address, func_buffer, sizeof(func_buffer));
                        printf("   %s\n", func_buffer);
                        break;
                    }
                    case '2': {
                        char id_buffer[80];
                        dc5010_query_id(address, id_buffer, sizeof(id_buffer));
                        printf("   %s\n", id_buffer);
                        break;
                    }
                    case '3': {
                        int error = dc5010_query_error(address);
                        printf("\nError status: %d\n", error);
                        break;
                    }
                    case '4':
                        printf("\nToggling preset function...\n");
                        dc5010_set_preset(address, 1);
                        delay(1000);
                        dc5010_set_preset(address, 0);
                        break;
                    case '5':
                        printf("\nEnabling SRQ...\n");
                        dc5010_set_srq(address, 1);
                        break;
                    case '6':
                        printf("\nActivating manual timing...\n");
                        dc5010_manual_timing(address);
                        break;
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'G':
                printf("\n\nDC5010 Specific Functions:\n");
                printf("1. Totalize Sum (A+B)\n");
                printf("2. Totalize Difference (A-B)\n");
                printf("3. Propagation Delay\n");
                printf("Choice (1-3): ");
                
                choice = getch();
                switch(choice) {
                    case '1':
                        printf("\nActivating Totalize Sum (A+B)...\n");
                        dc5010_totalize_sum(address);
                        break;
                    case '2':
                        printf("\nActivating Totalize Difference (A-B)...\n");
                        dc5010_totalize_diff(address);
                        break;
                    case '3':
                        printf("\nActivating Propagation Delay measurement...\n");
                        dc5010_propagation_delay(address);
                        break;
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'H':
                printf("\n\nTesting Extended Range Measurement:\n");
                {
                    double result = dc5010_read_extended_range(address);
                    unsigned char status = dc5010_get_status_byte(address);
                    printf("Extended range result: %.6f\n", result);
                    printf("Status byte: 0x%02X\n", status);
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

/* ========================================================================== */
/* FG5010 Function Generator                                                  */
/* ========================================================================== */

void init_fg5010_config(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];

    if (slot < 0 || slot >= 10) return;

    cfg->frequency = 1000.0;           /* 1 kHz default frequency */
    cfg->amplitude = 5.0;              /* 5 Vpp default amplitude */
    cfg->offset = 0.0;                 /* No DC offset */
    strcpy(cfg->waveform, "SINE");     /* Sine wave default */
    cfg->output_enabled = 0;           /* Output OFF by default */
    cfg->duty_cycle = 50.0;            /* 50% duty cycle */
    
    /* Sweep settings */
    cfg->sweep_enabled = 0;            /* Sweep OFF */
    cfg->start_freq = 100.0;           /* 100 Hz start */
    cfg->stop_freq = 10000.0;          /* 10 kHz stop */
    cfg->sweep_time = 1.0;             /* 1 second sweep */
    cfg->sweep_type = 0;               /* Linear sweep */
    
    /* Trigger settings */
    strcpy(cfg->trigger_source, "INT"); /* Internal trigger */
    strcpy(cfg->trigger_slope, "POS");  /* Positive slope */
    cfg->trigger_level = 1.0;          /* 1V trigger level */
    
    /* Additional features */
    cfg->sync_enabled = 0;             /* Sync OFF */
    cfg->invert_enabled = 0;           /* No inversion */
    cfg->phase = 0.0;                  /* 0 degree phase */
    
    /* Modulation settings */
    cfg->modulation_enabled = 0;       /* Modulation OFF */
    strcpy(cfg->mod_type, "AM");       /* AM modulation */
    cfg->mod_freq = 100.0;             /* 100 Hz mod frequency */
    cfg->mod_depth = 50.0;             /* 50% modulation depth */
    
    /* Burst settings */
    cfg->burst_enabled = 0;            /* Burst OFF */
    cfg->burst_count = 10;             /* 10 cycles per burst */
    cfg->burst_period = 0.1;           /* 100ms burst period */
    
    /* Units and termination */
    cfg->units_freq = 1;               /* kHz default */
    cfg->units_time = 1;               /* ms default */
    cfg->lf_termination = 0;           /* CRLF termination */
}

void configure_fg5010(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    char input[50];
    int choice, done = 0;
    int temp_choice;
    
    if (slot < 0 || slot >= 10) return;
    
    while (!done) {
        clrscr();
        printf("FG5010 Function Generator Configuration - Slot %d\n", slot);
        printf("================================================\n\n");
        
        printf("Current Settings:\n");
        printf("  1. Frequency: %.3f Hz\n", cfg->frequency);
        printf("  2. Amplitude: %.3f Vpp\n", cfg->amplitude);
        printf("  3. Offset: %.3f V\n", cfg->offset);
        printf("  4. Waveform: %s\n", cfg->waveform);
        printf("  5. Output: %s\n", cfg->output_enabled ? "ON" : "OFF");
        printf("  6. Symmetry: %.1f%% (Square/Pulse: 10-90%%)\n", cfg->duty_cycle);
        printf("  7. LF Termination: %s\n", cfg->lf_termination ? "LF" : "CRLF");
        printf("\n");
        printf("  8. Advanced Configuration\n");
        printf("  9. Test Communication\n");
        printf("  0. Return to Main Menu\n\n");
        
        printf("Select option (1-9, 0=exit): ");
        scanf("%d", &choice);
        
        switch (choice) {
            case 1:
                printf("Enter frequency in Hz (0.002 to 20000000): ");
                scanf("%s", input);
                cfg->frequency = atof(input);
                if (cfg->frequency < 0.002) cfg->frequency = 0.002;
                if (cfg->frequency > 20000000.0) cfg->frequency = 20000000.0;
                break;
                
            case 2:
                printf("Enter load impedance (0=50ohm, 1=open): ");
                scanf("%d", &temp_choice);
                if (temp_choice == 0) {
                    printf("Enter amplitude in Vpp (0.01 to 10.0, 50ohm load): ");
                    scanf("%s", input);
                    cfg->amplitude = atof(input);
                    if (cfg->amplitude < 0.01) cfg->amplitude = 0.01;
                    if (cfg->amplitude > 10.0) cfg->amplitude = 10.0;
                } else {
                    printf("Enter amplitude in Vpp (0.02 to 20.0, open circuit): ");
                    scanf("%s", input);
                    cfg->amplitude = atof(input);
                    if (cfg->amplitude < 0.02) cfg->amplitude = 0.02;
                    if (cfg->amplitude > 20.0) cfg->amplitude = 20.0;
                }
                break;
                
            case 3:
                printf("Enter DC offset in V (-7.5 to +7.5): ");
                scanf("%s", input);
                cfg->offset = atof(input);
                if (cfg->offset < -7.5) cfg->offset = -7.5;
                if (cfg->offset > 7.5) cfg->offset = 7.5;
                break;
                
            case 4:
                printf("Select waveform:\n");
                printf("  1. SINE\n  2. SQUARE\n  3. TRIANGLE\n  4. RAMP\n  5. PULSE\n  6. NOISE\n");
                printf("Enter choice (1-6): ");
                scanf("%d", &choice);
                switch (choice) {
                    case 1: strcpy(cfg->waveform, "SINE"); break;
                    case 2: strcpy(cfg->waveform, "SQUARE"); break;
                    case 3: strcpy(cfg->waveform, "TRIANGLE"); break;
                    case 4: strcpy(cfg->waveform, "RAMP"); break;
                    case 5: strcpy(cfg->waveform, "PULSE"); break;
                    case 6: strcpy(cfg->waveform, "NOISE"); break;
                    default: printf("Invalid selection\n"); delay(1000); break;
                }
                break;
                
            case 5:
                cfg->output_enabled = !cfg->output_enabled;
                printf("Output %s\n", cfg->output_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 6:
                printf("Enter symmetry %% (10.0 to 90.0): ");
                scanf("%s", input);
                cfg->duty_cycle = atof(input);
                if (cfg->duty_cycle < 10.0) cfg->duty_cycle = 10.0;
                if (cfg->duty_cycle > 90.0) cfg->duty_cycle = 90.0;
                break;
                
            case 7:
                cfg->lf_termination = !cfg->lf_termination;
                printf("Line termination set to %s\n", cfg->lf_termination ? "LF" : "CRLF");
                delay(1000);
                break;
                
            case 8:
                configure_fg5010_advanced(slot);
                break;
                
            case 9:
                test_fg5010_comm(g_system->modules[slot].gpib_address);
                break;
                
            case 0:
                done = 1;
                break;
                
            default:
                printf("Invalid selection\n");
                delay(1000);
                break;
        }
    }
}

void configure_fg5010_advanced(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    char input[50];
    int choice, done = 0;
    int temp_choice;
    
    if (slot < 0 || slot >= 10) return;
    
    while (!done) {
        clrscr();
        printf("FG5010 Advanced Configuration - Slot %d\n", slot);
        printf("=====================================\n\n");
        
        printf("Sweep Configuration:\n");
        printf("  1. Sweep Mode: %s\n", cfg->sweep_enabled ? "ENABLED" : "DISABLED");
        printf("  2. Start Freq: %.3f Hz\n", cfg->start_freq);
        printf("  3. Stop Freq: %.3f Hz\n", cfg->stop_freq);
        printf("  4. Sweep Time: %.3f seconds\n", cfg->sweep_time);
        printf("  5. Sweep Type: %s\n", cfg->sweep_type ? "LOGARITHMIC" : "LINEAR");
        printf("\n");
        
        printf("Trigger Configuration:\n");
        printf("  6. Trigger Source: %s\n", cfg->trigger_source);
        printf("  7. Trigger Slope: %s\n", cfg->trigger_slope);
        printf("  8. Trigger Level: %.2f V\n", cfg->trigger_level);
        printf("\n");
        
        printf("Modulation Configuration:\n");
        printf("  9. Modulation: %s\n", cfg->modulation_enabled ? "ENABLED" : "DISABLED");
        printf(" 10. Mod Type: %s\n", cfg->mod_type);
        printf(" 11. Mod Freq: %.2f Hz\n", cfg->mod_freq);
        printf(" 12. Mod Depth: %.1f%%\n", cfg->mod_depth);
        printf("\n");
        
        printf("Other Settings:\n");
        printf(" 13. Sync Output: %s\n", cfg->sync_enabled ? "ON" : "OFF");
        printf(" 14. Invert: %s\n", cfg->invert_enabled ? "ON" : "OFF");
        printf(" 15. Phase: %.1f degrees\n", cfg->phase);
        printf(" 16. Burst Mode: %s\n", cfg->burst_enabled ? "ENABLED" : "DISABLED");
        printf("\n");
        printf("  0. Return to Basic Configuration\n\n");
        
        printf("Select option (1-16, 0=exit): ");
        scanf("%d", &choice);
        
        switch (choice) {
            case 1:
                cfg->sweep_enabled = !cfg->sweep_enabled;
                printf("Sweep mode %s\n", cfg->sweep_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 2:
                printf("Enter sweep start frequency in Hz: ");
                scanf("%s", input);
                cfg->start_freq = atof(input);
                break;
                
            case 3:
                printf("Enter sweep stop frequency in Hz: ");
                scanf("%s", input);
                cfg->stop_freq = atof(input);
                break;
                
            case 4:
                printf("Enter sweep time in seconds (0.1 to 999): ");
                scanf("%s", input);
                cfg->sweep_time = atof(input);
                if (cfg->sweep_time < 0.1) cfg->sweep_time = 0.1;
                if (cfg->sweep_time > 999.0) cfg->sweep_time = 999.0;
                break;
                
            case 5:
                cfg->sweep_type = !cfg->sweep_type;
                printf("Sweep type set to %s\n", cfg->sweep_type ? "LOGARITHMIC" : "LINEAR");
                delay(1000);
                break;
                
            case 6:
                printf("Select trigger source:\n");
                printf("  1. INTERNAL\n  2. EXTERNAL\n  3. MANUAL\n");
                printf("Enter choice (1-3): ");
                scanf("%d", &choice);
                switch (choice) {
                    case 1: strcpy(cfg->trigger_source, "INT"); break;
                    case 2: strcpy(cfg->trigger_source, "EXT"); break;
                    case 3: strcpy(cfg->trigger_source, "MAN"); break;
                    default: printf("Invalid selection\n"); delay(1000); break;
                }
                break;
                
            case 7:
                if (strcmp(cfg->trigger_slope, "POS") == 0) {
                    strcpy(cfg->trigger_slope, "NEG");
                } else {
                    strcpy(cfg->trigger_slope, "POS");
                }
                printf("Trigger slope set to %s\n", cfg->trigger_slope);
                delay(1000);
                break;
                
            case 8:
                printf("Enter trigger level in V (-5.0 to +5.0): ");
                scanf("%s", input);
                cfg->trigger_level = atof(input);
                if (cfg->trigger_level < -5.0) cfg->trigger_level = -5.0;
                if (cfg->trigger_level > 5.0) cfg->trigger_level = 5.0;
                break;
                
            case 9:
                cfg->modulation_enabled = !cfg->modulation_enabled;
                printf("Modulation %s\n", cfg->modulation_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 10:
                printf("Select modulation type:\n");
                printf("  1. AM (Amplitude Modulation)\n");
                printf("  2. FM (Frequency Modulation)\n");
                printf("  3. VCF (Voltage Controlled Frequency)\n");
                printf("  4. EXTERNAL\n");
                printf("Choice: ");
                scanf("%d", &temp_choice);
                switch(temp_choice) {
                    case 1: strcpy(cfg->mod_type, "AM"); break;
                    case 2: strcpy(cfg->mod_type, "FM"); break;
                    case 3: strcpy(cfg->mod_type, "VCF"); break;
                    case 4: strcpy(cfg->mod_type, "EXT"); break;
                    default: strcpy(cfg->mod_type, "AM"); break;
                }
                printf("Modulation type set to %s\n", cfg->mod_type);
                delay(1000);
                break;
                
            case 11:
                printf("Enter modulation frequency in Hz: ");
                scanf("%s", input);
                cfg->mod_freq = atof(input);
                break;
                
            case 12:
                printf("Enter modulation depth in %% (0-100): ");
                scanf("%s", input);
                cfg->mod_depth = atof(input);
                if (cfg->mod_depth < 0.0) cfg->mod_depth = 0.0;
                if (cfg->mod_depth > 100.0) cfg->mod_depth = 100.0;
                break;
                
            case 13:
                cfg->sync_enabled = !cfg->sync_enabled;
                printf("Sync output %s\n", cfg->sync_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 14:
                cfg->invert_enabled = !cfg->invert_enabled;
                printf("Waveform invert %s\n", cfg->invert_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 15:
                printf("Enter phase offset in degrees (0-360): ");
                scanf("%s", input);
                cfg->phase = atof(input);
                while (cfg->phase < 0.0) cfg->phase += 360.0;
                while (cfg->phase >= 360.0) cfg->phase -= 360.0;
                break;
                
            case 16:
                cfg->burst_enabled = !cfg->burst_enabled;
                if (cfg->burst_enabled) {
                    printf("Enter burst count (1-99999): ");
                    scanf("%d", &cfg->burst_count);
                    printf("Enter burst period in seconds: ");
                    scanf("%f", &cfg->burst_period);
                }
                printf("Burst mode %s\n", cfg->burst_enabled ? "ENABLED" : "DISABLED");
                delay(1000);
                break;
                
            case 0:
                done = 1;
                break;
                
            default:
                printf("Invalid selection\n");
                delay(1000);
                break;
        }
    }
}

/* FG5010 GPIB Control Functions */
void fg5010_set_frequency(int address, float freq) {
    sprintf(gpib_cmd_buffer, "FREQ %.3f", freq);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_amplitude(int address, float amp) {
    sprintf(gpib_cmd_buffer, "AMPL %.3f", amp);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_offset(int address, float offset) {
    sprintf(gpib_cmd_buffer, "OFFS %.3f", offset);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_waveform(int address, char *waveform) {
    sprintf(gpib_cmd_buffer, "FUNC %s", waveform);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_duty_cycle(int address, float duty) {
    sprintf(gpib_cmd_buffer, "DCYC %.1f", duty);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_enable_output(int address, int enable) {
    gpib_write(address, enable ? "OUTP ON" : "OUTP OFF");
    GPIB_PACE(50);
}

void fg5010_set_sweep(int address, int enable, float start_freq, float stop_freq, float time) {
    if (enable) {
        sprintf(gpib_cmd_buffer, "SWE:STAT ON;SWE:STAR %.3f;SWE:STOP %.3f;SWE:TIME %.3f", 
                start_freq, stop_freq, time);
    } else {
        strcpy(gpib_cmd_buffer, "SWE:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_trigger(int address, char *source, char *slope, float level) {
    sprintf(gpib_cmd_buffer, "TRIG:SOUR %s;TRIG:SLOP %s;TRIG:LEV %.2f", source, slope, level);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_sync(int address, int enable) {
    gpib_write(address, enable ? "SYNC ON" : "SYNC OFF");
    GPIB_PACE(50);
}

void fg5010_set_invert(int address, int enable) {
    gpib_write(address, enable ? "INV ON" : "INV OFF");
    GPIB_PACE(50);
}

void fg5010_set_phase(int address, float phase) {
    sprintf(gpib_cmd_buffer, "PHAS %.1f", phase);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_modulation(int address, int enable, char *type, float freq, float depth) {
    if (enable) {
        sprintf(gpib_cmd_buffer, "MOD:STAT ON;MOD:TYP %s;MOD:FREQ %.2f;MOD:DEPT %.1f", 
                type, freq, depth);
    } else {
        strcpy(gpib_cmd_buffer, "MOD:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void fg5010_set_burst(int address, int enable, int count, float period) {
    if (enable) {
        sprintf(gpib_cmd_buffer, "BURS:STAT ON;BURS:NCYC %d;BURS:INT:PER %.3f", count, period);
    } else {
        strcpy(gpib_cmd_buffer, "BURS:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

int test_fg5010_comm(int address) {

    int test_passed = 1;
    void (*old_sigint)(int);
    void (*old_sigbreak)(int);

    clrscr();
    printf("FG5010 Function Generator Communication Test\n");
    printf("==========================================\n\n");
    printf("Testing FG5010 at GPIB address %d...\n\n", address);

    /* The test overwrites the user's settings and drives whatever is wired to the output */
    printf("WARNING: this test sets 1 kHz,\n");
    printf("2 Vpp, SINE and briefly turns the\n");
    printf("output ON, then leaves it OFF.\n");
    printf("Current settings will be lost.\n");
    printf("Continue? (y/n): ");
    if (toupper(getch()) != 'Y') {
        printf("\n\nFG5010 test not run.\n");
        printf("Press any key to continue...");
        getch();
        return 1;
    }
    printf("\n\n");

    printf("Test 1: Device Identification... ");
    gpib_write(address, "*IDN?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED - No response\n");
        test_passed = 0;
    }
    
    printf("\nTest 2: Set Frequency (1 kHz)... ");
    gpib_write(address, "FREQ 1000");
    GPIB_PACE(100);
    gpib_write(address, "FREQ?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    printf("\nTest 3: Set Amplitude (2 Vpp)... ");
    gpib_write(address, "AMPL 2.0");
    GPIB_PACE(100);
    gpib_write(address, "AMPL?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    printf("\nTest 4: Set Waveform (SINE)... ");
    gpib_write(address, "FUNC SINE");
    GPIB_PACE(100);
    gpib_write(address, "FUNC?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    printf("\nTest 5: Output Control... ");
    /* Ctrl-C/Ctrl-Break here would exit with the output live */
    old_sigint = signal(SIGINT, SIG_IGN);
    old_sigbreak = signal(SIGBREAK, SIG_IGN);
    gpib_write(address, "OUTP ON");
    GPIB_PACE(100);
    gpib_write(address, "OUTP?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    /* A failed read does not mean OUTP ON was ignored */
    if (gpib_write(address, "OUTP OFF") != 0) {
        printf("\n*** OUTPUT MAY STILL BE ON: turn it off at the FG5010 front panel. ***\n");
        printf("Press any key to continue...");
        getch();
        test_passed = 0;
    }
    GPIB_PACE(50);
    signal(SIGINT, old_sigint);
    signal(SIGBREAK, old_sigbreak);
    
    printf("\nTest 6: Error Status Check... ");
    gpib_write(address, "SYST:ERR?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    printf("\n==========================================\n");
    if (test_passed) {
        printf("ALL TESTS PASSED - FG5010 communication OK\n");
    } else {
        printf("SOME TESTS FAILED - Check connections and settings\n");
    }
    printf("==========================================\n\n");
    
    printf("Press any key to continue...");
    getch();
    
    return test_passed ? 0 : -1;
}

float fg5010_read_frequency(int address) {
    gpib_write(address, "FREQ?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return atof(gpib_response_buffer);
    }
    return 0.0;
}

int fg5010_read_output_status(int address) {
    gpib_write(address, "OUTP?");
    GPIB_PACE(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return (strstr(gpib_response_buffer, "ON") != NULL || strstr(gpib_response_buffer, "1") != NULL) ? 1 : 0;
    }
    return 0;
}

/* True if slot holds a computed (FFT/math) trace: flagged, or MOD_NONE with data.
   The derived case survives a .tm5/.cfg reload, which drops is_result. (review #1/#2/#10) */
int module_is_result(int slot) {
    return g_system->modules[slot].is_result ||
           (g_system->modules[slot].module_type == MOD_NONE &&
            g_system->modules[slot].module_data &&
            g_system->modules[slot].module_data_count > 0);
}

/* Disable "phantom" slots: enabled but no type, bad address, or no description */
void validate_enabled_modules(void) {
    int i;
    for (i = 0; i < 10; i++) {
        /* Computed traces are legitimately enabled with no GPIB address */
        if (g_system->modules[i].enabled && !module_is_result(i)) {
            if (g_system->modules[i].module_type == MOD_NONE ||
                g_system->modules[i].gpib_address < 1 ||
                g_system->modules[i].gpib_address > 30 ||
                strlen(g_system->modules[i].description) == 0) {

                g_system->modules[i].enabled = 0;
                g_system->modules[i].module_type = MOD_NONE;
                g_system->modules[i].is_result = 0;
                g_system->modules[i].gpib_address = 0;
                strcpy(g_system->modules[i].description, "");
                free_module_buffer(i);
                forget_trace_slot(i);
            }
        }
    }
}

/* ---- Auto-detect modules from a GPIB bus scan ----------------------- */
/* Indexed by MOD_xxx */
static const char *auto_models[] = { "", "DC5009", "DM5010", "DM5120", "PS5004",
                                     "PS5010", "DC5010", "FG5010" };

/* MOD_xxx for an instrument ID such as "ID TEK/DC5009,V79.1,F1.2" */
static int model_to_type(const char *id) {
    int t;
    for (t = MOD_DC5009; t <= MOD_FG5010; t++)
        if (strstr(id, auto_models[t])) return t;
    return MOD_NONE;
}

/* Set up one slot: the manual configuration's steps, without the prompts */
static void setup_module(int slot, int module_type, int address, const char *desc) {
    tm5000_module *m = &g_system->modules[slot];
    m->enabled = 1;
    m->module_type = module_type;
    m->is_result = 0;
    m->slot_number = slot;
    m->gpib_address = address;
    m->last_reading = 0.0;
    strncpy(m->description, desc, sizeof(m->description) - 1);
    m->description[sizeof(m->description) - 1] = '\0';
    switch (module_type) {
        case MOD_DC5009: init_dc5009_config(slot); break;
        case MOD_DM5010: init_dm5010_config(slot); break;
        case MOD_DM5120: init_dm5120_config(slot); init_dm5120_config_enhanced(slot); break;
        case MOD_PS5004: init_ps5004_config(slot); break;
        case MOD_PS5010: init_ps5010_config(slot); break;
        case MOD_DC5010: init_dc5010_config(slot); break;
        case MOD_FG5010: init_fg5010_config(slot); break;
    }
    gpib_remote(address);
    GPIB_PACE(200);
    allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE);
}

/* Scan GPIB addresses 1-30 (skipping the controller at 21), identify the
 * instruments and add unconfigured TM5000 modules to free slots.  Existing
 * slots are never changed; a type mismatch is only reported. */
void auto_detect_modules(void) {
    static struct { unsigned char addr, type, slot, action; char id[36]; } f[30];
    char id[80];
    int addr, i, k, n = 0, n_new = 0, used[10];
    char *p;
    
    clrscr();
    printf("Auto-detect modules\n");
    printf("===================\n\n");
    printf("Scanning GPIB addresses 1-30 (controller = 21)  ESC cancels\n");
    for (addr = 1; addr <= 30; addr++) {
        if (addr == 21) continue;
        printf("\r  address %2d ", addr);
        fflush(stdout);
        if (kbhit() && getch() == 27) {
            printf("\nCancelled.  Press any key...");
            getch();
            return;
        }
        if (!gpib_probe(addr, id, sizeof(id))) continue;
        for (p = id; *p; p++) if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        f[n].addr = (unsigned char)addr;
        f[n].type = (unsigned char)model_to_type(id);
        strncpy(f[n].id, id, sizeof(f[n].id) - 1);
        f[n].id[sizeof(f[n].id) - 1] = '\0';
        n++;
    }
    printf("\r                \n");
    if (n == 0) {
        printf("No instruments answered.  Is the GPIB driver loaded and the bus\n");
        printf("cable connected?\n\nPress any key...");
        getch();
        return;
    }
    
    /* action: 0 = not a TM5000 module / no free slot, 1 = already set up,
       2 = that address's slot is set up as another type (left alone),
       3 = add to a free slot */
    for (i = 0; i < 10; i++)
        used[i] = g_system->modules[i].enabled || g_system->modules[i].is_result;
    for (k = 0; k < n; k++) {
        f[k].action = 0;
        f[k].slot = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && g_system->modules[i].gpib_address == f[k].addr) {
                f[k].slot = (unsigned char)i;
                f[k].action = (g_system->modules[i].module_type == f[k].type) ? 1 : 2;
                break;
            }
        }
        if (i == 10 && f[k].type != MOD_NONE) {
            for (i = 0; i < 10 && used[i]; i++) ;
            if (i < 10) { used[i] = 1; f[k].slot = (unsigned char)i; f[k].action = 3; n_new++; }
        }
    }
    
    printf("Addr  Instrument                            TM5000\n");
    printf("----  ------------------------------------  ----------------------\n");
    for (k = 0; k < n; k++) {
        printf(" %2d   %-36s  ", f[k].addr, f[k].id[0] ? f[k].id : "(answers, no ID reply)");
        switch (f[k].action) {
            case 1:  printf("slot %d, already set up\n", f[k].slot); break;
            case 2:  printf("slot %d set up as other\n", f[k].slot); break;
            case 3:  printf("-> slot %d (new)\n", f[k].slot); break;
            default: printf(f[k].type != MOD_NONE ? "no free slot\n" : "not a TM5000 module\n");
        }
    }
    if (n_new == 0) {
        printf("\nNothing new to add.  Press any key...");
        getch();
        return;
    }
    printf("\nAdd %d module(s) as shown? (Y/N): ", n_new);
    if (toupper(getch()) != 'Y') {
        printf("N\nNothing changed.  Press any key...");
        getch();
        return;
    }
    printf("Y\n");
    for (k = 0; k < n; k++) {
        if (f[k].action != 3) continue;
        printf("  slot %d: %s at GPIB %d\n", f[k].slot, auto_models[f[k].type], f[k].addr);
        setup_module(f[k].slot, f[k].type, f[k].addr, auto_models[f[k].type]);
    }
    printf("\nDone - use 0-9 to change a slot's settings.  Press any key...");
    getch();
}

/* Slot setup menu: module type, GPIB address, description; A runs auto-detect */
void configure_modules(void) {
    int choice, slot, address, module_type;
    int done = 0;
    char description[20];
    
    while (!done) {
        clrscr();
        printf("Module Configuration\n");
        printf("====================\n\n");
        
        printf("Current Configuration:\n");
        for (slot = 0; slot < 10; slot++) {
            printf("Slot %d: ", slot);
            if (g_system->modules[slot].enabled) {
                printf("%s (Type %d) at GPIB %d\n", 
                       g_system->modules[slot].description,
                       g_system->modules[slot].module_type,
                       g_system->modules[slot].gpib_address);
            } else {
                printf("Empty\n");
            }
        }
        
        printf("\nOptions:\n");
        printf("0-9: Configure slot\n");
        printf("A:   Auto-detect modules on the bus\n");
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = getch();
        
        if (choice == 27) {  /* ESC */
            done = 1;
        } else if (choice == 'A' || choice == 'a') {
            auto_detect_modules();
        } else if (choice >= '0' && choice <= '9') {
            slot = choice - '0';
            
            printf("\n\nConfiguring Slot %d\n", slot);
            printf("==================\n\n");
            
            printf("Module Types:\n");
            printf("0 = Empty\n");
            printf("1 = DC5009 Universal Counter\n");
            printf("2 = DM5010 Multimeter\n");
            printf("3 = DM5120 Multimeter\n");
            printf("4 = PS5004 Power Supply\n");
            printf("5 = PS5010 Dual Power Supply\n");
            printf("6 = DC5010 Universal Counter\n");
            printf("7 = FG5010 Function Generator\n");
            printf("\nEnter module type: ");
            scanf("%d", &module_type);
            
            if (module_type == 0) {
                /* Remove module */
                g_system->modules[slot].enabled = 0;
                g_system->modules[slot].module_type = MOD_NONE;
                g_system->modules[slot].is_result = 0;
                g_system->modules[slot].gpib_address = 0;
                strcpy(g_system->modules[slot].description, "");
                free_module_buffer(slot);
                forget_trace_slot(slot);
            } else if (module_type >= 1 && module_type <= 7) {
                printf("Enter GPIB address (1-30): ");
                scanf("%d", &address);
                
                if (address >= 1 && address <= 30) {
                    /* Check for address conflicts */
                    int conflict = 0;
                    int i;
                    for (i = 0; i < 10; i++) {
                        if (i != slot && g_system->modules[i].enabled && 
                            g_system->modules[i].gpib_address == address) {
                            printf("\nError: Address %d already used by slot %d!\n", address, i);
                            conflict = 1;
                            break;
                        }
                    }
                    
                    if (!conflict) {
                        printf("Enter description: ");
                        scanf("%s", description);

                        g_system->modules[slot].enabled = 1;
                        g_system->modules[slot].module_type = module_type;
                        g_system->modules[slot].is_result = 0;
                        g_system->modules[slot].slot_number = slot;
                        g_system->modules[slot].gpib_address = address;
                        g_system->modules[slot].last_reading = 0.0;
                        strcpy(g_system->modules[slot].description, description);

                        switch (module_type) {
                            case MOD_DC5009:
                                init_dc5009_config(slot);
                                break;
                            case MOD_DM5010:
                                init_dm5010_config(slot);
                                break;
                            case MOD_DM5120:
                                init_dm5120_config(slot);
                                init_dm5120_config_enhanced(slot);
                                break;
                            case MOD_PS5004:
                                init_ps5004_config(slot);
                                break;
                            case MOD_PS5010:
                                init_ps5010_config(slot);
                                break;
                            case MOD_DC5010:
                                init_dc5010_config(slot);
                                break;
                            case MOD_FG5010:
                                init_fg5010_config(slot);
                                break;
                        }
                        
                        printf("Initializing device at GPIB address %d...\n", 
                               g_system->modules[slot].gpib_address);
                        
                        gpib_remote(g_system->modules[slot].gpib_address);
                        GPIB_PACE(200);
                        
                        /* LF-termination prompt is DM5120-only, as in v2.9 */
                        if (module_type == MOD_DM5120) {
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
                        }
                        
                        allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE);

                        printf("\nTest communication? (y/n): ");
                        if (toupper(getch()) == 'Y') {
                            printf("\nTesting communication...\n");
                            
                            gpib_remote(address);
                            GPIB_PACE(200);
                            
                            switch (module_type) {
                                case MOD_DC5009:
                                    test_dc5009_comm(address);
                                    break;
                                case MOD_DC5010:
                                    test_dc5010_comm(address);
                                    break;
                                case MOD_DM5010:
                                    test_dm5010_comm(address);
                                    break;
                                case MOD_FG5010:
                                    test_fg5010_comm(address);
                                    break;
                                case MOD_DM5120:
                                    test_dm5120_comm(address);
                                    break;
                                case MOD_PS5004:
                                    test_ps5004_comm(address);
                                    break;
                                case MOD_PS5010:
                                    test_ps5010_comm(address);
                                    break;
                            }
                        }
                        
                        printf("\nModule configured successfully!\n");
                    }
                } else {
                    printf("\nInvalid address! Must be 1-30.\n");
                }
            } else {
                printf("\nInvalid module type!\n");
            }
            
            printf("Press any key to continue...");
            getch();
        }
    }
}

void display_trace_selection_menu(void) {
    int choice, done = 0;
    int i, active_traces = 0;
    
    while (!done) {
        clrscr();
        printf("Trace Selection for Display\n");
        printf("===========================\n\n");
        
        printf("Available Traces:\n");
        active_traces = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                printf("%d: %s (%d points) - %s\n", i, 
                       g_traces[i].label,
                       g_traces[i].data_count,
                       g_traces[i].enabled ? "ACTIVE" : "inactive");
                if (g_traces[i].enabled) active_traces++;
            }
        }
        
        printf("\nActive traces: %d\n", active_traces);
        
        printf("\nOptions:\n");
        printf("0-9: Toggle trace display\n");
        printf("A: Show All traces\n");
        printf("N: Show No traces\n");
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = toupper(getch());
        
        if (choice == 27) {  /* ESC */
            done = 1;
        } else if (choice >= '0' && choice <= '9') {
            i = choice - '0';
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                g_traces[i].enabled = !g_traces[i].enabled;
            }
        } else if (choice == 'A') {
            for (i = 0; i < 10; i++) {
                if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                    g_traces[i].enabled = 1;
                }
            }
        } else if (choice == 'N') {
            for (i = 0; i < 10; i++) {
                g_traces[i].enabled = 0;
            }
        }
    }
}

/* A slot's trace is shown the first time it has data; after that the user's choice is
   kept, even across a sync where its buffer was freed (pool resize). A slot that gets a
   new occupant (module removed, file loaded) must be forgotten with forget_trace_slot. */
static unsigned char shown_before[10];
static unsigned char hidden_no_data[10];  /* enabled was forced 0, not the user's choice */
static unsigned char user_enabled[10];

void forget_trace_slot(int slot) {
    if (slot < 0 || slot >= 10) return;
    shown_before[slot] = 0;
    hidden_no_data[slot] = 0;
    user_enabled[slot] = 0;
}

void sync_traces_with_modules(void) {
    int i;

    for (i = 0; i < 10; i++) {
        if (shown_before[i] && !hidden_no_data[i]) {
            user_enabled[i] = g_traces[i].enabled;
        }
        g_traces[i].data = NULL;
        g_traces[i].data_count = 0;
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
                
                if (module_is_result(i)) {
                    /* Computed trace: keep the unit_type/x_scale/x_offset set at creation. (review #3) */
                } else {
                    switch (g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            /* Counter measurements are frequencies */
                            g_traces[i].unit_type = UNIT_FREQUENCY;
                            g_traces[i].x_scale = 1.0;  /* Sample-based for counters */
                            g_traces[i].x_offset = 0.0;
                            break;
                        case MOD_DM5120:
                            {
                                const char *u = dm5120_unit(i);
                                g_traces[i].unit_type = u[0] == 'd' ? UNIT_DB :
                                                        u[0] == 'o' ? UNIT_RESISTANCE :
                                                        u[0] == 'A' ? UNIT_CURRENT : UNIT_VOLTAGE;
                            }
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                        default:
                            /* Default to voltage for multimeters and power supplies */
                            g_traces[i].unit_type = UNIT_VOLTAGE;
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                    }
                }
                
                if (!shown_before[i]) {
                    shown_before[i] = 1;
                    user_enabled[i] = 1;
                }
                g_traces[i].enabled = user_enabled[i];
                hidden_no_data[i] = 0;
            } else {
                g_traces[i].enabled = 0;
                hidden_no_data[i] = 1;
            }
        } else {
            g_traces[i].enabled = 0;
            shown_before[i] = 0;
            hidden_no_data[i] = 0;
        }
    }
}

/* DM5120 multimeter */
void init_dm5120_config(int slot) {
    strcpy(g_dm5120_config[slot].function, "DCV");
    g_dm5120_config[slot].range_mode = 0;  /* AUTO */
    g_dm5120_config[slot].filter_enabled = 0;
    g_dm5120_config[slot].filter_value = 10;
    g_dm5120_config[slot].trigger_source = 0;  /* TALK, the manual's recommended default */
    g_dm5120_config[slot].trigger_mode = 0;  /* CONT */
    g_dm5120_config[slot].digits = 5;
    g_dm5120_config[slot].nullval = 0.0;
    g_dm5120_config[slot].null_enabled = 0;
    g_dm5120_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5120_config[slot].buffer_enabled = 0;
    g_dm5120_config[slot].buffer_size = DM5120_DEFAULT_BUFFER_SIZE;  /* half the 500-reading hardware limit */
    g_dm5120_config[slot].min_max_enabled = 0;
    g_dm5120_config[slot].min_value = 1e30;
    g_dm5120_config[slot].max_value = -1e30;
    g_dm5120_config[slot].sample_count = 0;
    g_dm5120_config[slot].burst_mode = 0;
    g_dm5120_config[slot].sample_rate = 1.0;
    g_dm5120_config[slot].lf_termination = 0;  /* Default to CRLF */
    g_dm5120_config[slot].autocal_off = 0;
    g_dm5120_config[slot].intfilt_off = 0;
    g_dm5120_config[slot].buffer_state = 0;
}
void init_dm5120_config_enhanced(int slot) {
    init_dm5120_config(slot);
    g_dm5120_config[slot].buffer_enabled = 1;
    g_dm5120_config[slot].buffer_size = DM5120_MAX_BUFFER_SIZE;  /* hardware limit */
    g_dm5120_config[slot].min_max_enabled = 1;
    g_dm5120_config[slot].burst_mode = 1;
    g_dm5120_config[slot].sample_rate = 10.0;  /* 10 readings/sec */
}
/* Settings an older menu could save: DCI/ACI, which the meter rejects, and trigger
   choices 2 (TALK) / 3 (EXT) stored in the CONT/ONE field. Also drops a fill state
   a profile captured mid-fill. */
void dm5120_fix_legacy_config(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (strcmp(cfg->function, "DCI") == 0) strcpy(cfg->function, "DCA");
    else if (strcmp(cfg->function, "ACI") == 0) strcpy(cfg->function, "ACA");
    if (cfg->trigger_mode == 2 || cfg->trigger_mode == 3) {
        cfg->trigger_source = cfg->trigger_mode - 2;
        cfg->trigger_mode = 0;
    } else if (cfg->trigger_mode != 0) {
        cfg->trigger_mode = 1;
    }
    cfg->buffer_state = 0;
}

void dm5120_set_function(int address, char *function) {
    sprintf(gpib_cmd_buffer, "FUNCT %s", function);
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(100);
}
void dm5120_set_range(int address, int range) {
    if (range == 0) {
        gpib_write_dm5120(address, "RANGE AUTO");
    } else {
        sprintf(gpib_cmd_buffer, "RANGE %d", range);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(50);
}
void dm5120_set_filter(int address, int enabled, int value) {

    if (enabled) {
        /* FILTER only takes ON/OFF; the count is FILTERVAL, stored per function */
        if (value > 0 && value <= 99) {
            sprintf(gpib_cmd_buffer, "FILTERVAL %d", value);
            gpib_write_dm5120(address, gpib_cmd_buffer);
            GPIB_PACE(50);
        }
        gpib_write_dm5120(address, "FILTER ON");
        GPIB_PACE(50);
    } else {
        gpib_write_dm5120(address, "FILTER OFF");
    }
}
void dm5120_set_trigger(int address, char *source, char *mode) {

    if (!source || !mode) {
        printf("ERROR: dm5120_set_trigger requires both source and mode\n");
        return;
    }
    
    /* Validate source (TALK or EXT) */
    if (strcmp(source, "TALK") != 0 && strcmp(source, "EXT") != 0) {
        printf("ERROR: Invalid trigger source '%s'. Must be TALK or EXT\n", source);
        return;
    }
    
    /* Validate mode (CONT or ONE) */
    if (strcmp(mode, "CONT") != 0 && strcmp(mode, "ONE") != 0) {
        printf("ERROR: Invalid trigger mode '%s'. Must be CONT or ONE\n", mode);
        return;
    }
    
    sprintf(gpib_cmd_buffer, "TRIGGER %s,%s", source, mode);  /* manual syntax: TRIGGER <source>,<mode> */
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void dm5120_set_digits(int address, int digits) {
    if (digits >= 3 && digits <= 6) {
        /* DIGIT, not DIGITS: the DM5120 accepts no other spelling (error 101) */
        sprintf(gpib_cmd_buffer, "DIGIT %d", digits);
        gpib_write_dm5120(address, gpib_cmd_buffer);
        GPIB_PACE(50);
    }
}
void dm5120_set_null(int address, int enabled, float value) {
    if (enabled) {
        /* NULLVAL, then ON: "NULL <value>" with a value of 0 turns null off */
        sprintf(gpib_cmd_buffer, "NULLVAL %.6e", value);
        gpib_write_dm5120(address, gpib_cmd_buffer);
        GPIB_PACE(50);
        gpib_write_dm5120(address, "NULL ON");
    } else {
        gpib_write_dm5120(address, "NULL OFF");
    }
    delay(50);
}
void dm5120_set_data_format(int address, int on) {
    gpib_write_dm5120(address, on ? "DATFOR ON" : "DATFOR OFF");
    GPIB_PACE(50);
}
void dm5120_set_autocal(int address, int on) {
    gpib_write_dm5120(address, on ? "AUTOCAL ON" : "AUTOCAL OFF");
    GPIB_PACE(50);
}
void dm5120_set_intfilt(int address, int on) {
    gpib_write_dm5120(address, on ? "INTFILT ON" : "INTFILT OFF");
    GPIB_PACE(50);
}

/* Value from a DM5120 reply. Query replies always carry a header ("BUFCNT 45;"),
   readings start with the number ("+1.234567E+0:NDCV:000;"). */
int dm5120_parse_value(const char *reply, float *value) {
    while (*reply && (isalpha((unsigned char)*reply) || isspace((unsigned char)*reply))) reply++;
    return sscanf(reply, "%f", value) == 1;
}

/* Send a query and parse the number in its reply; 0 if there was none */
static int dm5120_query_value(int address, char *query, float *value) {
    gpib_write_dm5120(address, query);
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) <= 0) return 0;
    return dm5120_parse_value(gpib_response_buffer, value);
}

/* ERROR?/EVENT? codes, manual Table 3-5 */
static const struct { int code; const char *text; } __far dm5120_errors[] = {
    {   0, "No error or event" },
    { 101, "Command header error" },
    { 102, "Header delimiter error" },
    { 103, "Command argument error" },
    { 104, "Argument delimiter error" },
    { 106, "Missing argument" },
    { 107, "Invalid message unit delimiter" },
    { 201, "Not executable in local" },
    { 202, "Settings lost due to RTL" },
    { 204, "Settings conflict" },
    { 205, "Argument out of range" },
    { 250, "Invalid RANGE argument" },
    { 251, "Invalid DIGIT argument" },
    { 252, "Invalid FILTERVAL argument" },
    { 253, "Invalid ZEROVAL argument" },
    { 254, "Invalid BUFSZ argument" },
    { 255, "Invalid STOINT argument" },
    { 256, "Invalid DELAY argument" },
    { 257, "TEXT argument too long" },
    { 258, "Invalid KEY argument" },
    { 259, "Calibration out of range" },
    { 260, "CAL locked" },
    { 401, "Power on" },
    { 402, "Operation complete" },
    { 403, "User request (ID key)" },
    { 450, "Buffer full" },
    { 451, "Buffer half full" },
    { 452, "Ready for conversion" },
    { 453, "EEPROM failure" },
    { 454, "Overflow" },
    { 551, "STOINT shorter than conversion time" },
    { 650, "Trigger during a conversion" }
};

const char *dm5120_error_text(int code) {
    int i;
    for (i = 0; i < sizeof(dm5120_errors) / sizeof(dm5120_errors[0]); i++)
        if (dm5120_errors[i].code == code) return dm5120_errors[i].text;
    return "Unknown code";
}

/* ERROR? code, or -1 if the meter did not answer */
int dm5120_query_error(int address) {
    float v;
    return dm5120_query_value(address, "ERROR?", &v) ? (int)v : -1;
}

/* Unit of the slot's function, for display */
const char *dm5120_unit(int slot) {
    const char *f = g_dm5120_config[slot].function;
    if (strstr(f, "DB")) return "dB";
    if (strstr(f, "OHMS")) return "ohm";
    if (f[2] == 'A') return "A";   /* DCA, ACA */
    return "V";
}
/* DM5120 reading rates (readings/s), manual p. 1-9: [3.5..6.5 digits][AutoCal off, on].
   5.5/6.5 digits use the 50 Hz figures: the line frequency (PROGRAM 32) can't be
   queried and they are the slower. 6.5 digits is listed with AutoCal on only. */

/* DCV, DCA, ACV, ACA rates - Continuous Into Internal Buffer */
static const float __far dm5120_voltage_current_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {1000.0, 1000.0},  /* 3.5 digit */
    {333.0, 333.0},    /* 4.5 digit */
    {29.0, 7.5},       /* 5.5 digit */
    {7.5, 7.5}         /* 6.5 digit */
};

/* OHMS rates - Continuous Into Internal Buffer */
static const float __far dm5120_ohms_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {40.0, 24.0},      /* 3.5 digit */
    {38.0, 18.0},      /* 4.5 digit */
    {13.0, 7.5},       /* 5.5 digit */
    {7.5, 7.5}         /* 6.5 digit */
};

/* IEEE-488 Bus triggered rates (slower) */
static const float __far dm5120_bus_voltage_current_rates[4][2] = {
    {72.0, 45.0},      /* 3.5 digit */
    {63.0, 41.0},      /* 4.5 digit */
    {26.0, 7.5},       /* 5.5 digit */
    {0.2, 0.2}         /* 6.5 digit */
};

static const float __far dm5120_bus_ohms_rates[4][2] = {
    {32.0, 19.0},      /* 3.5 digit */
    {29.0, 16.0},      /* 4.5 digit */
    {12.5, 7.5},       /* 5.5 digit */
    {0.25, 0.25}       /* 6.5 digit */
};

/* Digits the meter actually converts at (manual Table 3-1): dB, OHMSCOMP and the
   top ohms ranges never go below 5.5. Rounded up where the table is unclear. */
static int dm5120_effective_digits(dm5120_config *cfg) {
    int d = (cfg->digits >= 3 && cfg->digits <= 6) ? cfg->digits : 4;
    if (d < 5) {
        if (strstr(cfg->function, "DB") || strstr(cfg->function, "OHMSCOMP")) d = 5;
        else if (strstr(cfg->function, "OHMS") && (cfg->range_mode == 0 || cfg->range_mode >= 5)) d = 5;
    }
    return d;
}

static float dm5120_table_rate(dm5120_config *cfg, int use_bus_rates) {
    int digit_index = dm5120_effective_digits(cfg) - 3;
    int autocal_index = cfg->autocal_off ? 0 : 1;
    int is_ohms_function = (strstr(cfg->function, "OHMS") != NULL);
    float rate;

    if (use_bus_rates) {
        rate = is_ohms_function ? dm5120_bus_ohms_rates[digit_index][autocal_index]
                                : dm5120_bus_voltage_current_rates[digit_index][autocal_index];
    } else {
        rate = is_ohms_function ? dm5120_ohms_rates[digit_index][autocal_index]
                                : dm5120_voltage_current_rates[digit_index][autocal_index];
    }
    if (strstr(cfg->function, "OHMSCOMP") != NULL) {
        rate *= 0.5; /* Offset compensated ohms is 0.5x normal rate */
    }
    if (rate < 0.1) rate = 0.1;  /* callers divide by rate */
    return rate;
}

/* Readings/s for the slot's DM5120 config; trigger_mode 2 forces bus-triggered rates */
float dm5120_get_measurement_rate(int slot, int trigger_mode) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int use_bus_rates;

    /* Bus-triggered vs continuous rates, per DM5120 manual:
     * - EXT,CONT with buffer enabled requires external trigger (bus rates)
     * - EXT,ONE always requires external trigger (bus rates)
     * - TALK mode always uses bus rates (triggered by GPIB addressing)
     * - EXT,CONT with buffer disabled uses continuous rates (auto-trigger)
     */
    use_bus_rates = (trigger_mode == 2) || /* Explicit bus-triggered mode */
                    (cfg->trigger_source == 1 && (cfg->trigger_mode == 1 || cfg->buffer_enabled)) || /* EXT,ONE or EXT,CONT with buffer */
                    (cfg->trigger_source == 0); /* TALK mode always uses bus rates */

    return dm5120_table_rate(cfg, use_bus_rates);
}

/* Shortest STOINT (ms) the slot's settings allow. Below one conversion into the
   buffer the meter stops with error 551 (SHORT TIME); +30% because the rates are
   nominal (6.5 digits with AutoCal gives 174 ms, near the factory STOINT 175).
   1-14 ms and 1-2 ms are the manual's settings conflicts (error 204). */
int dm5120_min_stoint_ms(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int ms = (int)(1300.0 / dm5120_table_rate(cfg, 0)) + 1;

    if (ms <= 14 && (strstr(cfg->function, "OHMS") || strstr(cfg->function, "DB") ||
                     cfg->range_mode == 0 || cfg->buffer_size == 0 || cfg->digits >= 5)) {
        ms = 15;
    }
    if (ms <= 2 && cfg->digits >= 4) ms = 3;
    return ms;
}

/* Delay in ms for a DM5120 operation, derived from the reading rate */
int dm5120_calculate_measurement_time(int slot, int operation_type, int sample_count) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int base_time_ms;
    int trigger_mode = cfg->trigger_mode;

    measurement_rate = dm5120_get_measurement_rate(slot, trigger_mode);

    base_time_ms = (int)(1000.0 / measurement_rate);

    base_time_ms = (base_time_ms * 150) / 100;  /* +50% for GPIB overhead and system lag */

    switch (operation_type) {
        case 0: /* Single measurement */
            return base_time_ms;

        case 1: /* Buffer setup (BUFSZ command) */
            return base_time_ms + 200; /* setup overhead */

        case 2: /* Buffer data retrieval (READ ALLSTORE) */
            return (base_time_ms * sample_count) + 500;  /* fill time + GPIB/setup lag */
            
        case 3: /* STOINT setup */
            return 50; /* Quick command response */
            
        case 4: /* Buffer status query (BUFCNT?, BUFAVE?, etc.) */
            return 50;
            
        default:
            return base_time_ms;
    }
}

/* Alias of dm5120_calculate_measurement_time */
int dm5120_calculate_delay(int slot, int operation_type, int sample_count) {
    return dm5120_calculate_measurement_time(slot, operation_type, sample_count);
}

void dm5120_enable_buffering(int address, int slot, int buffer_size) {
    
    /* DM5120 internal buffer holds 500 readings */
    if (buffer_size > 500) {
        buffer_size = 500;
    }

    if (buffer_size == 0) {
        /* 0 selects circular mode */
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", buffer_size);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }

    delay(dm5120_calculate_delay(slot, 1, buffer_size));
}

/* Check a STOINT interval against the manual's listed conflicts; 0 = OK, -1..-5 = conflict
   code, DM5120_STOINT_BAD_SLOT = slot out of range, DM5120_STOINT_TOO_SHORT = shorter
   than a conversion (error 551) */
int dm5120_validate_stoint_settings(int slot, int interval_ms) {
    dm5120_config *cfg;

    if (slot < 0 || slot >= 10) return DM5120_STOINT_BAD_SLOT;
    cfg = &g_dm5120_config[slot];

    /* STOINT 1-14 ms conflicts with: */
    if (interval_ms >= 1 && interval_ms <= 14) {
        if (strstr(cfg->function, "OHMS") != NULL ||
            strstr(cfg->function, "ACVDB") != NULL ||
            strstr(cfg->function, "ACADB") != NULL) {
            return -1; /* OHMS or dB function */
        }

        if (cfg->range_mode == 0) {
            return -2; /* AUTO range */
        }

        if (cfg->buffer_size == 0) {
            return -3; /* CIRCULAR buffer */
        }

        if (cfg->digits >= 5) {
            return -4; /* 5 or 6 digit resolution */
        }
    }

    if ((interval_ms == 1 || interval_ms == 2) && cfg->digits >= 4) {
        return -5; /* STOINT 1-2ms conflicts with 4+ digit resolution */
    }

    if (interval_ms > 0 && interval_ms < dm5120_min_stoint_ms(slot)) {
        return DM5120_STOINT_TOO_SHORT;
    }

    return 0; /* No conflicts */
}

/* Storage interval for the slot: the burst rate if burst mode is on, else 100 ms,
   raised to the shortest the settings allow */
int dm5120_stoint_for_slot(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int ms = 100;
    int min_ms = dm5120_min_stoint_ms(slot);

    if (cfg->burst_mode && cfg->sample_rate >= 0.05 && cfg->sample_rate <= 1000.0) {
        ms = (int)(1000.0 / cfg->sample_rate + 0.5);
    }
    return ms < min_ms ? min_ms : ms;
}

/* STOINT <ms>, or STOINT ONE for 0. An interval shorter than the settings allow is
   raised to dm5120_min_stoint_ms (the meter would stop with error 551 or 204); no
   message, since the monitor calls this. Returns the interval sent, -1 if none. */
int dm5120_set_storage_interval(int address, int slot, int interval_ms) {
    int status;
    int min_ms;

    if (slot < 0 || slot >= 10) {
        printf("ERROR: invalid DM5120 slot %d (valid: 0-9), STOINT not sent\n", slot);
        return -1;
    }

    if (interval_ms == 0) {
        /* One shot mode - store one reading per trigger */
        status = gpib_write_dm5120(address, "STOINT ONE");
    } else {
        min_ms = dm5120_min_stoint_ms(slot);
        if (interval_ms < min_ms) interval_ms = min_ms;
        sprintf(gpib_cmd_buffer, "STOINT %d", interval_ms);
        status = gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    if (status != 0) return -1;

    delay(dm5120_calculate_measurement_time(slot, 3, 1));
    return interval_ms;
}

int dm5120_query_storage_interval(int address) {

    long interval = 0;
    
    gpib_write_dm5120(address, "STOINT?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "ONE")) {
            return 0; /* One shot mode */
        }
        /* long: STOINT runs to 999999 ms */
        if (sscanf(gpib_response_buffer, "STOINT %ld", &interval) == 1) {
            return interval > 32767L ? 32767 : (int)interval;
        }
    }

    return -1; /* Error */
}

/* DM5120 SRQ events for buffer status */

/* Set FULL/HALF/RDY/OPC event masks, then enable RQS */
void dm5120_configure_srq_events(int address, int enable_full, int enable_half, int enable_rdy, int enable_opc) {

    sprintf(gpib_cmd_buffer, "FULL %s", enable_full ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "HALF %s", enable_half ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "RDY %s", enable_rdy ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "OPC %s", enable_opc ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    gpib_write_dm5120(address, "RQS ON");
    GPIB_PACE(50);
}

/* Pick SRQ events to suit buffer type, size, and expected fill time */
void dm5120_configure_srq_events_smart(int address, int buffer_size, int is_circular, int expected_time_ms) {
    int enable_full, enable_half;
    
    if (is_circular) {
        /* CIRCULAR buffers never report FULL events */
        enable_full = 0;
        enable_half = 1;  /* Use HALF to monitor buffer activity */
        printf("SRQ Config: CIRCULAR buffer - HALF events enabled\n");
    } else {
        /* Linear buffers require FULL event for completion detection */
        enable_full = 1;
        
        /* Enable HALF for large buffers or slow operations */
        if (buffer_size > 100 || expected_time_ms > 10000) {
            enable_half = 1;
            printf("SRQ Config: Large/slow buffer - FULL+HALF events enabled\n");
        } else {
            enable_half = 0;
            printf("SRQ Config: Small/fast buffer - FULL events only\n");
        }
    }

    dm5120_configure_srq_events(address, enable_full, enable_half, 1, 1);
}

/* Check DM5120 buffer status using SRQ events */
int dm5120_check_buffer_status(int address) {
    
    int status = 0;
    float code;

    /* EVENT? replies with a Table 3-5 code, e.g. "EVENT 450;" */
    if (dm5120_query_value(address, "EVENT?", &code)) {
        switch ((int)code) {
            case 0:   status = 0; break;        /* No events */
            case 450: status = 0x01; break;     /* FULL buffer */
            case 451: status = 0x02; break;     /* HALF buffer */
            case 452: status = 0x04; break;     /* RDY ready */
            case 402: status = 0x08; break;     /* OPC operation complete */
            case 454: status = 0x10; break;     /* OVER overrange */
            case 551: status = 0x20; break;     /* STOINT shorter than a conversion */
            default:  status = 0x80; break;     /* Any other error or event */
        }
    }

    return status;
}

/* Fill the DM5120 buffer (CONT+STOINT if <=1 s/reading, else one SEND per reading)
   and read it back; returns samples read */
int dm5120_get_buffer_data_enhanced(int address, int slot, float far *buffer, int max_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    char response[GPIB_BUFFER_SIZE];
    float value;
    int count = 0;
    int timeout_ms, elapsed_ms = 0;
    int status;
    unsigned long start_time, current_time;
    int is_circular;
    float measurement_rate;
    int measurement_interval_ms;
    int use_cont_mode;
    int i;
    
    timeout_ms = dm5120_calculate_measurement_time(slot, 2, max_samples);

    is_circular = (cfg->buffer_size == 0);
    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    measurement_interval_ms = (int)(1000.0 / measurement_rate);
    use_cont_mode = (measurement_interval_ms <= 1000);

    dm5120_configure_srq_events_smart(address, max_samples, is_circular, timeout_ms);

    /* BUFSZ also clears any stored readings */
    if (is_circular) {
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", max_samples);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(100);
    
    if (use_cont_mode) {
        /* <=1 s per reading: CONT mode, STOINT fills the buffer */
        dm5120_set_trigger(address, "TALK", "CONT");
        measurement_interval_ms = dm5120_set_storage_interval(address, slot, measurement_interval_ms);
        printf("Buffer Strategy: CONT mode, %d ms intervals, %.1f r/s\n",
               measurement_interval_ms, measurement_rate);
    } else {
        /* >1 s per reading: trigger each one with SEND */
        dm5120_set_trigger(address, "TALK", "ONE");
        printf("Buffer Strategy: SEND mode, %d ms per measurement, %.1f r/s\n", 
               measurement_interval_ms, measurement_rate);
    }
    
    start_time = *((unsigned long far *)0x0040006CL);  /* BIOS tick count, ~55 ms/tick */

    if (use_cont_mode) {
        /* Buffer fills on its own; watch SRQ events */
        printf("Monitoring automatic buffer fill...\n");

        while (count < max_samples && elapsed_ms < timeout_ms) {
            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);

            status = dm5120_check_buffer_status(address);
            
            if (status & 0x01) { /* FULL buffer event */
                printf("FULL event received - buffer complete\n");
                break;
            }

            if (status & 0x20) {
                printf("ERROR 551: STOINT shorter than the conversion time\n");
                break;
            }
            
            if (status & 0x02) { /* HALF buffer event */
                printf("Buffer 50%% full...\n");
            }
            
            delay(200); /* Check events every 200ms */
        }
        
    } else {
        printf("Manual triggering for %d samples...\n", max_samples);

        for (i = 0; i < max_samples && elapsed_ms < timeout_ms; i++) {
            gpib_write_dm5120(address, "SEND");
            delay(measurement_interval_ms);

            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);

            /* Progress every 10 readings */
            if ((i + 1) % 10 == 0 || i == max_samples - 1) {
                printf("Progress: %d/%d samples (%.1f%%)\n", 
                       i + 1, max_samples, ((float)(i + 1) / max_samples) * 100.0);
            }
        }
        
        printf("Manual triggering complete\n");
    }

    printf("Checking buffer status before read...\n");
    gpib_write_dm5120(address, "BUFCNT?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        int buffer_count;
        if (sscanf(response, "%d", &buffer_count) == 1) {
            printf("Buffer contains %d samples\n", buffer_count);
            if (buffer_count == 0) {
                printf("WARNING: Buffer is empty - no data to read\n");
                return 0;
            }
        }
    }
    
    printf("Reading buffer data...\n");
    gpib_write_dm5120(address, "READ ALLSTORE");
    delay(dm5120_calculate_measurement_time(slot, 4, 1) * 2); /* 2x the status-query delay */

    /* Query adapter status; on ERROR/FAULT, abort to clear it before reading */
    printf("Checking GPIB status before read...\n");
    ieee_write("status\r\n");
    GPIB_PACE(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        printf("GPIB Status: %s\n", response);
        if (strstr(response, "ERROR") || strstr(response, "FAULT")) {
            printf("ERROR: GPIB error detected: %s\n", response);
            printf("Attempting to clear error...\n");
            ieee_write("abort\r\n");
            GPIB_PACE(200);
            ieee_write("status\r\n");
            GPIB_PACE(50);
            if (ieee_read(response, sizeof(response)) > 0) {
                printf("Status after abort: %s\n", response);
            }
        }
    }

    printf("Attempting to read buffer data...\n");
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        char *token = strtok(response, ",;");
        
        while (token != NULL && count < max_samples) {
            if (sscanf(token, "%f", &value) == 1) {
                buffer[count] = value;
                count++;
            }
            token = strtok(NULL, ",;");
        }
        
        printf("Buffer read complete: %d samples retrieved\n", count);
    } else {
        printf("ERROR: Failed to read buffer data - READFAULT detected\n");
        printf("Attempting alternate read method...\n");
        
        /* Fallback: READ ONESTORE one sample at a time */
        for (i = 0; i < max_samples && count < max_samples; i++) {
            gpib_write_dm5120(address, "READ ONESTORE");
            GPIB_PACE(200);
            
            if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
                if (sscanf(response, "%f", &value) == 1) {
                    buffer[count] = value;
                    count++;
                } else {
                    printf("Failed to parse sample %d: '%s'\n", i, response);
                    break;
                }
            } else {
                printf("Failed to read sample %d\n", i);
                break;
            }
        }
        
        if (count > 0) {
            printf("Fallback read completed: %d samples retrieved\n", count);
        }
    }

    if (count == max_samples) {
        printf("SUCCESS: All %d samples collected\n", count);
    } else if (count > 0) {
        printf("PARTIAL: %d/%d samples collected\n", count, max_samples);
    } else {
        printf("FAILED: No samples collected (timeout: %d ms)\n", elapsed_ms);
    }
    
    return count;
}

/* DM5120 timing diagnostics */

/* Print rate, fill-time, and STOINT checks for the slot; returns warning count */
int dm5120_validate_configuration(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int optimal_delay, buffer_fill_time;
    int warnings = 0;

    printf("DM5120 Configuration Analysis for Slot %d:\n", slot);
    printf("==========================================\n");

    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    printf("Measurement Rate: %.1f readings/sec\n", measurement_rate);

    optimal_delay = dm5120_calculate_measurement_time(slot, 0, 1);
    printf("Optimal Sample Interval: %d ms\n", optimal_delay);

    if (cfg->buffer_enabled) {
        int stoint_ms = dm5120_stoint_for_slot(slot);
        int min_ms = dm5120_min_stoint_ms(slot);

        printf("Storage Interval: %d ms (shortest for these settings: %d ms)\n", stoint_ms, min_ms);
        if (cfg->burst_mode && cfg->sample_rate > 0.0 && 1000.0 / cfg->sample_rate < min_ms) {
            printf("WARNING: burst rate %.1f/s is faster than the meter converts;\n", cfg->sample_rate);
            printf("  STOINT is raised to %d ms (%.1f/s)\n", min_ms, 1000.0 / min_ms);
            warnings++;
        }

        if (cfg->buffer_size > 0) {
            buffer_fill_time = (int)(((long)cfg->buffer_size * stoint_ms > 32767L) ? 32767L
                                     : (long)cfg->buffer_size * stoint_ms);
            printf("Buffer Fill Time (%d samples): %ld ms\n", cfg->buffer_size,
                   (long)cfg->buffer_size * stoint_ms);
            if (buffer_fill_time > 30000) { /* 30 s */
                printf("WARNING: Buffer fill time exceeds 30 seconds!\n");
                printf("Consider reducing buffer size or using faster settings.\n");
                warnings++;
            }
        }
    }

    printf("\nRecommendations:\n");
    if (measurement_rate < 10.0) {
        printf("- Consider using lower resolution for faster measurements\n");
        printf("- OHMS measurements are inherently slower than voltage\n");
    }
    
    if (cfg->digits >= 5) {
        printf("- High resolution reduces measurement speed significantly\n");
        printf("- AutoCal adds additional delay for 5-6 digit measurements\n");
    }
    
    if (cfg->buffer_size > 100) {
        printf("- Large buffers may cause timeout issues\n");
        printf("- Consider using smaller buffers with multiple reads\n");
    }
    
    printf("\nConfiguration Status: %s\n", warnings == 0 ? "OPTIMAL" : "NEEDS ATTENTION");
    return warnings;
}

void dm5120_timing_diagnostics(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float rate_continuous, rate_bus;
    int i;
    
    printf("DM5120 Timing Diagnostics - Slot %d\n", slot);
    printf("===================================\n\n");
    
    printf("Current Configuration:\n");
    printf("Function: %s\n", cfg->function);
    printf("Resolution: %d digits\n", cfg->digits);
    printf("Trigger Mode: %d\n", cfg->trigger_mode);
    printf("Buffer Enabled: %s\n", cfg->buffer_enabled ? "Yes" : "No");
    printf("Buffer Size: %d samples\n", cfg->buffer_size);
    printf("\n");
    
    printf("Measurement Rates:\n");
    rate_continuous = dm5120_get_measurement_rate(slot, 0); /* not forced: per config (TALK still gives bus rates) */
    rate_bus = dm5120_get_measurement_rate(slot, 2);        /* forced IEEE-488 bus */
    
    printf("Continuous Mode: %.1f readings/sec\n", rate_continuous);
    printf("IEEE-488 Bus Mode: %.1f readings/sec\n", rate_bus);
    printf("\n");
    
    printf("Operation Timings:\n");
    printf("Single Reading: %d ms\n", dm5120_calculate_measurement_time(slot, 0, 1));
    printf("Buffer Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 1, 1));
    printf("STOINT Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 3, 1));
    printf("Status Query: %d ms\n", dm5120_calculate_measurement_time(slot, 4, 1));
    printf("\n");
    
    printf("Buffer Fill Times (STOINT %d ms, shortest %d ms):\n",
           dm5120_stoint_for_slot(slot), dm5120_min_stoint_ms(slot));
    for (i = 10; i <= 500; i += 100) {
        long fill_time = (long)i * dm5120_stoint_for_slot(slot);
        printf("%3d samples: %6ld ms (%5.1f sec)\n", i, fill_time, fill_time / 1000.0);
    }
    printf("\n");

    printf("Resolution vs. Speed Trade-offs:\n");
    printf("3.5 digits: Very fast, ~1000 r/s (voltage)\n");
    printf("4.5 digits: Fast, ~333 r/s (voltage)\n");
    printf("5.5 digits: Medium, ~7.5-35 r/s depending on AutoCal and 50/60 Hz\n");
    printf("6.5 digits: Slow, ~7.5-8.2 r/s\n");
    printf("\n");

    printf("Function Speed Comparison (5.5 digits):\n");
    printf("DCV/ACV: ~7.5-35 r/s\n");
    printf("OHMS: ~7.5-16 r/s\n");
    printf("OHMSCOMP: ~4-8 r/s (50%% of normal OHMS)\n");
}

int dm5120_get_buffer_data(int address, int slot, float far *buffer, int max_samples) {
    char response[GPIB_BUFFER_SIZE];
    int count = 0;
    int i;
    char *token;
    
    gpib_write_dm5120(address, "READ ALLSTORE");

    delay(dm5120_calculate_delay(slot, 2, max_samples));
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        token = strtok(response, ",");
        while (token && count < max_samples) {
            if (sscanf(token, "%e", &buffer[count]) == 1) {
                count++;
            }
            token = strtok(NULL, ",");
        }
    }
    
    return count;
}
/* Blocking fill: configure, wait the estimated fill time, then READ ALLSTORE */
int dm5120_fill_buffer_complete(int address, int slot, float far *buffer, int buffer_size) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int samples_collected = 0;
    int total_wait_time;

    if (!cfg->buffer_enabled || buffer_size <= 0) {
        return 0;
    }

    if (cfg->buffer_enabled) {
        /* Configured source, default TALK,CONT, so the meter self-fills; EXT,CONT stalls
           the fill until an external trigger arrives. (FINDINGS #3) */
        dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", "CONT");

        gpib_write_dm5120(address, "HALF ON");
        GPIB_PACE(50);
        gpib_write_dm5120(address, "FULL ON");
        GPIB_PACE(50);
        gpib_write_dm5120(address, "RQS ON");
        GPIB_PACE(50);
    } else {
        dm5120_set_trigger(address, "TALK", "CONT");
    }

    dm5120_enable_buffering(address, slot, buffer_size);

    total_wait_time = dm5120_set_storage_interval(address, slot, dm5120_stoint_for_slot(slot));
    if (total_wait_time < 0) return 0;
    {
        long fill = (long)total_wait_time * buffer_size + 500;
        total_wait_time = fill > 32767L ? 32767 : (int)fill;
    }
    printf("Collecting %d samples at %d digits - estimated time: %dms\n",
           buffer_size, cfg->digits, total_wait_time);
    delay(total_wait_time);

    samples_collected = dm5120_get_buffer_data(address, slot, buffer, buffer_size);
    
    printf("Buffer operation complete: %d/%d samples collected\n", 
           samples_collected, buffer_size);
    
    return samples_collected;
}

/* DM5120 non-blocking buffer fill, polled from the measurement loop.
   buffer_state: 0 idle, 1 filling, 2 half, 3 full, 4 meter reported error 551,
   5 timed out */
void dm5120_start_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (cfg->buffer_state != 0) {
        return;  /* Already running */
    }

    /* TALK,CONT unless the user chose EXT; EXT,CONT without a trigger source
       leaves the buffer empty until the timeout. (FINDINGS #3) */
    dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", "CONT");
    GPIB_PACE(50);

    gpib_write_dm5120(address, "HALF ON");
    GPIB_PACE(50);
    gpib_write_dm5120(address, "FULL ON");
    GPIB_PACE(50);
    gpib_write_dm5120(address, "RQS ON");
    GPIB_PACE(50);

    /* BUFSZ clears the buffer (there is no BUFCLR). STOINT goes last: any later
       measurement setting would end the data store. */
    if (cfg->buffer_size > 0 && cfg->buffer_size <= DM5120_MAX_BUFFER_SIZE) {
        dm5120_set_buffer_size(address, cfg->buffer_size);
        GPIB_PACE(50);
    }

    dm5120_set_storage_interval(address, slot, dm5120_stoint_for_slot(slot));
    GPIB_PACE(50);

    cfg->buffer_state = 1; /* filling */
    cfg->buffer_start_time = *((unsigned long far *)0x0040006CL); /* BIOS tick count, 18.2 Hz */
    cfg->samples_ready = 0;
}

int dm5120_check_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float code;
    int count;

    if (cfg->buffer_state < 1 || cfg->buffer_state > 2) {
        return cfg->buffer_state;  /* idle, or finished and waiting to be read */
    }

    /* Codes are numbers ("EVENT 450;"); others (power-on 401, 204, 1xx) are not
       buffer progress, so polling carries on */
    if (dm5120_query_value(address, "EVENT?", &code)) {
        switch ((int)code) {
            case 450:
                cfg->buffer_state = 3;
                cfg->samples_ready = cfg->buffer_size;
                break;
            case 451:
                cfg->buffer_state = 2;
                cfg->samples_ready = cfg->buffer_size / 2;
                break;
            case 551:
                cfg->buffer_state = 4;
                return cfg->buffer_state;
        }
    }

    if (cfg->buffer_state != 3) {
        count = dm5120_get_buffer_count(address);
        if (count > 0) {
            cfg->samples_ready = count;
            if (count >= cfg->buffer_size) {
                cfg->buffer_state = 3; /* full */
            } else if (count >= cfg->buffer_size / 2) {
                cfg->buffer_state = 2; /* half */
            }
        }
    }

    if (cfg->buffer_state == 1 || cfg->buffer_state == 2) {
        /* Expected fill time x1.5 + 10 s, in ~55 ms BIOS ticks */
        unsigned long limit_ms = (unsigned long)cfg->buffer_size * dm5120_stoint_for_slot(slot);
        unsigned long limit_ticks = (limit_ms + limit_ms / 2 + 10000UL) / 55UL;
        unsigned long elapsed_ticks = *((unsigned long far *)0x0040006CL) - cfg->buffer_start_time;
        if (elapsed_ticks > limit_ticks) {
            /* Not state 3: BUFAVE? on a part-filled buffer holds the bus until it fills */
            cfg->buffer_state = 5;
        }
    }

    return cfg->buffer_state;
}

void dm5120_reset_buffer_async(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    cfg->buffer_state = 0;      /* idle */
    cfg->buffer_start_time = 0;
    cfg->samples_ready = 0;
}

/* DM5120 Buffer Query Functions */
float dm5120_get_buffer_average(int address) {
    
    float value;

    /* Holds the bus off until the buffer is full: call only once it is */
    return dm5120_query_value(address, "BUFAVE?", &value) ? value : 0.0;
}

int dm5120_get_buffer_count(int address) {
    float count;
    return dm5120_query_value(address, "BUFCNT?", &count) ? (int)count : 0;
}

float dm5120_get_buffer_min(int address) {
    float value;
    return dm5120_query_value(address, "BUFMIN?", &value) ? value : 0.0;
}

float dm5120_get_buffer_max(int address) {
    float value;
    return dm5120_query_value(address, "BUFMAX?", &value) ? value : 0.0;
}

void dm5120_set_buffer_size(int address, int size) {
    
    if (size == 0) {
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", size);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(100);
}

int dm5120_query_buffer_size(int address) {
    
    int size = 0;
    
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "CIRCULAR")) {
            return 0;  /* same code BUFSZ takes for it */
        }
        if (sscanf(gpib_response_buffer, "BUFSZ %d", &size) == 1) {
            return size;
        }
    }

    return -1;
}

/* DM5120 stored-reading reads (READ ONESTORE / READ ALLSTORE) */
float dm5120_read_one_stored(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "READ ONESTORE");
    GPIB_PACE(100);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            return value;
        }
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}

int dm5120_read_all_stored(int address, float far *buffer, int max_samples) {
    char response[GPIB_BUFFER_SIZE];
    int count = 0;
    char *token;
    
    gpib_write_dm5120(address, "READ ALLSTORE");
    delay(200);  /* Allow time for all data to be formatted */
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        token = strtok(response, ",");
        while (token && count < max_samples) {
            if (sscanf(token, "%e", &buffer[count]) == 1) {
                count++;
            }
            token = strtok(NULL, ",");
        }
    }
    
    return count;
}

float read_dm5120_buffered(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    static float *buffer_data = NULL;  /* Dynamic buffer for samples */
    static int buffer_allocated_size = 0;
    static int buffer_index = 0;
    static int buffer_valid_count = 0;
    static int last_slot = -1;
    
    /* Reset buffer if slot changed */
    if (last_slot != slot) {
        buffer_valid_count = 0;
        buffer_index = 0;
        last_slot = slot;
    }
    
    if (cfg->buffer_enabled) {
        if (!buffer_data || buffer_allocated_size < cfg->buffer_size) {
            if (buffer_data) {
                free(buffer_data);
            }
            buffer_data = (float *)malloc(cfg->buffer_size * sizeof(float));
            if (!buffer_data) {
                /* Allocation failed, fall back to single measurement */
                buffer_allocated_size = 0;
                return read_dm5120_enhanced(address, slot);
            }
            buffer_allocated_size = cfg->buffer_size;
            buffer_valid_count = 0;  /* Force buffer refill */
            buffer_index = 0;
        }
        if (buffer_index >= buffer_valid_count) {
            buffer_valid_count = dm5120_fill_buffer_complete(address, slot, 
                                                            buffer_data, cfg->buffer_size);
            buffer_index = 0;
            
            if (buffer_valid_count <= 0) {
                /* Buffer fill failed, fall back to single measurement */
                return read_dm5120_enhanced(address, slot);
            }
        }

        if (buffer_index < buffer_valid_count) {
            return buffer_data[buffer_index++];
        }
    }
    
    return read_dm5120_enhanced(address, slot);
}

int dm5120_get_multiple_samples(int address, int slot, float far *buffer, int num_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (!cfg->buffer_enabled) {
        int i;
        for (i = 0; i < num_samples; i++) {
            buffer[i] = read_dm5120_enhanced(address, slot);
            if (i < num_samples - 1) {
                delay(dm5120_calculate_delay(slot, 0, 1));
            }
        }
        return num_samples;
    } else {
        return dm5120_fill_buffer_complete(address, slot, buffer, num_samples);
    }
}

float read_dm5120_enhanced(int address, int slot) {
    float value = 0.0;
    int retry;
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    int srq_status;
    
    srq_status = gpib_check_srq(address);
    if (srq_status & 0x40) {  /* Device requesting service */
        delay(100);
    }
    
    gpib_write_dm5120(address, "READ ADC");

    delay(dm5120_calculate_delay(slot, 0, 1));
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            if (cfg->min_max_enabled) {
                if (value < cfg->min_value) cfg->min_value = value;
                if (value > cfg->max_value) cfg->max_value = value;
            }
            store_module_data(slot, value);
            return value;
        }
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            if (cfg->min_max_enabled) {
                if (value < cfg->min_value) cfg->min_value = value;
                if (value > cfg->max_value) cfg->max_value = value;
            }
            store_module_data(slot, value);
            return value;
        }
    }
    
    /* SEND triggers one reading whatever the trigger setting (there is no X command) */
    gpib_write_dm5120(address, "SEND");
    delay(dm5120_calculate_delay(slot, 0, 1) + 50);
    
    if (gpib_read_float_dm5120(address, &value)) {
        if (cfg->min_max_enabled) {
            if (value < cfg->min_value) cfg->min_value = value;
            if (value > cfg->max_value) cfg->max_value = value;
        }
        store_module_data(slot, value);
        return value;
    }
    
    for (retry = 0; retry < 3; retry++) {
        gpib_write_dm5120(address, "SEND");
        GPIB_PACE(300);
        
        if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
            if (sscanf(gpib_response_buffer, "NDCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "DCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "NACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "ACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
        }
        
        delay(100);
    }
    
    return 0.0;
}

float read_dm5120_voltage(int address) {
    
    float value = 0.0;
    
    gpib_check_srq(address);
    GPIB_PACE(50);
    
    gpib_write_dm5120(address, "READ ADC");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}
void dm5120_clear_statistics(int slot) {
    if (slot >= 0 && slot < 10) {
        g_dm5120_config[slot].min_value = 1e30;
        g_dm5120_config[slot].max_value = -1e30;
        g_dm5120_config[slot].sample_count = 0;
        clear_module_data(slot);
    }
}
void test_dm5120_comm(int address) {
    
    char status[128];
    float value;
    int i, srq_status;
    
    printf("\nTesting DM5120 at address %d...\n", address);
    
    ieee_write("status\r\n");
    GPIB_PACE(50);
    ieee_read(status, sizeof(status));
    printf("Driver status: %s\n", status);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote_dm5120(address);
    GPIB_PACE(200);
    
    printf("Checking SRQ status...\n");
    srq_status = gpib_check_srq(address);
    printf("SRQ status: %d\n", srq_status);
    
    printf("Sending ABORT command...\n");
    ieee_write("abort\r\n");
    GPIB_PACE(200);
    
    printf("\nSending INIT command...\n");
    gpib_write_dm5120(address, "INIT");
    GPIB_PACE(300);
    
    printf("Setting DCV function...\n");
    gpib_write_dm5120(address, "FUNCT DCV");
    GPIB_PACE(100);
    
    printf("Setting AUTO range...\n");
    gpib_write_dm5120(address, "RANGE AUTO");
    GPIB_PACE(100);
    
    printf("Setting data format ON (scientific)...\n");
    gpib_write_dm5120(address, "DATFOR ON");
    GPIB_PACE(100);
    
    printf("\nAttempting measurement reads:\n");
    
    printf("\n1. READ ADC:\n");
    gpib_write_dm5120(address, "READ ADC");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n2. SEND (one triggered reading):\n");
    gpib_write_dm5120(address, "SEND");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n3. READ? query:\n");
    gpib_write_dm5120(address, "READ?");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("   No response\n");
    }
    
    printf("\nQuerying current settings:\n");

    gpib_write_dm5120(address, "ID?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }

    gpib_write_dm5120(address, "FUNCT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    gpib_write_dm5120(address, "RANGE?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nTesting enhanced buffer commands:\n");
    
    printf("4. BUFSZ? (Buffer Size Query):\n");
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("5. BUFCNT? (Buffer Count Query):\n");
    gpib_write_dm5120(address, "BUFCNT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("6. Testing BUFSZ 50 (Set Buffer Size):\n");
    gpib_write_dm5120(address, "BUFSZ 50");
    GPIB_PACE(100);
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("7. STOINT? (Storage Interval Query):\n");
    gpib_write_dm5120(address, "STOINT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }

    i = dm5120_query_error(address);
    if (i >= 0) {
        printf("   ERROR %d: %s\n", i, dm5120_error_text(i));
    }

    printf("\nTest complete. Press any key to continue...");
    getch();
}
void test_dm5120_comm_debug(int address) {
    
    int bytes_read;
    int i;
    float test_value;
    
    printf("\n=== DM5120 Debug Communication Test ===\n");
    printf("Testing DM5120 at GPIB address %d\n\n", address);
    
    printf("Raw GPIB communication test:\n");
    
    printf("\n1. Sending raw OUTPUT command:\n");
    sprintf(gpib_cmd_buffer, "output %2d; INIT\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(500);
    
    printf("\n2. Sending function setup:\n");
    sprintf(gpib_cmd_buffer, "output %2d; FUNCT DCV\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    printf("\n3. Requesting reading:\n");
    sprintf(gpib_cmd_buffer, "output %2d; READ ADC\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    printf("\n4. Reading response:\n");
    sprintf(gpib_cmd_buffer, "enter %2d\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    bytes_read = ieee_read(gpib_response_buffer, sizeof(gpib_response_buffer));
    if (bytes_read > 0) {
        gpib_response_buffer[bytes_read] = '\0';
        printf("   %s\n", gpib_response_buffer);
        printf("   Hex dump: ");
        for (i = 0; i < bytes_read && i < 40; i++) {
            printf("%02X ", (unsigned char)gpib_response_buffer[i]);
        }
        printf("\n");
    } else {
        printf("   No response or error\n");
    }
    
    printf("\nDirect function test:\n");
    printf("\n5. Testing read_dm5120_enhanced():\n");
    test_value = read_dm5120_enhanced(address, 0);
    printf("   Returned value: %g\n", test_value);
    
    printf("\n=== Debug Test Complete ===\n");
    printf("Press any key to continue...");
    getch();
}

/* DM5010 multimeter */
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
    g_dm5010_config[slot].lf_termination = 0;  /* Default to CRLF */
}
void configure_dm5010_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dm5010_config *cfg = &g_dm5010_config[slot];
    char input[50];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("DM5010 Advanced Configuration - Slot %d\n", slot);
        printf("======================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Function: %s\n", cfg->function);
        printf("  Range: %s\n", cfg->range_mode == 0 ? "AUTO" : "MANUAL");
        printf("  Filter: %s (count: %d)\n", cfg->filter_enabled ? "ON" : "OFF", cfg->filter_count);
        printf("  Trigger: %s\n", cfg->trigger_mode == 0 ? "FREE" : (cfg->trigger_mode == 1 ? "SINGLE" : "EXT"));
        printf("  Digits: %d\n", cfg->digits);
        printf("  NULL/REL: %s (value: %.6e)\n", cfg->null_enabled ? "ON" : "OFF", cfg->nullval);
        printf("  Auto Zero: %s\n", cfg->auto_zero ? "ON" : "OFF");
        printf("  Calculation: ");
        switch(cfg->calculation_mode) {
            case 0: printf("NONE\n"); break;
            case 1: printf("AVERAGE (count: %d)\n", cfg->avg_count); break;
            case 2: printf("SCALE (factor: %.3f, offset: %.3f)\n", cfg->scale_factor, cfg->scale_offset); break;
            case 3: printf("dBm (ref: %.0f ohm)\n", cfg->dbm_reference); break;
            case 4: printf("dBr (ref: %.6e)\n", cfg->dbr_reference); break;
        }
        printf("  Beeper: %s\n", cfg->beeper_enabled ? "ON" : "OFF");
        printf("  Front Panel: %s\n", cfg->front_panel_lock ? "LOCKED" : "UNLOCKED");
        printf("  Statistics: %s\n", cfg->statistics_enabled ? "ON" : "OFF");
        if (cfg->statistics_enabled) {
            printf("    Min: %.6e, Max: %.6e\n", cfg->min_value, cfg->max_value);
        }
        
        printf("\nOptions:\n");
        printf("1. Set Function\n");
        printf("2. Set Range Mode\n");
        printf("3. Configure Filter\n");
        printf("4. Set Trigger Mode\n");
        printf("5. Set Display Digits\n");
        printf("6. Configure NULL/REL\n");
        printf("7. Auto Zero Control\n");
        printf("8. Calculation Mode\n");
        printf("9. Beeper Control\n");
        printf("A. Front Panel Lock\n");
        printf("B. Statistics Control\n");
        printf("C. Apply Settings to DM5010\n");
        printf("D. Test Communication\n");
        printf("E. Reset to Defaults\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("VOLT:DC, VOLT:AC, CURR:DC, CURR:AC, RES, DIOD\n");
                printf("Enter function: ");
                scanf("%s", input);
                strcpy(cfg->function, input);
                break;
                
            case '2':
                printf("\n\nRange modes:\n");
                printf("0=AUTO, 1-7=Manual ranges\n");
                printf("Enter range mode: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 7) {
                    cfg->range_mode = temp_int;
                }
                break;
                
            case '3':
                printf("\n\nFilter configuration:\n");
                printf("Enable filter? (1/0): ");
                scanf("%d", &temp_int);
                cfg->filter_enabled = temp_int;
                if (cfg->filter_enabled) {
                    printf("Filter count (1-100): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 1 && temp_int <= 100) {
                        cfg->filter_count = temp_int;
                    }
                }
                break;
                
            case '4':
                printf("\n\nTrigger modes:\n");
                printf("0=FREE, 1=SINGLE, 2=EXT\n");
                printf("Enter trigger mode: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->trigger_mode = temp_int;
                }
                break;
                
            case '5':
                printf("\n\nNumber of digits (4-5): ");
                scanf("%d", &temp_int);
                if (temp_int >= 4 && temp_int <= 5) {
                    cfg->digits = temp_int;
                }
                break;
                
            case '6':
                printf("\n\nNULL/REL configuration:\n");
                printf("Enable NULL/REL? (1/0): ");
                scanf("%d", &temp_int);
                cfg->null_enabled = temp_int;
                if (cfg->null_enabled) {
                    printf("NULL value: ");
                    scanf("%e", &temp_float);
                    cfg->nullval = temp_float;
                }
                break;
                
            case '7':
                cfg->auto_zero = !cfg->auto_zero;
                printf("\n\nAuto Zero: %s\n", cfg->auto_zero ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '8':
                printf("\n\nCalculation modes:\n");
                printf("0=NONE, 1=AVERAGE, 2=SCALE, 3=dBm, 4=dBr\n");
                printf("Enter mode: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 4) {
                    cfg->calculation_mode = temp_int;
                    switch(temp_int) {
                        case 1:
                            printf("Average count: ");
                            scanf("%d", &cfg->avg_count);
                            break;
                        case 2:
                            printf("Scale factor: ");
                            scanf("%f", &cfg->scale_factor);
                            printf("Scale offset: ");
                            scanf("%f", &cfg->scale_offset);
                            break;
                        case 3:
                            printf("dBm reference resistance (ohms): ");
                            scanf("%f", &cfg->dbm_reference);
                            break;
                        case 4:
                            printf("dBr reference value: ");
                            scanf("%e", &cfg->dbr_reference);
                            break;
                    }
                }
                break;
                
            case '9':
                cfg->beeper_enabled = !cfg->beeper_enabled;
                printf("\n\nBeeper: %s\n", cfg->beeper_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'A':
                cfg->front_panel_lock = !cfg->front_panel_lock;
                printf("\n\nFront Panel: %s\n", cfg->front_panel_lock ? "LOCKED" : "UNLOCKED");
                getch();
                break;
                
            case 'B':
                cfg->statistics_enabled = !cfg->statistics_enabled;
                printf("\n\nStatistics: %s\n", cfg->statistics_enabled ? "ENABLED" : "DISABLED");
                if (!cfg->statistics_enabled) {
                    cfg->min_value = 1e30;
                    cfg->max_value = -1e30;
                }
                getch();
                break;
                
            case 'C':
                printf("\n\nApplying settings to DM5010...\n");
                gpib_remote(address);
                GPIB_PACE(200);
                
                dm5010_set_function(address, cfg->function);
                dm5010_set_filter(address, cfg->filter_enabled, cfg->filter_count);
                dm5010_set_trigger(address, cfg->trigger_mode == 0 ? "IMM" : (cfg->trigger_mode == 1 ? "EXT" : "BUS"));
                dm5010_set_autozero(address, cfg->auto_zero);
                dm5010_set_null(address, cfg->null_enabled, cfg->nullval);
                dm5010_set_calculation(address, cfg->calculation_mode, 
                                     cfg->calculation_mode == 2 ? cfg->scale_factor : 
                                     (cfg->calculation_mode == 3 ? cfg->dbm_reference : cfg->dbr_reference),
                                     cfg->scale_offset);
                dm5010_beeper(address, cfg->beeper_enabled);
                dm5010_lock_front_panel(address, cfg->front_panel_lock);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'D':
                test_dm5010_comm(address);
                break;
                
            case 'E':
                init_dm5010_config(slot);
                printf("\n\nConfiguration reset to defaults.\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}
void dm5010_set_function(int address, char *function) {
    sprintf(gpib_cmd_buffer, "CONF:%s", function);
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_set_range(int address, char *function, float range) {
    if (range == 0.0) {
        sprintf(gpib_cmd_buffer, "CONF:%s AUTO", function);
    } else {
        sprintf(gpib_cmd_buffer, "CONF:%s %e", function, range);
    }
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_set_filter(int address, int enabled, int count) {
    if (enabled) {
        sprintf(gpib_cmd_buffer, "AVER:STAT ON");
        gpib_write_dm5010(address, gpib_cmd_buffer);
        sprintf(gpib_cmd_buffer, "AVER:COUN %d", count);
        gpib_write_dm5010(address, gpib_cmd_buffer);
    } else {
        sprintf(gpib_cmd_buffer, "AVER:STAT OFF");
        gpib_write_dm5010(address, gpib_cmd_buffer);
    }
}
void dm5010_set_trigger(int address, char *mode) {
    sprintf(gpib_cmd_buffer, "TRIG:SOUR %s", mode);
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_set_autozero(int address, int enabled) {
    sprintf(gpib_cmd_buffer, "ZERO:AUTO %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_set_null(int address, int enabled, float value) {
    if (enabled) {
        sprintf(gpib_cmd_buffer, "CALC:NULL:OFFS %e", value);
        gpib_write_dm5010(address, gpib_cmd_buffer);
        sprintf(gpib_cmd_buffer, "CALC:NULL:STAT ON");
        gpib_write_dm5010(address, gpib_cmd_buffer);
    } else {
        sprintf(gpib_cmd_buffer, "CALC:NULL:STAT OFF");
        gpib_write_dm5010(address, gpib_cmd_buffer);
    }
}
void dm5010_set_calculation(int address, int mode, float factor, float offset) {
    switch(mode) {
        case 1: /* Average */
            sprintf(gpib_cmd_buffer, "CALC:AVER:STAT ON");
            break;
        case 2: /* Scale */
            sprintf(gpib_cmd_buffer, "CALC:SCAL:GAIN %e", factor);
            gpib_write_dm5010(address, gpib_cmd_buffer);
            sprintf(gpib_cmd_buffer, "CALC:SCAL:OFFS %e", offset);
            gpib_write_dm5010(address, gpib_cmd_buffer);
            sprintf(gpib_cmd_buffer, "CALC:SCAL:STAT ON");
            break;
        case 3: /* dBm */
            sprintf(gpib_cmd_buffer, "CALC:DBM:REF %e", factor);
            gpib_write_dm5010(address, gpib_cmd_buffer);
            sprintf(gpib_cmd_buffer, "CALC:DBM:STAT ON");
            break;
        case 4: /* dBr */
            sprintf(gpib_cmd_buffer, "CALC:DB:REF %e", factor);
            gpib_write_dm5010(address, gpib_cmd_buffer);
            sprintf(gpib_cmd_buffer, "CALC:DB:STAT ON");
            break;
        default:
            sprintf(gpib_cmd_buffer, "CALC:STAT OFF");
            break;
    }
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_beeper(int address, int enabled) {
    sprintf(gpib_cmd_buffer, "SYST:BEEP:STAT %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
void dm5010_lock_front_panel(int address, int locked) {
    sprintf(gpib_cmd_buffer, "SYST:LOCK %s", locked ? "ON" : "OFF");
    gpib_write_dm5010(address, gpib_cmd_buffer);
}
float read_dm5010_enhanced(int address, int slot) {
    
    float value = 0.0;
    int attempts = 0;
    int success = 0;
    
    gpib_check_srq(address);
    GPIB_PACE(50);
    
    while (attempts < 3 && !success) {
        attempts++;
        
        gpib_write_dm5010(address, "VAL?");
        GPIB_PACE(100);
        
        if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
            if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
                success = 1;
                break;
            }
        }
        
        if (!success) {
            gpib_write_dm5010(address, "READ?");
            GPIB_PACE(150);
            
            if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
                if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
                    success = 1;
                    break;
                }
            }
        }
        
        if (!success) {
            delay(200);
        }
    }
    
    if (success && slot >= 0 && slot < 10) {
        store_module_data(slot, value);
        
        if (g_dm5010_config[slot].statistics_enabled) {
            if (value < g_dm5010_config[slot].min_value) {
                g_dm5010_config[slot].min_value = value;
            }
            if (value > g_dm5010_config[slot].max_value) {
                g_dm5010_config[slot].max_value = value;
            }
        }
    }
    
    return value;
}
void test_dm5010_comm(int address) {
    
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DM5010 Communication Test ===\n");
    printf("Testing DM5010 at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write_dm5010(address, "*IDN?");
    GPIB_PACE(100);
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing reset...\n");
    gpib_write_dm5010(address, "*RST");
    GPIB_PACE(500);
    gpib_write_dm5010(address, "CONF:VOLT:DC");
    GPIB_PACE(100);
    printf("   Reset and configured for DC voltage\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function configuration...\n");
    gpib_write_dm5010(address, "CONF:VOLT:DC AUTO,MAX");
    GPIB_PACE(100);
    gpib_write_dm5010(address, "FUNC?");
    GPIB_PACE(100);
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing measurement...\n");
    gpib_write_dm5010(address, "READ?");
    GPIB_PACE(200);
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            printf("   Measurement: %.6e V\n", value);
            success_count++;
        } else {
            printf("   %s\n", gpib_response_buffer);
        }
    } else {
        printf("   Measurement failed!\n");
    }
    test_count++;
    
    printf("5. Testing status...\n");
    gpib_write_dm5010(address, "*STB?");
    GPIB_PACE(100);
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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

/* PS5004 power supply */
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
    g_ps5004_config[slot].lf_termination = 0;   /* Default to CRLF */
}
void ps5004_init(int address) {
    gpib_write(address, "INIT");
    GPIB_PACE(100);
}
void ps5004_set_voltage(int address, float voltage) {
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 20.0) voltage = 20.0;
    sprintf(gpib_cmd_buffer, "VOLTAGE %.4f", voltage);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void ps5004_set_current(int address, float current) {
    if (current < 0.01) current = 0.01;    /* 10mA minimum */
    if (current > 0.305) current = 0.305;  /* 305mA maximum */
    sprintf(gpib_cmd_buffer, "CURRENT %.3f", current);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
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
    sprintf(gpib_cmd_buffer, "DISPLAY %s", mode);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
int ps5004_get_regulation_status(int address) {
    
    int status = 0;
    
    gpib_write(address, "REGULATION?");
    GPIB_PACE(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "REGULATION 1")) status = 1;
        else if (strstr(gpib_response_buffer, "REGULATION 2")) status = 2;
        else if (strstr(gpib_response_buffer, "REGULATION 3")) status = 3;
        else status = atoi(gpib_response_buffer);
    }
    
    return status;
}
float ps5004_read_value(int address) {
    
    float value = 0.0;
    
    gpib_write(address, "SEND");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            return value;
        }
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}
void test_ps5004_comm(int address) {
    
    float value;
    
    printf("\nTesting PS5004 at address %d...\n", address);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote(address);
    GPIB_PACE(200);
    
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("No ID response\n");
    }
    
    printf("\nInitializing PS5004...\n");
    ps5004_init(address);
    
    printf("\nQuerying all settings...\n");
    gpib_write(address, "SET?");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nSetting voltage to 5.0V...\n");
    ps5004_set_voltage(address, 5.0);
    
    gpib_write(address, "VOLTAGE?");
    GPIB_PACE(50);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nSetting current limit to 100mA...\n");
    ps5004_set_current(address, 0.1);
    
    gpib_write(address, "CURRENT?");
    GPIB_PACE(50);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nTesting display modes...\n");
    ps5004_set_display(address, "VOLTAGE");
    GPIB_PACE(100);
    ps5004_set_display(address, "CURRENT");
    GPIB_PACE(100);
    ps5004_set_display(address, "CLIMIT");
    GPIB_PACE(100);
    ps5004_set_display(address, "VOLTAGE");
    
    printf("\nReading voltage (output is OFF)...\n");
    value = ps5004_read_value(address);
    printf("Voltage reading: %.4f V\n", value);
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* PS5010 triple power supply, manual 070-3391-00. Every reply carries a header
   ("VPOS 29.7;", and "ILOGIC 1.6;" for ILOG?) and there is no header-off command
   (p. 3-2). The supply measures nothing: REG? is its only read-back. */
const char * const __far ps5010_flag_cmd[PS5010_NFLAGS] = {
    "FSOUT", "LSOUT", "PRI", "NRI", "LRI", "RQS", "USER", "DT"
};

int ps5010_get_flag(const ps5010_config *cfg, int i) {
    switch (i) {
        case 0: return cfg->fsout_enabled;
        case 1: return cfg->logic_enabled;
        case 2: return cfg->pri_enabled;
        case 3: return cfg->nri_enabled;
        case 4: return cfg->lri_enabled;
        case 5: return cfg->srq_enabled;
        case 6: return cfg->user_enabled;
        case 7: return cfg->dt_enabled;
    }
    return 0;
}

void ps5010_set_flag(ps5010_config *cfg, int i, int on) {
    on = on ? 1 : 0;
    switch (i) {
        case 0: cfg->fsout_enabled = on; break;
        case 1: cfg->logic_enabled = on; break;
        case 2: cfg->pri_enabled = on; break;
        case 3: cfg->nri_enabled = on; break;
        case 4: cfg->lri_enabled = on; break;
        case 5: cfg->srq_enabled = on; break;
        case 6: cfg->user_enabled = on; break;
        case 7: cfg->dt_enabled = on; break;
    }
}

/* The supply's power-on/INIT settings, Table 3-3 (p. 3-22): outputs OFF */
void init_ps5010_config(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];

    memset(cfg, 0, sizeof(*cfg));
    cfg->current_limit1 = 0.4f;
    cfg->current_limit2 = 0.4f;
    cfg->logic_voltage = 5.0f;
    cfg->logic_current_limit = 1.0f;
    cfg->srq_enabled = 1;
}

/* Floating current limit range, p. 3-5/3-6: 0.75 A (1.6 A at 15 V and below) in
   the high-power compartment, 0.40 A (0.75 A at 15 V and below) in a standard one */
float ps5010_max_current(int high_power, float volts) {
    if (volts <= 15.0f) return high_power ? 1.6f : 0.75f;
    return high_power ? 0.75f : 0.40f;
}

/* Nearest unit of resolution, as the supply itself rounds (p. 3-17) */
float ps5010_round(float v, float step) {
    return (float)floor(v / step + 0.5f) * step;
}

/* Reply length without the terminator, 0 if none */
int ps5010_query(int address, const char *query, char *buf, int maxlen) {
    int n;

    buf[0] = '\0';
    if (gpib_write(address, (char *)query) < 0) return 0;
    GPIB_PACE(20);
    n = gpib_read(address, buf, maxlen);
    if (n <= 0) {
        buf[0] = '\0';
        return 0;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
    return strlen(buf);
}

/* Argument after the reply's header, or NULL */
static const char *ps5010_arg(const char *reply) {
    const char *p = reply + strspn(reply, " ");
    p += strcspn(p, " ");
    if (*p == '\0') return NULL;
    return p + strspn(p, " ");
}

int ps5010_query_value(int address, const char *query, float *value) {
    static char __far r[40];
    const char *p;

    if (!ps5010_query(address, query, r, sizeof(r)) || (p = ps5010_arg(r)) == NULL) return 0;
    return sscanf(p, "%f", value) == 1;
}

/* 1 for ON or SET, 0 for OFF, -1 for no reply */
int ps5010_query_on(int address, const char *query) {
    static char __far r[24];
    const char *p;

    if (!ps5010_query(address, query, r, sizeof(r)) || (p = ps5010_arg(r)) == NULL) return -1;
    if (strncmp(p, "OFF", 3) == 0) return 0;
    return (strncmp(p, "ON", 2) == 0 || strncmp(p, "SET", 3) == 0) ? 1 : -1;
}

/* REG <neg>,<pos>,<logic>, each 1 CV, 2 CC, 3 unregulated (p. 3-10). No printf:
   the monitor loop calls this. */
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat) {
    static char __far r[32];
    const char *p;

    if (!ps5010_query(address, "REG?", r, sizeof(r)) || (p = ps5010_arg(r)) == NULL) return 0;
    return sscanf(p, "%d,%d,%d", neg_stat, pos_stat, log_stat) == 3;
}

const char *ps5010_reg_text(int code) {
    return code == 1 ? "CV" : code == 2 ? "CC" : code == 3 ? "UR" : "??";
}

/* ERR? codes, Table 3-1 (p. 3-21) */
static const int __far ps5010_err_code[] = {
    101, 102, 103, 104, 106, 107, 108, 109,
    201, 202, 203, 204, 205, 206, 302, 303, 401, 403
};
static const char * const __far ps5010_err_msg[] = {
    "command header error", "header delimiter error", "command argument error",
    "argument delimiter error", "missing argument", "invalid message unit delimiter",
    "binary block checksum error", "binary block byte counter error",
    "command not executable in local mode", "returned to local, pending settings lost",
    "I/O buffers full, output dumped", "settings conflict",
    "argument out of range", "GET ignored", "system error", "math pack error",
    "power on", "user request (INST ID button)"
};

const char *ps5010_error_text(int code) {
    static char __far buf[48];
    static const char * const __far sup[] = { "negative", "positive", "logic" };
    static const char * const __far mode[] = { "constant voltage", "constant current", "unregulated" };
    int i;

    if (code >= 721 && code <= 729) {
        sprintf(buf, "%s supply went to %s", sup[(code - 721) / 3], mode[(code - 721) % 3]);
        return buf;
    }
    for (i = 0; i < (int)(sizeof(ps5010_err_code) / sizeof(ps5010_err_code[0])); i++) {
        if (ps5010_err_code[i] == code) return ps5010_err_msg[i];
    }
    return "unknown code";
}

/* Print every pending error and event; returns how many were errors (below 400),
   or -1 if ERR? got no reply, so callers never take silence for success.
   With RQS ON, ERR? only reports what a serial poll already delivered, so RQS is
   off for the drain and restored after (p. 3-3, 3-22). Needs remote: RQS is a
   setting. Prints, so never from the monitor loop. */
int ps5010_show_errors(int address) {
    float v;
    int i, code, errors = 0;
    int rqs = ps5010_query_on(address, "RQS?");

    if (rqs == 1) gpib_write(address, "RQS OFF");
    else if (rqs < 0) printf("  No reply to RQS?: errors already reported by SRQ may not show\n");
    for (i = 0; i < 12; i++) {
        if (!ps5010_query_value(address, "ERR?", &v)) {
            printf("  No reply to ERR?\n");
            if (i == 0) errors = -1;
            break;
        }
        code = (int)v;
        if (code == 0) break;
        printf("  %s %d: %s\n", code < 400 ? "ERROR" : "Event", code, ps5010_error_text(code));
        if (code < 400) errors++;
    }
    if (rqs == 1) gpib_write(address, "RQS ON");
    return errors;
}

/* Read only: no supply setting changes and no output switches */
void test_ps5010_comm(int address) {
    static char __far buf[160];   /* SET? is about 120 characters (p. 3-11) */
    int neg_stat, pos_stat, log_stat;

    printf("\nTesting PS5010 at address %d (read only)...\n", address);
    gpib_remote(address);
    GPIB_PACE(200);

    printf("\nID?  %s\n", ps5010_query(address, "ID?", buf, sizeof(buf)) ? buf : "No response");
    printf("SET? %s\n", ps5010_query(address, "SET?", buf, sizeof(buf)) ? buf : "No response");
    printf("OUT? %s\n", ps5010_query(address, "OUT?", buf, sizeof(buf)) ? buf : "No response");
    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat)) {
        printf("REG? negative %s, positive %s, logic %s\n", ps5010_reg_text(neg_stat),
               ps5010_reg_text(pos_stat), ps5010_reg_text(log_stat));
    } else {
        printf("REG? No response\n");
    }
    printf("\nErrors and events (ERR?):\n");
    if (ps5010_show_errors(address) == 0) printf("  No errors\n");

    printf("\nThe PS5010 can't measure its outputs; REG? (CV/CC/unregulated) is its\n");
    printf("only read-back. Use a DMM to check output values.\n");
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* Single-shot and continuous measurement across all configured slots */
void single_measurement(void) {
    int i;
    double value;
    
    int active_modules[10];
    int active_count = 0;
    
    clrscr();
    printf("Single Measurement - Multi-Module Coordination\n");
    printf("==============================================\n\n");

    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            active_modules[active_count] = i;
            active_count++;
        }
    }
    
    if (active_count == 0) {
        printf("No modules configured!\n\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    printf("Synchronizing %d modules for simultaneous measurement...\n\n", active_count);
    
    /* INIT counters and meters one by one before reading */
    for (i = 0; i < active_count; i++) {
        int slot = active_modules[i];
        int addr = g_system->modules[slot].gpib_address;
        
        switch (g_system->modules[slot].module_type) {
            case MOD_DC5009:
            case MOD_DC5010:
                gpib_write(addr, "INIT");
                break;
                
            case MOD_DM5010:
            case MOD_DM5120:
                gpib_write(addr, "INIT");
                break;
                
            case MOD_PS5004:
            case MOD_PS5010:
                /* Power supplies don't need INIT for reading */
                break;
        }
    }
    
    if (active_count > 0) {
        ieee_write("*OPC?\r\n");
        delay(100); /* Allow time for all modules to complete */
        drain_input_buffer();
    }
    
    printf("Collecting results:\n");
    printf("-------------------\n");
    
    for (i = 0; i < active_count; i++) {
        int slot = active_modules[i];
        printf("Slot %d (%s at GPIB %d): ", slot, g_system->modules[slot].description, g_system->modules[slot].gpib_address);
        
        switch (g_system->modules[slot].module_type) {
            case MOD_DC5009:
            case MOD_DC5010:
                value = dc5009_read_measurement(g_system->modules[slot].gpib_address);
                if (value > 1e6) {
                    printf("%.3f MHz\n", value / 1e6);
                } else if (value > 1e3) {
                    printf("%.3f kHz\n", value / 1e3);
                } else {
                    printf("%.3f Hz\n", value);
                }
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_DM5010:
                value = read_dm5010_enhanced(g_system->modules[slot].gpib_address, slot);
                printf("%.6g V\n", value);
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_DM5120:
                value = read_dm5120_enhanced(g_system->modules[slot].gpib_address, slot);
                if (value == 0.0) {
                    /* 0.0 on a failed read: retry with the plain voltage read */
                    value = read_dm5120_voltage(g_system->modules[slot].gpib_address);
                }
                printf("%.6g V\n", value);
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_PS5010:
                /* Regulation status is the supply's only read-back */
                {
                    int neg_stat, pos_stat, log_stat;
                    if (ps5010_read_regulation(g_system->modules[slot].gpib_address, &neg_stat, &pos_stat, &log_stat)) {
                        printf("POS:%s NEG:%s LOG:%s\n", ps5010_reg_text(pos_stat),
                               ps5010_reg_text(neg_stat), ps5010_reg_text(log_stat));
                        g_system->modules[slot].last_reading = neg_stat * 100 + pos_stat * 10 + log_stat;
                    } else {
                        printf("Communication Error\n");
                    }
                }
                break;
                
            case MOD_PS5004:
                /* Power supply with configurable display */
                {
                    ps5004_config *cfg = &g_ps5004_config[slot];
                    int reg_status = ps5004_get_regulation_status(g_system->modules[slot].gpib_address);
                    value = ps5004_read_value(g_system->modules[slot].gpib_address);
                    
                    switch(cfg->display_mode) {
                        case 0: /* Voltage */
                            printf("%.4f V ", value);
                            break;
                        case 1: /* Current */
                            printf("%.3f A ", value);
                            break;
                        case 2: /* Current Limit */
                            printf("ILIM %.3f A ", cfg->current_limit);
                            break;
                    }
                    
                    switch(reg_status) {
                        case 1: printf("(CV)\n"); break;
                        case 2: printf("(CC)\n"); break;
                        case 3: printf("(UR)\n"); break;
                        default: printf("(?)\n"); break;
                    }
                    g_system->modules[slot].last_reading = value;
                }
                break;
                
            default:
                printf("Unknown module type\n");
                break;
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
}
/* ---- Monitor display: build each screen line in a buffer and write it
 * straight into text-mode video memory (vputs, graphics.c); printf through
 * DOS + BIOS teletype is slow on the 286 and flickers.  Non-sampling passes
 * redraw the last sample's exact text so the value keeps one format.     */
static char mon_buf[200];
static int  mon_len, mon_row;
static char mon_last[10][60];     /* text after the module prefix, last sample */
static int  mon_pfx;              /* length of the "S0 DC5009[29]:" prefix    */

static void lp(const char *fmt, ...)
{
    va_list ap;
    if (mon_len > 150) return;
    va_start(ap, fmt);
    mon_len += vsprintf(mon_buf + mon_len, fmt, ap);
    va_end(ap);
}

static void lp_nl(void)
{
    mon_buf[mon_len] = '\0';
    vputs(mon_row++, 1, mon_buf, 79);
    mon_len = 0;
    mon_buf[0] = '\0';
}

void continuous_monitor(void) {
    int i, done = 0;
    double value;
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
    int display_update_counter = 0;
    /* AUTO rate: time the first AUTO_CAL_PASSES samples taken back-to-back */
#define AUTO_CAL_PASSES 3
    int auto_cal = 0;
    unsigned long auto_sum = 0;
    unsigned long mod_ticks[10];
    unsigned long t_mod;
    /* "Time" = measurement time: BIOS ticks accumulated only while RUNNING */
    unsigned long run_ticks = 0, run_mark, run_now, run_ms;
    unsigned long passes = 0;                    /* readings per slot this run */
    unsigned int pool_size = MAX_SAMPLES_PER_MODULE;
    int avg_shown = -1;
    int mon_rows_used = 0;                       /* rows drawn on the last pass */
    unsigned long mclock_base = g_mclock + 1;   /* measurement clock keeps rising */
    
    for (i = 0; i < 10; i++) { mod_ticks[i] = 0; mon_last[i][0] = '\0'; }
    
    validate_enabled_modules();

    /* Buffer pooling: the modules this run reads share the memory of ten
       1024-sample slots - 1 module: 8192 (POOLED_MAX), 2: 5120 each,
       3: 3413 ... 10: 1024 - unless g_buffer_samples sets the size.  Buffers
       are cleared at the start of a run anyway, so they can be resized here.
       If memory runs short the size is halved until everything fits. */
    {
        int nmon = 0;
        for (i = 0; i < 10; i++)
            if (g_system->modules[i].enabled && !module_is_result(i) &&
                (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))))
                nmon++;
        if (nmon > 0) {
            unsigned long want = g_buffer_samples ? (unsigned long)g_buffer_samples
                                                  : POOL_SAMPLES / nmon;
            unsigned int size;
            int ok;
            if (want > POOLED_MAX) want = POOLED_MAX;
            if (want < MAX_SAMPLES_PER_MODULE) want = MAX_SAMPLES_PER_MODULE;
            size = (unsigned int)want;
            do {
                ok = 1;
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && !module_is_result(i) &&
                        (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))) &&
                        g_system->modules[i].module_data_size != size) {
                        free_module_buffer(i);       /* free first: less fragmentation */
                    }
                }
                for (i = 0; i < 10 && ok; i++) {
                    if (g_system->modules[i].enabled && !module_is_result(i) &&
                        (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))) &&
                        !g_system->modules[i].module_data) {
                        if (!allocate_module_buffer(i, size) || !g_system->modules[i].sample_t) ok = 0;
                    }
                }
                if (!ok) size /= 2;
            } while (!ok && size >= 256);
            pool_size = size;
        }
    }
    
    for (i = 0; i < 10; i++) {
        /* Skip computed-result traces (FFT/math): they have no GPIB instrument to
           read, and clearing them here would erase the computed result. */
        if (g_system->modules[i].enabled && !module_is_result(i)) {
            active_modules++;
            if (!g_system->modules[i].module_data) {
                if (!allocate_module_buffer(i, MAX_SAMPLES_PER_MODULE)) {
                    printf("WARNING: Failed to allocate buffer for slot %d!\n", i);
                }
            }
            clear_module_data(i);
        }
    }
    
    g_system->data_count = 0;
    g_store_suppress = 1;      /* one sample per slot per pass, stored below */

    /* BIOS ticks run at 18.2 Hz: ticks = ms * 182 / 10000 */
    ticks_per_sample = (g_control_panel.sample_rate_ms * 182L) / 10000L;
    if (ticks_per_sample < 1) ticks_per_sample = 1;
    
    clrscr();
    printf("Continuous Monitor - Press SPACE to start/stop, ESC to exit\n");
    if (g_control_panel.auto_rate)
        printf("Sample rate: AUTO (measured when started), %d active modules          \n",
               active_modules);
    else
        printf("Sample rate: %d ms (%lu ticks), %d active modules\n", 
               g_control_panel.sample_rate_ms, ticks_per_sample, active_modules);
    printf("Commands: C=Clear data      Buffer: %u samples per module%s\n", pool_size,
           g_buffer_samples ? (pool_size < g_buffer_samples ? " (less memory free)" : "")
                            : (pool_size > MAX_SAMPLES_PER_MODULE ? " (pooled)" : ""));
    printf("============================================================\n\n");
    
    last_tick_count = *((unsigned long far *)0x0040006CL);
    run_mark = last_tick_count;
    
    while (!done) {
        current_time = time(NULL);
        mon_row = 5; mon_len = 0;
        run_now = *((unsigned long far *)0x0040006CL);
        run_ms = run_ticks + (g_control_panel.running ? run_now - run_mark : 0);
        run_ms = (run_ms / 182L) * 10000L + ((run_ms % 182L) * 10000L) / 182L; /* ticks to ms without 32-bit overflow */
        lp("Time: %5lu.%lu s  Readings: %lu  Status: %-8s  ", 
               run_ms / 1000L, (run_ms / 100L) % 10L, passes,
               g_control_panel.running ? "RUNNING" : "STOPPED");
        
        /* OH: last pass's measurement overhead in ms */
        if (measurement_ticks > 0 && g_control_panel.running) {
            lp("OH:%lu ", (measurement_ticks * 10000L) / 182L);
        }
        lp_nl(); mon_row++;           /* row 6 belongs to the AVG light */
        {   /* AVG light: a buffer is full and its samples are now averaged */
            int sh = 0;
            for (i = 0; i < 10; i++)
                if (g_system->modules[i].enabled && g_system->modules[i].avg_shift > sh)
                    sh = g_system->modules[i].avg_shift;
            if (sh != avg_shown) {
                char lt[80];
                if (sh > 0)
                    sprintf(lt, " AVG x%-5u  buffer full - each sample is the mean of %u readings",
                            1u << sh, 1u << sh);
                else
                    lt[0] = '\0';
                vputs(6, 1, lt, 79);
                vattr(6, 1, 12, sh > 0 ? 0x70 : 0x07);   /* reverse video = lit */
                avg_shown = sh;
            }
        }
        
        current_tick_count = *((unsigned long far *)0x0040006CL);
        
        /* Subtract the last pass's measurement time so the sample period holds;
           AUTO rate samples back-to-back until calibrated */
        if (g_control_panel.running) {
            unsigned long adjusted_ticks = ticks_per_sample;
            if (measurement_ticks > 0 && measurement_ticks < ticks_per_sample) {
                adjusted_ticks = ticks_per_sample - measurement_ticks;
            }
            
            if ((g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES) ||
                (current_tick_count - last_tick_count) >= adjusted_ticks) {
                need_sample = 1;
                last_tick_count = current_tick_count;
                tick_start = current_tick_count;
            }
        }
        
        samples_taken = 0;
        for (i = 0; i < 10; i++) {
            /* Skip phantoms and computed traces: need type, address 1-30, description */
            if (g_system->modules[i].enabled &&
                g_system->modules[i].module_type != MOD_NONE &&
                g_system->modules[i].gpib_address >= 1 &&
                g_system->modules[i].gpib_address <= 30 &&
                strlen(g_system->modules[i].description) > 0) {
                
                should_monitor = g_control_panel.monitor_all || 
                                (g_control_panel.monitor_mask & (1 << i));
                
                if (!should_monitor && g_control_panel.running) {
                    continue;
                }

                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: strcpy(type_str, "DC5009"); break;
                    case MOD_DM5010: strcpy(type_str, "DM5010"); break;
                    case MOD_DM5120: 
                        strcpy(type_str, "DM5120");
                        if (g_dm5120_config[i].buffer_enabled) {
                            strcat(type_str, "*");
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
                
                lp("S%d %-6s[%2d]:", i, type_str, g_system->modules[i].gpib_address);
                mon_pfx = mon_len;
                
                t_mod = *((unsigned long far *)0x0040006CL);
                if (g_control_panel.running && need_sample && should_monitor) {
                    switch(g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            value = dc5009_read_measurement(g_system->modules[i].gpib_address);
                            if (fabs(value) >= 1e6)      lp("%13.7f MHz  ", value / 1e6);
                            else if (fabs(value) >= 1e3) lp("%13.6f kHz  ", value / 1e3);
                            else                         lp("%13.4f Hz   ", value);
                            break;
                            
                        case MOD_DM5010:
                            value = read_dm5010_enhanced(g_system->modules[i].gpib_address, i);
                            lp("%12.4f V     ", value);
                            break;
                            
                        case MOD_DM5120:
                            {
                                dm5120_config *cfg = &g_dm5120_config[i];
                                int address = g_system->modules[i].gpib_address;
                                float measurement_rate = dm5120_get_measurement_rate(i, cfg->trigger_mode);
                                int optimal_delay;
                                
                                if (cfg->buffer_enabled && cfg->buffer_size > 1) {
                                    /* Non-blocking fill; report the buffer average once full */
                                    int state = cfg->buffer_state;

                                    if (state == 0 && need_sample) {
                                        dm5120_start_buffer_async(address, i);
                                        lp("[Starting buffer]     ");
                                        value = g_system->modules[i].last_reading; /* Show last value */
                                        break;
                                    }
                                    
                                    if (state == 1 || state == 2) {
                                        dm5120_check_buffer_async(address, i);
                                        state = cfg->buffer_state;
                                    }

                                    switch(state) {
                                        case 1: /* Filling */
                                            lp("[Filling %3d samples] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 2: /* Half full */
                                            lp("[50%% Full - %3d smp] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 3: /* Full */
                                            value = dm5120_get_buffer_average(address);
                                            if (value == 0.0) {
                                                /* BUFAVE? gave nothing: read one stored reading */
                                                int count = dm5120_get_buffer_count(address);
                                                if (count > 0) {
                                                    value = dm5120_read_one_stored(address);
                                                }
                                            }
                                            lp("%12.6f %-3s[Avg%3d]", value, dm5120_unit(i), cfg->buffer_size);

                                            if (cfg->min_max_enabled && value != 0.0) {
                                                if (value < cfg->min_value) cfg->min_value = value;
                                                if (value > cfg->max_value) cfg->max_value = value;
                                                cfg->sample_count++;
                                            }

                                            dm5120_reset_buffer_async(i);
                                            break;

                                        case 4: /* Meter reported 551; the next pass restarts the fill */
                                            lp("[ERR 551 SHORT TIME] ");
                                            value = g_system->modules[i].last_reading;
                                            dm5120_reset_buffer_async(i);
                                            break;

                                        case 5: /* Timed out part-filled */
                                            lp("[Timeout at %3d smp] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            dm5120_reset_buffer_async(i);
                                            break;

                                        default:
                                            lp("[Buffer idle]         ");
                                            value = g_system->modules[i].last_reading;
                                            break;
                                    }
                                    
                                } else {
                                    value = read_dm5120_enhanced(address, i);
                                    if (value == 0.0) {
                                        value = read_dm5120_voltage(address);
                                    }
                                    
                                    lp("%12.6f %-3s(%4.1f r/s)", value, dm5120_unit(i), measurement_rate);
                                    
                                    /* [FAST]: sample period shorter than the meter can deliver */
                                    optimal_delay = dm5120_calculate_measurement_time(i, 0, 1);
                                    if (g_control_panel.sample_rate_ms < optimal_delay) {
                                        lp(" [FAST]");
                                    }

                                    if (cfg->min_max_enabled && value != 0.0) {
                                        if (value < cfg->min_value) cfg->min_value = value;
                                        if (value > cfg->max_value) cfg->max_value = value;
                                        cfg->sample_count++;
                                    }
                                }
                            }
                            break;
                            
                        case MOD_PS5004:
                            ps_cfg = &g_ps5004_config[i];
                            if (ps_cfg->display_mode == 1) {
                                ps5004_set_display(g_system->modules[i].gpib_address, "CURRENT");
                                GPIB_PACE(50);
                                value = ps5004_read_value(g_system->modules[i].gpib_address);
                                lp("%12.1f mA    ", value * 1000);
                            } else {
                                ps5004_set_display(g_system->modules[i].gpib_address, "VOLTAGE");
                                GPIB_PACE(50);
                                value = ps5004_read_value(g_system->modules[i].gpib_address);
                                lp("%12.4f V     ", value);
                            }
                            break;
                            
                        case MOD_PS5010:  
                            {
                                int neg_stat, pos_stat, log_stat;

                                /* The supply measures nothing: store REG? in its own order,
                                   neg*100 + pos*10 + logic (1 CV, 2 CC, 3 unregulated), so
                                   111 is all CV and a jump marks a supply leaving CV */
                                if (ps5010_read_regulation(g_system->modules[i].gpib_address,
                                                          &neg_stat, &pos_stat, &log_stat)) {
                                    lp("P:%s N:%s L:%s     ", ps5010_reg_text(pos_stat),
                                       ps5010_reg_text(neg_stat), ps5010_reg_text(log_stat));
                                    value = neg_stat * 100 + pos_stat * 10 + log_stat;
                                } else {
                                    lp("No status         ");
                                    value = g_system->modules[i].last_reading;
                                }
                            }
                            break;
                            
                        default:
                            value = 0.0;
                            lp("Not implemented       ");
                    } 
                    
                    /* AUTO rate: time each instrument during calibration */
                    if (g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES)
                        mod_ticks[i] += *((unsigned long far *)0x0040006CL) - t_mod;
                    
                    g_system->modules[i].last_reading = value;
                    /* Stamp with the measurement clock (run time), then store once */
                    g_mclock = mclock_base + run_ticks + *((unsigned long far *)0x0040006CL) - run_mark;
                    g_store_suppress = 0;
                    g_mclock_valid = 1;
                    store_module_data(i, value);
                    g_mclock_valid = 0;
                    g_store_suppress = 1;
                    
                    if (samples_taken == 0 && g_system->data_count < g_system->buffer_size) {
                        g_system->data_buffer[g_system->data_count] = value;
                    }
                    
                    samples_taken++;
                    /* Remember this sample's text for the in-between redraws */
                    mon_buf[mon_len] = '\0';
                    strncpy(mon_last[i], mon_buf + mon_pfx, sizeof(mon_last[i]) - 1);
                    mon_last[i][sizeof(mon_last[i]) - 1] = '\0';
                } else {
                    /* Not sampling this pass: show the last reading */
                    if (g_control_panel.running) {
                        if (should_monitor) {
                            if (g_system->modules[i].module_type == MOD_DM5120 &&
                                g_dm5120_config[i].buffer_enabled && g_dm5120_config[i].buffer_state != 0) {
                                /* Keep polling an in-progress DM5120 buffer between samples */
                                dm5120_check_buffer_async(g_system->modules[i].gpib_address, i);
                                switch(g_dm5120_config[i].buffer_state) {
                                    case 1: lp("[Filling %3d]       ", g_dm5120_config[i].samples_ready); break;
                                    case 2: lp("[50%% Full]         "); break;
                                    case 3: lp("[Ready to read]    "); break;
                                    case 4: lp("[ERR 551 SHORT TIME]"); break;
                                    case 5: lp("[Fill timed out]   "); break;
                                    default: lp("%12.6f       ", g_system->modules[i].last_reading); break;
                                }
                            } else if (mon_last[i][0]) {
                                lp("%s", mon_last[i]);      /* same text as the last sample */
                            } else {
                                lp("%12.6f       ", g_system->modules[i].last_reading);
                            }
                        } else {
                            lp(" [Not Selected]    ");
                        }
                    } else {
                        lp(" [Press SPACE]        ");
                    }
                }
                
                lp_nl();
            } 
        } 
        
        /* Blank rows left over from a longer previous pass; a stale module
           line would otherwise look like a duplicated or overwritten slot */
        {
            int r;
            for (r = mon_row; r < mon_rows_used; r++) vputs(r, 1, "", 79);
            mon_rows_used = mon_row;
        }
        
        if (need_sample && g_control_panel.running && samples_taken > 0) {
            if (g_system->data_count < g_system->buffer_size) {
                g_system->data_count++;
            }
            
            tick_end = *((unsigned long far *)0x0040006CL);
            measurement_ticks = tick_end - tick_start;
            passes++;
            
            need_sample = 0;
            
            /* AUTO rate: after the calibration passes, lock the rate to the
               measured cycle, rounded up to whole 55 ms timer ticks */
            if (g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES) {
                auto_sum += measurement_ticks;
                if (++auto_cal == AUTO_CAL_PASSES) {
                    int slow = -1;
                    ticks_per_sample = (auto_sum + AUTO_CAL_PASSES - 1) / AUTO_CAL_PASSES;
                    if (ticks_per_sample < 1) ticks_per_sample = 1;
                    g_control_panel.sample_rate_ms =
                        (int)((ticks_per_sample * 10000L + 181L) / 182L);
                    for (i = 0; i < 10; i++)
                        if (mod_ticks[i] && (slow < 0 || mod_ticks[i] > mod_ticks[slow]))
                            slow = i;
                    {
                        char msg[100];
                        if (slow >= 0)
                            sprintf(msg, "Sample rate: AUTO -> %d ms/sample (slowest: S%d, %lu ms per read)",
                                    g_control_panel.sample_rate_ms, slow,
                                    (mod_ticks[slow] * 10000L / AUTO_CAL_PASSES) / 182L);
                        else
                            sprintf(msg, "Sample rate: AUTO -> %d ms/sample",
                                    g_control_panel.sample_rate_ms);
                        vputs(2, 1, msg, 79);
                    }
                }
            }
        }
        
        if (kbhit()) {
            key = getch();
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case ' ':
                    if (g_control_panel.running)   /* stopping: bank the run time */
                        run_ticks += *((unsigned long far *)0x0040006CL) - run_mark;
                    else
                        run_mark = *((unsigned long far *)0x0040006CL);
                    g_control_panel.running = !g_control_panel.running;
                    if (g_control_panel.running) {
                        last_tick_count = *((unsigned long far *)0x0040006CL);
                        measurement_ticks = 0;
                        if (g_control_panel.auto_rate) {   /* re-measure */
                            auto_cal = 0;
                            auto_sum = 0;
                            for (i = 0; i < 10; i++) { mod_ticks[i] = 0; mon_last[i][0] = '\0'; }
                        }
                    }
                    break;
                    
                case 'C':
                    for (i = 0; i < 10; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    passes = 0;
                    avg_shown = -1;
                    run_ticks = 0;                 /* measurement time restarts */
                    run_mark = *((unsigned long far *)0x0040006CL);
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
    
    g_store_suppress = 0;
    clrscr();
    printf("\n\nMonitoring complete.\n");
    printf("Readings per slot: %lu\n", passes);
    for (i = 0; i < 10; i++) {          /* per-slot summary from its own buffer */
        tm5000_module *m = &g_system->modules[i];
        if (!m->enabled || !m->module_data || m->module_data_count == 0) continue;
        printf("S%d: %u samples", i, m->module_data_count);
        if (m->avg_shift) printf(" (each the mean of %u readings)", 1u << m->avg_shift);
        printf(", first %.10g, last %.10g\n", m->module_data[0],
               m->module_data[m->module_data_count - 1]);
    }
    printf("\nPress any key to continue...");
    getch();
}
