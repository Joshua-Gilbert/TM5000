# TM5000 v3.6 Changelog

## Version 3.6 — Regression Fixes
*Released: 2026-06-04*

v3.6 implements fixes for every issue catalogued in
[docs/FINDINGS.md](../FINDINGS.md) — the cross-version regression analysis produced
during the repo modernization. Each fix is tagged in the source with the matching
`FINDINGS #n`. No new features; this is a correctness release.

**Build:** compiles cleanly (0 errors, no new warnings) with OpenWatcom (verified with
the OpenWatcom v2 successor toolchain). The makefile now enables linker dead-code
elimination (`-zm` + `OPTION ELIMINATE`), so `tm5000.exe` is **≈ 269 KB (274,924 bytes)**
— smaller than v3.5/v3.6-baseline (~283 KB) despite the added features.

---

### Fixed

#### 1. Computed FFT/math result traces no longer wiped (FINDINGS #1) — HIGH
FFT and math-result traces were stored in a module slot tagged `MOD_NONE` with no
GPIB address. `validate_enabled_modules()` (run on entry to continuous monitoring)
treated exactly those slots as "phantom" modules and **freed them**, and the monitor's
init loop additionally `clear_module_data()`'d every enabled slot — so computing an
FFT and then opening the continuous monitor destroyed the result. This also produced
the "ghost modules counted in continuous monitoring" symptom.

- Added an `is_result` bit to `tm5000_module` (reused one of the reserved flag bits;
  in-memory only — not serialized, so file compatibility is unaffected). `tm5000.h`
- `validate_enabled_modules()` now skips `is_result` slots instead of freeing them. `modules.c`
- `continuous_monitor()`'s init loop skips `is_result` slots, so they are neither
  counted as active modules nor cleared. `modules.c`
- All five result-creation sites set `is_result = 1` (FFT, differentiation,
  integration, smoothing, dual-trace math). `math_functions.c`, `math_enhanced.c`
- The flag is reset to 0 wherever a slot is (re)assigned to a real instrument or
  removed (`configure_modules`, init, phantom cleanup), so it can't leak across reuse.

#### 2. FFT dB sensitivity restored / deepened (FINDINGS #2) — HIGH
The dB conversion floored any bin below `1e-8` linear to `-160 dB` — *less* sensitive
than v3.3 (`1e-10` / `-200`), which dropped genuine low-amplitude/low-frequency
content. (The v3.5 changelog claimed a `1e-12`/`-240` fix that was never actually in
the code.) Now applied for real:
- Threshold `1e-8 → 1e-12`, floor `-160 → -240 dB`. `math_functions.c`
- Normalization left at `/(N/2)` deliberately — it is a constant dB offset that does
  not affect spectrum shape or the low-frequency cutoff, and keeping it preserves the
  absolute dB readings existing users are calibrated to.

#### 3. FFT spectrum no longer silently truncated (FINDINGS #6) — MEDIUM
When `output_points < N/2`, the magnitude loop wrote only the first `output_points`
bins — i.e. just the lowest slice of the spectrum — silently discarding all higher
frequencies. It now **decimates across the full 0..Nyquist span**, peak-preserving
(each output bin takes the max of its source-bin group, so narrow high-frequency
peaks survive), and the trace's frequency axis (`x_scale`, peak frequency, peak
centering, on-screen resolution) is scaled to match. In the default case
(`output_points == N/2`) this is an exact 1:1 mapping — behaviour is unchanged. `math_functions.c`

#### 4. DM5120 buffer fill no longer requires external trigger (FINDINGS #3) — HIGH
Both the synchronous and asynchronous DM5120 buffer-fill paths hardcoded
`TRIGGER EXT,CONT`, which needs an external trigger source — with none connected the
buffer never filled and the operation hit a 30-second timeout (the documented "buffer
timeout / manual front-panel intervention"). v2.9 used a self-running internal
trigger. Both paths now honour the configured trigger source, defaulting to
`TALK,CONT` (no external hardware required — the same setting the function's own
single-measurement path uses); `EXT` is opt-in via the DM5120 advanced config. `modules.c`

#### 5. Enhanced Statistics now labels non-voltage traces correctly (FINDINGS #4) — MEDIUM-HIGH
`calculate_statistics()` always called the voltage scaler, so statistics on an FFT
(dB), counter (Hz), resistance (Ω), current (A), or power trace were reported in
volts. It now dispatches on the trace's `unit_type` to the matching unit helper
(`get_db_units`, `get_frequency_units`, `get_current_units`, `get_resistance_units`,
`get_power_units`, `get_derivative_units`, or `get_graph_units`). `ui.c`

#### 6. `.tm5` import resynchronises to the data marker (FINDINGS #5) — MEDIUM
`load_data()` read the global block with `fscanf("%f")` (which leaves the stream
mid-line) and then blindly skipped a single line to reach the per-module section. Any
mismatch in the global sample count desynchronised the entire per-module read
("failed to read sample" / corrupted import). It now scans forward to the explicit
`ModuleData:` marker, tolerating an off-by-one or wrong global count and any leftover
partial line. `data.c`

---

### Notes
- The four assembly modules remain dormant (not called from C); they are now **stripped
  from the image** by `OPTION ELIMINATE` rather than linked dead-weight. See
  [docs/ARCHITECTURE.md](../ARCHITECTURE.md) and FINDINGS.
- The code-reuse / dead-code cleanup from the review pass and the enhanced-math wiring
  (curve fit, cross-correlation, phase shift) are folded into this release; see the
  "Added" and "Code cleanup" sections in the consolidated [CHANGELOG](../../CHANGELOG.md).
  Header-hygiene items (god-header de-declaration, `MAX_MODULES`, `static`-ization)
  remain for a future pass.
