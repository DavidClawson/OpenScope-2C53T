#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import argparse
import hashlib
import importlib.util
import io
import plistlib
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

IAP_FLASH_PATH = Path(__file__).resolve().parent / "iap_flash.py"
IAP_SPEC = importlib.util.spec_from_file_location("iap_flash", IAP_FLASH_PATH)
assert IAP_SPEC is not None and IAP_SPEC.loader is not None
iap_flash = importlib.util.module_from_spec(IAP_SPEC)
IAP_SPEC.loader.exec_module(iap_flash)


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


IAP_SECT = 2048


def fat12_volume(label: bytes = b"IAP        ", sectors: int = 32) -> bytes:
    """A FAT12 volume shaped like the 2C53T IAP drive: 2048-byte sectors,
    1 reserved + 2 FATs of 1 sector, root dir at sector 3 holding the label."""
    vol = bytearray(IAP_SECT * sectors)
    vol[0x0B:0x0D] = IAP_SECT.to_bytes(2, "little")
    vol[0x0D] = 1                                  # sectors per cluster
    vol[0x0E:0x10] = (1).to_bytes(2, "little")     # reserved
    vol[0x10] = 2                                  # FATs
    vol[0x11:0x13] = (64).to_bytes(2, "little")    # root entries
    vol[0x16:0x18] = (1).to_bytes(2, "little")     # sectors per FAT
    vol[0x26] = 0x29
    vol[0x2B:0x36] = b"NO NAME    "
    vol[0x36:0x3E] = b"FAT12   "
    root = 3 * IAP_SECT
    vol[root:root + 11] = label
    vol[root + 11] = 0x08                          # volume-label entry
    vol[root + 32:root + 43] = b"READY   TXT"
    vol[root + 43] = 0x20
    return bytes(vol)


# BSD dd, measured on macOS: a failed write prints the summary, a failed open
# does not.
DD_VANISHED_MID_WRITE = ("dd: /dev/rdisk4: Device not configured\n"
                         "1+0 records in\n0+0 records out\n"
                         "0 bytes transferred in 0.01 secs (0 bytes/sec)\n")
DD_VANISHED_BEFORE_OPEN = "dd: /dev/rdisk4: No such file or directory\n"


class FakeMacDisk:
    """subprocess.run for _write_image_macos: the snapshot dd, mcopy into the
    snapshot copy, and the raw-device dd writes (recorded as (seek, count))."""

    def __init__(self, volume: bytes, fail_call=None, fail_stderr: str = "") -> None:
        self.volume = volume
        self.fail_call = fail_call      # index of the raw-device write that fails
        self.fail_stderr = fail_stderr
        self.writes: list[tuple[int, int]] = []

    def __call__(self, cmd, **kw):
        done = types.SimpleNamespace(returncode=0, stdout="", stderr="")
        if Path(cmd[0]).name == "mcopy":
            # the copy changes sectors 1-2 (FATs), 3 (dir) and 10-13 (data)
            target = Path(cmd[cmd.index("-i") + 1])
            img = bytearray(target.read_bytes())
            for sector in (1, 2, 3, 10, 11, 12, 13):
                img[sector * IAP_SECT + 40] ^= 0x5A
            target.write_bytes(bytes(img))
            return done
        if cmd[:2] == ["sudo", "dd"] and "bs=1m" in cmd:
            Path(cmd[3][len("of="):]).write_bytes(self.volume)
            return done
        if cmd[:2] == ["sudo", "dd"] and cmd[3] == "of=/dev/rdisk4":
            if len(self.writes) == self.fail_call:
                self.writes.append((-1, -1))
                return types.SimpleNamespace(returncode=1, stdout="", stderr=self.fail_stderr)
            args = dict(a.split("=", 1) for a in cmd[2:] if "=" in a)
            self.writes.append((int(args["seek"]), int(args["count"])))
            return done
        if cmd == ["sync"]:
            return done
        raise AssertionError(f"unexpected command {cmd}")


