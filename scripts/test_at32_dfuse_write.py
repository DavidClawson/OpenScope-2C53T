#!/usr/bin/env python3
"""ROM DFU writer: libusb discovery and busy-stall handling, with a fake USB stack."""
from __future__ import annotations

import errno
import importlib.util
import io
import sys
import types
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock


class FakeUSBError(Exception):
    def __init__(self, strerror: str, code: int = 0, errno_: int | None = None) -> None:
        super().__init__(strerror)
        self.errno = errno_


fake_libusb1 = types.SimpleNamespace(get_backend=lambda find_library=None: None)
fake_usb = types.SimpleNamespace()
fake_usb.core = types.SimpleNamespace(USBError=FakeUSBError, find=lambda **kw: None)
fake_usb.util = types.SimpleNamespace()
fake_usb.backend = types.SimpleNamespace(libusb1=fake_libusb1)
FAKE_MODULES = {
    "usb": fake_usb,
    "usb.core": fake_usb.core,
    "usb.util": fake_usb.util,
    "usb.backend": fake_usb.backend,
    "usb.backend.libusb1": fake_libusb1,
}

MODULE_PATH = Path(__file__).resolve().parent / "at32_dfuse_write.py"
with mock.patch.dict(sys.modules, FAKE_MODULES):
    SPEC = importlib.util.spec_from_file_location("at32_dfuse_write", MODULE_PATH)
    assert SPEC is not None and SPEC.loader is not None
    dfuse = importlib.util.module_from_spec(SPEC)
    SPEC.loader.exec_module(dfuse)

PIPE = FakeUSBError("Pipe error", -9, errno.EPIPE)


class StallingDevice:
    """GETSTATUS replies in order; an exception entry is raised instead."""

    def __init__(self, replies) -> None:
        self.replies = list(replies)
        self.polls = 0

    def ctrl_transfer(self, bm, req, value, index, data_or_len, timeout=0):
        assert req == dfuse.DFU_GETSTATUS
        self.polls += 1
        reply = self.replies.pop(0)
        if isinstance(reply, Exception):
            raise reply
        return reply


def status(state: int, poll_ms: int = 1) -> bytes:
    return bytes([0, poll_ms, 0, 0, state, 0])


class DfuseWriteTests(unittest.TestCase):
    def setUp(self) -> None:
        sleep = mock.patch.object(dfuse.time, "sleep")
        sleep.start()
        self.addCleanup(sleep.stop)

    def test_busy_stalls_are_retried_like_dfu_util(self) -> None:
        dev = StallingDevice([status(4), PIPE, PIPE, status(dfuse.STATE_DFU_DNLOAD_IDLE)])
        with redirect_stdout(io.StringIO()) as out:
            dfuse.wait_idleish(dev, "erase 0x08004000")
        self.assertEqual(dev.polls, 4)
        self.assertIn("stalled GETSTATUS", out.getvalue())

    def test_persistent_stall_names_the_step(self) -> None:
        dev = StallingDevice([PIPE] * 5)
        with redirect_stdout(io.StringIO()), self.assertRaises(RuntimeError) as ctx:
            dfuse.wait_idleish(dev, "erase 0x08006800")
        self.assertIn("erase 0x08006800", str(ctx.exception))
        self.assertEqual(dev.polls, dfuse.MAX_BUSY_STALLS + 1)

    def test_other_usb_errors_are_not_retried(self) -> None:
        dev = StallingDevice([FakeUSBError("No such device", -4, errno.ENODEV)])
        with self.assertRaises(RuntimeError):
            dfuse.wait_idleish(dev, "write")
        self.assertEqual(dev.polls, 1)

    def test_libusb_found_in_homebrew_when_not_on_system_path(self) -> None:
        seen = []

        def get_backend(find_library=None):
            if find_library is None:
                return None
            seen.append(find_library("usb-1.0"))
            return "backend"

        brew = dfuse.LIBUSB_FALLBACKS[0]
        with mock.patch.dict(sys.modules, FAKE_MODULES), \
             mock.patch.object(fake_libusb1, "get_backend", get_backend), \
             mock.patch.object(dfuse.Path, "exists", lambda self: str(self) == brew):
            self.assertEqual(dfuse.libusb_backend(), "backend")
        self.assertEqual(seen, [brew])

    def test_missing_libusb_says_how_to_install_it(self) -> None:
        with mock.patch.dict(sys.modules, FAKE_MODULES), \
             mock.patch.object(dfuse.Path, "exists", lambda self: False), \
             self.assertRaises(SystemExit) as ctx:
            dfuse.libusb_backend()
        self.assertIn("brew install libusb", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
