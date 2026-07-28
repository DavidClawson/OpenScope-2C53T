#!/usr/bin/env python3
from __future__ import annotations

import contextlib
import io
import struct
import tempfile
import unittest
from pathlib import Path

import flash_preflight
import switch_firmware


def image(sp: int, rv: int, payload: bytes, size: int = 256) -> bytes:
    data = struct.pack("<II", sp, rv) + payload
    return data.ljust(size, b"\xFF")


class FlashSafetyTests(unittest.TestCase):
    def test_dfu_make_targets_preserve_stock_settings_page(self) -> None:
        makefile = (Path(__file__).resolve().parents[1] / "firmware/Makefile").read_text()
        self.assertIn("at32_dfuse_write.py 0x08004000 $< --preserve-blank-pages-range 0x08006000:0x08007000", makefile)
        self.assertIn(
            "at32_dfuse_write.py 0x08004000 $(BUILD_DIR)/$(TARGET).bin --preserve-blank-pages-range 0x08006000:0x08007000",
            makefile,
        )

    def test_preserve_ranges_are_sector_aligned_before_hid_flash(self) -> None:
        hid_flash = (Path(__file__).resolve().parents[1] / "scripts/hid_flash.py").read_text()
        self.assertIn("SECTOR_SIZE = 2048", hid_flash)
        self.assertIn("validate_preserve_ranges", hid_flash)
        self.assertIn("not {SECTOR_SIZE}-byte sector-aligned", hid_flash)

    def test_stock_named_image_is_classified_and_hid_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "APP_2C53T_V1.2.0.bin"
            data = image(0x20036F90, 0x08007311, b"vendor")
            path.write_bytes(data)

            self.assertEqual(flash_preflight.classify_image(path, data, flash_preflight.APP_ADDRESS), "stock-app")
            args = type("Args", (), {
                "image": path,
                "address": flash_preflight.APP_ADDRESS,
                "allow_unknown_app": False,
                "allow_missing_device": False,
                "image_only": True,
            })()
            out = io.StringIO()
            err = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                rc = flash_preflight.preflight_hid_app(args)
            self.assertEqual(rc, 2)
            self.assertIn("stock-app", out.getvalue())
            self.assertIn("Do not flash stock/vendor APP_2C53T", err.getvalue())

    def test_openscope_named_image_passes_image_only_preflight(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "firmware.bin"
            data = image(0x20037FE0, 0x08020199, b"OpenScope 2C53T")
            path.write_bytes(data)

            args = type("Args", (), {
                "image": path,
                "address": flash_preflight.APP_ADDRESS,
                "allow_unknown_app": False,
                "allow_missing_device": False,
                "image_only": True,
            })()
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = flash_preflight.preflight_hid_app(args)
            self.assertEqual(rc, 0)
            self.assertIn("Preflight: OK", out.getvalue())

    def test_stock_tail_pairs_are_classified_as_false_scatter_candidates(self) -> None:
        path = switch_firmware.DEFAULT_STOCK
        if not path.exists():
            self.skipTest("stock archive image is not present")
        data = path.read_bytes()
        entries = flash_preflight.stock_false_scatter_candidates(data)

        self.assertEqual(len(entries), 2)
        self.assertEqual(entries[0], (0x080BE480, 0x20000000, 0x1240, 0x080071C0))
        self.assertEqual(entries[1], (0x080BE680, 0x20001240, 0x35D50, 0x0800721C))
        self.assertEqual(flash_preflight.validate_stock_scatter_sources(data), [])

    def test_stock_command_dry_run_builds_user_image(self) -> None:
        path = switch_firmware.DEFAULT_STOCK
        if not path.exists():
            self.skipTest("stock archive image is not present")
        out = io.StringIO()
        args = type("Args", (), {
            "image": path,
            "out": switch_firmware.DEFAULT_STOCK_USER,
            "flash": False,
            "no_jump": False,
            "keep_splash": False,
            "dry_run": True,
        })()
        with contextlib.redirect_stdout(out):
            rc = switch_firmware.cmd_stock(args)
        text = out.getvalue()
        self.assertEqual(rc, 0)
        self.assertIn("boot-chain warning", text)
        self.assertIn("+ make -C firmware/stock_dispatcher", text)
        self.assertIn(str(switch_firmware.STOCK_BUILDER), text)
        self.assertIn(str(switch_firmware.DEFAULT_STOCK_LAUNCHER), text)

    def test_stock_flash_dry_run_uses_low_flash_and_run0(self) -> None:
        path = switch_firmware.DEFAULT_STOCK
        if not path.exists():
            self.skipTest("stock archive image is not present")
        out = io.StringIO()
        args = type("Args", (), {
            "image": path,
            "out": switch_firmware.DEFAULT_STOCK_USER,
            "flash": True,
            "no_jump": False,
            "keep_splash": False,
            "dry_run": True,
        })()
        with contextlib.redirect_stdout(out):
            rc = switch_firmware.cmd_stock(args)
        text = out.getvalue()
        self.assertEqual(rc, 0)
        self.assertIn(str(switch_firmware.HID_FLASH), text)
        self.assertIn(str(switch_firmware.DEFAULT_STOCK_USER), text)
        self.assertIn("--address 0x08000000", text)
        self.assertIn("--allow-low-flash", text)
        self.assertIn("--allow-unknown-app", text)
        self.assertIn("--preserve-blank-blocks", text)
        self.assertIn("--preserve-blank-blocks-range 0x08006000:0x08007000", text)
        self.assertIn("--preserve-blank-blocks-from 0x080BE800", text)
        self.assertIn("--run-address 0x08000000", text)

    def test_stock_keep_splash_dry_run_passes_builder_flag(self) -> None:
        path = switch_firmware.DEFAULT_STOCK
        if not path.exists():
            self.skipTest("stock archive image is not present")
        out = io.StringIO()
        args = type("Args", (), {
            "image": path,
            "out": switch_firmware.DEFAULT_STOCK_USER,
            "flash": False,
            "no_jump": False,
            "keep_splash": True,
            "dry_run": True,
        })()
        with contextlib.redirect_stdout(out):
            rc = switch_firmware.cmd_stock(args)
        text = out.getvalue()
        self.assertEqual(rc, 0)
        self.assertIn(str(switch_firmware.STOCK_BUILDER), text)
        self.assertIn("--keep-splash", text)

    def test_inspect_reports_stock_and_openscope_do_not_fit_together(self) -> None:
        stock = switch_firmware.DEFAULT_STOCK
        openscope = switch_firmware.DEFAULT_OPENSCOPE
        if not stock.exists() or not openscope.exists():
            self.skipTest("stock archive or OpenScope build image is not present")

        out = io.StringIO()
        args = type("Args", (), {"stock": stock, "openscope": openscope})()
        with contextlib.redirect_stdout(out):
            rc = switch_firmware.cmd_inspect(args)

        self.assertEqual(rc, 0)
        self.assertIn("stock + openscope fits: 0", out.getvalue())

    def test_openscope_build_flag_runs_make_before_preflight(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "firmware.bin"
            path.write_bytes(image(0x20037FE0, 0x08020199, b"OpenScope 2C53T"))

            out = io.StringIO()
            args = type("Args", (), {
                "image": path,
                "build": True,
                "image_only": True,
                "preflight_only": True,
                "dry_run": True,
                "no_jump": False,
            })()
            with contextlib.redirect_stdout(out):
                rc = switch_firmware.cmd_openscope(args)

        self.assertEqual(rc, 0)
        self.assertIn("+ make -C firmware", out.getvalue())

    def test_openscope_flash_preserves_stock_settings_hole(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "firmware.bin"
            payload = bytearray(b"\xFF" * 0x5000)
            payload[0:8] = struct.pack("<II", 0x20037FE0, 0x08007199)
            payload[0x3000:0x3011] = b"OpenScope 2C53T"
            path.write_bytes(bytes(payload))

            out = io.StringIO()
            args = type("Args", (), {
                "image": path,
                "build": False,
                "image_only": False,
                "preflight_only": False,
                "dry_run": True,
                "no_jump": False,
            })()
            with contextlib.redirect_stdout(out):
                rc = switch_firmware.cmd_openscope(args)

        text = out.getvalue()
        self.assertEqual(rc, 0)
        self.assertIn("--preserve-blank-blocks", text)
        self.assertIn("--preserve-blank-blocks-range 0x08006000:0x08007000", text)

    def test_openscope_dry_run_rejects_image_that_programs_stock_settings_hole(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "firmware.bin"
            payload = bytearray(b"\xFF" * 0x5000)
            payload[0:8] = struct.pack("<II", 0x20037FE0, 0x08007199)
            payload[0x2000] = 0x00
            payload[0x3000:0x3011] = b"OpenScope 2C53T"
            path.write_bytes(bytes(payload))

            out = io.StringIO()
            args = type("Args", (), {
                "image": path,
                "build": False,
                "image_only": False,
                "preflight_only": False,
                "dry_run": True,
                "no_jump": False,
            })()
            with contextlib.redirect_stdout(out):
                rc = switch_firmware.cmd_openscope(args)

        self.assertEqual(rc, 2)
        self.assertIn("stock saved-settings preserve page", out.getvalue())


if __name__ == "__main__":
    unittest.main()
