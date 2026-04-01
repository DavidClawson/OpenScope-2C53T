# Logic Analyzer Add-On Design

Brainstormed 2026-03-29. Using a cheap FX2LP-based USB logic analyzer (HiLetgo/Saleae clone, ~$10) as a peripheral to add digital channels.

## Hardware

- **Device:** HiLetgo USB Logic Analyzer (Cypress CY7C68013A / FX2LP)
- **Capabilities:** 8 channels, 24MHz max sample rate, 3.3V/5V tolerant
- **Protocol:** USB bulk transfers, requires firmware upload on every power-on
- **Firmware:** fx2lafw (open source, from sigrok project, ~2KB)
- **Connection:** USB OTG adapter to scope's USB port

## Two Architectures

### Option A: Direct to GD32 (limited)

```
FX2LP --> USB OTG --> GD32F307 --> LCD
```

- GD32 acts as USB host, uploads fx2lafw, captures bulk data
- **RAM limited to ~50-100KB buffer** (after accounting for other features)
- At 1MHz/8ch: ~50-100ms capture window
- Sufficient for: short UART bursts, single I2C/SPI transactions
- Not sufficient for: sustained capture, mixed-signal oscilloscope view

**Effort:** High. Need USB host stack for GD32 (bulk transfers, enumeration, firmware upload). GD32 SDK has USB host examples but only for MSC/HID classes.

### Option B: Via ESP32 Daughter Card (recommended)

```
FX2LP --> USB OTG --> ESP32-S3 --> SPI/UART --> GD32 --> LCD
```

- ESP32-S3 has its own USB OTG host + 8MB PSRAM
- Captures raw data into PSRAM (8 seconds at 1MHz, 330ms at 24MHz)
- **Decodes protocols on ESP32**, sends compact results to GD32
- GD32 only receives decoded frames, overlays on display

**Advantages over Option A:**
- 16x more buffer memory (8MB vs 50-100KB on GD32)
- Dual-core 240MHz for decode processing while maintaining USB throughput
- GD32 stays focused on display — no USB host overhead
- ESP32 can also stream data to PC over WiFi

## FX2LP Upload Sequence

1. Detect device: VID 04B4, PID 8613 (unconfigured CY7C68013A)
2. Upload fx2lafw via USB control transfers (vendor request 0xA0, CPUCS register)
3. Device disconnects and re-enumerates as VID 1D50, PID 608C
4. Configure: control transfer to set sample rate and channel mask
5. Start capture: bulk read from EP6 (512-byte packets)

Reference: sigrok wiki, fx2lafw source at sigrok.org/gitweb/?p=fx2lafw.git

## Protocol Decode Pipeline (ESP32 path)

```
Raw samples (8-bit, 1-24MHz)
    |
    v
Edge detector (per channel)
    |
    v
Protocol state machine (UART/I2C/SPI/CAN/1-Wire/JTAG)
    |
    v
Decoded frames: { protocol, timestamp, address, data[], flags }
    |
    v
Send to GD32 via SPI (compact binary format)
    |
    v
GD32 overlays decoded annotations on scope display
```

## Mixed-Signal Display Concept

With 8 digital channels overlaid below/above the analog waveform:

```
+------------------------------------------+
| CH1: 2V/div  DC    Trig: Auto   H=50us  |  status bar
+------------------------------------------+
|                                          |
|  ~~~ analog CH1 waveform ~~~             |  analog area
|  ~~~ analog CH2 waveform ~~~             |  (160px)
|                                          |
+------------------------------------------+
| D0 __|--|__|--|__|--|__|--|__|--          |  digital area
| D1 --__|--|__|--|__|--|__|--|__           |  (8 channels)
| D2 ________________________________     |  (64px, 8px each)
| D3 __|--|__|--|__|--|__|--|__|--          |
| D4 ________________________________     |
| D5 ________________________________     |
| D6 ________________________________     |
| D7 ________________________________     |
+------------------------------------------+
| UART: 0x55 0x48 0x65 0x6C 0x6C 0x6F     |  decoded data
+------------------------------------------+
```

## Trigger on Protocol Content

The ESP32 could watch for specific patterns and trigger the analog scope:
- "Trigger when UART receives 0xFF"
- "Trigger on I2C NACK"
- "Trigger when SPI CS goes low"
- "Trigger on CAN error frame"

This would be sent as a GPIO signal from ESP32 to GD32's external trigger input.

## Bill of Materials

| Item | Cost | Notes |
|------|------|-------|
| HiLetgo FX2LP analyzer | ~$10 | Amazon/AliExpress |
| USB OTG adapter | ~$3 | micro-USB or USB-C depending on scope port |
| 8-channel probe clips | ~$5 | Usually included with analyzer |
| **Total** | **~$18** | Turns $80 scope into mixed-signal |

## Priority

This is a Phase 2 feature — after basic firmware is working on real hardware and the ESP32 daughter card design is validated. The ESP32 path (Option B) is strongly preferred over direct GD32 connection.

## References

- sigrok project: https://sigrok.org/
- fx2lafw firmware: sigrok.org/gitweb/?p=fx2lafw.git
- Cypress FX2LP datasheet: CY7C68013A
- ESP32-S3 USB host: esp-idf/examples/peripherals/usb/host/
