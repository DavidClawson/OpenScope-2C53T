#!/usr/bin/env python3
"""bench.py — the shared measurement library for the OpenScope 2C53T bench.

WHY THIS FILE EXISTS
--------------------
Every bench experiment in this project used to re-implement the same serial
helper inline in a throwaway script.  That is how the measurement bugs got in,
and this project's characteristic failure is not a bad hypothesis — it is an
instrument that returns a **stable, plausible, wrong number** because it could
not detect what it claimed to.  A stable wrong number is indistinguishable from
a right one.  The documented casualties:

  * SSPI status reads at ``/2`` — garbage, believed for weeks.
  * PB4/MISO left floating — every status read taken on a line with no defined
    idle level.
  * Exp F compared output LEVELS, which cannot tell a pin driven LOW from a pin
    left FLOATING, and so "excluded" a class it could not see.
  * A fixed-bin DFT that reported **1.44** where a peak search found **22.8**.
  * ``fpga scope range <n> 0`` silently addressing BOTH channels, because the
    channel argument is 1-based and 0 means "no channel given".
  * Reads that discarded THREE header bytes and kept 1023 samples, when stock
    discards TWO and keeps 1024 — every sample array shifted one byte late.
    Independently confirmed on a second unit (commit ``ce22b49``).

So this module is written to make the *controls* easy and the *uncontrolled
negatives* awkward.  Three structural rules, not advisory ones:

1.  :class:`Result` has **no settable verdict**.  The verdict is computed from
    the controls attached to it.  A NEGATIVE with no passing control renders as
    **VOID**, and there is no field you can set to say otherwise.
2.  :func:`band` refuses a zero-width window, because a single fixed bin is the
    detector that reported 1.44 for a 22.8 tone.  :func:`fixed_bin` exists only
    to raise and tell you the story.
3.  Parsing is strict.  A hex dump with a gap in its offsets, or a short read,
    raises instead of silently returning a truncated array.

Dependencies: python3, numpy, pyserial.  Imports cleanly with no device
attached; nothing touches a serial port until you construct a device.

Self-test (no hardware needed)::

    python3 scripts/bench.py --selftest

Usage and two worked examples: ``scripts/README-bench.md``.
"""

from __future__ import annotations

import argparse
import glob
import re
import sys
import os
import time
from dataclasses import dataclass, field
from typing import Callable, Iterable, Optional, Sequence

import numpy as np

__all__ = [
    # errors
    "BenchError", "PromptTimeout", "ShortReadError",
    # transports
    "Transport", "SerialTransport", "ScriptedTransport",
    # devices
    "Scope", "Siggen", "JDS6600", "SiggenStatus", "PwmStatus", "OpreadStats",
    # signal sources (the --source convention) and the dry-run bench
    "SignalSource", "Esp32Source", "KodeDotSource", "KodeDotStatus", "ManualSource",
    "SimBench", "SOURCE_KINDS", "WAVEFORMS", "SCOPE_USB_ID", "ESPRESSIF_VID",
    "find_port", "add_source_args", "open_scope", "open_source", "open_bench",
    "parse_kodedot_reply", "parse_hz_text", "parse_amplitude_text",
    "vpp_from_reading", "expected_span_counts",
    # parsing
    "parse_dump", "parse_opread_stats", "parse_siggen_status", "parse_pwm_status",
    # analysis
    "spectrum", "peaks", "band", "band_peak", "window_for", "bin_of", "fixed_bin",
    # statistics
    "PairedStats", "paired_difference", "paired_control", "paired_experiment",
    # evidence
    "Verdict", "Control", "Result", "Experiment",
    # constants
    "STOCK_HEADER_DROP", "STOCK_WINDOW_BYTES", "STOCK_SAMPLES",
]


# ---------------------------------------------------------------------------
# Errors
# ---------------------------------------------------------------------------

class BenchError(RuntimeError):
    """Any bench-instrument fault.  Raised in preference to returning a
    plausible-looking value, which is this project's documented failure mode."""


class PromptTimeout(BenchError):
    """The device shell did not return its ``>`` prompt inside the timeout.

    This is raised rather than returning the partial buffer: a truncated hex
    dump parses perfectly well and yields a shorter, wrong array."""


class ShortReadError(BenchError):
    """A dump returned fewer bytes than requested.

    The old inline scripts handled this with ``if len(a) < 1000: continue``,
    open-coded per script and easy to forget.  Here it is unconditional."""


# ---------------------------------------------------------------------------
# Framing constants — see commit ce22b49
# ---------------------------------------------------------------------------

#: Bytes to discard from the head of a channel-read payload.
#:
#: Stock's op04/op05 handlers discard exactly TWO bytes (the opcode echo and
#: one dummy) and then capture 1024 samples.  This project discarded THREE and
#: kept 1023 for months, so every sample array was shifted one byte late and one
#: sample short; the third byte had been misread as a "buffer-valid flag", but
#: the status line reports it as 0x7C / 0x79 — mid-scale sample values, never
#: 0x01.  Fixed in ce22b49 and independently confirmed on a second unit.
#:
#: BLIND SPOT, recorded honestly: the debug shell's ``spi3 opread`` transmits
#: the opcode plus two filler bytes before it begins dumping, so the absolute
#: alignment of dumped byte 0 against stock's sample 0 has not been proven on
#: the wire — only that drop=2 of a 1026-byte window yields stock's 1024-sample
#: count and matches stock's decoded handler.  If you ever get a logic-analyzer
#: capture of stock's runtime reads, check this number first.
STOCK_HEADER_DROP = 2

#: Default payload length for ``spi3 opread`` — stock's channel-read shape.
#: Do NOT shorten it: frame shape is part of the protocol (see the 0x05 hazard
#: note on ``fpga_warmtest_read_channel``).
STOCK_WINDOW_BYTES = 1026

#: Samples that survive ``STOCK_WINDOW_BYTES`` minus ``STOCK_HEADER_DROP``.
STOCK_SAMPLES = STOCK_WINDOW_BYTES - STOCK_HEADER_DROP   # 1024


# ---------------------------------------------------------------------------
# Transports
# ---------------------------------------------------------------------------

class Transport:
    """Minimal command/response channel.

    Exists so the parsing logic can be exercised with no hardware attached —
    see :class:`ScriptedTransport` and ``--selftest``."""

    def exchange(self, line: str, timeout: float) -> str:  # pragma: no cover
        raise NotImplementedError

    def close(self) -> None:
        pass


_PORT_GLOBS = (
    "/dev/ttyACM*",          # Linux: AT32 CDC
    "/dev/ttyUSB*",          # Linux: CP2102/CH340 (ESP32)
    "/dev/cu.usbmodem*",     # macOS: AT32 CDC
    "/dev/cu.usbserial*",    # macOS: ESP32
)


def _autodetect(patterns: Sequence[str]) -> str:
    for pattern in patterns:
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[0]
    raise BenchError(
        "no serial port matched %s — pass port=... explicitly" % (list(patterns),))


class SerialTransport(Transport):
    """Line-oriented serial transport that reads until a prompt.

    Parameters
    ----------
    port
        Device path, or ``None`` to autodetect from ``patterns``.
    prompt
        Byte string that terminates a reply.  ``b">"`` for the AT32 debug
        shell; ``None`` for the ESP32 siggen, which has no prompt and is read
        by quiet-time instead.
    settle
        Seconds to wait after opening before draining the banner.  The ESP32
        resets when DTR asserts and needs ~2 s; the AT32 CDC needs ~0.3 s.
    """

    def __init__(self, port: Optional[str] = None, baud: int = 115200,
                 prompt: Optional[bytes] = b">", settle: float = 0.3,
                 patterns: Sequence[str] = _PORT_GLOBS,
                 quiet_time: float = 0.25):
        import serial  # imported here so `import bench` works without pyserial

        self.port = port or _autodetect(patterns)
        self.prompt = prompt
        self.quiet_time = quiet_time
        try:
            self._ser = serial.Serial(self.port, baud, timeout=0.05)
        except Exception as exc:                       # pragma: no cover
            raise BenchError("cannot open %s: %s" % (self.port, exc)) from exc
        time.sleep(settle)
        self.drain()

    def drain(self, max_bytes: int = 1 << 20) -> bytes:
        """Discard anything already buffered (banner, stale dump tail)."""
        return self._ser.read(max_bytes)

    def exchange(self, line: str, timeout: float) -> str:
        self._ser.reset_input_buffer()
        self._ser.write((line + "\r\n").encode())
        self._ser.flush()
        deadline = time.time() + timeout
        buf = bytearray()
        last = time.time()
        while time.time() < deadline:
            chunk = self._ser.read(8192)
            if chunk:
                buf += chunk
                last = time.time()
                if self.prompt is not None and buf.rstrip().endswith(self.prompt):
                    return buf.decode("utf-8", "replace")
            elif self.prompt is None and buf and (time.time() - last) >= self.quiet_time:
                # Prompt-less device (ESP32): reply is over when it goes quiet.
                return buf.decode("utf-8", "replace")
            else:
                time.sleep(0.005)
        if self.prompt is None:
            # Quiet-time devices legitimately answer nothing to some commands.
            return buf.decode("utf-8", "replace")
        raise PromptTimeout(
            "no prompt %r within %.1fs after %r (got %d bytes). Partial replies "
            "are NOT returned: a truncated dump parses fine and yields a wrong "
            "array." % (self.prompt, timeout, line, len(buf)))

    def close(self) -> None:
        try:
            self._ser.close()
        except Exception:                              # pragma: no cover
            pass


class ScriptedTransport(Transport):
    """Replays canned replies.  For ``--selftest`` and for unit-testing scripts.

    ``replies`` maps an exact command string to a reply, or is a callable
    ``(line) -> str``.  Unknown commands raise, so a test cannot silently pass
    by exercising a path that was never scripted."""

    def __init__(self, replies):
        self.replies = replies
        self.log: list[str] = []

    def exchange(self, line: str, timeout: float) -> str:
        self.log.append(line)
        if callable(self.replies):
            out = self.replies(line)
        else:
            if line not in self.replies:
                raise BenchError("ScriptedTransport: no reply scripted for %r" % line)
            out = self.replies[line]
        if out is None:
            raise BenchError("ScriptedTransport: reply for %r is None" % line)
        return out


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

_DUMP_RE = re.compile(r"^([0-9A-Fa-f]{4}):((?:\s+[0-9A-Fa-f]{2})+)\s*$")

_OPREAD_STATS_RE = re.compile(
    r"op\s+([0-9A-Fa-f]{2}):\s*s=([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})"
    r"\s+nff=(\d+)/(\d+)\s+min=(\d+)\s+max=(\d+)\s+mean=(\d+)\s+span=(\d+)")


@dataclass(frozen=True)
class OpreadStats:
    """The device's own summary line for one ``spi3 opread`` window.

    Useful as a cross-check on the parsed dump: if ``min``/``max``/``span``
    disagree with the numpy array, the serial link dropped bytes."""
    opcode: int
    header: tuple            # (r0, r1, r2) — bytes clocked out with opcode+2 fillers
    nonff: int
    length: int
    min: int
    max: int
    mean: int
    span: int


def parse_dump(text: str, strict: bool = True) -> np.ndarray:
    """Parse ``NNNN: xx xx ...`` hex-dump lines into a uint8 array.

    ``strict`` (default) verifies that each line's declared offset equals the
    number of bytes seen so far.  A USB CDC drop that loses one 16-byte line is
    otherwise invisible: the array simply comes back 16 samples shorter, with
    every later sample shifted — exactly the class of defect that has cost this
    project weeks.  Non-dump lines (command echo, stats line, prompt) are
    ignored, so you can hand it the whole reply.
    """
    vals: list[int] = []
    for raw in text.splitlines():
        m = _DUMP_RE.match(raw.strip())
        if not m:
            continue
        offset = int(m.group(1), 16)
        if strict and offset != len(vals):
            raise BenchError(
                "hex dump discontinuity: line declares offset 0x%04X but %d "
                "bytes have been seen — the link dropped data, so this window "
                "is unusable" % (offset, len(vals)))
        vals.extend(int(b, 16) for b in m.group(2).split())
    return np.array(vals, dtype=np.uint8)


def parse_opread_stats(text: str) -> Optional[OpreadStats]:
    """Parse the ``op 04: s=.. nff=../.. min=.. max=..`` summary, or None."""
    m = _OPREAD_STATS_RE.search(text)
    if not m:
        return None
    g = m.groups()
    return OpreadStats(
        opcode=int(g[0], 16),
        header=(int(g[1], 16), int(g[2], 16), int(g[3], 16)),
        nonff=int(g[4]), length=int(g[5]),
        min=int(g[6]), max=int(g[7]), mean=int(g[8]), span=int(g[9]),
    )


_SIGGEN_RE = re.compile(
    r"\[siggen\]\s+CH(\d)\s+mode=(\w+)\s+freq=([-\d.]+)\s*Hz\s+amp=(-?\d+)\s*mVpp"
    r"\s+mid=(-?\d+)\s*mV\s+duty=(-?\d+)%\s+phase=(-?\d+)\s*deg")

_PWM_RE = re.compile(
    r"\[pwm\]\s+GPIO27\s+req=([-\d.]+)\s*Hz\s+(\d+)%\s+(\d+)-bit\s+actual=(\d+)\s*Hz")

_PWM_OFF_RE = re.compile(r"\[pwm\]\s+GPIO27\s+off")


@dataclass(frozen=True)
class SiggenStatus:
    """One parsed ``[siggen] CHn ...`` line.

    ``freq_hz`` is what the ESP32 *believes* it is generating.  It is NOT a
    measurement of the wire: on bench unit #1 (2026-08-17) a nominal 100 Hz
    request read 82 Hz through stock firmware, and 250 Hz read 208 Hz — the
    same 0.82 factor on both channels.  Derive the true rate from the capture,
    or cross-check with a counter."""
    ch: int
    mode: str
    freq_hz: float
    amp_mvpp: int
    mid_mv: int
    duty_pct: int
    phase_deg: int


@dataclass(frozen=True)
class PwmStatus:
    on: bool
    req_hz: float = 0.0
    duty_pct: int = 0
    bits: int = 0
    actual_hz: int = 0


def parse_siggen_status(text: str) -> dict:
    """Return ``{1: SiggenStatus, 2: SiggenStatus}`` for whatever is present."""
    out: dict = {}
    for m in _SIGGEN_RE.finditer(text):
        ch = int(m.group(1))
        out[ch] = SiggenStatus(
            ch=ch, mode=m.group(2), freq_hz=float(m.group(3)),
            amp_mvpp=int(m.group(4)), mid_mv=int(m.group(5)),
            duty_pct=int(m.group(6)), phase_deg=int(m.group(7)))
    return out


def parse_pwm_status(text: str) -> Optional[PwmStatus]:
    m = _PWM_RE.search(text)
    if m:
        return PwmStatus(True, float(m.group(1)), int(m.group(2)),
                         int(m.group(3)), int(m.group(4)))
    if _PWM_OFF_RE.search(text):
        return PwmStatus(False)
    return None


# ---------------------------------------------------------------------------
# Scope — the AT32 USB CDC debug shell
# ---------------------------------------------------------------------------

