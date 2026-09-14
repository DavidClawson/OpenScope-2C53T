# EXP-43 — the trigger defaults are accepted on a fresh boot; NORMAL freezes for a new reason

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` at `3ec8fd9` (trigger level owned by the UI, AUTO edge-wait derived from the timebase)
- **Script:** `scripts/exp43_trigger_defaults.py`
- **Status:** **CONFIRMED (AUTO)**, **NEW DEFECT (NORMAL)**

## 1. Problem

EXP-41/42 found the interlock and its two missing pieces: reg 0x08 must sit
inside the signal (the boot burst wrote stock's captured 0xAD) and the AUTO
edge-wait must exceed the capture cycle. Both are now defaults. Do they do
the job on a boot with nothing typed, and does NORMAL mode capture?

## 2. Hypothesis

Fresh boot: `fpga scope level` reports reconcile code 0x80 and 0x80 in force;
`fpga autowait` reports "derived"; PC0 edges advance untouched. At 0x10 the
derived budget is ~327 ms and the body metric shows 0/10 gross seams. NORMAL
at level 0 keeps capturing; NORMAL at +100 (code 0xFC, above the signal)
freezes — the negative control.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp → CH1, range 5. First command after boot is a
readback. `fpga scope trigmode` (new) selects the mode; `fpga scope level`
(new) is the one write path to reg 0x08.

## 4. Control

| control | result |
|---|---|
| readback before any write | reconcile 0x80 = in force 0x80 |
| edges advance on an untouched boot | +8 in 3 s |
| NORMAL at +100 freezes | yes (edges 0) — but see §5: level 0 froze too, so this control cannot distinguish "waiting on the edge" from "dead" |

## 5. Results

| | measured |
|---|---|
| derived AUTO budget at 0x10 | 327 ms |
| AUTO body max RMS ×10 | 6.1 6.0 6.1 6.1 6.1 6.2 6.1 6.1 6.0 6.0 — **0/10 gross** |
| AUTO edges per pair | 0.50 |
| NORMAL, level 0 | generation 382 ×4, **FROZEN**, edges +0, reads +0 |
| NORMAL, level +100 | frozen, edges +0 |
| NORMAL, level 0 again | frozen, edges +0 |

## 6. Blind spots

- The fresh-boot `fpga scope level` line was eaten by the CDC banner; the
  value was confirmed on the next readback.
- The 0.50 edges/pair in AUTO was accepted here as "one per pair"; EXP-45/46
  show it is one per *two* pairs.

## 7. Conclusion

- **Established:** the AUTO defaults hold with nothing typed — trigger level
  inside the signal from the reconcile, derived budget, static body.
- **New defect:** NORMAL froze at level 0 with zero edges and zero reads: it
  waits for an edge before its first read, and a read is what starts a
  capture (EXP-41). Fix attempted next (priming read, `30effa9`).
