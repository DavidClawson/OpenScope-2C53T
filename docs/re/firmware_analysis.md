# Firmware Analysis

## Firmware Versions

| Version | File | Size | Date | Stack Pointer | Active IRQs |
|---|---|---|---|---|---|
| V1.0.3 | APP_2C53T_V1.0.3_240814.bin | 452 KB | Aug 2024 | 0x20036CD0 | 5 |
| V1.0.7 | APP_2C53T_V1.0.7_241205.bin | 475 KB | Dec 2024 | 0x20036D10 | 5 |
| V1.1.2 | APP_2C53T_V1.1.2_250929.bin | 733 KB | Sep 2025 | 0x20036F90 | 5 |
| V1.2.0 | APP_2C53T_V1.2.0_251015.bin | 734 KB | Oct 2025 | 0x20036F90 | 6 |

### Version Changes (from Update Log)

**V1.1.0:** Anti-aliased fonts, added 6 languages, sinusoidal interpolation, zoom during pause
**V1.1.1:** Removed the 6 added languages (Russian, Japanese, Korean, Spanish, Portuguese, German... but Spanish/Portuguese/German strings are still present in V1.2.0)
**V1.1.2:** Fixed measurement bugs, pause display errors, trigger errors
**V1.2.0:** Fixed buzzer discontinuity in multimeter on/off mode. **Added EXTI3 interrupt handler.**

### Key Finding: EXTI3 Added in V1.2.0

V1.2.0 added exactly one new interrupt handler: **EXTI3** (external interrupt on GPIO pin 3). This corresponds to the buzzer fix for continuity mode. This pin is likely the **continuity detection signal** — the exact signal you'd hook into for adding a screen flash on continuity.

## Binary Structure (V1.2.0)

```
Offset 0x00000000 (addr 0x08000000): Vector table
Offset 0x00000140 (addr 0x08000140): Code begins
Offset 0x0003F1C8 (addr 0x0803F1C8): Code ends, data begins
Offset 0x000B7680 (addr 0x080B7680): Binary ends
```

- **Not encrypted or compressed** — entropy analysis confirms raw ARM code + data
- **Strings are plaintext** — directly readable and patchable
- **Multi-language** — English, German, Spanish, Portuguese throughout

## Decompiled Output

**File:** `decompiled_2C53T.c` (34,618 lines, 292 functions)

Generated via Ghidra headless analysis with ARM Cortex-M4, base address 0x08000000.

### Largest Functions (likely main modules)

| Function | Lines | Likely Purpose |
|---|---|---|
| FUN_08019e98 | 2,530 | Main UI event loop / state machine |
| FUN_0802a664 | 1,679 | Unknown (possibly waveform processing) |
| FUN_08030524 | 1,370 | Unknown (possibly signal generator) |
| FUN_08031f20 | 1,308 | Unknown (possibly multimeter) |
| FUN_0801d2ec | 972 | Unknown |
| FUN_0801f6f8 | 883 | Unknown |
| FUN_0802c250 | 852 | Unknown |

### Most Referenced Global Variables

These are the "hot" variables — understanding them is key to understanding the firmware:

| Variable | References | Likely Purpose |
|---|---|---|
| DAT_20008352 | 284 | Display buffer or state pointer |
| DAT_20008350 | 282 | Display buffer or state pointer |
| DAT_20008354 | 201 | Display dimension or count |
| DAT_20008358 | 152 | Buffer pointer |
| DAT_20000125 | 142 | Mode or state flag |
| DAT_20008356 | 111 | Display dimension or count |
| DAT_2000010c | 80 | Configuration value |
| DAT_2000107c | 63 | Font or resource pointer |

## Key Strings and Addresses

**Full list:** `strings_with_addresses.txt` (290 entries)

### Strings Most Useful for RE

| Address | String | Use |
|---|---|---|
| 080b58de | "Auto Shutdown" | Menu label → find auto-off settings UI |
| 080b58ae | "Auto shutdown soon" | Warning → find shutdown timer countdown |
| 080b5dd0 | "Sound and light" | Settings → find buzzer/display config |
| 080b5ed6 | "Continuity" | Multimeter mode → find continuity detection code |
| 080b5b54 | "Multimeter" | Mode switch → find mode state machine |
| 080b5bbc | "Signal Generator" | Mode switch |
| 080b5ce8 | "Oscilloscope Settings" | Settings menu |
| 080b5dab | "Factory Reset" | Reset handler |
| 080b5e6b | "Startup on Boot" | Boot configuration |
| 080b535e | "USB Sharing" | USB mass storage mode |
| 080b5ad2 | "2:/Screenshot file/%d.bmp" | Screenshot save path |
| 080b5681 | "3:/System file/%d.jpg" | UI asset loading path |
| 080b5841 | "3:/System file/9999.bin" | Configuration file |
| 080b4a71 | "Version:" | About screen |
| 080b5eb4 | "Battery low, charge now" | Low battery handler |
| 080b5de0 | "Exceeded Limit" | Measurement range exceeded |

### Display Function

**FUN_08032f6c** — Text rendering function. Called with:
- param_1: string address (e.g., 0x80bc612)
- param_2: X coordinate
- param_3: Y coordinate

### GPIO Configuration Function

**FUN_080302fc** — GPIO pin mode configuration. Sets CRL/CRH registers 4 bits at a time. Called with:
- param_1: GPIO base address (0x40010C00 = GPIOB, 0x40011000 = GPIOC)
- param_2: pin configuration structure
