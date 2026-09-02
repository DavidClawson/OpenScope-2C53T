# EXP-33 (H7 step 1b) — stock-exact bit-bang framing also configures; framing excluded

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-bringup-bb-faithful` (guest-bringup + `FPGA_CONFIG_B` +
  `FPGA_CONFIG_B_FAITHFUL`). `fpga configbb` now runs stock's EXACT bit-bang
  framing: empty CS pulse, 100 ms, `05 00` ERASE_SRAM, 100 ms, `12 00`, 100 ms,
  `15 00`, then `3B` + full payload, immediate `3A` close + lone `00` frame — **no
  prelude reads, WITH 0x05**. (V0.4/non-faithful omits 0x05 and does 11/13/41
  prelude reads.)
- **Status:** CLOSED — positive. Framing **excluded** as the differentiator.

## 1. Question

EXP-32 left one fork between our two config paths: our bit-bang (GPIO, succeeds) vs
our hardware-SPI (AF, walls). The bit-bang differs from the AF path in two ways at
once — GPIO-vs-AF electrical drive AND V0.4 framing (no-05, prelude reads). Does the
**framing** carry the success, or the **drive**? Re-run bit-bang with stock's exact
framing: if it still configures, framing is out and only GPIO-vs-AF drive remains.

## 2. Procedure

True power cycle → anchor → `fpga configbb` (faithful framing) → `spi3 gowin`.

Verdict is read from the **anchor transition**, NOT the `configbb` printout: the
FAITHFUL branch does stock's immediate close and never reads `cfg_status_reg`, so
its printed `STATUS 00000000 / DONE_FINAL=no` is the zeroed default (stale). Its
banner string "05-less V0.4 framing" is also static text; the compiled framing is
faithful.

## 3. Result

```
anchor (before):  IDCODE 0x0120681B  STATUS 0x00039020   -> OPEN, cold, receptive
fpga configbb:    (faithful framing fires)
spi3 gowin (after): IDCODE silent/0x00000000 at /256      -> PORT CLOSED
```

Clean open → closed transition, **properly isolated** (power cycle → anchored OPEN
→ ONE config action → CLOSED). Port CLOSED at /256 is the Exp L signature of a
successfully configured part (the SSPI pins pass to the user design and stop
answering config reads).

Contrast with EXP-32's non-faithful run of this SAME loader, which left the port
answering `0x0120681B` at close yet reported explicit `DONE_FINAL 0x0003F460`. Both
framings indicate a successful config; the faithful one matches Exp L's port-closed
shape more exactly (it does no post-upload reads to keep the port awake).

## 4. Conclusion

- **Established:** stock-exact bit-bang framing (WITH 0x05, 100 ms gaps, no reads,
  immediate close) configures the FPGA — same as the V0.4/no-05/reads framing.
- **Excluded:** SSPI config **framing** (0x05 presence, prelude reads, gap
  structure, close timing) as the bit-bang-vs-hardware-SPI differentiator. Joins
  bytes, payload, cadence, CS-dummy, and write rate on the excluded list.
- **Last standing difference between OUR two paths:** GPIO push-pull drive
  (bit-bang, succeeds) vs SPI3 alternate-function drive (hardware-SPI, walls) —
  the electrical character of how PB3/4/5 are driven, the only axis left after
  bytes / framing / rate / registers are all matched or excluded.

  **But note the cross-check that keeps this from being the whole story:** STOCK
  succeeds on AF drive. So GPIO-vs-AF cannot be a hard "AF can't configure this
  part" law. The precise open question is therefore still **"what does stock's AF
  path do that ours does not"** — with the AF *transfer dynamics* (DMA-streamed vs
  polled-with-gaps upload; H3, never run on a validated readout) the leading
  remaining candidate, distinct from the static register snapshot H6 already
  matched.

## 5. Residual ambiguity (stated, not hidden)

Port-closed at /256 = configured (Exp L) OR a dead/desynced bus — the two look
identical on a status read. Weighing against "dead bus": the transition was cleanly
isolated from OPEN, and the non-faithful run of this exact loader gave an explicit
`DONE_FINAL 0x0003F460`. A **live trace** from a faithful-framed config would remove
the ambiguity entirely (needs a coldtrace-style readout build, not this bus-released
bringup image).

## 6. Next

1. **DMA-vs-polled AF upload on a validated readout** (H3, leading) — the one
   dynamic AF difference never tested with a valid read. Stock sets CTRL2=0x03
   (RXDMAEN+TXDMAEN); we poll DT with inter-byte gaps.
2. **Live-trace confirmation of faithful bit-bang** — fold FAITHFUL into a
   coldtrace-style build and look for the demo-trace latch-off, to kill the
   port-closed ambiguity for good.
3. Shipping remains unaffected — coldtrace (bit-bang) already cold-boots to a live
   scope.
