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
int print_screen_ps(const char *caption);

/* what the graph's P key prints (set in the print menu) */
#define PRINT_MODE_POSTSCRIPT 0
#define PRINT_MODE_TEXT       1
#define PRINT_MODE_SCREEN     2
extern int g_print_mode;
void print_graph_selected(void);
extern int g_lpt_error;

/* Unit conversion for printing */
void get_print_units(double range, char **unit_str, double *scale_factor, int *decimal_places, char **postscript_unit);

/* Custom header control */
extern int g_use_custom_header;
extern char g_custom_header_text[64];

#endif /* PRINT_H */