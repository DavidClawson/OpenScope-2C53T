/*
 * OpenScope 2C53T - Configuration Save/Load
 *
 * Stores and retrieves user-configurable device settings.
 * Settings are validated with magic number, version, and checksum.
 *
 * Storage backend: currently uses a static memory buffer (stub).
 * TODO: Use flash_fs_write_atomic() when SPI flash driver is available.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAGIC    0x4F534332  /* "OSC2" */
#define CONFIG_VERSION  1

/* All user-configurable settings */
typedef struct {
    uint32_t magic;              /* CONFIG_MAGIC for validation */
    uint16_t version;            /* CONFIG_VERSION */
    /* Oscilloscope */
    uint8_t  scope_ch1_vdiv;     /* Volts/div index (0-9) */
    uint8_t  scope_ch2_vdiv;
    uint8_t  scope_timebase;     /* Timebase index (0-20) */
    uint8_t  scope_trigger_mode; /* 0=auto, 1=normal, 2=single */
    uint8_t  scope_trigger_edge; /* 0=rising, 1=falling */
    int16_t  scope_trigger_level;
    uint8_t  scope_ch1_coupling; /* 0=DC, 1=AC, 2=GND */
    uint8_t  scope_ch2_coupling;
    bool     scope_ch1_enabled;
    bool     scope_ch2_enabled;
    /* FFT */
    uint8_t  fft_window;         /* fft_window_t value */
    uint8_t  fft_avg_count;
    bool     fft_max_hold;
    /* Signal Generator */
    uint8_t  siggen_waveform;
    float    siggen_frequency;
    float    siggen_amplitude;
    float    siggen_offset;
    /* Display */
    uint8_t  display_brightness;
    uint8_t  display_persist_mode;
    uint8_t  language;           /* 0=English */
    /* System */
    uint8_t  auto_shutdown_mins; /* 0=disabled, 5/10/15/30/60 */
    uint8_t  sound_enabled;
    /* Checksum */
    uint32_t checksum;           /* Sum of all bytes before this field */
} device_config_t;

/* Initialize config with defaults */
void config_init_defaults(device_config_t *cfg);

/* Compute checksum for config validation */
uint32_t config_compute_checksum(const device_config_t *cfg);

/* Validate a config (magic, version, checksum) */
bool config_validate(const device_config_t *cfg);

/* Serialize config to a byte buffer (for saving to flash).
 * Returns number of bytes written, or 0 on failure. */
uint32_t config_serialize(const device_config_t *cfg, uint8_t *buf, uint32_t buf_size);

/* Deserialize config from a byte buffer (loaded from flash).
 * Returns false if magic, version, or checksum is invalid. */
bool config_deserialize(device_config_t *cfg, const uint8_t *buf, uint32_t buf_size);

/* Save config to flash (uses flash_fs if available, otherwise stub).
 * Returns true on success. */
bool config_save(const device_config_t *cfg);

/* Load config from flash (returns false if no saved config). */
bool config_load(device_config_t *cfg);

#endif /* CONFIG_H */
