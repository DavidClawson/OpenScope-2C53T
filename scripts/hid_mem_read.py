#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from hid_flash import open_bootloader_device, read_memory


def format_words(data: bytes, base: int) -> str:
    lines: list[str] = []
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        words = []
        for word_offset in range(0, len(chunk), 4):
            word = chunk[word_offset:word_offset + 4]
            if len(word) == 4:
                words.append(f"0x{struct.unpack('<I', word)[0]:08X}")
            else:
                words.append(word.hex())
        lines.append(f"0x{base + offset:08X}: " + " ".join(words))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Read flash/SRAM through the high-recovery HID diagnostic command.")
    parser.add_argument("address", type=lambda text: int(text, 0))
    parser.add_argument("size", type=lambda text: int(text, 0))
    parser.add_argument("--out", type=Path, help="write raw bytes to this file")
    args = parser.parse_args()

    dev = open_bootloader_device()
    try:
        data = read_memory(dev, args.address, args.size)
    finally:
        dev.close()

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_bytes(data)
        print(f"wrote {len(data)} bytes to {args.out}")
    print(format_words(data, args.address))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
