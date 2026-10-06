#!/usr/bin/env python3
"""EXP-72 -- what sets the FPGA's ~189 ms post-handover hold?

Our display refreshes at ~3-4 Hz; stock's looks like 30-40 fps. EXP-53 found
the FPGA holds a handed-over record ~189 ms before rolling again, so the
fastest possible handover cycle is hold + one fill. Stock's June boot capture
shows a fresh data-ready per 04/05 pair at a ~29 ms cadence with the SAME five
post-config register writes we send -- so either stock's hold is shorter for a
reason not yet in our model, or something differs that we have not varied.

Method: a fast, always-triggering input (JDS CH1 1 kHz sine, 2 Vpp, +0.5 V,
range 5, level code 128 -- the EXP-55 posture at a higher frequency), NORMAL
mode, the acq loop told to poll immediately and often (`fpga postedge 1`,
`fpga pollgap 5`), so the handover interval measures the FPGA, not our loop.
Per condition: the acq task logs every handover (`fpga holdlog`, added for
this experiment): ms since the previous one, roll reads polled in between,
strobes on the pair. Median interval reported. Then FPGA status (opcode 03) for the stock comparison
(stock: 00 01 42 2E 2E; ours at boot: 00 00 54 85 85).

PREDICTIONS (written before the run)
  P1 baseline, timebases 0x0E..0x12: interval ~= 189 + fill + <=5 ms poll
     (0x0E ~215, 0x0F ~235, 0x10 ~276, 0x11 ~400, 0x12 ~600) -- hold constant.
  P2 polling cadence (5 vs 29 ms) and the read clock (/2 vs /256): no change
     to the hold beyond the poll granularity (EXP-53: reads inside the cycle
     are harmless).
  P3 register writes at 0x0E (reg 0x02 00/01/02; reg 0x06 and 0x07 walking
     bits + FF): H0 = no value moves the interval. Any condition with an
     interval < ~60 ms reproduces stock's cadence and names the control.
  Falsifiers: baseline intervals not fitting hold + fill (the model is
     wrong); a value that kills handovers (engine broken or trigger disabled
     -- the restore control after each register separates those).
  Blind spots: register contents cannot be read back, so an ignored write
     and an irrelevant one look the same; a control that only latches with a
     reg 0x01 re-arm is caught only because every block ends by re-writing
     the timebase; registers outside 02/06/07 are not swept (--wild adds
     0x09..0x0F, which have no known function).
"""
import argparse, json, os, re, sys, time
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600

OUT = "/home/david/osc/reverse_engineering/captures/exp72/"
FS = {0x0E: 49930.1, 0x0F: 24979.1, 0x10: 12490.0, 0x11: 4990.8, 0x12: 2494.9}


def st(sc):
    t = sc.cmd("status", timeout=8.0)
    return (int(re.search(r"PC0 edges: (\d+)", t).group(1)),
            int(re.search(r"SPI3 OK: (\d+)", t).group(1)))


def status03(sc):
    t = sc.cmd("spi3 seq 03 ff ff ff ff", timeout=6.0)
    m = re.search(r"MISO: ([0-9A-Fa-f ]+)", t)
    return m.group(1).strip() if m else "?"


