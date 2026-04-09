#!/usr/bin/env python3
"""Find absolute flash references in a raw firmware image.

This is meant for cases where stock code builds flash addresses with
`movw`/`movt` pairs and some of those targets may point beyond the end of
the vendor-supplied binary.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


DEFAULT_FIRMWARE = Path(
    "/Users/david/Desktop/osc/archive/2C53T Firmware V1.2.0/"
    "APP_2C53T_V1.2.0_251015.bin"
)
DEFAULT_BASE = 0x08000000
FLASH_START = 0x08000000
FLASH_END = 0x08100000

OBJDUMP_RE = re.compile(
    r"^\s*([0-9a-fA-F]+):\s+[0-9a-fA-F ]+\s+([a-z0-9.]+)\s+(.*)$"
)
MOV_IMM_RE = re.compile(
    r"^(r\d+|ip|lr),\s*#([0-9a-fA-Fx]+)(?:\s*[;@].*)?$",
    re.IGNORECASE,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "firmware",
        nargs="?",
        type=Path,
        default=DEFAULT_FIRMWARE,
        help="Raw firmware image to scan",
    )
    parser.add_argument(
        "--base",
        type=lambda s: int(s, 0),
        default=DEFAULT_BASE,
        help="Load address used when disassembling the raw image",
    )
    parser.add_argument(
        "--objdump",
        default="arm-none-eabi-objdump",
        help="Objdump binary to use",
    )
    parser.add_argument(
        "--window",
        type=lambda s: int(s, 0),
        default=0x20,
        help="Maximum distance between movw and movt instructions",
    )
    parser.add_argument(
        "--show-all",
        action="store_true",
        help="Print in-file flash references as well as missing ones",
    )
    return parser.parse_args()


def parse_imm(text: str) -> int:
    return int(text, 0)


def run_objdump(objdump: str, firmware: Path, base: int) -> str:
    cmd = [
        objdump,
        "-D",
        "-b",
        "binary",
        "-marm",
        "-Mforce-thumb",
        f"--adjust-vma={base:#x}",
        str(firmware),
    ]
    return subprocess.check_output(cmd, text=True)


def scan_refs(disasm: str, window: int) -> list[dict[str, int | str | bool]]:
    movw_state: dict[str, tuple[int, int]] = {}
    refs: list[dict[str, int | str | bool]] = []

    for line in disasm.splitlines():
        match = OBJDUMP_RE.match(line)
        if not match:
            continue

        insn_addr = int(match.group(1), 16)
        mnemonic = match.group(2).lower()
        operands = match.group(3).strip()

        imm_match = MOV_IMM_RE.match(operands)
        if mnemonic == "movw" and imm_match:
            reg = imm_match.group(1).lower()
            imm = parse_imm(imm_match.group(2))
            movw_state[reg] = (insn_addr, imm)
            continue

        if mnemonic != "movt" or not imm_match:
            continue

        reg = imm_match.group(1).lower()
        high = parse_imm(imm_match.group(2))
        if reg not in movw_state:
            continue

        movw_addr, low = movw_state[reg]
        if insn_addr - movw_addr > window:
            continue

        full = (high << 16) | low
        if not (FLASH_START <= full < FLASH_END):
            continue

        refs.append(
            {
                "movw_addr": movw_addr,
                "movt_addr": insn_addr,
                "reg": reg,
                "target": full,
            }
        )

    return refs


def main() -> int:
    args = parse_args()
    firmware = args.firmware.expanduser().resolve()
    if not firmware.is_file():
        print(f"ERROR: firmware not found: {firmware}", file=sys.stderr)
        return 1

    file_end = args.base + firmware.stat().st_size
    try:
        disasm = run_objdump(args.objdump, firmware, args.base)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: objdump failed: {exc}", file=sys.stderr)
        return 1

    refs = scan_refs(disasm, args.window)
    grouped: dict[int, list[dict[str, int | str | bool]]] = defaultdict(list)
    for ref in refs:
        grouped[int(ref["target"])].append(ref)

    missing_targets = [target for target in sorted(grouped) if target >= file_end]
    in_file_targets = [target for target in sorted(grouped) if target < file_end]

    print(f"firmware: {firmware}")
    print(f"base:     {args.base:#010x}")
    print(f"size:     0x{firmware.stat().st_size:x}")
    print(f"file_end: {file_end:#010x}")
    print(f"flash refs: {len(refs)} pairs, {len(grouped)} unique targets")
    print(f"missing refs: {len(missing_targets)} unique targets beyond file end")

    def dump_targets(title: str, targets: list[int]) -> None:
        if not targets:
            return
        print()
        print(title)
        for target in targets:
            ref_list = grouped[target]
            sites = ", ".join(
                f"{int(ref['movw_addr']):#010x}/{int(ref['movt_addr']):#010x}"
                for ref in ref_list
            )
            print(f"  {target:#010x}  refs={len(ref_list)}  sites={sites}")

    dump_targets("Missing high-flash targets:", missing_targets)
    if args.show_all:
        dump_targets("In-file flash targets:", in_file_targets)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
