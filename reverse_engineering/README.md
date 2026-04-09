# Reverse Engineering Reference

This directory contains analysis of the FNIRSI 2C53T stock firmware, performed via Ghidra decompilation for the purpose of hardware interoperability.

## Legal Basis

This reverse engineering was conducted for **interoperability purposes** — to understand the hardware interfaces (FPGA protocol, ADC configuration, peripheral initialization, button matrix, etc.) needed to write compatible replacement firmware.

- **US:** Reverse engineering for interoperability is protected under fair use (*Sega v. Accolade*, 9th Cir. 1992; *Sony v. Connectix*, 9th Cir. 2000)
- **EU:** Software Directive (2009/24/EC), Article 6 explicitly permits decompilation for interoperability

No FNIRSI source code is redistributed in this repository. The raw Ghidra decompilation output is excluded from version control. What is published here is **original analytical work**: hardware documentation, protocol specifications, annotated code fragments, and architectural analysis derived from studying the binary.

The replacement firmware in `firmware/src/` is a clean-room implementation that does not copy FNIRSI code.

## Contents

### Documentation
- `ARCHITECTURE.md` — System architecture overview (start here)
- `HARDWARE_PINOUT.md` — Complete MCU pin assignments
- `FPGA_PROTOCOL_COMPLETE.md` — Full FPGA command/data protocol specification
- `CALIBRATION.md` — ADC calibration data format and pipeline
- `HARDWARE_TESTS.md` — Hardware probing procedures and results
- `COVERAGE.md` — RE coverage tracker (309 functions catalogued)

