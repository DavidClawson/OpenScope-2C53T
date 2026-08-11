#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import subprocess
import sys
from pathlib import Path


VID = "2e3c"
PID_ROM_DFU = "df11"
PID_HID_IAP = "af01"
PID_CDC = "5740"

FLASH_BASE = 0x08000000
APP_ADDRESS = 0x08004000
APP_SETTINGS_ADDRESS = 0x080FF800
APP_SLOT_END_ADDRESS = 0x080F0000
STOCK_SAVED_CONFIG_ADDRESS = 0x08006000
STOCK_SAVED_CONFIG_END_ADDRESS = 0x08007000
HIGH_BOOTLOADER_ADDRESS = APP_SLOT_END_ADDRESS
HIGH_DISPATCHER_ADDRESS = 0x080E0000
FLASH_MAX = 0x08100000
RAM_MASK = 0xFFF00000
RAM_BASE = 0x20000000
BLOCK_SIZE = 1024
BOOTLOADER_UPDATER_MARKER = b"OpenScope bootloader updater"
HIGH_BOOTLOADER_READ_MEM_ACK_PATTERN = b"\x5A\xAB\xFF\x00"
HIGH_BOOTLOADER_READ_MEM_SRAM_END = struct.pack("<I", 0x20038000)
STOCK_FALSE_SCATTER_TABLE_OFFSET = 0x0B7314
STOCK_FALSE_SCATTER_TABLE_LENGTH = 0x20
STOCK_RUNTIME_BASE = FLASH_BASE
STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET = 0x184
STOCK_BOOT_PREAMBLE_CANDIDATE = FLASH_BASE + STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET
STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET = 0x2F0
STOCK_LOW_RUNTIME_ENTRY_CANDIDATE = FLASH_BASE + STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET
STOCK_NATIVE_ENTRY_CANDIDATE_OFFSET = 0x310
STOCK_NATIVE_ENTRY_CANDIDATE = FLASH_BASE + STOCK_NATIVE_ENTRY_CANDIDATE_OFFSET
STOCK_NATIVE_ENTRY_LITERAL_OFFSET = STOCK_NATIVE_ENTRY_CANDIDATE_OFFSET + 0x44
STOCK_NATIVE_ENTRY_LITERAL_TARGETS = (0x08033F7D, 0x0802A9C5, 0x08007185)
STOCK_RESET_VECTOR = 0x08007311
STOCK_RESET_DISPATCH_TABLE_OFFSET = 0x7308
STOCK_RESET_DISPATCH_CASE_OFFSET = 0x7310


USB_MODES = {
    PID_ROM_DFU: "ROM_DFU",
    PID_HID_IAP: "HID_IAP",
    PID_CDC: "CDC_DEBUG",
}


def read_file(path: Path) -> bytes:
    if not path.exists():
        raise FileNotFoundError(path)
    return path.read_bytes()


def read_vectors(data: bytes) -> tuple[int, int]:
    if len(data) < 8:
        raise ValueError("image is too small to contain an ARM vector table")
    return struct.unpack("<II", data[:8])


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def padded_len(length: int) -> int:
    pad = BLOCK_SIZE - (length % BLOCK_SIZE)
    return length if pad == BLOCK_SIZE else length + pad


def classify_image(path: Path, data: bytes, address: int) -> str:
    name = path.name.upper()
    has_openscope_string = b"OpenScope" in data or b"OPENSCOPE" in data
    if name.startswith("APP_2C53T"):
        return "stock-app"
    if address == FLASH_BASE:
        return "openscope-bootloader" if b"BOOTLOADER MODE" in data or b"HID IAP" in data else "bootloader-like"
    if address == HIGH_BOOTLOADER_ADDRESS:
        return "openscope-high-bootloader" if b"BOOTLOADER MODE" in data or b"HID IAP" in data else "high-bootloader-like"
    if address == APP_ADDRESS and has_openscope_string:
        return "openscope-app"
    return "unknown-app" if address >= APP_ADDRESS else "unknown"


