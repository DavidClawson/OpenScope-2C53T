# EXP-34 (H3 + H7 step 2) — the config transaction is fully exonerated; DMA-vs-polled is dead and the 0x15 frame is identical to stock

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1 (analysis only — no bench run this experiment)
- **Method:** static — disassembly of stock `master_init` from
  `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin` (link base
  `0x08007000`), cross-read against `firmware/src/drivers/fpga.c` and the
  Exp E SWD register dump (`analysis_v120/expE_swd_state_diff_2026-07-28.md`).
- **Status:** CLOSED — two hypotheses retired (H3 DMA-vs-polled; the `0x15`
  frame/pin-config difference). The config *transaction* is now exonerated
  end-to-end; the differentiator is state/sequencing, not the SPI traffic.

## 1. Question

After EXP-31/32/33 excluded bytes, framing, write rate, and every SPI3
transport defect, the last-standing candidate for "what does stock's AF
hardware-SPI do that ours does not" was a **dynamic** difference invisible to a
static register snapshot: the leading form was **H3 — stock DMA-streams the
115,638-byte `0x3B` upload (CTRL2=0x03 sets RXDMAEN+TXDMAEN) where we poll `DT`
with inter-byte gaps.** Settle it from the machine code, and diff the `0x15`
CONFIG_ENABLE frame itself across all three transports.

## 2. H3 — stock's `0x3B` upload is POLLED, not DMA

Disassembly of the upload loop at flash `0x0802DB28`:

```
0802db28: movw r1,#0x1d19 ; movt r1,#0x805   -> r1 = 0x08051D19  (payload ptr)
0802db2c: movw r2,#0xc3b6 ; movt r2,#1        -> r2 = 0x0001C3B6  (115638 = len)
0802db30: movs r0,#0                          -> r0 = index
  loop body (per byte, x3 unrolled):
    ldrb r3,[r1,r0]         ; load payload[i]
    <spin: ldr [r6,#0]; lsls #30; bpl>   ; wait TBE  (STAT bit1)
    str  r3,[r6,#4]         ; write DR
    <spin: ldr [r6,#0]; lsls #31; beq>   ; wait RBNE (STAT bit0)
    ldr  r3,[r6,#4]         ; read DR (discard)
  adds r0,#3 ; cmp r0,r2 ; loop until index == 115638
```

This is a CPU **per-byte store/load loop** (`str DT` / `ldr DT`), spinning on
TBE then RBNE for every byte. Two independent facts make "polled" dispositive:

1. **The loop is mutually exclusive with DMA.** A CPU writing `DT` 115,638
   times cannot coexist with a DMA channel also writing `DT` — they would
   corrupt each other. An explicit CPU `DT` loop *is* the polled path.

2. **The `CTRL2=0x03` DMA-enable bits have no channel behind them.** No DMA
   channel's peripheral-address register is loaded with SPI3's `DT`
   (`0x40003C0C`) anywhere in the decompile. Stock *does* arm DMA channels —
   for **USART2-RX** (meter data → `meter->usart_rx_buf`,
   `master_init_phase3.c:237`) and **SPI-flash reads** (`0x40005C00` base,
   `phase3:307-329`) — but **not** for the `0x3B` config stream. So
   `TXDMAEN|RXDMAEN` on SPI3 raise DMA *request* lines no channel services:
   asserted and ignored, exactly as `fpga.c:4968` already stated.

**⇒ H3 refuted.** Both stock-AF and our-AF poll `DT` byte-by-byte with
inter-byte gaps (stock stalls on RBNE every byte; ours actually runs the
issue-#11 double-buffered *gapless* pump, so **our upload is LESS gapped than
stock's**).

**And it was never the wall anyway.** Per Exp N, STATUS reads `00039020`
(EDIT_MODE bit7 clear) *before the first `0x3B` byte is clocked*. The wall
stands at `0x15`; upload dynamics are downstream and cannot be the
config-entry differentiator in either direction.

## 3. The `0x15` CONFIG_ENABLE frame — identical across all three transports

Decoded stock's prelude framing from `0x0802D8F0`–`0x0802DB06` (r6 = SPI3;
`STAT`@`[r6,#0]`, `DT`@`[r6,#4]`; r4 = GPIOB, CS = PB6 = `0x40`; `lr`-offset =
CS↑ deassert, `ip`-offset = CS↓ assert — fixed by the 100 ms delay sitting in
the CS-HIGH gap). Per-frame idiom:

```
CS↓ | cmd | 00 | CS↑ | 00 (clocked CS-HIGH) | [100ms after 05/12, none before 3B]
```

| feature of the `0x15` frame | stock AF (disasm) | our bit-bang (works) | our AF (walls) | status |
|---|---|---|---|---|
| SPI mode | 3 | 3 | 3 | matched |
| `0x15` + one trailing `0x00` in CS-low frame | yes | yes | yes | matched |
| CS↓ before cmd, CS↑ after trailing byte | yes | yes | yes | matched |
| one `0x00` clocked **CS-HIGH** between frames | yes (`0x802dac2`) | yes (`bb_xfer(0)`) | opt `FPGA_HW_CS_DUMMY` | **excluded (EXP-29)** |
| polled full-duplex, read-DR-per-byte | yes | (GPIO) | yes (`spi3_xfer`) | matched |
| 100 ms after `05`/`12`, none before `3B` | yes | yes (faithful) | yes (`prelude_gap_ms`) | matched |
| write clock rate | /2 | ~2 MHz | /2 **and** /256 | **excluded (EXP-32)** |
| `0x05` present | yes | opt | opt | **excluded (EXP-27)** |
| **PB3/PB5 pin register nibble** | **`9`** AF-PP 10 MHz | (GPIO-PP) | **`9`** AF-PP 10 MHz | **matched (Exp E SWD, line 73)** |
| PB6 CS nibble | `1` GPIO-PP | `1` | `1` | matched |

Two corrections banked while decoding:

- **`fpga.c` ~4340 comment is stale.** It claims stock "clocks NOTHING while CS
  is high" (from the /64 capture) and removed the CS-high dummy on that basis.
  The machine code shows stock **does** clock a `0x00` with CS high between
  every frame (`0x802dac2`, after CS↑, before the next CS↓). EXP-29's H4 decode
  already caught this; the comment never got updated. Re-adding the dummy
  (`FPGA_HW_CS_DUMMY=1`) was tested in EXP-29 and still walled.
- **PB3/PB5 AF pin config is bit-identical to stock.** The Exp E SWD dump reads
  nibble `9` (AF push-pull, 10 MHz slew) on both PB3 and PB5 for stock *and*
  ours (line 73). The "STRONGER-vs-stock slew" lead EXP-29 flagged as never
  fully diffed is now closed: our AF pins are configured identically to stock's.

## 4. Conclusion — the transaction is exonerated

Our AF `0x15` frame is **indistinguishable** from stock's AF `0x15` frame —
same bytes, same CS framing, same mode, same polled full-duplex structure, same
pin registers — and indistinguishable from the bit-bang frame that *succeeds*
from the same boot posture. Every enumerable is matched or excluded. Yet stock
configures and our AF walls.

That is a paradox: identical inputs, different outcome. It means the
differentiator is something a static register snapshot + a byte-stream diff
**cannot** see. Today's H3 death removed the leading such candidate (both poll).
What remains, in priority order:

1. **A dynamic transport property not in the byte list** — the one concrete
   difference this diff surfaced: stock runs the *entire* config at a single
   prescaler and never touches BR; our AF path switches BR to /256 for anchored
   reads. A stray SCK edge on a BR/CTRL1 reconfigure could desync the SSPI
   command FSM. **Tempered:** the wall is measured *after* `0x15`, so only a
   *pre*-`0x15` BR change could be causal — narrowing but not eliminating it.
   → next bench test (§5).
2. **FPGA config-FSM state when `0x15` arrives** — the unexplained
   `POR=1 / DONE_FINAL=0` combo; stock does ~9.8 KB of init (and its PC9
   sequencing) first. *Counter-evidence:* bit-bang succeeds from our cold
   posture too — so if this is it, it is about bit-bang being *slow enough* to
   let something settle, not prior init.
3. **Uncontrolled FPGA power/POR state** across the compared runs.

## 5. Next

**Zero-read, single-BR AF attempt.** Drive `05/12/15/3B/3A` with the prescaler
NEVER touched (exactly stock's open-loop path), reading the wall only in a
separate frame afterward:

- flips it → the BR/CTRL1 reconfigure was the dynamic culprit (item 1);
- walls → items 2/3 move to the front and an LA side-by-side of the two paths'
  actual `0x15` edges becomes the physical-layer backstop.

Shipping remains unaffected: the bit-bang coldtrace path already cold-boots to a
live scope. This whole line is an *understanding* question (and a possible
faster-config path), not a blocker.
