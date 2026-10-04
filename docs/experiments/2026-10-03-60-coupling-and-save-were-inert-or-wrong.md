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
- **CH2 moves the opposite way to CH1** (+1 V → −50 counts). The criteria use |Δ|, so
  this test cannot say which is inverted: the scope's CH2 front end, or the JDS CH2
  output. **OPEN, and it matters.** If the scope inverts CH2, then its trace, its trigger
  slope and any cursor voltage on CH2 are upside down. A cable swap at the generator
  decides it.
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
- **NOT excluded:** CH2 sign inversion in the scope; an inert BW-limit control.
- **Follow-up:**
  - cable-swap test for the CH2 sign;
  - audit the BW limit and the remaining Settings → Oscilloscope items the same way;
  - add `coupling_bench.py` next to `mask_bench.py` in the regression set.