def validate_vectors(data: bytes, address: int, *, app_slot: bool) -> tuple[int, int, list[str]]:
    sp, rv = read_vectors(data)
    errors: list[str] = []
    if (sp & RAM_MASK) != RAM_BASE:
        errors.append(f"stack pointer 0x{sp:08X} is not in SRAM")
    if not (FLASH_BASE <= rv < FLASH_MAX):
        errors.append(f"reset vector 0x{rv:08X} is not in internal flash")
    if app_slot and rv < APP_ADDRESS:
        errors.append(f"reset vector 0x{rv:08X} is below app slot 0x{APP_ADDRESS:08X}")
    if not app_slot and not (FLASH_BASE <= rv < APP_ADDRESS):
        errors.append(f"bootloader reset vector 0x{rv:08X} is not inside bootloader region")
    if address % 0x400 != 0:
        errors.append(f"address 0x{address:08X} is not 1KB-aligned")
    return sp, rv, errors


def validate_bootloader_payload(data: bytes) -> list[str]:
    _sp, _rv, errors = validate_vectors(data, FLASH_BASE, app_slot=False)
    if len(data) > APP_ADDRESS - FLASH_BASE:
        errors.append("bootloader payload exceeds the 16KB bootloader region")
    if b"HID IAP" not in data and b"BOOTLOADER MODE" not in data:
        errors.append("bootloader payload is missing the OpenScope HID bootloader marker")
    return errors


def validate_stage0_payload(data: bytes) -> list[str]:
    sp, rv = read_vectors(data)
    errors: list[str] = []
    if (sp & RAM_MASK) != RAM_BASE:
        errors.append(f"stack pointer 0x{sp:08X} is not in SRAM")
    if len(data) > 0x800:
        errors.append("stage0 payload exceeds the 2KB low-flash sector")
    if not (
        FLASH_BASE <= rv < FLASH_BASE + 0x800
        or HIGH_DISPATCHER_ADDRESS <= rv < HIGH_BOOTLOADER_ADDRESS
    ):
        errors.append(
            f"stage0 reset vector 0x{rv:08X} is neither inside the stage0 sector "
            "nor the stock-switcher high dispatcher window"
        )
    return errors


def validate_high_bootloader_payload(data: bytes) -> list[str]:
    sp, rv, errors = validate_vectors(data, HIGH_BOOTLOADER_ADDRESS, app_slot=True)
    if (sp & RAM_MASK) != RAM_BASE:
        errors.append(f"stack pointer 0x{sp:08X} is not in SRAM")
    if not (HIGH_BOOTLOADER_ADDRESS <= rv < FLASH_MAX):
        errors.append(f"high bootloader reset vector 0x{rv:08X} is not in high recovery flash")
    if len(data) > FLASH_MAX - HIGH_BOOTLOADER_ADDRESS:
        errors.append("high bootloader payload exceeds the 64KB high recovery region")
    if b"HID IAP" not in data and b"BOOTLOADER MODE" not in data:
        errors.append("high bootloader payload is missing the OpenScope HID bootloader marker")
    if HIGH_BOOTLOADER_READ_MEM_ACK_PATTERN not in data or HIGH_BOOTLOADER_READ_MEM_SRAM_END not in data:
        errors.append("high bootloader payload is missing the high-HID memory readback command")
    return errors


def validate_bootloader_updater_image(data: bytes) -> list[str]:
    _sp, _rv, errors = validate_vectors(data, APP_ADDRESS, app_slot=True)
    if APP_ADDRESS + padded_len(len(data)) > APP_SETTINGS_ADDRESS:
        errors.append("padded updater image would overlap the app settings sector")
    if BOOTLOADER_UPDATER_MARKER not in data:
        errors.append("updater image is missing the OpenScope bootloader updater marker")
    return errors


def validate_stock_saved_config_hole(data: bytes, address: int) -> list[str]:
    """Ensure an app-slot image leaves the stock settings page unprogrammed.

    The PC switcher can preserve only all-0xFF blocks.  If the OpenScope image
    puts code or data in 0x08006000..0x08007000, switching back to OpenScope
    destroys stock's saved settings before a later stock reflash can preserve
    them.
    """
    image_end = address + padded_len(len(data))
    overlap_start = max(address, STOCK_SAVED_CONFIG_ADDRESS)
    overlap_end = min(image_end, STOCK_SAVED_CONFIG_END_ADDRESS)
    if overlap_start >= overlap_end:
        return []

    start = overlap_start - address
    end = overlap_end - address
    if all(byte == 0xFF for byte in data[start:end]):
        return []

    return [
        "app image programs bytes inside the stock saved-settings preserve page "
        f"0x{STOCK_SAVED_CONFIG_ADDRESS:08X}..0x{STOCK_SAVED_CONFIG_END_ADDRESS:08X}"
    ]


