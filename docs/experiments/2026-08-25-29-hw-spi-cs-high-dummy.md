# EXP-29 — CS-HIGH dummy byte does NOT break the hardware-SPI wall (H2)

- **Date:** 2026-08-25
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-configA-csdummy` (hardware-SPI config, `FPGA_CONFIG_A`,
  `FPGA_HW_CS_DUMMY=1`, SCOPE_DEBUG_OVERLAY)
- **Status:** CLOSED — negative. H2 (CS-high dummy) **excluded**. Notable because
  it refuted a *doubly-converged* hypothesis.

## 1. Problem

The bit-bang loader (works) and stock's SPI3 config path (works) both clock a
`0x00` dummy byte with **CS HIGH** before every config frame; our failing
hardware-SPI path (`fpga_spi3_config_sequence`) *explicitly removed* exactly this
(fpga.c ~4195, "8 stray SCK edges … desync the parser. Removed entirely"). Is
that missing CS-high dummy why hardware-SPI config entry fails? (H2 of
`docs/config_entry_hw_spi_hypotheses.md`.)

## 2. Hypothesis

The Gowin SSPI command FSM needs idle clocks between frames to advance state. If
so, re-adding the dummy flips the wall:

- **H true →** `CFG` shows `D1` / anchor `A-1` (Exp L port-closed) / demo trace
  gone.
- **H false →** `CFG:00039020 D0 A0` — the wall, unmoved.

## 3. Why this was the lead

Two independent zero-hardware agents converged on it the same day:
- **H4** decoded stock's config path from *machine code* (flash
  `0x0802D5E8`–`0x0802DB28`): per-frame `CS HIGH → clock 0x00 dummy (CS HIGH) →
  CS LOW → cmd → 0x00 → CS HIGH`, for 05/12/15/3B. Catch-all found no other
  SPI3-config-region divergence.
- **H1H2** confirmed our bit-bang `bb_cmd16` (fpga.c:3881) clocks `bb_xfer(0)` =
  8 SCK edges CS-HIGH before every frame, and the HW path deleted it.

Both working transports have it; the one failing transport is the only one that
removed it. That is as strong as a firmware-only lead gets on this project.

## 4. Procedure

`FPGA_HW_CS_DUMMY=1` inserts `(void)spi3_xfer(0x00)` immediately before each
prelude `SPI3_CS_ASSERT()` (05/12/15 frames; mode-0 default). `spi3_xfer` clocks
8 SCK edges on the DT write regardless of the software-GPIO CS, so this is an
exact functional mirror of `bb_xfer(0)`. IAP-flash → **true power cycle** (POWER
→ "Goodbye" → unplug USB → replug) → scope mode → read LCD `CFG:`.

Knob wiring verified in source: `fpga.c:440` macro, guards at `fpga.c:4304/4322/
4350/4361`, `Makefile:741` passes `-DFPGA_HW_CS_DUMMY=1`. Build linked clean
(text 600908).

## 5. Control

- **Anchor `A`** = IDCODE (`0x0120681B`) bit-offset on the same read. `A0` =
  read path valid AND part cold/unconfigured — the per-read validity control.
- The wall pole `CFG:00039020 D0 A0` is the same signature EXP-26/27 recorded on
  cold reads.

## 6. Results

| read | CFG | D | E | A | L | H |
|---|---|---|---|---|---|---|
| cold | `00039020` | 0 | 0 | 0 | 0 | 0 |

`A0` → validated cold read. `E0` → no error bits: the bytes are not *rejected*,
config entry simply never happens (consistent with Exp N). Bit-identical to the
without-dummy wall.

## 7. Blind spots

- **No positive readback that the dummy byte was clocked on the wire.** The knob
  is compile-time-correct and the build flashed, so the code path executed with
  near-certainty — but nothing in the overlay confirms the extra 8 SCK edges
  appeared on SCK. An LA capture would close this; absent one, "the dummy fired
  and it still walled" rests on code inspection, not measurement. (The anchor
  `A0` validates the *read* instrument, not the *dummy transmission*.)
- Tests the CS-high dummy only. The transport's clock/edge character and the
  GPIO-AF pin drive config (drive strength / speed / CTRL1 setup) are untouched —
  and those were NOT fully diffed by H4/H5 (both noted the SPI3 CTRL1 + AF-pin
  setup lives *outside* the config-sequence region they examined).
- One unit.

## 8. Conclusion

- **Established:** re-adding the CS-HIGH `0x00` dummy before each prelude frame,
  making our HW-SPI prelude match both working transports on this axis, does NOT
  break the wall on unit #1. Reproduces the wall bit-for-bit.
- **Excluded:** the missing CS-high dummy (H2) as the hardware-SPI-vs-working
  differentiator. Joins inter-byte gap (EXP-26), `0x05` (EXP-27), clock rate,
  DMA-vs-polled (H3), and static MCU state on the excluded list.
- **NOT excluded, and now the front-runners:** (a) the **SPI3 CTRL1 + PB3/4/5
  AF-pin config** (drive strength / output speed / mode-switch ordering) — the
  gap H4/H5 left open, still a zero-hardware diff; (b) the transport's
  **electrical edge character** (BIS-3 waveform, gap already excluded) — LA-only.
- **Lesson:** convergence of two static analyses is not proof. The wall is
  decided by something neither the byte stream nor the CS/dummy framing captures —
  which pushes hard toward the SPI3 *peripheral setup* diff and the LA wire-diff.
