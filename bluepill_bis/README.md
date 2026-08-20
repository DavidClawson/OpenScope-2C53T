# bluepill_bis — the config-transport bisect rig

A Blue Pill (STM32F103C8) standing in for the 2C53T's MCU, driving a Gowin
FPGA's SSPI config port with **both** of this project's config transports —
the hardware-SPI sequence that failed for two months and the bit-bang
transplant that broke the wall. Purpose: run BIS-1/BIS-2/BIS-4 from
`docs/dev_plan_2026-08-21.md` §6 on a dev-board target with zero risk to
bench unit #1.

Both sequences are **ports of the bench-proven code in
`firmware/src/drivers/fpga.c`** (`fpga_spi3_config_sequence` /
`fpga_bitbang_config_sequence`) with the fidelity details preserved: same
pins via the F103's SPI1 remap (PB3/4/5 + PB6 soft CS — the scope's exact
pins), stock's SWJ posture (`MAPR=0x02000000`), MISO pull-up default (Exp E),
mode-3 MSB-first, reads at /256, each transport's native read framing.
If `fpga.c`'s sequences change, re-port — this rig is only evidence while it
matches them.

## Why the F103 is a fair proxy

The AT32F403A is STM32F1-family compatible; SPI/GPIO/AFIO register maps are
the same. Sequence-level results (BIS-1/BIS-2) transfer directly. Waveform
results (BIS-3) are "STM32F1-class" evidence only — most Blue Pills carry
F103 clones, and neither clone nor genuine is the AT32 — so a positive BIS-3
finding must be confirmed at the 2C53T's own pins.

## Wiring (Tang Nano 20K, v1.3 schematic — verify by continuity first)

| Blue Pill | signal        | Tang Nano 20K              |
|-----------|---------------|----------------------------|
| PB3       | SCLK          | header pin 20 (FPGA pin 52)|
| PB5       | MOSI → SI/DIN | header pin 8  (FPGA pin 54)|
| PB4       | MISO ← SO     | header pin 7  (FPGA pin 56)|
| PB6       | CS → SSPI_CS_N| header pin 11 (FPGA pin 55)|
| GND       | GND           | GND                        |

Both boards are 3.3 V logic; power each from its own USB, common ground.
Keep the 20K's HDMI unplugged (SCLK/DOUT share EDID-labelled nets). MODE
straps: the 20K's MODE0/MODE1 are its two user buttons — look up the GW2A
SSPI-slave MODE value in UG290 on arrival. Tang Nano 9K wiring: map the same
four signals from its schematic when it arrives (2026-08-24).

UART console: PA9 (TX) / PA10 (RX) → any 3.3 V USB-UART (the FT232H on the
bench does UART) at 115200 8N1.

## Build / flash / run

```
make            # arm-none-eabi build, ~3.4 KB
make flash      # ST-Link/V2 via OpenOCD (no RDP hazard on a Blue Pill)
python3 host.py --port /dev/ttyUSB0 i         # anchor: IDCODE probe first
python3 host.py --port /dev/ttyUSB0 b         # bit-bang V0.4 entry attempt
python3 host.py --port /dev/ttyUSB0 2 --fs top.fs   # BIS-2 + payload upload
```

Legs: `i`/`I` IDCODE probe (hw/bb) · `a` hardware-SPI base (05/12/15) ·
`1` BIS-1 (hw + V0.4 prelude reads) · `2` BIS-2 (hw, no 0x05) ·
`b` bit-bang V0.4 · `B` bit-bang **with** 0x05 (cross-check).
Knobs: `e <idcode>` expected IDCODE per target (default `0120681B`; the
dev boards differ — read it with `i` first), `d <cmd_br> <upload_br>`,
`m <0|1>` MISO float/pull-up.

Payloads: `--fs` accepts a Gowin ASCII `.fs` (packed to bytes MSB-first) or
a raw binary. Entry legs need **no payload** — the wall signature
(`ST15 ... EDIT:0`) is measurable payload-free; full-config runs
(`DONE:1` + the Exp L closed-port check on `ID15`) stream one over UART
(~10 s per 115 KB at 115200).

## Method

Per project law: **anchor every session with an IDCODE read first** (`i`).
A stable wrong number is indistinguishable from a right one — three separate
instrument bugs each survived for weeks because no measurement had a known
correct answer.
