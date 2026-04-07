#!/usr/bin/env python3
"""
Capstone-based peripheral register write tracer for ARM Cortex-M firmware.

Disassembles a region of the stock firmware binary and extracts every
store instruction (STR/STRH/STRB) targeting peripheral MMIO space
(0x40000000 - 0x5FFFFFFF). Uses lightweight register tracking to resolve
base+offset addressing.

Usage:
    python3 trace_peripheral_writes.py <binary> <start_offset> <length>
    python3 trace_peripheral_writes.py  # defaults to stock V1.2.0 master_init

Output: sorted table of peripheral register writes.

Part of the SPI3 compliance audit escalation (2026-04-06).
"""

import sys
import struct
from collections import defaultdict
from capstone import *
from capstone.arm import *

# Peripheral address ranges
PERIPH_BASE = 0x40000000
PERIPH_END  = 0x60000000  # includes EXMC/XMC at 0xA0000000? No, handle separately.

# Known peripheral register names (AT32F403A / GD32F307 compatible)
PERIPH_NAMES = {
    # CRM / RCC
    0x40021000: "CRM_CTRL",
    0x40021004: "CRM_CFG",
    0x40021008: "CRM_CLKINT",
    0x4002100C: "CRM_APB2RST",
    0x40021010: "CRM_APB1RST",
    0x40021014: "CRM_AHBEN",
    0x40021018: "CRM_APB2EN",
    0x4002101C: "CRM_APB1EN",
    0x40021020: "CRM_BPDC",
    0x40021024: "CRM_CTRLSTS",
    0x40021028: "CRM_AHBRST",
    0x4002102C: "CRM_MISC1",  # AT32 specific
    0x40021030: "CRM_MISC2",  # AT32 specific

    # IOMUX / AFIO
    0x40010000: "IOMUX_EVCTRL",
    0x40010004: "IOMUX_REMAP",
    0x40010008: "IOMUX_EXTIC1",
    0x4001000C: "IOMUX_EXTIC2",
    0x40010010: "IOMUX_EXTIC3",
    0x40010014: "IOMUX_EXTIC4",
    0x40010020: "IOMUX_REMAP2",
    0x40010024: "IOMUX_REMAP3",
    0x40010028: "IOMUX_REMAP4",
    0x4001002C: "IOMUX_REMAP5",
    0x40010030: "IOMUX_REMAP6",
    0x40010034: "IOMUX_REMAP7",
    0x40010038: "IOMUX_REMAP8",

    # GPIOA
    0x40010800: "GPIOA_CFGLR",
    0x40010804: "GPIOA_CFGHR",
    0x40010808: "GPIOA_IDT",
    0x4001080C: "GPIOA_ODT",
    0x40010810: "GPIOA_SCR",
    0x40010814: "GPIOA_CLR",
    0x40010818: "GPIOA_WPR",

    # GPIOB
    0x40010C00: "GPIOB_CFGLR",
    0x40010C04: "GPIOB_CFGHR",
    0x40010C08: "GPIOB_IDT",
    0x40010C0C: "GPIOB_ODT",
    0x40010C10: "GPIOB_SCR",
    0x40010C14: "GPIOB_CLR",
    0x40010C18: "GPIOB_WPR",

    # GPIOC
    0x40011000: "GPIOC_CFGLR",
    0x40011004: "GPIOC_CFGHR",
    0x40011008: "GPIOC_IDT",
    0x4001100C: "GPIOC_ODT",
    0x40011010: "GPIOC_SCR",
    0x40011014: "GPIOC_CLR",
    0x40011018: "GPIOC_WPR",

    # GPIOD
    0x40011400: "GPIOD_CFGLR",
    0x40011404: "GPIOD_CFGHR",
    0x40011408: "GPIOD_IDT",
    0x4001140C: "GPIOD_ODT",
    0x40011410: "GPIOD_SCR",
    0x40011414: "GPIOD_CLR",

    # GPIOE
    0x40011800: "GPIOE_CFGLR",
    0x40011804: "GPIOE_CFGHR",
    0x40011808: "GPIOE_IDT",
    0x4001180C: "GPIOE_ODT",
    0x40011810: "GPIOE_SCR",
    0x40011814: "GPIOE_CLR",

    # SPI3 (0x40003C00)
    0x40003C00: "SPI3_CTRL1",
    0x40003C04: "SPI3_CTRL2",
    0x40003C08: "SPI3_STS",
    0x40003C0C: "SPI3_DT",
    0x40003C10: "SPI3_CPOLY",
    0x40003C14: "SPI3_RCRC",
    0x40003C18: "SPI3_TCRC",
    0x40003C1C: "SPI3_I2SCTRL",
    0x40003C20: "SPI3_I2SCLKP",

    # SPI2 (0x40003800) — SPI flash
    0x40003800: "SPI2_CTRL1",
    0x40003804: "SPI2_CTRL2",
    0x40003808: "SPI2_STS",
    0x4000380C: "SPI2_DT",

    # USART2 (0x40004400)
    0x40004400: "USART2_STS",
    0x40004404: "USART2_DT",
    0x40004408: "USART2_BAUDR",
    0x4000440C: "USART2_CTRL1",
    0x40004410: "USART2_CTRL2",
    0x40004414: "USART2_CTRL3",

    # USART1 (0x40013800)
    0x40013800: "USART1_STS",
    0x40013804: "USART1_DT",
    0x40013808: "USART1_BAUDR",
    0x4001380C: "USART1_CTRL1",

    # TMR2 (0x40000000)
    0x40000000: "TMR2_CTRL1",
    0x40000004: "TMR2_CTRL2",
    0x40000024: "TMR2_PR",
    0x4000002C: "TMR2_DIV",

    # TMR3 (0x40000400)
    0x40000400: "TMR3_CTRL1",
    0x40000404: "TMR3_CTRL2",
    0x4000040C: "TMR3_IDEN",
    0x40000424: "TMR3_PR",
    0x4000042C: "TMR3_DIV",

    # TMR5 (0x40000C00)
    0x40000C00: "TMR5_CTRL1",
    0x40000C04: "TMR5_CTRL2",
    0x40000C24: "TMR5_PR",
    0x40000C2C: "TMR5_DIV",

    # TMR6 (0x40001000)
    0x40001000: "TMR6_CTRL1",
    0x40001024: "TMR6_PR",
    0x4000102C: "TMR6_DIV",

    # TMR7 (0x40001400)
    0x40001400: "TMR7_CTRL1",
    0x40001424: "TMR7_PR",
    0x4000142C: "TMR7_DIV",

    # TMR8 (0x40013400) — on APB2
    0x40013400: "TMR8_CTRL1",
    0x40013424: "TMR8_PR",
    0x4001342C: "TMR8_DIV",

    # DAC (0x40007400)
    0x40007400: "DAC_CTRL",
    0x40007404: "DAC_SWTRG",
    0x40007408: "DAC1_DHR12R",
    0x4000740C: "DAC1_DHR12L",
    0x40007414: "DAC2_DHR12R",

    # ADC1 (0x40012400)
    0x40012400: "ADC1_STS",
    0x40012404: "ADC1_CTRL1",
    0x40012408: "ADC1_CTRL2",
    0x4001240C: "ADC1_SPT1",
    0x40012410: "ADC1_SPT2",
    0x40012434: "ADC1_OSQ3",
    0x4001244C: "ADC1_ODT",

    # DMA1 (0x40020000)
    0x40020000: "DMA1_ISTS",
    0x40020004: "DMA1_ICLR",
    0x40020008: "DMA1_C1CTRL",
    0x4002000C: "DMA1_C1DTCNT",
    0x40020010: "DMA1_C1PADDR",
    0x40020014: "DMA1_C1MADDR",
    0x4002001C: "DMA1_C2CTRL",
    0x40020020: "DMA1_C2DTCNT",
    0x40020024: "DMA1_C2PADDR",
    0x40020028: "DMA1_C2MADDR",
    0x40020044: "DMA1_C4CTRL",
    0x40020048: "DMA1_C4DTCNT",
    0x4002004C: "DMA1_C4PADDR",
    0x40020050: "DMA1_C4MADDR",
    0x40020058: "DMA1_C5CTRL",
    0x4002005C: "DMA1_C5DTCNT",
    0x40020060: "DMA1_C5PADDR",
    0x40020064: "DMA1_C5MADDR",

    # NVIC (0xE000E000)
    0xE000E100: "NVIC_ISER0",
    0xE000E104: "NVIC_ISER1",
    0xE000E108: "NVIC_ISER2",
    0xE000E400: "NVIC_IPR0",

    # SysTick (0xE000E000)
    0xE000E010: "SYSTICK_CTRL",
    0xE000E014: "SYSTICK_LOAD",
    0xE000E018: "SYSTICK_VAL",

    # SCB
    0xE000ED0C: "SCB_AIRCR",
    0xE000ED08: "SCB_VTOR",

    # EXMC / XMC (0xA0000000)
    0xA0000000: "XMC_SNCTL0",
    0xA0000004: "XMC_SNTCFG0",
    0xA0000104: "XMC_SNWTCFG0",
    0xA0000220: "XMC_EXT0",  # AT32-specific extended register

    # IWDG (0x40003000)
    0x40003000: "IWDG_CMD",
    0x40003004: "IWDG_DIV",
    0x40003008: "IWDG_RLD",
}


