# State 9 Entry Bridges and Preview Subcases 2026-04-08

## Summary

This pass tightens two open questions from
[state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md):

1. visible `state 9` is not best treated as a top-level user-facing mode by
   itself
2. the `F6A low nibble = 3 / 4` side is not one monolithic "preview blob"

The strongest new result is that the downloaded stock app enters visible
`state 9` through at least three concrete **visible-state-`2` promotion
helpers**, each stamping a different preset into `0xF68..0xF6B` and then
re-entering the shared bank emitter:

- raw `0x0800A034` -> preset `9 / 1 / 3 / 0`
- raw `0x0800A578` -> preset `9 / 1 / 2 / 0`
- raw `0x0800A720` -> preset `9 / 1 / 4 / 0`

That means visible `state 9` is now best read as a **shared internal runtime
work state** above multiple scope-side families, not as a single direct user
screen.

The second useful result is that the `F6A low nibble = 3 / 4` branch at raw
`0x08008F66` does have a real internal split on `F6B`:

- `F6B = 0` -> large preview-bank rebuild / follow-on staging path
- `F6B = 1 / 3 / 4` -> small relative-byte toggle shims
- `F6B = 2` -> special normalize/hardware-latch path

So the `3 / 4` branch is a family of preview-side helpers, not one single case.

Primary references:

