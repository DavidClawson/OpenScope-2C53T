#!/usr/bin/env python3
"""The RAM-resident installer must never branch into flash.

fwl_ram_install() (firmware/src/drivers/fw_loader.c) erases and reprograms
the app slot from RAM with interrupts off. Anything it calls must also be in
RAM: the flash it would otherwise jump to is the flash it is erasing.

That is exactly what went wrong in the maintainer-built v0.4.0 release
(EXP-57, EXP-59, EXP-62 A/B, issue #42): its compiler turned the 0xFF fill
after rf_w25q_read_raw() into an unguarded `bl memset` through a veneer
placed in .data, and newlib's memset sat in app-slot page 1. Page 0 and 1
erased, the next call fetched 0xFFFFFFFF and the unit hung with USB
enumerated but silent. Our GCC 15.3 -Os build inlined the fill, so the same
source worked. The attribute on RF now forbids the transformation; this test
proves, on the linked ELF, that no branch from a RAM function leaves RAM, so
a toolchain change cannot bring the bug back silently.

It builds `make guest` into its own BUILD_DIR (the layout fwapply installs),
then disassembles every function placed in SRAM. test_checker_catches_the_
release_pattern feeds the checker the exact shape of the bad code and
requires it to fail.

THE RAW .BIN CHECKER
--------------------
The installer that runs on `fwapply`/`fwswap` is the one inside the image the
unit is RUNNING, not the one being installed. So the question that matters
for a release is "is it safe to fwapply FROM this .bin?", and releases ship as
.bin with no ELF. check_raw_bin() answers it from the bytes alone:

  1. .data is found through Reset_Handler's literal pool (_sidata, _sdata,
     _edata). The triple is accepted only if the load image ends exactly at
     the end of the file, which is where objcopy puts .data -- so the RAM run
     address is derived, not assumed.
  2. The installer is found by its literal pool inside that image (AIRCR
     0x05FA0004, FLASH key 0x45670123 and a KEYR address, 0x40022004 or
     0x40022044, within 64 bytes), and its entry by the flash->RAM veneer that
     fw_loader_install_slot() reaches it through.
  3. Every `ldr.w pc, [pc]` veneer in the .data image whose literal points out
     of .data is flagged. On this memory map that is THE discriminating check:
     a `bl` reaches +-16 MB and flash is 0x18000000 away from SRAM, so a RAM
     function can only reach flash through such a veneer -- and the veneer
     itself sits in RAM, so the release's `bl 0x20000350` passes any
     "branch target in RAM?" test.
  4. Belt and braces: the installer and everything it calls is disassembled by
     recursive descent (never past a return or into a literal pool, so data
     is not decoded as code) and every branch must stay inside .data; ldr pc,
     register-indirect jumps and jump tables are refused outright.

It goes RED on openscope-2c53t-v0.4.0-coldtrace.bin (the veneer at file
0x098060 -> 0x08007C59) and GREEN on a fresh `make guest`. The release check
skips -- loudly, as missing coverage -- when that artefact is not on disk;
two synthetic mutations of the fresh guest image keep the RED path covered
everywhere. A .bin can be checked by hand:

  python3 scripts/test_ramfunc_isolated.py --check-bin path/to/image.bin
"""
from __future__ import annotations

import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIRMWARE = REPO / "firmware"
BUILD_DIR = "build-gate-ramfunc"
RAM_LO, RAM_HI = 0x20000000, 0x20040000

NM = "arm-none-eabi-nm"
OBJDUMP = "arm-none-eabi-objdump"

# objdump -d lines like "20000286:  f000 f863  bl  20000350 <__memset_veneer>"
BRANCH_RE = re.compile(
    r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+"
    r"(bl|blx|b|b\.w|b\.n|bx|cb[nz]z|b(?:eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)(?:\.[nw])?)\s+"
    r"(?:r\d+,\s*)?(?:0x)?([0-9a-f]+)(?:\s+<([^>]+)>)?",
    re.I)
LDR_PC_RE = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+ldr(?:\.w)?\s+pc\s*,", re.I)
BX_REG_RE = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s?){1,2}\s+(?:bx|blx)\s+(r\d+|ip)\b", re.I)


