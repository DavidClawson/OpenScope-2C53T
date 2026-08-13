#!/usr/bin/env python3
"""Compile the firmware. This is the suite that makes the gate mean something.

WHY THIS EXISTS
---------------
On 2026-08-13 `python3 scripts/run_tests.py --strict` reported "10/10 suites,
no skips" against a tree whose firmware did not compile. Every other suite in
SUITES inspects the IAP/bootloader/stock-image tooling -- reading source text,
built *bootloader* binaries, or the stock vendor image. **None of them ran the
compiler over `firmware/src`.** Two holes were demonstrated, not theorised:

  1. A malformed comment in `flash_fs.h` broke every translation unit that
     included it. The gate stayed green.
  2. `test_flash_switcher` asserted a fixed flash-budget verdict against
     whatever image happened to be sitting in `firmware/build/firmware.bin`.
     After `make emu` (~227 KB) it FAILED; after `make` (~538 KB) it PASSED,
     with no source change. Fixed in 156957b; this suite deliberately does not
     reintroduce the same hazard -- see ISOLATED BUILD DIRECTORIES below.

So: this suite compiles every target a source change could break, from clean.

WHAT IT CHECKS
--------------
  * every target links, and produces a non-empty artifact
  * the artifact's vector table is plausible (SP in SRAM, odd reset vector in
    flash) -- a cheap guard against a mislinked image
  * no NEW compiler warnings, against a recorded baseline (see the ratchet)

ISOLATED BUILD DIRECTORIES
--------------------------
Each target builds into its own `build-gate-*` directory, never `build/`.
Two reasons, both learned the hard way:

  * the gate must not clobber an image a developer just built to flash; and
  * more importantly, the gate must not become the thing that *populates* the
    ambient artifact other suites read. That coupling is precisely hole (2).

Each gate directory is removed before use, so every run is a full compile of
every translation unit. An incremental build emits no diagnostics for
unchanged files, which would quietly hollow out the warning ratchet.

THE WARNING RATCHET, AND WHY IT IS NOT "ZERO WARNINGS"
-----------------------------------------------------
`docs/dev_plan_2026-08-13.md` ground rule 5 claimed `guest` "must stay
warning-clean". It never was, and `emu` is further from it -- see
WARNING_BASELINE below for the measured truth. A hard zero-warning gate
adopted today would have exactly two outcomes, both bad: unrelated files get
edited by whoever trips it next, or somebody silences the category with
`-Wno-`. Either way the gate stops reporting on the thing it names.

So the contract is a ratchet: the baseline is frozen, and any warning NOT in
it fails. New code cannot add warnings; old ones are visible, counted, and can
be retired one at a time (pruning the baseline is a passing change, not a
failing one). Warnings from vendored third-party code and from the toolchain's
own libc are excluded -- they are toolchain-version noise, not ours.

Usage:
  python3 scripts/test_firmware_build.py          # via run_tests.py normally
  KEEP_GATE_BUILDS=1 python3 scripts/test_firmware_build.py -v
"""

from __future__ import annotations

import os
import re
import shutil
import struct
import subprocess
import unittest
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FIRMWARE = REPO / "firmware"

# The cross compiler every target needs.
TOOLCHAIN = "arm-none-eabi-gcc"

# Vendor trees that are gitignored and cloned by hand (see CLAUDE.md "First-time
# setup"). A git worktree does NOT inherit them from the main checkout, which is
# the common way to hit this.
AT32_LIB = FIRMWARE / "at32f403a_lib/libraries/drivers/inc/at32f403a_407_gpio.h"
FREERTOS = FIRMWARE / "FreeRTOS/tasks.c"


@dataclass
class BuildSpec:
    """One `make` invocation the gate is responsible for."""

    name: str
    cwd: Path
    args: list[str]
    build_dir: str
    artifact: str
    needs: list[Path]
    # A firmware image starts with (initial SP, reset vector). The bootloader
    # and the app both do; hwtest does too. Nothing here is a raw payload.
    check_vector_table: bool = True


