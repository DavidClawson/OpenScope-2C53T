# Function and Variable Map

## Key Discovery: String Table Architecture

The firmware uses a **two-region string system**:

- **Region 1 (0x080B3000-0x080B7680):** Strings included in the firmware update `.bin` file. These are the ones Ghidra found directly. Contains file paths, format strings, and some UI labels.

- **Region 2 (0x080BB000-0x080C0000+):** Strings stored in flash **beyond** the update file. These persist across firmware updates and are likely written at the factory. Most UI text (menu labels, mode names, translations) lives here. The decompiled code references these as `0x80bcXXX` addresses.

This means the firmware update only replaces ~733KB of a 1MB flash. The remaining ~267KB holds string tables, possibly additional font data, and calibration.

**Implication:** To get the full string table, you'd need to dump the entire MCU flash via SWD/JTAG, or connect via USB Sharing mode and see if the full flash is accessible.

## Named Functions

### Display Primitives

| Address | Name | Signature | Description |
|---|---|---|---|
| 08032f6c | `draw_text` | (string_addr, x, y) | Draws text string at screen coordinates |
| 08008154 | `draw_ui_element` | (x, y, w, h, label, data, color, style) | Draws a UI widget/button with label and data |
| 080003b4 | `sprintf_to_buffer` | (buffer, format_str, value, ...) | Formats a value into a string buffer |
| 08033cfc | `memory_alloc` | (size) → ptr | Allocates a memory buffer, returns pointer |

### String Utilities

| Address | Name | Signature | Description |
|---|---|---|---|
| 0802d534 | `uart_print_string` | (string_addr) | Outputs a string (USART debug or FPGA?) |
| 0802dc40 | `string_compare` | (str1, str2) → int | Compares two strings, returns 0 if equal |
| 0802d8b8 | `string_copy_or_parse` | (dest, src, flags) → uint | String manipulation, 223 lines |

### GPIO / Hardware

| Address | Name | Signature | Description |
|---|---|---|---|
| 080302fc | `gpio_configure_pins` | (gpio_base, pin_config) | Sets CRL/CRH pin modes, 4 bits per pin |
| 080304e0 | `gpio_read_pin` | (gpio_base, pin_mask) → int | Reads GPIO pin state |

### I2C (Touch Panel)

| Address | Name | Signature | Description |
|---|---|---|---|
| 08036848 | `i2c_init` | (i2c_base, config) | Initialize I2C peripheral |
| 08036830 | `i2c_enable` | (i2c_base) | Enable I2C peripheral |
| 0803683c | `i2c_transfer` | (i2c_base, direction) → int | I2C read/write transfer |

### UI Screens

| Address | Name | Lines | Description |
|---|---|---|---|
| 08019e98 | `main_event_loop` | 2,530 | The main UI state machine. Handles all mode switching, user input, display updates |
| 08015f50 | `draw_oscilloscope_screen` | 782 | Oscilloscope display with waveform rendering. Switches on `ui_state_flags & 0xf` |
| 0800e79c | `draw_measurement_values` | 69 | Measurement readout display |
| 0800ec70 | `draw_measurement_display` | 311 | Measurement display with floating-point math (auto-ranging) |
| 0800bd84 | `draw_mode_label` | 13 | Draws current mode label + selection indicator |
| 0800bde0 | `draw_channel_info` | 70 | Channel information with formatted strings |
| 08018324 | `draw_settings_menu` | 123 | Settings menu with UI elements |
| 080096e8 | `draw_status_indicator_1` | 45 | Status display |
| 08009a94 | `draw_status_indicator_2` | 46 | Status display |

### Interrupt Handlers

| Address | Vector | Name | Description |
|---|---|---|---|
| 0802A995 | 15 (SysTick) | `systick_handler` | System tick timer |
| 08009C11 | 25 (EXTI3) | `exti3_continuity_handler` | Continuity detection pin. **Added in V1.2.0** |
| 08009671 | 28 (DMA1_Ch2) | `dma1_ch2_handler` | DMA transfer complete |
| 0802E8E5 | 36 (USB_LP) | `usb_interrupt_handler` | USB device interrupt |
| 0802E71D | 45 (TIM3) | `timer3_interrupt_handler` | Timer for measurements/delays |
| 0802E7B5 | 54 (USART2) | `usart2_irq_handler` | UART RX interrupt |
| 0802E78D | 59 (TIM8_BRK) | `timer8_break_handler` | PWM for buzzer or signal generator |

### USART2 Protocol Handler

| Address | Name | Description |
|---|---|---|
| 0802e7bc | `usart2_protocol_handler` | Called from USART2 IRQ context. Performs string comparisons and prints — suggests a **text-based command protocol** with the FPGA (not binary commands like the 1013D). This is a significant architectural difference from the 1013D. |

## Named Global Variables

### Display System

| Address | Name | References | Description |
|---|---|---|---|
| 20008350 | `viewport_x` | 282 | Display viewport X origin |
| 20008352 | `viewport_y` | 284 | Display viewport Y origin |
| 20008354 | `viewport_width` | 201 | Display viewport width in pixels |
| 20008356 | `viewport_height` | 111 | Display viewport height in pixels |
| 20008358 | `framebuffer_ptr` | 152 | Pointer to 16-bit RGB565 pixel buffer |
| 20008360 | `string_format_buffer` | 32 | Buffer for sprintf output |

### Font System

| Address | Name | References | Description |
|---|---|---|---|
| 20001078 | `font_base_ptr` | 56 | Base pointer to font data |
| 2000107c | `font_table_ptr` | 63 | Pointer to font glyph lookup table |
| 20001080 | `font_loaded_flag` | 33 | Flag indicating font data is loaded |

### UI State

| Address | Name | References | Description |
|---|---|---|---|
| 20000125 | `device_mode` | 142 | Current device mode (scope/DMM/siggen) |
| 20001058 | `current_mode_index` | 41 | Index into mode/selection tables |
| 20001062 | `ui_state_flags` | varies | UI state bitmask, lower nibble used in switch |
| 20001061 | `ui_sub_state` | varies | Sub-state within current UI mode |
| 20001063 | `ui_state_flag_2` | varies | Additional state flag |

### System

| Address | Name | References | Description |
|---|---|---|---|
| 200000fc | `system_state` | 61 | Overall system state |
| 200000fd | `system_flag` | 43 | System flag |
| 2000010c | `config_value` | 80 | Configuration setting |
| 20000130 | `setting_value` | 51 | Another setting |

## Framebuffer Pixel Access Pattern

The display uses 16-bit RGB565 pixels with this access pattern:
```c
pixel_addr = framebuffer_ptr +
    ((y - viewport_y) * viewport_width + (x - viewport_x)) * 2;
```

This confirms:
- 16-bit color (RGB565)
- Row-major pixel layout
- Viewport clipping is done manually in the drawing code
