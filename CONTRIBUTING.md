# Contributing to TM5000

TM5000 is a 16-bit MS-DOS instrument-control program for Tektronix TM5000-series
hardware, kept alive as a retrocomputing / educational project. Contributions —
bug fixes, documentation, restored features, hardware notes — are welcome.

## Repository layout

```
TM5000/
├── src/         Current canonical source (v3.7) — build from here
├── archive/     Historical, frozen versions (read-only reference)
│   ├── v1-binaries/   V1.8 / V1.9 executables (no source survives)
│   ├── v2-source/     V2.0–V2.9 monolithic single-file C
│   └── v3.0 … v3.5/   Earlier modular releases
├── docs/        Architecture notes, per-version changelogs, build guide
├── CHANGELOG.md Consolidated version history
├── README.md
└── LICENSE
```

- **`src/` is the only tree you build and change.** It tracks the latest
  release line.
- **`archive/` is frozen.** Don't "fix" archived versions — they are kept
  precisely as historical record of what worked and what didn't. They are a
  valuable cross-version reference when diagnosing regressions.

## Building & running

See [docs/BUILD.md](docs/BUILD.md). In short: OpenWatcom C/C++ 1.9, then
`wmake` in `src/`, producing `tm5000.exe`. Test under DOSBox if you don't have
period hardware.

## Coding conventions

This is a constrained 16-bit DOS C codebase. Match the existing style:

- **C89 / ANSI C.** Declare all variables at the top of a block; no `//`
  line comments in code that must stay strictly portable (the existing code
  mixes both — prefer `/* */`).
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
4. If you add or remove a source file, update the `src/makefile` object list
   **and** the link line (they are currently maintained separately).
5. Open a pull request describing the change and how you verified it (real
   hardware, DOSBox, or reasoning).

## Reporting issues

Useful bug reports include: the version (`src` / which `archive` build), the
instrument module(s) involved, the exact steps, and what you expected vs. saw.
Cross-referencing against an archived version where the behavior was correct is
especially helpful.
