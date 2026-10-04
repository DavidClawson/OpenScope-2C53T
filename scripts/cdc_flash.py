#!/usr/bin/env python3
"""
!! `fwapply` of a ~620 KB image (it crosses flash bank 2 at 0x08080000) HUNG in
!! the RAM installer on 2 of 2 genuine-Winbond units: EXP-57 (unit #1,
!! 2026-09-22) and EXP-59 (unit #3, 2026-10-01), issue #42. USB stayed
!! enumerated but silent and the app slot was left unbootable. Cause open
!! (EXP-62). Prefer --stage-only. Recovery: hold MENU + pinhole reset → IAP
!! drive → scripts/iap_flash.py.
Flash a firmware image over the OpenScope CDC debug shell.

Host half of firmware/src/drivers/fw_loader.c: sends `fwload <size> <crc32>`,
streams the raw image, and compares the device's STAGED verdict (and the slot
manifest it reads back) with the host's own size and CRC32. Then, unless
--stage-only, sends `fwapply`. The device installs the image and SYSTEM-RESETS
into it, so the port must DROP, COME BACK, and answer `version`. Keep USB
attached: the cable carries the power rail through the reset.

The tool reports what it saw, not what it sent. Exit status:
  0  the port dropped, came back, and `version` answered (with the image's own
     build stamp, when the image carries one)
  1  usage or environment: no port, the port would not open, no `fwload` GO
  2  refused, nothing installed: the image is not linked for 0x08007000, the
     staged size/CRC differ from the host's, or the device refused `fwapply`.
     ALSO 2: INSTALLER HUNG, the port stayed enumerated but silent (#42)
  3  the port dropped but never came back
  4  the port came back but did not confirm the image (no `version` answer,
     or a build stamp that is not the image's)
Exits 2 (hung) and 3 print the recovery steps. argparse's own usage errors
also exit 2, before anything is sent.

The port is picked by USB VID:PID 2e3c:5740 (OpenScope app, CDC shell), not
as "the first usbmodem": other boards on the bench enumerate as usbmodem too.

The image must be linked for the app slot the installer writes, 0x08007000 —
`make guest` here, and the 2C23T port's own default. The plain `make` flavour is
for the HID bootloader: its vector table sits at 0x08004000, so `objcopy` emits a
file based there, and installing it at 0x08007000 puts everything 0x3000 low.
Its reset vector still points into the slot (the code IS at 0x08007000), so the
device's own gates pass it; this tool refuses it on the blank stock-settings
page that layout leaves at file offset 0x2000..0x3000.

Images stage into a 1 MB W25Q cache slot (a or b, default b), so this
firmware's own ~600 KB image round-trips fine, and so does the 2C23T port's.
A staged slot persists: `fwswap a|b` in the shell installs a cached image
later with no transfer at all. Nothing in this path can write below
0x08007000, so the factory IAP always survives.

Usage:
  python3 scripts/cdc_flash.py <image.bin> [--port /dev/cu.usbmodemXXX]
                               [--slot a|b] [--stage-only]
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import time
import zlib

import serial
from serial.tools import list_ports

CDC_VID, CDC_PID = 0x2E3C, 0x5740   # OpenScope app running: CDC debug shell

# fw_loader.c's own limits (FWL_APP_BASE, FWL_APP_CEILING, FWL_MIN_IMAGE).
APP_BASE = 0x08007000
APP_CEILING = 0x080C0000
SLOT_MAX = APP_CEILING - APP_BASE    # 757 760 B = 740 KB (FWL_DATA_MAX)
MIN_IMAGE = 8192

# Plain `make` (ld/at32f403a_app.ld, objcopy --gap-fill 0xFF): vector table at
# 0x08004000, code from 0x08007000, so file offsets 0x2000..0x3000 are the stock
# saved-settings page 0x08006000..0x08007000, left blank by contract. In a guest
# image the same offsets are code (measured: 1-2 % 0xFF).
HID_LAYOUT_HOLE = (0x2000, 0x3000)

DROP_TIMEOUT_S = 60     # fwapply -> SYSRESETREQ drops the port
RETURN_TIMEOUT_S = 30   # drop -> a 2e3c:5740 port is listed again
VERSION_TIMEOUT_S = 10  # port listed -> `version` answers

RECOVERY = ("recovery: hold MENU + pinhole reset → IAP drive → "
            "scripts/iap_flash.py")

STATUS_RE = re.compile(
    rb"fwload: (?P<state>[A-Z]+) slot=(?P<slot>[ab]) (?P<got>\d+)/(?P<exp>\d+) "
    rb"crc=(?P<crc>[0-9A-Fa-f]{8}) err=(?P<err>[^\r\n]*)\r?\n")
CACHE_RE = re.compile(
    rb"cache:\s+A=(?P<a_size>\d+)/(?P<a_crc>[0-9A-Fa-f]{8}) "
    rb"B=(?P<b_size>\d+)/(?P<b_crc>[0-9A-Fa-f]{8})\r?\n")
# cmd_version prints "Build: " __DATE__ " " __TIME__; the same literal sits in
# the image, so the host knows which build should come back.
BUILD_RE = re.compile(rb"Build: ([A-Z][a-z]{2} [ \d]\d \d{4} \d{2}:\d{2}:\d{2})")


# ── seams (the tests replace these) ─────────────────────────────────────────

def enumerate_ports():
    return list_ports.comports()


def open_port(path: str):
    return serial.Serial(path, 115200, timeout=0.1)


# ── checks ──────────────────────────────────────────────────────────────────

def image_errors(data: bytes) -> list[str]:
    """Why `data` must not be installed at 0x08007000 (empty = fine)."""
    if len(data) < MIN_IMAGE:
        return [f"image is {len(data)} B; the loader takes "
                f"{MIN_IMAGE}..{SLOT_MAX} B"]
    errors = []
    if len(data) > SLOT_MAX:
        errors.append(f"image is {len(data)} B; the app slot "
                      f"0x{APP_BASE:08X}..0x{APP_CEILING:08X} holds {SLOT_MAX} B")
    sp, rv = struct.unpack_from("<II", data)
    if not 0x20000000 < sp <= 0x20040000:
        errors.append(f"initial SP 0x{sp:08X} is not in SRAM")
    entry = rv & ~1
    if not rv & 1:
        errors.append(f"reset vector 0x{rv:08X} has no Thumb bit")
    if not APP_BASE <= entry < APP_CEILING:
        errors.append(f"reset vector 0x{rv:08X} is outside the app slot "
                      f"0x{APP_BASE:08X}..0x{APP_CEILING:08X}: "
                      f"not linked for 0x{APP_BASE:08X}")
    elif entry - APP_BASE >= len(data):
        errors.append(f"reset vector 0x{rv:08X} is past the end of a "
                      f"{len(data)} B image based at 0x{APP_BASE:08X}")
    lo, hi = HID_LAYOUT_HOLE
    if len(data) >= hi and data[lo:hi] == b"\xff" * (hi - lo):
        errors.append(
            f"file bytes 0x{lo:04X}..0x{hi:04X} are all 0xFF: the plain-`make` "
            "layout (vector table at 0x08004000, stock-settings page "
            "0x08006000 left blank). Installed at 0x08007000 it lands 0x3000 "
            "low and does not boot; build `make guest` (or a guest-* target)")
    return errors


def staged_errors(text: bytes, size: int, crc: int, slot: str) -> list[str]:
    """Compare the device's last status block with what the host streamed.

    The `fwload:` line's crc= echoes the CRC the host announced (it catches a
    mangled command line); the `cache:` entry is the slot manifest read back
    from the W25Q after the device's at-rest verify — what fwapply installs."""
    found = list(STATUS_RE.finditer(text))
    if not found:
        return ["no `fwload: <state> slot=...` verdict from the device"]
    m = found[-1]
    errors = []
    state = m["state"].decode()
    if state != "STAGED":
        errors.append(f"device verdict {state} (err={m['err'].decode().strip()})")
    if m["slot"].decode() != slot:
        errors.append(f"device staged slot {m['slot'].decode()}, host asked {slot}")
    got, exp = int(m["got"]), int(m["exp"])
    if got != size or exp != size:
        errors.append(f"device size {got}/{exp} B, host image {size} B")
    if int(m["crc"], 16) != crc:
        errors.append(f"device crc32 {m['crc'].decode().upper()}, "
                      f"host crc32 {crc:08X}")
    c = CACHE_RE.search(text, m.end())
    if c is None:
        errors.append("no `cache:` line after the verdict (slot manifest unread)")
    else:
        msize, mcrc = int(c[f"{slot}_size"]), int(c[f"{slot}_crc"], 16)
        if msize != size or mcrc != crc:
            errors.append(f"slot {slot} manifest {msize} B crc32 {mcrc:08X}, "
                          f"host image {size} B crc32 {crc:08X}")
    return errors


