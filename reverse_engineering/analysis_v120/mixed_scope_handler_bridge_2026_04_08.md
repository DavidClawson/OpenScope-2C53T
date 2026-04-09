# Mixed Scope Handler Bridge

Date: 2026-04-08

Purpose:
- promote `FUN_08002FE8` from a loose forced slice to the current best bridge
  between visible scope mode, packed scope substate, and queued selector banks
- explain why this function is a stronger next trace target than the older
  `0x08015848` redraw ladder
- pin down the exact state transitions and command-bank emissions that now look
  closest to the missing scope-enter orchestration

Primary references:
- [mixed_scope_handlers_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handlers_force_2026_04_08.c)
- [mixed_scope_handler_bridge_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_bridge_xrefs_2026_04_08.txt)
- [packed_scope_state_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_writers_2026_04_08.md)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)
- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)
- [high_flash_scope_indexing_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_scope_indexing_2026_04_08.md)

## Executive Summary

`FUN_08002FE8` at `0x08002FE8` is now the strongest single static lead for the
scope-mode problem.

That is because it directly ties together:

1. `DAT_20001060`, the best current top-level scope-active state byte
2. the packed substate bundle `DAT_20001061..DAT_20001063` (`0xF69..0xF6B`)
3. the exact `0x20002D6C` selector banks that mirror the command families we
   have been bench-testing (`0x0B..0x11`, `0x16..0x19`, `0x1A..0x1E`,
   `0x1F..0x21`, `0x25..0x29`, `0x12..0x14`, `0x2C`)

So the current best RE question is no longer:

- "what did `0x08015848` mean?"

It is now:

- "who drives `DAT_20001060` into `2 / 3 / 4 / 9` before `FUN_08002FE8`
  executes, and what user-visible scope action does each state represent?"

## 1. Why `FUN_08002FE8` matters more than the old redraw-ladder lead

The corrected right-panel note in
[scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)
showed that `0x08015848` belongs to an overlay/file-resource ladder that
hard-sets `DAT_20001060 = 10`.

That was useful as a correction, but it moved the real scope-enter trail
elsewhere.

`FUN_08002FE8` is better because it is the first recovered body that:

- switches directly on `DAT_20001060`
- mutates the packed scope substate bytes
- emits the familiar scope-selector banks

That makes it look much more like a real bridge between high-level scope UI
state and the lower-level FPGA/display selector choreography.

The fresh xref pass in
[mixed_scope_handler_bridge_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_bridge_xrefs_2026_04_08.txt)
also came back with:

- caller count `0`
- callee count `0`
- body size `1` byte

That does not weaken this lead. It strengthens the interpretation that
`0x08002FE8` is an interior forced slice inside a larger owner that Ghidra has
not reconstructed cleanly as a normal function.

## 2. Visible scope state enters an internal `9` path here

The most important transition in the forced output is:

- `case 2`:
  - `DAT_20001060 = 9`
  - clear `_DAT_20001061 = 0`
  - clear `DAT_20001063 = 0`
  - fall into the nested bank-emission switch

This lines up cleanly with the earlier `2 <-> 9` hypothesis from
[scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md):

- `2` still looks like the externally visible scope-active state
- `9` now looks like a real internal scope transition / selector-bank state

That is more concrete than the older helper-cluster wording because it is now
grounded in a recovered mixed-purpose handler instead of a narrower auxiliary
slice.

## 3. Packed scope substate is owned here, not only read elsewhere

`FUN_08002FE8` does not just branch on `DAT_20001060`. It also rewrites the
packed bytes:

- `DAT_20001061`
- `DAT_20001062`
- `DAT_20001063`

Important examples from the forced decompile:

- `case 3`
  - if `DAT_20001061 == 2`, collapse to `CONCAT11(DAT_20001062, 1)` and mask
    `DAT_20001063 &= 0x0F`, then queue `0x19`
  - if `DAT_20001061 == 1`, zero `DAT_20001063`, fold `DAT_20001062` into the
    packed pair, then queue `0x19`
- `case 4`
  - if `DAT_20001061 == 1`, fold `DAT_20001062`, clear `DAT_20001063`, then
    queue `0x20`, `0x21`
- `case 9`
  - if `DAT_20001061 == 2`, fold/mask and queue `0x14`
  - if `DAT_20001061 == 1`, fold/clear and queue `0x13`, `0x14`
  - otherwise collapse back to visible `DAT_20001060 = 2`, then clear packed
    state again

That means the packed bytes are not just passive display inputs. They are part
of the same state transition surface that decides which selector bank gets sent.

## 4. The nested switch matches the scope selector families we already know

After the `DAT_20001060` pre-processing, `FUN_08002FE8` emits banks that map
very closely to the families we have already been testing in firmware:

- `state 0`:
  - `1, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11`
- `state 1`:
  - `9`, then `7` or `10`, then `0x1A, 0x1B, 0x1C, 0x1D, 0x1E`
- `state 2`:
  - `2, 3, 4, 5, 6, 8`
- `state 3`:
  - `8, 9`, then `7` or `10`, then `0x16, 0x17, 0x18, 0x19`
- `state 4`:
  - `0x1F, 9, 0x20, 0x21`
- `state 5`:
  - `0x25, 9, 0x26, 0x27, 0x28`
- `state 6`:
  - `0x29`
- `state 7`:
  - `0x15`
- `state 8`:
  - `0x2C`
- `state 9`:
  - `0x12, 0x13, 0x14, 9`, then `7` or `10`

This is the most convincing static bridge so far between:

- the state bytes we can reason about
- and the exact command-bank families that matter for scope bring-up

## 5. Why this narrows the FPGA scope problem

This does not prove that the downloaded vendor app contains every final FPGA TX
detail. But it does narrow the missing piece.

The most likely problem is now less about:

- blindly discovering another raw command family

and more about:

- reproducing the right high-level transition into the correct `DAT_20001060`
  state
- with the matching packed `0xF69..0xF6B` substate already staged
- so the correct selector bank fires in the same sequence stock expects

That is a much stronger theory than "maybe the right answer is still hidden in a
single unknown payload byte."

## 6. Best next trace

The next best RE move is to treat `FUN_08002FE8` as the hub and trace outward.

Priority order:

1. find who enters `DAT_20001060 = 2`
2. find who promotes `2 -> 9`
3. find who sets `DAT_20001060 = 3` and `4`
4. connect those entries to user-visible scope actions or submenus
5. only after that, compare the resulting selector-bank path against the live
   firmware experiments

Practically, the next pass should focus on callers and nearby enclosing bodies
for the `0x08002FE8` region, not on more detached overlay/resource clusters.
