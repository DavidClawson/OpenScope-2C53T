# Meter W25Q Calibration Boundary

Date: 2026-06-06

This note records the DMM-specific boundary from the bench unit's W25Q128
dump. It does not prove that meter factory calibration is absent. It does prove
that the obvious `System file/9999.BIN` lead is not a recovered meter
calibration source on this dump, and must not be used to justify coefficients.

## Dump Evidence

### Current Full Bench Dump, 2026-06-06

- Raw image: `tmp/w25q-full-2026-06-06.bin`
- Size: `0x1000000` bytes (`16 MiB`, W25Q128 full device)
- SHA-256:
  `5706dcd936bdb5d60bfdb5c972fb1db7b1554c6004b57ce36c7544ac8a377d14`
- Transport: read-only OpenScope USB CDC `FLASHDUMP` binary payload via
  `scripts/dump_spi_flash.py`
- Consistency check: first `0x400000` bytes match the earlier
  `tmp/w25q-first4m.bin` dump byte-for-byte.
- Extraction command:
  `python3 scripts/extract_spi_flash_fat.py tmp/w25q-full-2026-06-06.bin --out tmp/w25q-full-2026-06-06-extract`

Extraction summary:

```json
[
  {
    "base": "0x000000",
    "bytes_per_sector": 4096,
    "sectors_per_cluster": 1,
    "total_sectors_16": 512,
    "volume_size_bytes": 2097152,
    "root_entries": 2,
    "manifest_entries": 165
  },
  {
    "base": "0x200000",
    "bytes_per_sector": 4096,
    "sectors_per_cluster": 1,
    "total_sectors_16": 3584,
    "volume_size_bytes": 14680064,
    "root_entries": 0,
    "manifest_entries": 0
  }
]
```

Volume 0 again contains the populated `System file` directory of JPG UI assets
plus the same empty placeholder:

```json
{
  "path": "System file/9999.BIN",
  "name": "9999.BIN",
  "attr": 32,
  "cluster": 0,
  "size": 0,
  "is_dir": false
}
```

Volume 1 is a valid FAT12-looking volume at `0x200000` (`MSDOS5.0`, 4096-byte
sectors, 512 root directory entries in the boot sector), but its root directory
extracts no reachable files. Raw full-image scans do show written orphaned or
unlinked FAT/data regions after `0x200000`, including BMP-looking framebuffer
payloads:

- `0x219000`, `0x240000`, `0x284000`, `0x2AB000`, `0x2D2000`, `0x317000`:
  each starts with a BMP header, `320x-240`, `16` bpp, size `153654` bytes.
- `0xCB2000..0xCC3000`: sparse FAT directory-like dot entries only in the
  sampled bytes.
- No JPEG/JFIF/Exif signatures appear after `0x200000`; the JPG assets are in
  Volume 0.

These raw leftovers are not a recovered meter calibration source. They are not
named or xrefed as DMM data, and their observed structure matches screenshots or
filesystem residue rather than range coefficients.

Machine-check boundary constants:

- `summary.json` Volume 0: `"base": "0x000000"`, `"manifest_entries": 165`
- `summary.json` Volume 1: `"base": "0x200000"`, `"manifest_entries": 0`
- `System file/9999.BIN`: `"attr": 32`, `"cluster": 0`, `"size": 0`,
  `"is_dir": false`
- Volume 1 reachable-root manifest: empty list (`[]`)
- raw high-flash residue: only unxrefed FAT/BMP-like leftovers, not a
  recovered meter calibration file
- scope of the claim: not proof that all possible factory calibration is
  absent

### Older Archived Dump

Source note: `w25q128_dump_2026_04_08.md`

- Raw image: `/Users/david/Desktop/osc/archive/w25q128_dump.bin`
- SHA-256:
  `320b36c84526c882f855850acc22792a7d7dab0c9fafba1a9e59293731cbd455`
- Transport: read-only USB CDC `flash dump <addr> <len>` from the current
  bench unit
