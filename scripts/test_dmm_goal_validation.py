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

    def test_unrecovered_meter_coefficients_are_absent(self) -> None:
        result = validate_dmm_goal.verify_no_unrecovered_meter_coefficients()
        self.assertIn("firmware/src/drivers/meter_data.c", result["checked"])
        self.assertIn("METER_CAL_LOW_OHM_FACTOR", result["forbidden"])

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
        self.assertIn("range class bits cover all 16 combinations",
                      result["regex_anchors"])
        self.assertIn("all non-voltage modes reject voltage frames",
                      result["regex_anchors"])
        self.assertIn("METER_REJECT_MISSING_AC_EVIDENCE",
                      result["snippet_anchors"])

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

    def test_stock_meter_selector_state_writers_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_meter_selector_state_sequences()
        sequences = result["sequences"]

        self.assertEqual(sequences["init_selector_reset"]["addr"], "0x08026fde")
        self.assertEqual(sequences["rx_force_mode_8"]["addr"], "0x08036d14")
        self.assertEqual(sequences["rx_force_mode_1"]["addr"], "0x08036d50")
        self.assertEqual(sequences["rx_shadow_two_with_frame_bit"]["addr"], "0x080373a8")
        self.assertIn("8a f8 2d 0f", sequences["init_selector_reset"]["bytes"])
        self.assertIn("87 f8 2d 0f", sequences["rx_force_mode_8"]["bytes"])
        self.assertIn("86 f8 2d 0f", sequences["rx_force_mode_1"]["bytes"])
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
            result["gpio_mux_porta_portb"]["calls"],
            [
                "0x08001f06", "0x08003644", "0x08003e3a", "0x0801a534",
                "0x0801c7d8", "0x0801d0a0", "0x0802554c", "0x0802724a",
            ],
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


if __name__ == "__main__":
    unittest.main()
