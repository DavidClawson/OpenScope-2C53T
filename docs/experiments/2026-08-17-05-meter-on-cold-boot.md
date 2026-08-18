# EXP-05 — Multimeter working on a cold-boot scope build

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` @ 18:45:30 (meter mode-entry sequence)
- **Status:** **CONFIRMED**

## 1. Problem
EXP-04 gave two scope channels from a cold boot, but the meter was untested
there. A first attempt (PC11 driven from the UI mode) produced an **audible
relay click and a frozen DMM screen** — necessary but not sufficient.

## 2. Hypothesis
PC11 is only the first third of stock's mode-entry sequence. Stock's mirror pair
(`0x0800E360` enter / `0x0800E3E4` exit) does pins **and** bus **and** commands:
USART2 UEN, resume dvom_TX/dvom_RX, PC11, reseed. A coldtrace build does none of
the rest — `fpga_init` returns early "before the meter-frontend routing + meter
USART commands", and the task-creation branch makes only the acquisition task.
If that is the whole gap, supplying bus + tasks + activation makes the meter read.

## 3. Procedure
Three changes: `fpga_set_meter_mux()` now raises USART2 UEN alongside PC11;
`dvom_TX`/`dvom_RX`/`meter_poll` are created in warm-handoff builds (safe for
scope mode — every one is gated on `current_mode == MODE_MULTIMETER`); and the
poll task sends stock's activation words once on entry (`0x0508`, `0x0509`, the
PC7-dependent `0x0507`/`0x050A`, `0x0514`) rather than the display task, so a
mode switch does not stall on ~80 ms of framed writes.

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| same cell under STOCK, same session | reads the cell | 1.61 V | ✅ |
| independent external DMM | agrees | 1.61 V | ✅ |
| PC11 actually high (not assumed) | 1 | GPIOC IDT bit 11 = **1** | ✅ |
| bytes actually arriving (not a screen artifact) | > 0 | **+300 in 4 s** | ✅ |

## 5. Results
Meter reads **1.6 V** on the AA cell, against 1.61 V from both stock and an
external DMM. `rx_bytes` climbs steadily; PC11 high.

`echo_frames` stays **0** — data frames (`0x5A 0xA5`) arrive but no echo frames
(`0xAA 0x55`). Recorded as an open detail, not a problem: the meter works.

## 6. Blind spots
- Only DCV on one cell was tested. Resistance, continuity and the other submodes
  are untested on this build.
- The mode-entry path was exercised scope→meter. Meter→scope→meter, and booting
  directly into meter mode, are untested.
- Screen value is user-reported; the counters are the instrument evidence.

## 7. Conclusion
- **Established:** the multimeter works on a cold-boot scope build. Together with
  EXP-04, both month-old symptoms are closed on open firmware with no stock
  involvement.
- **Root cause, both symptoms:** stock applies a **mode-entry sequence** — pins,
  then bus, then commands — and our firmware implemented the pins only. PC11 was
  genuinely load-bearing (A/B/A: 276 bytes / 0 / 276) which made it look like the
  whole answer; it was one third of it.
- **NOT established:** the other meter submodes; mode-cycling robustness; why no
  echo frames appear.
