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

Built clean with OpenWatcom 2.0 (no new warnings), both executables; not yet run on
hardware.

### Fixed — memory safety
- **Printing:** legend and spectrum lines overran `label[40]` (text) and `label[80]`
  (PostScript), the screen-print caption overran `hdr[96]`, and a custom header over 63
  characters overran `g_custom_header_text`. Headers are now cut to 63.
- **Stack:** the profile functions kept up to 5 KB of locals on the 2 KB stack, so
  delete, save and first use aborted with "Stack Overflow!". Those buffers are now
  static. The GPIB terminal's 128-byte line plus CRLF wrote past its buffer, and
  over-long console lines are now refused instead of being sent in pieces.
- **GPIB:** a command too long for the 128-byte bus buffer is refused with an
  error instead of overflowing it. `gpib_write`, `gpib_write_dm5120` and
  `gpib_write_dm5010` return -1 when a command was refused or the write failed. A long
  driver status is cut to fit `g_error_msg`.
- **Buffers:** a failed `allocate_module_buffer` left the slot pointing at freed
  memory, and the trace kept a pointer to it after `free_module_buffer`. A failed
  allocation now leaves the slot empty, with its trace alias cleared. FFT,
  derivative, integral, smoothing and dual-trace results grow a too-small target
  buffer instead of writing past it, and stop with a message if memory runs out.
- Print and report lines are written with `snprintf`, so an overload reading
  (about 1e38) can't overrun the line buffers.
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
- **Loaded and FFT traces** no longer claim a time axis. `.tm5` files carry no time
  stamps, but loaded samples were stamped with the live measurement clock.
- Profile save errors name the cause: bad name, GPIB address over 30, all slots in
  use, or a write failure.
- File names typed at the save/load prompts are limited to 75 characters, so the
  extension still fits.
- Export file sizes of 32 KB and up were truncated to 16 bits. In scientific
  notation, zero now follows the precision setting.
