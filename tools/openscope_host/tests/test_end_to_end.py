"""Host tool against the REAL firmware protocol code, no hardware.

firmware/src/drivers/esp_comm.c is compiled into a shared library with a
small shim (fw_shim.c) and driven through ctypes. A FakeLink stands in for
pyserial: host writes go into esp_comm_route() exactly as CDC RX chunks do
on the device, protocol replies come back from the firmware's own writer,
and non-protocol bytes reach a tiny fake shell. So these tests exercise the
real parser, dispatcher, router and STATUS encoder from the host's side —
including the acceptance criteria of remote_protocol.md §6.
"""
from __future__ import annotations

import ctypes
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DRIVERS = os.path.join(ROOT, "firmware", "src", "drivers")
sys.path.insert(0, os.path.join(HERE, ".."))

from openscope import cli, proto  # noqa: E402
from openscope.device import Device, DeviceError, Nak, Timeout  # noqa: E402

_LIB = None


def lib():
    global _LIB
    if _LIB is None:
        cc = shutil.which("cc") or shutil.which("gcc")
        if cc is None:
            raise unittest.SkipTest("no C compiler for the firmware shim")
        out = os.path.join(tempfile.mkdtemp(), "libfwshim.so")
        subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                        "-I", DRIVERS, os.path.join(HERE, "fw_shim.c"),
                        os.path.join(DRIVERS, "esp_comm.c"), "-o", out], check=True)
        L = ctypes.CDLL(out)
        L.shim_feed.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint32]
        L.shim_poll.argtypes = [ctypes.c_uint32]
        L.shim_take_tx.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
        L.shim_take_tx.restype = ctypes.c_uint32
        L.shim_take_shell.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
        L.shim_take_shell.restype = ctypes.c_uint32
        L.shim_set_meter.argtypes = [ctypes.c_uint32, ctypes.c_float] + [ctypes.c_int] * 4 + [ctypes.c_char_p] * 2
        L.shim_set_meter_wrong_mode.argtypes = [ctypes.c_int]
        L.shim_set_status.argtypes = [ctypes.c_int] * 4 + [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int]
        _LIB = L
    return _LIB


class FakeLink:
    """pyserial stand-in wired to the firmware shim (64-byte 'USB packets')."""

    def __init__(self):
        self.L = lib()
        self.L.shim_init()
        self.port = "/dev/fake-openscope"
        self.now = 0
        self.rx = bytearray()          # device -> host bytes not yet read
        self.shell_line = bytearray()
        self.shell_seen = bytearray()
        self.fail_next_write = False
        self.fail_next_read_after_write = False
        self.nak_status_after_reopen = False
        self._corrupt_next_status = False
        self.reopens = 0

    # device side ------------------------------------------------------
    def _pump(self):
        buf = ctypes.create_string_buffer(65536)
        n = self.L.shim_take_tx(buf, 65536)
        self.rx += buf.raw[:n]
        n = self.L.shim_take_shell(buf, 65536)
        for b in buf.raw[:n]:
            self.shell_seen.append(b)
            if b in (0x0D, 0x0A):
                # Like firmware shell_feed(): EVERY CR and every LF ends a line,
                # echoes CRLF and prints a prompt (so CRLF yields two prompts).
                line = bytes(self.shell_line).decode()
                self.shell_line.clear()
                self.rx += line.encode() + b"\r\n"
                if line:
                    self.rx += self._shell_reply(line)
                self.rx += b"> "
            else:
                self.shell_line.append(b)

    @staticmethod
    def _shell_reply(line):
        return {"version": b"OpenScope 2C53T\r\nBuild: test\r\n"}.get(line, b"Unknown command\r\n")

    # pyserial-ish surface used by Device ------------------------------
    def write(self, data):
        if self._corrupt_next_status and data[:2] == bytes([0xAA, proto.CMD_STATUS]):
            self._corrupt_next_status = False
            data = data[:-1] + bytes([data[-1] ^ 0xFF])         # garbled by the reconnect -> NAK
        if self.fail_next_write:
            self.fail_next_write = False
            raise OSError(6, "Device not configured")        # what macOS says after a replug
        for i in range(0, len(data), 64):
            self.now += 1
            self.L.shim_feed(data[i:i + 64], len(data[i:i + 64]), self.now)
        self._pump()

    def read(self, n=4096):
        if self.fail_next_read_after_write:
            self.fail_next_read_after_write = False
            raise OSError(6, "Device not configured")         # replug after the device acted
        self.now += 5
        self.L.shim_poll(self.now)
        self._pump()
        out = bytes(self.rx[:n])
        del self.rx[:n]
        return out

    def drain(self, quiet=0.15, max_wait=1.5):
        return self.read(1 << 20)

    def reopen(self):
        self.reopens += 1
        if self.nak_status_after_reopen:
            self._corrupt_next_status = True

    def open(self):
        self.opened = True

    closed = False

    def close(self):
        self.closed = True


