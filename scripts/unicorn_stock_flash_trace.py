#!/usr/bin/env python3
"""
Trace stock firmware early-init SPI2 flash transactions in Unicorn.

This is a software-only fallback for the "W25Q128 boot sniff" question when
physical probing is impractical. It executes the stock master-init function
under Unicorn, backs SPI2 reads with the real dumped W25Q128 image, and logs
flash transactions observed on PB12/PB13/PB14/PB15.
"""

from __future__ import annotations

import argparse
import struct
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn import UC_HOOK_MEM_UNMAPPED, UC_HOOK_MEM_WRITE, UC_MEM_READ_UNMAPPED
from unicorn import UC_MODE_MCLASS, UC_MODE_THUMB
from unicorn.arm_const import (
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_SP,
)


FLASH_BASE = 0x08000000
FLASH_SIZE = 1024 * 1024
RAM_BASE = 0x20000000
RAM_SIZE = 224 * 1024
PERIPH_BASE = 0x40000000
PERIPH_SIZE = 0x00100000
EXMC_BASE = 0x60000000
EXMC_SIZE = 0x00100000
SCB_BASE = 0xE0000000
SCB_SIZE = 0x00100000
XMC_BASE = 0xA0000000
XMC_SIZE = 0x00010000

GPIOB_SCR = 0x40010C10
GPIOB_CLR = 0x40010C14
PB12_MASK = 1 << 12

SPI2_CTRL1 = 0x40003800
SPI2_STS = 0x40003808
SPI2_DT = 0x4000380C
SPI3_STS = 0x40003C08
SPI3_DT = 0x40003C0C

USART2_STS = 0x40004400
SYSTICK_CTRL = 0xE000E010
SYSTICK_LOAD = 0xE000E014
SYSTICK_VAL = 0xE000E018
ADC1_CTRL2 = 0x40012408
GPIOC_ISTAT = 0x40011008

MASTER_INIT = 0x08023A50 | 1
FATFS_INIT = 0x0802E7BC | 1
RETURN_SENTINEL = 0x08FFFFF0

# Expensive display-oriented helpers we can safely skip for this trace.
# The goal here is external-flash traversal, not faithful LCD rendering.
SKIP_FUNCTIONS = {
    0x080022DC: "lcd_palette_init",
}

PREVIEW_DISPATCHER = 0x0800BCD4
PREVIEW_STATE = 0x20008350
PREVIEW_STATE_SIZE = 12
PREVIEW_FAST_LOOPS = {
    0x08036290: (0x08036304, 0x0C),
    0x080363E2: (0x08036456, 0x08),
    0x080364EC: (0x08036560, 0x04),
}

# Expensive in-line display/pacing blocks inside master_init that are safe to
# fast-forward when the goal is external-flash traversal.
SKIP_JUMPS = {
    0x0802450C: (0x080246E4, "lcd_init_blit_block_1"),
    0x08024A80: (0x08025024, "lcd_init_blit_block_2"),
    0x08024CDC: (0x08025024, "lcd_init_blit_block_2_tail"),
    0x08026EAE: (0x08026EB2, "timer3_init_call"),
}

DEFAULT_STOCK = Path("/Users/david/Desktop/osc/archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin")
DEFAULT_DUMP = Path("/Users/david/Desktop/osc/archive/w25q128_dump.bin")
DEFAULT_OUT = Path("/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_2026_04_08.txt")
DISPLAY_ALLOC_STATE = 0x20001070
DISPLAY_ALLOC_POOL = 0x20008000
DISPLAY_ALLOC_SIZE = 0x2BC00

FS_ROOT_STRINGS = {
    0x080BBA1F: b"2:\x00",
    0x080BBA22: b"3:\x00",
    0x080BB927: b"2:/\x00",
    0x080BB92B: b"3:/\x00",
}

FS_DIR_ROOT_STRINGS = {
    0x080BC18B: b"2:/Screenshot simple file\x00",
    0x080BC1A5: b"3:/System file\x00",
    0x080BC1B4: b"2:/Screenshot file\x00",
}

FS_LOGO_STRINGS = {
    0x080BBC58: b"2:/LOGO\x00",
}

FS_PROBE_9999_STRING = {
    0x080BC841: b"3:/System file/9999.bin\x00",
}

OVERLAY_FORMAT_STRINGS = {
    0x080BC859: b"2:/Screenshot simple file/%d.bin\x00",
    0x080BCAD2: b"2:/Screenshot file/%d.bmp\x00",
    0x080BCAE5: b"%d.bmp\x00",
}


