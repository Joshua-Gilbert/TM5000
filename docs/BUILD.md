# Building TM5000

TM5000 is a 16-bit MS-DOS application. It is built with the **OpenWatcom
C/C++ 1.9** toolchain targeting real-mode DOS, and it runs on period hardware
(or DOSBox / 86Box) — not on a modern OS directly.

The canonical, buildable source tree is [`src/`](../src) (TM5000 v3.7).
Historical versions live under [`archive/`](../archive) and are kept for
reference; this guide describes building the current `src/` tree.

---

## 1. Prerequisites

| Component | Requirement |
|-----------|-------------|
| Compiler  | OpenWatcom C/C++ **1.9** (`wcc`, the **16-bit** compiler — *not* `wcc386`) |
| Assembler | OpenWatcom `wasm` (for the `.asm` optimization modules) |
| Linker    | OpenWatcom `wlink` |
| Build tool| `wmake` (native) **or** GNU `make` calling the Watcom `.exe`s |
| Run target| MS-DOS 3.3+ on a 286/287, CGA graphics, a Personal488 (IOtech) GPIB card — or [DOSBox](https://www.dosbox.com/) for testing without hardware |

Install OpenWatcom 1.9 and make sure `WATCOM\binnt` (Windows) or
`watcom/binl` (Linux) is on your `PATH`. The OpenWatcom installer normally
sets the `WATCOM`, `INCLUDE`, and `PATH` environment variables via
`owsetenv.bat` / `owsetenv.sh`.

---

## 2. Build flags (what they mean)

The `makefile` compiles with:

```
CFLAGS = -ml -2 -bt=dos -os -d0 -zc -zt100
```

| Flag | Meaning | Why it matters |
|------|---------|----------------|
| `-ml` | **Large** memory model | The data set exceeds 64 KB; far pointers are required for buffers |
| `-2`  | 80286 instruction set | Target CPU is the 286 |
| `-bt=dos` | Build target = DOS | Real-mode 16-bit output |
| `-os` | Optimize for **size** | Must fit conventional 640 KB memory |
| `-d0` | No debug info | Smaller binary (use `-d2` for a debug build) |
| `-zc` | Place `const` data in the **code** segment | Frees scarce `DGROUP` data space |
| `-zt100` | Data threshold 100 | Items ≥100 bytes go *far* automatically, keeping `DGROUP` under 64 KB |

Assembly modules build with `-ml -2`; `trig287_simple.asm` additionally uses
`-fp2` (287 FPU instructions).

> These flags are **load-bearing** for a 286/287/CGA DOS target. Changing the
> memory model or data threshold will break the 64 KB-segment compliance the
> code depends on.

---

## 3. Building

From inside `src/`:

```sh
wmake          # native OpenWatcom make
# or
make           # GNU make (e.g. under WSL) invoking the Watcom tools
```

Output: `tm5000.exe` (Driver488) and `tm5000g.exe` (GRIDGPIB, ~290 KB each).

`tm5000g.exe` uses `gpib_gg.c` + `gridgpib.c` instead of `gpib.c`, and compiles
`modules.c` / `module_funcs.c` a second time with `-DGRIDGPIB` (as
`modules_g.obj` / `module_funcs_g.obj`) so the fixed Driver488 pacing waits
(`GPIB_PACE`) compile to nothing. It needs `GRIDGPIB.COM` loaded instead of
`DRVR488` — see [`driver/gridgpib/`](../driver/gridgpib).

OpenWatcom 2.0 for Linux (`binl64`) also builds the tree with GNU make (its
`wcc` writes `.o` by default, hence `-fo=.obj`):

```sh
make CC="wcc -q -fo=.obj" ASM="wasm -q -fo=.obj" LINKER="wlink option quiet"
```

Built this way, `tm5000g.exe` is byte-identical to the build tested on the
GRiDCase 1520.

Other makefile targets:

- `wmake clean` — remove `*.obj` and the executable
- `wmake help`  — print build help

> **Note:** the `wcl` shortcut target is **incomplete** — it omits several C
> modules and all assembly objects, so it produces a different/broken binary.
> Use the default `all` target (`wmake` with no argument).

---

## 4. Running

On real hardware or DOSBox:

1. Ensure the Personal488 driver (`DRVR488.EXE`) is loaded (for GPIB hardware).
2. Run `TM5000.EXE` from the DOS prompt.

To test the UI without GPIB hardware, launch under DOSBox — the program starts
and the menus are navigable; instrument operations will simply time out with no
hardware attached.

---

## 5. Known build-system issues (carried over from v3.5)

These are documented here so they can be cleaned up incrementally. See the
modernization notes in [the changelog](../CHANGELOG.md).

1. **Hardcoded toolchain paths.** The v3.5 makefile referenced absolute WSL
   paths (`/mnt/c/WATCOM/BINNT/wcc.exe`). A portable makefile should use
   `$(WATCOM)` / `PATH` instead.
2. **Duplicated object list.** The link line repeats the object list instead of
   referencing `$(OBJS)`, so the two can silently drift when a module is added.
3. **Phantom assembly rules.** The makefile contained a build rule and `help`
   text for `trig287.asm` and `fft_286.asm`, which **do not exist** — only
   `trig287_simple.asm` ships. These are dead/misleading entries.
4. **Vendored header.** `ieeeio_w.c` needs `IEEEIO.H`; v3.5 relied on picking it
   up from the v3.0 folder. The modern `src/` tree vendors `IEEEIO.H` directly
   so the build is self-contained.
5. **Dormant assembly.** `cga_asm.asm`, `mem286.asm`, `fixed286.asm`, and
   `trig287_simple.asm` are assembled and linked but **not currently called**
   from C (only `extern` declarations exist). They are optimization scaffolding;
   verify the call wiring before relying on — or pruning — them.
