/*
 * TM5000 GPIB Control System - Data Management Module
 * Version 3.3
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Fixed load/save module activation issues, removed legacy format support, fixed CSV parsing bug with pipe delimiters
 * 3.3 - Fixed fseek reliability issues in configuration and measurement data loading
 */

#include "data.h"
#include "modules.h"

/* Allocate memory buffer for a module's data */
int allocate_module_buffer(int slot, unsigned int size) {
    if (slot < 0 || slot >= 10) return 0;
    
    /* Free existing buffer if present */
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
    }
    
    /* Allocate new buffer */
    g_system->modules[slot].module_data = (float far *)_fmalloc(size * sizeof(float));
    if (g_system->modules[slot].module_data) {
        g_system->modules[slot].module_data_size = size;
        g_system->modules[slot].module_data_count = 0;
        return 1;  /* Success */
    }
    
    return 0;  /* Failed */
}

/* Free memory buffer for a module */
void free_module_buffer(int slot) {
    if (slot < 0 || slot >= 10) return;
    
    if (g_system->modules[slot].module_data) {
        _ffree(g_system->modules[slot].module_data);
        g_system->modules[slot].module_data = NULL;
        g_system->modules[slot].module_data_size = 0;
        g_system->modules[slot].module_data_count = 0;
    }
}

/* Store a data value in a module's buffer */
void store_module_data(int slot, float value) {
    if (slot < 0 || slot >= 10) return;
    if (!g_system->modules[slot].module_data) return;
    
    if (g_system->modules[slot].module_data_count < g_system->modules[slot].module_data_size) {
        g_system->modules[slot].module_data[g_system->modules[slot].module_data_count] = value;
        g_system->modules[slot].module_data_count++;
        g_system->modules[slot].last_reading = value;
    }
}

/* Clear all data in a module's buffer */
void clear_module_data(int slot) {
    if (slot < 0 || slot >= 10) return;
    g_system->modules[slot].module_data_count = 0;
}

/* Save measurement data to file */
void save_data(void) {
    FILE *fp;
    char filename[80];
    int i, j;
    int total_module_samples = 0;
    
    clrscr();
    printf("Save Data\n");
    printf("=========\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".tm5");
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot create file\n");
        getch();
        return;
    }
    
    /* Write file header and format version */
    fprintf(fp, "TM5000 Data File v%s\n", TM5000_VERSION);
    fprintf(fp, "FileFormat: ModuleData\n");  /* Enhanced format indicator */
    
    /* Write global sample information */
    fprintf(fp, "GlobalSamples: %u\n", g_system->data_count);
    fprintf(fp, "SampleRateMs: %d\n", g_control_panel.sample_rate_ms);
    fprintf(fp, "RateType: %s\n", g_control_panel.use_custom ? "Custom" : "Preset");
    if (!g_control_panel.use_custom) {
        fprintf(fp, "PresetIndex: %d\n", g_control_panel.selected_rate);
    }
    
    /* Write module configuration */
    fprintf(fp, "Modules:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            printf("Saving module %d: type=%d, addr=%d, desc='%s', samples=%u\n",
                   i, g_system->modules[i].module_type, g_system->modules[i].gpib_address,
                   g_system->modules[i].description, g_system->modules[i].module_data_count);
            
            fprintf(fp, "%d|%d|%d|%s|%u\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description,
                    g_system->modules[i].module_data_count);
            total_module_samples += g_system->modules[i].module_data_count;
        }
    }
    
    /* Write module-specific configurations */
    fprintf(fp, "ModuleConfigs:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            switch (g_system->modules[i].module_type) {
                case MOD_DM5120:
                    save_dm5120_config(fp, i);
                    break;
                case MOD_DM5010:
                    save_dm5010_config(fp, i);
                    break;
                case MOD_PS5004:
                    save_ps5004_config(fp, i);
                    break;
                case MOD_PS5010:
                    save_ps5010_config(fp, i);
                    break;
                case MOD_DC5009:
                    save_dc5009_config(fp, i);
                    break;
                case MOD_DC5010:
                    save_dc5010_config(fp, i);
                    break;
                case MOD_FG5010:
                    save_fg5010_config(fp, i);
                    break;
            }
        }
    }
    
    /* Write global data buffer */
    fprintf(fp, "GlobalData:\n");
    for (i = 0; i < g_system->data_count; i++) {
        fprintf(fp, "%.6e\n", g_system->data_buffer[i]);
    }
    
    /* Write per-module data */
    fprintf(fp, "ModuleData:\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            /* Always write slot entry for enabled modules */
            unsigned int data_count = 0;
            if (g_system->modules[i].module_data && g_system->modules[i].module_data_count > 0) {
                data_count = g_system->modules[i].module_data_count;
            }
            
            printf("Writing slot %d data section: %u samples\n", i, data_count);
            fprintf(fp, "Slot%d:%u\n", i, data_count);
            
            /* Only write actual data if it exists */
            if (data_count > 0) {
                for (j = 0; j < data_count; j++) {
                    fprintf(fp, "%.6e\n", g_system->modules[i].module_data[j]);
                }
                printf("Wrote %u data values for slot %d\n", data_count, i);
            } else {
                printf("No measurement data for slot %d (configured but not measured)\n", i);
            }
        }
    }
    
    fprintf(fp, "EndOfFile\n");
    fclose(fp);
    
    printf("\nData saved successfully!\n");
    printf("File: %s\n", filename);
    printf("Global samples: %u\n", g_system->data_count);
    printf("Total module samples: %d\n", total_module_samples);
    printf("\nPress any key to continue...");
    getch();
}

