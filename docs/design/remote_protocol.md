# OpenScope Remote Protocol — design spec

**Status:** design, not implemented. Written 2026-08-13 in answer to
[issue #10](https://github.com/DavidClawson/OpenScope-2C53T/issues/10) (PC remote view/control).

**Audience:** anyone who wants to build the host-side tool or the firmware-side endpoint.
There is a concrete, evening-sized first task at the end of this document.

---

## 1. What this is for

Put the instrument on a real screen. Three things, roughly in order of how soon they are
buildable:

1. **Meter logging** — stream multimeter readings to a PC, plot and log them for hours.
   Buildable *today*: the meter data path works.
2. **Waveform view** — stream captured scope frames to a PC and render them larger than
   320×240. Buildable *now that cold-boot capture works* (2026-08-13), pending the caveats in §2.
3. **Remote control** — drive mode / timebase / range / trigger from the host, including
   synthetic button presses.

**Non-goals for v1:** firmware update over this channel (the HID bootloader and
`scripts/iap_flash.py` already do that), file transfer, and anything that needs the device to
initiate a connection.

---

## 2. Grounding — what already exists

Everything below is in the tree today. Read these before designing anything new.

### 2.1 The CDC debug shell — `firmware/src/drivers/usb_debug.c`

A USB CDC-ACM virtual serial port with a line-oriented ASCII command shell.

| Property | Value | Where |
|---|---|---|
| Class | CDC ACM, full speed | `usb_debug_init()`, `usb_debug.c:95` |
| Endpoint max packet | 64 bytes in and out | `cdc_class.h:58-59` |
| USB clock | HICK (internal RC) + ACC trimmed against SOF | `usb_debug.c:103-110` |
| Line terminator | `\r` or `\n`; prompt is `"> "` | `shell_feed()`, `usb_debug.c:2697` |
| Max command length | 128 bytes (`CMD_BUF_SIZE`) | `usb_debug.c:2688` |
| Echo | on for USB, off for RTT | `shell_feed(..., echo)` |
| Shell task | priority 2, 512-word stack, polls every 10 ms when idle | `usb_debug.c:2822-2831` |
| Second transport | SEGGER RTT over SWD, same shell | `usb_debug.c:145-175`, `drivers/rtt.c` |

Baud rate is meaningless on CDC ACM — the host may open at any rate.
`scripts/serial_cmd.py` opens at 115200 out of habit.

The shell already exposes ~60 commands (`cmd_help`, `usb_debug.c:404`). The useful ones for a
remote tool: `version`, `status`, `uptime`, `gpio read`, `fpga cmd`, `trig`, `spi3 acqread`,
`reboot bootloader`.

**Host-side precedent already in the repo:** `scripts/serial_cmd.py` finds the port, flushes the
banner, writes `cmd + "\r\n"`, and drains until quiet. It is 70 lines and it works. The host tool
described here should start as a proper version of that file, not a rewrite from zero.

### 2.2 The binary framing already written — `firmware/src/drivers/esp_comm.{c,h}`

This is the important find. A binary packet protocol was designed for the (unbuilt) ESP32
co-processor, and **the framing layer is implemented and compiled into every build**
(`firmware/Makefile:47`). The command handlers are mostly `TODO` stubs, but the parser, the
encoder and the checksum are real code:

```
[0xAA] [cmd] [len_hi] [len_lo] [payload …] [checksum]
checksum = XOR of every byte from cmd through the end of payload
```

- `esp_comm_receive_byte()` — byte-at-a-time state machine, `esp_comm.c:77`
- `esp_comm_send_response()` — encoder, `esp_comm.c:144`
- `esp_comm_set_writer()` — the transport is a single `void (*)(uint8_t)` function pointer,
  `esp_comm.c:70`. **Nothing currently sets it**, so the protocol is inert.

Two asymmetries worth knowing before you design on top of it:

- **Receive is capped at 256 bytes** of payload (`ESP_MAX_PAYLOAD`, checked at
  `esp_comm.c:104`). **Transmit is not capped** — `esp_comm_send_response()` takes a length and
  a pointer and streams it. So device→host bulk (waveform frames) can be a single large packet;
  host→device must stay ≤ 256 or the parser drops it silently.
- Button IDs `ESP_BTN_CH1..ESP_BTN_POWER` (1..15) are numerically identical to `button_id_t` in
  `ui.h:41`, so remote button injection needs no translation table.

**This spec adopts that framing rather than inventing a new one.** It is already written, already
compiled, already documented (`docs/design/esp32_coprocessor.md`), and reusing it means the
eventual WiFi/BLE path and the USB path speak the same language.

### 2.3 The data the device can actually produce

| Source | Shape | Where |
|---|---|---|
| Scope samples | two 1024-byte buffers, unsigned 8-bit, one per channel | `FPGA_ADC_BUF_SIZE`, `fpga.h:29`; `fpga_get_ch1_buf()` / `fpga_get_ch2_buf()`, `fpga.h:496` |
| Capture validity | `fpga_data_ready()` — true after ≥1 successful SPI3 acquisition | `fpga.h:490` |
| Meter reading | `meter_reading_t` — value, BCD digits, unit string, flags, `update_count` | `drivers/meter_data.h` |
| Scope settings | `scope_state_t` — per-channel vdiv/coupling/probe, trigger, timebase index | `ui/scope_state.h` |
| Device mode | `current_mode`, `meter_submode`, `meter_layout` | `ui/ui.h:76-83` |

**Honesty requirements for anything that ships these over the wire:**

- Scope samples are **raw ADC counts with placeholder calibration**. Per `CLAUDE.md`, the
  baseline is ~55 and per-range calibration has not been done (bench-gated, plan §F2). A host
  tool must present volts as *uncalibrated* until real cal lands, or present counts.
- The scope UI draws a **synthetic square wave** when no real data has arrived
  (`scope_ui.c:379`), latched off permanently by the first real sample (`scope_ui.c:337`).
  **A remote viewer must never stream the demo waveform as if it were a capture.** Gate every
  waveform frame on `fpga_data_ready()` and set an explicit `synthetic` flag if you ever send
  fallback data at all. This project's recurring failure mode is instruments that report
  confidently on state they cannot observe; do not add another one.
- Meter absolute accuracy has only ever been verified on bench unit #1, and the low-Ω scale
  factor is hardcoded per-device (`meter_data.c`). Ship the raw BCD alongside the scaled value
  so a host can re-derive it.
- CH2 is unverified — its trigger reference is believed to need TMR13 CH1 PWM on PA6, which the
  firmware never programs (plan §F3). Expect CH2 to be flat.

### 2.4 The transport quirk you must design around

**USB CDC enumeration is unreliable and the cause is unresolved.** See `CLAUDE.local.md` for the
full history. Current state of knowledge:

- The correlation is with **build type**, 5/5 on bench unit #1: builds that run the bit-bang FPGA
  config path (`guest-coldtrace`) enumerated; builds that run the hardware-SPI config path did
  not, failing with the host-side `error -71` "device not responding to setup address".
- Two earlier theories — temperature, and reset type — were each tested and **refuted**. Do not
  re-derive them.
- Mechanism is **not established**, only the correlation. A single-variable test is queued as
  plan §F1 (maintainer, bench-gated).
- The bootloader's USB (HID/MSC) is solid throughout, so cable, port and PHY are excluded.

**Consequences for this design, and they are not optional:**

1. The host tool must treat "no device found" as a normal outcome, not a crash. Enumerate,
   report clearly which port it looked at, and exit non-zero with a readable message.
2. It must not assume a stable device path. `scripts/serial_cmd.py`'s glob approach
   (`/dev/ttyACM*`, `/dev/cu.usbmodem*`) is the right pattern; add a `--port` override.
3. It must tolerate the port disappearing mid-session (device reboot, IAP flash) and reconnect
   rather than die.
4. The protocol must be **stateless per command** so a reconnect loses nothing but the
   in-flight request.
5. Anything time-critical needs a timeout with a real error, not an infinite block. Note that
   `usb_send_bytes()` on the firmware side already gives up after a TX timeout and silently
   drops output (`usb_debug.c:168`) — a truncated response is a possible failure mode, so the
   host must be able to detect a short/incomplete packet. The checksum does this.

---

## 3. Design

### 3.1 Layering

```
  host tool (Python)            firmware
  ┌────────────────┐            ┌──────────────────────────┐
  │ openscope.cli  │            │ command handlers         │
  ├────────────────┤            ├──────────────────────────┤
  │ openscope.proto│  packets   │ esp_comm dispatch        │
  ├────────────────┤ ─────────► ├──────────────────────────┤
  │ openscope.link │  bytes     │ esp_comm framing (exists)│
  ├────────────────┤            ├──────────────────────────┤
  │ pyserial       │            │ CDC endpoint / RTT       │
  └────────────────┘            └──────────────────────────┘
```

The `link` layer is swappable by design: CDC today, the ESP32 UART bridge later, RTT for
maintainer debugging. Nothing above `link` may assume USB.

### 3.2 Coexistence with the ASCII shell — the one real integration decision

Both the shell and the binary protocol want the same CDC OUT endpoint. Three options were
considered:

| Option | Verdict |
|---|---|
| Separate CDC interface (composite device) | Cleanest, but touches USB descriptors — high risk for a first contribution, and descriptor changes are exactly the sort of thing that could interact with the unresolved enumeration bug. **No.** |
| Escape sequence to switch the shell into binary mode | Stateful; a reconnect mid-session leaves the device in the wrong mode. **No.** |
| **Sniff the sync byte in the RX path** | **Yes.** |

**Chosen:** in `vUsbDebugTask`, before handing a received buffer to `shell_feed()`, feed bytes to
`esp_comm_receive_byte()` whenever a packet is in progress or the next byte is `0xAA`. Packets
are self-delimiting and checksummed, so a malformed one resyncs on the next `0xAA`.

Note the interaction to handle carefully: `shell_feed()` accepts any byte `>= ' '`
(`usb_debug.c:2715`), and `char` is unsigned on ARM EABI, so `0xAA` would otherwise be appended
to the command buffer as text. The sniffing must happen *before* `shell_feed()`, not after.
Conversely, ASCII commands never contain `0xAA`, so the shell is unaffected.

Keep the ASCII shell. It is how the bench is debugged, it is how `scripts/serial_cmd.py` works,
and a human with `screen` is a valuable fallback when the binary tool misbehaves.

### 3.3 Packet layout

Unchanged from `esp_comm.h`:

```
byte 0      0xAA        sync
byte 1      cmd         command or response code
byte 2..3   len         payload length, big-endian
byte 4..    payload
last        checksum    XOR of bytes 1..(n-1)
```

Multi-byte values inside payloads are **little-endian** (matches the MCU, avoids byte-swapping
on the hot path). The length field is the one exception and stays big-endian because the
existing parser already decodes it that way (`esp_comm.c:93-100`). Yes, that is inconsistent;
changing it would break a working parser for cosmetic gain.

### 3.4 Command set

Existing codes are kept as-is. New codes for remote view/control start at `0x20` to leave room.

#### Host → device

| Code | Name | Payload | Notes |
|---|---|---|---|
| `0x01` | `PING` | — | exists; replies `DATA` with the version string |
| `0x08` | `STATUS` | — | exists; **handler currently returns hardcoded mode 0 and battery 100** (`esp_comm.c:201-202`) — fixing that is the first task, see §6 |
| `0x0A` | `BUTTON` | `u8 button_id` | exists; **handler is a TODO stub** (`esp_comm.c:222`) — needs to `xQueueSend` to the input queue |
| `0x20` | `SUBSCRIBE` | `u8 stream_id`, `u16 interval_ms` | 0 interval = unsubscribe |
| `0x21` | `GET_METER` | — | one-shot meter reading |
| `0x22` | `GET_WAVEFORM` | `u8 channel_mask` | bit0 = CH1, bit1 = CH2 |
| `0x23` | `GET_SCOPE_STATE` | — | full `scope_state_t` snapshot |
| `0x24` | `SET_MODE` | `u8 device_mode` | 0=scope 1=meter 2=siggen 3=settings |
| `0x25` | `SET_TIMEBASE` | `u8 index` | index into `timebase_table[]`, 0..20 |
| `0x26` | `SET_VDIV` | `u8 channel`, `u8 index` | index into `vdiv_table[]`, 0..9 |
| `0x27` | `SET_TRIGGER` | `u8 mode`, `u8 edge`, `u8 source`, `i16 level` | mirrors `trigger_state_t` |
| `0x28` | `SET_METER_MODE` | `u8 submode`, `u8 layout` | submode 0..9, layout 0..3 |
| `0x29` | `RUN_STOP` | `u8 running` | mirrors `scope_state.running` |

#### Device → host

| Code | Name | Payload |
|---|---|---|
| `0x81` | `ACK` | — (exists) |
| `0x82` | `NAK` | `u8 error_code` (exists) |
| `0x83` | `DATA` | command-specific (exists) |
| `0x85` | `STATUS` | `device_status_t` (exists) |
| `0x90` | `METER_FRAME` | see §3.5 |
| `0x91` | `WAVEFORM_FRAME` | see §3.5 |
| `0x92` | `SCOPE_STATE` | packed `scope_state_t` |
| `0x93` | `EVENT` | `u8 event_id`, `u8 detail` — mode change, button press, capture armed |

Add error codes to the existing `ESP_ERR_*` list for `UNSUPPORTED_IN_MODE` and
`NO_CAPTURE_DATA`. **`NO_CAPTURE_DATA` is the correct reply when `fpga_data_ready()` is false.**
Never substitute the demo waveform.

#### Streams (for `SUBSCRIBE`)

| ID | Stream | Natural rate |
|---|---|---|
| 1 | meter readings | ~4 Hz — the meter poll task's cadence (`fpga_meter_poll_task`) |
| 2 | waveform frames | see the throughput budget below |
| 3 | events | on change |

Subscriptions are cleared on any parser resync failure and on reconnect, so a dead host cannot
leave the device streaming forever. The device should also stop a stream after N consecutive TX
timeouts.

### 3.5 Frame payloads

**`METER_FRAME` (0x90)** — a flattened subset of `meter_reading_t`, chosen so a host can render
the reading *and* re-derive it:

```
u32  update_count        monotonic; host can detect drops
f32  value               scaled value as the firmware computed it
i16  raw_bcd             raw 4-digit BCD integer, uncalibrated
u8   decimal_pos
u8   result_class        meter_result_class_t (NORMAL/OL/UNDERRANGE/…)
u8   flags               bit0 negative, bit1 ac, bit2 autorange, bit3 hold
u8   submode
u8   unit_variant
u8   unit_len
u8[] unit                ASCII, e.g. "V", "kOhm"
```

Fixed part is 18 bytes. Sending `raw_bcd` alongside `value` is deliberate: the per-device
calibration problem means a host log should record what the instrument saw, not only what it
concluded.

**`WAVEFORM_FRAME` (0x91)**:

```
u32  frame_id            increments per capture
u8   channel             0 = CH1, 1 = CH2
u8   flags               bit0 = calibrated (currently always 0)
u8   timebase_idx
u8   vdiv_idx
u16  sample_count        1024 today
u16  reserved
u8[] samples             unsigned 8-bit ADC counts, as read from the FPGA
```

12-byte header + 1024 samples = 1036 bytes, one packet per channel. This exceeds
`ESP_MAX_PAYLOAD`, which is legal in this direction (§2.2) — but a `_Static_assert` or a runtime
guard should document that the 256-byte cap is receive-only, so nobody "fixes" it later.

`flags` bit0 exists so that when real per-range calibration lands (plan §F2) the host can tell
calibrated frames from the current placeholder ones without a protocol version bump. It must
stay 0 until that work is bench-validated.

### 3.6 Throughput budget — read this before promising a frame rate

Derived from the code, **not measured**; treat as an upper bound to be verified on hardware.

`usb_send_bytes()` (`usb_debug.c:145`) sends one 64-byte packet at a time and waits for the
previous transfer to complete, polling with `vTaskDelay(1)` — one FreeRTOS tick, 1 ms at the
configured 1000 Hz tick rate. In the worst case that is **64 bytes per millisecond ≈ 64 kB/s**.

A two-channel waveform frame is ~2 kB, so ~32 ms of TX pacing per frame → **~30 fps ceiling**,
and realistically less. That happens to sit right next to stock's observed runtime cadence of
0x04/0x05 read pairs every ~29 ms (~34 Hz, from the June capture), so ~10–20 fps to the host is
a sane initial target and single-channel streaming is materially cheaper.

If that proves too slow, the fix is a proper zero-copy CDC TX path, not a protocol change. Do not
design around a throughput number nobody has measured.

### 3.7 Versioning

`PING` returns the firmware version string (`esp_comm.c:47`, currently `"0.2.0-dev"`). Add a
protocol version byte to the `STATUS` payload. Host refuses to run against an unknown major.
Unknown command codes must return `NAK/UNKNOWN_CMD`, never silence — silence is
indistinguishable from a dropped packet, and this project has been burned by exactly that class
of ambiguity before.

---

## 4. Host side

### 4.1 Shape

```
tools/openscope_host/
  openscope/
    link.py        transport: port discovery, open, read/write, reconnect
    proto.py       pack/unpack, checksum, command constants
    device.py      high-level API: dev.meter(), dev.waveform(), dev.set_mode()
    cli.py         `openscope` entry point
    plot.py        optional live plot (matplotlib or pyqtgraph)
  tests/
    test_proto.py  pure-Python round-trip tests, no hardware
  README.md
```

`proto.py` must be **fully testable without hardware** — the encoder and decoder are pure
functions over bytes. That is what `tests/test_proto.py` covers, and it is how a contributor
without a 2C53T can still make progress.

### 4.2 CLI sketch

```bash
openscope ports                       # list candidate ports, say which one it would pick
openscope info                        # PING + STATUS
openscope shell                       # passthrough to the ASCII debug shell
openscope meter --log meter.csv       # subscribe to meter stream, append CSV
openscope meter --plot                # live plot
openscope scope --frames 100 --out capture.npz
openscope scope --live                # live waveform window
openscope press MENU                  # inject a button
openscope set mode scope
openscope set timebase 7
```

Every subcommand exits non-zero with a one-line explanation when the device is absent. No
tracebacks for the expected failure.

### 4.3 Dependencies

`pyserial` only, for the core. Plotting stays an optional extra (`pip install openscope[plot]`)
so that logging works on a headless machine. The repo already uses `python3` scripts with
minimal deps; match that.

---

## 5. Milestones

| # | Deliverable | Needs hardware? | Blocked by |
|---|---|---|---|
| M0 | `proto.py` + round-trip tests | no | nothing |
| M1 | Bind `esp_comm` to the CDC RX/TX path; `PING`/`STATUS` answer honestly | yes, to verify | nothing |
| M2 | `openscope info` / `ports` / `shell` | yes | M1 |
| M3 | `GET_METER` + meter stream + CSV logging | yes | M1 |
| M4 | `BUTTON` injection + `SET_MODE` | yes | M1 |
| M5 | `GET_WAVEFORM` + waveform stream + live view | yes | M1, and a build that captures (`guest-coldtrace`) |
| M6 | Full control surface (timebase, vdiv, trigger, run/stop) | yes | M5 |

M0 is doable on any machine. M1–M4 need only the meter, which works. M5 needs the cold-boot
capture path, which now exists but whose calibration does not.

---

## 6. First task — pick this one

**Make `STATUS` tell the truth, and get one real packet out of the device.**

Small, self-contained, testable, and it unblocks everything above.

**Firmware** (`firmware/src/drivers/`):

1. In `usb_debug.c`, add a CDC writer and register it:
   `esp_comm_set_writer(fn)` where `fn` pushes one byte through the existing CDC TX path.
   Buffer bytes and flush per packet — do not call the 64-byte chunked sender once per byte.
2. In `vUsbDebugTask`, before `shell_feed()`, route bytes into `esp_comm_receive_byte()` when a
   packet is in progress or the byte is `0xAA` (§3.2). When it returns true, call
   `esp_comm_process(esp_comm_get_packet())`.
3. Fix `handle_status()` (`esp_comm.c:195`): `status.current_mode` is hardcoded to 0 and
   `status.battery_pct` to 100. Wire them to `current_mode` (`ui/ui.h:76`) and the real battery
   percentage (`drivers/battery.h`). Add a protocol version byte (§3.7).

**Host** (`tools/openscope_host/`):

4. `proto.py`: `encode(cmd, payload) -> bytes` and an incremental `Decoder` class.
5. `tests/test_proto.py`: encode/decode round-trip, a deliberately corrupted checksum that must
   be rejected, a truncated packet that must not be accepted, and resync after garbage.
   **Make the tests fail when they should** — assert the rejection paths, do not just assert the
   happy path. See ground rule 6 in `docs/dev_plan_2026-08-13.md`.
6. `openscope info`: find the port, send `PING` and `STATUS`, print the result. Print a clear
   message and exit 1 when no device is found (§2.4).

**Acceptance:** on a host with no device attached, `pytest tools/openscope_host/tests` passes
and `openscope info` prints "no OpenScope found on /dev/ttyACM*" and exits 1. On a device
running a `guest-coldtrace` build, `openscope info` prints the firmware version, the *actual*
current mode, and the *actual* battery percentage — verified by changing mode on the device and
seeing the reported value change.

**What NOT to do in this task:** do not touch USB descriptors, do not touch
`fpga_spi3_config_sequence()` or `fpga_bitbang_config_sequence()` (bench-validated and fragile,
maintainer-only — `docs/dev_plan_2026-08-13.md` ground rule 2), and do not try to fix the CDC
enumeration bug. If your build does not enumerate, flash `guest-coldtrace`; that is the known-
good configuration.

---

## 7. Open questions

- **Does binary traffic on the shared endpoint disturb the ASCII shell in practice?** Reasoned
  through in §3.2, not tested.
- **Is 64 kB/s the real ceiling?** §3.6 is derived from code. Someone should measure it.
- **Does streaming while capturing perturb acquisition?** Reading the FPGA config port on a
  running configured device is known to desynchronise acquisition (Experiment L, `CLAUDE.md`).
  Waveform reads use a different path (0x04/0x05), so this is probably fine — *probably* is not
  a measurement.
- **Should waveform frames carry a timestamp?** Useful for logging; the device has
  `uptime_seconds` and the FreeRTOS tick. Cheap to add now, awkward later.

## 8. Related

- [`docs/design/esp32_coprocessor.md`](esp32_coprocessor.md) — where this framing came from, and
  where it goes next (WiFi/BLE bridge over the same packets)
- [`docs/dev_plan_2026-08-13.md`](../dev_plan_2026-08-13.md) — ground rules, and the bench-gated
  queue that governs calibration and CH2
- [`docs/stock_vs_openscope.md`](../stock_vs_openscope.md) — feature comparison; remote control
  is one of the things stock cannot do at all
- `scripts/serial_cmd.py` — the existing minimal host driver; start here
