#!/usr/bin/env python3
"""Drive the OpenScope USB CDC debug shell non-interactively.

Usage: uv run python scripts/serial_cmd.py "<cmd1>" "<cmd2>" ...
Opens the AT32 VCP, flushes the banner, sends each command (CR+LF),
and prints everything the device replies. Read-only side effects depend
entirely on the commands you pass.
"""
import glob
import sys
import time

import serial

PORT_GLOBS = [
    # macOS
    "/dev/cu.usbmodem*", "/dev/tty.usbmodem*",
    # Linux (mars): AT32 CDC enumerates as ttyACM
    "/dev/ttyACM*", "/dev/ttyUSB*",
]


def find_port():
    for g in PORT_GLOBS:
        hits = sorted(glob.glob(g))
        if hits:
            return hits[0]
    raise SystemExit("No usbmodem serial port found")


def drain(ser, settle=0.4, max_wait=4.0):
    """Read until the device goes quiet for `settle` seconds."""
    out = bytearray()
    last = time.time()
    start = time.time()
    while time.time() - start < max_wait:
        n = ser.in_waiting
        if n:
            out += ser.read(n)
            last = time.time()
        elif time.time() - last >= settle:
            break
        else:
            time.sleep(0.02)
    return out.decode("utf-8", "replace")


def main():
    cmds = sys.argv[1:]
    port = find_port()
    ser = serial.Serial(port, 115200, timeout=0.1)
    print(f"# connected: {port}\n", flush=True)
    time.sleep(0.3)
    banner = drain(ser, settle=0.5, max_wait=2.0)  # flush welcome/prompt
    for c in cmds:
        ser.reset_input_buffer()
        ser.write((c + "\r\n").encode())
        ser.flush()
        # status / h2verify can be long+slow; give them more time
        long_cmd = any(k in c for k in ("status", "h2verify", "acqtest", "scan",
                                        "reinit", "acqread", "scopetest", "gowin"))
        resp = drain(ser, settle=2.0 if long_cmd else 0.35,
                     max_wait=20.0 if long_cmd else 4.0)
        print(f"===== $ {c} =====")
        print(resp.strip("\r\n"))
        print()
    ser.close()


if __name__ == "__main__":
    main()
