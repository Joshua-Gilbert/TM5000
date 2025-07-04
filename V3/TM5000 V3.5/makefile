# TM5000 GPIB Control System Makefile
# Version 3.5 - Assembly Optimized for 286/287/CGA
# OpenWatcom C/C++ 16-bit DOS compilation with assembly optimizations

# Compiler settings
CC = /mnt/c/WATCOM/BINNT/wcc.exe
ASM = /mnt/c/WATCOM/BINNT/wasm.exe
LINKER = /mnt/c/WATCOM/BINNT/wlink.exe
CFLAGS = -ml -2 -bt=dos -os -d0 -zc -zt100
ASMFLAGS = -ml -2
TARGET = tm5000.exe

# Object files with assembly optimizations
OBJS = main.obj gpib.obj modules.obj graphics.obj ui.obj data.obj print.obj math_functions.obj math_enhanced.obj ui_math_menus.obj module_funcs.obj ieeeio_w.obj config_profiles.obj export_enhanced.obj cga_asm.obj mem286.obj trig287_simple.obj fixed286.obj

# Default target
all: $(TARGET)

# Link executable with assembly optimizations
$(TARGET): $(OBJS)
	$(LINKER) system dos file main.obj,gpib.obj,modules.obj,graphics.obj,ui.obj,data.obj,print.obj,math_functions.obj,math_enhanced.obj,ui_math_menus.obj,module_funcs.obj,ieeeio_w.obj,config_profiles.obj,export_enhanced.obj,cga_asm.obj,mem286.obj,trig287_simple.obj,fixed286.obj name $(TARGET)

# Compile main program
main.obj: main.c tm5000.h
	$(CC) $(CFLAGS) main.c

# Compile GPIB module
gpib.obj: gpib.c gpib.h tm5000.h
	$(CC) $(CFLAGS) gpib.c

# Compile modules support
modules.obj: modules.c modules.h tm5000.h gpib.h
	$(CC) $(CFLAGS) modules.c

# Compile graphics module
graphics.obj: graphics.c graphics.h tm5000.h
	$(CC) $(CFLAGS) graphics.c

# Compile UI module
ui.obj: ui.c ui.h tm5000.h graphics.h
	$(CC) $(CFLAGS) ui.c

# Compile data management
data.obj: data.c data.h tm5000.h
	$(CC) $(CFLAGS) data.c

# Compile printing module
print.obj: print.c print.h tm5000.h graphics.h
	$(CC) $(CFLAGS) print.c

# Compile math functions module
math_functions.obj: math_functions.c math_functions.h tm5000.h
	$(CC) $(CFLAGS) math_functions.c

# Compile enhanced math functions module
math_enhanced.obj: math_enhanced.c math_functions.h tm5000.h
	$(CC) $(CFLAGS) math_enhanced.c

# Compile math UI menus module
ui_math_menus.obj: ui_math_menus.c ui.h math_functions.h tm5000.h
	$(CC) $(CFLAGS) ui_math_menus.c

# Compile module functions
module_funcs.obj: module_funcs.c module_funcs.h tm5000.h
	$(CC) $(CFLAGS) module_funcs.c

# Compile external GPIB library
ieeeio_w.obj: ieeeio_w.c
	$(CC) $(CFLAGS) ieeeio_w.c

# Compile configuration profiles module
config_profiles.obj: config_profiles.c config_profiles.h tm5000.h
	$(CC) $(CFLAGS) config_profiles.c

# Compile enhanced export module
export_enhanced.obj: export_enhanced.c data.h tm5000.h
	$(CC) $(CFLAGS) export_enhanced.c

# Assembly modules for 286/287 optimizations
cga_asm.obj: cga_asm.asm
	$(ASM) $(ASMFLAGS) cga_asm.asm

mem286.obj: mem286.asm
	$(ASM) $(ASMFLAGS) mem286.asm

trig287.obj: trig287.asm
	$(ASM) $(ASMFLAGS) -fp2 trig287.asm

trig287_simple.obj: trig287_simple.asm
	$(ASM) $(ASMFLAGS) -fp2 trig287_simple.asm

fixed286.obj: fixed286.asm
	$(ASM) $(ASMFLAGS) fixed286.asm


# Clean build files (works on both DOS and Linux)
clean:
	-del *.obj 2>nul || rm -f *.obj
	-del $(TARGET) 2>nul || rm -f $(TARGET)

# Alternative compilation using wcl (if preferred)
wcl: main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c
	/mnt/c/WATCOM/BINNT/wcl.exe $(CFLAGS) main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c -fe=$(TARGET)

# Help target
help:
	@echo TM5000 Version 3.5 Assembly Optimized Build System
	@echo =================================================
	@echo ""
	@echo Available targets:
	@echo   all     - Build complete system with assembly optimizations (default)
	@echo   wcl     - Build using wcl single command (C only)
	@echo   clean   - Remove object files and executable
	@echo   help    - Show this help
	@echo.
	@echo C Module structure:
	@echo   main.c           - Main program and initialization
	@echo   gpib.c           - GPIB communication functions
	@echo   modules.c        - Instrument module support
	@echo   graphics.c       - Display and graphics functions (CGA optimized)
	@echo   ui.c             - User interface and menus
	@echo   data.c           - Data management and storage
	@echo   print.c          - Printing and export functions
	@echo   math_functions.c - Mathematical functions (287 optimized)
	@echo   math_enhanced.c  - Enhanced math operations
	@echo.
	@echo Assembly Optimizations (286/287/CGA):
	@echo   fft_286.asm   - 287-optimized FFT butterfly operations
	@echo   cga_asm.asm   - CGA graphics acceleration
	@echo   mem286.asm    - 286 memory operations
	@echo   trig287.asm   - 287 trigonometric functions
	@echo   fixed286.asm  - Fixed-point math fallback
	@echo.
	@echo Compiler flags: $(CFLAGS)
	@echo Assembly flags: $(ASMFLAGS)
	@echo Target: $(TARGET)

.PHONY: all clean wcl help