/*
 * OpenScope 2C53T - Configuration Save/Load
 *
 * Stores and retrieves user-configurable device settings, and — as of
 * 2026-08-13 — actually persists them across a power cycle.
 *
 * STORAGE BACKEND
 * ---------------
 * The append record log in FLASH_REGION_SETTINGS (flash_regions.h), i.e. the
 * 64 KB window at 0xF10000 of the external W25Q128JV. Nothing here ever names
 * an absolute chip address: every access goes through a region id, so the
 * region layer's read-only enforcement stands between this code and the stock
 * UI assets / `3:/System file/` factory-calibration path. A bug in this file
 * cannot reach them — see test_config_persist.c, which proves that by aiming
 * the writer at a read-only table entry and checking the chip is untouched.
 *
 * Records are appended, never rewritten in place: each save is a new record and
 * the newest CRC-valid one wins. The region layer programs the record header
 * BEFORE the payload, so a save interrupted by a power cut leaves a record of
 * known length whose CRC fails; the scanner steps over it and the previous good
 * settings are still loaded.
 *
 * DEFENCE IN DEPTH. Two independent integrity checks guard a load:
 *   1. the region layer's CRC32 over the record payload (catches a torn or
 *      bit-rotted record);
 *   2. this file's magic / version / checksum (catches a well-formed record
 *      that is not a config of this version).
 * Anything that fails either check falls back to defaults rather than loading
 * garbage into live UI state.
 *
 * ZERO-INIT INVARIANT (load-bearing). device_config_t contains compiler
 * padding. config_init_defaults() memsets the whole struct, so padding is a
 * deterministic zero; callers must derive every config from it and then mutate
 * fields, never build one on an uninitialised stack struct. If padding were
 * random, the checksum would still be self-consistent, but the append log's
 * "identical payload" elision would never fire and every save would burn a
 * record.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAGIC    0x4F534332  /* "OSC2" */

/* Bumped to 3 on 2026-08-13: v3 adds the theme/probe/bw-limit/trigger-source/
 * math/meter block below. A v2 record on a device updated in the field fails
 * the version check and the device comes up on defaults — which is the whole
 * point of having the field. */
#define CONFIG_VERSION  3

#define CONFIG_MODE_OSCILLOSCOPE  0
#define CONFIG_MODE_MULTIMETER    1
#define CONFIG_MODE_SIGNAL_GEN     2
#define CONFIG_MODE_SETTINGS       3

/* All user-configurable settings.
 *
 * WHICH FIELDS ARE LIVE: settings_store.c captures and re-applies the scope
 * block, the theme, the math block and the meter block. The FFT, signal
 * generator, brightness, language, auto-shutdown and sound fields are stored
 * and round-tripped but NOT yet applied at boot — nothing in the firmware
 * currently owns them (there is no backlight PWM, and siggen is initialised
 * before this layer runs). They are listed here rather than deleted so a v3
 * record stays forward-compatible when those get wired up; do not read them
 * back expecting them to mean anything on hardware yet. */
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
    uint8_t  startup_mode;       /* CONFIG_MODE_* */
    /* System */
    uint8_t  auto_shutdown_mins; /* 0=disabled, 5/10/15/30/60 */
    uint8_t  sound_enabled;
    /* v3 block */
    uint8_t  theme;              /* theme_id_t */
    uint8_t  scope_ch1_probe;    /* 0=1x, 1=10x */
    uint8_t  scope_ch2_probe;
    uint8_t  scope_ch1_bw_limit; /* 20MHz limit on/off */
    uint8_t  scope_ch2_bw_limit;
    uint8_t  scope_trigger_source; /* 0=CH1, 1=CH2 */
    uint8_t  math_enabled;
    uint8_t  math_op;
    uint8_t  meter_submode;
    uint8_t  meter_layout;
    /* Checksum */
    uint32_t checksum;           /* Sum of all bytes before this field */
} device_config_t;

/* Initialize config with defaults. Memsets first — see the zero-init
 * invariant above. */
void config_init_defaults(device_config_t *cfg);

/* Compute checksum for config validation */
uint32_t config_compute_checksum(const device_config_t *cfg);

/* Validate a config (magic, version, checksum) */
bool config_validate(const device_config_t *cfg);

/* Serialize config to a byte buffer (for saving to flash).
 * Returns number of bytes written, or 0 on failure. */
uint32_t config_serialize(const device_config_t *cfg, uint8_t *buf, uint32_t buf_size);

/* Deserialize config from a byte buffer (loaded from flash).
 * Returns false if magic, version, or checksum is invalid.
 * NOTE: on failure *cfg has already been overwritten with the rejected bytes.
 * Use config_load()/config_load_or_defaults(), which never leave a caller's
 * struct holding a rejected record. */
bool config_deserialize(device_config_t *cfg, const uint8_t *buf, uint32_t buf_size);

