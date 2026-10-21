# TM5000 GPIB Control System

![Version](https://img.shields.io/badge/Version-3.6-brightgreen) ![Platform](https://img.shields.io/badge/Platform-DOS%2016--bit-blue) ![License](https://img.shields.io/badge/License-Research-lightgrey)

<img width="1344" height="1691" alt="Gridcase 1520 with Tektronix TM5006A GPIB measurement system — FFT analysis of precision voltage reference" src="https://github.com/user-attachments/assets/7f70d8ae-4ebc-4a04-a70c-bf06f67a440f" />

**Figure:** Gridcase 1520 running a Tektronix TM5006A mainframe over GPIB — FFT of a precision voltage reference.

TM5000 is a 16-bit MS-DOS program that controls Tektronix TM5000-series
instruments over GPIB (IEEE-488) through a Personal488 (IOtech/CEC) interface,
and acquires, analyzes, plots, prints, and exports their measurements. It was
written for the Gridcase 1520 portable.

The repository keeps every version, from the earliest binaries to the current
modular source. The buildable source is in [`src/`](src); prior versions are
frozen under [`archive/`](archive).

## Repository layout

```
TM5000/
├── src/                 Current canonical source (v3.6) — build from here
├── archive/             Frozen historical versions (reference only)
│   ├── v1-binaries/     V1.8 / V1.9 executables (no source survives)
│   ├── v2-source/       V2.0–V2.9 monolithic single-file C
│   └── v3.0/ … v3.5/    Earlier modular releases (each self-contained)
├── docs/
│   ├── BUILD.md         Toolchain + build/run instructions
│   ├── ARCHITECTURE.md  Architecture notes (v3.6)
│   ├── FINDINGS.md      Cross-version regression analysis (what worked vs broke)
│   └── changelogs/      Per-version changelogs (v3.5, v3.6) + release notes
├── CHANGELOG.md         Consolidated version history
├── CONTRIBUTING.md
├── README.md
└── LICENSE
```

## Features

### Instruments
- **DM5010** — 4½-digit multimeter
- **DM5120** — 6½-digit multimeter with built-in math and reading buffer
- **PS5004** — single-channel power supply (0–20 V, 300 mA)
- **PS5010** — dual-channel power supply with logic supply (0–32 V, 0.75 A)
- **DC5009** — universal counter/timer
- **DC5010** — counter/timer with rise/fall time measurement
- **FG5010** — function generator

### Acquisition
- Up to **10 instrument slots** on one GPIB bus
- **1024 samples** per module
- Sample intervals from 100 ms to 10 s, plus custom rates
- Continuous monitoring with live display
- Buffers allocated dynamically within the 640 KB DOS budget

### Analysis
- **FFT** (power-of-2, 64–1024 points) with Rectangular/Hamming/Hanning/Blackman windows
- Statistics: min, max, mean, standard deviation, RMS
- Waveform math: differentiation, integration, smoothing/filtering, scaling, offset,
  dual-trace operations, curve fitting (linear/polynomial/exponential), correlation,
  cross-correlation, phase/delay
- Automatic peak detection
- The FFT runs in C on the 80287 when present; without one it falls back to a coarse
  ≤64-point DFT

### Display
- **CGA graphics** (320×200, 4-color) with engineering grid
- Up to 10 simultaneous color-coded traces
- Auto-scaling with manual override; zoom/pan; cursor measurements
- Automatic V/mV/µV unit scaling

### Files and export
- Native `.tm5` measurement files and `.cfg` configuration files
- Export to CSV (with metadata) / text / PostScript
- Configuration profiles — save/restore complete system setups

### Printing
- PostScript output and Brother dot-matrix support over LPT1
- Custom headers, automatic legends, IEEE scientific-notation formatting

## Architecture

v3.x is a modular rewrite of the monolithic v2.9, which had outgrown the DOS
64 KB segment limit. Details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

| Area | Files |
|------|-------|
| **Core** | `main.c`, `tm5000.h` (the single shared header) |
| **GPIB / driver** | `gpib.c/.h`, `ieeeio_w.c`, `ieeeio.h` (Personal488 glue) |
| **Instruments** | `modules.c/.h`, `module_funcs.c/.h` |
| **Data & export** | `data.c/.h`, `export_enhanced.c`, `config_profiles.c/.h` |
| **Math** | `math_functions.c/.h`, `math_enhanced.c` |
| **UI / display / print** | `ui.c/.h`, `ui_math_menus.c`, `graphics.c/.h`, `print.c/.h` |
| **Assembly (286/287/CGA)** | `cga_asm.asm`, `mem286.asm`, `fixed286.asm`, `trig287_simple.asm` |

The four assembly modules are linked but nothing in C calls them (only `extern`
declarations exist); they are optimization scaffolding. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Versions

| Version | Release | Key change |
|---------|---------|-----------|
| **v3.0** | June 2025 | Modular rewrite; every unit within the 64 KB segment limit |
| **v3.1** | June 2025 | Full instrument support; 287 optimizations |
| **v3.2** | June 2025 | FFT printing; configuration persistence |
| **v3.3** | June 2025 | File-I/O fixes; duplicate-symbol cleanup |
| **v3.4** | July 2025 | 1024-sample buffers; ~22% memory reduction |
| **v3.5** | July 2025 | Configuration profiles; metadata export; dual-trace math |
| **v3.6** | June 2026 | Regression fixes; polynomial/exponential fit, cross-correlation, phase/delay; linker dead-strip |

## Requirements

**To run:** IBM PC-compatible 80286+ · 640 KB RAM (1 MB recommended) · CGA graphics ·
MS-DOS 3.3+ · a Personal488 (IOtech/CEC) GPIB interface with its DOS driver
(`DRVR488.EXE`). An 80287 coprocessor and a Microsoft-compatible mouse are
recommended.

**To build:** OpenWatcom C/C++ (16-bit DOS target) plus its assembler.
See [docs/BUILD.md](docs/BUILD.md).

## Quick start

```sh
# Build (from the source tree)
cd src
wmake                     # tm5000.exe (~276 KB); a prebuilt copy is committed
# WATCOM_BIN defaults to a WSL path; elsewhere: wmake WATCOM_BIN=C:/WATCOM/binnt

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

## Status

v3.6 fixes the six v3.5 issues catalogued in [docs/FINDINGS.md](docs/FINDINGS.md)
(computed traces wiped by continuous monitoring, FFT dB clipping, DM5120 buffer
timeouts, Enhanced Statistics units, `.tm5` import desync, FFT truncation), each traced
against the archived versions. What changed: [CHANGELOG.md](CHANGELOG.md).

## License

Provided for research and educational purposes. Copyright © 2025. See [LICENSE](LICENSE).
