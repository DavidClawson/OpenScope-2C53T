# EXP-72 — the ~200 ms hold is fixed in the FPGA design; stock's fast display is the roll

- **Date:** 2026-10-05 → 06
- **Unit:** bench unit #1 (V1.4)
- **Build:** `guest-coldtrace` = v0.4.1 + `fpga holdlog` + `fpga autolive` (Build Oct 5 2026 20:07:01). Uncommitted at the time of the run.
- **Status:** **ANSWERED.** The hold is a property of the design, not a register we're missing. One defect of our own, fixed behind a switch.

## 1. Problem
The scope trace refreshes at ~2–4 frames/s; stock's looks like 30–40. EXP-53 found the FPGA
holds a handed-over record ~189 ms before rolling again. The June stock capture was read as
"a fresh data-ready per 04/05 pair every ~29 ms". If both were true, stock knew a setting we
don't.

## 2. Hypotheses
- **H1:** the handover cycle is hold + fill, with the hold constant across timebases.
- **H2:** some register write (stock or unknown) shortens the hold.
- **H3:** our own loop, not the FPGA, limits the display.

Predictions were written into `scripts/exp72_hold_sweep.py` before the run.

## 3. Method
- **Signal:** JDS CH1 1 kHz sine, 2 Vpp, +0.5 V, into scope CH1 at range 5; level code 128; NORMAL. The input crosses the trigger level within 1 ms of any arm.
- **Loop settings:** `fpga postedge 1` and `fpga pollgap 5`, so the loop polls immediately and often, and the interval measures the FPGA.
- **New instrument:** `fpga holdlog`. The acq task logs every handover: ms since the previous one, roll reads polled in between, and strobes on the pair. Nothing goes over the shell inside the window.
- **Per condition:** an 8 s window, reporting the median interval.
- **Separately, H3:** commits/s measured with the input quiet (JDS off), with the release AUTO policy and with `fpga autolive on`.

## 4. Control
- **Timebase baseline:** five timebases, with 0x0E repeated.
- **Register blocks:** each block ends by writing back the stock value and the timebase, then measures again. That catches a write that breaks the engine.

**Run 1 was invalid and is not used.**
- It counted PC0 edges from the host across `status` calls. Too coarse, and ambiguous between 1 and 2 strobes per handover.
- `fpga acqbr off` does **not** restore the SPI clock; it leaves whatever is in force. So the `/256` row contaminated every later row.

The script now restores `/2` explicitly.

## 5. Results

**H1 confirmed.** Interval − fill, logged on the device:

| Timebase | 0x0E | 0x0F | 0x10 | 0x11 | 0x12 |
|---|---|---|---|---|---|
| Median interval (ms) | 223 | 244 | 284 | 408 | 613 |
| − fill (ms) | 202.5 | 203.0 | 202.0 | 202.8 | 202.6 |

Constant to ±0.5 ms over a 20× range of fill. **1 strobe per handover**, not 2: EXP-54's 2:1
ratio no longer holds after v16's 1,027-byte read. That's ~13 ms more than EXP-53's 189 ms,
consistent with 5 ms polling plus the 1 ms strobe check. Some windows show medians at exactly
2× (445 ms, one handover missed); they appear in baseline and restore rows alike.

**H2 refuted for every register we can write:**
- **reg 0x02:** 00/01/02.
- **reg 0x06 and 0x07:** each bit singly, plus FF.
- **reg 0x09–0x0F:** 01 and FF each.

All gave 200.5–205.5 ms, except reg 0x06 = 01 / FF: **zero handovers**, restored by 00. The
2026-08-15 desk sweep had already decoded reg 0x06 as **trigger source channel**. So that's
the trigger moved to the undriven CH2, not a stop bit.

Other variables:
- **Polling at stock's 29 ms:** 232 ms (poll quantisation only).
- **Our derived poll start:** 222 ms.
- **Reads at /256:** 1,107 ms. Slow reads stretch the cycle 5×; stock reads at /2.

