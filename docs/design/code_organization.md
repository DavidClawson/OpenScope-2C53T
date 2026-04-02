# Code Organization

## Current State

The project grew organically during one RE session. It needs restructuring before it grows further.

## Proposed Source Tree

```
OpenScope-2C53T/
│
├── docs/                          # Documentation (what we have now)
│   ├── README.md                  # Project overview
│   ├── hardware.md                # MCU, peripherals, memory map
│   ├── firmware_analysis.md       # RE findings
│   ├── freertos_tasks.md          # Task architecture
│   ├── fpga_protocol.md           # FPGA communication
│   ├── function_map.md            # Named functions/variables
│   ├── rtos_analysis.md           # FreeRTOS identification
│   ├── community_issues.md        # User-reported bugs
│   ├── roadmap.md                 # Project roadmap
│   ├── use_cases.md               # Use case analysis
│   ├── feature_brainstorm.md      # Feature ideas
│   ├── industry_modules.md        # Industry-specific modules
│   ├── accessories.md             # Add-on hardware designs
│   ├── fft_design.md              # FFT implementation design
│   ├── code_organization.md       # This file
│   ├── re_guide.md                # Reverse engineering guide
│   ├── reference_projects.md      # Related projects
│   └── device_testing_plan.md     # Hardware testing checklist
│
├── firmware/                      # Custom firmware source
│   ├── Makefile
│   ├── ld/
│   │   └── gd32f307.ld           # Linker script
│   │
│   ├── src/                       # Application source
│   │   ├── main.c                 # Entry point, FreeRTOS task creation
│   │   │
│   │   ├── tasks/                 # FreeRTOS task implementations
│   │   │   ├── display_task.c     # UI rendering (receives commands via queue)
│   │   │   ├── scope_task.c       # Oscilloscope acquisition + processing
│   │   │   ├── input_task.c       # Button/touch input handling
│   │   │   ├── fpga_task.c        # FPGA data readout + calibration
│   │   │   ├── dvom_task.c        # Digital voltmeter TX/RX
│   │   │   └── usb_task.c         # USB mass storage + streaming
│   │   │
│   │   ├── drivers/               # Hardware abstraction
│   │   │   ├── lcd.c / lcd.h      # ST7789V display driver (EXMC/FSMC)
│   │   │   ├── fpga.c / fpga.h    # FPGA USART2 command interface
│   │   │   ├── fpga_spi.c / .h    # FPGA SPI2 data transfer
│   │   │   ├── touch.c / touch.h  # Touch panel I2C driver
│   │   │   ├── buttons.c / .h     # Physical button GPIO scanning
│   │   │   ├── buzzer.c / .h      # Buzzer PWM control
│   │   │   ├── flash.c / flash.h  # SPI flash (W25Q128) driver
│   │   │   ├── battery.c / .h     # Battery ADC monitoring
│   │   │   ├── usb.c / usb.h      # USB device driver
│   │   │   └── dac.c / dac.h      # Signal generator DAC output
│   │   │
│   │   ├── ui/                    # User interface
│   │   │   ├── ui.c / ui.h        # UI framework (menus, widgets, navigation)
│   │   │   ├── scope_ui.c / .h    # Oscilloscope display (grid, waveforms, measurements)
│   │   │   ├── meter_ui.c / .h    # Multimeter display
│   │   │   ├── siggen_ui.c / .h   # Signal generator display
│   │   │   ├── settings_ui.c / .h # Settings menus
│   │   │   ├── fonts.c / fonts.h  # Font data and text rendering
│   │   │   └── colors.h           # Color constants (RGB565)
│   │   │
│   │   ├── dsp/                   # Signal processing
│   │   │   ├── fft.c / fft.h      # FFT engine (wraps CMSIS-DSP)
│   │   │   ├── measurements.c / .h # Auto-measurements (freq, Vpp, RMS, duty, etc.)
│   │   │   ├── trigger.c / .h     # Trigger detection and configuration
│   │   │   └── math_channel.c / .h # Math operations (A+B, A-B, A×B)
│   │   │
│   │   ├── decode/                # Protocol decoders
│   │   │   ├── decode.c / .h      # Decoder framework (common interface)
│   │   │   ├── uart_decode.c / .h # UART/RS-232 decoder
│   │   │   ├── i2c_decode.c / .h  # I2C decoder
│   │   │   ├── spi_decode.c / .h  # SPI decoder
│   │   │   ├── can_decode.c / .h  # CAN bus decoder (from waveform)
│   │   │   ├── can_native.c / .h  # CAN bus via MCU controller (if available)
│   │   │   └── onewire_decode.c   # 1-Wire decoder
│   │   │
│   │   ├── modules/               # Application modules (JSON procedure runner)
│   │   │   ├── module_loader.c / .h # Load/parse JSON procedure files
│   │   │   ├── guided_test.c / .h   # Guided test execution engine
│   │   │   └── pass_fail.c / .h     # Pass/fail evaluation against criteria
│   │   │
│   │   └── util/                  # Utilities
│   │       ├── fatfs_glue.c / .h  # FatFS filesystem integration
│   │       ├── config.c / .h      # Settings save/load to flash
│   │       ├── printf.c / .h      # Lightweight printf implementation
│   │       └── delay.c / delay.h  # Delay functions (ms/us)
│   │
│   ├── include/                   # Project-wide headers
│   │   ├── FreeRTOSConfig.h       # FreeRTOS configuration
│   │   ├── gd32f30x_libopt.h     # GD32 peripheral library selection
│   │   └── board.h                # Pin definitions, clock config, hardware constants
│   │
│   ├── gd32f30x_lib/             # GD32 HAL (cloned, .gitignore'd or submodule)
│   ├── FreeRTOS/                  # FreeRTOS kernel (cloned, .gitignore'd or submodule)
│   └── build/                    # Build output (.gitignore'd)
│
├── emulator/                      # Emulation tools
│   ├── emu_2c53t.py              # Unicorn-based emulator
│   ├── lcd_server.py             # WebSocket LCD framebuffer server
│   ├── pyproject.toml            # Python dependencies (uv)
│   └── renode/                   # Renode platform emulation
│       ├── gd32f307_2c53t.repl   # Platform description
│       ├── run_2c53t.resc        # Main run script
│       ├── run_diagnostic.resc   # Diagnostic variant
│       ├── rcu_peripheral.py     # Clock controller simulator
│       ├── fmc_peripheral.py     # Flash controller simulator
│       ├── adc_peripheral.py     # ADC simulator
│       └── fpga_dvom_sim.py      # FPGA/DVOM protocol responder
│
├── frontend/                      # React browser UI
│   ├── src/
│   │   ├── App.jsx               # Device photo with LCD overlay + button hotspots
│   │   ├── App.css               # Styling
│   │   └── main.jsx              # Entry point
│   ├── public/
│   │   └── scope.jpg             # Device photo
│   ├── package.json
│   └── vite.config.js
│
├── modules/                       # JSON procedure files (by industry)
│   ├── automotive/
│   │   ├── compression_test.json
│   │   ├── injector_analysis.json
│   │   └── can_decode.json
│   ├── hvac/
│   │   ├── compressor_current.json
│   │   └── capacitor_test.json
│   ├── ham_radio/
│   │   ├── antenna_analyzer.json
│   │   └── harmonic_check.json
│   └── education/
│       ├── rc_circuit.json
│       └── component_id.json
│
├── reverse_engineering/           # RE artifacts (separate from docs)
│   ├── decompiled_2C53T.c        # Original decompilation
│   ├── decompiled_2C53T_v2.c     # Updated with named functions
│   ├── strings_with_addresses.txt
│   ├── string_references.txt
│   └── ghidra_scripts/
│       ├── ApplyNames.java
│       ├── DecompileAll.java
│       ├── DumpStrings.java
│       └── FindStringRefs.java
│
├── .gitignore
├── LICENSE                        # GPL v3
└── README.md                      # → docs/README.md (or top-level overview)
```

