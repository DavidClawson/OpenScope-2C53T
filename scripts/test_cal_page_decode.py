#!/usr/bin/env python3
"""Tests for scripts/cal_page_decode.py against the two archived pages.

The numbers asserted here are not derived: they are what the DEVICE printed.
`make guest-caldump` diffs the live page against the embedded unit #1 archive in
three regions, and on the never-flashed unit #3 it showed

    hdr/settings 7/48    CALIBRATION 127/256    tail 0/208     CRC32 711FB4A9

(issue #28, 2026-10-01; unit #1's own CRC32 is 59E91404). If this decoder ever
disagrees with those, the decoder is wrong, not the device.

The archived pages are tracked with `git add -f` because *.bin is ignored. A
checkout that is missing one FAILS here rather than skipping: a skipped test is
exactly how a missing artifact goes unnoticed (see scripts/run_tests.py).
"""

from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts" / "cal_page_decode.py"
UNIT1 = ROOT / "archive" / "factory_cal" / "unit1_mcu_settings_page_0x08006000.bin"
UNIT3 = ROOT / "archive" / "factory_cal" / "unit3_pristine_mcu_settings_page_0x08006000.bin"
README = ROOT / "archive" / "factory_cal" / "README.md"
CAL_DUMP_C = ROOT / "firmware" / "src" / "util" / "cal_dump.c"

UNIT1_SHA256 = "6004374abb123b99aa8a2516f66b0f6465032e817ebd3fb463c2559accdf621f"
UNIT3_SHA256 = "464f921d61f1e32c10e20bf7a9304c6835a823ea0077b82beec638fa69e34038"


