#!/usr/bin/env python3
"""
Host-side USB debug helper for OpenScope 2C53T live DMM validation.

Examples:
    python3 scripts/openscope_live_debug.py list
    python3 scripts/openscope_live_debug.py command "status"
    python3 scripts/openscope_live_debug.py command "meter wave" --timeout 4
    python3 scripts/openscope_live_debug.py meter-dump --interval 0.25 --count 20
    python3 scripts/openscope_live_debug.py meter-mux-stream --count 32 --delay-ms 250
    python3 scripts/openscope_live_debug.py meter-adc-snapshot
    python3 scripts/openscope_live_debug.py poll "meter wave" --duration 10 --log tmp/meter-wave.log

Except for list/help, commands open the selected USB CDC serial port. The meter
frontend, mux stream, ADC snapshot, dump, and poll helpers are read-only; modes
that intentionally change device state should be run through the generic
command path so the command text is explicit.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager, nullcontext
import fcntl
import glob
import os
from pathlib import Path
import select
import sys
import termios
import time
from typing import Iterable, TextIO
import zlib


DEFAULT_BAUD = 115200
DEFAULT_COMMAND = "meter dump"
DEFAULT_TIMEOUT = 2.0
PROMPT_SUFFIXES = (b"\n> ", b"\r\n> ", b"> ")
SHADOW_PALETTE_RGB565 = (
    0x0000,
    0xFFFF,
    0xF800,
    0x07E0,
    0x001F,
    0xFFE0,
    0x07FF,
    0xF81F,
    0x2104,
    0x8410,
    0xFCA0,
    0x055F,
    0x2945,
    0x18C3,
    0x3186,
    0x6BB0,
)


class SerialError(RuntimeError):
    """Raised for serial setup and communication failures."""


@contextmanager
def serial_device_lock(port: str) -> Iterable[None]:
    lock_dir = Path(os.environ.get("OPENSCOPE_LOCK_DIR", "/tmp"))
    real_port = os.path.realpath(port)
    lock_name = "openscope-" + "".join(
        ch if ch.isalnum() or ch in "._-" else "_" for ch in real_port
    ) + ".lock"
    lock_path = lock_dir / lock_name
    with lock_path.open("w") as lock_file:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)


class PosixSerial:
    """Small USB CDC serial wrapper using only Python's standard library."""

    def __init__(self, port: str, baud: int, read_timeout: float) -> None:
        self.port = port
        self.read_timeout = read_timeout
        self._rx_buffer = bytearray()
        try:
            self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError as exc:
            raise SerialError(f"failed to open {port}: {exc}") from exc

        try:
            self._configure(baud)
        except Exception:
            os.close(self.fd)
            raise

    def _configure(self, baud: int) -> None:
        baud_attr = _baud_attr(baud)
        try:
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = 0
            attrs[1] = 0
            attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
            attrs[3] = 0
            attrs[4] = baud_attr
            attrs[5] = baud_attr
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            termios.tcflush(self.fd, termios.TCIOFLUSH)
        except termios.error as exc:
            raise SerialError(f"failed to configure {self.port}: {exc}") from exc

    def write_line(self, text: str) -> None:
        data = (text.rstrip("\r\n") + "\r").encode("utf-8")
        offset = 0
        deadline = time.monotonic() + self.read_timeout
        while offset < len(data):
            if time.monotonic() >= deadline:
                raise SerialError(f"write timeout on {self.port}")
            _, writable, _ = select.select([], [self.fd], [], 0.05)
            if not writable:
                continue
            offset += os.write(self.fd, data[offset:])

    def _read_to_buffer(self, deadline: float) -> None:
        remaining = max(0.0, deadline - time.monotonic())
        readable, _, _ = select.select([self.fd], [], [], min(0.05, remaining))
        if not readable:
            return
        try:
            chunk = os.read(self.fd, 4096)
        except BlockingIOError:
            return
        if chunk:
            self._rx_buffer.extend(chunk)

    def read_until_prompt(self, timeout: float) -> bytes:
        response = bytearray(self._rx_buffer)
        self._rx_buffer.clear()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if any(response.endswith(suffix) for suffix in PROMPT_SUFFIXES):
                return bytes(response)
            self._read_to_buffer(deadline)
            if self._rx_buffer:
                response.extend(self._rx_buffer)
                self._rx_buffer.clear()
        raise TimeoutError(f"no prompt from {self.port} within {timeout:.1f}s")

    def read_line(self, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            newline = self._rx_buffer.find(b"\n")
            if newline >= 0:
                line = bytes(self._rx_buffer[: newline + 1])
                del self._rx_buffer[: newline + 1]
                return line
            self._read_to_buffer(deadline)
        raise TimeoutError(f"no line from {self.port} within {timeout:.1f}s")

    def read_exact(self, length: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while len(self._rx_buffer) < length and time.monotonic() < deadline:
            self._read_to_buffer(deadline)
        if len(self._rx_buffer) < length:
            raise TimeoutError(
                f"only read {len(self._rx_buffer)} of {length} payload bytes from {self.port}"
            )
        data = bytes(self._rx_buffer[:length])
        del self._rx_buffer[:length]
        return data

    def drain(self, quiet_window: float = 0.15, max_wait: float = 1.0) -> bytes:
        response = bytearray()
        deadline = time.monotonic() + max_wait
        quiet_deadline = time.monotonic() + quiet_window
        while time.monotonic() < deadline and time.monotonic() < quiet_deadline:
            readable, _, _ = select.select([self.fd], [], [], 0.05)
            if not readable:
                continue
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                continue
            if chunk:
                response.extend(chunk)
                quiet_deadline = time.monotonic() + quiet_window
        return bytes(response)

    def close(self) -> None:
        os.close(self.fd)

    def __enter__(self) -> "PosixSerial":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


def _baud_attr(baud: int) -> int:
    name = f"B{baud}"
    value = getattr(termios, name, None)
    if value is None:
        raise SerialError(f"unsupported baud rate for termios: {baud}")
    return value


def discover_ports() -> list[str]:
    patterns = [
        "/dev/cu.usbmodem*",
        "/dev/tty.usbmodem*",
        "/dev/ttyACM*",
        "/dev/ttyUSB*",
        "/dev/serial/by-id/*OpenScope*",
        "/dev/serial/by-id/*FNIRSI*",
        "/dev/serial/by-id/*2C53T*",
        "/dev/serial/by-id/*usbmodem*",
    ]
    ports: list[str] = []
    seen: set[str] = set()
    for pattern in patterns:
        for raw_path in glob.glob(pattern):
            path = str(Path(raw_path))
            if path not in seen:
                ports.append(path)
                seen.add(path)
    return sorted(ports)


def choose_port(port: str | None) -> str:
    if port:
        return port
    ports = discover_ports()
    if not ports:
        raise SerialError("no candidate USB CDC serial ports found")
    if len(ports) > 1:
        formatted = "\n".join(f"  {item}" for item in ports)
        raise SerialError(
            "multiple candidate ports found; pass --port explicitly:\n" + formatted
        )
    return ports[0]


def clean_response(command: str, data: bytes) -> str:
    text = data.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    marker = "\n" + command
    marker_pos = text.find(marker)
    if marker_pos >= 0:
        text = text[marker_pos + 1 :]
    elif text.startswith(command):
        pass

    cleaned: list[str] = []
    echo_removed = False
    for line in text.split("\n"):
        stripped = line.strip()
        if not echo_removed and stripped == command:
            echo_removed = True
            continue
        if stripped == ">":
            continue
        if stripped.endswith(">") and stripped[:-1].strip() == "":
            continue
        cleaned.append(line.rstrip())
    return "\n".join(cleaned).strip()


def run_command(port: str, baud: int, command: str, timeout: float) -> str:
    with PosixSerial(port, baud, timeout) as serial:
        serial.drain()
        serial.write_line(command)
        return clean_response(command, serial.read_until_prompt(timeout))


def rgb565_to_rgb888(pixel: int) -> tuple[int, int, int]:
    r5 = (pixel >> 11) & 0x1F
    g6 = (pixel >> 5) & 0x3F
    b5 = pixel & 0x1F
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))


