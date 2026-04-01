#!/usr/bin/env python3
"""
Comprehensive disassembly and pseudocode annotation of the FNIRSI 2C53T
FPGA/acquisition init function.

The function actually starts at 0x08023A50 (push.w {r4-r11,lr}) and runs
through ~0x080276F0, but the user-specified range of interest is
0x08025800-0x08027700.  We disassemble 0x08023A50-0x08027700 for full
context, with extra detail in the 0x08025800-0x08027700 range.

Output: reverse_engineering/analysis_v120/init_function_decompile.txt
"""

import struct
import sys
from pathlib import Path

# Add venv to path
sys.path.insert(0, "/Users/david/Desktop/osc/emulator/.venv/lib/python3.13/site-packages")
import capstone

# ── Configuration ──────────────────────────────────────────────────────

BIN_PATH  = "/Users/david/Desktop/osc/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000
FUNC_START = 0x08023A50   # True function start (push.w)
RANGE_START = 0x08025800   # User's area of interest start
RANGE_END   = 0x08027700   # User's area of interest end
OUTPUT_PATH = "/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt"

# ── Known addresses ────────────────────────────────────────────────────

KNOWN_FUNCTIONS = {
    0x080302fc: "gpio_init",
    0x08036848: "spi_init",
    0x0803acf0: "xQueueGenericSend",
    0x0803b09c: "xQueueGenericSendFromISR",
    0x0803b1d8: "xQueueReceive",
    0x0803a53c: "vTaskDelay",
    0x0803a390: "vTaskDelay2",
    0x0803ab74: "xQueueCreate",
    0x0803b6a0: "xTaskCreate",
    0x0803bd88: "xTaskCreate_v2",
    0x0803a610: "vTaskResume",
    0x0803ac38: "xQueueGenericReset",
    0x0802a430: "timer_init",
    0x0803b3a8: "vTaskSuspend",
    0x080018a4: "FUN_080018a4",
    0x0801a058: "FUN_0801a058",
    0x08001a58: "FUN_08001a58",
    0x08036934: "fpga_task_main",
    0x08025e00: "usart_tx_cmd",          # placeholder, discovered during analysis
}

PERIPHERAL_MAP = {
    0x40003C00: "SPI3_CTL0",
    0x40003C04: "SPI3_CTL1",
    0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA",
    0x40003800: "SPI2_CTL0",
    0x40003804: "SPI2_CTL1",
    0x40003808: "SPI2_STAT",
    0x4000380C: "SPI2_DATA",
    0x40004400: "USART_STS",
    0x40004404: "USART_DATA",
    0x40004408: "USART_BRR",
    0x4000440C: "USART_CTL0",
    0x40004410: "USART_CTL1",
    0x40010000: "AFIO_EC",
    0x40010004: "AFIO_PCF0",
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
    0x40021000: "RCU_CTL",
    0x40021004: "RCU_CFG0",
    0x40021008: "RCU_INT",
    0x4002100C: "RCU_APB2RST",
    0x40021010: "RCU_APB1RST",
    0x40021014: "RCU_AHBEN",
    0x40021018: "RCU_APB2EN",
    0x4002101C: "RCU_APB1EN",
    0x40020000: "DMA0_INTF",
    0x40020004: "DMA0_INTC",
    0xE000E010: "SysTick_CTL",
    0xE000E014: "SysTick_LOAD",
    0xE000E018: "SysTick_VAL",
    0xE000ED0C: "SCB_AIRCR",
    0xE000EF00: "SysTick_??_F0",
    0xE000EF04: "SysTick_??_F4",
    0xE000E100: "NVIC_ISER0",
    0xE000E104: "NVIC_ISER1",
    0xE000E180: "NVIC_ICER0",
    0xE000E184: "NVIC_ICER1",
    0xE000E200: "NVIC_ISPR0",
    0xE000E400: "NVIC_IPR0",
    0x40000000: "TIM2_CTL0",
    0x40000400: "TIM3_CTL0",
    0x40000800: "TIM4_CTL0",
    0x40000C00: "TIM5_CTL0",
    0x40012400: "ADC1_STAT",
    0x40012800: "ADC2_STAT",
}

RAM_MAP = {
    0x200000F8: "meter_state_base",
    0x20000005: "usart_tx_buf",
    0x2000000F: "usart_tx_idx",
    0x20002B1C: "systick_reload_1",
    0x20002B20: "systick_reload_2",
    0x20002D6C: "queue_handle_1",
    0x20002D70: "queue_handle_2",
    0x20002D74: "queue_handle_3",
    0x20002D78: "spi3_data_queue",
    0x20002D7C: "queue_handle_5",
    0x20002D80: "queue_handle_6",
    0x20002D84: "queue_handle_7",
    0x20002D88: "queue_handle_8",
    0x20002D8C: "queue_handle_9",
    0x20002D90: "queue_handle_10",
    0x20000000: "ram_base",
}

GPIO_PIN_NAMES = {
    (0x40010800, 0x0001): "PA0",  (0x40010800, 0x0002): "PA1",
    (0x40010800, 0x0004): "PA2",  (0x40010800, 0x0008): "PA3",
    (0x40010800, 0x0010): "PA4",  (0x40010800, 0x0020): "PA5",
    (0x40010800, 0x0040): "PA6",  (0x40010800, 0x0080): "PA7",
    (0x40010800, 0x0100): "PA8",  (0x40010800, 0x0200): "PA9",
    (0x40010800, 0x0400): "PA10", (0x40010800, 0x0800): "PA11",
    (0x40010800, 0x1000): "PA12", (0x40010800, 0x2000): "PA13",
    (0x40010800, 0x4000): "PA14", (0x40010800, 0x8000): "PA15",
    (0x40010C00, 0x0001): "PB0",  (0x40010C00, 0x0002): "PB1",
    (0x40010C00, 0x0004): "PB2",  (0x40010C00, 0x0008): "PB3",
    (0x40010C00, 0x0010): "PB4",  (0x40010C00, 0x0020): "PB5",
    (0x40010C00, 0x0040): "PB6",  (0x40010C00, 0x0080): "PB7",
    (0x40010C00, 0x0100): "PB8",  (0x40010C00, 0x0200): "PB9",
    (0x40010C00, 0x0400): "PB10", (0x40010C00, 0x0800): "PB11",
    (0x40010C00, 0x1000): "PB12", (0x40010C00, 0x2000): "PB13",
    (0x40010C00, 0x4000): "PB14", (0x40010C00, 0x8000): "PB15",
    (0x40011000, 0x0001): "PC0",  (0x40011000, 0x0002): "PC1",
    (0x40011000, 0x0004): "PC2",  (0x40011000, 0x0008): "PC3",
    (0x40011000, 0x0010): "PC4",  (0x40011000, 0x0020): "PC5",
    (0x40011000, 0x0040): "PC6",  (0x40011000, 0x0080): "PC7",
    (0x40011000, 0x0100): "PC8",  (0x40011000, 0x0200): "PC9",
    (0x40011000, 0x0400): "PC10", (0x40011000, 0x0800): "PC11",
    (0x40011000, 0x1000): "PC12", (0x40011000, 0x2000): "PC13",
    (0x40011000, 0x4000): "PC14", (0x40011000, 0x8000): "PC15",
    (0x40011400, 0x0001): "PD0",  (0x40011400, 0x0002): "PD1",
    (0x40011400, 0x0004): "PD2",  (0x40011400, 0x0008): "PD3",
    (0x40011400, 0x0010): "PD4",  (0x40011400, 0x0020): "PD5",
    (0x40011400, 0x0040): "PD6",  (0x40011400, 0x0080): "PD7",
    (0x40011400, 0x0100): "PD8",  (0x40011400, 0x0200): "PD9",
    (0x40011400, 0x0400): "PD10", (0x40011400, 0x0800): "PD11",
    (0x40011400, 0x1000): "PD12", (0x40011400, 0x2000): "PD13",
    (0x40011400, 0x4000): "PD14", (0x40011400, 0x8000): "PD15",
}

