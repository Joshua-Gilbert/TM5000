/*
 * TM5000 GPIB Control System - Graphics Module
 * Version 3.3
 * Complete implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 */

#include "graphics.h"

/* Initialize CGA graphics mode */
void init_graphics(void) {
    union REGS regs;
    regs.h.ah = 0x00;  /* Set video mode */
    regs.h.al = 0x04;  /* 320x200 4-color CGA graphics */
    int86(0x10, &regs, &regs);
    
    /* Set CGA color palette */
    outportb(0x3D9, 0x30);
}

/* Return to text mode */
void text_mode(void) {
    union REGS regs;
    regs.h.ah = 0x00;  /* Set video mode */
    regs.h.al = 0x03;  /* 80x25 color text mode */
    int86(0x10, &regs, &regs);
}

/* Clear screen */
void clrscr(void) {
    union REGS regs;
    regs.w.ax = 0x0600;  /* Scroll window up */
    regs.h.bh = 0x07;    /* White on black */
    regs.h.ch = 0;       /* Top row */
    regs.h.cl = 0;       /* Left column */
    regs.h.dh = 24;      /* Bottom row */
    regs.h.dl = 79;      /* Right column */
    int86(0x10, &regs, &regs);
    
    gotoxy(1, 1);
}

/* Position cursor */
void gotoxy(int x, int y) {
    union REGS regs;
    regs.h.ah = 0x02;    /* Set cursor position */
    regs.h.bh = 0;       /* Video page 0 */
    regs.h.dh = y - 1;   /* Row (0-based) */
    regs.h.dl = x - 1;   /* Column (0-based) */
    int86(0x10, &regs, &regs);
}

/* Set text attribute */
void textattr(unsigned char attr) {
    union REGS regs;
    regs.h.ah = 0x09;    /* Write character and attribute */
    regs.h.bl = attr;    /* Attribute */
    regs.w.cx = 0;       /* Don't write any characters */
    regs.h.bh = 0;       /* Video page 0 */
    int86(0x10, &regs, &regs);
}

/* Clear to end of line */
void clreol(void) {
    union REGS regs;
    int x, y;
    
    /* Get current cursor position */
    regs.h.ah = 0x03;
    regs.h.bh = 0;
    int86(0x10, &regs, &regs);
    x = regs.h.dl;
    y = regs.h.dh;
    
    /* Clear from cursor to end of line */
    regs.h.ah = 0x09;     /* Write character and attribute */
    regs.h.al = ' ';      /* Space character */
    regs.h.bh = 0;        /* Video page 0 */
    regs.h.bl = 0x07;     /* Normal attribute (white on black) */
    regs.w.cx = 80 - x;   /* Number of characters to end of line */
    int86(0x10, &regs, &regs);
}

/* Plot a pixel in CGA graphics mode */
void plot_pixel(int x, int y, unsigned char color) {
    unsigned int offset;
    unsigned char shift, mask;
    
    /* Bounds checking */
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
        return;
    
    /* Calculate video memory offset for CGA interlaced memory layout */
    offset = (y >> 1) * 80 + (x >> 2);
    if (y & 1) offset += 8192;  /* Odd lines are offset by 8KB */
    
    /* Calculate bit position within byte (4 pixels per byte) */
    shift = 6 - ((x & 3) << 1);  /* 6, 4, 2, 0 */
    mask = ~(3 << shift);        /* Clear existing pixel bits */
    
    /* Set new pixel value */
    video_mem[offset] = (video_mem[offset] & mask) | ((color & 3) << shift);
}

/* Draw a line using Bresenham's algorithm */
void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int e2;
    
    while (1) {
        plot_pixel(x1, y1, color);
        
        if (x1 == x2 && y1 == y2) break;
        
        e2 = err << 1;
        if (e2 > -dy) { 
            err -= dy; 
            x1 += sx; 
        }
        if (e2 < dx) { 
            err += dx; 
            y1 += sy; 
        }
    }
}

/* Draw a rectangle */
void draw_rectangle(int x1, int y1, int x2, int y2, unsigned char color) {
    draw_line(x1, y1, x2, y1, color);  /* Top */
    draw_line(x2, y1, x2, y2, color);  /* Right */
    draw_line(x2, y2, x1, y2, color);  /* Bottom */
    draw_line(x1, y2, x1, y1, color);  /* Left */
}

/* Fill a rectangle */
void fill_rectangle(int x1, int y1, int x2, int y2, unsigned char color) {
    int x, y;
    int temp;
    
    /* Ensure x1,y1 is top-left and x2,y2 is bottom-right */
    if (x1 > x2) { temp = x1; x1 = x2; x2 = temp; }
    if (y1 > y2) { temp = y1; y1 = y2; y2 = temp; }
    
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            plot_pixel(x, y, color);
        }
    }
}