class IapFlashTests(unittest.TestCase):
    def write_macos(self, disk: FakeMacDisk):
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "APP_test.bin"
            image.write_bytes(b"\x00" * 64)
            with mock.patch.object(iap_flash.subprocess, "run", disk):
                return iap_flash._write_image_macos("/dev/disk4", image)

    def test_iap_fat_label_read_from_root_dir_entry(self) -> None:
        self.assertIn("IAP", iap_flash.fat_volume_labels(fat12_volume()))
        self.assertTrue(iap_flash.is_iap_volume(iap_flash.fat_volume_labels(fat12_volume())))
        self.assertFalse(iap_flash.is_iap_volume(
            iap_flash.fat_volume_labels(fat12_volume(b"KINGSTON   "))))

    def test_iap_mdir_label_plain_and_mtools_lfn_forms(self) -> None:
        self.assertEqual(iap_flash.mdir_volume_label(" Volume in drive : is IAP\n"), "IAP")
        self.assertEqual(iap_flash.mdir_volume_label(
            " Volume in drive : is IAP___ (abbr=IAP        )\n"), "IAP")
        self.assertEqual(iap_flash.mdir_volume_label(" Volume in drive : has no label\n"), "")

    def test_iap_macos_write_refuses_a_volume_not_labelled_iap(self) -> None:
        disk = FakeMacDisk(fat12_volume(b"KINGSTON   "))
        ok, msg = self.write_macos(disk)
        self.assertFalse(ok)
        self.assertIn("not IAP", msg)
        self.assertEqual(disk.writes, [])

    def test_iap_macos_clean_write_sends_every_changed_sector_last_alone(self) -> None:
        disk = FakeMacDisk(fat12_volume())
        ok, msg = self.write_macos(disk)
        self.assertTrue(ok, msg)
        self.assertEqual(disk.writes, [(1, 3), (10, 3), (13, 1)])

    def test_iap_macos_disk_vanishing_before_the_last_sector_is_not_success(self) -> None:
        disk = FakeMacDisk(fat12_volume(), fail_call=1, fail_stderr=DD_VANISHED_MID_WRITE)
        ok, msg = self.write_macos(disk)
        self.assertFalse(ok)
        self.assertIn("3 of 7 changed sectors", msg)

    def test_iap_macos_disk_vanishing_on_the_last_sector_is_the_flash(self) -> None:
        disk = FakeMacDisk(fat12_volume(), fail_call=2, fail_stderr=DD_VANISHED_MID_WRITE)
        ok, msg = self.write_macos(disk)
        self.assertTrue(ok, msg)
        self.assertIn("all 7 changed sectors", msg)

    def test_iap_macos_disk_gone_before_the_last_sector_opened_is_not_success(self) -> None:
        disk = FakeMacDisk(fat12_volume(), fail_call=2, fail_stderr=DD_VANISHED_BEFORE_OPEN)
        ok, msg = self.write_macos(disk)
        self.assertFalse(ok)
        self.assertIn("6 of 7 changed sectors", msg)

    def test_iap_dd_failure_classifier(self) -> None:
        classify = iap_flash.classify_dd_failure
        self.assertEqual(classify(DD_VANISHED_MID_WRITE, last_call=True), "flashed")
        self.assertEqual(classify(DD_VANISHED_MID_WRITE, last_call=False), "incomplete")
        self.assertEqual(classify(DD_VANISHED_BEFORE_OPEN, last_call=True), "incomplete")
        self.assertEqual(classify("dd: /dev/rdisk4: Permission denied\n", last_call=True), "error")

    def test_iap_split_last_sector(self) -> None:
        self.assertEqual(iap_flash.split_last_sector([(1, 4), (10, 14)]),
                         [(1, 4), (10, 13), (13, 14)])
        self.assertEqual(iap_flash.split_last_sector([(1, 4), (9, 10)]), [(1, 4), (9, 10)])

    def test_iap_linux_write_refuses_a_volume_not_labelled_iap(self) -> None:
        calls = []

        def mtools(args, dev):
            calls.append(args[0])
            return types.SimpleNamespace(returncode=0, stderr="",
                                         stdout=" Volume in drive : has no label\n")

        with mock.patch.object(iap_flash, "_mtools_with_fallback", mtools):
            ok, msg = iap_flash._write_image_linux("/dev/sdc", Path("APP_test.bin"))
        self.assertFalse(ok)
        self.assertIn("not IAP", msg)
        self.assertEqual(calls, ["mdir"])

    def test_iap_slot_capacity_is_740_kb_and_stock_fits(self) -> None:
        self.assertEqual(iap_flash.APP_SLOT_MAX, 757760)
        self.assertEqual(iap_flash.image_size_error(751232), "")      # stock V1.2.0
        self.assertEqual(iap_flash.image_size_error(757760), "")
        self.assertIn("holds 757760", iap_flash.image_size_error(757761))

    def flash_plan(self, size: int):
        prompts = []

        def answer_no(prompt=""):
            prompts.append(prompt)
            return "n"

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "APP_test.bin"
            data = (bytes(range(256)) * (size // 256 + 1))[:size]
            path.write_bytes(data)
            out = io.StringIO()
            with mock.patch("builtins.input", answer_no), redirect_stdout(out):
                ok = iap_flash.flash_image("/dev/disk4",
                                           {"path": path, "size": size, "label": "test"})
        return ok, out.getvalue(), prompts, hashlib.sha256(data).hexdigest()

    def test_iap_flash_plan_refuses_an_image_over_the_slot_before_asking(self) -> None:
        ok, out, prompts, _sha = self.flash_plan(757760 + 2)
        self.assertFalse(ok)
        self.assertIn("Refusing", out)
        self.assertEqual(prompts, [])

    def test_iap_flash_plan_prints_size_and_full_sha256(self) -> None:
        ok, out, prompts, sha = self.flash_plan(4096)
        self.assertFalse(ok)                       # answered "n"
        self.assertIn(sha, out)
        self.assertIn("(4096 bytes", out)
        self.assertEqual(len(prompts), 1)

    def test_iap_macos_detection_wants_the_label_not_a_small_usb_disk(self) -> None:
        def diskutil(disks):
            def run(cmd, **kw):
                blob = {"AllDisks": list(disks)} if cmd[:2] == ["diskutil", "list"] else disks[cmd[-1]]
                return types.SimpleNamespace(stdout=plistlib.dumps(blob).decode(), returncode=0)
            return run

        stick = {"VolumeName": "", "BusProtocol": "USB", "TotalSize": 8_400_000,
                 "MediaName": "Generic Flash Disk"}
        hinted = {"VolumeName": "", "BusProtocol": "USB", "TotalSize": 8_400_000,
                  "MediaName": "AT32 MSC"}
        labelled = {"VolumeName": "IAP", "BusProtocol": "USB", "TotalSize": 8_400_000,
                    "MediaName": "Generic"}
        with mock.patch.object(iap_flash, "_run", diskutil({"disk4": stick})):
            self.assertIsNone(iap_flash.find_iap_disk_macos())
        with mock.patch.object(iap_flash, "_run", diskutil({"disk3": hinted, "disk4": labelled})):
            self.assertEqual(iap_flash.find_iap_disk_macos(), "/dev/disk4")

    def test_iap_status_refuses_upgrade_mode_without_a_labelled_disk(self) -> None:
        with redirect_stdout(io.StringIO()):
            self.assertFalse(iap_flash.print_device_status({"mode": "iap", "dev": None}))
            self.assertTrue(iap_flash.print_device_status({"mode": "iap", "dev": "/dev/disk4"}))


if __name__ == "__main__":
    unittest.main()
