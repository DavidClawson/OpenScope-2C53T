# High-Flash Targets To Inspect First — 2026-04-08

This note turns the raw missing-address dump in
`high_flash_refs_2026_04_08.txt` into a practical Ghidra queue.

The goal is not "inspect every missing `0x080Bxxxx` address." The goal is
to inspect the ones most likely to explain why scope mode never fully arms
on the real board.

## Short version

Top priority is still the `0x080BB3FC` family.

That is the only missing region we have seen so far that is:

- read directly by scope-command builders
- converted immediately into raw `0x0500 | low_byte` TX words
- already linked to our failed scope wake experiments on hardware

Several other missing high-flash targets are real, but many of them now
look like UI strings, filesystem names, or display assets rather than
scope-protocol material.

## Ranking criteria

Targets were ranked higher when they are used by:

1. direct raw FPGA command builders
2. scope-state / trigger / range logic
3. likely config-file lookup paths that could gate scope entry

Targets were ranked lower when they are used by:

- `sprintf` / `snprintf` style helpers
- `measurement_dispatch`
- obvious display text or drawing paths
- generic filesystem / JPEG / asset decode code

## Top 10

### 1. `0x080BB3FC`

Why it is first:

- Direct scope lookup table.
- At `0x080042E6` / `0x080042F4` and `0x080048C0` / `0x080048CC`, stock
  reads `ldrb [0x080BB3FC + index]`.
- The result is immediately promoted to `0x0500 | value` and queued for
  UART TX.

Why it matters:

- This is the cleanest missing link between internal scope state and the
  raw low bytes the FPGA actually sees.
- If the downloaded vendor image omits this table, our guessed `0x050C`,
  `0x050E`, `0x0511`, etc. may still be structurally wrong.

### 2. `0x080BB40C`

Why it is second:

- This is a 16-bit indexed table, not a plain string.
- At `0x0800C072` / `0x0800C076` and `0x08014172` / `0x08014176`, stock
  forms `0x080BB40C + state[0xF63] * 4` and then loads a halfword with an
  additional bit/range index.
- It has 15 references, including scope-adjacent code.

Why it matters:

- This looks like a compact per-mode or per-range decision table.
- It is much more likely to encode trigger/range/coupling behavior than a
  display string would.

### 3. `0x080BB3EC`

Why it is high:

- Read in numeric logic, not rendering logic.
- At `0x0802A4BC` / `0x0803372E`, stock extracts a nibble, indexes
  `0x080BB3EC`, then uses the result as a shift amount.

Why it matters:

- This looks like a decode helper paired with `0x080BB404`.
- It may explain how stock unpacks packed scope-state bits before feeding
  later decision logic.

### 4. `0x080BB404`

Why it is paired with `0x080BB3EC`:

- Accessed immediately after the `0x080BB3EC` nibble lookup in the same
  numeric path.
- Seen at `0x0802A4CA` / `0x0802A4D4`.

Why it matters:

- Very likely part of the same decode family.
- If `0x080BB3EC` says "how far to shift", `0x080BB404` may say "which
  mask / field shape to apply next."

### 5. `0x080BC18B`

Why it still makes the list:

- Used from scope-adjacent code at `0x0800B134` and `0x080157C4`.
- Also used from `0x0802CFC0`, which goes through a filesystem helper path.

Why it matters:

- This no longer looks like a direct protocol table.
- It likely points to a config name, file name, or other scope-related
  asset loaded through the SPI-flash filesystem.
- If scope startup depends on an external config object, this is one of the
  most plausible missing names to recover first.

### 6. `0x080BC1B4`

Why it is next:

- Same general family as `0x080BC18B`.
- Referenced at `0x0802E836` and `0x0802EA0C` from filesystem-oriented
  control paths.

Why it matters:

- Strong candidate for a sibling file/path/config token.
- Worth checking immediately after `0x080BC18B` because the two may be part
  of the same scope-init resource chain.

### 7. `0x080BB927`

Why it is on the list:

- In `0x0802E81A`, stock passes `0x080BB927` directly into a filesystem
  helper (`0x0802DC40` path).

