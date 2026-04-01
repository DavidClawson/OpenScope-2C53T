# Developer & User Experience Plan

*March 2026 — planning document for making OpenScope accessible to different audiences*

## User Tiers

### Tier 1: End User ("I just want better firmware")

**Skill level:** Can follow instructions, comfortable copying files. No command line.

**Experience:** Go to GitHub releases, download a `.bin`, flash via USB.

**What this needs:**
- GitHub Actions CI that builds pre-configured firmware profiles on every tagged release
- Release page with named downloads and clear flash instructions
- Changelog and known issues list per release

**Example release page:**
```
OpenScope v0.3.0 — FNIRSI 2C53T

Downloads:
  openscope-2c53t-v0.3.0-standard.bin    (scope + DMM + siggen + FFT)
  openscope-2c53t-v0.3.0-automotive.bin   (+ CAN, K-Line, alternator/compression tests)
  openscope-2c53t-v0.3.0-full.bin         (everything, 680KB)
  openscope-2c53t-v0.3.0-minimal.bin      (scope + DMM only, 420KB)

How to flash:
  1. Power off the oscilloscope
  2. Hold MENU button, then press POWER to enter firmware upgrade mode
  3. Connect USB cable — device appears as a USB drive
  4. Copy the .bin file to the drive
  5. Disconnect USB, power cycle the device

To restore stock firmware:
  Same process using the original APP_2C53T_*.bin file

Known issues:
  - DMM auto-range not yet implemented
  - Trigger level UI incomplete
  - ...
```

Zero tools required. Just a download and a file copy.

---

### Tier 2: Customizer ("I want to pick my modules")

**Skill level:** Comfortable with a terminal OR a web browser. May or may not have dev tools.

**Two options, serving different sub-audiences:**

#### Option A: Web-Based Configurator (no tools needed)

Static site hosted on GitHub Pages. User selects modules via checkboxes, gets a custom `.bin`.

**How it works technically:**

1. Static HTML/JS site on GitHub Pages (free)
2. User checks boxes for desired modules
3. Site triggers a GitHub Actions workflow via the API, passing module selections as parameters
4. Actions runner installs `arm-none-eabi-gcc`, runs `make FEATURES="FFT CAN KLINE AUTOMOTIVE"`
5. Workflow uploads `.bin` as an artifact
6. Site polls until build completes (~60-90 seconds), provides download link

**Optimization — hybrid approach (how QMK does it):**
- Common profiles are pre-built at release time (instant download)
- Unusual/custom combinations trigger an on-demand Actions build
- Best of both worlds: fast for most users, flexible for power users

**Cost:** GitHub Actions provides 2,000 free minutes/month for public repos. Each build takes ~2-3 minutes. Supports hundreds of custom builds per month at zero cost.