def device(timeout=0.3):
    return Device(FakeLink(), timeout=timeout)


class TestAcceptance(unittest.TestCase):
    """remote_protocol.md §6 acceptance, against the firmware's own code."""

    def test_info_reports_actual_mode_and_battery(self):
        dev = device()
        dev.link.L.shim_set_status(0, 64, proto.FLAG_CHARGING, 3900, 5000, 0, 0)
        out = io.StringIO()
        with mock.patch.object(cli.Device, "open", return_value=dev), redirect_stdout(out):
            self.assertEqual(cli.main(["info"]), 0)
        text = out.getvalue()
        self.assertIn("firmware  OpenScope 2C53T shim", text)
        self.assertIn("mode      scope", text)
        self.assertIn("battery   64% (3900 mV, charging)", text)

        # "verified by changing mode on the device and seeing the value change"
        dev.link.L.shim_set_status(1, 63, 0, 3890, 6000, 0, 0)
        out = io.StringIO()
        with mock.patch.object(cli.Device, "open", return_value=dev), redirect_stdout(out):
            cli.main(["info"])
        self.assertIn("mode      meter", out.getvalue())
        self.assertIn("battery   63% (3890 mV)", out.getvalue())

    def test_no_device_is_a_clean_exit_1(self):
        err = io.StringIO()
        with mock.patch("openscope.link.candidate_ports", return_value=[]), redirect_stderr(err):
            self.assertEqual(cli.main(["info"]), 1)
        self.assertIn("no OpenScope found", err.getvalue())
        with mock.patch.object(cli, "candidate_ports", return_value=[]), redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["ports"]), 1)


class TestButtons(unittest.TestCase):
    def test_press_reaches_the_injector(self):
        dev = device()
        dev.press("MENU")
        self.assertEqual(dev.link.L.shim_last_button(), 9)
        self.assertEqual(dev.link.L.shim_presses(), 1)

    def test_full_queue_is_a_nak_not_success(self):
        dev = device()
        dev.link.L.shim_set_inject_ok(0)
        with self.assertRaises(Nak) as cm:
            dev.press("OK")
        self.assertEqual(proto.ERRORS[cm.exception.code], "NOT_READY")
        err = io.StringIO()
        with mock.patch.object(cli.Device, "open", return_value=dev), redirect_stderr(err):
            self.assertEqual(cli.main(["press", "OK"]), 2)
        self.assertIn("NOT_READY", err.getvalue())


class TestMeter(unittest.TestCase):
    def test_no_reading_yet_is_not_ready(self):
        dev = device()
        with self.assertRaises(Nak) as cm:
            dev.meter()
        self.assertEqual(proto.ERRORS[cm.exception.code], "NOT_READY")

    def test_reading_roundtrip_through_firmware_encoder(self):
        dev = device()
        dev.link.L.shim_set_meter(42, 1.6141, 16141, 3, 1, 0x04, b"V", b"1.6141")
        m = dev.meter()
        self.assertEqual((m.update_count, m.raw_bcd, m.decimal_pos, m.unit, m.display), (42, 16141, 3, "V", "1.6141"))
        self.assertAlmostEqual(m.value, 1.6141, places=5)
        self.assertEqual(m.result, "normal")
        self.assertTrue(m.autorange and not m.hold and not m.negative)

    def test_continuous_log_survives_not_ready(self):
        dev = device()
        calls = {"n": 0}
        real = dev.meter

        def flaky():
            calls["n"] += 1
            if calls["n"] == 1:
                raise Nak(proto.CMD_GET_METER, 0x07)          # NOT_READY during a range change
            if calls["n"] > 3:
                raise KeyboardInterrupt                         # stop the "until Ctrl-C" loop
            dev.link.L.shim_set_meter(calls["n"], 1.0, 1000, 3, 1, 0, b"V", b"1.000")
            return real()
        dev.meter = flaky
        with mock.patch.object(cli.Device, "open", return_value=dev), \
                mock.patch("time.sleep"), redirect_stdout(io.StringIO()) as out, redirect_stderr(io.StringIO()):
            self.assertEqual(cli.main(["meter", "--count", "0"]), 0)
        self.assertIn("1.000 V", out.getvalue())

    def test_outside_meter_mode_is_refused_not_frozen(self):
        dev = device()
        dev.link.L.shim_set_meter(42, 1.6141, 16141, 3, 1, 0, b"V", b"1.6141")
        dev.link.L.shim_set_meter_wrong_mode(1)
        with self.assertRaises(Nak) as cm:
            dev.meter()
        self.assertEqual(proto.ERRORS[cm.exception.code], "UNSUPPORTED_IN_MODE")

    def test_finite_count_gives_up_on_a_frozen_meter(self):
        dev = device()
        dev.link.L.shim_set_meter(5, 1.0, 1000, 3, 1, 0, b"V", b"1.000")   # never changes
        clock = {"t": 1000.0}

        def fake_time():
            clock["t"] += 1.0
            return clock["t"]
        err = io.StringIO()
        with mock.patch.object(cli.Device, "open", return_value=dev), mock.patch("time.sleep"), \
                mock.patch.object(cli, "_now", fake_time), redirect_stdout(io.StringIO()), redirect_stderr(err):
            self.assertEqual(cli.main(["meter", "--count", "2"]), 2)
        self.assertIn("not updating", err.getvalue())

    def test_cli_meter_logs_csv(self):
        import csv
        dev = device()
        dev.link.L.shim_set_meter(7, 3.3, 3300, 3, 1, 0, b"V", b"3.300")
        log = os.path.join(tempfile.mkdtemp(), "m.csv")
        with mock.patch.object(cli.Device, "open", return_value=dev), redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["meter", "--count", "1", "--log", log]), 0)
        rows = list(csv.DictReader(open(log)))
        self.assertEqual(rows[0]["display"], "3.300")
        self.assertEqual(rows[0]["raw_bcd"], "3300")