def stock_false_scatter_candidates(data: bytes) -> list[tuple[int, int, int, int]]:
    """Return the old false-positive "scatter" pairs near the stock file tail.

    The bytes at file offset 0x0B7314 look like two Keil-style
    (src, dst, len, handler) records if decoded out of context.  Static review
    shows that they are embedded in a Unicode/codepoint tail table, and the
    decoded handlers land inside normal firmware logic rather than copy/zero
    helper entry points.  Keep exposing them so tests and docs can prevent this
    false blocker from coming back.
    """
    if len(data) < STOCK_FALSE_SCATTER_TABLE_OFFSET + STOCK_FALSE_SCATTER_TABLE_LENGTH:
        return []
    entries: list[tuple[int, int, int, int]] = []
    for offset in range(
        STOCK_FALSE_SCATTER_TABLE_OFFSET,
        STOCK_FALSE_SCATTER_TABLE_OFFSET + STOCK_FALSE_SCATTER_TABLE_LENGTH,
        16,
    ):
        src, dst, length, handler = struct.unpack_from("<IIII", data, offset)
        if src == 0 or length == 0:
            continue
        entries.append((src, dst, length, handler))
    return entries


def stock_literal_xrefs(data: bytes, targets: tuple[int, ...]) -> dict[int, list[int]]:
    """Find aligned little-endian word references to stock flash addresses."""
    refs: dict[int, list[int]] = {target: [] for target in targets}
    for target in targets:
        patterns = {target, target | 1, target & ~1}
        for value in patterns:
            needle = struct.pack("<I", value)
            start = 0
            while True:
                offset = data.find(needle, start)
                if offset < 0:
                    break
                if offset % 4 == 0:
                    refs[target].append(offset)
                start = offset + 1
        refs[target] = sorted(set(refs[target]))
    return refs


def validate_stock_scatter_sources(data: bytes, runtime_base: int = STOCK_RUNTIME_BASE) -> list[str]:
    """Deprecated compatibility shim.

    Earlier tooling treated stock bytes at 0x0B7314 as a Keil scatter table and
    rejected stock because the decoded source ranges extended past EOF.  That
    interpretation is now classified as a false positive, so this validator no
    longer emits errors.  Use validate_stock_boot_chain() for the real stock
    safety gate.
    """
    _ = (data, runtime_base)
    return []


