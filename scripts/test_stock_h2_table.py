#!/usr/bin/env python3
"""Side-effect-free stock V1.2.0 H2/SPI3 table structure guard."""

from __future__ import annotations

from collections import Counter
import hashlib
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
BIN = REPO / "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
EXPECTED_STOCK_SHA256 = (
    "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
)
EXPECTED_H2_SHA256 = (
    "27ec73be12a946e1a53b5254a4c91707a78e6a79b6f9e96b0e070164454d1aa4"
)

FILE_OFFSET = 0x51D19
FLASH_ADDR = 0x08051D19
TABLE_SIZE = 0x1C3B6
RECORD_SIZE = 3
BLOCK_SIZE = 160
SENTINEL_OFFSET = 30
SENTINEL_BYTES = b"\xff" * 6

EXPECTED_STATS = {
    "total_bytes": 115638,
    "zero_bytes": 75356,
    "nonzero_bytes": 40282,
    "ff_bytes": 6199,
    "records": 38546,
    "all_zero_records": 20654,
    "nonzero_records": 17892,
    "records_with_ff": 3537,
    "all_ff_records": 824,
    "full_blocks": 722,
    "tail_bytes": 118,
    "sentinel_blocks": 546,
    "blocks_without_sentinel": 176,
}

EXPECTED_SENTINEL_RUNS = [
    (0, 543, True, 0x00000, 0x153FF, 87040),
    (544, 567, False, 0x15400, 0x162FF, 3840),
    (568, 569, True, 0x16300, 0x1643F, 320),
    (570, 721, False, 0x16440, 0x1C33F, 24320),
]

EXPECTED_BLOCK_MARKERS = {
    0: {
        "sentinel": True,
        "pre": "00 30 ee",
        "post": "00 00 20",
    },
    543: {
        "sentinel": True,
        "pre": "00 65 c5",
        "post": "ff ff ff",
    },
    568: {
        "sentinel": True,
        "pre": "ff ff ff",
        "post": "ff ff ff",
    },
    569: {
        "sentinel": True,
        "pre": "ff ff ff",
        "post": "ff ff ff",
    },
    570: {
        "sentinel": False,
        "pre": "00 00 00",
        "post": "00 00 00",
    },
    721: {
        "sentinel": False,
        "pre": "50 00 00",
        "post": "ef ff ff",
    },
}


def _hex(buf: bytes) -> str:
    return buf.hex(" ")


def _assert_equal(label: str, actual: object, expected: object) -> None:
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected!r}, got {actual!r}")


def load_h2_table() -> bytes:
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    end = FILE_OFFSET + TABLE_SIZE
    if len(raw) < end:
        raise AssertionError(f"{BIN}: size {len(raw)} too small for H2 end {end}")
    data = raw[FILE_OFFSET:end]
    _assert_equal("H2 table length", len(data), TABLE_SIZE)
    _assert_equal("H2 table sha256", hashlib.sha256(data).hexdigest(), EXPECTED_H2_SHA256)
    return data


def _records(data: bytes) -> list[bytes]:
    _assert_equal("H2 table record remainder", len(data) % RECORD_SIZE, 0)
    return [data[i:i + RECORD_SIZE] for i in range(0, len(data), RECORD_SIZE)]


def _sentinel_map(data: bytes) -> list[bool]:
    full_blocks = len(data) // BLOCK_SIZE
    return [
        data[i * BLOCK_SIZE + SENTINEL_OFFSET:
             i * BLOCK_SIZE + SENTINEL_OFFSET + len(SENTINEL_BYTES)] == SENTINEL_BYTES
        for i in range(full_blocks)
    ]


def _sentinel_runs(flags: list[bool]) -> list[tuple[int, int, bool, int, int, int]]:
    if not flags:
        return []
    runs: list[tuple[int, int, bool, int, int, int]] = []
    cur = flags[0]
    start = 0
    for idx, val in enumerate(flags[1:], 1):
        if val == cur:
            continue
        runs.append((
            start,
            idx - 1,
            cur,
            start * BLOCK_SIZE,
            idx * BLOCK_SIZE - 1,
            (idx - start) * BLOCK_SIZE,
        ))
        start = idx
        cur = val
    runs.append((
        start,
        len(flags) - 1,
        cur,
        start * BLOCK_SIZE,
        len(flags) * BLOCK_SIZE - 1,
        (len(flags) - start) * BLOCK_SIZE,
    ))
    return runs


def verify_h2_table() -> dict[str, object]:
    """Verify the stock H2 blob layout without extracting or replaying it.

    This proves the local reverse-engineering tables are byte-grounded.  It
    does not prove FPGA acceptance, apply semantics, or DMM physical
    calibration coefficients.
    """
    data = load_h2_table()
    records = _records(data)
    flags = _sentinel_map(data)
    byte_counts = Counter(data)

    stats = {
        "total_bytes": len(data),
        "zero_bytes": byte_counts[0],
        "nonzero_bytes": len(data) - byte_counts[0],
        "ff_bytes": byte_counts[0xFF],
        "records": len(records),
        "all_zero_records": sum(1 for rec in records if rec == b"\x00\x00\x00"),
        "nonzero_records": sum(1 for rec in records if rec != b"\x00\x00\x00"),
        "records_with_ff": sum(1 for rec in records if 0xFF in rec),
        "all_ff_records": sum(1 for rec in records if rec == b"\xff\xff\xff"),
        "full_blocks": len(data) // BLOCK_SIZE,
        "tail_bytes": len(data) % BLOCK_SIZE,
        "sentinel_blocks": sum(flags),
        "blocks_without_sentinel": len(flags) - sum(flags),
    }
    _assert_equal("H2 stats", stats, EXPECTED_STATS)

    runs = _sentinel_runs(flags)
    _assert_equal("H2 sentinel runs", runs, EXPECTED_SENTINEL_RUNS)

    tail_start = stats["full_blocks"] * BLOCK_SIZE
    _assert_equal("H2 tail start", tail_start, 0x1C340)
    _assert_equal("H2 tail end", len(data) - 1, 0x1C3B5)

    for block_index, expected in EXPECTED_BLOCK_MARKERS.items():
        off = block_index * BLOCK_SIZE
        actual = {
            "sentinel": flags[block_index],
            "pre": _hex(data[off + 27:off + 30]),
            "post": _hex(data[off + 36:off + 39]),
        }
        _assert_equal(f"H2 block {block_index} marker", actual, expected)

    return {
        "file_offset": f"0x{FILE_OFFSET:05x}",
        "flash_addr": f"0x{FLASH_ADDR:08x}",
        "table_sha256": EXPECTED_H2_SHA256,
        "stats": stats,
        "sentinel_runs": runs,
        "tail_range": (tail_start, len(data) - 1, stats["tail_bytes"]),
    }


def main() -> None:
    result = verify_h2_table()
    print(
        "stock H2/SPI3 table: ok "
        f"offset={result['file_offset']} size={result['stats']['total_bytes']} "
        f"sha256={result['table_sha256']}"
    )
    print(
        "stock H2/SPI3 table runs: "
        + "; ".join(
            f"{start}-{end}:{'sentinel' if has else 'dense'}"
            for start, end, has, _, _, _ in result["sentinel_runs"]
        )
    )


if __name__ == "__main__":
    main()