def parse_screen_dump(text: str) -> tuple[int, int, int, int, str, list[list[int]]]:
    header = None
    rows: list[list[int]] = []
    dump_format = "rgb565"

    for line in text.splitlines():
        if line.startswith("SCREENDUMP "):
            header = line

    if header is None:
        raise ValueError("screen dump response has no SCREENDUMP header")

    fields: dict[str, str] = {}
    for item in header.split()[1:]:
        if "=" in item:
            key, value = item.split("=", 1)
            fields[key] = value

    try:
        x = int(fields["x"], 10)
        y = int(fields["y"], 10)
        w = int(fields["w"], 10)
        h = int(fields["h"], 10)
        dump_format = fields.get("format", "rgb565")
    except KeyError as exc:
        raise ValueError(f"screen dump header missing {exc.args[0]!r}") from exc

    rows = []
    for line in text.splitlines():
        if not line.startswith("ROW "):
            continue
        parts = line.split(" ", 2)
        if len(parts) != 3:
            raise ValueError(f"bad ROW line: {line[:80]!r}")
        row_index = int(parts[1], 10)
        payload = parts[2].strip()
        if row_index != len(rows):
            raise ValueError(f"expected ROW {len(rows)}, got ROW {row_index}")

        if dump_format == "rgb565":
            if len(payload) % 4 != 0:
                raise ValueError(f"ROW {row_index} has odd RGB565 hex length")
            pixels = [int(payload[i : i + 4], 16) for i in range(0, len(payload), 4)]
        elif dump_format in ("rgb565-rle", "rgb888-rle565", "rgb888h-rle565"):
            pixels = []
            if payload:
                for item in payload.split():
                    if len(item) != 9 or item[4] != ":":
                        raise ValueError(f"bad RLE item in ROW {row_index}: {item!r}")
                    count = int(item[:4], 16)
                    color = int(item[5:], 16)
                    pixels.extend([color] * count)
        elif dump_format == "mono1":
            pixels = []
            for item in payload:
                if item == "0":
                    pixels.append(0x0000)
                elif item == "1":
                    pixels.append(0xFFFF)
                else:
                    raise ValueError(f"bad mono1 bit in ROW {row_index}: {item!r}")
        elif dump_format == "indexed4":
            pixels = []
            for item in payload:
                try:
                    pixels.append(SHADOW_PALETTE_RGB565[int(item, 16)])
                except (ValueError, IndexError) as exc:
                    raise ValueError(f"bad indexed4 nibble in ROW {row_index}: {item!r}") from exc
        else:
            raise ValueError(f"unsupported screen dump format: {dump_format}")
        rows.append(pixels)

    if len(rows) != h:
        raise ValueError(f"expected {h} rows, got {len(rows)}")
    for row_index, row in enumerate(rows):
        if len(row) != w:
            raise ValueError(f"ROW {row_index} expected {w} pixels, got {len(row)}")
    return x, y, w, h, dump_format, rows


