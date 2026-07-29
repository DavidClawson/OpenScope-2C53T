#!/usr/bin/env python3
"""Hunt stock firmware for a RECONFIG_N-style GPIO pulse before CONFIG_ENABLE.

Background: apicula (docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md) says a running,
auto-booted GW1N only re-enters configuration after RECONFIG_N goes low (>=25ns)
or a power cycle. Our firmware's CONFIG_ENABLE (0x15) is ignored; stock's is not.
If stock asserts such a trigger it must appear as a GPIO write in its init path
BEFORE the config-enable instruction at flash 0x0802DA42.

The Exp E SWD register diff CANNOT find this: it is a static snapshot, and a pin
pulsed LOW then released reads HIGH at the config-enable instant, identical to a
pin merely held HIGH. Only static analysis (or a logic analyzer) can see the edge.

On AT32/STM32F1 GPIO, from the port base:
    +0x0C = ODR        +0x10 = BSRR (scr, set)      +0x14 = BRR (clr, reset)

A pulse is therefore a write to +0x14 (or a BSRR upper-half reset) followed by a
write to +0x10 on the same port.

Usage:
    scripts/find_gpio_pulses.py <stock.bin> [--link-base 0x08007000]

NOTE: base registers are resolved by the disassembler only as register NAMES.
Mapping r4/r6/r7/sl to an actual GPIO port needs dataflow — use the Ghidra
project for that. This script narrows the search to a handful of instructions.
"""
import argparse
import re
import subprocess
import sys

# Flash address of `movs r0,#0x15` — the CONFIG_ENABLE opcode load in stock V1.2.0.
CONFIG_ENABLE_ADDR = 0x0802DA42
# Start of master init (FUN_08023A50).
MASTER_INIT_ADDR = 0x08023A50

OFF_NAME = {12: "ODR", 16: "scr/BSRR", 20: "clr/BRR"}
STR_RE = re.compile(
    r"^\s*([0-9a-f]{6,8}):\s+[0-9a-f ]+\s+str(?:\.w)?\s+(\w+), \[(\w+)(?:, #(\d+))?\]"
)


def disassemble(path, link_base):
    out = subprocess.run(
        ["arm-none-eabi-objdump", "-D", "-b", "binary", "-marm",
         "-Mforce-thumb", f"--adjust-vma={link_base:#x}", path],
        capture_output=True, text=True, check=True,
    )
    return out.stdout.splitlines()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--link-base", type=lambda s: int(s, 0), default=0x08007000)
    ap.add_argument("--lo", type=lambda s: int(s, 0), default=MASTER_INIT_ADDR)
    ap.add_argument("--hi", type=lambda s: int(s, 0), default=CONFIG_ENABLE_ADDR)
    args = ap.parse_args()

    hits = []
    for line in disassemble(args.binary, args.link_base):
        m = STR_RE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        off = int(m.group(4) or 0)
        if off not in OFF_NAME:
            continue
        src, base = m.group(2), m.group(3)
        # `sp` relative stores are stack frame traffic, never GPIO.
        if base == "sp":
            continue
        hits.append((addr, src, base, off))

    window = [h for h in hits if args.lo <= h[0] < args.hi]
    print(f"GPIO-shaped stores (ODR/BSRR/BRR), non-sp base:")
    print(f"  whole image : {len(hits)}")
    print(f"  0x{args.lo:08X}..0x{args.hi:08X} : {len(window)}\n")

    for addr, src, base, off in window:
        flag = "   <-- drives a pin LOW" if off == 20 else ""
        print(f"  0x{addr:08X}  str {src},[{base},#{off}]  {OFF_NAME[off]}{flag}")

    lows = [h for h in window if h[3] == 20]
    print(f"\n{len(lows)} write(s) drive a pin LOW before CONFIG_ENABLE.")
    print("Resolve each base register to a port in Ghidra; any that is later set")
    print("HIGH before 0x%08X is a RECONFIG_N candidate." % args.hi)
    return 0


if __name__ == "__main__":
    sys.exit(main())