def measure(sc, tag, code, window, rows, log):
    """Handover log recorded by the acq task; no shell traffic in the window."""
    time.sleep(1.5)                               # settle after the change
    sc.cmd("fpga holdlog on")
    time.sleep(window)
    t = sc.cmd("fpga holdlog off", timeout=8.0)
    ent = [tuple(map(int, m)) for m in re.findall(r"^hl (\d+) (\d+) (\d+)", t, re.M)]
    dts = sorted(e[0] for e in ent)
    med = dts[len(dts) // 2] if dts else float("inf")
    pol = sorted(e[1] for e in ent); edg = [e[2] for e in ent]
    fill = 1024.0 / FS[code] * 1000.0 if code in FS else float("nan")
    s03 = status03(sc)
    row = dict(tag=tag, code=code, n=len(ent), dt_med=med, dt_min=dts[0] if dts else None,
               dt_max=dts[-1] if dts else None, polls_med=pol[len(pol) // 2] if pol else None,
               edges_per_ho=round(sum(edg) / len(edg), 2) if edg else None, fill_ms=round(fill, 1),
               dt_minus_fill=round(med - fill, 1), status03=s03, raw=ent)
    rows.append(row); log.write(json.dumps(row) + "\n"); log.flush()
    print("%-22s tb 0x%02X  n %3d  dt med %6s min %5s max %5s  -fill %7.1f  polls %4s  edges/ho %4s  st03 %s" % (
        tag, code, len(ent), med, row["dt_min"], row["dt_max"], med - fill, row["polls_med"],
        row["edges_per_ho"], s03), flush=True)
    return row


def seq(sc, reg, val):
    t = sc.cmd("spi3 seq %02x %02x" % (reg, val), timeout=6.0)
    if "error" in t.lower() or "usage" in t.lower():
        print("  write failed:", t.strip()[:120])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--window", type=float, default=6.0)
    ap.add_argument("--wild", action="store_true", help="also sweep regs 0x09..0x0F (no known function)")
    ap.add_argument("--wild-only", action="store_true", help="0x0E baseline control, then regs 0x09..0x0F only")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    stamp = time.strftime("%Y-%m-%d_%H%M%S")
    log = open(OUT + "exp72_%s.jsonl" % stamp, "w")
    sc = Scope("/dev/ttyACM0")
    build = next((l for l in sc.version().splitlines() if l.startswith("Build:")), "?")
    print("EXP-72 device:", build); log.write(json.dumps({"build": build}) + "\n")

    jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(1000.0, 1); jds.amp(2.0, 1)
    jds.offset(0.5, 1); jds.output(True, False); jds.close()
    sc.scope_range(5, 1)
    print(sc.cmd("fpga scope level 0").strip().splitlines()[-1])
    print("boot status03:", status03(sc))
    sc.cmd("fpga scope trigmode normal")
    sc.cmd("fpga postedge 1"); sc.cmd("fpga pollgap 5")
    rows = []

    print("-- P1 baseline: hold + fill across timebases")
    for code in ((0x0E,) if a.wild_only else (0x0E, 0x0F, 0x10, 0x11, 0x12, 0x0E)):
        sc.cmd("fpga scope timebase %02x" % code, timeout=6.0)
        measure(sc, "base", code, a.window if code < 0x11 else a.window + 4, rows, log)
    if not any(r["n"] for r in rows):
        print("NO HANDOVERS in the baseline -- input not triggering; stopping."); return 2

    if not a.wild_only:
        print("-- P2 polling cadence and read clock (0x0E)")
        sc.cmd("fpga pollgap 29"); measure(sc, "pollgap29 (stock)", 0x0E, a.window, rows, log)
        sc.cmd("fpga pollgap 5")
        sc.cmd("fpga acqbr 0"); measure(sc, "acqbr /2 (stock)", 0x0E, a.window, rows, log)
        sc.cmd("fpga acqbr 7"); measure(sc, "acqbr /256", 0x0E, a.window, rows, log)
        sc.cmd("fpga acqbr 0"); sc.cmd("fpga acqbr off")   # run 1: 'off' alone LEFT /256 in force
        sc.cmd("fpga postedge 0"); measure(sc, "postedge derived", 0x0E, a.window, rows, log)
        sc.cmd("fpga postedge 1")

    print("-- P3 register writes (0x0E); each block ends with the stock value + a timebase re-write")
    plan = [] if a.wild_only else [
        (0x02, 0x03, (0x00, 0x01, 0x02)),
        (0x06, 0x00, (0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0xFF)),
        (0x07, 0x00, (0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0xFF))]
    if a.wild or a.wild_only:
        plan += [(r, 0x00, (0x01, 0xFF)) for r in range(0x09, 0x10)]
    for reg, stock, vals in plan:
        for v in vals:
            seq(sc, reg, v)
            measure(sc, "reg%02X=%02X" % (reg, v), 0x0E, a.window, rows, log)
        seq(sc, reg, stock)
        sc.cmd("fpga scope timebase 0e", timeout=6.0)
        measure(sc, "reg%02X=%02X restore" % (reg, stock), 0x0E, a.window, rows, log)

    # restore
    sc.cmd("fpga postedge 0"); sc.cmd("fpga pollgap 30"); sc.cmd("fpga acqbr 0"); sc.cmd("fpga acqbr off")
    sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0)
    sc.close(); log.close()
    base = [r for r in rows if r["tag"] == "base"]
    print("baseline dt - fill: %s" % ", ".join("0x%02X %.0f" % (r["code"], r["dt_minus_fill"]) for r in base))
    fast = [r for r in rows if r["dt_med"] < 60]
    print("conditions under 60 ms: %s" % (", ".join(r["tag"] for r in fast) or "none"))
    print("log:", log.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
