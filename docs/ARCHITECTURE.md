# TM5000 Architecture (as-built, v3.7)

Technical description of the TM5000 GPIB control system as it is actually built in
[`src/`](../src). It supersedes the original v3.5 *planning* document. For the build
toolchain see [BUILD.md](BUILD.md); for the cross-version bug history see
[FINDINGS.md](FINDINGS.md); for version history see [../CHANGELOG.md](../CHANGELOG.md).

---

## 1. Overview

TM5000 is a single-process, menu-driven DOS application that controls Tektronix
TM5000-series instruments over GPIB (IEEE-488) and acquires, analyzes, plots, and
prints/export their measurements. It is a modular C rewrite (v3.0+) of the original
monolithic source, split so every translation unit stays within the DOS 64 KB segment
limit.

- **Language/target:** C89, OpenWatcom, 16-bit real-mode MS-DOS, 80286 + optional 80287.
- **Memory model:** large (`-ml`) — code and data pointers are far; data items ≥100 bytes
  go far automatically (`-zt100`), `const` lives in the code segment (`-zc`).
- **Display:** CGA 320×200 4-color.
- **GPIB hardware:** Personal488 (IOtech/CEC) via the vendor `ieeeio` driver shim
  (`gpib.c`, `tm5000.exe`), or the GRIDGPIB resident driver (`gpib_gg.c` +
  `gridgpib.c`, `tm5000g.exe`, v3.7). Both back ends implement `gpib.h`; the fixed
  Driver488 pacing waits are `GPIB_PACE(ms)`, compiled out with `-DGRIDGPIB`.
- **Samples (v3.7):** `double`, with a per-sample measurement-time stamp
  (`sample_t`, BIOS ticks). Buffers come from `alloc_samples()` (up to exactly
  64 KB = 8192 samples, via a DOS segment when `_fmalloc` cannot express it); the
  continuous monitor pools 10×1024 samples among the modules it reads; a full
  buffer merges pairs and keeps averaging (`avg_shift`).

---

## 2. Module map & layering

```
            main.c            program entry, globals, 287 detect, init/cleanup
              │
   ┌──────────┼─────────────────────────────────────────────┐
   │          │                                              │
 io layer   instruments        data/persistence      math            ui/display
 ───────    ───────────        ────────────────      ────            ──────────
 gpib.c     modules.c          data.c                math_functions.c  ui.c
 ieeeio_w.c module_funcs.c     export_enhanced.c     math_enhanced.c   ui_math_menus.c
            (DC5009/DC5010/     config_profiles.c                       graphics.c
             DM5010/DM5120/                                             print.c
             PS5004/PS5010/
             FG5010)
   asm (optimization, currently dormant): cga_asm  mem286  fixed286  trig287_simple
```

| File | Responsibility |
|------|----------------|
| `main.c` / `tm5000.h` | Entry point, all global definitions, 287 detection, the single shared header (system types, instrument config structs, globals as `extern`, constants). |
| `gpib.c/.h` | GPIB transport: generic `gpib_write/read`, SRQ/serial-poll, and DM5120/DM5010-specific I/O wrappers, over `ieeeio_w`. |
| `ieeeio_w.c` / `ieeeio.h` | Vendor Personal488 driver glue (Watcom port of IOtech IEEEIO). Lowest layer. |
| `modules.c/.h` | Per-instrument drivers, `configure_modules`, `single_measurement`, `continuous_monitor`, and `validate_enabled_modules`. Largest unit. |
| `module_funcs.c/.h` | Interactive "advanced configuration" menus / comm-test helpers per instrument. |
| `data.c/.h` | Per-module buffer alloc/store, `.tm5`/`.cfg` text persistence, enhanced-export types. |
| `export_enhanced.c` | Metadata-rich CSV/TSV export and real-time streaming export. |
| `config_profiles.c/.h` | Full-system configuration profiles saved to `PROFILES.DAT`. |
| `math_functions.c/.h` | FFT (pure C), differentiation, integration, smoothing, core statistics. |
| `math_enhanced.c` | Dual-trace ops, statistics, digital filtering, curve fitting (linear/poly/exp), correlation, cross-correlation, phase/delay. |
| `ui.c/.h` | Top-level menus, input helpers, the integration hub (`calculate_statistics`, file/profile/export menus). |
| `ui_math_menus.c` | CGA front-ends for the enhanced-math operations. |
| `graphics.c/.h` | CGA primitives, the waveform/graph engine, unit scaling (`get_units_for_type` + the per-unit helpers), mouse. |
| `print.c/.h` | Text and PostScript reports over LPT1. |

---

## 3. Key data structures (`tm5000.h`)

```c
#pragma pack(1)
typedef struct {                 /* one instrument slot (or computed-result slot) */
    float far *module_data;      /* sample buffer (far) */
    char  description[12];
    float last_reading;
    unsigned int module_data_count;
    unsigned int module_data_size;
    unsigned char module_type;   /* MOD_NONE / MOD_DC5009 ... MOD_FG5010 */
    unsigned char slot_number;
    unsigned char gpib_address;
    unsigned char enabled:1;
    unsigned char is_result:1;   /* computed (FFT/math) trace - see §5 */
    unsigned char reserved:6;
} tm5000_module;
#pragma pack()

measurement_system { tm5000_module modules[10]; int gpib_devices[10]; ... }  /* g_system */
trace_info         { label, data, x_scale, x_offset, unit_type, enabled, ... } /* g_traces[10] */
fft_config         { input_points, output_points, window_type, output_format, flags } /* g_fft_config */
```

