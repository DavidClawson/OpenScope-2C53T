from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

from canonicalize_yosys_json import canonicalize, primitive_inventory  # noqa: E402

VERIFY = TOOLS / "verify_structural_netlists.py"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CanonicalizeTests(unittest.TestCase):
    def test_bit_ids_and_source_attributes_do_not_affect_canonical_form(self) -> None:
        module_template = {
            "ports": {
                "a": {"direction": "input", "bits": [1]},
                "y": {"direction": "output", "bits": [2]},
            },
            "cells": {
                "u0": {
                    "type": "LUT4",
                    "parameters": {"INIT": "0000000000000010"},
                    "attributes": {"src": "ignored.v:1.1-2.1"},
                    "port_directions": {"I0": "input", "F": "output"},
                    "connections": {"I0": [1], "F": [2]},
                }
            },
            "netnames": {"a": {"bits": [1]}, "y": {"bits": [2]}},
        }
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "first.json"
            second = Path(tmp) / "second.json"
            first.write_text(json.dumps({"modules": {"top": module_template}}))
            changed = json.loads(json.dumps(module_template))
            changed["ports"]["a"]["bits"] = [41]
            changed["ports"]["y"]["bits"] = [99]
            changed["cells"]["u0"]["connections"] = {"I0": [41], "F": [99]}
            changed["cells"]["u0"]["attributes"]["src"] = "elsewhere.v:8.1-9.1"
            changed["netnames"]["a"]["bits"] = [41]
            changed["netnames"]["y"]["bits"] = [99]
            second.write_text(json.dumps({"modules": {"top": changed}}))
            canonical = canonicalize(first)
            self.assertEqual(canonical, canonicalize(second))
            self.assertEqual({"LUT4": 1}, primitive_inventory(canonical))


class VerifierIntegrationTests(unittest.TestCase):
    def run_verifier(self, archived: Path, fresh: Path, **overrides: str) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            str(VERIFY),
            "--archived",
            str(archived),
            "--archived-sha256",
            overrides.get("archived_sha256", sha256(archived)),
            "--fresh",
            str(fresh),
            "--fresh-sha256",
            overrides.get("fresh_sha256", sha256(fresh)),
        ]
        return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_matching_structure_passes_and_reports_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archived = Path(tmp) / "archived.v"
            fresh = Path(tmp) / "fresh.v"
            archived.write_text(
                "module top(input a, output y); wire n; LUT4 #(.INIT(16'h0002)) u0 (.I0(a), .F(n)); assign y=n; endmodule\n"
            )
            fresh.write_text(
                "module top(output y, input a); wire n; LUT4 #(.INIT(16'h0002)) u0 (.F(n), .I0(a)); assign y = n; endmodule\n"
            )
            result = self.run_verifier(archived, fresh)
            self.assertEqual(0, result.returncode, result.stderr)
            report = json.loads(result.stdout)
            self.assertTrue(report["structural_match"])
            self.assertEqual({"LUT4": 1}, report["primitive_inventory"]["archived"])
            self.assertIn("not behavioral", report["scope"])

    def test_changed_primitive_fails_structural_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archived = Path(tmp) / "archived.v"
            fresh = Path(tmp) / "fresh.v"
            archived.write_text("module top(input a, output y); LUT4 u0 (.I0(a), .F(y)); endmodule\n")
            fresh.write_text("module top(input a, output y); DFF u0 (.D(a), .Q(y)); endmodule\n")
            result = self.run_verifier(archived, fresh)
            self.assertEqual(1, result.returncode, result.stderr)
            report = json.loads(result.stdout)
            self.assertFalse(report["structural_match"])
            self.assertEqual({"LUT4": 1}, report["primitive_inventory"]["archived"])
            self.assertEqual({"DFF": 1}, report["primitive_inventory"]["fresh"])

    def test_wrong_input_hash_stops_with_precondition_error(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archived = Path(tmp) / "archived.v"
            fresh = Path(tmp) / "fresh.v"
            archived.write_text("module top(input a, output y); assign y=a; endmodule\n")
            fresh.write_text(archived.read_text())
            result = self.run_verifier(archived, fresh, archived_sha256="0" * 64)
            self.assertEqual(2, result.returncode)
            report = json.loads(result.stderr)
            self.assertIn("archived SHA-256 mismatch", report["error"])


if __name__ == "__main__":
    unittest.main()