def load_module():
    spec = importlib.util.spec_from_file_location("cal_page_decode", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load cal_page_decode")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


dec = load_module()


class ArchivedPageTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (UNIT1, UNIT3):
            if not path.exists():
                raise AssertionError(
                    f"{path.relative_to(ROOT)} is missing. It is tracked (git add -f, "
                    f"because *.bin is ignored); restore it rather than skipping this suite."
                )
        cls.unit1 = dec.load_page(UNIT1)
        cls.unit3 = dec.load_page(UNIT3)


class TestArchivedPages(ArchivedPageTestCase):
    def test_crc32_matches_what_the_device_printed(self):
        self.assertEqual(dec.crc32_hex(self.unit1), "59E91404")
        self.assertEqual(dec.crc32_hex(self.unit3), "711FB4A9")

    def test_sha256_of_the_archived_files(self):
        self.assertEqual(dec.sha256_hex(self.unit1), UNIT1_SHA256)
        self.assertEqual(dec.sha256_hex(self.unit3), UNIT3_SHA256)

    def test_both_pages_have_513_non_ff_bytes(self):
        self.assertEqual(dec.non_ff(self.unit1), 513)
        self.assertEqual(dec.non_ff(self.unit3), 513)

    def test_signatures_are_valid_normal_restore(self):
        for page in (self.unit1, self.unit3):
            ok, text = dec.check_signature(page)
            self.assertTrue(ok, text)
            self.assertIn("valid-normal-restore", text)

    def test_region_diff_matches_the_device(self):
        regions = {name: (n, hi - lo) for name, lo, hi, n in dec.region_diff(self.unit1, self.unit3)}
        self.assertEqual(regions["hdr/settings"], (7, 48))
        self.assertEqual(regions["CALIBRATION"], (127, 256))
        self.assertEqual(regions["tail"], (0, 208))

    def test_diff_is_symmetric_and_self_diff_is_empty(self):
        ab = dec.region_diff(self.unit1, self.unit3)
        ba = dec.region_diff(self.unit3, self.unit1)
        self.assertEqual(ab, ba)
        for page in (self.unit1, self.unit3):
            self.assertTrue(all(n == 0 for *_, n in dec.region_diff(page, page)))

    def test_rest_of_the_page_is_identical(self):
        d = dec.diff_pages(self.unit1, self.unit3)
        self.assertEqual(d["rest"][3], 0)
        self.assertEqual(d["whole"], 7 + 127 + 0)

    def test_header_diff_offsets(self):
        # Quoted from the issue #28 comment on unit #3 (and visible on its LCD).
        d = dec.diff_pages(self.unit1, self.unit3)
        self.assertEqual(d["header_bytes"], [0x03, 0x04, 0x05, 0x06, 0x0D, 0x12, 0x25])

    def test_tail_is_byte_identical_including_the_ram_pointers(self):
        d = dec.diff_pages(self.unit1, self.unit3)
        self.assertEqual(d["tail_bytes"], [])
        tail = self.unit3[0x130:0x200]
        self.assertTrue(any(tail), "tail should not be all zeros")
        pointers = [int.from_bytes(tail[i:i + 4], "little") for i in range(0, len(tail), 4)]
        self.assertTrue(any(0x20020000 <= p < 0x20040000 for p in pointers))

    def test_calibration_breakdown_isolates_the_settings_byte(self):
        # 127 = 1 (saved_mode_word high byte, a settings field) + 120 (table) + 6 (trailing).
        parts = {name.split(" ")[0]: n for name, lo, hi, n in dec.calibration_breakdown(self.unit1, self.unit3)}
        self.assertEqual(parts, {"settings": 1, "table": 120, "trailing": 6})
        self.assertEqual(sum(parts.values()), 127)

    def test_odd_words_are_flagged_with_the_known_values(self):
        odd = {w["offset"]: (w["a"], w["b"]) for w in dec.diff_pages(self.unit1, self.unit3)["odd_words"]}
        self.assertEqual(odd[0x128], (0x04FB, 0xFC04))
        self.assertEqual(odd[0x12C], (0xB6B8, 0xB8B7))
        # And they are the ONLY words that break the high-byte pattern on either page.
        for page in (self.unit1, self.unit3):
            self.assertEqual([off for off, _ in dec.off_pattern_words(page)], [0x128, 0x12C])
        # The odd words are excluded from the plain table deltas.
        plain = {w["offset"] for w in dec.diff_pages(self.unit1, self.unit3)["table_deltas"]}
        self.assertFalse(plain & {0x128, 0x12C})

    def test_sentinel_is_valid_on_both_pages(self):
        self.assertEqual(dec.u16(self.unit1, dec.SENTINEL_OFFSET), 0x0CD9)
        self.assertEqual(dec.u16(self.unit3, dec.SENTINEL_OFFSET), 0x0C98)

    def test_word_deltas_sum_back_to_the_pages(self):
        for w in dec.word_deltas(self.unit1, self.unit3):
            self.assertEqual(dec.u16(self.unit1, w["offset"]) + w["delta"], dec.u16(self.unit3, w["offset"]))
        self.assertEqual(len(dec.word_deltas(self.unit1, self.unit3)), 125)


class TestSignatureGuard(ArchivedPageTestCase):
    def mutated(self, sig: int) -> bytes:
        page = bytearray(self.unit3)
        page[0] = sig
        return bytes(page)

    def test_wrong_signature_is_reported(self):
        for sig in (0x00, 0x54, 0x56, 0xA5, 0xFF):
            ok, text = dec.check_signature(self.mutated(sig))
            self.assertFalse(ok, f"0x{sig:02X} must not be accepted")
            self.assertIn("INVALID SIGNATURE", text)
            self.assertIn(f"0x{sig:02X}", text)

    def test_meter_restore_signature_is_valid(self):
        ok, text = dec.check_signature(self.mutated(0xAA))
        self.assertTrue(ok)
        self.assertIn("valid-meter-restore", text)

    def test_decode_output_carries_the_warning(self):
        text = dec.decode_page(self.mutated(0x12), "mutated")
        self.assertIn("INVALID SIGNATURE", text)
        self.assertNotIn("INVALID SIGNATURE", dec.decode_page(self.unit3, "ok"))

    def test_diff_output_names_the_bad_side(self):
        text = dec.format_diff(self.unit1, self.mutated(0x12), "good", "bad")
        self.assertIn("WARNING B", text)
        self.assertNotIn("WARNING A", text)

    def run_cli(self, *args: str) -> subprocess.CompletedProcess:
        return subprocess.run([sys.executable, str(SCRIPT), *args], capture_output=True, text=True, cwd=ROOT)

    def test_cli_exit_status_and_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "bad_sig.bin"
            bad.write_bytes(self.mutated(0x12))
            proc = self.run_cli(str(bad))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            self.assertIn("INVALID SIGNATURE", proc.stdout)

            good = self.run_cli(str(UNIT3))
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertIn("711FB4A9", good.stdout)

            diff = self.run_cli("--diff", str(UNIT1), str(UNIT3))
            self.assertEqual(diff.returncode, 0, diff.stderr)
            for needle in ("7/48", "127/256", "0/208", "59E91404", "711FB4A9", "ODD"):
                self.assertIn(needle, diff.stdout)

            short = Path(tmp) / "short.bin"
            short.write_bytes(self.unit3[:100])
            bad_len = self.run_cli(str(short))
            self.assertEqual(bad_len.returncode, 2)
            self.assertIn("need 4096 bytes", bad_len.stderr)

            # A bootloader-style image (0x08000000..) holds the page at 0x6000.
            image = Path(tmp) / "image.bin"
            image.write_bytes(b"\xFF" * 0x6000 + self.unit3)
            refused = self.run_cli(str(image))
            self.assertEqual(refused.returncode, 2)
            self.assertIn("--offset", refused.stderr)
            at_offset = self.run_cli("--offset", "0x6000", str(image))
            self.assertEqual(at_offset.returncode, 0, at_offset.stderr)
            self.assertIn("711FB4A9", at_offset.stdout)


class TestNoDrift(unittest.TestCase):
    def test_region_table_matches_cal_dump_c(self):
        """The Python region bounds must be the ones the device uses."""
        text = CAL_DUMP_C.read_text()
        block = re.search(r"regions\[\]\s*=\s*\{(.*?)\};", text, re.DOTALL)
        self.assertIsNotNone(block, "regions[] table not found in cal_dump.c")
        found = [
            (int(lo, 16), int(hi, 16))
            for lo, hi in re.findall(r"\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*\"", block.group(1))
        ]
        self.assertEqual(found, [(lo, hi) for lo, hi, _ in dec.REGIONS])

    def test_cal_dump_no_longer_calls_the_tail_ram_garbage(self):
        text = CAL_DUMP_C.read_text()
        self.assertNotIn("differs on EVERY write", text)
        self.assertNotIn("RAM gbg", text)

    def test_readme_lists_both_archived_pages(self):
        text = README.read_text()
        for name, sha, crc in (
            (UNIT1.name, UNIT1_SHA256, "59E91404"),
            (UNIT3.name, UNIT3_SHA256, "711FB4A9"),
        ):
            self.assertIn(name, text)
            self.assertIn(sha, text)
            self.assertIn(crc, text)


if __name__ == "__main__":
    unittest.main()
