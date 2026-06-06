#!/usr/bin/env python3
"""Focused tests for the DMM goal validator.

The validator is a gate for executable checks and dangerous-regression
searches.  It deliberately does not assert exact RE-note prose; the human notes
can change wording while stock binary tests and firmware behavior remain the
source of truth.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


REPO = Path(__file__).resolve().parents[1]

VALIDATOR_PATH = REPO / "scripts/validate_dmm_goal.py"
VALIDATOR_SPEC = importlib.util.spec_from_file_location(
    "validate_dmm_goal", VALIDATOR_PATH
)
assert VALIDATOR_SPEC is not None and VALIDATOR_SPEC.loader is not None
validate_dmm_goal = importlib.util.module_from_spec(VALIDATOR_SPEC)
VALIDATOR_SPEC.loader.exec_module(validate_dmm_goal)

STOCK_H2_PATH = REPO / "scripts/test_stock_h2_table.py"
STOCK_H2_SPEC = importlib.util.spec_from_file_location(
    "test_stock_h2_table", STOCK_H2_PATH
)
assert STOCK_H2_SPEC is not None and STOCK_H2_SPEC.loader is not None
stock_h2_table = importlib.util.module_from_spec(STOCK_H2_SPEC)
STOCK_H2_SPEC.loader.exec_module(stock_h2_table)


class DmmGoalValidatorTests(unittest.TestCase):
    def test_software_gate_runs_behavior_and_stock_binary_checks(self) -> None:
        commands = validate_dmm_goal.SOFTWARE_GATE_COMMANDS
        self.assertIn(("python3", "scripts/test_stock_h2_table.py"), commands)
        self.assertIn(("python3", "scripts/test_stock_meter_literals.py"), commands)
        self.assertIn(("make", "-C", "firmware", "test-meter"), commands)
        self.assertIn(("make", "-C", "firmware"), commands)
        self.assertIn(("git", "diff", "--check"), commands)

    def test_forbidden_searches_keep_hacks_out_of_firmware(self) -> None:
        searches = {
            str(item["label"]): item
            for item in validate_dmm_goal.SOFTWARE_GATE_FORBIDDEN_SEARCHES
        }
        self.assertIn("decoder value-shape hacks", searches)
        self.assertEqual(
            searches["decoder value-shape hacks"]["pattern"],
            validate_dmm_goal.FORBIDDEN_RE,
        )
        self.assertIn("stale H2 acceptance wording", searches)
        self.assertIn("stale magnitude-feedback range TODO", searches)

    def test_static_guards_are_forbidden_regression_checks_not_prose_requirements(self) -> None:
        for guard in (
            validate_dmm_goal.verify_no_unrecovered_meter_coefficients,
            validate_dmm_goal.verify_ac_status_boundary,
            validate_dmm_goal.verify_h2_tx_only_boundary,
            validate_dmm_goal.verify_no_magnitude_range_feedback,
        ):
            result = guard()
            self.assertIn("forbidden", result)
            self.assertNotIn("required", result)
            self.assertNotIn("required_meter_data", result)
            self.assertNotIn("snippet_anchors", result)

    def test_no_ocr_guard_is_active(self) -> None:
        result = validate_dmm_goal.verify_no_ocr_pipeline()
        self.assertIn("scripts/validate_dmm_goal.py", result["checked"])
        self.assertIn("scripts/openscope_live_debug.py", result["checked"])
        self.assertIn("py" + "tess" + "eract", result["forbidden"])
        self.assertIn("image_" + "to_string", result["forbidden"])

    def test_live_validation_only_switches_voltage_modes(self) -> None:
        result = validate_dmm_goal.verify_live_validation_safety_contract()
        self.assertEqual(
            result["mode_commands"],
            ["mode meter 0 0", "mode meter 1 0", "mode meter 0 0"],
        )

    def test_meter_transition_drain_gates_usart_enable(self) -> None:
        result = validate_dmm_goal.verify_meter_transition_usart_gate()
        ordered = result["ordered"]
        self.assertIn(
            "USART2->ctrl1 = ctrl1 & ~(USART_CTRL1_UEN |",
            ordered,
        )
        self.assertIn(
            "USART2->ctrl1 = (ctrl1 | USART_CTRL1_UEN | USART_CTRL1_RDBFIEN) &",
            ordered,
        )

    def test_meter_dump_parser_and_dcv_assertion(self) -> None:
        text = """
