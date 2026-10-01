#!/usr/bin/env python3
"""Host tests for scripts/cdc_flash.py: no hardware, no serial port.

A fake scope speaks just enough of the shell (fwload -> GO -> status block,
fwapply -> goodbye, version -> Build line) and lists itself as a 2e3c:5740 port
next to an ESP32 on another usbmodem. A fake clock makes the 60 s / 30 s waits
instant: an empty read() advances it by the real port's 0.1 s timeout.

Each guard test also asserts the side effect the guard exists to prevent (no
`fwapply` typed, no port opened), so deleting a check fails its test.
"""
from __future__ import annotations

import importlib.util
import io
import struct
import sys
import tempfile
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

try:
    import serial
except ImportError:  # pyserial absent: every use of it is faked below anyway
    serial = types.ModuleType("serial")
    serial.SerialException = type("SerialException", (IOError,), {})
    serial.Serial = None
    serial.tools = types.ModuleType("serial.tools")
    serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
    serial.tools.list_ports.comports = lambda: []
    sys.modules.update({"serial": serial, "serial.tools": serial.tools,
                        "serial.tools.list_ports": serial.tools.list_ports})

MODULE_PATH = Path(__file__).resolve().parent / "cdc_flash.py"
SPEC = importlib.util.spec_from_file_location("cdc_flash", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
cdc_flash = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cdc_flash)

PORT = "/dev/cu.usbmodem083540BF89911"
ESP32 = types.SimpleNamespace(device="/dev/cu.usbmodem1101", vid=0x303A, pid=0x1001)
BUILD = b"Oct  1 2026 09:28:26"


def scope_port(path: str = PORT):
    return types.SimpleNamespace(device=path, vid=0x2E3C, pid=0x5740)


def version_reply(build: bytes = BUILD) -> bytes:
    return (b"version\r\nOpenScope 2C53T\r\nBuild: " + build +
            b"\r\nMCU: AT32F403A @ 240MHz\r\nSRAM: 224KB (EOPB0=0xFE)\r\n")


