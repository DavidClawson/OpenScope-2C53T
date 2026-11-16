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
from .link import FlushFailed, NoDevice, SerialLink


class DeviceError(Exception):
    pass


class Nak(DeviceError):
    def __init__(self, cmd: int, code: int):
        self.cmd, self.code = cmd, code
        super().__init__(f"device refused 0x{cmd:02X}: {proto.ERRORS.get(code, hex(code))}")


class Timeout(DeviceError):
    pass


class _WriteFailed(DeviceError):
    """The request never left the host: re-sending it cannot act twice.
    A DeviceError, so a second one (after the reopen) reaches every caller
    as a normal device error rather than a private exception."""

    def __init__(self, msg: str = "write to the port failed"):
        super().__init__(msg)


# Queries: re-sending after a lost reply cannot change the instrument.
IDEMPOTENT = frozenset({proto.CMD_PING, proto.CMD_STATUS, proto.CMD_GET_METER})

# Shell commands that only read. The shell also has commands that reset the
# device (fwapply, fwswap, reboot bootloader) or write flash, so a shell line
# is re-sent after a replug only if it is on this list (the MCP server also
# uses it as its default allowlist).
READ_ONLY_SHELL = ("version", "status", "uptime", "usbstat", "fwstat", "help")


SCREEN_HDR = re.compile(
    rb"SCREENBIN x=(\d+) y=(\d+) w=(\d+) h=(\d+) format=indexed4 len=(\d+) crc32=([0-9A-F]{8})\r\n")
SCREEN_END = re.compile(rb"SCREENBIN END(?: crc32=([0-9A-F]{8}))?\r\n")
PROMPT = b"> "
RX_GAP_S = 0.06    # > ESP_RX_GAP_MS (firmware/src/drivers/esp_comm.h)


@dataclass
class Screen:
    x: int
    y: int
    w: int
    h: int
    indexed4: bytes     # 2 pixels per byte, high nibble first, rows padded to bytes
    torn: bool = False  # transport verified, but the screen changed during the capture


