# TM5000 v3.7 — Grid-OS: TM5000G and shared fixes (2026-09-29 – 10-01)

Work done while building Grid-OS for the GRiDCase 1520. Everything was run on
the real machine (GRiDCase 1520, 286 + 287, TM5006A with DC5009 / DM5120 /
DM5010 / FG5010 / PS5004 / PS5010, PRS10 rubidium reference) except where
noted.

## New: TM5000G — TM5000 on the GRIDGPIB driver

- `gpib_gg.c` (GPIB back end v4.1) implements the unchanged `gpib.h` API on the
  GRIDGPIB resident driver (`driver/gridgpib/`, C interface `gridgpib.c/.h`).
  Every call returns when the bus transaction is complete; replies are read
  whole and never split across reads.
- A small Driver488 text-command interpreter behind `ieee_write`/`ieee_read`
  (OUTPUT, ENTER, SPOLL, STATUS, ABORT, RESET, CLEAR, REMOTE, LOCAL, TRIGGER,
  HELLO, TIME OUT; FILL/EOL/BREAK accepted) keeps the terminal and diagnostics
  working.
- Per-instrument read timeouts: DC5009/DC5010 12 s (long gates), DM5120 10 s
  (AUTOCAL pauses, seen with direct chip access too), DM5010 5 s, others 2 s.
  Waits end as soon as the reply arrives.
- The 215 fixed waits after bus calls in `modules.c` / `module_funcs.c` are now
  `GPIB_PACE(ms)`: `delay(ms)` in the Driver488 build (unchanged behaviour),
  nothing in the GRIDGPIB build (`-DGRIDGPIB`).
- `makefile`: `all` builds both `tm5000.exe` (Driver488) and `tm5000g.exe`.

## Measurement

- **Double-precision samples.** Module buffers, the global buffer, trace data,
  `last_reading`, statistics and the analysis functions use `double`; counter
  replies are parsed with `%lf`. A `float` holds ~7 digits — 1 Hz at 10 MHz,
  8 Hz at 100 MHz — so counter traces were quantised. Cost: 8 bytes per sample
  (was 4). Data files are written with `%.12g`.
- **Time stamps.** Every sample the continuous monitor stores carries its
  measurement time (BIOS tick, 54.9 ms; pauses excluded), 4 bytes per sample.
- **Buffer full → averaging instead of stopping.** At 1024 samples the buffer is
  merged in pairs (mean of values and of times) and each later sample is the
  mean of 2, 4, 8 … readings, so the buffer always covers the whole run. The
  monitor shows a reverse-video `AVG xN` light; the monitor counts `Readings`.
  Previously recording stopped silently at 1024 samples.
- **AUTO sample rate** (Sample Rate menu option 9): the first 3 passes are timed
  per instrument and the rate locks to the slowest cycle, rounded up to whole
  timer ticks.
- **Monitor Time = measurement time** (excludes pauses; reset with C).
- **DM5120 stored twice per pass** in the continuous monitor (once inside
  `read_dm5120_enhanced`, once by the monitor) — fixed.
- Statistics use a two-pass standard deviation (no cancellation on large values).
- Differentiate/integrate and export timestamps use the measured sample
  interval (including averaging) instead of the set rate.

## Graph

- **Offset axis** for small changes on large values (a 10 MHz source moving a
  few Hz): labels are signed offsets from a round reference shown in the title
  (`REF 10.000000MHZ`); auto-scale snaps to 1/2/2.5/5×10ⁿ per division. Axis
  limits are kept in `double` alongside the `float` `graph_scale` (profile file
  format unchanged).
- **Cursor without full redraw:** the cursor line and readout are drawn over a
  saved copy of the screen bytes beneath them; moving the cursor no longer
  redraws grid, traces and legend. Traces are mapped once per redraw in double
  (the 287 path previously plotted from float limits).
- **Mouse:** on by default when a driver is present, pixel-accurate in 320×200,
  pointer hidden while drawing; **M** toggles.
- **X: sample number ↔ measurement time** (0.1 s / M:SS / H:MM labels, never
  finer than the clock).
