# EXP-28 — the `cdc-fwload` + `main` merge boots on unit #2; cal backup and the heap-allocated X-Y buffers measured

- **Date:** 2026-08-26
- **Unit:** bench unit #2 (Stlkv), factory bootloader
- **Build:** `make guest-coldtrace` at merge commit `ea932d7` (this branch merged with `main` at `857802e`). 604 336 B, crc32 `BB211CBE`, sha256 `2f9698fe…`, banner `Build: Aug 26 2026 08:34:54`.
- **Status:** CONFIRMED

## 1. Problem

The merge of upstream `main` into `cdc-fwload` links and passes every host suite, but nothing about it had run on hardware. Three of the merged changes cannot be settled on the host: `cal_backup` writes a brand-new W25Q region on a chip whose write path only started working on 2026-08-25 (`479d120`), the X-Y snapshot buffers moved from `.bss` to the FreeRTOS heap in `6880b96` and had never been exercised in X-Y view, and `main` retired `measurement.c` in favour of `scope_measure.c` with a rise/fall measure and a true-scale graticule. PR #29 claims every behaviour it delivers is measured; after the merge that claim covers a tree no unit had booted.

## 2. Hypothesis

If the merged tree is sound, then: the image installs from a cache slot and boots (no watchdog reset, no dark device); `cal backup` writes a valid record into `calbackup` (0xFFD000) whose payload CRC equals the live MCU page CRC while the live page itself is untouched; and entering X-Y allocates exactly two 1 024 B blocks from the 32 KB FreeRTOS heap, once, leaving the pointers non-NULL and the device running. If the heap path is wrong instead, the pointers stay NULL (degrade-to-no-snapshot) or the device faults; if `cal_backup` cannot write, `cal status` keeps reporting `no backup (bad magic)`.

## 3. Procedure