class Scope:
    """The 2C53T under test, driven through its USB CDC debug shell.

    ::

        sc = Scope("/dev/ttyACM0")
        sc.seq(0x01, 0x10)              # timebase index
        v = sc.opread(0x04)             # 1024 samples, stock framing

    Every method that talks to the FPGA bus goes through the shell, which parks
    the continuous acquisition task first — a shell CS assert interleaved with
    acquisition frames is a desync class that needs a true FPGA power cycle to
    clear.

    NEVER read Gowin STATUS (``0x41``) on a configured part during capture: it
    desynchronises the running design, and only a true power cycle (POWER →
    "Goodbye" → unplug USB → replug) recovers it.
    """

    #: Opcodes that are register WRITES on the configured user design.  Reading
    #: them with 0xFF filler smashes live state (0x01 is the run/timebase
    #: register), so a sweep must skip them.
    WRITE_OPCODES = (0x01, 0x02, 0x06, 0x07, 0x08)

    def __init__(self, port: Optional[str] = "/dev/ttyACM0", baud: int = 115200,
                 transport: Optional[Transport] = None, settle: float = 0.4):
        self._port, self._baud, self._settle = port, baud, settle
        self.reconnects = 0          #: times opread() reopened the port after an empty window
        self._last_transport_error: Optional[BaseException] = None
        if transport is not None:
            self._t = transport
        else:
            self._t = SerialTransport(port, baud, prompt=b">", settle=settle,
                                      patterns=("/dev/ttyACM*", "/dev/cu.usbmodem*"))

    # -- raw ---------------------------------------------------------------

    def cmd(self, line: str, timeout: float = 3.0) -> str:
        """Send one shell line, return everything up to the ``>`` prompt.

        Raises :class:`PromptTimeout` rather than returning a partial reply."""
        return self._t.exchange(line, timeout)

    def close(self) -> None:
        self._t.close()

    # -- reads -------------------------------------------------------------

    def opread(self, op: int, n: int = STOCK_WINDOW_BYTES,
               drop: int = STOCK_HEADER_DROP, timeout: Optional[float] = None,
               dtype=float) -> np.ndarray:
        """One ``spi3 opread <op> <n> dump`` window as a numpy array.

        ``drop`` defaults to :data:`STOCK_HEADER_DROP` = **2**, matching stock's
        decoded handler (opcode echo + one dummy, then 1024 samples).  Dropping
        3 was a real bug that shifted every array one byte late and one sample
        short; it survived for months because the shifted data still looked like
        a waveform.  Change this only with evidence, and record why.

        Raises :class:`ShortReadError` if fewer than ``n`` bytes came back, so a
        truncated window can never be quietly analysed as if it were whole.
        """
        if not 0 <= op <= 0xFF:
            raise BenchError("opcode out of range: %r" % (op,))
        if drop < 0:
            raise BenchError("drop must be >= 0")
        if timeout is None:
            # ~3 chars of hex per byte at 115200, plus SPI time and slack.
            timeout = 3.0 + n / 300.0
        line = "spi3 opread %02x %d dump" % (op, n)
        raw = self._opread_once(line, timeout)
        if len(raw) == 0 and self._reconnect():
            # EXP-66: the device's CDC self-heal (#39) drops it off the bus for
            # ~200 ms and it comes back on the same node; a window read across
            # that gap is empty, not short. One reopen, one retry, then the
            # error stands. A SHORT window (some bytes) is never retried: it
            # is a torn record, and retrying would hide how often that is.
            raw = self._opread_once(line, timeout)
        if len(raw) < n:
            why = ("" if self._last_transport_error is None
                   else " (transport: %s)" % self._last_transport_error)
            self._last_transport_error = None
            raise ShortReadError(
                "opread %02X: asked for %d bytes, parsed %d — window unusable%s"
                % (op, n, len(raw), why))
        return raw[drop:drop + (n - drop)].astype(dtype)

    def _opread_once(self, line: str, timeout: float) -> np.ndarray:
        try:
            return parse_dump(self.cmd(line, timeout))
        except (PromptTimeout, OSError) as exc:
            # The port vanished mid-command (device re-enumerating): the
            # caller treats this as an empty window and may reconnect.
            self._last_transport_error = exc
            return np.zeros(0)

    def _reconnect(self, wait_s: float = 30.0) -> bool:
        """Reopen the serial port after the device re-enumerated. False when
        this Scope was given a transport (tests, selftest) or the port does
        not come back within ``wait_s``."""
        if not isinstance(self._t, SerialTransport):
            return False
        port = self._t.port
        try:
            self._t.close()
        except Exception:                              # pragma: no cover
            pass
        deadline = time.time() + wait_s
        while time.time() < deadline:
            if os.path.exists(port):
                try:
                    time.sleep(1.0)        # let the host finish enumerating
                    self._t = SerialTransport(port, self._baud, prompt=b">", settle=self._settle,
                                              patterns=(port,))
                    self.reconnects += 1
                    return True
                except BenchError:
                    pass
            time.sleep(0.25)
        return False

    def opread_stats(self, op: int, n: int = STOCK_WINDOW_BYTES,
                     timeout: Optional[float] = None) -> OpreadStats:
        """The device's own min/max/span summary, without a full dump.

        Cheap enough for a canary between sweep steps."""
        if timeout is None:
            timeout = 3.0 + n / 3000.0
        text = self.cmd("spi3 opread %02x %d" % (op, n), timeout)
        st = parse_opread_stats(text)
        if st is None:
            raise BenchError("opread %02X: no stats line in reply:\n%s" % (op, text))
        return st

    def reader(self, op: int, **kw) -> Callable[[], np.ndarray]:
        """A zero-argument closure reading ``op``.  Feed to :func:`paired_difference`."""
        return lambda: self.opread(op, **kw)

    # -- writes ------------------------------------------------------------

    def seq(self, *bytes_: int, timeout: float = 3.0) -> str:
        """``spi3 seq`` — CS-framed byte exchange.  Pass ``"|"`` for a CS pulse.

        ``sc.seq(0x01, 0x10)`` sets the timebase index;
        ``sc.seq(0x09, 0xFF, 0xFF, "|", 0x0A, 0xFF, 0xFF)`` reproduces the
        mid-sequence CS pulse pattern."""
        toks = []
        for b in bytes_:
            if b == "|":
                toks.append("|")
            elif isinstance(b, int) and 0 <= b <= 0xFF:
                toks.append("%02x" % b)
            else:
                raise BenchError("spi3 seq takes bytes 0..255 or '|', got %r" % (b,))
        return self.cmd("spi3 seq " + " ".join(toks), timeout)

    def gpio(self, pin: str, level: int, timeout: float = 3.0) -> str:
        """``gpio set <port><pin> <0|1>``, e.g. ``sc.gpio("E4", 1)``.

        Note: ``gpio set`` alone does nothing on a pin that is not already
        configured as an output.  Establish the bank first — one
        :meth:`scope_range` call per channel does that for the frontend."""
        if not re.fullmatch(r"[A-Ea-e]\d{1,2}", pin):
            raise BenchError("pin must look like 'B11' / 'E4', got %r" % (pin,))
        if level not in (0, 1):
            raise BenchError("level must be 0 or 1, got %r" % (level,))
        return self.cmd("gpio set %s %d" % (pin.upper(), level), timeout)

    def scope_range(self, n: int, ch: int, timeout: float = 3.0) -> str:
        """``fpga scope range <n> <ch>`` — coarse frontend range on ONE channel.

        ``ch`` must be 1 or 2 and is REQUIRED here on purpose.  The shell's
        channel argument is 1-based, so ``fpga scope range 5 0`` does not mean
        "channel 0" — it means "no channel given" and silently addresses BOTH
        banks.  That has already produced one wrong conclusion.  If you really
        want both, call :meth:`scope_range_both`."""
        if ch not in (1, 2):
            raise BenchError(
                "channel must be 1 or 2 (the shell arg is 1-based; 0 silently "
                "means BOTH). Use scope_range_both() if that is what you want.")
        if not 0 <= n <= 9:
            raise BenchError("range must be 0..9, got %r" % (n,))
        return self.cmd("fpga scope range %d %d" % (n, ch), timeout)

    def scope_range_both(self, n: int, timeout: float = 3.0) -> str:
        """Deliberately address both frontend banks (the shell's no-channel form)."""
        if not 0 <= n <= 9:
            raise BenchError("range must be 0..9, got %r" % (n,))
        return self.cmd("fpga scope range %d" % n, timeout)

    # -- convenience -------------------------------------------------------

    def version(self, timeout: float = 3.0) -> str:
        """Firmware banner.  Record it in the experiment file: which build was
        running is part of the measurement, not metadata."""
        return self.cmd("version", timeout).strip()

    def vdiv(self, ch: int, index: int) -> str:
        """Set a channel's volts/div range in BOTH display state and relays.

        Uses `fpga scope vdiv`, NOT `fpga scope range`. The raw form drives
        the relay bank behind the display's back, so the badge's counts->volts
        k stays at the OLD range — exactly how EXP-19's first grid run got
        fourteen refusals: the harness moved the hardware and the badge never
        heard. Same lesson as timebase()/timebase_raw() below."""
        return self.cmd(f"fpga scope vdiv {ch} {index}")

    def timebase(self, index: int) -> str:
        """Set the timebase in BOTH the display state and reg 0x01.

        Uses `fpga scope timebase`, NOT a raw `seq 01 XX`. The raw write
        changes the hardware behind the display's back: the firmware carries
        the current timebase in scope_state.timebase_idx (what labels the axis
        and picks fs for the Freq badge) and in fpga.c's acq_rate_idx (what is
        programmed into the register), and on 2026-08-19 these were found
        diverging on a stock boot -- 0x0A on the display, 0x08 in the register.
        Driving them apart from a bench script is how that stays hidden, so
        this method drives them together.
        """
        return self.cmd(f"fpga scope timebase {index & 0xFF:02X}")

    def timebase_raw(self, index: int) -> str:
        """Write reg 0x01 directly, leaving the display state stale.

        Only for deliberately testing the divergence above. If you want to
        change the timebase, use timebase().
        """
        return self.seq(0x01, index & 0xFF)

    def trigger_mode(self, mode: str) -> str:
        """Set the acquisition trigger mode through the single entry point.

        `fpga scope trigmode auto|normal|single` is the same path the UI
        button takes (trigger-modes spec S3 (i)). The mode is an acquisition
        policy in fpga.c, not a fabric register: EXP-55 found no polarity
        select and nothing here writes SPI3 directly.
        """
        return self.cmd(f"fpga scope trigmode {mode}")

    def trigger_level(self, level: int) -> str:
        """Set the trigger level through the single entry point.

        `fpga scope level <n>` is the one writer of SPI3 reg 0x08 (EXP-43,
        `3ec8fd9`); it records what is in force for the UI and re-primes the
        capture. `n` is the UI unit (code = 128 + 1.2 n by readback, EXP-56).
        The reply carries `code 0x..`, which is the number to trust.
        """
        return self.cmd(f"fpga scope level {int(level)}")

    def trigger_level_raw(self, code: int) -> str:
        """Write reg 0x08 directly, leaving the UI's idea of the level stale.

        Only for deliberately testing that divergence, like timebase_raw().
        The comparator fires at (code - 28) in record units (EXP-53/55/56).
        """
        return self.seq(0x08, code & 0xFF)


# ---------------------------------------------------------------------------
# Siggen — the ESP32 bench source (esp32_siggen/)
# ---------------------------------------------------------------------------

