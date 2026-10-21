/*
 * Delimited-text export (CSV/TSV/scientific) with optional metadata headers,
 * plus a streaming mode that appends one row per reading.
 */

#include "data.h"
#include "modules.h"

realtime_export_state g_realtime_export = {{0}, "", NULL, 0, 0, 0, 0};
export_statistics g_export_stats = {0, 0, 0, 0, 0, 0.0};

/* One row per sample index across all enabled slots; short buffers pad with 0. */
int export_data_enhanced(char *filename, export_config *config) {
    FILE *file;
    time_t start_time, end_time;
    int i, j;
    char timestamp_str[32];
    char value_str[32];
    unsigned long total_samples = 0;
    unsigned long file_size = 0;
    int enabled_modules[10];
    int enabled_count = 0;
    
    if (validate_export_config(config) != EXPORT_SUCCESS) {
        return EXPORT_ERROR_INVALID_CONFIG;
    }

    /* Row count is the longest buffer */
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && g_system->modules[i].module_data_count > 0) {
            enabled_modules[enabled_count++] = i;
            if (g_system->modules[i].module_data_count > total_samples) {
                total_samples = g_system->modules[i].module_data_count;
            }
        }
    }
    
    if (enabled_count == 0 || total_samples == 0) {
        return EXPORT_ERROR_NO_DATA;
    }
    
    start_time = time(NULL);
    
    file = fopen(filename, "w");
    if (!file) {
        return EXPORT_ERROR_FILE_CREATE;
    }

    if (config->flags & EXPORT_FLAG_METADATA) {
        if (export_system_metadata(file, config) != EXPORT_SUCCESS) {
            fclose(file);
            return EXPORT_ERROR_FILE_WRITE;
        }
    }

    if (config->flags & EXPORT_FLAG_SETTINGS) {
        if (export_instrument_settings(file, config) != EXPORT_SUCCESS) {
            fclose(file);
            return EXPORT_ERROR_FILE_WRITE;
        }
    }

    if (config->flags & EXPORT_FLAG_METADATA) {
        if (export_measurement_conditions(file, config) != EXPORT_SUCCESS) {
            fclose(file);
            return EXPORT_ERROR_FILE_WRITE;
        }
    }

    if (config->flags & EXPORT_FLAG_HEADERS) {
        if (strlen(config->custom_header) > 0) {
            fprintf(file, "# %s\n", config->custom_header);
        }

        if (config->flags & EXPORT_FLAG_TIMESTAMPS) {
            fprintf(file, "Timestamp%c", config->delimiter);
        }
        fprintf(file, "Sample");
        
        for (i = 0; i < enabled_count; i++) {
            int slot = enabled_modules[i];
            fprintf(file, "%cSlot_%d_%s", config->delimiter, slot, 
                   g_system->modules[slot].description);
            
            if (strlen(config->units_override) > 0) {
                fprintf(file, "_%s", config->units_override);
            } else {
                fprintf(file, "_V"); /* Default to volts */
            }
        }

        /* Placeholder columns: no sensor is read; rows carry the constants below. */
        if (config->flags & EXPORT_FLAG_METADATA) {
            fprintf(file, "%cTemperature_C%cHumidity_%%", config->delimiter, config->delimiter);
        }
        
        fprintf(file, "\n");
    }
    
    for (i = 0; i < total_samples; i++) {
        /* Reconstructed from start time + nominal sample rate, not recorded per sample */
        if (config->flags & EXPORT_FLAG_TIMESTAMPS) {
            time_t sample_time = config->export_start_time + 
                               (i * (g_control_panel.sample_rate_ms / 1000.0));
            format_timestamp(timestamp_str, sizeof(timestamp_str), sample_time, config);
            fprintf(file, "%s%c", timestamp_str, config->delimiter);
        }
        
        fprintf(file, "%d", i);

        for (j = 0; j < enabled_count; j++) {
            int slot = enabled_modules[j];
            float value = 0.0;

            if (i < g_system->modules[slot].module_data_count) {
                value = g_system->modules[slot].module_data[i];
            }

            format_data_value(value_str, sizeof(value_str), value, config);
            fprintf(file, "%c%s", config->delimiter, value_str);
        }

        /* Fixed placeholder temperature/humidity */
        if (config->flags & EXPORT_FLAG_METADATA) {
            fprintf(file, "%c23.5%c45.2", config->delimiter, config->delimiter);
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);

    file_size = get_export_file_size(filename);

    if (config->flags & EXPORT_FLAG_COMPRESS) {
        if (compress_export_file(filename) != EXPORT_SUCCESS) {
            /* Compression failure does not fail the export */
        }
    }

    end_time = time(NULL);
    update_export_statistics(total_samples, file_size, (float)(end_time - start_time));
    
    return EXPORT_SUCCESS;
}