def stock_boot_entry_inventory(data: bytes) -> list[dict[str, object]]:
    """Return stock V1.2.0 boot-entry candidates and their current verdicts.

    The downloaded APP looks like a vector-bearing Cortex image, but live trials
    and static review now disagree with the simple "vector[1] is reset handler"
    interpretation.  Keep this as structured data so safety gates and scripts
    can talk about the same evidence without reintroducing old entry myths.
    """
    inventory: list[dict[str, object]] = []
    try:
        sp, rv = read_vectors(data)
    except ValueError:
        return inventory

    vector_addr = rv & ~1
    vector_offset = vector_addr - FLASH_BASE
    reset_dispatch_anchor = (
        len(data) >= STOCK_RESET_DISPATCH_TABLE_OFFSET + 4
        and data[STOCK_RESET_DISPATCH_TABLE_OFFSET:STOCK_RESET_DISPATCH_TABLE_OFFSET + 4] == b"\xdf\xe8\x01\xf0"
    )
    reset_case_bytes = data[
        STOCK_RESET_DISPATCH_CASE_OFFSET:STOCK_RESET_DISPATCH_CASE_OFFSET + 12
    ] if len(data) >= STOCK_RESET_DISPATCH_CASE_OFFSET + 12 else b""
    inventory.append({
        "name": "vector_reset",
        "address": rv,
        "offset": vector_offset,
        "verdict": "not_proven_cold_start",
        "reason": (
            "vector[1] targets the 0x08007310 runtime dispatch/case body; "
            "nearby code contains a TBB dispatch anchor and the case body writes "
            "through registers that are not initialized by a Cortex reset fetch"
        ),
        "stack_pointer": sp,
        "dispatch_anchor_matches": reset_dispatch_anchor,
        "case_bytes": reset_case_bytes.hex(),
        "literal_reference_offsets": stock_literal_xrefs(data, (rv,)).get(rv, []),
    })

    if len(data) >= STOCK_NATIVE_ENTRY_LITERAL_OFFSET + 12:
        targets = struct.unpack_from("<III", data, STOCK_NATIVE_ENTRY_LITERAL_OFFSET)
        verdict = (
            "not_proven_cold_start"
            if targets == STOCK_NATIVE_ENTRY_LITERAL_TARGETS
            else "unknown"
        )
        inventory.append({
            "name": "native_entry_candidate_0x310",
            "address": STOCK_NATIVE_ENTRY_CANDIDATE | 1,
            "offset": STOCK_NATIVE_ENTRY_CANDIDATE_OFFSET,
            "verdict": verdict,
            "reason": (
                "file offset 0x310 contains a Keil native-base runtime entry "
                "candidate, but its literal targets require the stock body at "
                "0x08000000 and a still-missing handoff context; a live "
                "register-safe trace reached 0x0802A9C5 with an invalid "
                "context pointer, so this is not stock-equivalent reset yet"
            ),
            "literal_targets": targets,
            "literal_reference_offsets": stock_literal_xrefs(
                data,
                (STOCK_NATIVE_ENTRY_CANDIDATE, STOCK_NATIVE_ENTRY_CANDIDATE | 1),
            ),
            "literal_target_reference_offsets": stock_literal_xrefs(data, targets),
        })

    if len(data) >= STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET + 16:
        inventory.append({
            "name": "preamble_candidate_0x184",
            "address": STOCK_BOOT_PREAMBLE_CANDIDATE | 1,
            "offset": STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET,
            "verdict": "not_startup",
            "reason": (
                "live dispatcher trials proved this preamble can be reached, "
                "but software trace shows it consumes the old 0x080B7314 "
                "scatter-like records and branches into context-dependent "
                "ordinary firmware logic, so reachability is not boot proof"
            ),
            "first_bytes": data[
                STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET:STOCK_BOOT_PREAMBLE_CANDIDATE_OFFSET + 16
            ].hex(),
            "literal_reference_offsets": stock_literal_xrefs(
                data,
                (STOCK_BOOT_PREAMBLE_CANDIDATE, STOCK_BOOT_PREAMBLE_CANDIDATE | 1),
            ),
        })

    if len(data) >= STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET + 32:
        inventory.append({
            "name": "low_runtime_entry_candidate_0x2f0",
            "address": STOCK_LOW_RUNTIME_ENTRY_CANDIDATE | 1,
            "offset": STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET,
            "verdict": "plausible_trace_candidate",
            "reason": (
                "software trace from 0x080002F1 reaches stack setup, clock init, "
                "and master init at 0x08023A50 before failing only on unmodelled "
                "hardware state; this is the best current stock-launch trial "
                "candidate, but still lacks live stock UI/MSC proof"
            ),
            "first_bytes": data[
                STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET:STOCK_LOW_RUNTIME_ENTRY_CANDIDATE_OFFSET + 32
            ].hex(),
            "literal_reference_offsets": stock_literal_xrefs(
                data,
                (STOCK_LOW_RUNTIME_ENTRY_CANDIDATE, STOCK_LOW_RUNTIME_ENTRY_CANDIDATE | 1),
            ),
        })

    return inventory


def validate_stock_boot_chain(data: bytes) -> list[str]:
    errors: list[str] = []
    try:
        _sp, rv = read_vectors(data)
    except ValueError as exc:
        return [str(exc)]

    if rv == STOCK_RESET_VECTOR:
        errors.append(
            "stock reset vector 0x08007311 points into a runtime dispatcher/table region, "
            "not a proven cold-start entry"
        )
    if len(data) >= STOCK_NATIVE_ENTRY_LITERAL_OFFSET + 12:
        scatter_entry, libc_entry, main_entry = struct.unpack_from("<III", data, STOCK_NATIVE_ENTRY_LITERAL_OFFSET)
        if (scatter_entry, libc_entry, main_entry) == STOCK_NATIVE_ENTRY_LITERAL_TARGETS:
            errors.append(
                "stock Keil runtime entry candidate at file offset 0x310 uses native-base literal targets "
                "0x08033F7D/0x0802A9C5/0x08007185 and a missing handoff context; raw stock launch remains unproven"
            )
    return errors


