#!/usr/bin/env python3
"""Find what triggers SPI3 data reads in the FPGA task.

The SPI3 data receiver waits on queue at 0x20002D78.
Someone must send to this queue to trigger SPI3 reads.

Search the ENTIRE binary for xQueueGenericSend calls that target
the queue handle stored at 0x20002D78.

Also look at GPIOC_IDR reads in the FPGA task — might be a "data ready" pin.
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

# 1. Find all places that load 0x20002D78 (SPI3 queue handle address)
print("=" * 70)
print("  Searching for references to 0x20002D78 (SPI3 queue)")
print("=" * 70)

movw_vals = {}
refs_2d78 = []

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
                    if full == 0x20002D78:
                        refs_2d78.append((mw_addr, insn.address, reg))
            except ValueError:
                pass

for mw, mt, reg in refs_2d78:
    print(f"  MOVW at {mw:#010x}, MOVT at {mt:#010x} -> {reg}")

# 2. For each reference, disassemble surrounding context to see if it's
#    used with xQueueGenericSend
print()
print("=" * 70)
print("  Context around each 0x20002D78 reference")
print("=" * 70)

for mw, mt, reg in refs_2d78:
    start = mw - 32
    offset = start - BASE_ADDR
    code = fw[offset:offset + 256]

    print(f"\n--- Around {mw:#010x} ---")
    local_movw = {}
    for insn in md.disasm(code, start):
        if insn.address > mt + 96:
            break
        annotation = ""

        if insn.mnemonic == "movw":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                try:
                    local_movw[parts[0].strip()] = int(parts[1], 0)
                except ValueError:
                    pass
        elif insn.mnemonic == "movt":
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                r = parts[0].strip()
                try:
                    h = int(parts[1], 0)
                    if r in local_movw:
                        f = (h << 16) | local_movw[r]
                        if f == 0x20002D78:
                            annotation = "  ; *** SPI3 QUEUE ***"
                        elif f == 0x20002D74:
                            annotation = "  ; USART TX QUEUE"
                        elif f == 0x20002D6C:
                            annotation = "  ; QUEUE 0x20002D6C"
                        elif 0x20000000 <= f < 0x20040000:
                            annotation = f"  ; {f:#010x} (RAM)"
                        elif 0x08000000 <= f < 0x080c0000:
                            annotation = f"  ; {f:#010x} (FLASH)"
                except ValueError:
                    pass
        elif insn.mnemonic == "bl":
            try:
                target = int(insn.op_str.lstrip("#"), 0)
                names = {
                    0x0803acf0: "xQueueGenericSend",
                    0x0803b09c: "xQueueGenericSendFromISR",
                    0x0803b1d8: "xQueueReceive",
                    0x0803a53c: "vTaskDelay",
                    0x0803a390: "vTaskDelay2",
                }
                annotation = f"  ; -> {names.get(target, f'FUN_{target:08x}')}"
            except ValueError:
                pass

        marker = " >>>" if insn.address in (mw, mt) else "    "
        print(f"{marker} {insn.address:#010x}: {insn.mnemonic:<10s} {insn.op_str}{annotation}")

# 3. Search for GPIOC_IDR (0x40011008) in the FPGA task region
print()
print("=" * 70)
print("  GPIOC_IDR references in FPGA task (0x08036934-0x08039800)")
print("=" * 70)

movw_vals = {}
for insn in md.disasm(fw[0x36934:0x39800], BASE_ADDR + 0x36934):
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
                    if full == 0x40011008:
                        print(f"  GPIOC_IDR: MOVW at {mw_addr:#010x}, MOVT at {insn.address:#010x} -> {reg}")
            except ValueError:
                pass

# 4. Also find who sends to the USART RX queue — the USART RX ISR
# would receive bytes and put them in a queue, which the FPGA task
# processes. Let's find all xQueueGenericSendFromISR calls
print()
print("=" * 70)
print("  All xQueueGenericSendFromISR calls (ISR -> queue)")
print("=" * 70)

offset = 0
code = fw
for insn in md.disasm(code, BASE_ADDR):
    if insn.mnemonic == "bl":
        try:
            target = int(insn.op_str.lstrip("#"), 0)
            if target == 0x0803b09c:
                print(f"  {insn.address:#010x}: bl xQueueGenericSendFromISR")
        except ValueError:
            pass
