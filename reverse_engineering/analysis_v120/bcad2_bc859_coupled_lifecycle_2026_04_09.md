# BCAD2 + BC859 Coupled Lifecycle 2026-04-09

Purpose:
- test whether the solved BMP-side path and the solved `%d.bin` metadata path
  can coexist in one emulated stock session
- answer a narrower runtime question:
  are `FUN_08036084()` and `FUN_08035ED4()` isolated helpers, or sibling stages
  of one descriptor-backed overlay/resource lifecycle?

Artifacts:
- chained three-stage run:
  [unicorn_fatfs_then_bcad2_then_bc859_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_bcad2_then_bc859_2026_04_09.txt)
- mutated flash image:
  [w25q128_bcad2_bc859_runtime.bin](/Users/david/Desktop/osc/archive/w25q128_bcad2_bc859_runtime.bin)
- extracted filesystem view:
  [w25q128_bcad2_bc859_extract](/Users/david/Desktop/osc/archive/w25q128_bcad2_bc859_extract)
- resulting artifacts:
  [1.BMP](/Users/david/Desktop/osc/archive/w25q128_bcad2_bc859_extract/volume_200000/Screenshot%20file/1.BMP)
  [1.BIN](/Users/david/Desktop/osc/archive/w25q128_bcad2_bc859_extract/volume_200000/Screenshot%20simple%20file/1.BIN)

## 1. Chained Run Setup

The run stages were:

1. `fatfs_init()`
2. `FUN_08036084()` with:
   - root + directory-root + overlay descriptor families present
   - concrete path seeded in `string_format_buffer`:
     - `2:/Screenshot file/1.bmp`
   - stage-scoped preview stub and loop fast-forwarding enabled
3. `FUN_08035ED4()`

This is the first run that exercised both sibling artifact families in one
continuous emulated session.

## 2. The Full Three-Stage Lifecycle Completes

The chained run in
[unicorn_fatfs_then_bcad2_then_bc859_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_bcad2_then_bc859_2026_04_09.txt)
returns cleanly:

- `stop_pc = 0x08FFFFF0`
- `stop_r0 = 0`
- `run_error = None`
- `stage 2 of 2`

And it drives substantial flash traffic:

- `2520` SPI2 transactions
- `768` page programs
- `48` sector erases

So this is not a partial snapshot. The chained lifecycle now reaches a normal
return after both post-`fatfs_init()` consumers run.

## 3. Both Runtime Artifacts Exist Together

The extracted Volume 1 manifest now contains:

- `Screenshot file/1.BMP`
  - `cluster = 13`
  - `size = 153654`
- `Screenshot simple file/1.BIN`
  - `cluster = 51`
  - `size = 607`

That is exactly the combination we wanted to test.

So the BMP-side and `%d.bin`-side families are not merely two unrelated paths
that happen to work in isolation. They can coexist in one descriptor-backed
runtime session on the same mutated flash image.

## 4. What This Means

This strengthens the current runtime model in a useful way:

- `FUN_08036084()` is the preview/screenshot artifact builder
- `FUN_08035ED4()` is the per-index `%d.bin` metadata consumer/creator
- both depend on the missing high-flash overlay family
- and both fit naturally under the same right-panel/resource lifecycle

That moves the remaining question one layer up:

- not "what do these helpers do?"
- but "which higher runtime owner formats the BMP path, then later reaches the
  `%d.bin` metadata path for the same index?"

## 5. Best Next Step

The next highest-value move is to trace that shared higher owner above the now
solved sibling helpers:

- path formatter into `string_format_buffer` before `FUN_08036084()`
- later `%d.bin` formatter/caller path into `FUN_08035ED4()`
- and the panel/resource state byte updates that choose when both happen

At this point the emulator results argue that the hard part is no longer the
filesystem helpers themselves. It is the higher right-panel resource owner that
chooses when to invoke them together.
