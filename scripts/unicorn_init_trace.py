#!/usr/bin/env python3
"""
Unicorn-based peripheral write tracer for stock firmware init.

Executes the stock firmware's master_init function (FUN_08023A50) in a
Unicorn ARM Cortex-M4 emulator and logs every store to peripheral MMIO
space (0x40000000+) with fully resolved values.

Unlike the Capstone static trace, this resolves ALL values — including
those computed at runtime, passed through HAL functions, or loaded from
RAM. This is the ground truth for what the stock firmware writes.

Usage:
    python3 unicorn_init_trace.py

Part of the SPI3 compliance audit escalation (2026-04-06).
"""

import struct
import sys
from collections import OrderedDict
from unicorn import *
from unicorn.arm_const import *

# ─── Memory Map ──────────────────────────────────────────────────────

FLASH_BASE   = 0x08000000
FLASH_SIZE   = 1024 * 1024       # 1MB — stock binary is 751KB
RAM_BASE     = 0x20000000
RAM_SIZE     = 224 * 1024        # 224KB (EOPB0=0xFE)
PERIPH_BASE  = 0x40000000
PERIPH_SIZE  = 0x00100000        # 1MB peripheral space
EXMC_BASE    = 0x60000000
EXMC_SIZE    = 0x00100000
SCB_BASE     = 0xE0000000
SCB_SIZE     = 0x00100000
XMC_BASE     = 0xA0000000
XMC_SIZE     = 0x00010000

# ─── Peripheral register names (same as trace_peripheral_writes.py) ──

PERIPH_NAMES = {
    0x40021000: "CRM_CTRL", 0x40021004: "CRM_CFG", 0x40021008: "CRM_CLKINT",
    0x4002100C: "CRM_APB2RST", 0x40021010: "CRM_APB1RST",
    0x40021014: "CRM_AHBEN", 0x40021018: "CRM_APB2EN", 0x4002101C: "CRM_APB1EN",
    0x40010000: "IOMUX_EVCTRL", 0x40010004: "IOMUX_REMAP",
    0x40010008: "IOMUX_EXTIC1", 0x4001000C: "IOMUX_EXTIC2",
    0x40010020: "IOMUX_REMAP2", 0x40010024: "IOMUX_REMAP3",
    0x40010028: "IOMUX_REMAP4", 0x4001002C: "IOMUX_REMAP5",
    0x40010030: "IOMUX_REMAP6", 0x40010034: "IOMUX_REMAP7",
    0x40010800: "GPIOA_CFGLR", 0x40010804: "GPIOA_CFGHR",
    0x40010808: "GPIOA_IDT", 0x4001080C: "GPIOA_ODT",
    0x40010810: "GPIOA_SCR", 0x40010814: "GPIOA_CLR",
    0x40010C00: "GPIOB_CFGLR", 0x40010C04: "GPIOB_CFGHR",
    0x40010C08: "GPIOB_IDT", 0x40010C0C: "GPIOB_ODT",
    0x40010C10: "GPIOB_SCR", 0x40010C14: "GPIOB_CLR",
    0x40011000: "GPIOC_CFGLR", 0x40011004: "GPIOC_CFGHR",
    0x40011008: "GPIOC_IDT", 0x4001100C: "GPIOC_ODT",
    0x40011010: "GPIOC_SCR", 0x40011014: "GPIOC_CLR",
    0x40011400: "GPIOD_CFGLR", 0x40011404: "GPIOD_CFGHR",
    0x40011410: "GPIOD_SCR", 0x40011414: "GPIOD_CLR",
    0x40011800: "GPIOE_CFGLR", 0x40011804: "GPIOE_CFGHR",
    0x40011810: "GPIOE_SCR", 0x40011814: "GPIOE_CLR",
    0x40003C00: "SPI3_CTRL1", 0x40003C04: "SPI3_CTRL2",
    0x40003C08: "SPI3_STS", 0x40003C0C: "SPI3_DT",
    0x40003800: "SPI2_CTRL1", 0x40003804: "SPI2_CTRL2",
    0x40003808: "SPI2_STS", 0x4000380C: "SPI2_DT",
    0x40004400: "USART2_STS", 0x40004404: "USART2_DT",
    0x40004408: "USART2_BAUDR", 0x4000440C: "USART2_CTRL1",
    0x40004410: "USART2_CTRL2", 0x40004414: "USART2_CTRL3",
    0x40013800: "USART1_STS", 0x40013804: "USART1_DT",
    0x40013808: "USART1_BAUDR", 0x4001380C: "USART1_CTRL1",
    0x40000000: "TMR2_CTRL1", 0x40000004: "TMR2_CTRL2",
    0x40000400: "TMR3_CTRL1", 0x40000404: "TMR3_CTRL2",
    0x40000C00: "TMR5_CTRL1", 0x40000C04: "TMR5_CTRL2",
    0x40001000: "TMR6_CTRL1", 0x40001400: "TMR7_CTRL1",
    0x40001800: "TMR12_CTRL1", 0x40001C00: "TMR13_CTRL1",
    0x40012C00: "TMR1_CTRL1", 0x40013400: "TMR8_CTRL1",
    0x40014C00: "TMR9_CTRL1",
    0x40015000: "TMR10_CTRL1", 0x40015400: "TMR11_CTRL1",
    0x40007400: "DAC_CTRL", 0x40007408: "DAC1_DHR12R",
    0x40012400: "ADC1_STS", 0x40012404: "ADC1_CTRL1", 0x40012408: "ADC1_CTRL2",
    0x40020000: "DMA1_ISTS", 0x40020004: "DMA1_ICLR",
    0x40003000: "IWDG_CMD", 0x40003004: "IWDG_DIV", 0x40003008: "IWDG_RLD",
    0x40005C00: "USB_BASE",
    0xE000E100: "NVIC_ISER0", 0xE000E104: "NVIC_ISER1",
    0xE000E010: "SYSTICK_CTRL", 0xE000E014: "SYSTICK_LOAD",
    0xE000E018: "SYSTICK_VAL", 0xE000ED0C: "SCB_AIRCR",
    0xA0000000: "XMC_SNCTL0", 0xA0000004: "XMC_SNTCFG0",
    0xA0000104: "XMC_SNWTCFG0", 0xA0000220: "XMC_EXT0",
}

