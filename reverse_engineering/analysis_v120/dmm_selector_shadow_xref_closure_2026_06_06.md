# DMM Selector/Shadow Xref Closure

Date: 2026-06-06

This note records the current V1.2.0 evidence boundary for the stock DMM
selector/shadow bytes:

- `DAT_20001025` (`0x20001025`, `ms[0xF2D]`): stock digital DMM selector.
- `DAT_2000102e` (`0x2000102e`, `ms[0xF36]`): stock formatter/variant shadow.

These bytes are real DMM state-machine evidence and they feed the recovered
`0x05xx` raw-word command path. They are not the missing physical mux/range
writer for `ms[0x02]`/`ms[0x03]`, not a relay/AFE writer, and not a calibration
source for the low-DCV blocker.
In short: not the missing `ms[0x02]`/`ms[0x03]` analog mux/range writer.

## RAM-Map Surface

Current `reverse_engineering/analysis_v120/ram_map.txt` reports:

```text
0x20001025 DAT_20001025 (9 refs): FUN_0800ec70@0800ec70, FUN_080028e0@080028e0,
  unknown@0800f402, unknown@0800f43c, unknown@0800f476, unknown@0800f4b0,
  unknown@0800f4ea, unknown@0800f524, unknown@0800f55e
0x2000102E DAT_2000102e (7 refs): FUN_0800ec70@0800ec70, FUN_080028e0@080028e0
```

The important current interpretation is split:

- `FUN_080028E0` is the display formatter/state reducer. In
  `full_decompile.c:2931..3005`, it switches on `DAT_20001025` and uses
  `DAT_2000102e` to select unit/display-format state such as current unit
  indices `4` and `3`.
- `FUN_0800ec70` is display/render fallout. At
  `full_decompile.c:4449..4450`, it reads `DAT_2000102e`,
  `DAT_20001027`, and `DAT_20001025` before drawing an extra UI element. It is
  a consumer, not a selector/range writer.

## Binary-Guarded Writers And Consumers

`scripts/test_stock_meter_literals.py` currently pins the stronger byte-level
evidence:

| Guard | Stock offsets | Meaning |
| --- | --- | --- |
| selector table consumer xrefs | `0x080042E2`, `0x080048BA` | Read `DAT_20001025`, index the eight-byte table at runtime `0x080BB3FC`, form `0x0500 | low`, and stage the raw word at `0x20002D54`. |
| selector adjust prev/next | `0x080041F8`, `0x080042D4`, `0x080047CC`, `0x080048AC` | Decrement/increment `DAT_20001025` with wrap over `0..7`, emit the matching `0x05xx` raw word, queue display/update bytes `0x1D` and `0x1B`, and reset display/value state. |
| selector/shadow reset and RX writes | `0x08026FDE`, `0x08036D14`, `0x08036D50`, `0x08037220`, `0x080372E0`, `0x08037328`, `0x08037338`, `0x080373A8` | Reset or force selector/shadow bytes inside the stock transport/RX/display FSM. |
| dynamic raw-word helper | `0x08006060`, `0x08006120`, `0x08006194`, `0x0800626A`, `0x08006288`, `0x080062F8` | Gate on DMM runtime state, selector mask `0xC6`, and `DAT_2000102e`, then emit only recovered apply pairs `0x0C/0x0D`, `0x17/0x0E`, `0x11/0x16`, and `0x10/0x15`. |
| `dvom_TX` raw-word consumer | `0x080373F4` | Dequeues halfwords from `0x20002D74`, copies high/low bytes into the USART2 TX buffer, and starts the TX interrupt pump. |

Together these guards prove a stock digital selector/shadow state machine that
emits real USART2 DMM command words. They do not prove an analog range writer.

## Boundary For Future Work

Do not use `DAT_20001025` or `DAT_2000102e` as a shortcut for the unresolved
low-DCV physical mismatch. A future fix must still recover one of:

- a DMM-owned runtime writer or trace for `DAT_200000fa`/`DAT_200000fb`
  (`ms[0x02]`/`ms[0x03]`)
- an H2/SPI3 acceptance/apply condition plus multi-point DMM effect
- a W25Q/system-file/factory-calibration source
- safe multi-point stock/live traces that tie selector/mux/range state to the
  measured hard cases

Until that evidence exists, `DAT_20001025` is a digital selector, and
`DAT_2000102e` is formatter/variant shadow state.
