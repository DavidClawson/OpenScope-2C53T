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

The same guard now also verifies the selector consumer xrefs at
`0x080042E2..0x0800430A` and `0x080048BA..0x080048E2`.  These two stock code
sequences load the runtime `0x080BB3FC` table, read one byte indexed by
`DAT_20001025`, add `0x0500`, and store the raw UART halfword to `0x20002D54`
before queueing through the `0x20002D74` raw-word path.  This is selector-table
consumer evidence only; it still does not recover the analog mux bytes
`ms[0x02]`/`ms[0x03]` or any physical correction coefficient.

The stock raw-word queue consumer is now binary-guarded as well. This
`dvom_TX raw-word consumer guard` proves that the `dvom_TX` task at
`0x080373F4` uses queue handle `0x20002D74`, blocks in `xQueueReceive`, then
formats the received halfword into the USART2 TX buffer:

```text
0x080373F4: task prologue, sets r6 = 0x20002D74
0x08037420: ldr r0, [r6, #0]
0x08037428: bl 0x0803B1D8              ; xQueueReceive(queue, &halfword, -1)
0x08037430: ldrh.w r0, [sp, #6]
0x08037434: strb.w r9, [r8]            ; clear tx index at 0x2000000F
0x08037438: lsrs r1, r0, #8
0x0803743A: strb r0, [r7, #3]          ; low byte into tx buffer
0x08037440: strb r1, [r7, #2]          ; high byte into tx buffer
0x08037442: strb r0, [r7, #9]          ; byte-sum/check byte
0x08037444: ldr r0, [r5, #0]
0x08037446: orr.w r0, r0, #0x80        ; USART2 CTRL1 TDBEIEN
0x0803744A: str r0, [r5, #0]
```

The guarded bytes are:

```text
0x080373F4:
  82 b0 44 f2 0c 45 42 f6 74 56 40 f2 05 07 40 f2
  0f 08 c4 f2 00 05 c2 f2 00 06 0d f1 06 04 c2 f2
  00 07 c2 f2 00 08 4f f0 00 09 00 bf 30 68 21 46
  4f f0 ff 32 03 f0 d6 fe 01 28 f7 d1 bd f8 06 00
  88 f8 00 90 01 0a f8 70 00 eb 10 20 b9 70 78 72
  28 68 40 f0 80 00 28 60 0a 20 02 f0 9f ff e5 e7
```

This proves that the guarded selector-table producers feed a real stock
USART2 command path, not the display queue. It is still digital command-path
evidence only: it does not recover the DMM-specific `ms[0x02]`/`ms[0x03]`
analog mux writers, relay/range timing, or calibration.

The stock transport transition is now binary-guarded too. This
`meter transport transition guard` covers the two boot/config branches that
enable/resume or disable/drain the DMM USART2 path:

```text
0x08026F8E: USART2 CTRL1 |= 0x2000
0x08026F9E: load task handle 0x20002DA0, call vTaskResume
0x08026FAC: load task handle 0x20002DA4, call vTaskResume
0x08026FBA: prepare GPIOC BOP base
0x08026FC6: write 0x800 to GPIOC_BOP, setting PC11
0x08026FCE..0x08026FDA: reset max/min/avg sentinels to 0x7FC00000
0x08026FDE..0x08026FF6: reset selector/shadow/display state

0x0802700A: USART2 CTRL1 &= ~0x2000
0x0802701A: load task handle 0x20002DA0, call task suspend helper
0x08027028: load task handle 0x20002DA4, call task suspend helper
0x08027036..0x0802703A: write 0x800 to GPIOC clear register, clearing PC11
0x0802703E..0x0802704A: reset meter semaphore/queue 0x20002D7C
0x0802704E..0x0802705A: reset raw TX-word queue 0x20002D74
```

The guarded byte slices are:

