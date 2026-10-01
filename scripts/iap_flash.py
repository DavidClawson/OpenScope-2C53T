#!/usr/bin/env python3
"""
iap_flash — flash the FNIRSI 2C53T over its stock USB IAP (upgrade-mode) channel.

The 2C53T's resident bootloader exposes a FAT12 "IAP" volume when you hold
MENU + tap Power. Dropping an APP_*.bin onto it makes the bootloader flash and
reboot. On macOS, Finder drag-drop FAILS — the OS FAT driver mishandles the
volume's 2048-byte logical sectors and litters AppleDouble (._*) junk the
bootloader scans as firmware.

This tool writes the image the way the bootloader expects, bypassing the OS FAT
driver. On macOS the volume has three quirks that each forced a design choice:
  - the raw char device (/dev/rdiskN) rejects mtools' sub-block reads;
  - the buffered block device (/dev/diskN) never flushes writes to the USB
    target (macOS has no `blockdev --flushbufs`);
  - the volume is a write-only emulator (data sectors read back as zeros).
So on macOS we snapshot the volume, inject the file into the snapshot with
mtools, diff to find the changed 2048-byte sectors, and write ONLY those to the
raw device — the same minimal "dir + FAT + file data" write that `mcopy -i
/dev/sdX` performs directly on Linux. (Writing the whole volume corrupts the
flash; writing through the buffered device never reaches it.)

Subcommands:
    status            Show the connected device's USB mode and guidance.
    list              List detected device + available firmware images.
    flash [IMAGE]     Flash an image (interactive picker if IMAGE omitted).
    doctor            Check prerequisites (mtools, platform support).
    guide             Print the full enter-upgrade-mode / recovery walkthrough.

Run with no subcommand for the interactive flow (detect → pick → confirm → flash).

Requires: mtools (`brew install mtools` / `apt install mtools`).
Supports: macOS (diskutil) and Linux (lsblk). Windows: not yet.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

# ───────────────────────── constants ─────────────────────────

REPO_ROOT = Path(__file__).resolve().parent.parent

# USB identities the device presents in its three modes.
USB_IAP      = ("2e3c", "5720")   # upgrade mode — FAT12 "IAP" volume (flashable)
USB_RUNNING  = ("2e3c", "5740")   # normal app running — CDC shell (NOT flashable)
USB_ROM_DFU  = ("2e3c", "df11")   # ROM DFU (BOOT0+reset) — use dfu-util, not this

MACOS = platform.system() == "Darwin"  # raw-disk writes need root on macOS

IAP_VOLUME_LABEL = "IAP"
MARKER_READY     = "READY.TXT"
MARKER_SUCCESS   = "SUCCESS.TXT"

# Known-good images, keyed by sha256, for friendly labels + integrity hints.
KNOWN_SHA = {
    "a17c5c35c97bb898f15672a1747bc1041d8ed507c16999ddba0d1e4e2ec0c760":
        "Stock firmware V1.2.0 (verified)",
}

# Where to look for flashable app images, relative to the repo root.
IMAGE_SEARCH = [
    "firmware/build",                 # OpenScope builds
    "archive",                        # archived stock + subfolders
]
# Filename substrings to exclude (not app images): SPI-flash dumps, test/aux bins.
IMAGE_EXCLUDE = ("w25q", "dump", "option_bytes", "bootsector", "test_", "payload")

FLASH_TIMEOUT_S = 40

# The app slot the bootloader writes: 0x08007000..0x080C0000, the same ceiling
# firmware/src/drivers/fw_loader.c enforces (0x080C0000 up is the 2C23T port's
# store). Stock V1.2.0 is 751 232 B, 6.5 KB under it.
APP_SLOT_BASE    = 0x08007000
APP_SLOT_CEILING = 0x080C0000
APP_SLOT_MAX     = APP_SLOT_CEILING - APP_SLOT_BASE   # 757 760 B = 740 KB

# dd/mtools errors that mean the IAP disk left the bus under a write.
VANISH_ERRORS = ("not configured", "no such file", "no such device",
                 "i/o error", "input/output error")

# ───────────────────────── tiny UI kit ─────────────────────────

class C:
    """ANSI colours, auto-disabled when stdout isn't a TTY or NO_COLOR is set."""
    _on = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None
    RESET  = "\033[0m"  if _on else ""
    BOLD   = "\033[1m"  if _on else ""
    DIM    = "\033[2m"  if _on else ""
    RED    = "\033[31m" if _on else ""
    GREEN  = "\033[32m" if _on else ""
    YELLOW = "\033[33m" if _on else ""
    BLUE   = "\033[34m" if _on else ""
    CYAN   = "\033[36m" if _on else ""

