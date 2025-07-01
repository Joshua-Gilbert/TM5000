/*
 * TM5000 GPIB Control System - GPIB Communication Module
 * Version 3.3
 * Full implementation extracted from TM5000L.c
 * 
 * Version History:
 * 3.0 - Initial extraction from TM5000L.c
 * 3.1 - Version update
 */

#include "gpib.h"

extern void rawmode(int handle);

int ieee_write(const char *str) {
    if (ieee_out < 0) return -1;
    return write(ieee_out, str, strlen(str));
}

int ieee_read(char *buffer, int maxlen) {
    int bytes_read;
    
    if (ieee_in < 0) return -1;
    
    bytes_read = read(ieee_in, buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else if (bytes_read < 0) {
        if (errno == 5) {  /* General failure - often means no data */
            buffer[0] = '\0';
            return 0;  /* Return 0 to indicate no data, not error */
        }
        return bytes_read;
    }
    
    return bytes_read;
}

int gpib_check_srq(int address) {
    char cmd[80];
    char response[10];
    int status;
    
    sprintf(cmd, "spoll %2d\r\n", address);
    ieee_write(cmd);
    delay(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        status = atoi(response);
        return status;
    }
    return 0;
}

int ieee_spoll(int address, unsigned char *status) {
    char cmd[80];
    char response[10];
    int result;
    
    sprintf(cmd, "spoll %2d\r\n", address);
    ieee_write(cmd);
    delay(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        result = atoi(response);
        *status = (unsigned char)(result & 0xFF);
        return 0;  /* Success */
    }
    *status = 0;
    return -1;  /* Error */
}

void gpib_write(int address, char *command) {
    char cmd_buffer[GPIB_BUFFER_SIZE];
    
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 10 && 
            g_system->modules[slot].module_type == MOD_DM5120) {
            gpib_check_srq(address);
            delay(50);
        }
    }
    
    sprintf(cmd_buffer, "output %2d;%s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    if (g_system && address >= 10 && address < 18) {
        int slot = address - 10;
        if (slot >= 0 && slot < 10 && 
            g_system->modules[slot].module_type == MOD_DM5120) {
            delay(100);
        } else {
            delay(50);
        }
    } else {
        delay(50);
    }
}

int gpib_read(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    
    sprintf(cmd_buffer, "enter %2d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(50);
    
    bytes_read = ieee_read(buffer, maxlen);
    return bytes_read;
}

int gpib_read_float(int address, float *value) {
    char buffer[GPIB_BUFFER_SIZE];
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "enter %2d\r\n", address);
    ieee_write(cmd_buffer);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%*[^+-]%f", value) == 1) {
            return 1;
        }
        if (sscanf(buffer, "%f", value) == 1) {
            return 1;
        }
        if (sscanf(buffer, "%e", value) == 1) {
            return 1;
        }
    }
    
    return 0;
}

void gpib_remote(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "remote %2d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);  /* Increased delay for DM5120 */
}

void gpib_local(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "local %2d\r\n", address);
    ieee_write(cmd_buffer);
    delay(50);
}

void gpib_clear(int address) {
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "clear %2d\r\n", address);
    ieee_write(cmd_buffer);
    delay(100);
}

void gpib_write_dm5120(int address, char *command) {
    char cmd_buffer[GPIB_BUFFER_SIZE];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    gpib_check_srq(address);
    delay(50);
    
    sprintf(cmd_buffer, "output %2d; %s%s", address, command, termination);
    ieee_write(cmd_buffer);
    
    delay(200);
}

int gpib_read_dm5120(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "enter %2d%s", address, termination);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    bytes_read = ieee_read(buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

int gpib_read_float_dm5120(int address, float *value) {
    char buffer[GPIB_BUFFER_SIZE];
    char cmd_buffer[80];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "enter %2d%s", address, termination);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "NDCV%e", value) == 1) return 1;
        if (sscanf(buffer, "DCV%e", value) == 1) return 1;
        if (sscanf(buffer, "NACV%e", value) == 1) return 1;
        if (sscanf(buffer, "ACV%e", value) == 1) return 1;
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
    }
    
    return 0;
}