/* Load measurement data from file */
void load_data(void) {
    FILE *fp;
    char filename[80];
    char line[100];
    char desc[20];
    int slot, type, addr;
  
    unsigned int count;
    int i, j;
    unsigned int module_count;
    float value;
    int total_loaded = 0;
    int active_modules = 0;
    
    clrscr();
    printf("Load Data\n");
    printf("=========\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".tm5");
    
    fp = fopen(filename, "r");
    if (!fp) {
        /* Try without extension */
        filename[strlen(filename)-4] = '\0';
        fp = fopen(filename, "r");
        if (!fp) {
            printf("Error: Cannot open file '%s'\n", filename);
            getch();
            return;
        }
    }
    
    /* Check file format version - only support v3.2+ enhanced format */
    fgets(line, sizeof(line), fp);
    
    if (!fgets(line, sizeof(line), fp) || !strstr(line, "FileFormat: ModuleData")) {
        printf("Error: This file format is not supported. TM5000 v3.2+ only supports enhanced format files.\n");
        printf("Please use TM5000 v3.1 or earlier to convert legacy files.\n");
        fclose(fp);
        getch();
        return;
    }
    
    /* Clear existing data */
    for (i = 0; i < 10; i++) {
        clear_module_data(i);
        g_system->modules[i].enabled = 0;
    }
    g_system->data_count = 0;
    
    printf("Loading enhanced format file...\n");
    
    /* Read global sample count */
    fscanf(fp, "GlobalSamples: %u\n", &g_system->data_count);
    
    /* Read sample rate information */
    fscanf(fp, "SampleRateMs: %d\n", &g_control_panel.sample_rate_ms);
    fgets(line, sizeof(line), fp);  /* RateType line */
    if (strstr(line, "Custom")) {
        g_control_panel.use_custom = 1;
        sprintf(g_control_panel.custom_rate, "%d", g_control_panel.sample_rate_ms);
    } else {
        g_control_panel.use_custom = 0;
        fscanf(fp, "PresetIndex: %d\n", &g_control_panel.selected_rate);
    }
    
    /* Read module configuration */
    fgets(line, sizeof(line), fp);  /* "Modules:" */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "ModuleConfigs:", 14) == 0) {
            break;
        }
        if (strncmp(line, "GlobalData:", 11) == 0) {
            break;
        }
        
        if (sscanf(line, "%d|%d|%d|%[^|]|%u", &slot, &type, &addr, desc, &count) == 5) {
            if (slot >= 0 && slot < 10) {
                g_system->modules[slot].module_type = type;
                g_system->modules[slot].slot_number = slot;
                g_system->modules[slot].gpib_address = addr;
                g_system->modules[slot].enabled = 1;
                g_system->modules[slot].last_reading = 0.0;
                strcpy(g_system->modules[slot].description, desc);
                
                /* Initialize module-specific configuration */
                switch (type) {
                    case MOD_DM5120: 
                        init_dm5120_config(slot); 
                        init_dm5120_config_enhanced(slot); 
                        break;
                    case MOD_DM5010: 
                        init_dm5010_config(slot); 
                        break;
                    case MOD_PS5004: 
                        init_ps5004_config(slot); 
                        break;
                    case MOD_PS5010: 
                        init_ps5010_config(slot); 
                        break;
                    case MOD_DC5009: 
                        init_dc5009_config(slot); 
                        break;
                    case MOD_DC5010: 
                        init_dc5010_config(slot); 
                        break;
                    case MOD_FG5010: 
                        init_fg5010_config(slot); 
                        break;
                }
                
                /* Allocate buffer only if module has data, but keep module enabled */
                if (count > 0) {
                    if (!allocate_module_buffer(slot, count + 100)) {
                        printf("Error: Failed to allocate memory for module %d (%u samples)\n", slot, count);
                        printf("Module %d will remain configured but without data buffer\n", slot);
                        /* Keep module enabled even if buffer allocation fails */
                    } else {
                    }
                } else {
                }
                /* Module stays enabled regardless of data buffer status */
            } else {
            }
        } else {
        }
    }
    
    /* Read module-specific configurations if present */
    if (strncmp(line, "ModuleConfigs:", 14) == 0) {
        printf("Loading module configurations...\n");
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "GlobalData:", 11) == 0) break;
            
            /* Parse module config header like "Slot2:DM5120" */
            if (sscanf(line, "Slot%d:%*s", &slot) == 1) {
                if (slot >= 0 && slot < 10 && g_system->modules[slot].enabled) {
                    switch (g_system->modules[slot].module_type) {
                        case MOD_DM5120:
                            load_dm5120_config(fp, slot);
                            break;
                        case MOD_DM5010:
                            load_dm5010_config(fp, slot);
                            break;
                        case MOD_PS5004:
                            load_ps5004_config(fp, slot);
                            break;
                        case MOD_PS5010:
                            load_ps5010_config(fp, slot);
                            break;
                        case MOD_DC5009:
                            load_dc5009_config(fp, slot);
                            break;
                        case MOD_DC5010:
                            load_dc5010_config(fp, slot);
                            break;
                        case MOD_FG5010:
                            load_fg5010_config(fp, slot);
                            break;
                    }
                }
            }
        }
    }
    
    /* Read global data with enhanced validation */
    if (g_system->data_buffer == NULL) {
        printf("Error: Global data buffer not allocated\n");
        fclose(fp);
        return;
    }
    
    for (i = 0; i < g_system->data_count && i < g_system->buffer_size; i++) {
        if (fscanf(fp, "%f", &value) != 1) break;
        
        /* Validate data before storing */
        if (value != value || value == HUGE_VAL || value == -HUGE_VAL) {
            printf("Warning: Invalid global data value at index %d, setting to 0.0\n", i);
            value = 0.0;
        }
        
        g_system->data_buffer[i] = value;
    }
    
    /* Read per-module data */
    fgets(line, sizeof(line), fp);  /* Skip to ModuleData: */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "EndOfFile", 9) == 0) break;
        
        if (sscanf(line, "Slot%d:%u", &slot, &module_count) == 2) {
            if (slot >= 0 && slot < 10 && g_system->modules[slot].enabled) {
                printf("Processing slot %d with %u samples...\n", slot, module_count);
                
                /* Handle modules with no data (count=0) */
                if (module_count == 0) {
                    printf("Slot %d configured but has no measurement data\n", slot);
                    continue;  /* Skip data loading but keep module enabled */
                }
                
                /* Validate module has allocated buffer for data loading */
                if (!g_system->modules[slot].module_data) {
                    printf("Warning: Module %d has no data buffer, attempting allocation...\n", slot);
                    if (!allocate_module_buffer(slot, module_count + 100)) {
                        printf("Error: Failed to allocate buffer for module %d data\n", slot);
                        continue;  /* Skip data loading but keep module enabled */
                    }
                }
                
                /* Validate requested count doesn't exceed buffer size */
                if (module_count > g_system->modules[slot].module_data_size) {
                    printf("Warning: Module %d data count (%u) exceeds buffer size (%u), truncating...\n", 
                           slot, module_count, g_system->modules[slot].module_data_size);
                    module_count = g_system->modules[slot].module_data_size;
                }
                
                printf("Loading %u samples for slot %d...\n", module_count, slot);
                
                for (j = 0; j < module_count; j++) {
                    if (fscanf(fp, "%f", &value) == 1) {
                        /* Safety check for invalid values */
                        if (value != value || value == HUGE_VAL || value == -HUGE_VAL) {
                            printf("Warning: Invalid data value detected in slot %d sample %d, setting to 0.0\n", slot, j);
                            value = 0.0;  /* Use safe default instead of skipping */
                        }
                        store_module_data(slot, value);
                        total_loaded++;
                    } else {
                        printf("Warning: Failed to read sample %d for slot %d\n", j, slot);
                        break;
                    }
                }
                
                /* Update last reading */
                if (g_system->modules[slot].module_data_count > 0) {
                    g_system->modules[slot].last_reading = 
                        g_system->modules[slot].module_data[
                            g_system->modules[slot].module_data_count - 1];
                }
            }
        }
    }
    
    fclose(fp);
    
    /* Display summary */
    printf("\nData loaded successfully!\n");
    printf("File format: Enhanced (v3.2)\n");
    printf("Global samples: %u\n", g_system->data_count);
    printf("Sample rate: %d ms\n", g_control_panel.sample_rate_ms);
    
    for (i = 0; i < 10; i++) {
    }
    
    printf("\nActive modules after loading:\n");
    active_modules = 0;
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            printf("  Slot %d: %s (type %d, addr %d) - %u samples\n", 
                   i, g_system->modules[i].description, 
                   g_system->modules[i].module_type,
                   g_system->modules[i].gpib_address,
                   g_system->modules[i].module_data_count);
            active_modules++;
        }
    }
    
    if (active_modules == 0) {
        printf("  WARNING: No modules are active after loading!\n");
    } else {
        printf("Total active modules: %d\n", active_modules);
    }
    
    printf("\nPress any key to continue...");
    
    /* Synchronize loaded modules with display traces */
    sync_traces_with_modules();
    
    for (i = 0; i < 10; i++) {
    }
    
    /* Safety check: Ensure graph scale is valid after loading data */
    if (g_graph_scale.max_value <= g_graph_scale.min_value || 
        g_graph_scale.max_value != g_graph_scale.max_value ||
        g_graph_scale.min_value != g_graph_scale.min_value) {
        printf("\nWarning: Invalid graph scale detected, resetting to default range...\n");
        g_graph_scale.min_value = -1000.0;
        g_graph_scale.max_value = 1000.0;
    }
    
    getch();
}

