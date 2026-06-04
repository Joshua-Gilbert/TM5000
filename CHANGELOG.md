# Changelog

All notable changes to the TM5000 GPIB Control System. This consolidates the
per-version changelogs; the detailed originals are linked under each release.

The project evolved through three eras:
- **V1** — binaries only (no source survives)
- **V2** — a single monolithic C source file
- **V3** — the modular rewrite (current)

> ⚠️ **Accuracy note:** some fixes recorded as "done" in the original v3.2/v3.5
> changelogs are **not present in the shipped code**. See
> [docs/FINDINGS.md](docs/FINDINGS.md) for the code-verified record.

---

## [Unreleased] — Repository modernization

Housekeeping only; no change to program behavior.

- Restructured to a standard layout: current source under [`src/`](src), historical
  versions under [`archive/`](archive), documentation under [`docs/`](docs).
- Promoted v3.5 to `src/` as the canonical, buildable tree.
- Vendored `ieeeio.h` into `src/` (v3.5 previously relied on picking it up from the
  v3.0 folder; the build is now self-contained).
- Added root `.gitignore`, `CONTRIBUTING.md`, `docs/BUILD.md`, and this consolidated
  changelog; rewrote the root `README.md` for accuracy.
- `src/makefile`: removed the dead `trig287.asm` rule, corrected the `help` text
  (it advertised non-existent `fft_286.asm` / `trig287.asm`), made the toolchain path
  overridable via `WATCOM_BIN`, and flagged the incomplete `wcl` target. No flag or
  object-list changes — the produced binary is unchanged.
- Recorded cross-version regression analysis in [docs/FINDINGS.md](docs/FINDINGS.md).

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
