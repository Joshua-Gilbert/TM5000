# Changelog

All notable changes to the TM5000 GPIB Control System. Detailed per-version
changelogs are linked under each release.

The project has three eras:
- **V1** — binaries only (no source survives)
- **V2** — a single monolithic C source file
- **V3** — the modular rewrite (current)

> **Accuracy note:** several fixes recorded as done in the original v3.2/v3.5
> changelogs were not in the shipped code (see [docs/FINDINGS.md](docs/FINDINGS.md)).
> v3.6 applies the v3.5 FFT dB threshold/floor; the v3.2 GPIB abort/reset cleanup is
> still absent.

---

## [Unreleased] — Audit fixes

Built clean with OpenWatcom 2.0 (no new warnings); not yet run on hardware.

### Fixed — memory safety
- **Printing:** the text graph's row clear wrote 20 bytes past its line buffers on
  every row, legend lines overran `label[40]`, and a custom header over 63 characters
  overran `g_custom_header_text`. Headers are now cut to 63.
- **GPIB:** a custom command too long for the 128-byte bus buffer is refused with an
  error instead of overflowing it. A long driver status is cut to fit `g_error_msg`.
- **Buffers:** `allocate_module_buffer` allocates before freeing, so a failed
  allocation leaves the old data intact. FFT, derivative, integral, smoothing and
  dual-trace results grow a too-small target buffer instead of writing past it.
- `ieeescnf` read 256 bytes into a 128-byte buffer.

### Fixed — files and profiles
- **`.tm5` load** lost every other module's config, the global data and the module
  data: each config reader consumed the next header line. Skipped config blocks now
  print a warning. The file format is unchanged.
- **`.cfg` load** never applied `[GraphScale]`, and FG5010 slots got no default config.
- **Profiles:** the checksum covered its own low byte, so about 255 of 256 saved
  profiles failed verification and vanished from the list. Existing `PROFILES.DAT`
  files verify again with no version bump. After a delete, load, delete and overwrite
  acted on the wrong profile. The menu never opened `PROFILES.DAT` at all, because
  `init_profile_system` had no caller. It now tells I/O, not-a-profile-file and
  version-mismatch errors apart.
- Export file sizes of 32 KB and up were truncated to 16 bits. In scientific
  notation, zero now follows the precision setting.

### Fixed — math
- **Digital filters** ran the wrong recurrence (feedback read from `a0`, and the
  highpass `a1` was wrong). *Order* now cascades that many sections.
- The **no-287 FFT** used small-angle cos/sin approximations over the whole circle,
  and windowed a longer record than it transformed.
- Derivative, integral and smoothing gave wrong results when overwriting their own
  source slot. The integral trace now sets up its axes like the derivative.
- Removed `calculate_median`, `calculate_mode` and `calculate_frequency_analysis`: they
  were never called, and they returned scaled values in a 16-bit int.

### Fixed — instruments and UI
- **FG5010 communication test** reprograms frequency, amplitude and waveform and
  briefly enables the output. It now asks first, and always switches the output off
  again, even if the read-back times out. It also runs from the slot-setup
  "Test communication?" prompt.
- **DM5120:** the buffered fill sent `STOINT` as readings/s × 1000 (10 r/s sent
  `STOINT 10000`). The conflict check now tests the configured interval and refuses
  an invalid slot.
- **DM5120 bus handling** keyed off `address − 10` for addresses 10–17 only.
  Addresses are user-assigned, so the SRQ poll and longer settle now apply to every
  enabled DM5120, which adds about 200 ms per command to a DM5120 at any other
  address.
- The DC5010 communication test ran the DC5009 test. The DC5009/DC5010 query
  functions now fill the caller's buffer.
- The FPU probe began with a waiting `fclex`, which can hang a machine with no 287.
- "Select all" left slots 8–9 unselected. ALT-toggle could index `g_traces[-1]`.
  Graph status text had lowercase letters, which the CGA font draws as spaces.
  File-menu mouse rows were off by two.
- Custom GPIB commands starting with `F` (e.g. `FUNCT DCV`) triggered a read and a
  SEQUENCE error. Only queries (`?`) read now.

### Build
- Makefile dependencies list each file's direct includes, and `clean` also removes
  `.err` and `.map`.

---

## [3.6] — 2026-06-04 — Regression fixes, math, dead-strip

Detailed changelog: [docs/changelogs/CHANGELOG_v3.6.md](docs/changelogs/CHANGELOG_v3.6.md) ·
Root-cause analysis: [docs/FINDINGS.md](docs/FINDINGS.md)

Fixes every issue in `docs/FINDINGS.md`, wires up the stubbed curve-fit and correlation
math, and turns on linker dead-code elimination. Built with OpenWatcom 2.0.

### Fixed
- **Computed FFT/math traces survive continuous monitoring.** An `is_result` flag
  exempts them from phantom-module cleanup; this also removes the "ghost modules" count.
  *FINDINGS #1*
