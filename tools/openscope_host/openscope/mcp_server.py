"""MCP server: lets an LLM agent (Claude Code, Claude Desktop, …) drive an
OpenScope 2C53T through the same API as the `openscope` CLI.

    uv run --no-project --python 3.12 --with mcp --with pyserial python -m openscope.mcp_server [--port P] [--allow-raw-shell]

Claude Code:
    claude mcp add openscope -- uv run --no-project --python 3.12 --with mcp \
        --with pyserial --directory <repo>/tools/openscope_host python -m openscope.mcp_server

Safety: the debug shell can erase flash (`fwapply`, `flash wtest`), park the
FPGA or desynchronise acquisition. By default only the read-only commands in
READ_ONLY_SHELL are reachable from the agent; `--allow-raw-shell` lifts that
for bench work where a human is watching.
"""
from __future__ import annotations

import argparse
import threading
from typing import List, Optional

from . import proto
from .device import Device, DeviceError, Nak
from .link import NoDevice
from .screen import png_bytes

# Exact commands (or prefixes ending in a space) an agent may run by default.
READ_ONLY_SHELL = (
    "version", "status", "uptime", "usbstat", "fwstat", "help",
)


class ScopeSession:
    """One lazily opened Device shared by all tool calls, serialised by a lock."""

    def __init__(self, port: Optional[str] = None, allow_raw_shell: bool = False,
                 opener=Device.open):
        self.port = port
        self.allow_raw_shell = allow_raw_shell
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
                "battery_pct": st.battery_pct,
                "battery_mv": st.battery_mv,
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
        if proto.BUTTONS["POWER"] in ids and not self.allow_raw_shell:
            raise RuntimeError("POWER is refused by default (it can switch the scope off "
                               "and end the session); start the server with --allow-raw-shell")

        def run(dev: Device) -> str:
            for b in buttons:
                dev.press(b)
            return "pressed " + " ".join(b.upper() for b in buttons)
        return self._call(run)

    def shell(self, command: str) -> str:
        cmd = command.strip()
        if not self.allow_raw_shell and cmd not in READ_ONLY_SHELL:
            raise RuntimeError(f"'{cmd}' is not in the read-only allowlist "
                               f"({', '.join(READ_ONLY_SHELL)}); the raw shell can erase flash "
                               "or desynchronise the FPGA. Start with --allow-raw-shell to lift this.")
        return self._call(lambda dev: dev.shell(cmd, timeout=5.0))

    def screenshot_png(self, scale: int = 2) -> bytes:
        if not 1 <= scale <= 4:
            raise RuntimeError("scale must be 1..4")
        s = self._call(lambda dev: dev.screenshot())
        return png_bytes(s.w, s.h, s.indexed4, scale)


def build_server(session: ScopeSession):
    # The SDK resolves tool annotations in this module's namespace (this file
    # uses `from __future__ import annotations`), so Image must be global.
    global Image
    try:                                    # mcp >= 2: FastMCP was renamed MCPServer
        from mcp.server.mcpserver import Image, MCPServer as Server
    except ImportError:                     # mcp 1.x
        from mcp.server.fastmcp import FastMCP as Server, Image
    from mcp.types import ToolAnnotations

    read_only = ToolAnnotations(readOnlyHint=True, openWorldHint=False)
    mcp = Server("openscope")

    @mcp.tool(annotations=read_only)
    def scope_info() -> dict:
        """Status of the OpenScope 2C53T: firmware build, current mode (scope/meter/
        siggen/settings), battery, whether real capture data exists yet, USB health."""
        return session.info()

    @mcp.tool(annotations=read_only)
    def scope_meter() -> dict:
        """Current multimeter reading (the scope must be in meter mode; the release
        coldtrace build measures DC volts). update_count increases ~4 times a second;
        call again to see whether the value moved. NOT_READY means no reading yet."""
        return session.meter()

    @mcp.tool(annotations=ToolAnnotations(readOnlyHint=False, destructiveHint=False,
                                          idempotentHint=False, openWorldHint=False))
    def scope_press(buttons: List[str]) -> str:
        """Press front-panel buttons in order, like a person would. Names: CH1 CH2 MOVE
        SELECT TRIGGER PRM AUTO SAVE MENU UP DOWN LEFT RIGHT OK (POWER needs
        --allow-raw-shell). Take a screenshot afterwards to see the effect."""
        return session.press(buttons)

    @mcp.tool(annotations=read_only)
    def scope_screenshot(scale: int = 2) -> Image:
        """The scope's screen (320x240, CRC-checked, 16-colour palette so colours are
        approximate). scale 1..4 enlarges it."""
        return Image(data=session.screenshot_png(scale), format="png")

    @mcp.tool(annotations=ToolAnnotations(readOnlyHint=not session.allow_raw_shell,
                                          openWorldHint=False))
    def scope_shell(command: str) -> str:
        """Run one debug-shell command and return its text output. By default only
        read-only commands are allowed: version, status, uptime, usbstat, fwstat, help."""
        return session.shell(command)

    return mcp


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="openscope-mcp")
    ap.add_argument("--port")
    ap.add_argument("--allow-raw-shell", action="store_true",
                    help="expose every shell command and POWER (bench use, human watching)")
    a = ap.parse_args(argv)
    build_server(ScopeSession(a.port, a.allow_raw_shell)).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
