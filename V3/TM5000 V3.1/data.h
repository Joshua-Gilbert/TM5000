/*
 * TM5000 GPIB Control System - Data Management
 * Version 3.1
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

#endif /* DATA_H */
