# GD32F307 FMC (Flash Memory Controller) - Python Peripheral for Renode
#
# Register map:
#   0x00 FMC_WS     - Wait state register
#   0x04 FMC_KEY    - Unlock key register (write-only)
#   0x08 FMC_OBKEY  - Option byte unlock key (write-only)
#   0x0C FMC_STAT   - Status register
#   0x10 FMC_CTL    - Control register
#   0x14 FMC_ADDR   - Address register
#   0x18 (reserved)
#   0x1C FMC_OBSTAT - Option byte status register
#   0x20 FMC_WP     - Write protection register
#   0x100 FMC_PID   - Product ID (GD32 extension)
#
# FMC_STAT bits:
#   bit 0: BUSY (flash busy) - should be 0 when idle
#   bit 2: PGERR (programming error)
#   bit 4: WPERR (write protection error)
#   bit 5: ENDF (end of operation flag)
#
# FMC_CTL bits:
#   bit 0: PG (programming)
#   bit 1: PER (page erase)
#   bit 2: MER (mass erase)
#   bit 4: OBPG (option byte programming)
#   bit 5: OBER (option byte erase)
#   bit 6: START (start erase)
#   bit 7: LK (lock)
#   bit 9: OBWEN (option byte write enable) - set when OB unlocked
#   bit 12: ENDIE (end of operation interrupt enable)
#   bit 10: ERRIE (error interrupt enable)
#
# FMC_OBSTAT bits:
#   bit 0: OBERR (option byte error)
#   bit 1: SPC (security protection)
#   bits 2-9: USER (user option byte)
#   bits 10-25: DATA (data option byte)

if request.IsInit:
    regs = {}
    regs[0x00] = 0x00000032  # FMC_WS: prefetch enabled, 3 wait states
    regs[0x04] = 0x00000000  # FMC_KEY: write-only
    regs[0x08] = 0x00000000  # FMC_OBKEY: write-only
    regs[0x0C] = 0x00000000  # FMC_STAT: not busy, no errors
    regs[0x10] = 0x00000280  # FMC_CTL: locked (bit 7), OBWEN set (bit 9)
    regs[0x14] = 0x00000000  # FMC_ADDR
    regs[0x1C] = 0x03FFFFFC  # FMC_OBSTAT: default option bytes (no SPC, default user/data)
    regs[0x20] = 0xFFFFFFFF  # FMC_WP: all sectors writable (no write protection)
    regs[0x100] = 0x00000000 # FMC_PID

    unlock_state = 0  # 0=locked, 1=key1 received, 2=unlocked
    ob_unlock_state = 0

elif request.IsRead:
    off = request.Offset & 0x1FC
    if off == 0x0C:  # FMC_STAT
        # Always return "not busy" and "end of operation" set
        request.Value = 0x00000020  # ENDF set, BUSY clear
    elif off == 0x10:  # FMC_CTL
        # Return with OBWEN (bit 9) always set so OB operations don't hang
        val = regs.get(0x10, 0x00000280)
        # Ensure OBWEN is set
        val |= (1 << 9)
        request.Value = val
    elif off in regs:
        request.Value = regs[off]
    else:
        request.Value = 0x00000000

elif request.IsWrite:
    off = request.Offset & 0x1FC
    val = request.Value

    if off == 0x04:  # FMC_KEY - unlock sequence
        if val == 0x45670123:
            unlock_state = 1
        elif val == 0xCDEF89AB and unlock_state == 1:
            unlock_state = 2
            # Clear lock bit
            ctl = regs.get(0x10, 0x00000280)
            ctl &= ~(1 << 7)  # Clear LK
            regs[0x10] = ctl
        else:
            unlock_state = 0

    elif off == 0x08:  # FMC_OBKEY - option byte unlock
        if val == 0x45670123:
            ob_unlock_state = 1
        elif val == 0xCDEF89AB and ob_unlock_state == 1:
            ob_unlock_state = 2
            # Set OBWEN bit
            ctl = regs.get(0x10, 0x00000280)
            ctl |= (1 << 9)  # Set OBWEN
            regs[0x10] = ctl
        else:
            ob_unlock_state = 0

    elif off == 0x0C:  # FMC_STAT - writing 1 clears flags
        stat = regs.get(0x0C, 0)
        stat &= ~(val & 0x00000034)  # Clear PGERR, WPERR, ENDF on write-1
        regs[0x0C] = stat

    elif off == 0x10:  # FMC_CTL
        if val & (1 << 6):  # START bit - start erase
            # Erase completes immediately, set ENDF
            regs[0x0C] = 0x00000020  # ENDF set
            val &= ~(1 << 6)  # Clear START
        if val & (1 << 7):  # LK - re-lock
            unlock_state = 0
        regs[0x10] = val

    elif off == 0x00:  # FMC_WS
        regs[0x00] = val

    else:
        regs[off] = val
