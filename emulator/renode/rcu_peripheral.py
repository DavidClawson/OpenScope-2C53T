# GD32F307 RCU (Reset and Clock Unit) - Python Peripheral for Renode
#
# Register map (GD32F307, STM32F1-compatible with extensions):
#   0x00 RCU_CTL     - Control register (HSI/HSE/PLL ready bits)
#   0x04 RCU_CFG0    - Clock configuration 0
#   0x08 RCU_INT     - Clock interrupt register
#   0x0C RCU_APB2RST - APB2 reset register
#   0x10 RCU_APB1RST - APB1 reset register
#   0x14 RCU_AHBEN   - AHB enable register
#   0x18 RCU_APB2EN  - APB2 peripheral clock enable
#   0x1C RCU_APB1EN  - APB1 peripheral clock enable
#   0x20 RCU_BDCTL   - Backup domain control register
#   0x24 RCU_RSTSCK  - Reset source / clock register
#   0x28 RCU_AHBRST  - AHB reset register (GD32 extension)
#   0x2C RCU_CFG1    - Clock configuration 1 (GD32 extension)
#   0x34 RCU_DSV     - Deep-sleep voltage register
#   0xC0 RCU_ADDCTL  - Additional clock control (GD32 extension)
#   0xCC RCU_ADDINT  - Additional clock interrupt
#   0xE0 RCU_ADDAPB1EN - Additional APB1 enable
#   0xEC RCU_ADDAPB1RST - Additional APB1 reset
#
# Key bits in RCU_CTL (0x00):
#   bit 0:  HSIEN (HSI enable)
#   bit 1:  HSISTB (HSI stable/ready) - must be 1
#   bit 16: HSEEN (HSE enable)
#   bit 17: HSESTB (HSE stable/ready) - must be 1
#   bit 24: PLLEN (PLL enable)
#   bit 25: PLLSTB (PLL stable/ready) - must be 1
#   bit 26: PLL1EN
#   bit 27: PLL1STB - must be 1
#   bit 28: PLL2EN
#   bit 29: PLL2STB - must be 1
#
# Key bits in RCU_CFG0 (0x04):
#   bits 3:2: SWS (system clock switch status) - 0b10 = PLL selected
#   bits 1:0: SW  (system clock switch) - firmware writes 0b10 for PLL

if request.IsInit:
    # Register storage (offset -> value)
    regs = {}
    # RCU_CTL: All oscillators ready, PLL locked
    # HSI ready(1) | HSE ready(17) | PLL ready(25) | PLL1 ready(27) | PLL2 ready(29)
    regs[0x00] = 0x3F0A0083
    # RCU_CFG0: PLL as system clock (SWS=0b10), AHB/APB prescalers configured
    regs[0x04] = 0x001D840A
    # RCU_INT: No interrupts pending
    regs[0x08] = 0x00000000
    # RCU_APB2RST
    regs[0x0C] = 0x00000000
    # RCU_APB1RST
    regs[0x10] = 0x00000000
    # RCU_AHBEN: DMA, SRAM clock enabled
    regs[0x14] = 0x00000014
    # RCU_APB2EN: GPIO clocks etc
    regs[0x18] = 0x00007E7D
    # RCU_APB1EN: USART2, SPI2, timers, etc
    regs[0x1C] = 0x10EE4807
    # RCU_BDCTL
    regs[0x20] = 0x00000000
    # RCU_RSTSCK: LSI ready, reset flags
    regs[0x24] = 0x0C000002
    # RCU_AHBRST
    regs[0x28] = 0x00000000
    # RCU_CFG1: PLL prescalers
    regs[0x2C] = 0x00000000
    # RCU_DSV
    regs[0x34] = 0x00000000
    # RCU_ADDCTL: IRC48M ready
    regs[0xC0] = 0x00020000
    # RCU_ADDINT
    regs[0xCC] = 0x00000000
    # RCU_ADDAPB1EN
    regs[0xE0] = 0x00000000
    # RCU_ADDAPB1RST
    regs[0xEC] = 0x00000000

elif request.IsRead:
    off = request.Offset & 0x3FC  # Align to 4 bytes
    if off in regs:
        request.Value = regs[off]
    else:
        request.Value = 0x00000000

    # Auto-set ready bits when enable bits are written
    if off == 0x00:
        val = regs.get(0x00, 0)
        # If HSI enabled, set HSI ready
        if val & (1 << 0):
            val |= (1 << 1)
        # If HSE enabled, set HSE ready
        if val & (1 << 16):
            val |= (1 << 17)
        # If PLL enabled, set PLL ready
        if val & (1 << 24):
            val |= (1 << 25)
        # If PLL1 enabled, set PLL1 ready
        if val & (1 << 26):
            val |= (1 << 27)
        # If PLL2 enabled, set PLL2 ready
        if val & (1 << 28):
            val |= (1 << 29)
        regs[0x00] = val
        request.Value = val

elif request.IsWrite:
    off = request.Offset & 0x3FC
    val = request.Value

    if off == 0x00:  # RCU_CTL
        # Auto-set ready/stable bits for any enabled oscillator
        if val & (1 << 0):   # HSIEN
            val |= (1 << 1)  # HSISTB
        if val & (1 << 16):  # HSEEN
            val |= (1 << 17) # HSESTB
        if val & (1 << 24):  # PLLEN
            val |= (1 << 25) # PLLSTB
        if val & (1 << 26):  # PLL1EN
            val |= (1 << 27) # PLL1STB
        if val & (1 << 28):  # PLL2EN
            val |= (1 << 29) # PLL2STB
        regs[0x00] = val

    elif off == 0x04:  # RCU_CFG0
        # When firmware writes SW bits, mirror them to SWS
        sw = val & 0x3  # SW field
        val = (val & ~0xC) | (sw << 2)  # Set SWS = SW
        regs[0x04] = val

    elif off == 0x24:  # RCU_RSTSCK
        if val & (1 << 0):  # LSIEN
            val |= (1 << 1)  # LSISTB
        regs[0x24] = val

    elif off == 0xC0:  # RCU_ADDCTL
        if val & (1 << 16):  # IRC48MEN
            val |= (1 << 17)  # IRC48MSTB
        regs[0xC0] = val

    else:
        regs[off] = val
