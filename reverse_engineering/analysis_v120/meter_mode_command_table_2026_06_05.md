# Meter Mode Raw Command Table, 2026-06-05

This note records the local extraction of the eight-byte stock meter-mode raw
command table referenced by the `0x080BB3FC` notes.

The archived V1.2.0 app binary is loaded at app-slot VMA `0x08004000`, while the
current decompile notes use addresses as if the app started at `0x08000000`.
For literal data, `raw_app_base_offset_2026_04_08.md` says to inspect
`runtime_literal - 0x4000` in the current project/raw image. Applying that to
`0x080BB3FC` gives raw address `0x080B43FC`, file offset `0x000B43FC`.

Extraction from `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`:

```text
14 0c 17 0b 0a 12 11 10
```

These bytes are the low byte of raw UART words of the form `0x0500 | table[i]`.
They are not display/update selector bytes from queue `0x20002D6C`.
`scripts/test_stock_meter_literals.py` verifies these exact bytes at the
app-image address `0x080B43FC` (file offset `0x000B43FC`) while documenting the
runtime/app-slot literal as `0x080BB3FC`, so local selector policy fails the
software gate if the stock table evidence drifts.

Local port:

| Stock meter mode | Low byte | Raw word |
|---:|---:|---:|
| 0 | `0x14` | `0x0514` |
| 1 | `0x0C` | `0x050C` |
| 2 | `0x17` | `0x0517` |
| 3 | `0x0B` | `0x050B` |
| 4 | `0x0A` | `0x050A` |
| 5 | `0x12` | `0x0512` |
| 6 | `0x11` | `0x0511` |
| 7 | `0x10` | `0x0510` |

## Local Porting Map

The open firmware exposes eleven UI submodes, while the stock table has eight
stock slots. The current port therefore maps local UI state onto the recovered
stock slots below. This is a porting map, not proof that stock has eleven
separate modes.

| Local UI submode | Meaning | Stock slot | Selector | Extra apply word | Parser frame family |
|---:|---|---:|---:|---:|---|
| 0 | DC voltage | 0 | `0x0514` | none | voltage |
| 1 | AC voltage | 1 | `0x050C` | `0x050D` | voltage |
| 2 | DC current, small range | 2 | `0x0517` | `0x050E` | current |
| 3 | DC current, A range | 2 | `0x0517` | `0x050E` | current |
| 4 | AC current, small range | 3 | `0x050B` | none | current |
| 5 | AC current, A range | 3 | `0x050B` | none | current |
| 6 | resistance | 4 | `0x050A` | none | resistance |
| 7 | continuity | 6 | `0x0511` | `0x0516` | continuity |
| 8 | diode | 7 | `0x0510` | `0x0515` | diode |
| 9 | capacitance | 5 | `0x0512` | none | extended |
| 10 | temperature | 5 | `0x0512` | none | extended |

## Per-Submode Evidence Matrix

The table below separates stock-disassembly evidence from local policy. `High`
means the selector/display path is directly recovered from stock code or literal
tables; `Medium` means the stock slot is recovered but the local eleven-submode
split projects onto fewer stock slots; `Low` marks behavior that remains a local
conservative transition policy until stock runtime state writes or bench traces
prove the exact rule.

| Local submode | Stock selector evidence | Port C/E mux evidence | Port A/B mux evidence | Transition evidence | Confidence / gap |
|---:|---|---|---|---|---|
| 0 DCV | stock slot `0`, selector `0x0514`; stock case 0/DCV formatter in `full_decompile.c` around `0x080028E0` | projects stock slot `0` into `gpio_mux_portc_porte`; function recovered at `0x080018A4` | projects stock slot `0` into `gpio_mux_porta_portb`; function recovered at `0x08001A58` and writes PA15/PA10/PB10/PB11 | local reset + 20 ms settle + selector + 2-frame discard; stock proves command pacing/filtering, not exact constants | High selector; Medium mux projection; Low exact settle/discard proof |
| 1 ACV | stock slot `1`, selector `0x050C`, apply `0x050D`; ACV case at `0x08037228` reads `frame[7].0` | projects stock slot `1` | projects stock slot `1` | same local transition policy plus apply word | High selector/formatter; Medium mux; Low exact settle/discard proof |
| 2 DC mA | stock slot `2`, selector `0x0517`, apply `0x050E`; stock current formatter evidence distinguishes DCA unit indices | projects stock slot `2` | projects stock slot `2` | same local transition policy | Medium: stock current slot recovered, local small-current split not separately selector-proven |
| 3 DC A | same stock slot `2`, selector `0x0517`, apply `0x050E` | projects stock slot `2` | projects stock slot `2` | same local transition policy | Medium/Low: local A-range policy over shared stock slot; runtime range writer still missing |
| 4 AC mA | stock slot `3`, selector `0x050B`; stock case 3 writes ACA display/unit state | projects stock slot `3` | projects stock slot `3` | same local transition policy | Medium: ACA-like slot recovered |
| 5 AC A | same stock slot `3`, selector `0x050B` | projects stock slot `3` | projects stock slot `3` | same local transition policy | Low/Medium: AC A is local policy until stock A-range ACA evidence appears |
| 6 resistance | stock slot `4`, selector `0x050A`; stock case 4 formatter/unit state | projects stock slot `4` | projects stock slot `4` | same local transition policy | High selector; Medium mux |
| 7 continuity | stock slot `6`, selector `0x0511`, apply `0x0516`; continuity segment marker path is parser-visible | projects stock slot `6` | projects stock slot `6` | same local transition policy plus apply word | High selector; Medium mux |
| 8 diode | stock slot `7`, selector `0x0510`, apply `0x0515` | projects stock slot `7` | projects stock slot `7` | same local transition policy plus apply word | High selector; Medium mux |
| 9 capacitance | stock slot `5`, selector `0x0512`; stock case 5 has capacitance-like `digit_count + 2` formatting | projects stock slot `5` | projects stock slot `5` | same local transition policy | Medium: extended slot recovered; cap/temp split not separately selector-proven |
| 10 temperature | same stock slot `5`, selector `0x0512`; stock mode-5 path has Fahrenheit conversion clue and adjacent `32.0f` literal | projects stock slot `5` | projects stock slot `5` | same local transition policy | Medium/Low: conversion clue exists, separate selector not proven |

