#!/usr/bin/env python3
"""Flash a firmware image over the OpenScope CDC debug shell.

Host half of firmware/src/drivers/fw_loader.c: sends `fwload <size> <crc32>`,
streams the raw image, waits for the STAGED verdict, then (unless --stage-only)
sends `fwapply`. On apply the device installs the image and SYSTEM-RESETS into
it — a clean boot. The CDC port disappears; that is success, not a crash. Keep
USB attached: the cable carries the power rail through the reset.

The image must be linked for the app slot the installer writes, 0x08007000 —
`make guest` here, and the 2C23T port's own default. The plain `make` flavour is
for the HID bootloader: its vector table sits at 0x08004000, so `objcopy` emits a
file based there, and installing it at 0x08007000 puts everything 0x3000 low. It
passes every gate on the way (at offset 0 it holds a real vector table) and the
device simply does not come back — recover with MENU+Power and the stock IAP.

Images stage into a 1 MB W25Q cache slot (a or b, default b), so this
firmware's own ~600 KB image round-trips fine, and so does the 2C23T port's.
A staged slot persists: `fwswap a|b` in the shell installs a cached image
later with no transfer at all. Recovery from anything: MENU+Power stock IAP
(nothing in this path can write below 0x08007000).

Usage:
  python3 scripts/cdc_flash.py <image.bin> [--port /dev/cu.usbmodemXXX]
                               [--slot a|b] [--stage-only]
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
    ap.add_argument("--slot", choices=["a", "b"], default="b",
                    help="W25Q cache slot to stage into (default b)")
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

        s.write(f"fwload {len(data)} {crc:08X} {args.slot}\r\n".encode())
        got = read_until(s, b"GO ", 5.0)
        if b"GO " not in got:
            sys.exit("device did not accept fwload")

        t0 = time.time()
        # Everything the device says WHILE we stream counts as part of the
        # verdict: on a large image the device can finish, verify and print
        # `fwload: STAGED` before this loop writes its last chunk, and a
        # verdict scanned for only afterwards is then missed entirely. Seen
        # with a 609 192 B image, which staged fine and was reported as a
        # failure. Keep the tail bounded so a chatty `mon` cannot grow it.
        seen = b""
        for off in range(0, len(data), 2048):
            s.write(data[off:off + 2048])
            # drain progress lines so the OS buffer never backs up — and
            # STOP at the first ERROR verdict: pushing the rest of a binary
            # image into a shell that already gave up feeds it as garbage
            # command lines.
            chunk = s.read(4096)
            if chunk:
                sys.stdout.write(chunk.decode(errors="replace"))
                sys.stdout.flush()
                if b"ERROR" in chunk:
                    sys.exit("\ndevice reported an error mid-stream — aborted")
                seen = (seen + chunk)[-4096:]
        rate = len(data) / max(time.time() - t0, 1e-3) / 1024
        print(f"\nstreamed in {time.time() - t0:.1f}s ({rate:.0f} KB/s)")

        if b"fwload:" not in seen:
            seen += read_until(s, b"fwload:", 30.0)
        if b"STAGED" not in seen:
            sys.exit("staging did not verify — see the verdict above")

        if args.stage_only:
            print("staged only, not applied (per --stage-only)")
            return

        s.write(b"fwapply\r\n")
        read_until(s, b"recovery", 5.0)
        print("\napply sent. The device resets into the new image now.")


if __name__ == "__main__":
    main()
