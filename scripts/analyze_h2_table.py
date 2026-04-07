#!/usr/bin/env python3
"""
H2 bulk cal table extractor + structural analyzer.

Reads the 115,638-byte table at file offset 0x51D19 from the stock V1.2.0
binary, saves it as a standalone .bin, and runs an independent structural
audit to verify / extend the findings in
reverse_engineering/analysis_v120/spi3_bulk_cal_resolved.md.

This exists so the replay experiment (next session) starts from a clean,
verified understanding of the bytes rather than the disassembly-only view.
"""

import math
import os
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BIN = REPO / "archive" / "2C53T Firmware V1.2.0" / "APP_2C53T_V1.2.0_251015.bin"
OUT_DIR = REPO / "reverse_engineering" / "analysis_v120" / "h2_extracted"

FILE_OFFSET = 0x51D19          # table start in the flash image
FLASH_ADDR  = 0x08051D19       # MCU flash address
TABLE_SIZE  = 0x1C3B6          # 115,638 bytes
RECORD_SIZE = 3                # transmission unit
BLOCK_SIZE  = 160              # independent structural dimension
SENTINEL_OFFSET = 30           # byte offset within a block
SENTINEL_BYTES  = bytes([0xFF] * 6)


def shannon_entropy(buf: bytes) -> float:
    if not buf:
        return 0.0
    counts = Counter(buf)
    n = len(buf)
    return -sum((c / n) * math.log2(c / n) for c in counts.values())


def fmt_hex16(buf: bytes, base: int = 0) -> list[str]:
    """cat-style 16-byte hex dump."""
    lines = []
    for i in range(0, len(buf), 16):
        chunk = buf[i:i + 16]
        hx = " ".join(f"{b:02x}" for b in chunk)
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{base + i:08x}  {hx:<47}  {asc}")
    return lines


