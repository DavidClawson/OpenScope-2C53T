#!/usr/bin/env python3
"""Disassemble FUN_08036848 (SPI init function) and surrounding context."""

from capstone import *

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH_MAP = {
    0x40003C00: "SPI3_CTL0", 0x40003C04: "SPI3_CTL1", 0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA", 0x40003800: "SPI2_CTL0",
    0x40010C00: "GPIOB_CTL0", 0x40010C10: "GPIOB_BOP", 0x40010C14: "GPIOB_BC",
}

def disasm_func(fw, start, max_size=0x200, name=""):
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    offset = start - BASE_ADDR
    code = fw[offset:offset + max_size]

    movw_vals = {}

    print(f"=== {name} (0x{start:08x}) ===")

    found_return = False
    for insn in md.disasm(code, start):
        annotation = ""

        if insn.mnemonic == "movw":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                try:
                    movw_vals[parts[0].strip()] = int(parts[1], 0)
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
                        name_p = PERIPH_MAP.get(full)
                        if name_p:
                            annotation = f"  ; {reg} = {name_p}"
                        elif 0x40000000 <= full < 0x50000000:
                            annotation = f"  ; {reg} = {full:#010x}"
                except ValueError:
                    pass
        elif insn.mnemonic == "bl":
            try:
                target = int(insn.op_str.lstrip("#"), 0)
                annotation = f"  ; -> FUN_{target:08x}"
            except ValueError:
                pass

        print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{annotation}")

        if insn.mnemonic == "pop" and "pc" in insn.op_str:
            found_return = True
        if insn.mnemonic == "bx" and "lr" in insn.op_str:
            found_return = True
        if found_return and insn.mnemonic in ("push", ".byte"):
            break
    print()

with open(FIRMWARE, "rb") as f:
    fw = f.read()

# SPI init function
disasm_func(fw, 0x08036848, 0x100, "FUN_08036848 (SPI init)")

# Also look at FUN_0803a390 (called with arg 0xa = delay 10?)
disasm_func(fw, 0x0803a390, 0x100, "FUN_0803a390 (delay?)")

# Also look at the very start of the init function containing SPI3 setup
# Need to find the push that starts the function containing 0x08026480
# Search backwards for a push instruction
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True
md.skipdata = True

# Try scanning from 0x08026000 to find function starts
print("=== Searching for function start before 0x08026480 ===")
start = 0x08025E00
offset = start - BASE_ADDR
code = fw[offset:offset + 0x800]
last_push = None
for insn in md.disasm(code, start):
    if insn.address >= 0x08026500:
        break
    if insn.mnemonic == "push" and ("lr" in insn.op_str or "pc" in insn.op_str):
        last_push = insn.address
print(f"  Last push before 0x08026500: {last_push:#010x}")
print()

# Disassemble from the function start to the SPI3 init
if last_push:
    disasm_func(fw, last_push, 0xE00, f"Init function containing SPI3 setup (starts at {last_push:#010x})")
