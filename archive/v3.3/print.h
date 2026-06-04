/*
 * TM5000 GPIB Control System - Printing and Export
 * Version 3.3
 * Header file for printing and export functions
 */

#ifndef PRINT_H
#define PRINT_H

#include "tm5000.h"

/* Printing functions */
void print_report(void);
void print_graph_menu(void);
void print_graph_text(void);
void print_graph_postscript(void);

/* Printer communication */
void lpt_send_byte(unsigned char data);
void print_string(char *str);

/* Unit conversion for printing */
void get_print_units(float range, char **unit_str, float *scale_factor, int *decimal_places, char **postscript_unit);

#endif /* PRINT_H */