/* Save configuration settings to file */
void save_settings(void) {
    FILE *fp;
    char filename[80];
    int i;
    
    clrscr();
    printf("Save Configuration Settings\n");
    printf("===========================\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".cfg");
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot create file\n");
        getch();
        return;
    }
    
    fprintf(fp, "TM5000 Configuration File v%s\n", TM5000_VERSION);
    
    /* Control panel settings */
    fprintf(fp, "[ControlPanel]\n");
    fprintf(fp, "SampleRateMs=%d\n", g_control_panel.sample_rate_ms);
    fprintf(fp, "UseCustom=%d\n", g_control_panel.use_custom);
    fprintf(fp, "SelectedRate=%d\n", g_control_panel.selected_rate);
    fprintf(fp, "CustomRate=%s\n", g_control_panel.custom_rate);
    fprintf(fp, "MonitorMask=%d\n", g_control_panel.monitor_mask);
    fprintf(fp, "MonitorAll=%d\n", g_control_panel.monitor_all);
    
    /* Module configuration */
    fprintf(fp, "\n[Modules]\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            fprintf(fp, "Module%d=%d,%d,%s\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description);
        }
    }
    
    /* Module-specific configurations */
    fprintf(fp, "\n[ModuleConfigs]\n");
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            switch (g_system->modules[i].module_type) {
                case MOD_DM5120:
                    fprintf(fp, "DM5120_%d_LF=%d\n", i, g_dm5120_config[i].lf_termination);
                    fprintf(fp, "DM5120_%d_Function=%s\n", i, g_dm5120_config[i].function);
                    fprintf(fp, "DM5120_%d_Range=%d\n", i, g_dm5120_config[i].range_mode);
                    fprintf(fp, "DM5120_%d_Filter=%d\n", i, g_dm5120_config[i].filter_enabled);
                    break;
                case MOD_DC5009:
                    fprintf(fp, "DC5009_%d_Function=%s\n", i, g_dc5009_config[i].function);
                    fprintf(fp, "DC5009_%d_Channel=%s\n", i, g_dc5009_config[i].channel);
                    break;
                case MOD_DC5010:
                    fprintf(fp, "DC5010_%d_Function=%s\n", i, g_dc5010_config[i].function);
                    fprintf(fp, "DC5010_%d_Channel=%s\n", i, g_dc5010_config[i].channel);
                    break;
            }
        }
    }
    
    /* Graph scale settings */
    fprintf(fp, "\n[GraphScale]\n");
    fprintf(fp, "MinValue=%f\n", g_graph_scale.min_value);
    fprintf(fp, "MaxValue=%f\n", g_graph_scale.max_value);
    fprintf(fp, "AutoScale=%d\n", g_graph_scale.auto_scale);
    fprintf(fp, "ZoomFactor=%f\n", g_graph_scale.zoom_factor);
    
    fclose(fp);
    
    printf("\nSettings saved to %s\n", filename);
    printf("Press any key to continue...");
    getch();
}

