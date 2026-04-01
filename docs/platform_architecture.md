# Platform Architecture: From Firmware to SDK

*Goal: Structure the codebase so that the hard reverse-engineering and hardware bring-up work becomes a reusable platform. Developers should be able to build custom applications — a different meter UI, a dedicated automotive tool, a logic analyzer — without understanding FPGA protocols, EXMC timing, or SPI flash layout.*

## The Vision

The FNIRSI 2C53T with our bootloader and firmware isn't just a scope — it's a **programmable handheld instrument platform**: screen, buttons, analog inputs, signal generator, 16MB storage, battery, and optionally WiFi (ESP32 mod). Our job is to make the hard parts invisible and the creative parts easy.

## Layer Architecture

```
┌─────────────────────────────────────────────┐
│ Layer 4: Applications                        │
│                                              │
│  Our scope UI, meter UI, siggen UI           │
│  Someone's custom automotive diagnostic tool │
│  Someone's dedicated logic analyzer          │
│  A ham radio instrument suite                │
│  An educational electronics trainer          │
│                                              │
│  (Swappable. This is where creativity lives) │
├─────────────────────────────────────────────┤
│ Layer 3: Services / Middleware                │
│                                              │
│  FFT engine         Protocol decoders        │
│  Measurement engine  Config save/load        │
│  Data logging        Buzzer/audio system     │
│  Theme system        Font rendering          │
│                                              │
│  (Reusable building blocks, hardware-agnostic)│
├─────────────────────────────────────────────┤
│ Layer 2: Hardware Abstraction Layer (HAL)     │
│                                              │
│  LCD driver          FPGA communication      │
│  SPI flash driver    Button/touch input      │
│  DAC signal output   ADC sample capture      │
│  Power management    USB interface           │
│                                              │
│  (Board-specific, clean API at the boundary) │
├─────────────────────────────────────────────┤
│ Layer 1: Platform                            │
│                                              │
│  FreeRTOS            AT32/GD32 HAL library   │
│  Linker scripts      Startup code            │
│  Custom bootloader   CMSIS-DSP               │
│                                              │
│  (Chip-specific, rarely touched)             │
└─────────────────────────────────────────────┘
```

## What Each Layer Provides

### Layer 1 — Platform
The foundation. FreeRTOS provides tasks, queues, semaphores. The vendor HAL (AT32 or GD32) provides register-level peripheral access. CMSIS-DSP provides optimized math. The bootloader provides safe firmware updates. **Developers above this layer never touch these directly.**

### Layer 2 — Hardware Abstraction
This is where the reverse engineering pays off. The FPGA protocol, LCD timing, SPI flash filesystem — all the hard-won knowledge is encapsulated behind clean C APIs. A developer calls `hal_acq_read_samples()` and gets back an array of ADC values. They don't know or care that this involves SPI3 on remapped JTAG pins talking to a Gowin FPGA.

**Key APIs (see developer_experience.md for full signatures):**

| Header | What it abstracts |
|--------|-------------------|
| `hal/display.h` | LCD init, windowed pixel writes, fill, dimensions |
| `hal/acquisition.h` | Timebase config, trigger setup, sample readout |
| `hal/siggen.h` | DAC waveform output, DMA-driven playback |
| `hal/buttons.h` | Button scan, debounce, event queue |
| `hal/storage.h` | SPI flash read/write, filesystem operations |
| `hal/power.h` | Battery level, backlight, auto-shutdown |
| `hal/buzzer.h` | Tone generation, PWM audio, patterns |
| `hal/system.h` | Clock, delay, uptime, reset reason |

### Layer 3 — Services
Hardware-agnostic building blocks. The FFT engine takes an array of samples and returns frequency bins — it doesn't know where the samples came from. The protocol decoders take sample arrays and return decoded frames. The theme system provides colors without knowing the LCD controller.

**These are the building blocks a custom application would pick from.** Someone building a dedicated automotive tool would use the measurement engine and data logging but skip the FFT. A logic analyzer app would use protocol decoders but skip the signal generator.

### Layer 4 — Applications
The UI and user-facing logic. Our scope screen, meter screen, and signal generator screen are applications built on layers 1-3. But they're not special — they're just the default applications. Someone could replace them entirely while reusing everything underneath.

## Design Principles

### 1. Headers are the contract
Each layer communicates through header files with documented function signatures. If it's not in the header, it's not part of the API. A developer reading `hal/acquisition.h` should understand how to capture waveforms without reading the `.c` implementation.

### 2. No reaching through layers
Application code should never include AT32 HAL headers or write to hardware registers directly. If an app needs something the HAL doesn't expose, the answer is to extend the HAL — not to bypass it.

### 3. Services don't depend on each other
The FFT engine doesn't `#include "protocol_decode.h"`. Services are independent building blocks that applications compose. This keeps the dependency graph flat and allows mix-and-match module selection.

### 4. Board-specific code stays in one place
All pin assignments, peripheral configs, and timing parameters live in the `boards/` directory. Porting to a new board means implementing the HAL interfaces for that board's hardware — nothing above the HAL changes.

### 5. The emulator is a first-class board
The Renode/emulator build is just another board target. It implements the same HAL interfaces with simulated hardware. This means any application code that works in the emulator works on hardware, and developers without the physical device can still contribute.

## Current State vs Target

| Component | Current State | Target |
|-----------|---------------|--------|
| LCD driver | Working, mostly clean API | Formalize as `hal/display.h` |
| FPGA/ADC | Not yet implemented | Formalize as `hal/acquisition.h` after RE |
| Button input | Working on emulator, not on hardware | Formalize as `hal/buttons.h` |
| SPI flash | Not yet implemented | Formalize as `hal/storage.h` |
| Signal generator | Working (DDS module) | Wrap in `hal/siggen.h` |
| Font rendering | Working, clean API | Already service-layer quality |
| Theme system | Working, clean API | Already service-layer quality |
| FFT engine | Working, standalone | Already service-layer quality |
| Protocol decoders | Working, standalone | Already service-layer quality |
| Config/save | Working | Already service-layer quality |
| Power management | Basic (PC9 hold, backlight) | Formalize as `hal/power.h` |
| Buzzer | Not yet implemented | Formalize as `hal/buzzer.h` |
| Module API | Designed (module_api.h) | Implement and validate |

Much of the service layer (FFT, decoders, config, themes, fonts) is already clean and reusable. The main work is formalizing the HAL boundary and completing the hardware-specific drivers as we finish reverse engineering.

## What This Enables

- **Custom UIs:** Someone who hates our meter layout can write their own, using our sampling and measurement infrastructure
- **Single-purpose tools:** A dedicated automotive diagnostic app, a dedicated ham radio instrument, a classroom demo tool — each using only the services they need
- **Board ports:** When someone wants to run this on an OWON or Hantek scope with different hardware, they implement the HAL for their board and everything above it works
- **Community growth:** Low barrier to entry for contributors — write a module, test in the emulator, submit a PR. No reverse engineering required.

## Related Docs

- [Developer Experience](developer_experience.md) — User tiers, module API spec, distribution model, web configurator
- [Code Organization](code_organization.md) — Current and planned file/directory structure
- [Feature Brainstorm](feature_brainstorm.md) — Features organized by subsystem
- [Roadmap](roadmap.md) — Implementation phases and priorities
