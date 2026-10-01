"""MCP server: lets an LLM agent (Claude Code, Claude Desktop, …) drive an
OpenScope 2C53T through the same API as the `openscope` CLI.

    uv run --no-project --python 3.12 --with mcp --with pyserial python -m openscope.mcp_server [--port P] [--level L]

Claude Code:
    claude mcp add openscope -- uv run --no-project --python 3.12 --with mcp \
        --with pyserial --directory <repo>/tools/openscope_host python -m openscope.mcp_server

Safety: the debug shell can erase flash (`fwapply`, `flash wtest`), park the
FPGA or desynchronise acquisition. Whoever starts the server picks how much
of it the agent may reach (device.py owns the tables):

    --level readonly   READ_ONLY_SHELL only (the default)
    --level bench      + BENCH_SHELL: scope/acquisition settings and reads
    --level unsafe     the whole shell, for bench work with a human watching

NEVER_SHELL (flash writes, boot changes, resets, direct GPIO/memory writes)
and the POWER button are refused at every level. `--allow-raw-shell` is a
deprecated alias for `--level unsafe`.
"""
from __future__ import annotations

import argparse
import sys
import threading
from typing import List, Optional

from . import proto
from .device import Device, DeviceError, Nak
from .link import NoDevice
from .screen import png_bytes

# What an agent may run at each level (device.py owns the lists).
from .device import (BENCH_SHELL, NEVER_SHELL, READ_ONLY_SHELL,  # noqa: E402
                     SHELL_LEVELS, shell_refusal)


class Refused(RuntimeError):
    """A safety refusal (level, deny-list, POWER): reaches the agent as a
    ToolError like every other expected outcome, never as a server fault."""


class ScopeSession:
    """One lazily opened Device shared by all tool calls, serialised by a lock."""

    def __init__(self, port: Optional[str] = None, level: str = "readonly",
                 opener=Device.open, *, allow_raw_shell: bool = False):
        if allow_raw_shell:                 # deprecated spelling of level="unsafe"
            level = "unsafe"
        if level not in SHELL_LEVELS:
            raise ValueError(f"unknown level {level!r} (one of {', '.join(SHELL_LEVELS)})")
        self.port = port
        self.level = level
        self._opener = opener
        self._dev: Optional[Device] = None
        self._lock = threading.Lock()

    def _device(self) -> Device:
        if self._dev is None:
            self._dev = self._opener(self.port)
        return self._dev

    def _call(self, fn):
        with self._lock:
            try:
                return fn(self._device())
            except NoDevice as e:
                self._dev = None
                raise RuntimeError(f"{e}. Is the scope plugged in and running OpenScope?") from None
            except Nak as e:
                raise RuntimeError(str(e)) from None
            except (DeviceError, proto.ProtocolError, OSError) as e:
                self._drop()        # the next call reopens instead of reusing a dead port
                raise RuntimeError(f"device error: {e}") from None

    def _drop(self) -> None:
        if self._dev is not None:
            try:
                self._dev.close()
            finally:
                self._dev = None

    # ── tools ────────────────────────────────────────────────────
    def info(self) -> dict:
        def run(dev: Device) -> dict:
            st = dev.status()
            return {
                "port": dev.link.port,
                "firmware": dev.ping(),
                "protocol_version": st.proto_version,
                "mode": st.mode_name,
                "battery_pct": st.battery_pct if st.battery_known else None,
                "battery_mv": st.battery_mv if st.battery_known else None,
                "charging": st.charging,
                "battery_critical": st.battery_critical,
                "capture_ready": st.capture_ready,
                "uptime_s": round(st.uptime_ms / 1000, 1),
                "usb_tx_stalls": st.usb_tx_stalls,
                "usb_self_heals": st.usb_heals,
            }
        return self._call(run)

    def meter(self) -> dict:
        def run(dev: Device) -> dict:
            m = dev.meter()
            return {
                "value": m.value, "unit": m.unit, "display": m.display, "result": m.result,
                "raw_bcd": m.raw_bcd, "decimal_pos": m.decimal_pos, "update_count": m.update_count,
                "submode": m.submode, "ac": m.ac, "autorange": m.autorange, "hold": m.hold,
                "note": "absolute accuracy is unverified on this unit; raw_bcd is what the meter chip reported",
            }
        return self._call(run)

    def press(self, buttons: List[str]) -> str:
        ids = [proto.button_id(b) for b in buttons]      # validate all before pressing any
        if proto.BUTTONS["POWER"] in ids:
            raise Refused("POWER is refused at every --level: it can switch the scope off and "
                          "end the session. If it is really needed, ask the human at the bench.")

        def run(dev: Device) -> str:
            done = []
            for b in buttons:
                try:
                    dev.press(b)
                except Exception as e:
                    # Say exactly what already acted, so a retry does not press it twice.
                    # Only a NAK proves this button did not act; a lost reply or a
                    # port lost after the write means it may well have.
                    already = ("pressed " + " ".join(done) + "; ") if done else "nothing pressed before; "
                    if isinstance(e, Nak) and e.cmd == proto.CMD_BUTTON:   # the press's own refusal
                        this = f"{b.upper()} NOT pressed ({e})"
                    else:
                        this = (f"{b.upper()} MAY OR MAY NOT have been pressed ({e}) - "
                                "take a screenshot before retrying")
                    rest = " ".join(x.upper() for x in buttons[len(done) + 1:]) or "-"
                    raise DeviceError(f"{already}{this}; remaining not sent: {rest}") from None
                done.append(b.upper())
            return "pressed " + " ".join(done)
        return self._call(run)

    def shell(self, command: str) -> str:
        why = shell_refusal(command, self.level)
        if why:
            raise Refused(why)
        cmd = " ".join(command.split())     # exactly the line shell_refusal() checked
        return self._call(lambda dev: dev.shell(cmd, timeout=5.0))

    def screenshot_png(self, scale: int = 2) -> bytes:
        if not 1 <= scale <= 4:
            raise RuntimeError("scale must be 1..4")
        s = self._call(lambda dev: dev.screenshot())
        self.last_screenshot_torn = s.torn
        return png_bytes(s.w, s.h, s.indexed4, scale)


