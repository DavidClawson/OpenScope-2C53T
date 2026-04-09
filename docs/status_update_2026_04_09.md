# Status Update 2026-04-09

This update captures the most important result from today's software-only
reverse-engineering work on the stock external-flash path.

## Main Result

We now have a strong emulator-backed proof that the missing high-flash
descriptor family at `0x080BC859` is:

- `2:/Screenshot simple file/%d.bin`

and that it is a real runtime gate for the later metadata consumer
`FUN_08035ED4()`.

## What Changed Today

- The Unicorn SPI2 flash model now persists:
  - `0x20` sector erase
  - `0x02` page program
- The stock tracer now supports:
  - a third chained entry point
  - final `R0` reporting
  - optional saving of the mutated in-memory flash image

Primary script:

- [`scripts/unicorn_stock_flash_trace.py`](../scripts/unicorn_stock_flash_trace.py)

## Positive And Negative Controls

Negative control:

- mounted `fatfs_init()` state
- no `0x080BC859`
- `FUN_08035ED4()` returns `R0 = 2`
- trace stays at the old 64-transaction baseline

Artifact:

- [`unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt`](../reverse_engineering/analysis_v120/unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt)

Positive control:

- mounted `fatfs_init()` state
- `0x080BC859 = "2:/Screenshot simple file/%d.bin"`
- `FUN_08035ED4()` returns `R0 = 0`
- trace expands to the deep 330-transaction create/read path

Artifact:

- [`unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt`](../reverse_engineering/analysis_v120/unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt)

## Concrete Runtime Artifact

The successful `%d.bin` path is no longer just “more SPI activity”.
The mutated runtime FAT image contains a new file:

- `Screenshot simple file/1.BIN`

with:

- size `607` bytes
- exact shape `0x25A + 5`
- zero-filled content

That matches the read pattern inside `FUN_08035ED4()`:

- read `0x25A` bytes into `0x2000044E`
- then read a trailing 5-byte side record

The explicit three-stage lifecycle run is saved here:

- [`unicorn_fatfs_create_then_read_1bin_2026_04_09.txt`](../reverse_engineering/analysis_v120/unicorn_fatfs_create_then_read_1bin_2026_04_09.txt)

## Neighbor Check

The adjacent BMP-side family is still not enough by itself:

- `0x080BCAD2 = "2:/Screenshot file/%d.bmp"`
- `0x080BCAE5 = "%d.bmp"`

When seeded for `FUN_08036084()`, the trace stays flat at the old baseline and
returns `R0 = 1`.

Artifact:

- [`unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt`](../reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt)

## Why This Matters

This is the strongest software-only result so far on the missing stock-state
question:

- at least one important missing high-flash family is descriptor text, not
  hidden executable logic
- the stock runtime can synthesize a concrete external metadata file once that
  descriptor family exists
- the remaining black-screen gap now looks even more like “multiple missing
  descriptor/data families” than “mystery code path we have not traced”

## Next Step

Keep pushing the descriptor/data branch:

1. reconstruct the remaining neighboring descriptor families around the preview
   and overlay paths
2. use the persistent-write harness to test which ones produce concrete runtime
   artifacts
3. treat missing high-flash data as the leading stock-app blocker unless a new
   control disproves it
