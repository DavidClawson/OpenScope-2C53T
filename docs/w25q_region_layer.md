# W25Q region layer (plan item C1)

Design note for `firmware/src/drivers/flash_regions.{c,h}`. The code comments are
the primary reference; this is the one-page version for anyone building on it
(C2 streaming screenshot, D1 module loader, user calibration).

## What it is

A static region table plus a bounds-checked allocator over the W25Q128JV. Not a
filesystem, no FatFS, ~600 lines including comments.

## What it is for

**Not wear.** 100,000 P/E cycles per 4 KB sector is ~274 years at one erase a
day. The hazard is a **stray erase destroying data we cannot regenerate**: the
stock UI assets, and on a pristine unit the factory calibration under
`3:/System file/`. A sector erase is indivisible and irreversible, and
`flash_fs_raw_sector_erase(0)` will erase sector 0 without complaint.

The guard rail goes in *before* the first automated writer exists, which is why
this is worth doing now: today nothing writes the W25Q at runtime.

## Region map

Derived from `reverse_engineering/analysis_v120/w25q128_flash_map_2026-06-13.md`
(a live 16 MB dump parsed offline) plus the two archived full-chip dumps
(2026-04-08 and 2026-05-30), which are byte-identical to each other.

| id | range | size | kind |
|---|---|---|---|
| `sysvol` | `0x000000`–`0x1FFFFF` | 2 MB | **read-only** — FatFS `3:`, UI JPEGs, `System file/`, factory cal path |
| `uservol` | `0x200000`–`0xEFFFFF` | 13 MB | **read-only** — FatFS `2:`, stock screenshot store |
| `usercal` | `0xF00000`–`0xF0FFFF` | 64 KB | append log — user calibration overlay |
| `settings` | `0xF10000`–`0xF1FFFF` | 64 KB | append log — saved settings |
| `modules` | `0xF20000`–`0xF9FFFF` | 512 KB | read/write — module data assets |
| `scratch` | `0xFA0000`–`0xFFEFFF` | 380 KB | read/write — streaming screenshot, general use |
| `tail` | `0xFFF000`–`0xFFFFFF` | 4 KB | **read-only** — programmed (4096 zero bytes), purpose unknown |

Why the writable window sits at the top: it is `0xFF`-erased in both archived
dumps, byte for byte; it is the far end of volume `2:`, which stock allocates
from the bottom; and the one programmed sector at the very top is left read-only
rather than assumed free.

**Stated trade-off:** that window is unallocated free space *inside* FAT volume
`2:`. Raw records there are invisible to FatFS, so a stock firmware writing
enough screenshots could allocate those clusters and overwrite **us**. The risk
runs in the safe direction — stock can clobber our data, we can never clobber
stock's. **Ready for bench validation**; changing the window is a one-line edit
in the table.

## Design rules

1. **Default deny.** Any address in no region is unwritable. Granting write
   access is an explicit table edit.
2. **Fail closed, never partially.** Everything is validated before a byte is
   touched. A range crossing a region boundary is an error, not a clamp — even
   when both regions are writable.
3. **No implicit erase.** `flash_region_write()` never erases. If a target bit
   must go 0→1 it returns `FLASH_REGION_ERR_NEEDS_ERASE` and writes nothing. A
   hidden read-modify-write *is* the stray-erase mechanism.
4. **Verify after write.** Every program is read back and compared. This project
   has already been bitten by writes that silently did nothing.

Plus: identical writes are elided (no erase, no program), blank sectors are not
re-erased, and a malformed region table refuses to initialise so every write
path fails closed rather than falling back to "no table, no restrictions".

## API sketch

```c
flash_regions_bind_w25q();                       /* device binding */
flash_region_read (id, off, buf, len);
flash_region_write(id, off, data, len);          /* no erase, elides no-ops */
flash_region_erase(id, off, len);                /* sector-aligned, explicit  */
flash_region_reset(id);
flash_regions_write_abs(addr, data, len);        /* absolute address variant  */
flash_regions_erase_abs(addr, len);
flash_regions_check_abs(addr, len);              /* policy question, no I/O   */
flash_region_append     (id, data, len);         /* record log                */
flash_region_read_latest(id, buf, cap, &len);
flash_region_log_info   (id, &used, &free, &records);
flash_regions_stats();                           /* refused / elided counters */
```

Append records are `[u16 magic][u16 len][u32 crc32][payload padded to 4]`. The
header is programmed **before** the payload so a torn write leaves a record of
known length whose CRC fails — the scanner steps over it and the log keeps
working. Appending a payload identical to the newest valid record is elided
entirely, which is what keeps a frequently-saved value off the erase path.

## Verification

`scripts/test_flash_regions.py` (in the `run_tests.py` gate) builds
`firmware/tests/test_flash_regions.c` against the real `flash_regions.c` and a
**NOR flash model** — erase sets `0xFF`, program can only clear bits, page
programs cannot cross a page boundary, every erase and program counted. Refusals
are asserted as *zero erases, zero programs and a byte-identical chip image*,
not merely as a return code.

The same suite then **deletes each guard in turn and requires the tests to go
red**. A mutation that still passes is reported as a failure, because it means
the test was not testing anything. This is deliberate: the project's recurring
failure mode is instruments that report confidently on state they cannot
observe.

## Deliberately left for a follow-up

- **No caller yet.** Nothing invokes `flash_regions_bind_w25q()`, so the linker
  garbage-collects the whole module today (guest text size is unchanged). The
  first writer — user cal, module store, or streaming screenshot — wires it in.
- **The raw primitives remain an unguarded escape hatch.** `flash_fs_raw_*` are
  maintainer bench diagnostics and are not policed, so locking them down would
  change `flash wtest` behaviour. The hook, if wanted, is a
  `flash_regions_check_abs()` call at the top of each public wrapper in
  `flash_fs.c`.
- **The append log rescans on every access** rather than caching a cursor.
  Bounded and rare (appends are user-action or shutdown events), but a full
  64 KB log costs a few thousand 8-byte reads to scan.
- **No bench validation.** Every result here is a host test. The writable window
  in particular has never been written on real hardware.