def check_disassembly(text: str) -> list:
    """Return a list of violations (strings) found in objdump -d output of a
    RAM-resident function. Empty list = isolated."""
    bad = []
    for line in text.splitlines():
        m = BRANCH_RE.match(line)
        if m:
            addr, mnem, target, sym = m.group(1), m.group(2).lower(), int(m.group(3), 16), m.group(4)
            if mnem == "bx" and target < 0x10:
                continue
            if not (RAM_LO <= target < RAM_HI):
                bad.append(f"{addr}: {mnem} to 0x{target:08X}{' <' + sym + '>' if sym else ''} (outside RAM)")
            elif sym and "veneer" in sym.lower():
                bad.append(f"{addr}: {mnem} through a veneer <{sym}> (a veneer in RAM only exists to leave RAM)")
            continue
        if LDR_PC_RE.match(line):
            bad.append(f"{LDR_PC_RE.match(line).group(1)}: ldr pc (computed jump: veneer shape)")
    return bad


def ram_functions(elf: Path) -> list:
    out = subprocess.run([NM, "-S", "--defined-only", str(elf)], capture_output=True, text=True, check=True).stdout
    funcs = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2] in ("t", "T"):
            addr = int(parts[0], 16)
            if RAM_LO <= addr < RAM_HI:
                funcs.append((parts[3], addr, int(parts[1], 16)))
    return funcs


def build_guest() -> Path:
    env = dict(os.environ)
    subprocess.run(["make", "guest", f"BUILD_DIR={BUILD_DIR}"], cwd=FIRMWARE, env=env,
                   capture_output=True, text=True, check=True)
    return FIRMWARE / BUILD_DIR / "firmware.elf"


_GUEST_ELF = None


def guest_elf() -> Path:
    """One `make guest` per run, shared by the ELF and the raw-.bin classes."""
    global _GUEST_ELF
    if _GUEST_ELF is None:
        for tool in (NM, OBJDUMP, "make"):
            if shutil.which(tool) is None:
                raise unittest.SkipTest(f"{tool} not found: cannot inspect the RAM installer "
                                        f"-- this is MISSING COVERAGE, not a pass")
        try:
            _GUEST_ELF = build_guest()
        except subprocess.CalledProcessError as e:
            raise AssertionError(f"make guest failed:\n{e.stdout[-2000:]}\n{e.stderr[-2000:]}")
    return _GUEST_ELF


# ── Raw .bin checker ───────────────────────────────────────────────────────

GUEST_BASE = 0x08007000          # fwapply installs here (FWL_APP_BASE)
FLASH_KEY1 = 0x45670123
AIRCR_SYSRESETREQ = 0x05FA0004
KEYR_ADDRS = (0x40022004, 0x40022044)   # bank 0 / bank 1; a compiler may keep only one
POOL_SPAN = 64
# ldr.w pc, [pc, #-0] and ldr.w pc, [pc, #0]: the long-branch stubs ld emits for
# Thumb-2. Both load the destination from the word that follows.
VENEER_OPCODES = (bytes.fromhex("5ff800f0"), bytes.fromhex("dff800f0"))


@dataclass
class RawBinReport:
    data_off: int = 0                # file offset of the .data load image
    sdata: int = 0
    edata: int = 0
    installer: int = 0               # RAM address of fwl_ram_install
    pool: int = 0                    # RAM address of the identifying literal pool
    functions: list = field(default_factory=list)   # RAM entries walked, in order
    walked: bool = False             # False when objdump is unavailable
    violations: list = field(default_factory=list)


