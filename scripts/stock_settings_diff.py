#!/usr/bin/env python3
"""Compare two stock settings / flash dumps and summarize changed regions.

The tool is intentionally read-only.  Use it after taking two dumps with
`dump_spi_flash.py` or `hid_flash.py` memory reads, changing exactly one stock
setting between the dumps.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import stock_settings


def changed_ranges(before: bytes, after: bytes, *, max_gap: int = 0) -> list[tuple[int, int]]:
    if len(before) != len(after):
        raise ValueError(f"dump sizes differ: before={len(before)} after={len(after)}")
    if max_gap < 0:
        raise ValueError("max_gap must be non-negative")

    ranges: list[tuple[int, int]] = []
    start: int | None = None
    last_changed: int | None = None

    for offset, (old, new) in enumerate(zip(before, after)):
        if old == new:
            continue
        if start is None:
            start = offset
        elif last_changed is not None and offset - last_changed > max_gap + 1:
            ranges.append((start, last_changed + 1))
            start = offset
        last_changed = offset

    if start is not None and last_changed is not None:
        ranges.append((start, last_changed + 1))
    return ranges


def sector_summary(ranges: list[tuple[int, int]], sector_size: int) -> list[dict[str, int]]:
    if sector_size <= 0:
        raise ValueError("sector_size must be positive")

    sectors: dict[int, int] = {}
    for start, end in ranges:
        cursor = start
        while cursor < end:
            sector = cursor // sector_size
            sector_end = min(end, (sector + 1) * sector_size)
            sectors[sector] = sectors.get(sector, 0) + (sector_end - cursor)
            cursor = sector_end

    return [
        {
            "sector": sector,
            "start": sector * sector_size,
            "end": (sector + 1) * sector_size,
            "changed_bytes": changed,
        }
        for sector, changed in sorted(sectors.items())
    ]


def _hex_window(data: bytes, start: int, end: int, context: int) -> str:
    window_start = max(0, start - context)
    window_end = min(len(data), end + context)
    return data[window_start:window_end].hex(" ")


def field_labels(start: int, end: int) -> list[dict[str, object]]:
    return stock_settings.fields_overlapping(start, end)


def build_report(
    before: bytes,
    after: bytes,
    *,
    base_address: int,
    sector_size: int,
    max_gap: int,
    context: int,
    max_ranges: int,
) -> dict[str, object]:
    ranges = changed_ranges(before, after, max_gap=max_gap)
    changed = sum(end - start for start, end in ranges)
    shown = ranges[:max_ranges]

    return {
        "base_address": base_address,
        "size": len(before),
        "changed_bytes": changed,
        "range_count": len(ranges),
        "ranges": [
            {
                "offset_start": start,
                "offset_end": end,
                "address_start": base_address + start,
                "address_end": base_address + end,
                "changed_bytes": end - start,
                "before": _hex_window(before, start, end, context),
                "after": _hex_window(after, start, end, context),
                "known_fields": field_labels(start, end),
                "contains_unknown_stock_settings_bytes": not field_labels(start, end),
            }
            for start, end in shown
        ],
        "range_output_truncated": len(ranges) > len(shown),
        "sectors": sector_summary(ranges, sector_size),
    }


def print_text_report(report: dict[str, object]) -> None:
    print(
        "Changed "
        f"{report['changed_bytes']} bytes in {report['range_count']} ranges "
        f"(base 0x{report['base_address']:08X}, size {report['size']})"
    )
    print("Ranges:")
    for item in report["ranges"]:
        print(
            "  "
            f"0x{item['address_start']:08X}..0x{item['address_end']:08X} "
            f"({item['changed_bytes']} bytes)"
        )
        print(f"    before: {item['before']}")
        print(f"    after:  {item['after']}")
        if item["known_fields"]:
            labels = ", ".join(
                f"{field['name']}:{field['confidence']}" for field in item["known_fields"]
            )
            print(f"    known fields: {labels}")
        else:
            print("    known fields: none")
    if report["range_output_truncated"]:
        print("  ... range output truncated")

    print("Sectors:")
    for item in report["sectors"]:
        print(
            "  "
            f"sector {item['sector']} "
            f"0x{item['start']:06X}..0x{item['end']:06X}: "
            f"{item['changed_bytes']} changed bytes"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Diff two read-only stock settings / flash dumps")
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--base-address", type=lambda x: int(x, 0), default=0)
    parser.add_argument("--sector-size", type=lambda x: int(x, 0), default=0x1000)
    parser.add_argument("--max-gap", type=lambda x: int(x, 0), default=0)
    parser.add_argument("--context", type=lambda x: int(x, 0), default=8)
    parser.add_argument("--max-ranges", type=int, default=64)
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    report = build_report(
        args.before.read_bytes(),
        args.after.read_bytes(),
        base_address=args.base_address,
        sector_size=args.sector_size,
        max_gap=args.max_gap,
        context=args.context,
        max_ranges=args.max_ranges,
    )
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print_text_report(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
