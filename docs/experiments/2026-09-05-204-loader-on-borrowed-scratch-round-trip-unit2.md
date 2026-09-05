# EXP-204 — the loader on a borrowed scratch buffer: stage, install and swap back on unit #2

- **Date:** 2026-09-05
- **Numbering:** unit #2's writeups take the **201+ block** (see EXP-201).
- **Unit:** bench unit #2 (Stlkv), factory bootloader, W25Q = Zbit ZB25VQ128 clone (id 5E17)
- **Build:** `make guest-coldtrace` from the tree at `a45c56f` (`cdc-fwload` merged with `main` at `545a9d8`, plus `dfede95` and `a45c56f`). 607 012 B, crc32 `F7D8EDFD`, sha256 `ad5d834f…`, banner `Build: Sep  5 2026 23:23:43`.
- **Status:** CONFIRMED

## 1. Problem

Merging `main` at `545a9d8` into `cdc-fwload` left `make guest` and `make` (app) 568 B over the RAM ceiling, and the excess was this branch's: `main` alone links with 648 B to spare, the loader brought ~1.2 KB of statics. Two changes answer that on the host — the loader now borrows the shell task's `shell_bus_scratch` instead of owning a 512 B intake buffer (`dfede95`, 568 → 48 B over, measured), and a proposed 1 KB reduction of the FreeRTOS heap covers the rest (`a45c56f`, links with 1 044 B of linker headroom on guest, read from the map). Neither had run on hardware, and the first one moves the buffer the RAM-resident installer reads through while the app slot is erased under it.

## 2. Hypothesis

If lending the arena is sound, then on the new build: a `fwload` of a 607 KB image reaches `STAGED` with the announced CRC re-read at rest; `spi3 read` — the arena's other tenant — returns a plausible capture both before and after that transfer; `fwswap a` installs the cached port image from RAM and the device comes back as the port with the cache table intact. If the borrowed pointer is wrong for the installer, the device goes dark at `fwswap` (recovery MENU+Power); if the arena is contended, `spi3 read` after the transfer returns garbage or the stage fails its at-rest CRC.

## 3. Procedure

Host tools only, no physical intervention, device on USB throughout: `scripts/cdc_flash.py <image> --slot b --stage-only`, then shell commands over CDC (pyserial), then `fwswap`.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| firmware running at start | the 2C23T port | `version` → `OpenScope 2C53T port v2026.07.1 app=0x08007000 usb=MSC+CDC` |
| cache table at start | A = port, B = an older coldtrace | `FWC A size=131148 crc=1624067e`, `FWC B size=604336 crc=bb211cbe` |
| new image is app-slot shaped | SP in SRAM, odd in-slot PC | vector `SP 20037FE0 PC 0802961D` |

## 4. Control

The same host tool, cable, chip and protocol, but through the **port's** loader (unchanged code), staging the new image into slot B — then the port's installer installing it.

| control | expected | measured | passed? |
|---|---|---|---|
| stage via the port's loader | `STAGED slot=b crc f7d8edfd` | `fwload: STAGED slot=b size=607012 crc=f7d8edfd`, 30.9 s (19 KB/s) | yes |
| `fwswap b` on the port | new build boots, CDC re-enumerates | back after ~21 s as `/dev/cu.usbmodem07BA2ED888261`; `version` → `Build: Sep  5 2026 23:23:43` | yes |
| new build reads the shared cache identically | A/B as above | `cache: A=131148/1624067E B=607012/F7D8EDFD` | yes |

## 5. Results

All on the new build unless stated.

| step | reading |
|---|---|
| `spi3 read` BEFORE fwload | 64 bytes, all `0x32–0x34` (idle baseline, CH1), i.e. a live capture |
| heap low-water before | `mem read 0x2002f620 2` → `00003370 00003370` = **13 168 B** min-ever-free and free, on the 31 KB heap |
| `fwload` 607 012 B into slot B through the **new** loader | **`STAGED slot=b 607012/607012 crc=F7D8EDFD err=none`**, 64.9 s (9 KB/s) |
| `fwstat` after | `STAGED`, `cache: A=131148/1624067E B=607012/F7D8EDFD` |
| `spi3 read` AFTER fwload | 64 bytes, all `0x32–0x34` again — the arena still serves its original tenant |
| heap low-water after | `00003370 00003370` — unchanged; the loader takes nothing from the heap, as designed |
| `SPI3 OK` counter across the session | 612 at 40 s uptime → 2 503 at 165 s — capture kept running |
| `fwswap a` — **the new installer**, reading the W25Q through the borrowed pointer | device back after ~23 s as `/dev/cu.usbmodem00012`; `version` → the port; `FWC A size=131148 crc=1624067e`, `FWC B size=607012 crc=f7d8edfd` |

Also read, not interpreted: the new build's `status` shows `STATUS(0x41): 00000000 … NOT configured` alongside `IDCODE anchor after close: silent (config port closed)` and a climbing `SPI3 OK` — the usual presentation of the bit-bang build once the config port has closed, same as EXP-203.

Host side, same tree: `scripts/run_tests.py` 13/13 suites, 95 tests; `test-fw-loader` (now including the no-buffer gate) and `test-config-persist` 22/22 pass; guest, app, guest-coldtrace and guest-bringup-bb all link.

## 6. Blind spots

1. The stream rate through this firmware's loader (9 KB/s) is half the port's (19 KB/s). It was 598 948 B in 67.3 s on 2026-08-25 through the old owned buffer, so this is not new and not the scratch change — but this session did not isolate where the time goes.
2. One unit, one chip (a clone), one image size. The manifest-last, silence-timeout and drain paths were not re-exercised; they do not touch the buffer's ownership, but they do run through it.
3. `spi3 read` is a 64-byte view of the arena's first tenant; `spi3 frame` (the second, 2 KB) was not run.
4. The heap low-water mark is from a shell-only session — no X-Y view entered, no settings save. 13 168 B is therefore a ceiling on how low it went here, not the build's worst case.
5. The `fwapply` path (install the just-staged slot) was not run; `fwswap a` exercises the same installer from the other slot.

## 7. Conclusion

- **Established:** with the intake buffer borrowed from `shell_bus_scratch`, the new loader stages a 607 KB image to `STAGED` with the at-rest CRC matching, and its RAM installer installs from the cache and comes back; the arena's other tenant reads a live capture before and after. Both cache slots read identically from both firmwares. The 31 KB heap never went below 13 168 B free in this session.
- **Excluded:** that lending the buffer breaks the install path (the device came back), or that it corrupts the shell task's own use of the arena (`spi3 read` unchanged).
- **NOT excluded (explicitly):** anything on the torn-transfer and timeout paths; `spi3 frame`; the heap low-water mark under X-Y or a settings save; why this firmware's stream rate is half the port's.
- **Follow-up:** re-measure the heap low-water mark after entering X-Y and saving settings before anyone takes more than this 1 KB; run `spi3 frame` after a `fwload` once.
