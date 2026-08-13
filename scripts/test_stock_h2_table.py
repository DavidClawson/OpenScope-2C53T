#!/usr/bin/env python3
"""Side-effect-free stock V1.2.0 H2/SPI3 table structure guard."""

from __future__ import annotations

from collections import Counter
import hashlib
from pathlib import Path
import sys


REPO = Path(__file__).resolve().parents[1]
BIN = REPO / "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
EXPECTED_STOCK_SHA256 = (
    "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
)
EXPECTED_H2_SHA256 = (
    "5a0e73384e496bdb3b3d591b852bec2e806e70cbc71439c9829695324efd5c3b"
)

FILE_OFFSET = 0x4AD19
FLASH_ADDR = 0x08051D19
BASE_ADDR = 0x08000000
TABLE_SIZE = 0x1C3B6
RECORD_SIZE = 3
BLOCK_SIZE = 160
SENTINEL_OFFSET = 30
SENTINEL_BYTES = b"\xff" * 6

EXPECTED_STATS = {
    "total_bytes": 115638,
    "zero_bytes": 89288,
    "nonzero_bytes": 26350,
    "ff_bytes": 4750,
}

EXPECTED_H2_PREFIX = bytes.fromhex(
    "ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff "
    "ff ff ff ff ff ff a5 c3 06 00 00 00 01 20 68 1b"
)
EXPECTED_H2_SUFFIX = bytes.fromhex(
    "34 73 0a 00 00 00 00 00 23 77 ff ff ff ff ff ff "
    "ff ff 08 00 00 00 ff ff ff ff ff ff ff ff ff ff"
)

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

EXPECTED_SPI3_ENABLE_PC6_SEQUENCE = [
    {
        "label": "spi3_init_param_0x0100",
        "addr": 0x080265CE,
        "bytes": "4f f4 80 70 18 90",
        "meaning": "store SPI init word 0x00000100 into local config",
    },
    {
        "label": "spi3_init_param_0x01010100",
        "addr": 0x080265D4,
        "bytes": "40 f2 00 10 c0 f2 01 10 19 90",
        "meaning": "store SPI init word 0x01010100 into local config",
    },
    {
        "label": "spi3_init_helper_call",
        "addr": 0x080265DE,
        "bytes": "08 f5 40 60 18 a9 10 f0 30 f9",
        "meaning": "call stock SPI init helper for 0x40003C00",
    },
    {
        "label": "spi3_ctrl2_dma_bits_set",
        "addr": 0x080265E8,
        "bytes": "56 f8 04 0c 00 27 40 f0 02 00 46 f8 04 0c 56 f8 04 0c 10 a9 40 f0 01 00 46 f8 04 0c",
        "meaning": "set SPI3 CTRL2 bits 1 and 0 before SPI enable",
    },
    {
        "label": "spi3_ctrl1_spe_set",
        "addr": 0x08026604,
        "bytes": "d8 f8 00 0c 40 f0 40 00 c8 f8 00 0c",
        "meaning": "set SPI3 CTRL1 bit 6 (SPE)",
    },
    {
        "label": "gpioc_clock_enable_after_spi_enable",
        "addr": 0x08026610,
        "bytes": "db f8 00 00 40 f0 10 00 cb f8 00 00",
        "meaning": "enable GPIOC clock after SPI3 SPE",
    },
    {
        "label": "pc6_gpio_config_call_after_spi_enable",
        "addr": 0x0802661C,
        "bytes": "01 20 8d f8 47 00 41 f2 18 00 ad f8 45 00 20 46 8d f8 44 70 cd f8 40 90 09 f0 62 fe",
        "meaning": "configure GPIOC.6 after SPI3 SPE",
    },
    {
        "label": "pc6_high_after_spi_enable",
        "addr": 0x08026638,
        "bytes": "42 f6 1c 30 c4 f8 10 90",
        "meaning": "write GPIOC_BOP 0x40 after SPI3 SPE and PC6 config",
    },
    {
        "label": "first_handshake_cs_high_after_delay",
        "addr": 0x0802676E,
        "bytes": "40 20 44 f8 0e 00",
        "meaning": "first SPI3 handshake CS-high write after the delay",
    },
]

