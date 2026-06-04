/*
 * TM5000 GPIB Control System - User Interface Module
 * Version 3.5
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Version update
 * 3.3 - Version update
 * 3.5 - Integrated enhanced file operations and math analysis menus
 */

#include "ui.h"
#include "graphics.h"
#include "module_funcs.h"
#include "math_functions.h"
#include "modules.h"
#include "config_profiles.h"
#include "data.h"

/* Main menu function */
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
        printf("          TM5000 CONTROL SYSTEM v%s\n", TM5000_VERSION);
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
        
        printf("  Memory: %lu KB  ", (unsigned long)(_memavl() / 1024));
        printf("Data: %u/%u", g_system->data_count, g_system->buffer_size);
        if (g_mouse.present) {
            printf("  Mouse: ON");
        }
        printf("\n");
        printf("  ============================================\n\n");
        
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
                    
                    if (g_mouse.y == 5) { choice = '1'; break; }   /* Configure Modules */
                    if (g_mouse.y == 7) { choice = '2'; break; }   /* Measurement Operations */
                    if (g_mouse.y == 9) { choice = '3'; break; }  /* Module Functions */
                    if (g_mouse.y == 11) { choice = '4'; break; }  /* File Operations */
                    if (g_mouse.y == 13) { choice = '0'; break; }  /* Exit */
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

/* Display error message */
void display_error(char *msg) {
    strcpy(g_error_msg, msg);
    gotoxy(1, 24);
    textattr(0x4F);  /* White on red background */
    printf("Error: %s", msg);
    textattr(0x07);  /* Reset to normal */
    clreol();
}

/* Wait for keyboard or mouse input */
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

/* Measurement operations menu */
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
        
        printf("  4. Enhanced Math Functions");
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
        fflush(stdout);  /* Ensure all output is flushed before showing mouse */
        
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
                    
                    if (g_mouse.y == 5) { choice = '1'; break; }   /* Single Measurement */
                    if (g_mouse.y == 7) { choice = '2'; break; }   /* Continuous Measurement */
                    if (g_mouse.y == 9) { choice = '3'; break; }  /* Graph Display */
                    if (g_mouse.y == 11) { choice = '4'; break; }  /* Math Functions */
                    if (g_mouse.y == 13) { choice = '0'; break; }  /* Return to Main Menu */
                }
                
                delay(10);
            }
            hide_mouse();
        } else {
            choice = getch();
        }
        
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

/* File operations menu */
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
        
        printf("  6. Configuration Profiles\n\n");
        
        printf("  7. Export Data\n\n");
        
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
                    
                    if (g_mouse.y == 3) { choice = '1'; break; }   /* Save Data */
                    if (g_mouse.y == 5) { choice = '2'; break; }   /* Load Data */
                    if (g_mouse.y == 7) { choice = '3'; break; }  /* Save Settings */
                    if (g_mouse.y == 9) { choice = '4'; break; }  /* Load Settings */
                    if (g_mouse.y == 11) { choice = '5'; break; }  /* Print Report */
                    if (g_mouse.y == 13) { choice = '6'; break; }  /* Configuration Profiles */
                    if (g_mouse.y == 15) { choice = '7'; break; }  /* Export Data*/
                    if (g_mouse.y == 17) { choice = '0'; break; }  /* Return */
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
                
            case '6':
                configuration_profiles_menu();
                break;
                
            case '7':
                enhanced_export_menu();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Configuration profiles management menu */
