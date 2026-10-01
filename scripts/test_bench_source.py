#!/usr/bin/env python3
"""Tests for the bench's signal-source abstraction (scripts/bench.py).

WHY THIS EXISTS
---------------
The bench scripts measure the scope against a stimulus, and this project's
worst measurement error so far was the stimulus: the ESP32 generator delivered
0.825x the frequency it was told, and every sample rate fitted against the
COMMANDED number came out 1.21x too high (EXP-14). The abstraction that lets a
Kode Dot or a bench generator stand in for the ESP32 keeps one rule from that:
a source returns what it REPORTS (or what the operator's instruments read),
and a setter that cannot confirm what it asked for raises. These tests pin
that rule down, plus the two pieces of arithmetic that are new with a square
source: its Vpp is NOT Vrms x 2*sqrt(2), and its single fixed amplitude only
fits some ranges.

Nothing here opens a port: every device is a ScriptedTransport, a SimBench,
or a fake list of USB ports.

VERIFIED BY MUTATION
--------------------
Each assertion below was checked by reintroducing the bug it is meant to
catch (in a scratch copy of the scripts) and watching it go red:

  M1  KodeDotSource.freq() skips the req_hz == asked check         -> caught
  M2  a framed reply with no '>ok' is accepted                      -> caught
  M3  a square's Vrms converted with the sine factor 2*sqrt(2)      -> caught
  M4  verify_scope_cal.plan_run() skips the fixed-amplitude check   -> caught
  M5  ManualSource.tone() returns the requested Hz, not the typed   -> caught
  M6  find_port() returns the first of several matches              -> caught
  M7  KodeDotSource.freq_hz reports the request, not the actual     -> caught
  M8  range_coverage() uses the full span for a low-rail-centred
      square (midpoint geometry), so range 5 "fits"                 -> caught
  M9  SimBench NORMAL commits an unstrobed read (no hold)           -> caught
  M10 SimBench offset DAC back to the pre-EXP-64 sign               -> caught
  M11 exp22 runs the edge/order scenarios on a square               -> caught
  M12 exp22 checks against the commanded, not the reported, Hz      -> caught
  M13 SimBench level code with Python floor division (C truncates)  -> caught

Run: python3 scripts/test_bench_source.py
"""

from __future__ import annotations

import argparse
import contextlib
import io
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))

import bench  # noqa: E402
from bench import (BenchError, KodeDotSource, ManualSource, ScriptedTransport,  # noqa: E402
                   expected_span_counts, find_port, vpp_from_reading)
import exp22_stability  # noqa: E402
import measure_sample_rate  # noqa: E402
import verify_scope_cal  # noqa: E402
import numpy as np  # noqa: E402

# What the sigsrc app prints (kodeOS console framing), and the bare form the
# command set was first specified with.
FRAMED_STATUS = (
    ">|out mode=pwm pin=GPIO14 j3_pin=9 exp=7 gnd_j3_pins=10,11\r\n"
    ">|freq hz=1000.000000 req_hz=1000 err_ppm=+0.000 bits=13 div=9.765625 timing=clean\r\n"
    ">|clock src=PLL_F80M clk_hz=80000000 (SPLL=12x X1 40 MHz, /6) div_raw=0x009C4\r\n"
    ">|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%\r\n"
    ">|xtal ppm=+0.000 (nominal 40 MHz assumed)\r\n>|sweep idle\r\n>ok\r\n")
FRAMED_F1000 = (
    ">|freq hz=999.984741 req_hz=1000 err_ppm=-15.259 bits=13 div=9.765625 "
    "timing=frac-edge\r\n>|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%\r\n>ok\r\n")
BARE_F1000 = "freq 999.98 Hz (req 1000, clk 80000000 res 13 div 9765.6)\r\n"


def dot(replies: dict, **kw) -> KodeDotSource:
    """A KodeDotSource on a scripted link; `s` always answers with a status."""
    table = {"s": FRAMED_STATUS}
    table.update(replies)
    return KodeDotSource(transport=ScriptedTransport(table), **kw)


