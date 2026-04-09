# Right-Panel Final-Word Bridge Focus 2026-04-08

Purpose:
- assume the reconstructed right-panel choreography is basically correct
- focus only on how the detailed `state 5` path could materialize final
  `0x20002D74` words for `dvom_TX`
- rank the best bridge candidates and list the first concrete code/data sites
  to inspect next

Primary references:
- [queue_dispatch_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/queue_dispatch_split_2026_04_08.md)
- [dynamic_scope_word_builder_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/dynamic_scope_word_builder_2026_04_08.md)
- [right_panel_scope_choreography_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_scope_choreography_path_2026_04_08.md)
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)
- [scope_composite_state_presets_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_composite_state_presets_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md)

Address note:
- existing notes mix decompiled/project addresses and raw app-slot addresses
- for archived raw app code locations: raw address = decompiled address
  `+ 0x4000`
- this memo names decompiled/project addresses first and gives raw addresses
  only where that distinction matters

## Executive Summary

The best current model is a staged bridge, not a direct one.

The detailed right-panel path most likely reaches final `0x20002D74` words in
two steps:

1. the detailed commit/consume side stages a packed `0x0501`-class state and
   emits `0x13`, `0x14`
2. a nearby raw-word helper materializes that staged state into an actual
   halfword on `0x20002D74`

The best candidate bridge chain is:

1. `0x08006418` detailed commit bridge
2. `sub2 = 5` consume/collapse side around decompiled `0x080064E0..0x0800650C`
3. queue `0x13`, then `0x14` from the non-collapse `sub2 = 5` branch
4. follow the downstream selector/event path those bytes trigger
5. then reach a raw-word helper such as `0x08006060` or `0x08006120`

The strongest single materializer candidate is still `0x08006060`, because it
concretely does `0x0501 -> 0x20002D74`.

## 1. What is already grounded

From the queue split:

- `0x20002D74` is the real 16-bit UART TX queue consumed by `dvom_TX`
- `0x20002D54` is a staging halfword used by raw-word builders before enqueue
- `0x20002D6C` is selector/display-side, not the final wire queue

From the right-panel choreography:

- detailed right-panel work runs in visible `state 5`
- `0x08006120` or `0x080062F8` stage detail into `E1A` and `0xE12..0xE19`
- `0x08006418` raises `E1C = 2` and queues `0x2A`
- the detailed path then crosses into the `sub2 = 5` consume/collapse family

From the packed preset notes:

- decompiled `0x0800650C` writes `0x0501` to `0xF69`
- that means:
  - `DAT_20001061 = 1`
  - `DAT_20001062 = 5`
- the same path immediately emits `0x13`, then `0x14`

That is the tightest current bridge anchor between the detailed right-panel path
and anything that already resembles a final wire-word seed.

## 2. Ranked bridge candidates

### Rank 1: `sub2 = 5` consume/collapse plus downstream `0x13/0x14` path

Best candidate region:
- decompiled `0x080064E0..0x0800650C`
- raw `0x0800A4E0..0x0800A50C`

Why it ranks first:

1. It is the only detailed-path-specific family already grounded to stage
   `0x0501` in packed state.
2. It sits immediately downstream of the `E1C = 2 -> 0x2A` detailed commit.
3. It emits `0x13`, `0x14`, which is exactly the kind of internal transition
   traffic already associated with the packed consume/collapse families.
4. The raw split is now clearer:
   - the `F6A == 5` collapse branch clears state and tail-calls
     `FUN_0800B908`
   - the alternate branch stages `0x0501` at `0xF69`, clears `F6B`, and
     queues `0x13`, then `0x14`

Why it is still not sufficient by itself:

- this path is grounded on packed state and selector traffic
- it is not yet directly grounded on `0x20002D74`

So this is the best bridge family, but not yet the proven final materializer.

### Rank 2: `0x08006060` fixed `0x0501` raw-word builder

Best candidate function:
- decompiled `0x08006060`
- raw `0x0800A060`

Grounded behavior:

1. seed selector bytes at `+0xF2D` and `+0xF2E`
2. write `0x0501` to `0x20002D54`
3. enqueue that halfword to `0x20002D74`
4. queue `0x1D`, then `0x1B`

Why it ranks second:

- it is the cleanest confirmed `0x0501 -> 0x20002D74` materializer
- it also queues the same `0x13`, then `0x14` selector pair seen in the
  non-collapse `sub2 = 5` branch
- semantically, this is now the nearest known sibling for the staged detailed
  path, even though it stages `0x0301` at `0xF69` rather than `0x0501`

Why it is not rank 1:

- there is not yet a recovered direct control edge from the `sub2 = 5`
  `0x13/0x14` path into `0x08006060`

### Rank 3: `0x08006120` dynamic `0x050x` raw-word builder

Best candidate function:
- decompiled `0x08006120`
- raw `0x0800A120`

Grounded behavior:

1. build a staged halfword at `0x20002D54`
2. OR in `0x0500`
3. enqueue to `0x20002D74`
4. choose low byte from:
   - `0x0C / 0x0D`
   - `0x0E / 0x17`
   - `0x10 / 0x15`
   - `0x11 / 0x16`
