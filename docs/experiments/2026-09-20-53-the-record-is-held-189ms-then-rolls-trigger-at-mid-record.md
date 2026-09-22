# EXP-53 — the record is held ~189 ms after PC0, then the buffer rolls again; the trigger sits at mid-record, either polarity, on level − 28

**Date:** 2026-09-20 (session started 2026-09-19 evening) · **Unit:** bench #1 · **Branch:** `bench/2026-09-14`
**Image under test:** EXP-50 two-phase read + EXP-52 un-rotation (Build Sep 15 15:37:57), flashed at the start of the session; the un-rotation was inert on it (see Blind spots, (1)).
**Scripts (scratchpad, copied into `scripts/` with this commit):** `exp50c_margin.py`, `exp50d_tear.py`, `exp50e/f/g_anchor.py`, `exp50h_pulse.py`, `exp50i_selfresume.py`, `exp50j_levels.py`; raw records `exp50g/h/j_records.npz`.

## Problem

EXP-50 committed the "held record" at edge + fill + 30 ms and accepted it at 0x10. The same margin gave occasional torn records at 0x11 and 0x12. Is the derived margin wrong at slow codes, and what exactly is in the buffer as a function of time after the PC0 edge?

## Hypotheses (written before each run)

- **H1 (50c):** the record is complete at edge + fill; a hold read at any margin ≥ 30 ms is at the floor and the fill + 230 control is no better. *Falsifier:* residual climbs at small margins and settles at some M*.
- **H2 (50d):** if H1 fails because the table rate is too high, the tear index grows with margin at the *true* rate and the spectral rate from clean records is below the table.
- **H3 (50e–g):** the late record is anchored to the trigger (trigger index constant across records at one geometry).
- **H4 (50i):** the roll after the holdoff re-arms the engine by itself; pushing the arming read to 5 s leaves the edge rate unchanged. *Falsifier:* edges collapse.
- **H5 (50j):** the trigger sits a fixed number of samples before the write pointer, at the written level, one polarity.

## Procedure and controls

Sine 201.2 Hz / 50 Hz, 3 Vpp (JDS6600 ch1), range 5 (`vdiv 1 5`), AUTO unless stated, `fpga postedge 0` (derived arming read, fill + 230). Hold read varied with `fpga holdread <ms>` (absolute ms after the edge). Control for every margin row: the single arming read at fill + 230 (EXP-46/47's path), run first, same session. Body metric: max 32-block RMS of the residual against a sine fitted on samples 128–1023 (rate/drive floors: 6 at 0x10, 11 at 0x11, 22 at 0x12 for 201.2 Hz; 7–8 for 50 Hz). Tear locator: best two-segment sine split. Anchoring runs: JDS waveform code 3 (verified a triangle on the scope; code 2 is a square on this unit's firmware), 2 V + 0.5 V offset so nothing clips, drive chosen so the record spans > ½ period; records saved.

## Results

**(50c) margin sweep, 8 records each, body max median (control = single read at fill+230):**

| drive | tb | control | +30 | +60 | +100 | +150 | +230 | +330 |
|---|---|---|---|---|---|---|---|---|
| 201.2 | 0x12 | 22 | 47 | 51 | 56 | 65 | 22 | 22 |
| 201.2 | 0x11 | 12 | 11 (1/8 torn) | 11 | 11 | 11 | 11 | 11 |
| 201.2 | 0x10 | 6 | 6 | 6 | 6 | 7 (1/8) | 6 | 6 |
| 50 | 0x12 | 8 | 57 | 66 | 76 | 88 | 8 | 8 |
| 50 | 0x11 | 7 | 93 | 76 | 55 | 83 | 7 | 7 |
| 50 | 0x10 | 19* | 20 | 27 | 9 | 39 (5/8) | 27 | 8 |

*The 50 Hz/0x10 metric is unreliable (4.1 periods in the record); it is shown for completeness. H1 fails at the slow codes and the residual **rises** with margin, which no "not complete yet" story predicts.

