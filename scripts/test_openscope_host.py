#!/usr/bin/env python3
"""Run the host tool's suites (tools/openscope_host/tests) from the gate.

They live next to the tool so a contributor can work on it standalone; this
wrapper only makes scripts/run_tests.py see them. test_end_to_end compiles
the firmware's own esp_comm.c into a ctypes shim, so the host tool is gated
against the real protocol code, not a re-implementation of it.
"""
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent.parent / "tools" / "openscope_host" / "tests"

if __name__ == "__main__":
    sys.path.insert(0, str(TESTS))
    suite = unittest.defaultTestLoader.discover(str(TESTS), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)
