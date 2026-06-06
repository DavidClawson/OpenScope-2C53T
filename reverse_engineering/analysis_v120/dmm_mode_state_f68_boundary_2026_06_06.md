# DMM Mode-State `ms[0xF68]` Boundary

Date: 2026-06-06

This note records the current stock-visible boundary for
`DAT_20001060` (`0x20001060`, `ms[0xF68]`). The byte is important for the DMM
state machine because stock mode-init, selector/raw-word helper, and transport
paths gate on it. It is not the missing physical DMM range state.

## RAM-Map Surface

Current `reverse_engineering/analysis_v120/ram_map.txt` reports:

```text
0x20001060 DAT_20001060 (7 refs): FUN_08009014@08009014,
  FUN_08019e98@08019e98, unknown@0800b914, unknown@08015848,
  FUN_080096e8@080096e8, FUN_08009a94@08009a94
```

`scripts/test_stock_meter_literals.py` now guards this exact RAM-map surface as
the `stock mode-state RAM-map boundary`.

## DMM-Relevant Stock Evidence

The DMM paths already guarded in `scripts/test_stock_meter_literals.py` use
this state byte in three distinct ways:

| Evidence | Stock offsets | Meaning |
| --- | --- | --- |
| saved mode-init restore | `0x08026F50`, `0x08026F56`, `0x08026F80..0x08026F8C` | Saved `ms[0xF64]` copies into live `ms[0xF68]`; states `1`/`2`/`3` branch into boot transport paths. |
| runtime mode-switch transport | `0x08007360`, `0x0800741A`, `0x080074BE` | Runtime DMM entry/exit writes `ms[0xF68]` state `1`/`2`, enables or disables USART2, resumes/suspends `dvom_TX`/`dvom_RX`, toggles PC11, and resets queues/state. |
| boot mode-init dispatcher | `0x0800B908` plus callers `0x08002DAA`, `0x080051D6`, `0x0800533A`, `0x08005572`, `0x080271F8` | Reads `ms[0xF68]` and emits stock command banks through `0x20002D6C`. |
| runtime mode-init callers | `0x08006418`, `0x08006548`, state/latch slices `0x0800644E`, `0x080064E0`, `0x08006578`, `0x08006592`, `0x080065B2` | Mutate `ms[0xF68]` and neighboring latch/progress bytes, then tail-call `FUN_0800B908`. |
| dynamic raw-word helper | `0x08006120`, `0x080062F8` | Gates selector/apply helper behavior on `ms[0xF68] == 1` before emitting recovered digital `0x05xx` command words. |

This proves `ms[0xF68]` is a stock mode-init/command-bank/transport state byte.
It does not prove relay/AFE state, factory calibration, or the runtime analog
mux/range writer for `ms[0x02]`/`ms[0x03]`.

## Non-Meter Command-Bank Overlap

The `FUN_0800B908` TBH map also includes banks that can look tempting when a
new DMM scenario appears:

```text
state 4 -> 0x0800BB64: 0x00, 0x1F, 0x09, 0x20, 0x21
state 5 -> 0x0800BBBE: 0x00, 0x25, 0x09, 0x26, 0x27, 0x28
state 6 -> 0x0800BC2A: 0x29
state 8 -> 0x0800BCA6: 0x00, 0x2C
```

Those bytes are guarded as command-bank overlap, not as recovered DMM physical
frontend state. State 4 overlaps stock frequency/acquisition command-family
evidence. State 5 overlaps stock timebase/packed-state command-family evidence.
State 6 is a standalone command-bank tail. State 8's `0x2C` byte is not by
itself a continuity/diode physical frontend proof. None of these states recovers
capacitance/temperature physical range switching, a current range split, or
factory calibration.

Plain-language guard: frequency/acquisition command-family evidence is not DMM
physical range proof; timebase/packed-state command-family evidence is not
capacitance/temperature physical range proof.

Machine-readable guard: not capacitance/temperature physical range proof.

## Boundary For Future Work

Do not treat `DAT_20001060` as a recovered DMM physical range source. In
particular:

- `ms[0xF68]` command-bank state is not a low-DCV correction.
- `ms[0xF68]` state `1`/`2` transition evidence does not recover exact stock
  settle/discard counts.
- `ms[0xF68]` helper gating does not upgrade H2/SPI3 byte-count replay into
  FPGA acceptance/apply proof.

The remaining low-DCV blocker still needs a DMM-owned runtime writer or trace
for `ms[0x02]`/`ms[0x03]`, H2/SPI3 acceptance/effect evidence, or a real
factory-calibration source.