def hdr(s):  print(f"\n{C.BOLD}{C.CYAN}{s}{C.RESET}")
def ok(s):   print(f"  {C.GREEN}✓{C.RESET} {s}")
def warn(s): print(f"  {C.YELLOW}!{C.RESET} {s}")
def err(s):  print(f"  {C.RED}✗{C.RESET} {s}")
def info(s): print(f"    {C.DIM}{s}{C.RESET}")

SPINNER = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"

class Progress:
    """A single-line progress bar that animates 0→cap on a timer thread and
    snaps to 100% when finish() is called. Honest enough for opaque steps
    (mcopy / device-side flashing) that give us no real byte callback."""
    def __init__(self, label, est_seconds, cap=0.95, width=28):
        self.label, self.est, self.cap, self.width = label, est_seconds, cap, width
        self._stop = threading.Event()
        self._done = False
        self._t = None

    def _draw(self, frac, spin):
        filled = int(frac * self.width)
        bar = "█" * filled + "░" * (self.width - filled)
        c = C.GREEN if self._done else C.BLUE
        end = "\n" if self._done else ""
        sys.stdout.write(
            f"\r  {c}{spin}{C.RESET} {self.label:<26} {c}{bar}{C.RESET} {int(frac*100):3d}%{end}"
        )
        sys.stdout.flush()

    def _run(self):
        start = time.monotonic()
        i = 0
        while not self._stop.is_set():
            t = time.monotonic() - start
            frac = min(self.cap, (t / self.est) if self.est else self.cap)
            self._draw(frac, SPINNER[i % len(SPINNER)])
            i += 1
            time.sleep(0.08)

    def __enter__(self):
        if C._on:
            self._t = threading.Thread(target=self._run, daemon=True)
            self._t.start()
        else:
            print(f"  … {self.label}")
        return self

    def finish(self, success=True):
        self._stop.set()
        if self._t:
            self._t.join()
        self._done = True
        if C._on:
            self._draw(1.0 if success else 0.0, "✓" if success else "✗")

    def __exit__(self, *exc):
        if not self._stop.is_set():
            self.finish(success=exc[0] is None)

def human(n):
    return f"{n/1024:.0f} KB" if n < 1024*1024 else f"{n/1024/1024:.2f} MB"

def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()

# ───────────────────────── prerequisites ─────────────────────────

def have(cmd):
    return shutil.which(cmd) is not None

def check_prereqs(verbose=True):
    """Returns True if we can flash. Prints guidance when verbose."""
    sysname = platform.system()
    ready = True
    if verbose:
        hdr("Prerequisites")
    if sysname == "Darwin":
        (ok if have("diskutil") else err)("diskutil (built into macOS)") if verbose else None
    elif sysname == "Linux":
        if verbose: (ok if have("lsblk") else warn)("lsblk")
    else:
        if verbose: err(f"{sysname} is not supported yet (macOS / Linux only)")
        ready = False
    if have("mcopy") and have("mdir"):
        if verbose: ok("mtools (mcopy, mdir)")
    else:
        ready = False
        if verbose:
            err("mtools not found")
            info("install:  brew install mtools   (macOS)")
            info("          sudo apt install mtools  (Linux)")
    return ready

# ───────────────────────── device detection ─────────────────────────

def _run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)

def detect_usb_mode_macos():
    """Return ('iap'|'running'|'romdfu'|None) using system_profiler."""
    try:
        out = _run(["system_profiler", "SPUSBDataType", "-json"], timeout=15).stdout
    except Exception:
        return None
    import json
    blob = out.lower()
    if f'"product_id":"0x{USB_IAP[1]}"' in blob.replace(" ", "") or "msc iap" in blob:
        return "iap"
    if f"0x{USB_ROM_DFU[1]}" in blob:
        return "romdfu"
    if f"0x{USB_RUNNING[1]}" in blob:
        return "running"
    # Fallback: any 0x2e3c vendor present?
    if "0x2e3c" in blob:
        return "running"
    return None

