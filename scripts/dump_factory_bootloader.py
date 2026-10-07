#!/usr/bin/env python3
"""Dump the factory IAP bootloader (0x08000000-0x08006000, 24KB) over the CDC shell.

Stops at 0x08006000 on purpose. 0x08006000-0x08006FFF is NOT bootloader code: it
is stock's settings page, which carries the unit's own factory calibration. The
archive this script writes by default is what the DFU guide's full factory
restore flashes to 0x08000000 on *other* units, so a dump that included the page
put this unit's calibration on every unit restored from it (issue #38). Back the
page up on its own instead (README, "Back up your factory calibration first";
`mem read 0x08006000 1024`), and keep it out of this archive.

Reads internal flash from app context via the debug shell's `mem read` command
(64 words / 256 bytes per call), so RDP never gets in the way. Run on a unit
that still has the factory bootloader at 0x08000000 (i.e., one that flashes
via MENU+Power IAP).

Usage: uv run python scripts/dump_factory_bootloader.py [outfile]
Default outfile: archive/factory_iap_bootloader_2C53T.bin
"""
import glob
import re
import struct
import sys
import time

import serial

PORT_GLOBS = ["/dev/cu.usbmodem*", "/dev/tty.usbmodem*"]
BASE = 0x08000000
SIZE = 0x6000  # 24KB: IAP code only. 0x08006000 = per-unit settings/cal page (#38)
CHUNK_WORDS = 64  # shell cap per `mem read`

WORD_RE = re.compile(r"^0x([0-9A-Fa-f]{8}):((?:\s+[0-9A-Fa-f]{8})+)\s*$", re.M)


def find_port():
    for g in PORT_GLOBS:
        hits = sorted(glob.glob(g))
        if hits:
            return hits[0]
    raise SystemExit("No usbmodem serial port found — plug in the unit (normal mode, not IAP)")


def drain(ser, settle=0.25, max_wait=4.0):
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
            time.sleep(0.01)
    return out.decode("utf-8", "replace")


def read_chunk(ser, addr, words, retries=3):
    for attempt in range(retries):
        ser.reset_input_buffer()
        ser.write(f"mem read {addr:08X} {words}\r\n".encode())
        ser.flush()
        resp = drain(ser)
        data = {}
        for m in WORD_RE.finditer(resp):
            line_addr = int(m.group(1), 16)
            for i, w in enumerate(m.group(2).split()):
                data[line_addr + i * 4] = int(w, 16)
        chunk = bytearray()
        ok = True
        for i in range(words):
            a = addr + i * 4
            if a not in data:
                ok = False
                break
            chunk += struct.pack("<I", data[a])
        if ok:
            return bytes(chunk)
        time.sleep(0.2)
    raise SystemExit(f"FAILED at 0x{addr:08X} after {retries} attempts:\n{resp}")


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "archive/factory_iap_bootloader_2C53T.bin"
    port = find_port()
    ser = serial.Serial(port, 115200, timeout=0.1)
    print(f"# connected: {port}")
    time.sleep(0.3)
    drain(ser, settle=0.5, max_wait=2.0)  # flush banner

    blob = bytearray()
    t0 = time.time()
    for off in range(0, SIZE, CHUNK_WORDS * 4):
        blob += read_chunk(ser, BASE + off, CHUNK_WORDS)
        done = off + CHUNK_WORDS * 4
        print(f"\r{done}/{SIZE} bytes ({100 * done // SIZE}%)", end="", flush=True)
    ser.close()
    print(f"\ndump complete in {time.time() - t0:.1f}s")

    # Sanity: Cortex-M vector table — initial SP in SRAM, reset vector in this region (thumb bit set)
    sp, rv = struct.unpack_from("<II", blob, 0)
    sp_ok = 0x20000000 <= sp <= 0x20038000
    rv_ok = BASE <= (rv & ~1) < BASE + SIZE and (rv & 1)
    print(f"initial SP: 0x{sp:08X} {'OK' if sp_ok else 'SUSPECT'}")
    print(f"reset vec : 0x{rv:08X} {'OK (in-region thumb)' if rv_ok else 'SUSPECT'}")
    ff_tail = len(blob) - len(blob.rstrip(b"\xff"))
    print(f"trailing 0xFF padding: {ff_tail} bytes")
    if not (sp_ok and rv_ok):
        print("WARNING: vector table does not look like a bootloader — inspect before publishing")

    with open(out_path, "wb") as f:
        f.write(blob)
    print(f"wrote {len(blob)} bytes -> {out_path}")


if __name__ == "__main__":
    main()
