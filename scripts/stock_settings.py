#!/usr/bin/env python3
"""Inspect and offline-patch the stock 2C53T saved-settings blob.

The stock firmware restores a small internal-flash settings structure from
0x08006000 during boot.  This tool models only fields with current reverse-
engineering evidence.  It never writes to the device; patch commands write a
new dump file that can be reviewed before any future flash operation.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


STOCK_SAVED_CONFIG_ADDRESS = 0x08006000
STOCK_SAVED_CONFIG_SIZE = 0x130

OFFSET_SIGNATURE = 0x00
OFFSET_METER_MODE = 0x03
OFFSET_METER_RANGE = 0x04
OFFSET_TIMEBASE_RANGE = 0x0A
OFFSET_TIMEBASE_MODE = 0x0B
OFFSET_COUPLING = 0x0D
OFFSET_TRIGGER_MODE = 0x0F
OFFSET_SAVED_OPERATIONAL_MODE = 0x17
OFFSET_MODE_DATA = 0x2C
OFFSET_SAVED_MODE = 0x30

SIGNATURE_NORMAL_RESTORE = 0x55
SIGNATURE_METER_RESTORE = 0xAA

RECOVERED_FIELDS = [
    {
        "name": "signature",
        "offset": OFFSET_SIGNATURE,
        "size": 1,
        "confidence": "high",
        "description": "0x55 normal restore, 0xAA meter restore, other invalid/defaults",
    },
    {
        "name": "meter_mode",
        "offset": OFFSET_METER_MODE,
        "size": 1,
        "confidence": "medium",
        "description": "restored to ms[0x02], then passed to stock meter-mode select",
    },
    {
        "name": "meter_range_word",
        "offset": OFFSET_METER_RANGE,
        "size": 4,
        "confidence": "medium",
        "description": "restored to ms[0x03..0x06]",
    },
    {
        "name": "timebase_range",
        "offset": OFFSET_TIMEBASE_RANGE,
        "size": 1,
        "confidence": "medium",
        "description": "restored to ms[0x16]",
    },
    {
        "name": "timebase_mode",
        "offset": OFFSET_TIMEBASE_MODE,
        "size": 1,
        "confidence": "medium",
        "description": "restored to ms[0x17]",
    },
    {
        "name": "coupling",
        "offset": OFFSET_COUPLING,
        "size": 1,
        "confidence": "medium",
        "description": "restored to ms[0x2D]",
    },
    {
        "name": "trigger_mode",
        "offset": OFFSET_TRIGGER_MODE,
        "size": 1,
        "confidence": "medium",
        "description": "restored to ms[0x35]",
    },
    {
        "name": "saved_operational_mode",
        "offset": OFFSET_SAVED_OPERATIONAL_MODE,
        "size": 1,
        "confidence": "partial",
        "description": "restored to ms[0xF39]; older notes disagree on exact runtime meaning",
    },
    {
        "name": "mode_data_word",
        "offset": OFFSET_MODE_DATA,
        "size": 4,
        "confidence": "partial",
        "description": "restored to ms[0xF60..0xF63]",
    },
    {
        "name": "saved_mode_word",
        "offset": OFFSET_SAVED_MODE,
        "size": 2,
        "confidence": "partial",
        "description": "restored to ms[0xF64..0xF65]; nonzero low byte enables stock meter USART boot tail",
    },
]


def recovered_fields() -> list[dict[str, object]]:
    return [dict(field) for field in RECOVERED_FIELDS]


def fields_overlapping(start: int, end: int) -> list[dict[str, object]]:
    matches = []
    for field in RECOVERED_FIELDS:
        field_start = int(field["offset"])
        field_end = field_start + int(field["size"])
        if start < field_end and end > field_start:
            matches.append(dict(field))
    return matches


def _require_size(data: bytes, size: int = STOCK_SAVED_CONFIG_SIZE) -> None:
    if len(data) < size:
        raise ValueError(f"stock settings dump is too small: {len(data)} bytes, need at least {size}")


def _u8(data: bytes, offset: int) -> int:
    return data[offset]


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def signature_meaning(signature: int) -> str:
    if signature == SIGNATURE_NORMAL_RESTORE:
        return "valid-normal-restore"
    if signature == SIGNATURE_METER_RESTORE:
        return "valid-meter-restore"
    if signature == 0xFF:
        return "erased"
    if signature == 0x00:
        return "blank-or-invalid"
    return "invalid-or-unknown"


def infer_startup_mode(signature: int, saved_mode: int) -> dict[str, object]:
    reasons: list[str] = []
    confidence = "partial"

    if signature == SIGNATURE_METER_RESTORE:
        reasons.append("signature 0xAA makes stock set ms[0xF68]=8 before restoring settings")
    if saved_mode & 0x00FF:
        reasons.append("saved-mode low byte is nonzero; stock boot tail enables the meter USART path")

    if reasons:
        mode = "multimeter"
    elif signature == SIGNATURE_NORMAL_RESTORE and saved_mode == 0:
        mode = "scope-or-startup-default"
        reasons.append("signature 0x55 with saved-mode 0 takes the non-meter boot tail")
    else:
        mode = "unknown"
        confidence = "unknown"
        reasons.append("settings signature is not a recovered valid stock signature")

    return {
        "mode": mode,
        "confidence": confidence,
        "reasons": reasons,
    }


def parse_stock_settings(data: bytes, *, base_address: int = STOCK_SAVED_CONFIG_ADDRESS) -> dict[str, object]:
    _require_size(data)
    signature = _u8(data, OFFSET_SIGNATURE)
    saved_mode = _u16(data, OFFSET_SAVED_MODE)

    return {
        "base_address": base_address,
        "parsed_size": STOCK_SAVED_CONFIG_SIZE,
        "dump_size": len(data),
        "blank": all(byte == 0xFF for byte in data[:STOCK_SAVED_CONFIG_SIZE]),
        "signature": signature,
        "signature_meaning": signature_meaning(signature),
        "valid_signature": signature in (SIGNATURE_NORMAL_RESTORE, SIGNATURE_METER_RESTORE),
        "fields": {
            "meter_mode": _u8(data, OFFSET_METER_MODE),
            "meter_range_word": _u32(data, OFFSET_METER_RANGE),
            "timebase_range": _u8(data, OFFSET_TIMEBASE_RANGE),
            "timebase_mode": _u8(data, OFFSET_TIMEBASE_MODE),
            "coupling": _u8(data, OFFSET_COUPLING),
            "trigger_mode": _u8(data, OFFSET_TRIGGER_MODE),
            "saved_operational_mode": _u8(data, OFFSET_SAVED_OPERATIONAL_MODE),
            "mode_data_word": _u32(data, OFFSET_MODE_DATA),
            "saved_mode_word": saved_mode,
        },
        "startup_mode": infer_startup_mode(signature, saved_mode),
        "language": {
            "value": None,
            "confidence": "unknown",
            "reason": "No stock language field has been recovered from the saved-config blob yet.",
        },
        "evidence": [
            "reverse_engineering/analysis_v120/master_init_phase4.c maps the 0x08006000 restore layout",
            "reverse_engineering/analysis_v120/master_init_phase2.c maps the nonzero saved-mode meter boot tail",
            "reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md records the 0xF64 restore conflict and best current interpretation",
        ],
    }


def patch_startup_multimeter(data: bytes) -> tuple[bytes, list[dict[str, int]]]:
    _require_size(data)
    summary = parse_stock_settings(data)
    if not summary["valid_signature"]:
        raise ValueError(
            "refusing to patch invalid/erased stock settings; boot stock once and save settings first"
        )

    patched = bytearray(data)
    changes: list[dict[str, int]] = []
    updates = {
        OFFSET_SIGNATURE: SIGNATURE_METER_RESTORE,
        OFFSET_SAVED_MODE: 0x01,
        OFFSET_SAVED_MODE + 1: 0x00,
    }
    for offset, new_value in updates.items():
        old_value = patched[offset]
        if old_value == new_value:
            continue
        patched[offset] = new_value
        changes.append(
            {
                "offset": offset,
                "address": STOCK_SAVED_CONFIG_ADDRESS + offset,
                "old": old_value,
                "new": new_value,
            }
        )

    return bytes(patched), changes


def print_summary(summary: dict[str, object]) -> None:
    fields = summary["fields"]
    startup = summary["startup_mode"]
    print(f"Stock settings @ 0x{summary['base_address']:08X} ({summary['parsed_size']} parsed bytes)")
    print(
        f"  signature: 0x{summary['signature']:02X} "
        f"({summary['signature_meaning']})"
    )
    print(
        "  meter: "
        f"mode={fields['meter_mode']} "
        f"range_word=0x{fields['meter_range_word']:08X}"
    )
    print(
        "  scope: "
        f"timebase_range={fields['timebase_range']} "
        f"timebase_mode={fields['timebase_mode']} "
        f"coupling={fields['coupling']} "
        f"trigger_mode={fields['trigger_mode']}"
    )
    print(
        "  saved mode: "
        f"operational={fields['saved_operational_mode']} "
        f"mode_data=0x{fields['mode_data_word']:08X} "
        f"saved_mode_word=0x{fields['saved_mode_word']:04X}"
    )
    print(
        "  inferred startup: "
        f"{startup['mode']} ({startup['confidence']})"
    )
    print(f"  language: {summary['language']['confidence']}")


def cmd_inspect(args: argparse.Namespace) -> int:
    data = args.dump.read_bytes()
    summary = parse_stock_settings(data, base_address=args.base_address)
    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print_summary(summary)
    return 0


def cmd_patch_startup_multimeter(args: argparse.Namespace) -> int:
    data = args.dump.read_bytes()
    patched, changes = patch_startup_multimeter(data)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(patched)
    if args.json:
        print(json.dumps({"out": str(args.out), "changes": changes}, indent=2, sort_keys=True))
    else:
        print(f"wrote {args.out}")
        if changes:
            print("Changes:")
            for change in changes:
                print(
                    "  "
                    f"0x{change['address']:08X} "
                    f"0x{change['old']:02X} -> 0x{change['new']:02X}"
                )
        else:
            print("No changes needed; dump already requests multimeter startup.")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    inspect = sub.add_parser("inspect", help="summarize a stock settings dump")
    inspect.add_argument("dump", type=Path)
    inspect.add_argument("--base-address", type=lambda text: int(text, 0), default=STOCK_SAVED_CONFIG_ADDRESS)
    inspect.add_argument("--json", action="store_true")
    inspect.set_defaults(func=cmd_inspect)

    fields = sub.add_parser("fields", help="list recovered stock saved-settings fields")
    fields.add_argument("--json", action="store_true")
    fields.set_defaults(func=cmd_fields)

    patch = sub.add_parser(
        "patch-startup-multimeter",
        help="write an offline copy that asks stock to start in multimeter mode",
    )
    patch.add_argument("dump", type=Path)
    patch.add_argument("--out", type=Path, required=True)
    patch.add_argument("--json", action="store_true")
    patch.set_defaults(func=cmd_patch_startup_multimeter)

    return parser


def cmd_fields(args: argparse.Namespace) -> int:
    fields = recovered_fields()
    if args.json:
        print(json.dumps(fields, indent=2, sort_keys=True))
    else:
        for field in fields:
            start = int(field["offset"])
            end = start + int(field["size"])
            print(
                f"0x{STOCK_SAVED_CONFIG_ADDRESS + start:08X}.."
                f"0x{STOCK_SAVED_CONFIG_ADDRESS + end:08X} "
                f"{field['name']} ({field['confidence']}): {field['description']}"
            )
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
