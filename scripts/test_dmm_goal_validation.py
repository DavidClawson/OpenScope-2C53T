#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


MODULE_PATH = Path(__file__).resolve().parent / "validate_dmm_goal.py"
SPEC = importlib.util.spec_from_file_location("validate_dmm_goal", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validate_dmm_goal = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validate_dmm_goal)

STOCK_LITERALS_PATH = Path(__file__).resolve().parent / "test_stock_meter_literals.py"
STOCK_SPEC = importlib.util.spec_from_file_location(
    "test_stock_meter_literals", STOCK_LITERALS_PATH
)
assert STOCK_SPEC is not None and STOCK_SPEC.loader is not None
stock_meter_literals = importlib.util.module_from_spec(STOCK_SPEC)
STOCK_SPEC.loader.exec_module(stock_meter_literals)

STOCK_H2_PATH = Path(__file__).resolve().parent / "test_stock_h2_table.py"
STOCK_H2_SPEC = importlib.util.spec_from_file_location(
    "test_stock_h2_table", STOCK_H2_PATH
)
assert STOCK_H2_SPEC is not None and STOCK_H2_SPEC.loader is not None
stock_h2_table = importlib.util.module_from_spec(STOCK_H2_SPEC)
STOCK_H2_SPEC.loader.exec_module(stock_h2_table)


GOOD_DCV = """=== DMM State ===
mode=1 startup=Meter meter_submode=0 (DC Voltage) layout=0 (full)
valid=1 reading_submode=0 class=1 updates=57460 display=0.4365 unit=V
bcd_value=4365 decimal_pos=1 negative=0 unit_variant=0 bar_i100=2 aux_freq_i10=0
frame_family expected=0 observed=0 reject=0
frame=5A A5 44 8E EF C7 07 24 80 00 01 8A
"""


GOOD_ACV_REJECT = """=== DMM State ===
mode=1 startup=Meter meter_submode=1 (AC Voltage) layout=0 (full)
valid=0 reading_submode=1 class=0 updates=57461 display=--- unit=
bcd_value=0 decimal_pos=0 negative=0 unit_variant=0 bar_i100=0 aux_freq_i10=0
frame_family expected=0 observed=0 reject=3
frame=5A A5 44 8E EF C7 07 24 80 00 01 8A
"""


