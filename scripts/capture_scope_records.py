#!/usr/bin/env python3
"""Capture CH1 records from the device into the test-fixture format.

Reads through `spi3 read`, the SAME path the Freq badge reads
(fpga_get_ch1_buf), NOT `spi3 opread` -- that distinction has bitten this
project twice (EXP-13 s3, EXP-16).

Usage:
  capture_scope_records.py OUT.txt --codes 14,15,16 --freqs 150,330,700
                           [--ranges 5,8] [--reps 1] [--note "..."]
"""
import argparse, os, re, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
from bench import Scope, Siggen, parse_dump


def load_rates():
    """Sample rates straight out of scope_timebase.c -- one source of truth."""
    src = os.path.join(os.path.dirname(__file__), "..", "firmware",
                       "src", "ui", "scope_timebase.c")
    rates = {}
    for line in open(src):
        m = re.match(r"\s*/\*\s*(0x[0-9A-Fa-f]{2})\s*\*/\s*([0-9.]+)f", line)
        if m:
            rates[int(m.group(1), 16)] = float(m.group(2))
    return rates


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--codes", default="14,15,16")
    ap.add_argument("--freqs", default="150,330,700,1500,3000")
    ap.add_argument("--ranges", default="5,8")
    ap.add_argument("--reps", type=int, default=1)
    ap.add_argument("--note", default="")
    a = ap.parse_args()

    rates = load_rates()
    codes = [int(x, 0) for x in a.codes.split(",")]
    freqs = [float(x) for x in a.freqs.split(",")]
    ranges = [int(x) for x in a.ranges.split(",")]

    sc, sg = Scope(), Siggen()
    build = sc.version().strip().splitlines()
    build = next((l for l in build if l.startswith("Build:")), "?")

    # Pin the source configuration BEFORE measuring its loop rate: the rate
    # depends on it (EXP-14 -- one sine + one DC is ~300 Hz off two sines).
    sg.dc(0, ch=2)
    sg.sine(freqs[0], ch=1)
    hz, ratio = sg.fs(window=6.0)
    sg.use_measured_fs(True, window=0.0)
    print(f"source loop rate {hz:.1f} S/s (ratio {ratio:.4f}), usefs on")

    rows = []
    for rng in ranges:
        sc.scope_range(rng, 1)
        for code in codes:
            sc.timebase(code)
            for f in freqs:
                sg.sine(f, ch=1)
                time.sleep(0.4)
                for _ in range(a.reps):
                    v = parse_dump(sc.cmd("spi3 read 1024", timeout=8.0))
                    if len(v) != 1024:
                        print(f"  short read {len(v)} at r{rng} c{code:#04x} {f}Hz")
                        continue
                    span = int(v.max() - v.min())
                    rows.append((rng, code, f, span, v))
                    print(f"  r{rng} c{code:#04x} {f:7.1f}Hz span {span:3d}")

    # Closing control: did the source drift across the run?
    hz2, _ = sg.fs(window=6.0)
    drift = abs(hz2 - hz) / hz * 100.0
    print(f"source drift over run: {drift:.3f}%  ({hz:.1f} -> {hz2:.1f})")

    with open(a.out, "w") as fh:
        fh.write(f"# OpenScope 2C53T capture records, bench unit #1, CH1.\n")
        if a.note:
            for line in a.note.split("\n"):
                fh.write(f"# {line}\n")
        fh.write(f"# {build}\n")
        fh.write(f"# Source: ESP32 siggen usefs=1, loop {hz:.1f} S/s, "
                 f"drift {drift:.3f}% over the run.\n")
        fh.write(f"# Read path: `spi3 read` (acq buffer) -- the badge's path.\n")
        fh.write(f"# Format: range code fs drive_hz span then 1024 ADC counts.\n")
        for rng, code, f, span, v in rows:
            fh.write(f"{rng} {code} {rates[code]:g} {f:g} {span}\n")
            fh.write(" ".join(str(int(x)) for x in v) + "\n")
    print(f"wrote {len(rows)} records to {a.out}")
    if drift > 0.2:
        print("WARNING: source drifted >0.2% -- treat drive_hz as approximate")


if __name__ == "__main__":
    main()
