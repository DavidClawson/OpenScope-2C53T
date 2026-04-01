#!/usr/bin/env python3
"""Disassemble FUN_08036934 (the real FPGA task) from V1.2.0 firmware.

Extracts SPI3 configuration, USART2 protocol, and FPGA init sequence.
Uses Capstone for Thumb-2 disassembly with MOVW/MOVT pair tracking.
"""

from capstone import *
import struct
import sys

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

# Function boundaries
FUNC_START = 0x08036934
FUNC_END   = 0x08039800  # ~11KB, generous end

# Known peripheral registers
PERIPH_MAP = {
    # SPI3 (0x40003C00)
    0x40003C00: "SPI3_CTL0",
    0x40003C04: "SPI3_CTL1",
    0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA",
    0x40003C10: "SPI3_CRCPOLY",
    0x40003C14: "SPI3_RCRC",
    0x40003C18: "SPI3_TCRC",
    0x40003C1C: "SPI3_I2SCTL",
    0x40003C20: "SPI3_I2SPSC",
    # SPI2 (0x40003800) - flash interface
    0x40003800: "SPI2_CTL0",
    0x40003804: "SPI2_CTL1",
    0x40003808: "SPI2_STAT",
    0x4000380C: "SPI2_DATA",
    # USART2 (0x40004800)
    0x40004800: "USART2_STS",
    0x40004804: "USART2_DATA",
    0x40004808: "USART2_BRR",
    0x4000480C: "USART2_CTL0",
    0x40004810: "USART2_CTL1",
    0x40004814: "USART2_CTL2",
    0x40004818: "USART2_GTPR",
    # USART1 (0x40013800)
    0x40013800: "USART1_STS",
    0x40013804: "USART1_DATA",
    0x4001380C: "USART1_CTL0",
    # GPIO ports
    0x40010800: "GPIOA_CTL0",
    0x40010804: "GPIOA_CTL1",
    0x40010808: "GPIOA_ISTAT",
    0x4001080C: "GPIOA_OCTL",
    0x40010810: "GPIOA_BOP",
    0x40010814: "GPIOA_BC",
    0x40010C00: "GPIOB_CTL0",
    0x40010C04: "GPIOB_CTL1",
    0x40010C08: "GPIOB_ISTAT",
    0x40010C0C: "GPIOB_OCTL",
    0x40010C10: "GPIOB_BOP",
    0x40010C14: "GPIOB_BC",
    0x40011000: "GPIOC_CTL0",
    0x40011004: "GPIOC_CTL1",
    0x40011008: "GPIOC_ISTAT",
    0x4001100C: "GPIOC_OCTL",
    0x40011010: "GPIOC_BOP",
    0x40011014: "GPIOC_BC",
    0x40011400: "GPIOD_CTL0",
    0x40011404: "GPIOD_CTL1",
    0x40011408: "GPIOD_ISTAT",
    0x4001140C: "GPIOD_OCTL",
    0x40011410: "GPIOD_BOP",
    0x40011414: "GPIOD_BC",
    # RCU (clock control)
    0x40021000: "RCU_CTL",
    0x40021004: "RCU_CFG0",
    0x40021008: "RCU_INT",
    0x4002100C: "RCU_APB2RST",
    0x40021010: "RCU_APB1RST",
    0x40021014: "RCU_AHBEN",
    0x40021018: "RCU_APB2EN",
    0x4002101C: "RCU_APB1EN",
    # AFIO (alternate function / remap)
    0x40010000: "AFIO_EC",
    0x40010004: "AFIO_PCF0",
    0x40010008: "AFIO_EXTISS0",
    # DMA0
    0x40020000: "DMA0_INTF",
    0x40020004: "DMA0_INTC",
    0x40020008: "DMA0_CH0CTL",
    0x4002000C: "DMA0_CH0CNT",
    0x40020010: "DMA0_CH0PADDR",
    0x40020014: "DMA0_CH0MADDR",
    0x4002001C: "DMA0_CH1CTL",
    0x40020020: "DMA0_CH1CNT",
    0x40020024: "DMA0_CH1PADDR",
    0x40020028: "DMA0_CH1MADDR",
    # Timer registers
    0x40000000: "TMR1_CTL0",  # TIM2
    0x40000400: "TMR2_CTL0",  # TIM3
    0x40000800: "TMR3_CTL0",  # TIM4
    # NVIC / System
    0xE000ED04: "SCB_ICSR",   # PendSV trigger
    0xE000E100: "NVIC_ISER0",
    0xE000E200: "NVIC_ISPR0",
    0xE000E010: "SYSTICK_CTL",
    # EXMC (LCD)
    0x6001FFFE: "LCD_CMD",
    0x60020000: "LCD_DATA",
}

