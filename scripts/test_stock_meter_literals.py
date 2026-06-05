#!/usr/bin/env python3
"""Verify stock V1.2.0 DMM literal pools used by the multiplier notes."""

from __future__ import annotations

import hashlib
import math
import struct
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
BIN = REPO / "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
BASE = 0x08000000
EXPECTED_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
EXPECTED_METER_SELECTOR_TABLE = bytes.fromhex("14 0c 17 0b 0a 12 11 10")
EXPECTED_METER_SELECTOR_XREF_SEQUENCES = {
    0x080042E2: bytes.fromhex(
        "95 f8 2d 0f 4b f2 fc 32 41 1e 00 28 08 bf 07 21 "
        "c8 b2 c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 "
        "00 f5 a0 60 c2 f2 00 01 08 80"
    ),
    0x080048BA: bytes.fromhex(
        "95 f8 2d 0f 00 21 4b f2 fc 32 07 28 38 bf 41 1c "
        "c8 b2 c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 "
        "00 f5 a0 60 c2 f2 00 01 08 80"
    ),
}
EXPECTED_DVOM_TX_QUEUE_CONSUMER_SEQUENCES = {
    "dvom_tx_raw_word_consumer": (
        0x080373F4,
        bytes.fromhex(
            "82 b0 44 f2 0c 45 42 f6 74 56 40 f2 05 07 40 f2 "
            "0f 08 c4 f2 00 05 c2 f2 00 06 0d f1 06 04 c2 f2 "
            "00 07 c2 f2 00 08 4f f0 00 09 00 bf 30 68 21 46 "
            "4f f0 ff 32 03 f0 d6 fe 01 28 f7 d1 bd f8 06 00 "
            "88 f8 00 90 01 0a f8 70 00 eb 10 20 b9 70 78 72 "
            "28 68 40 f0 80 00 28 60 0a 20 02 f0 9f ff e5 e7"
        ),
    ),
}
EXPECTED_METER_TRANSPORT_TRANSITION_SEQUENCES = {
    "meter_transport_enable_resume_reset": (
        0x08026F8E,
        bytes.fromhex(
            "44 f2 0c 41 c4 f2 00 01 08 68 40 f4 00 50 08 60 "
            "42 f6 a0 50 c2 f2 00 00 00 68 13 f0 32 fb "
            "42 f6 a4 50 c2 f2 00 00 00 68 13 f0 2b fb "
            "41 f2 00 01 4f f4 00 60 c4 f2 01 01 08 61 "
            "00 20 c7 f6 c0 70 40 f2 01 11 ca f8 48 0f "
            "ca f8 4c 0f ca f8 50 0f 00 20 aa f8 35 1f "
            "ff 21 8a f8 5d 0f 8a f8 2d 0f 8a f8 2f 0f "
            "8a f8 38 1f aa f8 3c 0f"
        ),
    ),
    "meter_transport_disable_suspend_drain": (
        0x0802700A,
        bytes.fromhex(
            "44 f2 0c 41 c4 f2 00 01 08 68 20 f4 00 50 08 60 "
            "42 f6 a0 50 c2 f2 00 00 00 68 13 f0 b2 fb "
            "42 f6 a4 50 c2 f2 00 00 00 68 13 f0 ab fb "
            "4f f4 00 60 c8 f8 00 00 42 f6 7c 50 c2 f2 00 00 "
            "00 68 00 21 14 f0 ad f9 42 f6 74 50 c2 f2 00 00 "
            "00 68 00 21 13 f0 ed fd"
        ),
    ),
}
EXPECTED_RUNTIME_MODE_SWITCH_TRANSPORT_SEQUENCES = {
    "runtime_mode_switch_enable_resume_tail": (
        0x08007360,
        bytes.fromhex(
            "01 20 84 f8 68 0f 44 f2 0c 40 c4 f2 00 00 01 68 "
            "41 f4 00 51 01 60 42 f6 a0 50 c2 f2 00 00 00 68 "
            "33 f0 46 f9 42 f6 a4 50 c2 f2 00 00 00 68 33 f0 "
            "3f f9 41 f2 10 00 c4 f2 01 00 4f f4 00 61 01 60 "
            "00 20 c7 f6 c0 70 40 f2 01 11 c4 f8 48 0f c4 f8 "
            "4c 0f c4 f8 50 0f 00 20 a4 f8 35 1f ff 21 84 f8 "
            "5d 0f 84 f8 2f 0f 84 f8 38 1f a4 f8 3c 0f a4 f8 "
            "2c 1f a4 f8 69 0f 84 f8 6b 0f bd e8 10 40 04 f0 "
            "93 ba"
        ),
    ),
    "runtime_mode_switch_disable_suspend_drain": (
        0x0800741A,
        bytes.fromhex(
            "44 f2 0c 40 c4 f2 00 00 01 68 21 f4 00 51 01 60 "
            "42 f6 a0 50 c2 f2 00 00 00 68 33 f0 aa f9 42 f6 "
            "a4 50 c2 f2 00 00 00 68 33 f0 a3 f9 41 f2 14 00 "
            "c4 f2 01 00 4f f4 00 61 01 60 42 f6 7c 50 c2 f2 "
            "00 00 00 68 00 21 00 25 33 f0 a1 ff 42 f6 74 50 "
            "c2 f2 00 00 00 68 00 21 33 f0 e1 fb 01 20 84 f8 "
            "36 0f 00 20 c7 f6 c0 70 c4 f8 48 0f c4 f8 4c 0f "
            "c4 f8 50 0f a4 f8 3c 5f a4 f8 2d 5f c4 f8 30 5f "
            "0b e0"
        ),
    ),
    "runtime_mode_switch_active_epilogue": (
        0x080074BE,
        bytes.fromhex(
            "94 f8 54 13 02 20 84 f8 68 0f 00 20 09 07 a4 f8 "
            "69 0f 84 f8 6b 0f 06 d0 42 f6 50 50 c2 f2 00 00 "
            "4f f4 70 51 01 80 bd e8 b0 40 04 f0 0e ba"
        ),
    ),
}
EXPECTED_METER_SELECTOR_STATE_SEQUENCES = {
    "init_selector_reset": (
        0x08026FDE,
        bytes.fromhex(
            "00 20 aa f8 35 1f ff 21 8a f8 5d 0f "
            "8a f8 2d 0f 8a f8 2f 0f 8a f8 38 1f aa f8 3c 0f"
        ),
    ),
    "rx_force_mode_8": (
        0x08036D14,
        bytes.fromhex(
            "40 f2 f8 07 c2 f2 00 07 04 20 87 f8 35 0f "
            "08 20 00 24 87 f8 2d 0f c7 f8 30 4f"
        ),
    ),
    "rx_force_mode_1": (
        0x08036D50,
        bytes.fromhex("b1 20 86 f8 5d 0f 01 20 86 f8 2d 0f"),
    ),
    "rx_shadow_zero": (
        0x08037220,
        bytes.fromhex("00 20 87 f8 36 0f"),
    ),
    "rx_shadow_extended": (
        0x080372E0,
        bytes.fromhex(
            "49 07 4f f0 00 00 4f f0 02 01 87 f8 36 0f "
            "b2 79 58 bf c2 f3 80 11 87 f8 37 1f 03 21 87 f8 2f 1f"
        ),
    ),
    "rx_shadow_one": (
        0x08037328,
        bytes.fromhex("01 22 87 f8 36 2f"),
    ),
    "rx_shadow_two": (
        0x08037338,
        bytes.fromhex(
            "02 20 87 f8 36 0f 01 22 22 ea 01 00 "
            "87 f8 37 0f 87 f8 2f 2f"
        ),
    ),
    "rx_shadow_two_with_frame_bit": (
        0x080373A8,
        bytes.fromhex(
            "49 07 4f f0 02 02 b0 79 4f f0 02 01 87 f8 36 2f "
            "58 bf c0 f3 80 11 87 f8 37 1f 87 f8 2f 2f"
        ),
    ),
}
EXPECTED_ACV_FORMAT_SELECTOR_SEQUENCES = {
    "meter_mode_tbb_table": (
        0x080371C8,
        bytes.fromhex("15 30 3b 4b 59 68 04 04"),
    ),
    "acv_frame7_bit0_format_selector": (
        0x08037228,
        bytes.fromhex(
            "f0 79 42 f6 7c 5b c0 07 c2 f2 00 0b 42 d1 01 20 "
            "87 f8 37 0f 8c e0 97 f8 36 1f f0 79 01 29 3d d1"
        ),
    ),
    "acv_frame7_bit0_format_selector_branch_target": (
        0x080372BC,
        bytes.fromhex("87 f8 37 4f 01 20 49 e0"),
    ),
}
EXPECTED_MUX_RESTORE_SEQUENCES = {
    0x08025544: bytes.fromhex("a0 78 dc f7 ad f9 e0 78 dc f7 84 fa"),
    0x0802723E: bytes.fromhex(
        "9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc"
    ),
}
EXPECTED_METER_SAVED_CONFIG_UNPACK_SEQUENCES = {
    "saved_config_meter_state_unpack": (
        0x08025D92,
        bytes.fromhex(
            "20 68 40 f2 f8 0a c1 b2 55 29 c2 f2 00 0a 05 d0 "
            "aa 29 40 f0 f8 81 08 21 8a f8 68 1f 01 0a 02 0c "
            "00 0e 8a f8 00 10 8a f8 01 20 8a f8 02 00 60 68 "
            "ca f8 03 00"
        ),
    ),
}
EXPECTED_MUX_CALLS_BY_TARGET = {
    "gpio_mux_portc_porte": {
        "target": 0x080018A4,
        "calls": {
            0x080020B2: bytes.fromhex("ff f7 f7 fb"),
            0x080031E8: bytes.fromhex("fe f7 5c fb"),
            0x080039A2: bytes.fromhex("fd f7 7f ff"),
            0x0801A53E: bytes.fromhex("e7 f7 b1 f9"),
            0x0801C7CC: bytes.fromhex("e5 f7 6a f8"),
            0x0801D094: bytes.fromhex("e4 f7 06 fc"),
            0x08025546: bytes.fromhex("dc f7 ad f9"),
            0x08027242: bytes.fromhex("da f7 2f fb"),
        },
    },
    "gpio_mux_porta_portb": {
        "target": 0x08001A58,
        "calls": {
            0x08001F06: bytes.fromhex("ff f7 a7 fd"),
            0x08003644: bytes.fromhex("fe f7 08 fa"),
            0x08003E3A: bytes.fromhex("fd f7 0d fe"),
            0x0801A534: bytes.fromhex("e7 f7 90 fa"),
            0x0801C7D8: bytes.fromhex("e5 f7 3e f9"),
            0x0801D0A0: bytes.fromhex("e4 f7 da fc"),
            0x0802554C: bytes.fromhex("dc f7 84 fa"),
            0x0802724A: bytes.fromhex("da f7 05 fc"),
        },
    },
}
EXPECTED_MUX_WRITER_BODY_SEQUENCES = {
    "gpio_mux_portc_porte": {
        "target": 0x080018A4,
        "slices": {
            "switch_prologue": (
                0x080018A4,
                bytes.fromhex("09 28 00 f2 88 80 df e8 00 f0 05 08 0b 1f 22 25"),
            ),
            "gpio_pc12_pe_write_block": (
                0x080018C4,
                bytes.fromhex(
                    "4f f6 00 01 41 f6 10 02 cf f6 ff 71 c4 f2 01 02 "
                    "4f f4 80 53 53 50 41 f6 14 01 c4 f2 01 01 "
                    "10 23 0b 60 20 23 13 60 64 e0"
                ),
            ),
            "scope_calibration_table_select": (
                0x080019BA,
                bytes.fromhex(
                    "40 f2 f8 01 c2 f2 00 01 91 f8 2d 20 05 2a 06 d3 "
                    "01 eb 40 02 02 f5 27 70 02 f5 18 72 11 e0 "
                    "04 2a 09 d0 0a 7d 03 2a 06 d0 01 eb 40 02 "
                    "02 f5 31 70 02 f5 22 72 05 e0 01 eb 40 02 "
                    "02 f5 2c 70 02 f5"
                ),
            ),
            "dac1_scope_tail": (
                0x08001A20,
                bytes.fromhex(
                    "47 f2 04 40 c4 f2 00 00 41 68 09 0b 20 ee 01 0a "
                    "01 ee 10 2a b8 ee 41 1a 30 ee 01 0a bc ee c0 0a "
                    "10 ee 10 2a 61 f3 1f 32 42 60 01 68 41 f0 01 01 "
                    "01 60 70 47"
                ),
            ),
        },
    },
    "gpio_mux_porta_portb": {
        "target": 0x08001A58,
        "slices": {
            "switch_prologue": (
                0x08001A58,
                bytes.fromhex("09 28 00 f2 bb 80 df e8 00 f0 05 08 0b 1e 38 4c"),
            ),
            "gpio_pa15_pb11_pb10_write_block": (
                0x08001A78,
                bytes.fromhex(
                    "4f f6 00 41 40 f6 10 42 cf f6 ff 71 c4 f2 01 02 "
                    "4f f4 00 43 53 50 40 f6 14 41 c4 f2 01 01 "
                    "4f f4 00 63 0b 60 27 e0 4f f6"
                ),
            ),
            "gpio_high_modes_write_block": (
                0x08001B82,
                bytes.fromhex(
                    "4f f6 04 41 40 f6 10 42 cf f6 ff 71 c4 f2 01 02 "
                    "4f f4 00 43 53 50 4f f4 00 63 13 60 "
                    "4f f4 80 63 13 60 15 e0 4f f6 04 41 "
                    "40 f6 10 42 cf f6 ff 71 c4 f2 01 02 "
                    "4f f4 00 43 53 50 4f f4 00 63 40 f6 14 4c "
                    "13 60 c4 f2 01 0c 4f f4 80 63 cc f8 00 30 53 50"
                ),
            ),
            "scope_calibration_table_select": (
                0x08001BD4,
                bytes.fromhex(
                    "40 f2 f8 01 c2 f2 00 01 91 f8 2d 20 05 2a 06 d3 "
                    "01 eb 40 02 02 f5 45 70 02 f5 36 72 11 e0 "
                    "04 2a 09 d0 0a 7d 03 2a 06 d0 01 eb 40 02 "
                    "02 f5 4f 70 02 f5 40 72 05 e0 01 eb 40 02 "
                    "02 f5 4a 70 02 f5 3b 72 00 88 12 88"
                ),
            ),
            "dac1_scope_tail": (
                0x08001C3A,
                bytes.fromhex(
                    "41 f6 34 40 c4 f2 00 00 20 ee 01 0a 01 ee 10 2a "
                    "b8 ee 41 1a 30 ee 01 0a bc ee c0 0a "
                    "80 ed 00 0a 70 47"
                ),
            ),
        },
    },
}
EXPECTED_RUNTIME_MUX_STATE_WRITER_SEQUENCES = {
    "siggen_scope_autorange_increment": (
        0x08001EE8,
        bytes.fromhex(
            "0b eb 0a 00 10 f8 02 1f 08 29 3f f6 f3 ae "
            "01 31 ba f1 00 0f 01 70 00 f0 d6 80 "
            "9b f8 03 00 ff f7 a7 fd d4 e0"
        ),
    ),
    "scope_main_autorange_increment": (
        0x0801A526,
        bytes.fromhex(
            "51 1c bb f1 00 0f 01 70 04 d0 9a f8 03 00 "
            "e7 f7 90 fa 03 e0 9a f8 02 00 e7 f7 b1 f9 "
            "42 f6 6c 50 c2 f2 00 00 00 68 04 21 8d f8 5d 10 "
            "0d f1 5d 01 4f f0 ff 32 40 f2 2d 15 20 f0"
        ),
    ),
}
EXPECTED_SCOPE_SNAPSHOT_CONSUMER_SEQUENCES = {
    "scope_measurement_snapshot_from_mux_state": (
        0x08034078,
        bytes.fromhex(
            "2d e9 f0 4f 81 b0 2d ed 04 8b 40 f2 f8 05 c2 f2 "
            "00 05 95 f8 2d 00 4a f6 ab 27 ca f6 aa 27 "
            "a0 fb 07 12 a9 78 85 f8 c0 0d 85 f8 c1 1d "
            "d5 f8 1a 10 b5 f8 b4 0d 4f ea 31 41 "
            "c5 f8 c6 1d a9 8a 6b 79 a5 f8 be 1d "
            "b5 f8 b6 1d a5 f8 e0 0d"
        ),
    ),
}
EXPECTED_SCOPE_PRESET_MUX_OWNER_SEQUENCES = {
    "scope_preset_mux_increment_prologue": (
        0x08003148,
        bytes.fromhex(
            "f0 b5 81 b0 2d ed 02 8b 40 f2 f8 05 c2 f2 00 05 "
            "95 f8 68 0f 01 38 08 28 00 f2 b3 83 df e8 10 f0"
        ),
    ),
    "scope_preset_mux_increment_portc_branch": (
        0x080031B6,
        bytes.fromhex(
            "05 eb d0 10 10 f8 02 1f 08 29 00 f2 83 83 01 31 "
            "01 70 95 f8 54 03 05 eb d0 11 89 78 40 b2 "
            "05 29 04 bf 0a 21 85 f8 bb 1d b0 f1 ff 3f "
            "40 f3 2e 82 a8 78 fe f7 5c fb"
        ),
    ),
    "scope_preset_mux_increment_portab_branch": (
        0x08003642,
        bytes.fromhex(
            "e8 78 fe f7 08 fa 42 f6 6c 57 42 f6 53 54 "
            "c2 f2 00 07 c2 f2 00 04 04 21 38 68 21 70 "
            "21 46 4f f0 ff 32 37 f0 44 fb"
        ),
    ),
    "scope_preset_mux_decrement_prologue": (
        0x08003900,
        bytes.fromhex(
            "f0 b5 81 b0 2d ed 02 8b 40 f2 f8 06 c2 f2 00 06 "
            "96 f8 68 0f 01 38 08 28 00 f2 4b 84 df e8 10 f0"
        ),
    ),
    "scope_preset_mux_decrement_portc_branch": (
        0x08003970,
        bytes.fromhex(
            "06 eb d0 10 10 f8 02 1f 00 29 00 f0 1a 84 01 39 "
            "01 70 96 f8 54 03 06 eb d0 11 89 78 40 b2 "
            "04 29 04 bf 0a 21 86 f8 bb 1d b0 f1 ff 3f "
            "40 f3 4c 82 b0 78 fd f7 7f ff"
        ),
    ),
    "scope_preset_mux_decrement_portab_branch": (
        0x08003E38,
        bytes.fromhex(
            "f0 78 fd f7 0d fe 42 f6 6c 55 42 f6 53 54 "
            "c2 f2 00 05 c2 f2 00 04 04 21 28 68 21 70 "
            "21 46 4f f0 ff 32 36 f0 49 ff"
        ),
    ),
}
EXPECTED_SCOPE_UI_MUX_LUT_CONSUMER_SEQUENCES = {
    "scope_ui_draw_main_mux_lut_consumer": (
        0x080151B0,
        bytes.fromhex(
            "40 f2 f8 08 c2 f2 00 08 98 f8 16 00 4a f6 ab 23 "
            "40 44 81 78 ca f6 aa 23 ca b2 a2 fb 03 23 b8 f9 "
            "1c 20 90 f9 04 00 5c 08 10 1a 00 ee 10 0a a4 eb "
            "84 00 08 44 4b f6 b8 71 c0 b2 c0 f6 04 01 31 f8 "
            "10 00"
        ),
    ),
}


