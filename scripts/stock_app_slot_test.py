#!/usr/bin/env python3
"""
Helpers for reversible stock-app-slot experiments on the OpenScope 2C53T.

This keeps the permanent bootloader intact and only swaps the application
image at 0x08004000 using the existing HID flash path.
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
HID_FLASH = ROOT / "scripts" / "hid_flash.py"
DEFAULT_STOCK = ROOT / "archive" / "2C53T Firmware V1.2.0" / "APP_2C53T_V1.2.0_251015.bin"
DEFAULT_CURRENT = ROOT / "firmware" / "build" / "firmware.bin"
DEFAULT_RESTORE = ROOT / "firmware" / "build" / "stock_test_restore.bin"

APP_ADDRESS = 0x08004000
FLASH_MIN = 0x08000000
FLASH_MAX = 0x08100000
RAM_MASK = 0xFFF00000
RAM_BASE = 0x20000000


def read_vectors(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 8:
        raise ValueError(f"{path} is too small to contain vectors")
    return struct.unpack("<II", data[:8])


def validate_app_image(path: Path) -> tuple[int, int]:
    if not path.exists():
        raise FileNotFoundError(path)

    sp, rv = read_vectors(path)
    errors: list[str] = []

    if (sp & RAM_MASK) != RAM_BASE:
        errors.append(f"stack pointer 0x{sp:08X} is not in SRAM")
    if not (FLASH_MIN <= rv < FLASH_MAX):
        errors.append(f"reset vector 0x{rv:08X} is not in flash")
    if rv < APP_ADDRESS:
        errors.append(
            f"reset vector 0x{rv:08X} is below app slot 0x{APP_ADDRESS:08X}"
        )

    if errors:
        joined = "; ".join(errors)
        raise ValueError(f"{path} does not look like a valid app-slot image: {joined}")

    return sp, rv


def print_image_summary(label: str, path: Path) -> None:
    sp, rv = validate_app_image(path)
    size = path.stat().st_size
    print(f"{label}:")
    print(f"  path: {path}")
    print(f"  size: {size} bytes")
    print(f"  stack pointer: 0x{sp:08X}")
    print(f"  reset vector:  0x{rv:08X}")


def save_restore_image(source: Path, dest: Path) -> None:
    validate_app_image(source)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
    print(f"Saved restore image: {dest}")


def run_hid_flash(image: Path, *, no_jump: bool = False) -> None:
    validate_app_image(image)
    cmd = ["uv", "run", str(HID_FLASH), str(image)]
    if no_jump:
        cmd.append("--no-jump")
    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def cmd_inspect(args: argparse.Namespace) -> int:
    print_image_summary("Stock app", args.stock)
    if args.current.exists():
        print_image_summary("Current app", args.current)
    else:
        print(f"Current app:\n  path: {args.current}\n  missing")
    return 0


def cmd_save_restore(args: argparse.Namespace) -> int:
    save_restore_image(args.current, args.restore)
    return 0


def cmd_flash_stock(args: argparse.Namespace) -> int:
    print_image_summary("Stock app", args.stock)
    if args.save_restore:
        save_restore_image(args.current, args.restore)
    run_hid_flash(args.stock, no_jump=args.no_jump)
    return 0


def cmd_restore(args: argparse.Namespace) -> int:
    print_image_summary("Restore app", args.restore)
    run_hid_flash(args.restore, no_jump=args.no_jump)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Reversible stock-app-slot helper for OpenScope 2C53T"
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    inspect_p = sub.add_parser("inspect", help="Validate stock/current app images")
    inspect_p.add_argument("--stock", type=Path, default=DEFAULT_STOCK)
    inspect_p.add_argument("--current", type=Path, default=DEFAULT_CURRENT)
    inspect_p.set_defaults(func=cmd_inspect)

    save_p = sub.add_parser("save-restore", help="Save current app as restore image")
    save_p.add_argument("--current", type=Path, default=DEFAULT_CURRENT)
    save_p.add_argument("--restore", type=Path, default=DEFAULT_RESTORE)
    save_p.set_defaults(func=cmd_save_restore)

    flash_p = sub.add_parser("flash-stock", help="Flash stock app into app slot")
    flash_p.add_argument("--stock", type=Path, default=DEFAULT_STOCK)
    flash_p.add_argument("--current", type=Path, default=DEFAULT_CURRENT)
    flash_p.add_argument("--restore", type=Path, default=DEFAULT_RESTORE)
    flash_p.add_argument("--save-restore", action="store_true", default=True)
    flash_p.add_argument("--no-jump", action="store_true")
    flash_p.set_defaults(func=cmd_flash_stock)

    restore_p = sub.add_parser("restore", help="Restore saved custom app")
    restore_p.add_argument("--restore", type=Path, default=DEFAULT_RESTORE)
    restore_p.add_argument("--no-jump", action="store_true")
    restore_p.set_defaults(func=cmd_restore)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
