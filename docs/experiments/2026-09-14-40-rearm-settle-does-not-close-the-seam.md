# EXP-40 — a settle after the re-arm does not close the seam

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` + `fpga rearmwait <ms>` knob (branch
  `bench/2026-09-14`), image `exp40_coldtrace_rearmwait.bin`
- **Script:** `scripts/exp40_seam_rearm_wait.py`
- **Status:** **REFUTED** (controls held; the arm was live, not frozen)

## 1. Problem

EXP-29 refuted the re-arm as the seam fix; EXP-39 refuted the in-band marker
as a gate source. Stock's op-01 handler is gated on `tbl[0x0804D833 + tb] +
0x32` since the last arm — a per-timebase **time** table (u8: 9/21/41/82 at
codes 0x10–0x13, ≈ 1024/fs in 10 ms units). EXP-29 re-armed and read ~30 ms
later at every timebase, i.e. mid-capture. Is the missing interlock "re-arm,
wait one capture period, then read"?

## 2. Hypothesis

If the reg-01 write restarts a capture that completes and holds, re-arm +
settle ≥ 1024/fs gives records with no seam (max phase step at the ~0.3-sample
floor), while re-arm + settle 0 and no re-arm keep the ~20-sample seam. If the
seam is unchanged, the engine free-runs regardless of the reg-01 write.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp sine into CH1, timebase `0x10` (12,490 S/s, capture
82 ms), settle = ceil(1.15 × 82) = 95 ms, 10 `spi3 frame` records per arm,
seam metric `frame_seam` from EXP-22. Re-arm and settle read back per arm.

## 4. Control

| control | result | passed? |
|---|---|---|
| A1/A2 show seams | medians 23.6 / 20.2 samples | yes |
| generation counter advances, every arm | A1 42–44, B 40–44, **C 14–26**, A2 40–44 per record | yes — and C's lower advance is the settle demonstrably slowing the loop |
| toggles read back | all four arms confirmed | yes |

## 5. Results

| arm | re-arm | settle | median step | p90 | max | seam locations |
|---|---|---|---|---|---|---|
| A1 | off | 0 | 23.62 | 30.90 | 30.99 | 768 672 608 512 448 352 256 160 896 800 (marching) |
| B | ON | 0 | 22.71 | 25.88 | 29.37 | 96 32 768 672 384 288 192 928 32 768 |
| **C** | **ON** | **95 ms** | **19.69** | 27.62 | 27.97 | 384 96 768 512 32 736 416 128 864 544 |
| A2 | off | 0 | 20.17 | 27.53 | 29.10 | 192 928 32 736 640 352 256 160 96 32 |

Pooled: re-arm off (n=20) median 20.9; re-arm + 0 (n=10) 22.7; re-arm + 95 ms
(n=10) 19.7. No separation; A1/A2 drift 23.6 / 20.2 is as large as any
between-arm difference.

## 6. Blind spots

- One timebase. At a slower code the settle would be longer but the mechanism
  (below) makes the result independent of that.
- The settle is applied *after* the reg-01 write; EXP-41 then showed the
  reg-01 write does not start a capture at all, so a settle after it waits
  for nothing. This experiment could not have distinguished "the write does
  nothing" from "the write starts a capture that does not hold".

## 7. Conclusion

- **Established:** re-arm plus a post-re-arm settle ≥ the capture period
  leaves the seam untouched. The engine does not become static on a reg-01
  write followed by a wait.
- **Excluded:** "re-arm, wait, read" as the interlock, in this form.
- **NOT excluded:** a wait keyed to the thing that actually starts a capture.
  EXP-41 (same session, immediately after) found what that is.