- Volume 0 base `0x000000`: FAT12, `4096` bytes/sector, `512` sectors,
  implied span `0x200000`
- Volume 1 base `0x200000`: FAT12, `4096` bytes/sector, `3584` sectors,
  implied span `0xE00000`

The local first-4MiB extraction manifest for Volume 0 contains the populated
`System file` directory with 160 JPG UI assets and this exact placeholder:

```json
{
  "path": "System file/9999.BIN",
  "name": "9999.BIN",
  "attr": 32,
  "cluster": 0,
  "size": 0,
  "is_dir": false
}
```

Volume 1 extraction produced an empty manifest for this archived bench dump.
The `w25q128_dump_2026_04_08.md` spot checks at `0x400000`, `0x800000`, and
`0xF00000` were all erased `0xFF`. The 2026-06-06 full dump supersedes those
spot checks for this unit: later address space contains some filesystem
residue, but not a reachable DMM calibration file or a stock-xrefed meter table.

## DMM Interpretation

`9999.BIN` is not a recovered meter calibration source in this evidence set:
the directory entry is archive attribute `0x20`, cluster 0, size 0. The file
name exists, but the file has no cluster chain and no bytes to decode. Any
DMM coefficient derived from this lead would be invented.

This boundary is intentionally narrower than "there is no meter calibration":

- stock V1.2.0 string evidence still contains `3:/System file/9999.bin` at
  `0x080BC841`, but existing xref notes do not recover a direct DMM consumer
- high-flash descriptor reconstruction remains incomplete
- the 115,638-byte H2/SPI3 table may still contain frontend setup or
  calibration-like data, but current evidence proves byte-count replay only,
  not FPGA acceptance or DMM physical correction
- a second-unit W25Q comparison could still reveal populated files or device
  variation

Therefore production firmware must continue to fail closed for unrecovered
factory-calibration cases rather than using `9999.BIN`, W25Q presence, or the
low-DCV live mismatch as a coefficient source.

## Saved-Config Default Lead

A separate stock lead now exists in master init, but it is still a boundary, not
a usable DMM coefficient.  Stock restores a calibration-like saved-config table
into `0x20000358..0x2000044A`, checks `ms[0x34E]` at `0x080261A8`, and if that
sentinel is erased (`0xFFFF`) or zero, writes hardcoded defaults at
`0x080261BE..0x08026506` after the default-path entry at `0x08026198`.

This proves stock has persistent/default calibration-like table data outside
the empty `9999.BIN` lead. It does not prove those values are recovered DMM
physical coefficients. The current consumer evidence for the overlapping
`0x20000358` RAM region still belongs to scope/DAC/measurement paths, so these
defaults must not be applied to low-DCV, current, or low-Ohm readings without a
DMM-owned consumer xref or live trace.

## Production Guard

The open firmware `flash_fs_load_factory_cal()` path is a fail-closed placeholder.
It must not probe invented `cal_ch1.bin` / `cal_ch2.bin` names or
set `factory_cal_t.loaded` from arbitrary 301-byte files. The 301-byte regions
at stock state offsets `0x356` and `0x483` were reclassified as oscilloscope
roll-buffer state in `cal_data_myth_busted.md`, not DMM calibration.

The roll-buffer preload guard now binary-pins `FUN_08001830` / `0x08001830` and the
master-init callers at `0x080271A8`: stock passes `state+0x356` and
`state+0x483`, count `0x12D`, and `state[4/5] ^ 0x80`. That proves init-time
roll-buffer preload/transform shape, not a meter calibration table.

Until a real stock source is recovered, meter code must continue to reject
unresolved calibration-dependent readings instead of applying loaded flash bytes
or observed-case coefficients.

## Remaining Leads

- recover stock xrefs that dereference the `3:/System file/9999.bin` string or
  its containing resource table
- compare another unit's W25Q dump against this bench dump
- trace H2/SPI3 upload acceptance and apply semantics beyond byte count
- capture stock-mode/runtime traces for DMM mux/range transitions and any
  calibration table reads
