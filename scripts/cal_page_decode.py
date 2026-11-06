#!/usr/bin/env python3
"""Decode and diff the stock 2C53T saved-settings / calibration page.

The page is MCU internal flash 0x08006000..0x08006FFF (4 KB). Stock's settings
save rewrites it; it also carries the per-device factory calibration. This tool
only READS dump files (see archive/factory_cal/README.md for the ones we hold)
and never touches a device.

Usage:
  python3 scripts/cal_page_decode.py PAGE.bin [PAGE2.bin ...]   # decode each
  python3 scripts/cal_page_decode.py --diff A.bin B.bin         # compare two
  python3 scripts/cal_page_decode.py --offset 0x6000 IMAGE.bin  # page inside a
                                                                # larger image
Exit status is 1 if any page has a signature that is not 0x55 / 0xAA.

LAYOUT (region bounds are the device's own, firmware/src/util/cal_dump.c)
  0x000-0x02F  header + settings; signature at 0 (0x55 normal restore,
               0xAA meter restore). Named fields come from stock_settings.py.
  0x030-0x0AF  "block A" of the CALIBRATION region: uint16 LE words, ~1600-1716
  0x0B0-0x12F  "block B": uint16 LE words, ~3195-3306
  0x130-0x1FF  tail: zeros, then words that look like RAM pointers (0x2002xxxx).
               NOT calibration. Identical on unit #1's 2026-06-12 archive and on
               the never-flashed unit #3 (0/208 bytes differ), so it is not
               random per-unit noise; stock's own later saves have been seen to
               replace it (unit #1 live differed ~124/208 on 2026-08-14).
  0x200-0xFFF  erased (0xFF), apart from the ASCII "8KTA" at 0x800 on units #1/#3.

Finer structure inside the CALIBRATION region, which cal_dump.c's coarse region
split does not show:
  0x030-0x037  NOT table. 0x030 is stock_settings.py's saved_mode_word; unit #3
               has 0x0100 there, so one of the "127 calibration" bytes that
               differ between units #1 and #3 is a settings byte.
  0x038-0x127  the table: 60 little-endian uint32 words, 30 in block A and 30 in
               block B. Per issue #28 (maksidze's static analysis of stock) each
               word packs two channels, low16 = CH1 and high16 = CH2, block A =
               baseline endpoints, block B = upper endpoints, indexed
               [bank 0..2][range 0..9]. That reading is from code, NOT bench
               verified; this tool prints consecutive uint16 pairs so it is
               visible without committing to it.
  0x126        high half of table word 59 = sentinel B[59] = ms[0x34E]. Stock
               falls back to compiled-in defaults when it reads 0xFFFF or 0
               (factory_cal_truth_2026-08-14.md section 4.3).
  0x128, 0x12C two trailing words whose LOW halves do not follow the "xx 0C"
               pattern of block B (unit #3: 0xFC04 / 0xB8B7; unit #1: 0x04FB /
               0xB6B8). Their high halves (0x12A, 0x12E) do. Meaning unknown;
               the obvious 16-bit sums/xors of the table do not match them.
               They are flagged separately everywhere below. 0x12A and 0x12E
               equal the sentinel at 0x126 on both archived pages (unit #1
               0x0CD9, unit #3 0x0C98), so the last three B words repeat.

Pure Python 3, standard library only (stock_settings.py is a sibling module).
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import statistics
import struct
import sys
import zlib
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent

PAGE_BASE = 0x08006000
PAGE_SIZE = 0x1000

# MIRRORS firmware/src/util/cal_dump.c regions[]. test_cal_page_decode.py parses
# that file and fails if these bounds drift from it, because the whole point of
# the Python numbers is to match what the device prints.
REGIONS = (
    (0x000, 0x030, "hdr/settings"),
    (0x030, 0x130, "CALIBRATION"),
    (0x130, 0x200, "tail"),
)
REST = (0x200, PAGE_SIZE, "erased rest")  # not in cal_dump.c; shown for completeness

BLOCK_A = (0x030, 0x0B0)
BLOCK_B = (0x0B0, 0x130)
SETTINGS_IN_CAL = (0x030, 0x038)  # saved_mode_word, restored from the struct
TABLE = (0x038, 0x128)            # 60 uint32 words
TRAILING = (0x128, 0x130)         # two uint32 words
ODD_OFFSETS = (0x128, 0x12C)      # low halves that break the high-byte pattern
TRAILING_HIGH_OFFSETS = (0x12A, 0x12E)  # their high halves; == the sentinel value
SENTINEL_OFFSET = 0x126           # B[59] = ms[0x34E]
# High byte every genuine table word has, per block (1598..1716 -> 0x06xx,
# 3195..3306 -> 0x0Cxx). Used only to FLAG outliers, never to reject a page.
HIGH_BYTE = {"A": 0x06, "B": 0x0C}


def _load_stock_settings():
    spec = importlib.util.spec_from_file_location("stock_settings", SCRIPTS / "stock_settings.py")
    if spec is None or spec.loader is None:
        raise SystemExit("could not load stock_settings.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


stock_settings = _load_stock_settings()


class PageError(ValueError):
    """The input is not a usable 4 KB settings page."""


# --------------------------------------------------------------------------
# loading and basic facts
# --------------------------------------------------------------------------

def load_page(path: str | Path, offset: int = 0) -> bytes:
    data = Path(path).read_bytes()
    if offset:
        data = data[offset:]
    if len(data) < PAGE_SIZE:
        raise PageError(f"{path}: need {PAGE_SIZE} bytes at offset {offset:#x}, have {len(data)}")
    if len(data) > PAGE_SIZE and not offset:
        raise PageError(
            f"{path}: {len(data)} bytes, expected exactly {PAGE_SIZE}; "
            f"if this is an image that contains the page, pass --offset (e.g. 0x6000 for "
            f"a dump of 0x08000000-0x08006FFF or longer)"
        )
    return data[:PAGE_SIZE]


def u16(page: bytes, offset: int) -> int:
    return struct.unpack_from("<H", page, offset)[0]


def crc32_hex(page: bytes) -> str:
    return f"{zlib.crc32(page) & 0xFFFFFFFF:08X}"


def sha256_hex(page: bytes) -> str:
    return hashlib.sha256(page).hexdigest()


def non_ff(page: bytes) -> int:
    return sum(1 for b in page if b != 0xFF)


def check_signature(page: bytes) -> tuple[bool, str]:
    """(ok, text). ok only for the two signatures stock restores from."""
    sig = page[0]
    meaning = stock_settings.signature_meaning(sig)
    if sig in (stock_settings.SIGNATURE_NORMAL_RESTORE, stock_settings.SIGNATURE_METER_RESTORE):
        return True, f"0x{sig:02X} ({meaning})"
    return False, (
        f"0x{sig:02X} INVALID SIGNATURE ({meaning}); stock restores only 0x55 (normal) or "
        f"0xAA (meter) and otherwise applies its compiled-in defaults"
    )


# --------------------------------------------------------------------------
# words and flags
# --------------------------------------------------------------------------

def words(page: bytes, lo: int, hi: int) -> list[tuple[int, int]]:
    return [(off, u16(page, off)) for off in range(lo, hi, 2)]


def word_flag(off: int) -> str:
    """Short tag for the words that are not plain table entries."""
    if SETTINGS_IN_CAL[0] <= off < SETTINGS_IN_CAL[1]:
        return "settings"
    if off in ODD_OFFSETS:
        return "ODD"
    if off == SENTINEL_OFFSET:
        return "sentinel"
    if off in TRAILING_HIGH_OFFSETS:
        return "trail"
    return ""


def off_pattern_words(page: bytes) -> list[tuple[int, int]]:
    """Words in 0x038-0x12F whose high byte breaks the per-block pattern.

    On both archived pages this is exactly the two ODD words. On any other page
    it also catches outliers, which is the reason to look (issue #18's third
    unit has several out-of-range values scattered through the table).
    """
    out = []
    for off, val in words(page, TABLE[0], BLOCK_B[1]):
        block = "A" if off < BLOCK_A[1] else "B"
        if (val >> 8) != HIGH_BYTE[block]:
            out.append((off, val))
    return out


def table_range(page: bytes, lo: int, hi: int) -> tuple[int, int] | None:
    vals = [v for off, v in words(page, max(lo, TABLE[0]), min(hi, TABLE[1]))]
    return (min(vals), max(vals)) if vals else None


# --------------------------------------------------------------------------
# decode one page
# --------------------------------------------------------------------------

def _hex_rows(page: bytes, lo: int, hi: int) -> list[str]:
    return [f"  0x{off:03X}  " + " ".join(f"{b:02X}" for b in page[off:off + 16]) for off in range(lo, hi, 16)]


def _word_rows(page: bytes, lo: int, hi: int) -> list[str]:
    rows = []
    for off in range(lo, hi, 16):
        cells = []
        for i, (o, v) in enumerate(words(page, off, min(off + 16, hi))):
            sep = "  " if i % 2 == 0 and i else " "
            tag = word_flag(o)
            mark = {"settings": "s", "ODD": "!", "sentinel": "*", "trail": "t", "": " "}[tag]
            cells.append(f"{sep}{v:5d}{mark}")
        rows.append((f"  0x{off:03X} " + "".join(cells)).rstrip())
    return rows


def decode_page(page: bytes, label: str = "") -> str:
    ok, sig_text = check_signature(page)
    lines: list[str] = []
    add = lines.append
    add(f"=== {label or 'page'} ===")
    add(f"size {len(page)} B   CRC32 {crc32_hex(page)}   non-FF {non_ff(page)}/{len(page)}")
    add(f"sha256 {sha256_hex(page)}")
    add(f"signature @0x00: {sig_text}")
    add("")

    add("Header / settings 0x000-0x02F")
    lines.extend(_hex_rows(page, 0x00, 0x30))
    add("  named fields (stock_settings.py; offsets only, evidence grades are there):")
    for field in stock_settings.RECOVERED_FIELDS:
        off, size = int(field["offset"]), int(field["size"])
        if off >= REGIONS[0][1]:
            continue
        raw = page[off:off + size]
        value = int.from_bytes(raw, "little")
        add(f"    0x{off:02X} {field['name']:<24} {raw.hex(' '):<12} = 0x{value:0{size * 2}X} ({value})")
    add("")

    add("Calibration region 0x030-0x12F, uint16 LE, decimal, rows of 8 words (4 low/high pairs)")
    add("  flags: s = settings word, not table   ! = ODD word   * = sentinel B[59]   "
        "t = high half of a trailing word")
    for name, (lo, hi) in (("Block A", BLOCK_A), ("Block B", BLOCK_B)):
        rng = table_range(page, lo, hi)
        span = f"  table range {rng[0]}..{rng[1]}" if rng else ""
        add(f" {name} 0x{lo:03X}-0x{hi - 1:03X}{span}")
        lines.extend(_word_rows(page, lo, hi))
    add("")

    add("Flagged words")
    add(f"  0x{SETTINGS_IN_CAL[0]:03X} saved_mode_word (settings, inside the CALIBRATION region): "
        f"0x{u16(page, SETTINGS_IN_CAL[0]):04X}")
    sent = u16(page, SENTINEL_OFFSET)
    sent_ok = sent not in (0x0000, 0xFFFF)
    add(f"  0x{SENTINEL_OFFSET:03X} sentinel B[59] = ms[0x34E]: 0x{sent:04X} ({sent}) "
        f"{'valid, stock keeps the restored table' if sent_ok else 'ERASED/ZERO, stock would apply compiled-in defaults'}")
    for off in ODD_OFFSETS:
        v = u16(page, off)
        hi_off = off + 2
        add(f"  0x{off:03X} ODD word: 0x{v:04X} ({v}); neighbour 0x{hi_off:03X} = 0x{u16(page, hi_off):04X} "
            f"({'follows' if (u16(page, hi_off) >> 8) == HIGH_BYTE['B'] else 'breaks'} the xx0C pattern)")
    extra = [(o, v) for o, v in off_pattern_words(page) if o not in ODD_OFFSETS]
    if extra:
        add("  OFF-PATTERN table words (high byte not 0x06 in A / 0x0C in B), not the known ODD pair:")
        for o, v in extra:
            add(f"    0x{o:03X} 0x{v:04X} ({v})")
    else:
        add("  no other off-pattern table words")
    add("")

    tail = page[REGIONS[2][0]:REGIONS[2][1]]
    rest = page[REST[0]:REST[1]]
    nonzero_tail = sum(1 for b in tail if b)
    add(f"Tail 0x130-0x1FF: {nonzero_tail}/{len(tail)} non-zero bytes (not calibration)")
    ptrs = [struct.unpack_from('<I', page, o)[0] for o in range(REGIONS[2][0], REGIONS[2][1], 4)]
    ram = [p for p in ptrs if 0x20000000 <= p < 0x20040000]
    add(f"  {len(ram)} words in SRAM range 0x2000xxxx-0x2003xxxx")
    other = [(o, v) for o, v in words(page, REST[0], REST[1]) if v != 0xFFFF]
    add(f"Rest 0x200-0xFFF: {sum(1 for b in rest if b != 0xFF)} non-FF bytes"
        + (f", first at 0x{other[0][0]:03X}" if other else ""))
    return "\n".join(lines)


# --------------------------------------------------------------------------
# diff two pages
# --------------------------------------------------------------------------

def count_diff(a: bytes, b: bytes, lo: int, hi: int) -> int:
    return sum(1 for i in range(lo, hi) if a[i] != b[i])


def region_diff(a: bytes, b: bytes) -> list[tuple[str, int, int, int]]:
    """(name, lo, hi, differing bytes) in the device's own region table."""
    return [(name, lo, hi, count_diff(a, b, lo, hi)) for lo, hi, name in REGIONS]


