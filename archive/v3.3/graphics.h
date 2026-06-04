/*
 * TM5000 GPIB Control System - Graphics and Display
 * Version 3.3
 * Header file for graphics and display functions
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "tm5000.h"

/* Graphics initialization */
void init_graphics(void);
void text_mode(void);

/* Basic graphics functions */
void plot_pixel(int x, int y, unsigned char color);
void draw_line(int x1, int y1, int x2, int y2, unsigned char color);
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color);
void fill_rectangle(int x1, int y1, int x2, int y2, unsigned char color);
void draw_filled_rect(int x1, int y1, int x2, int y2, unsigned char color);
void draw_gradient_rect(int x1, int y1, int x2, int y2, unsigned char color1, unsigned char color2);

/* Text functions */
void clrscr(void);
void gotoxy(int x, int y);
void textattr(unsigned char attr);
void clreol(void);
void draw_text(int x, int y, char *text, unsigned char color);
void draw_text_scaled(int x, int y, char *text, unsigned char color, int scale_x, int scale_y);
void draw_readout(int x, int y, char *text);

/* Enhanced font support */
int get_font_index(char c);
extern unsigned char enhanced_font[][7];
extern unsigned char scientific_font[][7];

/* Graph display functions */
void graph_display(void);
void draw_waveform_far(float far *data, int count, unsigned char color);
void draw_grid_dynamic(int max_samples);
void draw_frequency_grid(int fft_samples);
void draw_legend_enhanced(int *is_fft_trace, int selected_trace);
float get_engineering_scale(float range, float *per_div, char **unit_str, int *decimal_places);
void auto_scale_graph(void);
int value_to_y(float value);
float y_to_value(int y);
void update_sample_rate(void);

/* Graph configuration */
void graph_config_menu(void);

/* Math functions for graph display */
void math_functions_menu(void);
void perform_fft(void);
void calculate_statistics(void);
void perform_differentiation(void);
void perform_integration(void);
void perform_smoothing(void);

/* Mouse support */
int init_mouse(void);
void show_mouse(void);
void hide_mouse(void);
void get_mouse_status(void);
void set_mouse_pos(int x, int y);
int mouse_in_region(int x1, int y1, int x2, int y2);

/* Utility functions */
void get_graph_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void get_frequency_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void get_db_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void get_derivative_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void get_current_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void get_resistance_units(float range, char **unit_str, float *scale_factor, int *decimal_places);
void snap_graph_scale_to_clean_values(void);
int get_module_color(int module_type);

#endif /* GRAPHICS_H */
