# Right-Panel Stage Helper Convergence 2026-04-08

## Summary

This pass answers one narrow question from the newer right-panel model:

- do the two staged-detail helpers feed the same packed-selector commit family?

The strongest current answer is:

- yes, they appear to converge on the same `sub2 = 5` commit bridge

More specifically:

- single-selection stage at raw `0x0800A19E..0x0800A236`
- mass-toggle stage at raw `0x0800A326..0x0800A382`

both produce the same downstream preconditions:

- `E1C == 0`
- `E1A != 0`
- bitmap across `0xE12..0xE19` is nonzero

and the `sub2 = 5` sibling at raw `0x0800A468..0x0800A4C8` only tests those
shared preconditions. It does **not** branch on `E1A == 1` versus `E1A == 2`.

That is a useful tightening because it means the unresolved split is probably
not "which packed-selector family does stage helper A choose versus stage helper
B?" The more likely remaining split is earlier:

- what runtime event flips the editor from coarse `E1A == 0` into any staged
  detail posture at all?

Primary references:

- [right_panel_event_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_event_cluster_2026_04_08.md)
- [right_panel_internal_branch_choice_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_internal_branch_choice_2026_04_08.md)
- [right_panel_sub2_5_vs_2_split_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_sub2_5_vs_2_split_2026_04_08.md)

## 1. Single-selection stage produces a valid staged-detail posture

The raw branch at `0x0800A19E..0x0800A236` does:

- require visible `state 5`
- require `E1C == 0`
- require `E1B != 0`
- if `E1A == 0`:
  - set `E1A = 1`
  - clear `0xE12..0xE19`
  - set exactly one selected bit from `E1D / 6` and `E1D % 6`
- if `E1A != 0`:
  - clear `E1A`
- then emit `0x28`
- then emit `0x26`

The important part for the later commit path is:

- `E1A` becomes nonzero
- bitmap becomes nonzero

That already satisfies the visible-state-`5` commit-side checks in the
`sub2 = 5` sibling.

## 2. Mass-toggle stage produces the same downstream shape

The sibling branch at `0x0800A326..0x0800A382` does:

- require visible `state 5`
- require `E1C == 0`
- require `E1B != 0`
- if `E1A == 2`:
  - write `E1A = 1`
  - clear `0xE12..0xE19`
- otherwise:
  - write `E1A = 2`
  - fill `0xE12..0xE19` with the repeated mask `0x3F3F3F3F`
- then emit `0x26`
- then emit `0x28`

Again, the important downstream result is:

- `E1A` becomes nonzero
- bitmap becomes nonzero

So despite different staging semantics, both helpers create the same broad
commit-side posture.

## 3. The `sub2 = 5` sibling only checks the shared posture, not the exact stage type

The visible-state-`5` path inside the `sub2 = 5` sibling at
`0x0800A468..0x0800A4C8` checks:

- `E1C == 0`
- `E1A != 0`
- OR across `0xE12..0xE19`
- if nonzero:
  - write `E1C = 2`
  - queue `0x2A`

What it does **not** do:

- compare `E1A` against `1`
- compare `E1A` against `2`
- inspect which particular bits were chosen, beyond the aggregate
  nonzero-bitmap test

That makes the convergence point much firmer:

- both stage helpers feed the same `sub2 = 5` commit-side family

## 4. The difference between the two helpers is local editing semantics

The difference between `0x0800A19E` and `0x0800A326` still matters, but it now
looks local to the editor behavior:

| Helper | Local editor meaning | Shared downstream result |
|---|---|---|
| `0x0800A19E` | single-selection from coarse cursor | `E1A != 0`, bitmap nonzero |
| `0x0800A326` | mass-toggle / broad selection stage | `E1A != 0`, bitmap nonzero |

So the stock question is probably not:

- "which one leads to selector `5`?"

It is more likely:

- "which user/runtime action chooses single-selection versus mass-toggle before
  they both hit the same commit-side family?"

## 5. What this changes

This simplifies the next branch.

We do **not** need to keep treating the two stage helpers as potential forks to
different packed-selector families. The stronger current model is:

1. visible `state 5` editor
2. choose a staged-detail helper:
   - single-selection
   - mass-toggle
3. both create staged-detail posture
4. both can feed `sub2 = 5`
5. commit bridge raises `E1C = 2` and queues `0x2A`

So the unresolved split moves one step earlier in the state machine.

## Best Next Move

1. Trace what runtime/event path chooses single-selection (`0x08006120`) versus
   mass-toggle (`0x080062F8`) inside visible `state 5`.
2. Keep treating `sub2 = 5` as their shared packed-selector commit family
   unless new raw evidence shows an exact `E1A == 1 / 2` split later.
3. Compare those two stage-helper entry paths against the generic adjust-prev /
   adjust-next event owners, because that is now the likeliest place where the
   real user-action distinction still lives.
