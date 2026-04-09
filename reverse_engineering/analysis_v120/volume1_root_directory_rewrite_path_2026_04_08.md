# Volume 1 Root Directory Rewrite Path 2026-04-08

Purpose:
- explain the heavy SPI2 write family seen in the seeded Unicorn `fatfs_init()`
  runs
- connect the write loop back to concrete stock code in `fatfs_init()`,
  `fatfs_read_write()`, and the low-level SPI2 flash helpers
- decide whether stock is repairing bad metadata or simply replaying a normal
  root-directory normalization path

Key references:
- [fatfs_init @ 0x0802E7BC](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22735)
- [fatfs_read_write @ 0x0802D534](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L20983)
- [FUN_0802F16C](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L23178)
- [unicorn_stock_flash_trace_third_pass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_third_pass_2026_04_08.md#L1)
- [w25q128_dump_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/w25q128_dump_2026_04_08.md#L1)

## 1. Exact Boot-Side Call Chain

Inside [fatfs_init](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22738), once stock mounts and opens `2:/`, it does:

1. `fatfs_read_write(0x080BBC58)`
2. `fatfs_read_write(0x080BC1B4)`
3. `fatfs_read_write(0x080BC18B)`
4. `fatfs_operation()`

That is the whole boot-time Volume 1 root-directory family:
- `0x080BBC58` -> current best synthetic path guess: `2:/LOGO`
- `0x080BC1B4` -> `2:/Screenshot file`
- `0x080BC18B` -> `2:/Screenshot simple file`

So the heavy write loop is not an arbitrary side path. It hangs directly off the
boot-time `2:/` root population sequence.

## 2. What `fatfs_read_write()` Actually Does Here

In [fatfs_read_write](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L20986):

- it resolves the descriptor/path into a live file object via:
  - `FUN_080337A0(...)`
  - `FUN_0802F6D8(...)`
- it computes the target cluster via `FUN_0802A2D4(...)`
- for volume type `2`, it uses flash addresses `cluster * 0x1000 + 0x200000`
- it reads the target 4 KiB sector(s) into the object buffer with
  `FUN_0802F16C(...)`
- then it overwrites the in-buffer FAT directory entry fields:
  - 8.3 short name at `+0x34..+0x3F`
  - attribute byte at `+0x3F = 0x10`
  - first-cluster fields
  - size fields
- finally it marks the buffer dirty and commits through `FUN_08036934(...)`

So this is plainly a directory-entry creation/update path, not a generic blob
writer.

## 3. Low-Level Flash Write Path

The commit path reaches [FUN_0802F16C](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L23178), which:

- reads a full 4 KiB sector with `FUN_0802F048(...)` (`0x03` read)
- conditionally erases a sector with `FUN_0802EE9C(...)` (`0x20`)
- page-programs 256-byte chunks with `FUN_0802F2AC(...)` (`0x02`)

This matches the emulated transaction stream exactly:
- sector erase at `0x207000`
- then page programs at:
  - `0x207000`
  - `0x207100`
  - ...
  - `0x207F00`

So the heavy SPI2 family is a full 4 KiB sector rewrite of the Volume 1 root
directory sector.

## 4. The Rewritten Data Matches The Real Dump

Two strong cross-checks line up:

### Parsed root entries from the real dump at `0x207000`

Direct parse of the current board dump shows:
- `LOGO` at offset `0x000`, attr `0x10`, cluster `10`
- `SCREEN~1` at offset `0x060`, attr `0x10`, cluster `11`
- `SCREEN~2` at offset `0x0C0`, attr `0x16`, cluster `12`

Those match the extracted manifest in
[volume_200000/manifest.json](/Users/david/Desktop/osc/archive/w25q128_extract/volume_200000/manifest.json):
- `LOGO`
- `Screenshot file`
- `Screenshot simple file`

The `SCREEN~1` / `SCREEN~2` names are exactly what a FAT short-name encoder
would produce for the two longer directory names.

### Byte-for-byte compare of the programmed pages

In the emulated `roots_plus_dirs` run, all 16 page-program payloads at
`0x207000..0x207F00` match the existing
[w25q128_dump.bin](/Users/david/Desktop/osc/archive/w25q128_dump.bin)
byte-for-byte.

That means:
- stock is not fixing a mismatch unique to our dump
- it is replaying a deterministic rewrite of the current Volume 1 root
  directory

## 5. Role Of `fatfs_operation()`

After the three `fatfs_read_write(...)` calls, [fatfs_operation](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L20527):

- opens the descriptor family rooted at `0x080BC18B`
- checks attribute bits from the discovered entry
- sets `*(entry + 0x0B) |= 6`
- marks the root object dirty (`*(local_40 + 3) = 1`)
- commits again through `FUN_08036934(...)`

So the boot sequence is not just "create three entries". It also does a
follow-up metadata/state update on the same root-family object.

## 6. What This Means

The heavy seeded-emulator write family is now well explained:

- it is a normal boot-time normalization/replay of the Volume 1 root directory
- it is triggered once the missing high-flash root + directory-root descriptors
  are present
- it is not evidence that our external flash is corrupt there
- it weakens the theory that `9999.BIN` is the direct boot-critical file
- it strengthens the theory that missing MCU-side descriptor data is the real
  gating factor for early boot filesystem behavior

## 7. Best Next Step

The sharpest next software-only move is now:

1. Trace what higher-level condition decides whether this Volume 1 root
   normalization path runs during real boot.
2. Compare that against the stock-app black-screen case.
3. Then follow the next consumer of those normalized root entries, rather than
   guessing more literal filenames.

The important correction is that the directory-root descriptor family is the
main gate. The `0x207000` write loop is a consequence of that gate, not the
primary mystery on its own.
