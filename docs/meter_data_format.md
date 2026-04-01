# Multimeter Data Format — Reverse Engineering Analysis

*Decoded from V1.2.0 firmware decompilation, 2026-03-31*

## Overview

> **Note:** Hardware probing (2026-03-31) eliminated USART2 and SPI2 as the FPGA command
> interface. The decompiled code references these peripherals, but the actual hardware
> communication path is still unknown. The data pipeline described here is from firmware
> analysis and may need revision once the true FPGA interface is identified.

The multimeter (DVOM) data pipeline has three stages:
1. **Raw acquisition** — FPGA sends measurement data (interface TBD)
2. **Scaling and auto-range** — `FUN_080028e0` converts raw data to engineering units
3. **Display formatting** — `FUN_0800ec70` renders the value with appropriate precision and units

## RAM Variables — Meter State Machine

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x20001025` | byte | `meter_mode` | Main mode: 0=DCV/ACV, 1=DCA, 2=resistance, 3=capacitance, 4=frequency, 5=duty, 6=diode/continuity, 7=temp |
| `0x20001026` | byte | `display_format` | Format selector (0-11), drives unit suffix and decimal placement |
| `0x20001027` | byte | `sub_mode` | Sub-mode within current mode (e.g., AC vs DC) |
| `0x20001028` | float | `measurement_value` | **Primary measurement result** — scaled float in engineering units |
| `0x2000102c` | byte | `auto_range_level` | Auto-range state: 1/2/3 for magnitude bracket |
| `0x2000102d` | byte | `range_lock` | 0 = auto-range, non-zero = manual range lock |
| `0x2000102e` | byte | `ac_dc_flag` | 0 = none, 1 = DC, 2 = AC |
| `0x2000102f` | byte | `decimal_shift` | Decimal point position (0-4), incremented during range scaling |
| `0x20001030` | byte | `unit_index` | Unit string table index (see unit table below) |
| `0x20001032` | ? | `secondary_value` | Secondary display value (shown in certain modes) |
| `0x20001035` | byte | `data_ready` | Non-zero when new measurement data available |
| `0x20001038` | float | `raw_measurement` | Unscaled raw measurement from FPGA |
| `0x2000103c` | byte | `raw_scale` | Scale factor for raw data |
| `0x20001058` | byte | `theme_index` | UI theme selector (indexes into display data tables) |
| `0x20001062` | byte | `display_row_state` | Row selector: high nibble = base, low nibble = count |

## Stage 1: Raw Data Arrival

The FPGA sends measurement data via USART2. The raw value lands in `_DAT_20001038` (raw float) with an associated scale in `DAT_2000103c`. When `DAT_20001035` is non-zero, new data is ready for processing.

## Stage 2: Scaling and Auto-Range (FUN_080028e0)

This 768-byte function is the core measurement processor. It:

### 2a. Converts raw to engineering units

```c
// Convert raw values to double-precision for accurate scaling
double raw_dp = float_to_double(_DAT_20001038);  // FUN_0803ed70
double scale = int_to_double(DAT_2000103c);       // FUN_0803e5da

// Scale raw measurement
double scaled = multiply(raw_dp, power_of_10(scale));  // FUN_0803e77c

// Do the same for the primary measurement
double meas_dp = float_to_double(_DAT_20001028);
double meas_scale = int_to_double(DAT_2000102f);
double result = multiply(meas_dp, power_of_10(meas_scale));

// Combine: result = scaled_measurement / raw_reference
result = divide(result, raw_dp);  // FUN_0803eb94
```

The firmware uses **double-precision soft-float** (all FUN_0803xxxx are libgcc soft-float routines: `__floatsidf`, `__ltdf2`, `__muldf3`, `__divdf3`, `__fixdfsi`, `__floatunsidf`, etc.) because the Cortex-M4 FPU only handles single-precision natively.

### 2b. Determines decimal shift

The decimal shift (`DAT_2000102f`) is computed by repeatedly dividing by a threshold constant (`DAT_08002c00`) and counting how many divisions are needed:

```c
DAT_2000102f = 0;
if (compare(result, threshold) == 0) {  // FUN_0803ee0c
    DAT_2000102f = 1;
    result = multiply(result, scale_factor);  // FUN_0803e124
}
// Repeat up to 4 times...
```

This yields `DAT_2000102f` in range 0-4, representing how many orders of magnitude to shift.

### 2c. Auto-range logic

After scaling, the absolute value determines the range level:

```c
_DAT_20001028 = double_to_float(result);  // FUN_0803df48
float abs_val = ABS(_DAT_20001028);

