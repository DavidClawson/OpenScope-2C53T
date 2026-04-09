# Missing Image / State Hypothesis Ranking — 2026-04-08

## Scope

This memo ranks the most plausible explanations for the current scope-mode
blocker, with emphasis on:

- the vendor app-slot flash black-screening
- missing high-flash references beyond the downloaded V1.2.0 app file end
- the live W25Q128 dump from the bench unit

The goal is to answer one narrow question:

> Is the remaining blocker more likely to be missing stock image / board
> state than wrong MCU-side command sequencing?

## Bottom line

Yes. The current evidence favors **missing MCU-side image/state** over
**wrong MCU command sequencing**.

The strongest reason is that the downloaded vendor app fails **before**
scope-mode behavior becomes the issue: it launches from our bootloader, then
goes dark, while static analysis shows boot-time filesystem setup depends on
multiple data pointers that are outside the downloaded app image.

The W25Q128 dump weakens one external-state theory:

- the external flash is present
- both FAT12 volumes are valid
- `System file` assets are populated

So the best current explanation is not "the board is missing a filesystem."
It is "the downloaded website app is not a sufficient stand-in for the
programmed stock MCU image/state."

## Ranked explanations

### 1. Missing MCU-side high-flash data/descriptors in the website app image

**Plausibility:** highest

This is the strongest explanation for the stock-app black screen and remains
the best image/state hypothesis overall.

Grounded evidence:

- `high_flash_refs_2026_04_08.txt` shows **65 unique flash targets beyond
  file end**, with the downloaded V1.2.0 app ending at `0x080B7680`.
- `fatfs_init_missing_high_flash_2026_04_08.md` shows that stock
  `fatfs_init()` depends on multiple missing pointers:
  - `0x080BBA1F`
  - `0x080BB927`
  - `0x080BBC58`
  - `0x080BC1B4`
  - `0x080BC18B`
  - `0x080BBA22`
  - `0x080BB92B`
  - `0x080BC1A5`
  - `0x080BC841`
- The bench result is consistent with that: the vendor app flashed cleanly
  into the normal app slot, the bootloader jumped into it, and the unit went
  dark instead of reaching a usable UI.

Why this matters for the scope blocker:

- If boot-time descriptor/config/resource data is absent from the website app,
  then scope failure on our board is not strong evidence that our scope
  command sequence is wrong.
- It is also consistent with earlier contradictions around missing
  high-flash-backed tables and descriptors.

### 2. Missing non-app MCU state below or outside the downloaded app

**Plausibility:** high

This is closely related to item 1, but distinct enough to keep separate.

Grounded evidence:

- `stock_iap_bootloader.md` shows the stock application has **no in-app
  firmware update path**. That implies the real factory programming model
  likely includes state outside the standalone `APP_2C53T_V1.2.0_251015.bin`
  file.
- The same note explicitly treats stock upgrades as likely going through ROM
  DFU or a separate stock bootloader that lives below the app.
- The vendor app successfully passed our app-slot vector validation and
  launched, so the black screen is not explained by "bad raw file format."

Interpretation:

- The programmed stock device may include a companion boot region, option-byte
  state, or higher-flash data region that the standalone website app does not
  reproduce.
- This is a stronger explanation than "our bootloader cannot launch the stock
  app." It did launch it.

### 3. External flash mismatch or missing small per-board metadata

**Plausibility:** medium

External state is still on the table, but the dump weakens the strongest
version of that theory.

Grounded evidence from `w25q128_dump_2026_04_08.md`:

- Volume 0 and Volume 1 are both valid FAT12 filesystems.
- `System file` contains `160` real `.JPG` assets.
- `9999.BIN` exists but is a **true zero-cluster placeholder**:
  - `size = 0`
  - `first cluster = 0`
- Volume 1 directories are present but empty.
- Most flash after the first few MiB appears erased.

What this does and does not prove:

- It **does** argue against "stock app black-screened because the SPI flash
  filesystem is absent."
- It does **not** eliminate smaller external-state problems:
  - a missing per-unit metadata file
  - a stale placeholder that stock expects to populate or repair
  - a mismatch between MCU-side missing descriptors and the actual on-flash
    files they are supposed to address

Related evidence from `external_flash_runtime_path_map_2026_04_08.md`:

- the `0x080BC841` path looks more like an early boot volume/layout probe than
  a direct scope config blob
- the `0x080BC859` family looks like a per-index metadata/resource path, not a
  direct `9999.BIN` consumer

