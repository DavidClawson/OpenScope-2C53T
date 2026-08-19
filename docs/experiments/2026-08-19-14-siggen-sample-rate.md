# EXP-14 — the signal generator was never an instrument

- **Date:** 2026-08-19
- **Unit:** bench unit #1 + the ESP32 bench source (`esp32_siggen/`)
- **Build:** device `Aug 18 2026 17:21:17` (`guest-coldtrace`); siggen reflashed
  mid-experiment to add rate reporting
- **Status:** **CONFIRMED** — rescales every sample rate this project has
  published, and produces its first cross-rig agreement on an absolute quantity

## 1. Problem

Stlkv reported (issue #18, 2026-08-18) that on codes `0x0D`–`0x10` our sample
rates sit a consistent **~1.2× above** his 2C23T-port rig's:

| code | ours | his |
|---|---|---|
| 0x10 | 14,890 | 12,437 |
| 0x0F | 30,235 | 25,000 |
| 0x0D | 150,706 | 125,000 |

He noted the shape of it precisely: *"Ratios agree beautifully; absolutes
don't."* A discrepancy that is constant across codes is not a disagreement
about the device. It is a scale error in one of the two measuring chains.

Separately, this project has carried a "the siggen's frequency readback is
~0.82× off" footnote since 2026-08-17, when stock firmware's own counter read
82 Hz and 208 Hz for commanded 100 Hz and 250 Hz. It was recorded as a caveat
and never chased. **0.82 and 1/1.2 are the same number**, and nobody had put
the two facts next to each other.

## 2. Hypothesis

Our ESP32 source delivers a frequency lower than the one commanded, by a
constant factor near 0.82, and every sample rate we have published — all fitted
against *commanded* frequencies — is therefore 1/0.82 ≈ 1.22× too high.

- **If true:** the generator's own achieved DDS loop rate is measurably below
  its assumed `FS = 40000`, our rates scale down onto Stlkv's, and the ratios
  between codes are untouched.
- **If false:** the loop runs at 40 kHz and the 1.2× lives somewhere else — in
  his chain, or in a genuine difference between a 2C23T and a 2C53T.

## 3. Procedure

**3a. Read the source.** `esp32_siggen.ino` is a software DDS:

```c
static const uint32_t FS = 40000;                 // sample rate (Hz)
...
g_inc[ch] = (uint32_t)((double)hz * 4294967296.0 / (double)FS);
...
void loop() {
  static uint32_t next_us = 0;
  uint32_t now = (uint32_t)micros();
  if ((int32_t)(now - next_us) >= 0) {
    next_us = now + (1000000UL / FS);             // <-- from `now`, after the fact
    ...
    dacWrite(DAC_PIN[0], next_sample(0));
    dacWrite(DAC_PIN[1], next_sample(1));
  }
  poll_serial();
}
```

`next_us` is set from `now` rather than advanced by a fixed period, so the
schedule cannot correct for its own lateness. If the body costs more than 25 µs
the 40 kHz target is never approached — the loop simply free-runs at whatever
two `dacWrite()` calls plus a serial poll cost, and `FS` becomes fiction while
remaining the number every commanded frequency is computed from.

**3b. Make it measurable.** Patch the sketch to count emitted samples and
divide by elapsed `micros()` — the ESP32's own crystal-derived clock, which is
independent of everything under test. Add `fs` (report), `fs reset`, and
`usefs 0|1` (whether `set_freq` divides by nominal 40000 or by the measured
rate). **`usefs` defaults to 0**, so flashing this firmware leaves the
instrument bit-identical to the one that took every previous measurement and
does not silently rescale the archive.

**3c.** Measure the achieved rate. **3d.** A/B the correction end-to-end
through the scope. **3e.** Re-measure the ladder. **3f.** Fold-test the result.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| rate stable over time | constant | 32,999.3 / 32,998.9 / 32,999.2 Hz over 5 / 15 / 36 s | ✓ |
| rate independent of waveform | same for sine/square/tri | 32,999.4 / 32,999.4 / 32,999.5 | ✓ |
| rate independent of *mode* | — | **DC differs: 33,301.5 (+0.9%)** | ⚠ see below |
| PWM does not perturb the loop | same | 32,999.6 with PWM on | ✓ |
| source held for a whole ladder run | <0.2% drift | 33,297.6 → 33,300.1, **0.01%** | ✓ |
| two scope read paths agree at 0x10 | <5% | 12,614 vs 12,537, **0.6%** | ✓ |

**The DC result is the control that earned its keep.** `next_sample()` returns
early for DC mode, so each channel parked in DC gives the loop ~300 Hz back:

| configuration | achieved | ratio |
|---|---|---|
| both channels sine | 32,999.5 | 0.8250 |
| one sine, one DC | 33,298.8 | 0.8325 |
| both DC | 33,557.4 | 0.8389 |

Repeatable to five digits. This is not noise, it is a clean function of state —
and it means **the source rate must be measured in the same channel
configuration the sweep runs in.** The first corrected ladder run did not do
that (it measured before parking CH2) and carried an unresolved ±0.9%. It was
re-run with the configuration pinned; only the pinned run is reported in §5.

A second, unglamorous finding from the same session: `Siggen.pwm_off()` in
`bench.py` has **never once succeeded** — the sketch answers a bare `[pwm] off`
which `parse_pwm_status()` cannot parse, so the helper raised every time it was
called. Fixed.

## 5. Results

### 5a. The generator, measured against its own crystal

```
[fs] nominal=40000  achieved=32999.3 Hz  ratio=0.8250  dt=5.4s
[fs] nominal=40000  achieved=32998.9 Hz  ratio=0.8250  dt=15.8s
[fs] nominal=40000  achieved=32999.2 Hz  ratio=0.8250  dt=36.2s
```

**0.8250.** Against the two independent estimates already on record:

| source of the estimate | factor |
|---|---|
| stock 2C53T firmware's frequency counter (2026-08-17, two tones) | 0.826 |
| Stlkv's rig vs ours, three codes (2026-08-18) | 0.8305 |
| **direct measurement against the ESP32 crystal (this experiment)** | **0.8250** |

Three chains with nothing in common agree to 0.7%.

### 5b. End-to-end A/B through the scope

Timebase `0x10`, eight tones 200–2600 Hz, fitted through the origin:

| source setting | fitted slope | implied fs |
|---|---|---|
| `usefs 0` — divides by nominal 40000 | 0.06723 (R² 0.9980) | 15,230 |
| `usefs 1` — divides by measured 33000 | 0.08190 (R² 0.9988) | 12,503 |

Slope ratio **1.2182** against the predicted 40000/32999 = **1.2121** — 0.5%.

A single-point version of this A/B gave 1.2857 and looked like a refutation; it
was one pair of integer bins (63 → 81) at a resolution that cannot support the
claim. Recorded because the temptation to believe the first number was real.

### 5c. The ladder, re-measured with the source pinned

Source: CH1 sine + CH2 parked, 33,297.6 Hz, drift 0.01% across the run.

| code | fitted fs | R² | previous (uncorrected) | Stlkv | vs Stlkv |
|---|---|---|---|---|---|
| 0x10 | **12,575** (mean of two read paths) | 0.999 | 14,853.6 | 12,437 | **1.1%** |
| 0x0F | **25,736** | 0.999 | 30,235 | 25,000 | 2.9% |
| 0x0E | **49,056** | 0.993 | 62,958 | — | — |
| 0x0D | **119,678** | 0.947 | 150,706 | 125,000 | 4.3% |
| 0x0C | 243,119 | 0.904 | — | — | still source-limited |
| 0x0B | 413,433 | 0.806 | — | — | still source-limited |
| 0x0A | 649,681 | −0.14 | — | — | still source-limited |
| 0x08 | 1,088 | 0.979 | withdrawn | — | **still incoherent** |

Run-to-run spread between the two corrected runs: **0.02% at 0x10**, 3.5–4.8%
at `0x0D`–`0x0F`, where our 4.5 kHz ceiling puts the tones in low bins.

Code `0x08` reproduced its EXP-12 signature exactly — reads of one unchanged
tone spread by 59–141 bins. The correction does not rescue it and was never
expected to; it stays withdrawn.

### 5d. Fold test — and it discriminates

Above Nyquist a tone lands at `|f − round(f/fs)·fs|`, a position far more
sensitive to `fs` than the in-band slope. Our source cannot exceed Nyquist at
these rates *nominally*, but a 4-samples-per-cycle sine still carries a
dominant fundamental, so pushing the DDS to 7–11 kHz reaches past 6,288 Hz.

| tone | predicted bin @ 12,575 | measured (top 3) | miss | predicted @ old 14,854 | miss |
|---|---|---|---|---|---|
| 7,000 | 454.0 | 449 / 451 / **455** | **1.0** | 482.6 | 27.6 |
| 8,000 | 372.5 | **366** / 369 / 363 | 3.5 | 472.5 | 103 |
| 9,000 | 291.1 | 285 / 290 / **291** | **0.1** | 403.6 | 112 |
| 10,000 | 209.7 | 200 / 203 / **206** | 3.7 | 334.6 | 128 |
| 11,000 | 128.3 | **123** / 122 / 120 | 5.3 | 265.7 | 142 |

**Worst miss 5.3 bins on the corrected rate, 142 on the pre-correction rate.**
The test does not merely accept the new number, it rejects the old one.

Also worth stating, because it protects earlier work: **the fold test is
scale-invariant under this bug.** It compares an alias position computed from
`f_commanded` and `fs_fitted`, and aliasing depends only on `f/fs` — both of
which carried the same factor. So EXP-10 and EXP-12's fold conclusions were
never affected by the source error, and code `0x08`'s failure there stands
exactly as recorded.

## 6. Blind spots

- **The ESP32 crystal is unverified.** It is a different, better-founded
  assumption than "the loop hits 40 kHz" — a crystal is a component with a spec,
  a scheduling loop is not — but it has not been checked against a reference.
  Everything here is traceable to that crystal and nothing further.
- **The residual 1.1–4.3% against Stlkv is unexplained.** It could be his
  generator, our crystal, our low-bin quantisation, or a real unit difference.
  Two rigs agreeing to ~1% is not two rigs agreeing.
- **The round 1-2-5 ladder is a hypothesis, not a result.** 12.5k / 25k / 50k /
  125k sits inside our spread on every code and Stlkv lands essentially on it,
  which is suggestive — but the measured values, not the round ones, are what
  went into `scope_timebase.c`.
- **Nothing here touches the vertical axis.** The loop-rate error moves
  frequency only; amplitude is set by DAC counts. `scope_cal.c` is unaffected
  and absolute volts remain unverified.
- **`0x0C`–`0x0A` are still source-limited**, unchanged by any of this.
- **One session, one unit, one generator.**

## 7. Conclusion

- **Established:** the ESP32 bench source delivers **0.8250×** the frequency it
  is commanded, because its DDS loop reschedules from `now` after the work is
  done and free-runs at 32,999 S/s against an assumed 40,000. Measured against
  the ESP32's own crystal, stable to five digits, and independently corroborated
  by stock firmware's counter and by Stlkv's rig.
- **Consequence:** every sample rate this project has published was **1.21×
  too high**. `scope_timebase.c` is rescaled; `tests/test_scope_timebase.c`
  gains a guard that fails the build if the old ladder returns.
- **What survives untouched:** every R², the linearity, the 1-2-5 shape of the
  ladder, both fold conclusions, and the entire vertical calibration. The error
  was purely multiplicative, which is exactly why nothing internal could see it.
- **The milestone:** corrected, our `0x10` lands **1.1%** from an independent
  rig — different unit, different generator, different firmware, different
  method. This is the project's first cross-rig agreement on an absolute
  quantity, and it exists only because somebody else published a number that
  disagreed with ours.
- **Method rule, and it is the whole lesson:** *a source is an instrument.* An
  instrument that has never been checked against anything is not a reference,
  and a constant it was built around is not a measurement. This is the same
  failure as the `/2` SSPI reads, the floating MISO, the invented `vdiv_table`
  labels and the double FFT — a stable, plausible, wrong number that nothing in
  the analysis could contradict because the analysis inherited it.
- **Follow-up:** (1) tell Stlkv, whose item 2 for next session is aimed at
  exactly this gap — he can skip it; (2) make the siggen self-calibrate at boot
  rather than leaving it opt-in; (3) test his falsifiable prediction that `0x07`
  measures the same rate as `0x08` on our rig; (4) check the ESP32 crystal
  against something, when a reference exists.
