#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate C payload for the bootloader updater app")
    parser.add_argument("bootloader", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    payload = args.bootloader.read_bytes()
    if len(payload) > 16 * 1024:
        raise SystemExit(f"bootloader payload is {len(payload)} bytes, exceeds 16KB")

    items = ", ".join(f"0x{b:02X}" for b in payload)
    args.output.write_text(
        '#include <stdint.h>\n'
        '__attribute__((used, section(".rodata.bootloader_payload")))\n'
        'const char bootloader_updater_marker[] = "OpenScope bootloader updater";\n'
        '__attribute__((used, section(".rodata.bootloader_payload")))\n'
        f'const uint8_t bootloader_payload[] = {{ {items} }};\n'
        f'const uint32_t bootloader_payload_size = {len(payload)}u;\n',
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