def _u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def locate_data(b: bytes, base: int = GUEST_BASE):
    """(file offset, _sdata, _edata) of .data, from Reset_Handler's literal
    pool. Raises ValueError if no unique self-consistent triple exists."""
    if len(b) < 8:
        raise ValueError("file too short for a vector table")
    reset = (_u32(b, 4) & ~1) - base
    if not (0 <= reset < len(b)):
        raise ValueError(f"reset vector 0x{_u32(b, 4):08X} is outside an image based at 0x{base:08X}")
    lo = reset & ~3
    words = [_u32(b, o) for o in range(lo, min(lo + 0x80, len(b) - 3), 4)]
    found = set()
    for sidata in words:
        off = sidata - base
        if not (0 <= off < len(b)):
            continue
        for sdata in words:
            for edata in words:
                if (RAM_LO <= sdata <= edata < RAM_HI and sdata % 4 == 0
                        and off + (edata - sdata) == len(b)):
                    found.add((off, sdata, edata))
    if len(found) != 1:
        raise ValueError(f"cannot locate .data from Reset_Handler's pool at file 0x{reset:X} "
                         f"(candidates: {sorted(found)})")
    return found.pop()


def _find_installer_pool(img: bytes, sdata: int) -> int:
    """RAM address of the installer's literal pool: the FLASH key with the
    AIRCR reset value and a KEYR address within POOL_SPAN bytes of it."""
    pos = {}
    for o in range(0, len(img) - 3, 4):
        pos.setdefault(_u32(img, o), []).append(o)
    clusters = []
    for k in pos.get(FLASH_KEY1, []):
        aircr = [o for o in pos.get(AIRCR_SYSRESETREQ, []) if abs(o - k) <= POOL_SPAN]
        keyr = [o for v in KEYR_ADDRS for o in pos.get(v, []) if abs(o - k) <= POOL_SPAN]
        if aircr and keyr:
            clusters.append(min([k] + aircr + keyr))
    if len(clusters) != 1:
        raise ValueError(f"expected one installer literal pool in .data, found {len(clusters)}")
    return sdata + clusters[0]


def _ram_veneer_entries(b: bytes, data_off: int, sdata: int, edata: int) -> list:
    """Destinations of the flash-resident veneers that jump into .data."""
    out = []
    for op in VENEER_OPCODES:
        o = b.find(op, 0, data_off)
        while o >= 0:
            if o % 4 == 0 and o + 8 <= data_off:
                dest = _u32(b, o + 4)
                if dest & 1 and sdata <= (dest & ~1) < edata:
                    out.append(dest & ~1)
            o = b.find(op, o + 1, data_off)
    return sorted(set(out))


INSN_RE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} ?){1,2})\s*(.*)$")
HEX_RE = re.compile(r"0x([0-9a-f]+)")
TARGET_RE = re.compile(r"(?:^|,\s*)0x([0-9a-f]+)$")
COND = r"(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)"
BRANCH_MNEM = re.compile(r"(bl|blx|b|cbn?z)(" + COND + r")?(?:\.[nw])?")
BXREG_MNEM = re.compile(r"(bx|blx)(" + COND + r")?")
POP_MNEM = re.compile(r"(pop|ldm|ldmia|ldmfd)(" + COND + r")?(?:\.w)?")
LOAD_MNEM = re.compile(r"v?ldr(d|h|sh|b|sb)?(" + COND + r")?(?:\.w)?")
PCDST_MNEM = re.compile(r"(ldr|mov|add|sub)(" + COND + r")?(?:\.w)?")


class _Thumb:
    """objdump over a raw Thumb image (.data, at its run address). A decode
    from an address that an earlier run already passed through as an
    instruction boundary reuses that run: Thumb decoding is a function of the
    start address, so the suffix is the same."""

    def __init__(self, img: bytes, vma: int, tmpdir: str):
        self.img, self.vma = img, vma
        self.path = os.path.join(tmpdir, "data.bin")
        with open(self.path, "wb") as f:
            f.write(img)
        self.index = {}

    def decode(self, start: int, stop: int) -> list:
        hit = self.index.get(start)
        if hit is not None:
            run, i = hit
            return run[i:]
        out = subprocess.run(
            [OBJDUMP, "-D", "-z", "-b", "binary", "-marm", "-Mforce-thumb",
             f"--adjust-vma=0x{self.vma:x}", f"--start-address=0x{start:x}",
             f"--stop-address=0x{stop:x}", self.path],
            capture_output=True, text=True, check=True).stdout
        run = []
        for line in out.splitlines():
            m = INSN_RE.match(line)
            if not m:
                continue
            text, _, comment = m.group(3).replace("\t", " ").partition("@")
            parts = text.split(None, 1)
            run.append((int(m.group(1), 16), 2 * len(m.group(2).split()),
                        parts[0].lower() if parts else "",
                        parts[1].strip() if len(parts) > 1 else "", comment.strip()))
        for i, insn in enumerate(run):
            self.index.setdefault(insn[0], (run, i))
        return run

    def word(self, addr: int):
        o = addr - self.vma
        return _u32(self.img, o) if 0 <= o <= len(self.img) - 4 else None


