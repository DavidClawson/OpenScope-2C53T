#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_PATH = ROOT / "scripts" / "stock_settings_diff.py"


def load_module():
    spec = importlib.util.spec_from_file_location("stock_settings_diff", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise SystemExit("could not load stock_settings_diff")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    diff = load_module()
    before = bytearray(b"\xFF" * 0x3000)
    after = bytearray(before)
    after[0x0000] = 0x55
    after[0x0030] = 0x01
    after[0x0031] = 0x00
    after[0x1002] = 0x55
    after[0x1003] = 0xAA
    after[0x1FFC] = 0x10
    after[0x2000] = 0x20

    ranges = diff.changed_ranges(bytes(before), bytes(after))
    if ranges != [(0x0000, 0x0001), (0x0030, 0x0032), (0x1002, 0x1004), (0x1FFC, 0x1FFD), (0x2000, 0x2001)]:
        raise SystemExit(f"unexpected ranges: {ranges}")

    report = diff.build_report(
        bytes(before),
        bytes(after),
        base_address=0x08006000,
        sector_size=0x1000,
        max_gap=0,
        context=1,
        max_ranges=8,
    )
    if report["changed_bytes"] != 7:
        raise SystemExit("wrong changed byte count")
    if report["ranges"][0]["known_fields"][0]["name"] != "signature":
        raise SystemExit(f"signature range not labelled: {report['ranges'][0]}")
    if report["ranges"][1]["known_fields"][0]["name"] != "saved_mode_word":
        raise SystemExit(f"saved-mode range not labelled: {report['ranges'][1]}")
    if report["ranges"][2]["address_start"] != 0x08007002:
        raise SystemExit("wrong address mapping")
    sectors = report["sectors"]
    if [item["changed_bytes"] for item in sectors] != [3, 3, 1]:
        raise SystemExit(f"wrong sector summary: {sectors}")

    language_before = bytearray(b"\xFF" * 0x100)
    language_after = bytearray(language_before)
    language_after[0x80] = 0x02
    language_report = diff.build_report(
        bytes(language_before),
        bytes(language_after),
        base_address=0x08006000,
        sector_size=0x1000,
        max_gap=0,
        context=1,
        max_ranges=8,
        focus="language",
    )
    focus = language_report["focus"]
    if focus["confidence"] != "candidate":
        raise SystemExit(f"single unknown language change not ranked as candidate: {focus}")
    if focus["candidate_ranges"][0]["address_start"] != 0x08006080:
        raise SystemExit(f"wrong language candidate address: {focus}")

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
