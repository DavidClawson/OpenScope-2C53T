# EXP-93 — Continuity beep latency: where the delay comes from

- **Date:** 2026-11-24
- **Unit:** my unit (new 2C53T, stock V1.2.0 as shipped, factory cal archived as `archive/factory_cal/unit4_mcu_settings_page_0x08006000.bin`)
- **Build:** `make guest-coldtrace-meter`, gcc 15.3.1, flashed through the factory IAP. Parts A–D on commit `7480998` (main); Part E on the edge-counter change in the commit that adds this file
- **Status:** CONFIRMED (fix works by ear; no audio number for the fixed build yet). H1 REFUTED; stock is 20x faster because it beeps from the meter SoC's buzzer tone on PC3 (EXTI3, already documented in #94), and our buzzer now counts the same edges. Part D's "active-low level" reading is WITHDRAWN (Part E). The Part D/E veto is SUPERSEDED (addendum 2026-11-27)
- **Number:** EXP-74 until 2026-11-27, then renumbered EXP-93, because #75 had claimed 74 first. Commits before the renumbering say EXP-74

## 1. Problem
Continuity mode beeps on shorted probes since the 2026-10-06 threshold change, but the
beep is noticeably slow to start. Is the delay set by something our firmware controls
(the 250 ms keepalive, the decoder, task scheduling), or by the meter chip's own
measurement cadence and settling?

## 2. Hypothesis
**H1 (chip settling):** after the probes touch, the meter chip sends one or more frames
that are not yet a sub-threshold Ohm reading (still `OL`, or a transitional value)
before the frame that shows the short. Onset delay is then about (n + 0.5) × the frame
interval.
- If true, the capture shows ≥ 1 non-short frame between the last `OL` and the first
  short frame on most touches, and the per-touch delay is ≥ 2 frame intervals.
- If false, the first frame after `OL` is already the short on every touch, and the
  delay is at most one frame interval. Then no decoder change can help, and only the
  chip's cadence bounds it.

**H2 (keepalive-bound cadence):** the data-frame cadence is set by our 250 ms
keepalive (CLAUDE.md: frames stop entirely when we stop transmitting).
- If true, frame intervals cluster at 250 ms or show the 250 ms period.
- If false, they sit at a different, regular value.

The buzzer side is excluded by code reading, not by this test: `buzzer_task`
(`continuity_buzzer.c`) re-reads the latest snapshot every 5 ms at priority 2, above
the display task (priority 1, ~87 ms per redraw per `ui dump`), and
`meter_continuity_is_short()` has no multi-frame confirmation.

## 3. Procedure
Host tool `reverse_engineering/captures/exp93/stream_loop.py` runs the shell's `meter stream 200 20`
back to back and stamps every line with one host monotonic clock. The device prints a
line only when `update_count` changes, polling every 20 ms, so arrival resolution is
~20 ms plus CDC latency.

1. Probes open, 4 s stream (H2, and the open-state control).
2. Transition capture: 120 s loop. The operator touches the probes together and holds
   ~2 s, then separates ~2 s, repeated.
3. Per touch: frames from the last `OL` to the first short frame, and the time between
   them.

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| running build | today's `guest-coldtrace-meter` | `version`: `Build: Nov 24 2026 21:43:46, gcc 15.3.1` |
| UI mode | meter, continuity | `ui dump`: `mode=1 meter_submode=7 (Continuity)` |
| decoder submode | 7 | `meter stream`: `sub=7` |

## 4. Control
Both known states must come through the same stream path in the same session: `OL`
with the probes open, and a sub-threshold Ohm reading with the probes held together.

| control | expected | measured | passed? |
|---|---|---|---|
| probes open, step 1 | `disp=OL`, `cls=5` (overload), frames updating | 25 frames, every one `OL cls=5`, `upd` 1031→1055 in 3.9 s | yes |
| probes held shorted | sub-30 Ω `Ohm` reading, `cls` normal | 69 frames across 6 holds, `cls=1 unit=Ohm`, 0.21–0.29 Ω | yes |

Both controls passed in the same capture, so the result below is a real measurement
and not void.

