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
        text = self.cmd("spi3 opread %02x %d dump" % (op, n), timeout)
        raw = parse_dump(text)
        if len(raw) < n:
            raise ShortReadError(
                "opread %02X: asked for %d bytes, parsed %d — window unusable"
                % (op, n, len(raw)))
        return raw[drop:drop + (n - drop)].astype(dtype)

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

    def trigger_level(self, code: int) -> str:
        """Set the digital trigger level (SPI3 register 0x08, an ADC code)."""
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
    WAVE = {"sine": 0, "square": 1, "triangle": 2, "tri": 2}

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