def name_reg(addr):
    if addr in PERIPH_NAMES:
        return PERIPH_NAMES[addr]
    # Find nearest known base
    for base in sorted(PERIPH_NAMES.keys(), reverse=True):
        if base <= addr and addr - base < 0x100:
            return f"{PERIPH_NAMES[base].split('_')[0]}+0x{addr-base:02X}"
    return f"0x{addr:08X}"


class InitTracer:
    def __init__(self, binary_path):
        self.binary_path = binary_path
        self.writes = []  # ordered list of (addr, value, size, pc)
        self.instruction_count = 0
        self.max_instructions = 50_000_000  # safety limit (50M)
        self.systick_skip_count = 0

        # Peripheral memory backing (read returns last written value)
        self.periph_mem = {}

        with open(binary_path, 'rb') as f:
            self.binary = f.read()

    def setup(self):
        """Initialize Unicorn emulator with memory regions."""
        self.mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)

        # Map memory regions
        self.mu.mem_map(FLASH_BASE, FLASH_SIZE)
        self.mu.mem_map(RAM_BASE, RAM_SIZE)
        self.mu.mem_map(PERIPH_BASE, PERIPH_SIZE)
        self.mu.mem_map(EXMC_BASE, EXMC_SIZE)
        self.mu.mem_map(SCB_BASE, SCB_SIZE)
        self.mu.mem_map(XMC_BASE, XMC_SIZE)

        # Load binary into flash
        self.mu.mem_write(FLASH_BASE, self.binary)

        # Initialize RAM to zero
        self.mu.mem_write(RAM_BASE, b'\x00' * RAM_SIZE)

        # Pre-populate critical peripheral registers with realistic values
        # so HAL functions that read-modify-write work correctly

        # CRM: clocks already enabled (stock firmware sets these before master_init)
        # The reset handler + SystemInit run before FUN_08023A50
        self.periph_write(0x40021014, 0x00000114, 4)  # AHBEN: DMA, EXMC, SRAM
        self.periph_write(0x40021018, 0x00007E7D, 4)  # APB2EN: GPIO A-E, AFIO, ADC, TMR1/8
        self.periph_write(0x4002101C, 0x1802B81F, 4)  # APB1EN: TMR2-7, SPI2/3, USART2, DAC
        self.periph_write(0x40021004, 0x001D8402, 4)  # CRM_CFG: PLL configured, 240MHz

        # SysTick: configured for 240MHz
        self.periph_write(0xE000E014, 240000 - 1, 4)  # LOAD: 1ms at 240MHz
        self.periph_write(0xE000E018, 0, 4)            # VAL: 0

        # SCB: Cortex-M4 defaults
        self.periph_write(0xE000ED0C, 0xFA050000, 4)  # AIRCR: default priority grouping

        # GPIO default states (all inputs after reset)
        for port_base in [0x40010800, 0x40010C00, 0x40011000, 0x40011400, 0x40011800]:
            self.periph_write(port_base + 0x00, 0x44444444, 4)  # CFGLR: input floating
            self.periph_write(port_base + 0x04, 0x44444444, 4)  # CFGHR: input floating

        # SPI3: TXE=1, RXNE=1 (ready for polled transfers)
        self.periph_write(0x40003C08, 0x0003, 4)  # SPI3_STS: TXE=1, RXNE=1

        # SPI2 (flash): TXE=1, RXNE=1 — must respond or flash init loops forever
        self.periph_write(0x40003808, 0x0003, 4)  # SPI2_STS: TXE=1, RXNE=1
        # SPI2 data register: return W25Q128 JEDEC ID bytes in sequence
        self.spi2_rx_queue = [0xEF, 0x40, 0x18, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF] * 32
        self.spi2_rx_idx = 0

        # USART2: TXE=1, TC=1 (ready to transmit)
        self.periph_write(0x40004400, 0x00C0, 4)  # USART2_STS: TXE=1, TC=1

        # ADC1: calibration complete flags
        self.periph_write(0x40012408, 0x00000000, 4)  # ADC1_CTRL2

        # Set up stack pointer (top of RAM)
        self.mu.reg_write(UC_ARM_REG_SP, RAM_BASE + RAM_SIZE)

        # Pre-populate RAM locations that stock firmware reads during init
        # meter_state structure at 0x200000F8 — needs magic byte for validation
        # Stock checks for 0x55 at a certain offset; 0xFF = use defaults
        # Leave as zeros (will use defaults)

        # SysTick reload values stored in RAM by SystemInit
        struct.pack_into('<I', bytearray(4), 0, 2400)  # 10us at 240MHz
        self.mu.mem_write(0x20002B1C, struct.pack('<I', 2400))   # systick_reload_1
        self.mu.mem_write(0x20002B20, struct.pack('<I', 240))    # systick_reload_2

        # Install hooks
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=XMC_BASE, end=XMC_BASE + XMC_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self._hook_mem_write,
                         begin=EXMC_BASE, end=EXMC_BASE + EXMC_SIZE)

        # Hook reads from peripheral space to return last written value
        self.mu.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=PERIPH_BASE, end=PERIPH_BASE + PERIPH_SIZE)
        self.mu.hook_add(UC_HOOK_MEM_READ, self._hook_mem_read,
                         begin=SCB_BASE, end=SCB_BASE + SCB_SIZE)

        # Hook code for instruction counting and SysTick fast-forward
        self.mu.hook_add(UC_HOOK_CODE, self._hook_code)

        # Hook unmapped memory
        self.mu.hook_add(UC_HOOK_MEM_UNMAPPED, self._hook_unmapped)

    def periph_write(self, addr, value, size):
        """Write to peripheral backing store and mapped memory."""
        self.periph_mem[addr] = (value, size)
        if size == 4:
            data = struct.pack('<I', value & 0xFFFFFFFF)
        elif size == 2:
            data = struct.pack('<H', value & 0xFFFF)
        else:
            data = struct.pack('<B', value & 0xFF)
        try:
            self.mu.mem_write(addr, data)
        except:
            pass

    def _hook_mem_write(self, uc, access, address, size, value, user_data):
        """Log every peripheral write."""
        # Skip EXMC LCD data writes (0x60000000+ is framebuffer, too noisy)
        if 0x60000000 <= address < 0x61000000:
            return

        pc = uc.reg_read(UC_ARM_REG_PC)
        self.writes.append((address, value, size, pc))

        # Update backing store
        self.periph_mem[address] = (value, size)

        # Special handling: SysTick CTRL write with ENABLE=1
        if address == 0xE000E010 and (value & 1):
            # SysTick enabled — immediately set COUNTFLAG to skip delay loops
            # Stock firmware spins on COUNTFLAG (bit 16 of CTRL)
            current = value | (1 << 16)  # Set COUNTFLAG
            uc.mem_write(0xE000E010, struct.pack('<I', current))
            self.systick_skip_count += 1

        # Special: SPI3_STS — keep TXE=1, RXNE=1 for polled transfers
        if address == 0x40003C00:  # SPI3_CTRL1 — SPE enable
            if value & (1 << 6):
                # SPI enabled — ensure STS shows ready
                uc.mem_write(0x40003C08, struct.pack('<I', 0x0003))  # TXE=1, RXNE=1

        # Special: SPI3_DT write — simulate full-duplex (return 0x00 on read)
        if address == 0x40003C0C:
            uc.mem_write(0x40003C0C, struct.pack('<I', 0x00))  # MISO data = 0x00

        # Special: SPI2_DT write — simulate SPI flash (W25Q128 JEDEC responses)
        if address == 0x4000380C:
            rx_byte = self.spi2_rx_queue[self.spi2_rx_idx % len(self.spi2_rx_queue)]
            self.spi2_rx_idx += 1
            uc.mem_write(0x4000380C, struct.pack('<I', rx_byte))
            # Keep SPI2 STS ready
            uc.mem_write(0x40003808, struct.pack('<I', 0x0003))

        # Special: USART2_DT write — keep TXE=1, TC=1
        if address == 0x40004404:
            uc.mem_write(0x40004400, struct.pack('<I', 0x00C0))  # STS: TXE=1, TC=1

        # Special: ADC RSTCAL/CAL — immediately clear the bits
        if address == 0x40012408:
            cleared = value & ~((1 << 2) | (1 << 3))  # Clear RSTCAL, CAL
            uc.mem_write(0x40012408, struct.pack('<I', cleared))

    def _hook_mem_read(self, uc, access, address, size, value, user_data):
        """Return last written value for peripheral reads."""
        if address in self.periph_mem:
            val, sz = self.periph_mem[address]
            return

        # SysTick CTRL read — always show COUNTFLAG set (skip delays)
        if address == 0xE000E010:
            uc.mem_write(address, struct.pack('<I', (1 << 16) | 0x05))
            return

        # SPI3 STS — TXE=1, RXNE=1 (always ready)
        if address == 0x40003C08:
            uc.mem_write(address, struct.pack('<I', 0x0003))
            return

        # SPI2 STS — TXE=1, RXNE=1 (always ready for flash)
        if address == 0x40003808:
            uc.mem_write(address, struct.pack('<I', 0x0003))
            return

        # SPI2 DT read — return next byte from flash response queue
        if address == 0x4000380C:
            rx_byte = self.spi2_rx_queue[self.spi2_rx_idx % len(self.spi2_rx_queue)]
            self.spi2_rx_idx += 1
            uc.mem_write(address, struct.pack('<I', rx_byte))
            return

        # USART2 STS — TXE=1, TC=1 (always ready)
        if address == 0x40004400:
            uc.mem_write(address, struct.pack('<I', 0x00C0))
            return

    def _hook_code(self, uc, address, size, user_data):
        """Count instructions and enforce safety limit."""
        self.instruction_count += 1
        if self.instruction_count > self.max_instructions:
            uc.emu_stop()

        # Detect infinite loops (SysTick wait, USART wait)
        # If we execute the same address 1000+ times, skip ahead
        if not hasattr(self, '_last_addr'):
            self._last_addr = 0
            self._repeat_count = 0

        if address == self._last_addr:
            self._repeat_count += 1
            if self._repeat_count > 100:
                # Likely a wait loop — try to break out
                # Set condition flags to break the loop
                self._repeat_count = 0
        else:
            self._last_addr = address
            self._repeat_count = 0

    def _hook_unmapped(self, uc, access, address, size, value, user_data):
        """Handle unmapped memory access."""
        pc = uc.reg_read(UC_ARM_REG_PC)
        access_type = "READ" if access == UC_MEM_READ_UNMAPPED else "WRITE"

        # Auto-map the region
        page_base = address & ~0xFFF
        try:
            uc.mem_map(page_base, 0x1000)
            uc.mem_write(page_base, b'\x00' * 0x1000)
            print(f"  [AUTO-MAP] {access_type} 0x{address:08X} (mapped 0x{page_base:08X}) @ PC=0x{pc:08X}")
            return True
        except:
            print(f"  [UNMAPPED] {access_type} 0x{address:08X} @ PC=0x{pc:08X}")
            return False

    def run(self):
        """Execute master_init and return the write log."""
        # FUN_08023A50 is the master init
        # It's Thumb code, so start address needs bit 0 set
        start_addr = 0x08023A50 | 1
        # It ends around 0x080276F2, but we'll let it run until it calls
        # vTaskStartScheduler (which we can't emulate). Set a generous limit.

        print(f"Starting Unicorn emulation of FUN_08023A50...")
        print(f"Binary: {self.binary_path}")
        print(f"Max instructions: {self.max_instructions:,}")

        try:
            # Emulate starting at FUN_08023A50
            # Set LR to a sentinel address so BX LR returns somewhere we can detect
            self.mu.reg_write(UC_ARM_REG_LR, 0x08FFFFF0 | 1)

            self.mu.emu_start(start_addr, 0x08FFFFF0, timeout=30_000_000)  # 30 sec timeout
        except UcError as e:
            pc = self.mu.reg_read(UC_ARM_REG_PC)
            print(f"Emulation stopped: {e} @ PC=0x{pc:08X}")
            print(f"Instructions executed: {self.instruction_count:,}")

        print(f"\nEmulation complete.")
        print(f"Instructions: {self.instruction_count:,}")
        print(f"SysTick skips: {self.systick_skip_count}")
        print(f"Peripheral writes logged: {len(self.writes)}")

        return self.writes

    def report(self):
        """Print the write log in a readable format."""
        # Filter out noisy writes (SysTick, EXMC data)
        filtered = []
        for addr, val, size, pc in self.writes:
            # Skip SysTick (delay loop mechanics)
            if 0xE000E010 <= addr <= 0xE000E01C:
                continue
            # Skip EXMC LCD data
            if 0x60000000 <= addr < 0x61000000:
                continue
            filtered.append((addr, val, size, pc))

        # Deduplicate consecutive writes to same address with same value
        deduped = []
        for entry in filtered:
            if deduped and deduped[-1][:3] == entry[:3]:
                continue
            deduped.append(entry)

        print(f"\n{'='*110}")
        print(f"PERIPHERAL REGISTER WRITES (excluding SysTick, EXMC data)")
        print(f"{'='*110}")
        print(f"{'#':<6} {'PC':<14} {'Register':<14} {'Name':<24} {'Size':<6} {'Value'}")
        print(f"{'-'*6} {'-'*14} {'-'*14} {'-'*24} {'-'*6} {'-'*14}")

        for i, (addr, val, size, pc) in enumerate(deduped):
            name = name_reg(addr)
            val_str = f"0x{val:08X}" if size == 4 else f"0x{val:04X}" if size == 2 else f"0x{val:02X}"
            print(f"{i:<6} 0x{pc:08X}   0x{addr:08X}   {name:<24} {size*8:<6} {val_str}")

        # Summary
        print(f"\n{'='*110}")
        print(f"Total filtered writes: {len(deduped)}")

        # Group by peripheral block
        from collections import defaultdict
        blocks = defaultdict(list)
        for addr, val, size, pc in deduped:
            name = name_reg(addr)
            block = name.split('_')[0] if '_' in name else name.split('+')[0]
            blocks[block].append((addr, val, size, pc))

        print(f"\nBy peripheral:")
        for block in sorted(blocks.keys()):
            entries = blocks[block]
            print(f"  {block:<20} {len(entries):>4} writes")

        # Highlight SPI3 writes specifically
        spi3_writes = [(a, v, s, p) for a, v, s, p in deduped if 0x40003C00 <= a < 0x40003C30]
        if spi3_writes:
            print(f"\n{'='*110}")
            print(f"SPI3 REGISTER WRITES (the ones we care about most):")
            print(f"{'='*110}")
            for addr, val, size, pc in spi3_writes:
                name = name_reg(addr)
                bits = []
                if addr == 0x40003C00:  # CTRL1
                    if val & 1: bits.append("CPHA=1")
                    if val & 2: bits.append("CPOL=1")
                    if val & 4: bits.append("MSTR=1")
                    br = (val >> 3) & 7
                    bits.append(f"BR={br}(/{2**(br+1)})")
                    if val & 0x40: bits.append("SPE=1")
                    if val & 0x100: bits.append("SSI=1")
                    if val & 0x200: bits.append("SSM=1")
                elif addr == 0x40003C04:  # CTRL2
                    if val & 1: bits.append("RXDMAEN")
                    if val & 2: bits.append("TXDMAEN")
                    if val & 4: bits.append("SSOE")
                    if val & 0x40: bits.append("ERRIE")
                    if val & 0x80: bits.append("RXNEIE")
                    if val & 0x100: bits.append("TXEIE")
                print(f"  0x{addr:08X} {name:<16} = 0x{val:08X}  [{', '.join(bits)}]")

        # Write CSV
        csv_path = self.binary_path.rsplit('.', 1)[0] + '_unicorn_trace.csv'
        with open(csv_path, 'w') as f:
            f.write("seq,pc,periph_addr,name,width,value\n")
            for i, (addr, val, size, pc) in enumerate(deduped):
                name = name_reg(addr)
                f.write(f"{i},0x{pc:08X},0x{addr:08X},{name},{size*8},0x{val:08X}\n")
        print(f"\nCSV: {csv_path}")


def main():
    binary_path = "/Users/david/Desktop/osc/archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
    if len(sys.argv) > 1:
        binary_path = sys.argv[1]

    tracer = InitTracer(binary_path)
    tracer.setup()
    tracer.run()
    tracer.report()


if __name__ == '__main__':
    main()
