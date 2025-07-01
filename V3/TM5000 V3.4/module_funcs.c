/*
 * TM5000 GPIB Control System - Module Functions Implementation
 * Version 3.0
 * Complete implementations extracted from TM5000L.c
 */

#include "modules.h"
#include "gpib.h"

/* Send custom GPIB command to selected module */
void send_custom_command(void) {
    int slot, i;
    char command[GPIB_BUFFER_SIZE];
    char response[GPIB_BUFFER_SIZE];
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
    
    printf("\nEnter slot number (0-9, or ESC to exit): ");
    {
        char input = getch();
        if (input == 27) {  /* ESC key */
            return;
        }
        if (input >= '0' && input <= '9') {
            slot = input - '0';
        } else {
            printf("\nInvalid input!\n");
            return;
        }
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

/* Advanced configuration for DM5120 modules */
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
        printf("B. LF Termination: %s\n", 
               cfg->lf_termination ? "ON (LF only)" : "OFF (CRLF)");
        
        printf("\nC. Query Current Status\n");
        printf("D. Apply Settings to DM5120\n");
        printf("E. Test Measurement\n");
        printf("F. Run Communication Test\n");
        printf("G. Clear Min/Max Statistics\n");
        printf("0. Return to Module Config\n");
        
        printf("\nSelect option: ");
        choice = toupper(getch());
        
        switch(choice) {
            case '1':  /* Function */
                printf("\n\nAvailable functions:\n");
                printf("0. DCV (DC Voltage)\n");
                printf("1. ACV (AC Voltage)\n");
                printf("2. DCI (DC Current)\n");
                printf("3. ACI (AC Current)\n");
                printf("4. OHMS (Resistance)\n");
                printf("Select: ");
                scanf("%d", &temp_int);
                
                switch(temp_int) {
                    case 0: strcpy(cfg->function, "DCV"); break;
                    case 1: strcpy(cfg->function, "ACV"); break;
                    case 2: strcpy(cfg->function, "DCI"); break;
                    case 3: strcpy(cfg->function, "ACI"); break;
                    case 4: strcpy(cfg->function, "OHMS"); break;
                    default: printf("Invalid selection\n");
                }
                break;
                
            case '2':  /* Range mode */
                cfg->range_mode = !cfg->range_mode;
                if (cfg->range_mode) {
                    printf("\n\nManual range mode enabled. Use DM5120 front panel to set range.\n");
                } else {
                    printf("\n\nAuto range mode enabled.\n");
                }
                printf("Press any key to continue...");
                getch();
                break;
                
            case '3':  /* Filter */
                cfg->filter_enabled = !cfg->filter_enabled;
                if (cfg->filter_enabled) {
                    printf("\n\nEnter filter value (1-100): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 1 && temp_int <= 100) {
                        cfg->filter_value = temp_int;
                    }
                }
                break;
                
            case '4':  /* Trigger mode */
                printf("\n\nTrigger modes:\n");
                printf("0. CONT (Continuous)\n");
                printf("1. ONE (Single shot)\n");
                printf("2. TALK (Talk trigger)\n");
                printf("3. EXT (External trigger)\n");
                printf("Select: ");
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 3) {
                    cfg->trigger_mode = temp_int;
                }
                break;
                
            case '5':  /* Digits */
                printf("\n\nNumber of digits (3-6): ");
                scanf("%d", &temp_int);
                if (temp_int >= 3 && temp_int <= 6) {
                    cfg->digits = temp_int;
                }
                break;
                
            case '6':  /* NULL */
                cfg->null_enabled = !cfg->null_enabled;
                if (cfg->null_enabled) {
                    printf("\n\nEnter null value: ");
                    scanf("%f", &temp_float);
                    cfg->nullval = temp_float;
                }
                break;
                
            case '7':  /* Data format */
                cfg->data_format = !cfg->data_format;
                printf("\n\nData format set to %s.\n", 
                       cfg->data_format ? "Scientific" : "NR3");
                printf("Press any key to continue...");
                getch();
                break;
                
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
                
            case 'B':  /* LF termination */
                cfg->lf_termination = !cfg->lf_termination;
                printf("\n\nLF termination %s.\n", 
                       cfg->lf_termination ? "enabled (LF only)" : "disabled (CRLF)");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'C':  /* Query status */
                printf("\n\nQuerying DM5120 status...\n");
                
                gpib_write(address, "FUNCT?");
                delay(100);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Function: %s\n", buffer);
                }
                
                gpib_write(address, "RANGE?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Range: %s\n", buffer);
                }
                
                gpib_write(address, "FILTER?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Filter: %s\n", buffer);
                }
                
                gpib_write(address, "TRIGGER?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Trigger: %s\n", buffer);
                }
                
                gpib_write(address, "ERROR?");
                delay(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Error status: %s\n", buffer);
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'D':  /* Apply settings */
                printf("\n\nApplying settings to DM5120...\n");
                
                gpib_remote(address);
                delay(200);
                
                dm5120_set_function(address, cfg->function);
                dm5120_set_range(address, cfg->range_mode);
                dm5120_set_filter(address, cfg->filter_enabled, cfg->filter_value);
                dm5120_set_trigger(address, cfg->trigger_mode == 0 ? "CONT" :
                                          cfg->trigger_mode == 1 ? "ONE" :
                                          cfg->trigger_mode == 2 ? "TALK" : "EXT");
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
                
            case 'E':  /* Test measurement */
                printf("\n\nTesting measurement...\n");
                
                temp_float = read_dm5120_enhanced(address, slot);
                printf("Reading: %.6f\n", temp_float);
                
                if (cfg->min_max_enabled) {
                    if (temp_float < cfg->min_value || cfg->sample_count == 0) {
                        cfg->min_value = temp_float;
                    }
                    if (temp_float > cfg->max_value || cfg->sample_count == 0) {
                        cfg->max_value = temp_float;
                    }
                    cfg->sample_count++;
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'F':  /* Run communication test */
                test_dm5120_comm(address);
                break;
                
            case 'G':  /* Clear statistics */
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

/* Advanced configuration for PS5004 power supply */
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
        printf("B. LF Termination: %s\n", 
               cfg->lf_termination ? "ON (LF only)" : "OFF (CRLF)");
        printf("C. Query Current Status\n");
        printf("D. Apply Settings to PS5004\n");
        printf("E. Test Measurement\n");
        printf("F. Run Communication Test\n");
        printf("G. Initialize PS5004 (INIT command)\n");
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
                
            case 'B':  /* LF termination */
                cfg->lf_termination = !cfg->lf_termination;
                printf("\n\nLF termination %s.\n", 
                       cfg->lf_termination ? "enabled (LF only)" : "disabled (CRLF)");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'C':  /* Query status */
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
                
            case 'D':  /* Apply settings */
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
                
            case 'E':  /* Test measurement */
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
                
            case 'F':  /* Run communication test */
                test_ps5004_comm(address);
                break;
                
            case 'G':  /* Initialize */
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

/* Advanced configuration for PS5010 dual power supply */
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
        printf("6. LF Termination: %s\n", 
               cfg->lf_termination ? "ON (LF only)" : "OFF (CRLF)");
        printf("7. Apply All Settings\n");
        printf("8. Read Regulation Status\n");
        printf("9. Test VTRA Command\n");
        printf("A. Run Communication Test\n");
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
                
            case '6':  /* LF termination */
                cfg->lf_termination = !cfg->lf_termination;
                printf("\n\nLF termination %s.\n", 
                       cfg->lf_termination ? "enabled (LF only)" : "disabled (CRLF)");
                printf("Press any key to continue...");
                getch();
                break;
                
            case '7':  /* Apply all settings */
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
                
            case '8':  /* Read regulation status */
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
                
            case '9':  /* Test tracking mode */
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
                
            case 'A':  /* Communication test */
                test_ps5010_comm(address);
                break;
                
            case '0':
                done = 1;
                break;
        }
    }
}

/* GPIB terminal mode for direct instrument communication */
void gpib_terminal_mode(void) {
    char command[GPIB_BUFFER_SIZE];
    char response[GPIB_BUFFER_SIZE];
    int done = 0;
    int bytes_read;
    int i;
    
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
        
        printf("Sending: '%s'\n", command);
        strcat(command, "\r\n");
        ieee_write(command);
        
        delay(100);
        
        bytes_read = ieee_read(response, sizeof(response)-1);
        if (bytes_read > 0) {
            response[bytes_read] = '\0';
            printf("Response (%d bytes): '%s'\n", bytes_read, response);
            
            printf("Hex: ");
            for (i = 0; i < bytes_read && i < 32; i++) {
                printf("%02X ", (unsigned char)response[i]);
                if (response[i] == '\r') printf("[CR] ");
                if (response[i] == '\n') printf("[LF] ");
            }
            printf("\n");
        } else {
            printf("No response received\n");
        }
        
        delay(50);
    }
}