Host: `23t/tools/cdc_bench.py flash <image> --slot b` (the port's own tool driving the upstream protocol), then `fwswap b`, then commands over the CDC shell of the installed firmware. No physical intervention; the device stayed on USB throughout.

**Preconditions verified by readback** (not assumed):

| what | expected | measured |
|---|---|---|
| firmware before the run | the 2C23T port | `OpenScope 2C53T port v2026.07.1 app=0x08007000 usb=MSC+CDC` |
| slot A (the fallback image) | port image, bootable | `A=131148/1624067E` |
| staged slot B at rest | 604 336 / `BB211CBE` | `FWC B size=604336 crc=bb211cbe` |
| banner of the booted image | the image built this session | `Build: Aug 26 2026 08:34:54` |
| UI mode while testing X-Y | oscilloscope (0) | `ui dump` → `mode=0` |

X-Y was entered by writing the view selector directly — `mem write 0x20000594 0x00000004` (`scope_view`, symbol from the ELF of the installed image; `SCOPE_VIEW_XY` = 4) — because the shell has no view command and the UI path is a button press. The word is read-modify-written whole; its other three bytes (`persist_enabled`, `math_op`, `math_enabled`) read 0 before and were written back as 0. Restored to `0x00000000` afterwards, verified by readback.

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| the streaming path this branch owns | STAGED with matching CRC | `fwload: STAGED slot=b size=604336 crc=bb211cbe`, 18.5 s, 32 KB/s | yes |
| `cal status` before `cal backup` | live page programmed, no backup | `live … programmed crc=0xDFD5381C` / `backup: no backup (bad magic)` | yes |
| heap counters before X-Y | free == min-ever free (nothing has dipped) | both `0x3770` = 14 192 B | yes |
| X-Y pointers before X-Y | NULL | `xy_xs=0`, `xy_ys=0` | yes |

The "no backup" line is what makes the backup result meaningful: the region was empty at the start of this session, so what `cal status` reports afterwards was written during it.

## 5. Results

#### The image boots and stays up

`fwswap b` installed from the cache and reset into the image; the shell answered on re-enumeration and uptime reached 633 s across the whole session with no reset. Note the port re-enumerates under a different device node than the port firmware (`/dev/cu.usbmodem07BA2ED888261` vs `…00012`) — a host-side detail worth knowing, not a device fault.

#### `cal backup` — first successful run on unit #2

`cal backup` → `ok`. `cal status` afterwards:

```
live  @0x08006000  programmed     crc=0xDFD5381C
backup: valid v1  payload_crc=0xDFD5381C  src=0x08006000
match : yes (live == backup)
```

Read back at the raw level, `flash read 0xFFD000 32`, the record header is `42 43 41 4C` (`BCAL`) / `01` / len `0x00001000` / src `0x08006000` / crc `1C 38 D5 DF` — the same `0xDFD5381C`, little-endian. The live MCU page CRC is unchanged from every earlier reading on this unit.

#### X-Y snapshot buffers on the heap — the numbers

| reading | before X-Y | after X-Y | delta |
|---|---|---|---|
| `xFreeBytesRemaining` | `0x3770` = 14 192 B | `0x2F60` = 12 128 B | **−2 064 B** |
| `xMinimumEverFreeBytesRemaining` | `0x3770` | `0x2F60` | tracks the same drop, no deeper dip |
| `xy_xs` | `0x00000000` | `0x20033CC8` | allocated |
| `xy_ys` | `0x00000000` | `0x200340D0` | allocated |
| `ui_scope_full_draws` | 1 | 27 | X-Y forces full draws, so the view is rendering |

−2 064 B is exactly 2 × (1 024 payload + 8 B heap_4 block header), and the two pointers are 0x408 = 1 032 B apart, i.e. adjacent blocks. `FPGA_ADC_BUF_SIZE` is 1 024. The allocation is gated on `have`, which requires `fpga_data_ready()`, so the snapshot copy ran with real capture data behind it, not on an empty frame. 12 128 B of heap remain.

`screen dump 140 116 40 24` over the plot area shows the X-Y axes and a gridline drawn (a solid column of `F` and a `3FFF`/`E0E0` row); with no signal on either channel there is no figure to see. Screen observation, weak evidence, recorded only as "the view draws".

#### The retired-`measurement.c` path answers, on its refusal branch

`fpga scope measure` prints the new fields — `rise1_smp100`, `fall1_smp100` — as `-` on all five rows, alongside `pp1=3` (noise) and `k1_uV=0` for range 3. That is the honest-refusal branch: an uncalibrated range and a signal-free input produce no number rather than a fabricated one. `fpga scope graticule` → `autofit (grid == position only)`; `fpga scope softtrig` → `ON (trace locked to trigger) (level=0 px, rising, src=CH1)`.

## 6. Blind spots

1. **No signal source was attached** (no ESP32 on the host's serial bus this session), so every measurement-side number is a refusal, not a value. Rise/fall, the true-scale graticule against a known amplitude, and an actual Lissajous figure are untested; this run says the paths execute and degrade honestly, nothing about their correctness.
2. **X-Y was entered by poking RAM**, not by the button that a user would press. The draw path and the allocation are the same, but the input path around them is not covered.
3. **One X-Y session only.** "Allocated once and kept" is the code's claim; a single entry cannot show that re-entering does not leak. The counter-evidence available is weak — min-ever-free equals current free, so nothing has been freed and re-taken.
4. **The allocation-failure branch is unexercised.** With 14 KB free the `pvPortMalloc` returning NULL path never ran; that it degrades to "no snapshot" rather than dereferencing NULL remains a code reading.
5. **`cal restore` was deliberately not run** — it writes MCU flash at 0x08006000. Only `cal status` and `cal backup` (W25Q-only) are covered. Auto-restore is compile-gated off and, when built in, acts only on a blank or zeroed live page (`cal_backup_should_auto_restore`), so it was never a risk here — code reading, not measured.
6. Meter and scope telemetry beyond the commands above were not re-checked on this build.

## 7. Conclusion

- **Established:** the merged tree installs from a W25Q cache slot, boots and runs on unit #2; `cal_backup` writes and validates a factory-cal mirror on this unit's Zbit clone, with the live page byte-identical by CRC before and after; the X-Y snapshot buffers allocate from the FreeRTOS heap for exactly 2 064 B, once, leaving 12 128 B free and the view rendering.
- **Excluded:** that the RAM-ceiling resolution in `6880b96` broke the X-Y path, and that the new cal-backup region is unreachable on a unit whose write path needed `479d120`.
- **Open:** everything that needs a signal — rise/fall values, graticule scale, a real Lissajous — plus the button-driven X-Y entry and the malloc-failure branch.