/* Draw anti-aliased line */
void draw_line_aa(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int e2;
    
    while (1) {
        plot_pixel(x1, y1, color);
        
        /* Add anti-aliasing by drawing adjacent pixels with reduced intensity */
        if (dx > dy) {
            if (y1 > 0) 
                plot_pixel(x1, y1-1, (color > 1) ? color-1 : 1);
            if (y1 < SCREEN_HEIGHT-1) 
                plot_pixel(x1, y1+1, (color > 1) ? color-1 : 1);
        } else {
            if (x1 > 0) 
                plot_pixel(x1-1, y1, (color > 1) ? color-1 : 1);
            if (x1 < SCREEN_WIDTH-1) 
                plot_pixel(x1+1, y1, (color > 1) ? color-1 : 1);
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

/* Initialize mouse driver */
int init_mouse(void) {
    union REGS regs;
    
    regs.w.ax = MOUSE_RESET;
    int86(MOUSE_INT, &regs, &regs);
    
    if (regs.w.ax == 0xFFFF) {
        g_mouse.present = 1;
        return 1;  /* Mouse present */
    }
    
    g_mouse.present = 0;
    return 0;  /* No mouse */
}

/* Show mouse cursor */
void show_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SHOW;
    int86(MOUSE_INT, &regs, &regs);
}

/* Hide mouse cursor */
void hide_mouse(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_HIDE;
    int86(MOUSE_INT, &regs, &regs);
}

/* Get current mouse status */
void get_mouse_status(void) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_STATUS;
    int86(MOUSE_INT, &regs, &regs);
    
    g_mouse.x = regs.w.cx / 8;  /* Convert to text coordinates */
    g_mouse.y = regs.w.dx / 8;
    g_mouse.left_button = regs.w.bx & 1;
    g_mouse.right_button = (regs.w.bx & 2) >> 1;
}

/* Set mouse position */
void set_mouse_pos(int x, int y) {
    union REGS regs;
    
    if (!g_mouse.present) return;
    
    regs.w.ax = MOUSE_SET_POS;
    regs.w.cx = x * 8;  /* Convert from text to pixel coordinates */
    regs.w.dx = y * 8;
    int86(MOUSE_INT, &regs, &regs);
}

/* Check if mouse is in specified region and button is pressed */
int mouse_in_region(int x1, int y1, int x2, int y2) {
    get_mouse_status();
    return (g_mouse.x >= x1 && g_mouse.x <= x2 && 
            g_mouse.y >= y1 && g_mouse.y <= y2 &&
            g_mouse.left_button);
}

/* Draw a circle using midpoint circle algorithm */
void draw_circle(int cx, int cy, int radius, unsigned char color) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    
    while (x <= y) {
        /* Draw 8 symmetric points */
        plot_pixel(cx + x, cy + y, color);
        plot_pixel(cx - x, cy + y, color);
        plot_pixel(cx + x, cy - y, color);
        plot_pixel(cx - x, cy - y, color);
        plot_pixel(cx + y, cy + x, color);
        plot_pixel(cx - y, cy + x, color);
        plot_pixel(cx + y, cy - x, color);
        plot_pixel(cx - y, cy - x, color);
        
        if (d < 0) {
            d = d + 2 * x + 3;
        } else {
            d = d + 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/* Draw a filled circle */
void fill_circle(int cx, int cy, int radius, unsigned char color) {
    int x, y;
    int radius_sq = radius * radius;
    
    for (y = -radius; y <= radius; y++) {
        for (x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius_sq) {
                plot_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/* Set CGA color palette */
void set_cga_palette(int palette) {
    /* palette: 0 = green/red/brown, 1 = cyan/magenta/white */
    outportb(0x3D9, 0x30 | (palette ? 0x20 : 0x00));
}

/* Wait for vertical retrace to reduce flicker */
void wait_vretrace(void) {
    /* Wait for start of vertical retrace */
    while (!(inportb(0x3DA) & 0x08));
    /* Wait for end of vertical retrace */
    while (inportb(0x3DA) & 0x08);
}

/* Graphics functions for graph_display */
void auto_scale_graph(void) {
    int i, j;
    float min_val = 1e30, max_val = -1e30;
    int found_data = 0;
    int has_fft_traces = 0;
    float range, margin;
    
    /* Check if we have FFT traces that need dB scaling */
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].unit_type == UNIT_DB) {
            has_fft_traces = 1;
            break;
        }
    }
    
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            for (j = 0; j < g_traces[i].data_count; j++) {
                if (g_traces[i].data[j] < min_val) min_val = g_traces[i].data[j];
                if (g_traces[i].data[j] > max_val) max_val = g_traces[i].data[j];
                found_data = 1;
            }
        }
    }
    
    if (!found_data) {
        g_graph_scale.min_value = 0.0;
        g_graph_scale.max_value = 10.0;
        return;
    }
    
    /* Apply FFT-specific scaling for dB traces */
    if (has_fft_traces) {
        /* For dB traces, use reasonable display range */
        range = max_val - min_val;
        if (range < 20.0) {
            /* Minimum 20dB range for good visibility */
            float center = (max_val + min_val) / 2.0;
            min_val = center - 10.0;
            max_val = center + 10.0;
        }
        /* Add small margin for dB traces */
        margin = (max_val - min_val) * 0.05;  /* 5% margin instead of 10% */
        g_graph_scale.min_value = min_val - margin;
        g_graph_scale.max_value = max_val + margin;
        return;
    }
    
    range = max_val - min_val;
    
    if (range < 0.1) {
        float center = (max_val + min_val) / 2.0;
        min_val = center - 0.5;
        max_val = center + 0.5;
        range = 1.0;
    }
    
    margin = range * 0.1;
    g_graph_scale.min_value = min_val - margin;
    g_graph_scale.max_value = max_val + margin;
    
    range = g_graph_scale.max_value - g_graph_scale.min_value;
    if (range < 0.000050) {  /* Less than 50µV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_uv = range * 1000000.0;  /* Convert to µV */
        range_uv = ((int)((range_uv + 2.5) / 5.0)) * 5.0;  /* Round to 5µV */
        if (range_uv < 5.0) range_uv = 5.0;  /* Minimum 5µV range */
        range = range_uv / 1000000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    else if (range < 0.005) {  /* Less than 5mV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_mv = range * 1000.0;  /* Convert to mV */
        range_mv = ((int)(range_mv * 10.0 + 0.5)) / 10.0;  /* Round to 0.1mV */
        if (range_mv < 0.1) range_mv = 0.1;  /* Minimum 0.1mV range */
        range = range_mv / 1000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    else if (range < 0.050) {  /* Less than 50mV total range */
        float center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
        float range_mv = range * 1000.0;  /* Convert to mV */
        range_mv = ((int)(range_mv + 0.5));  /* Round to 1mV */
        if (range_mv < 1.0) range_mv = 1.0;  /* Minimum 1mV range */
        range = range_mv / 1000.0;  /* Convert back to V */
        g_graph_scale.min_value = center - range / 2.0;
        g_graph_scale.max_value = center + range / 2.0;
    }
    
    if (g_graph_scale.min_value > 0 && g_graph_scale.min_value < 1.0) {
        g_graph_scale.min_value = 0.0;
    }
}

void get_graph_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 0.000001) {
        *unit_str = "UV";
        *scale_factor = 1000000.0;
        *decimal_places = 1;
    } else if (range < 0.001) {
        *unit_str = "UV";
        *scale_factor = 1000000.0;
        *decimal_places = 0;
    } else if (range < 0.01) {
        *unit_str = "MV";
        *scale_factor = 1000.0;
        *decimal_places = 3;  /* Reduced from 4 for cleaner display */
    } else if (range < 0.1) {
        *unit_str = "MV";
        *scale_factor = 1000.0;
        *decimal_places = 2;  /* Reduced from 3 for cleaner display */
    } else if (range < 1.0) {
        *unit_str = "MV";
        *scale_factor = 1000.0;
        *decimal_places = 1;
    } else if (range < 10.0) {
        *unit_str = "MV";
        *scale_factor = 1000.0;
        *decimal_places = 0;
    } else {
        *unit_str = "V";
        *scale_factor = 1.0;
        *decimal_places = 2;  /* Reduced from 3 for cleaner display */
    }
}

/* Get frequency units (Hz, kHz, MHz, GHz) for counter and FFT traces */
void get_frequency_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 1000.0) {
        *unit_str = "HZ";
        *scale_factor = 1.0;
        *decimal_places = 0;
    } else if (range < 1000000.0) {
        *unit_str = "KHZ";
        *scale_factor = 0.001;
        *decimal_places = 1;
    } else if (range < 1000000000.0) {
        *unit_str = "MHZ";
        *scale_factor = 0.000001;
        *decimal_places = 3;
    } else {
        *unit_str = "GHZ";
        *scale_factor = 0.000000001;
        *decimal_places = 6;
    }
}