# Known RAM addresses
RAM_MAP = {
    0x20000000: "ram_base",
    0x200000F8: "meter_state_base",
    0x20001025: "meter_mode",
    0x20001038: "meter_raw_float",
    0x20008350: "display_buf_ptr1",
    0x20008352: "display_buf_ptr2",
}

# Known function addresses
FUNC_MAP = {
    0x0803acf0: "xQueueGenericSend",
    0x0803b09c: "xQueueGenericSendFromISR",
    0x0803b1d8: "xQueueReceive",
    0x0803a53c: "vTaskDelay",
    0x08036934: "fpga_task_real",
    0x08002670: "dma_lcd_transfer",
    0x08004d60: "meter_state_machine",
    0x08008154: "display_render_engine",
    0x0803a0f4: "xTaskCreate",
    0x0803aa10: "xQueueCreate",
}

def load_firmware():
    with open(FIRMWARE, "rb") as f:
        return f.read()

def resolve_periph(addr):
    """Look up a peripheral register name."""
    if addr in PERIPH_MAP:
        return PERIPH_MAP[addr]
    # Check if it's near a known base
    for base_name, ranges in [
        ("SPI3", (0x40003C00, 0x40003C30)),
        ("SPI2", (0x40003800, 0x40003830)),
        ("USART2", (0x40004800, 0x40004820)),
        ("GPIOB", (0x40010C00, 0x40010C20)),
    ]:
        if ranges[0] <= addr < ranges[1]:
            return f"{base_name}+{addr - ranges[0]:#x}"
    if 0x40000000 <= addr < 0x50000000:
        return f"PERIPH_{addr:#010x}"
    if 0x20000000 <= addr < 0x20040000:
        if addr in RAM_MAP:
            return RAM_MAP[addr]
        return f"RAM_{addr:#010x}"
    if 0x08000000 <= addr < 0x080C0000:
        if addr in FUNC_MAP:
            return FUNC_MAP[addr]
        return f"FLASH_{addr:#010x}"
    return None

