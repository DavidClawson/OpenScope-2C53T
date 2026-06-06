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
        self.assertIn("firmware/src/drivers/flash_fs.c", result["checked"])
        self.assertIn("METER_CAL_LOW_OHM_FACTOR", result["forbidden"])
        self.assertIn("3:/System file/cal_ch1.bin", result["forbidden"])

    def test_re_coverage_requires_factory_cal_fail_closed_placeholder(self) -> None:
        coverage = validate_dmm_goal.verify_re_coverage()
        self.assertIn("flash_fs_load_factory_cal", coverage["terms"])
        self.assertIn("fail-closed placeholder", coverage["terms"])
        self.assertIn("cal_ch1.bin", coverage["terms"])
        self.assertIn("roll-buffer state", coverage["terms"])
        self.assertIn("roll-buffer preload guard", coverage["terms"])

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

    def test_transition_plan_property_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_transition_plan_property_contract()
        self.assertEqual(result["file"], "firmware/tests/test_fpga_meter_plan.c")
        self.assertIn(
            "every_submode_transition_drains_before_accepting_frames",
            result["tests"],
        )
        self.assertIn(
            "rx_frame_gate_preserves_discard_budget_while_busy",
            result["tests"],
        )
        self.assertIn("phase matrix iterates every local submode",
                      result["regex_anchors"])
        self.assertIn("planned discards drain in order",
                      result["regex_anchors"])
        self.assertIn("stable frame accepted", result["snippet_anchors"])

    def test_autoscan_property_contract_is_anchored(self) -> None:
        result = validate_dmm_goal.verify_autoscan_property_contract()
        self.assertEqual(result["file"], "firmware/tests/test_meter_auto.c")
        self.assertIn("ac_voltage_requires_frequency_evidence", result["tests"])
        self.assertIn("current_auto_scores_respect_ac_evidence", result["tests"])
        self.assertIn(
            "dc_voltage_scores_without_frequency_or_nonzero_magnitude",
            result["tests"],
        )
        self.assertIn("meter_auto_score(1, &r) == 0", result["snippet_anchors"])
        self.assertIn(
            "r.is_ac = true;\n    ASSERT(meter_auto_score(1, &r) == 0);",
            result["snippet_anchors"],
        )
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

    def test_stock_usart_tx_config_writer_meter_case_is_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_usart_tx_config_writer_meter_case_sequences()
        sequences = result["sequences"]
        callers = result["callers"]

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

    def test_stock_boot_mode_init_dmm_sequences_are_binary_grounded(self) -> None:
        result = stock_meter_literals.verify_boot_mode_init_dmm_sequences()
        sequences = result["sequences"]

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


if __name__ == "__main__":
    unittest.main()
