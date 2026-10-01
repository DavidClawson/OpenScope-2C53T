#!/usr/bin/env python3
"""Build and run the settings-persistence host tests in all three builds, then
prove the new ones can fail.

WHAT IT RUNS
------------
firmware/tests/test_config_persist.c, against the real config.c,
settings_store.c and flash_regions.c, built three ways (see its header):

  main           SETTINGS_PERSIST_WRITES=1               must pass
  nowrite        SETTINGS_PERSIST_WRITES=0               must pass: the S3
                 negative control — the change -> power cycle -> verify loop
                 persists nothing, and the positive loop test goes red
  known_defects  CONFIG_PERSIST_KNOWN_DEFECTS=1          must FAIL, on exactly
                 the listed tests. When one of them starts passing, the
                 firmware was fixed: promote that test into the main build and
                 drop it from KNOWN_DEFECTS below. This suite fails until then,
                 so a fix cannot land without the promotion.

WHY THE MUTATIONS
-----------------
Same reason as scripts/test_flash_regions.py: a guard test passes whether or
not the guard does anything. Each mutation takes a copy of one source file,
deletes or inverts one guarantee, rebuilds, and REQUIRES the named C tests to
go red. Literal substitutions: if a target string is no longer found exactly
once, this suite fails loudly instead of silently skipping the check.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "firmware"
TEST_C = FW / "tests" / "test_config_persist.c"

# Must match CONFIG_PERSIST_SRCS / CONFIG_PERSIST_CC in firmware/Makefile.
SRCS = (
    "src/util/config.c",
    "src/util/settings_store.c",
    "src/drivers/flash_regions.c",
    "src/ui/theme.c",
    "src/ui/scope_state.c",
    "src/ui/scope_cal.c",
    "src/ui/scope_timebase.c",
)
INCLUDES = ("src/util", "src/drivers", "src/ui", "src/dsp")
CFLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O1"]

BUILDS = {
    "main": ["-DSETTINGS_PERSIST_WRITES=1"],
    "nowrite": ["-DSETTINGS_PERSIST_WRITES=0"],
    "known_defects": ["-DSETTINGS_PERSIST_WRITES=1", "-DCONFIG_PERSIST_KNOWN_DEFECTS=1"],
}

KNOWN_DEFECTS = (
    "a save torn inside its magic or length does not stop later saves",
    "a damaged header mid-log does not stop later saves",
    "a compaction cut mid-erase never resurrects old settings",
)


def build_and_run(build: str, workdir: Path,
                  replace: tuple[str, str] | None = None) -> subprocess.CompletedProcess:
    """Compile one build (optionally with one source file swapped for mutated
    text) and run it. Never raises on a non-zero exit: red is the expected
    result for a mutation and for the known-defects build."""
    sources = []
    for rel in SRCS:
        if replace is not None and rel == replace[0]:
            mutated = workdir / Path(rel).name
            mutated.write_text(replace[1])
            sources.append(str(mutated))
        else:
            sources.append(str(FW / rel))

    binary = workdir / f"test_config_persist_{build}"
    cmd = ["gcc", *CFLAGS, *BUILDS[build]]
    for inc in INCLUDES:
        cmd += ["-I", str(FW / inc)]
    cmd += [str(TEST_C), *sources, "-o", str(binary)]
    built = subprocess.run(cmd, capture_output=True, text=True)
    if built.returncode != 0:
        return built
    return subprocess.run([str(binary)], capture_output=True, text=True)


def failing_lines(output: str) -> list[str]:
    return [line[len("FAIL  "):].strip()
            for line in output.splitlines() if line.startswith("FAIL  ")]


def failed(label: str, output: str) -> bool:
    return any(line.startswith(label) for line in failing_lines(output))


def run_build(build: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as tmp:
        return build_and_run(build, Path(tmp))


@dataclass
class Mutation:
    """One guarantee, deleted. `expect_fail` are the C test labels that must go red."""

    name: str
    file: str
    old: str
    new: str
    build: str = "main"
    expect_fail: tuple[str, ...] = ()


MUTATIONS: tuple[Mutation, ...] = (
    Mutation(
        name="SETTINGS_PERSIST_WRITES=0 write gate in config_save()",
        file="src/util/config.c",
        old="#if !SETTINGS_PERSIST_WRITES\n",
        new="#if 0 /* mutant: write gate removed */\n",
        build="nowrite",
        expect_fail=(
            "with writes stubbed, no change survives a power cycle",
            'with writes stubbed, "changes survive power cycles across compaction" goes red',
        ),
    ),
    Mutation(
        # The torn-record design rests on this order (flash_regions.c, "Append
        # log"). Before the power-cut sweep, nothing in either suite noticed it
        # being reversed.
        name="header-before-payload program order in flash_region_append()",
        file="src/drivers/flash_regions.c",
        old="""    st = program_verified(addr, hdr, REC_HDR_SIZE);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    st = program_verified(addr + REC_HDR_SIZE, (const uint8_t *)data, len);
    if (st != FLASH_REGION_OK) {
        return st;
    }
""",
        new="""    st = program_verified(addr + REC_HDR_SIZE, (const uint8_t *)data, len);
    if (st != FLASH_REGION_OK) {
        return st;
    }
    st = program_verified(addr, hdr, REC_HDR_SIZE);
    if (st != FLASH_REGION_OK) {
        return st;
    }
