# EXP-30 — gapless intra-prelude does NOT break the wall (post-H6 suspect a)

- **Date:** 2026-08-25
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-configA-gapless` (hardware-SPI config, `FPGA_CONFIG_A`,
  `FPGA_HW_PRELUDE_GAPLESS=1`, SCOPE_DEBUG_OVERLAY)
- **Status:** CLOSED — negative. Intra-frame polled-cadence stall **excluded**.
  **This is the last firmware-reachable axis; the firmware search is now
  exhausted.**

## 1. Problem

After H6 proved the SPI3 peripheral + pin registers are byte-identical between
stock (config succeeds) and ours (fails), the only firmware-reachable difference
left was **cadence**: our polled `spi3_xfer` waits for RDBF between the command
byte and its `0x00` param, so SCK idles mid-frame; both working transports
(bit-bang, stock's unrolled loop) clock continuously. Is that intra-frame stall
why hardware-SPI config entry fails?

## 2. Hypothesis

If the Gowin SSPI FSM needs the two bytes of a config frame clocked back-to-back,
removing the stall flips the wall:

- **H true →** `CFG` `D1` / anchor `A-1` / demo trace gone.
- **H false →** `CFG:00039020 D0 A0` — the wall, unmoved.

Note this is the **opposite** direction from EXP-26, which only ever *added*
inter-byte gaps. The intra-frame stall being *present* (not absent) had never
been tested as the cause.

## 3. Procedure

New helper `spi3_xfer2_gapless()` (fpga.c) primes byte 1 the moment TDBE frees —
before byte 0's RX drains — so the shift register reloads back-to-back and SCK
does not idle inside the CS-LOW frame (the double-buffered idiom of
`spi3_pump_h2_record`, for a 2-byte frame, keeping `init_hs[]` capture).
`FPGA_HW_PRELUDE_GAPLESS=1` routes all three mode-0 prelude frames (05/12/15)
through it. IAP-flash → **true power cycle** (POWER → "Goodbye" → unplug USB →
replug) → scope mode → read LCD `CFG:`.

## 4. Control

- **Anchor `A0`** on the read = IDCODE valid AND part cold/unconfigured.
- Wall pole `CFG:00039020 D0 A0` — the same signature EXP-26/27/29 recorded cold.

## 5. Results

| read | CFG | D | E | A | L | H |
|---|---|---|---|---|---|---|
| cold | `00039020` | 0 | 0 | 0 | 0 | 0 |

Bit-identical wall. `A0` = validated cold read; `E0` = no error bits.

## 6. Blind spots

- **No LA confirmation the frame was actually gapless on the wire.** The
  double-buffered priming executed (code path compiled in and reached), but
  whether SCK truly ran continuous with no stall is inferred from the register
  order, not measured. Same class of blind spot as EXP-29's dummy — and exactly
  what H7's logic-analyzer capture exists to resolve.
- Tests intra-frame cadence only.
- One unit.

## 7. Conclusion

- **Established:** clocking the prelude frames gaplessly (matching both working
  transports' continuous clocking) does NOT break the wall on unit #1.
- **Excluded:** the intra-frame polled-cadence stall (post-H6 suspect a) as the
  differentiator.
- **THE FIRMWARE SEARCH IS EXHAUSTED.** Every firmware-reachable axis is now
  excluded: bytes (issue #18), inter-byte gap (EXP-26), 0x05 (EXP-27), clock rate
  (/2 and /256), DMA-vs-polled (H3), CS-high dummy (EXP-29), SPI3 CTRL1 + pin
  config (H6), and intra-frame cadence (this). Combined with H6's proof that the
  registers are identical to stock at CONFIG_ENABLE, **no register snapshot or
  firmware knob can explain the wall.**
- **The remaining suspect is boot-history / FPGA-receptivity state** — and the
  only instrument that can see it is a logic-analyzer capture of a full **stock**
  boot (which succeeds) on SPI3 **plus candidate off-SPI3 control lines**,
  diffed against our boot. The long-standing suspicion (Exp G/B2 tension) that
  stock asserts a trigger on a pin the issue-#18 capture never watched is now the
  live lead. → H7.
