# Missing Image / State Forensic Split — 2026-04-08

## Scope

This note tightens the current image/state ranking using three concrete facts
that were not all integrated in one place before:

1. the downloaded stock app **did** flash into the app slot and **did** launch
   from our bootloader, but the unit then went dark
2. the downloaded V1.2.0 app still has many boot/runtime flash references
   beyond file end
3. the bench unit's W25Q128 dump is valid and populated, but contains only
   obvious FAT volumes plus small/empty metadata candidates

The question here is narrow:

> If the remaining blocker is image/state rather than pure MCU command
> sequencing, what is the most likely missing dependency class right now?

## Bottom line

The strongest current explanation is still:

1. **missing MCU-side high-flash descriptor/data region in the website app**

But the sharper ranking now is:

1. missing MCU-side high-flash descriptor/data region
2. small external-flash metadata mismatch or empty per-index records
3. generic non-app MCU state below/outside the standalone app

The most useful change from the earlier ranking is that the old broad
"non-app MCU state" bucket should move down. The stock app already passed
vector validation, launched, and reached a dark-screen failure *after* our
bootloader handed off. That makes a missing stock bootloader/IAP path or
generic option-byte state less convincing than either:

- missing high-flash data the website app never shipped with, or
- a smaller external metadata mismatch the stock app expected to repair/use

## Ranked hypotheses

### 1. Missing MCU-side high-flash descriptor/data region

**Rank:** highest

This remains the best explanation for the stock-app dark-screen result.

Grounded evidence:

- The downloaded app ends at `0x080B7680`, but
  [high_flash_refs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/high_flash_refs_2026_04_08.txt#L1)
  shows `65` unique targets beyond that end.
- The boot-time FatFs path in
  [fatfs_init_missing_high_flash_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fatfs_init_missing_high_flash_2026_04_08.md#L1)
  depends on missing higher-flash families such as:
  - `0x080BBA1F`
  - `0x080BB927`
  - `0x080BBC58`
  - `0x080BC1B4`
  - `0x080BC18B`
  - `0x080BC1A5`
  - `0x080BC841`
- The website app did not fail vector validation or crash before handoff. It
  launched and then dark-screened. That is consistent with "valid app body,
  missing boot-critical descriptor data," not "bad file format."

Why this stays on top:

- It explains the stock-app boot failure *upstream* of the scope-specific
  problem.
- It matches the specific missing families used by `fatfs_init()` and the
  descriptor-driven resource paths better than any external-only theory.
- It does not require assuming our current clean-room scope sequencing is the
  whole problem.

### 2. Small external-flash metadata mismatch or empty per-index records

**Rank:** second

This moved up relative to the older "non-app MCU state" bucket.

Grounded evidence:

- The W25Q128 dump in
  [w25q128_dump_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/w25q128_dump_2026_04_08.md#L1)
  proves the board already has:
  - two valid FAT12 volumes
  - `160` real JPG assets in `System file`
  - an empty `9999.BIN`
- The runtime path split in
  [external_flash_runtime_path_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/external_flash_runtime_path_map_2026_04_08.md#L1)
  shows a per-index metadata loader around `0x080BC859` that reads:
  - `0x25A` bytes into `0x2000044E`
  - then an extra `5` bytes of small state/config
- On failure, that path calls a create/repair helper rather than simply
  treating the file as optional.

Why this is not first:

- The external flash is present, mountable, and populated enough that the
  website app's dark screen is not well explained by "missing filesystem."
- The strongest missing references we have are still MCU-side higher-flash
  descriptor families, not absent external sectors.

Why it still matters:

- The empty `9999.BIN` placeholder and the create/repair paths mean a
  *small* missing per-board metadata structure is still plausible.
- If the stock app makes it through early descriptor-driven boot and then dies
  while trying to use or repair per-index metadata, this bucket would rise.

### 3. Generic non-app MCU state below/outside the standalone app

**Rank:** third

This is now weaker than the first two.

What still supports it:

- The stock application has no in-app upgrade path, per
  [stock_iap_bootloader.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/stock_iap_bootloader.md#L1),
  so the complete factory-programmed device may still include other MCU-side
  components not represented by the website app.

Why it drops:

- Our board already has the needed option-byte/SRAM posture for our own
  firmware.
- The stock app already launched from our bootloader, so a missing stock
  bootloader/IAP companion is not a sufficient explanation for the dark screen.
- The more concrete and better-grounded missing dependencies are now:
  - higher-flash descriptor data, and
  - smaller external metadata records.

This bucket should stay as a residual explanation, not the first place to
look.

## What would change the ranking

### Evidence that would strengthen hypothesis 1 further

- A stock-app boot attempt that shows very early failure with little or no
  meaningful SPI-flash traversal beyond boot-sector/root checks.
- A trace showing the app never reaches the deeper per-index metadata reads
  implied by the `0x080BC859` family.

### Evidence that would move hypothesis 2 above hypothesis 1

- A trace showing the stock app performs a coherent early descriptor/volume
  traversal, then repeatedly accesses or fails on a specific small metadata
  file/sector.
- A second unit's W25Q128 dump showing nontrivial metadata content where this
  unit has an empty placeholder or erased sectors.

### Evidence that would revive hypothesis 3

- A reproducible indication that the stock app expects a lower/other MCU flash
  region, option-byte state, or SRAM handshake value that our boot path is not
  reproducing, and that this failure happens before any meaningful filesystem
  work.

## Single best forensic next check

### Sniff the W25Q128 bus during a stock-app boot attempt

This is the best next check because it separates the three ranked buckets with
one capture:

- If the app dies before meaningful flash traversal, that supports missing
  MCU-side high-flash descriptor/data.
- If it performs coherent FAT traversal and then stalls on a small metadata
  path, that pushes the external-metadata bucket upward.
- If it barely touches the SPI flash at all and instead fails before storage
  activity, that makes a broader non-app MCU-state issue more plausible.

This is higher value than another command-sequencing experiment because the
website app already dark-screened before scope runtime behavior could even be
evaluated.

## What result would falsify the top hypothesis

The top hypothesis is:

> the website app is missing MCU-side higher-flash descriptor/data needed for
> early boot/runtime filesystem setup

The clearest falsifier would be:

- a stock-app boot capture showing **sustained, structured, and apparently
  successful** W25Q128 access well past the early boot probe phase, including:
  - boot-sector/FAT/root traversal
  - directory/file reads inside the expected populated volume(s)
  - deeper metadata/resource reads consistent with the `0x080BC859` family
- followed by failure **after** those reads, with no sign that boot died at
  the missing-descriptor stage

Why that falsifies it:

- If the website app can already drive the correct early filesystem traversal
  and reach deeper resource/metadata reads on this board, then the missing
  MCU-side descriptor/data theory is no longer the best explanation for the
  dark screen.
- At that point, the ranking should shift toward:
  1. small external metadata mismatch
  2. later runtime/hardware state

## Practical interpretation

Current best read:

- the website app is probably incomplete as a stand-in for the programmed
  stock MCU image
- but the most efficient way to test that claim now is not more static RE
  alone
- it is a stock-app boot capture on the W25Q128 bus, because that tells us
  whether the failure is:
  - before real storage traversal,
  - during metadata/resource lookup,
  - or after both

That is the shortest path to either confirming the top hypothesis or knocking
it down.
