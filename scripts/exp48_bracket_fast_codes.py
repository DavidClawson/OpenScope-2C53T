#!/usr/bin/env python3
"""EXP-48 -- the re-arm bracket at fast timebases, and whether a reg-01 write clears it.

WHY
    Stock's June capture reads a 04/05 pair every 28.9 ms at timebase 8 and
    every frame is fresh (re-decoded at the correct edge: 0/346 consecutive
    same-channel frames identical). But its trigger level (0xAD = 173) sat
    above its flat input (165-168): no trigger ever fired, so those were
    free-run reads of a rolling buffer, not triggered captures. Whether a
    TRIGGERED cadence can approach 29 ms is unmeasured.

PREDICTIONS (fill + ~190 ms model, EXP-46; written before the run)
    A. NORMAL sustain threshold vs post-edge delay: at 0x10 (fill 82 ms)
       frozen at 210, sustains at 300 (control, reproduces EXP-46). At
       0x0D/0x0C/0x0A/0x08 (fill <= 8 ms) frozen at <= 150, sustains by 210.
       Falsifier: a fast code sustaining at 1 ms => the constant is not a
       fixed clock and stock-like cadence is reachable by timebase alone.
    B. Shell probes with the task parked (SINGLE after its one record):
       read -> +1 edge; read, read 30-60 ms later -> +1 (second inside the
       bracket); read, `01 10`, read -> +1 if a reg-01 write does NOT clear
       the holdoff, +2 if it does; same for stock's `01 11; 01 10` pair and
       for `08 80`; read, 350 ms, read -> +2 (bracket passed).
    C. Stock's condition on our firmware: level +100, AUTO autowait 25 at
       0x08 -> pairs at ~25-30/s, 0 edges, consecutive frames differ.
"""
import os, re, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench import Scope, JDS6600                         # noqa: E402
from exp22_stability import grab_frame                    # noqa: E402

F_DRIVE = 201.2

def counters(sc):
    t = sc.cmd("status", timeout=6.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))

def normal_arm(sc, n=4):
    sc.cmd("fpga scope trigmode auto"); time.sleep(0.3)
    sc.cmd("fpga scope trigmode normal"); time.sleep(1.2)
    e0, o0 = counters(sc); g = [grab_frame(sc)["gen"] for _ in range(n)]; e1, o1 = counters(sc)
    return (not np.all(np.diff(g) == 0)), e1 - e0, o1 - o0

def set_tb(sc, tb):
    r = sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0)
    assert "NOT SET" not in r and "usage" not in r, r
    time.sleep(0.4)

def main():
    jds = JDS6600("/dev/ttyUSB0")
    jds.output(False, False); jds.waveform("sine", 1); jds.freq(F_DRIVE, 1); jds.amp(3.0, 1); jds.offset(0.0, 1)
    jds.output(True, False); jds.close()
    sc = Scope("/dev/ttyACM0")
    print("EXP-48  device:", next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?"))
    sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga acqgate off"); sc.cmd("fpga rearm off")
    sc.cmd("fpga pairgap 0"); sc.cmd("fpga acqbr off"); sc.cmd("fpga autowait 0")

    print("\n=== A. NORMAL sustain vs post-edge delay (ms); sweep stops after the first sustain + one confirm ===")
    for tb in (0x10, 0x0D, 0x0C, 0x0A, 0x08):
        sc.cmd("fpga scope trigmode auto"); set_tb(sc, tb)
        row = []; hits = 0
        for pe in (1, 60, 120, 150, 180, 210, 250, 300):
            r = sc.cmd("fpga postedge %d" % pe); assert re.search(r"delay %d ms" % pe, r), r
            ok, de, do = normal_arm(sc)
            row.append("%d:%s(+%d/+%d)" % (pe, "OK" if ok else "frz", de, do))
            if ok:
                hits += 1
                if hits >= 2: break
        print("  tb 0x%02X  %s" % (tb, "  ".join(row)))

    print("\n=== B. shell probes, task parked (SINGLE after one record), 0x10, postedge derived ===")
    sc.cmd("fpga postedge 0"); sc.cmd("fpga scope trigmode auto"); set_tb(sc, 0x10)
    sc.cmd("fpga scope trigmode single"); time.sleep(2.5)
    g = [grab_frame(sc)["gen"] for _ in range(2)]; print("  parked: gen %s (%s)" % (g, "held" if g[0] == g[1] else "NOT HELD"))
    def probe(label, actions):
        time.sleep(1.5); e0, o0 = counters(sc); ts = []
        for a in actions:
            if a == "read": t = time.time(); sc.opread_stats(0x04); ts.append((t, time.time()))
            elif a.startswith("wait"): time.sleep(float(a.split()[1]) / 1000.0)
            else: sc.cmd("spi3 seq " + a)
        time.sleep(0.6); e1, o1 = counters(sc)
        gap = ("%.0f ms read1-end -> read2-start" % ((ts[1][0] - ts[0][1]) * 1000)) if len(ts) > 1 else ""
        print("  %-34s edges +%d  (OK +%d)  %s" % (label, e1 - e0, o1 - o0, gap))
    probe("quiet", [])
    probe("read", ["read"])
    probe("read, read", ["read", "read"])
    probe("read, 01 10, read", ["read", "01 10", "read"])
    probe("read, 01 11, 01 10, read", ["read", "01 11", "01 10", "read"])
    probe("read, 08 80, read", ["read", "08 80", "read"])
    probe("read, wait 350, read", ["read", "wait 350", "read"])
    probe("read, read (repeat)", ["read", "read"])

    print("\n=== C. stock's condition: level +100 (above the signal), AUTO autowait 25, 0x08 ===")
    sc.cmd("fpga scope trigmode auto"); set_tb(sc, 0x08); sc.cmd("fpga postedge 1"); sc.cmd("fpga autowait 25")
    sc.cmd("fpga scope level 100"); time.sleep(1.0)
    e0, o0 = counters(sc); t0 = time.time(); fr = [np.asarray(grab_frame(sc)["ch1"]) for _ in range(4)]; e1, o1 = counters(sc); dt = time.time() - t0
    d = [int(np.sum(a != b)) for a, b in zip(fr, fr[1:])]
    print("  level +100: edges +%d pairs +%d in %.1f s (%.1f pairs/s)  consecutive-frame differing samples %s" % (e1 - e0, o1 - o0, dt, (o1 - o0) / dt, d))
    sc.cmd("fpga scope level 0"); time.sleep(1.0)
    e0, o0 = counters(sc); t0 = time.time(); time.sleep(6.0); e1, o1 = counters(sc); dt = time.time() - t0
    print("  level 0, autowait 25, postedge 1: edges +%d pairs +%d  edges/pair %.2f  pairs/s %.1f" % (e1 - e0, o1 - o0, (e1 - e0) / max(o1 - o0, 1), (o1 - o0) / dt))
    sc.cmd("fpga postedge 0"); sc.cmd("fpga autowait 0"); set_tb(sc, 0x10); sc.cmd("fpga scope trigmode auto")
    sc.close()

if __name__ == "__main__":
    main()