RCU_APB2EN_BITS = {
    0: "AFEN", 2: "PAEN", 3: "PBEN", 4: "PCEN", 5: "PDEN", 6: "PEEN",
    8: "ADC0EN", 9: "ADC1EN", 10: "ADC2EN", 11: "TIMER0EN",
    12: "SPI0EN", 14: "USART0EN",
}
RCU_APB1EN_BITS = {
    0: "TIMER1EN", 1: "TIMER2EN", 2: "TIMER3EN", 3: "TIMER4EN",
    4: "TIMER5EN", 5: "TIMER6EN",
    14: "SPI1EN", 15: "SPI2EN",
    17: "USART1EN", 18: "USART2EN", 19: "UART3EN", 20: "UART4EN",
    21: "I2C0EN", 22: "I2C1EN",
    25: "CAN0EN", 27: "BKPEN", 28: "PMUEN", 29: "DACEN",
}

# ── Helpers ────────────────────────────────────────────────────────────

def resolve_peripheral(addr):
    """Try to name a peripheral address, including base+offset."""
    if addr in PERIPHERAL_MAP:
        return PERIPHERAL_MAP[addr]
    # Check base + offset pattern
    for base_name, base_addr, max_off in [
        ("SPI3",  0x40003C00, 0x20), ("SPI2",  0x40003800, 0x20),
        ("USART", 0x40004400, 0x20),
        ("GPIOA", 0x40010800, 0x20), ("GPIOB", 0x40010C00, 0x20),
        ("GPIOC", 0x40011000, 0x20), ("GPIOD", 0x40011400, 0x20),
        ("RCU",   0x40021000, 0x30), ("AFIO",  0x40010000, 0x10),
        ("DMA0",  0x40020000, 0x80),
        ("TIM2",  0x40000000, 0x50), ("TIM3",  0x40000400, 0x50),
        ("TIM4",  0x40000800, 0x50), ("TIM5",  0x40000C00, 0x50),
        ("ADC1",  0x40012400, 0x60), ("ADC2",  0x40012800, 0x60),
        ("NVIC",  0xE000E000, 0x1000),
    ]:
        if base_addr <= addr < base_addr + max_off:
            return f"{base_name}+0x{addr - base_addr:02X}"
    if addr in RAM_MAP:
        return RAM_MAP[addr]
    return None

def resolve_ram(addr):
    """Name a RAM address."""
    if addr in RAM_MAP:
        return RAM_MAP[addr]
    # Check within known structures
    if 0x200000F8 <= addr < 0x200000F8 + 0x40:
        return f"meter_state[0x{addr - 0x200000F8:02X}]"
    if 0x20002D6C <= addr <= 0x20002D90:
        return f"queue_handles[0x{addr - 0x20002D6C:02X}]"
    if 0x20000000 <= addr < 0x20010000:
        return f"RAM_{addr:08X}"
    return None

def pin_from_val(gpio_base, val):
    """Identify GPIO pin from BOP/BC value."""
    key = (gpio_base, val)
    if key in GPIO_PIN_NAMES:
        return GPIO_PIN_NAMES[key]
    # Try to decode multi-pin
    pins = []
    for bit in range(16):
        if val & (1 << bit):
            k = (gpio_base, 1 << bit)
            if k in GPIO_PIN_NAMES:
                pins.append(GPIO_PIN_NAMES[k])
    return "|".join(pins) if pins else f"pin_mask=0x{val:04X}"

def rcu_bit_name(reg_addr, bit_val):
    """Name RCU enable bits."""
    bits_map = None
    if reg_addr == 0x40021018:
        bits_map = RCU_APB2EN_BITS
    elif reg_addr == 0x4002101C:
        bits_map = RCU_APB1EN_BITS
    if bits_map:
        names = []
        for bit, name in bits_map.items():
            if bit_val & (1 << bit):
                names.append(name)
        if names:
            return " | ".join(names)
    return None

def gpio_config_decode(mode_speed, config_bits):
    """Decode GPIO CTL register field values."""
    modes = {0: "input", 1: "out_10MHz", 2: "out_2MHz", 3: "out_50MHz"}
    in_configs = {0: "analog", 1: "floating", 2: "pull-up/down"}
    out_configs = {0: "push-pull", 1: "open-drain", 2: "AF push-pull", 3: "AF open-drain"}
    mode = modes.get(mode_speed, f"mode={mode_speed}")
    if mode_speed == 0:
        cfg = in_configs.get(config_bits, f"cfg={config_bits}")
    else:
        cfg = out_configs.get(config_bits, f"cfg={config_bits}")
    return f"{mode}, {cfg}"


