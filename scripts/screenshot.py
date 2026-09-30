#!/usr/bin/env python3
"""Capture the device's screen over the USB shell and save it as a PNG.

Uses `screen dumpbin`, which streams the firmware's 4-bit indexed LCD shadow
(every pixel written through lcd_write_data, rounded to the 16-colour palette
in lcd.c). The image is therefore the real screen content, but colours are
QUANTISED to that palette -- close, not pixel-exact. The device's CRC32 over
the pixel bytes is checked before anything is written, so a short or
corrupted transfer is refused rather than saved as a plausible picture.

Usage:
  screenshot.py out.png [--port /dev/ttyACM0] [--scale 2]
"""
import argparse
import re
import sys
import time
import zlib

import serial
from PIL import Image

# RGB565 values of lcd.c's lcd_shadow_palette[], in index order (lcd.h).
PALETTE_565 = [0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F,
               0x2104, 0x8410, 0xFCA0, 0x055F, 0x2945, 0x18C3, 0x3186, 0x6BB0]


def rgb(c):
    r = (c >> 11) & 0x1F
    g = (c >> 5) & 0x3F
    b = c & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


HDR = re.compile(rb"SCREENBIN x=(\d+) y=(\d+) w=(\d+) h=(\d+) format=indexed4 len=(\d+) crc32=([0-9A-F]{8})\r\n")


def capture(port, timeout=20.0):
    with serial.Serial(port, 115200, timeout=0.2) as s:
        s.reset_input_buffer()
        s.write(b"\r\n")
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(b"screen dumpbin\r\n")
        buf = b""
        t0 = time.time()
        m = None
        while time.time() - t0 < timeout:
            buf += s.read(4096)
            m = HDR.search(buf)
            if m:
                break
        if not m:
            raise SystemExit("no SCREENBIN header within %.0f s" % timeout)
        w, h, n, crc = int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6), 16)
        data = buf[m.end():]
        while len(data) < n and time.time() - t0 < timeout:
            data += s.read(n - len(data))
        if len(data) < n:
            raise SystemExit("short dump: %d of %d bytes" % (len(data), n))
        data = data[:n]
        got = zlib.crc32(data) & 0xFFFFFFFF
        if got != crc:
            raise SystemExit("CRC mismatch: device %08X, received %08X -- not saved" % (crc, got))
        return w, h, data


def to_image(w, h, data):
    pal = [rgb(c) for c in PALETTE_565]
    img = Image.new("RGB", (w, h))
    px = img.load()
    row_len = (w + 1) // 2
    for y in range(h):
        for x in range(w):
            b = data[y * row_len + (x >> 1)]
            idx = (b & 0x0F) if (x & 1) else (b >> 4)
            px[x, y] = pal[idx]
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args()
    w, h, data = capture(a.port)
    img = to_image(w, h, data)
    if a.scale > 1:
        img = img.resize((w * a.scale, h * a.scale), Image.NEAREST)
    img.save(a.out)
    print("saved %s (%dx%d, CRC verified)" % (a.out, w, h))


if __name__ == "__main__":
    main()