def guest_image(size: int = 0x4000, rv: int = 0x08007201) -> bytes:
    """Shaped like `make guest`: vectors for 0x08007000, code right after."""
    data = bytearray(bytes(range(256)) * (size // 256))
    data[0:8] = struct.pack("<II", 0x20037FE0, rv)
    stamp = b"OpenScope 2C53T\r\nBuild: " + BUILD + b"\r\n"
    data[0x3100:0x3100 + len(stamp)] = stamp
    return bytes(data)


def hid_layout_image() -> bytes:
    """Plain `make`: same in-slot reset vector, but the vector table is based
    at 0x08004000 and 0x08006000..0x08007000 (file 0x2000..0x3000) is blank."""
    data = bytearray(guest_image())
    data[0x200:0x3000] = b"\xff" * (0x3000 - 0x200)
    return bytes(data)


class FakeClock:
    def __init__(self) -> None:
        self.now = 1000.0

    def time(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeScope:
    """The device. after_apply: reset | hang | vanish | refuse."""
    DROP_AFTER = 8.0   # fwapply -> SYSRESETREQ
    BACK_AFTER = 3.0   # drop -> new image enumerates

    def __init__(self, after_apply: str = "reset", *, staged_crc=None,
                 manifest_crc=None, version: bytes | None = None) -> None:
        self.clock = FakeClock()
        self.after_apply = after_apply
        self.staged_crc = staged_crc        # crc= the fwload: line reports
        self.manifest_crc = manifest_crc    # crc the cache: line reports
        self.version = version_reply() if version is None else version
        self.typed = b""                    # every command line received
        self.out = b""
        self.expect = self.received = 0
        self.applied_at = None
        self.opened: list[str] = []

    def phase(self):
        """0 = original image up, None = off the bus, 1 = new image up."""
        if self.applied_at is None or self.after_apply in ("hang", "refuse"):
            return 0
        t = self.clock.now - self.applied_at
        if t < self.DROP_AFTER:
            return 0
        if self.after_apply == "vanish" or t < self.DROP_AFTER + self.BACK_AFTER:
            return None
        return 1

    def ports(self):
        return [ESP32] + ([scope_port()] if self.phase() is not None else [])

    def open(self, path: str):
        self.opened.append(path)
        if path != PORT or self.phase() is None:
            raise serial.SerialException(f"could not open port {path}")
        return FakeSerial(self, self.phase())

    def status(self, state: str, crc: int, manifest: int, err: str = "none") -> bytes:
        n = self.expect
        a = f"{n}/{manifest:08X}" if self.slot == "a" else "0/00000000"
        b = f"{n}/{manifest:08X}" if self.slot == "b" else "0/00000000"
        return (f"fwload: {state} slot={self.slot} {n}/{n} crc={crc:08X} "
                f"err={err}\r\ncache:  A={a} B={b}\r\n").encode()

    def feed(self, data: bytes) -> None:
        if self.received < self.expect:
            take = min(len(data), self.expect - self.received)
            self.received += take
            data = data[take:]
            if self.received == self.expect:
                crc = self.announced if self.staged_crc is None else self.staged_crc
                man = crc if self.manifest_crc is None else self.manifest_crc
                self.out += self.status("STAGED", crc, man)
        self.typed += data
        for line in data.split(b"\r"):
            line = line.strip()
            if line.startswith(b"fwload "):
                _, size, crc, slot = line.decode().split()
                self.expect, self.announced, self.slot = int(size), int(crc, 16), slot
                self.out += f"GO {size} bytes to slot {slot}, crc {crc} expected\r\n".encode()
            elif line == b"fwapply":
                self.applied_at = self.clock.now
                self.out += (b"applying: erase+program+verify from RAM, then SYSTEM RESET\r\n"
                             b"into the new image (a clean boot, not a jump). this port\r\n"
                             b"drops now. keep USB attached \xe2\x80\x94 it carries the rail through\r\n"
                             b"the reset. recovery = MENU+Power IAP.\r\n")
                if self.after_apply == "refuse":
                    self.out += self.status("ERROR", self.announced, self.announced,
                                            "crc mismatch")
            elif line == b"version" and self.phase() == 1:
                self.out += self.version


class FakeSerial:
    def __init__(self, scope: FakeScope, phase) -> None:
        self.scope, self.phase = scope, phase

    def __enter__(self):
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        pass

    def reset_input_buffer(self) -> None:
        self.scope.out = b""

    def _alive(self) -> None:
        if self.scope.phase() != self.phase:
            raise serial.SerialException(
                "device reports readiness to read but returned no data")

    def write(self, data: bytes) -> int:
        self._alive()
        self.scope.feed(bytes(data))
        return len(data)

    def read(self, n: int) -> bytes:
        self._alive()
        if self.scope.out:
            chunk, self.scope.out = self.scope.out[:n], self.scope.out[n:]
            return chunk
        self.scope.clock.sleep(0.1)
        return b""


def run_flash(scope: FakeScope, image: bytes, *argv: str):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "firmware.bin"
        path.write_bytes(image)
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(cdc_flash, "time", scope.clock), \
                mock.patch.object(cdc_flash, "enumerate_ports", scope.ports), \
                mock.patch.object(cdc_flash, "open_port", scope.open), \
                redirect_stdout(out), redirect_stderr(err):
            rc = cdc_flash.run([str(path), *argv])
    return rc, out.getvalue(), err.getvalue()


class ImageCheckTests(unittest.TestCase):
    def test_guest_image_passes(self) -> None:
        # positive control: a check that refused everything would pass the rest
        self.assertEqual(cdc_flash.image_errors(guest_image()), [])

    def test_plain_make_layout_refused_before_any_port_io(self) -> None:
        scope = FakeScope()
        rc, _out, err = run_flash(scope, hid_layout_image())
        self.assertEqual(rc, 2)
        self.assertIn("plain-`make`", err)
        self.assertEqual(scope.opened, [])

    def test_reset_vector_below_the_slot_refused(self) -> None:
        scope = FakeScope()
        rc, _out, err = run_flash(scope, guest_image(rv=0x08004041))
        self.assertEqual(rc, 2)
        self.assertIn("outside the app slot", err)
        self.assertEqual(scope.opened, [])

    def test_reset_vector_without_thumb_bit_refused(self) -> None:
        errors = cdc_flash.image_errors(guest_image(rv=0x08007200))
        self.assertTrue(any("Thumb" in e for e in errors), errors)

    def test_sp_outside_sram_refused(self) -> None:
        data = bytearray(guest_image())
        data[0:4] = struct.pack("<I", 0x08037FE0)
        errors = cdc_flash.image_errors(bytes(data))
        self.assertTrue(any("not in SRAM" in e for e in errors), errors)

    def test_image_larger_than_slot_refused(self) -> None:
        errors = cdc_flash.image_errors(guest_image(size=cdc_flash.SLOT_MAX + 256))
        self.assertTrue(any("holds 757760 B" in e for e in errors), errors)


class StagingTests(unittest.TestCase):
    def test_staged_crc_mismatch_refused_and_never_applied(self) -> None:
        scope = FakeScope(staged_crc=0x12345678)
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 2)
        self.assertIn("STAGING NOT CONFIRMED", err)
        self.assertIn("device crc32 12345678", err)
        self.assertNotIn(b"fwapply", scope.typed)

    def test_slot_manifest_mismatch_refused_and_never_applied(self) -> None:
        scope = FakeScope(manifest_crc=0x0BADC0DE)
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 2)
        self.assertIn("slot b manifest", err)
        self.assertNotIn(b"fwapply", scope.typed)

    def test_stage_only_confirms_and_never_applies(self) -> None:
        scope = FakeScope()
        rc, out, _err = run_flash(scope, guest_image(), "--stage-only")
        self.assertEqual(rc, 0)
        self.assertIn("staged and confirmed", out)
        self.assertNotIn(b"fwapply", scope.typed)

    def test_slot_a_is_checked_against_the_a_manifest(self) -> None:
        rc, out, _err = run_flash(FakeScope(), guest_image(), "--slot", "a", "--stage-only")
        self.assertEqual(rc, 0)
        self.assertIn("slot a", out)
        scope = FakeScope(manifest_crc=0x0BADC0DE)
        rc, _out, err = run_flash(scope, guest_image(), "--slot", "a")
        self.assertEqual(rc, 2)
        self.assertIn("slot a manifest", err)
        self.assertNotIn(b"fwapply", scope.typed)

    def test_staged_errors_requires_a_verdict(self) -> None:
        self.assertEqual(cdc_flash.staged_errors(b"GO 16384 bytes\r\n", 16384, 1, "b"),
                         ["no `fwload: <state> slot=...` verdict from the device"])


class ApplyOutcomeTests(unittest.TestCase):
    def test_port_drops_and_returns_with_the_image_build_is_success(self) -> None:
        scope = FakeScope("reset")
        rc, out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 0, err)
        self.assertIn("Build: Oct  1 2026 09:28:26", out)
        self.assertIn("installed and confirmed", out)
        self.assertIn(b"version", scope.typed)

    def test_port_never_drops_is_installer_hung_exit_2(self) -> None:
        scope = FakeScope("hang")
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 2)
        self.assertIn("INSTALLER HUNG: USB still enumerated but silent (see issue #42); "
                      "recovery: hold MENU + pinhole reset → IAP drive → "
                      "scripts/iap_flash.py", err)
        self.assertGreaterEqual(scope.clock.now - scope.applied_at,
                                cdc_flash.DROP_TIMEOUT_S)

    def test_port_drops_and_never_returns_exit_3(self) -> None:
        scope = FakeScope("vanish")
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 3)
        self.assertIn("DID NOT COME BACK", err)
        self.assertIn(cdc_flash.RECOVERY, err)

    def test_device_refusing_fwapply_is_exit_2_without_recovery(self) -> None:
        scope = FakeScope("refuse")
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 2)
        self.assertIn("REFUSED", err)
        self.assertNotIn(cdc_flash.RECOVERY, err)

    def test_return_on_another_build_is_not_success(self) -> None:
        scope = FakeScope("reset", version=version_reply(b"Sep 30 2026 00:04:02"))
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 4)
        self.assertIn("NOT what came back", err)

    def test_return_without_a_build_line_is_not_success(self) -> None:
        scope = FakeScope("reset", version=b"version\r\n> ")
        rc, _out, err = run_flash(scope, guest_image())
        self.assertEqual(rc, 4)
        self.assertIn("no Build line", err)