class Siggen:
    """The ESP32 two-channel signal generator.

    GPIO25 (DAC1) → CH1 probe tip, GPIO26 (DAC2) → CH2 probe tip, GPIO27 =
    hardware PWM, GND → ground clip(s).  Software DDS at 40 kSa/s, so useful
    from ~1 Hz to ~5 kHz; above that use :meth:`pwm`.

    Every setter PARSES the device's echo and raises if the device did not
    report the state that was asked for.  Assuming a serial write took effect
    is how ``usart tx`` frames got "sent" into a task that a coldtrace build
    never creates.

    A caution from the README, worth repeating: an anti-phase sine pair is a
    *weaker* two-channel test than two different waveform SHAPES.  If a display
    inverts, offsets or rescales a trace, anti-phase can be mistaken for one
    source drawn twice.  A triangle on one jack and a square on the other
    cannot: ``sg.tri(500, ch=1); sg.square(500, ch=2)``.
    """

    #: Measured on bench unit #1, 2026-08-17, through stock firmware: requested
    #: frequency came out ~0.82x on BOTH channels.  Provided so window
    #: calculations can allow for it — NOT as a calibration you should trust.
    NOMINAL_TO_ACTUAL = 0.82

    _MODE_ALIASES = {"off": "dc"}     # `off` sets mode 0, which reports as "dc"

    def __init__(self, port: Optional[str] = "/dev/ttyUSB0", baud: int = 115200,
                 transport: Optional[Transport] = None, settle: float = 2.0):
        if transport is not None:
            self._t = transport
        else:
            # No prompt; the ESP32 resets on DTR and needs ~2 s before it talks.
            self._t = SerialTransport(port, baud, prompt=None, settle=settle,
                                      patterns=("/dev/ttyUSB*", "/dev/cu.usbserial*"),
                                      quiet_time=0.25)

    def close(self) -> None:
        self._t.close()

    # -- raw ---------------------------------------------------------------

    def send(self, line: str, timeout: float = 1.0, settle: float = 0.25) -> str:
        """Send one line; return the reply.  ``settle`` lets the DDS/relays land."""
        out = self._t.exchange(line, timeout)
        if settle:
            time.sleep(settle)
        return out

    def _set(self, ch: int, line: str, expect_mode: Optional[str],
             expect_hz: Optional[float], settle: float) -> SiggenStatus:
        if ch not in (1, 2):
            raise BenchError("siggen channel must be 1 or 2, got %r" % (ch,))
        text = self.send(line, settle=settle)
        st = parse_siggen_status(text).get(ch)
        if st is None:
            raise BenchError(
                "siggen did not echo a CH%d status for %r — reply was:\n%s\n"
                "(unparsed reply means the command may not have taken effect; "
                "it is not treated as success)" % (ch, line, text.strip()))
        if expect_mode is not None:
            want = self._MODE_ALIASES.get(expect_mode, expect_mode)
            if st.mode != want:
                raise BenchError("siggen CH%d: asked for mode %s, device reports %s"
                                 % (ch, want, st.mode))
        if expect_hz is not None and expect_hz > 0:
            # DDS phase-increment quantisation, not wire accuracy — see the
            # 0.82 factor note.  This only catches "the command was ignored".
            if abs(st.freq_hz - expect_hz) > max(1.0, 0.05 * expect_hz):
                raise BenchError("siggen CH%d: asked for %.1f Hz, device reports %.1f Hz"
                                 % (ch, expect_hz, st.freq_hz))
        return st

    # -- waveforms ---------------------------------------------------------

    def sine(self, hz: float, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        return self._set(ch, "%d sine %g" % (ch, hz), "sine", hz, settle)

    def square(self, hz: float, duty: Optional[int] = None, ch: int = 1,
               settle: float = 0.35) -> SiggenStatus:
        line = "%d square %g" % (ch, hz) + ("" if duty is None else " %d" % duty)
        return self._set(ch, line, "square", hz, settle)

    def tri(self, hz: float, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        return self._set(ch, "%d tri %g" % (ch, hz), "tri", hz, settle)

    def saw(self, hz: float, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        return self._set(ch, "%d saw %g" % (ch, hz), "saw", hz, settle)

    def dc(self, mv: int, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        return self._set(ch, "%d dc %d" % (ch, mv), "dc", None, settle)

    def amp(self, mvpp: int, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        return self._set(ch, "%d amp %d" % (ch, mvpp), None, None, settle)

    def off(self, ch: int = 1, settle: float = 0.35) -> SiggenStatus:
        """Park a channel at its midpoint.  Reports back as ``mode=dc``."""
        return self._set(ch, "%d off" % ch, "off", None, settle)

    def phase(self, deg: int, ch: int = 2, settle: float = 0.35) -> SiggenStatus:
        """Phase relative to CH1.  Holds only while both frequencies are equal."""
        st = self._set(ch, "%d phase %d" % (ch, deg), None, None, settle)
        want = deg % 360
        if st.phase_deg != want:
            raise BenchError("siggen CH%d: asked for %d deg, device reports %d"
                             % (ch, want, st.phase_deg))
        return st

    # -- pwm ---------------------------------------------------------------

    def pwm(self, hz: float, duty: int = 50, settle: float = 0.35) -> PwmStatus:
        """Hardware LEDC PWM on GPIO27 — the only way above ~5 kHz."""
        text = self.send("pwm %g %d" % (hz, duty), settle=settle)
        st = parse_pwm_status(text)
        if st is None or not st.on:
            raise BenchError("pwm %g Hz: device did not confirm.  Reply:\n%s"
                             % (hz, text.strip()))
        return st

    def pwm_off(self, settle: float = 0.2) -> PwmStatus:
        text = self.send("pwm off", settle=settle)
        # The sketch answers a bare "[pwm] off" here, NOT a status line, so
        # parse_pwm_status returns None and this helper raised every time it
        # was called.  Found 2026-08-19 while pinning the source rate.
        if re.search(r"\[pwm\]\s*off", text):
            return PwmStatus(False)
        st = parse_pwm_status(text)
        if st is None:
            raise BenchError("pwm off: no confirmation.  Reply:\n%s" % text.strip())
        return st

    # -- achieved sample rate ---------------------------------------------

    def fs(self, window: float = 0.0) -> tuple:
        """The DDS loop's MEASURED sample rate, as ``(hz, ratio_to_nominal)``.

        The sketch's ``FS = 40000`` is an assumption the loop cannot hold: it
        reschedules from ``now`` after the work is already done, so the real
        period is whatever two ``dacWrite`` calls cost.  Bench unit's ESP32
        measures **32,999 Hz, ratio 0.8250**, stable to four digits over 36 s
        and identical across sine/square/tri (DC differs by 0.9%, because that
        path returns early -- never use DC as a timing reference).

        This is the source of the ~0.82 factor that has shadowed every
        frequency number in this project, and of the 1.2x gap against Stlkv's
        rig.  See docs/experiments/2026-08-19-14-siggen-sample-rate.md.

        **The rate depends on the CHANNEL CONFIGURATION** and is a clean
        function of it -- each channel in a waveform mode costs ~300 Hz of loop
        rate, because ``next_sample`` returns early for DC::

            both sine   32,999.5 Hz   0.8250
            sine + dc   33,298.8 Hz   0.8325
            both dc     33,557.4 Hz   0.8389

        Repeatable to five digits, and PWM does not affect it.  So measure the
        rate in the SAME configuration the tones will be produced in -- taking
        it before parking the unused channel builds in a 0.9% error.

        ``window`` seconds, if given, restarts the count and waits, which is
        what you want before quoting a number.
        """
        if window > 0:
            self.send("fs reset", settle=0.1)
            time.sleep(window)
        text = self.send("fs", timeout=2.0, settle=0.1)
        m = re.search(r"achieved=([0-9.]+)\s*Hz\s+ratio=([0-9.]+)", text)
        if not m:
            raise BenchError("siggen did not report an achieved rate.  Reply:\n%s"
                             % text.strip())
        return float(m.group(1)), float(m.group(2))

    def use_measured_fs(self, on: bool = True, window: float = 6.0) -> float:
        """Make ``set_freq`` divide by the MEASURED rate, so commanded ==
        delivered.  Returns the divisor now in force.

        Off by default on the device, deliberately: booting unchanged keeps the
        generator bit-identical to the one that took every earlier measurement,
        so flashing the reporting firmware does not silently rescale the
        archive.  Turn it ON for any new absolute-frequency work.
        """
        if on and window > 0:
            self.fs(window=window)          # need a measurement to adopt
        text = self.send("usefs %d" % (1 if on else 0), timeout=2.0, settle=0.2)
        m = re.search(r"divides by ([0-9.]+)\s*\((MEASURED|nominal)\)", text)
        if not m:
            raise BenchError("siggen did not confirm usefs %d.  Reply:\n%s"
                             % (on, text.strip()))
        want = "MEASURED" if on else "nominal"
        if m.group(2) != want:
            raise BenchError("siggen usefs: asked for %s, device reports %s"
                             % (want, m.group(2)))
        return float(m.group(1))

    # -- status ------------------------------------------------------------

    def status(self) -> dict:
        """Parsed state of both channels and the PWM.

        Returns ``{1: SiggenStatus, 2: SiggenStatus, "pwm": PwmStatus}``.
        Raises if either channel is missing from the reply, rather than
        returning a half-populated dict that a caller will index blindly."""
        text = self.send("status", timeout=2.0, settle=0.1)
        out = parse_siggen_status(text)
        missing = [c for c in (1, 2) if c not in out]
        if missing:
            raise BenchError("siggen status: no line for CH%s.  Reply:\n%s"
                             % (missing, text.strip()))
        pwm = parse_pwm_status(text)
        result: dict = dict(out)
        result["pwm"] = pwm if pwm is not None else PwmStatus(False)
        return result


# ---------------------------------------------------------------------------
# JDS6600 — the trusted bench signal generator (register protocol)
# ---------------------------------------------------------------------------

class JDS6600:
    """Driver for the JDS6600 DDS generator over its USB CH340 serial link.

    A DIFFERENT INSTRUMENT from :class:`Siggen`.  Siggen is the ESP32 sketch
    whose loop free-runs at 0.825x commanded (EXP-14); this is a commercial DDS
    generator that is **crystal-accurate in FREQUENCY** (it confirmed timebase
    0x0E to 0.1 %, EXP-20) but only ~1-2 % in amplitude.  Use it as the
    frequency reference; treat its amplitude as good-but-not-metrology.

    Protocol: ASCII registers, ``:rNN=`` to read, ``:wNN=value.`` to write.
    **The trailing period is part of the documented write format**
    (https://sigrok.org/wiki/Joy-IT_JDS6600) and this class always emits it.  A
    write WITHOUT it returns ``:ok`` and is silently ignored on the amplitude
    register -- precisely the "stable plausible wrong number" this module exists
    to prevent (EXP-20 lost a whole sweep to it) -- so every setter here READS
    BACK and raises if the value did not take.

    Registers: 20 output enable (a,b); 21/22 waveform; 23/24 frequency
    (Hz x 100, unit field 0); 25/26 amplitude (mV, == Vpp); 27/28 offset
    (1000 = 0 V, observed 1 LSB = 10 mV -- verify per firmware); 29/30 duty
    (0.1 %).
    """

    #: Known-good waveform codes.  Codes >= 2 are firmware-dependent on the
    #: JDS6600 family; pass a raw int when in doubt.
    # JDS6600 waveform codes: 0 sine, 1 square, 2 PULSE, 3 triangle. Until
    # 2026-09-22 this map sent 2 for "triangle", which is the pulse train;
    # EXP-53's triangle runs verified code 3 on the scope and wrote it by hand
    # (`write_raw(21, "3")`). A wrong shape on a slope-sign test is the kind
    # of stable, plausible instrument error this project keeps finding.
    WAVE = {"sine": 0, "square": 1, "pulse": 2, "triangle": 3, "tri": 3}

    def __init__(self, port: Optional[str] = "/dev/ttyUSB0", baud: int = 115200,
                 settle: float = 0.35, ser=None):
        if ser is not None:
            self._ser = ser                      # injected fake, for tests
        else:
            import serial  # lazy, like SerialTransport, so bare `import bench` works
            if isinstance(port, str) and any(c in port for c in "*?["):
                port = _autodetect((port,))
            try:
                self._ser = serial.Serial(port, baud, timeout=0.6)
            except Exception as exc:
                raise BenchError("cannot open JDS6600 on %s: %s" % (port, exc)) from exc
        self.port = port
        self._settle = settle
        time.sleep(0.2)
        self._ser.reset_input_buffer()

    def close(self) -> None:
        try:
            self._ser.close()
        except Exception:
            pass

    # -- raw register I/O --------------------------------------------------

    def _txn(self, line: str, wait: float = 0.3) -> str:
        self._ser.reset_input_buffer()
        self._ser.write((line + "\r\n").encode())
        time.sleep(wait)
        return self._ser.read(4000).decode("ascii", "replace")

    def read_raw(self, reg: int) -> str:
        """Value string after ``:rNN=`` (trailing '.' stripped).

        Register numbers are two digits in the JDS6600 protocol: a read of
        register 0 is queried and echoed as ``:r00=``, not ``:r0=``.  Padding
        to two digits is a no-op for the 20-30 range but is what makes the
        single-digit registers (waveform-independent config) parse at all."""
        pfx = ":r%02d=" % reg
        resp = self._txn(pfx)
        for ln in resp.splitlines():
            if ln.startswith(pfx):
                return ln[len(pfx):].rstrip(".").strip()
        raise BenchError("JDS6600 r%d: no '%s' line in reply:\n%s"
                         % (reg, pfx, resp.strip() or "<empty>"))

    def write_raw(self, reg: int, value: str) -> None:
        """Send ``:wNN=value.`` (period ALWAYS emitted) and confirm ``:ok``."""
        resp = self._txn(":w%02d=%s." % (reg, value))
        if ":ok" not in resp.lower():
            raise BenchError("JDS6600 w%d=%s: no :ok in reply:\n%s"
                             % (reg, value, resp.strip() or "<empty>"))
        time.sleep(self._settle)

    def _write_checked(self, reg: int, value: str, expect: str) -> str:
        """Write, then read back; raise if the register did not become ``expect``.

        This is the whole point of the class -- a JDS6600 answers ``:ok`` to a
        write it then ignores (missing period, out-of-range value), so success
        is proven by readback, never by the ack."""
        self.write_raw(reg, value)
        got = self.read_raw(reg)
        if got != expect:
            raise BenchError(
                "JDS6600 w%d: wrote %s, reads back %r (expected %r) -- the write "
                "did not take (trailing period? value out of range?)"
                % (reg, value, got, expect))
        return got

    @staticmethod
    def _ch_reg(base1: int, ch: int) -> int:
        if ch not in (1, 2):
            raise BenchError("JDS6600 channel must be 1 or 2, got %r" % (ch,))
        return base1 + (ch - 1)

    # -- setters (channel is 1 or 2) --------------------------------------

    def output(self, ch1: bool, ch2: bool) -> None:
        """Enable/disable the two channel outputs (written as ``a,b``)."""
        s = "%d,%d" % (int(bool(ch1)), int(bool(ch2)))
        self._write_checked(20, s, s)

    def waveform(self, wave, ch: int = 1) -> None:
        code = self.WAVE.get(wave, wave) if isinstance(wave, str) else wave
        self._write_checked(self._ch_reg(21, ch), str(code), str(code))

    def freq(self, hz: float, ch: int = 1) -> None:
        """Frequency in Hz (crystal-accurate).  Encoded as Hz x 100, unit 0."""
        s = "%d,0" % int(round(hz * 100))
        self._write_checked(self._ch_reg(23, ch), s, s)

    def amp(self, vpp: float, ch: int = 1) -> None:
        """Amplitude in volts peak-to-peak (JDS 'amplitude' == Vpp, EXP-20)."""
        mv = int(round(vpp * 1000))
        if not 0 <= mv <= 20000:
            raise BenchError("JDS6600 amp %.3f V out of 0..20 Vpp" % vpp)
        self._write_checked(self._ch_reg(25, ch), str(mv), str(mv))

    def offset(self, volts: float, ch: int = 1) -> None:
        """DC offset in volts.  Register 1000 = 0 V, observed 1 LSB = 10 mV."""
        code = int(round(1000 + volts * 100))
        if not 0 <= code <= 2000:
            raise BenchError("JDS6600 offset %.2f V out of range" % volts)
        self._write_checked(self._ch_reg(27, ch), str(code), str(code))

    def duty(self, pct: float, ch: int = 1) -> None:
        s = str(int(round(pct * 10)))
        self._write_checked(self._ch_reg(29, ch), s, s)

    def phase(self, deg: float) -> None:
        """CH2 phase relative to CH1, in degrees.

        The JDS6600 has ONE phase register (31, 0.1-degree units) -- it offsets
        CH2 against CH1; there is no per-channel phase.  Normalised to
        [0, 360).  Readback-verified like every setter here."""
        tenths = int(round(deg * 10)) % 3600
        self._write_checked(31, str(tenths), str(tenths))

    # -- state save / restore ---------------------------------------------

    _STATE_REGS = (20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30)

    def state(self) -> dict:
        """Snapshot the key registers as ``{reg: raw_string}`` for :meth:`restore`."""
        return {r: self.read_raw(r) for r in self._STATE_REGS}

    def restore(self, snap: dict) -> None:
        """Put a snapshot back; register 20 (output) written LAST."""
        for r in self._STATE_REGS:
            if r != 20 and r in snap:
                self._write_checked(r, snap[r], snap[r])
        if 20 in snap:
            self._write_checked(20, snap[20], snap[20])


# ---------------------------------------------------------------------------
# Signal sources — one contract for the ESP32 siggen, a Kode Dot, or a person
# ---------------------------------------------------------------------------
#
# The bench scripts were written against one rig: the ESP32 sketch on
# /dev/ttyUSB0.  A contributor with a different stimulus could not run them
# at all.  :class:`SignalSource` is the part of a stimulus a script actually
# uses — set a tone and learn the frequency it REALLY has, set a drive level
# and learn the amplitude it REALLY has, go quiet — and three implementations
# sit behind it:
#
#   esp32    :class:`Esp32Source`, a thin adapter over :class:`Siggen`
#            (which is left exactly as it was).
#   kodedot  :class:`KodeDotSource`, a Kode Dot (ESP32-P4) running the
#            sigsrc app: LEDC square on a J3 header pin, crystal-derived,
#            and it reports the frequency computed from its timer registers.
#   manual   :class:`ManualSource`, any generator plus a counter and a DMM:
#            the operator sets it and types what the instruments read.
#
# The contract is the Siggen one: a setter that cannot confirm what it asked
# for RAISES.  ``freq_hz`` is what the source reports it is generating, never
# the number that was requested.

#: USB IDs for port discovery.  On macOS the 2C53T's CDC shell and an
#: ESP32-P4's USB-Serial-JTAG BOTH enumerate as /dev/cu.usbmodem*, so a glob
#: picks whichever sorts first.  The vendor ID cannot be confused.
SCOPE_USB_ID = (0x2E3C, 0x5740)      # 2C53T app running: the CDC debug shell
ESPRESSIF_VID = 0x303A               # Kode Dot: ESP32-P4 USB-Serial-JTAG

SOURCE_KINDS = ("esp32", "kodedot", "manual")
WAVEFORMS = ("sine", "square")


def find_port(vid: int, pid: Optional[int] = None,
              serial_number: Optional[str] = None, what: str = "device",
              ports=None) -> str:
    """The one serial port whose USB VID (and PID / serial, if given) match.

    Raises rather than guessing: no match, or more than one, is reported with
    every port that WAS seen, so the operator can pass the right one.
    ``ports`` is injectable for tests (objects with ``device``, ``vid``,
    ``pid``, ``serial_number``); by default pyserial enumerates them, which
    lists devices without opening any."""
    if ports is None:
        from serial.tools import list_ports   # lazy: `import bench` needs no pyserial
        ports = list_ports.comports()
    ports = list(ports)

    def ident(p) -> str:
        if getattr(p, "vid", None) is None:
            return "%s (no USB id)" % p.device
        return "%s (%04x:%04x%s)" % (
            p.device, p.vid, p.pid or 0,
            " sn %s" % p.serial_number if getattr(p, "serial_number", None) else "")

    hits = [p for p in ports
            if getattr(p, "vid", None) == vid
            and (pid is None or getattr(p, "pid", None) == pid)
            and (serial_number is None
                 or (getattr(p, "serial_number", None) or "") == serial_number)]
    # macOS can list a device as /dev/cu.X and /dev/tty.X: one device, prefer cu.
    uniq: dict = {}
    for p in hits:
        key = (p.pid, getattr(p, "serial_number", None), getattr(p, "location", None),
               p.device.replace("/dev/tty.", "/dev/cu."))
        if key not in uniq or p.device.startswith("/dev/cu."):
            uniq[key] = p
    want = "%04x:%s%s" % (vid, "%04x" % pid if pid is not None else "*",
                          " serial %s" % serial_number if serial_number else "")
    if not uniq:
        raise BenchError(
            "no %s with USB id %s. Ports seen: %s. Plug it in, or pass the port "
            "explicitly." % (what, want, ", ".join(ident(p) for p in ports) or "none"))
    if len(uniq) > 1:
        raise BenchError(
            "%d ports match %s (%s): %s. Pick one by USB serial number or pass "
            "the port explicitly." % (len(uniq), what, want,
                                      ", ".join(ident(p) for p in uniq.values())))
    return next(iter(uniq.values())).device


# -- amplitude and frequency maths that depend on the waveform ---------------

_HZ_TEXT_RE = re.compile(
    r"^\s*([0-9]*\.?[0-9]+(?:[eE][+-]?\d+)?)\s*([kKM]?)\s*(?:[hH][zZ])?\s*$")
_AMP_TEXT_RE = re.compile(
    r"^\s*([0-9]*\.?[0-9]+)\s*(m?)V\s*(pp|p-p|rms|dc)?\s*$", re.IGNORECASE)


def parse_hz_text(text: str) -> Optional[float]:
    """Operator input like ``999.98``, ``1k``, ``1.0001 kHz``, ``2 MHz`` -> Hz.

    Lower-case ``m`` is refused rather than read as milli or mega: a counter
    reading is never millihertz here, and guessing is how 1 kHz becomes 1 MHz."""
    m = _HZ_TEXT_RE.match(text or "")
    if not m:
        return None
    val = float(m.group(1)) * {"": 1.0, "k": 1e3, "K": 1e3, "M": 1e6}[m.group(2)]
    return val if val > 0 else None


def parse_amplitude_text(text: str) -> Optional[tuple]:
    """Operator input -> ``(millivolts, unit)``, unit one of ``pp``/``rms``/``dc``.

    ``2.000 Vpp``, ``707 mVrms``, ``3.292 V`` (a DC level: the high level of
    a square whose low level is 0 V).  A bare number has no unit and returns
    None — Vpp, Vrms and a DC level differ by up to 2.8x, and the caller asks
    again rather than guessing."""
    m = _AMP_TEXT_RE.match(text or "")
    if not m:
        return None
    mv = float(m.group(1)) * (1.0 if m.group(2) else 1000.0)
    unit = (m.group(3) or "dc").lower().replace("p-p", "pp")
    return (mv, unit) if mv > 0 else None


def vpp_from_reading(value_mv: float, unit: str, waveform: str,
                     duty: float = 0.5) -> float:
    """Peak-to-peak millivolts from an amplitude reading.

    The span a capture shows is peak-to-peak, so every reference has to be
    turned into Vpp first, and HOW depends on the waveform:

    * ``pp``  — already peak-to-peak.
    * ``rms`` — a true-RMS DMM on AC.  Sine: Vpp = 2*sqrt(2)*Vrms.  Square of
      duty d: the AC-coupled RMS is Vpp*sqrt(d(1-d)), so at 50 % Vpp = 2*Vrms —
      NOT 2.83*Vrms.  Using the sine factor on a square over-states it 41 %.
    * ``dc``  — a DC reading of a square's high level whose low level is 0 V
      (the Kode Dot's 3V3 rail, ``dc 1`` on its pin): Vpp = Vhigh.  Meaningless
      for a sine, so refused.
    """
    if value_mv <= 0:
        raise BenchError("amplitude reading must be positive, got %r" % (value_mv,))
    if waveform not in WAVEFORMS:
        raise BenchError("waveform must be one of %s, got %r" % (WAVEFORMS, waveform))
    if unit == "pp":
        return float(value_mv)
    if unit == "rms":
        if waveform == "sine":
            return float(value_mv) * 2.0 * float(np.sqrt(2.0))
        if not 0.0 < duty < 1.0:
            raise BenchError("square duty must be inside (0, 1) for an RMS reading")
        return float(value_mv) / float(np.sqrt(duty * (1.0 - duty)))
    if unit == "dc":
        if waveform != "square":
            raise BenchError("a DC level gives a square's Vpp (low = 0 V), not a "
                             "sine's — enter the sine as Vpp or Vrms")
        return float(value_mv)
    raise BenchError("unit must be pp, rms or dc, got %r" % (unit,))


def expected_span_counts(vpp_mv: float, mv_per_count: float) -> float:
    """ADC counts a capture's peak-to-peak span should show for ``vpp_mv``.

    The span of a square IS its Vpp (its plateaus are its extremes), and so is
    a sine's; the waveform enters only through :func:`vpp_from_reading`.  A
    range with no calibration (gain 0) has no expected count and raises."""
    if mv_per_count <= 0:
        raise BenchError("range has no calibration (gain %r mV/count)" % (mv_per_count,))
    return float(vpp_mv) / float(mv_per_count)


# -- the contract -------------------------------------------------------------

class SignalSource:
    """What a bench script needs from a stimulus.  See the block comment above.

    ``tone()`` and ``drive()`` return what the source REPORTS (frequency in Hz,
    amplitude in mVpp), and raise if it did not confirm.  Scripts must use the
    returned value, not the one they asked for — that is the whole of the
    EXP-14 lesson (a generator delivering 0.825x what it was told)."""

    kind = "?"
    label = "source"            # what closing lines call it ("siggen off; ...")
    max_hz: Optional[float] = None
    waveforms: tuple = WAVEFORMS
    default_waveform = "sine"
    #: Fixed amplitude in mVpp, or None if the amplitude is adjustable.
    fixed_vpp_mv: Optional[float] = None
    #: Does quiet() sit at the MIDPOINT of the waveform (a bipolar generator
    #: switched off, the ESP32 parked at 1650 mV) or at its LOW level (a logic
    #: source)?  Centring on the quiet level puts it at code 128, so this
    #: decides how much of the ADC span a drive can use.
    quiet_is_midpoint = True
    #: Should a script go quiet() before `fpga scope center`?  The servo wants
    #: a quiet input (it centres the median).  Esp32Source says no: the
    #: historical flow centred with the previous drive running, and the
    #: maintainer's numbers stay comparable only if that is kept.
    center_on_quiet = True
    #: Can it hold a static low/high level (``hold(0|1)``)?
    can_hold = False
    #: Is the reference amplitude an independent measurement (a DMM), rather
    #: than what the generator was told?  Only then is measured/reference an
    #: accuracy figure.
    trusted_amplitude = False
    #: How many independent outputs.  One means both probes on the same pin,
    #: and the two-SHAPES two-channel control is not available.
    outputs = 1

    def __init__(self):
        self._freq_hz: Optional[float] = None

    @property
    def freq_hz(self) -> Optional[float]:
        """The frequency the source last REPORTED (None before any tone)."""
        return self._freq_hz

    def describe(self) -> str:
        return self.kind

    def can_produce(self, mvpp: float) -> bool:
        return mvpp >= 0

    def prepare_frequency(self, log: Callable[[str], None] = print) -> None:
        """Anything needed before this source's frequencies can be trusted."""

    def end_check(self, log: Callable[[str], None] = print) -> Optional[bool]:
        """Closing control on the source itself; None if it has none."""
        return None

    def tone(self, hz: float, waveform: Optional[str] = None) -> float:
        raise NotImplementedError

    def drive(self, mvpp: float, waveform: Optional[str] = None,
              hz: Optional[float] = None) -> float:
        raise NotImplementedError

    def quiet(self) -> None:
        raise NotImplementedError

    def hold(self, level: int) -> None:
        raise BenchError("%s cannot hold a static level" % self.kind)

    def close(self) -> None:
        pass


class Esp32Source(SignalSource):
    """:class:`Siggen` behind the :class:`SignalSource` contract.

    Does exactly what the scripts used to do inline, so the maintainer's rig
    gives the same numbers: ``tone()`` is CH1 sine at ``amp_mv``, ``drive()``
    is the historical pair — CH1 triangle 250 Hz, CH2 square 400 Hz, two
    different SHAPES — and ``quiet()`` parks both at their 1650 mV midpoint.
    ``Siggen`` itself is untouched."""

    kind = "esp32"
    label = "siggen"
    max_hz = 4500.0             # software DDS; the README's useful ceiling
    default_waveform = "pair"
    waveforms = ("pair",) + WAVEFORMS
    outputs = 2
    center_on_quiet = False     # historical order; see SignalSource
    MAX_VPP_MV = 3300.0         # DAC 0..3.3 V around a 1650 mV midpoint
    PAIR = ((1, "tri", 250.0), (2, "square", 400.0))

    def __init__(self, siggen: Siggen, amp_mv: int = 2000,
                 settle: Optional[float] = None, fs_window: float = 8.0):
        super().__init__()
        self.sg = siggen
        self.amp_mv = amp_mv
        self.fs_window = fs_window
        self._kw = {} if settle is None else {"settle": settle}
        self._fs_start: Optional[float] = None

    def describe(self) -> str:
        return "ESP32 siggen (esp32_siggen/), commanded mVpp"

    def can_produce(self, mvpp: float) -> bool:
        return 0 <= mvpp <= self.MAX_VPP_MV

    def prepare_frequency(self, log=print) -> None:
        # Verbatim from measure_sample_rate.py before the abstraction: park
        # CH2 and put CH1 in the sweep's mode FIRST, then measure the loop
        # rate (each live channel costs ~300 Hz of it), then divide by it.
        self.sg.off(2, **self._kw)
        self.sg.sine(1000, ch=1, **self._kw)
        fs_hz, ratio = self.sg.fs(window=self.fs_window)
        div = self.sg.use_measured_fs(True, window=0.0)
        log(f"source: CH1 sine + CH2 parked -> DDS loop {fs_hz:.1f} Hz "
            f"({ratio:.4f} x nominal); set_freq divides by {div:.1f}")
        if not 0.5 < ratio < 1.05:
            raise SystemExit("source rate ratio %.4f is not credible — stop and look"
                             % ratio)
        self._fs_start = fs_hz

    def end_check(self, log=print) -> Optional[bool]:
        if self._fs_start is None:
            return None
        self.sg.sine(1000, ch=1, **self._kw)
        fs_end, r_end = self.sg.fs(window=self.fs_window)
        drift = abs(fs_end - self._fs_start) / self._fs_start
        ok = drift < 0.002
        log(f"\nsource at end: {fs_end:.1f} Hz ({r_end:.4f}) — drift {drift*100:.2f}%  "
            f"{'PASS' if ok else 'FAIL — rates above are not traceable'}")
        return ok

    def tone(self, hz: float, waveform: Optional[str] = None) -> float:
        if waveform not in (None, "sine"):
            raise BenchError("Esp32Source.tone() is the historical CH1 sine")
        st = self.sg.sine(hz, ch=1, **self._kw)
        self.sg.amp(self.amp_mv, ch=1, **self._kw)
        self._freq_hz = st.freq_hz
        return st.freq_hz

    def drive(self, mvpp: float, waveform: Optional[str] = None,
              hz: Optional[float] = None) -> float:
        if mvpp == 0:
            self.quiet()
            return 0.0
        if not self.can_produce(mvpp):
            raise BenchError("ESP32 siggen cannot produce %g mVpp (0..%g)"
                             % (mvpp, self.MAX_VPP_MV))
        wf = waveform or self.default_waveform
        for ch, pair_shape, pair_hz in self.PAIR:
            shape = pair_shape if wf == "pair" else ("sine" if wf == "sine" else "square")
            getattr(self.sg, shape)(pair_hz, ch=ch, **self._kw)
            self.sg.amp(int(mvpp), ch=ch, **self._kw)
        return float(mvpp)

    def quiet(self) -> None:
        self.sg.off(1, **self._kw)
        self.sg.off(2, **self._kw)

    def close(self) -> None:
        self.sg.close()


# -- Kode Dot -----------------------------------------------------------------

@dataclass(frozen=True)
class KodeDotStatus:
    """What the Kode Dot sigsrc app reports.  ``hz`` is computed by the app from
    the LEDC timer registers read back after configuration —
    clk*256/(div_q8*2^bits), clk = PLL_F80M = X1 40 MHz x12/6 — so it is as
    accurate as the crystal (three Dots measured X1 at +31..+41 ppm)."""
    mode: Optional[str] = None          # pwm | dc_low | dc_high
    hz: Optional[float] = None
    req_hz: Optional[int] = None
    duty_pct: Optional[float] = None
    duty_step_pct: Optional[float] = None
    bits: Optional[int] = None
    div: Optional[float] = None
    timing: Optional[str] = None
    clk_hz: Optional[int] = None
    hz_xtal_corr: Optional[float] = None


# Two reply shapes are accepted.  The app's console framing — records start
# ">|", the command ends ">ok" or ">err msg=..." — e.g.
#   >|freq hz=999.984741 req_hz=1000 err_ppm=-15.259 bits=13 div=9.765625 timing=frac-edge
#   >|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%
#   >ok
# and the bare form the command set was first specified with:
#   freq 999.98 Hz (req 1000, clk 80000000 res 13 div 9765.6)
_KD_FREQ_RE = re.compile(
    r"\bfreq\s+(?:hz=)?(?P<hz>\d+(?:\.\d+)?)(?:\s*Hz)?\b.*?\breq(?:_hz)?[=\s]+(?P<req>\d+)",
    re.IGNORECASE)
_KD_DUTY_RE = re.compile(
    r"\bduty[=\s]+(?P<act>\d+(?:\.\d+)?)\s*%(?:.*?\breq[=\s]+(?P<req>\d+(?:\.\d+)?)\s*%)?"
    r"(?:.*?\bstep=(?P<step>\d+(?:\.\d+)?)\s*%)?", re.IGNORECASE)
_KD_MODE_RE = re.compile(r"\bmode=(\w+)")
_KD_CLK_RE = re.compile(r"\bclk(?:_hz)?[=\s]+(\d+)")
_KD_BITS_RE = re.compile(r"\b(?:bits=|res\s+)(\d+)")
_KD_DIV_RE = re.compile(r"\bdiv[=\s]+(\d+(?:\.\d+)?)")
_KD_TIMING_RE = re.compile(r"\btiming=([\w-]+)")
_KD_CORR_RE = re.compile(r"\bhz_xtal_corr=(\d+(?:\.\d+)?)")
_KD_ERR_RE = re.compile(r"^\s*>err(?:\s+msg=(.*))?$", re.MULTILINE)


def _kd_records(text: str) -> tuple:
    """``(records, framed, ok)``.  Framed replies keep only ``>|`` records, so
    log lines the console interleaves (sweep steps, KLOG) cannot be parsed as
    the answer; a bare reply keeps every non-empty line."""
    lines = [ln.strip() for ln in text.replace("\r", "\n").split("\n") if ln.strip()]
    recs = [ln[2:].strip() for ln in lines if ln.startswith(">|")]
    ok = any(ln == ">ok" for ln in lines)
    framed = bool(recs) or ok or any(ln.startswith(">err") for ln in lines)
    return (recs if framed else lines), framed, ok


def parse_kodedot_reply(text: str) -> KodeDotStatus:
    """Parse any sigsrc reply into a :class:`KodeDotStatus`; absent fields are None."""
    recs, _framed, _ok = _kd_records(text)
    f: dict = {}
    for rec in recs:
        m = _KD_FREQ_RE.search(rec)
        if m and "hz" not in f:
            f["hz"] = float(m.group("hz"))
            f["req_hz"] = int(m.group("req"))
            for key, rx, cast in (("bits", _KD_BITS_RE, int), ("div", _KD_DIV_RE, float),
                                  ("timing", _KD_TIMING_RE, str),
                                  ("hz_xtal_corr", _KD_CORR_RE, float)):
                mm = rx.search(rec)
                if mm:
                    f[key] = cast(mm.group(1))
        m = _KD_DUTY_RE.search(rec)
        if m and "duty_pct" not in f:
            f["duty_pct"] = float(m.group("act"))
            if m.group("step"):
                f["duty_step_pct"] = float(m.group("step"))
        m = _KD_MODE_RE.search(rec)
        if m and "mode" not in f:
            f["mode"] = m.group(1)
        m = _KD_CLK_RE.search(rec)
        if m and "clk_hz" not in f:
            f["clk_hz"] = int(m.group(1))
    return KodeDotStatus(**f)


class _DotSerialTransport(SerialTransport):
    """SerialTransport that never leaves DTR low with RTS high.

    On the P4's USB-Serial-JTAG, DTR=0 with RTS=1 holds the chip in reset,
    which drops the Dot out of the sigsrc app back to kodeOS.  pyserial opens
    with both asserted (DTR first, so it never passes through that state); on
    close, release RTS BEFORE DTR for the same reason."""

    #: kodeOS console terminators: every command ends in exactly one of these.
    _TERMINATORS = (b"\n>ok", b"\n>err")

    def exchange(self, line: str, timeout: float) -> str:
        """Read until the console's ``>ok`` / ``>err`` terminator, not until the
        port goes quiet.  The Dot's log lines (``!I``/``!W``/``!E``) share the
        port and can arrive continuously: on a Dot without its panel assembly
        the LED driver (KTD2026) retries every 40 ms and logs each attempt,
        so a quiet-time read never ends and every command costs its full
        timeout (EXP-63 bring-up, 2026-10-01).  A terminator read is what the
        protocol defines anyway.  On timeout the partial buffer is returned,
        as for any prompt-less device: ``_confirmed`` then refuses a framed
        reply that lacks its ``>ok``."""
        self._ser.reset_input_buffer()
        self._ser.write((line + "\r\n").encode())
        self._ser.flush()
        deadline = time.time() + timeout
        buf = bytearray()
        while time.time() < deadline:
            chunk = self._ser.read(8192)
            if chunk:
                buf += chunk
                tail = buf[-4096:]
                if any(t in tail for t in self._TERMINATORS):
                    # Let the terminator's own line end arrive, then stop.
                    end = time.time() + 0.05
                    while time.time() < end:
                        more = self._ser.read(8192)
                        if more:
                            buf += more
                        if b"\n>ok" in buf[-64:] and buf.endswith(b"\n"):
                            break
                        if b">err" in buf[-256:] and buf.endswith(b"\n"):
                            break
                    return buf.decode("utf-8", "replace")
            else:
                time.sleep(0.005)
        return buf.decode("utf-8", "replace")

    def close(self) -> None:
        try:
            self._ser.rts = False
            self._ser.dtr = False
        except Exception:                              # pragma: no cover
            pass
        super().close()


class KodeDotSource(SignalSource):
    """A Kode Dot running the sigsrc app: LEDC square wave on a J3 header pin.

    Command set: ``f <hz>`` (whole hertz, 1..10 MHz; replies with the ACTUAL
    frequency), ``d <percent>``, ``dc 0|1`` (hold the pin low/high), ``pwm``
    (back to the square), ``s`` (status), ``sweep [stop]``.  Levels are 0 V and
    the 3V3 rail, so the amplitude is fixed: ``v3v3_mv`` must be MEASURED (a
    DMM on the pin with ``dc 1``), and it is the reference Vpp.

    Port: the USB device with Espressif's VID 0x303A, or the one with
    ``serial_number`` when there are several.  Opened with DTR and RTS
    asserted (pyserial's default) — see :class:`_DotSerialTransport`.
    """

    kind = "kodedot"
    label = "Kode Dot"
    max_hz = 10_000_000.0
    waveforms = ("square",)
    default_waveform = "square"
    quiet_is_midpoint = False       # quiet = dc 0 = the LOW rail
    can_hold = True
    trusted_amplitude = True
    outputs = 1
    HZ_MIN, HZ_MAX = 1, 10_000_000
    #: An --amp request within this fraction of the measured rail is "the rail".
    AMP_TOLERANCE = 0.05
    #: Square frequency for drive() when none is given: not a sub-multiple of
    #: any timebase rate on the 1-2.5-5 ladder, so no code samples one phase.
    DRIVE_HZ = 330

    def __init__(self, port: Optional[str] = None, serial_number: Optional[str] = None,
                 baud: int = 115200, transport: Optional[Transport] = None,
                 v3v3_mv: float = 3292.0, settle: float = 0.0,
                 timeout: float = 1.5, check_alive: bool = True):
        super().__init__()
        if v3v3_mv <= 0:
            raise BenchError("v3v3_mv must be the measured high level, > 0")
        self.v3v3_mv = float(v3v3_mv)
        self.fixed_vpp_mv = self.v3v3_mv
        self.settle = settle
        self.timeout = timeout
        self.status_seen: Optional[KodeDotStatus] = None
        self._clk_start: Optional[int] = None
        if transport is not None:
            self._t = transport
        else:
            if port is None:
                port = find_port(ESPRESSIF_VID, serial_number=serial_number,
                                 what="Kode Dot (Espressif VID 0x303A)")
            self._t = _DotSerialTransport(port, baud, prompt=None, settle=0.3,
                                          patterns=(port,), quiet_time=0.15)
        if check_alive:
            st = self.status()
            if st.mode is None and st.hz is None:
                raise BenchError(
                    "the Dot answered `s` without a sigsrc status — is the sigsrc "
                    "app running (not the kodeOS launcher)?")

    def describe(self) -> str:
        return ("Kode Dot LEDC square, 0 V / %.0f mV (DMM-measured rail)"
                % self.v3v3_mv)

    # -- raw ---------------------------------------------------------------

    def send(self, line: str, timeout: Optional[float] = None) -> str:
        """One command; return the raw reply.  Raises on ``>err``."""
        text = self._t.exchange(line, self.timeout if timeout is None else timeout)
        m = _KD_ERR_RE.search(text)
        if m:
            raise BenchError("kodedot refused %r: %s" % (line, (m.group(1) or "").strip()))
        if self.settle:
            time.sleep(self.settle)
        return text

    def _confirmed(self, line: str, text: str) -> KodeDotStatus:
        _recs, framed, ok = _kd_records(text)
        if framed and not ok:
            raise BenchError(
                "kodedot: no '>ok' after %r — reply was:\n%s\n(an unterminated "
                "reply is not treated as success)" % (line, text.strip()))
        return parse_kodedot_reply(text)

    # -- setters, each confirmed from the echo ----------------------------

    def freq(self, hz) -> KodeDotStatus:
        """``f <hz>``.  Whole hertz only — the app refuses fractions, so this
        refuses them first rather than rounding behind the caller's back."""
        if isinstance(hz, float):
            if not hz.is_integer():
                raise BenchError("kodedot takes whole hertz; asked for %r "
                                 "(round it, and use the returned actual)" % hz)
            hz = int(hz)
        if not isinstance(hz, int) or not self.HZ_MIN <= hz <= self.HZ_MAX:
            raise BenchError("kodedot frequency must be %d..%d Hz, got %r"
                             % (self.HZ_MIN, self.HZ_MAX, hz))
        line = "f %d" % hz
        text = self.send(line)
        st = self._confirmed(line, text)
        if st.hz is None or st.req_hz is None:
            raise BenchError("kodedot did not report a frequency for %r — reply:\n%s"
                             % (line, text.strip()))
        if st.req_hz != hz:
            raise BenchError("kodedot: asked for %d Hz, device reports req %d Hz"
                             % (hz, st.req_hz))
        if abs(st.hz - hz) > 0.05 * hz:
            raise BenchError("kodedot: asked for %d Hz, device reports %.6f Hz actual"
                             % (hz, st.hz))
        self._freq_hz = st.hz
        return st

    def duty(self, pct: float) -> KodeDotStatus:
        """``d <percent>``; the app quantises to the timer's resolution and says
        so, so the check allows one reported step (or 0.5 % if none is given)."""
        if not 0.0 <= pct <= 100.0:
            raise BenchError("duty must be 0..100 %%, got %r" % (pct,))
        line = "d %g" % pct
        text = self.send(line)
        st = self._confirmed(line, text)
        if st.duty_pct is None:
            raise BenchError("kodedot did not report a duty for %r — reply:\n%s"
                             % (line, text.strip()))
        tol = max(st.duty_step_pct or 0.0, 0.5)
        if abs(st.duty_pct - pct) > tol:
            raise BenchError("kodedot: asked for %g %% duty, device reports %g %%"
                             % (pct, st.duty_pct))
        return st

    def dc(self, level: int) -> KodeDotStatus:
        """``dc 0|1`` — hold the pin at the low or high rail."""
        if level not in (0, 1):
            raise BenchError("dc level must be 0 or 1, got %r" % (level,))
        line = "dc %d" % level
        text = self.send(line)
        st = self._confirmed(line, text)
        want = "dc_high" if level else "dc_low"
        if st.mode != want:
            raise BenchError("kodedot: asked for %s, device reports mode=%s — reply:\n%s"
                             % (want, st.mode, text.strip()))
        return st

    def pwm(self) -> KodeDotStatus:
        """``pwm`` — back to the square wave after ``dc``."""
        text = self.send("pwm")
        st = self._confirmed("pwm", text)
        if st.hz is None:
            raise BenchError("kodedot did not confirm `pwm` — reply:\n%s" % text.strip())
        self._freq_hz = st.hz
        return st

    def status(self) -> KodeDotStatus:
        """``s`` — parsed; fields the reply lacks are None."""
        text = self.send("s")
        st = self._confirmed("s", text)
        if st.hz is not None:
            self._freq_hz = st.hz
        self.status_seen = st
        return st

    def sweep(self, stop: bool = False) -> str:
        """``sweep`` / ``sweep stop``.  Returns the reply; steps after the first
        arrive later as log lines, which is why scripts set tones with ``f``."""
        line = "sweep stop" if stop else "sweep"
        text = self.send(line)
        self._confirmed(line, text)
        return text

    # -- SignalSource ------------------------------------------------------

    def can_produce(self, mvpp: float) -> bool:
        return mvpp == 0 or abs(mvpp - self.v3v3_mv) <= self.AMP_TOLERANCE * self.v3v3_mv

    def prepare_frequency(self, log=print) -> None:
        st = self.status()
        self._clk_start = st.clk_hz
        self.duty(50)
        clk = ("clk %d Hz" % st.clk_hz) if st.clk_hz else "clock not reported"
        log("source: Kode Dot LEDC square (%s; PLL_F80M = X1 40 MHz x12/6); every "
            "frequency below is the one its timer registers produce" % clk)

    def end_check(self, log=print) -> Optional[bool]:
        st = self.status()
        same = self._clk_start is None or st.clk_hz in (None, self._clk_start)
        log("\nsource at end: Kode Dot %s — crystal-derived, no loop to drift; X1 "
            "error (+31..+41 ppm on three Dots) is below this method's resolution  %s"
            % ("clk %d Hz" % st.clk_hz if st.clk_hz else "status OK",
               "PASS" if same else "FAIL — the clock changed during the run"))
        return same

    def tone(self, hz: float, waveform: Optional[str] = None) -> float:
        if waveform not in (None, "square"):
            raise BenchError("kodedot produces a square wave only, not %r" % waveform)
        return self.freq(int(round(hz))).hz

    def drive(self, mvpp: float, waveform: Optional[str] = None,
              hz: Optional[float] = None) -> float:
        if waveform not in (None, "square"):
            raise BenchError("kodedot produces a square wave only, not %r" % waveform)
        if mvpp == 0:
            self.quiet()
            return 0.0
        if not self.can_produce(mvpp):
            raise BenchError("kodedot cannot produce %g mVpp: its only amplitude is "
                             "its rail, %.0f mVpp" % (mvpp, self.v3v3_mv))
        st = self.freq(int(round(hz or self.DRIVE_HZ)))
        if st.duty_pct is not None and \
                abs(st.duty_pct - 50.0) > max(st.duty_step_pct or 0.0, 0.5):
            self.duty(50)
        return self.v3v3_mv

    def quiet(self) -> None:
        self.dc(0)

    def hold(self, level: int) -> None:
        self.dc(level)

    def close(self) -> None:
        self._t.close()


# -- a person with a generator, a counter and a DMM ----------------------------

class ManualSource(SignalSource):
    """Any bench generator, driven by the operator at the keyboard.

    Each step prints what to set, waits for Enter, then asks for what the
    instruments READ — the counter's frequency, the DMM's amplitude with its
    unit — and returns that.  Unparseable or implausible input is asked for
    again (``attempts`` times), never guessed.  ``input_fn`` is injectable so
    tests and ``--dry-run`` can answer for the operator; ``pending`` and
    ``stage`` say what is being asked when it is called.
    """

    kind = "manual"
    label = "generator"
    trusted_amplitude = True        # it is a measurement, if the DMM is good
    #: A typed frequency further than this from the request is assumed a typo.
    FREQ_TOLERANCE = 0.10
    DRIVE_HZ = 330

    def __init__(self, waveform: str = "sine", input_fn: Callable[[str], str] = input,
                 print_fn: Callable[[str], None] = print, attempts: int = 3,
                 duty: float = 0.5):
        super().__init__()
        if waveform not in WAVEFORMS:
            raise BenchError("waveform must be one of %s" % (WAVEFORMS,))
        self.waveform = waveform
        self.default_waveform = waveform
        self.input_fn = input_fn
        self.print_fn = print_fn
        self.attempts = attempts
        self.duty = duty
        self.pending: dict = {}
        self.stage = ""
        self._last_amp: dict = {}

    def describe(self) -> str:
        return "manual generator (%s), operator-entered counter/DMM readings" % self.waveform

    def _ask(self, stage: str, prompt: str) -> str:
        self.stage = stage
        return self.input_fn(prompt)

    def _ask_parsed(self, stage: str, prompt: str, parse, check, explain: str):
        for _ in range(self.attempts):
            raw = self._ask(stage, prompt)
            val = parse(raw)
            if val is None:
                self.print_fn("  could not read %r — %s" % (raw, explain))
                continue
            problem = check(val)
            if problem:
                self.print_fn("  %s" % problem)
                continue
            return val
        raise BenchError("manual source: no usable answer after %d attempts" % self.attempts)

    def tone(self, hz: float, waveform: Optional[str] = None) -> float:
        wf = waveform or self.waveform
        self.pending = {"what": "tone", "hz": float(hz), "waveform": wf}
        self._ask("set", "[manual] set the generator to %g Hz %s, output ON; press Enter "
                  "when it is running: " % (hz, wf))

        def check(v):
            if abs(v - hz) > self.FREQ_TOLERANCE * hz:
                return ("%g Hz is %.0f%% from the %g Hz asked for — a typo is likelier "
                        "than a generator that far off; set it again and re-read"
                        % (v, 100.0 * abs(v - hz) / hz, hz))
            return None
        actual = self._ask_parsed(
            "freq", "[manual] the ACTUAL frequency your counter reads, in Hz: ",
            parse_hz_text, check, "type a number such as 999.98, 1k or 1.0001 kHz")
        self._freq_hz = float(actual)
        return float(actual)

    def drive(self, mvpp: float, waveform: Optional[str] = None,
              hz: Optional[float] = None) -> float:
        wf = waveform or self.waveform
        if mvpp == 0:
            self.quiet()
            return 0.0
        hz = hz or self.DRIVE_HZ
        self.pending = {"what": "drive", "mvpp": float(mvpp), "hz": float(hz),
                        "waveform": wf}
        self._ask("set", "[manual] set the generator to about %g mVpp %s at %g Hz, centred "
                  "on its quiet level; press Enter: " % (mvpp, wf, hz))
        last = self._last_amp.get((round(mvpp), wf))
        hint = ("square: 3.292 V = a high level over a 0 V low"
                if wf == "square" else "sine: give Vpp or Vrms")
        prompt = ("[manual] the amplitude your DMM/scope reads, with its unit (e.g. "
                  "2.000 Vpp, 0.707 Vrms; %s)%s: "
                  % (hint, " [Enter = %.1f mVpp again]" % last if last else ""))

        def parse(raw):
            if last and not (raw or "").strip():
                return (last, "pp")
            return parse_amplitude_text(raw)

        def check(reading):
            try:
                vpp = vpp_from_reading(reading[0], reading[1], wf, self.duty)
            except BenchError as exc:
                return str(exc)
            if not 0.5 * mvpp <= vpp <= 2.0 * mvpp:
                return ("that is %.0f mVpp, far from the %g mVpp asked for — check the "
                        "unit (Vpp / Vrms / V) and re-enter" % (vpp, mvpp))
            return None
        reading = self._ask_parsed("amp", prompt, parse, check,
                                   "give a number AND a unit: Vpp, mVpp, Vrms, mVrms or V")
        vpp = vpp_from_reading(reading[0], reading[1], wf, self.duty)
        self._last_amp[(round(mvpp), wf)] = vpp
        return vpp

    def quiet(self) -> None:
        if self.pending.get("what") == "quiet":
            return                  # already quiet: do not make the operator re-confirm
        self.pending = {"what": "quiet"}
        self._ask("set", "[manual] make the output QUIET at the waveform's midpoint (output "
                  "off for a generator centred on 0 V); press Enter: ")


# -- the --source CLI convention ----------------------------------------------

def add_source_args(ap: argparse.ArgumentParser, default: str = "esp32") -> None:
    """Add the shared bench-device flags to a script's parser.

    ``--source esp32|kodedot|manual``, ``--source-port`` (``--siggen-port`` is
    kept as an alias), ``--source-serial``, ``--scope-port``, ``--v3v3`` and
    ``--dry-run``.  Ports default to discovery: the scope by USB VID:PID
    2e3c:5740, a Kode Dot by Espressif's VID 0x303A, the ESP32 siggen by its
    USB-serial glob as before."""
    g = ap.add_argument_group("bench devices")
    g.add_argument("--source", choices=SOURCE_KINDS, default=default,
                   help="stimulus: esp32 = the ESP32 siggen sketch; kodedot = a Kode "
                        "Dot running sigsrc (square, crystal-derived, fixed "
                        "amplitude); manual = any generator, the operator types what "
                        "a counter/DMM reads (default: %(default)s)")
    g.add_argument("--source-port", "--siggen-port", dest="source_port", default=None,
                   help="source serial port (default: discover — kodedot by USB VID "
                        "0x303A, esp32 by /dev/ttyUSB* | /dev/cu.usbserial*)")
    g.add_argument("--source-serial", default=None,
                   help="kodedot: USB serial number, to pick one of several "
                        "Espressif devices")
    g.add_argument("--scope-port", default=None,
                   help="2C53T debug shell (default: discover by USB VID:PID "
                        "%04x:%04x)" % SCOPE_USB_ID)
    g.add_argument("--v3v3", type=float, default=3.292, metavar="VOLTS",
                   help="kodedot: the pin's high level, MEASURED with a DMM (`dc 1`); "
                        "the low level is taken as 0 V (default: %(default)s)")
    g.add_argument("--dry-run", action="store_true",
                   help="run the whole flow against a simulated scope and source; "
                        "opens no port")


def open_scope(args) -> Scope:
    port = args.scope_port or find_port(*SCOPE_USB_ID, what="2C53T debug shell")
    return Scope(port)


def open_source(args, waveform: Optional[str] = None,
                input_fn: Callable[[str], str] = input,
                print_fn: Callable[[str], None] = print) -> SignalSource:
    if args.source == "esp32":
        return Esp32Source(Siggen(args.source_port))
    if args.source == "kodedot":
        return KodeDotSource(port=args.source_port, serial_number=args.source_serial,
                             v3v3_mv=args.v3v3 * 1000.0)
    if args.source == "manual":
        return ManualSource(waveform=waveform or "sine", input_fn=input_fn,
                            print_fn=print_fn)
    raise BenchError("unknown source %r" % (args.source,))


def open_bench(args, waveform: Optional[str] = None, gains: Optional[dict] = None):
    """``(scope, source, sim)`` for a script's parsed ``args``.

    With ``--dry-run`` both ends are a :class:`SimBench` and ``sim`` is it;
    otherwise ``sim`` is None and real ports are opened (scope first, so a
    missing scope fails before the source is touched)."""
    if getattr(args, "dry_run", False):
        sim = SimBench(source=args.source, v3v3_mv=args.v3v3 * 1000.0, gains=gains)
        return sim.scope(), sim.source(waveform=waveform), sim
    scope = open_scope(args)
    try:
        return scope, open_source(args, waveform=waveform), None
    except BaseException:
        scope.close()
        raise


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

def spectrum(v: Sequence[float], detrend: bool = True) -> np.ndarray:
    """rFFT magnitude of ``v``, normalised by N, with DC zeroed.

    Normalisation matches every recorded number in ``docs/experiments/`` (a
    2 Vpp tone reads ~22 in these units on the bench frontend), so results stay
    comparable across sessions.  Do not "fix" it to N/2.
    """
    x = np.asarray(v, dtype=float)
    if x.size < 8:
        raise BenchError("spectrum needs >= 8 samples, got %d" % x.size)

    # Refuse a magnitude spectrum handed back in as if it were samples.
    #
    # peaks() and band() call spectrum() themselves, so `peaks(spectrum(v))`
    # transforms twice. That is not a subtle error with a subtle symptom: the
    # second transform of a clean tone returns bin 1, magnitude ~0.04, for ANY
    # input frequency. It looked exactly like "the capture cannot resolve
    # frequency", and it stood as a recorded hardware finding (EXP-08 s6, and
    # from there CLAUDE.md) until EXP-10 reproduced it synthetically.
    #
    # The tell is unambiguous: rfft output has ODD length (n/2+1 for even n),
    # is entirely non-negative, and has had DC zeroed by this function. A raw
    # capture is 1024 samples — even — so this cannot fire on one.
    if (x.size % 2 == 1 and x[0] == 0.0 and np.all(x >= 0.0)
            and float(np.max(x)) > 0.0):
        raise BenchError(
            "spectrum() was handed what looks like a magnitude spectrum "
            "(odd length %d, non-negative, DC exactly zero), not samples. "
            "peaks() and band() already call spectrum() internally — pass the "
            "RAW record: peaks(v), not peaks(spectrum(v))." % x.size)

    if detrend:
        x = x - x.mean()
    mag = np.abs(np.fft.rfft(x)) / x.size
    mag[0] = 0.0
    return mag


def peaks(v: Sequence[float], k: int = 3) -> list:
    """Top-``k`` spectral peaks as ``[(bin, magnitude), ...]``, DC excluded.

    **A peak SEARCH, never a fixed bin.**  The sample rate on this platform
    drifts enough between runs to move a tone a whole bin: a fixed-bin detector
    once reported **1.44** where a peak search on the same capture found
    **22.8** — a stable, plausible, wrong number that looked like an absent
    signal.  If you know roughly where a tone should be, use :func:`band`.
    """
    if k < 1:
        raise BenchError("k must be >= 1")
    mag = spectrum(v)
    idx = np.argsort(mag)[::-1][:k]
    idx = sorted(idx, key=lambda j: -mag[j])
    return [(int(i), round(float(mag[i]), 2)) for i in idx]


def band(v: Sequence[float], lo: int, hi: int) -> float:
    """Maximum magnitude in the **inclusive bin window** ``[lo, hi]``.

    This is the safe alternative to a fixed bin.  ``hi`` must be strictly
    greater than ``lo``: a zero-width window IS a fixed-bin detector, and that
    detector reported 1.44 for a tone whose true magnitude was 22.8 because the
    sample rate had drifted the tone out of the bin being watched.  Pick the
    window with :func:`window_for`.
    """
    lo, hi = int(lo), int(hi)
    if hi <= lo:
        raise BenchError(
            "band() needs a WINDOW (hi > lo); [%d,%d] is a fixed-bin detector. "
            "The sample rate drifts enough between runs to move a tone a whole "
            "bin — a fixed bin once reported 1.44 for a 22.8 tone. Use "
            "window_for(hz, fs, n) to size the window, or peaks() to search."
            % (lo, hi))
    mag = spectrum(v)
    if lo < 0 or hi >= mag.size:
        raise BenchError("window [%d,%d] outside 0..%d" % (lo, hi, mag.size - 1))
    return round(float(mag[lo:hi + 1].max()), 2)


def band_peak(v: Sequence[float], lo: int, hi: int) -> tuple:
    """``(bin, magnitude)`` of the strongest bin in ``[lo, hi]`` inclusive.

    Reporting WHICH bin won is worth the extra value: if the winner is pinned
    to an edge of your window, the tone has walked out of it and the number is
    a lower bound, not a measurement."""
    lo, hi = int(lo), int(hi)
    if hi <= lo:
        raise BenchError("band_peak() needs a window (hi > lo) — see band().")
    mag = spectrum(v)
    if lo < 0 or hi >= mag.size:
        raise BenchError("window [%d,%d] outside 0..%d" % (lo, hi, mag.size - 1))
    j = int(np.argmax(mag[lo:hi + 1])) + lo
    return j, round(float(mag[j]), 2)


def bin_of(hz: float, fs_hz: float, n: int = STOCK_SAMPLES) -> float:
    """Fractional FFT bin a tone of ``hz`` lands in at sample rate ``fs_hz``."""
    if fs_hz <= 0 or n <= 0:
        raise BenchError("fs and n must be positive")
    return hz * n / fs_hz


def window_for(hz: float, fs_hz: float, n: int = STOCK_SAMPLES,
               tol: float = 0.25, pad: int = 2) -> tuple:
    """Bin window ``(lo, hi)`` around ``hz``, allowing ``tol`` relative error.

    ``tol`` defaults to 25%, which covers the measured 0.82 nominal-to-actual
    factor on the ESP32 source plus the platform's own sample-rate drift.
    ``pad`` widens by a fixed number of bins so a low-frequency tone still gets
    a real window.  Always at least 2 bins wide, which is what :func:`band`
    requires."""
    centre = bin_of(hz, fs_hz, n)
    lo = int(max(1, np.floor(centre * (1 - tol)) - pad))
    hi = int(np.ceil(centre * (1 + tol)) + pad)
    if hi <= lo:
        hi = lo + 2
    return lo, hi


def fixed_bin(*_args, **_kwargs):
    """Deliberately unimplemented.  A fixed-bin detector is forbidden here.

    On 2026-08-16 a detector watching one hard-coded bin reported a magnitude
    of **1.44** for a tone that a peak search on the same capture measured at
    **22.8**: the sample rate had drifted enough between runs to move the tone
    a whole bin, and the detector faithfully reported the empty bin next to it.
    Nothing about that number looked wrong.  Use :func:`peaks` to search, or
    :func:`band` / :func:`window_for` for a windowed maximum.
    """
    raise BenchError(
        "fixed-bin detection is forbidden: a fixed bin reported 1.44 where a "
        "peak search found 22.8, because the sample rate drifted the tone into "
        "the next bin. Use peaks() or band(v, lo, hi).")


# ---------------------------------------------------------------------------
# Paired-difference statistics
# ---------------------------------------------------------------------------

@dataclass(frozen=True, eq=False)   # eq=False: `deltas` is an ndarray, and a
                                    # generated __eq__/__hash__ would raise on it
class PairedStats:
    """Outcome of a paired A-B-A design.  Not a conclusion — see the warning.

    A ``PairedStats`` on its own is HALF an experiment.  The design is only
    valid alongside an identical-opcode control (A,A,A) run through the same
    path in the same session; :func:`paired_experiment` runs both and returns a
    :class:`Result` whose verdict accounts for it."""
    n: int
    mean: float
    sd: float
    se: float
    t: float
    deltas: np.ndarray = field(repr=False)
    skipped: int = 0

    @property
    def significant(self) -> bool:
        return abs(self.t) >= 3.0

    def __str__(self) -> str:
        return ("mean %+7.3f  sd %6.3f  se %6.3f  t=%+7.2f  (n=%d%s)"
                % (self.mean, self.sd, self.se, self.t, self.n,
                   ", %d skipped" % self.skipped if self.skipped else ""))


def _stat_array(fn, arr) -> float:
    val = float(fn(arr))
    if not np.isfinite(val):
        raise BenchError("statistic returned %r" % val)
    return val


def paired_difference(read_a: Callable[[], np.ndarray],
                      read_b: Callable[[], np.ndarray],
                      n: int = 20,
                      stat: Callable = np.mean,
                      min_len: int = 1000,
                      progress: Optional[Callable[[int, int], None]] = None
                      ) -> PairedStats:
    """Drift-cancelling paired difference between two readers.

    Each trial reads **A, B, A** and compares B against the **midpoint** of the
    two A reads.  Because the midpoint of two samples taken either side of B is
    exactly what a linear drift predicts at B's instant, any linear drift
    cancels identically, and a residual is evidence of a genuine difference
    between the two sources.  This is the design that settled the
    two-converters question (EXP-01): op05 sat 2.6 codes below the drift-
    cancelled op04 with t = -357, while the control sat at -0.003.

    **This design REQUIRES an identical-opcode control run (A, A, A) to be
    valid.**  Without it the statistic cannot distinguish "B is a different
    source" from "the second read in any triple is systematically different" —
    a read-ORDER artifact, e.g. a per-read phase advance that biases the mean
    of a periodic window.  Run :func:`paired_control`, or better, use
    :func:`paired_experiment`, which runs the control first and refuses to
    report a bare negative.

    Parameters
    ----------
    read_a, read_b
        Zero-argument callables returning arrays.  ``Scope.reader(0x04)`` makes
        one.  ``read_b`` may be ``read_a`` — that is exactly the control.
    n
        Trials.  Each costs three reads.
    stat
        Per-window statistic.  ``np.mean`` measures offset; ``np.std`` measures
        gain.  Any callable array→float works.
    min_len
        Trials where any of the three windows is shorter are skipped and
        counted, never silently truncated.
    """
    if n < 3:
        raise BenchError("paired_difference needs n >= 3 trials, got %d" % n)
    deltas: list[float] = []
    skipped = 0
    for i in range(n):
        a = np.asarray(read_a(), dtype=float)
        b = np.asarray(read_b(), dtype=float)
        c = np.asarray(read_a(), dtype=float)
        if min(a.size, b.size, c.size) < min_len:
            skipped += 1
            continue
        mid = (_stat_array(stat, a) + _stat_array(stat, c)) / 2.0
        deltas.append(_stat_array(stat, b) - mid)
        if progress:
            progress(i + 1, n)
    if len(deltas) < 3:
        raise BenchError(
            "only %d usable trials out of %d (%d skipped as short reads) — "
            "not enough to report" % (len(deltas), n, skipped))
    d = np.asarray(deltas, dtype=float)
    mean = float(d.mean())
    # ddof=1: the sample standard deviation, so `se` is a real standard error.
    # Earlier inline scripts used the population sd (statistics.pstdev), which
    # inflates t by sqrt(n/(n-1)) — ~2.6% at n=20. Numbers here are therefore
    # very slightly more conservative than the ones recorded in EXP-01.
    sd = float(d.std(ddof=1))
    se = sd / np.sqrt(d.size)
    t = mean / se if se > 0 else 0.0
    return PairedStats(n=int(d.size), mean=mean, sd=sd, se=float(se),
                       t=float(t), deltas=d, skipped=skipped)


def paired_control(read_a: Callable[[], np.ndarray], n: int = 20,
                   stat: Callable = np.mean, min_len: int = 1000,
                   progress: Optional[Callable[[int, int], None]] = None
                   ) -> PairedStats:
    """The identical-opcode control for :func:`paired_difference`: A, A, A.

    This is a first-class function, not an afterthought, because the paired
    design is *invalid without it*.  It runs the identical code path, read
    order, cadence and drift as the test — the only change is that the middle
    read uses the same source.  Expected outcome: mean ~0, |t| small.  If it is
    not small, the instrument has a read-order artifact and the test result is
    VOID, not negative.
    """
    return paired_difference(read_a, read_a, n=n, stat=stat, min_len=min_len,
                             progress=progress)


# ---------------------------------------------------------------------------
# Evidence types — where uncontrolled negatives get blocked
# ---------------------------------------------------------------------------

class Verdict:
    """Verdict strings.  ``VOID`` is not a value you can assign — see
    :class:`Result`, which computes it."""
    POSITIVE = "POSITIVE"
    NEGATIVE = "NEGATIVE"
    INCONCLUSIVE = "INCONCLUSIVE"
    VOID = "VOID"

    OBSERVABLE = (POSITIVE, NEGATIVE, INCONCLUSIVE)


@dataclass(frozen=True)
class Control:
    """A known-good case, run in the same session through the same path.

    ``expected`` and ``measured`` are free text on purpose: a control is
    evidence you have to be able to read six months later.  ``passed`` is the
    only machine-readable part, and it is what decides whether an accompanying
    negative is recordable at all."""
    name: str
    expected: str
    measured: str
    passed: bool

    @classmethod
    def positive(cls, name: str, expected: str, measured: str, passed: bool) -> "Control":
        """A control that must SHOW something: 'the detector can see a tone at
        bin 17 at all'.  This is the one this project keeps skipping."""
        return cls(name, expected, measured, bool(passed))

    @classmethod
    def null(cls, name: str, expected: str, measured: str, passed: bool) -> "Control":
        """A control that must show NOTHING: the identical-opcode A,A,A run."""
        return cls(name, expected, measured, bool(passed))

    def __str__(self) -> str:
        return "%s %-44s expected %-20s measured %-24s" % (
            "PASS" if self.passed else "FAIL", self.name, self.expected, self.measured)

    def row(self) -> str:
        """One row of the experiment file's control table."""
        return "| %s | %s | %s | %s |" % (
            self.name, self.expected, self.measured, "yes" if self.passed else "**no**")


@dataclass(frozen=True, eq=False)   # eq=False: `data` is a dict; a generated
                                    # __hash__ would raise. Results are compared
                                    # by reading them, not by ==.
class Result:
    """A measurement plus the controls that decide whether it means anything.

    **There is no settable verdict.**  :attr:`verdict` is derived:

    ============================  ==========================================
    situation                     verdict
    ============================  ==========================================
    any attached control FAILED   ``VOID`` (the skill's rule: a failed
                                  control makes the experiment void, not
                                  negative)
    NEGATIVE/INCONCLUSIVE with
    no controls at all            ``VOID`` — an uncontrolled negative is
                                  unrecordable, because "X produced no
                                  signal" means nothing until something has
                                  produced a signal through that exact
                                  instrument
    POSITIVE with no controls     ``POSITIVE``, flagged "(uncontrolled)" —
                                  a signal is at least self-evidencing, but
                                  it is still marked
    otherwise                     the observed outcome
    ============================  ==========================================

    So the way to record a negative is to run the control.  There is no other
    way, and no flag to override it.
    """
    name: str
    observed: str
    detail: str = ""
    controls: tuple = ()
    data: dict = field(default_factory=dict)
    blind_spots: tuple = ()

    def __post_init__(self):
        if self.observed not in Verdict.OBSERVABLE:
            raise BenchError(
                "observed must be one of %s (VOID is computed from the "
                "controls, never asserted)" % (Verdict.OBSERVABLE,))
        ctrls = tuple(self.controls)
        for c in ctrls:
            if not isinstance(c, Control):
                raise BenchError("controls must be Control instances, got %r" % (c,))
        object.__setattr__(self, "controls", ctrls)
        object.__setattr__(self, "blind_spots", tuple(self.blind_spots))
        if self.verdict == Verdict.VOID:
            sys.stderr.write("bench: VOID result %r — %s\n" % (self.name, self.void_reason))

    # -- derived -----------------------------------------------------------

    @property
    def failed_controls(self) -> tuple:
        return tuple(c for c in self.controls if not c.passed)

    @property
    def verdict(self) -> str:
        if self.failed_controls:
            return Verdict.VOID
        if not self.controls and self.observed != Verdict.POSITIVE:
            return Verdict.VOID
        return self.observed

    @property
    def void_reason(self) -> str:
        if self.failed_controls:
            return ("control failed: %s — a failed control makes the experiment "
                    "VOID, not negative"
                    % ", ".join(c.name for c in self.failed_controls))
        if not self.controls and self.observed != Verdict.POSITIVE:
            return ("no control was run; '%s' through an instrument never shown "
                    "able to detect the thing is not evidence" % self.observed.lower())
        return ""

    @property
    def uncontrolled(self) -> bool:
        return not self.controls

    # -- rendering ---------------------------------------------------------

    def __str__(self) -> str:
        head = "%s: %s" % (self.name, self.verdict)
        if self.verdict == Verdict.VOID:
            head += "  <- %s" % self.void_reason
        elif self.uncontrolled:
            head += " (uncontrolled)"
        lines = [head]
        if self.detail:
            lines.append("    %s" % self.detail)
        for c in self.controls:
            lines.append("    control: %s" % c)
        for b in self.blind_spots:
            lines.append("    blind spot: %s" % b)
        return "\n".join(lines)

    def markdown(self) -> str:
        """Sections 4/5/7 of a ``docs/experiments/`` file, ready to paste."""
        out = ["## 4. Control", "",
               "| control | expected | measured | passed? |",
               "|---|---|---|---|"]
        if self.controls:
            out += [c.row() for c in self.controls]
        else:
            out.append("| **none run** | — | — | **no** |")
        out += ["", "## 5. Results", "", "- **%s** — %s" % (self.name, self.detail or "—")]
        for k, v in sorted(self.data.items()):
            out.append("  - `%s` = %s" % (k, v))
        if self.blind_spots:
            out += ["", "## 6. Blind spots", ""] + ["- %s" % b for b in self.blind_spots]
        out += ["", "## 7. Conclusion", "",
                "- **Verdict: %s**%s" % (
                    self.verdict,
                    "" if self.verdict != Verdict.VOID else " — %s" % self.void_reason)]
        return "\n".join(out)


class Experiment:
    """Collects controls and results for one bench cycle.

    Controls registered here are attached automatically to every result the
    experiment records afterwards, which is the "make controls easy" half of
    the design; :class:`Result` is the "make uncontrolled negatives awkward"
    half.  Order matters and mirrors the skill: register the control, having
    run it, *before* recording the result it licenses.

    ::

        exp = Experiment("EXP-04 — does reg 0x06 gate CH2?",
                         unit="bench unit #1", build="guest-coldtrace-slow")
        exp.control("detector sees 250 Hz on the working jack",
                    "peak near bin 17", "bin 17 @ 23.4", passed=True)
        exp.negative("reg 0x06 sweep 0..3", "both buffers unchanged, band 21.9-22.1")
        print(exp.summary())
    """

    def __init__(self, title: str, unit: str = "", build: str = "",
                 date: Optional[str] = None):
        self.title = title
        self.unit = unit
        self.build = build
        self.date = date or time.strftime("%Y-%m-%d")
        self.controls: list[Control] = []
        self.results: list[Result] = []

    # -- controls ----------------------------------------------------------

    def control(self, name: str, expected: str, measured: str, passed: bool) -> Control:
        c = Control(name, expected, measured, bool(passed))
        self.controls.append(c)
        return c

    def add_control(self, c: Control) -> Control:
        if not isinstance(c, Control):
            raise BenchError("add_control takes a Control")
        self.controls.append(c)
        return c

    @property
    def controls_ok(self) -> bool:
        return bool(self.controls) and all(c.passed for c in self.controls)

    # -- results -----------------------------------------------------------

    def record(self, name: str, observed: str, detail: str = "",
               data: Optional[dict] = None,
               blind_spots: Iterable[str] = ()) -> Result:
        r = Result(name=name, observed=observed, detail=detail,
                   controls=tuple(self.controls), data=dict(data or {}),
                   blind_spots=tuple(blind_spots))
        self.results.append(r)
        return r

    def positive(self, name: str, detail: str = "", **kw) -> Result:
        return self.record(name, Verdict.POSITIVE, detail, **kw)

    def negative(self, name: str, detail: str = "", **kw) -> Result:
        """Record a negative.  Renders as VOID unless a control has passed."""
        return self.record(name, Verdict.NEGATIVE, detail, **kw)

    def inconclusive(self, name: str, detail: str = "", **kw) -> Result:
        return self.record(name, Verdict.INCONCLUSIVE, detail, **kw)

    # -- rendering ---------------------------------------------------------

    def summary(self) -> str:
        head = "%s\n%s" % (self.title, "=" * len(self.title))
        meta = "  %s   unit: %s   build: %s" % (self.date, self.unit or "?",
                                                self.build or "?")
        lines = [head, meta, ""]
        lines.append("controls:")
        if self.controls:
            lines += ["  " + str(c) for c in self.controls]
        else:
            lines.append("  NONE — any negative recorded here will be VOID")
        lines += ["", "results:"]
        lines += ["  " + str(r).replace("\n", "\n  ") for r in self.results] or ["  none"]
        void = [r for r in self.results if r.verdict == Verdict.VOID]
        if void:
            lines += ["", "%d of %d results are VOID." % (len(void), len(self.results))]
        return "\n".join(lines)

    def markdown(self) -> str:
        out = ["# %s" % self.title, "",
               "- **Date:** %s" % self.date,
               "- **Unit:** %s" % (self.unit or "?"),
               "- **Build:** %s" % (self.build or "?"),
               "- **Status:** %s" % (", ".join(r.verdict for r in self.results) or "—"),
               ""]
        for r in self.results:
            out += [r.markdown(), ""]
        return "\n".join(out)


def paired_experiment(read_a: Callable[[], np.ndarray],
                      read_b: Callable[[], np.ndarray],
                      n: int = 20,
                      stat: Callable = np.mean,
                      t_crit: float = 3.0,
                      label: str = "A vs B",
                      min_len: int = 1000,
                      experiment: Optional[Experiment] = None,
                      progress: Optional[Callable[[int, int], None]] = None
                      ) -> Result:
    """Run the control FIRST, then the test, and return a :class:`Result`.

    This is the complete form of the EXP-01 design and the one you should reach
    for.  It runs :func:`paired_control` (A, A, A) before the test (A, B, A),
    attaches it as a null control, and lets :class:`Result` decide the verdict —
    so a null test result with a misbehaving instrument comes out VOID rather
    than being written down as "no difference".

    ``t_crit`` is used twice: the control PASSES if ``|t| < t_crit`` (no
    read-order artifact) and the test is POSITIVE if ``|t| >= t_crit``.
    """
    ctrl = paired_control(read_a, n=n, stat=stat, min_len=min_len, progress=progress)
    control = Control.null(
        name="identical-opcode control (A,A,A) — %s" % label,
        expected="|t| < %.1f" % t_crit,
        measured="mean %+.3f, t=%+.2f (n=%d)" % (ctrl.mean, ctrl.t, ctrl.n),
        passed=abs(ctrl.t) < t_crit)

    test = paired_difference(read_a, read_b, n=n, stat=stat, min_len=min_len,
                             progress=progress)
    observed = Verdict.POSITIVE if abs(test.t) >= t_crit else Verdict.NEGATIVE

    blind = (
        "cannot say WHICH source B is, only that it does or does not differ from A",
        "cannot separate 'different source' from 'same source through a different "
        "digital path applying a constant %s offset'" % getattr(stat, "__name__", "stat"),
    )
    controls = tuple(experiment.controls) + (control,) if experiment else (control,)
    result = Result(
        name="paired difference: %s (stat=%s)" % (label, getattr(stat, "__name__", stat)),
        observed=observed,
        detail="test %s | control %s" % (test, ctrl),
        controls=controls,
        data={"test_mean": round(test.mean, 4), "test_t": round(test.t, 2),
              "test_n": test.n, "control_mean": round(ctrl.mean, 4),
              "control_t": round(ctrl.t, 2), "control_n": ctrl.n},
        blind_spots=blind)
    if experiment is not None:
        experiment.add_control(control)
        experiment.results.append(result)
    return result


# ---------------------------------------------------------------------------
# Self-test — no hardware
# ---------------------------------------------------------------------------

def _synth_dump(values: Sequence[int], per_line: int = 16, drop_line: int = -1) -> str:
    """Render bytes as the firmware's hex dump.  ``drop_line`` omits one line,
    simulating a USB CDC drop."""
    out = []
    for i in range(0, len(values), per_line):
        if i // per_line == drop_line:
            continue
        chunk = values[i:i + per_line]
        out.append("%04X:" % i + "".join(" %02X" % (v & 0xFF) for v in chunk))
    return "\r\n".join(out)


def _fake_opread_reply(op: int, values: Sequence[int], **kw) -> str:
    body = _synth_dump(values, **kw)
    arr = np.asarray(values, dtype=int)
    stats = ("op %02X: s=FF FF FF nff=%d/%d min=%d max=%d mean=%d span=%d "
             "first16: %s" % (op, int((arr != 0xFF).sum()), arr.size, arr.min(),
                              arr.max(), int(arr.mean()),
                              int(arr.max() - arr.min()),
                              " ".join("%02X" % v for v in arr[:16])))
    return "spi3 opread %02x %d dump\r\n%s\r\n%s\r\n> " % (op, len(values), body, stats)


class SimBench:
    """A simulated bench: a 2C53T debug shell and one stimulus, for ``--dry-run``.

    It answers every command the bench scripts send with a well-formed,
    self-consistent reply: each timebase code has a rate, each range a gain,
    the offset DAC moves the trace, and the input is whatever the fake source
    was last told.  One Kode Dot pin drives both channels, as on the bench.
    It exists so a script's whole flow — argument handling, sequencing,
    parsing, maths and report — can run with no hardware attached.

    NOTHING IT PRODUCES IS EVIDENCE ABOUT THE HARDWARE.  Its rates are the
    1-2.5-5 ladder (including GUESSES for 0x06-0x0C), its gains a smooth
    ladder, its records perfectly coherent.  A dry run that "measures" 0x0A at
    1.25 MS/s has measured this class.
    """

    #: Ladder rates; 0x0A-0x0C continue the 1-2.5-5 pattern (a guess), and
    #: 0x06-0x09 sit at EXP-15's ~1.25 kS/s cluster (also a guess).
    FS_BY_CODE = {0x06: 1250.0, 0x07: 1250.0, 0x08: 1250.0, 0x09: 1250.0,
                  0x0A: 1.25e6, 0x0B: 5e5, 0x0C: 2.5e5, 0x0D: 1.25e5, 0x0E: 5e4,
                  0x0F: 2.5e4, 0x10: 1.25e4, 0x11: 5e3, 0x12: 2.5e3, 0x13: 1250.0,
                  0x14: 500.0}
    #: True mV/count per range when no table is supplied (0 = railed range).
    GAINS = {1: [0, 0, 0, 0, 12.95, 20.08, 39.51, 81.35, 256.7, 324.0],
             2: [0, 0, 0, 0, 8.73, 19.28, 38.37, 77.09, 205.8, 391.0]}
    DAC_MID = 2048
    DAC_PER_COUNT = 4.0         # offset-DAC codes per ADC count, every range
    LEDC_CLK = 80_000_000

    def __init__(self, source: str = "kodedot", v3v3_mv: float = 3292.0,
                 gains: Optional[dict] = None, noise: float = 0.6, seed: int = 1):
        if source not in SOURCE_KINDS:
            raise BenchError("SimBench source must be one of %s" % (SOURCE_KINDS,))
        self.kind = source
        self.v3v3 = float(v3v3_mv)
        self.gains = {ch: list(g) for ch, g in (gains or self.GAINS).items()}
        self.noise = noise
        self.rng = np.random.default_rng(seed)
        self.code = 0x10
        self.range = {1: 6, 2: 6}
        self.dac = {1: self.DAC_MID, 2: self.DAC_MID}
        self.log: list = []
        # stimulus state
        self.kd = {"mode": "pwm", "req": 1000, "duty": 50.0}
        self.kd.update(self._ledc_plan(1000))
        self.esp = {ch: {"mode": "dc", "hz": 0.0, "amp": 2000, "mid": 1650,
                         "duty": 50, "phase": 0} for ch in (1, 2)}
        self.man = {"what": "quiet", "hz": 1000.0, "vpp": 0.0, "waveform": "sine"}

    # -- the input voltage seen by both scope channels ---------------------

    def _ledc_plan(self, hz: int) -> dict:
        for bits in range(13, 0, -1):
            div_q8 = int(round(self.LEDC_CLK * 256.0 / (hz * (1 << bits))))
            if 256 <= div_q8 <= 0x3FFFF:
                return {"bits": bits, "div_q8": div_q8,
                        "hz": self.LEDC_CLK * 256.0 / (div_q8 * (1 << bits))}
        raise BenchError("no LEDC setting for %d Hz" % hz)

    @staticmethod
    def _shape(shape: str, phase: np.ndarray, duty: float = 0.5) -> np.ndarray:
        """Unit waveform in [-1, 1] at cycle phase ``phase`` (in cycles)."""
        p = np.mod(phase, 1.0)
        if shape == "sine":
            return np.sin(2 * np.pi * p)
        if shape == "square":
            return np.where(p < duty, 1.0, -1.0)
        if shape == "tri":
            return 1.0 - 4.0 * np.abs(p - 0.5)
        if shape == "saw":
            return 2.0 * p - 1.0
        return np.zeros_like(p)

    def vin(self, ch: int, t: np.ndarray) -> np.ndarray:
        """Millivolts at channel ``ch``'s probe tip at times ``t``."""
        if self.kind == "kodedot":            # one pin, both probes
            mode = self.kd["mode"]
            if mode == "dc_low":
                return np.zeros_like(t)
            if mode == "dc_high":
                return np.full_like(t, self.v3v3)
            high = self._shape("square", t * self.kd["hz"], self.kd["duty"] / 100.0) > 0
            return np.where(high, self.v3v3, 0.0)
        if self.kind == "esp32":
            s = self.esp[ch]
            if s["mode"] == "dc":
                return np.full_like(t, float(s["mid"]))
            return s["mid"] + 0.5 * s["amp"] * self._shape(
                s["mode"], t * s["hz"] + s["phase"] / 360.0, s["duty"] / 100.0)
        m = self.man                           # manual: bipolar, centred on 0 V
        if m["what"] == "quiet":
            return np.zeros_like(t)
        return 0.5 * m["vpp"] * self._shape(m["waveform"], t * m["hz"])

    def record(self, ch: int, n: int = STOCK_SAMPLES) -> np.ndarray:
        """One capture: ``n`` ADC codes at the current code/range/offset."""
        fs = self.FS_BY_CODE.get(self.code, 12500.0)
        t = self.rng.uniform(0.0, 1.0) + np.arange(n) / fs
        k = self.gains[ch][self.range[ch]]
        if k <= 0:
            return np.full(n, 255, dtype=int)        # railed, as ranges 0-3 are
        v_off = (self.dac[ch] - self.DAC_MID) / self.DAC_PER_COUNT * k
        codes = 128 + (self.vin(ch, t) - v_off) / k + self.rng.normal(0, self.noise, n)
        return np.clip(np.round(codes), 0, 255).astype(int)

    # -- transports ---------------------------------------------------------

    def _scope_reply(self, line: str) -> str:
        self.log.append(("scope", line))
        toks = line.split()
        if line == "version":
            return "OpenScope SIMULATED (bench.py SimBench, --dry-run) - no hardware\r\n> "
        m = re.fullmatch(r"fpga scope timebase ([0-9A-Fa-f]{1,2})", line)
        if m:
            self.code = int(m.group(1), 16)
            return "timebase 0x%02X (display + reg 0x01)\r\n> " % self.code
        m = re.fullmatch(r"fpga scope range (\d) ?([12])?", line)
        if m:
            r = int(m.group(1))
            for ch in ((int(m.group(2)),) if m.group(2) else (1, 2)):
                self.range[ch] = r
            return "range %d on %s\r\n> " % (r, "CH" + m.group(2) if m.group(2) else "both")
        m = re.fullmatch(r"fpga scope center (ch[12] )?(\d)", line)
        if m:
            ch = int(m.group(1)[2]) if m.group(1) else 1
            r = int(m.group(2))
            self.range[1] = self.range[2] = r       # the firmware applies both banks
            k = self.gains[ch][r]
            if k > 0:
                fs = self.FS_BY_CODE.get(self.code, 12500.0)
                level = float(np.median(self.vin(ch, np.arange(STOCK_SAMPLES) / fs)))
                self.dac[ch] = int(min(4095, max(0, round(
                    self.DAC_MID + self.DAC_PER_COUNT * level / k))))
            med = int(np.median(self.record(ch)))
            return ("CH%d range %d: center %s=%d (median=%d)\r\n> "
                    % (ch, r, "TMR13_C1DT" if ch == 2 else "DAC1", self.dac[ch], med))
        m = re.fullmatch(r"fpga scope vdiv ([12]) (\d)", line)
        if m:
            # The vdiv BUTTON's path: display state AND the relay bank (one channel).
            ch, r = int(m.group(1)), int(m.group(2))
            self.range[ch] = r
            return "vdiv CH%d = range %d (sim/div)\r\n> " % (ch, r)
        m = re.fullmatch(r"fpga scope measure (\d+)", line)
        if m:
            # The badge pipeline, as `fpga scope measure` prints it: pp from the
            # record's extremes, Vpp/Vrms through the channel's k (refused as
            # "-" when the range has no cal), the frequency from the record's
            # period at the code's rate (refused when the code has no rate).
            reps = int(m.group(1))
            fs = self.FS_BY_CODE.get(self.code)
            out = ["badge sources: rng1=%d k1_uV=%d  rng2=%d k2_uV=%d  tb=0x%02X inforce=0x%02X fs=%s"
                   % (self.range[1], int(self.gains[1][self.range[1]] * 1000),
                      self.range[2], int(self.gains[2][self.range[2]] * 1000),
                      self.code, self.code, int(fs) if fs else 0)]
            for i in range(reps):
                rec = {ch: self.record(ch) for ch in (1, 2)}
                pp = {ch: int(rec[ch].max() - rec[ch].min()) for ch in (1, 2)}
                k = {ch: self.gains[ch][self.range[ch]] for ch in (1, 2)}
                ac = rec[1] - rec[1].mean()
                vpp1 = "-" if k[1] <= 0 else str(int(pp[1] * k[1] * 1000))
                vrms1 = "-" if k[1] <= 0 else str(int(float(np.sqrt(np.mean(ac * ac))) * k[1] * 1000))
                vpp2 = "-" if k[2] <= 0 else str(int(pp[2] * k[2] * 1000))
                hz = self._source_hz()
                if fs and hz and pp[1] > 8:
                    per = fs / hz
                    per_s, f_m = "%d" % int(per * 100), "%d" % int(hz * 1000)
                else:
                    per_s, f_m = "-", "-"
                out.append("M %2d pp1=%d ppr1=%d Vpp1_uV=%s Vrms1_uV=%s duty1_pm=500 "
                           "per1_smp100=%s f1_mHz=%s rise1_smp100=- fall1_smp100=- pp2=%d Vpp2_uV=%s"
                           % (i, pp[1], max(0, pp[1] - 1), vpp1, vrms1, per_s, f_m, pp[2], vpp2))
            return line + "\r\n" + "\r\n".join(out) + "\r\n> "
        m = re.fullmatch(r"(trig2?) raw (\d+)", line)
        if m:
            ch = 2 if m.group(1) == "trig2" else 1
            self.dac[ch] = min(4095, int(m.group(2)))
            return ("%s = code %d\r\n> "
                    % ("TMR13_C1DT(PA6)" if ch == 2 else "DAC1(PA4)", self.dac[ch]))
        m = re.fullmatch(r"spi3 opread (0[45]) (\d+) dump", line)
        if m:
            op, n = int(m.group(1), 16), int(m.group(2))
            vals = [0x7C, 0x79] + list(self.record(1 if op == 0x04 else 2, n - 2))
            return _fake_opread_reply(op, vals)
        if toks[:2] == ["spi3", "read"] and len(toks) == 3:
            return "%s\r\n%s\r\n> " % (line, _synth_dump(list(self.record(1, int(toks[2])))))
        raise BenchError("SimBench scope: no simulated reply for %r" % line)

    def _source_hz(self) -> float:
        """The frequency the simulated stimulus is producing, 0 when quiet."""
        if self.kind == "kodedot":
            return float(self.kd.get("hz", 0.0)) if self.kd.get("mode") == "pwm" else 0.0
        if self.kind == "esp32":
            e = self.esp[1]
            return float(e["hz"]) if e["mode"] != "dc" else 0.0
        return float(self.man["hz"]) if self.man["what"] != "quiet" else 0.0

    def _kd_freq_rec(self) -> str:
        p = self.kd
        return (">|freq hz=%.6f req_hz=%d err_ppm=%+.3f bits=%d div=%.8g timing=%s"
                % (p["hz"], p["req"], (p["hz"] - p["req"]) / p["req"] * 1e6, p["bits"],
                   p["div_q8"] / 256.0, "clean" if p["div_q8"] % 256 == 0 else "frac-edge"))

    def _kd_duty_rec(self) -> str:
        full = 1 << self.kd["bits"]
        cnt = int(round(self.kd["duty"] / 100.0 * full))
        return (">|duty=%.4f%% req=%.3f%% count=%d/%d step=%.4f%%"
                % (100.0 * cnt / full, self.kd["duty"], cnt, full, 100.0 / full))

    def _kodedot_reply(self, line: str) -> str:
        self.log.append(("kodedot", line))
        toks = line.split()
        cmd, arg = toks[0].lower(), (toks[1] if len(toks) > 1 else None)
        out = []
        if cmd == "f" and arg and arg.isdigit() and 1 <= int(arg) <= 10_000_000:
            self.kd.update({"mode": "pwm", "req": int(arg)})
            self.kd.update(self._ledc_plan(int(arg)))
            out = [self._kd_freq_rec(), self._kd_duty_rec()]
        elif cmd == "d" and arg is not None:
            self.kd.update({"mode": "pwm", "duty": float(arg)})
            out = [self._kd_duty_rec()]
        elif cmd == "dc" and arg in ("0", "1"):
            self.kd["mode"] = "dc_high" if arg == "1" else "dc_low"
            out = [">|out mode=%s pin=GPIO14 j3_pin=9" % self.kd["mode"]]
        elif cmd == "pwm":
            self.kd["mode"] = "pwm"
            out = [self._kd_freq_rec(), self._kd_duty_rec()]
        elif cmd in ("s", "status"):
            out = [">|out mode=%s pin=GPIO14 j3_pin=9 exp=7 gnd_j3_pins=10,11" % self.kd["mode"],
                   self._kd_freq_rec(),
                   ">|clock src=PLL_F80M clk_hz=%d (SPLL=12x X1 40 MHz, /6) div_raw=0x%05X"
                   % (self.LEDC_CLK, self.kd["div_q8"]),
                   self._kd_duty_rec(), ">|xtal ppm=+0.000 (nominal 40 MHz assumed)",
                   ">|sweep idle"]
        elif cmd == "sweep":
            out = [">|sweep idle"] if arg == "stop" else \
                  [">|sweep step=1/15 hold_s=5 hz=10.000000 req_hz=10"]
        else:
            return ">err msg=unknown_command_%s_try_help\r\n" % cmd
        return "\r\n".join(out + [">ok"]) + "\r\n"

    def _siggen_line(self, ch: int) -> str:
        s = self.esp[ch]
        return ("[siggen] CH%d mode=%s  freq=%.1f Hz  amp=%d mVpp  mid=%d mV  duty=%d%%  "
                "phase=%d deg" % (ch, s["mode"], s["hz"], s["amp"], s["mid"], s["duty"],
                                  s["phase"]))

    def _siggen_reply(self, line: str) -> str:
        self.log.append(("siggen", line))
        toks = line.split()
        if toks == ["fs"]:
            return ("[fs] nominal=40000  achieved=32999.5 Hz  ratio=0.8250  n=264000  "
                    "dt=8.00s  set_freq uses MEASURED\n")
        if toks == ["fs", "reset"]:
            return "[fs] window reset\n"
        if toks[:1] == ["usefs"]:
            on = len(toks) > 1 and toks[1] != "0"
            return ("[fs] set_freq now divides by %s\n"
                    % ("32999.5 (MEASURED)" if on else "40000.0 (nominal)"))
        if toks == ["status"]:
            return "\n".join([self._siggen_line(1), self._siggen_line(2),
                              "[pwm] GPIO27 off"]) + "\n"
        if len(toks) >= 2 and toks[0] in ("1", "2"):
            ch, cmd, args = int(toks[0]), toks[1], toks[2:]
            s = self.esp[ch]
            if cmd in ("sine", "square", "tri", "saw"):
                s["mode"] = cmd
                if args:
                    s["hz"] = float(args[0])
                if cmd == "square" and len(args) > 1:
                    s["duty"] = int(args[1])
            elif cmd == "amp" and args:
                s["amp"] = int(int(args[0]) * 255 // 3300 * 3300 // 255)
            elif cmd == "off":
                s["mode"] = "dc"
            elif cmd == "phase" and args:
                s["phase"] = int(args[0]) % 360
            else:
                return "[siggen] ? unknown: %s\n" % cmd
            return self._siggen_line(ch) + "\n"
        return "[siggen] ? unknown: %s\n" % line

    def operator(self, src: "ManualSource") -> Callable[[str], str]:
        """An ``input_fn`` for :class:`ManualSource` that answers as an operator
        with a perfect counter and DMM would, and sets the simulated input."""
        def answer(_prompt: str) -> str:
            self.log.append(("operator", src.stage, dict(src.pending)))
            what = src.pending.get("what")
            if src.stage == "set":
                if what == "quiet":
                    self.man["what"] = "quiet"
                else:
                    self.man.update({"what": what, "hz": src.pending["hz"],
                                     "waveform": src.pending.get("waveform", "sine"),
                                     "vpp": src.pending.get("mvpp", 2000.0)})
                return ""
            if src.stage == "freq":
                return "%.4f" % self.man["hz"]
            if src.stage == "amp":
                return "%.4f Vpp" % (self.man["vpp"] / 1000.0)
            raise BenchError("SimBench operator: unexpected stage %r" % src.stage)
        return answer

    # -- devices wired to this bench ----------------------------------------

    def scope(self) -> "Scope":
        return Scope(transport=ScriptedTransport(self._scope_reply))

    def source(self, waveform: Optional[str] = None) -> "SignalSource":
        if self.kind == "kodedot":
            return KodeDotSource(transport=ScriptedTransport(self._kodedot_reply),
                                 v3v3_mv=self.v3v3)
        if self.kind == "esp32":
            return Esp32Source(Siggen(transport=ScriptedTransport(self._siggen_reply)),
                               settle=0, fs_window=0.0)
        src = ManualSource(waveform=waveform or "sine", input_fn=lambda _p: "")
        src.input_fn = self.operator(src)
        return src


class _T:
    """Tiny test harness: prints PASS/FAIL, tallies failures."""

    def __init__(self):
        self.fail = 0
        self.n = 0

    def ok(self, cond: bool, what: str, note: str = ""):
        self.n += 1
        if cond:
            print("  PASS  %s%s" % (what, ("  [%s]" % note) if note else ""))
        else:
            self.fail += 1
            print("  FAIL  %s%s" % (what, ("  [%s]" % note) if note else ""))

    def raises(self, fn, what: str, exc=BenchError):
        self.n += 1
        try:
            fn()
        except exc as e:
            print("  PASS  %s  [%s]" % (what, str(e).splitlines()[0][:70]))
            return
        except Exception as e:                                  # pragma: no cover
            self.fail += 1
            print("  FAIL  %s  [wrong exception: %r]" % (what, e))
            return
        self.fail += 1
        print("  FAIL  %s  [no exception raised]" % what)


def selftest() -> int:
    """Exercise parsing, statistics and verdict logic on synthetic data.

    Note: several checks deliberately build VOID results, so the run emits
    `bench: VOID result ...` warnings on stderr. That is the library working."""
    t = _T()
    rng = np.random.default_rng(20260817)

    print("\n1. hex-dump parsing")
    vals = [(i * 7 + 3) & 0xFF for i in range(1026)]
    text = _fake_opread_reply(0x04, vals)
    arr = parse_dump(text)
    t.ok(arr.size == 1026, "parses all 1026 dumped bytes", "got %d" % arr.size)
    t.ok(list(arr[:4]) == vals[:4], "byte values round-trip")
    t.ok(parse_dump("nothing here\r\n> ").size == 0, "non-dump text yields empty array")
    t.raises(lambda: parse_dump(_fake_opread_reply(0x04, vals, drop_line=3)),
             "a dropped dump line raises instead of shifting every later sample")

    print("\n2. opread framing — stock drops TWO, keeps 1024 (ce22b49)")
    sc = Scope(transport=ScriptedTransport({
        "spi3 opread 04 1026 dump": text,
        "spi3 opread 05 1026 dump": _fake_opread_reply(0x05, vals),
        "spi3 opread 06 1026 dump": _fake_opread_reply(0x06, vals[:512]),
    }))
    v = sc.opread(0x04)
    t.ok(v.size == STOCK_SAMPLES == 1024, "1026-byte window -> 1024 samples",
         "got %d" % v.size)
    t.ok(v[0] == vals[2], "sample[0] is dumped byte 2 (opcode echo + 1 dummy dropped)",
         "%d vs %d" % (v[0], vals[2]))
    t.ok(v[0] != vals[3], "sample[0] is NOT dumped byte 3 — the historical off-by-one")
    t.ok(sc.opread(0x04, drop=3).size == 1023,
         "drop=3 still available, and visibly yields the old 1023-sample array")
    t.raises(lambda: sc.opread(0x06), "a short window raises ShortReadError",
             ShortReadError)
    st = parse_opread_stats(text)
    t.ok(st is not None and st.opcode == 0x04 and st.length == 1026,
         "device stats line parses as a cross-check")
    t.ok(st.span == int(max(vals) - min(vals)), "stats span agrees with the dump")

    print("\n3. shell argument guards")
    t.raises(lambda: sc.scope_range(5, 0),
             "scope_range(ch=0) refused — the shell arg is 1-based, 0 means BOTH")
    t.raises(lambda: sc.gpio("XX9", 1), "gpio() rejects a malformed pin name")
    t.raises(lambda: sc.gpio("E4", 2), "gpio() rejects a level that is not 0/1")
    t.raises(lambda: sc.opread(0x1FF), "opread() rejects an out-of-range opcode")

    print("\n4. spectral detection — why a fixed bin is forbidden")
    n = STOCK_SAMPLES
    tone_bin = 22.3                      # the tone has drifted off bin 17
    sig = 128 + 46 * np.sin(2 * np.pi * tone_bin * np.arange(n) / n)
    mag = spectrum(sig)
    watched = float(mag[17])             # what a fixed-bin detector would report
    found = band(sig, 15, 25)
    top = peaks(sig, 3)
    t.ok(top[0][0] == 22, "peaks() finds the tone at bin 22", "top=%s" % top)
    t.ok(found > 10 * watched,
         "windowed band() recovers the tone a fixed bin misses",
         "fixed bin 17 -> %.2f, band(15,25) -> %.2f" % (watched, found))
    t.ok(band_peak(sig, 15, 25)[0] == 22, "band_peak() reports WHICH bin won")
    t.raises(lambda: band(sig, 17, 17), "band() refuses a zero-width window")
    t.raises(lambda: band(sig, 20, 15), "band() refuses an inverted window")
    t.raises(lambda: fixed_bin(sig, 17), "fixed_bin() exists only to refuse")
    lo, hi = window_for(250.0, fs_hz=14600.0, n=n)
    t.ok(lo < bin_of(250.0, 14600.0, n) < hi and hi > lo + 1,
         "window_for() brackets the nominal bin with room for drift",
         "(%d,%d) around %.1f" % (lo, hi, bin_of(250.0, 14600.0, n)))
    t.ok(abs(spectrum(sig)[0]) == 0.0, "DC bin is zeroed")
    t.raises(lambda: spectrum([1, 2, 3]), "spectrum() refuses a too-short window")
    # The double-FFT guard. peaks(spectrum(v)) transformed twice and returned
    # bin 1 / mag 0.04 for EVERY tone; that artifact was recorded as a hardware
    # finding ("the capture cannot resolve frequency") until EXP-10 reproduced
    # it synthetically. It must never be silently accepted again.
    t.raises(lambda: peaks(spectrum(sig), 1),
             "peaks(spectrum(v)) is refused, not silently double-transformed")
    t.ok(peaks(np.concatenate([np.zeros(500), np.full(524, 255.0)]), 1)[0][0] >= 1,
         "a railed capture starting at zero still analyses (guard needs odd length)")

    print("\n5. paired difference — drift cancellation and its control")
    clock = {"t": 0}

    def make_reader(offset: float, gain: float = 1.0):
        def rd():
            clock["t"] += 1
            drift = 0.05 * clock["t"]        # linear drift, the confound
            return (128 + drift + offset
                    + gain * rng.normal(0, 0.5, STOCK_SAMPLES))
        return rd

    read_a = make_reader(0.0)
    read_b = make_reader(3.0)               # B really is 3 codes higher
    ctrl = paired_control(read_a, n=20)
    t.ok(abs(ctrl.t) < 3.0, "A,A,A control is null despite steady drift", str(ctrl))
    test = paired_difference(read_a, read_b, n=20)
    t.ok(abs(test.mean - 3.0) < 0.2 and abs(test.t) > 10,
         "A,B,A recovers the true 3.0-code offset through the drift", str(test))
    t.raises(lambda: paired_difference(read_a, read_b, n=2),
             "paired_difference refuses fewer than 3 trials")
    short = lambda: np.zeros(10)
    t.raises(lambda: paired_difference(read_a, short, n=5),
             "all-short trials raise rather than reporting 0 usable trials")

    print("\n6. Result — the verdict is computed, never assigned")
    bare = Result("uncontrolled null result", Verdict.NEGATIVE, "no signal seen")
    t.ok(bare.verdict == Verdict.VOID,
         "a NEGATIVE with no control renders as VOID", bare.void_reason[:48])
    failed = Result("null result, control failed", Verdict.NEGATIVE, "",
                    controls=(Control.positive("detector sees a tone", "bin 17 lit",
                                               "nothing", passed=False),))
    t.ok(failed.verdict == Verdict.VOID, "a failed control makes it VOID, not negative")
    good = Result("null result, control passed", Verdict.NEGATIVE, "",
                  controls=(Control.positive("detector sees a tone", "bin 17 lit",
                                             "bin 17 @ 23.4", passed=True),))
    t.ok(good.verdict == Verdict.NEGATIVE, "a NEGATIVE with a passing control stands")
    pos = Result("uncontrolled positive", Verdict.POSITIVE, "")
    t.ok(pos.verdict == Verdict.POSITIVE and pos.uncontrolled,
         "an uncontrolled POSITIVE stands but is flagged")
    t.raises(lambda: Result("x", "VOID"), "VOID cannot be asserted as an observation")
    t.raises(lambda: Result("x", Verdict.NEGATIVE, controls=("not a control",)),
             "controls must be Control instances")
    t.ok("| **none run** | — | — | **no** |" in bare.markdown(),
         "markdown() shows an empty control table as a failure")

    print("\n7. paired_experiment wires the control in automatically")
    clock["t"] = 0
    res = paired_experiment(read_a, read_b, n=15, label="op04 vs op05")
    t.ok(res.verdict == Verdict.POSITIVE and len(res.controls) == 1,
         "a real difference reports POSITIVE with its control attached", str(res.data))
    clock["t"] = 0
    res_null = paired_experiment(read_a, make_reader(0.0), n=15, label="op04 vs op04'")
    t.ok(res_null.verdict == Verdict.NEGATIVE,
         "no difference reports NEGATIVE — because the control passed")

    print("\n8. Experiment collects controls for every later result")
    exp = Experiment("selftest experiment", unit="none", build="selftest")
    r_void = exp.negative("swept reg 0x06, nothing moved")
    t.ok(r_void.verdict == Verdict.VOID, "negative before any control is VOID")
    exp.control("detector sees 250 Hz on the working jack", "peak near bin 17",
                "bin 17 @ 23.4", passed=True)
    r_ok = exp.negative("swept reg 0x06, nothing moved")
    t.ok(r_ok.verdict == Verdict.NEGATIVE, "same negative after the control stands")
    t.ok("## 4. Control" in exp.markdown(), "markdown() emits experiment-file sections")

    print("\n9. siggen reply parsing (and refusing to assume)")
    line1 = ("[siggen] CH1 mode=sine  freq=100.0 Hz  amp=2000 mVpp  mid=1650 mV  "
             "duty=50%  phase=0 deg")
    line2 = ("[siggen] CH2 mode=square  freq=250.0 Hz  amp=1000 mVpp  mid=1650 mV  "
             "duty=25%  phase=180 deg")
    pwm = "[pwm] GPIO27 req=1000 Hz  50%  10-bit  actual=1000 Hz"
    sg = Siggen(transport=ScriptedTransport({
        "1 sine 100": line1,
        "2 square 250 25": line2,
        "status": "%s\n%s\n%s" % (line1, line2, pwm),
        "1 tri 500": line1,                    # device ignored it: still sine
        "1 sine 400": line1,                   # device ignored the frequency
        "pwm 1000 50": pwm,
        "pwm off": "[pwm] GPIO27 off",
        "2 off": "",                           # device said nothing at all
    }))
    s1 = sg.sine(100, ch=1, settle=0)
    t.ok(s1.mode == "sine" and abs(s1.freq_hz - 100.0) < 0.1, "sine() parses its echo")
    s2 = sg.square(250, duty=25, ch=2, settle=0)
    t.ok(s2.duty_pct == 25 and s2.phase_deg == 180, "square() parses duty and phase")
    t.raises(lambda: sg.tri(500, ch=1, settle=0),
             "a mode the device did not adopt raises instead of passing silently")
    t.raises(lambda: sg.sine(400, ch=1, settle=0),
             "a frequency the device did not adopt raises")
    t.raises(lambda: sg.off(ch=2, settle=0), "an unparsable reply is not success")
    stat = sg.status()
    t.ok(stat[1].freq_hz == 100.0 and stat[2].mode == "square" and stat["pwm"].on,
         "status() parses both channels and the PWM")
    t.ok(sg.pwm(1000, 50, settle=0).actual_hz == 1000, "pwm() parses actual_hz")
    t.ok(sg.pwm_off(settle=0).on is False, "pwm off parses")

    print("\n10. Kode Dot source — the ACTUAL frequency, and refusing to assume")
    status = (">|out mode=pwm pin=GPIO14 j3_pin=9\r\n>|freq hz=1000.000000 req_hz=1000 "
              "bits=13 div=9.765625 timing=clean\r\n>|clock src=PLL_F80M clk_hz=80000000\r\n"
              ">|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%\r\n>ok\r\n")
    f1000 = (">|freq hz=999.984741 req_hz=1000 err_ppm=-15.259 bits=13 div=9.765625 "
             "timing=frac-edge\r\n>|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%"
             "\r\n>ok\r\n")
    kd = KodeDotSource(transport=ScriptedTransport({
        "s": status,
        "f 1000": f1000,
        "f 2000": f1000,                                            # device kept 1 kHz
        "f 3000": ">err msg=no_ledc_setting_for_3000_hz\r\n",
        "f 4000": f1000.replace(">ok\r\n", ""),                     # unterminated
        "f 5000": "freq 4999.98 Hz (req 5000, clk 80000000 res 13 div 1953.1)\r\n",
        "dc 1": ">|out mode=dc_high pin=GPIO14 j3_pin=9\r\n>ok\r\n",
        "dc 0": ">|out mode=dc_high pin=GPIO14 j3_pin=9\r\n>ok\r\n",  # did not move
    }))
    t.ok(abs(kd.tone(1000) - 999.984741) < 1e-6 and kd.freq_hz != 1000.0,
         "tone() returns the reported actual, not the request", "%.6f" % kd.freq_hz)
    t.ok(abs(kd.freq(5000).hz - 4999.98) < 1e-9, "the bare `freq ... (req ...)` form parses")
    t.raises(lambda: kd.freq(2000), "a frequency the device did not adopt raises")
    t.raises(lambda: kd.freq(3000), "'>err msg=' raises with the device's reason")
    t.raises(lambda: kd.freq(4000), "a framed reply without '>ok' is not success")
    t.raises(lambda: kd.freq(12.5), "fractional hertz is refused (the app wants whole Hz)")
    t.ok(kd.dc(1).mode == "dc_high", "dc 1 is confirmed from 'mode=dc_high'")
    t.raises(lambda: kd.dc(0), "dc 0 answered with mode=dc_high raises")
    t.ok(kd.can_produce(3300) and not kd.can_produce(1000),
         "one amplitude: the rail (within 5 %), nothing else")
    t.raises(lambda: KodeDotSource(transport=ScriptedTransport({"s": "kodeOS> \r\n"})),
             "a port answering without a sigsrc status is refused")

    print("\n11. manual source and waveform-aware amplitudes")
    answers = iter(["", "oops", "1.0001 kHz", "", "3.292", "1.646 Vrms"])
    ms = ManualSource(waveform="square", input_fn=lambda _p: next(answers),
                      print_fn=lambda _m: None)
    t.ok(abs(ms.tone(1000) - 1000.1) < 1e-9,
         "the operator's counter reading is returned; garbage is asked again")
    t.ok(abs(ms.drive(3300) - 3292.0) < 1e-9,
         "a square's 1.646 Vrms is 3292 mVpp (2 x), after a unit-less answer is refused")
    t.ok(abs(vpp_from_reading(1000.0, "rms", "sine") - 2828.43) < 0.01,
         "a sine's Vrms is x 2*sqrt(2)")
    t.ok(abs(expected_span_counts(vpp_from_reading(3292.0, "dc", "square"), 20.0)
             - 164.6) < 1e-9, "square expected counts = Vpp / gain (3292 mV at 20 mV/ct)")
    t.raises(lambda: vpp_from_reading(3292.0, "dc", "sine"),
             "a DC level is not a sine's amplitude")
    t.raises(lambda: expected_span_counts(1000.0, 0.0), "a no-cal range has no expected count")

    print("\n12. port discovery by USB id (no port opened)")
    P = lambda d, v, p, sn=None: type("P", (), {"device": d, "vid": v, "pid": p,  # noqa: E731
                                                "serial_number": sn, "location": None})
    ports = [P("/dev/cu.usbmodem101", 0x303A, 0x1001, "A"),
             P("/dev/cu.usbmodem2101", 0x2E3C, 0x5740)]
    t.ok(find_port(ESPRESSIF_VID, ports=ports) == "/dev/cu.usbmodem101" and
         find_port(*SCOPE_USB_ID, ports=ports) == "/dev/cu.usbmodem2101",
         "VID separates the Dot from the scope (both /dev/cu.usbmodem*)")
    t.raises(lambda: find_port(ESPRESSIF_VID, ports=ports + [
        P("/dev/cu.usbmodem301", 0x303A, 0x1001, "B")]),
        "two Espressif devices and no serial number: refused, not guessed")
    t.raises(lambda: find_port(0x303A, ports=[]), "no match raises")

    print("\n13. SimBench — the --dry-run bench is self-consistent")
    sim = SimBench(source="kodedot")
    ssc, ssrc = sim.scope(), sim.source()
    ssc.timebase(0x0C)
    hz = ssrc.tone(50000)
    v = parse_dump(ssc.cmd("spi3 read 1024")).astype(float)
    want = bin_of(hz, SimBench.FS_BY_CODE[0x0C])
    t.ok(abs(peaks(v, 1)[0][0] - want) <= 1,
         "a 50 kHz Dot square at simulated 0x0C peaks at its bin, not a harmonic",
         "bin %d vs %.1f" % (peaks(v, 1)[0][0], want))
    ssc.scope_range(6, 1)
    ssrc.quiet()
    ssc.cmd("fpga scope center ch1 6")
    lo = ssc.opread(0x04).mean()
    ssrc.hold(1)
    hi = ssc.opread(0x04).mean()
    t.ok(abs((hi - lo) - 3292.0 / SimBench.GAINS[1][6]) < 1.0,
         "static levels differ by Vpp / gain counts", "%.1f counts" % (hi - lo))

    print("\n%d checks, %d failures" % (t.n, t.fail))
    return 1 if t.fail else 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: Optional[Sequence[str]] = None) -> int:
    p = argparse.ArgumentParser(
        description="Shared measurement library for the OpenScope 2C53T bench. "
                    "Import it from experiment scripts; run --selftest to check "
                    "the parsing and statistics with no hardware attached.")
    p.add_argument("--selftest", action="store_true",
                   help="exercise parsing/statistics/verdict logic on synthetic "
                        "data, with NO device present")
    p.add_argument("--version", action="store_true", help="print module info")
    args = p.parse_args(argv)
    if args.version:
        print("bench.py — OpenScope 2C53T bench library; numpy %s" % np.__version__)
        return 0
    if args.selftest:
        return selftest()
    p.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
