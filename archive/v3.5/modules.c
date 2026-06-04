/*
 * TM5000 GPIB Control System - Module Support
 * Version 3.3
 * Full implementation with DC5009 and DC5010 support
 * 
 * Version History:
 * 3.0 - Initial implementation with DC5009 and DC5010 support
 * 3.1 - Version update
 */

#include "modules.h"
#include "gpib.h"
#include "graphics.h"

/* Shared GPIB buffer pool to reduce memory usage */
static char __far gpib_cmd_buffer[80];
static char __far gpib_response_buffer[80];

/* Initialize DC5009 configuration to defaults */
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

/* Initialize DC5010 configuration to defaults */
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

/* DC5009 GPIB Functions */
void dc5009_set_function(int address, char *function, char *channel) {
    
    /* Handle special command formats */
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
    delay(100);
}

void dc5009_set_coupling(int address, char channel, char *coupling) {
    
    sprintf(gpib_cmd_buffer, "COU CHA %c %s", channel, coupling);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_impedance(int address, char channel, char *impedance) {
    
    sprintf(gpib_cmd_buffer, "TER CHA %c %s", channel, impedance);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_attenuation(int address, char channel, char *attenuation) {
    
    sprintf(gpib_cmd_buffer, "ATT CHA %c %s", channel, attenuation);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_slope(int address, char channel, char *slope) {
    
    sprintf(gpib_cmd_buffer, "SLO CHA %c %s", channel, slope);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_level(int address, char channel, float level) {
    
    sprintf(gpib_cmd_buffer, "LEV CHA %c %.3f", channel, level);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_filter(int address, int enabled) {
    gpib_write(address, enabled ? "FIL ON" : "FIL OFF");
    delay(50);
}

void dc5009_set_gate_time(int address, float gate_time) {
    
    sprintf(gpib_cmd_buffer, "GATE %.3f", gate_time);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_set_averaging(int address, int count) {
    
    sprintf(gpib_cmd_buffer, "AVG %d", count);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5009_auto_trigger(int address) {
    gpib_write(address, "AUTO");
    delay(100);
}

void dc5009_start_measurement(int address) {
    gpib_write(address, "START");
    delay(50);
}

void dc5009_stop_measurement(int address) {
    gpib_write(address, "STOP");
    delay(50);
}

float dc5009_read_measurement(int address) {
    
    float value = 0.0;
    
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1 || sscanf(gpib_response_buffer, "%e", &value) == 1) {
            return value;
        }
    }
    return 0.0;
}

int dc5009_check_overflow(int address) {
    
    
    gpib_write(address, "OVER?");
    delay(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return (strcmp(gpib_response_buffer, "ON") == 0);
    }
    return 0;
}

void dc5009_clear_overflow(int address) {
    gpib_write(address, "OVER OFF");
    delay(50);
}

/* DC5009 Query Commands */
void dc5009_query_function(int address, char *buffer) {
    gpib_write(address, "FUNC?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
}

void dc5009_query_id(int address, char *buffer) {
    gpib_write(address, "ID?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
}

int dc5009_query_error(int address) {
    
    gpib_write(address, "ERR?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
    return atoi(gpib_response_buffer);
}

/* DC5009 Advanced Functions */
void dc5009_set_preset(int address, int enabled) {
    gpib_write(address, enabled ? "PRE ON" : "PRE OFF");
    delay(50);
}

void dc5009_manual_timing(int address) {
    gpib_write(address, "TMAN");
    delay(50);
}

void dc5009_set_srq(int address, int enabled) {
    gpib_write(address, enabled ? "RQS ON" : "RQS OFF");
    delay(50);
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
    
    /* Read display value */
    gpib_write(address, "SEND");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        sscanf(gpib_response_buffer, "%f", &display_value);
    }
    
    /* Check for overflow condition */
    ieee_spoll(address, &status);
    if (status & 0x02) {  /* Overflow bit in status byte */
        /* Count overflow events - simplified approach */
        /* In a real implementation, this would track overflow events over time */
        overflow_count = 1;  /* This would need to be maintained globally */
    }
    
    /* Calculate extended range result per programming guide */
    total_result = (double)display_value + (overflow_count * OVERFLOW_MULTIPLIER);
    
    return total_result;
}

/* DC5010 GPIB Functions (similar to DC5009 but with additional capabilities) */
void dc5010_set_function(int address, char *function, char *channel) {
    
    
    /* Handle special command formats including DC5010-specific modes */
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
    delay(100);
}

void dc5010_set_coupling(int address, char channel, char *coupling) {
    
    sprintf(gpib_cmd_buffer, "COU CHA %c %s", channel, coupling);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_impedance(int address, char channel, char *impedance) {
    
    sprintf(gpib_cmd_buffer, "TER CHA %c %s", channel, impedance);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_attenuation(int address, char channel, char *attenuation) {
    
    sprintf(gpib_cmd_buffer, "ATT CHA %c %s", channel, attenuation);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_slope(int address, char channel, char *slope) {
    
    sprintf(gpib_cmd_buffer, "SLO CHA %c %s", channel, slope);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_level(int address, char channel, float level) {
    
    sprintf(gpib_cmd_buffer, "LEV CHA %c %.3f", channel, level);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_filter(int address, int enabled) {
    gpib_write(address, enabled ? "FIL ON" : "FIL OFF");
    delay(50);
}

void dc5010_set_gate_time(int address, float gate_time) {
    
    sprintf(gpib_cmd_buffer, "GATE %.3f", gate_time);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_set_averaging(int address, int count) {
    
    sprintf(gpib_cmd_buffer, "AVG %d", count);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_auto_trigger(int address) {
    gpib_write(address, "AUTO");
    delay(100);
}

void dc5010_start_measurement(int address) {
    gpib_write(address, "START");
    delay(50);
}

void dc5010_stop_measurement(int address) {
    gpib_write(address, "STOP");
    delay(50);
}

float dc5010_read_measurement(int address) {
    
    float value = 0.0;
    
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1 || sscanf(gpib_response_buffer, "%e", &value) == 1) {
            return value;
        }
    }
    return 0.0;
}

int dc5010_check_overflow(int address) {
    
    
    gpib_write(address, "OVER?");
    delay(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return (strcmp(gpib_response_buffer, "ON") == 0);
    }
    return 0;
}

void dc5010_clear_overflow(int address) {
    gpib_write(address, "OVER OFF");
    delay(50);
}

/* DC5010 specific functions */
void dc5010_set_burst_mode(int address, int enabled) {
    /* DC5010 specific burst mode implementation */
    
    sprintf(gpib_cmd_buffer, "BURST %s", enabled ? "ON" : "OFF");
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void dc5010_measure_rise_time(int address) {
    gpib_write(address, "RISE A");
    delay(100);
}

void dc5010_measure_fall_time(int address) {
    gpib_write(address, "FALL A");
    delay(100);
}

/* DC5010 Query Commands */
void dc5010_query_function(int address, char *buffer) {
    gpib_write(address, "FUNC?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
}

void dc5010_query_id(int address, char *buffer) {
    gpib_write(address, "ID?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
}

int dc5010_query_error(int address) {
    
    gpib_write(address, "ERR?");
    delay(50);
    gpib_read(address, gpib_response_buffer, 80);
    return atoi(gpib_response_buffer);
}

/* DC5010 Advanced Functions */
void dc5010_set_preset(int address, int enabled) {
    gpib_write(address, enabled ? "PRE ON" : "PRE OFF");
    delay(50);
}

void dc5010_manual_timing(int address) {
    gpib_write(address, "TMAN");
    delay(50);
}

void dc5010_totalize_sum(int address) {
    gpib_write(address, "TOT A+B");
    delay(100);
}

void dc5010_totalize_diff(int address) {
    gpib_write(address, "TOT A-B");
    delay(100);
}

void dc5010_propagation_delay(int address) {
    gpib_write(address, "PROB A&B");
    delay(100);
}

void dc5010_set_srq(int address, int enabled) {
    gpib_write(address, enabled ? "RQS ON" : "RQS OFF");
    delay(50);
}

unsigned char dc5010_get_status_byte(int address) {
    unsigned char status;
    ieee_spoll(address, &status);
    return status;
}

/* DC5010 Extended Range Measurement */
double dc5010_read_extended_range(int address) {
    
    float display_value = 0.0;
    int overflow_count = 0;
    unsigned char status;
    double total_result;
    const double OVERFLOW_MULTIPLIER = 10995.1162778;
    
    /* Read display value */
    gpib_write(address, "SEND");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        sscanf(gpib_response_buffer, "%f", &display_value);
    }
    
    /* Check for overflow condition */
    ieee_spoll(address, &status);
    if (status & 0x02) {  /* Overflow bit in status byte */
        /* Count overflow events - simplified approach */
        /* In a real implementation, this would track overflow events over time */
        overflow_count = 1;  /* This would need to be maintained globally */
    }
    
    /* Calculate extended range result per programming guide */
    total_result = (double)display_value + (overflow_count * OVERFLOW_MULTIPLIER);
    
    return total_result;
}

/* DC5009 Communication Test */
void test_dc5009_comm(int address) {
    
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5009 Communication Test ===\n");
    printf("Testing DC5009 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing initialization...\n");
    gpib_write(address, "INIT");
    delay(500);
    printf("   Initialization command sent\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function setup...\n");
    gpib_write(address, "FREQ A");
    delay(100);
    gpib_write(address, "FUNC?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing auto trigger...\n");
    gpib_write(address, "AUTO");
    delay(100);
    printf("   Auto trigger enabled\n");
    success_count++;
    test_count++;
    
    printf("5. Testing measurement...\n");
    gpib_write(address, "START");
    delay(200);
    gpib_write(address, "SEND");
    delay(200);
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

/* DC5010 Communication Test */
void test_dc5010_comm(int address) {
    
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5010 Communication Test ===\n");
    printf("Testing DC5010 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   ID query failed!\n");
    }
    test_count++;
    
    printf("2. Testing initialization...\n");
    gpib_write(address, "INIT");
    delay(500);
    printf("   Initialization command sent\n");
    success_count++;
    test_count++;
    
    printf("3. Testing function setup...\n");
    gpib_write(address, "FREQ A");
    delay(100);
    gpib_write(address, "FUNC?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing high frequency capability...\n");
    gpib_write(address, "GATE 0.1");  /* Shorter gate for high freq */
    delay(50);
    printf("   High frequency mode configured\n");
    success_count++;
    test_count++;
    
    printf("5. Testing rise time measurement (DC5010 specific)...\n");
    gpib_write(address, "RISE A");
    delay(100);
    printf("   Rise time function configured\n");
    success_count++;
    test_count++;
    
    printf("6. Testing measurement...\n");
    gpib_write(address, "FREQ A");  /* Back to frequency */
    delay(100);
    gpib_write(address, "START");
    delay(200);
    gpib_write(address, "SEND");
    delay(200);
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

/* Advanced Configuration Menus */
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
                delay(200);
                
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
                        dc5009_query_function(address, func_buffer);
                        printf("   %s\n", gpib_response_buffer);
                        break;
                    }
                    case '2': {
                        char id_buffer[80];
                        dc5009_query_id(address, id_buffer);
                        printf("   %s\n", gpib_response_buffer);
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
                delay(200);
                
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
                        dc5010_query_function(address, func_buffer);
                        printf("   %s\n", gpib_response_buffer);
                        break;
                    }
                    case '2': {
                        char id_buffer[80];
                        dc5010_query_id(address, id_buffer);
                        printf("   %s\n", gpib_response_buffer);
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
/* FG5010 Function Generator Implementation                                   */
/* ========================================================================== */

/* Initialize FG5010 configuration with default values */
void init_fg5010_config(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    
    if (slot < 0 || slot >= 10) return;
    
    /* Set default configuration values */
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

/* Basic configuration menu for FG5010 */
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

/* Advanced configuration menu for FG5010 */
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
    char cmd[50];
    sprintf(gpib_cmd_buffer, "FREQ %.3f", freq);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_amplitude(int address, float amp) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "AMPL %.3f", amp);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_offset(int address, float offset) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "OFFS %.3f", offset);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_waveform(int address, char *waveform) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "FUNC %s", waveform);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_duty_cycle(int address, float duty) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "DCYC %.1f", duty);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_enable_output(int address, int enable) {
    gpib_write(address, enable ? "OUTP ON" : "OUTP OFF");
    delay(50);
}

void fg5010_set_sweep(int address, int enable, float start_freq, float stop_freq, float time) {
    char cmd[100];
    if (enable) {
        sprintf(gpib_cmd_buffer, "SWE:STAT ON;SWE:STAR %.3f;SWE:STOP %.3f;SWE:TIME %.3f", 
                start_freq, stop_freq, time);
    } else {
        strcpy(gpib_cmd_buffer, "SWE:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_trigger(int address, char *source, char *slope, float level) {
    char cmd[100];
    sprintf(gpib_cmd_buffer, "TRIG:SOUR %s;TRIG:SLOP %s;TRIG:LEV %.2f", source, slope, level);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_sync(int address, int enable) {
    gpib_write(address, enable ? "SYNC ON" : "SYNC OFF");
    delay(50);
}

void fg5010_set_invert(int address, int enable) {
    gpib_write(address, enable ? "INV ON" : "INV OFF");
    delay(50);
}

void fg5010_set_phase(int address, float phase) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "PHAS %.1f", phase);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_modulation(int address, int enable, char *type, float freq, float depth) {
    char cmd[100];
    if (enable) {
        sprintf(gpib_cmd_buffer, "MOD:STAT ON;MOD:TYP %s;MOD:FREQ %.2f;MOD:DEPT %.1f", 
                type, freq, depth);
    } else {
        strcpy(gpib_cmd_buffer, "MOD:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

void fg5010_set_burst(int address, int enable, int count, float period) {
    char cmd[100];
    if (enable) {
        sprintf(gpib_cmd_buffer, "BURS:STAT ON;BURS:NCYC %d;BURS:INT:PER %.3f", count, period);
    } else {
        strcpy(gpib_cmd_buffer, "BURS:STAT OFF");
    }
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}

/* Test FG5010 communication */
int test_fg5010_comm(int address) {
    
    int test_passed = 1;
    
    clrscr();
    printf("FG5010 Function Generator Communication Test\n");
    printf("==========================================\n\n");
    printf("Testing FG5010 at GPIB address %d...\n\n", address);
    
    /* Test 1: Identification */
    printf("Test 1: Device Identification... ");
    gpib_write(address, "*IDN?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED - No response\n");
        test_passed = 0;
    }
    
    /* Test 2: Set frequency */
    printf("\nTest 2: Set Frequency (1 kHz)... ");
    gpib_write(address, "FREQ 1000");
    delay(100);
    gpib_write(address, "FREQ?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    /* Test 3: Set amplitude */
    printf("\nTest 3: Set Amplitude (2 Vpp)... ");
    gpib_write(address, "AMPL 2.0");
    delay(100);
    gpib_write(address, "AMPL?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    /* Test 4: Waveform selection */
    printf("\nTest 4: Set Waveform (SINE)... ");
    gpib_write(address, "FUNC SINE");
    delay(100);
    gpib_write(address, "FUNC?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    /* Test 5: Output control */
    printf("\nTest 5: Output Control... ");
    gpib_write(address, "OUTP ON");
    delay(100);
    gpib_write(address, "OUTP?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("PASSED\n");
        printf("   %s\n", gpib_response_buffer);
        
        /* Turn output back off for safety */
        gpib_write(address, "OUTP OFF");
        delay(50);
    } else {
        printf("FAILED\n");
        test_passed = 0;
    }
    
    /* Test 6: Error status */
    printf("\nTest 6: Error Status Check... ");
    gpib_write(address, "SYST:ERR?");
    delay(100);
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

/* Read current frequency from FG5010 */
float fg5010_read_frequency(int address) {
    char buffer[50];
    gpib_write(address, "FREQ?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return atof(gpib_response_buffer);
    }
    return 0.0;
}

/* Read output status from FG5010 */
int fg5010_read_output_status(int address) {
    char buffer[20];
    gpib_write(address, "OUTP?");
    delay(100);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        return (strstr(gpib_response_buffer, "ON") != NULL || strstr(gpib_response_buffer, "1") != NULL) ? 1 : 0;
    }
    return 0;
}

/* Validate and cleanup phantom enabled modules */
void validate_enabled_modules(void) {
    int i;
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            /* Check for valid configuration */
            if (g_system->modules[i].module_type == MOD_NONE ||
                g_system->modules[i].gpib_address < 1 ||
                g_system->modules[i].gpib_address > 30 ||
                strlen(g_system->modules[i].description) == 0) {
                
                /* Disable phantom module and free resources */
                g_system->modules[i].enabled = 0;
                g_system->modules[i].module_type = MOD_NONE;
                g_system->modules[i].gpib_address = 0;
                strcpy(g_system->modules[i].description, "");
                free_module_buffer(i);
            }
        }
    }
}

/* Stub implementations for other module functions - to be filled in from TM5000L.c */
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
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = getch();
        
        if (choice == 27) {  /* ESC */
            done = 1;
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
                g_system->modules[slot].gpib_address = 0;
                strcpy(g_system->modules[slot].description, "");
                free_module_buffer(slot);
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
                        
                        /* Configure module */
                        g_system->modules[slot].enabled = 1;
                        g_system->modules[slot].module_type = module_type;
                        g_system->modules[slot].slot_number = slot;
                        g_system->modules[slot].gpib_address = address;
                        g_system->modules[slot].last_reading = 0.0;
                        strcpy(g_system->modules[slot].description, description);
                        
                        /* Initialize module-specific configuration */
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
                        
                        /* Initialize device and handle LF termination like v2.9 */
                        printf("Initializing device at GPIB address %d...\n", 
                               g_system->modules[slot].gpib_address);
                        
                        gpib_remote(g_system->modules[slot].gpib_address);
                        delay(200);
                        
                        /* LF termination prompt only for DM5120 - v2.9 behavior */
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
                        
                        /* Allocate data buffer */
                        allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE);
                        
                        /* Test communication */
                        printf("\nTest communication? (y/n): ");
                        if (toupper(getch()) == 'Y') {
                            printf("\nTesting communication...\n");
                            
                            gpib_remote(address);
                            delay(200);
                            
                            switch (module_type) {
                                case MOD_DC5009:
                                case MOD_DC5010:
                                    test_dc5009_comm(address);
                                    break;
                                case MOD_DM5010:
                                    test_dm5010_comm(address);
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

#if 0  /* REMOVED - Duplicate function moved to ui.c to fix stack overflow */
void module_selection_menu(void) {
    int choice, done = 0;
    int i;
    
    while (!done) {
        clrscr();
        printf("Module Selection for Monitoring\n");
        printf("================================\n\n");
        
        printf("Current Selection:\n");
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                printf("%d: %s - %s\n", i, g_system->modules[i].description,
                       (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))) ? "SELECTED" : "not selected");
            }
        }
        
        printf("\nMonitor Mode: %s\n", g_control_panel.monitor_all ? "ALL MODULES" : "SELECTED MODULES");
        
        printf("\nOptions:\n");
        printf("0-9: Toggle module selection\n");
        printf("A: Select All\n");
        printf("N: Select None\n");
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = toupper(getch());
        
        if (choice == 27) {  /* ESC */
            done = 1;
        } else if (choice >= '0' && choice <= '9') {
            i = choice - '0';
            if (g_system->modules[i].enabled) {
                if (g_control_panel.monitor_mask & (1 << i)) {
                    g_control_panel.monitor_mask &= ~(1 << i);  /* Deselect */
                } else {
                    g_control_panel.monitor_mask |= (1 << i);   /* Select */
                }
                g_control_panel.monitor_all = 0;  /* Switch to selective mode */
            }
        } else if (choice == 'A') {
            g_control_panel.monitor_all = 1;
            g_control_panel.monitor_mask = 0xFF;  /* Select all */
        } else if (choice == 'N') {
            g_control_panel.monitor_all = 0;
            g_control_panel.monitor_mask = 0;     /* Select none */
        }
    }
}
#endif

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

void sync_traces_with_modules(void) {
    int i;
    int user_enabled[10];  /* Save user trace visibility preferences */
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
                
                /* Set unit type based on module type */
                /* Check if this is FFT data - preserve dB units */
                if (strcmp(g_system->modules[i].description, "FFT Result") == 0) {
                    /* FFT data - preserve existing unit_type (should be UNIT_DB) */
                    /* Don't override unit_type, x_scale, or x_offset for FFT */
                } else {
                    /* Regular module data - set unit type based on module type */
                    switch (g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            /* Counter measurements are frequencies */
                            g_traces[i].unit_type = UNIT_FREQUENCY;
                            g_traces[i].x_scale = 1.0;  /* Sample-based for counters */
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

/* DM5120 Functions - Complete implementations from TM5000L.c */
void init_dm5120_config(int slot) {
    strcpy(g_dm5120_config[slot].function, "DCV");
    g_dm5120_config[slot].range_mode = 0;  /* AUTO */
    g_dm5120_config[slot].filter_enabled = 0;
    g_dm5120_config[slot].filter_value = 10;
    g_dm5120_config[slot].trigger_source = 0;  /* TALK - default per manual recommendations */
    g_dm5120_config[slot].trigger_mode = 0;  /* CONT */
    g_dm5120_config[slot].digits = 5;
    g_dm5120_config[slot].nullval = 0.0;
    g_dm5120_config[slot].null_enabled = 0;
    g_dm5120_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5120_config[slot].buffer_enabled = 0;
    g_dm5120_config[slot].buffer_size = DM5120_DEFAULT_BUFFER_SIZE;  /* Use DM5120 hardware limit */
    g_dm5120_config[slot].min_max_enabled = 0;
    g_dm5120_config[slot].min_value = 1e30;
    g_dm5120_config[slot].max_value = -1e30;
    g_dm5120_config[slot].sample_count = 0;
    g_dm5120_config[slot].burst_mode = 0;
    g_dm5120_config[slot].sample_rate = 1.0;
    g_dm5120_config[slot].lf_termination = 0;  /* Default to CRLF */
}
void init_dm5120_config_enhanced(int slot) {
    init_dm5120_config(slot);
    g_dm5120_config[slot].buffer_enabled = 1;
    g_dm5120_config[slot].buffer_size = DM5120_MAX_BUFFER_SIZE;  /* Use DM5120 hardware limit */
    g_dm5120_config[slot].min_max_enabled = 1;
    g_dm5120_config[slot].burst_mode = 1;
    g_dm5120_config[slot].sample_rate = 10.0;  /* 10 readings/sec */
}
#if 0  /* REMOVED - Duplicate function moved to module_funcs.c */
void configure_dm5120_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dm5120_config *cfg = &g_dm5120_config[slot];
    char input[50];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("DM5120 Advanced Configuration - Slot %d\n", slot);
        printf("======================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Function: %s\n", cfg->function);
        printf("  Range: %s\n", cfg->range_mode == 0 ? "AUTO" : "MANUAL");
        printf("  Filter: %s (value: %d)\n", cfg->filter_enabled ? "ON" : "OFF", cfg->filter_value);
        printf("  Trigger: %s,%s\n", cfg->trigger_source == 0 ? "TALK" : "EXT", cfg->trigger_mode == 0 ? "CONT" : "ONE");
        printf("  Digits: %d\n", cfg->digits);
        printf("  NULL: %s (value: %.6e)\n", cfg->null_enabled ? "ON" : "OFF", cfg->nullval);
        printf("  Data Format: %s\n", cfg->data_format ? "Scientific" : "ASCII");
        printf("  Buffer: %s (size: %d)\n", cfg->buffer_enabled ? "ON" : "OFF", cfg->buffer_size);
        printf("  Min/Max: %s\n", cfg->min_max_enabled ? "ON" : "OFF");
        if (cfg->min_max_enabled && cfg->sample_count > 0) {
            printf("    Min: %.6e, Max: %.6e\n", cfg->min_value, cfg->max_value);
        }
        printf("  Termination: %s\n", cfg->lf_termination ? "LF only (for instruments showing LF)" : "Standard CRLF");
        
        printf("\nOptions:\n");
        printf("1. Set Function (DCV, ACV, OHMS, etc.)\n");
        printf("2. Set Range Mode\n");
        printf("3. Configure Filter\n");
        printf("4. Set Trigger Mode\n");
        printf("5. Set Display Digits\n");
        printf("6. Configure NULL/REL\n");
        printf("7. Data Format\n");
        printf("8. Buffer Control\n");
        printf("9. Min/Max Statistics\n");
        printf("A. Termination Mode\n");
        printf("B. Apply Settings to DM5120\n");
        printf("C. Test Communication\n");
        printf("D. Debug Communication\n");
        printf("E. Reset to Defaults\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("DCV      - DC Voltage\n");
                printf("ACV      - AC Voltage\n");
                printf("OHMS     - Resistance\n");
                printf("DCA      - DC Current\n");
                printf("ACA      - AC Current\n");
                printf("ACVDB    - AC Voltage in dBV\n");
                printf("ACADB    - AC Current in dB\n");
                printf("OHMSCOMP - Resistance with thermal compensation\n");
                printf("Enter function: ");
                scanf("%s", input);
                
                /* Validate function input */
                if (strcmp(input, "DCV") == 0 || strcmp(input, "ACV") == 0 || 
                    strcmp(input, "OHMS") == 0 || strcmp(input, "DCA") == 0 ||
                    strcmp(input, "ACA") == 0 || strcmp(input, "ACVDB") == 0 ||
                    strcmp(input, "ACADB") == 0 || strcmp(input, "OHMSCOMP") == 0) {
                    strcpy(cfg->function, input);
                    printf("Function set to: %s\n", input);
                } else {
                    printf("Invalid function! Please use one of: DCV, ACV, OHMS, DCA, ACA, ACVDB, ACADB, OHMSCOMP\n");
                }
                printf("Press any key to continue...");
                getch();
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
                    printf("Filter value (1-99): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 1 && temp_int <= 99) {
                        cfg->filter_value = temp_int;
                    }
                }
                break;
                
            case '4':
                printf("\n\nTrigger Configuration:\n");
                printf("Current: %s,%s\n", cfg->trigger_source ? "EXT" : "TALK", cfg->trigger_mode ? "ONE" : "CONT");
                printf("\n1. Change Trigger Source (TALK/EXT)\n");
                printf("2. Change Trigger Mode (CONT/ONE)\n");
                printf("0. Cancel\n");
                printf("Choice: ");
                temp_choice = getch();
                switch(temp_choice) {
                    case '1':
                        cfg->trigger_source = !cfg->trigger_source;
                        printf("\n\nTrigger source set to: %s\n", cfg->trigger_source ? "EXT" : "TALK");
                        if (cfg->trigger_source) {
                            printf("Note: EXT mode requires external trigger or ENTER key press\n");
                        } else {
                            printf("Note: TALK mode triggers when instrument is addressed\n");
                        }
                        getch();
                        break;
                    case '2':
                        cfg->trigger_mode = !cfg->trigger_mode;
                        printf("\n\nTrigger mode set to: %s\n", cfg->trigger_mode ? "ONE" : "CONT");
                        if (cfg->trigger_mode) {
                            printf("Note: ONE mode requires trigger for each reading\n");
                        } else {
                            printf("Note: CONT mode runs continuously after trigger\n");
                        }
                        getch();
                        break;
                }
                break;
                
            case '5':
                printf("\n\nNumber of digits (3-6): ");
                scanf("%d", &temp_int);
                if (temp_int >= 3 && temp_int <= 6) {
                    cfg->digits = temp_int;
                }
                break;
                
            case '6':
                printf("\n\nNULL configuration:\n");
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
                cfg->data_format = !cfg->data_format;
                printf("\n\nData format: %s\n", cfg->data_format ? "Scientific notation" : "ASCII");
                getch();
                break;
                
            case '8':
                printf("\n\nBuffer configuration:\n");
                printf("Enable buffering? (1/0): ");
                scanf("%d", &temp_int);
                cfg->buffer_enabled = temp_int;
                if (cfg->buffer_enabled) {
                    printf("Buffer size (10-1000): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= MIN_BUFFER_SIZE && temp_int <= MAX_BUFFER_SIZE) {
                        cfg->buffer_size = temp_int;
                    }
                }
                break;
                
            case '9':
                cfg->min_max_enabled = !cfg->min_max_enabled;
                printf("\n\nMin/Max tracking: %s\n", cfg->min_max_enabled ? "ENABLED" : "DISABLED");
                if (!cfg->min_max_enabled) {
                    dm5120_clear_statistics(slot);
                }
                getch();
                break;
                
            case 'A':
                cfg->lf_termination = !cfg->lf_termination;
                printf("\n\nTermination: %s\n", cfg->lf_termination ? "LF only (for instruments showing LF)" : "Standard CRLF");
                getch();
                break;
                
            case 'B':
                printf("\n\nApplying settings to DM5120...\n");
                gpib_remote_dm5120(address);
                delay(200);
                
                dm5120_set_function(address, cfg->function);
                dm5120_set_range(address, cfg->range_mode);
                dm5120_set_filter(address, cfg->filter_enabled, cfg->filter_value);
                dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", cfg->trigger_mode ? "ONE" : "CONT");
                dm5120_set_digits(address, cfg->digits);
                dm5120_set_null(address, cfg->null_enabled, cfg->nullval);
                dm5120_set_data_format(address, cfg->data_format);
                
                if (cfg->buffer_enabled) {
                    dm5120_enable_buffering(address, cfg->buffer_size);
                }
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'C':
                test_dm5120_comm(address);
                break;
                
            case 'D':
                test_dm5120_comm_debug(address);
                break;
                
            case 'E':
                init_dm5120_config(slot);
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
#endif
void dm5120_set_function(int address, char *function) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "FUNCT %s", function);
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(100);
}
void dm5120_set_range(int address, int range) {
    char cmd[50];
    if (range == 0) {
        gpib_write_dm5120(address, "RANGE AUTO");
    } else {
        sprintf(gpib_cmd_buffer, "RANGE %d", range);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(50);
}
void dm5120_set_filter(int address, int enabled, int value) {
    char cmd[50];
    
    if (enabled) {
        gpib_write_dm5120(address, "FILTER ON");
        delay(50);
        if (value > 0 && value <= 99) {
            sprintf(gpib_cmd_buffer, "FILTER %d", value);  /* Correct DM5120 syntax */
            gpib_write_dm5120(address, gpib_cmd_buffer);
            delay(50);
        }
    } else {
        gpib_write_dm5120(address, "FILTER OFF");
    }
}
void dm5120_set_trigger(int address, char *source, char *mode) {
    char cmd[50];
    
    /* Validate parameters */
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
    
    sprintf(gpib_cmd_buffer, "TRIGGER %s,%s", source, mode);  /* DM5120 manual syntax: TRIGGER <source>,<mode> */
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(50);
}
void dm5120_set_digits(int address, int digits) {
    char cmd[50];
    if (digits >= 3 && digits <= 6) {
        sprintf(gpib_cmd_buffer, "DIGITS %d", digits);  /* Correct DM5120 syntax */
        gpib_write_dm5120(address, gpib_cmd_buffer);
        delay(50);
    }
}
void dm5120_set_null(int address, int enabled, float value) {
    char cmd[50];
    if (enabled) {
        sprintf(gpib_cmd_buffer, "NULL %.6e", value);
        gpib_write_dm5120(address, gpib_cmd_buffer);
        delay(50);
        gpib_write_dm5120(address, "NULL ON");
    } else {
        gpib_write_dm5120(address, "NULL OFF");
    }
    delay(50);
}
void dm5120_set_data_format(int address, int on) {
    gpib_write_dm5120(address, on ? "DATFOR ON" : "DATFOR OFF");
    delay(50);
}
/* DM5120 Measurement Rate Tables (readings/second) based on manual specifications */

/* DCV, DCA, ACV, ACA rates - Continuous Into Internal Buffer */
static const float __far dm5120_voltage_current_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {1000.0, 1000.0},  /* 3.5 digit */
    {333.0, 333.0},    /* 4.5 digit */
    {35.0, 8.2},       /* 5.5 digit */
    {0.0, 8.2}         /* 6.5 digit (AutoCal ON only) */
};

/* OHMS rates - Continuous Into Internal Buffer */
static const float __far dm5120_ohms_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {40.0, 24.0},      /* 3.5 digit */
    {38.0, 18.0},      /* 4.5 digit */
    {16.0, 9.5},       /* 5.5 digit */
    {0.0, 9.0}         /* 6.5 digit (AutoCal ON only) */
};

/* IEEE-488 Bus triggered rates (slower) */
static const float __far dm5120_bus_voltage_current_rates[4][2] = {
    {72.0, 45.0},      /* 3.5 digit */
    {63.0, 41.0},      /* 4.5 digit */
    {29.0, 8.2},       /* 5.5 digit */
    {0.0, 0.25}        /* 6.5 digit */
};

static const float __far dm5120_bus_ohms_rates[4][2] = {
    {32.0, 19.0},      /* 3.5 digit */
    {29.0, 16.0},      /* 4.5 digit */
    {13.0, 9.0},       /* 5.5 digit */
    {0.0, 0.3}         /* 6.5 digit */
};

/* Get measurement rate in readings/second for current DM5120 configuration */
float dm5120_get_measurement_rate(int slot, int trigger_mode) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int digit_index, autocal_index;
    float rate;
    int is_ohms_function;
    int use_bus_rates;
    
    /* Convert digits to array index */
    switch (cfg->digits) {
        case 3: digit_index = 0; break;
        case 4: digit_index = 1; break;
        case 5: digit_index = 2; break;
        case 6: digit_index = 3; break;
        default: digit_index = 1; break; /* Default to 4.5 digit */
    }
    
    /* AutoCal assumption - can be enhanced to query instrument */
    autocal_index = (cfg->digits >= 5) ? 1 : 0; /* Assume AutoCal ON for high precision */
    
    /* Check if this is an OHMS function */
    is_ohms_function = (strstr(cfg->function, "OHMS") != NULL);
    
    /* Determine if we should use bus-triggered rates
     * Per DM5120 manual:
     * - EXT,CONT with buffer enabled requires external trigger (bus rates)
     * - EXT,ONE always requires external trigger (bus rates)  
     * - TALK mode always uses bus rates (triggered by GPIB addressing)
     * - EXT,CONT with buffer disabled uses continuous rates (auto-trigger)
     */
    use_bus_rates = (trigger_mode == 2) || /* Explicit bus-triggered mode */
                    (cfg->trigger_source == 1 && (cfg->trigger_mode == 1 || cfg->buffer_enabled)) || /* EXT,ONE or EXT,CONT with buffer */
                    (cfg->trigger_source == 0); /* TALK mode always uses bus rates */
    
    /* Select appropriate rate table */
    if (use_bus_rates) { /* IEEE-488 bus triggered */
        if (is_ohms_function) {
            rate = dm5120_bus_ohms_rates[digit_index][autocal_index];
        } else {
            rate = dm5120_bus_voltage_current_rates[digit_index][autocal_index];
        }
    } else { /* Continuous into internal buffer */
        if (is_ohms_function) {
            rate = dm5120_ohms_rates[digit_index][autocal_index];
        } else {
            rate = dm5120_voltage_current_rates[digit_index][autocal_index];
        }
    }
    
    /* Apply additional factors for special cases */
    if (strstr(cfg->function, "OHMSCOMP") != NULL) {
        rate *= 0.5; /* Offset compensated ohms is 0.5x normal rate */
    }
    
    /* Minimum rate safety check */
    if (rate < 0.1) rate = 0.1;
    
    return rate;
}

/* Calculate precise timing delay based on DM5120 measurement rates */
int dm5120_calculate_measurement_time(int slot, int operation_type, int sample_count) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int base_time_ms;
    int trigger_mode = cfg->trigger_mode;
    
    /* Get actual measurement rate for current configuration */
    measurement_rate = dm5120_get_measurement_rate(slot, trigger_mode);
    
    /* Calculate base time per measurement in milliseconds */
    base_time_ms = (int)(1000.0 / measurement_rate);
    
    /* Add generous safety margin for GPIB overhead and system lag */
    base_time_ms = (base_time_ms * 150) / 100;  /* 50% safety margin */
    
    /* Operation-specific timing */
    switch (operation_type) {
        case 0: /* Single measurement */
            return base_time_ms;
            
        case 1: /* Buffer setup (BUFSZ command) */
            return base_time_ms + 200; /* Setup overhead with extra margin */
            
        case 2: /* Buffer data retrieval (READ ALLSTORE) */
            /* Time for buffer fill + generous retrieval overhead for continuous monitoring */
            return (base_time_ms * sample_count) + 500;  /* Extra overhead for GPIB/setup lag */
            
        case 3: /* STOINT setup */
            return 50; /* Quick command response */
            
        case 4: /* Buffer status query (BUFCNT?, BUFAVE?, etc.) */
            return 50;
            
        default:
            return base_time_ms;
    }
}

/* Legacy function wrapper for backward compatibility */
int dm5120_calculate_delay(int slot, int operation_type, int sample_count) {
    return dm5120_calculate_measurement_time(slot, operation_type, sample_count);
}

void dm5120_enable_buffering(int address, int slot, int buffer_size) {
    char cmd[50];
    
    /* Enforce 500 sample limit for DM5120 internal buffer */
    if (buffer_size > 500) {
        buffer_size = 500;
    }
    
    if (buffer_size == 0) {
        /* Use circular buffer mode */
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", buffer_size);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    
    /* Use digit-aware timing for buffer setup */
    delay(dm5120_calculate_delay(slot, 1, buffer_size));
}

/* Validate STOINT settings against current DM5120 configuration */
int dm5120_validate_stoint_settings(int slot, int interval_ms) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    /* Check for timing conflicts as per DM5120 manual */
    if (interval_ms >= 1 && interval_ms <= 14) {
        /* STOINT 1-14ms conflicts with: */
        
        /* 1. OHMS functions */
        if (strstr(cfg->function, "OHMS") != NULL || 
            strstr(cfg->function, "ACVDB") != NULL ||
            strstr(cfg->function, "ACADB") != NULL) {
            return -1; /* Conflict with OHMS functions */
        }
        
        /* 2. AUTO range (assuming range_mode 0 = AUTO) */
        if (cfg->range_mode == 0) {
            return -2; /* Conflict with AUTO range */
        }
        
        /* 3. CIRCULAR buffer */
        if (cfg->buffer_size == 0) { /* 0 indicates circular buffer */
            return -3; /* Conflict with CIRCULAR buffer */
        }
        
        /* 4. 5 or 6 digit resolution */
        if (cfg->digits >= 5) {
            return -4; /* Conflict with high resolution */
        }
    }
    
    /* Additional check for very fast intervals */
    if ((interval_ms == 1 || interval_ms == 2) && cfg->digits >= 4) {
        return -5; /* STOINT 1-2ms conflicts with 4+ digit resolution */
    }
    
    return 0; /* No conflicts */
}

/* Set storage interval with proper validation and timing */
void dm5120_set_storage_interval(int address, int slot, int interval_ms) {
    char cmd[50];
    int validation_result;
    
    /* Validate settings first */
    validation_result = dm5120_validate_stoint_settings(slot, interval_ms);
    if (validation_result != 0) {
        printf("WARNING: STOINT %d conflicts with current settings (code %d)\n", 
               interval_ms, validation_result);
        printf("This may cause measurement errors or timeouts.\n");
    }
    
    if (interval_ms == 0) {
        /* One shot mode - store one reading per trigger */
        gpib_write_dm5120(address, "STOINT ONE");
    } else {
        /* Continuous mode with interval timing */
        sprintf(gpib_cmd_buffer, "STOINT %d", interval_ms);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    
    /* Use appropriate delay based on operation type */
    delay(dm5120_calculate_measurement_time(slot, 3, 1));
}

int dm5120_query_storage_interval(int address) {
    
    int interval = 0;
    
    gpib_write_dm5120(address, "STOINT?");
    delay(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "ONE")) {
            return 0; /* One shot mode */
        }
        if (sscanf(gpib_response_buffer, "STOINT %d", &interval) == 1) {
            return interval;
        }
    }
    
    return -1; /* Error */
}

/* SRQ Event Handling for DM5120 Buffer Status */

/* Enable/disable specific SRQ events */
void dm5120_configure_srq_events(int address, int enable_full, int enable_half, int enable_rdy, int enable_opc) {
    char cmd[50];
    
    /* Configure FULL buffer event */
    sprintf(gpib_cmd_buffer, "FULL %s", enable_full ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(50);
    
    /* Configure HALF buffer event */
    sprintf(gpib_cmd_buffer, "HALF %s", enable_half ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(50);
    
    /* Configure RDY (ready) event */
    sprintf(gpib_cmd_buffer, "RDY %s", enable_rdy ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(50);
    
    /* Configure OPC (operation complete) event */
    sprintf(gpib_cmd_buffer, "OPC %s", enable_opc ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    delay(50);
    
    /* Enable RQS (Service Request) */
    gpib_write_dm5120(address, "RQS ON");
    delay(50);
}

/* Intelligent SRQ event configuration based on buffer characteristics */
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
    
    /* Configure events with intelligent strategy */
    dm5120_configure_srq_events(address, enable_full, enable_half, 1, 1);
}

/* Check DM5120 buffer status using SRQ events */
int dm5120_check_buffer_status(int address) {
    
    int status = 0;
    
    /* Check for any pending events */
    gpib_write_dm5120(address, "EVENT?");
    delay(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        int event_code;
        if (sscanf(gpib_response_buffer, "EVENT %d", &event_code) == 1) {
            switch (event_code) {
                case 0:  status = 0; break;        /* No events */
                case 1:  status = 0x01; break;     /* FULL buffer */
                case 2:  status = 0x02; break;     /* HALF buffer */
                case 4:  status = 0x04; break;     /* RDY ready */
                case 8:  status = 0x08; break;     /* OPC operation complete */
                case 16: status = 0x10; break;     /* OVER overrange */
                default: status = 0x80; break;     /* Unknown event */
            }
        }
    }
    
    return status;
}

/* Enhanced buffer data retrieval with timeout and SRQ monitoring */
int dm5120_get_buffer_data_enhanced(int address, int slot, float far *buffer, int max_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    char cmd[50];
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
    
    /* Calculate proper timeout based on measurement rate and buffer size */
    timeout_ms = dm5120_calculate_measurement_time(slot, 2, max_samples);
    
    /* Determine buffer characteristics */
    is_circular = (cfg->buffer_size == 0);
    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    measurement_interval_ms = (int)(1000.0 / measurement_rate);
    use_cont_mode = (measurement_interval_ms <= 1000);
    
    /* Intelligent SRQ event configuration */
    dm5120_configure_srq_events_smart(address, max_samples, is_circular, timeout_ms);
    
    /* Configure buffer size (this clears any existing buffer data) */
    if (is_circular) {
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", max_samples);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(100);
    
    if (use_cont_mode) {
        /* Fast measurements (<=1 second): Use CONT mode with STOINT auto-fill */
        dm5120_set_trigger(address, "TALK", "CONT");
        dm5120_set_storage_interval(address, slot, measurement_interval_ms);
        printf("Buffer Strategy: CONT mode, %d ms intervals, %.1f r/s\n", 
               measurement_interval_ms, measurement_rate);
    } else {
        /* Slow measurements (>1 second): Use SEND commands for manual control */
        dm5120_set_trigger(address, "TALK", "ONE");
        printf("Buffer Strategy: SEND mode, %d ms per measurement, %.1f r/s\n", 
               measurement_interval_ms, measurement_rate);
    }
    
    start_time = *((unsigned long far *)0x0040006CL);
    
    if (use_cont_mode) {
        /* CONT mode: Buffer fills automatically, monitor SRQ events */
        printf("Monitoring automatic buffer fill...\n");
        
        while (count < max_samples && elapsed_ms < timeout_ms) {
            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);
            
            /* Check SRQ events */
            status = dm5120_check_buffer_status(address);
            
            if (status & 0x01) { /* FULL buffer event */
                printf("FULL event received - buffer complete\n");
                break;
            }
            
            if (status & 0x02) { /* HALF buffer event */
                printf("Buffer 50%% full...\n");
            }
            
            delay(200); /* Check events every 200ms */
        }
        
    } else {
        /* SEND mode: Manual trigger for each measurement */
        printf("Manual triggering for %d samples...\n", max_samples);
        
        for (i = 0; i < max_samples && elapsed_ms < timeout_ms; i++) {
            gpib_write_dm5120(address, "SEND");
            delay(measurement_interval_ms);
            
            /* Update elapsed time */
            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);
            
            /* Progress feedback every 10 measurements */
            if ((i + 1) % 10 == 0 || i == max_samples - 1) {
                printf("Progress: %d/%d samples (%.1f%%)\n", 
                       i + 1, max_samples, ((float)(i + 1) / max_samples) * 100.0);
            }
        }
        
        printf("Manual triggering complete\n");
    }
    
    /* Verify buffer has data before reading */
    printf("Checking buffer status before read...\n");
    gpib_write_dm5120(address, "BUFCNT?");
    delay(50);
    
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
    
    /* Read all stored data with extended timeout and error handling */
    printf("Reading buffer data...\n");
    gpib_write_dm5120(address, "READ ALLSTORE");
    delay(dm5120_calculate_measurement_time(slot, 4, 1) * 2); /* Double the timeout */
    
    /* Check for GPIB errors before attempting read */
    printf("Checking GPIB status before read...\n");
    ieee_write("status\r\n");
    delay(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        printf("GPIB Status: %s\n", response);
        if (strstr(response, "ERROR") || strstr(response, "FAULT")) {
            printf("ERROR: GPIB error detected: %s\n", response);
            printf("Attempting to clear error...\n");
            ieee_write("abort\r\n");
            delay(200);
            ieee_write("status\r\n");
            delay(50);
            if (ieee_read(response, sizeof(response)) > 0) {
                printf("Status after abort: %s\n", response);
            }
        }
    }
    
    /* Parse response and fill buffer with improved error handling */
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
        
        /* Try reading one sample at a time as fallback */
        for (i = 0; i < max_samples && count < max_samples; i++) {
            gpib_write_dm5120(address, "READ ONESTORE");
            delay(200);
            
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
    
    /* Report completion status */
    if (count == max_samples) {
        printf("SUCCESS: All %d samples collected\n", count);
    } else if (count > 0) {
        printf("PARTIAL: %d/%d samples collected\n", count, max_samples);
    } else {
        printf("FAILED: No samples collected (timeout: %d ms)\n", elapsed_ms);
    }
    
    return count;
}

/* Timing Validation and Diagnostic Functions */

/* Validate all DM5120 timing settings for optimal performance */
int dm5120_validate_configuration(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int optimal_delay, buffer_fill_time;
    int warnings = 0;
    
    printf("DM5120 Configuration Analysis for Slot %d:\n", slot);
    printf("==========================================\n");
    
    /* Check measurement rate */
    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    printf("Measurement Rate: %.1f readings/sec\n", measurement_rate);
    
    /* Check optimal timing */
    optimal_delay = dm5120_calculate_measurement_time(slot, 0, 1);
    printf("Optimal Sample Interval: %d ms\n", optimal_delay);
    
    /* Check buffer configuration */
    if (cfg->buffer_enabled && cfg->buffer_size > 0) {
        buffer_fill_time = dm5120_calculate_measurement_time(slot, 2, cfg->buffer_size);
        printf("Buffer Fill Time (%d samples): %d ms\n", cfg->buffer_size, buffer_fill_time);
        
        /* Check for buffer timeout issues */
        if (buffer_fill_time > 30000) { /* 30 second timeout */
            printf("WARNING: Buffer fill time exceeds 30 seconds!\n");
            printf("Consider reducing buffer size or using faster settings.\n");
            warnings++;
        }
    }
    
    /* Check STOINT conflicts */
    if (cfg->buffer_enabled) {
        int stoint_result = dm5120_validate_stoint_settings(slot, 100); /* Test with 100ms */
        if (stoint_result != 0) {
            printf("WARNING: STOINT timing conflicts detected (code %d)\n", stoint_result);
            switch (stoint_result) {
                case -1: printf("  - OHMS function conflicts with fast intervals\n"); break;
                case -2: printf("  - AUTO range conflicts with fast intervals\n"); break;
                case -3: printf("  - CIRCULAR buffer conflicts with fast intervals\n"); break;
                case -4: printf("  - High resolution (5-6 digits) conflicts\n"); break;
                case -5: printf("  - Very fast intervals conflict with resolution\n"); break;
            }
            warnings++;
        }
    }
    
    /* Performance recommendations */
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

/* Display comprehensive timing diagnostics */
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
    
    /* Show measurement rates for different modes */
    printf("Measurement Rates:\n");
    rate_continuous = dm5120_get_measurement_rate(slot, 0); /* Continuous */
    rate_bus = dm5120_get_measurement_rate(slot, 2);        /* IEEE-488 bus */
    
    printf("Continuous Mode: %.1f readings/sec\n", rate_continuous);
    printf("IEEE-488 Bus Mode: %.1f readings/sec\n", rate_bus);
    printf("\n");
    
    /* Show timing for different operations */
    printf("Operation Timings:\n");
    printf("Single Reading: %d ms\n", dm5120_calculate_measurement_time(slot, 0, 1));
    printf("Buffer Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 1, 1));
    printf("STOINT Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 3, 1));
    printf("Status Query: %d ms\n", dm5120_calculate_measurement_time(slot, 4, 1));
    printf("\n");
    
    /* Show buffer fill times for different sizes */
    printf("Buffer Fill Times:\n");
    for (i = 10; i <= 500; i += 100) {
        int fill_time = dm5120_calculate_measurement_time(slot, 2, i);
        printf("%3d samples: %5d ms (%3.1f sec)\n", i, fill_time, fill_time / 1000.0);
    }
    printf("\n");
    
    /* Performance vs. accuracy tradeoffs */
    printf("Resolution vs. Speed Trade-offs:\n");
    printf("3.5 digits: Very fast, ~1000 r/s (voltage)\n");
    printf("4.5 digits: Fast, ~333 r/s (voltage)\n");
    printf("5.5 digits: Medium, ~8-35 r/s depending on AutoCal\n");
    printf("6.5 digits: Slow, ~8 r/s (AutoCal required)\n");
    printf("\n");
    
    printf("Function Speed Comparison (5.5 digits):\n");
    printf("DCV/ACV: ~8-35 r/s\n");
    printf("OHMS: ~9-16 r/s\n");
    printf("OHMSCOMP: ~4-8 r/s (50%% of normal OHMS)\n");
}

int dm5120_get_buffer_data(int address, int slot, float far *buffer, int max_samples) {
    char response[GPIB_BUFFER_SIZE];
    int count = 0;
    int i;
    char *token;
    
    gpib_write_dm5120(address, "READ ALLSTORE");
    
    /* Use digit-aware timing scaled by sample count */
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
/* Complete buffer fill and retrieve multiple samples */
int dm5120_fill_buffer_complete(int address, int slot, float far *buffer, int buffer_size) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int samples_collected = 0;
    int total_wait_time;
    
    if (!cfg->buffer_enabled || buffer_size <= 0) {
        return 0;
    }
    
    /* Step 0: CRITICAL - Set trigger for buffer auto-fill */
    if (cfg->buffer_enabled) {
        /* For buffer auto-fill, use EXT,CONT with STOINT */
        dm5120_set_trigger(address, "EXT", "CONT");
        
        /* Enable SRQ events for buffer monitoring */
        gpib_write_dm5120(address, "HALF ON");
        delay(50);
        gpib_write_dm5120(address, "FULL ON");
        delay(50);
        gpib_write_dm5120(address, "RQS ON");
        delay(50);
    } else {
        /* For single measurements use TALK,CONT */
        dm5120_set_trigger(address, "TALK", "CONT");
    }
    
    /* Step 1: Configure buffer size */
    dm5120_enable_buffering(address, slot, buffer_size);
    
    /* Step 2: Set storage interval for continuous sampling */
    dm5120_set_storage_interval(address, slot, cfg->sample_rate > 0 ? (int)(cfg->sample_rate * 1000) : 100);
    
    /* Step 3: Wait for buffer to fill completely */
    total_wait_time = dm5120_calculate_delay(slot, 2, buffer_size);
    printf("Collecting %d samples at %d digits - estimated time: %dms\n", 
           buffer_size, cfg->digits, total_wait_time);
    delay(total_wait_time);
    
    /* Step 4: Retrieve all buffered data */
    samples_collected = dm5120_get_buffer_data(address, slot, buffer, buffer_size);
    
    printf("Buffer operation complete: %d/%d samples collected\n", 
           samples_collected, buffer_size);
    
    return samples_collected;
}

/* DM5120 Asynchronous Buffer Functions for Non-Blocking Operation */
void dm5120_start_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    /* Only start if buffer is idle */
    if (cfg->buffer_state != 0) {
        return;  /* Already running */
    }
    
    /* Configure for async buffer fill */
    dm5120_set_trigger(address, "EXT", "CONT");
    delay(50);
    
    /* Set storage interval for automatic triggering */
    dm5120_set_storage_interval(address, slot, 100); /* 100ms intervals */
    delay(50);
    
    /* Enable SRQ events for buffer monitoring */
    gpib_write_dm5120(address, "HALF ON");
    delay(50);
    gpib_write_dm5120(address, "FULL ON");
    delay(50);
    gpib_write_dm5120(address, "RQS ON");
    delay(50);
    
    /* Clear buffer and prepare for new data */
    gpib_write_dm5120(address, "BUFCLR");
    delay(50);
    
    /* Configure buffer size if needed */
    if (cfg->buffer_size > 0 && cfg->buffer_size <= DM5120_MAX_BUFFER_SIZE) {
        dm5120_set_buffer_size(address, cfg->buffer_size);
        delay(50);
    }
    
    /* Mark state as filling */
    cfg->buffer_state = 1; /* filling */
    cfg->buffer_start_time = *((unsigned long far *)0x0040006CL); /* Get tick count */
    cfg->samples_ready = 0;
}

int dm5120_check_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    char response[128];
    int event_found = 0;
    
    /* Only check if buffer is filling */
    if (cfg->buffer_state == 0) {
        return 0;  /* Not active */
    }
    
    /* Check for SRQ events */
    gpib_write_dm5120(address, "EVENT?");
    delay(50);
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        /* Check for HALF event */
        if (strstr(response, "HALF")) {
            cfg->buffer_state = 2; /* half full */
            cfg->samples_ready = cfg->buffer_size / 2;
            event_found = 1;
        }
        /* Check for FULL event */
        else if (strstr(response, "FULL")) {
            cfg->buffer_state = 3; /* full */
            cfg->samples_ready = cfg->buffer_size;
            event_found = 1;
        }
    }
    
    /* If no event, check buffer count directly */
    if (!event_found && cfg->buffer_state == 1) {
        int count = dm5120_get_buffer_count(address);
        if (count > 0) {
            cfg->samples_ready = count;
            
            /* Check if we've reached milestones */
            if (count >= cfg->buffer_size) {
                cfg->buffer_state = 3; /* full */
            } else if (count >= cfg->buffer_size / 2) {
                cfg->buffer_state = 2; /* half */
            }
        }
    }
    
    /* Check for timeout (30 seconds) */
    if (cfg->buffer_state == 1) {
        unsigned long current_ticks = *((unsigned long far *)0x0040006CL);
        unsigned long elapsed_ticks = current_ticks - cfg->buffer_start_time;
        if (elapsed_ticks > (30L * 182L / 10L)) { /* 30 seconds in ticks */
            /* Timeout - read whatever is available */
            cfg->buffer_state = 3; /* Force full state to trigger read */
        }
    }
    
    return cfg->buffer_state;
}

void dm5120_reset_buffer_async(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    /* Reset buffer state */
    cfg->buffer_state = 0;      /* idle */
    cfg->buffer_start_time = 0;
    cfg->samples_ready = 0;
}

/* DM5120 Buffer Query Functions */
float dm5120_get_buffer_average(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "BUFAVE?");
    delay(100);
    
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

int dm5120_get_buffer_count(int address) {
    
    int count = 0;
    
    gpib_write_dm5120(address, "BUFCNT?");
    delay(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%d", &count) == 1) {
            return count;
        }
    }
    
    return 0;
}

float dm5120_get_buffer_min(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "BUFMIN?");
    delay(100);
    
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

float dm5120_get_buffer_max(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "BUFMAX?");
    delay(100);
    
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

void dm5120_set_buffer_size(int address, int size) {
    char cmd[50];
    
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
    delay(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "CIRCULAR")) {
            return 500;  /* Full circular buffer size */
        }
        if (sscanf(gpib_response_buffer, "BUFSZ %d", &size) == 1) {
            return size;
        }
    }
    
    return 0;
}

/* DM5120 Buffer Reading Functions using proper TM5000 syntax */
float dm5120_read_one_stored(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "READ ONESTORE");
    delay(100);
    
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
        /* Parse comma-separated values */
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
        /* Allocate or resize buffer if needed */
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
        /* If buffer is empty or exhausted, refill it */
        if (buffer_index >= buffer_valid_count) {
            buffer_valid_count = dm5120_fill_buffer_complete(address, slot, 
                                                            buffer_data, cfg->buffer_size);
            buffer_index = 0;
            
            if (buffer_valid_count <= 0) {
                /* Buffer fill failed, fall back to single measurement */
                return read_dm5120_enhanced(address, slot);
            }
        }
        
        /* Return next sample from buffer */
        if (buffer_index < buffer_valid_count) {
            return buffer_data[buffer_index++];
        }
    }
    
    return read_dm5120_enhanced(address, slot);
}

/* Convenience function to get multiple samples directly */
int dm5120_get_multiple_samples(int address, int slot, float far *buffer, int num_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    if (!cfg->buffer_enabled) {
        /* If buffering disabled, take individual measurements */
        int i;
        for (i = 0; i < num_samples; i++) {
            buffer[i] = read_dm5120_enhanced(address, slot);
            if (i < num_samples - 1) {
                /* Small delay between individual measurements */
                delay(dm5120_calculate_delay(slot, 0, 1));
            }
        }
        return num_samples;
    } else {
        /* Use buffering for efficiency */
        return dm5120_fill_buffer_complete(address, slot, buffer, num_samples);
    }
}

/* Example usage of corrected DM5120 buffering */
void dm5120_buffer_example(int address, int slot) {
    float samples[100];
    int count, i;
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    printf("\nDM5120 Buffer Example for Slot %d\n", slot);
    printf("Current settings: %d digits, buffer size %d\n", 
           cfg->digits, cfg->buffer_size);
    
    /* Method 1: Direct buffer fill */
    printf("\nMethod 1: Direct buffer fill (100 samples)\n");
    count = dm5120_fill_buffer_complete(address, slot, samples, 100);
    printf("Collected %d samples\n", count);
    
    if (count > 0) {
        printf("First 5 samples: ");
        for (i = 0; i < 5 && i < count; i++) {
            printf("%.6f ", samples[i]);
        }
        printf("\n");
    }
    
    /* Method 2: Using convenience function */
    printf("\nMethod 2: Convenience function (50 samples)\n");
    count = dm5120_get_multiple_samples(address, slot, samples, 50);
    printf("Collected %d samples\n", count);
    
    /* Method 3: Sequential buffered readings */
    printf("\nMethod 3: Sequential buffered readings (10 calls)\n");
    for (i = 0; i < 10; i++) {
        float value = read_dm5120_buffered(address, slot);
        printf("Reading %d: %.6f\n", i+1, value);
    }
    
    printf("\nBuffer example complete.\n");
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
    
    /* Use digit-aware timing for voltage measurement */
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
    
    if (cfg->trigger_mode == 0) {  /* CONT mode */
        gpib_write_dm5120(address, "X");
        /* Use digit-aware timing for X command */
        delay(dm5120_calculate_delay(slot, 0, 1));
    } else {
        gpib_write_dm5120(address, "SEND");
        /* Use digit-aware timing for SEND command with slight overhead */
        delay(dm5120_calculate_delay(slot, 0, 1) + 50);
    }
    
    if (gpib_read_float_dm5120(address, &value)) {
        if (cfg->min_max_enabled) {
            if (value < cfg->min_value) cfg->min_value = value;
            if (value > cfg->max_value) cfg->max_value = value;
        }
        store_module_data(slot, value);
        return value;
    }
    
    for (retry = 0; retry < 3; retry++) {
        
        
        gpib_write_dm5120(address, "X");
        delay(300);
        
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
    
    float value = 0.0;
    
    gpib_check_srq(address);
    delay(50);
    
    gpib_write_dm5120(address, "READ ADC");
    delay(300);
    
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
    gpib_write_dm5120(address, "READ ADC");
    delay(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n2. Simple execute (X command):\n");
    gpib_write_dm5120(address, "X");
    delay(300);
    
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
    delay(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("   No response\n");
    }
    
    printf("\nQuerying current settings:\n");
    
    gpib_write_dm5120(address, "FUNCT?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    gpib_write_dm5120(address, "RANGE?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nTesting enhanced buffer commands:\n");
    
    printf("4. BUFSZ? (Buffer Size Query):\n");
    gpib_write_dm5120(address, "BUFSZ?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("5. BUFCNT? (Buffer Count Query):\n");
    gpib_write_dm5120(address, "BUFCNT?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("6. Testing BUFSZ 50 (Set Buffer Size):\n");
    gpib_write_dm5120(address, "BUFSZ 50");
    delay(100);
    gpib_write_dm5120(address, "BUFSZ?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("7. STOINT? (Storage Interval Query):\n");
    gpib_write_dm5120(address, "STOINT?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    gpib_write_dm5120(address, "ERROR?");
    delay(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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
    delay(500);
    
    printf("\n2. Sending function setup:\n");
    sprintf(gpib_cmd_buffer, "output %2d; FUNCT DCV\r\n", address);
    printf("   %s\n", gpib_response_buffer);
    ieee_write(gpib_cmd_buffer);
    delay(200);
    
    printf("\n3. Requesting reading:\n");
    sprintf(gpib_cmd_buffer, "output %2d; READ ADC\r\n", address);
    printf("   %s\n", gpib_response_buffer);
    ieee_write(gpib_cmd_buffer);
    delay(200);
    
    printf("\n4. Reading response:\n");
    sprintf(gpib_cmd_buffer, "enter %2d\r\n", address);
    printf("   %s\n", gpib_response_buffer);
    ieee_write(gpib_cmd_buffer);
    delay(200);
    
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

/* DM5010 Functions - Complete implementations from TM5000L.c */
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
                delay(200);
                
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
    char cmd[50];
    sprintf(gpib_cmd_buffer, "CONF:%s", function);
    gpib_write_dm5010(address, cmd);
}
void dm5010_set_range(int address, char *function, float range) {
    char cmd[100];
    if (range == 0.0) {
        sprintf(gpib_cmd_buffer, "CONF:%s AUTO", function);
    } else {
        sprintf(gpib_cmd_buffer, "CONF:%s %e", function, range);
    }
    gpib_write_dm5010(address, cmd);
}
void dm5010_set_filter(int address, int enabled, int count) {
    char cmd[50];
    if (enabled) {
        sprintf(gpib_cmd_buffer, "AVER:STAT ON");
        gpib_write_dm5010(address, cmd);
        sprintf(gpib_cmd_buffer, "AVER:COUN %d", count);
        gpib_write_dm5010(address, cmd);
    } else {
        sprintf(gpib_cmd_buffer, "AVER:STAT OFF");
        gpib_write_dm5010(address, cmd);
    }
}
void dm5010_set_trigger(int address, char *mode) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "TRIG:SOUR %s", mode);
    gpib_write_dm5010(address, cmd);
}
void dm5010_set_autozero(int address, int enabled) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "ZERO:AUTO %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}
void dm5010_set_null(int address, int enabled, float value) {
    char cmd[50];
    if (enabled) {
        sprintf(gpib_cmd_buffer, "CALC:NULL:OFFS %e", value);
        gpib_write_dm5010(address, cmd);
        sprintf(gpib_cmd_buffer, "CALC:NULL:STAT ON");
        gpib_write_dm5010(address, cmd);
    } else {
        sprintf(gpib_cmd_buffer, "CALC:NULL:STAT OFF");
        gpib_write_dm5010(address, cmd);
    }
}
void dm5010_set_calculation(int address, int mode, float factor, float offset) {
    char cmd[100];
    switch(mode) {
        case 1: /* Average */
            sprintf(gpib_cmd_buffer, "CALC:AVER:STAT ON");
            break;
        case 2: /* Scale */
            sprintf(gpib_cmd_buffer, "CALC:SCAL:GAIN %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(gpib_cmd_buffer, "CALC:SCAL:OFFS %e", offset);
            gpib_write_dm5010(address, cmd);
            sprintf(gpib_cmd_buffer, "CALC:SCAL:STAT ON");
            break;
        case 3: /* dBm */
            sprintf(gpib_cmd_buffer, "CALC:DBM:REF %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(gpib_cmd_buffer, "CALC:DBM:STAT ON");
            break;
        case 4: /* dBr */
            sprintf(gpib_cmd_buffer, "CALC:DB:REF %e", factor);
            gpib_write_dm5010(address, cmd);
            sprintf(gpib_cmd_buffer, "CALC:DB:STAT ON");
            break;
        default:
            sprintf(gpib_cmd_buffer, "CALC:STAT OFF");
            break;
    }
    gpib_write_dm5010(address, cmd);
}
void dm5010_beeper(int address, int enabled) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "SYST:BEEP:STAT %s", enabled ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}
void dm5010_lock_front_panel(int address, int locked) {
    char cmd[50];
    sprintf(gpib_cmd_buffer, "SYST:LOCK %s", locked ? "ON" : "OFF");
    gpib_write_dm5010(address, cmd);
}
float read_dm5010_enhanced(int address, int slot) {
    
    float value = 0.0;
    int attempts = 0;
    int success = 0;
    
    gpib_check_srq(address);
    delay(50);
    
    while (attempts < 3 && !success) {
        attempts++;
        
        gpib_write_dm5010(address, "VAL?");
        delay(100);
        
        if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
            if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
                success = 1;
                break;
            }
        }
        
        if (!success) {
            gpib_write_dm5010(address, "READ?");
            delay(150);
            
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
    delay(100);
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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
    if (gpib_read_dm5010(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        success_count++;
    } else {
        printf("   Function query failed!\n");
    }
    test_count++;
    
    printf("4. Testing measurement...\n");
    gpib_write_dm5010(address, "READ?");
    delay(200);
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
    delay(100);
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

/* PS5004 Functions - Complete implementations from TM5000L.c */
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
#if 0  /* REMOVED - Duplicate function moved to module_funcs.c */
void configure_ps5004_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    ps5004_config *cfg = &g_ps5004_config[slot];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("PS5004 Advanced Configuration - Slot %d\n", slot);
        printf("======================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Voltage: %.4f V\n", cfg->voltage);
        printf("  Current Limit: %.3f A (%.0f mA)\n", cfg->current_limit, cfg->current_limit * 1000);
        printf("  Output: %s\n", cfg->output_enabled ? "ON" : "OFF");
        printf("  Display Mode: ");
        switch(cfg->display_mode) {
            case 0: printf("Voltage\n"); break;
            case 1: printf("Current\n"); break;
            case 2: printf("Current Limit\n"); break;
            default: printf("Unknown\n"); break;
        }
        printf("  Interrupts:\n");
        printf("    VRI (Voltage Regulation): %s\n", cfg->vri_enabled ? "ON" : "OFF");
        printf("    CRI (Current Regulation): %s\n", cfg->cri_enabled ? "ON" : "OFF");
        printf("    URI (Unregulated): %s\n", cfg->uri_enabled ? "ON" : "OFF");
        printf("    DT (Device Trigger): %s\n", cfg->dt_enabled ? "ON" : "OFF");
        printf("    User (INST ID): %s\n", cfg->user_enabled ? "ON" : "OFF");
        printf("  Service Requests: %s\n", cfg->rqs_enabled ? "ON" : "OFF");
        
        printf("\nOptions:\n");
        printf("1. Set Output Voltage (0-20V)\n");
        printf("2. Set Current Limit (10-305mA)\n");
        printf("3. Output Control (ON/OFF)\n");
        printf("4. Display Mode\n");
        printf("5. Voltage Regulation Interrupt\n");
        printf("6. Current Regulation Interrupt\n");
        printf("7. Unregulated Interrupt\n");
        printf("8. Device Trigger\n");
        printf("9. User (INST ID) Button\n");
        printf("A. Service Requests\n");
        printf("B. Apply Settings to PS5004\n");
        printf("C. Test Communication\n");
        printf("D. Reset to Defaults\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nEnter voltage (0.0 to 20.0 V): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 20.0) {
                    cfg->voltage = temp_float;
                    printf("Voltage set to %.4f V\n", cfg->voltage);
                } else {
                    printf("Invalid voltage! Must be 0.0 to 20.0 V\n");
                }
                getch();
                break;
                
            case '2':
                printf("\n\nEnter current limit (0.010 to 0.305 A): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.010 && temp_float <= 0.305) {
                    cfg->current_limit = temp_float;
                    printf("Current limit set to %.3f A (%.0f mA)\n", cfg->current_limit, cfg->current_limit * 1000);
                } else {
                    printf("Invalid current! Must be 0.010 to 0.305 A (10 to 305 mA)\n");
                }
                getch();
                break;
                
            case '3':
                cfg->output_enabled = !cfg->output_enabled;
                printf("\n\nOutput: %s\n", cfg->output_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '4':
                printf("\n\nDisplay modes:\n");
                printf("0 = Voltage\n");
                printf("1 = Current\n");
                printf("2 = Current Limit\n");
                printf("Enter mode: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->display_mode = temp_int;
                }
                break;
                
            case '5':
                cfg->vri_enabled = !cfg->vri_enabled;
                printf("\n\nVoltage Regulation Interrupt: %s\n", cfg->vri_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '6':
                cfg->cri_enabled = !cfg->cri_enabled;
                printf("\n\nCurrent Regulation Interrupt: %s\n", cfg->cri_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '7':
                cfg->uri_enabled = !cfg->uri_enabled;
                printf("\n\nUnregulated Interrupt: %s\n", cfg->uri_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '8':
                cfg->dt_enabled = !cfg->dt_enabled;
                printf("\n\nDevice Trigger: %s\n", cfg->dt_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case '9':
                cfg->user_enabled = !cfg->user_enabled;
                printf("\n\nUser (INST ID) Button: %s\n", cfg->user_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'A':
                cfg->rqs_enabled = !cfg->rqs_enabled;
                printf("\n\nService Requests: %s\n", cfg->rqs_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'B':
                printf("\n\nApplying settings to PS5004...\n");
                gpib_remote(address);
                delay(200);
                
                ps5004_init(address);
                ps5004_set_voltage(address, cfg->voltage);
                ps5004_set_current(address, cfg->current_limit);
                ps5004_set_output(address, cfg->output_enabled);
                
                switch(cfg->display_mode) {
                    case 0: ps5004_set_display(address, "VOLTAGE"); break;
                    case 1: ps5004_set_display(address, "CURRENT"); break;
                    case 2: ps5004_set_display(address, "CLIMIT"); break;
                }
                
                /* Configure interrupts */
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
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'C':
                test_ps5004_comm(address);
                break;
                
            case 'D':
                init_ps5004_config(slot);
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
#endif
void ps5004_init(int address) {
    gpib_write(address, "INIT");
    delay(100);
}
void ps5004_set_voltage(int address, float voltage) {
    char cmd[50];
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 20.0) voltage = 20.0;
    sprintf(gpib_cmd_buffer, "VOLTAGE %.4f", voltage);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
void ps5004_set_current(int address, float current) {
    char cmd[50];
    if (current < 0.01) current = 0.01;    /* 10mA minimum */
    if (current > 0.305) current = 0.305;  /* 305mA maximum */
    sprintf(gpib_cmd_buffer, "CURRENT %.3f", current);
    gpib_write(address, gpib_cmd_buffer);
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
    sprintf(gpib_cmd_buffer, "DISPLAY %s", mode);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
int ps5004_get_regulation_status(int address) {
    
    int status = 0;
    
    gpib_write(address, "REGULATION?");
    delay(50);
    
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
    delay(100);
    
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
    delay(200);
    
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    delay(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("No ID response\n");
    }
    
    printf("\nInitializing PS5004...\n");
    ps5004_init(address);
    
    printf("\nQuerying all settings...\n");
    gpib_write(address, "SET?");
    delay(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nSetting voltage to 5.0V...\n");
    ps5004_set_voltage(address, 5.0);
    
    gpib_write(address, "VOLTAGE?");
    delay(50);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nSetting current limit to 100mA...\n");
    ps5004_set_current(address, 0.1);
    
    gpib_write(address, "CURRENT?");
    delay(50);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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

/* PS5010 Functions - Complete implementations from TM5000L.c */
void init_ps5010_config(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    
    cfg->voltage1 = 0.0;        /* Positive supply (0 to +32V) */
    cfg->current_limit1 = 0.1;  /* 100mA default (50mA-750mA range, 1.6A @ ≤15V) */
    cfg->output1_enabled = 0;
    
    cfg->voltage2 = 0.0;        /* Negative supply (0 to -32V) */
    cfg->current_limit2 = 0.1;  /* 100mA default (50mA-750mA range, 1.6A @ ≤15V) */
    cfg->output2_enabled = 0;
    
    cfg->logic_voltage = 5.0;   /* Logic supply (4.5-5.5V range) */
    cfg->logic_current_limit = 1.0;  /* 1A default (100mA-3.0A range) */
    cfg->logic_enabled = 0;
    
    cfg->tracking_mode = 0;  /* Independent */
    cfg->tracking_ratio = 1.0;  /* 1:1 tracking */
    
    cfg->display_channel = 0;  /* Show channel 1 */
    cfg->display_mode = 0;     /* Show voltage */
    
    cfg->ovp_enabled = 0;
    cfg->ovp_level1 = 35.0;  /* Just above 32V max */
    cfg->ovp_level2 = 35.0;
    
    cfg->cv_mode1 = 1;
    cfg->cc_mode1 = 0;
    cfg->cv_mode2 = 1;
    cfg->cc_mode2 = 0;
    
    cfg->srq_enabled = 1;
    cfg->error_queue_size = 0;
    cfg->lf_termination = 0;  /* Default to CRLF */
}
#if 0  /* REMOVED - Duplicate function moved to module_funcs.c */
void configure_ps5010_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    ps5010_config *cfg = &g_ps5010_config[slot];
    float temp_float;
    int temp_int;
    
    while (!done) {
        clrscr();
        printf("PS5010 Advanced Configuration - Slot %d\n", slot);
        printf("======================================\n\n");
        
        printf("Current Configuration:\n");
        printf("  Channel 1 (POS): %.1f V, %.3f A, %s\n", cfg->voltage1, cfg->current_limit1, cfg->output1_enabled ? "ON" : "OFF");
        printf("  Channel 2 (NEG): %.1f V, %.3f A, %s\n", cfg->voltage2, cfg->current_limit2, cfg->output2_enabled ? "ON" : "OFF");
        printf("  Logic Supply: %.1f V, %.3f A, %s\n", cfg->logic_voltage, cfg->logic_current_limit, cfg->logic_enabled ? "ON" : "OFF");
        printf("  Tracking Mode: ");
        switch(cfg->tracking_mode) {
            case 0: printf("Independent\n"); break;
            case 1: printf("Series\n"); break;
            case 2: printf("Parallel (ratio %.2f)\n", cfg->tracking_ratio); break;
        }
        printf("  Display: Ch%d %s\n", cfg->display_channel + 1, cfg->display_mode ? "Current" : "Voltage");
        printf("  OVP: %s (Ch1: %.1fV, Ch2: %.1fV)\n", cfg->ovp_enabled ? "ON" : "OFF", cfg->ovp_level1, cfg->ovp_level2);
        printf("  Regulation Status:\n");
        printf("    Ch1: %s, Ch2: %s\n", 
               cfg->cv_mode1 ? "CV" : (cfg->cc_mode1 ? "CC" : "UR"),
               cfg->cv_mode2 ? "CV" : (cfg->cc_mode2 ? "CC" : "UR"));
        printf("  Service Requests: %s\n", cfg->srq_enabled ? "ON" : "OFF");
        
        printf("\nOptions:\n");
        printf("1. Set Channel 1 Voltage (0-40V)\n");
        printf("2. Set Channel 1 Current (0-0.5A)\n");
        printf("3. Set Channel 2 Voltage (0-40V)\n");
        printf("4. Set Channel 2 Current (0-0.5A)\n");
        printf("5. Set Logic Voltage (4.5-5.5V)\n");
        printf("6. Set Logic Current (0-2A)\n");
        printf("7. Output Control\n");
        printf("8. Tracking Mode\n");
        printf("9. Display Settings\n");
        printf("A. Over Voltage Protection\n");
        printf("B. Interrupts Control\n");
        printf("C. Service Requests\n");
        printf("D. Apply Settings to PS5010\n");
        printf("E. Test Communication\n");
        printf("F. Reset to Defaults\n");
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nEnter Channel 1 voltage (0.0 to 40.0 V): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 40.0) {
                    cfg->voltage1 = temp_float;
                }
                break;
                
            case '2':
                printf("\n\nEnter Channel 1 current limit (0.0 to 0.5 A): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 0.5) {
                    cfg->current_limit1 = temp_float;
                }
                break;
                
            case '3':
                printf("\n\nEnter Channel 2 voltage (0.0 to 40.0 V): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 40.0) {
                    cfg->voltage2 = temp_float;
                }
                break;
                
            case '4':
                printf("\n\nEnter Channel 2 current limit (0.0 to 0.5 A): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 0.5) {
                    cfg->current_limit2 = temp_float;
                }
                break;
                
            case '5':
                printf("\n\nEnter Logic voltage (4.5 to 5.5 V): ");
                scanf("%f", &temp_float);
                if (temp_float >= 4.5 && temp_float <= 5.5) {
                    cfg->logic_voltage = temp_float;
                }
                break;
                
            case '6':
                printf("\n\nEnter Logic current limit (0.0 to 2.0 A): ");
                scanf("%f", &temp_float);
                if (temp_float >= 0.0 && temp_float <= 2.0) {
                    cfg->logic_current_limit = temp_float;
                }
                break;
                
            case '7':
                printf("\n\nOutput Control:\n");
                printf("1. Channel 1: %s\n", cfg->output1_enabled ? "ON" : "OFF");
                printf("2. Channel 2: %s\n", cfg->output2_enabled ? "ON" : "OFF");
                printf("3. Logic: %s\n", cfg->logic_enabled ? "ON" : "OFF");
                printf("Toggle which (1-3)? ");
                temp_int = getch() - '0';
                switch(temp_int) {
                    case 1: cfg->output1_enabled = !cfg->output1_enabled; break;
                    case 2: cfg->output2_enabled = !cfg->output2_enabled; break;
                    case 3: cfg->logic_enabled = !cfg->logic_enabled; break;
                }
                break;
                
            case '8':
                printf("\n\nTracking modes:\n");
                printf("0 = Independent\n");
                printf("1 = Series\n");
                printf("2 = Parallel\n");
                printf("Enter mode: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 2) {
                    cfg->tracking_mode = temp_int;
                    if (temp_int == 2) {
                        printf("Tracking ratio: ");
                        scanf("%f", &cfg->tracking_ratio);
                    }
                }
                break;
                
            case '9':
                printf("\n\nDisplay channel (0=Ch1, 1=Ch2, 2=Logic): ");
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
                
            case 'A':
                cfg->ovp_enabled = !cfg->ovp_enabled;
                if (cfg->ovp_enabled) {
                    printf("\n\nOVP Level Ch1 (V): ");
                    scanf("%f", &cfg->ovp_level1);
                    printf("OVP Level Ch2 (V): ");
                    scanf("%f", &cfg->ovp_level2);
                }
                printf("OVP: %s\n", cfg->ovp_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'B':
                printf("\n\nInterrupt Control:\n");
                printf("PRI (Pos Reg Int): Toggle? (y/n) ");
                if (toupper(getch()) == 'Y') {
                    /* Toggle PRI interrupt */
                }
                printf("\nNRI (Neg Reg Int): Toggle? (y/n) ");
                if (toupper(getch()) == 'Y') {
                    /* Toggle NRI interrupt */
                }
                printf("\nLRI (Log Reg Int): Toggle? (y/n) ");
                if (toupper(getch()) == 'Y') {
                    /* Toggle LRI interrupt */
                }
                break;
                
            case 'C':
                cfg->srq_enabled = !cfg->srq_enabled;
                printf("\n\nService Requests: %s\n", cfg->srq_enabled ? "ENABLED" : "DISABLED");
                getch();
                break;
                
            case 'D':
                printf("\n\nApplying settings to PS5010...\n");
                gpib_remote(address);
                delay(200);
                
                ps5010_init(address);
                ps5010_set_voltage(address, 1, cfg->voltage1);
                ps5010_set_current(address, 1, cfg->current_limit1);
                ps5010_set_voltage(address, 2, cfg->voltage2);
                ps5010_set_current(address, 2, cfg->current_limit2);
                ps5010_set_voltage(address, 3, cfg->logic_voltage);
                ps5010_set_current(address, 3, cfg->logic_current_limit);
                
                if (cfg->output1_enabled || cfg->output2_enabled) {
                    ps5010_set_output(address, 1, 1);  /* Floating supplies */
                }
                if (cfg->logic_enabled) {
                    ps5010_set_output(address, 2, 1);  /* Logic supply */
                }
                
                ps5010_set_srq(address, cfg->srq_enabled);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'E':
                test_ps5010_comm(address);
                break;
                
            case 'F':
                init_ps5010_config(slot);
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
#endif
void ps5010_init(int address) {
    gpib_write(address, "INIT");
    delay(500);  /* PS5010 needs time for reset */
}
void ps5010_set_voltage(int address, int channel, float voltage) {
    char cmd[50];
    
    switch(channel) {
        case 1:  /* Positive floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 32.0) voltage = 32.0;
            sprintf(gpib_cmd_buffer, "VPOS %.1f", voltage);
            break;
        case 2:  /* Negative floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 32.0) voltage = 32.0;
            sprintf(gpib_cmd_buffer, "VNEG %.1f", voltage);
            break;
        case 3:  /* Logic supply */
            if (voltage < 4.5) voltage = 4.5;
            if (voltage > 5.5) voltage = 5.5;
            sprintf(gpib_cmd_buffer, "VLOG %.1f", voltage);
            break;
        default:
            return;  /* Invalid channel */
    }
    
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
void ps5010_set_current(int address, int channel, float current) {
    char cmd[50];
    
    switch(channel) {
        case 1:  /* Positive supply */
            if (current < 0.050) current = 0.050;  /* 50mA minimum */
            if (current > 0.750) current = 0.750;  /* 750mA maximum (1.6A @ ≤15V) */
            sprintf(gpib_cmd_buffer, "IPOS %.3f", current);
            break;
        case 2:  /* Negative supply */
            if (current < 0.050) current = 0.050;  /* 50mA minimum */
            if (current > 0.750) current = 0.750;  /* 750mA maximum (1.6A @ ≤15V) */
            sprintf(gpib_cmd_buffer, "INEG %.3f", current);
            break;
        case 3:  /* Logic supply */
            if (current < 0.100) current = 0.100;  /* 100mA minimum */
            if (current > 3.0) current = 3.0;      /* 3.0A maximum */
            sprintf(gpib_cmd_buffer, "ILOG %.3f", current);
            break;
        default:
            return;
    }
    
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
void ps5010_set_output(int address, int channel, int on) {
    char cmd[50];
    
    switch(channel) {
        case 0:  /* All outputs */
            sprintf(gpib_cmd_buffer, "OUT %s", on ? "ON" : "OFF");
            break;
        case 1:  /* Floating supplies only */
            sprintf(gpib_cmd_buffer, "FSOUT %s", on ? "ON" : "OFF");
            break;
        case 2:  /* Logic supply only */
            sprintf(gpib_cmd_buffer, "LSOUT %s", on ? "ON" : "OFF");
            break;
        default:
            return;
    }
    
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
void ps5010_set_tracking_voltage(int address, float voltage) {
    char cmd[50];
    
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 40.0) voltage = 40.0;
    
    sprintf(gpib_cmd_buffer, "VTRA %.1f", voltage);
    gpib_write(address, gpib_cmd_buffer);
    delay(50);
}
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat) {
    
    
    gpib_write(address, "REG?");
    delay(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        /* REG returns <neg>,<pos>,<logic> where:
           1 = voltage regulation
           2 = current regulation
           3 = unregulated */
        if (sscanf(gpib_response_buffer, "%d,%d,%d", neg_stat, pos_stat, log_stat) == 3) {
            return 1;  /* Success */
        }
    }
    
    return 0;  /* Failed */
}
int ps5010_get_settings(int address, char *buffer, int maxlen) {
    gpib_write(address, "SET?");
    delay(100);
    return gpib_read(address, gpib_response_buffer, maxlen);
}
int ps5010_get_error(int address) {
    
    int error_code = 0;
    
    gpib_write(address, "ERR?");
    delay(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        sscanf(gpib_response_buffer, "%d", &error_code);
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
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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
    if (ps5010_get_settings(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
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
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        int error_code = atoi(gpib_response_buffer);
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
    if (ps5010_get_settings(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nNote: PS5010 cannot measure output voltage/current.\n");
    printf("It can only report regulation status (CV/CC/UR).\n");
    printf("Use an external DMM to verify actual output values.\n");
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* Core Measurement Functions - Complete implementations from TM5000L.c */
void single_measurement(void) {
    int i;
    float value;
    
    int active_modules[10];
    int active_count = 0;
    
    clrscr();
    printf("Single Measurement - Multi-Module Coordination\n");
    printf("==============================================\n\n");
    
    /* Step 1: Identify enabled modules */
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
    
    /* Step 2: Initialize all modules simultaneously */
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
    
    /* Step 3: Wait for all operations to complete */
    if (active_count > 0) {
        ieee_write("*OPC?\r\n");
        delay(100); /* Allow time for all modules to complete */
        drain_input_buffer(); /* Clear any response */
    }
    
    printf("Collecting results:\n");
    printf("-------------------\n");
    
    /* Step 4: Display results from all modules */
    for (i = 0; i < active_count; i++) {
        int slot = active_modules[i];
        printf("Slot %d (%s at GPIB %d): ", slot, g_system->modules[slot].description, g_system->modules[slot].gpib_address);
        
        switch (g_system->modules[slot].module_type) {
            case MOD_DC5009:
            case MOD_DC5010:
                /* Frequency counter reading */
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
                /* Enhanced multimeter reading */
                value = read_dm5010_enhanced(g_system->modules[slot].gpib_address, slot);
                printf("%.6g V\n", value);
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_DM5120:
                /* Enhanced multimeter with fallback */
                value = read_dm5120_enhanced(g_system->modules[slot].gpib_address, slot);
                if (value == 0.0) {
                    /* Fallback method */
                    value = read_dm5120_voltage(g_system->modules[slot].gpib_address);
                }
                printf("%.6g V\n", value);
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_PS5010:
                /* Power supply regulation status */
                {
                    int neg_stat, pos_stat, log_stat;
                    if (ps5010_read_regulation(g_system->modules[slot].gpib_address, &neg_stat, &pos_stat, &log_stat)) {
                        printf("POS:%s NEG:%s LOG:%s\n",
                               pos_stat == 1 ? "CV" : (pos_stat == 2 ? "CC" : "UR"),
                               neg_stat == 1 ? "CV" : (neg_stat == 2 ? "CC" : "UR"),
                               log_stat == 1 ? "CV" : (log_stat == 2 ? "CC" : "UR"));
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
    int display_update_counter = 0;
    
    /* Validate and cleanup phantom enabled modules first */
    validate_enabled_modules();
    
    /* Initialize active modules and allocate buffers */
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
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
    
    /* Calculate timing - KEY TIMING LOGIC */
    ticks_per_sample = (g_control_panel.sample_rate_ms * 182L) / 10000L;
    if (ticks_per_sample < 1) ticks_per_sample = 1;
    
    clrscr();
    printf("Continuous Monitor - Press SPACE to start/stop, ESC to exit\n");
    printf("Sample rate: %d ms (%lu ticks), %d active modules\n", 
           g_control_panel.sample_rate_ms, ticks_per_sample, active_modules);
    printf("Commands: C=Clear data\n");
    printf("============================================================\n\n");
    
    last_tick_count = *((unsigned long far *)0x0040006CL);
    
    /* MAIN MEASUREMENT LOOP */
    while (!done) {
        current_time = time(NULL);
        gotoxy(1, 5);
        printf("Time: %ld sec  Samples: %u  Status: %-8s  ", 
               current_time - start_time, 
               g_system->data_count + (need_sample && samples_taken > 0 ? 1 : 0),
               g_control_panel.running ? "RUNNING" : "STOPPED");
        
        /* Display update optimization */
        if (++display_update_counter >= 10) {
            display_update_counter = 0;
            if (measurement_ticks > 0 && g_control_panel.running) {
                printf("OH:%lu ", (measurement_ticks * 10000L) / 182L);
            }
        }
        printf("\n\n");
        
        current_tick_count = *((unsigned long far *)0x0040006CL);
        
        /* CRITICAL TIMING LOGIC FOR SAMPLING */
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
        
        /* MODULE MEASUREMENT LOOP */
        samples_taken = 0;
        for (i = 0; i < 10; i++) {
            /* Skip ghost/phantom modules - must have valid type, address, and description */
            if (g_system->modules[i].enabled &&
                g_system->modules[i].module_type != MOD_NONE &&
                g_system->modules[i].gpib_address >= 1 &&
                g_system->modules[i].gpib_address <= 30 &&
                strlen(g_system->modules[i].description) > 0) {
                
                should_monitor = g_control_panel.monitor_all || 
                                (g_control_panel.monitor_mask & (1 << i));
                
                if (!should_monitor && g_control_panel.running) {
                    continue;  /* Skip to next module */
                }
                
                /* Module type display */
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
                
                printf("S%d %-6s[%2d]:", i, type_str, g_system->modules[i].gpib_address);
                
                /* ACTUAL MEASUREMENT COLLECTION */
                if (g_control_panel.running && need_sample && should_monitor) {
                    switch(g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            value = dc5009_read_measurement(g_system->modules[i].gpib_address);
                            printf("%12.6f MHz   ", value / 1e6);
                            break;
                            
                        case MOD_DM5010:
                            value = read_dm5010_enhanced(g_system->modules[i].gpib_address, i);
                            printf("%12.4f V     ", value);
                            break;
                            
                        case MOD_DM5120:
                            /* Enhanced DM5120 measurement with async buffer support */
                            {
                                dm5120_config *cfg = &g_dm5120_config[i];
                                int address = g_system->modules[i].gpib_address;
                                float measurement_rate = dm5120_get_measurement_rate(i, cfg->trigger_mode);
                                int optimal_delay;
                                
                                if (cfg->buffer_enabled && cfg->buffer_size > 1) {
                                    /* Async buffer operation */
                                    int state = cfg->buffer_state;
                                    
                                    /* Start buffer if idle and we need a sample */
                                    if (state == 0 && need_sample) {
                                        dm5120_start_buffer_async(address, i);
                                        printf("[Starting buffer]     ");
                                        value = g_system->modules[i].last_reading; /* Show last value */
                                        break;
                                    }
                                    
                                    /* Check for events if filling */
                                    if (state == 1 || state == 2) {
                                        dm5120_check_buffer_async(address, i);
                                        state = cfg->buffer_state;
                                    }
                                    
                                    /* Display status based on buffer state */
                                    switch(state) {
                                        case 1: /* Filling */
                                            printf("[Filling %3d samples] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 2: /* Half full */
                                            printf("[50%% Full - %3d smp] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 3: /* Full */
                                            /* Read buffer average and reset */
                                            value = dm5120_get_buffer_average(address);
                                            if (value == 0.0) {
                                                /* Fallback to reading buffer count and single value */
                                                int count = dm5120_get_buffer_count(address);
                                                if (count > 0) {
                                                    value = dm5120_read_one_stored(address);
                                                }
                                            }
                                            printf("%12.6f V [Avg%3d]", value, cfg->buffer_size);
                                            
                                            /* Update statistics */
                                            if (cfg->min_max_enabled && value != 0.0) {
                                                if (value < cfg->min_value) cfg->min_value = value;
                                                if (value > cfg->max_value) cfg->max_value = value;
                                                cfg->sample_count++;
                                            }
                                            
                                            /* Reset for next cycle */
                                            dm5120_reset_buffer_async(i);
                                            break;
                                            
                                        default:
                                            printf("[Buffer idle]         ");
                                            value = g_system->modules[i].last_reading;
                                            break;
                                    }
                                    
                                } else {
                                    /* Single measurement mode (non-buffered) */
                                    value = read_dm5120_enhanced(address, i);
                                    if (value == 0.0) {
                                        value = read_dm5120_voltage(address);
                                    }
                                    
                                    printf("%12.6f V (%4.1f r/s)", value, measurement_rate);
                                    
                                    /* Check for timing conflicts */
                                    optimal_delay = dm5120_calculate_measurement_time(i, 0, 1);
                                    if (g_control_panel.sample_rate_ms < optimal_delay) {
                                        printf(" [FAST]");
                                    }
                                    
                                    /* Update statistics */
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
                                    value = 0.0;
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
                    
                    /* STORE THE MEASUREMENT */
                    g_system->modules[i].last_reading = value;
                    store_module_data(i, value);
                    
                    if (samples_taken == 0 && g_system->data_count < g_system->buffer_size) {
                        g_system->data_buffer[g_system->data_count] = value;
                    }
                    
                    samples_taken++;
                } else {
                    /* Display previous readings when not sampling */
                    if (g_control_panel.running) {
                        if (should_monitor) {
                            /* Special handling for DM5120 in buffer mode */
                            if (g_system->modules[i].module_type == MOD_DM5120 && 
                                g_dm5120_config[i].buffer_enabled && g_dm5120_config[i].buffer_state != 0) {
                                /* Show buffer status even when not actively sampling */
                                dm5120_check_buffer_async(g_system->modules[i].gpib_address, i);
                                switch(g_dm5120_config[i].buffer_state) {
                                    case 1: printf("[Filling %3d]       ", g_dm5120_config[i].samples_ready); break;
                                    case 2: printf("[50%% Full]         "); break;
                                    case 3: printf("[Ready to read]    "); break;
                                    default: printf("%12.6f       ", g_system->modules[i].last_reading); break;
                                }
                            } else {
                                printf("%12.6f       ", g_system->modules[i].last_reading);
                            }
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
        
        /* POST-MEASUREMENT PROCESSING */
        if (need_sample && g_control_panel.running && samples_taken > 0) {
            if (g_system->data_count < g_system->buffer_size) {
                g_system->data_count++;
            }
            
            tick_end = *((unsigned long far *)0x0040006CL);
            measurement_ticks = tick_end - tick_start;
            
            need_sample = 0;
        }
        
        /* KEYBOARD INPUT HANDLING */
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
        
        /* ADAPTIVE DELAY SYSTEM */
        if (g_control_panel.running && samples_taken > 0) {
            delay(5);  /* Shorter delay when actively sampling */
        } else {
            delay(15);  /* Longer delay when idle to save CPU */
        }
    } 
    
    /* CLEANUP AND SUMMARY */
    printf("\n\nMonitoring complete.\n");
    printf("Total samples: %u\n", g_system->data_count);
    if (g_system->data_count > 0) {
        printf("First value: %.6f\n", g_system->data_buffer[0]);
        printf("Last value: %.6f\n", g_system->data_buffer[g_system->data_count-1]);
    }
    printf("\nPress any key to continue...");
    getch();
}