class DmmGoalValidationTests(unittest.TestCase):
    def test_parse_meter_dump_extracts_core_fields(self) -> None:
        parsed = validate_dmm_goal.parse_meter_dump_block(GOOD_DCV)

        self.assertEqual(parsed["meter_submode"], 0)
        self.assertEqual(parsed["reading_submode"], 0)
        self.assertEqual(parsed["valid"], 1)
        self.assertEqual(parsed["display"], "0.4365")
        self.assertEqual(parsed["unit"], "V")
        self.assertEqual(parsed["display_value"], 0.4365)
        self.assertEqual(parsed["reject"], 0)

    def test_dcv_visual_observation_tolerance_passes(self) -> None:
        parsed = validate_dmm_goal.parse_meter_dump_block(GOOD_DCV)
        validate_dmm_goal.assert_dcv_matches_observed(parsed, 0.435, 0.01)

    def test_dcv_visual_observation_mismatch_fails(self) -> None:
        parsed = validate_dmm_goal.parse_meter_dump_block(GOOD_DCV)
        with self.assertRaisesRegex(validate_dmm_goal.GateError, "DCV mismatch"):
            validate_dmm_goal.assert_dcv_matches_observed(parsed, 0.200, 0.05)

    def test_acv_same_dc_input_must_reject_missing_ac_evidence(self) -> None:
        parsed = validate_dmm_goal.parse_meter_dump_block(GOOD_ACV_REJECT)
        validate_dmm_goal.assert_acv_rejects_dc(parsed)

    def test_acv_confident_voltage_fails(self) -> None:
        parsed = validate_dmm_goal.parse_meter_dump_block(GOOD_DCV)
        parsed["meter_submode"] = 1
        parsed["reading_submode"] = 1
        with self.assertRaisesRegex(validate_dmm_goal.GateError, "ACV did not reject"):
            validate_dmm_goal.assert_acv_rejects_dc(parsed)

    def test_acv_selector_prefers_missing_evidence_over_later_blank(self) -> None:
        blank = GOOD_ACV_REJECT.replace("reject=3", "reject=0")
        selected = validate_dmm_goal.select_acv_reject_dump(GOOD_ACV_REJECT + blank)
        self.assertEqual(selected["reject"], 3)

    def test_re_coverage_requires_h2_acceptance_boundary(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn(
            "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md",
            coverage["docs"],
        )
        self.assertIn(
            "reverse_engineering/analysis_v120/h2_extracted/FINDINGS.md",
            coverage["docs"],
        )
        self.assertIn("acceptance proof", coverage["terms"])
        self.assertIn("unproven", coverage["terms"])
        self.assertIn("H2 table binary guard", coverage["terms"])
        self.assertIn("no ACK/apply proof", coverage["terms"])
        self.assertIn("watchdog reload state boundary guard", coverage["terms"])
        self.assertIn("DAT_2000105a", coverage["terms"])
        self.assertIn("not DMM ms[0x02]/ms[0x03]", coverage["terms"])

    def test_legacy_meter_fsm_note_does_not_overclaim_unit_strings(self) -> None:
        result = validate_dmm_goal.verify_legacy_meter_fsm_unit_lookup_boundary()

        self.assertEqual(
            result["checked"],
            "reverse_engineering/analysis_v120/meter_fsm_deep_dive.md",
        )
        self.assertIn(
            "Unit lookup boundary corrected 2026-06-06",
            result["required"],
        )
        self.assertIn(
            "not a recovered stock unit-string table",
            result["required"],
        )
        self.assertIn(
            "Unit strings live in flash at 0x804c40c",
            result["forbidden"],
        )

    def test_legacy_meter_fsm_note_does_not_overclaim_range_commands_absent(self) -> None:
        result = validate_dmm_goal.verify_legacy_meter_fsm_range_command_boundary()

        self.assertEqual(
            result["checked"],
            "reverse_engineering/analysis_v120/meter_fsm_deep_dive.md",
        )
        self.assertIn(
            "Range-command boundary corrected 2026-06-06",
            result["required"],
        )
        self.assertIn(
            "DMM-owned runtime analog range writer for `ms[0x02]`/`ms[0x03]`",
            result["required"],
        )
        self.assertIn(
            "never sends range commands back",
            result["forbidden"],
        )

    def test_h2_tx_only_boundary_is_guarded_in_firmware_surfaces(self) -> None:
        result = validate_dmm_goal.verify_h2_tx_only_boundary()

        self.assertIn("firmware/src/drivers/fpga.h", result["checked"])
        self.assertIn("firmware/src/drivers/fpga.c", result["checked"])
        self.assertIn("firmware/src/ui/scope_ui.c", result["checked"])
        self.assertIn("firmware/src/drivers/usb_debug.c", result["checked"])
        self.assertIn(
            "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md",
            result["checked"],
        )
        self.assertIn(
            "reverse_engineering/analysis_v120/fpga_h2_spi3_bulk.md",
            result["checked"],
        )
        self.assertIn(
            "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md",
            result["checked"],
        )
        self.assertIn(
            "TX complete: %s (no recovered FPGA ACK)",
            result["checked"]["firmware/src/drivers/usb_debug.c"],
        )
        self.assertIn(
            "H2 means bytes streamed, not recovered FPGA acceptance",
            result["checked"]["firmware/src/ui/scope_ui.c"],
        )
        self.assertIn(
            "Our custom firmware skips it entirely",
            result["forbidden"][
                "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md"
            ],
        )
        self.assertIn(
            "That's the gap",
            result["forbidden"][
                "reverse_engineering/analysis_v120/fpga_h2_spi3_bulk.md"
            ],
        )
        self.assertIn(
            "15 00 00 3B",
            result["checked"][
                "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md"
            ],
        )
        self.assertIn(
            "The dummy exchanges are the most likely fix",
            result["forbidden"][
                "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md"
            ],
        )

    def test_stock_h2_table_is_binary_grounded(self) -> None:
        result = stock_h2_table.verify_h2_table()
        self.assertEqual(result["file_offset"], "0x51d19")
        self.assertEqual(result["flash_addr"], "0x08051d19")
        self.assertEqual(result["stats"]["total_bytes"], 115638)
        self.assertEqual(result["stats"]["records"], 38546)
        self.assertEqual(result["stats"]["sentinel_blocks"], 546)
        self.assertEqual(result["tail_range"], (0x1C340, 0x1C3B5, 118))
        self.assertEqual(
            result["sentinel_runs"],
            [
                (0, 543, True, 0x00000, 0x153FF, 87040),
                (544, 567, False, 0x15400, 0x162FF, 3840),
                (568, 569, True, 0x16300, 0x1643F, 320),
                (570, 721, False, 0x16440, 0x1C33F, 24320),
            ],
        )

    def test_re_coverage_requires_dac1_scope_boundary(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn(
            "reverse_engineering/analysis_v120/meter_dac1_scope_boundary_2026_06_06.md",
            coverage["docs"],
        )
        self.assertIn("DAC1", coverage["terms"])
        self.assertIn("scope trigger", coverage["terms"])
        self.assertIn("not DMM calibration", coverage["terms"])

    def test_re_coverage_requires_w25q_system_file_boundary(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn(
            "reverse_engineering/analysis_v120/meter_w25q_calibration_boundary_2026_06_06.md",
            coverage["docs"],
        )
        self.assertIn("9999.BIN", coverage["terms"])
        self.assertIn("cluster 0", coverage["terms"])
        self.assertIn("size 0", coverage["terms"])
        self.assertIn("not a recovered meter calibration source", coverage["terms"])

    def test_re_coverage_requires_shared_local_split_boundary(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn("shared local splits", coverage["terms"])
        self.assertIn("eight-entry stock selector table", coverage["terms"])
        self.assertIn("without binary stock evidence", coverage["terms"])
        self.assertIn("uA is unresolved and unexposed", coverage["terms"])
        self.assertIn("UI/submode surface guard", coverage["terms"])
        self.assertIn("no recovered uA local submode", coverage["terms"])
        self.assertIn("logical DMM function matrix", coverage["terms"])
        self.assertIn("13-function matrix", coverage["terms"])

    def test_re_coverage_requires_magnitude_feedback_boundary(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn("Magnitude-Derived Range Feedback Boundary", coverage["terms"])
        self.assertIn("BCD overflow/underflow", coverage["terms"])
        self.assertIn("not a value-shape classifier", coverage["terms"])
        self.assertIn("magnitude-derived relay/range control", coverage["terms"])

    def test_ui_submode_surface_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_ui_submode_surface_contract()
        self.assertIn("firmware/src/ui/meter_ui.c", result["checked"])
        self.assertIn("firmware/src/drivers/meter_auto.c", result["checked"])
        self.assertIn(
            "uA is intentionally absent from this UI/submode",
            result["required"]["firmware/src/ui/meter_ui.c"],
        )
        self.assertIn(
            "#define FPGA_METER_LOGICAL_FUNCTION_COUNT 13u",
            result["required"]["firmware/src/drivers/fpga_meter_plan.h"],
        )
        self.assertIn(
            "uA",
            result["forbidden"]["firmware/src/drivers/meter_auto.c"],
        )

    def test_unrecovered_meter_coefficients_are_absent(self) -> None:
        result = validate_dmm_goal.verify_no_unrecovered_meter_coefficients()
        self.assertIn("firmware/src/drivers/meter_data.c", result["checked"])
        self.assertIn("firmware/src/drivers/flash_fs.c", result["checked"])
        self.assertIn("METER_CAL_LOW_OHM_FACTOR", result["forbidden"])
        self.assertIn("3:/System file/cal_ch1.bin", result["forbidden"])
        self.assertIn(
            "DCA formatter variant branch at 0x08002AFE/0x08002B54",
            result["required_meter_data"],
        )
        self.assertIn(
            "DAT_2000102e == 2 selects unit index 3",
            result["required_meter_data"],
        )

    def test_magnitude_range_feedback_is_absent(self) -> None:
        result = validate_dmm_goal.verify_no_magnitude_range_feedback()
        self.assertIn("firmware/src/drivers/fpga.c", result["checked"])
        self.assertIn(
            "Range feedback is intentionally not driven from the parsed number",
            result["required"],
        )
        self.assertIn("Detect BCD overflow", result["forbidden"])
        self.assertIn("send higher range params", result["forbidden"])

    def test_meter_aux_afe_policy_uses_mux_state_model(self) -> None:
        result = validate_dmm_goal.verify_meter_aux_afe_pin_policy()
        self.assertEqual(result["checked"], "firmware/src/drivers/fpga.c")
        self.assertIn(
            "fpga_apply_meter_mux_gpio_state(&mux_state);",
            result["required_body"],
        )
        self.assertIn(
            "fpga_gpio_write_level(GPIOB, (1U << 9), state->pb9);",
            result["required_file"],
        )
        self.assertIn(
            "saved-config default ms[0x02]=5 /\n     * ms[0x03]=5 is persistence evidence only",
            result["required_file"],
        )

    def test_meter_expected_selector_uses_plan_word(self) -> None:
        result = validate_dmm_goal.verify_meter_expected_selector_uses_plan_word()
        self.assertEqual(result["checked"], "firmware/src/drivers/fpga.c")
        self.assertIn(
            "(uint8_t)(plan.selector_word & 0x00FFU)",
            result["required_body"],
        )
        self.assertIn(
            "fpga_meter_stock_cmd_low_for_mode(plan.stock_mode)",
            result["forbidden_body"],
        )

    def test_meter_sequence_tail_uses_transition_plan(self) -> None:
        result = validate_dmm_goal.verify_meter_sequence_tail_uses_transition_plan()
        self.assertEqual(result["checked"], "firmware/src/drivers/fpga.c")
        self.assertIn(
            "fpga.meter_mode_start_word = plan.start_word;",
            result["required_body"],
        )
        self.assertIn(
            "(uint8_t)(plan.start_word & 0x00FFU)",
            result["required_body"],
        )
        self.assertIn(
            "fpga_timed_send_cmd(0x05, FPGA_CMD_METER_START, plan.settle_ms)",
            result["forbidden_body"],
        )

    def test_meter_transition_production_contract_uses_single_path(self) -> None:
        result = validate_dmm_goal.verify_meter_transition_production_contract()
        self.assertEqual(result["checked"], "firmware/src/drivers/fpga.c")
        self.assertIn(
            "fpga_meter_reset_transport();",
            result["required_helper"],
        )
        self.assertIn(
            "fpga_apply_meter_transition(submode, false);",
            result["required_set_mode"],
        )
        self.assertIn(
            "fpga_apply_meter_transition(submode, true);",
            result["required_reinit"],
        )
        self.assertIn(
            "fpga_send_meter_mode_sequence(submode);",
            result["forbidden_public"],
        )
        self.assertEqual(
            result["order"],
            [
                "meter_data_invalidate(submode);",
                "if (!fpga_meter_submode_is_valid(submode))",
                "fpga_meter_reset_transport();",
                "fpga_set_meter_frontend_for_submode(submode);",
                "fpga_scope_delay_ms(plan.settle_ms);",
                "fpga_send_meter_mode_sequence(submode);",
                "fpga_meter_discard_next_frames(plan.discard_frames);",
            ],
        )

    def test_meter_apply_words_are_stock_bound_at_use_site(self) -> None:
        result = validate_dmm_goal.verify_meter_apply_pair_production_comment()

        self.assertEqual(result["checked"], "firmware/src/drivers/fpga_meter_plan.c")
        self.assertIn("0x08006120 gates and masks", result["required_body"])
        self.assertIn(
            "0x08006194 / 0x0800626A choose low-byte pairs",
            result["required_body"],
        )
        self.assertIn("ACV 0x0C/0x0D", result["required_body"])
        self.assertIn("DCA 0x17/0x0E", result["required_body"])
        self.assertIn("continuity 0x11/0x16", result["required_body"])
        self.assertIn("diode 0x10/0x15", result["required_body"])
        self.assertIn(
            "must not be filled from the parsed numeric value",
            result["required_body"],
        )
        self.assertIn("display_value", result["forbidden_body"])

    def test_no_ocr_pipeline_guard_is_active(self) -> None:
        result = validate_dmm_goal.verify_no_ocr_pipeline()

        self.assertIn("scripts/validate_dmm_goal.py", result["checked"])
        self.assertIn("scripts/openscope_live_debug.py", result["checked"])
        self.assertIn("py" + "tess" + "eract", result["forbidden"])
        self.assertIn("image_" + "to_string", result["forbidden"])

    def test_ac_status_boundary_guard_is_active(self) -> None:
        result = validate_dmm_goal.verify_ac_status_boundary()

        self.assertIn(
            "reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md",
            result["checked"],
        )
        self.assertIn("rx[7] bit 2 = " + "AC flag", result["forbidden"])
        self.assertIn("AC/DC " + "flag for DCV", result["forbidden"])
        self.assertIn("not recovered as AC-present confidence", result["required"])

    def test_re_coverage_requires_factory_cal_fail_closed_placeholder(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn("flash_fs_load_factory_cal", coverage["terms"])
        self.assertIn("fail-closed placeholder", coverage["terms"])
        self.assertIn("cal_ch1.bin", coverage["terms"])
        self.assertIn("roll-buffer state", coverage["terms"])
        self.assertIn("roll-buffer preload guard", coverage["terms"])

    def test_re_coverage_requires_dmm_evidence_gap_ledger(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn(
            "reverse_engineering/analysis_v120/dmm_evidence_gap_ledger_2026_06_06.md",
            coverage["docs"],
        )
        self.assertIn("Current Low-DCV Blocker", coverage["terms"])
        self.assertIn("stock-visible decode: digits=4366", coverage["terms"])
        self.assertIn("Do not promote", coverage["terms"])
        self.assertIn("this visual mismatch into a decoder coefficient", coverage["terms"])
        self.assertIn("Mux-state xref closure", coverage["terms"])
        self.assertIn("classified as negative DMM evidence", coverage["terms"])
        self.assertIn(
            "not progress unless a new writer, xref owner, or trace is recovered",
            coverage["terms"],
        )
        self.assertIn("Next RE Target", coverage["terms"])

    def test_stock_roll_buffer_preload_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_roll_buffer_preload_sequences()
        self.assertEqual(
            result["sequences"]["roll_buffer_transform_entry"]["addr"],
            "0x08001830",
        )
        self.assertEqual(
            result["sequences"]["master_init_roll_buffer_callers"]["addr"],
            "0x080271a8",
        )

    def test_stock_display_formatter_dispatch_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_display_formatter_dispatch_sequences()
        self.assertEqual(
            result["sequences"]["display_formatter_mode_switch_cases"]["addr"],
            "0x08002aa0",
        )
        self.assertEqual(
            result["sequences"]["display_formatter_mode5_extended"]["addr"],
            "0x08002b20",
        )
        self.assertEqual(
            result["sequences"]["display_formatter_modes6_7_unit_offsets"]["addr"],
            "0x08002b34",
        )

    def test_stock_current_formatter_variant_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_current_formatter_variant_sequences()
        self.assertEqual(
            result["sequences"]["display_formatter_dca_variant_units"]["addr"],
            "0x08002afe",
        )
        self.assertEqual(
            result["sequences"]["display_formatter_dca_variant_one_target"]["addr"],
            "0x08002b54",
        )

    def test_stock_unit_lookup_boundary_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_unit_lookup_boundary_sequences()
        self.assertEqual(
            result["sequences"]["display_unit_lookup_zero_region"]["addr"],
            "0x0804c40c",
        )
        self.assertEqual(
            result["sequences"]["display_unit_lookup_draw_call"]["addr"],
            "0x08009ae4",
        )
        self.assertEqual(result["thumb_pointers"], [])

    def test_state_machine_property_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_state_machine_property_contract()
        self.assertEqual(result["file"], "firmware/tests/test_meter_data.c")
        self.assertIn(
            "state_machine_property_matrix_covers_all_submodes",
            result["tests"],
        )
        self.assertIn(
            "invalidate_clears_stale_payload_for_every_ordered_mode_transition",
            result["tests"],
        )
        self.assertIn(
            "marker_visible_family_mismatch_matrix_clears_stale_payload",
            result["tests"],
        )
        self.assertIn(
            "unclassified_normal_frames_follow_active_family_only",
            result["tests"],
        )
        self.assertIn(
            "frame6_0x40_is_not_a_global_resistance_family_marker",
            result["tests"],
        )
        self.assertIn("range class bits cover all 16 combinations",
                      result["regex_anchors"])
        self.assertIn("all non-voltage modes reject voltage frames",
                      result["regex_anchors"])
        self.assertIn(
            "local AC A display keeps stock ACA unit index",
            result["regex_anchors"],
        )
        self.assertIn(
            "unclassified normal frames are active-plan classified",
            result["regex_anchors"],
        )
        self.assertIn("METER_REJECT_MISSING_AC_EVIDENCE",
                      result["snippet_anchors"])
        self.assertIn("not a standalone cross-mode family marker",
                      result["snippet_anchors"])

    def test_transition_plan_property_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_transition_plan_property_contract()
        self.assertEqual(result["file"], "firmware/tests/test_fpga_meter_plan.c")
        self.assertIn(
            "every_submode_transition_drains_before_accepting_frames",
            result["tests"],
        )
        self.assertIn(
            "transition_settle_discard_policy_is_explicit_for_every_submode",
            result["tests"],
        )
        self.assertIn(
            "rx_frame_gate_preserves_discard_budget_while_busy",
            result["tests"],
        )
        self.assertIn(
            "frame_family_mismatch_policy_matrix_is_exhaustive",
            result["tests"],
        )
        self.assertIn(
            "frame_family_marker_visibility_documents_observed_gaps",
            result["tests"],
        )
        self.assertIn(
            "logical_function_capability_matrix_covers_all_dmm_modes",
            result["tests"],
        )
        self.assertIn("phase matrix iterates every local submode",
                      result["regex_anchors"])
        self.assertIn("logical DMM function table covers microamp gaps",
                      result["regex_anchors"])
        self.assertIn("frame-family policy iterates observed families",
                      result["regex_anchors"])
        self.assertIn(
            "marker-visible families stay limited to voltage and continuity",
            result["regex_anchors"],
        )
        self.assertIn(
            "active-plan-only families stay marker-unproven",
            result["regex_anchors"],
        )
        self.assertIn("planned discards drain in order",
                      result["regex_anchors"])
        self.assertIn(
            "local selector words preserve recovered stock command table",
            result["regex_anchors"],
        )
        self.assertIn(
            "shared local split pairs remain explicit",
            result["regex_anchors"],
        )
        self.assertIn("fpga_meter_frame_family_is_acceptable",
                      result["snippet_anchors"])
        self.assertIn("fpga_meter_frame_family_has_stock_marker",
                      result["snippet_anchors"])
        self.assertIn("bad submode word", result["snippet_anchors"])
        self.assertIn("FPGA_METER_START_WORD", result["snippet_anchors"])
        self.assertIn("bad plan has no start", result["snippet_anchors"])
        self.assertIn("DC uA is unresolved", result["snippet_anchors"])
        self.assertIn("AC uA is unresolved", result["snippet_anchors"])
        self.assertIn("uniform local settle/discard",
                      result["snippet_anchors"])
        self.assertIn("invalid submodes emit no settle/discard",
                      result["snippet_anchors"])
        self.assertIn("stable frame accepted", result["snippet_anchors"])

    def test_autoscan_property_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_autoscan_property_contract()
        self.assertEqual(result["file"], "firmware/tests/test_meter_auto.c")
        self.assertIn("ac_voltage_requires_frequency_evidence", result["tests"])
        self.assertIn("current_auto_scores_respect_ac_evidence", result["tests"])
        self.assertIn(
            "unresolved_microamp_functions_are_not_autoscan_candidates",
            result["tests"],
        )
        self.assertIn(
            "dc_voltage_scores_without_frequency_or_nonzero_magnitude",
            result["tests"],
        )
        self.assertIn("meter_auto_score(1, &r) == 0", result["snippet_anchors"])
        self.assertIn(
            "r.is_ac = true;\n    ASSERT(meter_auto_score(1, &r) == 0);",
            result["snippet_anchors"],
        )
        self.assertIn("FPGA_METER_FUNCTION_DC_UA", result["snippet_anchors"])
        self.assertIn("meter_auto_score(4, &r) == 0", result["snippet_anchors"])
        self.assertIn("r.bcd_value = 0", result["snippet_anchors"])

    def test_live_validation_only_switches_dcv_acv_without_current_probe(self) -> None:
        calls: list[list[str]] = []
        meter_dumps = [GOOD_DCV, GOOD_ACV_REJECT]
        old_live_debug = validate_dmm_goal.live_debug
        old_capture_webcam = validate_dmm_goal.capture_webcam
        old_sleep = validate_dmm_goal.time.sleep

        def fake_capture_webcam(device: str, output: Path, size: str) -> dict[str, object]:
            return {"device": device, "path": str(output), "bytes": 1, "size": size}

        def fake_live_debug(args: list[str]) -> str:
            calls.append(args)
            if args and args[0] == "meter-dump":
                return meter_dumps.pop(0)
            return "ok"

        try:
            validate_dmm_goal.live_debug = fake_live_debug
            validate_dmm_goal.capture_webcam = fake_capture_webcam
            validate_dmm_goal.time.sleep = lambda seconds: None
            args = validate_dmm_goal.argparse.Namespace(
                observed_source_voltage=0.4365,
                voltage_tolerance=0.01,
                webcam="/dev/video-test",
                webcam_size="1x1",
                timeout=0.1,
                port=None,
                settle_seconds=0.0,
            )

            result = validate_dmm_goal.run_live_validation(
                args, Path("tmp/test_live_validation_only_dcv_acv")
            )
        finally:
            validate_dmm_goal.live_debug = old_live_debug
            validate_dmm_goal.capture_webcam = old_capture_webcam
            validate_dmm_goal.time.sleep = old_sleep

        mode_commands = [call[1] for call in calls if call and call[0] == "command"]
        self.assertEqual(
            mode_commands,
            ["mode meter 0 0", "mode meter 1 0", "mode meter 0 0"],
        )
        self.assertTrue(result["passed"])
        self.assertIn("not probed", result["current_live"])
        self.assertIn("not probed", result["passive_live"])

    def test_live_validation_safety_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_live_validation_safety_contract()
        self.assertEqual(result["checked"], "scripts/validate_dmm_goal.py")
        self.assertEqual(
            result["mode_commands"],
            ["mode meter 0 0", "mode meter 1 0", "mode meter 0 0"],
        )
        self.assertIn(
            '"current_live": "not probed without correct jack and load-limited series wiring; parser tests cover voltage rejection"',
            result["required"],
        )
        self.assertIn("current/passive", result["forbidden"])

    def test_stock_meter_selector_table_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_selector_table()
        self.assertEqual(result["runtime_addr"], 0x080BB3FC)
        self.assertEqual(result["app_image_addr"], 0x080B43FC)
        self.assertEqual(result["bytes"], "14 0c 17 0b 0a 12 11 10")
        self.assertEqual(
            result["words"],
            [
                "0x0514", "0x050C", "0x0517", "0x050B",
                "0x050A", "0x0512", "0x0511", "0x0510",
            ],
        )

    def test_stock_meter_selector_consumers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_selector_xref_sequences()
        self.assertIn("0x080042e2", result["sequences"])
        self.assertIn("0x080048ba", result["sequences"])
        self.assertIn("4b f2 fc 32", result["sequences"]["0x080042e2"])
        self.assertIn("c0 f6 0b 02", result["sequences"]["0x080048ba"])
        self.assertIn("00 f5 a0 60", result["sequences"]["0x080048ba"])

    def test_stock_meter_selector_adjusters_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_selector_adjust_sequences()
        sequences = result["sequences"]
        self.assertEqual(
            sequences["selector_adjust_prev_prologue"]["addr"],
            "0x080041f8",
        )
        self.assertEqual(
            sequences["selector_adjust_prev_meter_case"]["addr"],
            "0x080042d4",
        )
        self.assertEqual(
            sequences["selector_adjust_next_prologue"]["addr"],
            "0x080047cc",
        )
        self.assertEqual(
            sequences["selector_adjust_next_meter_case"]["addr"],
            "0x080048ac",
        )
        self.assertIn(
            "95 f8 5d 0f 00 f0 f0 00 b0 28",
            sequences["selector_adjust_prev_meter_case"]["bytes"],
        )
        self.assertIn(
            "41 1e 00 28 08 bf 07 21",
            sequences["selector_adjust_prev_meter_case"]["bytes"],
        )
        self.assertIn(
            "07 28 38 bf 41 1c",
            sequences["selector_adjust_next_meter_case"]["bytes"],
        )
        self.assertIn(
            "1d 21 38 68 21 70",
            sequences["selector_adjust_next_meter_case"]["bytes"],
        )
        self.assertIn(
            "1b 21 38 68 21 70",
            sequences["selector_adjust_next_meter_case"]["bytes"],
        )

    def test_stock_dynamic_raw_word_helpers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_dynamic_raw_word_helper_sequences()
        sequences = result["sequences"]
        apply_pairs = result["apply_lowbyte_pairs"]
        self.assertEqual(
            sequences["selector_seed_state_pairs"]["addr"],
            "0x08006060",
        )
        self.assertEqual(
            sequences["selector_seed_emit_0501"]["addr"],
            "0x080060ca",
        )
        self.assertEqual(
            sequences["dynamic_raw_word_gate_and_mask"]["addr"],
            "0x08006120",
        )
        self.assertEqual(
            sequences["dynamic_raw_word_emit_tail"]["addr"],
            "0x08006288",
        )
        self.assertEqual(
            sequences["dynamic_helper_reverse_partner_gate"]["addr"],
            "0x080062f8",
        )
        self.assertIn(
            "40 f2 01 52 00 68 0a 80",
            sequences["selector_seed_emit_0501"]["bytes"],
        )
        self.assertIn(
            "11 f0 c6 0f",
            sequences["dynamic_raw_word_gate_and_mask"]["bytes"],
        )
        self.assertIn(
            "0c 20 01 29 08 bf 0d 20",
            sequences["dynamic_raw_word_lowbyte_pair_0c_0d"]["bytes"],
        )
        self.assertIn(
            "0e 20 01 29 08 bf 17 20",
            sequences["dynamic_raw_word_lowbyte_pairs_0e17_1116_1015"]["bytes"],
        )
        self.assertIn(
            "40 f4 a0 61",
            sequences["dynamic_raw_word_emit_tail"]["bytes"],
        )
        self.assertIn(
            "1b 22 00 68 0a 70",
            sequences["dynamic_raw_word_emit_tail"]["bytes"],
        )
        self.assertEqual(
            apply_pairs,
            [
                {"selector_low": "0x0C", "apply_low": "0x0D", "stock_mode": "ACV"},
                {"selector_low": "0x17", "apply_low": "0x0E", "stock_mode": "DCA"},
                {
                    "selector_low": "0x11",
                    "apply_low": "0x16",
                    "stock_mode": "continuity",
                },
                {"selector_low": "0x10", "apply_low": "0x15", "stock_mode": "diode"},
            ],
        )
        self.assertEqual(
            {
                (item["selector_low"], item["apply_low"])
                for item in apply_pairs
            },
            {("0x0C", "0x0D"), ("0x17", "0x0E"), ("0x11", "0x16"), ("0x10", "0x15")},
        )

    def test_stock_mux_state_ram_map_boundary_is_classified(self) -> None:
        result = stock_meter_literals.verify_mux_state_ram_map_boundary()
        symbols = result["symbols"]
        self.assertEqual(symbols["DAT_200000fa"]["count"], 25)
        self.assertEqual(symbols["DAT_200000fb"]["count"], 11)
        self.assertEqual(
            symbols["DAT_200000fa"]["refs"],
            [
                "FUN_08034078@08034078",
                "FUN_08001c60@08001c60",
                "FUN_08019e98@08019e98",
                "FUN_0801f6f8@0801f6f8",
                "FUN_0801d2ec@0801d2ec",
                "FUN_0801efc0@0801efc0",
                "unknown@080151c2",
            ],
        )
        self.assertEqual(
            symbols["DAT_200000fb"]["refs"],
            [
                "FUN_08034078@08034078",
                "FUN_08001c60@08001c60",
                "FUN_08019e98@08019e98",
                "FUN_0801f6f8@0801f6f8",
                "FUN_0801d2ec@0801d2ec",
            ],
        )

    def test_stock_mux_state_full_decompile_surface_is_classified(self) -> None:
        result = stock_meter_literals.verify_mux_state_full_decompile_surface()
        symbols = result["symbols"]

        self.assertEqual(symbols["DAT_200000fa"]["count"], 26)
        self.assertEqual(symbols["DAT_200000fb"]["count"], 10)
        self.assertEqual(
            [item["line"] for item in result["pair_writes"]],
            [2566, 8745],
        )
        self.assertEqual(symbols["DAT_200000fa"]["literal_direct_assignments"], [])
        self.assertEqual(symbols["DAT_200000fb"]["literal_direct_assignments"], [])
        self.assertIn("DAT_200000fb", result["pair_writes"][0]["target"])
        self.assertIn("DAT_200000fb", result["pair_writes"][1]["target"])
        self.assertIn(
            "scope/siggen autorange increment",
            result["pair_writes"][0]["classification"],
        )
        self.assertIn(
            "scope_main_fsm autorange increment",
            result["pair_writes"][1]["classification"],
        )
        self.assertIn(
            "FUN_080018a4(DAT_200000fa);",
            [item["text"] for item in symbols["DAT_200000fa"]["refs"]],
        )
        self.assertIn(
            "FUN_08001a58(DAT_200000fb);",
            [item["text"] for item in symbols["DAT_200000fb"]["refs"]],
        )

    def test_stock_mux_state_pair_write_contexts_are_classified(self) -> None:
        result = stock_meter_literals.verify_mux_state_pair_write_contexts()
        contexts = result["contexts"]

        self.assertEqual(
            sorted(contexts),
            [
                "scope_main_fsm_autorange_pair_write_context",
                "siggen_scope_autorange_pair_write_context",
            ],
        )
        for context in contexts.values():
            texts = [item["text"] for item in context]
            self.assertIn("FUN_080018a4(DAT_200000fa);", texts)
            self.assertIn("FUN_08001a58(DAT_200000fb);", texts)
        self.assertIn(
            "local_31 = 4;",
            [item["text"] for item in contexts["siggen_scope_autorange_pair_write_context"]],
        )
        self.assertIn(
            "FUN_0803acf0(_DAT_20002d6c,&local_6b,0xffffffff);",
            [item["text"] for item in contexts["scope_main_fsm_autorange_pair_write_context"]],
        )

    def test_scope_measurement_engine_mux_pointer_alias_is_read_only_consumer(self) -> None:
        result = stock_meter_literals.verify_scope_measurement_engine_mux_pointer_consumer_context()
        context = result["contexts"]["scope_measurement_engine_mux_pointer_consumer_context"]

        self.assertEqual(context["line_range"], [11411, 11491])
        texts = [item["text"] for item in context["required_lines"]]
        self.assertIn("pbVar19 = &DAT_200000fa + uVar22;", texts)
        self.assertIn("bVar3 = *pbVar19;", texts)
        self.assertIn("uVar31 = *pbVar19 / 3;", texts)
        self.assertIn(
            "uVar6 = *(undefined2 *)(&DAT_0804bfb8 + ((uint)*pbVar19 + uVar31 * -3 & 0xff) * 2);",
            texts,
        )
        self.assertIn("scope_measurement_engine", context["classification"])
        self.assertIn("read-only", context["classification"])
        self.assertIn("not a writer", context["classification"])

    def test_stock_dvom_tx_queue_consumer_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_dvom_tx_queue_consumer_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["dvom_tx_raw_word_consumer"]["addr"],
            "0x080373f4",
        )
        self.assertIn(
            "42 f6 74 56",
            sequences["dvom_tx_raw_word_consumer"]["bytes"],
        )
        self.assertIn(
            "03 f0 d6 fe",
            sequences["dvom_tx_raw_word_consumer"]["bytes"],
        )
        self.assertIn(
            "88 f8 00 90 01 0a f8 70 00 eb 10 20 b9 70 78 72",
            sequences["dvom_tx_raw_word_consumer"]["bytes"],
        )
        self.assertIn(
            "28 68 40 f0 80 00 28 60",
            sequences["dvom_tx_raw_word_consumer"]["bytes"],
        )

    def test_stock_meter_transport_transitions_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_transport_transition_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["boot_saved_mode_init_state_restore"]["addr"],
            "0x08026f50",
        )
        self.assertIn(
            "9a f8 64 0f a0 b1 8a f8 68 0f",
            sequences["boot_saved_mode_init_state_restore"]["bytes"],
        )
        self.assertIn(
            "01 28 17 d0 03 28 4c d0 02 28 51 d1",
            sequences["boot_saved_mode_init_state_restore"]["bytes"],
        )
        self.assertIn(
            "9a f8 68 0f 01 21 8a f8 69 1f 01 28 e7 d1",
            sequences["boot_saved_mode_init_state_restore"]["bytes"],
        )

        self.assertEqual(
            sequences["meter_transport_enable_resume_reset"]["addr"],
            "0x08026f8e",
        )
        self.assertIn(
            "08 68 40 f4 00 50 08 60",
            sequences["meter_transport_enable_resume_reset"]["bytes"],
        )
        self.assertIn(
            "42 f6 a0 50 c2 f2 00 00 00 68 13 f0 32 fb",
            sequences["meter_transport_enable_resume_reset"]["bytes"],
        )
        self.assertIn(
            "4f f4 00 60 c4 f2 01 01 08 61",
            sequences["meter_transport_enable_resume_reset"]["bytes"],
        )

        self.assertEqual(
            sequences["meter_transport_disable_suspend_drain"]["addr"],
            "0x0802700a",
        )
        self.assertIn(
            "08 68 20 f4 00 50 08 60",
            sequences["meter_transport_disable_suspend_drain"]["bytes"],
        )
        self.assertIn(
            "42 f6 a0 50 c2 f2 00 00 00 68 13 f0 b2 fb",
            sequences["meter_transport_disable_suspend_drain"]["bytes"],
        )
        self.assertIn(
            "42 f6 74 50 c2 f2 00 00 00 68 00 21 13 f0 ed fd",
            sequences["meter_transport_disable_suspend_drain"]["bytes"],
        )

    def test_stock_meter_selector_state_writers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_selector_state_sequences()
        sequences = result["sequences"]

        self.assertEqual(sequences["init_selector_reset"]["addr"], "0x08026fde")
        self.assertEqual(sequences["rx_force_mode_8"]["addr"], "0x08036d14")
        self.assertEqual(sequences["rx_force_mode_1"]["addr"], "0x08036d50")
        self.assertEqual(sequences["rx_shadow_zero"]["addr"], "0x08037220")
        self.assertEqual(sequences["rx_shadow_extended"]["addr"], "0x080372e0")
        self.assertEqual(sequences["rx_shadow_one"]["addr"], "0x08037328")
        self.assertEqual(sequences["rx_shadow_two"]["addr"], "0x08037338")
        self.assertEqual(sequences["rx_shadow_two_with_frame_bit"]["addr"], "0x080373a8")
        self.assertIn("8a f8 2d 0f", sequences["init_selector_reset"]["bytes"])
        self.assertIn("87 f8 2d 0f", sequences["rx_force_mode_8"]["bytes"])
        self.assertIn("86 f8 2d 0f", sequences["rx_force_mode_1"]["bytes"])
        self.assertIn("87 f8 36 0f", sequences["rx_shadow_zero"]["bytes"])
        self.assertIn("87 f8 36 0f", sequences["rx_shadow_extended"]["bytes"])
        self.assertIn("87 f8 36 2f", sequences["rx_shadow_one"]["bytes"])
        self.assertIn("87 f8 36 0f", sequences["rx_shadow_two"]["bytes"])
        self.assertIn("87 f8 36 2f", sequences["rx_shadow_two_with_frame_bit"]["bytes"])

    def test_stock_meter_mux_restore_sites_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_mux_restore_sequences()
        self.assertEqual(
            result["sequences"]["0x08025544"],
            "a0 78 dc f7 ad f9 e0 78 dc f7 84 fa",
        )
        self.assertEqual(
            result["sequences"]["0x0802723e"],
            "9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc",
        )

    def test_stock_meter_aux_afe_pins_are_config_only(self) -> None:
        result = stock_meter_literals.verify_meter_aux_afe_pin_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["pb9_pa6_output_config_only"]["addr"],
            "0x080241d4",
        )
        self.assertIn(
            "4f f4 00 70 18 90 28 46 21 46",
            sequences["pb9_pa6_output_config_only"]["bytes"],
        )
        self.assertIn(
            "40 20 18 90 40 f6 00 00 c4 f2 01 00",
            sequences["pb9_pa6_output_config_only"]["bytes"],
        )
        self.assertIn(
            "_DAT_40010c10 = 0x200",
            result["forbidden_direct_level_writes"],
        )
        self.assertIn(
            "_DAT_40010814 = 0x40",
            result["forbidden_direct_level_writes"],
        )

    def test_stock_meter_saved_config_unpack_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_saved_config_unpack_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["saved_config_meter_state_unpack"]["addr"],
            "0x08025d92",
        )
        self.assertIn(
            "55 29 c2 f2 00 0a 05 d0 aa 29",
            sequences["saved_config_meter_state_unpack"]["bytes"],
        )
        self.assertIn(
            "8a f8 02 00 60 68 ca f8 03 00",
            sequences["saved_config_meter_state_unpack"]["bytes"],
        )

    def test_stock_meter_saved_config_live_mux_store_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_saved_config_live_mux_store_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["saved_config_live_mux_store"]["addr"],
            "0x08025d94",
        )
        self.assertIn(
            "40 f2 f8 0a",
            sequences["saved_config_live_mux_store"]["bytes"],
        )
        self.assertIn(
            "c2 f2 00 0a",
            sequences["saved_config_live_mux_store"]["bytes"],
        )
        self.assertIn(
            "8a f8 02 00 60 68 ca f8 03 00",
            sequences["saved_config_live_mux_store"]["bytes"],
        )
        self.assertIn(
            "not a runtime DMM range writer",
            sequences["saved_config_live_mux_store"]["classification"],
        )

    def test_stock_meter_saved_config_pack_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_saved_config_pack_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["saved_config_meter_state_pack_reads"]["addr"],
            "0x08022410",
        )
        self.assertEqual(
            sequences["saved_config_meter_state_default_seed"]["addr"],
            "0x080224a0",
        )
        self.assertEqual(
            sequences["saved_config_meter_state_pack_writes"]["addr"],
            "0x0802258a",
        )
        self.assertIn(
            "b9 8b be 78",
            sequences["saved_config_meter_state_pack_reads"]["bytes"],
        )
        self.assertIn(
            "00 21 c0 f2 05 51 4c f6 32 62 c7 e9 00 12",
            sequences["saved_config_meter_state_default_seed"]["bytes"],
        )
        self.assertIn(
            "ca f8 00 60 d7 f8 03 60",
            sequences["saved_config_meter_state_pack_writes"]["bytes"],
        )
        self.assertIn(
            "ca f8 04 60",
            sequences["saved_config_meter_state_pack_writes"]["bytes"],
        )

    def test_stock_meter_saved_config_pack_caller_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_saved_config_pack_caller_sequences()
        sequences = result["sequences"]
        direct_callers = result["direct_callers"]
        classifications = result["classifications"]

        self.assertEqual(
            direct_callers,
            ["0x08002f8c", "0x08002fe2", "0x08005b4a", "0x0803972e"],
        )

        self.assertEqual(
            sequences["housekeeping_threshold_saved_config_pack_caller"]["addr"],
            "0x08002f80",
        )
        self.assertIn(
            "55 20 1f f0 16 fa",
            sequences["housekeeping_threshold_saved_config_pack_caller"]["bytes"],
        )
        self.assertEqual(
            sequences["post_function_literal_pool_bl_shaped_bytes"]["addr"],
            "0x08002f90",
        )
        self.assertIn(
            "55 20 1f f0 eb f9 00 00",
            sequences["post_function_literal_pool_bl_shaped_bytes"]["bytes"],
        )
        self.assertEqual(
            sequences["branch_island_bl_shaped_bytes_before_selector_seed"]["addr"],
            "0x08005b40",
        )
        self.assertIn(
            "00 20 1c f0 37 fc 00 00",
            sequences["branch_island_bl_shaped_bytes_before_selector_seed"]["bytes"],
        )
        self.assertEqual(
            sequences["probe_change_poweroff_saved_config_pack_caller"]["addr"],
            "0x080396f4",
        )
        self.assertIn(
            "b0 f5 61 7f",
            sequences["probe_change_poweroff_saved_config_pack_caller"]["bytes"],
        )
        self.assertIn(
            "b0 f5 e1 6f",
            sequences["probe_change_poweroff_saved_config_pack_caller"]["bytes"],
        )
        self.assertIn(
            "55 20 e8 f7 45 fe",
            sequences["probe_change_poweroff_saved_config_pack_caller"]["bytes"],
        )
        self.assertIn(
            "housekeeping threshold path",
            classifications["0x08002f8c"],
        )
        self.assertIn("literal/data region", classifications["0x08002fe2"])
        self.assertIn("branch island", classifications["0x08005b4a"])
        self.assertIn("controlled shutdown", classifications["0x0803972e"])
        self.assertIn(
            "not normal runtime DMM range switching",
            classifications["0x0803972e"],
        )

    def test_stock_usart_tx_config_writer_meter_case_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_usart_tx_config_writer_meter_case_sequences()
        sequences = result["sequences"]
        callers = result["callers"]
        direct_callers = result["direct_callers"]

        self.assertEqual(
            sequences["writer_tbb_prologue"]["addr"],
            "0x08039734",
        )
        self.assertEqual(
            sequences["meter_case_bitfield_body"]["addr"],
            "0x080397c8",
        )
        self.assertEqual(
            sequences["writer_common_update_mask_commit"]["addr"],
            "0x08039860",
        )
        self.assertIn(
            "df e8 02 f0",
            sequences["writer_tbb_prologue"]["bytes"],
        )
        self.assertIn(
            "23 f4 00 73 43 ea 4c 23",
            sequences["meter_case_bitfield_body"]["bytes"],
        )
        self.assertIn(
            "4f f4 80 74",
            sequences["meter_case_bitfield_body"]["bytes"],
        )
        self.assertIn(
            "10 68 20 43 10 60 10 bd",
            sequences["writer_common_update_mask_commit"]["bytes"],
        )
        self.assertEqual(
            callers["tim5_init_config_writer_call"]["addr"],
            "0x080272cc",
        )
        self.assertEqual(
            callers["tim2_init_config_writer_call"]["addr"],
            "0x08027338",
        )
        self.assertIn("12 f0 2e fa", callers["tim5_init_config_writer_call"]["bytes"])
        self.assertIn("12 f0 f6 f9", callers["tim2_init_config_writer_call"]["bytes"])
        self.assertEqual(direct_callers, ["0x080272d4", "0x08027344"])

    def test_stock_boot_mode_init_dmm_sequences_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_boot_mode_init_dmm_sequences()
        sequences = result["sequences"]
        direct_callers = result["direct_callers"]

        self.assertEqual(
            sequences["mode_init_dispatcher_tbh"]["addr"],
            "0x0800b908",
        )
        self.assertEqual(
            sequences["meter_basic_boot_probe_prefix"]["addr"],
            "0x0800b9d6",
        )
        self.assertEqual(
            sequences["meter_extended_boot_probe_prefix"]["addr"],
            "0x0800bace",
        )
        self.assertEqual(
            sequences["meter_variant_boot_tail"]["addr"],
            "0x0800bc32",
        )
        self.assertIn(
            "90 f8 68 0f 09 28",
            sequences["mode_init_dispatcher_tbh"]["bytes"],
        )
        self.assertIn(
            "09 21 28 68",
            sequences["meter_basic_boot_probe_prefix"]["bytes"],
        )
        self.assertIn(
            "07 21 00 06 58 bf 0a 21",
            sequences["meter_basic_boot_probe_prefix"]["bytes"],
        )
        self.assertIn(
            "1a 21 28 68",
            sequences["meter_basic_boot_range_tail"]["bytes"],
        )
        self.assertIn(
            "1e 20 27 e1",
            sequences["meter_basic_boot_range_tail"]["bytes"],
        )
        self.assertIn(
            "08 21 28 68",
            sequences["meter_extended_boot_probe_prefix"]["bytes"],
        )
        self.assertIn(
            "16 21 28 68",
            sequences["meter_extended_boot_range_tail"]["bytes"],
        )
        self.assertIn(
            "19 20 ab e0",
            sequences["meter_extended_boot_range_tail"]["bytes"],
        )
        self.assertIn(
            "12 21 28 68",
            sequences["meter_variant_boot_tail"]["bytes"],
        )
        self.assertIn(
            "14 21 28 68",
            sequences["meter_variant_boot_tail"]["bytes"],
        )
        self.assertEqual(
            direct_callers,
            [
                "0x08002daa",
                "0x080051d6",
                "0x0800533a",
                "0x08005572",
                "0x080271f8",
            ],
        )

    def test_stock_boot_mode_init_dmm_command_banks_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_boot_mode_init_dmm_command_banks()
        banks = result["banks"]

        self.assertEqual(
            banks["meter_basic_boot_probe_prefix"]["commands"],
            ["0x00", "0x09", "0x07/0x0A probe branch"],
        )
        self.assertEqual(
            banks["meter_basic_boot_range_tail"]["commands"],
            ["0x1A", "0x1B", "0x1C", "0x1D", "0x1E"],
        )
        self.assertEqual(
            banks["meter_extended_boot_probe_prefix"]["commands"],
            ["0x00", "0x08", "0x09", "0x07/0x0A probe branch"],
        )
        self.assertEqual(
            banks["meter_extended_boot_range_tail"]["commands"],
            ["0x16", "0x17", "0x18", "0x19"],
        )
        self.assertEqual(
            banks["meter_variant_boot_tail"]["commands"],
            ["0x00", "0x12", "0x13", "0x14", "0x09", "0x07/0x0A probe branch"],
        )
        self.assertIn(
            "07 21 00 06 58 bf 0a 21",
            banks["meter_basic_boot_probe_prefix"]["ordered_snippets"],
        )
        self.assertIn(
            "07 00 58 bf 0a 20",
            banks["meter_variant_boot_tail"]["ordered_snippets"],
        )

    def test_stock_runtime_mode_init_dispatch_callers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_runtime_mode_init_dispatch_caller_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["runtime_mode_init_forward_dispatcher"]["addr"],
            "0x08006418",
        )
        self.assertEqual(
            sequences["runtime_mode_init_reverse_dispatcher"]["addr"],
            "0x08006548",
        )
        self.assertIn(
            "90 f8 68 1f 01 39 08 29",
            sequences["runtime_mode_init_forward_dispatcher"]["bytes"],
        )
        self.assertIn(
            "c0 f8 68 1f 01 21 80 f8 55 13 bd e8 b0 40",
            sequences["runtime_mode_init_forward_state2_seed"]["bytes"],
        )
        self.assertIn(
            "80 f8 68 2f a0 f8 69 1f 80 f8 6b 1f",
            sequences["runtime_mode_init_forward_latch_collapse_to_state2"]["bytes"],
        )
        self.assertIn(
            "c0 f8 68 1f 01 21 80 f8 55 13 bd e8 b0 40",
            sequences["runtime_mode_init_reverse_state2_seed"]["bytes"],
        )
        self.assertIn(
            "80 f8 68 1f 00 21 a0 f8 1c 1e c0 f8 12 1e",
            sequences["runtime_mode_init_reverse_state_clear_to_state2"]["bytes"],
        )
        self.assertIn(
            "05 21 80 f8 68 1f bd e8 b0 40",
            sequences["runtime_mode_init_reverse_state5_seed"]["bytes"],
        )

    def test_stock_meter_mux_direct_callsites_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_mux_callsite_sequences()

        self.assertEqual(result["gpio_mux_portc_porte"]["target"], "0x080018a4")
        self.assertEqual(result["gpio_mux_porta_portb"]["target"], "0x08001a58")
        self.assertEqual(
            result["gpio_mux_portc_porte"]["calls"],
            [
                "0x080020b2", "0x080031e8", "0x080039a2", "0x0801a53e",
                "0x0801c7cc", "0x0801d094", "0x08025546", "0x08027242",
            ],
        )
        self.assertEqual(
            result["gpio_mux_portc_porte"]["direct_callers"],
            result["gpio_mux_portc_porte"]["calls"],
        )
        self.assertEqual(
            result["gpio_mux_porta_portb"]["calls"],
            [
                "0x08001f06", "0x08003644", "0x08003e3a", "0x0801a534",
                "0x0801c7d8", "0x0801d0a0", "0x0802554c", "0x0802724a",
            ],
        )
        self.assertEqual(
            result["gpio_mux_porta_portb"]["direct_callers"],
            result["gpio_mux_porta_portb"]["calls"],
        )
        self.assertEqual(
            result["gpio_mux_portc_porte"]["sequences"]["0x08025546"],
            "dc f7 ad f9",
        )
        self.assertEqual(
            result["gpio_mux_porta_portb"]["sequences"]["0x0802724a"],
            "da f7 05 fc",
        )

    def test_stock_meter_mux_writer_bodies_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_mux_writer_body_sequences()
        portc = result["gpio_mux_portc_porte"]
        porta = result["gpio_mux_porta_portb"]

        self.assertEqual(portc["target"], "0x080018a4")
        self.assertEqual(porta["target"], "0x08001a58")
        self.assertEqual(
            portc["slices"]["switch_prologue"]["bytes"],
            "09 28 00 f2 88 80 df e8 00 f0 05 08 0b 1f 22 25",
        )
        self.assertEqual(
            porta["slices"]["switch_prologue"]["bytes"],
            "09 28 00 f2 bb 80 df e8 00 f0 05 08 0b 1e 38 4c",
        )
        self.assertIn("gpio_pc12_pe_write_block", portc["slices"])
        self.assertIn("gpio_pa15_pb11_pb10_write_block", porta["slices"])
        self.assertIn("scope_calibration_table_select", portc["slices"])
        self.assertIn("scope_calibration_table_select", porta["slices"])
        self.assertIn("dac1_scope_tail", portc["slices"])
        self.assertIn("dac1_scope_tail", porta["slices"])

    def test_stock_scope_submode_mux_calls_are_negative_dmm_evidence(self) -> None:
        result = stock_meter_literals.verify_scope_submode_mux_call_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["scope_submode_post_calibration_mux_restore"]["addr"],
            "0x0801c7b8",
        )
        self.assertIn(
            "94 f8 30 00 00 f0 0f 00 e5 f7 6a f8",
            sequences["scope_submode_post_calibration_mux_restore"]["bytes"],
        )
        self.assertIn(
            "94 f8 30 00 00 f0 0f 00 e5 f7 3e f9",
            sequences["scope_submode_post_calibration_mux_restore"]["bytes"],
        )
        self.assertEqual(
            sequences["scope_submode_runtime_mux_restore"]["addr"],
            "0x0801d088",
        )
        self.assertIn(
            "96 f8 30 00 00 f0 0f 00 e4 f7 06 fc",
            sequences["scope_submode_runtime_mux_restore"]["bytes"],
        )
        self.assertIn(
            "96 f8 30 00 00 f0 0f 00 e4 f7 da fc",
            sequences["scope_submode_runtime_mux_restore"]["bytes"],
        )

    def test_stock_runtime_mux_state_writers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_runtime_mux_state_writer_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["siggen_scope_autorange_increment"]["addr"],
            "0x08001ee8",
        )
        self.assertEqual(
            sequences["scope_main_autorange_increment"]["addr"],
            "0x0801a526",
        )
        self.assertIn(
            "01 70 00 f0 d6 80 9b f8 03 00 ff f7 a7 fd",
            sequences["siggen_scope_autorange_increment"]["bytes"],
        )
        self.assertIn(
            "01 70 04 d0 9a f8 03 00 e7 f7 90 fa",
            sequences["scope_main_autorange_increment"]["bytes"],
        )
        self.assertIn(
            "9a f8 02 00 e7 f7 b1 f9",
            sequences["scope_main_autorange_increment"]["bytes"],
        )

    def test_stock_scope_snapshot_consumers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_scope_snapshot_consumer_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["scope_measurement_snapshot_from_mux_state"]["addr"],
            "0x08034078",
        )
        self.assertIn(
            "95 f8 2d 00",
            sequences["scope_measurement_snapshot_from_mux_state"]["bytes"],
        )
        self.assertIn(
            "a9 78 85 f8 c0 0d",
            sequences["scope_measurement_snapshot_from_mux_state"]["bytes"],
        )
        self.assertIn(
            "6b 79 a5 f8 be 1d",
            sequences["scope_measurement_snapshot_from_mux_state"]["bytes"],
        )

    def test_stock_scope_preset_mux_owners_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_scope_preset_mux_owner_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["scope_preset_mux_increment_prologue"]["addr"],
            "0x08003148",
        )
        self.assertEqual(
            sequences["scope_preset_mux_decrement_prologue"]["addr"],
            "0x08003900",
        )
        self.assertIn(
            "a8 78 fe f7 5c fb",
            sequences["scope_preset_mux_increment_portc_branch"]["bytes"],
        )
        self.assertIn(
            "e8 78 fe f7 08 fa",
            sequences["scope_preset_mux_increment_portab_branch"]["bytes"],
        )
        self.assertIn(
            "b0 78 fd f7 7f ff",
            sequences["scope_preset_mux_decrement_portc_branch"]["bytes"],
        )
        self.assertIn(
            "f0 78 fd f7 0d fe",
            sequences["scope_preset_mux_decrement_portab_branch"]["bytes"],
        )

    def test_stock_scope_ui_mux_lut_consumers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_scope_ui_mux_lut_consumer_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["scope_ui_draw_main_mux_lut_consumer"]["addr"],
            "0x080151b0",
        )
        self.assertIn(
            "98 f8 16 00",
            sequences["scope_ui_draw_main_mux_lut_consumer"]["bytes"],
        )
        self.assertIn(
            "40 44 81 78",
            sequences["scope_ui_draw_main_mux_lut_consumer"]["bytes"],
        )
        self.assertIn(
            "4b f6 b8 71 c0 b2 c0 f6 04 01 31 f8 10 00",
            sequences["scope_ui_draw_main_mux_lut_consumer"]["bytes"],
        )

    def test_stock_watchdog_reload_state_boundary_is_negative_dmm_evidence(self) -> None:
        result = stock_meter_literals.verify_watchdog_reload_state_boundary_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["init_iwdg_reload_from_DAT_2000105a"]["addr"],
            "0x08027372",
        )
        self.assertEqual(
            sequences["button_task_watchdog_reload_loop"]["addr"],
            "0x08039008",
        )
        self.assertIn("DAT_2000105a", result["ram_map"])
        self.assertEqual(result["full_decompile_ref"]["line"], 4733)
        self.assertIn("watchdog/UI housekeeping", result["classification"])
        self.assertIn("not DMM", result["classification"])
        self.assertIn(
            "97 f8 62 0f 00 28 18 bf c9 f8 00 00 a7 f8 6c 5f",
            sequences["button_task_watchdog_reload_loop"]["bytes"],
        )

    def test_stock_scope_mux_state_consumers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_scope_mux_state_consumer_sequences()
        sequences = result["sequences"]

        self.assertEqual(
            sequences["scope_timebase_ch1_mux_scale_consumer"]["addr"],
            "0x0801d2ec",
        )
        self.assertEqual(
            sequences["scope_timebase_ch2_mux_scale_consumer"]["addr"],
            "0x0801d8b8",
        )
        self.assertEqual(
            sequences["scope_math_delta_ch1_mux_scale_consumer"]["addr"],
            "0x0801f51e",
        )
        self.assertEqual(
            sequences["scope_math_delta_ch2_mux_scale_consumer"]["addr"],
            "0x0801f5fc",
        )
        self.assertEqual(
            sequences["scope_measurement_engine_mux_scale_consumer"]["addr"],
            "0x0801fd66",
        )
        self.assertIn(
            "98 f8 02 10 46 f2 cc 50",
            sequences["scope_timebase_ch1_mux_scale_consumer"]["bytes"],
        )
        self.assertIn(
            "98 f8 03 10 98 f9 05 20",
            sequences["scope_timebase_ch2_mux_scale_consumer"]["bytes"],
        )
        self.assertIn(
            "70 18 82 78 a2 fb 05 37",
            sequences["scope_math_delta_ch1_mux_scale_consumer"]["bytes"],
        )
        self.assertIn(
            "18 f8 02 1f 13 f9 04 2f",
            sequences["scope_measurement_engine_mux_scale_consumer"]["bytes"],
        )

    def test_re_coverage_requires_all_scope_mux_state_consumer_sites(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        for term in (
            "scope mux-state consumer guard",
            "0x0801D2EC",
            "0x0801D8B8",
            "0x0801F51E",
            "0x0801F5FC",
            "0x0801FD66",
            "remaining RAM-map consumers",
            "not DMM range proof",
        ):
            self.assertIn(term, coverage["terms"])


if __name__ == "__main__":
    unittest.main()