## Current Range Evidence Boundary

Stock selector slots 2 and 3 are the only current slots recovered from the
eight-byte command table:

| Stock slot | Selector | Stock formatter evidence | Local use |
|---:|---:|---|---|
| 2 | `0x0517` | `full_decompile.c` case 2 writes display unit state `4` for one DCA range and `3` for another, keyed by `DAT_2000102e`. `meter_fsm_deep_dive.md` maps those unit indices to inferred mA and A unit strings. | local DC current small range and DC A range |
| 3 | `0x050B` | `full_decompile.c` case 3 writes display unit state `5`; `meter_fsm_deep_dive.md` maps that unit index to an inferred ACA mA unit string. | local AC current small range; AC A is only local policy until proven |

No inspected stock path proves a separate uA selector, and no inspected AC path
proves an A-range ACA formatter. The stock evidence so far distinguishes DC
current ranges through frame/display unit state, not through additional
command-table slots. Until the runtime writer for the stock range state is
recovered or bench-proven, local uA and AC A labels are parser/UI policy on top
of the two recovered current slots.

## Voltage Frame-Family Marker Boundary

Low-DCV live and synthetic failure frames use both `frame[8]=0x80` and
`frame[8]=0x82` forms. In both, bit 7 is the stock range/status input that
selects stock decimal class `4`; in the `0x82` form the low seven bits also
carry the common DCV/voltage marker `0x02`. Therefore both `0x80` and `0x82`
with `frame[9]=0x00` must be treated as voltage-family payloads for wrong-mode
rejection unless a later stock xref proves another family owns that bit.

This is a frame-metadata rule, not a value-recognition rule. The decoder must not
infer voltage/current/passive family from the BCD count looking plausible.

## Extended Slot 5 Evidence Boundary

Stock slot 5 is solidly recovered as selector `0x0512`. The meaning of the
local capacitance and temperature split is narrower:

- `fpga_task_annotated.c` records the stock result-formatting switch case 5 as
  `digit_count + 2` for capacitance-like formatting.
- `meter_math_pipeline_annotated.c` contains a mode-5 conversion path using
  `value = value * 9 / 5 + 32` when the flag at `+0xF39` is set, with a
  Fahrenheit `32.0f` literal nearby.
- The local port maps both capacitance and temperature to stock slot 5 and
  parses both as the extended frame family.

That is evidence for a shared extended slot, not proof that stock exposes
separate capacitance and temperature selector modes matching the open
firmware's eleven UI submodes.

## Transition Timing Evidence Boundary

Stock evidence supports command pacing and frame filtering:

- `fpga_task_annotated.c` shows the TX interrupt enabled, followed by a 10-tick
  delay before accepting the next command.
- `full_decompile.c` and `usart2_isr_state_machine.md` show USART2 framing that
  accepts `0x5A/0xA5` data frames, validates `0xAA/0x55` echo frames, and drops
  invalid echo/data sequences.
- `meter_math_pipeline_annotated.c` marks `{any, 0x13, 0x14, 1..3}` as pending
  auto-range/mode-transition data.

No inspected stock path proves a fixed "discard exactly N frames" or "settle
exactly 20 ms" window after every mode switch. The open firmware's current
two-frame discard plus 20 ms settle is a conservative local transition policy
that should be replaced only when a stock path or repeatable bench capture
proves the exact rule.

