# TM5000 makefile: OpenWatcom wmake, 16-bit DOS, large-model C.

# Default is a WSL build calling the Windows tools; override for a native build:
#   wmake WATCOM_BIN=C:/WATCOM/binnt
WATCOM_BIN = /mnt/c/WATCOM/BINNT
CC = $(WATCOM_BIN)/wcc.exe
ASM = $(WATCOM_BIN)/wasm.exe
LINKER = $(WATCOM_BIN)/wlink.exe
# -zm: one segment per function so OPTION ELIMINATE can drop uncalled ones.
CFLAGS = -ml -2 -bt=dos -os -d0 -zc -zt100 -zm
# No -ml: the .asm files declare .MODEL SMALL and wasm 2.0 rejects the mismatch.
# trig287 adds -fp2 in its own rule.
ASMFLAGS = -2
TARGET = tm5000.exe

OBJS = main.obj gpib.obj modules.obj graphics.obj ui.obj data.obj print.obj math_functions.obj math_enhanced.obj ui_math_menus.obj module_funcs.obj ieeeio_w.obj config_profiles.obj export_enhanced.obj cga_asm.obj mem286.obj trig287_simple.obj fixed286.obj

all: $(TARGET)

# OPTION ELIMINATE drops unreferenced segments: with -zm, uncalled C functions
# and the unused assembly modules stay out of the image. The file list is not
# $(OBJS): keep the two in sync.
$(TARGET): $(OBJS)
	$(LINKER) option eliminate system dos file main.obj,gpib.obj,modules.obj,graphics.obj,ui.obj,data.obj,print.obj,math_functions.obj,math_enhanced.obj,ui_math_menus.obj,module_funcs.obj,ieeeio_w.obj,config_profiles.obj,export_enhanced.obj,cga_asm.obj,mem286.obj,trig287_simple.obj,fixed286.obj name $(TARGET)

main.obj: main.c tm5000.h gpib.h graphics.h ui.h data.h
	$(CC) $(CFLAGS) main.c

gpib.obj: gpib.c gpib.h tm5000.h
	$(CC) $(CFLAGS) gpib.c

modules.obj: modules.c modules.h gpib.h graphics.h tm5000.h
	$(CC) $(CFLAGS) modules.c

graphics.obj: graphics.c graphics.h tm5000.h
	$(CC) $(CFLAGS) graphics.c

ui.obj: ui.c ui.h graphics.h module_funcs.h math_functions.h modules.h config_profiles.h data.h tm5000.h
	$(CC) $(CFLAGS) ui.c

data.obj: data.c data.h modules.h tm5000.h
	$(CC) $(CFLAGS) data.c

print.obj: print.c print.h graphics.h tm5000.h
	$(CC) $(CFLAGS) print.c

math_functions.obj: math_functions.c tm5000.h math_functions.h
	$(CC) $(CFLAGS) math_functions.c

math_enhanced.obj: math_enhanced.c math_functions.h modules.h tm5000.h
	$(CC) $(CFLAGS) math_enhanced.c

ui_math_menus.obj: ui_math_menus.c tm5000.h math_functions.h ui.h
	$(CC) $(CFLAGS) ui_math_menus.c

module_funcs.obj: module_funcs.c modules.h module_funcs.h gpib.h tm5000.h
	$(CC) $(CFLAGS) module_funcs.c

# External GPIB driver library
ieeeio_w.obj: ieeeio_w.c ieeeio.h
	$(CC) $(CFLAGS) ieeeio_w.c

config_profiles.obj: config_profiles.c config_profiles.h data.h modules.h tm5000.h
	$(CC) $(CFLAGS) config_profiles.c

export_enhanced.obj: export_enhanced.c data.h modules.h tm5000.h
	$(CC) $(CFLAGS) export_enhanced.c

# Assembly modules (unreferenced; kept out of the exe by OPTION ELIMINATE)
cga_asm.obj: cga_asm.asm
	$(ASM) $(ASMFLAGS) cga_asm.asm

mem286.obj: mem286.asm
	$(ASM) $(ASMFLAGS) mem286.asm

trig287_simple.obj: trig287_simple.asm
	$(ASM) $(ASMFLAGS) -fp2 trig287_simple.asm

fixed286.obj: fixed286.asm
	$(ASM) $(ASMFLAGS) fixed286.asm


# del for DOS/Windows, rm as the fallback under WSL/Linux
clean:
	-del *.obj 2>nul || rm -f *.obj
	-del $(TARGET) 2>nul || rm -f $(TARGET)
	-del *.err 2>nul || rm -f *.err
	-del *.map 2>nul || rm -f *.map

# Incomplete single-command build: omits math_enhanced.c, ui_math_menus.c,
# config_profiles.c, export_enhanced.c and the .asm files. Use `all`.
wcl: main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c
	$(WATCOM_BIN)/wcl.exe $(CFLAGS) main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c -fe=$(TARGET)

help:
	@echo TM5000 Version 3.6 Build System
	@echo ===============================
	@echo ""
	@echo Available targets:
	@echo   all     - Build tm5000.exe (default)
	@echo   wcl     - Partial single-command build (C only, incomplete)
	@echo   clean   - Remove object files and executable
	@echo   help    - Show this help
	@echo.
	@echo C Module structure:
	@echo   main.c           - Main program and initialization
	@echo   gpib.c           - GPIB communication functions
	@echo   modules.c        - Instrument module support
	@echo   graphics.c       - CGA display and graph engine
	@echo   ui.c             - User interface and menus
	@echo   data.c           - Data management and storage
	@echo   print.c          - Text and PostScript printing
	@echo   math_functions.c - FFT, calculus, smoothing, statistics
	@echo   math_enhanced.c  - Dual-trace math, filters, curve fit, correlation
	@echo.
	@echo Assembly modules (unreferenced; dropped by OPTION ELIMINATE):
	@echo   cga_asm.asm         - CGA line/clear/copy routines
	@echo   mem286.asm          - 286 memory operations
	@echo   trig287_simple.asm  - 287 trigonometric helpers
	@echo   fixed286.asm        - Fixed-point math fallback
	@echo.
	@echo Compiler flags: $(CFLAGS)
	@echo Assembly flags: $(ASMFLAGS)
	@echo Target: $(TARGET)

.PHONY: all clean wcl help