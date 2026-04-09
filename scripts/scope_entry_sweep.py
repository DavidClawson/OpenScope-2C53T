#!/usr/bin/env python3
"""
Scope entry bench sweeper for the OpenScope 2C53T CDC shell.

This runs a consistent scope wake -> entry block -> heartbeat -> acquisition
sequence against a set of candidate 8-byte scope entry banks and prints a
short summary for each one.
"""

from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

import usb_debug


@dataclass(frozen=True)
class Candidate:
    name: str
    entry: tuple[int, int, int, int, int, int, int, int] | None
    commands: tuple[str, ...] | None
    note: str


DEFAULT_CANDIDATES = {
    "legacy": Candidate(
        "legacy",
        (0x03, 0x01, 0x01, 0x03, 0x80, 0x04, 0x02, 0x01),
        None,
        "Current firmware-style scope entry bank.",
    ),
    "ch1_dc": Candidate(
        "ch1_dc",
        (0x01, 0x00, 0x00, 0x03, 0x80, 0x04, 0x02, 0x01),
        None,
        "CH1 selector with DC/full-bandwidth flags cleared.",
    ),
    "ch2_dc": Candidate(
        "ch2_dc",
        (0x10, 0x00, 0x00, 0x03, 0x80, 0x04, 0x02, 0x01),
        None,
        "CH2-style selector with DC/full-bandwidth flags cleared.",
    ),
    "ch1_rng": Candidate(
        "ch1_rng",
        (0x01, 0x00, 0x00, 0x00, 0x02, 0x04, 0x02, 0x01),
        None,
        "CH1 selector with a small-nibble range hypothesis for 2V/div.",
    ),
    "ch2_rng": Candidate(
        "ch2_rng",
        (0x10, 0x00, 0x00, 0x01, 0x01, 0x04, 0x02, 0x01),
        None,
        "CH2 selector with a small-nibble range hypothesis for 200mV/div.",
    ),
    "stock_raw_min": Candidate(
        "stock_raw_min",
        None,
        (
            "fpga frame 0x02 0xA0",
            "fpga frame 0x05 0x01",
            "fpga frame 0x05 0x03",
        ),
        "Recovered raw-word minimum: mode select + scope entry + heartbeat.",
    ),
    "stock_raw_ch1": Candidate(
        "stock_raw_ch1",
        None,
        (
            "fpga frame 0x02 0xA0",
            "fpga frame 0x05 0x01",
            "fpga frame 0x05 0x0C",
            "fpga frame 0x05 0x0E",
            "fpga frame 0x05 0x11",
            "fpga frame 0x05 0x10",
            "fpga frame 0x05 0x03",
        ),
        "Recovered CH1-flavored raw bank from the stock queue builders.",
    ),
    "stock_raw_ch2": Candidate(
        "stock_raw_ch2",
        None,
        (
            "fpga frame 0x02 0xA0",
            "fpga frame 0x05 0x01",
            "fpga frame 0x05 0x0D",
            "fpga frame 0x05 0x17",
            "fpga frame 0x05 0x16",
            "fpga frame 0x05 0x15",
            "fpga frame 0x05 0x03",
        ),
        "Recovered CH2-flavored raw bank from the stock queue builders.",
    ),
    "stock_raw_aux": Candidate(
        "stock_raw_aux",
        None,
        (
            "fpga frame 0x05 0x08",
            "fpga frame 0x05 0x09",
            "fpga frame 0x05 0x14",
        ),
        "Other stock-derived raw words observed in queue builders.",
    ),
}


def parse_entry(text: str) -> tuple[int, ...]:
    pieces = [p.strip() for p in text.replace(":", ",").split(",") if p.strip()]
    if len(pieces) != 8:
        raise argparse.ArgumentTypeError("entry must contain exactly 8 comma-separated bytes")
    out = []
    for piece in pieces:
        out.append(int(piece, 16))
    return tuple(out)


def hex_bank(entry: tuple[int, ...]) -> str:
    return " ".join(f"{b:02X}" for b in entry)


