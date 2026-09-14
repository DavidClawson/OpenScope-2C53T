# EXP-30 — the data-ready line has never fired, and every record we have ever taken was a free-run read

**Date:** 2026-09-13 · **Unit:** bench #1 · **Build:** `guest-coldtrace-meter` + `fpga acqgate`
**Status:** Tested arm VOID by its own control. The follow-up measurement is the result.

## Problem

EXP-29 refuted stock's re-arm as the fix for the acquisition seam and located
the mechanism: one discontinuity per record whose location marches through the
buffer, which is the signature of reading across a live write pointer.

The untested half was stock's FIRST step. Its loop gates on the timebase
register *before* reading. Ours does not: in AUTO mode, when no trigger arrives
within the wait window, `fpga.c` falls through and reads the live buffer. In
AUTO — the default — that is the common case, not the rare one.

## Hypothesis

If the free-run read is the seam, then gating the read on a data-ready edge
collapses the discontinuity toward the noise floor.

**Falsifier:** an unchanged distribution.

## Procedure

`fpga acqgate on|off` added this session: in AUTO, a missing trigger becomes a
timeout instead of a fall-through read. Runtime-toggleable, default off, and
paired with `fpga rearm` because stock does both and they are load-bearing for
each other — the gate waits for a data-ready edge, the re-arm starts the next
capture so another edge can arrive.

Same rig as EXP-29: 201.2 Hz at 12,490 S/s, `spi3 frame`, block-phase seam
metric, A/B/A, 10 records per arm. Driver: `scripts/exp30_seam_gate.py`.

## Control

Three, and the second is the one that matters:

1. The OFF arms must show seams, or the metric is blind.
2. **The frame generation counter must ADVANCE between records in the gated
   arm.** A gate that starves the acquisition task freezes the published
   buffer, and a frozen record has a *constant* seam — which would read as a
   spectacular success. This control was written into the script before the
   run precisely because that was the most likely way to be fooled.
3. Gate and re-arm states read back off the device, never trusted from the
   setter.

## Results

| arm | gate | re-arm | median | p90 | gen advance |
|---|---|---|---:|---:|---|
| A1 | off | off | 23.63 | 27.01 | 44–46 per record |
| B | **ON** | **ON** | **21.66** | **21.66** | **0 — FROZEN** |
| A2 | off | off | 25.49 | 30.30 | 44–46 per record |

**Control 2 fired.** Arm B returned 21.66 samples ten times, identically, with
p90 equal to the median — the tightest distribution of the night. The generation
counter did not move once. The trace was frozen and every "record" was the same
stale buffer read ten times.

Without that control this would have been written up as the gate working.

`fpga acqgate` reported **637 skips**: the gate refused 637 reads and let zero
through. Not one data-ready edge arrived in the whole arm.

### The follow-up, which is the actual finding

```
SPI3 OK: 1315
PC0 edges: 0
SPI3 first byte: 0x80
```

**Zero. Across 1315 successful reads.** The edge interrupt is armed and correct
— `EXINT0_IRQHandler` at `fpga.c:3556`, line 0 mapped to port C at `fpga.c:5356`
— and it has never once fired.

Meanwhile the FPGA *is* announcing data-ready: the `0x80` marker is present in
the first byte of the read window, which is the in-band signal Stlkv documented
and which our own reader already captures.

## Blind spots

- One unit, one session. PC0 may behave differently on another board.
- This does not establish WHY the pin is silent: not connected to the FPGA's
  data-ready output, a level that never transitions, or an input configuration
  problem. Three different causes, none excluded.
- The NORMAL/SINGLE prediction below is read off the code path, **not
  measured** — no shell command sets `scope_state.trigger.mode`, which is
  itself a finding of this week's trigger spec.
- The seam metric resolves location only to 32 samples.

## Conclusion

**Established:** PC0 does not edge on this unit, and therefore **every
oscilloscope record this project has ever produced came from the free-run
fallback read.** That is a complete explanation of the seam: an un-interlocked
read of a buffer being written is exactly a rotation of a live buffer, which is
what EXP-29 measured.

**The gate is not refuted — it was never tested.** Gating on a signal that does
not exist tests nothing about gating. The hypothesis stands and needs a working
data-ready source.

**The viable source is already in our hands:** the in-band `0x80` marker in the
first byte of the read window. That is a gate you can build without knowing
anything about the pin.

**Prediction this hands us, untested and cheap:** in NORMAL or SINGLE trigger
mode the acquisition task waits for a PC0 edge and, on timeout, holds the last
trace without reading (`fpga.c:3658`). If PC0 never edges, **those two modes can
never capture at all.** This week's trigger spec found the trigger controls
decorative in the UI; this says the one part it judged genuinely functional —
the acquisition wait policy — is waiting on a signal that never arrives. Testing
it needs one button press and a look at the generation counter.

`fpga acqgate` is left in the tree, default OFF, with its warning intact. It is
the right instrument pointed at the wrong signal.
