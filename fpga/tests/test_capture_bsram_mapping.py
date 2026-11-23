#!/usr/bin/env python3
"""Reject a GW1N default capture build if BSRAM mapping regresses."""

import pathlib
import re
import shutil
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
RTL = ROOT / "fpga" / "rtl"


@unittest.skipUnless(shutil.which("yosys"), "yosys is required for BSRAM mapping check")
class CaptureBsramMappingTest(unittest.TestCase):
    def test_default_top_uses_two_bsrams(self):
        sources = [
            RTL / "capture_channel.sv",
            RTL / "trigger_timebase.sv",
            RTL / "trigger_comparator.sv",
            RTL / "spi_runtime_interface.sv",
            RTL / "spi_control_registers.sv",
            RTL / "rate_divider.sv",
            RTL / "fnirsi_2c53t_top.sv",
        ]
        script = " ; ".join([
            "read_verilog -sv " + " ".join(map(str, sources)),
            "synth_gowin -family gw1n -top fnirsi_2c53t_top",
            "stat",
        ])
        result = subprocess.run(
            ["yosys", "-Q", "-p", script],
            cwd=ROOT,
            check=True,
            text=True,
            capture_output=True,
        )

        self.assertNotIn("Replacing memory", result.stdout)

        cells = {
            name: int(count)
            for count, name in re.findall(r"^\s*(\d+)\s+(DPX9B|DFFE|LUT4)\s*$", result.stdout, re.MULTILINE)
        }
        self.assertEqual(cells.get("DPX9B"), 2, result.stdout)
        self.assertLess(cells.get("DFFE", 0), 100, result.stdout)
        self.assertLess(cells.get("LUT4", 0), 1000, result.stdout)

    def test_top_default_capture_depth_is_1024(self):
        top_source = (RTL / "fnirsi_2c53t_top.sv").read_text()
        self.assertRegex(
            top_source,
            r"parameter\s+int\s+unsigned\s+CAPTURE_DEPTH\s*=\s*1024\s*,",
        )


if __name__ == "__main__":
    unittest.main()