class ParseTests(unittest.TestCase):
    def test_version_reply_build_line_parsed(self) -> None:
        self.assertEqual(cdc_flash.parse_build(version_reply()), "Oct  1 2026 09:28:26")
        self.assertEqual(cdc_flash.parse_build(version_reply(b"Sep 30 2026 00:04:02")),
                         "Sep 30 2026 00:04:02")
        self.assertIsNone(cdc_flash.parse_build(b"OpenScope 2C53T\r\nBuild: \r\n"))

    def test_image_build_stamp_read_from_the_image(self) -> None:
        self.assertEqual(cdc_flash.image_build(guest_image()), "Oct  1 2026 09:28:26")
        self.assertIsNone(cdc_flash.image_build(b"\x00" * 64))


class PortSelectionTests(unittest.TestCase):
    def pick(self, ports):
        err = io.StringIO()
        with mock.patch.object(cdc_flash, "enumerate_ports", lambda: ports), \
                redirect_stderr(err):
            return cdc_flash.find_port(), err.getvalue()

    def test_picks_the_scope_and_ignores_other_usbmodems(self) -> None:
        port, _ = self.pick([ESP32, scope_port()])
        self.assertEqual(port, PORT)

    def test_only_other_usbmodems_is_no_port(self) -> None:
        port, err = self.pick([ESP32])
        self.assertIsNone(port)
        self.assertIn("ignored, not 2e3c:5740: /dev/cu.usbmodem1101", err)

    def test_two_scopes_is_ambiguous(self) -> None:
        port, err = self.pick([scope_port(), scope_port("/dev/cu.usbmodem2")])
        self.assertIsNone(port)
        self.assertIn("pick one with --port", err)


if __name__ == "__main__":
    unittest.main()