5. drive off selector bytes at:
   - `+0xF2D`
   - `+0xF36`
   - `+0xF39`

Why it ranks third:

- it is a real final-word builder
- it already shares address-family territory with the right-panel event cluster
- if the detailed path needs not just `0x0501`, but a family-specific
  `0x050x` word, this is the best current materializer candidate

Why it is below `0x08006060`:

- the detailed path is currently grounded to `0x0501` specifically, not yet to
  one of the dynamic `0x050C/0x050E/0x0510/0x0511` words

### Rank 4: `0x08006418` commit bridge itself

Best candidate function:
- decompiled `0x08006418`
- raw `0x0800A418`

Why it still matters:

- it is the entry point from staged right-panel detail into the packed consume
  family
- it is the narrowest confirmed control bridge from local editor state to the
  broader packed selector/runtime family

Why it is not a likely final-word writer:

- current evidence ties it to `0x20002D6C`, packed state, and `0x2A`
- not directly to `0x20002D74`

### Rank 5: broad owner `0x08003148`

Why it stays on the list:

- it owns `0x13/0x14` and related family traffic
- it may still be the higher chooser that feeds the detailed path into the
  packed consume side

Why it is lower priority:

- it is broader orchestration, not the clearest final-word materializer

## 3. Best current bridge model

The strongest current bridge model is:

1. visible `state 5`
2. stage detail with `0x08006120` or `0x080062F8`
3. commit through `0x08006418`
4. enter `sub2 = 5` consume/collapse
5. at decompiled `0x0800650C`, either:
   - collapse directly to visible `state 2` and tail-call `FUN_0800B908`
     when `F6A == 5`, or
   - stage `0x0501` into `0xF69`, clear `F6B`, and queue `0x13`, then `0x14`
6. follow the downstream selector/event path triggered by `0x13/0x14`
7. then materialize the actual UART halfword through:
   - `0x08006060` if the next sink is the fixed `0x0501` sibling path
   - `0x08006120` if the next sink is a family-specific dynamic `0x050x`

That is the cleanest bridge because every stage in it is already grounded in one
of the existing notes.

## 4. First concrete code sites to inspect next

Inspect these in this order:

1. decompiled `0x0800650C` / raw `0x0800A50C`
   - exact `strh 0x0501` site for the detailed `sub2 = 5` consume path
   - question: after `0x0501` staging, which consumer is reached through the
     queued `0x13/0x14` pair?

2. decompiled `0x080064E0..0x0800650C` / raw `0x0800A4E0..0x0800A50C`
   - whole `sub2 = 5` consume/collapse block
   - question: which branch collapses straight to shared emitter, and which
     branch instead hands off through queued selector bytes?

3. decompiled `0x08006060` / raw `0x0800A060`
   - fixed `0x0501 -> 0x20002D74` materializer
   - question: is this the first downstream sibling reached after the detailed
     path's `0x13/0x14` handoff?

4. decompiled `0x08006120` / raw `0x0800A120`
   - dynamic `0x050x -> 0x20002D74` materializer
   - question: can the detailed path's packed `0x0501` preset feed into its
     selector bytes at `+0xF2D / +0xF36 / +0xF39`?

5. `FUN_0800B908` call sites immediately after:
   - decompiled `0x08006500`
   - decompiled `0x0800658E`
   - decompiled `0x080065EA`
   - from [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
   - question: which of these runtime re-entry sites is the one paired with the
     detailed `sub2 = 5` path?

## 5. First concrete data sites to inspect next

Inspect these data/state locations next:

1. `0x20002D54`
   - staging halfword for final raw words
   - first place to prove whether the detailed path becomes `0x0501` or a
     family-specific `0x050x`

2. `0x20002D74`
   - final `dvom_TX` queue
   - the ultimate sink to confirm

3. `0xF69` (`DAT_20001061..62`)
   - packed halfword where the detailed path writes `0x0501`
   - the clearest bridge-state anchor

4. `+0xF2D`, `+0xF36`, `+0xF39`
   - selector inputs for the dynamic raw-word builder
   - best data-side question: does the detailed path ever rewrite these before
     `0x08006120` fires?

5. `+0x355`
   - runtime re-entry latch
   - useful for proving which re-entry path is active when detailed consume
     occurs

## 6. What to deprioritize

Deprioritize these for this specific bridge question:

- coarse `sub2 = 2` family around `0x08006548`
- mixed `sub2 = 4` family, even though it has grounded `0x02A0 -> 0x0503`
- broad visible scope FSM searches for direct `0x20002D74` writes

Those are still relevant to the larger scope problem, but they are not the best
next bridge targets for the detailed right-panel path.

## 7. Bottom line

If the right-panel choreography is basically correct, then the first serious
bridge candidate is:

- detailed `sub2 = 5` consume/collapse staging `0x0501` at `0xF69`

and the first serious materializer candidate is:

- `0x08006060` for a fixed `0x0501` word

with `0x08006120` as the best alternate materializer if the detailed path
actually resolves into a family-specific `0x050x` word before `dvom_TX`.