def build_fs_string_table(profile: str = "full", probe_override: str | None = None) -> dict[int, bytes]:
    table: dict[int, bytes] = {}

    if profile == "roots_only":
        table.update(FS_ROOT_STRINGS)
    elif profile == "roots_plus_dirs":
        table.update(FS_ROOT_STRINGS)
        table.update(FS_DIR_ROOT_STRINGS)
    elif profile == "roots_dirs_logo":
        table.update(FS_ROOT_STRINGS)
        table.update(FS_DIR_ROOT_STRINGS)
        table.update(FS_LOGO_STRINGS)
    elif profile == "roots_dirs_probe_9999":
        table.update(FS_ROOT_STRINGS)
        table.update(FS_DIR_ROOT_STRINGS)
        table.update(FS_PROBE_9999_STRING)
    elif profile == "overlay_formats":
        table.update(OVERLAY_FORMAT_STRINGS)
    elif profile == "roots_dirs_overlay":
        table.update(FS_ROOT_STRINGS)
        table.update(FS_DIR_ROOT_STRINGS)
        table.update(OVERLAY_FORMAT_STRINGS)
    elif profile == "full":
        table.update(FS_ROOT_STRINGS)
        table.update(FS_DIR_ROOT_STRINGS)
        table.update(FS_LOGO_STRINGS)
        table.update(FS_PROBE_9999_STRING)
        table.update(OVERLAY_FORMAT_STRINGS)
    else:
        raise ValueError(f"unknown fs profile: {profile}")

    if probe_override is not None:
        table[0x080BC841] = probe_override.encode("ascii") + b"\x00"

    return table


SYNTH_HIGH_FLASH_FS_STRINGS = build_fs_string_table("full")


@dataclass
class SpiTransaction:
    start_pc: int
    mosi: list[int] = field(default_factory=list)
    miso: list[int] = field(default_factory=list)
    opcode: int | None = None
    addr: int | None = None
    read_len: int = 0