def disassemble_function(fw_data):
    """Disassemble with MOVW/MOVT tracking and peripheral annotation."""
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    offset = FUNC_START - BASE_ADDR
    size = FUNC_END - FUNC_START
    code = fw_data[offset:offset + size]

    # Track MOVW/MOVT pairs per register
    movw_vals = {}  # reg_name -> low16

    # Collect results
    lines = []
    periph_accesses = []
    branch_targets = {}
    spi3_config = []
    usart2_ops = []
    queue_calls = []

    instructions = list(md.disasm(code, FUNC_START))

    for i, insn in enumerate(instructions):
        addr = insn.address
        mnemonic = insn.mnemonic
        op_str = insn.op_str
        annotation = ""

        # Track MOVW (load low 16 bits)
        if mnemonic == "movw":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    val = int(parts[1], 0)
                    movw_vals[reg] = val
                except ValueError:
                    pass

        # Track MOVT (load high 16 bits) - reconstruct 32-bit value
        elif mnemonic == "movt":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    high = int(parts[1], 0)
                    if reg in movw_vals:
                        full_val = (high << 16) | movw_vals[reg]
                        name = resolve_periph(full_val)
                        if name:
                            annotation = f"  ; {reg} = {full_val:#010x} = {name}"
                        else:
                            annotation = f"  ; {reg} = {full_val:#010x}"

                        # Track SPI3 accesses
                        if 0x40003C00 <= full_val < 0x40003C30:
                            spi3_config.append((addr, name, full_val))
                        # Track USART2 accesses
                        if 0x40004800 <= full_val < 0x40004820:
                            usart2_ops.append((addr, name, full_val))
                except ValueError:
                    pass

        # Track BL (function calls)
        elif mnemonic == "bl":
            try:
                target = int(op_str.lstrip("#"), 0)
                name = FUNC_MAP.get(target)
                if name:
                    annotation = f"  ; CALL {name}"
                    if "Queue" in name:
                        queue_calls.append((addr, name))
                else:
                    annotation = f"  ; CALL FUN_{target:08x}"
                branch_targets[target] = branch_targets.get(target, 0) + 1
            except ValueError:
                pass

        # Track conditional branches
        elif mnemonic.startswith("b") and mnemonic not in ("bl", "blx", "bx"):
            try:
                target = int(op_str.lstrip("#"), 0)
                if FUNC_START <= target <= FUNC_END:
                    annotation = f"  ; -> {target:#010x}"
            except ValueError:
                pass

        # Track STR to peripheral addresses (writes)
        elif mnemonic in ("str", "strh", "strb"):
            # Check if writing to a register we know holds a peripheral addr
            pass  # Complex - handled by MOVW/MOVT tracking

        # Track LDR from peripheral addresses (reads)
        elif mnemonic in ("ldr", "ldrh", "ldrb"):
            pass

        # Track immediate comparisons (likely command IDs or status checks)
        elif mnemonic == "cmp":
            parts = op_str.split(", #")
            if len(parts) == 2:
                try:
                    val = int(parts[1], 0)
                    if 0 < val < 0x100:
                        annotation = f"  ; compare with {val:#04x} ({val})"
                except ValueError:
                    pass

        line = f"  {addr:#010x}:  {mnemonic:<8s} {op_str}{annotation}"
        lines.append(line)

    return lines, spi3_config, usart2_ops, queue_calls, branch_targets

def main():
    print(f"Loading firmware: {FIRMWARE}")
    fw = load_firmware()
    print(f"Firmware size: {len(fw)} bytes ({len(fw)/1024:.1f} KB)")
    print(f"Disassembling FUN_08036934 ({FUNC_END - FUNC_START} bytes)...")
    print()

    lines, spi3_config, usart2_ops, queue_calls, branch_targets = disassemble_function(fw)

    # Print summary first
    print("=" * 70)
    print("SUMMARY: FPGA Task (FUN_08036934)")
    print("=" * 70)

    print(f"\nSPI3 register accesses ({len(spi3_config)}):")
    for addr, name, val in spi3_config:
        print(f"  {addr:#010x}: {name} ({val:#010x})")

    print(f"\nUSART2 register accesses ({len(usart2_ops)}):")
    for addr, name, val in usart2_ops:
        print(f"  {addr:#010x}: {name} ({val:#010x})")

    print(f"\nQueue operations ({len(queue_calls)}):")
    for addr, name in queue_calls:
        print(f"  {addr:#010x}: {name}")

    # External function calls
    external_calls = {t: c for t, c in branch_targets.items()
                      if not (FUNC_START <= t <= FUNC_END)}
    print(f"\nExternal function calls ({len(external_calls)}):")
    for target in sorted(external_calls):
        name = FUNC_MAP.get(target, f"FUN_{target:08x}")
        print(f"  {target:#010x}: {name} (called {external_calls[target]}x)")

    print()
    print("=" * 70)
    print("FULL DISASSEMBLY")
    print("=" * 70)

    for line in lines:
        print(line)

if __name__ == "__main__":
    main()
