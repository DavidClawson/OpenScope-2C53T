# Factory calibration — recovered MCU settings page

## `unit1_mcu_settings_page_0x08006000.bin`

4096 bytes. MCU internal flash `0x08006000–0x08006FFF` from **bench unit #1**, stock's
saved-settings page. Extracted 2026-08-14 from `../factory_iap_bootloader_2C53T.bin`,
which is `0x7000` bytes long and therefore spans `0x08000000–0x08007000` — the settings
page is inside it. That file was committed 2026-06-12 under a name that says
"bootloader", so nobody realised it also carried the settings page.

```
sha256  6004374abb123b99aa8a2516f66b0f6465032e817ebd3fb463c2559accdf621f
crc32   59E91404
```

Verified content: **513 non-`0xFF` bytes**, signature byte `0x55` at offset 0, data
through `0x803`, the rest erased. Values are small clustered integers of calibration
shape (`1701, 1626, 1700, 1628, 3201, 3304, …`), not the large values found in the
stock image's compiled-in default table.

## ✅ VERIFIED AGAINST LIVE HARDWARE — 2026-08-14

`make guest-caldump` embeds this file and diffs it against the live page on bench
unit #1. Result, read off the device:

| Region | Offsets | Differs | Meaning |
|---|---|---|---|
| header / settings | `0x000–0x02F` | ~15 / 48 | stock rewrote its settings — expected |
| **CALIBRATION** | `0x030–0x12F` | **0 / 256** | **byte-identical to this file** |
| tail (RAM garbage) | `0x130–0x1FF` | ~124 / 208 | uninitialised stack in stock's 512 B staging buffer; differs on every write, means nothing |

Whole-page diff was 139 bytes across `0x005–0x1FB`, which looked alarming until it
was split by region. **The calibration did not move.** Unit #1 still holds its factory
values, this file is a byte-exact copy of them, and stock never overwrote them with
its compiled-in defaults during the July/August bench sessions.

Note the method: the whole-page byte count conflated a meaningless garbage tail with
the data that matters and would have supported either conclusion. The per-region split
is what made it a measurement rather than an argument.

## Why this file exists separately

**This may be the only per-device factory calibration record this project possesses,
and it was one careless cleanup away from being lost.** A tidy-up that truncated the
bootloader archive to its nominal 24 KB would have destroyed it silently — the file
would still be a valid bootloader image, and nothing would have flagged the loss.
Copying it out under a name that says what it is removes that failure mode.

**Do not delete, truncate, or "clean up" either this file or
`../factory_iap_bootloader_2C53T.bin`.** They are not regenerable: no firmware we hold
computes these values from a measurement (an exhaustive scan of the stock image found
`master_init` to be the only writer of that RAM region), so if the physical page is ever
erased there is nothing to restore it from except this copy.

## What is established, and what is not

**Established:** the page is populated; its values differ from the defaults compiled into
the stock image at `0x0802D1BE–0x0802D506`; stock falls back to those defaults when the
page's sentinel is erased or zero; and nothing in any firmware we have reconstructs the
recovered values.

**NOT established — n = 1.** "Differs from the compiled-in defaults" is proven. "Differs
*between units*" is not. These could be a per-unit trim written at manufacture, or one
constant factory table flashed to every device. Only a second unit's page decides it, and
that distinction is the whole question of whether this data is irreplaceable.

`make guest-caldump` reads this page on a live unit and prints its CRC32 — read-only, no
case opening. Two never-flashed units reporting the *same* CRC means a constant table;
different CRCs means per-device trim. See `firmware/src/util/cal_dump.h`.

## Caveat on the archive generally

`archive/` holds 13 W25Q images but they are **one observation, not thirteen** — 11 of the
13 are emulator artifacts derived from a single physical read. Sample size in this project
is usually smaller than the file count suggests.

## Layout

Not yet decoded. Current reading: the stock RAM table it feeds
(`0x20000358–0x2000044A`, 242 bytes) is **4 sub-tables of 30 `uint16`**, with the regime
change at `0x20000394` — which is the address the documented meter DAC formula already
used for "upper bounds". Decoding is required for a *re-calibration* feature; it is **not**
required to back this up or restore it byte-for-byte.
