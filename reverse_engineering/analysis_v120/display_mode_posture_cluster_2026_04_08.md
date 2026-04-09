# Display Mode Posture Cluster 2026-04-08

## Summary

The region beginning at raw `0x080095AE` is now better understood as a **small
trigger-side posture cluster**, not just an isolated `display_mode` setter.

The useful corrections are:

1. the `display_mode` clear/set pair at:
   - raw `0x080095AE`
   - raw `0x08009732`
   is only the front door to a larger local state cluster
2. that same cluster fans out on `DAT_20001063` (`F6B`) and manipulates real
   trigger-side scope state:
   - `active_channel` (`+0x16`)
   - `trigger_run_mode` (`+0x17`)
   - `trigger_edge` (`+0x18`)
3. it also normalizes a smaller downstream posture bundle around:
   - `+0x35`
   - `+0x3C`
   - `+0x40`
   - `+0x23A`

So the current best read is:

- the later redraw/resource owner still consumes a render/overlay latch
- but the immediate upstream owner now looks less like generic redraw plumbing
  and more like a **trigger-side preview/control cluster** that also controls
  that latch

Primary references:

- [display_mode_latch_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_latch_map_2026_04_08.md)
- [trigger_posture_cluster_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/trigger_posture_cluster_owner_map_2026_04_08.md)
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
- [scope_dispatcher_trace_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_dispatcher_trace_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. Structural read: likely an interior dispatch slice, not a clean function

Raw `0x080095A8` is an epilogue:

```asm
0x080095A8: add sp, #24
0x080095AA: pop {r4, r5, r6, r7, r8, pc}
```

and raw `0x080095AE` immediately begins a new logic block with no prologue:

```asm
0x080095AE: ldrb.w r0, [r5, #52]
...
```

That makes the most conservative structural read:

- raw `0x080095AE..` is probably an interior dispatch/label slice inside a
  larger normalized handler family
- not a clean standalone C-style function entry

This matters because the next question is likely:

- "who dispatches into this slice?"

not:

- "who calls this as a normal BL target?"

## 2. The `display_mode` latch is only the front edge

The first two entry points remain:

- raw `0x080095AE`
  - clear `display_mode` back to `0` when already set
  - otherwise branch to the set path
- raw `0x08009732`
  - set `display_mode = 1`
  - if low nibble of `+0x354` is `3`, clear that nibble and zero `0x20002D50`

That part is already documented in:

- [display_mode_latch_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_latch_map_2026_04_08.md)

The new result is that both paths immediately converge into a broader local
posture cluster beginning at raw `0x08009758`, not a one-off render toggle.

## 3. `F6B` splits the cluster into real trigger-side control paths

At raw `0x080095C0`, the cluster reads:

- `DAT_20001063` (`F6B`)
- `*(base + 0x23A)`

and then branches by `F6B`.

### `F6B == 2`: channel-select side

Raw `0x08009874..0x08009892`:

- reads `*(base + 0x16)` = `active_channel`
- toggles it between `0` and `1`
- queues byte `6` to `0x20002D78`

So this branch now looks like a channel-select side path, not a generic redraw.

### `F6B == 1`: trigger-edge side

Raw `0x08009894..0x080098B6`:

- reads `*(base + 0x18)` = `trigger_edge`
- toggles it between `0` and `1`
- queues byte `7` to `0x20002D78`

So this branch now looks like a trigger-edge side path.

### `F6B == 0`: run-mode side

Raw `0x080095EE..0x08009612`:

- reads `*(base + 0x17)` = `trigger_run_mode`
- increments it modulo `3`
- writes the result back
- if `scope_active_flag == 0`, forces `scope_active_flag = 1`

This does not directly show the final trigger-byte send in the same small slice,
but it clearly is a trigger-run-mode control path. It also fits the nearby
broader scope/acquisition logic where trigger byte `8` is paired with the
advanced scope selector family, as already summarized in:

- [scope_dispatcher_trace_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_dispatcher_trace_2026_04_08.md#L102)

So the safest current phrasing is:

- `F6B == 0` is the run-mode/activation side of the same trigger-side cluster

## 4. `+0x23A` looks like a local preview/arming flag byte

The same cluster manipulates `*(base + 0x23A)` directly.

### `F6B != 0` arm/disarm path

At raw `0x08009818..0x080098C6`:

- if `F6B == 0`, it chooses between setting bit `0` or `1` in `+0x23A`
- if a sign-bit condition on `+0x23A` is met, it instead clears bit `0` or `1`
- if `+0x23A` remains nonzero, it calls `FUN_08022FC0()`
- if `+0x23A` collapses to zero, it falls into the `+0x354` nibble-sanitize path

So `+0x23A` now looks more like a small local preview/arming flag byte than a
generic configuration field.

This is useful because it ties the `display_mode` latch back to a more active
control posture instead of a passive UI setting.

## 5. The cluster also normalizes `+0x35`, `+0x3C`, and `+0x40`

The local block after the latch pair also manipulates three neighboring bytes
in a way that is hard to reconcile with their older labels.

### `+0x35`

Raw `0x08009702..0x08009714`:

- compares `*(base + 0x35)` against `F6B`
- clears `+0x35` back to `0` when they match

That looks more like a small posture mirror or local preview selector than a
generic "display sub config."

### `+0x3C`

Raw `0x08009716..0x08009730`:

- compares `*(base + 0x3C)` against `F6B`
- when unequal, copies `F6B` into `+0x3C`
- when equal, clears `+0x3C` back to `0`

That is much more toggle/mirror-like than the older `measurement_config` label.

### `+0x40`

Raw `0x08009758..0x08009812`:

- gates on `+0x3C`
- reads `*(base + 0x40)` as a pointer/list head
- walks linked elements via offsets `+4`, `+8`, and `+16`
- clears the pointer back to `0` when exhausted

So the current `measurement_state` label at `+0x40` is also looking stale in
this path. At minimum, that field participates in a pointer-driven preview or
work-list posture here.

## 6. Why this looks trigger-like, not generic

The strongest reason is the field mix.

This cluster directly manipulates:

- `active_channel`
- `trigger_run_mode`
- `trigger_edge`
- `scope_active_flag`
- `F6B`
- `+0x23A`

That is a much better fit for the unresolved mixed trigger-like family than for
a generic render helper.

This also lines up well with the broader static model in:

- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)

where the unresolved visible-state-`1` family already looked like a mixed
trigger-side preview path.

So the current best read is:

- raw `0x080095AE..0x08009B1E` contains a downstream trigger-side posture and
  preview/control cluster
- the `display_mode` latch is part of that cluster, not a separate owner

## 7. What this changes

This sharpens the next trace surface a lot.

There is also one structural correction now:

- the cluster really is an interior branch family
- but the owning function is now identified as the broad runtime controller at
  raw `0x08008D60` / decompiled `0x08004D60`

See:

- [trigger_posture_cluster_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/trigger_posture_cluster_owner_map_2026_04_08.md)

The more useful question is no longer:

- "who sets `display_mode` before the redraw owner?"

It is now:

- "which upstream scope family routes into this trigger-side posture cluster,
  and how do `F6B`, `+0x23A`, `+0x354`, `+0x35`, `+0x3C`, and `+0x40` stage the
  preview/control posture before the later redraw/resource owner consumes it?"

That is a more coherent next branch than following `display_mode` alone.

## Best Next Move

1. Stay inside the broad runtime controller at raw `0x08008D60` /
   decompiled `0x08004D60`, but treat visible `state 9`, not `state 8`, as the
   strongest staged owner above raw `0x080095AE..0x08009B1E`.
   See:
   - [state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md)
2. Compare this cluster directly against the mixed trigger-like family already
   identified in:
   - [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
   - [scope_owner_ui_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_owner_ui_action_map_2026_04_08.md)
3. Treat the current labels for:
   - `+0x35`
   - `+0x3C`
   - `+0x40`
   as provisional until that upstream owner is clearer.