## 5. Results
**Step 1 (H2), probes open:** 25 consecutive frames, interval ~162 ms (6.2 frames/s),
`update_count` advancing by exactly 1 each time. An 8 s dry run gave median 163 ms,
max 203 ms. The cadence is regular and does not sit at 250 ms.

**Step 2, transition capture** (`reverse_engineering/captures/exp93/run1_touches.txt`, 21:59:36–22:02:07, 150 s): 891
unique frames, `upd` 1577→2467 with **0 missing**. Interval median 163 ms, p5 138,
p95 204, max 492 (the max is a stream-restart gap on the host side).

Only two frame types appeared: `OL` (`cls=5`, 822 unique frames) and the short
(`cls=1 Ohm`, 69 unique frames, 0.21–0.29 Ω, `f6` rotating through
`07/0A/0D/0E/0F` as documented for low-Ω). No transitional values, no other unit.
The chip's own continuity flag (`beep=`) was 0 on every one of the 910 lines.

| touch | last `OL` upd | first short upd | frames | Δt last `OL` → short | held (frames) |
|---|---|---|---|---|---|
| 1 | 1990 | 1991 | 1 | 183 ms | 12 |
| 2 | 2017 | 2018 | 1 | 204 ms | 14 |
| 3 | 2044 | 2045 | 1 | 161 ms | 12 |
| 4 | 2070 | 2071 | 1 | 124 ms | 12 |
| 5 | 2102 | 2103 | 1 | 207 ms | 10 |
| 6 | 2128 | 2129 | 1 | 173 ms | 9 |

**H1 is refuted:** on 6 of 6 touches the frame right after the last `OL` is already the
short. **H2 is not supported:** the cadence is a regular ~163 ms, which is the chip's
own rhythm, not our 250 ms keepalive. That is passive evidence only; the keepalive
rate was not varied.

> **WITHDRAWN 2026-11-24, same session:** the paragraph below was a reading of our
> notes, and Part B's disassembly shows the notes were wrong. Kept as written.

Side observation: stock's EXTI3 handler (`0x08009C10`) has three conflicting labels
across the RE notes (continuity buzzer, button-matrix row change, DAC AWG). Even the
continuity-buzzer reading has it acting on `ms[0xF5D]`, which stock's meter decoder
sets from the decoded frame. So by our own notes stock also beeps per frame. That
comes from reading the notes, not from a measurement.

### Part B: touch-to-beep by audio, stock vs OpenScope (same unit, same probes)

Recorded on an iPhone (Voice Memos) about 10 cm from the scope: first OpenScope `main`
`7480998`, then stock V1.2.0 (`a17c5c35…`, flashed through the factory IAP), with the
same tap pattern. The two recordings (13 s and 17 s) are not in the repo, because
they are ambient room audio. The analysis scripts `latency.py` / `events.py` are in
`reverse_engineering/captures/exp93/`, and the per-tap numbers below are their
output. Method: the beep tone gets its own
band envelope (OpenScope 2.8–3.7 kHz, stock 1.7–2.1 kHz; the two buzzers sound
different), and the tip click is the strongest 10–16 kHz transient before each beep
onset. Every beep had exactly one click before it (`events.py` lists them all), so
the pairing is unambiguous.

| firmware | taps | click → beep onset |
|---|---|---|
| **stock V1.2.0** | 7 | **24, 24, 31, 32, 33, 38, 39 ms** (median 33) |
| **OpenScope main** | 6 | **475, 479, 612, 739, 853, 1012 ms** (median ~676) |

Stock is about 20× faster. Operator observation, weak evidence: on stock the beep
starts *before* the display shows the reading.

So the real OpenScope delay is 3–6 frame intervals, not the ≤ 1 frame the frame
capture suggested. The chip reports a touch several frames after it happens, so the
"last `OL`" frame in Part A was already after the touch. This is blind spot 1 below,
now measured.

### Part C: how stock does it (static, `APP_2C53T_V1.2.0_251015.bin`)

- **Vector 25 (IRQ 9, EXTI3)** → `0x08009C11`. All other EXTI vectors point at the
  default handler.
