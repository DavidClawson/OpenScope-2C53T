#!/usr/bin/env python3
"""Send one shell command to the scope and print every reply line with a host timestamp.

Usage: shell_ts.py "<command>" [max_seconds] [outfile]
Timestamps are seconds since the command was written, from time.monotonic().
Port: $OPENSCOPE_PORT, else the first /dev/cu.usbmodem* or /dev/ttyACM*.
"""
import glob
import os
import sys
import time

import serial

cmd = sys.argv[1]
max_s = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0
out = open(sys.argv[3], "w") if len(sys.argv) > 3 else None

port = os.environ.get("OPENSCOPE_PORT") or sorted(
    glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))[0]
s = serial.Serial(port, 115200, timeout=0.005)
time.sleep(0.2)
s.read(65536)
t0 = time.monotonic()
s.write((cmd + "\r\n").encode())
s.flush()
buf = b""
last = t0
while time.monotonic() - t0 < max_s:
    b = s.read(4096)
    now = time.monotonic()
    if b:
        buf += b
        last = now
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            txt = line.decode("utf-8", "replace").rstrip("\r")
            rec = f"{now - t0:8.3f}  {txt}"
            print(rec, flush=True)
            if out:
                out.write(rec + "\n")
    elif buf.strip() == b">" and now - last > 0.3:
        break
s.close()
