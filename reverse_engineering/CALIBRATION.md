# ADC Calibration & Measurement Processing Reference

## FNIRSI 2C53T — From Raw ADC to Voltage on Screen

---

## 1. Overview

The complete signal path from analog input to displayed waveform:

```
Analog Input
    |
    v
FPGA (Gowin GW1N-UV2)
  - 250 MS/s ADC sampling
  - Interleaved CH1/CH2 packing
  - 8-bit unsigned output
    |
    v
SPI3 @ 60 MHz (PB3/PB4/PB5, CS on PB6)
  - Mode 3 (CPOL=1, CPHA=1)
  - Full-duplex, MSB first
  - Software chip select (active LOW)
    |
    v
MCU (AT32F403A @ 240 MHz)
  - Raw samples into double-buffered RAM
  - VFP (hardware float) calibration pipeline
  - Per-sample gain/offset/clamp
    |
    v
Display Buffer (RAM)
  - Calibrated 8-bit values (0-255)
  - Maps to vertical pixels on LCD
    |
    v
ST7789V LCD (320x240 RGB565)
  - EXMC parallel bus at 0x60020000
```

The FPGA handles high-speed ADC sampling and delivers raw 8-bit data over SPI3. The MCU applies per-channel calibration using Cortex-M4F VFP (hardware floating-point) registers, then stores calibrated samples for the display task to render.

---

## 2. Raw ADC Data

### Sample Format

- **Resolution:** 8-bit unsigned (0-255)
- **Channels:** Interleaved CH1/CH2 byte pairs
- **DC offset:** Hardware has ~28 LSB offset (corrected in calibration)

### Buffer Sizes by Acquisition Mode

| Mode | Trigger Byte | Total Bytes | Sample Pairs | Buffer Location |
|------|-------------|-------------|--------------|-----------------|
| Normal (case 2) | 3 | 1024 | 512 | `ms+0x5B0` (interleaved) |
| Dual-channel (case 3) | 4 | 2048 | 1024 | CH1: `ms+0x5B0`, CH2: `ms+0x9B0` |
| Roll mode (case 1) | 2 | 5 per read | 1 pair | CH1: `ms+0x356`, CH2: `ms+0x483` |
| Fast timebase (case 0) | 1 | 0 (config only) | N/A | N/A |

Where `ms` = meter_state base at `0x200000F8`.

### Normal/Dual Mode Data Layout

```
SPI3 byte stream (case 2, 1024 bytes):
  [0] CH1[0]  [1] CH2[0]  [2] CH1[1]  [3] CH2[1] ...

SPI3 byte stream (case 3, 2048 bytes):
  [0] CH1[0]  [1] CH2[0]  [2] CH1[1]  [3] CH2[1] ...
  Split into:
    ms+0x5B0: CH1 buffer (1024 bytes)
    ms+0x9B0: CH2 buffer (1024 bytes)
```

### Roll Mode Data Layout

Roll mode uses a 300-sample circular buffer per channel. Each SPI3 read returns 5 bytes:

```
Byte 0: CH1 reference (high)
Byte 1: CH1 data sample
Byte 2: CH2 data sample
Byte 3: CH2 extra
Byte 4: Status (stored at ms+0x5AF)
```

The buffer is shift-and-append: all existing samples move down by 1 position before the new sample is written at the end. This is O(n) per sample but preserves display continuity.

### Double-Buffering Scheme

When the acquisition counter reaches the timebase threshold, the `input_and_housekeeping` function sends **two** items to the SPI3 data queue back-to-back:

```c
// Trigger double-buffered acquisition
uint8_t trigger = 1;
xQueueGenericSend(SPI3_DATA_QUEUE, &trigger, portMAX_DELAY, 0);
xQueueGenericSend(SPI3_DATA_QUEUE, &trigger, portMAX_DELAY, 0);
```

This causes two consecutive SPI3 read cycles (ping-pong), preventing display tearing while one buffer is being rendered and the other is being filled.

---

## 3. Calibration Data Storage

### SPI Flash (Persistent)

Calibration data is stored on the Winbond W25Q128JV SPI flash and loaded into RAM at boot. The flash layout uses two regions:

- **Region 2:** Offset 0x200000 (2 MB) -- user data, waveform storage
- **Region 3:** Raw page address -- system data, calibration

### RAM Locations: Gain/Offset Table

