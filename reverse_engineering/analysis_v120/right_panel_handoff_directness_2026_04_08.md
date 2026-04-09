# Right-Panel Handoff Directness

Date: 2026-04-08

Purpose:
- determine whether the coarse right-panel handoff `E1C = 1` can be entered
  directly in the downloaded vendor app
- separate the immediate state-`5` entry and state-`2` return shims from the
  later panel-subview handoff bytes
- identify the next highest-value search surface above the recovered
  right-panel event cluster

Primary references:
- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [panel_subview_writer_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_subview_writer_bridge_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [runtime_scope_bank_reentry_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/runtime_scope_bank_reentry_2026_04_08.md)
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)

## Executive Summary

This pass makes one important point much firmer:

1. In the downloaded vendor app, the only clean positive runtime writer for
   `E1C = 1` is still the old broad-owner bridge at `0x08003324`.
2. That bridge is only reachable when `E1C` was already `2`.
3. So the current best read is that coarse handoff `E1C = 1` is **derived**
   from the staged-detail `E1C = 2` path, not a primary entry state.
4. The immediate state-entry surface above the cluster looks different:
   - `0x080065B2` writes visible state `5` and re-enters the shared bank
     emitter
   - `0x08006592` returns directly to visible state `2` and clears the
     right-panel staging bytes

So the next search should not be "find another `E1C = 1` writer." It should be
"find who routes into the state-`5` entry shim and who routes back out through
the state-`2` reset shim."

## 1. The positive `E1C` writers are still asymmetric

The raw disassembly still gives the same asymmetric picture:

- `0x080064A4` writes `E1C = 2`
- `0x08003324` writes `E1C = 1`

What changed in this pass is confidence, not the addresses. Rechecking the raw
owners did not surface a second direct `E1C = 1` raiser elsewhere in the
downloaded image.

Relevant write sites:

- [mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08005FE0_080066C0_2026_04_08.txt)
  `0x080064A4..0x080064A6`
- [mode_scope_state_cluster_08003180_08003920_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08003180_08003920_2026_04_08.txt)
  `0x08003324..0x08003326`

That keeps the two-phase handoff model alive:

- `E1C = 2` = staged detailed subview handoff
- `E1C = 1` = coarser handoff consumed later by the controller

## 2. `E1C = 1` still appears downstream of `E1C = 2`

The older broad owner around `0x080032D6` is the critical proof point.

The branch shape is:

- load `E1C`
- if `E1C == 2`, fall into `0x08003324`
- if `E1C != 0`, bail out
- if `E1C == 0`, stay in the coarse `E1D` / `0x27 / 0x28` path

Then at `0x08003324`:

- write `E1C = 1`
- queue `0x2A`

That means the old broad owner does not look like an independent direct
generator of `E1C = 1`. It looks like a **downstream bridge** that consumes a
prior `E1C = 2` commit and converts it into the coarser handoff state.

Best current read:

- the downloaded app does not yet show a clean direct path into `E1C = 1`
  without first passing through `E1C = 2`

## 3. Controller-side `E1C` writes are cleanup, not new positive entries

The controller-side writes around `0x08005298` and `0x080052CE` help narrow the
search too.

From
[mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt):

- `0x08005298..0x080052A0`
  - clear `E1C`
  - write visible state `5`
- `0x080052CE..0x080052F8`
  - clear `E1C`
  - emit `0x26`, then `0x28`

These are not new positive `E1C` raisers. They are controller-side cleanup and
consumption paths after the handoff byte has already served its purpose.

So they do not weaken the main conclusion above.

## 4. The immediate state-entry surface sits above `E1C`

The more useful "one level higher" anchors are the immediate visible-state
shims in the narrower runtime family around `0x08006548`.

### 4.1 Direct state-`5` entry shim

At `0x080065B2..0x080065BC`:

- write visible state `5`
- tail-call `FUN_0800B908`

This is a cleaner first entry point into the right-panel family than any of the
`E1C` writes.

### 4.2 Direct state-`2` return shim

At `0x08006592..0x080065AE`:

- write visible state `2`
- clear `E1C`
- clear `0xE12`
- clear `0xE16`
- clear `E1A`
- tail-call `FUN_0800B908`

This looks like the clean "return to base scope panel" exit path above the
right-panel cluster.

Best current read:

- the real immediate control surface above the cluster is visible-state entry
  and exit, not `E1C = 1`

## 5. Updated staged hierarchy

The right-panel family now looks most like this:

1. entry/reset shims
   - `0x080065B2` -> visible state `5`
   - `0x08006592` -> visible state `2` + clear staged panel bytes
2. event cluster
   - adjust-prev / adjust-next
   - single-selection stage
   - mass-toggle stage
   - detailed commit `E1C = 2 -> 0x2A`
3. downstream coarse bridge
   - older broad owner converts `E1C = 2 -> E1C = 1`
   - queues another `0x2A`
4. controller consumption
   - state `5/6` consume `E1C`
   - `E1C = 1` can escalate through `0x2B -> state 5 -> state 12`

That is a better "above the cluster" model than assuming `E1C = 1` itself was
the primary entry point.

## 6. Practical implication

This narrows the next search usefully.

If the downloaded vendor image is representative here, then reproducing only the
coarse `E1C = 1` handoff would be skipping a stock staging step. The likely
stock order is:

- enter state `5`
- perform right-panel edits/staging
- commit to `E1C = 2`
- only then bridge into `E1C = 1` if the controller needs the coarser handoff

So the most informative next static trace is not "what writes `E1C = 1`?" It is
"what higher-level event family routes into `0x080065B2` and what routes into
the state-`5` cluster actions afterward?"

## 7. Best next move

The next RE pass should target three narrow questions:

1. what higher-level runtime path reaches the direct state-`5` entry shim at
   `0x080065B2`
2. what higher-level runtime path reaches the direct state-`2` return shim at
   `0x08006592`
3. whether any caller tree or adjacent branch can bypass the `E1C = 2`
   detailed-commit stage entirely, even if no direct `E1C = 1` writer is
   visible in the downloaded image

That is the shortest path from the recovered right-panel choreography to a
bench-reproducible stock control order.
