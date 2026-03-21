# GD32F307 ADC Peripheral - Python Peripheral for Renode
#
# Register map (GD32F307 ADC):
#   0x00 ADC_STAT   - Status register
#   0x04 ADC_CTL0   - Control register 0
#   0x08 ADC_CTL1   - Control register 1
#   0x0C ADC_SAMPT0 - Sample time register 0
#   0x10 ADC_SAMPT1 - Sample time register 1
#   0x14-0x28 ADC_IOFFx - Injected channel data offset
#   0x2C ADC_WDHT   - Watchdog high threshold
#   0x30 ADC_WDLT   - Watchdog low threshold
#   0x34 ADC_RSQ0   - Regular sequence register 0
#   0x38 ADC_RSQ1   - Regular sequence register 1
#   0x3C ADC_RSQ2   - Regular sequence register 2
#   0x40 ADC_ISQ    - Injected sequence register
#   0x44-0x50 ADC_IDATAx - Injected data registers
#   0x4C ADC_RDATA  - Regular data register
#
# Key auto-clear bits in ADC_CTL1 (offset 0x08):
#   bit 0: ADCON (ADC on) - stays set
#   bit 2: CLB (Calibration) - auto-clears when calibration done
#   bit 3: RSTCLB (Reset calibration) - auto-clears when reset done
#   bit 22: SWRCST (Start regular conversion by software) - auto-clears
#
# ADC_STAT bits (offset 0x00):
#   bit 1: EOC (end of conversion)
#   bit 3: STIC (start of injected conversion) - should be 0 when idle

if request.IsInit:
    regs = {}
    for i in range(0, 0x100, 4):
        regs[i] = 0
    regs[0x4C] = 0x00000800  # ADC_RDATA: mid-range for 12-bit

elif request.IsRead:
    off = request.Offset & 0xFC
    if off == 0x00:  # ADC_STAT
        # Return EOC set (conversion always complete), no other flags
        request.Value = 0x00000002
    elif off == 0x08:  # ADC_CTL1
        val = regs.get(0x08, 0)
        # Auto-clear calibration bits (bit 2 CLB, bit 3 RSTCLB)
        val &= ~((1 << 2) | (1 << 3))
        # Auto-clear software start bit (bit 22 SWRCST)
        val &= ~(1 << 22)
        regs[0x08] = val
        request.Value = val
    elif off == 0x4C:  # ADC_RDATA
        request.Value = regs.get(0x4C, 0x0800)
    elif off in regs:
        request.Value = regs[off]
    else:
        request.Value = 0

elif request.IsWrite:
    off = request.Offset & 0xFC
    val = request.Value
    regs[off] = val

    if off == 0x08:  # ADC_CTL1
        # When calibration/reset is started, it completes instantly
        # The read handler will auto-clear these bits
        pass
    elif off == 0x00:  # ADC_STAT - writing clears flags
        pass
