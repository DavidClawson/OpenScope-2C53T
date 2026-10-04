#!/usr/bin/env python3
"""Bench acceptance for waveform pass/fail masks (docs/specs/scope/mask-pass-fail.md).

Drives unit #1 over the CDC shell (`mask ...`) and the JDS6600 as the source.
Setup mirrors the exp22 trigger block that produced the host-test fixture:
range 5 CH1, trigger level 0, NORMAL, timebase 0x10, 201.2 Hz sine, 2.0 Vpp.

PASS CRITERIA ARE FIXED HERE, BEFORE ANY RUN, and each acceptance has a
negative control that must come out the other way:

  A  teach      8 records -> READY within 15 s, teach spread <= 16 counts
  B  baseline   >= 30 records tested; false-fail rate <= 1/30;
                and >= 80% of committed records judged (skips reported)
  C  amplitude  2.6 Vpp (+30%): >= 95% of tested FAIL      <- control for B
     recovery   back to 2.0 Vpp: >= 95% of tested PASS
  D  frequency  211.3 Hz (+5%): >= 95% of tested FAIL
                (at the span ends, 2.6 periods from the trigger, +5% moves the
                waveform ~8 samples -- 4x the default 2-sample slack)
  E  settings   trigger level moved: tested count does not move, SKIPs carry
                "settings"; level restored -> testing resumes
  F  hold       stop-on-fail ON + amplitude fault: hold within 3 s, NO record
                judged while held (acquisition frozen) for 2 s, `mask run`
                releases, judging resumes
                (run 2, 2026-10-03, found the first record after release was
                the FPGA's held fault-period capture and re-held; the firmware
                now SKIPs it as "stale" -- the criterion is unchanged)
  G  health     `missed` stays 0 throughout (every commit judged once)
  H  buttons    (shell `btn` injects into the key-scan queue) AUTO teaches to
                READY; MOVE x2 reaches Position, where RIGHT moves the trigger
                column and does NOT touch the mask (control); one more MOVE
                reaches Mask, where UP x2 makes tol_v 12, RIGHT makes tol_h 3
                and the trigger column does NOT move; SELECT toggles stop-on-
                fail; a held fail is released by OK; AUTO clears to EMPTY

Usage:  python3 scripts/mask_bench.py [--scope /dev/ttyACM0] [--jds /dev/ttyUSB0]
Writes a log next to the other EXP captures when --log is given.
"""
import argparse
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench import Scope, JDS6600  # noqa: E402

AMP = 2.0
FREQ = 201.2


def status(sc):
    r = sc.cmd("mask status", timeout=5.0)
    d = {"raw": r}
    # Anchor on the state word: the shell echoes the command line first, and
    # the first run of this script parsed "mask status" as state "status".
    m = re.search(r"^mask (EMPTY|TEACHING|READY)\b", r, re.M)
    d["state"] = m.group(1) if m else "?"
    m = re.search(r"tested (\d+)\s+pass (\d+)\s+fail (\d+)\s+skipped (\d+)\s+missed (\d+)", r)
    if m:
        d["tested"], d["pass"], d["fail"], d["skip"], d["missed"] = map(int, m.groups())
    m = re.search(r"teach (\d+)/(\d+)", r)
    if m:
        d["teach_got"], d["teach_target"] = map(int, m.groups())
    m = re.search(r"spread (\d+) cnt", r)
    d["spread"] = int(m.group(1)) if m else None
    d["hold"] = "HOLDING" in r
    m = re.search(r"skips:(.*)", r)
    d["skips"] = m.group(1).strip() if m else ""
    m = re.search(r"heap free (\d+)", r)
    d["heap"] = int(m.group(1)) if m else None
    m = re.search(r"tol v=(\d+) cnt h=(\d+)", r)
    d["tol"] = tuple(map(int, m.groups())) if m else None
    m = re.search(r"move-focus (\S+)", r)
    d["focus"] = m.group(1) if m else None
    d["stop"] = "stop-on-fail ON" in r
    return d


def hpos(sc):
    m = re.search(r"asked column (\d+)", sc.cmd("fpga scope hpos"))
    return int(m.group(1)) if m else None


