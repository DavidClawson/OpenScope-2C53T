# Right-Panel Selector-to-Wire Bridge Candidates 2026-04-08

Purpose:
- recover the best current bridge from the reconstructed right-panel detailed
  path into final `0x20002D74` UART TX words
- separate the detailed-path candidates from the broader selector-bank owners
- rank the most plausible helper clusters to inspect next

Primary references:
- [right_panel_scope_choreography_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_scope_choreography_path_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
- [runtime_owner_cluster_extension_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_owner_cluster_extension_2026_04_08.md)
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [scope_selector_bypass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_selector_bypass_2026_04_08.md)
- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

Address note:
- older notes mix decompiled/project addresses and raw app-slot addresses
- for code locations in the archived app image, raw address = decompiled address
  `+ 0x4000`
- this memo uses decompiled/project addresses unless a raw address is stated

## Executive Summary

The best current bridge for the detailed right-panel path is not a single
function that directly writes the final UART word. The stronger model is a
two-hop chain:

1. the staged-detail family around `0x08006418` converts right-panel editor
   state into the packed `sub2 = 5` path
2. the `sub2 = 5` consume/collapse side stages `0x0501` at `0xF69`, emits
   `0x13`, `0x14`, and re-enters the shared bank emitter
3. the adjacent raw-word helper cluster around `0x08006060 / 0x08006120`
   contains the first grounded writers to `0x20002D74`

So the best candidate bridge is:

- detailed right-panel staging
- `0x08006418` commit
- `0x080064E0..0x0800650C` consume/collapse
- shared-emitter re-entry
- then the raw-word cluster at `0x08006060 / 0x08006120`

That is a better fit than either of these weaker models:

- "the detailed right-panel path directly writes `0x20002D74` itself"
- "the broad visible scope FSM owns the missing wire words directly"

## 1. Grounded local right-panel path

The reconstructed detailed path is already strong on local editor state:

1. visible `state 5` editor posture
2. `0x08006120` single-selection stage or `0x080062F8` mass-toggle stage
3. bitmap built in `0xE12..0xE19`
4. `0x08006418` raises `E1C = 2` and queues `0x2A`
5. downstream consume/collapse enters the `sub2 = 5` family

That part is grounded in:
- [right_panel_scope_choreography_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_scope_choreography_path_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)

What is still missing is the final step from that staged local path into real
wire words on `0x20002D74`.

## 2. Why `sub2 = 5` is the primary bridge candidate

The strongest bridge candidate is the `sub2 = 5` sibling:

- decompiled `0x08006418`
- raw `0x0800A418`

Reasons:

1. It is the only sibling directly tied to the detailed bitmap-commit path.
   The visible-`state 5` branch requires:
   - `E1C == 0`
   - `E1A != 0`
   - nonzero OR across `0xE12..0xE19`
   - then raises `E1C = 2` and queues `0x2A`

2. Its visible-`state 9` consume/collapse side is the only right-panel family
   currently grounded to stage a `0x0501` halfword in packed state:
   - if `F6A == 5`, collapse back to visible `2`
   - otherwise write `0x0501` at `0xF69`, clear `F6B`, emit `0x13`, then
     `0x14`

3. The `0x0501` value is not arbitrary. It matches a confirmed raw-word anchor
   elsewhere in the helper cluster.

So `sub2 = 5` is the first place where the detailed editor path produces a
packed value that already resembles a final wire-word seed.

## 3. Why `0x08006060` is the best adjacent raw-word materializer

The next candidate is the small front-end helper at:

- decompiled `0x08006060`
- raw `0x0800A060`

This helper is a good bridge target because it does all of these in one place:

1. seeds selector bytes at `+0xF2D` and `+0xF2E`
2. writes raw word `0x0501`
3. enqueues that halfword to `0x20002D74`
4. queues display selectors `0x1D`, then `0x1B`

That is the closest current match to the `sub2 = 5` consume-side behavior:

- `sub2 = 5` stages `0x0501` in packed state
- `0x08006060` is a real helper that materializes `0x0501` onto the raw UART
  queue

This does not prove a direct call edge from `sub2 = 5` into `0x08006060`, but
it is the cleanest semantic match in the current recovered code.

## 4. Why `0x08006120` is the next-best bridge candidate

The dynamic raw-word builder at:

- decompiled `0x08006120`
- raw `0x0800A120`

is the other strong bridge candidate.

Reasons:

1. It is one of the few helpers that definitively writes scope-like words to
   `0x20002D74`.

2. It owns the dynamic `0x0500 | low_byte` family:
   - `0x050C / 0x050D`
   - `0x050E / 0x0517`
   - `0x0510 / 0x0515`
   - `0x0511 / 0x0516`

3. The same decompiled address is already part of the recovered right-panel
   event cluster on the selector-side:
   - as a visible-`state 5` single-selection stage
   - which means the right-panel path and the dynamic raw-word builder are not
     disjoint by address family

4. The helper cluster note shows `0x08006060`, `0x08006120`, `0x080062F8`,
   and `0x08006418` as one adjacent state-machine cluster rather than four
   isolated helpers.

This makes `0x08006120` the best candidate for the detailed path's
family-specific wire-word materialization after `sub2 = 5` consume/collapse has
already selected the broad packed target.

## 5. Why the bridge is probably two-hop, not direct

The negative evidence matters here.

From [scope_selector_bypass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_selector_bypass_2026_04_08.md):

- the visible scope-side code mostly touches `0x20002D6C` and `0x20002D78`
- it does not obviously write `0x20002D74`

From the right-panel notes:

- the detailed path is strongest on selector-side behavior
- the consume/collapse families stage packed presets and emit `0x13`, `0x14`
- runtime helpers repeatedly re-enter the shared bank emitter after staging
  `0xF68..0xF6B`

So the most defensible current model is:

1. right-panel editor builds staged detail
2. `sub2 = 5` turns that into packed `0x0501`-class state plus `0x13/0x14`
3. shared-emitter re-entry consumes the packed preset
4. adjacent raw-word helper(s) materialize the final `0x20002D74` word

That is stronger than claiming the right-panel helper itself directly pushes the
final word.

## 6. Candidate ranking

### Tier 1: most likely bridge path

1. `0x08006418` plus its `sub2 = 5` consume/collapse side
   - detailed-path specific
   - stages `0x0501`
   - already tied to `0x13/0x14`

2. shared-emitter re-entry after the `sub2 = 5` preset
   - repeated runtime pattern
   - likely the handoff point from packed state into command/wire behavior

3. `0x08006060`
   - confirmed `0x0501 -> 0x20002D74`
   - strongest semantic match to the `sub2 = 5` consume result

### Tier 2: likely family-specific materializer

4. `0x08006120`
   - confirmed dynamic `0x0500 | low_byte -> 0x20002D74`
   - already a right-panel event peer on the selector side
   - plausible source of the detailed-path final `0x050x` word once the broad
     `0x0501` preset has been consumed

5. `0x080062F8`
   - less likely as the final raw-word materializer
   - more likely the reverse/cleanup partner
   - still relevant because it shares the same cluster and state gates

### Tier 3: orchestration, not materialization

6. `0x08003148`
   - broad owner for `0x13/0x14`, `0x27/0x28`, `0x2A`, and others
   - important for choreography
   - weaker as the direct `0x20002D74` bridge

7. `0x08006548`
   - strong coarse editor sibling
   - much better fit for `sub2 = 2`
   - not the best bridge candidate for the detailed path

## 7. Best current bridge chain

The best current end-to-end bridge hypothesis is:

1. visible `state 5` editor posture
2. `0x08006120` or `0x080062F8` stages detail into `E1A` and `0xE12..0xE19`
3. `0x08006418` commit path raises `E1C = 2` and queues `0x2A`
4. `sub2 = 5` consume/collapse side stages `0x0501` at `0xF69`, emits
   `0x13`, `0x14`
5. runtime re-entry into the shared emitter
6. `0x08006060` or `0x08006120` materializes the corresponding final raw word
   onto `0x20002D74`

This is the strongest current bridge because every segment in it is already
grounded somewhere in the recovered notes. The remaining gap is the exact edge
between step 4 and step 6.

## 8. Weak alternatives

Two weaker alternatives should stay deprioritized:

1. `sub2 = 4` mixed trigger-like family
   - it does have grounded raw words `0x02A0 -> ... -> 0x0503`
   - but it is not the reconstructed detailed right-panel path

2. direct visible scope FSM ownership of `0x20002D74`
   - the current recovered visible scope handlers do not support that model

## 9. Best next trace

The shortest next static RE step is:

1. trace the exact control edge from `sub2 = 5` consume/collapse into the
   shared-emitter path
2. then inspect whether the next raw-word sink is:
   - `0x08006060` fixed `0x0501`, or
   - `0x08006120` dynamic `0x050x`

That is the smallest remaining gap between the recovered right-panel
choreography and a real final-wire materialization path.
