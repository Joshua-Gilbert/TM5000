# TM5000 v3.6 Changelog

## Version 3.6 — Regression Fixes
*Released: 2026-06-04*

Rationale for each fix of the issues in [docs/FINDINGS.md](../FINDINGS.md). Each
fix is tagged `FINDINGS #n` in the source. The new math, the dead-code cleanup, and
the full list of changes are in the consolidated [CHANGELOG](../../CHANGELOG.md).

**Build:** OpenWatcom 2.0, with linker dead-code elimination (`-zm` + `OPTION ELIMINATE`).
`tm5000.exe` is 275,948 bytes (~270 KB), down from 283,388 for the regression-fix build
before dead-code elimination, despite the added math.

---

### Fixed

#### Computed FFT/math result traces no longer wiped (FINDINGS #1) — HIGH
FFT and math-result traces were stored in a module slot tagged `MOD_NONE` with no
GPIB address. `validate_enabled_modules()` (run on entry to continuous monitoring)
treated those slots as "phantom" modules and **freed them**, and the monitor's init
loop `clear_module_data()`'d every enabled slot, so computing an FFT and then opening
the continuous monitor destroyed the result. This also produced the "ghost modules
counted in continuous monitoring" symptom.

- `tm5000_module` gains an `is_result` bit (one of the reserved flag bits). It is
  in-memory only, so file formats are unchanged. `tm5000.h`
- `module_is_result()` reports a slot as computed if the flag is set **or** it is
  `MOD_NONE` with data, so traces loaded from `.tm5`/`.cfg` (which don't carry the flag)
  are also recognized. `modules.c`
- `validate_enabled_modules()` and `continuous_monitor()`'s init loop skip computed
  slots: they are not freed, counted as active modules, or cleared. `modules.c`
- Every result-creation site sets `is_result = 1` (FFT, differentiation, integration,
  smoothing, dual-trace math), including the all-slots-full overwrite branches.
  `math_functions.c`, `math_enhanced.c`
- The flag is cleared wherever a slot is (re)assigned to a real instrument or removed
  (`configure_modules`, init, phantom cleanup, file load, profile restore), so it can't
  leak across reuse.

#### FFT dB sensitivity restored (FINDINGS #2) — HIGH
The dB conversion floored any bin below `1e-8` linear to `-160 dB`, less sensitive
than v3.3 (`1e-10` / `-200`), which dropped real low-amplitude/low-frequency content.
The v3.5 changelog claimed a `1e-12`/`-240` fix that was never in the code.
- Threshold `1e-8 → 1e-12`, floor `-160 → -240 dB`. `math_functions.c`
- Normalization stays at `/(N/2)`: it is a constant dB offset that does not change the
  spectrum shape or the low-frequency cutoff, and keeping it preserves existing absolute
  dB readings.

#### DM5120 buffer fill no longer requires an external trigger (FINDINGS #3) — HIGH
Both the synchronous and asynchronous DM5120 buffer-fill paths hardcoded
`TRIGGER EXT,CONT`, which needs an external trigger source; with none connected the
buffer never filled and the operation hit a 30-second timeout (the documented "buffer
timeout / manual front-panel intervention"). v2.9 used a self-running internal
trigger. Both paths now use the configured trigger source, defaulting to `TALK,CONT`
(the setting the single-measurement path already uses); `EXT` is opt-in via the DM5120
advanced config. `modules.c`

#### Enhanced Statistics labels non-voltage traces correctly (FINDINGS #4) — MEDIUM-HIGH
`calculate_statistics()` always called the voltage scaler, so statistics on an FFT
(dB), counter (Hz), resistance (Ω), current (A), or power trace were reported in
volts. It now dispatches on the trace's `unit_type` through `get_units_for_type()` to
the matching helper (`get_db_units`, `get_frequency_units`, `get_current_units`,
`get_resistance_units`, `get_power_units`, `get_derivative_units`, or
`get_graph_units`). `ui.c`, `graphics.c`

#### `.tm5` import resynchronises to the data marker (FINDINGS #5) — MEDIUM
`load_data()` read the global block with `fscanf("%f")`, which leaves the stream
mid-line, then skipped a single line to reach the per-module section. Any mismatch in
the global sample count desynchronised the per-module read ("failed to read sample" /
corrupted import). It now scans forward to the `ModuleData:` marker, tolerating a wrong
global count and any leftover partial line, and warns if the marker is missing. `data.c`

#### FFT spectrum no longer silently truncated (FINDINGS #6) — MEDIUM
When `output_points < N/2`, the magnitude loop wrote only the first `output_points`
bins, discarding all higher frequencies. It now **decimates across the full 0..Nyquist
span**, peak-preserving (each output bin takes the max of its source-bin group, so
narrow high-frequency peaks survive), and the frequency axis (`x_scale`, peak
frequency, peak centering, on-screen resolution) is scaled to match. When
`output_points == N/2` (the default) the mapping is 1:1 and output is unchanged.
`math_functions.c`

---

### Notes
- The four assembly modules are still not called from C; `OPTION ELIMINATE` now strips
  them from the image. See [ARCHITECTURE.md](../ARCHITECTURE.md).
- Header cleanup (de-declaring the god header, `MAX_MODULES`, `static`-ization) is left
  for a future pass.
