"""MCP layer: the session logic against the real firmware protocol (shim),
plus the shell levels (readonly/bench/unsafe), the never-over-MCP deny-list
and POWER. The FastMCP wiring is tested only when the `mcp` package is
importable (it needs Python >= 3.10); with mcp >= 2 refusals are also driven
through an in-process MCP client, i.e. exactly what the model receives."""
from __future__ import annotations

import asyncio
import contextlib
import io
import logging
import os
import re
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))

from openscope import device as devmod  # noqa: E402
from openscope import mcp_server  # noqa: E402
from openscope.link import NoDevice  # noqa: E402
from test_end_to_end import DRIVERS, device  # noqa: E402

LEVELS = devmod.SHELL_LEVELS


def session(allow_raw=False, dev=None, level="readonly"):
    d = dev or device()
    return mcp_server.ScopeSession(level=level, allow_raw_shell=allow_raw,
                                   opener=lambda port: d), d


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
        s2, d2 = session(allow_raw=True)        # --allow-raw-shell no longer lifts it
        with self.assertRaises(RuntimeError):
            s2.press(["POWER"])
        self.assertEqual(d2.link.L.shim_presses(), 0)

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


# ── shell levels ──────────────────────────────────────────────────────
# Lines as the experiment scripts / an agent would send them.
BENCH_LINES = (
    "fpga scope timebase 1A", "fpga scope range 3 1", "fpga scope center ch2 4",
    "fpga scope vdiv 1 5", "fpga scope trigmode normal", "fpga scope level -20",
    "fpga scope edge falling", "fpga scope hpos 160", "fpga scope softtrig on",
    "trig 3 40", "trig raw 2048", "trig2 raw 100", "mode meter 1", "mode 0", "mode",
    "fpga postedge 0", "fpga acqbr off", "fpga pairgap 0", "fpga rearm off",
    "fpga scope measure 3", "fpga scope freq", "spi3 read 1024", "spi3 opread 04 1026 dump",
    "spi3 opread 05", "spi3 opread 4 16", "spi3 opread 0x05 0x400 dump",
    "spi3 frame", "gpio read B11", "gpio scan", "meter trace", "cal status", "flash jedec",
) + tuple(n for n in devmod.BENCH_SHELL if n not in devmod.BENCH_ARG_RULES)
# Bench names whose arguments fall outside BENCH_ARG_RULES: unsafe only.
BENCH_ARG_REFUSED = (
    "spi3 opread 01 4", "spi3 opread 41", "spi3 opread 3C 1", "spi3 opread 06 2048 dump",
    "spi3 opread 15", "spi3 opread 004", "spi3 opread 04 16 dump x", "spi3 opread 04 16 DUMP",
    "spi3 opread",
)
UNSAFE_ONLY_LINES = (
    "spi3 seq 01 1A", "spi3 xfer 11 00 00 00", "fpga frame 0x05 0x14", "fpga cmd 05 14",
    "usart tx 05 14", "usbstat heal on", "mem read 0x40021000 4", "meter stream 10",
    "fpga stock commit", "fpga busrelease", "flash diag", "screen dump", "?",
    "nosuchcommand", "version; fwapply",
)
NEVER_LINES = (
    "fwload 1024 DEADBEEF a", "fwapply", "fwswap b", "fwcrumb clear", "cal backup",
    "cal restore force CONFIRM", "flash wtest 0x1000 CONFIRM", "mem write 0x40010C10 0x800",
    "mode startup meter", "mode startup", "reboot bootloader", "gpio set B11 1",
    "gpio mode A6 out", "bench restore", "spi3 armtest pb11", "fpga dbgclk 10", "fpga dbgarm",
    "fpga reinit", "fpga reinit 0 100 600 c9", "fpga reinit 0 100 600 rl",
    "flash erase 0", "flash write 0 00", "iap", "dfu", "reset",
    # spellings that must not slip past the name check
    "FWAPPLY", "  fwapply  ", "Gpio Set B11 1", "mode  startup   meter", "reboot",
)
# The firmware's line editor would rewrite these AFTER the host checked them:
# DEL/backspace delete, tabs and other control bytes are dropped, CR/LF end a
# line. Refused at every level, whatever they spell.
EDITED_LINES = (
    "fw\tapply", "gpio read B1" + "\x7f" * 12 + "set B11 1", "gpio read B1" + "\b" * 12 + "set B11 1",
    "version\rfwapply", "version\nfwapply", "fwapply\x00", "fwa\u0301pply", "x" * 128,
)


