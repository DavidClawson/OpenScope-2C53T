# Scope Cluster Control Bytes

Date: 2026-04-08

Purpose:
- re-audit the `+0xE10 / +0xE11 / +0xE1B` control bytes around the
  `0x08015780..0x08015A14` redraw ladder
- correct the earlier over-read of `0x08015848` as a generic scope-mode commit
- separate file/resource overlay state from the real scope-active `2 / 9` path

Primary references:
- [data_xrefs_scope_cluster_ctrl_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_scope_cluster_ctrl_2026_04_08.txt)
- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c)
- [scope_cluster_ctrl_force_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_xrefs_2026_04_08.txt)
- [scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L32348)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28279)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29622)
- [mode_selector_writer_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md)
- [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md)

## Executive Summary

This pass changes the interpretation of the `0x08015780..0x08015A14` cluster in
an important way.

1. `0x08015848` is **not** a generic "scope mode commit" helper.
2. It is an interior slice in a larger right-panel redraw ladder.
3. In the special `*(base + 0xE10) == 1` branch, that ladder:
   - loads `0x080BC18B`
   - calls `FUN_08034878(...)`
   - stores the returned byte to `*(base + 0xE1B)`
   - clears `*(base + 0xE10) = 0xFF`
   - hard-sets `DAT_20001060 = 10`
   - queues display selectors `0x24`, then `0x03`
4. `+0xE10 / +0xE11 / +0xE1B` therefore fit a file/resource overlay side path
   much better than the missing FPGA scope-enter path.

So the cluster is still useful, but mostly as a correction: it explains the
observed `= 10` state without making it the main scope blocker.

## 1. Raw-objdump correction to the ladder

The raw disassembly at
[scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_disasm_08015780_08015A20_2026_04_08.txt)
is the strongest source for this cluster.

At `0x080157B4`, the enclosing body reads `*(base + 0xE10)` and branches:

- `== 3` -> `0x08015876`
- `== 2` -> `0x08015884`
- `== 1` -> special path beginning at `0x080157C4`
- else -> common redraw tail at `0x080158BA`

The special `== 1` path then:

1. loads `0x080BC18B`
2. calls `FUN_08034878`
3. stores the returned byte to `*(base + 0xE1B)` at `0x080157EC`
4. writes `*(base + 0xE10) = 0xFF` at `0x0801583E`
5. writes `DAT_20001060 = 10` at `0x08015848`
6. queues selectors `0x24` and `0x03`

That is a much better fit for:

- a file/resource side overlay commit
- a browser/post-selection redraw handoff

than for:

- the missing scope-enter / FPGA-arm transition

## 2. `+0xE10` / `DAT_20000F08`: transient overlay branch selector

The best current read of `+0xE10` is a short-lived branch/control byte for this
panel/overlay family.

### Producer-side behavior

The forced slice at
[scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L10)
and the raw objdump around `0x0800A660` show:

- if `*(base + 0xE11) == 0xFF`, write `*(base + 0xE10) = 3`
- if `FUN_08036084()` or `FUN_08035ED4()` reports an error/fallback, write
  `*(base + 0xE10) = 3`
- on the clean path, write `*(base + 0xE10) = 2`, append `*(base + 0xE11)` into
  `*(base + 0xE25 + *(base + 0xE1B))`, then increment `*(base + 0xE1B)`
- in every case, finish by writing `DAT_20001060 = 2`

So `+0xE10` is not a stable top-level mode byte. It is a short-lived status /
branch selector layered on top of the broader `DAT_20001060 == 2` scope-visible
state.

### Consumer-side behavior

In
[key_task @ 0x08039008](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L32348),
the runtime policy is:

- if `(DAT_20000F08 & 0xFE) == 2`, clear it to `0`
- then, if the side-panel flags are clear, dispatch the real key handler

That means the `2` / `3` family is explicitly treated as transient overlay
state, not a long-lived scope mode.

## 3. `+0xE11` / `DAT_20000F09`: current resource/list index

This byte now looks like the current resource/list selection index for the same
overlay path.

The clearest writer is
[FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28279),
which scans entries, builds a compact list, and then:

- if no items survive, writes `DAT_20000F09 = 1`
- otherwise writes `DAT_20000F09 = last_item_id + 1`

The raw disassembly at `0x0803497C..0x08034992` confirms the same behavior:

- load `*(base + chosen_item + 0xE24)`
- add `1`
- store to `*(base + 0xE11)`
- else default to `1`

This byte is then consumed by:

- the special redraw-ladder path at `0x0801580E`
- `FUN_08035ED4()`, which formats a path with `DAT_20000F09`

So `+0xE11` is best read as:

- current browser/list/resource index

not:

- generic power configuration

## 4. `+0xE1B` / `DAT_20000F13`: entry count / append index

This byte is also no longer well-described as a single "SPI flash byte."

Two concrete behaviors are now visible:

1. the special `*(base + 0xE10) == 1` redraw-ladder path stores the return value
   of `FUN_08034878(...)` into `*(base + 0xE1B)`
2. the `0x0800A660` helper uses `*(base + 0xE1B)` as an append index into the
   `+0xE25[]` byte array, then increments it

That makes `+0xE1B` a better fit for:

- entry count / append cursor for the overlay list state

than for a passive config byte.

## 5. `+0xE1C` / `DAT_20000F14`: panel subview / toggle byte

This byte was not the main target of the pass, but the current evidence is still
strong enough to narrow it down.

The raw disassembly shows repeated `0 <-> 1 <-> 2` tests and writes on `+0xE1C`
inside the same panel/state family, and
[FUN_0801819C](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L5228)
renders different right-panel strings/colors from `DAT_20000F14`.

So `+0xE1C` now looks much more like:

- panel subview / toggle state

than:

- display brightness

## 6. Why this matters for the scope bring-up problem

This pass lowers the priority of the whole ladder as a direct scope-enter lead.

The stronger picture is now:

- `DAT_20001060 == 2` is still the main visible scope-active state
- `DAT_20001060 == 9` is still a real special substate layered on top of that
- the packed family `0xF69..0xF6B` is still where the scope-side draw logic is
  most active
- `+0xE10 / +0xE11 / +0xE1B` belong to a shorter-lived list/resource overlay
  branch that eventually feeds `DAT_20001060 = 10`

So the corrected branch order is:

1. keep this ladder documented as overlay/resource state
2. stop treating `0x08015848` as the main missing scope-mode commit
3. pivot back to the real scope-active `2 / 9` path and the packed
   `0xF69..0xF6B` family

## 7. Best next move

The next highest-value RE pass is now:

1. trace who feeds `DAT_20001060 == 2` and `== 9` in the true scope path
2. trace wider writers/readers for `0xF69..0xF6B`
3. use `+0xE10` only as a supporting overlay/resource clue, not as the main
   scope-enter hypothesis
