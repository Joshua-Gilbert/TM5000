# TM5000 GPIB Control System

## Professional Instrument Control for DOS Systems

![Version](https://img.shields.io/badge/Version-3.5-brightgreen) ![Platform](https://img.shields.io/badge/Platform-DOS%2016--bit-blue) ![Architecture](https://img.shields.io/badge/Architecture-Modular-orange) ![License](https://img.shields.io/badge/License-Research-lightgrey)

<img width="1344" height="1691" alt="Gridcase 1520 with Tektronix TM5006A GPIB measurement system — FFT analysis of precision voltage reference" src="https://github.com/user-attachments/assets/7f70d8ae-4ebc-4a04-a70c-bf06f67a440f" />

**Figure:** Gridcase 1520 running a Tektronix TM5006A modular measurement system via GPIB control — FFT spectral analysis of a precision voltage reference standard.

---

### 🎯 Overview

The TM5000 GPIB Control System is an instrument-control and data-acquisition
platform for DOS systems. Originally developed for the Gridcase 1520 portable
computer, it provides control of Tektronix TM5000-series instruments over a
GPIB (IEEE-488) interface, using the Personal488 (IOtech/CEC) driver.

This repository preserves the **entire history** of the project — from the
earliest binaries through the current modular source — which makes it a useful
study in how a constrained 16-bit DOS application evolved. The current,
buildable source is in [`src/`](src); every prior version is kept under
[`archive/`](archive).

### 📁 Repository layout

```
TM5000/
├── src/                 Current canonical source (v3.5) — build from here
├── archive/             Frozen historical versions (reference only)
│   ├── v1-binaries/     V1.8 / V1.9 executables (no source survives)
│   ├── v2-source/       V2.0–V2.9 monolithic single-file C
│   └── v3.0/ … v3.4/    Earlier modular releases (each self-contained)
├── docs/
│   ├── BUILD.md         Toolchain + build/run instructions
│   ├── ARCHITECTURE.md  v3.5 architecture notes
│   ├── FINDINGS.md      Cross-version regression analysis (what worked vs broke)
│   └── changelogs/      Detailed v3.5 changelog + release notes
├── CHANGELOG.md         Consolidated version history
├── CONTRIBUTING.md
├── README.md
└── LICENSE
```

### 🔧 System Capabilities

#### Instrument Support
- **DM5010** — 4½-digit multimeter with high-speed acquisition
- **DM5120** — 6½-digit precision multimeter with advanced math functions
- **PS5004** — single-channel precision power supply (0–20 V, 300 mA)
- **PS5010** — dual-channel power supply with logic supply (0–32 V, 0.75 A)
- **DC5009** — universal counter/timer with advanced triggering
- **DC5010** — enhanced counter with rise/fall time measurement
- **FG5010** — function generator

#### Data Acquisition
- Up to **10 instrument slots** simultaneously (single GPIB bus)
- **1024 samples** per module (v3.4+)
- Sample intervals from 100 ms to 10 s, plus custom rates
- Continuous real-time monitoring with live display
- Dynamic buffer management within the 640 KB DOS budget

#### Mathematical Analysis
- **FFT** (power-of-2, 64–1024 points) with Rectangular/Hamming/Hanning/Blackman windows
- Statistics: min, max, mean, standard deviation, RMS
- Waveform math: differentiation, integration, smoothing/filtering, scaling, offset,
  dual-trace operations, curve fitting, correlation, linear regression
- Automatic peak detection
- Optional 80287 coprocessor acceleration (pure-C FFT path; see *Known issues*)

#### Display & Visualization
- **CGA graphics** (320×200, 4-color) with engineering grid
- Up to 10 simultaneous color-coded traces
- Auto-scaling with manual override; zoom/pan; interactive cursor measurements
- Automatic V/mV/µV unit scaling

#### Data Management
- Native `.tm5` measurement files and `.cfg` configuration files
- Export to CSV / text / PostScript (enhanced CSV with metadata in v3.5)
- Configuration profiles — save/restore complete system setups (v3.5)

#### Printing
- PostScript output and Brother dot-matrix support over LPT1
- Custom headers, automatic legends, IEEE scientific-notation formatting

### 🏗️ Architecture

The v3.x series is a modular rewrite of the original monolithic v2.9 (which had
outgrown the DOS 64 KB segment limit). The current source is ~21,650 lines
across the files below. Full detail: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

| Area | Files |
|------|-------|
| **Core** | `main.c`, `tm5000.h` (the single shared header) |
| **GPIB / driver** | `gpib.c/.h`, `ieeeio_w.c`, `ieeeio.h` (Personal488 glue) |
| **Instruments** | `modules.c/.h`, `module_funcs.c/.h` |
| **Data & export** | `data.c/.h`, `export_enhanced.c`, `config_profiles.c/.h` |
| **Math** | `math_functions.c/.h`, `math_enhanced.c` |
| **UI / display / print** | `ui.c/.h`, `ui_math_menus.c`, `graphics.c/.h`, `print.c/.h` |
| **Assembly (286/287/CGA)** | `cga_asm.asm`, `mem286.asm`, `fixed286.asm`, `trig287_simple.asm` |

> The four assembly modules are assembled and linked but **not currently invoked**
> from C (only `extern` declarations exist) — treat them as optimization
> scaffolding. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
> [docs/FINDINGS.md](docs/FINDINGS.md).

#### Version evolution

| Version | Release | Key change |
|---------|---------|-----------|
| **v3.0** | June 2025 | Modular architecture; DOS segment compliance |
| **v3.1** | June 2025 | Full instrument support; 287 optimizations |
| **v3.2** | June 2025 | Stability; FFT printing; configuration persistence |
| **v3.3** | June 2025 | File-I/O reliability; symbol-conflict resolution |
| **v3.4** | July 2025 | 1024-sample buffers; ~22% memory reduction |
| **v3.5** | July 2025 | Configuration profiles; enhanced export; enhanced math |

### 🔨 Requirements

**To run:** IBM PC-compatible 80286+ · 640 KB RAM (1 MB recommended) · CGA graphics ·
MS-DOS 3.3+ · a Personal488 (IOtech/CEC) GPIB interface with its DOS driver
(`DRVR488.EXE`). An 80287 coprocessor and a Microsoft-compatible mouse are
recommended. The original target was the **Gridcase 1520** portable.

**To build:** OpenWatcom C/C++ 1.9 (16-bit DOS target) plus its assembler.
See [docs/BUILD.md](docs/BUILD.md).

### 💾 Quick start

```sh
# Build (from the source tree)
cd src
wmake                     # produces tm5000.exe (~276 KB)

# Run on DOS / DOSBox
#   1. load the Personal488 driver (DRVR488.EXE) if using GPIB hardware
#   2. TM5000.EXE
```

Basic flow inside the program:

```
Main Menu → Configure Modules → [select instrument] → [set parameters]
          → Measurement → [continuous / single] → [start acquisition]
          → File / Export → [save .tm5 / .cfg / CSV / PostScript]
```

### ⚠️ Known issues

v3.5 ships with several known limitations. Because every prior version is
preserved, most have been traced to a specific cause — and in several cases to a
version where the behavior *worked*. The full, code-referenced analysis is in
**[docs/FINDINGS.md](docs/FINDINGS.md)**. Highlights:

- **Some documented FFT/GPIB "fixes" were never actually applied** to the shipped
  code (verify changelog claims against source).
- Computed FFT/math result traces can be wiped on entering continuous monitoring.
- DM5120 buffer fills can time out (external-trigger default vs. v2.9's internal trigger).
- Enhanced Statistics mislabels non-voltage traces (dB/Hz/Ω/A) as volts.
- `.tm5` import can corrupt on count mismatch (parser stream desync).

### 🔬 Educational use

The TM5000 system is a useful platform for studying DOS/16-bit software
architecture, GPIB/IEEE-488 instrument control, real-time systems under tight
timing, and programming within hard memory constraints.

### 📄 License

Provided for research and educational purposes. See [LICENSE](LICENSE).

**Copyright © 2025 — For Educational and Research Use**
