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
    """Fail on invented coefficient paths, not on missing explanatory prose."""
    forbidden = [
        "METER_CAL_LOW_OHM_FACTOR",
        "0.0304f",
        "bench-unit stand-in",
        "3:/System file/cal_ch1.bin",
        "3:/System file/cal_ch2.bin",
        "apply_stock_dcv_voltage_multiplier",
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
        raise GateError(
            "unrecovered meter calibration/formatter boundary drifted: "
            f"forbidden_hits={hits}"
        )
    return {
        "checked": checked,
        "forbidden": forbidden,
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
    """Forbid stale AC/DC status-bit claims without requiring exact prose."""
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
    hits: list[str] = []
    for rel in checked:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        lowered = text.lower()
        for needle in forbidden:
            if needle.lower() in lowered:
                hits.append(f"{rel}: {needle}")
    if hits:
        raise GateError(
            "AC status boundary drifted: "
            f"forbidden_hits={hits}"
        )
    return {"checked": checked, "forbidden": forbidden}


def verify_h2_tx_only_boundary() -> dict[str, Any]:
    """Forbid stale H2 acceptance/choreography claims.

    Exact stock H2 bytes and CS chronology are checked in
    scripts/test_stock_h2_table.py.  This validator only keeps the dangerous
    old claims from re-entering docs/code as hard acceptance evidence.
    """
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
        "firmware/SPI3_HANDSHAKE_BYTE_ACCURATE.md": [
            "flash address `0x08051D1B`",
            "0x0802AC98",
            "The post-upload sends 0x3A between the first CS deassert",
            "0x3A   post-upload (after CS deassert)",
            "continuous stream \u2014 NO CS toggles",
            "continuous stream, no CS toggles",
            "0x0802ADCE",
            "0x0802ADC6",
        ],
        "firmware/src/drivers/usb_debug.c": [
            "spi3 h2verify                   Re-upload H2 + capture FPGA " + "responses",
            "H2 Upload " + "Verification",
            "accepting the data, we might see non-FF " + "responses (ACK bytes",
        ],
    }

    checked: list[str] = []
    stale: list[str] = []
    for rel, snippets in forbidden.items():
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        checked.append(rel)
        for snippet in snippets:
            if snippet in text:
                stale.append(f"{rel}: {snippet}")
    if stale:
        raise GateError(f"H2 TX-only boundary drifted: stale={stale}")
    return {"checked": checked, "forbidden": forbidden}


def verify_spi3_pc6_enable_order() -> dict[str, Any]:
    """Ensure FPGA SPI enable PC6 is raised only after stock-like SPI3 enable."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")

    start = text.find("void fpga_init(void)")
    end = text.find("systick_delay_ms(100);", start)
    if start < 0 or end < 0:
        raise GateError("could not locate fpga_init pre-H2 SPI3 init block")
    body = text[start:end]

    order = [
        "crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);",
        "SPI3_CS_DEASSERT();",
        "FPGA_SPI->ctrl1 =",
        "FPGA_SPI->ctrl2 = 0x03;",
        "FPGA_SPI->ctrl1 |= (1 << 6) /* SPE */;",
        "NVIC_EnableIRQ(SPI3_I2S3EXT_IRQn);",
        "GPIOC->scr = PC6_MASK;",
    ]
    missing = [snippet for snippet in order if snippet not in body]
    positions = [body.find(snippet) for snippet in order if snippet in body]
    if missing or positions != sorted(positions):
        raise GateError(
            "SPI3/PC6 stock-order guard drifted: "
            f"missing={missing} positions={positions}"
        )

    forbidden_early = "GPIOC->scr = PC6_MASK;"
    early_region = body[:body.find("FPGA_SPI->ctrl1 |= (1 << 6) /* SPE */;")]
    if forbidden_early in early_region:
        raise GateError("PC6 is raised before SPI3 SPE in fpga_init")

    return {
        "checked": rel,
        "ordered": order,
        "classification": (
            "PC6 FPGA SPI enable follows stock order: SPI3 register setup, "
            "SPE, IRQ enable, then PC6 HIGH before the 100 ms pre-H2 delay"
        ),
    }


def verify_meter_transition_usart_gate() -> dict[str, Any]:
    """Ensure DMM transition drain gates USART2 UEN like stock runtime switch."""
    rel = "firmware/src/drivers/fpga.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"static void fpga_meter_reset_transport\(void\)"
        r"(?P<body>[\s\S]*?)\n\n/\*",
        text,
    )
    if match is None:
        raise GateError("could not locate fpga_meter_reset_transport body")
    body = match.group("body")

    required_order = [
        "ctrl1 = USART2->ctrl1;",
        "USART2->ctrl1 = ctrl1 & ~(USART_CTRL1_UEN |",
        "if (tx_task_handle != NULL) vTaskSuspend(tx_task_handle);",
        "if (usart_tx_queue != NULL) xQueueReset(usart_tx_queue);",
        "GPIOC->clr = (1U << 11);",
        "USART2->ctrl1 = (ctrl1 | USART_CTRL1_UEN | USART_CTRL1_RDBFIEN) &",
        "if (rx_task_handle != NULL) vTaskResume(rx_task_handle);",
        "if (tx_task_handle != NULL) vTaskResume(tx_task_handle);",
    ]
    missing = [snippet for snippet in required_order if snippet not in body]
    positions = [body.find(snippet) for snippet in required_order if snippet in body]
    if missing or positions != sorted(positions):
        raise GateError(
            "DMM transition USART gate drifted: "
            f"missing={missing} positions={positions}"
        )

    stale = "USART2->ctrl1 = ctrl1 & ~(USART_CTRL1_RDBFIEN | USART_CTRL1_TDBEIEN);"
    if stale in body:
        raise GateError("DMM transition still leaves USART2 UEN enabled during drain")

    return {"checked": rel, "ordered": required_order}


def verify_pc4_post_h2_boundary() -> dict[str, Any]:
    """Keep the PC4 boundary out of firmware until stock evidence warrants it."""
    forbidden = {
        "firmware/src/drivers/fpga.c": [
            "GPIOC->scr = (1U << 4)",
            "GPIOC->clr = (1U << 4)",
            "PC4_MASK",
        ],
    }

    stale: list[str] = []
    for rel, snippets in forbidden.items():
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for snippet in snippets:
            if snippet in text:
                stale.append(f"{rel}: {snippet}")
    if stale:
        raise GateError(
            "PC4 post-H2 boundary drifted: "
            f"stale={stale}"
        )
    return {"checked": list(forbidden), "forbidden": forbidden}


def verify_spi3_case8_readback_boundary() -> dict[str, Any]:
    """Reject overclaims that SPI3 case-8 readback proves DMM calibration."""
    forbidden = {
        "firmware/src/drivers/usb_debug.c": [
            "case-8 DMM multiplier",
            "case-8 DMM range writer",
            "case-8 H2 ACK",
            "case-8 calibration proof",
        ],
        "reverse_engineering/analysis_v120/dmm_evidence_gap_ledger_2026_06_06.md": [
            "case-8 DMM multiplier",
            "case-8 DMM range writer",
            "case-8 H2 ACK",
            "case-8 calibration proof",
        ],
    }

    hits: list[str] = []
    for rel, snippets in forbidden.items():
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        for snippet in snippets:
            if snippet in text:
                hits.append(f"{rel}: {snippet}")
    if hits:
        raise GateError(
            "SPI3 case-8 readback overclaim found: "
            f"hits={hits}"
        )
    return {"checked": list(forbidden), "forbidden": forbidden}


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
        "static void fpga_apply_meter_mux_gpio_state",
        "fpga_gpio_write_level(GPIOB, (1U << 9), state->pb9);",
        "fpga_gpio_write_level(GPIOA, (1U << 6), state->pa6);",
        "fpga_set_meter_frontend_for_submode(0);",
    ]
    missing_file = [snippet for snippet in required_file if snippet not in text]
    init_start = text.find("void fpga_init(void)")
    init_end = text.find("QueueHandle_t fpga_create_tasks(void)", init_start)
    if init_start < 0 or init_end < 0:
        raise GateError("could not locate fpga_init body")
    init_body = text[init_start:init_end]
    boot_frontend = init_body.find("fpga_set_meter_frontend_for_submode(0);")
    boot_activate = init_body.find("usart2_send_cmd(0x05, 0x08);")
    bad_boot_order = (
        boot_frontend < 0 or boot_activate < 0 or boot_frontend > boot_activate
    )
    if missing_body or stale_body or missing_file or bad_boot_order:
        raise GateError(
            "PB9/PA6 auxiliary AFE pin policy drifted: "
            f"missing_body={missing_body} stale_body={stale_body} "
            f"missing_file={missing_file} bad_boot_order={bad_boot_order}"
        )
    return {
        "checked": rel,
        "required_body": required_body,
        "forbidden_body": forbidden_body,
        "required_file": required_file,
        "boot_frontend_before_activation": True,
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


def verify_no_magnitude_range_feedback() -> dict[str, Any]:
    """Ensure production DMM frontend code does not suggest value-shaped ranging."""
    checked: dict[str, str] = {}
    stale: list[str] = []
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

    if stale:
        raise GateError(
            "magnitude-derived range feedback boundary drifted: "
            f"stale={stale}"
        )
    return {
        "checked": list(checked),
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
        "invalid_submode_rejects_every_frame_family_corpus",
        "state_machine_property_matrix_covers_all_submodes",
        "goal_surface_property_enumerates_dmm_state_machine",
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
        "invalid submode rejects the whole frame-family corpus":
            r"test_invalid_submode_rejects_every_frame_family_corpus[\s\S]*"
            r"const uint8_t \*frames\[9\];[\s\S]*"
            r"process_frame\(frames\[i\], 99\);[\s\S]*"
            r"METER_REJECT_INVALID_SUBMODE",
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
    missing_tests = [
        name for name in required_tests
        if f"static int test_{name}" not in text or f"TEST({name});" not in text
    ]
    missing_regexes = [
        name for name, pattern in required_regexes.items()
        if re.search(pattern, text, re.MULTILINE) is None
    ]
    if missing_tests or missing_regexes:
        raise GateError(
            "state-machine property contract check failed: "
            f"missing_tests={missing_tests} "
            f"missing_regexes={missing_regexes}"
        )
    return {
        "file": rel,
        "tests": required_tests,
        "regex_anchors": list(required_regexes),
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
    missing_tests = [
        name for name in required_tests
        if f"static void test_{name}" not in text or f"test_{name}();" not in text
    ]
    missing_regexes = [
        name for name, pattern in required_regexes.items()
        if re.search(pattern, text, re.MULTILINE) is None
    ]
    if missing_tests or missing_regexes:
        raise GateError(
            "transition-plan property contract check failed: "
            f"missing_tests={missing_tests} "
            f"missing_regexes={missing_regexes}"
        )
    return {
        "file": rel,
        "tests": required_tests,
        "regex_anchors": list(required_regexes),
    }


def verify_autoscan_property_contract() -> dict[str, Any]:
    """Anchor auto-selection scoring so AC/current modes need real evidence."""
    rel = "firmware/tests/test_meter_auto.c"
    text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    required_tests = [
        "candidate_order_is_voltage_only_for_live_autoscan",
        "autoscan_does_not_probe_current_or_passive_modes",
        "wrong_submode_never_scores",
        "dirty_frame_family_state_never_scores",
        "dc_voltage_scores_without_frequency_or_nonzero_magnitude",
        "ac_voltage_requires_frequency_evidence",
        "current_auto_scores_respect_ac_evidence",
        "unresolved_microamp_functions_are_not_autoscan_candidates",
        "temperature_scores_as_passive_candidate",
        "continuity_marker_beats_resistance_normal",
    ]
    missing_tests = [
        name for name in required_tests
        if f"static int test_{name}" not in text or f"TEST({name});" not in text
    ]
    if missing_tests:
        raise GateError(
            "autoscan property contract check failed: "
            f"missing_tests={missing_tests}"
        )
    return {
        "file": rel,
        "tests": required_tests,
    }


def compact_contract_result(result: dict[str, Any]) -> dict[str, Any]:
    """Keep the main report focused on executable coverage, not text anchors."""
    compact: dict[str, Any] = {}
    for key in ("file", "checked"):
        if key in result:
            compact[key] = result[key]
    if "tests" in result:
        compact["test_count"] = len(result["tests"])
        compact["tests"] = result["tests"]
    if "regex_anchors" in result:
        compact["regex_anchor_count"] = len(result["regex_anchors"])
    if "code_terms" in result:
        compact["code_term_count"] = len(result["code_terms"])
    return compact


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
            "{ \"DC mA\",       \"mA\"",
            "{ \"DC Current\",  \"A\"",
            "{ \"AC mA\",       \"mA\"",
            "{ \"AC Current\",  \"A\"",
            "{ \"Temperature\", \"C\"",
        ],
        "firmware/src/drivers/meter_auto.c": [
            "static const uint8_t meter_auto_candidate_order[] = {",
            "0, 1",
        ],
    }
    forbidden = {
        "firmware/src/drivers/meter_auto.c": [
            "uA",
            "microamp",
            "11,",
            "2, 4, 3, 5",
            "6, 7, 8, 9, 10",
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

    allowed_mode_commands = ["mode meter 0 0", "mode meter 1 0", "mode meter 0 0"]
    mode_commands = re.findall(r'"(mode meter \d+ 0)"', body)
    forbidden_commands = [
        command for command in mode_commands
        if command not in {"mode meter 0 0", "mode meter 1 0"}
    ]
    bad_sequence = mode_commands != allowed_mode_commands
    if forbidden_commands or bad_sequence:
        raise GateError(
            "live validation safety contract drifted: "
            f"forbidden_commands={forbidden_commands} mode_commands={mode_commands}"
        )
    return {
        "checked": rel,
        "mode_commands": mode_commands,
        "forbidden": "current/passive meter mode commands in energized live validation",
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
        report["no_unrecovered_meter_coefficients"] = verify_no_unrecovered_meter_coefficients()
        report["no_ocr_pipeline"] = verify_no_ocr_pipeline()
        report["ac_status_boundary"] = verify_ac_status_boundary()
        report["h2_tx_only_boundary"] = verify_h2_tx_only_boundary()
        report["spi3_pc6_enable_order"] = verify_spi3_pc6_enable_order()
        report["meter_transition_usart_gate"] = verify_meter_transition_usart_gate()
        report["meter_aux_afe_pin_policy"] = verify_meter_aux_afe_pin_policy()
        report["live_validation_safety_contract"] = verify_live_validation_safety_contract()
        report["no_magnitude_range_feedback"] = verify_no_magnitude_range_feedback()
        if not args.skip_software:
            # These static contract checks are intentionally subordinate to
            # `make -C firmware test-meter` in the software gate above. They
            # name the executable C property/model tests that prove the DMM
            # state-machine matrix is present, but the report is compacted so
            # success does not depend on preserving arbitrary prose snippets.
            report["state_machine_property_contract"] = compact_contract_result(
                verify_state_machine_property_contract()
            )
            report["transition_plan_property_contract"] = compact_contract_result(
                verify_transition_plan_property_contract()
            )
            report["autoscan_property_contract"] = compact_contract_result(
                verify_autoscan_property_contract()
            )
            report["ui_submode_surface_contract"] = compact_contract_result(
                verify_ui_submode_surface_contract()
            )

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
