# Stock's runtime SPI3 command set, decoded from the jump table (2026-08-15)

Desk work on `APP_2C53T_V1.2.0_251015.bin` (link base `0x08007000`). This
replaces guesswork about the five "arm" writes with the actual dispatch.

## The task loop

`fpga` FreeRTOS task, entry `0x0803E454`. Register conventions:

| reg | value | meaning |
|---|---|---|
| `r7` | `0x40003C08` | SPI3 **STS** at `[r7,#0]`, **DT** at `[r7,#4]` |
| `r4` | `0x20002D78` | `spi3_data` queue (matches the queue map in CLAUDE.md) |
| `r9` | `0x200000F8` | scope state base |

Bit tests: `lsls #31` → STS bit0 = RDBF (rx ready); `lsls #30` → bit1 = TDBE (tx empty).

Loop body (`0x0803E4B2`):

```
xQueueReceive(spi3_data, &opcode, portMAX_DELAY)
GPIOB->BRR  = 0x40          ; PB6 LOW  -> CS assert          (0x0803E4F4)
TX opcode; drain DT                                           (0x0803E512)
if (opcode-1) > 8 -> done
tbh [pc, (opcode-1)<<1]     ; 9-entry jump table              (0x0803E536)
...handler...
GPIOB->BSRR = 0x40          ; PB6 HIGH -> CS release          (0x0803E498)
```

**One CS frame per command, opcode + payload = 2 bytes.** Our `spi3 seq` matches.

## The jump table (decoded from the halfword offsets at `0x0803E53A`)

| op | handler | payload / behaviour |
|---|---|---|
| `0x01` | `0x0803E54C` | TX `state[0x2d]` = **timebase index**. Gated on `tbl[0x0804D833 + tb] + 0x32 <= state[0xdb8]` |
| `0x02` | `0x0803E970` | TX `state[0x14]` if nonzero, else `3`. `state[0x14]` ∈ {1,2,3} = **CH1 / CH2 / BOTH** |
| `0x03` | `0x0803E9F2` | TX `0xFF`; also shifts the roll-mode buffers at `state[0x356]`/`state[0x483]` |
| `0x04` | `0x0803E5A4` | TX `0xFF` (discard), then 1024 × `0xFF` → `state[0x5B0 + 0..1023]` |
| `0x05` | `0x0803E68C` | TX `0xFF` (discard), then 1024 × `0xFF` → `state[0x5B0 + 1024..2047]` |
| `0x06` | `0x0803ED1C` | TX `state[0x16]` — used elsewhere as an **array index**: `state[0x1c] = sysstate[state[0x16]]`, `state[0x352 + state[0x16]]` |
| `0x07` | `0x0803ED5C` | TX `state[0x18]` ∈ {0,1,2,3} |
| `0x08` | `0x0803E75C` | TX `uxtb(r2)` = the VFP-computed **trigger comparator level** (from `state[0x352+ch]`, `state[0x1c]`) |
| `0x09` | `0x0803E79C` | TX `FF FF`, keep byte[2] → high byte; then **CS toggle + a new frame with opcode `0x0A`**, TX `FF FF`, keep byte[2] → low byte. Result is a 16-bit value in `state[0x46]` |

## Key results

1. **op `0x04` and op `0x05` are byte-identical SPI transactions.** The only
   difference is the destination: first vs second half of the 2048-byte buffer
   at `state[0x5B0]`. There is no channel-select action, no extra command, and
   no re-arm between them — `0x0803ED9C` (the op-04 completion path) only
   applies the gain/offset cal in place and falls through to the common tail.
   **⇒ Our MCU-side read protocol for CH2 is already exactly stock's.**

2. **Framing correction:** stock discards exactly **2** bytes (the opcode echo
   and one dummy) and keeps **1024** samples — total 1026 on the wire. Our
   readers have been discarding 3 and keeping 1023, so every sample array is
   shifted one byte late and one sample short. Cosmetic for spectra, wrong for
   any absolute sample indexing.

