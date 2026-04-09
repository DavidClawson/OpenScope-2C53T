# Scope Top-Level Gate Writers

Date: 2026-04-08

Purpose:
- document the first broader scope-side owners that sit above the already-mapped
  runtime preset families
- distinguish the real runtime controller from the more reset-like entry
  normalizer
- identify the sharpest next branch for reconnecting stock scope UI actions to
  the `state 5 / 2 / 1` runtime families

Primary references:
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)
- [mode_scope_state_cluster_08006840_080076A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006840_080076A0_2026_04_08.txt)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c)

## Executive Summary

The narrower runtime families still hold:

- visible state `5` feeds the timebase / right-panel family
- visible state `2` feeds the acquisition family
- visible state `1` feeds the mixed raw-word / trigger-like family

What is new is the layer above them.

1. `0x08004D60` now looks like the best current **broad runtime controller**
   above those families. It dispatches directly on `DAT_20001060`, writes new
   top-level visible states itself, and mixes selector-bank traffic with panel
   state and packed-state cleanup.
2. `0x08006840` looks different. It primarily seeds or normalizes
   `DAT_20001060..63`, clears right-panel bytes, zeros queue scratch, and then
   re-enters the shared bank emitter. Best current read: **reset / entry
   normalizer**, not the main menu/action owner.

That is a better hierarchy than the earlier model where the `state 5 / 2 / 1`
families looked like the highest useful layer.

## 1. `0x08004D60` is the first strong top-level runtime owner

The broad owner at `0x08004D60` is important for two reasons.

First, it dispatches directly on visible top-level state:

- load `DAT_20001060` at `0x08004D6E`
- jump table on that byte at `0x08004D78`

Second, it is not just a passive consumer of that state. It also **materializes
new visible states** from packed scope state and from panel-side conditions.

### 1.1 It converts packed `0xF69` back into visible `0xF68`

The first branch at `0x08004D90..0x08004DA4`:

- loads `DAT_20001061` (`0xF69`)
- writes `DAT_20001060 = DAT_20001061 + 1`
- clears `DAT_20001063` and the halfword at `0xF69`

Then it branches on the old `DAT_20001061` value:

- old `2` -> `0x080052A6`
- old `1` -> `0x080052B6`
- old `0` -> the larger setup block starting at `0x08004DB6`

This is a stronger bridge than the earlier narrow-family helpers, because it
shows stock promoting packed preset state back into visible top-level scope
state inside a larger owner.

## 2. The strongest direct writer to visible state `5` lives inside `0x08004D60`

The most useful branch for scope-mode reconstruction is currently the
`0x080051A4..0x080051DC` region.

That path:

- checks `0xE1C`
- if `0xE1C == 1`, queues selector `0x2B`
- then writes `DAT_20001060 = 5`
- immediately re-enters `FUN_0800B908`
- then stamps `DAT_20001060 = 12`

This is the cleanest direct runtime path yet from a panel-side condition into
visible state `5`, which is already our strongest timebase / right-panel gate.

There is also a second state-`5` writer in the same broad owner:

- `0x08005298..0x080052A0` clears `0xE1C`
- then writes `DAT_20001060 = 5`

Together those two branches make `0x08004D60` the best current parent owner for
the previously isolated state-`5` family at
[scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md).

## 3. `0x08004D60` also drives adjacent top-level states `4` and `6`

The same owner writes other visible top-level states too:

- `0x08004F08..0x08004F24` writes `DAT_20001060 = 4`, then flips hardware-side
  registers before falling back into the shared emit/re-entry flow
- `0x0800538C..0x08005398` writes `DAT_20001060 = 6` and tail-calls
  `FUN_0800B908`

There is also a follow-up coupling between state `4` and the selector-bank
traffic:

- if `DAT_20001060 == 4`, the exit path at `0x0800558A..0x080055A6` queues
  selector `0x21`

