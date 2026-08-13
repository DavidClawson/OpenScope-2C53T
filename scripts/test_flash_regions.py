#!/usr/bin/env python3
"""Build and run the W25Q region-layer host tests, then prove they can fail.

WHY THE SECOND HALF EXISTS
--------------------------
This project's recurring failure mode is an instrument that reports confidently
on state it cannot observe: status reads taken at /2, a floating MISO, a `status`
shell printing a refusal on a configured device. A guard test has exactly the
same failure mode — it passes whether or not the guard is doing anything, and
nobody notices until the day the guard is needed.

So this suite does not only run firmware/tests/test_flash_regions.c against the
real flash_regions.c. For each guard in the region layer it takes a copy of the
source, deletes that guard, rebuilds, and REQUIRES the C suite to go red on the
specific tests that cover it. A mutation that still passes is reported as a
failure here, because it means the test is not testing anything.

The mutations are literal source substitutions. If a substitution string is no
longer found — because the code was refactored — this suite fails loudly rather
than silently skipping the check.
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
SRC = REPO / "firmware" / "src" / "drivers" / "flash_regions.c"
HDR = REPO / "firmware" / "src" / "drivers" / "flash_regions.h"
TEST_C = REPO / "firmware" / "tests" / "test_flash_regions.c"

CFLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O1"]


@dataclass
class Mutation:
    """One guard, deleted. `expect_fail` are the C test labels that must go red."""

    name: str
    old: str
    new: str
    expect_fail: tuple[str, ...] = ()
    # For mutations the model aborts on outright (rather than reporting a failed
    # test), the substring the run must print.
    expect_output: str = ""


MUTATIONS: tuple[Mutation, ...] = (
    Mutation(
        name="read-only intersection check (absolute-address path)",
        old="""        bool intersects = (addr < r_end) && (r->start < end);
        if (intersects && !region_writable(r)) {
            return FLASH_REGION_ERR_READ_ONLY;
        }""",
        new="""        bool intersects = (addr < r_end) && (r->start < end);
        (void)intersects;""",
        expect_fail=(
            "write to a read-only region is refused",
            "erase of a read-only region is refused",
            "write crossing into read-only fails closed",
            "20k random ranges never touch read-only",
        ),
    ),
    Mutation(
        name="read-only check in flash_region_write()",
        old="""    if (!region_writable(r)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_READ_ONLY;
    }
    if (!range_fits(offset, len, r->length)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BOUNDS;
    }
    return write_checked(r->start + offset, (const uint8_t *)data, len);""",
        new="""    if (!range_fits(offset, len, r->length)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BOUNDS;
    }
    return write_checked(r->start + offset, (const uint8_t *)data, len);""",
        expect_fail=("write to a read-only region is refused",),
    ),
    Mutation(
        name="bounds check in flash_region_write()",
        old="""    if (!range_fits(offset, len, r->length)) {
        g_stats.writes_refused++;
        return FLASH_REGION_ERR_BOUNDS;
    }
    return write_checked(r->start + offset, (const uint8_t *)data, len);""",
        new="""    return write_checked(r->start + offset, (const uint8_t *)data, len);""",
        expect_fail=("write crossing into read-only fails closed",),
    ),
    Mutation(
        name="read-only check in flash_region_erase()",
        old="""    if (!region_writable(r)) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_READ_ONLY;
    }
    if ((offset % FLASH_REGION_SECTOR_SIZE) != 0u""",
        new="""    if ((offset % FLASH_REGION_SECTOR_SIZE) != 0u""",
        expect_fail=("erase of a read-only region is refused",),
    ),
    Mutation(
        name="erase alignment check",
        old="""    if ((offset % FLASH_REGION_SECTOR_SIZE) != 0u || (len % FLASH_REGION_SECTOR_SIZE) != 0u) {
        g_stats.erases_refused++;
        return FLASH_REGION_ERR_ALIGN;
    }""",
        new="",
        expect_fail=("erase alignment is enforced",),
    ),
    Mutation(
        name="post-write verification",
        old="""    return verify_range(addr, data, len);
}""",
        new="""    if (addr == 0xFFFFFFFFu) { return verify_range(addr, data, len); }
    return FLASH_REGION_OK;
}""",
        expect_fail=("verify catches a silent write failure",),
    ),
    Mutation(
        name="no-op write elision",
        old="""    if (identical) {
        g_stats.writes_elided++;""",
        new="""    if (identical && addr == 0xFFFFFFFFu) {
        g_stats.writes_elided++;""",
        expect_fail=("identical write is elided",),
    ),
    Mutation(
        name="append no-op elision",
        old="""        if (same) {
            g_stats.writes_elided++;
            return FLASH_REGION_OK;
        }""",
        new="""        (void)same;""",
        expect_fail=("append is elided when unchanged",),
    ),
    Mutation(
        name="page-boundary splitting in program_verified()",
        old="""        uint32_t page_left = FLASH_REGION_PAGE_SIZE - ((addr + done) % FLASH_REGION_PAGE_SIZE);
        uint32_t n = len - done;
        if (n > page_left) n = page_left;
""",
        new="""        uint32_t n = len - done;
""",
        expect_output="MODEL VIOLATION: program crosses a page boundary",
    ),
    Mutation(
        name="needs-erase refusal (would become a silent read-modify-write)",
        old="""    if (!bitcompat) {""",
        new="""    if (!bitcompat && addr == 0xFFFFFFFFu) {""",
        expect_fail=("write needing a bit set is refused, not erased",),
    ),
)


def compile_and_run(source_text: str, workdir: Path) -> subprocess.CompletedProcess:
    """Compile the C suite against `source_text` and run it. Never raises on a
    non-zero exit: a red run is the expected result for a mutation."""
    (workdir / "flash_regions.c").write_text(source_text)
    shutil.copy(HDR, workdir / "flash_regions.h")
    shutil.copy(TEST_C, workdir / "test_flash_regions.c")

    binary = workdir / "test_flash_regions"
    build = subprocess.run(
        ["gcc", *CFLAGS, "-o", str(binary),
         str(workdir / "test_flash_regions.c"), str(workdir / "flash_regions.c"),
         "-I", str(workdir)],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        return build
    return subprocess.run([str(binary)], capture_output=True, text=True)


def failing_labels(output: str) -> set[str]:
    return {line[len("FAIL  "):].strip()
            for line in output.splitlines() if line.startswith("FAIL  ")}


class RegionLayerHostTests(unittest.TestCase):
    def test_sources_exist(self) -> None:
        for path in (SRC, HDR, TEST_C):
            self.assertTrue(path.exists(), f"{path} is missing")

    def test_host_suite_passes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(SRC.read_text(), Path(tmp))
        self.assertEqual(proc.returncode, 0,
                         f"region-layer host tests failed:\n{proc.stdout}\n{proc.stderr}")
        self.assertIn("tests OK", proc.stdout)
        self.assertNotIn("FAIL", proc.stdout)

    def test_host_suite_is_not_trivially_small(self) -> None:
        """Guard against the suite quietly shrinking to nothing."""
        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(SRC.read_text(), Path(tmp))
        count = int(proc.stdout.rsplit("\n", 2)[-2].split()[0])
        self.assertGreaterEqual(count, 20, f"only {count} region tests ran")


class GuardMutationTests(unittest.TestCase):
    """Each guard, deleted in turn. The C suite must notice."""

    def _mutate(self, m: Mutation) -> None:
        original = SRC.read_text()
        self.assertEqual(
            original.count(m.old), 1,
            f"mutation target for {m.name!r} appears {original.count(m.old)} times in "
            f"flash_regions.c (expected exactly 1). The code moved; update the mutation "
            f"rather than dropping the check.",
        )
        mutated = original.replace(m.old, m.new)

        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(mutated, Path(tmp))

        self.assertNotEqual(
            proc.returncode, 0,
            f"removing the {m.name} did NOT make the tests fail. The guard is untested:\n"
            f"{proc.stdout}\n{proc.stderr}",
        )
        if m.expect_output:
            self.assertIn(m.expect_output, proc.stdout,
                          f"removing the {m.name} did not produce the expected diagnostic:\n"
                          f"{proc.stdout}")
        got = failing_labels(proc.stdout)
        for label in m.expect_fail:
            self.assertIn(
                label, got,
                f"removing the {m.name} should have failed the test {label!r}; "
                f"failures were {sorted(got)}",
            )


def _make_case(m: Mutation):
    def case(self: GuardMutationTests) -> None:
        self._mutate(m)
    case.__doc__ = f"deleting the {m.name} must turn the suite red"
    return case


for _i, _m in enumerate(MUTATIONS):
    _slug = "".join(c if c.isalnum() else "_" for c in _m.name).strip("_").lower()
    setattr(GuardMutationTests, f"test_mutation_{_i:02d}_{_slug}", _make_case(_m))


if __name__ == "__main__":
    if shutil.which("gcc") is None:
        print("gcc not found: the region-layer host tests cannot run", file=sys.stderr)
        raise SystemExit(1)
    unittest.main(verbosity=2)
