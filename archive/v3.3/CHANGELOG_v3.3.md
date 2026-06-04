# TM5000 v3.3 Changelog

## Version 3.3 - Critical Bug Fix Release
*Released: June 2025*

### 🐛 Critical Bug Fixes

- **CRITICAL: Fixed stack overflow and memory corruption in module selection**
  - **Root Cause**: Multiple symbol definitions causing linker conflicts
  - **Symptoms**: Module selection menu caused PostScript output, garbage, and crashes
  - **Discovery**: Function `module_selection_menu()` defined in 3 different files
  - **Solution**: Eliminated 7 duplicate function definitions across files
  - **Fixed Functions**: 
    - `module_selection_menu` (ui.c vs modules.c vs TM5000L.c)
    - `enhanced_font` (graphics.c vs main.c vs TM5000L.c)
    - `send_custom_command` (module_funcs.c vs gpib.c vs TM5000L.c)
    - `configure_dm5120_advanced` (module_funcs.c vs modules.c vs TM5000L.c)
    - `configure_ps5004_advanced` (module_funcs.c vs modules.c vs TM5000L.c)
    - `configure_ps5010_advanced` (module_funcs.c vs modules.c vs TM5000L.c)
    - `gpib_terminal_mode` (module_funcs.c vs gpib.c vs TM5000L.c)
  - **Files Modified**: main.c, gpib.c, modules.c, modules.h
  - **Impact**: Module selection menu now works reliably without crashes
  - **Result**: Zero linker warnings, clean compilation, reduced code size

- **CRITICAL: Fixed ghost/phantom modules in continuous monitoring**
  - **Root Cause**: Enabled modules without valid configuration were displayed
  - **Symptoms**: Empty or invalid modules shown in continuous monitor and selection menu
  - **Solution**: Added validation checks for module type, GPIB address, and description
  - **Fixed in continuous_monitor()**: Display loop now validates modules before showing
  - **Fixed in module_selection_menu()**: Only valid modules appear in selection list
  - **Files Modified**: modules.c, ui.c
  - **Impact**: Clean display showing only properly configured modules

- **CRITICAL: Fixed configuration and measurement data save/load failures**
  - **Root Cause**: `fseek(fp, -strlen(line), SEEK_CUR)` fails in DOS buffered I/O environments
  - **Solution**: Eliminated all unreliable fseek operations in file parsing
  - **Fixed in load_settings()**: 
    - Used goto label for direct [Modules] section processing
    - Eliminated fseek when transitioning between sections
  - **Fixed in load_data()**: 
    - Removed fseek from all 7 module config loading functions:
      - `load_dm5120_config()`
      - `load_dm5010_config()`
      - `load_ps5004_config()`
      - `load_ps5010_config()`
      - `load_dc5009_config()`
      - `load_dc5010_config()`
      - `load_fg5010_config()`
  - **Impact**: 
    - Configuration files (.cfg) now load modules reliably every time
    - Measurement data files (.tm5) now load without "failed to read" errors
    - File parsing is more robust and predictable

### 📝 Technical Details

The fseek issue manifested as:
- Modules configured in settings but not appearing after load
- "Failed to read sample" errors when loading measurement data
- Inconsistent file parsing behavior

The fix involved:
- Removing 9 instances of `fseek(fp, -strlen(line), SEEK_CUR)`
- Implementing direct section transitions without file repositioning
- Allowing natural file stream progression through sections

### ✨ Enhanced FFT Analysis System
*Released: June 2025*

- **MAJOR: Configurable FFT with full 1000-point capability**
  - **Input sizes**: 64, 128, 256, 512, 1024 points (up to 1000-point module buffer limit)
  - **Output resolution**: 32, 64, 128, 256, 512, 1024 points (configurable)
  - **Window functions**: 4 types with 287-optimized trigonometry
    - Rectangular (no windowing)
    - Hamming (good general purpose) 
    - Hanning (smooth frequency response)
    - Blackman (excellent sidelobe suppression)
  - **Output formats**: 3 configurable formats
    - dB Magnitude (original format)
    - Linear Magnitude (direct amplitude)
    - Power Spectrum (magnitude squared)
  - **Processing options**:
    - Configurable zero padding to next power of 2
    - DC component removal before FFT
    - Custom sample rate override
  - **Configuration menu**: Hierarchical interface matching TM5000 style
    - Real-time memory usage display
    - Input/output size validation
    - Settings persistence via global configuration

- **Technical Implementation**:
  - **Memory efficient**: Reuses existing 1000-point module slot system
  - **287-optimized**: Uses existing `cos_287_safe()` for window calculations
  - **Working space**: ~8KB maximum for largest 1024-point configuration
  - **C89 compliant**: Full compatibility with OpenWatcom 16-bit DOS compiler
  - **Files modified**: tm5000.h, main.c, math_functions.c, math_functions.h
  - **Code size**: +1,762 bytes for complete configurable FFT system
  
- **NEW: Peak Centering Feature**:
  - **Configurable peak centering**: Option to center FFT output on highest peak
  - **Circular shift algorithm**: Automatically centers peak in middle of display
  - **Frequency offset compensation**: X-axis adjusted to show relative frequencies
  - **All format support**: Works with dB, Linear, and Power spectrum outputs
  - **Smart peak detection**: Excludes DC component (0Hz) from peak finding
  - **Default enabled**: Peak centering enabled by default to match original behavior