void gpib_remote_dm5120(int address) {
    char cmd_buffer[80];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "remote %2d%s", address, termination);
    ieee_write(cmd_buffer);
    delay(200);  /* Extended delay for DM5120 */
}

void gpib_local_dm5120(int address) {
    char cmd_buffer[80];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "local %2d%s", address, termination);
    ieee_write(cmd_buffer);
    delay(100);
}

void gpib_clear_dm5120(int address) {
    char cmd_buffer[80];
    int slot;
    char *termination;
    
    slot = -1;
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].gpib_address == address && g_system->modules[i].module_type == MOD_DM5120) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot != -1 && g_dm5120_config[slot].lf_termination) {
        termination = "\n";  /* LF only for instruments showing "LF" */
    } else {
        termination = "\r\n";  /* Standard CRLF */
    }
    
    sprintf(cmd_buffer, "abort%s", termination);
    ieee_write(cmd_buffer);
    delay(100);
    
    sprintf(cmd_buffer, "clear %2d%s", address, termination);
    ieee_write(cmd_buffer);
    delay(200);
}

void gpib_write_dm5010(int address, char *command) {
    char cmd_buffer[GPIB_BUFFER_SIZE];
    
    gpib_check_srq(address);
    delay(50);
    
    sprintf(cmd_buffer, "output %2d; %s\r\n", address, command);
    ieee_write(cmd_buffer);
    
    delay(100);
}

int gpib_read_dm5010(int address, char *buffer, int maxlen) {
    char cmd_buffer[80];
    int bytes_read;
    
    sprintf(cmd_buffer, "enter %2d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    bytes_read = ieee_read(buffer, maxlen-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }
    
    return bytes_read;
}

int gpib_read_float_dm5010(int address, float *value) {
    char buffer[GPIB_BUFFER_SIZE];
    char cmd_buffer[80];
    
    sprintf(cmd_buffer, "enter %2d\r\n", address);
    ieee_write(cmd_buffer);
    
    delay(100);
    
    if (ieee_read(buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
        if (sscanf(buffer, "%eV", value) == 1) return 1;
        if (sscanf(buffer, "%eA", value) == 1) return 1;
        if (sscanf(buffer, "%eR", value) == 1) return 1;
    }
    
    return 0;
}

int command_has_response(const char *cmd) {
    if (strncasecmp(cmd, "hello", 5) == 0) return 1;
    if (strncasecmp(cmd, "status", 6) == 0) return 1;
    if (strncasecmp(cmd, "enter", 5) == 0) return 1;
    if (strncasecmp(cmd, "spoll", 5) == 0) return 1;
    
    if (strncasecmp(cmd, "fill", 4) == 0) return 1;
    
    if (strncasecmp(cmd, "output", 6) == 0) return 0;
    if (strncasecmp(cmd, "remote", 6) == 0) return 0;
    if (strncasecmp(cmd, "local", 5) == 0) return 0;
    if (strncasecmp(cmd, "clear", 5) == 0) return 0;
    if (strncasecmp(cmd, "reset", 5) == 0) return 0;  /* Usually no response */
    
    if (strncasecmp(cmd, "break", 5) == 0) return 0;
    
    return 0;
}

void drain_input_buffer(void) {
    char buffer[GPIB_BUFFER_SIZE];
    int bytes_read;
    int total_drained = 0;
    
    while ((bytes_read = read(ieee_in, buffer, sizeof(buffer))) > 0) {
        total_drained += bytes_read;
        if (total_drained > 1024) break;  /* Safety limit */
    }
}

int init_gpib_system(void) {
    int i, bytes_read;
    char response[GPIB_BUFFER_SIZE];
    
    printf("Opening IEEE device handles...\n");
    
    ieee_out = open("\\dev\\ieeeout", O_WRONLY | O_BINARY);
    if (ieee_out < 0) {
        printf("Cannot open \\DEV\\IEEEOUT\n");
        return -1;
    }
    
    printf("Waiting before opening input device...\n");
    delay(400);  /* 400ms delay to let driver settle */
    
    ieee_in = open("\\dev\\ieeein", O_RDONLY | O_BINARY);
    if (ieee_in < 0) {
        printf("Cannot open \\DEV\\IEEEIN\n");
        close(ieee_out);
        return -1;
    }
    
    printf("Setting raw mode...\n");
    rawmode(ieee_out);
    rawmode(ieee_in);  /* May fail on Personal488, that's OK */
    
    printf("Sending BREAK via IOCTL...\n");
    {
        union REGS regs;
        struct SREGS segs;
        char break_cmd[] = "BREAK";
        
        regs.x.ax = 0x4403;     /* IOCTL write */
        regs.x.bx = ieee_out;   /* File handle */
        regs.x.cx = 5;          /* Length of "BREAK" */
        regs.x.dx = (unsigned)break_cmd;
        segread(&segs);         /* Get current segment registers */
        
        intdosx(&regs, &regs, &segs);
        
        if (regs.x.cflag) {
            printf("  IOCTL BREAK failed, error code: %d\n", regs.x.ax);
        } else {
            printf("  IOCTL BREAK succeeded\n");
        }
    }
    delay(200);
    
    printf("Initializing IEEE subsystem...\n");
    
    /* Simple v2.9/v3.0 style initialization */
    ieee_write("hello\r\n");
    delay(100);
    
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("IOTech Driver: %s", response);
    } else {
        printf("Warning: No hello response\n");
    }
    
    printf("Querying status...\n");
    ieee_write("status\r\n");
    delay(100);
    
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Driver Status: %s", response);
    }
    
    printf("Draining input buffer...\n");
    drain_input_buffer();
    
    printf("GPIB system initialized successfully.\n");
    return 0;
}

