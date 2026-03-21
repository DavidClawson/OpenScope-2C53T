/*
 * OpenScope 2C53T - Configuration Save/Load
 *
 * See config.h for interface documentation.
 */

#include "config.h"
#include <stddef.h>
#include <string.h>

/* ========================================================================
 * Stub storage backend
 *
 * TODO: Use flash_fs_write_atomic() when SPI flash driver is available.
 * For now, save/load uses a static memory buffer so the rest of the
 * config system can be tested end-to-end.
 * ======================================================================== */

static uint8_t config_storage[sizeof(device_config_t)];
static bool    config_stored = false;

/* ========================================================================
 * Default configuration values
 * ======================================================================== */

void config_init_defaults(device_config_t *cfg)
{
    if (!cfg) return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->magic   = CONFIG_MAGIC;
    cfg->version = CONFIG_VERSION;

    /* Oscilloscope defaults */
    cfg->scope_ch1_vdiv     = 3;    /* 2V/div */
    cfg->scope_ch2_vdiv     = 5;    /* 200mV/div */
    cfg->scope_timebase     = 10;   /* 50us/div */
    cfg->scope_trigger_mode = 0;    /* Auto */
    cfg->scope_trigger_edge = 0;    /* Rising */
    cfg->scope_trigger_level = 0;
    cfg->scope_ch1_coupling = 0;    /* DC */
    cfg->scope_ch2_coupling = 0;    /* DC */
    cfg->scope_ch1_enabled  = true;
    cfg->scope_ch2_enabled  = true;

    /* FFT defaults */
    cfg->fft_window    = 1;         /* Hanning */
    cfg->fft_avg_count = 0;
    cfg->fft_max_hold  = false;

    /* Signal generator defaults */
    cfg->siggen_waveform  = 0;      /* Sine */
    cfg->siggen_frequency = 1000.0f;
    cfg->siggen_amplitude = 3.3f;
    cfg->siggen_offset    = 0.0f;

    /* Display defaults */
    cfg->display_brightness  = 80;
    cfg->display_persist_mode = 0;
    cfg->language = 0;              /* English */

    /* System defaults */
    cfg->auto_shutdown_mins = 30;
    cfg->sound_enabled      = 1;

    /* Compute and store checksum */
    cfg->checksum = config_compute_checksum(cfg);
}

/* ========================================================================
 * Checksum: simple byte-sum of all fields before the checksum field
 * ======================================================================== */

uint32_t config_compute_checksum(const device_config_t *cfg)
{
    if (!cfg) return 0;

    const uint8_t *bytes = (const uint8_t *)cfg;
    /* Sum all bytes up to (but not including) the checksum field */
    uint32_t len = offsetof(device_config_t, checksum);
    uint32_t sum = 0;

    for (uint32_t i = 0; i < len; i++) {
        sum += bytes[i];
    }

    return sum;
}

/* ========================================================================
 * Validation
 * ======================================================================== */

bool config_validate(const device_config_t *cfg)
{
    if (!cfg) return false;
    if (cfg->magic != CONFIG_MAGIC) return false;
    if (cfg->version != CONFIG_VERSION) return false;
    if (cfg->checksum != config_compute_checksum(cfg)) return false;

    return true;
}

/* ========================================================================
 * Serialization (struct <-> byte buffer)
 *
 * For v1, the struct is memcpy'd directly. This works because:
 * - Target is always ARM Cortex-M4 (little-endian, consistent alignment)
 * - The same compiler and platform are used for save and load
 * ======================================================================== */

uint32_t config_serialize(const device_config_t *cfg, uint8_t *buf, uint32_t buf_size)
{
    if (!cfg || !buf) return 0;
    if (buf_size < sizeof(device_config_t)) return 0;

    memcpy(buf, cfg, sizeof(device_config_t));
    return (uint32_t)sizeof(device_config_t);
}

bool config_deserialize(device_config_t *cfg, const uint8_t *buf, uint32_t buf_size)
{
    if (!cfg || !buf) return false;
    if (buf_size < sizeof(device_config_t)) return false;

    memcpy(cfg, buf, sizeof(device_config_t));

    /* Validate the deserialized config */
    return config_validate(cfg);
}

/* ========================================================================
 * Save / Load (stub backend — static buffer)
 *
 * TODO: Replace with SPI flash filesystem calls:
 *   config_save  -> flash_fs_write_atomic("3:/System file/config.bin", ...)
 *   config_load  -> flash_fs_read("3:/System file/config.bin", ...)
 * ======================================================================== */

bool config_save(const device_config_t *cfg)
{
    if (!cfg) return false;

    uint32_t written = config_serialize(cfg, config_storage, sizeof(config_storage));
    if (written == 0) return false;

    config_stored = true;
    return true;
}

bool config_load(device_config_t *cfg)
{
    if (!cfg) return false;
    if (!config_stored) return false;

    return config_deserialize(cfg, config_storage, sizeof(config_storage));
}
