/*
 * OpenScope 2C53T - Configuration Save/Load
 *
 * See config.h for the interface and for why the storage backend is the
 * region layer's append log rather than a raw sector rewrite.
 *
 * HISTORY, worth knowing before changing anything here: until 2026-08-13 this
 * file wrote to a static RAM buffer and had ZERO call sites, while the repo
 * advertised "config save/load with checksum". Nothing had ever survived a
 * reboot. The fix is not just "point it at flash" — it is that every failure
 * mode now has a defined, tested behaviour, and that the write goes through
 * flash_regions.h so a bug here cannot erase the factory calibration.
 */

#include "config.h"
#include "flash_regions.h"

#include <stddef.h>
#include <string.h>

/* A config record must fit the append log's per-record limit. If someone grows
 * the struct past 1 KB this fails at compile time rather than at the first
 * save on a user's device. */
_Static_assert(sizeof(device_config_t) <= FLASH_REGION_RECORD_MAX,
               "device_config_t no longer fits an append record");

/* The settings live in their own region. This is the ONLY place the storage
 * location is named, and it is named by id — not by address — so the region
 * table decides what is writable. Do not add a flash_regions_*_abs() call to
 * this file; the whole safety argument rests on that. */
#define CONFIG_REGION  FLASH_REGION_SETTINGS

static config_persist_stats_t g_stats;

/* ========================================================================
 * Default configuration values
 * ======================================================================== */