if (abs_val >= DAT_08002c08 && DAT_2000102c >= 2) {
    DAT_2000102c = 1;   // Switch to range 1 (highest)
} else if (abs_val >= DAT_08002c0c && DAT_2000102c >= 3) {
    DAT_2000102c = 2;   // Switch to range 2
} else if (abs_val >= 10.0 && DAT_2000102c > 3) {
    DAT_2000102c = 3;   // Switch to range 3
}
```

The thresholds (verified from binary):
- `0x08002c08` = **1000.0** (switch to range 1)
- `0x08002c0c` = **100.0** (switch to range 2)
- Literal **10.0** (switch to range 3)

This gives 4 auto-range levels:

| Range Level | Condition | Example (voltage mode) | Decimal Places |
|-------------|-----------|----------------------|----------------|
| 0 | abs < 10.0 | 1.234 mV | 3-4 |
| 1 | 1000.0 ≤ abs | 1.234 V (auto-scale up) | 0-1 |
| 2 | 100.0 ≤ abs < 1000.0 | 123.4 mV | 1-2 |
| 3 | 10.0 ≤ abs < 100.0 | 12.34 mV | 2-3 |

**Key insight:** Auto-ranging is upward-only (ranges can decrease but the code structure favors stepping up). The decimal shift (`DAT_2000102f`, 0-4) combined with the range level determines the final format string selection.

### 2d. Mode-dependent unit assignment

A large switch on `meter_mode` (0-7) sets `display_format` and `unit_index`:

| meter_mode | Meaning | display_format | unit_index | Notes |
|------------|---------|---------------|------------|-------|
| 0 | Voltage | 0-5 | decimal_shift or decimal_shift+2 | Sub-modes for DC/AC, also sends FPGA cmd 0x1B |
| 1 | Current (DCA) | 1-2 (AC/DC flag) | decimal_shift | |
| 2 | Resistance | 3-4 (AC/DC flag) | decimal_shift+2 | |
| 3 | Capacitance | 5 | decimal_shift+2 | |
| 4 | Frequency | 6 | decimal_shift+5 | |
| 5 | Duty Cycle | 7 | decimal_shift+9 | |
| 6 | Diode/Continuity | 8-9 | decimal_shift+10 | AC/DC flag selects diode vs continuity |
| 7 | Temperature | 10-11 | decimal_shift+10 | AC/DC flag selects C vs F? |

After assignment, FPGA commands 0x1C and 0x1E are sent to update the FPGA's measurement configuration.

## Stage 3: Display Formatting (FUN_0800ec70)

This 1,798-byte function renders the measurement value on screen.

### 3a. Format the primary value string

```c
double value_dp = float_to_double(_DAT_20001028);  // FUN_0803ed70
sprintf_to_buffer(&string_buffer, format_string_0x80bc340, value_dp);
// Format string at 0x80bc340 is likely "%+.4g" or similar scientific notation
```

### 3b. Normalize for display

The absolute value is repeatedly divided by 10 until it falls below 20:

```c
float display_val = ABS(_DAT_20001028);
while (display_val >= 20.0) {
    display_val /= 10.0;  // Up to 4 divisions
}
// Then check if < 2.0 and multiply by 10 if so
if (display_val < 2.0) {
    display_val *= 10.0;
}
```

This normalizes the display value to the range **2.0–20.0** (single-digit + fraction), which is the classic multimeter 3.5/4.5 digit display format where the most significant digit is 0-1 (for 19999 counts) or 0-3 (for 39999 counts).

### 3c. Bar graph rendering

Two passes render an analog bar graph (like the bar on a real DMM):
- First pass: draws segments from index `(uVar19>>1)+1` to 20 (0x15) — the "upper half"
- Second pass: draws segments from 0 to `uVar19>>1` — the "lower half"

Each segment:
1. Computes position via `FUN_0803e50a(index * 5 + 0xdc)` — converts integer to double (pixel offset = index*5 + 220)
2. Calls `FUN_080298c0` to compute segment endpoints from two boundary curves
3. Converts endpoints to float via `FUN_0803df48`
4. Uses Bresenham-style line drawing between segment endpoints

The bar graph has **21 segments** (indices 0-20), each 5 pixels wide, starting at pixel offset 220 (0xDC).

### 3d. Digit rendering

After the bar graph, individual digits are rendered:

```c
// Loop from (half_index) to 10, rendering digit cells
for (i = half_index; i <= 10; i++) {
    pos = FUN_0803e50a(i * 10 + 0xdc);           // X position
    FUN_080298c0(curve_data, param_2, pos, ...);  // Compute cell bounds
    sprintf_to_buffer(&string_buffer, digit_format_0x80bc040, i * 2);  // Format digit
    // ... divide, scale, render each digit at 12x12 pixels
    FUN_08008154(x, y, 0xc, 0xc);  // Render at 12x12 pixel cell
}
```

The digit format string at `0x80bc040` is likely `"%d"` — each digit rendered individually.

### 3e. Unit suffix and secondary display

```c
// Main reading at large size
FUN_08008154(0x28, 0x50, 0xf0, 0x3f);  // x=40, y=80, w=240, h=63