@dataclass
class BuildResult:
    spec: BuildSpec
    returncode: int = -1
    output: str = ""
    warnings: list[str] = field(default_factory=list)


BUILDS = (
    # The default target: application image at 0x08004000, linked with
    # ld/at32f403a_app.ld. What `make flash` ships to a unit running our own
    # HID bootloader.
    BuildSpec("app", FIRMWARE, ["make"], "build-gate-app", "firmware.bin",
              [AT32_LIB, FREERTOS]),
    # The bench build: image at 0x08007000 under the FNIRSI factory IAP
    # bootloader, -DGUEST_BUILD. Every bench session runs this one.
    BuildSpec("guest", FIRMWARE, ["make", "guest"], "build-gate-guest", "firmware.bin",
              [AT32_LIB, FREERTOS]),
    # -DEMULATOR_BUILD. Compiles a materially different set of code paths
    # (stubbed clock init, tasks compiled out), so it catches breakage the two
    # hardware builds cannot.
    BuildSpec("emu", FIRMWARE, ["make", "emu"], "build-gate-emu", "firmware.bin",
              [AT32_LIB, FREERTOS]),
    # Minimal bring-up image. Only build that compiles src/attic/hwtest.c, and
    # the only one that does not need FreeRTOS.
    BuildSpec("hwtest", FIRMWARE, ["make", "-f", "Makefile.hwtest"],
              "build-gate-hwtest", "hwtest.bin", [AT32_LIB]),
    # The permanent 16 KB USB HID IAP bootloader. Several other suites assert
    # against its *source text*; this is the only check that it still builds.
    BuildSpec("bootloader", FIRMWARE / "bootloader", ["make"], "build-gate",
              "bootloader.bin", [AT32_LIB]),
)

# ---------------------------------------------------------------------------
# Warning ratchet
# ---------------------------------------------------------------------------

# Measured on 2026-08-13, arm-none-eabi-gcc 13.2.1, at commit 9388e71 (the PR
# #13 merge). This is the REAL baseline that ground rule 5 misdescribed.
#
# `app`, `guest` and `bootloader` are genuinely warning-clean. `emu` and
# `hwtest` are not, and never have been:
#
#   * emu -- -DEMULATOR_BUILD compiles out the callers of several tasks,
#     leaving the definitions unreferenced. Real diagnostics about a real
#     (harmless) situation, not compiler noise.
#   * hwtest -- `power_hold_init()` is dead code in an archived bring-up file
#     (src/attic/). Left alone deliberately: attic files are kept as a record
#     of what was run on hardware, and editing one to quiet a warning would
#     falsify that record for no benefit.
#
# Entries are normalised: repo-relative path, no line/column numbers. Removing
# an entry once its warning is fixed is a PASSING change -- prune freely.
WARNING_BASELINE = {
    "app": set(),
    "guest": set(),
    "bootloader": set(),
    "hwtest": {
        "firmware/src/attic/hwtest.c: warning: 'power_hold_init' defined but not used [-Wunused-function]",
    },
    "emu": {
        "firmware/src/main.c: warning: variable 'scope_entered_frame' set but not used [-Wunused-but-set-variable]",
        "firmware/src/main.c: warning: 'startup_settings_checksum' defined but not used [-Wunused-function]",
        "firmware/src/drivers/continuity_buzzer.c: warning: 'buzzer_task' defined but not used [-Wunused-function]",
        "firmware/src/drivers/continuity_buzzer.c: warning: 'buzzer_task_handle' defined but not used [-Wunused-variable]",
        "firmware/src/drivers/meter_autoselect.c: warning: 'vMeterAutoselectTask' defined but not used [-Wunused-function]",
        "firmware/src/drivers/usb_debug.c: warning: 'vUsbDebugTask' defined but not used [-Wunused-function]",
    },
}

# Vendored third-party code and the toolchain's own libc. Diagnostics from
# these are a function of which compiler and which vendor drop is installed,
# not of anything in this repo, so ratcheting them would make the gate fail on
# a toolchain upgrade for no useful reason.
NOT_OURS = (
    "at32f403a_lib/",
    "at32f403a_gcc/",
    "FreeRTOS/",
    "CMSIS-DSP/",
    "gd32f30x_lib/",
    "newlib",
    "/usr/lib/",
    "/usr/arm-none-eabi/",
)

