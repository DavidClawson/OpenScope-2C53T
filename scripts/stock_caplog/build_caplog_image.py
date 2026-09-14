#!/usr/bin/env python3
"""Build the stock-caplog image: the user's own stock V1.2.0 APP + two hooks + a
code-cave logger that records every USART2 frame to the meter SoC and every GPIO
change, into SRAM that survives a MENU+Power round trip.

    python3 scripts/stock_caplog/build_caplog_image.py
    python3 scripts/stock_caplog/build_caplog_image.py --stock path/to/APP_2C53T_V1.2.0_251015.bin

Nothing stock is redistributed: the script reads the stock image you already
have (default: archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin, which
is gitignored) and refuses any other version by sha256, because the two hook
addresses and the six bytes each hook displaces are V1.2.0 facts.

What it does:
  1. compiles caplog.c + stubs.S with arm-none-eabi-gcc (-mgeneral-regs-only,
     no libc) and links them at 0x080BE700 with cave.ld, which asserts that the
     cave has no .data and no .bss;
  2. verifies the stock image (sha256, length, the six original bytes at each
     hook), then overwrites each hook with `b.w <stub>; nop` -- 12 bytes changed
     in the original image, nothing else;
  3. pads the image with 0xFF to the cave address and appends the cave.

The result flashes like any stock APP image (factory IAP: MENU+Power volume,
scripts/iap_flash.py). Read the block back after the run with
scripts/stock_caplog/caplog_dump.py. See docs/stock_caplog.md for the procedure
and what the hooks proved so far.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
DEFAULT_STOCK = ROOT / "archive" / "2C53T Firmware V1.2.0" / "APP_2C53T_V1.2.0_251015.bin"
DEFAULT_OUT = ROOT / "firmware" / "build" / "APP_2C53T_V1.2.0_caplog.bin"

APP_BASE = 0x08007000
CLEAN_SHA256 = "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760"
CLEAN_LEN = 0xB7680
CAVE_ADDR = 0x080BE700          # first 0x100-aligned address past the image, same 2 KB page
CAVE_MAX = 0x900                # cave.ld MEMORY length; the page ends at 0x080BF000
SRAM_BLOCK = 0x20037000
SRAM_BLOCK_LEN = 0x700

# (runtime address, the six original bytes, the stub that replays them)
HOOKS = (
    (0x0803E44C, bytes.fromhex("0a2002f09fff"), "caplog_stub_tx"),    # movs r0,#10; bl vTaskDelay
    (0x0802A994, bytes.fromhex("80b54ff01000"), "caplog_stub_tick"),  # push {r7,lr}; mov.w r0,#16
)

CFLAGS = [
    "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft", "-mgeneral-regs-only",
    "-Os", "-Wall", "-Wextra", "-ffreestanding", "-fno-builtin", "-nostdlib",
    "-fno-tree-loop-distribute-patterns", "-fno-stack-protector",
    "-fno-asynchronous-unwind-tables", "-fomit-frame-pointer",
]

TOOLS = ("gcc", "ld", "objcopy", "nm")


class CaplogError(RuntimeError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# --- Thumb-2 B.W (encoding T4) ------------------------------------------------

def enc_bw(src: int, dst: int) -> bytes:
    """`b.w dst` placed at src. Both are halfword-aligned code addresses."""
    off = dst - (src + 4)
    if not -(1 << 24) <= off < (1 << 24) or off % 2:
        raise CaplogError("b.w offset out of range or odd: %#x" % off)
    s = (off >> 24) & 1
    i1 = (off >> 23) & 1
    i2 = (off >> 22) & 1
    j1 = (~i1 & 1) ^ s
    j2 = (~i2 & 1) ^ s
    imm10 = (off >> 12) & 0x3FF
    imm11 = (off >> 1) & 0x7FF
    hw1 = 0xF000 | (s << 10) | imm10
    hw2 = 0x9000 | (j1 << 13) | (j2 << 11) | imm11
    return struct.pack("<HH", hw1, hw2)


def dec_bw(src: int, code: bytes) -> int:
    """Inverse of enc_bw: the target of the `b.w` at src."""
    hw1, hw2 = struct.unpack("<HH", code[:4])
    if (hw1 & 0xF800) != 0xF000 or (hw2 & 0xD000) != 0x9000:
        raise CaplogError("not a b.w T4 encoding: %04x %04x" % (hw1, hw2))
    s = (hw1 >> 10) & 1
    j1 = (hw2 >> 13) & 1
    j2 = (hw2 >> 11) & 1
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm10 = hw1 & 0x3FF
    imm11 = hw2 & 0x7FF
    off = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    if s:
        off -= 1 << 25
    return src + 4 + off


# --- the stock image ------------------------------------------------------------

def validate_stock(stock: bytes) -> None:
    got = sha256(stock)
    if got != CLEAN_SHA256:
        raise CaplogError(
            "stock image sha256 %s is not V1.2.0 (%s); the hook addresses are "
            "V1.2.0 facts, refusing" % (got, CLEAN_SHA256))
    if len(stock) != CLEAN_LEN:
        raise CaplogError("stock image is %#x bytes, expected %#x" % (len(stock), CLEAN_LEN))
    for addr, orig, _ in HOOKS:
        off = addr - APP_BASE
        if stock[off:off + 6] != orig:
            raise CaplogError("hook %#010x: found %s, expected %s"
                              % (addr, stock[off:off + 6].hex(), orig.hex()))


# --- the cave --------------------------------------------------------------------

def find_tools(toolchain_dir: Path | None) -> dict[str, str]:
    tools = {}
    for t in TOOLS:
        name = "arm-none-eabi-" + t
        path = str(toolchain_dir / name) if toolchain_dir else shutil.which(name)
        if not path or not os.path.exists(path):
            raise CaplogError("%s not found (%s)" % (name, "on PATH" if not toolchain_dir else toolchain_dir))
        tools[t] = path
    return tools


def run(cmd: list[str]) -> str:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise CaplogError("%s failed:\n%s%s" % (cmd[0], proc.stdout, proc.stderr))
    return proc.stdout


def build_cave(tools: dict[str, str], build_dir: Path) -> tuple[bytes, dict[str, int]]:
    """Compile and link the cave. Returns (cave bytes, {symbol: address})."""
    build_dir.mkdir(parents=True, exist_ok=True)
    run([tools["gcc"], *CFLAGS, "-c", str(HERE / "caplog.c"), "-o", str(build_dir / "caplog.o")])
    run([tools["gcc"], *CFLAGS, "-c", str(HERE / "stubs.S"), "-o", str(build_dir / "stubs.o")])
    run([tools["ld"], "-T", str(HERE / "cave.ld"), "-nostdlib", "-o", str(build_dir / "cave.elf"),
         str(build_dir / "stubs.o"), str(build_dir / "caplog.o")])
    run([tools["objcopy"], "-O", "binary", "-j", ".text", str(build_dir / "cave.elf"), str(build_dir / "cave.bin")])
    syms = {}
    for line in run([tools["nm"], "-n", str(build_dir / "cave.elf")]).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2].startswith("caplog_"):
            syms[parts[2]] = int(parts[0], 16)
    cave = (build_dir / "cave.bin").read_bytes()
    if len(cave) > CAVE_MAX:
        raise CaplogError("cave is %d bytes, the page has room for %d" % (len(cave), CAVE_MAX))
    for _, _, stub in HOOKS:
        if stub not in syms:
            raise CaplogError("stub %s missing from the cave" % stub)
    return cave, syms


# --- the patch -------------------------------------------------------------------

def patch_image(stock: bytes, cave: bytes, syms: dict[str, int]) -> bytes:
    validate_stock(stock)
    img = bytearray(stock)
    for addr, _, stub in HOOKS:
        off = addr - APP_BASE
        img[off:off + 4] = enc_bw(addr, syms[stub])
        img[off + 4:off + 6] = b"\x00\xbf"  # nop
    img += b"\xff" * (CAVE_ADDR - APP_BASE - len(img))
    img += cave
    return bytes(img)


def describe(stock: bytes, img: bytes, syms: dict[str, int]) -> str:
    changed = sum(1 for a, b in zip(stock, img) if a != b)
    lines = [
        "image: %d bytes, sha256 %s" % (len(img), sha256(img)),
        "  bytes changed inside the original image: %d (expected 12), appended: %d"
        % (changed, len(img) - len(stock)),
    ]
    for addr, _, stub in HOOKS:
        lines.append("  hook %#010x -> %s @ %#010x" % (addr, stub, syms[stub]))
    lines.append("  SRAM block %#010x..%#010x, read back with: mem read %08X 64 (x%d)"
                 % (SRAM_BLOCK, SRAM_BLOCK + SRAM_BLOCK_LEN, SRAM_BLOCK, SRAM_BLOCK_LEN // 256))
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--stock", type=Path, default=DEFAULT_STOCK, help="clean stock V1.2.0 APP image")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="patched image to write")
    ap.add_argument("--toolchain", type=Path, default=None, help="directory holding arm-none-eabi-*")
    ap.add_argument("--build-dir", type=Path, default=None, help="keep cave.elf/.bin/.o here (default: temp)")
    args = ap.parse_args(argv)

    try:
        if not args.stock.exists():
            raise CaplogError("stock image not found: %s" % args.stock)
        stock = args.stock.read_bytes()
        validate_stock(stock)
        tools = find_tools(args.toolchain)
        if args.build_dir:
            cave, syms = build_cave(tools, args.build_dir)
        else:
            with tempfile.TemporaryDirectory() as tmp:
                cave, syms = build_cave(tools, Path(tmp))
        img = patch_image(stock, cave, syms)
    except CaplogError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(img)
    print("wrote %s" % args.out)
    print(describe(stock, img, syms))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