def manual(answers, waveform="sine") -> tuple:
    """A ManualSource whose operator types `answers` in order."""
    it = iter(answers)
    asked: list = []
    printed: list = []

    def input_fn(prompt):
        asked.append(prompt)
        return next(it)
    return ManualSource(waveform=waveform, input_fn=input_fn,
                        print_fn=printed.append), asked, printed


class ManualSourceTests(unittest.TestCase):
    def test_tone_returns_the_frequency_the_operator_reads(self):
        src, asked, _ = manual(["", "999.98"])
        self.assertAlmostEqual(src.tone(1000), 999.98)
        self.assertAlmostEqual(src.freq_hz, 999.98)
        self.assertIn("1000 Hz", asked[0])           # told what to set
        self.assertIn("ACTUAL frequency", asked[1])  # then asked what it reads

    def test_tone_accepts_counter_style_units(self):
        src, _, _ = manual(["", "1.0001 kHz"])
        self.assertAlmostEqual(src.tone(1000), 1000.1)
        src, _, _ = manual(["", "2 MHz"])
        self.assertAlmostEqual(src.tone(2e6), 2e6)

    def test_garbage_and_typos_are_asked_again_not_guessed(self):
        # "abc" does not parse; "100" is 90 % off a 1 kHz request (a typo).
        src, asked, printed = manual(["", "abc", "100", "1001"])
        self.assertAlmostEqual(src.tone(1000), 1001.0)
        self.assertEqual(len(asked), 4)
        self.assertTrue(any("could not read" in p for p in printed))
        self.assertTrue(any("typo" in p for p in printed))

    def test_gives_up_after_the_attempt_limit(self):
        src, _, _ = manual(["", "x", "y", "z"])
        with self.assertRaises(BenchError):
            src.tone(1000)

    def test_amplitude_needs_a_unit(self):
        # A bare 3.292 could be Vpp, Vrms or a DC level — a 2.8x ambiguity.
        src, asked, printed = manual(["", "3.292", "3.292 Vpp"], waveform="square")
        self.assertAlmostEqual(src.drive(3300), 3292.0)
        self.assertEqual(len(asked), 3)
        self.assertTrue(any("unit" in p for p in printed))

    def test_amplitude_conversion_follows_the_waveform(self):
        src, _, _ = manual(["", "1.646 Vrms"], waveform="square")
        self.assertAlmostEqual(src.drive(3300), 3292.0)     # 2 x Vrms
        src, _, _ = manual(["", "3.292 V"], waveform="square")
        self.assertAlmostEqual(src.drive(3300), 3292.0)     # DC high level
        src, _, _ = manual(["", "707.1 mVrms"], waveform="sine")
        self.assertAlmostEqual(src.drive(2000), 2000.0, delta=0.1)  # 2.83 x Vrms

    def test_quiet_does_not_ask_twice_in_a_row(self):
        src, asked, _ = manual(["", ""])
        src.quiet()
        src.quiet()
        self.assertEqual(len(asked), 1)