/* Load configuration settings from file */
void load_settings(void) {
    FILE *fp;
    char filename[80];
    char line[GPIB_BUFFER_SIZE];
    char key[50], value[200];
    int slot, type, addr;
    char desc[50];
    
    clrscr();
    printf("Load Configuration Settings\n");
    printf("===========================\n\n");
    
    printf("Enter filename (without extension): ");
    scanf("%s", filename);
    strcat(filename, ".cfg");
    
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Cannot open file '%s'\n", filename);
        getch();
        return;
    }
    
    /* Verify file header */
    fgets(line, sizeof(line), fp);
    if (!strstr(line, "TM5000 Configuration")) {
        printf("Error: Invalid configuration file\n");
        fclose(fp);
        getch();
        return;
    }
    
    /* Parse configuration file sections */
    while (fgets(line, sizeof(line), fp)) {
        /* Remove newline characters */
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0) {
            continue;
        }
        
        
        /* Additional cleanup for any stray characters */
        {
            int i;
            for (i = strlen(line) - 1; i >= 0; i--) {
                if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n' || line[i] < 32) {
                    line[i] = '\0';
                } else {
                    break;
                }
            }
        }
        
        if (strcmp(line, "[ControlPanel]") == 0) {
            /* Read control panel settings */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    /* Don't use fseek - process the section line directly */
                    /* Remove newlines from the section line */
                    line[strcspn(line, "\r\n")] = 0;
                    
                    /* Check what section this is and set a flag for main loop */
                    if (strcmp(line, "[Modules]") == 0) {
                        /* Process modules section immediately */
                        goto process_modules_section;
                    }
                    break;
                }
                if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                    if (strcmp(key, "SampleRateMs") == 0) {
                        g_control_panel.sample_rate_ms = atoi(value);
                    } else if (strcmp(key, "UseCustom") == 0) {
                        g_control_panel.use_custom = atoi(value);
                    } else if (strcmp(key, "SelectedRate") == 0) {
                        g_control_panel.selected_rate = atoi(value);
                    } else if (strcmp(key, "CustomRate") == 0) {
                        strcpy(g_control_panel.custom_rate, value);
                    } else if (strcmp(key, "MonitorMask") == 0) {
                        g_control_panel.monitor_mask = atoi(value);
                    } else if (strcmp(key, "MonitorAll") == 0) {
                        g_control_panel.monitor_all = atoi(value);
                    }
                }
            }
        }
        
        if (strcmp(line, "[Modules]") == 0) {
        process_modules_section:
            /* Clear existing modules - match v2.9 behavior */
            for (slot = 0; slot < 10; slot++) {
                g_system->modules[slot].enabled = 0;
            }
            
            /* Read module configuration */
            
            while (fgets(line, sizeof(line), fp)) {
                
                /* Remove newline characters */
                line[strcspn(line, "\r\n")] = 0;
                
                if (strlen(line) == 0) {
                    continue;
                }
                
                if (line[0] == '[') {
                    /* End of section - don't use unreliable fseek */
                    break;
                }
                {
                    int fields = sscanf(line, "Module%d=%d,%d,%[^\n]", &slot, &type, &addr, desc);
                    if (fields >= 4) {
                    if (slot >= 0 && slot < 10) {
                        g_system->modules[slot].module_type = type;
                        g_system->modules[slot].gpib_address = addr;
                        g_system->modules[slot].enabled = 1;
                        g_system->modules[slot].last_reading = 0.0;
                        strcpy(g_system->modules[slot].description, desc);
                        g_system->modules[slot].slot_number = slot;
                        
                        /* Initialize module-specific configurations with defaults first */
                        switch (type) {
                            case MOD_DC5009:
                                init_dc5009_config(slot);
                                break;
                            case MOD_DC5010:
                                init_dc5010_config(slot);
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
                        }
                        
                        /* Allocate data buffer for module */
                        if (!allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE)) {
                            printf("Error: Failed to allocate memory for module %d\n", slot);
                            g_system->modules[slot].enabled = 0;  /* Disable if allocation fails */
                        } else {
                        }
                    }
                    } else {
                    }
                }
            }
        }
        
        if (strcmp(line, "[ModuleConfigs]") == 0) {
            /* Read module-specific configurations */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    /* End of section - don't use unreliable fseek */
                    break;
                }
                if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                    int slot;
                    if (sscanf(key, "DM5120_%d_LF", &slot) == 1 && slot >= 0 && slot < 10) {
                        g_dm5120_config[slot].lf_termination = atoi(value);
                    } else if (sscanf(key, "DM5120_%d_Function", &slot) == 1 && slot >= 0 && slot < 10) {
                        strcpy(g_dm5120_config[slot].function, value);
                    } else if (sscanf(key, "DM5120_%d_Range", &slot) == 1 && slot >= 0 && slot < 10) {
                        g_dm5120_config[slot].range_mode = atoi(value);
                    } else if (sscanf(key, "DM5120_%d_Filter", &slot) == 1 && slot >= 0 && slot < 10) {
                        g_dm5120_config[slot].filter_enabled = atoi(value);
                    } else if (sscanf(key, "DC5009_%d_Function", &slot) == 1 && slot >= 0 && slot < 10) {
                        strcpy(g_dc5009_config[slot].function, value);
                    } else if (sscanf(key, "DC5009_%d_Channel", &slot) == 1 && slot >= 0 && slot < 10) {
                        strcpy(g_dc5009_config[slot].channel, value);
                    } else if (sscanf(key, "DC5010_%d_Function", &slot) == 1 && slot >= 0 && slot < 10) {
                        strcpy(g_dc5010_config[slot].function, value);
                    } else if (sscanf(key, "DC5010_%d_Channel", &slot) == 1 && slot >= 0 && slot < 10) {
                        strcpy(g_dc5010_config[slot].channel, value);
                    }
                }
            }
        }
        
        if (strcmp(line, "[GraphScale]") == 0) {
            /* Read graph scale settings */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') break;
                if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                    if (strcmp(key, "MinValue") == 0) {
                        g_graph_scale.min_value = (float)atof(value);
                    } else if (strcmp(key, "MaxValue") == 0) {
                        g_graph_scale.max_value = (float)atof(value);
                    } else if (strcmp(key, "AutoScale") == 0) {
                        g_graph_scale.auto_scale = atoi(value);
                    } else if (strcmp(key, "ZoomFactor") == 0) {
                        g_graph_scale.zoom_factor = (float)atof(value);
                    }
                }
            }
        }
    }
    
    fclose(fp);
    
    /* Display summary */
    printf("\nSettings loaded successfully!\n");
    printf("Sample rate: %d ms\n", g_control_panel.sample_rate_ms);
    
    
    printf("\nModules configured:\n");
    for (slot = 0; slot < 10; slot++) {
        if (g_system->modules[slot].enabled) {
            printf("  Slot %d: %s (GPIB %d)\n", 
                   slot, 
                   g_system->modules[slot].description,
                   g_system->modules[slot].gpib_address);
        }
    }
    
    /* Put all loaded devices in remote mode */
    for (slot = 0; slot < 10; slot++) {
        if (g_system->modules[slot].enabled) {
            gpib_remote(g_system->modules[slot].gpib_address);
            delay(100);  /* Shorter delay for batch operation */
        }
    }
    
    
    
    /* Synchronize loaded modules with display traces */
    sync_traces_with_modules();
    
    /* Validate and cleanup phantom enabled modules after loading */
    validate_enabled_modules();
    
    printf("\nPress any key to continue...");
    getch();
}

