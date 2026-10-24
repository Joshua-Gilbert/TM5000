/*
 * Named configuration profiles, stored as a header plus MAX_CONFIG_PROFILES
 * fixed-size binary slots in PROFILES.DAT.
 */

#include "config_profiles.h"
#include "data.h"
#include "modules.h"
#include <stdlib.h>
#include <stddef.h>

char g_current_profile_name[PROFILE_NAME_LENGTH] = "";
unsigned char g_profile_modified = 0;

static char __far profile_file_path[80] = "";
static profile_file_header file_header;
static unsigned char profiles_loaded = 0;

/* Creates PROFILES.DAT if missing, then checks its magic and version. */
int init_profile_system(void) {
    FILE *file;

    /* Current directory only */
    strcpy(profile_file_path, "PROFILES.DAT");

    if (access(profile_file_path, F_OK) != 0) {
        if (create_profile_file() != PROFILE_SUCCESS) {
            return PROFILE_ERROR_FILE_IO;
        }
    }

    file = fopen(profile_file_path, "rb");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }

    if (fread(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }

    fclose(file);

    if (memcmp(file_header.magic, PROFILE_MAGIC, 4) != 0) {
        return PROFILE_ERROR_CORRUPT;
    }

    if (file_header.version != PROFILE_VERSION) {
        return PROFILE_ERROR_VERSION;
    }

    /* The menu calls this on every entry; keep the current-profile state after the first */
    if (!profiles_loaded) {
        g_current_profile_name[0] = '\0';
        g_profile_modified = 0;
    }
    profiles_loaded = 1;

    return PROFILE_SUCCESS;
}

