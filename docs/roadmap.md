# Roadmap

## What Works on Hardware

Everything in this section runs on the real FNIRSI 2C53T, tested on AT32F403A @ 240MHz.

- **Custom firmware boots and runs** — FreeRTOS scheduler with display + input tasks
- **LCD driver** — ST7789V via 16-bit EXMC, variable-width bitmap fonts (4 sizes from SF Pro + Menlo)
- **4 UI modes** — Oscilloscope, multimeter, signal generator, settings — all navigable via buttons
- **4 color themes** — Dark Blue, Classic Green, High Contrast, Night Red — switchable in settings
- **Button matrix** — 15/15 buttons hardware-confirmed, bidirectional 4x3 scan at 500Hz via TMR3 ISR
- **Battery monitor** — PB1 ADC with 16-sample averaging, percentage display, USB charge detection ("CHG"), low-battery auto-off at 3.3V
- **Power management** — PC9 hold, PB8 backlight, POWER button 3-2-1 countdown shutdown
- **USB HID bootloader** — Closed-case firmware updates via `make flash`, LCD status screen, auto-reboot after flash
- **FPGA USART** — Bidirectional 9600 baud communication, meter data flowing
- **Watchdog + health monitoring** — Task stack checking, fault recovery
- **Emulator** — Renode full-system emulation + SDL3 native LCD viewer with interactive buttons

## Implemented and Tested (Awaiting Real Data)

These features are written in C, unit-tested, and integrated into the firmware build. They currently run on synthetic demo waveforms because FPGA SPI3 data acquisition isn't connected yet. Once live ADC data flows, these light up.

| Feature | Tests | Notes |
|---------|-------|-------|
| FFT spectrum analyzer | 19 | 4096-point, 5 windows (Hann, Hamming, Blackman, Blackman-Harris, flat-top), averaging, max hold, harmonic labeling |
| Waterfall / spectrogram | — | Time-frequency display |
| Split view (time + freq) | — | Simultaneous waveform and spectrum |
| Protocol decoders | 132 | UART (async), SPI (CPOL/CPHA), I2C (debounced), CAN (full frame + CRC), K-Line/KWP2000 |
| Math channels | 5 | CH1+CH2, CH1-CH2, CH1*CH2, invert A, invert B |
| Auto-measurements | 18 | Frequency, period, Vpp, Vrms, Vavg, duty cycle, rise/fall time |
| Persistence display | 8 | 5 decay modes, anti-aliased phosphor rendering |
| DDS signal generator | 25 | 4 waveforms (sine, square, triangle, sawtooth), sub-Hz resolution |
| Bode plot | — | Log/linear sweep, quadrature demodulation, gain + phase |
| Component tester | — | Resistor, capacitor, ESR, diode, continuity (no inductance yet) |
| XY mode / Lissajous | — | CH1 vs CH2 scatter plot |
| Roll mode | — | Continuous scroll for slow signals |
| Trend plot | — | Measurement over time (min/max/avg auto-scale) |
| Mask / pass-fail | — | Template comparison with tolerance bounds |
| Config save/load | 10 | Checksum-verified settings persistence |
| Screenshot capture | 6 | BMP to SPI flash |
| Shared memory pool | — | Saves ~152KB RAM via buffer reuse |

## In Progress

**✅ FPGA config entry + data acquisition — SOLVED 2026-08-13.** The two-month blocker
is broken: our firmware now brings the Gowin FPGA up from a cold boot (bit-bang SSPI
loader, not the hardware-SPI peripheral), arms the capture engine, and reads live ADC
data — a cold-boot-to-live-scope on open firmware. See the devlog
`2026-08-13-cold-boot-to-scope.md`. **The current critical path is now a feature, not
a wall: timebase control** — the build captures ~µs sweeps with no configurable sample
rate, so it tracks slow signals but aliases audio-frequency ones. Next: send the FPGA
timebase commands (`0x0F/0x10/0x11`), wire up the timebase buttons, and calibrate.
Detail in `docs/bench_plan_2026-08-14.md`.

## Future Plans

Roughly ordered by priority. Not committed to timelines.

### Near-term (after data acquisition works)
- **Real oscilloscope display** — live waveforms from FPGA ADC
- **Multimeter with real readings** — FPGA USART meter data → display
- **Signal generator output** — DAC waveform generation on real hardware
- **ADC calibration pipeline** — per-channel calibration from SPI flash data

### Medium-term
- **USB streaming to PC** — CDC serial port, raw samples to sigrok/PulseView
- **CSV/data export** — waveform data to SPI flash, export via USB mass storage
- **Segmented memory** — trigger-only capture, skip dead time
- **Waveform reference library** — known-good overlays on SPI flash

### Memory and resource headroom

Enabling work for everything below — modules especially. Baseline measured
2026-08-13 on `make guest`, gcc 13.2.1.

**The shape of the problem: flash is abundant, RAM is not.**

| Resource | Used | Free |
|---|---:|---:|
| Internal flash (996 KB app slot) | 496,136 B (48.6%) | ~512 KB |
| — of which actual code (`.text`) | 107,020 B | |
| — of which `.rodata` | 388,720 B | |
| SRAM (224 KB) | see below | **24,272 B** after the waterfall move |
| External SPI flash via `.spim` | **0 B** | 16 MB, mapped at `0x08400000` |

Only ~107 KB of the flash is logic. Two data blobs dominate `.rodata`: the FPGA
bitstream (115,638 B) and the CMSIS-DSP FFT twiddle/bit-reversal tables
(~101,700 B). RAM is where features actually compete — three buffers held 90%
of it before the move below.