Six gain/offset pairs occupy addresses `0x20000358` through `0x20000434`, spaced at 20-byte (0x14) intervals:

| Address | Pair # | Likely Purpose | Referenced By |
|---------|--------|---------------|---------------|
| `0x20000358` | 1 | V/div range 1 gain/offset | `scope_main_fsm`, `FUN_080018a4` |
| `0x2000036C` | 2 | V/div range 2 gain/offset | `scope_main_fsm`, `FUN_080018a4` |
| `0x20000380` | 3 | V/div range 3 gain/offset | `scope_main_fsm`, `FUN_080018a4` |
| `0x20000394` | 4 | V/div range 4 gain/offset | `scope_main_fsm`, `FUN_080018a4` |
| `0x200003A8` | 5 | V/div range 5 gain/offset | `scope_main_fsm`, `FUN_080018a4` |
| `0x200003BC` | 6 | V/div range 6 gain/offset | `scope_main_fsm`, `FUN_080018a4` |

Additional entries referenced only by `scope_main_fsm` (0x08019e98):

| Address | Pair # | Notes |
|---------|--------|-------|
| `0x200003D0` | 7 | 9 references |
| `0x200003E4` | 8 | 9 references |
| `0x200003F8` | 9 | 16 references (heavily used) |
| `0x2000040C` | 10 | 9 references |
| `0x20000420` | 11 | 9 references |
| `0x20000434` | 12 | 14 references |

These 12 entries likely correspond to the 12 V/div settings available on the scope (from 10 mV/div to 10 V/div). Each 20-byte structure probably contains gain, offset, and auxiliary calibration coefficients for that voltage range.

### Per-Channel Calibration Arrays

Two 301-byte calibration arrays are loaded at boot:

| Channel | RAM Address | Offset from ms | Size | XOR |
|---------|-------------|-----------------|------|-----|
| CH1 | `ms+0x356` (0x2000044E) | +0x356 | 301 bytes (0x12D) | XOR 0x80 |
| CH2 | `ms+0x483` (0x2000057B) | +0x483 | 301 bytes (0x12D) | XOR 0x80 |

Note: These same RAM regions double as the roll mode circular buffers during acquisition. The calibration data is only needed during the loading phase.

### FUN_08001830: calibration_loader

This function loads per-channel calibration data from SPI flash into RAM at boot:

```c
// Called twice from system_init (FUN_08023A50):
//   calibration_loader(ms + 0x356, 0x12D, byte_xor_0x80);  // CH1
//   calibration_loader(ms + 0x483, 0x12D, byte_xor_0x80);  // CH2

void calibration_loader(uint8_t *dest, uint16_t count, uint8_t xor_val) {
    // Reads 'count' bytes from SPI flash calibration region
    // XORs each byte with xor_val (0x80) before storing
    // This XOR obfuscation is a simple protection against casual reading
    for (int i = 0; i < count; i++) {
        dest[i] = flash_read_byte() ^ xor_val;
    }
}
```

The XOR with 0x80 effectively flips the sign bit of each byte, converting between signed and unsigned representations. This is likely because the calibration data in flash is stored as signed offsets (-128 to +127) but used as unsigned correction values in the pipeline.

---

## 4. VFP Calibration Pipeline

### Register Assignment

The `spi3_acquisition_task` (FUN_08037454, 7164 bytes) preloads seven VFP single-precision registers at task entry. These persist across all acquisition cycles:

| VFP Register | Value | Purpose |
|-------------|-------|---------|
| `s16` | `-28.0` | **ADC zero offset** -- hardware DC bias correction |
| `s18` | *(literal pool)* | Calibration gain for CH1 |
| `s20` | *(literal pool)* | Calibration offset for CH2 |
| `s22` | *(literal pool)* | DC bias correction term |
| `s24` | `255.0` | Maximum clamp value |
| `s26` | `0.0` | Minimum clamp value |
| `s28` | *(literal pool)* | Range divisor (normalizes ADC to 0..1) |

The literal pool is at address `0x08037674+` in flash. The exact values of s18, s20, s22, and s28 depend on the compiled calibration defaults. They are likely updated at runtime when the voltage range or probe type changes.

### Why -28.0?

The FPGA ADC has a consistent DC offset of approximately 28 LSBs. A "zero" input does not produce ADC code 0; it produces approximately code 28. Every raw sample must be corrected by subtracting this offset before any further processing. This is the single most important calibration constant for replacement firmware.

