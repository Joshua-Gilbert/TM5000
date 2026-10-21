# GRIDGPIB — resident GPIB driver for the GRiDCase 1520

A small replacement for IOtech Driver488, written for the Grid-OS project.
`TM5000G.EXE` (the `tm5000g.exe` build of `src/`) talks to the bus through it.

| | GRIDGPIB 1.1 | Driver488 rev 2.6 |
|---|---|---|
| Resident size | **1,851 bytes** | ≈ 43 KB |
| Serial poll | **0.4–0.8 ms** | 87 ms |
| `ID?` query (Tek, typical) | **10–23 ms** | 89–101 ms |
| Whole read-only test set (bus time) | **8.5 s** | 34.8 s |

Measured on a GRiDCase 1520 with a TM5006A mainframe (DM5120, DM5010, DC5009,
FG5010, PS5004, PS5010, SI5020), a Tek 11403 and an HP 8753D. Direct chip access
from a test program gives the same times as GRIDGPIB; Driver488 costs a fixed
~87 ms per call.

## Use

```
GRIDGPIB          install on the first free vector INT 60h-66h, IFC + REN
GRIDGPIB /I:nn    install on INT nn (hex, 60-66; never 67h = EMS)
GRIDGPIB /U       unload (all devices local, REN off, memory freed)
GRIDGPIB /F       install even when Driver488 is loaded (not advised)
TM5000G           then run TM5000 on it
```

Load GRIDGPIB **instead of** DRVR488. The API (INT nn, AH = function) is
documented at the top of `GRIDGPIB.ASM`; clients find the driver by the
`GRIDGPIB` signature at the vector's entry + 2. `src/gridgpib.c/.h` is the C
interface used by TM5000G.

Hardware: NEC µPD7210 at base 02E1h, register n at 02E1h + n×400h (IOtech
Personal488 / NI PC2A layout), Grid = system controller at address 21.

## Build

```
nasm -f bin -o GRIDGPIB.COM GRIDGPIB.ASM
nasm -f bin -o GGTEST.COM   GGTEST.ASM
```

`GGTEST.COM` runs the automated read-only test set (scan, `ID?`/`*IDN?`, `SET?`,
meter readings, serial polls, all timed with the PIT) through the driver and
logs to `A:\GGTEST.TXT`.
