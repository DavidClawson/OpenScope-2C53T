# BCAD2 Preview Path Probe 2026-04-09

Purpose:
- follow up the `%d.bin` result by checking the neighboring BMP-side
  preview/resource family
- separate three questions:
  1. are the strings `0x080BCAD2 / 0x080BCAE5` enough on their own?
  2. does `FUN_08036084()` need a concrete preformatted path?
  3. if it does, how far does the BMP-side write path get before emulator
     limitations take over?

Artifacts:
- strings-only probe:
  [unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_probe_2026_04_09.txt)
- concrete-path probe:
  [unicorn_fatfs_then_fun_08036084_bcad2_pathseed_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_2026_04_09.txt)
- timeout/skip-render follow-up:
  [unicorn_fatfs_then_fun_08036084_bcad2_pathseed_skiprender_timeout_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_skiprender_timeout_2026_04_09.txt)
- extracted concrete-path dump:
  [w25q128_bcad2_pathseed_runtime_extract](/Users/david/Desktop/osc/archive/w25q128_bcad2_pathseed_runtime_extract)

## 1. Strings Alone Are Not Enough

Run:

- `fatfs_init()`
- then `FUN_08036084()`
- with:
  - root + directory-root descriptor families
  - `0x080BCAD2 = "2:/Screenshot file/%d.bmp"`
  - `0x080BCAE5 = "%d.bmp"`
  - `DAT_20000F09 = 1`

Result:

- `stop_r0 = 1`
- `64` transactions total
- old baseline only

Interpretation:

- unlike `FUN_08035ED4()`, `FUN_08036084()` does **not** format its own path
  from the missing high-flash strings alone
- seeding the descriptor text is not sufficient to unlock the BMP-side path

## 2. Concrete Path Seeding Changes The Result Immediately

Run:

- `fatfs_init()`
- then `FUN_08036084()`
- with:
  - the same descriptor strings
  - plus
    `string_format_buffer = "2:/Screenshot file/1.bmp"`

Result:

- [unicorn_fatfs_then_fun_08036084_bcad2_pathseed_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_2026_04_09.txt)
- `121` transactions total
- `32` page programs
- `2` sector erases
- new read traversal into:
  - `0x212000`
  - `0x201000`
- execution later dies with:
  - `stop_pc = 0x00000000`
  - `last_pc = 0x0800BCE4`
  - `run_error = UC_ERR_EXCEPTION`

Interpretation:

- this is the key correction
- `FUN_08036084()` expects the concrete BMP path to already be in
  `string_format_buffer`
- once that concrete path exists, the function does enter a deeper BMP-side
  create/write path

## 3. Concrete FAT Artifact: Zero-Size `1.BMP`

The concrete-path run produced a real new Volume 1 directory entry in the
mutated dump:

- `Screenshot file/1.BMP`

Seen in:

- [w25q128_bcad2_pathseed_runtime_extract](/Users/david/Desktop/osc/archive/w25q128_bcad2_pathseed_runtime_extract)

The entry properties are:

- attr `0x20`
- cluster `0`
- size `0`

Interpretation:

- the BMP-side path progressed far enough to create the directory entry itself
- but it did **not** progress far enough to allocate/populate the file data
- so the current blocker is later than simple descriptor/path resolution

## 4. The Remaining BMP-Side Blocker Looks Render-Side

The concrete-path run stops inside the preview/render family around `0x0800BCE4`
after `FUN_08036084()` has already started real flash work.

That matches the function body:

- it opens the already-formatted BMP path
- writes a `BM...` header
- allocates several framebuffer slices
- calls the preview/render helper `FUN_0800BCD4(...)`
- then writes pixel bands to flash

So the best current read is:

- missing descriptor text is **not** the main BMP-side blocker anymore
- the remaining emulator-side problem is inside preview/render execution and the
  later BMP payload write path

## 5. Timeout/Skip-Render Follow-Up

I also tried skipping the preview-render helper and capturing a timeout-bounded
runtime snapshot:

- [unicorn_fatfs_then_fun_08036084_bcad2_pathseed_skiprender_timeout_2026_04_09.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_then_fun_08036084_bcad2_pathseed_skiprender_timeout_2026_04_09.txt)

That run no longer failed immediately, but the timeout snapshot did **not**
settle into a clean `1.BMP` artifact. The extracted timeout image only retained
the three root directories.

So a simple "skip render helper and hope the rest completes" is not yet enough
to close the BMP-side path.

## 6. Best Current Read

This narrows the BMP-side family in a useful way:

- `0x080BCAD2 / 0x080BCAE5` are still real neighboring descriptors
- but the first missing input for `FUN_08036084()` is not descriptor text
  alone; it is the **concrete preformatted path**
- once that path exists, the function gets far enough to create a zero-size
  `1.BMP` directory entry before emulator-side render/payload issues stop it

So the BMP-side uncertainty is now much smaller than before:

- path formatting: understood
- entry creation: observed
- payload completion: still blocked by preview/render-side emulation

## 7. Best Next Step

The next highest-value move is to emulate or stub just enough of the
`FUN_0800BCD4(...)` preview pipeline to let `FUN_08036084()` finish the BMP
payload write path:

1. keep the concrete `1.bmp` path seeded
2. preserve the successful descriptor/data setup
3. replace the preview helper with a deterministic framebuffer producer rather
   than skipping it entirely

That is the cleanest way to learn whether the BMP-side path produces a real file
payload once rendering stops being the limiting factor.