- **✅ Waterfall history → shared pool** *(done, `935d020`)* — 20,480 B of `.bss`
  became a sub-tenant of the FFT pool region. Free SRAM 3,792 → 24,272 B. Also
  surfaced that `SHMEM_NEED_FFT` (90112) understates the radix-2 layout, which
  really needs 98304; now pinned by a `_Static_assert` in `fft.c`.
- **FPGA bitstream → external SPI flash** — frees ~115 KB of internal flash, the
  single largest item. It is already streamed to the FPGA byte-by-byte, so there
  is no reason it must be compiled in. Adds a W25Q dependency to the boot path,
  so this should wait until cold-boot config entry is fully settled; a failed
  read must fall back cleanly rather than leaving the FPGA unconfigured.
- **Prove the `.spim` XIP path** — the linker script already maps 16 MB at
  `0x08400000` with a `.spim` output section, and it is currently **0 bytes**:
  plumbed but never exercised on hardware. Needs a bring-up test (place one
  function there, call it, confirm it executes) before anything depends on it.
  This is the unlock for shipping modules as loadable assets rather than
  compiled-in data.
- **FFT twiddle tables → SPIM** — ~101 KB of internal flash, read-only and
  latency-tolerant. Only worth doing if internal flash gets tight; blocked on
  the item above.
- **Streaming screenshot → shrink the pool** — screenshot is the sole reason the
  pool is 150 KB (`SHMEM_NEED_SCREENSHOT` = 153,600). If it captured in row
  bands straight to SPI flash or USB, the pool could drop to the FFT need and
  free ~44 KB outright. PR #13 shrinks it to 96 KB by simply disabling
  screenshot, which is the same trade taken bluntly.

**Budgeting rule for new features:** anything needing large *transient* memory
should claim the pool (`SHMEM_OWNER_MODULE` already exists for this). Anything
needing large *persistent* memory is competing for ~24 KB and needs a design
conversation first.

### Automotive suite
- **Relative compression test** — cranking current analysis, per-cylinder bar chart
- **Alternator ripple test** — FFT of battery voltage, diode fault detection
- **Parasitic draw (fuse voltage drop)** — millivolt drop across blade fuses with built-in lookup table
- **Parasitic draw (current clamp)** — roll mode with event detection for module wake-ups
- **Injector pulse width** — per-cylinder duty cycle comparison
- **Ignition coil analysis** — dwell time, spark duration
- **Battery cranking analysis** — voltage sag curve, CCA estimate

### Specialized applications
- **Audio analysis** — THD+N, speaker impedance curve
- **Ham radio** — harmonic analysis, SWR measurement
- **HVAC/Solar** — motor start capacitor test, inverter THD
- **Industrial** — motor current signature analysis (MCSA), 4-20mA loop testing

### Hardware mods (Phase 3)
- **ESP32 WiFi co-processor** — $4 solder-in mod for phone display, remote control, data logging, OTA updates. Design doc at `docs/esp32_coprocessor.md`.

### Stretch track: custom Gowin bitstreams ("gateware for the FPGA")

Unblocked by the 2026-08-13 breakthrough. Now that our firmware can upload arbitrary
bitstreams to the Gowin GW1N-UV2 from a cold boot, we can write **our own FPGA designs**
— not just replay stock's — turning this from an open-firmware clone of the stock scope
into a scope that does things the stock one can't.

**Why it's realistic now (the prerequisites are already in hand):**
- **Upload path solved** — the bit-bang SSPI loader configures the FPGA every boot.
- **Open synthesis toolchain for this exact part** — `gw1n2-apicula` (Yosys → nextpnr →
  Apicula) targets the GW1N-2 family = our GW1N-UV2. Same toolchain we used to sim the
  netlist.
- **The stock design is reverse-engineered** — the netlist trace (`gw1n2-apicula`
  `tools/m_*.py`, `scope_unpacked.v`) mapped the pins, the ADC data bus into the BRAMs,
  the capture counter, the SPI control register, and the readback mux. Not a black box.

**What it could unlock:**
- **Hardware decimation / configurable sample rate** — the elegant fix for the timebase
  problem (custom fabric samples at any rate, instead of coaxing stock's opaque commands).
- **Our own clean MCU↔FPGA protocol** — a real register map + streaming interface we
  control, replacing stock's cryptic `0x04/0x05`.
- **Logic-analyzer mode, in-fabric edge/pulse triggers, hardware protocol decoders** —
  things stock's fabric simply doesn't do.

**The hard parts (be honest):** replicating the **ADC interface + PLL clocking** (the
250 MS/s DDR capture) is the real challenge — Apicula only partially decodes the rPLL —
and Apicula's completeness for the specific primitives (BSRAM, rPLL, DDR I/O) is a
wildcard.

**Suggested incremental path:** (1) synthesize a *trivial* design (a register the MCU
reads back over SPI) to prove "our bitstream runs and talks to us"; (2) **fork the stock
netlist** we already unpacked — keep its working ADC/PLL front end, tweak buffer
depth/trigger — rather than reinvent sampling; (3) then get ambitious.

**⚠ Hard safety rule:** experiment freely with SRAM config (a bad bitstream just means no
scope until reboot — zero brick risk), but **never write the FPGA's NV flash** — it holds
the meter design and is the only copy.

## Reverse Engineering Status

~98% of the stock firmware is understood:
- 309 functions identified and named (138 high, 182 medium, 42 low confidence)
- All ~40 FPGA commands mapped (0x00-0x2C)
- ADC data format cracked (interleaved CH1/CH2, 8-bit unsigned, offset -28.0)
- Complete hardware pinout documented
- 53-step boot sequence traced
- 8 FreeRTOS tasks and 7 queues mapped
- Remaining: PLL startup assembly, 42 low-confidence function names
