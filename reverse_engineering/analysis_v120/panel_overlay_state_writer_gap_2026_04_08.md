# Panel Overlay State Writer Gap 2026-04-08

Update 2026-04-09:
- this note is now partly superseded by
  [panel_overlay_state_writer_recovered_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/panel_overlay_state_writer_recovered_2026_04_09.md#L1)
- a raw-store sweep against the archived V1.2.0 app recovered a concrete direct
  `+0xE10 = 1` writer at:
  - raw `0x0800A810`
  - project/decompiled `0x08006810`
- that writer sits in a small helper beginning around:
  - raw `0x0800A7E4`
  - project/decompiled `0x080067E4`
- so the older “no clean direct writer” conclusion below should now be read as a
  historical boundary marker, not the current state of the RE

Purpose:
- re-check whether the downloaded app exposes a clean direct writer for
  `+0xE10` / `DAT_20000F08 == 1`
- decide whether the remaining `+0xE10 == 1` mystery is still a code-flow
  problem or is starting to look like a missing-data / hidden-dispatch problem

Key references:
- [overlay_seed_first_ui_consumer_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/overlay_seed_first_ui_consumer_2026_04_08.md#L1)
- [right_panel_redraw_cluster_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_redraw_cluster_force_2026_04_08.c#L1)
- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L1)
- [data_xrefs_right_panel_resource_family_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_right_panel_resource_family_2026_04_08.txt#L1)

## 1. Grounded Reads And Writes

The current grounded xrefs for `0x20000F08` are still sparse:

- reads in:
  - `0x080156D0`
  - `0x080157B4`
- write in:
  - `0x0801583E` inside `FUN_080157D0`

See:
- [data_xrefs_right_panel_resource_family_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/data_xrefs_right_panel_resource_family_2026_04_08.txt#L3)

Those redraw-ladder writes are all post-consume cleanup:

- `+0xE10 = 0xFF`

not the missing trigger-side `+0xE10 = 1` entry.

## 2. Known Runtime Writers Are `1`, `2`, `3`, `0`, Or `0xFF`

The resource/overlay commit family still shows:

- `+0xE10 = 3` on preview/metadata failure paths
- `+0xE10 = 2` on the clean append path

from:
- [scope_cluster_ctrl_force_2026_04_08.c](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_ctrl_force_2026_04_08.c#L1)

And the later raw-store sweep now adds:

- `+0xE10 = 1` from the helper at raw `0x0800A7E4..0x0800A82E`
  / project `0x080067E4..0x0800682E`
  when `DAT_20001060 / +0xF68 == 2`

The later redraw ladder then consumes:

- `1`
- `2`
- `3`
- `0xFF`

and clears the `1` branch back to `0xFF` after re-enumeration and redraw.

The key-loop slice interpretation also needs the later correction from
[raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md#L1):
inside raw `key_task` at `0x0803D064`, the `2/3` family is collapsed back to
`0`, not to a caller-provided arbitrary next state.

## 3. What This Means

This is now best read as a useful historical result:

- the first LCD-visible consumer of `+0xE10 == 1` is grounded
- the later `2/3/FF` writers are grounded
- and the decompile/xref surface had still not exposed the later recovered
  direct `+0xE10 = 1` writer

So the remaining uncertainty here is no longer “we have not looked in the right
obvious function.” It is increasingly one of:

1. an indirect dispatch/assignment path that the current project does not make
   cleanly visible
2. a hidden data-driven route tied to the missing descriptor families
3. a project/import limitation similar to the earlier shifted-data-table issues

## 4. Practical Conclusion

This is a good boundary marker for the current software-only trace surface.

We now have:

- the first filesystem normalization path
- the later descriptor enumeration path
- the first LCD-visible redraw consumer
- the later per-index metadata consumer

The exact higher owner above the recovered `+0xE10 = 1` helper is still
unresolved, but the direct trigger is no longer missing.

So the next best work is:

1. de-prioritize broad xref hunting for a missing literal `=1` writer
2. trace the higher owner around project `0x080067E4 / 0x08006840`
3. continue descriptor/data reconstruction for:
   - `0x080BC18B`
   - `0x080BCAE5`
   - `0x080BC859`
4. treat the remaining `+0xE10` gap as an ownership problem, not a missing
   primitive write
