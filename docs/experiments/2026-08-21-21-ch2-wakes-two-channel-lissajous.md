# EXP-21 — CH2 reads independently once armed; two channels are time-aligned; Lissajous works

- **Date:** 2026-08-21
- **Unit:** bench unit #1
- **Build:** running image, CDC shell (Build Aug 20 2026 14:59:16)
- **Source:** JDS6600, BOTH channels — CH1 → scope CH1 (BNC), CH2 → scope CH2
  (alligator leads). Driven via the new `bench.JDS6600` class.
- **Status:** CONFIRMED. Advances the long-standing CH2 gap.

## 1. Problem

The scope's CH2 (`spi3 opread 0x05`) has been dead or suspect for months. Two
competing readings were on record: op05 returns garbage/zeros, OR op05 simply
echoes CH1 (see [[ch2-gap-bounded]], [[spi3-runtime-dispatch-decoded]]). With a
two-channel generator we can drive **distinct** signals and settle it.

## 2. Results

### 2a. op05 is dead UNTIL CH2's offset ref is armed

Cold, `opread 0x05` returns an **all-zero buffer** (min=max=mean=0). Writing
**`trig2 raw 2048`** — which programs CH2's vertical-offset reference (PA6 /
TMR13 CH1 PWM-DAC, the register CLAUDE.md flagged as never-initialised) — turns
op05 **live** (pp jumped 0 → 108). So the CH2 readout was never broken; its
offset DAC was simply unprogrammed, exactly as the [[vertical-offset-refs]]
decode predicted. `trig2 raw <n>` self-inits and works on any build.

### 2b. The two channels are INDEPENDENT (op05 is not a CH1 echo)

Timebase `0x10` (12,490 S/s — below the ~30 kS/s `opread` tearing threshold;
`0x0E` gave torn records and jumpy FFT bins, a re-learned trap). Drove CH1 =
1 kHz, CH2 = 2 kHz, 3 Vpp each:

    op04 (CH1)  ->  988 Hz
    op05 (CH2)  -> 1988 Hz

Distinct frequencies on the two reads ⇒ **op05 carries CH2's own signal**, not
a copy of CH1. The "op05 == CH1" reading is refuted for this build (with CH2
armed).

### 2c. op04 / op05 are TIME-ALIGNED — real Lissajous is possible

Both channels at 1 kHz, six pair-reads of (op04, op05): both consistently on
bin 81, relative phase spread only **20.8°**. The two channel buffers come from
one synchronised acquisition, so `op04[i]` and `op05[i]` are the same instant —
an X-Y (Lissajous) figure renders coherently. Captured 1:1 / 1:2 / 2:3 / 3:4
ratios; the expected ellipse / figure-8 / etc. appear.

## 3. Caveats / open

- Measured on a build with CH2 armed by hand (`trig2 raw 2048`). Folding a CH2
  offset-arm into the boot path (`make guest-coldtrace-ch2` exists) would make
  this the default.
- `fpga scope center ch2` runs but is slow — it overran the 3 s shell timeout
  (a binary search); not a failure, just needs a longer command timeout.
- CH2 absolute vertical cal is separate and unmeasured here — 2a/2b/2c are about
  *presence and independence*, not gain. The [[vertical-cal-pending-real-source]]
  SCALE work was CH1.
- Read `opread` at a NON-tearing timebase (≤ `0x10`) for any spectral/phase
  claim, or use the acq-task path — the `0x0E` torn records nearly produced a
  false "not independent" result.

## 4. Conclusion

CH2 works: dead only because its offset reference was unprogrammed; arm it with
`trig2 raw 2048` and op05 delivers an independent, time-aligned second channel —
enough for two-trace display and Lissajous. The remaining CH2 work is boot-time
arming and absolute vertical cal, not "does CH2 read at all."
