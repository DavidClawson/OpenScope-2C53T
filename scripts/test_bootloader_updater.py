#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parent / "flash_preflight.py"
SPEC = importlib.util.spec_from_file_location("flash_preflight", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
flash_preflight = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(flash_preflight)


def image(sp: int, rv: int, size: int, marker: bytes = b"") -> bytes:
    data = sp.to_bytes(4, "little") + rv.to_bytes(4, "little")
    data += b"\xFF" * max(0, size - len(data) - len(marker))
    data += marker
    return data


def high_bootloader_marker() -> bytes:
    return (
        b"HID IAP"
        + flash_preflight.HIGH_BOOTLOADER_READ_MEM_ACK_PATTERN
        + flash_preflight.HIGH_BOOTLOADER_READ_MEM_SRAM_END
    )


class BootloaderUpdaterPreflightTests(unittest.TestCase):
    def test_updater_binaries_gap_fill_stock_settings_page(self) -> None:
        root = Path(__file__).resolve().parents[1]
        bootloader_makefile = (root / "firmware/bootloader_updater/Makefile").read_text()
        high_makefile = (root / "firmware/high_layout_updater/Makefile").read_text()
        self.assertIn("--gap-fill 0xFF -R .data -R .spim", bootloader_makefile)
        self.assertIn("--gap-fill 0xFF -R .data -R .spim", high_makefile)

        start = flash_preflight.STOCK_SAVED_CONFIG_ADDRESS - flash_preflight.APP_ADDRESS
        end = flash_preflight.STOCK_SAVED_CONFIG_END_ADDRESS - flash_preflight.APP_ADDRESS
        for image_path in (
            root / "firmware/bootloader_updater/build/bootloader_updater.bin",
            root / "firmware/high_layout_updater/build/high_layout_updater.bin",
        ):
            if not image_path.exists():
                self.skipTest(f"{image_path} has not been built")
            data = image_path.read_bytes()
            self.assertGreaterEqual(len(data), end)
            self.assertLess(len(data), 128 * 1024)
            self.assertEqual(data[start:end], b"\xFF" * (end - start))

    def test_stage0_directory_is_tracked_for_high_layout_builds(self) -> None:
        root = Path(__file__).resolve().parents[1]
        self.assertTrue((root / "firmware/stage0/Makefile").exists())
        self.assertTrue((root / "firmware/stage0/src/main.c").exists())
        self.assertTrue((root / "firmware/stage0/stage0.ld").exists())

    def test_hid_bootloader_update_entrypoint_is_present(self) -> None:
        root = Path(__file__).resolve().parents[1]
        script = root / "scripts/update_bootloader_via_hid.py"
        self.assertTrue(script.exists())
        self.assertIn("UPDATE BOOTLOADER", script.read_text())

    def test_valid_bootloader_payload(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "bootloader.bin"
            path.write_bytes(image(0x20037FE0, 0x080026C9, 12 * 1024, b"HID IAP"))
            errors = flash_preflight.validate_bootloader_payload(path.read_bytes())
        self.assertEqual(errors, [])

    def test_rejects_app_slot_payload_as_bootloader(self) -> None:
        errors = flash_preflight.validate_bootloader_payload(
            image(0x20037FE0, flash_preflight.APP_ADDRESS + 0x100, 4096)
        )
        self.assertTrue(any("not inside bootloader region" in err for err in errors))

    def test_updater_image_marker(self) -> None:
        updater = image(
            0x20037FE0,
            flash_preflight.APP_ADDRESS + 0x100,
            4096,
            flash_preflight.BOOTLOADER_UPDATER_MARKER,
        )
        errors = flash_preflight.validate_bootloader_updater_image(updater)
        self.assertEqual(errors, [])

    def test_updater_image_requires_marker(self) -> None:
        updater = image(0x20037FE0, flash_preflight.APP_ADDRESS + 0x100, 4096)
        errors = flash_preflight.validate_bootloader_updater_image(updater)
        self.assertTrue(any("marker" in err for err in errors))

    def test_valid_stage0_payload(self) -> None:
        errors = flash_preflight.validate_stage0_payload(
            image(0x20037FE0, flash_preflight.FLASH_BASE + 0x101, 1024)
        )
        self.assertEqual(errors, [])

    def test_valid_stock_switcher_low_vector_payload(self) -> None:
        errors = flash_preflight.validate_stage0_payload(
            image(0x20036F90, flash_preflight.HIGH_DISPATCHER_ADDRESS + 0x415, 2048)
        )
        self.assertEqual(errors, [])

    def test_stage0_payload_must_fit_one_sector(self) -> None:
        errors = flash_preflight.validate_stage0_payload(
            image(0x20037FE0, flash_preflight.FLASH_BASE + 0x101, 4096)
        )
        self.assertTrue(any("2KB" in err for err in errors))

    def test_stage0_payload_rejects_high_recovery_vector(self) -> None:
        errors = flash_preflight.validate_stage0_payload(
            image(0x20037FE0, flash_preflight.HIGH_BOOTLOADER_ADDRESS + 0x101, 1024)
        )
        self.assertTrue(any("stock-switcher high dispatcher" in err for err in errors))

    def test_valid_high_bootloader_payload(self) -> None:
        errors = flash_preflight.validate_high_bootloader_payload(
            image(0x20037FE0, flash_preflight.HIGH_BOOTLOADER_ADDRESS + 0x269, 12 * 1024, high_bootloader_marker())
        )
        self.assertEqual(errors, [])

    def test_high_bootloader_payload_must_have_high_reset_vector(self) -> None:
        errors = flash_preflight.validate_high_bootloader_payload(
            image(0x20037FE0, 0x080026C9, 12 * 1024, b"HID IAP")
        )
        self.assertTrue(any("high recovery flash" in err for err in errors))

    def test_high_bootloader_payload_requires_readback_command(self) -> None:
        errors = flash_preflight.validate_high_bootloader_payload(
            image(0x20037FE0, flash_preflight.HIGH_BOOTLOADER_ADDRESS + 0x269, 12 * 1024, b"HID IAP")
        )
        self.assertTrue(any("memory readback command" in err for err in errors))


if __name__ == "__main__":
    unittest.main()
