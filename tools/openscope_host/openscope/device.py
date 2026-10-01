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


# ── Shell levels for the MCP server (mcp_server.py --level) ──────────────
# Names are rows of the firmware's shell table (shell_cmds[] in
# firmware/src/drivers/usb_debug.c). A name matches a line it is a
# whole-word prefix of, as in the firmware's dispatcher, so "trig" never
# matches "trig2 ..." and "fwcrumb clear" is not "fwcrumb".
# tests/test_mcp_server.py checks every name below against that table and
# fails when a row is added that nobody has classified.
SHELL_LEVELS = ("readonly", "bench", "unsafe")

# --level bench = READ_ONLY_SHELL plus these: the scope/acquisition setters
# and measurement reads the experiment scripts use (scripts/bench.py,
# scripts/exp*.py). They change volatile state only (RAM, FPGA registers,
# the trigger DAC) or read; the one flash-writing form of `mode`
# (`mode startup ...`) is in NEVER_SHELL, which is checked first.
BENCH_SHELL = (
    # scope settings, through the same entry points the UI uses
    "fpga scope timebase", "fpga scope range", "fpga scope center", "fpga scope vdiv",
    "fpga scope trigmode", "fpga scope level", "fpga scope edge", "fpga scope hpos",
    "fpga scope softtrig", "fpga scope graticule", "trig", "trig2", "mode",
    # acquisition knobs of the seam/poll experiments (EXP-29..54)
    "fpga postedge", "fpga pollgap", "fpga autowait", "fpga rearmwait", "fpga rearm",
    "fpga acqgate", "fpga acqbr", "fpga pairgap", "fpga unrotate", "fpga edgefilter",
    "fpga holdread", "fpga diag clear",
    # one FPGA read window, limited to the channel-read opcodes (BENCH_ARG_RULES)
    "spi3 opread",
    # measurements and reads
    "fpga scope measure", "fpga scope freq", "fpga scope cal",
    "spi3 read", "spi3 frame", "gpio read", "gpio scan",
    "meter dump", "meter trace", "meter frontend", "meter adc-snapshot",
    "cal status", "settings", "ui dump", "flash jedec", "flash read", "flash dump",
)

# At --level bench these commands also need their arguments (everything
# after the name) to match: name -> (pattern, what it allows, why).
BENCH_ARG_RULES = {
    # `spi3 opread <op> [len [dump]]` clocks 0xFF filler under ANY opcode.
    # On the configured design 01/02/06/07/08 are register WRITES the filler
    # smashes (01 is the run register; `spi3 opsweep` skips them for that
    # reason) and config-port opcodes (11, 41, 15, 3A, 3B, 3C) desynchronise
    # it; 04/05 are the channel reads. Other opcodes need --level unsafe.
    "spi3 opread": (
        re.compile(r"(?:0[xX])?0?[45](?: (?:[0-9]{1,4}|0[xX][0-9a-fA-F]{1,4})(?: dump)?)?"),
        "only opcode 04 or 05, as `spi3 opread 04|05 [len [dump]]`",
        "other opcodes write FPGA registers under the 0xFF filler or hit the config port"),
}

# scope_shell never sends these, at any level: they erase or write flash,
# change what the scope boots, drive GPIO pins directly or reset it.
# Name -> why. This covers the debug shell only: front-panel presses
# (scope_press) are not filtered by level and can reach Settings > Startup
# on Boot (an MCU flash write) and Settings > Firmware Update (reboot into
# DFU); see mcp_server.py.
NEVER_SHELL_WHAT = "flash writes, boot changes, resets, direct GPIO/memory writes"
NEVER_SHELL_ROWS = {
    "fwload": "stages a firmware image into the W25Q cache (erases and writes it)",
    "fwapply": "erases and reprograms the MCU application flash, then resets",
    "fwswap": "installs a cached image over the MCU application flash, then resets",
    "fwcrumb clear": "erases the firmware-install trail (backup registers)",
    "cal backup": "erases and rewrites the factory-calibration backup in the W25Q",
    "cal restore": "rewrites the MCU factory-calibration page (0x08006000)",
    "flash wtest": "erases and writes a W25Q sector",
    "mem write": "writes any address (flash controller, GPIO, clocks, ...)",
    "mode startup": "erases and rewrites the MCU flash sector that sets what the scope boots into",
    "reboot": "reboots the scope (`reboot bootloader` = into the USB updater)",
    "gpio set": "drives a GPIO pin",
    "gpio mode": "changes a GPIO pin's direction",
    "bench restore": "rewrites the frontend/FPGA pins' modes and levels",
    "spi3 armtest": "pulses the FPGA run pin (PB11 or PC6) directly",
    "fpga dbgclk": "reconfigures PC6 as an output and clocks it",
    "fpga dbgarm": "reconfigures PB11 as an output and drives it",
    "fpga reinit": "replays the FPGA bitstream handshake (`rl` = Gowin RELOAD) and drives "
                   "PB11; its <a-e><pin> option makes any pin a push-pull output and pulses "
                   "it LOW for 10 ms, and `c9` is PC9, the power hold: the scope switches off",
}
# No such rows today. Denied so that a future command with one of these
# names cannot reach --level unsafe before anyone has reviewed it.
NEVER_SHELL_RESERVED = {
    "flash erase": "would erase flash",
    "flash write": "would write flash",
    "iap": "would enter the IAP updater",
    "dfu": "would enter DFU",
    "reset": "would reset the scope",
}
NEVER_SHELL = {**NEVER_SHELL_ROWS, **NEVER_SHELL_RESERVED}