def _walk(thumb: _Thumb, entry: int, lo: int, hi: int, file_of, via, bad: list):
    """Recursive-descent walk of the function at `entry`: it follows every
    branch, stops at returns and unconditional jumps, and never decodes a
    literal pool (the words loaded pc-relative), so data is not mistaken for
    code. Appends violations to `bad`; returns ({callee: call site}, literal
    addresses loaded)."""
    callees, seen, lits = {}, set(), set()
    work = [entry]
    while work:
        start = work.pop()
        if start in seen:
            continue
        for addr, size, mnem, ops, comment in thumb.decode(start, min(hi, start + 0x1000)):
            if addr in seen or any(a in lits for a in range(addr, addr + size)):
                break                       # joined code already walked, or ran into a pool
            seen.add(addr)
            where = f"0x{addr:08X} (file 0x{file_of(addr):06X})"
            if addr == entry and via is not None:
                where += f", called from 0x{via:08X}"
            if not mnem or "?" in mnem:
                bad.append(f"{where}: undecodable instruction on a reachable path")
                break
            ld = LOAD_MNEM.fullmatch(mnem)
            if ld and "[pc" in ops and (h := HEX_RE.search(comment)):
                lit = int(h.group(1), 16)
                width = {"d": 8, "h": 2, "sh": 2, "b": 1, "sb": 1}.get(ld.group(1), 4)
                if mnem.startswith("v"):
                    width = 8 if ops.startswith("d") else 4
                lits.update(range(lit, lit + width))
            dst = ops.split(",", 1)[0].strip()

            if (pc := PCDST_MNEM.fullmatch(mnem)) and dst == "pc":
                flat = ops.replace(" ", "")
                if pc.group(1) == "ldr" and flat.startswith("pc,[sp]"):
                    pass                    # ldr.w pc, [sp], #4: a one-register pop
                elif pc.group(1) == "mov" and flat == "pc,lr":
                    pass                    # return
                elif pc.group(1) == "ldr" and flat.startswith("pc,[pc"):
                    h = HEX_RE.search(comment)
                    dest = thumb.word(int(h.group(1), 16)) if h else None
                    tail = f" -> 0x{dest:08X}" if dest is not None else ""
                    bad.append(f"{where}: ldr.w pc, [pc] veneer{tail} (a veneer in RAM only "
                               f"exists to leave RAM -- the v0.4.0 hang)")
                else:
                    bad.append(f"{where}: {mnem} {ops} (computed jump: cannot be shown to stay in RAM)")
                if pc.group(2) is None:
                    break
                continue
            if mnem.startswith(("tbb", "tbh")):
                bad.append(f"{where}: {mnem} {ops} (jump table: targets not followed)")
                break
            if (bx := BXREG_MNEM.fullmatch(mnem)) and not TARGET_RE.search(ops):
                if ops != "lr" or bx.group(1) == "blx":
                    bad.append(f"{where}: {mnem} {ops} (indirect jump)")
                if bx.group(1) == "bx" and bx.group(2) is None:
                    break                   # unconditional bx (bx lr = return)
                continue
            if (pop := POP_MNEM.fullmatch(mnem)) and "pc" in ops:
                if not (pop.group(1) == "pop" or dst == "sp!"):
                    bad.append(f"{where}: {mnem} {ops} (computed jump)")
                if pop.group(2) is None:
                    break                   # unconditional return
                continue
            br = BRANCH_MNEM.fullmatch(mnem)
            t = TARGET_RE.search(ops)
            if br and t:
                target, kind = int(t.group(1), 16), br.group(1)
                if not (lo <= target < hi):
                    bad.append(f"{where}: {mnem} to 0x{target:08X} (outside .data "
                               f"0x{lo:08X}-0x{hi:08X}: leaves the RAM image)")
                elif kind == "blx":
                    bad.append(f"{where}: blx to 0x{target:08X} (switches to ARM state)")
                elif kind == "bl":
                    callees.setdefault(target, addr)
                else:
                    work.append(target)
                if kind == "b" and br.group(2) is None:
                    break                   # unconditional: what follows is not this path
    return callees, lits