class RecordingDevice:
    """Records shell lines (the shim's prompt wait costs 0.1 s per line)."""

    def __init__(self):
        self.lines = []

    def shell(self, line, timeout=3.0):
        self.lines.append(line)
        return "ok"


class TestShellLevels(unittest.TestCase):
    def assertRefused(self, lvl, line, *needles):
        s, d = session(level=lvl)               # the firmware shim: proves nothing hit the wire
        with self.assertRaises(mcp_server.Refused, msg=f"{lvl}: {line!r}") as cm:
            s.shell(line)
        self.assertEqual(bytes(d.link.shell_seen), b"", f"{lvl}: refused {line!r} reached the device")
        for n in needles:
            self.assertIn(n, str(cm.exception), f"{lvl}: {line!r}")
        return str(cm.exception)

    def assertAccepted(self, lvl, line):
        rec = RecordingDevice()
        mcp_server.ScopeSession(level=lvl, opener=lambda port: rec).shell(line)
        self.assertEqual(rec.lines, [" ".join(line.split())], f"{lvl}: {line!r} was not sent as checked")

    def test_what_was_checked_is_what_reaches_the_firmware(self):
        s, d = session(level="bench")
        s.shell("  fpga  scope   timebase 1A ")
        self.assertEqual(bytes(d.link.shell_seen), b"fpga scope timebase 1A\r")

    def test_readonly_is_the_old_allowlist(self):
        for line in devmod.READ_ONLY_SHELL + ("  version\n",):
            self.assertAccepted("readonly", line)
        for line in BENCH_LINES:
            self.assertRefused("readonly", line, "--level readonly", "--level bench")
        for line in UNSAFE_ONLY_LINES:
            self.assertRefused("readonly", line, "--level readonly", "--level unsafe")
        for line in BENCH_ARG_REFUSED:
            msg = self.assertRefused("readonly", line, "--level unsafe")
            self.assertNotIn("It is a bench command", msg, line)
        self.assertRefused("readonly", "versionx")
        self.assertRefused("readonly", "fwcrumb now")

    def test_bench_adds_the_bench_commands(self):
        for line in devmod.READ_ONLY_SHELL + BENCH_LINES:
            self.assertAccepted("bench", line)
        for line in UNSAFE_ONLY_LINES:
            self.assertRefused("bench", line, "--level bench", "--level unsafe")

    def test_bench_spi3_opread_only_reads_the_channels(self):
        for line in BENCH_ARG_REFUSED:
            self.assertRefused("bench", line, "--level bench", "only opcode 04 or 05", "--level unsafe")
        for line in BENCH_ARG_REFUSED[:4]:
            self.assertAccepted("unsafe", line)

    def test_unsafe_is_everything_but_the_deny_list(self):
        for line in devmod.READ_ONLY_SHELL + BENCH_LINES + UNSAFE_ONLY_LINES + BENCH_ARG_REFUSED:
            self.assertAccepted("unsafe", line)

    def test_deny_list_holds_at_every_level(self):
        for lvl in LEVELS:
            for line in NEVER_LINES:
                msg = self.assertRefused(lvl, line, "never available over MCP", "at any --level")
                self.assertIn("readonly, bench, unsafe", msg)

    def test_lines_the_firmware_would_rewrite_are_refused_everywhere(self):
        for lvl in LEVELS:
            for line in EDITED_LINES:
                self.assertRefused(lvl, line, "every --level")

    def test_whole_word_matching(self):
        self.assertAccepted("readonly", "fwcrumb")              # read-only...
        self.assertRefused("unsafe", "fwcrumb clear")           # ...its eraser never
        self.assertAccepted("bench", "trig2 raw 100")           # not shadowed by "trig"
        self.assertRefused("bench", "trigger 3 40")             # "trig" is a word
        self.assertAccepted("unsafe", "fwapplyx")               # not fwapply: firmware says Unknown command
        self.assertRefused("bench", "gpio reader B11")

    def test_power_refused_at_every_level(self):
        for lvl in LEVELS:
            for buttons in (["POWER"], ["MENU", "POWER"], ["power"]):
                s, d = session(level=lvl)
                with self.assertRaises(mcp_server.Refused) as cm:
                    s.press(buttons)
                self.assertIn("every --level", str(cm.exception))
                self.assertEqual(d.link.L.shim_presses(), 0, f"{lvl}: {buttons} pressed something")

    def test_session_levels(self):
        self.assertEqual(mcp_server.ScopeSession().level, "readonly")
        self.assertEqual(mcp_server.ScopeSession(allow_raw_shell=True).level, "unsafe")
        with self.assertRaises(ValueError):
            mcp_server.ScopeSession(level="root")
        with self.assertRaises(ValueError):
            devmod.shell_refusal("version", "root")