def name_reg(addr):
    """Look up a human-readable name for a peripheral register address."""
    if addr in PERIPH_NAMES:
        return PERIPH_NAMES[addr]
    # Try to identify the peripheral block
    blocks = {
        (0x40010800, 0x40010820): "GPIOA",
        (0x40010C00, 0x40010C20): "GPIOB",
        (0x40011000, 0x40011020): "GPIOC",
        (0x40011400, 0x40011420): "GPIOD",
        (0x40011800, 0x40011820): "GPIOE",
        (0x40003C00, 0x40003C30): "SPI3",
        (0x40004400, 0x40004420): "USART2",
        (0x40000000, 0x40000050): "TMR2",
        (0x40000400, 0x40000450): "TMR3",
        (0x40000C00, 0x40000C50): "TMR5",
        (0x40001000, 0x40001050): "TMR6",
        (0x40020000, 0x40020070): "DMA1",
        (0xE000E000, 0xE000F000): "NVIC/SCB",
        (0xA0000000, 0xA0001000): "XMC/EXMC",
    }
    for (lo, hi), name in blocks.items():
        if lo <= addr < hi:
            return f"{name}+{addr-lo:#x}"
    return f"PERIPH_{addr:#010x}"


def is_periph_addr(addr):
    """Check if address is in peripheral MMIO space."""
    return (0x40000000 <= addr < 0x60000000 or
            0xA0000000 <= addr < 0xA0001000 or
            0xE000E000 <= addr < 0xE000F000)


