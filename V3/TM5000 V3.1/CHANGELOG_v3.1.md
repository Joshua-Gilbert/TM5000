# TM5000 v3.1 Changelog

## Version 3.1 - Development
*Started: [Current Date]*

### 🔧 Version Update
- Updated all source files from v3.0 to v3.1
- Updated all header files to match
- Updated makefile version references
- Created v3.1 documentation structure

### 🐛 Bug Fixes
- **CRITICAL FIX: Module loading configuration restored**
  - Fixed load_settings() to include complete module initialization chain
  - Added missing module-specific configuration initialization
  - Added missing buffer allocation for loaded modules  
  - Added missing sync_traces_with_modules() call to connect modules to traces
  - Added missing last_reading initialization and gpib_remote() calls
  - Fixes "No modules configured" error after successful config load

- **MAJOR: Enhanced configuration persistence system**
  - Added [ModuleConfigs] section to save/load detailed module settings
  - DM5120 settings now preserved: LF termination, function, range, filter
  - DC5009/DC5010 settings now preserved: function, channel selections  
  - Module configurations no longer lost when reloading saved files
  - Fixes persistent bug where detailed module settings were not saved

- Fixed trace toggle functionality in graph display
  - Added 'T' key to access trace selection menu
  - Fixed ALT+0-9 key handling for DOS extended key codes
  - Now properly detects ALT key combinations (scan codes 120-129)

- **Fixed FFT trace crash when made active**
  - CRITICAL: Fixed missing trace data pointer in FFT function (math_functions.c:360-362)
  - Added division by zero protection in FFT frequency calculation (ui.c:1325)
  - Added NaN detection for invalid x_scale values
  - Added fallback protection in FFT freq_resolution calculation
  - MAJOR: Fixed grid display crash when dB units become dominant (graphics.c:744-769)
  - Added special dB grid labeling logic to handle negative dB ranges properly
  - FFT traces now display safely when volts traces are removed

### 📝 Enhancements
- **287 Math Coprocessor Optimizations** (per scientific programming guide)
  - Implemented Kahan summation for high-precision accumulation
  - Added 287-safe trigonometric functions with proper range reduction
  - Enhanced FPU precision control (extended 64-bit mode)
  - Improved Taylor series approximations for software fallback
  - Fixed accumulation errors in integration and smoothing functions

- **Enhanced Floating Point Drift Correction**
  - Replaced log10/pow operations with precise lookup table
  - Added 287-specific machine epsilon thresholds
  - Enhanced precision detection using double arithmetic
  - Optimized engineering value snapping for 1-2-5 scales
  - Reduced false corrections with adaptive thresholds

- **Enhanced Units System** 
  - Added Hz/MHz/GHz units for counter and FFT traces
  - Added dB units for FFT Y-axis display
  - Implemented intelligent FFT peak centering during generation
  - Added frequency X-axis support for FFT spectrum analysis
  - Smart unit priority system (dB > frequency > voltage)
  - Enhanced readouts with appropriate units per trace type

- **DC5009/DC5010 Counter Enhancements**
  - Added complete GPIB command set per programming guide
  - Implemented query commands (FUNC?, ID?, ERR?)
  - Added TIME AB/BA and EVENTS BA specific commands  
  - Enhanced with TOT A+B/A-B modes for DC5010
  - Added manual timing (TMAN) and propagation delay (PROB A&B)
  - Implemented Service Request (SRQ) and status byte support
  - Added preset function (PRE ON/OFF) and extended range calculations

- Graph display now supports two methods to toggle traces:
  - Press 'T' for trace selection menu (recommended)
  - Use ALT+0 through ALT+9 to toggle individual traces

### 🔨 Build System & Compilation
- **Added Missing GPIB Function**
  - Implemented ieee_spoll() function in gpib.c for DC5009/DC5010 status polling
  - Added ieee_spoll() declaration to gpib.h 
  - Resolves linker error for counter status byte functions
  - Executable: tm5000.exe (175 KB) successfully built

### 🐛 Known Issues
*(To be documented as discovered)*

---

## Version 3.0 - Production Release
*Released: Previously*

### Major Achievements:
- ✅ Resolved DOS segment size limitation through modularization
- ✅ Fixed critical buffer overflows and array bounds errors
- ✅ Enhanced graph display with crash protection
- ✅ Implemented ultra-precision zoom (1µV/div)
- ✅ Added direct voltage scale entry
- ✅ Extended slot support (0-9)
- ✅ Added mouse support to measurement operations
- ✅ Universal LF termination for all GPIB instruments

### Architecture:
- Split monolithic 302KB source into 9 modules
- Largest module now 122KB (60% reduction)
- Clean separation of concerns
- Maintained 100% backward compatibility

---

*For complete v3.0 history, see CHANGELOG_v3.0.md*