"""`openscope` command line (remote_protocol.md §4.2).

Every subcommand exits non-zero with one readable line when the device is
absent or refuses — no tracebacks for expected failures (§4.2)."""
from __future__ import annotations

import argparse
import sys
from typing import List, Optional

from . import proto
from .device import Device, DeviceError, Nak, Timeout
from .link import NoDevice, candidate_ports, looked_where

EXIT_OK, EXIT_NO_DEVICE, EXIT_DEVICE_ERROR, EXIT_USAGE = 0, 1, 2, 64


def _now() -> float:          # the meter staleness clock (tests replace it)
    import time
    return time.monotonic()


def _info(dev: Device, _a) -> int:
    version = dev.ping()
    st = dev.status()
    if st.battery_known:
        batt = f"{st.battery_pct}% ({st.battery_mv} mV{', charging' if st.charging else ''}"
        batt += ", CRITICAL)" if st.battery_critical else ")"
    else:
        batt = "unknown (no sample yet" + (", charging)" if st.charging else ")")
    print(f"port      {dev.link.port}")
    print(f"firmware  {version}")
    print(f"protocol  v{st.proto_version}")
    print(f"mode      {st.mode_name}")
    print(f"battery   {batt}")
    print(f"capture   {'real samples' if st.capture_ready else 'no capture data yet'}")
    print(f"uptime    {st.uptime_ms / 1000:.1f} s")
    print(f"usb       {st.usb_tx_stalls} TX stalls, {st.usb_heals} self-heals")
    return EXIT_OK


def _press(dev: Device, a) -> int:
    for b in a.buttons:
        dev.press(b)
        print(f"pressed {b.upper()}")
    return EXIT_OK


def _meter(dev: Device, a) -> int:
    import csv
    import time as _t
    f = open(a.log, "a", newline="") if a.log else None
    writer = csv.writer(f) if f else None
    if f and f.tell() == 0:
        writer.writerow(["t_unix", "update_count", "value", "unit", "display", "raw_bcd",
                         "decimal_pos", "result", "submode", "ac", "autorange", "hold"])
    continuous = a.count == 0
    last = None
    n = 0
    # A frozen update_count means the meter is not producing readings (wrong
    # mode, meter chip silent). A finite --count must not poll forever.
    stale_limit = max(3.0, 20 * a.interval)
    last_new = _now()
    try:
        while True:
            try:
                m = dev.meter()
            except (Nak, Timeout) as e:
                # A long log must survive a range change (NOT_READY) or one
                # lost reply; a one-shot read reports it.
                if not continuous:
                    raise
                print(f"# {e}", file=sys.stderr, flush=True)
                _t.sleep(max(a.interval, 0.5))
                continue
            if m.update_count == last and not continuous and _now() - last_new > stale_limit:
                print(f"error: meter reading not updating for {stale_limit:.0f} s "
                      f"(update_count stuck at {last})", file=sys.stderr)
                return EXIT_DEVICE_ERROR
            if m.update_count != last:          # only new readings, not re-reads
                last = m.update_count
                last_new = _now()
                print(f"{m.display} {m.unit}   ({m.result}, raw {m.raw_bcd}, #{m.update_count})", flush=True)
                if writer:
                    writer.writerow([f"{_t.time():.3f}", m.update_count, m.value, m.unit, m.display,
                                     m.raw_bcd, m.decimal_pos, m.result, m.submode,
                                     int(m.ac), int(m.autorange), int(m.hold)])
                    f.flush()                   # a killed logger keeps every row it printed
                n += 1
            if not continuous and n >= a.count:
                return EXIT_OK
            _t.sleep(a.interval)
    except KeyboardInterrupt:
        return EXIT_OK
    finally:
        if f:
            f.close()


CSV_FIELDS = ["t_unix", "frame_id", "channel", "index", "sample", "in_head_defect", "t_s",
              "timebase_idx", "sample_rate_hz", "vdiv_idx", "uv_per_div", "counts_per_div", "flags"]
NPZ_NOTE = ("samples: raw unsigned ADC counts [frame, channel, index]. calibrated=0: no per-unit "
            "calibration exists. volts_per_count = uv_per_div / 1e6 / counts_per_div is the bench-"
            "unit-#1 gain of the range (0 = none): amplitudes (Vpp) only, the zero point is not "
            "calibrated. sample_rate_hz 0 = no trustworthy rate. samples[..., :head_skip] are the "
            "known record-head defect (firmware scope_record.h).")


