# Right-Panel Stage Entry Event Split 2026-04-08

## Summary

This pass revisits one question from the newer right-panel model:

- is the single-selection versus mass-toggle split chosen *inside* visible
  `state 5`, or does stock route to them as separate event owners?

The strongest current answer is:

- `0x0800A120` and `0x0800A2F8` now look like **direct sibling event owners**
  in the same sense as `0x080081F8` and `0x080087CC`

That is an important correction. The right-panel editor still uses `E1A`,
`E1C`, and the staged bitmap to decide whether later packed-selector flow looks
coarse (`sub2 = 2`) or detailed (`sub2 = 5`), but the **choice between
single-selection and mass-toggle** no longer looks like a late hidden branch
inside visible `state 5`. It now looks much more like a higher event dispatcher
choosing between two sibling event-owner entries.

Primary references:

- [right_panel_stage_helper_convergence_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_helper_convergence_2026_04_08.md)
- [right_panel_internal_branch_choice_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_internal_branch_choice_2026_04_08.md)
- [right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)

## 1. `0x0800A120` has the shape of a full event owner, not a tiny local helper

The raw body at `0x0800A120` starts with a full prologue and an immediate
top-level dispatch on visible state `F68`:

- `push {r4, r5, r6, lr}`
- load base state block
- switch on `F68`

Its visible-state cases are mixed-purpose, not narrowly local:

| Visible state (`F68`) | Raw target | Best current read |
|---|---:|---|
| `1` | `0x0800A146..0x0800A2D6` | mixed selector/raw-word owner using `F2D/F36`, final word queue, then selector `0x1B` |
| `2` | `0x0800A1BC..0x0800A1F6` | posture toggle at `0x354`, then queue selector `0x02` |
| `5` | `0x0800A19E..0x0800A236` | single-selection stage from `E1D` |
| `else` | return | no-op for other visible states |

That is the same broad structural pattern we already trusted for the generic
`adjust-prev` owner at `0x080081F8`:

- one entry point
- top-level `F68` dispatch
- different behavior depending on visible state

So `0x0800A120` is better treated as a direct event-owner peer, not as an
interior state-`5` branch.

## 2. `0x0800A2F8` has the same broad owner shape

The raw body at `0x0800A2F8` starts the same way:

- `push {r4, r5, r7, lr}`
- load base state block
- switch on visible state `F68`

Its visible-state map is again broader than a local helper:

| Visible state (`F68`) | Raw target | Best current read |
|---|---:|---|
| `1` | `0x0800A31E..0x0800A324` | mixed posture gate around `F5D` |
| `2` | `0x0800A38C..0x0800A412` | posture update at `0x354`, then queue selector `0x02` |
| `5` | `0x0800A326..0x0800A382` | mass-toggle / broad bitmap stage |
| `else` | return | no-op for other visible states |

Again, that is exactly what we would expect from a sibling event owner:

- same top-level state discriminator
- state-`5` branch supplies the right-panel action we care about
- other visible-state branches handle mixed posture responsibilities

So `0x0800A2F8` also looks like a direct event-owner peer.

## 3. This lines up with the older adjust-prev / adjust-next model

The strongest reason to promote these two bodies is their parallelism with the
already-recovered adjustment owners:

- `0x080081F8` = generic adjust-prev
- `0x080087CC` = generic adjust-next
- `0x0800A120` = staged single-selection / apply-side owner
- `0x0800A2F8` = staged mass-toggle / reverse-side owner

All four have the same broad shape:

1. real function-style prologue
2. dispatch on visible state `F68`
3. state-specific behavior for several top-level runtime families
4. queue final selector or raw-word work from inside those visible-state cases

That is stronger evidence for a **four-peer event-owner family** than for a
single late branch hidden inside the visible-state-`5` editor.

## 4. The earlier "internal branch choice" model is only half the story

The older note
[right_panel_internal_branch_choice_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_internal_branch_choice_2026_04_08.md)
was still useful, but it now needs a correction in emphasis.

What still holds:

- once stock is already inside visible `state 5`, staged bytes still matter
- `E1A == 0`, `E1C == 0`, `E1B != 0` still matches the coarse editor posture
- `E1A != 0` plus a nonzero bitmap still matches the staged-detail commit side

What changed:

- the **choice of stage helper** now looks external to that local branch
- stock probably reaches visible-state-`5` single-selection and mass-toggle
  behavior by dispatching into different sibling owners
- visible-state-`5` local state then determines whether later flow stays coarse
  or advances into `sub2 = 5`

So the better hierarchy now is:

1. higher event dispatcher chooses one sibling owner
   - adjust-prev
   - adjust-next
   - single-selection
   - mass-toggle
2. chosen owner runs its visible-state-`5` branch
3. local right-panel staged state decides which downstream packed-selector
   family gets used

## 5. The dispatcher above these owners is still indirect / unresolved

I rechecked the raw app image for easy direct routing evidence and still did not
find any:

- no literal Thumb pointers to:
  - `0x0800A120`
  - `0x0800A2F8`
  - `0x080081F8`
  - `0x080087CC`
- no simple direct-call story in the downloaded app image

That is consistent with the earlier contradiction in
[right_panel_event_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_owner_map_2026_04_08.md):

- the `btn_map` region still looks invalid in the vendor download
- the `key_task` dispatch table still looks invalid in the vendor download

So the current safest read is:

- the four event owners are real
- the higher logical-event-to-owner routing is still hidden behind an indirect
  dispatch surface we do not fully trust in the downloaded image

## 6. Updated working model

The right-panel editor path now looks like this:

1. enter visible `state 5`
2. higher event routing chooses one sibling owner:
   - `adjust-prev`
   - `adjust-next`
   - `single-selection`
   - `mass-toggle`
3. chosen owner executes its visible-state-`5` branch
4. local staged state decides the downstream shape:
   - coarse posture stays aligned with `sub2 = 2`
   - staged-detail posture can feed `sub2 = 5`

That is a cleaner and more stock-shaped model than:

- "state `5` internally decides which stage helper to call"

## What This Changes

The next unresolved split is no longer:

- "what hidden editor branch picks single-selection versus mass-toggle?"

It is now:

- "what higher logical event routes to the `0x0800A120` owner versus the
  `0x0800A2F8` owner?"

That is a smaller and more realistic target.

## Best Next Move

1. Trace the higher event/menu dispatcher that routes into:
   - `0x080081F8`
   - `0x080087CC`
   - `0x0800A120`
   - `0x0800A2F8`
2. Keep treating the local `E1A / E1C / bitmap` state as the downstream
   discriminator for `sub2 = 2` versus `sub2 = 5`.
3. Stop assuming the single-selection versus mass-toggle choice is made by a
   hidden late branch inside visible `state 5`, unless new raw evidence forces
   that reading back.