/* Get dB units for FFT magnitude display */
void get_db_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    *unit_str = "DB";
    *scale_factor = 1.0;
    *decimal_places = 1;
}

/* Get derivative units (V/s) for differentiation display */
void get_derivative_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 0.001) {        /* < 1mV/s range, use µV/s */
        *unit_str = "UV/S";
        *scale_factor = 1000000.0;
        *decimal_places = 1;
    } else if (range < 1.0) {   /* < 1V/s range, use mV/s */
        *unit_str = "MV/S";
        *scale_factor = 1000.0;
        *decimal_places = 1;
    } else {                    /* >= 1V/s range, use V/s */
        *unit_str = "V/S";
        *scale_factor = 1.0;
        *decimal_places = 2;
    }
}

/* Get current units (A, mA, µA) for current measurements */
void get_current_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 0.000001) {     /* < 1µA range */
        *unit_str = "UA";
        *scale_factor = 1000000.0;
        *decimal_places = 1;
    } else if (range < 0.001) {  /* < 1mA range, use µA */
        *unit_str = "UA";
        *scale_factor = 1000000.0;
        *decimal_places = 0;
    } else if (range < 0.01) {   /* < 10mA range, use mA */
        *unit_str = "MA";
        *scale_factor = 1000.0;
        *decimal_places = 3;
    } else if (range < 0.1) {    /* < 100mA range, use mA */
        *unit_str = "MA";
        *scale_factor = 1000.0;
        *decimal_places = 2;
    } else if (range < 1.0) {    /* < 1A range, use mA */
        *unit_str = "MA";
        *scale_factor = 1000.0;
        *decimal_places = 1;
    } else if (range < 10.0) {   /* < 10A range, use mA */
        *unit_str = "MA";
        *scale_factor = 1000.0;
        *decimal_places = 0;
    } else {                     /* >= 10A range, use A */
        *unit_str = "A";
        *scale_factor = 1.0;
        *decimal_places = 2;
    }
}

/* Get resistance units (Ω, mΩ, µΩ) for resistance measurements */
void get_resistance_units(float range, char **unit_str, float *scale_factor, int *decimal_places) {
    if (range < 0.000001) {     /* < 1µΩ range */
        *unit_str = "UO";
        *scale_factor = 1000000.0;
        *decimal_places = 1;
    } else if (range < 0.001) {  /* < 1mΩ range, use µΩ */
        *unit_str = "UO";
        *scale_factor = 1000000.0;
        *decimal_places = 0;
    } else if (range < 0.01) {   /* < 10mΩ range, use mΩ */
        *unit_str = "MO";
        *scale_factor = 1000.0;
        *decimal_places = 3;
    } else if (range < 0.1) {    /* < 100mΩ range, use mΩ */
        *unit_str = "MO";
        *scale_factor = 1000.0;
        *decimal_places = 2;
    } else if (range < 1.0) {    /* < 1Ω range, use mΩ */
        *unit_str = "MO";
        *scale_factor = 1000.0;
        *decimal_places = 1;
    } else if (range < 10.0) {   /* < 10Ω range, use mΩ */
        *unit_str = "MO";
        *scale_factor = 1000.0;
        *decimal_places = 0;
    } else {                     /* >= 10Ω range, use Ω */
        *unit_str = "O";
        *scale_factor = 1.0;
        *decimal_places = 2;
    }
}