def shell_tool_description(level: str) -> str:
    """scope_shell's description: what this server's level lets the agent run."""
    never = ", ".join(NEVER_SHELL)
    head = "Run one debug-shell command and return its text output. "
    if level == "readonly":
        body = (f"This server runs at --level readonly (the default): only "
                f"{', '.join(READ_ONLY_SHELL)} are allowed. ")
    elif level == "bench":
        body = (f"This server runs at --level bench: the read-only commands "
                f"({', '.join(READ_ONLY_SHELL)}) plus these bench commands and their "
                f"arguments: {', '.join(BENCH_SHELL)}. They change scope/acquisition "
                "settings or read; anything else needs --level unsafe. ")
    else:
        body = ("This server runs at --level unsafe: every shell command except the list "
                "below. Raw commands can desynchronise the FPGA or the acquisition; "
                "a human should be watching. ")
    return (head + body
            + "Levels (readonly < bench < unsafe) are chosen by whoever starts the server. "
            + f"Never available over MCP at any level (flash writes, boot changes, resets, "
              f"direct GPIO/memory writes): {never}.")


def build_server(session: ScopeSession):
    # The SDK resolves tool annotations in this module's namespace (this file
    # uses `from __future__ import annotations`), so Image must be global.
    global Image
    try:                                    # mcp >= 2: FastMCP was renamed MCPServer
        from mcp.server.mcpserver import Image, MCPServer as Server
    except ImportError:                     # mcp 1.x
        from mcp.server.fastmcp import FastMCP as Server, Image
    from mcp.types import ToolAnnotations
    try:                                    # mcp >= 2: only a ToolError's message reaches
        from mcp.server.mcpserver.exceptions import ToolError   # the model; any other
    except ImportError:                     # exception is reported as a bare
        ToolError = RuntimeError            # "Error executing tool <name>"

    read_only = ToolAnnotations(readOnlyHint=True, openWorldHint=False)
    mcp = Server("openscope")

    def expected(fn):
        """Device refusals (not in meter mode, queue full, no device), safety
        refusals (level, deny-list, POWER) and bad arguments (an unknown
        button name) are outcomes the agent must read and act on, not
        server faults."""
        def run(*a, **kw):
            try:
                return fn(*a, **kw)
            except (RuntimeError, ValueError) as e:
                raise ToolError(str(e)) from None
        return run

    @mcp.tool(annotations=read_only)
    def scope_info() -> dict:
        """Status of the OpenScope 2C53T: firmware build, current mode (scope/meter/
        siggen/settings), battery, whether real capture data exists yet, USB health."""
        return expected(lambda: session.info())()

    @mcp.tool(annotations=read_only)
    def scope_meter() -> dict:
        """Current multimeter reading (the scope must be in meter mode; the release
        coldtrace build measures DC volts). update_count increases ~4 times a second;
        call again to see whether the value moved. NOT_READY means no reading yet;
        UNSUPPORTED_IN_MODE means the scope is not in meter mode (press MENU to change)."""
        return expected(lambda: session.meter())()

    @mcp.tool(annotations=ToolAnnotations(readOnlyHint=False, destructiveHint=False,
                                          idempotentHint=False, openWorldHint=False))
    def scope_press(buttons: List[str]) -> str:
        """Press front-panel buttons in order, like a person would. Names: CH1 CH2 MOVE
        SELECT TRIGGER PRM AUTO SAVE MENU UP DOWN LEFT RIGHT OK. POWER is refused at
        every server level (it can switch the scope off and end the session). Take a
        screenshot afterwards to see the effect."""
        return expected(lambda: session.press(buttons))()

    @mcp.tool(annotations=read_only)
    def scope_screenshot(scale: int = 2) -> Image:
        """The scope's screen (320x240, transport CRC-checked, 16-colour palette so
        colours are approximate). scale 1..4 enlarges it. On a live trace the frame
        can be slightly torn (the trace moved during the ~1 s transfer)."""
        return Image(data=expected(session.screenshot_png)(scale), format="png")

    @mcp.tool(description=shell_tool_description(session.level),
              annotations=ToolAnnotations(readOnlyHint=session.level == "readonly",
                                          destructiveHint=session.level == "unsafe",
                                          idempotentHint=False, openWorldHint=False))
    def scope_shell(command: str) -> str:
        return expected(lambda: session.shell(command))()

    return mcp


