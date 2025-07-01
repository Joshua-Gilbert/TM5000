# TM5000 GPIB Control System Makefile
# Version 3.2 - Enhanced Stability
# OpenWatcom C/C++ 16-bit DOS compilation

# Compiler settings
CC = /mnt/c/WATCOM/BINNT/wcc.exe
LINKER = /mnt/c/WATCOM/BINNT/wlink.exe
CFLAGS = -ml -2 -bt=dos -os -d0
TARGET = tm5000.exe

# Object files
OBJS = main.obj gpib.obj modules.obj graphics.obj ui.obj data.obj print.obj math_functions.obj module_funcs.obj ieeeio_w.obj

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJS)
	$(LINKER) system dos file main.obj,gpib.obj,modules.obj,graphics.obj,ui.obj,data.obj,print.obj,math_functions.obj,module_funcs.obj,ieeeio_w.obj name $(TARGET)

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

# Compile module functions
module_funcs.obj: module_funcs.c module_funcs.h tm5000.h
	$(CC) $(CFLAGS) module_funcs.c

# Compile external GPIB library
ieeeio_w.obj: ieeeio_w.c
	$(CC) $(CFLAGS) ieeeio_w.c

# Clean build files
clean:
	del *.obj
	del $(TARGET)

# Alternative compilation using wcl (if preferred)
wcl: main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c
	/mnt/c/WATCOM/BINNT/wcl.exe $(CFLAGS) main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c module_funcs.c ieeeio_w.c -fe=$(TARGET)

# Help target
help:
	@echo TM5000 Version 3.1 Build System
	@echo ==============================
	@echo.
	@echo Available targets:
	@echo   all     - Build complete system (default)
	@echo   wcl     - Build using wcl single command
	@echo   clean   - Remove object files and executable
	@echo   help    - Show this help
	@echo.
	@echo Module structure:
	@echo   main.c     - Main program and initialization
	@echo   gpib.c     - GPIB communication functions
	@echo   modules.c  - Instrument module support
	@echo   graphics.c - Display and graphics functions
	@echo   ui.c       - User interface and menus
	@echo   data.c     - Data management and storage
	@echo   print.c    - Printing and export functions
	@echo.
	@echo Compiler flags: $(CFLAGS)
	@echo Target: $(TARGET)

.PHONY: all clean wcl help