```text
0x08026F8E:
  44 f2 0c 41 c4 f2 00 01 08 68 40 f4 00 50 08 60
  42 f6 a0 50 c2 f2 00 00 00 68 13 f0 32 fb
  42 f6 a4 50 c2 f2 00 00 00 68 13 f0 2b fb
  41 f2 00 01 4f f4 00 60 c4 f2 01 01 08 61
  00 20 c7 f6 c0 70 40 f2 01 11 ca f8 48 0f
  ca f8 4c 0f ca f8 50 0f 00 20 aa f8 35 1f
  ff 21 8a f8 5d 0f 8a f8 2d 0f 8a f8 2f 0f
  8a f8 38 1f aa f8 3c 0f

0x0802700A:
  44 f2 0c 41 c4 f2 00 01 08 68 20 f4 00 50 08 60
  42 f6 a0 50 c2 f2 00 00 00 68 13 f0 b2 fb
  42 f6 a4 50 c2 f2 00 00 00 68 13 f0 ab fb
  4f f4 00 60 c8 f8 00 00 42 f6 7c 50 c2 f2 00 00
  00 68 00 21 14 f0 ad f9 42 f6 74 50 c2 f2 00 00
  00 68 00 21 13 f0 ed fd
```

This is stock evidence for the transport side of DMM transitions: pause/drain
via task suspension and queue reset, then resume with USART2 and PC11 active.
It does not recover the exact local settle delay or frame-discard count, so the
open firmware must keep those constants documented as conservative local
policy until a stock runtime trace proves them.

The runtime UI/mode-switch path carries the same transport shape and is now
covered by the `runtime mode-switch transport guard`.  This is distinct from
the boot/config branch above: `mode_switch_handler` dispatches on the live mode
state and runs the DMM entry/exit transition while the stock app is operating.
The guarded sites are:

```text
0x08007360: common enable/resume tail
            writes mode_state = 1 at `0x20001060`
            USART2 CTRL1 |= 0x2000
            resumes task handles `0x20002DA0` and `0x20002DA4`
            writes `0x800` to GPIOC_BOP at `0x40011010`, setting PC11
            resets max/min/avg sentinels and selector/display shadow bytes
            tail-calls `0x0800B908`

0x0800741A: meter-entry pause/drain case
            USART2 CTRL1 &= ~0x2000
            suspends task handles `0x20002DA0` and `0x20002DA4`
            writes `0x800` to GPIOC_BC/BRR at `0x40011014`, clearing PC11
            resets meter semaphore/queue `0x20002D7C`
            resets raw TX-word queue `0x20002D74`
            clears DMM selector/display shadow state before epilogue

0x080074BE: active/running epilogue
            writes mode_state = 2 at `0x20001060`
            clears transient display bytes
            optionally writes `0x3C00` to `0x20002D50`
            tail-calls `0x0800B908`
```

The guarded byte slices are:

```text
0x08007360:
  01 20 84 f8 68 0f 44 f2 0c 40 c4 f2 00 00 01 68
  41 f4 00 51 01 60 42 f6 a0 50 c2 f2 00 00 00 68
  33 f0 46 f9 42 f6 a4 50 c2 f2 00 00 00 68 33 f0
  3f f9 41 f2 10 00 c4 f2 01 00 4f f4 00 61 01 60
  00 20 c7 f6 c0 70 40 f2 01 11 c4 f8 48 0f c4 f8
  4c 0f c4 f8 50 0f 00 20 a4 f8 35 1f ff 21 84 f8
  5d 0f 84 f8 2f 0f 84 f8 38 1f a4 f8 3c 0f a4 f8
  2c 1f a4 f8 69 0f 84 f8 6b 0f bd e8 10 40 04 f0
  93 ba

0x0800741A:
  44 f2 0c 40 c4 f2 00 00 01 68 21 f4 00 51 01 60
  42 f6 a0 50 c2 f2 00 00 00 68 33 f0 aa f9 42 f6
  a4 50 c2 f2 00 00 00 68 33 f0 a3 f9 41 f2 14 00
  c4 f2 01 00 4f f4 00 61 01 60 42 f6 7c 50 c2 f2
  00 00 00 68 00 21 00 25 33 f0 a1 ff 42 f6 74 50
  c2 f2 00 00 00 68 00 21 33 f0 e1 fb 01 20 84 f8
  36 0f 00 20 c7 f6 c0 70 c4 f8 48 0f c4 f8 4c 0f
  c4 f8 50 0f a4 f8 3c 5f a4 f8 2d 5f c4 f8 30 5f
  0b e0

0x080074BE:
  94 f8 54 13 02 20 84 f8 68 0f 00 20 09 07 a4 f8
  69 0f 84 f8 6b 0f 06 d0 42 f6 50 50 c2 f2 00 00
  4f f4 70 51 01 80 bd e8 b0 40 04 f0 0e ba
```