class KodeDotTests(unittest.TestCase):
    def test_framed_reply_gives_the_actual_frequency(self):
        src = dot({"f 1000": FRAMED_F1000})
        st = src.freq(1000)
        self.assertAlmostEqual(st.hz, 999.984741)
        self.assertEqual((st.req_hz, st.bits, st.timing), (1000, 13, "frac-edge"))
        self.assertAlmostEqual(st.duty_pct, 50.0)

    def test_bare_reply_as_first_specified_also_parses(self):
        src = dot({"f 1000": BARE_F1000})
        st = src.freq(1000)
        self.assertAlmostEqual(st.hz, 999.98)
        self.assertEqual((st.req_hz, st.bits, st.clk_hz), (1000, 13, 80_000_000))
        self.assertAlmostEqual(st.div, 9765.6)

    def test_freq_hz_is_the_reported_actual_not_the_request(self):
        src = dot({"f 1000": FRAMED_F1000})
        self.assertAlmostEqual(src.tone(1000), 999.984741)
        self.assertAlmostEqual(src.freq_hz, 999.984741)
        self.assertNotEqual(src.freq_hz, 1000.0)

    def test_a_setting_the_device_did_not_adopt_raises(self):
        not_adopted = FRAMED_F1000.replace("req_hz=1000", "req_hz=999")
        with self.assertRaisesRegex(BenchError, "asked for 1000 Hz"):
            dot({"f 1000": not_adopted}).freq(1000)
        far = FRAMED_F1000.replace("hz=999.984741", "hz=500.000000")
        with self.assertRaisesRegex(BenchError, "actual"):
            dot({"f 1000": far}).freq(1000)

    def test_err_reply_raises_with_the_message(self):
        with self.assertRaisesRegex(BenchError, "no_ledc_setting"):
            dot({"f 1000": ">err msg=no_ledc_setting_for_1000_hz\r\n"}).freq(1000)

    def test_framed_reply_without_ok_is_not_success(self):
        truncated = FRAMED_F1000.replace(">ok\r\n", "")
        with self.assertRaisesRegex(BenchError, ">ok"):
            dot({"f 1000": truncated}).freq(1000)

    def test_unparsable_reply_raises(self):
        with self.assertRaises(BenchError):
            dot({"f 1000": "I (1234) kodeos: something else\r\n"}).freq(1000)

    def test_fractional_hz_is_refused_before_anything_is_sent(self):
        t = ScriptedTransport({"s": FRAMED_STATUS})
        src = KodeDotSource(transport=t)
        sent = len(t.log)
        with self.assertRaisesRegex(BenchError, "whole hertz"):
            src.freq(12.5)
        self.assertEqual(len(t.log), sent)

    def test_dc_and_duty_are_confirmed(self):
        src = dot({"dc 1": ">|out mode=dc_high pin=GPIO14 j3_pin=9\r\n>ok\r\n",
                   "dc 0": ">|out mode=dc_high pin=GPIO14 j3_pin=9\r\n>ok\r\n",
                   "d 50": ">|duty=50.0000% req=50.000% count=4096/8192 step=0.0122%\r\n>ok\r\n",
                   "d 25": ">|duty=50.0000% req=25.000% count=4096/8192 step=0.0122%\r\n>ok\r\n"})
        self.assertEqual(src.dc(1).mode, "dc_high")
        with self.assertRaisesRegex(BenchError, "dc_low"):
            src.dc(0)                                   # echo says still high
        self.assertAlmostEqual(src.duty(50).duty_pct, 50.0)
        with self.assertRaises(BenchError):
            src.duty(25)

    def test_a_port_that_is_not_running_sigsrc_is_refused(self):
        with self.assertRaisesRegex(BenchError, "sigsrc app"):
            KodeDotSource(transport=ScriptedTransport({"s": "kodeOS ready\r\n"}))

    def test_drive_has_one_amplitude(self):
        src = dot({"f 330": FRAMED_F1000.replace("999.984741", "330.000330")
                   .replace("req_hz=1000", "req_hz=330")}, v3v3_mv=3292.0)
        self.assertEqual(src.drive(3300), 3292.0)        # within 5 %: the rail
        with self.assertRaisesRegex(BenchError, "only amplitude"):
            src.drive(1000)


