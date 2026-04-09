# Right-Panel Scope Choreography Path 2026-04-08

Purpose:
- reconstruct one current best stock-faithful scope choreography path centered
  on the visible `state 5` right-panel/editor family
- connect editor posture, staged detail, commit, and consume/collapse into one
  runtime sequence
- separate what is grounded from what is still an unresolved gap

Primary trail:
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
- [right_panel_stage_helper_convergence_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_helper_convergence_2026_04_08.md)
- [right_panel_stage_entry_event_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_stage_entry_event_split_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [scope_runtime_preset_action_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_preset_action_map_2026_04_08.md)
- [runtime_owner_cluster_extension_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_owner_cluster_extension_2026_04_08.md)
- [state2_promotion_owner_family_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/state2_promotion_owner_family_map_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)

## Current Best Runtime Sequence

This is the clearest current end-to-end right-panel/state-`5` path.

1. Enter the right-panel editor posture.
   The stable editor case is visible `state 5` inside the broad controller at
   `0x08004D60`.
   Entry-side setup still looks like:
   - load `E1B`
   - clear `E1A`
   - clear `E1C`
   - leave the editor in the coarse-cursor posture
   The coarse family around `sub2 = 2` is the strongest current entry/exit
   partner here:
   - visible `state 6 -> 5` in the `sub2 = 2` sibling
   - visible `state 5 -> 2` direct reset in the same sibling

2. Run coarse cursor edits while `E1A == 0` and `E1C == 0`.
   The current best event-owner peers are:
   - `0x080081F8` = adjust-prev
   - `0x080087CC` = adjust-next
   - `0x080032E4` = coarse `E1D -= 3` helper in the older broad owner
   In the right-panel/state-`5` branch these edits move `E1D` and emit the
   coarse selector family:
   - `0x27`, then `0x28` in visible `state 5`
   - `0x29` in visible `state 6`

3. Higher event routing chooses a staged-detail owner.
   The single-selection versus mass-toggle split now looks higher than the
   local editor body.
   Best current event-owner peers:
   - `0x0800A120` = single-selection owner
   - `0x0800A2F8` = mass-toggle owner
   These are parallel to adjust-prev / adjust-next, not tiny late helpers.

4. Stage detail inside visible `state 5`.
   If the chosen owner lands in visible `state 5`, it builds staged detail
   rather than committing immediately.
   Single-selection side:
   - requires `E1C == 0` and `E1B != 0`
   - if `E1A == 0`, writes `E1A = 1`
   - clears `0xE12..0xE19`
   - derives one active bit from `E1D / 6` and `E1D % 6`
   - emits `0x28`, then `0x26`
   Mass-toggle side:
   - requires `E1C == 0` and `E1B != 0`
   - toggles `E1A` between `1` and `2`
   - clears or fills `0xE12..0xE19` with the broad mask posture
   - emits `0x26`, then `0x28`

5. Converge both stage helpers into the same detailed-commit bridge.
   The important correction from the newer notes is that both stage helpers feed
   the same downstream family.
   Shared preconditions after either helper:
   - `E1C == 0`
   - `E1A != 0`
   - bitmap across `0xE12..0xE19` is nonzero
   That is exactly what the `sub2 = 5` sibling expects.

6. Commit staged detail through `E1C = 2 -> 0x2A`.
   The clean commit bridge is `0x08006418`.
   Grounded behavior:
   - require `E1C == 0`
   - require `E1A != 0`
   - require nonzero OR across `0xE12..0xE19`
   - write `E1C = 2`
   - queue `0x2A`
   This is the first clean handoff from editor-side staging into the packed
   selector / runtime-family side.

7. Let the older owner and the broad controller consume the commit.
   The commit does not stop at the first `0x2A`.
   Two grounded downstream consumers exist:
   - older broad owner can convert `E1C = 2 -> E1C = 1` and queue another
     `0x2A`
   - broad controller `state 5 / state 6` consumes `E1C`
   Current best split:
   - if `E1C == 2`, the controller can stay on the detailed `0x26 / 0x28`
     side or clear `E1C` and fall back to visible `state 5`
   - if `E1C == 1` in visible `state 6`, the controller queues `0x2B`,
     writes visible `state 5`, re-enters the shared emitter, and stamps
     visible `state 12`

8. Cross into the packed-selector consume/collapse family.
   The right-panel path now looks tied to two adjacent runtime siblings:
   - `sub2 = 2` = coarse editor entry/exit family
   - `sub2 = 5` = staged-detail commit/consume family
   The `sub2 = 5` sibling is the important consume/collapse bridge for the
   detailed path:
   - visible `state 2` stamps preset `9 / 1 / 5 / 0`
   - latch `0x355 = 1`
   - tail-call shared emitter `0x0800B908`
   - visible `state 9` either:
     - collapses back to visible `2` when `F6A == 5`, or
     - stages `0x0501` at `0xF69`, clears `F6B`, emits `0x13`, then `0x14`
   The `sub2 = 2` sibling is the coarse counterpart:
   - visible `state 6 -> 5`
   - visible `state 5 -> 2` with panel-byte cleanup
   - visible `state 2 -> 9 / 1 / 2 / 0`
   - visible `state 9` either collapses or stages `0x0201`, then emits
     `0x13`, `0x14`

9. Re-enter the shared bank emitter instead of ending at one local helper.
   The packed families above are runtime bridges, not terminal leaves.
   They repeatedly:
   - stage visible or internal scope state in `F68..F6B`
   - set latch `0x355`
   - tail-call the shared emitter at `0x0800B908`
   That is the strongest current reason to treat the right-panel family as a
   choreography rather than a flat selector replay.

## Queue / TX Implications

1. The right-panel/state-`5` path is grounded most clearly on selector-queue
   behavior, not directly on final wire words.
   Confirmed selector-side outputs in this family:
   - `0x26`, `0x27`, `0x28`, `0x29`, `0x2A`, `0x2B`
   - `0x13`, `0x14` during packed consume/collapse
   - `0x1A` on the visible-`state 1` side of the `sub2 = 2` sibling

2. The selector/raw-word queue split still matters.
   Current best queue map:
   - `0x20002D6C` = byte selector/display/update queue
   - `0x20002D74` = final 16-bit UART TX-word queue
   - `0x20002D78` = acquisition / SPI trigger queue

3. For the right-panel family itself, the exact transition from selector-side
   choreography into final raw TX words is still only partially grounded.
   The strongest current anchors are:
   - `sub2 = 5` consume side can stage `0x0501` at `F69`, then emit
     `0x13`, `0x14`
   - `sub2 = 2` consume side can stage `0x0201` at `F69`, then emit
     `0x13`, `0x14`
   - shared emitter re-entry at `0x0800B908` is real and runtime-active

4. The only nearby family with directly grounded raw 16-bit words on
   `0x20002D74` is still the mixed `sub2 = 4` branch, not the right-panel
   family.
   That branch can emit:
   - `0x02A0`
   - later `0x0503`
   So `0x02A0 -> ... -> 0x0503` should not currently be treated as a proven
   right-panel/state-`5` path.

## Explicit Unresolved Gaps

1. Higher event dispatch above the four editor peers is still unresolved.
   We have a coherent peer set:
   - adjust-prev
   - adjust-next
   - single-selection
   - mass-toggle
   But the higher event/menu surface that routes into those owners is still not
   recovered cleanly from the downloaded app.

2. The right-panel path is not yet mapped directly onto final `0x20002D74`
   words.
   The sequence above is clear on selector-queue behavior and runtime presets,
   but the exact raw-word materialization for the `state 5 -> sub2 = 5`
   detailed path is still not directly grounded.

3. `E1C = 1` still looks derived, not primary.
   The best current reading is:
   - `E1C = 2` is the primary staged-detail commit handoff
   - `E1C = 1` is a downstream coarse bridge derived from it
   A direct independent `E1C = 1` entry path is not yet recovered.

4. The exact role of `state 12` after the `0x2B` handoff remains open.
   The broad controller clearly stamps visible `state 12` after the
   `E1C == 1` / `0x2B` path, but the downstream semantics of that state are not
   yet integrated into this right-panel memo.

5. The shared-emitter side may still depend on stock image/state that is not
   fully present in the downloaded vendor app.
   That does not invalidate the local choreography above, but it is still a
   plausible reason why our reproduced path can remain behaviorally silent on
   hardware.

## Short Working Model

The current best stock-faithful right-panel scope path is:

1. enter visible `state 5` editor posture
2. run coarse cursor edits on `E1D`
3. choose either single-selection or mass-toggle staging
4. build nonzero staged detail in `E1A` and `0xE12..0xE19`
5. commit through `E1C = 2 -> 0x2A`
6. let older owner and controller consume that handoff
7. cross into `sub2 = 5` consume/collapse or back through `sub2 = 2`
8. re-enter the shared bank emitter

That is the clearest current end-to-end right-panel/state-`5` choreography.