void draw_frequency_grid(int fft_samples) {
    int i, x, y, j;
    int pos;
    char label[20];
    float sample_rate;
    float max_freq;
    float freq;
    char *freq_unit;
    float freq_scale;
    float y_range;
    int has_db_traces;
    char *unit_label = "Mag";  /* Magnitude for Y axis */
    
    sample_rate = 1000.0 / g_control_panel.sample_rate_ms;  /* Hz */
    max_freq = sample_rate / 2.0;  /* Nyquist frequency */
    
    if (max_freq >= 1000000.0) {
        freq_unit = "MHZ";
        freq_scale = 1000000.0;
    } else if (max_freq >= 1000.0) {
        freq_unit = "KHZ";
        freq_scale = 1000.0;
    } else {
        freq_unit = "HZ";
        freq_scale = 1.0;
    }
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    
    for (i = 0; i <= 5; i++) {
        float value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            if (i % 2 == 0) {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 3) {
                    plot_pixel(x, pos, 1);
                    if (x+1 <= GRAPH_RIGHT) plot_pixel(x+1, pos, 1);
                }
            } else {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 6) {
                    plot_pixel(x, pos, 1);
                }
            }
            
            /* Check if we have dB traces for proper formatting */
            has_db_traces = 0;
            for (j = 0; j < 10; j++) {
                if (g_traces[j].enabled && g_traces[j].unit_type == UNIT_DB) {
                    has_db_traces = 1;
                    break;
                }
            }
            
            if (has_db_traces) {
                /* For dB values, use simple integer formatting - no scientific notation */
                if (i % 2 == 0) {  /* Only show labels on major grid lines */
                    sprintf(label, "%.0f", value);
                } else {
                    strcpy(label, "");  /* No label on minor grid lines */
                }
            } else {
                /* Original voltage formatting */
                if (value < 0.01) {
                    sprintf(label, "%.1e", value);
                } else if (value < 1.0) {
                    sprintf(label, "%.3f", value);
                } else {
                    sprintf(label, "%.1f", value);
                }
            }
            
            if (strlen(label) > 0) {
                draw_text(2, pos - 3, label, 2);
            }
        }
    }
    
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        if (i % 2 == 0) {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 3) {
                plot_pixel(pos, y, 1);
                if (y+1 <= GRAPH_BOTTOM) plot_pixel(pos, y+1, 1);
            }
        } else {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 6) {
                plot_pixel(pos, y, 1);
            }
        }
        
        freq = max_freq * i / 5.0;
        sprintf(label, "%.1f", freq / freq_scale);
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    sprintf(label, "FREQUENCY (%s)", freq_unit);
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, 5, label, 1);
}

float get_engineering_scale(float range, float *per_div, char **unit_str, int *decimal_places) {
    float raw_per_div, scale_multiplier, magnitude, rounded_per_div, normalized;
    int exponent;
    
    raw_per_div = range / 5.0;  /* 5 divisions */
    scale_multiplier = 1.0;
    
    /* Find the magnitude (power of 10) */
    if (raw_per_div == 0.0) {
        *per_div = 1.0;
        *unit_str = "V";
        *decimal_places = 1;
        return 1.0;
    }
    
    magnitude = log10(fabs(raw_per_div));
    exponent = (int)floor(magnitude);
    
    /* Normalize to 1-10 range */
    normalized = raw_per_div / pow(10.0, exponent);
    
    /* Round to engineering values: 1, 2, 5 */
    if (normalized <= 1.5) {
        rounded_per_div = 1.0 * pow(10.0, exponent);
    } else if (normalized <= 3.0) {
        rounded_per_div = 2.0 * pow(10.0, exponent);
    } else if (normalized <= 7.0) {
        rounded_per_div = 5.0 * pow(10.0, exponent);
    } else {
        rounded_per_div = 10.0 * pow(10.0, exponent);
    }
    
    *per_div = rounded_per_div;
    
    /* Choose units based on per-division value */
    if (rounded_per_div >= 0.1) {
        *unit_str = "V";
        scale_multiplier = 1.0;
        *decimal_places = (rounded_per_div >= 1.0) ? 1 : 2;
    } else if (rounded_per_div >= 0.0001) {
        *unit_str = "MV";
        scale_multiplier = 1000.0;
        *decimal_places = (rounded_per_div >= 0.001) ? 0 : 1;
    } else {
        *unit_str = "UV";
        scale_multiplier = 1000000.0;
        *decimal_places = (rounded_per_div >= 0.000001) ? 0 : 1;
    }
    
    return scale_multiplier;
}