/* ── Write enable — OFF BY DEFAULT ───────────────────────────────────
 *
 * SETTINGS_PERSIST_WRITES gates every flash WRITE this module can make.
 * Reads (config_load, config_load_or_defaults) are never gated.
 *
 * Why it defaults to 0, as of 2026-08-13: this is the first code in the
 * project's history to write the W25Q at runtime, and it has never done so on
 * any physical unit — every result behind it is a host model of NOR behaviour.
 * Models are where this project's bugs have historically lived.
 *
 * The exposure is NOT the factory calibration. Cal is in FLASH_REGION_SYSVOL
 * (0x000000-0x1FFFFF), marked READONLY and enforced by address; every write
 * here goes to FLASH_REGION_SETTINGS at 0xF10000, ~14 MB away and a different
 * region kind. tests/test_config_persist.c aims the writer at sysvol on
 * purpose and every write is refused, with the chip byte-identical after.
 *
 * The exposure is that the region map was derived from full-chip dumps of ONE
 * unit (bench unit #1), where 0xF00000+ is blank. Whether stock's FAT12 on a
 * different unit or firmware revision claims those clusters is unverified.
 * Worst case there is losing settings or a screenshot, not calibration.
 *
 * It stayed off because the failure is asymmetric: low probability, and —
 * until a factory-cal backup exists — no recovery path if the reasoning above
 * is wrong somewhere. The flip condition was: run the supervised bench check
 * (`make guest-persist`), and change this default only once it passed.
 *
 * IT PASSED — 2026-08-20, bench unit #1. First record ever written (one
 * 56-byte record at 0xF10000, verified raw over the shell), restored and
 * pushed to the FPGA across THREE consecutive power cycles, byte-identical
 * no-op elision confirmed across each power-off flush, and `flash wtest`
 * green. The default is therefore now 1. The interlock did its job in both
 * directions, at a price: between 2026-08-13 and 2026-08-20 nobody ran the
 * commissioning step, every flush on every bench build silently refused, and
 * a bench session blamed the write path — because the store's status counted
 * this CHOSEN refusal as a write FAILURE, the exact conflation the paragraph
 * below warned against, and nothing printed writes_enabled until the
 * `settings` shell command grew the line on 2026-08-20.
 *
 * With writes disabled config_save() returns false and counts saves_disabled,
 * NOT saves_failed. A refusal we chose is not a failure we suffered, and any
 * diagnostic that conflates them is lying in the way this project keeps
 * getting burned by.
 */
#ifndef SETTINGS_PERSIST_WRITES
#define SETTINGS_PERSIST_WRITES 1
#endif

/* ── Persistence ─────────────────────────────────────────────────────
 * These require flash_regions_init()/flash_regions_bind_w25q() to have run.
 * With no backend bound they refuse (config_save returns false) rather than
 * pretending: a save that silently goes nowhere is exactly the bug this
 * replaced. */

/* Append the config as a new record. magic/version/checksum are (re)stamped
 * from the current build, so a caller cannot save a record it has mislabelled.
 * A save whose payload is byte-identical to the newest record is elided by the
 * region layer and still returns true. Returns false if the layer is not bound,
 * refused the write, or could not verify it. */
bool config_save(const device_config_t *cfg);

/* Load the newest valid record. Returns false — leaving *cfg BYTE-FOR-BYTE
 * untouched — when there is no storage, no record, or the record fails any
 * integrity check. Nothing partially-parsed is ever left in the caller's
 * struct; that is a tested property, not an intention. */
bool config_load(device_config_t *cfg);

typedef enum {
    CONFIG_LOAD_OK = 0,             /* a valid record was loaded              */
    CONFIG_LOAD_NO_STORAGE,         /* region layer not bound                 */
    CONFIG_LOAD_EMPTY,              /* first boot: no record in the log       */
    CONFIG_LOAD_BAD_SIZE,           /* record is not a config of this build   */
    CONFIG_LOAD_INVALID,            /* magic / version / checksum mismatch    */
    CONFIG_LOAD_IO,                 /* flash read failed                      */
} config_load_result_t;

/* Load, or fill *cfg with defaults. *cfg is always left valid, so the caller
 * has no "did it work" branch to get wrong. */
config_load_result_t config_load_or_defaults(device_config_t *cfg);

const char *config_load_result_name(config_load_result_t r);

typedef struct {
    uint32_t saves_ok;         /* including elided no-op saves */
    uint32_t saves_failed;
    uint32_t saves_disabled;   /* refused because SETTINGS_PERSIST_WRITES==0.
                                * Deliberately NOT counted as saves_failed —
                                * nothing was attempted and nothing went wrong. */
    bool     writes_enabled;   /* mirrors SETTINGS_PERSIST_WRITES, so a reader
                                * of these stats never has to guess which build
                                * produced them. */
    uint32_t compactions;      /* log full -> region reset -> re-append */
    uint32_t loads_ok;
    uint32_t loads_defaulted;
    int32_t  last_save_status; /* flash_region_status_t of the last append */
    int32_t  last_load_status;
} config_persist_stats_t;

const config_persist_stats_t *config_persist_stats(void);
void config_persist_stats_reset(void);

/* ── Autosave policy ─────────────────────────────────────────────────
 * Pure timing policy, no I/O, so it can be tested without a flash model.
 * A settings change marks the state dirty; the write happens only once the
 * change has settled for settle_ms. Deliberately wrap-safe: FreeRTOS tick
 * counts roll over and a naive `now > marked + settle` compare would stall a
 * save for 49 days at the wrap. */
typedef struct {
    bool     pending;
    uint32_t marked_ms;
    uint32_t settle_ms;
} config_autosave_t;

void config_autosave_init(config_autosave_t *a, uint32_t settle_ms);
void config_autosave_mark(config_autosave_t *a, uint32_t now_ms);
bool config_autosave_due(const config_autosave_t *a, uint32_t now_ms);
void config_autosave_done(config_autosave_t *a);

#endif /* CONFIG_H */
