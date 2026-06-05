#!/usr/bin/env python3
"""Verify stock V1.2.0 DMM literal pools used by the multiplier notes."""

from __future__ import annotations

import hashlib
import math
import struct
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
BIN = REPO / "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE = 0x08000000
EXPECTED_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
EXPECTED_METER_SELECTOR_TABLE = bytes.fromhex("14 0c 17 0b 0a 12 11 10")
EXPECTED_METER_SELECTOR_XREF_SEQUENCES = {
    0x080042E2: bytes.fromhex(
        "95 f8 2d 0f 4b f2 fc 32 41 1e 00 28 08 bf 07 21 "
        "c8 b2 c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 "
        "00 f5 a0 60 c2 f2 00 01 08 80"
    ),
    0x080048BA: bytes.fromhex(
        "95 f8 2d 0f 00 21 4b f2 fc 32 07 28 38 bf 41 1c "
        "c8 b2 c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 "
        "00 f5 a0 60 c2 f2 00 01 08 80"
    ),
}
EXPECTED_METER_SELECTOR_STATE_SEQUENCES = {
    "init_selector_reset": (
        0x08026FDE,
        bytes.fromhex(
            "00 20 aa f8 35 1f ff 21 8a f8 5d 0f "
            "8a f8 2d 0f 8a f8 2f 0f 8a f8 38 1f aa f8 3c 0f"
        ),
    ),
    "rx_force_mode_8": (
        0x08036D14,
        bytes.fromhex(
            "40 f2 f8 07 c2 f2 00 07 04 20 87 f8 35 0f "
            "08 20 00 24 87 f8 2d 0f c7 f8 30 4f"
        ),
    ),
    "rx_force_mode_1": (
        0x08036D50,
        bytes.fromhex("b1 20 86 f8 5d 0f 01 20 86 f8 2d 0f"),
    ),
    "rx_shadow_zero": (
        0x08037220,
        bytes.fromhex("00 20 87 f8 36 0f"),
    ),
    "rx_shadow_extended": (
        0x080372E0,
        bytes.fromhex(
            "49 07 4f f0 00 00 4f f0 02 01 87 f8 36 0f "
            "b2 79 58 bf c2 f3 80 11 87 f8 37 1f 03 21 87 f8 2f 1f"
        ),
    ),
    "rx_shadow_one": (
        0x08037328,
        bytes.fromhex("01 22 87 f8 36 2f"),
    ),
    "rx_shadow_two": (
        0x08037338,
        bytes.fromhex(
            "02 20 87 f8 36 0f 01 22 22 ea 01 00 "
            "87 f8 37 0f 87 f8 2f 2f"
        ),
    ),
    "rx_shadow_two_with_frame_bit": (
        0x080373A8,
        bytes.fromhex(
            "49 07 4f f0 02 02 b0 79 4f f0 02 01 87 f8 36 2f "
            "58 bf c0 f3 80 11 87 f8 37 1f 87 f8 2f 2f"
        ),
    ),
}
EXPECTED_MUX_RESTORE_SEQUENCES = {
    0x08025544: bytes.fromhex("a0 78 dc f7 ad f9 e0 78 dc f7 84 fa"),
    0x0802723E: bytes.fromhex(
        "9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc"
    ),
}
EXPECTED_MUX_CALLS_BY_TARGET = {
    "gpio_mux_portc_porte": {
        "target": 0x080018A4,
        "calls": {
            0x080020B2: bytes.fromhex("ff f7 f7 fb"),
            0x080031E8: bytes.fromhex("fe f7 5c fb"),
            0x080039A2: bytes.fromhex("fd f7 7f ff"),
            0x0801A53E: bytes.fromhex("e7 f7 b1 f9"),
            0x0801C7CC: bytes.fromhex("e5 f7 6a f8"),
            0x0801D094: bytes.fromhex("e4 f7 06 fc"),
            0x08025546: bytes.fromhex("dc f7 ad f9"),
            0x08027242: bytes.fromhex("da f7 2f fb"),
        },
    },
    "gpio_mux_porta_portb": {
        "target": 0x08001A58,
        "calls": {
            0x08001F06: bytes.fromhex("ff f7 a7 fd"),
            0x08003644: bytes.fromhex("fe f7 08 fa"),
            0x08003E3A: bytes.fromhex("fd f7 0d fe"),
            0x0801A534: bytes.fromhex("e7 f7 90 fa"),
            0x0801C7D8: bytes.fromhex("e5 f7 3e f9"),
            0x0801D0A0: bytes.fromhex("e4 f7 da fc"),
            0x0802554C: bytes.fromhex("dc f7 84 fa"),
            0x0802724A: bytes.fromhex("da f7 05 fc"),
        },
    },
}


