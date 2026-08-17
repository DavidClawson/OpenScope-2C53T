# esp32_siggen — the bench signal source for OpenScope 2C53T

A serial-controlled 2-channel test-signal generator on any dual-DAC ESP32
(WROOM/WROVER, LOLIN D32 PRO). Every bench experiment in
[`docs/experiments/`](../docs/experiments/) is run against this, so it is
published to make those results reproducible.

## Wiring

| ESP32 | goes to |
|---|---|
| GPIO25 (DAC1) | scope CH1 probe tip |
| GPIO26 (DAC2) | scope CH2 probe tip |
| GPIO27 | hardware PWM output (high-frequency window probe) |
| GND | probe ground clip(s) |

## Build

Arduino IDE or `arduino-cli`, FQBN `esp32:esp32:esp32`. No libraries beyond the
ESP32 core.

## Control

USB serial at 115200, one command per line. An optional leading `1` or `2`
selects the channel (default CH1).

```
[ch] sine   <hz>            [ch] tri  <hz>        [ch] saw <hz>
[ch] square <hz> [duty%]    [ch] dc   <mV>        [ch] off
[ch] amp    <mVpp>          [ch] phase <deg>      status | ?
pwm <hz> [duty%]            pwm off
```

Examples: `sine 1000` (CH1), `2 tri 200` (CH2), `2 square 500 25`.

## Range and accuracy

The two DAC channels run a **software** DDS at 40 kSa/s, so they are usable from
about 1 Hz to ~5 kHz. Above that use `pwm` on GPIO27, which is hardware LEDC and
reaches MHz.

**Calibrate against the numbers, not the dial.** Measured on bench unit #1 on
2026-08-17: a nominal 100 Hz request read 82 Hz on stock firmware, and 250 Hz
read 208 Hz — the same 0.82 factor on both channels. Treat requested frequency
as approximate and derive the true rate from the capture, or cross-check with a
counter.

## Why two channels with settable phase

`phase <deg>` sets a channel's offset relative to CH1 and holds it when the two
frequencies are equal, which makes an anti-phase pair usable for testing whether
a scope's two channels are genuinely independent.

**A caution learned the hard way:** an anti-phase sine pair is a *weaker* test
than two different waveform SHAPES. If a display inverts, offsets or rescales a
trace, anti-phase can be mistaken for one source drawn twice. A triangle on one
jack and a square on the other cannot. Use `1 tri 500` + `2 square 500 50`.
