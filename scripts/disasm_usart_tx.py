#!/usr/bin/env python3
"""Disassemble the USART TX code that reads from 0x20000005 buffer."""

from capstone import *

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH = {
    0x40004400: "USART_STS", 0x40004404: "USART_DATA", 0x4000440C: "USART_CTL0",
    0x40010C10: "GPIOB_BOP", 0x40010C14: "GPIOB_BC",
    0x40011010: "GPIOC_BOP", 0x40011014: "GPIOC_BC",
}
FUNC = {
    0x0803acf0: "xQueueGenericSend", 0x0803b09c: "xQueueGenericSendFromISR",
    0x0803b1d8: "xQueueReceive", 0x0803a390: "vTaskDelay",
}

def disasm(fw, start, size, title):
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    md.skipdata = True
    off = start - BASE_ADDR
    code = fw[off:off+size]
    mvw = {}
    print(f"\n{'='*70}\n  {title} ({start:#010x})\n{'='*70}")
    for insn in md.disasm(code, start):
        a = ""
        if insn.mnemonic == "movw":
            p = insn.op_str.split(", #")
            if len(p)==2:
                try: mvw[p[0].strip()] = int(p[1],0)
                except: pass
        elif insn.mnemonic == "movt":
            p = insn.op_str.split(", #")
            if len(p)==2:
                r = p[0].strip()
                try:
                    h = int(p[1],0)
                    if r in mvw:
                        f = (h<<16)|mvw[r]
                        n = PERIPH.get(f)
                        if n: a = f"  ; {r} = {n}"
                        elif f==0x20000005: a = f"  ; {r} = TX_BUF_BASE"
                        elif f==0x2000000f: a = f"  ; {r} = TX_FLAG"
                        elif 0x20000000<=f<0x20040000: a = f"  ; {r} = {f:#010x}"
                        elif 0x40000000<=f<0x50000000: a = f"  ; {r} = {f:#010x} (PERIPH)"
                except: pass
        elif insn.mnemonic == "bl":
            try:
                t = int(insn.op_str.lstrip("#"),0)
                a = f"  ; -> {FUNC.get(t, f'FUN_{t:08x}')}"
            except: pass
        elif insn.mnemonic in ("movs","mov") and ", #" in insn.op_str:
            p = insn.op_str.split(", #")
            if len(p)==2:
                try:
                    v = int(p[1],0)
                    if 0<v<256: a = f"  ; = {v:#x}"
                except: pass
        print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{a}")
        if insn.mnemonic == "pop" and "pc" in insn.op_str: break
        if insn.mnemonic == "bx" and "lr" in insn.op_str: break

with open(FIRMWARE, "rb") as f:
    fw = f.read()

# Functions that reference 0x20000005 (USART TX buffer)
# Find function starts by scanning backwards for push
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True
md.skipdata = True

for target in [0x08027830, 0x080278C2]:
    # Scan backwards to find function start
    scan_start = target - 0x200
    code = fw[scan_start-BASE_ADDR:target-BASE_ADDR+0x10]
    last_push = scan_start
    for insn in md.disasm(code, scan_start):
        if insn.address >= target:
            break
        if insn.mnemonic == "push" and "lr" in insn.op_str:
            last_push = insn.address

    disasm(fw, last_push, target - last_push + 0x200,
           f"Function containing 0x20000005 ref at {target:#010x}")
