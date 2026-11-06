"""OpenScope remote protocol: framing, constants and payload codecs.

Pure functions over bytes — no I/O, fully testable without hardware
(docs/design/remote_protocol.md §3.3, §4.1).

Frame:  0xAA | cmd | len_hi | len_lo | payload | checksum
        checksum = XOR of cmd, len_hi, len_lo and every payload byte.
The length is big-endian (as the firmware parser decodes it); multi-byte
values inside payloads are little-endian (§3.3).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import List, Optional

SYNC = 0xAA
HEADER_LEN = 4
MAX_TX_PAYLOAD = 256        # device receive cap (ESP_MAX_PAYLOAD): host->device only
# Device->host is not capped by the firmware, but no reply this host asks for
# is larger (a WAVEFORM_FRAME is 24 + 1024 = 1048 B). Bounding it matters: a stray 0xAA in
# shell text followed by bytes that read as a huge length would otherwise make
# the decoder wait for data that never comes and hide every good frame after.
MAX_RX_PAYLOAD = 4096

PROTO_MAJOR = 1             # STATUS byte 0; refuse other majors (§3.7)

# Host -> device
CMD_PING = 0x01
CMD_STATUS = 0x08
CMD_BUTTON = 0x0A
CMD_GET_METER = 0x21
CMD_GET_WAVEFORM = 0x22

# Device -> host
RSP_ACK = 0x81
RSP_NAK = 0x82
RSP_DATA = 0x83
RSP_STATUS = 0x85
RSP_METER_FRAME = 0x90
RSP_WAVEFORM_FRAME = 0x91

ERRORS = {
    0x01: "UNKNOWN_CMD",
    0x02: "BAD_CHECKSUM",
    0x03: "BAD_LENGTH",
    0x04: "FLASH_WRITE",
    0x05: "FLASH_FULL",
    0x06: "INVALID_SLOT",
    0x07: "NOT_READY",
    0x08: "TRANSFER_ACTIVE",
    0x09: "UNSUPPORTED",
    0x0A: "TIMEOUT",
    0x0B: "NO_CAPTURE_DATA",
    0x0C: "UNSUPPORTED_IN_MODE",
    0x0D: "BAD_ARG",
}

# button_id_t (firmware/src/ui/ui.h) == ESP_BTN_* (esp_comm.h)
BUTTONS = {
    "CH1": 1, "CH2": 2, "MOVE": 3, "SELECT": 4, "TRIGGER": 5, "PRM": 6,
    "AUTO": 7, "SAVE": 8, "MENU": 9, "UP": 10, "DOWN": 11, "LEFT": 12,
    "RIGHT": 13, "OK": 14, "POWER": 15,
}

MODES = {0: "scope", 1: "meter", 2: "siggen", 3: "settings"}

FLAG_CHARGING = 0x01
FLAG_CAPTURE_READY = 0x02
FLAG_BATT_CRITICAL = 0x04
FLAG_BATT_UNKNOWN = 0x08


class ProtocolError(Exception):
    """A frame or payload that cannot be what it claims to be."""


def checksum(data: bytes) -> int:
    c = 0
    for b in data:
        c ^= b
    return c


def encode(cmd: int, payload: bytes = b"") -> bytes:
    """One host->device frame. Refuses payloads the device would drop."""
    if not 0 <= cmd <= 0xFF:
        raise ValueError(f"cmd out of range: {cmd}")
    if len(payload) > MAX_TX_PAYLOAD:
        raise ValueError(f"payload {len(payload)} B exceeds the device receive cap "
                         f"({MAX_TX_PAYLOAD} B); it would be NAKed as BAD_LENGTH")
    body = bytes([cmd, len(payload) >> 8, len(payload) & 0xFF]) + bytes(payload)
    return bytes([SYNC]) + body + bytes([checksum(body)])


@dataclass(frozen=True)
class Frame:
    cmd: int
    payload: bytes


class Decoder:
    """Incremental decoder for the device->host stream.

    The stream is shared with the ASCII shell, so bytes outside frames are
    expected (banner, echoes, prompts) and are collected as `text`. A byte
    0xAA that does not start a frame with a valid checksum is treated as
    noise: the decoder skips exactly that byte and rescans, so one corrupt
    or truncated frame cannot hide the next good one (resync).

    A frame is only returned once its checksum byte has arrived and matches.
    """

    def __init__(self, max_payload: int = MAX_RX_PAYLOAD):
        self._buf = bytearray()
        self.max_payload = max_payload
        self.text = bytearray()
        self.bad_checksum = 0
        self.resyncs = 0

    def feed(self, data: bytes) -> List[Frame]:
        self._buf += data
        frames: List[Frame] = []
        while True:
            i = self._buf.find(SYNC)
            if i < 0:
                self.text += self._buf
                self._buf.clear()
                return frames
            if i:
                self.text += self._buf[:i]
                del self._buf[:i]
            if len(self._buf) < HEADER_LEN:
                return frames                       # wait for the header
            n = (self._buf[2] << 8) | self._buf[3]
            if n > self.max_payload:
                self._skip_sync()
                continue
            total = HEADER_LEN + n + 1
            if len(self._buf) < total:
                return frames                       # truncated so far: never accept
            body = bytes(self._buf[1:HEADER_LEN + n])
            if checksum(body) != self._buf[total - 1]:
                self.bad_checksum += 1
                self._skip_sync()
                continue
            frames.append(Frame(self._buf[1], bytes(self._buf[HEADER_LEN:HEADER_LEN + n])))
            del self._buf[:total]

    def pending(self) -> int:
        """Bytes held back waiting for the rest of a possible frame."""
        return len(self._buf)

    def take_text(self) -> bytes:
        t = bytes(self.text)
        self.text.clear()
        return t

    def _skip_sync(self) -> None:
        # The 0xAA was not a frame start: it is text/noise. Keep scanning after it.
        self.resyncs += 1
        self.text.append(self._buf[0])
        del self._buf[:1]


@dataclass(frozen=True)
class Status:
    proto_version: int
    mode: int
    battery_pct: int
    flags: int
    battery_mv: int
    uptime_ms: int
    usb_tx_stalls: int
    usb_heals: int
    fw_version: str

    @property
    def mode_name(self) -> str:
        return MODES.get(self.mode, f"unknown({self.mode})")

    @property
    def charging(self) -> bool:
        return bool(self.flags & FLAG_CHARGING)

    @property
    def capture_ready(self) -> bool:
        return bool(self.flags & FLAG_CAPTURE_READY)

    @property
    def battery_critical(self) -> bool:
        return bool(self.flags & FLAG_BATT_CRITICAL)

    @property
    def battery_known(self) -> bool:
        return not (self.flags & FLAG_BATT_UNKNOWN)


STATUS_FIXED = struct.Struct("<BBBBHIIHB")   # 17 bytes, esp_comm.h STATUS v1


def parse_status(payload: bytes) -> Status:
    if len(payload) < STATUS_FIXED.size:
        raise ProtocolError(f"STATUS too short: {len(payload)} B < {STATUS_FIXED.size}")
    (ver, mode, pct, flags, mv, up, stalls, heals, fw_len) = STATUS_FIXED.unpack_from(payload)
    if ver != PROTO_MAJOR:
        raise ProtocolError(f"device speaks protocol v{ver}, this tool speaks v{PROTO_MAJOR}")
    if len(payload) != STATUS_FIXED.size + fw_len:
        raise ProtocolError(f"STATUS length {len(payload)} != {STATUS_FIXED.size} + fw_len {fw_len}")
    fw = payload[STATUS_FIXED.size:].decode("ascii", "replace")
    return Status(ver, mode, pct, flags, mv, up, stalls, heals, fw)


RESULT_CLASSES = {0: "none", 1: "normal", 2: "underrange", 3: "overrange", 4: "invalid",
                  5: "overload", 6: "blank", 7: "continuity"}
METER_FIXED = struct.Struct("<IfhBBBBBB")   # 16 bytes up to and including unit_len


@dataclass(frozen=True)
class MeterReading:
    update_count: int
    value: float
    raw_bcd: int
    decimal_pos: int
    result_class: int
    flags: int
    submode: int
    unit_variant: int
    unit: str
    display: str

    @property
    def result(self) -> str:
        return RESULT_CLASSES.get(self.result_class, f"class{self.result_class}")

    @property
    def negative(self) -> bool:
        return bool(self.flags & 0x01)

    @property
    def ac(self) -> bool:
        return bool(self.flags & 0x02)

    @property
    def autorange(self) -> bool:
        return bool(self.flags & 0x04)

    @property
    def hold(self) -> bool:
        return bool(self.flags & 0x08)


def parse_meter(payload: bytes) -> MeterReading:
    if len(payload) < METER_FIXED.size + 1:
        raise ProtocolError(f"METER_FRAME too short: {len(payload)} B")
    (count, value, bcd, dp, cls, flags, sub, var, unit_len) = METER_FIXED.unpack_from(payload)
    i = METER_FIXED.size
    unit = payload[i:i + unit_len]
    i += unit_len
    if i >= len(payload):
        raise ProtocolError("METER_FRAME truncated before display text")
    disp_len = payload[i]
    disp = payload[i + 1:i + 1 + disp_len]
    if len(unit) != unit_len or len(disp) != disp_len or i + 1 + disp_len != len(payload):
        raise ProtocolError("METER_FRAME length does not match its string lengths")
    return MeterReading(count, value, bcd, dp, cls, flags, sub, var,
                        unit.decode("ascii", "replace"), disp.decode("ascii", "replace"))


# ── WAVEFORM_FRAME v1 (esp_comm.h, remote_protocol.md §3.5) ─────────────
WAVE_FIXED = struct.Struct("<IBBBBHHIIHH")     # 24 bytes; header_len may grow
WAVE_MASK_CH1, WAVE_MASK_CH2 = 0x01, 0x02

WAVE_FLAG_CALIBRATED = 0x01        # per-unit calibration (always 0 today)
WAVE_FLAG_TB_MEASURED = 0x02       # sample_rate_hz: bench-measured, tier MEASURED
WAVE_FLAG_VDIV_MEASURED = 0x04     # uv_per_div: bench-measured, tier MEASURED
WAVE_FLAG_SYNTHETIC = 0x08         # never sent by the firmware; refused here if seen
WAVE_FLAG_TB_PROVISIONAL = 0x10    # rate right order of magnitude only
WAVE_FLAG_VDIV_PROVISIONAL = 0x20
WAVE_FLAG_TIME_ORDERED = 0x40      # hardware trigger at index 512
WAVE_FLAG_TB_DISAGREES = 0x80      # display timebase != the one in force: rate withheld


def channel_mask(channels) -> int:
    """(1,), [1, 2], "1,2", 3 -> the GET_WAVEFORM mask byte (bit0 CH1, bit1 CH2)."""
    if isinstance(channels, int):
        chans = [channels]
    elif isinstance(channels, str):
        chans = [c for c in channels.replace(" ", "").split(",") if c]
    else:
        chans = list(channels)
    mask = 0
    for c in chans:
        n = int(str(c).upper().replace("CH", ""))
        if n not in (1, 2):
            raise ValueError(f"channel {c!r}: the 2C53T has CH1 and CH2")
        mask |= 1 << (n - 1)
    if not mask:
        raise ValueError("no channel requested")
    return mask


@dataclass(frozen=True)
class Waveform:
    """One channel of one capture, exactly as the instrument reported it.

    `samples` are raw unsigned ADC counts. Volts need a gain AND a zero point;
    only the gain has been measured (bench unit #1), so `volts_per_count` gives
    amplitudes (Vpp), never absolute volts."""
    frame_id: int
    channel: int                # 0 = CH1, 1 = CH2
    flags: int
    timebase_idx: int           # reg 0x01 code in force
    vdiv_idx: int
    sample_count: int
    header_len: int
    sample_rate_hz: int         # 0 = no trustworthy rate
    uv_per_div: int             # 0 = no volts meaning on this range
    counts_per_div: int
    head_skip: int              # samples [0, head_skip) are the record-head defect
    samples: bytes

    @property
    def channel_name(self) -> str:
        return f"CH{self.channel + 1}"

    @property
    def calibrated(self) -> bool:
        return bool(self.flags & WAVE_FLAG_CALIBRATED)

    @property
    def timebase_measured(self) -> bool:
        return bool(self.flags & WAVE_FLAG_TB_MEASURED)

    @property
    def timebase_provisional(self) -> bool:
        return bool(self.flags & WAVE_FLAG_TB_PROVISIONAL)

    @property
    def vdiv_measured(self) -> bool:
        return bool(self.flags & WAVE_FLAG_VDIV_MEASURED)

    @property
    def vdiv_provisional(self) -> bool:
        return bool(self.flags & WAVE_FLAG_VDIV_PROVISIONAL)

    @property
    def time_ordered(self) -> bool:
        return bool(self.flags & WAVE_FLAG_TIME_ORDERED)

    @property
    def timebase_disagrees(self) -> bool:
        return bool(self.flags & WAVE_FLAG_TB_DISAGREES)

    @property
    def timebase_tier(self) -> str:
        return "measured" if self.timebase_measured else (
            "provisional" if self.timebase_provisional else "none")

    @property
    def vdiv_tier(self) -> str:
        return "measured" if self.vdiv_measured else (
            "provisional" if self.vdiv_provisional else "none")

    @property
    def volts_per_count(self) -> Optional[float]:
        """Gain only (bench unit #1, at the BNC, no probe factor); None if unmeasured."""
        if not self.uv_per_div or not self.counts_per_div:
            return None
        return self.uv_per_div / 1e6 / self.counts_per_div

    @property
    def seconds_per_sample(self) -> Optional[float]:
        return 1.0 / self.sample_rate_hz if self.sample_rate_hz else None

    @property
    def body(self) -> bytes:
        """The samples outside the known head defect — what to analyse."""
        return self.samples[min(self.head_skip, len(self.samples)):]

    def header(self) -> dict:
        return {
            "frame_id": self.frame_id, "channel": self.channel_name, "flags": self.flags,
            "timebase_idx": self.timebase_idx, "vdiv_idx": self.vdiv_idx,
            "sample_count": self.sample_count, "sample_rate_hz": self.sample_rate_hz or None,
            "timebase_tier": self.timebase_tier, "timebase_disagrees": self.timebase_disagrees,
            "uv_per_div": self.uv_per_div or None, "vdiv_tier": self.vdiv_tier,
            "counts_per_div": self.counts_per_div, "head_skip": self.head_skip,
            "time_ordered": self.time_ordered, "calibrated": self.calibrated,
        }

    def summary(self) -> dict:
        """Compact, honest description for a reader that will not plot 1024
        numbers: statistics over the body only, a period from level crossings,
        and volts/Hz only where the instrument has a measured number for them."""
        body = self.body
        out: dict = {"analysed_from": min(self.head_skip, len(self.samples)), "n": len(body)}
        if not body:
            out["note"] = "no samples outside the record-head defect"
            return out
        lo, hi = min(body), max(body)
        mean = sum(body) / len(body)
        out.update(min_counts=lo, max_counts=hi, mean_counts=round(mean, 2), span_counts=hi - lo)
        if lo == 0 or hi == 255:
            out["clipped"] = True       # railed: amplitude and period are lower bounds at best
        k = self.volts_per_count
        if k is not None:
            out["vpp_volts"] = round((hi - lo) * k, 4)
            out["vpp_note"] = ("from the bench-unit-#1 gain of this range (" + self.vdiv_tier +
                               "); uncalibrated for this unit, zero point unknown")
        else:
            out["vpp_volts"] = None
            out["vpp_note"] = (f"range {self.vdiv_idx} has no measured volts/div: "
                               "amplitudes are raw ADC counts")
        period = estimate_period(body)
        out["period_samples"] = None if period is None else round(period, 2)
        if period is None:
            out["period_note"] = "no repeating crossings found (flat, noise, or < 2 periods in the record)"
        elif self.sample_rate_hz:
            out["frequency_hz"] = round(self.sample_rate_hz / period, 3)
            out["frequency_note"] = f"timebase rate {self.timebase_tier} (bench unit #1)"
        else:
            out["frequency_hz"] = None
            out["frequency_note"] = (
                "display and hardware timebase disagree: rate withheld" if self.timebase_disagrees
                else f"timebase code 0x{self.timebase_idx:02X} has no trustworthy sample rate: "
                     "period is in samples only")
        return out


def estimate_period(samples, min_span: int = 4) -> Optional[float]:
    """Mean period, in samples, between rising crossings of the midpoint, with
    hysteresis of a quarter of the span so noise near the level cannot add
    crossings. Linear interpolation puts each crossing between samples. None
    when the record is flat (< min_span counts) or holds < 2 full periods."""
    if len(samples) < 3:
        return None
    lo, hi = min(samples), max(samples)
    if hi - lo < min_span:
        return None
    mid = (lo + hi) / 2.0
    hyst = (hi - lo) / 4.0
    armed = False
    crossings: List[float] = []
    for i in range(1, len(samples)):
        a, b = samples[i - 1], samples[i]
        if b <= mid - hyst:
            armed = True
        if armed and a < mid <= b:
            crossings.append(i - 1 + (mid - a) / (b - a))
            armed = False
    if len(crossings) < 3:              # >= 2 full periods
        return None
    return (crossings[-1] - crossings[0]) / (len(crossings) - 1)


def parse_waveform(payload: bytes) -> Waveform:
    if len(payload) < WAVE_FIXED.size:
        raise ProtocolError(f"WAVEFORM_FRAME too short: {len(payload)} B < {WAVE_FIXED.size}")
    (fid, ch, flags, tb, vdiv, count, hlen, rate, uv, cpd, skip) = WAVE_FIXED.unpack_from(payload)
    if hlen < WAVE_FIXED.size:
        raise ProtocolError(f"WAVEFORM_FRAME header_len {hlen} < {WAVE_FIXED.size}")
    if len(payload) != hlen + count:
        raise ProtocolError(f"WAVEFORM_FRAME length {len(payload)} != header {hlen} + {count} samples")
    if ch not in (0, 1):
        raise ProtocolError(f"WAVEFORM_FRAME channel {ch}: the 2C53T has CH1 and CH2")
    if flags & WAVE_FLAG_SYNTHETIC:
        # The firmware refuses rather than sends these (§2.3). A frame that
        # says it is not a capture is never handed to a caller as data.
        raise ProtocolError("device sent a frame flagged SYNTHETIC; refusing it (not a capture)")
    if fid == 0:
        raise ProtocolError("WAVEFORM_FRAME with frame_id 0: no committed record behind it")
    return Waveform(fid, ch, flags, tb, vdiv, count, hlen, rate, uv, cpd, skip,
                    bytes(payload[hlen:hlen + count]))


NAK_HINTS = {
    "NO_CAPTURE_DATA": "the scope has no real capture yet (nothing acquired since boot, or the FPGA "
                       "is not capturing); the device never substitutes its demo trace",
    "UNSUPPORTED_IN_MODE": "the scope is not in oscilloscope mode, so its capture buffers are not live",
    "NOT_READY": "no tear-free copy of the capture could be taken; try again",
    "UNSUPPORTED": "this firmware does not implement the request",
}


def nak_name(payload: bytes) -> str:
    if not payload:
        return "NAK(?)"
    return ERRORS.get(payload[0], f"0x{payload[0]:02X}")


def button_id(name_or_id) -> int:
    if isinstance(name_or_id, int):
        bid = name_or_id
    else:
        key = str(name_or_id).upper()
        if key.isdigit():
            bid = int(key)
        elif key in BUTTONS:
            bid = BUTTONS[key]
        else:
            raise ValueError(f"unknown button {name_or_id!r}; one of {', '.join(BUTTONS)}")
    if not 1 <= bid <= 15:
        raise ValueError(f"button id {bid} out of range 1..15")
    return bid


def find_frame(frames: List[Frame], wanted: set) -> Optional[Frame]:
    for f in frames:
        if f.cmd in wanted:
            return f
    return None
