/*
 * TM5000 GPIB Control System - GPIB Communication Module
 * Version 3.3
 * Header file for GPIB communication functions
 */

#ifndef GPIB_H
#define GPIB_H

#include "tm5000.h"

/* GPIB communication functions */
int init_gpib_system(void);
int ieee_write(const char *str);
int ieee_read(char *buffer, int maxlen);
void gpib_write(int address, char *command);
int gpib_read(int address, char *buffer, int maxlen);
int gpib_read_float(int address, float *value);
void gpib_remote(int address);
void gpib_local(int address);
void gpib_clear(int address);
int gpib_check_srq(int address);
int ieee_spoll(int address, unsigned char *status);

/* Grid-OS auto-detect: 1 if a device answers at address (id = its reply to
   ID? or *IDN?, may be empty), 0 if nothing is there.  Uses short timeouts
   and restores the normal ones. */
int gpib_probe(int address, char *id, int maxlen);

/* Which GPIB driver this build talks to (start-up banner, terminal) */
extern const char gpib_driver_name[];
void gpib_driver_help(void);        /* hints when init_gpib_system() fails */
int gpib_release_bus(void);         /* on exit: 1 = all devices put to local
                                       by the driver, 0 = caller does it */
extern const char gpib_driver_note[];   /* extra start-up note, may be "" */

/* DM5120-specific GPIB functions */
void gpib_write_dm5120(int address, char *command);
int gpib_read_dm5120(int address, char *buffer, int maxlen);
int gpib_read_float_dm5120(int address, float *value);
void gpib_remote_dm5120(int address);
void gpib_local_dm5120(int address);
void gpib_clear_dm5120(int address);

/* DM5010-specific GPIB functions */
void gpib_write_dm5010(int address, char *command);
int gpib_read_dm5010(int address, char *buffer, int maxlen);
int gpib_read_float_dm5010(int address, float *value);

/* Utility functions */
int command_has_response(const char *cmd);
void drain_input_buffer(void);
void gpib_terminal_mode(void);
void send_custom_command(void);
void check_gpib_error(void);

#endif /* GPIB_H */
