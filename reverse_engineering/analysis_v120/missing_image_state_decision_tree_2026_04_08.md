# Missing Image / State Decision Tree — 2026-04-08

## Scope

This note reduces the current missing-image/state branch to a falsifiable
decision tree tied to three concrete facts:

1. the downloaded stock app flashed into the app slot, launched from our
   bootloader, and then dark-screened
2. the downloaded app still references many higher-flash targets beyond file
   end, including boot-time FatFs families
3. the bench unit's W25Q128 is present, mountable, and populated with normal
   FAT volumes plus only small/empty metadata candidates

The goal is practical:

> define the top hypothesis, the single next check that would falsify it, the
> result that would instead strengthen the second-ranked hypothesis, and what
> we should do next in each case

## Top hypothesis

### Missing MCU-side high-flash descriptor/data region in the website app

This is still the best current explanation for the stock-app black screen.

Why it ranks first:

- the website app ends at `0x080B7680`, but
  [high_flash_refs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_refs_2026_04_08.txt#L1)
  shows `65` unique targets beyond that end
- the boot-time FatFs path in
  [fatfs_init_missing_high_flash_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fatfs_init_missing_high_flash_2026_04_08.md#L1)
  depends on missing higher-flash families such as:
  - `0x080BBA1F`
  - `0x080BB927`
  - `0x080BBC58`
  - `0x080BC1B4`
  - `0x080BC18B`
  - `0x080BC1A5`
  - `0x080BC841`
- the app did not fail vector validation or raw launch; it got far enough to
  hand off and then dark-screen, which is consistent with "valid app body,
  missing boot-critical descriptor data"

## Second-ranked hypothesis

### Small external-flash metadata mismatch or empty per-index records

This is the best alternative if the top hypothesis weakens.

Why it ranks second:

- the W25Q128 dump in
  [w25q128_dump_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/w25q128_dump_2026_04_08.md#L1)
  shows the board already has valid FAT volumes and `160` real JPG assets, so
  the problem is not "missing filesystem"
- but it also shows an empty `9999.BIN`, and the runtime path map in
  [external_flash_runtime_path_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/external_flash_runtime_path_map_2026_04_08.md#L1)
  points to small per-index metadata reads and create/repair logic around the
  `0x080BC859` family

## Single next check

### Sniff the W25Q128 bus during a stock-app boot attempt

This is the single best next check because it separates the two leading
branches with one observation.

What to capture:

- W25Q128 `CS`, `CLK`, `IO0`, `IO1`
- one clean boot attempt of the flashed stock app from the app slot

What matters:

- whether the app dies before meaningful storage traversal
- whether it performs coherent FAT traversal first
- whether it reaches deeper metadata/resource reads before failure

## Falsifier for the top hypothesis

The top hypothesis is:

> the website app is missing MCU-side higher-flash descriptor/data needed for
> early boot/runtime filesystem setup

The clearest falsifier is:

- a stock-app boot capture showing **sustained, structured, apparently
  successful** flash activity well past the early probe phase, including:
  - boot-sector/FAT/root traversal
  - directory/file reads inside the populated volumes
  - deeper metadata/resource reads consistent with the `0x080BC859` family
- followed by failure only **after** that storage work is already happening

Why that falsifies it:

- if the app can already drive the expected early storage traversal on this
  board, then the missing higher-flash descriptor/data region is no longer the
  best explanation for the dark screen

## Result that would strengthen the second-ranked hypothesis

The second-ranked hypothesis gets stronger if the capture shows:

- coherent early volume traversal that does **not** die at the first boot
  descriptor/probe stage
- then repeated accesses, retries, or stalls around one small metadata path,
  record family, or repair attempt
- especially if activity looks consistent with the per-index metadata loader /
  creator path around `0x080BC859`

That would say:

- the app is getting through early boot storage logic
- the failure is more likely a small missing/mismatched external record than a
  missing MCU-side high-flash descriptor block

## Practical branch after the capture

### Branch A: capture falsifies the top hypothesis

Observed pattern:

- structured early storage traversal succeeds
- deeper metadata/resource reads happen
- failure comes later

Practical next move:

1. pivot from high-flash boot descriptors to external metadata comparison
2. compare this unit's W25Q128 against another unit if available
3. inspect the exact sectors/files implicated by the trace
4. deprioritize more app-image/high-flash speculation until that metadata path
   is explained

### Branch B: capture strengthens the second-ranked hypothesis

Observed pattern:

- early traversal is coherent
- failure clusters around a small metadata/read-repair path

Practical next move:

1. extract or reconstruct the implicated metadata file/sector
2. compare placeholder entries like `9999.BIN` and nearby sectors against a
   second unit
3. instrument our firmware to read the same small record path directly

### Branch C: capture strengthens the top hypothesis instead

Observed pattern:

- very early failure
- little or no meaningful storage traversal beyond boot-sector/root checks
- no convincing deeper metadata/resource reads

Practical next move:

1. stop spending time on small external metadata first
2. treat missing MCU-side high-flash descriptor/data as the primary blocker
3. focus RE on the boot-critical missing families:
   - `0x080BC841`
   - `0x080BC18B`
   - `0x080BC1B4`
   - `0x080BC1A5`
   - `0x080BB927`
   - `0x080BBC58`
4. treat additional command-sequencing experiments as lower priority until the
   image/state gap is resolved

## Working conclusion

Current best read:

- top hypothesis: missing MCU-side high-flash descriptor/data in the website
  app
- best falsifier: a stock-app boot trace showing successful early storage
  traversal and later failure
- result that would strengthen the second-ranked branch: repeated failure on a
  small per-index metadata path after early traversal already works

So the next move is not another scope command sweep. It is one W25Q128 boot
capture that forces the missing-image/state branch into a concrete fork.
