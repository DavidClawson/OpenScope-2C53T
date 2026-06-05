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
    ]
    checked = [
        "firmware/src/drivers/meter_data.c",
        "firmware/src/drivers/meter_data.h",
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
        "invalid_submode_rejects_without_becoming_dcv",
        "state_machine_property_matrix_covers_all_submodes",
        "invalidate_clears_stale_payload_for_every_ordered_mode_transition",
        "marker_visible_family_mismatch_matrix_clears_stale_payload",
        "voltage_payload_clears_stale_reading_in_all_non_voltage_modes",
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
        "ordered source submodes are exhaustive":
            r"for \(uint8_t source = 0; source < FPGA_METER_LOCAL_SUBMODE_COUNT; source\+\+\)",
        "ordered destination submodes are exhaustive":
            r"for \(uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest\+\+\)",
        "all non-voltage modes reject voltage frames":
            r"static const uint8_t (wrong_family_)?modes\[\]\s*=\s*"
            r"\{\s*2,\s*3,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10\s*\};",
    }
    required_snippets = [
        "METER_REJECT_MISSING_AC_EVIDENCE",
        "METER_REJECT_WRONG_FRAME_FAMILY",
        "low-dcv-voltage",
        "0.4366",
        "0.2000f",
        "one-point display coefficient",
        "FPGA_METER_FRAME_FAMILY_CONTINUITY",
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
        "local_splits_do_not_invent_extra_stock_selectors",
        "fallbacks",
        "rx_frame_gate_preserves_discard_budget_while_busy",
        "every_submode_transition_drains_before_accepting_frames",
    ]
    required_regexes = {
        "transition plan iterates every local submode":
            r"for \(uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i\+\+\)",
        "phase matrix iterates every local submode":
            r"for \(uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode\+\+\)",
        "planned discards drain in order":
            r"for \(uint8_t i = 0; i < plan\.discard_frames; i\+\+\)",
    }
    required_snippets = [
        "FPGA_METER_TRANSITION_DISCARD_FRAMES",
        "FPGA_METER_TRANSITION_SETTLE_MS",
        "FPGA_METER_FRAME_FAMILY_VOLTAGE",
        "FPGA_METER_FRAME_FAMILY_CURRENT",
        "FPGA_METER_FRAME_FAMILY_CONTINUITY",
        "busy frame rejected",
        "stable frame accepted",
        "all recovered stock slots covered",
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
        "r.is_ac = true",
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
        "shared local splits", "eight-entry stock selector table",
        "without binary stock evidence", "uA is unresolved and unexposed",
        "H2 table binary guard", "tail bytes", "0x1C340", "no ACK/apply proof",
        "transition phase matrix", "busy transition frame", "stable frame",
        "selector consumer xrefs", "0x080042E2", "0x080048BA", "0x20002D54",
        "dvom_TX raw-word consumer guard", "0x080373F4", "0x20002D74",
        "USART2 command path",
        "meter transport transition guard", "0x08026F8E", "0x0802700A",
        "pause/drain", "task suspension and queue reset",
        "runtime mode-switch transport guard", "0x08007360", "0x0800741A",
        "0x080074BE", "active/running epilogue", "normal runtime transitions",
        "selector state writer guard", "0x08036D14", "0x08036D50",
        "0x080373A8", "digital stock DMM FSM",
        "32-case range-class matrix", "all 16 combinations",
        "ordered mode-transition stale matrix", "every source submode",
        "state-machine property anchors", "all local submodes 0..10",
        "live validation only switches DCV/ACV",
        "mux callsite guard", "0x080020B2", "0x0801A53E", "0x0802724A",
        "saved-config meter-state unpack guard", "0x08025D92", "0x08006000",
        "persistent saved-config writer",
        "saved-config meter-state pack guard", "0x080223BC", "0x080224A0",
        "0x05050000", "default mux-state bytes",
        "saved-config pack caller guard", "0x0803972E", "probe_change_handler",
        "controlled shutdown/config-save", "not normal runtime DMM range switching",
        "USART TX config writer meter-case guard", "0x08039734", "0x080397C8",
        "0x0100 update mask", "separate FPGA config bitfield path",
        "visible direct callers are TIM5/TIM2 init",
        "boot mode-init DMM sequence guard", "0x0800B908", "0x0800B9D6",
        "0x0800BACE", "0x0800BC32", "0x20002D6C",
        "boot-time command queue", "not a DMM calibration or range-writer proof",
        "runtime mode-init dispatcher caller evidence", "0x08006418",
        "0x08006548", "ms[0xF68]", "tail-call", "FUN_0800B908",
        "not a runtime analog range writer",
        "complete direct mux callsite list", "scope/siggen mux callers are not DMM runtime range proof",
        "mux writer body guard", "gpio_pc12_pe_write_block",
        "gpio_pa15_pb11_pb10_write_block", "DAC1/scope calibration tail",
        "runtime mux-state writer guard", "0x08001EE8", "0x0801A526",
        "negative DMM evidence", "scope snapshot consumer guard",
        "0x08034078", "consumer/snapshot path, not a DMM mux writer",
        "scope/preset mux owner guard", "0x08003148", "0x08003900",
        "not DMM runtime range proof",
        "scope UI mux-LUT consumer guard", "0x080151B0", "0x080151C2",
        "FUN_08015f50", "scope render/scale consumer",
        "ACV format selector guard", "0x080371C8", "0x08037228",
        "frame[7] bit 0", "not AC evidence",
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