class TestSharedStream(unittest.TestCase):
    def test_shell_keeps_working_next_to_the_protocol(self):
        dev = device()
        self.assertEqual(dev.ping(), "OpenScope 2C53T shim")
        self.assertEqual(dev.shell("version"), "OpenScope 2C53T\r\nBuild: test")
        self.assertEqual(dev.status().mode_name, "scope")
        self.assertNotIn(0xAA, dev.link.shell_seen, "a protocol byte leaked into the shell")

    def test_abandoned_request_does_not_poison_the_next(self):
        dev = device()
        dev.link.write(proto.encode(proto.CMD_BUTTON, b"\x09")[:-2])   # host dies mid-frame
        dev.link.read()                                                # time passes (gap)
        dev.link.now += 100
        self.assertEqual(dev.ping(), "OpenScope 2C53T shim")
        self.assertEqual(dev.link.L.shim_presses(), 0)


class TestRound3(unittest.TestCase):
    def test_second_write_failure_is_a_device_error(self):
        dev = device()
        def always_fail(data):
            raise OSError(6, "Device not configured")
        dev.link.write = always_fail
        for call in (dev.ping, dev.status, lambda: dev.press("OK"), lambda: dev.shell("version")):
            with self.assertRaises(DeviceError) as cm:
                call()
            self.assertIn("port lost twice", str(cm.exception))

    def test_unknown_major_refused_before_anything_acts(self):
        dev = device()
        with mock.patch.object(proto, "PROTO_MAJOR", 2):
            with self.assertRaises(proto.ProtocolError):
                dev._check_version()
        self.assertEqual(dev.link.L.shim_presses(), 0)

    def test_version_rechecked_after_reopen(self):
        dev = device()
        dev._check_version()
        dev.link.fail_next_write = True
        with mock.patch.object(proto, "PROTO_MAJOR", 2):
            with self.assertRaises(proto.ProtocolError):
                dev.press("OK")                 # the reopened port speaks another major
        self.assertEqual(dev.link.L.shim_presses(), 0, "pressed on a firmware we cannot speak to")

    def test_battery_unknown_is_not_printed_as_zero(self):
        dev = device()
        dev.link.L.shim_set_status(0, 0, proto.FLAG_CHARGING | proto.FLAG_BATT_UNKNOWN, 0, 1000, 0, 0)
        out = io.StringIO()
        with mock.patch.object(cli.Device, "open", return_value=dev), redirect_stdout(out):
            cli.main(["info"])
        self.assertIn("battery   unknown (no sample yet, charging)", out.getvalue())
        self.assertNotIn("0%", out.getvalue())


