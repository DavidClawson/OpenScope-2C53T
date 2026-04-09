# Scope Owner Start Map

Date: 2026-04-08

Purpose:
- correct the earlier owner-start guesses for the packed scope-state handlers
- separate the broad scope submenu owner from the narrower transition shims
- capture the best next trace target for mapping stock UI actions back onto the
  selector-bank traffic we still have not reproduced at runtime

Primary references:
- [scope_preset_owner_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_preset_owner_force_2026_04_08.c)
- [mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08004D40_08005020_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_08005020_2026_04_08.txt)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [scope_preset_owner_families_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_preset_owner_families_2026_04_08.md)

## Executive Summary

The earlier broad-owner anchor at `0x08003210` was close, but wrong. The real
function entry is `0x08003148`, confirmed by the raw prologue in
[mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt).

That matters because `FUN_08003148` is now the best current static bridge
between:

- visible packed scope state at `0xF68..0xF6B`
- selector-bank emission through `0x20002D6C`
- the mixed scope transition families that eventually reach `0x13/0x14`,
  `0x17..0x19`, `0x20/0x21`, `0x27/0x28`, `0x2A`, and `0x2C`

The narrower owners at `0x08006418` and `0x08006548` are still important, but
they now look like specialized `9 -> 2` transition shims for packed selector
targets `5` and `2`, not the main scope submenu owner.

## 1. Corrected owner starts

The current best owner map is:

- `0x08003148` -> broad packed-scope owner
- `0x08004D60` -> side owner with top-level mode writes and `0x1C/0x1F/0x20`
- `0x08006120` -> dynamic raw-word builder plus scope-side toggle path
- `0x080062F8` -> sibling cleanup / normalization owner
- `0x08006418` -> packed-selector-5 transition family
- `0x08006548` -> packed-selector-2 transition family

The most important correction is the first one. The older shorthand
`0x08003210..0x080034F0` should now be read as "interior of the broader
`FUN_08003148` body," not as the real function start.

## 2. Broad owner: `FUN_08003148`

`FUN_08003148` switches on `DAT_20001060 - 1` and spans several distinct
scope-side families inside one body.

Recovered selector families in this one owner:

- `0x08, 0x17, 0x18, 0x19`
- `0x20, 0x21`
- `0x27, 0x28`
- `0x2A`
- `0x2C`
- `0x13, 0x14`

It also emits a raw `0x0508` word to the `0x20002D74` TX-word queue.

That combination is stronger evidence than before that this owner is not a tiny
transition helper. It looks like a real scope submenu / state owner that spans
multiple user-facing adjustment families.

## 3. What each owner currently looks like

### `0x08003148`

Best current read:

- broad scope submenu owner
- owns the packed-state edits on `DAT_20001062` and `DAT_20001063`
- drives several selector banks, not just one family
- likely sits above the narrower confirm / collapse helpers

Concrete hints from the forced decompile:

- one branch decrements `DAT_20000F51`, clamps `DAT_20001062`, then queues
  `0x08`, `0x17`, `0x18`, `0x19`
- another branch decrements `DAT_20001062` and queues `0x20`, then `0x21`
- another branch steps `DAT_20000F14` / `bRam20000F15` and queues `0x27`, then
  `0x28`
- another branch turns `DAT_20000F14` into `1` and queues `0x2A`
- another branch decrements `DAT_20001058` and queues `0x2C`
- another branch decrements or normalizes `DAT_20001062` and queues `0x13`,
  then `0x14`

### `0x08004D60`

Best current read:

- side owner
- mixes top-level mode writes with measurement / display-state work
- still relevant, but not the best first target for the missing scope-entry path

### `0x08006120`

Best current read:

- dynamic raw-word family
- still owns the `0x050C/0x050D/0x050E/0x0517/0x0510/0x0515/0x0511/0x0516`
  bank
- also owns a scope-side toggle path that queues selector `0x02`
- not broad enough to explain the larger packed-state choreography by itself

### `0x080062F8`

Best current read:

- sibling cleanup / normalization path
- mirrors the `0x08006120` toggle family
- adjusts the same scope-side selector space, but still looks secondary to the
  broader owner

### `0x08006418`

Best current read:

- packed-selector-5 transition family
- stamps `0x00050109`
- when `DAT_20001062 != 5`, queues `0x13`, then `0x14`
- when `DAT_20001062 == 5`, clears the latch and collapses back to visible
  state `2`

### `0x08006548`

Best current read:

- packed-selector-2 transition family
- stamps `0x00020109`
- can also stamp `sub2 = 5`
- when `DAT_20001062 != 2`, queues `0x13`, then `0x14`
- when `DAT_20001062 == 2`, clears the latch and collapses back to visible
  state `2`

## 4. Practical implication

This shifts the next search again, but in a good way.

The highest-value next target is no longer "every helper that writes packed
state." It is specifically the internal case map inside `FUN_08003148`, because
that owner already spans the selector families that resemble the missing scope
sequencing we have been trying to reproduce on hardware.

## 5. Command-semantic hints from existing protocol work

Some of the broad-owner selector families already line up with meanings we had
recovered separately in
[FPGA_PROTOCOL_COMPLETE.md](/Users/david/Desktop/osc/reverse_engineering/FPGA_PROTOCOL_COMPLETE.md).

That gives the following working read:

- `0x17..0x19` -> trigger-family updates
- `0x20/0x21` -> acquisition run-mode / sample-depth family
- `0x27/0x28` -> timebase-family updates
- `0x13/0x14` -> packed-selector commit / collapse family
- `0x2A` -> still unresolved, but it clusters with the `DAT_20000F14` branch and
  looks more like a submenu confirm / special mode action than a base scope
  config
- `0x2C` -> standalone mode-8 family, still unresolved, but now clearly owned by
  the broad packed-state path rather than some unrelated helper

So the next question is not "what do those command numbers mean in the abstract"
as much as:

- which concrete scope UI actions inside `FUN_08003148` drive the trigger family
- which ones drive acquisition depth / run mode
- which ones drive timebase mode / period
- and which ones only participate in packed-state collapse back to visible scope
  state `2`

## 6. Best next move

Next static pass:

1. map the `FUN_08003148` case arms to concrete user-visible scope actions
2. identify which branch owns:
   - `0x17..0x19`
   - `0x20/0x21`
   - `0x27/0x28`
   - `0x2A`
   - `0x2C`
3. compare those branches against the runtime shell experiments that still leave
   scope mode silent
4. only then fold the narrower `0x08006418` / `0x08006548` transition shims
   back in as confirm/collapse helpers

That is the shortest path from the recovered packed presets to a stock-faithful
scope transition order we can actually test in firmware.
