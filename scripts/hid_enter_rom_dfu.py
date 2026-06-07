#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = ["hidapi"]
# ///
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from hid_flash import CMD_DFU, open_bootloader_device, send_recv


def main() -> int:
    dev = open_bootloader_device()
    try:
        send_recv(dev, CMD_DFU, expect_cmd=CMD_DFU)
        print("ROM DFU command acknowledged")
        time.sleep(0.5)
    finally:
        dev.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