def find_iap_disk_macos():
    """Return /dev/diskN backing the IAP volume, or None.

    A disk whose volume is labelled IAP wins; an AT32/MSC media name is a
    fallback for when diskutil reports no volume name. "Some small USB disk"
    is not a candidate: an 8 MB stick is not the scope. Either way the label
    is checked again on the volume's own FAT before anything is written
    (_write_image_macos)."""
    try:
        plist = plistlib.loads(_run(["diskutil", "list", "-plist"]).stdout.encode())
    except Exception:
        return None
    hinted = None
    for name in plist.get("AllDisks", []):
        try:
            info_pl = plistlib.loads(
                _run(["diskutil", "info", "-plist", name]).stdout.encode())
        except Exception:
            continue
        vol   = (info_pl.get("VolumeName") or "")
        media = (info_pl.get("MediaName") or "") + (info_pl.get("IORegistryEntryName") or "")
        if vol.upper() == IAP_VOLUME_LABEL:
            return f"/dev/{name}"
        if hinted is None and ("msc" in media.lower() or "at32" in media.lower()):
            hinted = f"/dev/{name}"
    return hinted

def find_iap_disk_linux():
    """Return /dev/sdX backing the IAP volume, or None (via lsblk JSON)."""
    import json
    try:
        out = _run(["lsblk", "-J", "-o", "NAME,LABEL,VENDOR,SIZE,TYPE"]).stdout
        data = json.loads(out)
    except Exception:
        return None
    def walk(nodes, want):
        for n in nodes:
            if want(n):
                return "/dev/" + n["name"]
            r = walk(n.get("children", []), want)
            if r:
                return r
        return None
    nodes = data.get("blockdevices", [])
    # The label wins; an AT32 vendor string is only a fallback, and the label
    # is read again off the FAT before mcopy writes (_write_image_linux).
    return (walk(nodes, lambda n: (n.get("label") or "").upper() == IAP_VOLUME_LABEL)
            or walk(nodes, lambda n: "AT32" in (n.get("vendor") or "").upper()))

def find_device():
    """Unified probe. Returns dict: {mode, dev}."""
    sysname = platform.system()
    if sysname == "Darwin":
        dev = find_iap_disk_macos()
        if dev:
            return {"mode": "iap", "dev": dev}
        return {"mode": detect_usb_mode_macos(), "dev": None}
    if sysname == "Linux":
        dev = find_iap_disk_linux()
        return {"mode": "iap" if dev else None, "dev": dev}
    return {"mode": None, "dev": None}

def print_device_status(d):
    hdr("Device")
    mode, dev = d["mode"], d["dev"]
    if mode == "iap" and dev:
        ok(f"Upgrade mode — ready to flash  ({dev})")
        return True
    if mode == "iap":
        err("Upgrade mode seen on USB (2e3c:5720), but no disk with a FAT volume labelled IAP.")
        info("Refusing to guess which disk to write. Replug, wait for the IAP drive, retry.")
        return False
    if mode == "running":
        warn("Device is running normally (CDC), not in upgrade mode.")
        info("Enter upgrade mode:  hold MENU + tap Power  (or pinhole reset while holding MENU).")
    elif mode == "romdfu":
        warn("Device is in ROM DFU mode (BOOT0+reset).")
        info("That's a different channel — use dfu-util / the first-time-setup path, not this tool.")
    else:
        err("No 2C53T detected over USB.")
        info("Plug in via USB-C, then enter upgrade mode: hold MENU + tap Power.")
        info("On macOS: keep the ST-Link UNPLUGGED while in upgrade mode (it browns out USB power).")
    return False

# ───────────────────────── firmware discovery ─────────────────────────

def looks_like_app_image(path):
    """Sanity-check the Cortex-M vector table so we never flash a SPI dump."""
    try:
        with open(path, "rb") as f:
            head = f.read(8)
        if len(head) < 8:
            return False
        sp    = int.from_bytes(head[0:4], "little")
        reset = int.from_bytes(head[4:8], "little")
        sram_ok  = 0x20000000 <= sp <= 0x20040000
        reset_ok = 0x08000000 <= (reset & ~1) < 0x08100000 and (reset & 1) == 1
        size_ok  = 0 < path.stat().st_size <= 1024 * 1024
        return sram_ok and reset_ok and size_ok
    except Exception:
        return False

