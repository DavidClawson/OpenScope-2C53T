#!/usr/bin/env python3
"""The factory IAP bootloader archive must not carry a calibration page (issue #38).

`archive/factory_iap_bootloader_2C53T.bin` is what the DFU guide's full factory
restore writes to 0x08000000 on someone else's unit. Until #38 it was 0x7000 bytes,
so it also covered 0x08006000-0x08006FFF, stock's settings page, and every unit
restored from it got bench unit #1's calibration. It is now the 0x6000 bytes of IAP
code only; unit #1's page lives in archive/factory_cal/ under its own name.

What these tests hold in place:

- the archive is 0x6000 bytes with the sha256 below, and ends on a flash-page
  boundary of the ROM DFU's own layout string (quoted in docs/dfu_mode_guide.md),
  so writing it erases and programs only pages below 0x08006000;
- its vector table points only inside those 0x6000 bytes, so nothing it boots
  through was cut off;
- it is the code every unit shares: with never-flashed unit #3's page appended it
  rebuilds unit #3's own 0x7000-byte dump, hash for hash;
- the restore guide and archive/factory_cal/README.md quote the same size and hash;
- scripts/dump_factory_bootloader.py, which writes this archive by default,
  never reads 0x08006000 and up: run against a simulated shell over a flash image
  that does have a page there, it reproduces the archive byte for byte.

Mutations checked by hand on 2026-10-05 (each must turn something red):

  1. archive restored to the 28,672-byte original (git show 22730c0:...)
       -> test_size_is_the_iap_code_only, test_sha256,
          test_ends_on_a_flash_page_boundary, test_the_cut_bytes_are_kept_as_unit1s_page,
          test_same_code_as_a_never_flashed_unit,
          test_dump_stops_below_the_settings_page FAIL (6)
  2. one byte of the IAP code flipped (offset 0x1000)
       -> test_sha256, test_same_code_as_a_never_flashed_unit,
          test_the_cut_bytes_are_kept_as_unit1s_page,
          test_dump_stops_below_the_settings_page FAIL (4)
  3. dump_factory_bootloader.SIZE = 0x7000
       -> test_dump_stops_below_the_settings_page FAIL ("dump read up to 0x08007000")
  4. guide back to "(28,672 bytes, sha256 `0c9ec7d6…`)"
       -> test_guide_quotes_size_and_sha256 FAIL
  5. dump_factory_bootloader.WORD_RE never matches (simulated shell answers
     unparsed) -> test_dump_stops_below_the_settings_page ERROR (SystemExit), so
     the simulated shell's output format is actually what the parser reads
"""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import re
import struct
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
ARCHIVE = ROOT / "archive" / "factory_iap_bootloader_2C53T.bin"
UNIT1_PAGE = ROOT / "archive" / "factory_cal" / "unit1_mcu_settings_page_0x08006000.bin"
UNIT3_PAGE = ROOT / "archive" / "factory_cal" / "unit3_pristine_mcu_settings_page_0x08006000.bin"
GUIDE = ROOT / "docs" / "dfu_mode_guide.md"
CAL_README = ROOT / "archive" / "factory_cal" / "README.md"
DUMP_SCRIPT = ROOT / "scripts" / "dump_factory_bootloader.py"

FLASH_BASE = 0x08000000
SETTINGS_PAGE = 0x08006000  # stock's per-unit settings + calibration page
IAP_SIZE = SETTINGS_PAGE - FLASH_BASE  # 0x6000
ARCHIVE_SHA256 = "7a6201ee16ce155675f1d235e468edfdce7a736318f2591d54a221d4ad06566b"
# The pre-#38 file (22730c0): 0x7000 bytes, i.e. this archive + unit #1's page.
OLD_ARCHIVE_SHA256 = "0c9ec7d642d233ea09c87274867ad3460e1dbec37c4332f7edb8f836175630c7"
# Never-flashed unit #3, 0x08000000-0x08006FFF, read with dump_factory_bootloader.py on
# 2026-09-30 (0x7000 bytes; the dump itself is not in the repo, its two halves are).
UNIT3_DUMP_SHA256 = "dc3d8b7daa1fe0224d140a4fc9191daaa27b7c1a2fff5f523f9047d7235f8dae"