def check_raw_bin(b: bytes, base: int = GUEST_BASE) -> RawBinReport:
    """Static check of a guest .bin's RAM installer. Raises ValueError when
    the installer cannot be located: a refusal, never a pass."""
    r = RawBinReport()
    r.data_off, r.sdata, r.edata = locate_data(b, base)
    img = b[r.data_off:]
    r.pool = _find_installer_pool(img, r.sdata)

    def file_of(addr):
        return r.data_off + addr - r.sdata

    for o in range(0, len(img) - 7, 4):
        if img[o:o + 4] in VENEER_OPCODES:
            dest = _u32(img, o + 4)
            if not (r.sdata <= (dest & ~1) < r.edata):
                r.violations.append(
                    f"0x{r.sdata + o:08X} (file 0x{r.data_off + o:06X}): ldr.w pc, [pc] veneer in "
                    f".data -> 0x{dest:08X} (RAM code calling out of the RAM image)")

    entries = [e for e in _ram_veneer_entries(b, r.data_off, r.sdata, r.edata) if e < r.pool]
    if not entries:
        raise ValueError("no flash->RAM veneer reaches code below the installer pool: "
                         "cannot find fwl_ram_install's entry")
    r.installer = max(entries)

    if shutil.which(OBJDUMP):           # without it: steps 1-3 only, and r.walked says so
        with tempfile.TemporaryDirectory() as tmp:
            thumb = _Thumb(img, r.sdata, tmp)
            via, todo = {r.installer: None}, [r.installer]
            while todo:
                fn = todo.pop(0)
                r.functions.append(fn)
                callees, lits = _walk(thumb, fn, r.sdata, r.edata, file_of, via[fn], r.violations)
                if fn == r.installer and not any(r.pool <= a < r.pool + POOL_SPAN + 4 for a in lits):
                    raise ValueError(f"code at 0x{r.installer:08X} does not load from the installer "
                                     f"pool at 0x{r.pool:08X}: wrong entry")
                for target, site in sorted(callees.items()):
                    if target not in via:
                        via[target] = site
                        todo.append(target)
        r.walked = True
    return r


def thumb_bl(src: int, dst: int) -> bytes:
    """Encode `bl dst` at address src (Thumb-2 T1)."""
    off = dst - (src + 4)
    assert -(1 << 24) <= off < (1 << 24) and off % 2 == 0
    s = (off >> 24) & 1
    j1 = (~((off >> 23) & 1) ^ s) & 1
    j2 = (~((off >> 22) & 1) ^ s) & 1
    return struct.pack("<HH", 0xF000 | (s << 10) | ((off >> 12) & 0x3FF),
                       0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF))


RELEASE_NAME = "openscope-2c53t-v0.4.0-coldtrace.bin"
RELEASE_SHA256 = "3089ddaea2ef0306e687338ddbd633aba16bf9e3d9737ccc668f87a2b354884a"


def release_bin():
    """The v0.4.0 release, via $OPENSCOPE_RELEASE_BIN or a bin/ directory
    beside any parent of the checkout. None if absent."""
    env = os.environ.get("OPENSCOPE_RELEASE_BIN")
    candidates = [Path(env)] if env else [p / "bin" / RELEASE_NAME for p in (REPO, *REPO.parents)]
    return next((p for p in candidates if p.is_file()), None)


