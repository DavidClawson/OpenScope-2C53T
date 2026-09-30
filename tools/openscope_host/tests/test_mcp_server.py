"""MCP layer: the session logic against the real firmware protocol (shim),
plus the safety allowlist. The FastMCP wiring is smoke-tested only when the
`mcp` package is importable (it needs Python >= 3.10)."""
from __future__ import annotations

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))

from openscope import mcp_server  # noqa: E402
from openscope.link import NoDevice  # noqa: E402
from test_end_to_end import device  # noqa: E402


def session(allow_raw=False, dev=None):
    d = dev or device()
    return mcp_server.ScopeSession(allow_raw_shell=allow_raw, opener=lambda port: d), d


class TestSession(unittest.TestCase):
    def test_info_is_structured(self):
        s, d = session()
        d.link.L.shim_set_status(1, 80, 0, 4000, 12345, 3, 1)
        info = s.info()
        self.assertEqual(info["mode"], "meter")
        self.assertEqual(info["battery_pct"], 80)
        self.assertEqual((info["usb_tx_stalls"], info["usb_self_heals"]), (3, 1))
        self.assertEqual(info["firmware"], "OpenScope 2C53T shim")

    def test_press_validates_all_before_pressing_any(self):
        s, d = session()
        with self.assertRaises(ValueError):
            s.press(["MENU", "NOPE"])
        self.assertEqual(d.link.L.shim_presses(), 0, "a bad name must not leave half the sequence pressed")
        s.press(["MENU", "OK"])
        self.assertEqual(d.link.L.shim_presses(), 2)

    def test_partial_press_sequence_says_what_acted(self):
        s, d = session()
        calls = {"n": 0}
        real = d.press

        def press(b):
            calls["n"] += 1
            if calls["n"] == 2:
                from openscope.device import Nak
                raise Nak(0x0A, 0x07)                       # queue full on the 2nd press
            return real(b)
        d.press = press
        with self.assertRaises(RuntimeError) as cm:
            s.press(["MENU", "OK", "UP"])
        msg = str(cm.exception)
        self.assertIn("pressed MENU", msg)
        self.assertIn("OK NOT pressed", msg)
        self.assertIn("remaining not sent: UP", msg)
        self.assertEqual(d.link.L.shim_presses(), 1)

    def test_lost_reply_is_not_reported_as_not_pressed(self):
        from openscope.device import Timeout
        s, d = session()
        real = d.press
        calls = {"n": 0}

        def press(b):
            calls["n"] += 1
            real(b)                                         # the device got it...
            if calls["n"] == 2:
                raise Timeout("no reply to 0x0A")           # ...but the ACK was lost
        d.press = press
        with self.assertRaises(RuntimeError) as cm:
            s.press(["MENU", "OK"])
        msg = str(cm.exception)
        self.assertIn("OK MAY OR MAY NOT have been pressed", msg)
        self.assertNotIn("OK NOT pressed", msg)

    def test_only_the_buttons_own_nak_means_not_pressed(self):
        from openscope.device import Nak
        s, d = session()
        def press(b):
            raise Nak(0x08, 0x02)                           # a STATUS NAK, not the button's
        d.press = press
        with self.assertRaises(RuntimeError) as cm:
            s.press(["OK"])
        self.assertIn("MAY OR MAY NOT", str(cm.exception))

    def test_power_refused_by_default(self):
        s, d = session()
        with self.assertRaises(RuntimeError):
            s.press(["POWER"])
        self.assertEqual(d.link.L.shim_presses(), 0)
        s2, d2 = session(allow_raw=True)
        s2.press(["POWER"])
        self.assertEqual(d2.link.L.shim_last_button(), 15)

    def test_shell_allowlist(self):
        s, d = session()
        self.assertIn("OpenScope 2C53T", s.shell("version"))
        for dangerous in ("fwapply", "flash wtest 0x1000 CONFIRM", "spi3 opread 41 4", "version; fwapply"):
            with self.assertRaises(RuntimeError, msg=dangerous):
                s.shell(dangerous)
        self.assertNotIn(b"fwapply", bytes(d.link.shell_seen), "refused command reached the device")

    def test_raw_shell_when_allowed(self):
        s, _ = session(allow_raw=True)
        self.assertIn("Unknown command", s.shell("spi3 opread 41 4"))

    def test_no_device_is_an_explanation(self):
        def boom(port):
            raise NoDevice("no OpenScope found on USB 2e3c:5740 (AT32 VCP)")
        s = mcp_server.ScopeSession(opener=boom)
        with self.assertRaises(RuntimeError) as cm:
            s.info()
        self.assertIn("plugged in", str(cm.exception))


class TestFastMcpWiring(unittest.TestCase):
    def test_tools_registered(self):
        try:
            import mcp  # noqa: F401
        except ImportError:
            self.skipTest("mcp SDK not installed (needs Python >= 3.10)")
        import asyncio
        s, _ = session()
        server = mcp_server.build_server(s)
        tools = asyncio.run(server.list_tools())
        self.assertEqual(sorted(t.name for t in tools),
                         ["scope_info", "scope_meter", "scope_press", "scope_screenshot", "scope_shell"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