/* Save DM5120 configuration to file */
void save_dm5120_config(FILE *fp, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    fprintf(fp, "Slot%d:DM5120\n", slot);
    fprintf(fp, "function=%s|range_mode=%d|filter_enabled=%d|filter_value=%d|digits=%d|\n",
            cfg->function, cfg->range_mode, cfg->filter_enabled, cfg->filter_value, cfg->digits);
    fprintf(fp, "nullval=%.6f|null_enabled=%d|data_format=%d|buffer_enabled=%d|buffer_size=%d|\n",
            cfg->nullval, cfg->null_enabled, cfg->data_format, cfg->buffer_enabled, cfg->buffer_size);
    fprintf(fp, "min_max_enabled=%d|min_value=%.6f|max_value=%.6f|sample_count=%d|\n",
            cfg->min_max_enabled, cfg->min_value, cfg->max_value, cfg->sample_count);
    fprintf(fp, "burst_mode=%d|sample_rate=%.6f|lf_termination=%d\n",
            cfg->burst_mode, cfg->sample_rate, cfg->lf_termination);
}

/* Save DM5010 configuration to file */
void save_dm5010_config(FILE *fp, int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    
    fprintf(fp, "Slot%d:DM5010\n", slot);
    fprintf(fp, "function=%s|range_mode=%d|filter_enabled=%d|filter_count=%d|trigger_mode=%d|\n",
            cfg->function, cfg->range_mode, cfg->filter_enabled, cfg->filter_count, cfg->trigger_mode);
    fprintf(fp, "digits=%d|nullval=%.6f|null_enabled=%d|data_format=%d|auto_zero=%d|\n",
            cfg->digits, cfg->nullval, cfg->null_enabled, cfg->data_format, cfg->auto_zero);
    fprintf(fp, "calculation_mode=%d|scale_factor=%.6f|scale_offset=%.6f|avg_count=%d|\n",
            cfg->calculation_mode, cfg->scale_factor, cfg->scale_offset, cfg->avg_count);
    fprintf(fp, "dbm_reference=%.6f|dbr_reference=%.6f|beeper_enabled=%d|front_panel_lock=%d|\n",
            cfg->dbm_reference, cfg->dbr_reference, cfg->beeper_enabled, cfg->front_panel_lock);
    fprintf(fp, "high_speed_mode=%d|statistics_enabled=%d|lf_termination=%d\n",
            cfg->high_speed_mode, cfg->statistics_enabled, cfg->lf_termination);
}

/* Save PS5004 configuration to file */
void save_ps5004_config(FILE *fp, int slot) {
    ps5004_config *cfg = &g_ps5004_config[slot];
    
    fprintf(fp, "Slot%d:PS5004\n", slot);
    fprintf(fp, "voltage=%.6f|current_limit=%.6f|output_enabled=%d|display_mode=%d|\n",
            cfg->voltage, cfg->current_limit, cfg->output_enabled, cfg->display_mode);
    fprintf(fp, "vri_enabled=%d|cri_enabled=%d|uri_enabled=%d|dt_enabled=%d|\n",
            cfg->vri_enabled, cfg->cri_enabled, cfg->uri_enabled, cfg->dt_enabled);
    fprintf(fp, "user_enabled=%d|rqs_enabled=%d|lf_termination=%d\n",
            cfg->user_enabled, cfg->rqs_enabled, cfg->lf_termination);
}

/* Save PS5010 configuration to file */
void save_ps5010_config(FILE *fp, int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    
    fprintf(fp, "Slot%d:PS5010\n", slot);
    fprintf(fp, "voltage1=%.6f|current_limit1=%.6f|output1_enabled=%d|\n",
            cfg->voltage1, cfg->current_limit1, cfg->output1_enabled);
    fprintf(fp, "voltage2=%.6f|current_limit2=%.6f|output2_enabled=%d|\n",
            cfg->voltage2, cfg->current_limit2, cfg->output2_enabled);
    fprintf(fp, "logic_voltage=%.6f|logic_current_limit=%.6f|logic_enabled=%d|\n",
            cfg->logic_voltage, cfg->logic_current_limit, cfg->logic_enabled);
    fprintf(fp, "tracking_mode=%d|tracking_ratio=%.6f|display_channel=%d|display_mode=%d|\n",
            cfg->tracking_mode, cfg->tracking_ratio, cfg->display_channel, cfg->display_mode);
    fprintf(fp, "ovp_enabled=%d|ovp_level1=%.6f|ovp_level2=%.6f|\n",
            cfg->ovp_enabled, cfg->ovp_level1, cfg->ovp_level2);
    fprintf(fp, "cv_mode1=%d|cc_mode1=%d|cv_mode2=%d|cc_mode2=%d|\n",
            cfg->cv_mode1, cfg->cc_mode1, cfg->cv_mode2, cfg->cc_mode2);
    fprintf(fp, "srq_enabled=%d|error_queue_size=%d|lf_termination=%d\n",
            cfg->srq_enabled, cfg->error_queue_size, cfg->lf_termination);
}

