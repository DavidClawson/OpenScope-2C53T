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

# Exact commands an agent may run by default (device.py owns the list).
from .device import READ_ONLY_SHELL  # noqa: E402


class ScopeSession:
    """One lazily opened Device shared by all tool calls, serialised by a lock."""

    def __init__(self, port: Optional[str] = None, allow_raw_shell: bool = False,
                 opener=Device.open):
        self.port = port
        self.allow_raw_shell = allow_raw_shell
        self._opener = opener
        self._dev: Optional[Device] = None
        self._lock = threading.Lock()
        self._last_frame_id: Optional[int] = None

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

    def waveform(self, channels: Optional[List[int]] = None) -> dict:
        """One capture as header + raw samples + a compact summary per channel.
        Refusals are explained, never papered over: no capture yet, wrong
        mode, unmeasured volts or rate all say what is missing and why."""
        try:
            mask = proto.channel_mask(channels if channels else [1])
        except ValueError as e:
            raise RuntimeError(str(e)) from None

        def run(dev: Device) -> dict:
            try:
                waves = dev.waveform(mask)
            except Nak as e:
                name = proto.ERRORS.get(e.code, "")
                raise RuntimeError(f"{e}: {proto.NAK_HINTS.get(name, 'refused')}") from None
            fid = waves[0].frame_id
            notes = [
                "samples are raw unsigned 8-bit ADC counts (0..255); calibrated=false: this unit has "
                "no calibration, so there are no absolute volts",
                f"samples[0:{waves[0].head_skip}] are a known record-head defect; the summary uses "
                "the rest",
            ]
            if waves[0].time_ordered:
                notes.append("record is time-ordered: the hardware trigger is at index 512")
            else:
                notes.append("record is not time-ordered: the trigger position in it is unknown")
            if fid == self._last_frame_id:
                notes.append("same frame_id as the previous call: no new capture since then "
                             "(acquisition stopped or held: RUN/STOP, SINGLE, or NORMAL without a trigger)")
            self._last_frame_id = fid
            return {
                "frame_id": fid,
                "channels": [dict(w.header(), summary=w.summary(), samples=list(w.samples))
                             for w in waves],
                "notes": notes,
            }
        return self._call(run)

    def press(self, buttons: List[str]) -> str:
        ids = [proto.button_id(b) for b in buttons]      # validate all before pressing any
        if proto.BUTTONS["POWER"] in ids and not self.allow_raw_shell:
            raise RuntimeError("POWER is refused by default (it can switch the scope off "
                               "and end the session); start the server with --allow-raw-shell")

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
        self.last_screenshot_torn = s.torn
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
    try:                                    # mcp >= 2: only a ToolError's message reaches
        from mcp.server.mcpserver.exceptions import ToolError   # the model; any other
    except ImportError:                     # exception is reported as a bare
        ToolError = RuntimeError            # "Error executing tool <name>"

    read_only = ToolAnnotations(readOnlyHint=True, openWorldHint=False)
    mcp = Server("openscope")

    def expected(fn):
        """Device refusals (not in meter mode, queue full, no device, allowlist)
        are outcomes the agent must read and act on, not server faults."""
        def run(*a, **kw):
            try:
                return fn(*a, **kw)
            except RuntimeError as e:
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

    @mcp.tool(annotations=read_only)
    def scope_waveform(channels: Optional[List[int]] = None) -> dict:
        """One captured waveform per channel ([1], [2] or [1, 2]; default [1]), both from
        the same capture (same frame_id). Each channel: header (sample rate and volts/div
        only where bench-measured, else null with the reason), a summary (min/max/mean
        counts, approximate Vpp, period and frequency estimate - computed after the
        record-head defect), and the 1024 raw ADC-count samples. Call again: a new
        frame_id means a new capture. NO_CAPTURE_DATA means the scope has no real capture
        (it never sends its demo trace); UNSUPPORTED_IN_MODE means it is not in scope mode."""
        return expected(lambda: session.waveform(channels))()

    @mcp.tool(annotations=ToolAnnotations(readOnlyHint=False, destructiveHint=False,
                                          idempotentHint=False, openWorldHint=False))
    def scope_press(buttons: List[str]) -> str:
        """Press front-panel buttons in order, like a person would. Names: CH1 CH2 MOVE
        SELECT TRIGGER PRM AUTO SAVE MENU UP DOWN LEFT RIGHT OK (POWER needs
        --allow-raw-shell). Take a screenshot afterwards to see the effect."""
        return expected(lambda: session.press(buttons))()

    @mcp.tool(annotations=read_only)
    def scope_screenshot(scale: int = 2) -> Image:
        """The scope's screen (320x240, transport CRC-checked, 16-colour palette so
        colours are approximate). scale 1..4 enlarges it. On a live trace the frame
        can be slightly torn (the trace moved during the ~1 s transfer)."""
        return Image(data=expected(session.screenshot_png)(scale), format="png")

    @mcp.tool(annotations=ToolAnnotations(readOnlyHint=not session.allow_raw_shell,
                                          openWorldHint=False))
    def scope_shell(command: str) -> str:
        """Run one debug-shell command and return its text output. By default only
        read-only commands are allowed: version, status, uptime, usbstat, fwstat, help."""
        return expected(lambda: session.shell(command))()

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
