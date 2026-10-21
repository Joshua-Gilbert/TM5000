# TM5000 Cross-Version Findings

Code-backed findings from comparing the preserved versions against each other.
Because the repo keeps every release (V1 binaries, the V2 monolith, the V3.0–V3.5
modular line), each regression can be traced to where the behavior worked and
where it broke.

All six numbered findings were fixed in v3.6; the source tags each fix
`FINDINGS #n` with the numbers below. File:line references are to the pre-fix
code, preserved in [`archive/v3.5`](../archive/v3.5). Other baselines:
- Last monolithic version: [`archive/v2-source/TM5000 V2.9.c`](../archive/v2-source)
- Earlier modular line: [`archive/v3.0`](../archive/v3.0) … [`archive/v3.4`](../archive/v3.4)

---

## Changelog claims that were never applied

The v3.2 and v3.5 changelogs record fixes as done that **the shipped code
contradicts**. Treat changelog "FIXED"/"RESTORED" claims as unverified until
checked against source.

- **FFT magnitude normalization "fix" is absent.** `docs/changelogs/CHANGELOG_v3.5.md`
  claims a change to `/N`, threshold `1e-12`, floor `-240.0` at `math_functions.c:566`.
  [`archive/v3.5/math_functions.c`](../archive/v3.5/math_functions.c) still reads
  `/ (N/2)` at that line. The documented "5 Hz fix" exists nowhere in the code.
- **GPIB exit cleanup "abort/reset" restoration is absent.** `archive/v3.2/CHANGELOG_v3.2.md`
  (lines 37–41) claims it "Added missing GPIB cleanup commands 'abort' and 'reset'…
  Restored proper driver shutdown sequence from v2.9." But neither v3.2's `main.c`,
  v2.9's cleanup (`archive/v2-source/TM5000 V2.9.c:8314-8333`), nor `src/main.c`
  contain abort/reset; all three do `gpib_local` + `close()` + a `delay`. Still true in v3.6.

---

## Regressions and feature losses

### 1. Computed FFT/math result traces are silently destroyed — HIGH
- FFT results are created with `module_type = MOD_NONE`, `gpib_address = 0`
  ([`archive/v3.5/math_functions.c:615-617`](../archive/v3.5/math_functions.c)); dual-trace
  math results likewise set `MOD_NONE` and never set a gpib_address
  ([`archive/v3.5/math_enhanced.c:129`](../archive/v3.5/math_enhanced.c)).
- `continuous_monitor()` calls `validate_enabled_modules()` first
  ([`archive/v3.5/modules.c:5412`](../archive/v3.5/modules.c)), which **disables and frees**
  any enabled slot where `module_type == MOD_NONE` or `gpib_address < 1`
  ([`archive/v3.5/modules.c:1816-1826`](../archive/v3.5/modules.c)). So computing an
  FFT/math trace and then entering continuous monitor wipes the result.
- **Worked in v2.9:** the monolith had no `validate_enabled_modules`, so computed-result
  slots survived. The v3.3 ghost-module fix collides with the v3.3+ math-result feature.
  This also explains the "ghost modules count in continuous monitoring" limitation.

### 2. FFT low-frequency (dB) sensitivity regressed vs v3.3 — HIGH
- **Better in v3.3:** `archive/v3.3/math_functions.c:506,509` — dB threshold
  `1e-10`, floor `-200.0`.
- **Worse in v3.5:** [`archive/v3.5/math_functions.c:570,573`](../archive/v3.5/math_functions.c)
  — threshold raised to `1e-8`, floor raised to `-160.0`. Both make low-amplitude
  (low-frequency) bins *less* likely to display, the opposite of the documented goal.
- The documented `/N` normalization fix was never applied (see above).

### 3. DM5120 buffering: bounded internal trigger (v2.9) → external-trigger state machine that times out — HIGH
- **v2.9** (`archive/v2-source/TM5000 V2.9.c:1115,1152-1190`): `read_dm5120_buffered`
  used a fixed 10-sample buffer, `delay(100)`, and an *internal* trigger
  (`TRIGGER INT`). Bounded and self-completing.
- **v3.5** ([`archive/v3.5/modules.c:3210-3255`](../archive/v3.5/modules.c)
  `dm5120_fill_buffer_complete`): issues `dm5120_set_trigger(address, "EXT", "CONT")` then
  blocks; the async path ([`archive/v3.5/modules.c:3343-3349`](../archive/v3.5/modules.c))
  only escapes via a 30-second timeout. `EXT` needs an external trigger source; with none
  connected the buffer never fills, which is the documented "buffer timeout" / "manual
  front-panel trigger" symptom. The changelog frames `EXT,CONT` as the fix; it is the cause.