/* 287-optimized engineering value lookup table for precision */
static const double engineering_values[] = {
    1.0e-9, 2.0e-9, 5.0e-9,      /* nanovolts */
    1.0e-8, 2.0e-8, 5.0e-8,
    1.0e-7, 2.0e-7, 5.0e-7,
    1.0e-6, 2.0e-6, 5.0e-6,      /* microvolts */
    1.0e-5, 2.0e-5, 5.0e-5, 8.0e-5,
    1.0e-4, 2.0e-4, 5.0e-4,      /* millivolts */
    1.0e-3, 2.0e-3, 5.0e-3,
    1.0e-2, 2.0e-2, 5.0e-2,
    1.0e-1, 2.0e-1, 5.0e-1,      /* volts */
    1.0, 2.0, 5.0,
    1.0e1, 2.0e1, 5.0e1,
    1.0e2, 2.0e2, 5.0e2,         /* hundreds of volts */
    1.0e3, 2.0e3, 5.0e3
};

#define NUM_ENGINEERING_VALUES (sizeof(engineering_values) / sizeof(engineering_values[0]))

/* 287-optimized snap function using extended precision and lookup table */
void snap_graph_scale_to_clean_values(void) {
    double y_range_d, center_d, per_div_d, clean_per_div;
    double best_diff, current_diff;
    int i, best_index;
    
    /* Use 287 extended precision for calculations */
    if (g_has_287) {
        unsigned short control_word;
        _asm {
            fstcw control_word
        }
        control_word &= ~0x0300;  /* Clear precision bits */
        control_word |= 0x0300;   /* Set extended precision */
        _asm {
            fldcw control_word
            fclex               /* Clear exceptions */
        }
    }
    
    /* Perform calculations in double precision to minimize errors */
    y_range_d = (double)g_graph_scale.max_value - (double)g_graph_scale.min_value;
    center_d = ((double)g_graph_scale.max_value + (double)g_graph_scale.min_value) * 0.5;
    
    /* Calculate current per-division value (5 divisions for screen) */
    per_div_d = y_range_d * 0.2;  /* 1/5 = 0.2, exact in binary */
    
    if (per_div_d <= 0.0) return;  /* Safety check */
    
    /* Find closest engineering value using lookup table */
    best_diff = 1.0e100;  /* Large initial value */
    best_index = 0;
    
    for (i = 0; i < NUM_ENGINEERING_VALUES; i++) {
        current_diff = fabs(per_div_d - engineering_values[i]);
        if (current_diff < best_diff) {
            best_diff = current_diff;
            best_index = i;
        }
    }
    
    clean_per_div = engineering_values[best_index];
    
    /* Set clean range using exact arithmetic */
    y_range_d = clean_per_div * 5.0;
    g_graph_scale.min_value = (float)(center_d - y_range_d * 0.5);
    g_graph_scale.max_value = (float)(center_d + y_range_d * 0.5);
}

void draw_grid_dynamic(int max_samples) {
    int i, x, y;
    float value;
    int pos;
    float y_range;
    char label[20];
    char *unit_label;
    int decimal_places;
    float per_div_value, scale_multiplier;
    float grid_start, grid_step;
    int has_frequency_traces = 0;
    int has_db_traces = 0;
    int has_voltage_traces = 0;
    int has_derivative_traces = 0;
    int has_current_traces = 0;
    int has_resistance_traces = 0;
    
    y_range = g_graph_scale.max_value - g_graph_scale.min_value;
    
    /* C89: Initialize pointer to prevent issues */
    unit_label = "V";
    scale_multiplier = 1.0;
    decimal_places = 1;
    
    /* Determine units based on active traces - simplified logic */
    
    /* Check what types of traces are enabled */
    for (i = 0; i < 10; i++) {
        if (g_traces[i].enabled && g_traces[i].data_count > 0) {
            switch (g_traces[i].unit_type) {
                case UNIT_FREQUENCY: has_frequency_traces = 1; break;
                case UNIT_DB: has_db_traces = 1; break;
                case UNIT_VOLTAGE: has_voltage_traces = 1; break;
                case UNIT_DERIVATIVE: has_derivative_traces = 1; break;
                case UNIT_CURRENT: has_current_traces = 1; break;
                case UNIT_RESISTANCE: has_resistance_traces = 1; break;
            }
        }
    }
    
    /* Unit priority: dB > derivative > current > resistance > frequency > voltage */
    if (has_db_traces) {
        get_db_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    } else if (has_derivative_traces) {
        get_derivative_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    } else if (has_current_traces) {
        get_current_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    } else if (has_resistance_traces) {
        get_resistance_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    } else if (has_frequency_traces) {
        get_frequency_units(y_range, &unit_label, &scale_multiplier, &decimal_places);
    } else {
        /* Default voltage units */
        if (y_range < 0.001) {  /* < 1mV range, use µV */
            unit_label = "UV";  /* Use uppercase - lowercase not in font */
            scale_multiplier = 1000000.0;
            decimal_places = 0;
        } else if (y_range < 1.0) {  /* < 1V range, use mV */
            unit_label = "MV";  /* Use uppercase - lowercase not in font */
            scale_multiplier = 1000.0;
            decimal_places = 0;
        } else {  /* >= 1V range, use V */
            unit_label = "V";
            scale_multiplier = 1.0;
            decimal_places = 1;
        }
    }
    
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_TOP-1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_TOP, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_LEFT-1, GRAPH_TOP-1, GRAPH_LEFT-1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, 3);
    draw_line(GRAPH_RIGHT+1, GRAPH_TOP-1, GRAPH_RIGHT+1, GRAPH_BOTTOM+1, 1);
    draw_line(GRAPH_RIGHT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, 3);
    
    /* Clear unit label area with black rectangle then draw new unit */
    /* Safety check: ensure coordinates are valid */
    if (unit_label != NULL) {
        fill_rectangle(2, 2, 35, 12, 0);  /* Clear larger area to prevent cutoff */
        draw_text_scaled(2, 2, unit_label, 3, 1, 1);
    }
    
    for (i = 0; i <= 5; i++) {
        value = g_graph_scale.min_value + (y_range * i / 5.0);
        pos = GRAPH_BOTTOM - (int)((float)GRAPH_HEIGHT * i / 5.0);
        
        if (pos >= GRAPH_TOP && pos <= GRAPH_BOTTOM) {
            if (i % 2 == 0) {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 3) {
                    plot_pixel(x, pos, 1);
                    if (x+1 <= GRAPH_RIGHT) plot_pixel(x+1, pos, 1);
                }
            } else {
                for (x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 6) {
                    plot_pixel(x, pos, 1);
                }
            }
            
            /* Special handling for dB units */
            if (has_db_traces && scale_multiplier == 1.0 && strcmp(unit_label, "DB") == 0) {
                /* For dB, show rounded values with 1 decimal place */
                sprintf(label, "%.1f", value);
            } else if (scale_multiplier == 1.0) {
                /* For volts, show units on each line */
                if (decimal_places == 0) {
                    sprintf(label, "%.0f%s", value * scale_multiplier, unit_label);
                } else {
                    sprintf(label, "%.*f%s", decimal_places, value * scale_multiplier, unit_label);
                }
            } else {
                /* For mV/µV, just show numbers (units in top left only) */
                if (decimal_places == 0) {
                    sprintf(label, "%.0f", value * scale_multiplier);
                } else {
                    sprintf(label, "%.*f", decimal_places, value * scale_multiplier);
                }
            }
            if (strlen(label) > 0) {  /* Only draw non-empty labels */
                draw_text(2, pos - 3, label, 2);
            }
        }
    }
    
    for (i = 0; i <= 5; i++) {
        pos = GRAPH_LEFT + (GRAPH_WIDTH * i / 5);
        
        if (i % 2 == 0) {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 3) {
                plot_pixel(pos, y, 1);
                if (y+1 <= GRAPH_BOTTOM) plot_pixel(pos, y+1, 1);
            }
        } else {
            for (y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 6) {
                plot_pixel(pos, y, 1);
            }
        }
        
        if (max_samples > 0) {
            int sample_num = (max_samples - 1) * i / 5;
            sprintf(label, "%d", sample_num);
        } else {
            sprintf(label, "%d", i * 20);
        }
        draw_text(pos - 6, GRAPH_BOTTOM + 3, label, 2);
    }
    
    draw_text(GRAPH_LEFT + GRAPH_WIDTH/2 - 30, 5, "TIME (SAMPLES)", 1);
}

