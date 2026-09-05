#!/usr/bin/env python3
"""Compare archived and fresh unpacked Verilog netlists structurally.

Both input SHA-256 values are mandatory. The verifier asks Yosys to parse each
input into JSON, canonicalizes named connectivity, reports primitive inventory,
and compares the canonical documents. Generated JSON is kept in a temporary
directory and is never written into the repository.

A passing result establishes only equality under this structural oracle. It
does not establish behavioral, timing, placement, routing, or bitstream
equivalence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from canonicalize_yosys_json import canonicalize, primitive_inventory

SHA256_RE = re.compile(r"[0-9a-fA-F]{64}\Z")
SCOPE = (
    "named structural connectivity only; not behavioral, timing, placement, "
    "routing, or bitstream equivalence"
)


class VerificationError(Exception):
    """An input or tool precondition prevented structural comparison."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def checked_hash(path: Path, expected: str, label: str) -> str:
    if not SHA256_RE.fullmatch(expected):
        raise VerificationError(f"{label} expected SHA-256 must be 64 hex digits")
    if not path.is_file():
        raise VerificationError(f"{label} input is not a file: {path}")
    actual = sha256_file(path)
    if actual != expected.lower():
        raise VerificationError(
            f"{label} SHA-256 mismatch: expected {expected.lower()}, got {actual}"
        )
    return actual


def yosys_json(input_path: Path, output_path: Path, *, yosys: str, top: str) -> None:
    input_link = output_path.with_suffix(".v")
    input_link.symlink_to(input_path.resolve())
    # Unpacked gate-level netlists intentionally instantiate Gowin primitives
    # without defining those vendor modules. Do not use ``hierarchy -check``:
    # the unresolved cells are exactly the structures this verifier inventories.
    script = f"read_verilog {input_link.name}; hierarchy -top {top}; write_json {output_path.name}"
    completed = subprocess.run(
        [yosys, "-q", "-p", script],
        cwd=output_path.parent,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        detail = completed.stderr.strip() or completed.stdout.strip() or "no diagnostic"
        raise VerificationError(
            f"Yosys failed for {input_path} with exit {completed.returncode}: {detail}"
        )


def canonical_digest(document: dict[str, Any]) -> str:
    payload = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def verify(args: argparse.Namespace) -> tuple[int, dict[str, Any]]:
    archived_hash = checked_hash(args.archived, args.archived_sha256, "archived")
    fresh_hash = checked_hash(args.fresh, args.fresh_sha256, "fresh")

    with tempfile.TemporaryDirectory(prefix="netlist-structural-check-") as tmp:
        work = Path(tmp)
        archived_json = work / "archived.json"
        fresh_json = work / "fresh.json"
        yosys_json(args.archived, archived_json, yosys=args.yosys, top=args.top)
        yosys_json(args.fresh, fresh_json, yosys=args.yosys, top=args.top)
        try:
            archived = canonicalize(archived_json, top=args.top)
            fresh = canonicalize(fresh_json, top=args.top)
        except (KeyError, json.JSONDecodeError, ValueError) as error:
            raise VerificationError(str(error)) from error

    match = archived == fresh
    report = {
        "oracle": "yosys_named_structural_connectivity",
        "scope": SCOPE,
        "structural_match": match,
        "inputs": {
            "archived": {"path": str(args.archived), "sha256": archived_hash},
            "fresh": {"path": str(args.fresh), "sha256": fresh_hash},
        },
        "canonical_sha256": {
            "archived": canonical_digest(archived),
            "fresh": canonical_digest(fresh),
        },
        "primitive_inventory": {
            "archived": primitive_inventory(archived),
            "fresh": primitive_inventory(fresh),
        },
    }
    return (0 if match else 1), report


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archived", required=True, type=Path)
    parser.add_argument("--archived-sha256", required=True)
    parser.add_argument("--fresh", required=True, type=Path)
    parser.add_argument("--fresh-sha256", required=True)
    parser.add_argument("--yosys", default="yosys")
    parser.add_argument("--top", default="top")
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        status, report = verify(args)
    except (OSError, VerificationError) as error:
        print(json.dumps({"error": str(error), "scope": SCOPE}, sort_keys=True), file=sys.stderr)
        raise SystemExit(2) from error
    print(json.dumps(report, indent=2, sort_keys=True))
    raise SystemExit(status)


if __name__ == "__main__":
    main()
