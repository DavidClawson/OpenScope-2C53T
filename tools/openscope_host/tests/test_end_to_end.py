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
from openscope.device import Device, Nak, Timeout  # noqa: E402

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
                line = bytes(self.shell_line).decode()
                self.shell_line.clear()
                if line:
                    self.rx += line.encode() + b"\r\n" + self._shell_reply(line) + b"> "
            else:
                self.shell_line.append(b)

    @staticmethod
    def _shell_reply(line):
        return {"version": b"OpenScope 2C53T\r\nBuild: test\r\n"}.get(line, b"Unknown command\r\n")

    # pyserial-ish surface used by Device ------------------------------
    def write(self, data):
        if self.fail_next_write:
            self.fail_next_write = False
            raise OSError(6, "Device not configured")        # what macOS says after a replug
        for i in range(0, len(data), 64):
            self.now += 1
            self.L.shim_feed(data[i:i + 64], len(data[i:i + 64]), self.now)
        self._pump()

    def read(self, n=4096):
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

    def close(self):
        pass


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


class TestTransportLoss(unittest.TestCase):
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