class Decompiler:
    def __init__(self, data, base_addr):
        self.data = data
        self.base = base_addr
        self.md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
        self.md.detail = True

        # Register tracking state
        self.regs = {}       # reg_name -> known value
        self.reg_movw = {}   # reg_name -> (addr, low16) waiting for MOVT
        self.output_lines = []

        # Track peripheral access patterns
        self.last_ldr_reg = None
        self.last_ldr_addr = None

        # Annotation buffer
        self.annotations = {}  # addr -> list of annotation strings
        self.branch_targets = set()

    def disasm_range(self, start, end):
        """Disassemble a range and return instruction list."""
        off_start = start - self.base
        off_end = end - self.base
        code = self.data[off_start:off_end]
        return list(self.md.disasm(code, start))

    def first_pass(self, instructions):
        """First pass: collect branch targets, MOVW/MOVT pairs."""
        regs = {}
        for insn in instructions:
            mn = insn.mnemonic
            ops = insn.op_str

            # Track branch targets for labels
            if mn in ('b', 'bne', 'beq', 'blt', 'bge', 'bgt', 'ble',
                       'bhi', 'bls', 'bcc', 'bcs', 'bpl', 'bmi',
                       'bvs', 'bvc', 'b.w', 'bne.w', 'beq.w', 'blt.w',
                       'bge.w', 'bgt.w', 'ble.w', 'bhi.w', 'bls.w',
                       'bcc.w', 'bcs.w'):
                try:
                    target = int(ops.lstrip('#'), 0)
                    self.branch_targets.add(target)
                except:
                    pass
            elif mn == 'cbz' or mn == 'cbnz':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        target = int(parts[1].strip().lstrip('#'), 0)
                        self.branch_targets.add(target)
                    except:
                        pass

            # Track MOVW/MOVT pairs
            if mn == 'movw':
                parts = ops.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        regs[reg] = (insn.address, val, 'movw')
                    except:
                        pass
            elif mn == 'movt':
                parts = ops.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        if reg in regs and regs[reg][2] == 'movw':
                            full = (val << 16) | regs[reg][1]
                            regs[reg] = (insn.address, full, 'full')
                            if insn.address not in self.annotations:
                                self.annotations[insn.address] = []
                            name = resolve_peripheral(full) or resolve_ram(full) or ""
                            note = f"{reg} = 0x{full:08X}"
                            if name:
                                note += f"  ({name})"
                            self.annotations[insn.address].append(note)
                    except:
                        pass

    def second_pass(self, instructions):
        """Main annotation pass with full register tracking."""
        regs = {}        # reg -> known value
        reg_src = {}     # reg -> description
        lines = []

        for i, insn in enumerate(instructions):
            addr = insn.address
            mn = insn.mnemonic
            ops = insn.op_str
            annotations = list(self.annotations.get(addr, []))

            # ── Register tracking ──────────────────────────────────
            if mn == 'movw':
                parts = ops.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        regs[reg] = val
                        reg_src[reg] = f"0x{val:04X} (low16)"
                    except:
                        pass

            elif mn == 'movt':
                parts = ops.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        if reg in regs:
                            full = (val << 16) | (regs[reg] & 0xFFFF)
                            regs[reg] = full
                            name = resolve_peripheral(full) or resolve_ram(full)
                            if name:
                                reg_src[reg] = name
                            else:
                                reg_src[reg] = f"0x{full:08X}"
                    except:
                        pass

            elif mn in ('mov', 'mov.w'):
                parts = ops.split(',')
                if len(parts) == 2:
                    dst = parts[0].strip()
                    src = parts[1].strip()
                    if src.startswith('#'):
                        try:
                            val = int(src.lstrip('#'), 0)
                            regs[dst] = val
                            reg_src[dst] = f"0x{val:X}"
                        except:
                            pass
                    elif src in regs:
                        regs[dst] = regs[src]
                        reg_src[dst] = reg_src.get(src, src)

            elif mn == 'movs':
                parts = ops.split(',')
                if len(parts) == 2:
                    dst = parts[0].strip()
                    src = parts[1].strip()
                    if src.startswith('#'):
                        try:
                            val = int(src.lstrip('#'), 0)
                            regs[dst] = val
                            reg_src[dst] = f"0x{val:X}"
                        except:
                            pass

            elif mn == 'add.w':
                # add.w rd, rn, rm, lsl #x  -- too complex, just note
                pass

            elif mn == 'ldr':
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    dst = parts[0].strip()
                    src = parts[1].strip()
                    # ldr rX, [rY, #offset]
                    if '[' in src:
                        import re
                        m = re.match(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', src)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs:
                                eff_addr = regs[base_reg] + offset
                                name = resolve_peripheral(eff_addr) or resolve_ram(eff_addr)
                                if name:
                                    annotations.append(f"load from {name} (0x{eff_addr:08X})")
                    # ldr from literal pool
                    if dst in regs:
                        del regs[dst]  # value unknown after load

            elif mn in ('ldr.w',):
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    dst = parts[0].strip()
                    src = parts[1].strip()
                    if '[' in src:
                        import re
                        m = re.match(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', src)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs:
                                eff_addr = regs[base_reg] + offset
                                name = resolve_peripheral(eff_addr) or resolve_ram(eff_addr)
                                if name:
                                    annotations.append(f"load from {name} (0x{eff_addr:08X})")
                    if dst in regs:
                        del regs[dst]

            elif mn in ('str', 'str.w'):
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    src_reg = parts[0].strip()
                    dst = parts[1].strip()
                    if '[' in dst:
                        import re
                        m = re.match(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', dst)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs:
                                eff_addr = regs[base_reg] + offset
                                name = resolve_peripheral(eff_addr) or resolve_ram(eff_addr)
                                if name:
                                    val_str = ""
                                    if src_reg in regs:
                                        val_str = f" = 0x{regs[src_reg]:X}"
                                    annotations.append(f"store to {name} (0x{eff_addr:08X}){val_str}")
                                    # Special: BOP/BC pin operations
                                    if "BOP" in name and src_reg in regs:
                                        gpio_base_addr = eff_addr & 0xFFFFF800
                                        if eff_addr & 0x1F == 0x10:
                                            pin = pin_from_val(gpio_base_addr, regs[src_reg])
                                            annotations.append(f"  -> SET pin {pin}")
                                    elif "BC" in name and src_reg in regs:
                                        gpio_base_addr = eff_addr & 0xFFFFF800
                                        if eff_addr & 0x1F == 0x14:
                                            pin = pin_from_val(gpio_base_addr, regs[src_reg])
                                            annotations.append(f"  -> CLEAR pin {pin}")
                                    # Special: RCU enable bits
                                    if "RCU" in name and ("APB2EN" in name or "APB1EN" in name):
                                        pass  # ORR annotations handle this

            elif mn in ('strb', 'strb.w'):
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    src_reg = parts[0].strip()
                    dst = parts[1].strip()
                    if '[' in dst:
                        import re
                        m = re.match(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', dst)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs:
                                eff_addr = regs[base_reg] + offset
                                name = resolve_peripheral(eff_addr) or resolve_ram(eff_addr)
                                if name:
                                    val_str = ""
                                    if src_reg in regs:
                                        val_str = f" = 0x{regs[src_reg]:X}"
                                    annotations.append(f"store byte to {name} (0x{eff_addr:08X}){val_str}")

            elif mn in ('strh', 'strh.w'):
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    src_reg = parts[0].strip()
                    dst = parts[1].strip()
                    if '[' in dst:
                        import re
                        m = re.match(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', dst)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs:
                                eff_addr = regs[base_reg] + offset
                                name = resolve_peripheral(eff_addr) or resolve_ram(eff_addr)
                                if name:
                                    val_str = ""
                                    if src_reg in regs:
                                        val_str = f" = 0x{regs[src_reg]:X}"
                                    annotations.append(f"store halfword to {name} (0x{eff_addr:08X}){val_str}")

            elif mn == 'orr':
                parts = ops.split(',')
                if len(parts) >= 3:
                    dst = parts[0].strip()
                    src1 = parts[1].strip()
                    src2 = parts[2].strip()
                    if src2.startswith('#'):
                        try:
                            val = int(src2.lstrip('#'), 0)
                            annotations.append(f"  {dst} |= 0x{val:X}")
                        except:
                            pass

            elif mn == 'bic':
                parts = ops.split(',')
                if len(parts) >= 3:
                    dst = parts[0].strip()
                    src2 = parts[2].strip()
                    if src2.startswith('#'):
                        try:
                            val = int(src2.lstrip('#'), 0)
                            annotations.append(f"  {dst} &= ~0x{val:X}")
                        except:
                            pass

            # ── Function calls ─────────────────────────────────────
            elif mn == 'bl':
                try:
                    target = int(ops.lstrip('#'), 0)
                    fname = KNOWN_FUNCTIONS.get(target, f"FUN_{target:08x}")
                    annotations.append(f"CALL {fname}")

                    # Annotate gpio_init calls
                    if target == 0x080302fc:
                        # r0 = gpio base, r1 = config struct ptr (on stack)
                        if 'r0' in regs:
                            gpio = regs['r0']
                            name = resolve_peripheral(gpio)
                            if name:
                                annotations.append(f"  gpio_init({name}, ...)")

                    # Annotate spi_init calls
                    elif target == 0x08036848:
                        if 'r0' in regs:
                            spi = regs['r0']
                            name = resolve_peripheral(spi)
                            if name:
                                annotations.append(f"  spi_init({name}, ...)")

                    # Annotate xQueueCreate
                    elif target == 0x0803ab74:
                        if 'r0' in regs and 'r1' in regs:
                            annotations.append(f"  xQueueCreate(length={regs['r0']}, itemSize={regs['r1']})")

                    # Annotate xTaskCreate
                    elif target in (0x0803b6a0, 0x0803bd88):
                        if 'r2' in regs:
                            annotations.append(f"  stack={regs['r2']} words")

                    # Annotate vTaskDelay
                    elif target in (0x0803a53c, 0x0803a390):
                        if 'r0' in regs:
                            annotations.append(f"  delay({regs['r0']} ticks)")

                except:
                    pass

            # ── Branches ───────────────────────────────────────────
            elif mn.startswith('b') and mn not in ('bl', 'bic', 'bfc', 'bfi', 'bx'):
                try:
                    # Extract target from last operand
                    parts = ops.split(',')
                    target_str = parts[-1].strip().lstrip('#')
                    target = int(target_str, 0)
                    if target in KNOWN_FUNCTIONS:
                        annotations.append(f"-> {KNOWN_FUNCTIONS[target]}")
                except:
                    pass

            # ── Build output line ──────────────────────────────────
            label = ""
            if addr in self.branch_targets:
                label = f"\nloc_{addr:08X}:"

            asm_str = f"  {addr:08X}:  {mn:10s} {ops}"
            if annotations:
                ann_str = "  ; " + " | ".join(annotations)
                asm_str += ann_str

            if label:
                lines.append(label)
            lines.append(asm_str)

        return lines

    def detect_code_vs_data(self, start, end):
        """Identify likely data regions (literal pools) within code."""
        # Disassemble and look for unreachable/nonsensical sequences
        data_regions = []
        off_s = start - self.base
        off_e = end - self.base

        # Check for common patterns: after unconditional branch, before next push/label
        instructions = self.disasm_range(start, end)
        after_branch = False
        data_start = None

        for insn in instructions:
            if after_branch:
                # Check if this looks like data (common: short literal values)
                if insn.mnemonic in ('push', 'push.w') or insn.address in self.branch_targets:
                    if data_start:
                        data_regions.append((data_start, insn.address))
                    after_branch = False
                    data_start = None
            if insn.mnemonic == 'b' or insn.mnemonic == 'b.w':
                after_branch = True
                data_start = insn.address + insn.size

        return data_regions

    def generate_pseudocode_header(self):
        """Generate the file header."""
        return [
            "=" * 80,
            "FNIRSI 2C53T V1.2.0 - FPGA/Acquisition Init Function Decompilation",
            "=" * 80,
            "",
            "Function: FUN_08023A50  (the massive FPGA/acquisition init)",
            "Range:    0x08023A50 - 0x080276F2  (~15.4 KB of code + data)",
            "Focus:    0x08025800 - 0x08027700  (user's requested range)",
            "",
            "This function:",
            "  - Configures GPIO pins for SPI3 (PB3-PB6), USART, timers, FPGA control",
            "  - Sets up SPI3 peripheral for FPGA ADC data transfer",
            "  - Configures USART for FPGA command channel",
            "  - Configures DMA for efficient SPI transfers",
            "  - Creates FreeRTOS queues for inter-task communication",
            "  - Creates FreeRTOS tasks (FPGA task, acquisition, measurement)",
            "  - Performs FPGA initialization handshake sequence",
            "  - Configures NVIC interrupts",
            "  - Sets up SysTick for timing",
            "",
            "Register allocation (observed at function entry):",
            "  r5  = RCU base area (0x40021018 = RCU_APB2EN)",
            "  r6  = GPIO port base (varies)",
            "  r7  = timer/peripheral base (varies)",
            "  r8  = NVIC/SysTick area",
            "  r9  = bit position / counter",
            "  r10 = GPIO port base (varies)",
            "  r11 = peripheral register pointer",
            "  fp  = frame pointer (r11 alias, or stack frame)",
            "",
            "Peripheral addresses:",
            "  SPI3:  0x40003C00  (FPGA data bus on PB3/PB4/PB5)",
            "  USART: 0x40004400  (FPGA command channel)",
            "  GPIOB: 0x40010C00  (PB3=SCK, PB4=MISO, PB5=MOSI, PB6=CS)",
            "  RCU:   0x40021000  (clock enables)",
            "  AFIO:  0x40010000  (pin remapping for JTAG->SPI3)",
            "",
            "-" * 80,
            "",
        ]

    def run(self):
        """Main entry point."""
        with open(BIN_PATH, "rb") as f:
            data = f.read()

        self.data = data

        # ── Phase 1: Full function context (0x08023A50 - 0x08025800) ──
        print("Phase 1: Disassembling full function prologue (0x08023A50-0x08025800)...")
        prologue_insns = self.disasm_range(FUNC_START, RANGE_START)

        # ── Phase 2: Main range of interest (0x08025800 - 0x08027700) ──
        print("Phase 2: Disassembling main range (0x08025800-0x08027700)...")
        main_insns = self.disasm_range(RANGE_START, RANGE_END)

        all_insns = prologue_insns + main_insns

        # ── First pass: collect metadata ──
        print("First pass: collecting branch targets and register values...")
        self.first_pass(all_insns)

        # ── Second pass: generate annotated output ──
        print("Second pass: generating annotations...")

        output = self.generate_pseudocode_header()

        # Prologue section (condensed)
        output.append("=" * 80)
        output.append("SECTION 1: FUNCTION PROLOGUE (0x08023A50 - 0x08025800)")
        output.append("  Context for register state entering the main section")
        output.append("=" * 80)
        output.append("")

        prologue_lines = self.second_pass(prologue_insns)
        output.extend(prologue_lines)

        output.append("")
        output.append("=" * 80)
        output.append("SECTION 2: MAIN FPGA INIT (0x08025800 - 0x08027700)")
        output.append("  This is the critical section with FPGA handshake,")
        output.append("  SPI3 setup, queue/task creation, USART commands")
        output.append("=" * 80)
        output.append("")

        main_lines = self.second_pass(main_insns)
        output.extend(main_lines)

        # ── Phase 3: Generate high-level pseudocode ──
        print("Phase 3: Generating high-level pseudocode...")
        pseudocode = self.generate_high_level_pseudocode()
        # Insert after header, before assembly
        output_final = self.generate_pseudocode_header() + pseudocode
        # Re-add the assembly sections
        output_final.append("")
        output_final.append("=" * 80)
        output_final.append("DETAILED ANNOTATED DISASSEMBLY")
        output_final.append("=" * 80)
        output_final.append("")
        # Rebuild with assembly (skip the header we already added)
        header_len = len(self.generate_pseudocode_header())
        output_final.extend(output[header_len:])

        output = output_final

        # ── Phase 4: Generate summary ──
        print("Phase 4: Generating summary analysis...")
        output.append("")
        output.append("")
        output.extend(self.generate_summary(all_insns))

        # Write output
        text = "\n".join(output) + "\n"
        Path(OUTPUT_PATH).parent.mkdir(parents=True, exist_ok=True)
        with open(OUTPUT_PATH, "w") as f:
            f.write(text)

        print(f"\nOutput written to: {OUTPUT_PATH}")
        print(f"Total lines: {len(output)}")
        print(f"Total instructions: {len(all_insns)}")

    def generate_high_level_pseudocode(self):
        """Generate high-level C-like pseudocode interpretation."""
        # Read binary for literal pool decoding
        base = self.base

        # Decode task name strings from literal pool
        pc_offsets = [
            (0x08025C88, 0xEB0, "xTaskCreate_v2", "0x080400B9", 10, "0x20002D88"),
            (0x08025CA4, 0xE9C, "xTaskCreate_v2", "0x080406C9", 1000, "0x20002D8C"),
            (0x08025CCE, 0xE7C, "xTaskCreate",    "0x0803DA51", 384, "0x20002D90"),
            (0x08025CEE, 0xE64, "xTaskCreate",    "0x08040009", 128, "0x20002D94"),
            (0x08025D0C, 0xE48, "xTaskCreate",    "0x0804009D", 256, "0x20002D98"),
            (0x08025D2C, 0xE2C, "xTaskCreate",    "0x0803E455", 128, "0x20002D9C"),
            (0x08025D4A, 0xE18, "xTaskCreate",    "0x0803E3F5", 64,  "0x20002DA0"),
            (0x08025D68, 0xE00, "xTaskCreate",    "0x0803DAC1", 128, "0x20002DA4"),
        ]

        task_names = []
        for pc_addr, offset, create_fn, func_addr, stack, handle_addr in pc_offsets:
            pc = (pc_addr + 4) & ~3
            str_addr = pc + offset
            off = str_addr - base
            s = ""
            for i in range(32):
                b = self.data[off + i]
                if b == 0:
                    break
                if 32 <= b < 127:
                    s += chr(b)
            task_names.append((s, func_addr, stack, handle_addr, create_fn))

        lines = [
            "=" * 80,
            "HIGH-LEVEL PSEUDOCODE INTERPRETATION",
            "=" * 80,
            "",
            "// ============================================================",
            "// FUN_08023A50: System initialization (FPGA, peripherals, RTOS)",
            "// This is the master init function called during boot.",
            "// It configures ALL hardware peripherals and creates ALL tasks.",
            "// ============================================================",
            "",
            "void system_init(void) {",
            "",
            "    // === SECTION 1: GPIO AND CLOCK INIT (0x08023A50 - 0x08025800) ===",
            "",
            "    // Enable GPIOC clock (RCU_APB2EN |= bit4)",
            "    RCU_APB2EN |= 0x10;",
            "",
            "    // Configure PC9 as output (power hold) and CLEAR it initially",
            "    gpio_init(GPIOC, pin=PC9, mode=output_50MHz, config=push_pull);",
            "    GPIOC_BC = (1 << 9);  // Clear PC9 (power hold LOW initially)",
            "",
            "    // Enable GPIOA, GPIOB clocks",
            "    RCU_APB2EN |= 0x04;  // GPIOA",
            "    RCU_APB2EN |= 0x08;  // GPIOB",
            "",
            "    // Extensive GPIO configuration for multiple peripherals...",
            "    // (47 calls to gpio_init in total across the full function)",
            "    // Configures: oscilloscope analog front-end, LCD, buttons,",
            "    // FPGA control lines, SPI flash, USB, debug UART, etc.",
            "",
            "    // Configure AFIO pin remapping:",
            "    //   AFIO_PCF0 &= ~0xF000  (clear JTAG remap bits 15:12)",
            "    //   AFIO_PCF0 |= 0x2000   (JTAG-DP disabled, SW-DP enabled)",
            "    //   This frees PB3/PB4/PB5 from JTAG for SPI3 use!",
            "",
            "    // NVIC priority group configuration",
            "    // Read SCB_AIRCR, extract PRIGROUP, configure interrupt priority",
            "",
            "",
            "    // === SECTION 2: MAIN FPGA INIT (0x08025800 - 0x08027700) ===",
            "",
            "    // --- 2a: NVIC interrupt enables ---",
            "    // NVIC_ISER0 |= 0x200  (enable IRQ9 = EXTI9_5 or DMA)",
            "    NVIC_ISER0 = 0x200;",
            "",
            "    // --- 2b: Enable SPI3 and USART clocks ---",
            "    RCU_APB1EN |= 0x20000;   // bit17 = USART1EN (USART1 clock enable)",
            "    RCU_APB2EN |= 0x04;      // GPIOA (already enabled, just ensuring)",
            "",
            "    // --- 2c: Timer5 (TIM5) configuration ---",
            "    // At 0x080258A0: timer_init() called with TIM5 (0x40000C00)",
            "    // Calculates prescaler from APB1 clock frequency",
            "    // The math: (APB1freq * 10) / some_divisor, round-up correction",
            "    timer_init(TIM5_base);  // 0x40000C00",
            "",
            "    // DMA0 interrupt configuration",
            "    // DMA0_INTC = 0x0F;  (clear DMA channel 0 flags)",
            "",
            "    // --- 2d: Another timer configuration ---",
            "    // At 0x08025980: timer_init() called again",
            "    timer_init(...);",
            "",
            "    // --- 2e: USART configuration (0x40004400 = USART2?) ---",
            "    // USART_CTL0: disable TX/RX, configure word length",
            "    // USART_CTL1: configure stop bits",
            "    // USART_BRR: set baud rate",
            "    // USART_CTL0: enable USART, enable TX, enable RX",
            "",
            "    // --- 2f: meter_state initialization ---",
            "    // meter_state_base (0x200000F8) fields set to defaults",
            "    // Various calibration/configuration values loaded",
            "",
            "",
            "    // === QUEUE CREATION (0x08025BFC - 0x08025C7A) ===",
            "",
            "    queue_handle_1  = xQueueCreate(20, 1);   // 0x20002D6C - 20 x 1-byte items",
            "    queue_handle_2  = xQueueCreate(15, 1);   // 0x20002D70 - 15 x 1-byte items",
            "    queue_handle_3  = xQueueCreate(10, 2);   // 0x20002D74 - 10 x 2-byte items",
            "    spi3_data_queue = xQueueCreate(15, 1);   // 0x20002D78 - 15 x 1-byte (FPGA SPI3 data)",
            "    queue_handle_5  = xQueueCreate(1, 0);    // 0x20002D7C - binary semaphore",
            "    queue_handle_6  = xQueueCreate(1, 0);    // 0x20002D80 - binary semaphore",
            "    queue_handle_7  = xQueueCreate(1, 0);    // 0x20002D84 - binary semaphore",
            "",
            "",
            "    // === TASK CREATION (0x08025C80 - 0x08025D74) ===",
            "",
        ]

        # Add task creation details
        for name, func, stack, handle, create_fn in task_names:
            prio_map = {
                "Timer1": 10, "Timer2": 1000, "display": 1, "key": 4,
                "osc": 2, "fpga": 3, "dvom_TX": 2, "dvom_RX": 3,
            }
            prio = prio_map.get(name, "?")
            lines.append(f'    // {create_fn}("{name}", {func}, stack={stack}w, prio={prio})')
            lines.append(f'    //   handle -> {handle}')

        lines.extend([
            "",
            "    // Signal queue_handle_6 (semaphore give)",
            "    xQueueGenericSend(queue_handle_6, NULL, 0);",
            "",
            "    // Suspend tasks initially",
            "    vTaskSuspend(queue_handle_5);  // suspend via handle at 0x20002D7C",
            "    vTaskSuspend(queue_handle_7);  // suspend via handle at 0x20002D84",
            "",
            "",
            "    // === USART TX COMMAND LOOP (0x08025D96 - 0x08025EFC) ===",
            "    // Sends initialization commands to FPGA via USART",
            "    // Pattern: build 10-byte command in usart_tx_buf (0x20000005),",
            "    //          then transmit byte-by-byte checking USART_STS for TX empty",
            "",
            "    // Command format appears to be:",
            "    //   byte[0] = command ID",
            "    //   byte[1..9] = parameters",
            "    //   Transmitted MSB-first through USART_DATA register",
            "",
            "",
            "    // === SPI3 INIT AND FPGA HANDSHAKE (0x08026540 - 0x080268D0) ===",
            "",
            "    // Configure SPI3 GPIO pins:",
            "    //   PB3 = SPI3_SCK  (AF push-pull, 50MHz)",
            "    //   PB4 = SPI3_MISO (input floating)",
            "    //   PB5 = SPI3_MOSI (AF push-pull, 50MHz)",
            "    //   PB6 = SPI3_CS   (GPIO output, active low)",
            "",
            "    // GPIOC PC6 = FPGA control line (set HIGH = FPGA select)",
            "    GPIOC_BOP = (1 << 6);  // Set PC6 HIGH",
            "",
            "    // SPI3 peripheral init:",
            "    //   spi_init(SPI3, {",
            "    //     prescaler = 0x100,     // clock divider",
            "    //     CPOL/CPHA = ...,",
            "    //     frame = 8-bit,",
            "    //     MSB first,",
            "    //     NSS = software,",
            "    //   });",
            "    //   SPI3_CTL1 |= 0x02;  // enable RX buffer not empty interrupt",
            "    //   SPI3_CTL1 |= 0x01;  // enable TX buffer empty interrupt (maybe DMA?)",
            "    //   SPI3_CTL0 |= 0x40;  // SPE = SPI enable",
            "",
            "    // SysTick delay (10ms from systick_reload_1)",
            "    SysTick_LOAD = systick_reload_1 * 10;",
            "    SysTick_VAL  = 0;",
            "    SysTick_CTL |= 1;  // enable",
            "    while (!(SysTick_CTL & 0x10000));  // wait for COUNTFLAG",
            "",
            "    // === FPGA SPI3 HANDSHAKE SEQUENCE ===",
            "    // Repeated pattern: wait TX empty -> write DATA -> wait RX ready -> read DATA",
            "",
            "    // Transaction 1: Send 0x00, receive response",
            "    spi3_tx_rx(0x00);  // -> reads back FPGA ID/status byte",
            "",
            "    // Transaction 2: Send 0x05, receive response",
            "    spi3_tx_rx(0x05);  // -> reads back FPGA version/config",
            "",
            "    // Transaction 3: Send 0x00, receive response",
            "    spi3_tx_rx(0x00);",
            "",
            "    // Transaction 4: Send 0x00, receive response",
            "    spi3_tx_rx(0x00);",
            "",
            "    // Compare received values, branch on FPGA response",
            "    // This determines FPGA firmware version / capabilities",
            "",
            "    // Additional SPI3 transactions (0x080268E0+):",
            "    //   Send 0x12, receive response",
            "    //   More handshake bytes...",
            "",
            "",
            "    // === LARGE STATE MACHINE / FPGA CONFIG LOOP (0x08026900 - 0x08027000) ===",
            "    // This section has many conditional branches and appears to:",
            "    //   - Send configuration bytes to FPGA via SPI3",
            "    //   - Read back status/config responses",
            "    //   - Set up ADC sampling parameters",
            "    //   - Configure trigger levels and timebase",
            "    //   - Initialize display buffer pointers (0x20008340, 0x20008360)",
            "",
            "",
            "    // === DMA CONFIGURATION (around 0x08027000) ===",
            "    // Configure DMA for SPI3 data transfers",
            "    // DMA0 channel used for efficient ADC data bulk reads from FPGA",
            "",
            "",
            "    // === TIMER7 / TIMER6 CONFIGURATION (0x08027100+) ===",
            "    timer_init(TIM7?);  // at 0x08027100",
            "    timer_init(TIM6?);  // at 0x08027140",
            "",
            "    // Configure meter measurement functions",
            "    FUN_080018a4(meter_state[2]);  // meter mode select",
            "    FUN_08001a58(meter_state[3]);  // meter range select",
            "",
            "",
            "    // === WATCHDOG TIMER INIT (0x080275A0+) ===",
            "    // FWDGT (Free Watchdog Timer) at 0x40003000:",
            "    //   FWDGT_CTL = 0x5555  (unlock write access)",
            "    //   FWDGT_PSC: set prescaler to /64 (bits 2:0 = 4)",
            "    //   FWDGT_RLD = 0x04E1  (reload value = 1249 -> ~5 sec timeout)",
            "    //   FWDGT_CTL = 0xAAAA  (reload counter)",
            "    //   FWDGT_CTL = 0xCCCC  (start watchdog)",
            "",
            "    // Enable TIM6 clock (RCU_APB1EN |= 0x40 = bit6 = TIMER5EN)",
            "    RCU_APB1EN |= 0x40;",
            "",
            "    // Configure TIM6/7 registers",
            "    timer_init(...);",
            "",
            "    // Final NVIC configuration",
            "    // NVIC_ISER1 |= 0x800  (enable IRQ43 = TIM7?)",
            "    NVIC_ISER1 = 0x800;",
            "",
            "    // Start timer",
            "    TIM_CTL0 |= 1;  // CEN = counter enable",
            "",
            "    // Configure timer task handles",
            "    // FUN_0803b768 + FUN_0803be38: likely vTimerSetTimerID / xTimerStart",
            "    // Applied to queue_handle_8 (Timer1 handle) and queue_handle_9 (Timer2 handle)",
            "",
            "    // Epilogue: restore stack, pop registers",
            "    // Tail-calls into 0x0803A6D8 (likely vTaskStartScheduler or similar)",
            "}",
            "",
            "",
            "// ============================================================",
            "// SPI3 TX/RX helper (inlined pattern, not a real function):",
            "// ============================================================",
            "//",
            "// uint8_t spi3_tx_rx(uint8_t tx_byte) {",
            "//     while (!(SPI3_STAT & 0x02));  // Wait TXE (TX buffer empty)",
            "//     SPI3_DATA = tx_byte;            // Write TX data",
            "//     while (!(SPI3_STAT & 0x01));  // Wait RXNE (RX buffer not empty)",
            "//     return SPI3_DATA;               // Read RX data",
            "// }",
            "",
            "",
            "// ============================================================",
            "// USART TX helper (inlined pattern):",
            "// ============================================================",
            "//",
            "// void usart_send_command(uint8_t *buf, int len) {",
            "//     for (int i = 0; i < len; i++) {",
            "//         while (!(USART_STS & 0x80));  // Wait TXE",
            "//         USART_DATA = buf[i];",
            "//     }",
            "//     while (!(USART_STS & 0x40));  // Wait TC (transmission complete)",
            "// }",
            "",
            "",
            "// ============================================================",
            "// TASK MAP (name -> entry point -> purpose):",
            "// ============================================================",
            "//",
            "//   Timer1    0x080400B9  Software timer task #1 (high priority=10)",
            "//   Timer2    0x080406C9  Software timer task #2 (priority=1000??)",
            "//   display   0x0803DA51  Display refresh task (priority=1, stack=384w=1536B)",
            "//   key       0x08040009  Button/key input task (priority=4, stack=128w=512B)",
            "//   osc       0x0804009D  Oscilloscope acquisition task (priority=2, stack=256w=1024B)",
            "//   fpga      0x0803E455  FPGA communication task (priority=3, stack=128w=512B)",
            "//   dvom_TX   0x0803E3F5  Multimeter TX task (priority=2, stack=64w=256B)",
            "//   dvom_RX   0x0803DAC1  Multimeter RX task (priority=3, stack=128w=512B)",
            "",
            "",
            "// ============================================================",
            "// QUEUE MAP (handle address -> purpose):",
            "// ============================================================",
            "//",
            "//   0x20002D6C  queue_handle_1   xQueueCreate(20, 1)  -- 20 byte items (key events?)",
            "//   0x20002D70  queue_handle_2   xQueueCreate(15, 1)  -- 15 byte items (commands?)",
            "//   0x20002D74  queue_handle_3   xQueueCreate(10, 2)  -- 10 halfword items (measurements?)",
            "//   0x20002D78  spi3_data_queue  xQueueCreate(15, 1)  -- 15 byte items (FPGA SPI3 data)",
            "//   0x20002D7C  queue_handle_5   xQueueCreate(1, 0)   -- binary semaphore",
            "//   0x20002D80  queue_handle_6   xQueueCreate(1, 0)   -- binary semaphore",
            "//   0x20002D84  queue_handle_7   xQueueCreate(1, 0)   -- binary semaphore",
            "",
            "",
            "// ============================================================",
            "// KEY GPIO PIN ASSIGNMENTS (from prologue GPIO config):",
            "// ============================================================",
            "//",
            "//   PC9  = Power hold (output, push-pull)",
            "//   PB8  = LCD backlight (output, push-pull)",
            "//   PC6  = FPGA SPI3 chip select / control",
            "//   PB3  = SPI3_SCK  (JTAG remapped to SPI3)",
            "//   PB4  = SPI3_MISO (JTAG remapped to SPI3)",
            "//   PB5  = SPI3_MOSI (JTAG remapped to SPI3)",
            "//   PB6  = SPI3 software CS (GPIO output)",
            "",
            "",
        ])
        return lines

    def generate_summary(self, all_insns):
        """Generate analysis summary section."""
        lines = [
            "=" * 80,
            "ANALYSIS SUMMARY",
            "=" * 80,
            "",
        ]

        # Collect all BL targets
        bl_targets = {}
        for insn in all_insns:
            if insn.mnemonic == 'bl':
                try:
                    target = int(insn.op_str.lstrip('#'), 0)
                    name = KNOWN_FUNCTIONS.get(target, f"FUN_{target:08x}")
                    if target not in bl_targets:
                        bl_targets[target] = []
                    bl_targets[target].append(insn.address)
                except:
                    pass

        lines.append("FUNCTION CALLS (sorted by address):")
        lines.append("-" * 60)
        for target in sorted(bl_targets.keys()):
            name = KNOWN_FUNCTIONS.get(target, f"FUN_{target:08x}")
            calls = bl_targets[target]
            lines.append(f"  {target:08X}  {name:30s}  called {len(calls)}x")
            for c in calls[:5]:
                lines.append(f"    from 0x{c:08X}")
            if len(calls) > 5:
                lines.append(f"    ... and {len(calls)-5} more")
        lines.append("")

        # Collect all resolved MOVW/MOVT addresses
        lines.append("RESOLVED PERIPHERAL/RAM ADDRESSES:")
        lines.append("-" * 60)
        resolved = set()
        regs = {}
        for insn in all_insns:
            if insn.mnemonic == 'movw':
                parts = insn.op_str.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        regs[reg] = val
                    except:
                        pass
            elif insn.mnemonic == 'movt':
                parts = insn.op_str.split(',')
                if len(parts) == 2:
                    reg = parts[0].strip()
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        if reg in regs:
                            full = (val << 16) | regs[reg]
                            regs[reg] = full
                            name = resolve_peripheral(full) or resolve_ram(full)
                            if name and full not in resolved:
                                resolved.add(full)
                                lines.append(f"  0x{full:08X}  {name}")
                    except:
                        pass
        lines.append("")

        # GPIO operations summary
        lines.append("GPIO PIN OPERATIONS (BOP=set, BC=clear):")
        lines.append("-" * 60)
        regs2 = {}
        for insn in all_insns:
            mn = insn.mnemonic
            ops = insn.op_str
            if mn == 'movw':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        regs2[parts[0].strip()] = int(parts[1].strip().lstrip('#'), 0)
                    except:
                        pass
            elif mn == 'movt':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        reg = parts[0].strip()
                        if reg in regs2:
                            regs2[reg] = (val << 16) | (regs2[reg] & 0xFFFF)
                    except:
                        pass
            elif mn in ('str', 'str.w'):
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    import re
                    m = re.match(r'\s*\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', parts[1])
                    if m:
                        base_reg = m.group(1)
                        offset = int(m.group(2), 0) if m.group(2) else 0
                        src_reg = parts[0].strip()
                        if base_reg in regs2:
                            eff = regs2[base_reg] + offset
                            # Check BOP (offset 0x10) or BC (offset 0x14)
                            for gpio_name, gpio_base in [("GPIOA", 0x40010800), ("GPIOB", 0x40010C00),
                                                          ("GPIOC", 0x40011000), ("GPIOD", 0x40011400)]:
                                if eff == gpio_base + 0x10 and src_reg in regs2:
                                    pin = pin_from_val(gpio_base, regs2[src_reg])
                                    lines.append(f"  0x{insn.address:08X}: {gpio_name} SET {pin}")
                                elif eff == gpio_base + 0x14 and src_reg in regs2:
                                    pin = pin_from_val(gpio_base, regs2[src_reg])
                                    lines.append(f"  0x{insn.address:08X}: {gpio_name} CLEAR {pin}")
            elif mn in ('mov', 'mov.w', 'movs'):
                parts = ops.split(',')
                if len(parts) == 2:
                    dst = parts[0].strip()
                    src = parts[1].strip()
                    if src.startswith('#'):
                        try:
                            regs2[dst] = int(src.lstrip('#'), 0)
                        except:
                            pass
                    elif src in regs2:
                        regs2[dst] = regs2[src]
        lines.append("")

        # SPI3 register accesses
        lines.append("SPI3 REGISTER ACCESSES:")
        lines.append("-" * 60)
        spi3_accesses = []
        regs3 = {}
        for insn in all_insns:
            mn = insn.mnemonic
            ops = insn.op_str
            if mn == 'movw':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        regs3[parts[0].strip()] = int(parts[1].strip().lstrip('#'), 0)
                    except:
                        pass
            elif mn == 'movt':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        reg = parts[0].strip()
                        if reg in regs3:
                            regs3[reg] = (val << 16) | (regs3[reg] & 0xFFFF)
                    except:
                        pass
            elif mn in ('ldr', 'ldr.w', 'str', 'str.w', 'strh', 'strh.w', 'ldrh', 'ldrh.w'):
                import re
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    for part in parts:
                        m = re.search(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', part)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs3:
                                eff = regs3[base_reg] + offset
                                if 0x40003C00 <= eff <= 0x40003C1F:
                                    reg_name = {0x40003C00: "CTL0", 0x40003C04: "CTL1",
                                                0x40003C08: "STAT", 0x40003C0C: "DATA"}.get(eff, f"+0x{eff-0x40003C00:02X}")
                                    op = "READ" if 'ldr' in mn else "WRITE"
                                    lines.append(f"  0x{insn.address:08X}: SPI3.{reg_name} {op}")
        lines.append("")

        # USART register accesses
        lines.append("USART REGISTER ACCESSES:")
        lines.append("-" * 60)
        regs4 = {}
        for insn in all_insns:
            mn = insn.mnemonic
            ops = insn.op_str
            if mn == 'movw':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        regs4[parts[0].strip()] = int(parts[1].strip().lstrip('#'), 0)
                    except:
                        pass
            elif mn == 'movt':
                parts = ops.split(',')
                if len(parts) == 2:
                    try:
                        val = int(parts[1].strip().lstrip('#'), 0)
                        reg = parts[0].strip()
                        if reg in regs4:
                            regs4[reg] = (val << 16) | (regs4[reg] & 0xFFFF)
                    except:
                        pass
            elif mn in ('ldr', 'ldr.w', 'str', 'str.w', 'strh', 'strh.w', 'ldrh', 'ldrh.w',
                        'strb', 'strb.w', 'ldrb', 'ldrb.w'):
                import re
                parts = ops.split(',', 1)
                if len(parts) == 2:
                    for part in parts:
                        m = re.search(r'\[(\w+)(?:,\s*#(0x[\da-fA-F]+|\d+))?\]', part)
                        if m:
                            base_reg = m.group(1)
                            offset = int(m.group(2), 0) if m.group(2) else 0
                            if base_reg in regs4:
                                eff = regs4[base_reg] + offset
                                if 0x40004400 <= eff <= 0x4000441F:
                                    reg_name = {0x40004400: "STS", 0x40004404: "DATA",
                                                0x40004408: "BRR", 0x4000440C: "CTL0",
                                                0x40004410: "CTL1"}.get(eff, f"+0x{eff-0x40004400:02X}")
                                    op = "READ" if 'ldr' in mn else "WRITE"
                                    lines.append(f"  0x{insn.address:08X}: USART.{reg_name} {op}")
        lines.append("")

        # Queue/task creation
        lines.append("QUEUE AND TASK CREATION:")
        lines.append("-" * 60)
        for insn in all_insns:
            if insn.mnemonic == 'bl':
                try:
                    target = int(insn.op_str.lstrip('#'), 0)
                    if target == 0x0803ab74:
                        lines.append(f"  0x{insn.address:08X}: xQueueCreate()")
                    elif target in (0x0803b6a0, 0x0803bd88):
                        lines.append(f"  0x{insn.address:08X}: xTaskCreate()")
                except:
                    pass
        lines.append("")

        # String references (look for task name strings in literal pool)
        lines.append("LITERAL POOL / DATA REGIONS:")
        lines.append("-" * 60)
        # Check bytes at likely data areas after unconditional branches
        for insn in all_insns:
            if insn.mnemonic in ('b', 'b.w'):
                # Check if bytes after this look like ASCII
                off = insn.address + insn.size - self.base
                if 0 <= off < len(self.data) - 8:
                    chunk = self.data[off:off+32]
                    # Check if it's printable ASCII
                    ascii_str = ""
                    for b in chunk:
                        if 32 <= b < 127:
                            ascii_str += chr(b)
                        elif b == 0 and len(ascii_str) >= 4:
                            lines.append(f"  0x{insn.address + insn.size:08X}: string \"{ascii_str}\"")
                            break
                        else:
                            break
                    # Also dump as words (addresses/constants)
                    if not ascii_str or len(ascii_str) < 4:
                        words = []
                        for j in range(0, min(16, len(chunk)), 4):
                            w = struct.unpack_from('<I', chunk, j)[0]
                            words.append(f"0x{w:08X}")
                        if any(w.startswith("0x0802") or w.startswith("0x0803") or w.startswith("0x2000") or w.startswith("0x4000") for w in words):
                            lines.append(f"  0x{insn.address + insn.size:08X}: data words: {' '.join(words)}")

        return lines


if __name__ == "__main__":
    d = Decompiler(None, BASE_ADDR)
    d.run()