/* Save DC5009 configuration to file */
void save_dc5009_config(FILE *fp, int slot) {
    dc5009_config *cfg = &g_dc5009_config[slot];
    
    fprintf(fp, "Slot%d:DC5009\n", slot);
    fprintf(fp, "function=%s|channel=%s|gate_time=%.6f|averaging=%d|\n",
            cfg->function, cfg->channel, cfg->gate_time, cfg->averaging);
    fprintf(fp, "coupling_a=%s|coupling_b=%s|impedance_a=%s|impedance_b=%s|\n",
            cfg->coupling_a, cfg->coupling_b, cfg->impedance_a, cfg->impedance_b);
    fprintf(fp, "attenuation_a=%s|attenuation_b=%s|slope_a=%s|slope_b=%s|\n",
            cfg->attenuation_a, cfg->attenuation_b, cfg->slope_a, cfg->slope_b);
    fprintf(fp, "level_a=%.6f|level_b=%.6f|filter_enabled=%d|auto_trigger=%d|\n",
            cfg->level_a, cfg->level_b, cfg->filter_enabled, cfg->auto_trigger);
    fprintf(fp, "overflow_enabled=%d|preset_enabled=%d|srq_enabled=%d|lf_termination=%d\n",
            cfg->overflow_enabled, cfg->preset_enabled, cfg->srq_enabled, cfg->lf_termination);
}

/* Save DC5010 configuration to file */
void save_dc5010_config(FILE *fp, int slot) {
    dc5010_config *cfg = &g_dc5010_config[slot];
    
    fprintf(fp, "Slot%d:DC5010\n", slot);
    fprintf(fp, "function=%s|channel=%s|gate_time=%.6f|averaging=%d|\n",
            cfg->function, cfg->channel, cfg->gate_time, cfg->averaging);
    fprintf(fp, "coupling_a=%s|coupling_b=%s|impedance_a=%s|impedance_b=%s|\n",
            cfg->coupling_a, cfg->coupling_b, cfg->impedance_a, cfg->impedance_b);
    fprintf(fp, "attenuation_a=%s|attenuation_b=%s|slope_a=%s|slope_b=%s|\n",
            cfg->attenuation_a, cfg->attenuation_b, cfg->slope_a, cfg->slope_b);
    fprintf(fp, "level_a=%.6f|level_b=%.6f|filter_enabled=%d|auto_trigger=%d|\n",
            cfg->level_a, cfg->level_b, cfg->filter_enabled, cfg->auto_trigger);
    fprintf(fp, "overflow_enabled=%d|preset_enabled=%d|srq_enabled=%d|\n",
            cfg->overflow_enabled, cfg->preset_enabled, cfg->srq_enabled);
    fprintf(fp, "rise_fall_enabled=%d|burst_mode=%d|lf_termination=%d\n",
            cfg->rise_fall_enabled, cfg->burst_mode, cfg->lf_termination);
}

/* Save FG5010 configuration to file */
void save_fg5010_config(FILE *fp, int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    
    fprintf(fp, "Slot%d:FG5010\n", slot);
    fprintf(fp, "frequency=%.6f|amplitude=%.6f|offset=%.6f|waveform=%s|\n",
            cfg->frequency, cfg->amplitude, cfg->offset, cfg->waveform);
    fprintf(fp, "output_enabled=%d|duty_cycle=%.6f|sweep_enabled=%d|\n",
            cfg->output_enabled, cfg->duty_cycle, cfg->sweep_enabled);
    fprintf(fp, "start_freq=%.6f|stop_freq=%.6f|sweep_time=%.6f|sweep_type=%d|\n",
            cfg->start_freq, cfg->stop_freq, cfg->sweep_time, cfg->sweep_type);
    fprintf(fp, "trigger_source=%s|trigger_slope=%s|trigger_level=%.6f|\n",
            cfg->trigger_source, cfg->trigger_slope, cfg->trigger_level);
    fprintf(fp, "sync_enabled=%d|invert_enabled=%d|phase=%.6f|\n",
            cfg->sync_enabled, cfg->invert_enabled, cfg->phase);
    fprintf(fp, "modulation_enabled=%d|mod_type=%s|mod_freq=%.6f|mod_depth=%.6f|\n",
            cfg->modulation_enabled, cfg->mod_type, cfg->mod_freq, cfg->mod_depth);
    fprintf(fp, "burst_enabled=%d|burst_count=%d|burst_period=%.6f|\n",
            cfg->burst_enabled, cfg->burst_count, cfg->burst_period);
    fprintf(fp, "units_freq=%d|units_time=%d|lf_termination=%d\n",
            cfg->units_freq, cfg->units_time, cfg->lf_termination);
}

/* Load DM5120 configuration from file */
void load_dm5120_config(FILE *fp, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    /* Read configuration lines until we hit another section or slot */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        /* Parse key=value pairs separated by | */
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "function") == 0) strcpy(cfg->function, value);
                else if (strcmp(key, "range_mode") == 0) cfg->range_mode = atoi(value);
                else if (strcmp(key, "filter_enabled") == 0) cfg->filter_enabled = atoi(value);
                else if (strcmp(key, "filter_value") == 0) cfg->filter_value = atoi(value);
                else if (strcmp(key, "digits") == 0) cfg->digits = atoi(value);
                else if (strcmp(key, "nullval") == 0) cfg->nullval = (float)atof(value);
                else if (strcmp(key, "null_enabled") == 0) cfg->null_enabled = atoi(value);
                else if (strcmp(key, "data_format") == 0) cfg->data_format = atoi(value);
                else if (strcmp(key, "buffer_enabled") == 0) cfg->buffer_enabled = atoi(value);
                else if (strcmp(key, "buffer_size") == 0) cfg->buffer_size = atoi(value);
                else if (strcmp(key, "min_max_enabled") == 0) cfg->min_max_enabled = atoi(value);
                else if (strcmp(key, "min_value") == 0) cfg->min_value = (float)atof(value);
                else if (strcmp(key, "max_value") == 0) cfg->max_value = (float)atof(value);
                else if (strcmp(key, "sample_count") == 0) cfg->sample_count = atoi(value);
                else if (strcmp(key, "burst_mode") == 0) cfg->burst_mode = atoi(value);
                else if (strcmp(key, "sample_rate") == 0) cfg->sample_rate = (float)atof(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load DM5010 configuration from file */
void load_dm5010_config(FILE *fp, int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "function") == 0) strcpy(cfg->function, value);
                else if (strcmp(key, "range_mode") == 0) cfg->range_mode = atoi(value);
                else if (strcmp(key, "filter_enabled") == 0) cfg->filter_enabled = atoi(value);
                else if (strcmp(key, "filter_count") == 0) cfg->filter_count = atoi(value);
                else if (strcmp(key, "trigger_mode") == 0) cfg->trigger_mode = atoi(value);
                else if (strcmp(key, "digits") == 0) cfg->digits = atoi(value);
                else if (strcmp(key, "nullval") == 0) cfg->nullval = (float)atof(value);
                else if (strcmp(key, "null_enabled") == 0) cfg->null_enabled = atoi(value);
                else if (strcmp(key, "data_format") == 0) cfg->data_format = atoi(value);
                else if (strcmp(key, "auto_zero") == 0) cfg->auto_zero = atoi(value);
                else if (strcmp(key, "calculation_mode") == 0) cfg->calculation_mode = atoi(value);
                else if (strcmp(key, "scale_factor") == 0) cfg->scale_factor = (float)atof(value);
                else if (strcmp(key, "scale_offset") == 0) cfg->scale_offset = (float)atof(value);
                else if (strcmp(key, "avg_count") == 0) cfg->avg_count = atoi(value);
                else if (strcmp(key, "dbm_reference") == 0) cfg->dbm_reference = (float)atof(value);
                else if (strcmp(key, "dbr_reference") == 0) cfg->dbr_reference = (float)atof(value);
                else if (strcmp(key, "beeper_enabled") == 0) cfg->beeper_enabled = atoi(value);
                else if (strcmp(key, "front_panel_lock") == 0) cfg->front_panel_lock = atoi(value);
                else if (strcmp(key, "high_speed_mode") == 0) cfg->high_speed_mode = atoi(value);
                else if (strcmp(key, "statistics_enabled") == 0) cfg->statistics_enabled = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load PS5004 configuration from file */
