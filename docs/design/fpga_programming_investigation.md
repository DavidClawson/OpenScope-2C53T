# FPGA Programming Path Investigation Guide

*How to determine whether the AT32F403A MCU can reprogram the Gowin GW1N-UV2 FPGA — enabling caseless FPGA updates over USB-C.*

## Goal

If the MCU has GPIO access to the FPGA's JTAG or SSPI programming pins, we can implement MCU-as-programmer: customer runs `make flash-fpga` over USB-C, MCU bit-bangs the new bitstream into the FPGA. No programmer, no pogo pins, no opening the case after initial setup.

## What We Know

| Item | Status |
|------|--------|
| FPGA model | GW1N-UV2 (identified by jbtronics on EEVblog, markings sanded off) |
| Package | **Unknown — needs physical identification** |
| Pin count | Unknown (QFN48 or QFN88 are likely candidates for the UV2) |
| Pin 1 corner | **Unknown — not yet identified** |
| Programming header | M0-M3, GND, VDD, VPP (on PCB near FPGA) |
| MCU↔FPGA data lines | SPI3 (PB3/4/5/6), USART2 (PA2/PA3), PC6, PB11 |
| Open-source toolchain | **Not available** — Apicula does not support GW1N-UV2 (only UV4+). Must use GowinEDA (free/proprietary) |

## Step 0: Identify the FPGA Package

From the board photos, the FPGA is the **diamond-oriented QFN chip in the upper-right area** of the PCB (viewed from the component side with USB-C at top-left). Markings are sanded off.

### Determine the package

1. **Measure the chip body** with calipers:
   - QFN48 = 6×6 mm (12 pins per side)
   - QFN88 = 11×11 mm (22 pins per side)
   - These are the two QFN packages Gowin offers for the GW1N-UV2
2. **Count the pads on one side** under magnification or macro lens — confirms the package
3. **Find pin 1** — look for:
   - A dot/dimple on the chip body (may be sanded off)
   - An asymmetric pad on the PCB (pin 1 pad is often different shape)
   - A silkscreen dot on the PCB near one corner

### Why it matters

The pinout is completely different between QFN48 and QFN88. JTAG and SSPI pins land on different physical locations. You need to know the package before you can trace anything.

### Datasheet references

- GW1N-UV2 QFN48 pinout: Gowin DS100 datasheet, Table "GW1N-2 Pin Assignment (QFN48)"
- GW1N-UV2 QFN88 pinout: same datasheet, Table "GW1N-2 Pin Assignment (QFN88)"
- Download GowinEDA (free) to get the full pin constraint files (.cst)

## Step 1: Locate the JTAG Pins

Gowin JTAG pins for programming the internal flash:

| Signal | Direction | Function |
|--------|-----------|----------|
| TCK | Input | Test clock |
| TMS | Input | Test mode select |
| TDI | Input | Test data in |
| TDO | Output | Test data out |

**These are NOT the M0-M3 pins on the programming header.** M0-M3 are mode-select pins that determine boot behavior. JTAG pins are separate dedicated pins on the FPGA.

However, the "programming header" on the PCB may carry JTAG signals despite being labeled M0-M3 — FNIRSI may have just used those silkscreen labels loosely, or the header may carry both mode pins AND JTAG. **Buzz out each pad to find out what it actually connects to.**

### Procedure

