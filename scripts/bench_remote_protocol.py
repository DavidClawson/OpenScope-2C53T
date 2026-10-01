#!/usr/bin/env python3
"""Bench validation of the remote protocol (#10) and the CDC health work (#39).

Runs on a unit flashed with a build of this branch and writes a report
(JSON + Markdown) to --out. Every step records what it observed; nothing is
inferred from the absence of an error.

  python3 scripts/bench_remote_protocol.py --out bench_out            # all steps
  python3 scripts/bench_remote_protocol.py --out bench_out --only info,press
  python3 scripts/bench_remote_protocol.py --out bench_out --soak-mb 16

Steps
  info     PING + STATUS; protocol major; build string
  crumbs   `fwcrumb` (install trail, EXP-57 follow-up) and `usbstat` baseline
  press    control: two screenshots with no press must match (else VOID);
           then BUTTON MENU, screenshot, BUTTON MENU, screenshot — images saved
  mode     acceptance criterion of remote_protocol.md §6: the operator changes
           mode on the device; STATUS must follow (interactive, --no-interactive skips)
  meter    10 GET_METER readings (meter mode) or the expected UNSUPPORTED_IN_MODE
  soak     #39: `flash dump` the W25Q for --soak-mb MB in 1 KB requests, first
           with `usbstat heal off` (the control: it must wedge) and then `heal on`; records stalls,
           host_slow, heals, reconnects and the endpoint register at the stall.
           With heal off a wedge needs a replug: the step then stops and says so.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "openscope_host"))
from openscope import proto  # noqa: E402
from openscope.device import Device, DeviceError, Nak, Timeout  # noqa: E402
from openscope.screen import png_bytes  # noqa: E402

STEPS = ("info", "crumbs", "press", "mode", "meter", "soak")


def shot(dev, out, name, log):
    s = dev.screenshot()
    path = os.path.join(out, f"{name}.png")
    with open(path, "wb") as f:
        f.write(png_bytes(s.w, s.h, s.indexed4, 2))
    log(f"  screenshot {name}: crc {zlib.crc32(s.indexed4) & 0xFFFFFFFF:08X}")
    return s.indexed4


def step_info(dev, a, r, log):
    st = dev.status()
    r["info"] = {"ping": dev.ping(), "proto": st.proto_version, "mode": st.mode_name,
                 "battery_known": st.battery_known, "battery_pct": st.battery_pct,
                 "battery_mv": st.battery_mv, "charging": st.charging,
                 "capture_ready": st.capture_ready, "uptime_ms": st.uptime_ms,
                 "usb_tx_stalls": st.usb_tx_stalls, "usb_heals": st.usb_heals}
    log(f"  {r['info']}")
    return st.proto_version == proto.PROTO_MAJOR


def step_crumbs(dev, a, r, log):
    r["crumbs"] = {"fwcrumb": dev.shell("fwcrumb"), "usbstat": dev.shell("usbstat")}
    log("  " + r["crumbs"]["fwcrumb"].replace("\n", "\n  "))
    log("  " + r["crumbs"]["usbstat"].replace("\n", "\n  "))
    return True


def step_press(dev, a, r, log):
    # Control first (EXP-60): with NO press the framebuffer must not change,
    # else a difference after MENU proves nothing (live trace). VOID then.
    c0 = shot(dev, a.out, "press_control_a", log)
    time.sleep(0.6)
    c1 = shot(dev, a.out, "press_control_b", log)
    if c0 != c1:
        r["press"] = {"void": "screen changes without a press (live trace?) - use a static screen"}
        log("  CONTROL FAILED: screen changes on its own -> VOID (switch to a static screen)")
        return False
    f0 = c1
    dev.press("MENU")
    time.sleep(0.6)
    f1 = shot(dev, a.out, "press_1_after_menu", log)
    dev.press("MENU")
    time.sleep(0.6)
    f2 = shot(dev, a.out, "press_2_after_menu_again", log)
    changed = f0 != f1
    r["press"] = {"frame_changed_after_menu": changed, "back_to_start": f2 == f0}
    log(f"  MENU changed the screen: {changed}; second MENU back to the first frame: {f2 == f0}")
    return changed


def step_mode(dev, a, r, log):
    if a.no_interactive:
        r["mode"] = {"skipped": "non-interactive"}
        return True
    start = dev.status().mode_name
    input(f"  Now in '{start}'. Change the mode ON THE DEVICE (buttons), then press Enter here... ")
    seen = []
    deadline = time.time() + 30
    while time.time() < deadline:
        m = dev.status().mode_name
        if not seen or seen[-1] != m:
            seen.append(m)
        if m != start:
            break
        time.sleep(0.5)
    r["mode"] = {"start": start, "seen": seen, "followed": seen[-1] != start}
    log(f"  STATUS modes seen: {seen}")
    return r["mode"]["followed"]


def step_meter(dev, a, r, log):
    if not a.no_interactive:
        ref = input("  Reference: external DMM reading of the same source (blank to skip): ").strip()
        r["meter_reference"] = ref or None
    readings = []
    for _ in range(10):
        try:
            m = dev.meter()
            readings.append({"count": m.update_count, "value": m.value, "display": m.display,
                             "unit": m.unit, "raw_bcd": m.raw_bcd, "result": m.result})
        except Nak as e:
            readings.append({"nak": proto.ERRORS.get(e.code, hex(e.code))})
        time.sleep(0.3)
    r["meter"] = readings
    for x in readings[:3]:
        log(f"  {x}")
    naks = {x.get("nak") for x in readings if "nak" in x}
    counts = [x["count"] for x in readings if "count" in x]
    if naks == {"UNSUPPORTED_IN_MODE"} and not counts:
        log("  not in meter mode: refused as expected (never a frozen reading)")
        return True
    fresh = len(set(counts)) > 1
    log(f"  update_count advanced: {fresh}")
    return fresh


def _flash_dump(dev, addr, n, deadline_s=4.0):
    ser = dev.link._ser
    ser.reset_input_buffer()
    dev.link.write(f"flash dump 0x{addr:06X} {n}\r".encode())
    buf = bytearray()
    t0 = time.time()
    while b"FLASHDUMP" not in buf:
        if time.time() - t0 > deadline_s:
            raise Timeout(f"no FLASHDUMP header (tail {bytes(buf[-20:])!r})")
        buf += dev.link.read()
    rest = bytearray(buf[buf.index(b"FLASHDUMP"):])
    while b"\n" not in rest:
        rest += dev.link.read()
    data = bytearray(rest[rest.index(b"\n") + 1:])
    while len(data) < n:
        if time.time() - t0 > deadline_s:
            raise Timeout(f"short {len(data)}/{n}")
        data += dev.link.read(n - len(data))
    return bytes(data[:n])


def soak_once(dev, a, heal, log):
    dev.shell(f"usbstat heal {'on' if heal else 'off'}")
    before = dev.shell("usbstat")
    total = a.soak_mb << 20
    t0 = time.time()
    addr, reconnects, failures = 0, 0, []
    while addr < total:
        try:
            _flash_dump(dev, addr, 1024)
            addr += 1024
        except (Timeout, OSError) as e:
            failures.append({"addr": addr, "error": str(e), "t": round(time.time() - t0, 1)})
            log(f"  stall at 0x{addr:06X}: {e}")
            try:
                dev._reopen()                    # a heal looks like a replug
                reconnects += 1
                dev.shell(f"usbstat heal {'on' if heal else 'off'}")
            except Exception as e2:
                log(f"  device did not come back ({e2}); heal={'on' if heal else 'off'} -> replug needed")
                return {"heal": heal, "reached": addr, "failures": failures, "reconnects": reconnects,
                        "recovered": False, "usbstat_before": before}
        if addr % (1 << 20) == 0:
            log(f"  0x{addr:06X}  {addr / max(1e-3, time.time() - t0) / 1024:.0f} KiB/s")
    return {"heal": heal, "reached": addr, "failures": failures, "reconnects": reconnects,
            "recovered": True, "seconds": round(time.time() - t0, 1),
            "usbstat_before": before, "usbstat_after": dev.shell("usbstat")}


def step_soak(dev, a, r, log):
    r["soak"] = []
    # Control first (EXP-61): heal OFF must reproduce the wedge through this
    # path, else the heal-ON run has nothing to heal and is VOID, not a success.
    for heal in (False, True):
        log(f"  soak {a.soak_mb} MB with heal {'on' if heal else 'off'}")
        res = soak_once(dev, a, heal, log)
        r["soak"].append(res)
        if not heal and not res["failures"]:
            log("  CONTROL FAILED: no wedge with heal off -> the heal-on run would be VOID; stopping")
            r["soak_void"] = True
            return False
        if not res["recovered"]:
            if not heal and not a.no_interactive:
                input("  Wedged with heal off (the control held). Replug USB + reset the scope, "
                      "wait for OpenScope, then press Enter... ")
                dev._reopen()
                continue
            break
    return bool(r["soak"]) and r["soak"][-1]["heal"] and r["soak"][-1]["recovered"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--port")
    ap.add_argument("--only", help="comma-separated subset of: " + ",".join(STEPS))
    ap.add_argument("--soak-mb", type=int, default=16)
    ap.add_argument("--no-interactive", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    steps = a.only.split(",") if a.only else list(STEPS)
    lines = []

    def log(m):
        print(m, flush=True)
        lines.append(m)

    r = {"started_unix": time.time(), "steps": {}}
    dev = Device.open(a.port)
    try:
        for name in steps:
            log(f"== {name}")
            try:
                ok = globals()[f"step_{name}"](dev, a, r, log)
            except (DeviceError, proto.ProtocolError, OSError) as e:
                ok = False
                log(f"  ERROR {e}")
            r["steps"][name] = "PASS" if ok else "FAIL"
            log(f"  -> {r['steps'][name]}")
    finally:
        dev.close()
        with open(os.path.join(a.out, "bench.json"), "w") as f:
            json.dump(r, f, indent=2, default=str)
        with open(os.path.join(a.out, "bench.md"), "w") as f:
            f.write("# Remote protocol bench\n\n| step | result |\n|---|---|\n")
            f.writelines(f"| {k} | {v} |\n" for k, v in r["steps"].items())
            f.write("\n```\n" + "\n".join(lines) + "\n```\n")
    return 0 if all(v == "PASS" for v in r["steps"].values()) else 1


if __name__ == "__main__":
    sys.exit(main())
