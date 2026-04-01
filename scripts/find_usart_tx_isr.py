#!/usr/bin/env python3
"""Find the USART TX interrupt handler by searching for writes to USART1_DATA.

The FPGA command USART is at 0x40004400 (AT32 USART2).
USART_DATA register is at offset 0x04 = 0x40004404.

The TX ISR writes bytes to this register. Find all code that writes to it.
Also look for reads from the RAM buffer at 0x20000005-0x2000000E.
"""

from capstone import *
import struct

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

with open(FIRMWARE, "rb") as f:
    fw = f.read()

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True
md.skipdata = True

# Search for MOVW/MOVT pairs that construct 0x40004404 (USART_DATA)
# or 0x40004400 (USART_STS)
print("=== References to USART at 0x40004400 (FPGA command USART) ===")

movw_vals = {}
usart_refs = []

for insn in md.disasm(fw, BASE_ADDR):
    if insn.mnemonic == "movw":
        parts = insn.op_str.split(", #")
        if len(parts) == 2:
            try:
                movw_vals[parts[0].strip()] = (int(parts[1], 0), insn.address)
            except ValueError:
                pass
    elif insn.mnemonic == "movt":
        parts = insn.op_str.split(", #")
        if len(parts) == 2:
            reg = parts[0].strip()
            try:
                high = int(parts[1], 0)
                if reg in movw_vals:
                    low, mw_addr = movw_vals[reg]
                    full = (high << 16) | low
                    if 0x40004400 <= full <= 0x40004418:
                        name = {0x40004400: "USART_STS", 0x40004404: "USART_DATA",
                                0x40004408: "USART_BRR", 0x4000440C: "USART_CTL0",
                                0x40004410: "USART_CTL1", 0x40004414: "USART_CTL2"}.get(full, f"USART+{full-0x40004400:#x}")
                        usart_refs.append((mw_addr, insn.address, reg, full, name))
            except ValueError:
                pass

for mw, mt, reg, val, name in usart_refs:
    print(f"  {name}: MOVW at {mw:#010x}, MOVT at {mt:#010x} -> {reg}")

# Now let's look at the RAM buffer at 0x20000005
# The FPGA task stores: [0x20000007]=high, [0x20000008]=low, [0x2000000E]=cksum
# Find who READS from 0x20000005 range
print()
print("=== References to 0x20000005 (USART TX buffer base) ===")

movw_vals = {}
for insn in md.disasm(fw, BASE_ADDR):
    if insn.mnemonic == "movw":
        parts = insn.op_str.split(", #")
        if len(parts) == 2:
            try:
                movw_vals[parts[0].strip()] = (int(parts[1], 0), insn.address)
            except ValueError:
                pass
    elif insn.mnemonic == "movt":
        parts = insn.op_str.split(", #")
        if len(parts) == 2:
            reg = parts[0].strip()
            try:
                high = int(parts[1], 0)
                if reg in movw_vals:
                    low, mw_addr = movw_vals[reg]
                    full = (high << 16) | low
                    if full == 0x20000005:
                        print(f"  0x20000005: MOVW at {mw_addr:#010x}, MOVT at {insn.address:#010x} -> {reg}")
            except ValueError:
                pass

# Also check the vector table more carefully
print()
print("=== Vector table (first 80 entries) ===")
print("  Looking for USART-related handlers...")
for i in range(80):
    off = i * 4
    if off + 4 > len(fw):
        break
    vec = struct.unpack_from("<I", fw, off)[0]
    if vec != 0 and vec != 0xFFFFFFFF and i >= 16:
        irq = i - 16
        # Only show non-default handlers
        if vec != 0x08007345:
            print(f"  Exception {i} (IRQ {irq}): {vec:#010x}")

# Let's also disassemble around 0x08007344 (the default handler that many IRQs point to)
print()
print("=== Default IRQ handler at 0x08007344 ===")
offset = 0x08007344 - BASE_ADDR
code = fw[offset:offset + 0x100]
for insn in md.disasm(code, 0x08007344):
    print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}")
    if insn.mnemonic in ("pop", "bx") and ("pc" in insn.op_str or "lr" in insn.op_str):
        break
    if insn.address > 0x080073C0:
        break