def parse_screenbin_header(line: bytes) -> tuple[int, int, int, int, str, int, int]:
    text = line.decode("ascii", errors="replace").strip()
    if not text.startswith("SCREENBIN "):
        raise ValueError(f"screen dumpbin response has no SCREENBIN header: {text[:80]!r}")

    fields: dict[str, str] = {}
    for item in text.split()[1:]:
        if "=" in item:
            key, value = item.split("=", 1)
            fields[key] = value

    try:
        x = int(fields["x"], 10)
        y = int(fields["y"], 10)
        w = int(fields["w"], 10)
        h = int(fields["h"], 10)
        dump_format = fields["format"]
        payload_len = int(fields["len"], 10)
        crc32 = int(fields["crc32"], 16)
    except KeyError as exc:
        raise ValueError(f"screen dumpbin header missing {exc.args[0]!r}") from exc

    if dump_format != "indexed4":
        raise ValueError(f"unsupported screen dumpbin format: {dump_format}")
    expected_len = ((w + 1) // 2) * h
    if payload_len != expected_len:
        raise ValueError(f"screen dumpbin len {payload_len} does not match expected {expected_len}")
    return x, y, w, h, dump_format, payload_len, crc32


def unpack_indexed4_payload(width: int, height: int, payload: bytes) -> list[list[int]]:
    row_len = (width + 1) // 2
    if len(payload) != row_len * height:
        raise ValueError(f"indexed4 payload size {len(payload)} does not match {width}x{height}")

    rows: list[list[int]] = []
    for row_index in range(height):
        row_bytes = payload[row_index * row_len : (row_index + 1) * row_len]
        row: list[int] = []
        for byte in row_bytes:
            row.append(SHADOW_PALETTE_RGB565[byte >> 4])
            if len(row) < width:
                row.append(SHADOW_PALETTE_RGB565[byte & 0x0F])
        rows.append(row)
    return rows


def run_screen_dumpbin(
    port: str,
    baud: int,
    command: str,
    timeout: float,
) -> tuple[int, int, int, int, str, list[list[int]]]:
    with PosixSerial(port, baud, timeout) as serial:
        serial.drain()
        serial.write_line(command)
        header = None
        while header is None:
            line = serial.read_line(timeout)
            if line.strip().startswith(b"SCREENBIN "):
                header = line
                break
            if line.strip().startswith((b"Usage:", b"ERR", b"Unknown command:")):
                raise ValueError(line.decode("utf-8", errors="replace").strip())

        x, y, w, h, dump_format, payload_len, expected_crc = parse_screenbin_header(header)
        payload = serial.read_exact(payload_len, timeout)
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError(
                f"screen dumpbin CRC mismatch: got {actual_crc:08X}, expected {expected_crc:08X}"
            )

        trailer = serial.read_until_prompt(timeout)
        if b"SCREENBIN END" not in trailer:
            raise ValueError("screen dumpbin trailer missing SCREENBIN END")

    rows = unpack_indexed4_payload(w, h, payload)
    return x, y, w, h, dump_format, rows


def write_bmp_rgb565(path: Path, width: int, height: int, rows: list[list[int]]) -> None:
    row_stride = ((width * 3 + 3) // 4) * 4
    pixel_bytes = row_stride * height
    file_size = 14 + 40 + pixel_bytes

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(b"BM")
        f.write(file_size.to_bytes(4, "little"))
        f.write((0).to_bytes(4, "little"))
        f.write((14 + 40).to_bytes(4, "little"))

        f.write((40).to_bytes(4, "little"))
        f.write(width.to_bytes(4, "little"))
        f.write(height.to_bytes(4, "little"))
        f.write((1).to_bytes(2, "little"))
        f.write((24).to_bytes(2, "little"))
        f.write((0).to_bytes(4, "little"))
        f.write(pixel_bytes.to_bytes(4, "little"))
        f.write((0).to_bytes(4, "little"))
        f.write((0).to_bytes(4, "little"))
        f.write((0).to_bytes(4, "little"))
        f.write((0).to_bytes(4, "little"))

        padding = b"\x00" * (row_stride - width * 3)
        for row in reversed(rows):
            for pixel in row:
                r, g, b = rgb565_to_rgb888(pixel)
                f.write(bytes((b, g, r)))
            f.write(padding)


def capture_screen(
    port: str,
    baud: int,
    timeout: float,
    output: Path,
    region: tuple[int, int, int, int] | None,
    use_rle: bool,
) -> str:
    if use_rle:
        capture_region = region or (0, 0, 320, 240)
        rx, ry, rw, rh = capture_region
        if rx < 0 or ry < 0 or rw <= 0 or rh <= 0 or rx + rw > 320 or ry + rh > 240:
            raise ValueError("screen region must satisfy 0<=x,y and x+w<=320 y+h<=240")

        full_rows: list[list[int]] = []
        cursor_y = ry
        end_y = ry + rh
        while cursor_y < end_y:
            page_y = min(cursor_y, 240 - 16)
            page_h = min(end_y - cursor_y, page_y + 16 - cursor_y)
            if page_h <= 0:
                raise ValueError(f"internal shadow page error at y={cursor_y}")
            run_command(port, baud, "screen shadow page %u" % page_y, timeout)
            time.sleep(0.1)
            run_command(port, baud, "mode meter 1 0", timeout)
            time.sleep(0.5)
            response = run_command(
                port,
                baud,
                "screen dump shadow %u %u %u %u" % (rx, cursor_y, rw, page_h),
                timeout,
            )
            _x, _y, _w, _h, dump_format, rows = parse_screen_dump(response)
            if dump_format != "indexed4":
                raise ValueError(f"expected indexed4 shadow dump, got {dump_format}")
            full_rows.extend(rows)
            cursor_y += page_h
        write_bmp_rgb565(output, rw, rh, full_rows)
        return (
            f"saved {output} from stitched LCD shadow region "
            f"x={rx} y={ry} w={rw} h={rh} format=indexed4"
        )

    prefix = "screen dumpbin"
    if region is None:
        command = prefix
    else:
        command = "%s %u %u %u %u" % ((prefix,) + region)
    try:
        x, y, w, h, dump_format, rows = run_screen_dumpbin(port, baud, command, timeout)
    except TimeoutError as exc:
        text_prefix = "screen dump"
        if region is None:
            text_command = text_prefix
        else:
            text_command = "%s %u %u %u %u" % ((text_prefix,) + region)
        response = run_command(port, baud, text_command, timeout)
        x, y, w, h, dump_format, rows = parse_screen_dump(response)
        dump_format = f"{dump_format} text-fallback after dumpbin failure: {exc}"
    except ValueError as exc:
        if not any(marker in str(exc) for marker in ("Unknown command", "Usage: screen dumpbin")):
            raise
        text_prefix = "screen dump"
        if region is None:
            text_command = text_prefix
        else:
            text_command = "%s %u %u %u %u" % ((text_prefix,) + region)
        response = run_command(port, baud, text_command, timeout)
        x, y, w, h, dump_format, rows = parse_screen_dump(response)
        dump_format = f"{dump_format} text-fallback after dumpbin unsupported: {exc}"
    write_bmp_rgb565(output, w, h, rows)
    return f"saved {output} from screen region x={x} y={y} w={w} h={h} format={dump_format}"


def write_log_line(log_file: TextIO | None, line: str) -> None:
    if log_file is None:
        return
    log_file.write(line + "\n")
    log_file.flush()


def poll_command(
    port: str,
    baud: int,
    command: str,
    timeout: float,
    interval: float,
    count: int | None,
    duration: float | None,
    log_file: TextIO | None,
) -> int:
    started = time.monotonic()
    iteration = 0
    with PosixSerial(port, baud, timeout) as serial:
        serial.drain()
        while True:
            if count is not None and iteration >= count:
                return 0
            if duration is not None and time.monotonic() - started >= duration:
                return 0

            iteration += 1
            stamp = time.strftime("%Y-%m-%d %H:%M:%S")
            try:
                serial.write_line(command)
                response = clean_response(command, serial.read_until_prompt(timeout))
                header = f"[{stamp}] poll={iteration} command={command!r}"
                print(header)
                print(response if response else "(empty response)")
                write_log_line(log_file, header)
                write_log_line(log_file, response if response else "(empty response)")
            except TimeoutError as exc:
                line = f"[{stamp}] poll={iteration} timeout: {exc}"
                print(line, file=sys.stderr)
                write_log_line(log_file, line)
                return 3
            except SerialError as exc:
                line = f"[{stamp}] poll={iteration} serial error: {exc}"
                print(line, file=sys.stderr)
                write_log_line(log_file, line)
                return 2

            if count is not None and iteration >= count:
                return 0
            if duration is not None and time.monotonic() - started >= duration:
                return 0
            time.sleep(max(0.0, interval))


def add_common_serial_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--port", help="USB CDC serial port. Omit only when discovery finds exactly one candidate.")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"serial baud rate, default {DEFAULT_BAUD}")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help=f"per-command timeout in seconds, default {DEFAULT_TIMEOUT}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="OpenScope 2C53T host-side USB debug helper for live DMM checks."
    )
    subparsers = parser.add_subparsers(dest="mode", required=True)

    list_parser = subparsers.add_parser("list", help="list candidate serial ports without opening them")
    list_parser.add_argument("--plain", action="store_true", help="print only device paths")

    command_parser = subparsers.add_parser("command", help="send one debug-shell command and print the response")
    add_common_serial_args(command_parser)
    command_parser.add_argument("command", help='debug-shell command, for example "status", "meter wave", or "mode meter 1 0"')

    poll_parser = subparsers.add_parser("poll", help="send one command repeatedly")
    add_common_serial_args(poll_parser)
    poll_parser.add_argument("command", help='debug-shell command to poll, for example "meter wave" or "meter frontend"')
    add_poll_args(poll_parser)

    meter_parser = subparsers.add_parser("meter-dump", help='poll the live DMM dump command, default "meter dump"')
    add_common_serial_args(meter_parser)
    meter_parser.add_argument("--command", default=DEFAULT_COMMAND, help=f"DMM dump command, default {DEFAULT_COMMAND!r}")
    add_poll_args(meter_parser)

    frontend_parser = subparsers.add_parser("meter-frontend", help="read current DMM analog frontend GPIO state")
    add_common_serial_args(frontend_parser)
    frontend_parser.add_argument("--log", type=Path, help="optional log file for the single frontend response")

    stream_parser = subparsers.add_parser("meter-stream", help='run firmware-side compact stream, default "meter stream"')
    add_common_serial_args(stream_parser)
    stream_parser.add_argument("--count", type=int, default=32, help="firmware stream count, default 32")
    stream_parser.add_argument("--delay-ms", type=int, default=250, help="firmware stream delay in milliseconds, default 250")
    stream_parser.add_argument("--log", type=Path, help="optional log file for the single stream response")

    mux_stream_parser = subparsers.add_parser(
        "meter-mux-stream",
        help="run firmware-side stream with DMM frame and frontend GPIO state",
    )
    add_common_serial_args(mux_stream_parser)
    mux_stream_parser.add_argument("--count", type=int, default=32, help="firmware stream count, default 32")
    mux_stream_parser.add_argument("--delay-ms", type=int, default=250, help="firmware stream delay in milliseconds, default 250")
    mux_stream_parser.add_argument("--log", type=Path, help="optional log file for the single stream response")

    adc_parser = subparsers.add_parser("meter-adc-snapshot", help="read DMM voltage waveform sampler state")
    add_common_serial_args(adc_parser)
    adc_parser.add_argument("--log", type=Path, help="optional log file for the single ADC snapshot response")

    screen_parser = subparsers.add_parser("screen-capture", help="save current read-only LCD shadow as a BMP file")
    add_common_serial_args(screen_parser)
    screen_parser.add_argument("--output", type=Path, default=Path("tmp/screen.bmp"), help="output BMP path, default tmp/screen.bmp")
    screen_parser.add_argument("--region", nargs=4, type=int, metavar=("X", "Y", "W", "H"), help="optional capture rectangle")
    screen_parser.add_argument("--no-rle", action="store_false", dest="use_rle", help="deprecated no-op: binary indexed4 is the default")
    screen_parser.add_argument(
        "--rle-shadow",
        action="store_true",
        dest="use_rle",
        help="deprecated debug path: page-stitches indexed shadow and switches to ACV meter screen",
    )
    screen_parser.set_defaults(use_rle=False)

    return parser


def add_poll_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--interval", type=float, default=0.5, help="delay between polls in seconds, default 0.5")
    parser.add_argument("--count", type=int, help="stop after this many polls")
    parser.add_argument("--duration", type=float, help="stop after this many seconds")
    parser.add_argument("--log", type=Path, help="optional log file for timestamped responses")


def open_log(path: Path | None) -> TextIO | None:
    if path is None:
        return None
    path.parent.mkdir(parents=True, exist_ok=True)
    return path.open("a", encoding="utf-8")


def log_context(path: Path | None):
    if path is None:
        return nullcontext(None)
    return open_log(path)


def print_ports(ports: Iterable[str], plain: bool) -> int:
    ports = list(ports)
    if plain:
        for port in ports:
            print(port)
        return 0 if ports else 1
    if not ports:
        print("No candidate OpenScope USB CDC serial ports found.")
        return 1
    print("Candidate OpenScope USB CDC serial ports:")
    for port in ports:
        print(f"  {port}")
    return 0


def validate_stream_args(count: int, delay_ms: int) -> None:
    if not 0 <= count <= 200:
        raise ValueError("stream count must be between 0 and 200")
    if not 0 <= delay_ms <= 5000:
        raise ValueError("stream delay must be between 0 and 5000 ms")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if args.mode == "list":
        return print_ports(discover_ports(), args.plain)

    try:
        if args.mode in ("meter-stream", "meter-mux-stream"):
            validate_stream_args(args.count, args.delay_ms)
        port = choose_port(args.port)
        with serial_device_lock(port):
            if args.mode == "command":
                print(run_command(port, args.baud, args.command, args.timeout))
                return 0

            if args.mode == "poll":
                with log_context(args.log) as log_file:
                    return poll_command(
                        port,
                        args.baud,
                        args.command,
                        args.timeout,
                        args.interval,
                        args.count,
                        args.duration,
                        log_file,
                    )

            if args.mode == "meter-dump":
                with log_context(args.log) as log_file:
                    return poll_command(
                        port,
                        args.baud,
                        args.command,
                        args.timeout,
                        args.interval,
                        args.count,
                        args.duration,
                        log_file,
                    )

            if args.mode == "meter-frontend":
                response = run_command(port, args.baud, "meter frontend", args.timeout)
                print(response)
                with log_context(args.log) as log_file:
                    write_log_line(log_file, response)
                return 0

            if args.mode == "meter-stream":
                command = f"meter stream {args.count} {args.delay_ms}"
                timeout = max(args.timeout, (args.count * args.delay_ms / 1000.0) + 2.0)
                response = run_command(port, args.baud, command, timeout)
                print(response)
                with log_context(args.log) as log_file:
                    write_log_line(log_file, response)
                return 0

            if args.mode == "meter-mux-stream":
                command = f"meter mux-stream {args.count} {args.delay_ms}"
                timeout = max(args.timeout, (args.count * args.delay_ms / 1000.0) + 2.0)
                response = run_command(port, args.baud, command, timeout)
                print(response)
                with log_context(args.log) as log_file:
                    write_log_line(log_file, response)
                return 0

            if args.mode == "meter-adc-snapshot":
                response = run_command(port, args.baud, "meter adc-snapshot", args.timeout)
                print(response)
                with log_context(args.log) as log_file:
                    write_log_line(log_file, response)
                return 0

            if args.mode == "screen-capture":
                region = tuple(args.region) if args.region is not None else None
                timeout = max(args.timeout, 4.0)
                print(capture_screen(port, args.baud, timeout, args.output, region, args.use_rle))
                return 0

    except TimeoutError as exc:
        print(f"timeout: {exc}", file=sys.stderr)
        return 3
    except ValueError as exc:
        print(f"parse error: {exc}", file=sys.stderr)
        return 4
    except SerialError as exc:
        print(f"serial error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        return 130

    print(f"unsupported mode: {args.mode}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
