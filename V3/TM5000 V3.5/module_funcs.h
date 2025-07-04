/*
 * TM5000 GPIB Control System - Module Functions Header
 * Version 3.0
 */

#ifndef MODULE_FUNCS_H
#define MODULE_FUNCS_H

#include "tm5000.h"

/* Module function prototypes */
void send_custom_command(void);
void configure_dm5120_advanced(int slot);
void configure_ps5004_advanced(int slot);
void configure_ps5010_advanced(int slot);
void gpib_terminal_mode(void);

/* Communication test functions */
void test_ps5004_comm(int address);
void test_ps5010_comm(int address);

/* DM5120 Enhanced Menu Functions */
void dm5120_buffer_query_menu(int address);
void dm5120_stoint_menu(int address, int slot);
void dm5120_srq_menu(int address, int slot);

#endif
