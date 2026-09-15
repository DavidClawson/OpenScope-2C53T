# EXP-50 — read the held record at edge + fill + 30 ms; arm at the bracket's end

- **Date:** 2026-09-15
- **Unit:** bench unit #1, `guest-coldtrace` (two-phase read + `fpga holdread` + `status` latency)
- **Script:** `scripts/exp50_hold_read_latency.py` (log `exp50.log`)
- **Status:** **CONFIRMED** — 14/14 EXP-47 lines hold, latency down ~190 ms at every measured code

## 1. Problem

EXP-48/49: the FPGA holds the completed record from the trigger until the
bracket ends (fill + ~190 ms) and ignores reads as arms inside it. Our task
read at fill + 230 ms — the arming read — so every record reached the
display ~200 ms later than it had to. Can the held record be read and
committed at fill + margin without breaking EXP-47's acceptance?

## 2. Hypothesis

If reads inside the bracket return the complete held record: `status` "acq
latency" (edge → committed record) ≈ fill + 30 + read = 55/115/240/445 ms at
0x0E/0x10/0x11/0x12 (was 250/311/435/640), NORMAL still sustains with edges
= commits, SINGLE one-shot, AUTO 1.00 edges/pair with a static body.
Falsifier: gross seams in AUTO (the early read is not the held record), or
any EXP-47 line regressing.

## 3. Procedure

Same as EXP-47 (JDS6600 201.2 Hz 3 Vpp → CH1, range 5, fresh boot, nothing
typed) plus `status` after every `spi3 frame` for the latency field. The
task's default is now two-phase: hold-read at `fpga holdread` (derived
fill + 30) → commit; arming pair read at `fpga postedge` (derived fill + 230),
staging only. A `fpga postedge` override restores the single-read path.

Readback first: hold-read 111 ms derived, arming read 311 ms derived, AUTO
budget 475 ms derived, level 0 → 0x80 in force, mode Auto.

## 4. Control

NORMAL at level +100 froze and resumed at 0; the previous image's latencies
(EXP-47 build, same drive) are the comparison column.

## 5. Results

| timebase | fill | NORMAL | edges / commits | latency ms (×4) | previous image |
|---|---|---|---|---|---|
| 0x0E | 21 | advancing | +27 / +28 | 62 62 62 62 | 250 |
| 0x10 | 82 | advancing | +21 / +21 | 123 123 123 122 | 311 |
| 0x11 | 205 | advancing | +15 / +14 | 247 ×4 | 435 |
| 0x12 | 410 | advancing | +10 / +10 | 451 452 452 452 | 640 |

Negative control 0 → +100 → 0: advancing / FROZEN / advancing. SINGLE 3/3
one record then held. AUTO at 0x10: 1.00 edges/pair, body 5.8–6.2, 0/10
gross, latency 123 ×10; status-only 6 s: 3.17 edges/s = 3.17 pairs/s (was
~2.9 — the arming read now lands at the bracket's end rather than after the
frame dump). Host suites 14/14.

## 6. Blind spots

- Latency is tick-resolution (1 ms) and includes the ~11 ms read + commit.
- The 30 ms margin is chosen, not swept: whether the record is complete
  earlier than fill + 30 (or at the edge itself, if PC0 marks completion
  rather than the trigger) is untested — a `fpga holdread` sweep would say.
- Unmeasured rates use 300 ms hold-read / 600 ms arm, both untested.

## 7. Conclusion

- **Established:** the held record is complete and static at fill + 30 ms
  after the edge at every measured code; reading it there costs nothing and
  cuts edge-to-display latency by ~190 ms. Capture rate unchanged in kind
  (FPGA-bound), slightly up in practice.
- **Shipped as default:** two-phase read, `fpga holdread` knob, `status`
  latency field.
