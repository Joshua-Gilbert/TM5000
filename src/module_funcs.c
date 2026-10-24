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
    printf("  DM5120:  FUNCT?, RANGE?, ERROR?, FUNCT DCV, RANGE AUTO, SEND\n");
    printf("  DM5010:  FUNCT?, ERR?, DCV 0, SEND\n");
    printf("  DC5009/DC5010: FUNC?, ERR?, FREQ A, AVE -1, SEND\n");
    printf("  PS5004:  ID?, SET?, VOLTAGE?, CURRENT?, REGULATION?, ERROR?, HELP?\n");
    printf("           VOLTAGE 5.0, CURRENT 0.1, OUTPUT ON/OFF, DISPLAY CURRENT, SEND\n");
    printf("  PS5010:  REG?, ERR?, VPOS?   FG5010: FREQ?, AMPL?, ERR?\n");
    printf("  All:     ID?, SET? (Tek codes and formats; *IDN? and other SCPI fail)\n");
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
    int choice, done = 0, i;
    long lval;

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
        printf("A. Trigger delay (DELAY)          B. Trigger on GET (DT TRIG/OFF)\n");
        printf("C. Send GET (bus trigger)         D. Null on this reading (NULLVAL ACQUIRE)\n");
        printf("E. Press a front-panel key (KEY)  F. Event reporting (ERRSTAT/OVER/USER)\n");
        printf("A-F are not kept in the slot; PONSAVE keeps them in the meter.\n");
        printf("0. Return\n\nSelect option: ");
        choice = toupper(getch());
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
            case 'A':
                printf("Delay from trigger to conversion, 0-65000 ms (manual p. 3-11): ");
                lval = -1;
                scanf("%ld", &lval);
                if (lval < 0 || lval > 65000L) {
                    printf("Out of range\n");
                    break;
                }
                sprintf(reply, "DELAY %ld", lval);
                gpib_write_dm5120(address, reply);
                if (dm5120_show_errors(address) == 0) printf("DELAY %ld ms set\n", lval);
                break;
            case 'B':
                /* GET adds a trigger to the TRIGGER source, it doesn't replace it (p. 3-12) */
                printf("Trigger a reading on GET: 1 = DT TRIG, 0 = DT OFF: ");
                lval = -1;
                scanf("%ld", &lval);
                if (lval != 0 && lval != 1) break;
                gpib_write_dm5120(address, lval ? "DT TRIG" : "DT OFF");
                if (dm5120_show_errors(address) == 0) printf("DT %s\n", lval ? "TRIG" : "OFF");
                break;
            case 'C':
                sprintf(reply, "TRIGGER %d\r\n", address);   /* Driver488's GET; gpib_gg has it too */
                ieee_write(reply);
                printf("GET sent (a reading follows only with DT TRIG)\n");
                break;
            case 'D':
                gpib_write_dm5120(address, "NULLVAL ACQUIRE");
                if (dm5120_show_errors(address) == 0)
                    printf("Null value taken from the latest reading; NULL ON uses it\n");
                break;
            case 'E':
                /* Table 3-2 */
                printf("0 ACV  1 OHMS  2 DCA  3 ACA  4 DECREMENT  5 INCREMENT  6 AUTO  7 NULL\n");
                printf("8 FILTER  9 dB  10 DCV  11 ENTER  12 PRGM  13 EXT  14 INT\n");
                printf("Key (0-14): ");
                lval = -1;
                scanf("%ld", &lval);
                if (lval < 0 || lval > 14) break;
                gpib_write_dm5120(address, "TEXT \"\"");   /* KEY needs the display text cleared */
                sprintf(reply, "KEY %ld", lval);
                gpib_write_dm5120(address, reply);
                if (dm5120_show_errors(address) == 0) printf("Key %ld pressed\n", lval);
                break;
            case 'F':
                {
                    static const char * __far evs[] = { "ERRSTAT", "OVER", "USER" };
                    printf("1 ERRSTAT (report errors)  2 OVER (overflow)  3 USER (ID key): ");
                    lval = -1;
                    scanf("%ld", &lval);
                    if (lval < 1 || lval > 3) break;
                    printf("1 = ON, 0 = OFF: ");
                    i = -1;
                    scanf("%d", &i);
                    if (i != 0 && i != 1) break;
                    sprintf(reply, "%s %s", evs[lval - 1], i ? "ON" : "OFF");
                    gpib_write_dm5120(address, reply);
                    if (dm5120_show_errors(address) == 0) printf("%s\n", reply);
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

/* What the PS5004 display, and so SEND, measures (manual p. 3-6) */
static const char* __far ps5004_disp_name[] = { "VOLTAGE (V)", "CURRENT (mA)", "CURRENT LIMIT (mA)" };

/* Number from the console in [lo, hi]; 0 after showing why it was refused */
static int ps5004_ask(const char *prompt, float lo, float hi, float *out) {
    float v;

    printf("%s", prompt);
    if (scanf("%f", &v) != 1) {
        drain_console_line();
        printf("Not a number\n");
    } else if (v < lo || v > hi) {
        printf("Out of range\n");
    } else {
        *out = v;
        return 1;
    }
    printf("Press any key to continue...");
    getch();
    return 0;
}

/* Menu choice 0..max from the console, -1 if none */
static int ps5004_ask_choice(int max) {
    int v = -1;
    if (scanf("%d", &v) != 1) drain_console_line();
    return v >= 0 && v <= max ? v : -1;
}

/* Raw bytes of a binary-block reply. Shown only: the block format is not in
   the manual (p. 3-10), and a 0Ah byte inside it ends the read early, so the
   tail is drained rather than left to answer the next query. */
static void ps5004_show_block(int address, const char *query) {
    char *r;
    int i, n = ps5004_query_block(address, query, &r);

    drain_input_buffer();
    printf("%s %d bytes:", query, n);
    for (i = 0; i < n && i < 40; i++) printf(" %02X", (unsigned char)r[i]);
    printf("\n");
}

/* Apply with RQS OFF so errors wait for ERROR? (manual p. 3-7) and DT OFF so
   each setting takes effect now; DT SET would hold them for GET (p. 3-6) */
static void ps5004_apply(int slot) {
    static const unsigned char __far plain[] = { 1, 2, 3, 5 };   /* VRI CRI URI USER */
    ps5004_config *cfg = &g_ps5004_config[slot];
    ps5004_config now;
    int address = g_system->modules[slot].gpib_address;
    int i;
    char *r;

    if (cfg->output_enabled) {
        printf("\n\nThe output will be ON at %.4f V, %.1f mA limit.", cfg->voltage, cfg->current_limit * 1000);
        if (!dm5120_confirm("Apply")) return;
    }
    printf("\n\nApplying settings to PS5004...\n");

    gpib_remote(address);
    GPIB_PACE(200);
    ps5004_release_dt(address);
    ps5004_send_flag(address, PS5004_RQS, 0);
    ps5004_show_errors(address, 0);     /* older events, and a 206 from the GET; report only Apply's */
    /* Output OFF before the new voltage, so a live load never sees it; ON only after */
    if (!cfg->output_enabled) ps5004_set_output(address, 0);
    ps5004_set_voltage(address, cfg->voltage);
    ps5004_set_current(address, cfg->current_limit);
    ps5004_forget_display(slot);
    ps5004_select_display(slot, cfg->display_mode);
    for (i = 0; i < 4; i++) ps5004_send_flag(address, plain[i], ps5004_flag(cfg, plain[i]));
    if (cfg->output_enabled) ps5004_set_output(address, 1);

    /* VRI/CRI/URI crossovers (724, 725) show up here too, so "codes", not "errors" */
    if (ps5004_show_errors(address, 1) == 0) printf("Settings applied\n");
    else printf("Settings sent; the supply reported the codes above\n");
    if (cfg->rqs_enabled) ps5004_send_flag(address, PS5004_RQS, 1);
    /* Last: after DT SET even RQS would wait for GET */
    if (cfg->dt_enabled) {
        gpib_write(address, cfg->dt_set ? "DT SET" : "DT ON");
        GPIB_PACE(50);
    }

    /* Read back: shows the supply's own 0.5 mV / 2.5 mA rounding */
    r = ps5004_query(address, "SET?");
    if (r == NULL) {
        printf("No reply to SET?\n");
    } else {
        now = *cfg;
        ps5004_parse_settings(r, &now);
        if (ps5004_compare(&now, cfg) == 0) printf("Read back with SET?: all settings match\n");
        else printf("(read back with SET?)\n");
    }
}

/* ID, ROM test, errors and events, single queries, HELP?, INIT, device clear */
static void ps5004_utilities_menu(int slot) {
    /* Each setting's own query (manual p. 3-5 to 3-15); SET? returns them all */
    static const char* __far queries[] = {
        "VOLTAGE?", "CURRENT?", "OUTPUT?", "DISPLAY?", "VRI?", "CRI?",
        "URI?", "DT?", "USER?", "RQS?", "REGULATION?"
    };
    int address = g_system->modules[slot].gpib_address;
    int choice, done = 0, i;
    char *r;
    float v;

    while (!done) {
        clrscr();
        printf("PS5004 Utilities - GPIB %d\n", address);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. ROM self test (TEST)\n");
        printf("3. Read pending errors/events (serial poll, ERROR?)\n");
        printf("4. Last event with its description (ERRMSG?)\n");
        printf("5. Last event code (EVENT?)\n");
        printf("6. Read each setting with its own query\n");
        printf("7. Firmware command list (HELP?)\n");
        printf("8. Binary settings blocks (LLSET?, FVOLTS?)\n");
        printf("9. INIT - power-on settings: output OFF, 0 V, 100 mA\n");
        printf("A. Device clear (SDC)\n");
        printf("0. Return\n\nSelect option: ");
        choice = toupper(getch());
        printf("\n\n");

        switch (choice) {
            case '1':
            case '4':
            case '5':
            case '7':
                r = ps5004_query(address, choice == '1' ? "ID?" : choice == '4' ? "ERRMSG?" :
                                          choice == '5' ? "EVENT?" : "HELP?");
                printf("%s\n", r ? r : "No reply");
                break;
            case '2':   /* TEST 0 passes, 394 fails (manual p. 3-13) */
                r = ps5004_query(address, "TEST");
                if (r && ps5004_parse_number(r, &v)) printf("%s: ROM test %s\n", r, v == 0 ? "passed" : "FAILED");
                else printf("No reply\n");
                break;
            case '3':
                if (ps5004_show_errors(address, 1) == 0) printf("No errors or events pending\n");
                break;
            case '6':
                for (i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
                    r = ps5004_query(address, queries[i]);
                    printf("%-12s %s\n", queries[i], r ? r : "no reply");
                }
                break;
            case '8':
                ps5004_show_block(address, "LLSET?");
                ps5004_show_block(address, "FVOLTS?");
                printf("\nShown only: the block format is not in the manual (p. 3-10),\n");
                printf("so TM5000 never sends LLSET or FVOLTS.\n");
                break;
            case '9':
                if (dm5120_confirm("INIT turns the output OFF and sets 0 V, 100 mA")) {
                    ps5004_release_dt(address);
                    ps5004_init(address);
                    ps5004_clear_srq(address);     /* a 206 from the release; INIT sets RQS ON */
                    init_ps5004_config(slot);
                    printf("\nSupply and slot at the power-on settings (manual Table 3-2)\n");
                }
                break;
            case 'A':
                gpib_clear(address);
                printf("Device clear sent: pending settings and events cleared,\n");
                printf("except power-on (manual p. 3-18)\n");
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

void configure_ps5004_advanced(int slot) {
    static const char __far flag_keys[] = "3567x9A";    /* ps5004_flag() order; DT has its own prompt */
    int choice, done = 0, temp_int, shown;
    int address = g_system->modules[slot].gpib_address;
    ps5004_config *cfg = &g_ps5004_config[slot];
    ps5004_config now;
    float temp_float;
    char *r;

    ps5004_forget_display(slot);   /* the terminal may have changed it */

    while (!done) {
        clrscr();
        printf("PS5004 Power Supply Configuration - Slot %d (GPIB %d)\n", slot, address);
        printf("===================================================\n\n");

        printf("1. Voltage: %.4f V\n", cfg->voltage);
        printf("2. Current Limit: %.1f mA\n", cfg->current_limit * 1000);
        printf("3. Output: %s\n", cfg->output_enabled ? "ON" : "OFF");
        printf("4. Display (what the monitor logs): %s\n", ps5004_disp_name[(unsigned)cfg->display_mode % 3]);
        printf("5. Voltage Regulation Interrupt (VRI): %s\n", cfg->vri_enabled ? "ON" : "OFF");
        printf("6. Current Regulation Interrupt (CRI): %s\n", cfg->cri_enabled ? "ON" : "OFF");
        printf("7. Unregulated Interrupt (URI): %s\n", cfg->uri_enabled ? "ON" : "OFF");
        printf("8. Device Trigger (DT): %s\n",
               !cfg->dt_enabled ? "OFF" : cfg->dt_set ? "SET (settings wait for GET)" : "ON");
        printf("9. INST ID Button Request (USER): %s\n", cfg->user_enabled ? "ON" : "OFF");
        printf("A. Service Requests (RQS): %s\n", cfg->rqs_enabled ? "ON" : "OFF");

        printf("\nC. Query Status (SET?, errors)    D. Apply Settings to PS5004\n");
        printf("E. Test Measurement (V, I, limit)  F. Communication Test\n");
        printf("L. Load Settings from the PS5004   T. Trigger (GET)\n");
        printf("U. Utilities (ID, ROM test, INIT, HELP)   0. Return\n");
        printf("\nSettings take effect on D. The message terminator (EOI or LF/EOI) is a\n");
        printf("rear-panel switch, not a bus command (manual p. 3-16).\n");

        printf("\nSelect option: ");
        choice = toupper(getch());

        switch(choice) {
            case '1':  /* 0.5 mV steps (manual p. 3-14) */
                if (ps5004_ask("\n\nVoltage, 0 to 20 V: ", 0.0, 20.0, &temp_float))
                    cfg->voltage = ps5004_round_voltage(temp_float);
                break;

            case '2':  /* 2.5 mA steps (manual p. 3-5) */
                if (ps5004_ask("\n\nCurrent limit, 10 to 305 mA: ", 10.0, 305.0, &temp_float))
                    cfg->current_limit = ps5004_round_current(temp_float / 1000.0);
                break;

            case '3':
            case '5':
            case '6':
            case '7':
            case '9':
            case 'A':
                temp_int = strchr(flag_keys, choice) - flag_keys;
                ps5004_set_flag(cfg, temp_int, !ps5004_flag(cfg, temp_int));
                break;

            case '4':  /* The front-panel limit knob works only in CURRENT LIMIT (manual p. 2-5) */
                printf("\n\n0. VOLTAGE  1. CURRENT  2. CURRENT LIMIT\nSelect: ");
                temp_int = ps5004_ask_choice(2);
                if (temp_int >= 0) cfg->display_mode = temp_int;
                break;

            case '8':
                printf("\n\n0. OFF  1. ON (GET accepted)\n");
                printf("2. SET: every later setting, the monitor's DISPLAY too, waits for GET (T)\nSelect: ");
                temp_int = ps5004_ask_choice(2);
                if (temp_int >= 0) {
                    cfg->dt_enabled = temp_int != 0;
                    cfg->dt_set = temp_int == 2;
                }
                break;

            case 'C':  /* Query status */
                printf("\n\nPS5004 settings (SET?):\n");
                r = ps5004_query(address, "SET?");
                if (r) ps5004_print_items(r);
                else printf("  No reply\n");
                printf("\nRegulation: %s\n", ps5004_regulation_name(ps5004_get_regulation_status(address)));
                printf("\nPending errors/events:\n");
                if (ps5004_show_errors(address, 1) == 0) printf("  none\n");
                printf("\nPress any key to continue...");
                getch();
                break;

            case 'D':
                ps5004_apply(slot);
                printf("Press any key to continue...");
                getch();
                break;

            case 'E':  /* Each display change waits two DVM readings (manual p. 3-12) */
                printf("\n\nMeasuring...\n");
                for (temp_int = 0; temp_int < 3; temp_int++) {
                    printf("%-20s", ps5004_disp_name[temp_int]);
                    ps5004_forget_display(slot);
                    ps5004_select_display(slot, temp_int);
                    /* Label by DISPLAY?: under DT SET the change waits for GET (p. 3-6) */
                    shown = ps5004_display_shown(address);
                    if (shown >= 0 && shown != temp_int) printf("not selected (DT SET? T sends GET)\n");
                    else if (shown < 0 || !ps5004_read_value(address, &temp_float)) printf("no reply\n");
                    else if (temp_int == 0) printf("%.4f V\n", temp_float);
                    else printf("%.1f mA\n", temp_float * 1000);
                }
                ps5004_forget_display(slot);
                ps5004_select_display(slot, cfg->display_mode);
                printf("Regulation          %s\n", ps5004_regulation_name(ps5004_get_regulation_status(address)));
                printf("\nPress any key to continue...");
                getch();
                break;

            case 'F':
                test_ps5004_comm(address);
                break;

            case 'L':  /* The supply's settings become the slot's */
                r = ps5004_query(address, "SET?");
                now = *cfg;
                if (r && ps5004_parse_settings(r, &now) == 10) {
                    *cfg = now;
                    ps5004_forget_display(slot);
                    printf("\n\nLoaded the supply's settings\n");
                } else {
                    printf("\n\nNo complete SET? reply; settings unchanged\n");
                }
                printf("Press any key to continue...");
                getch();
                break;

            case 'T':  /* GET is ignored, error 206, unless DT is enabled (manual p. 3-18) */
                printf("\n\n");
                r = ps5004_query(address, "DT?");
                if (r && strstr(r, "OFF")) printf("DT is OFF: the supply will ignore GET.\n");
                ps5004_trigger(address);
                printf("GET sent\n");
                ps5004_show_errors(address, 1);
                printf("Press any key to continue...");
                getch();
                break;

            case 'U':
                ps5004_utilities_menu(slot);
                break;

            case '0':
                done = 1;
                break;
        }
    }
}

/* ---- PS5010: menu values go to the supply only on Apply or an action that
   says so; anything that can turn an output on or change a live one asks first. */

static char __far ps5010_cmd[160];   /* also SET? replies, about 120 characters (p. 3-11) */

/* Number typed at the console: 1 typed, 0 ENTER alone, -1 ESC. Reads keys with
   getch so a newline left by an earlier scanf can't stand in for an answer. */
static int ps5010_getnum(float *v) {
    char s[12];
    int n = 0, c;

    fflush(stdout);
    while ((c = getch()) != '\r') {
        if (c == 27) {
            printf("\n");
            return -1;
        }
        if (c == '\b') {
            if (n) {
                n--;
                putch('\b'); putch(' '); putch('\b');
            }
        } else if (n < 11 && (isdigit(c) || c == '.')) {
            s[n++] = (char)c;
            putch(c);
        }
    }
    printf("\n");
    s[n] = '\0';
    if (n == 0) return 0;
    return sscanf(s, "%f", v) == 1 ? 1 : -1;
}

/* 1 with *v accepted (typed in range, or kept with ENTER), 0 otherwise */
static int ps5010_ask(const char *what, float lo, float hi, float *v) {
    float x;
    int r;

    printf("%s (%.2f-%.2f, ENTER keeps %.2f): ", what, lo, hi, *v);
    r = ps5010_getnum(&x);
    if (r == 0) return 1;
    if (r < 0) return 0;
    if (x < lo - 0.001f || x > hi + 0.001f) {
        printf("Out of range, not changed\n");
        return 0;
    }
    *v = x;
    return 1;
}

static int ps5010_confirm(const char *what) {
    int c;
    printf("%s (Y/N)? ", what);
    c = toupper(getch());
    printf("%c\n", c == 'Y' ? 'Y' : 'N');
    return c == 'Y';
}

static void ps5010_pause(void) {
    printf("\nPress any key...");
    getch();
}

/* What the supply would refuse with 205/204 (p. 3-21), or NULL */
static const char *ps5010_check(const ps5010_config *cfg) {
    if (cfg->voltage1 < 0.0f || cfg->voltage1 > 32.001f ||
        cfg->voltage2 < 0.0f || cfg->voltage2 > 32.001f) return "floating voltage outside 0-32 V";
    if (cfg->logic_voltage < 4.499f || cfg->logic_voltage > 5.501f) return "logic voltage outside 4.50-5.50 V";
    if (cfg->current_limit1 < 0.049f ||
        cfg->current_limit1 > ps5010_max_current(cfg->high_power, cfg->voltage1) + 0.001f ||
        cfg->current_limit2 < 0.049f ||
        cfg->current_limit2 > ps5010_max_current(cfg->high_power, cfg->voltage2) + 0.001f)
        return "floating current limit outside the compartment's range (option 6)";
    if (cfg->logic_current_limit < 0.099f || cfg->logic_current_limit > 3.001f) return "logic current limit outside 0.1-3.0 A";
    return NULL;
}

/* Lower a floating limit that a new voltage or compartment no longer allows */
static void ps5010_fit_limits(ps5010_config *cfg) {
    float m = ps5010_max_current(cfg->high_power, cfg->voltage1);
    if (cfg->current_limit1 > m + 0.001f) {
        cfg->current_limit1 = m;
        printf("IPOS lowered to %.2f A, the most allowed at %.2f V\n", m, cfg->voltage1);
    }
    m = ps5010_max_current(cfg->high_power, cfg->voltage2);
    if (cfg->current_limit2 > m + 0.001f) {
        cfg->current_limit2 = m;
        printf("INEG lowered to %.2f A, the most allowed at %.2f V\n", m, cfg->voltage2);
    }
}

static const char *ps5010_flag_word(int i, int on) {
    return on ? (i == 7 ? "SET" : "ON") : "OFF";
}

static void ps5010_show_out(int address) {
    if (ps5010_query(address, "OUT?", ps5010_cmd, sizeof(ps5010_cmd))) printf("Outputs now: %s\n", ps5010_cmd);
}

/* Every setting, read back into the menu (one query each) */
static void ps5010_read_back(int slot) {
    static const char * const __far q[] = { "VPOS?", "IPOS?", "VNEG?", "INEG?", "VLOG?", "ILOG?" };
    ps5010_config *cfg = &g_ps5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    float *dst[6];
    float x;
    int i, f, missed = 0;

    dst[0] = &cfg->voltage1; dst[1] = &cfg->current_limit1;
    dst[2] = &cfg->voltage2; dst[3] = &cfg->current_limit2;
    dst[4] = &cfg->logic_voltage; dst[5] = &cfg->logic_current_limit;
    for (i = 0; i < 6; i++) {
        if (ps5010_query_value(address, q[i], &x)) *dst[i] = (float)fabs(x);
        else missed++;
    }
    for (i = 0; i < PS5010_NFLAGS; i++) {
        sprintf(ps5010_cmd, "%s?", ps5010_flag_cmd[i]);
        f = ps5010_query_on(address, ps5010_cmd);
        if (f < 0) missed++;
        else ps5010_set_flag(cfg, i, f);
    }
    if (missed) printf("%d queries got no reply; those menu values are unchanged\n", missed);
    else printf("Menu now shows the supply's settings\n");
}

/* Send the menu's settings. Reporting and DT go first with RQS off, so every
   error below queues for ERR?; RQS is restored at the end. */
static void ps5010_apply(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    const char *bad = ps5010_check(cfg);
    int i, n, errors;

    if (bad) {
        printf("\nNot sent: %s\n", bad);
        return;
    }
    if (cfg->fsout_enabled || cfg->logic_enabled) {
        printf("\nOutputs that will be ON:\n");
        if (cfg->fsout_enabled)
            printf("  floating +%.2f V / -%.2f V, limits %.2f / %.2f A\n", cfg->voltage1,
                   cfg->voltage2, cfg->current_limit1, cfg->current_limit2);
        if (cfg->logic_enabled)
            printf("  logic %.2f V, limit %.1f A\n", cfg->logic_voltage, cfg->logic_current_limit);
        if (!ps5010_confirm("Energise them")) return;
    }

    printf("\nApplying settings to PS5010...\n");
    gpib_remote(address);
    GPIB_PACE(200);

    n = sprintf(ps5010_cmd, "RQS OFF");
    for (i = 2; i < PS5010_NFLAGS; i++) {
        if (i == 5) continue;    /* RQS */
        n += sprintf(ps5010_cmd + n, ";%s %s", ps5010_flag_cmd[i], ps5010_flag_word(i, ps5010_get_flag(cfg, i)));
    }
    gpib_write(address, ps5010_cmd);
    errors = ps5010_show_errors(address);

    /* One message: the supply executes its settings as a group, so no half-sent
       pair of V and I can conflict, and a processing error drops them all (p. 3-17) */
    /* snprintf: a damaged settings file can hold any float */
    snprintf(ps5010_cmd, sizeof(ps5010_cmd), "VPOS %.2f;IPOS %.2f;VNEG %.2f;INEG %.2f;VLOG %.2f;ILOG %.1f;FSOUT %s;LSOUT %s",
            cfg->voltage1, cfg->current_limit1, cfg->voltage2, cfg->current_limit2,
            cfg->logic_voltage, cfg->logic_current_limit,
            cfg->fsout_enabled ? "ON" : "OFF", cfg->logic_enabled ? "ON" : "OFF");
    gpib_write(address, ps5010_cmd);
    delay(50);   /* output switching up to 35 ms, current change 20 ms (p. 1-3, 1-4) */
    if (ps5010_show_errors(address) != 0) errors = 1;   /* -1: no reply */
    if (cfg->srq_enabled) gpib_write(address, "RQS ON");

    printf("%s\n", errors ? "Errors or no reply; R reads back what the supply holds." : "Settings applied.");
    if (cfg->dt_enabled) printf("DT SET: the supply holds them until a GET (Utilities).\n");
    ps5010_show_out(address);
}

/* Regulation-change, user and trigger settings, sent by Apply */
static void ps5010_srq_menu(ps5010_config *cfg) {
    static const char * const __far desc[] = {
        "SRQ when the positive supply changes CV/CC/unregulated",
        "SRQ when the negative supply changes mode",
        "SRQ when the logic supply changes mode",
        "service requests at all (off: errors still queue for ERR?)",
        "SRQ when the INST ID button is pressed",
        "new settings wait for GET (DT SET)"
    };
    int c, i;

    for (;;) {
        clrscr();
        printf("PS5010 SRQ and trigger settings (sent by Apply)\n");
        printf("===============================================\n\n");
        for (i = 2; i < PS5010_NFLAGS; i++) {
            printf("%d. %-4s %-3s  %s\n", i - 1, ps5010_flag_cmd[i],
                   ps5010_flag_word(i, ps5010_get_flag(cfg, i)), desc[i - 2]);
        }
        printf("\nThe monitor reads REG? and does not service SRQs.\n");
        printf("1-6 toggles, 0 returns: ");
        c = getch();
        if (c == '0' || c == 27) return;
        i = c - '0' + 1;
        if (i >= 2 && i < PS5010_NFLAGS) ps5010_set_flag(cfg, i, !ps5010_get_flag(cfg, i));
    }
}

/* Settings block from LLSET? (p. 3-7), kept to send back with LLSET */
static unsigned char __far ps5010_ll[160];
static int ps5010_ll_len = 0, ps5010_ll_addr = -1;

/* '%', 2-byte count of data + checksum, data, checksum; count, data and
   checksum sum to 0 mod 256. Moves a valid block to the start: its length, or 0. */
static int ps5010_ll_take(unsigned char *r, int n) {
    unsigned char *p = memchr(r, '%', n);
    unsigned sum = 0;
    int i, cnt, room;

    if (p == NULL) return 0;
    /* Compare lengths, not far pointers: a garbage count would wrap the offset */
    room = n - (int)(p - r) - 3;
    if (room < 1) return 0;
    cnt = p[1] * 256 + p[2];
    if (cnt < 1 || cnt > room) return 0;
    for (i = 1; i < 3 + cnt; i++) sum += p[i];
    if (sum & 0xFF) return 0;
    memmove(r, p, 3 + cnt);
    return 3 + cnt;
}

static void ps5010_utilities(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    int choice, i, n;
    unsigned hp;

    for (;;) {
        clrscr();
        printf("PS5010 Utilities - GPIB %d\n", address);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. ROM self test (TEST)\n");
        printf("3. Error/event queue (ERR?)\n");
        printf("4. All settings (SET?)\n");
        printf("5. Send GET (executes settings held by DT SET)\n");
        printf("6. INIT - power-on settings: outputs OFF, 0 V\n");
        printf("7. Save the supply's settings (LLSET?)\n");
        printf("8. Restore saved settings (LLSET)%s\n", ps5010_ll_addr == address ? "" : " - nothing saved");
        printf("0. Return\n\nSelect option: ");
        choice = getch();
        printf("\n\n");
        if (choice == '0' || choice == 27) return;

        gpib_remote(address);
        GPIB_PACE(100);
        switch (choice) {
            case '1':
            case '4':
                printf("%s\n", ps5010_query(address, choice == '1' ? "ID?" : "SET?", ps5010_cmd,
                                            sizeof(ps5010_cmd)) ? ps5010_cmd : "No reply");
                break;
            case '2':
                if (!ps5010_query(address, "TEST", ps5010_cmd, sizeof(ps5010_cmd))) {
                    printf("No reply\n");
                    break;
                }
                /* 0, or the failing ROM's code from Table 3-2 (p. 3-22) */
                n = atoi(ps5010_cmd + strcspn(ps5010_cmd, "0123456789"));
                printf("%s -> ", ps5010_cmd);
                if (n == 0) printf("ROM checksums good\n");
                else if (n >= 392 && n <= 395) printf("%X000 ROM checksum error\n", 0xC + n - 392);
                else printf("ROM error %d\n", n);
                break;
            case '3':
                if (ps5010_show_errors(address) == 0) printf("No errors\n");
                break;
            case '5':
                sprintf(ps5010_cmd, "TRIGGER %d\r\n", address);
                ieee_write(ps5010_cmd);
                delay(50);   /* GET programming time up to 35 ms (p. 1-3, 1-4) */
                if (ps5010_show_errors(address) == 0) printf("GET sent\n");
                ps5010_show_out(address);
                break;
            case '6':
                if (!ps5010_confirm("INIT switches every output OFF and sets 0 V; go ahead")) break;
                gpib_write(address, "INIT");
                GPIB_PACE(100);
                hp = cfg->high_power;
                init_ps5010_config(slot);
                cfg->high_power = hp;
                ps5010_show_errors(address);
                printf("Supply and menu at the power-on settings (Table 3-3)\n");
                break;
            case '7':
                n = 0;
                if (gpib_write(address, "LLSET?") == 0) {
                    GPIB_PACE(20);
                    n = gpib_read(address, (char *)ps5010_ll, sizeof(ps5010_ll));
                }
                ps5010_ll_len = n > 0 ? ps5010_ll_take(ps5010_ll, n) : 0;
                ps5010_ll_addr = ps5010_ll_len ? address : -1;
                if (ps5010_ll_len) printf("Saved a %d-byte settings block\n", ps5010_ll_len);
                else printf("No intact block (count/checksum wrong; a CR or LF in the\nbinary data ends a text-mode GPIB read). Nothing saved.\n");
                break;
            case '8':
                if (ps5010_ll_addr != address) {
                    printf("Nothing saved for this address (option 7 first)\n");
                    break;
                }
                for (i = 0; i < ps5010_ll_len; i++) {
                    if (ps5010_ll[i] == 0 || ps5010_ll[i] == '\r' || ps5010_ll[i] == '\n') break;
                }
                /* gpib_write sends a NUL-terminated line, "output NN;" + command + CRLF */
                if (i < ps5010_ll_len || ps5010_ll_len + 6 + 14 > GPIB_BUFFER_SIZE) {
                    printf("The block is too long or holds bytes (NUL, CR, LF) that a\ntext-mode GPIB write can't carry; not sent\n");
                    break;
                }
                if (!ps5010_confirm("LLSET restores every saved setting, outputs ON included; go ahead")) break;
                strcpy(ps5010_cmd, "LLSET ");
                memcpy(ps5010_cmd + 6, ps5010_ll, ps5010_ll_len);
                ps5010_cmd[6 + ps5010_ll_len] = '\0';
                gpib_write(address, ps5010_cmd);
                delay(50);   /* settle, not bus pacing: outputs may switch, up to 35 ms (p. 1-3, 1-4) */
                if (ps5010_show_errors(address) == 0) printf("Settings restored\n");
                ps5010_read_back(slot);
                ps5010_show_out(address);
                break;
        }
        ps5010_pause();
    }
}

void configure_ps5010_advanced(int slot) {
    int address = g_system->modules[slot].gpib_address;
    ps5010_config *cfg = &g_ps5010_config[slot];
    int choice, i, on;
    float v, a;

    for (;;) {
        clrscr();
        printf("PS5010 Triple Power Supply - Slot %d, GPIB %d\n", slot, address);
        printf("=============================================\n");
        printf("Menu values reach the supply only with D, which asks before any output is ON.\n\n");
        printf("1. Positive  VPOS %5.2f V   IPOS %.2f A\n", cfg->voltage1, cfg->current_limit1);
        printf("2. Negative  VNEG %5.2f V   INEG %.2f A\n", cfg->voltage2, cfg->current_limit2);
        printf("3. Logic     VLOG %5.2f V   ILOG %.1f A\n", cfg->logic_voltage, cfg->logic_current_limit);
        printf("4. Floating outputs, + and - together (FSOUT): %s\n", cfg->fsout_enabled ? "ON" : "OFF");
        printf("5. Logic output (LSOUT): %s\n", cfg->logic_enabled ? "ON" : "OFF");
        printf("6. Compartment: %s (floating limit to %.2f A, %.2f A at 15 V and below)\n",
               cfg->high_power ? "high power" : "standard",
               ps5010_max_current(cfg->high_power, 32.0f), ps5010_max_current(cfg->high_power, 0.0f));
        printf("7. SRQ/trigger:");
        for (i = 2; i < PS5010_NFLAGS; i++)
            printf(" %s %s", ps5010_flag_cmd[i], ps5010_flag_word(i, ps5010_get_flag(cfg, i)));
        printf("\n\nD. Apply settings to the PS5010   R. Read settings back from the PS5010\n");
        printf("S. Status (REG?, OUT?, SET?)      T. Set both floating now (VTRA/ITRA)\n");
        printf("O. Switch an output now           X. ALL OUTPUTS OFF now (OUT OFF)\n");
        printf("U. Utilities (ID, TEST, errors, GET, INIT, LLSET)\n");
        printf("C. Communication test (read only) 0. Return\n");
        printf("\nSelect option: ");
        choice = toupper(getch());
        printf("\n\n");

        switch (choice) {
            case '1':
            case '2':
                {
                    float *vp = choice == '1' ? &cfg->voltage1 : &cfg->voltage2;
                    float *ap = choice == '1' ? &cfg->current_limit1 : &cfg->current_limit2;

                    printf("%s floating supply (resolution 0.01 V to 10 V, 0.1 V above; 0.05 A)\n",
                           choice == '1' ? "Positive" : "Negative (magnitude)");
                    v = *vp;
                    if (ps5010_ask("Voltage", 0.0f, 32.0f, &v)) *vp = ps5010_round(v, v <= 10.0f ? 0.01f : 0.1f);
                    a = *ap;
                    if (ps5010_ask("Current limit, A", 0.05f, ps5010_max_current(cfg->high_power, *vp), &a))
                        *ap = ps5010_round(a, 0.05f);
                    ps5010_fit_limits(cfg);
                }
                ps5010_pause();
                break;
            case '3':
                printf("Logic supply (resolution 0.01 V; 0.1 A)\n");
                v = cfg->logic_voltage;
                if (ps5010_ask("Voltage", 4.5f, 5.5f, &v)) cfg->logic_voltage = ps5010_round(v, 0.01f);
                a = cfg->logic_current_limit;
                if (ps5010_ask("Current limit, A", 0.1f, 3.0f, &a)) cfg->logic_current_limit = ps5010_round(a, 0.1f);
                ps5010_pause();
                break;
            case '4':
                cfg->fsout_enabled = !cfg->fsout_enabled;
                break;
            case '5':
                cfg->logic_enabled = !cfg->logic_enabled;
                break;
            case '6':
                cfg->high_power = !cfg->high_power;
                ps5010_fit_limits(cfg);
                ps5010_pause();
                break;
            case '7':
                ps5010_srq_menu(cfg);
                break;
            case 'D':
                ps5010_apply(slot);
                ps5010_pause();
                break;
            case 'R':
                ps5010_read_back(slot);
                ps5010_pause();
                break;
            case 'S':
                {
                    int neg_stat, pos_stat, log_stat;
                    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat))
                        printf("REG? positive %s, negative %s, logic %s (CV/CC/UR)\n", ps5010_reg_text(pos_stat),
                               ps5010_reg_text(neg_stat), ps5010_reg_text(log_stat));
                    else
                        printf("REG? no reply\n");
                    ps5010_show_out(address);
                    if (ps5010_query(address, "SET?", ps5010_cmd, sizeof(ps5010_cmd))) printf("SET? %s\n", ps5010_cmd);
                    printf("\nThe PS5010 can't measure its outputs; use a DMM for values.\n");
                }
                ps5010_pause();
                break;
            case 'T':
                printf("VTRA/ITRA set both floating supplies to one magnitude, now.\n");
                v = cfg->voltage1;
                a = cfg->current_limit1;
                if (!ps5010_ask("Voltage, both", 0.0f, 32.0f, &v)) {
                    ps5010_pause();
                    break;
                }
                v = ps5010_round(v, v <= 10.0f ? 0.01f : 0.1f);
                if (a > ps5010_max_current(cfg->high_power, v)) a = ps5010_max_current(cfg->high_power, v);
                if (!ps5010_ask("Current limit, both, A", 0.05f, ps5010_max_current(cfg->high_power, v), &a)) {
                    ps5010_pause();
                    break;
                }
                a = ps5010_round(a, 0.05f);
                gpib_remote(address);
                on = ps5010_query_on(address, "FSOUT?");
                printf("Send VTRA %.2f;ITRA %.2f.%s\n", v, a, on == 0 ? "" :
                       on == 1 ? " Floating outputs are ON: the terminals change now." :
                                 " FSOUT? didn't answer: the outputs may be ON.");
                if (!ps5010_confirm("Send")) break;
                sprintf(ps5010_cmd, "VTRA %.2f;ITRA %.2f", v, a);
                gpib_write(address, ps5010_cmd);
                if (ps5010_show_errors(address) == 0) {
                    cfg->voltage1 = cfg->voltage2 = v;
                    cfg->current_limit1 = cfg->current_limit2 = a;
                    printf("Both floating supplies set; menu updated\n");
                }
                ps5010_pause();
                break;
            case 'O':
                printf("F)loating (FSOUT), L)ogic (LSOUT) or A)ll (OUT)? ");
                choice = toupper(getch());
                if (choice != 'F' && choice != 'L' && choice != 'A') break;
                printf("%c   1 = ON, 0 = OFF: ", choice);
                on = getch();
                if (on != '0' && on != '1') break;
                on -= '0';
                printf("%s\n", on ? "ON" : "OFF");
                gpib_remote(address);
                GPIB_PACE(100);
                if (on) {
                    /* What the supply holds, which may not be the menu's values */
                    for (i = 0; i < 3; i++) {
                        static const char * const __far vq[] = { "VPOS?", "VNEG?", "VLOG?" };
                        if (ps5010_query_value(address, vq[i], &v)) printf("  %.4s %.2f V", vq[i], fabs(v));
                    }
                    printf("\n");
                    if (!ps5010_confirm("Turn ON at the supply's present settings")) break;
                }
                sprintf(ps5010_cmd, "%s %s", choice == 'F' ? "FSOUT" : choice == 'L' ? "LSOUT" : "OUT",
                        on ? "ON" : "OFF");
                gpib_write(address, ps5010_cmd);
                delay(50);   /* output on/off about 12 ms (p. 1-3) */
                if (ps5010_show_errors(address) == 0) {
                    if (choice != 'L') cfg->fsout_enabled = on;
                    if (choice != 'F') cfg->logic_enabled = on;
                }
                ps5010_show_out(address);
                ps5010_pause();
                break;
            case 'X':
                gpib_remote(address);
                gpib_write(address, "OUT OFF");
                delay(50);   /* output on/off about 12 ms (p. 1-3) */
                cfg->fsout_enabled = 0;
                cfg->logic_enabled = 0;
                ps5010_show_errors(address);
                ps5010_show_out(address);
                ps5010_pause();
                break;
            case 'U':
                ps5010_utilities(slot);
                break;
            case 'C':
                test_ps5010_comm(address);
                break;
            case '0':
            case 27:
                return;
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