- **Pin mapping and edge**, master_init at `0x0802C75A`–`0x0802C7D0`, with
  `r2 = 0x40010008` (AFIO EXTICR1):
  - `EXTICR1 = (EXTICR1 & ~0xF000) | 0x2000` → EXTI line 3 ← **port C, so PC3**
  - `[r2,#0x3F8]` = `0x40010400` IMR: clear, then set bit 3
  - `[r2,#0x3FC]` = `0x40010404` EMR: clear bit 3
  - `[r2,#0x400]` = `0x40010408` RTSR: set bit 3
  - `0x4001040C` FTSR (reached as `0x40010800 − 0x3F4`): clear bit 3

  That is **rising edge only**. The `gpio_init` just before it configures pins
  `0x0008` (pin 3) as an input.
- **The ISR** (`0x08009C10`): it clears EXTI pending bit 3 and returns unless
  `ms[0xF68] == 1`. It then writes **`10` to `0x40015434`** (TMR11 `C1DT`, the buzzer
  PWM duty, so the buzzer is on) only when `ms[0xF2D] == 0`, or when
  `ms[0xF2D] == 2 && ms[0xF36] == 2`. The `clz` / `lsrs #5` at `0x08009C52` gives 1
  only when both bytes are 2, and the `beq` skips the duty write when it gives 0. It
  writes `0` to `0x20002D67` in every case that gets past the `ms[0xF68]` check.
  *Corrected 2026-11-25 (Metalaria, PR #102): the first version of this bullet said
  the duty is written "unless `ms[0xF2D] == 2 && ms[0xF36] == 2`", which reads the
  gate the wrong way round. Metalaria confirmed it by running the handler in Unicorn
  and read the bytes on a running stock unit: `ms[0xF2D]` / `ms[0xF36]` is 0 / 0 in
  Automatic and 2 / 2 in Continuity, the only two functions that pass. Diode is
  2 / 1, so stock does not pass the tone there.*
- **The countdown** (Metalaria, PR #102; code re-checked here): `0x08029D0C` and
  `0x08040508` run the same code. If `C1DT != 0`, increment `0x20002D67` and at 11
  clear both `C1DT` and the counter; otherwise clear the counter. It is the key
  click's timer, stepping every 10 ms on a running stock unit. Every PC3 edge restarts
  it, so stock's beep lasts 11 ticks past the last edge: 100.0 and 101.0 ms from the
  first step to 0 in two releases, which is 100–110 ms after the last edge depending
  on where it falls in the tick. Part E stopped 25 ms after the last edge; it now
  stops after 105 ms to match.

So stock's beep is driven by **rising edges on PC3**, which something outside the
MCU produces. The most likely source is the meter chip's own buzzer output. It does
not wait for the decoded 6 Hz frame.

Two old notes are wrong as a result. "The EXTI3 handler reads `ms+0xF5D`" does not
match the handler. That claim comes from `FPGA_TASK_ANALYSIS.md` §8 and was copied into
`ARCHITECTURE.md:528`, `HARDWARE_PINOUT.md:605`, `FPGA_PROTOCOL_COMPLETE.md:992` and
`CALIBRATION.md:557`. (`ANALYSIS_SUMMARY.md` only labels it "Continuity buzzer", which
is right; an earlier draft of this paragraph blamed it by mistake.) And CLAUDE.md's
"IOMUX remap: `(reg & ~0xF000) | 0x2000` at AFIO+0x08 — disables JTAG-DP" is this
EXTICR1 write. It maps EXTI3 to PC3 and has nothing to do with JTAG. `fpga.c:5327`
already doubted that note. Corrected 2026-11-24 in `FPGA_TASK_ANALYSIS.md` §8,
`CLAUDE.md` and `AGENTS.md`, then in the four copies above and the two `fpga.c`
comments that cited the remap note; `HARDWARE_PINOUT.md` now also names PC3.

### Part D: PC3 on OpenScope (same build, continuity mode)

Instrument: `reverse_engineering/captures/exp93/pc3_sample.py`, which sends the
shell's `mem read 40011008 1` (GPIOC IDR) back to back at ~36–40 samples/s.
Read-only.

| run | PC3 | PC13 (UP button, control) |
|---|---|---|
| probes open, 3 s (`pc3_baseline_open.txt`) | HIGH 119/119, 0 changes | HIGH 119/119 |
| 4 holds + UP press, 40 s (`pc3_touches.txt`) | **LOW exactly during the 4 holds**: 9.01–11.72, 14.74–17.66, 20.48–23.56, 26.55–29.42 s (94–113 samples each, all low); HIGH otherwise | one LOW run 34.92–35.19 s (the UP press) |

> **WITHDRAWN 2026-11-24, same session:** the reading below ("an active-low
> short-detect level"), the discrepancy paragraph after it and the polling design
> built on it. PC3 is the meter SoC's buzzer tone, and this table is what that tone
> looks like on a floating input sampled at ~38 Hz. See Part E. Kept as written.

So **PC3 is an active-low short-detect level**, and it works under OpenScope's
meter-mode selection. The control shows the instrument sees GPIOC input changes.

This leaves one discrepancy with Part C. Stock arms EXTI3 on PC3 *rising* edges,
which this level only produces on release. So stock probably starts the beep
elsewhere, for example by polling in its 500 Hz TMR3 scan, and uses the edge for
the stop. That is not traced. At ~38 samples/s this instrument also cannot rule out
narrow high pulses riding on the low level.

**Firmware change (superseded by Part E):** `continuity_buzzer.c` now samples PC3 in
`buzzer_task` (every 1 ms while beeping, 5 ms idle):
- **Start:** PC3 low for 4 ms.
- **Stop:** PC3 high for 10 ms.
- **Veto:** a fresh resistance reading at or above the threshold mutes the beep
  until release, so the threshold softkey still means something below the hardware
  threshold. `OL` frames never veto. *SUPERSEDED 2026-11-27: a stale frame could
  mute a real short, and the mute never cleared while the tone went on. See the
  addendum.*
- **Fallback:** until PC3 has gone low once in the session, the old decoded-reading
  path runs.
- **Gate:** meter mode, continuity submode, and no function change in progress.

Image `openscope-2c53t-main-7480998-pc3beep-coldtrace-meter.bin`. The acceptance
test is a re-run of Part B.

**Acceptance, by ear only (weak evidence):**
- **Shorted probes:** the operator reports the beep now "performing like the
  original firmware". No audio recording was made of this build, so there is no
  number yet.
- **47 Ω, threshold 30 Ω:** it beeps for about a second, then stops, and the
  display shows 47 Ω. That is the designed behaviour: PC3 fires at once, and the
  veto mutes it when the delayed reading arrives. It also means **the hardware
  threshold is above 47 Ω**.
- A 25 s frame capture meant to cover the 47 Ω hold
  (`run2_47ohm.txt`) saw only `OL` (169 frames). The resistor went on after the
  capture ended, so that file records nothing about 47 Ω.

### Part E: PC3 is the buzzer tone, counted as edges (same day)

**Correction, credit to #94.** PR #94 (`docs/re/system_map_corrections.md` §6, by
deOliveira-R) had already measured PC3 on unit #2 before this experiment began. The
meter SoC's BUZ output (pin 28) drives an EL3H7 optocoupler through 1 kΩ, and the
optocoupler's output is PC3. It gave 4480 edges/s with the leads shorted, a 2.24 kHz
tone, and none open. This experiment missed it because its search of the RE notes did
not cover `docs/re/`.

That explains Part D. Our init left PC3 floating (`GPIO_PULL_NONE`). The optocoupler
can only pull the pin low, so with nothing to pull it back up between pulses it
most likely stayed low for the whole hold, and a ~38 Hz level poll read solid LOW.
The mechanism is inferred, not measured (blind spots). Stock configures PC3 as an input with
**pull-up** (gpio_init struct before `0x0802C74A`: pins `0x0008`, pull `0x18`, mode
input) and counts **rising edges** on EXTI3. With a tone there is a rising edge on
every cycle, which also settles Part D's "rising edges only come on release"
discrepancy.

**Firmware change:** `continuity_buzzer.c` now does what stock does.
- **Pin:** PC3 is an input with pull-up. EXTI line 3 is mapped to port C, rising
  edge, NVIC priority 6. `EXINT3_IRQHandler` only counts edges (`pc3_edges`).
- **Start:** at least 2 new edges in one buzzer-task sample (1 ms while beeping,
  5 ms idle; at ~2.2 kHz a 5 ms sample holds ~11 edges).
- **Stop:** no edge for 25 ms. *Changed 2026-11-25 to 105 ms, the middle of stock's
  100–110 ms (Part C, countdown), after Metalaria's review on PR #102. Everything else
  in this part was measured on the 25 ms build; the change touches only when the beep
  ends. Image `openscope-2c53t-pc3edge-hold105-coldtrace-meter.bin`, sha256
  `efb9c3a1198c63f8…`; by ear on my unit the operator reports "all working fine" (weak
  evidence; the tail itself was not timed).*
- **Unchanged from Part D:** the threshold veto (*superseded 2026-11-27, see the
  addendum*), the fallback to the decoded reading until a tone has been seen in the
  session, and the gate.

Image `openscope-2c53t-pc3edge-coldtrace-meter.bin`, sha256 `16a8fa23f0915310…`.
Vector 25 → `EXINT3_IRQHandler` at `0x08015A41`, `pc3_edges` at `0x2000D544`
(from the ELF).

**Preconditions verified by readback** (`reverse_engineering/captures/exp93/pc3_readback.py`,
probes open, after flashing):
| what | expected | measured |
|---|---|---|
| GPIOC CRL, PC3 nibble | `8` (input with pull) | `0x41848338` → `8` |
| GPIOC ODR bit 3 | 1 (pull-up) | `0x00002B6D` → 1 |
| GPIOC IDR bit 3, open | 1 | `0x0000ABFC` → 1 |
| AFIO EXTICR1 [15:12] | 2 (port C) | `0x00002002` → 2 |
| EXTI IMR / RTSR / FTSR bit 3 | 1 / 1 / 0 | `0x09` / `0x08` / `0x01` → 1 / 1 / 0 |
| NVIC ISER0 bit 9 (EXTI3) | 1 | `0x20100240` → 1 |
| NVIC priority, IRQ 9 | 6 | IPR8 `0x00006000` → `0x60` → 6 |

**Instrument:** `reverse_engineering/captures/exp93/pc3_edge_rate.py` reads
`pc3_edges` over the shell and logs edges/s between reads (303 reads in 60 s).
Read-only.

**Control:** with the probes open the counter must not move. A 5 s check gave 26
reads, all at 3284, so 0 new edges. (The count was already 3284 from touches after
flashing.) In the 60 s run below, 234 of 303 intervals had 0 edges, all of them in the
open stretches before, between and after the holds.

**Results** (`pc3edge_touches.txt`, 60 s): five tone bursts, nothing else.

| burst | time (s) | edges | steady rate (interior reads) |
|---|---|---|---|
| 1 | 14.2–15.8 | 3382 | 2287/s |
| 2 | 17.2–19.8 | 5808 | 2289/s |
| 3 | 21.0–23.2 | 4302 | 2291/s |
| 4 | 24.4–26.2 | 3674 | 2288/s |
| 5 | 28.1–33.6 | 11911 | 2288/s |

So PC3 carries a **2.29 kHz tone** (rising edges/s) on my unit while the probes are
held, and nothing while they are open. That is #94's 2.24 kHz on unit #2 within 2.2%.
Bursts 1–4 are shorts and burst 5 is the 47 Ω, as the operator confirmed afterwards.
The log alone cannot tell them apart. So the SoC sounds the same tone for 47 Ω as for
a short, which agrees with Part D's "hardware threshold above 47 Ω".

**Acceptance, by ear only (weak evidence):**
- **Shorts:** the operator reports they "beep instantly and stop on release".
- **47 Ω, above the beep threshold setting:** it chirps for about a second, then goes
  quiet once the reading arrives, as on the Part D build. So the threshold mute still
  works on the edge path.

## 6. Blind spots
- **No independent touch timestamp** *(measured afterwards in Part B: the real delay
  is 475–1012 ms, so this blind spot hid most of the effect)*. The capture only measures last `OL` → first
  short. It cannot see when the tips actually met, so it cannot separate "the chip
  reports in the next frame" from "the chip's conversion window already contained
  the touch". The true touch-to-beep delay could be anywhere from ~0 to ~2 frame
  intervals. An audio recording of the tap click and the beep would measure it.
- **20 ms device polling + CDC latency** on arrival times. That's fine for 160 ms
  frames, but it means the Δt column carries about ±20 ms.
- **H2 was not manipulated.** A chip that runs at 6 Hz only while it sees traffic at
  ≥ 4 Hz would look identical here. Varying the keepalive needs a firmware knob.
- **One unit, one probe set, 6 touches**, all with firm metal-to-metal contact.
  Light or scratchy contact may behave differently.
- **Audio method (Part B):** the click is the tip-contact sound, and it may lead or lag
  true electrical contact by a few ms. The phone's recording path adds a fixed delay
  to both sounds, so it cancels out. Stock's 24–39 ms is therefore an upper bound on
  what its electrical path needs.
- **Part C is static only.** Nothing here shows PC3 actually toggling, what drives
  it, or whether it does so under OpenScope's meter-mode selection. That needs a
  measurement on our firmware, with a positive control on the same GPIOC input
  register. *(Measured afterwards in Parts D and E: it toggles, as #94 had shown.)*
- **Part E counts edges, it does not see the pin.** A level poll and an edge count
  both summarise the waveform; neither shows its duty or idle level, and no logic
  analyser was on PC3. Why the floating input read solid LOW in Part D is inferred.
- **Part E cannot tell a short from 47 Ω**: both give the same rate. Which burst was
  the resistor comes from the operator's sequence, and the SoC's own threshold is
  still not measured beyond "above 47 Ω".
- **Part E logs rate, not onset.** Reads are ~200 ms apart, so the log says nothing
  about touch-to-beep delay. The firmware path adds at most one 5 ms sample after the
  second edge, but no audio re-run of Part B was made for this build.
- **Chip mode not proven.** The chip sends resistance-style text in this submode and
  never sets its continuity flag. If it has a true continuity function with a faster
  update, this build may not be selecting it, and this test cannot tell.

## 7. Conclusion
- **Established:** in continuity mode the meter chip reports at a regular ~163 ms
  (6.1 frames/s) on my unit. The short appears in the first frame after `OL`, with no
  settling frames. Our side adds at most ~5 ms (buzzer poll). So once the chip
  reports, the beep onset is bounded by the frame cadence: about 80 ms on average and
  ~200 ms worst case.
- **Excluded:** decoder-side gains. No transitional frame exists that an earlier rule
  could catch. Display scheduling is also out, since the buzzer task outranks the
  display task.
- **Established (Part B):** the real touch-to-beep delay is ~676 ms median
  (475–1012) on OpenScope against 33 ms (24–39) on stock, on the same unit and probes.
- **Established (Part C, static, with Metalaria's live read on PR #102):** stock
  beeps from rising edges on **PC3** via EXTI3 by setting the TMR11 buzzer duty
  directly, only in the Automatic and Continuity functions, and stops 100–110 ms after
  the last edge. It does not use the decoded frame. OpenScope beeps in continuity only
  (it has no Automatic function), stays silent in Diode as stock does, and now stops
  105 ms after the last edge (25 ms in the build Part E measured).
- **Established (Part E):** PC3 carries the meter SoC's buzzer tone, as #94 found on
  unit #2: 2287–2291 rising edges/s in each of five holds on my unit under OpenScope
  with stock's pull-up, and 0 with the probes open. The buzzer now counts those edges
  on EXTI3, as stock does, and by ear beeps at once and stops on release.
- **Withdrawn (Part D):** "PC3 is an active-low short-detect level". That was the
  tone seen through a floating input.
- **NOT excluded (explicitly):** the SoC's own continuity threshold (only "above
  47 Ω" is known), and a meter cadence that depends on the keepalive rate (moot for
  the beep now).
- **Follow-up:** (1) ~~sample PC3 on OpenScope~~ done (Parts D, E). (2) ~~drive the
  buzzer from PC3 edges~~ done (Part E). (3) Re-run the Part B audio test on this build
  for a number. (4) Measure the SoC's threshold with a resistor ladder. (5) ~~Correct
  the four other copies of the old EXTI3 note listed in Part C~~ done.

## Addendum 2026-11-27: review of #102, the veto

**The problem** (DavidClawson on #102). The Part D/E veto latched on the first new
frame after the tone's onset that read at or above the threshold, and held until the
tone stopped. But a frame shows the probes as they were up to a second before it
arrived (Part B: 475–1012 ms). Sliding the probes off a 1 kΩ part onto a short could
mute the whole short on a frame that still showed 1 kΩ. And since the veto never
cleared while the tone went on, a 47 Ω part slid onto a short stayed quiet.

**The fix** (`firmware/src/drivers/continuity_veto.h`, used by
`continuity_buzzer.c`). A frame carries no measurement time, only its arrival, so:
- frames that arrive within **1200 ms** of the tone's onset are ignored (marked as
  seen, never counted). That is the longest lag measured, 1012 ms, plus about one
  frame interval (p95 204 ms);
- after that, each new frame decides: at or above the threshold mutes, anything else
  (below it, `OL`, no resistance) unmutes. The veto follows the latest frame and no
  longer latches. *Revised 2026-11-27 after the first bench run below (`4390251`):
  `OL` or a blank frame now changes nothing, and only a resistance below the
  threshold unmutes. Release reaches the frames in tens of ms, so an `OL` frame
  could land inside the tone's 105 ms hold and unmute a muted part on release.*

**Cost.** A part between the user's threshold and the SoC's beeps for up to 1.2 s
plus one frame (about 1.4 s) before it goes quiet. The Part D/E build went quiet at
the first frame that showed it, 0.5 to 1.2 s. A part that reads right at the
threshold beeps or not frame by frame, as the screen's SHORT does.

**Unchanged, now stated.** Once the tone has been seen in a session, PC3 is the only
source that starts a beep. A part between the SoC's threshold (above 47 Ω, not
measured further) and a higher user setting (50 or 100 Ω) does not beep, though the
screen says SHORT.

**Host test** (`make test-continuity-veto`, `firmware/tests/test_continuity_veto.c`).
A millisecond simulation of the probes, the SoC's tone (below an assumed 60 Ω, with
the 105 ms hold) and one frame per 163 ms that shows the probes `lag` ms before it
arrived, except that an open circuit shows as `OL` within `rel` ms (the release seen
on the bench). Seven scenarios, each at five lags (300, 475, 676, 850, 1012 ms), two
release lags (30 ms, or as slow as contact) and four frame phases. The gate as first
posted, two half-fixes and the first review fix are negative controls (as of
`4390251`):

| gate | runs as expected | the runs it gets wrong |
|---|---|---|
| `cont_veto_step` | 280/280 | none |
| control: as first posted (`9d1d135`) | 165/280 | 1 kΩ slid onto a short (40/40); 1 kΩ lifted, then a short tapped (35/40); 47 Ω slid onto a short (40/40) |
| control: no ignore window | 205/280 | the two 1 kΩ scenarios (40/40, 35/40) |
| control: window, but latching | 240/280 | 47 Ω slid onto a short (40/40) |
| control: `OL` unmutes (`6bf6e95`) | 260/280 | the release of a muted 47 Ω part, from open and after a short (10/40 each: the 30 ms release lag, at the two phases that put a frame inside the hold) |

So each part of the fix is needed. The runs of "1 kΩ lifted, then a short tapped"
that the controls get right are at lag 300 ms, where some phases put no frame into
the 100 ms that still shows the 1 kΩ part. The first version of the test (`6bf6e95`,
one lag model, three controls) gave 140/140 against 82, 102 and 120 of 140.

**Blind spots.**
- **Not on hardware yet.** The firmware builds (`make`), the host test passes; no unit
  has run this change. The acceptance test is by ear: short, 47 Ω, 1 kΩ slid onto a
  short, 47 Ω slid onto a short. *Run the same day, see the bench below, except the
  1 kΩ case: no resistor above the threshold was at hand.*
- **The 1200 ms rests on six taps** (Part B). A lag longer than 1200 ms would let a
  stale frame mute a short for the difference.
- **The lag was measured on shorts only.** Whether a 47 Ω or 1 kΩ reading lags the
  same way is not measured. *The bench below gives the first 47 Ω frame 483 to 965 ms
  after the tone started (five contacts), inside Part B's range; 1 kΩ still not.*
- **The simulation is regular:** frames arrive exactly every 163 ms and show the
  probes' state, with no readings in between (Part A saw none, on shorts). The SoC's
  threshold is an assumption that only decides which parts make a tone. *On the bench,
  sliding between 47 Ω and a short gave readings in between (23.7 and 24 Ω); below the
  threshold, they unmute, as a near short should.*

### Bench, my unit, 2026-11-27

**Instrument.** The fork's read-only shell command `buzzer status` (s00500
`feat/remote-control` `f9f72d5`, not part of #102) prints one line with the device
tick, whether the buzzer is on, its toggle counter (+1 per ms while it sounds), the
PC3 edge count, the tone state, the veto, the tone's age and the latest frame.
`reverse_engineering/captures/exp93/buzzer_log.py` polls it about 10 times a second;
`buzzer_episodes.py` splits a capture into tones. Beep time comes from the toggle
counter, so it is exact to the ms even where the poll is ~110 ms apart; mute and
unmute times are poll-limited. Image: `feat/remote-control` with this branch merged,
built as `guest-coldtrace-meter` (`guest-coldtrace` cannot reach the meter's
submodes; in it the continuity frames are rejected, `sub=0`). Continuity, threshold
30 Ω (the default), probes and one 47 Ω resistor, by hand.

**Positive control, both runs:** every short beeped from the tone's start to its
end (11 shorts, 0.19 to 2.0 s, never muted), and every 47 Ω contact went quiet, so
the instrument sees both states.

**Run 1, the `6bf6e95` rule** (`r04m`, build `Nov 27 2026 14:32:33`;
`bench_veto_6bf6e95.txt`):

| tone | what | beep | veto |
|---|---|---|---|
| #1 to #9 | shorts | the whole tone | never muted |
| #10 | 47 Ω, 3.8 s | 1267 ms | muted at the first frame after the window, held to release |
| #11 | 47 Ω, 2.4 s | 1260 ms, then **76 ms at release** | muted; an `OL` frame unmuted it at release |

In #11 the PC3 edges stopped at device tick ~144,896 and an `OL` frame arrived by
144,989, inside the tone's 105 ms hold. Under that rule `OL` unmutes, so the buzzer
sounded for the rest of the hold. In #10 the next frame came after the hold. Release
reaches the frames in tens of ms, where contact takes 0.5 to 1 s; the first host
test modelled one lag for both and could not see this. Fixed in `4390251`.

**Run 2, the `4390251` rule** (`r05m`, build `Nov 27 2026 14:44:44`;
`bench_veto_4390251.txt`):

| tone | what | beep | veto |
|---|---|---|---|
| #1, #5 | shorts, 0.84 s and 0.19 s | the whole tone | never muted |
| #2 | 47 Ω, slid onto a short and back twice, 12.7 s, the tone never stopping | 5675 ms | muted 1.39 s; **unmuted** when the first short frame came (0.25 Ω); muted when the frames showed 47.7 Ω again; unmuted (23.7 Ω, then 0.29 Ω); muted (47.7 Ω) |
| #3 | 47 Ω, slid onto a short and back, 6.8 s | 3160 ms | muted 1.35 s; unmuted (0.33 Ω); stayed on through 24 Ω; muted (47.7 Ω) |
| #4 | 47 Ω, 5.0 s | 1200 ms | muted, held to release |
| #6 | 47 Ω, 3.6 s | 1435 ms | muted, held to release |

- **The veto follows the frames:** three slides onto a short each unmuted at the
  first frame below the threshold, and three slides back each muted at the first
  frame at or above it, with the tone running throughout.
- **No release chirp:** in #2 and #6 an `OL` frame arrived inside the hold (#6: about
  30 ms after the last edge), the veto stayed on and the toggle counter did not move
  (6528 and 12596). The other two releases had no frame in the hold.
- **Cost, measured:** a 47 Ω part beeps for 1.20 to 1.44 s (four clean holds, both
  runs) before it goes quiet; "about 1.4 s" was the estimate.
- **Operator, by ear:** "feels good" (run 1).

**Not tested on hardware:** the review's own case, a part above the threshold slid
onto a short (stale frames after the onset). Only one resistor, 47 Ω, was at hand, so
that rests on the host test.