def read(addr: int, size: int) -> bytes:
    data = BIN.read_bytes()
    off = addr - BASE
    return data[off : off + size]


def expect_f64(addr: int, expected: float) -> None:
    actual = struct.unpack("<d", read(addr, 8))[0]
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.0):
        raise AssertionError(f"{addr:#010x}: expected f64 {expected}, got {actual}")


def expect_f32(addr: int, expected: float) -> None:
    actual = struct.unpack("<f", read(addr, 4))[0]
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.0):
        raise AssertionError(f"{addr:#010x}: expected f32 {expected}, got {actual}")


def expect_bytes(addr: int, expected_hex: str) -> None:
    expected = bytes.fromhex(expected_hex)
    actual = read(addr, len(expected))
    if actual != expected:
        raise AssertionError(
            f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
        )


def verify_meter_selector_table() -> dict[str, object]:
    """Check the stock eight-entry DMM 0x05xx selector low-byte table.

    The decompile notes refer to the app-slot runtime literal at 0x080BB3FC.
    This archived app image is read with base 0x08000000, so the same bytes are
    at 0x080B43FC / file offset 0x000B43FC after the documented app-slot
    correction.  Keep this binary-grounded so local DMM submode policy cannot
    drift into invented selector bytes.
    """
    runtime_addr = 0x080BB3FC
    app_image_addr = 0x080B43FC
    actual = read(app_image_addr, len(EXPECTED_METER_SELECTOR_TABLE))
    if actual != EXPECTED_METER_SELECTOR_TABLE:
        raise AssertionError(
            f"{runtime_addr:#010x}/file {app_image_addr:#010x}: "
            f"expected {EXPECTED_METER_SELECTOR_TABLE.hex(' ')}, got {actual.hex(' ')}"
        )
    return {
        "runtime_addr": runtime_addr,
        "app_image_addr": app_image_addr,
        "bytes": actual.hex(" "),
        "words": [f"0x05{b:02X}" for b in actual],
    }


