/*
 * TM5000 configuration profiles: save/load complete system setups (instrument
 * configs, graph, FFT, control panel) as named profiles in one file.
 */

#ifndef CONFIG_PROFILES_H
#define CONFIG_PROFILES_H

#include "tm5000.h"

/* Maximum number of configuration profiles per system */
#define MAX_CONFIG_PROFILES 10

/* Profile name and description lengths */
#define PROFILE_NAME_LENGTH 32
#define PROFILE_DESC_LENGTH 64

/* Loader rejects a header whose version differs, so bump on any layout change. */
#define PROFILE_MAGIC "TM5P"
#define PROFILE_VERSION 0x35

/* Written raw after the header: this layout, including the embedded *_config
 * structs from tm5000.h, is the on-disk format. */
#pragma pack(1)
typedef struct {
    dm5120_config dm5120_configs[10];
    dm5010_config dm5010_configs[10];
    ps5010_config ps5010_configs[10];
    fg5010_config fg5010_configs[10];
    dc5009_config dc5009_configs[10];
    dc5010_config dc5010_configs[10];
    ps5004_config ps5004_configs[10];

    char description[PROFILE_DESC_LENGTH];
    char name[PROFILE_NAME_LENGTH];
    unsigned char reserved[15];

    graph_scale graph_settings;
    fft_config fft_settings;
    control_panel_state panel_state;

    unsigned char module_enabled[10];
    unsigned char module_types[10];
    unsigned char gpib_addresses[10];

    time_t created;
    time_t modified;
    unsigned short checksum;
    unsigned char flags;                      /* PROFILE_FLAG_* */
} config_profile;
#pragma pack()

/* On-disk file header */
#pragma pack(1)
typedef struct {
    unsigned char reserved[20];
    char magic[4];                            /* PROFILE_MAGIC, not NUL-terminated */
    time_t created;
    unsigned short file_checksum;
    unsigned char version;                    /* PROFILE_VERSION */
    unsigned char profile_count;
} profile_file_header;
#pragma pack()

/* Profile list entry for UI display (in memory only, never written) */
#pragma pack(1)
typedef struct {
    char description[PROFILE_DESC_LENGTH];
    char name[PROFILE_NAME_LENGTH];
    time_t created;
    time_t modified;
    unsigned char slot;                       /* file slot index */
    unsigned char valid:1;
    unsigned char reserved:7;
} profile_list_entry;
#pragma pack()

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
#define PROFILE_ERROR_INVALID_ADDR  9
#define PROFILE_ERROR_VERSION       10

/* Profile operation flags */
#define PROFILE_FLAG_BACKUP_ON_SAVE     0x01
#define PROFILE_FLAG_VALIDATE_STRICT    0x02
#define PROFILE_FLAG_AUTO_TIMESTAMP     0x04
#define PROFILE_FLAG_COMPRESS_DATA      0x08

#endif /* CONFIG_PROFILES_H */