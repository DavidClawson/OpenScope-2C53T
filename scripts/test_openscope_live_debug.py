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
    TRACE_TEXT = """=== DMM Trace ===
trace v=1 snapshot=1
context mode=1 startup=Meter ui_sub=0 reading_sub=0 live=1 valid=1 updates=42
producer counts tx=33 rx_bytes=512 data=12 echo=33 rx_valid=1
producer_last_rx data=12 tx=33 echo=33 seq=2 seq_sub=0 busy=0 discard=0
rx_sync data_start=12 echo_start=33 data_hdr=12 echo_hdr=33 bad_second=0 stray=0
plan stock_mode=0 raw_low=14 family=0 mux=0 portc_porte=0 porta_portb=0 settle_ms=20 discard=2
wire selector=0514 apply=0000 has_apply=0 probe=0507 start=0509 seq_count=2 seq_sub=0
last_sequence selector=0514 apply=0000 probe=0507 start=0509
decoded display=0.4366 unit=V value_i10000=4366 raw=4366 dp=1 class=1 reject=0 family=0/0 extra=0189
stock_fsm mode=0 variant=0 format=0 dc_state=1 display_cmd=0 unit_index=0 composite=0
transition busy=0 discard_now=0 skip_count=2
producer_frame=5A A5 44 8E EF E7 07 24 80 00 01 89
parsed_frame=5A A5 44 8E EF E7 07 24 80 00 01 89
last_echo_frame=AA 55 00 09 00 00 00 AA 00 09
transition_history newest_first:
mth n=0 sub=0 seq=2 selector=0514 apply=0000 probe=0507 start=0509 tx=29..33 data=8..9 planned_gpio=0BB actual_gpio=0BB
producer_history newest_first:
rxh n=0 data=12 tx=33 echo=33 seq=2 seq_sub=0 busy=0 discard=0 frame=5A A5 44 8E EF E7 07 24 80 00 01 89
tx_history newest_first:
txh n=0 frame=00 00 05 09 00 00 00 00 00 0E
txh n=1 frame=00 00 05 07 00 00 00 00 00 0C
gpio control PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
gpio_frontend PC12=1 PE4=1 PE5=0 PE6=1 PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
h2 bytes=115638 done=1 post_enq=5 post_ok=5 post_drop=0 post_mask=1F spi_ok=0 spi_to=0
h2_post_rx n=0 trigger=01 len=2 bytes=FF FF
"""

    def test_parse_meter_trace_text_extracts_machine_readable_dcv_record(self) -> None:
        parsed = openscope_live_debug.parse_meter_trace_text(self.TRACE_TEXT)

        self.assertEqual(parsed["raw_measurement_source"], "USART2_DMM_12_BYTE_PRODUCER_FRAME")
        self.assertEqual(parsed["trace_version"], 1)
        self.assertEqual(parsed["context"]["ui_sub"], 0)
        self.assertEqual(parsed["plan"]["raw_low"], 14)
        self.assertEqual(parsed["wire"]["selector"], "0514")
        self.assertEqual(parsed["wire"]["apply"], "0000")
        self.assertEqual(parsed["decoded"]["display"], "0.4366")
        self.assertEqual(parsed["decoded"]["raw"], 4366)
        self.assertEqual(parsed["decoded"]["family_expected"], 0)
        self.assertEqual(parsed["decoded"]["family_observed"], 0)
        self.assertEqual(parsed["rx_sync"]["echo_hdr"], 33)
        self.assertEqual(parsed["rx_sync"]["bad_second"], 0)
        self.assertEqual(
            parsed["producer_frame"]["hex"],
            "5A A5 44 8E EF E7 07 24 80 00 01 89",
        )
        self.assertEqual(
            parsed["last_echo_frame"]["hex"],
            "AA 55 00 09 00 00 00 AA 00 09",
        )
        self.assertEqual(
            parsed["tx_history"][0]["frame"]["hex"],
            "00 00 05 09 00 00 00 00 00 0E",
        )
        self.assertEqual(
            parsed["tx_history"][1]["frame"]["hex"],
            "00 00 05 07 00 00 00 00 00 0C",
        )
        self.assertEqual(parsed["gpio_frontend"]["PC12"], 1)
        self.assertEqual(parsed["gpio_frontend"]["PB9"], 0)
        self.assertEqual(parsed["calibration_state"]["bytes"], 115638)
        self.assertEqual(parsed["h2_post_rx"][0]["bytes"], [0xFF, 0xFF])

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

    def test_meter_trace_json_mode_emits_parsed_trace(self) -> None:
        original_run_command = openscope_live_debug.run_command
        try:
            def fake_run_command(port: str, baud: int, command: str, timeout: float) -> str:
                self.assertEqual((port, baud, command, timeout),
                                 ("/dev/fake", 115200, "meter trace", 7.0))
                return self.TRACE_TEXT

            openscope_live_debug.run_command = fake_run_command
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = openscope_live_debug.main([
                    "meter-trace",
                    "--json",
                    "--port",
                    "/dev/fake",
                    "--timeout",
                    "7",
                ])
        finally:
            openscope_live_debug.run_command = original_run_command

        self.assertEqual(rc, 0)
        self.assertIn('"raw_measurement_source": "USART2_DMM_12_BYTE_PRODUCER_FRAME"', out.getvalue())
        self.assertIn('"selector": "0514"', out.getvalue())


if __name__ == "__main__":
    unittest.main()
