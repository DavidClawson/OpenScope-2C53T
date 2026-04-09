# High-Flash Ghidra Workflow — 2026-04-08

This is the concrete execution order for the targeted high-flash RE pass.

Use it together with:

- `high_flash_refs_2026_04_08.txt`
- `high_flash_targets_prioritized_2026_04_08.md`
- `scripts/run_high_flash_ghidra_passes.sh`

## Why this exists

The stock V1.2.0 app blob appears to be incomplete relative to the flash
layout expected by the code. We do **not** want another broad Ghidra sweep.

We want:

1. a ROM block sanity check
2. direct scope-command lookup recovery
3. missing resource/config name recovery

## Environment

Project:

- `/Users/david/Desktop/osc/ghidra_project/FNIRSI_2C53T`

Scripts:

- `/Users/david/Desktop/osc/reverse_engineering/ghidra_scripts`

Headless binary on this machine:

- `/opt/homebrew/Cellar/ghidra/12.0.4/libexec/support/analyzeHeadless`

## Important caveat

Ghidra caches compiled Java script classes under:

- `~/Library/ghidra/ghidra_12.0.4_PUBLIC/osgi/compiled-bundles`

If those were compiled under a newer JDK, headless runs can fail with
`UnsupportedClassVersionError`.

The wrapper script clears that cache before each pass. Use the wrapper
instead of hand-running `analyzeHeadless` unless you specifically want to
debug the headless environment.

## One-command workflow

Run everything:

```bash
cd /Users/david/Desktop/osc
./scripts/run_high_flash_ghidra_passes.sh all
```

Or run in stages:

```bash
cd /Users/david/Desktop/osc
./scripts/run_high_flash_ghidra_passes.sh memory
./scripts/run_high_flash_ghidra_passes.sh pass1
./scripts/run_high_flash_ghidra_passes.sh pass1-force
./scripts/run_high_flash_ghidra_passes.sh pass2
./scripts/run_high_flash_ghidra_passes.sh pass2-force
```

## Outputs

### Memory map sanity

- `analysis_v120/ghidra_memory_blocks_2026_04_08.txt`

Use this first to confirm whether the imported ROM really stops at
`0x080B7680` and whether any extra overlay or synthetic block already
exists above it.

### Pass 1: direct scope lookup paths

- `analysis_v120/high_flash_pass1_scope_tx_08004200_08004920.c`
- `analysis_v120/high_flash_pass1_scope_table_0800C000_0800C120.c`
- `analysis_v120/high_flash_pass1_scope_table_08014140_08014190.c`
- `analysis_v120/high_flash_pass1_scope_decode_0802A490_0802A4E0.c`
- `analysis_v120/high_flash_pass1_scope_decode_08033720_08033750.c`
- `analysis_v120/high_flash_pass1_force_scope_gap_functions.c`

Primary targets:

- `0x080BB3FC`
- `0x080BB40C`
- `0x080BB3EC`
- `0x080BB404`

Questions to answer:

1. What state fields index these tables?
2. Which internal scope commands depend on them?
3. Do they map directly to raw UART low bytes, trigger/range policy, or
   field-decode helpers?

Important note:

- On the current project, several of these addresses live in unnamed gap
  regions rather than clean Ghidra functions.
- The `pass1` range outputs still matter for bookkeeping, but the real
  decompile artifact is often `high_flash_pass1_force_scope_gap_functions.c`.

### Pass 2: missing filesystem/resource names

- `analysis_v120/high_flash_pass2_scope_resource_0802CFC0_0802D020.c`
- `analysis_v120/high_flash_pass2_scope_resource_0802E800_0802E8B0.c`
- `analysis_v120/high_flash_pass2_scope_resource_080157A0_08015830.c`
- `analysis_v120/high_flash_pass2_scope_resource_0800B100_0800B150.c`
- `analysis_v120/high_flash_pass2_force_resource_gap_functions.c`

Primary targets:

- `0x080BC18B`
- `0x080BC1B4`
- `0x080BB927`
- `0x080BBC58`
- `0x080BB92B`
- `0x080BC1A5`

Questions to answer:

1. Are these file paths, resource names, or compact config tables?
2. Are they used to open something from SPI flash before scope entry can
   complete?
3. Do they match any artifacts we should look for in a `W25Q128` dump?

## Manual GUI follow-up

After the memory-block dump:

1. Open the project in Ghidra GUI.
2. Check `Window -> Memory Map`.
3. Confirm the loaded ROM block end.
4. If needed, add an uninitialized read-only block above `0x080B7680`
   purely so the missing targets resolve cleanly as addresses during
   exploration.

Do **not** mark unknown bytes as initialized data unless we get a real dump.

## Not worth doing right now

- `ComprehensiveAnalysis.java`
- `DecompileAll.java`
- broad string sweeps
- blind raw-frame bench probing without new stock data

## Expected next deliverable

After these passes, the next useful note should be a narrow synthesis such
as:

- which direct scope tables are definitely missing
- which missing names likely refer to external SPI-flash resources
- whether the stock downloaded app is missing only data, or also expected
  filesystem-side companions
