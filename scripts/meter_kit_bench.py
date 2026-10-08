#!/usr/bin/env python3
"""Meter acceptance against a component kit: one capture per part.

Dev plan 2.4 ("the eight untested meter words on unit #1") and the open
resistance-decade question (#37: does frame[8] bit 7 mean "four decimals" in
ohms on a V1.4 unit?) both need known parts on the meter leads. A kit gives
NOMINAL values with a tolerance, not calibrated ones, so this judges what a
tolerance can judge -- decade, unit, sign, decoding -- and records the raw
frame next to every reading so a later decoder change can be tested against
exactly what the meter sent.

One capture per call, because a human swaps the parts:

  python3 scripts/meter_kit_bench.py res 1000 --tol 5 --label "1k 5% brown-black-red-gold"
  python3 scripts/meter_kit_bench.py cap 100e-9 --tol 20 --label "104 ceramic"
  python3 scripts/meter_kit_bench.py diode 0.6 --tol 25 --label "1N4148"
  python3 scripts/meter_kit_bench.py dcv -1.5 --tol 20 --label "AA cell, leads REVERSED"
  python3 scripts/meter_kit_bench.py cont 0 --label "leads shorted"
  python3 scripts/meter_kit_bench.py summary

Functions -> UI submode: dcv 0, res 6, cont 7, diode 8, cap 9.
Stable = the same display on 3 consecutive fresh frames (the meter sends
~4/s). Capacitors settle slowly; the default wait is longer for them.
Log: reverse_engineering/captures/meter_kit/<date>.jsonl (one JSON per capture).
"""
import argparse
import datetime
import json
import re
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from bench import Scope  # noqa: E402

FUNCS = {"dcv": (0, "V"), "res": (6, "Ohm"), "cont": (7, "Ohm"), "diode": (8, "V"), "cap": (9, "F")}
SCALE = {"": 1.0, "V": 1.0, "mV": 1e-3, "Ohm": 1.0, "kOhm": 1e3, "MOhm": 1e6,
         "F": 1.0, "mF": 1e-3, "uF": 1e-6, "nF": 1e-9, "pF": 1e-12}
LOG_DIR = ROOT / "reverse_engineering/captures/meter_kit"


def dump(sc):
    r = sc.cmd("meter dump", timeout=8.0)
    g = lambda pat: (re.search(pat, r) or [None, None])[1]
    return {
        "updates": int(g(r"updates=(\d+)") or 0),
        "valid": g(r"\bvalid=(\d)") == "1",
        "display": g(r"display=(\S+)"),
        "unit": g(r"display=\S+ unit=(\S*)") or "",
        "submode": int(g(r"reading_submode=(\d+)") or -1),
        "reject": int(g(r"reject=(\d+)") or 0),
        "frame": g(r"frame=([0-9A-F ]+)\r?\n"),
        "negative": g(r"negative=(\d)") == "1",
    }


def as_si(display, unit):
    """Decoded display + unit -> SI value, or None for OL/blank/unknown."""
    if display is None or not re.fullmatch(r"-?\d+(\.\d+)?", display):
        return None
    return float(display) * SCALE.get(unit, float("nan"))


def capture(sc, func, settle, need=3, timeout=30.0):
    sub, _ = FUNCS[func]
    sc.cmd("mode meter %d" % sub)
    time.sleep(settle)
    seen, last_upd, t0 = [], -1, time.time()
    while time.time() - t0 < timeout:
        d = dump(sc)
        if d["updates"] != last_upd:                 # a fresh frame
            last_upd = d["updates"]
            seen.append(d)
            tail = seen[-need:]
            if len(tail) == need and all(x["display"] == tail[0]["display"] and
                                         x["unit"] == tail[0]["unit"] for x in tail):
                return tail[-1], seen
        time.sleep(0.3)
    return (seen[-1] if seen else None), seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("func", choices=list(FUNCS) + ["summary"])
    ap.add_argument("nominal", nargs="?", type=float, help="SI units: ohms, farads, volts")
    ap.add_argument("--tol", type=float, default=5.0, help="tolerance, percent")
    ap.add_argument("--abs", type=float, default=None, help="absolute tolerance (SI), e.g. 0.5 ohm for continuity")
    ap.add_argument("--label", default="")
    ap.add_argument("--settle", type=float, default=None)
    ap.add_argument("--scope", default="/dev/ttyACM0")
    a = ap.parse_args()

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    log = LOG_DIR / (datetime.date.today().isoformat() + ".jsonl")
    if a.func == "summary":
        rows = [json.loads(l) for l in log.read_text().splitlines()] if log.exists() else []
        for r in rows:
            print("%-5s %-6s %-30s nominal %-10s read %-9s %-5s  %s" % (
                r["verdict"], r["func"], r["label"][:30], "%g" % r["nominal"],
                r["display"], r["unit"], r["frame"]))
        print("%d/%d PASS" % (sum(r["verdict"] == "PASS" for r in rows), len(rows)))
        return 0
    if a.nominal is None:
        ap.error("nominal value required")

    sc = Scope(a.scope)
    build = next((l.strip() for l in sc.version().splitlines() if l.startswith("Build:")), "?")
    settle = a.settle if a.settle is not None else (8.0 if a.func == "cap" else 3.0)
    d, seen = capture(sc, a.func, settle, timeout=60.0 if a.func == "cap" else 30.0)
    sc.close()
    if d is None:
        print("no frames arrived -- is the meter mode live?")
        return 2

    val = as_si(d["display"], d["unit"])
    tol = a.abs if a.abs is not None else abs(a.nominal) * a.tol / 100.0
    if val is None:
        verdict, why = "FAIL", "no numeric reading (%s)" % d["display"]
    elif abs(val - a.nominal) <= tol:
        verdict, why = "PASS", "within %g" % tol
    else:
        ratio = (val / a.nominal) if a.nominal else float("inf")
        hint = ""
        for k in (10.0, 0.1, 1000.0, 0.001, -1.0):
            if a.nominal and abs(ratio - k) / abs(k) < 0.05:
                hint = "  <-- x%g: decade/unit/sign error, not tolerance" % k
        verdict, why = "FAIL", "off by %+.1f%%%s" % ((ratio - 1) * 100 if a.nominal else 0, hint)

    rec = dict(time=datetime.datetime.now().isoformat(timespec="seconds"), build=build,
               func=a.func, label=a.label, nominal=a.nominal, tol=tol, display=d["display"],
               unit=d["unit"], value=val, negative=d["negative"], reject=d["reject"],
               frame=d["frame"], frames_seen=len(seen), verdict=verdict)
    with log.open("a") as f:
        f.write(json.dumps(rec) + "\n")
    print("%s  %s %s  read %s %s (= %s)  nominal %g +/- %g  %s\n      frame %s" % (
        verdict, a.func, a.label, d["display"], d["unit"], "%g" % val if val is not None else "-",
        a.nominal, tol, why, d["frame"]))
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