""",
        expect_fail=("a save torn after its length does not stop later saves",),
    ),
    Mutation(
        name="skip-and-continue over a CRC-failed record in log_scan()",
        file="src/drivers/flash_regions.c",
        old="""            out->valid_records++;
        }
""",
        new="""            out->valid_records++;
        } else {
            break;  /* mutant */
        }
""",
        expect_fail=(
            "damage mid-log is never applied",
            "a save torn after its length does not stop later saves",
        ),
    ),
    Mutation(
        name="record CRC check in log_scan()",
        file="src/drivers/flash_regions.c",
        old="        if (actual == crc) {\n",
        new="        if (1) { (void)actual; (void)crc;  /* mutant */\n",
        expect_fail=(
            "damage at the end of the log is never applied",
            "a save torn at any byte is never applied",
        ),
    ),
    Mutation(
        name="compaction of a full log in config_save()",
        file="src/util/config.c",
        old="    if (st == FLASH_REGION_ERR_FULL) {\n",
        new="    if (0) {  /* mutant */\n",
        expect_fail=(
            "changes survive power cycles across compaction",
            "a compaction cut before its erase keeps the old log",
        ),
    ),
    Mutation(
        name="meter_layout restore in settings_store_apply()",
        file="src/util/settings_store.c",
        old="    meter_layout  = clamp_idx(cfg->meter_layout, METER_LAYOUT_COUNT, METER_LAYOUT_FULL);\n",
        new="    (void)0;  /* mutant: meter_layout not restored */\n",
        expect_fail=("changes survive power cycles across compaction",),
    ),
)


class SettingsPersistHostTests(unittest.TestCase):
    main: subprocess.CompletedProcess

    @classmethod
    def setUpClass(cls) -> None:
        cls.main = run_build("main")

    def test_sources_exist(self) -> None:
        for path in (TEST_C, *(FW / rel for rel in SRCS)):
            self.assertTrue(path.exists(), f"{path} is missing")

    def test_main_build_passes(self) -> None:
        self.assertEqual(self.main.returncode, 0,
                         f"settings persistence host tests failed:\n"
                         f"{self.main.stdout}\n{self.main.stderr}")
        self.assertIn("tests OK", self.main.stdout)
        self.assertEqual(failing_lines(self.main.stdout), [])

    def test_main_build_is_not_trivially_small(self) -> None:
        """Guard against the suite quietly shrinking."""
        count = int(self.main.stdout.rsplit("\n", 2)[-2].split()[0])
        self.assertGreaterEqual(count, 30, f"only {count} settings persistence tests ran")

    def test_negative_control_passes(self) -> None:
        """Writes compiled out: nothing persists, and the positive loop goes red."""
        proc = run_build("nowrite")
        self.assertEqual(proc.returncode, 0,
                         f"negative control failed:\n{proc.stdout}\n{proc.stderr}")
        self.assertIn("2 tests OK", proc.stdout)

    def test_known_defects_are_still_red(self) -> None:
        """Expected to fail until the firmware is fixed; promote on green."""
        proc = run_build("known_defects")
        self.assertIn("tests,", proc.stdout,
                      f"known-defects build did not run:\n{proc.stdout}\n{proc.stderr}")
        for label in KNOWN_DEFECTS:
            self.assertTrue(
                failed(label, proc.stdout),
                f"known defect {label!r} now PASSES: the firmware was fixed. Move that "
                f"test into the main build of test_config_persist.c and drop it from "
                f"KNOWN_DEFECTS in this file.\n{proc.stdout}",
            )
        unexpected = [line for line in failing_lines(proc.stdout)
                      if not any(line.startswith(label) for label in KNOWN_DEFECTS)]
        self.assertEqual(unexpected, [], "unlisted failures in the known-defects build")


class GuaranteeMutationTests(unittest.TestCase):
    """Each guarantee, deleted in turn. The C suite must notice."""

    def _mutate(self, m: Mutation) -> None:
        original = (FW / m.file).read_text()
        self.assertEqual(
            original.count(m.old), 1,
            f"mutation target for {m.name!r} appears {original.count(m.old)} times in "
            f"{m.file} (expected exactly 1). The code moved; update the mutation "
            f"rather than dropping the check.",
        )
        with tempfile.TemporaryDirectory() as tmp:
            proc = build_and_run(m.build, Path(tmp), (m.file, original.replace(m.old, m.new)))

        self.assertIn("tests,", proc.stdout,
                      f"the {m.name} mutant did not build or run:\n{proc.stdout}\n{proc.stderr}")
        self.assertNotEqual(proc.returncode, 0,
                            f"removing the {m.name} did NOT make the tests fail. "
                            f"It is untested:\n{proc.stdout}")
        for label in m.expect_fail:
            self.assertTrue(failed(label, proc.stdout),
                            f"removing the {m.name} should have failed {label!r}; "
                            f"failures were {failing_lines(proc.stdout)}")


def _make_case(m: Mutation):
    def case(self: GuaranteeMutationTests) -> None:
        self._mutate(m)
    case.__doc__ = f"deleting the {m.name} must turn the suite red"
    return case


for _i, _m in enumerate(MUTATIONS):
    _slug = "".join(c if c.isalnum() else "_" for c in _m.name).strip("_").lower()
    setattr(GuaranteeMutationTests, f"test_mutation_{_i:02d}_{_slug}", _make_case(_m))


if __name__ == "__main__":
    if shutil.which("gcc") is None:
        print("gcc not found: the settings persistence host tests cannot run", file=sys.stderr)
        raise SystemExit(1)
    unittest.main(verbosity=2)