=== DMM State ===
mode=1 startup=Meter meter_submode=0 (DC Voltage) layout=0 (full)
valid=1 reading_submode=0 class=1 updates=12 display=0.4365 unit=V
bcd_value=4365 decimal_pos=1 negative=0
frame_family expected=0 observed=0 reject=0
frame=5A A5 44 8E EF E7 07 24 80 00 01 89
"""
        parsed = validate_dmm_goal.parse_meter_dump_block(text)
        self.assertEqual(parsed["meter_submode"], 0)
        self.assertEqual(parsed["reading_submode"], 0)
        self.assertEqual(parsed["valid"], 1)
        self.assertEqual(parsed["display_value"], 0.4365)
        validate_dmm_goal.assert_dcv_matches_observed(parsed, 0.435, 0.01)
        with self.assertRaisesRegex(validate_dmm_goal.GateError, "DCV mismatch"):
            validate_dmm_goal.assert_dcv_matches_observed(parsed, 0.200, 0.05)

    def test_acv_reject_assertion_requires_missing_ac_evidence_shape(self) -> None:
        reject = {
            "meter_submode": 1,
            "reading_submode": 1,
            "valid": 0,
            "display": "---",
            "reject": 3,
        }
        validate_dmm_goal.assert_acv_rejects_dc(reject)
        reject["valid"] = 1
        with self.assertRaisesRegex(validate_dmm_goal.GateError, "ACV did not reject"):
            validate_dmm_goal.assert_acv_rejects_dc(reject)

    def test_stock_h2_table_is_binary_grounded(self) -> None:
        if not stock_h2_table.BIN.exists():
            self.skipTest(f"missing stock APP binary: {stock_h2_table.BIN}")

        result = stock_h2_table.verify_h2_table()
        self.assertEqual(result["file_offset"], "0x51d19")
        self.assertEqual(result["flash_addr"], "0x08051d19")
        self.assertEqual(result["stats"]["total_bytes"], 115638)
        self.assertEqual(result["tail_range"], (0x1C340, 0x1C3B5, 118))
        labels = {item["label"] for item in result["preamble_sequence"]}
        self.assertIn("h2_start_3b_with_cs_low", labels)
        spi3_pc6_labels = [item["label"] for item in result["spi3_enable_pc6_sequence"]]
        self.assertLess(
            spi3_pc6_labels.index("spi3_ctrl1_spe_set"),
            spi3_pc6_labels.index("pc6_high_after_spi_enable"),
        )
        self.assertLess(
            spi3_pc6_labels.index("pc6_high_after_spi_enable"),
            spi3_pc6_labels.index("first_handshake_cs_high_after_delay"),
        )
        close_labels = {item["label"] for item in result["close_sequence"]}
        self.assertIn("close_opcode_3a_with_cs_low", close_labels)
        self.assertEqual(
            [item["payload"] for item in result["post_h2_queue_sequence"]],
            [1, 2, 6, 7, 8],
        )
        self.assertEqual(
            {item["queue"] for item in result["post_h2_queue_sequence"]},
            {"0x20002d78"},
        )

    def test_skip_live_main_report_does_not_emit_prose_coverage_gate(self) -> None:
        outdir = REPO / "tmp/test_dmm_goal_validation_lean"
        rc = validate_dmm_goal.main([
            "--skip-software",
            "--skip-live",
            "--outdir",
            str(outdir),
        ])
        self.assertEqual(rc, 0)
        report = (outdir / "report.json").read_text(encoding="utf-8")
        self.assertNotIn("re_comment_coverage", report)
        self.assertNotIn("state_machine_property_contract", report)
        self.assertNotIn("snippet_anchors", report)

    def test_nonlive_main_report_names_executable_property_contracts(self) -> None:
        outdir = REPO / "tmp/test_dmm_goal_validation_property_report"
        original_run_software_gate = validate_dmm_goal.run_software_gate
        original_changed = validate_dmm_goal.firmware_changed_since_upstream
        original_preflight = validate_dmm_goal.preflight_current_firmware_image
        try:
            validate_dmm_goal.run_software_gate = lambda: []
            validate_dmm_goal.firmware_changed_since_upstream = lambda: True
            validate_dmm_goal.preflight_current_firmware_image = (
                lambda: {"status": "test-skip"}
            )

            rc = validate_dmm_goal.main([
                "--skip-live",
                "--outdir",
                str(outdir),
            ])
        finally:
            validate_dmm_goal.run_software_gate = original_run_software_gate
            validate_dmm_goal.firmware_changed_since_upstream = original_changed
            validate_dmm_goal.preflight_current_firmware_image = original_preflight

        self.assertEqual(rc, 0)
        report = (outdir / "report.json").read_text(encoding="utf-8")
        self.assertIn("state_machine_property_contract", report)
        self.assertIn("transition_plan_property_contract", report)
        self.assertIn("autoscan_property_contract", report)
        self.assertIn("meter_aux_afe_pin_policy", report)
        self.assertIn("boot_frontend_before_activation", report)
        self.assertIn("state_machine_property_matrix_covers_all_submodes", report)
        self.assertIn("test_count", report)
        self.assertNotIn("snippet_anchors", report)
        self.assertNotIn("required_note_terms", report)


if __name__ == "__main__":
    unittest.main()