def extract_line(block: str, prefix: str) -> str:
    for line in block.splitlines():
        if line.strip().startswith(prefix):
            return line.strip()
    return ""


def send(ser, cmd: str, timeout: float = 3.0) -> str:
    print(f"\n> {cmd}")
    out = usb_debug.send_command(ser, cmd, timeout=timeout)
    print(out)
    return out


def run_candidate(ser, cand: Candidate, beats: int, beat_ms: int) -> None:
    print("\n" + "=" * 72)
    print(f"{cand.name}: {cand.note}")
    if cand.entry is not None:
        print(f"entry: {hex_bank(cand.entry)}")
    if cand.commands is not None:
        print("commands:")
        for cmd in cand.commands:
            print(f"  {cmd}")

    send(ser, "fpga scope wake", timeout=4.0)
    send(ser, "fpga diag clear")
    if cand.entry is not None:
        send(ser, f"fpga scope entry {hex_bank(cand.entry)}")
    if cand.commands is not None:
        for cmd in cand.commands:
            send(ser, cmd)
    send(ser, "fpga scope acqmode")
    send(ser, f"fpga scope beat {beats} {beat_ms}", timeout=max(4.0, beats * beat_ms / 1000.0 + 2.0))
    status = send(ser, "status", timeout=4.0)
    acq = send(ser, "fpga acq", timeout=4.0)
    spi = send(ser, "spi3 read 16", timeout=4.0)

    print("\nSummary:")
    for key in (
        "TX count:",
        "RX bytes:",
        "Data frames:",
        "Echo frames:",
        "SPI3 OK:",
        "SPI3 first byte:",
        "HS:",
    ):
        line = extract_line(status, key)
        if line:
            print(f"  {line}")

    acq_line = acq.splitlines()[1].strip() if len(acq.splitlines()) > 1 else acq.strip()
    if acq_line:
        print(f"  {acq_line}")

    spi_line = ""
    for line in spi.splitlines():
        if ":" in line and line[:4].isdigit():
            spi_line = line.strip()
            break
    if spi_line:
        print(f"  {spi_line}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run repeatable scope-entry bench candidates.")
    parser.add_argument(
        "--candidate",
        action="append",
        default=[],
        choices=sorted(DEFAULT_CANDIDATES.keys()),
        help="Named built-in candidate to run (repeatable). Defaults to all built-ins.",
    )
    parser.add_argument(
        "--entry",
        action="append",
        type=parse_entry,
        default=[],
        help="Custom 8-byte entry bank, e.g. 01,00,00,03,80,04,02,01",
    )
    parser.add_argument("--beats", type=int, default=10, help="Heartbeat count (default: 10)")
    parser.add_argument("--beat-ms", type=int, default=50, help="Heartbeat delay in ms (default: 50)")
    args = parser.parse_args()

    candidates: list[Candidate] = []
    if args.candidate:
        candidates.extend(DEFAULT_CANDIDATES[name] for name in args.candidate)
    else:
        candidates.extend(DEFAULT_CANDIDATES.values())

    for idx, entry in enumerate(args.entry, start=1):
        candidates.append(Candidate(f"custom_{idx}", entry, None, "User-specified entry bank."))

    port = usb_debug.find_device()
    print(f"Connecting to {port}...")
    if usb_debug.serial is None:
        print("pyserial not installed; using stdlib serial fallback.")

    ser = usb_debug.open_serial(port)
    try:
        time.sleep(1.0)
        if hasattr(ser, "read_available"):
            banner_bytes = ser.read_available()
        elif ser.in_waiting:
            banner_bytes = ser.read(ser.in_waiting)
        else:
            banner_bytes = b""

        if banner_bytes:
            banner = banner_bytes.decode("utf-8", errors="replace")
            for line in banner.strip().split("\r\n"):
                if line.strip():
                    print(line.rstrip())

        for cand in candidates:
            run_candidate(ser, cand, args.beats, args.beat_ms)
    finally:
        ser.close()


if __name__ == "__main__":
    main()