def read(addr: int, size: int) -> bytes:
    data = BIN.read_bytes()
    off = addr - BASE
    return data[off : off + size]


def expect_f64(addr: int, expected: float) -> None:
    actual = struct.unpack("<d", read(addr, 8))[0]
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.0):
        raise AssertionError(f"{addr:#010x}: expected f64 {expected}, got {actual}")


def expect_f32(addr: int, expected: float) -> None:
    actual = struct.unpack("<f", read(addr, 4))[0]
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.0):
        raise AssertionError(f"{addr:#010x}: expected f32 {expected}, got {actual}")


def expect_bytes(addr: int, expected_hex: str) -> None:
    expected = bytes.fromhex(expected_hex)
    actual = read(addr, len(expected))
    if actual != expected:
        raise AssertionError(
            f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
        )


def verify_meter_selector_table() -> dict[str, object]:
    """Check the stock eight-entry DMM 0x05xx selector low-byte table.

    The decompile notes refer to the app-slot runtime literal at 0x080BB3FC.
    This archived app image is read with base 0x08000000, so the same bytes are
    at 0x080B43FC / file offset 0x000B43FC after the documented app-slot
    correction.  Keep this binary-grounded so local DMM submode policy cannot
    drift into invented selector bytes.
    """
    runtime_addr = 0x080BB3FC
    app_image_addr = 0x080B43FC
    actual = read(app_image_addr, len(EXPECTED_METER_SELECTOR_TABLE))
    if actual != EXPECTED_METER_SELECTOR_TABLE:
        raise AssertionError(
            f"{runtime_addr:#010x}/file {app_image_addr:#010x}: "
            f"expected {EXPECTED_METER_SELECTOR_TABLE.hex(' ')}, got {actual.hex(' ')}"
        )
    return {
        "runtime_addr": runtime_addr,
        "app_image_addr": app_image_addr,
        "bytes": actual.hex(" "),
        "words": [f"0x05{b:02X}" for b in actual],
    }


def verify_meter_selector_xref_sequences() -> dict[str, object]:
    """Check stock code paths that consume the eight-entry selector table.

    The two guarded sequences are the decrement/increment selector branches
    documented in `high_flash_scope_indexing_2026_04_08.md`.  They load the
    runtime literal `0x080BB3FC`, read one table byte indexed by
    `DAT_20001025`, add `0x0500`, and store the raw UART halfword to
    `0x20002D54` before queueing it through `0x20002D74`.

    This proves the raw selector-table consumer path only.  It does not recover
    the analog mux bytes `ms[0x02]`/`ms[0x03]` or any physical calibration.
    """
    checked: dict[str, str] = {}
    for addr, expected in EXPECTED_METER_SELECTOR_XREF_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
            )
        checked[f"{addr:#010x}"] = actual.hex(" ")
    return {"sequences": checked}