1. **Power off the device, battery disconnected**
2. **Set multimeter to continuity mode**
3. **Get the JTAG pin numbers** from the datasheet for your identified package (QFN48 or QFN88)
4. **Probe from each programming header pad to the FPGA JTAG pins:**

   For QFN48 (if that's the package):
   | JTAG Signal | QFN48 Pin |
   |-------------|-----------|
   | TCK | Pin 19 |
   | TMS | Pin 18 |
   | TDI | Pin 21 |
   | TDO | Pin 20 |

   For QFN88 (if that's the package):
   | JTAG Signal | QFN88 Pin |
   |-------------|-----------|
   | TCK | Pin 40 |
   | TMS | Pin 39 |
   | TDI | Pin 42 |
   | TDO | Pin 41 |

   *(Verify these against the actual datasheet — these are approximate from typical Gowin pinouts)*

5. **Record which header pad connects to which FPGA pin**

## Step 2: Check If MCU GPIOs Connect to FPGA JTAG

This is the critical question. Probe continuity from **each known MCU GPIO** to the FPGA JTAG pins identified in Step 1.

### Most likely candidates (GPIOs the MCU already uses for the FPGA)

| MCU Pin | Current Function | Could it dual-purpose? |
|---------|-----------------|----------------------|
| PB3 | SPI3 SCK | Unlikely — actively used for data |
| PB4 | SPI3 MISO | Unlikely — actively used for data |
| PB5 | SPI3 MOSI | Unlikely — actively used for data |
| PB6 | SPI3 CS (GPIO) | Possible — only needed during SPI transfers |
| PC6 | FPGA SPI enable | Possible — control pin |
| PB11 | FPGA active mode | Possible — control pin |
| PC11 | Meter MUX enable | Possible |

### Less obvious candidates

Check the MCU pins near the FPGA on the PCB — FNIRSI may have routed spare GPIOs to JTAG for factory programming/testing. Look at:
- Any unpopulated resistor pads between the MCU and FPGA
- Test points near the FPGA
- Traces from MCU pins that don't have a known function yet

### What you're hoping to find

**Best case:** 4 MCU GPIOs wired to TCK/TMS/TDI/TDO. MCU can bit-bang full JTAG.

**Good case:** MCU GPIOs wired to SSPI pins (SPI slave programming). The FPGA's SSPI interface uses:
| Signal | Function |
|--------|----------|
| SSPI_CS | Chip select |
| SSPI_CLK | Clock |
| SSPI_D0 | Data 0 (MOSI equivalent) |
| SSPI_D1 | Data 1 (MISO equivalent) |

If the existing SPI3 lines (PB3-PB6) happen to also connect to the FPGA's SSPI pins (not just the user I/O pins used for ADC data), then SPI3 is already the programming interface. This would be ideal — same physical wires, just a different protocol.

**Okay case:** Programming header only, no MCU connection. Customer needs a one-time JTAG flash. You could sell a pogo-pin programming jig.

**Worst case:** JTAG pins are bonded but not routed to anything accessible. Would need bodge wires.

## Step 3: Check the SSPI Possibility (the Dream Scenario)

Gowin FPGAs can be programmed via SSPI (SPI slave) if the mode pins are set correctly. The existing SPI3 connection between MCU and FPGA might already be on the right pins.

### How to check

1. From the datasheet, find which FPGA pins are SSPI_CS, SSPI_CLK, SSPI_D0, SSPI_D1 for your package
2. Buzz out the existing SPI3 traces:
   - Where does PB3 (SCK) land on the FPGA? Which pin number?
   - Where does PB4 (MISO) land?
   - Where does PB5 (MOSI) land?
   - Where does PB6 (CS) land?
3. Compare those FPGA pin numbers to the SSPI pin assignments

If SPI3 SCK lands on SSPI_CLK, MOSI on SSPI_D0, etc. — **the MCU can already program the FPGA over the existing SPI3 bus.** You'd just need to:
- Set the mode pins appropriately (may need one GPIO to control a mode pin)
- Reset the FPGA into programming mode
- Stream the bitstream as SPI data

## Step 4: Check Mode Pin Routing

The M0-M3 mode pins on the programming header determine the FPGA's boot/configuration mode:

| M[3:0] | Mode | Description |
|--------|------|-------------|
| 0000 | JTAG only | No auto-boot, wait for JTAG |
| Others | Various | Auto-boot from internal flash, SSPI slave, MSPI master, etc. |

For normal operation, these are likely hardwired (pull-up/pull-down resistors) to auto-boot from internal flash. Check:

1. **Are any mode pins connected to MCU GPIOs?** (Probe continuity from M0-M3 header pads to MCU pins)
2. **What are the default states?** (Measure voltage on each mode pin with the board powered)
3. If a mode pin is MCU-controlled, the MCU could switch the FPGA into JTAG or SSPI programming mode on demand, then switch back to normal boot

## Step 5: Document Everything

For each finding, record:

```
FPGA package: QFN__ (___×___ mm, ___ pins per side)
Pin 1 location: _______________

Programming header actual connections:
  Pad "M0" → FPGA pin ___ (function: _______) 
  Pad "M1" → FPGA pin ___ (function: _______)
  Pad "M2" → FPGA pin ___ (function: _______)
  Pad "M3" → FPGA pin ___ (function: _______)
  Pad "GND" → ground confirmed: Y/N
  Pad "VDD" → 3.3V confirmed: Y/N
  Pad "VPP" → FPGA pin ___ / voltage: ___V

SPI3 FPGA-side pin mapping:
  PB3 (SCK)  → FPGA pin ___  (is this SSPI_CLK? Y/N)
  PB4 (MISO) → FPGA pin ___  (is this SSPI_D1? Y/N)
  PB5 (MOSI) → FPGA pin ___  (is this SSPI_D0? Y/N)
  PB6 (CS)   → FPGA pin ___  (is this SSPI_CS? Y/N)

JTAG accessibility:
  TCK (FPGA pin ___) connects to: _______________
  TMS (FPGA pin ___) connects to: _______________
  TDI (FPGA pin ___) connects to: _______________
  TDO (FPGA pin ___) connects to: _______________

Mode pins (powered state):
  M0 = ___V (pulled HIGH/LOW/floating)
  M1 = ___V
  M2 = ___V
  M3 = ___V

Conclusion: MCU can / cannot program FPGA because: _______________
```

## Tools Needed

- Multimeter with continuity mode
- Magnifying glass or macro camera (for pin counting and pin 1 ID)
- Calipers or ruler (for package measurement)
- Gowin GW1N-UV2 datasheet (DS100) — for pin assignments
- Optionally: Gowin programmer (~$20) for initial experiments even if MCU path works

## What Happens Next

| Finding | Next Step |
|---------|-----------|
| SPI3 = SSPI pins | Implement SPI bitstream upload in firmware. Caseless FPGA update via USB-C. Best outcome. |
| MCU GPIOs on JTAG | Implement JTAG bit-bang in firmware. Slower but still caseless. |
| Mode pins MCU-controlled | Can switch FPGA into programming mode from firmware. Enables either JTAG or SSPI path. |
| No MCU connection to programming pins | Need a one-time pogo-pin flash, or solder bodge wires. Workable but less elegant. |
| Header is actually JTAG (not mode pins) | Even better — just need a $20 programmer clip. But still requires case open. |
