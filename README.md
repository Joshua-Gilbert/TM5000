# TM5000 GPIB Control System Series 3.0 
## Professional Instrument Control for DOS Systems

![TM5000](https://img.shields.io/badge/Version-3.5-brightgreen) ![Platform](https://img.shields.io/badge/Platform-DOS%2016--bit-blue) ![Architecture](https://img.shields.io/badge/Architecture-Modular-orange) ![License](https://img.shields.io/badge/License-Research-lightgrey)

### 🎯 **Overview**

The TM5000 GPIB Control System Series 3.0 is a comprehensive instrument control and data acquisition platform for DOS systems. Originally developed for the Gridcase 1520 portable computer, this system provides professional-grade control of Tektronix TM5000 series instruments via GPIB (IEEE-488) interface.

### 🔧 **System Capabilities**

#### **Instrument Support**
- **DM5010**: 4½ digit multimeter with high-speed acquisition
- **DM5120**: 6½ digit precision multimeter with advanced math functions
- **PS5004**: Single-channel precision power supply (0-20V, 300mA)
- **PS5010**: Dual-channel power supply with logic supply (0-32V, 0.75A)
- **DC5009**: Universal counter/timer with advanced triggering
- **DC5010**: Enhanced counter with rise/fall time measurement
- **FG5010**: Function generator

#### **Data Acquisition Features**
- **Multi-Module Support**: Up to 10 instrument slots simultaneously
- **High-Resolution Sampling**: 1024 samples per module (v3.4+)
- **Flexible Sample Rates**: 100ms to 10s intervals with custom rates
- **Continuous Monitoring**: Real-time data acquisition with live display
- **Buffer Management**: Intelligent memory allocation and optimization

#### **Mathematical Analysis**
- **FFT Analysis**: Power-of-2 optimized Fast Fourier Transform (64-1024 points)
- **Window Functions**: Rectangular, Hamming, Hanning, Blackman
- **Statistical Functions**: Min, Max, Mean, Standard Deviation, RMS
- **Waveform Math**: Differentiation, integration, scaling, offset, digital filtering, dual trace operations, linear regression, and more!
- **Peak Detection**: Automatic peak finding with centering options
- **287 Math Coprocessor**: Optimized calculations when available

#### **Display and Visualization**
- **CGA Graphics**: 320×200 4-color graphics with engineering grid
- **Multi-Trace Display**: Up to 10 simultaneous traces with color coding
- **Auto-Scaling**: Intelligent range detection with manual override
- **Unit Management**: Automatic V/mV/µV scaling based on signal range
- **Zoom and Pan**: Ultra-precision zoom down to 1µV per division
- **Cursor Measurements**: Interactive measurement with live readouts

#### **Data Management**
- **File Formats**: Native .tm5 measurement files and .cfg configuration files
- **Export Options**: CSV, text, and PostScript formats
- **Configuration Persistence**: Save/restore complete system setups
- **Legacy Compatibility**: Backward compatibility with earlier data formats
- **Automatic Backup**: Intelligent configuration preservation

#### **Printing and Documentation**
- **PostScript Output**: Professional-quality plots with proper scaling
- **Brother Printer Support**: Direct parallel port printing
- **Custom Headers**: User-defined plot titles and annotations
- **Scale Documentation**: Automatic legend generation with units
- **Engineering Format**: IEEE-standard scientific notation support

### 🏗️ **Technical Architecture**

#### **Modular Design (v3.0+)**
The Series 3.0 represents a complete architectural rewrite from the original monolithic v2.9:

```
TM5000 Architecture
### Core System Files
- `main.c` - Main program entry point
- `tm5000.h` - Primary header with all structure definitions
- `tm5000.exe` - Compiled executable (282KB)
- `makefile` - OpenWatcom build configuration
### Module Files
- `modules.c/.h` - GPIB instrument module management
- `module_funcs.c/.h` - Per-module function implementations
- `gpib.c/.h` - GPIB communication layer
- `data.c/.h` - Data buffer management
### User Interface
- `ui.c/.h` - Primary user interface
- `ui_enhanced.c` - Enhanced v3.5 menus (stub)
- `ui_math_menus.c` - Mathematical analysis menus
- `graphics.c/.h` - CGA graphics and plotting
- `print.c/.h` - Report generation and printing
### Mathematical Functions
- `math_functions.c/.h` - Core FFT and mathematical analysis
- `math_enhanced.c` - Advanced math functions
### New v3.5 Features
- `config_profiles.c/.h` - Configuration save/load system
- `export_enhanced.c` - Advanced CSV export with metadata
### Assembly Optimizations
- `cga_asm.asm` - CGA graphics acceleration
- `mem286.asm` - 286 memory operations
- `fixed286.asm` - Fixed-point arithmetic
- `trig287_simple.asm` - Basic 287 trigonometry
```

#### **Memory Management**
- **Dynamic Allocation**: Intelligent buffer sizing based on usage
- **DOS Compatibility**: Optimized for 640KB conventional memory
- **Segment Management**: Each module stays under 64KB DOS limit
- **Far Memory**: Strategic use of extended addressing for data buffers
- **Memory Optimization**: 22% reduction in v3.4 through buffer optimization

#### **Version Evolution**
| Version | Release | Key Features |
|---------|---------|--------------|
| **v3.0** | June 2025 | Modular architecture, DOS segment compliance |
| **v3.1** | June 2025 | Full instrument support, 287 optimizations |
| **v3.2** | June 2025 | Enhanced stability, configuration persistence |
| **v3.3** | June 2025 | File I/O reliability, symbol conflict resolution |
| **v3.4** | July 2025 | 1024-sample buffers, memory optimization |

### 🔨 **System Requirements**

#### **Minimum Requirements**
- **Computer**: IBM PC-compatible with 80286 processor or higher
- **Memory**: 640KB conventional RAM (1MB recommended)
- **Graphics**: CGA-compatible graphics adapter
- **Storage**: 770KB (fits on single high-density floppy disk)
- **Operating System**: MS-DOS 3.3 or higher
- **GPIB Interface**: National Instruments GPIB card with DOS drivers

#### **Recommended Configuration**
- **Gridcase 1520** portable computer (original target platform)
- **80287 Math Coprocessor** for enhanced calculation performance
- **Mouse**: Microsoft-compatible mouse for improved navigation
- **Printer**: PostScript-compatible or Brother dot-matrix printer
- **GPIB Instruments**: Tektronix TM5000 series modules

#### **Development Requirements** (for building from source)
- **Compiler**: OpenWatcom C/C++ 1.9 with DOS target support
- **Assembler**: MASM or compatible for low-level GPIB routines
- **Tools**: Standard DOS development utilities (MAKE, LINK)

### ⚠️ **Limitations and Restrictions**

#### **System Limitations**
- **DOS 16-bit Only**: No Windows, Linux, or modern OS support
- **CGA Graphics**: 320×200 4-color display
- **Memory Constraints**: 640KB conventional memory limit
- **GPIB Required**: Requires GPIB interface hardware
- **Tektronix Specific**: Optimized for TM5000 series instruments only
- **Maximum 10 Instruments**: Single GPIB bus limit
- **1024 Sample Limit**: Per-module buffer maximum
- **No Network Support**: Local operation only

### 💾 **Installation and Usage**

#### **Quick Start**
1. **Prepare DOS System**: Ensure DOS 3.3+ with GPIB drivers installed
2. **Copy Files**: Copy all TM5000 files to floppy disk or hard drive
3. **Verify GPIB**: Test GPIB interface with `DRVR488.EXE` loaded. (National Instruments Driver)
4. **Launch Program**: Execute `TM5000.EXE` from DOS prompt
5. **Configure Modules**: Use main menu to detect and configure instruments

#### **Basic Operation**
```
Main Menu → Configure Modules → [Select Instrument Type] → [Set Parameters]
         → Measurement Operations → [Continuous/Single] → [Start Acquisition]
         → File Operations → [Save Data/Configuration] → [Export Results]
```

#### **Advanced Features**
- **FFT Analysis**: Data → Mathematical Functions → FFT Analysis
- **Multi-Trace Display**: View → Graph Display → [Enable Multiple Traces]
- **Custom Sampling**: Configure → Sample Rate → [Custom Rate Entry]
- **Precision Measurements**: Use DM5120 with enhanced configuration

### 📚 **Documentation Structure**

```
TM5000 Documentation
├── README.md                    # Current overview document
├── TM5000_v3.4_Structure.md     # v3.4 architecture details
├── CHANGELOG_v3.3.md            # Version history
├── Installation_Guide.md        # Setup instructions
└── User_Manual.md               # Complete operation guide
```

### 🔬 **Educational Use**

The TM5000 system serves as an educational platform for:
- **DOS Programming**: Understanding 16-bit software architecture
- **Instrument Control**: Learning GPIB/IEEE-488 communication protocols
- **Real-Time Systems**: Studying deterministic timing in constrained environments
- **Embedded Programming**: Working within memory and processing constraints

### 📄 **Licensing**

The TM5000 GPIB Control System is provided for research and educational purposes.

**Copyright © 2025 - For Educational and Research Use**

---

*TM5000 GPIB Control System Series 3.0 - Professional Instrument Control for DOS*
