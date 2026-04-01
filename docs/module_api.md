# OpenScope Module API Reference

*Draft v0.1 — March 2026*

This document defines the interface that all OpenScope modules must implement. Modules are self-contained features (protocol decoders, analysis tools, test procedures, etc.) that plug into the OpenScope platform.

## Overview

A module is a C source file (or small set of files) that:
1. Implements a standard set of lifecycle functions
2. Declares itself via the `OPENSCOPE_MODULE()` macro
3. Describes its metadata and resource needs in a `module.yaml` file
4. Makes no direct hardware calls — all hardware access goes through the platform API

Modules can live in the main repo (`src/modules/`, `src/decode/`) or in external repos (out-of-tree).

---

## Module Header: `module_api.h`

```c
#ifndef MODULE_API_H
#define MODULE_API_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Version — bump major on breaking changes, minor on additions
 * ═══════════════════════════════════════════════════════════════════ */
#define OPENSCOPE_MODULE_API_VERSION_MAJOR  0
#define OPENSCOPE_MODULE_API_VERSION_MINOR  1

/* ═══════════════════════════════════════════════════════════════════
 * Button IDs (matches ui.h — reproduced here so modules don't
 * need to include platform internals)
 * ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    MOD_BTN_NONE = 0,
    MOD_BTN_CH1, MOD_BTN_CH2,
    MOD_BTN_MOVE, MOD_BTN_SELECT,
    MOD_BTN_TRIGGER, MOD_BTN_PRM,
    MOD_BTN_AUTO, MOD_BTN_SAVE, MOD_BTN_MENU,
    MOD_BTN_UP, MOD_BTN_DOWN,
    MOD_BTN_LEFT, MOD_BTN_RIGHT,
    MOD_BTN_OK, MOD_BTN_POWER,
} mod_button_t;

/* ═══════════════════════════════════════════════════════════════════
 * Sample data passed to modules each acquisition cycle
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    const int16_t *ch1;          /* Channel 1 samples (NULL if unavailable) */
    const int16_t *ch2;          /* Channel 2 samples (NULL if unavailable) */
    uint16_t       num_samples;  /* Number of samples per channel */
    float          sample_rate_hz;
    float          voltage_scale; /* Volts per ADC count */
    float          time_offset;   /* Trigger offset in seconds */
} mod_sample_data_t;

/* ═══════════════════════════════════════════════════════════════════
 * Drawing region allocated to the module
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    uint16_t x, y;               /* Top-left corner */
    uint16_t w, h;               /* Width and height in pixels */
} mod_draw_region_t;

/* ═══════════════════════════════════════════════════════════════════
 * Module settings (optional — for modules with configurable params)
 * ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    MOD_SETTING_BOOL,
    MOD_SETTING_INT,
    MOD_SETTING_FLOAT,
    MOD_SETTING_ENUM,
} mod_setting_type_t;

typedef struct {
    const char          *name;       /* "Baud Rate" */
    const char          *key;        /* "baud_rate" (for save/load) */
    mod_setting_type_t   type;
    union {
        struct { bool *value; } bool_val;
        struct { int32_t *value; int32_t min; int32_t max; int32_t step; } int_val;
        struct { float *value; float min; float max; float step; } float_val;
        struct { uint8_t *value; const char **labels; uint8_t count; } enum_val;
    };
} mod_setting_t;

/* ═══════════════════════════════════════════════════════════════════
 * Module status (returned by process, displayed in status bar)
 * ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    MOD_STATUS_IDLE,             /* Module loaded but not actively processing */
    MOD_STATUS_RUNNING,          /* Normal operation */
    MOD_STATUS_TRIGGERED,        /* Found something interesting (e.g., pattern match) */
    MOD_STATUS_PASS,             /* Test passed (for test/validation modules) */
    MOD_STATUS_FAIL,             /* Test failed */
    MOD_STATUS_ERROR,            /* Module encountered an error */
} mod_status_t;

/* ═══════════════════════════════════════════════════════════════════
 * Feature requirement flags (declare what platform features you need)
 * ═══════════════════════════════════════════════════════════════════ */
#define MOD_REQUIRES_SCOPE     (1 << 0)  /* Needs ADC sample data */
#define MOD_REQUIRES_SIGGEN    (1 << 1)  /* Needs signal generator output */
#define MOD_REQUIRES_DMM       (1 << 2)  /* Needs multimeter readings */
#define MOD_REQUIRES_DUAL_CH   (1 << 3)  /* Needs both channels */
#define MOD_REQUIRES_FFT       (1 << 4)  /* Needs FFT results */

/* ═══════════════════════════════════════════════════════════════════
 * Module definition structure
 * ═══════════════════════════════════════════════════════════════════ */
typedef struct {
    /* ── Identity ─────────────────────────────────────────────── */
    const char *name;            /* "I2C Protocol Decoder" */
    const char *short_name;      /* "I2C" (status bar, max 6 chars) */
    const char *description;     /* One-line description */
    const char *version;         /* Semantic version: "1.0.0" */
    const char *author;          /* "Your Name" or "Organization" */

    uint16_t api_version_major;  /* OPENSCOPE_MODULE_API_VERSION_MAJOR */
    uint16_t api_version_minor;  /* OPENSCOPE_MODULE_API_VERSION_MINOR */

    /* ── Lifecycle ────────────────────────────────────────────── */

    /* Called once when module is activated. Allocate buffers, set defaults.
     * Return true on success, false if initialization failed. */
    bool (*init)(void);

    /* Called once when module is deactivated. Free resources. */
    void (*deinit)(void);

    /* ── Processing ───────────────────────────────────────────── */

    /* Called each acquisition cycle with new sample data.
     * Do your analysis here. Store results for draw() to render.
     * Must complete within 10ms to avoid blocking the acquisition pipeline.
     * Returns current module status. */
    mod_status_t (*process)(const mod_sample_data_t *data);

    /* ── Rendering ────────────────────────────────────────────── */

    /* Called by the display task to render module output.
     * Draw ONLY within the provided region. Called at display refresh rate.
     * Use the platform drawing API (mod_lcd_*), not hardware registers. */
    void (*draw)(const mod_draw_region_t *region);

    /* Optional: draw a one-line summary for the status bar (max ~20 chars).
     * Example: "I2C: 0x48 W [ACK]" or "PASS: 142 PSI".
     * NULL if not needed. */
    void (*draw_status)(uint16_t x, uint16_t y);

    /* ── Input ────────────────────────────────────────────────── */

    /* Called on button press. Return true if the module consumed the event
     * (prevents it from propagating to the default handler).
     * NULL if the module doesn't handle buttons. */
    bool (*on_button)(mod_button_t btn);

    /* ── Settings ─────────────────────────────────────────────── */

    /* Array of configurable settings. NULL if none.
     * The platform renders these in a standard settings UI. */
    const mod_setting_t *settings;
    uint8_t              num_settings;

    /* ── Resource declarations ────────────────────────────────── */

    uint32_t required_features;  /* MOD_REQUIRES_* bitmask */
    uint32_t ram_bytes;          /* Estimated RAM usage (for configurator) */
    uint32_t flash_bytes;        /* Estimated flash usage (for configurator) */

} openscope_module_t;

/* ═══════════════════════════════════════════════════════════════════
 * Registration macro — places module struct in a dedicated linker
 * section so the platform can discover all compiled-in modules
 * ═══════════════════════════════════════════════════════════════════ */
#define OPENSCOPE_MODULE(varname) \
    __attribute__((section(".openscope_modules"), used)) \
    const openscope_module_t varname

#endif /* MODULE_API_H */
```

