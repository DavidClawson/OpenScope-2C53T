#!/usr/bin/env python3
"""The RAM-resident installer must never branch into flash.

fwl_ram_install() (firmware/src/drivers/fw_loader.c) erases and reprograms
the app slot from RAM with interrupts off. Anything it calls must also be in
RAM: the flash it would otherwise jump to is the flash it is erasing.

That is exactly what went wrong in the maintainer-built v0.4.0 release
(EXP-57, EXP-59, EXP-62 A/B): its compiler turned the 0xFF fill after
rf_w25q_read_raw() into an unguarded `bl memset` through a veneer placed in
.data, and newlib's memset sat in app-slot page 1. Page 0 and 1 erased, the
next call fetched 0xFFFFFFFF and the unit hung with USB enumerated but
silent. Our GCC 15.3 -Os build inlined the fill, so the same source worked.
The attribute on RF now forbids the transformation; this test proves, on
the linked ELF, that no branch from a RAM function leaves RAM, so a toolchain
change cannot bring the bug back silently.

It builds `make guest` into its own BUILD_DIR (the layout fwapply installs),
then disassembles every function placed in SRAM. test_checker_catches_the_
release_pattern feeds the checker the exact shape of the bad code and
requires it to fail.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIRMWARE = REPO / "firmware"
BUILD_DIR = "build-gate-ramfunc"
RAM_LO, RAM_HI = 0x20000000, 0x20040000

NM = "arm-none-eabi-nm"
OBJDUMP = "arm-none-eabi-objdump"

# objdump -d lines like "20000286:  f000 f863  bl  20000350 <__memset_veneer>"
BRANCH_RE = re.compile(
    r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+"
    r"(bl|blx|b|b\.w|b\.n|bx|cb[nz]z|b(?:eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)(?:\.[nw])?)\s+"
    r"(?:r\d+,\s*)?(?:0x)?([0-9a-f]+)(?:\s+<([^>]+)>)?",
    re.I)
LDR_PC_RE = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+ldr(?:\.w)?\s+pc\s*,", re.I)
BX_REG_RE = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+(?:bx|blx)\s+(r\d+|ip)\b", re.I)


def check_disassembly(text: str) -> list:
    """Return a list of violations (strings) found in objdump -d output of a
    RAM-resident function. Empty list = isolated."""
    bad = []
    for line in text.splitlines():
        m = BRANCH_RE.match(line)
        if m:
            addr, mnem, target, sym = m.group(1), m.group(2).lower(), int(m.group(3), 16), m.group(4)
            if mnem == "bx" and target < 0x10:
                continue
            if not (RAM_LO <= target < RAM_HI):
                bad.append(f"{addr}: {mnem} to 0x{target:08X}{' <' + sym + '>' if sym else ''} (outside RAM)")
            elif sym and "veneer" in sym.lower():
                bad.append(f"{addr}: {mnem} through a veneer <{sym}> (a veneer in RAM only exists to leave RAM)")
            continue
        if LDR_PC_RE.match(line):
            bad.append(f"{LDR_PC_RE.match(line).group(1)}: ldr pc (computed jump: veneer shape)")
    return bad


def ram_functions(elf: Path) -> list:
    out = subprocess.run([NM, "-S", "--defined-only", str(elf)], capture_output=True, text=True, check=True).stdout
    funcs = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2] in ("t", "T"):
            addr = int(parts[0], 16)
            if RAM_LO <= addr < RAM_HI:
                funcs.append((parts[3], addr, int(parts[1], 16)))
    return funcs


def build_guest() -> Path:
    env = dict(os.environ)
    subprocess.run(["make", "guest", f"BUILD_DIR={BUILD_DIR}"], cwd=FIRMWARE, env=env,
                   capture_output=True, text=True, check=True)
    return FIRMWARE / BUILD_DIR / "firmware.elf"


class TestRamfuncIsolated(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for tool in (NM, OBJDUMP, "make"):
            if shutil.which(tool) is None:
                raise unittest.SkipTest(f"{tool} not found: cannot inspect the RAM installer")
        try:
            cls.elf = build_guest()
        except subprocess.CalledProcessError as e:
            raise AssertionError(f"make guest failed:\n{e.stdout[-2000:]}\n{e.stderr[-2000:]}")

    def test_installer_is_present_in_ram(self):
        names = {n for n, _, _ in ram_functions(self.elf)}
        for expected in ("fwl_ram_install", "rf_w25q_read_raw", "rf_spi2_xfer"):
            self.assertIn(expected, names, f"{expected} is not in SRAM: the RF section attribute is gone")

    def test_no_branch_leaves_ram(self):
        funcs = ram_functions(self.elf)
        self.assertTrue(funcs)
        for name, addr, size in funcs:
            with self.subTest(function=name):
                self.assertNotIn("veneer", name.lower(),
                                 f"{name} at 0x{addr:08X}: a veneer linked into RAM means a RAM "
                                 f"function calls out to flash — exactly the v0.4.0 hang")
                dis = subprocess.run([OBJDUMP, "-d", f"--disassemble={name}", str(self.elf)],
                                     capture_output=True, text=True, check=True).stdout
                self.assertIn(f"<{name}>:", dis, f"objdump did not disassemble {name}")
                bad = check_disassembly(dis)
                self.assertEqual(bad, [], f"{name} leaves RAM:\n  " + "\n  ".join(bad))
                for m in BX_REG_RE.finditer(dis):
                    # bx lr is the return; any other register-indirect jump is suspicious
                    self.assertEqual(m.group(1).lower(), "lr", f"{name}: indirect jump via {m.group(1)}")

    def test_checker_catches_the_release_pattern(self):
        """The exact shape found in openscope-2c53t-v0.4.0-coldtrace.bin."""
        release_like = (
            "20000280:\t4b30      \tldr\tr3, [pc, #192]\n"
            "20000286:\tf000 f863 \tbl\t20000350 <__memset_veneer>\n"
            "20000350:\tf85f f000 \tldr.w\tpc, [pc]\t; 20000354\n"
        )
        bad = check_disassembly(release_like)
        self.assertEqual(len(bad), 2, bad)
        direct = "20000286:\tf007 fce7 \tbl\t8007c58 <memset>\n"
        self.assertEqual(len(check_disassembly(direct)), 1)
        fine = ("200001ec:\tf7ff ff50 \tbl\t20000090 <rf_spi2_xfer>\n"
                "20000230:\td1f9      \tbne.n\t20000226 <fwl_ram_install+0x116>\n"
                "20000340:\t4770      \tbx\tlr\n")
        self.assertEqual(check_disassembly(fine), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
