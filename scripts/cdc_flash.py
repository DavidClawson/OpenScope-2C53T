#!/usr/bin/env python3
"""Flash a firmware image over the OpenScope CDC debug shell.

Host half of firmware/src/drivers/fw_loader.c: sends `fwload <size> <crc32>`,
streams the raw image, waits for the STAGED verdict, then (unless --stage-only)
sends `fwapply`. The device does not reboot on apply — the new image is
started directly, so the CDC port disappears; that is success, not a crash.

The staging region caps images at ~382 KB. This tool therefore CANNOT
round-trip this firmware's own ~600 KB image yet (see fw_loader.h); it exists
to swap in other 0x08007000-linked images (e.g. the 2C23T port) and back out
via their own update paths. Recovery from anything: MENU+Power stock IAP.

Usage:
  python3 scripts/cdc_flash.py <image.bin> [--port /dev/cu.usbmodemXXX]
                               [--stage-only]
"""

import argparse
import sys
import time
import zlib

import serial
from serial.tools import list_ports


def find_port() -> str:
    for p in list_ports.comports():
        if "usbmodem" in p.device or "ttyACM" in p.device:
            return p.device
    sys.exit("no CDC port found; pass --port")


def read_until(s: serial.Serial, token: bytes, deadline_s: float) -> bytes:
    buf = b""
    end = time.time() + deadline_s
    while time.time() < end:
        chunk = s.read(4096)
        if chunk:
            buf += chunk
            sys.stdout.write(chunk.decode(errors="replace"))
            sys.stdout.flush()
            if token in buf:
                break
    return buf


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--port", default=None)
    ap.add_argument("--stage-only", action="store_true",
                    help="stage and verify, but do not apply")
    args = ap.parse_args()

    with open(args.image, "rb") as f:
        data = f.read()
    if len(data) % 2:
        data += b"\xff"
    crc = zlib.crc32(data) & 0xFFFFFFFF
    port = args.port or find_port()
    print(f"{args.image}: {len(data)} bytes, crc32 {crc:08X}, port {port}")

    with serial.Serial(port, 115200, timeout=0.1) as s:
        time.sleep(0.3)
        s.reset_input_buffer()

        s.write(f"fwload {len(data)} {crc:08X}\r\n".encode())
        got = read_until(s, b"GO ", 5.0)
        if b"GO " not in got:
            sys.exit("device did not accept fwload")

        t0 = time.time()
        for off in range(0, len(data), 2048):
            s.write(data[off:off + 2048])
            # drain progress lines so the OS buffer never backs up
            chunk = s.read(4096)
            if chunk:
                sys.stdout.write(chunk.decode(errors="replace"))
                sys.stdout.flush()
        rate = len(data) / max(time.time() - t0, 1e-3) / 1024
        print(f"\nstreamed in {time.time() - t0:.1f}s ({rate:.0f} KB/s)")

        got = read_until(s, b"fwload:", 10.0)
        if b"STAGED" not in got:
            sys.exit("staging did not verify — see the verdict above")

        if args.stage_only:
            print("staged only, not applied (per --stage-only)")
            return

        s.write(b"fwapply\r\n")
        read_until(s, b"recovery", 5.0)
        print("\napply sent. The port drops when the new image takes over.")


if __name__ == "__main__":
    main()
