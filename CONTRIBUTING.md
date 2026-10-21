# Contributing to TM5000

TM5000 is a 16-bit MS-DOS instrument-control program for Tektronix TM5000-series
hardware, maintained as a retrocomputing / educational project. Bug fixes,
documentation, restored features, and hardware notes are welcome.

## Where to work

The full tree is in the [README](README.md#repository-layout).

- **`src/` is the only tree you build and change.** It tracks the latest release.
- **`archive/` is frozen.** Don't fix archived versions; they are the record of
  what worked and what didn't, and the baseline for diagnosing regressions.
- **`driver/gridgpib/`** holds the GRIDGPIB resident driver that `tm5000g.exe`
  runs on; it is assembled separately with NASM.

## Building & running

See [docs/BUILD.md](docs/BUILD.md). In short: OpenWatcom C/C++ (2.0 verified), then
build in `src/`, producing `tm5000.exe` and `tm5000g.exe`. Test under DOSBox if you
don't have period hardware.

## Coding conventions

This is a constrained 16-bit DOS C codebase. Match the existing style:

- **C89 / ANSI C.** Declare variables at the top of a block. Prefer `/* */`
  comments; the existing code mixes in some `//`.
- **Memory is scarce.** Conventional memory is 640 KB. Keep each module under
  the 64 KB DOS segment limit; use far pointers (`_fmalloc`, `far`) for large
  buffers, and prefer dynamic allocation over large static arrays.
- **`tm5000.h` is the single shared header** — all cross-module types, globals
  (`extern`), and constants live there. Globals are *defined* once in `main.c`.
- **Keep `-ml -2` clean.** Code must compile and run on a plain 80286 with the
  large memory model; a 287 coprocessor is optional, not assumed.
- **Don't assume hardware in core paths.** GPIB calls should degrade
  gracefully (time out) when no instrument responds.

## Making changes

1. Branch from `main`.
2. Make focused commits with clear messages.
3. If you change behavior, update [CHANGELOG.md](CHANGELOG.md) and the relevant
   doc in `docs/`.
4. If you add or remove a source file, update the `src/makefile` object lists
   (`OBJS`, and `OBJS_G` for `tm5000g.exe`) **and** both link lines; the link
   lines do not use `$(OBJS)` / `$(OBJS_G)`.
5. Open a pull request describing the change and how you verified it (real
   hardware, DOSBox, or reasoning).

## Reporting issues

Useful bug reports include: the version (`src` / which `archive` build), the
instrument module(s) involved, the exact steps, and what you expected vs. saw.
Cross-referencing against an archived version where the behavior was correct is
especially helpful.