def calibration_breakdown(a: bytes, b: bytes) -> list[tuple[str, int, int, int]]:
    """Split the device's CALIBRATION count into settings word / table / trailing."""
    parts = (
        ("settings word (saved_mode_word)", SETTINGS_IN_CAL),
        ("table (60 uint32 words)", TABLE),
        ("trailing words (ODD pair + neighbours)", TRAILING),
    )
    return [(name, lo, hi, count_diff(a, b, lo, hi)) for name, (lo, hi) in parts]


def word_deltas(a: bytes, b: bytes, lo: int = REGIONS[1][0], hi: int = REGIONS[1][1]) -> list[dict]:
    """Differing uint16 words in [lo, hi) with delta = b - a and flags."""
    out = []
    for off in range(lo, hi, 2):
        va, vb = u16(a, off), u16(b, off)
        if va != vb:
            out.append({"offset": off, "a": va, "b": vb, "delta": vb - va, "flag": word_flag(off)})
    return out


def diff_pages(a: bytes, b: bytes) -> dict:
    deltas = word_deltas(a, b)
    return {
        "regions": region_diff(a, b),
        "rest": (REST[2], REST[0], REST[1], count_diff(a, b, REST[0], REST[1])),
        "whole": count_diff(a, b, 0, PAGE_SIZE),
        "calibration_breakdown": calibration_breakdown(a, b),
        "header_bytes": [i for i in range(REGIONS[0][1]) if a[i] != b[i]],
        "tail_bytes": [i for i in range(REGIONS[2][0], REGIONS[2][1]) if a[i] != b[i]],
        "word_deltas": deltas,
        "table_deltas": [d for d in deltas if d["flag"] == ""],
        "odd_words": [
            {"offset": off, "a": u16(a, off), "b": u16(b, off)} for off in ODD_OFFSETS
        ],
    }