class Device:
    def __init__(self, link: SerialLink, timeout: float = 1.5,
                 sleep: Callable[[float], None] = time.sleep):
        self.link = link
        self.timeout = timeout
        self._sleep = sleep
        self.decoder = proto.Decoder()

    # ── lifecycle ─────────────────────────────────────────────────
    @classmethod
    def open(cls, port: Optional[str] = None, check_version: bool = True, **kw) -> "Device":
        link = SerialLink(port)
        link.open()
        dev = cls(link, **kw)
        dev._settle()
        if check_version:
            try:
                dev._check_version()
            except Exception:
                dev.close()
                raise
        return dev

    def _check_version(self) -> None:
        """§3.7: refuse to talk to an unknown protocol major before sending
        anything that acts (parse_status raises ProtocolError on a mismatch).
        Runs on open and again after every reopen: the reappearing port may be
        a different firmware (e.g. right after an IAP flash).

        Deliberately NOT through request(): its replug handling reopens, and
        reopening runs this check, so a port that keeps vanishing would
        recurse without bound. A failure here is final for this call."""
        try:
            frame = self._request_once(proto.CMD_STATUS, b"", (proto.RSP_STATUS,))
        except (Timeout, Nak):
            self._resync()
            raise
        except (OSError, _WriteFailed) as e:
            raise DeviceError(f"port lost again right after (re)opening: {e}") from None
        self.proto_version = proto.parse_status(frame.payload).proto_version

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
        """Send one frame, return the matching reply. NAK raises Nak.

        If the port vanishes (reboot, IAP flash, the firmware's CDC self-heal
        from #39 — all look like a replug) it is reopened once. The request is
        re-sent only when that cannot act twice: the write itself failed, or
        the command is a pure query. A BUTTON whose write went through but
        whose reply was lost is NOT re-sent (it may already have been
        pressed); that raises DeviceError instead.
        """
        if cmd != proto.CMD_STATUS:
            self._ensure_verified()
        try:
            return self._request_once(cmd, payload, expect)
        except (Timeout, Nak):
            self._resync()
            raise
        except _WriteFailed as e:
            self._reopen()
            return self._retry(cmd, payload, expect, e.__cause__)
        except OSError as e:
            if cmd not in IDEMPOTENT:
                # The frame left the host: whatever happens while reopening
                # (a NAK or timeout of the version check, no device) must not
                # replace "it may already have acted" with an error that reads
                # like this command's own refusal.
                note = ""
                try:
                    self._reopen()
                except Exception as re:
                    note = f"; reopen then failed: {re}"
                raise DeviceError(f"port lost after 0x{cmd:02X} was sent; not re-sending "
                                  f"(it may already have acted): {e}{note}") from None
            self._reopen()
            return self._retry(cmd, payload, expect, e)

    def _retry(self, cmd, payload, expect, first) -> proto.Frame:
        try:
            return self._request_once(cmd, payload, expect)
        except (Timeout, Nak):
            self._resync()          # same rule as the first attempt: nothing stale survives
            raise
        except (OSError, _WriteFailed):
            raise DeviceError(f"port lost twice: {first}") from None

    def _reopen(self) -> None:
        """Reopen after the port vanished. The port that reappears is
        UNVERIFIED until its protocol major checks out; any failure of that
        check is final for this call and reported as a failed reconnect (a
        DeviceError, never a Nak that could read as the command's own
        refusal), and the next request re-checks before it sends anything."""
        checked = getattr(self, "proto_version", None) is not None
        self._verified = not checked
        self.link.reopen()
        self._settle()
        if checked:
            try:
                self._check_version()
            except (Timeout, Nak) as e:
                raise DeviceError(f"version check after reconnect failed: {e}") from None
            self._verified = True

    def _ensure_verified(self) -> None:
        if not getattr(self, "_verified", True):
            try:
                self._check_version()
            except (Timeout, Nak) as e:
                raise DeviceError(f"device not verified since reconnect: {e}") from None
            self._verified = True

    def _write(self, data: bytes) -> None:
        try:
            self.link.write(data)
        except FlushFailed:
            raise                   # possibly delivered: the post-write path decides
        except OSError as e:
            raise _WriteFailed() from e

    def _request_once(self, cmd: int, payload: bytes, expect) -> proto.Frame:
        wanted = set(expect) | {proto.RSP_NAK}
        self._write(proto.encode(cmd, payload))
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

    def meter(self) -> proto.MeterReading:
        """One coherent multimeter reading (raises Nak NOT_READY before the first)."""
        return proto.parse_meter(self.request(proto.CMD_GET_METER, expect=(proto.RSP_METER_FRAME,)).payload)

    def press(self, button) -> None:
        self.request(proto.CMD_BUTTON, bytes([proto.button_id(button)]))

    # ── ASCII shell (same port; §3.2 keeps it alive next to the protocol) ──
    def shell(self, line: str, timeout: float = 3.0) -> str:
        """Run one shell command and return its output (echo and prompt stripped).

        Terminated with CR only: the firmware treats CR and LF each as a line
        end, so CRLF would print a second, empty prompt. After a replug the
        line is re-sent only if its write failed or it is in READ_ONLY_SHELL;
        e.g. `fwswap b` drops the port by design and must not run twice."""
        if "\n" in line or "\r" in line:
            raise ValueError("one command per call")
        self._ensure_verified()
        try:
            return self._shell_once(line, timeout)
        except _WriteFailed as e:
            self._reopen()
            return self._shell_retry(line, timeout, e.__cause__)
        except OSError as e:
            if line.strip() not in READ_ONLY_SHELL:
                note = ""
                try:
                    self._reopen()
                except Exception as re:
                    note = f"; reopen then failed: {re}"
                raise DeviceError(f"port lost after '{line}' was sent; not re-sending "
                                  f"(it may already have acted): {e}{note}") from None
            self._reopen()
            return self._shell_retry(line, timeout, e)

    def _shell_retry(self, line, timeout, first) -> str:
        try:
            return self._shell_once(line, timeout)
        except (OSError, _WriteFailed):
            raise DeviceError(f"port lost twice: {first}") from None

    def _shell_once(self, line: str, timeout: float) -> str:
        self.link.drain(quiet=0.05, max_wait=0.3)
        self._write(line.encode("ascii") + b"\r")
        text = self._read_until_prompt(timeout).decode("utf-8", "replace")
        if text.startswith(line):
            text = text[len(line):]
        text = text.strip("\r\n")
        while text.endswith("> ") or text.endswith(">"):          # any extra empty prompts
            text = text[:text.rfind(">")].rstrip("\r\n ")
        return text

    def _with_reopen(self, fn):
        """For screenshot() only: `screen dumpbin` is read-only, so it may be
        re-run after a replug."""
        try:
            return fn()
        except (OSError, _WriteFailed) as e:
            self._reopen()
            try:
                return fn()
            except (OSError, _WriteFailed):
                raise DeviceError(f"port lost twice: {e}") from None

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
        self._ensure_verified()     # same rule as every other request
        return self._with_reopen(lambda: self._screenshot_once(region, attempts, timeout))

    def _screenshot_once(self, region, attempts: int, timeout: float) -> Screen:
        """Prefer a frame whose header CRC matches (consistent). On a live
        screen that may never happen: then accept a frame whose TRAILER CRC
        (firmware: CRC of the bytes actually sent) matches — transport intact,
        frame possibly torn — and mark it torn instead of failing. Firmware
        without the trailer behaves as before (header CRC or failure)."""
        cmd = "screen dumpbin" + ("" if region is None else " %d %d %d %d" % tuple(region))
        last = ""
        torn_frame = None
        for _ in range(attempts):
            self.link.drain(quiet=0.05, max_wait=0.3)
            self._write(cmd.encode() + b"\r")
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
            crc_hdr = int(m.group(6), 16)
            rest = bytearray(buf[m.end():])
            while len(rest) < n and time.time() - t0 < timeout:
                rest += self.link.read(n - len(rest))
            data = bytes(rest[:n])
            tail = bytearray(rest[n:])
            end = SCREEN_END.search(tail)
            while not end and time.time() - t0 < min(timeout, 3.0):
                tail += self.link.read()
                end = SCREEN_END.search(tail)
            self._settle()
            if len(data) != n:
                last = f"short capture ({len(data)}/{n} B)"
                continue
            got = zlib.crc32(data) & 0xFFFFFFFF
            if got == crc_hdr:
                return Screen(x, y, w, h, data)
            if end and end.group(1) and got == int(end.group(1), 16):
                torn_frame = Screen(x, y, w, h, data, torn=True)
                last = "screen changed during capture"
                continue
            last = f"CRC mismatch (transport): got {got:08X}"
        if torn_frame is not None:
            return torn_frame
        raise DeviceError(f"screenshot failed after {attempts} attempts: {last}")
