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


def sample_blob(signature: int = 0x55, saved_mode: int = 0, language: int = 0x02) -> bytes:
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
    data[0x2C:0x30] = (0xAABBCC00 | language).to_bytes(4, "little")
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
    if normal["language"]["value"] != "english" or normal["language"]["confidence"] != "confirmed":
        raise SystemExit(f"stock language not decoded as English: {normal['language']}")
    if normal["fields"]["language"] != 0x02:
        raise SystemExit("raw language field not decoded")

    chinese = stock.parse_stock_settings(sample_blob(language=0x01))
    if chinese["language"]["value"] != "chinese":
        raise SystemExit(f"stock language not decoded as Chinese: {chinese['language']}")

    unknown_language = stock.parse_stock_settings(sample_blob(language=0x03))
    if unknown_language["language"]["confidence"] != "unknown":
        raise SystemExit(f"unexpected unknown-language decode: {unknown_language['language']}")

    meter = stock.parse_stock_settings(sample_blob(0xAA, 1))
    if meter["startup_mode"]["mode"] != "multimeter":
        raise SystemExit(f"unexpected meter startup inference: {meter['startup_mode']}")
    meter_overlay = stock.openscope_config_overlay(meter)
    if meter_overlay["fields"]["startup_mode"]["config_value"] != stock.OPENSCOPE_CONFIG_MODES["multimeter"]:
        raise SystemExit(f"multimeter overlay did not map to OpenScope config: {meter_overlay}")
    if meter_overlay["fields"]["language"]["config_value"] != stock.OPENSCOPE_CONFIG_LANGUAGES["english"]:
        raise SystemExit(f"English overlay did not map to OpenScope config: {meter_overlay}")
    if not meter_overlay["apply_safe"]:
        raise SystemExit("valid multimeter overlay should be apply-safe")

    chinese_overlay = stock.openscope_config_overlay(chinese)
    if chinese_overlay["fields"]["language"]["config_value"] != stock.OPENSCOPE_CONFIG_LANGUAGES["chinese"]:
        raise SystemExit(f"Chinese overlay did not map to OpenScope config: {chinese_overlay}")

    normal_overlay = stock.openscope_config_overlay(normal)
    if normal_overlay["fields"]["startup_mode"]["config_value"] != stock.OPENSCOPE_CONFIG_MODES["oscilloscope"]:
        raise SystemExit(f"normal overlay did not map to OpenScope scope mode: {normal_overlay}")

    patched, changes = stock.patch_startup_multimeter(sample_blob(saved_mode=0x2200))
    if patched[0] != 0xAA or patched[0x30:0x32] != b"\x01\x00":
        raise SystemExit("startup multimeter patch missed required bytes")
    if [(item["offset"], item["new"]) for item in changes] != [(0x00, 0xAA), (0x30, 0x01), (0x31, 0x00)]:
        raise SystemExit(f"unexpected changes: {changes}")
    if patched[0x32:] != sample_blob(saved_mode=0x2200)[0x32:]:
        raise SystemExit("patch touched bytes outside the guarded offsets")

    fields = stock.recovered_fields()
    if not any(field["name"] == "saved_mode_word" and field["offset"] == 0x30 for field in fields):
        raise SystemExit("recovered field inventory lost saved_mode_word")
    if not any(field["name"] == "language" and field["offset"] == 0x2C for field in fields):
        raise SystemExit("recovered field inventory lost language")

    try:
        stock.patch_startup_multimeter(bytes([0xFF]) * 0x1000)
    except ValueError:
        pass
    else:
        raise SystemExit("erased stock settings must not be patchable")
    erased = stock.parse_stock_settings(bytes([0xFF]) * 0x1000)
    if stock.openscope_config_overlay(erased)["apply_safe"]:
        raise SystemExit("erased stock settings must not produce an apply-safe overlay")

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
        if out.read_bytes()[0] != 0xAA or out.read_bytes()[0x30:0x32] != b"\x01\x00":
            raise SystemExit("CLI patch output is wrong")
        overlay_proc = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), "export-openscope-overlay", str(out)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if overlay_proc.returncode != 0:
            raise SystemExit(overlay_proc.stderr)
        overlay = __import__("json").loads(overlay_proc.stdout)
        if overlay["fields"]["startup_mode"]["value"] != "multimeter":
            raise SystemExit(f"CLI overlay did not export multimeter startup: {overlay}")

    print("1 test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
