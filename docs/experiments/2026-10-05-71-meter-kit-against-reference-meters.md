# EXP-71 — the meter against a component kit and two reference meters

- **Date:** 2026-10-05
- **Unit:** bench unit #1 (V1.4)
- **Build:** `make guest-coldtrace-meter` on branch `fix/meter-sign-decade` (Build Oct 5 2026 12:35:47), i.e. main `9170a50` + the sign fix `94c1340`
- **Status:** **CONFIRMED.** Two decoder defects found and fixed. A third (the sign) was confirmed fixed on hardware. Every reading the decoder handles correctly agrees with an independent meter within 0–1.5 %.

## 1. Problem
Dev plan 2.4 ("the eight untested meter words on unit #1"): resistance, capacitance and diode
had been checked only on unit #2, against nominal part values with no reference meter.
Issue #37 left a decoder question open: does frame[8] bit 7 mean "four decimals" in ohms? Its
evidence conflicted across units. And the 10-03 sign fix had never seen a negative voltage.

## 2. Hypothesis
With a reference meter beside each part, every disagreement can be placed: in the meter chip
(its own digits are wrong), in the frame encoding (the bits mean something else), or in our
decoder (it reads correct bits wrongly). Kit tolerances alone cannot separate the three.

## 3. Procedure
- **Kit parts** went on the meter leads, captured with `scripts/meter_kit_bench.py`. That
  script switches the meter function (`mode meter <n>`), waits for 3 identical fresh frames,
  and logs the decoded text beside the raw frame. Long captures used a 10–15 s loop that
  records every distinct frame.
- **References:**
  - David's handheld DMM for resistance;
  - a **FNIRSI DMC100** for capacitance, diode and DC volts.
- **Each part** was measured on the reference first, then on ours.
- **Resistors:** from 100 kΩ up, the part lay on the table with its leads wrapped round the
  probes.
- **Log:** `reverse_engineering/captures/meter_kit/2026-10-05.jsonl`.

## 4. Control
The parts that decoded correctly are the control for those that did not: same unit, leads,
script and session. The reference meter is the control for the kit's tolerance.

## 5. Results

| Part | Reference | Frame (bytes 2–8) | Ours, before | Ours, after fix |
|---|---|---|---|---|
| 100 Ω | 98.9 Ω | `C4 CF 9F 8A 0A 20 00` | 99.77 Ω | 99.77 Ω |
| 220 Ω | 218.2 Ω | `A4 AD ED 4B 4E 20 80` | **2.204 kΩ** | 0.2204 kΩ |
| 1 kΩ (1 %) | 1.005 kΩ | `EC 0B EA 8B 4F 20 80` | **10103 kΩ** | 1.0103 kΩ |
| 2.2 kΩ | 2.175 kΩ | `A4 1D 8A CA 47 20 00` | 2.175 kΩ | 2.175 kΩ |
| 10 kΩ | 9.68 kΩ | `C4 FF 87 EA 47 20 00` | 9.676 kΩ | 9.676 kΩ |
| 100 kΩ | 99.7 kΩ | `EC 0B 1A CA 47 20 00` | 101.15 kΩ | 101.15 kΩ |
| 1 MΩ | 0.994 MΩ | `C4 CF 4F 8E 0A 24 80` | 994.7 kΩ | 994.7 kΩ |
| 104 ceramic | 61.4 nF | `E4 E7 5B CE 27 10 00` | 60.45 nF | 60.45 nF |
| electrolytic | 227.5 µF | `A4 AD ED 9F 1F 10 00` | 228.3 µF | 228.3 µF |
| 1N4007 | 0.635 V | `E0 FB 87 EF 8B 00 02` | **rejected** | 0.630 V |
| red LED | 1.817 V | `00 FA EF EB 8F 00 02` | **rejected** | 1.808 V |
| AA cell | 1.615 V | `EE 07 8A 4F 0E 00 82` | 1.6134 V | 1.6134 V |
| AA cell, reversed | −1.615 V | `FE 07 CA 87 0F 00 82` | **−1.6153 V** ✓ | −1.6153 V |

