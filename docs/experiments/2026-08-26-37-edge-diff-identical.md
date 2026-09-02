# EXP-37 (H7 step 5) — LA edge-diff: AF and GPIO 0x15 frames are digitally identical; the differentiator is below 24 MHz resolution

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-bringup-bb-slow` + new `spi3 edgecap [reps]` verb
  (`fpga_edgecap_15()`): emits N identical `0x15` CONFIG_ENABLE frames
  (CS-high dummy `0x00`, CS-low `0x15 0x00`) over AF hardware-SPI3 at /256, a
  60 ms gap, then N over GPIO bit-bang — same pins, same frame, drive type the
  only variable. Isolated frames; does NOT configure.
- **Capture:** `reverse_engineering/captures/edgecap_af_vs_bitbang_0x15_2026-08-26.sr`
  (HiLetgo fx2lafw, 24 MS/s, D0=SCK D1=MISO D2=MOSI D3=CS, 16 reps/group).
- **Status:** CLOSED — the two frames are digitally identical at 24 MHz. The
  drive-type differentiator is **below LA resolution** (sub-42 ns / analog), or
  FPGA-internal. Progress here is now gated on a faster instrument (not on hand).

## 1. Result — structurally identical

Synchronized capture (sigrok started, `spi3 edgecap 16` fired 0.7 s in). Both
bursts decoded manually (mode 3: sample MOSI on SCK rising, MSB-first):

| | AF hardware-SPI (WALLS) | GPIO bit-bang (CONFIGURES) |
|---|---|---|
| SCK edges within CS-low, per frame | **32** (all 16 frames) | **32** (all 16 frames) |
| decoded framed bytes | `[0x15, 0x00]` | `[0x15, 0x00]` |
| CS-high dummy | `0x00` (8 clocks) | `0x00` (8 clocks) |
| mode | 3 (idle-high SCK, sample on rising) | 3 |
| SCK period | 2.125 µs (471 kHz) | 29.375 µs (**34 kHz**) |
| CS-assert → first SCK falling | 1.250 µs | 0.125 µs |
| stray SCK edges (CS high, outside dummy) | none (>42 ns) | none |

Both frames are **bit-identical in structure**: 8 dummy clocks + 16 framed
clocks = 24 SCK pulses, decoding `[0x15, 0x00]`, mode 3, dead-consistent across
all 16 reps (edge count min = max). The only digital differences are:

1. **Rate** — 471 kHz (AF /256) vs 34 kHz (bit-bang). A build artifact (see §2),
   and rate is already excluded (EXP-36); structure is rate-invariant.
2. **CS-to-first-clock lead** — AF 1.25 µs vs GPIO 0.125 µs, from `spi3_xfer`'s
   TDBE-wait + DR-write latency vs bit-bang's immediate SCK drop. Sub-µs, and the
   FPGA re-syncs on CS; not a plausible config gate.

There is **no extra edge, no glitch >42 ns, no phase difference, no framing
difference** between the frame that walls and the frame that configures.

## 2. Correction to EXP-36 — the slowed bit-bang was 34 kHz, not 470 kHz

This capture measured `FPGA_BB_HALF_DELAY=250` at **34 kHz** (period 29.375 µs,
exact). EXP-36 estimated ~470 kHz; the estimate was ~14× off (the `volatile int`
delay loop is ~14 cycles/iteration, not ~2). **EXP-36's config result stands**
(the slowed GPIO bit-bang configures — DONE_FINAL `0003F460`), but its "matched
~470 kHz" phrasing is wrong. Restated correctly:

- GPIO configures at **34 kHz** (measured, EXP-37) and at fast bit-bang rate
  (~MHz, EXP-32).
- AF walls at **470 kHz, 1.9 MHz, 60 MHz** (EXP-32/36).

GPIO's working range straddles below and around AF's failing range; outcome
tracks **drive type, not rate**. (A single directly-measured matched-rate point
was not captured — the two bursts here are at 471 kHz vs 34 kHz — but the
structural identity is rate-invariant, so the conclusion does not depend on it.)

## 3. Conclusion

At 24 MHz LA resolution the MCU-side digital waveform is **identical** between the
walling AF path and the configuring GPIO path: same bytes, clock count, framing,
mode, drive posture (both push-pull, `9`-nibble, STRONGER). Whatever makes AF's
`0x15` fail to latch config-enable while GPIO's succeeds is therefore:

- a **sub-42 ns analog feature** the 24 MHz LA cannot resolve — most plausibly a
  brief runt/transient the SPI3 peripheral emits that explicit GPIO writes do not
  (the AF path's one physical distinctive), invisible here because AF's 1.06 µs
  half-period edges are clean at 42 ns sampling; or
- **FPGA-internal** — the config FSM responding differently to an electrically
  near-identical stimulus; or
- a **sequence-context** effect the isolated-`0x15` test omits (though EXP-35's
  in-sequence single-BR run also walled).

## 4. Next — blocked on resolution, not on ideas

The bench has only the 24 MHz HiLetgo (no scope, no buffered analyzer;
`bench-hardware-inventory`). Resolving a sub-42 ns feature needs a **>100 MS/s
scope or a buffered analyzer** (e.g. DSLogic Plus ~$150) — a purchase decision.
The FT232H on hand samples at ~30 MS/s and does not help.

**This is a natural close for the config-entry investigation.** The wall is now
fully characterized: every firmware-controllable axis is matched or excluded
(EXP-27→36), and the residual is a physical-layer difference invisible at 24 MHz
(EXP-37). **Shipping is unaffected** — the bit-bang coldtrace path already
cold-boots to a live scope, and this line is understanding (plus a possible
faster hardware-SPI config path) rather than a blocker.