def main() -> int:
    if not BIN.exists():
        print(f"ERROR: stock binary not found at {BIN}", file=sys.stderr)
        return 1

    # ─── Extract ────────────────────────────────────────────────────
    raw = BIN.read_bytes()
    if len(raw) < FILE_OFFSET + TABLE_SIZE:
        print(f"ERROR: binary too small: {len(raw)} bytes, need >= "
              f"{FILE_OFFSET + TABLE_SIZE}", file=sys.stderr)
        return 1

    data = raw[FILE_OFFSET:FILE_OFFSET + TABLE_SIZE]
    assert len(data) == TABLE_SIZE

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    bin_out = OUT_DIR / "h2_cal_table.bin"
    bin_out.write_bytes(data)

    # ─── Top-line stats ─────────────────────────────────────────────
    total = len(data)
    n_zero = data.count(0)
    n_ff = data.count(0xFF)
    n_nonzero = total - n_zero
    uniq = len(set(data))
    entropy = shannon_entropy(data)

    # ─── 3-byte record analysis ─────────────────────────────────────
    assert total % RECORD_SIZE == 0, "table size not a multiple of record size"
    num_records = total // RECORD_SIZE
    records = [data[i:i + 3] for i in range(0, total, RECORD_SIZE)]

    rec_all_zero = sum(1 for r in records if r == b"\x00\x00\x00")
    rec_has_ff = sum(1 for r in records if 0xFF in r)
    rec_all_ff = sum(1 for r in records if r == b"\xff\xff\xff")
    rec_nonzero = num_records - rec_all_zero

    # Byte-0 distribution across records — if this is a register-write
    # table, byte 0 may be a register index
    byte0_counter = Counter(r[0] for r in records if r != b"\x00\x00\x00")

    unique_records = len(set(records))
    record_counter = Counter(records)
    top_records = record_counter.most_common(10)

    # ─── 160-byte block analysis ────────────────────────────────────
    full_blocks = total // BLOCK_SIZE
    tail_bytes = total - full_blocks * BLOCK_SIZE

    sentinel_map = []       # True if block has FF×6 at offset 30
    for i in range(full_blocks):
        blk = data[i * BLOCK_SIZE:(i + 1) * BLOCK_SIZE]
        has = blk[SENTINEL_OFFSET:SENTINEL_OFFSET + 6] == SENTINEL_BYTES
        sentinel_map.append(has)

    n_with_sentinel = sum(sentinel_map)
    n_without_sentinel = full_blocks - n_with_sentinel

    # Find contiguous runs of "has sentinel" / "no sentinel"
    runs = []
    if sentinel_map:
        cur_val = sentinel_map[0]
        cur_start = 0
        for i, v in enumerate(sentinel_map[1:], 1):
            if v != cur_val:
                runs.append((cur_start, i - 1, cur_val))
                cur_start = i
                cur_val = v
        runs.append((cur_start, len(sentinel_map) - 1, cur_val))

    # ─── Pre-sentinel record extraction ─────────────────────────────
    # Every sentinel block has a 3-byte record at bytes 27-29 (record index 9)
    # just before the FF×6 sentinel at bytes 30-35. Extract those across all
    # sentinel-bearing blocks to see if they encode a block tag / counter /
    # checksum / ID. This is the single most structurally suspicious signal
    # in the first scan: block 0's record-9 is `00 30 ee`, block 1's is
    # `00 0a 8c`, and they look like they might be 16-bit identifiers.
    presentinel_records = []
    postsentinel_records = []
    for i, has in enumerate(sentinel_map):
        if not has:
            continue
        blk_start = i * BLOCK_SIZE
        rec9 = data[blk_start + 27:blk_start + 30]   # record just before sentinel
        rec12 = data[blk_start + 36:blk_start + 39]  # record just after sentinel
        presentinel_records.append((i, rec9))
        postsentinel_records.append((i, rec12))

    # Count unique pre-sentinel records — if there's a small unique set,
    # these are tags/flags; if there are ~as many unique as sentinel blocks,
    # each carries a distinct per-block identifier.
    pre_unique = Counter(rec for _, rec in presentinel_records)

    # Interpret record-9 bytes 1-2 as 16-bit big-endian tag (byte 27 = 0x00
    # in blocks 0 and 1, so the meaningful part is bytes 28-29)
    tags_be = []
    for bidx, rec in presentinel_records:
        if rec[0] == 0x00:
            tags_be.append((bidx, (rec[1] << 8) | rec[2]))
        else:
            tags_be.append((bidx, None))  # byte 27 not zero — not a tag

    # How many blocks have the tag format (byte 27 == 0)?
    n_tagged = sum(1 for _, t in tags_be if t is not None)
    # Are the tags monotonic by block index?
    monotonic = True
    prev_tag = -1
    for _, t in tags_be:
        if t is None:
            continue
        if t <= prev_tag:
            monotonic = False
            break
        prev_tag = t

    # ─── Zero-padded region boundaries ──────────────────────────────
    # Find the largest contiguous run of zero bytes.
    largest_zero_start = -1
    largest_zero_len = 0
    cur_start = -1
    cur_len = 0
    for i, b in enumerate(data):
        if b == 0:
            if cur_start == -1:
                cur_start = i
            cur_len += 1
        else:
            if cur_len > largest_zero_len:
                largest_zero_len = cur_len
                largest_zero_start = cur_start
            cur_start = -1
            cur_len = 0
    if cur_len > largest_zero_len:
        largest_zero_len = cur_len
        largest_zero_start = cur_start

    # ─── First/last non-zero positions ──────────────────────────────
    first_nonzero = next((i for i, b in enumerate(data) if b != 0), -1)
    last_nonzero = next((i for i in range(total - 1, -1, -1)
                         if data[i] != 0), -1)

    # ─── Per-4KB-window entropy ─────────────────────────────────────
    window = 4096
    window_stats = []
    for i in range(0, total, window):
        chunk = data[i:i + window]
        ent = shannon_entropy(chunk)
        nz = sum(1 for b in chunk if b != 0)
        window_stats.append((i, len(chunk), ent, nz))

    # ─── Byte histogram (top 10) ────────────────────────────────────
    byte_hist = Counter(data)
    top_bytes = byte_hist.most_common(10)

    # ─── Period search: does 160 stand alone? ───────────────────────
    # Check several candidate periods by computing cross-correlation proxy:
    # for each candidate P, measure the fraction of offsets i where
    # data[i] == data[i + P]. A high fraction = periodic.
    periods = [3, 20, 40, 80, 160, 320, 480, 640]
    period_scores = {}
    SAMPLE = min(20000, total // 2)  # enough to be representative, fast
    for p in periods:
        if p >= SAMPLE:
            continue
        matches = sum(1 for i in range(SAMPLE) if data[i] == data[i + p])
        period_scores[p] = matches / SAMPLE

    # ─── Write the analysis report ──────────────────────────────────
    report_out = OUT_DIR / "h2_cal_table_analysis.md"
    lines = []
    p = lines.append

    p("# H2 Cal Table — Independent Structural Analysis")
    p("")
    p(f"**Source:** `{BIN.relative_to(REPO)}`")
    p(f"**Extracted offset:** `0x{FILE_OFFSET:05x}` → `0x{FILE_OFFSET + TABLE_SIZE - 1:05x}`")
    p(f"**Flash address:** `0x{FLASH_ADDR:08x}` → `0x{FLASH_ADDR + TABLE_SIZE - 1:08x}`")
    p(f"**Extraction size:** `0x{TABLE_SIZE:05x}` = {TABLE_SIZE:,} bytes")
    p(f"**Output binary:** `{bin_out.relative_to(REPO)}`")
    p("")
    p("Generated by `scripts/analyze_h2_table.py` — independent verification of the")
    p("findings in `spi3_bulk_cal_resolved.md`, plus a few structural questions that")
    p("doc didn't answer.")
    p("")

    p("## 1. Top-Line Stats")
    p("")
    p("| Metric | Value |")
    p("|---|---|")
    p(f"| Total bytes | {total:,} |")
    p(f"| Zero bytes | {n_zero:,} ({n_zero / total * 100:.1f}%) |")
    p(f"| Non-zero bytes | {n_nonzero:,} ({n_nonzero / total * 100:.1f}%) |")
    p(f"| 0xFF bytes | {n_ff:,} ({n_ff / total * 100:.1f}%) |")
    p(f"| Unique byte values | {uniq} / 256 |")
    p(f"| Shannon entropy | {entropy:.3f} bits/byte |")
    p("")
    p("**Cross-check against `spi3_bulk_cal_resolved.md` §4:**")
    p("")
    p("| Metric | Doc claim | This analysis | Match |")
    p("|---|---|---|---|")
    p(f"| Total bytes | 115,638 | {total:,} | {'OK' if total == 115638 else 'MISMATCH'} |")
    p(f"| Non-zero | 40,282 (34.8%) | {n_nonzero:,} ({n_nonzero / total * 100:.1f}%) | "
      f"{'OK' if n_nonzero == 40282 else 'CHECK'} |")
    p(f"| Zero | 75,356 (65.2%) | {n_zero:,} ({n_zero / total * 100:.1f}%) | "
      f"{'OK' if n_zero == 75356 else 'CHECK'} |")
    p(f"| 0xFF | 6,199 (5.4%) | {n_ff:,} ({n_ff / total * 100:.1f}%) | "
      f"{'OK' if n_ff == 6199 else 'CHECK'} |")
    p("")

    p("## 2. 3-Byte Record Analysis")
    p("")
    p(f"- Total records: **{num_records:,}** ({num_records} × {RECORD_SIZE} = {num_records * RECORD_SIZE})")
    p(f"- All-zero records: {rec_all_zero:,} ({rec_all_zero / num_records * 100:.1f}%)")
    p(f"- Records with any 0xFF: {rec_has_ff:,} ({rec_has_ff / num_records * 100:.1f}%)")
    p(f"- All-0xFF records: {rec_all_ff:,}")
    p(f"- Non-zero records: {rec_nonzero:,}")
    p(f"- Unique record values: **{unique_records:,}** out of {num_records:,} "
      f"({unique_records / num_records * 100:.2f}%)")
    p("")
    p("**Cross-check:**")
    p("")
    p(f"- Doc claim: 20,654 all-zero records — this analysis: {rec_all_zero:,} "
      f"({'OK' if rec_all_zero == 20654 else 'CHECK'})")
    p(f"- Doc claim: 17,892 non-zero records — this analysis: {rec_nonzero:,} "
      f"({'OK' if rec_nonzero == 17892 else 'CHECK'})")
    p(f"- Doc claim: 3,537 records with 0xFF — this analysis: {rec_has_ff:,} "
      f"({'OK' if rec_has_ff == 3537 else 'CHECK'})")
    p("")
    p("### Top 10 most common 3-byte records")
    p("")
    p("| Record | Count | % of total |")
    p("|---|---|---|")
    for rec, cnt in top_records:
        hx = " ".join(f"{b:02x}" for b in rec)
        p(f"| `{hx}` | {cnt:,} | {cnt / num_records * 100:.2f}% |")
    p("")
    p("### Byte-0 distribution across non-zero records")
    p("")
    p("If this is a sparse register-write table, byte 0 of each record might be a")
    p("register index. If byte-0 diversity is low, records are keyed by a small set")
    p("of \"registers\"; if high, byte 0 is data, not an index.")
    p("")
    p(f"- Unique byte-0 values across non-zero records: **{len(byte0_counter)}** / 256")
    p(f"- Top 15 most-common byte-0 values:")
    p("")
    p("| Byte 0 | Count |")
    p("|---|---|")
    for b0, cnt in byte0_counter.most_common(15):
        p(f"| `0x{b0:02x}` | {cnt:,} |")
    p("")

    p("## 3. 160-Byte Block Structure")
    p("")
    p(f"- Full 160-byte blocks: **{full_blocks}**")
    p(f"- Tail bytes past last full block: {tail_bytes}")
    p(f"- Blocks with `FF×6` sentinel at offset 30: **{n_with_sentinel}** "
      f"({n_with_sentinel / full_blocks * 100:.1f}%)")
    p(f"- Blocks WITHOUT sentinel: **{n_without_sentinel}** "
      f"({n_without_sentinel / full_blocks * 100:.1f}%)")
    p("")
    p("**Cross-check:**")
    p("")
    p(f"- Doc claim: 546 of 722 full blocks with sentinel (75.6%)")
    p(f"- This analysis: **{n_with_sentinel} of {full_blocks}** "
      f"({n_with_sentinel / full_blocks * 100:.1f}%) "
      f"{'OK' if n_with_sentinel == 546 and full_blocks == 722 else 'CHECK'}")
    p("")
    p("### Contiguous runs of sentinel / no-sentinel blocks")
    p("")
    p("The doc says the sentinel pattern \"breaks down later where the data becomes")
    p("denser/more variable\" — this table pins it down precisely.")
    p("")
    p("| Run | Block range | Count | Has sentinel? |")
    p("|---|---|---|---|")
    for idx, (a, b, v) in enumerate(runs):
        p(f"| {idx} | {a}–{b} | {b - a + 1} | {'YES' if v else 'no'} |")
    p("")

    p("## 3b. Pre/Post-Sentinel Record Structure")
    p("")
    p("Each sentinel block's record 9 (bytes 27–29, immediately before the `FF×6`")
    p("sentinel at bytes 30–35) was extracted across all 546 sentinel-bearing blocks.")
    p("Similarly, record 12 (bytes 36–38, immediately after the sentinel) was extracted.")
    p("")
    p(f"- Unique pre-sentinel records: **{len(pre_unique)}** / {len(presentinel_records)} sentinel blocks")
    p(f"- Pre-sentinel records with byte 27 == `0x00` (tag format): **{n_tagged}** / {len(presentinel_records)}")
    p(f"- Tags interpreted as big-endian 16-bit values (bytes 28–29): **{'MONOTONIC' if monotonic else 'NOT monotonic'}** across block order")
    p("")
    p("### First 20 sentinel blocks — pre/post-sentinel records")
    p("")
    p("| Block | Pre-sentinel (rec 9) | BE16 tag | Post-sentinel (rec 12) |")
    p("|---|---|---|---|")
    for (bidx, pre), (_, post) in list(zip(presentinel_records, postsentinel_records))[:20]:
        pre_hx = " ".join(f"{b:02x}" for b in pre)
        post_hx = " ".join(f"{b:02x}" for b in post)
        if pre[0] == 0x00:
            tag = f"0x{(pre[1] << 8) | pre[2]:04x} ({(pre[1] << 8) | pre[2]})"
        else:
            tag = "—"
        p(f"| {bidx} | `{pre_hx}` | {tag} | `{post_hx}` |")
    p("")
    p("### Last 10 sentinel blocks — pre/post-sentinel records")
    p("")
    p("| Block | Pre-sentinel (rec 9) | BE16 tag | Post-sentinel (rec 12) |")
    p("|---|---|---|---|")
    for (bidx, pre), (_, post) in list(zip(presentinel_records, postsentinel_records))[-10:]:
        pre_hx = " ".join(f"{b:02x}" for b in pre)
        post_hx = " ".join(f"{b:02x}" for b in post)
        if pre[0] == 0x00:
            tag = f"0x{(pre[1] << 8) | pre[2]:04x} ({(pre[1] << 8) | pre[2]})"
        else:
            tag = "—"
        p(f"| {bidx} | `{pre_hx}` | {tag} | `{post_hx}` |")
    p("")
    p("### Top 10 most-common pre-sentinel records")
    p("")
    p("| Record | Count |")
    p("|---|---|")
    for rec, cnt in pre_unique.most_common(10):
        hx = " ".join(f"{b:02x}" for b in rec)
        p(f"| `{hx}` | {cnt} |")
    p("")

    p("## 4. Zero-Padded Region (largest contiguous zero run)")
    p("")
    p(f"- Start offset (within table): `0x{largest_zero_start:05x}` "
      f"(flash `0x{FLASH_ADDR + largest_zero_start:08x}`)")
    p(f"- Length: {largest_zero_len:,} bytes")
    p(f"- End offset: `0x{largest_zero_start + largest_zero_len - 1:05x}` "
      f"(flash `0x{FLASH_ADDR + largest_zero_start + largest_zero_len - 1:08x}`)")
    p("")
    p(f"- First non-zero byte in table: offset `0x{first_nonzero:05x}`")
    p(f"- Last non-zero byte in table: offset `0x{last_nonzero:05x}`")
    p("")

    p("## 5. Per-4KB Window Entropy + Non-Zero Density")
    p("")
    p("Windows with near-zero entropy are zero-padded / defaults. Windows >5 bits/byte")
    p("are dense data (likely coefficients). This is more precise than the doc's")
    p("three-bucket summary.")
    p("")
    p("| Window | Offset | Bytes | Entropy (bits/byte) | Non-zero | NZ % |")
    p("|---|---|---|---|---|---|")
    for i, (off, sz, ent, nz) in enumerate(window_stats):
        p(f"| {i} | `0x{off:05x}` | {sz} | {ent:.3f} | {nz} | "
          f"{nz / sz * 100:.1f}% |")
    p("")

    p("## 6. Byte Histogram (top 10 values)")
    p("")
    p("| Byte | Count | % |")
    p("|---|---|---|")
    for b, cnt in top_bytes:
        p(f"| `0x{b:02x}` | {cnt:,} | {cnt / total * 100:.2f}% |")
    p("")

    p("## 7. Period Search")
    p("")
    p("Cross-correlation proxy: for each candidate period P, fraction of positions")
    p(f"i in [0, {SAMPLE}) where `data[i] == data[i + P]`. A uniform-random byte")
    p("stream would score ~`1/256 = 0.004`. Scores near that = no correlation at")
    p("that period; higher scores indicate structural repetition.")
    p("")
    p("| Period | Score | Interpretation |")
    p("|---|---|---|")
    for per, score in sorted(period_scores.items()):
        if score > 0.5:
            tag = "strong repetition"
        elif score > 0.2:
            tag = "moderate"
        elif score > 0.05:
            tag = "weak"
        else:
            tag = "noise floor"
        p(f"| {per} | {score:.3f} | {tag} |")
    p("")

    p("## 8. First 256 Bytes (hex + ASCII)")
    p("")
    p("```")
    for line in fmt_hex16(data[:256], base=FLASH_ADDR):
        p(line)
    p("```")
    p("")

    p("## 9. Last 256 Bytes (hex + ASCII)")
    p("")
    p("```")
    tail_base = FLASH_ADDR + TABLE_SIZE - 256
    for line in fmt_hex16(data[-256:], base=tail_base):
        p(line)
    p("```")
    p("")

    p("## 10. Files Written")
    p("")
    p(f"- `{bin_out.relative_to(REPO)}` — raw {total:,}-byte extraction")
    p(f"- `{report_out.relative_to(REPO)}` — this report")
    p("")

    report_out.write_text("\n".join(lines) + "\n")

    # ─── Console summary ────────────────────────────────────────────
    print("H2 cal table — extracted and analyzed")
    print(f"  Source: {BIN.relative_to(REPO)}")
    print(f"  Extracted {total:,} bytes → {bin_out.relative_to(REPO)}")
    print(f"  Report: {report_out.relative_to(REPO)}")
    print()
    print("Top-line checks:")
    def ck(label, got, expected):
        ok = "OK " if got == expected else "!! "
        print(f"  {ok}{label}: {got:,} (expected {expected:,})")
    ck("total bytes   ", total, 115638)
    ck("zero bytes    ", n_zero, 75356)
    ck("nonzero bytes ", n_nonzero, 40282)
    ck("0xFF bytes    ", n_ff, 6199)
    ck("all-zero recs ", rec_all_zero, 20654)
    ck("nonzero recs  ", rec_nonzero, 17892)
    ck("FF-bearing recs", rec_has_ff, 3537)
    ck("total blocks  ", full_blocks, 722)
    ck("sentinel blks ", n_with_sentinel, 546)
    print()
    print(f"Unique 3-byte records: {unique_records:,} / {num_records:,} "
          f"({unique_records / num_records * 100:.2f}%)")
    print(f"Largest zero run: {largest_zero_len:,} bytes at table offset "
          f"0x{largest_zero_start:05x}")
    print(f"Sentinel-block runs: {len(runs)} contiguous segments")
    print(f"Pre-sentinel records: {len(pre_unique)} unique across {len(presentinel_records)} blocks")
    print(f"  tag format (byte 27 == 0x00): {n_tagged} / {len(presentinel_records)}")
    print(f"  tags monotonic across blocks: {monotonic}")
    print()
    print("Period scores (higher = more periodic):")
    for per in sorted(period_scores):
        print(f"  P={per:4d}: {period_scores[per]:.3f}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
