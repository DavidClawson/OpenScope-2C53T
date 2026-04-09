# Unicorn Stock Flash Trace Third Pass 2026-04-08

Purpose:
- reduce the seeded high-flash table to smaller subsets
- identify which missing descriptor families actually gate direct `fatfs_init()`
- test whether the `0x080BC841` probe behaves like a semantically important
  path or merely like a generic non-null string slot

Artifacts:
- tracer:
  [unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
- second-pass baseline:
  [unicorn_stock_flash_trace_second_pass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_second_pass_2026_04_08.md#L1)
- saved ablation traces:
  - [unicorn_fatfs_init_trace_roots_only_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_only_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_roots_plus_dirs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_plus_dirs_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_roots_dirs_logo_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_logo_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_roots_dirs_probe_9999_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_probe_9999_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_roots_dirs_logo_long_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_logo_long_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_roots_dirs_probe_9999_long_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_probe_9999_long_2026_04_08.txt)
- saved probe-override traces:
  - [unicorn_fatfs_init_trace_probe_9999_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_9999_bin_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_probe_1_jpg_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_1_jpg_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_probe_ss_1_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_ss_1_bin_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_probe_bogus_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_bogus_bin_2026_04_08.txt)
  - [unicorn_fatfs_init_trace_probe_empty_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_empty_2026_04_08.txt)

## Correction To The Second Pass

The second-pass note correctly identified high-flash descriptor seeding as a
real gate, but it overstated the importance of the guessed
`0x080BC841 = "3:/System file/9999.bin"` path.

The third-pass ablations, plus the later long reruns, show:
- the root strings and directory-root strings matter more than the specific
  `9999.bin` probe text
- the apparent short `2:/LOGO` split in the first third-pass run was mostly an
  instruction-budget artifact
- once the directory roots are seeded, the direct `0x080BC841` path content is
  not semantically discriminating in this harness

## A/B Split Results

All runs below used:
- direct entry to `fatfs_init()`
- seeded allocator block at `0x20001070`
- the real [w25q128_dump.bin](/Users/david/Desktop/osc/archive/w25q128_dump.bin)

### 1. Roots Only

Seeded strings:
- `2:`
- `3:`
- `2:/`
- `3:/`

Result from [unicorn_fatfs_init_trace_roots_only_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_only_2026_04_08.txt):
- `2` SPI2 transactions total
- both are `0x03` reads
- addresses:
  - `0x200000`
  - `0x000000`
- no write-enable, erase, status, or page-program traffic

Interpretation:
- the drive/root strings alone are enough to make stock touch both FAT volume
  boot sectors
- they are not enough to trigger the later metadata-repair path

### 2. Roots Plus Directory Roots

Added strings:
- `2:/Screenshot simple file`
- `3:/System file`
- `2:/Screenshot file`

Result from [unicorn_fatfs_init_trace_roots_plus_dirs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_plus_dirs_2026_04_08.txt):
- `64` SPI2 transactions
- opcode mix:
  - `0x03`: `13`
  - `0x06`: `17`
  - `0x05`: `17`
  - `0x02`: `16`
  - `0x20`: `1`
- early reads include:
  - `0x200000`
  - `0x205000`
  - `0x206000`
  - `0x207000`
  - later `0x000000`
  - later `0x003000`
- write/repair traffic includes:
  - `0x20 20 70 00` sector erase at `0x207000`
  - repeated `0x02 20 7x 00 ...` page-program writes
  - the first nonzero page-program payload at `0x207000` begins with literal
    `LOGO` and fits a FAT directory-entry shape, which strongly suggests stock
    is synthesizing or repairing the Volume 1 root directory rather than
    writing opaque calibration data
  - a byte-for-byte compare against the real
    [w25q128_dump.bin](/Users/david/Desktop/osc/archive/w25q128_dump.bin)
    shows all `16` programmed pages at `0x207000..0x207F00` already match the
    dumped flash exactly, so this looks more like a normal root-directory
    rewrite/normalization path than a mismatch-driven repair

Interpretation:
- the directory-root descriptors are the real switch that unlock the deeper
  flash traversal and the metadata-style repair/write family
- this is the strongest current evidence that the missing high-flash directory
  descriptor family is genuinely boot-relevant

### 3. Roots Plus Directory Roots Plus `2:/LOGO`

Added string:
- `2:/LOGO`

Initial result from [unicorn_fatfs_init_trace_roots_dirs_logo_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_logo_2026_04_08.txt):
- `18` SPI2 transactions
- opcode mix:
  - `0x03`: `14`
  - `0x06`: `2`
  - `0x05`: `1`
  - `0x20`: `1`
- stop point:
  - `0x0802F108`

Long-rerun correction from [unicorn_fatfs_init_trace_roots_dirs_logo_long_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_logo_long_2026_04_08.txt):
- `67` transactions
- opcode mix:
  - `0x03`: `16`
  - `0x06`: `17`
  - `0x05`: `17`
  - `0x02`: `16`
  - `0x20`: `1`
- clean return to `0x08FFFFF0`

Interpretation:
- the short 18-transaction result was not a real semantic split; it was a
  clipped run
- with enough instruction budget, the `LOGO` case falls into the same heavy
  repair family as the directory-root-only case
- the real effect of adding `LOGO` is just a small amount of extra traversal
  on top of the same page-program loop

### 4. Roots Plus Directory Roots Plus `0x080BC841` Probe

Without `LOGO`, adding the guessed `0x080BC841` probe back in gives the same
overall family as the directory-root case:
- [unicorn_fatfs_init_trace_roots_dirs_probe_9999_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_probe_9999_2026_04_08.txt)
- `65` transactions
- same `0x03 / 0x06 / 0x05 / 0x02 / 0x20` mix
- slightly different stop point (`0x08036840`) and one extra short read

Long-rerun correction from [unicorn_fatfs_init_trace_roots_dirs_probe_9999_long_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_roots_dirs_probe_9999_long_2026_04_08.txt):
- `67` transactions
- same heavy opcode mix as the long `LOGO` case
- clean return to `0x08FFFFF0`

Interpretation:
- the `0x080BC841` family can perturb the later traversal slightly
- but it is not the primary unlock for the write/repair family
- like `LOGO`, it ends up in the same heavy repair path once the directory-root
  descriptors are present

## Probe-Override Sweep

I then kept the successful `roots_plus_dirs` base and varied only the
`0x080BC841` string.

Tested probes:
- `3:/System file/9999.bin`
- `3:/System file/1.jpg`
- `3:/System file/36.jpg`
- `2:/Screenshot simple file/1.bin`
- `2:/Screenshot file/1.bmp`
- `3:/NOPE/ghost.bin`
- `3:/x`
- `3:/`
- empty string

Result:
- every one of those probe overrides converged to the same heavy family
- all produced either `64` or `65` transactions with the same
  `0x03 / 0x06 / 0x05 / 0x02 / 0x20` opcode pattern

The saved traces make this reproducible:
- [unicorn_fatfs_init_trace_probe_9999_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_9999_bin_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_1_jpg_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_1_jpg_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_36_jpg_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_36_jpg_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_ss_1_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_ss_1_bin_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_sf_1_bmp_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_sf_1_bmp_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_bogus_bin_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_bogus_bin_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_short_x_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_short_x_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_root_only_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_root_only_2026_04_08.txt)
- [unicorn_fatfs_init_trace_probe_empty_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_probe_empty_2026_04_08.txt)

Interpretation:
- in this harness, `0x080BC841` currently behaves more like a "descriptor slot
  exists" condition than a path with meaningful file-class semantics
- the content of that slot is much less important than the presence of the
  root and directory-root families
- the same caution now applies to `LOGO`: it is not the main branch selector we
  briefly thought it was

## What This Means Now

The software-only picture is stronger and narrower now:
- missing high-flash descriptor data is still a real blocker
- the most important seeded family is the volume root + directory-root set
- the old "maybe `9999.bin` is the boot-critical trigger" theory is weaker
- neither `2:/LOGO` nor `0x080BC841` is the main discriminator once the
  directory roots are present; they only add small traversal differences on top
  of the same heavy repair family
- the heavy family itself now looks like a deterministic rewrite of the Volume
  1 root-directory sector, not evidence that the current dump is corrupt there

This also improves the interpretation of the stock-app black screen:
- it still looks like a missing MCU-side descriptor/resource problem
- but it no longer looks specifically like "stock needs a valid `9999.bin`"
- the external-flash dump and the emulator now agree that `9999.BIN` is more
  likely a side path or placeholder than the main boot gate
- the heaviest emulated write path now looks like FAT root-directory synthesis
  for Volume 1, not like arbitrary flash mutation

## Best Next Step

Best next software-only move:
- trace the directory-root driven repair path itself rather than overfocusing on
  `0x080BBC58` or `0x080BC841`
- and use the now-confirmed `0x207000..0x207FFF` root-directory rewrite path to
  identify which helper is replaying existing metadata and why

That is a better next target than continuing to guess specific `0x080BC841`
filenames or treating `LOGO` as a unique branch switch.
