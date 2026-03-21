#!/usr/bin/env python3
"""
FNIRSI 2C53T Emulator
Emulates the GD32F307 MCU running the oscilloscope firmware.
Uses Unicorn Engine for ARM Cortex-M4 CPU emulation.
"""

import struct
import sys
import os
import json
import time
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
from unicorn import *
from unicorn.arm_const import *
from capstone import *

# ─── Memory Map ──────────────────────────────────────────────────────

FLASH_BASE   = 0x08000000
FLASH_SIZE   = 1024 * 1024       # 1MB
RAM_BASE     = 0x20000000
RAM_SIZE     = 256 * 1024        # 256KB
PERIPH_BASE  = 0x40000000
PERIPH_SIZE  = 0x00100000        # 1MB peripheral region
EXMC_BASE    = 0x60000000        # LCD memory-mapped region
EXMC_SIZE    = 0x00100000
SCB_BASE     = 0xE0000000        # System control block
SCB_SIZE     = 0x00100000
FSMC_REG_BASE = 0xA0000000       # FSMC control registers
FSMC_REG_SIZE = 0x00010000

# ─── GD32F307 Peripheral Addresses ──────────────────────────────────

# Timers
TIM2_BASE   = 0x40000000
TIM3_BASE   = 0x40000400
TIM4_BASE   = 0x40000800
TIM5_BASE   = 0x40000C00
TIM6_BASE   = 0x40001000
TIM7_BASE   = 0x40001400

# SPI
SPI1_BASE   = 0x40003400
SPI2_BASE   = 0x40003800

# USART
USART1_BASE = 0x40004000
USART2_BASE = 0x40004400

# I2C
I2C0_BASE   = 0x40005400
I2C1_BASE   = 0x40005800

# USB
USB_BASE    = 0x40005C00
USB_SRAM    = 0x40006000

# CAN
CAN0_BASE   = 0x40006400
CAN1_BASE   = 0x40006800

# DAC
DAC_BASE    = 0x40007000

# GPIO
AFIO_BASE   = 0x40010000
GPIOA_BASE  = 0x40010800
GPIOB_BASE  = 0x40010C00
GPIOC_BASE  = 0x40011000
GPIOD_BASE  = 0x40011400
GPIOE_BASE  = 0x40011800

# SPI0
SPI0_BASE   = 0x40013800

# USART0
USART0_BASE = 0x40013C00

# Clock
RCU_BASE    = 0x40021000

# NVIC / System
NVIC_BASE   = 0xE000E000
SCB_VTOR    = 0xE000ED08
SCB_AIRCR   = 0xE000ED0C
SCB_CPACR   = 0xE000ED88
SYSTICK_BASE = 0xE000E010

# ─── LCD State ───────────────────────────────────────────────────────

LCD_WIDTH  = 320
LCD_HEIGHT = 240

class LCDController:
    """Emulates the LCD display connected via EXMC/FSMC"""
    def __init__(self):
        self.framebuffer = bytearray(LCD_WIDTH * LCD_HEIGHT * 2)  # RGB565
        self.cmd_register = 0
        self.x_pos = 0
        self.y_pos = 0
        self.x_start = 0
        self.x_end = LCD_WIDTH - 1
        self.y_start = 0
        self.y_end = LCD_HEIGHT - 1
        self.writing_data = False
        self.pixel_count = 0
        self.dirty = True

    def write_command(self, cmd):
        """LCD command register write (RS=0)"""
        self.cmd_register = cmd & 0xFF
        self.writing_data = False

        if cmd == 0x2C:  # Memory Write - start accepting pixel data
            self.writing_data = True
            self.x_pos = self.x_start
            self.y_pos = self.y_start
            self.pixel_count = 0

    def write_data(self, data):
        """LCD data register write (RS=1)"""
        if self.cmd_register == 0x2A:  # Column Address Set
            # Typically 4 bytes: x_start_hi, x_start_lo, x_end_hi, x_end_lo
            pass
        elif self.cmd_register == 0x2B:  # Row Address Set
            pass
        elif self.writing_data or self.cmd_register == 0x2C:
            # Pixel data (RGB565, 16-bit)
            self.writing_data = True
            pixel = data & 0xFFFF
            if 0 <= self.x_pos < LCD_WIDTH and 0 <= self.y_pos < LCD_HEIGHT:
                offset = (self.y_pos * LCD_WIDTH + self.x_pos) * 2
                self.framebuffer[offset] = pixel & 0xFF
                self.framebuffer[offset + 1] = (pixel >> 8) & 0xFF
                self.dirty = True

            self.x_pos += 1
            if self.x_pos > self.x_end:
                self.x_pos = self.x_start
                self.y_pos += 1
            self.pixel_count += 1

    def get_framebuffer_rgb888(self):
        """Convert RGB565 framebuffer to RGB888 for display"""
        rgb888 = bytearray(LCD_WIDTH * LCD_HEIGHT * 3)
        for i in range(LCD_WIDTH * LCD_HEIGHT):
            lo = self.framebuffer[i * 2]
            hi = self.framebuffer[i * 2 + 1]
            pixel = lo | (hi << 8)
            r = ((pixel >> 11) & 0x1F) << 3
            g = ((pixel >> 5) & 0x3F) << 2
            b = (pixel & 0x1F) << 3
            rgb888[i * 3] = r
            rgb888[i * 3 + 1] = g
            rgb888[i * 3 + 2] = b
        return bytes(rgb888)