def _stats(deltas: list[int]) -> str:
    if not deltas:
        return "none"
    return f"min {min(deltas):+d}  median {statistics.median(deltas):+g}  max {max(deltas):+d}"


def format_diff(a: bytes, b: bytes, label_a: str = "A", label_b: str = "B") -> str:
    d = diff_pages(a, b)
    lines: list[str] = []
    add = lines.append
    add(f"=== diff  A = {label_a}  (CRC32 {crc32_hex(a)})")
    add(f"          B = {label_b}  (CRC32 {crc32_hex(b)}) ===")
    for tag, page in (("A", a), ("B", b)):
        ok, text = check_signature(page)
        if not ok:
            add(f"  WARNING {tag}: signature {text}")
    add("")
    add("Per-region differing bytes (same table and numbers as the device's cal_dump screen)")
    for name, lo, hi, n in d["regions"]:
        add(f"  {name:<13} 0x{lo:03X}-0x{hi - 1:03X}  {n}/{hi - lo}")
        if name == "CALIBRATION":
            for sub, slo, shi, sn in d["calibration_breakdown"]:
                add(f"      of which 0x{slo:03X}-0x{shi - 1:03X} {sub}: {sn}/{shi - slo}")
    name, lo, hi, n = d["rest"]
    add(f"  {name:<13} 0x{lo:03X}-0x{hi - 1:03X}  {n}/{hi - lo}   (not in cal_dump.c)")
    add(f"  whole page    {d['whole']}/{PAGE_SIZE}")
    add("")

    add("Header byte differences")
    if d["header_bytes"]:
        for i in d["header_bytes"]:
            names = [str(f["name"]) for f in stock_settings.fields_overlapping(i, i + 1)]
            add(f"  0x{i:02X}  {a[i]:02X} -> {b[i]:02X}   {', '.join(names) if names else '-'}")
    else:
        add("  none")
    add("")

    tail_n = len(d["tail_bytes"])
    add(f"Tail 0x130-0x1FF: {tail_n}/208 bytes differ")
    if tail_n:
        shown = ", ".join(f"0x{i:03X}" for i in d["tail_bytes"][:16])
        add(f"  offsets: {shown}{' ...' if tail_n > 16 else ''}")
    add("")

    flagged = [w for w in d["word_deltas"] if w["flag"]]
    table = d["table_deltas"]
    add(f"Calibration region uint16 words that differ: {len(d['word_deltas'])}/128 "
        f"({len(table)} plain table words, {len(flagged)} flagged)")
    for block, (lo, hi) in (("A", BLOCK_A), ("B", BLOCK_B)):
        sel = [w["delta"] for w in table if lo <= w["offset"] < hi]
        add(f"  block {block}: {len(sel)} words differ, delta (B-A) {_stats(sel)}")
    add("")
    add("  offset   A      B      delta (B-A)")
    for w in table:
        add(f"  0x{w['offset']:03X}  {w['a']:5d}  {w['b']:5d}  {w['delta']:+6d}")
    add("")

    add("Flagged words (kept out of the table deltas above)")
    for w in flagged:
        add(f"  0x{w['offset']:03X}  {w['flag']:<8} 0x{w['a']:04X} -> 0x{w['b']:04X}   ({w['a']} -> {w['b']}, {w['delta']:+d})")
    for w in d["odd_words"]:
        if w["a"] == w["b"]:
            add(f"  0x{w['offset']:03X}  ODD      identical on both: 0x{w['a']:04X}")
    return "\n".join(lines)


# --------------------------------------------------------------------------
# command line
# --------------------------------------------------------------------------

def _int(text: str) -> int:
    return int(text, 0)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pages", nargs="*", help="page dump(s) to decode")
    parser.add_argument("--diff", nargs=2, metavar=("A", "B"), help="compare two pages")
    parser.add_argument("--offset", type=_int, default=0,
                        help="byte offset of the page inside each input file (e.g. 0x6000)")
    args = parser.parse_args(argv)
    if not args.pages and not args.diff:
        parser.error("give at least one page, or --diff A B")

    status = 0
    try:
        for path in args.pages:
            page = load_page(path, args.offset)
            print(decode_page(page, path))
            print()
            if not check_signature(page)[0]:
                status = 1
        if args.diff:
            a_path, b_path = args.diff
            a, b = load_page(a_path, args.offset), load_page(b_path, args.offset)
            print(format_diff(a, b, a_path, b_path))
            if not (check_signature(a)[0] and check_signature(b)[0]):
                status = 1
    except (PageError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return status


if __name__ == "__main__":
    raise SystemExit(main())
