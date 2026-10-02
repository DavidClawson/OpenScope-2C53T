# 2C53T board revisions

The 2C53T has shipped on at least two boards. This page records what is known about each, which
parts have been identified, and what that means for running OpenScope.

The unlabelled-board inventory, its photos, the stock meter command capture and the SY7088 and
SD7501 identifications are by **[@saulvalenzuela23](https://github.com/saulvalenzuela23)**
([#37](https://github.com/DavidClawson/OpenScope-2C53T/issues/37),
[#30](https://github.com/DavidClawson/OpenScope-2C53T/issues/30)), reproduced here with permission.
Where a statement was checked independently on another unit, the check is named.

## Summary

| | **V1.4** (`2C53T-V1.4_20250507`) | **Unlabelled** (earlier production) |
|---|---|---|
| Revision marking | silkscreen `2C53T-V1.4_20250507` | none on silkscreen or copper |
| Example units | bench units #1 and #2, @mquerostudio's unit #3 | @saulvalenzuela23 (bought March 2025, stock V1.0.6) |
| OpenScope v0.4.0 | **runs** (units #1 and #3) | **runs** (@saulvalenzuela23, 2026-10-02: live capture, meter 3.299 V on a 3.300 V supply) |
| Battery / boost | Silergy **SY7088DGC** boost, marked `VTxyz` (e.g. `VTHZA`, `VTHnA`) | **TC4056A** linear charger + **SB6284** boost (`B6284`), 4.7 µH, SS34 |
| ADC | AD9288-compatible dual 8-bit (pinout traced by @maksidze) | **MXT2088**, dual 8-bit 100 MS/s (marking read under angled light) |
| Multimeter | separate meter chip on a UART (behaviour) | **SDIC SD7501/SD7502** behind a **π122U31** digital isolator |
| Rail test points | not documented | silkscreened `4.3V`, `3.3V`, `2.5V`, `-2V5`, `-3V3` |

## Parts common to both (as far as known)

| Part | Marking / identity | Notes |
|---|---|---|
| MCU | Artery **AT32F403A**, LQFP-100 (marking sanded) | Identified on V1.4 by teardown; the unlabelled unit enumerates as `2e3c:df11` in ROM DFU. |
| FPGA | Gowin **GW1N-UV2QN48** | Configured by the MCU at every boot; the bitstream lives inside the MCU application image, not on the SPI flash. |
| SPI flash | Winbond **W25Q128JV** (16 MB) | Holds UI images and two FAT volumes. Unit #2 has a Zbit `ZB25VQ128` clone. |
| ADC | dual 8-bit, AD9288 family | **Two independent converters**, one per channel, each on its own 8-bit bus into the FPGA. Not interleaved. Up to 100 MS/s per channel, so the marketed 250 MSa/s is not a real-time per-channel rate. The MXT2088 reading on the unlabelled board and @maksidze's AD9288 pinout trace on V1.4 agree. |
| Frontend muxes | **74HC4051** (range select), 74HC4053 | On V1.4 the 4051 select lines are PE4/PE5/PE6 (CH1) and PB11/PB10/PA10 (CH2), bench-measured. |
| Op-amps | Gainsil **GS8092** (unlabelled board) | |
| Input relays | Hongfa **HFD4/3-S** signal relays; **Cosmo KTY214S** PhotoMOS for AC/DC (unlabelled board) | |
| MCU crystal | 8.000 MHz | |
| FPGA clock | 25.000 MHz oscillator | |

## The multimeter is a separate, isolated chip

On the unlabelled board the meter is an **SDIC SD7501/SD7502** (LQFP-64, marking sanded, identified by
tracing its pins against the datasheet: crystal on pins 59/60, UART on pins 35/36, buzzer output on pin
28 through an EL3H7 optocoupler). It talks to the main MCU over a **9600 8N1 UART through a π122U31
digital isolator**.

The pads marked `RX`, `TX`, `GND`, `3V3` are on the **MCU side** of the isolator, so the meter traffic
can be logged with an ordinary 3.3 V USB-serial adapter without touching the isolated side.

**Command frames (MCU → meter), 10 bytes:**

```
AA 55 05 <code> 00 00 00 00 00 <(0x05 + code) & 0xFF>
```

| Function | Code | Function | Code |
|---|---|---|---|
| Auto | `0x14` | Temperature | `0x12` |
| DC voltage | `0x0C` | Small DC current (mA) | `0x11` |
| AC voltage | `0x0D` | Small AC current (mA) | `0x16` |
| Resistance | `0x0B` | Large DC current (A) | `0x10` |
| Diode | `0x0E` | Large AC current (A) | `0x15` |
| Continuity | `0x17` | Capacitance | `0x0A` |

This table was captured on the unlabelled board with a logic analyser on stock firmware, and it is
**identical** to the word table @Stlkv measured on a V1.4 board with a logger patched into stock
V1.2.0 ([#15](https://github.com/DavidClawson/OpenScope-2C53T/issues/15)). Two board revisions and two
methods agree. OpenScope's transmit path builds exactly this frame (`meter_build_tx_frame()` in
`firmware/src/drivers/fpga.c`).

**Telemetry (meter → MCU)** is a 12-byte frame starting `5A A5`, about every 160 ms. Bytes 2–6 carry
**seven-segment display bitmasks**, not ADC counts. That matches @Stlkv's decoder
([#35](https://github.com/DavidClawson/OpenScope-2C53T/pull/35)). Open question: on the unlabelled
board byte 8 read `0x82` at `0.000 V` and `0x02` at `3.299 V`, but on V1.4 unit #1 a live `1.6141 V`
reading also carried `0x82`. So bit 7 of byte 8 is probably an annunciator, not a zero-scale flag.

## The 8-pin part near the charger (V1.4)

Marked `VTHZA` on one unit and `VTHnA` on another, this is a **Silergy SY7088DGC** synchronous boost
regulator with output disconnect, DFN2x3-8. `VT` is Silergy's device code for the SY7088, and the
remaining letters are year, week and lot. That explains why two units carry different markings.
Applying 20 V to the charge port exceeds its absolute maximum rating, which is the failure reported in #30.

On the unlabelled board this area is laid out differently: a TC4056A charger and an SB6284 boost
converter instead.

## Running OpenScope on each board

- **V1.4:** v0.4.0 runs on bench unit #1 and on @mquerostudio's unit #3 (flashed through the factory
  `IAP` drive, no case opening).
- **Unlabelled:** **v0.4.0 runs** (@saulvalenzuela23, 2026-10-02, [#37](https://github.com/DavidClawson/OpenScope-2C53T/issues/37)):
  it boots to the OpenScope screen, the scope captures live through the FPGA, and the meter read
  3.299 V against a 3.300 V bench supply. So PC9 holds the power on this board too. He then returned the
  unit to stock with his own calibration page restored, and checked the static calibration tables
  (`0x08006000`–`0x0800612F`) byte-for-byte against his pre-flash backup; stock rewrites runtime UI state
  at `0x08006130`–`0x080061DF`. His calibration dump: [#28](https://github.com/DavidClawson/OpenScope-2C53T/issues/28)
  (CRC32 `A9DFD19B`, 345 non-FF bytes).
- **ROM DFU on the unlabelled board** needs the **power button held** for the whole session: the ROM
  bootloader does not drive the board's power latch, so a pinhole reset with BOOT0 bridged just drops
  the rail. See the DFU guide.

**Back up your calibration first, on any board.** See the README's
[calibration backup](../README.md#back-up-your-factory-calibration-first).

## Photos

- Unlabelled board: 18 high-resolution photos in
  [#37](https://github.com/DavidClawson/OpenScope-2C53T/issues/37), plus the SD7501 datasheet.
- V1.4: the photos in [`docs/images/`](images/) and the DFU guide.

If your 2C53T looks different from both, photos of both sides of the PCB are very welcome in a new
issue, especially around the FPGA, the ADC, the meter chip and the charger.

## Open questions

1. ~~Does PC9 hold the power on the unlabelled board?~~ Yes: v0.4.0 runs there (2026-10-02).
2. What does bit 7 of telemetry byte 8 mean?
3. Is the V1.4 meter chip also an SD7501 behind an isolator? Its behaviour on the wire is identical.
4. Are there more revisions? Purchase dates and stock firmware versions help place them.
