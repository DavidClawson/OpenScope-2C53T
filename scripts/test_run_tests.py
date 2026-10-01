#!/usr/bin/env python3
"""Tests for scripts/run_tests.py's --strict / --allow-skip contract.

WHY THIS EXISTS
---------------
CI cannot have the copyrighted stock FNIRSI image, so a plain `--strict` would
fail there forever. `--allow-skip SUITE:REASON` lets CI name exactly the skips
it cannot avoid -- but an allowlist is also the easiest way to make the gate
lie. These tests pin the only property that matters: a skip is tolerated only
when BOTH its suite and its reason were named, and nothing else is.

Each case drives the real runner over throwaway suites in a temp directory, so
the skip lines it parses are the ones real `unittest -v` output produces
(including the docstring form that once defeated the parser).
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).resolve().parent / "run_tests.py"
SPEC = importlib.util.spec_from_file_location("run_tests", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
run_tests = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(run_tests)

STOCK = "stock archive image is not present: /nowhere/APP.bin"
NO_GCC = "cannot build 'app': arm-none-eabi-gcc is not on PATH"

UNITTEST_SKIP = '''
import unittest
class T(unittest.TestCase):
    def test_skips(self):
        """First docstring line, so the result trails a second line."""
        self.skipTest(%r)
if __name__ == "__main__":
    unittest.main()
'''

FAKE_SUITES = {
    # unittest form, WITH a docstring: the skip result trails a second line.
    "test_fake_unittest_stock.py": UNITTEST_SKIP % STOCK,
    # the non-unittest form some real suites print: a bare `skipped '...'` line.
    "test_fake_bare_stock.py": f"print(\"skipped '{STOCK}'\")\nprint('0 tests OK (1 skipped)')\n",
    # a DIFFERENT reason for skipping.
    "test_fake_unittest_toolchain.py": UNITTEST_SKIP % NO_GCC,
    "test_fake_clean.py": (
        "import unittest\n"
        "class T(unittest.TestCase):\n"
        "    def test_ok(self):\n"
        "        self.assertTrue(True)\n"
        "if __name__ == '__main__':\n"
        "    unittest.main()\n"
    ),
    "test_fake_failing.py": (
        "import unittest\n"
        "class T(unittest.TestCase):\n"
        "    def test_bad(self):\n"
        "        self.fail('boom')\n"
        "if __name__ == '__main__':\n"
        "    unittest.main()\n"
    ),
}

ALLOW_STOCK_UNITTEST = ("--allow-skip", "test_fake_unittest_stock.py:stock archive")
ALLOW_STOCK_BARE = ("--allow-skip", "test_fake_bare_stock.py:stock archive")
ALLOW_NO_GCC = ("--allow-skip", "test_fake_unittest_toolchain.py:arm-none-eabi-gcc is not on PATH")


class AllowSkipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._tmp = tempfile.TemporaryDirectory()
        cls.scripts = Path(cls._tmp.name) / "scripts"
        cls.scripts.mkdir()
        for name, source in FAKE_SUITES.items():
            (cls.scripts / name).write_text(source)

    @classmethod
    def tearDownClass(cls) -> None:
        cls._tmp.cleanup()

    def run_runner(self, *argv: str, suites: tuple[str, ...] = tuple(FAKE_SUITES)) -> tuple[int, str]:
        """Run main() over the fake suites. Returns (exit code, stdout+stderr)."""
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(run_tests, "SCRIPTS", self.scripts), \
                mock.patch.object(run_tests, "SUITES", suites), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = run_tests.main(list(argv))
            except SystemExit as exc:  # argparse errors exit 2
                code = int(exc.code)
        return code, out.getvalue() + err.getvalue()

    def test_parsing_sanity(self) -> None:
        """Guard the harness itself: both fakes must be parsed as ONE skip with
        the reason the cases below match, or every case here proves nothing."""
        for name in ("test_fake_unittest_stock.py", "test_fake_bare_stock.py"):
            with mock.patch.object(run_tests, "SCRIPTS", self.scripts):
                result = run_tests.run_suite(name)
            self.assertEqual(len(result.skipped), 1, name)
            self.assertIn("stock archive image is not present", result.skipped[0][1], name)

    def test_strict_without_allowlist_fails_on_any_skip(self) -> None:
        code, text = self.run_runner("--strict", "-k", "fake_unittest_stock")
        self.assertEqual(code, 1, text)
        self.assertIn("--strict: failing because 1 test(s) were skipped", text)

    def test_allowed_skip_passes_strict_but_is_still_reported(self) -> None:
        code, text = self.run_runner("--strict", "-k", "fake_unittest_stock", *ALLOW_STOCK_UNITTEST)
        self.assertEqual(code, 0, text)
        self.assertIn("DID NOT RUN", text)  # still a headline, not hidden
        self.assertIn("[allowed]", text)
        self.assertIn(STOCK, text)  # and the reason is still printed

    def test_whole_suite_bare_skip_can_be_allowed(self) -> None:
        code, text = self.run_runner("--strict", "-k", "fake_bare_stock", *ALLOW_STOCK_BARE)
        self.assertEqual(code, 0, text)
        self.assertIn("(whole suite)", text)

    def test_same_suite_skipping_for_a_new_reason_still_fails(self) -> None:
        """The reason half of the rule is load-bearing: allow the stock image
        and a missing toolchain in the same suite must still turn the gate red."""
        code, text = self.run_runner(
            "--strict", "-k", "fake_unittest_toolchain",
            "--allow-skip", "test_fake_unittest_toolchain.py:stock archive",
        )
        self.assertEqual(code, 1, text)
        self.assertIn("outside the --allow-skip list", text)

    def test_allowing_one_suite_does_not_allow_another(self) -> None:
        code, text = self.run_runner("--strict", "-k", "fake_bare_stock", *ALLOW_STOCK_UNITTEST)
        self.assertEqual(code, 1, text)

    def test_mixed_run_fails_on_the_one_unlisted_skip(self) -> None:
        code, text = self.run_runner(
            "--strict", *ALLOW_STOCK_UNITTEST, *ALLOW_STOCK_BARE,
            suites=("test_fake_unittest_stock.py", "test_fake_bare_stock.py",
                    "test_fake_unittest_toolchain.py", "test_fake_clean.py"),
        )
        self.assertEqual(code, 1, text)
        self.assertIn("2 on the --allow-skip list, 1 not", text)
        self.assertIn("failing because 1 test(s)", text)

    def test_mixed_run_passes_when_every_skip_is_listed(self) -> None:
        code, text = self.run_runner(
            "--strict", *ALLOW_STOCK_UNITTEST, *ALLOW_STOCK_BARE, *ALLOW_NO_GCC,
            suites=("test_fake_unittest_stock.py", "test_fake_bare_stock.py",
                    "test_fake_unittest_toolchain.py", "test_fake_clean.py"),
        )
        self.assertEqual(code, 0, text)
        self.assertIn("3 on the --allow-skip list, 0 not", text)

    def test_a_failing_suite_is_not_excused_by_the_allowlist(self) -> None:
        code, text = self.run_runner(
            "--strict", "-k", "fake_failing", "--allow-skip", "test_fake_failing.py:boom",
        )
        self.assertEqual(code, 1, text)
        self.assertIn("FAILED: test_fake_failing.py", text)

    def test_no_skips_passes_strict(self) -> None:
        code, text = self.run_runner("--strict", "-k", "fake_clean")
        self.assertEqual(code, 0, text)
        self.assertIn("All tests ran. No skips.", text)

    def test_skips_never_fail_without_strict(self) -> None:
        code, _ = self.run_runner("-k", "fake_unittest_toolchain")
        self.assertEqual(code, 0)

    def test_malformed_rules_are_rejected(self) -> None:
        for bad in (
            "test_fake_unittest_stock.py",       # no reason: would blind the whole suite
            "test_fake_unittest_stock.py:",      # empty reason
            ":stock archive",                    # no suite
            "test_fake_typo.py:stock archive",   # not a suite: a typo must not be a silent no-op
        ):
            with self.subTest(rule=bad):
                code, text = self.run_runner("--strict", "-k", "fake_clean", "--allow-skip", bad)
                self.assertEqual(code, 2, text)

    def test_every_real_suite_name_is_addressable(self) -> None:
        """CI names real suites in --allow-skip, so each must parse as a rule."""
        for suite in run_tests.SUITES:
            self.assertEqual(run_tests.parse_allow_skip(f"{suite}:why"), (suite, "why"))

    def test_this_suite_is_registered_in_the_gate(self) -> None:
        self.assertIn(Path(__file__).name, run_tests.SUITES)


if __name__ == "__main__":
    unittest.main()
