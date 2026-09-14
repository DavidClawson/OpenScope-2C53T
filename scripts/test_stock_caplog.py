#!/usr/bin/env python3
"""Tests for the stock-caplog logger (scripts/stock_caplog/).

Three layers, each skipping honestly when its input is missing:
  1. pure: the Thumb-2 b.w encoder against the bytes of the reference patch,
     and the `mem read` reply parser;
  2. with the (gitignored) stock V1.2.0 image: the hook sites hold the six
     bytes each stub replays;
  3. with arm-none-eabi-gcc on PATH as well: the cave builds, fits its page,
     and the patched image changes exactly 12 bytes of the original;
  4. the checked-in sample dumps decode to the word map that PR (1) uses --
     stock's meter menu walked twice, then its AC/DC toggles -- and to a
     frontend that never moved a pin.
"""

from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
CAPLOG = REPO / "scripts" / "stock_caplog"
SAMPLES = CAPLOG / "samples"


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, CAPLOG / (name + ".py"))
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


build = load("build_caplog_image")
dump = load("caplog_dump")

# The ten pins this firmware drives per DMM submode, plus the meter gate and the
# AC/DC coupling relays. Stock's meter must not have touched any of them.
FRONTEND_PINS = {"C12", "E4", "E5", "E6", "A15", "A10", "B10", "B11", "B9", "A6",
                 "C11", "D12", "D13", "C1", "C2", "C4", "D2"}
KEY_MATRIX_PINS = {"A7", "A8", "B0", "C5", "C10", "E2", "E3"}
MENU_WALK = [0x050C, 0x0517, 0x050B, 0x050A, 0x0512, 0x0511, 0x0510, 0x0514]


def toolchain_present() -> bool:
    return all(shutil.which("arm-none-eabi-" + t) for t in build.TOOLS)


class BwEncoder(unittest.TestCase):
    def test_matches_the_reference_patch_bytes(self):
        # The two hooks as they were flashed on unit #2 (2026-09-07), cave stubs
        # at 0x080BE700 (tx) and 0x080BE712 (tick).
        self.assertEqual(build.enc_bw(0x0803E44C, 0x080BE700), bytes.fromhex("80f058b9"))
        self.assertEqual(build.enc_bw(0x0802A994, 0x080BE712), bytes.fromhex("93f0bdbe"))

    def test_round_trips_across_the_range(self):
        for src, dst in [(0x0803E44C, 0x080BE700), (0x0802A994, 0x080BE712),
                         (0x08007000, 0x080BEFFE), (0x080BE700, 0x08007002),
                         (0x08010000, 0x08010004), (0x08010000, 0x0800FFF0)]:
            self.assertEqual(build.dec_bw(src, build.enc_bw(src, dst)), dst, hex(dst))

    def test_rejects_odd_or_far_targets(self):
        with self.assertRaises(build.CaplogError):
            build.enc_bw(0x08007000, 0x08007001)
        with self.assertRaises(build.CaplogError):
            build.enc_bw(0x08007000, 0x08007000 + (1 << 25))


class MemReadParser(unittest.TestCase):
    def test_reassembles_words_and_checks_addresses(self):
        text = ("mem read 20037000 8\r\n"
                "0x20037000: 4C504143 0000A12F 00000010 0000035C\r\n"
                "0x20037010: 00001C50 7FCC0000 0000304C 00000038\r\n>")
        data = dump.parse_mem_read(text, 0x20037000, 8)
        self.assertEqual(data[:4], b"CAPL")
        self.assertEqual(len(data), 32)

    def test_dropped_line_raises_instead_of_shifting(self):
        text = ("0x20037000: 4C504143 0000A12F 00000010 0000035C\r\n"
                "0x20037020: 00001C50 7FCC0000 0000304C 00000038\r\n>")
        with self.assertRaises(ValueError):
            dump.parse_mem_read(text, 0x20037000, 8)

    def test_short_reply_raises(self):
        with self.assertRaises(ValueError):
            dump.parse_mem_read("0x20037000: 4C504143 0000A12F 00000010 0000035C\r\n>", 0x20037000, 8)

    def test_port_shell_format_reassembles_bytes(self):
        # The 2C23T port's `mem <addr> <len>`: 32 bytes per line, no 0x prefix.
        text = ("mem 20037000 64\r\n"
                "20037000: 43 41 50 4c 2f a1 00 00 10 00 00 00 5c 03 00 00 50 1c 00 00 00 00 cc 7f 4c 30 00 00 38 00 00 00\r\n"
                "20037020: " + " ".join(["00"] * 32) + "\r\n>")
        data = dump.parse_port_mem(text, 0x20037000, 64)
        self.assertEqual(data[:4], b"CAPL")
        self.assertEqual(len(data), 64)
        with self.assertRaises(ValueError):
            dump.parse_port_mem(text.replace("20037020", "20037040"), 0x20037000, 64)