def verify_meter_selector_xref_sequences() -> dict[str, object]:
    """Check stock code paths that consume the eight-entry selector table.

    The two guarded sequences are the decrement/increment selector branches
    documented in `high_flash_scope_indexing_2026_04_08.md`.  They load the
    runtime literal `0x080BB3FC`, read one table byte indexed by
    `DAT_20001025`, add `0x0500`, and store the raw UART halfword to
    `0x20002D54` before queueing it through `0x20002D74`.

    This proves the raw selector-table consumer path only.  It does not recover
    the analog mux bytes `ms[0x02]`/`ms[0x03]` or any physical calibration.
    """
    checked: dict[str, str] = {}
    for addr, expected in EXPECTED_METER_SELECTOR_XREF_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
            )
        checked[f"{addr:#010x}"] = actual.hex(" ")
    return {"sequences": checked}


def verify_dvom_tx_queue_consumer_sequences() -> dict[str, object]:
    """Check the stock dvom_TX task that consumes raw DMM/FPGA TX words.

    `FUN_080373F4` blocks on queue handle `0x20002D74`, receives one
    halfword, clears the USART2 TX index at `0x2000000F`, splits the word into
    the TX frame buffer at `0x20000005 + 2/3`, writes the byte-sum at `+9`,
    and sets USART2 CTRL1 bit 7 (`0x80`) to start the TX interrupt pump.
    This proves that selector-table producers feeding `0x20002D74` reach the
    stock USART2 command channel; it still does not prove analog mux bytes or
    meter calibration.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_DVOM_TX_QUEUE_CONSUMER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_meter_transport_transition_sequences() -> dict[str, object]:
    """Check stock DMM transport enable/resume and disable/drain slices.

    The boot/config transition code enables USART2 and resumes the two DVOM
    tasks before resetting meter display/selector state.  Its paired disable
    path clears USART2 enable, suspends the same task handles, clears PC11,
    resets the meter semaphore, and drains the raw TX-word queue at
    `0x20002D74`.  Guard these as stock transport sequencing evidence; the
    exact local settle/discard constants remain OpenScope policy.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_TRANSPORT_TRANSITION_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_runtime_mode_switch_transport_sequences() -> dict[str, object]:
    """Check runtime mode-switch transport pause/drain/resume slices.

    `mode_switch_handler` around 0x080073E4 contains the UI/runtime transition
    side of the same stock DMM transport behavior: one tail enables USART2,
    resumes both DVOM tasks, asserts PC11, and clears meter display/selector
    state; the meter-entry case disables USART2, suspends both DVOM tasks,
    clears PC11, resets `0x20002D7C` and `0x20002D74`, and clears stale meter
    state before the active epilogue.  This guards runtime transition evidence,
    not a recovered analog range writer or exact settle/discard count.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_RUNTIME_MODE_SWITCH_TRANSPORT_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_meter_selector_state_sequences() -> dict[str, object]:
    """Check stock selector/shadow-state writers used by the DMM FSM.

    These sequences prove RAM-state coupling around `DAT_20001025` and adjacent
    formatter bytes: init clears the selector, RX classification can force mode
    8 or mode 1, and later RX branches update `DAT_2000102E`/`DAT_2000102F`/
    `DAT_20001027` before the display formatter consumes them.

    This is still digital DMM FSM evidence only.  It does not recover the
    analog mux bytes `ms[0x02]`/`ms[0x03]` or any factory calibration source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SELECTOR_STATE_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_acv_format_selector_sequences() -> dict[str, object]:
    """Check stock ACV submode dispatch and `frame[7].0` format selection.

    The guarded TBB table maps stock DMM submode 1 to the ACV case at
    `0x08037228`.  That case reads meter frame byte 7, tests bit 0, and writes
    the formatter state byte at `[r7,#0xf37]` (`DAT_2000102F`-adjacent local
    display state).  This is stock evidence for ACV decimal/format selection.
    It is explicitly not AC-evidence; local AC confidence still has to come
    from independent frequency/AC metadata and must reject DC input in ACV.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_ACV_FORMAT_SELECTOR_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_meter_mux_restore_sequences() -> dict[str, object]:
    """Check stock saved-state ms[0x02]/ms[0x03] mux apply call sites.

    These instruction bytes are the binary evidence behind
    `meter_mode_command_table_2026_06_05.md`: master init loads saved
    meter-state byte 2, calls `gpio_mux_portc_porte`, then loads saved byte 3
    and calls `gpio_mux_porta_portb`.  This proves boot/saved-state apply
    evidence only; it does not prove a runtime DMM range writer.
    """
    checked: dict[str, str] = {}
    for addr, expected in EXPECTED_MUX_RESTORE_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{addr:#010x}: expected {expected.hex(' ')}, got {actual.hex(' ')}"
            )
        checked[f"{addr:#010x}"] = actual.hex(" ")
    return {"sequences": checked}


def verify_meter_saved_config_unpack_sequences() -> dict[str, object]:
    """Check stock saved-config unpack into `ms[0x02]`/`ms[0x03]`.

    Master init reads persistent config at `0x08006000`, accepts signatures
    `0x55` and `0xAA`, optionally writes meter-mode state `8`, then unpacks
    word 0 into `ms[0x00]`, `ms[0x01]`, `ms[0x02]` and stores word 1 at
    `ms[0x03]`.  This is the recovered persistent-state writer feeding the
    guarded mux apply calls.  It is not evidence of a runtime DMM range writer
    during local mode switching.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SAVED_CONFIG_UNPACK_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_meter_mux_callsite_sequences() -> dict[str, object]:
    """Check every stock direct BL callsite to the two mux writers.

    This is a binary callsite guard, not a DMM mode/range proof.  The stock
    decompile exposes scope/siggen runtime callers plus DMM-relevant boot and
    saved-state apply sites.  Keeping the complete direct BL list guarded makes
    it harder to mistake partial xrefs for recovered DMM runtime mux writers.
    """
    checked: dict[str, object] = {}
    for name, info in EXPECTED_MUX_CALLS_BY_TARGET.items():
        calls: dict[int, bytes] = info["calls"]  # type: ignore[assignment]
        target = int(info["target"])
        call_bytes: dict[str, str] = {}
        for addr, expected in calls.items():
            actual = read(addr, len(expected))
            if actual != expected:
                raise AssertionError(
                    f"{name} call {addr:#010x}: "
                    f"expected {expected.hex(' ')}, got {actual.hex(' ')}"
                )
            call_bytes[f"{addr:#010x}"] = actual.hex(" ")
        checked[name] = {
            "target": f"{target:#010x}",
            "calls": sorted(call_bytes),
            "sequences": call_bytes,
        }
    return checked


