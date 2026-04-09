# BCAD2 Preview Completion 2026-04-09

Purpose:
- close the remaining BMP-side uncertainty after
  [bcad2_preview_path_probe_2026_04_09.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/bcad2_preview_path_probe_2026_04_09.md)
- answer one narrow question:
  once the concrete BMP path is seeded, is the rest of the `FUN_08036084()`
  path fundamentally blocked by missing descriptors, or only by emulator cost?

Artifacts:
- completed long run:
  [unicorn_fatfs_then_fun_08036084_bcad2_pathseed_stage_stub_fastloops_long_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_stage_stub_fastloops_long_2026_04_09.txt)
- completed mutated flash image:
  [w25q128_bcad2_pathseed_stage_stub_fastloops_long_runtime.bin](/Users/david/Desktop/osc/archive/w25q128_bcad2_pathseed_stage_stub_fastloops_long_runtime.bin)
- extracted filesystem view:
  [w25q128_bcad2_pathseed_stage_stub_fastloops_long_extract](/Users/david/Desktop/osc/archive/w25q128_bcad2_pathseed_stage_stub_fastloops_long_extract)
- resulting BMP artifact:
  [1.BMP](/Users/david/Desktop/osc/archive/w25q128_bcad2_pathseed_stage_stub_fastloops_long_extract/volume_200000/Screenshot%20file/1.BMP)

## 1. Stage-Scoped Preview Stubbing Was The Right Fix

The earlier global skip at `0x0800BCD4` was too blunt.

The useful correction was:

- stub `FUN_0800BCD4` only during the chained second stage
- fill the active framebuffer deterministically from the live preview state at
  `0x20008350`
- fast-forward the three huge pixel post-process loops in `FUN_08036084()`:
  - `0x08036290 -> 0x08036304`
  - `0x080363E2 -> 0x08036456`
  - `0x080364EC -> 0x08036560`

That changed the question from "can this path even open and create?" to "can it
finish a full write/close lifecycle?".

## 2. The BMP-Side Path Now Completes Cleanly

The completed long run in
[unicorn_fatfs_then_fun_08036084_bcad2_pathseed_stage_stub_fastloops_long_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_stage_stub_fastloops_long_2026_04_09.txt)
returns cleanly:

- `stop_pc = 0x08FFFFF0`
- `stop_r0 = 0`
- `run_error = None`

And it fully exercises the BMP-side flash path:

- `2254` SPI2 transactions
- `688` page programs
- `43` sector erases
- preview dispatcher stub hit `8` times
- all three large pixel loops were skipped exactly once

So the BMP-side path is no longer "partially understood". In the emulator, it
now completes end-to-end.

## 3. Concrete Filesystem Result: `Screenshot file/1.BMP`

The extracted Volume 1 manifest now contains:

- `Screenshot file/1.BMP`
- `cluster = 13`
- `size = 153654` (`0x25836`)

That is the exact payload shape implied by `FUN_08036084()`:

- 54-byte BMP header
- 320 x 20 x 2 bytes
- 320 x 201 x 2 bytes
- 320 x 19 x 2 bytes

Total:

- `54 + 12800 + 128640 + 12160 = 153654`

So the earlier zero-size `cluster=0` entry was not a descriptor mystery. It was
just a partial run that had not yet reached the later write/close path.

## 4. The Resulting File Is A Valid BMP

Host-side `file(1)` identifies the created artifact as:

- `PC bitmap, Windows 3.x format, 320 x -240 x 16`
- `image size 153600`
- `cbSize 153654`
- `bits offset 54`

And the header begins with the expected:

- `42 4D` (`BM`)

So this is not just a directory entry with random bytes behind it. It is a
structurally valid screenshot artifact.

## 5. What This Means

This materially tightens the missing-descriptor story:

- `0x080BC859 = "2:/Screenshot simple file/%d.bin"` is now a proven runtime gate
  for the metadata `%d.bin` lifecycle
- `0x080BCAD2 / 0x080BCAE5` are now also grounded as a real neighboring BMP-side
  family
- the BMP-side family additionally depends on the already-formatted runtime path
  in `string_format_buffer`
- once that concrete path exists, the rest of the BMP-side path is no longer
  blocked by unknown descriptors

So the BMP-side uncertainty is now basically gone:

- descriptor family: understood
- path formatting requirement: understood
- artifact creation: observed
- full payload completion: observed

## 6. Best Next Step

The next highest-value move is no longer more BMP-side emulation work.

It is to trace the higher runtime owner that formats `string_format_buffer`
before `FUN_08036084()` runs, and compare that with the already-solved
`0x080BC859` `%d.bin` path. That should tell us how stock couples:

- preview/screenshot generation
- `%d.bin` metadata creation
- and the broader right-panel resource state machine

At this point, the remaining uncertainty looks increasingly like "which
descriptor-backed runtime owner chooses these sibling artifact paths?" rather
than "what does the BMP helper do?".
