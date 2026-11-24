#!/usr/bin/env python3
"""Log the firmware's PC3 rising-edge counter (EXINT3) as edges per second.

Usage: pc3_edge_rate.py <counter_addr_hex> <seconds> <outfile>
  counter_addr_hex: address of `pc3_edges`, from
    arm-none-eabi-nm firmware/build/firmware.elf | grep pc3_edges
Reads it with the shell's `mem read <addr> 1` about every 100 ms and logs
"<t_host_s> <count> <edges_per_s_since_previous>". Read-only.
Expected from #94 (unit #2): ~2240 rising edges/s with the leads shorted, 0 open.
Port: $OPENSCOPE_PORT, else the first /dev/cu.usbmodem* or /dev/ttyACM*.
"""
import glob
import os
import re
import sys
import time

import serial

addr = int(sys.argv[1], 16)
secs = float(sys.argv[2])
out = open(sys.argv[3], "w", buffering=1)
port = os.environ.get("OPENSCOPE_PORT") or sorted(
    glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))[0]
s = serial.Serial(port, 115200, timeout=0.002)
time.sleep(0.2)
s.read(65536)
rx = re.compile(rb"0x%08X:\s+([0-9A-Fa-f]{8})" % addr)
t0 = time.monotonic()
buf = b""
prev = None
rates = []
while time.monotonic() - t0 < secs:
    s.write(b"mem read %08X 1\r\n" % addr)
    deadline = time.monotonic() + 0.3
    m = None
    while time.monotonic() < deadline:
        buf += s.read(256)
        m = rx.search(buf)
        if m:
            buf = buf[m.end():]
            break
    if not m:
        continue
    t = time.monotonic() - t0
    c = int(m.group(1), 16)
    rate = "" if prev is None else f"{(c - prev[1]) / (t - prev[0]):.0f}"
    if rate:
        rates.append(float(rate))
    out.write(f"{t:8.3f} {c:10d} {rate}\n")
    prev = (t, c)
    time.sleep(0.1)
s.close()
active = [r for r in rates if r > 100]
print(f"samples {len(rates)}; idle (<=100/s): {len(rates) - len(active)}; "
      f"active: {len(active)}" + (f", median {sorted(active)[len(active) // 2]:.0f} edges/s" if active else ""))