## GPIO Mux Evidence Boundary

Stock master init calls the two GPIO mux functions from saved meter state:

```text
0x08025544: ldrb r0, [r4, #2]  ; ms[0x02]
0x08025546: bl   0x080018a4    ; gpio_mux_portc_porte
0x0802554a: ldrb r0, [r4, #3]  ; ms[0x03]
0x0802554c: bl   0x08001a58    ; gpio_mux_porta_portb
```

The same pair repeats after the later probe/attenuation restore block at
`0x0802723e..0x0802724a`. `gpio_mux_portc_porte` controls PC12 and PE4/PE5/PE6;
`gpio_mux_porta_portb` controls PA15, PA10, PB10, and PB11: the decompile at
`0x08001a58..0x08001bba` writes GPIOA bit `0x8000`, GPIOA bit `0x400`, GPIOB bit
`0x400`, and GPIOB bit `0x800` through the BOP/BCR registers. The current
open-firmware transition plan now represents these as separate
`portc_porte_mux` and `porta_portb_mux` fields instead of treating the stock
function/range selectors as one generic mux.

The unresolved part is the live/runtime writer for `ms[0x03]` during local
small-current versus A-range operation. Until that is recovered or bench-proven,
local submodes 2/3 and 4/5 share the same recovered stock current slot and are
split only by parser/UI range state. Treat current readings that still look like
voltage payloads as a frontend activation failure, not a decimal decoder issue.

### Mux Writer Xref Audit, 2026-06-06

The text decompile/xref pass found no DMM-specific runtime writer that maps the
eight recovered DMM selector words directly to unique `ms[0x02]` and `ms[0x03]`
bytes. The evidence splits like this:

| Evidence | Stock offsets / files | Classification |
|---|---|---|
| `gpio_mux_portc_porte` body | `FUN_080018a4`, `full_decompile.c:2206..2295`; 10-way `param_1` switch writing GPIOC/E plus DAC calibration tables | hardware writer, stock-proven |
| `gpio_mux_porta_portb` body | `FUN_08001a58`, `full_decompile.c:2300..2365`; 10-way `param_1` switch writing GPIOA/B pins including PB11 | hardware writer, stock-proven |
| saved-state restore/apply | `0x08025544..0x0802554c` and `0x0802723e..0x0802724a` load saved `ms[0x02]`/`ms[0x03]` then call both mux writers | DMM-relevant boot/saved-state evidence |
| direct decompile callers | `function_names.md` lists `FUN_08001c60` as `siggen_configure` and `FUN_08019e98` as `scope_main_fsm`; `function_map_complete.txt` lists only callers `08001c60,08019e98` for both mux writers | scope/siggen runtime evidence, not DMM selector proof |
| clipping auto-range write | `full_decompile.c:2564..2574` increments `(&DAT_200000fa)[uVar20]`, then calls `FUN_080018a4(DAT_200000fa)` for channel 0 or `FUN_08001a58(DAT_200000fb)` for channel 1 and queues command `4` | scope/siggen auto-range path inside `FUN_08001c60`; not DMM |
| scope main auto-range write | `full_decompile.c:6880..6999` scans sample buffers, enters range selection, then reuses `DAT_200000fa/DAT_200000fb` for DAC/calibration recompute | oscilloscope acquisition path; not DMM |
| explicit scope-submode mux calls | `full_decompile.c:7564..7565` and `7988..7989` call both mux writers with `DAT_20000128 & 0xf`; `scope_main_fsm_annotated.c` names `DAT_20000128`/state `+0x30` as scope sub-mode | scope runtime reconfiguration, not DMM |
| DAC1 writes | `FUN_080018a4` at `0x080018A4..0x08001A52` and inline recomputes at `full_decompile.c:2603..2624`, `6960..7020`, `7771..7990` write `0x40007408` from scope calibration tables | scope trigger/comparator threshold; not DMM calibration |
| waveform calibration/render use | `full_decompile.c:8611..8624`, `9840..9971` index `DAT_080465cc` and calibration deltas through current and saved `DAT_200000fa/DAT_200000fb` | scope display/calibration path, not DMM selector proof |

This means the open firmware can legitimately project the recovered stock DMM
slots into the two mux bytes, but it must keep that projection marked as a local
policy until either Ghidra data-xrefs recover the DMM saved-state writers or a
stock-runtime trace records `0x200000fa/0x200000fb` while switching DMM modes.
Do not treat scope auto-range writes as evidence for DMM current/voltage range
decoding, and do not repair a surprising DMM reading by adding a numeric
coefficient on top of those scope paths.

The same boundary applies to DAC1 (`0x40007408`). Stock DAC1 writes are real
and table-backed, but current xrefs tie them to the scope trigger/comparator
path. They are not a recovered meter reference or low-DCV correction source.
