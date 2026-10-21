# TM5000 makefile: OpenWatcom wmake, 16-bit DOS, large-model C.
# Builds tm5000.exe (Driver488) and tm5000g.exe (GRIDGPIB).

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
# Same program on the GRIDGPIB resident driver
TARGET_G = tm5000g.exe

OBJS = main.obj gpib.obj modules.obj graphics.obj ui.obj data.obj print.obj math_functions.obj math_enhanced.obj ui_math_menus.obj module_funcs.obj ieeeio_w.obj config_profiles.obj export_enhanced.obj cga_asm.obj mem286.obj trig287_simple.obj fixed286.obj

# gpib_gg + gridgpib replace gpib. modules.c and module_funcs.c compile again
# with -DGRIDGPIB, which drops the fixed GPIB waits (GPIB_PACE in tm5000.h).
OBJS_G = main.obj gpib_gg.obj gridgpib.obj modules_g.obj graphics.obj ui.obj data.obj print.obj math_functions.obj math_enhanced.obj ui_math_menus.obj module_funcs_g.obj ieeeio_w.obj config_profiles.obj export_enhanced.obj cga_asm.obj mem286.obj trig287_simple.obj fixed286.obj

all: $(TARGET) $(TARGET_G)

# OPTION ELIMINATE drops unreferenced segments: with -zm, uncalled C functions
# and the unused assembly modules stay out of the image. The file list is not
# $(OBJS): keep the two in sync.
$(TARGET): $(OBJS)
	$(LINKER) option eliminate system dos file main.obj,gpib.obj,modules.obj,graphics.obj,ui.obj,data.obj,print.obj,math_functions.obj,math_enhanced.obj,ui_math_menus.obj,module_funcs.obj,ieeeio_w.obj,config_profiles.obj,export_enhanced.obj,cga_asm.obj,mem286.obj,trig287_simple.obj,fixed286.obj name $(TARGET)

# The file list is not $(OBJS_G): keep the two in sync.
$(TARGET_G): $(OBJS_G)
	$(LINKER) option eliminate system dos file main.obj,gpib_gg.obj,gridgpib.obj,modules_g.obj,graphics.obj,ui.obj,data.obj,print.obj,math_functions.obj,math_enhanced.obj,ui_math_menus.obj,module_funcs_g.obj,ieeeio_w.obj,config_profiles.obj,export_enhanced.obj,cga_asm.obj,mem286.obj,trig287_simple.obj,fixed286.obj name $(TARGET_G)

gpib_gg.obj: gpib_gg.c gpib.h gridgpib.h tm5000.h
	$(CC) $(CFLAGS) gpib_gg.c

gridgpib.obj: gridgpib.c gridgpib.h
	$(CC) $(CFLAGS) gridgpib.c

modules_g.obj: modules.c modules.h tm5000.h gpib.h data.h graphics.h
	$(CC) $(CFLAGS) -DGRIDGPIB -fo=modules_g.obj modules.c

module_funcs_g.obj: module_funcs.c module_funcs.h tm5000.h modules.h gpib.h
	$(CC) $(CFLAGS) -DGRIDGPIB -fo=module_funcs_g.obj module_funcs.c

main.obj: main.c tm5000.h gpib.h graphics.h ui.h data.h
	$(CC) $(CFLAGS) main.c

gpib.obj: gpib.c gpib.h tm5000.h
	$(CC) $(CFLAGS) gpib.c

modules.obj: modules.c modules.h tm5000.h gpib.h data.h graphics.h
	$(CC) $(CFLAGS) modules.c

graphics.obj: graphics.c graphics.h tm5000.h data.h
	$(CC) $(CFLAGS) graphics.c

ui.obj: ui.c ui.h tm5000.h graphics.h module_funcs.h math_functions.h modules.h config_profiles.h data.h print.h
	$(CC) $(CFLAGS) ui.c

data.obj: data.c data.h tm5000.h modules.h
	$(CC) $(CFLAGS) data.c

print.obj: print.c print.h tm5000.h graphics.h data.h
	$(CC) $(CFLAGS) print.c

math_functions.obj: math_functions.c math_functions.h tm5000.h data.h
	$(CC) $(CFLAGS) math_functions.c

math_enhanced.obj: math_enhanced.c math_functions.h tm5000.h modules.h data.h
	$(CC) $(CFLAGS) math_enhanced.c

ui_math_menus.obj: ui_math_menus.c ui.h math_functions.h tm5000.h data.h
	$(CC) $(CFLAGS) ui_math_menus.c

module_funcs.obj: module_funcs.c module_funcs.h tm5000.h modules.h gpib.h
	$(CC) $(CFLAGS) module_funcs.c

# External GPIB driver library
ieeeio_w.obj: ieeeio_w.c ieeeio.h
	$(CC) $(CFLAGS) ieeeio_w.c

config_profiles.obj: config_profiles.c config_profiles.h tm5000.h data.h modules.h
	$(CC) $(CFLAGS) config_profiles.c

export_enhanced.obj: export_enhanced.c data.h tm5000.h modules.h
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
	-del $(TARGET_G) 2>nul || rm -f $(TARGET_G)
	-del *.err 2>nul || rm -f *.err
	-del *.map 2>nul || rm -f *.map

# Incomplete single-command build of tm5000.exe only: omits math_enhanced.c,
# ui_math_menus.c, config_profiles.c, export_enhanced.c and the .asm files. Use `all`.
wcl: main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c
	$(WATCOM_BIN)/wcl.exe $(CFLAGS) main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c -fe=$(TARGET)

help:
	@echo TM5000 Version 3.7 Build System
	@echo ===============================
	@echo ""
	@echo Available targets:
	@echo   all     - Build tm5000.exe (Driver488) and tm5000g.exe (GRIDGPIB) (default)
	@echo   wcl     - Partial single-command build of tm5000.exe (C only, incomplete)
	@echo   clean   - Remove object files and both executables
	@echo   help    - Show this help
	@echo.
	@echo C Module structure:
	@echo   main.c           - Main program and initialization
	@echo   gpib.c           - GPIB communication via Driver488 (tm5000.exe)
	@echo   gpib_gg.c        - GPIB communication via GRIDGPIB (tm5000g.exe)
	@echo   gridgpib.c       - GRIDGPIB resident driver interface
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
	@echo Targets: $(TARGET) $(TARGET_G)

.PHONY: all clean wcl help