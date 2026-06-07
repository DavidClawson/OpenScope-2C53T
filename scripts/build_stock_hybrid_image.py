#!/usr/bin/env python3
"""Build the stock-user image used by the PC firmware switcher.

The archived APP_2C53T V1.2.0 image is linked for 0x08007000: its reset
vector is 0x08007311 and stock later writes VTOR=0x08007000.  The switcher
therefore keeps only a low reset vector at 0x08000000, places the untouched
stock APP bytes at 0x08007000, and jumps through a tiny high-flash dispatcher at
0x080E0000.  The high recovery HID bootloader remains at 0x080F0000.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_STOCK = ROOT / "archive" / "2C53T Firmware V1.2.0" / "APP_2C53T_V1.2.0_251015.bin"
DEFAULT_STOCK_DISPATCHER = ROOT / "firmware" / "stock_dispatcher" / "build_user" / "stock_dispatcher.bin"
DEFAULT_OUT = ROOT / "firmware" / "build" / "stock_user_dispatcher.bin"

FLASH_BASE = 0x08000000
STOCK_APP_ADDRESS = 0x08007000
STOCK_APP_OFFSET = STOCK_APP_ADDRESS - FLASH_BASE
HIGH_DISPATCHER_ADDRESS = 0x080E0000
HIGH_DISPATCHER_OFFSET = HIGH_DISPATCHER_ADDRESS - FLASH_BASE
HIGH_RECOVERY_ADDRESS = 0x080F0000
FLASH_MAX = 0x08100000

EXPECTED_STOCK_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
EXPECTED_STOCK_SP = 0x20036F90
EXPECTED_STOCK_RV = 0x08007311
STOCK_RUNTIME_TABLE_CHECK_OFFSET = 0x00033F7C
STOCK_END_CHECK_OFFSET = 0x000B7670


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def word(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def validate_stock(stock: bytes) -> list[str]:
    errors: list[str] = []
    if sha256(stock) != EXPECTED_STOCK_SHA256:
        errors.append(f"stock sha256 drifted: {sha256(stock)}")
    if len(stock) <= STOCK_END_CHECK_OFFSET + 4:
        errors.append("stock image is truncated")
        return errors
    sp = word(stock, 0)
    rv = word(stock, 4)
    if sp != EXPECTED_STOCK_SP:
        errors.append(f"unexpected stock stack pointer 0x{sp:08X}")
    if rv != EXPECTED_STOCK_RV:
        errors.append(f"unexpected stock reset vector 0x{rv:08X}")
    if word(stock, STOCK_RUNTIME_TABLE_CHECK_OFFSET) != 0x008F008F:
        errors.append("stock runtime table anchor drifted")
    if word(stock, STOCK_END_CHECK_OFFSET) != 0x3F027302:
        errors.append("stock tail anchor drifted")
    return errors


def validate_dispatcher(dispatcher: bytes) -> list[str]:
    errors: list[str] = []
    if len(dispatcher) < 8:
        return ["stock dispatcher image is too small"]
    sp = word(dispatcher, 0)
    rv = word(dispatcher, 4)
    if (sp & 0xFFF00000) != 0x20000000:
        errors.append(f"stock dispatcher SP 0x{sp:08X} is not SRAM")
    if not (HIGH_DISPATCHER_ADDRESS <= rv < HIGH_RECOVERY_ADDRESS):
        errors.append(f"stock dispatcher reset vector 0x{rv:08X} is not in dispatcher flash")
    if HIGH_DISPATCHER_OFFSET + len(dispatcher) > HIGH_RECOVERY_ADDRESS - FLASH_BASE:
        errors.append("stock dispatcher would overlap high recovery bootloader")
    return errors


def build(stock: bytes, dispatcher: bytes) -> tuple[bytes, list[str]]:
    errors = validate_stock(stock) + validate_dispatcher(dispatcher)
    worst_len = max(STOCK_APP_OFFSET + len(stock), HIGH_DISPATCHER_OFFSET + len(dispatcher))
    if FLASH_BASE + worst_len > HIGH_RECOVERY_ADDRESS:
        errors.append("stock user image would overlap high recovery bootloader")
    if FLASH_BASE + worst_len > FLASH_MAX:
        errors.append("stock user image would exceed internal flash")
    if errors:
        raise SystemExit("stock user image rejected:\n  - " + "\n  - ".join(errors))

    dispatcher_rv = word(dispatcher, 4)
    image = bytearray(b"\xFF" * worst_len)
    image[0:4] = stock[0:4]
    image[4:8] = dispatcher_rv.to_bytes(4, "little")
    image[STOCK_APP_OFFSET:STOCK_APP_OFFSET + len(stock)] = stock
    image[HIGH_DISPATCHER_OFFSET:HIGH_DISPATCHER_OFFSET + len(dispatcher)] = dispatcher
    return bytes(image), [
        f"0x08000004: low vector -> 0x{dispatcher_rv:08X}",
        f"0x{STOCK_APP_ADDRESS:08X}: stock APP bytes ({len(stock)} bytes)",
        f"0x{HIGH_DISPATCHER_ADDRESS:08X}: stock launcher ({len(dispatcher)} bytes)",
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Build stock-user launcher image")
    parser.add_argument("--stock", type=Path, default=DEFAULT_STOCK)
    parser.add_argument("--stock-dispatcher", type=Path, default=DEFAULT_STOCK_DISPATCHER)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    stock = args.stock.read_bytes()
    dispatcher = args.stock_dispatcher.read_bytes()
    image, layout = build(stock, dispatcher)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(image)

    print(f"stock:  {args.stock}")
    print(f"launcher: {args.stock_dispatcher}")
    print(f"out:    {args.out}")
    print(f"size:   {len(image)} bytes")
    print(f"sha256: {sha256(image)}")
    print("layout: low vector at 0x08000000, stock APP at 0x08007000, high launcher at 0x080E0000")
    for item in layout:
        print(f"stock_layout: {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