This strengthens the state-machine evidence for pause/drain/resume in normal runtime transitions.
It still does not recover a DMM-specific `ms[0x02]`/
`ms[0x03]` analog range writer, exact settle/discard counts, or any factory
calibration acceptance/apply proof.

The same script now also carries a selector state writer guard for the stock
digital DMM state machine. These sites prove stock RAM coupling around
`DAT_20001025` (`0x20001025`, selector), `DAT_2000102E` (`0x2000102e`, mode/range
shadow), `DAT_2000102F` (`0x2000102f`, display decimal shift), and
`DAT_20001027` (`0x20001027`, formatter substate). They are not analog
mux/range writers:

```text
0x08026FDE: init/reset clears selector/shadow state, including
            `strb.w r0, [sl, #0xf2d]` and `strb.w r0, [sl, #0xf2f]`
0x08036D14: RX classifier special branch writes `DAT_20001025 = 8`
0x08036D50: RX classifier B0/B1 branch writes `DAT_20001025 = 1`
0x08037220: RX branch writes `DAT_2000102E = 0`
0x080372E0: RX branch writes `DAT_2000102E = 0`,
            `DAT_2000102F = frame-derived bit`, `DAT_20001027 = 3`
0x08037328: RX branch writes `DAT_2000102E = 1`
0x08037338: RX branch writes `DAT_2000102E = 2`,
            `DAT_2000102F = 1 & ~frame_flag`, `DAT_20001027 = 1`
0x080373A8: RX branch writes `DAT_2000102E = 2`,
            `DAT_2000102F = frame-derived bit`, `DAT_20001027 = 2`
```

`FUN_080028E0` then reads `DAT_20001025` at `0x08002A9A` and dispatches the
formatter/unit cases listed below. This selector state writer guard proves the
digital stock DMM FSM around selector and formatter shadow bytes; it still does
not recover `ms[0x02]`/`ms[0x03]`, the analog frontend range writer, or a
factory calibration coefficient.

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
recovered or bench-proven, uA is unresolved and unexposed in the local UI, while
AC A remains parser/UI policy on top of the recovered ACA current slot.

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

`scripts/test_stock_meter_literals.py` binary-guards both saved-state apply
sites so this evidence cannot drift silently:

```text
0x08025544: a0 78 dc f7 ad f9 e0 78 dc f7 84 fa
0x0802723e: 9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc
```

These bytes prove the two boot/saved-state apply sequences only. They still do
not prove a runtime DMM writer that changes `ms[0x02]`/`ms[0x03]` while the user
switches local DMM ranges.

The unresolved part is the live/runtime writer for `ms[0x03]` during local
small-current versus A-range operation. Until that is recovered or bench-proven,
local submodes 2/3 and 4/5 share the same recovered stock current slot and are
split only by parser/UI range state. Treat current readings that still look like
voltage payloads as a frontend activation failure, not a decimal decoder issue.

### Mux Writer Xref Audit, 2026-06-06

The text decompile/xref pass found no DMM-specific runtime writer that maps the
eight recovered DMM selector words directly to unique `ms[0x02]` and `ms[0x03]`
bytes. A halfword-aligned binary sweep found all 16 direct `BL` callsites to
the two mux writers, and `scripts/test_stock_meter_literals.py` now carries a
mux callsite guard so this list cannot silently shrink back to a partial
function-map view:

```text
gpio_mux_portc_porte target 0x080018A4:
  0x080020B2: ff f7 f7 fb
  0x080031E8: fe f7 5c fb
  0x080039A2: fd f7 7f ff
  0x0801A53E: e7 f7 b1 f9
  0x0801C7CC: e5 f7 6a f8
  0x0801D094: e4 f7 06 fc
  0x08025546: dc f7 ad f9
  0x08027242: da f7 2f fb

gpio_mux_porta_portb target 0x08001A58:
  0x08001F06: ff f7 a7 fd
  0x08003644: fe f7 08 fa
  0x08003E3A: fd f7 0d fe
  0x0801A534: e7 f7 90 fa
  0x0801C7D8: e5 f7 3e f9
  0x0801D0A0: e4 f7 da fc
  0x0802554C: dc f7 84 fa
  0x0802724A: da f7 05 fc
```

The evidence splits like this:

| Evidence | Stock offsets / files | Classification |
|---|---|---|
| `gpio_mux_portc_porte` body | `FUN_080018a4`, `full_decompile.c:2206..2295`; 10-way `param_1` switch writing GPIOC/E plus DAC calibration tables | hardware writer, stock-proven |
| `gpio_mux_porta_portb` body | `FUN_08001a58`, `full_decompile.c:2300..2365`; 10-way `param_1` switch writing GPIOA/B pins including PB11 | hardware writer, stock-proven |
| saved-state restore/apply | `0x08025544..0x0802554c` and `0x0802723e..0x0802724a` load saved `ms[0x02]`/`ms[0x03]` then call both mux writers | DMM-relevant boot/saved-state evidence |
| direct decompile callers | `function_names.md` lists `FUN_08001c60` as `siggen_configure` and `FUN_08019e98` as `scope_main_fsm`; `function_map_complete.txt` lists only callers `08001c60,08019e98` for both mux writers even though the binary guard proves more BL sites | decompile/function-map limitation; not enough for DMM selector proof |
| `FUN_08001c60` scope/siggen channel setup | `0x08001F06` / `0x080020B2`; `full_decompile.c:2564..2574` increments `(&DAT_200000fa)[uVar20]`, then calls `FUN_080018a4(DAT_200000fa)` for channel 0 or `FUN_08001a58(DAT_200000fb)` for channel 1 and queues command `4` | scope/siggen auto-range path; not DMM |
| scope/preset mux owner handlers | `0x08003148` and `0x08003900` wrap the callsites `0x080031E8` / `0x08003644` and `0x080039A2` / `0x08003E3A`; binary disassembly shows they increment/decrement `(&DAT_200000fa)[DAT_2000044c >> 7]`, call `FUN_080018a4` for channel 0 or `FUN_08001a58` for channel 1, then queue command `4` | scope/preset UI mux owners; not DMM runtime range proof |
| scope UI mux-LUT consumer | `0x080151B0..0x080151F2` in `FUN_08015f50`/`scope_ui_draw_main`; reads `DAT_2000010e`, loads `(&DAT_200000fa)[idx]`, derives the modulo-3 scale index, then reads `DAT_0804bfb8` | scope render/scale consumer; not a DMM range writer |
| scope main auto-range write | `0x0801A534` / `0x0801A53E`; `full_decompile.c:6880..6999` scans sample buffers, enters range selection, then reuses `DAT_200000fa/DAT_200000fb` for DAC/calibration recompute; `full_decompile.c:8744..8752` performs the actual mux call after `(&DAT_200000fa)[uVar70] = bVar37 + 1` | oscilloscope acquisition path; not DMM |
| explicit scope-submode mux calls | `full_decompile.c:7564..7565` and `7988..7989` call both mux writers with `DAT_20000128 & 0xf`; `scope_main_fsm_annotated.c` names `DAT_20000128`/state `+0x30` as scope sub-mode | scope runtime reconfiguration, not DMM |
| DAC1 writes | `FUN_080018a4` at `0x080018A4..0x08001A52` and inline recomputes at `full_decompile.c:2603..2624`, `6960..7020`, `7771..7990` write `0x40007408` from scope calibration tables | scope trigger/comparator threshold; not DMM calibration |
| waveform calibration/render use | `full_decompile.c:8611..8624`, `9840..9971` index `DAT_080465cc` and calibration deltas through current and saved `DAT_200000fa/DAT_200000fb` | scope display/calibration path, not DMM selector proof |

### Mux Writer Body Guard, 2026-06-06

`scripts/test_stock_meter_literals.py` now also binary-guards representative
slices inside the two mux writer bodies, not only their callsites:

```text
gpio_mux_portc_porte / FUN_080018a4:
  0x080018A4 switch prologue: 09 28 00 f2 88 80 df e8 ...
  0x080018C4 gpio_pc12_pe_write_block: GPIOC/E BOP/BCR writes for PC12 and PE pins
  0x080019BA scope_calibration_table_select: indexes scope calibration tables
  0x08001A20 DAC1/scope calibration tail: updates 0x40007408/0x40007404

gpio_mux_porta_portb / FUN_08001a58:
  0x08001A58 switch prologue: 09 28 00 f2 bb 80 df e8 ...
  0x08001A78 gpio_pa15_pb11_pb10_write_block: GPIOA/B writes for PA15/PB11/PB10
  0x08001B82 gpio_high_modes_write_block: higher mux modes writing PA/B pins
  0x08001BD4 scope_calibration_table_select: indexes scope calibration tables
  0x08001C3A DAC1/scope calibration tail: updates scope DAC state
```

This mux writer body guard proves that the functions behind `ms[0x02]` and
`ms[0x03]` really are 10-way GPIO hardware writers with scope-calibration/DAC1
tails. It deliberately does not prove that any inspected DMM runtime branch
writes those bytes during local range switching, and it does not turn the DAC1
tail into a DMM calibration coefficient.

### Runtime Mux-State Writer Guard, 2026-06-06

The text decompile currently exposes only two runtime writes to the
`DAT_200000fa`/`DAT_200000fb` mux-state pair, and
`scripts/test_stock_meter_literals.py` now binary-guards both:

```text
0x08001EE8: `FUN_08001c60` increments `(&DAT_200000fa)[uVar20]`,
            calls `FUN_080018a4(DAT_200000fa)` or
            `FUN_08001a58(DAT_200000fb)`, then queues command `4`
0x0801A526: `FUN_08019e98` writes `(&DAT_200000fa)[uVar70] = bVar37 + 1`,
            calls `FUN_080018a4`/`FUN_08001a58`, then queues command `4`
```

This is negative DMM evidence. Both guarded writer branches are scope/siggen
autorange/frontend paths in the current decompile context. They prove that the
stock firmware mutates the mux-state pair at runtime, but they do not recover a
DMM-mode runtime writer for `ms[0x02]`/`ms[0x03]`. A future DMM correction must
find a DMM-owned writer or a stock runtime trace; it must not reuse these
scope/siggen autorange branches as a meter range proof.

This means the open firmware can legitimately project the recovered stock DMM
slots into the two mux bytes for fail-closed local operation, but it must keep
that projection marked as a local policy. The complete direct mux callsite list
does not recover runtime DMM `ms[0x02]`/`ms[0x03]` writers: no inspected
callsite maps the eight DMM selector slots to analog mux bytes. Until either
Ghidra data-xrefs recover the DMM saved-state writers or a stock-runtime trace
records `0x200000fa/0x200000fb` while switching DMM modes, scope/siggen mux callers are not DMM runtime range proof.
Do not treat scope auto-range writes as evidence for DMM current/voltage range
decoding, and do not repair a surprising DMM reading by adding a numeric
coefficient on top of those scope paths.

The same boundary applies to DAC1 (`0x40007408`). Stock DAC1 writes are real
and table-backed, but current xrefs tie them to the scope trigger/comparator
path. They are not a recovered meter reference or low-DCV correction source.

### Scope Snapshot Consumer Guard, 2026-06-06

`FUN_08034078` is another easy place to draw the wrong conclusion. The stock
decompile copies the current scope/mux state into the `DAT_20000eb8..` snapshot
block before scope measurement/display math:

```text
full_decompile.c:26144  DAT_20000eb8 = DAT_20000125;
full_decompile.c:26145  DAT_20000eb9 = DAT_200000fa;
full_decompile.c:26155  _DAT_20000eba = _DAT_200000fb;
```

Those snapshot bytes are later consumed by scope scale/table paths such as
`full_decompile.c:8613`, `9087`, and `9847..10066`, where
`DAT_080465cc` is indexed by the saved mux state. The function is named
`scope_display_refresh` in `function_names.md`, is called from `scope_main_fsm`
and scope render paths, and does not call `FUN_080018a4` or `FUN_08001a58`.

`scripts/test_stock_meter_literals.py` binary-guards the opening snapshot block:

```text
0x08034078:
  2d e9 f0 4f 81 b0 2d ed 04 8b 40 f2 f8 05 c2 f2
  00 05 95 f8 2d 00 4a f6 ab 27 ca f6 aa 27 a0 fb
  07 12 a9 78 85 f8 c0 0d 85 f8 c1 1d d5 f8 1a 10
  b5 f8 b4 0d 4f ea 31 41 c5 f8 c6 1d a9 8a 6b 79
  a5 f8 be 1d b5 f8 b6 1d a5 f8 e0 0d
```

This is a scope snapshot consumer guard. It proves that stock reads the current
mux-state pair into a measurement/display snapshot, then uses that snapshot in
scope math. It is explicitly a consumer/snapshot path, not a DMM mux writer,
not a DMM mode/range transition, and not a factory meter calibration source.

### Scope/Preset Mux Owner Guard, 2026-06-06

The remaining early mux callsites from the direct BL sweep are now classified
from stock binary disassembly instead of left as possible DMM evidence. The two
paired handlers are:

```text
0x08003148: scope/preset mux increment handler
0x08003900: scope/preset mux decrement handler
```

Both handlers read `DAT_20001060` (`[base+0xf68]`) for a UI/state switch and
use `DAT_2000044c` (`[base+0x354]`) as the channel selector. The low nibble
selects an action, while the sign/high bit selects which mux-state byte is
edited:

```text
0x080031B6: add.w r0, r5, r0, lsr #7
0x080031BA: ldrb.w r1, [r0, #2]!
0x080031C4: adds r1, #1
0x080031C6: strb r1, [r0, #0]
...
0x080031DE: cmp.w r0, #-1
0x080031E2: ble.w 0x08003642
0x080031E6: ldrb r0, [r5, #2]
0x080031E8: bl 0x080018a4

0x08003642: ldrb r0, [r5, #3]
0x08003644: bl 0x08001a58
0x08003658: movs r1, #4
0x08003664: bl 0x0803acf0
```

The decrement-side handler mirrors the same ownership shape:

```text
0x08003970: add.w r0, r6, r0, lsr #7
0x08003974: ldrb.w r1, [r0, #2]!
0x0800397E: subs r1, #1
0x08003980: strb r1, [r0, #0]
...
0x08003998: cmp.w r0, #-1
0x0800399C: ble.w 0x08003E38
0x080039A0: ldrb r0, [r6, #2]
0x080039A2: bl 0x080018a4

0x08003E38: ldrb r0, [r6, #3]
0x08003E3A: bl 0x08001a58
0x08003E4E: movs r1, #4
0x08003E5A: bl 0x0803acf0
```

`scripts/test_stock_meter_literals.py` carries a scope/preset mux owner guard
for the two prologues and all four mux branches:

```text
0x08003148 increment prologue:
  f0 b5 81 b0 2d ed 02 8b 40 f2 f8 05 c2 f2 00 05
  95 f8 68 0f 01 38 08 28 00 f2 b3 83 df e8 10 f0
0x080031B6 increment Port C/E branch:
  05 eb d0 10 10 f8 02 1f ... a8 78 fe f7 5c fb
0x08003642 increment Port A/B branch:
  e8 78 fe f7 08 fa ... 04 21 38 68 21 70 ... 37 f0 44 fb

0x08003900 decrement prologue:
  f0 b5 81 b0 2d ed 02 8b 40 f2 f8 06 c2 f2 00 06
  96 f8 68 0f 01 38 08 28 00 f2 4b 84 df e8 10 f0
0x08003970 decrement Port C/E branch:
  06 eb d0 10 10 f8 02 1f ... b0 78 fd f7 7f ff
0x08003E38 decrement Port A/B branch:
  f0 78 fd f7 0d fe ... 04 21 28 68 21 70 ... 36 f0 49 ff
```

This resolves the "additional early UI/scope-owner mux sites" row from the
previous audit: these are stock scope/preset UI mux owners. They prove another
runtime owner of the shared mux-state pair, but they are not tied to the
eight-entry DMM selector table and are not DMM runtime range proof.

### Scope UI Mux-LUT Consumer Guard, 2026-06-06

The `ram_map.txt` entry `unknown@080151c2` for `DAT_200000fa` resolves into
`FUN_08015f50`, already named `scope_ui_draw_main` in `function_names.md`. The
stock decompile around `full_decompile.c:11411..11438` reads the selected
channel's mux byte and uses it to index the scope scale table:

```text
full_decompile.c:11411  pbVar19 = &DAT_200000fa + uVar22;
full_decompile.c:11418  FUN_0803e5da(*(undefined2 *)(&DAT_0804bfb8 + ...), ...)
full_decompile.c:11438  uVar6 = *(undefined2 *)(&DAT_0804bfb8 + ...);
```

The corresponding stock instruction slice starts at `0x080151B0`:

```text
0x080151B0: movw r8,#0xf8
0x080151B4: movt r8,#0x2000
0x080151B8: ldrb.w r0,[r8,#22]      ; DAT_2000010e channel/index
0x080151C0: add r0,r8
0x080151C2: ldrb r1,[r0,#2]         ; (&DAT_200000fa)[idx]
0x080151E4: movw r1,#0xbfb8
0x080151E8: movt r1,#0x804          ; DAT_0804bfb8
0x080151EE: ldrh.w r0,[r1,r0,lsl #1]
```

`scripts/test_stock_meter_literals.py` binary-guards that mux-LUT consumer:

```text
0x080151B0:
  40 f2 f8 08 c2 f2 00 08 98 f8 16 00 4a f6 ab 23
  40 44 81 78 ca f6 aa 23 ca b2 a2 fb 03 23 b8 f9
  1c 20 90 f9 04 00 5c 08 10 1a 00 ee 10 0a a4 eb
  84 00 08 44 4b f6 b8 71 c0 b2 c0 f6 04 01 31 f8
  10 00
```

This is scope render/scale math. It consumes `DAT_200000fa` and
`DAT_0804bfb8`, but it performs no mux writer call, no DMM selector-table
transition, and no meter calibration. Do not use this xref to justify a DMM
range correction.