- [state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. State `9` is entered through concrete runtime promotion bridges

The cleanest grounded pattern is:

1. visible `state 2`
2. helper stamps `9 / 1 / x / 0`
3. helper sets latch `+0x355 = 1`
4. helper tail-calls the shared emitter

Three concrete promotion sites are now raw-verified:

| Raw site | Decompiled site | Preset written to `0xF68..0xF6B` | Best current family |
|---|---:|---|---|
| `0x0800A034` | `0x08006034` | `9 / 1 / 3 / 0` | acquisition-side |
| `0x0800A578` | `0x08006578` | `9 / 1 / 2 / 0` | timebase / right-panel |
| `0x0800A720` | `0x08006720` | `9 / 1 / 4 / 0` | mixed trigger-like |

The matching state-`9` consume/collapse sides are also concrete:

| Raw site | Decompiled site | Consume/collapse behavior |
|---|---:|---|
| `0x0800A006` | `0x08006006` | collapse on `F6A == 3`, else stage `0x0301` and emit `0x13`, `0x14` |
| `0x0800A5C0` | `0x080065C0` | collapse on `F6A == 2`, else stage `0x0201` and emit `0x13`, `0x14` |
| `0x0800A6F2` | `0x080066F2` | collapse on `F6A == 4`, else stage `0x0401` and emit `0x13`, `0x14` |

This sharpens the current working model:

- visible `state 9` is a shared internal preset/consume bridge
- the important runtime/UI question is which helper family promotes visible
  `state 2` into which internal `9 / 1 / x / 0` layout

## 2. The mixed `sub2 = 4` family is better grounded now

The mixed family around raw `0x0800A6AC` is now stronger than the older
"probably trigger-like" label.

Grounded behavior:

1. visible `state 2` promotes directly to `9 / 1 / 4 / 0` at raw `0x0800A720`
2. visible `state 9` either:
   - collapses back to visible `2` if `F6A == 4`
   - or stages `0x0401` at `0xF69`, clears `F6B`, and emits `0x13`, `0x14`
3. visible `state 1` in the same family works through:
   - `+0xF2D`
   - `+0xF2E`
   - `+0xF3D`
   - `+0xF5D`
4. that visible-state-`1` side can emit:
   - raw TX word `0x02A0`
   - later raw TX word `0x0503`

That still does not give a final user-facing label, but it makes the current
best read firmer:

- `sub2 = 4` is a real mixed runtime family
- it is much closer to the unresolved trigger-like / preview side than to the
  timebase or acquisition families

## 3. Literal `TBH` bytes confirm the `F6A = 3 / 4` preview-side split

At raw `0x08008F70`, the `state 9`, `F6A = 3 / 4` branch executes:

```asm
0x08008F70: tbh [pc, r1, lsl #1]
```

The literal bytes in the app image at file offset `0x4F74` are:

```text
05 00 5d 04 61 04 75 04 59 04
```

Decoded halfwords:

- `0x0005`
- `0x045d`
- `0x0461`
- `0x0475`
- `0x0459`

With the normal Thumb `TBH` base at raw `0x08008F74`, the subtargets are:

| `F6B` | Raw target | Best current read |
|---|---:|---|
| `0` | `0x08008F7E` | large preview-bank rebuild |
| `1` | `0x0800982E` | small relative-byte toggle |
| `2` | `0x08009836` | normalize/hardware-latch path |
| `3` | `0x0800985E` | small relative-byte toggle |
| `4` | `0x08009826` | small relative-byte toggle |

So the `3 / 4` preview side is definitely a real **subfamily** keyed by `F6B`,
not just a single block.

## 4. `F6B = 0` is the heavy preview-bank rebuild case

The `F6B = 0` target begins at raw `0x08008F7E`.

Grounded signs:

- it reads a bitmap/control byte at `+0x14`
- it rebuilds bytes into the `+0x356..` bank in a 301-byte-stride pattern
- source bytes are indexed by the low nibble of `F6A`
- stored values are XORed with `0x80`
- it later calls `FUN_0802DA70()`
- it then branches through additional local logic that can:
  - update `+0x14` / `+0x15` / `+0x16`
  - queue byte `6` to `0x20002D78`
  - touch `0x40011400`
  - sanitize the low nibble of `+0x354`
  - queue byte `2` to `0x20002D78`

The safest current read is:

- `F6B = 0` is the real preview-bank rebuild / staged-follow-on path inside the
  `F6A = 3 / 4` side of visible `state 9`

## 5. `F6B = 1 / 3 / 4` are small toggle shims

The remaining toggle cases are much smaller:

- raw `0x0800982E`
- raw `0x0800985E`
- raw `0x08009826`

Common pattern:

- compute a byte address relative to `base + (F6A low nibble)`
- read one byte
- if it is zero, write `1`
- otherwise write `0`
- return to the shared sink at `0x08009B1E`

The exact logical meaning of those three bytes is still open, but structurally
they are clearly tiny toggle helpers, not full preview-bank rebuilds.

## 6. `F6B = 2` is the special normalize/hardware-latch path

The `F6B = 2` target at raw `0x08009836` behaves differently from both the
heavy rebuild case and the tiny toggle shims.

Grounded behavior:

- normalizes one relative byte through `clz >> 5`
- branches on the normalized result
- writes to the peripheral block at `0x40011400`
- then falls into the same shared local family as the other preview-side cases

So the safest current read is:

- `F6B = 2` is a special normalize-and-apply branch inside the `F6A = 3 / 4`
  preview family

## 7. What this changes

This sharpens the current model in two useful ways.

First:

- visible `state 9` is now better understood as a shared internal runtime
  bridge entered from multiple visible-state-`2` families

Second:

- `F6A = 3 / 4` is no longer just "the preview branch"
- it is a small controller of its own:
  - one heavy rebuild case
  - one normalize/apply case
  - three tiny toggle cases

That is a better fit for the current bench story than the older flatter model.

## Best Next Move

1. Trace what higher-level UI/event owner chooses the visible-state-`2`
   promotion helper for `sub2 = 4` rather than the `sub2 = 2 / 3` helpers.
2. Decode the logical meaning of the three tiny toggle bytes used by
   `F6B = 1 / 3 / 4` in the `F6A = 3 / 4` family.
3. Re-audit `+0x4C` alongside this note, because visible `state 9` now has:
   - a heavy preview-side controller
   - and a later control-word phase
   which look increasingly like two halves of the same staged stock path.
