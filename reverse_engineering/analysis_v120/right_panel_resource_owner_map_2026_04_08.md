# Right Panel Resource Owner Map 2026-04-08

## Summary

The runtime callers of the external-flash helpers are now grounded well enough to separate three different roles:

1. **boot-time overlay/resource enumeration**
2. **runtime overlay refresh / list rebuild**
3. **runtime per-entry preview build + metadata mount**

This makes the `FUN_08034878()` / `FUN_08035ED4()` family look much less like a generic boot configuration path and much more like a **right-panel overlay/resource stack**. It also makes the empty external-flash `9999.BIN` placeholder look even less likely to be the direct explanation for the stock-app black screen.

One important correction applies to every raw-app check in this note:

- the decompiled/project addresses are being used as if the app were loaded at `0x08000000`
- the archived V1.2.0 app binary has to be raw-disassembled at `0x08004000`
- so **raw app checks require adding `0x4000`** to the decompiled address

See:

- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)

## 1. Boot-Time Enumeration: `FUN_08034878(0x080BC18B)`

The cleanest in-range boot call is still in the master init disassembly:

- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5589)

At `0x08027294`, stock does:

```c
FUN_08034878(0x080BC18B);
DAT_20000F13 = return_byte;
```

That confirms one real use of `FUN_08034878()` is to enumerate a descriptor-backed list at boot and seed:

- `DAT_20000F13` / `panel_entry_count`

This matches the current RAM-map entry:

- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L253)

## 2. Runtime Overlay Refresh: `DAT_20000F08 == 1`

The broad redraw/overlay ladder contains a second, clearly runtime call:

- [high_flash_pass1_force_scope_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass1_force_scope_gap_functions.c#L4780)

When `DAT_20000F08 == 1`, stock:

1. calls `FUN_08034878(0x080BC18B)`
2. draws the current per-index label via `0x080BCAE5` and `DAT_20000F09`
3. sets `DAT_20000F08 = -1`
4. sets `DAT_20001060 = 10`
5. queues display selectors `0x24` then `0x03`

That makes this path look like:

- rebuild overlay/resource list
- render current entry label
- hand off back into the display/state machine

not like a scope command or a one-off config file load.

## 3. Runtime Resource Mode 11: paired `0x080BCAD2` / `0x080BC859`

The higher-flash resource gap pass shows a sibling runtime branch:

- [high_flash_pass2_force_resource_gap_functions.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_pass2_force_resource_gap_functions.c#L72)

When `DAT_20001060 == 0x0B`, stock walks a packed flag bank and, for each selected slot, formats and processes **two** path families:

- `0x080BCAD2`
- `0x080BC859`

After that loop, it does:

```c
DAT_20000F13 = FUN_08034878(0x080BC18B);
uRam20000f12 = 0;
_DAT_20000f14 = 0;
DAT_20001060 = 5;
```

So this branch looks like a **resource rebuild / overlay re-entry** path that feeds visible state `5`, not a low-level filesystem probe.

## 4. Runtime Per-Entry Commit Bridge: `0x0800A66A`

The forced slice at `0x0800A66A` is the clearest runtime owner of `FUN_08035ED4()` so far:

- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L9)

Behavior:

1. if `*(base + 0xE11) == -1`, set `*(base + 0xE10) = 3`
2. otherwise format `0x080BCAD2`
3. call `FUN_08036084()`
4. if that succeeds, call `FUN_08035ED4()`
5. on success:
   - set `*(base + 0xE10) = 2`
   - append current `+0xE11` into `+0xE25[]`
   - increment `+0xE1B`
6. finally force `*(base + 0xF68) = 2`

This is a runtime commit/append path over the same right-panel overlay state bytes already identified in:

- [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)

## 5. `FUN_08036084()` is a preview/resource writer

`FUN_08036084()` is not a trivial open helper:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29714)

It:

1. opens the currently formatted path
2. writes a `BM...` header block
3. allocates framebuffer-sized buffers
4. emits several display bands using repeated `FUN_0800BCD4(...)` calls
5. writes those pixel buffers out to the file

So the runtime `0x080BCAD2 -> FUN_08036084() -> FUN_08035ED4()` sequence is best read as:

- build/update a preview/resource file
- then load its matching per-index metadata blob

That is much more specific than a generic "SPI flash mount" story.

## 6. `FUN_08035ED4()` is the metadata side of the same path

`FUN_08035ED4()` still reads:

- formatted path family `0x080BC859`
- payload `0x25A` bytes into `0x2000044E`
- then a 5-byte tail

See:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)
- [external_flash_runtime_path_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/external_flash_runtime_path_map_2026_04_08.md#L38)

So the current best layered interpretation is:

- `0x080BC18B` = descriptor/list family
- `0x080BCAE5` = per-index display label family
- `0x080BCAD2` = preview/resource file family
- `0x080BC859` = per-index metadata family

The actual literal strings remain missing because these descriptors live above the end of the downloaded V1.2.0 app image.

## 7. What This Changes

This narrows the external-flash story:

- The traced runtime owners are **right-panel overlay/resource** code, not a direct scope-FPGA init path.
- The empty `9999.BIN` entry on the board is still interesting, but it is not the best current explanation for the stock-app boot failure.
- The strongest current explanation is still that the downloaded vendor app is missing the high-flash descriptor payloads needed to drive these boot/runtime filesystem paths.

## 8. Sibling Runtime Owners: `0x0800AFBC` and `0x0800B10A`

A forced pass on the sibling `0x080BCAD2` sites confirms that `0x0800A66A` is not alone:

- [right_panel_resource_siblings_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_siblings_force_2026_04_08.c#L1)

### `0x0800AFBC`

This slice is a **multi-slot staged resource rebuild**:

1. walks the packed flag bank at `+0xE1A`
2. for each selected bit, formats both:
   - `0x080BCAD2`
   - `0x080BC859`
3. calls `FUN_0802E12C()` after each formatted path
4. rebuilds the descriptor list with `FUN_08034878(0x080BC18B)`
5. clears:
   - `+0xE1A`
   - `+0xE1C`
6. forces visible state `5`

That is the runtime bitmap-driven counterpart to the earlier high-flash gap interpretation.

### `0x0800B10A`

This slice is the **single-selection version** of the same family:

1. formats one `0x080BCAD2` path
2. calls `FUN_0802E12C()`
3. formats one `0x080BC859` path
4. calls `FUN_0802E12C()`
5. rebuilds the list with `FUN_08034878(0x080BC18B)`
6. clears:
   - `+0xE1A`
   - `+0xE1C`
7. forces visible state `5`

So the current family split is now:

- `0x0800A66A` = per-entry preview build + metadata mount + append
- `0x0800AFBC` = bitmap/multi-slot rebuild + return to state `5`
- `0x0800B10A` = single-slot rebuild + return to state `5`

### `0x0800AD26`

This sibling is less directly a metadata path, but still fits the same overlay family:

1. calls `FUN_0802A534()`
2. sets up a small viewport / buffer
3. waits on `_DAT_20002D84`
4. then dispatches via a callback or indexed table

So it currently looks more like a **resource/preview display owner** than another raw metadata writer.

### Pointer-table check

A raw binary scan of the downloaded app image did **not** find plain little-endian Thumb pointers to:

- `0x0800A66A`
- `0x0800AFBC`
- `0x0800B10A`
- `0x0800AD26`

So the current best read is that this family is probably reached through a wider owner, jump table, or normalized dispatch path rather than a simple direct callback table embedded in the image.

## 9. State-Byte Anchors in Normal-Sized Functions

A targeted RAM-xref pass helps connect the forced sibling slices back into the normal display/state code:

- [data_xrefs_right_panel_resource_family_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_right_panel_resource_family_2026_04_08.txt#L1)

### `+0xE10` / `DAT_20000F08`

`DAT_20000F08` still only has one grounded write in the current functionized project:

- `0x0801583E` inside [FUN_080157D0](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_cluster_neighbors_2026_04_08.c#L1)

That is the same redraw bridge that:

- draws the per-index label via `0x080BCAE5`
- sets `DAT_20001060 = 10`
- queues selectors `0x24` then `0x03`

and it also has nearby reads at:

- `0x080156D0`
- `0x080157B4`

So `+0xE10` now looks like a **local overlay/rebuild control byte** consumed inside the redraw cluster, not a broad top-level mode byte.

### `0x080156D0` is the first real-sized redraw owner

A forced pass on the earlier `+0xE10` read sites shows that `0x080156D0` is the first normal-sized owner above the tiny redraw shims:

- [right_panel_redraw_cluster_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_redraw_cluster_force_2026_04_08.c#L1)

`FUN_080156D0()`:

1. reads `*(base + 0xE10)`
2. redraws the right-panel invalidation rectangles when:
   - `+0xE10 != 0/-1`
   - or several related overlay/status bytes are active
3. if `+0xE10 == 1`, it:
   - rebuilds the list with `FUN_08034878(0x080BC18B)`
   - renders the current per-index label via `0x080BCAE5`
   - sets `+0xE10 = 0xFF`
   - forces `DAT_20001060 = 10`
   - queues `0x24` then `0x03`

So `0x080157D0` is no longer the best top-level redraw anchor here. It is more accurate to treat:

- `0x080156D0` as the broader overlay redraw/controller body
- `0x080157B4` / `0x080157D0` as narrower sibling slices inside the same cluster

That is the cleanest bridge so far between:

- hidden resource rebuild handlers like `0x0800AFBC`
- the overlay control byte `+0xE10`
- and the visible display handoff into state `10`

### Hidden state-assignment bridge at `0x08039058`

There is also a useful hidden helper slice inside the broader runtime loop:

- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L143)

`FUN_08039058()` does:

1. read `bVar1 = *(base + 0xE10)`
2. if `(bVar1 & 0xFE) == 2`, rewrite:
   - `*(base + 0xE10) = (char)unaff_r5`
3. if side-panel flags are clear, dispatch through an indirect handler table
4. update another packed state word at `+0xF6C`

After correcting for the app-slot base offset, the raw archived app image
supports this interpretation: decompiled `0x08039058` corresponds to raw
`0x0803D058`, which is the interior overlay-state slice inside the same key-loop
body as decompiled `0x08039008`.

This is important because it gives a plausible path for:

- transient `2 / 3` overlay states
- collapsing into some caller-selected next state

without requiring a direct literal `*(base + 0xE10) = 1` store in the visible decompilation.

So the best current read is still:

- `+0xE10 = 1` may be arriving through this hidden state-assignment bridge
- not through a simple standalone writer that Ghidra has already surfaced cleanly

### `+0xE1B` / `DAT_20000F13`

`DAT_20000F13` is written by:

- boot/runtime list enumeration in `FUN_08034878()`
- the redraw bridge in `FUN_080157D0()`
- the forced sibling handlers `0x0800A66A`, `0x0800AFBC`, and `0x0800B10A`

So it is the clearest current "entry count / append cursor" byte for this family.

### `+0xE1C` / `DAT_20000F14`

`DAT_20000F14` is consumed by a normal display function:

- [FUN_0801819C](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L5227)

That function renders a two-choice panel highlight based on whether `DAT_20000F14 == 1`, which makes `+0xE1C` look like a **panel subview / choice selector** downstream of the resource-family rebuilds.

This is useful because it ties the hidden/forced resource slices back into ordinary UI drawing code, even though the actual writers for `+0xE1A` and `+0xE1C` are still sparse in the functionized project.

## 10. The redraw slices sit inside a broader owner

Once the raw-app base offset is corrected, the archived app image shows that the
three redraw slices:

- `0x080156D0`
- `0x080157B4`
- `0x080157D0`

map to raw app addresses:

- `0x080196D0`
- `0x080197B4`
- `0x080197D0`

and they sit inside a broader owner that starts earlier, around:

- decompiled `0x08015640`
- raw `0x08019640`

The raw owner around `0x08019640..0x08019840` does:

1. initial panel/resource draw setup
2. a gate on `*(base + 0x34)` which matches:
   - `DAT_2000012C`
   - [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L81)
   - current label: `display_mode`
3. the `+0xE10` redraw/overlay branch family at `0x080196D0`
4. the `+0xE10 == 1` rebuild path at `0x080197C4`
5. the `+0xE11` label/render handoff at `0x080197D0`

So the next clean trace surface is no longer just the isolated redraw slices.
It is the larger owner around decompiled `0x08015640`.

### `display_mode` is now upstream of this owner

The `+0x34` gate in this broader redraw/resource owner is now better grounded:

- [display_mode_latch_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/display_mode_latch_map_2026_04_08.md)

The important correction is that the later redraw/resource owner appears to
**consume** `display_mode`, not decide it.

The cleanest grounded runtime set/clear sites now live earlier in the broader
scope controller family:

- decompiled `0x080055AE` / raw `0x080095AE`
- decompiled `0x08005732` / raw `0x08009732`

That means the right-panel redraw/resource stack should now be read as
downstream of the scope controller's display latch, rather than as the source
of that latch.

## Best Next Steps

1. Trace the larger owner around `0x0800A66A` instead of treating it as a standalone function.
2. Trace the sibling `0x080BCAD2` sites called out in the high-flash scan:
   - `0x0800AD26`
   - `0x0800AFBC`
   - `0x0800B10A`
   These are likely peers in the same overlay/resource family.
3. Find the higher-level state/menu path that chooses between the:
   - per-entry append path (`0x0800A66A`)
   - multi-slot rebuild path (`0x0800AFBC`)
   - single-slot rebuild path (`0x0800B10A`)
4. Trace the wider owner around `0x08015640` together with the hidden sibling writers, because that now looks like the cleanest bridge between:
   - hidden resource rebuild handlers
   - `+0xE10 / +0xE1B / +0xE1C`
   - visible panel draw behavior
5. Trace which local branch inside the earlier broad scope controller reaches
   the `display_mode` clear/set pair, because the redraw/overlay owner now
   looks downstream of that latch rather than upstream of it.
6. Decode how:
   - `DAT_20001063` (`0xF6B`)
   - `*(base + 0x23A)`
   - the low nibble of `*(base + 0x354)`
   feed the `display_mode = 1` path.
7. Keep using the hidden state-assignment slice `0x08039058` as a plausible transient `2 / 3 -> caller-selected state` bridge, but evaluate it with the corrected app-slot address mapping.
8. Keep `FUN_08034878()` and `FUN_08035ED4()` tied to the right-panel overlay/resource family in future notes.
9. Continue treating the missing high-flash descriptor region as the main blocker for reproducing stock boot/runtime behavior from the website app alone.
