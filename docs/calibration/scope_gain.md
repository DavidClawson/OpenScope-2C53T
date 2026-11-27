# Scope gain table: the data per unit

`firmware/src/ui/scope_cal.c` holds one number per channel and range: mV per
ADC count at the BNC. Since 2026-11-27 that table is shared, derived from
more than one unit by the rule below. To add a unit, add its columns here and
re-derive; `firmware/tests/test_scope_cal.c` holds the same per-unit numbers
and fails if a table row stops following the rule.

## The units

| | s00500's unit | sparsematrix's unit ([#95]) | bench unit #1 (withdrawn) |
|---|---|---|---|
| Reference | the 2C53T's own generator, its scale set against a Rigol DS1104Z after Self-Cal: 0.7937 mV per DAC code, about ±1-2 % absolute ([EXP-80]) | 4-digit DMM on the same node | ESP32 generator at commanded amplitudes, later scaled by 0.92 against a JDS6600 (EXP-20/21) |
| Method | 4-9 generator levels per range, least-squares slope of counts against volts, both channels | one level per point: record mean minus a captured zero (8 records, tip grounded), against the DMM | span at five drive amplitudes |
| Uncertainty | slope SE up to about 1.2 % on the ranges used, plus the reference's ±1-2 % | about ±0.5 % (±1 % on range 4) | see EXP-08, EXP-20/21 |
| Ranges | 0-9 (8-9 not usable, below) | 4-9 | 4-9 |
| Date | 2026-11-26 | 2026-11 | 2026-08-17 |
| Write-ups | [EXP-83] (CH1), [EXP-84] (CH2), [EXP-89] (ranges 2-9 again, the zero), [EXP-92] (ranges 0-3) | [#95] | `docs/experiments/2026-08-17-08-per-range-gain-ladders.md` |

The s00500 write-ups live on that fork (`s00500/OpenScope-2C53T`, branch
`feat/remote-control`); their experiment numbers are that fork's.

## The numbers (mV per count)

| range | CH1 s00500 | CH1 sparsematrix | **CH1 table** | CH2 s00500 | CH2 sparsematrix | **CH2 table** |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0.4028 | | **0.4028** | 0.3978 | | **0.3978** |
| 1 | 0.7981 | | **0.7981** | 0.7928 | | **0.7928** |
| 2 | 1.9872 | | **1.9872** | 1.9732 | | **1.9732** |
| 3 | 3.927 | | **3.927** | 3.906 | | **3.906** |
| 4 | 7.866 | 7.77 | **7.82** | 7.783 | 7.93 | **7.86** |
| 5 | 20.01 | 19.83 | **19.92** | 19.86 | 19.98 | **19.92** |
| 6 | 39.79 | 39.46 | **39.63** | 39.39 | 39.95 | **39.67** |
| 7 | 78.90 | 78.19 | **78.55** | 77.11 | 79.18 | **78.15** |
| 8 | (194.87) | 193.5 | **193.5** | (206.53) | 196.0 | **196.0** |
| 9 | (404.21) | 384.2 | **384.2** | (430.48) | 391.1 | **391.1** |

At 32 counts per division the table reads, CH1: 13 mV, 26 mV, 64 mV, 126 mV,
250 mV, 637 mV, 1.27 V, 2.51 V, ~6.19 V, ~12.29 V per division (CH2 within
1.8 %). With a 10x probe, ten times that.

## The rule

1. **Each entry is the mean of the units that measured it at usable
   resolution.** Ranges 4-7: both units. Ranges 0-3: s00500's only (nobody
   else has measured them). Ranges 8-9: sparsematrix's only.
2. **s00500's ranges 8-9 are not usable**, shown in brackets above. The
   generator's 3 V spans only 7-15 counts there, and the fits' intercepts put
   the generator's own 0 V at -55 and -71 mV (CH2, EXP-89), where every other
   range gives +7 to +10 mV. Something in those fits is off. They stay as a
   cross-check: 0.7 to 10 % from sparsematrix's numbers.
3. **Tiers.** MEASURED: two units agree within 3 %, or one unit's
   multi-level fit with both channels within 2 % (ranges 0-3). PROVISIONAL:
   one unit at usable resolution (ranges 8-9). The label then carries a `~`.
4. **No source scale.** Both references are absolute, so
   `SCOPE_CAL_SOURCE_SCALE` is 1.0.

## Checks the table passes (asserted in `test_scope_cal.c`)

- **The two units agree** within 2.6 % on every entry of ranges 4-7 (s00500's
  reads 0.8-1.2 % higher on CH1 and 0.6-2.6 % lower on CH2).
- **The channels agree** within 1.8 % on every range.
- **The ladder is 1-2-5**: every step is within 2 % of x2 or x2.5
  (x2, x2.5, x2, x2, x2.5, x2, x2, x2.5, x2 from range 0 to 9).

## What changed against the table it replaces

Bench unit #1's rows, times its 0.92 source scale, against the new table:

| range | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---:|---:|---:|---:|---:|---:|
| CH1 | +65.6 % | +0.8 % | -0.3 % | +3.6 % | +32.7 % | -15.7 % |
| CH2 | +11.1 % | -3.2 % | -3.3 % | -1.4 % | +5.0 % | 0.0 % |

Ranges 5-7 held, as their MEASURED tier said. The provisional ranges were
the ones off, worst on CH1, and range 8 is the power-on default.

Ranges 0-3 had no gain (0.0f): in August they sat at 255 with a quiet input
(EXP-11). With the offset references armed as `main` does now (DAC1 2048,
TMR13 2544), a shorted input sits at 44-49 counts on CH1 and 101-106 on CH2
on every range (EXP-89, s00500's unit), and the four ranges measure as
cleanly as the others. Unit #1 has not been re-checked since August.

## Not covered

- Two units for ranges 4-7, one for the rest. A third unit's numbers on
  ranges 0-3 or 8-9 would turn those into a cross-checked mean.
- Gain only. The 0 V of each range is set by the offset references and is
  not in this table.
- Temperature and drift: each unit was measured in one session.

[#95]: https://github.com/DavidClawson/OpenScope-2C53T/issues/95
[EXP-80]: https://github.com/s00500/OpenScope-2C53T/blob/feat/remote-control/docs/experiments/2026-11-26-80-the-generator-level-after-self-cal.md
[EXP-83]: https://github.com/s00500/OpenScope-2C53T/blob/feat/remote-control/docs/experiments/2026-11-26-83-loopback-ch1-gain-crosstalk-and-the-badge.md
[EXP-84]: https://github.com/s00500/OpenScope-2C53T/blob/feat/remote-control/docs/experiments/2026-11-26-84-ch2-gain-and-its-offset-reference.md
[EXP-89]: https://github.com/s00500/OpenScope-2C53T/blob/feat/remote-control/docs/experiments/2026-11-26-89-the-zero.md
[EXP-92]: https://github.com/s00500/OpenScope-2C53T/blob/feat/remote-control/docs/experiments/2026-11-26-92-ranges-0-and-1.md