---

## Platform Drawing API (available to modules)

Modules render via these functions, not direct hardware access. This ensures portability across boards and compatibility with the emulator.

```c
/* ── Primitives ───────────────────────────────────────────────── */
void mod_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void mod_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void mod_lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void mod_lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* ── Text ─────────────────────────────────────────────────────── */
void mod_lcd_draw_string(uint16_t x, uint16_t y, const char *str,
                         uint16_t fg, uint16_t bg);
void mod_lcd_draw_string_small(uint16_t x, uint16_t y, const char *str,
                               uint16_t fg, uint16_t bg);
int  mod_lcd_printf(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                    const char *fmt, ...);

/* ── Colors (RGB565) ──────────────────────────────────────────── */
#define MOD_COLOR_BLACK       0x0000
#define MOD_COLOR_WHITE       0xFFFF
#define MOD_COLOR_RED         0xF800
#define MOD_COLOR_GREEN       0x07E0
#define MOD_COLOR_BLUE        0x001F
#define MOD_COLOR_YELLOW      0xFFE0
#define MOD_COLOR_CYAN        0x07FF
#define MOD_COLOR_ORANGE      0xFD20
#define MOD_COLOR_GRAY        0x8410
#define MOD_COLOR_DARK_GRAY   0x4208
#define MOD_COLOR_PASS_GREEN  0x2F85  /* Green for pass/good indicators */
#define MOD_COLOR_FAIL_RED    0xD8A2  /* Red for fail/bad indicators */

/* ── Utility ──────────────────────────────────────────────────── */
uint16_t mod_lcd_rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t mod_lcd_screen_width(void);
uint16_t mod_lcd_screen_height(void);
```

