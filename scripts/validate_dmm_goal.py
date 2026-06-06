#!/usr/bin/env python3
"""Goal gate for stock-grounded OpenScope 2C53T DMM validation.

This script intentionally does not OCR webcam frames.  The webcam image is
captured as evidence; the observed source/load value is read from that evidence
by the operator/agent using the image-view tool, then passed here as a recorded
visual observation.  The script verifies firmware/host gates and compares the
live CDC DMM reading against that visual observation.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any


REPO = Path(__file__).resolve().parents[1]
FORBIDDEN_RE = r"raw_bcd|display_value|magnitude|looks like|1800|2600"
DMM_MODE_COUNT = 11


class GateError(RuntimeError):
    pass


def run(cmd: list[str], *, timeout: float | None = None) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        cmd,
        cwd=REPO,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    if proc.returncode != 0:
        raise GateError(
            "command failed (%s):\n%s" % (" ".join(cmd), proc.stdout.rstrip())
        )
    return proc


def run_software_gate() -> list[dict[str, Any]]:
    commands = [
        ["python3", "-m", "py_compile",
         "scripts/openscope_live_debug.py",
         "scripts/flash_preflight.py",
         "scripts/hid_flash.py",
         "scripts/validate_dmm_goal.py",
         "scripts/test_stock_h2_table.py"],
        ["python3", "scripts/test_openscope_live_debug.py"],
        ["python3", "scripts/test_flash_preflight.py"],
        ["python3", "scripts/test_stock_h2_table.py"],
        ["python3", "scripts/test_dmm_goal_validation.py"],
        ["make", "-C", "firmware", "test-meter"],
        ["make", "-C", "firmware", "clean"],
        ["make", "-C", "firmware"],
        ["git", "diff", "--check"],
    ]
    results: list[dict[str, Any]] = []
    for cmd in commands:
        started = time.monotonic()
        proc = run(cmd)
        results.append({
            "cmd": cmd,
            "seconds": round(time.monotonic() - started, 3),
            "tail": "\n".join(proc.stdout.rstrip().splitlines()[-12:]),
        })

    proc = subprocess.run(
        ["rg", "-n", FORBIDDEN_RE, "firmware/src/drivers", "firmware/src/ui"],
        cwd=REPO,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if proc.returncode == 0:
        raise GateError("forbidden decoder/search terms found:\n" + proc.stdout.rstrip())
    if proc.returncode not in (1,):
        raise GateError("forbidden-search command failed:\n" + proc.stdout.rstrip())
    results.append({
        "cmd": ["!", "rg", "-n", FORBIDDEN_RE,
                "firmware/src/drivers", "firmware/src/ui"],
        "seconds": 0,
        "tail": "no hits",
    })
    return results


def verify_no_unrecovered_meter_coefficients() -> dict[str, Any]:
    forbidden = [
        "METER_CAL_LOW_OHM_FACTOR",
        "0.0304f",
        "bench-unit stand-in",
        "3:/System file/cal_ch1.bin",
        "3:/System file/cal_ch2.bin",
        "apply these coefficients in meter_data.c",
    ]
    checked = [
        "firmware/src/drivers/meter_data.c",
        "firmware/src/drivers/meter_data.h",
        "firmware/src/drivers/flash_fs.c",
        "firmware/src/drivers/flash_fs.h",
    ]
    hits: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for needle in forbidden:
            if needle in text:
                hits.append(f"{rel}: {needle}")
    if hits:
        raise GateError("unrecovered meter calibration coefficient still present: " +
                        "; ".join(hits))
    return {"checked": checked, "forbidden": forbidden}


def verify_no_ocr_pipeline() -> dict[str, Any]:
    forbidden = [
        "py" + "tess" + "eract",
        "easy" + "ocr",
        "tess" + "eract",
        "image_" + "to_string",
        "image-to" + "-text",
        "image to" + " text",
    ]
    checked = [
        "scripts/validate_dmm_goal.py",
        "scripts/openscope_live_debug.py",
    ]
    hits: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        lowered = text.lower()
        for needle in forbidden:
            if needle.lower() in lowered:
                hits.append(f"{rel}: {needle}")
    if hits:
        raise GateError(
            "DMM visual validation must remain image-view/manual evidence, "
            "not an OCR pipeline: " + "; ".join(hits)
        )
    return {"checked": checked, "forbidden": forbidden}


def verify_ac_status_boundary() -> dict[str, Any]:
    checked = [
        "reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md",
        "reverse_engineering/analysis_v120/fpga_comms_deep_dive.c",
        "reverse_engineering/analysis_v120/meter_math_pipeline_annotated.c",
        "reverse_engineering/analysis_v120/meter_fsm_deep_dive.md",
        "reverse_engineering/analysis_v120/fpga_task_decompile.txt",
        "scripts/decompile_fpga_task.py",
    ]
    forbidden = [
        "rx[7] bit 2 = " + "AC flag",
        "Bit 2: " + "AC flag",
        "AC/DC " + "flag for DCV",
        "status_byte bit2 (" + "AC/DC flag)",
        "AC flag / " + "decimal helper",
    ]
    required = [
        "not recovered as AC-present confidence",
        "status/decimal helper",
    ]
    hits: list[str] = []
    missing: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        lowered = text.lower()
        for needle in forbidden:
            if needle.lower() in lowered:
                hits.append(f"{rel}: {needle}")
        for needle in required:
            if needle.lower() not in lowered:
                missing.append(f"{rel}: {needle}")
    if hits or missing:
        raise GateError(
            "AC status boundary drifted: "
            f"forbidden_hits={hits} missing_required={missing}"
        )
    return {"checked": checked, "forbidden": forbidden, "required": required}


def verify_h2_tx_only_boundary() -> dict[str, Any]:
    """Ensure H2 diagnostics keep TX-complete separate from FPGA acceptance."""
    required = {
        "firmware/src/drivers/fpga.h": [
            "H2 SPI3 table TX diagnostic",
            "streamed the stock 115638-byte table",
            "no recovered ACK",
            "not proof that the table was accepted or applied",
        ],
        "firmware/src/drivers/fpga.c": [
            "TX-side diagnostic only",
            "Stock ignores MISO during this loop",
            "no\n     * FPGA ACK/apply status has been recovered",
            "never be treated as DMM calibration acceptance proof",
        ],
        "firmware/src/ui/scope_ui.c": [
            "H2 means bytes streamed, not recovered FPGA acceptance",
            "H2T:%c",
        ],
        "firmware/src/drivers/usb_debug.c": [
            "TX complete: %s (no recovered FPGA ACK)",
            "Bytes sent: %lu / 115638",
        ],
        "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md": [
            "The open firmware now streams the",
            "proves byte transmission",
            "still require a recovered ACK/apply condition",
        ],
        "reverse_engineering/analysis_v120/fpga_h2_spi3_bulk.md": [
            "Current open-firmware boundary",
            "TX-side diagnostic only",
            "unresolved acceptance/effect boundary",
        ],
    }
    forbidden = {
        "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md": [
            "Our custom firmware skips it entirely",
        ],
        "reverse_engineering/analysis_v120/fpga_h2_spi3_bulk.md": [
            "No\nbulk cal upload via SPI3 cmds 0x3B/0x3A",
            "That's the gap",
        ],
    }

    checked: dict[str, list[str]] = {}
    missing: list[str] = []
    for rel, snippets in required.items():
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        checked[rel] = snippets
        for snippet in snippets:
            if snippet not in text:
                missing.append(f"{rel}: {snippet}")
    stale: list[str] = []
    for rel, snippets in forbidden.items():
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for snippet in snippets:
            if snippet in text:
                stale.append(f"{rel}: {snippet}")
    if missing or stale:
        raise GateError(
            "H2 TX-only boundary drifted: "
            f"missing={missing} stale={stale}"
        )
    return {"checked": checked, "forbidden": forbidden}


def verify_state_machine_property_contract() -> dict[str, Any]:
    """Anchor the broad DMM software model so the gate cannot pass a thin harness.

    The C tests are still the executable proof.  This static check makes sure
    the goal gate is tied to the adversarial state-machine tests themselves,
    not just to a green aggregate test target whose contents could drift.
    """
    rel = "firmware/tests/test_meter_data.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    required_tests = [
        "dcv_stock_range_class_priority_all_bit_combinations",
        "dcv_live_0200_frame_preserves_stock_math_as_unresolved_frontend",
        "acv_rejects_dc_voltage_without_ac_evidence",
        "ac_current_rejects_current_frame_without_ac_evidence",
        "ac_modes_require_frequency_hint_boundaries",
        "passive_formatter_debug_fields_cover_diode_and_extended_splits",
        "invalid_submode_rejects_without_becoming_dcv",
        "state_machine_property_matrix_covers_all_submodes",
        "invalidate_clears_stale_payload_for_every_ordered_mode_transition",
        "transport_gate_blocks_source_frames_during_every_transition",
        "marker_visible_family_mismatch_matrix_clears_stale_payload",
        "frame6_0x40_is_not_a_global_resistance_family_marker",
        "voltage_payload_clears_stale_reading_in_all_non_voltage_modes",
        "low_dcv_voltage_payload_clears_stale_current_reading",
        "dcv_aux_extra_bytes_do_not_change_stock_range_class",
        "large_current_submodes_use_active_local_range_state",
        "current_submodes_do_not_expose_unproven_microamp_unit",
    ]
    required_regexes = {
        "all local submodes 0..10 are enumerated":
            r"static const uint8_t modes\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=\s*"
            r"\{\s*0,\s*1,\s*2,\s*3,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10\s*\};",
        "range class bits cover all 16 combinations":
            r"for \(uint8_t bits = 0; bits < 16; bits\+\+\)",
        "range class +10000 extension is covered":
            r"for \(uint8_t extend = 0; extend < 2; extend\+\+\)",
        "dcv auxiliary extra bytes are varied independently":
            r"static const uint16_t extra_cases\[\]\s*=\s*"
            r"\{\s*0x0000,\s*0x0031,\s*0x014E,\s*0x017F,\s*0x03FF,\s*0xFFFF\s*\};",
        "ac evidence boundaries cover all AC submodes":
            r"static const uint8_t ac_modes\[\]\s*=\s*\{\s*1,\s*4,\s*5\s*\};",
        "ac evidence rejects below and above frequency window":
            r"static const uint16_t extras\[\]\s*=\s*"
            r"\{\s*0x0000,\s*0x002C,\s*0x002D,\s*0x0041,\s*0x0042\s*\};",
        "ordered source submodes are exhaustive":
            r"for \(uint8_t source = 0; source < FPGA_METER_LOCAL_SUBMODE_COUNT; source\+\+\)",
        "ordered destination submodes are exhaustive":
            r"for \(uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest\+\+\)",
        "transport gate uses planned destination discard budget":
            r"uint8_t discard = dest_plan\.discard_frames;",
        "transition gate blocks busy source frames":
            r"fpga_meter_rx_frame_should_parse\(true, &discard,\s*&transition_skips\)",
        "all non-voltage modes reject voltage frames":
            r"static const uint8_t (wrong_family_)?modes\[\]\s*=\s*"
            r"\{\s*2,\s*3,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10\s*\};",
        "special voltage terminal frames reject in every non-voltage submode":
            r"static const uint8_t special_voltage_reject_modes\[\]\s*=\s*"
            r"\{\s*2,\s*3,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10\s*\};",
        "low dcv stale-current guard covers all current submodes":
            r"static const uint8_t low_dcv_current_modes\[\]\s*=\s*"
            r"\{\s*2,\s*3,\s*4,\s*5\s*\};",
        "local AC A display keeps stock ACA unit index":
            r"process_frame\(ac_current_frame,\s*5\);[\s\S]*"
            r"ASSERT\(expect_normal_reading\(\"2\.261\",\s*\"A\",\s*2\.261f,\s*0\.001f\)\);[\s\S]*"
            r"ASSERT\(meter_reading\.stock_mode == 3\);[\s\S]*"
            r"ASSERT\(meter_reading\.stock_unit_index == 5\);",
    }
    required_snippets = [
        "METER_REJECT_MISSING_AC_EVIDENCE",
        "METER_REJECT_WRONG_FRAME_FAMILY",
        "meter_reading.is_ac == ((statuses[s] & 0x04U) != 0)",
        "low-dcv-voltage",
        "0.4366",
        "0.2000f",
        "one-point display coefficient",
        "dbg_frame[10]",
        "dbg_frame[11]",
        "FPGA_METER_FRAME_FAMILY_CONTINUITY",
        "FPGA_METER_FRAME_FAMILY_DIODE",
        "FPGA_METER_FRAME_FAMILY_EXTENDED",
        "not a standalone cross-mode family marker",
        "meter_reading.stock_composite_index == 12",
        "meter_reading.stock_composite_index == 9",
        "uA",
    ]

    missing_tests = [
        name for name in required_tests
        if f"static int test_{name}" not in text or f"TEST({name});" not in text
    ]
    missing_regexes = [
        name for name, pattern in required_regexes.items()
        if re.search(pattern, text, re.MULTILINE) is None
    ]
    missing_snippets = [snippet for snippet in required_snippets if snippet not in text]
    if missing_tests or missing_regexes or missing_snippets:
        raise GateError(
            "state-machine property contract check failed: "
            f"missing_tests={missing_tests} "
            f"missing_regexes={missing_regexes} "
            f"missing_snippets={missing_snippets}"
        )
    return {
        "file": rel,
        "tests": required_tests,
        "regex_anchors": list(required_regexes),
        "snippet_anchors": required_snippets,
    }


def verify_transition_plan_property_contract() -> dict[str, Any]:
    """Anchor the transition-plan tests that exercise stock-like mode changes."""
    rel = "firmware/tests/test_fpga_meter_plan.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    required_tests = [
        "transition_plan_covers_mux_family_and_settle_policy",
        "state_machine_contract_is_exhaustive",
        "frame_family_mismatch_policy_matrix_is_exhaustive",
        "local_splits_do_not_invent_extra_stock_selectors",
        "fallbacks",
        "rx_frame_gate_preserves_discard_budget_while_busy",
        "every_submode_transition_drains_before_accepting_frames",
    ]
    required_regexes = {
        "local stock-mode mapping preserves recovered shared slots":
            r"static const uint8_t expected_stock_mode\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=\s*"
            r"\{\s*0,\s*1,\s*2,\s*2,\s*3,\s*3,\s*4,\s*6,\s*7,\s*5,\s*5\s*\};",
        "local selector words preserve recovered stock command table":
            r"static const uint16_t expected_words\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=\s*"
            r"\{\s*0x0514,\s*0x050C,\s*0x0517,\s*0x0517,\s*0x050B,\s*"
            r"0x050B,\s*0x050A,\s*0x0511,\s*0x0510,\s*0x0512,\s*0x0512\s*\};",
        "runtime apply words stay limited to recovered helper slots":
            r"static const uint16_t expected_apply\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=\s*"
            r"\{\s*0x0000,\s*0x050D,\s*0x050E,\s*0x050E,\s*0x0000,\s*"
            r"0x0000,\s*0x0000,\s*0x0516,\s*0x0515,\s*0x0000,\s*0x0000\s*\};",
        "shared local split pairs remain explicit":
            r"\{\s*2,\s*3,\s*\"DC current small/A\"\s*\}[\s\S]*"
            r"\{\s*4,\s*5,\s*\"AC current small/A\"\s*\}[\s\S]*"
            r"\{\s*9,\s*10,\s*\"capacitance/temperature\"\s*\}",
        "transition plan iterates every local submode":
            r"for \(uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i\+\+\)",
        "phase matrix iterates every local submode":
            r"for \(uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode\+\+\)",
        "planned discards drain in order":
            r"for \(uint8_t i = 0; i < plan\.discard_frames; i\+\+\)",
        "frame-family policy iterates expected families":
            r"for \(unsigned e = 0; e < sizeof\(families\); e\+\+\)",
        "frame-family policy iterates observed families":
            r"for \(unsigned o = 0; o < sizeof\(families\); o\+\+\)",
    }
    required_snippets = [
        "FPGA_METER_TRANSITION_DISCARD_FRAMES",
        "FPGA_METER_TRANSITION_SETTLE_MS",
        "FPGA_METER_FRAME_FAMILY_VOLTAGE",
        "FPGA_METER_FRAME_FAMILY_CURRENT",
        "FPGA_METER_FRAME_FAMILY_RESISTANCE",
        "FPGA_METER_FRAME_FAMILY_CONTINUITY",
        "FPGA_METER_FRAME_FAMILY_DIODE",
        "FPGA_METER_FRAME_FAMILY_EXTENDED",
        "fpga_meter_frame_family_is_acceptable",
        "families[e] == families[o]",
        "busy frame rejected",
        "stable frame accepted",
        "all recovered stock slots covered",
        "bad submode word",
        "bad plan portc/porte mux",
        "bad plan porta/portb mux",
        "bad plan discard",
    ]

    missing_tests = [
        name for name in required_tests
        if f"static void test_{name}" not in text or f"test_{name}();" not in text
    ]
    missing_regexes = [
        name for name, pattern in required_regexes.items()
        if re.search(pattern, text, re.MULTILINE) is None
    ]
    missing_snippets = [snippet for snippet in required_snippets if snippet not in text]
    if missing_tests or missing_regexes or missing_snippets:
        raise GateError(
            "transition-plan property contract check failed: "
            f"missing_tests={missing_tests} "
            f"missing_regexes={missing_regexes} "
            f"missing_snippets={missing_snippets}"
        )
    return {
        "file": rel,
        "tests": required_tests,
        "regex_anchors": list(required_regexes),
        "snippet_anchors": required_snippets,
    }


def verify_autoscan_property_contract() -> dict[str, Any]:
    """Anchor auto-selection scoring so AC/current modes need real evidence."""
    rel = "firmware/tests/test_meter_auto.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    required_tests = [
        "candidate_order_keeps_voltage_before_passive_and_current",
        "wrong_submode_never_scores",
        "dirty_frame_family_state_never_scores",
        "dc_voltage_scores_without_frequency_or_nonzero_magnitude",
        "ac_voltage_requires_frequency_evidence",
        "current_auto_scores_respect_ac_evidence",
        "temperature_scores_as_passive_candidate",
        "continuity_marker_beats_resistance_normal",
    ]
    required_snippets = [
        "meter_auto_score(1, &r) == 0",
        "r.aux_freq_hz = 49.9f",
        "r.is_ac = true;\n    ASSERT(meter_auto_score(1, &r) == 0);",
        "r.submode = 4;\n    ASSERT(meter_auto_score(4, &r) == 0);",
        "meter_auto_score(4, &r) == 0",
        "meter_auto_score(5, &r) == 0",
        "r.observed_frame_family = (uint8_t)FPGA_METER_FRAME_FAMILY_VOLTAGE",
        "r.reject_reason = METER_REJECT_WRONG_FRAME_FAMILY",
        "r.bcd_value = 0",
        "dc_voltage_scores_without_frequency_or_nonzero_magnitude",
    ]

    missing_tests = [
        name for name in required_tests
        if f"static int test_{name}" not in text or f"TEST({name});" not in text
    ]
    missing_snippets = [snippet for snippet in required_snippets if snippet not in text]
    if missing_tests or missing_snippets:
        raise GateError(
            "autoscan property contract check failed: "
            f"missing_tests={missing_tests} "
            f"missing_snippets={missing_snippets}"
        )
    return {
        "file": rel,
        "tests": required_tests,
        "snippet_anchors": required_snippets,
    }


def verify_re_coverage() -> dict[str, Any]:
    required_docs = [
        "reverse_engineering/analysis_v120/meter_stock_multiplier_tables_2026_06_05.md",
        "reverse_engineering/analysis_v120/meter_mode_command_table_2026_06_05.md",
        "reverse_engineering/analysis_v120/meter_acv_stock_case_2026_06_05.md",
        "reverse_engineering/analysis_v120/meter_math_pipeline_annotated.c",
        "reverse_engineering/analysis_v120/dmm_state_machine_contract_2026_06_05.md",
        "reverse_engineering/analysis_v120/meter_dac1_scope_boundary_2026_06_06.md",
        "reverse_engineering/analysis_v120/meter_w25q_calibration_boundary_2026_06_06.md",
        "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md",
        "reverse_engineering/analysis_v120/h2_extracted/FINDINGS.md",
    ]
    required_code = [
        "firmware/src/drivers/meter_data.c",
        "firmware/src/drivers/fpga_meter_plan.c",
        "firmware/src/drivers/fpga.c",
        "firmware/src/ui/meter_ui.c",
    ]
    required_terms = [
        "DC voltage", "AC voltage", "DC current", "AC current",
        "resistance", "continuity", "diode", "capacitance",
        "temperature", "selector", "mux", "settle", "discard",
        "empirical", "stock", "H2", "SPI3", "115,638",
        "acceptance proof", "unproven", "DAC1", "scope trigger",
        "not DMM calibration", "METER_REJECT_UNRESOLVED_CALIBRATION",
        "low-Ohm normal frames therefore", "9999.BIN", "cluster 0",
        "size 0", "not a recovered meter calibration source",
        "flash_fs_load_factory_cal", "fail-closed placeholder",
        "cal_ch1.bin", "cal_ch2.bin", "roll-buffer state",
        "roll-buffer preload guard", "0x08001830", "0x080271A8",
        "state+0x356", "state+0x483", "0x12D",
        "display formatter dispatch guard", "0x08002AA0", "0x08002B20",
        "0x08002B34", "DAT_20001026", "DAT_20001030",
        "unit indices 8/9/10/11", "format offsets +9/+10",
        "unit lookup boundary guard", "0x0804C40C", "0x08009AE4",
        "not a recovered stock unit string table", "zero-filled lookup region",
        "shared local splits", "eight-entry stock selector table",
        "without binary stock evidence", "uA is unresolved and unexposed",
        "H2 table binary guard", "tail bytes", "0x1C340", "no ACK/apply proof",
        "transition phase matrix", "busy transition frame", "stable frame",
        "selector consumer xrefs", "0x080042E2", "0x080048BA", "0x20002D54",
        "selector adjuster guard", "0x080041F8", "0x080047CC",
        "decrement/increment", "wrap over 0..7", "0x1D", "0x1B",
        "dynamic raw-word helper guard", "0x08006060", "0x08006120",
        "0x080062F8", "0x0501", "mask `0xC6`", "0x0C/0x0D",
        "0x0E/0x17", "0x11/0x16", "0x10/0x15",
        "dvom_TX raw-word consumer guard", "0x080373F4", "0x20002D74",
        "USART2 command path",
        "meter transport transition guard", "0x08026F8E", "0x0802700A",
        "pause/drain", "task suspension and queue reset",
        "0x08026F50", "ms[0xF64]", "saved mode-init restore",
        "runtime mode-switch transport guard", "0x08007360", "0x0800741A",
        "0x080074BE", "active/running epilogue", "normal runtime transitions",
        "selector state writer guard", "0x08036D14", "0x08036D50",
        "0x080373A8", "digital stock DMM FSM",
        "32-case range-class matrix", "all 16 combinations",
        "ordered mode-transition stale matrix", "every source submode",
        "transport-gate source-frame matrix", "planned destination discard budget",
        "state-machine property anchors", "all local submodes 0..10",
        "live validation only switches DCV/ACV",
        "mux callsite guard", "0x080020B2", "0x0801A53E", "0x0802724A",
        "saved-config meter-state unpack guard", "0x08025D92", "0x08006000",
        "persistent saved-config writer",
        "saved-config meter-state pack guard", "0x080223BC", "0x080224A0",
        "0x05050000", "default mux-state bytes",
        "saved-config pack caller guard", "0x08002F8C", "0x08002FE2",
        "0x08005B4A", "0x0803972E", "direct-BL-shaped",
        "literal/data region", "branch island", "probe_change_handler",
        "controlled shutdown/config-save", "housekeeping threshold path",
        "not normal runtime DMM range switching",
        "USART TX config writer meter-case guard", "0x08039734", "0x080397C8",
        "0x0100 update mask", "separate FPGA config bitfield path",
        "visible direct callers are TIM5/TIM2 init",
        "complete direct callsite set", "0x080272D4", "0x08027344",
        "no runtime DMM caller is recovered",
        "boot mode-init DMM sequence guard", "0x0800B908", "0x0800B9D6",
        "0x0800BACE", "0x0800BC32", "0x20002D6C",
        "complete direct callsite set for this dispatcher", "0x08002DAA",
        "0x080051D6", "0x0800533A", "0x08005572", "0x080271F8",
        "command-dispatch entry evidence only",
        "command-byte banks", "0x1A..0x1E", "0x16..0x19", "0x12/0x13/0x14",
        "boot-time command queue", "not a DMM calibration or range-writer proof",
        "runtime mode-init dispatcher caller evidence", "0x08006418",
        "0x08006548", "ms[0xF68]", "tail-call", "FUN_0800B908",
        "not a runtime analog range writer",
        "complete direct mux callsite list", "whole APP image for direct",
        "callers to `0x080018A4` and `0x08001A58`",
        "scope/siggen mux callers are not DMM runtime range proof",
        "mux writer body guard", "gpio_pc12_pe_write_block",
        "gpio_pa15_pb11_pb10_write_block", "DAC1/scope calibration tail",
        "runtime mux-state writer guard", "0x08001EE8", "0x0801A526",
        "mux-state RAM-map boundary", "DAT_200000fa (25 refs)",
        "DAT_200000fb (11 refs)", "function-level refs",
        "negative DMM evidence", "scope snapshot consumer guard",
        "scope-submode mux call guard", "0x0801C7B8", "0x0801D088",
        "DAT_20000128", "state[0x30]", "scope runtime reconfiguration",
        "0x08034078", "consumer/snapshot path, not a DMM mux writer",
        "scope/preset mux owner guard", "0x08003148", "0x08003900",
        "not DMM runtime range proof",
        "scope UI mux-LUT consumer guard", "0x080151B0", "0x080151C2",
        "FUN_08015f50", "scope render/scale consumer",
        "ACV format selector guard", "0x080371C8", "0x08037228",
        "frame[7] bit 0", "not AC evidence", "is_ac status-bit mirror",
        "frame[7].2", "ACV/ACA auto confidence",
        "scope mux-state consumer guard", "0x0801D2EC", "0x0801EFC0",
        "0x0801F6F8", "remaining RAM-map consumers", "not DMM range proof",
    ]

    haystack = ""
    missing_files: list[str] = []
    for rel in required_docs + required_code:
        path = REPO / rel
        if not path.exists():
            missing_files.append(rel)
            continue
        haystack += "\n" + path.read_text(encoding="utf-8", errors="replace")

    missing_terms = [term for term in required_terms
                     if term.lower() not in haystack.lower()]
    if missing_files or missing_terms:
        raise GateError(
            "RE/comment coverage check failed: "
            f"missing_files={missing_files} missing_terms={missing_terms}"
        )
    return {
        "docs": required_docs,
        "code": required_code,
        "terms": required_terms,
    }


def firmware_changed_since_upstream() -> bool:
    proc = run(["git", "diff", "--name-only", "origin/feature/meter-voltage-waveform..HEAD"])
    return any(line.startswith("firmware/") for line in proc.stdout.splitlines())


def preflight_current_firmware_image() -> dict[str, Any]:
    image = REPO / "firmware/build/firmware.bin"
    if not image.exists():
        return {"status": "skipped", "reason": "firmware image does not exist yet"}
    proc = run([
        "python3", "scripts/flash_preflight.py", "hid-app",
        "--image", str(image.relative_to(REPO)),
        "--address", "0x08004000",
        "--image-only",
    ])
    return {"status": "ok", "tail": "\n".join(proc.stdout.rstrip().splitlines()[-8:])}


def capture_webcam(device: str, output: Path, size: str) -> dict[str, Any]:
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "ffmpeg", "-hide_banner", "-loglevel", "error",
        "-f", "v4l2", "-input_format", "mjpeg", "-video_size", size,
        "-i", device, "-frames:v", "1", str(output),
    ]
    run(cmd, timeout=10)
    if not output.exists() or output.stat().st_size <= 0:
        raise GateError(f"webcam capture did not create a non-empty frame: {output}")
    return {"device": device, "path": str(output), "bytes": output.stat().st_size}


def live_debug(args: list[str]) -> str:
    cmd = ["python3", "scripts/openscope_live_debug.py", *args]
    return run(cmd, timeout=30).stdout


def parse_meter_dump_block(text: str) -> dict[str, Any]:
    blocks = [block for block in text.split("=== DMM State ===") if "valid=" in block]
    if blocks:
        text = "=== DMM State ===" + blocks[-1]

    data: dict[str, Any] = {}
    patterns = {
        "mode": r"mode=(\d+)",
        "meter_submode": r"meter_submode=(\d+)",
        "valid": r"valid=(\d+)",
        "reading_submode": r"reading_submode=(\d+)",
        "class": r"class=(\d+)",
        "display": r"display=([^\s]+)",
        "unit": r"unit=([^\s]*)",
        "bcd_value": r"bcd_value=(-?\d+)",
        "decimal_pos": r"decimal_pos=(\d+)",
        "reject": r"reject=(\d+)",
    }
    for key, pattern in patterns.items():
        match = re.search(pattern, text)
        if not match:
            raise GateError(f"meter dump missing {key}: {text[:400]!r}")
        value = match.group(1)
        data[key] = int(value) if value.lstrip("-").isdigit() else value
    try:
        data["display_value"] = float(str(data["display"]))
    except ValueError:
        data["display_value"] = None
    frame_match = re.search(r"frame=([0-9A-Fa-f ]+)", text)
    if frame_match:
        data["frame"] = frame_match.group(1).strip()
    return data


def parse_meter_dumps(text: str) -> list[dict[str, Any]]:
    blocks = [block for block in text.split("=== DMM State ===") if "valid=" in block]
    if not blocks:
        return [parse_meter_dump_block(text)]
    return [parse_meter_dump_block("=== DMM State ===" + block) for block in blocks]


def select_dcv_dump(text: str) -> dict[str, Any]:
    dumps = parse_meter_dumps(text)
    for dump in reversed(dumps):
        if dump.get("meter_submode") == 0 and dump.get("reading_submode") == 0 and \
           dump.get("valid") == 1 and dump.get("display_value") is not None:
            return dump
    return dumps[-1]


def select_acv_reject_dump(text: str) -> dict[str, Any]:
    dumps = parse_meter_dumps(text)
    for dump in reversed(dumps):
        if dump.get("meter_submode") == 1 and dump.get("reading_submode") == 1 and \
           dump.get("valid") == 0 and dump.get("display") == "---" and \
           dump.get("reject") == 3:
            return dump
    return dumps[-1]


def assert_dcv_matches_observed(meter: dict[str, Any], observed: float, tolerance: float) -> None:
    if meter["valid"] != 1:
        raise GateError(f"DCV is invalid: {meter}")
    if meter["meter_submode"] != 0 or meter["reading_submode"] != 0:
        raise GateError(f"DCV mode/submode mismatch: {meter}")
    if meter.get("unit") != "V" or meter.get("display_value") is None:
        raise GateError(f"DCV did not produce a numeric volt reading: {meter}")
    delta = abs(float(meter["display_value"]) - observed)
    if delta > tolerance:
        raise GateError(
            f"DCV mismatch: CDC={meter['display_value']} V, "
            f"visual_observed={observed} V, tolerance={tolerance} V"
        )


def assert_acv_rejects_dc(meter: dict[str, Any]) -> None:
    if meter["meter_submode"] != 1 or meter["reading_submode"] != 1:
        raise GateError(f"ACV mode/submode mismatch: {meter}")
    if meter["valid"] != 0 or meter["display"] != "---" or meter["reject"] != 3:
        raise GateError(f"ACV did not reject DC input as missing AC evidence: {meter}")


def run_live_validation(args: argparse.Namespace, outdir: Path) -> dict[str, Any]:
    result: dict[str, Any] = {
        "visual_observed_source_voltage_v": args.observed_source_voltage,
        "voltage_tolerance_v": args.voltage_tolerance,
        "passive_live": "not probed on energized voltage input; parser tests cover stale/wrong-family rejection",
        "current_live": "not probed without correct jack and load-limited series wiring; parser tests cover voltage rejection",
    }
    errors: list[str] = []
    webcam = capture_webcam(args.webcam, outdir / "webcam_source_load.jpg", args.webcam_size)
    result["webcam"] = webcam
    screen_path = outdir / "openscope_screen.bmp"
    screen_stdout = live_debug([
        "screen-capture", "--output", str(screen_path),
        "--timeout", str(args.timeout),
        *([] if args.port is None else ["--port", args.port]),
    ])
    result["screen_capture"] = {"path": str(screen_path), "stdout": screen_stdout.strip()}

    live_debug(["command", "mode meter 0 0", "--timeout", str(args.timeout),
                *([] if args.port is None else ["--port", args.port])])
    time.sleep(args.settle_seconds)
    dcv_text = live_debug([
        "meter-dump", "--count", "5", "--interval", "0.4",
        "--timeout", str(args.timeout),
        *([] if args.port is None else ["--port", args.port]),
    ])
    dcv = select_dcv_dump(dcv_text)
    result["dcv"] = dcv
    try:
        assert_dcv_matches_observed(dcv, args.observed_source_voltage, args.voltage_tolerance)
    except GateError as exc:
        errors.append(str(exc))

    live_debug(["command", "mode meter 1 0", "--timeout", str(args.timeout),
                *([] if args.port is None else ["--port", args.port])])
    time.sleep(args.settle_seconds)
    acv_text = live_debug([
        "meter-dump", "--count", "5", "--interval", "0.4",
        "--timeout", str(args.timeout),
        *([] if args.port is None else ["--port", args.port]),
    ])
    acv = select_acv_reject_dump(acv_text)
    result["acv_same_dc_input"] = acv
    try:
        assert_acv_rejects_dc(acv)
    except GateError as exc:
        errors.append(str(exc))

    live_debug(["command", "mode meter 0 0", "--timeout", str(args.timeout),
                *([] if args.port is None else ["--port", args.port])])

    result["passed"] = not errors
    if errors:
        result["errors"] = errors
        result["error"] = "; ".join(errors)
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--outdir", type=Path, default=Path("tmp/dmm_goal_validation"))
    parser.add_argument("--skip-software", action="store_true")
    parser.add_argument("--skip-live", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--timeout", type=float, default=4.0)
    parser.add_argument("--webcam", default="/dev/video0")
    parser.add_argument("--webcam-size", default="1920x1080")
    parser.add_argument("--observed-source-voltage", type=float,
                        help="volts read visually from the captured webcam evidence")
    parser.add_argument("--voltage-tolerance", type=float, default=0.05)
    parser.add_argument("--settle-seconds", type=float, default=1.0)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    outdir = args.outdir
    outdir.mkdir(parents=True, exist_ok=True)

    report: dict[str, Any] = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "head": run(["git", "rev-parse", "HEAD"]).stdout.strip(),
    }
    try:
        if not args.skip_software:
            report["software_gate"] = run_software_gate()
            report["firmware_changed_since_upstream"] = firmware_changed_since_upstream()
            report["flash_preflight"] = preflight_current_firmware_image()
        report["state_machine_property_contract"] = verify_state_machine_property_contract()
        report["transition_plan_property_contract"] = verify_transition_plan_property_contract()
        report["autoscan_property_contract"] = verify_autoscan_property_contract()
        report["re_comment_coverage"] = verify_re_coverage()
        report["no_unrecovered_meter_coefficients"] = verify_no_unrecovered_meter_coefficients()
        report["no_ocr_pipeline"] = verify_no_ocr_pipeline()
        report["ac_status_boundary"] = verify_ac_status_boundary()
        report["h2_tx_only_boundary"] = verify_h2_tx_only_boundary()

        if not args.skip_live:
            if args.observed_source_voltage is None:
                raise GateError(
                    "--observed-source-voltage is required for live validation; "
                    "read it from the captured webcam evidence with the image-view tool"
                )
            report["live_validation"] = run_live_validation(args, outdir)
            if not report["live_validation"].get("passed", False):
                raise GateError(report["live_validation"].get("error", "live validation failed"))

        report_path = outdir / "report.json"
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8")
        print(f"DMM goal validation ok: {report_path}")
        return 0
    except Exception as exc:
        report["error"] = str(exc)
        report_path = outdir / "report.json"
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8")
        print(f"DMM goal validation failed: {exc}", file=sys.stderr)
        print(f"partial report: {report_path}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