def discover_images(extra=None):
    """Return a list of dicts {path, size, label} for plausible app images."""
    seen, out = set(), []
    candidates = []
    for rel in IMAGE_SEARCH:
        base = REPO_ROOT / rel
        if base.is_dir():
            candidates += sorted(base.rglob("*.bin"))
    if extra:
        candidates.append(Path(extra))
    for p in candidates:
        p = p.resolve()
        if p in seen or not p.is_file():
            continue
        if any(x in p.name.lower() for x in IMAGE_EXCLUDE):
            continue
        if not looks_like_app_image(p):
            continue
        seen.add(p)
        out.append({"path": p, "size": p.stat().st_size, "label": _label_for(p)})
    # Built OpenScope first, then stock by name.
    out.sort(key=lambda d: (0 if "build" in str(d["path"]) else 1, str(d["path"])))
    return out

def _label_for(path):
    name = path.name.lower()
    if "build" in str(path) and name in ("firmware.bin", "openscope.bin"):
        return "OpenScope (your build)"
    if name.startswith("app_2c53t"):
        return "Stock firmware " + path.stem.replace("APP_2C53T_", "").replace("_", " ")
    return path.stem

def print_image_table(images):
    if not images:
        warn("No app images found in firmware/build or archive/.")
        info("Pass an explicit path:  iap_flash.py flash /path/to/APP_2C53T_xxx.bin")
        return
    hdr("Available firmware")
    for i, im in enumerate(images, 1):
        sha = sha256_of(im["path"])
        tag = KNOWN_SHA.get(sha)
        verified = f"{C.GREEN}✓ {tag}{C.RESET}" if tag else f"{C.DIM}sha {sha[:8]}…{C.RESET}"
        rel = im["path"].relative_to(REPO_ROOT) if str(im["path"]).startswith(str(REPO_ROOT)) else im["path"]
        print(f"  {C.BOLD}{i}){C.RESET} {im['label']:<28} {human(im['size']):>9}   {verified}")
        info(str(rel))

