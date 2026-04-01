#!/usr/bin/env python3
"""Disassemble the full USART handler function at 0x080277B4."""

from capstone import *

FIRMWARE = "2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE_ADDR = 0x08000000

PERIPH = {
    0x40004400: "USART_STS", 0x40004404: "USART_DATA", 0x4000440C: "USART_CTL0",
}

with open(FIRMWARE, "rb") as f:
    fw = f.read()

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True
md.skipdata = True

start = 0x080277B4
off = start - BASE_ADDR
code = fw[off:off+0x400]
mvw = {}

print(f"=== USART handler at {start:#010x} ===\n")

return_count = 0
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
                    elif f==0x20000005: a = f"  ; {r} = TX_BUF (0x20000005)"
                    elif f==0x2000000f: a = f"  ; {r} = TX_FLAG (0x2000000F)"
                    elif 0x20000000<=f<0x20040000: a = f"  ; {r} = {f:#010x}"
                    elif 0x40000000<=f<0x50000000: a = f"  ; {r} = {f:#010x} (PERIPH)"
            except: pass
    elif insn.mnemonic == "bl":
        try:
            t = int(insn.op_str.lstrip("#"),0)
            names = {0x0803acf0: "xQueueGenericSend", 0x0803b09c: "xQueueGenericSendFromISR",
                     0x0803b1d8: "xQueueReceive"}
            a = f"  ; -> {names.get(t, f'FUN_{t:08x}')}"
        except: pass
    elif insn.mnemonic in ("movs","mov") and ", #" in insn.op_str:
        p = insn.op_str.split(", #")
        if len(p)==2:
            try:
                v = int(p[1],0)
                if 0<v<256: a = f"  ; = {v:#x} ({v})"
            except: pass
    elif insn.mnemonic == "strb":
        a = "  ; STORE_BYTE"
    elif insn.mnemonic == "ldrb":
        a = "  ; LOAD_BYTE"

    print(f"  {insn.address:#010x}: {insn.mnemonic:<8s} {insn.op_str}{a}")

    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        return_count += 1
        if return_count >= 3:  # Allow for branches within function
            break
    if insn.address > start + 0x300:
        break