def detect_usb_modes() -> list[tuple[str, str]]:
    modes: list[tuple[str, str]] = []
    sysfs = Path("/sys/bus/usb/devices")
    if sysfs.exists():
        for dev in sysfs.iterdir():
            try:
                vid = (dev / "idVendor").read_text(encoding="ascii").strip().lower()
                pid = (dev / "idProduct").read_text(encoding="ascii").strip().lower()
            except OSError:
                continue
            if vid == VID and pid in USB_MODES:
                modes.append((pid, USB_MODES[pid]))

    if not modes and shutil.which("lsusb"):
        try:
            out = subprocess.check_output(["lsusb"], text=True, stderr=subprocess.DEVNULL)
        except (OSError, subprocess.SubprocessError):
            out = ""
        for line in out.splitlines():
            marker = f"id {VID}:"
            if marker not in line.lower():
                continue
            pid = line.lower().split(marker, 1)[1][:4]
            if pid in USB_MODES:
                modes.append((pid, USB_MODES[pid]))

    return sorted(set(modes))


def print_image_report(label: str, path: Path, data: bytes, address: int, app_slot: bool) -> tuple[str, list[str]]:
    sp, rv, errors = validate_vectors(data, address, app_slot=app_slot)
    kind = classify_image(path, data, address)
    plen = padded_len(len(data))
    print(f"{label}:")
    print(f"  path: {path}")
    print(f"  kind: {kind}")
    print(f"  address: 0x{address:08X}")
    print(f"  size: {len(data)} bytes; padded: {plen} bytes")
    print(f"  sha256: {sha256(data)}")
    print(f"  stack pointer: 0x{sp:08X}")
    print(f"  reset vector:  0x{rv:08X}")
    return kind, errors


def require_usb_mode(expected_pid: str, allow_missing: bool, image_only: bool) -> list[str]:
    modes = detect_usb_modes()
    if modes:
        print("USB detected:")
        for pid, mode in modes:
            print(f"  {VID}:{pid} {mode}")
    else:
        print("USB detected: none")

    if image_only:
        return []
    if any(pid == expected_pid for pid, _mode in modes):
        return []
    if allow_missing and not modes:
        return []

    expected = USB_MODES[expected_pid]
    present = ", ".join(f"{VID}:{pid} {mode}" for pid, mode in modes) or "none"
    return [f"expected USB mode {expected} ({VID}:{expected_pid}), saw {present}"]


def preflight_hid_app(args: argparse.Namespace) -> int:
    errors: list[str] = []
    data = read_file(args.image)
    kind, image_errors = print_image_report("HID app image", args.image, data, args.address, True)
    errors.extend(image_errors)
    errors.extend(require_usb_mode(PID_HID_IAP, args.allow_missing_device, args.image_only))

    if args.address != APP_ADDRESS:
        errors.append("HID IAP app flashing is only allowed at 0x08004000")
    if args.address + padded_len(len(data)) > APP_SLOT_END_ADDRESS:
        errors.append("padded app image would overlap the high recovery bootloader region")
    errors.extend(validate_stock_saved_config_hole(data, args.address))
    if kind == "stock-app":
        errors.extend(validate_stock_boot_chain(data))
    if kind != "openscope-app" and not args.allow_unknown_app:
        errors.append(
            f"HID IAP only accepts OpenScope app images by default; got {kind}. "
            "Do not flash stock/vendor APP_2C53T images through HID IAP."
        )

    return finish(
        errors,
        "uv run ../scripts/hid_flash.py <image> --preserve-blank-blocks "
        "--preserve-blank-blocks-range 0x08006000:0x08007000",
    )


