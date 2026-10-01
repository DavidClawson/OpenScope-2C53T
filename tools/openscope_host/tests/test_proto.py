"""proto.py: framing and payload codecs, no hardware (remote_protocol.md §6 step 5).

The rejection paths are asserted, not just the happy path: a corrupted
checksum must be rejected, a truncated frame must never be accepted, and the
decoder must resync after garbage. test_guards_are_load_bearing removes the
checksum comparison and requires a corrupted frame to slip through.
"""
from __future__ import annotations

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from openscope import proto  # noqa: E402


def status_payload(ver=1, mode=1, pct=73, flags=3, mv=3987, up=123456, stalls=2, heals=1, fw=b"OpenScope x"):
    return struct.pack("<BBBBHIIHB", ver, mode, pct, flags, mv, up, stalls, heals, len(fw)) + fw


class TestEncode(unittest.TestCase):
    def test_ping_bytes(self):
        self.assertEqual(proto.encode(proto.CMD_PING), bytes([0xAA, 0x01, 0x00, 0x00, 0x01]))

    def test_length_is_big_endian_and_checksum_xor(self):
        f = proto.encode(0x0A, bytes([0x09]))
        self.assertEqual(f, bytes([0xAA, 0x0A, 0x00, 0x01, 0x09, 0x0A ^ 0x01 ^ 0x09]))

    def test_refuses_payload_the_device_would_drop(self):
        proto.encode(0x20, bytes(256))
        with self.assertRaises(ValueError):
            proto.encode(0x20, bytes(257))


class TestDecoder(unittest.TestCase):
    def test_roundtrip(self):
        d = proto.Decoder()
        for n in (0, 1, 255, 256, 1036, 4000):
            payload = bytes((i * 31 + 7) & 0xFF for i in range(n))
            raw = bytes([0xAA, 0x91, n >> 8, n & 0xFF]) + payload
            raw += bytes([proto.checksum(raw[1:])])
            frames = d.feed(raw)
            self.assertEqual(frames, [proto.Frame(0x91, payload)], n)

    def test_corrupted_checksum_rejected(self):
        d = proto.Decoder()
        raw = bytearray(proto.encode(0x83, b"hello"))
        raw[-1] ^= 0xFF
        self.assertEqual(d.feed(bytes(raw)), [])
        self.assertEqual(d.bad_checksum, 1)

    def test_corrupted_payload_rejected(self):
        d = proto.Decoder()
        raw = bytearray(proto.encode(0x83, b"hello"))
        raw[5] ^= 0x01
        self.assertEqual(d.feed(bytes(raw)), [])

    def test_truncated_frame_never_accepted(self):
        full = proto.encode(0x83, b"payload")
        for cut in range(1, len(full)):
            d = proto.Decoder()
            self.assertEqual(d.feed(full[:cut]), [], f"accepted a frame cut at {cut}")

    def test_split_feeding_reassembles(self):
        full = proto.encode(0x85, b"abcdefgh")
        d = proto.Decoder()
        got = []
        for b in full:
            got += d.feed(bytes([b]))
        self.assertEqual(got, [proto.Frame(0x85, b"abcdefgh")])

    def test_resync_after_garbage(self):
        d = proto.Decoder()
        garbage = bytes([0xAA, 0x83, 0x00, 0x05, 1, 2]) + bytes([0xAA, 0x13, 0x37])   # two false starts
        good = proto.encode(0x81)
        frames = d.feed(garbage + good)
        # the false start announcing 5 bytes swallows the next header bytes;
        # the decoder must still find the real frame once those fail the checksum
        frames += d.feed(proto.encode(0x81))
        self.assertIn(proto.Frame(0x81, b""), frames)
        self.assertGreaterEqual(d.resyncs, 1)

    def test_resync_does_not_lose_following_frame(self):
        d = proto.Decoder()
        bad = bytearray(proto.encode(0x83, b"xx"))
        bad[-1] ^= 1
        frames = d.feed(bytes(bad) + proto.encode(0x85, b"ok"))
        self.assertEqual(frames, [proto.Frame(0x85, b"ok")])

    def test_shell_text_is_kept_separately(self):
        d = proto.Decoder()
        frames = d.feed(b"\r\n> " + proto.encode(0x81) + b"version\r\n")
        self.assertEqual(frames, [proto.Frame(0x81, b"")])
        self.assertEqual(d.take_text(), b"\r\n> version\r\n")

    def test_oversize_length_is_noise(self):
        d = proto.Decoder(max_payload=1024)
        self.assertEqual(d.feed(bytes([0xAA, 0x83, 0xFF, 0xFF]) + proto.encode(0x81)),
                         [proto.Frame(0x81, b"")])


class TestStatus(unittest.TestCase):
    def test_parse(self):
        s = proto.parse_status(status_payload())
        self.assertEqual((s.mode_name, s.battery_pct, s.battery_mv), ("meter", 73, 3987))
        self.assertTrue(s.charging and s.capture_ready and not s.battery_critical)
        self.assertEqual((s.uptime_ms, s.usb_tx_stalls, s.usb_heals), (123456, 2, 1))
        self.assertEqual(s.fw_version, "OpenScope x")

    def test_unknown_major_refused(self):
        with self.assertRaises(proto.ProtocolError):
            proto.parse_status(status_payload(ver=2))

    def test_length_mismatch_refused(self):
        with self.assertRaises(proto.ProtocolError):
            proto.parse_status(status_payload()[:-1])
        with self.assertRaises(proto.ProtocolError):
            proto.parse_status(status_payload()[:10])


