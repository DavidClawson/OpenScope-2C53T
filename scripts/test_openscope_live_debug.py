#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import contextlib
import io
import importlib.util
from unittest import mock
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


class DiscoveryTests(unittest.TestCase):
    @mock.patch.object(openscope_live_debug.glob, "glob")
    @mock.patch.object(openscope_live_debug, "linux_tty_vid_pid")
    def test_discover_ports_rejects_non_openscope_tty_acm(self, mock_vid_pid, mock_glob) -> None:
        def fake_glob(pattern: str) -> list[str]:
            return ["/dev/ttyACM0"] if pattern == "/dev/ttyACM*" else []

        mock_glob.side_effect = fake_glob
        mock_vid_pid.return_value = ("239a", "8029")

        self.assertEqual(openscope_live_debug.discover_ports(), [])
        mock_vid_pid.assert_called_with("/dev/ttyACM0")

    @mock.patch.object(openscope_live_debug.glob, "glob")
    @mock.patch.object(openscope_live_debug, "linux_tty_vid_pid")
    def test_discover_ports_accepts_openscope_cdc_vid_pid(self, mock_vid_pid, mock_glob) -> None:
        def fake_glob(pattern: str) -> list[str]:
            return ["/dev/ttyACM1"] if pattern == "/dev/ttyACM*" else []

        mock_glob.side_effect = fake_glob
        mock_vid_pid.return_value = ("2e3c", "5740")

        self.assertEqual(openscope_live_debug.discover_ports(), ["/dev/ttyACM1"])

    @mock.patch.object(openscope_live_debug.glob, "glob")
    @mock.patch.object(openscope_live_debug, "linux_tty_vid_pid")
    def test_choose_port_fails_closed_when_only_other_cdc_device_exists(self, mock_vid_pid, mock_glob) -> None:
        def fake_glob(pattern: str) -> list[str]:
            return ["/dev/ttyACM0"] if pattern == "/dev/ttyACM*" else []

        mock_glob.side_effect = fake_glob
        mock_vid_pid.return_value = ("239a", "8029")

        with self.assertRaisesRegex(openscope_live_debug.SerialError,
                                    "no candidate USB CDC serial ports found"):
            openscope_live_debug.choose_port(None)


def make_trace_text(submode: int, *, reject: int = 0, valid: int = 1,
                    display: str = "ERR", value_i10000: int = 0,
                    family: int = 0, selector: str = "0514",
                    apply: str = "0000") -> str:
    return f"""=== DMM Trace ===
trace v=1 snapshot=1
context mode=1 startup=Meter ui_sub={submode} reading_sub={submode} live={valid} valid={valid} updates=42
producer counts tx=33 rx_bytes=512 data=12 echo=0 rx_valid=1
producer_last_rx data=12 tx=33 echo=0 seq=2 seq_sub={submode} busy=0 discard=0
rx_sync data_start=12 echo_start=0 data_hdr=12 echo_hdr=0 bad_second=0 stray=0
plan stock_mode={submode} raw_low=14 family={family} mux=0 portc_porte=0 porta_portb=0 settle_ms=20 discard=2
wire config=0000 has_config=0 selector={selector} apply={apply} has_apply=0 probe=0507 start=0509 seq_count=2 seq_sub={submode}
last_sequence config=0000 selector={selector} apply={apply} probe=0507 start=0509
decoded display={display} unit=V value_i10000={value_i10000} raw=0 dp=0 class=1 reject={reject} family={family}/{family} extra=014B
stock_fsm mode={submode} variant=0 format=0 dc_state=0 display_cmd=0 unit_index=0 composite=0
transition busy=0 discard_now=0 skip_count=0
producer_frame=5A A5 04 E0 9B EF 07 28 00 00 01 4B
    parsed_frame=5A A5 04 E0 9B EF 07 28 00 00 01 4B
    first_transition_rx valid=1 armed=0 sub={submode} seq=2 config=0000 selector={selector} apply={apply} probe=0507 start=0509 planned_gpio=0BB actual_gpio=0BB data=9 tx=33 echo=0 busy=0 discard=2 h2_bytes=115638 h2_done=1 h2_post_ok=5 h2_post_mask=1F frame=5A A5 04 E0 9B EF 07 28 00 00 01 4B
    last_echo_frame=00 00 00 00 00 00 00 00 00 00
    rx_raw newest_first:
    rxraw n=0 tx=33 txi=10 rxi=11 byte=4B
    rxraw n=1 tx=33 txi=10 rxi=10 byte=01
    transition_history newest_first:
mth n=0 sub={submode} seq=2 config=0000 selector={selector} apply={apply} probe=0507 start=0509 tx=29..33 data=8..9 planned_gpio=0BB actual_gpio=0BB
producer_history newest_first:
rxh n=0 data=12 tx=33 echo=0 seq=2 seq_sub={submode} busy=0 discard=0 frame=5A A5 04 E0 9B EF 07 28 00 00 01 4B
tx_history newest_first:
txh n=0 tx=33 frame=00 00 05 09 00 00 00 00 00 0E
tx_control_history newest_first:
txc n=0 tx=32 frame=00 00 05 07 00 00 00 00 00 0C
gpio control PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
gpio_frontend PC12=1 PE4=1 PE5=0 PE6=1 PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
h2 bytes=115638 done=1 post_enq=5 post_ok=5 post_drop=0 post_mask=1F spi_ok=0 spi_to=0 rx00=0 rxff=115638 rxother=0 close_len=6
factory_cal loaded=0 ch_size=301 channels=2
"""


