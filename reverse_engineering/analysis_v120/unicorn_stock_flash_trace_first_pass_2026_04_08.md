# Unicorn Stock Flash Trace First Pass 2026-04-08

Purpose:
- document the first software-only attempt to trace stock-app external-flash
  traffic without a physical logic analyzer
- record what already works and what still blocks deeper traversal

Artifacts:
- tracer script:
  [unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
- first report:
  [unicorn_stock_flash_trace_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_2026_04_08.txt)

## What works

The new Unicorn tracer now:
- executes stock `master_init`
- backs SPI2 with the real board dump at
  [w25q128_dump.bin](/Users/david/Desktop/osc/archive/w25q128_dump.bin)
- tracks flash-CS on `PB12`
- parses basic SPI flash transactions
- logs early SPI2 commands seen during init

The first observed transaction is:
- opcode `0x90`
- MOSI: `90 00 00 00 FF FF`

That matches the already-documented stock flash-ID path:
- `spi2_read_jedec_id` uses `0x90 + 3 dummy bytes + 2 ID bytes`

Grounding:
- [function_names.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/function_names.md#L180)

## Current limit

The first pass does not yet reach real `0x03` block-read traffic.

Observed stop state:
- `instructions = 5,000,001`
- `stop_pc = 0x08024638`
- only one completed SPI2 transaction logged

The stop PC lands inside the early init body:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L1378)

So the current result is:
- the software-only trace path is viable
- but current emulator fidelity is still insufficient to carry boot far enough
  to observe saved-settings reads, FAT traversal, or deeper metadata/resource
  access

## Practical interpretation

This is still useful because it gives us a no-hardware path worth extending.

The best next improvements are:
- improve the Unicorn environment around the code path that stalls near
  `0x08024638`
- keep extending the SPI2 model only as needed
- treat this as the best current fallback to a physical W25Q128 boot sniff

## Recommendation

If physical probing is blocked by tiny pins or missing hardware, the next best
software-only branch is to keep iterating on
[unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
until it reaches real `0x03` block reads from the dumped flash image.
