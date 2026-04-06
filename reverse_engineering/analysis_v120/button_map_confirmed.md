# FNIRSI 2C53T Button Map — Hardware Confirmed (2026-04-01, PRM fix 2026-04-04)

Verified on real hardware using `fulltest2.c` with bidirectional 4x3 GPIO matrix scan
at 500Hz (TMR3 ISR). Every button tested individually with visual confirmation on LCD.

## Complete Button Map

| Bit | Event Mask | Physical Button | GPIO Source | Matrix Position | Confirmed |
|-----|-----------|----------------|-------------|-----------------|-----------|
| 0 | 0x0001 | **POWER** | PC8 | Passive (active LOW) | YES |
| 1 | 0x0002 | **AUTO** | PE2 + PC10 | Row PE2, Col PC10 | YES |
| 2 | 0x0004 | **CH1** | PC5 + PC10 | Row PC5, Col PC10 | YES |
| 3 | 0x0008 | **MOVE** | PB0 + PA8 | Row PB0, Col PA8 | YES |
| 4 | 0x0010 | **SELECT** | PC5 + PA8 | Row PC5, Col PA8 | YES |
| 5 | 0x0020 | **TRIGGER** | PA7 + PA8 | Row PA7, Col PA8 | YES |
| 6 | 0x0040 | **PRM** | PB7 | Passive (active HIGH, pull-down) | YES (2026-04-04 — pull-down fix) |
| 7 | 0x0080 | **CH2** | PA7 + PE3 | Row PA7, Col PE3 | YES |
| 8 | 0x0100 | **SAVE** | PB0 + PE3 | Row PB0, Col PE3 | YES |
| 9 | 0x0200 | **MENU** | PE2 + PE3 | Row PE2, Col PE3 | YES |
| 10 | 0x0400 | **UP** | PC13 | Passive (active LOW) | YES |
| 11 | 0x0800 | **DOWN** | PC5 + PE3 | Row PC5, Col PE3 | YES |
| 12 | 0x1000 | **LEFT** | PA7 + PC10 | Row PA7, Col PC10 | YES |
| 13 | 0x2000 | **RIGHT** | PE2 + PA8 | Row PE2, Col PA8 | YES |
| 14 | 0x4000 | **PLAY/PAUSE** | PB0 + PC10 | Row PB0, Col PC10 | YES |

## Summary

- **15 of 15 buttons fully confirmed** on hardware
- **PRM (bit 6, PB7)** — active HIGH. Root cause of earlier detection failure: `button_scan.c` initialized PB7 with **pull-up** (copied from `fulltest2.c`), so the MCU's own pull-up forced idle HIGH and a press couldn't be distinguished from idle. Fix (2026-04-04): initialize PB7 with **pull-down**, matching `btntest2.c:176`. Idle now reads LOW, press reads HIGH, debounce logic already handles it correctly once the skip-bit-6 workaround is removed.

## Matrix Wiring Diagram

```
              Col PE3 (bit grp 0)  Col PA8 (bit grp 1)  Col PC10 (bit grp 2)
Row PA7 (1):     CH2 (0x0080)       TRIGGER (0x0020)      LEFT (0x1000)
Row PC5 (2):     DOWN (0x0800)      SELECT (0x0010)       CH1 (0x0004)
Row PB0 (4):     SAVE (0x0100)      MOVE (0x0008)         PLAY/PAUSE (0x4000)
Row PE2 (8):     MENU (0x0200)      RIGHT (0x2000)        AUTO (0x0002)

Passive:
  PC8  → POWER (0x0001, active LOW)
  PB7  → PRM   (0x0040, active HIGH, pull-down)
  PC13 → UP    (0x0400, active LOW)
```

## Scan Algorithm

1. Read 3 passive pins (PC8, PB7, PC13)
2. Phase 1: Set PA8/PC10/PE3 as output LOW, read PA7/PB0/PC5/PE2 as input pull-up → identifies which ROW
3. Phase 2: Swap — set PA7/PB0/PC5/PE2 as output LOW, read PA8/PC10/PE3 as input pull-up → identifies which COLUMN
4. Row × Column intersection = unique button ID
5. Debounce: 70 ticks at 500Hz = 140ms to confirm press

## Debounce Timing

| Event | Ticks | Time at 500Hz |
|-------|-------|---------------|
| Short press confirmed | 70 (0x46) | 140 ms |
| Long press / repeat start | 72 (0x48) | 144 ms |
| Repeat interval | 2 | 4 ms |
| Release detected | counter 2-69 on release | immediate |

## Test Firmware

- File: `firmware/src/fulltest2.c`
- Build: `make -f Makefile.hwtest TEST=fulltest2`
- Features: TMR3 ISR at 500Hz, matrix scan, debounce, USART exchange, SPI3 probe, visual bit display