# ─── Peripheral State ────────────────────────────────────────────────

class GD32F307Peripherals:
    """Emulates the GD32F307 peripheral registers"""
    def __init__(self):
        self.lcd = LCDController()

        # Clock control - firmware checks PLL lock status
        self.rcu_regs = {
            0x00: 0x0D00683A,  # RCU_CTL - HSE ready, PLL locked
            0x04: 0x001D0402,  # RCU_CFG0 - PLL as system clock
            0x08: 0x00000000,  # RCU_INT
            0x0C: 0x00000000,  # RCU_APB2RST
            0x10: 0x00000000,  # RCU_APB1RST
            0x14: 0x00000014,  # RCU_AHBEN
            0x18: 0x00000000,  # RCU_APB2EN
            0x1C: 0x00000000,  # RCU_APB1EN
            0x20: 0x00000000,  # RCU_BDCTL
            0x24: 0x0C000000,  # RCU_RSTSCK
            0x28: 0x00000000,  # RCU_CFG1
        }

        # GPIO port states (input data registers)
        self.gpio_idr = {
            GPIOA_BASE: 0x0000,
            GPIOB_BASE: 0x0000,
            GPIOC_BASE: 0x0000,
            GPIOD_BASE: 0x0000,
            GPIOE_BASE: 0x0000,
        }

        # SysTick
        self.systick_ctrl = 0
        self.systick_load = 0
        self.systick_val = 0

        # USART2 state (FPGA communication)
        self.usart2_rx_buffer = []
        self.usart2_tx_buffer = []
        self.usart2_sr = 0xC0  # TC=1, TXE=1 (transmit ready)

        # Timer states
        self.timer_cnt = {}

        # Instruction counter for periodic events
        self.insn_count = 0
        self.last_systick = 0

        # Flash/SPI state
        self.spi_rx_data = 0xFF

        # EXTI state
        self.exti_imr = 0
        self.exti_pr = 0

        # Logging
        self.log_periph = os.environ.get('EMU_LOG_PERIPH', '0') == '1'
        self.unknown_reads = set()
        self.unknown_writes = set()

    def peripheral_name(self, addr):
        """Get human-readable name for a peripheral address"""
        regions = [
            (RCU_BASE, RCU_BASE + 0x100, "RCU"),
            (GPIOA_BASE, GPIOA_BASE + 0x400, "GPIOA"),
            (GPIOB_BASE, GPIOB_BASE + 0x400, "GPIOB"),
            (GPIOC_BASE, GPIOC_BASE + 0x400, "GPIOC"),
            (GPIOD_BASE, GPIOD_BASE + 0x400, "GPIOD"),
            (GPIOE_BASE, GPIOE_BASE + 0x400, "GPIOE"),
            (AFIO_BASE, AFIO_BASE + 0x400, "AFIO"),
            (TIM2_BASE, TIM2_BASE + 0x400, "TIM2"),
            (TIM3_BASE, TIM3_BASE + 0x400, "TIM3"),
            (TIM4_BASE, TIM4_BASE + 0x400, "TIM4"),
            (TIM5_BASE, TIM5_BASE + 0x400, "TIM5"),
            (TIM6_BASE, TIM6_BASE + 0x400, "TIM6"),
            (TIM7_BASE, TIM7_BASE + 0x400, "TIM7"),
            (SPI0_BASE, SPI0_BASE + 0x400, "SPI0"),
            (SPI1_BASE, SPI1_BASE + 0x400, "SPI1"),
            (SPI2_BASE, SPI2_BASE + 0x400, "SPI2"),
            (USART0_BASE, USART0_BASE + 0x400, "USART0"),
            (USART1_BASE, USART1_BASE + 0x400, "USART1"),
            (USART2_BASE, USART2_BASE + 0x400, "USART2"),
            (I2C0_BASE, I2C0_BASE + 0x400, "I2C0"),
            (I2C1_BASE, I2C1_BASE + 0x400, "I2C1"),
            (USB_BASE, USB_BASE + 0x800, "USB"),
            (CAN0_BASE, CAN0_BASE + 0x400, "CAN0"),
            (CAN1_BASE, CAN1_BASE + 0x400, "CAN1"),
            (DAC_BASE, DAC_BASE + 0x400, "DAC"),
            (NVIC_BASE, NVIC_BASE + 0x1000, "NVIC"),
            (SYSTICK_BASE, SYSTICK_BASE + 0x20, "SysTick"),
            (0xE000ED00, 0xE000ED90, "SCB"),
            (FSMC_REG_BASE, FSMC_REG_BASE + 0x100, "FSMC_CTL"),
        ]
        for start, end, name in regions:
            if start <= addr < end:
                return f"{name}+0x{addr - start:02X}"
        return f"UNK_0x{addr:08X}"

    def read(self, addr, size):
        """Handle peripheral register reads"""
        # RCU (Clock Control)
        if RCU_BASE <= addr < RCU_BASE + 0x100:
            offset = addr - RCU_BASE
            val = self.rcu_regs.get(offset, 0)
            return val

        # GPIO Input Data Register (IDR at offset 0x08)
        for gpio_base in self.gpio_idr:
            if addr == gpio_base + 0x08:
                return self.gpio_idr[gpio_base]

        # SysTick
        if addr == SYSTICK_BASE:        # CTRL
            return self.systick_ctrl | 0x10000  # COUNTFLAG=1
        elif addr == SYSTICK_BASE + 4:  # LOAD
            return self.systick_load
        elif addr == SYSTICK_BASE + 8:  # VAL
            return 0  # Always at zero (triggers reload)

        # USART2
        if addr == USART2_BASE + 0x00:  # SR
            return self.usart2_sr
        elif addr == USART2_BASE + 0x04:  # DR
            if self.usart2_rx_buffer:
                return self.usart2_rx_buffer.pop(0)
            return 0

        # NVIC / SCB
        if addr == SCB_CPACR:  # CPACR - enable FPU
            return 0x00F00000
        if addr == SCB_VTOR:
            return FLASH_BASE
        if 0xE000E3F0 <= addr < 0xE000E400:
            return 0  # Interrupt priority level

        # FSMC control registers
        if FSMC_REG_BASE <= addr < FSMC_REG_BASE + FSMC_REG_SIZE:
            return 0

        # SPI status - always ready
        if addr in (SPI0_BASE + 0x08, SPI1_BASE + 0x08, SPI2_BASE + 0x08):
            return 0x03  # TXE=1, RXNE=1

        # SPI data register
        if addr in (SPI0_BASE + 0x0C, SPI1_BASE + 0x0C, SPI2_BASE + 0x0C):
            return self.spi_rx_data

        # Timer CNT registers
        for timer_base in (TIM2_BASE, TIM3_BASE, TIM4_BASE, TIM5_BASE):
            if addr == timer_base + 0x24:  # CNT
                self.timer_cnt[timer_base] = self.timer_cnt.get(timer_base, 0) + 100
                return self.timer_cnt[timer_base] & 0xFFFF
            if addr == timer_base + 0x10:  # SR
                return 0x01  # UIF set (update interrupt flag)

        # I2C status registers
        if addr in (I2C0_BASE + 0x14, I2C1_BASE + 0x14):  # SR1
            return 0x01  # SB (start bit generated)
        if addr in (I2C0_BASE + 0x18, I2C1_BASE + 0x18):  # SR2
            return 0x03  # BUSY + MSL

        # EXTI
        if addr == 0x40010400 + 0x0C:  # EXTI_PD (pending)
            return self.exti_pr

        # USB
        if USB_BASE <= addr < USB_BASE + 0x800:
            return 0

        # Default: log unknown and return 0
        if self.log_periph and addr not in self.unknown_reads:
            self.unknown_reads.add(addr)
            name = self.peripheral_name(addr)
            print(f"  [PERIPH READ]  {name} (size={size})")
        return 0

    def write(self, addr, size, value):
        """Handle peripheral register writes"""
        # RCU
        if RCU_BASE <= addr < RCU_BASE + 0x100:
            offset = addr - RCU_BASE
            self.rcu_regs[offset] = value
            # Auto-set ready/lock bits
            if offset == 0x00:  # RCU_CTL
                if value & (1 << 16):  # HSEEN
                    self.rcu_regs[0x00] |= (1 << 17)  # HSERDY
                if value & (1 << 24):  # PLLEN
                    self.rcu_regs[0x00] |= (1 << 25)  # PLLRDY
            return

        # GPIO configuration (CRL/CRH)
        for gpio_base in self.gpio_idr:
            if gpio_base <= addr < gpio_base + 0x20:
                return  # Accept silently

        # SysTick
        if addr == SYSTICK_BASE:
            self.systick_ctrl = value
            return
        elif addr == SYSTICK_BASE + 4:
            self.systick_load = value
            return
        elif addr == SYSTICK_BASE + 8:
            self.systick_val = value
            return

        # USART2
        if addr == USART2_BASE + 0x04:  # DR (transmit)
            ch = chr(value & 0x7F)
            self.usart2_tx_buffer.append(ch)
            return
        if USART2_BASE <= addr < USART2_BASE + 0x20:
            return

        # LCD via EXMC
        if EXMC_BASE <= addr < EXMC_BASE + EXMC_SIZE:
            # RS line determines command vs data
            # Typically: addr bit 1 or bit 16 selects RS
            # 0x60000000 = command, 0x60020000 = data (if A16 used for RS)
            if addr == EXMC_BASE:
                self.lcd.write_command(value)
            else:
                self.lcd.write_data(value)
            return

        # FSMC control registers
        if FSMC_REG_BASE <= addr < FSMC_REG_BASE + FSMC_REG_SIZE:
            return

        # NVIC
        if 0xE000E100 <= addr < 0xE000E200:  # ISER (interrupt enable)
            return
        if 0xE000E180 <= addr < 0xE000E200:  # ICER (interrupt disable)
            return
        if 0xE000E200 <= addr < 0xE000E300:  # ISPR (interrupt set pending)
            return
        if 0xE000E400 <= addr < 0xE000E500:  # IPR (interrupt priority)
            return

        # SCB
        if 0xE000ED00 <= addr < 0xE000ED90:
            return

        # Timers, SPI, I2C, DAC, USB, CAN - accept silently
        for base in (TIM2_BASE, TIM3_BASE, TIM4_BASE, TIM5_BASE,
                     TIM6_BASE, TIM7_BASE, SPI0_BASE, SPI1_BASE, SPI2_BASE,
                     I2C0_BASE, I2C1_BASE, DAC_BASE, USB_BASE,
                     CAN0_BASE, CAN1_BASE, AFIO_BASE,
                     USART0_BASE, USART1_BASE):
            if base <= addr < base + 0x400:
                return

        # EXTI
        if 0x40010400 <= addr < 0x40010800:
            return

        # Default: log unknown
        if self.log_periph and addr not in self.unknown_writes:
            self.unknown_writes.add(addr)
            name = self.peripheral_name(addr)
            print(f"  [PERIPH WRITE] {name} = 0x{value:08X} (size={size})")


