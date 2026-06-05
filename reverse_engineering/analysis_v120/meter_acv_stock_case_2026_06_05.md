# Meter DMM Stock FSM Check, 2026-06-05

This note records the stock-firmware check that drove the local DMM display FSM
port. It keeps disassembly evidence here, not in firmware comments.

## Input Binary

Downloaded bundle:
`https://cdn.shopify.com/s/files/1/0694/8310/2426/files/2C53T_Firmware_V1.2.0.zip?v=1760665494`

Extracted application:
`APP_2C53T_V1.2.0_251015.bin`

- size: `751232` bytes
- sha256: `a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760`

`analyzeHeadless` was not installed on the bench host, so the pass used the
existing `full_decompile.c` / annotated analysis plus fresh
`arm-none-eabi-objdump` disassembly from this downloaded binary.

## ACV Decimal Evidence

The stock DMM mode handler dispatches through a Thumb `TBB` table at
`0x080371c4`. The table bytes at `0x080371c8` are:

```text
15 30 3b 4b 59 68 04 04
```

For `TBB`, the branch target is `pc + 2 * table[submode]`. Submode `1`
(AC Voltage) therefore reaches `0x08037228`.

The ACV case at `0x08037228` reads byte `frame[7]`, tests bit 0, and writes the
decimal/range state at the nearby meter state offset:

```text
08037228: ldrb r0, [r6, #7]
0803722a: lsls r0, r0, #31
0803722c: bmi.n 0x080372bc
0803722e: movs r0, #1
08037230: strb r0, [r7, #0xf37]
...
080372bc: strb r4, [r7, #0xf37]
080372be: movs r0, #1
```

This agrees with the existing annotated analysis: ACV decimal/probe selection
is tied to `frame[7]` bit 0, not to a broad `frame[6]` variant list.

Ported local mapping:

```text
frame[7] bit0 set   -> stock ACV format index 0 -> local X.XXX V
frame[7] bit0 clear -> stock ACV format index 1 -> local XXX.X V
```

The local renderer stores the decimal insertion position directly, while stock
stores a format-template index. The mapping above preserves the observed low
voltage `7.005 V` and mains `227.6 V` shapes without using frame `[10..11]` for
range selection.

## What This Does Not Prove

The live custom-firmware mains captures show:

```text
raw=2276..2278, submode=1, frame[7]=00, frame[8]=02, frame[9]=00,
extra=0031/0032
```

Treating `extra=0031/0032` as about `49/50 Hz` is empirical live evidence from
the bench setup. This stock pass did not prove that ACV uses `[10..11]` as a
companion frequency field. Stock frequency-unit formatter paths exist elsewhere
in the DMM FSM, but they do not by themselves prove an ACV companion-Hz field.

The custom firmware may expose `[10..11]` as a narrow auxiliary frequency hint,
but it must not use it to decide the ACV voltage decimal/range.

## Local Port

`firmware/src/drivers/meter_data.c` now has a single stock-style DMM display
state layer. It tracks stock mode, variant, format, DC substate, display
command, unit index, and composite format index, then translates those into the
local `decimal_pos` and `unit_suffix` fields used by the UI.

The local UI has eleven manual submodes while the stock display formatter has
eight state-machine slots. The port therefore has an explicit UI-submode to
stock-mode mapping for voltage, current, resistance, continuity, diode, and the
extended/frequency-like slot used by capacitance and temperature. Translation
overrides keep user-facing units sane for local split modes such as large
current, continuity, diode, capacitance, and temperature.

Live ACV smoke after the port:

```text
raw=2275..2278 dp=3 unit=V disp=227.5..227.8
frame[7]=00 frame[8]=02 frame[9]=00 extra=0031 aux_freq_i10=490
```

The voltage display is now driven by the stock display FSM translation. The
waveform source remains unresolved: the candidate SPI3 meter sample stream still
advances at about 1 kHz but stays flat `0xFF`.
