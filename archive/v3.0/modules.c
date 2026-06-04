/*
 * TM5000 GPIB Control System - Module Support
 * Version 3.0
 * Full implementation with DC5009 and DC5010 support
 */

#include "modules.h"
#include "gpib.h"
#include "graphics.h"

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
    char cmd[80];
    sprintf(cmd, "%s %s", function, channel);
    gpib_write(address, cmd);
    delay(100);
}

void dc5009_set_coupling(int address, char channel, char *coupling) {
    char cmd[80];
    sprintf(cmd, "COU CHA %c %s", channel, coupling);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_impedance(int address, char channel, char *impedance) {
    char cmd[80];
    sprintf(cmd, "TER CHA %c %s", channel, impedance);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_attenuation(int address, char channel, char *attenuation) {
    char cmd[80];
    sprintf(cmd, "ATT CHA %c %s", channel, attenuation);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_slope(int address, char channel, char *slope) {
    char cmd[80];
    sprintf(cmd, "SLO CHA %c %s", channel, slope);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_level(int address, char channel, float level) {
    char cmd[80];
    sprintf(cmd, "LEV CHA %c %.3f", channel, level);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_filter(int address, int enabled) {
    gpib_write(address, enabled ? "FIL ON" : "FIL OFF");
    delay(50);
}

void dc5009_set_gate_time(int address, float gate_time) {
    char cmd[80];
    sprintf(cmd, "GATE %.3f", gate_time);
    gpib_write(address, cmd);
    delay(50);
}

void dc5009_set_averaging(int address, int count) {
    char cmd[80];
    sprintf(cmd, "AVG %d", count);
    gpib_write(address, cmd);
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
    char buffer[256];
    float value = 0.0;
    
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%f", &value) == 1 || sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
    }
    return 0.0;
}

int dc5009_check_overflow(int address) {
    char buffer[256];
    
    gpib_write(address, "OVER?");
    delay(50);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        return (strcmp(buffer, "ON") == 0);
    }
    return 0;
}

void dc5009_clear_overflow(int address) {
    gpib_write(address, "OVER OFF");
    delay(50);
}

/* DC5010 GPIB Functions (similar to DC5009 but with additional capabilities) */
void dc5010_set_function(int address, char *function, char *channel) {
    char cmd[80];
    sprintf(cmd, "%s %s", function, channel);
    gpib_write(address, cmd);
    delay(100);
}

void dc5010_set_coupling(int address, char channel, char *coupling) {
    char cmd[80];
    sprintf(cmd, "COU CHA %c %s", channel, coupling);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_impedance(int address, char channel, char *impedance) {
    char cmd[80];
    sprintf(cmd, "TER CHA %c %s", channel, impedance);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_attenuation(int address, char channel, char *attenuation) {
    char cmd[80];
    sprintf(cmd, "ATT CHA %c %s", channel, attenuation);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_slope(int address, char channel, char *slope) {
    char cmd[80];
    sprintf(cmd, "SLO CHA %c %s", channel, slope);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_level(int address, char channel, float level) {
    char cmd[80];
    sprintf(cmd, "LEV CHA %c %.3f", channel, level);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_filter(int address, int enabled) {
    gpib_write(address, enabled ? "FIL ON" : "FIL OFF");
    delay(50);
}

void dc5010_set_gate_time(int address, float gate_time) {
    char cmd[80];
    sprintf(cmd, "GATE %.3f", gate_time);
    gpib_write(address, cmd);
    delay(50);
}

void dc5010_set_averaging(int address, int count) {
    char cmd[80];
    sprintf(cmd, "AVG %d", count);
    gpib_write(address, cmd);
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
    char buffer[256];
    float value = 0.0;
    
    gpib_write(address, "SEND");
    delay(100);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%f", &value) == 1 || sscanf(buffer, "%e", &value) == 1) {
            return value;
        }
    }
    return 0.0;
}

int dc5010_check_overflow(int address) {
    char buffer[256];
    
    gpib_write(address, "OVER?");
    delay(50);
    
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        return (strcmp(buffer, "ON") == 0);
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
    char cmd[80];
    sprintf(cmd, "BURST %s", enabled ? "ON" : "OFF");
    gpib_write(address, cmd);
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

/* DC5009 Communication Test */
void test_dc5009_comm(int address) {
    char buffer[256];
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5009 Communication Test ===\n");
    printf("Testing DC5009 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    delay(100);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("   ID: %s\n", buffer);
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
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("   Current function: %s\n", buffer);
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
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", &value) == 1 || sscanf(buffer, "%f", &value) == 1) {
            printf("   Measurement: %.6e Hz\n", value);
            success_count++;
        } else {
            printf("   Measurement parse failed: %s\n", buffer);
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
    char buffer[256];
    float value;
    int test_count = 0;
    int success_count = 0;
    
    printf("\n=== DC5010 Communication Test ===\n");
    printf("Testing DC5010 Universal Counter at GPIB address %d\n\n", address);
    
    printf("1. Testing identification...\n");
    gpib_write(address, "ID?");
    delay(100);
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("   ID: %s\n", buffer);
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
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        printf("   Current function: %s\n", buffer);
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
    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", &value) == 1 || sscanf(buffer, "%f", &value) == 1) {
            printf("   Measurement: %.6e Hz\n", value);
            success_count++;
        } else {
            printf("   Measurement parse failed: %s\n", buffer);
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
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE\n");
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
        printf("0. Exit\n\n");
        
        printf("Choice: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':
                printf("\n\nAvailable functions:\n");
                printf("FREQ, PERIOD, WIDTH, RATIO, TIME, TOTALIZE\n");
                printf("RISE, FALL (DC5010 specific)\n");
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
                
            case '0':
                done = 1;
                break;
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
                        allocate_module_buffer(slot, 1000);
                        
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
                    printf("\nInvalid address! Must be 10-30.\n");
                }
            } else {
                printf("\nInvalid module type!\n");
            }
            
            printf("Press any key to continue...");
            getch();
        }
    }
}

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
    g_dm5120_config[slot].trigger_mode = 0;  /* CONT */
    g_dm5120_config[slot].digits = 5;
    g_dm5120_config[slot].nullval = 0.0;
    g_dm5120_config[slot].null_enabled = 0;
    g_dm5120_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5120_config[slot].buffer_enabled = 0;
    g_dm5120_config[slot].buffer_size = 100;
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
    g_dm5120_config[slot].buffer_size = 1000;
    g_dm5120_config[slot].min_max_enabled = 1;
    g_dm5120_config[slot].burst_mode = 1;
    g_dm5120_config[slot].sample_rate = 10.0;  /* 10 readings/sec */
}
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
        printf("  Trigger: %s\n", cfg->trigger_mode == 0 ? "CONT" : "SINGLE");
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
                printf("DCV, ACV, OHMS, FREQ, PERIOD, CURR\n");
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
                    printf("Filter value (1-99): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 1 && temp_int <= 99) {
                        cfg->filter_value = temp_int;
                    }
                }
                break;
                
            case '4':
                cfg->trigger_mode = !cfg->trigger_mode;
                printf("\n\nTrigger mode: %s\n", cfg->trigger_mode ? "SINGLE" : "CONTINUOUS");
                getch();
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
                    if (temp_int >= 10 && temp_int <= 1000) {
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
                dm5120_set_trigger(address, cfg->trigger_mode ? "SINGLE" : "CONT");
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
    sprintf(cmd, "TRIG %s", mode);
    gpib_write_dm5120(address, cmd);
    delay(50);
}
void dm5120_set_digits(int address, int digits) {
    char cmd[50];
    if (digits >= 3 && digits <= 6) {
        sprintf(cmd, "NDIG %d", digits);
        gpib_write_dm5120(address, cmd);
        delay(50);
    }
}
void dm5120_set_null(int address, int enabled, float value) {
    char cmd[50];
    if (enabled) {
        sprintf(cmd, "NULL %.6e", value);
        gpib_write_dm5120(address, cmd);
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
void dm5120_enable_buffering(int address, int buffer_size) {
    char cmd[50];
    sprintf(cmd, "BUFFER %d", buffer_size);
    gpib_write_dm5120(address, cmd);
    delay(100);
}
void dm5120_start_buffer_sequence(int address) {
    gpib_write_dm5120(address, "BUFSTART");
    delay(100);
}
int dm5120_get_buffer_data(int address, float far *buffer, int max_samples) {
    char cmd[50];
    char response[256];
    int count = 0;
    int i;
    char *token;
    
    sprintf(cmd, "BUFDATA? %d", max_samples);
    gpib_write_dm5120(address, cmd);
    delay(200);
    
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
    float value = 0.0;
    
    if (cfg->buffer_enabled) {
        gpib_write_dm5120(address, "BUFREAD?");
        delay(100);
        
        if (gpib_read_float_dm5120(address, &value)) {
            return value;
        }
    }
    
    return read_dm5120_enhanced(address, slot);
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
            if (cfg->min_max_enabled) {
                if (value < cfg->min_value) cfg->min_value = value;
                if (value > cfg->max_value) cfg->max_value = value;
            }
            store_module_data(slot, value);
            return value;
        }
        if (sscanf(buffer, "%e", &value) == 1) {
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
        delay(300);
    } else {
        gpib_write_dm5120(address, "SEND");
        delay(400);
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
        char buffer[256];
        
        gpib_write_dm5120(address, "X");
        delay(300);
        
        if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
            if (sscanf(buffer, "NDCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(buffer, "DCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(buffer, "NACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(buffer, "ACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(buffer, "%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(buffer, "%f", &value) == 1) {
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
void test_dm5120_comm_debug(int address) {
    char buffer[256];
    int bytes_read;
    int i;
    float test_value;
    
    printf("\n=== DM5120 Debug Communication Test ===\n");
    printf("Testing DM5120 at GPIB address %d\n\n", address);
    
    printf("Raw GPIB communication test:\n");
    
    printf("\n1. Sending raw OUTPUT command:\n");
    sprintf(buffer, "output %d; INIT\r\n", address);
    printf("   Command: %s", buffer);
    ieee_write(buffer);
    delay(500);
    
    printf("\n2. Sending function setup:\n");
    sprintf(buffer, "output %d; FUNCT DCV\r\n", address);
    printf("   Command: %s", buffer);
    ieee_write(buffer);
    delay(200);
    
    printf("\n3. Requesting reading:\n");
    sprintf(buffer, "output %d; voltage?\r\n", address);
    printf("   Command: %s", buffer);
    ieee_write(buffer);
    delay(200);
    
    printf("\n4. Reading response:\n");
    sprintf(buffer, "enter %d\r\n", address);
    printf("   Command: %s", buffer);
    ieee_write(buffer);
    delay(200);
    
    bytes_read = ieee_read(buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("   Response (%d bytes): '%s'\n", bytes_read, buffer);
        printf("   Hex dump: ");
        for (i = 0; i < bytes_read && i < 40; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
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
        default:
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

/* PS5010 Functions - Complete implementations from TM5000L.c */
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
    cfg->lf_termination = 0;  /* Default to CRLF */
}
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

/* Core Measurement Functions - Complete implementations from TM5000L.c */
void single_measurement(void) {
    int i;
    float value;
    char buffer[256];
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
    
    /* Initialize active modules and allocate buffers */
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
            if (g_system->modules[i].enabled) {
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
                            value = read_dm5120_voltage(g_system->modules[i].gpib_address);
                            if (value == 0.0) {
                                value = read_dm5120_enhanced(g_system->modules[i].gpib_address, i);
                            }
                            printf("%12.6f V     ", value);
                            
                            if (g_dm5120_config[i].min_max_enabled) {
                                if (value < g_dm5120_config[i].min_value) g_dm5120_config[i].min_value = value;
                                if (value > g_dm5120_config[i].max_value) g_dm5120_config[i].max_value = value;
                                g_dm5120_config[i].sample_count++;
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
