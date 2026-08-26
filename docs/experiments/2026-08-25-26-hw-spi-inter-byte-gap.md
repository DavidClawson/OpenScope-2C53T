# EXP-26 — inter-byte gap is NOT why hardware-SPI config fails (BIS-3, on the real part)

- **Date:** 2026-08-25
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-configA-gap GAP=<n>` (hardware-SPI config, FPGA_CONFIG_A,
  `FPGA_HW_UPLOAD_GAP_NOPS=<n>`, SCOPE_DEBUG_OVERLAY on for the LCD readout)
- **Status:** CLOSED — negative. Inter-byte gap **excluded**.

## 1. Problem

Since 2026-08-13 the scope ships by **bit-banging** the FPGA config over GPIO;
the same 115,638-byte payload over the **hardware SPI3 peripheral** silently
fails to enter configuration (`CFG:00039020`, DONE_FINAL=0, edit-mode never
engages — the wall). The rig dry-run (2026-08-20, EXP-19) measured the one
transport difference byte-level fidelity was blind to: hardware SPI streams
**gaplessly** from the FIFO, while the working bit-bang loader leaves **~1.4 µs
inter-byte gaps**. BIS-3 / runbook Appendix A: **is the inter-byte gap the
deciding factor?**

## 2. Hypothesis

If inter-byte gaps are what the Gowin config path needs, then inserting a
bit-bang-sized gap into the hardware-SPI transport flips the wall:

- **H true →** `CFG` shows **`D1`** (DONE_FINAL set) and/or the anchor **`A`
  goes to −1** (Exp L: a configured part closes its SSPI port) and/or the
  synthetic demo trace **disappears** (scope_ui.c:337 latch).
- **H false →** `CFG:00039020 D0 A0`, demo trace present — the wall, unmoved.

Both outcomes are distinguishable on the LCD, so the test is decisive either way.

## 3. Procedure

`FPGA_HW_UPLOAD_GAP_NOPS` (fpga.c, `spi3_pump_h2_record`): after each payload
byte completes (RDBF), idle `N` volatile-nop iterations before priming the next
— reintroducing a true inter-byte gap on the wire. `N=0` = the shipping gapless
double-buffered pump. Estimated timing at 240 MHz (~20 ns/iter): `N=70`≈1.4 µs
(the bit-bang match), `N=200`≈4 µs.

Per gap value: IAP-flash → **true power cycle** (POWER → "Goodbye" → unplug USB
→ replug — resets the FPGA cold and re-runs config on a genuinely unconfigured
part) → switch to scope mode → read the LCD `CFG:` line.

**Preconditions verified by readback:** every run's `A` field is the IDCODE
anchor (`0x0120681B`) bit-offset — `A0` on each read confirms the config-port
read path is valid AND the part is cold/unconfigured, not a floating-bus number.

## 4. Control

`GAP=0` (gapless) is the negative pole and the readout control in one binary
family: **`CFG:00039020 D0 E0 A0 L0 H0`** — the wall, on a read the anchor
proves valid (`A0`), part cold. Recorded first, same session.

The positive pole (a part that *can* reach `D1`/`A−1` on this unit) is
`guest-coldtrace`, bench-established repeatedly on unit #1; not re-flashed this
run, since the anchor `A` field is the per-read validity control that the
gapless-vs-wrong-number failure mode actually requires.

## 5. Results

| GAP (nops) | ≈ gap | CFG | D | E | A | demo trace |
|---|---|---|---|---|---|---|
| 0 (gapless) | 0 | `00039020` | 0 | 0 | 0 | present |
| 70 | ~1.4 µs (bit-bang match) | `00039020` | 0 | 0 | 0 | present |
| 200 | ~4 µs | `00039020` | 0 | 0 | 0 | present |

Bit-identical across the range that brackets the working bit-bang value.
`A0` on every read → each is a validated cold read, not an artifact. `E0` → no
error bits: the bytes are not being *rejected*, config entry simply never
happens (consistent with Exp N).

**Why this is an exclusion, not just three tried values:** the wall is decided
at `0x15` CONFIG_ENABLE, *before* the payload. The prelude commands
(`0x05`/`0x12`/`0x15`) are already sent through `spi3_xfer` — the **gapped**
per-byte path (it "clock-stretches between every byte", fpga.c). So config entry
was *already* being attempted with gaps on the entry-critical commands and
failed; this test added gaps to the payload too and it still failed. Gaps are
present on both sides of the wall and move nothing.

## 6. Blind spots

- **Tests inter-byte GAP only.** It does NOT vary the other transport
  differences the rig capture flagged: **clock edge rate / drive strength**,
  GPIO-push-pull vs SPI-AF character, CS setup/hold, or the clock idle level
  between bytes. The bit-bang loader differs on all of these too; any of them
  remains live.
- **The nop→µs conversion is estimated, not scope-verified.** `N=70`/`200`
  bracket 1.4 µs on a cycle estimate; a logic-analyzer capture of the gapped
  upload would pin the actual widths. The bracket is wide enough that a narrow
  window near 1.4 µs is unlikely, but not impossible.
- **One unit.** Unit #1 only.

## 7. Conclusion

- **Established:** inserting a bit-bang-sized (and larger) inter-byte gap into
  the hardware-SPI config transport does **not** break the wall on unit #1.
- **Excluded:** the **inter-byte gap** as the hardware-SPI-vs-bit-bang
  differentiator (BIS-3's gap component) — corroborated two ways (CFG `D0 A0`
  and demo-trace-present) and reinforced by the prelude already being gapped.
- **NOT excluded:** the transport's **clock/edge character and GPIO-vs-AF
  electrical drive** (BIS-3's waveform component), the **prelude reads** (BIS-1),
  the **`0x05` omission** (BIS-2 / `guest-config05`), and the **part state**
  (BIS-4). These are exactly the legs the Tang Nano rig exists to run — and the
  narrowing points at the *electrical* waveform, which the firmware gap knob
  structurally cannot reach. The dev-board track carries the weight from here.