class FormulaTests(unittest.TestCase):
    GAIN = 20.0           # a made-up range: 20 mV per count

    def test_square_expected_counts_come_from_vpp(self):
        vpp_dc = vpp_from_reading(3292.0, "dc", "square")      # DMM on the high rail
        vpp_rms = vpp_from_reading(1646.0, "rms", "square")    # true-RMS DMM, AC
        self.assertAlmostEqual(expected_span_counts(vpp_dc, self.GAIN), 164.6)
        self.assertAlmostEqual(expected_span_counts(vpp_rms, self.GAIN), 164.6)

    def test_square_is_not_converted_like_a_sine(self):
        sine = expected_span_counts(vpp_from_reading(1000.0, "rms", "sine"), self.GAIN)
        square = expected_span_counts(vpp_from_reading(1000.0, "rms", "square"), self.GAIN)
        self.assertAlmostEqual(sine, 141.421, places=3)        # 2*sqrt(2) x Vrms
        self.assertAlmostEqual(square, 100.0)                   # 2 x Vrms at 50 %
        self.assertAlmostEqual(sine / square, 2 ** 0.5, places=6)

    def test_peak_to_peak_is_waveform_independent_and_duty_matters_for_rms(self):
        self.assertEqual(vpp_from_reading(2000.0, "pp", "sine"),
                         vpp_from_reading(2000.0, "pp", "square"))
        # 25 % duty: AC rms = Vpp * sqrt(0.25 * 0.75)
        self.assertAlmostEqual(vpp_from_reading(1000.0 * 0.75 ** 0.5 * 0.5, "rms",
                                                "square", duty=0.25), 1000.0)

    def test_meaningless_conversions_raise(self):
        with self.assertRaises(BenchError):
            vpp_from_reading(3292.0, "dc", "sine")
        with self.assertRaises(BenchError):
            expected_span_counts(3292.0, 0.0)                   # no-cal range

    def test_coverage_of_a_low_rail_centred_square(self):
        # Made-up table: r0 no cal, then 30 / 20 / 10 / 1 ... mV per count.
        table = {ch: {0: 0.0, 1: 1.0, 2: 10.0, 3: 20.0, 4: 30.0, 5: 300.0,
                      6: 0.0, 7: 0.0, 8: 0.0, 9: 0.0} for ch in (1, 2)}
        cov, head = verify_scope_cal.range_coverage(table, 1.0, 3000.0, midpoint=False)
        self.assertEqual(head, 255 - 128 - verify_scope_cal.CLIP_MARGIN)
        self.assertEqual(cov[0][0], "nocal")
        self.assertEqual(cov[1][0], "clips")        # 3000 counts
        self.assertEqual(cov[2][0], "clips")        # 300 counts > 119
        self.assertEqual(cov[3][0], "clips")        # 150 counts > 119 (half span!)
        self.assertEqual(cov[4][0], "ok")           # 100 counts
        self.assertEqual(cov[5][0], "coarse")       # 10 counts
        cov_mid, head_mid = verify_scope_cal.range_coverage(table, 1.0, 3000.0, midpoint=True)
        self.assertEqual(cov_mid[3][0], "ok")       # 150 < 240 once centred on the midpoint
        self.assertAlmostEqual(cov[4][1][1], 100.0)


