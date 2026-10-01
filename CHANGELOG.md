# Changelog

All notable changes to the TM5000 GPIB Control System. This consolidates the
per-version changelogs; the detailed originals are linked under each release.

The project evolved through three eras:
- **V1** — binaries only (no source survives)
- **V2** — a single monolithic C source file
- **V3** — the modular rewrite (current)

> ⚠️ **Accuracy note:** several fixes recorded as "done" in the original v3.2/v3.5
> changelogs were **not actually present in the shipped code** (see
> [docs/FINDINGS.md](docs/FINDINGS.md)). v3.6 applies them for real and verifies the
> result against a build.

---

## [Unreleased] — Grid-OS: TM5000G (GRIDGPIB), double precision, graph and print fixes

Detailed changelog: [docs/changelogs/CHANGELOG_GRIDOS.md](docs/changelogs/CHANGELOG_GRIDOS.md) ·
Driver: [driver/gridgpib/](driver/gridgpib/)

### Added
- **Auto-detect modules** (Configure Modules → A): scans GPIB addresses 1–30, reads
  each instrument's `ID?` (or `*IDN?`), lists everything found and adds the TM5000
  modules that aren't set up yet to free slots (existing slots are never changed).
- **TM5000G** (`tm5000g.exe`): the same program on the GRIDGPIB resident driver
  (1.85 KB resident vs ≈43 KB for Driver488; serial poll 0.5 ms vs 87 ms; test-set
  bus time 8.5 s vs 34.8 s). `wmake` now builds both executables.
- AUTO sample rate; per-sample measurement-time stamps; X axis in samples or time.
- Averaging when a buffer fills (mean of 2, 4, 8 … readings) with an `AVG xN`
  light, instead of silently stopping at 1024 samples.
- Graph: P prints the screen as shown (PostScript), O print menu, M mouse toggle.

### Changed
- Samples, statistics and analysis use `double` (counter resolution at 10 MHz+).
- Graph offset axis for small changes on large values; cursor drawn without full
  redraws; readable footer.

### Fixed
- Printed duration overflowed (16-bit `int`); print report used the wrong buffer;
  text-graph stack overrun; LPT dropped bytes while the printer was busy.
- DM5120 stored twice per pass in the continuous monitor.
- Buffered text landing at the wrong cursor position (`clrscr`/`gotoxy`).

---

## [3.6] — 2026-06-04 — Regression fixes

Detailed changelog: [docs/changelogs/CHANGELOG_v3.6.md](docs/changelogs/CHANGELOG_v3.6.md) ·
Root-cause analysis: [docs/FINDINGS.md](docs/FINDINGS.md)

Correctness release — implements a fix for every issue in `docs/FINDINGS.md`. Builds
clean (0 errors, no new warnings); verified with the OpenWatcom v2 toolchain.

### Fixed
- **Computed FFT/math traces no longer wiped** on entering continuous monitoring
  (added an `is_result` flag so they're exempt from phantom-module cleanup; also
  resolves "ghost modules" in the monitor). *FINDINGS #1*
- **FFT low-frequency dB sensitivity** — applied the long-documented-but-missing fix
  (threshold `1e-8→1e-12`, floor `-160→-240 dB`). *FINDINGS #2*
- **FFT spectrum no longer silently truncated** when `output_points < N/2` — now
  peak-preserving decimation across the full spectrum with a correctly scaled
  frequency axis. *FINDINGS #6*
- **DM5120 buffer fill no longer stalls** waiting on an external trigger — defaults to
  `TALK,CONT`, with `EXT` opt-in. *FINDINGS #3*
- **Enhanced Statistics** labels dB/Hz/Ω/A/power traces correctly instead of always
  volts. *FINDINGS #4*
- **`.tm5` import** resynchronises to the `ModuleData:` marker, tolerating a global
  sample-count mismatch. *FINDINGS #5*

### Added (enhanced math wired up)
- **Polynomial & exponential curve fitting** — the `curve_fitting_menu` options that
  previously read "coming in v3.6" now call the implemented `fit_polynomial`
  (order 2–3) and `fit_exponential` routines.
- **Cross-correlation & phase/delay analysis** — implemented the previously
  declared-only `calculate_cross_correlation` and `calculate_phase_shift` and wired them
  into `correlation_analysis_menu` (peak-alignment lag, zero-lag correlation, and a
  sample-rate-scaled time delay).

