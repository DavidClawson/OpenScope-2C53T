"""link.py without hardware: the bounded drain that replaced ser.flush()."""
from __future__ import annotations

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from openscope.link import FlushFailed, SerialLink  # noqa: E402


class FakeSer:
    def __init__(self, drains_after=None, raise_on_poll=None):
        self.writes = []
        self.polls = 0
        self.drains_after = drains_after
        self.raise_on_poll = raise_on_poll

    def write(self, data):
        self.writes.append(data)

    @property
    def out_waiting(self):
        self.polls += 1
        if self.raise_on_poll:
            raise self.raise_on_poll
        return 0 if (self.drains_after is not None and self.polls > self.drains_after) else 5

    def flush(self):
        raise AssertionError("ser.flush() is an unbounded tcdrain and must not be used")


def link(ser, timeout=0.05):
    l = SerialLink(drain_timeout=timeout)
    l._ser = ser
    return l


class TestBoundedDrain(unittest.TestCase):
    def test_returns_once_the_os_queue_is_empty(self):
        ser = FakeSer(drains_after=3)
        link(ser).write(b"abc")
        self.assertEqual(ser.writes, [b"abc"])

    def test_a_device_that_never_accepts_data_is_bounded(self):
        with self.assertRaises(FlushFailed):
            link(FakeSer(drains_after=None)).write(b"abc")

    def test_termios_style_error_becomes_possibly_delivered(self):
        import termios
        with self.assertRaises(FlushFailed) as cm:
            link(FakeSer(raise_on_poll=termios.error(6, "Device not configured"))).write(b"x")
        self.assertIsInstance(cm.exception, OSError)


if __name__ == "__main__":
    unittest.main(verbosity=2)