class Spi2Model:
    def __init__(self, dump_bytes: bytes):
        self.dump = bytearray(dump_bytes)
        self.cs_active = False
        self.current: SpiTransaction | None = None
        self.transactions: list[SpiTransaction] = []

        self.cmd: int | None = None
        self.addr_bytes: list[int] = []
        self.read_ptr: int | None = None
        self.read_dummy_pending = False
        self.program_data: list[int] = []
        self.jedec_idx = 0
        self.legacy_id_idx = 0
        self.last_rx = 0xFF
        self.write_enable = False
        self.page_programs: list[tuple[int, int]] = []
        self.sector_erases: list[int] = []
        self.total_program_bytes = 0

    def cs_assert(self, pc: int) -> None:
        if self.cs_active:
            return
        self.cs_active = True
        self.current = SpiTransaction(start_pc=pc)
        self.cmd = None
        self.addr_bytes = []
        self.read_ptr = None
        self.read_dummy_pending = False
        self.program_data = []
        self.jedec_idx = 0
        self.legacy_id_idx = 0
        self.last_rx = 0xFF

    def cs_deassert(self) -> None:
        if not self.cs_active:
            return
        self.cs_active = False
        if self.current is not None:
            self._finalize_transaction()
            if self.current.opcode is None and self.current.mosi:
                self.current.opcode = self.current.mosi[0]
            self.transactions.append(self.current)
        self.current = None
        self.cmd = None
        self.addr_bytes = []
        self.read_ptr = None
        self.read_dummy_pending = False
        self.program_data = []
        self.jedec_idx = 0
        self.last_rx = 0xFF

    def _dump_byte(self, addr: int) -> int:
        if 0 <= addr < len(self.dump):
            return self.dump[addr]
        return 0xFF

    def _finalize_transaction(self) -> None:
        if self.current is None or self.cmd is None or self.current.addr is None:
            return

        if self.cmd == 0x20 and self.write_enable:
            sector_base = self.current.addr & ~0xFFF
            sector_end = min(sector_base + 0x1000, len(self.dump))
            for idx in range(sector_base, sector_end):
                self.dump[idx] = 0xFF
            self.sector_erases.append(sector_base)
            self.write_enable = False
        elif self.cmd == 0x02 and self.write_enable and self.program_data:
            page_base = self.current.addr & ~0xFF
            page_offset = self.current.addr & 0xFF
            for idx, byte in enumerate(self.program_data):
                addr = page_base + ((page_offset + idx) & 0xFF)
                if 0 <= addr < len(self.dump):
                    # NOR page-program can only clear bits without an erase.
                    self.dump[addr] &= byte
            self.page_programs.append((self.current.addr, len(self.program_data)))
            self.total_program_bytes += len(self.program_data)
            self.write_enable = False

    def transceive(self, tx: int) -> int:
        if not self.cs_active or self.current is None:
            self.last_rx = 0xFF
            return self.last_rx

        self.current.mosi.append(tx & 0xFF)
        rx = 0xFF

        if self.cmd is None:
            self.cmd = tx & 0xFF
            self.current.opcode = self.cmd
            if self.cmd in (0x02, 0x03, 0x0B, 0x20, 0x90):
                self.addr_bytes = []
                self.read_ptr = None
                self.read_dummy_pending = False
                self.program_data = []
                rx = 0x00
            elif self.cmd == 0x9F:
                self.jedec_idx = 0
                rx = 0x00
            elif self.cmd == 0x06:
                self.write_enable = True
                rx = 0x00
            elif self.cmd == 0x04:
                self.write_enable = False
                rx = 0x00
            elif self.cmd == 0x05:
                rx = 0x02 if self.write_enable else 0x00
            else:
                rx = 0x00
        elif self.cmd in (0x02, 0x03, 0x0B, 0x20, 0x90) and self.read_ptr is None and self.current.addr is None:
            self.addr_bytes.append(tx & 0xFF)
            if len(self.addr_bytes) == 3:
                if self.cmd == 0x90:
                    self.current.addr = 0
                    self.legacy_id_idx = 0
                elif self.cmd in (0x02, 0x20):
                    self.current.addr = (
                        (self.addr_bytes[0] << 16)
                        | (self.addr_bytes[1] << 8)
                        | self.addr_bytes[2]
                    )
                else:
                    self.read_ptr = (
                        (self.addr_bytes[0] << 16)
                        | (self.addr_bytes[1] << 8)
                        | self.addr_bytes[2]
                    )
                    self.current.addr = self.read_ptr
                    if self.cmd == 0x0B:
                        self.read_dummy_pending = True
            rx = 0x00
        elif self.cmd == 0x9F:
            jedec = [0xEF, 0x40, 0x18]
            rx = jedec[self.jedec_idx] if self.jedec_idx < len(jedec) else 0xFF
            self.jedec_idx += 1
        elif self.cmd == 0x90:
            legacy = [0xEF, 0x18]
            rx = legacy[self.legacy_id_idx] if self.legacy_id_idx < len(legacy) else 0xFF
            self.legacy_id_idx += 1
        elif self.cmd == 0x05:
            rx = 0x02 if self.write_enable else 0x00
        elif self.cmd == 0x02 and self.current.addr is not None:
            self.program_data.append(tx & 0xFF)
            rx = 0x00
        elif self.cmd in (0x03, 0x0B) and self.read_ptr is not None:
            if self.read_dummy_pending:
                self.read_dummy_pending = False
                rx = 0x00
            else:
                rx = self._dump_byte(self.read_ptr)
                self.read_ptr += 1
                self.current.read_len += 1
        else:
            rx = 0xFF

        self.current.miso.append(rx)
        self.last_rx = rx
        return rx


