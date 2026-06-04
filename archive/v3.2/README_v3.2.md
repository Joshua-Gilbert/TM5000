# TM5000 GPIB Control System v3.2

**Enhanced Stability Release**

TM5000 v3.2 is the enhanced stability release building on the solid foundation of v3.1. This version focuses on long-term reliability and robustness.

## What's New in v3.2

Version 3.2 sets the stage for future stability improvements while maintaining all the enhanced features from v3.1:

- **Stable Base**: Built on v3.1's proven module loading and FFT fixes
- **Enhanced Configuration**: Persistent module-specific settings
- **Advanced Units**: Hz/MHz/GHz and dB support with intelligent priority
- **Complete Counter Support**: Full DC5009/DC5010 GPIB implementation
- **Crash-Free FFT**: Resolved all FFT trace display issues

## System Requirements

- **Hardware**: Gridcase 1520 or compatible 8086/8088 system
- **GPIB Interface**: Personal488 card with DRVR488.EXE driver v2.2+
- **Memory**: 640KB conventional memory recommended
- **Math Coprocessor**: 8087/80287 optional (enhances performance)

## Installation

1. Copy all files to your working directory
2. Ensure DRVR488.EXE is loaded
3. Compile with: `make all`
4. Run: `tm5000.exe`

## Key Features

### 🔧 **Enhanced Module Support**
- **Persistent Configuration**: Module settings saved between sessions
- **Complete Initialization**: Proper module setup on configuration load
- **GPIB Status Polling**: Full ieee_spoll() implementation

### 📊 **Advanced Units System**
- **Smart Priority**: dB > frequency > voltage unit selection
- **Engineering Units**: Automatic Hz/kHz/MHz/GHz scaling
- **FFT Integration**: dB display with frequency X-axis

### 🎯 **DC5009/DC5010 Counters**
- **Complete GPIB Command Set**: All programming guide functions
- **Query Commands**: FUNC?, ID?, ERR? support
- **Advanced Modes**: TIME AB/BA, EVENTS BA, TOT A+B/A-B
- **Status Support**: SRQ and status byte handling

### 📈 **FFT Analysis**
- **Peak Centering**: Automatic centering of highest dB spike
- **Frequency Display**: X-axis shows actual frequencies
- **Crash-Free**: Robust display when FFT traces become active
- **Unit Integration**: Seamless dB/frequency unit switching

## Documentation

- **CHANGELOG_v3.2.md**: Complete version history
- **ENHANCED_UNITS_SYSTEM.md**: Detailed units implementation
- **TM5000_v3.1_Structure.md**: System architecture

## Development Notes

### Build System
- **Compiler**: OpenWatcom C16 with DOS target
- **Standards**: C89 compliant for maximum compatibility
- **Memory Model**: Large model for DOS segment limitations

### Architecture
- **Modular Design**: 9 separate modules for maintainability
- **287 Optimization**: Enhanced performance with math coprocessor
- **Memory Management**: Careful buffer allocation for 640KB limit

## Version History

- **v3.2**: Enhanced stability foundation (Current)
- **v3.1**: Major fixes and unit system enhancements  
- **v3.0**: Modular architecture and stability improvements
- **v2.9**: Legacy monolithic version

## Support

For issues or questions:
1. Check CHANGELOG_v3.2.md for known issues
2. Verify GPIB driver installation
3. Ensure proper hardware configuration

---

**TM5000 v3.2 - Built for reliability on the Gridcase 1520**