## Key Design Principles

### 1. Each module is a standalone .c/.h pair

Every feature should be self-contained with a clean header defining its public interface. No function should reach across module boundaries except through the defined API.

```c
// Good: clean interface
#include "fft.h"
fft_process(samples, num_samples, sample_rate);
fft_get_magnitude(magnitudes, num_bins);

// Bad: reaching into internals
extern float32_t fft_output[4096];  // don't do this
```

### 2. Decoder plugin interface

All protocol decoders implement the same interface:

```c
typedef struct {
    const char *name;           // "UART", "I2C", "SPI", "CAN"
    void (*init)(void *config);
    int  (*decode)(int16_t *ch1, int16_t *ch2, uint16_t num_samples,
                   decode_result_t *results, int max_results);
    void (*draw_overlay)(lcd_context_t *lcd, decode_result_t *results, int count);
    void (*cleanup)(void);
} decoder_t;

// Register decoders at compile time
extern const decoder_t uart_decoder;
extern const decoder_t i2c_decoder;
extern const decoder_t spi_decoder;
extern const decoder_t can_decoder;
```

Adding a new decoder = write one .c file implementing this interface + add it to the decoder list. No changes to core code needed.

### 3. Display task receives commands, doesn't know about features

The display task is a generic renderer. Feature modules send display commands:

