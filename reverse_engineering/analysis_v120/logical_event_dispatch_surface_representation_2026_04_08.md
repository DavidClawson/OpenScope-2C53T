# Logical Event Dispatch Surface Representation 2026-04-08

## Scope

Reconstruct the true representation of the logical-event dispatch surface used
by `key_task`, with focus on runtime literal region:

- `0x08046544/48`

and the corrected project-data rule:

- `project_addr = runtime_literal - 0x4000`

Primary references:

- [key_task_dispatch_surface_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/key_task_dispatch_surface_contradiction_2026_04_08.md)
- [button_event_dispatch_reaudit_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/button_event_dispatch_reaudit_2026_04_08.md)
- [ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md)

## Executive Summary

The corrected address model removes the last easy "wrong bytes" escape hatch.

The strongest current result is:

- the disputed `key_task` / button-scan surface in the downloaded V1.2.0 app
  is best interpreted as a metadata run, not as the live dispatch table

More specifically:

1. the code producers are real and directly reference runtime `0x08046528` and
   `0x08046548`
2. raw app bytes and corrected Ghidra project bytes agree on those surfaces
3. the first 16 `key_task` dispatch slots are all impossible direct handlers
4. the wider neighborhood decodes like a structured PREL31-style metadata run,
   much closer to unwind/extab data than to a byte map or flat handler table

That means the current best interpretation is **not**:

- a lingering project-base mistake
- a plain `btn_map`
- a plain handler table

The most defensible interpretation now is:

- in the downloaded website app, this region is metadata-like data
- the live dispatch representation must therefore be elsewhere, materialized
  later, or absent from this standalone app image

## 1. The code producers are still real

Two raw code slices matter here.

### `key_task`

Raw app:

- `0x0803D008..0x0803D09A`

Key sequence:

```asm
0x0803D012: movw  r8, #0x6548
0x0803D028: movt  r8, #0x0804
...
0x0803D08C: ldrb.w r0, [sp, #7]
0x0803D090: add.w  r0, r8, r0, lsl #2
0x0803D094: ldr.w  r0, [r0, #-4]
0x0803D098: blx    r0
```

So `key_task` really does:

- consume a one-byte logical event
- index a 4-byte stride surface rooted at runtime `0x08046548`
- load from `[slot - 4]`
- branch indirectly through the loaded word

### Button scan producer

Raw app:

- `0x0803D382..0x0803D40C`

Key sequence:

```asm
0x0803D382: movw  sl, #0x6528
0x0803D392: movt  sl, #0x0804
...
0x0803D3D2: add.w  r5, sl, r6
0x0803D3D6: ldrb   r5, [r5, #15]
...
0x0803D3E8: ldrb.w r1, [sl, r6]
...
0x0803D406: ldrb.w r1, [sl, r6]
```

So the button-scan path also really does treat runtime `0x08046528` as a byte
surface.

This is a real contradiction inside the downloaded app image, not a stale
decompiler-only artifact.

## 2. The corrected address rule rules out the old project-base excuse

For the current flat-import Ghidra project:

- runtime `0x08046528` maps to project `0x08042528`
- runtime `0x08046544/48` map to project `0x08042544/48`

That correction is already proven in
[ghidra_project_data_target_shift_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/ghidra_project_data_target_shift_2026_04_08.md).

The important consequence here is:

- raw app bytes and corrected project bytes now agree on the disputed surface

So for this question, "overlay/misbased Ghidra artifact" is no longer the best
explanation.

## 3. The first 16 `key_task` slots are all impossible direct handlers

Using the raw `key_task` dispatch formula:

- `slot_addr = 0x08046548 + event_id * 4 - 4`

the first 16 loaded words are:

| Event ID | Slot address | Loaded word |
|---:|---:|---:|
| 0 | `0x08046544` | `0xFFFFF08B` |
| 1 | `0x08046548` | `0x0000084D` |
| 2 | `0x0804654C` | `0xFFFFFB4B` |
| 3 | `0x08046550` | `0x00000295` |
| 4 | `0x08046554` | `0xFFFFFEB2` |
| 5 | `0x08046558` | `0x0000009A` |
| 6 | `0x0804655C` | `0xFFFFFFAE` |
| 7 | `0x08046560` | `0xFFFFFF95` |
| 8 | `0x08046564` | `0x000000E4` |
| 9 | `0x08046568` | `0xFFFFFE23` |
| 10 | `0x0804656C` | `0x0000038B` |
| 11 | `0x08046570` | `0xFFFFF9AF` |
| 12 | `0x08046574` | `0x00000B44` |
| 13 | `0x08046578` | `0xFFFFE917` |
| 14 | `0x0804657C` | `0x00007058` |
| 15 | `0x08046580` | `0x000029F4` |

None of those are plausible direct Thumb targets in the app image.

That strongly rules out:

- a flat direct handler table
- a mildly scrambled direct handler table

It also weakens the "more exotic dispatch representation" hypothesis unless a
missing translation step is found between `ldr` and `blx`. No such translation
is present in the raw `key_task` slice.

## 4. The wider region behaves like structured PREL31 metadata

The wider neighborhood:

- runtime `0x080464DC..0x080466A0`

does not look random. It behaves like a structured metadata run.

Representative words:

| Runtime address | Raw word | PREL31 target |
|---:|---:|---:|
| `0x08046528` | `0xFFFFFE2F` | `0x08046357` |
| `0x0804652C` | `0x00000372` | `0x0804689E` |
| `0x08046544` | `0xFFFFF08B` | `0x080455CF` |
| `0x08046548` | `0x0000084D` | `0x08046D95` |
| `0x0804654C` | `0xFFFFFB4B` | `0x08046097` |
| `0x08046550` | `0x00000295` | `0x080467E5` |

That is a much better fit for PREL31-based metadata than for live event tables.

An additional useful signal:

- if the `+4 mod 8` lane is walked from runtime `0x080464E4`, 34 of the next
  39 PREL31-decoded targets are nondecreasing

That is not proof of a canonical `.ARM.exidx` section by itself, but it is
substantially closer to unwind/extab-style metadata than to:

- a byte `btn_map`
- a 4-byte handler table
- a small logical-event lookup surface

## 5. Best current interpretation

The three candidate explanations rank like this:

### 1. True metadata run in the website app image

Most likely.

The corrected bytes look like metadata in both raw and project views, and both
producer sites land inside that same run.

This is the best fit for the data itself.

### 2. Overlay / project-base artifact

Lower.

This was a good suspicion earlier, but the corrected `-0x4000` rule now removes
it for this specific question. The raw file itself carries the same
metadata-looking bytes.

### 3. More exotic dispatch representation

Possible, but weaker.

For that to hold, there still has to be an additional transform between the
loaded word and the final callable target. The raw `key_task` slice does not
show one.

## 6. Practical consequence

For Track 1, the most defensible conclusion is:

- runtime `0x08046544/48` is not the live dispatch table in the downloaded
  website app image

The real unresolved question is now:

- where the live logical-event dispatch representation actually resides on a
  programmed stock unit, or how it is materialized from data not present in the
  standalone app image

That is a better next question than continuing to force this specific window
into a handler-table interpretation.

## Concrete Next Addresses

### Raw producer sites

- `0x0803D008..0x0803D09A` — `key_task` dispatch load and `blx`
- `0x0803D382..0x0803D40C` — button-scan reads from runtime `0x08046528`

### Corrected project/runtime metadata window

- runtime `0x080464DC..0x080466A0`
- project `0x080424DC..0x080426A0`

Treat this as one structured metadata span until proven otherwise.

### Highest-value follow-up outside the metadata span

If another real dispatch surface exists in the website app, it is more likely
outside that metadata window:

- runtime `0x08046300..0x080464DB`
- runtime `0x080466A0..0x08046920`

### If a programmed stock image ever becomes available

Compare these exact surfaces first:

- runtime `0x08046528`
- runtime `0x08046544..0x08046580`
- runtime `0x080464DC..0x080466A0`

That will answer quickly whether this contradiction is intrinsic to the
website app image or only to our current available artifact set.
