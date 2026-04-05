# H2: The Boot-Time SPI3 Bulk Exchange (cmds 0x3B/0x3A)

**Status:** Leading hypothesis for the low-Ω meter calibration gap
after H1 (DAC reference cal) was ruled out. This document captures
what's been confirmed from the disassembly and what still needs
bench verification. Derived from `init_function_decompile.txt`
lines 4754-5100 and the stock V1.2.0 binary at
`archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`.

## What's definitively confirmed

### 1. It's SPI3, not USART2

Prior notes in `CLAUDE.md` called this the "SPI3 calibration
exchange", but it was sometimes referenced elsewhere as if it were a
USART exchange. Direct reading of master init resolves the
ambiguity: the transfer happens inside the SPI3 init phase using
register `0x40003C0C` (SPI3_DATA). **Not USART2.** Cross-ref:

- `init_function_decompile.txt:4774-4775` — `str r0, [r6, #4]`
  where `r6 = 0x40003C08` (SPI3_STAT). `[r6+4] = 0x40003C0C` =
  SPI3_DATA. The value written is `0x3B` (the opcode that opens the
  exchange).
- `init_function_decompile.txt:4996-4997` — same pattern, writes
  `0x3A` (the opcode that closes the exchange).

### 2. The source table is at `0x08051D19`

Confirmed from disassembly:
```asm
08026B28:  movw r1, #0x1d19
08026B32:  movt r1, #0x805     ; r1 = 0x08051D19
```

The loop body reads 3 bytes per iteration via `ldrb r3, [r1, r0]`,
`ldrb r7, [r3, #1]`, `ldrb r3, [r3, #2]` (where `r3 = r1 + r0`),
then TXes each via SPI3_DATA. `r0` is incremented by 3 per iteration.

### 3. The structure is 3-byte records

Confirmed from the loop (`adds r0, #3`) and from inspecting the
binary. At `0x51D19` in the V1.2.0 binary, the bytes show repeating
3-byte groupings with 177/411 non-zero bytes — consistent with a
sparse lookup table where most entries are defaults.

### 4. Opcodes 0x3B and 0x3A bracket the transfer

- **0x3B** (line 4774-4775): opens the exchange, transmitted first
- **(loop)** sends 3-byte records
- **0x3A** (line 4996-4997): closes the exchange, transmitted after
  the loop completes

## What's ambiguous

### The transfer size

The loop's exit condition is `cmp r0, r2; beq exit`. From the
disassembly at lines 4793-4797:
```asm
08026B28:  movw r2, #0xc3b6
08026B36:  movt r2, #1         ; r2 = 0x0001C3B6 = 115638
```

Taken literally, the loop runs until `r0` reaches `115638`, which
would transfer 115,638 bytes — far larger than the "411 bytes (137
entries × 3)" folklore in `CLAUDE.md`.

Two interpretations:

1. **Folklore is correct (411 bytes).** `CLAUDE.md`'s claim pre-dates
   the disasm I've read; it may be derived from observed bench
   traffic rather than disasm. If so, the actual loop has an early
   exit condition I haven't found — perhaps based on an SPI3
   response byte indicating "FPGA ack".

2. **Disasm is correct (115,638 bytes).** Then the table at
   `0x08051D19` is a large FPGA configuration or LUT blob, not
   calibration. `0x08051D19 + 0x1C3B6 = 0x0806E0CF`, which IS within
   the 751KB stock binary (ends at `0x080B7680`), so the math is
   physically possible.

**I cannot resolve this from disasm alone.** The bytes at 4798-4833
were misdecoded by Ghidra (Ghidra thought they were instructions
but they're likely data), so there could be an early-exit branch in
that region that I can't see without either a better disassembler
or a logic analyzer capture of stock boot.

### What the data actually does

Three possibilities:

1. **FPGA meter IC cal LUT** — most impactful for the low-Ω fix
2. **FPGA bitstream patch / config registers** — less likely since
   the Gowin GW1N-UV2 is non-volatile
3. **Generic FPGA setup** (register init, not cal) — the transfer
   might configure internal FPGA state unrelated to meter accuracy

Empirically: we know that WITHOUT this transfer, our meter reads
low-Ω at 33% of actual value. If the transfer is the fix, option 1
is confirmed. If not, option 3.

## What needs hardware access to resolve

### Option A (fastest, if a stock unit is available)

Hook a logic analyzer to SPI3 (PB3/PB5/PB6) on a stock 2C53T during
boot. Capture the 0x3B→bulk→0x3A exchange. The capture gives us:

- Exact byte count
- Exact byte sequence (definitive, no disasm ambiguity)
- Whether anything comes back from the FPGA (ACK pattern?)

### Option B (pure software, less certain)

Extract candidate byte ranges from `APP_2C53T_V1.2.0_251015.bin` at
offset `0x51D19`:

- **Short candidate**: first 411 bytes (matches folklore)
- **Medium candidate**: first 4KB or 16KB (until first obvious
  structure boundary)
- **Long candidate**: 115,638 bytes (matches literal disasm)

Add a test path in our firmware that replays each candidate during
boot, and bench the 147 Ω result for each. Whichever candidate
fixes low-Ω is the correct size.

**Risk**: replaying the wrong bytes could misconfigure the FPGA in
other ways (break scope mode, etc.). Keep this behind a test-only
compile flag.

## What our firmware currently does

Our `fpga.c` boot sequence (visible in `src/drivers/fpga.c` around
line 508) sends only these USART2 commands via `usart2_send_cmd`:

```c
usart2_send_cmd(0x00, FPGA_CMD_INIT_01);
usart2_send_cmd(0x00, FPGA_CMD_INIT_02);
usart2_send_cmd(0x00, FPGA_CMD_INIT_06);
usart2_send_cmd(0x00, FPGA_CMD_INIT_07);
usart2_send_cmd(0x00, FPGA_CMD_INIT_08);
```

Then the SPI3 init + handshake, then active mode (PB11 HIGH). **No
bulk cal upload via SPI3 cmds 0x3B/0x3A.** That's the gap.

## Recommended next move

**If we have hardware access**: bench-capture the exchange on a
stock unit (Option A). That gives us the truth in one session.

**If we don't**: extract candidate byte ranges from the stock binary
(Option B) and add a test-only boot path in our firmware. Bench
each candidate and observe whether low-Ω improves. This is slower
but purely software-driven.

**If neither is feasible this session**: leave H2 as the documented
leading hypothesis, and move to a different area (e.g., frame[11]
rolling counter, echo frame payload, or another meter gap).

## Binary fingerprint of the table

For reference, the first 60 bytes at `0x51D19` in `APP_2C53T_V1.2.0_251015.bin`:

```
00 00 04 00 00 00 20 10 00 00 20 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 30 ee ff ff
ff ff ff ff 00 00 20 00 10 00 00 00 00 00 00 00
00 00 00 00 00 10 00 00 08 00 20 00
```

177 of 411 bytes are non-zero in the first "411-byte" candidate
region. The pattern doesn't look obviously like calibration
coefficients (which would typically be dense multi-byte values),
but does fit "sparse configuration" — mostly zeros with scattered
feature enables.
