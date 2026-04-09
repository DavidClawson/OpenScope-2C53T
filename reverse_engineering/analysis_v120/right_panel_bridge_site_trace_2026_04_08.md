# Right-Panel Bridge Site Trace 2026-04-08

Purpose:
- trace only the two top bridge sites named in the latest bridge notes:
  - decompiled `0x0800650C` / raw `0x0800A50C`
  - decompiled `0x08006060` / raw `0x0800A060`
- answer two concrete questions:
  - whether `0x0800650C` can be tied directly to shared-emitter re-entry
  - whether `0x08006060` is the first plausible fixed-`0x0501`
    materializer after that

Primary references:
- [right_panel_final_word_bridge_focus_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_final_word_bridge_focus_2026_04_08.md)
- [right_panel_selector_to_wire_bridge_candidates_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_selector_to_wire_bridge_candidates_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08006010_080060A0_2026_04_08.txt)
- [mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_080064E0_08006650_2026_04_08.txt)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)

Address note:
- code addresses in the archived app use the app-slot base
- raw address = decompiled address `+ 0x4000`

## Executive Summary

The answer is narrower than the earlier bridge notes implied.

1. `0x0800650C` is not itself a shared-emitter re-entry site.
   It is the non-collapse arm of the same `sub2 = 5` owner whose sibling arm
   does re-enter `FUN_0800B908` at raw `0x0800A4FC..0x0800A500`.
   The concrete behavior at `0x0800650C` is:
   - stage packed halfword `0x0501` at `0xF69`
   - clear `F6B`
   - enqueue selector bytes `0x13`, then `0x14` to `0x20002D6C`
   - return

2. `0x08006060` is still the first plausible fixed-`0x0501` materializer in the
   adjacent cluster, but that needs one refinement:
   - `0x08006060` itself is an interior dispatch point inside the broader
     `0x08006010` owner
   - the actual fixed-`0x0501 -> 0x20002D74` materialization starts a little
     later at raw `0x0800A0CA`

So the strongest current bridge is:
- concrete `sub2 = 5` owner at raw `0x0800A4E0..0x0800A542`
- nearby shared-emitter re-entry on its collapse arm only
- then the adjacent `0x08006010` owner family as the first plausible fixed
  `0x0501` raw-word materializer family

What is still missing is the direct control-flow edge from the `sub2 = 5`
collapse/re-entry into the later fixed-`0x0501` materializer path.

## 1. `0x0800650C` belongs to the same owner as a real re-entry site

The raw `sub2 = 5` cluster is:

- raw `0x0800A4E0..0x0800A542`
- decompiled `0x080064E0..0x08006542`

Its first split is on `DAT_20001062` / `F6A`:

- raw `0x0800A4E0`: `ldrb.w r1, [r0, #0xf6a]`
- raw `0x0800A4E4`: compare against `5`

From there the owner has two clearly different arms.

### Collapse arm: real shared-emitter re-entry

If `F6A == 5`, the owner does:

- clear latch `+0x355`
- set visible state `F68 = 2`
- clear `F69`
- clear `F6B`
- tail-call shared emitter

Concrete sites:

- raw `0x0800A4E8..0x0800A4FC`
- tail-call at raw `0x0800A500 -> 0x0800B908`

This is the already-grounded `sub2 = 5` collapse/re-entry path from
[runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md).

### Non-collapse arm: `0x0501`, then `0x13`, `0x14`

If `F6A != 5`, the same owner instead does:

- raw `0x0800A504`: `movw r1, #0x501`
- raw `0x0800A50C`: `strh.w r1, [r0, #0xf69]`
- raw `0x0800A51A`: clear `F6B`
- raw `0x0800A522..0x0800A52E`: enqueue selector byte `0x13`
- raw `0x0800A532..0x0800A542`: enqueue selector byte `0x14`

That means `0x0800650C` is concretely tied to the same owner that contains a
real shared-emitter re-entry, but it is not itself the re-entry site. It is the
alternate arm that stages packed `0x0501` state and emits the internal
`0x13/0x14` selector pair.

## 2. What this means for the bridge claim around `0x0800650C`

This narrows the earlier bridge wording:

- strong claim we can keep:
  - `0x0800650C` is part of the correct `sub2 = 5` detailed-path owner
  - that owner is concretely adjacent to a real shared-emitter re-entry

