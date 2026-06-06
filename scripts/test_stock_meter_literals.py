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
RAM_MAP = REPO / "reverse_engineering/analysis_v120/ram_map.txt"
FULL_DECOMPILE = REPO / "reverse_engineering/analysis_v120/full_decompile.c"
BASE = 0x08000000
EXPECTED_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
EXPECTED_METER_SELECTOR_TABLE = bytes.fromhex("14 0c 17 0b 0a 12 11 10")
EXPECTED_MUX_STATE_RAM_MAP_REFS = {
    "DAT_200000fa": {
        "addr": "0x200000FA",
        "count": 25,
        "refs": [
            "FUN_08034078@08034078",
            "FUN_08001c60@08001c60",
            "FUN_08019e98@08019e98",
            "FUN_0801f6f8@0801f6f8",
            "FUN_0801d2ec@0801d2ec",
            "FUN_0801efc0@0801efc0",
            "unknown@080151c2",
        ],
    },
    "DAT_200000fb": {
        "addr": "0x200000FB",
        "count": 11,
        "refs": [
            "FUN_08034078@08034078",
            "FUN_08001c60@08001c60",
            "FUN_08019e98@08019e98",
            "FUN_0801f6f8@0801f6f8",
            "FUN_0801d2ec@0801d2ec",
        ],
    },
}
EXPECTED_MUX_STATE_FULL_DECOMPILE_REFS = {
    "DAT_200000fa": [
        (2564, "bVar2 = (&DAT_200000fa)[uVar20];"),
        (2566, "(&DAT_200000fa)[uVar20] = bVar2 + 1;"),
        (2568, "FUN_080018a4(DAT_200000fa);"),
        (2603, "uVar6 = (uint)DAT_200000fa;"),
        (2710, "uVar6 = (uint)DAT_200000fa;"),
        (6881, "bVar37 = (&DAT_200000fa)[uVar70];"),
        (6999, "uVar49 = (uint)DAT_200000fa;"),
        (7456, "iVar46 = (uint)DAT_200000fa * 2;"),
        (7461, "iVar46 = (uint)DAT_200000fa * 2;"),
        (8617, "(&DAT_080465cc + (uint)(byte)(&DAT_200000fa)[uVar58] * 2),"),
        (8745, "(&DAT_200000fa)[uVar70] = bVar37 + 1;"),
        (8747, "FUN_080018a4(DAT_200000fa);"),
        (9078, "(&DAT_080465cc + (uint)(byte)(&DAT_200000fa)[uVar49] * 2),"),
        (9841, "((uint)*(ushort *)(&DAT_080465cc + (uint)DAT_200000fa * 2),"),
        (11043, "[(uint)(byte)(&DAT_200000fa)[uVar6] % 3] << 2,"),
        (11046, "if ((byte)(&DAT_200000fa)[uVar6] < 3) {"),
        (
            11050,
            "uVar2 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar6] - 3) / 3));",
        ),
        (11072, "[(uint)(byte)(&DAT_200000fa)[uVar6] % 3] << 2,"),
        (11075, "if ((byte)(&DAT_200000fa)[uVar6] < 3) {"),
        (
            11080,
            "uVar2 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar6] - 3) / 3));",
        ),
        (11100, "[(uint)(byte)(&DAT_200000fa)[uVar7] % 3] << 2,"),
        (11103, "if ((byte)(&DAT_200000fa)[uVar7] < 3) {"),
        (
            11108,
            "uVar2 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar7] - 3) / 3));",
        ),
        (11411, "pbVar19 = &DAT_200000fa + uVar22;"),
        (26025, "DAT_20000eb9 = DAT_200000fa;"),
        (26145, "DAT_20000eb9 = DAT_200000fa;"),
    ],
    "DAT_200000fb": [
        (2571, "FUN_08001a58(DAT_200000fb);"),
        (2660, "uVar6 = (uint)DAT_200000fb;"),
        (2734, "uVar6 = (uint)DAT_200000fb;"),
        (7021, "uVar49 = (uint)DAT_200000fb;"),
        (7474, "iVar46 = (uint)DAT_200000fb * 2;"),
        (7479, "iVar46 = (uint)DAT_200000fb * 2;"),
        (8750, "FUN_08001a58(DAT_200000fb);"),
        (9969, "((uint)*(ushort *)(&DAT_080465cc + (uint)DAT_200000fb * 2),"),
        (26035, "_DAT_20000eba = _DAT_200000fb;"),
        (26155, "_DAT_20000eba = _DAT_200000fb;"),
    ],
}
EXPECTED_MUX_STATE_FULL_DECOMPILE_PAIR_WRITES = {
    2566: {
        "text": "(&DAT_200000fa)[uVar20] = bVar2 + 1;",
        "target": "DAT_200000fa/DAT_200000fb selected by uVar20",
        "classification": "scope/siggen autorange increment in FUN_08001c60",
    },
    8745: {
        "text": "(&DAT_200000fa)[uVar70] = bVar37 + 1;",
        "target": "DAT_200000fa/DAT_200000fb selected by uVar70",
        "classification": "scope_main_fsm autorange increment in FUN_08019e98",
    },
}
EXPECTED_MUX_STATE_PAIR_WRITE_CONTEXTS = {
    "siggen_scope_autorange_pair_write_context": [
        (2564, "bVar2 = (&DAT_200000fa)[uVar20];"),
        (2565, "if (bVar2 < 9) {"),
        (2566, "(&DAT_200000fa)[uVar20] = bVar2 + 1;"),
        (2567, "if (uVar20 == 0) {"),
        (2568, "FUN_080018a4(DAT_200000fa);"),
        (2569, "}"),
        (2570, "else {"),
        (2571, "FUN_08001a58(DAT_200000fb);"),
        (2572, "}"),
        (2573, "local_31 = 4;"),
    ],
    "scope_main_fsm_autorange_pair_write_context": [
        (8745, "(&DAT_200000fa)[uVar70] = bVar37 + 1;"),
        (8746, "if (uVar70 == 0) {"),
        (8747, "FUN_080018a4(DAT_200000fa);"),
        (8748, "}"),
        (8749, "else {"),
        (8750, "FUN_08001a58(DAT_200000fb);"),
        (8751, "}"),
        (8752, "local_6b = 4;"),
        (8753, "FUN_0803acf0(_DAT_20002d6c,&local_6b,0xffffffff);"),
    ],
}
EXPECTED_MODE_STATE_RAM_MAP_REF = {
    "symbol": "DAT_20001060",
    "addr": "0x20001060",
    "count": 7,
    "refs": [
        "FUN_08009014@08009014",
        "FUN_08019e98@08019e98",
        "unknown@0800b914",
        "unknown@08015848",
        "FUN_080096e8@080096e8",
        "FUN_08009a94@08009a94",
    ],
    "classification": (
        "overloaded mode-init/command-bank/transport state byte; not DMM "
        "ms[0x02]/ms[0x03] analog range state"
    ),
}
EXPECTED_SAVED_MODE_F64_RAM_MAP_REF = {
    "symbol": "DAT_2000105c",
    "addr": "0x2000105C",
    "count": 2,
    "refs": [
        "FUN_08015f50@08015f50",
    ],
    "classification": (
        "display-menu compare plus boot/config saved-mode restore byte; not "
        "DMM ms[0x02]/ms[0x03] analog range state"
    ),
}
EXPECTED_SAVED_MODE_F64_SEQUENCES = {
    "saved_mode_f64_config_load": (
        0x08025E40,
        bytes.fromhex(
            "d4 e9 0b 01 04 f1 38 03 ca f8 60 0f 08 0c 0a 0e "
            "aa f8 64 1f 8a f8 08 00 8a f8 09 20 60 6b 01 0c "
            "aa f8 0a 00"
        ),
    ),
    "saved_mode_f64_to_live_f68_restore": (
        0x08026F50,
        bytes.fromhex(
            "9a f8 64 0f a0 b1 8a f8 68 0f 01 28 17 d0 03 28"
        ),
    ),
}
EXPECTED_SCOPE_MEASUREMENT_ENGINE_MUX_POINTER_CONSUMER_CONTEXT = {
    "scope_measurement_engine_mux_pointer_consumer_context": {
        "line_range": (11411, 11491),
        "required_lines": [
            (11411, "pbVar19 = &DAT_200000fa + uVar22;"),
            (11412, "bVar3 = *pbVar19;"),
            (
                11418,
                "(&DAT_0804bfb8 + ((bVar3 / 3) * -3 + (uint)bVar3 & 0xff) * 2),",
            ),
            (11435, "uVar31 = *pbVar19 / 3;"),
            (11438, "uVar6 = *(undefined2 *)(&DAT_0804bfb8 + ((uint)*pbVar19 + uVar31 * -3 & 0xff) * 2);"),
            (11488, "bVar3 = *pbVar19;"),
            (11491, "uVar6 = *(undefined2 *)(&DAT_0804bfb8 + (uVar18 & 0xff) * 2);"),
        ],
        "forbidden_substrings": ["*pbVar19 =", "pbVar19[0] ="],
        "classification": "scope_measurement_engine read-only mux-state pointer consumer, not a writer",
    },
}
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
EXPECTED_METER_SELECTOR_ADJUST_SEQUENCES = {
    "selector_adjust_prev_prologue": (
        0x080041F8,
        bytes.fromhex(
            "f0 b5 85 b0 40 f2 f8 05 c2 f2 00 05 95 f8 68 0f "
            "09 28 00 f2 db 82 df e8 10 f0 0a 00 61 00 ca 00 "
            "eb 00 0b 01 3d 00 2a 01 d9 02 d9 02 42 01"
        ),
    ),
    "selector_adjust_prev_meter_case": (
        0x080042D4,
        bytes.fromhex(
            "95 f8 5d 0f 00 f0 f0 00 b0 28 00 f0 71 82 95 f8 "
            "2d 0f 4b f2 fc 32 41 1e 00 28 08 bf 07 21 c8 b2 "
            "c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 00 f5 "
            "a0 60 c2 f2 00 01 08 80 42 f6 74 50 c2 f2 00 00 "
            "00 68 00 26 4f f0 ff 32 85 f8 5d 6f 36 f0 e6 fc "
            "42 f6 6c 57 42 f6 53 54 c2 f2 00 07 c2 f2 00 04 "
            "1d 21 38 68 21 70 21 46 4f f0 ff 32 36 f0 d6 fc "
            "1b 21 38 68 21 70 21 46 4f f0 ff 32 36 f0 ce fc "
            "95 f8 2d 0f 95 f8 3c 1f 02 28 18 bf 01 20 85 f8 "
            "36 0f 00 20 c7 f6 c0 70 c5 f8 48 0f c5 f8 4c 0f "
            "00 29 c5 f8 50 0f 18 bf 85 f8 3c 6f 95 f8 3d 0f "
            "50 b1 00 20 85 f8 3d 0f 1a 21 38 68 21 70 21 46 "
            "4f f0 ff 32 36 f0 aa fc 05 b0 bd e8 f0 40 fe f7 "
            "9d ba"
        ),
    ),
    "selector_adjust_next_prologue": (
        0x080047CC,
        bytes.fromhex(
            "f0 b5 85 b0 40 f2 f8 05 c2 f2 00 05 95 f8 68 0f "
            "09 28 00 f2 cd 81 df e8 10 f0 0a 00 63 00 cc 00 "
            "ed 00 05 01 41 00 23 01 cb 01 cb 01 3d 01"
        ),
    ),
    "selector_adjust_next_meter_case": (
        0x080048AC,
        bytes.fromhex(
            "95 f8 5d 0f 00 f0 f0 00 b0 28 00 f0 61 81 95 f8 "
            "2d 0f 00 21 4b f2 fc 32 07 28 38 bf 41 1c c8 b2 "
            "c0 f6 0b 02 10 5c 85 f8 2d 1f 42 f6 54 51 00 f5 "
            "a0 60 c2 f2 00 01 08 80 42 f6 74 50 c2 f2 00 00 "
            "00 68 00 26 4f f0 ff 32 85 f8 5d 6f 36 f0 fa f9 "
            "42 f6 6c 57 42 f6 53 54 c2 f2 00 07 c2 f2 00 04 "
            "1d 21 38 68 21 70 21 46 4f f0 ff 32 36 f0 ea f9 "
            "1b 21 38 68 21 70 21 46 4f f0 ff 32 36 f0 e2 f9 "
            "95 f8 2d 0f 95 f8 3c 1f 02 28 18 bf 01 20 85 f8 "
            "36 0f 00 20 c7 f6 c0 70 c5 f8 48 0f c5 f8 4c 0f "
            "00 29 c5 f8 50 0f 18 bf 85 f8 3c 6f 95 f8 3d 0f "
            "50 b1 00 20 85 f8 3d 0f 1a 21 38 68 21 70 21 46 "
            "4f f0 ff 32 36 f0 be f9 05 b0 bd e8 f0 40 fd f7 "
            "b1 bf"
        ),
    ),
}
EXPECTED_DYNAMIC_RAW_WORD_HELPER_SEQUENCES = {
    "selector_seed_blocker_gate": (
        0x0800604E,
        bytes.fromhex(
            "b0 2a 2b d1 01 31 01 f0 0f 02 53 1e 04 2b 80 f8 "
            "5d 1f"
        ),
    ),
    "selector_seed_state_pairs": (
        0x08006060,
        bytes.fromhex(
            "33 d8 01 22 01 21 df e8 03 f0 2c 03 24 27 2a 00 "
            "05 21 03 22 25 e0 40 f2 01 31 42 f6 6c 55 a0 f8 "
            "69 1f 00 21 42 f6 53 54 c2 f2 00 05 80 f8 6b 1f "
            "c2 f2 00 04 13 21 28 68 21 70 21 46 4f f0 ff 32 "
            "34 f0 26 fe 28 68 14 23 31 e0 00 21 80 f8 5d 1f "
            "b0 bd 07 21 05 22 04 e0 08 21 06 22 01 e0 0a 21 "
            "07 22 80 f8 2d 2f 80 f8 2e 1f"
        ),
    ),
    "selector_seed_emit_0501": (
        0x080060CA,
        bytes.fromhex(
            "42 f6 74 50 42 f6 54 51 c2 f2 00 00 c2 f2 00 01 "
            "40 f2 01 52 00 68 0a 80 4f f0 ff 32 34 f0 03 fe "
            "42 f6 6c 55 42 f6 53 54 c2 f2 00 05 c2 f2 00 04 "
            "1d 21 28 68 21 70 21 46 4f f0 ff 32 34 f0 f3 fd "
            "28 68 1b 23 21 46 4f f0 ff 32 23 70 bd e8 b0 40 "
            "34 f0 e9 bd"
        ),
    ),
    "dynamic_raw_word_gate_and_mask": (
        0x08006120,
        bytes.fromhex(
            "70 b5 40 f2 f8 05 c2 f2 00 05 95 f8 68 0f 05 28 "
            "35 d0 02 28 42 d0 01 28 49 d1 95 f8 5d 0f 00 f0 "
            "f0 00 b0 28 43 d0 95 f8 2d 0f 00 26 00 21 07 28 "
            "c7 f6 c0 76 85 f8 5d 1f 00 f2 bd 80 01 22 02 fa "
            "00 f1 11 f0 c6 0f 00 f0 b6 80 95 f8 36 1f 01 38 "
            "01 29 42 f6 54 54 08 bf 02 22 06 28 c2 f2 00 04 "
            "85 f8 36 2f 00 f2 80 80 df e8 00 f0"
        ),
    ),
    "dynamic_raw_word_lowbyte_pair_0c_0d": (
        0x08006194,
        bytes.fromhex("0c 20 01 29 08 bf 0d 20 73 e0"),
    ),
    "dynamic_raw_word_lowbyte_pairs_0e17_1116_1015": (
        0x0800626A,
        bytes.fromhex(
            "0e 20 01 29 08 bf 17 20 08 e0 11 20 01 29 08 bf "
            "16 20 03 e0 10 20 01 29 08 bf 15 20 20 80"
        ),
    ),
    "dynamic_raw_word_emit_tail": (
        0x08006288,
        bytes.fromhex(
            "c5 f8 48 6f c5 f8 4c 6f c5 f8 50 6f fc f7 24 fb "
            "20 88 4f f0 ff 32 40 f4 a0 61 42 f6 74 50 c2 f2 "
            "00 00 00 68 21 80 21 46 34 f0 1e fd 42 f6 6c 50 "
            "42 f6 53 51 c2 f2 00 00 c2 f2 00 01 1b 22 00 68 "
            "0a 70 4f f0 ff 32 34 f0"
        ),
    ),
    "dynamic_helper_reverse_partner_gate": (
        0x080062F8,
        bytes.fromhex(
            "b0 b5 40 f2 f8 00 c2 f2 00 00 90 f8 68 1f 05 29 "
            "0d d0 02 29 3e d0 01 29 3b d1 90 f8 5d 1f 01 f0 "
            "f0 01 b0 29 35 d0 00 21 80 f8 5d 1f"
        ),
    ),
    "dynamic_helper_reverse_partner_display": (
        0x08006326,
        bytes.fromhex(
            "90 f8 1c 1e 00 29 18 bf b0 bd 90 f8 1b 1e 49 b3 "
            "90 f8 1a 1e 02 22 02 29 08 bf 01 22 02 39 42 f6 "
            "6c 55 18 bf 4f f0 3f 31 42 f6 53 54 c2 f2 00 05 "
            "80 f8 1a 2e c0 f8 16 1e c0 f8 12 1e c2 f2 00 04 "
            "26 21 28 68 21 70 21 46 4f f0 ff 32 34 f0 bd fc "
            "28 68 28 23 21 46 4f f0 ff 32 23 70 bd e8 b0 40 "
            "34 f0"
        ),
    ),
}
EXPECTED_DYNAMIC_RAW_WORD_APPLY_LOWBYTE_PAIRS = [
    {"selector_low": "0x0C", "apply_low": "0x0D", "stock_mode": "ACV"},
    {"selector_low": "0x17", "apply_low": "0x0E", "stock_mode": "DCA"},
    {"selector_low": "0x11", "apply_low": "0x16", "stock_mode": "continuity"},
    {"selector_low": "0x10", "apply_low": "0x15", "stock_mode": "diode"},
]
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
EXPECTED_METER_BASIC_RAW_WORD_SEQUENCES = {
    "meter_basic_configure_0508": {
        "addr": 0x080033CA,
        "word": "0x0508",
        "classification": (
            "stock raw-word queue materializer for the meter configure word; "
            "command sequencing only, not a DMM range writer or calibration"
        ),
        "bytes": bytes.fromhex(
            "42 f6 74 50 42 f6 54 51 c2 f2 00 00 c2 f2 00 01 "
            "00 68 4f f4 a1 63 4f f0 ff 32 0b 80 19 e1"
        ),
    },
    "meter_basic_start_0509": {
        "addr": 0x08003BA4,
        "word": "0x0509",
        "classification": (
            "stock raw-word queue materializer for the meter start/poll word; "
            "command sequencing only, not a DMM range writer or calibration"
        ),
        "bytes": bytes.fromhex(
            "42 f6 74 50 42 f6 54 51 c2 f2 00 00 c2 f2 00 01 "
            "00 68 40 f2 09 53 4f f0 ff 32 0b 80 28 e2"
        ),
    },
    "meter_basic_variant_0514": {
        "addr": 0x08005B7A,
        "word": "0x0514",
        "classification": (
            "stock raw-word queue materializer for the meter variant/setup word; "
            "command sequencing only, not a DMM range writer or calibration"
        ),
        "bytes": bytes.fromhex(
            "42 f6 74 50 42 f6 54 51 c2 f2 00 00 c2 f2 00 01 "
            "40 f2 14 52 00 68 00 26 0a 80 4f f0 ff 32 "
            "85 f8 5d 6f"
        ),
    },
}
EXPECTED_ROLL_BUFFER_PRELOAD_SEQUENCES = {
    "roll_buffer_transform_entry": (
        0x08001830,
        bytes.fromhex(
            "02 f0 ff 03 43 ea 03 22 42 ea 02 42 ff f7 40 bd "
            "10 b5 20 3a c0 f0 0b 80 b1 e8 18 50 a0 e8 18 50 "
            "b1 e8 18 50 a0 e8 18 50 20 3a bf f4 f5 af"
        ),
    ),
    "master_init_roll_buffer_callers": (
        0x080271A8,
        bytes.fromhex(
            "9a f8 04 10 0a f2 56 30 81 f0 80 02 40 f2 2d 11 "
            "da f7 3a fb a0 07 45 f2 34 04 c4 f2 01 04 11 d4 "
            "9a f8 05 10 0a f2 83 40 81 f0 80 02 40 f2 2d 11 "
            "da f7 2a fb 06 e0 02 20 45 f2 34 04 8a f8 15 00"
        ),
    ),
}
EXPECTED_METER_TRANSPORT_TRANSITION_SEQUENCES = {
    "boot_saved_mode_init_state_restore": (
        0x08026F50,
        bytes.fromhex(
            "9a f8 64 0f a0 b1 8a f8 68 0f 01 28 17 d0 "
            "03 28 4c d0 02 28 51 d1 9a f8 54 03 00 07 "
            "42 f6 50 50 c2 f2 00 00 0c bf 00 21 4f f4 "
            "70 51 01 80 44 e0 9a f8 68 0f 01 21 8a f8 "
            "69 1f 01 28 e7 d1"
        ),
    ),
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
EXPECTED_DISPLAY_FORMATTER_DISPATCH_SEQUENCES = {
    "display_formatter_mode_switch_cases": (
        0x08002AA0,
        bytes.fromhex(
            "6c 55 07 28 c2 f2 00 05 00 f2 87 80 df e8 00 f0 "
            "04 1b 27 2f 12 38 42 4a 98 f8 2f 0f 03 28 72 d8 "
            "05 21 df e8 00 f0 02 5a 64 69 00 20 88 f8 2e 0f "
            "ff 20 66 e0 98 f8 37 1f 06 20 88 f8 2e 0f 48 1d "
            "88 f8 38 0f 69 e0 98 f8 36 0f 41 1e 02 29 38 bf "
            "88 f8 2e 0f 98 f8 37 0f 88 f8 38 0f 5d e0 98 f8 "
            "36 0f 01 28 26 d0 02 28 57 d1 03 20 00 e0 05 20 "
            "98 f8 37 1f 88 f8 2e 0f 88 1c 88 f8 38 0f 4c e0 "
            "98 f8 37 1f 07 20 88 f8 2e 0f 01 f1 09 00 88 f8 "
            "38 0f 42 e0 98 f8 36 0f 02 28 13 d0 01 28 16 d1 "
            "08 20 12 e0 98 f8 36 0f 02 28 0d d0 01 28 0e d1 "
            "0a 20 0a e0 98 f8 37 1f 04 20 88 f8 2e 0f 88 f8 "
            "38 1f 2a e0 09 20 00 e0 0b 20 88 f8 2e 0f 98 f8 "
            "37 0f 0a 30 88 f8 38 0f 1f e0 98 f8 36 0f 41 1e "
            "02 29 38 bf 88 f8 2e 0f 98 f8 37 0f 09 e0 98 f8 "
            "36 0f 02 28 02 d1 03 21 88 f8 2e 1f 98 f8 37 0f "
            "02 30 88 f8 38 0f"
        ),
    ),
    "display_formatter_mode5_extended": (
        0x08002B20,
        bytes.fromhex(
            "98 f8 37 1f 07 20 88 f8 2e 0f 01 f1 09 00 88 f8 38 0f"
        ),
    ),
    "display_formatter_modes6_7_unit_offsets": (
        0x08002B34,
        bytes.fromhex(
            "98 f8 36 0f 02 28 13 d0 01 28 16 d1 08 20 12 e0 "
            "98 f8 36 0f 02 28 0d d0 01 28 0e d1 0a 20 0a e0 "
            "98 f8 37 1f 04 20 88 f8 2e 0f 88 f8 38 1f 2a e0 "
            "09 20 00 e0 0b 20 88 f8 2e 0f 98 f8 37 0f 0a 30 "
            "88 f8 38 0f"
        ),
    ),
}
EXPECTED_CURRENT_FORMATTER_VARIANT_SEQUENCES = {
    "display_formatter_dca_variant_units": (
        0x08002AFE,
        bytes.fromhex(
            "98 f8 36 0f 01 28 26 d0 02 28 57 d1 03 20 00 e0 "
            "05 20 98 f8 37 1f 88 f8 2e 0f 88 1c 88 f8 38 0f"
        ),
    ),
    "display_formatter_dca_variant_one_target": (
        0x08002B54,
        bytes.fromhex(
            "98 f8 37 1f 04 20 88 f8 2e 0f 88 f8 38 1f 2a e0"
        ),
    ),
}
EXPECTED_UNIT_LOOKUP_BOUNDARY_SEQUENCES = {
    "display_unit_lookup_zero_region": (
        0x0804C40C,
        bytes.fromhex(
            "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
            "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
            "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        ),
    ),
    "display_unit_lookup_draw_call": (
        0x08009AE4,
        bytes.fromhex(
            "95 f8 60 0f 4c f2 3c 42 95 f8 2e 1f 00 eb 40 00 "
            "c0 f6 04 02 02 eb 00 10 00 eb 81 00 50 f8 30 0c"
        ),
    ),
}
EXPECTED_MUX_RESTORE_SEQUENCES = {
    0x08025544: bytes.fromhex("a0 78 dc f7 ad f9 e0 78 dc f7 84 fa"),
    0x0802723E: bytes.fromhex(
        "9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc"
    ),
}
EXPECTED_METER_AUX_AFE_PIN_INIT_SEQUENCES = {
    "pb9_pa6_output_config_only": (
        0x080241D4,
        bytes.fromhex(
            "4f f4 00 70 18 90 28 46 21 46 0c f0 8d f8 "
            "40 20 18 90 40 f6 00 00 c4 f2 01 00 21 46 0c f0 84 f8"
        ),
    ),
}
FORBIDDEN_METER_AUX_AFE_DIRECT_LEVEL_WRITES = [
    "_DAT_40010c10 = 0x200",
    "_DAT_40010c14 = 0x200",
    "_DAT_40010810 = 0x40",
    "_DAT_40010814 = 0x40",
]
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
EXPECTED_METER_SAVED_CONFIG_LIVE_MUX_STORE_SEQUENCES = {
    "saved_config_live_mux_store": (
        0x08025D94,
        bytes.fromhex(
            "40 f2 f8 0a c1 b2 55 29 c2 f2 00 0a 05 d0 aa 29 "
            "40 f0 f8 81 08 21 8a f8 68 1f 01 0a 02 0c 00 0e "
            "8a f8 00 10 8a f8 01 20 8a f8 02 00 60 68 ca f8 "
            "03 00"
        ),
    ),
}
EXPECTED_METER_SAVED_CONFIG_PACK_SEQUENCES = {
    "saved_config_meter_state_pack_reads": (
        0x08022410,
        bytes.fromhex(
            "39 7e 97 f8 2d 90 05 91 79 8b 3a 78 06 91 4f ea "
            "09 21 07 91 b9 8b be 78 09 04 0d 91 b7 f8 34 12 "
            "4f ea 02 2c 09 04 03 91 b7 f8 32 12 32 06 0a 92 "
            "fa 7d 04 91 b7 f8 36 12 7b 78 3d 7d bc 7d 12 06 "
            "00 91 b7 f8 38 12 1e 04"
        ),
    ),
    "saved_config_meter_state_default_seed": (
        0x080224A0,
        bytes.fromhex(
            "00 21 c0 f2 05 51 4c f6 32 62 c7 e9 00 12 03 22 "
            "3a 75 4f f4 80 72 fa 82"
        ),
    ),
    "saved_config_meter_state_pack_writes": (
        0x0802258A,
        bytes.fromhex(
            "4c ea 0e 04 26 43 0a 9c 26 43 ca f8 00 60 d7 f8 "
            "03 60 09 9c ca f8 04 60 fe 79 26 43 32 43 08 9e "
            "32 43 ca f8 08 20 07 9a 05 9e 32 43"
        ),
    ),
}
EXPECTED_METER_SAVED_CONFIG_PACK_CALLER_SEQUENCES = {
    "housekeeping_threshold_saved_config_pack_caller": (
        0x08002F80,
        bytes.fromhex(
            "6d af 7b e7 09 29 de d1 e7 e7 55 20 1f f0 16 fa"
        ),
    ),
    "post_function_literal_pool_bl_shaped_bytes": (
        0x08002F90,
        bytes.fromhex(
            "cd cc cc cc cc cc 4c 3f 3d 0a d7 a3 70 3d 10 40 "
            "d7 a3 70 3d 0a d7 0f 40 5c 8f c2 f5 28 5c 0f 40 "
            "f6 28 5c 8f c2 f5 0e 40 8f c2 f5 28 5c 8f 0e 40 "
            "52 b8 1e 85 eb 51 0e 40 29 5c 8f c2 f5 28 0e 40 "
            "ec 51 b8 1e 85 eb 0d 40 71 3d 0a d7 a3 70 0d 40 "
            "55 20 1f f0 eb f9 00 00"
        ),
    ),
    "branch_island_bl_shaped_bytes_before_selector_seed": (
        0x08005B40,
        bytes.fromhex(
            "bd e8 f0 41 35 f0 d4 b8 00 20 1c f0 37 fc 00 00 "
            "2d e9 f0 43 81 b0 40 f2 f8 05"
        ),
    ),
    "probe_change_poweroff_saved_config_pack_caller": (
        0x080396F4,
        bytes.fromhex(
            "a0 b1 b1 f8 6c 2f 03 28 02 f1 01 02 a1 f8 6c 2f "
            "08 d0 02 28 0b d0 01 28 08 d1 90 b2 b0 f5 61 7f "
            "04 d9 09 e0 90 b2 b0 f5 61 6f 05 d8 80 bd 90 b2 "
            "b0 f5 e1 6f 98 bf 80 bd 55 20 e8 f7 45 fe 00 00"
        ),
    ),
}
EXPECTED_METER_SAVED_CONFIG_PACK_DIRECT_CALLS = [
    0x08002F8C,
    0x08002FE2,
    0x08005B4A,
    0x0803972E,
]
EXPECTED_METER_SAVED_CONFIG_PACK_CALLER_CLASSES = {
    "0x08002f8c": (
        "executable housekeeping threshold path; not normal runtime DMM range switching"
    ),
    "0x08002fe2": (
        "direct-BL-shaped bytes inside post-function literal/data region"
    ),
    "0x08005b4a": (
        "direct-BL-shaped bytes in branch island before selector seed function"
    ),
    "0x0803972e": (
        "controlled shutdown/config-save path; not normal runtime DMM range switching"
    ),
}
EXPECTED_USART_TX_CONFIG_WRITER_SEQUENCES = {
    "writer_tbb_prologue": (
        0x08039734,
        bytes.fromhex(
            "10 b5 0a 78 06 2a 88 bf 10 bd df e8 02 f0 "
            "04 98 1c 98 43 98 6c 00"
        ),
    ),
    "meter_case_bitfield_body": (
        0x080397C8,
        bytes.fromhex(
            "02 46 91 f8 01 c0 52 f8 20 3f 0c f0 01 0c "
            "23 f4 00 73 43 ea 4c 23 13 60 91 f8 01 c0 "
            "d2 f8 00 e0 cc f3 54 03 63 f3 cb 2e c2 f8 "
            "00 e0 91 f8 02 c0 50 f8 1c 3f 4f f4 80 74 "
            "6c f3 01 03 03 60 c9 78 03 68 09 01 5f fa "
            "81 fc 23 f0 f0 0e 6f f0 0c 03 22 e0"
        ),
    ),
    "writer_common_update_mask_commit": (
        0x08039860,
        bytes.fromhex(
            "4e ea 0c 01 01 60 01 68 19 40 01 60 "
            "10 68 20 43 10 60 10 bd"
        ),
    ),
}
EXPECTED_USART_TX_CONFIG_WRITER_CALLER_SEQUENCES = {
    "tim5_init_config_writer_call": (
        0x080272CC,
        bytes.fromhex(
            "4f f4 80 30 10 90 38 46 12 f0 2e fa "
            "b8 68 05 21 61 f3 06 10 b8 60"
        ),
    ),
    "tim2_init_config_writer_call": (
        0x08027338,
        bytes.fromhex("02 20 c0 f2 01 00 10 90 4f f0 80 40 12 f0 f6 f9"),
    ),
}
EXPECTED_USART_TX_CONFIG_WRITER_DIRECT_CALLS = [0x080272D4, 0x08027344]
EXPECTED_BOOT_MODE_INIT_DMM_SEQUENCES = {
    "mode_init_dispatcher_tbh": (
        0x0800B908,
        bytes.fromhex(
            "b0 b5 82 b0 40 f2 f8 00 c2 f2 00 00 90 f8 68 0f "
            "09 28 00 f2 d8 81 42 f6 6c 55 c2 f2 00 05 df e8 "
            "10 f0 0a 00 56 00 a1 00 d2 00 1d 01 4a 01 80 01 "
            "82 01 be 01"
        ),
    ),
    "meter_basic_boot_probe_prefix": (
        0x0800B9D6,
        bytes.fromhex(
            "00 21 28 68 0d f1 07 04 8d f8 07 10 21 46 4f f0 "
            "ff 32 2f f0 82 f9 09 21 28 68 8d f8 07 10 21 46 "
            "4f f0 ff 32 2f f0 79 f9 41 f2 08 00 c4 f2 01 00 "
            "00 68 07 21 00 06 58 bf 0a 21 28 68 8d f8 07 10 "
            "21 46 4f f0 ff 32 2f f0 68 f9"
        ),
    ),
    "meter_basic_boot_range_tail": (
        0x0800BA20,
        bytes.fromhex(
            "1a 21 28 68 8d f8 07 10 21 46 4f f0 ff 32 2f f0 "
            "5f f9 1b 21 28 68 8d f8 07 10 21 46 4f f0 ff 32 "
            "2f f0 56 f9 1c 21 28 68 8d f8 07 10 21 46 4f f0 "
            "ff 32 2f f0 4d f9 1d 21 28 68 8d f8 07 10 21 46 "
            "4f f0 ff 32 2f f0 44 f9 1e 20 27 e1"
        ),
    ),
    "meter_extended_boot_probe_prefix": (
        0x0800BACE,
        bytes.fromhex(
            "00 21 28 68 0d f1 07 04 8d f8 07 10 21 46 4f f0 "
            "ff 32 2f f0 06 f9 08 21 28 68 8d f8 07 10 21 46 "
            "4f f0 ff 32 2f f0 fd f8 09 21 28 68 8d f8 07 10 "
            "21 46 4f f0 ff 32 2f f0 f4 f8 41 f2 08 00 c4 f2 "
            "01 00 00 68 07 21 00 06 58 bf 0a 21 28 68 8d f8 "
            "07 10 21 46"
        ),
    ),
    "meter_extended_boot_range_tail": (
        0x0800BB2A,
        bytes.fromhex(
            "16 21 28 68 8d f8 07 10 21 46 4f f0 ff 32 2f f0 "
            "da f8 17 21 28 68 8d f8 07 10 21 46 4f f0 ff 32 "
            "2f f0 d1 f8 18 21 28 68 8d f8 07 10 21 46 4f f0 "
            "ff 32 2f f0 c8 f8 19 20 ab e0"
        ),
    ),
    "meter_variant_boot_tail": (
        0x0800BC32,
        bytes.fromhex(
            "00 21 28 68 0d f1 07 04 8d f8 07 10 21 46 4f f0 "
            "ff 32 2f f0 54 f8 12 21 28 68 8d f8 07 10 21 46 "
            "4f f0 ff 32 2f f0 4b f8 13 21 28 68 8d f8 07 10 "
            "21 46 4f f0 ff 32 2f f0 42 f8 14 21 28 68 8d f8 "
            "07 10 21 46 4f f0 ff 32 2f f0 39 f8 09 21 28 68 "
            "8d f8 07 10 21 46 4f f0 ff 32 2f f0 30 f8 41 f2 "
            "08 00 c4 f2 01 00 00 68 00 06 4f f0 07 00 58 bf "
            "0a 20"
        ),
    ),
}
EXPECTED_BOOT_MODE_INIT_DMM_DIRECT_CALLS = [
    0x08002DAA,
    0x080051D6,
    0x0800533A,
    0x08005572,
    0x080271F8,
]
EXPECTED_BOOT_MODE_INIT_DMM_COMMAND_BANKS = {
    "meter_basic_boot_probe_prefix": {
        "commands": ["0x00", "0x09", "0x07/0x0A probe branch"],
        "ordered_snippets": [
            "00 21 28 68",
            "09 21 28 68",
            "07 21 00 06 58 bf 0a 21",
        ],
    },
    "meter_basic_boot_range_tail": {
        "commands": ["0x1A", "0x1B", "0x1C", "0x1D", "0x1E"],
        "ordered_snippets": [
            "1a 21 28 68",
            "1b 21 28 68",
            "1c 21 28 68",
            "1d 21 28 68",
            "1e 20",
        ],
    },
    "meter_extended_boot_probe_prefix": {
        "commands": ["0x00", "0x08", "0x09", "0x07/0x0A probe branch"],
        "ordered_snippets": [
            "00 21 28 68",
            "08 21 28 68",
            "09 21 28 68",
            "07 21 00 06 58 bf 0a 21",
        ],
    },
    "meter_extended_boot_range_tail": {
        "commands": ["0x16", "0x17", "0x18", "0x19"],
        "ordered_snippets": [
            "16 21 28 68",
            "17 21 28 68",
            "18 21 28 68",
            "19 20",
        ],
    },
    "meter_variant_boot_tail": {
        "commands": ["0x00", "0x12", "0x13", "0x14", "0x09", "0x07/0x0A probe branch"],
        "ordered_snippets": [
            "00 21 28 68",
            "12 21 28 68",
            "13 21 28 68",
            "14 21 28 68",
            "09 21 28 68",
            "07 00 58 bf 0a 20",
        ],
    },
}
EXPECTED_METER_PROBE_BRANCH_GUARDS = {
    "meter_basic_boot_probe_prefix": {
        "addr": 0x0800B9D6,
        "source_register": "GPIOC_IDR 0x40011008",
        "source_snippet": "41 f2 08 00 c4 f2 01 00 00 68",
        "branch_snippet": "07 21 00 06 58 bf 0a 21",
        "polarity": "PC7 high keeps 0x07; PC7 low selects 0x0A via IT PL",
        "classification": (
            "probe/tail sequencing only; not DMM runtime range state, "
            "low-DCV correction, or factory calibration"
        ),
    },
    "meter_extended_boot_probe_prefix": {
        "addr": 0x0800BACE,
        "source_register": "GPIOC_IDR 0x40011008",
        "source_snippet": "41 f2 08 00 c4 f2 01 00 00 68",
        "branch_snippet": "07 21 00 06 58 bf 0a 21",
        "polarity": "PC7 high keeps 0x07; PC7 low selects 0x0A via IT PL",
        "classification": (
            "probe/tail sequencing only; not DMM runtime range state, "
            "low-DCV correction, or factory calibration"
        ),
    },
    "meter_variant_boot_tail": {
        "addr": 0x0800BC32,
        "source_register": "GPIOC_IDR 0x40011008",
        "source_snippet": "41 f2 08 00 c4 f2 01 00 00 68",
        "branch_snippet": "00 06 4f f0 07 00 58 bf 0a 20",
        "polarity": "PC7 high keeps 0x07; PC7 low selects 0x0A via IT PL",
        "classification": (
            "probe/tail sequencing only; not DMM runtime range state, "
            "low-DCV correction, or factory calibration"
        ),
    },
}
EXPECTED_RUNTIME_MODE_INIT_DISPATCH_CALLER_SEQUENCES = {
    "runtime_mode_init_forward_dispatcher": (
        0x08006418,
        bytes.fromhex(
            "b0 b5 40 f2 f8 00 c2 f2 00 00 90 f8 68 1f 01 39 "
            "08 29 53 d8 df e8 01 f0 05 0f 52 52 1c 4e 52 52 53 00"
        ),
    ),
    "runtime_mode_init_forward_state2_seed": (
        0x0800644E,
        bytes.fromhex(
            "40 f2 09 11 c0 f2 05 01 c0 f8 68 1f 01 21 80 f8 "
            "55 13 bd e8 b0 40 05 f0 50 ba"
        ),
    ),
    "runtime_mode_init_forward_latch_collapse_to_state2": (
        0x080064E0,
        bytes.fromhex(
            "90 f8 6a 1f 05 29 0d d1 00 21 02 22 80 f8 55 13 "
            "80 f8 68 2f a0 f8 69 1f 80 f8 6b 1f bd e8 b0 40 "
            "05 f0 02 ba"
        ),
    ),
    "runtime_mode_init_reverse_dispatcher": (
        0x08006548,
        bytes.fromhex(
            "b0 b5 40 f2 f8 00 c2 f2 00 00 90 f8 68 1f 01 39 "
            "08 29 0c d8 df e8 01 f0 05 0c 0b 0b 19 29 0b 0b 30 00"
        ),
    ),
    "runtime_mode_init_reverse_state2_seed": (
        0x08006578,
        bytes.fromhex(
            "40 f2 09 11 c0 f2 02 01 c0 f8 68 1f 01 21 80 f8 "
            "55 13 bd e8 b0 40 05 f0 bb b9"
        ),
    ),
    "runtime_mode_init_reverse_state_clear_to_state2": (
        0x08006592,
        bytes.fromhex(
            "02 21 80 f8 68 1f 00 21 a0 f8 1c 1e c0 f8 12 1e "
            "c0 f8 16 1e 80 f8 1a 1e bd e8 b0 40 05 f0 ab b9"
        ),
    ),
    "runtime_mode_init_reverse_state5_seed": (
        0x080065B2,
        bytes.fromhex("05 21 80 f8 68 1f bd e8 b0 40 05 f0 a4 b9"),
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
EXPECTED_MUX_WRITER_LITERAL_POINTER_REFS = {
    "gpio_mux_portc_porte": {
        "target": 0x080018A4,
        "forms": {
            "even": 0x080018A4,
            "thumb": 0x080018A5,
        },
        "expected_refs": [],
        "classification": (
            "no static 32-bit literal/function-pointer refs in the APP image; "
            "direct BL sites and saved-state callers remain the recovered mux "
            "writer surface, not hidden DMM runtime range proof"
        ),
    },
    "gpio_mux_porta_portb": {
        "target": 0x08001A58,
        "forms": {
            "even": 0x08001A58,
            "thumb": 0x08001A59,
        },
        "expected_refs": [],
        "classification": (
            "no static 32-bit literal/function-pointer refs in the APP image; "
            "direct BL sites and saved-state callers remain the recovered mux "
            "writer surface, not hidden DMM runtime range proof"
        ),
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
EXPECTED_MUX_WRITER_SCOPE_TAIL_CONTEXTS = {
    "gpio_mux_portc_porte_scope_tail_context": {
        "line_range": (2274, 2293),
        "classification": (
            "scope threshold/calibration tail: DAT_20000125/DAT_2000010c "
            "select scope tables, DAT_200000fc offsets the threshold, DAC1 is "
            "updated; not a DMM calibration coefficient"
        ),
        "snippets": [
            "if (DAT_20000125 < 5) {",
            "if ((DAT_20000125 == 4) || (DAT_2000010c == '\\x03')) {",
            "fVar5 = (float)VectorSignedToFloat(DAT_200000fc + 100",
            "_DAT_40007408 = uVar4 & 0xfff | _DAT_40007408 & 0xfffff000;",
            "_DAT_40007404 = _DAT_40007404 | 1;",
        ],
    },
    "gpio_mux_porta_portb_scope_tail_context": {
        "line_range": (2375, 2392),
        "classification": (
            "scope threshold/calibration tail: DAT_20000125/DAT_2000010c "
            "select scope tables, DAT_200000fd offsets the threshold, TIM/DAC "
            "threshold state is updated; not a DMM calibration coefficient"
        ),
        "snippets": [
            "if (DAT_20000125 < 5) {",
            "if ((DAT_20000125 == 4) || (DAT_2000010c == '\\x03')) {",
            "fVar5 = (float)VectorSignedToFloat(DAT_200000fd + 100",
            "_DAT_40001c34 = VectorFloatToUnsigned(",
        ],
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
EXPECTED_SCOPE_SUBMODE_MUX_CALL_SEQUENCES = {
    "scope_submode_post_calibration_mux_restore": (
        0x0801C7B8,
        bytes.fromhex(
            "1e f0 9a fa 40 f2 f8 04 c2 f2 00 04 "
            "94 f8 30 00 00 f0 0f 00 e5 f7 6a f8 "
            "94 f8 30 00 00 f0 0f 00 e5 f7 3e f9 "
            "ff f7 58 bb"
        ),
    ),
    "scope_submode_runtime_mux_restore": (
        0x0801D088,
        bytes.fromhex(
            "00 0a 4e 46 96 f8 30 00 00 f0 0f 00 "
            "e4 f7 06 fc 96 f8 30 00 00 f0 0f 00 "
            "e4 f7 da fc 96 f8 32 00 b2 46 ff 28"
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
EXPECTED_WATCHDOG_RELOAD_STATE_SEQUENCES = {
    "init_iwdg_reload_from_DAT_2000105a": (
        0x08027372,
        bytes.fromhex(
            "9a f8 62 0f 4e f2 10 07 00 28 1e bf 45 f2 04 41 "
            "c4 f2 01 01 08 63 42 f6 20 30 c2 f2 00 00"
        ),
    ),
    "button_task_watchdog_reload_loop": (
        0x08039008,
        bytes.fromhex(
            "82 b0 42 f6 70 56 40 f2 f8 07 46 f2 48 58 45 f2 "
            "34 49 00 25 c2 f2 00 06 0d f1 07 04 c2 f2 00 07 "
            "c0 f6 04 08 c4 f2 01 09 8d f8 07 50 08 e0 00 bf "
            "97 f8 62 0f 00 28 18 bf c9 f8 00 00 a7 f8 6c 5f "
            "30 68 21 46 4f f0 ff 32 02 f0 c2 f8 01 28 f7 d1 "
            "97 f8 10 0e 00 f0 fe 01 02 29 03 d1 87 f8 10 5e "
            "04 e0 00 bf 01 28 e3 d0 ff 28 e1 d0 97 f8 2f 00 "
            "97 f8 30 10 97 f8 33 20 08 43 97 f8 2c 1f 10 43 "
            "08 43 d5 d1 9d f8 07 00 08 eb 80 00 50 f8 04 0c "
            "80 47 cd e7"
        ),
    ),
}
EXPECTED_SCOPE_MUX_STATE_CONSUMER_SEQUENCES = {
    "scope_timebase_ch1_mux_scale_consumer": (
        0x0801D2EC,
        bytes.fromhex(
            "2d e9 f0 4f 87 b0 40 f2 f8 08 c2 f2 00 08 98 f8 "
            "2c 00 00 28 00 f0 39 81 98 f8 2e 00 00 28 00 f0 "
            "34 81 98 f8 02 10 46 f2 cc 50 c0 f6 04 00 30 f8 "
            "11 10 98 f9 04 20 00 ee 10 1a b8 ee"
        ),
    ),
    "scope_timebase_ch2_mux_scale_consumer": (
        0x0801D8B8,
        bytes.fromhex(
            "98 f8 03 10 98 f9 05 20 30 f8 11 10 03 ee 10 2a "
            "02 ee 10 1a"
        ),
    ),
    "scope_math_delta_ch1_mux_scale_consumer": (
        0x0801F51E,
        bytes.fromhex(
            "b1 7d 96 ed 93 1a 70 18 82 78 a2 fb 05 37 7b 08 "
            "03 eb 43 03 d2 1a a2 5c 92 00 00 ee 10 2a b8 ee "
            "c0 0a 21 ee 00 0a 86 ed 93 0a 80 78 03 28 04 d2"
        ),
    ),
    "scope_math_delta_ch2_mux_scale_consumer": (
        0x0801F5FC,
        bytes.fromhex(
            "b1 7d 96 ed 94 1a 70 18 82 78 a2 fb 05 37 7b 08 "
            "03 eb 43 03 d2 1a a2 5c 92 00 00 ee 10 2a b8 ee "
            "c0 0a 21 ee 00 0a 86 ed 94 0a 80 78 03 28 04 d2"
        ),
    ),
    "scope_measurement_engine_mux_scale_consumer": (
        0x0801FD66,
        bytes.fromhex(
            "09 90 0d f1 66 00 cd f8 30 80 18 f8 02 1f 13 f9 "
            "04 2f 10 f8 0a 00 0d 93 cb b2 a3 fb 0b 37 80 1a "
            "0a eb 4a 02 0e eb 02 1b 80 38 7c 08 cb f8 68 00"
        ),
    ),
}


def read(addr: int, size: int) -> bytes:
    data = BIN.read_bytes()
    off = addr - BASE
    return data[off : off + size]


def find_direct_thumb_bl_callers(target: int) -> list[int]:
    data = BIN.read_bytes()
    callers: list[int] = []
    for off in range(0, len(data) - 3, 2):
        h1, h2 = struct.unpack_from("<HH", data, off)
        if (h1 & 0xF800) != 0xF000 or (h2 & 0xD000) != 0xD000:
            continue
        sign = (h1 >> 10) & 1
        imm10 = h1 & 0x03FF
        j1 = (h2 >> 13) & 1
        j2 = (h2 >> 11) & 1
        imm11 = h2 & 0x07FF
        i1 = (~(j1 ^ sign)) & 1
        i2 = (~(j2 ^ sign)) & 1
        imm = (
            (sign << 24) |
            (i1 << 23) |
            (i2 << 22) |
            (imm10 << 12) |
            (imm11 << 1)
        )
        if sign:
            imm -= 1 << 25
        addr = BASE + off
        if addr + 4 + imm == target:
            callers.append(addr)
    return callers


def find_literal_word_refs(value: int) -> list[int]:
    data = BIN.read_bytes()
    needle = value.to_bytes(4, "little")
    refs: list[int] = []
    start = 0
    while True:
        off = data.find(needle, start)
        if off < 0:
            break
        refs.append(BASE + off)
        start = off + 1
    return refs


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


def _parse_ram_map_mux_state_refs() -> dict[str, dict[str, object]]:
    lines = RAM_MAP.read_text(encoding="utf-8", errors="replace").splitlines()
    parsed: dict[str, dict[str, object]] = {}
    for symbol in EXPECTED_MUX_STATE_RAM_MAP_REFS:
        for line in lines:
            if f" {symbol} " not in line:
                continue
            before_refs, refs_text = line.split(": ", 1)
            parts = before_refs.split()
            parsed[symbol] = {
                "addr": parts[0],
                "count": int(parts[2].strip("()")),
                "refs": [item.strip() for item in refs_text.split(",")],
                "line": line,
            }
            break
        else:
            raise AssertionError(f"{RAM_MAP}: missing {symbol} entry")
    return parsed


def verify_mux_state_ram_map_boundary() -> dict[str, object]:
    """Check the stock RAM-map xref boundary for `DAT_200000fa/fb`.

    `DAT_200000fa` and `DAT_200000fb` are the saved-state mux bytes that feed
    `gpio_mux_portc_porte` / `gpio_mux_porta_portb` during boot/saved-state
    restore.  The current V1.2.0 Ghidra RAM map exposes their function-level
    refs as scope/siggen writers and scope consumers only.  Guarding this map
    keeps future DMM work from citing an unclassified RAM-map hit as the missing
    runtime DMM `ms[0x02]`/`ms[0x03]` writer without first adding a real stock
    xref/trace.
    """
    parsed = _parse_ram_map_mux_state_refs()
    checked: dict[str, dict[str, object]] = {}
    for symbol, expected in EXPECTED_MUX_STATE_RAM_MAP_REFS.items():
        actual = parsed[symbol]
        if actual["addr"] != expected["addr"]:
            raise AssertionError(
                f"{symbol}: expected addr {expected['addr']}, got {actual['addr']}"
            )
        if actual["count"] != expected["count"]:
            raise AssertionError(
                f"{symbol}: expected count {expected['count']}, got {actual['count']}"
            )
        if actual["refs"] != expected["refs"]:
            raise AssertionError(
                f"{symbol}: expected refs {expected['refs']}, got {actual['refs']}"
            )
        checked[symbol] = actual
    return {"ram_map": str(RAM_MAP.relative_to(REPO)), "symbols": checked}


def verify_mode_state_ram_map_boundary() -> dict[str, object]:
    """Check the RAM-map boundary for `DAT_20001060` / `ms[0xF68]`.

    This byte gates stock mode-init, command-bank, and transport behavior, and
    the DMM selector/raw-word helper reads it. It is intentionally a different
    state layer from the missing DMM analog mux/range bytes at `ms[0x02]` and
    `ms[0x03]`; do not use an `ms[0xF68]` hit as a physical range writer.
    """
    lines = RAM_MAP.read_text(encoding="utf-8", errors="replace").splitlines()
    expected = EXPECTED_MODE_STATE_RAM_MAP_REF
    symbol = expected["symbol"]

    for line in lines:
        if f" {symbol} " not in line:
            continue
        before_refs, refs_text = line.split(": ", 1)
        parts = before_refs.split()
        actual = {
            "addr": parts[0],
            "count": int(parts[2].strip("()")),
            "refs": [item.strip() for item in refs_text.split(",")],
            "line": line,
            "classification": expected["classification"],
        }
        break
    else:
        raise AssertionError(f"{RAM_MAP}: missing {symbol} entry")

    if actual["addr"] != expected["addr"]:
        raise AssertionError(
            f"{symbol}: expected addr {expected['addr']}, got {actual['addr']}"
        )
    if actual["count"] != expected["count"]:
        raise AssertionError(
            f"{symbol}: expected count {expected['count']}, got {actual['count']}"
        )
    if actual["refs"] != expected["refs"]:
        raise AssertionError(
            f"{symbol}: expected refs {expected['refs']}, got {actual['refs']}"
        )
    return {"ram_map": str(RAM_MAP.relative_to(REPO)), "symbol": actual}


def verify_saved_mode_f64_boundary() -> dict[str, object]:
    """Check `ms[0xF64]` as saved-mode restore state, not range state.

    Older notes mapped the same absolute byte as display draw height because
    the RAM map only exposes two display-menu refs. Stock init/restore evidence
    is stronger for the DMM goal: config word 12 writes `ms[0xF64]`, and boot
    copies its low byte into live `ms[0xF68]`.
    """
    expected = EXPECTED_SAVED_MODE_F64_RAM_MAP_REF
    symbol = expected["symbol"]
    lines = RAM_MAP.read_text(encoding="utf-8", errors="replace").splitlines()

    for line in lines:
        if f" {symbol} " not in line:
            continue
        before_refs, refs_text = line.split(": ", 1)
        parts = before_refs.split()
        actual = {
            "addr": parts[0],
            "count": int(parts[2].strip("()")),
            "refs": [item.strip() for item in refs_text.split(",")],
            "line": line,
            "classification": expected["classification"],
        }
        break
    else:
        raise AssertionError(f"{RAM_MAP}: missing {symbol} entry")

    if actual["addr"] != expected["addr"]:
        raise AssertionError(
            f"{symbol}: expected addr {expected['addr']}, got {actual['addr']}"
        )
    if actual["count"] != expected["count"]:
        raise AssertionError(
            f"{symbol}: expected count {expected['count']}, got {actual['count']}"
        )
    if actual["refs"] != expected["refs"]:
        raise AssertionError(
            f"{symbol}: expected refs {expected['refs']}, got {actual['refs']}"
        )

    sequences: dict[str, dict[str, object]] = {}
    for name, (addr, expected_bytes) in EXPECTED_SAVED_MODE_F64_SEQUENCES.items():
        actual_bytes = read(addr, len(expected_bytes))
        if actual_bytes != expected_bytes:
            raise AssertionError(
                f"{name} @ {addr:#010x}: expected {expected_bytes.hex(' ')}, "
                f"got {actual_bytes.hex(' ')}"
            )
        sequences[name] = {
            "addr": f"0x{addr:08x}",
            "bytes": actual_bytes.hex(" "),
        }
    return {
        "ram_map": str(RAM_MAP.relative_to(REPO)),
        "symbol": actual,
        "sequences": sequences,
    }


def _parse_full_decompile_symbol_refs() -> dict[str, list[tuple[int, str]]]:
    lines = FULL_DECOMPILE.read_text(encoding="utf-8", errors="replace").splitlines()
    parsed: dict[str, list[tuple[int, str]]] = {}
    for symbol in EXPECTED_MUX_STATE_FULL_DECOMPILE_REFS:
        parsed[symbol] = [
            (line_no, line.strip())
            for line_no, line in enumerate(lines, 1)
            if symbol in line
        ]
    return parsed


def verify_mux_state_full_decompile_surface() -> dict[str, object]:
    """Check the complete full-decompile surface for `DAT_200000fa/fb`.

    The RAM map gives a function-level boundary. This text-level guard pins
    the exact visible stock decompile lines so a future DMM pass cannot cite a
    newly visible `ms[0x02]`/`ms[0x03]` reference as runtime meter evidence
    without classifying it. The indexed writes through `&DAT_200000fa` are
    shared-pair writes: index 0 targets `DAT_200000fa`, and index 1 can target
    `DAT_200000fb` before applying `FUN_08001a58(DAT_200000fb)`.
    """
    parsed = _parse_full_decompile_symbol_refs()
    checked: dict[str, dict[str, object]] = {}
    pair_writes: list[dict[str, object]] = []
    for symbol, expected_refs in EXPECTED_MUX_STATE_FULL_DECOMPILE_REFS.items():
        actual_refs = parsed[symbol]
        if actual_refs != expected_refs:
            raise AssertionError(
                f"{symbol} full-decompile refs drifted: expected "
                f"{expected_refs}, got {actual_refs}"
            )

        checked[symbol] = {
            "count": len(actual_refs),
            "refs": [
                {"line": line_no, "text": line}
                for line_no, line in actual_refs
            ],
            "literal_direct_assignments": [
                {"line": line_no, "text": line}
                for line_no, line in actual_refs
                if line.startswith(f"{symbol} =")
            ],
        }

    actual_pair_writes = [
        (line_no, line)
        for line_no, line in parsed["DAT_200000fa"]
        if line.startswith("(&DAT_200000fa)[")
    ]
    expected_pair_write_lines = sorted(EXPECTED_MUX_STATE_FULL_DECOMPILE_PAIR_WRITES)
    actual_pair_write_lines = [line_no for line_no, _line in actual_pair_writes]
    if actual_pair_write_lines != expected_pair_write_lines:
        raise AssertionError(
            "mux-state full-decompile pair writes drifted: expected "
            f"{expected_pair_write_lines}, got {actual_pair_write_lines}"
        )
    for line_no, line in actual_pair_writes:
        expected = EXPECTED_MUX_STATE_FULL_DECOMPILE_PAIR_WRITES[line_no]
        if line != expected["text"]:
            raise AssertionError(
                f"mux-state pair write text drifted at {line_no}: expected "
                f"{expected['text']!r}, got {line!r}"
            )
        pair_writes.append(
            {
                "line": line_no,
                "text": line,
                "target": expected["target"],
                "classification": expected["classification"],
            }
        )

    return {
        "full_decompile": str(FULL_DECOMPILE.relative_to(REPO)),
        "symbols": checked,
        "pair_writes": pair_writes,
    }


def verify_mux_state_pair_write_contexts() -> dict[str, object]:
    """Pin the branch/call context that classifies aliased mux-pair writes.

    A single `(&DAT_200000fa)[idx]` assignment is not enough evidence by
    itself: index 1 can target `DAT_200000fb`. These blocks prove the same
    branches immediately apply the matching GPIO mux writer and queue command
    `4`, keeping the writes classified as scope/siggen autorange paths rather
    than recovered DMM range switching.
    """
    lines = FULL_DECOMPILE.read_text(encoding="utf-8", errors="replace").splitlines()
    checked: dict[str, list[dict[str, object]]] = {}
    for name, expected_lines in EXPECTED_MUX_STATE_PAIR_WRITE_CONTEXTS.items():
        actual_lines = [
            (line_no, lines[line_no - 1].strip())
            for line_no, _text in expected_lines
        ]
        if actual_lines != expected_lines:
            raise AssertionError(
                f"{name} mux-state pair-write context drifted: expected "
                f"{expected_lines}, got {actual_lines}"
            )
        checked[name] = [
            {"line": line_no, "text": text}
            for line_no, text in actual_lines
        ]
    return {"full_decompile": str(FULL_DECOMPILE.relative_to(REPO)), "contexts": checked}


def verify_scope_measurement_engine_mux_pointer_consumer_context() -> dict[str, object]:
    """Check the `&DAT_200000fa + idx` pointer alias is read-only scope math.

    The full-decompile symbol ref at line 11411 lives inside `FUN_0801f6f8`
    (`scope_measurement_engine`) and is easy to under-classify: later
    `*pbVar19` uses no longer mention `DAT_200000fa`.  This guard pins the
    local text surface and fails if the inspected scope measurement block grows
    a write through that alias.
    """
    lines = FULL_DECOMPILE.read_text(encoding="utf-8", errors="replace").splitlines()
    checked: dict[str, dict[str, object]] = {}
    for name, expected in EXPECTED_SCOPE_MEASUREMENT_ENGINE_MUX_POINTER_CONSUMER_CONTEXT.items():
        line_start, line_end = expected["line_range"]
        required_lines = expected["required_lines"]
        actual_lines = [
            (line_no, lines[line_no - 1].strip())
            for line_no, _text in required_lines
        ]
        if actual_lines != required_lines:
            raise AssertionError(
                f"{name} required lines drifted: expected "
                f"{required_lines}, got {actual_lines}"
            )
        block = [
            {"line": line_no, "text": lines[line_no - 1].strip()}
            for line_no in range(line_start, line_end + 1)
        ]
        forbidden_hits = [
            item for item in block
            if any(pattern in item["text"] for pattern in expected["forbidden_substrings"])
        ]
        if forbidden_hits:
            raise AssertionError(
                f"{name} has forbidden pointer-write forms: {forbidden_hits}"
            )
        checked[name] = {
            "line_range": [line_start, line_end],
            "required_lines": [
                {"line": line_no, "text": text}
                for line_no, text in actual_lines
            ],
            "classification": expected["classification"],
        }
    return {"full_decompile": str(FULL_DECOMPILE.relative_to(REPO)), "contexts": checked}


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


def verify_meter_selector_adjust_sequences() -> dict[str, object]:
    """Check stock prev/next selector stepping around the raw-word table.

    The guarded `0x080041F8`/`0x080047CC` handlers are the paired stock adjust
    owners for the selector state while `ms[0xF68] == 1`: they gate on
    `bRam20001055 & 0xF0 != 0xB0`, decrement/increment `DAT_20001025` with
    wrap over 0..7, load `0x080BB3FC + DAT_20001025`, stage
    `0x0500 | table[selector]` at `0x20002D54`, enqueue it through
    `0x20002D74`, then queue display commands `0x1D` and `0x1B` and reset the
    visible value state before tail-calling `FUN_080028E0`.

    This is digital selector stepping and raw-word emission evidence.  It is
    not the missing analog `ms[0x02]`/`ms[0x03]` runtime range writer and does
    not explain the low-DCV physical mismatch.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SELECTOR_ADJUST_SEQUENCES.items():
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


def verify_dynamic_raw_word_helper_sequences() -> dict[str, object]:
    """Check stock helper slices around dynamic `0x0500 | low_byte` words.

    This cluster is adjacent to the runtime mode-init helpers but has a
    different job.  The guarded `0x08006060` slice seeds `DAT_20001025` and
    `DAT_2000102E` pairs before emitting fixed raw word `0x0501` through
    `0x20002D74` and display/update commands `0x1D`/`0x1B`.  The guarded
    `0x08006120` slice only emits a dynamic raw word when `ms[0xF68] == 1`,
    the `0xB0` blocker is clear, and the selector matches mask `0xC6`; it then
    chooses one of the low-byte pairs `0x0C/0x0D`, `0x0E/0x17`,
    `0x11/0x16`, or `0x10/0x15`, writes NaN display sentinels, calls
    `FUN_080028E0`, ORs in `0x0500`, queues the halfword through
    `0x20002D74`, and queues display/update byte `0x1B`.

    The guarded `0x080062F8` partner shares the same outer `ms[0xF68]` gate
    but clears or updates state and emits display/update bytes instead of the
    dynamic `0x05xx` word.  These are digital command/state-machine anchors,
    not analog `ms[0x02]`/`ms[0x03]` range writers or calibration proof.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_DYNAMIC_RAW_WORD_HELPER_SEQUENCES.items():
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
    return {
        "sequences": checked,
        "apply_lowbyte_pairs": EXPECTED_DYNAMIC_RAW_WORD_APPLY_LOWBYTE_PAIRS,
    }


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


def verify_meter_basic_raw_word_sequences() -> dict[str, object]:
    """Check stock direct raw-word materializers for basic meter bring-up.

    These sites enqueue `0x0508`, `0x0509`, and `0x0514` through the same
    `0x20002D74` raw-word path as the selector/apply helpers.  They ground the
    local wake/start/probe tail as stock command sequencing.  They are not
    analog `ms[0x02]`/`ms[0x03]` range writers, low-DCV correction words, or
    factory calibration coefficients.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, info in EXPECTED_METER_BASIC_RAW_WORD_SEQUENCES.items():
        addr: int = info["addr"]  # type: ignore[assignment]
        expected: bytes = info["bytes"]  # type: ignore[assignment]
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "word": str(info["word"]),
            "bytes": actual.hex(" "),
            "classification": str(info["classification"]),
        }
    return {"sequences": checked}


def verify_roll_buffer_preload_sequences() -> dict[str, object]:
    """Check init-only 301-byte roll-buffer preload evidence.

    Stock master init calls `FUN_08001830` twice at `0x080271A8..0x080271DC`:
    once with `state + 0x356`, once with `state + 0x483`, both with count
    `0x12D` and `state[4/5] ^ 0x80`.  Later RAM-map work identifies those
    destinations as oscilloscope roll-buffer regions, not a DMM factory
    calibration source.  Guarding this sequence keeps the old 301-byte cal
    myth from becoming a production meter coefficient again.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_ROLL_BUFFER_PRELOAD_SEQUENCES.items():
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

    The guarded boot restore prelude reads saved byte `ms[0xF64]`, copies it to
    live mode-init state `ms[0xF68]` when nonzero, and branches state 1/2/3 into
    the restore-time transport paths.  The following enable tail enables USART2
    and resumes the two DVOM tasks before resetting meter display/selector
    state.  Its paired disable path clears USART2 enable, suspends the same
    task handles, clears PC11, resets the meter semaphore, and drains the raw
    TX-word queue at `0x20002D74`.

    Guard these as stock restore/transport sequencing evidence; the exact
    local settle/discard constants remain OpenScope policy, and this is not an
    analog range writer or calibration source.
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
    8 or mode 1, and later RX branches update the `DAT_2000102E` variant
    shadow plus `DAT_2000102F`/`DAT_20001027` before the display formatter
    consumes them.  The current formatter reads that variant shadow for its
    mA/A display choice, but these guarded byte sequences are not a recovered
    current range writer.

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


def verify_display_formatter_dispatch_sequences() -> dict[str, object]:
    """Check stock display formatter mode-family dispatch evidence.

    `FUN_080028E0` writes display-only state bytes after the meter parser has
    classified a frame: `DAT_20001026` is the unit index and `DAT_20001030` is
    the format template offset.  The guarded mode switch pins the stock offsets
    for DCA/ACA/resistance/mode 5/modes 6-7, including mode 5 unit index 7 with
    `DAT_2000102f + 9` and modes 6/7 unit indices 8/9/10/11 with
    `DAT_2000102f + 10`.  This is formatter evidence only; it does not prove a
    separate capacitance-vs-temperature selector or any runtime analog range
    writer.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_DISPLAY_FORMATTER_DISPATCH_SEQUENCES.items():
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


def verify_current_formatter_variant_sequences() -> dict[str, object]:
    """Check stock current formatter use of the digital variant shadow.

    `FUN_080028E0` case 2 reads `DAT_2000102E` and uses it only to choose the
    DCA display unit index: variant 1 branches to unit index 4 with the raw
    `DAT_2000102F` format state, while variant 2 falls through to unit index 3
    and the `DAT_2000102F + 2` format state shared with the ACA formatter case.
    Guard this explicitly so a future DMM pass cannot reinterpret the variant
    shadow as a recovered physical current range writer or factory coefficient.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_CURRENT_FORMATTER_VARIANT_SEQUENCES.items():
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


def verify_unit_lookup_boundary_sequences() -> dict[str, object]:
    """Check the stock unit lookup negative boundary.

    Stock draw code at `0x08009AE4` computes
    `0x0804C40C + DAT_20001058 * 0x30 + DAT_20001026 * 4` and loads one word
    for the unit-render call.  Older notes treated the base as a recovered
    12-entry unit-string pointer table, but the downloaded V1.2.0 image has a
    zero-filled first 48 bytes there.  Those words are not valid in-image
    Thumb/text pointers, so they are negative evidence: stock
    formatter unit indices are real, but unit string contents are not recovered
    from this APP image.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_UNIT_LOOKUP_BOUNDARY_SEQUENCES.items():
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

    words = struct.unpack(
        "<12I",
        read(EXPECTED_UNIT_LOOKUP_BOUNDARY_SEQUENCES["display_unit_lookup_zero_region"][0], 48),
    )
    thumb_pointers = [
        value for value in words
        if (value & 1) and BASE <= (value & ~1) < BASE + BIN.stat().st_size
    ]
    if thumb_pointers:
        raise AssertionError(
            "0x0804C40C unexpectedly contains in-image Thumb pointers: "
            + ", ".join(f"{value:#010x}" for value in thumb_pointers)
        )
    checked["display_unit_lookup_zero_region"]["words"] = " ".join(
        f"{value:#010x}" for value in words
    )
    return {"sequences": checked, "thumb_pointers": thumb_pointers}


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


def verify_meter_aux_afe_pin_sequences() -> dict[str, object]:
    """Check stock PB9/PA6 auxiliary AFE pin evidence.

    Stock master init configures PB9 and PA6 as GPIO outputs near
    `0x080241D4..0x080241F0`, but the current stock decompile does not expose
    a direct BOP/BCR level write for PB9 (`GPIOB` bit 9) or PA6 (`GPIOA` bit
    6).  Guard that boundary so the open firmware does not treat an invented
    high level as recovered DMM frontend truth.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_AUX_AFE_PIN_INIT_SEQUENCES.items():
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

    decompile = FULL_DECOMPILE.read_text(encoding="utf-8", errors="replace")
    hits = [
        needle for needle in FORBIDDEN_METER_AUX_AFE_DIRECT_LEVEL_WRITES
        if needle in decompile
    ]
    if hits:
        raise AssertionError(
            "stock PB9/PA6 level writes are now visible and need classification: "
            + ", ".join(hits)
        )
    return {
        "sequences": checked,
        "forbidden_direct_level_writes": FORBIDDEN_METER_AUX_AFE_DIRECT_LEVEL_WRITES,
    }


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


def verify_meter_saved_config_live_mux_store_sequences() -> dict[str, object]:
    """Check the direct live mux-state stores in stock saved-config unpack.

    The slice materializes base `0x200000f8`, validates the saved-config
    signature, then stores byte 2 of word 0 to `ms[0x02]` and word 1 to
    `ms[0x03..0x06]`.  This is a narrow binary guard for the known boot/restore
    live stores.  It is deliberately not a claim that stock exposes a local
    runtime DMM range writer at these addresses.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SAVED_CONFIG_LIVE_MUX_STORE_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        checked[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
            "classification": (
                "saved-config boot/restore direct live mux stores; "
                "not a runtime DMM range writer"
            ),
        }
    return {"sequences": checked}


def verify_meter_saved_config_pack_sequences() -> dict[str, object]:
    """Check stock saved-config pack/default path for `ms[0x02]`/`ms[0x03]`.

    `FUN_080223BC` is the paired persistence packer for the unpack path at
    `0x08025D92`: when called with signature `0x55`, it reads the current
    `ms[0x00]`, `ms[0x01]`, `ms[0x02]` bytes and later stores
    `ms[0x03..0x06]` as word 1 in the persistent config buffer.  Its invalid
    config/default branch seeds word 0 with `0x05050000`, meaning default
    `ms[0x02] = 5` and `ms[0x03] = 5` before later fields are populated.

    This is persistence evidence only.  It proves what stock saves/restores for
    the mux-state bytes, not a runtime DMM range writer.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SAVED_CONFIG_PACK_SEQUENCES.items():
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


def verify_meter_saved_config_pack_caller_sequences() -> dict[str, object]:
    """Check and classify raw stock direct-BL hits to the config packer.

    The direct sweep intentionally covers all BL-shaped stock bytes, including
    literal/branch-island false positives.  That keeps the evidence honest:
    `0x0803972E` is a controlled shutdown/config-save call, `0x08002F8C`
    appears in a housekeeping threshold path, and the other two hits are not
    classified as normal executable DMM range writers.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_METER_SAVED_CONFIG_PACK_CALLER_SEQUENCES.items():
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
    direct_calls = find_direct_thumb_bl_callers(0x080223BC)
    if direct_calls != EXPECTED_METER_SAVED_CONFIG_PACK_DIRECT_CALLS:
        raise AssertionError(
            "FUN_080223BC direct BL-shaped hits drifted: expected "
            f"{[f'{addr:#010x}' for addr in EXPECTED_METER_SAVED_CONFIG_PACK_DIRECT_CALLS]}, "
            f"got {[f'{addr:#010x}' for addr in direct_calls]}"
        )
    direct_callers = [f"{addr:#010x}" for addr in direct_calls]
    if sorted(direct_callers) != sorted(EXPECTED_METER_SAVED_CONFIG_PACK_CALLER_CLASSES):
        raise AssertionError(
            "saved-config pack caller classifications no longer cover direct hits"
        )
    return {
        "sequences": checked,
        "direct_callers": direct_callers,
        "classifications": dict(EXPECTED_METER_SAVED_CONFIG_PACK_CALLER_CLASSES),
    }


def verify_usart_tx_config_writer_meter_case_sequences() -> dict[str, object]:
    """Check the stock `FUN_08039734` TBB writer and its meter-case-shaped arm.

    The guarded `cmd_type == 4` arm writes parameter bits into the config words
    at `[r0+0x20]` and `[r0+0x1C]`, then ORs update mask `0x0100` through the
    shared epilogue.  Visible direct callers in master init pass TIM5/TIM2 base
    addresses, so this is evidence for a separate FPGA/timer-style config
    bitfield writer shape, not proof of the normal runtime DMM selector/range
    source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_USART_TX_CONFIG_WRITER_SEQUENCES.items():
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

    callers: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_USART_TX_CONFIG_WRITER_CALLER_SEQUENCES.items():
        actual = read(addr, len(expected))
        if actual != expected:
            raise AssertionError(
                f"{name} {addr:#010x}: expected {expected.hex(' ')}, "
                f"got {actual.hex(' ')}"
            )
        callers[name] = {
            "addr": f"{addr:#010x}",
            "bytes": actual.hex(" "),
        }

    direct_calls = find_direct_thumb_bl_callers(0x08039734)
    if direct_calls != EXPECTED_USART_TX_CONFIG_WRITER_DIRECT_CALLS:
        raise AssertionError(
            "FUN_08039734 direct BL callers drifted: expected "
            f"{[f'{addr:#010x}' for addr in EXPECTED_USART_TX_CONFIG_WRITER_DIRECT_CALLS]}, "
            f"got {[f'{addr:#010x}' for addr in direct_calls]}"
        )

    return {
        "sequences": checked,
        "callers": callers,
        "direct_callers": [f"{addr:#010x}" for addr in direct_calls],
    }


def verify_boot_mode_init_dmm_sequences() -> dict[str, object]:
    """Check stock `FUN_0800B908` boot-time DMM command queue sequences.

    This dispatcher reads `ms[0xF68]`, sends one-byte command codes to queue
    `0x20002D6C`, and is called by boot/runtime resume tails.  The guarded meter
    arms prove stock command ordering for basic, extended, and variant meter
    boot/resume setup.  They are not evidence for analog range writers,
    calibration coefficients, or the exact runtime source of `ms[0x02]` and
    `ms[0x03]`.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_BOOT_MODE_INIT_DMM_SEQUENCES.items():
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
    direct_calls = find_direct_thumb_bl_callers(0x0800B908)
    if direct_calls != EXPECTED_BOOT_MODE_INIT_DMM_DIRECT_CALLS:
        raise AssertionError(
            "FUN_0800B908 direct BL callers drifted: expected "
            f"{[f'{addr:#010x}' for addr in EXPECTED_BOOT_MODE_INIT_DMM_DIRECT_CALLS]}, "
            f"got {[f'{addr:#010x}' for addr in direct_calls]}"
        )
    return {
        "sequences": checked,
        "direct_callers": [f"{addr:#010x}" for addr in direct_calls],
    }


def _require_ordered_hex_snippets(name: str, haystack_hex: str, snippets: list[str]) -> None:
    pos = 0
    for snippet in snippets:
        found = haystack_hex.find(snippet, pos)
        if found < 0:
            raise AssertionError(f"{name}: missing ordered command snippet {snippet}")
        pos = found + len(snippet)


def verify_boot_mode_init_dmm_command_banks() -> dict[str, object]:
    """Check documented stock command-byte banks inside `FUN_0800B908` arms.

    The surrounding sequence guard already pins the exact stock bytes.  This
    extra check extracts the mode-init command banks that local DMM comments and
    state-machine policy refer to: basic `0x00/0x09/(0x07|0x0A)` probing,
    range tails `0x1A..0x1E` and `0x16..0x19`, and the variant
    `0x12/0x13/0x14` family.  These are one-byte commands queued to
    `0x20002D6C`, not raw `0x05xx` selector words and not analog mux writers.
    """
    checked: dict[str, dict[str, object]] = {}
    for name, expected in EXPECTED_BOOT_MODE_INIT_DMM_COMMAND_BANKS.items():
        addr, data = EXPECTED_BOOT_MODE_INIT_DMM_SEQUENCES[name]
        actual = read(addr, len(data))
        if actual != data:
            raise AssertionError(f"{name}: sequence bytes drifted before command-bank check")
        haystack_hex = actual.hex(" ")
        _require_ordered_hex_snippets(name, haystack_hex, expected["ordered_snippets"])
        checked[name] = {
            "addr": f"{addr:#010x}",
            "commands": list(expected["commands"]),
            "ordered_snippets": list(expected["ordered_snippets"]),
        }
    return {"banks": checked}


def verify_meter_probe_branch_sequences() -> dict[str, object]:
    """Check stock GPIOC bit-7 gated `0x07`/`0x0A` probe command branches.

    `FUN_0800B908` has three meter arms that load GPIOC IDR (`0x40011008`),
    shift bit 7 into the sign position, and conditionally replace command
    `0x07` with `0x0A` through an `IT PL` instruction.  This guards the local
    `fpga_probe_cmd_byte()` source and polarity as digital probe/tail sequencing.
    It is not DMM runtime range state, not a low-DCV correction path, and not a
    physical calibration source.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, expected in EXPECTED_METER_PROBE_BRANCH_GUARDS.items():
        addr, data = EXPECTED_BOOT_MODE_INIT_DMM_SEQUENCES[name]
        actual = read(addr, len(data))
        if actual != data:
            raise AssertionError(f"{name}: sequence bytes drifted before probe-branch check")
        haystack_hex = actual.hex(" ")
        _require_ordered_hex_snippets(
            name,
            haystack_hex,
            [expected["source_snippet"], expected["branch_snippet"]],
        )
        if addr != expected["addr"]:
            raise AssertionError(f"{name}: expected guard addr {expected['addr']:#010x}, got {addr:#010x}")
        checked[name] = {
            "addr": f"{addr:#010x}",
            "source_register": str(expected["source_register"]),
            "source_snippet": str(expected["source_snippet"]),
            "branch_snippet": str(expected["branch_snippet"]),
            "polarity": str(expected["polarity"]),
            "classification": str(expected["classification"]),
        }
    return {"branches": checked}


def verify_runtime_mode_init_dispatch_caller_sequences() -> dict[str, object]:
    """Check runtime helper slices that tail-call the mode-init dispatcher.

    The guarded `0x08006418`/`0x08006548` helper pair reads the current
    `ms[0xF68]` mode-init state, mutates `ms[0xF68]`, `ms[0xF69]`,
    `ms[0xF6B]`, and latch bytes, then tail-calls `FUN_0800B908`.  This proves
    runtime entry into the already guarded command-byte dispatcher.  It does
    not turn the dispatcher into an analog range writer and does not recover
    meter calibration.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_RUNTIME_MODE_INIT_DISPATCH_CALLER_SEQUENCES.items():
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
        direct_calls = find_direct_thumb_bl_callers(target)
        expected_calls = sorted(calls)
        if direct_calls != expected_calls:
            raise AssertionError(
                f"{name} direct BL callers drifted: expected "
                f"{[f'{addr:#010x}' for addr in expected_calls]}, "
                f"got {[f'{addr:#010x}' for addr in direct_calls]}"
            )
        checked[name] = {
            "target": f"{target:#010x}",
            "calls": sorted(call_bytes),
            "direct_callers": [f"{addr:#010x}" for addr in direct_calls],
            "sequences": call_bytes,
        }
    return checked


def verify_meter_mux_writer_literal_pointer_refs() -> dict[str, object]:
    """Check the APP image has no static literal pointers to the mux writers.

    Stock has real `ldr`/`blx` dispatch surfaces, so the direct-BL callsite guard
    is not the whole possible story.  This negative guard scans the whole APP
    image for 32-bit literal/function-pointer forms of `FUN_080018a4` and
    `FUN_08001a58`, including Thumb-bit addresses.  Finding none does not prove
    no computed runtime path exists, but it prevents future work from citing a
    hidden table entry without adding new binary evidence.
    """
    checked: dict[str, object] = {}
    for name, info in EXPECTED_MUX_WRITER_LITERAL_POINTER_REFS.items():
        forms: dict[str, int] = info["forms"]  # type: ignore[assignment]
        refs_by_form: dict[str, list[str]] = {}
        flat_refs: list[str] = []
        for form_name, value in forms.items():
            refs = find_literal_word_refs(value)
            refs_by_form[form_name] = [f"{addr:#010x}" for addr in refs]
            flat_refs.extend(refs_by_form[form_name])
        expected_refs = list(info["expected_refs"])  # type: ignore[arg-type]
        if sorted(flat_refs) != sorted(expected_refs):
            raise AssertionError(
                f"{name} literal pointer refs drifted: expected "
                f"{expected_refs}, got {sorted(flat_refs)}"
            )
        checked[name] = {
            "target": f"{int(info['target']):#010x}",
            "forms": {key: f"{value:#010x}" for key, value in forms.items()},
            "refs": refs_by_form,
            "classification": str(info["classification"]),
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


def verify_mux_writer_scope_tail_contexts() -> dict[str, object]:
    """Check mux-writer tails remain classified as scope threshold logic.

    The mux writers are DMM-relevant hardware writers, but their trailing
    table/DAC math reads adjacent scope state (`DAT_20000125`,
    `DAT_2000010c`, `DAT_200000fc/fd`) and updates scope threshold registers.
    Guard this context so those adjacent refs do not become a guessed DMM
    calibration/range correction for low DCV.
    """
    lines = FULL_DECOMPILE.read_text(
        encoding="utf-8", errors="replace"
    ).splitlines()
    checked: dict[str, object] = {}

    for name, info in EXPECTED_MUX_WRITER_SCOPE_TAIL_CONTEXTS.items():
        start, end = info["line_range"]
        snippets: list[str] = info["snippets"]
        segment = "\n".join(lines[start - 1:end])
        missing = [snippet for snippet in snippets if snippet not in segment]
        if missing:
            raise AssertionError(
                f"{name} full_decompile.c:{start}..{end} missing {missing}"
            )
        checked[name] = {
            "line_range": [start, end],
            "snippets": snippets,
            "classification": info["classification"],
        }

    return {"file": str(FULL_DECOMPILE.relative_to(REPO)), "contexts": checked}


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


def verify_scope_submode_mux_call_sequences() -> dict[str, object]:
    """Check scope-submode mux restore calls that are not DMM range writers.

    Two stock scope paths read `state[0x30]` / `DAT_20000128`, mask it with
    `0x0f`, then call both GPIO mux writers.  This proves those direct BL sites
    are scope post-calibration/runtime reconfiguration paths.  They do not read
    the eight-entry DMM selector table, do not emit DMM selector words, and do
    not recover a runtime DMM writer for `ms[0x02]`/`ms[0x03]`.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_SCOPE_SUBMODE_MUX_CALL_SEQUENCES.items():
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


def verify_watchdog_reload_state_boundary_sequences() -> dict[str, object]:
    """Check `DAT_2000105a` / `meter_state + 0xf62` is watchdog/UI state.

    The FPGA-task disassembly has an easy-to-misread `ldrb.w r0, [r7,#0xf62]`
    at `0x08039038`, with `r7 = 0x200000f8`.  That address is
    `DAT_2000105a`, not `DAT_200000fa`/`DAT_200000fb` and not meter
    `ms[0x02]`/`ms[0x03]`.  The guarded block writes the byte to `IWDG_RLR`
    (`0x40015434`) and clears `meter_state + 0xf6c`, then runs button-event
    dispatch/housekeeping checks.  It is negative DMM evidence, not a recovered
    analog range writer.
    """
    checked: dict[str, dict[str, object]] = {}
    for name, (addr, expected) in EXPECTED_WATCHDOG_RELOAD_STATE_SEQUENCES.items():
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

    ram_map_lines = RAM_MAP.read_text(encoding="utf-8", errors="replace").splitlines()
    expected_ram_map = (
        "0x2000105A DAT_2000105a (1 refs): FUN_08015f50@08015f50"
    )
    if expected_ram_map not in ram_map_lines:
        raise AssertionError(f"{RAM_MAP}: DAT_2000105a RAM-map entry drifted")

    lines = FULL_DECOMPILE.read_text(encoding="utf-8", errors="replace").splitlines()
    expected_full_ref = (4733, "uVar15 = FUN_0803e5da(DAT_2000105a);")
    actual_full_ref = (expected_full_ref[0], lines[expected_full_ref[0] - 1].strip())
    if actual_full_ref != expected_full_ref:
        raise AssertionError(
            "DAT_2000105a full-decompile consumer drifted: expected "
            f"{expected_full_ref}, got {actual_full_ref}"
        )

    return {
        "sequences": checked,
        "ram_map": expected_ram_map,
        "full_decompile_ref": {
            "line": expected_full_ref[0],
            "text": expected_full_ref[1],
        },
        "classification": (
            "watchdog/UI housekeeping boundary, not DMM ms[0x02]/ms[0x03]"
        ),
    }


def verify_scope_mux_state_consumer_sequences() -> dict[str, object]:
    """Check remaining RAM-map consumers of the shared mux-state pair.

    `ram_map.txt` lists `FUN_0801d2ec`, `FUN_0801efc0`, and `FUN_0801f6f8`
    as additional refs to `DAT_200000fa`/`DAT_200000fb`.  The guarded slices
    classify those refs as scope timebase, scope math, and scope measurement
    scale-table consumers.  They read mux bytes and scope offset bytes, then
    index `DAT_080465cc`/`DAT_0804bfb8`-style scale tables; they do not call
    the mux writers and are not DMM runtime range proof.
    """
    checked: dict[str, dict[str, str]] = {}
    for name, (addr, expected) in EXPECTED_SCOPE_MUX_STATE_CONSUMER_SEQUENCES.items():
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
    mux_state_ram_map = verify_mux_state_ram_map_boundary()
    mode_state_ram_map = verify_mode_state_ram_map_boundary()
    saved_mode_f64 = verify_saved_mode_f64_boundary()
    mux_state_full_decompile = verify_mux_state_full_decompile_surface()
    mux_state_pair_write_contexts = verify_mux_state_pair_write_contexts()
    scope_measurement_mux_pointer_context = (
        verify_scope_measurement_engine_mux_pointer_consumer_context()
    )
    selector = verify_meter_selector_table()
    selector_xrefs = verify_meter_selector_xref_sequences()
    selector_adjust = verify_meter_selector_adjust_sequences()
    dynamic_raw_word_helpers = verify_dynamic_raw_word_helper_sequences()
    dvom_tx_consumers = verify_dvom_tx_queue_consumer_sequences()
    meter_basic_raw_words = verify_meter_basic_raw_word_sequences()
    roll_buffer_preload = verify_roll_buffer_preload_sequences()
    transport_transitions = verify_meter_transport_transition_sequences()
    runtime_transport_transitions = verify_runtime_mode_switch_transport_sequences()
    selector_state = verify_meter_selector_state_sequences()
    acv_format = verify_acv_format_selector_sequences()
    display_formatter = verify_display_formatter_dispatch_sequences()
    current_formatter = verify_current_formatter_variant_sequences()
    unit_lookup_boundary = verify_unit_lookup_boundary_sequences()
    mux_restore = verify_meter_mux_restore_sequences()
    aux_afe_pins = verify_meter_aux_afe_pin_sequences()
    saved_config_unpack = verify_meter_saved_config_unpack_sequences()
    saved_config_live_mux_store = verify_meter_saved_config_live_mux_store_sequences()
    saved_config_pack = verify_meter_saved_config_pack_sequences()
    saved_config_pack_callers = verify_meter_saved_config_pack_caller_sequences()
    usart_tx_config_writer = verify_usart_tx_config_writer_meter_case_sequences()
    boot_mode_init = verify_boot_mode_init_dmm_sequences()
    boot_mode_init_banks = verify_boot_mode_init_dmm_command_banks()
    meter_probe_branches = verify_meter_probe_branch_sequences()
    runtime_mode_init_callers = verify_runtime_mode_init_dispatch_caller_sequences()
    mux_calls = verify_meter_mux_callsite_sequences()
    mux_literal_refs = verify_meter_mux_writer_literal_pointer_refs()
    mux_bodies = verify_meter_mux_writer_body_sequences()
    mux_scope_tails = verify_mux_writer_scope_tail_contexts()
    runtime_mux_writers = verify_runtime_mux_state_writer_sequences()
    scope_submode_mux_calls = verify_scope_submode_mux_call_sequences()
    scope_snapshots = verify_scope_snapshot_consumer_sequences()
    scope_preset_mux_owners = verify_scope_preset_mux_owner_sequences()
    scope_ui_mux_lut_consumers = verify_scope_ui_mux_lut_consumer_sequences()
    watchdog_reload_boundary = verify_watchdog_reload_state_boundary_sequences()
    scope_mux_state_consumers = verify_scope_mux_state_consumer_sequences()
    for symbol, info in mux_state_ram_map["symbols"].items():
        print(
            f"stock mux-state RAM-map boundary {symbol}: "
            + ", ".join(info["refs"])
        )
    print(
        "stock mode-state RAM-map boundary "
        f"{mode_state_ram_map['symbol']['addr']}: "
        + ", ".join(mode_state_ram_map["symbol"]["refs"])
    )
    print(
        "stock saved-mode F64 boundary "
        f"{saved_mode_f64['symbol']['addr']}: "
        + ", ".join(item["addr"] for item in saved_mode_f64["sequences"].values())
    )
    pair_write_count = len(mux_state_full_decompile["pair_writes"])
    for symbol, info in mux_state_full_decompile["symbols"].items():
        print(
            f"stock mux-state full-decompile surface {symbol}: "
            f"{info['count']} refs, "
            f"{len(info['literal_direct_assignments'])} literal direct assignments"
        )
    print(f"stock mux-state full-decompile pair writes: {pair_write_count}")
    print(
        "stock mux-state pair-write contexts: "
        + ", ".join(mux_state_pair_write_contexts["contexts"].keys())
    )
    print(
        "stock scope measurement-engine mux-pointer consumer contexts: "
        + ", ".join(scope_measurement_mux_pointer_context["contexts"].keys())
    )
    print(f"stock meter selector table: {selector['bytes']}")
    print("stock meter selector xref sites: " +
          ", ".join(selector_xrefs["sequences"].keys()))
    print("stock meter selector adjust sites: " +
          ", ".join(item["addr"] for item in selector_adjust["sequences"].values()))
    print("stock dynamic raw-word helper sites: " +
          ", ".join(item["addr"] for item in dynamic_raw_word_helpers["sequences"].values()))
    print("stock dvom_TX raw-word consumer sites: " +
          ", ".join(item["addr"] for item in dvom_tx_consumers["sequences"].values()))
    print("stock meter basic raw-word sites: " +
          ", ".join(item["addr"] for item in meter_basic_raw_words["sequences"].values()))
    print("stock roll-buffer preload sites: " +
          ", ".join(item["addr"] for item in roll_buffer_preload["sequences"].values()))
    print("stock meter transport transition sites: " +
          ", ".join(item["addr"] for item in transport_transitions["sequences"].values()))
    print("stock runtime mode-switch transport sites: " +
          ", ".join(item["addr"] for item in runtime_transport_transitions["sequences"].values()))
    print("stock meter selector state sites: " +
          ", ".join(item["addr"] for item in selector_state["sequences"].values()))
    print("stock ACV format selector sites: " +
          ", ".join(item["addr"] for item in acv_format["sequences"].values()))
    print("stock display formatter dispatch sites: " +
          ", ".join(item["addr"] for item in display_formatter["sequences"].values()))
    print("stock current formatter variant sites: " +
          ", ".join(item["addr"] for item in current_formatter["sequences"].values()))
    print("stock unit lookup boundary sites: " +
          ", ".join(item["addr"] for item in unit_lookup_boundary["sequences"].values()))
    print("stock meter mux restore sites: " +
          ", ".join(mux_restore["sequences"].keys()))
    print("stock meter auxiliary AFE pin config sites: " +
          ", ".join(item["addr"] for item in aux_afe_pins["sequences"].values()))
    print("stock meter saved-config unpack sites: " +
          ", ".join(item["addr"] for item in saved_config_unpack["sequences"].values()))
    print("stock meter saved-config live mux-store sites: " +
          ", ".join(item["addr"] for item in saved_config_live_mux_store["sequences"].values()))
    print("stock meter saved-config pack sites: " +
          ", ".join(item["addr"] for item in saved_config_pack["sequences"].values()))
    print("stock meter saved-config pack caller sites: " +
          ", ".join(item["addr"] for item in saved_config_pack_callers["sequences"].values()))
    print("stock meter saved-config pack direct BL-shaped hits: " +
          ", ".join(saved_config_pack_callers["direct_callers"]))
    print("stock USART TX config writer meter-case sites: " +
          ", ".join(item["addr"] for item in usart_tx_config_writer["sequences"].values()))
    print("stock USART TX config writer visible callers: " +
          ", ".join(item["addr"] for item in usart_tx_config_writer["callers"].values()))
    print("stock boot mode-init DMM sequence sites: " +
          ", ".join(item["addr"] for item in boot_mode_init["sequences"].values()))
    print("stock boot mode-init DMM command banks: " +
          "; ".join(
              f"{name}=" + ",".join(item["commands"])
              for name, item in boot_mode_init_banks["banks"].items()
          ))
    print("stock meter probe branch guards: " +
          "; ".join(
              f"{name}@{item['addr']}={item['source_register']}->{item['polarity']}"
              for name, item in meter_probe_branches["branches"].items()
          ))
    print("stock runtime mode-init dispatcher caller sites: " +
          ", ".join(item["addr"] for item in runtime_mode_init_callers["sequences"].values()))
    for name, info in mux_calls.items():
        print(f"stock {name} direct BL sites: " + ", ".join(info["calls"]))
    for name, info in mux_literal_refs.items():
        refs = [
            ref for refs_for_form in info["refs"].values()
            for ref in refs_for_form
        ]
        print(f"stock {name} literal/function-pointer refs: " +
              (", ".join(refs) if refs else "none"))
    for name, info in mux_bodies.items():
        print(f"stock {name} body slices: " +
              ", ".join(info["slices"].keys()))
    print("stock mux-writer scope tail contexts: " +
          ", ".join(mux_scope_tails["contexts"].keys()))
    print("stock runtime mux-state writer sites: " +
          ", ".join(item["addr"] for item in runtime_mux_writers["sequences"].values()))
    print("stock scope-submode mux call sites: " +
          ", ".join(item["addr"] for item in scope_submode_mux_calls["sequences"].values()))
    print("stock scope snapshot consumer sites: " +
          ", ".join(item["addr"] for item in scope_snapshots["sequences"].values()))
    print("stock scope/preset mux owner sites: " +
          ", ".join(item["addr"] for item in scope_preset_mux_owners["sequences"].values()))
    print("stock scope UI mux-LUT consumer sites: " +
          ", ".join(item["addr"] for item in scope_ui_mux_lut_consumers["sequences"].values()))
    print("stock watchdog reload state boundary sites: " +
          ", ".join(item["addr"] for item in watchdog_reload_boundary["sequences"].values()))
    print("stock scope mux-state consumer sites: " +
          ", ".join(item["addr"] for item in scope_mux_state_consumers["sequences"].values()))
    print("stock meter literal pools: ok")


if __name__ == "__main__":
    main()
