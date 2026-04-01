#!/usr/bin/env python3
"""Search for SPI3 base address via alternative patterns.

SPI3_CTL0 at 0x40003C00 might be loaded:
1. Via LDR from a literal pool
2. Via offset from SPI3_STAT (which IS found)
3. Via a function that takes the SPI base as a parameter

Also disassemble the second SPI3_STAT reference at 0x08026548.
"""

from capstone import *
import struct

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

def main():
    with open(FIRMWARE, "rb") as f:
        fw = f.read()

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    # 1. Search for 0x40003C00 as a 32-bit literal in the binary
    target_bytes = struct.pack("<I", 0x40003C00)
    print("=== Searching for 0x40003C00 as literal in binary ===")
    pos = 0
    while True:
        pos = fw.find(target_bytes, pos)
        if pos < 0:
            break
        addr = BASE_ADDR + pos
        print(f"  Found at {addr:#010x} (file offset {pos:#x})")
        pos += 1

    # Also search for SPI3 base variants
    for val, name in [(0x40003C00, "SPI3_BASE"), (0x40003C08, "SPI3_STAT"),
                       (0x40003C0C, "SPI3_DATA"), (0x40003C04, "SPI3_CTL1")]:
        target_bytes = struct.pack("<I", val)
        count = fw.count(target_bytes)
        if count:
            print(f"  {name} ({val:#010x}): {count} literal occurrences")

    print()

    # 2. Disassemble the area around 0x08026548 (second SPI3_STAT ref)
    print("=== Disassembly around 0x08026548 (second SPI3_STAT reference) ===")
    start = 0x08026480
    offset = start - BASE_ADDR
    code = fw[offset:offset + 0x400]

    movw_vals = {}
    periph_map = {
        0x40003C00: "SPI3_CTL0", 0x40003C04: "SPI3_CTL1", 0x40003C08: "SPI3_STAT",
        0x40003C0C: "SPI3_DATA", 0x40010C00: "GPIOB_CTL0", 0x40010C10: "GPIOB_BOP",
        0x40010C14: "GPIOB_BC", 0x4000440C: "USART1_CTL0", 0x40004400: "USART1_STS",
        0x40004404: "USART1_DATA",
        0x4002101C: "RCU_APB1EN", 0x40021018: "RCU_APB2EN",
        0x40010004: "AFIO_PCF0",
    }

    for insn in md.disasm(code, start):
        if insn.address > start + 0x380:
            break
        annotation = ""

        if insn.mnemonic == "movw":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    movw_vals[reg] = int(parts[1], 0)
                except ValueError:
                    pass
        elif insn.mnemonic == "movt":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    high = int(parts[1], 0)
                    if reg in movw_vals:
                        full = (high << 16) | movw_vals[reg]
                        name = periph_map.get(full)
                        if name:
                            annotation = f"  ; {reg} = {full:#010x} = {name}"
                        elif 0x40000000 <= full < 0x50000000:
                            annotation = f"  ; {reg} = {full:#010x} (PERIPH)"
                        elif 0x20000000 <= full < 0x20040000:
                            annotation = f"  ; {reg} = {full:#010x} (RAM)"
                except ValueError:
                    pass
        elif insn.mnemonic == "bl":
            try:
                target = int(insn.op_str.lstrip("#"), 0)
                annotation = f"  ; CALL FUN_{target:08x}"
            except ValueError:
                pass
        elif insn.mnemonic == "cmp":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                try:
                    val = int(parts[1], 0)
                    annotation = f"  ; == {val:#x}"
                except ValueError:
                    pass

        marker = " >>>" if insn.address in (0x08026548, 0x08026574) else "    "
        print(f"{marker} {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{annotation}")

    print()

    # 3. Search for the boot init function that configures SPI3
    # Look for writes near 0x08002BE0 (the boot init gap mentioned in CLAUDE.md)
    print("=== Disassembly of boot init area (0x08002B00 - 0x08002D00) ===")
    start = 0x08002B00
    offset = start - BASE_ADDR
    code = fw[offset:offset + 0x400]
    movw_vals = {}

    for insn in md.disasm(code, start):
        if insn.address > start + 0x3F0:
            break
        annotation = ""

        if insn.mnemonic == "movw":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    movw_vals[reg] = int(parts[1], 0)
                except ValueError:
                    pass
        elif insn.mnemonic == "movt":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    high = int(parts[1], 0)
                    if reg in movw_vals:
                        full = (high << 16) | movw_vals[reg]
                        name = periph_map.get(full)
                        if name:
                            annotation = f"  ; {reg} = {full:#010x} = {name}"
                        elif 0x40000000 <= full < 0x50000000:
                            annotation = f"  ; {reg} = {full:#010x} (PERIPH)"
                        elif 0x20000000 <= full < 0x20040000:
                            annotation = f"  ; {reg} = {full:#010x} (RAM)"
                        elif 0x08000000 <= full < 0x080c0000:
                            annotation = f"  ; {reg} = {full:#010x} (FLASH)"
                except ValueError:
                    pass
        elif insn.mnemonic == "bl":
            try:
                target = int(insn.op_str.lstrip("#"), 0)
                annotation = f"  ; CALL FUN_{target:08x}"
            except ValueError:
                pass

        print(f"    {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{annotation}")

if __name__ == "__main__":
    main()
