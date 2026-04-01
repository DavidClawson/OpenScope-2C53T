#!/usr/bin/env python3
"""Search the entire V1.2.0 firmware for SPI3 peripheral accesses.

Looks for MOVW/MOVT pairs that construct addresses in the SPI3 range (0x40003C00-0x40003C20).
Also searches for the AFIO remap (JTAG disable) and SPI3 clock enable.
"""

from capstone import *
import struct

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

TARGETS = {
    # SPI3 registers
    0x40003C00: "SPI3_CTL0",
    0x40003C04: "SPI3_CTL1",
    0x40003C08: "SPI3_STAT",
    0x40003C0C: "SPI3_DATA",
    # AFIO remap (for JTAG disable)
    0x40010004: "AFIO_PCF0",
    # RCU clock enables
    0x4002101C: "RCU_APB1EN",  # SPI3 clock is on APB1
    0x40021010: "RCU_APB1RST", # APB1 reset
    # GPIOB config (for SPI3 pins PB3/4/5/6)
    0x40010C00: "GPIOB_CTL0",
    # USART1 (GD32 naming) at 0x40004400 = command USART
    0x40004400: "USART1_STS",
    0x40004404: "USART1_DATA",
    0x40004408: "USART1_BRR",
    0x4000440C: "USART1_CTL0",
}

def main():
    with open(FIRMWARE, "rb") as f:
        fw = f.read()

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True

    # Track MOVW values per register
    movw_vals = {}
    results = []

    print(f"Scanning {len(fw)} bytes for SPI3/AFIO/RCU peripheral accesses...")
    print()

    for insn in md.disasm(fw, BASE_ADDR):
        addr = insn.address
        mnemonic = insn.mnemonic
        op_str = insn.op_str

        if mnemonic == "movw":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    movw_vals[reg] = (int(parts[1], 0), addr)
                except ValueError:
                    pass

        elif mnemonic == "movt":
            parts = op_str.split(", #")
            if len(parts) == 2:
                reg = parts[0].strip()
                try:
                    high = int(parts[1], 0)
                    if reg in movw_vals:
                        low, movw_addr = movw_vals[reg]
                        full = (high << 16) | low
                        if full in TARGETS:
                            results.append((movw_addr, addr, reg, full, TARGETS[full]))
                except ValueError:
                    pass

    # Print results grouped by target
    by_target = {}
    for movw_addr, movt_addr, reg, val, name in results:
        by_target.setdefault(name, []).append((movw_addr, movt_addr, reg, val))

    for name in sorted(by_target.keys()):
        hits = by_target[name]
        print(f"{name} ({hits[0][3]:#010x}) — {len(hits)} references:")
        for movw_addr, movt_addr, reg, val in hits:
            print(f"  MOVW at {movw_addr:#010x}, MOVT at {movt_addr:#010x} -> {reg}")
        print()

    # Now disassemble around each SPI3_CTL0 reference to find the init config
    if "SPI3_CTL0" in by_target:
        print("=" * 70)
        print("DISASSEMBLY AROUND SPI3_CTL0 REFERENCES")
        print("=" * 70)
        for movw_addr, movt_addr, reg, val in by_target["SPI3_CTL0"]:
            # Disassemble ±64 bytes around the MOVW
            start = movw_addr - 64
            offset = start - BASE_ADDR
            code = fw[offset:offset + 256]
            print(f"\n--- Context around {movw_addr:#010x} ---")
            for insn in md.disasm(code, start):
                if insn.address > movt_addr + 64:
                    break
                marker = " >>>" if insn.address in (movw_addr, movt_addr) else "    "
                print(f"{marker} {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}")

    # Also check for AFIO_PCF0 context
    if "AFIO_PCF0" in by_target:
        print()
        print("=" * 70)
        print("DISASSEMBLY AROUND AFIO_PCF0 (JTAG REMAP) REFERENCES")
        print("=" * 70)
        for movw_addr, movt_addr, reg, val in by_target["AFIO_PCF0"]:
            start = movw_addr - 32
            offset = start - BASE_ADDR
            code = fw[offset:offset + 192]
            print(f"\n--- Context around {movw_addr:#010x} ---")
            for insn in md.disasm(code, start):
                if insn.address > movt_addr + 96:
                    break
                marker = " >>>" if insn.address in (movw_addr, movt_addr) else "    "
                print(f"{marker} {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}")

if __name__ == "__main__":
    main()