def make_dump_text(submode: int, *, valid: int = 1, result_class: int = 1,
                   display: str = "ERR", unit: str = "V", beep: int = 0,
                   reject: int = 0, family: int = 0) -> str:
    return f"""=== DMM State ===
mode=1 startup=Meter meter_submode={submode} layout=0
valid={valid} reading_submode={submode} class={result_class} updates=42 display={display} unit={unit}
bcd_value=0 decimal_pos=0 negative=0 unit_variant=0 bar_i100=0 aux_freq_i10=0
flags ac=0 auto=0 hold=0 probe=0 range_ind=0 range_cmd=0 beep={beep}
stock_fsm mode={submode} variant=0 format=0 dc_state=0 display_cmd=0 unit_index=0 composite=0
frame_family expected={family} observed={family} reject={reject}
frame=5A A5 04 E0 9B EF 07 28 00 00 01 4B
nibbles=0E 0B 0E 07 raw_digits=00 00 00 00
"""


class SequenceSerial:
    responses: list[str] = []
    instances: list["SequenceSerial"] = []

    def __init__(self, port: str, baud: int, read_timeout: float) -> None:
        self.written: list[str] = []
        self._responses = list(self.responses)
        self.instances.append(self)

    def __enter__(self) -> "SequenceSerial":
        return self

    def __exit__(self, *_exc: object) -> None:
        return None

    def drain(self, *_args: object, **_kwargs: object) -> bytes:
        return b""

    def write_line(self, text: str) -> None:
        self.written.append(text)

    def read_until_prompt(self, timeout: float) -> bytes:
        if not self._responses:
            raise TimeoutError("fake response queue exhausted")
        return (self._responses.pop(0) + "\r\n> ").encode("ascii")


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
    TRACE_TEXT = """=== DMM Mux Arms Trace ===
mux_arms portc_porte=5 porta_portb=0 settle_ms=300 planned_gpio=1BB actual_gpio=1BB
=== DMM Trace ===
trace v=1 snapshot=1
context mode=1 startup=Meter ui_sub=0 reading_sub=0 live=1 valid=1 updates=42
producer counts tx=33 rx_bytes=512 data=12 echo=33 rx_valid=1
producer_last_rx data=12 tx=33 echo=33 seq=2 seq_sub=0 busy=0 discard=0
rx_sync data_start=12 echo_start=33 data_hdr=12 echo_hdr=33 bad_second=0 stray=0
plan stock_mode=0 raw_low=14 family=0 mux=0 portc_porte=0 porta_portb=0 settle_ms=20 discard=2
wire config=0508 has_config=1 selector=0514 apply=0000 has_apply=0 probe=0507 start=0509 seq_count=2 seq_sub=0
last_sequence config=0508 selector=0514 apply=0000 probe=0507 start=0509
decoded display=0.4366 unit=V value_i10000=4366 raw=4366 dp=1 class=1 reject=0 family=0/0 extra=0189
stock_fsm mode=0 variant=0 format=0 dc_state=1 display_cmd=0 unit_index=0 composite=0
transition busy=0 discard_now=0 skip_count=2
producer_frame=5A A5 44 8E EF E7 07 24 80 00 01 89
parsed_frame=5A A5 44 8E EF E7 07 24 80 00 01 89
first_transition_rx valid=1 armed=0 sub=0 seq=2 config=0508 selector=0514 apply=0000 probe=0507 start=0509 planned_gpio=0BB actual_gpio=0BB data=9 tx=33 echo=33 busy=1 discard=2 h2_bytes=115638 h2_done=1 h2_post_ok=5 h2_post_mask=1F frame=5A A5 44 8E EF E7 07 24 80 00 01 89
last_echo_frame=AA 55 00 09 00 00 00 AA 00 09
rx_raw newest_first:
rxraw n=0 tx=33 txi=10 rxi=11 byte=89
rxraw n=1 tx=33 txi=10 rxi=10 byte=01
rxraw n=2 tx=33 txi=10 rxi=0 byte=5A
transition_history newest_first:
mth n=0 sub=0 seq=2 config=0508 selector=0514 apply=0000 probe=0507 start=0509 tx=29..33 data=8..9 planned_gpio=0BB actual_gpio=0BB
producer_history newest_first:
rxh n=0 data=12 tx=33 echo=33 seq=2 seq_sub=0 busy=0 discard=0 frame=5A A5 44 8E EF E7 07 24 80 00 01 89
tx_history newest_first:
txh n=0 tx=33 frame=00 00 05 09 00 00 00 00 00 0E
txh n=1 tx=32 frame=00 00 05 07 00 00 00 00 00 0C
tx_control_history newest_first:
txc n=0 tx=32 frame=00 00 05 07 00 00 00 00 00 0C
txc n=1 tx=31 frame=00 00 05 14 00 00 00 00 00 19
gpio control PC6=1 PB11=1 PC11=1 PC7=1 PC0=1
gpio_frontend PC12=1 PE4=1 PE5=0 PE6=1 PA15=1 PA10=1 PB10=0 PB9=0 PA6=0
h2 bytes=115638 done=1 post_enq=5 post_ok=5 post_drop=0 post_mask=1F spi_ok=0 spi_to=0 rx00=0 rxff=115638 rxother=0 close_len=6
factory_cal loaded=0 ch_size=301 channels=2
h2_close_rx bytes=FF FF FF FF FF FF
h2_post_rx n=0 trigger=01 len=2 bytes=FF FF
"""

    def test_parse_meter_trace_text_extracts_machine_readable_dcv_record(self) -> None:
        parsed = openscope_live_debug.parse_meter_trace_text(self.TRACE_TEXT)

        self.assertEqual(parsed["raw_measurement_source"], "USART2_DMM_12_BYTE_PRODUCER_FRAME")
        self.assertEqual(parsed["mux_arms"]["portc_porte"], 5)
        self.assertEqual(parsed["mux_arms"]["porta_portb"], 0)
        self.assertEqual(parsed["mux_arms"]["planned_gpio"], "1BB")
        self.assertEqual(parsed["mux_arms"]["actual_gpio"], "1BB")
        self.assertEqual(parsed["trace_version"], 1)
        self.assertEqual(parsed["context"]["ui_sub"], 0)
        self.assertEqual(parsed["plan"]["raw_low"], 14)
        self.assertEqual(parsed["wire"]["config"], "0508")
        self.assertEqual(parsed["wire"]["has_config"], 1)
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
        self.assertEqual(parsed["rx_raw"][0]["byte"], "89")
        self.assertEqual(parsed["rx_raw"][2]["rxi"], 0)
        self.assertEqual(parsed["first_transition_rx"]["valid"], 1)
        self.assertEqual(parsed["first_transition_rx"]["config"], "0508")
        self.assertEqual(parsed["first_transition_rx"]["selector"], "0514")
        self.assertEqual(parsed["first_transition_rx"]["apply"], "0000")
        self.assertEqual(parsed["first_transition_rx"]["probe"], "0507")
        self.assertEqual(parsed["first_transition_rx"]["planned_gpio"], "0BB")
        self.assertEqual(parsed["first_transition_rx"]["actual_gpio"], "0BB")
        self.assertEqual(parsed["first_transition_rx"]["h2_bytes"], 115638)
        self.assertEqual(parsed["first_transition_rx"]["h2_post_mask"], "1F")
        self.assertEqual(
            parsed["first_transition_rx"]["frame"]["hex"],
            "5A A5 44 8E EF E7 07 24 80 00 01 89",
        )
        self.assertEqual(
            parsed["tx_history"][0]["frame"]["hex"],
            "00 00 05 09 00 00 00 00 00 0E",
        )
        self.assertEqual(parsed["tx_history"][0]["tx"], 33)
        self.assertEqual(
            parsed["tx_history"][1]["frame"]["hex"],
            "00 00 05 07 00 00 00 00 00 0C",
        )
        self.assertEqual(parsed["tx_history"][1]["tx"], 32)
        self.assertEqual(
            parsed["tx_control_history"][0]["frame"]["hex"],
            "00 00 05 07 00 00 00 00 00 0C",
        )
        self.assertEqual(parsed["tx_control_history"][0]["tx"], 32)
        self.assertEqual(
            parsed["tx_control_history"][1]["frame"]["hex"],
            "00 00 05 14 00 00 00 00 00 19",
        )
        self.assertEqual(parsed["tx_control_history"][1]["tx"], 31)
        self.assertEqual(parsed["gpio_frontend"]["PC12"], 1)
        self.assertEqual(parsed["gpio_frontend"]["PB9"], 0)
        self.assertEqual(parsed["calibration_state"]["bytes"], 115638)
        self.assertEqual(parsed["calibration_state"]["rxff"], 115638)
        self.assertEqual(parsed["calibration_state"]["rxother"], 0)
        self.assertEqual(parsed["factory_cal"]["loaded"], 0)
        self.assertEqual(parsed["factory_cal"]["ch_size"], 301)
        self.assertEqual(parsed["factory_cal"]["channels"], 2)
        self.assertEqual(parsed["h2_close_rx"], [0xFF] * 6)
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

    def test_meter_mux_arms_mode_sends_state_changing_trace_command(self) -> None:
        calls: list[tuple[str, int, str, float]] = []
        original_run_command = openscope_live_debug.run_command
        try:
            def fake_run_command(port: str, baud: int, command: str, timeout: float) -> str:
                calls.append((port, baud, command, timeout))
                return self.TRACE_TEXT

            openscope_live_debug.run_command = fake_run_command
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = openscope_live_debug.main([
                    "meter-mux-arms",
                    "5",
                    "0",
                    "--settle-ms",
                    "250",
                    "--json",
                    "--port",
                    "/dev/fake",
                    "--timeout",
                    "7",
                ])
        finally:
            openscope_live_debug.run_command = original_run_command

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [("/dev/fake", 115200, "meter mux-arms 5 0 250", 7.0)])
        self.assertIn('"mux_arms"', out.getvalue())

    def test_meter_mux_arms_rejects_out_of_range_arm(self) -> None:
        original_run_command = openscope_live_debug.run_command
        try:
            def fake_run_command(*_args: object) -> str:
                self.fail("out-of-range mux arm should fail before serial command")

            openscope_live_debug.run_command = fake_run_command
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                rc = openscope_live_debug.main([
                    "meter-mux-arms",
                    "10",
                    "0",
                    "--port",
                    "/dev/fake",
                ])
        finally:
            openscope_live_debug.run_command = original_run_command

        self.assertEqual(rc, 4)
        self.assertIn("Port C/E mux arm", err.getvalue())

    def test_parse_meter_dump_text_extracts_beep_and_frame_family(self) -> None:
        parsed = openscope_live_debug.parse_meter_dump_text(
            make_dump_text(7, result_class=7, display="CONT", unit="Ohm",
                           beep=1, family=3)
        )

        self.assertEqual(parsed["context"]["meter_submode"], 7)
        self.assertEqual(parsed["reading"]["class"], 7)
        self.assertEqual(parsed["reading"]["unit"], "Ohm")
        self.assertEqual(parsed["flags"]["beep"], 1)
        self.assertEqual(parsed["frame_family"]["expected"], 3)
        self.assertEqual(parsed["frame"]["hex"], "5A A5 04 E0 9B EF 07 28 00 00 01 4B")

    def test_shorted_probes_sweep_uses_real_mode_trace_dump_sequence(self) -> None:
        original_serial = openscope_live_debug.PosixSerial
        original_sleep = openscope_live_debug.time.sleep
        try:
            responses: list[str] = []
            for submode in range(11):
                responses.append(f"mode=meter submode={submode}")
                if submode == 0:
                    responses.append(make_trace_text(0, display="0.0000", value_i10000=0, family=0))
                    responses.append(make_dump_text(0, valid=1, result_class=1, display="0.0000", unit="V", family=0))
                elif submode == 1:
                    responses.append(make_trace_text(1, reject=3, valid=0, display="---", family=0))
                    responses.append(make_dump_text(1, valid=0, result_class=0, display="---", unit="V", reject=3, family=0))
                elif submode in (2, 3):
                    responses.append(make_trace_text(submode, family=1))
                    responses.append(make_dump_text(submode, family=1))
                elif submode in (4, 5):
                    responses.append(make_trace_text(submode, reject=3, valid=0, display="---", family=1))
                    responses.append(make_dump_text(submode, valid=0, result_class=0, display="---", unit="A", reject=3, family=1))
                elif submode == 6:
                    responses.append(make_trace_text(6, reject=4, valid=0, display="---", family=2))
                    responses.append(make_dump_text(6, valid=0, result_class=0, display="---", unit="Ohm", reject=4, family=2))
                elif submode == 7:
                    responses.append(make_trace_text(7, display="CONT", family=3, selector="0511", apply="0516"))
                    responses.append(make_dump_text(7, valid=1, result_class=7, display="CONT", unit="Ohm", beep=1, family=3))
                elif submode == 8:
                    responses.append(make_trace_text(8, family=4, selector="0510", apply="0515"))
                    responses.append(make_dump_text(8, family=4))
                else:
                    responses.append(make_trace_text(submode, family=5, selector="0512"))
                    responses.append(make_dump_text(submode, family=5))

            SequenceSerial.responses = responses
            SequenceSerial.instances = []
            openscope_live_debug.PosixSerial = SequenceSerial
            openscope_live_debug.time.sleep = lambda _seconds: None

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = openscope_live_debug.main([
                    "shorted-probes-sweep",
                    "--port", "/dev/fake",
                    "--settle-ms", "700",
                    "--samples", "1",
                    "--timeout", "7",
                ])
        finally:
            openscope_live_debug.PosixSerial = original_serial
            openscope_live_debug.time.sleep = original_sleep

        self.assertEqual(rc, 0)
        written = SequenceSerial.instances[0].written
        self.assertEqual(written[0:3], ["mode meter 0 0", "meter trace", "meter dump"])
        self.assertEqual(written[-3:], ["mode meter 10 0", "meter trace", "meter dump"])
        self.assertEqual([cmd for cmd in written if cmd.startswith("mode meter")],
                         [f"mode meter {submode} 0" for submode in range(11)])
        self.assertIn("PASS sub=7 Continuity: continuity beep on", out.getvalue())

    def test_shorted_probes_sweep_fails_on_nonzero_dcv(self) -> None:
        original_serial = openscope_live_debug.PosixSerial
        original_sleep = openscope_live_debug.time.sleep
        try:
            responses: list[str] = []
            for submode in range(11):
                responses.append(f"mode=meter submode={submode}")
                responses.append(make_trace_text(submode, display="0.4300", value_i10000=4300,
                                                 family=0 if submode < 2 else min(submode, 5)))
                responses.append(make_dump_text(submode, valid=1, result_class=1, display="0.4300",
                                                unit="V", family=0 if submode < 2 else min(submode, 5)))
            SequenceSerial.responses = responses
            SequenceSerial.instances = []
            openscope_live_debug.PosixSerial = SequenceSerial
            openscope_live_debug.time.sleep = lambda _seconds: None

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = openscope_live_debug.main([
                    "shorted-probes-sweep",
                    "--port", "/dev/fake",
                    "--samples", "1",
                    "--zero-limit-v", "0.02",
                ])
        finally:
            openscope_live_debug.PosixSerial = original_serial
            openscope_live_debug.time.sleep = original_sleep

        self.assertEqual(rc, 5)
        self.assertIn("FAIL sub=0 DC Voltage", out.getvalue())


if __name__ == "__main__":
    unittest.main()
