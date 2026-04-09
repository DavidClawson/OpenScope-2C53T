# State 9 Preview/Posture Case Map 2026-04-08

## Summary

The strongest correction from this pass is that the later posture cluster at:

- raw `0x080095AE..0x08009B1E`
- decompiled `0x080055AE..0x08005B1E`

is **not** primarily owned by visible `state 8`.

Visible `state 8` is only a small cleanup shim:

- raw `0x080091E6`
- decompiled `0x080051E6`

It just forces `DAT_20001060 = 0` and re-enters the shared emitter.

The real staged owner above the later posture cluster is visible `state 9`:

- raw `0x08008F28`
- decompiled `0x08004F28`

That branch is much richer:

1. it gates on `DAT_20001061` (`F69`)
2. it dispatches on the low nibble of `DAT_20001062` (`F6A`)
3. one side of that dispatch reaches the later `display_mode` / trigger-posture
   cluster
4. another side builds a larger preview/bitmap bank
5. a later phase re-enters through `0x08009218` and edits the control word at
   `+0x4C`

So the current best read is:

- `state 8` = cleanup/reset shim
- `state 9` = staged preview/posture owner above the later cluster

Primary references:

- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [trigger_posture_cluster_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/trigger_posture_cluster_owner_map_2026_04_08.md)
- [display_mode_posture_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_posture_cluster_2026_04_08.md)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. Correction: visible `state 8` is only a shim

The top-level `TBH` table still dispatches visible `state 8` to:

- raw `0x080091E6`
- decompiled `0x080051E6`

But the body there is tiny:

```asm
0x080091E6: movs    r0, #0
0x080091E8: strb.w  r0, [r5, #0xF68]
0x080091EC: b.n     0x08009392
```

So this branch does **not** own the later posture cluster in any meaningful
way. It just:

1. forces visible state `0`
2. re-enters the shared emitter path

That makes the earlier "state 8 is the strongest direct owner" reading too
strong.

## 2. Visible `state 9` is the real staged owner

Visible `state 9` starts at:

- raw `0x08008F28`
- decompiled `0x08004F28`

Its first gate is `DAT_20001061` (`F69`):

```asm
0x08008F28: ldrb.w  r0, [r5, #0xF69]
0x08008F2C: cmp     r0, #1
0x08008F2E: bhi.w   0x08009218
0x08008F32: adds    r0, #1
0x08008F36: strb.w  r0, [r5, #0xF69]
0x08008F3A: bne.w   0x08009B24
```

That gives the current working phase model:

1. if `F69 > 1`, `state 9` skips directly into a later control phase at
   `0x08009218`
2. if `F69 == 0`, it increments to `1` and immediately exits through the shared
   selector sink at `0x08009B24`
3. if `F69 == 1`, it increments to `2` and then continues into the richer
   `F6A` sub-dispatch

So `F69` now looks like a real phase/stage byte inside visible `state 9`, not
just a passive packed-state side field.

## 3. First rich split: low nibble of `F6A`

Once `F69` reaches `2`, visible `state 9` dispatches on the low nibble of
`DAT_20001062` (`F6A`):

```asm
0x08008F3E: ldrb.w  r3, [r5, #0xF6A]
0x08008F42: and.w   r0, r3, #0xF
0x08008F46: cmp     r0, #2
0x08008F48: beq.w   0x08009B24
0x08008F4C: cmp     r0, #7
0x08008F4E: bhi.w   0x08009B1E
0x08008F52: tbh     [pc, r0, lsl #1]
```

Decoded low-nibble targets:

| `F6A & 0xF` | Raw target | Decompiled target | Best current read |
|---|---:|---:|---|
| `0` | `0x080095AE` | `0x080055AE` | `display_mode` latch clear/set front edge |
| `1` | `0x080095C0` | `0x080055C0` | `+0x23A` preview/arming flag path |
| `2` | `0x08009B1E` | `0x08005B1E` | early exit |
| `3` | `0x08008F66` | `0x08004F66` | preview/bitmap subfamily |
| `4` | `0x08008F66` | `0x08004F66` | same preview/bitmap subfamily |
| `5` | `0x080095D8` | `0x080055D8` | direct `F6B` trigger/channel control path |
| `6` | `0x0800964C` | `0x0800564C` | pointer/list drain via `+0x3C/+0x40` |
| `7` | `0x08009702` | `0x08005702` | local mirror/cleanup via `+0x35/+0x3C` |

This is the cleanest ownership result of the pass:

- the later posture cluster is not a free-floating neighbor
- it is one member of a larger visible-state-`9` subfamily keyed by `F6A`

## 4. The posture cluster is now best read as `state 9`, `F6A low nibble = 0/1/5/6/7`

This reframes the later cluster in a useful way.

The known posture-side branches now sit under visible `state 9` like this:

- `F6A low nibble = 0`
  - raw `0x080095AE`
  - `display_mode` clear/set front edge
- `F6A low nibble = 1`
  - raw `0x080095C0`
  - `+0x23A` preview/arming flag path
