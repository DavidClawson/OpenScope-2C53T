# External SSPI bring-up rig (ESP32 + optional FT232H)

> ⚠️ **EXPERIMENTAL — UNTESTED ON HARDWARE (2026-06-14).**
> This whole branch (`experimental/esp32-bringup`) is bench-validation-pending.
> We expect to test it in the next couple of days once magnet wire arrives.
> Nothing here has been confirmed working. Do not treat results as ground truth.

A bench rig for cracking the FPGA **config-entry** problem on the FNIRSI 2C53T
without the slow build → flash → pinhole-reset loop, by driving the Gowin
GW1N-UV2's SSPI config port from an **external 3.3 V SPI master** soldered to
the back-side SPI3 test pads that **@maksidze exposed on GitHub issue #18**.

## Why this exists

Stock firmware reconfigures the FPGA over SSPI every boot; ours sends a
byte-perfect bitstream and the FPGA rejects it at every checkpoint (`0x3A`
close → `FF` not `F8`; status → all-`FF` not `00 01 42 2E 2E`). The input is
provably identical to stock, so the cause is the FPGA's internal **config-entry
state**, not our handshake bytes. Iterating that on MCU firmware is painfully
slow. An external master lets us sweep the SSPI handshake (and clock) in
**seconds**, then port the winning recipe back into `fpga.c`.

We also learned the SSPI port is **clock-limited**: IDCODE reads garbage at
60 MHz but correct (`01 20 68 1B`) at ~470 kHz — so an ESP32 bit-banging at a
few hundred kHz is in the sweet spot, not a limitation.

## Two halves — and what each one needs

| Half | Hardware | What it answers | Notes |
|---|---|---|---|
| **SSPI iterate** (this sketch) | **ESP32 only** | What SSPI sequence/clock makes the FPGA accept config? | The fast loop. **@maksidze can run this half today** — just needs an ESP32. |
| **JTAG oracle** | **FT232H** + openFPGALoader | Is our *bitstream itself* correct? (SRAM-load it directly) | One-time diagnostic. **SRAM is volatile** — gone every power cycle; it is **not** a permanent fix. Optional; the FT232H (~$10–15) is not export-controlled. |

The endgame is still **MCU firmware doing SSPI** — these tools find the recipe;
the recipe then goes back into `fpga.c`.

## Procedure

1. **Solder** magnet wire (30–34 AWG) to the SPI3 pads — SCK, MISO, MOSI, CS —
   **plus a GND** via. Strain-relieve each joint (hot glue / Kapton); these pads
   lift if the wire flexes. Scrape/tin the enamel before soldering.
2. **Flash** the 2C53T with this branch's firmware (`make guest` for the
   factory-bootloader unit; `make` otherwise).
3. In the 2C53T debug shell, run **`fpga busrelease`** — the MCU tri-states
   PB3/PB5/PB6, keeps PC6/PB11 HIGH, and holds PC9 power. The bus is now ours.
4. **(Oracle, ideally first)** If you have the FT232H on the JTAG TAP pads,
   SRAM-load the bitstream with openFPGALoader and see if the scope comes alive.
   Yes ⇒ bitstream good, problem is purely SSPI entry.
5. **Iterate** with the ESP32: `id` first (does the config port answer at all?),
   then `enable` → `status`, sweeping `clk` and `raw` framings until the status
   register flips toward the stock `00 01 42 2E 2E` / a clean `0x3A` close.

## Wiring (ESP32 default VSPI ↔ SPI3 pads, straight through)

```
ESP32 GPIO18 (SCK)  ── SCK  pad
ESP32 GPIO23 (MOSI) ── MOSI pad     (ESP32 drives FPGA data-in)
ESP32 GPIO19 (MISO) ── MISO pad     (ESP32 reads  FPGA data-out)
ESP32 GPIO5  (CS)   ── CS   pad     (active LOW)
ESP32 GND           ── board GND    (REQUIRED)
```

ESP32 is native 3.3 V → direct connect. **Do not** use a 5 V Arduino Uno without
level shifting. A **powered USB hub** is handy: ESP32 + FT232H + the scope's
USB-C (flash/CDC shell) can all be live at once.

## Serial commands (115200 baud)

```
id              read IDCODE (0x11) — expect 01 20 68 1B
status          read STATUS_REGISTER (0x41)
enable          send config-enable prelude (05 / 12 / 15)
clk <kHz>       set SCK frequency (e.g. clk 400)
raw XX XX ..    one CS frame of hex bytes, prints MISO echo
help            list commands
```

## Firmware side (`fpga busrelease`)

`fpga_bus_release()` (in `firmware/src/drivers/fpga.c`) tri-states the
MCU-driven SPI3 lines (PB3 SCK / PB5 MOSI / PB6 CS → floating input), disables
the SPI3 peripheral, and stages PC6=HIGH / PB11=HIGH while leaving the PC9 power
hold untouched. The acquisition task checks `fpga.bus_released` and stays off
the bus. There is intentionally **no un-release command** — power-cycle or
re-flash to reclaim the bus.

## Status / TODO

- [ ] Bench-validate `fpga busrelease` actually Hi-Z's the lines (scope the pads).
- [ ] Confirm the IDCODE read framing (dummy-byte count) against the #18 capture.
- [ ] Add the 115,638-byte bitstream upload path (`0x3B` + data + `0x3A`) once the
      IDCODE/prelude round-trip is confirmed — the array can be generated from
      `firmware/src/drivers/fpga_cal_table.h`.
