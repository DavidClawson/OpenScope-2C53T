#!/usr/bin/env python3
"""Run `meter stream 200 20` back to back for a fixed time, one host clock for all lines.

Usage: stream_loop.py <seconds> <outfile>
Each output line: "<t_host_s>  <device line>". Gaps between consecutive streams are
~20-40 ms (command turnaround), visible as a jump in the device-side index.
Port: $OPENSCOPE_PORT, else the first /dev/cu.usbmodem* or /dev/ttyACM*.
"""
import glob
import os
import sys
import time

import serial

total = float(sys.argv[1])
out = open(sys.argv[2], "w", buffering=1)
port = os.environ.get("OPENSCOPE_PORT") or sorted(
    glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))[0]
s = serial.Serial(port, 115200, timeout=0.005)
time.sleep(0.2)
s.read(65536)
t0 = time.monotonic()
out.write(f"# start {time.strftime('%H:%M:%S')} port {port}\n")
buf = b""
while time.monotonic() - t0 < total:
    s.write(b"meter stream 200 20\r\n")
    s.flush()
    t_cmd = time.monotonic()
    while True:
        b = s.read(4096)
        now = time.monotonic()
        if b:
            buf += b
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                out.write(f"{now - t0:8.3f}  {line.decode('utf-8', 'replace').rstrip()}\n")
        if buf.strip().endswith(b">") and now - t_cmd > 3.9:
            buf = b""
            break
        if now - t_cmd > 8:
            out.write(f"{now - t0:8.3f}  # stream timeout\n")
            buf = b""
            break
out.write(f"# end {time.strftime('%H:%M:%S')}\n")
s.close()
