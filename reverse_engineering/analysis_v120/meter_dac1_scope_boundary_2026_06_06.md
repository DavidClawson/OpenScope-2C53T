# DAC1 / Scope Boundary for DMM Work

Date: 2026-06-06

Stock source: `APP_2C53T_V1.2.0_251015.bin`, decompile
`reverse_engineering/analysis_v120/full_decompile.c`.

## Conclusion

Do not treat DAC1 (`0x40007408`) as DMM calibration. The stock-visible DAC1
formula is real, but the recovered users are scope frontend/trigger/autorange
paths. It is not evidence for a meter reference voltage, a DMM low-voltage
coefficient, or a current-mode correction.

The low-DCV live blocker (`0.200 V` source versus `0.4366 V` decoded from
`5A A5 44 8E EF E7 07 24 80 00 01 89`) must stay unresolved until stock DMM
state, W25Q/system-file data, H2 acceptance/effect, or broad bench evidence
proves a real calibration path.

## Stock Evidence

| Evidence | Offset / lines | Classification |
|---|---|---|
| `FUN_080018a4(param_1)` writes PC12 and PE4/PE5/PE6, then recomputes DAC1 from `DAT_20000358..DAT_200003bc` tables and `DAT_200000fc + 100` | `0x080018A4`, `full_decompile.c:2206..2295`; DAC write at `0x08001A48..0x08001A50` | GPIO writer plus DAC1 scope-threshold update |
| `FUN_08001c60` increments `DAT_200000fa/fb` on clipping/autorange, calls `FUN_080018a4` or `FUN_08001a58`, then queues selector `4` | `full_decompile.c:2564..2574` | scope/siggen autorange, not DMM |
| `FUN_08001c60` inlines the same DAC1 recompute after changing `DAT_200000fc` | `full_decompile.c:2603..2624`, `2710..2731` | scope channel threshold refresh |
| `FUN_08019e98` derives `DAT_20000125` from scope UI/range state, queues scope/acquisition selectors, and recomputes DAC1 from `DAT_200000fa` | `full_decompile.c:6960..7020` | scope main FSM/autorange |
| `FUN_08019e98` uses `DAT_20000128 & 0x0f`, calls both mux writers, and refreshes DAC1/CH2 threshold | `full_decompile.c:7771..7990`, direct calls at `0x0801C780..0x0801C784` | scope submode/range path |
| Master init applies saved bytes through both mux writers | `0x08025544..0x0802554C`, `0x0802723E..0x0802724A` | boot/saved-state apply only; not runtime DMM calibration proof |

## Mux Writer Scope-Tail Guard

`scripts/test_stock_meter_literals.py` now also guards the decompile context for
the two mux-writer tails:

- `full_decompile.c:2274..2293`:
  `gpio_mux_portc_porte` reads `DAT_20000125`, `DAT_2000010c`, and
  `DAT_200000fc + 100`, then writes `_DAT_40007408` / `_DAT_40007404`.
- `full_decompile.c:2375..2392`:
  `gpio_mux_porta_portb` reads `DAT_20000125`, `DAT_2000010c`, and
  `DAT_200000fd + 100`, then writes `_DAT_40001c34`.

Those adjacent state bytes are scope threshold/calibration selectors and
offsets. They are not the missing DMM `ms[0x02]`/`ms[0x03]` runtime writer and
not a low-DCV correction or meter calibration coefficient.

## Boundary for Open Firmware

- It is valid to project stock DMM selector slots into the recovered GPIO mux
  writers while the exact DMM `ms[0x02]`/`ms[0x03]` writers remain missing.
- It is not valid to port the DAC1 formula as a DMM correction unless a future
  stock trace proves DAC1 is connected to a DMM measurement path.
- A complete H2 SPI3 replay byte count is also not acceptance proof; see
  `spi3_bulk_cal_resolved.md` and `h2_extracted/FINDINGS.md`.
