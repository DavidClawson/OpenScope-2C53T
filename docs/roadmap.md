# Roadmap

## What Works on Hardware

Everything in this section runs on the real FNIRSI 2C53T, tested on AT32F403A @ 240MHz.

- **Custom firmware boots and runs** — FreeRTOS scheduler with display + input tasks
- **LCD driver** — ST7789V via 16-bit EXMC, variable-width bitmap fonts (4 sizes from SF Pro + Menlo)
- **4 UI modes** — Oscilloscope, multimeter, signal generator, settings — all navigable via buttons
- **4 color themes** — Dark Blue, Classic Green, High Contrast, Night Red — switchable in settings
- **Button matrix** — 14/15 buttons hardware-confirmed, bidirectional 4x3 scan at 500Hz via TMR3 ISR
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

**FPGA SPI3 data acquisition** — This is the critical path. The FPGA sends ADC samples over SPI3 (PB3/PB4/PB5 at 60MHz), but getting it working requires:
1. PB11 HIGH (active mode signal to FPGA)
2. Full USART boot command sequence (commands 0x01-0x08)
3. Queue-driven triggering (not polled)
4. SysTick delays between boot phases

Root cause is identified and documented in `reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md`. Implementation is next.

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
- **Custom FPGA bitstream** — Gowin GW1N-UV2 has open-source toolchain (Yosys + nextpnr-gowin + Apicula). Could enable higher sample rates, custom triggers, hardware acceleration.

## Reverse Engineering Status

~98% of the stock firmware is understood:
- 309 functions identified and named (138 high, 182 medium, 42 low confidence)
- All ~40 FPGA commands mapped (0x00-0x2C)
- ADC data format cracked (interleaved CH1/CH2, 8-bit unsigned, offset -28.0)
- Complete hardware pinout documented
- 53-step boot sequence traced
- 8 FreeRTOS tasks and 7 queues mapped
- Remaining: PLL startup assembly, 42 low-confidence function names