**(50d) tear index vs margin, 50 Hz.** 0x12: 628, 704, 804, 928 at +30/+60/+100/+150 ms → slope 2.50 samples/ms = fs, extrapolating to index 0 at **edge + 189 ms**. 0x11: 232, 380, 580, 832 → slope 4.99/ms, index 0 at **edge + 188.5 ms**. At +190/+210 the tear is gone or in the head. **Spectral rate from the clean +330 records: 2500 (table 2494.9) and 4998 (table 4990.8) — H2 refuted, the table is right.** Phase jump across the tear constant per code (−136°, +129°): the whole loop is phase-locked to the drive, so a free-run snapshot at a fixed delay after the edge has a fixed phase.

⇒ From ~189 ms after the edge the buffer is being **overwritten at fs from the frozen pointer onward, continuously** (seam = fs × elapsed, mod 1024; at +230 → 105, at +330 → 355 at 0x12, both observed exactly). The "record" read at fill + 230 (EXP-46/47's path, and the 4-day soak's arming read) is a **free-running snapshot** that only looked static because of the phase lock. The buffer is frozen only from the edge until edge + ~189 ms.

**(50e–g) anchoring with a triangle:** all records are rotations of a continuous segment (first and last samples adjacent in time), fits rms 0.7 after un-rotation, but the un-rotated trigger index fell into two clusters ~280 samples apart at both codes; H3 as stated was not decidable — see 50j.

**(50h) NORMAL + square/pulse (1 Hz at 0x12, 4 Hz at 0x10): 0 edges, 8 identical records, at both codes.** AUTO on the same square: 7 and 17 edges / 12 s. **Open defect: NORMAL does not trigger on a square at these rates.** (The JDS duty register did not produce 10 %; not pursued.)

**(50i-a) self re-arm — H4 REFUTED:** NORMAL, 0x10, 201.2 Hz, hold read edge+40: arming read derived → **+37 edges / 12 s**; arming read at edge + 5000 → **+2**; derived again → **+38**. The roll after the hold is autonomous but the trigger detector is armed only by a read.

**(50i-b) corrected hold read, edge + 40 ms vs the rolling read at edge + 250, 6 records:**

| drive | tb | edge+40 | edge+250 |
|---|---|---|---|
| 201.2 | 0x10 | 6 6 6 6 6 6 | 70 6 7 7 6 7 |
| 201.2 | 0x11 | 11 11 11 12 11 12 | 19 11 11 11 11 21 |
| 201.2 | 0x12 | 22 22 22 22 22 23 | 38 61 61 46 62 62 |
| 50 | 0x11 | 7 8 7 7 7 8 | 79 90 87 86 82 86 |
| 50 | 0x12 | 8 8 8 8 8 8 | 81 84 77 80 77 82 |

**(50j) level sweep, 2 Hz triangle spanning 33..133 at 0x12, frozen buffer (edge+40), 10 records per level.** Distance back from the pointer to the last crossing of **(level − 28)** in record scale:

| level written | level − 28 | edges / 12 s | back-to-rise | back-to-fall |
|---|---|---|---|---|
| 54 | 26 (below floor) | **0** | none | none |
| 83 | 55 | 17 | 510 507 504 / (245 240…) | 521 514 506 519 510 509 514 / … |
| 104 | 76 | 16 | 513 508 506 506 | 510 523 511 511 510 508 |
| 128 | 100 | 23 | 501 513 506 509 504 508 | 509 514 514 517 |
| 152 | 124 (peak is 133) | 18 | 509 512 513 508 513 508 513 508 508 | 513 |

In every record one polarity's crossing sits **504–523 samples before the pointer**; which polarity varies record to record. The 28 is required: at level 152 the record never reaches 152 yet triggers; at 54 the record dips to 33 yet never triggers. Earlier edge counts (177 → 0, 82 → 8, 128 → 11, 54 → 0 on the same signal) agree.

## Blind spots

