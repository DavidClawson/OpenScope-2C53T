#!/usr/bin/env python3
"""Hold the OpenScope CDC port open and read `usbstat` after every reconnect.

The #39 evidence record (firmware/src/util/usb_evidence.c) is read in the
session AFTER the reset, so the host must be back on the port before the next
reset or power-off overwrites it. This script does the bench operator's part
of the rehearsal unattended:

  1. opens the port (DTR asserted) and sends `usbstat`. That completed write
     clears this session's PENDING flag (usb_ev_send_completed), which is what
     makes a reset done now the *clean* control. Note that `pending=` in any
     usbstat read over CDC is 0 by construction: the command's own echo
     completes a send before the field is formatted. `cleared_at=` next to it
     is the tick of that clearing send, so the log still shows how long the
     stall lasted, plus the session number and the counters;
  2. keeps reading until the port vanishes, logs the time, waits for the
     device to re-enumerate (listing only, no probe open: a DTR 1->0 makes the
     firmware drop its banner instead of stalling on it), opens it once and
     sends `usbstat`. The `previous session` line in that reply is the
     evidence; the current-session counters next to it (stalls, host_slow,
     last stall, dropped_closed) are produced by this tool's own timing
     against the boot banner and are not;
  3. keeps that same port open (no close/reopen, no second request), so the
     next event is captured the same way.

Everything goes to stdout and to --log with wall-clock timestamps. Stop with
Ctrl-C. Needs pyserial; the port is found by the Artery VCP VID:PID like
tools/openscope_host does (or pass --port).

    python3 scripts/bench_usb_evidence.py --log dumps/exp65_usbstat.log

Exit codes: 0 = --once and the reply carried a `previous session` line;
1 = no port / first open failed; 2 = the device did not come back within
--reconnect-timeout; 3 = --once but the reply had no `previous session` line
(nothing is inferred from the absence of an error); 130 = Ctrl-C.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

AT32_VCP = (0x2E3C, 0x5740)
PROMPT = b"usbstat\r"          # CR only, as tools/openscope_host sends (CRLF runs an empty line too)


def listed_ports() -> set:
    hits = set()
    for p in list_ports.comports():
        if (p.vid, p.pid) == AT32_VCP:
            dev = p.device
            if dev.startswith("/dev/tty."):          # macOS: prefer the cu.* node
                dev = "/dev/cu." + dev[len("/dev/tty."):]
            hits.add(dev)
    return hits


def find_port() -> str | None:
    hits = listed_ports()
    return sorted(hits)[0] if hits else None


class Log:
    def __init__(self, path: str | None):
        self.f = open(path, "a", encoding="utf-8") if path else None

    def __call__(self, msg: str) -> None:
        line = f"{datetime.now().isoformat(timespec='milliseconds')} {msg}"
        print(line, flush=True)
        if self.f:
            self.f.write(line + "\n")
            self.f.flush()

    def lines(self, data: bytes) -> None:
        for line in data.decode("ascii", "replace").splitlines():
            self(f"  < {line.rstrip()}")


def read_for(ser: serial.Serial, seconds: float) -> bytes:
    end = time.monotonic() + seconds
    buf = b""
    while time.monotonic() < end:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
    return buf


def wait_for_port(port_hint: str | None, deadline: float) -> str | None:
    """Wait until the device is enumerated again. Only LISTS (or stats) the
    node: a probe open here would raise and drop DTR before the real open, and
    the firmware treats DTR 1->0 as "host closed the port" and drops its banner
    instead of stalling on it (EXP-65 steps 2 and 3, sessions #5 and #1:
    dropped_closed=1, stalls=0). The observer must not touch the thing it
    observes until it means to."""
    while time.monotonic() < deadline:
        if port_hint:
            if os.path.exists(port_hint) or port_hint in listed_ports():
                return port_hint
        else:
            port = find_port()
            if port:
                return port
        time.sleep(0.2)
    return None


def query(ser: serial.Serial, log: Log, seconds: float) -> bytes:
    """One `usbstat`, reply logged and returned."""
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.write(PROMPT)
    out = read_for(ser, seconds)
    log.lines(out)
    return out


def hold(ser: serial.Serial, log: Log) -> None:
    """Read (and log) until the port goes away; returns on the exception."""
    try:
        while True:
            chunk = ser.read(4096)
            if chunk:
                log.lines(chunk)
    except Exception as e:                     # SerialException, OSError, termios.error
        log(f"port lost ({e.__class__.__name__}); waiting for re-enumeration")
    finally:
        try:
            ser.close()
        except Exception:
            pass


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", help="serial device (default: by USB VID:PID)")
    ap.add_argument("--log", help="append every line to this file too")
    ap.add_argument("--settle", type=float, default=1.5,
                    help="seconds to wait after the port reappears before opening it")
    ap.add_argument("--reconnect-timeout", type=float, default=120.0,
                    help="give up waiting for the port after this many seconds")
    ap.add_argument("--once", action="store_true",
                    help="exit after the first reconnect's usbstat")
    args = ap.parse_args()
    log = Log(args.log)

    port = args.port or find_port()
    if not port:
        log("no OpenScope port (VID:PID 2E3C:5740) found")
        return 1
    try:
        ser = serial.Serial(port, 115200, timeout=0.1, exclusive=True)
    except Exception as e:
        log(f"open {port} failed: {e}")
        return 1
    log(f"opened {port} (DTR asserted); reading this session's usbstat")
    try:
        query(ser, log, 2.0)
    except Exception as e:
        log(f"first usbstat failed ({e.__class__.__name__}); treating it as a port loss")
    log("holding the port open. Do the reset / power-off now.")
    hold(ser, log)

    events = 0
    while True:
        lost = time.monotonic()
        deadline = lost + args.reconnect_timeout
        reply = None
        while reply is None:
            new = wait_for_port(args.port, deadline)
            if not new:
                log("device did not come back; exiting")
                return 2
            log(f"port back after {time.monotonic() - lost:.1f} s; settling {args.settle} s")
            time.sleep(args.settle)
            port = new
            ser = None
            try:
                ser = serial.Serial(port, 115200, timeout=0.1, exclusive=True)
                reply = query(ser, log, 2.5)
            except Exception as e:
                log(f"open/usbstat on {port} failed ({e.__class__.__name__}: {e}); waiting again")
                if ser is not None:
                    try:
                        ser.close()
                    except Exception:
                        pass
                time.sleep(0.5)
        events += 1
        has_prev = b"previous session" in reply
        log(f"usbstat after event #{events} logged above"
            + ("" if has_prev else " -- NO `previous session` line in the reply"))
        if args.once:
            return 0 if has_prev else 3
        log("holding the port open. Do the reset / power-off now.")
        hold(ser, log)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