# ─── Emulator Core ───────────────────────────────────────────────────

class FNIRSI2C53TEmulator:
    def __init__(self, firmware_path):
        self.firmware_path = firmware_path
        self.peripherals = GD32F307Peripherals()
        self.running = False
        self.insn_count = 0
        self.max_instructions = 0  # 0 = unlimited
        self.disassembler = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        self.breakpoints = set()
        self.trace_enabled = os.environ.get('EMU_TRACE', '0') == '1'
        self.last_pc = 0
        self.stuck_count = 0

        # Load firmware
        with open(firmware_path, 'rb') as f:
            self.firmware = f.read()

        print(f"Loaded firmware: {len(self.firmware)} bytes ({len(self.firmware)/1024:.0f} KB)")

        # Read vector table
        self.initial_sp = struct.unpack('<I', self.firmware[0:4])[0]
        self.reset_vector = struct.unpack('<I', self.firmware[4:8])[0]
        print(f"Initial SP:   0x{self.initial_sp:08X}")
        print(f"Reset vector: 0x{self.reset_vector:08X}")

        # Initialize Unicorn
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)

        # Map memory regions
        self.uc.mem_map(FLASH_BASE, FLASH_SIZE)
        self.uc.mem_map(RAM_BASE, RAM_SIZE)
        self.uc.mem_map(PERIPH_BASE, PERIPH_SIZE)
        self.uc.mem_map(EXMC_BASE, EXMC_SIZE)
        self.uc.mem_map(SCB_BASE, SCB_SIZE)
        self.uc.mem_map(FSMC_REG_BASE, FSMC_REG_SIZE)

        # Load firmware into flash
        self.uc.mem_write(FLASH_BASE, self.firmware)

        # Set up stack pointer
        self.uc.reg_write(UC_ARM_REG_SP, self.initial_sp)

        # Enable FPU (CPACR)
        self.uc.mem_write(SCB_CPACR, struct.pack('<I', 0x00F00000))

        # Install hooks
        self.uc.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)

        self.uc.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=EXMC_BASE, end=EXMC_BASE + EXMC_SIZE)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=EXMC_BASE, end=EXMC_BASE + EXMC_SIZE)

        self.uc.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)

        self.uc.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=FSMC_REG_BASE, end=FSMC_REG_BASE + FSMC_REG_SIZE)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=FSMC_REG_BASE, end=FSMC_REG_BASE + FSMC_REG_SIZE)

        # Unmapped memory handler
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._hook_unmapped)

        # Code execution hook (for tracing and stuck detection)
        self.uc.hook_add(UC_HOOK_CODE, self._hook_code)

    def _hook_mem_read(self, uc, access, addr, size, value, user_data):
        val = self.peripherals.read(addr, size)
        # Write the value back so the CPU gets it
        if size == 1:
            uc.mem_write(addr, struct.pack('<B', val & 0xFF))
        elif size == 2:
            uc.mem_write(addr, struct.pack('<H', val & 0xFFFF))
        elif size == 4:
            uc.mem_write(addr, struct.pack('<I', val & 0xFFFFFFFF))

    def _hook_mem_write(self, uc, access, addr, size, value, user_data):
        self.peripherals.write(addr, size, value)

    def _hook_unmapped(self, uc, access, addr, size, value, user_data):
        pc = uc.reg_read(UC_ARM_REG_PC)
        access_type = "READ" if access == UC_MEM_READ_UNMAPPED else "WRITE"
        print(f"\n[UNMAPPED {access_type}] addr=0x{addr:08X} size={size} value=0x{value:X} PC=0x{pc:08X}")

        # Map the region and continue (don't crash)
        aligned = addr & 0xFFFFF000
        try:
            uc.mem_map(aligned, 0x1000)
            print(f"  -> Auto-mapped 0x{aligned:08X}")
        except:
            pass
        return True

    def _hook_code(self, uc, addr, size, user_data):
        self.insn_count += 1

        # Trace mode
        if self.trace_enabled and self.insn_count % 1000 == 0:
            code = uc.mem_read(addr, size)
            for insn in self.disassembler.disasm(bytes(code), addr):
                print(f"  0x{addr:08X}: {insn.mnemonic} {insn.op_str}")

        # Stuck detection - if PC hasn't changed in 100k instructions, we're looping
        if self.insn_count % 100000 == 0:
            pc = addr
            if pc == self.last_pc:
                self.stuck_count += 1
                if self.stuck_count >= 3:
                    print(f"\n[STUCK] PC=0x{pc:08X} for {self.stuck_count * 100000} instructions")
                    print(f"  Total instructions: {self.insn_count:,}")
                    self.dump_state()
                    uc.emu_stop()
            else:
                self.stuck_count = 0
            self.last_pc = pc

        # Instruction limit
        if self.max_instructions and self.insn_count >= self.max_instructions:
            print(f"\n[LIMIT] Reached {self.max_instructions:,} instructions")
            self.dump_state()
            uc.emu_stop()

        # Progress reporting
        if self.insn_count % 1000000 == 0:
            pc = uc.reg_read(UC_ARM_REG_PC)
            print(f"[{self.insn_count/1000000:.0f}M instructions] PC=0x{pc:08X}")

    def dump_state(self):
        """Print current CPU state"""
        regs = ['R0', 'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7',
                'R8', 'R9', 'R10', 'R11', 'R12', 'SP', 'LR', 'PC']
        reg_ids = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                   UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
                   UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
                   UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC]

        print("\n=== CPU State ===")
        for i in range(0, len(regs), 4):
            line = ""
            for j in range(4):
                if i + j < len(regs):
                    val = self.uc.reg_read(reg_ids[i + j])
                    line += f"  {regs[i+j]:3s}=0x{val:08X}"
            print(line)

        # Show USART2 TX buffer
        if self.peripherals.usart2_tx_buffer:
            tx = ''.join(self.peripherals.usart2_tx_buffer)
            print(f"\nUSART2 TX: {repr(tx)}")

        # Show LCD pixel count
        print(f"LCD pixels written: {self.peripherals.lcd.pixel_count}")
        print(f"Total instructions: {self.insn_count:,}")

    def run(self, max_instructions=0):
        """Start emulation"""
        self.max_instructions = max_instructions
        start_addr = self.reset_vector & ~1  # Clear Thumb bit for entry

        print(f"\nStarting emulation at 0x{start_addr:08X} (Thumb mode)")
        print(f"{'Max instructions: ' + f'{max_instructions:,}' if max_instructions else 'Running until stopped...'}")
        print("=" * 60)

        self.running = True
        try:
            self.uc.emu_start(start_addr | 1, 0, 0,
                              max_instructions if max_instructions else 0)
        except UcError as e:
            pc = self.uc.reg_read(UC_ARM_REG_PC)
            print(f"\n[ERROR] {e} at PC=0x{pc:08X}")
            self.dump_state()
        finally:
            self.running = False

        self.dump_state()

    def save_framebuffer(self, path="framebuffer.bin"):
        """Save LCD framebuffer as raw RGB565"""
        with open(path, 'wb') as f:
            f.write(self.peripherals.lcd.framebuffer)
        print(f"Framebuffer saved to {path} ({LCD_WIDTH}x{LCD_HEIGHT} RGB565)")

    def save_framebuffer_ppm(self, path="framebuffer.ppm"):
        """Save LCD framebuffer as PPM image (viewable with most image viewers)"""
        rgb = self.peripherals.lcd.get_framebuffer_rgb888()
        with open(path, 'wb') as f:
            f.write(f"P6\n{LCD_WIDTH} {LCD_HEIGHT}\n255\n".encode())
            f.write(rgb)
        print(f"Framebuffer saved to {path}")


# ─── Main ─────────────────────────────────────────────────────────────

def main():
    import argparse
    parser = argparse.ArgumentParser(description='FNIRSI 2C53T Emulator')
    parser.add_argument('firmware', help='Path to firmware .bin file')
    parser.add_argument('-n', '--max-instructions', type=int, default=5_000_000,
                        help='Max instructions to execute (default: 5M, 0=unlimited)')
    parser.add_argument('-t', '--trace', action='store_true',
                        help='Enable instruction tracing (every 1000th instruction)')
    parser.add_argument('-p', '--log-peripherals', action='store_true',
                        help='Log unknown peripheral accesses')
    parser.add_argument('-s', '--save-framebuffer', action='store_true',
                        help='Save framebuffer as PPM image after emulation')
    args = parser.parse_args()

    if args.trace:
        os.environ['EMU_TRACE'] = '1'
    if args.log_peripherals:
        os.environ['EMU_LOG_PERIPH'] = '1'

    emu = FNIRSI2C53TEmulator(args.firmware)
    emu.run(max_instructions=args.max_instructions)

    if args.save_framebuffer:
        emu.save_framebuffer_ppm(
            os.path.join(os.path.dirname(args.firmware), 'framebuffer.ppm'))


if __name__ == '__main__':
    main()
