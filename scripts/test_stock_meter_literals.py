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
    print(f"stock meter selector table: {selector['bytes']}")
    print("stock meter literal pools: ok")


if __name__ == "__main__":
    main()
