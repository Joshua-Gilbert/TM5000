# TM5000 Architecture (as-built, v3.8)

How [`src/`](../src) is built and organized. Toolchain: [BUILD.md](BUILD.md);
cross-version bug history: [FINDINGS.md](FINDINGS.md); releases:
[../CHANGELOG.md](../CHANGELOG.md).

---

## 1. Overview

TM5000 is a single-process, menu-driven DOS application that controls Tektronix
TM5000-series instruments over GPIB (IEEE-488) and acquires, analyzes, plots, prints,
and exports their measurements. It is a modular C rewrite (v3.0+) of the original
monolithic source, split so every translation unit stays within the DOS 64 KB segment
limit.

- **Language/target:** C89, OpenWatcom, 16-bit real-mode MS-DOS, 80286 + optional 80287.
- **Memory model:** large (`-ml`) — code and data pointers are far; data items ≥100 bytes
  go far automatically (`-zt100`), `const` lives in the code segment (`-zc`).
- **Display:** CGA 320×200 4-color.
- **GPIB hardware:** Personal488 (IOtech/CEC), driven either through the vendor
  `ieeeio` shim and Driver488 (`gpib.c`, `tm5000.exe`) or through the GRIDGPIB
  resident driver (`gpib_gg.c` + `gridgpib.c`, `tm5000g.exe`). Both back ends
  implement `gpib.h`. The fixed waits Driver488 needs after bus calls are
  `GPIB_PACE(ms)`, compiled out with `-DGRIDGPIB` because GRIDGPIB calls return only
  when the transaction is complete.
- **Samples:** `double`, each with a measurement-time stamp (`sample_t`, BIOS
  ticks). Buffers come from `alloc_samples()`: up to exactly 64 KB = 8192 samples,
  taken as a DOS segment when `_fmalloc` cannot express that size. The continuous
  monitor pools 10×1024 samples among the modules it reads; a full buffer merges
  pairs and keeps averaging (`avg_shift`).

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
  or        (DC5009/DC5010/     config_profiles.c                       graphics.c
 gpib_gg.c   DM5010/DM5120/                                             print.c
 gridgpib.c  PS5004/PS5010/
             FG5010)
   asm (optimization, currently dormant): cga_asm  mem286  fixed286  trig287_simple