### 4. Enhanced Statistics ignores trace `unit_type` (always shows volts) — MEDIUM-HIGH
- [`archive/v3.5/ui.c:941`](../archive/v3.5/ui.c):
  `get_engineering_scale(range, NULL, &unit_str, &decimal_places)` — no unit_type argument;
  the function only derives V/mV/µV from the numeric range. Results print `unit_str`
  ([`archive/v3.5/ui.c:946-951`](../archive/v3.5/ui.c)), always voltage.
- So statistics on an FFT (dB), counter (Hz), resistance (Ω), or current (A) trace
  are labeled as volts. Cursor readouts and print became unit-aware in v3.2/v3.3; the
  v3.5 enhanced-stats path bypasses that.

### 5. `.tm5` import corruption — MEDIUM (latent since v3.3)
- `load_data` mixes `fscanf("%f")` for GlobalData
  ([`archive/v3.5/data.c:367`](../archive/v3.5/data.c)) with a single `fgets` to "skip to
  ModuleData" ([`archive/v3.5/data.c:379`](../archive/v3.5/data.c)); the fscanf leaves the
  stream mid-line, so the fgets consumes the wrong line. Any off-by-one in GlobalSamples
  desyncs the whole per-module read. The structure is identical in v3.3/v3.4/v3.5: a latent
  design issue, not a fresh regression.

### 6. FFT spectrum truncated (not decimated) when `output_points < N/2` — MEDIUM
- [`archive/v3.5/math_functions.c:564`](../archive/v3.5/math_functions.c) loops
  `i < output_points && i < N/2`, writing only the first `output_points` bins. A 1024-pt
  input with 64 output points shows only the lowest 64 of 512 bins; high-frequency content
  is dropped rather than resampled.
- **v2.9 always stored the full N/2 spectrum** (`archive/v2-source/TM5000 V2.9.c:7442-7445`).
  Introduced at v3.3 with configurable FFT; persists in v3.5. Default sizes
  (256→128, N/2=128) align, so it only bites on non-default sizes.

---

## Root-cause map for v3.5's documented "Current Limitations"

| Limitation (from v3.5 README) | Root cause | Earlier version that did it right |
|---|---|---|
| FFT 5 Hz low-freq display | `/N` fix never applied; constants regressed (`1e-8`/`-160` vs v3.3 `1e-10`/`-200`) (Finding 2) | v3.3 more sensitive; v2.9 used linear magnitude (no dB cutoff path) |
| Import measurement corruption | `fscanf`/`fgets` stream desync in `load_data` (Finding 5) | Same since v3.3 — latent, no clean baseline |
| Ghost modules in continuous monitoring | Computed-result slots (`MOD_NONE`/addr 0) counted then freed by `validate_enabled_modules` (Finding 1) | v2.9 had no validation, so results survived |
| DM5120 buffer timeout | `EXT,CONT` trigger with no external source → 30 s timeout (Finding 3) | v2.9 internal trigger + bounded 10-sample buffer |
| Enhanced Stats needs unit detection | `get_engineering_scale` called with NULL unit param (Finding 4) | cursor/print unit system (v3.2/v3.3) is unit-aware |

---

## Recommended fixes, ranked by value (all applied in v3.6)

Paths are to the pre-fix code in `archive/v3.5/`. What v3.6 actually did is in
[CHANGELOG_v3.6.md](changelogs/CHANGELOG_v3.6.md).

1. **Stop wiping computed traces.** Give FFT/math result slots a distinct marker
   (e.g. `MOD_COMPUTED` or an `is_result` flag) so `validate_enabled_modules` and
   the continuous-monitor count ignore them instead of treating them as ghosts.
   Fixes Finding 1 *and* the ghost-count limitation.
   (`modules.c:1816`, `math_functions.c:615`, `math_enhanced.c:129`)
2. **Actually apply the FFT dB fix.** At minimum restore v3.3's `1e-10`/`-200`
   (or better), and decide `/N` vs `/(N/2)` deliberately; re-check whether the
   5 Hz cutoff is in the auto-scale/grid path rather than normalization.
   (`math_functions.c:564-573`)
3. **DM5120 buffer triggering.** Default to internal/continuous triggering (v2.9
   model) and make `EXT` opt-in; shorten/abort the blocking wait.
   (`modules.c:3219-3246,3342-3350`)
4. **Wire `unit_type` into Enhanced Statistics** so dB/Hz/Ω/A traces report correct
   units. (`ui.c:940-951`)
5. **Harden `.tm5` import parsing** — replace mixed `fscanf`/`fgets` with
   line-oriented parsing + explicit count validation. (`data.c:359-383`)
6. **FFT output sizing** — decimate/bin-average to `output_points`, or clamp it to
   `N/2`, so high frequencies aren't silently dropped. (`math_functions.c:564`)

---

## Method

Claims in the V3.0–V3.5 changelogs/READMEs were checked against source across v2.9
(monolith), v3.0, v3.3, v3.4, and v3.5; each item above cites a file:line. The archive
directories were moved with `git mv` only, so their content and line numbers match the
original version directories.
