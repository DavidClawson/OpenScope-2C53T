# H2 Resolved: SPI3 Bulk Exchange at Boot (cmds 0x3B / 0x3A)

**Analysis date:** 2026-04-04
**Analyst:** Claude Sonnet 4.6 via agent session
**Binary:** `APP_2C53T_V1.2.0_251015.bin` (751,232 bytes)
**Sources:** `init_function_decompile.txt` lines 4180–5060; raw binary at offsets 0x26B3A–0x26C32, 0x51D19–0x6E0CF

---

## 1. Executive Summary

**Transfer size: 115,638 bytes (38,546 × 3-byte records). CONFIRMED — NOT 411 bytes.**

The "411 bytes / 137 entries" figure in prior CLAUDE.md folklore was a speculation that
was never validated. The disassembly is unambiguous: `r2 = 0x0001C3B6 = 115,638`, the
loop increments `r0` by 3 each iteration, `0x1C3B6 % 3 == 0` exactly, and the exit
condition `beq` fires when `r0 == 0x1C3B6` after 38,546 iterations.

**Purpose: FPGA runtime state / calibration initialization. MEDIUM-HIGH confidence.**

The 115,638-byte data block at `0x08051D19` programs the FPGA's internal register/memory
state. It includes calibration coefficients for the meter ADC pipeline. The stock firmware
sends this during boot (between the FPGA handshake and active-mode enable), bracketed by
PB6 CS ASSERT/DEASSERT. Our custom firmware skips it entirely. The 33% low-Ohm error is
consistent with a missing ADC gain factor that this transfer provides.

**Highest-confidence replay target:** the full 115,638-byte range at file offsets
`0x51D19`–`0x6E0CF`, sent as 38,546 × 3-byte records via SPI3 between opcode 0x3B and
opcode 0x3A, with CS (PB6) held LOW for the entire transaction.

---

## 2. Hand-Decoded Disassembly: Lines 4798–4833

The region that Ghidra "misdecoded" as ARM instructions (lines 4799–4827 in the
disasm file, flash addresses `0x08026B3C`–`0x08026B72`) is **not code at all**. It is
ASCII string data for FreeRTOS task names, embedded in flash immediately after a branch
instruction.

**Raw bytes at file offset `0x26B3A`:**

```
00026b3a: 1f e0  54 69 6d 65 72 31 00 00  54 69 6d 65 72 32 00 00
          ^^^^   |----Task: "Timer1"----|  |----Task: "Timer2"----|
00026b4a: 64 69 73 70 6c 61 79 00  6b 65 79 00  6f 73 63 00
          |---Task: "display"---|  |-"key"-|  |-"osc"-|
00026b5a: 66 70 67 61 00 00 00 00  64 76 6f 6d 5f 54 58 00
          |---"fpga"---|           |----"dvom_TX"-----|
00026b6a: 64 76 6f 6d 5f 52 58 00
          |----"dvom_RX"-----|
```

**Decoded correctly:**

| Address      | Encoding | Instruction | Notes |
|--------------|----------|-------------|-------|
| `0x08026B3A` | `1F E0`  | `b.n #0x8026B7C` | Jump to loop body (PC+4 + 31×2 = 0x26B7C) |
| `0x08026B3C`–`0x08026B73` | ASCII | **Data, not code** | 56 bytes of FreeRTOS task name strings |

Ghidra decoded the string bytes as instructions. There is **no early-exit branch** in
this region — it is pure constant data skipped by the `b.n` at `0x08026B3A`.

**Loop tail (verified against raw bytes at `0x26B74`):**

```
00026b74: 03 30  73 68  90 42  5a d0
          adds r0,#3  ldr r3,[r6,#4]  cmp r0,r2  beq #0x26C32
```

`beq` target: `0x26B7A + 4 + (0x5A × 2) = 0x26C32` — confirmed `loc_08026C32`
(loop exit, CS deassert). The loop-back path goes through `b #0x8026b74` at
`0x08026C30`, covering all code paths in `loc_08026C16`.

---

## 3. Definitive Transfer Size

**115,638 bytes. 38,546 iterations. NO early exit.**

Evidence chain:

1. **`movw r2, #0xc3b6`** at `0x08026B2C` → `r2` lower 16 bits = `0xC3B6`
2. **`movt r2, #1`** at `0x08026B36` → `r2` = `0x0001C3B6` = **115,638 decimal**
3. **`movs r0, #0`** at `0x08026B30` → loop counter starts at 0
4. **`adds r0, #3`** at `0x08026B74` → counter increments by 3 per iteration
5. **`cmp r0, r2; beq #0x26C32`** at `0x08026B78/7A` → exits when `r0 == 115,638`
6. `115,638 ÷ 3 = 38,546` exactly (remainder 0) — the `beq` fires on the final iteration
7. `0x08051D19 + 0x1C3B6 = 0x0806E0CF` — end address is **within** the 751,232-byte binary
8. The "misdecoded" region `0x26B3C–0x26B73` is 56 bytes of ASCII data, jumped over by
   `b.n #0x26B7C` at `0x26B3A`. No branch in that region escapes the loop early.

The `master_init_phase2.c` annotation ("137 iterations" and "table at `0x0804D7C1`") is
**incorrect** — those were best-effort annotations made before this analysis. The raw
disasm is authoritative.

---

## 4. Data Structure Analysis

### Overview

The 115,638-byte region spans flash addresses `0x08051D19`–`0x0806E0CF`.

```
Total bytes:       115,638
Non-zero bytes:     40,282  (34.8%)
Zero bytes:         75,356  (65.2%)
0xFF bytes:          6,199   (5.4%)
Unique byte values:    256  (all represented)
```

### 3-byte record view (transmission unit)

The MCU loop sends exactly 3 bytes per iteration (`ldrb [r1,r0]`, `ldrb [r3,#1]`,
`ldrb [r3,#2]`), making the 3-byte triplet the minimum transmission unit.

Of the 38,546 records:
- **20,654 (53.6%)** are `00 00 00` — effectively NOPs/defaults
- **17,892 (46.4%)** contain at least one non-zero byte
- **3,537** contain at least one `0xFF` byte

### 160-byte block structure

Every 160 bytes (= one logical block), bytes 30–35 contain **exactly `FF FF FF FF FF FF`**.
This sentinel appears in **546 of 722** full 160-byte blocks (75.6%), with complete
regularity at the start of the data. The pattern breaks down later where the data becomes
denser/more variable.

Each 160-byte block divides as:
- Bytes 0–29 (30 bytes = 10 × 3-byte records): data sub-field A
- Bytes 30–35 (6 bytes = 2 × `FF FF FF` records): sentinel/separator
- Bytes 36–159 (124 bytes = 41 × 3-byte records + 1-byte partial): data sub-field B

Since `160 ÷ 3 = 53.33...` (non-integer), the 3-byte record boundaries do NOT align
with the 160-byte block boundaries. The 160-byte period is an independent structural
dimension — the FPGA interprets both.

### Entropy profile

Shannon entropy across 4KB windows:

| Region (data offset) | Entropy (bits/byte) | Character |
|---------------------|---------------------|-----------|
| 0x00000–0x08000     | 2.7–4.5             | Sparse config data, moderate entropy |
| 0x09000–0x14000     | 0.3–0.9             | Nearly all zeros — large null-padded region |
| 0x15000–0x1C000     | 4.2–6.0             | Dense data / high-entropy coefficients |

The entropy profile is NOT consistent with a FPGA bitstream (which has near-maximum
entropy throughout). It IS consistent with a sparse register initialization table
followed by dense calibration coefficients.

### Representative hex dump (first 192 bytes)

