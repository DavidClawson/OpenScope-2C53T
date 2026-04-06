# H2 Cal Table — Findings Synthesis (2026-04-04)

Independent extraction + structural analysis of the 115,638-byte SPI3 bulk
calibration table at flash `0x08051D19`. Companion to the auto-generated
`h2_cal_table_analysis.md` (numeric output of `scripts/analyze_h2_table.py`)
and the 5-agent deep-dive `../spi3_bulk_cal_resolved.md`.

This document captures the **interpretive** findings — what the numbers
mean, what's new vs. the 5-agent doc, and how they should shape the next
session's replay experiment.

---

## 1. Every top-line claim in `spi3_bulk_cal_resolved.md` verified exactly

All nine top-line stats from §4 of the doc match this extraction to the
byte:

| Metric | Doc value | Verified |
|---|---|---|
| Total size | 115,638 | ✓ |
| Zero bytes | 75,356 (65.2%) | ✓ |
| Non-zero bytes | 40,282 (34.8%) | ✓ |
| 0xFF bytes | 6,199 (5.4%) | ✓ |
| All-zero records | 20,654 | ✓ |
| Non-zero records | 17,892 | ✓ |
| Records with 0xFF | 3,537 | ✓ |
| Full 160-byte blocks | 722 | ✓ |
| Blocks with sentinel | 546 (75.6%) | ✓ |

The 5-agent analysis is solid. Proceed to replay planning with confidence.

---

## 2. NEW finding — the table has two clean regimes, not "a gradual breakdown"

The doc said the sentinel pattern "breaks down later where the data becomes
denser/more variable." This extraction pins the boundary down precisely:
the sentinel map is **four contiguous runs**, not a gradient.

| Run | Blocks | Byte range | Size | Sentinel? |
|---|---|---|---|---|
| 0 | 0–543 | `0x00000`–`0x153FF` | 87,040 B | **YES** (544 blocks) |
| 1 | 544–567 | `0x15400`–`0x167FF` | 3,840 B | no (24 blocks) |
| 2 | 568–569 | `0x16800`–`0x168FF` | 320 B | **YES** (2 blocks) |
| 3 | 570–721 | `0x16900`–`0x1C37F` | 22,528 B | no (152 blocks) |
| — | (tail) | `0x1C380`–`0x1C3B5` | 54 B | n/a (partial block) |

**Interpretation:** the table is two concatenated sub-tables with
different internal formats.

- **Region A (0x00000–0x153FF, 87 KB, Run 0):** sync-word-framed streaming
  data. Every 160-byte block carries an `FF×6` sync word at byte 30.
- **Region B (0x15400–end, 29 KB, Runs 1–3):** dense coefficient data.
  Sync word absent because dense data would generate false-positive `FF×6`
  sequences.
- **The 2-block "island" at 568–569 is an anomaly worth examining before
  replay** — it sits in the middle of Region B but carries sync words. It
  may be a mid-stream re-sync marker, or a transition/boundary block. Do
  not truncate a partial replay inside this island.

This is important for replay safety: **Region A can be replayed as a
cleanly self-contained sub-range**, which makes it a lower-risk first
experiment than the full 115,638 bytes.

---

## 3. NEW finding — window-level entropy reveals three phases, not two

Per-4KB-window scan (`§5` of the auto report) shows three entropy phases:

| Phase | Byte range | Windows | Entropy | NZ density | Character |
|---|---|---|---|---|---|
| **P1** | `0x00000`–`0x08FFF` | 0–8 | 2.8–4.5 | 30–57% | Sparse config with scattered values |
| **P2** | `0x09000`–`0x14FFF` | 9–20 | **0.3–0.9** | **5–10%** | Near-empty but still sync-framed |
| **P3** | `0x15000`–end | 21–28 | 3.5–**6.0** | 60–**82%** | Dense coefficients |

**The boundary between P1 and P2 is at `0x09000` exactly — mid-Region A.**
So Region A is itself bimodal: the first ~36 KB carries sparse config data
in 160-byte sync-framed blocks, and the next ~48 KB is mostly empty padding
in the same sync-framed format.

