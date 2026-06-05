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
`0x080371c4`. The ACV format selector guard in
`scripts/test_stock_meter_literals.py` now checks the literal table bytes at
`0x080371c8`:

```text
15 30 3b 4b 59 68 04 04
```

For `TBB`, the branch target is `pc + 2 * table[submode]`. Submode `1`
(AC Voltage) therefore reaches `0x08037228`.

The ACV case at `0x08037228` reads byte `frame[7]`, tests bit 0, and writes the
decimal/range state at the nearby meter state offset. The guarded literal bytes
are:

```text
08037228: f0 79 42 f6 7c 5b c0 07 c2 f2 00 0b 42 d1 01 20
08037238: 87 f8 37 0f 8c e0 97 f8 36 1f f0 79 01 29 3d d1
080372bc: 87 f8 37 4f 01 20 49 e0
```

Corresponding stock disassembly:

```text
08037228: ldrb r0, [r6, #7]
0803722a: movw fp, #0x2d7c
0803722e: lsls r0, r0, #31
08037230: movt fp, #0x2000
08037234: bne 0x080372bc
08037236: movs r0, #1
08037238: strb.w r0, [r7, #0xf37]
...
080372bc: strb.w r4, [r7, #0xf37]
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

This is not AC evidence. It proves only that stock ACV selects one of two
format/decimal shapes from `frame[7] bit 0`. Local confidence for ACV must still
come from independent AC/frequency metadata and must reject a DC input in ACV
instead of treating this format bit as proof of an alternating signal.

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

## Voltage Range-Hint Evidence

The annotated stock RX analysis also exposes ordered range/exponent hint bits in
the 12-byte meter frame. Raw `full_decompile.c` proves the later
`DAT_2000102f` / `DAT_20001030` formatter, but current raw decompile evidence
does not prove these bits are literally written into `DAT_2000102c`; treat them
as a voltage range hint, not as a renamed RAM variable. The annotated decompile
records this priority:

| Frame bit | Stock class | Decimal multiplier | Evidence |
|---|---:|---:|---|
| none set | 0 | `1.0` | stock priority fallback |
| `frame[5] & 0x10` | 1 | `0.1` | stock priority branch |
| `frame[4] & 0x10` | 2 | `0.01 V/count` | live/custom 32 V capture displays `31.96 V` |
| `frame[3] & 0x10` | 3 | `0.001 V/count` | live/custom 5 V capture displays `4.994 V` |
| `frame[8] & 0x80` | 4 | `0.0001` | live/custom 1.5 V capture uses `frame[2].3` to extend `4977` into `14977`, then class 4 renders `1.4977 V` |

The 2026-06-05 32 V failure frame was:

```text
5A A5 86 0F DA EF 07 00 02 00 03 FF
digits=3196, frame[4].4=1, extra=03FF
```

The prior local decoder forced ordinary DCV to decimal position `1`, rendering
`3.196 V`. The corrected local port uses the stock-analysis range hint (`2`) and renders
`31.96 V`. The `[10..11]` value is intentionally ignored for voltage exponent
selection; `03FF` is not stock evidence for a voltage range.

The 2026-06-05 low-DCV failure frame was:

```text
5A A5 4E CE 8F 8A 0A 00 82 00 01 7F
digits=4977, frame[8].7=1, extra=017F
```

The previous local decoder treated hint `4` as decimal position `0`, rendering
about `4977 V` for a 1.5 V cell. The stock-only correction is the `frame[2].3`
raw extension: `4977` becomes `14977`, then `frame[8].7` selects class `4` and
renders `1.4977 V`. The local port does not infer a multiplier from the numeric
digits, and it does not use a one-point low-voltage coefficient.

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