def trace_stores(binary_data, base_addr, start_offset, length, label="stock"):
    """
    Disassemble ARM Thumb code and extract peripheral register stores.

    Uses lightweight register tracking: follows MOV, MOVW, MOVT, LDR=imm,
    ADD instructions to resolve base registers for STR/STRH/STRB.
    """
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True

    code = binary_data[start_offset:start_offset + length]

    # Register state tracking (simplified — tracks known constant values)
    regs = {}  # reg_id -> known_value or None

    stores = []  # list of (flash_addr, periph_addr, width, reg_name, value_info)

    # Also track LDR from literal pool
    def read_word(offset):
        """Read a 32-bit word from the binary at a given flash offset."""
        if 0 <= offset < len(binary_data) - 3:
            return struct.unpack_from('<I', binary_data, offset)[0]
        return None

    for insn in md.disasm(code, base_addr + start_offset):
        mnemonic = insn.mnemonic

        # Skip non-ARM instructions
        if insn.id == 0:
            continue

        # Track MOVW (16-bit immediate to register)
        if mnemonic == 'movw' and len(insn.operands) == 2:
            dst = insn.operands[0]
            src = insn.operands[1]
            if dst.type == ARM_OP_REG and src.type == ARM_OP_IMM:
                regs[dst.reg] = src.imm & 0xFFFF

        # Track MOVT (set upper 16 bits)
        elif mnemonic == 'movt' and len(insn.operands) == 2:
            dst = insn.operands[0]
            src = insn.operands[1]
            if dst.type == ARM_OP_REG and src.type == ARM_OP_IMM:
                if dst.reg in regs and regs[dst.reg] is not None:
                    regs[dst.reg] = (regs[dst.reg] & 0xFFFF) | ((src.imm & 0xFFFF) << 16)
                else:
                    regs[dst.reg] = (src.imm & 0xFFFF) << 16

        # Track MOV (register immediate)
        elif mnemonic in ('mov', 'mov.w', 'movs') and len(insn.operands) == 2:
            dst = insn.operands[0]
            src = insn.operands[1]
            if dst.type == ARM_OP_REG and src.type == ARM_OP_IMM:
                regs[dst.reg] = src.imm
            elif dst.type == ARM_OP_REG and src.type == ARM_OP_REG:
                if src.reg in regs:
                    regs[dst.reg] = regs[src.reg]

        # Track LDR Rn, [PC, #imm] (literal pool load)
        elif mnemonic in ('ldr', 'ldr.w') and len(insn.operands) == 2:
            dst = insn.operands[0]
            src = insn.operands[1]
            if (dst.type == ARM_OP_REG and src.type == ARM_OP_MEM and
                src.mem.base == ARM_REG_PC):
                # PC-relative literal pool load
                # PC is aligned to 4 bytes and points 4 bytes ahead in Thumb
                pc_val = (insn.address + 4) & ~3
                pool_addr = pc_val + src.mem.disp
                file_offset = pool_addr - base_addr
                val = read_word(file_offset)
                if val is not None:
                    regs[dst.reg] = val

        # Track ADD with immediate
        elif mnemonic in ('add', 'add.w', 'adds') and len(insn.operands) >= 2:
            if len(insn.operands) == 3:
                dst = insn.operands[0]
                src1 = insn.operands[1]
                src2 = insn.operands[2]
                if (dst.type == ARM_OP_REG and src1.type == ARM_OP_REG and
                    src2.type == ARM_OP_IMM and src1.reg in regs and regs[src1.reg] is not None):
                    regs[dst.reg] = (regs[src1.reg] + src2.imm) & 0xFFFFFFFF
            elif len(insn.operands) == 2:
                dst = insn.operands[0]
                src = insn.operands[1]
                if dst.type == ARM_OP_REG and src.type == ARM_OP_IMM:
                    if dst.reg in regs and regs[dst.reg] is not None:
                        regs[dst.reg] = (regs[dst.reg] + src.imm) & 0xFFFFFFFF

        # Track ORR with immediate (common for setting bits)
        elif mnemonic in ('orr', 'orr.w', 'orrs') and len(insn.operands) == 3:
            dst = insn.operands[0]
            src1 = insn.operands[1]
            src2 = insn.operands[2]
            if (dst.type == ARM_OP_REG and src1.type == ARM_OP_REG and
                src2.type == ARM_OP_IMM and src1.reg in regs and regs[src1.reg] is not None):
                regs[dst.reg] = (regs[src1.reg] | src2.imm) & 0xFFFFFFFF

        # Track BIC (bit clear)
        elif mnemonic in ('bic', 'bic.w', 'bics') and len(insn.operands) == 3:
            dst = insn.operands[0]
            src1 = insn.operands[1]
            src2 = insn.operands[2]
            if (dst.type == ARM_OP_REG and src1.type == ARM_OP_REG and
                src2.type == ARM_OP_IMM and src1.reg in regs and regs[src1.reg] is not None):
                regs[dst.reg] = (regs[src1.reg] & ~src2.imm) & 0xFFFFFFFF

        # === STORE INSTRUCTIONS — the main target ===
        elif mnemonic in ('str', 'str.w', 'strh', 'strh.w', 'strb', 'strb.w'):
            if len(insn.operands) >= 2:
                val_op = insn.operands[0]  # value being stored
                mem_op = insn.operands[1]  # destination memory

                if mem_op.type == ARM_OP_MEM:
                    base_reg = mem_op.mem.base
                    disp = mem_op.mem.disp
                    index_reg = mem_op.mem.index

                    target_addr = None

                    # Simple base + displacement
                    if base_reg in regs and regs[base_reg] is not None and index_reg == 0:
                        target_addr = (regs[base_reg] + disp) & 0xFFFFFFFF
                    # Base + index register
                    elif (base_reg in regs and regs[base_reg] is not None and
                          index_reg != 0 and index_reg in regs and regs[index_reg] is not None):
                        target_addr = (regs[base_reg] + regs[index_reg]) & 0xFFFFFFFF

                    if target_addr is not None and is_periph_addr(target_addr):
                        width = 32 if 'str' == mnemonic[:3] and 'h' not in mnemonic and 'b' not in mnemonic else \
                                16 if 'h' in mnemonic else 8
                        # Adjust for strh/strb
                        if mnemonic.startswith('strh'):
                            width = 16
                        elif mnemonic.startswith('strb'):
                            width = 8
                        else:
                            width = 32

                        # Try to get stored value
                        val_str = "?"
                        if val_op.type == ARM_OP_REG and val_op.reg in regs and regs[val_op.reg] is not None:
                            val_str = f"0x{regs[val_op.reg]:08X}"
                        elif val_op.type == ARM_OP_REG:
                            val_str = insn.reg_name(val_op.reg)

                        stores.append({
                            'addr': insn.address,
                            'target': target_addr,
                            'width': width,
                            'name': name_reg(target_addr),
                            'value': val_str,
                            'disasm': f"{insn.mnemonic} {insn.op_str}",
                        })

        # Invalidate registers on function calls or branches
        if mnemonic in ('bl', 'blx'):
            # Caller-saved registers (r0-r3, r12, lr) are destroyed
            for r in [ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R12, ARM_REG_LR]:
                regs.pop(r, None)

        # On branch, we lose all tracking (conservative)
        if mnemonic in ('b', 'b.w', 'bx') and not mnemonic.startswith('bic'):
            regs.clear()

    return stores


