#!/usr/bin/env python3
"""Static invariants of the debug-shell command table (usb_debug.c).

The table replaced a ~110-entry if/strcmp dispatch chain on 2026-08-20
(structural audit item 8.9). Its whole value is that one row binds a
command's name, handler, bus-safety flag and help text together — these
tests fail the suite if that binding rots:

  1. every row's handler function is defined in the file;
  2. every non-alias row carries help text (an alias row is an extra name
     for a handler that already has a helped row);
  3. row names are unique;
  4. THE ONE THAT BITES: any handler that drives the raw SPI3 bus
     (spi3_raw_xfer directly, or via cmd_spi3_acqread_one) must either be
     flagged SC_SPI3 (dispatcher parks the acquisition task, audit P0.1)
     or park internally with fpga_acq_pause(). Before the table existed,
     seven shell commands silently drove the bus under a running acq task.
"""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "firmware/src/drivers/usb_debug.c"

ROW_RE = re.compile(
    r'CMD_([AV])\("(?P<name>[^"]+)",\s*(?P<fn>\w+),\s*(?P<flags>[^,]+),\s*'
    r'(?P<help>NULL|")',
    re.S)


def parse():
    text = SRC.read_text()
    rows = []
    for m in re.finditer(
            r'CMD_([AV])\(\s*"([^"]+)",\s*(\w+),\s*([A-Z0-9_ |]+?),\s*(NULL|")',
            text):
        rows.append({
            "kind": m.group(1),
            "name": m.group(2),
            "fn": m.group(3),
            "flags": m.group(4).strip(),
            "has_help": m.group(5) != "NULL",
        })
    # drop the two macro-definition pseudo-matches if the regex ever catches
    rows = [r for r in rows if r["fn"] != "fn"]
    return text, rows


class ShellTableInvariants(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text, cls.rows = parse()
        assert len(cls.rows) > 90, f"table parse broke: {len(cls.rows)} rows"

    def test_handlers_defined(self):
        missing = [r["fn"] for r in self.rows
                   if not re.search(r"static void %s\(" % r["fn"], self.text)]
        self.assertEqual(missing, [])

    def test_help_present_on_non_alias_rows(self):
        helped = {r["fn"] for r in self.rows if r["has_help"]}
        naked = [r["name"] for r in self.rows
                 if not r["has_help"] and r["fn"] not in helped]
        self.assertEqual(naked, [],
                         "rows with no help and no helped sibling row")

    def test_names_unique(self):
        names = [r["name"] for r in self.rows]
        dupes = sorted({n for n in names if names.count(n) > 1})
        self.assertEqual(dupes, [])

    def _bodies(self):
        """fn name -> body text, for every static function in the file.
        Function bodies end at the first close-brace in column 0, which
        holds for this file's uniform style."""
        if not hasattr(self, "_body_cache"):
            self._body_cache = {}
            # Lazy prefix so the captured word is the identifier directly
            # before '(' — a greedy [\w ]+ here backtracks to single letters.
            for m in re.finditer(
                    r"^static [^(\n]*?(\w+)\(([^;{)]*)\)\s*\n\{(.*?)\n\}",
                    self.text, re.S | re.M):
                self._body_cache[m.group(1)] = m.group(3)
        return self._body_cache

    def test_raw_spi3_users_are_flagged_or_park(self):
        bodies = self._bodies()
        # Transitive closure: a function is a bus user if its body touches
        # the raw bus itself, or calls any local function that is one. This
        # catches indirection like cmd_spi3_gowin -> spi3_gowin_read_reg ->
        # spi3_raw_xfer (the first negative control missed exactly that).
        def touches_bus(body):
            return ("spi3_raw_xfer" in body
                    or "0x40003C" in body)          # SPI3 register block
        users = {fn for fn, b in bodies.items() if touches_bus(b)}
        changed = True
        while changed:
            changed = False
            for fn, b in bodies.items():
                if fn in users:
                    continue
                if any(re.search(r"\b%s\s*\(" % u, b) for u in users):
                    users.add(fn)
                    changed = True

        offenders = []
        for r in self.rows:
            if r["fn"] not in users:
                continue
            flagged = "SC_SPI3" in r["flags"]
            parks = "fpga_acq_pause" in bodies.get(r["fn"], "")
            if not (flagged or parks):
                offenders.append(r["name"])
        self.assertEqual(offenders, [],
                         "raw-SPI3 shell commands neither SC_SPI3-flagged nor "
                         "parking internally — the exact hole audit P0.1 closed")

    def test_needargs_only_on_arg_handlers(self):
        bad = [r["name"] for r in self.rows
               if "SC_NEEDARGS" in r["flags"] and r["kind"] != "A"]
        self.assertEqual(bad, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
