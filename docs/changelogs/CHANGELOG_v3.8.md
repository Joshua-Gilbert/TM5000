# TM5000 v3.8 — every instrument checked against its manual (2026-10-20 – 10-24)

v3.8 rechecks each instrument driver against Tektronix's own documentation and fixes
the October 2026 audit findings. The summary of every fix is in
[CHANGELOG.md](../../CHANGELOG.md#38--2026-10-24--every-instrument-checked-against-its-manual-audit-fixes).
This file records how the drivers were checked, what each one now covers, and what
still has to be confirmed on the bench. None of v3.8 has been run on hardware yet.

## Sources

| Instrument | Document |
|---|---|
| DM5120 | Instruction manual 070-7240-00, and the GPIB parser table in the FV1.0 firmware image (`5120-800_v1.0.bin`, ID `TEK/DM5120,V81.1,FV1.0`) |
| DC5009 | Instruction manual 070-3888-00 |
| DC5010 | Instruction manual 070-3897-02 |
| DM5010 | Instruction manual 070-2994-01 |
| FG5010 | Instruction manual 070-3467-01 |
| PS5004 | Instruction manual 070-4442-00 |
| PS5010 | Instruction manual 070-3391-00, reference guide 070-3402-00 |

All are on TekWiki (w140.com/tekwiki). Most are scans without a text layer: they were
OCR'd, and every command, reply format and table relied on was read from the page image.

## Method

Every string the program sends and every reply it parses was compared with the manual.
All seven instruments use Tek Codes & Formats (`HEADER arg;`, replies with a header), not
SCPI. The bug classes found again and again:

- SCPI or invented commands (`*IDN?`, `CONF:`, `SYST:ERR?`, `VAL?`, `GATE`, `BUFCLR`, `X`)
  and wrong spellings (`DIGITS`); the DM5120 accepts no abbreviations at all.
- Replies parsed without skipping their header, so `ERR?`, `REG?` and `BUFCNT?` read 0.
- `INIT` used as "start a reading": on the meters and counters it restores the power-on
  settings.
- Intervals and timeouts shorter than the instrument needs (DM5120 error 551).
- Menus that stored a choice in the wrong field, edited features the instrument lacks,
  or could not be reached.
- Failed reads stored as 0; readings labelled with the wrong unit.
- Comm tests that switched supply or generator outputs on without asking.

Each finding was confirmed by a second, independent check of the page images and the
code before it was fixed, and each fix was reviewed again against the manual.

## Command coverage

Every command and query in each manual's command list is either used by the driver or
reachable from that instrument's menu.

| Instrument | Commands | Excluded |
|---|---|---|
| DC5009 / DC5010 | 63 | none |
| DM5010 | 52 | none (the LLO interface message is not offered: GRIDGPIB's Driver488 emulation would release REN on the whole bus) |
| FG5010 | 61 | none |
| PS5004 | 38 | `LLSET`, `FVOLTS` binary blocks: neither driver can send the bytes, and a bad block could switch a live supply |
| PS5010 | 49 | none |
| DM5120 | 67 (firmware table: 33 commands, 34 queries) | `CAL` (rewrites the stored calibration) |

Rules every driver now follows: outputs change only on an explicit menu action, with a
Y/N prompt before anything turns on; commands that write EEPROM or reset the instrument
ask first; setting sequences run with RQS off and then list each error with the manual's
text for its code.

## Compatibility

- `PROFILES.DAT`: no config struct changed size or field offsets, so `PROFILE_VERSION` is
  unchanged. Fields that were renamed reuse slots that older versions left at 0 (or 1
  where the meaning is inverted), and older values are converted when a profile or
  settings file loads.
- `.tm5` and `.cfg` formats are unchanged; the loaders now bound every field they read.
- `tm5000.exe` 382,760 bytes, `tm5000g.exe` 383,050 bytes (v3.7: about 316 KB). DGROUP
  is 6.4 KB of 64 KB.

## Bench checks

First:
- **Memory:** the main menu's "Memory:" readout with DRVR488 or GRIDGPIB resident. The
  executables are about 65 KB larger than v3.7's; the monitor's sample pool shrinks when memory is short.
- **Outputs:** the FG5010 output test (`INIT`, 1 kHz 2 V p-p, ends with `OUT OFF`
  confirmed); PS5004 and PS5010 Apply prompts; PS5004 Apply dropping the output before a
  new voltage on a live load.

Per instrument:
- **DM5120:** no error 551 at 5½/6½ digits, OHMS and OHMSCOMP with the buffer on;
  `EVENT?` codes arrive as numbers; Single Measurement keeps the applied setup (no
  `INIT`); `TEXT ""` clears the display.
- **DC5009 / DC5010:** single shot with `STOP;RES` and with `DT TRIG` + GET; whether
  `RDY?` ever reports 1 while TOT or TMAN runs; no write timeout on GRIDGPIB while the
  counter holds NRFD after `AUTO`/`INIT` (up to 2.5 s).
- **DM5010:** Driver488 passes `FUNCT?;CALC?;SEND` as one message; long `AVE` reads raise
  and restore the Driver488 timeout; over-range shows OVERRANGE.
- **FG5010:** how both drivers return the one-byte 0xFF "no output" reply; `LLSET?`
  blocks under Driver488, which may stop at an LF byte in the data.
- **PS5004:** whether `SEND` puts a space before the exponent (`50.0 E-3`); the
  power-on SRQ is released at auto-detect.
- **PS5010:** `REG?` decoding on a live supply; standard vs high-power compartment
  current limits; `TEST` within GRIDGPIB's 2 s read timeout.
