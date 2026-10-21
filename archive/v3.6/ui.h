/*
 * TM5000 GPIB Control System - User Interface
 * Version 3.5
 * Header file for user interface functions
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 * 3.2 - Version update
 * 3.3 - Version update
 * 3.5 - Added menus for configuration profiles, enhanced export, file browser
 */

#ifndef UI_H
#define UI_H

#include "tm5000.h"

/* Main menu system */
void main_menu(void);
void file_operations_menu(void);
void measurement_menu(void);

/* Enhanced file operations menus (v3.5) */
void configuration_profiles_menu(void);
void enhanced_export_menu(void);
void file_browser_menu(void);

/* Enhanced math menus (v3.5) */
void dual_trace_operations_menu(void);
void statistics_analysis_menu(void);
void digital_filter_menu(void);
void curve_fitting_menu(void);
void correlation_analysis_menu(void);
void calculate_statistics(void);
void math_functions_menu(void);

/* Error handling and display */
void display_error(char *msg);
void display_success(char *msg);

/* Input handling */
int wait_for_input(void);
int get_numeric_input(char *prompt, float *value, float min, float max);
int get_string_input(char *prompt, char *buffer, int max_length);
int get_yes_no(char *prompt);

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

/* UI utility functions (v3.5) */
void display_header(char *title);
void display_footer(char *prompt);
void display_menu_item(int number, char *text, int enabled);
void display_progress(int current, int total);
void clear_screen_area(int x1, int y1, int x2, int y2);

/* Utility functions */
void delay(unsigned int milliseconds);

#endif /* UI_H */