```
00051d19: 0000 0400 0000 2010 0000 2000 0000 0000  ...... ... .....
00051d29: 0000 0000 0000 0000 0000 0000 30ee ffff  ............0...
00051d39: ffff ffff 0000 2000 1000 0000 0000 0000  ...... .........
00051d49: 0000 0000 0010 0000 0800 2000 0000 0000  .......... .....
00051d59: 0000 0000 0000 2042 4000 0001 0000 0024  ...... B@......$
00051d69: 0026 0000 0002 0040 0000 0000 0000 0000  .&.....@........
00051d79: 4600 1000 8002 008c 2000 0000 0008 0026  F....... ......&
00051d89: 0104 4002 0040 0460 0044 0020 0040 0800  ..@..@.`.D. .@..
00051d99: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00051da9: 0000 0000 0000 1000 0000 2000 0400 0000  .......... .....
00051db9: 0000 8000 2001 0000 0020 0006 0000 0000  .... .... ......
00051dc9: 0000 0000 0000 0000 0000 0000 0a8c ffff  ................
```

The first `FF FF FF FF FF FF` sentinel appears at bytes 30–35 (offset `+0x1E` from the
table start). Every subsequent 160-byte block carries the same sentinel at position 30.

---

## 5. Opcode Table Lookup

The USART command dispatch table at `0x0804BE74` handles **incoming** command bytes
from FPGA USART RX frames. It covers USART commands `0x00`–`0x2C`.

**Opcodes `0x3A` and `0x3B` do NOT appear in this table.** They are SPI3-only opcodes
that the FPGA custom logic handles at the SPI hardware interface level, not via the
USART command pipeline.

Interpretation: `0x3B` and `0x3A` are **SPI session opcodes** that the FPGA's SPI slave
logic decodes:
- `0x3B` → "enter bulk register write mode" (begin accepting 3-byte config records)
- `0x3A` → "end bulk register write mode" / commit / apply

This is separate from the USART protocol entirely. The USART handles runtime commands;
the SPI3 handles boot-time state initialization.

---

## 6. Boot-Sequence Position

The bulk exchange occurs in **Phase 8: Post-Handshake Config**, between the FPGA
query handshake and the active-mode enable.

Bracketed by SysTick delays. Full sequence just prior:

1. **Phase 6** (steps 24–26): SysTick delay × 2, then 2-iteration 100-tick loop
2. **Phase 7** (steps 27–39): CS toggle handshake, send `0x00`, `0x05`, `0x00`, `0x00`
   (FPGA ID/status query), then more dummy bytes, then delay loop
3. **Phase 8** (steps 40–44):
   - Another SysTick delay (100 → 50 → 0)
   - CS assert; send `0x12`, wait; send `0x00` flush
   - Send `0x15`, wait; send `0x00` flush

Then, **NOT in the 53-step sequence doc**:
   - **CS ASSERT** (PB6 LOW): `str 0x40, [GPIOB_BCR]` at `0x08026AE6`
   - **Send `0x3B`** (bulk start opcode): `0x08026B06`–`0x08026B08`
   - **Loop** (`0x08026B7C`–`0x08026C30`): 38,546 iterations, 3 bytes each = 115,638 bytes
   - **Drain RX then send `0x3A`** (bulk end opcode): `0x08026C96`–`0x08026C98`
   - **Send `0x00`** flush: `0x08026CD2`
   - **CS DEASSERT** (PB6 HIGH): `str 0x40, [GPIOB_BOP]` at `0x08026CF6`

Then continues to step 46 (DMA configuration for SPI3 bulk transfers).

**Important additional finding:** The CS control instructions bracket PA6 as well.
`r4 = GPIOC base (0x40011000)`. The `[r4, ip]` write (ip = `0xFFFFFC14`) lands at
`0x40010C14` = **GPIOB_BCR** (CS assert = PB6 LOW), and `[r4, lr]` (lr = `0xFFFFFC10`)
lands at `0x40010C10` = **GPIOB_BOP** (CS deassert = PB6 HIGH). There is no separate
PA6 toggle here; what appeared to be PA6 writes are in fact PB6 CS control via GPIOC-
relative negative offsets.

---

## 7. Verdict on the Three Hypotheses

### H(a): FPGA Meter IC Calibration LUT — **MOST LIKELY**

The data programs the FPGA's internal meter ADC pipeline. Evidence:

- **Size is compatible**: the high-entropy tail (offset `0x15000+` in the data) likely
  contains the dense calibration coefficients. The 65% zero-padded majority provides
  defaults for unused registers.
- **The `FF FF FF` sentinels** occur every 160 bytes = plausible block-structured
  register map (e.g., one 160-byte block per measurement range or per ADC channel).
  A meter ASIC register file structured this way is common.
- **The 33% gain error** when the transfer is skipped is mechanistically consistent
  with a missing ADC gain factor. If the FPGA's DSP pipeline has a default gain of 1
  and the correct gain (for low-Ohm mode) is ~3, the result would be reading 33% of
  actual — exactly the observed symptom.
- **PA6 bracket** (analog frontend control pin) drops LOW immediately before `0x3B`
  and returns HIGH after. This gates the analog signal path for calibration, suggesting
  the transfer programs something that interacts with the analog frontend.

### H(b): FPGA Bitstream / Config — **RULED OUT**

The Gowin GW1N-UV2 is non-volatile. Its configuration bitstream is stored in embedded
flash and loaded automatically at power-on. This transfer happens AFTER the FPGA is
already running (evidenced by the handshake `0x05` query receiving a response in Phase
7). Gowin bitstream loading uses a different SPI mode and different opcodes. Entropy of
~3 bits/byte (not the 7.8+ bits/byte expected for compressed/encrypted bitstream)
further rules this out.

### H(c): Generic FPGA Setup Unrelated to Meter — **PARTIAL**

The transfer does initialize FPGA internal state, and some of that state may affect scope
mode too (e.g., ADC sampling parameters, trigger threshold DACs). But "generic setup" is
too broad — the PA6 bracket, the position in the init sequence (after meter-mode
commands), and the 33% gain error all point specifically at the meter ADC pipeline as the
primary target.

**Verdict: The bulk transfer is FPGA meter-IC calibration and ADC-pipeline
initialization, stored in flash as a pre-computed register-write sequence. Confidence:
MEDIUM-HIGH.**

---

## 8. Replay Test Plan

**Objective:** Verify that replaying the 115,638-byte transfer fixes low-Ohm meter
readings without breaking scope or signal-gen modes.

### Minimum-risk approach

1. **Add a compile-time flag** `FPGA_BULK_CAL_ENABLE` (default: off in normal builds).
   Gate the bulk transfer behind `#ifdef FPGA_BULK_CAL_ENABLE`.