### Analysis (V1.2.0 firmware)
  - `analysis_v120/` — Detailed analysis of the latest firmware version
  - `FPGA_TASK_ANALYSIS.md` — FPGA task state machine and sub-functions
  - `FPGA_BOOT_SEQUENCE.md` — 53-step boot initialization sequence
  - `fpga_task_annotated.c` — Annotated FPGA task with commentary
  - `button_map_confirmed.md` — Hardware-confirmed button matrix mapping
  - `function_names.md` — Complete function naming inventory
  - `function_map_complete.txt` — 309-entry function address map
  - Recent scope/dispatch notes:
    - `right_panel_handoff_directness_2026_04_08.md` — shows that `E1C = 1` still looks derived from a prior `E1C = 2` commit in the downloaded app, and promotes the visible state-`5` / state-`2` shims as the cleaner search surface above the right-panel cluster
    - `right_panel_sub2_5_vs_2_split_2026_04_08.md` — compares the `sub2 = 5` and `sub2 = 2` packed-selector siblings and argues that `5` is the staged-detail commit bridge while `2` is the coarse editor entry/exit family
    - `right_panel_internal_branch_choice_2026_04_08.md` — explains the downstream `sub2 = 5` versus `sub2 = 2` split inside visible state-`5`, with a later correction that the single-selection versus mass-toggle choice itself appears to come from separate sibling event owners
    - `right_panel_stage_helper_convergence_2026_04_08.md` — shows that the single-selection and mass-toggle stage helpers both appear to feed the same `sub2 = 5` commit family, pushing the remaining distinction back to their entry events
    - `right_panel_stage_entry_event_split_2026_04_08.md` — promotes raw `0x0800A120` and `0x0800A2F8` to direct sibling event owners, parallel to adjust-prev / adjust-next, so the remaining unresolved split is higher event routing into those owners
    - `right_panel_scope_choreography_path_2026_04_08.md` — compact end-to-end runtime sequence for the visible state-`5` right-panel family: editor posture, staged detail, `E1C = 2 -> 0x2A` commit, packed `sub2 = 5 / 2` consume-collapses, and the current queue/raw-word gaps
    - `logical_event_dispatcher_hypothesis_2026_04_08.md` — strongest current two-stage model: button scan produces logical event IDs, `key_task` gates and dispatches those IDs through an opaque surface, and the selected semantic owner then branches again on visible runtime state
    - `missing_image_state_hypothesis_ranked_2026_04_08.md` — ranks missing MCU-side high-flash data and non-app board state above wrong scope sequencing as the best explanation for the current blocker, and prioritizes W25Q128 boot-bus capture plus second-unit flash comparison
    - `right_panel_event_cluster_2026_04_08.md` — unifies the right-panel event peers into one staged cluster: coarse `E1D` edits, single-bit staging, mass-toggle staging, `E1C = 2 -> 0x2A` commit, and later controller consumption
    - `ghidra_project_data_target_shift_2026_04_08.md` — proves the current Ghidra project imported the app flat at `0x08000000`, so absolute flash literals from code must be checked at `project_addr = runtime_literal - 0x4000` to land on the right bytes
    - `key_task_dispatch_surface_contradiction_2026_04_08.md` — shows that raw `key_task` still does `ldr -> blx`, but the literal `0x08046544/48` region looks exidx-like rather than handler-like, sharpening the event-dispatch contradiction in the downloaded app
    - `right_panel_event_owner_map_2026_04_08.md` — promotes `0x080041F8 / 0x080047CC` to the stock adjust-prev / adjust-next owner pair, pairs them with the direct stage-side owners at raw `0x0800A120 / 0x0800A2F8`, and now treats `0x08046544/48` as an unresolved dispatch surface rather than a confirmed plain handler table
    - `right_panel_event_path_2026_04_08.md` — action-level model for the right-panel scope family: state-`5` entry, `E1D` cursor edits, `E1A` staged-detail latch, and the `E1C = 2 -> 0x2A` commit handoff
    - `panel_subview_action_meaning_2026_04_08.md` — interprets `E1C = 1` as the coarse right-panel/timebase handoff and `E1C = 2` as the staged bitmap/toggle handoff, with a caveat that exact UI labels still depend on missing high-flash resources
    - `panel_subview_writer_bridge_2026_04_08.md` — traces concrete runtime writers for `+0xE1C` and shows how `E1C = 1` versus `E1C = 2` feed the state-`5` / state-`6` controller cases through selector `0x2A`
    - `scope_runtime_controller_case_map_2026_04_08.md` — decodes the `0x08004D60` jump-table cases and shows `state 5` as the stable right-panel editor, `state 6` as its handoff phase, and `state 7` as a transient shim above `state 4`
    - `state9_preview_posture_case_map_2026_04_08.md` — corrects the later posture-family owner: `state 8` is only a cleanup shim, while visible `state 9` is the real staged preview/posture owner with an `F69` phase gate and an `F6A` low-nibble subtable
    - `state9_entry_bridges_and_preview_subcases_2026_04_08.md` — grounds the concrete visible-state-`2 -> state-9` promotion helpers and splits the `F6A = 3 / 4` preview side into heavy rebuild, normalize/apply, and tiny-toggle subcases
    - `runtime_owner_cluster_extension_2026_04_08.md` — extends the newer sibling-owner model to include the adjacent `sub2 = 5` staged-detail family and the packed-state normalizer at raw `0x0800A834`
    - `scope_top_level_gate_writers_2026_04_08.md` — promotes `0x08004D60` to the best current broad runtime owner above the `state 5 / 2 / 1` families, with `0x08006840` reclassified as a reset/entry normalizer
    - `runtime_scope_bank_reentry_2026_04_08.md` — raw call-site correction showing the shared bank emitter at `0x0800B908` is re-entered at runtime after staging scope presets
    - `scope_runtime_family_gate_map_2026_04_08.md` — maps the three runtime preset families back to their visible top-level gate states: `state 5` timebase/right-panel, `state 2` acquisition, `state 1` mixed trigger-like
    - `scope_runtime_preset_action_map_2026_04_08.md` — first action-level split of the runtime preset families: `sub2=2` timebase-side, `sub2=3` acquisition-side, `sub2=4` still mixed/trigger-like
    - `state2_promotion_owner_family_map_2026_04_08.md` — compares the three visible-state-`2 -> state-9` promotion owners as sibling runtime families, shows their distinct visible-state-`1` setup sides, and argues that the real chooser is probably one layer above them
    - `scope_owner_ui_action_map_2026_04_08.md` — first-pass mapping from the broad owner at `0x08003148` to likely acquisition, timebase, trigger-like preview, and packed-state commit families
    - `scope_owner_start_map_2026_04_08.md` — corrected owner starts, with `0x08003148` as the broad packed-scope owner and `0x08006418 / 0x08006548` as narrower transition shims
    - `mixed_scope_handler_bridge_2026_04_08.md` — `FUN_08002FE8` as the current best bridge between `DAT_20001060`, packed scope state, and selector-bank emission
    - `scope_composite_state_presets_2026_04_08.md` — raw 32-bit and 16-bit presets across `0xF68..0xF6B`, including the `9/1/3/0`, `9/1/2/0`, and `9/1/4/0` transition layouts
    - `scope_preset_owner_families_2026_04_08.md` — groups the new preset helpers into one broad submenu owner plus narrower `9 -> 2` transition families for selector targets `2 / 3 / 4 / 5`
    - `dynamic_scope_word_builder_2026_04_08.md` — dynamic `0x0500 | low_byte` builder at `0x08006120`
    - `selector_writer_audit_2026_04_08.md` — active writer audit for `DAT_20001025 / DAT_2000102E`
    - `scope_selector_bypass_2026_04_08.md` — why the visible scope FSM may bypass the selector family
    - `enclosing_helper_cluster_2026_04_08.md` — adjacent helper cluster at `0x08006060 / 0x08006120 / 0x080062F8 / 0x08006418`
    - `mode_selector_writer_map_2026_04_08.md` — concrete writer map for `DAT_20001060` and its restore / transient states
    - `scope_low_byte_2_path_2026_04_08.md` — why low byte `2` is the strongest current scope-active state
    - `scope_state_commit_bridge_2026_04_08.md` — exact-byte xrefs plus the corrected `0x08015848` redraw-ladder interpretation
    - `scope_cluster_control_bytes_2026_04_08.md` — `+0xE10 / +0xE11 / +0xE1B` overlay/list control bytes around the right-panel ladder
    - `packed_scope_state_writers_2026_04_08.md` — concrete runtime writers for `0xF69..0xF6B` in the downloaded app
    - `ripcord_feedback_bridge_2026_04_08.md` — cross-check of the `ripcord` notes against hardware-confirmed pins and the newer queue-split / packed-state model, with adopt-versus-caution guidance
    - `scope_experiment_priorities_after_ripcord_2026_04_08.md` — turns the ripcord cross-check into concrete bench priorities: instrument packed state, stage runtime posture first, then test grouped selector families
    - `mcu_fpga_gap_decisive_bench_plan_2026_04_08.md` — bounded four-experiment bench plan to separate selector-vs-wire errors, missing runtime choreography, and missing stock image/state using the current CDC shell and diagnostics
    - `payload_anchor_sweep_results_2026_04_08.md` — live Experiment 3 results showing no payload-sensitive change on `0x02A0`, `0x0501`, or `0x0503`, which triggers the current command-side hard stop and shifts priority toward missing stock image/state
    - `w25q128_stock_boot_sniff_plan_2026_04_08.md` — board-specific logic-analyzer plan for sniffing the SPI2 flash bus during a stock-app boot attempt, with hookup, safe flash/restore loop, and interpretation criteria
    - `unicorn_stock_flash_trace_first_pass_2026_04_08.md` — software-only fallback to the W25Q128 boot sniff: first Unicorn pass now reaches the stock `0x90` flash-ID transaction against the real dump, but still stalls before real `0x03` block reads
    - `unicorn_stock_flash_trace_second_pass_2026_04_08.md` — second Unicorn pass pushes `master_init()` through LCD, ADC, SPI3, and passive-key waits, shows that boot still only reaches the SPI2 `0x90` ID probe in this harness, and proves direct `fatfs_init()` currently fails on missing allocator bootstrap state at `0x20001070`
    - `unicorn_stock_flash_trace_third_pass_2026_04_08.md` — ablation pass on the synthetic high-flash descriptor table: roots and directory roots are the real gates for direct `fatfs_init()`, while both `2:/LOGO` and `0x080BC841` only make small traversal differences once those root descriptors are present
    - `volume1_root_directory_rewrite_path_2026_04_08.md` — traces the heavy SPI2 write family back to the boot-time `2:/` root population sequence, shows it is a deterministic rewrite of the Volume 1 FAT root directory at `0x207000..0x207FFF`, and explains why it is not evidence of external-flash corruption
    - `fatfs_boot_followon_non_gate_2026_04_08.md` — shows that `fatfs_init()` is unconditionally called and not checked at its call site, narrows `DAT_20001066` to USB MSC bank selection, and promotes the later `FUN_08034878(0x080BC18B)` right-panel enumerator as the first strong follow-on consumer of the same descriptor family
    - `overlay_seed_first_ui_consumer_2026_04_08.md` — identifies the right-panel redraw ladder as the first clearly LCD-visible consumer of the overlay state seeded by `FUN_08034878(0x080BC18B)`, which suggests the remaining unknowns are increasingly descriptor-data shaped rather than hidden-branch shaped
    - `panel_overlay_state_writer_gap_2026_04_08.md` — historical boundary note for the earlier `+0xE10` writer hunt; now superseded in part by the later recovered direct `=1` writer
    - `panel_overlay_state_writer_recovered_2026_04_09.md` — raw-store sweep correction proving the downloaded app does contain a direct `+0xE10 = 1` writer at project `0x08006810`, tied to the `+0xF68 == 2` handoff helper just ahead of the broader `0x08006840` normalizer cluster
    - `overlay_handoff_normalizer_cluster_2026_04_09.md` — unifies the adjacent mixed `0x02A0 -> ... -> 0x0503` gate, the recovered `+0xE10 = 1` writer, and the broader `0x08006840` reset/entry normalizer into one tighter transition cluster
    - `mixed_sub2_4_to_overlay_bridge_2026_04_09.md` — shows that the mixed `sub2 = 4` runtime family, the `0x02A0 -> 0x0503` gate, the recovered `+0xE10 = 1` writer, and the `0x08006840` normalizer all live in one contiguous raw owner block
    - `mixed_cluster_parent_boundary_2026_04_09.md` — boundary check showing that the next raw prologue at `0x08006954` is a different heavy hardware/acquisition owner rather than a linear parent of the compact mixed cluster, which shifts the next search toward indirect chooser/event routing
    - `bc859_descriptor_reconstruction_2026_04_08.md` — chained Unicorn result showing that reconstructing `0x080BC859` as `2:/Screenshot simple file/%d.bin` is enough to unlock the later metadata consumer on top of mounted `fatfs_init()` state, while the same path stays flat without that descriptor family
    - `bc859_metadata_lifecycle_2026_04_09.md` — persistent-write follow-up proving the `%d.bin` family is the real runtime gate: without it `FUN_08035ED4()` returns `2`, with it the function returns `0`, the mutated FAT image now contains a concrete zero-filled `Screenshot simple file/1.BIN` artifact of size `0x25A + 5`, and the neighboring BMP-side `0x080BCAD2 / 0x080BCAE5` strings remain insufficient by themselves
    - `bcad2_preview_path_probe_2026_04_09.md` — follow-up on the BMP-side preview family showing that `FUN_08036084()` needs a concrete preformatted BMP path, not just the descriptor strings, and that with `2:/Screenshot file/1.bmp` seeded it gets far enough to create a zero-size `Screenshot file/1.BMP` entry before the preview/render side becomes the remaining blocker
    - `bcad2_preview_completion_2026_04_09.md` — completed BMP-side emulator closure: with a stage-scoped preview stub and loop fast-forwarding, the `FUN_08036084()` path now runs end-to-end and produces a valid `Screenshot file/1.BMP` artifact (`cluster=13`, `size=153654`, `BM` header), which shows the remaining BMP-side gap was emulator cost rather than another missing descriptor family
    - `bcad2_bc859_coupled_lifecycle_2026_04_09.md` — chained `fatfs_init() -> FUN_08036084() -> FUN_08035ED4()` run proving the BMP screenshot path and the `%d.bin` metadata path coexist in one emulated stock session, producing both `Screenshot file/1.BMP` and `Screenshot simple file/1.BIN` on the same mutated flash image
    - `overlay_artifact_owner_family_2026_04_09.md` — unifies the solved BMP and `%d.bin` helpers into one right-panel overlay/runtime family: `FUN_08034878()` as the shared enumerator/repair bridge, `FUN_0802EA08()` as the screenshot-directory validator, and the runtime split between single-entry commit, single-slot rebuild, and bitmap/multi-slot rebuild
    - `right_panel_shim_case_map_2026_04_08.md` — decodes the compact owner at `0x08006548` and shows `state 6 -> state 5` entry, `state 5 -> state 2` reset, and `state 2 -> 9/1/2/0` preset promotion
    - `state6_entry_gate_2026_04_08.md` — narrows visible `state 6` to an `E1B != 0` gate inside the broad `state 5` controller, making it look like a transient armed-panel handoff rather than an independent submenu
    - `right_panel_resource_owner_map_2026_04_08.md` — separates boot-time descriptor enumeration, runtime overlay list rebuild, and the later `preview build -> metadata mount` path around `FUN_08034878()`, `FUN_08036084()`, and `FUN_08035ED4()`
    - `raw_app_base_offset_2026_04_08.md` — explains the `+0x4000` address shift needed when comparing decompiled/Ghidra notes against the archived V1.2.0 app-slot binary, and uses that correction to reconcile the key-loop and right-panel redraw cluster with raw objdump
    - `display_mode_latch_map_2026_04_08.md` — shows that `DAT_2000012C` is latched earlier in the broad scope controller family and only consumed later by the right-panel redraw/resource owner
    - `display_mode_posture_cluster_2026_04_08.md` — expands the latch pair into a larger trigger-side posture cluster keyed by `F6B`, `+0x23A`, `+0x35`, `+0x3C`, and `+0x40`, with concrete branches for active-channel, trigger-edge, and run-mode control
    - `trigger_posture_cluster_owner_map_2026_04_08.md` — identifies the owning function for the later trigger/posture cluster as the broad runtime controller at raw `0x08008D60` / decompiled `0x08004D60`

### Reference Data
- `strings_with_addresses.txt` — 290 firmware strings mapped to addresses
- `vector_table.txt` — ARM interrupt vector table
- `dispatch_table.txt` — FPGA command dispatch table
- `gpio_access_map.txt` — All GPIO register accesses

### Ghidra Scripts
- `ghidra_scripts/` — Java scripts for automated firmware analysis