class TestMeter(unittest.TestCase):
    def payload(self, unit=b"kOhm", disp=b"OL"):
        return (struct.pack("<IfhBBBBBB", 9, 0.0, 0, 0, 5, 0x04, 2, 1, len(unit)) + unit
                + bytes([len(disp)]) + disp)

    def test_parse(self):
        m = proto.parse_meter(self.payload())
        self.assertEqual((m.unit, m.display, m.result, m.submode), ("kOhm", "OL", "overload", 2))

    def test_string_lengths_must_add_up(self):
        p = self.payload()
        for bad in (p[:-1], p + b"x", p[:15]):
            with self.assertRaises(proto.ProtocolError):
                proto.parse_meter(bad)


def wave_payload(fid=42, ch=0, flags=0x06, tb=0x10, vdiv=6, samples=bytes(range(16)), hlen=24,
                 rate=12490, uv=1264486, cpd=32, skip=4, extra=b""):
    hdr = struct.pack("<IBBBBHHIIHH", fid, ch, flags, tb, vdiv, len(samples), hlen, rate, uv, cpd, skip)
    return hdr + extra + samples


class TestWaveform(unittest.TestCase):
    def test_parse(self):
        w = proto.parse_waveform(wave_payload())
        self.assertEqual((w.frame_id, w.channel_name, w.sample_rate_hz, w.uv_per_div), (42, "CH1", 12490, 1264486))
        self.assertTrue(w.timebase_measured and w.vdiv_measured and not w.calibrated)
        self.assertEqual(w.samples, bytes(range(16)))
        self.assertEqual(w.body, bytes(range(4, 16)), "body starts after head_skip")
        self.assertAlmostEqual(w.volts_per_count, 1.264486 / 32)

    def test_longer_header_is_skipped_not_misread(self):
        """header_len lets the firmware append fields without a major bump."""
        w = proto.parse_waveform(wave_payload(hlen=28, extra=b"\xEE" * 4))
        self.assertEqual(w.samples, bytes(range(16)))

    def test_rejections(self):
        good = wave_payload()
        for bad, why in ((good[:-1], "short"), (good + b"x", "long"), (good[:20], "no header"),
                         (wave_payload(hlen=20), "header_len below the fixed part"),
                         (wave_payload(ch=2), "no CH3"),
                         (wave_payload(fid=0), "frame_id 0 = no record"),
                         (wave_payload(flags=proto.WAVE_FLAG_SYNTHETIC), "synthetic is refused, not returned")):
            with self.assertRaises(proto.ProtocolError, msg=why):
                proto.parse_waveform(bad)

    def test_unmeasured_numbers_are_not_invented(self):
        w = proto.parse_waveform(wave_payload(flags=0, rate=0, uv=0))
        self.assertIsNone(w.volts_per_count)
        self.assertIsNone(w.seconds_per_sample)
        self.assertEqual((w.timebase_tier, w.vdiv_tier), ("none", "none"))
        h = w.header()
        self.assertIsNone(h["sample_rate_hz"])
        self.assertIsNone(h["uv_per_div"])

    def test_channel_mask(self):
        self.assertEqual(proto.channel_mask([1]), 1)
        self.assertEqual(proto.channel_mask("1,2"), 3)
        self.assertEqual(proto.channel_mask(["CH2"]), 2)
        for bad in ([], [3], "0", [1, 3]):
            with self.assertRaises(ValueError):
                proto.channel_mask(bad)


class TestPeriodEstimate(unittest.TestCase):
    def test_sine_and_square(self):
        import math
        sine = [int(round(128 + 60 * math.sin(2 * math.pi * i / 37.3))) for i in range(900)]
        self.assertAlmostEqual(proto.estimate_period(sine), 37.3, delta=0.2)
        square = [200 if (i // 25) % 2 else 50 for i in range(900)]
        self.assertAlmostEqual(proto.estimate_period(square), 50.0, delta=0.01)

    def test_noise_at_the_level_does_not_add_crossings(self):
        import math
        noisy = [int(round(128 + 60 * math.sin(2 * math.pi * i / 50) + (3 if i % 2 else -3)))
                 for i in range(900)]
        self.assertAlmostEqual(proto.estimate_period(noisy), 50.0, delta=0.5)

    def test_refuses_what_it_cannot_know(self):
        self.assertIsNone(proto.estimate_period([128] * 900), "flat")
        self.assertIsNone(proto.estimate_period([127, 128, 129, 128] * 225), "span below the floor")
        one = [200 if 300 <= i < 600 else 50 for i in range(900)]
        self.assertIsNone(proto.estimate_period(one), "one edge is not a period")


class TestButtons(unittest.TestCase):
    def test_names_and_ids(self):
        self.assertEqual(proto.button_id("menu"), 9)
        self.assertEqual(proto.button_id("15"), 15)
        for bad in ("FOO", "0", "16", 99):
            with self.assertRaises(ValueError):
                proto.button_id(bad)


class TestGuardsAreLoadBearing(unittest.TestCase):
    def test_checksum_comparison_is_what_rejects(self):
        import inspect
        src = inspect.getsource(proto.Decoder.feed)
        guard = "if checksum(body) != self._buf[total - 1]:"
        self.assertIn(guard, src, "guard moved; update this mutation")
        ns = {}
        mutated = inspect.getsource(proto).replace(guard, "if False:")
        exec(compile(mutated, "proto_mutant", "exec"), ns)
        raw = bytearray(proto.encode(0x83, b"hello"))
        raw[-1] ^= 0xFF
        self.assertEqual(len(ns["Decoder"]().feed(bytes(raw))), 1,
                         "mutant should accept the corrupted frame the real decoder rejects")


if __name__ == "__main__":
    unittest.main(verbosity=2)