void config_init_defaults(device_config_t *cfg)
{
    if (!cfg) return;

    /* Memset first: this is what makes the struct's padding deterministic,
     * which is what makes the append log's no-op elision work. */
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
    cfg->startup_mode = CONFIG_MODE_MULTIMETER;

    /* System defaults */
    cfg->auto_shutdown_mins = 30;
    cfg->sound_enabled      = 1;

    /* v3 block */
    cfg->theme                = 0;  /* THEME_DARK_BLUE */
    cfg->scope_ch1_probe      = 0;  /* 1x */
    cfg->scope_ch2_probe      = 0;
    cfg->scope_ch1_bw_limit   = 0;
    cfg->scope_ch2_bw_limit   = 0;
    cfg->scope_trigger_source = 0;  /* CH1 */
    cfg->math_enabled         = 0;
    cfg->math_op              = 0;
    cfg->meter_submode        = 0;  /* DCV */
    cfg->meter_layout         = 0;  /* Full */

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
 *
 * A record written by a different build of the struct is caught by the
 * version field and by the length check in config_load().
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
 * Save / Load — the W25Q settings append log
 * ======================================================================== */

const char *config_load_result_name(config_load_result_t r)
{
    switch (r) {
    case CONFIG_LOAD_OK:         return "loaded";
    case CONFIG_LOAD_NO_STORAGE: return "no storage bound";
    case CONFIG_LOAD_EMPTY:      return "no saved record";
    case CONFIG_LOAD_BAD_SIZE:   return "record size mismatch";
    case CONFIG_LOAD_INVALID:    return "record failed validation";
    case CONFIG_LOAD_IO:         return "flash read error";
    }
    return "unknown";
}

const config_persist_stats_t *config_persist_stats(void)
{
    /* Stamped on every read so a caller can never be handed a stats block that
     * does not say which kind of build produced it. saves_ok == 0 means two
     * completely different things depending on this flag, and a diagnostic
     * that cannot tell them apart is the failure mode this project keeps
     * paying for. */
    g_stats.writes_enabled = (SETTINGS_PERSIST_WRITES != 0);
    return &g_stats;
}

void config_persist_stats_reset(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.writes_enabled = (SETTINGS_PERSIST_WRITES != 0);
}

bool config_save(const device_config_t *cfg)
{
    if (!cfg) {
        g_stats.saves_failed++;
        return false;
    }
#if !SETTINGS_PERSIST_WRITES
    /* Writes disabled at build time (the default — see config.h). Bail before
     * touching the region layer at all, so a disabled build issues no erase,
     * no program, and no CS assert on SPI2. Counted separately from failures:
     * this is a choice, not a fault. */
    g_stats.saves_disabled++;
    return false;
#else
    if (!flash_regions_ready()) {
        /* No backend bound (host build with no model, emulator, or a region
         * table that failed its self-check). Refuse loudly instead of
         * returning true and losing the data. */
        g_stats.saves_failed++;
        g_stats.last_save_status = (int32_t)FLASH_REGION_ERR_NOT_INIT;
        return false;
    }

    /* Stamp identity and checksum from THIS build rather than trusting the
     * caller's copy: a record can then never claim a version it is not. */
    device_config_t rec = *cfg;
    rec.magic    = CONFIG_MAGIC;
    rec.version  = CONFIG_VERSION;
    rec.checksum = config_compute_checksum(&rec);

    uint8_t buf[sizeof(device_config_t)];
    if (config_serialize(&rec, buf, sizeof(buf)) != sizeof(buf)) {
        g_stats.saves_failed++;
        return false;
    }

    flash_region_status_t st = flash_region_append(CONFIG_REGION, buf, sizeof(buf));

    if (st == FLASH_REGION_ERR_FULL) {
        /* The log filled. This is the ONE place this file erases anything, it
         * is an explicit whole-region reset of our own append region (the
         * layer bounds it), and it is the documented way out of a full log.
         * A power cut between the reset and the re-append costs the settings
         * and the device comes up on defaults — which is why compaction is
         * driven by "actually full" and not by a heuristic. */
        flash_region_status_t rst = flash_region_reset(CONFIG_REGION);
        if (rst != FLASH_REGION_OK) {
            g_stats.saves_failed++;
            g_stats.last_save_status = (int32_t)rst;
            return false;
        }
        g_stats.compactions++;
        st = flash_region_append(CONFIG_REGION, buf, sizeof(buf));
    }

    g_stats.last_save_status = (int32_t)st;
    if (st != FLASH_REGION_OK) {
        /* Deliberately NOT retried with an erase. FLASH_REGION_ERR_NEEDS_ERASE
         * here means the next slot in our region is not blank — i.e. reality
         * disagrees with the flash map. Erasing on that basis is exactly the
         * stray-erase behaviour the region layer exists to prevent; surface it
         * instead (config_persist_stats()->last_save_status). */
        g_stats.saves_failed++;
        return false;
    }

    g_stats.saves_ok++;
    return true;
#endif /* SETTINGS_PERSIST_WRITES */
}

/* Forward: the load worker, defined below with the rest of the load path. */
static config_load_result_t config_try_load(device_config_t *cfg);

bool config_load(device_config_t *cfg)
{
    if (!cfg) return false;

    if (config_try_load(cfg) != CONFIG_LOAD_OK) {
        /* *cfg is untouched — deliberately NOT filled with defaults here. A
         * caller that wants defaults asks for them by name via
         * config_load_or_defaults(); a caller that wants "did my saved config
         * come back" gets a clean no. */
        g_stats.loads_defaulted++;
        return false;
    }
    g_stats.loads_ok++;
    return true;
}

/* Reads the newest record. Writes *cfg ONLY when it returns CONFIG_LOAD_OK,
 * so every failure path leaves the caller's struct as it found it. */
static config_load_result_t config_try_load(device_config_t *cfg)
{
    uint8_t buf[sizeof(device_config_t)];
    uint32_t len = 0;

    if (!flash_regions_ready()) {
        return CONFIG_LOAD_NO_STORAGE;
    }

    flash_region_status_t st =
        flash_region_read_latest(CONFIG_REGION, buf, sizeof(buf), &len);
    g_stats.last_load_status = (int32_t)st;

    if (st == FLASH_REGION_ERR_NOT_FOUND) {
        return CONFIG_LOAD_EMPTY;            /* first boot: blank log */
    }
    if (st == FLASH_REGION_ERR_BOUNDS) {
        return CONFIG_LOAD_BAD_SIZE;         /* record bigger than a config */
    }
    if (st == FLASH_REGION_ERR_BAD_ARG || st == FLASH_REGION_ERR_READ_ONLY) {
        /* The region this build points at is not an append log (or is not
         * writable at all). That is a region-table problem, not a flash
         * problem — report it as "no storage" so it cannot be mistaken for a
         * transient read failure. */
        return CONFIG_LOAD_NO_STORAGE;
    }
    if (st != FLASH_REGION_OK) {
        return CONFIG_LOAD_IO;
    }
    if (len != sizeof(device_config_t)) {
        /* Right region, wrong shape — e.g. a record written by a build whose
         * struct was a different size. Never memcpy a short record into a
         * config: the tail would be whatever was on the stack. */
        return CONFIG_LOAD_BAD_SIZE;
    }

    /* Deserialize into scratch so a rejected record never lands in the
     * caller's struct: config_deserialize() overwrites before it validates,
     * and that behaviour must stay contained here. */
    device_config_t scratch;
    if (!config_deserialize(&scratch, buf, len)) {
        return CONFIG_LOAD_INVALID;          /* magic / version / checksum */
    }

    *cfg = scratch;
    return CONFIG_LOAD_OK;
}

config_load_result_t config_load_or_defaults(device_config_t *cfg)
{
    if (!cfg) return CONFIG_LOAD_INVALID;

    config_load_result_t result = config_try_load(cfg);
    if (result == CONFIG_LOAD_OK) {
        g_stats.loads_ok++;
        return result;
    }

    config_init_defaults(cfg);
    g_stats.loads_defaulted++;
    return result;
}

/* ========================================================================
 * Autosave policy (pure; no I/O)
 * ======================================================================== */

void config_autosave_init(config_autosave_t *a, uint32_t settle_ms)
{
    if (!a) return;
    a->pending   = false;
    a->marked_ms = 0;
    a->settle_ms = settle_ms;
}

void config_autosave_mark(config_autosave_t *a, uint32_t now_ms)
{
    if (!a) return;
    /* Re-marking pushes the deadline out: a burst of adjustments collapses to
     * one write once the user stops, instead of one write per keypress. */
    a->pending   = true;
    a->marked_ms = now_ms;
}

bool config_autosave_due(const config_autosave_t *a, uint32_t now_ms)
{
    if (!a || !a->pending) return false;
    /* Unsigned subtraction, so a tick-counter wrap yields the true elapsed
     * time rather than a ~49-day stall. */
    return (uint32_t)(now_ms - a->marked_ms) >= a->settle_ms;
}

void config_autosave_done(config_autosave_t *a)
{
    if (!a) return;
    a->pending = false;
}
