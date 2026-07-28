#!/usr/bin/env python3
from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOOTLOADER_MAIN = ROOT / "firmware" / "bootloader" / "src" / "main.c"


class BootloaderPowerEntryTests(unittest.TestCase):
    def test_power_alone_is_not_a_bootloader_entry_gesture(self) -> None:
        source = BOOTLOADER_MAIN.read_text()
        entry_block = source.split("/* 3. Flash upgrade flag", 1)[0]
        entry_block = entry_block.split("/* 2.5: Button-based bootloader entry.", 1)[1]
        executable_entry_block = "\n".join(
            line for line in entry_block.splitlines()
            if not line.strip().startswith("*")
        )

        self.assertEqual(executable_entry_block.count("power_button_pressed()"), 1)
        self.assertIn("power_button_pressed() && prm_button_pressed()", executable_entry_block)
        self.assertNotIn("busy_delay_ms(600)", executable_entry_block)

    def test_recovery_docs_name_power_prm_not_power_alone(self) -> None:
        root = Path(__file__).resolve().parents[1]
        readme = (root / "README.md").read_text()
        claude = (root / "CLAUDE.md").read_text()
        dfu_header = (root / "firmware/src/drivers/dfu_boot.h").read_text()
        watchdog = (root / "firmware/src/drivers/watchdog.c").read_text()

        self.assertIn("POWER+PRM", readme)
        self.assertIn("POWER+PRM", claude)
        self.assertIn("POWER+PRM", dfu_header)
        self.assertIn("POWER+PRM", watchdog)
        self.assertNotIn("Hold POWER at boot = force bootloader", claude)


if __name__ == "__main__":
    unittest.main()
