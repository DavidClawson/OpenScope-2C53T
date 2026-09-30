"""High-level API: one method per thing you can ask the instrument.

Stateless per request (remote_protocol.md §2.4 rule 4): each call sends one
frame and waits for its answer with a real timeout. If the port disappears
(reboot, IAP flash, or the firmware's CDC self-heal, issue #39) the call
reopens it once and retries, so a transient replug costs one request.
"""
from __future__ import annotations

import re
import time
import zlib
from dataclasses import dataclass
from typing import Callable, Optional, Tuple

from . import proto
from .link import NoDevice, SerialLink


class DeviceError(Exception):
    pass


class Nak(DeviceError):
    def __init__(self, cmd: int, code: int):
        self.cmd, self.code = cmd, code
        super().__init__(f"device refused 0x{cmd:02X}: {proto.ERRORS.get(code, hex(code))}")


class Timeout(DeviceError):
    pass


SCREEN_HDR = re.compile(
    rb"SCREENBIN x=(\d+) y=(\d+) w=(\d+) h=(\d+) format=indexed4 len=(\d+) crc32=([0-9A-F]{8})\r\n")
PROMPT = b"> "
RX_GAP_S = 0.06    # > ESP_RX_GAP_MS (firmware/src/drivers/esp_comm.h)


@dataclass
class Screen:
    x: int
    y: int
    w: int
    h: int
    indexed4: bytes     # 2 pixels per byte, high nibble first, rows padded to bytes


class Device:
    def __init__(self, link: SerialLink, timeout: float = 1.5,
                 sleep: Callable[[float], None] = time.sleep):
        self.link = link
        self.timeout = timeout
        self._sleep = sleep
        self.decoder = proto.Decoder()

    # ── lifecycle ─────────────────────────────────────────────────
    @classmethod
    def open(cls, port: Optional[str] = None, **kw) -> "Device":
        link = SerialLink(port)
        link.open()
        dev = cls(link, **kw)
        dev._settle()
        return dev

    def close(self) -> None:
        self.link.close()

    def _resync(self) -> None:
        """After an error: let the device's inter-byte gap timeout expire
        (ESP_RX_GAP_MS = 50 ms) so it has abandoned any half frame, then drop
        anything still in flight. Otherwise the next request's bytes could
        land inside a stale frame, or a late reply be taken for its answer."""
        self._sleep(RX_GAP_S)
        self._settle()

    def _settle(self) -> None:
        self.link.drain()
        self.decoder = proto.Decoder()

    # ── binary protocol ───────────────────────────────────────────
    def request(self, cmd: int, payload: bytes = b"", expect=(proto.RSP_ACK,)) -> proto.Frame:
        """Send one frame, return the matching reply. NAK raises Nak."""
        try:
            return self._request_once(cmd, payload, expect)
        except (Timeout, Nak):
            self._resync()
            raise
        except (OSError, IOError) as e:
            # Port vanished mid-request: reopen once (the device may have
            # self-healed its CDC endpoint, which looks like a replug).
            self.link.reopen()
            self._settle()
            try:
                return self._request_once(cmd, payload, expect)
            except (OSError, IOError):
                raise DeviceError(f"port lost twice: {e}") from None

    def _request_once(self, cmd: int, payload: bytes, expect) -> proto.Frame:
        wanted = set(expect) | {proto.RSP_NAK}
        self.link.write(proto.encode(cmd, payload))
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            chunk = self.link.read()
            if not chunk:
                continue
            for f in self.decoder.feed(chunk):
                if f.cmd == proto.RSP_NAK:
                    raise Nak(cmd, f.payload[0] if f.payload else 0xFF)
                if f.cmd in wanted:
                    return f
        raise Timeout(f"no reply to 0x{cmd:02X} within {self.timeout:.1f} s "
                      f"(held {self.decoder.pending()} B, text {len(self.decoder.text)} B)")

    def ping(self) -> str:
        return self.request(proto.CMD_PING, expect=(proto.RSP_DATA,)).payload.decode("ascii", "replace")

    def status(self) -> proto.Status:
        return proto.parse_status(self.request(proto.CMD_STATUS, expect=(proto.RSP_STATUS,)).payload)

    def press(self, button) -> None:
        self.request(proto.CMD_BUTTON, bytes([proto.button_id(button)]))

    # ── ASCII shell (same port; §3.2 keeps it alive next to the protocol) ──
    def shell(self, line: str, timeout: float = 3.0) -> str:
        """Run one shell command and return its output (echo and prompt stripped)."""
        if "\n" in line or "\r" in line:
            raise ValueError("one command per call")
        self.link.drain(quiet=0.05, max_wait=0.3)
        self.link.write(line.encode("ascii") + b"\r\n")
        out = self._read_until_prompt(timeout)
        text = out.decode("utf-8", "replace")
        if text.startswith(line):
            text = text[len(line):]
        return text.strip("\r\n")

    def _read_until_prompt(self, timeout: float) -> bytes:
        buf = bytearray()
        deadline = time.time() + timeout
        quiet_since = None
        while time.time() < deadline:
            chunk = self.link.read()
            if chunk:
                buf += chunk
                quiet_since = None
                continue
            if buf.endswith(PROMPT):
                if quiet_since is None:
                    quiet_since = time.time()
                elif time.time() - quiet_since > 0.1:
                    return bytes(buf[:-len(PROMPT)])
        raise Timeout(f"shell: no prompt within {timeout:.1f} s (got {bytes(buf[-60:])!r})")

    def screenshot(self, region: Optional[Tuple[int, int, int, int]] = None,
                   attempts: int = 3, timeout: float = 20.0) -> Screen:
        """The device's own framebuffer via `screen dumpbin`, CRC-checked.

        The dump is raw bytes (it can contain 0xAA), so it is read directly,
        never through the frame decoder. A live trace can change the screen
        during the dump and fail the device's CRC: retried `attempts` times.
        """
        cmd = "screen dumpbin" + ("" if region is None else " %d %d %d %d" % tuple(region))
        last = ""
        for _ in range(attempts):
            self.link.drain(quiet=0.05, max_wait=0.3)
            self.link.write(cmd.encode() + b"\r\n")
            buf = bytearray()
            t0 = time.time()
            m = None
            while time.time() - t0 < timeout and not m:
                buf += self.link.read()
                m = SCREEN_HDR.search(buf)
            if not m:
                last = "no SCREENBIN header"
                continue
            x, y, w, h, n = (int(m.group(i)) for i in range(1, 6))
            crc = int(m.group(6), 16)
            data = bytearray(buf[m.end():])
            while len(data) < n and time.time() - t0 < timeout:
                data += self.link.read(n - len(data))
            data = bytes(data[:n])
            self._settle()
            if len(data) == n and (zlib.crc32(data) & 0xFFFFFFFF) == crc:
                return Screen(x, y, w, h, data)
            last = f"CRC/length mismatch ({len(data)}/{n} B)"
        raise DeviceError(f"screenshot failed after {attempts} attempts: {last}")
