#!/usr/bin/env python3
"""Run every flash-safety / firmware test suite and report skips loudly.

WHY THIS EXISTS
---------------
On 2026-08-11 a review reported "49 tests pass" as a clean result. Five tests
had been skipped because local artifacts were missing, and one of them was
test_updater_binaries_gap_fill_stock_settings_page -- the only test in the
whole suite that inspects a real built binary rather than the text of a source
file. The single highest-value check was silently absent from the number that
was used to approve the change.

`unittest` prints skips as a quiet "(skipped=5)" suffix that reads like a
footnote. This runner makes them impossible to miss, names each one, and can
fail outright on them, so "everything passed" cannot quietly mean "everything
that ran passed".

Usage:
  python3 scripts/run_tests.py             # run all, report skips prominently
  python3 scripts/run_tests.py --strict    # exit non-zero if anything skipped
  python3 scripts/run_tests.py -k iap      # only suites matching a substring
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent

# Suites in rough order of how much they protect: flash safety first.
#
# test_firmware_build.py is last because it is by far the slowest (~15 s; it
# compiles five targets from clean) and because a source-level failure is
# usually easier to read after the cheap text checks have had their say. It is
# also the only suite that runs a compiler over firmware/src at all -- until it
# was added on 2026-08-13, a green gate was compatible with firmware that did
# not build. See its module docstring.
SUITES = (
    "test_iap_erase_guard.py",
    "test_flash_regions.py",
    "test_cal_backup.py",
    "test_flash_switcher.py",
    "test_flash_preflight.py",
    "test_bootloader_updater.py",
    "test_bootloader_power_entry.py",
    "test_stock_dispatcher_power_handoff.py",
    "test_stock_hybrid_power_hold_patch.py",
    "test_stock_settings.py",
    "test_stock_settings_diff.py",
    "test_shell_table.py",
    "test_firmware_build.py",
)

RAN_RE = re.compile(r"^Ran (\d+) test", re.MULTILINE)
SKIP_COUNT_RE = re.compile(r"skipped=(\d+)")
# unittest -v prints:  test_name (module.Class.test_name) ... skipped 'reason'
#
# ...UNLESS the test has a docstring, in which case unittest puts its first
# line on a SECOND line and the result trails that instead:
#
#   test_guest_builds (mod.Class.test_guest_builds)
#   `make guest` -- the bench image. ... skipped 'reason'
#
# The original single-line pattern silently missed that form, and the fallback
# below then reported every such skip as "(unnamed) / reason not reported" --
# a reporter that loses the very information it exists to surface. Found
# 2026-08-13 when test_firmware_build.py (whose tests all have docstrings) had
# its skip reasons swallowed. The optional group absorbs the docstring line.
SKIP_LINE_RE = re.compile(
    r"^(?P<name>\S+) \([^)]*\)(?:\n.*?)? \.\.\. skipped ['\"](?P<reason>.*)['\"]",
    re.MULTILINE,
)
# the non-unittest suites print a bare:  skipped 'reason'
BARE_SKIP_RE = re.compile(r"^skipped ['\"](.*)['\"]\s*$", re.MULTILINE)

GREEN, RED, YELLOW, BOLD, DIM, RESET = (
    ("\033[32m", "\033[31m", "\033[33m", "\033[1m", "\033[2m", "\033[0m")
    if sys.stdout.isatty()
    else ("", "", "", "", "", "")
)


class SuiteResult:
    def __init__(self, name: str) -> None:
        self.name = name
        self.ok = False
        self.ran = 0
        self.skipped: list[tuple[str, str]] = []
        self.duration = 0.0
        self.output = ""


def run_suite(name: str) -> SuiteResult:
    result = SuiteResult(name)
    path = SCRIPTS / name
    if not path.exists():
        result.output = f"{name} does not exist"
        return result

    started = time.monotonic()
    # -v so individual skip reasons are printed and can be attributed.
    proc = subprocess.run(
        [sys.executable, str(path), "-v"],
        capture_output=True,
        text=True,
        cwd=SCRIPTS.parent,
    )
    result.duration = time.monotonic() - started
    result.output = (proc.stdout or "") + (proc.stderr or "")
    result.ok = proc.returncode == 0

    ran = RAN_RE.search(result.output)
    result.ran = int(ran.group(1)) if ran else 0
    result.skipped = [
        (m.group("name"), m.group("reason")) for m in SKIP_LINE_RE.finditer(result.output)
    ]
    result.skipped += [("(whole suite)", m.group(1)) for m in BARE_SKIP_RE.finditer(result.output)]

    # Suites that don't use unittest report their own totals; fall back to the
    # aggregate count so the summary isn't silently zero for them.
    if not result.ran:
        legacy = re.search(r"^(\d+) tests? OK", result.output, re.MULTILINE)
        result.ran = int(legacy.group(1)) if legacy else 0
    if not result.skipped:
        count = SKIP_COUNT_RE.search(result.output)
        if count and int(count.group(1)):
            result.skipped = [("(unnamed)", "reason not reported")] * int(count.group(1))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--strict", action="store_true", help="exit non-zero if any test was skipped")
    parser.add_argument("-k", metavar="SUBSTR", help="only run suites whose filename contains SUBSTR")
    parser.add_argument("-v", "--verbose", action="store_true", help="print full output of failing suites")
    args = parser.parse_args()

    suites = [s for s in SUITES if not args.k or args.k in s]
    if not suites:
        print(f"no suite matches {args.k!r}", file=sys.stderr)
        return 2

    results = [run_suite(name) for name in suites]

    print()
    for r in results:
        mark = f"{GREEN}PASS{RESET}" if r.ok else f"{RED}FAIL{RESET}"
        skip_note = f"  {YELLOW}{len(r.skipped)} skipped{RESET}" if r.skipped else ""
        print(f"  {mark}  {r.name:<44} {r.ran:>3} tests  {DIM}{r.duration:5.2f}s{RESET}{skip_note}")

    failed = [r for r in results if not r.ok]
    skipped = [(r, name, reason) for r in results for name, reason in r.skipped]
    total_ran = sum(r.ran for r in results)

    if failed and args.verbose:
        for r in failed:
            print(f"\n{RED}{'=' * 70}\n{r.name}\n{'=' * 70}{RESET}\n{r.output}")

    print(f"\n{BOLD}{len(results) - len(failed)}/{len(results)} suites, {total_ran} tests{RESET}")

    if skipped:
        # The whole point: skips get a headline, not a suffix.
        print(f"\n{YELLOW}{BOLD}⚠  {len(skipped)} TEST(S) DID NOT RUN{RESET}")
        print(f"{YELLOW}   A skipped test is not a passing test. Most skips here mean a local")
        print(f"   artifact is missing, which removes real coverage -- often the checks")
        print(f"   that inspect built binaries rather than source text.{RESET}\n")
        for r, name, reason in skipped:
            print(f"   {DIM}{r.name}{RESET}  {name}\n      {YELLOW}{reason}{RESET}")
        print(
            f"\n   {DIM}Most are fixed by building first (make -C firmware/bootloader, etc.)\n"
            f"   and providing archive/2C53T Firmware V1.2.0/.{RESET}"
        )

    if failed:
        print(f"\n{RED}{BOLD}FAILED: {', '.join(r.name for r in failed)}{RESET}")
        if not args.verbose:
            print(f"{DIM}Re-run with -v for full output.{RESET}")
        return 1
    if skipped and args.strict:
        print(f"\n{RED}{BOLD}--strict: failing because {len(skipped)} test(s) were skipped.{RESET}")
        return 1
    if not skipped:
        print(f"{GREEN}All tests ran. No skips.{RESET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
