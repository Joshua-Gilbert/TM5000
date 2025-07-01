/*
 * TM5000 GPIB Control System - Data Management Module
 * Version 3.1
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
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
            fprintf(fp, "%d,%d,%d,%s,%u\n", 
                    i,
                    g_system->modules[i].module_type,
                    g_system->modules[i].gpib_address,
                    g_system->modules[i].description,
                    g_system->modules[i].module_data_count);
            total_module_samples += g_system->modules[i].module_data_count;
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
        if (g_system->modules[i].enabled && 
            g_system->modules[i].module_data &&
            g_system->modules[i].module_data_count > 0) {
            
            fprintf(fp, "Slot%d:%u\n", i, g_system->modules[i].module_data_count);
            
            for (j = 0; j < g_system->modules[i].module_data_count; j++) {
                fprintf(fp, "%.6e\n", g_system->modules[i].module_data[j]);
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
    long file_pos;  
    unsigned int count;
    int i, j;
    int is_new_format = 0;
    unsigned int module_count;
    float value;
    int total_loaded = 0;
    
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
    
    /* Check file format version */
    fgets(line, sizeof(line), fp);
    
    if (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "FileFormat: ModuleData")) {
            is_new_format = 1;
        } else {
            rewind(fp);
            fgets(line, sizeof(line), fp);  /* Skip header again */
        }
    }
    
    /* Clear existing data */
    for (i = 0; i < 10; i++) {
        clear_module_data(i);
        g_system->modules[i].enabled = 0;
    }
    g_system->data_count = 0;
    
    if (is_new_format) {
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
            if (strncmp(line, "GlobalData:", 11) == 0) break;
            
            if (sscanf(line, "%d,%d,%d,%[^,],%u", &slot, &type, &addr, desc, &count) == 5) {
                if (slot >= 0 && slot < 10) {
                    g_system->modules[slot].module_type = type;
                    g_system->modules[slot].gpib_address = addr;
                    g_system->modules[slot].enabled = 1;
                    strcpy(g_system->modules[slot].description, desc);
                    
                    /* Allocate buffer with extra space */
                    if (count > 0) {
                        allocate_module_buffer(slot, count + 100);
                    }
                }
            }
        }
        
        /* Read global data */
        for (i = 0; i < g_system->data_count && i < g_system->buffer_size; i++) {
            if (fscanf(fp, "%f", &g_system->data_buffer[i]) != 1) break;
        }
        
        /* Read per-module data */
        fgets(line, sizeof(line), fp);  /* Skip to ModuleData: */
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "EndOfFile", 9) == 0) break;
            
            if (sscanf(line, "Slot%d:%u", &slot, &module_count) == 2) {
                if (slot >= 0 && slot < 10 && g_system->modules[slot].enabled) {
                    printf("Loading %u samples for slot %d...\n", module_count, slot);
                    
                    for (j = 0; j < module_count; j++) {
                        if (fscanf(fp, "%f", &value) == 1) {
                            /* Safety check for invalid values */
                            if (value != value || value == HUGE_VAL || value == -HUGE_VAL) {
                                printf("Warning: Invalid data value detected in slot %d, skipping...\n", slot);
                                continue;  /* Skip NaN and infinite values */
                            }
                            store_module_data(slot, value);
                            total_loaded++;
                        } else {
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
    } else {
        printf("Loading legacy format file...\n");
        
        /* Read legacy format */
        fscanf(fp, "Samples: %u\n", &g_system->data_count);
        
        /* Try to read sample rate (may not exist in old files) */
        file_pos = ftell(fp);
        if (fscanf(fp, "SampleRateMs: %d\n", &g_control_panel.sample_rate_ms) != 1) {
            fseek(fp, file_pos, SEEK_SET);
            g_control_panel.sample_rate_ms = 500;  /* Default */
        }
        
        /* Read module configuration */
        fgets(line, sizeof(line), fp);  /* Skip "Modules:" */
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Data:", 5) == 0) break;
            
            if (sscanf(line, "%d,%d,%d,%[^\n]", &slot, &type, &addr, desc) >= 3) {
                g_system->modules[slot].module_type = type;
                g_system->modules[slot].gpib_address = addr;
                g_system->modules[slot].enabled = 1;
                if (strlen(desc) > 0) {
                    strcpy(g_system->modules[slot].description, desc);
                }
            }
        }
        
        /* Read global data */
        g_system->data_count = 0;
        while (fscanf(fp, "%f", &value) == 1) {
            /* Safety check for invalid values */
            if (value != value || value == HUGE_VAL || value == -HUGE_VAL) {
                printf("Warning: Invalid data value detected, skipping...\n");
                continue;  /* Skip NaN and infinite values */
            }
            g_system->data_buffer[g_system->data_count] = value;
            g_system->data_count++;
            if (g_system->data_count >= g_system->buffer_size) break;
        }
        
        /* For compatibility, copy global data to first enabled module */
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled) {
                allocate_module_buffer(i, g_system->data_count);
                for (j = 0; j < g_system->data_count; j++) {
                    store_module_data(i, g_system->data_buffer[j]);
                }
                printf("Copied %u samples to slot %d for compatibility\n", 
                       g_system->data_count, i);
                break;
            }
        }
    }
    
    fclose(fp);
    
    /* Display summary */
    printf("\nData loaded successfully!\n");
    printf("File format: %s\n", is_new_format ? "Enhanced (v3.0)" : "Legacy");
    printf("Global samples: %u\n", g_system->data_count);
    printf("Sample rate: %d ms\n", g_control_panel.sample_rate_ms);
    printf("\nModule data loaded:\n");
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && g_system->modules[i].module_data_count > 0) {
            printf("  Slot %d: %u samples\n", i, g_system->modules[i].module_data_count);
        }
    }
    
    printf("\nPress any key to continue...");
    
    /* Synchronize loaded modules with display traces */
    sync_traces_with_modules();
    
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
    char line[256];
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
        
        if (strlen(line) == 0) continue;
        
        if (strcmp(line, "[ControlPanel]") == 0) {
            /* Read control panel settings */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    fseek(fp, -strlen(line), SEEK_CUR);
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
        } else if (strcmp(line, "[Modules]") == 0) {
            /* Clear existing modules - match v2.9 behavior */
            for (slot = 0; slot < 10; slot++) {
                g_system->modules[slot].enabled = 0;
            }
            
            /* Read module configuration */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    fseek(fp, -strlen(line), SEEK_CUR);
                    break;
                }
                if (sscanf(line, "Module%d=%d,%d,%[^\n]", &slot, &type, &addr, desc) == 4) {
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
                        allocate_module_buffer(slot, 1000);
                    }
                }
            }
                   
        } else if (strcmp(line, "[ModuleConfigs]") == 0) {
            /* Read module-specific configurations */
            while (fgets(line, sizeof(line), fp)) {
                if (line[0] == '[') {
                    fseek(fp, -strlen(line), SEEK_CUR);
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
        } else if (strcmp(line, "[GraphScale]") == 0) {
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
    
    printf("\nPress any key to continue...");
    getch();
}