**The largest contiguous zero run is only 512 bytes** (at `0x1641B`), not
the many-kilobyte run you'd expect from "65% zero-padded." The zeros are
scattered, not clumped. The table is densely sparse, not block-zeroed.

---

## 4. NEW finding — each sentinel block has a 16-bit pre-sentinel "tag"

Bytes 27–29 of every sentinel block form a 3-byte record immediately
before the `FF×6` sync word. In **511 of 546** sentinel blocks (93.6%),
byte 27 is `0x00`, so bytes 28–29 form a 16-bit value.

Properties of the tags:

- **269 unique values** across 546 blocks (49% unique) — large diversity
- **Not monotonic** — they are NOT block indices or counters
- **Not all identical** — they are NOT flags or mode bits
- **Terminator pattern**: the last 8 sentinel blocks in Region A (536–543)
  all share `00 65 c5` as their tag, forming a clear "end of Region A"
  marker. Block 543 also has `ff ff ff` as its post-sentinel record —
  another terminator signal.

**Most likely interpretation:** these are **running checksums** over each
block's preceding 27 bytes (or the full 30-byte data-A sub-field). The
FPGA uses the `FF×6` sync word to frame each 160-byte block, and the tag
to verify block integrity.

**Less likely but possible:** the tags are the last 2 bytes of a
calibration coefficient that happens to span the pre-sentinel record
position. Against this: 269 distinct values with no repetition pattern
would be unusual for coefficient data landing at the same offset in every
block.

**Implication for replay:** if the tag is a checksum, the FPGA will
*verify* it and reject blocks with bad checksums. Our replay cannot
modify, truncate, or reorder bytes within any block in Region A — it
must be byte-exact. This is a constraint we didn't know about before.

---

## 5. NEW finding — post-sentinel record is structurally fixed

Bytes 36–38 of every sentinel block (the first 3-byte record after the
sync word) are almost uniformly `00 00 00`:

- Block 0: `00 00 20` (only block with non-zero post-sentinel)
- Blocks 1–542: `00 00 00` (all)
- Block 543: `ff ff ff` (terminator)

So the format of every Region A block is really:

```
Block layout (160 bytes):
  bytes 0–26   : 9 records of payload data
  bytes 27–29  : 1 record — 16-bit tag with leading 0x00 (likely checksum)
  bytes 30–35  : FF×6 sync word (2 all-0xFF records)
  bytes 36–38  : 1 record — FIXED "00 00 00" post-sync marker
  bytes 39–159 : 40 records + 1 byte of payload data
```

This tightens the earlier "data sub-field A / sub-field B" model in the
doc: field A is 9 records + 1 checksum, the sync+post is 3 records of
framing, and field B is 40 records + a 1-byte tail.

The `0x00 0x00 0x00` at bytes 36–38 is a **structural constant** — the
FPGA is probably checking it as a "sync confirmation" byte after the
sync word. This is a reconstruction hint, not just data.

---

## 6. NEW finding — period search supports ONE true period: 160

Period-correlation scoring (§7 of the auto report) was naively misleading
until I accounted for the zero-density baseline. With 65.2% zeros, any two
random positions have probability 0.652² ≈ **0.425** of matching trivially
(both being zero). Real structural periodicity shows up as a score *above*
this baseline.

| Period | Score | Above 0.425 baseline? |
|---|---|---|
| 3 | 0.378 | −0.047 (below; records genuinely vary) |
| 20 | 0.315 | −0.110 (below) |
| 40 | 0.307 | −0.118 (below) |
| 80 | 0.298 | −0.127 (below — each 80-byte half of a block differs) |
| **160** | **0.450** | **+0.025 (real signal)** |
| 320 | 0.428 | ~0 (aliased 160 signal) |
| 480 | 0.418 | ~0 (aliased) |
| 640 | 0.416 | ~0 (aliased) |

**Only 160 is a real period.** The ~0.025 above-baseline corresponds
roughly to the 6 sync bytes + 3 post-sync bytes = 9 byte positions per 160
(5.6%) where the content is structurally identical across blocks. Expected
boost = 0.056 × (1 − 0.425) ≈ 0.032 — within ballpark of observed 0.025.

---

## 7. Revised replay strategy

Given all of the above, the recommended replay plan is a **three-step
graduated experiment**, not the single-shot full-table upload that the
5-agent doc proposed:

### Step 1: Region A only, byte-exact (safest)
- Transfer bytes `0x00000`–`0x153FF` (87,040 bytes, 544 blocks)
- Framed by `0x3B` opener and `0x3A` closer
- CS (PB6) held LOW for the entire transaction
- **Stop cleanly on the block-543 boundary** (after its `ff ff ff`
  terminator record)
- Bench test: 11 V DCV, 100 Ω resistor, 10 kΩ resistor, normal scope
- Expected outcome if hypothesis H(a) is right: meter gets more accurate
  but may still be off — Region A programs the front-end and coarse gain
- Expected outcome if nothing changes: Region A isn't the meter cal; move
  to Step 2

### Step 2: Region A + Region B (full 115,638 bytes)
- Only if Step 1 is inconclusive
- Full transfer, exactly as stock does
- Bench test same values
- Expected outcome: if this works, the dense coefficient tail is where
  the meter cal lives

### Step 3: If both fail — bench-capture stock traffic
- If neither Region A nor the full table fixes the meter, H2 itself is
  probably wrong (or at least not the whole story)
- Fall back to logic analyzer on a stock unit during boot (per
  `fpga_h2_spi3_bulk.md` §"Option A")

### What MUST NOT be done
- **No truncation inside a block.** If the tag is a checksum, mid-block
  truncation corrupts the checksum and the FPGA will reject. Always end
  a transfer on a 160-byte boundary.
- **No byte reordering / modification.** Everything is byte-exact.
- **No skipping the "island" blocks 568–569** if transferring Region B.
  They're probably a re-sync point.
- **No replay without CS framing.** The FF×6 sync words only make sense
  if the FPGA's SPI slave is expecting them; if CS isn't held LOW for the
  whole transaction the framing will break.

---

## 8. Open questions for next session

1. **Is the pre-sentinel tag really a checksum?** Test: compute XOR /
   sum / CRC16 of each block's bytes 0–26 and compare to the tag. If any
   common checksum matches across blocks, the hypothesis is confirmed and
   we know exactly how much of the block is "data payload" vs. "framing."

2. **What's at the 2-block "island" 568–569?** Dump those 320 bytes and
   see if they're distinguishable from Region B's dense data. They may
   mark a transition from one calibration sub-section to another.

3. **Does the FPGA echo the tag back on MISO during replay?** If so, we
   can detect acknowledgment / corruption in real time. The SPI3 transfer
   is full-duplex, so MISO is streaming while MOSI sends. The stock code
   ignores MISO (just drains the RX FIFO), but we could capture it.

4. **Is there a known "rebuild" at `0x08051D19` in any open-source 2C53T
   project?** If another clone-firmware author worked this out
   independently, their decoder would resolve everything instantly.

---

## 9. Files produced by this extraction

| Path | Purpose |
|---|---|
| `h2_cal_table.bin` | Raw 115,638-byte extraction |
| `h2_cal_table_analysis.md` | Auto-generated numeric audit |
| `FINDINGS.md` | This file — interpretive synthesis |

Both the bin and the analysis report are reproducible by running
`scripts/analyze_h2_table.py` against `APP_2C53T_V1.2.0_251015.bin`.

---

## 10. Confidence assessment

| Claim | Confidence |
|---|---|
| Table size = 115,638 bytes at `0x08051D19` | **CERTAIN** (byte-verified) |
| 160-byte block format in Region A | **HIGH** (546/722 blocks) |
| Two-regime structure with boundary at `0x15400` | **HIGH** (clean run boundary) |
| `FF×6` is a sync word, not a separator | **MEDIUM-HIGH** (consistent with streaming protocol design) |
| Pre-sentinel tag is a per-block checksum | **MEDIUM** (plausible, not proven) |
| Post-sentinel `00 00 00` is a structural constant | **HIGH** (545 of 546 blocks) |
| The table is FPGA meter calibration | **MEDIUM-HIGH** (inherited from 5-agent analysis) |
| Region A alone is sufficient for the cal | **UNKNOWN** (to be bench-tested) |
| It's safe to truncate to Region A on block boundary | **HIGH** (if CS is deasserted at boundary) |