WARNING_RE = re.compile(r"^(?P<file>[^\s:]+):(?:\d+:)?(?:\d+:)?\s*(?P<rest>warning:.*)$")


def normalise_warning(line: str) -> str | None:
    """Turn one compiler diagnostic into a stable, location-insensitive key.

    Line numbers are dropped on purpose: an unrelated edit above a warning
    would otherwise "introduce" a new one and fail the gate.
    """
    match = WARNING_RE.match(line.strip())
    if not match:
        return None
    raw = match.group("file")
    if any(marker in raw for marker in NOT_OURS):
        return None
    path = Path(raw)
    if not path.is_absolute():
        # Compiler messages are relative to the make cwd; both are under
        # firmware/, so resolve against each candidate root.
        for root in (FIRMWARE, FIRMWARE / "bootloader"):
            if (root / path).exists():
                path = root / path
                break
        else:
            return None
    try:
        rel = path.resolve().relative_to(REPO)
    except ValueError:
        return None
    return f"{rel}: {match.group('rest')}"


# ---------------------------------------------------------------------------


def missing_prerequisites(spec: BuildSpec) -> list[str]:
    problems = []
    if shutil.which(TOOLCHAIN) is None:
        problems.append(f"{TOOLCHAIN} is not on PATH")
    for need in spec.needs:
        if not need.exists():
            # Name the tree, not the probe file -- the tree is what you clone.
            tree = need.relative_to(REPO).parts[:2]
            problems.append(f"{'/'.join(tree)}/ is not present")
    return sorted(set(problems))


def run_build(spec: BuildSpec) -> BuildResult:
    result = BuildResult(spec)
    gate_dir = spec.cwd / spec.build_dir
    shutil.rmtree(gate_dir, ignore_errors=True)
    proc = subprocess.run(
        [*spec.args, f"BUILD_DIR={spec.build_dir}"],
        cwd=spec.cwd,
        capture_output=True,
        text=True,
    )
    result.returncode = proc.returncode
    result.output = (proc.stdout or "") + (proc.stderr or "")
    seen = []
    for line in result.output.splitlines():
        key = normalise_warning(line)
        if key and key not in seen:
            seen.append(key)
    result.warnings = seen
    return result


def tail(text: str, lines: int = 40) -> str:
    return "\n".join(text.splitlines()[-lines:])


