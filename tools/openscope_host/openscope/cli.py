"""`openscope` command line (remote_protocol.md §4.2).

Every subcommand exits non-zero with one readable line when the device is
absent or refuses — no tracebacks for expected failures (§4.2)."""
from __future__ import annotations

import argparse
import sys
from typing import List, Optional

from . import proto
from .device import Device, DeviceError, Nak
from .link import NoDevice, candidate_ports, looked_where

EXIT_OK, EXIT_NO_DEVICE, EXIT_DEVICE_ERROR, EXIT_USAGE = 0, 1, 2, 64


def _info(dev: Device, _a) -> int:
    version = dev.ping()
    st = dev.status()
    batt = f"{st.battery_pct}% ({st.battery_mv} mV{', charging' if st.charging else ''}"
    batt += ", CRITICAL)" if st.battery_critical else ")"
    print(f"port      {dev.link.port}")
    print(f"firmware  {version}")
    print(f"protocol  v{st.proto_version}")
    print(f"mode      {st.mode_name}")
    print(f"battery   {batt}")
    print(f"capture   {'real samples' if st.capture_ready else 'no capture data yet'}")
    print(f"uptime    {st.uptime_ms / 1000:.1f} s")
    print(f"usb       {st.usb_tx_stalls} TX stalls, {st.usb_heals} self-heals")
    return EXIT_OK


def _press(dev: Device, a) -> int:
    for b in a.buttons:
        dev.press(b)
        print(f"pressed {b.upper()}")
    return EXIT_OK


def _shell(dev: Device, a) -> int:
    print(dev.shell(" ".join(a.line), timeout=a.timeout))
    return EXIT_OK


def _screenshot(dev: Device, a) -> int:
    from .screen import save_png
    s = dev.screenshot()
    save_png(a.out, s.w, s.h, s.indexed4, scale=a.scale)
    print(f"saved {a.out} ({s.w}x{s.h}, CRC verified)")
    return EXIT_OK


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="openscope", description="Drive an OpenScope 2C53T over USB.")
    ap.add_argument("--port", help="serial port (default: auto-detect by USB VID:PID)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports", help="list candidate ports")
    sub.add_parser("info", help="firmware version, mode, battery, USB health")
    p = sub.add_parser("press", help="inject button presses, e.g. `press MENU OK`")
    p.add_argument("buttons", nargs="+", metavar="BUTTON",
                   help=" | ".join(proto.BUTTONS))
    p = sub.add_parser("shell", help="run one ASCII debug-shell command")
    p.add_argument("line", nargs="+")
    p.add_argument("--timeout", type=float, default=3.0)
    p = sub.add_parser("screenshot", help="save the device screen as PNG (needs Pillow)")
    p.add_argument("out")
    p.add_argument("--scale", type=int, default=2)
    return ap


def main(argv: Optional[List[str]] = None) -> int:
    a = build_parser().parse_args(argv)
    if a.cmd == "ports":
        ports = candidate_ports()
        if not ports:
            print(f"no OpenScope found on {looked_where()}")
            return EXIT_NO_DEVICE
        for i, p in enumerate(ports):
            print(p + ("   <- would use" if i == 0 else ""))
        return EXIT_OK

    handlers = {"info": _info, "press": _press, "shell": _shell, "screenshot": _screenshot}
    try:
        dev = Device.open(a.port)
    except NoDevice as e:
        print(str(e), file=sys.stderr)
        return EXIT_NO_DEVICE
    try:
        return handlers[a.cmd](dev, a)
    except Nak as e:
        print(str(e), file=sys.stderr)
        return EXIT_DEVICE_ERROR
    except (DeviceError, proto.ProtocolError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT_DEVICE_ERROR
    finally:
        dev.close()
