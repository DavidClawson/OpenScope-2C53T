# Trigger/Posture Cluster Owner Map 2026-04-08

## Summary

The raw `0x080095AE..0x08009B1E` trigger/posture cluster is no longer best
treated as a floating upstream mystery. Forced-Thumb raw disassembly now shows
that it belongs to the already-known broad runtime controller at:

- raw `0x08008D60`
- decompiled `0x08004D60`

That is the same broad controller already described in:

- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)

So the current best structural read is:

1. `0x08004D60` is the broad visible-state runtime owner
2. the later `display_mode` / trigger-posture logic is an interior branch family
   inside that same owner
3. the posture cluster is therefore downstream of the broad controller, not a
   separate anonymous owner above it

Primary references:

- [display_mode_posture_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_posture_cluster_2026_04_08.md)
- [display_mode_latch_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_latch_map_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. Real function entry: raw `0x08008D60`

The forced-Thumb raw window around the posture cluster shows a clean prologue at
raw `0x08008D60`:

```asm
0x08008D60: stmdb sp!, {r4, r5, r6, r7, r8, lr}
0x08008D64: sub   sp, #24
0x08008D66: movw  r5, #0x00F8
...
0x08008D6E: ldrb.w r0, [r5, #0xF68]
0x08008D74: bhi.w 0x080095A8
0x08008D78: tbh   [pc, r0, lsl #1]
```

Because the archived vendor app is an app-slot image, the corrected mapping is:

- raw `0x08008D60`
- decompiled `0x08004D60`

That matches the broad runtime controller already identified in the earlier
notes.

## 2. The broad controller dispatches on `DAT_20001060` (`F68`)

The jump table at raw `0x08008D78` is the key structural anchor.

With the normal Thumb `TBH` base at `0x08008D7C`, the case entries are:

| Visible state (`F68`) | Raw entry | Best current read |
|---|---:|---|
| `0` | `0x08008D90` | packed-state consume / normalize |
| `1` | `0x08008E24` | mixed raw-word / trigger-like branch |
| `2` | `0x08008E5E` | acquisition-side branch |
| `3` | `0x08008E74` | packed transition helper |
| `4` | `0x08008E86` | mixed `0x1F/0x20` family |
| `5` | `0x0800911C` | right-panel editor branch |
| `6` | `0x080091A4` | right-panel transient handoff |
| `7` | `0x08008F08` | scope-visible mixed branch |
| `8` | `0x080091E6` | trigger/posture-side branch candidate |
| `9` | `0x08008F28` | packed trigger-like branch candidate |

This is useful because the later posture cluster reuses the same state bytes
and the same queue/emitter conventions as the rest of this owner.

## 3. The `display_mode` / posture block is inside this owner

The raw cluster documented in
[display_mode_posture_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_posture_cluster_2026_04_08.md)
starts at:

- raw `0x080095AE`
- clear/set front edge at raw `0x080095AE` and `0x08009732`
- broader posture work through raw `0x08009B1E`

The same forced-Thumb disassembly also shows:

- an epilogue at raw `0x080095A8`
- later branches from the same broader owner targeting blocks beyond that
  epilogue
- a second epilogue only at raw `0x08009B3E`

So raw `0x080095AE` is not the start of a new normal function. It is an
interior branch target inside the broader `0x08008D60` controller.

That keeps the older cautious wording intact:

- "likely an interior dispatch slice"

but the owning function is now clearer:

- owner = raw `0x08008D60` / decompiled `0x08004D60`

## 4. This ties the posture cluster directly to the known right-panel controller

The same owner already contains the established right-panel branches:

- raw `0x0800911C` -> visible `state 5` right-panel editor
- raw `0x080091A4` -> visible `state 6` transient handoff

and the same function later contains the `display_mode` / trigger-posture
cluster at raw `0x080095AE..0x08009B1E`.

So the local hierarchy is now:

1. broad runtime controller enters visible `state 5` / `6`
2. that same controller later reaches the `display_mode` latch and the
   `F6B`-driven posture branches
3. the later redraw/resource owner consumes the resulting latch/posture bytes

This is a cleaner model than:

- right-panel controller here
- anonymous posture owner somewhere above it
- redraw/resource owner later

The posture logic is not "somewhere above it." It is part of the same broad
controller body.

## 5. Why this is useful

This narrows the search surface meaningfully.

We no longer need to ask:

- "what unknown owner reaches raw `0x080095AE`?"

The better question is now:

- "which visible-state case inside `0x08004D60` reaches the later posture
  sub-branches, and how do `F6B`, `+0x23A`, `+0x354`, `+0x35`, `+0x3C`, and
  `+0x40` stage that trigger-side posture before the redraw/resource family
  consumes it?"

That is a much smaller problem.

## 6. Corrected inbound owner: visible `state 9`, not `state 8`

The next raw pass corrects the earlier local overread.

Visible `state 8` is only a tiny cleanup shim at:

- raw `0x080091E6`
- decompiled `0x080051E6`

and it only:

1. writes visible state `0`
2. re-enters the shared emitter

The real staged owner above the later posture cluster is visible `state 9`:

- raw `0x08008F28`
- decompiled `0x08004F28`

The key reason is that visible `state 9` contains the real staged path:

1. `F69` phase gate
2. low-nibble `F6A` sub-dispatch
3. direct targets into:
   - `0x080095AE`
   - `0x080095C0`
   - `0x080095D8`
   - `0x0800964C`
   - `0x08009702`
4. later re-entry through `0x08009218`, which reaches the `+0x4C` control-word
   edits and the shared selector sink

So the best current read is:

- visible `state 8` = cleanup/reset shim
- visible `state 9` = the real preview/posture owner above
  `0x080095AE..0x08009B1E`

See:

- [state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md)

## 7. What this changes

This corrects the local mental model in a useful way:

- `display_mode` is still a downstream latch
- the trigger/posture cluster is still real
- both still belong to the known broad runtime owner at `0x08004D60`
- but the immediate staged owner above the later cluster is now visible
  `state 9`, not `state 8`

That is steady progress because it converts a vague "upstream mystery" into a
single-owner internal case-map problem with a much cleaner staged path.