class TestRamfuncIsolated(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.elf = guest_elf()

    def test_installer_is_present_in_ram(self):
        names = {n for n, _, _ in ram_functions(self.elf)}
        for expected in ("fwl_ram_install", "rf_w25q_read_raw", "rf_spi2_xfer"):
            self.assertIn(expected, names, f"{expected} is not in SRAM: the RF section attribute is gone")

    def test_no_branch_leaves_ram(self):
        funcs = ram_functions(self.elf)
        self.assertTrue(funcs)
        for name, addr, size in funcs:
            with self.subTest(function=name):
                self.assertNotIn("veneer", name.lower(),
                                 f"{name} at 0x{addr:08X}: a veneer linked into RAM means a RAM "
                                 f"function calls out to flash — exactly the v0.4.0 hang")
                dis = subprocess.run([OBJDUMP, "-d", f"--disassemble={name}", str(self.elf)],
                                     capture_output=True, text=True, check=True).stdout
                self.assertIn(f"<{name}>:", dis, f"objdump did not disassemble {name}")
                bad = check_disassembly(dis)
                self.assertEqual(bad, [], f"{name} leaves RAM:\n  " + "\n  ".join(bad))
                for m in BX_REG_RE.finditer(dis):
                    # bx lr is the return; any other register-indirect jump is suspicious
                    self.assertEqual(m.group(1).lower(), "lr", f"{name}: indirect jump via {m.group(1)}")

    def test_checker_catches_the_release_pattern(self):
        """The exact shape found in openscope-2c53t-v0.4.0-coldtrace.bin."""
        release_like = (
            "20000280:\t4b30      \tldr\tr3, [pc, #192]\n"
            "20000286:\tf000 f863 \tbl\t20000350 <__memset_veneer>\n"
            "20000350:\tf85f f000 \tldr.w\tpc, [pc]\t; 20000354\n"
        )
        bad = check_disassembly(release_like)
        self.assertEqual(len(bad), 2, bad)
        direct = "20000286:\tf007 fce7 \tbl\t8007c58 <memset>\n"
        self.assertEqual(len(check_disassembly(direct)), 1)
        fine = ("200001ec:\tf7ff ff50 \tbl\t20000090 <rf_spi2_xfer>\n"
                "20000230:\td1f9      \tbne.n\t20000226 <fwl_ram_install+0x116>\n"
                "20000340:\t4770      \tbx\tlr\n")
        self.assertEqual(check_disassembly(fine), [])


class TestRawBinGuest(unittest.TestCase):
    """check_raw_bin() on a fresh `make guest`, with the ELF as ground truth."""

    @classmethod
    def setUpClass(cls):
        elf = guest_elf()
        cls.bin = (elf.parent / "firmware.bin").read_bytes()
        out = subprocess.run([NM, str(elf)], capture_output=True, text=True, check=True).stdout
        cls.sym = {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
        cls.report = check_raw_bin(cls.bin)

    def test_locates_data_and_installer_like_the_elf(self):
        r, s = self.report, self.sym
        self.assertEqual((r.data_off, r.sdata, r.edata), (s["_sidata"] - GUEST_BASE, s["_sdata"], s["_edata"]))
        self.assertEqual(r.installer, s["fwl_ram_install"])
        self.assertTrue(r.walked)
        self.assertEqual(set(r.functions),
                         {s["fwl_ram_install"], s["rf_w25q_read_raw"], s["rf_spi2_xfer"]})

    def test_fresh_guest_is_green(self):
        bad = self.report.violations
        self.assertEqual(bad, [], "installer leaves RAM:\n  " + "\n  ".join(bad))

    def _patched(self, *patches) -> bytes:
        """The fresh image with (RAM address, bytes) pairs written over it."""
        r, b = self.report, bytearray(self.bin)
        for addr, data in patches:
            o = r.data_off + addr - r.sdata
            b[o:o + len(data)] = data
        return bytes(b)

    def _first_call(self) -> int:
        """Address of the installer's first `bl` (its call to rf_w25q_read_raw)."""
        r = self.report
        with tempfile.TemporaryDirectory() as tmp:
            run = _Thumb(self.bin[r.data_off:], r.sdata, tmp).decode(r.installer, r.pool)
        return next(a for a, _, m, _, _ in run if m == "bl")

    def test_planted_release_veneer_goes_red(self):
        """Rebuild the release's defect inside the fresh image: a veneer to
        0x08007C59 in .data and the installer's call retargeted onto it.
        Both the veneer scan and the branch walk must fire."""
        r, site = self.report, self._first_call()
        ven = r.edata - 8
        bad = check_raw_bin(self._patched(
            (ven, VENEER_OPCODES[0] + struct.pack("<I", 0x08007C59)),
            (site, thumb_bl(site, ven)))).violations
        self.assertEqual(len(bad), 2, bad)
        self.assertIn(f"0x{ven:08X} (file 0x{r.data_off + ven - r.sdata:06X}): ldr.w pc, [pc] veneer "
                      f"in .data -> 0x08007C59", bad[0])
        self.assertIn(f"called from 0x{site:08X}: ldr.w pc, [pc] veneer -> 0x08007C59", bad[1])

    def test_call_into_bss_goes_red(self):
        """No veneer, just a `bl` past _edata (into .bss): the branch walk
        alone must catch it."""
        r, site = self.report, self._first_call()
        bad = check_raw_bin(self._patched((site, thumb_bl(site, r.edata + 0x100)))).violations
        self.assertEqual(len(bad), 1, bad)
        self.assertIn(f"bl to 0x{r.edata + 0x100:08X}", bad[0])
        self.assertIn("leaves the RAM image", bad[0])


class TestRawBinRelease(unittest.TestCase):
    """The artefact behind EXP-57/59/62: the checker must call it unsafe."""

    @classmethod
    def setUpClass(cls):
        path = release_bin()
        if path is None:
            raise unittest.SkipTest(
                f"{RELEASE_NAME} not found (set OPENSCOPE_RELEASE_BIN, or put it in a bin/ directory "
                f"beside the checkout) -- this is MISSING COVERAGE of the RED case, not a pass")
        cls.bin = path.read_bytes()
        digest = hashlib.sha256(cls.bin).hexdigest()
        if digest != RELEASE_SHA256:
            raise unittest.SkipTest(f"{path} is not the v0.4.0 release (sha256 {digest[:16]}...) "
                                    f"-- this is MISSING COVERAGE of the RED case, not a pass")

    def test_release_is_red(self):
        """Issue #42's evidence, found from the bytes: .data at file 0x097D10,
        the installer at 0x20000148, its `bl` at 0x20000286 into the veneer at
        0x20000350 (file 0x098060) that jumps to newlib's memset in page 1."""
        r = check_raw_bin(self.bin)
        self.assertEqual((r.data_off, r.sdata, r.edata), (0x097D10, 0x20000000, 0x2000045C))
        self.assertEqual(r.installer, 0x20000148)
        expected = ["0x20000350 (file 0x098060): ldr.w pc, [pc] veneer in .data -> 0x08007C59"]
        if r.walked:
            expected.append("0x20000350 (file 0x098060), called from 0x20000286: "
                            "ldr.w pc, [pc] veneer -> 0x08007C59")
        self.assertEqual(len(r.violations), len(expected), r.violations)
        for line, prefix in zip(r.violations, expected):
            self.assertTrue(line.startswith(prefix), (line, prefix))


def main_check_bin(paths) -> int:
    worst = 0
    for p in paths:
        try:
            r = check_raw_bin(Path(p).read_bytes())
        except ValueError as e:
            print(f"{p}: CANNOT CHECK -- {e}")
            worst = max(worst, 2)
            continue
        verdict = "UNSAFE: do not fwapply/fwswap while running this image" if r.violations else "ok"
        print(f"{p}: {verdict}  (.data file 0x{r.data_off:06X} -> 0x{r.sdata:08X}-0x{r.edata:08X}, "
              f"installer 0x{r.installer:08X}, {len(r.functions)} RAM functions walked"
              f"{'' if r.walked else ', NOT walked: no objdump'})")
        for v in r.violations:
            print(f"  {v}")
        worst = max(worst, 1 if r.violations else 0)
    return worst


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--check-bin":
        sys.exit(main_check_bin(sys.argv[2:]))
    unittest.main(verbosity=2)
