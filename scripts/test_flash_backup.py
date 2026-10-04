#!/usr/bin/env python3
"""Host tests for scripts/flash_backup.py against a fake CDC shell.

The fake speaks the firmware's `flash dump` framing ("FLASHDUMP <n>\\r\\n" +
raw bytes + "> ") over a synthetic 64 KB flash image, and can be told to
misbehave the ways issue #39 observed (bare prompt, then silence) plus the
ones a framing parser must refuse (wrong announced length, short payload,
device ERR line).

Per ground rule 6 the rejection paths are asserted directly, and
test_guards_are_load_bearing re-runs the suite's key check with a validation
step removed and REQUIRES it to go wrong.
"""
from __future__ import annotations

import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import flash_backup as fb  # noqa: E402

SIZE = 64 * 1024
IMAGE = bytes((i * 7 + (i >> 8)) & 0xFF for i in range(SIZE))


class FakeShell:
    """Answers `flash dump` requests from IMAGE. `faults` maps request index
    -> 'bare' | 'silent' | 'wronglen' | 'short' | 'err'. 'wedge_at' makes every
    request from that index on silent, like the real #39 hang."""

    def __init__(self, faults=None, wedge_at=None):
        self.faults = dict(faults or {})
        self.wedge_at = wedge_at
        self.requests = 0
        self.rx = bytearray()

    # pyserial surface used by flash_backup
    def reset_input_buffer(self):
        self.rx.clear()

    def flush(self):
        pass

    def write(self, data: bytes):
        m = re.match(rb"flash dump 0x([0-9A-F]+) (\d+)\r\n", data)
        assert m, data
        addr, n = int(m.group(1), 16), int(m.group(2))
        idx = self.requests
        self.requests += 1
        fault = "silent" if (self.wedge_at is not None and idx >= self.wedge_at) \
            else self.faults.get(idx)
        payload = IMAGE[addr:addr + n]
        if fault == "silent":
            return
        if fault == "bare":
            self.rx += b"> "
        elif fault == "wronglen":
            self.rx += b"FLASHDUMP %d\r\n" % (n + 1) + payload + b"\x00> "
        elif fault == "short":
            self.rx += b"FLASHDUMP %d\r\n" % n + payload[: n // 2]
        elif fault == "err":
            self.rx += b"ERR: flash dump failed (3)\r\n> "
        else:
            self.rx += b"FLASHDUMP %d\r\n" % n + payload + b"> "

    def read(self, n: int) -> bytes:
        out = bytes(self.rx[:n])
        del self.rx[:n]
        return out


def run_dump(shell, start=0, end=SIZE, chunk=1024, retries=2, path=None):
    fb_time = fb.time.time
    clock = [0.0]

    def fake_time():  # advance fast so timeouts elapse without real waiting
        clock[0] += 0.05
        return clock[0]

    fb.time.time = fake_time
    try:
        tmp = path or os.path.join(tempfile.mkdtemp(), "out.bin")
        reached = fb.dump(shell, tmp, start, end, chunk, retries,
                          log=lambda m: None, sleep=lambda s: None)
        with open(tmp, "rb") as f:
            return reached, f.read(), tmp
    finally:
        fb.time.time = fb_time


class TestReadChunkRejects(unittest.TestCase):
    """Every malformed reply must raise, never return plausible bytes."""

    def _one(self, fault):
        shell = FakeShell(faults={0: fault})
        fb_time = fb.time.time
        clock = [0.0]
        fb.time.time = lambda: clock.__setitem__(0, clock[0] + 0.05) or clock[0]
        try:
            return fb.read_chunk(shell, 0, 256)
        finally:
            fb.time.time = fb_time

    def test_good_reply(self):
        self.assertEqual(self._one(None), IMAGE[:256])

    def test_bare_prompt_rejected(self):
        with self.assertRaises(fb.ChunkError):
            self._one("bare")

    def test_silence_rejected(self):
        with self.assertRaises(fb.ChunkError):
            self._one("silent")

    def test_wrong_announced_length_rejected(self):
        with self.assertRaises(fb.ChunkError):
            self._one("wronglen")

    def test_short_payload_rejected(self):
        with self.assertRaises(fb.ChunkError):
            self._one("short")

    def test_device_err_rejected(self):
        with self.assertRaises(fb.ChunkError):
            self._one("err")


class TestDump(unittest.TestCase):
    def test_clean_run_is_exact(self):
        reached, data, _ = run_dump(FakeShell())
        self.assertEqual(reached, SIZE)
        self.assertEqual(data, IMAGE)

    def test_transient_faults_are_retried(self):
        shell = FakeShell(faults={3: "bare", 10: "short", 20: "wronglen"})
        reached, data, _ = run_dump(shell)
        self.assertEqual(reached, SIZE)
        self.assertEqual(data, IMAGE)
        self.assertEqual(shell.requests, SIZE // 1024 + 3)

    def test_wedge_stops_and_keeps_good_prefix(self):
        reached, data, _ = run_dump(FakeShell(wedge_at=17))
        self.assertEqual(reached, 17 * 1024)
        self.assertEqual(data, IMAGE[:17 * 1024])  # nothing past the wedge

    def test_resume_after_wedge_completes_exactly(self):
        reached, _, path = run_dump(FakeShell(wedge_at=17))
        reached2, data, _ = run_dump(FakeShell(), start=reached, path=path)
        self.assertEqual(reached2, SIZE)
        self.assertEqual(data, IMAGE)

    def test_region_dump_lands_at_its_offset(self):
        path = os.path.join(tempfile.mkdtemp(), "out.bin")
        run_dump(FakeShell(), start=0, end=SIZE, path=path)
        with open(path, "r+b") as f:  # corrupt a region, then re-dump only it
            f.seek(0x4000)
            f.write(b"\x00" * 0x2000)
        _, data, _ = run_dump(FakeShell(), start=0x4000, end=0x6000, path=path)
        self.assertEqual(data, IMAGE)


class TestGuardsAreLoadBearing(unittest.TestCase):
    """Remove the announced-length check and the wrong-length fault must now
    slip through as data. If it still raises, the test above proves nothing."""

    def test_length_check_is_what_rejects(self):
        src = Path(fb.__file__).read_text()
        guard = "    if announced != n:\n"
        self.assertIn(guard, src, "guard text moved; update this mutation")
        mutated = src.replace(guard, "    if False:\n")
        ns: dict = {"__name__": "flash_backup_mutant"}
        exec(compile(mutated, "flash_backup_mutant", "exec"), ns)
        shell = FakeShell(faults={0: "wronglen"})
        clock = [0.0]
        real = ns["time"].time
        ns["time"].time = lambda: clock.__setitem__(0, clock[0] + 0.05) or clock[0]
        try:
            got = ns["read_chunk"](shell, 0, 256)
        finally:
            ns["time"].time = real
        self.assertEqual(len(got), 256)  # mutant accepts a frame the real code rejects


if __name__ == "__main__":
    unittest.main(verbosity=2)
