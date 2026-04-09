#!/usr/bin/env python3
"""
Dump the external W25Q128 over the OpenScope USB CDC debug shell.

This uses the firmware-side `flash read <addr> <len>` command, so it is
read-only and keeps all traffic inside the existing USB serial path.
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

import usb_debug


HEX_LINE_RE = re.compile(r"^0x[0-9A-Fa-f]+:\s*(.*)$")


def parse_flash_read_output(text: str) -> bytes:
    data = bytearray()
    for line in text.splitlines():
        match = HEX_LINE_RE.match(line.strip())
        if not match:
            continue
        payload = match.group(1).strip()
        if not payload:
            continue
        for byte_str in payload.split():
            data.append(int(byte_str, 16))
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Dump W25Q128 over USB CDC shell")
    parser.add_argument("output", type=Path, help="Output file")
    parser.add_argument("--addr", type=lambda x: int(x, 0), default=0, help="Start address")
    parser.add_argument("--length", type=lambda x: int(x, 0), required=True, help="Bytes to dump")
    parser.add_argument("--chunk", type=lambda x: int(x, 0), default=4096, help="Chunk size (<=4096)")
    parser.add_argument("--timeout", type=float, default=5.0, help="Per-command timeout")
    args = parser.parse_args()

    if args.chunk <= 0 or args.chunk > 4096:
        print("ERROR: --chunk must be between 1 and 4096", file=sys.stderr)
        return 1

    port = usb_debug.find_device()
    print(f"Connecting to {port}...")
    ser = usb_debug.open_serial(port)

    try:
        if hasattr(ser, "read_available"):
            ser.read_available()
        elif ser.in_waiting:
            ser.read(ser.in_waiting)

        jedec = usb_debug.send_command(ser, "flash jedec", timeout=args.timeout)
        print(jedec)
        if "ERR:" in jedec:
            return 1

        args.output.parent.mkdir(parents=True, exist_ok=True)
        remaining = args.length
        addr = args.addr

        with args.output.open("wb") as f:
            while remaining > 0:
                chunk = min(args.chunk, remaining)
                cmd = f"flash dump 0x{addr:X} {chunk}"

                ser.reset_input_buffer()
                ser.write((cmd + "\r").encode())

                header = bytearray()
                deadline = time.time() + args.timeout
                while time.time() < deadline:
                    b = ser.read(1)
                    if b:
                        header.extend(b)
                        if header.endswith(b"FLASHDUMP ") or header.endswith(b"\r\nFLASHDUMP "):
                            break
                else:
                    print(f"ERROR: timeout waiting for dump header at 0x{addr:06X}", file=sys.stderr)
                    return 1

                len_digits = bytearray()
                while time.time() < deadline:
                    b = ser.read(1)
                    if not b:
                        continue
                    if b == b"\r":
                        break
                    len_digits.extend(b)
                if not len_digits:
                    print(f"ERROR: malformed dump header at 0x{addr:06X}", file=sys.stderr)
                    return 1
                _ = ser.read(1)  # consume trailing '\n'

                expected = int(len_digits.decode("ascii"), 10)
                if expected != chunk:
                    print(
                        f"ERROR: device announced {expected} bytes at 0x{addr:06X}, expected {chunk}",
                        file=sys.stderr,
                    )
                    return 1

                data = bytearray()
                while len(data) < chunk and time.time() < deadline:
                    piece = ser.read(chunk - len(data))
                    if piece:
                        data.extend(piece)
                if len(data) != chunk:
                    print(
                        f"ERROR: expected {chunk} bytes at 0x{addr:06X}, got {len(data)}",
                        file=sys.stderr,
                    )
                    return 1

                # Drain the trailing prompt from the shell task.
                time.sleep(0.02)
                if hasattr(ser, "read_available"):
                    ser.read_available()
                elif ser.in_waiting:
                    ser.read(ser.in_waiting)

                f.write(data)
                addr += chunk
                remaining -= chunk

                done = args.length - remaining
                pct = (done * 100) // args.length if args.length else 100
                print(f"\r[{pct:3d}%] 0x{done:06X} / 0x{args.length:06X}", end="", flush=True)

        print()
        print(f"Saved {args.length} bytes to {args.output}")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