- strong claim we should not make:
  - "`0x0800650C` itself tail-calls the shared emitter"

The better current interpretation is:

- the `sub2 = 5` owner has a collapse/re-entry arm
- and a non-collapse `0x0501 + 0x13/0x14` arm

So `0x0800650C` is a valid bridge anchor, but only at the owner-family level,
not as a direct re-entry site.

## 3. The adjacent `0x08006010` owner has the same structural pattern

The nearby `sub2 = 3` cluster at:

- raw `0x0800A010..0x0800A11A`
- decompiled `0x08006010..0x0800611A`

shows the same two-arm pattern:

- collapse arm:
  - raw `0x0800A018..0x0800A030`
  - sets visible state `2`
  - clears packed bytes
  - tail-calls `0x0800B908`

- non-collapse arm:
  - raw `0x0800A076..0x0800A082`
  - stages packed halfword `0x0301` at `F69`
  - then emits selector bytes `0x13`, `0x14`

This matters because it shows the bridge shape is not unique to `sub2 = 5`.
The cluster repeatedly alternates between:

- collapse + re-entry
- or packed halfword staging + `0x13/0x14`

That makes the later raw-word materializer hypothesis more defensible at the
owner-family level than it would be from the `sub2 = 5` branch alone.

## 4. `0x08006060` is best treated as an interior site, not the first write

The earlier notes treated `0x08006060` as the fixed-`0x0501` helper entry.
The raw disassembly sharpens that.

Inside the broader `0x08006010` owner:

- raw `0x0800A04E..0x0800A0C2` is a small dispatch/setup arm
- raw `0x0800A060` is just an interior point inside that dispatch
- the first actual fixed-`0x0501` raw-word materialization starts at:
  - raw `0x0800A0CA`
  - raw `0x0800A0DA`: `movw r2, #0x501`
  - raw `0x0800A0E0`: `strh r2, [r1, #0]` to `0x20002D54`
  - raw `0x0800A0E6`: enqueue to `0x20002D74`

Then the same arm queues:

- selector `0x1D`
- selector `0x1B`

So the precise statement is:

- the broader `0x08006060` site family is still the first plausible fixed
  `0x0501` materializer after the re-entry hypothesis
- but instruction-for-instruction, the materialization is really the later
  `0x080060CA..0x080060E6` sequence

## 5. Is `0x08006060` the first plausible `0x0501` materializer after the
`sub2 = 5` bridge?

Yes, with one explicit caveat.

### Why "yes" still holds

It is still the best adjacent fixed-`0x0501` raw-word sink because:

1. it is the first nearby owner family that provably does:
   - `0x0501 -> 0x20002D54 -> 0x20002D74`
2. it sits in the same narrow helper cluster family already tied to:
   - packed presets
   - `0x13/0x14`
   - shared-emitter re-entry
3. it matches the detailed-path bridge requirement better than `0x08006120`,
   which materializes dynamic `0x050x` words instead of fixed `0x0501`

### Why the caveat matters

What is still not proven is the exact runtime edge:

- `sub2 = 5` collapse/re-entry at `0x0800A4FC -> 0x0800B908`
  followed by
- execution of the later fixed-`0x0501` path in the `0x08006010` owner

So `0x08006060` is the first plausible materializer after that, but only as a
cluster-level bridge hypothesis. The direct path through `FUN_0800B908` into
that fixed-`0x0501` arm remains ungrounded.

## 6. Tightened conclusion

The bridge can now be stated more precisely:

- `0x0800650C` is a valid detailed-path bridge anchor because it sits in the
  exact `sub2 = 5` owner whose sibling arm re-enters the shared emitter
- but `0x0800650C` itself is not the re-entry site; it is the alternate
  `0x0501 + 0x13/0x14` arm
- the first adjacent fixed-`0x0501` raw-word materializer is still the broader
  `0x08006060` owner family, with the actual enqueue sequence beginning at raw
  `0x0800A0CA`

So the best next trace is narrower than before:

1. follow the concrete edge from `FUN_0800B908` back into the `0x08006010`
   owner family
2. determine whether shared-emitter re-entry can really land on the fixed
   `0x0501` arm at raw `0x0800A0CA`
3. only after that, decide whether the detailed path feeds:
   - fixed `0x0501` via the `0x08006010` owner, or
   - dynamic `0x050x` via `0x08006120`