### Conditions for VFP Calibration

The full VFP calibration path runs only when ALL of these conditions are met:

1. `spi3_transfer_mode` (ms[0x14]) is not 3
2. `timebase_index` (ms[0x2D]) is less than 4 (fast timebases)
3. `calibration_mode` (ms[0x33]) is 0 (not in self-calibration)
4. Both probe types (ms[0x352] and ms[0x353]) are valid (not 0x00 or 0xFF)

If any condition fails, raw uncalibrated samples are stored directly.

---

## 5. Per-Sample Calibration Formula

This is the core calibration applied to every raw ADC sample in normal and dual-channel scope modes. Extracted from disassembly at `0x08037918`:

```c
/*
 * Calibrate a single raw 8-bit ADC sample.
 *
 * Parameters (from VFP registers and meter_state):
 *   raw_sample    -- unsigned 8-bit from FPGA SPI3
 *   s16_offset    -- -28.0 (ADC zero offset)
 *   s28_divisor   -- range divisor (normalizes raw to 0..1)
 *   s22_bias      -- DC bias correction
 *   s24_max       -- 255.0 (upper clamp)
 *   s26_min       -- 0.0 (lower clamp)
 *   voltage_range -- from ms[0x1C], int16_t, current V/div setting
 *   dc_offset     -- from ms[0x04], int8_t, per-channel DC offset
 */
uint8_t calibrate_sample(uint8_t raw_sample,
                         float s16_offset,      // -28.0
                         float s28_divisor,
                         float s22_bias,
                         float s24_max,         // 255.0
                         float s26_min,         // 0.0
                         int16_t voltage_range,
                         int8_t dc_offset)
{
    // Step 1: Convert raw byte to float
    float raw_f = (float)(uint8_t)raw_sample;

    // Step 2: Remove ADC offset and normalize
    //   For 8-bit ADC with -28 offset: maps ~28-255 to 0.0-1.0
    float normalized = (raw_f + s16_offset) / s28_divisor;

    // Step 3: Compute display range (voltage range minus DC offset)
    float range = (float)((int16_t)voltage_range - dc_offset);

    // Step 4: Scale to display coordinates and apply bias
    float result = normalized * range + (float)dc_offset + s22_bias;

    // Step 5: Clamp to valid display range (0-255 for 8-bit display buffer)
    if (result > s24_max) result = s24_max;   // 255.0
    if (result < 0.0f)    result = s26_min;   // 0.0

    // Step 6: Convert back to integer
    return (uint8_t)(int)result;
}
```

### Applying to a Full Buffer

```c
// After SPI3 bulk read fills ms+0x5B0 with 1024 raw bytes:
void calibrate_buffer(volatile uint8_t *ms) {
    float s16 = -28.0f;
    float s28 = /* range_divisor from literal pool */;
    float s22 = /* dc_bias from literal pool */;
    int16_t vrange = *(int16_t *)(ms + 0x1C);
    int8_t dc_off_ch1 = *(int8_t *)(ms + 0x04);
    int8_t dc_off_ch2 = *(int8_t *)(ms + 0x05);

    // Case 2 (normal): interleaved in single buffer
    for (int i = 0; i < 1024; i += 2) {
        ms[0x5B0 + i]     = calibrate_sample(ms[0x5B0 + i],
                                s16, s28, s22, 255.0f, 0.0f,
                                vrange, dc_off_ch1);
        ms[0x5B0 + i + 1] = calibrate_sample(ms[0x5B0 + i + 1],
                                s16, s28, s22, 255.0f, 0.0f,
                                vrange, dc_off_ch2);
    }

    // Case 3 (dual): separate buffers
    for (int i = 0; i < 1024; i++) {
        ms[0x5B0 + i] = calibrate_sample(ms[0x5B0 + i],
                            s16, s28, s22, 255.0f, 0.0f,
                            vrange, dc_off_ch1);
        ms[0x9B0 + i] = calibrate_sample(ms[0x9B0 + i],
                            s16, s28, s22, 255.0f, 0.0f,
                            vrange, dc_off_ch2);
    }
}
```

---

## 6. Roll Mode Probe Compensation

Roll mode uses an alternate calibration formula when the probe type value exceeds `0xDC` (220 decimal). This threshold distinguishes standard probes from high-attenuation or specialty probes that require gain compensation.

### Probe Gain Formula