EXPECTED_PREAMBLE_SEQUENCE = [
    ("cs_high_before_sync_00", 0x0802676E, "40 20 44 f8 0e 00"),
    ("sync_00_with_cs_high", 0x0802678E, "00 20 70 60"),
    ("cs_low_before_05", 0x080267B0, "40 20 44 f8 0c 00"),
    ("cmd_05_with_cs_low", 0x080267D2, "05 20 70 60"),
    ("pad_00_with_cs_low", 0x0802680E, "00 20 70 60"),
    ("cs_high_before_group1_tail_00", 0x08026830, "40 20 44 f8 0e 00"),
    ("group1_tail_00_with_cs_high", 0x08026852, "00 20 70 60"),
    ("cs_low_before_12", 0x080268EA, "40 20 44 f8 0c 00"),
    ("cmd_12_with_cs_low", 0x0802690A, "12 20 70 60"),
    ("pad_00_after_12_with_cs_low", 0x08026946, "00 20 70 60"),
    ("cs_high_before_group2_tail_00", 0x08026968, "40 20 44 f8 0e 00"),
    ("group2_tail_00_with_cs_high", 0x0802698A, "00 20 70 60"),
    ("cs_low_before_15", 0x08026A22, "40 20 44 f8 0c 00"),
    ("cmd_15_with_cs_low", 0x08026A42, "15 20 70 60"),
    ("pad_00_after_15_with_cs_low", 0x08026A7E, "00 20 70 60"),
    ("cs_high_before_group3_tail_00", 0x08026AA0, "40 20 44 f8 0e 00"),
    ("group3_tail_00_with_cs_high", 0x08026AC2, "00 20 70 60"),
    ("cs_low_before_h2_start", 0x08026AE4, "40 20 44 f8 0c 00"),
    ("h2_start_3b_with_cs_low", 0x08026B06, "3b 20 70 60"),
]

EXPECTED_CLOSE_SEQUENCE = [
    {
        "label": "cs_high_after_bulk_body",
        "addr": 0x08026C32,
        "bytes": "40 20 44 f8 0e 00",
        "meaning": "PB6 CS HIGH / GPIOB_BOP",
    },
    {
        "label": "flush_00_with_cs_high",
        "addr": 0x08026C52,
        "bytes": "00 20 70 60",
        "meaning": "SPI3 TX 0x00",
    },
    {
        "label": "cs_low_before_close_opcode",
        "addr": 0x08026C74,
        "bytes": "40 20 44 f8 0c 00",
        "meaning": "PB6 CS LOW / GPIOB_BCR",
    },
    {
        "label": "close_opcode_3a_with_cs_low",
        "addr": 0x08026C96,
        "bytes": "3a 20 70 60",
        "meaning": "SPI3 TX 0x3A",
    },
    {
        "label": "flush_00_with_cs_low",
        "addr": 0x08026CD2,
        "bytes": "00 20 70 60",
        "meaning": "SPI3 TX 0x00",
    },
    {
        "label": "cs_high_after_close",
        "addr": 0x08026CF4,
        "bytes": "40 20 44 f8 0e 00",
        "meaning": "PB6 CS HIGH / GPIOB_BOP",
    },
    {
        "label": "post_close_flush_00_with_cs_high",
        "addr": 0x08026D16,
        "bytes": "00 20 70 60",
        "meaning": "SPI3 TX 0x00",
    },
    {
        "label": "cs_low_for_final_flush",
        "addr": 0x08026D38,
        "bytes": "40 20 44 f8 0c 00",
        "meaning": "PB6 CS LOW / GPIOB_BCR",
    },
    {
        "label": "final_flush_00_with_cs_low",
        "addr": 0x08026D5A,
        "bytes": "00 20 70 60",
        "meaning": "SPI3 TX 0x00",
    },
    {
        "label": "cs_high_before_last_visible_zero",
        "addr": 0x08026D7C,
        "bytes": "40 20 44 f8 0e 00",
        "meaning": "PB6 CS HIGH / GPIOB_BOP",
    },
    {
        "label": "last_visible_flush_00_with_cs_high",
        "addr": 0x08026D9E,
        "bytes": "41 f2 14 08 00 20 c4 f2 01 08 70 60",
        "meaning": "SPI3 TX 0x00 after r8 setup",
    },
]