**Reference projects:**
- [QMK Configurator](https://config.qmk.fm/) — keyboard firmware, very similar concept
- [ESPHome Dashboard](https://esphome.io/) — ESP32 firmware builder
- [Marlin Auto Build](https://marlinfw.org/) — 3D printer firmware

#### Option B: Local CLI Configurator (for developers)

Python script for those who prefer command line or want offline builds.

```
$ python tools/configure.py

  OpenScope Firmware Configurator (2C53T)

  Core features (always included):
    ✓ Oscilloscope (2-channel, 250 MS/s)
    ✓ Multimeter
    ✓ Signal generator

  Optional modules:                              Size est.
    [X] FFT spectrum analyzer                     +32 KB
    [X] Waterfall spectrogram                     +8 KB
    [ ] Protocol: I2C decode                      +12 KB
    [ ] Protocol: SPI decode                      +10 KB
    [X] Protocol: CAN bus decode                  +14 KB
    [X] Protocol: K-Line / KWP2000                +12 KB
    [ ] Protocol: UART decode                     +8 KB
    [X] Automotive: Compression test              +6 KB
    [X] Automotive: Alternator ripple test        +4 KB
    [ ] Component testing                         +10 KB
    [ ] Bode plot (requires siggen loopback)      +8 KB
    [ ] Math channel (A+B, A-B, A×B)             +6 KB
    [ ] XY mode                                   +4 KB
    [ ] Roll mode                                 +4 KB
    [ ] Persistence / phosphor display            +6 KB

  Flash estimate: 612 KB / 733 KB available

  [Enter] Build   [Space] Toggle   [Q] Quit
```

**Prerequisites:** Python 3, `arm-none-eabi-gcc` (script checks and provides install instructions).

Under the hood, this just sets the `FEATURES` Makefile variable and runs `make`. The Makefile already supports `FEATURES ?= FFT` — extend this to all modules.

---

### Tier 3: Module Developer ("I want to write a protocol decoder")

**Skill level:** C programmer. May or may not have oscilloscope hardware.

**Key insight:** The emulator means they never need the hardware. They write code, test in Renode, see results in the React frontend in their browser.

#### Module API

Formalize the pattern that already exists in `src/decode/` and `src/modules/`:

```c
// include/module_api.h

typedef struct {
    const char *name;            // "I2C Decode"
    const char *short_name;      // "I2C" (for status bar)
    const char *description;     // "Decodes I2C bus traffic with address/data overlay"
    const char *version;         // "1.0.0"
    const char *author;          // "contributor name"

    // Lifecycle
    void (*init)(void);
    void (*deinit)(void);

    // Processing — called each acquisition cycle
    void (*process)(const int16_t *ch1, const int16_t *ch2,
                    uint16_t num_samples, float sample_rate_hz);

    // Rendering — called by display task
    void (*draw)(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    // Input — called on button press (return true if handled)
    bool (*on_button)(button_id_t btn);

    // Optional: settings menu items
    const module_setting_t *settings;
    uint8_t num_settings;

    // Metadata for configurator
    uint32_t required_features;   // bitmask: needs scope data? siggen?
    uint32_t flash_size_est;      // bytes, for configurator size display
} openscope_module_t;

// Module registration macro
#define OPENSCOPE_MODULE(varname) \
    __attribute__((section(".modules"), used)) \
    const openscope_module_t varname
```

#### Developer workflow

1. Clone repo
2. `cp src/modules/template.c src/modules/my_module.c`
3. Implement the 4-5 functions (init, process, draw, on_button, deinit)
4. Add one line to Makefile (`C_SOURCES += src/modules/my_module.c`)
5. Add one line to module registry (`modules/registry.h`)
6. `make renode` — test in emulator with simulated signals
7. Open browser to `localhost:5173` — see module rendering on simulated LCD
8. Submit PR

#### What the template provides

```c
// src/modules/template.c — copy and modify

#include "module_api.h"
#include "ui.h"
#include "lcd.h"

static void template_init(void) {
    // Allocate buffers, set defaults
}

static void template_process(const int16_t *ch1, const int16_t *ch2,
                              uint16_t num_samples, float sample_rate_hz) {
    // Analyze sample data
    // Store results for draw() to render
}

static void template_draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Render results to the LCD region you've been given
    // Use lcd_draw_string(), lcd_draw_line(), etc.
}

static bool template_on_button(button_id_t btn) {
    // Handle button presses relevant to your module
    // Return true if you consumed the event
    return false;
}

static void template_deinit(void) {
    // Clean up
}

OPENSCOPE_MODULE(template_module) = {
    .name        = "Template Module",
    .short_name  = "TMPL",
    .description = "Starting point for new modules",
    .version     = "0.1.0",
    .author      = "Your Name",
    .init        = template_init,
    .process     = template_process,
    .draw        = template_draw,
    .on_button   = template_on_button,
    .deinit      = template_deinit,
    .flash_size_est = 8192,
};
```

#### Test signal injection

Modules can include test data for emulator testing:

```c
// In the module or as a separate test file
#ifdef EMULATOR_BUILD
static const int16_t test_i2c_waveform[] = { ... };
// Injected into the signal path when running in Renode
#endif
```

This pattern already exists with `fft_test_signals.c`.

---

### Tier 4: Board Porter ("I want to run this on my OWON HDS272S")

**Skill level:** Embedded developer with RE skills. Has the hardware.

#### Board support package structure

```
firmware/boards/
  2c53t/              # Default board (FNIRSI 2C53T)
    board.h           # Pin definitions, memory map, screen dimensions
    lcd_hw.c          # ST7789V via EXMC init + pixel write
    fpga_hw.c         # USART2 string protocol to Gowin GW1N-UV2
    buttons_hw.c      # 15-button GPIO mapping
    adc_hw.c          # FPGA → sample buffer transfer
    dac_hw.c          # GD32F307 DAC0/DAC1 for siggen
    clock_hw.c        # 120MHz PLL, peripheral clocks
    gd32f307.ld       # Linker script
  owon_hds272s/       # Example second board
    board.h
    lcd_hw.c
    fpga_hw.c         # Anlogic FPGA protocol (different from 2C53T)
    buttons_hw.c
    ...
```

#### HAL interface (what each board must implement)

```c
// include/hal/display.h
void hal_lcd_init(void);
void hal_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void hal_lcd_write_pixels(const uint16_t *rgb565, uint32_t count);
void hal_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
uint16_t hal_lcd_width(void);
uint16_t hal_lcd_height(void);

// include/hal/acquisition.h
void hal_acq_init(void);
void hal_acq_set_timebase(uint32_t sample_rate_hz);
void hal_acq_set_trigger(uint8_t channel, int16_t level, bool rising);
bool hal_acq_triggered(void);
uint16_t hal_acq_read_samples(int16_t *buf, uint16_t max_samples, uint8_t channel);

// include/hal/siggen_hw.h
void hal_dac_init(void);
void hal_dac_write(uint8_t channel, uint16_t value);
void hal_dac_start_dma(const uint16_t *buf, uint16_t len, uint32_t rate_hz);

// include/hal/buttons.h
void hal_buttons_init(void);
button_id_t hal_buttons_scan(void);  // Returns which button is pressed

// include/hal/system.h
void hal_system_init(void);          // Clock, power, peripheral enables
void hal_delay_ms(uint32_t ms);
uint32_t hal_millis(void);
```

#### New board scaffold tool

```
$ python tools/new_board.py owon_hds272s --mcu gd32f303

Created firmware/boards/owon_hds272s/ with:
  board.h          — pin definitions (fill in)
  lcd_hw.c         — LCD driver stubs
  fpga_hw.c        — FPGA communication stubs
  buttons_hw.c     — button mapping stubs
  adc_hw.c         — acquisition stubs
  dac_hw.c         — DAC output stubs
  clock_hw.c       — system init stubs
  gd32f303.ld      — linker script template
  README.md        — porting checklist

Build with: make BOARD=owon_hds272s
```

#### Porting checklist (generated in README.md)

```markdown
# Porting Checklist: OWON HDS272S

## Phase 1: Boot (get LED blinking or UART output)
- [ ] Identify MCU exactly (package, flash/RAM size)
- [ ] Write linker script with correct memory regions
- [ ] Implement clock_hw.c (system clock, peripheral enables)
- [ ] Verify FreeRTOS boots (UART printf or GPIO toggle)

## Phase 2: Display (get pixels on screen)
- [ ] Identify LCD controller and bus interface
- [ ] Implement lcd_hw.c (init sequence, pixel write)
- [ ] Verify: splash screen renders

## Phase 3: Input (respond to buttons)
- [ ] Map button GPIOs
- [ ] Implement buttons_hw.c
- [ ] Verify: mode switching works

## Phase 4: Acquisition (get real waveforms)
- [ ] Reverse-engineer FPGA command protocol
- [ ] Implement fpga_hw.c + adc_hw.c
- [ ] Verify: live waveform on screen

## Phase 5: Output (signal generator)
- [ ] Identify DAC hardware
- [ ] Implement dac_hw.c
- [ ] Verify: signal generator produces waveforms
```

---

## Module Contribution Model

### Phase 1: Pull Requests (now → community forming)

Standard open-source PR workflow:

1. Contributor writes a module following the module spec
2. Submits PR to `src/modules/` or `src/decode/`
3. Maintainer reviews against spec: clean API usage, no hardware-specific code in module logic, includes test signals for emulator
4. Merged → automatically available in configurator

**This is the right starting model.** Simple, maintainer controls quality, no infrastructure overhead.

### Phase 2: Community Modules Directory (when community grows)

If the project grows beyond what one maintainer can review:

```yaml
# modules/community_registry.yaml
modules:
  - name: onewire_decode
    repo: https://github.com/someuser/openscope-onewire
    version: "1.2.0"
    description: "1-Wire protocol decoder (DS18B20, iButton, etc.)"
    status: community  # not officially reviewed

  - name: can_fd_decode
    repo: https://github.com/anotheruser/openscope-canfd
    version: "0.3.0"
    description: "CAN FD protocol decoder"
    status: community
```

The configurator shows these with a "community" badge (vs. "official" for in-repo modules). Build system can pull them in at build time via git.

**Reference:** Arduino Library Manager, PlatformIO Registry, Home Assistant HACS.

### Phase 3: Out-of-Tree / Third-Party Modules

For modules that don't belong in the main repo — proprietary, highly niche, or experimental. Someone maintains their own module in their own repo and combines it with core OpenScope at build time.

**Use case example:** An industrial automation company builds a pass/fail test module for their motor controllers. It's proprietary, specific to their hardware, and they want to maintain it themselves. But they want it running on the OpenScope platform alongside FFT, CAN decode, etc.

#### Their repo structure

```
acme-motor-test/
  module.yaml          # Module metadata and resource declarations
  motor_test.c         # Implements openscope module_api.h interface
  motor_test.h
  test_signals.c       # Optional: emulator test data
  README.md            # What it does, how to use it
```

#### Three ways to include it

**Source inclusion (open-source modules):**
```bash
make FEATURES="FFT CAN" EXTRA_MODULES=~/acme-motor-test
```
Makefile adds their `.c` files to the build. Everything compiles together into one `.bin`.

**Pre-compiled library (proprietary modules):**
```bash
# Module author compiles against OpenScope headers
arm-none-eabi-gcc -c motor_test.c \
  -I /path/to/openscope/firmware/include \
  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
arm-none-eabi-ar rcs libmotor_test.a motor_test.o

# End user links it into their build
make FEATURES="FFT CAN" EXTRA_LIBS=~/acme-motor-test/libmotor_test.a
```
Source code stays private. Only the compiled `.a` and `module.yaml` are distributed.

**Web configurator (no local tools):**
```
┌──────────────────────────────────────────┐
│ Official Modules:                         │
│   [X] FFT spectrum analyzer               │
│   [ ] CAN bus decode                      │
│                                           │
│ Add Custom Module:                        │
│   [Paste GitHub repo URL]                 │
│   or [Upload .a library + module.yaml]    │
│                                           │
│ Added:                                    │
│   ✓ Acme Motor Test (v1.0.0)             │
│     RAM: 4096 B  Flash: ~12 KB            │
│                                           │
│ Total flash: 624 KB / 733 KB  [Build]     │
└──────────────────────────────────────────┘
```
GitHub Actions clones their repo, validates `module.yaml`, builds everything, returns a `.bin`.

#### Module spec enforcement

The `module.yaml` declares resource needs so the build system can validate:

```
⚠ Module "Acme Motor Test" requests 4096 bytes RAM
  Available after core firmware: 18,432 bytes
  Remaining after this module: 14,336 bytes
  ✓ Fits
```

The `module_api.h` header defines what modules can and cannot do:
- **CAN do:** Read sample buffers, draw to allocated screen region, allocate from module heap, read buttons, call math functions
- **CANNOT do:** Access hardware registers directly, write outside allocated screen region, allocate unbounded memory, block for extended periods

This isn't sandboxed (it's C on bare metal), but the spec + test suite + review process catches violations. See [Module API Reference](module_api.md) and [module.yaml schema](../firmware/module.yaml.example) for full details.

#### Stable API commitment

Once `module_api.h` reaches v1.0, it becomes a stable interface. Modules compiled against v1.0 headers will link against v1.0+ firmware. Breaking changes require a major version bump with migration guide. This is the same promise that makes Linux kernel modules and Arduino libraries viable ecosystems.

### Side-Loading (runtime module loading) — future possibility

Technically possible on Cortex-M4 but complex:

- Reserve flash region for module slots (e.g., 0x080B8000–0x080FF000)
- Modules compiled as position-independent code or at fixed slot addresses
- Core firmware scans slots at boot, finds valid module headers, registers them
- Users flash modules independently via USB or SPI flash

The Miniware DS213 does this with 4 firmware slots. But it adds significant complexity for marginal benefit when the web configurator produces a custom binary in 60 seconds.

**Recommendation:** Don't build this unless there's clear demand. Compile-time inclusion via configurator covers 99% of use cases.

---

## Implementation Roadmap

### Now (pre-hardware)
- [x] Makefile `FEATURES` flag for FFT (already exists)
- [ ] Extend `FEATURES` to cover all modules and decoders
- [ ] Define `module_api.h` interface
- [ ] Create `src/modules/template.c`

### v0.1 Release (first working firmware on real hardware)
- [ ] GitHub Actions CI: build `.bin` on tagged release
- [ ] Release page with pre-built profiles (standard, automotive, full, minimal)
- [ ] Flash instructions in release notes
- [ ] Known issues list

### v0.2 Release
- [ ] Python CLI configurator (`tools/configure.py`)
- [ ] Contributing guide for module developers
- [ ] Module template with emulator test signal example

### v0.3+ Release
- [ ] Web-based configurator on GitHub Pages
- [ ] On-demand builds via GitHub Actions API
- [ ] Board abstraction (`make BOARD=...`)
- [ ] `tools/new_board.py` scaffold generator

### Future (if community grows)
- [ ] Community module registry
- [ ] Automated module testing in CI
- [ ] Per-module documentation site
- [ ] Runtime module loading (if demand exists)

---

## Reference Projects

Projects that have solved similar distribution/customization problems:

| Project | Approach | Relevant because |
|---------|----------|-----------------|
| [QMK Firmware](https://github.com/qmk/qmk_firmware) | Web configurator + GitHub Actions builds | Gold standard for "pick features, get binary" UX |
| [Marlin Firmware](https://github.com/MarlinFirmware/Marlin) | `Configuration.h` with `#define` flags | Large community, many board targets, compile-time config |
| [ESPHome](https://esphome.io/) | YAML config → compiled firmware | Declarative module selection, OTA updates |
| [Tasmota](https://github.com/arendst/Tasmota) | Web-based build configurator | Pre-built "environments" + custom builds |
| [OpenWrt](https://openwrt.org/) | `menuconfig` + package feeds | Multi-device, community packages, board support system |
| [Zephyr RTOS](https://zephyrproject.org/) | Kconfig + devicetree + west | Most sophisticated board abstraction, but heavy |
| [PlatformIO](https://platformio.org/) | Library registry + build system | Community module distribution |
| [Arduino Library Manager](https://www.arduino.cc/reference/en/libraries/) | Central registry, simple API contract | Lowest barrier to entry for contributors |
