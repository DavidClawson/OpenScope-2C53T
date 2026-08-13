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

    def test_bootloader_nacks_misaligned_erase_addresses(self) -> None:
        source = (Path(__file__).resolve().parents[1] / "firmware/bootloader/src/hid_iap_user.c").read_text()
        validator = source.split("iap_result_type iap_erase_sector(uint32_t address)", 1)[0]
        self.assertIn("static uint8_t iap_write_address_valid(uint32_t address)", validator)
        self.assertIn("address & (HID_IAP_BUFFER_LEN - 1)", validator)
        self.assertIn("static uint8_t iap_erase_required(uint32_t address)", validator)
        self.assertIn("return (address & (SECTOR_SIZE_2K - 1)) == 0", validator)

        addr_handler = source.split("iap_result_type iap_address(uint8_t *pdata, uint32_t len)\n{", 1)[1]
        addr_handler = addr_handler.split("iap_result_type iap_data_write", 1)[0]
        self.assertIn("same_erased_sector", addr_handler)
        self.assertIn("iap_write_address_valid(address) && (erase_needed || same_erased_sector)", addr_handler)
        self.assertIn("!erase_needed || iap_erase_sector(address) == IAP_SUCCESS", addr_handler)
        self.assertIn("result = IAP_NACK;", addr_handler)
        self.assertLess(addr_handler.index("iap_erase_sector(address)"), addr_handler.index("iap_clear_upgrade_flag()"))
        self.assertLess(addr_handler.index("iap_clear_upgrade_flag()"), addr_handler.index("iap_info.state = IAP_STS_ADDR"))

    def test_high_layout_stage0_uses_hid_upgrade_flag(self) -> None:
        stage0 = (Path(__file__).resolve().parents[1] / "firmware/stage0/src/main.c").read_text()
        dispatcher = (Path(__file__).resolve().parents[1] / "firmware/stock_dispatcher/src/main.c").read_text()

        self.assertIn("IAP_UPGRADE_COMPLETE_FLAG     0x41544B38u", stage0)
        self.assertIn("IAP_UPGRADE_COMPLETE_FLAG     0x41544B38u", dispatcher)
        self.assertNotIn("0x41544F4Bu", stage0)

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
        self.assertIn("+ uv run", text)
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
        """Budget arithmetic, pinned with synthetic images of realistic size.

        This deliberately does NOT read firmware/build/firmware.bin. That path
        holds whatever target was built last: an `emu` image (~226 KB) DOES fit
        alongside stock, an app image (~538 KB) does not. Asserting a fixed
        verdict against an ambient artifact makes the result depend on build
        order rather than on the code under test -- the same failure family
        run_tests.py exists to surface, one layer down.
        """
        with tempfile.TemporaryDirectory() as td:
            stock = Path(td) / "stock.bin"
            openscope = Path(td) / "firmware.bin"
            stock.write_bytes(
                image(0x20036F90, 0x08007311, b"stock", size=751232))
            openscope.write_bytes(
                image(0x20037FE0, 0x08014C61, b"OpenScope 2C53T", size=538068))

            out = io.StringIO()
            args = type("Args", (), {"stock": stock, "openscope": openscope})()
            with contextlib.redirect_stdout(out):
                rc = switch_firmware.cmd_inspect(args)

            self.assertEqual(rc, 0)
            self.assertIn("stock + openscope fits: 0", out.getvalue())

    def test_inspect_budget_matches_real_image_sizes(self) -> None:
        """Same tool, real artifacts -- but assert self-consistency rather than
        a fixed verdict, so this keeps exercising the real binaries without
        silently depending on which make target ran last."""
        stock = switch_firmware.DEFAULT_STOCK
        openscope = switch_firmware.DEFAULT_OPENSCOPE
        if not stock.exists() or not openscope.exists():
            self.skipTest("stock archive or OpenScope build image is not present")

        out = io.StringIO()
        args = type("Args", (), {"stock": stock, "openscope": openscope})()
        with contextlib.redirect_stdout(out):
            rc = switch_firmware.cmd_inspect(args)
        text = out.getvalue()

        self.assertEqual(rc, 0)
        total = stock.stat().st_size + openscope.stat().st_size
        expected_fits = 1 if total <= switch_firmware.INTERNAL_FLASH_BYTES else 0
        self.assertIn(f"stock + openscope: {total} bytes", text)
        self.assertIn(f"stock + openscope fits: {expected_fits}", text)

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
        self.assertIn("+ uv run", text)
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