The system supports **up to 10 slots**. Per-instrument settings live in parallel
`g_<inst>_config[10]` arrays; `config_profile` captures all of them plus graph/FFT/panel
state for save/restore.

---

## 4. GPIB & instruments

- All bus I/O funnels through `gpib.c` over the Personal488 shim (`ieeeio_w.c`); logical
  slot ↔ GPIB address mapping is `address = slot + base`.
- Seven instrument drivers live in `modules.c`. **DC5009 and DC5010 share one
  implementation**: the 22 byte-identical `dc5010_*` routines forward to their `dc5009_*`
  counterparts; only the DC5010-unique features (rise/fall time, A±B totalize, burst) are
  separate.
- DM5120 buffered acquisition defaults to an internal (`TALK,CONT`) trigger so the buffer
  self-fills without external trigger hardware; `EXT` is opt-in per the advanced config.

---

## 5. Acquisition & the computed-result trace mechanism

A measurement slot normally maps to a real instrument. **Computed results** (FFT,
differentiation, integration, smoothing, dual-trace math) are also stored in a module
slot, but with `module_type == MOD_NONE`, `gpib_address == 0`, and `is_result == 1`.

`module_is_result(slot)` recognizes such a slot **either** by the `is_result` flag **or**
derived from state (`MOD_NONE` with allocated data) — the derived test lets computed
traces survive a `.tm5`/`.cfg` reload, which does not serialize the flag. This is used by:

- `validate_enabled_modules()` — never purges a computed-result slot as a "phantom".
- `continuous_monitor()` — never counts, clears, or GPIB-reads a computed-result slot.
- `sync_traces_with_modules()` — preserves the computed trace's `unit_type`/`x_scale`/
  `x_offset` instead of reclassifying it by `module_type`.

This is the v3.6 fix for the long-standing "computed traces get wiped" regression (see
FINDINGS #1).

---

## 6. Math subsystem

- **FFT** (`math_functions.c`): pure-C, power-of-2, 64–1024 points, with
  Rectangular/Hamming/Hanning/Blackman windows. Uses the 80287 when present; falls back to
  a coarse software DFT otherwise (approximate, ≤64-point — a coprocessor is recommended).
  Output formats: dB (threshold `1e-12`, floor `-240 dB`), linear, power. When the requested
  output size is smaller than N/2 the spectrum is **peak-preserving decimated** across the
  full 0..Nyquist span with a correspondingly scaled frequency axis.
- **Enhanced analysis** (`math_enhanced.c`): dual-trace operations; basic/rolling
  statistics; digital filtering; **curve fitting** — linear, polynomial (order 2–3, with X
  centered+scaled for numerical conditioning), and exponential; Pearson **correlation**;
  **cross-correlation** (per-lag overlap-normalized, double-precision accumulators) and
  **phase/delay** analysis (peak-alignment lag over the reliable lag range).
- Result traces are written into a computed-result slot (§5) and displayed like any trace.

---

## 7. Persistence & export

- **`.tm5` measurement files / `.cfg` configuration files**: line-oriented text. A file has
  a `GlobalData:` block, a `ModuleData:` marker, then `Slot<n>:<count>` records with samples,
  ending at `EndOfFile`. Loading resynchronizes to the `ModuleData:` marker (tolerant of a
  global-count mismatch) and warns if the marker is absent.
- **Configuration profiles** (`config_profiles.c`): the full multi-instrument + graph/FFT/
  panel state to `PROFILES.DAT`, checksum-validated.
- **Enhanced export** (`export_enhanced.c`): CSV/TSV with metadata/timestamps/settings and
  optional real-time streaming.

---

## 8. Build & code size

OpenWatcom, large model, 286 target — see [BUILD.md](BUILD.md) for the exact flags. Two
size-relevant build options are enabled:

- `wcc -zm` — emit each function in its own segment, so that
- `wlink OPTION ELIMINATE` can **drop every unreferenced function** (and the dormant
  assembly modules) from the image. OpenWatcom's linker has no identical-code *folding*, so
  duplicate functions are still consolidated in source (e.g. the DC5009/DC5010 forwarders).

Result: `tm5000.exe` ≈ **270 KB** (dead-stripped), within the 640 KB conventional-memory
budget with room for the 1024-sample × 10-module far buffers.

---

## 9. Assembly modules (dormant)

`cga_asm.asm`, `mem286.asm`, `fixed286.asm`, and `trig287_simple.asm` contain CGA/286/287
optimization routines that are **not currently called from C** (only `extern`-declared).
With `OPTION ELIMINATE` they are stripped from the image, so they cost nothing today; they
remain in the tree as scaffolding to wire in (or remove) later.

---

## 10. Known design notes / limitations

- The non-287 software-DFT FFT path is approximate and low-resolution; accurate FFTs need a
  coprocessor.
- `tm5000.h` is a "god header" (all subsystem prototypes); header/API hygiene cleanups are
  tracked for a future pass.
- See [FINDINGS.md](FINDINGS.md) for the code-verified cross-version regression analysis and
  [../CHANGELOG.md](../CHANGELOG.md) for what changed in each release.

---

*As-built for v3.6. Update this document when the module structure, data layout, or build
options change.*
