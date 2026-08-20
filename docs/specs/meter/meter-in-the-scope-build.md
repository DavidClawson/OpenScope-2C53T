# Spec: Multimeter alive in the scope build

**Track:** meter
**Stage now:** S1 in `make guest`; **dead in `guest-coldtrace`** — one device,
two builds, no coexistence. This is issue #15.
**Champion:** —

## What it is

One image where the mode button takes you from a live scope to a working
multimeter and back. Today the shippable scope build (`guest-coldtrace`) holds
a dead meter: after the bit-bang FPGA configuration, USART2 RX goes silent and
no meter frame ever arrives (bench, 2026-08-17).

## Prior art

Stock does both in one image, trivially — which is exactly why this gap is the
first thing a stock user would notice. Wishlist Tier 1 #2 (manual range lock)
and the >10 V decimal-latch bug are the next two meter asks, but both are
moot until the meter runs in the build people actually use.

## Our angle

Once coexistence works, our meter already beats stock's documented annoyances
in reach: the decoder is ours (band-level dp fix, stable low-Ω/kΩ readings),
so range lock and honest resolution are firmware-sized follow-ons, not
reverse-engineering projects. Each gets its own spec once this one is at S2.

## Hardware dependencies

The mechanism of the kill is the open question, and it is entangled with two
live #18 threads:

- **Leading hypothesis:** the uploaded bitstream replaces the NV design, and
  the meter function lived in the NV fabric — so configuring the scope
  *removes* the meter. Consistent with Exp A (NV design services the meter
  without any upload) and with the kill appearing exactly at config.
- **Competing hypothesis:** the meter was never the FPGA (open #18 question —
  second IC near the input jacks); then config shouldn't kill it unless the
  FPGA sits in the USART2 path, and the kill is a pin/routing side-effect.
- Stock manifestly has both working after its own upload — so whatever stock's
  bitstream + runtime does, coexistence is achievable on this hardware.
- PC11 (meter MUX enable) is HIGH in stock's meter mode; our runtime leaves
  it floating (bench GPIO posture sweep, 2026-08-17).

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| S1 (in coldtrace) | In `guest-coldtrace`, after FPGA config and a mode switch to meter, USART2 data frames arrive and a DCV reading tracks a bench source. The kill mechanism is written up — which hypothesis won matters for CH2 too. |
| S2 | DCV 0–9 V and resistance re-verified within a few percent against a bench DMM *post-config*, same session writeup. |
| S3 | Host regression over captured USART frames for the decoder (exists in part); on-device `bench.py` acceptance: scope→meter→scope cycle with a live reading at each stop. |
| S4 | Range lock (wishlist #2), >10 V dp fix, honest trailing digits (wishlist #4) — each promoted through its own spec. |

## Open questions

1. Does a USART2 command *after* config get an echo frame back? (Cheapest
   discriminator between "fabric no longer implements the meter" and "our
   USART posture broke".) Run before theorising.
2. If the meter function must be re-established post-config, is it a USART
   command sequence (stock's 0x01–0x08 boot commands, re-sent) or does stock's
   bitstream simply include both functions and ours is fine as-is?
