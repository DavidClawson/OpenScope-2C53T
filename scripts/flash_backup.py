#!/usr/bin/env python3
"""Full backup of the external W25Q128 SPI flash via the USB CDC debug shell.

The firmware's `flash dump <addr> <len>` command streams RAW BINARY after a
"FLASHDUMP <len>\\r\\n" header line, then returns to the "> " prompt. We loop
over the chip in chunks and write one binary file.

WHY RETRY + RESUME (issue #39)
------------------------------
On guest-coldtrace v0.4.0 the CDC shell wedges during long runs: one request
comes back as a bare "> " with no FLASHDUMP header, then the port goes silent
while the UI keeps running. Observed after 1.0 / 6.0 / 8.1 / 4.4 MB on one
unit, at a different address each time. Only a USB replug + reset recovers it.
A full 16 MB dump therefore can't be assumed to finish in one run. So:

  - every chunk is validated (header length must match, byte count must be
    exact) and retried a few times;
  - if the port stays silent, we stop, print the resume command and exit 2.
    The output file keeps every chunk written so far, at its own offset;
  - `--resume` continues from the end of the existing file.

Usage:
  flash_backup.py OUT.bin [total_bytes]             # whole chip (16 MB)
  flash_backup.py OUT.bin --resume                  # after replug + reset
  flash_backup.py OUT.bin --start 0xCB0000 --end 0xCD0000   # one region
Options: --port, --chunk (1..4096, default 1024), --retries (default 3)
"""
from __future__ import annotations

import argparse
import glob
import os
import sys
import time

import serial

DEFAULT_TOTAL = 16 * 1024 * 1024  # W25Q128 = 16 MB
DEFAULT_CHUNK = 1024              # 4096 wedged sooner in the #39 runs
MAX_CHUNK = 4096                  # firmware limit: `len must be 1..4096`
AT32_VCP = (0x2E3C, 0x5740)       # Artery virtual COM port (app CDC shell)
EXIT_WEDGED = 2


class ChunkError(Exception):
    """One request did not produce a well-formed FLASHDUMP reply."""


def find_port() -> str:
    """Prefer the AT32 VCP by VID:PID; other usbmodem devices may be attached."""
    try:
        from serial.tools import list_ports
        hits = sorted(p.device for p in list_ports.comports()
                      if (p.vid, p.pid) == AT32_VCP and p.device.startswith("/dev/cu."))
        hits = hits or sorted(p.device for p in list_ports.comports()
                              if (p.vid, p.pid) == AT32_VCP)
        if hits:
            return hits[0]
    except Exception:
        pass
    for g in ("/dev/cu.usbmodem*", "/dev/tty.usbmodem*", "/dev/ttyACM*"):
        hits = sorted(glob.glob(g))
        if hits:
            return hits[0]
    raise SystemExit("No OpenScope serial port found (AT32 VCP 2e3c:5740)")


def read_chunk(ser, addr: int, n: int, timeout: float = 4.0) -> bytes:
    """Send one `flash dump`, return exactly n bytes or raise ChunkError."""
    ser.reset_input_buffer()
    ser.write(f"flash dump 0x{addr:06X} {n}\r\n".encode())
    ser.flush()
    deadline = time.time() + timeout
    buf = bytearray()
    while b"FLASHDUMP" not in buf:
        if b"ERR" in buf and b"\n" in buf[buf.index(b"ERR"):]:
            raise ChunkError(f"device error: {bytes(buf[buf.index(b'ERR'):]).strip()!r}")
        if time.time() > deadline:
            raise ChunkError(f"no FLASHDUMP header; got tail {bytes(buf[-40:])!r}")
        buf += ser.read(64)
    rest = bytearray(buf[buf.index(b"FLASHDUMP"):])
    while b"\n" not in rest:
        if time.time() > deadline:
            raise ChunkError(f"header line never ended: {bytes(rest[:40])!r}")
        rest += ser.read(1)
    nl = rest.index(b"\n")
    fields = bytes(rest[:nl]).split()
    try:
        announced = int(fields[1])
    except (IndexError, ValueError):
        raise ChunkError(f"malformed header {bytes(rest[:nl])!r}") from None
    if announced != n:
        raise ChunkError(f"header announces {announced} bytes, asked for {n}")
    data = bytearray(rest[nl + 1:])
    while len(data) < n:
        if time.time() > deadline:
            raise ChunkError(f"short read: {len(data)}/{n} bytes")
        data += ser.read(n - len(data))
    return bytes(data[:n])