void configuration_profiles_menu(void) {
    int choice;
    int done = 0;
    profile_list_entry profiles[MAX_CONFIG_PROFILES];
    int profile_count;
    char profile_name[PROFILE_NAME_LENGTH];
    char profile_desc[PROFILE_DESC_LENGTH];
    int i, selected_index;
    
    while (!done) {
        clrscr();
        display_header("Configuration Profiles");
        
        /* List existing profiles */
        profile_count = list_config_profiles(profiles, MAX_CONFIG_PROFILES);
        
        printf("Current Profiles:\n");
        printf("-----------------\n");
        
        if (profile_count == 0) {
            printf("  No profiles saved\n");
        } else {
            for (i = 0; i < profile_count && i < 8; i++) {
                char date_str[20];
                strftime(date_str, sizeof(date_str), "%m/%d/%y", 
                        localtime(&profiles[i].modified));
                printf("%d. %-20s [%s]\n", i + 1, profiles[i].name, date_str);
            }
        }
        
        printf("\n");
        
        /* Show current profile status */
        if (strlen(g_current_profile_name) > 0) {
            printf("Current Profile: %s", g_current_profile_name);
            if (g_profile_modified) {
                printf(" [Modified]");
            }
            printf("\n\n");
        }
        
        display_menu_item('S', "Save Current Configuration", 1);
        display_menu_item('L', "Load Profile", profile_count > 0);
        display_menu_item('D', "Delete Profile", profile_count > 0);
        display_menu_item('N', "New Profile from Current", 1);
        display_menu_item(0, "Return", 1);
        
        display_footer("Select option: ");
        
        choice = toupper(getch());
        
        switch(choice) {
            case 'S':
                printf("\n\nProfile name (max 31 chars): ");
                if (get_string_input("", profile_name, PROFILE_NAME_LENGTH - 1)) {
                    printf("Description (optional): ");
                    get_string_input("", profile_desc, PROFILE_DESC_LENGTH - 1);
                    
                    if (save_config_profile(profile_name, profile_desc) == PROFILE_SUCCESS) {
                        display_success("Profile saved successfully");
                    } else {
                        display_error("Failed to save profile");
                    }
                }
                break;
                
            case 'L':
                if (profile_count > 0) {
                    printf("\n\nSelect profile (1-%d): ", profile_count);
                    selected_index = getch() - '1';
                    
                    if (selected_index >= 0 && selected_index < profile_count) {
                        if (load_config_profile(profiles[selected_index].name) == PROFILE_SUCCESS) {
                            display_success("Profile loaded successfully");
                        } else {
                            display_error("Failed to load profile");
                        }
                    }
                }
                break;
                
            case 'D':
                if (profile_count > 0) {
                    printf("\n\nSelect profile to delete (1-%d): ", profile_count);
                    selected_index = getch() - '1';
                    
                    if (selected_index >= 0 && selected_index < profile_count) {
                        if (get_yes_no("Delete this profile?")) {
                            if (delete_config_profile(profiles[selected_index].name) == PROFILE_SUCCESS) {
                                display_success("Profile deleted");
                            } else {
                                display_error("Failed to delete profile");
                            }
                        }
                    }
                }
                break;
                
            case 'N':
                printf("\n\nNew profile name: ");
                if (get_string_input("", profile_name, PROFILE_NAME_LENGTH - 1)) {
                    printf("Description: ");
                    get_string_input("", profile_desc, PROFILE_DESC_LENGTH - 1);
                    
                    if (save_config_profile(profile_name, profile_desc) == PROFILE_SUCCESS) {
                        display_success("New profile created");
                    } else {
                        display_error("Failed to create profile");
                    }
                }
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
        
        if (!done && choice != '0' && choice != 27) {
            printf("\nPress any key to continue...");
            getch();
        }
    }
}

/* Enhanced data export menu */
void enhanced_export_menu(void) {
    int choice;
    int done = 0;
    export_config config;
    char filename[80];
    int enabled_count = 0;
    int i;
    
    /* Initialize export configuration with defaults */
    config.format = EXPORT_FORMAT_CSV;
    config.flags = EXPORT_FLAG_HEADERS | EXPORT_FLAG_METADATA;
    config.delimiter = ',';
    config.precision = 6;
    config.sample_rate_override = 0.0;
    config.custom_header[0] = '\0';
    config.units_override[0] = '\0';
    config.export_start_time = time(NULL);
    strcpy(config.filename_template, "MEASUREMENT_%Y%M%D_%H%N%S.CSV");
    
    /* Count enabled modules */
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && g_system->modules[i].module_data_count > 0) {
            enabled_count++;
        }
    }
    
    while (!done) {
        clrscr();
        display_header("Enhanced Data Export");
        
        printf("Current Export Settings:\n");
        printf("-----------------------\n");
        printf("Format: ");
        switch (config.format) {
            case EXPORT_FORMAT_CSV: printf("CSV\n"); break;
            case EXPORT_FORMAT_TSV: printf("TSV\n"); break;
            case EXPORT_FORMAT_SCIENTIFIC: printf("Scientific CSV\n"); break;
            default: printf("Custom\n"); break;
        }
        
        printf("Options: ");
        if (config.flags & EXPORT_FLAG_METADATA) printf("[Metadata] ");
        if (config.flags & EXPORT_FLAG_TIMESTAMPS) printf("[Timestamps] ");
        if (config.flags & EXPORT_FLAG_SETTINGS) printf("[Settings] ");
        if (config.flags & EXPORT_FLAG_SCIENTIFIC) printf("[Scientific] ");
        if (config.flags & EXPORT_FLAG_COMPRESS) printf("[Compress] ");
        printf("\n");
        
        printf("Precision: %d decimal places\n", config.precision);
        printf("Template: %s\n\n", config.filename_template);
        
        if (enabled_count == 0) {
            printf("*** No data available to export ***\n\n");
        } else {
            printf("Ready to export %d module(s)\n\n", enabled_count);
        }
        
        display_menu_item(1, "Change Format", 1);
        display_menu_item(2, "Toggle Metadata", 1);
        display_menu_item(3, "Toggle Timestamps", 1);
        display_menu_item(4, "Toggle Scientific Notation", 1);
        display_menu_item(5, "Set Precision", 1);
        display_menu_item(6, "Export Now", enabled_count > 0);
        display_menu_item(0, "Return", 1);
        
        display_footer("Select option: ");
        
        choice = getch();
        
        switch(choice) {
            case '1':
                config.format = (config.format + 1) % 3;
                if (config.format == EXPORT_FORMAT_TSV) {
                    config.delimiter = '\t';
                } else {
                    config.delimiter = ',';
                }
                break;
                
            case '2':
                config.flags ^= EXPORT_FLAG_METADATA;
                break;
                
            case '3':
                config.flags ^= EXPORT_FLAG_TIMESTAMPS;
                break;
                
            case '4':
                config.flags ^= EXPORT_FLAG_SCIENTIFIC;
                break;
                
            case '5':
                printf("\n\nEnter precision (1-9): ");
                {
                    int new_precision = getch() - '0';
                    if (new_precision >= 1 && new_precision <= 9) {
                        config.precision = new_precision;
                    }
                }
                break;
                
            case '6':
                if (enabled_count > 0) {
                    printf("\n\nExporting data...\n");
                    
                    /* Generate filename from template */
                    generate_filename_from_template(filename, sizeof(filename), 
                                                  config.filename_template, time(NULL));
                    
                    if (export_data_enhanced(filename, &config) == EXPORT_SUCCESS) {
                        printf("Export successful: %s\n", filename);
                        printf("File size: %d bytes\n", get_export_file_size(filename));
                    } else {
                        display_error("Export failed");
                    }
                    printf("\nPress any key to continue...");
                    getch();
                }
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Continuous monitoring setup menu */
void continuous_monitor_setup(void) {
    int choice;
    int done = 0;
    int i;
    
    while (!done) {
        clrscr();
        printf("Continuous Monitoring Setup\n");
        printf("===========================\n\n");
        
        printf("Current Settings:\n");
        printf("  Sample Rate: %d ms (%s)\n", 
               g_control_panel.sample_rate_ms,
               g_control_panel.use_custom ? "custom" : "preset");
        printf("  Monitoring: %s\n", 
               g_control_panel.monitor_all ? "All modules" : "Selected modules");
        
        if (!g_control_panel.monitor_all) {
            int count = 0;
            printf("  Selected modules: ");
            for (i = 0; i < 10; i++) {
                if (g_control_panel.monitor_mask & (1 << i)) {
                    if (count > 0) printf(", ");
                    printf("S%d", i);
                    count++;
                }
            }
            if (count == 0) printf("None");
            printf("\n");
        }
        
        printf("\nOptions:\n");
        printf("1. Set Sample Rate\n");
        printf("2. Select Modules to Monitor\n");
        printf("3. Start Monitoring\n");
        printf("0. Return to Menu\n\n");
        printf("Choice: ");
        
        choice = getch();
        
        switch(choice) {
            case '1':
                sample_rate_menu();
                break;
                
            case '2':
                module_selection_menu();
                break;
                
            case '3':
                continuous_monitor();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Sample rate configuration menu */
void sample_rate_menu(void) {
    int choice;
    int custom_rate;
    int i;
    
    clrscr();
    printf("Sample Rate Configuration\n");
    printf("=========================\n\n");
    
    printf("Preset Rates:\n");
    for (i = 0; i < NUM_RATE_PRESETS; i++) {
        printf("%d. %s\n", i + 1, (char far *)sample_rate_labels[i]);
    }
    printf("8. Custom Rate\n");
    printf("0. Cancel\n\n");
    
    printf("Current: %d ms\n\n", g_control_panel.sample_rate_ms);
    printf("Choice: ");
    
    choice = getch();
    
    if (choice >= '1' && choice <= '7') {
        i = choice - '1';
        g_control_panel.sample_rate_ms = ((int far *)sample_rate_presets)[i];
        g_control_panel.selected_rate = i;
        g_control_panel.use_custom = 0;
        printf("\nSample rate set to %s\n", (char far *)sample_rate_labels[i]);
        printf("Press any key...");
        getch();
    } else if (choice == '8') {
        printf("\nEnter custom rate (10-60000 ms): ");
        scanf("%d", &custom_rate);
        if (custom_rate >= 10 && custom_rate <= 60000) {
            g_control_panel.sample_rate_ms = custom_rate;
            g_control_panel.use_custom = 1;
            sprintf(g_control_panel.custom_rate, "%d", custom_rate);
            printf("Custom rate set to %d ms\n", custom_rate);
        } else {
            printf("Invalid rate! Must be 10-60000 ms\n");
        }
        printf("Press any key...");
        getch();
    }
}

/* FG5010 Pulse Program Menu (stub) */
void fg5010_pulse_program_menu(int slot) {
    clrscr();
    printf("FG5010 Pulse Program Menu - Slot %d\n", slot);
    printf("===================================\n\n");
    printf("This feature is not yet implemented.\n\n");
    printf("Press any key to return...");
    getch();
}


/* Math functions menu */
void math_functions_menu(void) {
    int choice;
    int done = 0;
    int i;
    int has_data = 0;
    
    while (!done) {
        clrscr();
        printf("Math Functions\n");
        printf("==============\n\n");
        
        if (g_has_287) {
            printf("287 Coprocessor: ENABLED - Fast calculations\n\n");
        } else {
            printf("287 Coprocessor: NOT FOUND - Software mode\n\n");
        }
        
        printf("Available data:\n");
        printf("---------------\n");
        has_data = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && 
                g_system->modules[i].module_data && 
                g_system->modules[i].module_data_count > 0) {
                printf("Slot %d: %s - %u samples\n", 
                       i, g_system->modules[i].description,
                       g_system->modules[i].module_data_count);
                has_data = 1;
            }
        }
        
        if (!has_data) {
            printf("No data available. Run measurements first.\n");
        }
        
        printf("\nFunctions:\n");
        printf("----------\n\n");
        printf("1. FFT - Frequency Analysis\n");
        printf("2. Statistics (Min/Max/Avg/StdDev)\n");
        printf("3. Differentiation (dV/dt)\n");
        printf("4. Integration\n");
        printf("5. Smoothing Filter\n");
        printf("6. Dual-Trace Operations\n");
        printf("7. Digital Filtering\n");
        printf("8. Curve Fitting\n");
        printf("9. Correlation Analysis\n");
        printf("0. Return to Menu\n\n");
        
        printf("Choice: ");
        
        choice = getch();
        
        if (!has_data && choice >= '1' && choice <= '9') {
            printf("\n\nNo data to process!");
            printf("\nPress any key...");
            getch();
            continue;
        }
        
        switch(choice) {
            case '1':
                perform_fft();
                break;
                
            case '2':
                calculate_statistics();
                break;
                
            case '3':
                perform_differentiation();
                break;
                
            case '4':
                perform_integration();
                break;
                
            case '5':
                perform_smoothing();
                break;
                
            case '6':
                dual_trace_operations_menu();
                break;
                
            case '7':
                digital_filter_menu();
                break;
                
            case '8':
                curve_fitting_menu();
                break;
                
            case '9':
                correlation_analysis_menu();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Calculate enhanced statistics for measurement data */
void calculate_statistics(void) {
    int slot;
    statistics_result result;
    float far *data;
    int count;
    char *unit_str;
    float scale_factor;
    int decimal_places;
    
    clrscr();
    printf("\n\nEnhanced Statistics Calculation\n");
    printf("===============================\n\n");
    
    printf("Select source slot (0-9): ");
    scanf("%d", &slot);
    
    if (slot < 0 || slot > 9 || !g_system->modules[slot].enabled ||
        !g_system->modules[slot].module_data || 
        g_system->modules[slot].module_data_count == 0) {
        printf("\nInvalid slot or no data!\n");
        printf("Press any key...");
        getch();
        return;
    }
    
    data = g_system->modules[slot].module_data;
    count = g_system->modules[slot].module_data_count;
    
    printf("\nCalculating enhanced statistics for %d samples...\n", count);
    printf("Module: %s\n\n", g_system->modules[slot].description);
    
    /* Use enhanced statistics calculation from math_enhanced.c */
    if (calculate_basic_statistics(data, count, &result) == MATH_SUCCESS) {
        /* Determine appropriate engineering units and scaling */
        float range = result.max_value - result.min_value;
        scale_factor = get_engineering_scale(range, NULL, &unit_str, &decimal_places);
        
        printf("Statistical Results:\n");
        printf("--------------------\n");
        printf("Sample count:    %d\n", result.sample_count);
        printf("Mean:           %.*f %s\n", decimal_places, result.mean * scale_factor, unit_str);
        printf("RMS:            %.*f %s\n", decimal_places, result.rms * scale_factor, unit_str);
        printf("Standard dev:   %.*f %s\n", decimal_places, result.std_dev * scale_factor, unit_str);
        printf("Minimum:        %.*f %s\n", decimal_places, result.min_value * scale_factor, unit_str);
        printf("Maximum:        %.*f %s\n", decimal_places, result.max_value * scale_factor, unit_str);
        printf("Peak-to-peak:   %.*f %s\n", decimal_places, result.peak_to_peak * scale_factor, unit_str);
        printf("Median:         %.*f %s\n", decimal_places, result.median * scale_factor, unit_str);
        
        /* Additional analysis */
        printf("\nSignal Analysis:\n");
        printf("----------------\n");
        
        if (g_has_287) {
            /* Calculate signal characteristics */
            float snr_estimate = 0.0;
            if (result.std_dev > 0.0) {
                snr_estimate = 20.0 * log10(fabs(result.mean) / result.std_dev);
            }
            
            printf("SNR estimate:   %.1f dB\n", snr_estimate);
            
            /* Signal quality assessment */
            if (result.std_dev / fabs(result.mean) < 0.01) {
                printf("Signal quality: Excellent (< 1%% noise)\n");
            } else if (result.std_dev / fabs(result.mean) < 0.05) {
                printf("Signal quality: Good (< 5%% noise)\n");
            } else if (result.std_dev / fabs(result.mean) < 0.10) {
                printf("Signal quality: Fair (< 10%% noise)\n");
            } else {
                printf("Signal quality: Poor (> 10%% noise)\n");
            }
        } else {
            /* Simple software mode analysis */
            printf("Signal range:   %.*f %s\n", decimal_places, result.peak_to_peak * scale_factor, unit_str);
            printf("DC component:   %.*f %s\n", decimal_places, result.mean * scale_factor, unit_str);
        }
        
        /* Data quality indicators */
        printf("\nData Quality:\n");
        printf("-------------\n");
        if (count >= 1000) {
            printf("Sample size:    Excellent (%d samples)\n", count);
        } else if (count >= 100) {
            printf("Sample size:    Good (%d samples)\n", count);
        } else if (count >= 10) {
            printf("Sample size:    Fair (%d samples)\n", count);
        } else {
            printf("Sample size:    Poor (%d samples)\n", count);
        }
        
        /* Check for potential clipping */
        if (g_system->modules[slot].module_type == MOD_DM5120) {
            /* DM5120 specific range checking */
            if (fabs(result.max_value) > 10.0 || fabs(result.min_value) > 10.0) {
                printf("Range warning:  Signal may be clipped\n");
            } else {
                printf("Range status:   Normal\n");
            }
        }
        
    } else {
        printf("Error: Unable to calculate statistics\n");
    }
    
    printf("\nPress any key to continue...");
    getch();
}

/* Data analysis menu stub */
void data_analysis_menu(void) {
    printf("\n\nData analysis functions not yet implemented.\n");
    printf("Press any key to continue...");
    getch();
}

/* Unit conversion menu stub */
void unit_conversion_menu(void) {
    printf("\n\nUnit conversion functions not yet implemented.\n");
    printf("Press any key to continue...");
    getch();
}

/* Calculator menu stub */
void calculator_menu(void) {
    printf("\n\nCalculator functions not yet implemented.\n");
    printf("Press any key to continue...");
    getch();
}

/* Module selection menu for monitoring */
void module_selection_menu(void) {
    int done = 0;
    int i, key;
    char status;
    int selected_count = 0;
    
    while (!done) {
        clrscr();
        printf("\n\nModule Selection for Monitoring\n");
        printf("===============================\n\n");
        
        printf("Configure Module Monitoring\n");
        printf("---------------------------\n\n");
        
        printf("Available Modules:\n");
        for (i = 0; i < 10; i++) {
            /* Only show valid modules - not ghost/phantom modules */
            if (g_system->modules[i].enabled &&
                g_system->modules[i].module_type != MOD_NONE &&
                g_system->modules[i].gpib_address >= 1 &&
                g_system->modules[i].gpib_address <= 30 &&
                strlen(g_system->modules[i].description) > 0) {
                status = (g_control_panel.monitor_mask & (1 << i)) ? '*' : ' ';
                printf("  %d. [%c] Slot %d: %-20s (GPIB %d)\n", 
                       i, status, i, 
                       g_system->modules[i].description,
                       g_system->modules[i].gpib_address);
            }
        }
        
        printf("\nMonitoring Status:\n");
        printf("------------------\n");
        
        selected_count = 0;
        for (i = 0; i < 10; i++) {
            /* Only count valid modules - not ghost/phantom modules */
            if (g_system->modules[i].enabled &&
                g_system->modules[i].module_type != MOD_NONE &&
                g_system->modules[i].gpib_address >= 1 &&
                g_system->modules[i].gpib_address <= 30 &&
                strlen(g_system->modules[i].description) > 0 &&
                (g_control_panel.monitor_mask & (1 << i))) {
                selected_count++;
            }
        }
        
        if (g_control_panel.monitor_all) {
            printf("  Mode: Monitor ALL enabled modules\n");
        } else if (selected_count > 0) {
            printf("  Mode: Selective monitoring (%d modules selected)\n", selected_count);
        } else {
            printf("  Mode: No modules selected\n");
        }
        
        printf("\nCommands:\n");
        printf("---------\n");
        printf("  0-9  : Toggle module selection\n");
        printf("  A    : Select ALL modules\n");
        printf("  N    : Select NONE\n");
        printf("  ESC  : Save and return\n\n");
        
        printf("Select option: ");
        
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

/* Module settings submenu */
void module_settings_menu(void) {
    int choice;
    int done = 0;
    int i, found;
    int slot;
    
    while (!done) {
        clrscr();
        printf("Module Settings\n");
        printf("===============\n\n");
        
        printf("Advanced Module Configuration:\n");
        printf("------------------------------\n\n");
        printf("1. DM5120 Advanced Configuration\n\n");
        printf("2. PS5004 Advanced Configuration\n\n");
        printf("3. PS5010 Advanced Configuration\n\n");
        printf("4. FG5010 Advanced Configuration\n\n");
        printf("0. Return to Module Functions Menu\n");
        
        printf("\nEnter choice: ");
        choice = getch();
        
        switch(choice) {
            case '1':
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
                    getch();
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
                    getch();
                }
                break;
                
            case '2':
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
                    getch();
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
                    getch();
                }
                break;
                
            case '3':
                found = 0;
                clrscr();
                printf("PS5010 Power Supply Modules:\n");
                printf("============================\n\n");
                
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
                    getch();
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
                    getch();
                }
                break;
                
            case '4':
                found = 0;
                clrscr();
                printf("FG5010 Function Generator Modules:\n");
                printf("==================================\n\n");
                
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && 
                        g_system->modules[i].module_type == MOD_FG5010) {
                        printf("Slot %d: FG5010 (GPIB %d)\n", 
                               i, g_system->modules[i].gpib_address);
                        found = 1;
                    }
                }
                
                if (!found) {
                    printf("No FG5010 modules configured.\n");
                    printf("\nPress any key to continue...");
                    getch();
                    break;
                }
                
                printf("\nEnter slot number: ");
                scanf("%d", &slot);
                
                if (slot >= 0 && slot < 10 && 
                    g_system->modules[slot].enabled &&
                    g_system->modules[slot].module_type == MOD_FG5010) {
                    
                    printf("\nFG5010 Configuration Options:\n");
                    printf("1. Advanced Configuration\n");
                    printf("2. Pulse Program Menu\n");
                    printf("Choice: ");
                    scanf("%d", &choice);
                    
                    if (choice == 1) {
                        configure_fg5010_advanced(slot);
                    } else if (choice == 2) {
                        fg5010_pulse_program_menu(slot);
                    }
                } else {
                    printf("Invalid slot or not a FG5010.\n");
                    printf("\nPress any key to continue...");
                    getch();
                }
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Module functions menu */
void module_functions_menu(void) {
    int choice;
    int done = 0;
    int i, found;
    int slot;
    
    while (!done) {
        clrscr();
        printf("Module Functions Menu\n");
        printf("====================\n\n");
        
        printf("Configured Modules:\n");
        printf("------------------\n");
        found = 0;
        
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
            }
        }
        if (!found) {
            printf("  No modules configured\n");
        }
        
        printf("\nFunctions:\n");
        printf("----------\n\n");
        printf("1. Send Custom GPIB Command\n\n");
        printf("2. Module Settings\n\n");
        printf("3. Test Module Communication\n\n");
        printf("4. GPIB Terminal Mode\n\n");
        printf("0. Return to Main Menu\n");
        
        printf("\nEnter choice: ");
        choice = getch();
        
        switch(choice) {
            case '1':
                send_custom_command();
                break;
                
            case '2':
                module_settings_menu();
                break;
                
            case '3':
                printf("\n\nEnter GPIB address to test (1-30): ");
                scanf("%d", &i);
                
                if (i < 1 || i > 30) {
                    printf("Invalid address! Must be 1-30.\n");
                    printf("Press any key to continue...");
                    getch();
                    break;
                }
                
                printf("\nSelect test type:\n");
                printf("1. DM5120 communication test\n");
                printf("2. PS5004 communication test\n");
                printf("3. PS5010 communication test\n");
                printf("4. FG5010 communication test\n");
                printf("5. Generic GPIB test\n");
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
                        test_fg5010_comm(i);
                        break;
                    case '5':
                        gpib_terminal_mode();
                        break;
                }
                break;
                
            case '4':
                gpib_terminal_mode();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
    }
}

/* Graph display function - complete implementation from TM5000L.c */
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
    int current_sample;      
    int slot;                
    float center, range, shift;  
    float x_scale;           
    int x_pos1, x_pos2, y_pos1, y_pos2;  
    int old_cursor_x = -1;   
    char *unit_str;          
    float sample_to_x_scale; 
    int keyboard_mode = 1;   
    float y_scale;           
    float normalized_y;      
    int readout_needs_update = 1;  
    int last_readout_x = -1;       
    int last_readout_y = -1;
    int last_readout_width = 0;
    int last_sample_number = -1;  /* Track sample changes for immediate refresh */
    float mouse_graph_position;        /* Mouse position normalized to graph area */
    int shift_pressed = 0;   
    int is_fft_trace[10];     
    int is_derivative_trace[10];  /* Track derivative traces */
    float freq_scale;        
    char *freq_unit_str;     
    float freq_multiplier;   
    int should_monitor;      
    char *display_unit_str;      
    float display_scale_factor;  
    int display_decimal_places;  
    float y_range;
    float max_abs_value;
    float per_div_value;
    static int zoom_operations_count = 0;  /* Count zoom operations for periodic cleaning */
    /* Enhanced cursor mode state variables */
    int fine_cursor_mode = 0;    /* 0=coarse, 1=fine */
    float current_frequency = 0.0;  /* For FFT frequency-based navigation */
    int cursor_mode_changed = 0;    /* Flag to update cursor mode display */
    
    for (i = 0; i < 10; i++) {
        is_fft_trace[i] = 0;
        if (g_system->modules[i].enabled && 
            strcmp(g_system->modules[i].description, "FFT Result") == 0) {
            is_fft_trace[i] = 1;
        }
    }
    
    /* Detect derivative traces similar to FFT */
    for (i = 0; i < 10; i++) {
        is_derivative_trace[i] = 0;
        if (g_system->modules[i].enabled && 
            strcmp(g_system->modules[i].description, "Derivative") == 0) {
            is_derivative_trace[i] = 1;
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
        /* First try slot 0 if it has data */
        if (g_traces[0].enabled && g_traces[0].data_count > 0) {
            selected_trace = 0;
        } else {
            /* Otherwise find first active slot with data */
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                    selected_trace = i;
                    break;
                }
            }
        }
    }
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled) {
            active_traces++;
        }
    }
    
    if (selected_trace >= 0 && selected_trace < 10 && g_traces[selected_trace].enabled && g_traces[selected_trace].data_count > 1) {
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
        current_sample = 0;
        sample_to_x_scale = 1.0;  /* Default scale */
    }
    
    init_graphics();
    
    if (g_graph_scale.auto_scale) {
        auto_scale_graph();
    }
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    display_scale_factor = get_engineering_scale(y_range, &per_div_value, &display_unit_str, &display_decimal_places);
    
    if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
        float sample_rate = (g_control_panel.sample_rate_ms > 0) ? (1000.0 / g_control_panel.sample_rate_ms) : 1000.0;
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
    
    /* Initialize cursor to be visible for immediate user feedback */
    cursor_visible = 1;
    readout_needs_update = 1;
    
    if (g_has_287) {
        float y_range_check = g_graph_scale.max_value - g_graph_scale.min_value;
        if (y_range_check == 0.0) y_range_check = 1.0;  /* Prevent divide by zero */
        y_scale = (float)GRAPH_HEIGHT / y_range_check;
    }
    
    while (!done) {
        if (need_redraw) {
            _fmemset(video_mem, 0, 16384);
            
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                draw_frequency_grid(g_traces[selected_trace].data_count, selected_trace);
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
                    
                    if (effective_samples > 1) {
                        if (g_has_287) {
                            x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                        } else {
                            x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                        }
                    } else {
                        x_scale = (float)GRAPH_WIDTH;  /* Prevent divide by zero */
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
            
            draw_legend_enhanced(is_fft_trace, selected_trace);
            
            draw_gradient_rect(0, 185, SCREEN_WIDTH-1, 199, 1, 2);
            
            draw_text(2, 186, "A:AUTO +/-:ZOOM F:FINE/COARSE H:HELP", 3);
            draw_text(2, 193, "0-9:SELECT ALT+#:TOGGLE ESC:EXIT", 3);
            
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                float sample_rate = (g_control_panel.sample_rate_ms > 0) ? (1000.0 / g_control_panel.sample_rate_ms) : 1000.0;
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
            draw_text(240, 186, readout, 3);
            
            if (g_has_287) {
                draw_text(240, 193, "287", 3);
            }
            
            if (selected_trace >= 0 && g_traces[selected_trace].enabled && 
                g_traces[selected_trace].data_count >= 1000) {
                draw_text(255, 193, "1K", 3);
            }
            
            if (g_graph_scale.sample_count > 0) {
                char range_str[20];
                sprintf(range_str, "R:%d-%d", 
                        g_graph_scale.sample_start + 1, 
                        g_graph_scale.sample_start + g_graph_scale.sample_count);
                draw_text(270, 193, range_str, 3);
            }
            
            if (selected_trace >= 0 && g_system->modules[selected_trace].module_type == MOD_DM5120) {
                dm5120_config *cfg = &g_dm5120_config[selected_trace];
                if (cfg->buffer_enabled && cfg->burst_mode) {
                    sprintf(readout, "BUF@%.0fHz", cfg->sample_rate);
                    draw_text(280, 193, readout, 3);
                }
            }
            
            /* Show cursor mode indicator */
            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                char mode_str[10];
                if (is_fft_trace[selected_trace]) {
                    /* FFT trace: show frequency resolution */
                    if (fine_cursor_mode) {
                        strcpy(mode_str, "0.01Hz");
                    } else {
                        strcpy(mode_str, "0.1Hz");
                    }
                } else {
                    /* Voltage trace: show sample resolution */
                    if (fine_cursor_mode) {
                        strcpy(mode_str, "1smp");
                    } else {
                        strcpy(mode_str, "10smp");
                    }
                }
                draw_text(290, 186, mode_str, fine_cursor_mode ? 3 : 2);  /* Bright when fine, dim when coarse */
            }
            
            need_redraw = 0;
        }
        
        if (cursor_x != old_cursor_x && cursor_visible) {
            need_redraw = 1;  /* Trigger full redraw to clear old cursor */
            old_cursor_x = cursor_x;
            readout_needs_update = 1;
            continue;  /* Skip to redraw immediately */
        }
        
        if (cursor_visible && cursor_x >= GRAPH_LEFT && cursor_x <= GRAPH_RIGHT && 
            readout_needs_update) {
            
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
                            effective_rate = (g_control_panel.sample_rate_ms > 0) ? (cfg->sample_rate / (1000.0 / g_control_panel.sample_rate_ms)) : cfg->sample_rate;
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
                
                /* Bounds check for all data access */
                if (sample_num >= 0 && sample_num < g_traces[selected_trace].data_count) {
                    value = g_traces[selected_trace].data[sample_num];
                } else {
                    value = 0.0;  /* Safe fallback */
                }
                
                /* Simple readout based on trace type */
                if (g_traces[selected_trace].unit_type == UNIT_DB) {
                    /* FFT trace - show dB value and frequency */
                    if (g_traces[selected_trace].x_scale != 0.0 && 
                        g_traces[selected_trace].x_scale == g_traces[selected_trace].x_scale) { /* NaN check */
                        /* Calculate frequency using x_scale and x_offset or current_frequency if available */
                        float freq;
                        if (keyboard_mode && current_frequency > 0.0) {
                            /* Use current_frequency from keyboard navigation for precise readout */
                            freq = current_frequency;
                        } else {
                            /* Calculate frequency using x_scale and x_offset */
                            freq = sample_num * g_traces[selected_trace].x_scale + g_traces[selected_trace].x_offset;
                        }
                        
                        /* Enhanced precision based on cursor mode */
                        if (fabs(freq) >= 1e6) {
                            sprintf(readout, "S%d[%.1fMHZ]:%.1fDB", selected_trace, freq / 1e6, value);
                        } else if (fabs(freq) >= 1e3) {
                            sprintf(readout, "S%d[%.1fKHZ]:%.1fDB", selected_trace, freq / 1e3, value);
                        } else {
                            /* Fine mode: show 0.01Hz precision, coarse mode: show 0.1Hz */
                            if (fine_cursor_mode) {
                                sprintf(readout, "S%d[%.2fHZ]:%.1fDB", selected_trace, freq, value);
                            } else {
                                sprintf(readout, "S%d[%.1fHZ]:%.1fDB", selected_trace, freq, value);
                            }
                        }
                    } else {
                        /* Invalid x_scale - show sample number instead */
                        sprintf(readout, "S%d[%d]:%.1fDB", selected_trace, sample_num, value);
                    }
                } else if (g_traces[selected_trace].unit_type == UNIT_FREQUENCY) {
                    /* Counter trace - show frequency value */
                    if (value >= 1e6) {
                        sprintf(readout, "S%d[%d]:%.3fMHZ", selected_trace, sample_num, value / 1e6);
                    } else if (value >= 1e3) {
                        sprintf(readout, "S%d[%d]:%.1fKHZ", selected_trace, sample_num, value / 1e3);
                    } else {
                        sprintf(readout, "S%d[%d]:%.0fHZ", selected_trace, sample_num, value);
                    }
                } else if (g_traces[selected_trace].unit_type == UNIT_DERIVATIVE) {
                    /* Derivative trace - show V/s value with appropriate scaling */
                    if (fabs(value) < 0.001) {
                        sprintf(readout, "S%d[%d]:%.1fUV/S", selected_trace, sample_num, value * 1000000.0);
                    } else if (fabs(value) < 1.0) {
                        sprintf(readout, "S%d[%d]:%.1fMV/S", selected_trace, sample_num, value * 1000.0);
                    } else {
                        sprintf(readout, "S%d[%d]:%.2fV/S", selected_trace, sample_num, value);
                    }
                } else if (g_traces[selected_trace].unit_type == UNIT_CURRENT) {
                    /* Current trace - show A/mA/µA value with appropriate scaling */
                    if (fabs(value) < 0.001) {
                        sprintf(readout, "S%d[%d]:%.1fUA", selected_trace, sample_num, value * 1000000.0);
                    } else if (fabs(value) < 1.0) {
                        sprintf(readout, "S%d[%d]:%.1fMA", selected_trace, sample_num, value * 1000.0);
                    } else {
                        sprintf(readout, "S%d[%d]:%.2fA", selected_trace, sample_num, value);
                    }
                } else if (g_traces[selected_trace].unit_type == UNIT_RESISTANCE) {
                    /* Resistance trace - show Ω/mΩ/µΩ value with appropriate scaling */
                    if (fabs(value) < 0.001) {
                        sprintf(readout, "S%d[%d]:%.1fUO", selected_trace, sample_num, value * 1000000.0);
                    } else if (fabs(value) < 1.0) {
                        sprintf(readout, "S%d[%d]:%.1fMO", selected_trace, sample_num, value * 1000.0);
                    } else {
                        sprintf(readout, "S%d[%d]:%.2fO", selected_trace, sample_num, value);
                    }
                } else {
                    /* Voltage trace - use current graph scale units */
                    float current_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    if (current_range < 0.001) {
                        sprintf(readout, "S%d[%d]:%.0f", selected_trace, sample_num, value * 1000000.0);
                    } else if (current_range < 1.0) {
                        sprintf(readout, "S%d[%d]:%.1f", selected_trace, sample_num, value * 1000.0);
                    } else {
                        sprintf(readout, "S%d[%d]:%.4f", selected_trace, sample_num, value);
                    }
                }
                
                {
                    int readout_x = cursor_x - 30;
                    int readout_y = GRAPH_TOP + 20;
                    int readout_width = strlen(readout) * 6;  /* CGA character width */
                    
                    if (readout_x < GRAPH_LEFT) readout_x = GRAPH_LEFT;
                    if (readout_x > GRAPH_RIGHT - 60) readout_x = GRAPH_RIGHT - 60;
                    
                    /* Clear old readout if sample number has changed */
                    if (last_sample_number >= 0 && last_sample_number != sample_num) {
                        
                        /* Generous clearing area for CGA limitations */
                        int clear_x1 = last_readout_x - 5;
                        int clear_x2 = last_readout_x + last_readout_width + 5;
                        int clear_y1 = last_readout_y - 2;
                        int clear_y2 = last_readout_y + 10;
                        
                        /* Ensure clearing bounds stay within graph area */
                        if (clear_x1 < GRAPH_LEFT) clear_x1 = GRAPH_LEFT;
                        if (clear_x2 > GRAPH_RIGHT) clear_x2 = GRAPH_RIGHT;
                        if (clear_y1 < GRAPH_TOP) clear_y1 = GRAPH_TOP;
                        if (clear_y2 > GRAPH_BOTTOM) clear_y2 = GRAPH_BOTTOM;
                        
                        /* Clear the generous cursor box area */
                        fill_rectangle(clear_x1, clear_y1, clear_x2, clear_y2, 0);
                        
                        /* Redraw traces that intersect the cleared area */
                        for (i = 0; i < 10; i++) {
                            if (g_traces[i].enabled && g_traces[i].data_count > 1) {
                                int trace_x_start = clear_x1 - GRAPH_LEFT;
                                int trace_x_end = clear_x2 - GRAPH_LEFT;
                                
                                if (trace_x_start >= 0 && trace_x_start < GRAPH_WIDTH) {
                                    /* Calculate sample range for the cleared area */
                                    float x_norm_start = (float)trace_x_start / (float)GRAPH_WIDTH;
                                    float x_norm_end = (float)trace_x_end / (float)GRAPH_WIDTH;
                                    
                                    int sample_start = (int)(x_norm_start * (g_traces[i].data_count - 1));
                                    int sample_end = (int)(x_norm_end * (g_traces[i].data_count - 1)) + 1;
                                    
                                    if (sample_start < 0) sample_start = 0;
                                    if (sample_end >= g_traces[i].data_count) sample_end = g_traces[i].data_count - 1;
                                    
                                    /* Redraw trace segments in cleared area */
                                    for (j = sample_start; j < sample_end && j < g_traces[i].data_count - 1; j++) {
                                        float x_scale_local = (float)GRAPH_WIDTH / (float)(g_traces[i].data_count - 1);
                                        int x_pos1 = GRAPH_LEFT + (int)(j * x_scale_local);
                                        int x_pos2 = GRAPH_LEFT + (int)((j + 1) * x_scale_local);
                                        int y_pos1 = value_to_y(g_traces[i].data[j]);
                                        int y_pos2 = value_to_y(g_traces[i].data[j + 1]);
                                        
                                        /* Only redraw lines that intersect the cleared rectangle */
                                        if ((x_pos1 >= clear_x1 && x_pos1 <= clear_x2) ||
                                            (x_pos2 >= clear_x1 && x_pos2 <= clear_x2)) {
                                            if ((y_pos1 >= clear_y1 && y_pos1 <= clear_y2) ||
                                                (y_pos2 >= clear_y1 && y_pos2 <= clear_y2)) {
                                                draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    /* Draw the new readout */
                    draw_text(readout_x, readout_y, readout, 3);
                    
                    /* Update tracking variables */
                    last_readout_x = readout_x;
                    last_readout_y = readout_y;
                    last_readout_width = readout_width;
                    last_sample_number = sample_num;
                }
            }
            
            readout_needs_update = 0;
        }
        
        if (g_mouse.present && mouse_visible) {
            get_mouse_status();
            
            /* Scale mouse coordinates to screen */
            x = g_mouse.x * 320 / 80;
            y = g_mouse.y * 200 / 25;
            
            /* Only process if mouse is in graph area */
            if ((x != old_x || y != old_y) && 
                x >= GRAPH_LEFT && x <= GRAPH_RIGHT &&
                y >= GRAPH_TOP && y <= GRAPH_BOTTOM) {
                
                keyboard_mode = 0;
                
                /* Map mouse position directly to sample number first, then calculate cursor_x */
                if (selected_trace >= 0 && g_traces[selected_trace].enabled && 
                    g_traces[selected_trace].data_count > 1) {
                    
                    /* Calculate sample_to_x_scale */
                    sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
                    
                    /* Map mouse X position to sample number (0 to data_count-1) */
                    mouse_graph_position = (float)(x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                    if (mouse_graph_position < 0.0) mouse_graph_position = 0.0;
                    if (mouse_graph_position > 1.0) mouse_graph_position = 1.0;
                    
                    current_sample = (int)(mouse_graph_position * (g_traces[selected_trace].data_count - 1) + 0.5);
                    
                    /* Bounds check */
                    if (current_sample < 0) current_sample = 0;
                    if (current_sample >= g_traces[selected_trace].data_count) {
                        current_sample = g_traces[selected_trace].data_count - 1;
                    }
                    
                    /* Now calculate cursor_x using same method as arrow keys */
                    cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                } else {
                    /* Fallback for when no trace is selected */
                    cursor_x = x;
                    if (cursor_x < GRAPH_LEFT) cursor_x = GRAPH_LEFT;
                    if (cursor_x > GRAPH_RIGHT) cursor_x = GRAPH_RIGHT;
                }
                
                cursor_visible = 1;
                readout_needs_update = 1;
                
                old_x = x;
                old_y = y;
            }
        }
        
        if (kbhit()) {
            key = getch();
            
            /* Extended key handling moved to main switch statement case 0 */
            
            if (key >= 128 && key <= 137) {  /* ALT+0 through ALT+9 - alternative handling */
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
                    
                    /* Initialize cursor position to center of selected trace */
                    if (g_traces[slot].data_count > 0) {
                        current_sample = g_traces[slot].data_count / 2;
                        sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[slot].data_count - 1);
                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                        readout_needs_update = 1;
                    }
                    
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    display_scale_factor = get_engineering_scale(y_range, &per_div_value, &display_unit_str, &display_decimal_places);
                    
                    if (is_fft_trace[selected_trace]) {
                        float sample_rate = (g_control_panel.sample_rate_ms > 0) ? (1000.0 / g_control_panel.sample_rate_ms) : 1000.0;
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
                    
                    if (g_traces[selected_trace].data_count > 1) {
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
                        
                        /* Initialize current_frequency for FFT traces */
                        if (is_fft_trace[selected_trace]) {
                            current_frequency = g_traces[selected_trace].x_offset + 
                                              (current_sample * g_traces[selected_trace].x_scale);
                        }
                    } else {
                        current_sample = 0;
                        sample_to_x_scale = (float)GRAPH_WIDTH;
                        cursor_x = GRAPH_LEFT + GRAPH_WIDTH / 2;
                    }
                    
                    if (g_has_287) {
                        float y_range_check = g_graph_scale.max_value - g_graph_scale.min_value;
                        if (y_range_check == 0.0) y_range_check = 1.0;  /* Prevent divide by zero */
                        y_scale = (float)GRAPH_HEIGHT / y_range_check;
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
                    display_scale_factor = get_engineering_scale(y_range, &per_div_value, &display_unit_str, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        float y_range_check = g_graph_scale.max_value - g_graph_scale.min_value;
                        if (y_range_check == 0.0) y_range_check = 1.0;  /* Prevent divide by zero */
                        y_scale = (float)GRAPH_HEIGHT / y_range_check;
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
                    
                    if (range < 0.000005) {  /* Less than 5µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range (1µV per div) */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000010) {  /* Less than 10µV total range */
                        range = 0.000005;  /* Set to exactly 5µV total (1µV per div) */
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
                    
                    /* 287-optimized precision drift detection */
                    {
                        double current_range_d = (double)g_graph_scale.max_value - (double)g_graph_scale.min_value;
                        double per_div_d = current_range_d * 0.2;  /* 1/5 exactly representable */
                        double expected_range_d = per_div_d * 5.0;
                        double range_error_d = fabs(current_range_d - expected_range_d);
                        double relative_error = range_error_d / current_range_d;
                        
                        /* 287-specific machine epsilon for float precision */
                        double float_epsilon = g_has_287 ? 1.19209290e-07 : 5.96046448e-08;
                        double drift_threshold = float_epsilon * 16.0;  /* Conservative threshold */
                        
                        zoom_operations_count++;
                        
                        /* Enhanced drift detection using 287 precision characteristics */
                        if (current_range_d < 1.0e-3) {
                            /* Ultra-precision ranges (µV): more sensitive to 287 rounding */
                            if (relative_error > drift_threshold && zoom_operations_count >= 1) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        } else if (current_range_d < 1.0) {
                            /* mV ranges: moderate sensitivity */
                            if (relative_error > (drift_threshold * 2.0) && zoom_operations_count >= 2) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        } else {
                            /* V ranges: less frequent correction but force periodic cleanup */
                            if ((relative_error > (drift_threshold * 4.0) && zoom_operations_count >= 3) ||
                                zoom_operations_count >= 6) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        }
                    }
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    display_scale_factor = get_engineering_scale(y_range, &per_div_value, &display_unit_str, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        float y_range_check = g_graph_scale.max_value - g_graph_scale.min_value;
                        if (y_range_check == 0.0) y_range_check = 1.0;  /* Prevent divide by zero */
                        y_scale = (float)GRAPH_HEIGHT / y_range_check;
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
                    
                    if (range < 0.000005) {  /* Less than 5µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range (1µV per div) */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000010) {  /* Less than 10µV total range */
                        range = 0.000005;  /* Set to exactly 5µV total (1µV per div) */
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
                    
                    /* 287-optimized precision drift detection */
                    {
                        double current_range_d = (double)g_graph_scale.max_value - (double)g_graph_scale.min_value;
                        double per_div_d = current_range_d * 0.2;  /* 1/5 exactly representable */
                        double expected_range_d = per_div_d * 5.0;
                        double range_error_d = fabs(current_range_d - expected_range_d);
                        double relative_error = range_error_d / current_range_d;
                        
                        /* 287-specific machine epsilon for float precision */
                        double float_epsilon = g_has_287 ? 1.19209290e-07 : 5.96046448e-08;
                        double drift_threshold = float_epsilon * 16.0;  /* Conservative threshold */
                        
                        zoom_operations_count++;
                        
                        /* Enhanced drift detection using 287 precision characteristics */
                        if (current_range_d < 1.0e-3) {
                            /* Ultra-precision ranges (µV): more sensitive to 287 rounding */
                            if (relative_error > drift_threshold && zoom_operations_count >= 1) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        } else if (current_range_d < 1.0) {
                            /* mV ranges: moderate sensitivity */
                            if (relative_error > (drift_threshold * 2.0) && zoom_operations_count >= 2) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        } else {
                            /* V ranges: less frequent correction but force periodic cleanup */
                            if ((relative_error > (drift_threshold * 4.0) && zoom_operations_count >= 3) ||
                                zoom_operations_count >= 6) {
                                snap_graph_scale_to_clean_values();
                                zoom_operations_count = 0;
                            }
                        }
                    }
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    display_scale_factor = get_engineering_scale(y_range, &per_div_value, &display_unit_str, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        float y_range_check = g_graph_scale.max_value - g_graph_scale.min_value;
                        if (y_range_check == 0.0) y_range_check = 1.0;  /* Prevent divide by zero */
                        y_scale = (float)GRAPH_HEIGHT / y_range_check;
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
                            
                        case 75:  /* Left arrow - context-aware cursor movement */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                
                                /* Check if this is an FFT trace */
                                if (is_fft_trace[selected_trace]) {
                                    /* FFT frequency-based navigation */
                                    float freq_step;
                                    float sample_pos;
                                    
                                    if (fine_cursor_mode) {
                                        freq_step = 0.01;  /* Fine: 0.01Hz */
                                    } else {
                                        freq_step = 0.1;   /* Coarse: 0.1Hz */
                                    }
                                    
                                    /* Calculate new frequency */
                                    current_frequency -= freq_step;
                                    if (current_frequency < g_traces[selected_trace].x_offset) {
                                        current_frequency = g_traces[selected_trace].x_offset;
                                    }
                                    
                                    /* Convert frequency back to sample position */
                                    sample_pos = (current_frequency - g_traces[selected_trace].x_offset) / g_traces[selected_trace].x_scale;
                                    current_sample = (int)(sample_pos + 0.5);
                                    if (current_sample < 0) current_sample = 0;
                                    
                                    /* Update cursor position */
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(sample_pos * GRAPH_WIDTH / g_traces[selected_trace].data_count);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)sample_pos * GRAPH_WIDTH / g_traces[selected_trace].data_count);
                                    }
                                } else {
                                    /* Voltage trace sample-based navigation (preserve existing behavior) */
                                    int step_size = fine_cursor_mode ? 1 : 10;  /* Fine: 1 sample, Coarse: 10 samples */
                                    
                                    if (current_sample >= step_size) {
                                        current_sample -= step_size;
                                        if (g_has_287) {
                                            cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                        } else {
                                            cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                        }
                                    }
                                }
                                cursor_visible = 1;
                                readout_needs_update = 1;
                            }
                            break;
                            
                        case 77:  /* Right arrow - context-aware cursor movement */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                
                                /* Check if this is an FFT trace */
                                if (is_fft_trace[selected_trace]) {
                                    /* FFT frequency-based navigation */
                                    float freq_step;
                                    float max_freq;
                                    float sample_pos;
                                    
                                    if (fine_cursor_mode) {
                                        freq_step = 0.01;  /* Fine: 0.01Hz */
                                    } else {
                                        freq_step = 0.1;   /* Coarse: 0.1Hz */
                                    }
                                    
                                    /* Calculate maximum frequency */
                                    max_freq = g_traces[selected_trace].x_offset + 
                                             (g_traces[selected_trace].data_count * g_traces[selected_trace].x_scale);
                                    
                                    /* Calculate new frequency */
                                    current_frequency += freq_step;
                                    if (current_frequency > max_freq) {
                                        current_frequency = max_freq;
                                    }
                                    
                                    /* Convert frequency back to sample position */
                                    sample_pos = (current_frequency - g_traces[selected_trace].x_offset) / g_traces[selected_trace].x_scale;
                                    current_sample = (int)(sample_pos + 0.5);
                                    if (current_sample >= g_traces[selected_trace].data_count) {
                                        current_sample = g_traces[selected_trace].data_count - 1;
                                    }
                                    
                                    /* Update cursor position */
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(sample_pos * GRAPH_WIDTH / g_traces[selected_trace].data_count);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)sample_pos * GRAPH_WIDTH / g_traces[selected_trace].data_count);
                                    }
                                } else {
                                    /* Voltage trace sample-based navigation (preserve existing behavior) */
                                    int step_size = fine_cursor_mode ? 1 : 10;  /* Fine: 1 sample, Coarse: 10 samples */
                                    
                                    if (current_sample + step_size < g_traces[selected_trace].data_count) {
                                        current_sample += step_size;
                                        if (g_has_287) {
                                            cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                        } else {
                                            cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                        }
                                    }
                                }
                                cursor_visible = 1;
                                readout_needs_update = 1;
                            }
                            break;
                            
                        default:
                            /* ALT+1 through ALT+9 have scan codes 120-128, ALT+0 is 129 */
                            if (key >= 120 && key <= 129) {
                                if (key == 129) {
                                    slot = 0;  /* ALT+0 */
                                } else {
                                    slot = key - 119;  /* ALT+1 through ALT+9 */
                                }
                                
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
                    
                case 'F':  /* Toggle fine/coarse cursor mode */
                case 'f':
                    fine_cursor_mode = !fine_cursor_mode;
                    cursor_mode_changed = 1;
                    /* Update current frequency for FFT traces to maintain position */
                    if (selected_trace >= 0 && is_fft_trace[selected_trace] && g_traces[selected_trace].enabled) {
                        current_frequency = g_traces[selected_trace].x_offset + 
                                          (current_sample * g_traces[selected_trace].x_scale);
                    }
                    need_redraw = 1;  /* Redraw to update mode indicator */
                    break;
                    
                case 'H':  /* Help/Config menu */
                case 'h':
                    text_mode();
                    graph_config_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
                    
                case 'T':  /* Trace selection menu */
                case 't':
                    text_mode();
                    display_trace_selection_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
            }
        }
        
        delay(10);
    }
    
    text_mode();
}

/* UI utility functions for enhanced menus */
void display_header(char *title) {
    int i, len, padding;
    
    clrscr();
    printf("TM5000 v%s - ", TM5000_VERSION);
    printf("%s\n", title);
    
    len = strlen(title) + 14;  /* Account for version string */
    for (i = 0; i < len; i++) {
        printf("=");
    }
    printf("\n\n");
}

void display_footer(char *prompt) {
    printf("\n%s", prompt);
}

void display_menu_item(int number, char *text, int enabled) {
    if (enabled) {
        if (number >= '0' && number <= '9') {
            printf("%c. %s\n", number, text);
        } else {
            printf("%d. %s\n", number, text);
        }
    } else {
        if (number >= '0' && number <= '9') {
            printf("%c. (%s)\n", number, text);
        } else {
            printf("%d. (%s)\n", number, text);
        }
    }
}

void display_success(char *msg) {
    printf("\n*** %s ***\n", msg);
}

/* Enhanced input functions */
int get_string_input(char *prompt, char *buffer, int max_length) {
    if (prompt && strlen(prompt) > 0) {
        printf("%s", prompt);
    }
    
    if (fgets(buffer, max_length, stdin) == NULL) {
        return 0;
    }
    
    /* Remove newline */
    buffer[strcspn(buffer, "\n")] = '\0';
    
    return strlen(buffer) > 0;
}

int get_yes_no(char *prompt) {
    char response;
    
    printf("%s (Y/N): ", prompt);
    response = toupper(getch());
    printf("%c\n", response);
    
    return (response == 'Y');
}