/* Opens the stream file; rows are appended by update_realtime_export. */
int start_realtime_export(char *filename_template, export_config *config) {
    char actual_filename[80];
    time_t current_time;

    if (g_realtime_export.active) {
        stop_realtime_export();
    }

    if (validate_export_config(config) != EXPORT_SUCCESS) {
        return EXPORT_ERROR_INVALID_CONFIG;
    }

    current_time = time(NULL);
    if (generate_filename_from_template(actual_filename, sizeof(actual_filename), 
                                       filename_template, current_time) != EXPORT_SUCCESS) {
        return EXPORT_ERROR_TEMPLATE;
    }
    
    g_realtime_export.file = fopen(actual_filename, "w");
    if (!g_realtime_export.file) {
        return EXPORT_ERROR_FILE_CREATE;
    }

    memcpy(&g_realtime_export.config, config, sizeof(export_config));
    g_realtime_export.samples_exported = 0;
    g_realtime_export.session_start = current_time;
    g_realtime_export.active = 1;
    g_realtime_export.error_count = 0;
    strncpy(g_realtime_export.current_filename, actual_filename, 
            sizeof(g_realtime_export.current_filename) - 1);
    
    if (config->flags & EXPORT_FLAG_HEADERS) {
        export_system_metadata(g_realtime_export.file, config);

        if (config->flags & EXPORT_FLAG_TIMESTAMPS) {
            fprintf(g_realtime_export.file, "Timestamp%c", config->delimiter);
        }
        fprintf(g_realtime_export.file, "Sample%cSlot%cValue", 
                config->delimiter, config->delimiter);
        
        if (strlen(config->units_override) > 0) {
            fprintf(g_realtime_export.file, "_%s", config->units_override);
        }
        
        fprintf(g_realtime_export.file, "\n");
        fflush(g_realtime_export.file);
    }
    
    return EXPORT_SUCCESS;
}

int update_realtime_export(int slot, float value, time_t timestamp) {
    char timestamp_str[32];
    char value_str[32];

    if (!g_realtime_export.active || !g_realtime_export.file) {
        return EXPORT_ERROR_REALTIME;
    }

    if (g_realtime_export.config.flags & EXPORT_FLAG_TIMESTAMPS) {
        format_timestamp(timestamp_str, sizeof(timestamp_str), timestamp, 
                        &g_realtime_export.config);
        fprintf(g_realtime_export.file, "%s%c", timestamp_str, 
                g_realtime_export.config.delimiter);
    }
    
    format_data_value(value_str, sizeof(value_str), value, &g_realtime_export.config);
    fprintf(g_realtime_export.file, "%lu%c%d%c%s\n", 
            g_realtime_export.samples_exported, 
            g_realtime_export.config.delimiter, slot,
            g_realtime_export.config.delimiter, value_str);
    
    /* Flush every row so a crash or power loss keeps what was captured */
    fflush(g_realtime_export.file);
    
    g_realtime_export.samples_exported++;
    
    return EXPORT_SUCCESS;
}

int stop_realtime_export(void) {
    if (!g_realtime_export.active) {
        return EXPORT_ERROR_REALTIME;
    }

    if (g_realtime_export.file) {
        fprintf(g_realtime_export.file, "# Export completed: %lu samples exported\n", 
                g_realtime_export.samples_exported);
        fprintf(g_realtime_export.file, "# Session duration: %ld seconds\n", 
                time(NULL) - g_realtime_export.session_start);
        
        fclose(g_realtime_export.file);
        g_realtime_export.file = NULL;
    }
    
    if (g_realtime_export.config.flags & EXPORT_FLAG_COMPRESS) {
        compress_export_file(g_realtime_export.current_filename);
    }

    update_export_statistics(g_realtime_export.samples_exported, 
                           get_export_file_size(g_realtime_export.current_filename),
                           (float)(time(NULL) - g_realtime_export.session_start));

    g_realtime_export.active = 0;
    g_realtime_export.samples_exported = 0;
    g_realtime_export.current_filename[0] = '\0';
    
    return EXPORT_SUCCESS;
}