def verify_meter_mux_writer_body_sequences() -> dict[str, object]:
    """Check stock GPIO mux writer body slices and their scope-calibration tails.

    These are the bodies that the recovered ms[0x02]/ms[0x03] saved-state
    apply sites and the direct BL callsite list target.  The guarded slices
    cover the 10-way switch prologues, representative GPIO BOP/BCR pin writes,
    and the trailing DAC1/scope calibration-table recompute.  This proves the
    hardware writer shape; it still does not prove a DMM runtime range writer
    or a DMM calibration coefficient.
    """
    checked: dict[str, object] = {}
    for name, info in EXPECTED_MUX_WRITER_BODY_SEQUENCES.items():
        slices: dict[str, tuple[int, bytes]] = info["slices"]  # type: ignore[assignment]
        checked_slices: dict[str, dict[str, str]] = {}
        for slice_name, (addr, expected) in slices.items():
            actual = read(addr, len(expected))
            if actual != expected:
                raise AssertionError(
                    f"{name} {slice_name} {addr:#010x}: "
                    f"expected {expected.hex(' ')}, got {actual.hex(' ')}"
                )
            checked_slices[slice_name] = {
                "addr": f"{addr:#010x}",
                "bytes": actual.hex(" "),
            }
        checked[name] = {
            "target": f"{int(info['target']):#010x}",
            "slices": checked_slices,
        }
    return checked


