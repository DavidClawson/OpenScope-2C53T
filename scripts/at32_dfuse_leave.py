#!/usr/bin/env python3
from __future__ import annotations

import argparse
import time

import usb.core
import usb.util

from at32_dfuse_write import (
    DFU_DNLOAD,
    INTF,
    clear_to_idle,
    get_status,
    open_dev,
    set_address,
)


def leave_to_address(address: int) -> None:
    dev = open_dev()
    clear_to_idle(dev)
    set_address(dev, address)

    # ST/Artery DfuSe "leave" is a zero-length DNLOAD after SET_ADDRESS.
    # The ROM enters manifest state, disconnects, then starts the selected code.
    dev.ctrl_transfer(0x21, DFU_DNLOAD, 0, INTF, b"", timeout=5000)
    for _ in range(20):
        try:
            status = get_status(dev)
            print(f"status={status}", flush=True)
        except usb.core.USBError as exc:
            print(f"device detached after leave: {exc}", flush=True)
            break
        time.sleep(0.1)

    try:
        usb.util.release_interface(dev, INTF)
    except usb.core.USBError:
        pass
    try:
        dev.reset()
    except usb.core.USBError:
        pass


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Ask the AT32 ROM DfuSe bootloader to leave DFU and jump to an address."
    )
    parser.add_argument(
        "address",
        nargs="?",
        type=lambda s: int(s, 0),
        default=0x08000000,
        help="flash address to jump to after leaving DFU, default: 0x08000000",
    )
    args = parser.parse_args()
    leave_to_address(args.address)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