def parse_build(text: bytes) -> str | None:
    m = BUILD_RE.search(text)
    return m.group(1).decode() if m else None


def image_build(data: bytes) -> str | None:
    stamps = set(BUILD_RE.findall(data))
    return stamps.pop().decode() if len(stamps) == 1 else None


# ── port plumbing ───────────────────────────────────────────────────────────

def port_paths() -> list[str]:
    return [p.device for p in enumerate_ports()]


def scope_ports() -> list[str]:
    return sorted(p.device for p in enumerate_ports()
                  if p.vid == CDC_VID and p.pid == CDC_PID)


def find_port() -> str | None:
    ports = scope_ports()
    if len(ports) == 1:
        return ports[0]
    if ports:
        print(f"{len(ports)} OpenScope CDC ports ({', '.join(ports)}); "
              "pick one with --port", file=sys.stderr)
        return None
    print(f"no OpenScope CDC port (USB {CDC_VID:04x}:{CDC_PID:04x}) found; "
          "pass --port", file=sys.stderr)
    others = [d for d in port_paths() if "usbmodem" in d or "ttyACM" in d]
    if others:
        print("  ignored, not 2e3c:5740: " + ", ".join(others), file=sys.stderr)
    return None


def quiet_close(s) -> None:
    """Close a handle whose device may already be gone."""
    try:
        s.close()
    except (serial.SerialException, OSError):
        pass


