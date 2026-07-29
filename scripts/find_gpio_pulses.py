#!/usr/bin/env python3
"""Hunt stock firmware for a RECONFIG_N-style GPIO pulse before CONFIG_ENABLE.

Background: apicula (docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md) says a running,
auto-booted GW1N only re-enters configuration after RECONFIG_N goes low (>=25ns)
or a power cycle. Exp N (2026-07-28) measured exactly that behaviour on our own
bench: every SSPI read command answers correctly while not one config command
moves the STATUS register. If stock asserts such a trigger it must appear as a
GPIO write before the config-enable instruction.

WHAT CHANGED (2026-07-28, after Exp O)
--------------------------------------
Two defects in the previous version, both of which mattered:

1. It started the scan at MASTER_INIT and so could never see anything stock does
   BEFORE master init. That is not a hypothetical gap — stock_pre_fpga_gpio_state.md
   records that PC9's power-hold is asserted by the reset/clock-init stub at file
   offset 0, long before master init runs. Early hardware init is exactly where a
   RECONFIG_N pulse would live. The scan now covers the whole image up to
   CONFIG_ENABLE, reset stub included.

2. It reported any store to offset +0x0C/+0x10/+0x14 as a GPIO access without
   resolving the base register. Every peripheral has those offsets, so the
   2026-07-27 run produced 5 "candidates" of which 3 were timers (TMR7, TMR13).
   Base registers are now resolved by backward dataflow — literal-pool loads
   (read out of the binary), movw/movt pairs, register moves and constant adds —
   and a hit is only reported if the resolved address really is a GPIO port
   register. Anything unresolvable is reported SEPARATELY as such, never silently
   dropped and never counted as a hit.

Usage:
    scripts/find_gpio_pulses.py <stock.bin> [--link-base 0x08007000]
    scripts/find_gpio_pulses.py <stock.bin> --show-unresolved
"""
import argparse
import re
import struct
import subprocess
import sys

# TRUE-flash address of `movs r0,#0x15` — the CONFIG_ENABLE opcode load.
# Address convention: doc/Ghidra = file_offset + 0x08000000, TRUE flash =
# file_offset + 0x08007000. This is a TRUE-flash address, matching --link-base.
CONFIG_ENABLE_ADDR = 0x0802DA42

GPIO_PORTS = {
    0x40010800: "GPIOA",
    0x40010C00: "GPIOB",
    0x40011000: "GPIOC",
    0x40011400: "GPIOD",
    0x40011800: "GPIOE",
}
# Only registers whose write changes a pin LEVEL. CRL/CRH (mode) and LCKR are
# not pulses; IDR is read-only.
GPIO_REGS = {0x0C: "ODR", 0x10: "scr/BSRR (set HIGH)", 0x14: "clr/BRR (set LOW)"}

INSN_RE = re.compile(r"^\s*([0-9a-f]{4,8}):\s+((?:[0-9a-f]{2,4} ?)+)\s+(\S+)\s*(.*)$")
STR_RE = re.compile(r"^str(?:\.w|b|h)?$")
LDR_PC_RE = re.compile(r"^(\w+), \[pc, #\d+\]\s*@ \(0x([0-9a-f]+)\)")
MEM_OP_RE = re.compile(r"^(\w+), \[(\w+)(?:, #(-?\d+))?\]")

BACKTRACK = 80          # instructions to walk back looking for the base's definition


def disassemble(path, link_base):
    out = subprocess.run(
        ["arm-none-eabi-objdump", "-D", "-b", "binary", "-marm",
         "-Mforce-thumb", f"--adjust-vma={link_base:#x}", path],
        capture_output=True, text=True, check=True,
    )
    insns = []
    for line in out.stdout.splitlines():
        m = INSN_RE.match(line)
        if m:
            insns.append((int(m.group(1), 16), m.group(3), m.group(4).split(";")[0].strip(), line.rstrip()))
    return insns


def literal(blob, link_base, addr):
    """Read the 4-byte literal-pool constant at a TRUE-flash address."""
    off = addr - link_base
    if 0 <= off <= len(blob) - 4:
        return struct.unpack_from("<I", blob, off)[0]
    return None


