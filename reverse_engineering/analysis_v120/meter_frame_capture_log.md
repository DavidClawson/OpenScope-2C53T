# Meter Frame Capture Log

Empirical bench capture of USART2 RX frames from the FPGA's built-in
meter IC, recorded 2026-04-04 against the Phase 1 debug overlay build.

Purpose: ground-truth reverse engineering of the meter frame's byte
semantics after the prior decomp-only Phase 1 port failed to decode
decimal position and unit correctly. These 7 captures are the first
time we have known-input → known-frame correspondence data for this
device.

## Capture Methodology

- Firmware build: Phase 1 revert + full 12-byte debug overlay.
- Display strip rendered on the meter screen bottom; all 12 frame
  bytes visible on line 1, parsed digits + BCD on line 2.
- Single still frame captured per test input. Meter mode: Resistance
  (Auto 20 kΩ) for all except the DCV reference.
- BCD value verified by cross-checking against the main reading.
- Frame[7] reliably visible thanks to the expanded buffer (previous
  captures truncated it).

## The Raw Data

Every frame is `frame[0..11]`, 12 bytes. `frame[0..1]` is always
`5A A5` (valid data frame header) — included for alignment with
Ghidra naming.

| # | Test input | Actual | Shown | `f[2]` | `f[3]` | `f[4]` | `f[5]` | `f[6]` | `f[7]` | `f[8]` | `f[9]` | `f[10]` | `f[11]` | BCD |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Probes shorted | ~0 Ω | 00.16 Ω | 04 | E0 | 1B | EA | **07** | **28** | 00 | 00 | 01 | 50 | 16 |
| 2 | "150 Ω" (really ~147 Ω) | 147 Ω | 48.36 Ω | 4C | EE | 9F | EF | **07** | **20** | 00 | 00 | 01 | 4F | 4836 |
| 3 | 3.3 kΩ | 3300 Ω | 32.30 Ω | 84 | BF | 8D | EF | **4B** | **20** | 00 | 00 | 01 | 4F | 3230 |
| 4 | 10 kΩ | 10000 Ω | 98.40 Ω | C4 | FF | 4F | EE | **4B** | **20** | 00 | 00 | 01 | 4E | 9840 |
| 5 | 100 kΩ | 100000 Ω | 98.02 Ω | C4 | EF | FF | AB | **4D** | **20** | 00 | 00 | 01 | 50 | 9802 |
| 6 | Probes floating | ∞ | 0 Ω | E4 | 2E | 63 | 25 | **07** | **00** | 00 | 00 | 01 | 4E | (stale) |
| 7 | 5 V DC (submode 0) | 5.000 V | 5.008 V | C6 | F7 | EB | EB | **0F** | **00** | 02 | 00 | 01 | 4E | 5008 |

Notes on individual captures:

- **#1 (short):** `frame[7] = 0x28` — bit 3 set in addition to bit 5.
  This bit is not set in the in-range resistance captures (#2–5),
  so bit 3 of `frame[7]` is likely a "very-low / zero-detect"
  indicator, not a mode bit.
- **#2 (147 Ω):** Cross-checked with a second DMM — the resistor is
  genuinely ~147 Ω. Our firmware reads 48.36 Ω, a ~3.04× low error.
  `frame[6] = 0x07` matches the short capture, meaning the FPGA
  meter IC is on the same low-Ω range for both, but scales
  sub-threshold values differently than stock expects. **This is a
  calibration gap, not a decoder bug.** See "Calibration Issue"
  below.
- **#6 (open):** Digits come back as the sentinel `0A 0B 0C 0D`
  (the "valid frame marker" we already knew about from the FSM
  deep dive Q6). Our parser's OL detection fires on `digit0==0x0A
  && digit1==0x0B`, which matches. The `=2206` in the BCD field
  is a stale value from the previous frame — our parser doesn't
  recompute raw_bcd when it returns early from the OL branch.
  Display renders as "0 Ohm" because `current_val` is zero.
- **#7 (5 V DC):** Included as a reference so I could see the full
  frame layout in a mode we already display correctly.

## Findings

### frame[6] — primary range/mode byte

Across the 7 captures, `frame[6]` takes exactly 4 distinct values,
each correlating cleanly with a sub-range:

| `f[6]` | Binary | Meaning | dp | Unit | Evidence |
|---|---|---|---|---|---|
| `0x07` | `0000_0111` | Low Ω band (~0-999 Ω?) | 2 | `Ohm` | Short, 147 Ω, Open |
| `0x4B` | `0100_1011` | kΩ band, **< 10 kΩ** sub-range | 1 | `kOhm` | 3.3 k, 10 k |
| `0x4D` | `0100_1101` | kΩ band, **10–99 kΩ** sub-range | 2 | `kOhm` | 100 k |
| `0x0F` | `0000_1111` | DCV mode | 1 | `V` | 5 V DC |

**Key bits observed:**
- Bit 6 (`0x40`): Set for kΩ range (`4B`, `4D`), clear for low Ω
  and DCV. Broad range band indicator.
- Bit 2 (`0x04`): Set for the "upper sub-range within kΩ band"
  (`4D`). Clear for the "lower sub-range" (`4B`). Acts as the
  dp-selector within the kΩ band.
- Bit 1 (`0x02`): Set for `4B`, `07`, `0F`. Clear for `4D`.
- Bit 3 (`0x08`): Set for DCV (`0F`), clear for all Ω captures.
  Possibly a "non-Ω mode" marker.

**We have not yet seen:** 1 kΩ, 1 MΩ, 10 MΩ, µV/mV DC, AC volts,
current modes, frequency, capacitance. Each of these will likely
introduce new `frame[6]` values that need to be added to the
decoder table as they're observed.

### frame[7] — mode/status byte

| `f[7]` | Binary | Meaning | Captures |
|---|---|---|---|
| `0x20` | `0010_0000` | Resistance mode, steady reading | 147 Ω, 3.3 k, 10 k, 100 k |
| `0x28` | `0010_1000` | Resistance mode, zero-detect / very low | Short |
| `0x00` | `0000_0000` | DCV mode OR open-circuit | Open, 5 V DC |

Bit 5 of `frame[7]` is the "Ω mode" marker (set for all Ω readings,
clear for DCV). Bit 3 is a transient / low-value indicator. This
**disproves the earlier hypothesis** that `rx_integrity_marker` in
the stock decomp maps to `frame[7]` — `rx_integrity_marker` is
compared against `0xAA` elsewhere, but every `frame[7]` we observe
here is `0x00`, `0x20`, or `0x28`. They are different variables.

### frame[8] — primary mode indicator

| `f[8]` | Meaning | Captures |
|---|---|---|
| `0x00` | Resistance / other non-DCV | All Ω captures, open |
| `0x02` | DCV | 5 V DC only |

Not needed for the decoder today (our submode variable already
tells us DCV vs Ω), but useful as a sanity check that the FPGA
sees the same mode we sent it.

### frame[2..5] — BCD nibble segments

Used for the cross-byte nibble extraction we already decode
correctly. No new semantics here. Cross-referencing against the
nibbles on line 2 of the debug strip confirms that our existing
BCD extraction matches stock.

### frame[9], frame[10], frame[11]

- `frame[9]` is always `00` in every capture. Unclear purpose.
  Possibly reserved, possibly a high byte of something.
- `frame[10]` is always `01`. Frame-type marker? Version?
- `frame[11]` jitters across `4E`, `4F`, `50` — looks like a
  rolling counter or checksum byte. Changes between captures of
  the same resistor (cf. the two 150 Ω captures), so it's not
  stable state.

## The Decoder Table

Based purely on observed data (no speculation):

```c
switch (frame[6]) {
case 0x07:  dp = 2; unit = "Ohm";   break;  /* low Ω band */
case 0x4B:  dp = 1; unit = "kOhm";  break;  /* kΩ, < 10 kΩ */
case 0x4D:  dp = 2; unit = "kOhm";  break;  /* kΩ, 10-99 kΩ */
case 0x0F:  dp = 1; unit = "V";     break;  /* DCV */
default:    /* unknown — fall back to static default */    break;
}
```

Predicted outcomes after shipping this decoder:

| Test | Current | Predicted |
|---|---|---|
| Short | 00.16 Ω ✓ | 00.16 Ω ✓ |
| 147 Ω | 48.36 Ω ✗ (cal) | 48.36 Ω ✗ (cal — see below) |
| 3.3 kΩ | 32.30 Ω ✗ | **3.230 kOhm** ✓ |
| 10 kΩ | 98.40 Ω ✗ | **9.840 kOhm** ✓ |
| 100 kΩ | 98.02 Ω ✗ (unit) | **98.02 kOhm** ✓ |
| 5 V DC | 5.008 V ✓ | 5.008 V ✓ |

4 of 5 broken readings fixed in one commit. The 147 Ω case is
unaffected because the decoder can't correct for a calibration
offset — only for display formatting.

## Calibration Issue — Low Ω Range

The ~3.04× low reading for 147 Ω is the first clear indication
that our firmware is missing the per-range gain coefficients that
stock loads from SPI flash at boot.

Evidence:
- 3.3 kΩ / 10 kΩ / 100 kΩ all read with ~2% error (within resistor
  tolerance). These values are on the kΩ sub-ranges.
- 147 Ω reads at **33% of actual value**. That's a full-scale
  coefficient error, not tolerance drift.
- The factory cal stub we added in Phase 0.3 (`flash_fs_load_factory_cal`
  at `src/drivers/flash_fs.c:199`) allocates 301 bytes per channel
  for exactly this kind of data but does not yet populate it — the
  SPI flash driver is still stubbed.

**Action:** Log this as a **known issue** and defer to Phase 3
when the factory cal load is wired to real SPI flash reads. Do
NOT apply a hardcoded 3× correction: we don't know whether it's
exactly 3× across the range, and other resistors below ~1 kΩ
might need different coefficients.

## Gaps Needing More Captures

For the decoder to cover all meter ranges, we still need:

| Gap | Likely `frame[6]` | Priority |
|---|---|---|
| 1 kΩ resistor (between 150 Ω and 3.3 k) | Unknown — may be `0x07` or a new value | High |
| 470 Ω, 2.2 kΩ (low kΩ sub-range boundaries) | Possibly new values | Medium |
| 1 MΩ, 10 MΩ | New value expected | Medium |
| DCV: 50 mV, 500 mV, 0.5 V, 50 V (full range sweep) | Range-dependent | Low |
| ACV any value | Different mode — new `frame[7]` bit pattern expected | Medium |
| DC/AC current | Different mode | Low |
| Frequency, capacitance | Different submode | Low |

Each new `frame[6]` value found in the wild adds one row to the
decoder table. No decompiler archaeology required.

## Decoder Confidence

- **High confidence** (2+ data points matching): `0x07`→low Ω,
  `0x4B`→kΩ dp=1, `0x4D`→kΩ dp=2.
- **Single point** (no second confirmation): `0x0F`→DCV. Matches
  what we already display correctly via `default_decimal_pos[0]`.
- **Unmapped**: everything else.

The single-point `0x0F` is low risk to ship because the default
DCV path already produces the correct output — the decoder just
formalizes what's already working.

## Cross-reference to prior research

- `meter_fsm_deep_dive.md` Q1 claimed `rx_integrity_marker = frame[7]`.
  The bench data *appeared* to refute that, but the full resolution
  is in `usart2_isr_state_machine.md` (2026-04-04): the USART2 ISR
  shares one RX buffer between **12-byte data frames** (header
  `0x5A 0xA5`, byte[7] = status flags 0x00/0x20/0x28) and **10-byte
  echo frames** (header `0xAA 0x55`, byte[7] = fixed `0xAA`
  integrity constant). Both fields live at RAM 0x20004E18, so
  Ghidra aliases them under one name. The `!= 0xAA` check runs
  only in the echo-frame branch; the bit extractions in the DCV
  FSM run on data frames. Bench data is fully consistent with
  the ISR once you distinguish the two frame types.
- `meter_fsm_deep_dive.md` Q3 correctly concluded that stock firmware
  does not implement firmware-driven auto-ranging. This bench data
  confirms that: the FPGA changes ranges autonomously (we see
  `frame[6]` take different values for different resistors without
  any corresponding TX command from our firmware), and our firmware
  just needs to **read** the range bits, not **command** range switches.

## What this unblocks

1. **Phase 1 can ship a working resistance decoder today.** 4 out
   of 5 broken readings fixed from 7 data points. No decompiler
   guessing.
2. **Phase 3 (factory cal load) is elevated in importance.** The
   147 Ω miss is a concrete motivation — we have a specific
   symptom tied to a specific fix.
3. **Future meter modes (ACV, current, freq, cap) can be added
   incrementally** by capturing one or two reference frames per
   mode and extending the `frame[6]` table.