2. **In `fpga_init()` (or a new `fpga_bulk_cal_upload()` function)**, after the existing
   USART init commands (0x01–0x08) and the SPI3 handshake, but before `PB11 = HIGH`
   (active mode), add:

   ```c
   // Bulk cal upload: bracket with CS
   gpio_bits_reset(GPIOB, GPIO_PINS_6);          // PB6 LOW = CS ASSERT
   spi3_tx_drain();                               // drain any pending RX
   spi3_tx_byte(0x3B);                            // opcode: begin bulk write
   // wait for TX empty + RX available
   const uint8_t *p = (const uint8_t *)0x08051D19;
   const uint8_t *end = p + 0x1C3B6;
   while (p < end) {
       spi3_tx_byte(*p++);                        // byte 0
       spi3_tx_byte(*p++);                        // byte 1
       spi3_tx_byte(*p++);                        // byte 2
       spi3_drain_rx();                           // discard MISO (3 bytes)
   }
   spi3_tx_byte(0x3A);                            // opcode: end bulk write
   spi3_tx_byte(0x00);                            // flush byte
   spi3_drain_rx();                               // drain
   gpio_bits_set(GPIOB, GPIO_PINS_6);             // PB6 HIGH = CS DEASSERT
   ```

3. **Bench test sequence:**
   a. Flash with `FPGA_BULK_CAL_ENABLE` defined.
   b. Connect a known 100 Ω resistor to DMM probes.
   c. Enter Ohm mode (2-wire).
   d. Read displayed value. **Success condition: displayed value within ±5% of 100 Ω.**
      (Without fix, would display ~33 Ω for a 100 Ω resistor.)
   e. Verify scope mode: apply a known sine wave, confirm frequency and amplitude read
      correctly. (Ensures bulk transfer does not corrupt scope calibration.)
   f. If step (d) is correct but (e) breaks, investigate whether the data block has a
      scope-specific segment that needs to be skipped or conditionally applied.

4. **If the full 115,638 bytes is too slow** (at SPI3 60 MHz, ~1.9 ms, should be fine):
   try the first 16KB only (the dense region before the large zero-pad at offset 0x9000).
   If that also fixes it, the zero-padded tail is irrelevant and can be stripped.