int export_system_metadata(FILE *file, export_config *config) {
    time_t current_time;
    char time_str[32];
    
    if (!file) {
        return EXPORT_ERROR_FILE_WRITE;
    }
    
    current_time = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));
    
    fprintf(file, "# TM5000 Measurement Data Export\n");
    fprintf(file, "# Generated: %s\n", time_str);
    fprintf(file, "# System: TM5000 v%s\n", TM5000_VERSION);
    fprintf(file, "# Export Format: ");
    
    switch (config->format) {
        case EXPORT_FORMAT_CSV: fprintf(file, "CSV\n"); break;
        case EXPORT_FORMAT_TSV: fprintf(file, "TSV\n"); break;
        case EXPORT_FORMAT_SCIENTIFIC: fprintf(file, "Scientific CSV\n"); break;
        default: fprintf(file, "Custom\n"); break;
    }
    
    fprintf(file, "# Precision: %d decimal places\n", config->precision);
    fprintf(file, "#\n");
    
    return EXPORT_SUCCESS;
}

int export_instrument_settings(FILE *file, export_config *config) {
    int i;
    int found_instruments = 0;
    
    if (!file) {
        return EXPORT_ERROR_FILE_WRITE;
    }
    
    fprintf(file, "# Instrument Configuration:\n");
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            fprintf(file, "# Slot %d: %s, GPIB Address %d\n", 
                   i, g_system->modules[i].description, 
                   g_system->modules[i].gpib_address);
            found_instruments = 1;
        }
    }
    
    if (!found_instruments) {
        fprintf(file, "# No instruments configured\n");
    }
    
    fprintf(file, "#\n");
    
    return EXPORT_SUCCESS;
}

int export_measurement_conditions(FILE *file, export_config *config) {
    float actual_sample_rate;
    
    if (!file) {
        return EXPORT_ERROR_FILE_WRITE;
    }
    
    actual_sample_rate = (config->sample_rate_override > 0.0) ? 
                        config->sample_rate_override : 
                        (1000.0 / g_control_panel.sample_rate_ms);
    
    fprintf(file, "# Measurement Parameters:\n");
    fprintf(file, "# Sample Rate: %.3f Hz (%.0f ms intervals)\n", 
           actual_sample_rate, 1000.0 / actual_sample_rate);
    fprintf(file, "# Graph Scale: %.6f to %.6f V\n", 
           g_graph_scale.min_value, g_graph_scale.max_value);
    fprintf(file, "# Auto-scale: %s\n", g_graph_scale.auto_scale ? "ON" : "OFF");
    fprintf(file, "#\n");
    
    return EXPORT_SUCCESS;
}

int format_data_value(char *buffer, int buffer_size, float value, export_config *config) {
    if (!buffer || buffer_size < 32) {
        return EXPORT_ERROR_MEMORY;
    }
    
    if (config->flags & EXPORT_FLAG_SCIENTIFIC) {
        return format_scientific_notation(buffer, buffer_size, value, config->precision);
    } else {
        snprintf(buffer, buffer_size, "%.*f", config->precision, value);
    }
    
    return EXPORT_SUCCESS;
}

/* Always "YYYY-MM-DD HH:MM:SS"; config is not consulted. */
int format_timestamp(char *buffer, int buffer_size, time_t timestamp, export_config *config) {
    struct tm *tm_info;
    
    if (!buffer || buffer_size < 32) {
        return EXPORT_ERROR_MEMORY;
    }
    
    tm_info = localtime(&timestamp);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
    
    return EXPORT_SUCCESS;
}

int format_scientific_notation(char *buffer, int buffer_size, float value, int precision) {
    int exponent = 0;
    float mantissa = value;
    
    if (!buffer || buffer_size < 32) {
        return EXPORT_ERROR_MEMORY;
    }
    
    if (value == 0.0) {
        snprintf(buffer, buffer_size, "%.*fE+00", precision, 0.0);
        return EXPORT_SUCCESS;
    }
    
    /* Normalize mantissa to [1, 10) */
    if (fabs(value) >= 1.0) {
        while (fabs(mantissa) >= 10.0) {
            mantissa /= 10.0;
            exponent++;
        }
    } else {
        while (fabs(mantissa) < 1.0) {
            mantissa *= 10.0;
            exponent--;
        }
    }
    
    snprintf(buffer, buffer_size, "%.*fE%+03d", precision, mantissa, exponent);
    
    return EXPORT_SUCCESS;
}

