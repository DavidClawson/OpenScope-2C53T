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
# is larger (a waveform frame is 1036 B). Bounding it matters: a stray 0xAA in
# shell text followed by bytes that read as a huge length would otherwise make
# the decoder wait for data that never comes and hide every good frame after.
MAX_RX_PAYLOAD = 4096

PROTO_MAJOR = 1             # STATUS byte 0; refuse other majors (§3.7)

# Host -> device
CMD_PING = 0x01
CMD_STATUS = 0x08
CMD_BUTTON = 0x0A
CMD_GET_METER = 0x21

# Device -> host
RSP_ACK = 0x81
RSP_NAK = 0x82
RSP_DATA = 0x83
RSP_STATUS = 0x85
RSP_METER_FRAME = 0x90

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
