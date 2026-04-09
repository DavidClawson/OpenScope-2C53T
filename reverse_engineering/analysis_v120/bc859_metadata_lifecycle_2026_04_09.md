# BC859 Metadata Lifecycle 2026-04-09

Purpose:
- extend the Unicorn SPI2 model so erase/page-program effects persist in the
  in-memory flash image
- rerun the `%d.bin` metadata path with a true positive and negative control
- inspect the resulting FAT image instead of reasoning only from SPI
  transactions

Artifacts:
- tracer:
  [unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
- negative control trace:
  [unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt)
- positive control trace:
  [unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt)
- three-stage lifecycle trace:
  [unicorn_fatfs_create_then_read_1bin_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_create_then_read_1bin_2026_04_09.txt)
- mutated runtime dump:
  [w25q128_bc859_runtime.bin](/Users/david/Desktop/osc/archive/w25q128_bc859_runtime.bin)
- extracted runtime dump:
  [w25q128_bc859_runtime_extract](/Users/david/Desktop/osc/archive/w25q128_bc859_runtime_extract)

## 1. Harness Upgrade

The Unicorn tracer now:

- keeps the SPI2 dump as a mutable `bytearray`
- applies `0x20` sector erase effects to the in-memory flash image
- applies `0x02` page-program effects to the in-memory flash image
- supports a third chained entry point
- reports the final `R0` return value from the last stage
- can optionally save the mutated runtime flash image with `--dump-out`

This matters because the earlier `%d.bin` result only proved that the missing
descriptor family unlocked a deeper path. It did **not** yet prove what was
being created on external flash.

## 2. Negative Control: No `0x080BC859`

Run:

- `fatfs_init()`
- then `FUN_08035ED4()`
- with:
  - mounted allocator/bootstrap state
  - root + directory-root descriptor families
  - `DAT_20000F09 = 1`
  - **no** `0x080BC859`

Result:

- [unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_read_no_bc859_persistent_2026_04_09.txt)
- `stop_r0 = 0x00000002`
- `64` transactions total
- only the old baseline root rewrite remains:
  - `0x03`: `13`
  - `0x06`: `17`
  - `0x05`: `17`
  - `0x02`: `16`
  - `0x20`: `1`

Interpretation:

- without the `%d.bin` descriptor family, the metadata reader does not reach
  the deeper create/read behavior
- the new write-persistence model by itself is **not** enough to unlock the
  path

## 3. Positive Control: `0x080BC859` Present

Run:

- `fatfs_init()`
- then `FUN_08035ED4()`
- with:
  - the same mounted allocator/bootstrap state
  - the same root + directory-root descriptor families
  - `DAT_20000F09 = 1`
  - `0x080BC859 = "2:/Screenshot simple file/%d.bin"`

Result:

- [unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_read_bc859_persistent_2026_04_09.txt)
- `stop_r0 = 0x00000000`
- `330` transactions total
- `96` page programs
- `6` sector erases
- `24` real read transactions

Interpretation:

- the `%d.bin` descriptor family remains the real runtime gate
- once it is present, the function reaches the success return path
- the success is not just "more traffic happened"; the final return value now
  matches the good branch in
  [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)

## 4. Three-Stage Lifecycle Check

I also ran the explicit three-stage chain:

1. `fatfs_init()`
2. `FUN_0802E12C()` with `string_format_buffer = "2:/Screenshot simple file/1.bin"`
3. `FUN_08035ED4()`

Result:

- [unicorn_fatfs_create_then_read_1bin_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_create_then_read_1bin_2026_04_09.txt)
- `stop_r0 = 0x00000000`
- `334` transactions total

This adds a few extra traversal reads compared with the simpler positive
control, but it does **not** change the main conclusion. The positive control
already reaches the success path as soon as the `%d.bin` descriptor family is
available.

## 5. Concrete FAT Result

The mutated runtime dump was saved at:

- [w25q128_bc859_runtime.bin](/Users/david/Desktop/osc/archive/w25q128_bc859_runtime.bin)

and extracted with:

- [extract_spi_flash_fat.py](/Users/david/Desktop/osc/scripts/extract_spi_flash_fat.py)

The extracted Volume 1 manifest now contains a new file that is absent from the
real dump:

- `Screenshot simple file/1.BIN`

Specifically:

- directory: `Screenshot simple file`
- cluster: `13`
- size: `607` bytes

The original extracted Volume 1 manifest had only:

- `LOGO`
- `Screenshot file`
- `Screenshot simple file`

with no files under `Screenshot simple file`.

## 6. The Created File Matches The Expected `0x25A + 5` Shape

The new file:

- [1.BIN](/Users/david/Desktop/osc/archive/w25q128_bc859_runtime_extract/volume_200000/Screenshot%20simple%20file/1.BIN)

has:

- size `607` bytes
- decimal `607 = 0x25A + 5`

and its content is currently all zero bytes.

That shape matches the reader logic in
[FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29648):

- first read `0x25A` bytes into `0x2000044E`
- then read a trailing `5`-byte side record

So the current best interpretation is:

- `FUN_08035ED4()` is not just probing a hypothetical metadata file
- the `%d.bin` family really does back a concrete runtime metadata artifact on
  Volume 1
- when the descriptor family exists, stock can create a zero-initialized
  `0x25A + 5` placeholder and then proceed through the success branch

## 7. Best Current Read

This is the strongest software-only result so far on the missing high-flash
descriptor side:

- `0x080BC859 = "2:/Screenshot simple file/%d.bin"` is now supported by:
  - the negative control
  - the positive control
  - the saved runtime FAT image
  - the concrete created file shape

This also narrows the larger hypothesis:

- at least one important missing high-flash family is **descriptor text**, not
  hidden executable logic
- the runtime code can synthesize the external metadata file once the missing
  descriptor family exists
- the remaining stock-app black-screen gap is now even more likely to be other
  missing descriptor/data families such as `0x080BCAD2`, `0x080BCAE5`, and
  related overlay/resource strings, not a totally separate filesystem model

## 8. Best Next Step

The next highest-value software-only move is:

1. reconstruct and test the neighboring `0x080BCAD2 / 0x080BCAE5` screenshot /
   preview descriptor family with the same persistent-write harness
2. save and extract the mutated runtime dump again
3. compare whether the BMP-side resource path creates concrete files or only
   directory/label metadata

That is now the cleanest continuation of the descriptor-reconstruction branch.

## 9. Neighbor Check: `0x080BCAD2 / 0x080BCAE5` Do Not Unlock `FUN_08036084()` By Themselves

I ran one immediate follow-up on the adjacent preview/resource family:

- `fatfs_init()`
- then `FUN_08036084()`
- with:
  - root + directory-root descriptors
  - `0x080BCAD2 = "2:/Screenshot file/%d.bmp"`
  - `0x080BCAE5 = "%d.bmp"`
  - `DAT_20000F09 = 1`

Artifact:

- [unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt)

Result:

- `stop_r0 = 0x00000001`
- `64` transactions total
- no traversal beyond the baseline Volume 1 root rewrite

Interpretation:

- the neighboring BMP-side descriptor family is **not** sufficient by itself to
  unlock a deeper runtime path
- `0x080BC859` is currently unique among the tested missing families in being
  able to drive a concrete new FAT artifact by descriptor seeding alone
- the BMP-side preview/resource path probably still depends on additional
  state or neighboring descriptors above simple string reconstruction