/* Expands %Y %M %D %H %N %S (N is minute, since M is month); others pass through. */
int generate_filename_from_template(char *output, int output_size, char *template, time_t timestamp) {
    struct tm *tm_info;
    char temp_buffer[128];
    char *src, *dst;
    
    if (!output || !template || output_size < 64) {
        return EXPORT_ERROR_TEMPLATE;
    }
    
    tm_info = localtime(&timestamp);
    src = template;
    dst = temp_buffer;
    
    while (*src && (dst - temp_buffer) < sizeof(temp_buffer) - 20) {
        if (*src == '%' && *(src + 1)) {
            src++; /* Skip % */
            switch (*src) {
                case 'Y': /* Year (4 digits) */
                    dst += sprintf(dst, "%04d", tm_info->tm_year + 1900);
                    break;
                case 'M': /* Month (2 digits) */
                    dst += sprintf(dst, "%02d", tm_info->tm_mon + 1);
                    break;
                case 'D': /* Day (2 digits) */
                    dst += sprintf(dst, "%02d", tm_info->tm_mday);
                    break;
                case 'H': /* Hour (2 digits) */
                    dst += sprintf(dst, "%02d", tm_info->tm_hour);
                    break;
                case 'N': /* Minute (2 digits) */
                    dst += sprintf(dst, "%02d", tm_info->tm_min);
                    break;
                case 'S': /* Second (2 digits) */
                    dst += sprintf(dst, "%02d", tm_info->tm_sec);
                    break;
                default:
                    *dst++ = '%';
                    *dst++ = *src;
                    break;
            }
        } else {
            *dst++ = *src;
        }
        src++;
    }
    
    *dst = '\0';
    
    strncpy(output, temp_buffer, output_size - 1);
    output[output_size - 1] = '\0';
    
    return EXPORT_SUCCESS;
}

/* Also fills in a default delimiter from the format when none is set. */
int validate_export_config(export_config *config) {
    if (!config) {
        return EXPORT_ERROR_INVALID_CONFIG;
    }

    if (config->format > EXPORT_FORMAT_CUSTOM) {
        return EXPORT_ERROR_INVALID_CONFIG;
    }

    if (config->precision < 1 || config->precision > 9) {
        return EXPORT_ERROR_INVALID_CONFIG;
    }

    if (config->delimiter == '\0') {
        switch (config->format) {
            case EXPORT_FORMAT_TSV:
                config->delimiter = '\t';
                break;
            default:
                config->delimiter = ',';
                break;
        }
    }
    
    return EXPORT_SUCCESS;
}

long get_export_file_size(char *filename) {
    FILE *file;
    long size;
    
    if (!filename) {
        return 0;
    }
    
    file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }
    
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fclose(file);

    return (size < 0) ? 0 : size;
}

int update_export_statistics(unsigned long samples, unsigned long bytes, float duration) {
    g_export_stats.total_exports++;
    g_export_stats.total_samples_exported += samples;
    g_export_stats.total_bytes_exported += bytes;
    g_export_stats.last_export_time = time(NULL);

    /* Running mean of export duration */
    if (g_export_stats.total_exports > 1) {
        g_export_stats.average_export_time = 
            ((g_export_stats.average_export_time * (g_export_stats.total_exports - 1)) + duration) / 
            g_export_stats.total_exports;
    } else {
        g_export_stats.average_export_time = duration;
    }
    
    return EXPORT_SUCCESS;
}

/* Stub: no compression is performed; always reports success. */
int compress_export_file(char *filename) {
    return EXPORT_SUCCESS;
}

int get_export_statistics(export_statistics *stats) {
    if (!stats) {
        return EXPORT_ERROR_MEMORY;
    }
    
    memcpy(stats, &g_export_stats, sizeof(export_statistics));
    return EXPORT_SUCCESS;
}

int reset_export_statistics(void) {
    memset(&g_export_stats, 0, sizeof(export_statistics));
    return EXPORT_SUCCESS;
}