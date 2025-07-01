/*
 * TM5000 GPIB Control System - User Interface
 * Version 3.0
 * Header file for user interface functions
 */

#ifndef UI_H
#define UI_H

#include "tm5000.h"

/* Main menu system */
void main_menu(void);
void file_operations_menu(void);
void measurement_menu(void);

/* Error handling and display */
void display_error(char *msg);

/* Input handling */
int wait_for_input(void);

/* Menu functions not defined elsewhere */
void module_functions_menu(void);
void continuous_monitor_setup(void);
void sample_rate_menu(void);
void module_selection_menu(void);

/* Data functions not defined elsewhere */
void save_settings(void);
void load_settings(void);

/* Analysis functions not defined elsewhere */
void test_dm5120_comm_debug(int address);
void sync_traces_with_modules(void);

/* Graphics helper functions not in graphics.h */
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color);
void draw_gradient_rect(int x1, int y1, int x2, int y2, unsigned char color1, unsigned char color2);
void draw_text(int x, int y, char *text, unsigned char color);

/* Utility functions */
void delay(unsigned int milliseconds);

#endif /* UI_H */