def preflight_rom_dfu_app(args: argparse.Namespace) -> int:
    errors: list[str] = []
    data = read_file(args.image)
    _kind, image_errors = print_image_report("ROM DFU app image", args.image, data, APP_ADDRESS, True)
    errors.extend(image_errors)
    errors.extend(require_usb_mode(PID_ROM_DFU, args.allow_missing_device, args.image_only))
    if classify_image(args.image, data, APP_ADDRESS) == "stock-app":
        errors.extend(validate_stock_boot_chain(data))
    if APP_ADDRESS + len(data) > APP_SLOT_END_ADDRESS:
        errors.append("app image would overlap the high recovery bootloader region")
    errors.extend(validate_stock_saved_config_hole(data, APP_ADDRESS))
    return finish(
        errors,
        "python3 ../scripts/at32_dfuse_write.py 0x08004000 <image> "
        "--preserve-blank-pages-range 0x08006000:0x08007000 --leave 0x08004000",
    )


def preflight_rom_dfu_bootloader(args: argparse.Namespace) -> int:
    errors: list[str] = []
    data = read_file(args.bootloader)
    _kind, image_errors = print_image_report("ROM DFU bootloader image", args.bootloader, data, FLASH_BASE, False)
    errors.extend(image_errors)
    errors.extend(require_usb_mode(PID_ROM_DFU, args.allow_missing_device, args.image_only))
    if len(data) > APP_ADDRESS - FLASH_BASE:
        errors.append("bootloader image exceeds the 16KB bootloader region")
    return finish(errors, "dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D <bootloader>")


def preflight_rom_dfu_all(args: argparse.Namespace) -> int:
    errors: list[str] = []
    boot = read_file(args.bootloader)
    app = read_file(args.image)
    if args.option_bytes is not None:
        option = read_file(args.option_bytes)
        print(f"Option bytes:\n  path: {args.option_bytes}\n  size: {len(option)} bytes")
        if len(option) != 48:
            errors.append("option-bytes blob must be exactly 48 bytes for this ROM DFU descriptor")
    _boot_kind, boot_errors = print_image_report("ROM DFU bootloader image", args.bootloader, boot, FLASH_BASE, False)
    _app_kind, app_errors = print_image_report("ROM DFU app image", args.image, app, APP_ADDRESS, True)
    errors.extend(boot_errors)
    errors.extend(app_errors)
    errors.extend(require_usb_mode(PID_ROM_DFU, args.allow_missing_device, args.image_only))
    if len(boot) > APP_ADDRESS - FLASH_BASE:
        errors.append("bootloader image exceeds the 16KB bootloader region")
    if APP_ADDRESS + len(app) > APP_SLOT_END_ADDRESS:
        errors.append("app image would overlap the high recovery bootloader region")
    errors.extend(validate_stock_saved_config_hole(app, APP_ADDRESS))
    return finish(
        errors,
        "dfu-util bootloader at 0x08000000, then at32_dfuse_write app at 0x08004000 "
        "with --leave 0x08004000",
    )


def preflight_rom_dfu_high_layout(args: argparse.Namespace) -> int:
    errors: list[str] = []
    stage0 = read_file(args.stage0)
    high_boot = read_file(args.high_bootloader)
    app = read_file(args.image)
    if args.option_bytes is not None:
        option = read_file(args.option_bytes)
        print(f"Option bytes:\n  path: {args.option_bytes}\n  size: {len(option)} bytes")
        if len(option) != 48:
            errors.append("option-bytes blob must be exactly 48 bytes for this ROM DFU descriptor")

    _stage0_kind, _stage0_report_errors = print_image_report(
        "ROM DFU stage0 image", args.stage0, stage0, FLASH_BASE, False
    )
    _high_kind, high_report_errors = print_image_report(
        "ROM DFU high recovery bootloader image",
        args.high_bootloader,
        high_boot,
        HIGH_BOOTLOADER_ADDRESS,
        True,
    )
    _app_kind, app_report_errors = print_image_report("ROM DFU app image", args.image, app, APP_ADDRESS, True)
    errors.extend(high_report_errors)
    errors.extend(app_report_errors)
    if _app_kind == "stock-app":
        errors.extend(validate_stock_boot_chain(app))
    errors.extend(validate_stage0_payload(stage0))
    errors.extend(validate_high_bootloader_payload(high_boot))
    errors.extend(require_usb_mode(PID_ROM_DFU, args.allow_missing_device, args.image_only))
    if APP_ADDRESS + len(app) > APP_SLOT_END_ADDRESS:
        errors.append("app image would overlap the high recovery bootloader region")
    errors.extend(validate_stock_saved_config_hole(app, APP_ADDRESS))

    return finish(
        errors,
        "dfu-util stage0 at 0x08000000, app at 0x08004000, high bootloader at 0x080F0000",
    )