def write_capture_csv(path: str, captures) -> int:
    """Long format, one row per sample, every row self-describing. `captures`
    is [(t_unix, [Waveform, ...]), ...]. Returns the number of sample rows."""
    import csv
    rows = 0
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(CSV_FIELDS)
        for t, waves in captures:
            for wf in waves:
                dt = wf.seconds_per_sample
                for i, v in enumerate(wf.samples):
                    w.writerow([f"{t:.3f}", wf.frame_id, wf.channel + 1, i, v, int(i < wf.head_skip),
                                "" if dt is None else f"{i * dt:.9g}",
                                wf.timebase_idx, wf.sample_rate_hz, wf.vdiv_idx, wf.uv_per_div,
                                wf.counts_per_div, wf.flags])
                    rows += 1
    return rows


def write_capture_npz(path: str, captures) -> None:
    import numpy as np                   # optional: callers check before capturing
    first = captures[0][1]
    if any(len(waves) != len(first) or any(len(x.samples) != len(first[0].samples) for x in waves)
           for _, waves in captures):
        raise ValueError("frames differ in shape; use a .csv output")
    np.savez_compressed(
        path,
        samples=np.array([[np.frombuffer(x.samples, dtype=np.uint8) for x in waves]
                          for _, waves in captures], dtype=np.uint8),
        t_unix=np.array([t for t, _ in captures], dtype=np.float64),
        frame_id=np.array([waves[0].frame_id for _, waves in captures], dtype=np.uint32),
        channels=np.array([x.channel + 1 for x in first], dtype=np.uint8),
        flags=np.array([[x.flags for x in waves] for _, waves in captures], dtype=np.uint8),
        timebase_idx=np.array([waves[0].timebase_idx for _, waves in captures], dtype=np.uint8),
        sample_rate_hz=np.array([waves[0].sample_rate_hz for _, waves in captures], dtype=np.uint32),
        vdiv_idx=np.array([[x.vdiv_idx for x in waves] for _, waves in captures], dtype=np.uint8),
        uv_per_div=np.array([[x.uv_per_div for x in waves] for _, waves in captures], dtype=np.uint32),
        counts_per_div=np.array([waves[0].counts_per_div for _, waves in captures], dtype=np.uint16),
        head_skip=np.array([waves[0].head_skip for _, waves in captures], dtype=np.uint16),
        note=np.array(NPZ_NOTE),
    )


def nak_explained(e: Nak) -> str:
    name = proto.ERRORS.get(e.code, "")
    hint = proto.NAK_HINTS.get(name)
    return f"{e}: {hint}" if hint else str(e)


def _wave_line(wf: proto.Waveform) -> str:
    sm = wf.summary()
    parts = [f"#{wf.frame_id} {wf.channel_name}: {wf.sample_count} samples"]
    if "min_counts" in sm:
        parts.append(f"counts {sm['min_counts']}..{sm['max_counts']} (mean {sm['mean_counts']})")
    if sm.get("vpp_volts") is not None:
        parts.append(f"~{sm['vpp_volts']:.3g} Vpp ({wf.vdiv_tier} gain, uncalibrated)")
    if sm.get("frequency_hz"):
        parts.append(f"~{sm['frequency_hz']:.4g} Hz")
    elif sm.get("period_samples"):
        parts.append(f"period {sm['period_samples']} samples")
    rate = (f"{wf.sample_rate_hz} S/s {wf.timebase_tier}" if wf.sample_rate_hz
            else f"code 0x{wf.timebase_idx:02X}: no trustworthy rate")
    parts.append(rate)
    return ", ".join(parts)


