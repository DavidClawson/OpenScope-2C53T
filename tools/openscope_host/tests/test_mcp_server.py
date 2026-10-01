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
from test_end_to_end import arm_waveform, device  # noqa: E402


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


class TestWaveform(unittest.TestCase):
    def test_header_samples_and_summary(self):
        s, d = session()
        c1, _ = arm_waveform(d, step=2)
        r = s.waveform([1, 2])
        self.assertEqual([c["channel"] for c in r["channels"]], ["CH1", "CH2"])
        ch1 = r["channels"][0]
        self.assertEqual(bytes(ch1["samples"]), c1)
        self.assertEqual(ch1["sample_rate_hz"], 12490)
        self.assertEqual(ch1["vdiv_tier"], "measured")
        sm = ch1["summary"]
        self.assertAlmostEqual(sm["frequency_hz"], 12490 / 40.0, delta=2.0)
        self.assertIsNotNone(sm["vpp_volts"])
        self.assertIn("uncalibrated", sm["vpp_note"])
        self.assertEqual(r["channels"][1]["vdiv_tier"], "provisional")
        self.assertTrue(any("head" in n for n in r["notes"]))

    def test_repeated_frame_is_called_out(self):
        s, d = session()
        arm_waveform(d, step=0)
        s.waveform()
        r = s.waveform()
        self.assertTrue(any("no new capture" in n for n in r["notes"]))

    def test_no_capture_is_explained(self):
        s, _ = session()
        with self.assertRaises(RuntimeError) as cm:
            s.waveform([1])
        self.assertIn("NO_CAPTURE_DATA", str(cm.exception))
        self.assertIn("never substitutes its demo trace", str(cm.exception))

    def test_unmeasured_range_says_counts(self):
        s, d = session()
        arm_waveform(d, tb=0x08, tb_tier=0, rate=0, vdiv1=(2, 0, 0))
        sm = s.waveform([1])["channels"][0]["summary"]
        self.assertIsNone(sm["vpp_volts"])
        self.assertIn("raw ADC counts", sm["vpp_note"])
        self.assertIsNone(sm["frequency_hz"])
        self.assertIn("0x08", sm["frequency_note"])

    def test_bad_channel_is_a_readable_refusal(self):
        s, _ = session()
        with self.assertRaises(RuntimeError):
            s.waveform([3])


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
                         ["scope_info", "scope_meter", "scope_press", "scope_screenshot", "scope_shell",
                          "scope_waveform"])


class TestRefusalsReachTheModel(unittest.TestCase):
    def test_tool_error_carries_the_device_reason(self):
        try:
            import mcp  # noqa: F401
        except ImportError:
            self.skipTest("mcp SDK not installed (needs Python >= 3.10)")
        import asyncio
        s, d = session()
        d.link.L.shim_set_meter_wrong_mode(1)
        server = mcp_server.build_server(s)
        try:
            from mcp.server.mcpserver.exceptions import ToolError   # mcp >= 2
        except ImportError:
            ToolError = Exception                                   # mcp 1.x wraps every message
        with self.assertRaises(Exception) as cm:
            asyncio.run(server.call_tool("scope_meter", {}))
        # mcp >= 2 forwards only a ToolError's message to the model; anything
        # else reaches it as a bare "Error executing tool scope_meter".
        self.assertIsInstance(cm.exception, ToolError)
        self.assertIn("UNSUPPORTED_IN_MODE", str(cm.exception))

    def test_waveform_tool_refuses_and_answers_through_the_sdk(self):
        try:
            import mcp  # noqa: F401
        except ImportError:
            self.skipTest("mcp SDK not installed (needs Python >= 3.10)")
        import asyncio
        try:
            from mcp.server.mcpserver.exceptions import ToolError   # mcp >= 2
        except ImportError:
            ToolError = Exception
        s, d = session()
        server = mcp_server.build_server(s)
        with self.assertRaises(Exception) as cm:
            asyncio.run(server.call_tool("scope_waveform", {"channels": [1]}))
        self.assertIsInstance(cm.exception, ToolError)
        self.assertIn("NO_CAPTURE_DATA", str(cm.exception))
        arm_waveform(d)
        out = asyncio.run(server.call_tool("scope_waveform", {}))
        self.assertIn("frame_id", str(out))


if __name__ == "__main__":
    unittest.main(verbosity=2)