def main():
    # Default: stock V1.2.0 master_init (FUN_08023A50)
    binary_path = "/Users/david/Desktop/osc/archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
    base_addr = 0x08000000

    # master_init: 0x08023A50 to approximately 0x080276F2 (15.4KB)
    start_offset = 0x23A50
    length = 0x276F2 - 0x23A50  # ~15.6KB

    if len(sys.argv) > 1:
        binary_path = sys.argv[1]
    if len(sys.argv) > 2:
        start_offset = int(sys.argv[2], 0)
    if len(sys.argv) > 3:
        length = int(sys.argv[3], 0)

    with open(binary_path, 'rb') as f:
        binary_data = f.read()

    print(f"Binary: {binary_path}")
    print(f"Region: 0x{base_addr + start_offset:08X} - 0x{base_addr + start_offset + length:08X} ({length} bytes)")
    print(f"{'='*100}")

    stores = trace_stores(binary_data, base_addr, start_offset, length)

    # Sort by target register address for easy comparison
    stores_by_target = defaultdict(list)
    for s in stores:
        stores_by_target[s['target']].append(s)

    # Print in order of target address
    print(f"\n{'Flash Addr':<14} {'Periph Reg':<24} {'Name':<20} {'Width':<6} {'Value':<14} {'Disasm'}")
    print(f"{'-'*14} {'-'*24} {'-'*20} {'-'*6} {'-'*14} {'-'*40}")

    for target in sorted(stores_by_target.keys()):
        for s in stores_by_target[target]:
            print(f"0x{s['addr']:08X}   0x{s['target']:08X}              "
                  f"{s['name']:<20} {s['width']:<6} {s['value']:<14} {s['disasm']}")

    # Summary by peripheral
    print(f"\n{'='*100}")
    print(f"\nSummary: {len(stores)} peripheral stores found")
    print(f"\nBy peripheral block:")
    block_counts = defaultdict(int)
    for s in stores:
        block = s['name'].split('_')[0] if '_' in s['name'] else s['name'].split('+')[0]
        block_counts[block] += 1
    for block, count in sorted(block_counts.items(), key=lambda x: -x[1]):
        print(f"  {block:<20} {count:>4} writes")

    # Output CSV for diffing
    csv_path = binary_path.rsplit('.', 1)[0] + '_periph_trace.csv'
    with open(csv_path, 'w') as f:
        f.write("flash_addr,periph_addr,name,width,value,disasm\n")
        for s in sorted(stores, key=lambda x: x['addr']):
            f.write(f"0x{s['addr']:08X},0x{s['target']:08X},{s['name']},{s['width']},{s['value']},\"{s['disasm']}\"\n")
    print(f"\nCSV written to: {csv_path}")

    return stores


if __name__ == '__main__':
    main()