class TestRound4(unittest.TestCase):
    def test_open_failures_are_one_line_exits(self):
        for exc in (proto.ProtocolError("device speaks protocol v2"), Timeout("no reply to 0x08"),
                    OSError(16, "Resource busy")):
            err = io.StringIO()
            with mock.patch.object(cli.Device, "open", side_effect=exc), redirect_stderr(err):
                self.assertEqual(cli.main(["info"]), 2, repr(exc))
            self.assertEqual(len(err.getvalue().strip().splitlines()), 1, err.getvalue())

    def test_port_that_keeps_vanishing_does_not_recurse(self):
        dev = device()
        dev._check_version()                                   # proto_version set, as after open()
        def always_fail(data):
            raise OSError(6, "Device not configured")
        dev.link.write = always_fail
        with self.assertRaises(DeviceError):
            dev.ping()
        self.assertLessEqual(dev.link.reopens, 2, "reopen recursed")


class TestRound5(unittest.TestCase):
    def test_nak_during_reopen_keeps_may_have_acted(self):
        from openscope import mcp_server
        dev = device()
        dev._check_version()
        dev.link.fail_next_read_after_write = True            # BUTTON sent, then the port drops
        dev.link.nak_status_after_reopen = True               # and the version re-check is NAKed
        with self.assertRaises(DeviceError) as cm:
            dev.press("OK")
        self.assertIn("may already have acted", str(cm.exception))
        self.assertEqual(dev.link.L.shim_presses(), 1)
        dev2 = device()
        dev2._check_version()
        dev2.link.fail_next_read_after_write = True
        dev2.link.nak_status_after_reopen = True
        s = mcp_server.ScopeSession(opener=lambda port: dev2)
        with self.assertRaises(RuntimeError) as cm2:
            s.press(["OK"])
        self.assertIn("MAY OR MAY NOT", str(cm2.exception))
        self.assertNotIn("NOT pressed (", str(cm2.exception).replace("MAY OR MAY NOT", ""))

    def test_device_open_itself_refuses_an_unknown_major(self):
        from openscope import device as devmod
        made = {}

        def factory(port=None, **kw):
            made["link"] = FakeLink()
            return made["link"]
        with mock.patch.object(devmod, "SerialLink", factory), mock.patch.object(proto, "PROTO_MAJOR", 2):
            with self.assertRaises(proto.ProtocolError):
                Device.open()
        self.assertTrue(made["link"].closed, "port left open after refusing")
        self.assertEqual(made["link"].L.shim_presses(), 0)


class TestTransportLoss(unittest.TestCase):
    def test_button_is_not_resent_after_its_write_went_through(self):
        dev = device()
        dev.link.fail_next_read_after_write = True
        with self.assertRaises(DeviceError):
            dev.press("OK")
        self.assertEqual(dev.link.L.shim_presses(), 1, "the press happened once and was not repeated")

    def test_query_is_resent_after_a_lost_reply(self):
        dev = device()
        dev.link.fail_next_read_after_write = True
        self.assertEqual(dev.ping(), "OpenScope 2C53T shim")
        self.assertEqual(dev.link.reopens, 1)

    def test_non_read_only_shell_is_not_resent(self):
        dev = device()
        sent = []
        real_write = dev.link.write

        def write(data):
            sent.append(data)
            real_write(data)
            dev.link.fail_next_read_after_write = True         # `fwswap b` resets the device
        dev.link.write = write
        with self.assertRaises(DeviceError):
            dev.shell("fwswap b")
        self.assertEqual(sum(1 for d in sent if d.startswith(b"fwswap")), 1, "re-sent a resetting command")

    def test_flush_failure_counts_as_possibly_sent(self):
        from openscope.link import FlushFailed
        dev = device()
        real_write = dev.link.write
        state = {"first": True}

        def write(data):
            real_write(data)                                    # the device did get it
            if state["first"]:
                state["first"] = False
                raise FlushFailed("tcdrain: device vanished")
        dev.link.write = write
        with self.assertRaises(DeviceError):
            dev.press("OK")
        self.assertEqual(dev.link.L.shim_presses(), 1, "a possibly-delivered press was re-sent")

    def test_shell_survives_a_replug(self):
        dev = device()
        dev.link.fail_next_write = True
        self.assertEqual(dev.shell("version"), "OpenScope 2C53T\r\nBuild: test")
        self.assertEqual(dev.link.reopens, 1)

    def test_replug_mid_request_is_retried_once(self):
        dev = device()
        dev.link.fail_next_write = True          # CDC self-heal (#39) looks like a replug
        self.assertEqual(dev.ping(), "OpenScope 2C53T shim")
        self.assertEqual(dev.link.reopens, 1)

    def test_silent_device_times_out_with_a_real_error(self):
        dev = device(timeout=0.05)
        dev.link.write = lambda data: None       # swallowed: nothing ever answers
        with self.assertRaises(Timeout):
            dev.ping()


if __name__ == "__main__":
    unittest.main(verbosity=2)
