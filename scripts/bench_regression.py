#!/usr/bin/env python3
"""The bench regression: every hardware acceptance that guards a shipped
feature, run as one command against one unit, with one log.

Until 2026-10-03 "the regression" meant `exp22_stability.py --trigger-only`
by convention -- the v0.4.0 release cited it, and nothing else on the bench
was re-run before a release. Features accepted since (mask pass/fail,
EXP-69; coupling / probe / SAVE, EXP-70) had acceptance scripts but nothing
that made a release re-run them. This is that thing.

Suites, in run order (each sets up its own scope state and generator):

  trigger    exp22_stability.py --trigger-only   trigger modes / edge / position /
                                                  record integrity (EXP-54..58)
  mask       mask_bench.py                       mask pass/fail A-H (EXP-69)
  coupling   coupling_bench.py                   AC/DC relays, probe 1X/10X (EXP-70)

The verdict of each suite is ITS OWN exit code -- this runner does not
re-judge anything, it only refuses to call a run green when a suite failed,
crashed, timed out or never started.

Bench wiring assumed by every suite: JDS6600 CH1 -> scope CH1, JDS CH2 ->
scope CH2. (The JDS CH2 DC offset is unreliable -- see bench.py -- and the
suites only use it where a change of either sign suffices.)

Usage:
  python3 scripts/bench_regression.py                 # all suites
  python3 scripts/bench_regression.py --only mask     # one suite
  python3 scripts/bench_regression.py --list          # show, run nothing
Log: reverse_engineering/captures/regression/<date>_<build>/ (one .log per
suite + SUMMARY.txt; exp22's frames saved as .npz like the v0.4.0 run).
"""
import argparse
import datetime
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from bench import Scope  # noqa: E402

SUITES = [
    ("trigger", "exp22_stability.py", ["--trigger-only"], "--scope-port", "--jds-port", 1800),
    ("mask", "mask_bench.py", [], "--scope", "--jds", 900),
    ("coupling", "coupling_bench.py", [], "--scope", "--jds", 900),
]

# The one line in each suite's output that summarises it, for SUMMARY.txt.
SUMMARY_RE = re.compile(r"(\d+/\d+ criteria met|OVERALL: (?:PASS|FAIL))")


def build_line(port):
    try:
        sc = Scope(port)
    except Exception as exc:              # no device / wrong port: a refusal, not a crash
        print("cannot open %s: %s" % (port, exc))
        return None
    try:
        return next((l.strip() for l in sc.version().splitlines() if l.startswith("Build:")), None)
    finally:
        sc.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scope", default="/dev/ttyACM0")
    ap.add_argument("--jds", default="/dev/ttyUSB0")
    ap.add_argument("--only", choices=[s[0] for s in SUITES])
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--out", default=None, help="log directory (default: dated, under captures/regression)")
    a = ap.parse_args()

    suites = [s for s in SUITES if a.only in (None, s[0])]
    if a.list:
        for name, script, extra, *_rest in suites:
            print("%-9s %s %s" % (name, script, " ".join(extra)))
        return 0

    build = build_line(a.scope)
    if not build:
        print("device on %s did not answer `version` -- refusing to start" % a.scope)
        return 2
    stamp = re.sub(r"[^0-9A-Za-z]+", "_", build.replace("Build:", "")).strip("_")
    out = Path(a.out) if a.out else ROOT / "reverse_engineering/captures/regression" / (
        datetime.date.today().isoformat() + "_" + stamp)
    out.mkdir(parents=True, exist_ok=True)
    print("device %s\nlogs   %s" % (build, out.relative_to(ROOT) if out.is_relative_to(ROOT) else out))

    results = []
    for name, script, extra, sflag, jflag, timeout in suites:
        cmd = [sys.executable, str(HERE / script)] + extra + [sflag, a.scope, jflag, a.jds]
        if name == "trigger":
            cmd += ["--save", str(out / "trigger_frames.npz")]
        print("\n== %s: %s" % (name, " ".join(Path(c).name if c.endswith(".py") else c for c in cmd[1:])),
              flush=True)
        t0 = time.time()
        log = out / ("%s.log" % name)
        try:
            p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, timeout=timeout)
            text, code = p.stdout, p.returncode
        except subprocess.TimeoutExpired as e:
            text = (e.stdout or "") if isinstance(e.stdout, str) else ""
            text += "\n*** TIMED OUT after %d s ***\n" % timeout
            code = None
        log.write_text(text)
        summ = [m.group(0) for m in SUMMARY_RE.finditer(text)]
        fails = [l.strip() for l in text.splitlines() if re.match(r"\s+FAIL\b", l)]
        ok = code == 0
        results.append((name, ok, code, time.time() - t0, summ[-1] if summ else "-", fails))
        print("   %s  exit %s  %.0f s  %s" % ("PASS" if ok else "FAIL", code, time.time() - t0,
                                             summ[-1] if summ else "(no summary line)"))
        for f in fails[:8]:
            print("     " + f)

    # The device must still be the same image at the end: a reset mid-run
    # (brownout, hard fault + boot loop) would otherwise pass unnoticed.
    end_build = build_line(a.scope)
    same = end_build == build
    lines = ["device %s" % build, "end    %s%s" % (end_build, "" if same else "   <-- CHANGED")]
    for name, ok, code, dt, summ, fails in results:
        lines.append("%-9s %s  exit %-4s %5.0f s  %s" % (name, "PASS" if ok else "FAIL", code, dt, summ))
    green = all(r[1] for r in results) and same and len(results) == len(suites)
    lines.append("REGRESSION %s (%d/%d suites)" % ("GREEN" if green else "RED",
                                                  sum(r[1] for r in results), len(suites)))
    (out / "SUMMARY.txt").write_text("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines))
    return 0 if green else 1


if __name__ == "__main__":
    sys.exit(main())
