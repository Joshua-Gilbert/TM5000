/*
 * TM5000 GPIB Control System - Mathematical Functions Header
 * Version 3.3
 */

#ifndef MATH_FUNCTIONS_H
#define MATH_FUNCTIONS_H

/* Mathematical function prototypes */
void perform_fft(void);
int fft_configuration_menu(void);
void execute_fft_with_config(void);
void generate_window_function(float far *window, int N, int window_type);
void perform_differentiation(void);  
void perform_integration(void);
void perform_smoothing(void);

#endif