def dump(ser, out_path: str, start: int, end: int, chunk: int, retries: int,
         log=print, sleep=time.sleep) -> int:
    """Dump [start, end) into out_path at matching offsets. Returns next address."""
    mode = "r+b" if os.path.exists(out_path) else "wb"
    t0 = time.time()
    with open(out_path, mode) as f:
        addr = start
        while addr < end:
            n = min(chunk, end - addr)
            for attempt in range(retries + 1):
                try:
                    data = read_chunk(ser, addr, n)
                    break
                except ChunkError as e:
                    log(f"  retry 0x{addr:06X} #{attempt + 1}: {e}")
                    sleep(0.5 * (attempt + 1))
            else:
                f.flush()
                return addr
            f.seek(addr)
            f.write(data)
            addr += n
            if addr % (1 << 20) == 0 or addr == end:
                rate = (addr - start) / max(1e-3, time.time() - t0) / 1024
                log(f"  0x{addr:06X}  {100.0 * addr / end:5.1f}%  {rate:.0f} KiB/s")
    return addr


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out")
    ap.add_argument("total", nargs="?", type=lambda s: int(s, 0), default=None,
                    help="bytes to dump from 0 (legacy form of --end)")
    ap.add_argument("--start", type=lambda s: int(s, 0), default=None)
    ap.add_argument("--end", type=lambda s: int(s, 0), default=None)
    ap.add_argument("--resume", action="store_true",
                    help="continue from the end of the existing OUT file")
    ap.add_argument("--port")
    ap.add_argument("--chunk", type=int, default=DEFAULT_CHUNK)
    ap.add_argument("--retries", type=int, default=3)
    a = ap.parse_args(argv)

    if not 1 <= a.chunk <= MAX_CHUNK:
        ap.error(f"--chunk must be 1..{MAX_CHUNK}")
    end = a.end if a.end is not None else (a.total if a.total is not None else DEFAULT_TOTAL)
    if a.resume:
        if a.start is not None:
            ap.error("--resume and --start are mutually exclusive")
        have = os.path.getsize(a.out) if os.path.exists(a.out) else 0
        start = (have // a.chunk) * a.chunk
    else:
        start = a.start or 0
        if a.start is None and os.path.exists(a.out):
            # A fresh dump must not silently inherit (or clobber) an old backup.
            ap.error(f"{a.out} exists: use --resume, --start/--end, or pick a new name")
    if start >= end:
        print(f"# nothing to do: 0x{start:06X} >= 0x{end:06X}")
        return 0

    port = a.port or find_port()
    ser = serial.Serial(port, 115200, timeout=0.2)
    print(f"# port {port}  ->  {a.out}  0x{start:06X}..0x{end:06X}  chunk {a.chunk}", flush=True)
    time.sleep(0.3)
    t0 = time.time()
    reached = dump(ser, a.out, start, end, a.chunk, a.retries,
                   log=lambda m: print(m, flush=True))
    ser.close()
    if reached < end:
        print(f"# shell stopped answering at 0x{reached:06X} (see issue #39).")
        print("# Replug USB + reset the scope, then continue with:")
        print(f"#   python3 {sys.argv[0]} {a.out} --resume --end 0x{end:X} --chunk {a.chunk}")
        return EXIT_WEDGED
    print(f"# done: {a.out}  0x{start:06X}..0x{end:06X}  in {time.time() - t0:.0f}s", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