```c
/*
 * Roll mode calibration with probe compensation.
 *
 * Applied per-sample when probe_type > 0xDC.
 * Uses different VFP register roles than normal mode.
 */
uint8_t calibrate_roll_sample(uint8_t raw_byte,
                               uint8_t probe_type,
                               float s16_offset,     // -28.0
                               float s18_gain,        // CH1 cal gain
                               float s20_offset,      // CH2 cal offset
                               float s22_bias,        // DC bias
                               float s24_max,         // 255.0
                               float s26_min,         // 0.0
                               int8_t dc_offset)
{
    // Step 1: Compute probe gain factor
    float probe_f = (float)probe_type;
    float probe_gain = (probe_f + s16_offset) / s18_gain;

    // Step 2: Normalize raw sample with probe compensation
    float raw_f = (float)raw_byte;
    float adjusted = (raw_f + s20_offset - (float)dc_offset) / probe_gain;

    // Step 3: Add bias and DC offset back
    float result = adjusted + s22_bias + (float)dc_offset;

    // Step 4: Clamp
    if (result > s24_max) result = s24_max;
    if (result < s26_min) result = s26_min;

    return (uint8_t)(int)result;
}
```

### When This Path Activates

```c
// In spi3_acquisition_task, roll mode (case 1):
uint8_t ch1_probe = ms[0x352];  // CH1 probe type
uint8_t ch2_probe = ms[0x353];  // CH2 probe type

if (ch1_probe > 0xDC) {
    // Use probe-compensated calibration for CH1 roll samples
    calibrated = calibrate_roll_sample(raw_ch1, ch1_probe, ...);
} else {
    // Use standard calibration
    calibrated = calibrate_sample(raw_ch1, ...);
}
// Same logic for CH2 using ch2_probe
```

Probe type values:
- `0x00` = no probe connected
- `0xFF` = no probe connected (alternate encoding)
- `0x01-0xDC` = standard probe (normal calibration)
- `0xDD-0xFE` = high-attenuation/specialty probe (probe-compensated calibration)

---

## 7. Voltage Range Encoding

### Config Register Layout

The voltage range is stored as a 16-bit value in the meter_state structure at offset `0x1C` (address `0x20000114`). When sent to the FPGA via USART config commands, it uses a specific bit layout:

```
config[0x18] voltage range encoding:

For CH1 (Type 0 config writer):
  bits [1:0]   = range_low  (2 bits from params[2])
  bits [15:12] = range_high (4 bits from params[3])

For CH2 (Type 1 config writer):
  bits [9:8]   = range_low  (2 bits from params[2], shifted left 8)
  bits [15:12] = range_high (4 bits from params[3])
```

### XOR Transformation for FPGA

Before sending the voltage range to the FPGA via SPI3, the firmware applies a bitwise transformation:

```c
int16_t voltage_range = *(int16_t *)(ms + 0x1C);
int16_t command_code = ~0x7F ^ voltage_range;  // XOR with 0xFF80
```

This transformation maps the MCU's internal range representation to the FPGA's expected encoding. The `~0x7F` evaluates to `0xFF80` (or `-128` as signed), which effectively inverts the upper bits while preserving the lower 7 bits.

### Config Register Bit Map (config[0x20])

```
Bit 1:  CH1 AC/DC coupling    (0=DC, 1=AC)
Bit 3:  CH1 bandwidth limit   (0=off, 1=20MHz)
Bit 5:  CH2 AC/DC coupling    (0=DC, 1=AC)
Bit 7:  CH2 bandwidth limit   (0=off, 1=20MHz)
Bit 9:  Trigger edge           (0=rising, 1=falling)
Bit 11: Trigger source         (0=CH1, 1=CH2)
```

---

## 8. Meter Calibration

The multimeter mode uses a completely separate calibration path from the oscilloscope. Meter data arrives via USART2 (9600 baud) rather than SPI3.

### Data Flow

```
FPGA Meter ADC
    |
    v
USART2 RX (12-byte frames, 0x5A 0xA5 header)
    |
    v
meter_data_processor (BCD decode + double-precision calibration)
    |
    v
meter_mode_handler (8-state FSM for range/coupling/overload)
    |
    v
Display (large digit rendering)
```

### BCD Digit Extraction

USART RX frame bytes [2..6] encode BCD digits as cross-byte nibble pairs:

```c
void extract_meter_digits(volatile uint8_t *rx, uint8_t digits[4]) {
    // Each digit is formed from the high nibble of one byte
    // and the low nibble of the next byte
    uint8_t b2 = rx[2], b3 = rx[3], b4 = rx[4], b5 = rx[5], b6 = rx[6];

    uint8_t raw0 = (b2 & 0xF0) | (b3 & 0x0F);
    uint8_t raw1 = (b3 & 0xF0) | (b4 & 0x0F);
    uint8_t raw2 = (b4 & 0xF0) | (b5 & 0x0F);
    uint8_t raw3 = (b5 & 0xF0) | (b6 & 0x0F);

    // Look up each raw value in the command_lookup_table
    // to get decimal digit (0-9) or special code
    digits[0] = lookup_table[raw0];
    digits[1] = lookup_table[raw1];
    digits[2] = lookup_table[raw2];
    digits[3] = lookup_table[raw3];
}
```

### Special Digit Codes

| Pattern | digits[] | Meaning | Action |
|---------|----------|---------|--------|
| `0x0A, 0x0B` | `[0],[1]` | "OL" -- Overload | Display "OL", skip cal |
| `0x10, 0x10` | `[0],[1]` | Blank display | No measurement active |
| `0x10, 0x11` | `[0],[1]` | Partial blank | Transitioning |
| `0x12, 0x0A, *, 5` | `[1],[2],*,[3]` | Continuity detected | Trigger buzzer (ms[0xF5D] = 0xB1) |
| `0x13, 0x14` | `[1],[2]` | Mode change | Re-init meter FSM |
| `0xFF` | any | Invalid/unrecognized | Skip frame |

### Double-Precision Calibration

The meter uses ARM EABI double-precision soft-float calls for calibration math, achieving higher precision than the scope's single-precision VFP pipeline:

```c
// Simplified meter calibration pipeline
void calibrate_meter_reading(int raw_bcd_value) {
    // 1. Convert BCD integer to double
    double value = (double)raw_bcd_value;

    // 2. Apply calibration coefficient (selected by meter_cal_coeff)
    //    cal_coeff comes from the 8-state meter_mode_handler
    double cal = get_calibration_coefficient(ms[0xF37]);

    // 3. Double-precision division by reference
    //    Uses __aeabi_ddiv for precision
    double normalized = __aeabi_ddiv(d8_reference, cal);

    // 4. Scale by accumulated factor
    double scaled = __aeabi_dmul(normalized, d10_accumulator);

    // 5. Convert to integer for display
    int result = __aeabi_d2iz(scaled);

    // 6. Classify result
    if (result == 0)          ms[0xF34] = 4;  // invalid
    else if (result < min)    ms[0xF34] = 2;  // underrange
    else if (result > max)    ms[0xF34] = 3;  // overrange
    else                      ms[0xF34] = 1;  // normal
}
```

### Literal Pool Constants (meter)

From the literal pool at `0x080373CC`:

| Address | Double Value | Likely Purpose |
|---------|-------------|----------------|
| `0x080373CC` | 0.0 | Zero reference |
| `0x080373DC` | 494.0 | Calibration reference value |
| `0x080373EC` | 4503599627370496.0 (2^52) | Double-to-integer rounding trick |

### Range Scaling Factors

Result formatting depends on meter sub-mode (ms[0x10], values 0-7):

| Sub-mode | Function | Decimal Offset | Notes |
|----------|----------|----------------|-------|
| 0 | Voltage DC | digit_count + 0x0A | Standard DC voltage |
| 1 | Voltage AC | +2 (polarity-dependent) | Checks polarity flag |
| 2 | Current | +2 (flag-dependent) | Checks measurement flags |
| 3 | (unused) | 0xFF | Returns invalid |
| 4 | Resistance | conditional | Dual-channel offset logic |
| 5 | Capacitance | digit_count + 2 | |
| 6 | Frequency | digit_count + 0x0A | Same as DC voltage format |
| 7 | Diode/Continuity | special | Custom handling path |

### Overload Detection

Overload is detected at two levels:

1. **BCD level:** digit pattern `0x0A, 0x0B` = immediate "OL" display
2. **FSM level:** meter_mode_handler state 0 checks rx[7] bits:
   - Bit 1 set: overload_flag = 1
   - Bit 0 set: overload_flag = 2
   - Stored at ms[0xF36]

### Continuity Threshold

Continuity detection uses a specific BCD pattern match, not a voltage threshold:

```c
// In meter_data_processor:
if (digit1 == 0x12 && digit2 == 0x0A && digit3 == 5) {
    if (ms[0xF5D] != 0xB0) {
        ms[0xF5D] = 0xB1;    // Trigger buzzer ON
        ms[0xF2D] = 1;       // Update mode state
    }
}
// Buzzer driven by EXTI3 interrupt at 0x08009C10
// reads ms[0xF5D]: 0xB0 = buzzer active, 0xB1 = trigger start
```

### Meter Mode State Machine

The 8-state FSM at `meter_mode_handler` (504 bytes) processes rx[6] and rx[7] status bits:

```
State 0 (IDLE):     Check AC/auto-range/overload bits -> states 1-5
State 1 (POLARITY): rx[7] bit 0 -> set/clear cal_coeff
State 2 (OVERLOAD): Check overload_flag, range change via bit 3
State 3 (AC/DC):    rx[7] bit 2 = AC, rx[6] bit 6 = hold
State 4 (RANGE):    rx[6] bits 4-5 = range indicators
State 5 (AUTO):     ms[0xF39] flag -> cal_coeff 0 or 4
State 6 (STANDBY):  rx[6] bit 4 -> cal_coeff selection
State 7 (STANDBY):  Same as state 6
```

---

## 9. Signal Generator Calibration

The signal generator uses the AT32F403A's dual-channel 12-bit DAC output.

### USART Config Writer (Types 5 and 6)

Signal generator configuration is sent to the FPGA via `usart_tx_config_writer`:

- **Type 5:** Frequency -- configures frequency registers for DAC output timing
- **Type 6:** Waveform -- selects waveform type (sine, square, triangle, sawtooth, etc.)

### SPI3 Feedback (Case 6)

During signal generation, the SPI3 acquisition task reads feedback data:

```c
// Case 6 in spi3_acquisition_task:
uint8_t trig_mode = ms[0x18];   // trigger_mode byte
spi3_xfer(trig_mode);            // Send as SPI3 command, read feedback
```

This allows the firmware to monitor the actual output and adjust the DAC if needed.

### DAC Configuration

The DAC is configured through `siggen_configure` (FUN at ~1.6 KB). GPIO MUX routing functions select between oscilloscope input and signal generator output pins. The 12-bit DAC provides 0-3.3V output range with 0.8 mV resolution.

---

## 10. Known Calibration Constants

All hardcoded constants discovered in the V1.2.0 firmware binary:

### ADC / Calibration

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| ADC zero offset | `-28.0` | VFP s16, literal pool | FPGA ADC DC bias correction |
| Max clamp | `255.0` | VFP s24 | Upper bound for calibrated samples |
| Min clamp | `0.0` | VFP s26 | Lower bound for calibrated samples |
| Meter cal reference | `494.0` | Literal pool 0x080373DC | Double-precision meter reference |
| Double rounding | `2^52` | Literal pool 0x080373EC | Fast double-to-int conversion |
| Cal array size | `301` (0x12D) | calibration_loader param | Per-channel cal data bytes |
| Cal XOR mask | `0x80` | calibration_loader param | Flash obfuscation key |
| Gain/offset entries | 12 | 0x20000358-0x20000434 | Per-V/div cal coefficients |
| Voltage range XOR | `0xFF80` (~0x7F) | spi3_acquisition_task | MCU-to-FPGA range transform |

### Timing / Thresholds

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| Button short press | 70 ticks (0x46) | input_and_housekeeping | Debounce confirm threshold |
| Button long press | 72 ticks (0x48) | input_and_housekeeping | Long-press detect threshold |
| Watchdog feed interval | 11 calls | input_and_housekeeping | IWDG reload frequency (~50 ms) |
| Frame counter wrap | 99 | input_and_housekeeping | ms[0x3D] wraps 0-99 |
| Power-off tick threshold | 101 (0x65) | input_and_housekeeping | Auto power-off counter limit |
| USART TX delay | 10 ticks | usart_tx_frame_builder | Inter-frame spacing |
| SPI3 init delay | 10 ticks | spi3_init_and_setup | Post-handshake stabilization |
| Freq measurement cycle | 51 (0x33) | input_and_housekeeping | TIM5 read interval |

