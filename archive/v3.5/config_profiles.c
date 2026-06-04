/*
 * TM5000 GPIB Control System - Configuration Profiles Module
 * Version 3.5
 * Complete implementation for configuration profile management
 * 
 * This module provides comprehensive save/load functionality for
 * complete system configurations, enabling users to quickly switch
 * between different measurement setups.
 * 
 * Version History:
 * 3.5 - Initial implementation for configuration profile management
 */

#include "config_profiles.h"
#include "data.h"
#include "modules.h"
#include <stdlib.h>

/* Global profile management state */
char g_current_profile_name[PROFILE_NAME_LENGTH] = "";
unsigned char g_profile_modified = 0;

/* Static file management variables */
static char __far profile_file_path[80] = "";  /* Reduced from 128 to 80 bytes */
static profile_file_header file_header;
static unsigned char profiles_loaded = 0;

/* Initialize the profile management system */
int init_profile_system(void) {
    FILE *file;  /* C89: Declare at function start */
    
    /* Set profile file path to current directory (simplified) */
    strcpy(profile_file_path, "PROFILES.DAT");
    
    /* Create profile file if it doesn't exist */
    if (access(profile_file_path, F_OK) != 0) {
        if (create_profile_file() != PROFILE_SUCCESS) {
            return PROFILE_ERROR_FILE_IO;
        }
    }
    
    /* Load and validate file header */
    file = fopen(profile_file_path, "rb");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }
    
    if (fread(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    fclose(file);
    
    /* Validate file format */
    if (memcmp(file_header.magic, PROFILE_MAGIC, 4) != 0 || 
        file_header.version != PROFILE_VERSION) {
        return PROFILE_ERROR_CORRUPT;
    }
    
    profiles_loaded = 1;
    g_current_profile_name[0] = '\0';
    g_profile_modified = 0;
    
    return PROFILE_SUCCESS;
}

/* Save current system configuration as a named profile */
int save_config_profile(char *name, char *description) {
    config_profile *profile;
    FILE *file;
    long profile_offset;
    int profile_index, i;  /* C89: Declare loop variable */
    profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];
    int profile_count;
    
    /* Allocate profile structure */
    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Validate input parameters */
    if (!name || strlen(name) == 0 || strlen(name) >= PROFILE_NAME_LENGTH) {
        free(profile);
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    if (!description) {
        description = "";
    }
    
    if (strlen(description) >= PROFILE_DESC_LENGTH) {
        free(profile);
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    /* Check if profile already exists */
    profile_count = list_config_profiles(existing_profiles, MAX_CONFIG_PROFILES);
    profile_index = -1;
    
    /* C89: Declare variables at beginning of block */
    for (i = 0; i < profile_count; i++) {
        if (strcmp(existing_profiles[i].name, name) == 0) {
            profile_index = i;
            break;
        }
    }
    
    /* If new profile, check if we have space */
    if (profile_index == -1) {
        if (profile_count >= MAX_CONFIG_PROFILES) {
            free(profile);
            return PROFILE_ERROR_FULL;
        }
        /* Find first empty slot */
        for (i = 0; i < MAX_CONFIG_PROFILES; i++) {
            if (!existing_profiles[i].valid) {
                profile_index = i;
                break;
            }
        }
        if (profile_index == -1) {
            profile_index = profile_count; /* Use next available slot */
        }
    }
    
    /* Capture current system configuration */
    if (capture_current_config(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Set profile metadata */
    strncpy(profile->name, name, PROFILE_NAME_LENGTH - 1);
    profile->name[PROFILE_NAME_LENGTH - 1] = '\0';
    
    strncpy(profile->description, description, PROFILE_DESC_LENGTH - 1);
    profile->description[PROFILE_DESC_LENGTH - 1] = '\0';
    
    profile->created = (profile_index == -1) ? time(NULL) : existing_profiles[profile_index].modified;
    profile->modified = time(NULL);
    profile->flags = PROFILE_FLAG_AUTO_TIMESTAMP;
    
    /* Calculate checksum */
    profile->checksum = calculate_profile_checksum(profile);
    
    /* Validate profile */
    if (validate_config_profile(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    /* Open profile file for writing */
    file = fopen(profile_file_path, "r+b");
    if (!file) {
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Calculate profile offset in file */
    profile_offset = sizeof(profile_file_header) + (profile_index * sizeof(config_profile));
    
    /* Seek to profile position */
    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Write profile data */
    if (fwrite(profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Update file header if this is a new profile */
    if (profile_count <= profile_index) {
        file_header.profile_count = profile_index + 1;
        
        /* Write updated header */
        if (fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            free(profile);
            return PROFILE_ERROR_FILE_IO;
        }
        
        if (fwrite(&file_header, sizeof(profile_file_header), 1, file) != 1) {
            fclose(file);
            free(profile);
            return PROFILE_ERROR_FILE_IO;
        }
    }
    
    fclose(file);
    
    /* Update current profile tracking */
    strncpy(g_current_profile_name, name, PROFILE_NAME_LENGTH - 1);
    g_current_profile_name[PROFILE_NAME_LENGTH - 1] = '\0';
    g_profile_modified = 0;
    
    free(profile);
    return PROFILE_SUCCESS;
}

/* Load a named configuration profile */
int load_config_profile(char *name) {
    config_profile *profile;
    FILE *file;
    long profile_offset;
    int profile_index, i;  /* C89: Declare loop variable */
    profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];
    int profile_count;
    
    /* Allocate profile structure */
    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Validate input */
    if (!name || strlen(name) == 0) {
        free(profile);
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    /* Find profile index */
    profile_count = list_config_profiles(existing_profiles, MAX_CONFIG_PROFILES);
    profile_index = -1;
    
    for (i = 0; i < profile_count; i++) {
        if (strcmp(existing_profiles[i].name, name) == 0) {
            profile_index = i;
            break;
        }
    }
    
    if (profile_index == -1) {
        free(profile);
        return PROFILE_ERROR_NOT_FOUND;
    }
    
    /* Open profile file for reading */
    file = fopen(profile_file_path, "rb");
    if (!file) {
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Calculate profile offset in file */
    profile_offset = sizeof(profile_file_header) + (profile_index * sizeof(config_profile));
    
    /* Seek to profile position */
    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Read profile data */
    if (fread(profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }
    
    fclose(file);
    
    /* Validate profile integrity */
    if (verify_profile_integrity(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_CHECKSUM;
    }
    
    /* Apply configuration to system */
    if (apply_config_profile(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Update current profile tracking */
    strncpy(g_current_profile_name, name, PROFILE_NAME_LENGTH - 1);
    g_current_profile_name[PROFILE_NAME_LENGTH - 1] = '\0';
    g_profile_modified = 0;
    
    free(profile);
    return PROFILE_SUCCESS;
}

/* Delete a named configuration profile */
int delete_config_profile(char *name) {
    profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];
    config_profile empty_profile;
    FILE *file;
    long profile_offset;
    int profile_index, i;  /* C89: Declare loop variable */
    int profile_count;
    
    /* Validate input */
    if (!name || strlen(name) == 0) {
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    /* Find profile index */
    profile_count = list_config_profiles(existing_profiles, MAX_CONFIG_PROFILES);
    profile_index = -1;
    
    for (i = 0; i < profile_count; i++) {
        if (strcmp(existing_profiles[i].name, name) == 0) {
            profile_index = i;
            break;
        }
    }
    
    if (profile_index == -1) {
        return PROFILE_ERROR_NOT_FOUND;
    }
    
    /* Clear the profile slot */
    memset(&empty_profile, 0, sizeof(config_profile));
    
    /* Open profile file for writing */
    file = fopen(profile_file_path, "r+b");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Calculate profile offset in file */
    profile_offset = sizeof(profile_file_header) + (profile_index * sizeof(config_profile));
    
    /* Seek to profile position */
    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Write empty profile data */
    if (fwrite(&empty_profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    fclose(file);
    
    /* Clear current profile if it was deleted */
    if (strcmp(g_current_profile_name, name) == 0) {
        g_current_profile_name[0] = '\0';
        g_profile_modified = 0;
    }
    
    return PROFILE_SUCCESS;
}

/* List available configuration profiles */
int list_config_profiles(profile_list_entry profiles[], int max_count) {
    FILE *file;
    config_profile *profile;
    int count = 0, i;  /* C89: Declare loop variable */
    long profile_offset;
    
    /* Allocate profile structure */
    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return 0;
    }
    
    /* Clear output array */
    memset(profiles, 0, max_count * sizeof(profile_list_entry));
    
    /* Open profile file for reading */
    file = fopen(profile_file_path, "rb");
    if (!file) {
        free(profile);
        return 0;
    }
    
    /* Read file header */
    if (fread(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        free(profile);
        return 0;
    }
    
    /* Read each profile slot */
    for (i = 0; i < MAX_CONFIG_PROFILES && count < max_count; i++) {
        profile_offset = sizeof(profile_file_header) + (i * sizeof(config_profile));
        
        if (fseek(file, profile_offset, SEEK_SET) != 0) {
            break;
        }
        
        if (fread(profile, sizeof(config_profile), 1, file) != 1) {
            break;
        }
        
        /* Check if profile slot is valid */
        if (profile->name[0] != '\0' && verify_profile_integrity(profile) == PROFILE_SUCCESS) {
            strncpy(profiles[count].name, profile->name, PROFILE_NAME_LENGTH - 1);
            profiles[count].name[PROFILE_NAME_LENGTH - 1] = '\0';
            
            strncpy(profiles[count].description, profile->description, PROFILE_DESC_LENGTH - 1);
            profiles[count].description[PROFILE_DESC_LENGTH - 1] = '\0';
            
            profiles[count].modified = profile->modified;
            profiles[count].valid = 1;
            count++;
        }
    }
    
    fclose(file);
    free(profile);
    return count;
}

/* Capture current system configuration into a profile structure */
int capture_current_config(config_profile *profile) {
    int i;
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Clear profile structure */
    memset(profile, 0, sizeof(config_profile));
    
    /* Copy instrument configurations */
    memcpy(profile->dm5120_configs, g_dm5120_config, sizeof(g_dm5120_config));
    memcpy(profile->dm5010_configs, g_dm5010_config, sizeof(g_dm5010_config));
    memcpy(profile->ps5004_configs, g_ps5004_config, sizeof(g_ps5004_config));
    memcpy(profile->ps5010_configs, g_ps5010_config, sizeof(g_ps5010_config));
    memcpy(profile->dc5009_configs, g_dc5009_config, sizeof(g_dc5009_config));
    memcpy(profile->dc5010_configs, g_dc5010_config, sizeof(g_dc5010_config));
    memcpy(profile->fg5010_configs, g_fg5010_config, sizeof(g_fg5010_config));
    
    /* Copy system configuration */
    memcpy(&profile->graph_settings, &g_graph_scale, sizeof(g_graph_scale));
    memcpy(&profile->fft_settings, &g_fft_config, sizeof(g_fft_config));
    memcpy(&profile->panel_state, &g_control_panel, sizeof(g_control_panel));
    
    /* Copy module states */
    for (i = 0; i < 10; i++) {
        profile->module_enabled[i] = g_system->modules[i].enabled;
        profile->module_types[i] = g_system->modules[i].module_type;
        profile->gpib_addresses[i] = g_system->modules[i].gpib_address;
    }
    
    return PROFILE_SUCCESS;
}

/* Apply a configuration profile to the current system */
int apply_config_profile(config_profile *profile) {
    int i;
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Apply instrument configurations */
    memcpy(g_dm5120_config, profile->dm5120_configs, sizeof(g_dm5120_config));
    memcpy(g_dm5010_config, profile->dm5010_configs, sizeof(g_dm5010_config));
    memcpy(g_ps5004_config, profile->ps5004_configs, sizeof(g_ps5004_config));
    memcpy(g_ps5010_config, profile->ps5010_configs, sizeof(g_ps5010_config));
    memcpy(g_dc5009_config, profile->dc5009_configs, sizeof(g_dc5009_config));
    memcpy(g_dc5010_config, profile->dc5010_configs, sizeof(g_dc5010_config));
    memcpy(g_fg5010_config, profile->fg5010_configs, sizeof(g_fg5010_config));
    
    /* Apply system configuration */
    memcpy(&g_graph_scale, &profile->graph_settings, sizeof(g_graph_scale));
    memcpy(&g_fft_config, &profile->fft_settings, sizeof(g_fft_config));
    memcpy(&g_control_panel, &profile->panel_state, sizeof(g_control_panel));
    
    /* Apply module states */
    for (i = 0; i < 10; i++) {
        g_system->modules[i].enabled = profile->module_enabled[i];
        g_system->modules[i].module_type = profile->module_types[i];
        g_system->modules[i].gpib_address = profile->gpib_addresses[i];
    }
    
    return PROFILE_SUCCESS;
}

/* Calculate checksum for profile data integrity */
unsigned short calculate_profile_checksum(config_profile *profile) {
    unsigned short checksum = 0;
    unsigned char *data = (unsigned char *)profile;
    int size = sizeof(config_profile) - sizeof(profile->checksum);
    int i;  /* C89: Declare loop variable */
    
    for (i = 0; i < size; i++) {
        checksum += data[i];
        checksum = (checksum << 1) | (checksum >> 15);  /* Rotate left */
    }
    
    return checksum;
}

/* Verify profile data integrity */
int verify_profile_integrity(config_profile *profile) {
    unsigned short calculated_checksum;
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    calculated_checksum = calculate_profile_checksum(profile);
    
    if (calculated_checksum != profile->checksum) {
        return PROFILE_ERROR_CHECKSUM;
    }
    
    return PROFILE_SUCCESS;
}

/* Validate configuration profile */
int validate_config_profile(config_profile *profile) {
    int i;  /* C89: Declare loop variable */
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    /* Validate profile name */
    if (profile->name[0] == '\0') {
        return PROFILE_ERROR_INVALID_NAME;
    }
    
    /* Check for valid characters in name */
    for (i = 0; profile->name[i] != '\0' && i < PROFILE_NAME_LENGTH; i++) {
        char c = profile->name[i];
        if (c < 32 || c > 126 || c == '\\' || c == '/' || c == ':' || 
            c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return PROFILE_ERROR_INVALID_NAME;
        }
    }
    
    /* Validate GPIB addresses */
    for (i = 0; i < 10; i++) {
        if (profile->gpib_addresses[i] > 30) {
            return PROFILE_ERROR_INVALID_NAME;
        }
    }
    
    return PROFILE_SUCCESS;
}

/* Create new profile file */
int create_profile_file(void) {
    FILE *file;
    config_profile empty_profile;
    int i;  /* C89: Declare loop variable */
    
    /* Initialize file header */
    memset(&file_header, 0, sizeof(profile_file_header));
    memcpy(file_header.magic, PROFILE_MAGIC, 4);
    file_header.version = PROFILE_VERSION;
    file_header.profile_count = 0;
    file_header.created = time(NULL);
    file_header.file_checksum = 0;
    
    /* Create file */
    file = fopen(profile_file_path, "wb");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Write header */
    if (fwrite(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    /* Write empty profile slots */
    memset(&empty_profile, 0, sizeof(config_profile));
    for (i = 0; i < MAX_CONFIG_PROFILES; i++) {
        if (fwrite(&empty_profile, sizeof(config_profile), 1, file) != 1) {
            fclose(file);
            return PROFILE_ERROR_FILE_IO;
        }
    }
    
    fclose(file);
    return PROFILE_SUCCESS;
}

/* Removed unused file browser directory functions to save memory */

/* Mark current profile as modified */
int mark_profile_modified(void) {
    g_profile_modified = 1;
    return PROFILE_SUCCESS;
}

/* Check if current profile is modified */
int is_current_profile_modified(void) {
    return g_profile_modified;
}