class StockImage(unittest.TestCase):
    def setUp(self):
        if not build.DEFAULT_STOCK.exists():
            self.skipTest("stock archive image is not present: %s" % build.DEFAULT_STOCK)
        self.stock = build.DEFAULT_STOCK.read_bytes()

    def test_hook_sites_hold_the_replayed_bytes(self):
        build.validate_stock(self.stock)  # sha256, length, six bytes at each hook

    def test_cave_builds_fits_and_patches_twelve_bytes(self):
        if not toolchain_present():
            self.skipTest("arm-none-eabi-gcc is not on PATH")
        with tempfile.TemporaryDirectory() as tmp:
            cave, syms = build.build_cave(build.find_tools(None), Path(tmp))
        self.assertLessEqual(len(cave), build.CAVE_MAX)
        self.assertEqual(syms["caplog_stub_tx"], build.CAVE_ADDR, "stubs must lead the cave")
        self.assertIn("caplog_stub_tick", syms)
        img = build.patch_image(self.stock, cave, syms)
        self.assertEqual(len(img), build.CAVE_ADDR - build.APP_BASE + len(cave))
        changed = sum(1 for a, b in zip(self.stock, img) if a != b)
        self.assertEqual(changed, 12)
        for addr, _, stub in build.HOOKS:
            off = addr - build.APP_BASE
            self.assertEqual(build.dec_bw(addr, img[off:off + 4]), syms[stub])
            self.assertEqual(img[off + 4:off + 6], b"\x00\xbf")
        self.assertEqual(img[len(self.stock):build.CAVE_ADDR - build.APP_BASE],
                         b"\xff" * (build.CAVE_ADDR - build.APP_BASE - len(self.stock)))


class SampleDumps(unittest.TestCase):
    """Unit #2, stock V1.2.0 with the hooks, 2026-09-07. These three blocks are
    the measurement behind the word table in fpga_meter_plan.c."""

    def decode(self, name: str) -> dict:
        d = dump.decode((SAMPLES / name).read_bytes())
        self.assertTrue(d["magic_ok"])
        return d

    def assert_frontend_untouched(self, d: dict):
        moved = set(d["toggles"])
        self.assertFalse(moved & FRONTEND_PINS, "frontend pins toggled: %s" % (moved & FRONTEND_PINS))
        self.assertTrue(moved <= KEY_MATRIX_PINS | {"B12"}, "unexpected pins: %s" % moved)

    def test_run2_is_the_menu_walked_twice(self):
        d = self.decode("run2-menu-walk.caplog")
        self.assertEqual(d["tx_count"], 16)
        self.assertEqual([w for _, _, w in d["tx"]], MENU_WALK * 2)
        for _, frame, _ in d["tx"]:
            self.assertEqual(frame[:2], b"\xaa\x55")
            self.assertEqual(frame[9], (frame[2] + frame[3]) & 0xFF)
            self.assertEqual(frame[4:9], b"\x00" * 5)
        self.assert_frontend_untouched(d)

    def test_run3_adds_the_ac_dc_toggle_words(self):
        d = self.decode("run3-ac-dc-toggles.caplog")
        words = [w for _, _, w in d["tx"]]
        self.assertEqual(d["tx_count"], 24)
        # DC Voltage -> AC Voltage, Continuity -> Diode, small DC -> small AC, large DC -> large AC
        for dc, ac in [(0x050C, 0x050D), (0x0517, 0x050E), (0x0511, 0x0516), (0x0510, 0x0515)]:
            i = words.index(dc)
            self.assertEqual(words[i + 1], ac, "toggle after %04X" % dc)
        self.assertEqual(set(words), {0x050C, 0x050D, 0x0517, 0x050E, 0x050B, 0x050A, 0x0512,
                                      0x0511, 0x0516, 0x0510, 0x0515, 0x0514})
        self.assertNotIn(0x050F, words)
        self.assert_frontend_untouched(d)
        self.assertEqual(d["toggles"]["B12"], 820)
        self.assertTrue(all(d["toggles"][p] == 63 for p in KEY_MATRIX_PINS))

    def test_run1_selects_capacitance_with_0x0a_and_resistance_with_0x0b(self):
        d = self.decode("run1-capacitance-walk.caplog")
        self.assertEqual([w for _, _, w in d["tx"]],
                         [0x050C, 0x0517, 0x050B, 0x050A, 0x050B, 0x050A])
        self.assert_frontend_untouched(d)

    def test_word_map_matches_the_firmware_table(self):
        """The twelve stock words, one per function, are exactly the set the
        firmware sends -- fpga_meter_plan.c's per-submode table plus Auto."""
        d = self.decode("run3-ac-dc-toggles.caplog")
        plan = (REPO / "firmware/src/drivers/fpga_meter_plan.c").read_text()
        import re
        m = re.search(r"stock_meter_word_low_for_submode\[[^\]]*\]\s*=\s*\{(.*?)\};", plan, re.S)
        if m is None:
            self.skipTest("fpga_meter_plan.c does not carry the per-submode word table yet (PR #33)")
        table = {int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))}
        measured = {w & 0xFF for _, _, w in d["tx"]} - {0x14}  # Auto has no local submode
        self.assertEqual(table, measured)


if __name__ == "__main__":
    unittest.main()