def parse_args(argv=None) -> argparse.Namespace:
    """Arguments with `level` resolved. Warnings go to stderr: stdout is the
    MCP JSON-RPC stream."""
    ap = argparse.ArgumentParser(prog="openscope-mcp")
    ap.add_argument("--port")
    ap.add_argument("--level", choices=SHELL_LEVELS, default=None,
                    help="what scope_shell may run: readonly (default; status-type reads), "
                         "bench (+ scope/acquisition settings and measurement reads), "
                         "unsafe (whole shell, human watching). Flash writes, boot "
                         "changes, resets, GPIO writes and POWER are refused at every level.")
    ap.add_argument("--allow-raw-shell", action="store_true",
                    help="deprecated alias for --level unsafe")
    a = ap.parse_args(argv)
    if a.allow_raw_shell:
        if a.level not in (None, "unsafe"):
            ap.error(f"--allow-raw-shell means --level unsafe; it conflicts with --level {a.level}")
        sys.stderr.write("openscope-mcp: --allow-raw-shell is deprecated, use --level unsafe. "
                         "It no longer lifts the POWER refusal, and the never-over-MCP "
                         "commands (fwapply, flash writes, reboot, gpio set, ...) stay "
                         "refused at every level.\n")
        a.level = "unsafe"
    if a.level is None:
        a.level = "readonly"
    return a


def main(argv=None) -> int:
    a = parse_args(argv)
    build_server(ScopeSession(a.port, a.level)).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