class StockFlashTracer:
    def __init__(
        self,
        binary_path: Path,
        dump_path: Path,
        entry: int = MASTER_INIT,
        seed_display_alloc: bool = False,
        seed_high_flash_fs: bool = False,
        high_flash_fs_strings: dict[int, bytes] | None = None,
        fs_profile: str = "full",
        fs_probe_override: str | None = None,
        arg_regs: dict[int, int] | None = None,
        seed_u8: dict[int, int] | None = None,
        seed_u32: dict[int, int] | None = None,
        seed_cstr: dict[int, bytes] | None = None,
        second_entry: int | None = None,
        second_arg_regs: dict[int, int] | None = None,
        third_entry: int | None = None,
        third_arg_regs: dict[int, int] | None = None,
        max_instructions: int = 50_000_000,
    ):
        self.binary_path = binary_path
        self.dump_path = dump_path
        self.entry = entry | 1
        self.seed_display_alloc = seed_display_alloc
        self.seed_high_flash_fs = seed_high_flash_fs
        self.fs_profile = fs_profile
        self.fs_probe_override = fs_probe_override
        self.arg_regs = dict(arg_regs or {})
        self.seed_u8 = dict(seed_u8 or {})
        self.seed_u32 = dict(seed_u32 or {})
        self.seed_cstr = dict(seed_cstr or {})
        self.second_entry = (second_entry | 1) if second_entry is not None else None
        self.second_arg_regs = dict(second_arg_regs or {})
        self.third_entry = (third_entry | 1) if third_entry is not None else None
        self.third_arg_regs = dict(third_arg_regs or {})
        self.max_instructions = max_instructions

        self.binary = binary_path.read_bytes()
        self.dump = dump_path.read_bytes()
        if high_flash_fs_strings is None:
            self.high_flash_fs_strings = build_fs_string_table(fs_profile, fs_probe_override)
        else:
            self.high_flash_fs_strings = dict(high_flash_fs_strings)

        self.mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.periph_mem: dict[int, tuple[int, int]] = {}
        self.instructions = 0
        self.systick_skips = 0
        self.stop_pc = 0
        self.last_pc = 0
        self.last_lr = 0
        self.run_error: str | None = None
        self.skipped_calls: Counter[str] = Counter()
        self.skipped_blocks: Counter[str] = Counter()
        self.spi2 = Spi2Model(self.dump)
        self.stop_r0 = 0
        self.stage = 0
        self.stages: list[tuple[int, dict[int, int]]] = [(self.entry, self.arg_regs)]
        if self.second_entry is not None:
            self.stages.append((self.second_entry, self.second_arg_regs))
        if self.third_entry is not None:
            self.stages.append((self.third_entry, self.third_arg_regs))

    def periph_write(self, addr: int, value: int, size: int) -> None:
        self.periph_mem[addr] = (value, size)
        if size == 4:
            data = struct.pack("<I", value & 0xFFFFFFFF)
        elif size == 2:
            data = struct.pack("<H", value & 0xFFFF)
        else:
            data = struct.pack("<B", value & 0xFF)
        try:
            self.mu.mem_write(addr, data)
        except UcError:
            pass

    def setup(self) -> None:
        self.mu.mem_map(FLASH_BASE, FLASH_SIZE)
        self.mu.mem_map(RAM_BASE, RAM_SIZE)
        self.mu.mem_map(PERIPH_BASE, PERIPH_SIZE)
        self.mu.mem_map(EXMC_BASE, EXMC_SIZE)
        self.mu.mem_map(SCB_BASE, SCB_SIZE)
        self.mu.mem_map(XMC_BASE, XMC_SIZE)

        self.mu.mem_write(FLASH_BASE, self.binary)
        self.mu.mem_write(RAM_BASE, b"\x00" * RAM_SIZE)
        self.mu.reg_write(UC_ARM_REG_SP, RAM_BASE + RAM_SIZE)

        if self.seed_display_alloc:
            self._seed_display_allocator()
        if self.seed_high_flash_fs:
            self._seed_high_flash_fs()
        self._seed_manual_memory()

        self.periph_write(0x40021014, 0x00000114, 4)
        self.periph_write(0x40021018, 0x00007E7D, 4)
        self.periph_write(0x4002101C, 0x1802B81F, 4)
        self.periph_write(0x40021004, 0x001D8402, 4)

        self.periph_write(SYSTICK_LOAD, 240000 - 1, 4)
        self.periph_write(SYSTICK_VAL, 0, 4)
        self.periph_write(0xE000ED0C, 0xFA050000, 4)

        for port_base in [0x40010800, 0x40010C00, 0x40011000, 0x40011400, 0x40011800]:
            self.periph_write(port_base + 0x00, 0x44444444, 4)
            self.periph_write(port_base + 0x04, 0x44444444, 4)
        self.periph_write(GPIOC_ISTAT, 1 << 8, 4)

        self.periph_write(SPI2_STS, 0x0003, 4)
        self.periph_write(SPI2_DT, 0xFF, 4)
        self.periph_write(SPI3_STS, 0x0003, 4)
        self.periph_write(SPI3_DT, 0xFF, 4)
        self.periph_write(USART2_STS, 0x00C0, 4)
        self.periph_write(ADC1_CTRL2, 0x00000000, 4)

        self.mu.mem_write(0x20002B1C, struct.pack("<I", 2400))
        self.mu.mem_write(0x20002B20, struct.pack("<I", 240))

        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write, begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write, begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read, begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read, begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)
        self.mu.hook_add(UC_HOOK_CODE, self._hook_code)
        self.mu.hook_add(UC_HOOK_MEM_UNMAPPED, self._hook_unmapped)

    def _hook_mem_write(self, uc, access, address, size, value, user_data):
        stored_value = value

        if address == GPIOB_CLR and (value & PB12_MASK):
            pc = uc.reg_read(UC_ARM_REG_PC)
            self.spi2.cs_assert(pc)
        elif address == GPIOB_SCR and (value & PB12_MASK):
            self.spi2.cs_deassert()

        if address == SPI2_DT:
            rx = self.spi2.transceive(value & 0xFF)
            uc.mem_write(SPI2_DT, struct.pack("<I", rx))
            uc.mem_write(SPI2_STS, struct.pack("<I", 0x0003))

        if address == SPI3_DT:
            uc.mem_write(SPI3_DT, struct.pack("<I", 0xFF))
            uc.mem_write(SPI3_STS, struct.pack("<I", 0x0003))

        if address == ADC1_CTRL2:
            cleared = value & ~((1 << 2) | (1 << 3))
            uc.mem_write(ADC1_CTRL2, struct.pack("<I", cleared))
            stored_value = cleared

        if address == SYSTICK_CTRL and (value & 1):
            current = value | (1 << 16)
            uc.mem_write(SYSTICK_CTRL, struct.pack("<I", current))
            self.systick_skips += 1

        if address == USART2_STS - 0x04 + 0x04:  # USART2_DT write
            uc.mem_write(USART2_STS, struct.pack("<I", 0x00C0))

        self.periph_mem[address] = (stored_value, size)

    def _hook_mem_read(self, uc, access, address, size, value, user_data):
        if address == SYSTICK_CTRL:
            uc.mem_write(address, struct.pack("<I", (1 << 16) | 0x05))
            return

        if address == SPI2_STS:
            uc.mem_write(address, struct.pack("<I", 0x0003))
            return

        if address == SPI2_DT:
            uc.mem_write(address, struct.pack("<I", self.spi2.last_rx))
            return

        if address == SPI3_STS:
            uc.mem_write(address, struct.pack("<I", 0x0003))
            return

        if address == SPI3_DT:
            uc.mem_write(address, struct.pack("<I", 0xFF))
            return

        if address == USART2_STS:
            uc.mem_write(address, struct.pack("<I", 0x00C0))
            return

        if address == GPIOC_ISTAT:
            uc.mem_write(address, struct.pack("<I", 1 << 8))
            return

        if address in self.periph_mem:
            val, _ = self.periph_mem[address]
            if size == 4:
                uc.mem_write(address, struct.pack("<I", val & 0xFFFFFFFF))
            elif size == 2:
                uc.mem_write(address, struct.pack("<H", val & 0xFFFF))
            else:
                uc.mem_write(address, struct.pack("<B", val & 0xFF))

    def _stub_preview_dispatcher(self, uc) -> bool:
        # `FUN_0800BCD4` is a jump-table preview dispatcher used by
        # `FUN_08036084()`. During chained BMP-side experiments we only need it
        # to leave a deterministic framebuffer behind so the later FAT write
        # path can proceed.
        if self.stage == 0:
            return False

        viewport = self.mu.mem_read(PREVIEW_STATE, PREVIEW_STATE_SIZE)
        viewport_x, viewport_y, width, height, framebuffer_ptr = struct.unpack("<HHHHI", viewport)
        pixel_count = width * height
        byte_count = pixel_count * 2

        if pixel_count == 0 or byte_count > 0x40000:
            return False
        if framebuffer_ptr < RAM_BASE or framebuffer_ptr + byte_count > RAM_BASE + RAM_SIZE:
            return False

        code = uc.reg_read(UC_ARM_REG_R0) & 0xFF
        pixels = bytearray(byte_count)
        for idx in range(pixel_count):
            x = idx % width
            y = idx // width
            red = ((x + viewport_x + code * 3) >> 3) & 0x1F
            green = ((y + viewport_y + code * 5) >> 2) & 0x3F
            blue = ((x ^ y ^ code) >> 1) & 0x1F
            pixel = (red << 11) | (green << 5) | blue
            struct.pack_into("<H", pixels, idx * 2, pixel)

        self.mu.mem_write(framebuffer_ptr, bytes(pixels))
        self.skipped_calls["preview_dispatcher_stage_stub"] += 1
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
        return True

    def _fast_forward_preview_loop(self, uc, address: int) -> bool:
        if self.stage == 0:
            return False

        jump = PREVIEW_FAST_LOOPS.get(address & ~1)
        if jump is None:
            return False

        target, sp_offset = jump
        viewport = self.mu.mem_read(PREVIEW_STATE, PREVIEW_STATE_SIZE)
        _, _, width, height, _ = struct.unpack("<HHHHI", viewport)
        pixel_count = width * height

        sp = uc.reg_read(UC_ARM_REG_SP)
        self.mu.mem_write(sp + sp_offset, struct.pack("<H", pixel_count & 0xFFFF))
        self.skipped_blocks[f"preview_loop_{address & ~1:08x}"] += 1
        uc.reg_write(UC_ARM_REG_PC, target | 1)
        return True

    def _hook_code(self, uc, address, size, user_data):
        self.last_pc = address
        self.last_lr = uc.reg_read(UC_ARM_REG_LR)

        jump = SKIP_JUMPS.get(address & ~1)
        if jump is not None:
            target, name = jump
            self.skipped_blocks[name] += 1
            uc.reg_write(UC_ARM_REG_PC, target | 1)
            return

        if (address & ~1) == PREVIEW_DISPATCHER and self._stub_preview_dispatcher(uc):
            return

        if self._fast_forward_preview_loop(uc, address):
            return

        func = SKIP_FUNCTIONS.get(address & ~1)
        if func is not None:
            lr = uc.reg_read(UC_ARM_REG_LR)
            self.skipped_calls[func] += 1
            uc.reg_write(UC_ARM_REG_PC, lr)
            return

        if (address & ~1) == RETURN_SENTINEL:
            if self.stage + 1 < len(self.stages):
                self.stage += 1
                next_entry, next_arg_regs = self.stages[self.stage]
                uc.reg_write(UC_ARM_REG_LR, RETURN_SENTINEL | 1)
                reg_map = {
                    0: UC_ARM_REG_R0,
                    1: UC_ARM_REG_R1,
                    2: UC_ARM_REG_R2,
                    3: UC_ARM_REG_R3,
                }
                for reg_idx, reg_value in next_arg_regs.items():
                    if reg_idx in reg_map:
                        uc.reg_write(reg_map[reg_idx], reg_value)
                uc.reg_write(UC_ARM_REG_PC, next_entry)
                return
            uc.emu_stop()
            return

        self.instructions += 1
        if self.instructions > self.max_instructions:
            uc.emu_stop()

    def _hook_unmapped(self, uc, access, address, size, value, user_data):
        page_base = address & ~0xFFF
        try:
            uc.mem_map(page_base, 0x1000)
            uc.mem_write(page_base, b"\x00" * 0x1000)
            return True
        except UcError:
            return False

    def _seed_display_allocator(self) -> None:
        # `memory_alloc()` at 0x08033CFC expects a display-buffer pool descriptor
        # at 0x20001070: callback @ +0, pool base @ +8, allocation table @ +12,
        # and "loaded" flag @ +16. For direct function-entry experiments we can
        # seed a synthetic empty pool so the allocator path becomes usable.
        state = bytearray(0x20)
        struct.pack_into("<I", state, 0x00, 0)
        struct.pack_into("<I", state, 0x08, DISPLAY_ALLOC_POOL)
        struct.pack_into("<I", state, 0x0C, DISPLAY_ALLOC_POOL)
        struct.pack_into("<B", state, 0x10, 1)
        self.mu.mem_write(DISPLAY_ALLOC_STATE, bytes(state))
        self.mu.mem_write(DISPLAY_ALLOC_POOL, b"\x00" * DISPLAY_ALLOC_SIZE)

    def _seed_high_flash_fs(self) -> None:
        for address, data in self.high_flash_fs_strings.items():
            self.mu.mem_write(address, data)

    def _seed_manual_memory(self) -> None:
        for address, value in self.seed_u8.items():
            self.mu.mem_write(address, struct.pack("<B", value & 0xFF))
        for address, value in self.seed_u32.items():
            self.mu.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))
        for address, data in self.seed_cstr.items():
            self.mu.mem_write(address, data)

    def run(self) -> None:
        self.mu.reg_write(UC_ARM_REG_LR, RETURN_SENTINEL | 1)
        reg_map = {
            0: UC_ARM_REG_R0,
            1: UC_ARM_REG_R1,
            2: UC_ARM_REG_R2,
            3: UC_ARM_REG_R3,
        }
        for reg_idx, reg_value in self.arg_regs.items():
            if reg_idx in reg_map:
                self.mu.reg_write(reg_map[reg_idx], reg_value)
        try:
            self.mu.emu_start(self.entry, 0, timeout=0)
        except UcError as exc:
            self.run_error = str(exc)
        self.stop_pc = self.mu.reg_read(UC_ARM_REG_PC)
        self.stop_r0 = self.mu.reg_read(UC_ARM_REG_R0)
        self.spi2.cs_deassert()

    def render_report(self) -> str:
        txns = self.spi2.transactions
        counts = Counter(t.opcode for t in txns if t.opcode is not None)

        lines: list[str] = []
        lines.append("Stock flash trace via Unicorn")
        lines.append(f"binary: {self.binary_path}")
        lines.append(f"dump:   {self.dump_path}")
        lines.append(f"entry:  0x{self.entry:08X}")
        lines.append(f"stages: {[f'0x{entry:08X}' for entry, _ in self.stages]}")
        lines.append(f"seed_display_alloc: {self.seed_display_alloc}")
        lines.append(f"seed_high_flash_fs: {self.seed_high_flash_fs}")
        lines.append(f"fs_profile: {self.fs_profile}")
        if self.fs_probe_override is not None:
            lines.append(f"fs_probe_override: {self.fs_probe_override!r}")
        if self.arg_regs:
            lines.append(f"arg_regs: {self.arg_regs}")
        if self.second_entry is not None:
            lines.append(f"second_entry: 0x{self.second_entry:08X}")
        if self.second_arg_regs:
            lines.append(f"second_arg_regs: {self.second_arg_regs}")
        if self.third_entry is not None:
            lines.append(f"third_entry: 0x{self.third_entry:08X}")
        if self.third_arg_regs:
            lines.append(f"third_arg_regs: {self.third_arg_regs}")
        if self.seed_u8:
            lines.append(f"seed_u8: {self.seed_u8}")
        if self.seed_u32:
            lines.append(f"seed_u32: {self.seed_u32}")
        if self.seed_cstr:
            seeded = {hex(addr): data.decode('ascii', 'replace').rstrip(chr(0)) for addr, data in self.seed_cstr.items()}
            lines.append(f"seed_cstr: {seeded}")
        if self.seed_high_flash_fs:
            lines.append(f"seeded_fs_entries: {len(self.high_flash_fs_strings)}")
        lines.append(f"instructions: {self.instructions}")
        lines.append(f"systick_skips: {self.systick_skips}")
        lines.append(f"completed_stages: {self.stage + 1}/{len(self.stages)}")
        lines.append(f"stop_pc: 0x{self.stop_pc:08X}")
        lines.append(f"stop_r0: 0x{self.stop_r0:08X}")
        lines.append(f"last_pc: 0x{self.last_pc:08X}")
        lines.append(f"last_lr: 0x{self.last_lr:08X}")
        if self.run_error is not None:
            lines.append(f"run_error: {self.run_error}")
        lines.append(f"transactions: {len(txns)}")
        lines.append(f"page_programs: {len(self.spi2.page_programs)}")
        lines.append(f"sector_erases: {len(self.spi2.sector_erases)}")
        lines.append(f"programmed_bytes: {self.spi2.total_program_bytes}")
        if self.skipped_calls:
            lines.append("skipped_calls:")
            for name, count in sorted(self.skipped_calls.items()):
                lines.append(f"  {name}: {count}")
        if self.skipped_blocks:
            lines.append("skipped_blocks:")
            for name, count in sorted(self.skipped_blocks.items()):
                lines.append(f"  {name}: {count}")
        lines.append("")
        lines.append("Opcode summary:")
        if counts:
            for opcode, count in sorted(counts.items()):
                lines.append(f"  0x{opcode:02X}: {count}")
        else:
            lines.append("  <none>")

        lines.append("")
        lines.append("First 40 transactions:")
        for i, txn in enumerate(txns[:40]):
            opcode = "??" if txn.opcode is None else f"{txn.opcode:02X}"
            addr = "--------" if txn.addr is None else f"{txn.addr:06X}"
            preview = " ".join(f"{b:02X}" for b in txn.mosi[:12])
            if len(txn.mosi) > 12:
                preview += " ..."
            lines.append(
                f"{i:02d}: pc=0x{txn.start_pc:08X} op=0x{opcode} addr=0x{addr} "
                f"read_len={txn.read_len} mosi=[{preview}]"
            )

        read_txns = [t for t in txns if t.opcode in (0x03, 0x0B) and t.addr is not None]
        if read_txns:
            lines.append("")
            lines.append("Read transactions:")
            for i, txn in enumerate(read_txns[:40]):
                end_addr = txn.addr + max(txn.read_len - 1, 0)
                lines.append(
                    f"{i:02d}: op=0x{txn.opcode:02X} start=0x{txn.addr:06X} "
                    f"len={txn.read_len} end=0x{end_addr:06X}"
                )

        if self.spi2.sector_erases:
            lines.append("")
            lines.append("Sector erases:")
            for i, addr in enumerate(self.spi2.sector_erases[:20]):
                lines.append(f"{i:02d}: 0x{addr:06X}")

        if self.spi2.page_programs:
            lines.append("")
            lines.append("Page programs:")
            for i, (addr, length) in enumerate(self.spi2.page_programs[:40]):
                lines.append(f"{i:02d}: start=0x{addr:06X} len={length}")

        return "\n".join(lines) + "\n"