- `F6A low nibble = 5`
  - raw `0x080095D8`
  - direct `F6B`-driven trigger/channel side path
- `F6A low nibble = 6`
  - raw `0x0800964C`
  - pointer/list drain using `+0x3C/+0x40`
- `F6A low nibble = 7`
  - raw `0x08009702`
  - local mirror cleanup using `+0x35/+0x3C`

So the better question is no longer:

- "which visible state reaches the posture cluster?"

It is now:

- "which runtime/UI condition drives visible `state 9` with a specific low
  nibble in `F6A`?"

That is a much tighter problem.

## 5. The preview/bitmap side is `F6A low nibble = 3/4`

The paired `3/4` cases both land at:

- raw `0x08008F66`
- decompiled `0x08004F66`

That branch is not the same as the later posture cluster. It is the broader
preview/bitmap side of visible `state 9`.

Useful signs:

1. it immediately switches on `F6B`
2. it uses a nested `TBH`
3. it copies many inverted bytes into the `+0x356..` roll-buffer region
4. it touches the byte at `+0x14` and later shared sinks

The current safest read is:

- `F6A low nibble = 3/4` is the preview-bank / bitmap-building side of visible
  `state 9`
- the posture cluster at `0x080095AE..` is a sibling branch, not the same case

The next raw pass tightens that further:

- visible `state 9` is entered through concrete visible-state-`2` promotion
  helpers that stamp `9 / 1 / 2 / 0`, `9 / 1 / 3 / 0`, or `9 / 1 / 4 / 0`
- the `F6A = 3 / 4` side is itself split by `F6B` into:
  - `F6B = 0` heavy preview-bank rebuild
  - `F6B = 2` normalize/hardware-latch path
  - `F6B = 1 / 3 / 4` small toggle shims

See:

- [state9_entry_bridges_and_preview_subcases_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_entry_bridges_and_preview_subcases_2026_04_08.md)

## 6. Later phase: `state 9` re-enters through `0x08009218`

When `F69 > 1`, visible `state 9` skips the `F6A` subtable and re-enters later
at:

- raw `0x08009218`
- decompiled `0x08005218`

That later phase is where the control word at `+0x4C` becomes important.

The strongest grounded pattern there is:

1. branch on the high nibble of `F6B`
2. if the high nibble is `0xD` or `0xC`, edit `+0x4C`
3. fall into the shared selector sink at `0x08009B24`

The `0xD` branch at `0x0800934E` / `0x0800534E`:

- trims or repacks the upper 16 bits of `+0x4C`
- then exits through the selector sink

The `0xC` branch at `0x08009364` / `0x08005364`:

- toggles bit positions in the upper 16 bits of `+0x4C`
- then exits through the same sink

So the best current read is:

- visible `state 9` has an early preview/posture phase keyed by `F6A`
- and a later control-word phase keyed by the high nibble of `F6B`

That is a much more coherent ownership model than the earlier "state 8 owns the
later posture labels" idea.

## 7. Practical working map

| Visible state | Stage gate | Next selector | Best current meaning |
|---|---|---|---|
| `8` | none | re-enter only | cleanup/reset shim |
| `9` | `F69 == 0 -> 1` | `0x14` | staged preview entry |
| `9` | `F69 == 1 -> 2` | `F6A` sub-dispatch | preview/posture family split |
| `9` | `F69 > 1` | `F6B` high-nibble path | later control-word / commit phase |

The entry side is also clearer now:

| Visible source | Raw bridge | Packed preset | Best current family |
|---|---:|---|---|
| `state 2` | `0x0800A034` | `9 / 1 / 3 / 0` | acquisition-side |
| `state 2` | `0x0800A578` | `9 / 1 / 2 / 0` | timebase / right-panel |
| `state 2` | `0x0800A720` | `9 / 1 / 4 / 0` | mixed trigger-like |

That is the best current owner model above the later posture cluster.

## 8. What this changes

This corrects two earlier overreads:

1. visible `state 8` is not the main owner above the posture cluster
2. the later posture cluster is not one monolithic branch

The sharper model is:

- `state 9` is the real staged owner
- `F69` chooses the phase
- `F6A low nibble` chooses the preview/posture subgroup
- `F6B high nibble` later drives the `+0x4C` control-word phase
- visible `state 9` is usually reached through visible-state-`2` promotion
  helpers, not as an isolated top-level user mode

That is steady progress because it turns one vague "mixed posture family" into
an ordered staged controller.

## Best Next Move

1. Trace what higher-level UI/event owner chooses the visible-state-`2`
   promotion helper for `sub2 = 4` rather than the `sub2 = 2 / 3` helpers.
2. Decode the logical meaning of the three tiny toggle bytes used by
   `F6B = 1 / 3 / 4` in the `F6A = 3 / 4` preview-side family.
3. Re-audit `+0x4C` as a real control word, because visible `state 9` now uses
   it explicitly in its later phase.
4. Compare the visible-state-`9` `0x14` sink against the older packed
   commit/collapse notes, because this still looks like the cleanest bridge
   between preview posture and later control-word edits.
