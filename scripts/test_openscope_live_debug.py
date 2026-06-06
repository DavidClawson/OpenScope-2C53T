#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import contextlib
import io
import importlib.util
import tempfile
import unittest
import zlib


MODULE_PATH = Path(__file__).resolve().parent / "openscope_live_debug.py"
SPEC = importlib.util.spec_from_file_location("openscope_live_debug", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
openscope_live_debug = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(openscope_live_debug)


class FakeSerial:
    header: bytes = b""
    payload: bytes = b""
    trailer: bytes = b"\r\nSCREENBIN END\r\n> "

    def __init__(self, port: str, baud: int, read_timeout: float) -> None:
        self.written: list[str] = []

    def __enter__(self) -> "FakeSerial":
        return self

    def __exit__(self, *_exc: object) -> None:
        return None

    def drain(self) -> bytes:
        return b""

    def write_line(self, text: str) -> None:
        self.written.append(text)

    def read_line(self, timeout: float) -> bytes:
        return self.header

    def read_exact(self, length: int, timeout: float) -> bytes:
        if length != len(self.payload):
            raise TimeoutError(f"requested {length}, fake has {len(self.payload)}")
        return self.payload

    def read_until_prompt(self, timeout: float) -> bytes:
        return self.trailer


class ScreenDumpBinTests(unittest.TestCase):
    def setUp(self) -> None:
        self.original_serial = openscope_live_debug.PosixSerial
        openscope_live_debug.PosixSerial = FakeSerial

    def tearDown(self) -> None:
        openscope_live_debug.PosixSerial = self.original_serial

    def test_dumpbin_reads_exact_payload_with_prompt_bytes_inside(self) -> None:
        payload = bytes([0x01, 0x3E, 0x23, 0x45])
        crc = zlib.crc32(payload) & 0xFFFFFFFF
        FakeSerial.payload = payload
        FakeSerial.header = (
            f"SCREENBIN x=7 y=8 w=3 h=2 format=indexed4 len={len(payload)} "
            f"crc32={crc:08X}\r\n"
        ).encode("ascii")

        x, y, w, h, dump_format, rows = openscope_live_debug.run_screen_dumpbin(
            "/dev/fake", 115200, "screen dumpbin 7 8 3 2", 1.0
        )

        self.assertEqual((x, y, w, h, dump_format), (7, 8, 3, 2, "indexed4"))
        self.assertEqual(len(rows), 2)
        self.assertEqual([row[:3] for row in rows], [
            [
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x0],
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x1],
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x3],
            ],
            [
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x2],
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x3],
                openscope_live_debug.SHADOW_PALETTE_RGB565[0x4],
            ],
        ])

    def test_dumpbin_crc_mismatch_fails_loudly(self) -> None:
        FakeSerial.payload = b"\x12\x34"
        FakeSerial.header = b"SCREENBIN x=0 y=0 w=4 h=1 format=indexed4 len=2 crc32=00000000\r\n"

        with self.assertRaisesRegex(ValueError, "CRC mismatch"):
            openscope_live_debug.run_screen_dumpbin(
                "/dev/fake", 115200, "screen dumpbin 0 0 4 1", 1.0
            )

    def test_capture_screen_does_not_text_fallback_on_crc_mismatch(self) -> None:
        FakeSerial.payload = b"\x12\x34"
        FakeSerial.header = b"SCREENBIN x=0 y=0 w=4 h=1 format=indexed4 len=2 crc32=00000000\r\n"

        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "CRC mismatch"):
                openscope_live_debug.capture_screen(
                    "/dev/fake",
                    115200,
                    1.0,
                    Path(tmp) / "screen.bmp",
                    (0, 0, 4, 1),
                    False,
                )

    def test_rle_shadow_capture_is_disabled_because_it_mutates_state(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "mutates LCD shadow state"):
                openscope_live_debug.capture_screen(
                    "/dev/fake",
                    115200,
                    1.0,
                    Path(tmp) / "screen.bmp",
                    None,
                    True,
                )


class MeterTraceCliTests(unittest.TestCase):
    def test_meter_trace_mode_sends_read_only_trace_command(self) -> None:
        calls: list[tuple[str, int, str, float]] = []
        original_run_command = openscope_live_debug.run_command
        try:
            def fake_run_command(port: str, baud: int, command: str, timeout: float) -> str:
                calls.append((port, baud, command, timeout))
                return "=== DMM Trace ===\ntrace v=1 snapshot=1"

            openscope_live_debug.run_command = fake_run_command
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = openscope_live_debug.main([
                    "meter-trace",
                    "--port",
                    "/dev/fake",
                    "--timeout",
                    "7",
                ])
        finally:
            openscope_live_debug.run_command = original_run_command

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [("/dev/fake", 115200, "meter trace", 7.0)])
        self.assertIn("trace v=1 snapshot=1", out.getvalue())


if __name__ == "__main__":
    unittest.main()
