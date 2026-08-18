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

## `unit_maksidze_mcu_settings_page_0x08006000.bin` — CROSS-UNIT COMPARISON, 2026-08-16

4096 bytes, same region, from **a different physical 2C53T** (maksidze, issue #18),
dumped with his own MCU-memory reader. sha256 `682069379cf36c6e282ee0afd971348f…`.

This is the comparison `analysis_v120/factory_cal_truth_2026-08-14.md` was blocked on.
The standing caution was that every unit *we* had dumped had already been reflashed by
us, so "absent in our dumps" could not be distinguished from "absent on a pristine
unit". A second, independently dumped device settles it.

**229 of 4096 bytes differ, and they fall in exactly three regions — the same split
the live-hardware verification above established:**

| Region | Offsets | Differs | Reading |
|---|---|---|---|
| header / settings | `0x003–0x012` | 16 B | per-unit settings, expected |
| **CALIBRATION** | `0x025–0x12E` | **266 B** | **per-device values — see below** |
| garbage tail | `0x180–0x1FB` | 124 B | uninitialised staging buffer, means nothing |
| erased | `0x1FC–0xFFF` | **0 B** | 3580 bytes of `0xFF`, byte-identical |

**The calibration region is structured, not random.** It is an array of 16-bit LE
pairs, in two groups whose values are plausible 12-bit DAC codes:

| | group 1 (low, high) | group 2 (low, high) |
|---|---|---|
| unit #1 | 1598–1628, 1696–1717 | 3195–3221, 3283–3306 |
| maksidze | 1639–1667, 1657–1705 | 3239–3268, 3230–3290 |

Same layout, same clusters, different numbers — with unit #1 sitting systematically
~30 codes below maksidze's and showing about twice the spread. That is what
per-device factory calibration looks like, and it is not what a compiled-in default
table looks like (the stock image's defaults are large values of a different shape).

The pair structure matches the documented meter DAC path exactly: `CLAUDE.md` records
cal tables at RAM `0x20000394` (upper bounds) and `0x20000358` (baselines), 2 bytes per
entry, combined as `DAC = ((upper - baseline) / divisor) * (offset + 100) + baseline`
and written to `0x40007408`. Pairs of (baseline, upper) per range is precisely the
shape seen here.

**Conclusion: per-device factory calibration DOES exist on this hardware, it lives in
MCU internal flash at `0x08006000`, and it is confined to roughly `0x025–0x12E`.**
Two independent units agree on layout and disagree on values. This is also the region
our own app overwrites, so it must be preserved or archived before flashing a unit
that has never been dumped.

## THIRD UNIT + ARRAY ALIGNMENT SETTLED — 2026-08-17

`unit_dendi_mcu_settings_page_0x08006000.bin` — a third unit, dumped by its owner and relayed
by maksidze (issue #18, 2026-08-16). Diffs against the other two:

| pair | differing bytes | of which in cal 0x025-0x12E |
|---|---|---|
| unit1 vs maksidze | 229 | 130 |
| unit1 vs dendi    | 268 | 161 |
| maksidze vs dendi | 184 | 162 |

All three differ pairwise across essentially the whole calibration region while the erased
remainder (0x1FC-0xFFF) stays byte-identical `0xFF`. Per-device factory calibration is
established beyond the two-unit case.

**Alignment/endianness pinned.** The array is little-endian `uint16` starting at page offset
**0x026**. At that alignment 129/132 entries land inside 12-bit DAC range; the other three
candidate alignments give ~7/132. (An earlier writeup quoted the same cluster values from a
big-endian-at-0x025 read — coincidentally similar, but 0x026/LE is the correct parse.)

**Structure: two blocks of 30 (baseline, upper) pairs.**

| block | offsets | unit1 | maksidze | dendi |
|---|---|---|---|---|
| 1 | 0x038-0x0AE | 1598-1628 / 1696-1717 | 1639-1667 / 1657-1705 | 1509-1676 |
| 2 | 0x0B0-0x126 | 3195-3221 / 3283-3306 | 3230-3290 | 3145-3282 |

30 entries per table matches the decompiled RAM cal tables exactly — baselines at `0x20000358`,
upper bounds at `0x20000394`, delta 0x3C = 60 bytes = 30 x uint16. Block 2 sits at roughly 2x
block 1.

Dendi's page carries a handful of out-of-range outliers (62808, 50862, 27857, 27887, 31944)
scattered through otherwise well-formed entries; not yet explained, and worth keeping in mind
before treating any single unit's page as a clean reference.
