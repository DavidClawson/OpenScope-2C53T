#!/usr/bin/env python3
"""Host driver for the bluepill_bis rig (see src/rig.c for the leg map).

Usage:
  python3 host.py --port /dev/ttyUSB0 i           # IDCODE anchor, hardware SPI
  python3 host.py --port /dev/ttyUSB0 b           # bit-bang V0.4 entry attempt
  python3 host.py --port /dev/ttyUSB0 1 --fs top.fs   # BIS-1 with payload
  python3 host.py --port /dev/ttyUSB0 --raw 'e 0120681B'

Payload files: a Gowin .fs (ASCII 0/1 lines; comment/header lines are
skipped) is packed to raw bytes MSB-first; any other file is streamed as-is.
The scope's extracted stock bitstream (binary) works unchanged.

Requires pyserial (same dependency as scripts/bench.py).
"""
import argparse
import sys
import time

import serial


def fs_to_bytes(path: str) -> bytes:
    raw = open(path, "rb").read()
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError:
        return raw                      # already binary
    bits = []
    for line in text.splitlines():
        line = line.strip()
        if not line or any(c not in "01" for c in line):
            continue                    # header / comment / checksum line
        bits.append(line)
    if not bits:
        return raw
    stream = "".join(bits)
    stream += "0" * (-len(stream) % 8)
    return bytes(int(stream[i:i + 8], 2) for i in range(0, len(stream), 8))


def read_until_prompt(ser: serial.Serial, timeout: float = 15.0) -> str:
    out, deadline = [], time.time() + timeout
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            out.append(chunk.decode(errors="replace"))
            if "OK>" in "".join(out):
                break
    return "".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("leg", nargs="?", help="single-letter leg (i I a 1 2 b B s)")
    ap.add_argument("--fs", help="bitstream to stream as the 0x3B payload")
    ap.add_argument("--raw", help="send an arbitrary rig command line instead")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    time.sleep(0.3)
    ser.reset_input_buffer()

    if args.raw:
        ser.write((args.raw + "\n").encode())
        print(read_until_prompt(ser), end="")
        return 0
    if not args.leg:
        ap.error("need a leg or --raw")

    if args.fs:
        payload = fs_to_bytes(args.fs)
        print(f"# payload: {len(payload)} bytes")
        ser.write(b"p\n")
        banner = read_until_prompt(ser, 3)
        print(banner, end="")
        if "PAYLOAD:ARMED" not in banner:
            print("!! rig did not arm payload mode", file=sys.stderr)
            return 1

    ser.write((args.leg + "\n").encode())

    if args.fs:
        # wait for the rig to open the 0x3B frame and ask
        buf, deadline = "", time.time() + 10
        while "PAYLOAD:SEND" not in buf and time.time() < deadline:
            buf += ser.read(ser.in_waiting or 1).decode(errors="replace")
        print(buf, end="")
        if "PAYLOAD:SEND" not in buf:
            print("!! rig never requested the payload", file=sys.stderr)
            return 1
        ser.write(len(payload).to_bytes(4, "little"))
        for i in range(0, len(payload), 4096):
            ser.write(payload[i:i + 4096])
        print(f"# streamed {len(payload)} bytes")

    print(read_until_prompt(ser, 60), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