That matters because it means `0x08004D60` is not just a timebase-side owner.
It appears to be a broader scope submenu controller that can feed several
runtime families before the narrower shims take over.

## 4. The mixed `0x1F / 0x20` branch also belongs to `0x08004D60`

The `0x08004E86..0x08004F04` branch still looks like the mixed acquisition /
trigger-side path we had trouble classifying earlier.

It:

- works from `DAT_20001061`, the low nibble of `DAT_20001062`, and
  `DAT_20001063`
- queues `0x1F` then `0x20`
- advances `0xF60`
- then exits through the shared `0x08005586` store that writes
  `DAT_20001061 = 1`

I am still treating the exact meaning of that branch as an inference, but it is
now clearly part of the same broad owner that writes visible states `5`, `4`,
and `6`. That is a stronger result than the earlier view where the mixed family
looked detached from the broader scope controller.

## 5. `0x08006840` looks like a reset / entry normalizer, not the main owner

The second broad region at `0x08006840` looks useful, but different.

Its early cases do not look like live submenu/action ownership. They mostly
stage or normalize packed-visible state before re-entering the shared bank
emitter:

- `0x0800685E..0x0800686E`
  - clear `0xE1C`, `0xE12`, `0xE16`, `0xE1A`
  - write halfword `0x0100` at `0xF68`
- `0x08006904..0x0800690C`
  - write halfword `0x0100`
- `0x0800691C`
  - write halfword `0x0200`
- `0x08006926`
  - write halfword `0x0300`
- `0x08006930..0x0800693A`
  - clear `0x355`
  - write `0x0100`

All of those paths converge at `0x0800693E..0x0800694E`, which:

- clears scratch at `0x20002D50`
- tail-calls `FUN_0800B908`

Best current read:

- `0x08006840` is a **scope entry/reset normalizer**
- it seeds the visible and packed scope-state bundle
- but it is probably **not** the best direct parent of the runtime
  `state 5 / 2 / 1` action families

## 6. Updated hierarchy

The current scope-state hierarchy now looks like this:

1. broad entry/reset normalizer:
   - `0x08006840`
2. broad runtime controller:
   - `0x08004D60`
3. narrower runtime families:
   - state `5` -> `0x08006548` timebase / right-panel
   - state `2` -> `0x08005FCC` acquisition
   - state `1` -> `0x080066AC` mixed raw-word / trigger-like

That is still an inference from static evidence, but it is the sharpest model
so far.

## 7. What This Changes

This means the next pass should **not** spend time re-explaining the already
mapped narrow families.

The better question is now:

- which branches inside `0x08004D60` feed visible states `5`, `4`, and `6`,
  and how do those branches line up with the real scope UI actions?

That is a shorter path to reconstructing stock scope-mode choreography than
continuing to chase standalone packed-state fragments.

## 8. Best Next Move

The next static pass should focus on the inbound branch conditions inside
`0x08004D60`:

1. trace who drives `0xE1C == 1` before the `0x2B -> state 5 -> state 12`
   sequence at `0x080051A4`
2. trace who drives the alternate `state 5` entry at `0x08005298`
3. trace what user-visible path lands in `state 4` before the `0x21` follow-up
   at `0x0800558A`
4. trace what the `state 6` handoff at `0x0800538C` represents in scope UI
5. compare those branches directly against the already-known family meanings:
   - `state 5` -> timebase / right-panel
   - `state 2` -> acquisition
   - `state 1` -> mixed trigger-like

That is now the best static path to a stock-faithful scope transition order we
can later try to reproduce on hardware.

Update after the later raw-window pass:

- the downstream `display_mode` / trigger-posture cluster at raw
  `0x080095AE..0x08009B1E` is now confirmed to sit inside this same broad owner,
  not in a separate upstream controller
- see
  [trigger_posture_cluster_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/trigger_posture_cluster_owner_map_2026_04_08.md)
