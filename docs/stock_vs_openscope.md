# OpenScope vs. the stock FNIRSI firmware

**Written 2026-08-13** in answer to
[issue #12](https://github.com/DavidClawson/OpenScope-2C53T/issues/12).
Supersedes [`docs/ideas/gaps_and_priorities.md`](ideas/gaps_and_priorities.md), which was written
in March 2026 and is now substantially out of date.

This document exists because "what am I gaining and losing?" is a fair question that this project
had never answered properly. The answer below is deliberately unflattering in places. A
comparison that oversells costs the project credibility with exactly the people who read it.

---

## The short version

**OpenScope is development firmware.** As of today it boots from cold, configures the FPGA
itself, and captures real waveforms on both channels — a thing that took four months and only
started working on 2026-08-13. That is a genuine milestone and it is also *the beginning* of a
working oscilloscope, not the end of one.

| If you want to… | Use |
|---|---|
| Use the device as an instrument you depend on | **Stock** |
| Measure a signal above about 15 Hz on the scope | **Stock** (see [Timebase](#timebase-the-big-one)) |
| Save a screenshot, browse files, use USB storage | **Stock** |
| Trust absolute meter accuracy on your own unit | **Stock** |
| Log or chart a multimeter reading over time | **OpenScope** |
| Test automotive fuses / estimate parasitic drain | **OpenScope** |
| Change how the instrument looks or behaves | **OpenScope** |
| Read, modify and build the firmware yourself | **OpenScope** |
| Help build an open-source instrument | **OpenScope** |

The hardware is identical either way. The analog front end, the 250 MS/s ADC and the Gowin FPGA
are the same silicon; no firmware changes the instrument's underlying bandwidth or sample-rate
ceiling.

**PR #16 adds a PC-driven dual-boot switcher**, so this is not a one-way choice — you can keep
stock and switch.

---

## Summary table

Legend: ✅ works · ⚠️ partial / caveated · 🔬 code exists but is not reachable from the UI ·
❌ absent

| | Stock V1.2.0 | OpenScope | Notes |
|---|---|---|---|
| **Oscilloscope** | | | |
| Live capture from cold boot | ✅ | ✅ | Ours needs the `guest-coldtrace` build; see [Which build](#which-build-actually-captures) |
| Timebase / sample-rate control | ✅ | ❌ | **The single biggest gap.** See below |
| Volts/div | ✅ | ⚠️ | 10 steps in the UI, wired to the frontend relays; calibration is placeholder |
| Trigger mode / edge / source | ✅ | ⚠️ | In state and sent to the FPGA; **trigger level has no button binding** |
| Coupling AC/DC/GND | ✅ | ⚠️ | Selectable; DC/AC relay behaviour bench-confirmed, per-range table still approximate |
| Probe 1× / 10× | ✅ | ✅ | |
| Cursors (Δt, 1/Δt, ΔV) | ✅ | ✅ | Ours is arguably nicer |
| Auto-measurements (Vpp, Vrms, freq…) | ✅ | ❌ | **Ours are hardcoded strings.** `scope_ui.c:253-288`. The engine exists but is never called |
| CH2 | ✅ | ⚠️ | Capture path runs; the CH2 trigger reference is believed to need a timer PWM we never program |
| FFT | ⚠️ broken on stock | ⚠️ | Ours is a real 4096-point engine with 5 windows — **but it is fed a synthetic test signal, not the ADC** |
| Waterfall / split view | ❌ | ⚠️ | Same caveat: real renderer, synthetic input |
| Protocol decoders (UART/SPI/I²C/CAN/K-Line) | ❌ | 🔬 | 132 unit tests, zero call sites in firmware, no UI, no signal source |
| Persistence / math channels | ✅ / — | 🔬 | Implemented, but they render a synthetic sine rather than the live trace |
| XY, roll, trend, mask test | partial | 🔬 | Compiled libraries with no UI path |
| **Multimeter** | | | |
| DCV / ACV / current / resistance / continuity / diode / capacitance | ✅ | ✅ | 10 sub-modes, real data over USART2 |
| Temperature | ✅ | ❌ | No sub-mode for it |
| Chart (strip-chart) view | ❌ | ✅ | 300-sample scrolling trace, auto-scaled |
| Stats view (histogram, min/max/avg) | ❌ | ✅ | 20-bin histogram |
| Fuse-drop / parasitic-drain tester | ❌ | ✅ | 5 fuse types, 47 ratings, 3 sub-views |
| Relative (Δ) mode, auto-hold | partial | ✅ | Hold uses stability detection |
| Manual range lock | ✅ | ❌ | Neither has it, actually — stock's is a known community complaint, ours is absent |
| Continuity buzzer | ✅ | ❌ | Ours flashes the screen; **there is no buzzer drive code** |
| Absolute accuracy | ✅ factory-calibrated | ⚠️ | See [Calibration](#calibration-the-honest-part) |
| DCV above ~10 V | ✅ | ❌ | Known bug: 11 V reads as ~0.997 V |
| **Signal generator** | | | |
| Waveforms | 8 | 8 | Ours: sine, square, triangle, saw, full/half rectified, pulse, noise |
| Frequency range | to 50 kHz | to 25 kHz | Ours is a 6-step preset cycle, not free entry |
| Amplitude / duty control | ✅ | ⚠️ | 6 fixed amplitudes, duty in 10 % steps; **no UI for either**, and the info bar text is hardcoded |
| Actually drives the DAC | ✅ | ✅ | Real DAC1/PA4 + TMR6 + DMA driver — **but inert on the `guest-coldtrace` build**, which needs PA4 for the scope trigger reference |
| **Display & UI** | | | |
| Colour themes | 1 | 4 | Dark Blue, Classic Green, High Contrast, Night Red |
| Languages | 4+ | 1 | English only |
| Backlight dimming | ✅ | ❌ | We set the pin HIGH once |
| Redraw efficiency | ✅ | ⚠️ | Meter is throttled; scope/siggen still repaint unconditionally, and the waterfall is *very* slow |
| **Storage & files** | | | |
| BMP screenshot to flash | ✅ | ❌ | SAVE shows a "SAVED #N" popup and writes nothing |
| On-device file browser | ✅ | ❌ | |
| USB Mass Storage | ✅ | ❌ | Our USB is CDC serial only |
| Filesystem on the 16 MB SPI flash | ✅ FatFs, 2 volumes | ❌ | Raw read/erase/program primitives work; the filesystem layer is TODO stubs |
| Settings persist across power cycles | ✅ | ❌ | `config_save()` writes to a **RAM buffer**, and nothing calls it |
| Waveform export | ❌ | ❌ | On both roadmaps, built in neither |
| **Firmware & recovery** | | | |
| Closed-case firmware update | ⚠️ vendor tool | ✅ | `make flash` over USB-C |
| 3-strike crash recovery / safe mode | ❌ | ✅ | Bootloader forces safe mode after 3 failed boots |
| Debug shell over USB | ❌ | ✅ | ~80 commands; see [the CDC caveat](#the-usb-serial-caveat) |
| Source available, modifiable | ❌ | ✅ | GPL v3 |
| **Power** | | | |
| Auto power-off (15/30/60 min) | ✅ | ❌ | Ours has only battery-critical shutdown and a manual 3-second POWER hold. The "Auto Shutdown" menu item is a dead entry |
| Battery percentage / charge detect | ✅ | ✅ | |

---

## The parts that need more than a table row

### Timebase — the big one

**The scope captures, but you cannot set the sweep speed.** Each hardware sweep is a
~microsecond, 1024-sample snapshot refreshed about 34 times a second, and the current build never
configures the FPGA's sample rate.

In practice, on bench unit #1 with a signal generator:

- 1 Hz square: crisp
- 2 Hz sine: stair-steps, recognisable
- 50 Hz and up: the trace freezes to a stuck level

That is not a bug so much as a missing feature. As the devlog puts it: *"The scope has a vertical
axis and no horizontal knob."* The UI has 21 timebase steps from 5 ns to 20 ms; they currently
collapse into three coarse hardware buckets with guessed parameters, none of which has been
validated on hardware.

There is a complication worth knowing about, because it means this is not a quick fix: a
gate-level analysis of the FPGA bitstream (`gw1n2-apicula`) found **no rate-control logic in the
capture path at all** — no divisor register, no address-counter enable, no clock divider. If that
reading holds, the timebase has to be implemented MCU-side by changing *when* we re-arm and read,
which is a different piece of work than "send the right register value".

**Roadmap:** current critical path — [`docs/roadmap.md` § In Progress](roadmap.md) and
[`docs/bench_plan_2026-08-14.md`](bench_plan_2026-08-14.md).
**Devlog:** [`2026-08-13-cold-boot-to-scope.md`](devlog/2026-08-13-cold-boot-to-scope.md).

### What "implemented" means in this project

This is the correction that matters most, and it applies to claims made in earlier issue replies
and in the roadmap.

A large amount of OpenScope's feature list is **library code with unit tests and no path from the
UI to it.** Protocol decoders, math channels, persistence, XY mode, roll mode, trend plots, mask
testing, the auto-measurement engine, the screenshot BMP writer, config save/load — all compiled,
all tested, **zero call sites in the firmware.** The linker garbage-collects most of them out of
the image entirely.

Separately, several features that *are* reachable are fed **synthetic data rather than the ADC**:

- The FFT, waterfall and split views all generate a 1 kHz test square wave internally and analyse
  that. They do not read `fpga_get_ch1_buf()`.
- The persistence overlay and the math channel render synthetic sines.
- The scope's measurement badges are literal strings: `"1.00kHz"`, `"707mV"`, `"50.0%"`,
  `"1.00ms"`.

So the honest phrasing is: *the DSP is written and tested; wiring it to live capture is a
separate, unstarted job.* Earlier replies implied these would "light up" once the scope
captured. The scope now captures and they did not light up, because nobody has connected them
yet. That connection work is small and it is a good first contribution.

**Roadmap:** this is the work implied by [`docs/roadmap.md` § Near-term](roadmap.md), and
[`docs/dev_plan_2026-08-13.md` §B](dev_plan_2026-08-13.md) covers the rendering half.

### Calibration — the honest part

Stock ships with **per-channel factory calibration** read from files on the internal SPI flash
(`3:/System file/cal_ch1.bin`, `cal_ch2.bin`). OpenScope's loader for those files exists but sits
on top of the stubbed filesystem layer, so it always reads zero bytes and falls back to built-in
defaults. The firmware is honest about this in a comment; the effect is not subtle.

Consequences:

- **Scope vertical accuracy is placeholder.** The baseline sits around 55 counts and the trace
  can clip against the top of the window. Do not read volts off the screen and believe them.
- **Meter behaviour and ratios port fine; absolutes are per-unit.** The low-Ω scale factor
  (0.0304) is hardcoded to bench unit #1. Resistance and DCV up to ~9 V read within a few percent
  *on that unit*. Nobody has verified absolute accuracy on any other device.
- **DCV above ~10 V is wrong** — 11 V reads as roughly 0.997 V, a decimal-point/auto-range latch
  problem with the same root cause as the resistance banding that was already fixed.

**Roadmap:** [`docs/roadmap.md` § "W25Q region layer" and § "User calibration mode"](roadmap.md);
[`docs/dev_plan_2026-08-13.md` §C1](dev_plan_2026-08-13.md) (the storage layer that gates it) and
§F2 (per-range scope calibration, bench-gated).

### Storage, screenshots and USB

These three all trace back to one missing piece. Stock runs FatFs over the 16 MB W25Q128 with two
volumes and uses it for screenshots, the file browser and calibration data, exposed to a PC as
USB Mass Storage.

OpenScope has the **raw flash primitives** — JEDEC ID, read, sector erase, page program,
read-modify-write, all with mutex protection and bounds checks. What it does not have is anything
above them: `flash_fs_read()` returns zero bytes, `flash_fs_write_atomic()` is a block of TODO
comments, and there is no FatFs port.

So: no screenshots (SAVE increments a counter and shows a popup), no file browser, no mass
storage, no persistent settings. The deliberate design decision is **not** to port FatFs but to
build a region table with address-enforced read-only regions, because the factory calibration on
your device is irreplaceable and a stray erase would destroy it.

**Roadmap:** [`docs/dev_plan_2026-08-13.md` §C1](dev_plan_2026-08-13.md) — this blocks screenshots
(§C2), modules (§D1) and user calibration.

### Modules

Earlier descriptions of this project mention "procedure modules" for automotive, HVAC, ham radio
and education. To be exact: **`modules/` contains four empty directories.** There is no module
loader in the firmware. Two automotive analysis routines (compression test, alternator test) are
compiled in with no callers and no UI.

The design is settled — modules will be *data plus a firmware-side interpreter*, not loadable
binaries, because the MCU's execute-in-place flash window collides with the USB pins — but none of
it is built. It is, however, the best contribution surface for a domain expert who does not write
firmware.

**Roadmap:** [`docs/dev_plan_2026-08-13.md` §D](dev_plan_2026-08-13.md).

### The USB serial caveat

OpenScope's USB is a CDC serial debug shell with about 80 commands (FPGA registers, GPIO, memory,
SPI flash, acquisition). It is genuinely useful and stock has nothing like it.

**It does not always enumerate.** The failure correlates 5/5 with build type on the bench unit —
builds using the bit-bang FPGA config path enumerate, builds using the hardware-SPI path do not,
failing with a host-side `error -71`. The mechanism is unresolved; two earlier theories
(temperature, reset type) were tested and refuted. The bootloader's USB is unaffected and always
works, so firmware updates are never at risk.

**Roadmap:** [`docs/dev_plan_2026-08-13.md` §F1](dev_plan_2026-08-13.md) (bench-gated).
A remote-control protocol designed around this quirk is specified in
[`docs/design/remote_protocol.md`](design/remote_protocol.md).

### Which build actually captures

`make guest` — the ordinary build — **does not include the bit-bang FPGA loader** and therefore
does not capture. Only `make guest-coldtrace` currently produces a cold-boot live trace. Folding
that path into the default build is a known outstanding task.

That build also carries bench-rig behaviour you should know about: it boots straight into scope
mode, the SAVE button toggles the input-coupling relay for live A/B testing instead of taking a
screenshot, and the signal generator is disabled because the scope needs the same DAC pin.

---

## Things OpenScope has that stock does not

Stated plainly, and only the ones that actually work today:

- **Multimeter Chart view** — a 300-sample scrolling strip chart, auto-scaled. This is the
  feature that prompted issue #12, and it exists.
- **Multimeter Stats view** — min / max / avg / peak-to-peak / sample count plus a 20-bin
  histogram.
- **Fuse-drop tester** — 5 automotive fuse types, 47 ratings, three sub-views, parasitic-drain
  estimation from real millivolt readings.
- **Scope cursors** with Δt, 1/Δt and ΔV readouts.
- **Four colour themes.**
- **Component tester, resistor calculator, Bode-plot screen** in the settings tree.
- **HID bootloader with 3-strike safe-mode recovery** — three failed boots drop to a bootloader
  with a red banner instead of bricking. You should not need to open the case again.
- **A USB debug shell** — direct FPGA register poking from a terminal.
- **A health/diagnostics panel** with live heap and buffer-pool statistics.
- **It is GPL v3 and you can change it.**
- **It configures the FPGA itself from a cold boot** — which is not a user-facing feature, but it
  is the thing that makes every future scope feature possible on open firmware.

---

## Reading the stock firmware's own weaknesses

For completeness, since the comparison cuts both ways. From community reports collected in
[`docs/community_wishlist.md`](community_wishlist.md), stock's known problems include unreliable
Normal/Single triggering at slow timebases, no manual DMM range lock, awkward Min/Max/Avg
semantics, padded trailing zeros on readings, an unlabelled and reportedly broken FFT, and
occasional screenshot file corruption.

OpenScope beats stock on exactly one of those today (the FFT display is labelled, with a dB grid,
peak markers and window name — though it is analysing a synthetic signal). The rest are open
opportunities, not accomplishments.

---

## If you try it

**Do not put this on a device you need to depend on.** It is development firmware.

- Use it **alongside a known-good meter**, which is what you would sensibly do with any
  unfamiliar instrument.
- Absolute accuracy has never been verified on any unit but the bench unit.
- Expect the scope to be useful only for very slow signals until timebase control lands.
- Going back to stock is supported: `python3 scripts/iap_flash.py`, or drag-and-drop on Windows.
  See [`docs/dfu_mode_guide.md`](dfu_mode_guide.md).
- After PR #16 merges, a PC-driven dual-boot switcher lets you keep both.

## If you want to help

The highest-value contributions right now, in rough order of ratio between impact and difficulty:

1. **Wire the FFT / waterfall to the real ADC buffers** instead of the synthetic test signal.
   Small, self-contained, and it turns a claimed feature into a real one.
2. **Wire the auto-measurement engine** to replace the hardcoded badge strings. The engine is
   written and tested; it is simply never called.
3. **Fill in the module JSON** for a trade you know — no firmware experience needed
   ([`docs/dev_plan_2026-08-13.md` §D2](dev_plan_2026-08-13.md)).
4. **The W25Q region layer** ([§C1](dev_plan_2026-08-13.md)) — unglamorous, and it unblocks
   screenshots, persistent settings, modules and user calibration all at once.
5. **Host-side tooling** — see [`docs/design/remote_protocol.md`](design/remote_protocol.md),
   which ends with a concrete evening-sized first task.

Contributions get reviewed and merged; PR #16 went from review to approved in a day.
