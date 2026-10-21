# Building TM5000

TM5000 is a 16-bit real-mode MS-DOS application built with the **OpenWatcom
C/C++** toolchain. It runs on period hardware or DOSBox / 86Box, not on a modern
OS directly.

This guide covers the current tree, [`src/`](../src) (v3.7). Versions under
[`archive/`](../archive) are reference only.

---

## 1. Prerequisites

| Component | Requirement |
|-----------|-------------|
| Compiler  | OpenWatcom C/C++ **2.0** (`wcc`, the **16-bit** compiler — *not* `wcc386`). Only 2.0 is verified; 1.9 is untested with `-zm` / `OPTION ELIMINATE` |
| Assembler | OpenWatcom `wasm` (for the `.asm` optimization modules) |
| Linker    | OpenWatcom `wlink` |
| Build tool| `wmake` (native) **or** GNU `make` calling the Watcom `.exe`s |
| Run target| MS-DOS 3.3+ on a 286/287, CGA graphics, a Personal488 (IOtech) GPIB card (driven by `DRVR488`, or by GRIDGPIB for `tm5000g.exe`) — or [DOSBox](https://www.dosbox.com/) for testing without hardware |

Install OpenWatcom and run `owsetenv.bat` / `owsetenv.sh`: the compiler needs
`WATCOM` and `INCLUDE` to find its headers and libraries.

The makefile ignores `PATH` and calls the tools through `WATCOM_BIN`, which
defaults to `/mnt/c/WATCOM/BINNT` (a WSL build running the Windows `.exe`s).
Override it for any other install, e.g. `wmake WATCOM_BIN=C:/WATCOM/binnt`.

---

## 2. Build flags

The `makefile` compiles with:

```
CFLAGS = -ml -2 -bt=dos -os -d0 -zc -zt100 -zm
```

and links with `wlink option eliminate system dos ...`.

| Flag | Meaning | Why it matters |
|------|---------|----------------|
| `-ml` | **Large** memory model | The data set exceeds 64 KB; far pointers are required for buffers |
| `-2`  | 80286 instruction set | Target CPU is the 286 |
| `-bt=dos` | Build target = DOS | Real-mode 16-bit output |
| `-os` | Optimize for **size** | Must fit conventional 640 KB memory |
| `-d0` | No debug info | Smaller binary (use `-d2` for a debug build) |
| `-zc` | Place `const` data in the **code** segment | Frees scarce `DGROUP` data space |
| `-zt100` | Data threshold 100 | Items ≥100 bytes go *far* automatically, keeping `DGROUP` under 64 KB |
| `-zm` | One segment per function | Lets `wlink option eliminate` drop unreferenced functions from the image |

Assembly modules build with `-2` only: they declare `.MODEL SMALL` themselves,
and OpenWatcom 2.0's `wasm` rejects a forced `-ml`. `trig287_simple.asm` adds
`-fp2` (287 FPU instructions).

> Changing the memory model or data threshold breaks the 64 KB-segment layout
> the code depends on.

---

## 3. Building

From inside `src/`:

```sh
wmake          # native OpenWatcom make
# or
make           # GNU make (e.g. under WSL) invoking the Watcom tools
```

Output: `tm5000.exe` (Driver488, 316,388 bytes) and `tm5000g.exe` (GRIDGPIB,
316,102 bytes), ~310 KB each. Prebuilt copies of both are committed in `src/`.

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

- `wmake clean` — remove `*.obj`, `*.err`, `*.map` and both executables
- `wmake help`  — print build help

The `wcl` target is **incomplete**: it omits `math_enhanced.c`,
`ui_math_menus.c`, `config_profiles.c`, `export_enhanced.c`, and all assembly
objects, and builds only the Driver488 program, so it produces a partial binary.
Use the default `all` target.

---

## 4. Running

On real hardware or DOSBox:

1. Load the GPIB driver: `DRVR488.EXE` for `TM5000.EXE`, or `GRIDGPIB.COM`
   (instead of `DRVR488`) for `TM5000G.EXE`.
2. Run `TM5000.EXE` or `TM5000G.EXE` from the DOS prompt.

Without GPIB hardware, the program still starts under DOSBox and the menus work;
instrument operations time out.

---

## 5. Build-system caveats

1. **Duplicated object lists.** Each link line repeats its object list instead of
   using `$(OBJS)` / `$(OBJS_G)`, so the lists can drift when a module is added.
2. **Vendored header.** `ieeeio_w.c` needs `ieeeio.h`, which is vendored in `src/`
   so the build is self-contained.
3. **Dormant assembly.** `cga_asm.asm`, `mem286.asm`, `fixed286.asm`, and
   `trig287_simple.asm` are assembled and linked but nothing in C calls them (only
   `extern` declarations exist). Check the call wiring before relying on, or
   pruning, them.