def require(path: Path) -> bytes:
    if not path.exists():
        raise AssertionError(
            f"{path.relative_to(ROOT)} is missing. It is tracked in git; restore it "
            f"rather than skipping this suite."
        )
    return path.read_bytes()


def dfu_page_size_from_guide() -> int:
    """Page size from the ROM DFU layout string the guide quotes from `dfu-util -l`."""
    m = re.search(r"@Internal Flash\s+/0x08000000/(\d+)\*(\d+)Kg", GUIDE.read_text())
    if m is None:
        raise AssertionError("docs/dfu_mode_guide.md no longer quotes the ROM DFU layout string")
    return int(m.group(2)) * 1024


class TestArchiveFile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.blob = require(ARCHIVE)

    def test_size_is_the_iap_code_only(self):
        self.assertEqual(
            len(self.blob), IAP_SIZE,
            f"archive is {len(self.blob):#x} bytes; anything past {IAP_SIZE:#x} is "
            f"0x08006000 and up, a unit's own calibration page (#38)",
        )

    def test_sha256(self):
        digest = hashlib.sha256(self.blob).hexdigest()
        self.assertNotEqual(digest, OLD_ARCHIVE_SHA256, "the pre-#38 28 KB file is back")
        self.assertEqual(digest, ARCHIVE_SHA256)

    def test_ends_on_a_flash_page_boundary(self):
        page = dfu_page_size_from_guide()
        self.assertEqual(page, 0x800)
        end = FLASH_BASE + len(self.blob)
        # dfu-util erases each page the file touches; ending on a boundary at or
        # below the settings page means step 1 of the restore cannot erase it.
        self.assertEqual(len(self.blob) % page, 0)
        self.assertLessEqual(end, SETTINGS_PAGE)

    def test_vectors_point_inside_the_file(self):
        sp = struct.unpack_from("<I", self.blob, 0)[0]
        self.assertTrue(0x20000000 <= sp < 0x20038000, f"initial SP {sp:#010x}")
        # 16 system vectors; zero entries are unused slots.
        vectors = struct.unpack_from("<15I", self.blob, 4)
        used = [v for v in vectors if v != 0]
        self.assertTrue(used)
        for v in used:
            self.assertEqual(v & 1, 1, f"vector {v:#010x} is not Thumb")
            self.assertTrue(
                FLASH_BASE <= (v & ~1) < FLASH_BASE + len(self.blob),
                f"vector {v:#010x} points outside the archived 0x6000 bytes",
            )

    def test_the_cut_bytes_are_kept_as_unit1s_page(self):
        # Not regenerable data: the 4096 bytes the trim removed must still be in
        # the repo under their own name. Their content is pinned by sha256 in
        # test_cal_page_decode.py; here only that the page exists and is a page.
        page = require(UNIT1_PAGE)
        self.assertEqual(len(page), 0x1000)
        self.assertEqual(page[0], 0x55)
        old = hashlib.sha256(self.blob + page).hexdigest()
        self.assertEqual(old, OLD_ARCHIVE_SHA256,
                         "archive + unit #1's page no longer rebuilds the pre-#38 file")

    def test_same_code_as_a_never_flashed_unit(self):
        # The archived 0x6000 bytes are the IAP code every unit shares, not unit #1's:
        # followed by unit #3's own page they rebuild unit #3's 28 KB dump exactly.
        page = require(UNIT3_PAGE)
        self.assertEqual(hashlib.sha256(self.blob + page).hexdigest(), UNIT3_DUMP_SHA256)