def echo(chunk: bytes) -> None:
    sys.stdout.write(chunk.decode(errors="replace"))
    sys.stdout.flush()


def read_until(s, token: bytes, deadline_s: float) -> bytes:
    buf = b""
    end = time.time() + deadline_s
    while time.time() < end:
        chunk = s.read(4096)
        if chunk:
            buf += chunk
            echo(chunk)
            if token in buf:
                break
    return buf


def status_complete(buf: bytes) -> bool:
    m = STATUS_RE.search(buf)
    return m is not None and CACHE_RE.search(buf, m.end()) is not None


def read_status(s, buf: bytes, deadline_s: float) -> bytes:
    """Read on until `buf` holds a whole status block (fwload: + cache:)."""
    end = time.time() + deadline_s
    while not status_complete(buf) and time.time() < end:
        chunk = s.read(4096)
        if chunk:
            buf += chunk
            echo(chunk)
    return buf


def watch_apply(s, port: str, tracked: bool) -> str:
    """After `fwapply`: 'dropped', 'refused' or 'hung'.

    Dropped = the open handle errors out (the USB device went away) or, when
    the port was listed before, it is no longer listed. Refused = the device
    printed a status block instead of resetting (fw_loader_install_slot
    returns only on refusal, before anything is erased)."""
    said = b""
    start = time.time()
    note = start + 10
    while time.time() - start < DROP_TIMEOUT_S:
        try:
            chunk = s.read(4096)
        except (serial.SerialException, OSError):
            return "dropped"
        if chunk:
            said += chunk
            echo(chunk)
            if status_complete(said):
                return "refused"
        if tracked and port not in port_paths():
            return "dropped"
        if time.time() >= note:
            print(f"  … {note - start:.0f} s, port still up and silent")
            note += 10
    return "hung"


def wait_for_return(port: str, others: set[str]) -> str | None:
    end = time.time() + RETURN_TIMEOUT_S
    while time.time() < end:
        back = [p for p in scope_ports() if p not in others]
        if back:
            return port if port in back else back[0]
        time.sleep(0.25)
    return None


def query_version(port: str) -> bytes:
    """Open the returned port (retrying: macOS lists a node before it opens)
    and ask `version` until a Build line answers or the deadline passes."""
    reply = b""
    end = time.time() + VERSION_TIMEOUT_S
    while time.time() < end:
        try:
            with open_port(port) as s:
                s.write(b"version\r")
                stop = time.time() + 2.0
                while time.time() < stop:
                    reply += s.read(4096)
                    if BUILD_RE.search(reply):
                        return reply
        except (serial.SerialException, OSError):
            pass
        time.sleep(0.5)
    return reply


def stage(s, args, data: bytes, crc: int) -> int | None:
    """fwload + stream + verdict. Returns an exit code to stop with, or None
    when the slot is staged and confirmed and fwapply may follow."""
    time.sleep(0.3)
    s.reset_input_buffer()

    s.write(f"fwload {len(data)} {crc:08X} {args.slot}\r\n".encode())
    got = read_until(s, b"GO ", 5.0)
    if b"GO " not in got:
        print("device did not accept fwload", file=sys.stderr)
        return 1

    t0 = time.time()
    # Everything the device says WHILE we stream counts as part of the
    # verdict: on a large image the device can finish, verify and print
    # `fwload: STAGED` before this loop writes its last chunk, and a
    # verdict scanned for only afterwards is then missed entirely. Seen
    # with a 609 192 B image, which staged fine and was reported as a
    # failure. Keep the tail bounded so a chatty `mon` cannot grow it.
    seen = b""
    for off in range(0, len(data), 2048):
        s.write(data[off:off + 2048])
        # drain progress lines so the OS buffer never backs up — and
        # STOP at the first ERROR verdict: pushing the rest of a binary
        # image into a shell that already gave up feeds it as garbage
        # command lines.
        chunk = s.read(4096)
        if chunk:
            echo(chunk)
            if b"ERROR" in chunk:
                print("\ndevice reported an error mid-stream — aborted",
                      file=sys.stderr)
                return 2
            seen = (seen + chunk)[-4096:]
    rate = len(data) / max(time.time() - t0, 1e-3) / 1024
    print(f"\nstreamed in {time.time() - t0:.1f}s ({rate:.0f} KB/s)")

    seen = read_status(s, seen, 30.0)
    bad = staged_errors(seen, len(data), crc, args.slot)
    if bad:
        print("\nSTAGING NOT CONFIRMED — not applying:", file=sys.stderr)
        for e in bad:
            print(f"  - {e}", file=sys.stderr)
        return 2
    print(f"\nstaged and confirmed: slot {args.slot}, {len(data)} B, "
          f"crc32 {crc:08X} (device verdict and slot manifest)")

    if args.stage_only:
        print("staged only, not applied (per --stage-only)")
        return 0
    return None