def preflight_hid_bootloader_updater(args: argparse.Namespace) -> int:
    errors: list[str] = []
    boot = read_file(args.bootloader)
    updater = read_file(args.updater_image)

    _boot_kind, boot_report_errors = print_image_report(
        "Embedded bootloader payload", args.bootloader, boot, FLASH_BASE, False
    )
    _updater_kind, updater_report_errors = print_image_report(
        "HID bootloader-updater app image", args.updater_image, updater, APP_ADDRESS, True
    )
    errors.extend(boot_report_errors)
    errors.extend(updater_report_errors)
    errors.extend(validate_bootloader_payload(boot))
    errors.extend(validate_bootloader_updater_image(updater))
    errors.extend(require_usb_mode(PID_HID_IAP, args.allow_missing_device, args.image_only))

    return finish(
        errors,
        "uv run ../scripts/hid_flash.py firmware/bootloader_updater/build/bootloader_updater.bin",
    )


def finish(errors: list[str], intended_command: str) -> int:
    print(f"Intended flash command: {intended_command}")
    if errors:
        print("Preflight: FAIL", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        return 2
    print("Preflight: OK")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="OpenScope 2C53T flash safety preflight")
    sub = parser.add_subparsers(dest="mode", required=True)

    def add_device_arg(p: argparse.ArgumentParser) -> None:
        p.add_argument(
            "--allow-missing-device",
            action="store_true",
            help="allow image-only preflight when no 2e3c USB device is present",
        )
        p.add_argument(
            "--image-only",
            action="store_true",
            help="print image and detected-USB evidence but do not require the flash-mode USB device",
        )

    hid = sub.add_parser("hid-app", help="preflight custom HID IAP app-slot flash")
    hid.add_argument("--image", type=Path, required=True)
    hid.add_argument("--address", type=lambda s: int(s, 0), default=APP_ADDRESS)
    hid.add_argument("--allow-unknown-app", action="store_true")
    add_device_arg(hid)
    hid.set_defaults(func=preflight_hid_app)

    dfu_app = sub.add_parser("rom-dfu-app", help="preflight ROM DFU app-slot flash")
    dfu_app.add_argument("--image", type=Path, required=True)
    add_device_arg(dfu_app)
    dfu_app.set_defaults(func=preflight_rom_dfu_app)

    dfu_boot = sub.add_parser("rom-dfu-bootloader", help="preflight ROM DFU bootloader flash")
    dfu_boot.add_argument("--bootloader", type=Path, required=True)
    add_device_arg(dfu_boot)
    dfu_boot.set_defaults(func=preflight_rom_dfu_bootloader)

    dfu_all = sub.add_parser("rom-dfu-all", help="preflight ROM DFU bootloader + app flash")
    dfu_all.add_argument("--image", type=Path, required=True)
    dfu_all.add_argument("--bootloader", type=Path, required=True)
    dfu_all.add_argument("--option-bytes", type=Path)
    add_device_arg(dfu_all)
    dfu_all.set_defaults(func=preflight_rom_dfu_all)

    high_layout = sub.add_parser("rom-dfu-high-layout", help="preflight stage0 + high recovery bootloader + app flash")
    high_layout.add_argument("--stage0", type=Path, required=True)
    high_layout.add_argument("--high-bootloader", type=Path, required=True)
    high_layout.add_argument("--image", type=Path, required=True)
    high_layout.add_argument("--option-bytes", type=Path)
    add_device_arg(high_layout)
    high_layout.set_defaults(func=preflight_rom_dfu_high_layout)

    updater = sub.add_parser(
        "hid-bootloader-updater",
        help="preflight an app-slot updater that rewrites the custom HID bootloader",
    )
    updater.add_argument("--bootloader", type=Path, required=True)
    updater.add_argument("--updater-image", type=Path, required=True)
    add_device_arg(updater)
    updater.set_defaults(func=preflight_hid_bootloader_updater)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