Why it matters:

- This looks like a missing file or directory name.
- If the stock device expects external config or calibration content from
  SPI flash, identifying these names will tell us what to dump or recreate
  first.

### 8. `0x080BBC58`

Why it is paired with `0x080BB927`:

- Immediately follows the `0x080BB927` lookup path in the same
  `0x0802E81A` branch.

Why it matters:

- Likely sibling resource name in the same filesystem workflow.
- Useful for turning "scope depends on external data" into named files
  rather than speculation.

### 9. `0x080BB92B`

Why it is next:

- Mirrors the previous pattern at `0x0802E88A`.
- Also passed directly into the filesystem helper path.

Why it matters:

- Another likely file/path token.
- Helps reconstruct the missing filesystem/config dependency chain around
  scope or calibration setup.

### 10. `0x080BC1A5`

Why it rounds out the first pass:

- Paired with `0x080BB92B` in the same `0x0802E88A` branch.
- Same "likely missing filename / asset token" behavior as the other
  filesystem-open strings.

Why it matters:

- If `0x080BB927` / `0x080BBC58` are one resource pair, this may be the
  next pair in the sequence.
- Good candidate for discovering whether scope startup depends on named
  resources not present in the downloaded app image.

## Why these were *not* ranked higher

### `0x080BC499`, `0x080BC54A`, `0x080BC58B`, `0x080BC5FC`

These have many references, but the usage looks display-oriented:

- passed into `measurement_dispatch`
- used from `scope_ui_draw_main`
- paired with screen coordinates and draw calls

They are probably strings, labels, or UI assets, not protocol blockers.

### `0x080BC859`, `0x080BCAD2`, `0x080BCAE5`

These are missing and scope-adjacent, but they are mostly used through
`sprintf`/`snprintf` style formatting or other presentation paths.

They are still worth labeling eventually, just not before the ten targets
above.

### `0x080BE05C`, `0x080BE118`

These are heavily referenced, but the current caller context looks more
like a generic numeric/decode table inside a broader non-scope data path
than a direct scope-command dependency.

Useful later, but not first.

## Suggested Ghidra pass

### Pass 1: direct scope-command builders

Use `DecompileRange.java` or `ForceDecompile.java` on:

- `0x08004200-0x08004920`
- `0x0800C000-0x0800C120`
- `0x08014140-0x08014190`
- `0x0802A490-0x0802A4E0`
- `0x08033720-0x08033750`

Goal:

- fully annotate the `0x080BB3FC`, `0x080BB40C`, `0x080BB3EC`, and
  `0x080BB404` families
- recover exact index variables and state fields used to address them

### Pass 2: missing filesystem/config names

Focus on:

- `0x0802CFC0-0x0802D020`
- `0x0802E800-0x0802E8B0`
- `0x080157A0-0x08015830`
- `0x0800B100-0x0800B150`

Goal:

- identify whether `0x080BC18B`, `0x080BC1A5`, `0x080BC1B4`,
  `0x080BB927`, `0x080BBC58`, and `0x080BB92B` are file paths, resource
  names, or compact config tables

### Pass 3: memory map sanity

Before doing any large RE pass:

- verify the current Ghidra project really ends the loaded ROM block at
  `0x080B7680`
- do not run another broad `ComprehensiveAnalysis.java` pass first

If useful, create an uninitialized read-only block above the file end so
the missing references resolve as addresses cleanly while staying visibly
"unknown."

## Best current hypothesis

The missing regions split into two classes:

1. real scope-decision tables
   - `0x080BB3FC`
   - `0x080BB40C`
   - `0x080BB3EC`
   - `0x080BB404`

2. likely resource/config names used by SPI-flash filesystem helpers
   - `0x080BC18B`
   - `0x080BC1B4`
   - `0x080BB927`
   - `0x080BBC58`
   - `0x080BB92B`
   - `0x080BC1A5`

That split actually supports the current hardware behavior:

- raw meter transport works
- raw scope heartbeat works
- but stock may still depend on both:
  - missing scope decision tables in high flash
  - missing external named resources/config loaded through SPI flash