class FirmwareBuildTests(unittest.TestCase):
    results: dict[str, BuildResult] = {}
    blocked: dict[str, str] = {}

    @classmethod
    def setUpClass(cls) -> None:
        runnable = []
        for spec in BUILDS:
            problems = missing_prerequisites(spec)
            if problems:
                cls.blocked[spec.name] = "; ".join(problems)
            else:
                runnable.append(spec)
        if not runnable:
            return
        # Independent BUILD_DIRs, so the targets cannot collide. `make -j` is
        # NOT usable here: `guest`/`emu` are `clean all`, and under -j the
        # clean races the build.
        with ThreadPoolExecutor(max_workers=min(len(runnable), os.cpu_count() or 1)) as pool:
            for result in pool.map(run_build, runnable):
                cls.results[result.spec.name] = result

    @classmethod
    def tearDownClass(cls) -> None:
        if os.environ.get("KEEP_GATE_BUILDS"):
            return
        for spec in BUILDS:
            shutil.rmtree(spec.cwd / spec.build_dir, ignore_errors=True)

    def _result(self, name: str) -> BuildResult:
        if name in self.blocked:
            # Deliberately a skip, not a silent pass: run_tests.py gives skips
            # a headline and --strict exits non-zero on them, so a missing
            # toolchain or vendor tree cannot be mistaken for coverage.
            self.skipTest(
                f"cannot build '{name}': {self.blocked[name]} -- this is MISSING COVERAGE, "
                "not a pass. Install arm-none-eabi-gcc and clone the vendor trees "
                "(see CLAUDE.md 'First-time setup'); in a git worktree, symlink them "
                "from the main checkout."
            )
        return self.results[name]

    def _assert_builds(self, name: str) -> BuildResult:
        result = self._result(name)
        self.assertEqual(
            result.returncode, 0,
            f"\n'{' '.join(result.spec.args)}' FAILED (exit {result.returncode}) "
            f"in {result.spec.cwd.relative_to(REPO)}:\n\n{tail(result.output)}\n",
        )
        artifact = result.spec.cwd / result.spec.build_dir / result.spec.artifact
        self.assertTrue(artifact.exists(), f"{name}: {artifact} was not produced")
        self.assertGreater(artifact.stat().st_size, 1024, f"{name}: artifact is implausibly small")

        if result.spec.check_vector_table:
            sp, reset = struct.unpack("<II", artifact.read_bytes()[:8])
            self.assertTrue(
                0x20000000 <= sp <= 0x20040000,
                f"{name}: initial SP {sp:#010x} is not in AT32 SRAM -- image mislinked",
            )
            self.assertTrue(
                reset & 1 and 0x08000000 <= reset <= 0x08100000,
                f"{name}: reset vector {reset:#010x} is not an odd flash address -- image mislinked",
            )
        return result

    def test_app_builds(self) -> None:
        """`make` -- the application image at 0x08004000."""
        self._assert_builds("app")

    def test_guest_builds(self) -> None:
        """`make guest` -- the bench image at 0x08007000. Ground rule 5's headline target."""
        self._assert_builds("guest")

    def test_emu_builds(self) -> None:
        """`make emu` -- compiles code paths the hardware builds do not."""
        self._assert_builds("emu")

    def test_hwtest_builds(self) -> None:
        """`make -f Makefile.hwtest` -- the minimal bring-up image."""
        self._assert_builds("hwtest")

    def test_bootloader_builds(self) -> None:
        """`make -C firmware/bootloader` -- the permanent 16 KB HID IAP bootloader."""
        self._assert_builds("bootloader")

    def test_no_new_compiler_warnings(self) -> None:
        """Ratchet: every warning must already be in WARNING_BASELINE.

        Not a zero-warning rule -- see the module docstring for why.
        """
        blocked = sorted(self.blocked)
        if blocked:
            self.skipTest(
                f"warning ratchet cannot run: {', '.join(blocked)} did not build "
                "(missing toolchain or vendor trees) -- MISSING COVERAGE, not a pass"
            )
        new: list[str] = []
        for name, result in sorted(self.results.items()):
            for warning in result.warnings:
                if warning not in WARNING_BASELINE.get(name, set()):
                    new.append(f"[{name}] {warning}")
        self.assertEqual(
            new, [],
            "\nNEW COMPILER WARNING(S) -- fix them, or if the diagnostic is genuinely\n"
            "expected, add it to WARNING_BASELINE in this file WITH a reason:\n\n  "
            + "\n  ".join(new) + "\n",
        )

    def test_warning_baseline_is_not_stale(self) -> None:
        """The other half of the ratchet: a fixed warning must be pruned.

        Without this the baseline only ever grows, and it stops describing the
        tree -- which is exactly how ground rule 5 came to claim `guest` was
        warning-clean when it had never been checked.
        """
        blocked = sorted(self.blocked)
        if blocked:
            self.skipTest(
                f"warning ratchet cannot run: {', '.join(blocked)} did not build "
                "(missing toolchain or vendor trees) -- MISSING COVERAGE, not a pass"
            )
        stale: list[str] = []
        for name, result in sorted(self.results.items()):
            for warning in sorted(WARNING_BASELINE.get(name, set())):
                if warning not in result.warnings:
                    stale.append(f"[{name}] {warning}")
        self.assertEqual(
            stale, [],
            "\nWARNING_BASELINE lists warning(s) the compiler no longer emits.\n"
            "Good news -- delete these entries so the baseline keeps describing reality:\n\n  "
            + "\n  ".join(stale) + "\n",
        )


if __name__ == "__main__":
    unittest.main()
