#!/usr/bin/env python3
"""Trace the USART command init sequence sent to FPGA at boot.

From the init function, there are multiple xQueueGenericSend calls at:
  0x08025D7C, 0x08026DDA-0x08026E2A
These send bytes to the USART TX queue (0x20002D6C or 0x20002D74).

Also trace the USART TX interrupt handler to understand the frame format.
"""

from capstone import *

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH_MAP = {
    0x40003C00: "SPI3_CTL0", 0x40003C08: "SPI3_STAT", 0x40003C0C: "SPI3_DATA",
    0x4000440C: "USART1_CTL0", 0x40004400: "USART1_STS", 0x40004404: "USART1_DATA",
    0x40010C10: "GPIOB_BOP", 0x40010C14: "GPIOB_BC",
    0x40011008: "GPIOC_IDR", 0x40011010: "GPIOC_BOP",
    0x40021018: "RCU_APB2EN", 0x4002101C: "RCU_APB1EN",
}

FUNC_MAP = {
    0x0803acf0: "xQueueGenericSend",
    0x0803b09c: "xQueueGenericSendFromISR",
    0x0803b1d8: "xQueueReceive",
    0x0803a53c: "vTaskDelay",
    0x080302fc: "gpio_init",
    0x0803ab74: "xQueueCreate",
    0x0803b6a0: "xTaskCreate",
    0x0803bd88: "xTaskCreate2",
    0x0803a390: "vTaskDelay2",
}

def disasm_range(fw, start, end, title):
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    offset = start - BASE_ADDR
    code = fw[offset:offset + (end - start)]
    movw_vals = {}

    print(f"\n{'='*70}")
    print(f"  {title}")
    print(f"  {start:#010x} — {end:#010x}")
    print(f"{'='*70}")

    for insn in md.disasm(code, start):
        if insn.address > end:
            break
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
                        name = PERIPH_MAP.get(full)
                        if name:
                            annotation = f"  ; {reg} = {name}"
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
                fn = FUNC_MAP.get(target, f"FUN_{target:08x}")
                annotation = f"  ; -> {fn}"
            except ValueError:
                pass
        elif insn.mnemonic in ("movs", "mov"):
            # Track immediate moves for command bytes
            parts = insn.op_str.split(", #")
            if len(parts) == 2:
                try:
                    val = int(parts[1], 0)
                    if 0 < val < 0x100:
                        annotation = f"  ; = {val:#04x} ({val})"
                except ValueError:
                    pass
        elif insn.mnemonic == "strb":
            annotation = "  ; STORE BYTE"

        print(f"  {insn.address:#010x}: {insn.mnemonic:<10s} {insn.op_str}{annotation}")

with open(FIRMWARE, "rb") as f:
    fw = f.read()

# 1. The queue sends during init — commands being sent to FPGA
# From the init code at 0x08026D90-0x08026E30
disasm_range(fw, 0x08026D80, 0x08026E40,
    "USART init commands sent to FPGA (xQueueGenericSend calls)")

# 2. Earlier queue sends in the init
disasm_range(fw, 0x08025D50, 0x08025DA0,
    "Earlier init queue sends")

# 3. The USART TX interrupt handler
# On AT32, USART2 (0x40004400) IRQ = USART2_IRQHandler
# The vector table is at 0x08000000. USART2 IRQ number on AT32F403A = 38
# Vector offset = 0x40 + 38*4 = 0x40 + 0x98 = 0xD8
import struct
vec_offset = 0xD8
usart2_handler = struct.unpack_from("<I", fw, vec_offset)[0]
print(f"\n\nUSART2 IRQ vector at offset 0x{vec_offset:X}: {usart2_handler:#010x}")

# Actually, AT32 USART numbering: USART1=IRQ37, USART2=IRQ38, USART3=IRQ39
# But the FPGA USART is at 0x40004400 = AT32 USART2 = IRQ38
# Vector = 0x40 + 38*4 = 0xD8
# Let's also check nearby vectors
for irq in range(36, 42):
    off = 0x40 + irq * 4
    handler = struct.unpack_from("<I", fw, off)[0]
    print(f"  IRQ {irq}: vector at 0x{off:02X} = {handler:#010x}")

# Disassemble the USART2 handler
if usart2_handler > 0x08000000 and usart2_handler < 0x080c0000:
    # Clear the thumb bit
    handler_addr = usart2_handler & ~1
    disasm_range(fw, handler_addr, handler_addr + 0x200,
        f"USART2 IRQ Handler (at {handler_addr:#010x})")

# 4. Also look at what happens right after SPI3 enable (0x08026600-0x08026680)
# This is where SysTick delays and first SPI3 transactions happen
disasm_range(fw, 0x08026600, 0x080268C0,
    "Post-SPI3-enable: SysTick delays + first SPI3 transactions")

# 5. The FPGA task USART command section (0x080373F0-0x08037454)
disasm_range(fw, 0x080373F0, 0x08037460,
    "FPGA task: USART TX command sender")