1. The un-rotation on the tested image never applied: the hold read is also an opcode-0x04 read and overwrote the arming timestamp, so every cycle computed a negative latency and bailed. Found by readback (`acq rotation: -1`), fixed in the v2+ image (stamp only at the three arming sites). EXP-52 proper has not run yet.
2. The body metric excludes 0..127, so a seam in the head is invisible to it — that is how the rolling read at fill + 230 passed EXP-46/47/50 at 0x10 (phase lock put the seam in the head).
3. The drive periods in 50e–g were the same number of samples at both codes, so a fixed-sample shift and a fixed-phase shift were indistinguishable; 50j did not need that distinction.
4. Whether the arming read resets the pointer (EXP-51's model, L = latency × fs) or the pointer simply keeps rolling (L = fs × (T_cycle − 189 ms)) is **not decided here** — it is the EXP-52 run on the v4 image (arm→edge timestamp vs the seam in the data).
5. "Either polarity" may be a mode selected by reg 0x02 (stock writes `02 03`); untested.
6. The 28-code offset between comparator and record is measured on range 5 only.

## Conclusion

- **The FPGA captures 512 samples after a crossing of (reg-0x08 − 28), either polarity, pulses PC0 when the record is complete, holds the buffer ~189 ms, then resumes rolling from the pointer.** The trigger is at raw index (pointer − 512) mod 1024: mid-record. The 189 ms is a hold, not a re-arm refusal; the fill + 190 re-arm bracket of EXP-46 is the hold plus one roll.
- **Read the held record at edge + 40 ms (or 64 samples + 15 ms, whichever is longer), at every timebase.** Accepted at 0x10/0x11/0x12 with the rolling read as the negative control. EXP-50's fill + 30 was right at 0x10 only because edge + 112 < 189. The firmware default is changed accordingly (`fpga_acq_hold_read_get`).
- **The arming read stays** (H4 refuted). The old single-read-at-fill+230 path returns a phase-locked free-run snapshot, not the triggered record; EXP-46/47/50's "static body" acceptances were real but could not have told the two apart.
- **Withdrawn:** EXP-42's "first 32–64 samples invalid" and EXP-51's "index L is the trigger point" (the seam is the pointer; the trigger is 512 before it). Whether L = arm→edge latency × fs is EXP-52's question, still open.
- **New open items:** NORMAL does not trigger on a square at 1–4 Hz; reg 0x02 as polarity select; the pointer origin (EXP-52).

## Postscript (2026-09-22, v4 image with the arming timestamp fixed) — PC0 is a handover strobe

With the stamp taken only at arming reads, `status` reports **arm → edge = 6–9 µs, on 50 consecutive cycles at 0x10 and 0x12** (201.2 Hz), never more. A capture cannot complete 9 µs after the read that supposedly starts it; the read is finding a record that is *already complete* and the FPGA strobes PC0 as it hands it over. That reconciles every earlier observation:

- EXP-41 "one read → +1 edge" (idle +0) — a read hands over, idle does not.
- Level 177 → 0 edges while AUTO kept reading (EXP-53) — no completed record, no strobe.
- Arming read at 5 s → 2 edges (50i) — no read, no handover.
- The "held record" at edge + 40 ms is the same record the arming read already returned; EXP-50's "190 ms latency win" measured edge → commit of a re-read, not trigger → display.
- EXP-46's re-arm bracket = hold (189 ms) + one fill: the earliest a *new* record can be complete after a handover.
- **The NORMAL freeze on slow squares (50h):** the single arming read landed before the crossing, found the roll, and nothing read again — no read, no handover, ever. AUTO recovered only because its fallback read *is* a read.

**Consequence: the loop is a poll**, which is exactly what stock's 29 ms cadence was (EXP-48/49 read that as a poll of a *held* buffer; it is a poll for a *handover*). Implemented as the v5 image: after a handover sleep fill + 100 ms, then read every 30 ms; a read that strobes PC0 is committed as the record, one that does not is the roll and is discarded (AUTO commits it as free-run once its budget expires). Acceptance: `scripts/exp54_poll_acceptance.py`. EXP-52's question ("does the pointer start at the arming read") is dissolved: there is no arming read, and the pointer is set by the FPGA's own cycle, which the MCU cannot timestamp. The un-rotation from timestamps is retired; `fpga unrotate` stays off.