#if 0  /* REMOVED - Duplicate function moved to module_funcs.c */
void gpib_terminal_mode(void) {
    char command[GPIB_BUFFER_SIZE];
    char response[GPIB_BUFFER_SIZE];
    int done = 0;
    int bytes_read;
    int i;
    union REGS regs;
    struct SREGS segs;
    
    clrscr();
    printf("GPIB Terminal Mode - Personal488\n");
    printf("=================================\n\n");
    printf("Enter commands EXACTLY as in KYBDCTRL\n");
    printf("Examples:\n");
    printf("  REMOTE 16\n");
    printf("  OUTPUT 16; voltage?\n");
    printf("  ENTER 16\n");
    printf("  STATUS\n");
    printf("Type 'EXIT' to return\n\n");
    printf("Note: BREAK is sent via IOCTL, not as a command\n\n");
    
    drain_input_buffer();
    
    ieee_write("status\r\n");
    delay(100);
    bytes_read = ieee_read(response, sizeof(response));
    if (bytes_read > 0) {
        printf("Initial Status: %s\n", response);
    }
    
    while (getchar() != '\n');
    
    while (!done) {
        printf("\nGPIB> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }
        
        command[strcspn(command, "\r\n")] = 0;
        
        if (strcasecmp(command, "EXIT") == 0) {
            done = 1;
            continue;
        }
        
        if (strlen(command) == 0) {
            continue;
        }
        
        if (strncasecmp(command, "BREAK", 5) == 0) {
            printf("Sending BREAK via IOCTL...\n");
            
            strcpy(response, "BREAK");
            regs.x.ax = 0x4403;     /* IOCTL write */
            regs.x.bx = ieee_out;   /* File handle */
            regs.x.cx = 5;          /* Length of "BREAK" */
            regs.x.dx = (unsigned)response;
            segread(&segs);         /* Get current segment registers */
            
            intdosx(&regs, &regs, &segs);
            
            if (regs.x.cflag) {
                printf("IOCTL BREAK failed, error: %d\n", regs.x.ax);
            } else {
                printf("IOCTL BREAK succeeded\n");
            }
            delay(200);
            continue;
        }
        
        printf("Sending: '%s\\r\\n'\n", command);
        strcat(command, "\r\n");
        ieee_write(command);
        
        if (command_has_response(command)) {
            printf("Reading response...\n");
            delay(100);
            
            bytes_read = ieee_read(response, sizeof(response)-1);
            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("Response (%d bytes): '%s'\n", bytes_read, response);
                
                printf("Hex dump: ");
                for (i = 0; i < bytes_read && i < 40; i++) {
                    printf("%02X ", (unsigned char)response[i]);
                }
                printf("\n");
            } else {
                printf("No response or timeout\n");
            }
        } else {
            delay(50);
            printf("Command sent (no response expected)\n");
        }
    }
    
    printf("\nExiting terminal mode...\n");
}
#endif