- Zoom and pan work on the double-precision limits.
- **Footer:** solid background (the two-tone bar was unreadable on the plasma
  display and took 4,800 pixel writes), 3×5 font, right-aligned scale/div and
  status (cursor step, 287, AVGxN, time unit); fields no longer overlap.
- **P** prints the screen as shown (PostScript grey image of the graph area,
  caption with scale and status); **O** opens the print menu.

## Printing

- Text and PostScript graphs work in double and use the same offset-axis rule;
  legend values get enough decimals for the trace's spread.
- **Duration** came from `(samples − 1) × rate` in 16-bit `int` and overflowed
  (1023 × 275 ms printed as 19.2 s, 841 × 275 as −30.9 s); it now comes from
  the samples' time stamps (marked "measured"), else samples × interval ×
  averaging in floating point. The measured sample interval and the averaging
  factor are printed; the x axis follows the X:TIME view.
- **Print report** statistics come from each slot's own buffer (the old global
  buffer only held the first slot's first 1024 readings).
- Text graph: 10 labelled divisions; fixed a stack overrun (the line-clearing
  loop wrote 100 bytes into 81/80-byte buffers).
- LPT: port address from the BIOS (0040:0008) instead of a fixed 378h; BUSY is
  waited out for up to 10 s (bytes were dropped after ~1000 polls); a real
  timeout stops the job with "Printer not ready".

## Display

- Continuous monitor lines are built in a buffer and written straight to video
  memory, only when changed (smoother on the 1520).
- `clrscr()` / `gotoxy()` flush `stdout` first (buffered text was landing at the
  new cursor position — duplicated-looking module lines).

## Added after the first Grid-OS merge (PR #2)

- **Auto-detect modules** (Configure Modules → A) using `gpib_probe()` (new in
  `gpib.h`): GRIDGPIB serial-polls with a 150 ms timeout and queries `ID?` /
  `*IDN?`; Driver488 uses `TIME OUT 1` around `OUTPUT`/`ENTER`. Tested on the bus.
- **Pooled buffers**: the monitor shares 10 × 1024 samples among the modules it
  reads (1 module: 8192 = 8 × 1024); `alloc_samples()` gets a 64 KB block from DOS
  when `_fmalloc` cannot express the size. Buffer size per module selectable
  (AUTO, 1024, 2048, 4096, 8192). Tested: 8192 samples, averaging to ×2.
- **P key** prints with the print menu's setting (PostScript plot by default; text
  graph or screen copy via menu option 5).
- **Monitor**: stale rows blanked each pass; summary on a cleared screen; counter
  readings in Hz/kHz/MHz. Tested.
- **Printouts**: range and per-division values keep 3 significant digits (a 2 Hz
  range printed "0 Hz per division"); Y-scale limits get enough decimals; double
  limits everywhere. Graph settings screen (H) shows double limits.
- **FFT**: input sizes 64–8192, output up to the input size; double-precision
  twiddles with a full-precision π; default sample rate from the source's measured
  interval; FFT/math results reallocate a too-small reused slot (they wrote past
  it); an FFT result keeps the source measurement's duration for printouts.
  Derivative/integral/smoothing/trace math copy the source's time stamps and keep
  double precision.
- **Start-up text** names the driver the build uses (`gpib_driver_name`,
  `gpib_driver_help()`).
- **Exit**: GRIDGPIB build drops REN once (`gpib_release_bus()`) instead of GTL to
  every slot; FFT/math slots skipped; mouse driver reset.
- Open: FFT trace cursor readout reported missing on the Grid (not yet diagnosed).

## Mouse pointer after a key (Grid-OS 0.3.1, 2026-10-06)

- Grid-OS 0.3.1 gives DOS programs the mouse (INT 33h), so TM5000G now finds it.
  Its review found that the main, Measurement and File Operations menus called
  `hide_mouse()` twice when left by a key (once before `getch()`, once after the
  loop) but `show_mouse()` once. An INT 33h driver counts hides, as Microsoft's
  does, so after the first keyboard choice the pointer never came back. The
  inner `hide_mouse()` is gone; the one after the loop covers both the key and
  the click. Tested in Grid-OS's emulator: after a key choice and back, the
  pointer shows again.