def _scope(dev: Device, a) -> int:
    import time as _t
    mask = proto.channel_mask(a.channels)
    out = a.out
    if out and not out.lower().endswith((".csv", ".npz")):
        print("error: --out must end in .csv or .npz", file=sys.stderr)
        return EXIT_USAGE
    if out and out.lower().endswith(".npz"):
        try:
            import numpy  # noqa: F401
        except ImportError:
            print("error: .npz needs numpy, which is not installed; use a .csv output",
                  file=sys.stderr)
            return EXIT_USAGE
    if a.frames < 1:
        print("error: --frames must be >= 1", file=sys.stderr)
        return EXIT_USAGE
    captures = []
    last_id = None
    # A frame_id that does not advance means acquisition is stopped or held
    # (RUN/STOP, SINGLE done, NORMAL with no trigger). Re-reads are not new
    # captures, so a finite --frames must not poll forever.
    stale_limit = max(3.0, 20 * a.interval)
    last_new = _now()
    status = EXIT_OK
    while len(captures) < a.frames:
        try:
            waves = dev.waveform(mask)
        except Nak as e:
            if proto.ERRORS.get(e.code) == "NOT_READY" and _now() - last_new <= stale_limit:
                _t.sleep(max(a.interval, 0.05))       # no tear-free copy this time
                continue
            print(nak_explained(e), file=sys.stderr)
            status = EXIT_DEVICE_ERROR
            break
        if waves[0].frame_id != last_id:
            last_id = waves[0].frame_id
            last_new = _now()
            captures.append((_t.time(), waves))
            for wf in waves:
                print(_wave_line(wf), flush=True)
        elif _now() - last_new > stale_limit:
            print(f"error: no new capture for {stale_limit:.0f} s (frame_id stuck at {last_id}): "
                  "acquisition is stopped or held (RUN/STOP, SINGLE, NORMAL without a trigger); "
                  f"got {len(captures)} of {a.frames} frames", file=sys.stderr)
            status = EXIT_DEVICE_ERROR
            break
        if len(captures) < a.frames:
            _t.sleep(a.interval)
    if out and captures:
        if out.lower().endswith(".npz"):
            write_capture_npz(out, captures)
        else:
            write_capture_csv(out, captures)
        note = "" if status == EXIT_OK else " (partial)"
        print(f"saved {out}: {len(captures)} frame(s) x {len(captures[0][1])} channel(s){note}")
    return status


def _shell(dev: Device, a) -> int:
    print(dev.shell(" ".join(a.line), timeout=a.timeout))
    return EXIT_OK


def _screenshot(dev: Device, a) -> int:
    from .screen import save_png
    s = dev.screenshot()
    save_png(a.out, s.w, s.h, s.indexed4, scale=a.scale)
    note = "transport verified; the live screen changed during capture (torn)" if s.torn else "CRC verified"
    print(f"saved {a.out} ({s.w}x{s.h}, {note})")
    return EXIT_OK


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="openscope", description="Drive an OpenScope 2C53T over USB.")
    ap.add_argument("--port", help="serial port (default: auto-detect by USB VID:PID)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports", help="list candidate ports")
    sub.add_parser("info", help="firmware version, mode, battery, USB health")
    p = sub.add_parser("press", help="inject button presses, e.g. `press MENU OK`")
    p.add_argument("buttons", nargs="+", metavar="BUTTON",
                   help=" | ".join(proto.BUTTONS))
    p = sub.add_parser("meter", help="print (and optionally log to CSV) multimeter readings")
    p.add_argument("--count", type=int, default=1, help="readings to take (0 = until Ctrl-C)")
    p.add_argument("--interval", type=float, default=0.25, help="poll period, s (meter updates ~4 Hz)")
    p.add_argument("--log", metavar="CSV", help="append readings to this CSV (raw BCD kept, see #28)")
    p = sub.add_parser("scope", help="read captured waveforms (raw ADC counts) and save them")
    p.add_argument("--frames", type=int, default=1, help="distinct captures to take (new frame_id each)")
    p.add_argument("--channels", default="1", help="1, 2 or 1,2 (default 1)")
    p.add_argument("--interval", type=float, default=0.05, help="poll period, s")
    p.add_argument("--out", metavar="FILE", help="save to .csv (always) or .npz (needs numpy)")
    p = sub.add_parser("shell", help="run one ASCII debug-shell command")
    p.add_argument("line", nargs="+")
    p.add_argument("--timeout", type=float, default=3.0)
    p = sub.add_parser("screenshot", help="save the device screen as PNG")
    p.add_argument("out")
    p.add_argument("--scale", type=int, default=2)
    return ap


def main(argv: Optional[List[str]] = None) -> int:
    a = build_parser().parse_args(argv)
    if a.cmd == "ports":
        ports = candidate_ports()
        if not ports:
            print(f"no OpenScope found on {looked_where()}")
            return EXIT_NO_DEVICE
        for i, p in enumerate(ports):
            print(p + ("   <- would use" if i == 0 else ""))
        return EXIT_OK

    handlers = {"info": _info, "press": _press, "meter": _meter, "shell": _shell,
                "screenshot": _screenshot, "scope": _scope}
    dev = None
    try:
        dev = Device.open(a.port)           # also checks the protocol major (§3.7)
        return handlers[a.cmd](dev, a)
    except NoDevice as e:                   # none found, busy, or did not come back
        print(str(e), file=sys.stderr)
        return EXIT_NO_DEVICE
    except Nak as e:
        print(str(e), file=sys.stderr)
        return EXIT_DEVICE_ERROR
    except (DeviceError, proto.ProtocolError, ValueError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT_DEVICE_ERROR
    finally:
        if dev is not None:
            dev.close()