class TestDocsQuoteTheFile(unittest.TestCase):
    def test_guide_quotes_size_and_sha256(self):
        text = GUIDE.read_text()
        m = re.search(
            r"\[`archive/factory_iap_bootloader_2C53T\.bin`\]\([^)]*\) \(([\d,]+) bytes[^)]*?"
            r"sha256 `([0-9a-f]{64})`\)",
            text,
        )
        self.assertIsNotNone(m, "restore guide no longer states the archive's size and full sha256")
        self.assertEqual(int(m.group(1).replace(",", "")), IAP_SIZE)
        self.assertEqual(m.group(2), ARCHIVE_SHA256)

    def test_factory_cal_readme_quotes_sha256(self):
        text = CAL_README.read_text()
        self.assertIn(ARCHIVE_SHA256, text)
        self.assertIn("24,576 (`0x6000`)", text)


def load_dump_module():
    """Import the dump script with a stand-in `serial` (pyserial is not a test dependency)."""
    fake_serial = types.ModuleType("serial")
    fake_serial.Serial = None  # replaced per test
    saved = sys.modules.get("serial")
    sys.modules["serial"] = fake_serial
    try:
        spec = importlib.util.spec_from_file_location("dump_factory_bootloader", DUMP_SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    finally:
        if saved is None:
            del sys.modules["serial"]
        else:
            sys.modules["serial"] = saved
    return module


class SimulatedShell:
    """Answers `mem read <addr> <n>` the way usb_debug.c's cmd_mem_read prints it."""

    MEM_READ = re.compile(rb"mem read ([0-9A-Fa-f]{8}) (\d+)\r\n")

    def __init__(self, flash: bytes):
        self.flash = flash
        self.out = b""
        self.reads: list[tuple[int, int]] = []

    def __call__(self, port, baud, timeout=None):  # stands in for serial.Serial(...)
        return self

    def reset_input_buffer(self):
        self.out = b""

    def write(self, data: bytes):
        m = self.MEM_READ.fullmatch(data)
        if m is None:
            raise AssertionError(f"unexpected shell command {data!r}")
        addr, count = int(m.group(1), 16), min(int(m.group(2)), 64)
        self.reads.append((addr, count))
        lines = []
        for i in range(count):
            a = addr + 4 * i
            off = a - FLASH_BASE
            word = struct.unpack_from("<I", self.flash, off)[0]
            if i % 4 == 0:
                lines.append(f"0x{a:08X}:")
            lines[-1] += f" {word:08X}"
        self.out += ("\r\n".join(lines) + "\r\n").encode()

    def flush(self):
        pass

    def close(self):
        pass

    def take(self) -> str:
        out, self.out = self.out, b""
        return out.decode()


class TestDumpScript(unittest.TestCase):
    def test_dump_stops_below_the_settings_page(self):
        iap = require(ARCHIVE)
        page = require(UNIT1_PAGE)
        # A unit with the factory IAP and a calibration page right after it.
        flash = iap + page + b"\xA5" * 0x1000
        shell = SimulatedShell(flash)
        mod = load_dump_module()

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "dump.bin"
            with mock.patch.object(mod.serial, "Serial", shell), \
                 mock.patch.object(mod, "find_port", return_value="/dev/cu.usbmodemSIM"), \
                 mock.patch.object(mod, "drain", lambda ser, **kw: ser.take()), \
                 mock.patch.object(mod.time, "sleep", lambda s: None), \
                 mock.patch.object(sys, "argv", [str(DUMP_SCRIPT), str(out)]), \
                 contextlib.redirect_stdout(io.StringIO()) as log:
                mod.main()
            dumped = out.read_bytes()

        highest = max(addr + 4 * count for addr, count in shell.reads)
        self.assertLessEqual(
            highest, SETTINGS_PAGE,
            f"dump read up to {highest:#010x}: the settings page would land in the archive",
        )
        self.assertEqual(len(dumped), IAP_SIZE)
        self.assertEqual(dumped, iap)
        self.assertEqual(hashlib.sha256(dumped).hexdigest(), ARCHIVE_SHA256)
        self.assertIn("OK (in-region thumb)", log.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