int value_to_y(float value) {
    float normalized = (value - g_graph_scale.min_value) / 
                      (g_graph_scale.max_value - g_graph_scale.min_value);
    return GRAPH_BOTTOM - (int)(normalized * GRAPH_HEIGHT);
}

void draw_legend_enhanced(int *is_fft_trace, int selected_trace) {
    int i, y_pos = 2;
    char label[40];
    int legend_x = SCREEN_WIDTH - 58;  /* Moved left by 3 characters (18 pixels) */
    char *module_name;
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            /* Get module name based on trace type first, then module type */
            if (is_fft_trace[i]) {
                module_name = "FFT";
            } else if (g_traces[i].enabled && g_traces[i].unit_type == UNIT_DERIVATIVE) {
                module_name = "DERIV";
            } else if (g_traces[i].enabled && g_traces[i].unit_type == UNIT_RESISTANCE) {
                module_name = "OHMS";
            } else {
                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: module_name = "DC5009"; break;
                    case MOD_DM5010: module_name = "DM5010"; break;
                    case MOD_DM5120: module_name = "DM5120"; break;
                    case MOD_PS5004: module_name = "PS5004"; break;
                    case MOD_PS5010: module_name = "PS5010"; break;
                    case MOD_DC5010: module_name = "DC5010"; break;
                    case MOD_FG5010: module_name = "FG5010"; break;
                    default: module_name = "UNKNOWN"; break;
                }
            }
            
            /* Check if trace is currently active/visible */
            if (g_traces[i].enabled) {
                /* Active traces: show "S0:DM5120" in bright color */
                sprintf(label, "S%d:%s", i, module_name);
                draw_text(legend_x, y_pos, label, 3);
            } else {
                /* Inactive traces: show just "DM5120" in dark color */
                sprintf(label, "%s", module_name);
                draw_text(legend_x, y_pos, label, 1);
            }
            
            y_pos += 12;  /* Compact spacing */
            
            if (y_pos > GRAPH_BOTTOM - 15) break;
        }
    }
}

void draw_text(int x, int y, char *text, unsigned char color) {
    draw_text_scaled(x, y, text, color, 1, 1);
}


/* Additional functions needed for graphics implementation */
int get_module_color(int module_type) {
    switch(module_type) {
        case MOD_DC5009: return 2;  /* Green */
        case MOD_DM5010: return 3;  /* Cyan */
        case MOD_DM5120: return 1;  /* Blue */
        case MOD_PS5004: return 2;  /* Green */
        case MOD_PS5010: return 3;  /* Cyan */
        case MOD_DC5010: return 1;  /* Blue */
        case MOD_FG5010: return 2;  /* Green */
        default: return 1;          /* Blue */
    }
}

