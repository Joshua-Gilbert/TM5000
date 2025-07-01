/*
 * TM5000 GPIB Control System - Module Functions Header
 * Version 3.4
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

#endif
