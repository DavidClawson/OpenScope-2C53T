#!/usr/bin/env python3
"""PC-side firmware switcher for OpenScope and stock mode."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from flash_preflight import (
    APP_ADDRESS,
    APP_SLOT_END_ADDRESS,
    STOCK_SAVED_CONFIG_ADDRESS,
    STOCK_SAVED_CONFIG_END_ADDRESS,
    STOCK_RUNTIME_BASE,
    classify_image,
    padded_len,
    read_file,
    read_vectors,
    sha256,
    stock_false_scatter_candidates,
    validate_stock_boot_chain,
    validate_stock_saved_config_hole,
    validate_vectors,
)


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_STOCK = ROOT / "archive" / "2C53T Firmware V1.2.0" / "APP_2C53T_V1.2.0_251015.bin"
DEFAULT_OPENSCOPE = ROOT / "firmware" / "build" / "firmware.bin"
DEFAULT_STOCK_LAUNCHER = ROOT / "firmware" / "stock_dispatcher" / "build" / "stock_dispatcher.bin"
DEFAULT_STOCK_USER = ROOT / "firmware" / "build" / "stock_user_dispatcher.bin"
HID_FLASH = ROOT / "scripts" / "hid_flash.py"
FLASH_PREFLIGHT = ROOT / "scripts" / "flash_preflight.py"
STOCK_BUILDER = ROOT / "scripts" / "build_stock_hybrid_image.py"
HID_FLASH_CMD = ["uv", "run", str(HID_FLASH)]

EXPECTED_STOCK_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
INTERNAL_FLASH_BYTES = 1024 * 1024
STOCK_APP_ADDRESS = 0x08007000


def run(cmd: list[str], *, cwd: Path = ROOT, dry_run: bool = False) -> int:
    print("+ " + " ".join(cmd))
    if dry_run:
        return 0
    return subprocess.run(cmd, cwd=cwd, check=False).returncode


def describe_image(label: str, path: Path, address: int = APP_ADDRESS) -> tuple[str, str, int]:
    data = read_file(path)
    kind = classify_image(path, data, address)
    sp, rv = read_vectors(data)
    digest = sha256(data)
    print(f"{label}:")
    print(f"  path: {path}")
    print(f"  kind: {kind}")
    print(f"  size: {len(data)} bytes; padded: {padded_len(len(data))} bytes")
    print(f"  sha256: {digest}")
    print(f"  stack pointer: 0x{sp:08X}")
    print(f"  reset vector:  0x{rv:08X}")
    return kind, digest, len(data)


def require_valid_openscope(path: Path) -> int:
    data = read_file(path)
    kind = classify_image(path, data, APP_ADDRESS)
    _sp, _rv, errors = validate_vectors(data, APP_ADDRESS, app_slot=True)
    if kind != "openscope-app":
        errors.append(f"expected an OpenScope app image, got {kind}")
    if APP_ADDRESS + padded_len(len(data)) > APP_SLOT_END_ADDRESS:
        errors.append("padded app image would overlap the high recovery bootloader region")
    errors.extend(validate_stock_saved_config_hole(data, APP_ADDRESS))
    if errors:
        print("OpenScope image rejected:")
        for error in errors:
            print(f"  - {error}")
        return 2
    return 0


def validate_stock_image(path: Path) -> int:
    data = read_file(path)
    kind = classify_image(path, data, APP_ADDRESS)
    sp, rv, errors = validate_vectors(data, APP_ADDRESS, app_slot=True)
    digest = sha256(data)
    print(f"  native-base coverage: 0x{STOCK_RUNTIME_BASE:08X}..0x{STOCK_RUNTIME_BASE + len(data):08X}")
    if kind != "stock-app":
        errors.append(f"expected a stock APP_2C53T image, got {kind}")
    if digest != EXPECTED_STOCK_SHA256:
        errors.append(f"stock image sha256 drifted: {digest}")
    if sp != 0x20036F90:
        errors.append(f"unexpected stock stack pointer 0x{sp:08X}")
    if rv != 0x08007311:
        errors.append(f"unexpected stock reset vector 0x{rv:08X}")
    for index, (src, dst, length, handler) in enumerate(stock_false_scatter_candidates(data)):
        print(
            f"  false-scatter-candidate[{index}]: src=0x{src:08X} dst=0x{dst:08X} "
            f"len=0x{length:X} handler=0x{handler:08X}"
        )
    for warning in validate_stock_boot_chain(data):
        print(f"  boot-chain warning: {warning}")
    if errors:
        print("Stock image rejected:")
        for error in errors:
            print(f"  - {error}")
        return 2
    return 0


def cmd_inspect(args: argparse.Namespace) -> int:
    rc = 0
    if args.stock.exists():
        describe_image("Stock image", args.stock)
    else:
        print(f"Stock image missing: {args.stock}")
        rc = 1
    if args.openscope.exists():
        describe_image("OpenScope image", args.openscope)
    else:
        print(f"OpenScope image missing: {args.openscope}")
        rc = 1

    stock_size = args.stock.stat().st_size if args.stock.exists() else 0
    openscope_size = args.openscope.stat().st_size if args.openscope.exists() else 0
    print("Flash budget:")
    print(f"  internal flash: {INTERNAL_FLASH_BYTES} bytes")
    print(f"  stock + openscope: {stock_size + openscope_size} bytes")
    print(f"  stock + openscope fits: {1 if stock_size + openscope_size <= INTERNAL_FLASH_BYTES else 0}")
    print(f"  free after stock + 16K manager: {INTERNAL_FLASH_BYTES - stock_size - 16 * 1024} bytes")
    return rc


def cmd_openscope(args: argparse.Namespace) -> int:
    if args.build:
        rc = run(["make", "-C", "firmware"], dry_run=args.dry_run)
        if rc:
            return rc
    describe_image("OpenScope image", args.image)
    rc = require_valid_openscope(args.image)
    if rc:
        return rc
    preflight_cmd = [
        sys.executable,
        str(FLASH_PREFLIGHT),
        "hid-app",
        "--image",
        str(args.image),
    ]
    if args.image_only:
        preflight_cmd.append("--image-only")
    rc = run(preflight_cmd, dry_run=args.dry_run)
    if rc or args.preflight_only:
        return rc
    flash_cmd = [
        *HID_FLASH_CMD,
        str(args.image),
        "--preserve-blank-blocks",
        "--preserve-blank-blocks-range",
        f"0x{STOCK_SAVED_CONFIG_ADDRESS:08X}:0x{STOCK_SAVED_CONFIG_END_ADDRESS:08X}",
    ]
    if args.no_jump:
        flash_cmd.append("--no-jump")
    return run(flash_cmd, dry_run=args.dry_run)


def build_stock_user_image(args: argparse.Namespace) -> int:
    rc = run(["make", "-C", "firmware/stock_dispatcher"], dry_run=args.dry_run)
    if rc:
        return rc
    builder_cmd = [
        sys.executable,
        str(STOCK_BUILDER),
        "--stock",
        str(args.image),
        "--stock-dispatcher",
        str(DEFAULT_STOCK_LAUNCHER),
        "--out",
        str(args.out),
    ]
    if args.keep_splash:
        builder_cmd.append("--keep-splash")
    return run(builder_cmd, dry_run=args.dry_run)


def cmd_stock(args: argparse.Namespace) -> int:
    describe_image("Stock image", args.image)
    rc = validate_stock_image(args.image)
    if rc:
        return rc
    rc = build_stock_user_image(args)
    if rc:
        return rc
    print(f"Stock user image: {args.out}")
    if not args.flash:
        return 0

    stock_settings_start = STOCK_APP_ADDRESS + padded_len(args.image.stat().st_size)
    flash_cmd = [
        *HID_FLASH_CMD,
        str(args.out),
        "--address",
        "0x08000000",
        "--allow-low-flash",
        "--allow-unknown-app",
        "--preserve-blank-blocks",
        "--preserve-blank-blocks-range",
        f"0x{STOCK_SAVED_CONFIG_ADDRESS:08X}:0x{STOCK_SAVED_CONFIG_END_ADDRESS:08X}",
        "--preserve-blank-blocks-from",
        f"0x{stock_settings_start:08X}",
    ]
    if args.no_jump:
        flash_cmd.append("--no-jump")
    else:
        flash_cmd.extend(["--run-address", "0x08000000"])
    return run(flash_cmd, dry_run=args.dry_run)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Switch a 2C53T between OpenScope and stock modes")
    sub = parser.add_subparsers(dest="cmd", required=True)

    inspect = sub.add_parser("inspect", help="show image facts and flash budget")
    inspect.add_argument("--stock", type=Path, default=DEFAULT_STOCK)
    inspect.add_argument("--openscope", type=Path, default=DEFAULT_OPENSCOPE)
    inspect.set_defaults(func=cmd_inspect)

    openscope = sub.add_parser("openscope", help="flash/run the guarded OpenScope app through HID IAP")
    openscope.add_argument("--image", type=Path, default=DEFAULT_OPENSCOPE)
    openscope.add_argument("--build", action="store_true", help="build firmware/build/firmware.bin before flashing")
    openscope.add_argument("--image-only", action="store_true", help="run preflight without requiring HID device")
    openscope.add_argument("--preflight-only", action="store_true", help="stop after flash preflight")
    openscope.add_argument("--dry-run", action="store_true", help="print commands but do not execute them")
    openscope.add_argument("--no-jump", action="store_true", help="flash but leave the bootloader running")
    openscope.set_defaults(func=cmd_openscope)

    stock = sub.add_parser("stock", help="build or flash the proven stock-user launcher image")
    stock.add_argument("--image", type=Path, default=DEFAULT_STOCK)
    stock.add_argument("--out", type=Path, default=DEFAULT_STOCK_USER)
    stock.add_argument("--flash", action="store_true", help="flash and run stock through high HID recovery")
    stock.add_argument("--no-jump", action="store_true", help="flash but leave the bootloader running")
    stock.add_argument("--keep-splash", action="store_true", help="do not fast-forward the stock FNIRSI splash")
    stock.add_argument("--dry-run", action="store_true", help="print commands but do not execute them")
    stock.set_defaults(func=cmd_stock)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
