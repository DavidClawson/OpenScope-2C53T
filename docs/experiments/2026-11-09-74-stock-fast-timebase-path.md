# EXP-74: what stock does on a timebase change, and the measured fast-code rate ladder

- **Date:** 2026-11-09
- **Unit:** one 2C53T (a contributor's unit, neither #1 nor #3), CH1, x1 probe; static part on `APP_2C53T_V1.2.0_251015.bin` (linked at `0x08007000`)
- **Build:** `make guest-coldtrace` on `main` @ `7848b50` plus the changes this note ships with
- **Source:** crystal-locked square waves `f = 50 MHz / k` from a Zynq-7000 board (PS FCLK0 100 MHz into a PL divider, driven onto a 3.3 V header pin), probe ground on the board ground
- **Status:** CONFIRMED for codes 0x04-0x14; 0x00-0x03 measured at 125 MS/s here, stock's 250 MS/s interleave NOT reproduced (open)

All addresses are **flash** addresses (objdump `--adjust-vma=0x08007000`).
State base `ms` = `0x200000F8`; `ms[0x2D]` = timebase index.

## 1. Problem

OpenScope only had measured rates for reg-0x01 codes 0x0D-0x14 (125 kS/s and
slower). The fast codes were either "--" or flagged INCOHERENT. The target use
(2 MHz regulator ripple, ns-scale spikes) needs >= 10 MS/s. The open question
was whether stock reaches its fast timebases with extra FPGA commands (USART
0x20/0x21/0x26-0x28, trigger, readout length) that we do not send, or whether
the rate follows from reg 0x01 alone.

## 2. Findings (static, stock V1.2.0)

### 2.1 The timebase handler writes the FPGA exactly once: SPI3 op 0x01 = index

Timebase step handler, `0x0800B3C2..0x0800B7B6` (the `+` key path; the `-`
path at `0x0800B998` is symmetric):

1. `ms[0x2D]++` (bounded at 0x1C: 29 indices, 0x00-0x13 FPGA-paced, 0x14-0x1C roll/TMR3-paced).
2. Recomputes the **CH1 offset DAC** (DAC1 `0x40007408` + SWTRG) and the
   **CH2 offset PWM** (`TMR13_C1DT 0x40001C34`) from the per-range cal,
   choosing the cal **bank** by the NEW index (see 2.3).
3. If `ms[0x2E]` (frequency counter on): for index >= 20 programs TMR2/TMR3
   for roll pacing (table `0x080465A4`), else stops it.
4. `0x08030A70` (sample-period bookkeeping, 2.2), `0x0803B2F8(0,0)` (horizontal
   position bookkeeping, no MMIO), sets **PD2** (`0x40011410 = 4`, meaning unknown).
5. Queues `2` on the queue at `0x20002D6C`. **This is the UI/display queue,
   not a USART command queue**: its dispatch table at `0x0804BE74` maps item 2
   to `0x080104EC`, a draw routine that only acts in meter mode. Earlier notes
   call this queue `usart_cmd`; that label is wrong for this path.
6. Queues trigger byte `1` on the SPI3 queue (`0x20002D78`). The acq task
   (`0x0803E4B2`) sends the trigger byte as the CS-framed **opcode** and, for
   opcode 1, `ms[0x2D]` as the value (`0x0803E54C..0x0803E596`), gated on the
   PC0 poll counter (`TIMEBASE_SAMPLE_TABLE[0x0804D833] + 50`; the table is
   1 for every index <= 0x0D).

**No USART frame is sent on a timebase change.** In particular nothing
corresponding to OpenScope's `fpga_send_scope_sequence()` 0x20/0x21/0x26/0x27/0x28
block is emitted here; that block is our guess, not a stock sequence.

### 2.2 The rate ladder, from stock's own per-index table

`0x08030A70` computes stock's sample period from a `uint64` table at
`0x080465E0` (one entry per index) and doubles it when `index < 4` **and**
both channels are on (`ms[0x14] == 3`). The table, times **0.4**, reproduces
every rate this project has measured, so the factor is the stock label
geometry (the same 2.5x documented in `scope_timebase.h`):

| index | table | x0.4 = period | rate | our evidence |
|---|---|---|---|---|
| 0x00-0x03 | 10 | 4 ns | **250 MS/s** (125 MS/s with both channels on) | none |
| 0x04 | 20 | 8 ns | **125 MS/s** | frozen record (before this note) |
| 0x05 | 50 | 20 ns | 50 MS/s | none |
| 0x06 | 100 | 40 ns | 25 MS/s | "INCOHERENT" (opread artefact, see EXP-12 note) |
| 0x07 | 200 | 80 ns | 12.5 MS/s | "INCOHERENT" |
| 0x08 | 500 | 200 ns | 5 MS/s | **5.00 MS/s** (Stlkv, EXP-12 resolution) |
| 0x09 | 1000 | 400 ns | 2.5 MS/s | "INCOHERENT" |
| 0x0A | 2000 | 800 ns | 1.25 MS/s | ~0.76-1 MS/s (source-limited) |
| 0x0B | 5000 | 2 us | 500 kS/s | ~0.5-0.6 MS/s |
| 0x0C | 10000 | 4 us | 250 kS/s | ~0.3 MS/s |
| 0x0D | 20000 | 8 us | 125 kS/s | 123.7 kS/s (provisional) |
| 0x0E | 50000 | 20 us | 50 kS/s | 49.93 kS/s |
| 0x0F | 100000 | 40 us | 25 kS/s | 24.98 kS/s |
| 0x10 | 200000 | 80 us | 12.5 kS/s | 12.49 kS/s |

These were **stock's claims** when this section was written; section 5
measures them. But they turn the "INCOHERENT" flags on 0x06-0x09
into "unmeasured with a fast enough read", and they name 0x04 = 125 MS/s and
0x00-0x03 = 250 MS/s as the codes worth fighting for.

0x00-0x03 share one table value: they differ only in stock's on-screen zoom
(time/div labels from `{1,2,5}[i%3] * 10^(3+i/3)` ps, tables `0x080466C8`),
not in the FPGA rate.

### 2.3 Offset cal has three banks, selected by timebase

Confirms maksidze's issue-#28 decode with the code sites:
bank 0 for index >= 5, bank 1 for index == 4 or (index < 4 and both channels on),
bank 2 for index < 4 with one channel. On **this unit's** cal page
(`backup/cal_page_0x08006000_20261109-005724.bin`) the banks differ by at
most ~15 DAC codes per range (CH1 base 1611-1629, upper 3185-3204; CH2 base
1664-1685, upper 3221-3278). So the bank switch is a fine trim, and **cannot
explain a record railed near 0 at 0x04**.

### 2.4 Readout differences at fast indices

From the earlier orchestrator decode (`ACQUISITION_TASK_DEEP_DIVE.md`,
`scope_main_fsm`): for index < 5 stock issues both the 0x04 and 0x05 reads
regardless of which channels are on, and for index <= 3 with one channel it
treats the two 1024-byte reads as **one 2048-sample record** (0x800 wrap
instead of 0x400). Together with 2.2 (250 MS/s single, 125 MS/s dual) this
reads as the two ADC halves being time-interleaved onto one channel at
0x00-0x03. `docs/board_revisions.md` says the two converters are one per
channel and "not interleaved"; that is a statement about the analog routing
and is **not** yet reconciled with this. The bench decides.

OpenScope already reads 0x04 and 0x05 on every poll, so "we never read 0x05"
is **excluded** as the cause of the 0x04 freeze.

## 3. Procedure

Hypothesis: if the rate follows from reg 0x01 alone, then with nothing but
`fpga scope timebase <code>` the alias position of a known tone matches
stock's table (2.2) for every code; if extra commands were needed, the fast
codes would fold at some other rate or not advance at all.

Fold test per code (`scripts/measure_sample_rate.py` method, driven from the
host over the CDC shell):

1. `fpga scope trigmode auto`, `fpga scope timebase <code>`.
2. For 3-10 divider tones whose alias lies in 0.03-0.47 cycles/sample
   (chosen from ratios 0.11-2.62 of stock's rate, so codes are also tested
   above Nyquist), read three CH1 records (`waveform`), skip the first 128
   samples, Hann window, 16x zero-padded FFT, parabolic peak interpolation.
3. Fit the sample rate on a grid of +-30 % around stock's value
   (step 1e-5 relative) by least squares on the alias positions; tones with
   less than 20 counts peak-to-peak are left out.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| tone register on the source | `0x80000000 \| (k-1)` | read back per tone |
| trigger mode | Auto | `fpga scope trigmode` reply |
| CH1 offset DAC (`0x40007408`) | mid-scale, not railed | `0x800` (see 6.) |
| code in force | requested code | `fpga scope timebase` reply, reg 0x01 |

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| 0x0E, 0x0F, 0x10 (rates measured before, EXP-63) | 50 k / 25 k / 12.5 kS/s | 50 000 / 25 000 / 12 500 S/s, rms 0.00 bins | yes |
| 0x08 (5.00 MS/s, EXP-12 resolution) | 5 MS/s | 5 000 000 S/s, rms 0.00 bins | yes |

## 5. Results

### 5.1 Codes 0x04-0x14: every code matches stock's table

Best complete run per code (all runs with the source settled and trigger mode
Auto agree). "rms" is the fit residual in FFT bins of a 1024-sample record.

| code | rate | tones | tone range | fit vs stock | rms (bins) |
|---|---|---|---|---|---|
| 0x04 | 125 MS/s | 3 | 12.5 MHz - 50 MHz | +0.0000 % | 0.00 |
| 0x05 | 50 MS/s | 4 | 5.556 MHz - 16.67 MHz | +0.0000 % | 0.00 |
| 0x06 | 25 MS/s | 6 | 2.778 MHz - 16.67 MHz | +0.0000 % | 0.00 |
| 0x07 | 12.5 MS/s | 8 | 1.389 MHz - 16.67 MHz | +0.0000 % | 0.00 |
| 0x08 | 5 MS/s | 8 | 549.5 kHz - 6.25 MHz | +0.0000 % | 0.00 |
| 0x09 | 2.5 MS/s | 9 | 274.7 kHz - 4.545 MHz | +0.0000 % | 0.00 |
| 0x0A | 1.25 MS/s | 10 | 137.4 kHz - 3.333 MHz | +0.0000 % | 0.00 |
| 0x0B | 500 kS/s | 10 | 55.01 kHz - 1.316 MHz | +0.0000 % | 0.00 |
| 0x0C | 250 kS/s | 10 | 27.5 kHz - 657.9 kHz | +0.0000 % | 0.00 |
| 0x0D | 125 kS/s | 10 | 13.75 kHz - 326.8 kHz | +0.0000 % | 0.01 |
| 0x0E | 50 kS/s | 6 | 5.5 kHz - 108.5 kHz | +0.0000 % | 0.00 |
| 0x0F | 25 kS/s | 10 | 2.75 kHz - 65.53 kHz | +0.0000 % | 0.00 |
| 0x10 | 12.5 kS/s | 10 | 1.375 kHz - 32.74 kHz | +0.0000 % | 0.00 |
| 0x11 | 5 kS/s | 10 | 550 Hz - 13.1 kHz | +0.0000 % | 0.01 |
| 0x12 | 2.5 kS/s | 10 | 275 Hz - 6.55 kHz | +0.0000 % | 0.00 |
| 0x13 | 1.25 kS/s | 10 | 137.5 Hz - 3.275 kHz | +0.0000 % | 0.00 |
| 0x14 | 500 S/s | 10 | 55 Hz - 1.31 kHz | +0.0000 % | 0.00 |

The previous table did not round toward the 1-2-5 ladder on purpose
(EXP-17). This run does not round either: the round values are what the fit
returns, and the same tones reject every earlier figure. Worst miss of the
same tones under each rate (bins of 1024):

| code | this run | miss | earlier figure | miss |
|---|---|---|---|---|
| 0x0D | 125 000 | 0.01 | 123 662.7 (EXP-18, provisional) | 28.95 |
| 0x0E | 50 000 | 0.00 | 49 930.1 (EXP-17) | 3.11 |
| 0x0F | 25 000 | 0.01 | 24 979.1 (EXP-17) | 2.24 |
| 0x10 | 12 500 | 0.01 | 12 490.0 (EXP-17) | 2.15 |
| 0x11 | 5 000 | 0.01 | 4 990.8 (EXP-18) | 4.95 |
| 0x12 | 2 500 | 0.01 | 2 494.9 (EXP-18) | 5.49 |
| 0x13 | 1 250 | 0.01 | 1 250.4 (EXP-18) | 0.87 |
| 0x14 | 500 | 0.01 | 500.2 (EXP-18) | 1.08 |

The earlier figures were all within 0.2 % of the ladder except 0x0D (1.1 %); their
residual errors fit a bench source whose own rate was only known to a few
parts per thousand (EXP-14), which a crystal-locked source removes.

The grid step limits the result to +-0.001 %. The 0x0A-0x0C values agree
with EXP-63 on unit #3; 0x0D is 125.0 kS/s, not the provisional 123.7 kS/s.
The INCOHERENT flags on 0x06, 0x07 and 0x09 do not reproduce: with a fast
enough read every record is coherent.

Failed runs, all explained and repeated:
- one run right after boot (first record still the persisted code's);
- 0x0E-0x10 in one run while the trigger mode had been switched to Single
  from the front panel (records stop advancing);
- a run with the CH1 offset DAC at `0xFFF` (position pushed to the top), which
  railed every record.

### 5.2 Codes 0x00-0x03: 125 MS/s on CH1, no interleave

With 12.5 / 16.67 / 25 MHz tones, codes 0x00-0x03 behave exactly like 0x04:
the CH1 record folds at 125.00 MS/s (pp 109-129 counts). The op-0x05 record
(read as CH2) carries no tone (pp 3-5 counts, CH2 open). Interleaving CH1 and
CH2 sample by sample does not give a consistent 250 MS/s fold either: the
interleaved record matches 250 MS/s for some tone/code pairs and not for
others (16.67 MHz always lands at 0.433 cycles/sample, which no single rate
explains together with the 12.5 and 25 MHz positions). So
with only reg 0x01 written, 0x00-0x03 are 125 MS/s single-ADC captures here;
whatever stock does to get 250 MS/s (2.4) is a different readout or a
different FPGA state that this build does not set up.

### 5.3 Side findings on the display path

- **Display freeze (fixed with this note):** at 0x14 (500 S/s) with a MHz
  tone on CH1, the frequency estimator's peak landed in bin 1. Bin 0 was left
  as a raw real part, `sqrtf()` of a negative value gave NaN, and the
  harmonic loop's bound was never reached: the display task spun forever from
  the first frame (found via the FreeRTOS TCBs over `mem read`; PC in
  `scope_freq.c` analyse()). Fix and regression test in `scope_freq.c` /
  `test_scope_freq.c`.
- **FFT at 125 MS/s:** 25 MHz and 50 MHz tones show as `pk 25.0MHz` /
  `pk 50.0MHz` with the span ending at 62.5 MHz. Harmonics above Nyquist fold
  back (the 3rd of 25 MHz appears at 125 - 75 = 50 MHz and is tagged "H2"),
  so harmonic tags are only meaningful below fs/2.

## 6. Blind spots

- One unit, CH1 only, one probe; CH2 and the dual-channel case (stock halves
  the rate at 0x00-0x03 with both channels on) were not measured.
- The source is a 3.3 V logic square through a header pin; amplitude and
  frontend bandwidth above a few MHz were not characterised, only frequency.
- A fold fit cannot tell a rate from its exact multiples that alias the same
  way for every tone; the tone set spans 0.11-2.62 x fs to make that unlikely,
  and every code also matched stock's table, which was not used as a prior
  beyond the +-30 % search window.
- PD2's role (set by stock on every timebase change) is still unknown; we do
  not set it and the rates did not need it.

## 7. Conclusion

- **Established:** reg 0x01 alone sets the sample rate; no USART timing
  commands are needed. Codes 0x04-0x14 run at stock's table rates
  (125 MS/s down to 500 S/s) on this unit, every one fold-tested against a
  crystal-locked source.
- **Excluded:** cal-bank offset and missing op-0x05 reads as causes of the old
  0x04 "frozen record"; the old INCOHERENT flags as properties of the codes.
- **NOT excluded (explicitly):** that 0x00-0x03 reach 250 MS/s with a stock
  readout (2048-sample, two-half record) or an FPGA setting we do not send.
- **Follow-up:** reproduce stock's 0x00-0x03 readout (2.4) and fold-test it;
  measure CH2 and both channels on.
