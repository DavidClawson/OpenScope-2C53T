#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

import usb.core
import usb.util


VID = 0x2E3C
PID = 0xDF11
INTF = 0
ALT_INTERNAL_FLASH = 0
TRANSFER = 2048
SECTOR_SIZE = 2048

DFU_DNLOAD = 1
DFU_UPLOAD = 2
DFU_GETSTATUS = 3
DFU_CLRSTATUS = 4
DFU_ABORT = 6

STATE_DFU_IDLE = 2
STATE_DFU_DNLOAD_IDLE = 5
STATE_DFU_ERROR = 10


def open_dev():
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        raise SystemExit("AT32 ROM DFU device 2e3c:df11 not found")
    try:
        dev.set_configuration()
    except usb.core.USBError:
        pass
    try:
        if dev.is_kernel_driver_active(INTF):
            dev.detach_kernel_driver(INTF)
    except (NotImplementedError, usb.core.USBError):
        pass
    usb.util.claim_interface(dev, INTF)
    dev.set_interface_altsetting(interface=INTF, alternate_setting=ALT_INTERNAL_FLASH)
    return dev


def get_status(dev):
    data = dev.ctrl_transfer(0xA1, DFU_GETSTATUS, 0, INTF, 6, timeout=5000)
    status = data[0]
    poll_ms = data[1] | (data[2] << 8) | (data[3] << 16)
    state = data[4]
    return status, poll_ms, state


def clear_to_idle(dev):
    for _ in range(8):
        status, poll_ms, state = get_status(dev)
        if state == STATE_DFU_IDLE:
            return
        if state == STATE_DFU_ERROR:
            dev.ctrl_transfer(0x21, DFU_CLRSTATUS, 0, INTF, b"", timeout=5000)
        else:
            dev.ctrl_transfer(0x21, DFU_ABORT, 0, INTF, b"", timeout=5000)
        time.sleep(max(poll_ms, 20) / 1000.0)
    status, poll_ms, state = get_status(dev)
    if state != STATE_DFU_IDLE:
        raise RuntimeError(f"DFU did not return to idle: status={status} state={state} poll={poll_ms}")


def wait_idleish(dev, label):
    for _ in range(80):
        status, poll_ms, state = get_status(dev)
        if state in (STATE_DFU_IDLE, STATE_DFU_DNLOAD_IDLE):
            return
        if state == STATE_DFU_ERROR:
            raise RuntimeError(f"{label}: DFU error status={status}")
        time.sleep(max(poll_ms, 20) / 1000.0)
    status, poll_ms, state = get_status(dev)
    raise RuntimeError(f"{label}: timeout status={status} state={state} poll={poll_ms}")


def special(dev, payload: bytes, label: str):
    dev.ctrl_transfer(0x21, DFU_DNLOAD, 0, INTF, payload, timeout=5000)
    wait_idleish(dev, label)
    # AT32 ROM DFU can report ready a little early after flash commands.
    time.sleep(0.08)


def erase_page(dev, address: int):
    special(dev, b"\x41" + struct.pack("<I", address), f"erase 0x{address:08X}")


def set_address(dev, address: int):
    special(dev, b"\x21" + struct.pack("<I", address), f"set-address 0x{address:08X}")


def write_block(dev, data: bytes):
    dev.ctrl_transfer(0x21, DFU_DNLOAD, 2, INTF, data, timeout=5000)
    wait_idleish(dev, "write")
    time.sleep(0.08)


def parse_preserve_range(text: str) -> tuple[int, int]:
    if ":" in text:
        start_text, end_text = text.split(":", 1)
        start = int(start_text, 0)
        end = int(end_text, 0)
    elif "+" in text:
        start_text, length_text = text.split("+", 1)
        start = int(start_text, 0)
        end = start + int(length_text, 0)
    else:
        raise argparse.ArgumentTypeError("expected START:END or START+LEN")
    if end <= start:
        raise argparse.ArgumentTypeError("preserve range end must be greater than start")
    if start % SECTOR_SIZE != 0 or end % SECTOR_SIZE != 0:
        raise argparse.ArgumentTypeError(f"preserve range must be {SECTOR_SIZE}-byte sector-aligned")
    return start, end


def preserve_page(page_addr: int, chunk: bytes, ranges: list[tuple[int, int]]) -> bool:
    page_end = page_addr + len(chunk)
    return chunk == b"\xFF" * len(chunk) and any(start <= page_addr and page_end <= end for start, end in ranges)


def upload(dev, address: int, size: int) -> bytes:
    set_address(dev, address)
    out = bytearray()
    block = 2
    while len(out) < size:
        chunk = dev.ctrl_transfer(0xA1, DFU_UPLOAD, block, INTF, min(TRANSFER, size - len(out)), timeout=5000)
        out.extend(bytes(chunk))
        block += 1
    return bytes(out)


def write_image(
    dev,
    address: int,
    data: bytes,
    *,
    verify: bool,
    skip_blank_pages: bool = False,
    preserve_blank_page_ranges: list[tuple[int, int]] | None = None,
):
    preserve_blank_page_ranges = preserve_blank_page_ranges or []
    offset = 0
    while offset < len(data):
        chunk = data[offset:offset + TRANSFER]
        if len(chunk) < TRANSFER:
            chunk += b"\xFF" * (TRANSFER - len(chunk))
        page_addr = address + offset
        if preserve_page(page_addr, chunk, preserve_blank_page_ranges):
            print(f"preserve blank 0x{page_addr:08X}", flush=True)
            offset += TRANSFER
            continue
        if skip_blank_pages and chunk == b"\xFF" * len(chunk):
            print(f"erase blank 0x{page_addr:08X}", flush=True)
            clear_to_idle(dev)
            erase_page(dev, page_addr)
            clear_to_idle(dev)
            offset += TRANSFER
            continue
        print(f"erase/write 0x{page_addr:08X}", flush=True)
        clear_to_idle(dev)
        erase_page(dev, page_addr)
        clear_to_idle(dev)
        set_address(dev, page_addr)
        write_block(dev, chunk)
        if verify:
            got = upload(dev, page_addr, len(chunk))
            if got != chunk:
                raise RuntimeError(f"verify failed at 0x{page_addr:08X}")
        offset += TRANSFER


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("address", type=lambda s: int(s, 0))
    parser.add_argument("image", type=Path)
    parser.add_argument("--no-inline-verify", action="store_true")
    parser.add_argument(
        "--skip-blank-pages",
        action="store_true",
        help="for all-0xFF pages, erase-to-blank instead of programming them",
    )
    parser.add_argument(
        "--preserve-blank-pages-range",
        action="append",
        type=parse_preserve_range,
        default=[],
        metavar="START:END",
        help="for all-0xFF pages wholly inside this sector-aligned range, do not erase/program",
    )
    args = parser.parse_args()
    dev = open_dev()
    clear_to_idle(dev)
    write_image(
        dev,
        args.address,
        args.image.read_bytes(),
        verify=not args.no_inline_verify,
        skip_blank_pages=args.skip_blank_pages,
        preserve_blank_page_ranges=args.preserve_blank_pages_range,
    )
    clear_to_idle(dev)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
