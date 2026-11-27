#!/usr/bin/env python3
"""Build and run the CDC wedge evidence host tests, then prove they can fail.

WHY THE SECOND HALF EXISTS
--------------------------
The evidence record (firmware/src/util/usb_evidence.c, issue #39) lives in
.noinit so it survives the reset that recovers a wedged CDC port. After a cold
power-up those same bytes are SRAM noise, and the guard that keeps noise from
being printed as "previous session: STALL PENDING" is magic AND checksum. A
guard nobody has seen fail is a guard nobody knows works.

So, same method as test_cal_backup.py: for each guard, copy the source,
delete the guard, rebuild, and REQUIRE firmware/tests/test_usb_evidence.c to
go red. A mutation that still passes is a failure here. A substitution string
that is no longer found (the code was refactored) fails loudly instead of
skipping, and so does a mutant that does not compile (test_remote_proto.py's
rule: a build failure proves nothing about the guard).
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
SRC = REPO / "firmware" / "src" / "util" / "usb_evidence.c"
HDR = REPO / "firmware" / "src" / "util" / "usb_evidence.h"
TEST_C = REPO / "firmware" / "tests" / "test_usb_evidence.c"

BASE_CFLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O1"]
MUTANT_CFLAGS = ["-std=c11", "-Wall", "-Wextra", "-O1"]   # a mutant may leave an unused name


class BuildFailed(Exception):
    """A mutant that does not compile proves nothing about the guard."""


@dataclass
class Mutation:
    """One guard, weakened. The C suite must exit non-zero against it."""

    name: str
    old: str
    new: str


MUTATIONS: tuple[Mutation, ...] = (
    Mutation(
        name="checksum check dropped -> noise carrying the magic trusted",
        old="if (usb_ev_checksum(ev) != ev->check) {\n        return false;\n    }",
        new="if (0) {\n        return false;\n    }",
    ),
    Mutation(
        name="magic check dropped -> a foreign sealed record trusted",
        old="if (ev->magic != USB_EV_MAGIC) {\n        return false;\n    }",
        new="if (0) {\n        return false;\n    }",
    ),
    Mutation(
        name="checksum covers only the header -> corrupt evidence trusted",
        old="for (size_t i = 0; i < offsetof(usb_evidence_t, check); i++) {",
        new="for (size_t i = 0; i < 8u; i++) {",
    ),
    Mutation(
        name="a stall no longer marks the record pending",
        old="uint8_t f = USB_EV_FLAG_PENDING;",
        new="uint8_t f = 0;",
    ),
    Mutation(
        name="a completed send no longer clears pending -> false wedge report",
        old="ev->flags &= (uint8_t)~USB_EV_FLAG_PENDING;",
        new="(void)0;",
    ),
    Mutation(
        name="stall not sealed -> the wedge evidence is discarded at boot",
        old="        ev->tx_host_slow++;\n    }\n    usb_ev_seal(ev);",
        new="        ev->tx_host_slow++;\n    }\n",
    ),
    Mutation(
        name="alive pass not sealed -> every record reads as torn",
        old="ev->rx_gap_timeouts = gap;\n    usb_ev_seal(ev);",
        new="ev->rx_gap_timeouts = gap;",
    ),
    Mutation(
        name="new session not cleared -> previous evidence reported twice",
        old="    memset(ev, 0, sizeof(*ev));\n    ev->magic = USB_EV_MAGIC;",
        new="    ev->magic = USB_EV_MAGIC;",
    ),
    Mutation(
        name="invalid record copied as 'previous' -> power-up noise printed",
        old="        memset(prev, 0, sizeof(*prev));",
        new="        *prev = *ev;",
    ),
    Mutation(
        name="heal does not snapshot its stall -> the wedge's endpoint state is overwritten (EXP-66)",
        old="    ev->heal_stall_tick = ev->stall_tick;\n    ev->heal_stall_ept = ev->stall_ept;",
        new="",
    ),
    Mutation(
        name="session number not advanced -> a reset looks like a power cycle",
        old="seq = (uint16_t)(prev->seq + 1u);",
        new="seq = prev->seq;",
    ),
)


def compile_and_run(source_text: str, workdir: Path, cflags: list[str]) -> subprocess.CompletedProcess:
    (workdir / "usb_evidence.c").write_text(source_text)
    shutil.copy(HDR, workdir / "usb_evidence.h")
    shutil.copy(TEST_C, workdir / "test_usb_evidence.c")
    binary = workdir / "t"
    build = subprocess.run(
        ["gcc", *cflags, "-o", str(binary),
         str(workdir / "test_usb_evidence.c"), str(workdir / "usb_evidence.c"),
         "-I", str(workdir)],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        raise BuildFailed(build.stderr)
    return subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)


class TestUsbEvidence(unittest.TestCase):
    def test_source_files_present(self):
        for path in (SRC, HDR, TEST_C):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_base_suite_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = compile_and_run(SRC.read_text(), Path(tmp), BASE_CFLAGS)
        self.assertEqual(proc.returncode, 0,
                         f"base suite failed:\n{proc.stdout}\n{proc.stderr}")

    def test_unit_has_no_section_attribute(self):
        # The .noinit placement belongs to the firmware (usb_debug.c). Here it
        # would be a hard error under the host's Mach-O clang and kill this
        # suite, and it would tie the pure unit to one linker script.
        self.assertNotIn("section(", SRC.read_text())

    def test_mutations_are_caught(self):
        base = SRC.read_text()
        for m in MUTATIONS:
            with self.subTest(mutation=m.name):
                self.assertEqual(base.count(m.old), 1,
                                 f"mutation string not found exactly once (refactor?): {m.name}")
                mutated = base.replace(m.old, m.new, 1)
                self.assertNotEqual(mutated, base, "replacement was a no-op")
                with tempfile.TemporaryDirectory() as tmp:
                    try:
                        proc = compile_and_run(mutated, Path(tmp), MUTANT_CFLAGS)
                    except BuildFailed as exc:
                        self.fail(f"mutant '{m.name}' does not compile, so it proves "
                                  f"nothing:\n{exc}")
                self.assertNotEqual(
                    proc.returncode, 0,
                    f"mutation '{m.name}' did NOT make the suite fail — "
                    f"the guard is not tested.\n{proc.stdout}")


if __name__ == "__main__":
    if shutil.which("gcc") is None:
        # run_tests.py reports this form as a skip, by name, rather than a pass.
        print("skipped 'gcc not found: the usb-evidence host tests cannot run'")
        sys.exit(0)
    unittest.main(verbosity=2)
