# FPGA Communication Protocol — FNIRSI 2C53T

## Physical Layer
- USART2: PA2 (TX), PA3 (RX)
- 9600 baud, 8N1
- MCU sends 10-byte command frames
- FPGA responds with 12-byte response frames

## TX Frame (MCU → FPGA): 10 bytes
```
Byte 0: Header (likely 0xAA)
Byte 1: Header (likely 0x55)
Byte 2: Command high byte
Byte 3: Command low byte
Byte 4: Device ID / fixed value (stored at DAT_20000008)
Bytes 5-7: Parameters (unknown)
Byte 8: Trailer (0xAA)
Byte 9: Checksum (byte2 + byte3)
```

## RX Frame (FPGA → MCU): 12 bytes
```
Byte 0: 0x5A (header)
Byte 1: 0xA5 (header)
Bytes 2-11: Data (includes ADC samples, button state, status)
```

Captured idle heartbeat: `5A A5 E4 2E 63 25 07 00 00 00 00 xx`

## FPGA Command Codes (from mode dispatcher FUN_0800735c)

### Mode 0: Oscilloscope
Commands: 0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11

### Mode 1: Multimeter (basic)
Commands: 0x00, 0x09, 0x07/0x0A*, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E
(* 0x07 if PC7=HIGH, else 0x0A)

### Mode 2: Signal Generator
Commands: 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, then falls through to mode 9 tail

### Mode 3: Multimeter (extended)
Commands: 0x00, 0x08, 0x09, 0x07/0x0A*, 0x16, 0x17, 0x18, 0x19

### Mode 4: (Unknown)
Commands: 0x00, 0x1F, 0x09, 0x20, 0x21

### Mode 5: (Unknown)
Commands: 0x00, 0x25, 0x09, 0x26, 0x27, 0x28

### Mode 6: (Standalone)
Commands: 0x29

### Mode 7: (Standalone)
Commands: 0x15

### Mode 8: (Unknown)
Commands: 0x00, 0x2C

### Mode 9: (Multimeter variant)
Commands: 0x00, 0x12, 0x13, 0x14, then 0x09, 0x07/0x0A*

### Command Map Summary
| Code | Hex | Likely Purpose |
|------|-----|----------------|
| 0x00 | 00 | Reset/Init |
| 0x01 | 01 | Scope: configure |
| 0x02-0x06 | 02-06 | Signal gen setup |
| 0x07 | 07 | Meter: probe detected |
| 0x08 | 08 | Meter: configure |
| 0x09 | 09 | Meter: start measure |
| 0x0A | 0A | Meter: no probe |
| 0x0B-0x11 | 0B-11 | Scope: channel/trigger/timebase |
| 0x12-0x14 | 12-14 | Meter variant setup |
| 0x15 | 15 | Standalone mode 7 |
| 0x16-0x1E | 16-1E | Meter range configs |
| 0x1F-0x21 | 1F-21 | Mode 4 configs |
| 0x25-0x28 | 25-28 | Mode 5 configs |
| 0x29 | 29 | Standalone mode 6 |
| 0x2C | 2C | Mode 8 config |