So external flash still matters, but the dump pushes it below MCU-side missing
image/state as the primary explanation.

### 4. Wrong MCU-side scope command sequencing alone

**Plausibility:** medium-low

This is no longer the best top-level explanation.

Why it moved down:

- The stock website app itself dark-screens before normal operation, which is
  upstream of our scope-mode sequencing problem.
- The missing high-flash evidence explains a boot-critical failure mode that
  exists even before scope interaction.
- The external flash dump shows the board is not missing the obvious asset
  volumes needed for basic startup.

This does **not** mean sequencing is solved. It means sequencing is now less
plausible as the **primary** blocker than missing image/state.

### 5. Scope-specific FPGA or board hardware fault

**Plausibility:** low-to-medium

This cannot be ruled out, but current evidence does not make it the leading
theory.

Why it stays lower:

- meter-side FPGA interaction is alive on this board
- the black-screening stock app is better explained by missing image/state
  than by a scope-only hardware fault
- the external flash is readable and structured

This becomes more important only if the image/state hypotheses fail.

## What the current evidence really says

### Strongly supported

- The downloaded V1.2.0 vendor app is likely **not sufficient by itself** to
  represent the programmed stock MCU image.
- The stock app black-screen result is a meaningful signal, not noise.
- The W25Q128 dump shows the board is not simply missing its asset volumes.

### Weakly supported

- `9999.BIN` as the direct boot blocker.
- the idea that a larger hidden external calibration blob is missing from this
  board's W25Q128
- the idea that wrong scope wire words alone are the main remaining gap

## Highest-value next forensic checks

### 1. Sniff the W25Q128 bus during stock-app boot on this board

**Value:** highest

Why:

- It directly tests what the dark-screening stock app tries to open or read
  before failure.
- It does not depend on SWD bypassing read protection.
- It can separate:
  - "app dies before meaningful flash activity"
  - "app probes for a missing file/path/descriptor"
  - "app reads valid files, then fails later for some other reason"

Best target signals:

- SPI flash CS / CLK / IO0 / IO1
- boot attempt starting from our app-slot stock flash experiment

### 2. Get a second unit's W25Q128 dump and compare it byte-for-byte

**Value:** very high

Why:

- MCU flash may remain unreadable, but external flash comparison is still
  feasible.
- A second-unit diff can answer whether our current board's external state is
  unusual even when the website app is held constant.

Primary compare points:

- root directory entries for both FAT volumes
- exact sector containing `9999.BIN`
- any per-index metadata or non-JPG content in `System file`
- boot sectors and FAT chains

### 3. Re-audit the missing high-flash descriptor families as boot-critical or not

**Value:** high

Why:

- The current evidence already makes these descriptors the best MCU-side
  explanation.
- The fastest way to reduce uncertainty is to separate:
  - boot-critical filesystem descriptors
  - scope/runtime resource descriptors
  - display-only strings/assets

Priority families:

- `0x080BC841` boot probe family
- `0x080BC18B` / `0x080BC1B4` / `0x080BC1A5`
- `0x080BB927` / `0x080BB92B` / `0x080BBC58`

### 4. Inspect the exact on-flash sectors around `9999.BIN` and the resource roots

**Value:** medium-high

Why:

- The current dump already shows `9999.BIN` is a zero-cluster placeholder.
- The next useful question is whether nearby sectors contain related metadata,
  alternate names, or repair-created entries that were not obvious from the
  first filesystem pass.

This is lower than bus sniffing because the runtime path currently points more
strongly at missing descriptor indirection than at the literal `9999.BIN`
string.

### 5. Only after 1-4, try synthetic external-flash perturbations

**Value:** medium

Examples:

- populate a sacrificial `9999.BIN`
- create likely missing metadata entries on a copied flash image
- replay a minimal descriptor-matched directory structure if bus sniffing
  exposes concrete filenames

Why this is not first:

- without the bus trace or clearer descriptor recovery, it is still too easy
  to perturb the wrong file and learn nothing

## Practical conclusion

If the question is "what should we spend time on next," the answer is:

1. **treat missing MCU-side image/state as the top hypothesis**
2. **treat external flash mismatch as the next feasible differential check**
3. **stop treating wrong scope command sequencing as the main explanation**

The highest-value near-term experiment is a **stock-app boot capture on the
W25Q128 bus**, followed by a **second-unit W25Q128 comparison**.
