#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_PATH = ROOT / "scripts" / "stock_settings.py"


def load_module():
    spec = importlib.util.spec_from_file_location("stock_settings", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise SystemExit("could not load stock_settings")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sample_blob(signature: int = 0x55, saved_mode: int = 0) -> bytes:
    stock = load_module()
    data = bytearray(b"\xFF" * 0x1000)
    data[0x00] = signature
    data[0x03] = 5
    data[0x04:0x08] = (0x12345678).to_bytes(4, "little")
    data[0x0A] = 7
    data[0x0B] = 9
    data[0x0D] = 2
    data[0x0F] = 1
    data[0x17] = 3
    data[0x2C:0x30] = (0xAABBCCDD).to_bytes(4, "little")
    data[0x30:0x32] = saved_mode.to_bytes(2, "little")
    data[0x12C:0x130] = b"\x01\x02\x03\x04"
    assert len(data) >= stock.STOCK_SAVED_CONFIG_SIZE
    return bytes(data)


def main() -> int:
    stock = load_module()

    normal = stock.parse_stock_settings(sample_blob())
    if normal["signature_meaning"] != "valid-normal-restore":
        raise SystemExit("normal signature not decoded")
    if normal["startup_mode"]["mode"] != "scope-or-startup-default":
        raise SystemExit(f"unexpected normal startup inference: {normal['startup_mode']}")
    if normal["fields"]["meter_range_word"] != 0x12345678:
        raise SystemExit("meter range word not decoded")
    if normal["language"]["confidence"] != "unknown":
        raise SystemExit("stock language should remain explicitly unknown")

    meter = stock.parse_stock_settings(sample_blob(0xAA, 1))
    if meter["startup_mode"]["mode"] != "multimeter":
        raise SystemExit(f"unexpected meter startup inference: {meter['startup_mode']}")

    patched, changes = stock.patch_startup_multimeter(sample_blob())
    if patched[0] != 0xAA or patched[0x30] != 1:
        raise SystemExit("startup multimeter patch missed required bytes")
    if [(item["offset"], item["new"]) for item in changes] != [(0x00, 0xAA), (0x30, 0x01)]:
        raise SystemExit(f"unexpected changes: {changes}")
    if patched[0x31] != sample_blob()[0x31] or patched[0x32:] != sample_blob()[0x32:]:
        raise SystemExit("patch touched bytes outside the guarded offsets")

    try:
        stock.patch_startup_multimeter(bytes([0xFF]) * 0x1000)
    except ValueError:
        pass
    else:
        raise SystemExit("erased stock settings must not be patchable")

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "settings.bin"
        out = Path(tmp) / "settings-meter.bin"
        path.write_bytes(sample_blob())
        proc = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), "patch-startup-multimeter", str(path), "--out", str(out)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if proc.returncode != 0:
            raise SystemExit(proc.stderr)
        if out.read_bytes()[0] != 0xAA or out.read_bytes()[0x30] != 1:
            raise SystemExit("CLI patch output is wrong")

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
