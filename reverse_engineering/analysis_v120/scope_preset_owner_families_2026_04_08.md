# Scope Preset Owner Families

Date: 2026-04-08

Purpose:
- group the newly recovered `0xF68..0xF6B` preset helpers by behavior
- distinguish broad scope-submenu owners from narrower collapse/preset helpers
- identify the most likely next trace points for mapping presets to user-visible
  scope actions

Primary references:
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [mode_scope_state_cluster_08003210_080034F0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003210_080034F0_2026_04_08.txt)
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)
- [mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D70_08004F50_2026_04_08.txt)
- [mixed_scope_handler_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mixed_scope_handler_bridge_2026_04_08.md)

## Executive Summary

The current preset helpers fall into three behavior families:

1. a broad scope-submenu owner rooted at `0x08003148`, with the previously
   noted `0x08003210..0x080034F0` region sitting inside it
2. narrow `9 -> 2` transition helpers for packed selector `3`
3. narrow `9 -> 2` transition helpers for packed selectors `5`, `2`, and a new
   sibling target `4`

That is useful because it suggests the next trace should not start from every
raw preset write equally. The best owner to reconstruct first is the broad
`FUN_08003148` body, because it drives multiple selector-bank
families and looks much more like a real user-facing scope submenu controller.

## 1. Broad owner: `0x08003148`

This region is the strongest candidate for a true scope-submenu owner rather
than a tiny transition shim.

Correction:

- the earlier shorthand `0x08003210..0x080034F0` was an interior slice
- the raw prologue in
  [mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080030C0_080031A0_2026_04_08.txt)
  shows the actual function entry at `0x08003148`

Evidence:

- updates `DAT_20001062`
- updates `DAT_20000E1C` / `DAT_20000E1D`
- updates `DAT_20000F60`
- steps `DAT_20001063`
- emits several different selector families through `0x20002D6C`
- emits a raw `0x0508` word to `0x20002D74`

Recovered queue families in this one region:

- `0x08, 0x17, 0x18, 0x19`
- `0x20, 0x21`
- `0x27, 0x28`
- `0x2A`
- `0x2C`

That is a strong sign this owner spans several scope-side submenus or
transition branches, not just one isolated mode.

Current best read:

- `FUN_08003148` is the most promising place to map concrete UI
  actions such as trigger/range/mode submenus onto packed selector values

## 2. Narrow owner family A: packed selector `3`

The `0x08006010..0x080060A0` family is narrower and cleaner.

Recovered behavior:

- collapse to visible scope state `2` when `DAT_20001062 == 3`
- otherwise stamp composite preset `9/1/3/0`
- or stamp packed preset `sub1=1, sub2=3`
- then emit `0x13`, `0x14`

That makes it look less like a full submenu owner and more like a dedicated
transition family for one packed selector target.

Best current read:

- this family likely corresponds to one specific scope subview/confirm cycle,
  not the entire scope menu surface

## 3. Narrow owner family B: packed selectors `5` and `2`

The `0x080064E0..0x08006650` family is the sibling of the previous one.

Recovered behavior:

- collapse to visible scope state `2` when `DAT_20001062 == 5`
- collapse to visible scope state `2` when `DAT_20001062 == 2`
- otherwise stamp composite preset `9/1/2/0`
- or stamp packed presets `sub1=1, sub2=5` and `sub1=1, sub2=2`
- then emit `0x13`, `0x14`

That suggests selector targets `2`, `3`, and `5` are a matched cluster in the
scope transition machinery.

The later runtime call-site pass extends that cluster again:

- `0x08006720` stamps `9 / 1 / 4 / 0`
- `0x08006704` can collapse that path back to visible scope state `2`

So the better current cluster is `2 / 3 / 4 / 5`, not only `2 / 3 / 5`.

## 4. Side owner: top-level mode writers `4` and `5`

The `0x08004D70..0x08004F50` region contributes direct top-level mode writes:

- `DAT_20001060 = DAT_20001061 + 1`
- `DAT_20001060 = 4`

It also mixes in queue families such as:

- `0x1C`
- `0x1F`, `0x20`

This makes it relevant, but it still looks more like a side owner or a sibling
transition path than the best first target.

## 5. Best next move

One useful correction from the latest pass:

- these preset helpers re-enter `FUN_0800B908` directly at runtime
- they are not just detached state shims

Reference:
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)

Priority order for the next static pass:

1. reconstruct the larger owner at `0x08003148`
2. map which packed selector values inside that owner correspond to:
   - `0x17..0x19`
   - `0x20, 0x21`
   - `0x27, 0x28`
   - `0x2A`
   - `0x2C`
3. only after that, fold the narrower `selector 3 / 5 / 2` helper families back
   in as specialized transition shims

That gives the shortest path from the recovered raw state presets to something
we can translate back into stock-faithful firmware sequencing.