- **PostScript:** the graph title and trace labels are escaped, so `(`, `)` or `\` in
  them no longer breaks the print job. The `%%Creator` line reports the real version.
  A print that spans midnight while the printer is busy no longer times out at once.

### Fixed — math
- **Digital filters** ran the wrong recurrence (feedback read from `a0`, and the
  highpass `a1` was wrong). *Order* now cascades that many sections, and the
  accumulator is double, so a 10 MHz counter trace keeps sub-Hz detail.
- **FFT** no longer scrubs NaNs and clips extremes in the source measurement itself;
  it works on its copy.
- The **no-287 FFT** used small-angle cos/sin approximations over the whole circle,
  and windowed a longer record than it transformed.
- Derivative, integral and smoothing gave wrong results when overwriting their own
  source slot. The integral trace now sets up its axes like the derivative.
- Removed `calculate_median`, `calculate_mode` and `calculate_frequency_analysis`: they
  were never called, and they returned scaled values in a 16-bit int.

### Fixed — instruments and UI
- **FG5010 communication test** reprograms frequency, amplitude and waveform and
  briefly enables the output. It now asks first, and always switches the output off
  again, even if the read-back times out. Ctrl-C/Ctrl-Break are ignored while the
  output is on. If the `OUTP OFF` write fails, a front-panel warning is shown. It also
  runs from the slot-setup "Test communication?" prompt.
- **DM5120:** the buffered fill sent `STOINT` as readings/s × 1000 (10 r/s sent
  `STOINT 10000`). The conflict check now tests the configured interval and refuses
  an invalid slot.
- **DM5120 bus handling** keyed off `address − 10` for addresses 10–17 only.
  Addresses are user-assigned, so the SRQ poll and longer settle now apply to every
  enabled DM5120, which adds about 200 ms per command to a DM5120 at any other
  address.
- **DM5010 advanced config** sent an uninitialized buffer instead of the command it
  had just built: all nine `dm5010_set_*` setters. They now send the commands v2.9
  sent (`CONF:`, `AVER:`, `CALC:`, `SYST:`); that command set has not been checked
  against the DM5010 manual.
- The DC5010 communication test ran the DC5009 test. The DC5009/DC5010 query
  functions now fill the caller's buffer.
- The FPU probe began with a waiting `fclex`, which can hang a machine with no 287,
  and read its control word uninitialized.
- The STOINT menu read a 16-bit int but accepted up to 999999, so 70000 wrapped to
  4464 and was sent. It now accepts 1–32767.
- "Select all" left slots 8–9 unselected. ALT-toggle could index `g_traces[-1]`.
  File-menu mouse clicks picked the item above the one clicked. A trace is shown
  the first time its slot has data; after that its visibility is the user's,
  including across the monitor's buffer-pool resize and file loads.
- Two fixed Driver488 settle waits in the custom-command and terminal screens now
  compile away in the GRIDGPIB build, like the other pacing waits.
- The custom-command and terminal consoles looped forever at end of input (Ctrl-Z
  or redirected stdin). They now return to the menu. The terminal also reports a
  failed driver write instead of "No response received".
- **Digital filters** start in steady state for the first sample, so a DC-offset trace
  no longer opens with a step or spike.
- Custom GPIB commands starting with `F` (e.g. `FUNCT DCV`) triggered a read and a
  SEQUENCE error. Only queries (`?`) read now.

### Fixed — DM5120 (checked against manual 070-7240-00 and the FV1.0 firmware's command table)
- **Error 551 in continuous monitoring:** the async buffer fill always sent `STOINT 100`,
  shorter than one conversion at 5½/6½ digits (about 133 ms at 50 Hz with AutoCal) and
  OHMSCOMP, so the meter stopped with 551 (SHORT TIME). Every `STOINT` is now raised to
  the conversion time for the slot's function, digits, range and AutoCal setting from the
  continuous-into-buffer rates (50 Hz figures, +30%), and to the 1–14 ms conflict
  floor: 174 ms at 6½ digits with AutoCal, next to the factory `STOINT 175`. The fill
  uses the burst rate when burst mode is on, else 100 ms, before that floor. The STOINT
  menu and configuration check report "shorter than a conversion" as its own code.
- The async fill gave up after a fixed 30 s, but 500 readings at 174 ms take 87 s. The
  limit now follows the buffer size × interval. A timed-out fill is reset instead of
  read: `BUFAVE?` holds the bus until a linear buffer is full.
- `EVENT?` replies with a number (`EVENT 450;` full, `451` half, `551` short time), but
  the fill looked for the words HALF/FULL and the status check for codes 1/2/4/8/16, so
  no event was ever seen. A 551 now shows on the monitor line.
- Every query reply carries a header (`BUFCNT 45;`), which the `BUFCNT?`, `BUFAVE?`,
  `BUFMIN?` and `BUFMAX?` parsers didn't skip: the count was always 0 and the buffer
  average always fell back to one stored reading.
- Commands the meter rejects (it accepts no abbreviations or variants): `DIGITS` is
  `DIGIT`, the filter count is `FILTERVAL` (not `FILTER <n>`), `BUFCLR` doesn't exist
  (`BUFSZ` clears the buffer), `X` is `SEND`, and the function menu's `DCI`/`ACI` are
  `DCA`/`ACA` (saved profiles holding the old names are corrected on Apply). Null is
  sent as `NULLVAL` then `NULL ON`, since `NULL 0` turns null off.
- The trigger menu stored TALK/EXT in the CONT/ONE field, so choosing EXT sent
  `TRIGGER TALK,ONE`. Source and mode are now asked separately, and a profile holding
  the old values is converted when it loads.
- "Manual" range sent `RANGE 1` (300 mV). The range menu lists RANGE 0–7 with each
  function's full scale (Table 3-3).
- `BUFSZ?` reported CIRCULAR as 500, which the buffer menu showed as LINEAR and its
  Clear turned into a 500-reading linear buffer.
- The STOINT menu showed "Continuous" for any interval and "ONE" when the query failed.
- Query Status ignored the LF-termination setting.

### Added — DM5120
- Functions ACVDB, ACADB and OHMSCOMP. Monitor lines and trace axes show V, A, ohm or
  dB to match the function.
- AutoCal and internal filter on/off (sent on Apply; AutoCal feeds the timing). They use
  two spare bits in the slot config, both 0 = factory ON, so `PROFILES.DAT` keeps its
  layout and version and older profiles load as ON.
- Query Status reads the whole setup with `SET?`. Apply sends its commands with RQS
  off, so errors stack, then prints each one with the manual's text (Table 3-5) instead
  of an unconditional "Settings applied!". Query Status prints whatever is still queued.
- Utilities menu: `ID?`, self test (`TEST?`), error/event queue, front-panel `TEXT`,
  `INIT`, `RESET` and `PONSAVE` (both confirmed first), and the firmware's `HELP?`.
- The reading-rate tables use the 50 Hz figures where they are slower, and the 6½-digit
  bus-triggered rate is 0.2/s (was 0.25).

### Build
- Makefile dependencies list each file's direct includes, including the GRIDGPIB
  objects, and `clean` also removes `.err` and `.map`.

---

## [3.7] — 2026-10-01 — TM5000G on GRIDGPIB, double precision, long runs

Detailed changelog: [docs/changelogs/CHANGELOG_v3.7.md](docs/changelogs/CHANGELOG_v3.7.md) ·
Driver: [driver/gridgpib/](driver/gridgpib/) · Previous source: [archive/v3.6/](archive/v3.6/)

Built and tested for Grid-OS on a GRiDCase 1520.

### Added
- **TM5000G (`tm5000g.exe`): the same program on the GRIDGPIB resident driver.**
  1.85 KB resident vs ≈43 KB for Driver488; serial poll 0.5 ms vs 87 ms; test-set
  bus time 8.5 s vs 34.8 s. `make` builds both executables.
- **Auto-detect modules** (Configure Modules → A). Scans GPIB addresses 1–30, reads
  each instrument's `ID?` (or `*IDN?`) and adds TM5000 modules not yet set up to
  free slots; existing slots are never changed.
- **Pooled buffers.** The continuous monitor shares 10 × 1024 samples among the
  modules it reads (one module: 8192); buffer size can also be set
  (Continuous Monitoring Setup → 4: AUTO, 1024 … 8192).
- **A full buffer averages instead of stopping.** Each stored sample becomes the mean
  of 2, 4, 8 … readings, shown by an `AVG xN` light.
- AUTO sample rate; per-sample measurement-time stamps; X axis in samples or time.
- Graph: P prints (with the print menu's setting), O opens the print menu, M toggles
  the mouse.
- FFT input and output up to 8192 points.

### Changed
- **Samples, statistics and analysis use `double`** so counter readings keep their
  resolution at 10 MHz and above.
- Graph offset axis for small changes on large values; cursor drawn without full
  redraws; readable footer.
- FFT and math use the source's measured sample interval; math traces keep its
  time stamps; FFT printouts show the source measurement's duration.
- Start-up text and terminal name the driver the build uses.
- The GRIDGPIB build releases the bus with one REN-off on exit.

### Fixed
- **Printing:** duration overflowed a 16-bit `int`; scales showed "0" on small
  ranges; the print report used the wrong buffer; text-graph stack overrun; LPT
  dropped bytes while the printer was busy.
- FFT/math results were written past a reused slot's buffer.
- DM5120 stored twice per pass in the continuous monitor.
- **Monitor:** stale rows looked like duplicated slots; the summary printed over them;
  counter readings below 1 MHz were shown as MHz.
- Buffered text landed at the wrong cursor position (`clrscr`/`gotoxy`).
- **Mouse pointer stayed hidden after a menu was left by a key.** The main,
  Measurement and File Operations menus hid it twice (in the key branch and after
  the loop) but showed it once, and the INT 33h driver counts hides.

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
  is **not present** in the code of v3.2 (or v2.9, v3.5, v3.6, v3.7). See FINDINGS.md.

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