**H3 confirmed for quiet inputs.** Input quiet → 0 strobes/s. The loop reads the roll 33.3×/s
and the release AUTO policy commits one per 475 ms budget: **2.1 commits/s**. With
`fpga autolive on`, the budget runs from the last *handover* rather than the last commit:
**33.3 commits/s**. Control: with the 1 kHz input triggering and autolive on, commits = 3.4/s =
handovers, so a triggering signal still shows only handed-over records.

**The June stock capture, re-read.** Its input was **flat**: op 04 at 164–169 throughout
(`desk_sweep_2026-08-15.md` §6: "seams are undetectable because the input is flat"). Stock was
in the untriggered regime, reading every 28.9 ms. The "18 ms" was the idle gap after each pair,
not a trigger strobe. Stock's own AUTO fallback re-arms only after PC0 has been low for
**≥510 ms** (`desk_sweep` §3). That's the shape of a design whose triggered cycle is slow, as
ours measures.

## 6. Blind spots
- Register contents can't be read back. A write the FPGA ignored looks like one it accepted and found irrelevant.
- Reg 0x00 wasn't written, and multi-bit combinations weren't swept.
- The hold wasn't measured on stock firmware. The argument that stock is equally slow when triggered rests on its 510 ms fallback timer and a flat-input capture, not on a measurement.
- How often the display task actually *draws* at 33 commits/s wasn't measured. The user's hand-held probe test was inconclusive because hand pickup (mains hum) triggers.

## 7. Conclusion
- **Established:**
  - The triggered handover cycle is fill + ~202 ms at every measured timebase. No register write we can make shortens it.
  - Our release AUTO policy discards ~94 % of the reads on a quiet input.
- **Corrected:**
  - "Stock delivers a fresh record per read pair" was a flat-input capture read as triggered.
  - Reg 0x06 is the trigger source (desk sweep 2026-08-15), not an unknown stop bit.
- **Fixed:** `fpga autolive`, default **ON** from the commit that lands this write-up (OFF in
  the image the measurements were taken on). `fpga autolive off` restores v0.4.1's policy.
- **Follow-up:**
  - **Reg 0x07 = trigger edge** (desk-sweep decode) has never been tested as a hardware edge select. EXP-55 tested reg 0x02. A triangle-wave run would settle it and could retire the MCU edge filter.
  - Measure the display frame rate.
  - A higher triggered rate would need our own FPGA design.

Logs: `reverse_engineering/captures/exp72/` (`exp72_2026-10-05_195930.jsonl` = run 2,
`exp72_2026-10-06_085519.jsonl` = wild sweep; run 1's `…194945.jsonl` is kept and is invalid).

## 8. Regression on the default-on image

Image `guest-coldtrace`, Build Oct 6 2026 10:04:11, installed over USB (`fwapply`).

**First run: RED, 2/3.** Mask 10/10 and coupling 17/17 passed; the trigger suite failed one
line, `edge-negctl` (filter OFF, 12/12 records rising). The cause was not the firmware:
- The scenario runs in NORMAL, where autolive's code doesn't run.
- Five repeats passed, three with autolive on and two with it off.
- The edges came in runs of 6–11 (`rrrrrrffffff`, `ffffrrrrrrrr`, `fffffrffffff`), because the ~612 ms capture cycle drifts slowly against the 500 ms triangle. So 12 grabs can land inside one run.

The control now takes 24 grabs (`scripts/exp22_stability.py`).

**Trigger suite re-run on the same image: PASS.** All three suites are green on this image.

Logs:
- `captures/regression/2026-10-06_Oct_6_2026_10_04_11_gcc_14_2_1_20241119/` (the RED run, kept)
- `…_trigger_rerun_n24/`

Host side: 18/18 C test targets, 15/15 Python suites (117 tests, no skips), and all five build
flavours compile.
