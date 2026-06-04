/*
 * TM5000 GPIB Control System - Configuration Profiles Module
 * Version 3.5
 * Header file for configuration profile management
 * 
 * This module provides save/load functionality for complete system
 * configurations including all instrument settings, graph display
 * settings, FFT configuration, and control panel state.
 * 
 * Version History:
 * 3.5 - Initial implementation for configuration profile management
 */

#ifndef CONFIG_PROFILES_H
#define CONFIG_PROFILES_H

#include "tm5000.h"

/* Maximum number of configuration profiles per system */
#define MAX_CONFIG_PROFILES 10

/* Profile name and description lengths */
#define PROFILE_NAME_LENGTH 32
#define PROFILE_DESC_LENGTH 64

/* Profile file magic number and version */
#define PROFILE_MAGIC "TM5P"
#define PROFILE_VERSION 0x35

/* Configuration Profile Structure - optimized member ordering and bit fields */
#pragma pack(1)
typedef struct {
    /* Largest instrument configuration arrays first */
    dm5120_config dm5120_configs[10];         /* DM5120 settings - largest first */
    dm5010_config dm5010_configs[10];         /* DM5010 settings */
    ps5010_config ps5010_configs[10];         /* PS5010 settings */
    fg5010_config fg5010_configs[10];         /* FG5010 settings */
    dc5009_config dc5009_configs[10];         /* DC5009 settings */
    dc5010_config dc5010_configs[10];         /* DC5010 settings */
    ps5004_config ps5004_configs[10];         /* PS5004 settings - smallest last */
    
    /* String members - largest first */
    char description[PROFILE_DESC_LENGTH];    /* 64 bytes - User description */
    char name[PROFILE_NAME_LENGTH];           /* 32 bytes - Profile name */
    unsigned char reserved[15];               /* 15 bytes - Future expansion */
    
    /* System configuration structures */
    graph_scale graph_settings;               /* Graph display settings */
    fft_config fft_settings;                 /* FFT configuration */
    control_panel_state panel_state;          /* Control panel state */
    
    /* Module state arrays */
    unsigned char module_enabled[10];         /* Module enable states */
    unsigned char module_types[10];           /* Module type assignments */
    unsigned char gpib_addresses[10];         /* GPIB address assignments */
    
    /* Timestamps and integers */
    time_t created;                           /* 4 bytes - Creation timestamp */
    time_t modified;                          /* 4 bytes - Last modification */
    unsigned short checksum;                  /* 2 bytes - Profile data checksum */
    unsigned char flags;                      /* 1 byte - Configuration flags */
} config_profile;
#pragma pack()

/* Profile file header structure - optimized member ordering */
#pragma pack(1)
typedef struct {
    unsigned char reserved[20];               /* 20 bytes - largest member first */
    char magic[4];                            /* 4 bytes - Magic number "TM5P" */
    time_t created;                           /* 4 bytes - File creation time */
    unsigned short file_checksum;             /* 2 bytes - File integrity checksum */
    unsigned char version;                    /* 1 byte - File format version */
    unsigned char profile_count;              /* 1 byte - Number of profiles in file */
} profile_file_header;
#pragma pack()

/* Profile list entry for UI display - optimized member ordering and bit fields */
#pragma pack(1)
typedef struct {
    char description[PROFILE_DESC_LENGTH];    /* 64 bytes - largest first */
    char name[PROFILE_NAME_LENGTH];           /* 32 bytes */
    time_t modified;                          /* 4 bytes */
    unsigned char valid:1;                    /* 1 bit - valid profile flag */
    unsigned char reserved:7;                 /* 7 bits - reserved */
} profile_list_entry;
#pragma pack()

/* Global profile management state */
extern char g_current_profile_name[PROFILE_NAME_LENGTH];
extern unsigned char g_profile_modified;

/* Core profile management functions */
int init_profile_system(void);
int save_config_profile(char *name, char *description);
int load_config_profile(char *name);
int delete_config_profile(char *name);
int rename_config_profile(char *old_name, char *new_name);

/* Profile listing and information */
int list_config_profiles(profile_list_entry profiles[], int max_count);
int get_profile_info(char *name, config_profile *profile);
int profile_exists(char *name);

/* Profile validation and integrity */
int validate_config_profile(config_profile *profile);
unsigned short calculate_profile_checksum(config_profile *profile);
int verify_profile_integrity(config_profile *profile);

/* Current system state management */
int capture_current_config(config_profile *profile);
int apply_config_profile(config_profile *profile);
int mark_profile_modified(void);
int is_current_profile_modified(void);

/* Profile file operations */
int create_profile_file(void);
int backup_profile_file(void);
int repair_profile_file(void);
int get_profile_file_status(void);

/* Utility functions */
char *get_profile_filename(void);

/* Error codes for profile operations */
#define PROFILE_SUCCESS             0
#define PROFILE_ERROR_NOT_FOUND     1
#define PROFILE_ERROR_EXISTS        2
#define PROFILE_ERROR_INVALID_NAME  3
#define PROFILE_ERROR_FILE_IO       4
#define PROFILE_ERROR_CHECKSUM      5
#define PROFILE_ERROR_FULL          6
#define PROFILE_ERROR_MEMORY        7
#define PROFILE_ERROR_CORRUPT       8

/* Profile operation flags */
#define PROFILE_FLAG_BACKUP_ON_SAVE     0x01
#define PROFILE_FLAG_VALIDATE_STRICT    0x02
#define PROFILE_FLAG_AUTO_TIMESTAMP     0x04
#define PROFILE_FLAG_COMPRESS_DATA      0x08

#endif /* CONFIG_PROFILES_H */