#if 0  /* REMOVED - Duplicate function moved to module_funcs.c */
void send_custom_command(void) {
    char command[GPIB_BUFFER_SIZE];
    char response[GPIB_BUFFER_SIZE];
    int address;
    int bytes_read;
    int i;
    
    clrscr();
    printf("Send Custom GPIB Command\n");
    printf("========================\n\n");
    
    printf("Enter GPIB address (1-30): ");
    scanf("%d", &address);
    
    if (address < 1 || address > 30) {
        printf("Invalid address! Must be 1-30.\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    while (getchar() != '\n');
    
    printf("Enter command to send: ");
    if (!fgets(command, sizeof(command), stdin)) {
        printf("Error reading command\n");
        getch();
        return;
    }
    
    command[strcspn(command, "\r\n")] = 0;
    
    if (strlen(command) == 0) {
        printf("No command entered\n");
        getch();
        return;
    }
    
    printf("\nSending to address %d: '%s'\n", address, command);
    
    gpib_write(address, command);
    
    printf("Command sent. Waiting for response...\n");
    delay(200);
    
    bytes_read = gpib_read(address, response, sizeof(response));
    if (bytes_read > 0) {
        printf("Response (%d bytes): '%s'\n", bytes_read, response);
        
        printf("Hex dump: ");
        for (i = 0; i < bytes_read && i < 40; i++) {
            printf("%02X ", (unsigned char)response[i]);
        }
        printf("\n");
    } else {
        printf("No response received\n");
    }
    
    printf("\nPress any key to continue...");
    getch();
}
#endif

/* Check GPIB error status and handle errors */
void check_gpib_error(void) {
    char status[GPIB_BUFFER_SIZE];
    int bytes_read;
    
    memset(status, 0, sizeof(status));
    
    ieee_write("status\r\n");
    delay(50);
    
    bytes_read = ieee_read(status, sizeof(status));
    
    if (bytes_read > 0) {
        printf("GPIB Status: %s\n", status);
        
        if (strstr(status, "ERROR") || strstr(status, "Error")) {
            strcpy(g_error_msg, status);
            gpib_error = 1;
            
            if (strstr(status, "SEQUENCE")) {
                printf("Attempting to clear SEQUENCE error...\n");
                
                ieee_write("abort\r\n");
                delay(100);
                
                ieee_write("status\r\n");
                delay(50);
                bytes_read = ieee_read(status, sizeof(status));
                if (bytes_read > 0) {
                    printf("Status after abort: %s\n", status);
                    if (!strstr(status, "ERROR")) {
                        gpib_error = 0;  /* Error cleared */
                    }
                }
            }
        } else {
            gpib_error = 0;
        }
    } else {
        printf("Warning: No status response received\n");
        gpib_error = 1;
        strcpy(g_error_msg, "No status response");
    }
}