def verify_runtime_mux_state_writer_sequences() -> dict[str, object]:
    """Check recovered runtime writes to the DAT_200000fa/fb mux-state pair.

    The text decompile exposes two runtime increment/write paths for
    `(&DAT_200000fa)[idx]`: one in `FUN_08001c60` and one in `FUN_08019e98`.
    Both are scope/siggen autorange paths that call the mux writers and queue a
    scope frontend update.  Guard these as negative DMM evidence: the inspected
    runtime writer set is not the DMM `ms[0x02]`/`ms[0x03]` range writer.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_RUNTIME_MUX_STATE_WRITER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_scope_snapshot_consumer_sequences() -> dict[str, object]:
    """Check stock scope measurement snapshots that consume current mux state.

    `FUN_08034078` copies `DAT_20000125`, `DAT_200000fa`,
    `DAT_2000010c`, `DAT_20000112`, `DAT_200000fd`, and `DAT_200000fb`
    into the `DAT_20000eb8..DAT_20000ebe` snapshot block before scope
    measurement/display math indexes waveform scale tables.  Guard it as a
    consumer/snapshot path, not a DMM mux writer or calibration source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_SCOPE_SNAPSHOT_CONSUMER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_scope_preset_mux_owner_sequences() -> dict[str, object]:
    """Check stock scope/preset mux-owner handlers around 0x08003148/0x08003900.

    These paired handlers increment/decrement `(&DAT_200000fa)[idx]` using
    the selector byte at `DAT_2000044c`, then dispatch to `FUN_080018a4` for
    channel 0 or `FUN_08001a58` for channel 1 and queue command `4`.  They are
    scope/preset UI owners for the mux-state pair, not DMM runtime range
    writers tied to the eight-entry meter selector table.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_SCOPE_PRESET_MUX_OWNER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def verify_scope_ui_mux_lut_consumer_sequences() -> dict[str, object]:
    """Check stock scope UI scale/LUT reads that consume current mux state.

    `FUN_08015f50` (`scope_ui_draw_main`) reads `DAT_2000010e` as a channel
    index, loads `(&DAT_200000fa)[idx]`, derives the modulo-3 LUT index, and
    reads `DAT_0804bfb8` before scope display math.  This is a scope render
    consumer of the mux-state pair, not a DMM range writer or calibration
    source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_SCOPE_UI_MUX_LUT_CONSUMER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }
    return {"sequences": checked}


