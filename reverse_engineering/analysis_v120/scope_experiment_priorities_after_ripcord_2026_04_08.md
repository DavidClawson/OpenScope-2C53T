# Scope Experiment Priorities After ripcord Cross-Check

Date: 2026-04-08

Purpose:
- turn the `ripcord` cross-check into concrete bench priorities
- keep the parts of `ripcord` that are useful without inheriting its
  over-strong wire-level assumptions
- connect the cross-check back to the current `osc` packed-state / right-panel
  runtime model

Primary references:
- [ripcord_feedback_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ripcord_feedback_bridge_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_handoff_directness_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_handoff_directness_2026_04_08.md)
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- `/Users/david/Desktop/ripcord/notes/scope_acquisition_spec.md`
- `/Users/david/Desktop/ripcord/notes/fpga_version_evolution.md`

## Executive Summary

The `ripcord` cross-check sharpens the next bench plan in one important way:

1. keep `ripcord`'s staged prerequisite and timer-cadence model
2. keep the newer `osc` queue-split and packed-state runtime model
3. stop prioritizing flat raw-byte sweeps of `0x0B..0x11` by themselves
4. prioritize runtime experiments that stage state, re-enter the shared bank
   emitter, and only then exercise the grouped selector families

Best current read:

- the blocker is more likely "missing preset + queue + timer choreography"
  than "one wrong direct wire-level command byte"

## 1. What ripcord changes for the bench

From the cross-check:

- adopt:
  - staged boot/runtime prerequisites
  - timer-driven re-arm as the better stock match
  - grouped scope-entry families as a useful model
- treat cautiously:
  - exact wire-level wording around `0x0B..0x11`
  - strong pin-role and task-role claims based only on xrefs

So the next experiments should treat the scope command groups as **families to
be reached through stateful runtime entry**, not isolated raw command bytes.

## 2. What the current osc RE says to do instead

The newer `osc` work points to a stock-like staged runtime order:

1. enter or restore a visible top-level scope state (`2`, `5`, or `6`)
2. stage packed scope bytes at `0xF68..0xF6B`
3. re-enter the shared bank emitter at `FUN_0800B908`
4. let the right-panel / acquisition families run from there
5. preserve timer-driven cadence while those families are active

That is a better match to stock than:

- direct `0x0B..0x11` sweeps
- direct `0x27 / 0x28 / 0x2A` sweeps without state staging
- display-loop-triggered acquisition without a stock-like runtime posture

## 3. Recommended Next Bench Experiments

### 3.1 Instrument the live packed-state and panel bytes

Expose these bytes live in firmware or the shell:

- `DAT_20001060..DAT_20001063`
- `0xE1A`
- `0xE1B`
- `0xE1C`
- `0xE1D`
- `0x355`

Without that visibility, it is too easy to think a selector-family experiment
did nothing when it may have staged internal state but failed before the final
hand-off.

### 3.2 Add runtime helpers that stage state and re-enter `FUN_0800B908`

The most useful shell/bench helpers now would be:

- set visible state `6`, then re-enter the shared emitter
- set visible state `5`, then re-enter the shared emitter
- set packed preset `9 / 1 / 2 / 0`, then re-enter
- clear back to visible state `2`, then re-enter

Those are more stock-shaped than another pass of raw `fpga scope entry ...`
experiments.

### 3.3 Prefer grouped family checks after staged entry

Once the runtime posture is staged, then test:

- right-panel/timebase family: `0x26 / 0x27 / 0x28 / 0x2A / 0x2B`
- acquisition family: `0x20 / 0x21`
- mixed trigger-like family: `0x08 / 0x17 / 0x18 / 0x19`

This keeps the selector-family model, but only after stock-like state entry.

### 3.4 Keep timer-driven cadence in the loop

Bench experiments should continue to prefer:

- timer-driven or dedicated-task acquisition triggering
- repeated runtime re-entry and heartbeat checks

over one-shot display-driven polls alone.

## 4. De-prioritized Bench Work

These are still worth keeping around, but they are no longer the top branch:

- brute-force raw `0x0B..0x11` parameter sweeps in isolation
- one-byte-at-a-time wire-level guesses without state staging
- using `ripcord` pin-role claims as the primary hardware map

## 5. Practical Next Firmware Goal

The best next firmware-side experiment is now:

1. add a diagnostic view of `0xF68..0xF6B`, `0xE1A..0xE1D`, and `0x355`
2. add shell helpers that force the `state 6 -> state 5` and
   `state 2 -> 9/1/2/0 -> state 2` posture transitions
3. rerun grouped scope-family experiments only after those staged entries

That is the shortest route from the `ripcord` cross-check to a more stock-faithful
scope-mode bench plan.