class TestDenyListAgainstFirmwareTable(unittest.TestCase):
    """The level tables name rows of the firmware's shell table; a name that
    is not there is a typo (it would allow or deny nothing). A row nobody has
    classified would be reachable at --level unsafe without a deny review,
    so every row must be read-only, bench, denied or in REVIEWED_UNSAFE_ONLY."""

    # Rows reviewed and left at --level unsafe: raw FPGA/SPI3/USART access,
    # stock-bringup probes, streams that can outlast the 5 s shell timeout,
    # reads with side effects (mem read has no address guard; flash diag
    # leaves the W25Q write-enable latch set), screen-capture plumbing.
    REVIEWED_UNSAFE_ONLY = {
        "?", "usbstat heal", "usart raw", "usart tx", "bench snapshot", "buzzer test",
        "mem read", "flash diag", "screen dump", "screen dumpbin", "screen shadow",
        "fpga cmd", "fpga frame", "fpga selftest", "fpga busrelease", "fpga busreacquire",
        "fpga configbb", "fpga stock diag", "fpga stock clear", "fpga stock set",
        "fpga stock preset", "fpga stock base2", "fpga stock state5", "fpga stock state6",
        "fpga stock prev", "fpga stock next", "fpga stock select", "fpga stock toggle",
        "fpga stock commit", "fpga stock consume", "fpga stock bridge fixed",
        "fpga stock bridge dynamic", "fpga stock reenter", "fpga wire words",
        "fpga wire entry", "fpga wire scope", "fpga usart", "meter hdr", "fpga rate",
        "fpga scope reinit", "fpga meter reinit", "fpga scope wake", "fpga scope acqmode",
        "fpga scope beat", "fpga scope entry", "fpga scope timing", "fpga scope trig",
        "meter autoscan", "meter auto", "meter probe-tail", "meter boot-sequence",
        "meter pc11-timing", "meter mux-arms", "meter mux-stream", "meter stream",
        "meter wave", "fpga acq", "spi3 xfer", "spi3 seq", "spi3 acqread",
        "spi3 opsweep", "spi3 gowin", "spi3 edgecap", "spi3 scopetest", "spi3 acqtest",
        "spi3 stock-readback", "spi3 h2txdiag", "spi3 h2verify", "spi3 probe",
    }

    @classmethod
    def setUpClass(cls):
        with open(os.path.join(DRIVERS, "usb_debug.c")) as f:
            cls.text = f.read()
        cls.rows = [m.group(1) for m in re.finditer(r'CMD_[AV]\(\s*"([^"]+)",\s*(\w+),', cls.text)
                    if m.group(2) != "fn"]
        assert len(cls.rows) > 90, f"table parse broke: {len(cls.rows)} rows"

    @staticmethod
    def wp(line, name):
        return line == name or line.startswith(name + " ")

    def test_names_exist(self):
        for name in devmod.READ_ONLY_SHELL + devmod.BENCH_SHELL:
            self.assertIn(name, self.rows, f"{name!r} is not a firmware shell command")
        for name in self.REVIEWED_UNSAFE_ONLY:
            self.assertIn(name, self.rows)
        for name in devmod.NEVER_SHELL_ROWS:
            row_under_it = any(self.wp(r, name) for r in self.rows)
            argument_form = any(self.wp(name, r) for r in self.rows) and name in self.text
            self.assertTrue(row_under_it or argument_form, f"{name!r} matches no firmware command")
        for name in devmod.NEVER_SHELL_RESERVED:
            self.assertFalse(any(self.wp(r, name) for r in self.rows),
                             f"the firmware now has {name!r}: review it and move it to NEVER_SHELL_ROWS")

    def test_every_row_is_classified_once(self):
        for r in self.rows:
            where = [c for c, hit in (
                ("readonly", r in devmod.READ_ONLY_SHELL),
                ("bench", any(self.wp(r, b) for b in devmod.BENCH_SHELL)),
                ("never", any(self.wp(r.lower(), n) for n in devmod.NEVER_SHELL)),
                ("reviewed-unsafe", r in self.REVIEWED_UNSAFE_ONLY)) if hit]
            self.assertEqual(len(where), 1,
                             f"firmware shell row {r!r} is classified {where or 'nowhere'}: add it to "
                             "BENCH_SHELL or NEVER_SHELL_ROWS (device.py) or REVIEWED_UNSAFE_ONLY")
            if where == ["bench"]:
                self.assertIn(r, devmod.BENCH_SHELL, f"{r!r} is bench only because a shorter name covers it")

    def test_no_bench_command_is_denied(self):
        for b in devmod.BENCH_SHELL:
            self.assertNotIn("never", devmod.shell_refusal(b, "bench") or "", b)
        for b in devmod.BENCH_ARG_RULES:
            self.assertIn(b, devmod.BENCH_SHELL)