def main() -> int:
    def parse_int(value: str) -> int:
        return int(value, 0)

    def parse_kv_int(value: str) -> tuple[int, int]:
        addr_s, val_s = value.split("=", 1)
        return int(addr_s, 0), int(val_s, 0)

    def parse_kv_cstr(value: str) -> tuple[int, bytes]:
        addr_s, text = value.split("=", 1)
        return int(addr_s, 0), text.encode("ascii") + b"\x00"

    parser = argparse.ArgumentParser(description="Trace stock firmware SPI2 flash accesses in Unicorn")
    parser.add_argument("--binary", type=Path, default=DEFAULT_STOCK)
    parser.add_argument("--dump", type=Path, default=DEFAULT_DUMP)
    parser.add_argument("--dump-out", type=Path, help="optional path to save the mutated in-memory SPI flash image after emulation")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--entry", type=parse_int, default=MASTER_INIT)
    parser.add_argument("--seed-display-alloc", action="store_true")
    parser.add_argument("--seed-high-flash-fs", action="store_true")
    parser.add_argument(
        "--fs-profile",
        choices=[
            "full",
            "roots_only",
            "roots_plus_dirs",
            "roots_dirs_logo",
            "roots_dirs_probe_9999",
            "overlay_formats",
            "roots_dirs_overlay",
        ],
        default="full",
        help="synthetic high-flash filesystem descriptor profile to seed when --seed-high-flash-fs is active",
    )
    parser.add_argument(
        "--fs-probe",
        help="override the synthetic 0x080BC841 probe string when --seed-high-flash-fs is active",
    )
    parser.add_argument("--arg0", type=parse_int)
    parser.add_argument("--arg1", type=parse_int)
    parser.add_argument("--arg2", type=parse_int)
    parser.add_argument("--arg3", type=parse_int)
    parser.add_argument("--entry2", type=parse_int, help="second function entry to jump to after the first returns")
    parser.add_argument("--entry2-arg0", type=parse_int)
    parser.add_argument("--entry2-arg1", type=parse_int)
    parser.add_argument("--entry2-arg2", type=parse_int)
    parser.add_argument("--entry2-arg3", type=parse_int)
    parser.add_argument("--entry3", type=parse_int, help="third function entry to jump to after the second returns")
    parser.add_argument("--entry3-arg0", type=parse_int)
    parser.add_argument("--entry3-arg1", type=parse_int)
    parser.add_argument("--entry3-arg2", type=parse_int)
    parser.add_argument("--entry3-arg3", type=parse_int)
    parser.add_argument("--seed-u8", action="append", default=[], help="seed byte memory as ADDR=VALUE")
    parser.add_argument("--seed-u32", action="append", default=[], help="seed 32-bit memory as ADDR=VALUE")
    parser.add_argument("--seed-cstr", action="append", default=[], help="seed C string memory as ADDR=ASCII_TEXT")
    parser.add_argument("--max-instructions", type=int, default=50_000_000)
    args = parser.parse_args()

    arg_regs = {}
    for idx, value in enumerate([args.arg0, args.arg1, args.arg2, args.arg3]):
        if value is not None:
            arg_regs[idx] = value
    second_arg_regs = {}
    for idx, value in enumerate([args.entry2_arg0, args.entry2_arg1, args.entry2_arg2, args.entry2_arg3]):
        if value is not None:
            second_arg_regs[idx] = value
    third_arg_regs = {}
    for idx, value in enumerate([args.entry3_arg0, args.entry3_arg1, args.entry3_arg2, args.entry3_arg3]):
        if value is not None:
            third_arg_regs[idx] = value

    seed_u8 = dict(parse_kv_int(item) for item in args.seed_u8)
    seed_u32 = dict(parse_kv_int(item) for item in args.seed_u32)
    seed_cstr = dict(parse_kv_cstr(item) for item in args.seed_cstr)

    tracer = StockFlashTracer(
        args.binary,
        args.dump,
        entry=args.entry,
        seed_display_alloc=args.seed_display_alloc,
        seed_high_flash_fs=args.seed_high_flash_fs,
        fs_profile=args.fs_profile,
        fs_probe_override=args.fs_probe,
        arg_regs=arg_regs,
        seed_u8=seed_u8,
        seed_u32=seed_u32,
        seed_cstr=seed_cstr,
        second_entry=args.entry2,
        second_arg_regs=second_arg_regs,
        third_entry=args.entry3,
        third_arg_regs=third_arg_regs,
        max_instructions=args.max_instructions,
    )
    tracer.setup()
    tracer.run()
    report = tracer.render_report()

    if args.dump_out is not None:
        args.dump_out.parent.mkdir(parents=True, exist_ok=True)
        args.dump_out.write_bytes(bytes(tracer.spi2.dump))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(report)
    print(report, end="")
    if args.dump_out is not None:
        print(f"saved dump: {args.dump_out}")
    print(f"saved: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