def resolve_base(insns, idx, reg, blob, link_base):
    """Walk backwards from insns[idx] resolving `reg` to a constant address.

    Returns (value, None) on success or (None, reason) when the value cannot be
    established. Deliberately conservative: any write to the tracked register
    that this cannot model exactly aborts, rather than guessing. A wrong base is
    worse than no base -- that is what produced the timer false positives.
    """
    offset = 0
    tracked = reg
    high = None            # movt half, seen BEFORE the movw when walking backwards
    for j in range(idx - 1, max(-1, idx - BACKTRACK) - 1, -1):
        addr, mnem, ops, _ = insns[j]

        # Literal-pool load: ldr rX, [pc, #N] @ (0xADDR)
        m = LDR_PC_RE.match(ops)
        if m and mnem.startswith("ldr") and m.group(1) == tracked:
            val = literal(blob, link_base, int(m.group(2), 16))
            return (None, "literal out of range") if val is None else (val + offset, None)

        parts = [p.strip() for p in ops.split("@")[0].split(",")]
        if not parts or parts[0] != tracked:
            continue

        # movt supplies the TOP half. Walking backwards we meet it before the
        # movw that supplies the bottom half, so remember it and keep going.
        # Treating movt as an opaque write is what made this miss every
        # movw/movt-built GPIO base, including the known PC9 write at 0x0802AAB6.
        if mnem == "movt" and len(parts) == 2 and parts[1].startswith("#"):
            try:
                high = int(parts[1][1:], 0)
            except ValueError:
                return None, f"unparsed movt at {addr:#x}"
            continue

        # movw / movs / mov.w with an immediate — the bottom half, or the whole
        # value when no movt accompanies it.
        if mnem in ("movw", "mov.w", "movs", "mov") and len(parts) == 2 and parts[1].startswith("#"):
            try:
                base = int(parts[1][1:], 0)
            except ValueError:
                return None, f"unparsed immediate at {addr:#x}"
            if high is not None:
                base = (base & 0xFFFF) | (high << 16)
            return (base + offset, None)

        # Register move: keep walking, now tracking the source.
        if mnem in ("mov", "mov.w", "movs") and len(parts) == 2 and re.fullmatch(r"[a-z0-9]+", parts[1]):
            tracked = parts[1]
            continue

        # add rX, rY, #imm  /  adds rX, #imm  -- fold the constant in.
        if mnem in ("add", "add.w", "adds") and parts[-1].startswith("#"):
            try:
                offset += int(parts[-1][1:], 0)
            except ValueError:
                return None, f"unparsed add at {addr:#x}"
            if len(parts) == 3 and re.fullmatch(r"[a-z0-9]+", parts[1]):
                tracked = parts[1]
            continue

        # Anything else that writes the tracked register: give up honestly.
        return None, f"{mnem} at {addr:#x}"

    return None, "not found within backtrack window"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--link-base", type=lambda s: int(s, 0), default=0x08007000)
    ap.add_argument("--lo", type=lambda s: int(s, 0), default=None,
                    help="scan start (default: image start, i.e. the reset stub)")
    ap.add_argument("--hi", type=lambda s: int(s, 0), default=CONFIG_ENABLE_ADDR)
    ap.add_argument("--show-unresolved", action="store_true")
    args = ap.parse_args()

    blob = open(args.binary, "rb").read()
    lo = args.lo if args.lo is not None else args.link_base

    insns = disassemble(args.binary, args.link_base)
    print(f"disassembled {len(insns)} instructions")
    print(f"scanning 0x{lo:08X}..0x{args.hi:08X}  "
          f"({'whole image up to CONFIG_ENABLE, reset stub included' if args.lo is None else 'custom range'})\n")

    hits, unresolved = [], []
    for i, (addr, mnem, ops, raw) in enumerate(insns):
        if not (lo <= addr < args.hi) or not STR_RE.match(mnem):
            continue
        m = MEM_OP_RE.match(ops)
        if not m:
            continue
        base_reg, off = m.group(2), int(m.group(3) or 0)
        if base_reg == "sp":
            continue

        val, why = resolve_base(insns, i, base_reg, blob, args.link_base)
        if val is None:
            unresolved.append((addr, raw, why))
            continue

        target = val + off
        port_base = target & ~0x3FF
        reg_off = target & 0x3FF
        if port_base in GPIO_PORTS and reg_off in GPIO_REGS:
            # Resolve the STORED VALUE too — the pin mask. Without it a LOW and a
            # HIGH on the same port cannot be paired into a pulse, which is the
            # whole point of the scan. Same backward dataflow, on the source reg.
            mask, _ = resolve_base(insns, i, m.group(1), blob, args.link_base)
            hits.append((addr, GPIO_PORTS[port_base], GPIO_REGS[reg_off], reg_off, raw, mask))

    def pins(mask):
        if mask is None:
            return "?"
        return ",".join(f"{i}" for i in range(16) if mask >> i & 1) or f"{mask:#x}"

    print(f"=== GPIO level writes before CONFIG_ENABLE: {len(hits)} ===")
    for addr, port, reg, reg_off, raw, mask in hits:
        mark = "  <-- LOW" if reg_off == 0x14 else ""
        print(f"  0x{addr:08X}  {port} {reg:<22} pins {pins(mask)}{mark}")

    # Pair LOW then HIGH on the same port+pin: that is the shape of a pulse.
    print("\n=== pulse candidates (a LOW, then a HIGH, same port+pin) ===")
    found = 0
    for li, (la, lport, _, lo_off, _, lmask) in enumerate(hits):
        if lo_off != 0x14 or lmask is None:
            continue
        for (ha, hport, _, hi_off, _, hmask) in hits[li + 1:]:
            if hi_off == 0x10 and hport == lport and hmask is not None and (hmask & lmask):
                common = hmask & lmask
                print(f"  {lport} pin(s) {pins(common)}:  LOW 0x{la:08X}  ->  HIGH 0x{ha:08X}")
                found += 1
                break
    if not found:
        print("  none — every LOW in this window is a held level, not a pulse.")

    lows = [h for h in hits if h[3] == 0x14]
    print(f"\n{len(lows)} write(s) drive a pin LOW; {found} form a LOW->HIGH pulse.")
    print("NOTE: this is ADDRESS order, not EXECUTION order. A pair here is a")
    print("candidate to check, not a proven runtime pulse.")

    print(f"\n=== unresolved base registers: {len(unresolved)} ===")
    print("These are stores this script could not attribute to a peripheral.")
    print("They are NOT hits, but they are NOT cleared either -- a GPIO write")
    print("through a function argument or struct pointer lands here.")
    if args.show_unresolved:
        for addr, raw, why in unresolved:
            print(f"  0x{addr:08X}  ({why})\n              {raw.strip()}")
    else:
        print("Re-run with --show-unresolved to list them.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
