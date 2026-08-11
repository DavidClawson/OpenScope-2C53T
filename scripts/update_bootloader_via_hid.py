#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import flash_preflight


CONFIRM_PHRASE = "UPDATE BOOTLOADER"


def wait_for_hid_iap(timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        modes = flash_preflight.detect_usb_modes()
        if any(pid == flash_preflight.PID_HID_IAP for pid, _mode in modes):
            return True
        time.sleep(0.25)
    return False


def run_preflight(args: argparse.Namespace) -> int:
    cmd = [
        sys.executable,
        str(Path(__file__).resolve().parent / "flash_preflight.py"),
        "hid-bootloader-updater",
        "--bootloader",
        str(args.bootloader),
        "--updater-image",
        str(args.updater_image),
    ]
    if args.image_only:
        cmd.append("--image-only")
    if args.allow_missing_device:
        cmd.append("--allow-missing-device")
    return subprocess.call(cmd)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Flash a bootloader-updater app through the existing HID IAP path. "
            "The updater app rewrites 0x08000000..0x08003FFF after it starts."
        )
    )
    parser.add_argument("--bootloader", type=Path, default=Path("firmware/bootloader/build/bootloader.bin"))
    parser.add_argument(
        "--updater-image",
        type=Path,
        default=Path("firmware/bootloader_updater/build/bootloader_updater.bin"),
    )
    parser.add_argument("--yes", action="store_true", help=f"skip interactive '{CONFIRM_PHRASE}' confirmation")
    parser.add_argument("--image-only", action="store_true", help="run image checks without requiring HID IAP USB mode")
    parser.add_argument("--allow-missing-device", action="store_true")
    parser.add_argument("--timeout", type=float, default=20.0, help="seconds to wait for HID IAP after updater runs")
    args = parser.parse_args()

    rc = run_preflight(args)
    if rc != 0:
        return rc
    if args.image_only:
        return 0

    print()
    print("This will flash an updater app and that app will erase/rewrite the 16KB bootloader region.")
    print("Power loss during that erase/program window can require ROM DFU or SWD recovery.")
    if not args.yes:
        answer = input(f"Type {CONFIRM_PHRASE!r} to continue: ")
        if answer != CONFIRM_PHRASE:
            print("Cancelled.")
            return 1

    hid_flash = Path(__file__).resolve().parent / "hid_flash.py"
    rc = subprocess.call(["uv", "run", str(hid_flash), str(args.updater_image)])
    if rc != 0:
        return rc

    if wait_for_hid_iap(args.timeout):
        print("Bootloader updater completed; device is back in HID IAP.")
        return 0

    print("Updater flash command completed, but HID IAP did not reappear before timeout.", file=sys.stderr)
    return 3


if __name__ == "__main__":
    raise SystemExit(main())