3. **Opcode `0x0A` exists and is undocumented anywhere in this project.** It is
   only reachable from op `0x09`'s handler. On bench unit #1 it returns a stable
   `0x89` at byte[2] while op `0x09` returns `0x00` — so the pair reads
   `0x0089` = 137. Stable across repeated reads, so it is not a free-running
   counter. `state[0x46]` is where stock stores it. Meaning unknown.
   Also noted: byte[0] of the op-`0x09` frame read `0x80` on 5 of 6 tries and
   `0x00` once — a possible ready/valid flag, unconfirmed.

## Bench results from this decode (all NEGATIVE for CH2)

Two-tone rig: 100 Hz into the CH1 jack, 250 Hz into the CH2 jack, 600 mVpp,
both ranges 3, free-run (`08 ff`), reg01 = `0x10`. fs ≈ 14.6 kS/s, so the tones
land at k ≈ 7 and k ≈ 17.5 cycles/buffer. Detector is a direct DFT bin scan.

- **Baseline:** both op04 and op05 show **only k=7** (the CH1-jack tone).
  The CH2-jack tone is absent from both buffers. The user confirmed the same
  thing visually — yellow and blue traces on screen are identical.
- **op `0x06` swept 0..3** — no change to either buffer. (This was the strongest
  candidate: stock uses `state[0x16]` as a channel index.)
- **op `0x07` swept 0..3** — no change.
- Previously swept and also negative: op `0x02` 0..3, op `0x09` 0..3, the CH2
  relay bank, PA15, PD13, PB11, PC1/PC2, and a GPIO sweep of PC3/PC4/PC14/PC15/
  PA5/PB2/PE0/PE1/PD7/PA1/PB13 as configured outputs, both polarities.

**Retraction:** the earlier note that op04/op05 differ by a systematic ~3-code
ADC offset (read as evidence of two converters) is withdrawn. The means drift
read-to-read on *both* opcodes over the same 117–124 range; the offset is drift,
not a fixed per-converter bias.

## USART2 is not the answer either

The `guest-coldtrace` family sets `FPGA_USART_SILENT_SCOPE=1`, leaving USART2
electrically dark (UEN clear) — a leftover from the config-entry experiments.
Since the documented command table has `0x01` = "Scope: configure channel,
type 0 (CH1) / type 1 (CH2)" and `0x0B–0x11` = channel/trigger/timebase, this
looked like the missing channel.

USART2 was brought up **live, with no rebuild** — `BAUDR` and PA2 AF-PP are
configured outside the `#if`, so only two register pokes are needed:

```
mem write 0x4000440C 0x202C     # RE|TE|RDBFIEN|UEN
mem write 0xE000E104 0x40       # NVIC ISER1 bit6 = IRQ38 (USART2)
```

(`BAUDR` read back `0x30D4`, byte-identical to stock's.) Frames then transmit
from `usart raw` (TX is polled, not interrupt-driven).

Sent `0x0001` p1=0, `0x0001` p1=1, `0x000B` p1=1, `0x000B` p1=3, `0x0011` p1=1.
**All five: no change to either buffer, and no echo frame ever came back.**
The FPGA does not acknowledge scope commands on USART2 in this state — which is
weak additional support for the open question of whether USART2 reaches the
FPGA at all, or only a separate meter device.

## Where that leaves CH2

Bounded tightly. Every MCU-reachable channel is now excluded: SPI3 command set
(fully decoded from stock and matched), USART2, GPIO, and the analog relay
banks. The load-bearing observation is that **CH1's attenuator bank moves BOTH
buffers while CH2's bank moves NEITHER** — both readout paths are fed from the
CH1 analog node.

The decisive next experiment is Komzpa's `debugclk_hw_top` image (issue #24),
which drives *synthetic* known patterns through the real capture/readout path:
CH1 = free-running 8-bit ramp, CH2 = walking one (`1 << ramp[2:0]`). If op05
returns the walking-one, the readout mux separates the channels and the fault is
analog/ADC-side. If op05 returns the ramp, the interleave model is wrong.
No amount of poking at our own firmware can distinguish those.
