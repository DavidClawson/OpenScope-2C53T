#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def c_array(name: str, data: bytes) -> str:
    items = ", ".join(f"0x{b:02X}" for b in data)
    return f"const uint8_t {name}[] = {{ {items} }};\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate C payload for the high-recovery layout installer")
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--high-bootloader", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    stage0 = args.stage0.read_bytes()
    high = args.high_bootloader.read_bytes()
    if len(stage0) > 2 * 1024:
        raise SystemExit(f"stage0 payload is {len(stage0)} bytes, exceeds 2KB")
    if len(high) > 64 * 1024:
        raise SystemExit(f"high bootloader payload is {len(high)} bytes, exceeds 64KB")

    args.output.write_text(
        '#include <stdint.h>\n'
        '__attribute__((used, section(".rodata.high_layout_payload")))\n'
        'const char high_layout_updater_marker[] = "OpenScope high recovery layout updater";\n'
        '__attribute__((used, section(".rodata.high_layout_payload")))\n'
        + c_array("stage0_payload", stage0)
        + f"const uint32_t stage0_payload_size = {len(stage0)}u;\n"
        '__attribute__((used, section(".rodata.high_layout_payload")))\n'
        + c_array("high_bootloader_payload", high)
        + f"const uint32_t high_bootloader_payload_size = {len(high)}u;\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
