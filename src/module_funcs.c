/*
 * Interactive module menus: custom-command console, raw GPIB terminal, and the
 * DM5120 / PS5004 / PS5010 advanced configuration screens.
 */

#include "modules.h"
#include "module_funcs.h"
#include "gpib.h"

static void drain_console_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) ;
}

/* One console line without its newline: 1 = line, 0 = over-long (rest discarded,
   so it can't go out as a command of its own), -1 = end of input. */
static int read_console_line(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) return -1;
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        drain_console_line();
        printf("Command too long, not sent\n");
        return 0;
    }
    buf[strcspn(buf, "\r\n")] = 0;
    return 1;
}

/* Command loop to one configured slot; reads a reply when the command looks like a query. */
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
            printf("Press any key to continue...");
            getch();
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
    
    drain_console_line();

    while (!done) {
        int got;

        printf("\nCommand> ");
        fflush(stdout);

        got = read_console_line(command, sizeof(command));
        if (got < 0) break;
        if (got == 0) continue;

        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }

        if (strlen(command) == 0) {
            continue;
        }

        printf("Sending: '%s'\n", command);

        ieee_write("fill off\r\n");
        GPIB_PACE(50);

        if (gpib_write(g_system->modules[slot].gpib_address, command) != 0) {
            printf("Command not sent\n");
        } else if (strchr(command, '?') != NULL ||
            strcasecmp(command, "X") == 0 ||
            strcasecmp(command, "SEND") == 0) {

            printf("Reading response...\n");
            GPIB_PACE(200);
            
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

/* DM5120 FUNCT arguments, manual p. 3-15 */
static const char* __far dm5120_functs[] = {
    "DCV", "ACV", "OHMS", "DCA", "ACA", "ACVDB", "ACADB", "OHMSCOMP"
};

/* Full scale of RANGE 1-7 for a function, manual Table 3-3 */
static const char *dm5120_range_label(const char *funct, int range) {
    static const char* __far volts[] = { "300 mV", "3 V", "30 V", "300 V" };
    static const char* __far ohms[] = { "300 ohm", "3 kohm", "30 kohm", "300 kohm", "3 Mohm", "30 Mohm", "300 Mohm" };
    static const char* __far amps[] = { "300 uA", "3 mA", "30 mA", "300 mA", "3 A" };

    if (range <= 0 || range > 7 || strstr(funct, "DB")) return "AUTO";
    if (strstr(funct, "OHMSCOMP")) return ohms[range > 3 ? 2 : range - 1];
    if (strstr(funct, "OHMS")) return ohms[range - 1];
    if (funct[2] == 'A') return amps[range > 5 ? 4 : range - 1];
    return volts[range > 4 ? 3 : range - 1];
}

/* Print and drain ERROR? until it reports 0; returns how many errors were shown */
static int dm5120_show_errors(int address) {
    int i, code, shown = 0;
    for (i = 0; i < 8; i++) {
        code = dm5120_query_error(address);
        if (code <= 0) break;
        printf("  ERROR %d: %s\n", code, dm5120_error_text(code));
        shown++;
    }
    return shown;
}

static int dm5120_confirm(const char *what) {
    printf("\n%s - are you sure (Y/N)? ", what);
    return toupper(getch()) == 'Y';
}

/* ID, self test, error queue, front-panel text, INIT/RESET/PONSAVE */
static void dm5120_utilities_menu(int address) {
    static char __far reply[128];
    char text[16];
    int choice, done = 0;

    while (!done) {
        clrscr();
        printf("DM5120 Utilities - GPIB %d\n", address);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. Self test (TEST?)\n");
        printf("3. Read error/event queue (ERROR?)\n");
        printf("4. Show text on the display (TEXT)\n");
        printf("5. Clear display text\n");
        printf("6. INIT - return to the power-on settings\n");
        printf("7. RESET - factory default settings\n");
        printf("8. PONSAVE - save current settings as power-on\n");
        printf("9. Firmware help (HELP?)\n");
        printf("0. Return\n\nSelect option: ");
        choice = getch();
        printf("\n\n");

        switch (choice) {
            case '1':
            case '2':
            case '9':
                gpib_write_dm5120(address, choice == '1' ? "ID?" : choice == '2' ? "TEST?" : "HELP?");
                GPIB_PACE(100);
                if (gpib_read_dm5120(address, reply, sizeof(reply)) > 0) {
                    printf("%s\n", reply);
                    if (choice == '2') printf("%s\n", strstr(reply, "TEST 0") ? "Self test passed" : "Self test FAILED");
                } else {
                    printf("No reply\n");
                }
                break;
            case '3':
                if (dm5120_show_errors(address) == 0) printf("No errors or events\n");
                break;
            case '4':
                printf("Text (up to 10 characters, no quotes): ");
                /* Leading space skips the newline an earlier scanf left behind */
                if (scanf(" %15[^\n]", text) != 1) break;
                drain_console_line();
                if (strlen(text) > 10 || strchr(text, '"')) {
                    printf("Up to 10 characters, no quotes\n");
                    break;
                }
                sprintf(reply, "TEXT \"%s\"", text);
                gpib_write_dm5120(address, reply);
                printf("Sent. Option 5 clears it.\n");
                break;
            case '5':
                gpib_write_dm5120(address, "TEXT \"\"");
                printf("Display text cleared\n");
                break;
            case '6':
                gpib_write_dm5120(address, "INIT");
                GPIB_PACE(300);
                printf("Meter at its power-on settings. D in the setup menu re-applies TM5000's.\n");
                break;
            case '7':
                if (dm5120_confirm("Factory defaults (address and terminator are kept)")) {
                    gpib_write_dm5120(address, "RESET");
                    GPIB_PACE(300);
                    printf("\nMeter reset. D in the setup menu re-applies TM5000's settings.\n");
                }
                break;
            case '8':
                if (dm5120_confirm("Write the current settings to the meter's EEPROM")) {
                    gpib_write_dm5120(address, "PONSAVE");
                    GPIB_PACE(300);
                    printf("\nSaved as the power-on settings\n");
                }
                break;
            case '0':
            case 27:
                done = 1;
                break;
        }
        if (!done) {
            printf("\nPress any key...");
            getch();
        }
    }
}

void configure_dm5120_advanced(int slot) {
    int choice, done = 0;
    int address = g_system->modules[slot].gpib_address;
    dm5120_config *cfg = &g_dm5120_config[slot];
    static char __far settings[400];
    int temp_int;
    float temp_float;

    dm5120_fix_legacy_config(slot);

    while (!done) {
        clrscr();
        printf("DM5120 Advanced Configuration - Slot %d\n", slot);
        printf("================================\n\n");

        printf("1. Function: %s\n", cfg->function);
        printf("2. Range: %s", dm5120_range_label(cfg->function, cfg->range_mode));
        if (cfg->range_mode) printf(" (RANGE %d)", cfg->range_mode);
        printf("\n");
        printf("3. Filter: %s (Value: %d)\n",
               cfg->filter_enabled ? "ON" : "OFF", cfg->filter_value);
        printf("4. Trigger: %s,%s\n", cfg->trigger_source ? "EXT" : "TALK",
               cfg->trigger_mode ? "ONE" : "CONT");
        printf("5. Digits: %d\n", cfg->digits);
        printf("6. NULL: %s (Value: %.6f)\n",
               cfg->null_enabled ? "ON" : "OFF", cfg->nullval);
        printf("7. Data Format: %s\n", cfg->data_format ? "ON (value:function:location)" : "OFF (value only)");
        printf("8. Buffering: %s (Size: %d)\n",
               cfg->buffer_enabled ? "ON" : "OFF", cfg->buffer_size);
        printf("9. Min/Max Tracking: %s", cfg->min_max_enabled ? "ON" : "OFF");
        if (cfg->min_max_enabled && cfg->sample_count > 0) {
            printf(" (Min: %.6f, Max: %.6f, Samples: %d)",
                   cfg->min_value, cfg->max_value, cfg->sample_count);
        }
        printf("\n");
        printf("A. Burst Mode: %s (Rate: %.1f/s, STOINT %d ms)\n",
               cfg->burst_mode ? "ON" : "OFF", cfg->sample_rate, dm5120_stoint_for_slot(slot));
        printf("B. LF Termination: %s\n",
               cfg->lf_termination ? "ON (LF only)" : "OFF (CRLF)");
        printf("M. AutoCal: %-3s              N. Internal Filter: %s\n",
               cfg->autocal_off ? "OFF" : "ON", cfg->intfilt_off ? "OFF" : "ON");

        printf("\nC. Query Status (SET?)        D. Apply Settings to DM5120\n");
        printf("E. Test Measurement           F. Communication Test\n");
        printf("G. Clear Min/Max Statistics   H. Buffer Query Commands\n");
        printf("I. Storage Interval (STOINT)  J. SRQ Event Configuration\n");
        printf("K. Timing Diagnostics         L. Configuration Validation\n");
        printf("P. Utilities (ID, self test, errors, reset)   0. Return\n");

        printf("\nSelect option: ");
        choice = toupper(getch());

        switch(choice) {
            case '1':  /* Function */
                printf("\n\nAvailable functions:\n");
                printf("0. DCV (DC Voltage)      4. OHMS (Resistance)\n");
                printf("1. ACV (AC Voltage)      5. ACVDB (AC Voltage, dB)\n");
                printf("2. DCA (DC Current)      6. ACADB (AC Current, dB)\n");
                printf("3. ACA (AC Current)      7. OHMSCOMP (Offset-compensated ohms)\n");
                printf("Select: ");
                temp_int = -1;
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 7) {
                    strcpy(cfg->function, dm5120_functs[temp_int]);
                } else {
                    printf("Invalid selection\n");
                }
                break;

            case '2':  /* Range */
                printf("\n\nRanges for %s:\n", cfg->function);
                printf("0. AUTO\n");
                for (temp_int = 1; temp_int <= 7; temp_int++) {
                    printf("%d. %s\n", temp_int, dm5120_range_label(cfg->function, temp_int));
                }
                printf("Select: ");
                temp_int = -1;
                scanf("%d", &temp_int);
                if (temp_int >= 0 && temp_int <= 7) {
                    cfg->range_mode = temp_int;
                }
                break;

            case '3':  /* Filter */
                cfg->filter_enabled = !cfg->filter_enabled;
                if (cfg->filter_enabled) {
                    printf("\n\nEnter filter value (1-99 readings per time constant): ");
                    scanf("%d", &temp_int);
                    if (temp_int >= 1 && temp_int <= 99) {
                        cfg->filter_value = temp_int;
                    }
                }
                break;

            case '4':  /* Trigger source and mode */
                printf("\n\nTrigger source: 0. TALK (each read)  1. EXT (rear input / ENTER key)\n");
                printf("Select: ");
                temp_int = -1;
                scanf("%d", &temp_int);
                if (temp_int == 0 || temp_int == 1) cfg->trigger_source = temp_int;
                printf("Trigger mode: 0. CONT (continuous)  1. ONE (one reading per trigger)\n");
                printf("Select: ");
                temp_int = -1;
                scanf("%d", &temp_int);
                if (temp_int == 0 || temp_int == 1) cfg->trigger_mode = temp_int;
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
                printf("\n\nDATFOR %s.\n", cfg->data_format ? "ON" : "OFF");
                printf("Press any key to continue...");
                getch();
                break;

            case '8':  /* Buffering */
                cfg->buffer_enabled = !cfg->buffer_enabled;
                if (cfg->buffer_enabled) {
                    printf("\n\nEnter buffer size (%d-%d): ", DM5120_MIN_BUFFER_SIZE, DM5120_MAX_BUFFER_SIZE);
                    scanf("%d", &temp_int);
                    if (temp_int >= DM5120_MIN_BUFFER_SIZE && temp_int <= DM5120_MAX_BUFFER_SIZE) {
                        cfg->buffer_size = temp_int;
                        printf("Buffer size set to %d samples (DM5120 hardware limit: %d)\n",
                               temp_int, DM5120_MAX_BUFFER_SIZE);
                    } else {
                        printf("Invalid size. DM5120 hardware supports %d-%d samples only.\n",
                               DM5120_MIN_BUFFER_SIZE, DM5120_MAX_BUFFER_SIZE);
                    }
                    printf("Press any key to continue...");
                    getch();
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
                    temp_int = dm5120_min_stoint_ms(slot);
                    if (1000.0 / cfg->sample_rate < temp_int) {
                        printf("The meter needs %d ms per reading at these settings,\n", temp_int);
                        printf("so the buffer fills at %.1f/s.\n", 1000.0 / temp_int);
                        printf("Press any key to continue...");
                        getch();
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

            case 'M':  /* AutoCal: off gives up to 4x the reading rate, less accuracy */
                cfg->autocal_off = !cfg->autocal_off;
                break;

            case 'N':  /* Internal filter: needed for full accuracy at 5.5/6.5 digits */
                cfg->intfilt_off = !cfg->intfilt_off;
                break;

            case 'C':  /* Query status */
                printf("\n\nQuerying DM5120 settings (SET?)...\n\n");
                gpib_write_dm5120(address, "SET?");
                GPIB_PACE(100);
                if (gpib_read_dm5120(address, settings, sizeof(settings)) > 0) {
                    /* One query response per ';', three to a line */
                    char *item = strtok(settings, ";");
                    int col = 0;
                    while (item) {
                        while (*item == ' ' || *item == '\r' || *item == '\n') item++;
                        if (*item) {
                            printf("%-26.26s", item);
                            if (++col % 3 == 0) printf("\n");
                        }
                        item = strtok(NULL, ";");
                    }
                    if (col % 3) printf("\n");
                } else {
                    printf("No reply\n");
                }
                printf("\n");
                if (dm5120_show_errors(address) == 0) printf("No errors pending\n");

                printf("\nPress any key to continue...");
                getch();
                break;

            case 'D':  /* Apply settings */
                printf("\n\nApplying settings to DM5120...\n");

                gpib_remote(address);
                GPIB_PACE(200);

                /* With RQS ON each write's serial poll drops the error queued before it;
                   with RQS OFF errors stack until ERROR? (manual p. 3-12) */
                gpib_write_dm5120(address, "RQS OFF");
                GPIB_PACE(50);

                dm5120_set_function(address, cfg->function);
                dm5120_set_range(address, cfg->range_mode);
                dm5120_set_autocal(address, !cfg->autocal_off);
                dm5120_set_intfilt(address, !cfg->intfilt_off);
                dm5120_set_filter(address, cfg->filter_enabled, cfg->filter_value);
                dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", cfg->trigger_mode ? "ONE" : "CONT");
                dm5120_set_digits(address, cfg->digits);
                dm5120_set_null(address, cfg->null_enabled, cfg->nullval);
                dm5120_set_data_format(address, cfg->data_format);

                if (cfg->buffer_enabled) {
                    dm5120_enable_buffering(address, slot, cfg->buffer_size);
                }

                if (dm5120_show_errors(address) == 0) {
                    printf("Settings applied\n");
                } else {
                    printf("The meter reported the errors above\n");
                }
                gpib_write_dm5120(address, "RQS ON");
                GPIB_PACE(50);
                printf("Press any key to continue...");
                getch();
                break;

            case 'E':  /* Test measurement */
                printf("\n\nTesting measurement...\n");

                temp_float = read_dm5120_enhanced(address, slot);
                printf("Reading: %.6f %s\n", temp_float, dm5120_unit(slot));

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

            case 'H':  /* Buffer Query Commands */
                dm5120_buffer_query_menu(address);
                break;

            case 'I':  /* Storage Interval (STOINT) */
                dm5120_stoint_menu(address, slot);
                break;

            case 'J':  /* SRQ Event Configuration */
                dm5120_srq_menu(address, slot);
                break;

            case 'K':  /* Timing Diagnostics */
                printf("\n\n");
                dm5120_timing_diagnostics(slot);
                printf("Press any key to continue...");
                getch();
                break;

            case 'L':  /* Configuration Validation */
                printf("\n\n");
                {
                    int warnings = dm5120_validate_configuration(slot);
                    printf("\nValidation complete. Found %d warnings.\n", warnings);
                }
                printf("Press any key to continue...");
                getch();
                break;

            case 'P':
                dm5120_utilities_menu(address);
                break;

            case '0':
                done = 1;
                break;
        }
    }
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
                    cfg->current_limit = temp_float / 1000.0;  /* entered in mA, stored in A */
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
                GPIB_PACE(100);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("All settings:\n%s\n", buffer);
                }
                
                gpib_write(address, "REGULATION?");
                GPIB_PACE(50);
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
                GPIB_PACE(50);
                if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
                    printf("Error status: %s\n", buffer);
                }
                
                printf("\nPress any key to continue...");
                getch();
                break;
                
            case 'D':  /* Apply settings */
                printf("\n\nApplying settings to PS5004...\n");
                
                gpib_remote(address);
                GPIB_PACE(200);
                
                ps5004_set_voltage(address, cfg->voltage);
                ps5004_set_current(address, cfg->current_limit);
                
                switch(cfg->display_mode) {
                    case 0: ps5004_set_display(address, "VOLTAGE"); break;
                    case 1: ps5004_set_display(address, "CURRENT"); break;
                    case 2: ps5004_set_display(address, "CLIMIT"); break;
                }
                
                gpib_write(address, cfg->vri_enabled ? "VRI ON" : "VRI OFF");
                GPIB_PACE(50);
                gpib_write(address, cfg->cri_enabled ? "CRI ON" : "CRI OFF");
                GPIB_PACE(50);
                gpib_write(address, cfg->uri_enabled ? "URI ON" : "URI OFF");
                GPIB_PACE(50);
                
                gpib_write(address, cfg->dt_enabled ? "DT ON" : "DT OFF");
                GPIB_PACE(50);
                gpib_write(address, cfg->user_enabled ? "USER ON" : "USER OFF");
                GPIB_PACE(50);
                gpib_write(address, cfg->rqs_enabled ? "RQS ON" : "RQS OFF");
                GPIB_PACE(50);
                
                ps5004_set_output(address, cfg->output_enabled);
                
                printf("Settings applied!\n");
                printf("Press any key to continue...");
                getch();
                break;
                
            case 'E':  /* Test measurement */
                printf("\n\nTesting measurement...\n");
                
                ps5004_set_display(address, "VOLTAGE");
                GPIB_PACE(100);
                
                printf("Voltage reading: ");
                temp_float = ps5004_read_value(address);
                printf("%.4f V\n", temp_float);
                
                ps5004_set_display(address, "CURRENT");
                GPIB_PACE(100);
                
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
                        printf("\nEnter positive voltage (0-32V): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 32.0) {
                            cfg->voltage1 = temp_float;
                        }
                        break;
                    case 'C':
                        printf("\nEnter current limit in mA (50-750): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 50.0 && temp_float <= 750.0) {
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
                        printf("\nEnter negative voltage (0-32V): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.0 && temp_float <= 32.0) {
                            cfg->voltage2 = temp_float;
                        }
                        break;
                    case 'C':
                        printf("\nEnter current limit in mA (50-750): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 50.0 && temp_float <= 750.0) {
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
                        printf("\nEnter current limit in A (0.1-3.0): ");
                        scanf("%f", &temp_float);
                        if (temp_float >= 0.1 && temp_float <= 3.0) {
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
                GPIB_PACE(200);
                
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
                
            case '9':  /* VTRA tracking-voltage test */
                printf("\n\nTesting tracking voltage command...\n");
                printf("Note: Hardware tracking mode is set by front panel switch.\n");
                printf("VTRA command sets both POS and NEG to same voltage.\n\n");
                
                printf("Setting both supplies to 10V using VTRA...\n");
                ps5010_set_tracking_voltage(address, 10.0);
                GPIB_PACE(200);
                
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

/* Raw Driver488-command console (gpib_gg.c interprets the commands under GRIDGPIB):
   each line is sent with CRLF, then a reply is always read. */
void gpib_terminal_mode(void) {
    char command[GPIB_BUFFER_SIZE];
    char response[GPIB_BUFFER_SIZE];
    int done = 0;
    int bytes_read;
    int i;
    
    clrscr();
    printf("GPIB Terminal Mode - %s\n", gpib_driver_name);
    printf("=================================\n\n");
    printf("Enter Driver488-style commands (as in KYBDCTRL)\n");
    printf("Examples:\n");
    printf("  REMOTE 16\n");
    printf("  OUTPUT 16; voltage?\n");
    printf("  ENTER 16\n");
    printf("  STATUS\n");
    printf("Type 'EXIT' to return\n\n");
    printf("\n");
    
    ieee_write("status\r\n");
    GPIB_PACE(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Initial Status: %s\n", response);
    }
    
    drain_console_line();

    while (!done) {
        int got;

        printf("\nGPIB> ");
        fflush(stdout);

        /* 2 bytes kept for the CRLF appended below */
        got = read_console_line(command, sizeof(command) - 2);
        if (got < 0) break;
        if (got == 0) continue;

        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }

        if (strlen(command) == 0) {
            continue;
        }

        printf("Sending: '%s'\n", command);
        strcat(command, "\r\n");
        if (ieee_write(command) < 0) {
            printf("Command not sent (driver write failed)\n");
            continue;
        }

        GPIB_PACE(100);
        
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

        GPIB_PACE(50);
    }
}

/* DM5120 buffer/STOINT/SRQ submenus. Option tables and reply buffers are far to spare DGROUP. */

static const char* __far buffer_opts[] = {
    "Count", "Average", "Min", "Max", "Size", "One Val", "All Vals", "Set Size", "Clear", "Exit"
};

void dm5120_buffer_query_menu(int address) {
    char choice;
    static char __far response[128];
    float value;
    int count, i, buffer_size;
    int done = 0;
    
    while (!done) {
        clrscr();
        printf("DM5120 Buffer Menu - GPIB %d\n", address);
        printf("============================\n");
        
        buffer_size = dm5120_query_buffer_size(address);
        count = dm5120_get_buffer_count(address);
        
        if (buffer_size == 0) printf("Size: CIRCULAR (500), Count: %d\n\n", count);
        else if (buffer_size > 0) printf("Size: %d (LINEAR), Count: %d\n\n", buffer_size, count);
        else printf("Size: no reply, Count: %d\n\n", count);
        
        for (i = 0; i < 9; i++) {
            printf("%d.%s ", i+1, buffer_opts[i]);
        }
        printf("0.%s\n\n", buffer_opts[9]);
        
        choice = getch();
        
        switch(choice) {
            case '1':
                count = dm5120_get_buffer_count(address);
                printf("\nCount: %d\n", count);
                break;
            case '2':
                value = dm5120_get_buffer_average(address);
                printf("\nAverage: %.6e\n", value);
                break;
            case '3':
                value = dm5120_get_buffer_min(address);
                printf("\nMin: %.6e\n", value);
                break;
            case '4':
                value = dm5120_get_buffer_max(address);
                printf("\nMax: %.6e\n", value);
                break;
            case '5':
                buffer_size = dm5120_query_buffer_size(address);
                if (buffer_size == 0) printf("\nSize: CIRCULAR\n");
                else if (buffer_size > 0) printf("\nSize: %d\n", buffer_size);
                else printf("\nNo reply\n");
                break;
            case '6':
                gpib_write_dm5120(address, "READ ONESTORE");
                GPIB_PACE(100);
                if (gpib_read_dm5120(address, response, 128) > 0) {
                    printf("\nValue: %s\n", response);
                }
                break;
            case '7':
                count = dm5120_get_buffer_count(address);
                buffer_size = dm5120_query_buffer_size(address);
                /* ALLSTORE (like BUFAVE/BUFMIN/BUFMAX) holds the bus until a linear buffer fills */
                if (buffer_size > 0 && count < buffer_size) {
                    printf("\nBuffer holds %d of %d; the meter answers when it is full\n", count, buffer_size);
                }
                printf("\nReading %d values...\n", count);
                gpib_write_dm5120(address, "READ ALLSTORE");
                GPIB_PACE(200);
                if (gpib_read_dm5120(address, response, 128) > 0) {
                    printf("Data: %s\n", response);
                }
                break;
            case '8':
                printf("\nEnter size (0=CIRCULAR, 10-500): ");
                scanf("%d", &i);
                if (i == 0 || (i >= 10 && i <= 500)) {
                    dm5120_set_buffer_size(address, i);
                    printf("Set to %s\n", i == 0 ? "CIRCULAR" : "LINEAR");
                }
                break;
            case '9':
                /* Resending the size clears the buffer */
                buffer_size = dm5120_query_buffer_size(address);
                if (buffer_size >= 0) {
                    dm5120_set_buffer_size(address, buffer_size);
                    printf("\nBuffer cleared\n");
                } else {
                    printf("\nNo reply to BUFSZ?, not cleared\n");
                }
                break;
            case '0':
                done = 1;
                break;
        }
        if (choice != '0') {
            printf("Press any key...");
            getch();
        }
    }
}

static const char* __far stoint_opts[] = {
    "Set ONE", "Set ms", "Query", "Validate", "Common", "Exit"
};
static const int __far test_intervals[] = {1, 5, 10, 20, 100, 175, 350};
#define N_TEST_INTERVALS (sizeof(test_intervals) / sizeof(test_intervals[0]))
static const char* __far conflict_msgs[] = {
    "OHMS/dB conflict", "AUTO range conflict", "CIRCULAR conflict",
    "5-6 digit conflict", "4+ digit conflict", "invalid slot",
    "shorter than a conversion (error 551)"   /* -1..-5, BAD_SLOT, TOO_SHORT */
};

static void print_stoint(int interval) {
    if (interval > 0) printf("%d ms\n", interval);
    else if (interval == 0) printf("ONE\n");
    else printf("no reply\n");
}

void dm5120_stoint_menu(int address, int slot) {
    char choice;
    int interval, current_interval;
    long entered;
    char done = 0;
    int validation_result, i;
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    while (!done) {
        clrscr();
        printf("DM5120 STOINT Menu - GPIB %d\n", address);
        printf("============================\n");
        
        current_interval = dm5120_query_storage_interval(address);
        printf("Current: ");
        print_stoint(current_interval);
        printf("Function: %s, Digits: %d, AutoCal: %s\n", cfg->function, cfg->digits,
               cfg->autocal_off ? "OFF" : "ON");
        printf("Range: %s, Buffer: %s\n",
               cfg->range_mode == 0 ? "AUTO" : "MANUAL",
               cfg->buffer_enabled ? "ON" : "OFF");
        printf("Shortest interval for these settings: %d ms\n\n", dm5120_min_stoint_ms(slot));
        
        for (i = 0; i < 5; i++) {
            printf("%d.%s ", i+1, stoint_opts[i]);
        }
        printf("0.%s\n\n", stoint_opts[5]);
        
        choice = getch();
        
        switch(choice) {
            case '1':
                gpib_write_dm5120(address, "STOINT ONE");
                GPIB_PACE(100);
                printf("\nSTOINT set to ONE\n");
                break;
            case '2':
                /* long so an out-of-range entry cannot wrap into a valid int; capped at
                   what dm5120_set_storage_interval's int carries */
                printf("\nEnter ms (1-32767): ");
                entered = 0;
                scanf("%ld", &entered);
                if (entered >= 1 && entered <= 32767) {
                    int sent;
                    interval = (int)entered;
                    validation_result = dm5120_validate_stoint_settings(slot, interval);
                    sent = dm5120_set_storage_interval(address, slot, interval);
                    if (sent >= 0) {
                        printf("Set to %d ms\n", sent);
                        if (sent != interval && validation_result < 0) {
                            printf("Raised from %d ms: %s\n", interval, conflict_msgs[-validation_result-1]);
                        }
                    } else {
                        printf("STOINT not set\n");
                    }
                } else {
                    printf("Invalid range\n");
                }
                break;
            case '3':
                current_interval = dm5120_query_storage_interval(address);
                printf("\nSTOINT: ");
                print_stoint(current_interval);
                break;
            case '4':
                printf("\nValidating...\n");
                for (i = 0; i < N_TEST_INTERVALS; i++) {
                    validation_result = dm5120_validate_stoint_settings(slot, test_intervals[i]);
                    printf("%4dms: %s\n", test_intervals[i],
                           validation_result == 0 ? "OK" : conflict_msgs[-validation_result-1]);
                }
                break;
            case '5':
                printf("\nCommon values:\n1ms=1000r/s, 5ms=200r/s\n");
                printf("10ms=100r/s, 20ms=50r/s, 100ms=10r/s\n");
                printf("175ms (factory default) suits 6.5 digits with AutoCal\n");
                break;
            case '0':
                done = 1;
                break;
        }
        if (choice != '0') {
            printf("Press any key...");
            getch();
        }
    }
}

static const char* __far srq_opts[] = {
    "FULL", "HALF", "RDY", "OPC", "RQS", "Query", "Smart", "Apply", "Exit"
};

void dm5120_srq_menu(int address, int slot) {
    char choice;
    static char __far response[64];
    char done = 0;
    char full_enabled = 0, half_enabled = 0, rdy_enabled = 0, opc_enabled = 0;
    dm5120_config *cfg = &g_dm5120_config[slot];
    int i;
    
    while (!done) {
        clrscr();
        printf("DM5120 SRQ Menu - GPIB %d\n", address);
        printf("=========================\n");
        
        printf("Buffer: %s (%d), Mode: %s\n", 
               cfg->buffer_enabled ? "ON" : "OFF", cfg->buffer_size,
               cfg->buffer_size == 0 ? "CIRCULAR" : "LINEAR");
        
        printf("Events: FULL:%s HALF:%s RDY:%s OPC:%s\n\n", 
               full_enabled ? "ON" : "OFF", half_enabled ? "ON" : "OFF",
               rdy_enabled ? "ON" : "OFF", opc_enabled ? "ON" : "OFF");
        
        for (i = 0; i < 8; i++) {
            printf("%d.%s ", i+1, srq_opts[i]);
        }
        printf("0.%s\n\n", srq_opts[8]);
        
        choice = getch();
        
        switch(choice) {
            case '1':
                full_enabled = !full_enabled;
                printf("\nFULL: %s\n", full_enabled ? "ON" : "OFF");
                break;
            case '2':
                half_enabled = !half_enabled;
                printf("\nHALF: %s\n", half_enabled ? "ON" : "OFF");
                break;
            case '3':
                rdy_enabled = !rdy_enabled;
                printf("\nRDY: %s\n", rdy_enabled ? "ON" : "OFF");
                break;
            case '4':
                opc_enabled = !opc_enabled;
                printf("\nOPC: %s\n", opc_enabled ? "ON" : "OFF");
                break;
            case '5':
                printf("\nRQS (1/0): ");
                {
                    int rqs;
                    scanf("%d", &rqs);
                    gpib_write_dm5120(address, rqs ? "RQS ON" : "RQS OFF");
                    GPIB_PACE(100);
                    printf("RQS: %s\n", rqs ? "ON" : "OFF");
                }
                break;
            case '6':
                gpib_write_dm5120(address, "EVENT?");
                GPIB_PACE(100);
                if (gpib_read_dm5120(address, response, 64) > 0) {
                    float code;
                    printf("\n%s\n", response);
                    if (dm5120_parse_value(response, &code)) {
                        printf("%s\n", dm5120_error_text((int)code));
                    }
                }
                break;
            case '7':
                {
                    int is_circular = (cfg->buffer_size == 0);
                    dm5120_configure_srq_events_smart(address, cfg->buffer_size, 
                                                     is_circular, 10000);
                    if (is_circular) {
                        full_enabled = 0; half_enabled = 1;
                        printf("\nCircular: HALF=ON\n");
                    } else {
                        full_enabled = 1; half_enabled = (cfg->buffer_size > 100);
                        printf("\nLinear: FULL=ON HALF=%s\n", half_enabled ? "ON" : "OFF");
                    }
                    rdy_enabled = 1; opc_enabled = 0;
                }
                break;
            case '8':
                dm5120_configure_srq_events(address, full_enabled, half_enabled, 
                                           rdy_enabled, opc_enabled);
                printf("\nSRQ Applied\n");
                break;
            case '0':
                done = 1;
                break;
        }
        if (choice != '0') {
            printf("Press any key...");
            getch();
        }
    }
}