### Repository / tooling (same date)
- Restructured to a standard layout: source under [`src/`](src), history under
  [`archive/`](archive), docs under [`docs/`](docs); promoted v3.5 to `src/` and froze
  it at [`archive/v3.5`](archive/v3.5).
- Vendored `ieeeio.h` into `src/` so the build is self-contained.
- Added root `.gitignore`, `CONTRIBUTING.md`, `docs/BUILD.md`, this consolidated
  changelog, and the regression analysis in `docs/FINDINGS.md`; rewrote the root README.
- `src/makefile`: overridable `WATCOM_BIN`, removed the dead `trig287.asm` rule, fixed
  the stale `help` text, flagged the incomplete `wcl` target (object list/flags unchanged).

### Code cleanup (behavior-preserving refactor)
From the code-review pass; build-verified and confirmed by an adversarial
behavior-preservation review. No runtime behavior change.
- Consolidated the **DC5009/DC5010 driver twins**: 22 byte-identical `dc5010_*`
  functions now forward to their `dc5009_*` implementations (single source of truth;
  the DC5010-unique functions are untouched).
- Deduplicated the two PostScript-unit ladders in `print.c` into one `ps_unit_of()` helper.
- Removed **15 dead (zero-caller) functions** + their stale prototypes: graphics
  primitives (`draw_circle`, `fill_circle`, `draw_rectangle`, `set_cga_palette`,
  `wait_vretrace`, `draw_filled_rect`, `draw_readout`, `mouse_in_region`,
  `set_mouse_pos`), unreachable stub menus (`data_analysis_menu`, `unit_conversion_menu`,
  `calculator_menu`), and superseded/demo readers (`read_dm5120`, `read_ps5010`,
  `dm5120_buffer_example`).
- **Enabled linker dead-code elimination** — `wcc -zm` (one segment per function) +
  `wlink OPTION ELIMINATE` now strip every unreferenced function and the dormant
  assembly modules from the image. (OpenWatcom `wlink` has no identical-code *folding*
  à la MSVC `/OPT:ICF`, so the DC5009/DC5010 twins still had to be forwarded by hand —
  but it *can* eliminate unreferenced code.) `src/makefile` and the build are updated.
- **Binary size (OpenWatcom v2):** 283,388 (v3.6) → 280,348 (reuse refactor + manual
  dead-code removal) → **275,948 final** — **−7,440 bytes (−2.6%) smaller than v3.6
  even after adding the curve-fit / cross-correlation / phase-shift features**, thanks
  to the dead-strip build options. Lesson: consolidating *small* duplicate functions is
  binary-neutral; eliminating/removing whole unused functions is the real lever.