/* Saves the current configuration; a profile with the same name is overwritten. */
int save_config_profile(char *name, char *description) {
    config_profile *profile;
    FILE *file;
    long profile_offset;
    int profile_index, i, j, slot, result;
    static profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];  /* static: the DOS stack is 2 KB */
    int profile_count;

    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }

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
    
    profile_count = list_config_profiles(existing_profiles, MAX_CONFIG_PROFILES);
    profile_index = -1;

    for (i = 0; i < profile_count; i++) {
        if (strcmp(existing_profiles[i].name, name) == 0) {
            profile_index = i;
            break;
        }
    }
    
    if (profile_index != -1) {
        slot = existing_profiles[profile_index].slot;
    } else {
        if (profile_count >= MAX_CONFIG_PROFILES) {
            free(profile);
            return PROFILE_ERROR_FULL;
        }
        /* First file slot not held by a listed profile */
        for (slot = 0; slot < MAX_CONFIG_PROFILES; slot++) {
            for (j = 0; j < profile_count; j++) {
                if (existing_profiles[j].slot == slot) {
                    break;
                }
            }
            if (j == profile_count) {
                break;
            }
        }
    }

    if (capture_current_config(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_MEMORY;
    }

    strncpy(profile->name, name, PROFILE_NAME_LENGTH - 1);
    profile->name[PROFILE_NAME_LENGTH - 1] = '\0';
    
    strncpy(profile->description, description, PROFILE_DESC_LENGTH - 1);
    profile->description[PROFILE_DESC_LENGTH - 1] = '\0';
    
    profile->modified = time(NULL);
    profile->created = (profile_index == -1) ? profile->modified : existing_profiles[profile_index].created;
    profile->flags = PROFILE_FLAG_AUTO_TIMESTAMP;

    profile->checksum = calculate_profile_checksum(profile);

    result = validate_config_profile(profile);
    if (result != PROFILE_SUCCESS) {
        free(profile);
        return result;
    }

    file = fopen(profile_file_path, "r+b");
    if (!file) {
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    profile_offset = sizeof(profile_file_header) + ((long) slot * sizeof(config_profile));

    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    if (fwrite(profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    /* New slot past the old count: bump the header's profile_count */
    if (file_header.profile_count <= slot) {
        file_header.profile_count = slot + 1;

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

    strncpy(g_current_profile_name, name, PROFILE_NAME_LENGTH - 1);
    g_current_profile_name[PROFILE_NAME_LENGTH - 1] = '\0';
    g_profile_modified = 0;

    free(profile);
    return PROFILE_SUCCESS;
}

int load_config_profile(char *name) {
    config_profile *profile;
    FILE *file;
    long profile_offset;
    int profile_index, i;
    static profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];  /* static: the DOS stack is 2 KB */
    int profile_count;

    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }

    if (!name || strlen(name) == 0) {
        free(profile);
        return PROFILE_ERROR_INVALID_NAME;
    }

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
    
    file = fopen(profile_file_path, "rb");
    if (!file) {
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    profile_offset = sizeof(profile_file_header) +
                     ((long) existing_profiles[profile_index].slot * sizeof(config_profile));

    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    if (fread(profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        free(profile);
        return PROFILE_ERROR_FILE_IO;
    }

    fclose(file);

    if (verify_profile_integrity(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_CHECKSUM;
    }

    if (apply_config_profile(profile) != PROFILE_SUCCESS) {
        free(profile);
        return PROFILE_ERROR_MEMORY;
    }

    strncpy(g_current_profile_name, name, PROFILE_NAME_LENGTH - 1);
    g_current_profile_name[PROFILE_NAME_LENGTH - 1] = '\0';
    g_profile_modified = 0;
    
    free(profile);
    return PROFILE_SUCCESS;
}

/* Zeroes the slot in place; later slots do not move. */
int delete_config_profile(char *name) {
    static profile_list_entry existing_profiles[MAX_CONFIG_PROFILES];  /* static: the DOS stack is 2 KB */
    static config_profile empty_profile;
    FILE *file;
    long profile_offset;
    int profile_index, i;
    int profile_count;

    if (!name || strlen(name) == 0) {
        return PROFILE_ERROR_INVALID_NAME;
    }
    
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
    
    memset(&empty_profile, 0, sizeof(config_profile));
    
    file = fopen(profile_file_path, "r+b");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }
    
    profile_offset = sizeof(profile_file_header) +
                     ((long) existing_profiles[profile_index].slot * sizeof(config_profile));

    if (fseek(file, profile_offset, SEEK_SET) != 0) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    if (fwrite(&empty_profile, sizeof(config_profile), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
    fclose(file);
    
    if (strcmp(g_current_profile_name, name) == 0) {
        g_current_profile_name[0] = '\0';
        g_profile_modified = 0;
    }
    
    return PROFILE_SUCCESS;
}

/* Returns valid (named, checksum-OK) profiles packed into profiles[0..count-1]; a list
   index is not a file slot, use .slot for that. Also reloads file_header. */
int list_config_profiles(profile_list_entry profiles[], int max_count) {
    FILE *file;
    config_profile *profile;
    int count = 0, i;
    long profile_offset;
    
    profile = (config_profile *) malloc(sizeof(config_profile));
    if (!profile) {
        return 0;
    }
    
    memset(profiles, 0, max_count * sizeof(profile_list_entry));
    
    file = fopen(profile_file_path, "rb");
    if (!file) {
        free(profile);
        return 0;
    }
    
    if (fread(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        free(profile);
        return 0;
    }
    
    for (i = 0; i < MAX_CONFIG_PROFILES && count < max_count; i++) {
        profile_offset = sizeof(profile_file_header) + ((long) i * sizeof(config_profile));
        
        if (fseek(file, profile_offset, SEEK_SET) != 0) {
            break;
        }
        
        if (fread(profile, sizeof(config_profile), 1, file) != 1) {
            break;
        }
        
        if (profile->name[0] != '\0' && verify_profile_integrity(profile) == PROFILE_SUCCESS) {
            strncpy(profiles[count].name, profile->name, PROFILE_NAME_LENGTH - 1);
            profiles[count].name[PROFILE_NAME_LENGTH - 1] = '\0';
            
            strncpy(profiles[count].description, profile->description, PROFILE_DESC_LENGTH - 1);
            profiles[count].description[PROFILE_DESC_LENGTH - 1] = '\0';
            
            profiles[count].created = profile->created;
            profiles[count].modified = profile->modified;
            profiles[count].slot = (unsigned char) i;
            profiles[count].valid = 1;
            count++;
        }
    }
    
    fclose(file);
    free(profile);
    return count;
}

int capture_current_config(config_profile *profile) {
    int i;
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    memset(profile, 0, sizeof(config_profile));
    
    memcpy(profile->dm5120_configs, g_dm5120_config, sizeof(g_dm5120_config));
    memcpy(profile->dm5010_configs, g_dm5010_config, sizeof(g_dm5010_config));
    memcpy(profile->ps5004_configs, g_ps5004_config, sizeof(g_ps5004_config));
    memcpy(profile->ps5010_configs, g_ps5010_config, sizeof(g_ps5010_config));
    memcpy(profile->dc5009_configs, g_dc5009_config, sizeof(g_dc5009_config));
    memcpy(profile->dc5010_configs, g_dc5010_config, sizeof(g_dc5010_config));
    memcpy(profile->fg5010_configs, g_fg5010_config, sizeof(g_fg5010_config));
    
    memcpy(&profile->graph_settings, &g_graph_scale, sizeof(g_graph_scale));
    memcpy(&profile->fft_settings, &g_fft_config, sizeof(g_fft_config));
    memcpy(&profile->panel_state, &g_control_panel, sizeof(g_control_panel));
    
    for (i = 0; i < 10; i++) {
        profile->module_enabled[i] = g_system->modules[i].enabled;
        profile->module_types[i] = g_system->modules[i].module_type;
        profile->gpib_addresses[i] = g_system->modules[i].gpib_address;
    }
    
    return PROFILE_SUCCESS;
}

/* Restores configs and slot type/address/enable only; descriptions and data buffers are untouched. */
int apply_config_profile(config_profile *profile) {
    int i;
    
    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }
    
    memcpy(g_dm5120_config, profile->dm5120_configs, sizeof(g_dm5120_config));
    for (i = 0; i < 10; i++) dm5120_fix_legacy_config(i);
    memcpy(g_dm5010_config, profile->dm5010_configs, sizeof(g_dm5010_config));
    memcpy(g_ps5004_config, profile->ps5004_configs, sizeof(g_ps5004_config));
    memcpy(g_ps5010_config, profile->ps5010_configs, sizeof(g_ps5010_config));
    memcpy(g_dc5009_config, profile->dc5009_configs, sizeof(g_dc5009_config));
    memcpy(g_dc5010_config, profile->dc5010_configs, sizeof(g_dc5010_config));
    memcpy(g_fg5010_config, profile->fg5010_configs, sizeof(g_fg5010_config));
    
    memcpy(&g_graph_scale, &profile->graph_settings, sizeof(g_graph_scale));
    memcpy(&g_fft_config, &profile->fft_settings, sizeof(g_fft_config));
    memcpy(&g_control_panel, &profile->panel_state, sizeof(g_control_panel));
    
    for (i = 0; i < 10; i++) {
        g_system->modules[i].enabled = profile->module_enabled[i];
        g_system->modules[i].module_type = profile->module_types[i];
        g_system->modules[i].gpib_address = profile->gpib_addresses[i];
        g_system->modules[i].is_result = 0;  /* profiles hold real modules only (FINDINGS #1) */
        forget_trace_slot(i);
    }
    
    return PROFILE_SUCCESS;
}

/* Rotating byte sum over all but the last 2 bytes of the struct (flags is not covered).
   That range includes the checksum field's low byte, which is hashed as 0 so save
   (field still 0) and verify (field set) agree and existing files still verify. */
unsigned short calculate_profile_checksum(config_profile *profile) {
    unsigned short checksum = 0;
    unsigned char *data = (unsigned char *)profile;
    int size = sizeof(config_profile) - sizeof(profile->checksum);
    int skip = offsetof(config_profile, checksum);
    int i;

    for (i = 0; i < size; i++) {
        checksum += (i >= skip) ? 0 : data[i];
        checksum = (checksum << 1) | (checksum >> 15);  /* Rotate left */
    }
    
    return checksum;
}

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

int validate_config_profile(config_profile *profile) {
    int i;

    if (!profile) {
        return PROFILE_ERROR_MEMORY;
    }

    if (profile->name[0] == '\0') {
        return PROFILE_ERROR_INVALID_NAME;
    }

    /* Printable ASCII, none of the DOS filename-reserved characters */
    for (i = 0; profile->name[i] != '\0' && i < PROFILE_NAME_LENGTH; i++) {
        char c = profile->name[i];
        if (c < 32 || c > 126 || c == '\\' || c == '/' || c == ':' || 
            c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return PROFILE_ERROR_INVALID_NAME;
        }
    }
    
    /* GPIB primary addresses are 0-30 */
    for (i = 0; i < 10; i++) {
        if (profile->gpib_addresses[i] > 30) {
            return PROFILE_ERROR_INVALID_ADDR;
        }
    }
    
    return PROFILE_SUCCESS;
}

/* Header plus MAX_CONFIG_PROFILES zeroed slots, so every slot offset is seekable. */
int create_profile_file(void) {
    FILE *file;
    static config_profile empty_profile;  /* static: the DOS stack is 2 KB */
    int i;

    memset(&file_header, 0, sizeof(profile_file_header));
    memcpy(file_header.magic, PROFILE_MAGIC, 4);
    file_header.version = PROFILE_VERSION;
    file_header.profile_count = 0;
    file_header.created = time(NULL);
    file_header.file_checksum = 0;
    
    file = fopen(profile_file_path, "wb");
    if (!file) {
        return PROFILE_ERROR_FILE_IO;
    }
    
    if (fwrite(&file_header, sizeof(profile_file_header), 1, file) != 1) {
        fclose(file);
        return PROFILE_ERROR_FILE_IO;
    }
    
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

int mark_profile_modified(void) {
    g_profile_modified = 1;
    return PROFILE_SUCCESS;
}

int is_current_profile_modified(void) {
    return g_profile_modified;
}