#!/usr/bin/env python3
"""Find the full init function that contains the SPI3 setup at 0x08026500.
Trace ALL peripheral accesses to find FPGA enable/reset pins."""

from capstone import *

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH_MAP = {
    0x40003C00: "SPI3_CTL0", 0x40003C04: "SPI3_CTL1", 0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA",
    0x40003800: "SPI2_CTL0", 0x40003808: "SPI2_STAT", 0x4000380C: "SPI2_DATA",
    0x40004400: "USART1_STS", 0x40004404: "USART1_DATA", 0x40004408: "USART1_BRR",
    0x4000440C: "USART1_CTL0",
    0x40004800: "USART2_STS", 0x40004804: "USART2_DATA", 0x4000480C: "USART2_CTL0",
    0x40010800: "GPIOA_CTL0", 0x40010804: "GPIOA_CTL1", 0x40010808: "GPIOA_ISTAT",
    0x4001080C: "GPIOA_OCTL", 0x40010810: "GPIOA_BOP", 0x40010814: "GPIOA_BC",
    0x40010C00: "GPIOB_CTL0", 0x40010C04: "GPIOB_CTL1", 0x40010C08: "GPIOB_ISTAT",
    0x40010C0C: "GPIOB_OCTL", 0x40010C10: "GPIOB_BOP", 0x40010C14: "GPIOB_BC",
    0x40011000: "GPIOC_CTL0", 0x40011004: "GPIOC_CTL1", 0x40011008: "GPIOC_ISTAT",
    0x4001100C: "GPIOC_OCTL", 0x40011010: "GPIOC_BOP", 0x40011014: "GPIOC_BC",
    0x40011400: "GPIOD_CTL0", 0x40011408: "GPIOD_ISTAT", 0x40011410: "GPIOD_BOP",
    0x40012400: "ADC0_STS",  # Actually ADC1 on AT32
    0x40012800: "ADC1_STS",  # ADC2
    0x40013800: "USART0_STS",
    0x40015400: "TMR9_BASE",  # Or TMR1_4?
    0x40021000: "RCU_CTL", 0x40021004: "RCU_CFG0",
    0x40021014: "RCU_AHBEN", 0x40021018: "RCU_APB2EN", 0x4002101C: "RCU_APB1EN",
    0x40010004: "AFIO_PCF0",
    0x40020000: "DMA0_INTF",
    0xE000E010: "SYSTICK_CTL", 0xE000E014: "SYSTICK_LOAD", 0xE000E018: "SYSTICK_VAL",
    0xE000ED04: "SCB_ICSR",
}

with open(FIRMWARE, "rb") as f:
    fw = f.read()

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True
md.skipdata = True

# Find function start — scan backwards from 0x08026480 for a push
start_scan = 0x08025800
offset = start_scan - BASE_ADDR
code = fw[offset:offset + 0x1000]
last_push = None
for insn in md.disasm(code, start_scan):
    if insn.address >= 0x08026490:
        break
    if insn.mnemonic == "push" and "lr" in insn.op_str:
        last_push = insn.address

if last_push:
    print(f"Function likely starts at: {last_push:#010x}")
else:
    print("No push found, scanning from 0x08025800")
    last_push = 0x08025800

# Now disassemble from function start, but only print lines with peripheral accesses
start = last_push if last_push else 0x08026400
offset = start - BASE_ADDR
code = fw[offset:offset + 0x2000]  # 8KB should be enough

movw_vals = {}
print(f"\n=== Peripheral accesses in init function (from {start:#010x}) ===\n")

for insn in md.disasm(code, start):
    if insn.address > start + 0x1F00:
        break

    annotation = ""
    is_interesting = False

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
                    name = PERIPH_MAP.get(full)
                    if name:
                        annotation = f"  ; {reg} = {name}"
                        is_interesting = True
                    elif 0x40000000 <= full < 0x50000000:
                        annotation = f"  ; {reg} = {full:#010x} (PERIPH)"
                        is_interesting = True
                    elif 0x20000000 <= full < 0x20040000:
                        annotation = f"  ; {reg} = {full:#010x} (RAM)"
                    elif 0x08000000 <= full < 0x080c0000:
                        annotation = f"  ; {reg} = {full:#010x} (FLASH)"
            except ValueError:
                pass
    elif insn.mnemonic == "bl":
        try:
            target = int(insn.op_str.lstrip("#"), 0)
            annotation = f"  ; -> FUN_{target:08x}"
            is_interesting = True
        except ValueError:
            pass

    # Also flag GPIO BOP/BC writes (pin set/clear)
    if insn.mnemonic == "str" and any(x in insn.op_str for x in ["r6, [r", "sb, [r", "#0x10", "#0x14"]):
        is_interesting = True

    if is_interesting or annotation:
        print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{annotation}")

    # Stop at function return after SPI3 area
    if insn.address > 0x08026800 and insn.mnemonic == "pop" and "pc" in insn.op_str:
        print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}  ; FUNCTION END")
        break