```c
// Any task can request a display update
display_send_cmd(DISPLAY_CMD_DRAW_WAVEFORM);
display_send_cmd(DISPLAY_CMD_DRAW_FFT);
display_send_cmd(DISPLAY_CMD_DRAW_MEASUREMENTS);
display_send_cmd(DISPLAY_CMD_DRAW_DECODE_OVERLAY);
```

### 4. Hardware drivers hide register details

No module outside of `drivers/` should ever write to a hardware register directly:

```c
// Good
lcd_fill_rect(10, 20, 100, 50, COLOR_YELLOW);
fpga_set_timebase(TIMEBASE_50US);
battery_get_voltage();

// Bad
*(volatile uint32_t *)0x60020000 = pixel_data;
*(volatile uint32_t *)0x4000440C = command_byte;
```

### 5. Configuration is centralized

All persistent settings go through `config.c` which handles save/load to flash:

```c
config_set_int("scope.timebase", TIMEBASE_50US);
config_set_int("fft.window", FFT_WINDOW_HANNING);
config_set_int("fft.size", 4096);
config_save();  // writes to flash
```

## Build System

The Makefile should support feature flags:

```makefile
# Enable/disable features at compile time
FEATURES ?= FFT DECODE_UART DECODE_I2C DECODE_CAN MODULES

# Conditional compilation
ifeq ($(findstring FFT,$(FEATURES)),FFT)
  SRCS += src/dsp/fft.c
  CFLAGS += -DFEATURE_FFT
endif

ifeq ($(findstring DECODE_UART,$(FEATURES)),DECODE_UART)
  SRCS += src/decode/uart_decode.c
  CFLAGS += -DFEATURE_DECODE_UART
endif
```

This lets us produce different firmware builds:
```bash
make FEATURES="FFT DECODE_UART DECODE_I2C"           # electronics bench
make FEATURES="FFT DECODE_CAN MODULES"                # automotive
make FEATURES="FFT"                                     # minimal
make                                                    # everything
```

## Migration Plan

Don't reorganize everything at once. Move files as we work on them:

1. **Now:** Move RE artifacts to `reverse_engineering/`, create `firmware/src/drivers/` and move `lcd.c`
2. **When adding FFT:** Create `firmware/src/dsp/fft.c`
3. **When adding UART decode:** Create `firmware/src/decode/` with decoder interface
4. **When adding UI:** Create `firmware/src/ui/` with scope/meter/siggen screens
5. **When adding modules:** Create `modules/` with JSON files