- **FFT dB floor lowered** to the documented-but-never-applied values: threshold
  `1e-8→1e-12`, floor `-160→-240 dB`. *FINDINGS #2*
- **DM5120 buffer fill no longer waits on an external trigger.** Defaults to
  `TALK,CONT`; `EXT` is opt-in. *FINDINGS #3*
- **Enhanced Statistics** labels dB/Hz/Ω/A/power traces by their unit instead of volts.
  *FINDINGS #4*
- **`.tm5` import** resyncs to the `ModuleData:` marker, tolerating a global
  sample-count mismatch, and warns if the marker is missing. *FINDINGS #5*
- **FFT spectrum is decimated, not truncated,** when `output_points < N/2`
  (peak-preserving, with a matching frequency axis). *FINDINGS #6*
- **Computed traces persist across `.tm5`/`.cfg` reload.** `module_is_result()` also
  derives "computed" from state (`MOD_NONE` + data), since the flag isn't serialized;
  the all-slots-full FFT/derivative/integral/smoothed overwrite branches mark the
  result; `sync_traces_with_modules` preserves units for every computed trace, not
  just `"FFT Result"`.
- **Non-287 FFT** applies the output format, fixes the frequency axis, and drops the
  garbage tail (with an "approximate" notice) instead of storing mislabeled linear data.

### Added
- **Polynomial and exponential curve fitting.** The `curve_fitting_menu` options that
  read "coming in v3.6" call `fit_polynomial` (order 2–3) and `fit_exponential`.
  `fit_polynomial` centres and scales X; raw sample indices up to ~1023 made the cubic
  normal equations ill-conditioned in single precision.
- **Cross-correlation and phase/delay analysis.** Implemented the declared-only
  `calculate_cross_correlation` and `calculate_phase_shift` and wired them into
  `correlation_analysis_menu` (peak-alignment lag, zero-lag correlation, sample-rate-scaled
  time delay). Each lag is normalized by its overlap energy so the peak isn't biased toward
  lag 0; accumulators are `double`; the search is bounded to the reliable lag range; a
  progress note prints during the scan.

### Changed (no runtime behavior change)
- **Linker dead-code elimination:** `wcc -zm` (one segment per function) +
  `wlink OPTION ELIMINATE` strip unreferenced functions and the dormant assembly modules.
  `wlink` has no identical-code folding (cf. MSVC `/OPT:ICF`), so duplicates are merged
  in source.
- **DC5009/DC5010:** the 22 byte-identical `dc5010_*` functions forward to their
  `dc5009_*` twins; DC5010-only functions are unchanged.
- One `ps_unit_of()` helper replaces the two PostScript-unit ladders in `print.c`; one
  `get_units_for_type()` replaces the copies in `ui.c`/`print.c`.
- Removed **15 zero-caller functions** and their prototypes: graphics primitives
  (`draw_circle`, `fill_circle`, `draw_rectangle`, `set_cga_palette`, `wait_vretrace`,
  `draw_filled_rect`, `draw_readout`, `mouse_in_region`, `set_mouse_pos`), unreachable
  stub menus (`data_analysis_menu`, `unit_conversion_menu`, `calculator_menu`), and
  superseded/demo readers (`read_dm5120`, `read_ps5010`, `dm5120_buffer_example`).
- **Binary size:** 283,388 bytes (the regression-fix build, before cleanup and
  dead-strip) → **275,948 bytes** (−7,440, −2.6%) with the new features included.
  Removing whole unused functions moved the size; merging small duplicates was
  roughly neutral.

### Repository and tooling
- Standard layout: source under [`src/`](src), history under [`archive/`](archive),
  docs under [`docs/`](docs); v3.5 promoted to `src/` and frozen at
  [`archive/v3.5`](archive/v3.5).
- Vendored `ieeeio.h` into `src/` so the build is self-contained.
- Added root `.gitignore`, `CONTRIBUTING.md`, `docs/BUILD.md`, this changelog, and
  `docs/FINDINGS.md`; rewrote the root README.
- `src/makefile`: overridable `WATCOM_BIN`; removed the rule and `help` entries for the
  nonexistent `trig287.asm`/`fft_286.asm`; flagged the incomplete `wcl` target.

---

## [3.5] — July 2025 — Data management

Detailed changelog: [docs/changelogs/CHANGELOG_v3.5.md](docs/changelogs/CHANGELOG_v3.5.md) ·
Release notes: [docs/changelogs/v3.5-release-notes.md](docs/changelogs/v3.5-release-notes.md)

### Added
- Configuration profiles — save/load complete system configurations
  (`config_profiles.c`, persisted to `PROFILES.DAT`).
- Data export — CSV/TSV with metadata, timestamps, custom formatting, and
  real-time streaming export (`export_enhanced.c`).
