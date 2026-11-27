#!/usr/bin/env python3
"""Read back the PC3 / EXTI3 configuration over the shell's `mem read`.

Usage: pc3_readback.py
Prints GPIOC CRL/IDR/ODR, AFIO EXTICR1, EXTI IMR/RTSR/FTSR/PR, NVIC ISER0 and
the IRQ 8-11 priority word. Pass `pc3_edges` as an extra hex address to read
the edge counter too. Read-only.
Port: $OPENSCOPE_PORT, else the first /dev/cu.usbmodem* or /dev/ttyACM*.
"""
import glob
import os
import re
import sys
import time

import serial

regs = [("GPIOC_CRL", 0x40011000), ("GPIOC_IDR", 0x40011008), ("GPIOC_ODR", 0x4001100C),
        ("AFIO_EXTICR1", 0x40010008), ("EXTI_IMR", 0x40010400), ("EXTI_RTSR", 0x40010408),
        ("EXTI_FTSR", 0x4001040C), ("EXTI_PR", 0x40010414), ("NVIC_ISER0", 0xE000E100),
        ("NVIC_IPR8", 0xE000E408)]
regs += [("extra", int(a, 16)) for a in sys.argv[1:]]
port = os.environ.get("OPENSCOPE_PORT") or sorted(
    glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))[0]
s = serial.Serial(port, 115200, timeout=0.05)
time.sleep(0.2)
s.read(65536)
for name, a in regs:
    s.write(b"mem read %08X 1\r\n" % a)
    buf = b""
    m = None
    deadline = time.monotonic() + 0.5
    while time.monotonic() < deadline:
        buf += s.read(256)
        m = re.search(rb"0x%08X:\s+([0-9A-Fa-f]{8})" % a, buf)
        if m:
            break
    print(f"{name:13s} {a:08X} = {m.group(1).decode() if m else 'NO REPLY'}")
s.close()
