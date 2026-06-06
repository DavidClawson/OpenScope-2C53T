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
SOFTWARE_GATE_COMMANDS: tuple[tuple[str, ...], ...] = (
    (
        "python3", "-m", "py_compile",
        "scripts/openscope_live_debug.py",
        "scripts/flash_preflight.py",
        "scripts/hid_flash.py",
        "scripts/validate_dmm_goal.py",
        "scripts/test_dmm_goal_validation.py",
        "scripts/test_stock_meter_literals.py",
        "scripts/test_stock_h2_table.py",
    ),
    ("python3", "scripts/test_openscope_live_debug.py"),
    ("python3", "scripts/test_flash_preflight.py"),
    ("python3", "scripts/test_stock_h2_table.py"),
    ("python3", "scripts/test_stock_meter_literals.py"),
    ("python3", "scripts/test_dmm_goal_validation.py"),
    ("make", "-C", "firmware", "test-meter"),
    ("make", "-C", "firmware", "clean"),
    ("make", "-C", "firmware"),
    ("git", "diff", "--check"),
)
SOFTWARE_GATE_FORBIDDEN_SEARCHES: tuple[dict[str, object], ...] = (
    {
        "label": "decoder value-shape hacks",
        "pattern": FORBIDDEN_RE,
        "paths": ("firmware/src/drivers", "firmware/src/ui"),
    },
    {
        "label": "stale AC/DC status-bit claims",
        "pattern": (
            "frame" + r"\[7\]\.2.*AC/DC|" +
            "AC/DC" + r".*frame\[7\]\.2"
        ),
        "paths": (
            "reverse_engineering",
            "scripts",
            "firmware/src/drivers",
            "firmware/src/ui",
        ),
    },
    {
        "label": "stale H2 dummy-exchange choreography",
        "pattern": "dummy" + "-exchange|" + "test" + "-sequence",
        "paths": ("reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md",),
    },
    {
        "label": "stale magnitude-feedback range TODO",
        "pattern": "Detect BCD " + "overflow|send higher " + "range params",
        "paths": ("firmware/src/drivers", "firmware/src/ui"),
    },
    {
        "label": "stale H2 acceptance wording",
        "pattern": (
            "H2 Upload " + "Verification|" +
            "Re-upload H2 " + r"\+ capture FPGA responses|" +
            "accepting the data, we might see non-FF " + "responses"
        ),
        "paths": (
            "firmware/src/drivers",
            "reverse_engineering/analysis_v120",
            "scripts",
        ),
    },
)


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
    results: list[dict[str, Any]] = []
    for command in SOFTWARE_GATE_COMMANDS:
        cmd = list(command)
        started = time.monotonic()
        proc = run(cmd)
        results.append({
            "cmd": cmd,
            "seconds": round(time.monotonic() - started, 3),
            "tail": "\n".join(proc.stdout.rstrip().splitlines()[-12:]),
        })

    for search in SOFTWARE_GATE_FORBIDDEN_SEARCHES:
        pattern = str(search["pattern"])
        paths = list(search["paths"])
        label = str(search["label"])
        started = time.monotonic()
        proc = subprocess.run(
            ["rg", "-n", pattern, *paths],
            cwd=REPO,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if proc.returncode == 0:
            raise GateError(
                f"forbidden search hit ({label}):\n" + proc.stdout.rstrip()
            )
        if proc.returncode not in (1,):
            raise GateError(
                f"forbidden search failed ({label}):\n" + proc.stdout.rstrip()
            )
        results.append({
            "cmd": ["!", "rg", "-n", pattern, *paths],
            "label": label,
            "seconds": round(time.monotonic() - started, 3),
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
        "apply_stock_dcv_voltage_multiplier",
        "apply these coefficients in meter_data.c",
    ]
    required_meter_data = [
        "apply_stock_dcv_decimal_exponent",
        "DCA formatter variant branch at 0x08002AFE/0x08002B54",
        "DAT_2000102e == 1 selects unit index 4",
        "DAT_2000102e == 2 selects unit index 3",
        "That proves formatter state only, not a recovered physical current range writer",
    ]
    checked = [
        "firmware/src/drivers/meter_data.c",
        "firmware/src/drivers/meter_data.h",
        "firmware/src/drivers/flash_fs.c",
        "firmware/src/drivers/flash_fs.h",
    ]
    hits: list[str] = []
    missing: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for needle in forbidden:
            if needle in text:
                hits.append(f"{rel}: {needle}")
        if rel == "firmware/src/drivers/meter_data.c":
            compact_text = " ".join(text.split()).replace(" * ", " ")
            missing.extend(
                needle for needle in required_meter_data
                if needle not in compact_text
            )
    if hits or missing:
        raise GateError(
            "unrecovered meter calibration/formatter boundary drifted: "
            f"forbidden_hits={hits} missing_meter_data={missing}"
        )
    return {
        "checked": checked,
        "forbidden": forbidden,
        "required_meter_data": required_meter_data,
    }


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
            "Replay H2 TX + sample MISO; no ACK/apply proof",
            "H2 TX Replay Diagnostic",
            "Samples MISO only; no recovered ACK/apply proof",
            "TX/sample diagnostic only; not calibration proof",
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
        "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md": [
            "2026-06-06 correction",
            "00 05 00 00",
            "12 00 00",
            "15 00 00 3B",
            "0x08026B7C..0x08026C30",
            "H2 byte count remains diagnostic only",
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
        "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md": [
            "CRITICAL — 4 dummy exchanges",
            "4 dummy SPI exchanges",
            "4 more dummy exchanges",
            "4+4 dummy SPI exchanges",
            "Recommended Test Sequence",
            "The dummy exchanges are the most likely fix",
        ],
        "firmware/src/drivers/usb_debug.c": [
            "spi3 h2verify                   Re-upload H2 + capture FPGA " + "responses",
            "H2 Upload " + "Verification",
            "accepting the data, we might see non-FF " + "responses (ACK bytes",
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


def verify_meter_aux_afe_pin_policy() -> dict[str, Any]:
    """Ensure PB9/PA6 DMM setup follows recovered stock evidence."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")

    match = re.search(
        r"static void fpga_set_meter_frontend_for_submode\(uint8_t submode\)"
        r"(?P<body>[\s\S]*?)\n}\n\nstatic void fpga_send_meter_wake_preamble",
        text,
    )
    if match is None:
        raise GateError("could not locate fpga_set_meter_frontend_for_submode block")
    body = match.group("body")
    required_body = [
        "Invalid local submodes still apply the baseline state",
        "(void)fpga_meter_mux_gpio_state_for_submode(submode, &mux_state);",
        "fpga_apply_meter_mux_gpio_state(&mux_state);",
    ]
    missing_body = [snippet for snippet in required_body if snippet not in body]
    forbidden_body = [
        "GPIOB->scr = (1U << 9);",
        "GPIOA->scr = (1U << 6);",
    ]
    stale_body = [snippet for snippet in forbidden_body if snippet in body]

    required_file = [
        "stock init configures them as outputs",
        "no stock BOP/BCR level write has been recovered",
        "GPIOB->clr = (1U << 9);",
        "GPIOA->clr = (1U << 6);",
        "static void fpga_apply_meter_mux_gpio_state",
        "fpga_gpio_write_level(GPIOB, (1U << 9), state->pb9);",
        "fpga_gpio_write_level(GPIOA, (1U << 6), state->pa6);",
        "production writes cannot\n     * drift away from the state-machine table",
        "not a recovered stock\n     * runtime DMM mux writer",
        "saved-config default ms[0x02]=5 /\n     * ms[0x03]=5 is persistence evidence only",
        "low-DCV mismatch still\n     * needs a new writer, trace, H2/apply proof, or factory-calibration source",
    ]
    missing_file = [snippet for snippet in required_file if snippet not in text]
    if missing_body or stale_body or missing_file:
        raise GateError(
            "PB9/PA6 auxiliary AFE pin policy drifted: "
            f"missing_body={missing_body} stale_body={stale_body} "
            f"missing_file={missing_file}"
        )
    return {
        "checked": rel,
        "required_body": required_body,
        "forbidden_body": forbidden_body,
        "required_file": required_file,
    }


def verify_meter_expected_selector_uses_plan_word() -> dict[str, Any]:
    """Ensure debug/expected selector metadata mirrors the transition plan word."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")

    match = re.search(
        r"fpga_meter_selector_t fpga_meter_expected_selectors\(uint8_t submode\)"
        r"(?P<body>[\s\S]*?)\n}\n\nstatic void fpga_gpio_write_level",
        text,
    )
    if match is None:
        raise GateError("could not locate fpga_meter_expected_selectors block")
    body = match.group("body")
    required_body = [
        "fpga_meter_transition_plan_t plan =",
        "selectors.function_selector = plan.stock_mode;",
        "plan.selector_word != FPGA_METER_INVALID_SELECTOR_WORD",
        "(uint8_t)(plan.selector_word & 0x00FFU)",
        "selectors.voltage_function_axis = plan.voltage_function_axis;",
    ]
    forbidden_body = [
        "fpga_meter_stock_cmd_low_for_mode(plan.stock_mode)",
    ]
    missing_body = [snippet for snippet in required_body if snippet not in body]
    stale_body = [snippet for snippet in forbidden_body if snippet in body]
    if missing_body or stale_body:
        raise GateError(
            "meter expected-selector metadata drifted from transition plan: "
            f"missing_body={missing_body} stale_body={stale_body}"
        )
    return {
        "checked": rel,
        "required_body": required_body,
        "forbidden_body": forbidden_body,
    }


def verify_meter_sequence_tail_uses_transition_plan() -> dict[str, Any]:
    """Ensure the runtime selector/apply/probe/start tail follows the plan."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")

    match = re.search(
        r"static void fpga_send_meter_mode_sequence\(uint8_t submode\)"
        r"(?P<body>[\s\S]*?)\n}\n\nvoid fpga_set_meter_mode",
        text,
    )
    if match is None:
        raise GateError("could not locate fpga_send_meter_mode_sequence block")
    body = match.group("body")
    required_body = [
        "fpga_meter_transition_plan_t plan =",
        "fpga.meter_mode_selector_word = plan.selector_word;",
        "fpga.meter_mode_apply_word = plan.apply_word;",
        "fpga.meter_mode_probe_word = plan.has_probe_detect ? probe_word : 0;",
        "fpga.meter_mode_start_word = plan.start_word;",
        "if (plan.has_probe_detect)",
        "(uint8_t)(plan.start_word >> 8)",
        "(uint8_t)(plan.start_word & 0x00FFU)",
    ]
    forbidden_body = [
        "fpga.meter_mode_start_word = (uint16_t)(0x0500U | FPGA_CMD_METER_START)",
        "fpga_timed_send_cmd(0x05, FPGA_CMD_METER_START, plan.settle_ms)",
    ]
    missing_body = [snippet for snippet in required_body if snippet not in body]
    stale_body = [snippet for snippet in forbidden_body if snippet in body]
    if missing_body or stale_body:
        raise GateError(
            "meter runtime sequence tail drifted from transition plan: "
            f"missing_body={missing_body} stale_body={stale_body}"
        )
    return {
        "checked": rel,
        "required_body": required_body,
        "forbidden_body": forbidden_body,
    }


def verify_meter_transition_production_contract() -> dict[str, Any]:
    """Ensure normal DMM mode changes and reinit share one transition path."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")

    def between(start: str, end: str) -> str:
        start_at = text.find(start)
        if start_at < 0:
            raise GateError(f"could not locate {start!r}")
        end_at = text.find(end, start_at)
        if end_at < 0:
            raise GateError(f"could not locate end marker {end!r} after {start!r}")
        return text[start_at:end_at]

    helper = between(
        "static void fpga_apply_meter_transition(uint8_t submode, bool wake_preamble)",
        "\nvoid fpga_set_meter_mode",
    )
    set_mode = between(
        "void fpga_set_meter_mode(uint8_t submode)",
        "\nvoid fpga_meter_reinit",
    )
    reinit = between(
        "void fpga_meter_reinit(uint8_t submode)",
        "\nvoid fpga_scope_wake",
    )

    required_helper = [
        "fpga_meter_transition_plan_t plan =",
        "meter_transition_busy = true;",
        "meter_data_invalidate(submode);",
        "if (!fpga_meter_submode_is_valid(submode))",
        "meter_transition_busy = false;",
        "fpga_meter_reset_transport();",
        "if (wake_preamble)",
        "fpga_send_meter_wake_preamble();",
        "fpga_set_meter_frontend_for_submode(submode);",
        "fpga_scope_delay_ms(plan.settle_ms);",
        "fpga_send_meter_mode_sequence(submode);",
        "fpga_meter_discard_next_frames(plan.discard_frames);",
    ]
    order = [
        "meter_data_invalidate(submode);",
        "if (!fpga_meter_submode_is_valid(submode))",
        "fpga_meter_reset_transport();",
        "fpga_set_meter_frontend_for_submode(submode);",
        "fpga_scope_delay_ms(plan.settle_ms);",
        "fpga_send_meter_mode_sequence(submode);",
        "fpga_meter_discard_next_frames(plan.discard_frames);",
    ]
    required_set_mode = [
        "if (!fpga.initialized) return;",
        "dac_output_is_running()",
        "dac_output_stop();",
        "fpga_apply_meter_transition(submode, false);",
    ]
    required_reinit = [
        "if (!fpga.initialized) return;",
        "fpga_apply_meter_transition(submode, true);",
    ]
    forbidden_public = [
        "fpga_meter_reset_transport();",
        "fpga_set_meter_frontend_for_submode(submode);",
        "fpga_scope_delay_ms(plan.settle_ms);",
        "fpga_send_meter_mode_sequence(submode);",
        "fpga_meter_discard_next_frames(plan.discard_frames);",
    ]

    missing_helper = [snippet for snippet in required_helper if snippet not in helper]
    missing_set_mode = [snippet for snippet in required_set_mode if snippet not in set_mode]
    missing_reinit = [snippet for snippet in required_reinit if snippet not in reinit]
    stale_public = [
        snippet for snippet in forbidden_public
        if snippet in set_mode or snippet in reinit
    ]
    ordering = [helper.find(snippet) for snippet in order]
    bad_order = (
        any(index < 0 for index in ordering) or
        any(left >= right for left, right in zip(ordering, ordering[1:]))
    )

    if missing_helper or missing_set_mode or missing_reinit or stale_public or bad_order:
        raise GateError(
            "meter production transition contract drifted: "
            f"missing_helper={missing_helper} "
            f"missing_set_mode={missing_set_mode} "
            f"missing_reinit={missing_reinit} "
            f"stale_public={stale_public} bad_order={bad_order}"
        )
    return {
        "checked": rel,
        "required_helper": required_helper,
        "required_set_mode": required_set_mode,
        "required_reinit": required_reinit,
        "forbidden_public": forbidden_public,
        "order": order,
    }


def verify_meter_apply_pair_production_comment() -> dict[str, Any]:
    """Ensure stock-derived apply words stay documented at their use site."""
    rel = "firmware/src/drivers/fpga_meter_plan.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"bool fpga_meter_stock_apply_cmd_word_for_submode"
        r"\(uint8_t submode, uint16_t \*word\)"
        r"(?P<body>[\s\S]*?)\n}\n\nfpga_meter_frame_family_t",
        text,
    )
    if match is None:
        raise GateError("could not locate meter stock apply-word helper")
    body = match.group("body")
    required_body = [
        "Stock V1.2.0 dynamic raw-word helper boundary",
        "0x08006120 gates and masks",
        "0x08006194 / 0x0800626A choose low-byte pairs",
        "0x08006288 emits 0x0500 | low",
        "ACV 0x0C/0x0D",
        "DCA 0x17/0x0E",
        "continuity 0x11/0x16",
        "diode 0x10/0x15",
        "no recovered",
        "DCV",
        "ACA",
        "resistance",
        "capacitance",
        "temperature",
        "microamp",
        "must not be filled from the parsed numeric value",
        "one-point live observations",
        "local range guesses",
    ]
    forbidden_body = [
        "looks like",
        "raw_bcd",
        "display_value",
        "magnitude-based",
    ]
    missing_body = [snippet for snippet in required_body if snippet not in body]
    stale_body = [snippet for snippet in forbidden_body if snippet in body]
    if missing_body or stale_body:
        raise GateError(
            "meter apply-word production comment drifted from stock boundary: "
            f"missing_body={missing_body} stale_body={stale_body}"
        )
    return {
        "checked": rel,
        "required_body": required_body,
        "forbidden_body": forbidden_body,
    }


def verify_legacy_meter_fsm_unit_lookup_boundary() -> dict[str, Any]:
    """Ensure the legacy FSM note no longer overclaims stock unit strings."""
    rel = "reverse_engineering/analysis_v120/meter_fsm_deep_dive.md"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    required = [
        "Unit lookup boundary corrected 2026-06-06",
        "0x08009AE4",
        "zero-filled region at `0x0804C40C`",
        "not a recovered stock unit-string table",
        "local suffix text remains inferred/local",
        "display-state evidence, not recovered stock suffix-string evidence",
        "Stock unit suffix table contents",
        "not recovered stock string-table evidence",
    ]
    forbidden = [
        "Unit strings live in flash at 0x804c40c",
        "DAT_20001026 to string lookup",
        "goes through flash table 0x804c40c",
        "Flash string table contents",
        "Strings in our firmware should follow the unit_variant table",
    ]
    missing = [snippet for snippet in required if snippet not in text]
    stale = [snippet for snippet in forbidden if snippet in text]
    if missing or stale:
        raise GateError(
            "legacy meter FSM unit lookup boundary drifted: "
            f"missing={missing} stale={stale}"
        )
    return {
        "checked": rel,
        "required": required,
        "forbidden": forbidden,
    }


def verify_legacy_meter_fsm_range_command_boundary() -> dict[str, Any]:
    """Ensure the legacy FSM note no longer overclaims range commands absent."""
    rel = "reverse_engineering/analysis_v120/meter_fsm_deep_dive.md"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    normalized = " ".join(text.split())
    required = [
        "Range-command boundary corrected 2026-06-06",
        "No magnitude-derived range feedback function was found",
        "display-side formatter and meter-mode dispatcher, not a range controller",
        "DMM boot/runtime mode-init command banks",
        "`0x1A..0x1E`, `0x16..0x19`, `0x12/0x13/0x14`",
        "runtime dispatcher callers into `FUN_0800B908`",
        "DMM-owned runtime analog range writer for `ms[0x02]`/`ms[0x03]`",
        "Do not turn this into a claim that stock sends no meter range commands at all",
        "DMM-owned runtime range writer",
        "old \"never sends range commands\" conclusion is too strong",
    ]
    forbidden = [
        "True auto-range is not implemented in stock firmware",
        "does not send range commands back",
        "has no code to command FPGA range changes",
        "never sends range commands back",
        "autonomous and not firmware-controlled",
    ]
    missing = [
        snippet for snippet in required
        if " ".join(snippet.split()) not in normalized
    ]
    stale = [
        snippet for snippet in forbidden
        if " ".join(snippet.split()) in normalized
    ]
    if missing or stale:
        raise GateError(
            "legacy meter FSM range-command boundary drifted: "
            f"missing={missing} stale={stale}"
        )
    return {
        "checked": rel,
        "required": required,
        "forbidden": forbidden,
    }


def verify_legacy_top_level_dmm_protocol_boundaries() -> dict[str, Any]:
    """Ensure top-level legacy RE docs cannot revive old DMM range/cal claims."""
    checked = [
        "reverse_engineering/FPGA_PROTOCOL_COMPLETE.md",
        "reverse_engineering/ARCHITECTURE.md",
        "reverse_engineering/CALIBRATION.md",
    ]
    required = {
        "reverse_engineering/FPGA_PROTOCOL_COMPLETE.md": [
            "Legacy correction (2026-06-06)",
            "Type-4-shaped arm and command-bank bytes above are not proof of normal DMM runtime range configuration",
            "DMM-owned runtime analog writer for `ms[0x02]`/`ms[0x03]`",
            "display_decimal_shift",
            "DAT_2000102f",
            "not factory calibration",
            "status/decimal helper and is not recovered as AC-present confidence",
        ],
        "reverse_engineering/ARCHITECTURE.md": [
            "Legacy correction (2026-06-06)",
            "`ms[0xF37]` is `DAT_2000102f`",
            "not a recovered factory calibration coefficient",
            "rx[7] bit 2` as AC-present confidence",
            "DMM-owned runtime analog range writer",
        ],
        "reverse_engineering/CALIBRATION.md": [
            "Double-Precision Display Decimal Pipeline",
            "`ms[0xF37]` to `DAT_2000102f`",
            "not a recovered factory calibration coefficient source",
            "rx[7] bit 2 = status/decimal helper",
            "Do not use `DAT_2000102f`/`ms[0xF37]`, `rx[7] bit 2`, or the `0x1B/0x1C/0x1E` command-bank bytes as a low-DCV/current/range coefficient",
        ],
    }
    forbidden = [
        "Meter: configure range",
        "Meter range config",
        "`meter_cal_coeff`",
        "Calibration coefficient selector",
        "rx[7] bit 2 = AC flag",
        "rx[7] bit 2 = AC,",
        "range indicator 2 / standby",
        "hold flag / cal coefficient",
        "Sets cal_coeff",
        "sets calibration coefficient",
        "set/clear cal_coeff",
        "cal_coeff 0 or 4",
        "cal_coeff selection",
    ]

    missing: list[str] = []
    hits: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        normalized = " ".join(text.split())
        for snippet in required[rel]:
            if " ".join(snippet.split()) not in normalized:
                missing.append(f"{rel}: {snippet}")
        for snippet in forbidden:
            if " ".join(snippet.split()) in normalized:
                hits.append(f"{rel}: {snippet}")
    if missing or hits:
        raise GateError(
            "legacy top-level DMM protocol/calibration docs drifted: "
            f"missing={missing} stale={hits}"
        )
    return {"checked": checked, "required": required, "forbidden": forbidden}


def verify_no_magnitude_range_feedback() -> dict[str, Any]:
    """Ensure production DMM frontend code does not suggest value-shaped ranging."""
    checked: dict[str, str] = {}
    stale: list[str] = []
    required = [
        "Range feedback is intentionally not driven from the parsed number",
        "`frame[8].7`, `frame[3].4`, `frame[4].4`, `frame[5].4`",
        "Do not infer\n         * a new relay/range command from the decoded number here",
        "`0.200 V` visual vs `0.4366 V` CDC",
        "frontend/H2/acceptance evidence problem",
        "Stock command-bank replay (0x1A..0x1E)",
        "command sequencing evidence only",
        "It does not prove DMM range\n     * parameters, a low-DCV correction, or a runtime writer for ms[0x02] /\n     * ms[0x03]",
        "0x0508 at 0x080033CA",
        "0x0509 at 0x08003BA4",
        "0x0514 at 0x08005B7A",
        "shared meter configure/setup byte; not a recovered DMM range",
    ]
    forbidden = [
        "TODO: Implement MCU-side auto-ranging with relay switching",
        "Detect BCD overflow",
        "Detect BCD underflow",
        "send higher range params",
        "send lower range params",
        "wildly wrong outside it",
        "param=0",
        "10V range",
        "Below ~1V",
        "Above 10V",
        "TODO: Find params",
        "Meter channel gain/offset/coupling initialization",
        "configure the FPGA meter IC",
        "Meter: configure range",
    ]

    for rel in [
        "firmware/src/drivers/fpga.c",
        "firmware/src/drivers/meter_data.c",
        "firmware/src/ui/meter_ui.c",
    ]:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        checked[rel] = text
        for snippet in forbidden:
            if snippet in text:
                stale.append(f"{rel}: {snippet}")

    missing = [
        snippet for snippet in required
        if snippet not in checked["firmware/src/drivers/fpga.c"]
    ]
    if missing or stale:
        raise GateError(
            "magnitude-derived range feedback boundary drifted: "
            f"missing={missing} stale={stale}"
        )
    return {
        "checked": list(checked),
        "required": required,
        "forbidden": forbidden,
    }


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
        "transition_phase_marker_frames_follow_destination_state",
        "marker_visible_family_mismatch_matrix_clears_stale_payload",
        "unclassified_normal_frames_follow_active_family_only",
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
        "transition phase marker matrix iterates every destination submode":
            r"test_transition_phase_marker_frames_follow_destination_state[\s\S]*"
            r"for \(uint8_t dest = 0; dest < FPGA_METER_LOCAL_SUBMODE_COUNT; dest\+\+\)",
        "stable marker frames follow destination parser state":
            r"test_transition_phase_marker_frames_follow_destination_state[\s\S]*"
            r"process_frame\(markers\[m\]\.frame, dest\);[\s\S]*"
            r"meter_reading\.expected_frame_family == dest_plan\.frame_family",
        "unclassified normal frames iterate every local submode":
            r"static const uint8_t modes\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=\s*"
            r"\{\s*0,\s*1,\s*2,\s*3,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10\s*\};",
        "unclassified normal frames are active-plan classified":
            r"test_unclassified_normal_frames_follow_active_family_only[\s\S]*"
            r"fpga_meter_frame_family_for_submode\(mode\),[\s\S]*"
            r"METER_REJECT_NONE",
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
        "unclassified normal frame",
        "active local transition plan",
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
        "stock_apply_words_for_runtime_family_switch",
        "mux_gpio_state_matches_stock_projection_for_every_submode",
        "mux_writer_stock_arm_truth_table_covers_all_10_switch_arms",
        "transition_settle_discard_policy_is_explicit_for_every_submode",
        "state_machine_contract_is_exhaustive",
        "frame_family_mismatch_policy_matrix_is_exhaustive",
        "frame_family_marker_visibility_documents_observed_gaps",
        "logical_function_capability_matrix_covers_all_dmm_modes",
        "local_splits_do_not_invent_extra_stock_selectors",
        "local_splits_share_mux_gpio_state",
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
        "runtime apply words are subset of stock dynamic helper pairs":
            r"static const uint16_t stock_dynamic_apply_words\[\]\s*=\s*"
            r"\{\s*0x050D,\s*0x050E,\s*0x0516,\s*0x0515\s*\};",
        "shared local split pairs remain explicit":
            r"\{\s*2,\s*3,\s*\"DC current small/A\"\s*\}[\s\S]*"
            r"\{\s*4,\s*5,\s*\"AC current small/A\"\s*\}[\s\S]*"
            r"\{\s*9,\s*10,\s*\"capacitance/temperature\"\s*\}",
        "transition plan iterates every local submode":
            r"for \(uint8_t i = 0; i < FPGA_METER_LOCAL_SUBMODE_COUNT; i\+\+\)",
        "mux gpio state table covers every local submode":
            r"static const fpga_meter_mux_gpio_state_t expected\[FPGA_METER_LOCAL_SUBMODE_COUNT\]\s*=",
        "stock mux-arm truth table covers all ten switch arms":
            r"test_mux_writer_stock_arm_truth_table_covers_all_10_switch_arms[\s\S]*"
            r"static const fpga_meter_mux_gpio_state_t expected\[10\]\s*=",
        "stock mux-arm truth table iterates arms 0 through 9":
            r"for \(uint8_t arm = 0; arm < 10; arm\+\+\)",
        "stock mux-arm invalid fallback is fail closed":
            r"invalid stock mux arm rejected[\s\S]*"
            r"invalid stock mux arm keeps baseline pc12[\s\S]*"
            r"invalid stock mux arm keeps baseline pb9",
        "auxiliary AFE pins stay low in tested mux states":
            r"\{\s*1,\s*1,\s*0,\s*1,\s*1,\s*1,\s*0,\s*1,\s*0,\s*0\s*\}",
        "logical DMM function table covers microamp gaps":
            r"static const uint8_t expected_submode\[FPGA_METER_LOGICAL_FUNCTION_COUNT\]\s*=\s*"
            r"\{\s*0,\s*1,\s*FPGA_METER_INVALID_LOCAL_SUBMODE,\s*2,\s*3,\s*"
            r"FPGA_METER_INVALID_LOCAL_SUBMODE,\s*4,\s*5,\s*6,\s*7,\s*8,\s*9,\s*10,\s*\};",
        "phase matrix iterates every local submode":
            r"for \(uint8_t mode = 0; mode < FPGA_METER_LOCAL_SUBMODE_COUNT; mode\+\+\)",
        "planned discards drain in order":
            r"for \(uint8_t i = 0; i < plan\.discard_frames; i\+\+\)",
        "frame-family policy iterates expected families":
            r"for \(unsigned e = 0; e < sizeof\(families\); e\+\+\)",
        "frame-family policy iterates observed families":
            r"for \(unsigned o = 0; o < sizeof\(families\); o\+\+\)",
        "marker-visible families stay limited to voltage and continuity":
            r"static const uint8_t marker_visible\[\]\s*=\s*"
            r"\{\s*FPGA_METER_FRAME_FAMILY_VOLTAGE,\s*"
            r"FPGA_METER_FRAME_FAMILY_CONTINUITY,\s*\};",
        "active-plan-only families stay marker-unproven":
            r"static const uint8_t active_plan_only\[\]\s*=\s*"
            r"\{\s*FPGA_METER_FRAME_FAMILY_CURRENT,\s*"
            r"FPGA_METER_FRAME_FAMILY_RESISTANCE,\s*"
            r"FPGA_METER_FRAME_FAMILY_DIODE,\s*"
            r"FPGA_METER_FRAME_FAMILY_EXTENDED,\s*\};",
    }
    required_snippets = [
        "FPGA_METER_TRANSITION_DISCARD_FRAMES",
        "FPGA_METER_TRANSITION_SETTLE_MS",
        "FPGA_METER_START_WORD",
        "FPGA_METER_FRAME_FAMILY_VOLTAGE",
        "FPGA_METER_FRAME_FAMILY_CURRENT",
        "FPGA_METER_FRAME_FAMILY_RESISTANCE",
        "FPGA_METER_FRAME_FAMILY_CONTINUITY",
        "FPGA_METER_FRAME_FAMILY_DIODE",
        "FPGA_METER_FRAME_FAMILY_EXTENDED",
        "fpga_meter_frame_family_is_acceptable",
        "fpga_meter_frame_family_has_stock_marker",
        "fpga_meter_mux_gpio_state_for_stock_mux_arms",
        "fpga_meter_mux_gpio_state_for_submode",
        "expect_mux_state",
        "mux gpio submode",
        "stock mux arm",
        "invalid stock mux arm rejected",
        "mux gpio shared",
        "families[e] == families[o]",
        "busy frame rejected",
        "stable frame accepted",
        "all recovered stock slots covered",
        "bad submode word",
        "bad plan portc/porte mux",
        "bad plan porta/portb mux",
        "bad plan discard",
        "bad plan has no probe",
        "bad plan has no start",
        "plan probe detect",
        "plan start word",
        "all logical DMM functions covered",
        "DC uA is unresolved",
        "AC uA is unresolved",
        "uniform local settle/discard",
        "invalid submodes emit no settle/discard",
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
        "unresolved_microamp_functions_are_not_autoscan_candidates",
        "temperature_scores_as_passive_candidate",
        "continuity_marker_beats_resistance_normal",
    ]
    required_snippets = [
        "meter_auto_score(1, &r) == 0",
        "r.aux_freq_hz = 49.9f",
        "r.aux_freq_hz = 1.0f",
        "r.aux_freq_hz = 44.9f",
        "r.aux_freq_hz = 45.0f",
        "r.aux_freq_hz = 65.0f",
        "r.aux_freq_hz = 66.0f",
        "r.is_ac = true;\n    ASSERT(meter_auto_score(1, &r) == 0);",
        "r.submode = 4;\n    ASSERT(meter_auto_score(4, &r) == 0);",
        "FPGA_METER_FUNCTION_DC_UA",
        "FPGA_METER_INVALID_LOCAL_SUBMODE",
        "meter_auto_score(4, &r) == 0",
        "meter_auto_score(5, &r) == 0",
        "meter_auto_score(4, &r) == 50",
        "meter_auto_score(5, &r) == 50",
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


def verify_ui_submode_surface_contract() -> dict[str, Any]:
    """Keep unresolved current ranges out of the user-visible mode surface."""
    checked = [
        "firmware/src/ui/ui.h",
        "firmware/src/ui/meter_ui.c",
        "firmware/src/drivers/fpga_meter_plan.h",
        "firmware/src/drivers/meter_auto.c",
        "firmware/src/drivers/meter_data.h",
    ]
    required = {
        "firmware/src/ui/ui.h": [
            "#define METER_SUBMODE_COUNT     11",
        ],
        "firmware/src/drivers/fpga_meter_plan.h": [
            "#define FPGA_METER_LOCAL_SUBMODE_COUNT 11u",
            "#define FPGA_METER_LOGICAL_FUNCTION_COUNT 13u",
            "FPGA_METER_FUNCTION_DC_UA",
            "FPGA_METER_FUNCTION_AC_UA",
        ],
        "firmware/src/ui/meter_ui.c": [
            "uA is intentionally absent from this UI/submode",
            "{ \"DC mA\",       \"mA\"",
            "{ \"DC Current\",  \"A\"",
            "{ \"AC mA\",       \"mA\"",
            "{ \"AC Current\",  \"A\"",
            "{ \"Temperature\", \"C\"",
        ],
        "firmware/src/drivers/meter_auto.c": [
            "static const uint8_t meter_auto_candidate_order[] = {\n"
            "    0, 1, 6, 7, 8, 9, 10, 2, 4, 3, 5\n"
            "};",
        ],
        "firmware/src/drivers/meter_data.h": [
            "uA is unresolved and not exposed as a local mode",
        ],
    }
    forbidden = {
        "firmware/src/drivers/meter_auto.c": [
            "uA",
            "microamp",
            "11,",
        ],
    }

    missing: list[str] = []
    hits: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for snippet in required.get(rel, []):
            if snippet not in text:
                missing.append(f"{rel}: {snippet}")
        for snippet in forbidden.get(rel, []):
            if snippet in text:
                hits.append(f"{rel}: {snippet}")
    if missing or hits:
        raise GateError(
            "UI/submode surface contract check failed: "
            f"missing={missing} forbidden_hits={hits}"
        )
    return {
        "checked": checked,
        "required": required,
        "forbidden": forbidden,
    }


def verify_live_validation_safety_contract() -> dict[str, Any]:
    """Keep energized live validation away from passive/current DMM ranges."""
    rel = "scripts/validate_dmm_goal.py"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"def run_live_validation\(args: argparse\.Namespace, outdir: Path\)"
        r"(?P<body>[\s\S]*?)\n\n\ndef build_parser",
        text,
    )
    if match is None:
        raise GateError("could not locate run_live_validation body")
    body = match.group("body")

    required = [
        '"passive_live": "not probed on energized voltage input; parser tests cover stale/wrong-family rejection"',
        '"current_live": "not probed without correct jack and load-limited series wiring; parser tests cover voltage rejection"',
        '"mode meter 0 0"',
        '"mode meter 1 0"',
    ]
    allowed_mode_commands = ["mode meter 0 0", "mode meter 1 0", "mode meter 0 0"]
    mode_commands = re.findall(r'"(mode meter \d+ 0)"', body)
    forbidden_commands = [
        command for command in mode_commands
        if command not in {"mode meter 0 0", "mode meter 1 0"}
    ]
    missing = [snippet for snippet in required if snippet not in body]
    bad_sequence = mode_commands != allowed_mode_commands
    if missing or forbidden_commands or bad_sequence:
        raise GateError(
            "live validation safety contract drifted: "
            f"missing={missing} forbidden_commands={forbidden_commands} "
            f"mode_commands={mode_commands}"
        )
    return {
        "checked": rel,
        "mode_commands": mode_commands,
        "required": required,
        "forbidden": "current/passive meter mode commands in energized live validation",
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
        "reverse_engineering/analysis_v120/dmm_selector_shadow_xref_closure_2026_06_06.md",
        "reverse_engineering/analysis_v120/dmm_mode_state_f68_boundary_2026_06_06.md",
        "reverse_engineering/analysis_v120/dmm_saved_mode_f64_boundary_2026_06_06.md",
        "reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md",
        "reverse_engineering/analysis_v120/SPI3_INIT_SEQUENCE_DECODED.md",
        "reverse_engineering/analysis_v120/h2_extracted/FINDINGS.md",
        "reverse_engineering/analysis_v120/dmm_evidence_gap_ledger_2026_06_06.md",
        "reverse_engineering/analysis_v120/live_dmm_visual_validation_2026_06_05.md",
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
        "UI/submode surface guard", "no recovered uA local submode",
        "logical DMM function matrix", "DC uA", "AC uA",
        "FPGA_METER_INVALID_LOCAL_SUBMODE", "13-function matrix",
        "H2 table binary guard", "tail bytes", "0x1C340", "no ACK/apply proof",
        "SPI3 init choreography correction", "0x08026B7C..0x08026C30",
        "H2 byte count remains diagnostic only",
        "transition phase matrix", "busy transition frame", "stable frame",
        "transition settle/discard policy guard",
        "uniform local settle/discard policy",
        "invalid submodes emit no settle/discard",
        "exact stock settle/discard counts remain open",
        "selector consumer xrefs", "0x080042E2", "0x080048BA", "0x20002D54",
        "selector adjuster guard", "0x080041F8", "0x080047CC",
        "decrement/increment", "wrap over 0..7", "0x1D", "0x1B",
        "dynamic raw-word helper guard", "0x08006060", "0x08006120",
        "0x080062F8", "0x0501", "mask `0xC6`", "0x0C/0x0D",
        "0x0E/0x17", "0x11/0x16", "0x10/0x15",
        "stock dynamic apply-pair boundary",
        "ACV 0x0C/0x0D", "DCA 0x17/0x0E",
        "continuity 0x11/0x16", "diode 0x10/0x15",
        "dvom_TX raw-word consumer guard", "0x080373F4", "0x20002D74",
        "USART2 command path", "10-tick stock dvom_TX command pacing guard",
        "0x0803744C", "0x0803744E", "BL 0x0803A390",
        "not a DMM settle/discard rule",
        "meter transport transition guard", "0x08026F8E", "0x0802700A",
        "pause/drain", "task suspension and queue reset",
        "0x08026F50", "ms[0xF64]", "saved mode-init restore",
        "runtime mode-switch transport guard", "0x08007360", "0x0800741A",
        "0x080074BE", "active/running epilogue", "normal runtime transitions",
        "selector state writer guard", "0x08036D14", "0x08036D50",
        "0x08037220", "0x080372E0", "0x08037328", "0x08037338",
        "0x080373A8", "digital stock DMM FSM", "variant shadow writer",
        "current formatter reads `DAT_2000102e`",
        "not a recovered current range writer",
        "DMM Selector/Shadow Xref Closure",
        "DAT_20001025` (`0x20001025`, `ms[0xF2D]`)",
        "DAT_2000102e` (`0x2000102e`, `ms[0xF36]`)",
        "DAT_20001025` has 9 RAM-map refs",
        "DAT_2000102e` has 7 RAM-map refs",
        "digital DMM selector/formatter-shadow state",
        "not the missing `ms[0x02]`/`ms[0x03]` analog mux/range writer",
        "not a low-DCV correction",
        "DMM Mode-State `ms[0xF68]` Boundary",
        "DAT_20001060` (`0x20001060`, `ms[0xF68]`)",
        "stock mode-state RAM-map boundary",
        "DAT_20001060` has 7 RAM-map refs",
        "stock mode-init/command-bank/transport state byte",
        "`ms[0xF68]` command-bank state is not a low-DCV correction",
        "`ms[0xF68]` state `1`/`2` transition evidence does not recover exact stock",
        "`ms[0xF68]` helper gating does not upgrade H2/SPI3 byte-count replay",
        "DMM Saved-Mode `ms[0xF64]` Boundary",
        "DAT_2000105c` (`0x2000105C`, `ms[0xF64]`)",
        "saved_mode_f64_config_load",
        "saved_mode_f64_to_live_f68_restore",
        "not display-only bitmap height",
        "Saved-mode `ms[0xF64]` boundary",
        "DAT_2000105c` has only two RAM-map refs",
        "stock config load writes word 12's low halfword",
        "boot restore reads it at `0x08026F50`",
        "`ms[0xF64]` is saved mode-init state",
        "current formatter variant guard", "0x08002AFE", "0x08002B54",
        "DAT_20001026 = 3", "DAT_20001026 = 4",
        "current-jack safety proof",
        "32-case range-class matrix", "all 16 combinations",
        "ordered mode-transition stale matrix", "every source submode",
        "transport-gate source-frame matrix", "planned destination discard budget",
        "state-machine property anchors", "all local submodes 0..10",
        "live validation only switches DCV/ACV",
        "mux callsite guard", "0x080020B2", "0x0801A53E", "0x0802724A",
        "auxiliary AFE PB9/PA6 guard", "0x080241D4", "0x080241E2",
        "configured as outputs only", "no recovered stock BOP/BCR level write",
        "stock-reset/output-low state",
        "saved-config meter-state unpack guard", "0x08025D92", "0x08006000",
        "persistent saved-config writer",
        "saved-config live mux-store guard", "0x08025D94",
        "0x08025DBC", "0x08025DC2",
        "boot/restore direct live mux stores",
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
        "Boot Mode-Init TBH State Map Guard", "0x0800B926",
        "state 0 -> 0x0800B93E", "state 9 -> 0x0800BC32",
        "bounded to states `0..9`",
        "not DMM\n  `ms[0x02]`/`ms[0x03]` analog mux state",
        "Meter Probe Branch Guard", "GPIOC bit 7", "0x40011008", "0x07/0x0A",
        "probe/tail sequencing only", "not a physical range/calibration source",
        "Meter Basic Raw-Word Queue Guard", "0x080033CA", "0x08003BA4",
        "0x08005B7A", "0x0508", "0x0509", "0x0514",
        "wake/start/variant sequencing",
        "not DMM runtime range state",
        "runtime mode-init dispatcher caller evidence", "0x08006418",
        "0x08006548", "ms[0xF68]", "tail-call", "FUN_0800B908",
        "not a runtime analog range writer",
        "Meter Transport Operation Guard", "USART2 enable",
        "DVOM task 0x20002DA0", "PC11 through GPIOC_BOP",
        "reset raw TX-word queue 0x20002D74",
        "clear stale meter state", "does not recover exact settle/discard timing",
        "complete direct mux callsite list", "whole APP image for direct",
        "callers to `0x080018A4` and `0x08001A58`",
        "Mux Writer Literal-Pointer Negative Guard",
        "no static 32-bit literal/function-pointer refs",
        "computed or state-mediated path still requires a new trace",
        "scope/siggen mux callers are not DMM runtime range proof",
        "mux writer body guard", "gpio_pc12_pe_write_block",
        "gpio_pa15_pb11_pb10_write_block", "DAC1/scope calibration tail",
        "Mux Writer Scope-Tail Guard", "full_decompile.c:2274..2293",
        "full_decompile.c:2375..2392", "DAT_200000fc + 100",
        "DAT_200000fd + 100", "_DAT_40001c34",
        "scope threshold/calibration context",
        "not missing DMM runtime range state",
        "Stock Mux Arm Truth Table Guard",
        "fpga_meter_mux_gpio_state_for_stock_mux_arms",
        "whole switch-arm truth table",
        "ten switch arms (`0..9`)",
        "Recovered stock switch arm, not mapped to a local DMM selector",
        "Arms `8` and `9` are important negative/unfinished evidence",
        "recover a stock DMM-owned writer or trace that selects a specific arm",
        "Mux GPIO State Projection Guard", "fpga_meter_mux_gpio_state_for_submode",
        "Final projected levels", "Stock slot 0 projection",
        "Local split, no extra stock slot recovered",
        "production DMM frontend apply path now",
        "tested state-machine table and hardware-facing writes cannot",
        "runtime mux-state writer guard", "0x08001EE8", "0x0801A526",
        "mux-state RAM-map boundary", "DAT_200000fa (25 refs)",
        "DAT_200000fb (11 refs)", "function-level refs",
        "negative DMM evidence", "scope snapshot consumer guard",
        "mux-state full-decompile surface guard",
        "DAT_200000fa` references and all 10 `DAT_200000fb` references",
        "only decompile-visible indexed",
        "DAT_200000fa/DAT_200000fb selected",
        "no literal direct assignment",
        "literal or aliased write",
        "mux-state pair-write context guard",
        "full_decompile.c:2564..2573",
        "full_decompile.c:8745..8753",
        "command `4` enqueue",
        "scope-submode mux call guard", "0x0801C7B8", "0x0801D088",
        "DAT_20000128", "state[0x30]", "scope runtime reconfiguration",
        "0x08034078", "consumer/snapshot path, not a DMM mux writer",
        "scope/preset mux owner guard", "0x08003148", "0x08003900",
        "not DMM runtime range proof",
        "scope UI mux-LUT consumer guard", "0x080151B0", "0x080151C2",
        "FUN_08015f50", "scope render/scale consumer",
        "watchdog reload state boundary guard", "0x08039038",
        "DAT_2000105a", "IWDG_RLR", "meter_state + 0xf62",
        "not DMM ms[0x02]/ms[0x03]",
        "scope measurement-engine mux-pointer consumer context guard",
        "full_decompile.c:11411..11491",
        "forbids local pointer-write forms",
        "read-only scope measurement/scale math",
        "Scope Trigger Overlay 105B Boundary", "DAT_2000105B", "0x080BB40C",
        "FUN_08021B40", "scope_draw_trigger_overlay",
        "zero-filled app-slot shadow", "not DMM range proof",
        "ACV format selector guard", "0x080371C8", "0x08037228",
        "frame[7] bit 0", "not AC evidence", "is_ac status-bit mirror",
        "frame[7].2", "ACV/ACA auto confidence",
        "autoscan AC confidence", "`45..65` Hz", "diagnostic only",
        "scope mux-state consumer guard", "0x0801D2EC", "0x0801D8B8",
        "0x0801F51E", "0x0801F5FC", "0x0801FD66", "0x0801EFC0",
        "0x0801F6F8", "remaining RAM-map consumers", "not DMM range proof",
        "Magnitude-Derived Range Feedback Boundary", "BCD overflow/underflow",
        "metadata-driven", "frame[8].7", "frame[3].4", "frame[4].4",
        "frame[5].4", "not a value-shape classifier",
        "magnitude-derived relay/range control",
        "DMM Evidence / Gap Ledger", "Current Low-DCV Blocker",
        "Per-Path Evidence Status", "stock-visible decode: digits=4366",
        "Do not promote",
        "this visual mismatch into a decoder coefficient",
        "DMM-owned runtime writer or trace for `DAT_200000fa`/`DAT_200000fb`",
        "no recovered FPGA ACK/apply",
        "status or DMM calibration effect",
        "Mux-state xref closure",
        "25 RAM-map refs / 26 full-decompile refs",
        "11 RAM-map refs",
        "10 full-decompile refs",
        "full_decompile.c:2566",
        "full_decompile.c:8745",
        "classified and guarded as scope/siggen autorange",
        "current static mux-state surface is already",
        "PB9=1 PA6=1",
        "earlier flashed local firmware\nstate",
        "not stock PB9/PA6 evidence",
        "current-head\nstock-boundary policy keeps PB9/PA6 low",
        "Do not treat PB9/PA6 high as a low-DCV\ncorrection",
        "classified as negative DMM evidence",
        "not progress unless a new writer, xref owner, or trace is recovered",
        "Saved-config default boundary",
        "default persistent mux bytes are `ms[0x02]=5` and `ms[0x03]=5`",
        "persistence/default evidence only",
        "not a universal frontend setting",
        "Next RE Target", "multiple DCV points, including low DCV, 5 V, and 32 V",
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
        report["ui_submode_surface_contract"] = verify_ui_submode_surface_contract()
        report["live_validation_safety_contract"] = verify_live_validation_safety_contract()
        report["re_comment_coverage"] = verify_re_coverage()
        report["no_unrecovered_meter_coefficients"] = verify_no_unrecovered_meter_coefficients()
        report["no_ocr_pipeline"] = verify_no_ocr_pipeline()
        report["ac_status_boundary"] = verify_ac_status_boundary()
        report["h2_tx_only_boundary"] = verify_h2_tx_only_boundary()
        report["meter_aux_afe_pin_policy"] = verify_meter_aux_afe_pin_policy()
        report["meter_expected_selector_plan_word"] = verify_meter_expected_selector_uses_plan_word()
        report["meter_sequence_tail_transition_plan"] = verify_meter_sequence_tail_uses_transition_plan()
        report["meter_transition_production_contract"] = verify_meter_transition_production_contract()
        report["meter_apply_pair_production_comment"] = verify_meter_apply_pair_production_comment()
        report["legacy_meter_fsm_unit_lookup_boundary"] = (
            verify_legacy_meter_fsm_unit_lookup_boundary()
        )
        report["legacy_meter_fsm_range_command_boundary"] = (
            verify_legacy_meter_fsm_range_command_boundary()
        )
        report["legacy_top_level_dmm_protocol_boundaries"] = (
            verify_legacy_top_level_dmm_protocol_boundaries()
        )
        report["no_magnitude_range_feedback"] = verify_no_magnitude_range_feedback()

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
