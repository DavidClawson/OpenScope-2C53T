# W25Q SPI-flash WRITE layer — reimplemented + bench-confirmed (2026-06-13)

First build-bucket win against the ABSENT subsystem surfaced by wf4. Reimplemented the
stock W25Q128 write driver byte-exact in `firmware/src/drivers/flash_fs.c` and
**bench-verified it on unit #2**.

## Primitives (all byte-faithful to the V1.2.0 stock decode)

| Our fn (flash_fs.c) | Stock | Opcode / behavior |
|---|---|---|
| `raw_write_enable` | FUN_0802f344 | WREN 0x06 |
| `raw_wait_busy` | FUN_0802f11c | RDSR 0x05, poll BUSY (bit0); **bounded** (no infinite spin) |
| `raw_sector_erase_nolock` / `flash_fs_raw_sector_erase` | FUN_0802ee9c | 4KB erase 0x20 + 24-bit addr |
| `raw_page_program_nolock` | FUN_0802f36c | page program 0x02 + addr + data, CS↑, wait-busy |
| `raw_program_nolock` / `flash_fs_raw_program` | FUN_0802f2ac | 256B page-boundary split loop |
| `raw_write_block_nolock` / `flash_fs_raw_write_block` | FUN_0802f16c | smart 4KB read-modify-write: erase a sector only when a target byte isn't already 0xFF; 4KB scratch heap-allocated (RAM is tight) |

## Bench result (unit #2, `flash wtest 0x500000 CONFIRM`)

```
wtest @0x500000: sector blank, OK to proceed
PASS: page-program (in-place)
PASS: erase + read-modify-write
PASS: sector restored to 0xFF
```

`wtest` is self-protecting (refuses any non-blank 4KB sector, restores to 0xFF after), so it
can only ever touch erased flash. Erase is *genuinely* proven here because path 2 writes
non-0xFF data that must be erased before the read-modify-write. → 6 fns to **R2/V2**, the
first bench-confirmed device behavior in the coverage ledger (all prior 13 were static V1).
Headline 18.6% → 27.1% (19/70).

## RE insight: stock's byte primitive waits for full SPI completion, ours didn't

Reads worked but writes silently did nothing (WREN latched WEL=1, no block-protect, yet
programs/erases never executed — proven via the `flash diag` status-register probe:
`SR1=0x02 SR2=0x02 SR3=0x60`, BP=0, WEL-after-WREN=1). Root cause: our
`flash_fs_raw_spi_xfer` waited only for **RXNE**, so CS rose while the SPI **BSY** flag was
still set — truncating the program frame's final byte, which the W25Q rejects (a program must
be a whole number of bytes). Reads tolerate the early CS edge (data already latched). Stock's
byte primitive `FUN_0802f0c4` polls `i2c_transfer(0x40003800, …)` (the SPI2 SR, mislabeled
i2c by Ghidra) in a way that waits for full byte completion, so stock never hit this. Fix:
an explicit `raw_spi_settle()` (poll BSY clear, bounded) before every CS-high on the write
primitives.

## Gotchas recorded (cost several bench cycles)

- **Unit #2 = FNIRSI factory bootloader, app slot 0x08007000 → always `make guest`.** Plain
  `make` (0x08004000) flashes but hangs in firmware-update mode. See `[[unit2-needs-make-guest]]`.
- The debug shell task has only **2KB stack** — the first `wtest` used ~1.1KB of arrays and
  overflowed it (red fault screen); rewritten to a single 64-byte buffer.

## Still ABSENT (the rest of the subsystem, next build targets)

FatFs LFN directory ops, file read/write/rename/delete, screenshot-BMP save. The write
*primitives* are the foundation; the FatFs layer on top is still stubbed in `flash_fs.c`.