def main() -> None:
    if not BIN.exists():
        print(f"stock meter literal pools: skipped; missing {BIN}", file=sys.stderr)
        return

    digest = hashlib.sha256(BIN.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise AssertionError(f"{BIN}: expected sha256 {EXPECTED_SHA256}, got {digest}")

    for addr, value in (
        (0x08036C60, 1000.0),
        (0x08036C68, 0.5),
        (0x08036C70, 0.0),
        (0x08036C78, 100.0),
        (0x08036C80, 10.0),
        (0x08036C90, 4.0),
        (0x080373D0, 3.0),
        (0x080373D8, 2.0),
        (0x080373E0, 1.0),
        (0x080373E8, 0.0),
        (0x08002BF0, 1000.0),
        (0x08002BF8, 0.0),
        (0x08002C00, 10000.0),
    ):
        expect_f64(addr, value)

    for addr, value in (
        (0x08036C88, 10000.0),
        (0x080373F0, 32.0),
        (0x08002C08, 1000.0),
        (0x08002C0C, 100.0),
    ):
        expect_f32(addr, value)

    expect_bytes(0x08036C8C, "00 bf 00 bf")
    selector = verify_meter_selector_table()
    selector_xrefs = verify_meter_selector_xref_sequences()
    dvom_tx_consumers = verify_dvom_tx_queue_consumer_sequences()
    transport_transitions = verify_meter_transport_transition_sequences()
    runtime_transport_transitions = verify_runtime_mode_switch_transport_sequences()
    selector_state = verify_meter_selector_state_sequences()
    acv_format = verify_acv_format_selector_sequences()
    mux_restore = verify_meter_mux_restore_sequences()
    saved_config_unpack = verify_meter_saved_config_unpack_sequences()
    mux_calls = verify_meter_mux_callsite_sequences()
    mux_bodies = verify_meter_mux_writer_body_sequences()
    runtime_mux_writers = verify_runtime_mux_state_writer_sequences()
    scope_snapshots = verify_scope_snapshot_consumer_sequences()
    scope_preset_mux_owners = verify_scope_preset_mux_owner_sequences()
    scope_ui_mux_lut_consumers = verify_scope_ui_mux_lut_consumer_sequences()
    print(f"stock meter selector table: {selector['bytes']}")
    print("stock meter selector xref sites: " +
          ", ".join(selector_xrefs["sequences"].keys()))
    print("stock dvom_TX raw-word consumer sites: " +
          ", ".join(item["addr"] for item in dvom_tx_consumers["sequences"].values()))
    print("stock meter transport transition sites: " +
          ", ".join(item["addr"] for item in transport_transitions["sequences"].values()))
    print("stock runtime mode-switch transport sites: " +
          ", ".join(item["addr"] for item in runtime_transport_transitions["sequences"].values()))
    print("stock meter selector state sites: " +
          ", ".join(item["addr"] for item in selector_state["sequences"].values()))
    print("stock ACV format selector sites: " +
          ", ".join(item["addr"] for item in acv_format["sequences"].values()))
    print("stock meter mux restore sites: " +
          ", ".join(mux_restore["sequences"].keys()))
    print("stock meter saved-config unpack sites: " +
          ", ".join(item["addr"] for item in saved_config_unpack["sequences"].values()))
    for name, info in mux_calls.items():
        print(f"stock {name} direct BL sites: " + ", ".join(info["calls"]))
    for name, info in mux_bodies.items():
        print(f"stock {name} body slices: " +
              ", ".join(info["slices"].keys()))
    print("stock runtime mux-state writer sites: " +
          ", ".join(item["addr"] for item in runtime_mux_writers["sequences"].values()))
    print("stock scope snapshot consumer sites: " +
          ", ".join(item["addr"] for item in scope_snapshots["sequences"].values()))
    print("stock scope/preset mux owner sites: " +
          ", ".join(item["addr"] for item in scope_preset_mux_owners["sequences"].values()))
    print("stock scope UI mux-LUT consumer sites: " +
          ", ".join(item["addr"] for item in scope_ui_mux_lut_consumers["sequences"].values()))
    print("stock meter literal pools: ok")


if __name__ == "__main__":
    main()