---

## Module Rules

### MUST

- Implement `init`, `deinit`, `process`, and `draw` (all four are required)
- Set `api_version_major` and `api_version_minor` from the header constants
- Complete `process()` within 10ms (acquisition pipeline budget)
- Draw only within the `mod_draw_region_t` passed to `draw()`
- Declare resource needs honestly in `module.yaml` and struct fields
- Include at least one test signal file for emulator testing
- Use only the `mod_lcd_*` drawing API, not direct LCD hardware functions

### SHOULD

- Handle `init()` failure gracefully (return false, don't crash)
- Provide a `draw_status()` for the status bar summary
- Use the settings system for user-configurable parameters
- Follow the project code style (gnu11, 4-space indent)
- Provide a README explaining what the module does and how to use it

### MUST NOT

- `#include "gd32f30x.h"` or any board-specific header
- Access hardware registers or memory-mapped peripherals directly
- Allocate memory dynamically after `init()` (allocate everything upfront)
- Block in `process()` or `draw()` (no busy-wait loops, no vTaskDelay)
- Write to flash or persistent storage without going through the platform API
- Call FreeRTOS API directly (use platform wrappers if needed)

---

## Lifecycle

```
                    ┌─────────┐
                    │  LOAD   │  Module compiled into firmware
                    └────┬────┘
                         │
                    ┌────▼────┐
                    │  init() │  Allocate buffers, set defaults
                    └────┬────┘
                         │  (returns true)
              ┌──────────▼──────────┐
              │     ACTIVE LOOP     │
              │                     │
              │  ┌──────────────┐   │
              │  │  process()   │◄──│── Called each acquisition cycle
              │  └──────┬───────┘   │
              │         │           │
              │  ┌──────▼───────┐   │
              │  │   draw()     │◄──│── Called each display refresh
              │  └──────┬───────┘   │
              │         │           │
              │  ┌──────▼───────┐   │
              │  │ on_button()  │◄──│── Called on user input (optional)
              │  └──────────────┘   │
              └──────────┬──────────┘
                         │  (user deactivates module)
                    ┌────▼────┐
                    │ deinit()│  Free resources
                    └─────────┘
```

---

## Example: Minimal Module

```c
/* src/modules/example_voltage_logger.c
 *
 * Simple module that tracks min/max voltage over time
 * and displays a pass/fail against user-set thresholds.
 */

#include "module_api.h"
#include <string.h>
#include <stdio.h>

/* ── State ────────────────────────────────────────────────────── */
static float min_v, max_v, current_v;
static uint32_t sample_count;
static float threshold_low  = 0.5f;
static float threshold_high = 3.0f;

/* ── Settings ─────────────────────────────────────────────────── */
static const mod_setting_t settings[] = {
    {
        .name = "Low Threshold (V)",
        .key  = "thresh_low",
        .type = MOD_SETTING_FLOAT,
        .float_val = { .value = &threshold_low, .min = 0.0f, .max = 5.0f, .step = 0.1f }
    },
    {
        .name = "High Threshold (V)",
        .key  = "thresh_high",
        .type = MOD_SETTING_FLOAT,
        .float_val = { .value = &threshold_high, .min = 0.0f, .max = 5.0f, .step = 0.1f }
    },
};

/* ── Lifecycle ────────────────────────────────────────────────── */

static bool vlog_init(void) {
    min_v = 9999.0f;
    max_v = -9999.0f;
    current_v = 0.0f;
    sample_count = 0;
    return true;
}

static void vlog_deinit(void) {
    /* Nothing to free — all static */
}

/* ── Processing ───────────────────────────────────────────────── */

static mod_status_t vlog_process(const mod_sample_data_t *data) {
    if (!data->ch1 || data->num_samples == 0)
        return MOD_STATUS_IDLE;

    /* Find min/max/average in this buffer */
    float sum = 0;
    for (uint16_t i = 0; i < data->num_samples; i++) {
        float v = data->ch1[i] * data->voltage_scale;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum += v;
    }
    current_v = sum / data->num_samples;
    sample_count += data->num_samples;

    /* Pass/fail check */
    if (min_v < threshold_low || max_v > threshold_high)
        return MOD_STATUS_FAIL;
    return MOD_STATUS_PASS;
}

/* ── Rendering ────────────────────────────────────────────────── */

static void vlog_draw(const mod_draw_region_t *r) {
    char buf[32];

    mod_lcd_fill_rect(r->x, r->y, r->w, r->h, MOD_COLOR_BLACK);

    mod_lcd_draw_string(r->x + 4, r->y + 4, "Voltage Logger",
                        MOD_COLOR_WHITE, MOD_COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Now:  %.3f V", current_v);
    mod_lcd_draw_string(r->x + 4, r->y + 20, buf,
                        MOD_COLOR_CYAN, MOD_COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Min:  %.3f V", min_v);
    uint16_t min_color = (min_v < threshold_low) ? MOD_COLOR_FAIL_RED : MOD_COLOR_PASS_GREEN;
    mod_lcd_draw_string(r->x + 4, r->y + 36, buf,
                        min_color, MOD_COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Max:  %.3f V", max_v);
    uint16_t max_color = (max_v > threshold_high) ? MOD_COLOR_FAIL_RED : MOD_COLOR_PASS_GREEN;
    mod_lcd_draw_string(r->x + 4, r->y + 52, buf,
                        max_color, MOD_COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Samples: %lu", (unsigned long)sample_count);
    mod_lcd_draw_string(r->x + 4, r->y + 72, buf,
                        MOD_COLOR_GRAY, MOD_COLOR_BLACK);
}

static void vlog_draw_status(uint16_t x, uint16_t y) {
    char buf[24];
    snprintf(buf, sizeof(buf), "VLOG: %.2fV", current_v);
    mod_lcd_draw_string_small(x, y, buf,
                              MOD_COLOR_YELLOW, MOD_COLOR_BLACK);
}

static bool vlog_on_button(mod_button_t btn) {
    if (btn == MOD_BTN_OK) {
        /* Reset min/max on OK press */
        min_v = 9999.0f;
        max_v = -9999.0f;
        sample_count = 0;
        return true;  /* consumed */
    }
    return false;
}

/* ── Registration ─────────────────────────────────────────────── */

OPENSCOPE_MODULE(voltage_logger_module) = {
    .name             = "Voltage Logger",
    .short_name       = "VLOG",
    .description      = "Track min/max/avg voltage with pass/fail thresholds",
    .version          = "1.0.0",
    .author           = "OpenScope",
    .api_version_major = OPENSCOPE_MODULE_API_VERSION_MAJOR,
    .api_version_minor = OPENSCOPE_MODULE_API_VERSION_MINOR,
    .init             = vlog_init,
    .deinit           = vlog_deinit,
    .process          = vlog_process,
    .draw             = vlog_draw,
    .draw_status      = vlog_draw_status,
    .on_button        = vlog_on_button,
    .settings         = settings,
    .num_settings     = sizeof(settings) / sizeof(settings[0]),
    .required_features = MOD_REQUIRES_SCOPE,
    .ram_bytes         = 64,
    .flash_bytes       = 4096,
};
```

---

## Test Signals for Emulator

Modules should include test data so they can be exercised without hardware:

```c
/* example_voltage_logger_test.c — only compiled in EMULATOR_BUILD */
#ifdef EMULATOR_BUILD

#include <stdint.h>

/* Simulated voltage ramp: 0V → 3.3V → 0V over 1000 samples */
const int16_t test_voltage_ramp[1000] = {
    /* Generated programmatically — values represent ADC counts */
    0, 6, 13, 20, 26, 33, /* ... */
};

const uint16_t test_voltage_ramp_len = 1000;
const float    test_voltage_ramp_rate = 100000.0f;  /* 100 kHz */

#endif
```

The emulator test framework injects these into the signal path when running in Renode, so the module processes realistic data and renders to the simulated LCD.

---

## See Also

- [module.yaml schema](../firmware/module.yaml.example) — metadata file format
- [Developer Experience Plan](developer_experience.md) — how modules fit into the project
- [Template module](../firmware/src/modules/template.c) — copy this to start a new module
