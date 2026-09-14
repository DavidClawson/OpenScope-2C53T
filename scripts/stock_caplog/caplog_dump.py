#!/usr/bin/env python3
"""Read the stock-caplog SRAM block out of the OpenScope debug shell and decode it.

    python3 scripts/stock_caplog/caplog_dump.py --port /dev/ttyACM0 --save run.caplog
    python3 scripts/stock_caplog/caplog_dump.py --hid --save run.caplog      # HID bootloader, untested
    python3 scripts/stock_caplog/caplog_dump.py --decode run.caplog

The block is 0x700 bytes at 0x20037000. It is written by the logger in
scripts/stock_caplog/caplog.c while the *stock* firmware runs, and read back
after a MENU+Power round trip -- the rail stays up, the SRAM keeps the block --
by a firmware that leaves that SRAM alone. THIS FIRMWARE DOES NOT: its .bss ends
above 0x20037000 (`_ebss` 0x200375D4 in the app flavour, higher in guest) and
startup zeroes it, measured 2026-09-13 as an all-zero block. The proven reader is
the 2C23T port (Stlkv/OpenScope-2C23T-2C53T-port, RAM ends 0x20036078), whose
shell answers `mem <addr> <len>`; this tool speaks both that and this shell's
`mem read <addr> <count>` and picks by the reply. If the magic is not CAPL, the
block did not survive (or stock never reached a SysTick with the hooks in place);
say so rather than decode noise.

Decodes: header, the last 32 TX frames to the meter SoC (with the 0x05xx word),
the last 64 GPIO events (ODR changes with the pins that moved, CFG changes with
the per-pin mode), and the per-pin toggle counters. `decode()` returns a dict so
tests can assert on it; `render()` prints it.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # scripts/ for hid_flash.py

BASE = 0x20037000
SIZE = 0x700
MAGIC = 0x4C504143  # "CAPL"
PORTS = "ABCDE"
WORDS_PER_READ = 64  # usb_debug.c cmd_mem_read caps count at 64

MEM_LINE_RE = re.compile(r"^0x([0-9A-Fa-f]{8}):((?:\s+[0-9A-Fa-f]{8})+)\s*$")
PORT_LINE_RE = re.compile(r"^([0-9A-Fa-f]{8}):\s*((?:[0-9A-Fa-f]{2}\s*)+)$")
PORT_BYTES_PER_READ = 256  # the 2C23T port's `mem <addr> <len>`: 32 bytes per line


def parse_mem_read(text: str, base: int, count: int) -> bytes:
    """Reassemble the bytes of one `mem read <base> <count>` reply.

    The shell prints `0xADDR: W W W W` with four little-endian words per line.
    Every declared address is checked against the running offset, so a dropped
    line raises instead of shifting every later word.
    """
    words: list[int] = []
    for line in text.splitlines():
        m = MEM_LINE_RE.match(line.strip())
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr != base + 4 * len(words):
            raise ValueError("mem read line at 0x%08X, expected 0x%08X (dropped line?)"
                             % (addr, base + 4 * len(words)))
        words.extend(int(w, 16) for w in m.group(2).split())
    if len(words) != count:
        raise ValueError("mem read returned %d words, expected %d" % (len(words), count))
    return b"".join(struct.pack("<I", w) for w in words)


class _Shell:
    """One shell exchange, terminated by the `>` prompt or by 0.3 s of silence.

    Not bench.Scope on purpose: that reads strictly to `>`, and the 2C23T port's
    shell -- the one reader measured to leave the block alone -- prints no
    prompt. A reply that stops mid-line is still an error downstream: both
    parsers check every declared address and the total length."""

    def __init__(self, port: str | None):
        import glob
        import serial  # pyserial

        port = port or next(iter(sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbmodem*"))), None)
        if port is None:
            raise RuntimeError("no /dev/ttyACM* or /dev/cu.usbmodem* found; pass --port")
        self._ser = serial.Serial(port, 115200, timeout=0.05)
        import time
        time.sleep(0.3)
        self._ser.read(1 << 20)

    def cmd(self, line: str, timeout: float = 3.0, quiet: float = 0.3) -> str:
        import time
        self._ser.reset_input_buffer()
        self._ser.write((line + "\r\n").encode())
        self._ser.flush()
        deadline = time.time() + timeout
        buf = bytearray()
        last = time.time()
        while time.time() < deadline:
            chunk = self._ser.read(8192)
            if chunk:
                buf += chunk
                last = time.time()
                if buf.rstrip().endswith(b">"):
                    return buf.decode("utf-8", "replace")
            elif buf and time.time() - last >= quiet:
                return buf.decode("utf-8", "replace")
            else:
                time.sleep(0.005)
        raise RuntimeError("no reply within %.1fs after %r (%d bytes)" % (timeout, line, len(buf)))

    def close(self) -> None:
        self._ser.close()


def parse_port_mem(text: str, base: int, count: int) -> bytes:
    """Reassemble one `mem <base> <count>` reply of the 2C23T port's shell:
    `ADDR:b0b1...` (32 bytes per line, no 0x prefix), same address check as above."""
    data = bytearray()
    for line in text.splitlines():
        m = PORT_LINE_RE.match(line.strip())
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr != base + len(data):
            raise ValueError("mem line at 0x%08X, expected 0x%08X (dropped line?)" % (addr, base + len(data)))
        data += bytes.fromhex(m.group(2).replace(" ", ""))
    if len(data) != count:
        raise ValueError("mem returned %d bytes, expected %d" % (len(data), count))
    return bytes(data)


def read_block(port: str | None) -> bytes:
    sc = _Shell(port)
    try:
        # Which shell? This firmware answers `mem read`; the 2C23T port answers `mem`.
        probe = sc.cmd("mem read %08X 4" % BASE, timeout=3.0)
        upstream = any(MEM_LINE_RE.match(l.strip()) for l in probe.splitlines())
        data = bytearray()
        if upstream:
            for addr in range(BASE, BASE + SIZE, WORDS_PER_READ * 4):
                reply = sc.cmd("mem read %08X %d" % (addr, WORDS_PER_READ), timeout=5.0)
                data += parse_mem_read(reply, addr, WORDS_PER_READ)
        else:
            for addr in range(BASE, BASE + SIZE, PORT_BYTES_PER_READ):
                reply = sc.cmd("mem %08X %d" % (addr, PORT_BYTES_PER_READ), timeout=5.0)
                data += parse_port_mem(reply, addr, PORT_BYTES_PER_READ)
    finally:
        sc.close()
    return bytes(data)


def read_block_hid() -> bytes:
    """Read the block through the OpenScope HID bootloader's diagnostic memory
    read (scripts/hid_flash.py), without booting any application firmware.
    UNTESTED on hardware as of 2026-09-13: unit #2 runs the factory IAP
    bootloader, which has no read command. Whether the HID bootloader's own
    startup leaves 0x20037000 alone is the thing to check first."""
    from hid_flash import open_bootloader_device, read_memory

    dev = open_bootloader_device()
    try:
        return read_memory(dev, BASE, SIZE)
    finally:
        dev.close()


def cfg_pins(cfgl: int, cfgh: int) -> list[str]:
    modes = []
    for pin in range(16):
        reg = cfgl if pin < 8 else cfgh
        nib = (reg >> (4 * (pin & 7))) & 0xF
        mode, cnf = nib & 3, nib >> 2
        if mode == 0:
            modes.append(["analog", "in-float", "in-pull", "in-?"][cnf])
        else:
            modes.append(["out-pp", "out-od", "af-pp", "af-od"][cnf])
    return modes


def decode(data: bytes) -> dict:
    if len(data) < SIZE:
        raise ValueError("block is %d bytes, expected %d" % (len(data), SIZE))
    magic, ticks, txn, gn = struct.unpack_from("<4I", data, 0)
    out = {
        "magic_ok": magic == MAGIC,
        "ticks": ticks,
        "tx_count": txn,
        "gpio_count": gn,
        "odr": dict(zip(PORTS, struct.unpack_from("<5H", data, 0x10))),
        "cfg": {},
        "tx": [],       # (tick, frame bytes, word) oldest first
        "events": [],   # (tick, kind, port, payload) oldest first
        "toggles": {},  # "B12": count
    }
    cfg = struct.unpack_from("<10I", data, 0x1C)
    for p in range(5):
        out["cfg"][PORTS[p]] = cfg_pins(cfg[2 * p], cfg[2 * p + 1])

    recs = []
    for i in range(min(txn, 32)):
        idx = (txn - min(txn, 32) + i) & 31
        r = data[0x60 + idx * 16: 0x60 + idx * 16 + 16]
        t, typ, ln = struct.unpack_from("<IBB", r, 0)
        frame = bytes(r[6:16])
        recs.append((t, frame, (frame[2] << 8) | frame[3]))
    out["tx"] = sorted(recs, key=lambda x: x[0])

    evs = []
    for i in range(min(gn, 64)):
        idx = (gn - min(gn, 64) + i) & 63
        r = data[0x260 + idx * 16: 0x260 + idx * 16 + 16]
        t, typ, port = struct.unpack_from("<IBB", r, 0)
        P = PORTS[port] if port < 5 else "?"
        if typ == 2:
            new, old = struct.unpack_from("<HH", r, 6)
            evs.append((t, "ODR", P, {"old": old, "new": new,
                                      "moved": ["%s%d%s" % (P, b, "^" if (new >> b) & 1 else "v")
                                                for b in range(16) if ((new ^ old) >> b) & 1]}))
        elif typ == 3:
            cfgl, cfgh = struct.unpack_from("<II", r, 8)
            evs.append((t, "CFG", P, {"cfglr": cfgl, "cfghr": cfgh, "pins": cfg_pins(cfgl, cfgh)}))
        else:
            evs.append((t, "?", P, {"raw": bytes(r)}))
    out["events"] = sorted(evs, key=lambda e: e[0])

    tog = struct.unpack_from("<80H", data, 0x660)
    out["toggles"] = {"%s%d" % (PORTS[i // 16], i % 16): c for i, c in enumerate(tog) if c}
    return out


def render(d: dict) -> str:
    L = []
    L.append("magic %s" % ("CAPL OK" if d["magic_ok"] else "BAD -- block not initialised or did not survive"))
    L.append("ticks %d  tx_count %d  gpio_events %d" % (d["ticks"], d["tx_count"], d["gpio_count"]))
    L.append("final ODR: " + "  ".join("%s=%04X" % (p, v) for p, v in d["odr"].items()))
    for p, modes in d["cfg"].items():
        L.append("final CFG %s: " % p + " ".join("%s%d:%s" % (p, i, m) for i, m in enumerate(modes)))
    L.append("")
    L.append("-- TX frames to the meter SoC (oldest first, last %d of %d) --" % (len(d["tx"]), d["tx_count"]))
    for t, f, w in d["tx"]:
        L.append("  t=%8d  %s   word 0x%04X" % (t, f.hex(" "), w))
    L.append("")
    L.append("-- GPIO events (oldest first, last %d of %d) --" % (len(d["events"]), d["gpio_count"]))
    for t, kind, P, pl in d["events"]:
        if kind == "ODR":
            L.append("  t=%8d  ODR %s %04X->%04X  %s" % (t, P, pl["old"], pl["new"], " ".join(pl["moved"])))
        elif kind == "CFG":
            L.append("  t=%8d  CFG %s CFGLR=%08X CFGHR=%08X  %s"
                     % (t, P, pl["cfglr"], pl["cfghr"], " ".join("%s%d:%s" % (P, i, m) for i, m in enumerate(pl["pins"]))))
        else:
            L.append("  t=%8d  ?? %s" % (t, pl["raw"].hex()))
    L.append("")
    L.append("-- ODR toggle counts per pin (non-zero) --")
    L.append("  " + (" ".join("%s=%d" % kv for kv in d["toggles"].items()) if d["toggles"] else "none"))
    return "\n".join(L)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", help="OpenScope CDC shell (default: autodetect)")
    ap.add_argument("--save", type=Path, help="write the raw 0x700-byte block here")
    ap.add_argument("--decode", type=Path, help="decode a saved block instead of reading the device")
    ap.add_argument("--hid", action="store_true",
                    help="read through the OpenScope HID bootloader (hid_flash.py) instead of a shell; untested")
    a = ap.parse_args(argv)
    if a.decode:
        data = a.decode.read_bytes()
    elif a.hid:
        data = read_block_hid()
    else:
        data = read_block(a.port)
        if a.save:
            a.save.write_bytes(data)
            print("saved %d bytes to %s" % (len(data), a.save))
    d = decode(data)
    print(render(d))
    return 0 if d["magic_ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
