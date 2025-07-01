/*
 * TM5000 GPIB Control System - Data Management
 * Version 3.2
 * Header file for data management functions
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

#endif /* DATA_H */