def window(sc, n_tested, timeout):
    """Reset counts, then wait until n_tested records were judged (or timeout).
    Returns the final status."""
    sc.cmd("mask reset")
    t0 = time.time()
    s = status(sc)
    while time.time() - t0 < timeout:
        time.sleep(1.0)
        s = status(sc)
        if s.get("tested", 0) >= n_tested:
            break
    return s


def line(tag, ok, msg):
    print("  %s  %-10s %s" % ("PASS" if ok else "FAIL", tag, msg))
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scope", default="/dev/ttyACM0")
    ap.add_argument("--jds", default="/dev/ttyUSB0")
    ap.add_argument("--n", type=int, default=40, help="records per window")
    a = ap.parse_args()

    sc = Scope(a.scope)
    sg = JDS6600(a.jds)
    build = next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?")
    print("device %s" % build)
    if "mask" not in sc.cmd("help", timeout=5.0):
        print("this image has no `mask` command -- flash the mask build first")
        return 2

    print("setup: range 5 CH1, level 0, NORMAL, timebase 0x10; JDS CH1 %.1f Hz %.1f Vpp"
          % (FREQ, AMP))
    sc.trigger_mode("normal")
    sc.trigger_level(0)
    sc.timebase(0x10)
    sc.vdiv(1, 5)
    sc.cmd("fpga scope hpos 160")
    sg.waveform("sine", 1); sg.freq(FREQ, 1); sg.amp(AMP, 1); sg.offset(0.0, 1)
    sg.output(True, False)
    time.sleep(1.5)
    results = []

    # A -- teach
    sc.cmd("mask clear")
    sc.cmd("mask stop off")
    sc.cmd("mask tol 8 2")
    print(sc.cmd("mask teach 8 ch1", timeout=5.0).strip())
    t0 = time.time()
    s = status(sc)
    while s["state"] != "READY" and time.time() - t0 < 15:
        time.sleep(0.5)
        s = status(sc)
    results.append(line("A teach", s["state"] == "READY" and (s["spread"] or 99) <= 16,
                        "state %s spread %s heap free %s (%.1f s)"
                        % (s["state"], s["spread"], s["heap"], time.time() - t0)))
    if s["state"] != "READY":
        print(s["raw"])
        return 1

    # B -- baseline
    s = window(sc, a.n, 60)
    judged = s["tested"] + s["skip"]
    results.append(line("B base", s["tested"] >= 30 and s["fail"] * 30 <= s["tested"]
                        and s["tested"] >= 0.8 * judged,
                        "tested %d pass %d fail %d skip %d [%s]"
                        % (s["tested"], s["pass"], s["fail"], s["skip"], s["skips"])))
    missed_ok = s["missed"] == 0

    # C -- amplitude fault and recovery
    sg.amp(AMP * 1.3, 1); time.sleep(0.8)
    s = window(sc, 20, 40)
    results.append(line("C amp+30%", s["tested"] >= 10 and s["fail"] >= 0.95 * s["tested"],
                        "tested %d fail %d" % (s["tested"], s["fail"])))
    sg.amp(AMP, 1); time.sleep(0.8)
    s = window(sc, 20, 40)
    results.append(line("C recover", s["tested"] >= 10 and s["pass"] >= 0.95 * s["tested"],
                        "tested %d pass %d" % (s["tested"], s["pass"])))
    missed_ok &= s["missed"] == 0

    # D -- frequency fault
    sg.freq(FREQ * 1.05, 1); time.sleep(0.8)
    s = window(sc, 20, 40)
    results.append(line("D freq+5%", s["tested"] >= 10 and s["fail"] >= 0.95 * s["tested"],
                        "tested %d fail %d" % (s["tested"], s["fail"])))
    sg.freq(FREQ, 1); time.sleep(0.8)

    # E -- settings change is refused, not scored
    sc.trigger_level(10); time.sleep(0.8)
    sc.cmd("mask reset")
    time.sleep(4.0)
    s = status(sc)
    results.append(line("E settings", s["tested"] == 0 and "settings" in s["skips"],
                        "tested %d skip %d [%s]" % (s["tested"], s["skip"], s["skips"])))
    sc.trigger_level(0); time.sleep(0.8)
    s = window(sc, 10, 30)
    results.append(line("E resume", s["tested"] >= 5 and s["pass"] >= 0.95 * s["tested"],
                        "tested %d pass %d" % (s["tested"], s["pass"])))

    # F -- stop on fail holds the failing record
    sc.cmd("mask stop on")
    sc.cmd("mask reset")
    sg.amp(AMP * 1.3, 1)
    t0 = time.time()
    s = status(sc)
    while not s["hold"] and time.time() - t0 < 3.0:
        time.sleep(0.2)
        s = status(sc)
    held = s["hold"]
    n0 = s.get("tested", 0) + s.get("skip", 0)
    time.sleep(2.0)
    s2 = status(sc)
    n1 = s2.get("tested", 0) + s2.get("skip", 0)
    sg.amp(AMP, 1); time.sleep(0.5)
    sc.cmd("mask run")
    time.sleep(3.0)
    s3 = status(sc)
    n2 = s3.get("tested", 0) + s3.get("skip", 0)
    results.append(line("F hold", held and n1 == n0 and n2 > n1 and not s3["hold"],
                        "held %s; judged %d -> %d while held, %d after release; still held %s [%s]"
                        % (held, n0, n1, n2, s3["hold"], s3["skips"])))
    sc.cmd("mask stop off")
    missed_ok &= s3["missed"] == 0

    # H -- the same controls from the buttons
    sc.cmd("mask clear"); sc.cmd("mask stop off"); sc.cmd("mask tol 8 2")
    for _ in range(4):                               # normalise the MOVE stage
        if status(sc)["focus"] == "vdiv":
            break
        sc.cmd("btn move")
    sc.cmd("btn auto")
    t0 = time.time()
    s = status(sc)
    while s["state"] != "READY" and time.time() - t0 < 15:
        time.sleep(0.5)
        s = status(sc)
    h_ok = s["state"] == "READY"
    msg = ["auto->%s" % s["state"]]
    sc.cmd("btn move 2")                            # vdiv -> trig -> position
    s = status(sc); x0 = hpos(sc)
    sc.cmd("btn right")
    s2 = status(sc); x1 = hpos(sc)
    ctl = s["focus"] == "position" and x1 == (x0 or 0) + 16 and s2["tol"] == (8, 2)
    msg.append("position: col %s->%s tol %s" % (x0, x1, s2["tol"]))
    sc.cmd("btn left")                               # put the column back
    sc.cmd("btn move")
    s = status(sc)
    sc.cmd("btn up 2"); sc.cmd("btn right")
    s2 = status(sc); x2 = hpos(sc)
    msk = s["focus"] == "mask" and s2["tol"] == (12, 3) and x2 == x0
    msg.append("mask focus: tol %s col %s" % (s2["tol"], x2))
    sc.cmd("btn select")
    stop_on = status(sc)["stop"]
    msg.append("select->stop %s" % stop_on)
    sg.amp(AMP * 1.3, 1)
    t0 = time.time()
    while not status(sc)["hold"] and time.time() - t0 < 3.0:
        time.sleep(0.2)
    held = status(sc)["hold"]
    sg.amp(AMP, 1); time.sleep(0.5)
    sc.cmd("btn ok"); time.sleep(0.5)
    released = not status(sc)["hold"]
    msg.append("ok: held %s released %s" % (held, released))
    sc.cmd("btn auto")
    s = status(sc)
    msg.append("auto->%s focus %s" % (s["state"], s["focus"]))
    h_ok = h_ok and ctl and msk and stop_on and held and released and \
        s["state"] == "EMPTY" and s["focus"] != "mask"
    sc.cmd("mask stop off"); sc.cmd("mask tol 8 2")
    results.append(line("H buttons", h_ok, "; ".join(msg)))

    results.append(line("G health", missed_ok, "missed commits 0 throughout" if missed_ok
                        else "a commit went unjudged"))

    print("\n" + status(sc)["raw"].strip())
    print("\n%d/%d criteria met" % (sum(results), len(results)))
    sg.output(False, False)
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