// AC/DC indicator (if mode 2 and appropriate flags)
if (DAT_2000102e == 2 && (DAT_20001027 == 1 && DAT_20001025 == 0 || DAT_20001025 == 1)) {
    sprintf_to_buffer(&string_buffer, format_0x0800f3b8, _DAT_20001032);
    FUN_08008154(0x82, 0x44, 0x3c, 0x10);  // Secondary value at x=130, y=68
}

// Unit label (if not in manual range lock)
if (DAT_2000102d == 0 && DAT_2000102c < 5) {
    char *unit_format = unit_table[DAT_2000102c];  // Table at 0x0800f3e4
    double val_dp = float_to_double(_DAT_20001028);
    sprintf_to_buffer(&string_buffer, unit_format, val_dp);
}
FUN_08008154(0x28, 0x50, 0xf0, 0x3f);  // Main value display area

// Range indicator (if not -1)
if (DAT_20001030 != -1) {
    FUN_08008154(0x102, 0x70, 0x34, 0x1c);  // Range badge at x=258, y=112
}
```

## Format Strings (Verified from Binary)

### Precision Format Strings (MCU flash @ 0x080b532c)

| Address | String | Usage |
|---------|--------|-------|
| `0x080b532c` | `"%.0f"` | 0 decimal places |
| `0x080b5331` | `"%.1f"` | 1 decimal place |
| `0x080b5336` | `"%.2f"` | 2 decimal places |
| `0x080b533b` | `"%.3f"` | 3 decimal places |
| `0x080b5340` | `"%.4f"` | 4 decimal places |

### Unit Format Strings (MCU flash @ 0x080b5c03)

| Address | String | Usage |
|---------|--------|-------|
| `0x080b5c03` | `"%.0f%s"` | Value + unit suffix (0 decimals) |
| `0x080b5c0a` | `"%.1f%s"` | Value + unit suffix (1 decimal) |
| `0x080b5c11` | `"%.2f%s"` | Value + unit suffix (2 decimals) |
| `0x080b5c18` | `"%.3f%s"` | Value + unit suffix (3 decimals) |
| `0x080b5c1f` | `"%.4f%s"` | Value + unit suffix (4 decimals) |

### Measurement Display Strings (MCU flash @ 0x08015a64)

| Address | String | Usage |
|---------|--------|-------|
| `0x08015a64` | `"%s%.2f%s"` | prefix + 2-decimal value + suffix |
| `0x08015a70` | `"%s%.1f%s"` | prefix + 1-decimal value + suffix |
| `0x08015aa0` | `"%.2fmV"` | Millivolt display |
| `0x08015aa8` | `"%.2fV"` | Volt display |

### Unit Suffix Table — STORED IN SPI FLASH

The unit suffix string pointers at `0x0800f3e4` point to addresses `0x080bc32c–0x080bc340`, which are **beyond the MCU flash binary** (which ends at `0x080b7680`). These are in the **external SPI flash** (Winbond W25Q128), memory-mapped into the MCU address space. This means unit strings (V, mV, A, mA, kΩ, etc.) are part of the UI asset system, not firmware — and may vary by language.

### Other Strings

| Address | String | Usage |
|---------|--------|-------|
| `0x0800f3b0` | `"HOLD"` | Manual range hold indicator |
| `0x0800f3b8` | `"%d Hz"` | Frequency mode secondary value |
| `0x0800ebec` | `"%d.%d"` | Integer.fraction display (e.g., "12.34") |
| `0x0800ebfc` | `"%02d %03d"` | Two-field display (likely time or frequency) |

## Display Layout

```
┌──────────────────────────────────────┐
│ [Bar graph: 21 segments, 5px each]   │  y ≈ 0x18-0x30
├──────────────────────────────────────┤
│                                      │
│   -12.345 mV                         │  y=0x50, 240×63px
│                    AC  0.023         │  y=0x44, secondary
│                                      │
│                           [mV]       │  y=0x70, range badge
└──────────────────────────────────────┘
```

## Key Constants in Flash (Verified from Binary)

| Address | Type | Value | Purpose |
|---------|------|-------|---------|
| `0x08002bf0` | double | **1000.0** | Scale base constant |
| `0x08002bf8` | double | 0.0 | Sign extraction reference |
| `0x08002c00` | double | **10000.0** | Decimal shift threshold |
| `0x08002c08` | float | **1000.0** | Auto-range upper threshold |
| `0x08002c0c` | float | **100.0** | Auto-range middle threshold |
| `0x0800ef60` | float | **220.0** | Bar graph pixel base offset (0xDC) |
| `0x0800ef68` | double | **189.0** | Bar graph Y curve parameter |
| `0x0800ef70` | double | **184.0** | Bar graph Y curve parameter |
| `0x0800ef78` | float | **0.0** | Bar graph initial accumulator |
| `0x0800f3a0` | double | **184.0** | Digit rendering Y position |
| `0x0800f3a8` | double | **-6.0** | Digit Y offset (negative = above baseline) |
| `0x0800f3b0` | string | `"HOLD"` | Manual hold indicator |
| `0x0800f3b8` | string | `"%d Hz"` | Frequency secondary display format |
| `0x0800f3e4` | ptr[5] | → `0x080bcXXX` | **Unit suffix strings — stored in SPI flash, not MCU flash!** |

## Soft-Float Library Functions

All in the 0x0803xxxx range — these are GCC libgcc soft-float for double precision:

| Address | Proposed Name | GCC Equivalent |
|---------|--------------|----------------|
| `0x0803df48` | `double_to_float` | `__truncdfsf2` |
| `0x0803dfac` | `double_divide` | `__divdf3` |
| `0x0803e124` | `double_multiply` | `__muldf3` |
| `0x0803e450` | `float_to_int` | `__fixsfsi` |
| `0x0803e50a` | `int_to_double` | `__floatsidf` |
| `0x0803e5da` | `byte_to_double` | `__floatsidf` (small) |
| `0x0803e77c` | `double_mul_pow10` | Custom: multiply by 10^n |
| `0x0803eb94` | `double_sub` | `__subdf3` |
| `0x0803ed70` | `float_to_double` | `__extendsfdf2` |
| `0x0803ee0c` | `double_compare` | `__ltdf2` or `__cmpdf2` |

## What This Tells Us

1. **The FPGA sends raw float values** to the MCU — `_DAT_20001038` is a 32-bit float, not raw ADC counts
2. **All precision-critical math uses double-precision** soft-float (slow but accurate for 4.5-digit display)
3. **Auto-ranging uses 3 thresholds** baked into flash, with hysteresis
4. **The display is a 3.5/4.5 digit format** — values normalized to 2.0-20.0 range (classic DMM "19999 counts" style)
5. **Unit assignment is fully determined by mode + range** — no complex negotiation with FPGA
6. **Bar graph = 21 segments** at 5px spacing, Bresenham-rendered

## Meter Mode Dispatch (FUN_08004d60 — 3,568 bytes, unmapped gap)

This previously-unmapped function is the **main meter state machine**. Located in the 21.4KB gap at 0x08002BE0–0x08008154. Identified by disassembly from binary.

- **Base register:** `r5 = 0x200000F8` — the meter state struct base
- **Dispatch variable:** `[r5 + 0xF68]` = `0x20001060` — indexes a 10-way jump table (TBH instruction)
- **10 cases** (0-9), corresponding to meter initialization, command dispatch, display refresh, mode transitions, and sub-mode selection

### Case 0: Initialization

Enables USART2 TX interrupt (`0x4000440C |= 0x2000`), clears accumulators:
- `0x20001040/44/48` = `0x7FC00000` (NaN — marks as uninitialized)
- `0x2000102D` = `0x0101` (range_lock initial)
- `0x20001030` = `0xFF` (unit_index = unset)
- `0x20001027` = 0 (sub_mode cleared)
- `0x20001034` = 0 (data_flag cleared)

### Case 1: FPGA Command Dispatch

Sends command 0x1C via `FUN_0803acf0` (fpga_send_command) with queue handle from `0x20002D6C`.

### Case 4: Sub-Mode Selection

Contains a nested 7-way jump table indexed by `[r5 + 0xF6A] & 0xF` (display_row low nibble). Sends FPGA commands 0x1F and 0x20 for component tester mode.

### Key Finding: Float Values NOT Written Here

Despite being 3,568 bytes, this function **never writes to `0x20001038` (raw_measurement) or `0x20001028` (processed_measurement)**. The float values must be written by a different function — likely the actual FPGA response handler, which hasn't been identified yet. This is consistent with the P0 team's finding that the FPGA communication path is still unknown.

## Still Unknown

- **Unit suffix strings** at 0x080bc32c–0x080bc340 are in SPI flash — need to dump SPI flash contents to read them (or capture at runtime)
- **How `_DAT_20001038` is populated** — does the USART2 ISR write it directly, or is there an intermediate processing step?
- **FPGA data encoding** — does the FPGA send IEEE 754 floats, fixed-point integers, or raw ADC counts that get converted?
- **Format strings at 0x080bc040 and 0x080bc340** — these pointers are also in SPI flash (the primary number format and value display format)
- **Calibration constant loading** — the 6 RAM calibration table pairs (0x20000358–0x20000434) must be loaded from SPI flash at boot, but the loader function hasn't been identified
