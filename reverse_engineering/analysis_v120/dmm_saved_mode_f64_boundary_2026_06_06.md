# DMM Saved-Mode `ms[0xF64]` Boundary

Date: 2026-06-06

This note records the current stock-visible boundary for
`DAT_2000105c` (`0x2000105C`, `ms[0xF64]`). It resolves the older documentation
conflict where `STATE_STRUCTURE.md` treated the same offset as display-only
bitmap draw height.

## Stock Evidence

Current `reverse_engineering/analysis_v120/ram_map.txt` reports only the
display/menu-level refs:

```text
0x2000105C DAT_2000105c (2 refs): FUN_08015f50@08015f50
```

Those two full-decompile refs are read-only selection-style compares in the UI
renderer:

```text
full_decompile.c:4813  if ((uint)DAT_2000105c == (uVar11 & 0xff)) {
full_decompile.c:4824  if ((uint)DAT_2000105c == (uVar11 & 0xff)) {
```

The DMM-relevant stock evidence is outside the RAM-map line:

| Evidence | Stock offset | Meaning |
| --- | --- | --- |
| saved config load | `0x08025E40..0x08025E64` | Loads config words 11/12, stores word 11 to `ms[0xF60]`, stores low halfword of word 12 to `ms[0xF64]`, then stores word-12 high bytes to `ms[0x08]` and `ms[0x09]`. |
| saved mode restore | `0x08026F50..0x08026F5E` | Reads low byte of `ms[0xF64]`; if nonzero, copies it to live `ms[0xF68]` and branches restored states into transport paths. |

`scripts/test_stock_meter_literals.py` now guards both byte slices as
`saved_mode_f64_config_load` and `saved_mode_f64_to_live_f68_restore`.

## Boundary

`ms[0xF64]` is saved mode-init state for the DMM command/transport layer, with
some UI renderer comparison refs. It is not display-only bitmap height, and it
is not physical DMM range state.

Do not use `ms[0xF64]` as:

- a runtime `ms[0x02]`/`ms[0x03]` analog mux/range writer
- exact stock settle/discard evidence
- H2/SPI3 acceptance proof
- a low-DCV correction

The low-DCV blocker still needs a DMM-owned runtime writer or trace for
`ms[0x02]`/`ms[0x03]`, H2/SPI3 acceptance/effect evidence, or a real
factory-calibration source.
