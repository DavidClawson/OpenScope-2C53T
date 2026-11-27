#!/usr/bin/env python3
"""Sample GPIOC IDR (0x40011008) over the shell's `mem read` as fast as the link allows.

Usage: pc3_sample.py <seconds> <outfile>
Logs "<t_host_s> <idr_hex> pc3=<0|1> pc13=<0|1>" per sample and prints a summary:
sample rate, and for PC3 and PC13 the fraction of samples high and the number of
level changes. PC13 is the UP button (passive input): pressing it during the run is
the positive control that this instrument sees GPIOC input changes at all.
Read-only: `mem read` of an input data register has no side effects.
Port: $OPENSCOPE_PORT, else the first /dev/cu.usbmodem* or /dev/ttyACM*.
"""
import glob
import os
import re
import sys
import time

import serial

secs = float(sys.argv[1])
out = open(sys.argv[2], "w", buffering=1)
port = os.environ.get("OPENSCOPE_PORT") or sorted(
    glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))[0]
s = serial.Serial(port, 115200, timeout=0.002)
time.sleep(0.2)
s.read(65536)
rx = re.compile(rb"0x40011008:\s+([0-9A-Fa-f]{8})")
t0 = time.monotonic()
buf = b""
n = 0
prev = {3: None, 13: None}
changes = {3: 0, 13: 0}
highs = {3: 0, 13: 0}
while time.monotonic() - t0 < secs:
    s.write(b"mem read 40011008 1\r\n")
    deadline = time.monotonic() + 0.2
    while time.monotonic() < deadline:
        buf += s.read(256)
        m = rx.search(buf)
        if m:
            buf = buf[m.end():]
            break
    else:
        continue
    t = time.monotonic() - t0
    idr = int(m.group(1), 16)
    n += 1
    for bit in (3, 13):
        v = (idr >> bit) & 1
        highs[bit] += v
        if prev[bit] is not None and v != prev[bit]:
            changes[bit] += 1
        prev[bit] = v
    out.write(f"{t:8.3f} {idr:08X} pc3={(idr >> 3) & 1} pc13={(idr >> 13) & 1}\n")
s.close()
el = time.monotonic() - t0
print(f"samples {n} in {el:.1f}s ({n / el:.0f}/s)")
for bit in (3, 13):
    print(f"PC{bit}: high {highs[bit]}/{n}, level changes {changes[bit]}")