class TestAllowRawShellAlias(unittest.TestCase):
    def parse(self, *argv):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            a = mcp_server.parse_args(list(argv))
        return a.level, err.getvalue()

    def test_levels(self):
        self.assertEqual(self.parse(), ("readonly", ""))
        for lvl in LEVELS:
            self.assertEqual(self.parse("--level", lvl), (lvl, ""))

    def test_alias_is_unsafe_with_a_warning(self):
        for argv in (["--allow-raw-shell"], ["--allow-raw-shell", "--level", "unsafe"]):
            lvl, err = self.parse(*argv)
            self.assertEqual(lvl, "unsafe")
            self.assertIn("deprecated", err)
            self.assertIn("--level unsafe", err)
            self.assertIn("POWER", err)

    def test_alias_conflicts_with_a_lower_level(self):
        for lvl in ("readonly", "bench"):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                mcp_server.parse_args(["--allow-raw-shell", "--level", lvl])
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            mcp_server.parse_args(["--level", "root"])

    def test_main_wires_the_level_and_keeps_stdout_clean(self):
        made = {}

        class FakeServer:
            def run(self):
                made["ran"] = True

        def build(sess):
            made["level"] = sess.level
            return FakeServer()
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(mcp_server, "build_server", build), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            self.assertEqual(mcp_server.main(["--allow-raw-shell"]), 0)
        self.assertEqual(made, {"level": "unsafe", "ran": True})
        self.assertEqual(out.getvalue(), "", "stdout is the MCP JSON-RPC stream")
        self.assertIn("deprecated", err.getvalue())


def _need_mcp(test):
    try:
        import mcp  # noqa: F401
    except ImportError:
        test.skipTest("mcp SDK not installed (needs Python >= 3.10)")