void load_ps5004_config(FILE *fp, int slot) {
    ps5004_config *cfg = &g_ps5004_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "voltage") == 0) cfg->voltage = (float)atof(value);
                else if (strcmp(key, "current_limit") == 0) cfg->current_limit = (float)atof(value);
                else if (strcmp(key, "output_enabled") == 0) cfg->output_enabled = atoi(value);
                else if (strcmp(key, "display_mode") == 0) cfg->display_mode = atoi(value);
                else if (strcmp(key, "vri_enabled") == 0) cfg->vri_enabled = atoi(value);
                else if (strcmp(key, "cri_enabled") == 0) cfg->cri_enabled = atoi(value);
                else if (strcmp(key, "uri_enabled") == 0) cfg->uri_enabled = atoi(value);
                else if (strcmp(key, "dt_enabled") == 0) cfg->dt_enabled = atoi(value);
                else if (strcmp(key, "user_enabled") == 0) cfg->user_enabled = atoi(value);
                else if (strcmp(key, "rqs_enabled") == 0) cfg->rqs_enabled = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load PS5010 configuration from file */
void load_ps5010_config(FILE *fp, int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "voltage1") == 0) cfg->voltage1 = (float)atof(value);
                else if (strcmp(key, "current_limit1") == 0) cfg->current_limit1 = (float)atof(value);
                else if (strcmp(key, "output1_enabled") == 0) cfg->output1_enabled = atoi(value);
                else if (strcmp(key, "voltage2") == 0) cfg->voltage2 = (float)atof(value);
                else if (strcmp(key, "current_limit2") == 0) cfg->current_limit2 = (float)atof(value);
                else if (strcmp(key, "output2_enabled") == 0) cfg->output2_enabled = atoi(value);
                else if (strcmp(key, "logic_voltage") == 0) cfg->logic_voltage = (float)atof(value);
                else if (strcmp(key, "logic_current_limit") == 0) cfg->logic_current_limit = (float)atof(value);
                else if (strcmp(key, "logic_enabled") == 0) cfg->logic_enabled = atoi(value);
                else if (strcmp(key, "tracking_mode") == 0) cfg->tracking_mode = atoi(value);
                else if (strcmp(key, "tracking_ratio") == 0) cfg->tracking_ratio = (float)atof(value);
                else if (strcmp(key, "display_channel") == 0) cfg->display_channel = atoi(value);
                else if (strcmp(key, "display_mode") == 0) cfg->display_mode = atoi(value);
                else if (strcmp(key, "ovp_enabled") == 0) cfg->ovp_enabled = atoi(value);
                else if (strcmp(key, "ovp_level1") == 0) cfg->ovp_level1 = (float)atof(value);
                else if (strcmp(key, "ovp_level2") == 0) cfg->ovp_level2 = (float)atof(value);
                else if (strcmp(key, "cv_mode1") == 0) cfg->cv_mode1 = atoi(value);
                else if (strcmp(key, "cc_mode1") == 0) cfg->cc_mode1 = atoi(value);
                else if (strcmp(key, "cv_mode2") == 0) cfg->cv_mode2 = atoi(value);
                else if (strcmp(key, "cc_mode2") == 0) cfg->cc_mode2 = atoi(value);
                else if (strcmp(key, "srq_enabled") == 0) cfg->srq_enabled = atoi(value);
                else if (strcmp(key, "error_queue_size") == 0) cfg->error_queue_size = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load DC5009 configuration from file */
void load_dc5009_config(FILE *fp, int slot) {
    dc5009_config *cfg = &g_dc5009_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "function") == 0) strcpy(cfg->function, value);
                else if (strcmp(key, "channel") == 0) strcpy(cfg->channel, value);
                else if (strcmp(key, "gate_time") == 0) cfg->gate_time = (float)atof(value);
                else if (strcmp(key, "averaging") == 0) cfg->averaging = atoi(value);
                else if (strcmp(key, "coupling_a") == 0) strcpy(cfg->coupling_a, value);
                else if (strcmp(key, "coupling_b") == 0) strcpy(cfg->coupling_b, value);
                else if (strcmp(key, "impedance_a") == 0) strcpy(cfg->impedance_a, value);
                else if (strcmp(key, "impedance_b") == 0) strcpy(cfg->impedance_b, value);
                else if (strcmp(key, "attenuation_a") == 0) strcpy(cfg->attenuation_a, value);
                else if (strcmp(key, "attenuation_b") == 0) strcpy(cfg->attenuation_b, value);
                else if (strcmp(key, "slope_a") == 0) strcpy(cfg->slope_a, value);
                else if (strcmp(key, "slope_b") == 0) strcpy(cfg->slope_b, value);
                else if (strcmp(key, "level_a") == 0) cfg->level_a = (float)atof(value);
                else if (strcmp(key, "level_b") == 0) cfg->level_b = (float)atof(value);
                else if (strcmp(key, "filter_enabled") == 0) cfg->filter_enabled = atoi(value);
                else if (strcmp(key, "auto_trigger") == 0) cfg->auto_trigger = atoi(value);
                else if (strcmp(key, "overflow_enabled") == 0) cfg->overflow_enabled = atoi(value);
                else if (strcmp(key, "preset_enabled") == 0) cfg->preset_enabled = atoi(value);
                else if (strcmp(key, "srq_enabled") == 0) cfg->srq_enabled = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load DC5010 configuration from file */
