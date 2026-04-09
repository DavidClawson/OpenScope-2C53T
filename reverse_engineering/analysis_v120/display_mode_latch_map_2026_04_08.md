# Display Mode Latch Map 2026-04-08

## Summary

`DAT_2000012C` (`display_mode`, `+0x34`) now looks less like a broad top-level
"what screen are we in?" byte and more like a **downstream render/overlay
latch** that is controlled by the earlier scope runtime controller family.

The useful correction is:

1. the later redraw/resource owner around decompiled `0x08015640`
   (raw `0x08019640`) only **consumes** `display_mode`
2. the cleanest grounded runtime writers live earlier, immediately after the
   shared tail of the broad scope controller around:
   - decompiled `0x080055AE`
   - raw `0x080095AE`
   - decompiled `0x08005732`
   - raw `0x08009732`
3. those writers sit in the same wider family that already owns visible
   `state 4 / 5 / 6`, not in a separate overlay-only subsystem

So the redraw/resource stack should now be treated as **downstream of the scope
controller's display-mode latch**, not as the origin of that latch.

Primary references:

- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)
- [display_mode_posture_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_posture_cluster_2026_04_08.md)
- [scope_top_level_gate_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_top_level_gate_writers_2026_04_08.md)
- [scope_runtime_controller_case_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_runtime_controller_case_map_2026_04_08.md)
- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md)
- [mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_scope_state_cluster_08004D40_080056C0_2026_04_08.txt)

## 1. Address correction first

All raw checks in this note use the app-slot offset rule from:

- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

For the archived V1.2.0 stock app:

- `raw_app_addr = decompiled_addr + 0x4000`

So the useful set/clear pair is:

- decompiled `0x080055AE` -> raw `0x080095AE`
- decompiled `0x08005732` -> raw `0x08009732`

## 2. Clear path: decompiled `0x080055AE`, raw `0x080095AE`

Raw disassembly:

```asm
0x080095AE: ldrb.w r0, [r5, #52]    ; +0x34 = DAT_2000012C
0x080095B2: cmp     r0, #0
0x080095B4: beq.w   0x08009732
0x080095B8: movs    r0, #0
0x080095BA: strb.w  r0, [r5, #52]   ; display_mode = 0
0x080095BE: b.n     0x08009758
```

This is the cleanest grounded runtime clear so far:

- if `display_mode != 0`, clear it back to `0`
- then fall into the same downstream local flow that later reaches the
  preview/list handling around `0x08009758`

That means the controller is explicitly normalizing the latch before entering
the downstream render/resource logic.

## 3. Set path: decompiled `0x08005732`, raw `0x08009732`

Raw disassembly:

```asm
0x08009732: ldrb.w r0, [r5, #852]   ; +0x354
0x08009736: movs    r1, #1
0x08009738: and.w   r2, r0, #0x0F
0x0800973C: cmp     r2, #3
0x0800973E: strb.w  r1, [r5, #52]   ; display_mode = 1
0x08009742: bne.n   0x08009758
0x08009744: and.w   r0, r0, #0xF0
0x08009748: strb.w  r0, [r5, #852]  ; clear low nibble of +0x354
0x0800974C: movw    r0, #0x2D50
0x08009754: movs    r1, #0
0x08009756: strh    r1, [r0]
```

So the cleanest grounded runtime set is:

- if the earlier branch found `display_mode == 0`, set it to `1`
- if the low nibble of `+0x354` is `3`, clear that nibble and zero queue
  scratch at `0x20002D50`
- then continue into the same downstream list/preview flow

This makes `display_mode = 1` look much more like a **latched entry posture**
into a later render/preview path than a free-standing UI preference byte.

## 4. Why this belongs to the broad scope controller family

These writes are not floating in isolation.

In the same raw window:

- `0x08009586..0x080095A8` is the shared exit tail of the broad controller
- that tail still contains the known visible-state normalization and the
  `visible state 4 -> selector 0x21` follow-up
- the `display_mode` clear/set pair begins immediately after that tail

So the best current structural read is:

