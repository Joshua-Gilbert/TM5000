# TM5000 Control System v3.3

## Version 3.3 - Critical Bug Fix Release
*Released: June 2025*

Building on the enhanced stability of v3.2, version 3.3 resolves critical file I/O reliability issues that prevented proper loading of saved configurations and measurement data.

## 🔧 Key Features

### From V3.3 (New)
- **Fixed Critical Stack Overflow**: Module selection menu no longer crashes
- **Eliminated Symbol Conflicts**: Removed 7 duplicate function definitions
- **Fixed Configuration Loading**: Modules now load reliably from .cfg files
- **Fixed Measurement Data Loading**: .tm5 files load without errors
- **Improved File I/O**: Eliminated unreliable fseek operations
- **Enhanced Stability**: Zero linker warnings, clean compilation

### From V3.2 (Inherited)
- **Stable Base**: Built on proven v3.1 module loading and FFT fixes
- **Enhanced Configuration**: Module-specific settings saved between sessions
- **Advanced Units System**: Smart priority (dB > frequency > voltage)
- **Complete Counter Support**: Full DC5009/DC5010 GPIB implementation
- **Crash-Free FFT**: Fixed cursor crashes, display artifacts, and grid scaling

### From V3.1 (Inherited)
- **Module Support**: All TM5000 modules (DM5010/5120, PS5004/5010, DC5009/5010, FG5010)
- **Advanced Functions**: FFT analysis, statistics, waveform math
- **Professional Printing**: PostScript and text output with proper units
- **Smart Memory**: Automatic and manual allocation management

## 🐛 Critical Bug Fixes

### Stack Overflow and Memory Corruption
- **Problem**: Module selection menu caused PostScript output, garbage, and crashes
- **Root Cause**: Multiple functions defined in different files causing symbol conflicts
- **Discovery**: 7 functions had duplicate definitions across modules
- **Solution**: Eliminated all duplicate symbols, maintained single source of truth
- **Impact**: Module selection works reliably, zero linker warnings

### Configuration and Measurement Data Loading
- **Problem**: Files would fail to load with modules not appearing after loading
- **Root Cause**: `fseek(fp, -strlen(line), SEEK_CUR)` fails in DOS buffered I/O
- **Solution**: Eliminated all fseek operations in file parsing
- **Impact**: Both .cfg and .tm5 files now load reliably

## 📋 System Requirements

- **Computer**: Gridcase 1520 or compatible DOS system
- **Memory**: 640KB RAM minimum
- **Graphics**: CGA graphics adapter
- **GPIB**: National Instruments GPIB card with DOS drivers
- **DOS**: MS-DOS 3.3 or higher recommended
- **Compiler**: OpenWatcom C/C++ 1.9 (for rebuilding)

## 🚀 Installation

1. Copy all files to your Gridcase 1520
2. Ensure GPIB drivers are installed and configured
3. Run `TM5000.EXE` from DOS prompt

## 📁 Files Included

- `TM5000.EXE` - Main executable (v3.3)
- `*.C` - Source code files
- `*.H` - Header files  
- `MAKEFILE` - Build configuration
- `README_v3.3.md` - This file
- `CHANGELOG_v3.3.md` - Detailed changes

## 📖 Documentation

- [CHANGELOG_v3.3.md](CHANGELOG_v3.3.md) - Complete version history
- [TM5000_v3.3_Structure.md](TM5000_v3.3_Structure.md) - Architecture guide

## 🔄 Version History

- **v3.3** (June 2025) - Critical bug fix for file loading
- **v3.2** (June 2025) - Enhanced stability release
- **v3.1** (June 2025) - Major expansion with full TM5000 support
- **v3.0** (June 2025) - Modular architecture rewrite
- **v2.9** - Legacy monolithic version

## ⚡ Quick Start

1. Configure modules: Main Menu → Configure Modules
2. Select measurement mode: Single or Continuous
3. Save configurations: File Operations → Save Settings
4. Load saved data: File Operations → Load Data

## 🛠️ Building from Source

```bash
make clean
make
```

Requires OpenWatcom C/C++ 1.9 with DOS target support.

---
*TM5000 Control System - Professional GPIB instrument control for the Gridcase 1520*