### Buffer Sizes

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| Roll buffer max | 300 (0x12C) | spi3_acquisition_task | Circular buffer sample limit |
| Normal capture | 1024 (0x400) | spi3_acquisition_task (case 2) | Single-channel bytes |
| Dual capture | 2048 (0x800) | spi3_acquisition_task (case 3) | Dual-channel bytes |
| SPI flash page | 4096 (0x1000) | spi_flash_loader | Flash read unit |
| USART TX frame | 10 bytes | usart_tx_frame_builder | MCU to FPGA |
| USART RX frame | 12 bytes | usart2_isr | FPGA to MCU |

### Auto Power-Off Tiers

| Change Type | Threshold | Duration | Trigger |
|------------|-----------|----------|---------|
| Probe disconnect (1) | 900 (0x384) | ~15 minutes | Probe removed |
| Probe type change (2) | 1800 (0x708) | ~30 minutes | Different probe inserted |
| Full probe swap (3) | 3600 (0xE10) | ~1 hour | Complete probe replacement |
| Shutdown code | 0x55 | -- | Passed to power-off function |

### SPI3 Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| Clock speed | 60 MHz | APB1 (120 MHz) / prescaler 2 |
| Mode | 3 (CPOL=1, CPHA=1) | Clock idle HIGH, sample on falling edge |
| Frame size | 8-bit | MSB first |
| CS pin | PB6 | Software-controlled, active LOW |
| SPI enable pin | PC6 | Must be HIGH for FPGA SPI3 |
| Active mode pin | PB11 | Must be HIGH during measurement |

---

## Implementation Notes for Replacement Firmware

### Minimum Viable Calibration

For a first working scope display, implement this minimal pipeline:

```c
#define ADC_OFFSET  (-28.0f)
#define CLAMP_MAX   255.0f
#define CLAMP_MIN   0.0f

// Simplified: assumes range_divisor and bias are known
#define RANGE_DIV   227.0f   // 255 - 28 = approximate usable range
#define DC_BIAS     0.0f     // Start with zero, tune later

void calibrate_scope_buffer(uint8_t *buf, int count,
                             int16_t voltage_range, int8_t dc_offset) {
    for (int i = 0; i < count; i++) {
        float raw = (float)buf[i];
        float norm = (raw + ADC_OFFSET) / RANGE_DIV;
        float range = (float)(voltage_range - dc_offset);
        float result = norm * range + (float)dc_offset + DC_BIAS;

        if (result > CLAMP_MAX) result = CLAMP_MAX;
        if (result < CLAMP_MIN) result = CLAMP_MIN;
        buf[i] = (uint8_t)(int)result;
    }
}
```

### Critical Initialization Order

1. Set PC9 HIGH (power hold) -- first instruction
2. Enable IOMUX/AFIO clock, remap JTAG to free PB3/PB4/PB5
3. Configure SPI3 GPIO pins (PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS)
4. Configure SPI3: Mode 3, Master, /2 prescaler, 8-bit, SW NSS
5. Set PC6 HIGH (SPI enable) and PB11 HIGH (active mode)
6. Send USART boot command sequence (commands 0x01-0x08)
7. Perform SPI3 dummy handshake (CS assert, send 0x00, CS deassert)
8. Wait 10 ms for FPGA stabilization
9. Load calibration data from SPI flash (301 bytes per channel)
10. Begin acquisition loop

### Known Gotchas

- **PB11 must be HIGH** before FPGA will respond to SPI3 (active mode signal)
- **USART boot commands must complete** before SPI3 data is available
- **Double-buffer triggers** -- always send two queue items per acquisition cycle
- **Roll mode reuses calibration array memory** -- load cal data before entering roll mode
- **IWDG must be fed every ~50 ms** or the MCU will reset
- **Probe type 0x00 and 0xFF both mean "no probe"** -- skip calibration for both

---

## Source Files

| File | Contents |
|------|----------|
| `analysis_v120/fpga_task_annotated.c` | Complete annotated C for all 10 FPGA sub-functions |
| `analysis_v120/FPGA_TASK_ANALYSIS.md` | SPI3 format, VFP pipeline, state machines, key discoveries |
| `analysis_v120/ram_map.txt` | All SRAM variable locations and cross-references |
| `analysis_v120/gap_functions.md` | calibration_loader and other gap function analysis |
| `analysis_v120/FPGA_BOOT_SEQUENCE.md` | 53-step boot sequence including calibration init |
| `analysis_v120/FPGA_PROTOCOL.md` | FPGA command codes and USART frame format |
| `COVERAGE.md` | Overall RE coverage: 309 functions, 95% named |