1. broad scope controller handles visible `state 4 / 5 / 6`
2. the same broader family then clears or sets `display_mode`
3. only after that does the later redraw/resource stack consume the latch

This is much tighter than the earlier idea that `display_mode` might be chosen
inside the downstream overlay owner itself.

## 5. Relationship to the later redraw/resource owner

The broader redraw/resource owner around:

- decompiled `0x08015640`
- raw `0x08019640`

still gates on `*(base + 0x34)` before entering the `+0xE10` redraw family, as
already documented in:

- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md)

But the new result changes the direction of causality:

- the redraw/resource owner **does not appear to decide** `display_mode`
- it **inherits** a `display_mode` posture from the earlier controller family

So the most accurate current layered model is:

1. broad scope controller decides whether the display/render latch should be `0`
   or `1`
2. later redraw/resource owner checks that latch
3. later redraw/resource owner then performs the overlay/list/preview work

## 6. What `display_mode` now most likely means

I would keep the `display_mode` label for continuity, but soften its meaning.

Best current read:

- **not** a simple global "screen type" selector
- **not** the primary owner of the right-panel overlay path
- **more likely** a render/overlay posture latch that controls whether the
  downstream preview/resource/display path is active

This also fits the set/clear asymmetry:

- it is cleared if already active
- it is set to `1` only through the branch that also consults `+0x354`

That behavior looks more like a staged post-action render mode than a passive
user preference.

## 7. New dependency worth watching: `+0x354`

The set path ties `display_mode = 1` to the low nibble of `+0x354`.

Current grounded facts:

- `display_mode = 1` is always written at the set site
- if `(+0x354 & 0x0F) == 3`, stock additionally:
  - clears the low nibble of `+0x354`
  - zeros `0x20002D50`

So `+0x354` is now one of the better local dependency candidates above the
downstream redraw/resource owner.

I am not promoting a strong semantic label for `+0x354` yet, but it now looks
more like a packed action/posture byte than a passive display setting.

## 8. Adjacent preview/state cluster after the latch pair

The latch pair does not stand alone.

Immediately after the clear/set sites, the same raw region enters a small state
cluster keyed on `+0x3C`, `+0x40`, `+0x35`, and `F6B`:

- `0x08009758` gates on `*(base + 0x3C)`
- `0x08009762` reads the pointer/list head at `*(base + 0x40)`
- `0x08009702..0x08009714` compares `*(base + 0x35)` against `F6B` and clears
  `+0x35` when they match
- `0x08009716..0x08009730` mirrors `+0x3C` against `F6B`, either copying the
  current `F6B` value into `+0x3C` or clearing `+0x3C` back to `0`

So the current best local read is:

- `+0x34`, `+0x35`, `+0x3C`, and `+0x40` form a small downstream
  display/preview posture cluster
- the broad controller family appears to normalize that cluster before the
  later redraw/resource owner consumes it

This makes the next local trace surface sharper than "just follow `display_mode`
alone."

The newer cluster note:

- [display_mode_posture_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_posture_cluster_2026_04_08.md)

pushes that one step further: the whole `0x080095AE..0x08009B1E` region now
looks more like a trigger-side posture/preview cluster than a generic display
helper.

## 9. What this changes

This narrows the next search substantially.

The better upstream question is no longer:

- "who sets `+0xE10 = 1` before the redraw owner runs?"

It is now:

- "what inbound branch in the broad scope controller reaches the
  `display_mode` clear/set pair, and how do `F6B`, `+0x23A`, and `+0x354`
  control that branch?"

That is the next clean bridge above the later redraw/resource owner.

## Best Next Move

1. Trace the local branch path that reaches raw `0x080095AE` and `0x08009732`
   inside the broader controller family.
2. Decode the role of:
   - `DAT_20001063` (`0xF6B`)
   - `*(base + 0x23A)`
   - the low nibble of `*(base + 0x354)`
3. Trace the small downstream posture cluster around:
   - `+0x35`
   - `+0x3C`
   - `+0x40`
   and how it mirrors or derives from `F6B`.
4. Keep the later redraw/resource owner around decompiled `0x08015640` as a
   downstream consumer, not the upstream source of the display-mode decision.