### Review hardening
Fixes from an extra-high-effort multi-agent code review of the above changes
(adversarially verified, no behaviour regressions):
- **Computed-trace persistence now actually works:** `module_is_result()` derives
  "computed trace" from state (`MOD_NONE` + data) as well as the `is_result` flag, so
  FFT/math traces survive a `.tm5`/`.cfg` reload (the flag isn't serialized); the
  all-slots-full FFT/derivative/integral/smoothed *overwrite* branches now mark the
  result too; and `sync_traces_with_modules` preserves units for *all* computed traces,
  not just `"FFT Result"`.
- **Cross-correlation** now normalizes per-lag by overlap energy (peak no longer biased
  toward lag 0), uses `double` accumulators, builds the centred series once, and bounds
  the search to the reliable lag range.
- **fit_polynomial** centres/scales X so the normal equations stay conditioned (raw
  sample indices up to ~1023 made the cubic fit garbage in single precision).
- **Non-287 FFT** now applies the output format, fixes the frequency axis, and drops the
  garbage tail (with an "approximate" notice) instead of storing mislabeled linear data.
- Minor: shared `get_units_for_type()` helper (was duplicated across `ui.c`/`print.c`),
  `.tm5` import warns if the data marker is missing, cross-correlation prints a progress note.

---

## [3.5] — July 2025 — Data Management Foundation

Detailed changelog: [docs/changelogs/CHANGELOG_v3.5.md](docs/changelogs/CHANGELOG_v3.5.md) ·
Release notes: [docs/changelogs/v3.5-release-notes.md](docs/changelogs/v3.5-release-notes.md)

### Added
- Configuration Profiles system — save/load complete system configurations
  (`config_profiles.c`, persisted to `PROFILES.DAT`).
- Enhanced data export — CSV/TSV with metadata, timestamps, custom formatting, and
  real-time streaming export (`export_enhanced.c`).
- Enhanced math (`math_enhanced.c`, ~1,100 lines — a full implementation, **not** the
  "stub" the original docs called it): dual-trace operations, rolling statistics,
  digital filtering, curve fitting, correlation, with descriptive trace legends.
- Direct-memory-access CGA assembly for faster draw rates; 286/287 assembly modules.

### Changed
- FFT moved to a pure-C implementation to eliminate 287-coprocessor hangs.

### Known issues (root-caused in [docs/FINDINGS.md](docs/FINDINGS.md))
- FFT low-frequency display; `.tm5` import corruption; ghost modules counted in
  continuous monitoring; DM5120 buffer timeouts; Enhanced Stats unit detection.
- ⚠️ The documented FFT magnitude-normalization fix (`/N`, `1e-12`, `-240`) was
  **never applied**; the code still uses `/(N/2)`, `1e-8`, `-160` — a regression from
  v3.3's `1e-10`/`-200`.

---

## [3.4] — July 2025 — Buffers & Memory Optimization

(No standalone changelog was written for v3.4; see
[archive/v3.4/TM5000_v3.4_Structure.md](archive/v3.4/TM5000_v3.4_Structure.md).)

### Changed
- 1024-sample per-module buffers (up from earlier limits).
- ~22% memory reduction through buffer optimization for the 640 KB DOS budget.

---

## [3.3] — June 2025 — Critical Bug-Fix Release

Detailed changelog: [archive/v3.3/CHANGELOG_v3.3.md](archive/v3.3/CHANGELOG_v3.3.md)

### Fixed
- Stack overflow / memory corruption in module selection — eliminated 7 duplicate
  function definitions causing linker symbol conflicts (clean compile, smaller code).
- Ghost/phantom modules in continuous monitoring — added module type / GPIB address /
  description validation. *(Note: this validation later collides with computed math
  result traces in v3.5 — see FINDINGS.md #2.)*
- Configuration/measurement save-load failures — removed unreliable
  `fseek(..., SEEK_CUR)` calls that fail under DOS buffered I/O.

---

## [3.2] — June 2025 — Enhanced Stability

Detailed changelog: [archive/v3.2/CHANGELOG_v3.2.md](archive/v3.2/CHANGELOG_v3.2.md)

### Added
- FFT trace printing with proper dB units and a frequency X-axis (text + PostScript).

### Fixed
- Cursor not visible / arrow keys inert (uninitialized `cursor_visible`, no default
  selected trace).
- FFT grid showing "+01" scientific-notation artifacts (dB-aware auto-scaling).
- FFT cursor crash when selected as the active trace (bounds checking).
- CGA unit-display artifacts (uppercase HZ/KHZ/DB, etc.).
- ⚠️ Claimed "Restored GPIB abort/reset cleanup from v2.9 (main.c:260-264)" — this fix
  is **not present** in the code of v3.2 (or v2.9, or v3.5). See FINDINGS.md.

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

## [3.0] — June 2025 — Modular Architecture Release

Detailed changelog: [archive/v3.0/CHANGELOG_v3.0.md](archive/v3.0/CHANGELOG_v3.0.md)

### Changed
- **Complete architectural rewrite.** Split the monolithic ~302 KB / 8,400-line source
  (which had hit the OpenWatcom *"E1118: segment too large"* wall) into ~8 modular
  translation units, each within the DOS 64 KB segment limit.
- New modular makefile with per-module dependency rules.

---

## [2.0 – 2.9] — Monolithic era

Source: [archive/v2-source/](archive/v2-source) (V2.2–V2.9 `.c` sources + V2.0–V2.9 binaries)

- Single-file C implementation (`TM5000L.c`, ~302 KB at v2.9) with "decent
  functionality." Became difficult to edit and eventually un-compilable as the single
  source segment exceeded the DOS 64 KB limit — the motivation for the v3.0 rewrite.
- v2.9 remains a useful behavioral baseline: several areas (DM5120 internal-trigger
  buffering, full-spectrum FFT storage, computed-trace survival) worked in v2.9 and
  regressed later. See [docs/FINDINGS.md](docs/FINDINGS.md).

---

## [1.8 – 1.9] — Binary-only era

Binaries: [archive/v1-binaries/](archive/v1-binaries)

- Earliest preserved builds (`tm5000 V1.8.exe`, `tm5000 V1.9.exe`). Limited
  functionality; no source survives. Kept as historical record.
