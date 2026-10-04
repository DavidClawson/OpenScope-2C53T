# EXP-60 — coupling buttons were label-only; SAVE flipped CH1's input path

- **Date:** 2026-10-03
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace`, v4 (Build Oct 3 2026 18:29:21), branch `bench/2026-09-14`
- **Status:** **CONFIRMED.** Both defects found by code reading, fixed, and the fix verified
  on the wire: `scripts/coupling_bench.py` passed 14/14.

## 1. Problem
Two user-facing scope controls in the release image (`guest-coldtrace`, v0.4.0) were
checked against what they drive:

- **SAVE in scope mode** toggled PC12, under `FPGA_WARM_HANDOFF_TEST`. That flag is a
  bench leftover from the warm-handoff runs, and the release image also defines it. PC12
  is CH1's input **path** select: direct vs ~30× attenuated, set per range by the range
  table (bench 2026-08-15). So one press put CH1 on the wrong path for its range, and
  volts/div stayed wrong until the next range change. In other builds SAVE printed
  `SAVED #n` and wrote nothing (`screenshot.c` has no caller).
- **CH1/CH2 coupling** (the buttons, plus the Settings menu's OK and LEFT/RIGHT) cycled a
  DC/AC/GND **label**. PD12/PD13, the AC/DC relays (HIGH = DC), were driven HIGH once in
  init and never again. The only other path was a USART command sent unobeyed, from a
  re-init coldtrace does not run. The popup and info bar said "AC" over a DC-coupled
  input: an inert control that looked correct, the EXP-17 timebase-button shape.

## 2. Hypothesis
With coupling routed through one writer that drives PD12/PD13 and verifies by readback,
a +1 V DC step moves a DC-coupled channel's record mean by ~47 counts at range 5 and
moves an AC-coupled one by ~0. Each relay acts on its own channel only.

**Falsifiers:**
- the label and the pin disagree;
- AC still passes the step;
- the other channel's response changes when one is switched.

## 3. Procedure
`scripts/coupling_bench.py`, criteria fixed in its docstring before run 1. Coupling was
changed only by `btn ch1` / `btn ch2`, a new shell command that posts into the key-scan
queue. Readback came from the new read-only `fpga scope coupling`, which prints each
label beside the PD12/PD13 pin state and GPIO mode.

- **Source:** JDS6600, both channels, sine 201.2 Hz, 0.5 Vpp.
- **Scope:** range 5 on both channels, timebase 0x10, AUTO.
- **Measurement:** offset 0 → +1.0 V; the change in the median whole-record mean over 4
  committed records.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| image | v4 Build line | `Oct 3 2026 18:29:21` |
| PD12/PD13 after boot | GPIO push-pull, HIGH, label DC | `mode 0x1 odt 1 -> relay DC AGREE`, both |

## 4. Control
DC on the same channel is the control for AC. The untouched channel is the control for
isolation. Both are in the table below.

## 5. Results

**Run 1: 13/14.** The first measurement had CH2 in DC with Δ +0.1. Every later CH2 DC
measurement gave −50. Isolation runs (`captures/exp60/jds_first_step_*.log`):

| sequence | CH1 Δ | CH2 Δ |
|---|---|---|
| right after JDS output enable | +49.7 | **−0.1** |
| after a CH2 range change only | +49.7 | −50.5 |
| right after JDS output re-enable only | +49.8 | **−0.0** |
| again | +49.9 | −50.3 |

The scope's range change does not matter; the generator's output enable does. **After
the JDS6600's outputs are enabled, the first CH2 offset change does not reach the
output**, although register 28 reads back correct. That is an instrument quirk, recorded
in the `JDS6600` docstring in `scripts/bench.py`. The script now primes one offset step
after enabling outputs.

**Run 2: 14/14.**

| criterion | CH1 | CH2 |
|---|---|---|
| readback DC / AC / DC again | agree ×3 | agree ×3 |
| DC Δ | +49.8 | −50.2 |
| AC Δ | **+0.0** | **−0.2** |
| restored DC Δ | +49.7 | −50.3 |
| isolation (other channel DC while this one is AC) | CH2 −50.1 | CH1 +49.7 |
| labels seen | DC, AC | DC, AC (no GND) |

**SAVE** (v4, scope mode, 3 × `btn save`): PC12 read `0, out PP` before and after every
press. CH1 Vpp held at 2.008–2.028 V, a one-count jitter.

## 6. Blind spots
- ~~**CH2 moves the opposite way to CH1.**~~ **Resolved, see §8:** the scope does not
  invert; the JDS CH2 offset misbehaves.
- **AC's low-frequency corner** (~9 Hz from the 2026-08-15 PD12-LOW observation) was not
  re-measured; only DC blocking was.
- **GND** was removed from the cycle, not implemented. Stock may have a ground path this
  project has not found.
- **20 MHz bandwidth limit** (Settings) is fed only by the same unobeyed USART command.
  It is very likely inert too, and was **not** fixed here.
- **The SAVE popup text** was not captured: it is transient, and the dump needs a held
  screen.
- **Physical buttons** were not pressed. `btn` enters below the key scan.

## 7. Conclusion
- **Established:**
  - coupling now drives the relays on both channels, verified by readback and by the
    signal;
  - AC blocks a 1 V step to ≤ 0.2 counts, DC passes it at ~50;
  - each relay is isolated to its own channel;
  - SAVE no longer touches PC12.
- **Established about v0.4.0:**
  - its coupling control was label-only;
  - its SAVE changed CH1's gain path;
  - both are in the README's Sharp edges.
- **Excluded (§8):** CH2 sign inversion in the scope.
- **Fixed (§9):** the probe setting now scales every volts readout; the BW-limit row says
  `n/a`.
- **Follow-up:**
  - audit the BW limit and the remaining Settings → Oscilloscope items the same way;
  - add `coupling_bench.py` next to `mask_bench.py` in the regression set.

## 8. Addendum — the CH2 sign is the generator's, not the scope's

**Question.** Scope CH2 read about −50 counts for a +1 V command, against +50 on CH1. Is
the scope's CH2 front end inverting, or the JDS6600's CH2 output?

**Method.** Cables swapped at the generator (JDS CH1 → scope CH2, JDS CH2 → scope CH1).
Each JDS channel stepped 0 → +1 V **on its own**, twice, with the others at 0 V; both
scope channels DC, range 5. Predictions stated before the run:
- **scope inverts:** JDS CH1 step → scope CH2 −50.
- **generator inverts:** JDS CH1 step → scope CH2 +50.

| step | scope CH1 Δ | scope CH2 Δ |
|---|---|---|
| JDS CH1 +1 V, rep 0 | **−50.0** | +50.1 |
| JDS CH1 +1 V, rep 1 | +0.2 | **+50.1** |
| JDS CH2 +1 V, rep 0 | −0.1 | +0.2 |
| JDS CH2 +1 V, rep 1 | +0.2 | +0.2 |

**Reading.**
- JDS CH1 into scope CH2 reads **+50.1 on both repeats**, so **scope CH2 is not
  inverted**. It matches scope CH1 before the swap (+49.8).
- The JDS **CH2** output did not follow its own offset steps at all here.
- On the first CH1 write, the JDS CH2 output moved by about −1 V (scope CH1 −50.0).
- So the JDS CH2 DC offset is not reliable, and not independent of CH1 writes. That is
  the source of both the "first step reads 0" effect and the −50 in runs 1–2.
- No model of its behaviour is claimed from four readings. `bench.py` now says: do DC
  work on JDS CH1.

**Effect on §5.** None of the conclusions change. Every coupling criterion needs only a
1 V change of either sign, and the AC/DC contrast and the isolation were measured within
one sequence on the same channels.

Log: `captures/exp60/cable_swap_sign_test.log`.

## 9. Addendum — the probe setting and the 20M limit were label-only too

The same audit (button → what it writes → readback) on Settings → Oscilloscope:

- **Probe 1X/10X** reached nothing that shows volts. Its only consumer was a bit in the
  same unobeyed USART command, so a user on a 10× probe read 10× too small.
  - **Fix:** `scope_cal` takes a registered probe-factor source (the setting stays in
    `scope_state`) and applies it in `scope_cal_volts_per_count()`. Badges, cursor, and
    the status-bar and popup V/div labels all derive from that one point.
  - `scope_cal_mv_per_count()` and the `fpga scope cal` table stay **BNC-referred**.
    The dump now prints the probe factors on their own line.
  - SELECT shows a popup (it changed the probe silently).
  - Host test: a CH2-only ×10 source scales CH2's volts, volts/div and label, and leaves
    the cal table, CH1 and the true-scale gate alone. Control: no source, or a nonsense
    factor, means ×1.
- **20M bandwidth limit**: its only consumer is that USART bit; no hardware path is
  known. The menu now shows `n/a` and the toggles are refused.

**Bench (v5, then v6):** `coupling_bench.py` gained three probe criteria.
- **v5: 16/17.** Probe ×10.000 exact (CH1 Vpp 2.028 V → 20.284 V → 2.028 V), but the
  byte-identical cal-dump criterion **failed**. The µV/count columns were identical; the
  dump's **V/div column** had moved, because it reused the on-screen (probe-tip) label.
  That is the confusion the BNC/tip split exists to prevent, so the firmware changed:
  `scope_cal_range_label_bnc()` for the dump. The criterion now excludes only the new
  probe-factor line, a change recorded in the script.
- **v6: 17/17.**
- `mask_bench.py` regression: 10/10 on v5.

Not verified on screen: the `n/a` menu text. Reaching the menu cycles through
signal-generator mode, which shares DAC1 with CH1's offset in this image.

Logs: `captures/exp60/coupling_probe_bench_v5.log`, `coupling_probe_bench_v6.log`,
`mask_bench_v5_regression.log`.
