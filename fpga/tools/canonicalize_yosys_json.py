#!/usr/bin/env python3
"""Canonicalize a one-module Yosys JSON netlist by named connectivity.

Yosys assigns integer bit IDs in parse order. Apicula emits declarations and
instances from sets, so two structurally matching unpack runs can receive
different IDs. This tool replaces each integer bit with the complete, sorted
set of named aliases attached to that bit and sorts dictionaries before
serialization. Source-path attributes are deliberately excluded.

This is a structural comparison aid. A match does not establish behavioral,
timing, placement, routing, or bitstream equivalence.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


def canonicalize(path: Path, *, top: str = "top") -> dict[str, Any]:
    data = json.loads(path.read_text())
    if set(data["modules"]) != {top}:
        modules = ", ".join(sorted(data["modules"]))
        raise ValueError(f"expected only module {top!r} in {path}; found: {modules}")
    module = data["modules"][top]

    aliases: dict[int, list[str]] = defaultdict(list)
    for name, net in module["netnames"].items():
        for index, bit in enumerate(net["bits"]):
            if isinstance(bit, int):
                aliases[bit].append(f"{name}[{index}]")

    def bit_name(bit: int | str) -> str | tuple[str, ...]:
        if isinstance(bit, str):
            return bit
        names = aliases.get(bit)
        if not names:
            return (f"$unnamed:{bit}",)
        return tuple(sorted(names))

    ports = {
        name: {
            "direction": port["direction"],
            "bits": [bit_name(bit) for bit in port["bits"]],
        }
        for name, port in sorted(module["ports"].items())
    }

    cells = {}
    for name, cell in sorted(module["cells"].items()):
        cells[name] = {
            "type": cell["type"],
            "parameters": dict(sorted(cell.get("parameters", {}).items())),
            "port_directions": dict(sorted(cell.get("port_directions", {}).items())),
            "connections": {
                port: [bit_name(bit) for bit in bits]
                for port, bits in sorted(cell["connections"].items())
            },
        }

    alias_groups = sorted(tuple(sorted(names)) for names in aliases.values() if names)
    return {"ports": ports, "cells": cells, "alias_groups": alias_groups}


def primitive_inventory(canonical: dict[str, Any]) -> dict[str, int]:
    """Return a stable cell-type inventory from canonicalized structure."""
    counts = Counter(cell["type"] for cell in canonical["cells"].values())
    return dict(sorted(counts.items()))


def write_canonical_json(result: dict[str, Any], output: Path) -> None:
    output.write_text(json.dumps(result, sort_keys=True, separators=(",", ":")) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="one-module Yosys JSON input")
    parser.add_argument("output", type=Path, help="canonical JSON output")
    parser.add_argument("--top", default="top", help="expected sole module (default: top)")
    args = parser.parse_args()

    try:
        result = canonicalize(args.input, top=args.top)
    except (KeyError, json.JSONDecodeError, ValueError) as error:
        parser.error(str(error))
    write_canonical_json(result, args.output)


if __name__ == "__main__":
    main()