- **NEW: Enhanced Differentiation with Proper Units**:
  - **Derivative unit type**: Added UNIT_DERIVATIVE (V/s) for differentiation results
  - **Multi-scale units**: Automatic µV/s, mV/s, V/s scaling based on magnitude
  - **Graph integration**: Proper Y-axis labels and scaling in graph display
  - **Print support**: Full PostScript printing with correct derivative units
  - **Trace setup**: Complete trace configuration with color and labeling
  - **Unit priority**: Derivative traces prioritized over voltage in mixed displays

### 🔧 Display and Cursor Fixes

- **FIXED: dB Gridline Formatting**:
  - **Proper decimal places**: dB gridlines now show 1 decimal place (e.g., "-20.0" instead of "-20")
  - **Consistent rounding**: Values properly rounded for clean display
  - **CGA compatibility**: All graphics mode text remains uppercase for CGA support

- **FIXED: Cursor Functionality for Special Traces**:
  - **FFT cursor fix**: Corrected frequency calculation using x_scale and x_offset
  - **Peak centering support**: Cursor now works correctly with centered FFT displays
  - **Derivative cursor**: Added proper V/s unit display for differentiation traces
  - **Multi-scale readout**: µV/s, mV/s, V/s automatic scaling in cursor display
  - **Frequency accuracy**: FFT cursor shows accurate frequency regardless of centering mode

### ⚡ New Current Units Support

- **NEW: Complete Current Measurement Units**:
  - **Unit type**: Added UNIT_CURRENT for ampere measurements
  - **Multi-scale units**: Automatic µA, mA, A scaling based on magnitude
  - **CGA compatible**: All units use uppercase (UA, MA, A) for graphics mode
  - **Graph integration**: Proper Y-axis labels and scaling for current traces
  - **Cursor support**: Current readout with automatic unit scaling
  - **Print support**: Full PostScript printing with current units (uA, mA, A)
  - **Unit priority**: Current traces prioritized between derivative and frequency
  - **Module ready**: Compatible with DM5010/DM5120 CURR function and PS power supplies

- **User Experience**:
  - **Backward compatible**: Existing FFT workflow preserved
  - **Configuration first**: Menu appears before execution for parameter selection
  - **Smart defaults**: 256→128 points, Hamming window, dB output
  - **Comprehensive feedback**: Shows configuration summary and memory usage
  - **Trace integration**: Results automatically setup with appropriate units and scaling

### ⚡ Complete Resistance Units Support
*NEW: Professional-Grade Ω/mΩ/µΩ Implementation*

- **NEW: Complete Resistance Measurement Units**:
  - **Unit type**: Added UNIT_RESISTANCE for ohm measurements  
  - **Multi-scale units**: Automatic µΩ, mΩ, Ω scaling based on magnitude
  - **CGA compatible**: All units use uppercase (UO, MO, O) for graphics mode
  - **Graph integration**: Proper Y-axis labels and scaling for resistance traces
  - **Cursor support**: Resistance readout with automatic unit scaling
  - **Print support**: Full PostScript printing with resistance units (µΩ, mΩ, Ω)
  - **Unit priority**: Resistance traces prioritized between current and frequency
  - **Module ready**: Compatible with DM5120 OHMS function and resistance modules

- **Enhanced Graph Legend System**:
  - **FIXED: Derivative traces** now show as "DERIV" instead of "UNKNOWN"
  - **NEW: Resistance traces** show as "OHMS" in legend
  - **Improved trace identification**: Clear labeling for all special trace types
  - **Priority-based display**: Trace type takes precedence over module name

- **Complete Integration**:
  - **Graphics system**: Full resistance unit support in grid scaling and display
  - **User interface**: Cursor display with automatic resistance unit scaling
  - **Print system**: PostScript output with proper resistance symbols
  - **Legend system**: Clear identification of resistance measurements
  - **C89 compliant**: Full compatibility with OpenWatcom 16-bit DOS compiler

### 🔧 Version Update
- Updated all source files from v3.2 to v3.3
- Updated all header files to match
- Updated version macros in tm5000.h

---

## Previous Versions

### Version 3.2 - Enhanced Stability
*Released: June 2025*

- Fixed cursor functionality and visibility
- Fixed FFT grid display artifacts
- Fixed FFT cursor crashes
- Fixed CGA graphics compatibility
- Fixed GPIB driver shutdown errors
- Enhanced printing system with FFT/counter support
- Complete counter trace printing

### Version 3.1 - Major Expansion
*Released: June 2025*

- Added support for all TM5000 modules
- Implemented FFT analysis and waveform math
- Added comprehensive configuration system
- Professional PostScript printing
- Smart memory management

### Version 3.0 - Modular Architecture
*Released: June 2025*

- Complete rewrite with modular design
- Split monolithic code for DOS segment limits
- Maintained backward compatibility
- Enhanced maintainability

---
*For complete details on previous versions, see their respective CHANGELOG files*