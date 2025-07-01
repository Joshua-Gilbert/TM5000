/*
 * Complete Working graph_display Function from TM5000L.c
 * This version works properly with text rendering and graphics
 */

void graph_display(void) {
    int done = 0;
    int i, j, x, y, old_x = -1, old_y = -1;
    int key;
    char readout[80];
    float value;
    int mouse_visible = 0;
    int need_redraw = 1;
    int active_traces = 0;
    int any_data = 0;
    int max_samples = 0;
    int cursor_x = -1;        
    int selected_trace = -1;  
    int cursor_visible = 0;
    float x_normalized;       
    int sample_num;          
    int slot;                
    float center, range, shift;  
    float x_scale;           
    int x_pos1, x_pos2, y_pos1, y_pos2;  
    int old_cursor_x = -1;   
    char *unit_str;          
    int current_sample;      
    float sample_to_x_scale; 
    int keyboard_mode = 1;   
    float y_scale;           
    float normalized_y;      
    int readout_needs_update = 1;  
    int last_readout_x = -1;       
    int shift_pressed = 0;   
    int is_fft_trace[8];     
    float freq_scale;        
    char *freq_unit_str;     
    float freq_multiplier;   
    int should_monitor;      
    char *display_unit_str;      
    float display_scale_factor;  
    int display_decimal_places;  
    float y_range;
    
    /* Initialize FFT trace detection */
    for (i = 0; i < 10; i++) {
        is_fft_trace[i] = 0;
        if (g_system->modules[i].enabled && 
            strcmp(g_system->modules[i].description, "FFT Result") == 0) {
            is_fft_trace[i] = 1;
        }
    }
    
    /* Check for available data and find maximum sample count */
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && 
            g_system->modules[i].module_data && 
            g_system->modules[i].module_data_count > 0) {
            any_data = 1;
            if (g_system->modules[i].module_data_count > max_samples) {
                max_samples = g_system->modules[i].module_data_count;
            }
        }
    }
    
    /* Exit if no data available */
    if (!any_data && g_system->data_count == 0) {
        printf("No data to display. Run measurements first.\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    /* Synchronize traces with module data */
    sync_traces_with_modules();
    
    /* Find first enabled trace as default selection */
    if (selected_trace == -1) {
        for (i = 0; i < 10; i++) {
            if (g_traces[i].enabled && g_traces[i].data_count > 0) {
                selected_trace = i;
                break;
            }
        }
    }
    
    /* Count active traces */
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled) {
            active_traces++;
        }
    }
    
    /* Initialize cursor position */
    if (selected_trace >= 0 && selected_trace < 10 && g_traces[selected_trace].enabled) {
        current_sample = g_traces[selected_trace].data_count / 2;
        if (g_has_287) {
            sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
            cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
        } else {
            sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
            cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
        }
    } else {
        cursor_x = GRAPH_LEFT + GRAPH_WIDTH / 2;
    }
    
    /* Initialize graphics mode */
    init_graphics();
    
    /* Perform auto-scaling if enabled */
    if (g_graph_scale.auto_scale) {
        auto_scale_graph();
    }
    
    /* Set up display units and scaling */
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
    
    /* Configure frequency units for FFT traces */
    if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
        float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
        float max_freq = sample_rate / 2.0;
        
        if (max_freq >= 1000000.0) {
            freq_unit_str = "MHz";
            freq_multiplier = 1000000.0;
        } else if (max_freq >= 1000.0) {
            freq_unit_str = "kHz";
            freq_multiplier = 1000.0;
        } else {
            freq_unit_str = "Hz";
            freq_multiplier = 1.0;
        }
        unit_str = freq_unit_str;
    } else {
        unit_str = display_unit_str;
    }
    
    /* Calculate Y-axis scaling for 287 math coprocessor */
    if (g_has_287) {
        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
    }
    
    /* Main display loop */
    while (!done) {
        /* Redraw entire graph if needed */
        if (need_redraw) {
            /* Clear video memory */
            _fmemset(video_mem, 0, 16384);
            
            /* Draw appropriate grid (frequency or time domain) */
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                draw_frequency_grid(g_traces[selected_trace].data_count);
            } else {
                draw_grid_dynamic(max_samples);
            }
            
            /* Draw all enabled traces */
            for (i = 0; i < 10; i++) {
                if (g_traces[i].enabled && g_traces[i].data_count > 1) {
                    int decimation_factor = 1;
                    int effective_samples;
                    int start_sample = g_graph_scale.sample_start;
                    int display_count;
                    
                    /* Determine sample range to display */
                    if (g_graph_scale.sample_count == 0) {
                        start_sample = 0;
                        display_count = g_traces[i].data_count;
                    } else {
                        display_count = g_graph_scale.sample_count;
                        if (start_sample >= g_traces[i].data_count) {
                            start_sample = 0;
                        }
                        if (start_sample + display_count > g_traces[i].data_count) {
                            display_count = g_traces[i].data_count - start_sample;
                        }
                    }
                    
                    effective_samples = display_count;
                    
                    /* Apply decimation if needed for large datasets */
                    if (display_count > GRAPH_WIDTH) {
                        if (g_has_287) {
                            decimation_factor = (display_count + GRAPH_WIDTH - 1) / GRAPH_WIDTH;
                        } else {
                            decimation_factor = display_count / GRAPH_WIDTH;
                            if (decimation_factor < 1) decimation_factor = 1;
                        }
                        effective_samples = display_count / decimation_factor;
                    }
                    
                    /* Calculate X-axis scaling */
                    if (g_has_287) {
                        x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                    } else {
                        x_scale = (float)GRAPH_WIDTH / (float)(effective_samples - 1);
                    }
                    
                    /* Draw trace as connected line segments */
                    for (j = 0; j < effective_samples - 1; j++) {
                        int sample_idx1 = start_sample + (j * decimation_factor);
                        int sample_idx2 = start_sample + ((j + 1) * decimation_factor);
                        
                        /* Bounds checking */
                        if (sample_idx1 >= g_traces[i].data_count) sample_idx1 = g_traces[i].data_count - 1;
                        if (sample_idx2 >= g_traces[i].data_count) sample_idx2 = g_traces[i].data_count - 1;
                        
                        /* Calculate X positions */
                        if (g_has_287) {
                            x_pos1 = GRAPH_LEFT + (int)(j * x_scale);
                            x_pos2 = GRAPH_LEFT + (int)((j + 1) * x_scale);
                        } else {
                            x_pos1 = GRAPH_LEFT + (int)((float)j * x_scale);
                            x_pos2 = GRAPH_LEFT + (int)((float)(j + 1) * x_scale);
                        }
                        
                        /* Clamp X positions to graph bounds */
                        if (x_pos1 < GRAPH_LEFT) x_pos1 = GRAPH_LEFT;
                        if (x_pos2 > GRAPH_RIGHT) x_pos2 = GRAPH_RIGHT;
                        
                        /* Calculate Y positions */
                        if (g_has_287) {
                            normalized_y = (g_traces[i].data[sample_idx1] - g_graph_scale.min_value) * y_scale;
                            y_pos1 = GRAPH_BOTTOM - (int)normalized_y;
                            
                            normalized_y = (g_traces[i].data[sample_idx2] - g_graph_scale.min_value) * y_scale;
                            y_pos2 = GRAPH_BOTTOM - (int)normalized_y;
                        } else {
                            y_pos1 = value_to_y(g_traces[i].data[sample_idx1]);
                            y_pos2 = value_to_y(g_traces[i].data[sample_idx2]);
                        }
                        
                        /* Clamp Y positions to graph bounds */
                        if (y_pos1 < GRAPH_TOP) y_pos1 = GRAPH_TOP;
                        if (y_pos1 > GRAPH_BOTTOM) y_pos1 = GRAPH_BOTTOM;
                        if (y_pos2 < GRAPH_TOP) y_pos2 = GRAPH_TOP;
                        if (y_pos2 > GRAPH_BOTTOM) y_pos2 = GRAPH_BOTTOM;
                        
                        /* Draw line segment with anti-aliasing */
                        if (x_pos1 >= GRAPH_LEFT && x_pos2 <= GRAPH_RIGHT) {
                            if (i == selected_trace) {
                                /* Highlight selected trace with thicker line */
                                draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                                if (y_pos1 > GRAPH_TOP && y_pos1 < GRAPH_BOTTOM) {
                                    draw_line_aa(x_pos1, y_pos1-1, x_pos2, y_pos2-1, g_traces[i].color);
                                }
                            } else {
                                draw_line_aa(x_pos1, y_pos1, x_pos2, y_pos2, g_traces[i].color);
                            }
                        }
                    }
                }
            }
            
            /* Draw enhanced legend */
            draw_legend_enhanced(is_fft_trace);
            
            /* Draw status bar with gradient background */
            draw_gradient_rect(0, 185, SCREEN_WIDTH-1, 199, 1, 2);
            
            /* Draw status bar text */
            draw_text(2, 188, "A:AUTO +/-:ZOOM H:HELP/CONFIG", 3);
            draw_text(2, 195, "0-9:SELECT ALT+#:TOGGLE ESC:EXIT", 3);
            
            /* Draw range and scale information */
            if (selected_trace >= 0 && is_fft_trace[selected_trace]) {
                float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                float max_freq = sample_rate / 2.0;
                sprintf(readout, "0-%.1f%s", max_freq / freq_multiplier, freq_unit_str);
            } else {
                if (display_decimal_places == 0) {
                    sprintf(readout, "%.0f-%.0f%s", 
                            g_graph_scale.min_value * display_scale_factor,
                            g_graph_scale.max_value * display_scale_factor,
                            display_unit_str);
                } else {
                    sprintf(readout, "%.*f-%.*f%s", 
                            display_decimal_places, g_graph_scale.min_value * display_scale_factor,
                            display_decimal_places, g_graph_scale.max_value * display_scale_factor,
                            display_unit_str);
                }
            }
            draw_text(240, 188, readout, 3);
            
            /* Show 287 coprocessor status */
            if (g_has_287) {
                draw_text(240, 195, "287", 2);
            }
            
            /* Show large dataset indicator */
            if (selected_trace >= 0 && g_traces[selected_trace].enabled && 
                g_traces[selected_trace].data_count >= 1000) {
                draw_text(255, 195, "1K", 2);
            }
            
            /* Show sample range if zoomed */
            if (g_graph_scale.sample_count > 0) {
                char range_str[20];
                sprintf(range_str, "R:%d-%d", 
                        g_graph_scale.sample_start + 1, 
                        g_graph_scale.sample_start + g_graph_scale.sample_count);
                draw_text(270, 195, range_str, 6);  /* Yellow for range indicator */
            }
            
            /* Show DM5120 buffer status */
            if (selected_trace >= 0 && g_system->modules[selected_trace].module_type == MOD_DM5120) {
                dm5120_config *cfg = &g_dm5120_config[selected_trace];
                if (cfg->buffer_enabled && cfg->burst_mode) {
                    sprintf(readout, "BUF@%.0fHz", cfg->sample_rate);
                    draw_text(280, 195, readout, 1);
                }
            }
            
            need_redraw = 0;
            cursor_visible = 1;
        }
        
        /* Update cursor position if changed */
        if (cursor_x != old_cursor_x && cursor_visible) {
            need_redraw = 1;
            old_cursor_x = cursor_x;
            readout_needs_update = 1;
            continue;
        }
        
        /* Draw cursor and readout */
        if (cursor_visible && cursor_x >= GRAPH_LEFT && cursor_x <= GRAPH_RIGHT && 
            readout_needs_update && !need_redraw) {
            
            /* Draw cursor line */
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 2) {
                plot_pixel(cursor_x, y, 3);
            }
            
            /* Calculate and display readout */
            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                if (keyboard_mode && g_has_287) {
                    sample_num = current_sample;
                } else {
                    float effective_rate = 1.0;
                    if (g_system->modules[selected_trace].module_type == MOD_DM5120) {
                        dm5120_config *cfg = &g_dm5120_config[selected_trace];
                        if (cfg->buffer_enabled && cfg->burst_mode) {
                            effective_rate = cfg->sample_rate / (1000.0 / g_control_panel.sample_rate_ms);
                        }
                    }
                    
                    x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                    if (effective_rate > 1.0) {
                        sample_num = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) * effective_rate + 0.5);
                    } else {
                        sample_num = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                    }
                }
                
                if (sample_num < 0) sample_num = 0;
                if (sample_num >= g_traces[selected_trace].data_count) {
                    sample_num = g_traces[selected_trace].data_count - 1;
                }
                
                if (keyboard_mode && sample_num >= 0 && sample_num < g_traces[selected_trace].data_count) {
                    value = g_traces[selected_trace].data[sample_num];
                } else {
                    value = g_traces[selected_trace].data[sample_num];
                }
                
                /* Format readout based on trace type */
                if (is_fft_trace[selected_trace]) {
                    float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                    float freq = (sample_rate / 2.0) * sample_num / (g_traces[selected_trace].data_count - 1);
                    sprintf(readout, "S%d[%.1f%s]:%.3f", selected_trace, 
                            freq / freq_multiplier, freq_unit_str, value);
                } else {
                    if (display_decimal_places == 0) {
                        sprintf(readout, "S%d[%d]:%.0f %s", selected_trace, sample_num, 
                                value * display_scale_factor, display_unit_str);
                    } else {
                        sprintf(readout, "S%d[%d]:%.*f %s", selected_trace, sample_num, 
                                display_decimal_places, value * display_scale_factor, display_unit_str);
                    }
                }
                
                /* Position readout near cursor */
                {
                    int readout_x = cursor_x - 30;
                    if (readout_x < GRAPH_LEFT) readout_x = GRAPH_LEFT;
                    if (readout_x > GRAPH_RIGHT - 60) readout_x = GRAPH_RIGHT - 60;
                    draw_text(readout_x, GRAPH_TOP + 20, readout, 3);
                }
            }
            
            readout_needs_update = 0;
            last_readout_x = cursor_x;
        }
        
        /* Handle mouse input */
        if (g_mouse.present && mouse_visible) {
            get_mouse_status();
            
            x = g_mouse.x * 320 / 80;
            y = g_mouse.y * 200 / 25;
            
            if ((x != old_x || y != old_y) && 
                x >= GRAPH_LEFT && x <= GRAPH_RIGHT &&
                y >= GRAPH_TOP && y <= GRAPH_BOTTOM) {
                
                keyboard_mode = 0;
                cursor_x = x;
                cursor_visible = 1;
                
                old_x = x;
                old_y = y;
            }
        }
        
        /* Handle keyboard input */
        if (kbhit()) {
            key = getch();
            
            /* Handle ALT+number keys for trace toggling */
            if (key >= 128 && key <= 137) {  /* ALT+0 through ALT+9 */
                slot = key - 128;  /* Convert to slot number 0-9 */
                
                if (slot < 10) {
                    if (g_system->modules[slot].enabled && 
                        g_system->modules[slot].module_data &&
                        g_system->modules[slot].module_data_count > 0) {
                        g_traces[slot].enabled = !g_traces[slot].enabled;
                        
                        active_traces = 0;
                        for (i = 0; i < 10; i++) {
                            if (g_traces[i].enabled) active_traces++;
                        }
                        
                        if (!g_traces[selected_trace].enabled) {
                            selected_trace = -1;
                            for (i = 0; i < 10; i++) {
                                if (g_traces[i].enabled) {
                                    selected_trace = i;
                                    break;
                                }
                            }
                        }
                        
                        need_redraw = 1;
                    }
                }
                continue;
            }
            
            /* Handle number keys for trace selection */
            if (key >= '0' && key <= '9') {
                slot = key - '0';
                
                if (g_traces[slot].enabled) {
                    selected_trace = slot;
                    cursor_visible = 1;
                    keyboard_mode = 1;
                    
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    
                    if (is_fft_trace[selected_trace]) {
                        float sample_rate = 1000.0 / g_control_panel.sample_rate_ms;
                        float max_freq = sample_rate / 2.0;
                        
                        if (max_freq >= 1000000.0) {
                            freq_unit_str = "MHz";
                            freq_multiplier = 1000000.0;
                        } else if (max_freq >= 1000.0) {
                            freq_unit_str = "kHz";
                            freq_multiplier = 1000.0;
                        } else {
                            freq_unit_str = "Hz";
                            freq_multiplier = 1.0;
                        }
                        unit_str = freq_unit_str;
                    } else {
                        unit_str = display_unit_str;
                    }
                    
                    if (g_has_287) {
                        x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                        current_sample = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                        sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                    } else {
                        current_sample = g_traces[selected_trace].data_count / 2;
                        sample_to_x_scale = (float)GRAPH_WIDTH / (float)(g_traces[selected_trace].data_count - 1);
                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                    }
                    
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    
                    need_redraw = 1;
                }
                continue;
            }
            
            /* Handle command keys */
            switch(toupper(key)) {
                case 27:  /* ESC - Exit */
                    done = 1;
                    break;
                    
                case 'A':  /* Auto-scale */
                    g_graph_scale.auto_scale = 1;
                    auto_scale_graph();
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case 'R':  /* Manual redraw */
                    need_redraw = 1;
                    cursor_visible = 1;
                    if (selected_trace >= 0 && g_traces[selected_trace].enabled && keyboard_mode) {
                        x_normalized = (float)(cursor_x - GRAPH_LEFT) / (float)GRAPH_WIDTH;
                        current_sample = (int)(x_normalized * (g_traces[selected_trace].data_count - 1) + 0.5);
                        if (current_sample < 0) current_sample = 0;
                        if (current_sample >= g_traces[selected_trace].data_count) {
                            current_sample = g_traces[selected_trace].data_count - 1;
                        }
                    }
                    break;
                    
                case '+':  /* Zoom in with precision handling */
                    if (g_has_287) {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) * 0.5;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.4;
                    } else {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 0.8;
                    }
                    
                    /* Precision zoom controls for very small ranges */
                    if (range < 0.000010) {  /* Less than 10µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 1.0) range_uv = 1.0;  /* Minimum 1µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000050) {  /* Less than 50µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.005) {  /* Less than 5mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
                        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    else if (range < 0.050) {  /* Less than 50mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
                        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    
                    g_graph_scale.min_value = center - range;
                    g_graph_scale.max_value = center + range;
                    g_graph_scale.auto_scale = 0;
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case '-':  /* Zoom out with precision handling */
                    if (g_has_287) {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) * 0.5;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.625;
                    } else {
                        center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2;
                        range = (g_graph_scale.max_value - g_graph_scale.min_value) / 2;
                        range *= 1.25;
                    }
                    
                    /* Precision zoom controls for very small ranges */
                    if (range < 0.000010) {  /* Less than 10µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)(range_uv + 0.5));  /* Round to 1µV */
                        if (range_uv < 1.0) range_uv = 1.0;  /* Minimum 1µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.000050) {  /* Less than 50µV total range */
                        float range_uv = range * 1000000.0;  /* Convert to µV */
                        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
                        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
                        range = range_uv / 1000000.0;  /* Convert back to V */
                    }
                    else if (range < 0.005) {  /* Less than 5mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
                        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    else if (range < 0.050) {  /* Less than 50mV total range */
                        float range_mv = range * 1000.0;  /* Convert to mV */
                        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
                        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
                        range = range_mv / 1000.0;  /* Convert back to V */
                    }
                    
                    g_graph_scale.min_value = center - range;
                    g_graph_scale.max_value = center + range;
                    g_graph_scale.auto_scale = 0;
                    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
                    get_graph_units(y_range, &display_unit_str, &display_scale_factor, &display_decimal_places);
                    unit_str = display_unit_str;
                    if (g_has_287) {
                        y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                    }
                    need_redraw = 1;
                    break;
                    
                case 0:  /* Extended key (arrow keys) */
                    key = getch();
                    switch(key) {
                        case 72:  /* Up arrow - pan up */
                            shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                            g_graph_scale.min_value += shift;
                            g_graph_scale.max_value += shift;
                            g_graph_scale.auto_scale = 0;
                            if (g_has_287) {
                                y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                            }
                            need_redraw = 1;
                            break;
                            
                        case 80:  /* Down arrow - pan down */
                            shift = (g_graph_scale.max_value - g_graph_scale.min_value) * 0.1;
                            g_graph_scale.min_value -= shift;
                            g_graph_scale.max_value -= shift;
                            g_graph_scale.auto_scale = 0;
                            if (g_has_287) {
                                y_scale = (float)GRAPH_HEIGHT / (g_graph_scale.max_value - g_graph_scale.min_value);
                            }
                            need_redraw = 1;
                            break;
                            
                        case 75:  /* Left arrow - move one sample left */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                if (current_sample > 0) {
                                    current_sample--;
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                    }
                                    cursor_visible = 1;
                                }
                            }
                            break;
                            
                        case 77:  /* Right arrow - move one sample right */
                            if (selected_trace >= 0 && g_traces[selected_trace].enabled) {
                                keyboard_mode = 1;
                                if (current_sample < g_traces[selected_trace].data_count - 1) {
                                    current_sample++;
                                    if (g_has_287) {
                                        cursor_x = GRAPH_LEFT + (int)(current_sample * sample_to_x_scale);
                                    } else {
                                        cursor_x = GRAPH_LEFT + (int)((float)current_sample * sample_to_x_scale);
                                    }
                                    cursor_visible = 1;
                                }
                            }
                            break;
                    }
                    break;
                    
                case 'M':  /* Toggle mouse */
                    if (g_mouse.present) {
                        mouse_visible = !mouse_visible;
                        if (!mouse_visible) {
                            keyboard_mode = 1;
                        }
                    }
                    break;
                    
                case 'C':  /* Clear data */
                    for (i = 0; i < 10; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    text_mode();
                    printf("\nAll data cleared. Press any key...");
                    getch();
                    done = 1;
                    break;
                
                case 'P':  /* Print graph */
                    text_mode();
                    print_graph_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
                    
                case 'H':  /* Help/Config menu */
                case 'h':
                    text_mode();
                    graph_config_menu();
                    need_redraw = 1;
                    init_graphics();
                    break;
            }
        }
        
        /* Small delay to prevent excessive CPU usage */
        delay(10);
    }
    
    /* Return to text mode before exiting */
    text_mode();
}