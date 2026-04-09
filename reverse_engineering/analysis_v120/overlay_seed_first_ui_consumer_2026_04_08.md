# Overlay Seed First UI Consumer 2026-04-08

Purpose:
- identify the first concrete runtime/UI-visible consumer of the overlay state
  seeded from `FUN_08034878(0x080BC18B)`
- separate that consumer from earlier boot-time filesystem normalization
- assess whether there is much unexplained code-flow left before the missing
  high-flash data becomes the dominant uncertainty

Key references:
- [FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)
- [right_panel_redraw_cluster_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_redraw_cluster_force_2026_04_08.c#L1)
- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L1)
- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md#L1)
- [fatfs_boot_followon_non_gate_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fatfs_boot_followon_non_gate_2026_04_08.md#L1)

## 1. What `FUN_08034878()` Seeds

[FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)
enumerates the descriptor family rooted at `0x080BC18B` and seeds:

- `DAT_20000F13` / panel entry count
- `DAT_20000F09` / panel entry index

Internally it also formats:

- `0x080BCAE5` via `FUN_08000370(...)`

for each discovered slot, and it calls [FUN_0802EA08](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22906)
to validate or probe the companion screenshot-family state.

So this is already more than a filesystem open. It is building the live overlay
selection state used later by the UI.

## 2. The First Strong UI-Visible Consumer Is The Right-Panel Redraw Ladder

The clearest runtime/UI-visible consumer of that seeded state is not in early
boot. It is the redraw ladder around:

- `0x080156D0`
- `0x080157B4`
- `0x080157D0`

captured in:

- [right_panel_redraw_cluster_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_redraw_cluster_force_2026_04_08.c#L1)

When `+0xE10 == 1`, these slices do:

1. `FUN_08034878(0x080BC18B)`
2. store the return byte into `+0xE1B`
3. draw a label with:
   - `FUN_080003b4(&DAT_20008360, 0x080BCAE5, +0xE11)`
4. redraw the right-panel region with `FUN_08008154(...)`
5. clear `+0xE10` to `0xFF`
6. force visible state `10`
7. queue display selectors `0x24` then `0x03`

This is the first place where the seeded overlay state becomes clearly
LCD-visible behavior:

- list enumeration
- current-entry label formatting
- right-panel redraw
- display-state handoff

## 3. `DAT_20000F09` Also Feeds The Metadata Path, But Later

The next strong consumer is the metadata loader:

- [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L41)

which formats:

- `0x080BC859` with `DAT_20000F09`

and then reads:

- a `0x25A`-byte blob into `0x2000044E`
- plus a 5-byte trailer

That is important, but it is not the earliest clearly UI-visible behavior. It
is a later per-index metadata consumer used by the right-panel resource family.

So the current sequencing is:

1. boot-time filesystem normalization in `fatfs_init()`
2. boot/runtime descriptor enumeration in `FUN_08034878(...)`
3. first strong UI-visible use in the right-panel redraw ladder
4. later per-index metadata use in `FUN_08035ED4()`

## 4. What This Means About Remaining Trace Surface

This is the useful milestone:

- we are no longer missing a large unknown control-flow branch between the
  seeded descriptor state and the UI
- the first actual LCD-visible consumer is already identified
- the code-flow between enumeration and visible redraw is now fairly compact

That means we are getting close to the end of what plain code tracing of the
downloaded app can explain on its own.

The remaining uncertainty is shifting toward:

- missing high-flash descriptor/data content behind `0x080BC18B`,
  `0x080BCAE5`, `0x080BC859`, and related families
- exact contents of the missing table/data region
- runtime behavior that depends on those contents rather than on unseen code
  branches

## 5. Best Next Step

The sharpest next software-only move is now:

1. trace the first path that sets `+0xE10 = 1` before the redraw ladder runs
2. compare the later metadata consumer `FUN_08035ED4()` against the same entry
   index to see whether both survive with synthetic descriptor content
3. treat additional plain control-flow hunting as lower yield than descriptor
   content reconstruction

So yes: this is close to the edge of what the downloaded app’s code alone can
tell us. The next remaining gap is increasingly data-shaped, not branch-shaped.
