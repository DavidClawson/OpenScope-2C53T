#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import argparse
import importlib.util
import io
import struct
import sys
import tempfile
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parent / "flash_preflight.py"
SPEC = importlib.util.spec_from_file_location("flash_preflight", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
flash_preflight = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(flash_preflight)

sys.modules.setdefault("hid", types.SimpleNamespace(device=lambda: None))
HID_FLASH_PATH = Path(__file__).resolve().parent / "hid_flash.py"
HID_SPEC = importlib.util.spec_from_file_location("hid_flash", HID_FLASH_PATH)
assert HID_SPEC is not None and HID_SPEC.loader is not None
hid_flash = importlib.util.module_from_spec(HID_SPEC)
HID_SPEC.loader.exec_module(hid_flash)


def image(sp: int = 0x20037FE0, rv: int = 0x08004040, marker: bytes = b"OpenScope") -> bytes:
    return struct.pack("<II", sp, rv) + marker + b"\x00" * 64


class FlashPreflightTests(unittest.TestCase):
    def test_classifies_openscope_app(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            data = image()
            path.write_bytes(data)

            kind = flash_preflight.classify_image(path, data, flash_preflight.APP_ADDRESS)

        self.assertEqual(kind, "openscope-app")

    def test_classifies_stock_filename_as_stock_app(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "APP_2C53T_V1.2.0.bin"
            data = image(marker=b"")
            path.write_bytes(data)

            kind = flash_preflight.classify_image(path, data, flash_preflight.APP_ADDRESS)

        self.assertEqual(kind, "stock-app")

    def test_app_vector_below_slot_is_rejected(self) -> None:
        _sp, _rv, errors = flash_preflight.validate_vectors(
            image(rv=0x08000100),
            flash_preflight.APP_ADDRESS,
            app_slot=True,
        )

        self.assertTrue(any("below app slot" in err for err in errors))

    def test_hid_preflight_rejects_stock_app_even_image_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "APP_2C53T_V1.2.0.bin"
            path.write_bytes(image(marker=b""))
            args = argparse.Namespace(
                image=path,
                address=flash_preflight.APP_ADDRESS,
                allow_missing_device=False,
                image_only=True,
                allow_unknown_app=False,
            )

            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                rc = flash_preflight.preflight_hid_app(args)

        self.assertEqual(rc, 2)
        self.assertIn("stock-app", stdout.getvalue())
        self.assertIn("Do not flash stock/vendor APP_2C53T", stderr.getvalue())

    def test_hid_preflight_accepts_openscope_app_image_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            path.write_bytes(image())
            args = argparse.Namespace(
                image=path,
                address=flash_preflight.APP_ADDRESS,
                allow_missing_device=False,
                image_only=True,
                allow_unknown_app=False,
            )

            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                rc = flash_preflight.preflight_hid_app(args)

        self.assertEqual(rc, 0)
        self.assertIn("openscope-app", stdout.getvalue())
        self.assertIn("Preflight: OK", stdout.getvalue())
        self.assertEqual(stderr.getvalue(), "")

    def test_hid_preflight_rejects_openscope_image_that_programs_stock_settings_hole(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            data = bytearray(b"\xFF" * 0x5000)
            data[0:8] = struct.pack("<II", 0x20037FE0, 0x08007199)
            data[0x2000] = 0x00
            data[0x3000:0x3011] = b"OpenScope 2C53T"
            path.write_bytes(bytes(data))
            args = argparse.Namespace(
                image=path,
                address=flash_preflight.APP_ADDRESS,
                allow_missing_device=False,
                image_only=True,
                allow_unknown_app=False,
            )

            stderr = io.StringIO()
            with redirect_stdout(io.StringIO()), redirect_stderr(stderr):
                rc = flash_preflight.preflight_hid_app(args)

        self.assertEqual(rc, 2)
        self.assertIn("stock saved-settings preserve page", stderr.getvalue())

    def test_hid_preflight_accepts_blank_stock_settings_hole_and_prints_preserve_command(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "firmware.bin"
            data = bytearray(b"\xFF" * 0x5000)
            data[0:8] = struct.pack("<II", 0x20037FE0, 0x08007199)
            data[0x3000:0x3011] = b"OpenScope 2C53T"
            path.write_bytes(bytes(data))
            args = argparse.Namespace(
                image=path,
                address=flash_preflight.APP_ADDRESS,
                allow_missing_device=False,
                image_only=True,
                allow_unknown_app=False,
            )

            stdout = io.StringIO()
            with redirect_stdout(stdout):
                rc = flash_preflight.preflight_hid_app(args)

        self.assertEqual(rc, 0)
        self.assertIn("--preserve-blank-blocks-range 0x08006000:0x08007000", stdout.getvalue())

    def test_hid_flash_repeats_stock_image_guard(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            with self.assertRaisesRegex(RuntimeError, "refusing stock/vendor APP_2C53T"):
                hid_flash.validate_hid_app_image(
                    "APP_2C53T_V1.2.0.bin",
                    image(marker=b""),
                    flash_preflight.APP_ADDRESS,
                )
        self.assertIn("kind: stock-app", stdout.getvalue())

    def test_hid_flash_rejects_non_app_slot_address_before_usb(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            with self.assertRaisesRegex(RuntimeError, "only allowed at 0x08004000"):
                hid_flash.validate_hid_app_image(
                    "firmware.bin",
                    image(),
                    flash_preflight.APP_ADDRESS + 0x400,
                )
        self.assertIn("address: 0x08004400", stdout.getvalue())

    def test_hid_flash_rejects_openscope_image_that_programs_stock_settings_hole(self) -> None:
        firmware = bytearray(b"\xFF" * 0x5000)
        firmware[0:8] = struct.pack("<II", 0x20037FE0, 0x08007199)
        firmware[0x2000] = 0x00
        firmware[0x3000:0x3011] = b"OpenScope 2C53T"

        with self.assertRaisesRegex(RuntimeError, "stock saved-settings preserve page"):
            hid_flash.validate_hid_app_image(
                "firmware.bin",
                bytes(firmware),
                flash_preflight.APP_ADDRESS,
            )

    def test_hid_flash_allows_low_flash_only_with_explicit_flag(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            hid_flash.validate_hid_app_image(
                "stock_user_image.bin",
                image(rv=0x080E07B1, marker=b"launcher"),
                flash_preflight.FLASH_BASE,
                allow_unknown_app=True,
                allow_low_flash=True,
            )
        self.assertIn("address: 0x08000000", stdout.getvalue())

    def test_hid_flash_still_rejects_stock_filename_with_low_flash_flag(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "refusing stock/vendor APP_2C53T"):
            hid_flash.validate_hid_app_image(
                "APP_2C53T_V1.2.0.bin",
                image(rv=0x08007311, marker=b""),
                flash_preflight.FLASH_BASE,
                allow_unknown_app=True,
                allow_low_flash=True,
            )

    def test_hid_flash_rejects_app_overlap_with_high_recovery(self) -> None:
        oversized = image() + b"\xFF" * (flash_preflight.APP_SLOT_END_ADDRESS - flash_preflight.APP_ADDRESS)
        with self.assertRaisesRegex(RuntimeError, "high recovery bootloader region"):
            hid_flash.validate_hid_app_image(
                "firmware.bin",
                oversized,
                flash_preflight.APP_ADDRESS,
            )

    def test_hid_flash_crc_matches_bootloader_word_algorithm(self) -> None:
        self.assertEqual(hid_flash.at32_crc32_words(b"\x00" * 4), 0xC704DD7B)
        self.assertEqual(hid_flash.at32_crc32_words(b"\xFF" * 4), 0x00000000)
        self.assertEqual(hid_flash.at32_crc32_words(bytes(range(16))), 0xA97AFF4D)
        self.assertEqual(hid_flash.at32_crc32_words(b"12345678"), 0x49E3C2FB)

    def test_hid_flash_crc_verify_sends_full_block_count(self) -> None:
        firmware = bytes(range(256)) * 8
        expected_crc = hid_flash.at32_crc32_words(firmware)

        class FakeDevice:
            def __init__(self) -> None:
                self.writes: list[bytes] = []

            def write(self, data: bytes) -> int:
                self.writes.append(data)
                return len(data)

            def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                return struct.pack(">HHI", hid_flash.CMD_CRC, hid_flash.ACK, expected_crc).ljust(length, b"\x00")

        dev = FakeDevice()
        hid_flash.verify_flash_crc(dev, flash_preflight.APP_ADDRESS, firmware)

        self.assertEqual(len(dev.writes), 1)
        report = dev.writes[0]
        self.assertEqual(report[1:3], struct.pack(">H", hid_flash.CMD_CRC))
        self.assertEqual(report[3:7], struct.pack(">I", flash_preflight.APP_ADDRESS))
        self.assertEqual(report[7:9], struct.pack(">H", 2))

    def test_hid_flash_low_flash_sends_unlock_and_direct_run(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "stock_user_image.bin"
            path.write_bytes(image(rv=0x080E07B1, marker=b"launcher"))

            class FakeDevice:
                def __init__(self) -> None:
                    self.writes: list[bytes] = []
                    self.read_count = 0

                def write(self, data: bytes) -> int:
                    self.writes.append(data)
                    return len(data)

                def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                    self.read_count += 1
                    last = self.writes[-1]
                    cmd = struct.unpack(">H", last[1:3])[0]
                    if cmd == hid_flash.CMD_CRC:
                        firmware = path.read_bytes().ljust(hid_flash.BLOCK_SIZE, b"\xFF")
                        crc = hid_flash.at32_crc32_words(firmware)
                        return struct.pack(">HHI", cmd, hid_flash.ACK, crc).ljust(length, b"\x00")
                    return struct.pack(">HH", cmd, hid_flash.ACK).ljust(length, b"\x00")

                def get_manufacturer_string(self) -> str:
                    return "test"

                def get_product_string(self) -> str:
                    return "HID IAP"

                def close(self) -> None:
                    pass

            dev = FakeDevice()
            with mock.patch.object(hid_flash, "open_bootloader_device", return_value=dev), redirect_stdout(io.StringIO()):
                hid_flash.flash_firmware(
                    path,
                    app_address=flash_preflight.FLASH_BASE,
                    allow_unknown_app=True,
                    allow_low_flash=True,
                    run_address=flash_preflight.FLASH_BASE,
                )

        commands = [struct.unpack(">H", write[1:3])[0] for write in dev.writes]
        self.assertIn(hid_flash.CMD_LOW_FLASH, commands)
        self.assertIn(hid_flash.CMD_RUN_ADDR, commands)
        unlock = dev.writes[commands.index(hid_flash.CMD_LOW_FLASH)]
        self.assertEqual(unlock[3:7], struct.pack(">I", hid_flash.LOW_FLASH_MAGIC))
        run = dev.writes[commands.index(hid_flash.CMD_RUN_ADDR)]
        self.assertEqual(run[3:7], struct.pack(">I", flash_preflight.FLASH_BASE))

    def test_hid_flash_preserves_only_blank_blocks_at_or_above_floor(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "stock_user_image.bin"
            block0 = image(rv=0x080E07B1, marker=b"launcher").ljust(hid_flash.BLOCK_SIZE, b"\x00")
            block1 = b"\xFF" * hid_flash.BLOCK_SIZE
            block2 = b"\xFF" * hid_flash.BLOCK_SIZE
            block3 = b"\xFF" * hid_flash.BLOCK_SIZE
            path.write_bytes(block0 + block1 + block2 + block3)

            class FakeDevice:
                def __init__(self) -> None:
                    self.writes: list[bytes] = []

                def write(self, data: bytes) -> int:
                    self.writes.append(data)
                    return len(data)

                def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                    last = self.writes[-1]
                    cmd = struct.unpack(">H", last[1:3])[0]
                    if cmd == hid_flash.CMD_CRC:
                        address, block_count = struct.unpack(">IH", last[3:9])
                        if address == flash_preflight.FLASH_BASE:
                            if block_count != 2:
                                raise AssertionError(f"unexpected block count {block_count}")
                            crc = hid_flash.at32_crc32_words(block0 + block1)
                        else:
                            raise AssertionError(f"unexpected CRC address 0x{address:08X}")
                        return struct.pack(">HHI", cmd, hid_flash.ACK, crc).ljust(length, b"\x00")
                    return struct.pack(">HH", cmd, hid_flash.ACK).ljust(length, b"\x00")

                def get_manufacturer_string(self) -> str:
                    return "test"

                def get_product_string(self) -> str:
                    return "HID IAP"

                def close(self) -> None:
                    pass

            dev = FakeDevice()
            with mock.patch.object(hid_flash, "open_bootloader_device", return_value=dev), redirect_stdout(io.StringIO()):
                hid_flash.flash_firmware(
                    path,
                    app_address=flash_preflight.FLASH_BASE,
                    allow_unknown_app=True,
                    allow_low_flash=True,
                    run_address=flash_preflight.FLASH_BASE,
                    preserve_blank_blocks=True,
                    preserve_blank_blocks_from=flash_preflight.FLASH_BASE + 2 * hid_flash.BLOCK_SIZE,
                )

        addr_writes = [
            struct.unpack(">I", write[3:7])[0]
            for write in dev.writes
            if struct.unpack(">H", write[1:3])[0] == hid_flash.CMD_ADDR
        ]
        self.assertIn(flash_preflight.FLASH_BASE + hid_flash.BLOCK_SIZE, addr_writes)
        self.assertNotIn(flash_preflight.FLASH_BASE + 2 * hid_flash.BLOCK_SIZE, addr_writes)

    def test_hid_flash_preserves_named_blank_ranges_below_floor(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "stock_user_image.bin"
            block0 = image(rv=0x080E07B1, marker=b"launcher").ljust(hid_flash.BLOCK_SIZE, b"\x00")
            block1 = b"launcher".ljust(hid_flash.BLOCK_SIZE, b"\x33")
            block2 = b"\xFF" * hid_flash.BLOCK_SIZE
            block3 = b"\xFF" * hid_flash.BLOCK_SIZE
            path.write_bytes(block0 + block1 + block2 + block3)

            class FakeDevice:
                def __init__(self) -> None:
                    self.writes: list[bytes] = []

                def write(self, data: bytes) -> int:
                    self.writes.append(data)
                    return len(data)

                def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                    last = self.writes[-1]
                    cmd = struct.unpack(">H", last[1:3])[0]
                    if cmd == hid_flash.CMD_CRC:
                        address, block_count = struct.unpack(">IH", last[3:9])
                        if address == flash_preflight.FLASH_BASE:
                            if block_count != 2:
                                raise AssertionError(f"unexpected block count {block_count}")
                            crc = hid_flash.at32_crc32_words(block0 + block1)
                        else:
                            raise AssertionError(f"unexpected CRC address 0x{address:08X}")
                        return struct.pack(">HHI", cmd, hid_flash.ACK, crc).ljust(length, b"\x00")
                    return struct.pack(">HH", cmd, hid_flash.ACK).ljust(length, b"\x00")

                def get_manufacturer_string(self) -> str:
                    return "test"

                def get_product_string(self) -> str:
                    return "HID IAP"

                def close(self) -> None:
                    pass

            dev = FakeDevice()
            with mock.patch.object(hid_flash, "open_bootloader_device", return_value=dev), redirect_stdout(io.StringIO()):
                hid_flash.flash_firmware(
                    path,
                    app_address=flash_preflight.FLASH_BASE,
                    allow_unknown_app=True,
                    allow_low_flash=True,
                    run_address=flash_preflight.FLASH_BASE,
                    preserve_blank_blocks=True,
                    preserve_blank_blocks_from=flash_preflight.FLASH_BASE + 10 * hid_flash.BLOCK_SIZE,
                    preserve_blank_block_ranges=[
                        (
                            flash_preflight.FLASH_BASE + 2 * hid_flash.BLOCK_SIZE,
                            flash_preflight.FLASH_BASE + 4 * hid_flash.BLOCK_SIZE,
                        )
                    ],
                )

        addr_writes = [
            struct.unpack(">I", write[3:7])[0]
            for write in dev.writes
            if struct.unpack(">H", write[1:3])[0] == hid_flash.CMD_ADDR
        ]
        self.assertIn(flash_preflight.FLASH_BASE + hid_flash.BLOCK_SIZE, addr_writes)
        self.assertNotIn(flash_preflight.FLASH_BASE + 2 * hid_flash.BLOCK_SIZE, addr_writes)

    def test_hid_flash_rejects_partial_sector_preserve_selection(self) -> None:
        firmware = (
            image(rv=0x080E07B1, marker=b"launcher").ljust(hid_flash.BLOCK_SIZE, b"\x00")
            + b"\xFF" * hid_flash.BLOCK_SIZE
        )
        with self.assertRaisesRegex(RuntimeError, "part of erase sector"):
            hid_flash.validate_preserved_sectors(
                firmware,
                flash_preflight.FLASH_BASE,
                preserve_from=None,
                ranges=[
                    (
                        flash_preflight.FLASH_BASE + hid_flash.BLOCK_SIZE,
                        flash_preflight.FLASH_BASE + hid_flash.SECTOR_SIZE,
                    )
                ],
            )

    def test_hid_memory_read_chunks_by_report_payload(self) -> None:
        class FakeDevice:
            def __init__(self) -> None:
                self.writes: list[bytes] = []

            def write(self, data: bytes) -> int:
                self.writes.append(data)
                return len(data)

            def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                last = self.writes[-1]
                cmd = struct.unpack(">H", last[1:3])[0]
                if cmd == hid_flash.CMD_IDLE:
                    return struct.pack(">HH", cmd, hid_flash.ACK).ljust(length, b"\x00")
                address, requested = struct.unpack(">IH", last[3:9])
                payload = bytes((address + i) & 0xFF for i in range(requested))
                return struct.pack(">HHB", cmd, hid_flash.ACK, requested).ljust(5, b"\x00") + payload + b"\x00" * (length - 5 - requested)

        dev = FakeDevice()
        data = hid_flash.read_memory(dev, 0x20037FA0, hid_flash.READ_MEM_CHUNK_SIZE + 3)

        self.assertEqual(len(data), hid_flash.READ_MEM_CHUNK_SIZE + 3)
        commands = [struct.unpack(">H", write[1:3])[0] for write in dev.writes]
        self.assertEqual(commands, [hid_flash.CMD_IDLE, hid_flash.CMD_READ_MEM, hid_flash.CMD_READ_MEM])
        first_address, first_len = struct.unpack(">IH", dev.writes[1][3:9])
        second_address, second_len = struct.unpack(">IH", dev.writes[2][3:9])
        self.assertEqual((first_address, first_len), (0x20037FA0, hid_flash.READ_MEM_CHUNK_SIZE))
        self.assertEqual((second_address, second_len), (0x20037FA0 + hid_flash.READ_MEM_CHUNK_SIZE, 3))

    def test_hid_verify_written_ranges_groups_contiguous_blocks(self) -> None:
        first = b"\x11" * hid_flash.BLOCK_SIZE
        second = b"\x22" * hid_flash.BLOCK_SIZE
        third = b"\x33" * hid_flash.BLOCK_SIZE

        class FakeDevice:
            def __init__(self) -> None:
                self.writes: list[bytes] = []

            def write(self, data: bytes) -> int:
                self.writes.append(data)
                return len(data)

            def read(self, length: int, timeout_ms: int = 5000) -> bytes:
                last = self.writes[-1]
                cmd = struct.unpack(">H", last[1:3])[0]
                address, block_count = struct.unpack(">IH", last[3:9])
                if address == 0x0800B000:
                    if block_count != 2:
                        raise AssertionError(f"unexpected block count {block_count}")
                    expected_crc = hid_flash.at32_crc32_words(first + second)
                elif address == 0x0800C000:
                    if block_count != 1:
                        raise AssertionError(f"unexpected block count {block_count}")
                    expected_crc = hid_flash.at32_crc32_words(third)
                else:
                    raise AssertionError(f"unexpected CRC address 0x{address:08X}")
                return struct.pack(">HHI", cmd, hid_flash.ACK, expected_crc).ljust(length, b"\x00")

        dev = FakeDevice()
        hid_flash.verify_written_ranges(
            dev,
            [
                (0x0800B000, first),
                (0x0800B400, second),
                (0x0800C000, third),
            ],
        )

        commands = [struct.unpack(">H", write[1:3])[0] for write in dev.writes]
        self.assertEqual(commands, [hid_flash.CMD_CRC, hid_flash.CMD_CRC])


if __name__ == "__main__":
    unittest.main()
