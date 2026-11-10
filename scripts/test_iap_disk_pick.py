#!/usr/bin/env python3
"""find_iap_disk_macos must pick the "IAP" volume, not a phantom listed before it (2026-11-10)."""
import plistlib
import sys
import types
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import iap_flash  # noqa: E402

DISKS = {
    "disk4": {"VolumeName": "", "MediaName": "Disk Image", "BusProtocol": "Disk Image", "TotalSize": 17_645_436_928},
    "disk9": {"VolumeName": "UNDEFINED", "MediaName": "microcontroller", "BusProtocol": "USB", "TotalSize": 425_984},
    "disk10": {"VolumeName": "IAP", "MediaName": "Msc iap", "BusProtocol": "USB", "TotalSize": 8_386_560},
}


def fake_run(cmd, timeout=None):
    if cmd[:3] == ["diskutil", "list", "-plist"]:
        data = {"AllDisks": list(DISKS)}
    else:
        data = DISKS[cmd[-1]]
    return types.SimpleNamespace(stdout=plistlib.dumps(data).decode())


class PickIap(unittest.TestCase):
    def setUp(self):
        self.orig = iap_flash._run
        iap_flash._run = fake_run

    def tearDown(self):
        iap_flash._run = self.orig

    def test_label_beats_phantom_listed_first(self):
        self.assertEqual(iap_flash.find_iap_disk_macos(), "/dev/disk10")

    def test_phantom_alone_is_not_a_target(self):
        saved = DISKS.pop("disk10")
        try:
            self.assertIsNone(iap_flash.find_iap_disk_macos())
        finally:
            DISKS["disk10"] = saved


if __name__ == "__main__":
    unittest.main()