EXPECTED_POST_H2_SPI3_QUEUE_SEQUENCE = [
    {
        "label": "queue_trigger_1",
        "load_queue_addr": 0x08026DC8,
        "payload_addr": 0x08026DCE,
        "store_addr": 0x08026DD6,
        "send_addr": 0x08026DDA,
        "payload": 1,
        "payload_bytes": "01 25",
        "store_bytes": "8d f8 60 50",
        "send_bytes": "13 f0 89 ff",
    },
    {
        "label": "queue_trigger_2",
        "load_queue_addr": 0x08026DE0,
        "payload_addr": 0x08026DDE,
        "store_addr": 0x08026DE4,
        "send_addr": 0x08026DEE,
        "payload": 2,
        "payload_bytes": "02 21",
        "store_bytes": "8d f8 60 10",
        "send_bytes": "13 f0 7f ff",
    },
    {
        "label": "queue_trigger_6",
        "load_queue_addr": 0x08026DF4,
        "payload_addr": 0x08026DF2,
        "store_addr": 0x08026DF8,
        "send_addr": 0x08026E02,
        "payload": 6,
        "payload_bytes": "06 21",
        "store_bytes": "8d f8 60 10",
        "send_bytes": "13 f0 75 ff",
    },
    {
        "label": "queue_trigger_7",
        "load_queue_addr": 0x08026E08,
        "payload_addr": 0x08026E06,
        "store_addr": 0x08026E0C,
        "send_addr": 0x08026E16,
        "payload": 7,
        "payload_bytes": "07 21",
        "store_bytes": "8d f8 60 10",
        "send_bytes": "13 f0 6b ff",
    },
    {
        "label": "queue_trigger_8",
        "load_queue_addr": 0x08026E1C,
        "payload_addr": 0x08026E1A,
        "store_addr": 0x08026E20,
        "send_addr": 0x08026E2A,
        "payload": 8,
        "payload_bytes": "08 21",
        "store_bytes": "8d f8 60 10",
        "send_bytes": "13 f0 61 ff",
    },
]