SHELL_LINE_MAX = 127    # firmware CMD_BUF_SIZE - 1; it silently drops the rest


def _word_prefix(line: str, name: str) -> bool:
    return line == name or line.startswith(name + " ")


def shell_refusal(line: str, level: str) -> Optional[str]:
    """Why the MCP server must not send `line` at `level`, or None if it may.

    What is checked is what runs: the caller sends " ".join(line.split()).
    A control character is refused outright because the firmware's line
    editor would rewrite the line after this check (backspace/DEL delete,
    tabs are dropped: "fw<TAB>apply" runs fwapply). The deny-list is checked
    before any allowlist, case-insensitively, so no level can lift it."""
    if level not in SHELL_LEVELS:
        raise ValueError(f"unknown shell level {level!r} (one of {', '.join(SHELL_LEVELS)})")
    raw = line.strip()
    if any(not (" " <= ch <= "~") for ch in raw):
        return (f"{raw!r} is refused at every --level: it contains a control or non-ASCII "
                "character, and the firmware's line editor applies backspace/DEL and drops "
                "tabs, so what ran could differ from what was checked. "
                "Send one plain printable-ASCII command.")
    if len(raw) > SHELL_LINE_MAX:
        return (f"refused at every --level: the command is {len(raw)} characters; the "
                f"firmware's line buffer keeps {SHELL_LINE_MAX} and silently drops the rest.")
    cmd = " ".join(raw.split())
    low = cmd.lower()
    for name, why in NEVER_SHELL.items():
        if _word_prefix(low, name):
            return (f"scope_shell never sends '{cmd}', at any --level "
                    f"({', '.join(SHELL_LEVELS)}): `{name}` {why}. This shell command is "
                    "never available over MCP; if it is really needed, ask the human at "
                    "the bench to run it.")
    if cmd in READ_ONLY_SHELL or level == "unsafe":
        return None
    bench, limit = _bench_verdict(cmd)
    if level == "bench":
        if bench:
            return None
        if limit:
            return (f"'{cmd}' is not allowed at --level bench, where {limit}. This form "
                    "needs --level unsafe, which only whoever starts the MCP server can choose.")
        return (f"'{cmd}' is not allowed at --level bench (bench adds to the read-only "
                f"commands: {', '.join(BENCH_SHELL)}). It needs --level unsafe, which only "
                "whoever starts the MCP server can choose.")
    return (f"'{cmd}' is not allowed at --level readonly, the default (allowed: "
            f"{', '.join(READ_ONLY_SHELL)}). "
            + ("It is a bench command: it needs --level bench or unsafe. " if bench
               else f"At --level bench {limit}, so this form needs --level unsafe. " if limit
               else "It needs --level unsafe. ")
            + "Only whoever starts the MCP server can choose the level: the raw shell "
            "can erase flash or desynchronise the FPGA.")


def _bench_verdict(cmd: str) -> Tuple[bool, str]:
    """(allowed at bench, why not if a BENCH_ARG_RULES limit refused it)."""
    names = [n for n in BENCH_SHELL if _word_prefix(cmd, n)]
    if not names:
        return False, ""
    name = max(names, key=len)          # the row the firmware would dispatch to
    rule = BENCH_ARG_RULES.get(name)
    if rule and not rule[0].fullmatch(cmd[len(name):].strip()):
        return False, f"`{name}` takes {rule[1]} ({rule[2]})"
    return True, ""


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