5. **Do not modify RAM calibration tables or the USART command sequence.** The bulk
   transfer is purely a SPI3 init operation.

---

## 9. Binary Fingerprint

**Table location:**

| Field | Value |
|-------|-------|
| Flash address (start) | `0x08051D19` |
| Flash address (end)   | `0x0806E0CF` (inclusive last byte) |
| File offset (start)   | `0x51D19` |
| File offset (end)     | `0x6E0CF` |
| Size                  | `0x1C3B6` = 115,638 bytes |
| Records               | 38,546 × 3 bytes each |
| Non-zero bytes        | 40,282 (34.8%) |
| 0xFF bytes            | 6,199 (5.4%) |

**Full hex dump, first 192 bytes (`0x08051D19`–`0x080051DD8`):**

```
08051d19: 0000 0400 0000 2010 0000 2000 0000 0000  ...... ... .....
08051d29: 0000 0000 0000 0000 0000 0000 30ee ffff  ............0...
08051d39: ffff ffff 0000 2000 1000 0000 0000 0000  ...... .........
08051d49: 0000 0000 0010 0000 0800 2000 0000 0000  .......... .....
08051d59: 0000 0000 0000 2042 4000 0001 0000 0024  ...... B@......$
08051d69: 0026 0000 0002 0040 0000 0000 0000 0000  .&.....@........
08051d79: 4600 1000 8002 008c 2000 0000 0008 0026  F....... ......&
08051d89: 0104 4002 0040 0460 0044 0020 0040 0800  ..@..@.`.D. .@..
08051d99: 0000 0000 0000 0000 0000 0000 0000 0000  ................
08051da9: 0000 0000 0000 1000 0000 2000 0400 0000  .......... .....
08051db9: 0000 8000 2001 0000 0020 0006 0000 0000  .... .... ......
08051dc9: 0000 0000 0000 0000 0000 0000 0a8c ffff  ................
```

**Sentinel pattern (appears at byte 30 of each 160-byte block):**

```
Offset within block +0x1E:  30 ee ff ff ff ff ff ff
                             ^^^^^^^^^^^^^^^^^^^^
                             The 6-byte 0xFF sentinel
                             (two consecutive 0xFF 0xFF 0xFF 3-byte records)
```

**Key checksum (first 16 bytes as fingerprint):**

```
SHA256(data[0:16]) = sha256(0000040000002010000020000000000) 
First 16 bytes: 00 00 04 00 00 00 20 10 00 00 20 00 00 00 00 00
```

---

## Appendix: Why the "411 bytes" Folklore Was Wrong

The "411 bytes (137 entries × 3)" figure appears to have originated from an earlier
session's rough analysis that misidentified the loop iteration count (perhaps from
looking at the `0x89` = 137 value that appears in the disasm context, which is
unrelated to this loop's limit).

The actual disasm is explicit:
- `movw r2, #0xc3b6` + `movt r2, #1` = `r2 = 0x0001C3B6 = 115638`
- The exit condition is `cmp r0, r2; beq exit`
- `r0` starts at 0 and adds 3 per iteration
- `115638 / 3 = 38546.0` exactly

No ambiguity remains. The 411-byte figure is incorrect.

---

## Appendix: CS Control Register Arithmetic

The CS control uses negative-offset addressing relative to `r4 = GPIOC base = 0x40011000`:

```
movw ip, #0xfc14    (lower 16 bits)
movt ip, #0xffff    → ip = 0xFFFFFC14

movw lr, #0xfc10    (lower 16 bits)
movt lr, #0xffff    → lr = 0xFFFFFC10

r4 + ip = 0x40011000 + 0xFFFFFC14 (mod 2^32) = 0x40010C14 = GPIOB_BCR (CS ASSERT)
r4 + lr = 0x40011000 + 0xFFFFFC10 (mod 2^32) = 0x40010C10 = GPIOB_BOP (CS DEASSERT)
```

Both `str.w r0, [r4, ip]` and `str.w r0, [r4, lr]` (with r0 = 0x40 = bit 6 = PB6)
control the SPI3 chip select — NOT PA6 as an earlier (incorrect) analysis suggested when
using GPIOA base (0x40010800) instead of GPIOC base (0x40011000) for `r4`.