SPI3_TRIGGER_DISPATCH_TABLE_ADDR = 0x0803753A
EXPECTED_SPI3_TRIGGER_TARGETS = {
    1: 0x08037550,
    2: 0x08037974,
    3: 0x080379F6,
    4: 0x080375A8,
    5: 0x08037690,
    6: 0x08037D20,
    7: 0x08037D60,
    8: 0x08037760,
    9: 0x080377A0,
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


def verify_h2_close_sequence() -> list[dict[str, object]]:
    """Verify the stock H2 close opcode is sent while PB6 CS is asserted.

    The local firmware must follow this byte-grounded sequence.  In
    particular, 0x08026C74 drives GPIOB_BCR (PB6 LOW) before 0x08026C96 writes
    0x3A to SPI3_DATA; 0x3A-with-CS-high is a stale reverse-engineering error.
    """
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    verified: list[dict[str, object]] = []
    for item in EXPECTED_CLOSE_SEQUENCE:
        addr = int(item["addr"])
        expected = bytes.fromhex(str(item["bytes"]))
        off = addr - BASE_ADDR
        actual = raw[off:off + len(expected)]
        _assert_equal(f"H2 close {item['label']}", actual.hex(" "), item["bytes"])
        verified.append({
            "label": item["label"],
            "addr": f"0x{addr:08x}",
            "bytes": item["bytes"],
            "meaning": item["meaning"],
        })
    return verified


def verify_h2_preamble_sequence() -> list[dict[str, object]]:
    """Verify the stock pre-H2 handshake CS framing.

    Stock does not send the 11 preamble bytes as one continuous CS-low
    transaction.  The CS edges around 00/05/00/00, 12/00/00, 15/00/00/3B are
    byte-grounded here so the open firmware cannot silently regress to the
    old long-CS-low framing.
    """
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    verified: list[dict[str, object]] = []
    for label, addr, hex_bytes in EXPECTED_PREAMBLE_SEQUENCE:
        expected = bytes.fromhex(hex_bytes)
        off = addr - BASE_ADDR
        actual = raw[off:off + len(expected)]
        _assert_equal(f"H2 preamble {label}", actual.hex(" "), hex_bytes)
        verified.append({
            "label": label,
            "addr": f"0x{addr:08x}",
            "bytes": hex_bytes,
        })
    return verified


def verify_spi3_enable_pc6_sequence() -> list[dict[str, object]]:
    """Verify stock enables SPI3 before driving PC6 high.

    PC6 is the FPGA SPI-enable line.  The low-DCV failure is upstream of DMM
    display math, so boot-order claims around SPI3/H2 need to stay
    byte-grounded.  Stock V1.2.0 sets SPI3 CTRL2/CTRL1/SPE first, then
    configures GPIOC.6 and writes GPIOC_BOP=0x40, then delays before the first
    SPI3 handshake transfer at 0x0802676E.
    """
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    verified: list[dict[str, object]] = []
    last_addr = 0
    for item in EXPECTED_SPI3_ENABLE_PC6_SEQUENCE:
        addr = int(item["addr"])
        if addr <= last_addr:
            raise AssertionError(
                f"SPI3/PC6 order regressed at {item['label']}: "
                f"0x{addr:08x} after 0x{last_addr:08x}"
            )
        last_addr = addr
        expected = bytes.fromhex(str(item["bytes"]))
        off = addr - BASE_ADDR
        actual = raw[off:off + len(expected)]
        _assert_equal(f"SPI3/PC6 {item['label']}", actual.hex(" "), item["bytes"])
        verified.append({
            "label": item["label"],
            "addr": f"0x{addr:08x}",
            "bytes": item["bytes"],
            "meaning": item["meaning"],
        })
    return verified


def verify_post_h2_spi3_queue_sequence() -> list[dict[str, object]]:
    """Verify post-H2 stock boot bytes are SPI3 queue payloads, not USART TX."""
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    verified: list[dict[str, object]] = []
    for item in EXPECTED_POST_H2_SPI3_QUEUE_SEQUENCE:
        load_addr = int(item["load_queue_addr"])
        load_actual = raw[load_addr - BASE_ADDR:load_addr - BASE_ADDR + 4]
        _assert_equal(
            f"post-H2 {item['label']} queue load",
            load_actual.hex(" "),
            "d9 f8 00 00",
        )

        for field in ("payload", "store", "send"):
            addr = int(item[f"{field}_addr"])
            expected_hex = str(item[f"{field}_bytes"])
            actual = raw[addr - BASE_ADDR:addr - BASE_ADDR + len(bytes.fromhex(expected_hex))]
            _assert_equal(
                f"post-H2 {item['label']} {field}",
                actual.hex(" "),
                expected_hex,
            )

        verified.append({
            "label": item["label"],
            "payload": item["payload"],
            "queue": "0x20002d78",
            "payload_addr": f"0x{int(item['payload_addr']):08x}",
            "send_addr": f"0x{int(item['send_addr']):08x}",
        })
    return verified


def verify_spi3_trigger_dispatch_table() -> list[dict[str, object]]:
    """Verify the stock SPI3 queue consumer's public trigger-byte targets.

    The post-H2 boot queue sends public bytes 1, 2, 6, 7, and 8.  Trigger byte 8
    (`trigger_byte - 1 == 7`) is the status/pre-acquisition exchange target at
    0x08037760.  The two-phase calibration readback is public trigger byte 9,
    which is not part of the recovered post-H2 boot queue.
    """
    raw = BIN.read_bytes()
    stock_sha = hashlib.sha256(raw).hexdigest()
    _assert_equal("stock APP sha256", stock_sha, EXPECTED_STOCK_SHA256)

    table_off = SPI3_TRIGGER_DISPATCH_TABLE_ADDR - BASE_ADDR
    verified: list[dict[str, object]] = []
    for trigger, expected_target in EXPECTED_SPI3_TRIGGER_TARGETS.items():
        halfword = int.from_bytes(raw[table_off + (trigger - 1) * 2:
                                      table_off + trigger * 2], "little")
        target = SPI3_TRIGGER_DISPATCH_TABLE_ADDR + 4 + halfword * 2
        _assert_equal(
            f"SPI3 trigger {trigger} dispatch target",
            f"0x{target:08x}",
            f"0x{expected_target:08x}",
        )
        verified.append({
            "trigger": trigger,
            "target": f"0x{target:08x}",
        })
    return verified


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
    byte_counts = Counter(data)

    stats = {
        "total_bytes": len(data),
        "zero_bytes": byte_counts[0],
        "nonzero_bytes": len(data) - byte_counts[0],
        "ff_bytes": byte_counts[0xFF],
    }
    _assert_equal("H2 stats", stats, EXPECTED_STATS)
    _assert_equal("H2 corrected prefix", data[:len(EXPECTED_H2_PREFIX)], EXPECTED_H2_PREFIX)
    _assert_equal("H2 corrected suffix", data[-len(EXPECTED_H2_SUFFIX):], EXPECTED_H2_SUFFIX)
    _assert_equal("H2 Gowin preamble offset", data.find(bytes.fromhex("a5 c3")), 22)
    _assert_equal("H2 Gowin IDCODE offset", data.find(bytes.fromhex("01 20 68 1b")), 28)

    spi3_enable_pc6_sequence = verify_spi3_enable_pc6_sequence()
    preamble_sequence = verify_h2_preamble_sequence()
    close_sequence = verify_h2_close_sequence()
    post_h2_queue_sequence = verify_post_h2_spi3_queue_sequence()
    spi3_trigger_dispatch = verify_spi3_trigger_dispatch_table()

    return {
        "file_offset": f"0x{FILE_OFFSET:05x}",
        "flash_addr": f"0x{FLASH_ADDR:08x}",
        "table_sha256": EXPECTED_H2_SHA256,
        "stats": stats,
        "gowin_preamble_offset": data.find(bytes.fromhex("a5 c3")),
        "gowin_idcode_offset": data.find(bytes.fromhex("01 20 68 1b")),
        "spi3_enable_pc6_sequence": spi3_enable_pc6_sequence,
        "preamble_sequence": preamble_sequence,
        "close_sequence": close_sequence,
        "post_h2_queue_sequence": post_h2_queue_sequence,
        "spi3_trigger_dispatch": spi3_trigger_dispatch,
    }


def main() -> None:
    if not BIN.exists():
        print(f"stock H2/SPI3 table: skipped; missing {BIN}", file=sys.stderr)
        return

    result = verify_h2_table()
    print(
        "stock H2/SPI3 table: ok "
        f"offset={result['file_offset']} size={result['stats']['total_bytes']} "
        f"sha256={result['table_sha256']}"
    )
    print(
        "stock H2/SPI3 bitstream: "
        f"gowin_preamble_offset={result['gowin_preamble_offset']} "
        f"idcode_offset={result['gowin_idcode_offset']}"
    )
    print(
        "stock SPI3 enable/PC6 order: "
        + " -> ".join(item["label"] for item in result["spi3_enable_pc6_sequence"])
    )
    print(
        "stock H2/SPI3 preamble: "
        + " -> ".join(item["label"] for item in result["preamble_sequence"])
    )
    print(
        "stock H2/SPI3 close: "
        + " -> ".join(item["label"] for item in result["close_sequence"])
    )
    print(
        "stock H2/SPI3 post-H2 queue: "
        + " -> ".join(
            f"{item['payload']}@{item['queue']}" for item in result["post_h2_queue_sequence"]
        )
    )
    print(
        "stock SPI3 trigger dispatch: "
        + " -> ".join(
            f"{item['trigger']}->{item['target']}" for item in result["spi3_trigger_dispatch"]
        )
    )


if __name__ == "__main__":
    main()