class TestLevelsOverMcp(unittest.TestCase):
    def setUp(self):
        _need_mcp(self)
        quiet = logging.getLogger("mcp")
        self.addCleanup(quiet.setLevel, quiet.level)
        quiet.setLevel(logging.CRITICAL)            # the SDK logs every ToolError

    def tools(self, lvl):
        s, _ = session(level=lvl)
        return {t.name: t for t in asyncio.run(mcp_server.build_server(s).list_tools())}

    def test_descriptions_and_hints_follow_the_level(self):
        for lvl in LEVELS:
            t = self.tools(lvl)
            sh, ann = t["scope_shell"], t["scope_shell"].annotations
            hint = lambda snake, camel: getattr(ann, snake, None) if hasattr(ann, snake) \
                else getattr(ann, camel)                    # mcp >= 2 renamed the fields
            self.assertEqual(hint("read_only_hint", "readOnlyHint"), lvl == "readonly", lvl)
            self.assertEqual(hint("destructive_hint", "destructiveHint"), lvl == "unsafe", lvl)
            self.assertIn(f"--level {lvl}", sh.description)
            for name in devmod.NEVER_SHELL:
                self.assertIn(name, sh.description, f"{lvl}: deny-list entry {name!r} not described")
            self.assertIn("POWER is refused at every server level",
                          " ".join(t["scope_press"].description.split()))
        bench = self.tools("bench")["scope_shell"].description
        self.assertIn("fpga scope timebase", bench)
        self.assertIn("spi3 opread takes only opcode 04 or 05", bench)

    def call(self, lvl, tool, args):
        """What the model receives: (is_error, text). With mcp >= 2 through a
        real in-process client; with 1.x via call_tool, where an error is an
        exception carrying the message."""
        s, d = session(level=lvl)
        server = mcp_server.build_server(s)
        try:
            from mcp import Client
        except ImportError:
            Client = None
        if Client is None:
            try:
                r = asyncio.run(server.call_tool(tool, args))
            except Exception as e:                  # noqa: BLE001
                return True, str(e), d
            return False, str(r), d

        async def go():
            async with Client(server) as c:
                return await c.call_tool(tool, args)
        r = asyncio.run(go())
        err = getattr(r, "is_error", None)
        if err is None:
            err = r.isError
        return bool(err), " ".join(getattr(c, "text", "") for c in r.content), d

    def test_every_refusal_reaches_the_model_with_its_reason(self):
        # A bare "Error executing tool ..." (no reason) is what a non-ToolError
        # exception would produce on mcp >= 2: each case checks the reason.
        cases = []
        for lvl in LEVELS:
            cases += [(lvl, "scope_shell", {"command": "fwapply"}, "never available over MCP"),
                      (lvl, "scope_shell", {"command": "mode startup meter"}, "never available over MCP"),
                      (lvl, "scope_shell", {"command": "fw\tapply"}, "every --level"),
                      (lvl, "scope_press", {"buttons": ["POWER"]}, "POWER is refused at every --level"),
                      (lvl, "scope_press", {"buttons": ["NOPE"]}, "unknown button")]
        cases += [("readonly", "scope_shell", {"command": "fpga scope timebase 1A"}, "--level bench"),
                  ("bench", "scope_shell", {"command": "spi3 seq 01 1A"}, "--level unsafe"),
                  ("bench", "scope_shell", {"command": "spi3 opread 01 4"}, "only opcode 04 or 05")]
        for lvl, tool, args, reason in cases:
            is_error, text, d = self.call(lvl, tool, args)
            self.assertTrue(is_error, (lvl, tool, args))
            self.assertIn(reason, text, (lvl, tool, args))
            self.assertEqual(bytes(d.link.shell_seen), b"", (lvl, args))
            self.assertEqual(d.link.L.shim_presses(), 0, (lvl, args))

    def test_allowed_commands_run(self):
        for lvl, cmd in (("readonly", "version"), ("bench", "fpga scope timebase 1A"),
                         ("unsafe", "spi3 seq 01 1A")):
            is_error, text, d = self.call(lvl, "scope_shell", {"command": cmd})
            self.assertFalse(is_error, (lvl, cmd, text))
            self.assertEqual(bytes(d.link.shell_seen), cmd.encode() + b"\r")


if __name__ == "__main__":
    unittest.main(verbosity=2)