def verify_meter_selector_state_sequences() -> dict[str, object]:
    """Check stock selector/shadow-state writers used by the DMM FSM.

    These sequences prove RAM-state coupling around `DAT_20001025` and adjacent
    formatter bytes: init clears the selector, RX classification can force mode
    8 or mode 1, and later RX branches update `DAT_2000102E`/`DAT_2000102F`/
    `DAT_20001027` before the display formatter consumes them.

    This is still digital DMM FSM evidence only.  It does not recover the
    analog mux bytes `ms[0x02]`/`ms[0x03]` or any factory calibration source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SELECTOR_STATE_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_meter_mux_restore_sequences() -> dict[str, object]:
    """Check stock saved-state ms[0x02]/ms[0x03] mux apply call sites.

    These instruction bytes are the binary evidence behind
    `meter_mode_command_table_2026_06_05.md`: master init loads saved
    meter-state byte 2, calls `gpio_mux_portc_porte`, then loads saved byte 3
    and calls `gpio_mux_porta_portb`.  This proves boot/saved-state apply
    evidence only; it does not prove a runtime DMM range writer.
    """
    checked: dict[str, str] = {}
    for addr, expected in EXPECTED_MUX_RESTORE_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
            )
        checked[f"{addr:#010x}"] = actual.hex(" ")
    return {"sequences": checked}


def verify_meter_mux_callsite_sequences() -> dict[str, object]:
    """Check every stock direct BL callsite to the two mux writers.

    This is a binary callsite guard, not a DMM mode/range proof.  The stock
    decompile exposes scope/siggen runtime callers plus DMM-relevant boot and
    saved-state apply sites.  Keeping the complete direct BL list guarded makes
    it harder to mistake partial xrefs for recovered DMM runtime mux writers.
    """
    checked: dict[str, object] = {}
    for name, info in EXPECTED_MUX_CALLS_BY_TARGET.items():
        calls: dict[int, bytes] = info["calls"]  # type: ignore[assignment]
        target = int(info["target"])
        call_bytes: dict[str, str] = {}
        for addr, expected in calls.items():
            actual = read(addr, len(expected))
            if actual != expected:
                raise AssertionError(
                    f"{name} call {addr:#010x}: "
                    f"expected {expected.hex(' ')}, got {actual.hex(' ')}"
                )
            call_bytes[f"{addr:#010x}"] = actual.hex(" ")
        checked[name] = {
            "target": f"{target:#010x}",
            "calls": sorted(call_bytes),
            "sequences": call_bytes,
        }
    return checked


def main() -> None:
    if not BIN.exists():
        print(f"stock meter literal pools: skipped; missing {BIN}", file=sys.stderr)
        return

    digest = hashlib.sha256(BIN.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise AssertionError(f"{BIN}: expected sha256 {EXPECTED_SHA256}, got {digest}")

    for addr, value in (
        (0x08036C60, 1000.0),
        (0x08036C68, 0.5),
        (0x08036C70, 0.0),
        (0x08036C78, 100.0),
        (0x08036C80, 10.0),
        (0x08036C90, 4.0),
        (0x080373D0, 3.0),
        (0x080373D8, 2.0),
        (0x080373E0, 1.0),
        (0x080373E8, 0.0),
        (0x08002BF0, 1000.0),
        (0x08002BF8, 0.0),
        (0x08002C00, 10000.0),
    ):
        expect_f64(addr, value)

    for addr, value in (
        (0x08036C88, 10000.0),
        (0x080373F0, 32.0),
        (0x08002C08, 1000.0),
        (0x08002C0C, 100.0),
    ):
        expect_f32(addr, value)

    expect_bytes(0x08036C8C, "00 bf 00 bf")
    selector = verify_meter_selector_table()
    selector_xrefs = verify_meter_selector_xref_sequences()
    selector_state = verify_meter_selector_state_sequences()
    mux_restore = verify_meter_mux_restore_sequences()
    mux_calls = verify_meter_mux_callsite_sequences()
    print(f"stock meter selector table: {selector['bytes']}")
    print("stock meter selector xref sites: " +
          ", ".join(selector_xrefs["sequences"].keys()))
    print("stock meter selector state sites: " +
          ", ".join(item["addr"] for item in selector_state["sequences"].values()))
    print("stock meter mux restore sites: " +
          ", ".join(mux_restore["sequences"].keys()))
    for name, info in mux_calls.items():
        print(f"stock {name} direct BL sites: " + ", ".join(info["calls"]))
    print("stock meter literal pools: ok")


if __name__ == "__main__":
    main()