**The resistance decade (frame[8] bit 7).** An explicit decimal point in the digits always
wins. Without one, bit 7 marks the 2 kΩ range, shown with four decimals (`x.xxxx kΩ`), and
frame[2] bit 3 is a leading 1. That rule fits every reference-backed frame. The 220 Ω row was
**predicted before it was measured**:

- **The prediction.** Unit #2's "2.2 kΩ" fixture (`2168`, bit 7 set, no point) should be a
  220 Ω part. If so, a 220 Ω here sends the same shape: digits ≈ 2xxx, bit 7 set, no point.
- **The result.** It did (`2203`/`2204`, bit 7, no point; DMM 218.2 Ω). A 2.2 kΩ here sends an
  explicit point with bit 7 clear.
- So unit #2's part was a 220 Ω, reading 216.8 Ω, and its old expected value (2.168 kΩ) was a
  hand decode with no reference behind it.

**Diode mode.** On this meter chip a diode test is a voltage measurement: it carries the
voltage marker (frame[8] = 0x02). The frame-family model therefore classified it as DC volts
and rejected it in diode mode. EXP-206 had called that "the expected negative", with no diode
on the leads. frame[6]'s upper nibble is an annunciator field, outside the digit nibbles:

| Frames | upper nibble |
|---|---|
| 1N4007 (`8B`), LED (`8F`), unit #2's open-diode frame (`80`) | **8** |
| every DC-volts frame (unit #1 ×2, saul ×7, fixtures ×4) | **0** |

Classifying "voltage marker + annunciator 8" as diode keeps the stale-frame guard in both
directions.

**The sign** (fixed 10-03 from @saulvalenzuela23's frames, host-tested only until now). The
reversed cell sends frame[2] = `0xFE` (bit 4 set) with frame[7] = `0x00`. The pre-fix decoder
read only frame[7] bit 0, so it would have shown +1.6153 V.

**The first 100 kΩ capture** read 89–94 kΩ and drifted, with fingers across the leads. Kept
in the log, not used.

## 6. Blind spots
- **Neither reference meter is calibrated.** The 0–1.5 % agreement shows the decoder
  reproduces what the meter chip measured, not that either meter is accurate. 100 kΩ (+1.5 %)
  and 100 Ω (+0.9 %, probably lead resistance) can't be assigned to either meter.
- **Not tested:** continuity, temperature, AC volts, currents, the frequency/duty functions.
  The 2 kΩ-range rule was seen in resistance only; continuity shares the branch, untested.
- **One unit**, plus the frames from saul's board and unit #2.
- **The fixes are host-tested here.** The re-check of the fixed image on the device is a
  separate step (follow-up).

## 7. Conclusion
- **Established:**
  - frame[8] bit 7 = the 2 kΩ range (four decimals) in ohms when no explicit point is
    present, the same meaning as in DC volts;
  - frame[6] upper nibble 8 = diode;
  - the sign fix works on hardware;
  - capacitance nF/µF and the leading "1" in kΩ decode correctly against a reference.
- **Corrected:** unit #2's "2.2 kΩ" fixture was a 220 Ω part (216.8 Ω).
- **Fixed:** 220 Ω / 1 kΩ read 10× high; diode and LED readings were rejected.
- **Guarded:** `test_kit_frames_with_reference` checks 13 frames against the **reference**
  readings (±2 %), plus three controls:
  - bit 7 cleared → ten times higher;
  - a diode frame in DC mode is rejected;
  - a DC frame in diode mode is rejected.

  It also checks unit #2's open-diode frame now reads OL.
- **Follow-up:**
  - re-capture 220 Ω, 1 kΩ, diode and LED on the fixed image;
  - tell @Stlkv about the 220 Ω;
  - continuity, temperature and AC volts against the DMC100.