```

| File | Responsibility |
|------|----------------|
| `main.c` / `tm5000.h` | Entry point, all global definitions, 287 detection, the single shared header (system types, instrument config structs, globals as `extern`, constants). |
| `gpib.c/.h` | GPIB transport: generic `gpib_write/read`, SRQ/serial-poll, and DM5120/DM5010-specific I/O wrappers, over `ieeeio_w`. |
| `ieeeio_w.c` / `ieeeio.h` | Vendor Personal488 driver glue (Watcom port of IOtech IEEEIO). Lowest layer. |
| `gpib_gg.c` | `tm5000g.exe` only: the `gpib.h` API on GRIDGPIB, per-instrument read timeouts, and a small Driver488 text-command interpreter behind `ieee_write`/`ieee_read` for the terminal and diagnostics. |
| `gridgpib.c/.h` | C interface to the GRIDGPIB resident driver (INT 60h–66h, found by its signature). |
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
    double far *module_data;     /* sample buffer (far) */
    char  description[12];
    double last_reading;
    unsigned int module_data_count;
    unsigned int module_data_size;
    unsigned char module_type;   /* MOD_NONE / MOD_DC5009 ... MOD_FG5010 */
    unsigned char slot_number;
    unsigned char gpib_address;
    unsigned char enabled:1;
    unsigned char is_result:1;   /* computed (FFT/math) trace - see §5 */
    unsigned char time_ok:1;     /* every stored sample has a time stamp */
    unsigned char reserved:5;
    unsigned long far *sample_t; /* BIOS-tick time of each sample */
    ...                          /* averaging state (acc_*, avg_shift), FFT source duration */
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

- All bus I/O funnels through the `gpib.h` API: `gpib.c` over the Personal488 shim
  (`ieeeio_w.c`) in `tm5000.exe`, `gpib_gg.c` over GRIDGPIB in `tm5000g.exe`. Each
  slot's GPIB address (1–30) is set by the user in the slot-setup menu; addresses must be
  unique. Auto-detect (Configure Modules → A) scans addresses 1–30 with `gpib_probe()`
  and fills free slots only.
- Seven instrument drivers live in `modules.c` (the DM5120, PS5004 and PS5010 setup
  menus in `module_funcs.c`). All the instruments use Tek Codes & Formats: commands are
  `HEADER arg;`, replies carry their header and are parsed after it, and each driver's
  strings were checked against its manual (sources in
  [changelogs/CHANGELOG_v3.8.md](changelogs/CHANGELOG_v3.8.md)). Outputs change only on
  an explicit menu action with a Y/N prompt.
- **DC5009 and DC5010 share one table-driven implementation** (`dc50xx_*`):
  `dc5010_config` is a typedef of `dc5009_config`, and the slot's model selects the
  function list and the model-only commands (DC5010 `FIL`/`TER`/`NULL`/`RISE`/`FALL`;
  DC5009 `SOUR`/`FPTR`). Readings carry the function's unit (`UNIT_TIME`,
  `UNIT_COUNT` added for them).
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

See FINDINGS #1 for the regression this prevents.

---

## 6. Math subsystem

- **FFT** (`math_functions.c`): C radix-2 FFT, power-of-2, 64–8192 points, with
  Rectangular/Hamming/Hanning/Blackman windows. Runs on the 80287 when present; otherwise
  falls back to a coarse software DFT (approximate, ≤64 points).
  Output formats: dB (threshold `1e-12`, floor `-240 dB`), linear, power. When the requested
  output size is smaller than N/2 the spectrum is **peak-preserving decimated** across the
  full 0..Nyquist span with a correspondingly scaled frequency axis.
- **Analysis** (`math_enhanced.c`): dual-trace operations; basic/rolling
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
- **Export** (`export_enhanced.c`): CSV/TSV with metadata/timestamps/settings and
  optional real-time streaming.

---

## 8. Build & code size

OpenWatcom, large model, 286 target; exact flags in [BUILD.md](BUILD.md). Two options
control size:

- `wcc -zm` emits each function in its own segment, so that
- `wlink OPTION ELIMINATE` can drop every unreferenced function from the image. The linker
  has no identical-code folding, so duplicate functions are consolidated in source instead
  (e.g. the shared DC5009/DC5010 driver).

`tm5000.exe` is 382,760 bytes and `tm5000g.exe` 383,050 bytes (~374 KB each), leaving
room in 640 KB for the pooled 10×1024-sample far buffers (8 bytes per sample plus a
4-byte time stamp). `tm5000g.exe` swaps `gpib.c` for `gpib_gg.c` + `gridgpib.c` and
builds `modules.c` / `module_funcs.c` with `-DGRIDGPIB`.

---

## 9. Assembly modules (dormant)

`cga_asm.asm`, `mem286.asm`, `fixed286.asm`, and `trig287_simple.asm` contain CGA/286/287
optimization routines that are **not called from C** (only `extern`-declared). With
`OPTION ELIMINATE` they are stripped from the image; they stay in the tree as
scaffolding to wire in or remove.

They are not safe to call as written:

- All four are `.MODEL SMALL` with near procedures reading stack arguments at `[bp+4]`,
  while the C side is large-model with Watcom's register calling convention.
- `cga_asm.asm`: the `PUBLIC` names (`cga_draw_hline_`, …) don't match the procedure names
  (`_cga_draw_hline`, …). `cga_draw_hline` exits its leading-pixel loop with `cx` still
  pushed. `cga_clear_screen` writes 32 KB, past CGA's 16 KB.
- `mem286.asm`: in `fast_fmemcpy_286`, `cmp cx,1000h` clobbers the carry from `shr`, so
  every aligned copy of an even count moves one extra byte. `fast_memcmp_286` compares
  the later byte of a mismatched word first, so its sign can disagree with `memcmp`.
- `fixed286.asm`: `fixed_mul_286`, `fixed_div_286`, `fixed_cos_286` and `fixed_sqrt_286`
  have wrong arithmetic or stack handling, and `FIXED_PI`, `FIXED_2PI` and `FIXED_PI2`
  are wrong.

---

## 10. Known limitations

- The non-287 software-DFT FFT path is approximate and low-resolution; accurate FFTs need a
  coprocessor.
- `tm5000.h` is a "god header" (all subsystem prototypes); header/API cleanup
  (de-declaring, `MAX_MODULES`, `static`-ization) is left for a future pass.

---

*Describes v3.8. Update when the module structure, data layout, or build options change.*
