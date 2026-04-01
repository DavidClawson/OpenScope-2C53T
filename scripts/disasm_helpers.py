#!/usr/bin/env python3
"""Disassemble helper functions called by the FPGA task.

Focus on:
- FUN_080302fc (14 calls) - likely SPI3 transfer
- FUN_0802f16c (3 calls) - memory copy / DMA?
- FUN_08033ef8 (4 calls) - unknown
- FUN_08034078 (1 call) - unknown
- FUN_0803c8e0 (5 calls) - likely delay/wait
- FUN_080028e0 (1 call) - possible init
- FUN_08002c78 (1 call) - possible init
- FUN_0803e124 (5 calls) - FreeRTOS?
- FUN_0803ee0c (7 calls) - most called after 080302fc
"""

from capstone import *
import struct

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH_MAP = {
    0x40003C00: "SPI3_CTL0", 0x40003C04: "SPI3_CTL1", 0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA", 0x40003C10: "SPI3_CRCPOLY",
    0x40003800: "SPI2_CTL0", 0x40003804: "SPI2_CTL1", 0x40003808: "SPI2_STAT",
    0x4000380C: "SPI2_DATA",
    0x40004800: "USART2_STS", 0x40004804: "USART2_DATA", 0x40004808: "USART2_BRR",
    0x4000480C: "USART2_CTL0", 0x40004810: "USART2_CTL1", 0x40004814: "USART2_CTL2",
    0x40010800: "GPIOA_CTL0", 0x40010808: "GPIOA_ISTAT", 0x4001080C: "GPIOA_OCTL",
    0x40010810: "GPIOA_BOP", 0x40010814: "GPIOA_BC",
    0x40010C00: "GPIOB_CTL0", 0x40010C04: "GPIOB_CTL1", 0x40010C08: "GPIOB_ISTAT",
    0x40010C0C: "GPIOB_OCTL", 0x40010C10: "GPIOB_BOP", 0x40010C14: "GPIOB_BC",
    0x40011000: "GPIOC_CTL0", 0x40011004: "GPIOC_CTL1", 0x40011008: "GPIOC_ISTAT",
    0x4001100C: "GPIOC_OCTL", 0x40011010: "GPIOC_BOP", 0x40011014: "GPIOC_BC",
    0x40021014: "RCU_AHBEN", 0x40021018: "RCU_APB2EN", 0x4002101C: "RCU_APB1EN",
    0x40010004: "AFIO_PCF0",
    0x40020000: "DMA0_INTF", 0x40020004: "DMA0_INTC",
    0x40020008: "DMA0_CH0CTL", 0x4002000C: "DMA0_CH0CNT",
    0x40020010: "DMA0_CH0PADDR", 0x40020014: "DMA0_CH0MADDR",
    0x4002001C: "DMA0_CH1CTL", 0x40020020: "DMA0_CH1CNT",
    0x40020024: "DMA0_CH1PADDR", 0x40020028: "DMA0_CH1MADDR",
    0xE000ED04: "SCB_ICSR",
}

FUNC_MAP = {
    0x0803acf0: "xQueueGenericSend",
    0x0803b1d8: "xQueueReceive",
    0x0803a53c: "vTaskDelay",
    0x0803c8e0: "FUN_0803c8e0",
    0x080302fc: "FUN_080302fc",
    0x0802f16c: "FUN_0802f16c",
}

# Functions to disassemble: (start_addr, max_size, name)
TARGETS = [
    (0x080302fc, 0x400, "FUN_080302fc (14 calls from FPGA task)"),
    (0x0802f16c, 0x200, "FUN_0802f16c (3 calls - memory/DMA?)"),
    (0x08033ef8, 0x200, "FUN_08033ef8 (4 calls)"),
    (0x08034078, 0x200, "FUN_08034078 (1 call)"),
    (0x0803c8e0, 0x200, "FUN_0803c8e0 (5 calls - delay?)"),
    (0x080028e0, 0x200, "FUN_080028e0 (1 call - init?)"),
    (0x08002c78, 0x200, "FUN_08002c78 (1 call - init?)"),
    (0x0803ee0c, 0x100, "FUN_0803ee0c (7 calls)"),
    (0x0803e124, 0x100, "FUN_0803e124 (5 calls)"),
    (0x0803e5da, 0x100, "FUN_0803e5da (4 calls)"),
    (0x0803e77c, 0x100, "FUN_0803e77c (4 calls)"),
    (0x0803ed70, 0x100, "FUN_0803ed70 (4 calls)"),
    (0x08019e98, 0x200, "FUN_08019e98 (1 call)"),
    (0x080223bc, 0x200, "FUN_080223bc (1 call)"),
    (0x0800157c, 0x200, "FUN_0800157c (1 call)"),
]

def load_firmware():
    with open(FIRMWARE, "rb") as f:
        return f.read()

def disasm_one(fw, start, max_size, name):
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    offset = start - BASE_ADDR
    code = fw[offset:offset + max_size]

    movw_vals = {}
    lines = []
    found_return = False

    for insn in md.disasm(code, start):
        addr = insn.address
        mnemonic = insn.mnemonic
        op_str = insn.op_str
        annotation = ""

        if mnemonic == "movw":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    movw_vals[reg] = int(parts[1], 0)
                except ValueError:
                    pass

        elif mnemonic == "movt":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    high = int(parts[1], 0)
                    if reg in movw_vals:
                        full = (high << 16) | movw_vals[reg]
                        name_p = PERIPH_MAP.get(full)
                        if name_p:
                            annotation = f"  ; {reg} = {full:#010x} = {name_p}"
                        elif 0x40000000 <= full < 0x50000000:
                            annotation = f"  ; {reg} = {full:#010x} (PERIPH)"
                        elif 0x20000000 <= full < 0x20040000:
                            annotation = f"  ; {reg} = {full:#010x} (RAM)"
                        elif 0x08000000 <= full < 0x080c0000:
                            fn = FUNC_MAP.get(full)
                            annotation = f"  ; {reg} = {full:#010x}" + (f" = {fn}" if fn else " (FLASH)")
                        else:
                            annotation = f"  ; {reg} = {full:#010x}"
                except ValueError:
                    pass

        elif mnemonic == "bl":
            try:
                target = int(op_str.lstrip("#"), 0)
                fn = FUNC_MAP.get(target, f"FUN_{target:08x}")
                annotation = f"  ; -> {fn}"
            except ValueError:
                pass

        elif mnemonic == "cmp":
            parts = op_str.split(", #")
            if len(parts) == 2:
                try:
                    val = int(parts[1], 0)
                    annotation = f"  ; == {val:#x} ({val})"
                except ValueError:
                    pass

        line = f"  {addr:#010x}:  {mnemonic:<8s} {op_str}{annotation}"
        lines.append(line)

        # Stop at function return (but allow multiple returns for branches)
        if mnemonic == "pop" and "pc" in op_str:
            found_return = True
            # Continue a bit more to catch tail code
        elif mnemonic == "bx" and "lr" in op_str:
            found_return = True

        # Stop after first return + some slack
        if found_return and (addr - start) > 0x10 and mnemonic in ("push", ".byte"):
            break

    return lines

def main():
    fw = load_firmware()

    for start, max_size, name in TARGETS:
        print("=" * 70)
        print(f"  {name}")
        print(f"  Address: {start:#010x}")
        print("=" * 70)
        lines = disasm_one(fw, start, max_size, name)
        for line in lines:
            print(line)
        print()

if __name__ == "__main__":
    main()