# ── main ────────────────────────────────────────────────────────────────────

def run(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Flash an image over the OpenScope CDC shell (fwload/fwapply).")
    ap.add_argument("image")
    ap.add_argument("--port", default=None,
                    help="CDC port (default: the one 2e3c:5740 device)")
    ap.add_argument("--slot", choices=["a", "b"], default="b",
                    help="W25Q cache slot to stage into (default b)")
    ap.add_argument("--stage-only", action="store_true",
                    help="stage and verify, but do not apply")
    args = ap.parse_args(argv)

    with open(args.image, "rb") as f:
        data = f.read()
    if len(data) % 2:
        data += b"\xff"
    crc = zlib.crc32(data) & 0xFFFFFFFF
    build = image_build(data)

    bad = image_errors(data)
    if bad:
        print(f"REFUSED {args.image}: not an app image for 0x{APP_BASE:08X}",
              file=sys.stderr)
        for e in bad:
            print(f"  - {e}", file=sys.stderr)
        return 2

    if args.port:
        # a /dev/serial/by-id/... link: track the node the OS lists
        port = os.path.realpath(args.port) if os.path.islink(args.port) else args.port
        if port not in scope_ports():
            print(f"note: --port {port} is not listed as "
                  f"{CDC_VID:04x}:{CDC_PID:04x}; using it as given")
    else:
        port = find_port()
        if port is None:
            return 1
    print(f"{args.image}: {len(data)} bytes, crc32 {crc:08X}, port {port}")
    print(f"image build: {build}" if build else
          "image carries no `Build:` stamp: a reset can be confirmed, "
          "not which image came back")

    try:
        s = open_port(port)
    except (serial.SerialException, OSError) as e:
        print(f"cannot open {port}: {e}", file=sys.stderr)
        return 1
    try:
        rc = stage(s, args, data, crc)
        if rc is not None:
            return rc
        # Which ports to watch: ours drops; any OTHER scope already listed
        # must not be mistaken for ours coming back. If ours is not listed
        # (odd --port), only the read error can show the drop.
        tracked = port in port_paths()
        others = set(scope_ports()) - {port} if tracked else set()
        s.write(b"fwapply\r\n")
        outcome = watch_apply(s, port, tracked)
    finally:
        quiet_close(s)

    if outcome == "refused":
        print("\nfwapply REFUSED by the device (status above): nothing was "
              "erased, the running image stays", file=sys.stderr)
        return 2
    if outcome == "hung":
        print(f"\nno reset within {DROP_TIMEOUT_S} s of fwapply.\n"
              f"INSTALLER HUNG: USB still enumerated but silent "
              f"(see issue #42); {RECOVERY}", file=sys.stderr)
        return 2

    print("\nport dropped (system reset); waiting for it to come back …")
    back = wait_for_return(port, others)
    if back is None:
        print(f"\nDID NOT COME BACK: port dropped but no "
              f"{CDC_VID:04x}:{CDC_PID:04x} port within {RETURN_TIMEOUT_S} s "
              f"(see issue #42); {RECOVERY}", file=sys.stderr)
        if not build:
            print("(this image carries no `Build:` stamp and may have no USB "
                  "shell at all: check the screen before recovering)",
                  file=sys.stderr)
        return 3

    reply = query_version(back)
    running = parse_build(reply)
    if running is None:
        print(f"\nport {back} came back but `version` gave no Build line "
              f"(got {reply[:200]!r}): the install is not confirmed",
              file=sys.stderr)
        return 4
    print(f"device on {back}: Build: {running}")
    if build and running != build:
        print(f"\nthe device runs build {running}, the image is build {build}: "
              "the new image is NOT what came back", file=sys.stderr)
        return 4
    print("installed and confirmed: reset, re-enumerated, "
          + ("running the image's build" if build else "answering `version`"))
    return 0


def main() -> None:
    sys.exit(run())


if __name__ == "__main__":
    main()