/* Font data arrays */
unsigned char enhanced_font[][7] = {
    /* 0x20 space */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x21 ! */     {0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00},
    /* 0x22 " */     {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x23 # */     {0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00},
    /* 0x24 $ */     {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    /* 0x25 % */     {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
    /* 0x26 & */     {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D},
    /* 0x27 ' */     {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x28 ( */     {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    /* 0x29 ) */     {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    /* 0x2A * */     {0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00},
    /* 0x2B + */     {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    /* 0x2C , */     {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08},
    /* 0x2D - */     {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    /* 0x2E . */     {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
    /* 0x2F / */     {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
    /* 0x30 0 */     {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    /* 0x31 1 */     {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 0x32 2 */     {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    /* 0x33 3 */     {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    /* 0x34 4 */     {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    /* 0x35 5 */     {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    /* 0x36 6 */     {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    /* 0x37 7 */     {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04},
    /* 0x38 8 */     {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    /* 0x39 9 */     {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    /* 0x3A : */     {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
    /* 0x3B ; */     {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08},
    /* 0x3C < */     {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},
    /* 0x3D = */     {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
    /* 0x3E > */     {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
    /* 0x3F ? */     {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},
    /* 0x40 @ */     {0x0E, 0x11, 0x01, 0x0D, 0x15, 0x15, 0x0E},
    /* 0x41 A */     {0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11},
    /* 0x42 B */     {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    /* 0x43 C */     {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    /* 0x44 D */     {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    /* 0x45 E */     {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    /* 0x46 F */     {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    /* 0x47 G */     {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    /* 0x48 H */     {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 0x49 I */     {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 0x4A J */     {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    /* 0x4B K */     {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    /* 0x4C L */     {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    /* 0x4D M */     {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    /* 0x4E N */     {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},
    /* 0x4F O */     {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x50 P */     {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    /* 0x51 Q */     {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    /* 0x52 R */     {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    /* 0x53 S */     {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
    /* 0x54 T */     {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    /* 0x55 U */     {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x56 V */     {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    /* 0x57 W */     {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    /* 0x58 X */     {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    /* 0x59 Y */     {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04},
    /* 0x5A Z */     {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    /* Add more characters as needed... */
    /* 0x5B [ */     {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
    /* 0x5C \ */     {0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00},
    /* 0x5D ] */     {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
    /* 0x5E ^ */     {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00},
    /* 0x5F _ */     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}
};

int get_font_index(char c) {
    if (c >= 0x20 && c <= 0x5F) {
        return c - 0x20;
    }
    return 0;  /* space for unknown */
}

void draw_text_scaled(int x, int y, char *text, unsigned char color, int scale_x, int scale_y) {
    int i, j, sx, sy;
    unsigned char font_byte;
    int char_index;
    
    while (*text) {
        char_index = get_font_index(*text);
        
        for (i = 0; i < 7; i++) {
            font_byte = enhanced_font[char_index][i];
            
            for (j = 0; j < 5; j++) {
                if (font_byte & (0x10 >> j)) {
                    for (sy = 0; sy < scale_y; sy++) {
                        for (sx = 0; sx < scale_x; sx++) {
                            plot_pixel(x + j*scale_x + sx, y + i*scale_y + sy, color);
                        }
                    }
                }
            }
        }
        
        x += 6 * scale_x;  /* Character width + spacing */
        text++;
        
        if (x > SCREEN_WIDTH - 6*scale_x) break;
    }
}

/* Draw gradient rectangle for status bars */
void draw_gradient_rect(int x1, int y1, int x2, int y2, unsigned char color1, unsigned char color2) {
    int x, y;
    unsigned char color;
    int height = y2 - y1 + 1;
    
    for (y = y1; y <= y2; y++) {
        /* Calculate gradient color based on vertical position */
        if (height > 1) {
            if ((y - y1) < height / 2) {
                color = color1;
            } else {
                color = color2;
            }
        } else {
            color = color1;
        }
        
        /* Draw horizontal line */
        for (x = x1; x <= x2; x++) {
            plot_pixel(x, y, color);
        }
    }
}

/* Graph configuration menu */
void graph_config_menu(void) {
    int choice;
    int done = 0;
    
    while (!done) {
        clrscr();
        printf("Graph Configuration Menu\n");
        printf("========================\n\n");
        
        printf("Current Settings:\n");
        printf("  Scale: %.6f to %.6f\n", g_graph_scale.min_value, g_graph_scale.max_value);
        printf("  Auto-scale: %s\n", g_graph_scale.auto_scale ? "ON" : "OFF");
        {
            float current_range = g_graph_scale.max_value - g_graph_scale.min_value;
            float per_div = current_range / 5.0;
            if (per_div >= 1.0) {
                printf("  Voltage per div: %.3fV\n", per_div);
            } else if (per_div >= 0.001) {
                printf("  Voltage per div: %.1fmV\n", per_div * 1000.0);
            } else {
                printf("  Voltage per div: %.0fuV\n", per_div * 1000000.0);
            }
        }
        printf("  Sample start: %d\n", g_graph_scale.sample_start);
        printf("  Sample count: %d\n", g_graph_scale.sample_count);
        printf("\n");
        
        printf("1. Set Y-axis scale manually\n");
        printf("2. Toggle auto-scale\n");
        printf("3. Set voltage per division (e.g. 2.5V, 500mV, 50uV)\n");
        printf("4. Set sample range\n");
        printf("5. Reset to defaults\n");
        printf("0. Return to graph\n\n");
        
        printf("Enter choice: ");
        choice = getch();
        
        switch(choice) {
            case '1':
                printf("\nEnter minimum value: ");
                scanf("%f", &g_graph_scale.min_value);
                printf("Enter maximum value: ");
                scanf("%f", &g_graph_scale.max_value);
                g_graph_scale.auto_scale = 0;
                break;
                
            case '2':
                g_graph_scale.auto_scale = !g_graph_scale.auto_scale;
                if (g_graph_scale.auto_scale) {
                    auto_scale_graph();
                }
                break;
                
            case '3':
                {
                    float per_div_value;
                    char unit_input[10];
                    float range, center;
                    
                    printf("\nSet voltage per division\n");
                    printf("Examples: 5.0V, 2.5V, 100mV, 50mV, 10uV, 1uV\n");
                    printf("Enter value and unit (e.g. 2.5V): ");
                    
                    if (scanf("%f%s", &per_div_value, unit_input) == 2) {
                        /* Convert to volts based on unit */
                        if (strstr(unit_input, "uV") || strstr(unit_input, "UV")) {
                            per_div_value = per_div_value / 1000000.0;  /* µV to V */
                        } else if (strstr(unit_input, "mV") || strstr(unit_input, "MV")) {
                            per_div_value = per_div_value / 1000.0;     /* mV to V */
                        }
                        /* Otherwise assume V units */
                        
                        if (per_div_value >= 0.000001 && per_div_value <= 1000.0) {
                            /* Calculate total range (5 divisions) */
                            range = per_div_value * 5.0;
                            
                            /* Center around current center or zero */
                            center = (g_graph_scale.max_value + g_graph_scale.min_value) / 2.0;
                            
                            g_graph_scale.min_value = center - (range / 2.0);
                            g_graph_scale.max_value = center + (range / 2.0);
                            g_graph_scale.auto_scale = 0;
                            
                            printf("Set to %.6f volts per division\n", per_div_value);
                        } else {
                            printf("Invalid range! Use values between 1uV and 1000V per division.\n");
                        }
                    } else {
                        printf("Invalid input format! Use: value + unit (e.g. 2.5V)\n");
                    }
                }
                break;
                
            case '4':
                printf("\nEnter sample start (0 for beginning): ");
                scanf("%d", &g_graph_scale.sample_start);
                printf("Enter sample count (0 for all): ");
                scanf("%d", &g_graph_scale.sample_count);
                if (g_graph_scale.sample_start < 0) g_graph_scale.sample_start = 0;
                if (g_graph_scale.sample_count < 0) g_graph_scale.sample_count = 0;
                break;
                
            case '5':
                g_graph_scale.min_value = -10.0;
                g_graph_scale.max_value = 10.0;
                g_graph_scale.auto_scale = 1;
                g_graph_scale.zoom_factor = 1.0;
                g_graph_scale.sample_start = 0;
                g_graph_scale.sample_count = 0;
                auto_scale_graph();
                break;
                
            case '0':
            case 27:  /* ESC */
                done = 1;
                break;
        }
        
        if (!done && choice != '0' && choice != 27) {
            printf("\nPress any key to continue...");
            getch();
        }
    }
}

/* Draw filled rectangle - wrapper for existing fill_rectangle function */
void draw_filled_rect(int x1, int y1, int x2, int y2, unsigned char color) {
    fill_rectangle(x1, y1, x2, y2, color);
}

/* Draw readout box with text - complete implementation from TM5000L.c */
void draw_readout(int x, int y, char *text) {
    int text_len = strlen(text) * 6;
    int box_x = x + 5;
    int box_y = y - 30;
    int i, j;
    char enhanced_text[80];
    
    /* Position adjustment to keep box on screen */
    if (box_x + text_len + 8 > SCREEN_WIDTH) {
        box_x = x - text_len - 12;
    }
    if (box_y < 0) box_y = y + 5;
    
    /* Fill background with black (color 0) */
    for (i = 0; i < 16; i++) {
        for (j = 0; j < text_len + 8; j++) {
            plot_pixel(box_x + j, box_y + i, 0);
        }
    }
    
    /* Draw top border (2 lines thick) */
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y, 3);     /* Top line */
        plot_pixel(box_x + j, box_y + 1, 1); /* Second line */
    }
    
    /* Draw bottom border (2 lines thick) */
    for (j = 0; j < text_len + 8; j++) {
        plot_pixel(box_x + j, box_y + 15, 3); /* Bottom line */
        plot_pixel(box_x + j, box_y + 14, 1); /* Second line */
    }
    
    /* Draw left border (2 pixels thick) */
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x, box_y + i, 3);     /* Left edge */
        plot_pixel(box_x + 1, box_y + i, 1); /* Second column */
    }
    
    /* Draw right border (2 pixels thick) */
    for (i = 0; i < 16; i++) {
        plot_pixel(box_x + text_len + 7, box_y + i, 3); /* Right edge */
        plot_pixel(box_x + text_len + 6, box_y + i, 1); /* Second column */
    }
    
    /* Copy and enhance text with units */
    strcpy(enhanced_text, text);
    
    /* Auto-add voltage units if not present */
    if (strstr(text, ":") && !strstr(text, "V") && !strstr(text, "mV")) {
        if (g_graph_scale.max_value - g_graph_scale.min_value < 0.1) {
            strcat(enhanced_text, " mV");
        } else {
            strcat(enhanced_text, " V");
        }
    }
    
    /* Draw text with shadow effect */
    draw_text(box_x + 5, box_y + 5, enhanced_text, 1);  /* Shadow (color 1) */
    draw_text(box_x + 4, box_y + 4, enhanced_text, 3);  /* Main text (color 3) */
}