- `math_enhanced.c`: dual-trace operations, rolling statistics, digital
  filtering, curve fitting, correlation, with descriptive trace legends.
- Direct-memory-access CGA assembly; 286/287 assembly modules.

### Changed
- FFT moved to a pure-C implementation to eliminate 287-coprocessor hangs.

### Known issues (root-caused in [docs/FINDINGS.md](docs/FINDINGS.md))
- FFT low-frequency display; `.tm5` import corruption; ghost modules counted in
  continuous monitoring; DM5120 buffer timeouts; Enhanced Stats unit detection.
- The documented FFT magnitude-normalization fix (`/N`, `1e-12`, `-240`) was
  **never applied**; the code still uses `/(N/2)`, `1e-8`, `-160`, a regression from
  v3.3's `1e-10`/`-200`.

---

## [3.4] — July 2025 — Buffers and memory

(No standalone changelog was written for v3.4; see
[archive/v3.4/TM5000_v3.4_Structure.md](archive/v3.4/TM5000_v3.4_Structure.md).)

### Changed
- 1024-sample per-module buffers (up from earlier limits).
- ~22% memory reduction from buffer changes, for the 640 KB DOS budget.

---

## [3.3] — June 2025 — Bug fixes

Detailed changelog: [archive/v3.3/CHANGELOG_v3.3.md](archive/v3.3/CHANGELOG_v3.3.md)

### Fixed
- Stack overflow / memory corruption in module selection — removed 7 duplicate
  function definitions that caused linker symbol conflicts.
- Ghost/phantom modules in continuous monitoring — added module type / GPIB address /
  description validation. *(This validation later wiped computed math result traces;
  see FINDINGS #1.)*
- Configuration/measurement save-load failures — removed `fseek(..., SEEK_CUR)` calls,
  which fail under DOS buffered I/O.

---

## [3.2] — June 2025 — Stability

Detailed changelog: [archive/v3.2/CHANGELOG_v3.2.md](archive/v3.2/CHANGELOG_v3.2.md)

### Added
- FFT trace printing with proper dB units and a frequency X-axis (text + PostScript).

### Fixed
- Cursor not visible / arrow keys inert (uninitialized `cursor_visible`, no default
  selected trace).
- FFT grid showing "+01" scientific-notation artifacts (dB-aware auto-scaling).
- FFT cursor crash when selected as the active trace (bounds checking).
- CGA unit-display artifacts (uppercase HZ/KHZ/DB, etc.).
- Claimed "Restored GPIB abort/reset cleanup from v2.9 (main.c:260-264)" — this fix
  is **not present** in the code of v3.2 (or v2.9, v3.5, v3.6). See FINDINGS.md.

---

## [3.1] — June 2025 — Full Instrument Support & 287 Optimizations

Detailed changelog: [archive/v3.1/CHANGELOG_v3.1.md](archive/v3.1/CHANGELOG_v3.1.md)

### Fixed
- Module loading after config load ("No modules configured" error) — restored the full
  init chain (config init, buffer allocation, `sync_traces_with_modules`, GPIB remote).
- Configuration persistence — added `[ModuleConfigs]` so per-instrument settings
  (DM5120 termination/function/range/filter; DC5009/DC5010 function/channel) survive reload.
- FFT trace crash when made active; dB-grid display crash; trace toggle (`T` key, ALT+0–9).

### Added
- 287 math-coprocessor optimizations (Kahan summation, range-reduced trig, FPU precision
  control); floating-point drift correction via lookup table.

---

## [3.0] — June 2025 — Modular rewrite

Detailed changelog: [archive/v3.0/CHANGELOG_v3.0.md](archive/v3.0/CHANGELOG_v3.0.md)

### Changed
- **Modular rewrite.** Split the monolithic ~302 KB / 8,400-line source
  (which had hit the OpenWatcom *"E1118: segment too large"* wall) into ~8 modular
  translation units, each within the DOS 64 KB segment limit.
- New modular makefile with per-module dependency rules.

---

## [2.0 – 2.9] — Monolithic era

Source: [archive/v2-source/](archive/v2-source) (V2.2–V2.9 `.c` sources + V2.0–V2.9 binaries)

- Single-file C implementation (`TM5000L.c`, ~302 KB at v2.9). Became hard to edit
  and finally un-compilable once its code segment exceeded the DOS 64 KB limit, which
  prompted the v3.0 rewrite.
- v2.9 is still a behavioral baseline: several areas (DM5120 internal-trigger
  buffering, full-spectrum FFT storage, computed-trace survival) worked in v2.9 and
  regressed later. See [docs/FINDINGS.md](docs/FINDINGS.md).

---

## [1.8 – 1.9] — Binary-only era

Binaries: [archive/v1-binaries/](archive/v1-binaries)

- Earliest preserved builds (`tm5000 V1.8.exe`, `tm5000 V1.9.exe`). Limited
  functionality; no source survives.
