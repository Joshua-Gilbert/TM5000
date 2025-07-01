# TM5000 GPIB Control System v3.1

## 🔧 **Development Version**

TM5000 v3.1 is the active development version building upon the stable v3.0 release.

### Version History:
- **v3.0** - Production release with modular architecture
- **v3.1** - Current development version

## 🏗️ **Modular Architecture** (Inherited from v3.0)

| Module | Purpose | Size |
|--------|---------|------|
| `main.c` | System initialization & globals | 17KB |
| `gpib.c` | GPIB communication | 18KB |
| `modules.c` | Instrument support | 122KB |
| `graphics.c` | Display & visualization | 38KB |
| `ui.c` | User interface | 66KB |
| `data.c` | Data management | 20KB |
| `print.c` | Printing functions | 38KB |
| `math_functions.c` | Mathematical operations | 15KB |

**Executable: 167KB**

## 🚀 **Quick Start**

### Build TM5000 v3.1:
```bash
# Full build
make all

# Or single command  
wcl -ml -2 -bt=dos -os -d0 main.c gpib.c modules.c graphics.c ui.c data.c print.c math_functions.c ieeeio_w.c
```

### Clean build files:
```bash
make clean
```

## 📝 **Changes in v3.1**

*(To be documented as development progresses)*

## ✅ **Features from v3.0**

1. **✅ DOS Compatibility** - Resolved segment size limitations
2. **✅ Modular Design** - Clean separation of concerns
3. **✅ Buffer Overflow Fixes** - Array bounds corrected
4. **✅ Enhanced Zoom** - Ultra-precision to 1µV/div
5. **✅ Direct Voltage Setting** - Custom scale entry
6. **✅ Mouse Support** - In measurement operations
7. **✅ Extended Slots** - Support for slots 0-9
8. **✅ Universal LF Support** - All GPIB instruments

## 🛠️ **Requirements**

- **Compiler**: OpenWatcom C/C++ 1.9
- **Target**: DOS 16-bit 
- **Hardware**: Personal488 GPIB interface
- **Memory**: Standard DOS memory model

---

**TM5000 v3.1 - Development version for future enhancements** 🚧