# Factory calibration — what exists, where, and how we know

Date: 2026-08-14
Scope: re-examination of a load-bearing belief ("irreplaceable per-device factory cal
lives on the W25Q") that was used to justify gating `SETTINGS_PERSIST_WRITES` OFF.

**No bench hardware was used.** Everything below is static analysis of binaries and dumps
already in the repo. Items needing the device are marked **ready for bench validation**.

---

## Verdict, up front

1. **Per-device factory calibration EXISTS.** It is **not** on the W25Q. It is a
   242-byte table inside stock's saved-settings page in **MCU internal flash at
   `0x08006000`**.

2. **We already have one unit's copy, and have had it since 2026-06-12 without knowing.**
   `archive/factory_iap_bootloader_2C53T.bin` was archived as "the factory IAP
   bootloader". It is `0x7000` bytes long, so it spans `0x08000000`–`0x08007000` — which
   **includes the settings page**. Its `0x6000` page is populated, signature `0x55`,
   sentinel valid, with a full calibration table. **It is git-tracked**, so the data is
   already safe in version control.

3. **That table is not any firmware's defaults.** Stock ships a compiled-in default table
   (§5). V1.0.7, V1.1.2 and V1.2.0 all carry the **identical** default value multiset;
   V1.0.3 has none. The recovered page shares only **7 of 120** values with it —
   coincidence level. Deltas are systematic: table A averages **−29**, table B **+77**,
   with **disjoint** ranges in three of four sub-tables.

4. **The shipped firmware cannot have produced those values.** An exhaustive whole-image
   scan (§6) shows the *only* code that writes this RAM region is `master_init`'s
   restore-or-defaults block. Nothing in stock computes it from a measurement. Non-default
   values therefore **must** have been seeded from outside the shipped firmware — i.e. at
   manufacture. **It is not reproducible by any firmware we hold.**

5. **The W25Q holds no calibration at all**, and now for a stronger reason than before:
   volume 0 is **bit-identical across every dump in the archive**, and 11 of our 13
   "dumps" turn out to be emulator artifacts derived from a single physical read.

6. **The gate should still open.** `SETTINGS_PERSIST_WRITES` governs W25Q writes only.
   The real asset is in MCU flash, which `config_save()` cannot reach. The justification
   for closing the gate was wrong, but the decision was harmless — and the *actual* risk
   it should have been pointing at was left unguarded until 2026-07-27.

**What is still open (n=1):** whether this table differs *between units* (true per-unit
trim) or is a constant factory table written to every unit. Both are "not in the firmware
image" and both make it worth preserving; only the first makes it strictly irreplaceable.
One more pristine unit's page settles it (§10).

---

## 0. Method and addressing

Addresses are **flash** addresses per the corrected convention in `CLAUDE.md`: the stock
app is linked at `0x08007000`, so file offset `N` = flash `0x08007000 + N`.

**The June 2026 notes this started from are in the LEGACY convention** (base `0x08000000`
= file offsets). `meter_w25q_calibration_boundary_2026_06_06.md` cites the sentinel at
`0x080261A8` and defaults at `0x080261BE..0x08026506`; those are legacy. The real flash
addresses are **`0x0802D1A8`** and **`0x0802D1BE..0x0802D506`**, inside `master_init`
(`0x0802AA50`) — verified by disassembly. Every address below was re-derived.

### Instrument failures found and fixed during this work

Per ground rule 6, these are recorded rather than quietly corrected:

- A regex that mis-split objdump's tab-separated columns made the extractor return
  **zero stores** from a range that visibly contains 28. Caught by hand-checking output.
- The next version silently missed **six `strd` instructions** and all `add.w`-computed
  base registers. The final extractor **reports unresolved stores**; its run resolved
  100% (0 unresolved), so coverage is measured, not assumed.
- Piping a binary to `objdump /dev/stdin` **silently produces no disassembly**. This
  poisoned a cross-version search that reported "signature not present in V1.0.3/V1.0.7"
  — a false negative. Caught only because the same code returned 0 for V1.2.0, where the
  answer was independently known. The fixed version added
  `assert "movw" in out`. **Any earlier conclusion drawn from that helper is void.**
- One `movw`+`movt` pair at `0x08009C10` initially looked like an absolute write into the
  cal region. Reading the instruction shows `movt r0,#16385` = `0x4001`, composing
  `0x40010414` (a peripheral), not `0x20000414`. Discarded.

Each of these would have produced a confident, stable, wrong answer.

---

## 1. The invented filenames — independently re-verified

Searched as raw bytes (`cal_ch1`, `cal_ch2`, `CAL_CH1`, `CAL_CH2`) plus the FAT 8.3 form:

| Image | `cal_ch*.bin` | `9999.bin` | `System file` |
|---|---|---|---|
| `w25q128_dump_2026_05_30.bin` (16 MB) | **0** | 8.3 form `9999    BIN` ×1 | LFN only |
| `APP_2C53T_V1.2.0` | **0** | 1 @ flash `0x080BC850` | 46 |
| `APP_2C53T_V1.0.7` | **0** | 1 | 46 |
| `APP_2C53T_V1.0.3` | **0** | **0** | 46 |

**Confirmed invented** — absent from every dump and every stock binary across three
versions. `meter_w25q_calibration_boundary_2026_06_06.md:167` said so on 2026-06-06 and
was cited as fact for two months anyway.

Incidental: `9999.bin` is absent from V1.0.3 and present from V1.0.7 — the same boundary
at which the default cal table appears (§5.3). The real string is
`3:/System file/9999.bin` at `0x080BC841`.

---

## 2. What is actually on the W25Q

Full FAT walk of `w25q128_dump_2026_05_30.bin`. A scan for `MSDOS5.0` and for `55 AA` at
every sector+`0x1FE` found **exactly two** boot sectors — no third volume, no raw region.

| | Volume 0 | Volume 1 |
|---|---|---|
| Base / size | `0x000000` / 2 MB | `0x200000` / 14 MB |
| FAT / root / data | `0x1000` / `0x3000` / `0x7000` | `0x201000` / `0x205000` / `0x209000` |

**Volume 0** — `System Volume Information/` (cluster 2) and `System file/` (cluster 5),
both directories, **confirming the stated claim**. Contents: **160 JPEGs** (135 JFIF +
25 Exif; all with valid SOI, complete EOI at exactly the declared size, intact chains),
Windows metadata (`WPSettings.dat`, `IndexerVolumeGuid`), and `9999.BIN`.

**`9999.BIN` — confirmed, raw directory entry** at `0x0000B380`:

```
39 39 39 39 20 20 20 20 42 49 4E 20 10 31 EF 5E 26 59 26 59 00 00 0B 58 F1 58 00 00 00 00 00 00
└──── "9999    " ────┘ └"BIN"┘ ^attr=0x20                          └clus=0┘ └── size=0 ────┘
```

Live entry (first byte `0x39`, not `0xE5`), **first cluster 0, size 0** — a well-formed
empty file, not corruption. **Nothing in it to decode, and never was.**

**Volume 1** — three directories (`LOGO/`, `Screenshot file/`, `Screenshot simple file/`),
**all empty, zero files**.

**Totals: 163 live files, 1,336,664 bytes, all in volume 0. Zero deleted entries** on the
whole chip (image-wide `0xE5` tombstone scan: 0 hits).

**Unreachable data:** 80.078% of the chip is `0xFF`. **1,586,757 bytes of written data
belong to no file** — JPEG remnants in freed volume-0 clusters, **~1.2 MB of orphaned
320×240 RGB565 BMP screenshots** at `0x214000`–`0x33DFFF`, and stale directory clusters.
**No calibration-shaped data; nothing resembling the 242-byte table.**

**Conclusion: the W25Q holds UI JPEGs, Windows metadata, one empty placeholder and
deleted-screenshot residue.** The UI JPEGs alone justify keeping stock's regions
READONLY, which is the rule the region layer actually implements.

---

## 3. The archive is one observation, not thirteen

**11 of 13 `archive/w25q128*.bin` files are emulator artifacts**, produced by
`scripts/unicorn_stock_flash_trace.py --dump-out`, whose `DEFAULT_DUMP` is
`archive/w25q128_dump.bin` — **the physical dump is the emulator's backing store**. The
`bcad2`/`bc859` names are stock *string-literal addresses*
(`0x80bcad2` = `"2:/Screenshot file/%d.bmp"`), not breakpoints.

All 55 pairwise diffs were computed. **Every differing byte lies at or above `0x200000`;
the count below `0x200000` is exactly 0 for all 55 pairs.** All variation is emulated
screenshot saves.

**Consequences:**

- **At most ONE independent physical W25Q observation exists.** The two physical dumps are
  byte-identical, equally consistent with two honest reads and with the May file being a
  copy of the April one. No per-read receipt exists.
- **Zero evidence of device-to-device W25Q variation** — we have never dumped a second
  device. `README.md:39` says the firmware is validated on one physical unit.
- **A third full dump existed and is gone.** `meter_w25q_calibration_boundary_2026_06_06.md:15-19`
  records `tmp/w25q-full-2026-06-06.bin`, sha256 `5706dcd9…` — a **different hash**, same
  board, orphaned BMPs at *different* offsets. `tmp/` no longer exists. That **contradicts**
  `w25q128_flash_map_2026-06-13.md:62` ("the W25Q never changes").

This does not change the calibration answer. It changes how hard the evidence may be
leaned on, and it should stop the archive being cited as a population.

---

## 4. The `0x08006000` MCU-flash hole

**It is stock's saved-settings page** — traced end to end in the binary.

### 4.1 The writer — the only in-application flash writer in stock

At `0x08029A48`:

```
8029a48:  mrs r1,PRIMASK / cpsid i           ; interrupts off
8029a4e:  movw r1,#0x0123 / movt r1,#0x4567  ; 0x45670123  FLASH KEY1
8029a52:  movw r2,#0x89AB / movt r2,#0xCDEF  ; 0xCDEF89AB  FLASH KEY2
8029a56:  movw r7,#0x6000 / movt r7,#0x0800  ; 0x08006000  target page
8029a66:  str r1,[r5,#-8] / str r2,[r5,#-8]  ; -> FLASH_KEYR  (0x40022004)
8029a6e:  str r1,[r5,#56] / str r2,[r5,#56]  ; -> FLASH_KEYR2 (0x40022044, bank 2)
```

`r5 = 0x4002200C` = `FLASH_STS`; `[r5,#4]` = `CTRL`, `[r5,#8]` = `ADDR`. Erase is textbook
STM32F1/AT32: `CTRL |= 0x02` (PGERS), `ADDR = 0x08006000`, `CTRL |= 0x40` (ERSTR), poll,
clear. Program loop at `0x08029AEC` writes **512 bytes** from a RAM staging buffer via a
halfword-program helper at `0x08035AFC`.

A preceding loop copies **2048 bytes** (`0x800` — the AT32F403A page size) from the page
and scans it; if already blank the erase is skipped. So this is **erase-if-needed at a
fixed address**, not an append log — which confirms `scripts/stock_settings.py`'s
fixed-offset model (language byte at `0x0800602C`) is correct.

> **Correction for `analysis_v120/stock_iap_bootloader.md`:** the claim that stock has "no
> in-app programming path at all" is right about *firmware update* but wrong as stated.
> Stock unlocks and programs its own flash — just this one page.

### 4.2 The struct

Staging buffer packed at `sl+0x00 … sl+0x12C`; restore reads `[r4,#0x00] … [r4,#0x12C]`
with `r4 = 0x08006000`. **Struct size `0x130` = 304 bytes**, matching
`STOCK_SAVED_CONFIG_SIZE`. `[0x00]` = signature (`0x55`/`0xAA`), then user settings
(language at `[0x2C]`), then **`[0x38]…[0x12C]` = the calibration table**, packed one
`uint32` per entry as `(B[i] << 16) | A[i]`.

Confirmed directly in the writer at `0x08029902`:

```
ldrh r2,[r7,#0x2B6]    ; A[43]  (meter_state + 0x2B6)
ldrh r3,[r7,#0x32E]    ; B[43]  (0x32E − 0x2B6 = 0x78 = 120 = table stride)
orr  r1,r2,r3,lsl #16
str  r1,[sl,#0xE0]
```

and in the restore at `0x0802D178`:

```
ldr  r1,[r4,#0x124] ; strh r1,[sl,#0x2D6] ; lsrs r0,r1,#16 ; strh r0,[sl,#0x34E]
```

i.e. struct word 59 → `A[59]` and `B[59]`. The two directions agree exactly.

### 4.3 The sentinel

```
0802d19e:  ldrh r0,[sl,#0x34E]    ; sl = 0x200000F8 -> RAM 0x20000446
0802d1b4:  cmp r0,#0xFFFF ; beq 0x0802d1be   -> write compiled-in defaults
0802d1b8:  cmp r0,#0      ; bne 0x0802d50a   -> keep restored values
```

**If the sentinel is `0xFFFF` (erased) or `0`, stock overwrites calibration with the
defaults compiled into its own image.** The sentinel is not a separate field: it is the
**last entry of the last sub-table**, and the defaults block writes it **last**, at
`0x0802D506` — a deliberate commit barrier.

---

## 5. The compiled-in default table

Range `0x0802D1BE`–`0x0802D50A`. 63 stores (28 `str.w`, 29 `strh.w`, 6 `strd`); **all 63
resolved, 0 unresolved**. Yields **109 halfword slots** spanning RAM
**`0x20000358`–`0x20000449`** — matching the June note's independently derived restore
span `0x20000358..0x2000044A`.

### 5.1 Layout

Two parallel `uint16` tables of 60 entries, stride `0x78` = 120 B, each split into two
30-entry halves by a sharp change in value regime:

| Sub-table | RAM | `ms[]` | Entries | Default range |
|---|---|---|---|---|
| A-low ("baselines") | `0x20000358`–`0x20000393` | `0x260` | 30 | 1620 – 1641 |
| A-high ("upper bounds") | `0x20000394`–`0x200003CF` | `0x29C` | 30 | 3247 – 3278 |
| B-low | `0x200003D0`–`0x2000040B` | `0x2D8` | 30 | 1591 – 1629 |
| B-high | `0x2000040C`–`0x20000447` | `0x314` | 30 | 3232 – 3271 |
| trailing halfword | `0x20000448` | `0x350` | 1 | 1530 |

4 × 60 B + 2 = 242 B → `0x20000358`–`0x2000044A`. ✅

**This independently explains a documented fact.** `CLAUDE.md` records the DAC formula as
using "upper bounds at `0x20000394`, baselines at `0x20000358`" — and `0x20000394` is
*exactly* where the regime changes from ~1620 to ~3255. `DAC = ((upper − base)/divisor) ×
(offset + 100) + base` is a linear interpolation between a baseline and an upper-bound
12-bit DAC code, indexed by range. B is the same structure for the second channel.

Character: ~1620 ≈ 39.6% and ~3255 ≈ 79.5% of 12-bit full scale; **ratio 2.009** — a 1×
and a 2× reference point. `B[i]/A[i]` ∈ [0.980, 1.000] across all 54 comparable pairs.

> **Resolves the `CLAUDE.md` / `CALIBRATION.md` disagreement** ("6 × 20 B" vs "12 × 20 B"):
> neither. It is 4 × 30 × `uint16` = 240 B + 2.

### 5.2 Unexplained

**12 of 122 halfword slots are never written by the defaults path** — A indices 20–25 and
B indices 36–41. They *are* written on the restore path (`0x0802CF5C`, `0x0802CF6A`,
`0x0802CF94`, `0x0802CFA2`, `0x0802D040`, `0x0802D04E`). A unit booting on defaults leaves
those 12 holding whatever was there. FNIRSI bug or unused entries — **not determined**.

### 5.3 The defaults are identical in every version that has them

Structural search (dense clusters of `movw` immediates in the two value families):

| Version | Default table | Value range |
|---|---|---|
| V1.0.3 | **none** | — |
| V1.0.7 | file `0x022B3E` | 1591 – 3318 |
| V1.1.2 | file `0x0260FA` | 1591 – 3318 |
| V1.2.0 | file `0x0261BE` (= flash `0x0802D1BE`) | 1591 – 3318 |

V1.0.7 vs V1.1.2: **ordered-identical**. V1.0.7/V1.1.2 vs V1.2.0: **multiset-identical**,
46 positional differences — the same values, reordered by register allocation between
builds. **Every firmware version that ships a default table ships the same one.**

---

## 6. Is any of it per-device? — the decisive evidence

### 6.1 Nothing in stock computes these values

The search was made exhaustive, not representative.

**All `str`/`strh`/`strb`/`strd` in the whole image** with an immediate offset in
`0x260`–`0x352`, filtered by base register:

- 177 in `0x0802CE`–`0x0802D4` — `master_init`'s restore + defaults blocks.
- 5 at `0x0808C3xx`, 5 at `0x08099Dxx` — `[sp,#…]`, stack frames. Not this struct.
- 4 at `0x0801E6xx` — base `0x20008350`, stored value `0x8C73` (RGB565), bounds 314/316
  (screen coords). **LCD drawing.** Not this struct.

**Plus the gap an offset scan cannot see** — absolute-address writers: (a) every 4-byte
aligned literal in the image landing in `0x20000358`–`0x2000044A` (12 hits, all constants
inside the FPGA bitstream blob or the round number `0x20000400` used as a scalar, none
loaded as a pointer here); (b) every `movw`+`movt` composing such an address — one
candidate, at `0x08009C10`, which actually composes `0x40010414`, a peripheral. Discarded.

**Result: `master_init` `0x0802CE00`–`0x0802D50A` is the only code in the entire stock
image that writes this region.** There is no ADC read, no averaging, no solve, no
self-calibration routine feeding this table. Furthermore §4.2 shows the *writer* sources
its bytes from the same RAM region the restore populated — a closed RAM ↔ flash loop.

**Therefore the shipped firmware can only ever persist either (a) the compiled-in defaults
or (b) values that were already in the page.**

### 6.2 A real unit's page contains neither the defaults nor anything the firmware could make

`archive/factory_iap_bootloader_2C53T.bin` (28,672 B = `0x7000`, git-tracked, committed
2026-06-12 in `22730c0`) spans `0x08000000`–`0x08007000` and therefore **contains the
settings page** at its `0x6000` offset. Captured by `scripts/dump_factory_bootloader.py`
via `mem read` over our own USB CDC shell — so from a unit running OpenScope, with the
factory IAP bootloader intact.

The page is populated, and every structural prediction of §4–§5 holds:

- byte 0 = `0x55` — valid "normal restore" signature
- struct `[0x38]…[0x12C]` = the calibration table, **regime change from `0x06xx` to
  `0x0Cxx` at exactly struct offset `0xB0` = entry index 30** — precisely the sub-table
  boundary predicted in §5.1
- sentinel `B[59]` = `ms[0x34E]` = **`0x0CD9` = 3289** — valid, so stock never applied
  defaults on this unit
- `[0x130]`–`[0x1FF]` = RAM garbage (stack addresses `0x2002DF7C`, `0x080374F1`…), exactly
  as predicted by "the writer programs 512 bytes but the struct is only 304"
- `[0x200]`–`[0x7FF]` = **all `0xFF`**, exactly as predicted by "erase 2 KB, program 512 B"

Five independent structural predictions confirmed on data the model was not derived from.

**Comparison against the compiled-in defaults:**

| | identical to default | mean delta | min | max |
|---|---|---|---|---|
| Table A (54 comparable) | **4 / 54** | **−29.4** | −77 | +8 |
| Table B (54 comparable) | **0 / 54** | **+77.4** | +28 | +115 |

| Sub-table | recovered | default | overlap |
|---|---|---|---|
| A-low | 1598 – 1628 | 1620 – 1641 | partial |
| A-high | 3195 – 3221 | 3247 – 3278 | **disjoint** |
| B-low | 1696 – 1716 | 1591 – 1629 | **disjoint** |
| B-high | 3283 – 3306 | 3232 – 3271 | **disjoint** |

Against **every** firmware version's default immediates, the recovered 120 values overlap
in only **7** — coincidence level for values clustered in narrow bands.

### 6.3 The conclusion

The values are not the defaults of V1.0.7, V1.1.2 or V1.2.0 (§5.3 — all the same table),
and V1.0.3 has no table at all. The shipped firmware cannot compute them (§6.1). The
sentinel is valid, so the defaults path was never taken on that unit.

**⇒ The table was written by something other than the shipped firmware — a factory
calibration step. It is per-device data and it is not reproducible by any firmware we
hold.**

### 6.4 What this does NOT establish

**n = 1.** We have exactly one such page. "Differs from the defaults" is proven;
**"differs between units" is not directly observed.** Two readings remain open:

- **(a) true per-unit trim** — each unit measured at manufacture. Then the page is
  strictly irreplaceable and losing it permanently degrades that unit.
- **(b) a constant factory table** — the same non-default table written to every unit by
  the production tool. Then it is recoverable from any one unit, and the copy we already
  hold would serve as a golden reference for all.

The systematic per-table offsets (A −29, B +77) are equally consistent with both. **Both
make the page worth preserving; only (a) makes it irreplaceable.** One more pristine
unit's page distinguishes them (§10).

---

## 7. Why we cannot answer §6.4 from our own hardware

Our app's linker script leaves the page alone:

```
FLASH_VEC : ORIGIN = 0x08004000, LENGTH = 8K   ; ends exactly at 0x08006000
FLASH_APP : ORIGIN = 0x08007000, LENGTH = 932K ; resumes after the page
```

and the flashers pass `--preserve-blank-blocks-range 0x08006000:0x08007000` /
`--preserve-blank-pages-range 0x08006000:0x08007000` (`firmware/Makefile:378,383,398`).

**That protection landed in `f1b893e`, "tools: preserve stock settings across OpenScope
flashes", on 2026-07-27** — later than every flash of bench units #1 and #2 before that
date.

The 2026-06-12 dump proves the page **survived** our flashing on at least that unit, which
**refutes** `w25q128_flash_map_2026-06-13.md:76-79` — "`0x08006000` is overwritten on
both, and we hold **no MCU-flash readout**". That document was written **one day after**
the readout it says we do not have was committed. The pessimism was wrong and it cost two
months: the answer was in the repo the whole time, filed under a name that did not mention
settings.

What remains true is that this is one unit's page, and later flashes on either unit —
before the preserve flag — may have destroyed the other. **Ready for bench validation:**
re-dump `0x08006000`–`0x080067FF` from both bench units and compare to the archived copy.
If bench unit #1 still matches, we have a second confirmed observation for free.

---

## 8. Recommendation on `SETTINGS_PERSIST_WRITES`

### **Open the gate.** Set `SETTINGS_PERSIST_WRITES=1`.

The stated justification — "irreplaceable per-device factory cal on the W25Q" — was wrong
in both halves: the filename cited does not exist, and **the calibration is not on the
W25Q at all**. The real asset is in MCU flash, which this gate does not govern.

**The writable window is empty.** Measured on `w25q128_dump_2026_05_30.bin`:

| Region | Range | Bytes | `0xFF` | Other |
|---|---|---|---|---|
| `usercal` | `0xF00000`–`0xF0FFFF` | 65,536 | **100.00%** | 0 |
| `settings` | `0xF10000`–`0xF1FFFF` | 65,536 | **100.00%** | 0 |
| `modules` | `0xF20000`–`0xF9FFFF` | 524,288 | **100.00%** | 0 |
| `scratch` | `0xFA0000`–`0xFFEFFF` | 389,120 | **100.00%** | 0 |
| **window total** | `0xF00000`–`0xFFEFFF` | **1,044,480** | **100.00%** | **0** |
| `tail` (READONLY) | `0xFFF000`–`0xFFFFFF` | 4,096 | 0% | 4,096 zero bytes |

Not one written byte. The programmed `tail` sector is correctly left READONLY.

**The code cannot reach outside it.** `config_save()` (`firmware/src/util/config.c:196-266`)
only calls `flash_region_append(CONFIG_REGION, …)` with `CONFIG_REGION` =
`FLASH_REGION_SETTINGS` at `0xF10000`. Its single erase is `flash_region_reset(CONFIG_REGION)`,
a bounded reset of our own append region, taken only on `ERR_FULL`. On `ERR_NEEDS_ERASE`
it **refuses and surfaces the status** rather than erasing. Bounds are enforced by address,
with `sysvol` and `uservol` READONLY.

**Conditions and residual risk:**

1. **The window is erased on the one unit we dumped.** A user's unit could differ.
   Quantified: volume 1's data area starts at `0x209000`, so stock must allocate
   **13,594,624 bytes ≈ 88 screenshots** (153,654 B each) before reaching `0xF00000`.
   Reachable for a heavy user — so the collision is real, not theoretical. But the
   direction is safe: **stock can clobber us; we can never clobber stock.** Worst case we
   lose our own settings.
2. **The 2026-06-06 hash discrepancy (§3) is unexplained.** It suggests the W25Q has
   changed at least once on one unit. That does not touch the write path, but the claim
   "the chip never changes" should stop being asserted.
3. **This would be the first-ever write to this chip from our firmware.**
   **Ready for bench validation:** enable the gate, save settings, power-cycle, confirm
   restore, dump the full chip and diff against `archive/w25q128_dump_2026_05_30.bin` —
   the diff must be confined to `0xF10000`–`0xF1FFFF`. That converts every claim in this
   section from static to observed.

**Re-scope task C1b before doing it.** As written it says "read `cal_ch1.bin` and
`cal_ch2.bin`" — those files do not exist and the task cannot be executed. The thing that
actually needs backing up is **MCU flash `0x08006000`–`0x080067FF`**, and §6 shows why.
Note the ordering argument in the dev plan is now inverted: the C1b backup was framed as a
prerequisite for opening the gate, but the gate touches the W25Q and the backup concerns
MCU flash. **They are independent.** The gate need not wait for it; the backup should
happen anyway, and sooner, for a better reason.

---

## 9. What needs protecting that nobody is protecting

**Already protected (better than assumed):**

- **MCU flash `0x08006000`–`0x08006FFF`** — protected twice: the linker leaves it
  unmapped, and `scripts/flash_preflight.py` *enforces* it with a dedicated check ("Ensure
  an app-slot image leaves the stock settings page unprogrammed"). A real enforced guard,
  not a convention.
- **The whole W25Q below `0xF00000`** — READONLY by address in `flash_region_table`.
- **One unit's factory calibration** — already in git, in
  `archive/factory_iap_bootloader_2C53T.bin`. Safe, but **only by accident**, and
  undocumented until now.

**Gaps, in priority order:**

1. **The archived page is not identified as calibration anywhere.** A future cleanup that
   reads "factory IAP bootloader dump" could reasonably truncate it to the 24 KB
   bootloader and silently destroy the only per-device calibration record we possess.
   **Highest-value, lowest-cost fix: document it, and extract the page to its own named
   artifact.** Recommended: `archive/stock_saved_config_unit1_2026-06-12.bin` (2 KB),
   with the sha256 of the parent recorded.
2. **No firmware path reads, backs up, or restores that page.** A user flashing OpenScope
   keeps their page only because of a guard added 2026-07-27. There is no code that copies
   it anywhere, and no restore path if it is lost.
3. **The preflight guard has no test asserting it fires.** Per ground rule 6, a check never
   shown to fail when it should is not yet a check. Feed it an image that *does* program
   `0x08006000` and require rejection.
4. **`0x08003800` (2 KB upgrade-flag sector)** is named in `CLAUDE.md`'s flash layout but
   appears in no region table and no preflight rule. In the archived dump its
   `0x3800`–`0x4000` range is largely written (1,858 non-`FF`/non-zero bytes), so it is not
   empty. Undocumented; worth a look.
5. **`tail` sector `0xFFF000`** — 4,096 zero bytes, purpose unknown, correctly READONLY.

---

## 10. What would settle the remaining question

One measurement decides §6.4, and it does not need our bench:

**Dump `0x08006000`–`0x080067FF` from a second unit** (ideally pristine), and compare its
calibration table to the archived one. `scripts/dump_factory_bootloader.py` already does
exactly this over USB CDC and needs no case-crack — though it requires our firmware for
the shell, so a pristine-unit read wants the factory IAP or ROM DFU read path instead.

- **Identical tables** ⇒ reading (b): a constant factory table. The copy we hold is a
  golden reference for every unit; nothing is irreplaceable.
- **Different tables, both differing from the §5 defaults** ⇒ reading (a): true per-unit
  trim. Every user must back up their page **before** flashing anything, and that becomes
  a release-blocking requirement.

Cheap, needs no hardware we lack, and a good ask for issue #12 or #18 — the same channel
that produced the FPGA breakthrough.

Secondary: re-dump both bench units (§7); resolve the 12 unwritten slots (§5.2); decode
the physical meaning of the A/B sub-tables against the known DAC formula.

---

## Appendix — corrections to existing docs

| Document | Correction |
|---|---|
| `analysis_v120/w25q128_flash_map_2026-06-13.md:76-79` | "we hold no MCU-flash readout" and "`0x08006000` is overwritten on both" are **refuted** — the readout was committed one day earlier and the page is intact in it. |
| `analysis_v120/w25q128_flash_map_2026-06-13.md:62` | "the W25Q never changes" is contradicted by the 2026-06-06 dump's differing hash. |
| `docs/dev_plan_2026-08-13.md` §C1b | Task is not executable as written (`cal_ch1.bin`/`cal_ch2.bin` do not exist). Re-scope to MCU flash `0x08006000`; it is **independent of** the `SETTINGS_PERSIST_WRITES` gate, not a prerequisite. |
| `analysis_v120/meter_w25q_calibration_boundary_2026_06_06.md:149-162` | Addresses are LEGACY; flash equivalents are `0x0802D1A8` and `0x0802D1BE..0x0802D506`. Structural claims otherwise confirmed. Its conclusion "does not prove those values are recovered DMM physical coefficients" is now **superseded**: they are not the defaults, and the firmware cannot compute them. |
| `analysis_v120/stock_iap_bootloader.md` | "stock has no in-app programming path at all" is too broad — stock programs `0x08006000` (§4.1). True for firmware *update*. |
| `analysis_v120/master_init_phase4.c:521-566` | Layout broadly right; the table is **4 × 30 `uint16` sub-tables**, not "40 × uint32", and the sentinel is the last sub-table entry, not a separate field. |
| `CLAUDE.md` calibration bullet | "6 × 20 B" vs `CALIBRATION.md`'s "12 × 20 B" — neither. 4 × 30 × `uint16` = 240 B + 2, `0x20000358`–`0x2000044A`. Also: "loaded from SPI flash at boot" is wrong; it is restored from **MCU** flash `0x08006000`. |
| `scripts/stock_settings.py` | Model confirmed. Worth adding: 2 KB page, erase-if-needed, 512 bytes programmed, and the calibration table at `[0x38]…[0x12C]` which the current field list does not model. |

## Appendix — reproduction

Scratchpad scripts (one-shot analysis, not committed):

- `dis.sh` — objdump wrapper applying `--adjust-vma=0x08007000`
- `defaults2.py` — immediate-tracking extractor; **reports unresolved stores**
- `hunt.py` / `hunt2.py` — cross-version default-table location and comparison
- `compare.py` — recovered page vs compiled-in defaults

Every table in §5, §6 and §8 is reproducible from `archive/` contents alone. The single
most important input is `archive/factory_iap_bootloader_2C53T.bin` at offset `0x6000`,
length `0x800`; parent sha256
`0c9ec7d642d233ea09c87274867ad3460e1dbec37c4332f7edb8f836175630c7`.
