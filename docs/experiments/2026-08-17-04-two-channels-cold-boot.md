# EXP-04 — Two independent channels from a cold boot on open firmware

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` @ `ca7f28b` (per-mode GPIO posture fix)
- **Status:** **CONFIRMED**

## 1. Problem
After EXP-03 localised the fault to our runtime, the decode of stock's
scope-mode entry identified two unapplied GPIO postures: a 2-bit channel mask on
PC1/PC2 (`ms[0x14]`, stock `0x0802E202`) and the meter mux on PC11. Does
applying them give two channels and a working meter **from a cold boot**, with
no stock firmware and no warm handoff?

## 2. Hypothesis
If the channel mask was the whole CH2 story, then with mask 3 (PC2 H, PC1 L)
`op04` carries the CH1 jack's 100 Hz tone and `op05` carries the CH2 jack's
250 Hz tone. Falsifier: one channel again, or a result that does not reverse
when the mask changes.

## 3. Procedure
**True power cycle first** (POWER → "Goodbye" → unplug USB → replug) so the FPGA
is genuinely cold and our own bit-bang loader must configure it. This matters:
measuring on the leftover warm-handoff configuration would have let us claim
"cold boot works" on stock's config.

100 Hz on the CH1 jack, 250 Hz on the CH2 jack, `spi3 seq 01 10` (~14.6 kS/s so
both tones resolve), front end at PC12=0 and mux code 5.

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| mask must be REVERSIBLE, not a lucky boot | mask 1 collapses to one channel | mask 1 → op05 shows CH1 21.80, CH2 tone 0.88 | ✅ |
| return to mask 3 | two channels again | op04 CH1 18.32, op05 CH2 55.11 | ✅ |
| CH1 liveness inside every reading | ~20 throughout | 18.6–22.6 in all 8 mux rows | ✅ |
| detector must SEE 250 Hz | validated in EXP-03 | bin 17 @ 23.4 on a known-good jack | ✅ |

## 5. Results
| mask | op04 100 Hz | op04 250 Hz | op05 100 Hz | op05 250 Hz |
|---|---|---|---|---|
| **3 = BOTH** | **17.24** | 1.91 | 3.98 | **37.15** |
| 1 = CH1 | 15.77 | 2.56 | 21.80 | 0.88 |
| 2 = CH2 | 0.62 | 64.62 | 1.88 | 61.64 |
| **3 again** | **18.32** | 1.54 | 3.54 | **55.11** |

**CH2 clipping, and the front end matters more than expected:**
- A first pass read `op05 = 0.00` exactly. Cause: cold-boot front-end defaults
  leave CH2's input disconnected — NOT a configuration failure. Restoring the
  front end produced the table above.
- CH2 ranges ≥5 give `railed = 1024/1024`: PA15 drops with bit 0 of the relay
  table, disconnecting the input. CH2 is only usable at ranges ≤4.
- Sweeping CH2's mux with PA15 held HIGH, only codes 1/2/3 connect; 0 and 4–7
  rail, which looks like unconnected taps.
- At code 3, CH2 is clean below ~300 mVpp: span 34 @ 300 mV and 17 @ 150 mV,
  both `railed=0` — about **8.8 mV/count**, linear. At 2000 mVpp, 442/1024
  samples rail, and **a clipped sine renders on the LCD as a square wave**
  (user-reported, and it matched the railed count exactly).

**Screen (weak evidence, but the deliverable):** two visibly distinct traces.

## 6. Blind spots
- The mask is applied from `fpga_set_scope_frontend_ranges`. If the capture
  engine ever arms before that call, the mask would not be set when it matters;
  this test does not prove the ordering is correct in general.
- Absolute volts are not calibrated — 8.8 mV/count is one tap on one channel.
- CH2's DC headroom looks asymmetric (500 mVpp already clips 134 samples while
  300 mVpp is clean), suggesting an offset eating headroom. Coupling (PD12/PD13)
  was not varied here.
- The meter half was NOT re-tested after this power cycle; PC11 was proven in
  the warm-handoff state only.

## 7. Conclusion
- **Established:** two independent scope channels from a cold boot on open
  firmware, reversible under mask control. The CH2 fault was a 2-bit
  channel-enable code on PC1/PC2 that our firmware left floating, landing in
  mask 1 = CH1 into both converters.
- **Explains, in one mechanism:** both buffers carrying CH1 at full amplitude;
  CH1's attenuator moving both; CH2's relay bank moving neither; two genuinely
  distinct converters (EXP-01) both fed from one source.
- **NOT established:** the meter on a cold boot; CH2 absolute calibration; the
  arm-vs-mask ordering; CH2's usable range table (only 3 of 8 taps connect).
- **Follow-up:** re-test the meter cold; fix the relay table so CH2's input stays
  connected across its usable ranges; derive CH2's gain ladder properly.
