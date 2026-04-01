#!/usr/bin/env python3
"""
Comprehensive decompilation of FPGA Task (FUN_08036934) from FNIRSI 2C53T V1.2.0 firmware.

Disassembles the full 0x08036934-0x08039870 range, resolves MOVW/MOVT pairs,
identifies SPI3 transfer patterns, CS control, queue operations, table branches,
VFP operations, and outputs clean annotated pseudocode.
"""

import struct
import sys
import os
from collections import defaultdict

# Capstone disassembler
from capstone import *
from capstone.arm import *

FIRMWARE_PATH = os.path.join(os.path.dirname(__file__), "..",
                             "2C53T Firmware V1.2.0", "APP_2C53T_V1.2.0_251015.bin")
BASE_ADDR = 0x08000000
FUNC_START = 0x08036934
FUNC_END = 0x08039870  # generous end to capture tail functions

OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "..",
                           "reverse_engineering", "analysis_v120", "fpga_task_decompile.txt")

# ============================================================================
# Known symbols
# ============================================================================
KNOWN_FUNCTIONS = {
    0x0800157c: "spi_flash_write_page",
    0x080028e0: "fpga_state_update",
    0x08002c78: "input_handler",
    0x08019e98: "display_update_meter",
    0x080223bc: "system_reset_or_power_off",
    0x0802f16c: "spi_flash_read",
    0x080302fc: "gpio_init",
    0x08033ef8: "command_lookup_table",
    0x08034078: "measurement_calc",
    0x0803a390: "vTaskDelay",
    0x0803acf0: "xQueueGenericSend",
    0x0803b06c: "uxQueueMessagesWaiting",
    0x0803b09c: "xQueueGenericSendFromISR",
    0x0803b1d8: "xQueueReceive",
    0x0803b3a8: "xQueueSemaphoreTake",
    0x0803c8e0: "__aeabi_ddiv",
    0x0803df48: "__aeabi_d2iz",
    0x0803dfac: "__aeabi_d2uiz_v2",
    0x0803e124: "__aeabi_dmul",
    0x0803e5da: "__aeabi_ui2d",
    0x0803e77c: "__aeabi_d2uiz",
    0x0803eb94: "__aeabi_dcmpge",
    0x0803ed70: "__aeabi_dcmplt",
    0x0803edf0: "__aeabi_dcmple",
    0x0803edfe: "__aeabi_dcmpgt",
    0x0803ee0c: "__aeabi_dcmpeq",
}

KNOWN_RAM = {
    0x20000005: "usart_tx_buf",
    0x200000F8: "meter_state",
    0x20002D50: "ram_button_state",
    0x20002D58: "debounce_counters",
    0x20002D67: "watchdog_counter",
    0x20002D6C: "usart_cmd_queue_ptr",
    0x20002D70: "button_event_queue_ptr",
    0x20002D74: "usart_tx_queue_handle",
    0x20002D78: "spi3_data_queue_handle",
    0x20002D7C: "meter_semaphore_handle",
    0x20002D80: "spi3_rx_queue_handle",
    0x20004E11: "usart_rx_frame",
}

KNOWN_PERIPH = {
    0x40002424: "TIM5_CNT",   # actually 0x40000C24
    0x40003C00: "SPI3_CTL0",
    0x40003C04: "SPI3_CTL1",
    0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA",
    0x40004400: "USART2_STS",
    0x40004404: "USART2_DATA",
    0x40004408: "USART2_BRR",
    0x4000440C: "USART2_CTL0",
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
    0x40015434: "IWDG_RLR",  # independent watchdog
    0xE000ED04: "SCB_ICSR",
}

KNOWN_FLASH = {
    0x0804BE74: "usart_cmd_dispatch_table",
    0x0804D833: "timebase_sample_count_table",
    0x08046528: "button_map_table",
}


def resolve_name(addr):
    """Resolve an address to a human-readable name."""
    if addr in KNOWN_FUNCTIONS:
        return KNOWN_FUNCTIONS[addr]
    if addr in KNOWN_RAM:
        return KNOWN_RAM[addr]
    if addr in KNOWN_PERIPH:
        return KNOWN_PERIPH[addr]
    if addr in KNOWN_FLASH:
        return KNOWN_FLASH[addr]
    if 0x20000000 <= addr < 0x20040000:
        # Check if it's a known offset from meter_state
        if 0x200000F8 <= addr < 0x20001100:
            off = addr - 0x200000F8
            return f"meter_state+0x{off:X}"
        return f"RAM_0x{addr:08X}"
    if 0x40000000 <= addr < 0x50000000:
        return f"PERIPH_0x{addr:08X}"
    if 0x08000000 <= addr < 0x08100000:
        return f"FLASH_0x{addr:08X}"
    return f"0x{addr:08X}"