def pick_image(images):
    if not images:
        return None
    print_image_table(images)
    while True:
        try:
            sel = input(f"\n{C.BOLD}Select firmware [1-{len(images)}]{C.RESET} (q to quit): ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return None
        if sel.lower() in ("q", "quit", ""):
            return None
        if sel.isdigit() and 1 <= int(sel) <= len(images):
            return images[int(sel) - 1]
        err("Invalid selection.")

# ───────────────────────── flashing ─────────────────────────

def _mtools(args, dev, sudo=False):
    env = dict(os.environ, MTOOLS_SKIP_CHECK="1")
    # Resolve to an absolute path so `sudo` finds it even when root's PATH
    # lacks Homebrew (/opt/homebrew/bin on Apple Silicon).
    binary = shutil.which(args[0]) or args[0]
    if sudo:
        # sudo's env_reset strips MTOOLS_SKIP_CHECK — re-inject it via `env`.
        cmd = ["sudo", "env", "MTOOLS_SKIP_CHECK=1", binary] + args[1:]
    else:
        cmd = [binary] + args[1:]
    return subprocess.run(cmd, capture_output=True, text=True, env=env)

def _mtools_with_fallback(args, dev):
    """Run an mtools command; retry under sudo on a permission error.
    On macOS the raw disk always needs root, so go straight to sudo (creds
    are pre-authenticated up front in flash_image, so this won't prompt)."""
    if MACOS:
        return _mtools(args, dev, sudo=True)
    r = _mtools(args, dev, sudo=False)
    if r.returncode != 0 and any(
        s in (r.stderr or "").lower()
        for s in ("permission", "not permitted", "denied", "could not open")
    ):
        warn("Raw disk needs elevated access — retrying with sudo (you may be prompted).")
        r = _mtools(args, dev, sudo=True)
    return r

def unmount(dev):
    if platform.system() == "Darwin":
        _run(["diskutil", "unmountDisk", "force", dev])
    else:
        # Linux: unmount any mounted partitions of the device.
        _run(["sh", "-c", f"for m in $(lsblk -nro MOUNTPOINT {dev} 2>/dev/null); do umount \"$m\"; done"])

def read_marker(dev):
    """Return the current marker filename on the IAP volume (READY/SUCCESS), or ''."""
    r = _mtools_with_fallback(["mdir", "-i", dev, "::/"], dev)
    listing = (r.stdout or "").upper()
    if MARKER_SUCCESS.replace(".TXT", "") in listing and "SUCCESS" in listing:
        return MARKER_SUCCESS
    if "READY" in listing:
        return MARKER_READY
    return ""

def fat_volume_labels(img):
    """Volume labels a FAT12/16 image carries: the root-directory label entry
    and the boot sector's BPB label. Either may be the one the OS shows."""
    labels = []
    if len(img) < 0x40:
        return labels
    bps = int.from_bytes(img[0x0B:0x0D], "little")
    reserved = int.from_bytes(img[0x0E:0x10], "little")
    nfats = img[0x10]
    root_entries = int.from_bytes(img[0x11:0x13], "little")
    fat_sectors = int.from_bytes(img[0x16:0x18], "little")
    if bps and root_entries and fat_sectors:
        start = (reserved + nfats * fat_sectors) * bps
        for off in range(start, min(start + 32 * root_entries, len(img) - 31), 32):
            entry = img[off:off + 32]
            if entry[0] == 0x00:
                break
            if entry[0] != 0xE5 and entry[11] & 0x08 and entry[11] != 0x0F:
                labels.append(entry[:11].decode("ascii", "replace").strip())
                break
    if img[0x26] == 0x29:
        bpb = img[0x2B:0x36].decode("ascii", "replace").strip()
        if bpb and bpb.upper() != "NO NAME":
            labels.append(bpb)
    return labels

def is_iap_volume(labels):
    return any(l.upper() == IAP_VOLUME_LABEL for l in labels)

def mdir_volume_label(listing):
    """The label in `mdir` output, or ''. Plain: " Volume in drive : is IAP".
    mtools 4.0.49 shows a label that has an LFN entry as
    "IAP___ (abbr=IAP        )"; the 8.3 abbreviation is the FAT label."""
    m = re.search(r"Volume in drive \S+ is (.+)", listing or "")
    if not m:
        return ""
    abbr = re.search(r"\(abbr=([^)]*)\)", m.group(1))
    return (abbr.group(1) if abbr else m.group(1)).strip()

def split_last_sector(runs):
    """The dd calls for the changed-sector runs, in sector order, with the
    image's last sector in a call of its own: the bootloader takes the file
    when that sector lands, so only on that call is a vanishing disk news of
    success."""
    s, e = runs[-1]
    calls = list(runs[:-1]) + ([(s, e - 1)] if e - s > 1 else [])
    return calls + [(e - 1, e)]

def classify_dd_failure(stderr, last_call):
    """'flashed' | 'incomplete' | 'error' for a dd write call that failed.

    BSD dd prints its "N+M records in" summary once it is past opening both
    files (verified: none on an open error, "1+0 records in / 0+0 records
    out" on a failed write). On the last-sector call a summary means that
    sector was handed to the device as it left the bus; no summary means the
    disk was already gone before it went out."""
    low = (stderr or "").lower()
    if not any(x in low for x in VANISH_ERRORS):
        return "error"
    if not last_call:
        return "incomplete"
    m = re.search(r"(\d+)\+(\d+) records in", stderr)
    return "flashed" if m and int(m.group(1)) + int(m.group(2)) >= 1 else "incomplete"

def _write_image(dev, path):
    """Write the app image onto the IAP volume. Returns (ok, message)."""
    return _write_image_macos(dev, path) if MACOS else _write_image_linux(dev, path)

def _write_image_macos(dev, path):
    # Mirror what `mcopy -i /dev/sdc` does on Linux: write ONLY the sectors the
    # file copy actually changed (dir + FAT + the file's data), not the whole
    # volume. We can't mcopy straight to the raw device (sub-block reads fail)
    # and the buffered device won't flush to USB — so: snapshot the volume,
    # inject the file into the snapshot with mtools, diff to find the changed
    # 2048-byte sectors, and dd just those runs to the raw (unbuffered) device.
    import tempfile
    SECT = 2048
    raw = dev.replace("/dev/disk", "/dev/rdisk")
    before = tempfile.NamedTemporaryFile(prefix="iap_b_", suffix=".img", delete=False).name
    after  = tempfile.NamedTemporaryFile(prefix="iap_a_", suffix=".img", delete=False).name
    try:
        # 1. snapshot the live volume (aligned reads off the raw device)
        r = subprocess.run(["sudo", "dd", f"if={raw}", f"of={before}", "bs=1m"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return (False, "reading volume (dd): " + (r.stderr or "").strip())
        # 1b. the snapshot is the volume we are about to write: its own FAT
        #     must say IAP, whatever diskutil matched on
        with open(before, "rb") as fb:
            labels = fat_volume_labels(fb.read(1 << 20))
        if not is_iap_volume(labels):
            return (False, f"refusing to write {dev}: its FAT volume label is "
                           f"{', '.join(labels) or 'missing'}, not {IAP_VOLUME_LABEL} "
                           "(not the 2C53T upgrade-mode drive)")
        # 2. after = before + our file (mtools on a plain file — no constraints)
        shutil.copyfile(before, after)
        mc = shutil.which("mcopy") or "mcopy"
        m = subprocess.run([mc, "-o", "-i", after, str(path), f"::/{path.name}"],
                           capture_output=True, text=True,
                           env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
        if m.returncode != 0:
            return (False, "staging file (mcopy): " + (m.stderr or m.stdout or "").strip())
        # 3. diff into contiguous runs of changed 2048-byte sectors
        runs, start, idx = [], None, 0
        with open(before, "rb") as fb, open(after, "rb") as fa:
            while True:
                sb, sa = fb.read(SECT), fa.read(SECT)
                if not sa:
                    break
                if sb != sa:
                    if start is None:
                        start = idx
                elif start is not None:
                    runs.append((start, idx)); start = None
                idx += 1
            if start is not None:
                runs.append((start, idx))
        if not runs:
            return (False, "mcopy changed nothing (FAT read incoherent?)")
        # 4. write only those runs to the raw device, block-aligned
        calls = split_last_sector(runs)
        total, done = sum(e - s for s, e in runs), 0
        for i, (s, e) in enumerate(calls):
            w = subprocess.run(
                ["sudo", "dd", f"if={after}", f"of={raw}", "bs=2048",
                 f"skip={s}", f"seek={s}", f"count={e - s}", "conv=notrunc"],
                capture_output=True, text=True)
            if w.returncode != 0:
                # The bootloader reboots the instant it has the complete file,
                # yanking the disk out under the last write — a success
                # signal, but ONLY there: earlier, the file was not whole.
                verdict = classify_dd_failure(w.stderr, i == len(calls) - 1)
                if verdict == "flashed":
                    return (True, f"all {total} changed sectors written; the "
                                  "device left the bus as the last one went out")
                if verdict == "incomplete":
                    return (False, f"the IAP disk disappeared with {done} of "
                                   f"{total} changed sectors written: the image "
                                   "did NOT reach the device whole (unplug? "
                                   "brown-out?). dd: " + (w.stderr or "").strip())
                return (False, "writing run (dd): " + (w.stderr or "").strip())
            done += e - s
        subprocess.run(["sync"])
        return (True, "")
    finally:
        for f in (before, after):
            try:
                os.unlink(f)
            except OSError:
                pass

def _write_image_linux(dev, path):
    d = _mtools_with_fallback(["mdir", "-i", dev, "::/"], dev)
    label = mdir_volume_label(d.stdout)
    if d.returncode != 0 or label.upper() != IAP_VOLUME_LABEL:
        return (False, f"refusing to write {dev}: its FAT volume label is "
                       f"{label or 'missing'}, not {IAP_VOLUME_LABEL} "
                       "(not the 2C53T upgrade-mode drive) "
                       + (d.stderr or "").strip())
    m = _mtools_with_fallback(["mcopy", "-o", "-i", dev, str(path), f"::/{path.name}"], dev)
    if m.returncode != 0:
        return (False, (m.stderr or m.stdout or "").strip())
    subprocess.run(["sync"])
    pre = [] if os.geteuid() == 0 else ["sudo"]
    subprocess.run(pre + ["blockdev", "--flushbufs", dev], capture_output=True)
    subprocess.run(["sync"])
    return (True, "")

def image_size_error(size):
    """Why an image of `size` bytes cannot go into the app slot, or ''."""
    if size <= 0:
        return "the image is empty"
    if size > APP_SLOT_MAX:
        return (f"the image is {size} bytes; the app slot 0x{APP_SLOT_BASE:08X}.."
                f"0x{APP_SLOT_CEILING:08X} holds {APP_SLOT_MAX} ({human(APP_SLOT_MAX)})")
    return ""

def flash_image(dev, image):
    path = image["path"]
    size = path.stat().st_size
    sha = sha256_of(path)
    tag = KNOWN_SHA.get(sha)

    hdr("Flash plan")
    print(f"  Image:   {C.BOLD}{path.name}{C.RESET}")
    print(f"  Size:    {human(size)}  ({size} bytes; app slot holds {APP_SLOT_MAX})")
    print(f"  SHA256:  {sha}")
    print("           " + (f"{C.GREEN}✓ {tag}{C.RESET}" if tag else f"{C.YELLOW}(unrecognized — flash at your own risk){C.RESET}"))
    print(f"  Target:  {dev}  (IAP volume)")
    too_big = image_size_error(size)
    if too_big:
        err(f"Refusing: {too_big}.")
        return False
    info("Overwrites the device application. Recovery: re-run this tool and flash any image.")

    try:
        if input(f"\n{C.BOLD}Proceed?{C.RESET} [y/N] ").strip().lower() not in ("y", "yes"):
            print("Aborted.")
            return False
    except (EOFError, KeyboardInterrupt):
        print("\nAborted.")
        return False

    hdr("Flashing")
    if MACOS:
        info("Raw-disk write needs admin access — authenticating once up front.")
        if subprocess.run(["sudo", "-v"]).returncode != 0:
            err("sudo authentication failed.")
            return False
    unmount(dev)

    # Phase 1: write the image onto the IAP volume (raw dd on macOS).
    result = {}
    def _do_write():
        result["r"] = _write_image(dev, path)
    with Progress("Writing image", est_seconds=18 if MACOS else max(1.5, size / 700_000)) as bar:
        t = threading.Thread(target=_do_write); t.start(); t.join()
        wok, wmsg = result.get("r", (False, "no result"))
        bar.finish(success=wok)
    if not wok:
        err("write failed:")
        info(wmsg)
        return False

    # Phase 2: the bootloader auto-detects the written file, flashes it, and
    # reboots — at which point the IAP volume's device node disappears. That
    # vanish is the reliable success signal (on macOS the buffered directory
    # cache is stale after a raw write, so we don't trust a re-read marker).
    with Progress("Device flashing & verifying", est_seconds=6) as bar:
        deadline = time.monotonic() + FLASH_TIMEOUT_S
        outcome = "timeout"
        while time.monotonic() < deadline:
            time.sleep(0.6)
            if not Path(dev).exists():
                outcome = "rebooted"; break
            if not MACOS and read_marker(dev) == MARKER_SUCCESS:
                outcome = "success"; break
        bar.finish(success=outcome in ("success", "rebooted"))

    if outcome in ("success", "rebooted"):
        hdr("Done")
        ok("Firmware flashed — " + (
            "the bootloader wrote SUCCESS.TXT." if outcome == "success" else
            "the whole image was written and the IAP drive then left the bus "
            "(the bootloader flashes and reboots)."))
        if wmsg:
            info(wmsg)
        if "build" in str(path):
            info("If this was a debug build with a USB CDC shell, replug USB to enumerate it.")
        return True
    err("Timed out waiting for the device to confirm the flash.")
    info("It may still have flashed. Re-run `iap_flash.py status` to check, or reflash.")
    return False

# ───────────────────────── guide text ─────────────────────────

GUIDE = f"""{C.BOLD}FNIRSI 2C53T — flashing over the stock IAP (upgrade-mode) channel{C.RESET}

{C.CYAN}1. Enter upgrade mode{C.RESET}
   • Connect the device to this computer with USB-C.
   • Hold {C.BOLD}MENU{C.RESET} and tap {C.BOLD}Power{C.RESET}  (works even from a frozen app —
     the bootloader checks MENU at boot). The screen shows "firmware upgrade".
   • The device mounts a FAT12 volume named "{IAP_VOLUME_LABEL}".

{C.CYAN}2. Flash{C.RESET}
   • Run:  {C.BOLD}iap_flash.py{C.RESET}        (interactive: pick firmware, confirm)
     or:   {C.BOLD}iap_flash.py flash <path-to-APP_2C53T_xxx.bin>{C.RESET}
   • The tool writes through mtools to the raw device (bypassing the macOS FAT
     driver, which otherwise corrupts the write), then waits for the bootloader
     to confirm and reboot.

{C.CYAN}3. Recovery — you can always get back to stock{C.RESET}
   • Re-enter upgrade mode (MENU + tap pinhole reset) and flash a stock image:
     {C.DIM}archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin{C.RESET}
   • A bad/garbled flash is never a brick — just reflash.

{C.YELLOW}Notes{C.RESET}
   • In upgrade mode the device runs on USB power only. Keep any SWD/ST-Link
     probe UNPLUGGED — it can brown out the USB-powered volume mid-write.
   • Finder drag-drop does NOT work on macOS (2048-byte sectors + AppleDouble
     junk). Always use this tool.
   • mtools is required:  brew install mtools
"""

# ───────────────────────── CLI ─────────────────────────

def cmd_status(args):
    print_device_status(find_device())

def cmd_list(args):
    print_device_status(find_device())
    print_image_table(discover_images(args.image))

def cmd_doctor(args):
    fine = check_prereqs(verbose=True)
    hdr("Result")
    (ok if fine else err)("Ready to flash." if fine else "Resolve the items above, then retry.")
    sys.exit(0 if fine else 1)

def cmd_guide(args):
    print(GUIDE)

def cmd_flash(args, interactive_default=False):
    if not check_prereqs(verbose=False):
        err("Missing prerequisites — run `iap_flash.py doctor`.")
        sys.exit(1)
    d = find_device()
    if not print_device_status(d):
        sys.exit(1)

    if args.image and Path(args.image).is_file():
        p = Path(args.image).resolve()
        if not looks_like_app_image(p):
            warn("That file doesn't look like a valid Cortex-M app image (bad vector table).")
            try:
                if input("  Flash anyway? [y/N] ").strip().lower() not in ("y", "yes"):
                    sys.exit(1)
            except (EOFError, KeyboardInterrupt):
                sys.exit(1)
        image = {"path": p, "size": p.stat().st_size, "label": _label_for(p)}
    else:
        image = pick_image(discover_images(args.image))
        if not image:
            print("Nothing selected.")
            sys.exit(0)

    sys.exit(0 if flash_image(d["dev"], image) else 1)

def build_parser():
    p = argparse.ArgumentParser(
        prog="iap_flash.py",
        description="Flash the FNIRSI 2C53T over its stock USB IAP (upgrade-mode) channel.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""examples:
  iap_flash.py                 interactive: detect device, pick firmware, flash
  iap_flash.py status          show the device's USB mode
  iap_flash.py list            list device + available firmware images
  iap_flash.py flash           pick + flash interactively
  iap_flash.py flash fw.bin    flash a specific image (skips the picker)
  iap_flash.py doctor          check prerequisites (mtools, platform)
  iap_flash.py guide           full enter-upgrade-mode / recovery walkthrough

{C.DIM}A bad flash is never a brick — re-enter upgrade mode and reflash any image.{C.RESET}""",
    )
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("status", help="show the connected device's USB mode")
    pl = sub.add_parser("list", help="list device + available firmware")
    pl.add_argument("image", nargs="?", help="also consider this image path")
    pf = sub.add_parser("flash", help="flash an image (interactive if omitted)")
    pf.add_argument("image", nargs="?", help="path to APP_2C53T_*.bin (optional)")
    pf.add_argument("-y", "--yes", action="store_true", help="(reserved) assume yes")
    sub.add_parser("doctor", help="check prerequisites")
    sub.add_parser("guide", help="print the full flashing walkthrough")
    return p

def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.cmd == "status":   return cmd_status(args)
    if args.cmd == "list":     return cmd_list(args)
    if args.cmd == "doctor":   return cmd_doctor(args)
    if args.cmd == "guide":    return cmd_guide(args)
    if args.cmd == "flash":    return cmd_flash(args)
    # No subcommand → interactive flow.
    print(f"{C.BOLD}iap_flash{C.RESET} — FNIRSI 2C53T USB flasher  {C.DIM}(--help for options){C.RESET}")
    ns = argparse.Namespace(image=None, yes=False)
    return cmd_flash(ns, interactive_default=True)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrupted.")
        sys.exit(130)