class CliTests(unittest.TestCase):
    def run_cli(self, mod, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mod.main(argv)
        return out.getvalue()

    def refused(self, mod, argv) -> str:
        """Run a CLI that must refuse BEFORE opening anything."""
        real = mod.open_bench
        mod.open_bench = lambda *a, **k: self.fail("a device was opened before refusing")
        try:
            with self.assertRaises(SystemExit) as cm:
                mod.main(argv)
        finally:
            mod.open_bench = real
        return str(cm.exception.code)

    def test_kodedot_refuses_an_amplitude_it_cannot_produce(self):
        msg = self.refused(verify_scope_cal, ["--source", "kodedot", "--amp", "1000", "2000"])
        self.assertIn("cannot produce 1000 mVpp and 2000 mVpp", msg)
        self.assertIn("3292 mVpp", msg)
        self.assertRegex(msg, r"fits\s+r6 .*r7")            # which ranges it covers
        self.assertRegex(msg, r"clips\s+r4 .*r5")
        self.assertIn("no cal   r0 r1 r2 r3", msg)
        msg = self.refused(verify_scope_cal, ["--source", "kodedot", "--amp", "1000"])
        self.assertIn("cannot produce 1000 mVpp", msg)

    def test_kodedot_refuses_ranges_its_square_would_clip(self):
        msg = self.refused(verify_scope_cal, ["--source", "kodedot", "--ranges", "5", "6"])
        self.assertIn("range(s) 5", msg)

    def test_kodedot_refuses_a_sine_and_esp32_refuses_over_its_dac(self):
        self.assertIn("square only", self.refused(
            verify_scope_cal, ["--source", "kodedot", "--waveform", "sine"]))
        self.assertIn("cannot produce", self.refused(
            verify_scope_cal, ["--amp", "1000", "4000"]))

    def test_the_rail_amplitude_is_accepted_and_the_measurement_used(self):
        out = self.run_cli(verify_scope_cal, ["--source", "kodedot", "--amp", "3300",
                                              "--v3v3", "3.292", "--reps", "1", "--dry-run"])
        self.assertIn("reference 3292 mVpp", out)
        self.assertIn("control PASSED", out)

    def test_sample_rate_flags_need_codes(self):
        self.assertIn("give --codes", self.refused(measure_sample_rate, ["--tones", "100"]))

    def test_a_script_can_offer_its_own_instrument_as_a_source(self):
        ap = argparse.ArgumentParser()
        bench.add_source_args(ap, default="jds6600", kinds=("jds6600", "kodedot"))
        self.assertEqual(ap.parse_args([]).source, "jds6600")
        self.assertEqual(ap.parse_args(["--source", "kodedot"]).source, "kodedot")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            ap.parse_args(["--source", "esp32"])                # not offered here
        args = ap.parse_args([])
        with self.assertRaisesRegex(BenchError, "unknown source 'jds6600'"):
            bench.open_source(args)                             # the script opens it
        args.dry_run = True
        with self.assertRaisesRegex(BenchError, "SimBench source"):
            bench.open_bench(args)

    def exp22_refused(self, argv) -> str:
        """exp22 must refuse with a usage error BEFORE opening any device."""
        mod = exp22_stability
        real = (mod.open_bench, mod.Scope, mod.JDS6600)
        mod.open_bench = mod.Scope = mod.JDS6600 = \
            lambda *a, **k: self.fail("a device was opened before refusing")
        err = io.StringIO()
        try:
            with contextlib.redirect_stderr(err), self.assertRaises(SystemExit) as cm:
                mod.main(argv)
        finally:
            mod.open_bench, mod.Scope, mod.JDS6600 = real
        self.assertEqual(cm.exception.code, 2)
        return err.getvalue()

    def test_exp22_kodedot_is_trigger_block_only_and_jds_has_no_dry_run(self):
        self.assertIn("pass --trigger-only", self.exp22_refused(["--source", "kodedot"]))
        self.assertIn("pass --trigger-only", self.exp22_refused(
            ["--source", "kodedot", "--trigger-only", "--no-trigger"]))
        self.assertIn("no simulator", self.exp22_refused(["--trigger-only", "--dry-run"]))


class PortDiscoveryTests(unittest.TestCase):
    @staticmethod
    def port(dev, vid, pid, sn=None):
        return SimpleNamespace(device=dev, vid=vid, pid=pid, serial_number=sn, location=None)

    def test_vendor_id_separates_scope_and_dot(self):
        ports = [self.port("/dev/cu.usbmodem101", 0x303A, 0x1001, "E8:F6"),
                 self.port("/dev/cu.usbmodem2101", 0x2E3C, 0x5740)]
        self.assertEqual(find_port(0x303A, ports=ports), "/dev/cu.usbmodem101")
        self.assertEqual(find_port(*bench.SCOPE_USB_ID, ports=ports), "/dev/cu.usbmodem2101")

    def test_two_dots_need_a_serial_number(self):
        ports = [self.port("/dev/cu.usbmodem101", 0x303A, 0x1001, "AAA"),
                 self.port("/dev/cu.usbmodem201", 0x303A, 0x1001, "BBB")]
        with self.assertRaisesRegex(BenchError, "2 ports match"):
            find_port(0x303A, ports=ports)
        self.assertEqual(find_port(0x303A, serial_number="BBB", ports=ports),
                         "/dev/cu.usbmodem201")

    def test_no_match_lists_what_was_seen(self):
        with self.assertRaisesRegex(BenchError, r"usbserial-1 \(10c4:ea60\)"):
            find_port(0x303A, ports=[self.port("/dev/cu.usbserial-1", 0x10C4, 0xEA60)])

    def test_cu_and_tty_of_one_device_are_one_device(self):
        ports = [self.port("/dev/tty.usbmodem101", 0x303A, 0x1001, "AAA"),
                 self.port("/dev/cu.usbmodem101", 0x303A, 0x1001, "AAA")]
        self.assertEqual(find_port(0x303A, ports=ports), "/dev/cu.usbmodem101")


class DryRunTests(unittest.TestCase):
    """Each script's whole flow against SimBench, for every source."""

    def run_cli(self, mod, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mod.main(argv + ["--dry-run"])
        return out.getvalue()

    def test_verify_scope_cal_every_source(self):
        for argv in (["--source", "esp32"], ["--source", "kodedot"],
                     ["--source", "kodedot", "--center-mid"],
                     ["--source", "manual", "--waveform", "square"]):
            with self.subTest(argv=argv):
                out = self.run_cli(verify_scope_cal, argv + ["--reps", "2"])
                self.assertIn("control PASSED", out)
                self.assertIn("consistent", out)
                self.assertNotIn("INCONSISTENT", out)
        out = self.run_cli(verify_scope_cal, ["--source", "kodedot", "--center-mid",
                                              "--reps", "2"])
        self.assertRegex(out, r"\n 5  1 \|")                 # range 5 is reachable
        self.assertIn("static-level", out)

    def test_measure_sample_rate_every_source(self):
        out = self.run_cli(measure_sample_rate, ["--source", "esp32"])
        self.assertIn("=== 3. fast codes, acq-buffer read ===", out)   # historical run
        self.assertIn("paths agree", out)
        for src in ("kodedot", "manual"):
            with self.subTest(source=src):
                out = self.run_cli(measure_sample_rate, ["--source", src, "--codes",
                                                         "0x0C", "0x0B"])
                self.assertIn("paths agree", out)
                self.assertEqual(out.count("FOLD HOLDS"), 2)
                self.assertIn("done", out)



    def test_exp22_trigger_block_kodedot(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), self.assertRaises(SystemExit) as cm:
            exp22_stability.main(["--source", "kodedot", "--trigger-only", "--dry-run"])
        text = out.getvalue()
        self.assertEqual(cm.exception.code, 0, text[-2000:])
        self.assertIn("OVERALL: PASS -- trigger block on a square-only", text)
        self.assertIn("11 scenarios run, all pass; 10 SKIPPED", text)
        self.assertNotIn("FAIL", text)
        # every skip says why, at the scenario and in the summary
        self.assertEqual(text.count("\n  SKIP  SEAM --"), 6)
        self.assertEqual(text.count("\n  SKIP  HPOS --"), 4)
        self.assertEqual(text.count("SKIP (square:"), 10)
        # sines are run as squares, named so, checked against the REPORTED Hz
        self.assertIn("NORMAL 0x10 sine-as-square PASS", text)
        self.assertIn("vs 200.997 reported by the source", text)
        self.assertEqual(text.count("PASS  body"), 3)          # 0x10, 0x11, 0x12
        self.assertIn("negctl-strobe", text)                   # the hold metric's control ran


class SimAcquisitionTests(unittest.TestCase):
    """bench.SimBench's acquisition loop: the policy exp22's trigger block
    checks, through the same parsers exp22 uses (grab_frame, read_status)."""

    def setUp(self):
        self.sim = bench.SimBench(source="kodedot")
        self.sc = self.sim.scope()
        self.sim.source().tone(201)
        self.sc.vdiv(1, 5)
        self.sc.cmd("trig raw 1504")          # record midline ~114 (exp22's host centre)

    def grab(self):
        return exp22_stability.grab_frame(self.sc)

    def run_mode(self, mode, level, n=4):
        self.sc.trigger_level(level)
        self.sc.trigger_mode(mode)
        st0 = exp22_stability.read_status(self.sc)
        gens = [self.grab()["gen"] for _ in range(n)]
        st1 = exp22_stability.read_status(self.sc)
        return gens, st1["edges"] - st0["edges"], st1["ok"] - st0["ok"]

    def test_level_code_is_the_firmware_mapping(self):
        # fpga_trigger_code_from_level(): 128 + level*256/206, C truncation
        self.assertEqual([bench.SimBench.trig_code(v) for v in (-100, -1, 0, 1, 30, 100)],
                         [4, 127, 128, 129, 165, 252])
        self.assertIn("reg 0x08 code 0xA5 in force", self.sc.trigger_level(30))

    def test_normal_holds_above_the_signal_and_auto_falls_back(self):
        gens, edges, ok = self.run_mode("normal", 100)
        self.assertEqual((len(set(gens)), edges, ok), (1, 0, 0))
        gens, edges, ok = self.run_mode("auto", 100)            # the fallback
        self.assertEqual((len(set(gens)), edges, ok), (4, 0, 4))
        gens, edges, ok = self.run_mode("normal", 0)
        self.assertEqual((len(set(gens)), edges, ok), (4, 4, 4))  # every commit strobed

    def test_single_takes_one_record_until_the_mode_is_selected_anew(self):
        self.sc.trigger_level(0)
        self.sc.trigger_mode("auto")
        g0 = self.grab()["gen"]
        self.sc.trigger_mode("single")
        self.assertEqual([self.grab()["gen"] for _ in range(3)], [g0 + 2] * 3)
        self.sc.trigger_mode("single")                           # same mode: no re-arm
        self.assertEqual(self.grab()["gen"], g0 + 2)

    def test_a_strobed_record_crosses_the_level_at_512(self):
        self.sc.trigger_level(0)
        self.sc.trigger_mode("normal")
        v = np.asarray(self.grab()["ch1"], float)
        lvl = 128 - 28                                           # record units
        self.assertTrue((v[500:510].mean() - lvl) * (v[514:524].mean() - lvl) < 0)
        self.assertLessEqual(v.max(), 227)                       # ADC - 28

    def test_a_higher_offset_dac_code_raises_the_trace(self):
        # EXP-64 run 2, unit #3: about 8 DAC codes per count, rising
        self.sc.trigger_mode("auto")
        self.sc.cmd("trig raw 1400")
        lo = np.mean(self.grab()["ch1"])
        self.sc.cmd("trig raw 1600")
        hi = np.mean(self.grab()["ch1"])
        self.assertAlmostEqual(hi - lo, 200 / 8.0, delta=3.0)


class TestScopeOpreadReconnect(unittest.TestCase):
    """EXP-66: a window read across the device's CDC self-heal comes back
    EMPTY (the port vanished mid-command). Scope.opread() may reopen once and
    retry; a SHORT window (torn record) is never retried."""

    @staticmethod
    def _dump(n):
        lines = []
        for off in range(0, n, 16):
            chunk = " ".join("%02x" % ((off + i) & 0xFF) for i in range(min(16, n - off)))
            lines.append("%04X: %s" % (off, chunk))
        return "spi3 opread 04 %d dump\r\n" % n + "\r\n".join(lines) + "\r\n> "

    def _scope(self, replies):
        calls = {"n": 0}

        def reply(line):
            calls["n"] += 1
            r = replies[min(calls["n"], len(replies)) - 1]
            if isinstance(r, Exception):
                raise r
            return r
        return bench.Scope(transport=bench.ScriptedTransport(reply)), calls

    def test_empty_window_retries_once_after_a_reconnect(self):
        sc, calls = self._scope([bench.PromptTimeout("port gone"), self._dump(64)])
        sc._reconnect = lambda wait_s=30.0: True          # the port came back
        v = sc.opread(0x04, n=64)
        self.assertEqual(len(v), 62)                     # 64 minus the 2 header bytes
        self.assertEqual(calls["n"], 2)

    def test_empty_window_without_a_port_is_an_error_that_names_the_cause(self):
        sc, calls = self._scope([bench.PromptTimeout("no prompt b'>' within 6.4s")])
        sc._reconnect = lambda wait_s=30.0: False         # device never came back
        with self.assertRaises(bench.ShortReadError) as cm:
            sc.opread(0x04, n=64)
        self.assertIn("parsed 0", str(cm.exception))
        self.assertIn("no prompt", str(cm.exception))
        self.assertEqual(calls["n"], 1)

    def test_a_short_window_is_never_retried(self):
        sc, calls = self._scope([self._dump(48), self._dump(64)])
        sc._reconnect = lambda wait_s=30.0: self.fail("a torn window must not trigger a reconnect")
        with self.assertRaises(bench.ShortReadError):
            sc.opread(0x04, n=64)
        self.assertEqual(calls["n"], 1)

    def test_a_scripted_transport_never_reconnects(self):
        sc, _ = self._scope([bench.PromptTimeout("x")])
        self.assertFalse(sc._reconnect(wait_s=0.0))


if __name__ == "__main__":
    unittest.main()
