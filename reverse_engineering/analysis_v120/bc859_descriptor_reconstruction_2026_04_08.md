# BC859 Descriptor Reconstruction 2026-04-08

Purpose:
- test whether the missing high-flash `0x080BC859` family is a real runtime
  gate for the later metadata consumer
- use direct chained Unicorn execution to separate:
  - mounted-volume state from `fatfs_init()`
  - descriptor-string reconstruction
  - later metadata/create-entry behavior

Artifacts:
- tracer:
  [unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
- chained create-entry run:
  [unicorn_fatfs_then_fun_0802e12c_create_1bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_0802e12c_create_1bin_2026_04_08.txt)
- chained metadata run with no overlay string:
  [unicorn_fatfs_then_fun_08035ed4_no_overlay_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_no_overlay_2026_04_08.txt)
- chained metadata run with only `0x080BC859` seeded:
  [unicorn_fatfs_then_fun_08035ed4_bc859_only_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_bc859_only_2026_04_08.txt)
- chained metadata run with the broader overlay family:
  [unicorn_fatfs_then_fun_08035ed4_overlay_probe_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_overlay_probe_2026_04_08.txt)

## 1. Harness Upgrade

The Unicorn tracer now supports:

- direct `r0..r3` argument seeding
- arbitrary seeded bytes / words / C strings in RAM or flash
- a two-stage chained execution model:
  - run first entry point
  - keep RAM/peripheral state
  - jump into a second entry point after the first returns

This was necessary because direct entry to [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)
without a prior `fatfs_init()` run returned before touching SPI2. The function
needs mounted-volume state, not just the path string.

## 2. Baseline: No `0x080BC859` Reconstruction

Run:

- `fatfs_init()`
- then `FUN_08035ED4()`
- with:
  - seeded allocator state
  - `roots_plus_dirs`
  - `DAT_20000F09 = 1`
  - **no** reconstructed `0x080BC859`

Result:

- [unicorn_fatfs_then_fun_08035ed4_no_overlay_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_no_overlay_2026_04_08.txt)
- `64` transactions total
- exactly the same family as the earlier `fatfs_init()` heavy baseline:
  - `0x03`: `13`
  - `0x06`: `17`
  - `0x05`: `17`
  - `0x02`: `16`
  - `0x20`: `1`

Interpretation:

- the later metadata function does **not** progress in any meaningful way
  without the missing `0x080BC859` family

## 3. Reconstructing `0x080BC859` As `%d.bin` Is Enough To Unlock The Path

I then reran the same chained path but seeded only:

- `0x080BC859 = "2:/Screenshot simple file/%d.bin"`

using the already known in-range literal at:

- [fatfs_init_missing_high_flash_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fatfs_init_missing_high_flash_2026_04_08.md#L70)

Result:

- [unicorn_fatfs_then_fun_08035ed4_bc859_only_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_bc859_only_2026_04_08.txt)
- `330` transactions total
- opcode mix:
  - `0x03`: `24`
  - `0x06`: `102`
  - `0x05`: `102`
  - `0x02`: `96`
  - `0x20`: `6`

New read addresses beyond the baseline include:

- `0x213000`
- `0x201000`
- `0x214000`
- `0x203000`

Interpretation:

- the metadata consumer is no longer stalling at the missing descriptor text
- it reaches a much deeper create/repair path on the real dump
- the `%d.bin` reconstruction is not just plausible; it is sufficient to unlock
  the later runtime behavior in this harness

## 4. Broader Overlay Seeding Is Not Needed For This Consumer

For completeness, I also ran the chained metadata path with a broader synthetic
overlay family:

- `0x080BC859 = "2:/Screenshot simple file/%d.bin"`
- `0x080BCAD2 = "2:/Screenshot file/%d.bmp"`
- `0x080BCAE5 = "%d.bmp"`

Result:

- [unicorn_fatfs_then_fun_08035ed4_overlay_probe_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08035ed4_overlay_probe_2026_04_08.txt)
- same `330`-transaction family as the `BC859-only` run

So for the later metadata consumer:

- `0x080BC859` is the important unlock
- `0x080BCAD2` and `0x080BCAE5` are not needed to push this path forward

That makes sense against the code:

- [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)

which formats only `0x080BC859` directly.

## 5. Create-Entry Sanity Check

I also ran:

- `fatfs_init()`
- then direct `FUN_0802E12C()`
- with:
  - `string_format_buffer = "2:/Screenshot simple file/1.bin"`

Result:

- [unicorn_fatfs_then_fun_0802e12c_create_1bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_0802e12c_create_1bin_2026_04_08.txt)
- `68` transactions total
- baseline `64` plus extra reads including:
  - `0x205000`
  - `0x206000`
  - `0x207000`
  - `0x213000`

Interpretation:

- the chained direct-create path is real
- it begins traversing the screenshot-simple subtree when given a concrete
  `%d.bin` path
- so the harness is now strong enough to test individual reconstructed
  descriptor families directly

## 6. Best Current Read

This is the strongest software-only descriptor result so far:

- `0x080BC859` very likely corresponds to the missing family
  `"2:/Screenshot simple file/%d.bin"`
- that family is a real runtime gate for the later metadata consumer
- once mounted state exists, reconstructing `0x080BC859` alone is enough to
  unlock a much deeper create/repair path on the real dump

So the uncertainty is shrinking again:

- for `FUN_08035ED4()`, the missing descriptor family is no longer vague
- it is now strongly pinned to the `%d.bin` metadata path

## 7. Best Next Step

The next highest-value software-only move is:

1. persist SPI2 page-program / erase effects into the in-memory dump model
2. chain:
   - `fatfs_init()`
   - `FUN_0802E12C()` create path
   - `FUN_08035ED4()` read path
3. see whether the created `1.bin` artifact becomes readable as a `0x25A + 5`
   metadata blob in the same emulated session

That would turn the current descriptor-string reconstruction into a full
metadata lifecycle test.