class FPGATaskDecompiler:
    def __init__(self, firmware_data):
        self.fw = firmware_data
        self.md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        self.md.detail = True
        self.md.skipdata = True

        # Track MOVW/MOVT pairs per register
        self.reg_values = {}  # reg_id -> (value, is_complete, addr)
        self.resolved_addrs = {}  # instruction addr -> resolved value

        # Collected annotations
        self.annotations = {}  # addr -> list of annotation strings
        self.block_labels = {}  # addr -> label string

        # SPI3 transfer blocks
        self.spi3_xfers = []
        # CS operations
        self.cs_ops = []
        # Queue operations
        self.queue_ops = []
        # VFP operations
        self.vfp_blocks = []

        # Sub-functions identified within the range
        self.subfunctions = {}

    def read_bytes(self, addr, size):
        offset = addr - BASE_ADDR
        if offset < 0 or offset + size > len(self.fw):
            return None
        return self.fw[offset:offset+size]

    def read_u16(self, addr):
        b = self.read_bytes(addr, 2)
        return struct.unpack('<H', b)[0] if b else None

    def read_u32(self, addr):
        b = self.read_bytes(addr, 4)
        return struct.unpack('<I', b)[0] if b else None

    def disassemble_range(self, start, end):
        """Disassemble a range and return list of instructions."""
        code = self.read_bytes(start, end - start)
        if code is None:
            return []
        instructions = list(self.md.disasm(code, start))
        return instructions

    def resolve_movw_movt(self, instructions):
        """Track MOVW/MOVT pairs across all instructions to resolve full 32-bit values."""
        reg_low = {}   # reg_name -> (low16, addr)

        for insn in instructions:
            mnem = insn.mnemonic
            # MOVW sets lower 16 bits
            if mnem == 'movw' and len(insn.operands) == 2:
                dst = insn.reg_name(insn.operands[0].reg)
                if insn.operands[1].type == ARM_OP_IMM:
                    val = insn.operands[1].imm & 0xFFFF
                    reg_low[dst] = (val, insn.address)

            # MOVT sets upper 16 bits
            elif mnem == 'movt' and len(insn.operands) == 2:
                dst = insn.reg_name(insn.operands[0].reg)
                if insn.operands[1].type == ARM_OP_IMM:
                    high = insn.operands[1].imm & 0xFFFF
                    if dst in reg_low:
                        low_val, low_addr = reg_low[dst]
                        full = (high << 16) | low_val
                        self.resolved_addrs[low_addr] = full
                        self.resolved_addrs[insn.address] = full
                        # Annotate
                        name = resolve_name(full)
                        self.annotations.setdefault(insn.address, []).append(
                            f"  // {dst} = 0x{full:08X} = {name}")

            # Any other write to a register clears it
            elif insn.id != 0 and len(insn.operands) > 0 and insn.operands[0].type == ARM_OP_REG:
                dst = insn.reg_name(insn.operands[0].reg)
                if mnem not in ('movw', 'movt', 'cmp', 'tst'):
                    if dst in reg_low:
                        del reg_low[dst]

    def identify_spi3_xfers(self, instructions):
        """Identify SPI3 transfer pattern: wait TXE, write DATA, wait RXNE, read DATA."""
        i = 0
        while i < len(instructions) - 6:
            # Look for the TXE wait pattern: ldr + lsls #0x1e
            insn = instructions[i]
            if (insn.mnemonic in ('ldr', 'ldr.w') and
                i + 1 < len(instructions) and
                instructions[i+1].mnemonic in ('lsls', 'lsls.w')):

                lsls = instructions[i+1]
                # Check for bit 1 check (lsls #0x1e = check TXE)
                if (len(lsls.operands) >= 2 and
                    lsls.operands[-1].type == ARM_OP_IMM and
                    lsls.operands[-1].imm == 0x1e):

                    # This is a TXE wait. Look ahead for str [reg, #4] (write DATA)
                    # and then RXNE wait (lsls #0x1f) and ldr [reg, #4] (read DATA)
                    start_addr = insn.address
                    # Search forward within 20 instructions for the pattern
                    found_write = False
                    found_rxne = False
                    found_read = False
                    end_addr = start_addr
                    write_val = None

                    for j in range(i+2, min(i+25, len(instructions))):
                        ij = instructions[j]
                        if ij.mnemonic in ('str', 'str.w') and not found_write:
                            # Check if writing to offset 4 (DATA reg)
                            if (len(ij.operands) == 2 and
                                ij.operands[1].type == ARM_OP_MEM and
                                ij.operands[1].mem.disp == 4):
                                found_write = True
                                write_val = ij.reg_name(ij.operands[0].reg) if ij.operands[0].type == ARM_OP_REG else "?"

                        elif (ij.mnemonic in ('lsls', 'lsls.w') and
                              found_write and not found_rxne):
                            if (len(ij.operands) >= 2 and
                                ij.operands[-1].type == ARM_OP_IMM and
                                ij.operands[-1].imm == 0x1f):
                                found_rxne = True

                        elif (ij.mnemonic in ('ldr', 'ldr.w') and
                              found_rxne and not found_read):
                            if (len(ij.operands) == 2 and
                                ij.operands[1].type == ARM_OP_MEM and
                                ij.operands[1].mem.disp == 4):
                                found_read = True
                                end_addr = ij.address
                                break

                    if found_write and found_rxne and found_read:
                        self.spi3_xfers.append({
                            'start': start_addr,
                            'end': end_addr,
                            'write_reg': write_val
                        })
                        self.annotations.setdefault(start_addr, []).append(
                            f"  // === SPI3_XFER(write={write_val}) ===")
            i += 1

    def identify_cs_ops(self, instructions):
        """Identify CS assert/deassert: write 0x40 to GPIOB_BC or GPIOB_BOP."""
        for i, insn in enumerate(instructions):
            if insn.mnemonic in ('str', 'str.w') and len(insn.operands) == 2:
                if insn.operands[1].type == ARM_OP_MEM:
                    # Look for immediate value 0x40 being stored
                    # Check the source register for a recent movs #0x40 or similar
                    pass
            # More reliable: look for writes to known GPIOB_BC/BOP addresses
            # These typically use: movs rN, #0x40; str rN, [rM] where rM points to BC/BOP
            if insn.mnemonic in ('str', 'str.w'):
                # Check if destination resolves to GPIOB_BC (0x40010C14) or GPIOB_BOP (0x40010C10)
                if len(insn.operands) == 2 and insn.operands[1].type == ARM_OP_MEM:
                    base_reg = insn.reg_name(insn.operands[1].mem.base)
                    disp = insn.operands[1].mem.disp
                    # Check nearby resolved addresses
                    for j in range(max(0, i-10), i):
                        prev = instructions[j]
                        if prev.address in self.resolved_addrs:
                            resolved = self.resolved_addrs[prev.address]
                            target = resolved + disp
                            if target == 0x40010C14:  # GPIOB_BC
                                self.cs_ops.append(('assert', insn.address))
                                self.annotations.setdefault(insn.address, []).append(
                                    "  // CS_ASSERT (PB6 LOW)")
                            elif target == 0x40010C10:  # GPIOB_BOP
                                self.cs_ops.append(('deassert', insn.address))
                                self.annotations.setdefault(insn.address, []).append(
                                    "  // CS_DEASSERT (PB6 HIGH)")

    def identify_queue_ops(self, instructions):
        """Identify FreeRTOS queue send/receive calls."""
        for insn in instructions:
            if insn.mnemonic in ('bl', 'b.w'):
                target = insn.operands[0].imm if insn.operands[0].type == ARM_OP_IMM else 0
                if target in KNOWN_FUNCTIONS:
                    fname = KNOWN_FUNCTIONS[target]
                    if 'Queue' in fname or 'Semaphore' in fname or 'TaskDelay' in fname.replace('v', 'V'):
                        self.queue_ops.append((insn.address, fname))

    def identify_vfp_blocks(self, instructions):
        """Identify blocks of VFP (floating-point) instructions."""
        in_vfp = False
        block_start = 0
        vfp_count = 0

        for insn in instructions:
            is_vfp = insn.mnemonic.startswith('v') or insn.mnemonic.startswith('vmov')
            if is_vfp:
                if not in_vfp:
                    block_start = insn.address
                    in_vfp = True
                    vfp_count = 1
                else:
                    vfp_count += 1
            else:
                if in_vfp and vfp_count >= 3:
                    self.vfp_blocks.append((block_start, insn.address, vfp_count))
                in_vfp = False
                vfp_count = 0

    def decode_tbh(self, insn_addr, instructions, idx):
        """Decode a TBH (table branch halfword) instruction to find jump targets."""
        # TBH [pc, rN, lsl #1] - table starts at PC+4 (aligned)
        table_base = insn_addr + 4
        # Find the preceding CMP to get the max index
        max_idx = 0
        for j in range(max(0, idx-5), idx):
            prev = instructions[j]
            if prev.mnemonic == 'cmp' and len(prev.operands) == 2:
                if prev.operands[1].type == ARM_OP_IMM:
                    max_idx = prev.operands[1].imm
                    break

        targets = []
        for i in range(max_idx + 1):
            offset_val = self.read_u16(table_base + i * 2)
            if offset_val is not None:
                target = table_base + offset_val * 2
                targets.append((i, target))

        return targets

    def decode_tbb(self, insn_addr, instructions, idx):
        """Decode a TBB (table branch byte) instruction."""
        table_base = insn_addr + 4
        max_idx = 0
        for j in range(max(0, idx-5), idx):
            prev = instructions[j]
            if prev.mnemonic == 'cmp' and len(prev.operands) == 2:
                if prev.operands[1].type == ARM_OP_IMM:
                    max_idx = prev.operands[1].imm
                    break

        targets = []
        for i in range(max_idx + 1):
            b = self.read_bytes(table_base + i, 1)
            if b:
                offset_val = b[0]
                target = table_base + offset_val * 2
                targets.append((i, target))

        return targets

    def identify_subfunctions(self, instructions):
        """Identify sub-function boundaries by push {... lr} patterns."""
        for i, insn in enumerate(instructions):
            if insn.mnemonic == 'push':
                # Check if lr is in the register list
                op_str = insn.op_str
                if 'lr' in op_str:
                    self.subfunctions[insn.address] = f"sub_{insn.address:08X}"

    def build_pseudocode(self, instructions):
        """Build annotated pseudocode from the instruction stream."""
        lines = []
        indent = 0
        current_func = None

        # Identify all table branches first
        table_branches = {}
        for idx, insn in enumerate(instructions):
            if insn.mnemonic == 'tbh':
                targets = self.decode_tbh(insn.address, instructions, idx)
                table_branches[insn.address] = ('tbh', targets)
            elif insn.mnemonic == 'tbb':
                targets = self.decode_tbb(insn.address, instructions, idx)
                table_branches[insn.address] = ('tbb', targets)

        # Track which addresses are branch targets for labeling
        branch_targets = set()
        for insn in instructions:
            if insn.mnemonic in ('b', 'b.w', 'beq', 'bne', 'bhi', 'bhs', 'blo', 'bls',
                                 'bge', 'bgt', 'ble', 'blt', 'bmi', 'bpl',
                                 'beq.w', 'bne.w', 'bhi.w', 'bhs.w', 'blo.w',
                                 'bmi.w', 'bpl.w', 'bge.w', 'bgt.w', 'ble.w', 'blt.w',
                                 'cbz', 'cbnz'):
                if insn.operands and insn.operands[-1].type == ARM_OP_IMM:
                    branch_targets.add(insn.operands[-1].imm)

        # Add table branch targets
        for addr, (kind, targets) in table_branches.items():
            for idx_val, target in targets:
                branch_targets.add(target)

        # Build address -> instruction index map
        addr_to_idx = {insn.address: i for i, insn in enumerate(instructions)}

        # Now generate pseudocode
        spi3_xfer_ranges = set()
        for xfer in self.spi3_xfers:
            for a in range(xfer['start'], xfer['end'] + 4, 2):
                spi3_xfer_ranges.add(a)

        # Track which SPI3 xfer blocks we've already annotated
        annotated_xfer_starts = set()

        for idx, insn in enumerate(instructions):
            addr = insn.address

            # Sub-function boundary
            if addr in self.subfunctions and addr != FUNC_START:
                lines.append("")
                lines.append("=" * 78)
                func_name = self.subfunctions[addr]
                lines.append(f"  FUNCTION {func_name}  (0x{addr:08X})")
                lines.append("=" * 78)
                lines.append("")

            # Branch target label
            if addr in branch_targets:
                lines.append(f"  loc_{addr:08X}:")

            # Table branch annotation
            if addr in table_branches:
                kind, targets = table_branches[addr]
                lines.append(f"    // {kind.upper()} switch table:")
                for case_idx, target in targets:
                    lines.append(f"    //   case {case_idx}: -> 0x{target:08X}")
                lines.append(f"    0x{addr:08X}:  {insn.mnemonic:10s} {insn.op_str}")
                continue

            # Custom annotations
            if addr in self.annotations:
                for ann in self.annotations[addr]:
                    lines.append(ann)

            # Format the instruction
            # Annotate BL calls
            call_annotation = ""
            if insn.mnemonic in ('bl', 'blx'):
                if insn.operands and insn.operands[0].type == ARM_OP_IMM:
                    target = insn.operands[0].imm
                    if target in KNOWN_FUNCTIONS:
                        call_annotation = f"  // {KNOWN_FUNCTIONS[target]}()"
                elif insn.operands and insn.operands[0].type == ARM_OP_REG:
                    call_annotation = f"  // indirect call via {insn.reg_name(insn.operands[0].reg)}"

            # Annotate branches
            branch_annotation = ""
            if insn.mnemonic in ('b', 'b.w', 'beq', 'bne', 'bhi', 'bhs', 'blo', 'bls',
                                 'bge', 'bgt', 'ble', 'blt', 'bmi', 'bpl',
                                 'beq.w', 'bne.w', 'bhi.w', 'bhs.w', 'blo.w',
                                 'bmi.w', 'bpl.w', 'cbz', 'cbnz'):
                if insn.operands and insn.operands[-1].type == ARM_OP_IMM:
                    target = insn.operands[-1].imm
                    branch_annotation = f"  // -> loc_{target:08X}"

            # Annotate resolved MOVW/MOVT
            mov_annotation = ""
            if addr in self.resolved_addrs and insn.mnemonic == 'movt':
                val = self.resolved_addrs[addr]
                name = resolve_name(val)
                mov_annotation = f"  // = 0x{val:08X} ({name})"

            # SPI3 xfer condensed annotation
            spi3_note = ""
            if addr in spi3_xfer_ranges:
                for xfer in self.spi3_xfers:
                    if addr == xfer['start'] and addr not in annotated_xfer_starts:
                        annotated_xfer_starts.add(addr)
                        spi3_note = f"  // >>>>> SPI3_XFER: wait_TXE, write({xfer['write_reg']}), wait_RXNE, read"

            annotation = call_annotation or branch_annotation or mov_annotation or spi3_note

            line = f"    0x{addr:08X}:  {insn.mnemonic:10s} {insn.op_str:40s}{annotation}"
            lines.append(line)

        return lines

    def generate_structure_analysis(self, instructions):
        """Generate high-level structure analysis."""
        lines = []
        lines.append("=" * 78)
        lines.append("  FPGA TASK DECOMPILATION — FNIRSI 2C53T V1.2.0")
        lines.append("  Function: FUN_08036934 (0x08036934 - 0x080396C6)")
        lines.append("  Plus sub-functions through 0x08039870")
        lines.append("=" * 78)
        lines.append("")

        # Identify the actual sub-function boundaries
        lines.append("FUNCTION MAP:")
        lines.append("-" * 78)

        push_addrs = []
        for insn in instructions:
            if insn.mnemonic == 'push' and 'lr' in insn.op_str:
                push_addrs.append(insn.address)

        func_ranges = [
            (0x08036934, 0x08036A4E, "spi_flash_loader", "SPI flash bulk read (4KB pages, addr << 12)"),
            (0x08036A50, 0x08036ABC, "usart_cmd_dispatcher", "USART command queue dispatch loop"),
            (0x08036AC0, 0x080371B0, "meter_data_processor", "Meter USART RX frame decode + measurement math (VFP heavy)"),
            (0x080371B0, 0x080373A8, "meter_mode_handler", "Meter mode FSM: probe detect, range select, overload"),
            (0x080373A8, 0x08037428, "meter_result_enqueue", "Enqueue meter measurement result"),
            (0x08037428, 0x08039050, "spi3_acquisition_task", "SPI3 ADC data acquisition main loop (9 command handlers)"),
            (0x08039050, 0x08039188, "spi3_init_and_setup", "SPI3 peripheral init, GPIOB config for SPI3 remap"),
            (0x08039188, 0x080396C6, "input_and_housekeeping", "Button/probe input, watchdog, timers, queue send"),
            (0x080396C8, 0x08039734, "probe_change_handler", "Probe connection change detection + auto power-off"),
            (0x08039734, 0x08039870, "usart_tx_config_writer", "USART TX config register writer (7 command types)"),
        ]

        for start, end, name, desc in func_ranges:
            size = end - start
            lines.append(f"  0x{start:08X}-0x{end:08X}  ({size:5d} bytes)  {name}")
            lines.append(f"    {desc}")
        lines.append("")

        return lines

    def generate_pseudocode_blocks(self, instructions):
        """Generate high-level pseudocode blocks."""
        lines = []

        lines.append("=" * 78)
        lines.append("  HIGH-LEVEL PSEUDOCODE")
        lines.append("=" * 78)
        lines.append("")

        lines.append("""
// ============================================================================
// FUN_08036934: spi_flash_loader(ctx)
// 0x08036934 - 0x08036A4E
// Called with r0 = pointer to a context struct (ctx)
// Reads 4KB pages from SPI flash into ctx->buf
// ============================================================================
void spi_flash_loader(struct flash_ctx *ctx) {
    if (ctx->pending == 0) goto check_write;

    uint8_t region = ctx->region;   // ctx[1]
    uint32_t page = ctx->page;      // ctx[0x30]

    if (region == 3) {
        flash_addr = page << 12;    // Region 3: raw page address
    } else if (region == 2) {
        flash_addr = 0x200000 + (page << 12);  // Region 2: offset 2MB
    } else {
        goto done;
    }

    spi_flash_read(ctx->buf, flash_addr, 0x1000);  // Read 4KB page

    // Check if more pages needed
    remaining = ctx->page - ctx->start_page;
    ctx->pending = 0;
    if (remaining >= ctx->total_pages) goto check_write;

    // If continuous mode, read next page
    if (ctx->mode == 2) {
        next_page = ctx->total_pages + ctx->page;
        if (region == 3)
            next_addr = next_page << 12;
        else if (region == 2)
            next_addr = 0x200000 + (next_page << 12);
        spi_flash_read(ctx->buf, next_addr, 0x1000);
    }

check_write:
    if (ctx->type == 3 && ctx->write_flag == 1) {
        spi_flash_write_page(ctx->buf + 0x34, 0xFFC);
        // Write signature "ARRa" at offset 0x34
        ctx->buf[0x34] = 0x41615252;  // "RRAa"
        ctx->buf[0x234] = 0xAA55;     // marker at offset 0x1FE
        ctx->buf[0x218] = 0x61417272; // "rAa" at offset 0x1E4
        // Copy calibration data
        ...
        spi_flash_read(ctx->buf, next_addr, 0x1000);
        ctx->write_flag = 0;
    }

    if (region == 2 || region == 3)
        return 0;  // success
    return 1;       // not a valid region
}


// ============================================================================
// FUN_08036A50: usart_cmd_dispatcher
// 0x08036A50 - 0x08036ABC
// FreeRTOS task loop: receives USART commands from queue, dispatches via table
// ============================================================================
void usart_cmd_dispatcher(void) {
    // r5 = &RAM[0x20002D6C]  (usart_cmd_queue_ptr)
    // r6 = 0x0804BE74        (usart_cmd_dispatch_table)
    // r7 = &RAM[0x20002D80]  (spi3_rx_queue_handle)
    // r8 = &meter_state      (0x200000F8)

    uint8_t cmd_byte;
    while (1) {
        // Try non-blocking receive
        if (xQueueReceive(*usart_cmd_queue_ptr, &cmd_byte, 0) == pdTRUE) {
            // Dispatch: load function pointer from table indexed by cmd_byte
            void (*handler)(void) = usart_cmd_dispatch_table[cmd_byte];
            handler();  // blx r0
            continue;
        }

        // Check if spi3_rx_queue is empty AND meter in mode 2
        if (uxQueueMessagesWaiting(*spi3_rx_queue_handle) == 0) {
            if (meter_state[0xF68] == 2) {
                // Signal SPI3 queue to keep it alive
                xQueueGenericSend(*spi3_rx_queue_handle, NULL, 0);
            }
        }

        // Blocking receive (wait forever)
        if (xQueueReceive(*usart_cmd_queue_ptr, &cmd_byte, portMAX_DELAY) == pdTRUE) {
            void (*handler)(void) = usart_cmd_dispatch_table[cmd_byte];
            handler();
        }
    }
}


// ============================================================================
// FUN_08036AC0: meter_data_processor
// 0x08036AC0 - 0x080371B0
// Processes USART RX frames from FPGA for multimeter readings
// Uses double-precision FP via ABI calls and VFP for measurement math
// ============================================================================
void meter_data_processor(void) {
    // fp = &RAM[0x20002D7C]  (meter_semaphore_handle)
    // r7 = &meter_state      (0x200000F8)
    // r6 = &usart_rx_frame   (0x20004E11)

    // Pre-load VFP constants from literal pool:
    //   d8, d9, d11, d12, d13 = various measurement constants

    uint8_t r4 = 0;  // previous digit count

    while (1) {
        // Wait for meter semaphore (blocking)
        xQueueSemaphoreTake(*meter_semaphore_handle, portMAX_DELAY);

        if (meter_state[0xF35] == 0) goto process_calibration;

        // Decode USART RX frame bytes into digit nibbles
        // Frame bytes [2..7] contain BCD-encoded measurement data
        uint8_t b2 = usart_rx_frame[2];
        uint8_t b3 = usart_rx_frame[3];
        uint8_t b4 = usart_rx_frame[4];
        uint8_t b5 = usart_rx_frame[5];
        uint8_t b6 = usart_rx_frame[6];

        // Extract nibble pairs and look up via command_lookup_table()
        uint8_t digit0 = command_lookup_table((b2 & 0xF0) | (b3 & 0x0F));
        uint8_t digit1 = command_lookup_table((b3 & 0xF0) | (b4 & 0x0F));
        uint8_t digit2 = command_lookup_table((b4 & 0xF0) | (b5 & 0x0F));
        uint8_t digit3 = command_lookup_table((b5 & 0xF0) | (b6 & 0x0F));

        // Special value checks:
        if (digit0 == 0x0A && digit1 == 0x0B) {
            // "OL" (overload) indication
            meter_state[0xF35] = special_mode;
            goto store_and_continue;
        }
        if (digit1 == 0 && digit2 == 0x0E) {
            // Special zero display
            ...
        }
        if (digit0 == 0x10 && digit1 == 0x10) {
            // Blank display
            ...
        }
        if (digit0 == 0x10 && digit1 == 0x11) {
            // Partial blank
            ...
        }
        if (digit1 == 0x12 && digit2 == 0x0A) {
            // Mode 0x12:0x0A pattern -> set meter_state[0xF35] = 5
            if (digit3 == 5 && meter_state[0xF5D] != 0xB0) {
                meter_state[0xF5D] = 0xB1;  // continuity buzzer state
                meter_state[0xF2D] = 1;
            }
            ...
        }
        if (digit1 == 0x13 && digit2 == 0x14) {
            // Mode change pattern
            ...
        }
        if (digit0 == 0xFF || digit1 == 0xFF || digit2 == 0xFF) {
            // Invalid digits -> skip
            goto store_and_continue;
        }
        if (digit3 == 0xFF) {
            // Fourth digit invalid
            goto special_3digit;
        }

        // Compute raw measurement value from 4 BCD digits
        // Digits > 0x10 are treated as 0 (subtract 0x10)
        int raw = digit0_adjusted * 1000 + digit1_adjusted * 100
                + digit2_adjusted * 10 + digit3_adjusted;

        // Convert to float, apply scaling
        float value = (float)raw * scale_factor;
        ...

process_calibration:
        // Heavy FP section: calibration and range scaling
        // Uses double-precision arithmetic via __aeabi_ddiv, __aeabi_dmul, etc.
        //
        // Pattern repeated for each calibration channel:
        //   temp = __aeabi_dcmplt(meter_state[0xF30], ...)  // check polarity
        //   divisor = __aeabi_ui2d(meter_state[0xF37])       // cal coefficient
        //   quotient = __aeabi_ddiv(d8_constant, divisor)
        //   result = __aeabi_d2uiz(quotient, temp)
        //   ... compare, clamp, store

        // This processes up to 4 measurement channels with:
        //   - Polarity detection (dcmplt)
        //   - Division by calibration factor
        //   - Multiplication by range constant
        //   - Clamping to valid range
        //   - Decimal point placement via digit_count
        //   - Overload detection (dcmpeq against 0, dcmpeq against d13/d9 thresholds)
        //   - Classification: 1=normal, 2=underrange, 3=overrange, 4=invalid

        // Store processed result
        measurement_calc(result);

        // TBB switch on meter_state[0x10] (meter sub-mode, 0-7):
        //   case 0: result += digit_count + 0x0A   (add decimal offset)
        //   case 1: check polarity -> result += 2
        //   case 2: check flags -> result += 2
        //   case 3: result = 0xFF                   (invalid)
        //   case 4: check dual -> conditional offset
        //   case 5: result += digit_count + 2
        //   case 6: result += digit_count + 0x0A
        //   case 7: special handling
        ...

store_and_continue:
        meter_state[0xF30] = 0;
    }
}


// ============================================================================
// 0x080371B0 - 0x080373A8: meter_mode_handler
// Processes USART RX frame byte[7] bits to determine meter operating mode
// ============================================================================
void meter_mode_handler(uint8_t *rx_frame) {
    uint8_t status_byte = rx_frame[7];
    uint8_t flags_byte = rx_frame[6];

    // TBB switch on meter_state[0xF2E] (current meter mode state, 0-8):
    //   case 0: if bit0 of r8 set -> go to AC mode handler
    //           else check bit3 -> auto-range
    //           set meter_state[0xF36] = 0
    //
    //   case 1: if status_byte bit0 == 0 -> set cal_coeff[0xF37] = 1
    //           else clear cal_coeff[0xF37]
    //
    //   case 2: if meter_state[0xF36] == 1:
    //               if status_byte bit0 -> overload handler
    //               else set cal_coeff = 1, advance to state 2
    //           else:
    //               check bit3 -> range change
    //
    //   case 3: check status_byte bit2 (AC/DC flag)
    //           check flags_byte bit6 (hold flag)
    //           set cal_coeff, advance state
    //
    //   case 4: check flags_byte bits 5,4 (range indicators)
    //           if bit5 -> set cal_coeff = 0, state = 4
    //           if bit4 -> set cal_coeff = 1, state = 4
    //           else check status_byte bit0 -> state selection
    //
    //   case 5: check meter_state[0xF39] (auto-range flag)
    //           set cal_coeff appropriately, state = 5
    //
    //   case 6: clear cal_coeff, go to state 6 (standby)
    //
    //   case 7: bit test pattern -> complex state transition
    //
    //   case 8: zero/reset pattern

    // After state processing, enqueue to meter_semaphore if fp != NULL
    if (meter_state[0xF2E] changed) {
        xQueueGenericSend(*meter_semaphore_handle, ...);
    }
}


// ============================================================================
// FUN_08037428: spi3_acquisition_task
// 0x08037428 - 0x08039050
// THE MAIN SPI3 ADC ACQUISITION LOOP
// This is the heart of the FPGA data interface
// ============================================================================
void spi3_acquisition_task(void) {
    // Register allocation:
    //   r7  = &SPI3_STAT (0x40003C08)   -- used as base for SPI3 register access
    //   sb  = &meter_state (0x200000F8)
    //   sl  = channel selector
    //   r8  = sample index
    //   fp  = various temp
    //
    // VFP registers preloaded with calibration constants:
    //   s16-s28 = gain, offset, scale factors for ADC calibration

    // Pre-load calibration from meter_state
    // s16 = offset CH1, s18 = gain CH1
    // s20 = offset CH2, s22 = gain CH2
    // s24 = max_value (clamp), s26 = min_value (clamp)
    // s28 = range_divisor

    while (1) {
        // Wait for trigger from queue
        xQueueReceive(*spi3_data_queue_handle, &trigger_byte, portMAX_DELAY);

        // Check meter_state[0x14] for transfer mode
        if (meter_state[0x14] != 0) {
            // === ACTIVE TRANSFER MODE ===
            // SPI3 transfer with chip select:

            // --- CS ASSERT ---
            GPIOB_BC = 0x40;  // PB6 LOW

            // Send command byte via SPI3
            spi3_xfer(command_byte);   // wait TXE, write, wait RXNE, read

            // --- CS DEASSERT ---
            GPIOB_BOP = 0x40; // PB6 HIGH
        } else {
            // === IDLE MODE ===
            // Send dummy byte 0x03
            // CS ASSERT
            spi3_xfer(0x03);
            // CS DEASSERT
        }

        // Read back status
        spi3_xfer(0x00);  // clock out status byte

        //
        // MAIN SAMPLE READ LOOP
        // TBH switch at 0x08037536 on (sample_count - 1), cases 0-8:
        //
        //   case 0: SCOPE_FAST_TIMEBASE
        //     Read timebase_sample_count_table[meter_state[0x2D]]
        //     If sample_count + 0x32 <= db8_counter -> fast capture complete
        //     Perform SPI3 bulk read: for each sample:
        //       CS_ASSERT
        //       spi3_xfer(command)  // send read command
        //       spi3_xfer(0x00)    // clock out high byte
        //       spi3_xfer(0x00)    // clock out low byte
        //       CS_DEASSERT
        //       // Assemble 16-bit sample, apply VFP calibration
        //       // Store to meter_state+0x5B0 (CH1) or +0x9B0 (CH2) buffer
        //
        //   case 1: SCOPE_ROLL_MODE
        //     Similar to case 0 but with circular buffer management
        //     Uses meter_state[0xDB4] as read pointer
        //     Increments meter_state[0xDB6] (sample count, max 0x12C = 300)
        //
        //   case 2: SCOPE_NORMAL
        //     Standard acquisition with pre/post trigger
        //     Calibration: for each raw sample byte from SPI3:
        //       calibrated = (float)(raw + offset) / range_divisor * (end - start) + start
        //       clamp(calibrated, 0, max_value)
        //       store as uint8_t
        //     Processes 2048 samples (0x800) with interleaved CH1/CH2
        //     CH1 buffer: meter_state+0x5B0
        //     CH2 buffer: meter_state+0x9B0
        //
        //   case 3: SCOPE_SINGLE_SHOT
        //     Like case 2 but stops after one acquisition
        //
        //   case 4: SCOPE_XY_MODE
        //     Reads both channels simultaneously
        //     No time-domain processing, just calibrate and store
        //
        //   case 5: METER_ADC_READ
        //     Reads ADC values for multimeter mode
        //     Simpler: just read N bytes via SPI3, no calibration
        //
        //   case 6: SIGGEN_FEEDBACK
        //     Signal generator output feedback monitoring
        //     Reads DAC output level via SPI3
        //
        //   case 7: CALIBRATION
        //     Internal calibration mode
        //     Reads reference voltages, computes gain/offset
        //
        //   case 8: SELF_TEST
        //     Self-test pattern readback
        //     Verifies FPGA communication integrity

        //
        // ADC CALIBRATION MATH (repeated per sample, VFP):
        //
        //   // Load raw byte from SPI3 buffer
        //   raw = spi3_read_byte();
        //
        //   // Apply calibration (single-precision float):
        //   float cal = ((float)raw + s16_offset) / s28_divisor;
        //   float result = cal * (float)(end - start) + (float)start + s22_bias;
        //
        //   // Clamp to [0, s24_max]:
        //   if (result > s24_max) result = s24_max;
        //   if (result < 0) result = s26_min;  // typically 0
        //
        //   // Store calibrated sample:
        //   sample_buf[index] = (uint8_t)result;
        //

    } // end while(1)
}


// ============================================================================
// 0x08039050 - 0x08039188: spi3_init_and_setup
// Initializes SPI3 peripheral and remaps JTAG pins for SPI3 use
// ============================================================================
void spi3_init_and_setup(void) {
    // Wait for trigger from spi3_data_queue
    xQueueReceive(*spi3_data_queue_handle, &mode, portMAX_DELAY);

    // Configure GPIOB pins for SPI3 (remapped from JTAG):
    //   PB3 = SPI3_SCK  (AF push-pull)
    //   PB4 = SPI3_MISO (input floating)
    //   PB5 = SPI3_MOSI (AF push-pull)
    //   PB6 = CS (GPIO output push-pull)

    gpio_init(GPIOB, &config_sck);   // PB3: AF_PP, 50MHz
    gpio_init(GPIOB, &config_miso);  // PB4: floating input
    gpio_init(GPIOB, &config_mosi);  // PB5: AF_PP, 50MHz
    gpio_init(GPIOB, &config_cs);    // PB6: GPIO output

    // CS deassert (HIGH)
    GPIOB_BOP = 0x40;

    // Configure SPI3:
    //   Master mode, CPOL=0, CPHA=0
    //   8-bit data frame
    //   MSB first
    //   Software NSS management
    //   Clock divider (sets baud rate)
    SPI3_CTL0 = ...;
    SPI3_CTL1 = ...;

    // Enable SPI3
    SPI3_CTL0 |= SPI_ENABLE;

    // Initial handshake with FPGA:
    //   CS_ASSERT
    //   spi3_xfer(0x00)  // dummy
    //   CS_DEASSERT
    //   vTaskDelay(10)
}


// ============================================================================
// 0x08039188 - 0x080396C6: input_and_housekeeping
// Button/probe input handling, watchdog feeding, timer management
// ============================================================================
void input_and_housekeeping(void) {
    // Configure GPIO pins for input scanning:
    gpio_init(GPIOB, ...);  // Multiple calls for different pin configs
    gpio_init(GPIOC, ...);

    // Build input state byte from GPIO reads:
    uint8_t input_state = 0;
    input_state |= (~GPIOC_ISTAT >> 7) & 1;   // PC7 (probe detect)
    input_state |= (GPIOB_ISTAT >> 4) & 2;     // PB5 state -> bit 1
    input_state |= (~GPIOB_CTL1[0x408] << 2) & 4;  // special register
    input_state |= (~GPIOC_ISTAT << 1) & 8;    // PC3/PC4 state

    // TBB switch on input_state (1-8), detects which probe/input changed:
    //   case 1: button_events |= 0x80    (CH1 probe)
    //   case 2: button_events |= 0x800   (CH2 probe)
    //   case 3: button_events |= 0x100   (probe type 1)
    //   case 4: button_events |= 0x200   (probe type 2)
    // Then similar for PB8 input:
    //   case 1: button_events |= 0x20    (button group 1)
    //   case 2: button_events |= 0x10    (button group 2)
    //   case 3: button_events |= 0x08    (button group 3)
    //   case 4: button_events |= 0x2000  (special)
    // And GPIOC IDR bit 10:
    //   case 1: button_events |= 0x1000  (trigger)
    //   case 2: button_events |= 0x04    (select)
    //   case 3: button_events |= 0x4000  (menu)
    //   case 4: button_events |= 0x02    (OK)

    // Debounce loop for 15 buttons (5 groups of 3):
    //   debounce_counters[0x20002D58 + i] tracks each button
    //   Threshold: 0x46 (70) ticks for press, 0x48 (72) for release
    //   On confirmed press: event_count++, store button ID in sp[0xA]
    //   Uses button_map_table (0x08046528) for mapping physical to logical IDs

    for (int group = 0; group < 5; group++) {
        for (int btn = 0; btn < 3; btn++) {
            uint16_t mask = (1 << (group*3 + btn));
            if (button_events & mask) {
                // Button pressed
                if (counter[btn] != 0xFF) counter[btn]++;
                if (counter[btn] == 0x46) {
                    // Press confirmed
                    event_count++;
                    button_id = button_map_table[group*3 + btn + 0x0F];
                }
                if (button_events_prev & mask) {
                    if (counter[btn] == 0x48) {
                        // Long press
                        event_count++;
                        button_id = button_map_table[group*3 + btn];
                        counter[btn]--;  // stay at 0x47
                    }
                }
            } else {
                // Button released
                if (counter[btn] >= 2 && counter[btn] <= 0x45) {
                    event_count++;
                    button_id = button_map_table[group*3 + btn];
                }
                counter[btn] = 0;
            }
        }
    }

    // If exactly 1 button event detected:
    if ((uint8_t)event_count == 1 && button_id != 0) {
        xQueueGenericSend(*button_event_queue_ptr, &button_id, 0);
    }

    // Watchdog management:
    //   IWDG_RLR (0x40015434) - if non-zero, increment counter
    //   If counter >= 11 (0x0B), reset watchdog
    if (IWDG_RLR != 0) {
        watchdog_counter++;
        if (watchdog_counter >= 0x0B) {
            IWDG_RLR = 0;  // reload
        }
    } else {
        watchdog_counter = 0;
    }

    // Housekeeping counters (all in meter_state):
    meter_state[0x3D]++;  // frame counter (wraps at 99)
    if (meter_state[0x3D] > 99) meter_state[0x3D] = 0;

    // Auto-power-off timer management:
    if (meter_state[0xE10] >= 2 && meter_state[0xE10] != 0xFF) {
        meter_state[0xE24]++;  // power-off tick counter
        if (meter_state[0xE24] >= 0x65) {  // 101 ticks
            meter_state[0xE10] = 0;  // disable auto-off
        }
    } else {
        meter_state[0xE24] = 0;
    }

    // Countdown timers:
    if (meter_state[0x32] != 0xFF) meter_state[0x32]++;
    if (meter_state[0x2A] != 0) meter_state[0x2A]--;  // display hold timer
    if (meter_state[0x28] != 0) meter_state[0x28]--;  // button repeat timer
    if (meter_state[0x2C] != 0) meter_state[0x2C]--;
    if (meter_state[0xDBB] != 0) meter_state[0xDBB]--;
    if (meter_state[0xDBA] != 0) meter_state[0xDBA]--;

    // Frequency/period measurement using TIM5:
    meter_state[0x45]++;  // measurement cycle counter
    if (meter_state[0x45] was 0) {
        TIM5_CNT = 0;       // reset timer at 0x40002424 (actually 0x40000C24)
        TIM5_CR1 = 0;       // also offset 0xC00 from base
    }
    if (meter_state[0x45] == 0x33) {  // 51 cycles
        if (meter_state[0x2E] != 0) {  // frequency measurement enabled
            // Read TIM5 counters for both channels
            uint32_t cnt1 = (uint32_t)TIM5_CH1_CNT;
            uint32_t cnt2 = (uint32_t)TIM5_CH2_CNT;

            // Convert to frequency: f = 1,000,000,000 / (2 * count)
            float fcnt1 = (float)cnt1;
            fcnt1 = fcnt1 * 2.0f;
            int32_t period1 = (int32_t)fcnt1;
            meter_state[0x50] = period1;
            int32_t freq1 = (period1 != 0) ? (1000000000 / period1) : 0;
            meter_state[0x54] = freq1;

            float fcnt2 = (float)cnt2;
            fcnt2 = fcnt2 * 2.0f;
            int32_t period2 = (int32_t)fcnt2;
            meter_state[0x80] = period2;
            int32_t freq2 = (period2 != 0) ? (1000000000 / period2) : 0;
            meter_state[0x84] = freq2;

            // Copy trigger state
            meter_state[0x230] = meter_state[0x231];
            meter_state[0x231] = 0;
        }
        meter_state[0x45] = 0;  // reset cycle counter
    }

    // Trigger/timebase readiness check:
    if (!(GPIOC_ISTAT & 1)) {  // bit 0 of GPIOC
        // FPGA indicates data ready
        if (meter_state[0x17] != 2) {  // not in hold mode
            if (meter_state[0x2D] <= 0x13) {  // valid timebase
                uint16_t sample_count = meter_state[0xDB8]++;
                uint8_t threshold = timebase_sample_count_table[meter_state[0x2D]] + 0x32;
                if (threshold == sample_count) {
                    // Acquisition complete - trigger SPI3 read
                    xQueueGenericSend(*spi3_data_queue_handle, &[1], portMAX_DELAY);
                    xQueueGenericSend(*spi3_data_queue_handle, &[1], portMAX_DELAY);
                    // (sent twice - double-buffered?)
                }
            }
        }
    } else {
        meter_state[0xDB8] = 0;  // reset sample counter
    }
}


// ============================================================================
// FUN_080396C8: probe_change_handler
// 0x080396C8 - 0x08039734
// Detects probe connection changes and handles auto power-off
// ============================================================================
void probe_change_handler(void) {
    input_handler();  // reads GPIOC, updates probe state

    // Save previous state, clear change flags
    meter_state[0xF6F] = meter_state[0xF70];
    meter_state[0xF70] = 0;
    meter_state[0xDB1] = meter_state[0xDB2];
    meter_state[0xDB2] = 0;

    uint8_t change = meter_state[0xF65];
    if (change == 0) return;

    // Increment change counter
    meter_state[0xF6C]++;

    // Auto power-off thresholds based on change type:
    //   change == 3: threshold = 0x0E10 (3600 = 1 hour)
    //   change == 2: threshold = 0x0708 (1800 = 30 min)
    //   change == 1: threshold = 0x0384 (900 = 15 min)
    if (change == 3 && counter > 0x0E10) goto power_off;
    if (change == 2 && counter > 0x0708) goto power_off;
    if (change == 1 && counter > 0x0384) goto power_off;
    return;

power_off:
    system_reset_or_power_off(0x55);  // trigger power down
}


// ============================================================================
// FUN_08039734: usart_tx_config_writer
// 0x08039734 - 0x08039870
// Writes USART TX configuration registers based on command type
// 7 command types via TBB switch
// ============================================================================
void usart_tx_config_writer(uint8_t *config_base, uint8_t *params) {
    uint8_t cmd_type = params[0];
    if (cmd_type > 6) return;

    // TBB switch at 0x0803973E on cmd_type (0-6):
    //
    //   case 0: scope_ch1_config
    //     config_base[0x20].bit1 = params[1].bit0    // AC/DC coupling
    //     config_base[0x20].bit3 = params[1].bit1    // bandwidth limit
    //     config_base[0x18] = (params[2] & 3) << 0   // voltage range low
    //     config_base[0x18] |= (params[3] & 0xF) << 12  // voltage range high
    //     r4 = 1  (channel mask)
    //
    //   case 1: scope_ch2_config
    //     Same structure as case 0 but with bit offsets:
    //     config_base[0x20].bit5 = params[1].bit0
    //     config_base[0x20].bit7 = params[1].bit1
    //     config_base[0x18] = (params[2] & 3) << 8
    //     config_base[0x18] |= (params[3] & 0xF) << 12
    //     r4 = 0x10
    //
    //   case 2: scope_trigger_config
    //     config_base[0x20].bit9 = params[1].bit0    // trigger edge
    //     config_base[0x20].bit11 = params[1].bit1   // trigger source
    //     config_base[0x1C] = trigger level settings
    //     r4 = 0x10
    //
    //   case 3: scope_timebase_config
    //     Direct register writes for timebase/sample rate
    //
    //   case 4: meter_range_config
    //     Meter range selection registers
    //
    //   case 5: siggen_freq_config
    //     Signal generator frequency registers
    //
    //   case 6: siggen_wave_config
    //     Signal generator waveform type registers
}
""")

        return lines

    def generate_spi3_transfer_detail(self, instructions):
        """Detail all SPI3 transfer locations."""
        lines = []
        lines.append("")
        lines.append("=" * 78)
        lines.append("  SPI3 TRANSFER LOCATIONS")
        lines.append("=" * 78)
        lines.append("")

        for i, xfer in enumerate(self.spi3_xfers):
            lines.append(f"  [{i:2d}] 0x{xfer['start']:08X} - 0x{xfer['end']:08X}  "
                        f"write_reg={xfer['write_reg']}")
        if not self.spi3_xfers:
            lines.append("  (SPI3 transfers detected via pattern in full disassembly below)")
        lines.append("")

        return lines

    def generate_cs_operations(self):
        lines = []
        lines.append("=" * 78)
        lines.append("  CS (CHIP SELECT) OPERATIONS")
        lines.append("=" * 78)
        lines.append("")
        for op_type, addr in self.cs_ops:
            lines.append(f"  0x{addr:08X}: CS_{op_type.upper()}")
        if not self.cs_ops:
            lines.append("  (CS ops detected via GPIOB_BC/BOP writes with value 0x40)")
        lines.append("")
        return lines

    def generate_queue_operations(self):
        lines = []
        lines.append("=" * 78)
        lines.append("  FREERTOS QUEUE OPERATIONS")
        lines.append("=" * 78)
        lines.append("")
        for addr, fname in self.queue_ops:
            lines.append(f"  0x{addr:08X}: {fname}()")
        lines.append("")
        return lines

    def generate_table_branches(self, instructions):
        lines = []
        lines.append("=" * 78)
        lines.append("  TABLE BRANCH DECODE")
        lines.append("=" * 78)
        lines.append("")

        for idx, insn in enumerate(instructions):
            if insn.mnemonic in ('tbh', 'tbb'):
                if insn.mnemonic == 'tbh':
                    targets = self.decode_tbh(insn.address, instructions, idx)
                else:
                    targets = self.decode_tbb(insn.address, instructions, idx)

                lines.append(f"  {insn.mnemonic.upper()} at 0x{insn.address:08X}:")
                for case_idx, target in targets:
                    lines.append(f"    case {case_idx}: -> 0x{target:08X}")
                lines.append("")

        return lines

    def generate_vfp_summary(self):
        lines = []
        lines.append("=" * 78)
        lines.append("  VFP (FLOATING-POINT) BLOCKS")
        lines.append("=" * 78)
        lines.append("")
        for start, end, count in self.vfp_blocks:
            lines.append(f"  0x{start:08X} - 0x{end:08X}  ({count} VFP instructions)")
        lines.append(f"  Total VFP blocks: {len(self.vfp_blocks)}")
        lines.append("")
        return lines

    def run(self):
        """Main decompilation pipeline."""
        print(f"Loading firmware: {FIRMWARE_PATH}")
        print(f"Disassembling 0x{FUNC_START:08X} - 0x{FUNC_END:08X} "
              f"({FUNC_END - FUNC_START} bytes)")

        instructions = self.disassemble_range(FUNC_START, FUNC_END)
        print(f"Disassembled {len(instructions)} instructions")

        # Analysis passes
        print("Pass 1: Resolving MOVW/MOVT pairs...")
        self.resolve_movw_movt(instructions)
        print(f"  Resolved {len(self.resolved_addrs)} address pairs")

        print("Pass 2: Identifying SPI3 transfer patterns...")
        self.identify_spi3_xfers(instructions)
        print(f"  Found {len(self.spi3_xfers)} SPI3 transfers")

        print("Pass 3: Identifying CS operations...")
        self.identify_cs_ops(instructions)
        print(f"  Found {len(self.cs_ops)} CS operations")

        print("Pass 4: Identifying queue operations...")
        self.identify_queue_ops(instructions)
        print(f"  Found {len(self.queue_ops)} queue operations")

        print("Pass 5: Identifying VFP blocks...")
        self.identify_vfp_blocks(instructions)
        print(f"  Found {len(self.vfp_blocks)} VFP blocks")

        print("Pass 6: Identifying sub-functions...")
        self.identify_subfunctions(instructions)
        print(f"  Found {len(self.subfunctions)} sub-function entries")

        # Generate output
        output_lines = []

        # Structure analysis
        output_lines.extend(self.generate_structure_analysis(instructions))

        # High-level pseudocode
        output_lines.extend(self.generate_pseudocode_blocks(instructions))

        # Table branch decode
        output_lines.extend(self.generate_table_branches(instructions))

        # SPI3 transfers
        output_lines.extend(self.generate_spi3_transfer_detail(instructions))

        # CS operations
        output_lines.extend(self.generate_cs_operations())

        # Queue operations
        output_lines.extend(self.generate_queue_operations())

        # VFP summary
        output_lines.extend(self.generate_vfp_summary())

        # Full annotated disassembly
        output_lines.append("")
        output_lines.append("=" * 78)
        output_lines.append("  FULL ANNOTATED DISASSEMBLY")
        output_lines.append("=" * 78)
        output_lines.append("")

        output_lines.extend(self.build_pseudocode(instructions))

        # Write output
        output_text = "\n".join(output_lines)
        os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
        with open(OUTPUT_PATH, 'w') as f:
            f.write(output_text)

        print(f"\nOutput written to: {OUTPUT_PATH}")
        print(f"Total lines: {len(output_lines)}")

        # Summary statistics
        print(f"\n{'='*60}")
        print("SUMMARY")
        print(f"{'='*60}")
        print(f"  Instructions disassembled: {len(instructions)}")
        print(f"  MOVW/MOVT pairs resolved:  {len(self.resolved_addrs)}")
        print(f"  SPI3 transfer patterns:    {len(self.spi3_xfers)}")
        print(f"  CS assert/deassert ops:    {len(self.cs_ops)}")
        print(f"  Queue operations:          {len(self.queue_ops)}")
        print(f"  VFP instruction blocks:    {len(self.vfp_blocks)}")
        print(f"  Sub-functions identified:  {len(self.subfunctions)}")
        print(f"  Table branches decoded:    {sum(1 for i in instructions if i.mnemonic in ('tbh','tbb'))}")


if __name__ == "__main__":
    with open(FIRMWARE_PATH, 'rb') as f:
        firmware = f.read()

    decompiler = FPGATaskDecompiler(firmware)
    decompiler.run()