void load_dc5010_config(FILE *fp, int slot) {
    dc5010_config *cfg = &g_dc5010_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "function") == 0) strcpy(cfg->function, value);
                else if (strcmp(key, "channel") == 0) strcpy(cfg->channel, value);
                else if (strcmp(key, "gate_time") == 0) cfg->gate_time = (float)atof(value);
                else if (strcmp(key, "averaging") == 0) cfg->averaging = atoi(value);
                else if (strcmp(key, "coupling_a") == 0) strcpy(cfg->coupling_a, value);
                else if (strcmp(key, "coupling_b") == 0) strcpy(cfg->coupling_b, value);
                else if (strcmp(key, "impedance_a") == 0) strcpy(cfg->impedance_a, value);
                else if (strcmp(key, "impedance_b") == 0) strcpy(cfg->impedance_b, value);
                else if (strcmp(key, "attenuation_a") == 0) strcpy(cfg->attenuation_a, value);
                else if (strcmp(key, "attenuation_b") == 0) strcpy(cfg->attenuation_b, value);
                else if (strcmp(key, "slope_a") == 0) strcpy(cfg->slope_a, value);
                else if (strcmp(key, "slope_b") == 0) strcpy(cfg->slope_b, value);
                else if (strcmp(key, "level_a") == 0) cfg->level_a = (float)atof(value);
                else if (strcmp(key, "level_b") == 0) cfg->level_b = (float)atof(value);
                else if (strcmp(key, "filter_enabled") == 0) cfg->filter_enabled = atoi(value);
                else if (strcmp(key, "auto_trigger") == 0) cfg->auto_trigger = atoi(value);
                else if (strcmp(key, "overflow_enabled") == 0) cfg->overflow_enabled = atoi(value);
                else if (strcmp(key, "preset_enabled") == 0) cfg->preset_enabled = atoi(value);
                else if (strcmp(key, "srq_enabled") == 0) cfg->srq_enabled = atoi(value);
                else if (strcmp(key, "rise_fall_enabled") == 0) cfg->rise_fall_enabled = atoi(value);
                else if (strcmp(key, "burst_mode") == 0) cfg->burst_mode = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}

/* Load FG5010 configuration from file */
void load_fg5010_config(FILE *fp, int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    char line[GPIB_BUFFER_SIZE];
    char key[64], value[64];
    char *token;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Slot", 4) == 0 || strncmp(line, "GlobalData:", 11) == 0) {
            /* End of this module's config - don't use unreliable fseek */
            break;
        }
        
        token = strtok(line, "|");
        while (token) {
            if (sscanf(token, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "frequency") == 0) cfg->frequency = (float)atof(value);
                else if (strcmp(key, "amplitude") == 0) cfg->amplitude = (float)atof(value);
                else if (strcmp(key, "offset") == 0) cfg->offset = (float)atof(value);
                else if (strcmp(key, "waveform") == 0) strcpy(cfg->waveform, value);
                else if (strcmp(key, "output_enabled") == 0) cfg->output_enabled = atoi(value);
                else if (strcmp(key, "duty_cycle") == 0) cfg->duty_cycle = (float)atof(value);
                else if (strcmp(key, "sweep_enabled") == 0) cfg->sweep_enabled = atoi(value);
                else if (strcmp(key, "start_freq") == 0) cfg->start_freq = (float)atof(value);
                else if (strcmp(key, "stop_freq") == 0) cfg->stop_freq = (float)atof(value);
                else if (strcmp(key, "sweep_time") == 0) cfg->sweep_time = (float)atof(value);
                else if (strcmp(key, "sweep_type") == 0) cfg->sweep_type = atoi(value);
                else if (strcmp(key, "trigger_source") == 0) strcpy(cfg->trigger_source, value);
                else if (strcmp(key, "trigger_slope") == 0) strcpy(cfg->trigger_slope, value);
                else if (strcmp(key, "trigger_level") == 0) cfg->trigger_level = (float)atof(value);
                else if (strcmp(key, "sync_enabled") == 0) cfg->sync_enabled = atoi(value);
                else if (strcmp(key, "invert_enabled") == 0) cfg->invert_enabled = atoi(value);
                else if (strcmp(key, "phase") == 0) cfg->phase = (float)atof(value);
                else if (strcmp(key, "modulation_enabled") == 0) cfg->modulation_enabled = atoi(value);
                else if (strcmp(key, "mod_type") == 0) strcpy(cfg->mod_type, value);
                else if (strcmp(key, "mod_freq") == 0) cfg->mod_freq = (float)atof(value);
                else if (strcmp(key, "mod_depth") == 0) cfg->mod_depth = (float)atof(value);
                else if (strcmp(key, "burst_enabled") == 0) cfg->burst_enabled = atoi(value);
                else if (strcmp(key, "burst_count") == 0) cfg->burst_count = atoi(value);
                else if (strcmp(key, "burst_period") == 0) cfg->burst_period = (float)atof(value);
                else if (strcmp(key, "units_freq") == 0) cfg->units_freq = atoi(value);
                else if (strcmp(key, "units_time") == 0) cfg->units_time = atoi(value);
                else if (strcmp(key, "lf_termination") == 0) cfg->lf_termination = atoi(value);
            }
            token = strtok(NULL, "|");
        }
    }
}
