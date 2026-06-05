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

    def test_unrecovered_meter_coefficients_are_absent(self) -> None:
        result = validate_dmm_goal.verify_no_unrecovered_meter_coefficients()
        self.assertIn("firmware/src/drivers/meter_data.c", result["checked"])
        self.assertIn("METER_CAL_LOW_OHM_FACTOR", result["forbidden"])

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


if __name__ == "__main__":
    unittest.main()
