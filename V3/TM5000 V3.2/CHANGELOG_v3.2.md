# TM5000 v3.2 Changelog

## Version 3.2 - Enhanced Stability
*Started: June 2025*

### 🔧 Version Update
- Updated all source files from v3.1 to v3.2
- Updated all header files to match
- Updated makefile version references
- Created v3.2 documentation structure

### 🐛 Bug Fixes
- **CRITICAL: Fixed cursor functionality - cursor not visible and arrow keys not working**
  - **Root Cause**: cursor_visible never initialized to 1, selected_trace had no default
  - **Solution**: Initialize cursor_visible = 1 on graph entry (ui.c:1126-1127)
  - **Solution**: Prioritize slot 0 selection, fallback to any active slot (ui.c:1070-1081)
  - **Impact**: Cursor visible immediately, arrow keys work for sample-based navigation

- **CRITICAL: Fixed FFT grid display showing "+01" scientific notation artifacts**
  - **Root Cause**: draw_frequency_grid() using voltage formatting for negative dB values
  - **Solution**: Added FFT-specific auto-scaling with minimum 20dB range (graphics.c:325-363) 
  - **Solution**: Clean dB formatting eliminates scientific notation in grid labels
  - **Impact**: Clean integer dB increments on grid lines when FFT traces displayed

- **CRITICAL: Fixed FFT cursor crash when selected as active trace**
  - Fixed bounds checking in data access (ui.c:1316-1321)
  - Fixed FFT frequency calculation to account for centered data (ui.c:1328-1331)
  - FFT cursor now shows correct frequency relative to peak center
  - Eliminated array access violations in FFT trace selection

- **CRITICAL: Fixed CGA graphics mode compatibility for FFT and counter units**
  - Fixed lowercase unit display causing "large M" artifacts in CGA mode
  - Changed all cursor readouts to uppercase: HZ, KHZ, MHZ, GHZ, DB (ui.c:1334-1351)
  - Fixed grid labels to use uppercase "DB" instead of "dB" (graphics.c:748)
  - All units now properly display in CGA graphics mode without artifacts

- **CRITICAL: Fixed GPIB driver error on program exit (regression from v3.0+)**
  - Added missing GPIB cleanup commands: "abort" and "reset" (main.c:260-264)
  - Restored proper driver shutdown sequence from v2.9 with 500ms reset delay
  - Added input buffer draining to ensure clean driver state before exit
  - Eliminates "GPIB primary address driver error" that appeared in v3.0+ modular versions

### 🖨️ Enhanced Printing System
- **CRITICAL: Fixed FFT PostScript printing showing voltage units instead of dB**
  - **Root Cause**: sync_traces_with_modules() was overriding FFT unit_type every graph display
  - **Solution**: Preserve FFT unit_type when description == "FFT Result" (modules.c:1534-1555)
  - **Impact**: All FFT PostScript output now shows correct "dB" units throughout
  - **Fixed**: Units/div, Min/Max/Avg statistics, and bottom legend all display "dB"

- **NEW: FFT trace printing support with proper dB units and frequency X-axis**
  - Text printing: Shows frequency scale instead of sample numbers for FFT traces
  - PostScript printing: Professional frequency-labeled X-axis with Hz/kHz/MHz scaling
  - Proper dB units displayed on Y-axis for FFT magnitude traces
  - FFT traces automatically use "Magnitude [dB]" Y-axis labeling

- **CRITICAL: Fixed configuration and measurement data save/load failures**
  - **Root Cause**: fseek(fp, -strlen(line), SEEK_CUR) fails in DOS buffered I/O
  - **Solution**: Eliminated all fseek operations in section parsing
  - **Fixed in load_settings()**: Used goto for direct [Modules] section processing
  - **Fixed in load_data()**: Removed fseek from 7 module config loading functions
  - **Impact**: Configuration files (.cfg) now load modules reliably
  - **Impact**: Measurement data files (.tm5) now load without errors

- **NEW: Counter trace printing support with Hz/MHz/GHz units**
  - Added get_trace_print_units() function for trace-specific unit handling
  - Enhanced unit detection based on trace unit_type (0=Voltage, 1=Frequency, 2=dB)
  - Counter traces properly display in Hz, kHz, MHz, or GHz as appropriate
  - Frequency traces use "Frequency [Hz/kHz/MHz]" Y-axis labeling

- **Enhanced print statistics and legends**
  - Trace statistics (min/max/average/RMS) use proper units for each trace type
  - PostScript legends show correct units for FFT and counter measurements  
  - Both text and PostScript formats automatically adapt to trace content

### 🎯 Goals for v3.2
Future enhancements and stability improvements will be tracked here.

---

## Version 3.1 - Production Release
*Released: June 2025*

### Major Achievements:
- ✅ Fixed persistent module loading configuration bug
- ✅ Enhanced configuration persistence system with module-specific settings
- ✅ Fixed FFT trace crashes when made active or when becoming dominant unit type
- ✅ Added Hz/MHz/GHz and dB units system with intelligent priority
- ✅ Enhanced DC5009/DC5010 counter implementation with complete GPIB command set
- ✅ Added ieee_spoll() function for proper GPIB status polling

### Architecture:
- Complete module initialization chain in configuration loading
- Enhanced configuration file format with [ModuleConfigs] section
- Special dB grid display handling for FFT traces
- Unit priority system: dB > frequency > voltage
- FFT peak centering with frequency X-axis display

---

*For complete v3.1 history, see CHANGELOG_v3.1.md*