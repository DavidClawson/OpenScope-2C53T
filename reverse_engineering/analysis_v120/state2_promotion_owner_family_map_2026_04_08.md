# State 2 Promotion Owner Family Map 2026-04-08

## Summary

This pass compares the three concrete visible-state-`2 -> state-9` promotion
families side by side instead of treating them as isolated helpers.

The strongest new result is that the downloaded stock app now looks like it has
three real **sibling promotion owners**, each with the same top-level shape:

1. branch on visible `state`
2. if visible `state == 2`, stamp `9 / 1 / x / 0` into `0xF68..0xF6B`
3. set latch `+0x355 = 1`
4. tail-call the shared emitter at `0x0800F908`
5. if visible `state == 9`, either collapse back to visible `2` or stage
   a family-specific `0x?01` word at `0xF69` and emit `0x13`, `0x14`
6. if visible `state == 1`, run a family-specific setup side

The three owners are:

| Raw owner | Decompiled owner | Internal preset | Best current family |
|---|---:|---|---|
| `0x08009FCC` | `0x08005FCC` | `9 / 1 / 3 / 0` | acquisition-side |
| `0x0800A548` | `0x08006548` | `9 / 1 / 2 / 0` | timebase / right-panel |
| `0x0800A6AC` | `0x080066AC` | `9 / 1 / 4 / 0` | mixed trigger-like |

That makes the next search surface cleaner:

- the real chooser above these helpers is probably **not** the tiny
  visible-state-`2` promotion store itself
- it is more likely a higher event/menu owner that routes into one of these
  three sibling families

Primary references:

- [state9_entry_bridges_and_preview_subcases_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_entry_bridges_and_preview_subcases_2026_04_08.md)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [runtime_owner_cluster_extension_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_owner_cluster_extension_2026_04_08.md)
- [scope_runtime_family_gate_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_family_gate_map_2026_04_08.md)
- [state9_preview_posture_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state9_preview_posture_case_map_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. All three families share the same top-level runtime pattern

The three owners are not byte-identical, but they are structurally close.

Each one:

- begins with a push/prologue and a base pointer to `0x200000F8`
- reads visible `state` from `0xF68`
- handles at least visible `state 9`, visible `state 2`, and visible `state 1`
- uses `+0x355` as the re-entry latch
- ultimately re-enters the shared bank emitter at raw `0x0800F908`

That means the earlier flat reading was too weak. These are not random helper
fragments; they are sibling runtime owners with the same role at different
scope-side family boundaries.

## 2. Acquisition-side sibling: raw `0x08009FCC`

This owner is the cleanest `sub2 = 3` family.

Grounded behavior:

- visible `state 2` -> stamp `9 / 1 / 3 / 0`
- visible `state 9`
  - collapse to visible `2` if `F6A == 3`
  - else stage `0x0301` at `0xF69`, clear `F6B`, emit `0x13`, then `0x14`
- visible `state 1`
  - keys on `+0xF2D == 3`
  - reads `+0xF5D`
  - can materialize `0x0501`
  - later emits selector bytes `0x1D`, then `0x1B`

The visible-state-`1` side is important because it is already more specific
than "generic preset setup." It does not look like the right-panel family and
does not look like the mixed `0x02A0 -> 0x0503` path either.

Best current read:

- this owner is the acquisition-side sibling above the `9 / 1 / 3 / 0`
  preset bridge

## 3. Timebase / right-panel sibling: raw `0x0800A548`

This owner remains the cleanest `sub2 = 2` family.

Grounded behavior:

- visible `state 2` -> stamp `9 / 1 / 2 / 0`
- visible `state 9`
  - collapse to visible `2` if `F6A == 2`
  - else stage `0x0201` at `0xF69`, clear `F6B`, emit `0x13`, then `0x14`
- visible `state 5`
  - can collapse directly to visible `2`
  - clears `+0xE1C`
  - clears `+0xE12..+0xE19`
  - clears `+0xE1A`
- visible `state 1`
  - keys on `+0xF2D`
  - uses `+0xF3D`, `+0xF35`, `+0xF30`, `+0xF40`
  - emits `0x1A`
  - then falls into a follow-on helper

This is still the strongest current right-panel/timebase bridge because it is
the only sibling with direct cleanup of the staged right-panel bytes.

Best current read:

- this owner is the timebase/right-panel sibling above the `9 / 1 / 2 / 0`
  preset bridge

## 4. Mixed trigger-like sibling: raw `0x0800A6AC`

This owner remains the mixed branch, but it is more concrete than before.

Grounded behavior:

- visible `state 2` -> stamp `9 / 1 / 4 / 0`
- visible `state 9`
  - collapse to visible `2` if `F6A == 4`
  - else stage `0x0401` at `0xF69`, clear `F6B`, emit `0x13`, then `0x14`
- visible `state 1`
  - keys on `+0xF2D == 3`
  - uses `+0xF5D`, `+0xF3C`, `+0xF36`
  - can emit raw TX word `0x02A0`
  - later emits raw TX word `0x0503`

Unlike the timebase sibling, it does not clear the `+0xE1C / +0xE1D` panel
bytes. Unlike the acquisition sibling, it has a raw-wire-word side that is
already grounded on `0x02A0` and `0x0503`.

Best current read:

- this owner is the strongest current mixed trigger-like sibling above the
  `9 / 1 / 4 / 0` preset bridge

## 5. The family-specific visible-state-`1` sides are the real differentiator

The visible-state-`2` promotion stores are simple and easy to over-focus on.
The stronger discriminator is the visible-state-`1` setup side:

| Sibling owner | Visible-state-`1` signs | Best current meaning |
|---|---|---|
| `0x08009FCC` | `+0xF2D == 3`, `+0xF5D`, `0x0501`, then `0x1D / 0x1B` | acquisition-side |
| `0x0800A548` | `+0xF2D`, `+0xF3D`, `+0xF35`, `+0xF30`, `+0xF40`, then `0x1A` | timebase / right-panel |
| `0x0800A6AC` | `+0xF2D == 3`, `+0xF5D`, `+0xF3C`, `+0xF36`, then `0x02A0 -> 0x0503` | mixed trigger-like |

That makes the next question more precise:

- what higher-level event/menu owner routes into one of these three visible-
  state-`1` setup sides?

That is probably the real chooser we still want, not the tiny visible-state-`2`
promotion stores.

## 6. No plain direct-call or literal-pointer references were found

Two negative checks are still useful:

1. raw objdump did not show plain `bl` / `b.w` calls to:
   - `0x08009FCC`
   - `0x0800A548`
   - `0x0800A6AC`
   - or their visible-state-`2` promotion labels
2. a wider raw branch search only surfaced the expected local `beq` edges inside
   the sibling owners themselves:
   - `0x08009FE0 -> 0x0800A034`
   - `0x0800A6C0 -> 0x0800A720`
   and did not reveal a higher direct branch source
3. the raw app image does not contain literal Thumb pointers to those addresses

So the safest current read is:

- these sibling owners are probably reached through a wider indirect dispatcher
  or as interior case-label slices inside larger owners
- the next branch should look for that higher routing layer, not just ordinary
  direct callers

## 7. What this changes

This is a meaningful tightening:

- the `9 / 1 / 2 / 0`, `9 / 1 / 3 / 0`, and `9 / 1 / 4 / 0` bridges are no
  longer just "three preset helpers"
- they now look like outputs of three sibling runtime-owner families
- each family has a distinct visible-state-`1` setup side
- and the wider cluster now appears to include:
  - a `sub2 = 5` staged-detail sibling
  - a cluster-level normalizer at raw `0x0800A834`

That means the next RE step should climb one level up:

- find the event/menu owner that chooses the acquisition-side sibling
- find the event/menu owner that chooses the timebase/right-panel sibling
- find the event/menu owner that chooses the mixed trigger-like sibling

## Best Next Move

1. Trace the higher-level owner that routes into the visible-state-`1` setup
   side of:
   - raw `0x08009FCC`
   - raw `0x0800A548`
   - raw `0x0800A6AC`
2. Treat the visible-state-`2 -> 9 / 1 / x / 0` stores as confirmation points,
   not as the primary selection logic.
3. Compare that higher routing layer against the already-grounded broad-owner
   selector families:
   - acquisition-side
   - timebase/right-panel
   - mixed trigger-like
