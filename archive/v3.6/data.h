/*
 * TM5000 GPIB Control System - Data Management
 * Version 3.5
 * Header file for data management functions
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Fixed load/save module activation issues
 * 3.3 - Fixed fseek reliability issues
 * 3.5 - Enhanced data export with metadata and real-time streaming
 */

#ifndef DATA_H
#define DATA_H

#include "tm5000.h"

/* Data buffer management */
int allocate_module_buffer(int slot, unsigned int size);
void free_module_buffer(int slot);
void store_module_data(int slot, float value);
void clear_module_data(int slot);

/* File I/O operations */
void save_data(void);
void load_data(void);
void save_settings(void);
void load_settings(void);

/* Module configuration save/load functions */
void save_dm5120_config(FILE *fp, int slot);
void save_dm5010_config(FILE *fp, int slot);
void save_ps5004_config(FILE *fp, int slot);
void save_ps5010_config(FILE *fp, int slot);
void save_dc5009_config(FILE *fp, int slot);
void save_dc5010_config(FILE *fp, int slot);
void save_fg5010_config(FILE *fp, int slot);

void load_dm5120_config(FILE *fp, int slot);
void load_dm5010_config(FILE *fp, int slot);
void load_ps5004_config(FILE *fp, int slot);
void load_ps5010_config(FILE *fp, int slot);
void load_dc5009_config(FILE *fp, int slot);
void load_dc5010_config(FILE *fp, int slot);
void load_fg5010_config(FILE *fp, int slot);

/* Enhanced Export System (v3.5) */

/* Export format types */
#define EXPORT_FORMAT_CSV           0
#define EXPORT_FORMAT_TSV           1
#define EXPORT_FORMAT_SCIENTIFIC    2
#define EXPORT_FORMAT_CUSTOM        3

/* Export option flags */
#define EXPORT_FLAG_METADATA        0x01
#define EXPORT_FLAG_TIMESTAMPS      0x02
#define EXPORT_FLAG_SETTINGS        0x04
#define EXPORT_FLAG_REALTIME        0x08
#define EXPORT_FLAG_COMPRESS        0x10
#define EXPORT_FLAG_SCIENTIFIC      0x20
#define EXPORT_FLAG_HEADERS         0x40
#define EXPORT_FLAG_STATISTICS      0x80

/* Enhanced Export Configuration - optimized member ordering and bit fields */
#pragma pack(1)
typedef struct {
    char custom_header[128];                 /* 128 bytes - largest member first */
    char filename_template[64];              /* 64 bytes - second largest */
    char units_override[16];                 /* 16 bytes */
    time_t export_start_time;                /* 4 bytes - For timestamping */
    float sample_rate_override;              /* 4 bytes - Override auto-detected sample rate */
    int precision;                           /* 4 bytes - Decimal places (1-9) */
    unsigned char format;                    /* 1 byte - Export format type */
    unsigned char flags;                     /* 1 byte - Option flags */
    char delimiter;                          /* 1 byte - Custom delimiter character */
} export_config;
#pragma pack()

/* Real-time Export State - optimized member ordering */
#pragma pack(1)
typedef struct {
    export_config config;                   /* Large structure - first */
    char current_filename[128];              /* 128 bytes - largest simple member */
    FILE *file;                              /* 4 bytes - Output file handle */
    unsigned long samples_exported;          /* 4 bytes - Counter for exported samples */
    time_t session_start;                    /* 4 bytes - Session start timestamp */
    unsigned char active:1;                  /* 1 bit - active flag */
    unsigned char reserved:7;                /* 7 bits - reserved */
    unsigned char error_count;               /* 1 byte - Error counter */
} realtime_export_state;
#pragma pack()

/* Global real-time export state */
extern realtime_export_state g_realtime_export;

/* Enhanced Export Functions */
int export_data_enhanced(char *filename, export_config *config);
int export_data_with_metadata(char *filename, export_config *config);
int export_measurement_summary(char *filename, export_config *config);

/* Real-time Export Functions */
int start_realtime_export(char *filename_template, export_config *config);
int update_realtime_export(int slot, float value, time_t timestamp);
int stop_realtime_export(void);
int pause_realtime_export(void);
int resume_realtime_export(void);
int get_realtime_export_status(void);

/* Metadata Export Functions */
int export_system_metadata(FILE *file, export_config *config);
int export_instrument_settings(FILE *file, export_config *config);
int export_measurement_conditions(FILE *file, export_config *config);
int export_calibration_info(FILE *file, export_config *config);

/* Data Format Functions */
int format_data_value(char *buffer, int buffer_size, float value, export_config *config);
int format_timestamp(char *buffer, int buffer_size, time_t timestamp, export_config *config);
int format_scientific_notation(char *buffer, int buffer_size, float value, int precision);
int generate_filename_from_template(char *output, int output_size, char *template, time_t timestamp);

/* Compression Functions */
int compress_export_file(char *filename);
int estimate_compressed_size(char *filename);

/* Export Validation and Utilities */
int validate_export_config(export_config *config);
int get_export_file_size(char *filename);
int get_estimated_export_size(export_config *config);
int cleanup_export_temp_files(void);

/* Export Statistics - optimized member ordering */
#pragma pack(1)
typedef struct {
    unsigned long total_exports;             /* 4 bytes - largest members first */
    unsigned long total_samples_exported;    /* 4 bytes */
    unsigned long total_bytes_exported;      /* 4 bytes */
    time_t last_export_time;                 /* 4 bytes */
    float average_export_time;               /* 4 bytes */
    unsigned char most_used_format;          /* 1 byte - smallest last */
} export_statistics;
#pragma pack()

extern export_statistics g_export_stats;

int get_export_statistics(export_statistics *stats);
int reset_export_statistics(void);
int update_export_statistics(unsigned long samples, unsigned long bytes, float duration);

/* Export Error Codes */
#define EXPORT_SUCCESS              0
#define EXPORT_ERROR_FILE_CREATE    1
#define EXPORT_ERROR_FILE_WRITE     2
#define EXPORT_ERROR_INVALID_CONFIG 3
#define EXPORT_ERROR_NO_DATA        4
#define EXPORT_ERROR_MEMORY         5
#define EXPORT_ERROR_REALTIME       6
#define EXPORT_ERROR_COMPRESSION    7
#define EXPORT_ERROR_TEMPLATE       8

#endif /* DATA_H */
