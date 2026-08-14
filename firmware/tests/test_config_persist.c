/*
 * Host tests for settings persistence — src/util/config.c (the record format
 * and the flash access) and src/util/settings_store.c (capture/apply, dirty
 * tracking, write policy).
 *
 * Build (from firmware/):
 *   gcc -std=c11 -Wall -Wextra -Werror -O1 -o build/test_config_persist \
 *       tests/test_config_persist.c src/util/config.c src/util/settings_store.c \
 *       src/drivers/flash_regions.c src/ui/theme.c src/ui/scope_state.c \
 *       -Isrc/util -Isrc/drivers -Isrc/ui -Isrc/dsp
 * Run:
 *   ./build/test_config_persist
 * Or:  make test-config-persist
 *
 * WHY IT LOOKS LIKE test_flash_regions.c
 * --------------------------------------
 * Same reason, same harness: the backend below is a NOR flash model with real
 * semantics (erase sets 0xFF, program can only clear bits, a page program
 * cannot cross a 256 B page) and it counts every erase and program. So a
 * negative result is asserted as ZERO erases, ZERO programs and a
 * byte-identical chip image — not merely as a return code. Code that returned
 * the right error while still writing would fail here.
 *
 * THE ONE THAT MATTERS MOST is test_writer_cannot_reach_a_readonly_region: the
 * chip holds irreplaceable factory calibration, and the whole point of routing
 * settings through the region layer is that a bug in THIS code cannot reach it.
 * That test aims the settings writer straight at a read-only region and checks
 * the bytes are still there afterwards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "settings_store.h"
#include "flash_regions.h"
#include "theme.h"
#include "scope_state.h"
#include "math_channel.h"
#include "ui.h"        /* METER_SUBMODE_COUNT / METER_LAYOUT_* and the globals */

/* ═══════════════════════════════════════════════════════════════════
 * Globals that live in main.c on the device. settings_store.c reads and
 * writes exactly these four; anything else it touched would fail to link,
 * which is itself a useful constraint on what counts as a "setting".
 * ═══════════════════════════════════════════════════════════════════ */

volatile bool     math_enabled  = false;
volatile uint8_t  math_op       = 0;
volatile uint8_t  meter_submode = 0;
volatile uint8_t  meter_layout  = 0;

/* ═══════════════════════════════════════════════════════════════════
 * Test harness
 * ═══════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_failed = 0;
static int current_failed = 0;

#define CHECK(cond, ...) do {                                   \
    if (!(cond)) {                                              \
        printf("  FAIL (line %d): ", __LINE__);                 \
        printf(__VA_ARGS__);                                    \
        printf("\n");                                           \
        current_failed++;                                       \
    }                                                           \
} while (0)

#define CHECK_LOAD(expr, expect) do {                           \
    config_load_result_t r_ = (expr);                           \
    if (r_ != (expect)) {                                       \
        printf("  FAIL (line %d): %s -> \"%s\", expected \"%s\"\n", \
               __LINE__, #expr, config_load_result_name(r_),    \
               config_load_result_name(expect));                \
        current_failed++;                                       \
    }                                                           \
} while (0)

typedef void (*test_fn)(void);

static void run(const char *name, test_fn fn)
{
    current_failed = 0;
    tests_run++;
    fn();
    if (current_failed) {
        tests_failed++;
        printf("FAIL  %s\n", name);
    } else {
        printf("ok    %s\n", name);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * NOR flash model (same semantics as tests/test_flash_regions.c)
 * ═══════════════════════════════════════════════════════════════════ */

#define MODEL_SIZE  FLASH_REGION_CHIP_SIZE

typedef struct {
    uint8_t *mem;
    uint32_t erases;
    uint32_t programs;
    uint32_t reads;
    uint32_t touched_lo;
    uint32_t touched_hi;
} nor_model_t;

static nor_model_t model;

static void model_alloc(void)
{
    if (model.mem == NULL) {
        model.mem = malloc(MODEL_SIZE);
        if (model.mem == NULL) {
            printf("FATAL: cannot allocate %u byte flash model\n", (unsigned)MODEL_SIZE);
            exit(2);
        }
        memset(model.mem, 0xFF, MODEL_SIZE);
    }
}

static void model_counters_reset(void)
{
    model.erases = model.programs = model.reads = 0;
    model.touched_lo = UINT32_MAX;
    model.touched_hi = 0;
}

/* Wipe the chip: a factory-fresh part. */
static void model_blank(void)
{
    model_alloc();
    memset(model.mem, 0xFF, MODEL_SIZE);
    model_counters_reset();
}

static void model_touch(uint32_t addr, uint32_t len)
{
    if (addr < model.touched_lo) model.touched_lo = addr;
    if (addr + len > model.touched_hi) model.touched_hi = addr + len;
}

static int model_read(void *ctx, uint32_t addr, void *buf, uint32_t len)
{
    (void)ctx;
    if (addr >= MODEL_SIZE || len > MODEL_SIZE - addr) {
        printf("  MODEL VIOLATION: read out of range 0x%X+%u\n", addr, len);
        exit(3);
    }
    model.reads++;
    memcpy(buf, model.mem + addr, len);
    return 0;
}

static int model_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (addr % FLASH_REGION_SECTOR_SIZE || addr >= MODEL_SIZE) {
        printf("  MODEL VIOLATION: bad erase 0x%X\n", addr);
        exit(3);
    }
    model.erases++;
    model_touch(addr, FLASH_REGION_SECTOR_SIZE);
    memset(model.mem + addr, 0xFF, FLASH_REGION_SECTOR_SIZE);
    return 0;
}

static int model_program(void *ctx, uint32_t addr, const void *data, uint32_t len)
{
    (void)ctx;
    const uint8_t *src = data;
    if (addr >= MODEL_SIZE || len > MODEL_SIZE - addr) {
        printf("  MODEL VIOLATION: program out of range 0x%X+%u\n", addr, len);
        exit(3);
    }
    if (len > FLASH_REGION_PAGE_SIZE ||
        (addr / FLASH_REGION_PAGE_SIZE) != ((addr + len - 1) / FLASH_REGION_PAGE_SIZE)) {
        printf("  MODEL VIOLATION: program crosses a page boundary 0x%X+%u\n", addr, len);
        exit(3);
    }
    model.programs++;
    model_touch(addr, len);
    for (uint32_t i = 0; i < len; i++) {
        model.mem[addr + i] &= src[i];      /* NOR: bits only go 1 -> 0 */
    }
    return 0;
}

static const flash_region_backend_t model_backend = {
    .read = model_read, .erase_sector = model_erase, .program = model_program, .ctx = NULL,
};

/* The device binding, replaced for the host build. settings_store_init() calls
 * this; on hardware it binds the flash_fs raw SPI2 primitives. */
flash_region_status_t flash_regions_bind_w25q(void)
{
    return flash_regions_init(&model_backend);
}

static uint8_t *snapshot(void)
{
    uint8_t *s = malloc(MODEL_SIZE);
    if (!s) { printf("FATAL: snapshot alloc\n"); exit(2); }
    memcpy(s, model.mem, MODEL_SIZE);
    return s;
}

static bool unchanged_since(const uint8_t *snap)
{
    return memcmp(snap, model.mem, MODEL_SIZE) == 0;
}

/* Plausible irreplaceable content everywhere the region table calls read-only,
 * so a stray write or erase shows up as data loss rather than as a change to
 * blank flash. (0x007000 is the start of stock volume "3:"'s data region; an
 * earlier comment here named `3:/System file/cal_ch1.bin`, an invented filename
 * that exists in no dump — the fill is what matters, not the label.) */
static void fill_readonly_regions(void)
{
    for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
        const flash_region_t *r = &flash_region_table[i];
        if (r->kind == FLASH_REGION_KIND_READONLY) {
            memset(model.mem + r->start, 0x5A, r->length);
        }
    }
}

static bool readonly_regions_intact(const uint8_t *snap)
{
    for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
        const flash_region_t *r = &flash_region_table[i];
        if (r->kind != FLASH_REGION_KIND_READONLY) continue;
        if (memcmp(snap + r->start, model.mem + r->start, r->length) != 0) {
            return false;
        }
    }
    return true;
}

/* A factory-fresh device with the region layer bound. */
static void fresh_device(void)
{
    model_blank();
    fill_readonly_regions();
    model_counters_reset();
    config_persist_stats_reset();
    flash_regions_stats_reset();
    if (flash_regions_bind_w25q() != FLASH_REGION_OK) {
        printf("FATAL: could not bind the region layer\n");
        exit(2);
    }
}

/* Simulate a power cycle: flash contents survive, all RAM state does not. */
static void power_cycle(void)
{
    model_counters_reset();
    theme_init(THEME_DARK_BLUE);
    scope_state_init(scope_state_get());
    math_enabled = false;
    math_op = 0;
    meter_submode = 0;
    meter_layout = 0;
    if (flash_regions_bind_w25q() != FLASH_REGION_OK) {
        printf("FATAL: rebind failed\n");
        exit(2);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Record forgery — the tests have to be able to write records the code
 * under test would never produce (torn, wrong version, wrong size).
 * crc32 is re-implemented here rather than shared: an independent copy of
 * the spec means a mismatch shows up as a test failure instead of both
 * sides being wrong together.
 * ═══════════════════════════════════════════════════════════════════ */

#define REC_MAGIC     0xA5C3u
#define REC_HDR_SIZE  8u

static uint32_t crc32_of(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static uint32_t settings_start(void)
{
    return flash_region_table[FLASH_REGION_SETTINGS].start;
}

/* Place a record directly into the model, bypassing the layer. Returns the
 * offset of the next record slot. */
static uint32_t forge_record(uint32_t offset, const void *payload, uint16_t len, uint32_t crc)
{
    uint32_t addr = settings_start() + offset;
    uint8_t hdr[REC_HDR_SIZE] = {
        (uint8_t)(REC_MAGIC & 0xFF), (uint8_t)(REC_MAGIC >> 8),
        (uint8_t)(len & 0xFF), (uint8_t)(len >> 8),
        (uint8_t)(crc & 0xFF), (uint8_t)((crc >> 8) & 0xFF),
        (uint8_t)((crc >> 16) & 0xFF), (uint8_t)((crc >> 24) & 0xFF),
    };
    memcpy(model.mem + addr, hdr, REC_HDR_SIZE);
    memcpy(model.mem + addr + REC_HDR_SIZE, payload, len);
    return offset + REC_HDR_SIZE + ((len + 3u) & ~3u);
}

/* ═══════════════════════════════════════════════════════════════════
 * 1. First boot, round trip, newest-wins
 * ═══════════════════════════════════════════════════════════════════ */

static void test_first_boot_on_blank_flash_yields_defaults(void)
{
    fresh_device();
    uint8_t *snap = snapshot();

    device_config_t cfg;
    memset(&cfg, 0xAB, sizeof cfg);              /* poison: must be overwritten */
    CHECK_LOAD(config_load_or_defaults(&cfg), CONFIG_LOAD_EMPTY);

    device_config_t defaults;
    config_init_defaults(&defaults);
    CHECK(memcmp(&cfg, &defaults, sizeof cfg) == 0, "first boot did not produce defaults");
    CHECK(config_validate(&cfg), "defaults must validate");

    /* A load is a read. It must never write, and in particular must never
     * "initialise" the log by writing a default record. */
    CHECK(model.programs == 0 && model.erases == 0,
          "loading on a blank device wrote to flash (%u programs, %u erases)",
          model.programs, model.erases);
    CHECK(unchanged_since(snap), "loading modified the chip");
    free(snap);
}

static void test_save_then_power_cycle_round_trips(void)
{
    fresh_device();

    device_config_t saved;
    config_init_defaults(&saved);
    saved.scope_ch1_vdiv = 7;
    saved.scope_timebase = 4;
    saved.theme = 2;
    saved.meter_submode = 5;
    saved.scope_trigger_level = -31;
    CHECK(config_save(&saved), "save failed");

    power_cycle();

    device_config_t loaded;
    memset(&loaded, 0x00, sizeof loaded);
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == 7, "vdiv lost: %u", loaded.scope_ch1_vdiv);
    CHECK(loaded.scope_timebase == 4, "timebase lost");
    CHECK(loaded.theme == 2, "theme lost");
    CHECK(loaded.meter_submode == 5, "meter submode lost");
    CHECK(loaded.scope_trigger_level == -31, "trigger level lost");
    CHECK(config_validate(&loaded), "loaded config must validate");
    CHECK(model.erases == 0, "a plain save/load erased a sector");
}

static void test_newest_record_wins(void)
{
    fresh_device();
    device_config_t cfg;
    config_init_defaults(&cfg);

    for (uint8_t i = 1; i <= 5; i++) {
        cfg.scope_ch1_vdiv = i;
        CHECK(config_save(&cfg), "save %u failed", i);
    }

    power_cycle();
    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == 5, "expected the newest record, got vdiv %u",
          loaded.scope_ch1_vdiv);
}

static void test_saving_the_same_settings_twice_is_free(void)
{
    fresh_device();
    device_config_t cfg;
    config_init_defaults(&cfg);
    CHECK(config_save(&cfg), "first save failed");

    uint32_t programs = model.programs;
    for (int i = 0; i < 20; i++) {
        CHECK(config_save(&cfg), "repeat save %d failed", i);
    }
    CHECK(model.programs == programs, "unchanged saves programmed flash (%u -> %u)",
          programs, model.programs);
    CHECK(model.erases == 0, "unchanged saves erased flash");
    CHECK(config_persist_stats()->saves_ok == 21, "elided saves must still count as ok");
}

/* ═══════════════════════════════════════════════════════════════════
 * 2. Damage: torn, corrupt, foreign
 * ═══════════════════════════════════════════════════════════════════ */

static void test_torn_record_falls_back_to_the_previous_one(void)
{
    fresh_device();

    device_config_t good;
    config_init_defaults(&good);
    good.scope_ch1_vdiv = 6;
    CHECK(config_save(&good), "first save failed");

    device_config_t newer = good;
    newer.scope_ch1_vdiv = 9;
    CHECK(config_save(&newer), "second save failed");

    /* Tear the newest record the way a power cut mid-program does: header
     * already programmed, payload only partly written (bits cleared). */
    uint32_t second_off = REC_HDR_SIZE + ((sizeof(device_config_t) + 3u) & ~3u);
    model.mem[settings_start() + second_off + REC_HDR_SIZE + 4] = 0x00;

    power_cycle();
    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == 6,
          "a torn newest record must fall back to the previous good one, got %u",
          loaded.scope_ch1_vdiv);

    /* And the log survives: a later save lands after the damaged record and
     * becomes the newest. */
    device_config_t next = good;
    next.scope_ch1_vdiv = 2;
    CHECK(config_save(&next), "save after a torn record failed");
    power_cycle();
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == 2, "save after a torn record did not become newest");
}

static void test_corrupt_checksum_falls_back_to_defaults(void)
{
    fresh_device();

    /* A record that is perfectly well-formed as far as the region layer is
     * concerned — correct CRC32 over the payload — but whose config checksum
     * does not match its contents. Only config_validate() can catch this, so
     * this is the test that the second integrity check is real. */
    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.scope_ch1_vdiv = 9;                  /* mutate AFTER checksumming */
    uint8_t payload[sizeof cfg];
    memcpy(payload, &cfg, sizeof payload);
    forge_record(0, payload, (uint16_t)sizeof payload, crc32_of(payload, sizeof payload));

    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_INVALID);

    device_config_t defaults;
    config_init_defaults(&defaults);
    CHECK(memcmp(&loaded, &defaults, sizeof loaded) == 0,
          "a bad-checksum record must leave defaults, not partial data");
    CHECK(loaded.scope_ch1_vdiv != 9, "the rejected record's value leaked into the config");
}

/* config_load() (as opposed to config_load_or_defaults()) promises the
 * caller's struct is untouched on failure. Without that, a rejected record is
 * memcpy'd in first and only then found to be bad — so a caller who ignores
 * the return value ends up running on a record the code just rejected. */
static void test_a_rejected_record_never_lands_in_the_callers_struct(void)
{
    fresh_device();

    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.scope_ch1_vdiv = 9;
    cfg.meter_submode  = 6;                  /* mutate AFTER checksumming */
    uint8_t payload[sizeof cfg];
    memcpy(payload, &cfg, sizeof payload);
    forge_record(0, payload, (uint16_t)sizeof payload, crc32_of(payload, sizeof payload));

    device_config_t target;
    memset(&target, 0xAB, sizeof target);
    device_config_t before = target;

    CHECK(!config_load(&target), "a bad-checksum record must not load");
    CHECK(memcmp(&target, &before, sizeof target) == 0,
          "the rejected record was written into the caller's struct anyway");
}

static void test_version_mismatch_falls_back_to_defaults(void)
{
    fresh_device();

    /* A fully valid record from a different firmware version: right magic,
     * right size, correct config checksum, correct CRC32. Only the version
     * differs. This is the field-upgrade case. */
    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.scope_ch1_vdiv = 8;
    cfg.version = 99;
    cfg.checksum = config_compute_checksum(&cfg);
    CHECK(!config_validate(&cfg), "a version-99 config must not validate");

    uint8_t payload[sizeof cfg];
    memcpy(payload, &cfg, sizeof payload);
    forge_record(0, payload, (uint16_t)sizeof payload, crc32_of(payload, sizeof payload));

    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_INVALID);
    CHECK(loaded.version == CONFIG_VERSION, "defaults must carry the current version");
    CHECK(loaded.scope_ch1_vdiv == 3, "foreign record's value survived into defaults");
}

static void test_wrong_size_record_falls_back_to_defaults(void)
{
    fresh_device();

    /* A record from a build whose struct was smaller. If this were memcpy'd in,
     * the tail of the config would be uninitialised stack. */
    uint8_t payload[16];
    memset(payload, 0x11, sizeof payload);
    forge_record(0, payload, (uint16_t)sizeof payload, crc32_of(payload, sizeof payload));

    device_config_t loaded;
    memset(&loaded, 0x00, sizeof loaded);
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_BAD_SIZE);
    CHECK(config_validate(&loaded), "must be left holding valid defaults");

    /* And one that is too big for the caller's buffer — the region layer
     * reports BOUNDS rather than truncating. */
    fresh_device();
    uint8_t big[sizeof(device_config_t) + 64];
    memset(big, 0x22, sizeof big);
    forge_record(0, big, (uint16_t)sizeof big, crc32_of(big, sizeof big));
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_BAD_SIZE);
    CHECK(config_validate(&loaded), "oversized record must still leave valid defaults");
}

static void test_completely_corrupt_log_falls_back_to_defaults(void)
{
    fresh_device();
    device_config_t cfg;
    config_init_defaults(&cfg);
    CHECK(config_save(&cfg), "save failed");

    /* Clear bits all over the record: header magic intact is not required —
     * this is the "the log is rubble" case. */
    memset(model.mem + settings_start(), 0x00, 512);

    device_config_t loaded;
    config_load_result_t r = config_load_or_defaults(&loaded);
    CHECK(r != CONFIG_LOAD_OK, "a destroyed log must not load");
    CHECK(config_validate(&loaded), "a destroyed log must still leave valid defaults");
}

/* ═══════════════════════════════════════════════════════════════════
 * 3. Containment — the reason this goes through the region layer
 * ═══════════════════════════════════════════════════════════════════ */

/* Aim the settings writer at a read-only region and check the bytes survive.
 * This is the "a bug in your code cannot reach a read-only region" test: the
 * table entry the writer uses is swapped for a read-only one, which is the
 * strongest form of the mistake — the code asks to write to protected flash
 * and the layer is the only thing standing in the way. */
static void test_writer_cannot_reach_a_readonly_region(void)
{
    /* A table where the id config.c writes to (FLASH_REGION_SETTINGS == 3)
     * is READ-ONLY and holds "factory calibration". */
    static const flash_region_t hostile[] = {
        [FLASH_REGION_SYSVOL]   = { "sysvol",   0x000000u, 0x010000u, FLASH_REGION_KIND_READONLY },
        [FLASH_REGION_USERVOL]  = { "uservol",  0x010000u, 0x010000u, FLASH_REGION_KIND_READONLY },
        [FLASH_REGION_USER_CAL] = { "usercal",  0x020000u, 0x010000u, FLASH_REGION_KIND_APPEND   },
        [FLASH_REGION_SETTINGS] = { "factory",  0x030000u, 0x010000u, FLASH_REGION_KIND_READONLY },
    };

    model_blank();
    memset(model.mem + 0x030000u, 0x5A, 301);      /* irreplaceable content in a read-only region */
    config_persist_stats_reset();
    model_counters_reset();
    CHECK(flash_regions_init_table(&model_backend, hostile, 4) == FLASH_REGION_OK,
          "hostile table should be structurally valid");
    uint8_t *snap = snapshot();

    device_config_t cfg;
    config_init_defaults(&cfg);
    for (int i = 0; i < 5; i++) {
        cfg.scope_ch1_vdiv = (uint8_t)i;
        CHECK(!config_save(&cfg), "a save into a read-only region must fail");
    }

    CHECK(model.programs == 0, "%u programs reached read-only flash", model.programs);
    CHECK(model.erases == 0, "%u erases reached read-only flash", model.erases);
    CHECK(unchanged_since(snap), "read-only flash content changed");
    CHECK(model.mem[0x030000u] == 0x5A && model.mem[0x030000u + 300] == 0x5A,
          "the stand-in factory calibration was damaged");
    CHECK(config_persist_stats()->saves_failed == 5, "refusals not counted");
    CHECK(config_persist_stats()->last_save_status == (int32_t)FLASH_REGION_ERR_READ_ONLY,
          "expected a READ_ONLY status, got %d",
          (int)config_persist_stats()->last_save_status);

    /* Loading is refused too, but as "no storage" — a read-only region cannot
     * be an append log — and the caller is still left holding valid defaults
     * rather than nothing. */
    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_NO_STORAGE);
    CHECK(config_validate(&loaded), "must still be left with valid defaults");
    free(snap);

    fresh_device();      /* restore the shipped table for later tests */
}

static void test_normal_operation_never_leaves_the_settings_region(void)
{
    fresh_device();
    uint8_t *snap = snapshot();

    const flash_region_t *set = flash_region_get(FLASH_REGION_SETTINGS);
    device_config_t cfg;
    config_init_defaults(&cfg);

    /* Enough saves to fill the log several times over, so compaction (the one
     * erase path this code has) runs repeatedly. */
    const uint32_t per_record = REC_HDR_SIZE + ((sizeof(device_config_t) + 3u) & ~3u);
    const uint32_t capacity = set->length / per_record;
    for (uint32_t i = 0; i < capacity * 3u; i++) {
        cfg.scope_ch1_vdiv = (uint8_t)(i % 10u);
        cfg.scope_timebase = (uint8_t)(i % 20u);
        CHECK(config_save(&cfg), "save %u failed", i);
        if (current_failed) break;
    }

    CHECK(config_persist_stats()->compactions >= 2, "expected repeated compaction, saw %u",
          config_persist_stats()->compactions);
    CHECK(model.touched_lo >= set->start, "touched 0x%X, below the settings region",
          model.touched_lo);
    CHECK(model.touched_hi <= set->start + set->length,
          "touched 0x%X, past the end of the settings region", model.touched_hi);
    CHECK(readonly_regions_intact(snap), "a read-only region changed during normal saves");

    /* Nothing outside the settings region moved at all. */
    CHECK(memcmp(snap, model.mem, set->start) == 0, "flash below the settings region changed");
    CHECK(memcmp(snap + set->start + set->length,
                 model.mem + set->start + set->length,
                 MODEL_SIZE - (set->start + set->length)) == 0,
          "flash above the settings region changed");
    free(snap);

    /* The value written last is still the one that loads, across compaction. */
    power_cycle();
    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == cfg.scope_ch1_vdiv, "post-compaction value is wrong");
}

static void test_no_storage_bound_refuses_rather_than_pretending(void)
{
    model_blank();
    fill_readonly_regions();
    config_persist_stats_reset();
    model_counters_reset();

    /* A malformed table leaves the layer dead. That is the same state as
     * "never bound", and it is exactly when the old stub would have claimed
     * success and lost the data. */
    static const flash_region_t broken[] = {
        { "a", 0x000000u, 0x002000u, FLASH_REGION_KIND_RW },
        { "b", 0x001000u, 0x002000u, FLASH_REGION_KIND_RW },   /* overlaps */
    };
    CHECK(flash_regions_init_table(&model_backend, broken, 2) == FLASH_REGION_ERR_TABLE,
          "overlapping table should be rejected");
    CHECK(!flash_regions_ready(), "layer must not be ready");
    uint8_t *snap = snapshot();

    device_config_t cfg;
    config_init_defaults(&cfg);
    CHECK(!config_save(&cfg), "save with no storage must return false");

    device_config_t loaded;
    memset(&loaded, 0x77, sizeof loaded);
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_NO_STORAGE);
    CHECK(config_validate(&loaded), "must still be left with valid defaults");
    CHECK(model.programs == 0 && model.erases == 0, "a dead layer touched flash");
    CHECK(unchanged_since(snap), "a dead layer changed flash");
    free(snap);

    fresh_device();
}

/* ═══════════════════════════════════════════════════════════════════
 * 4. Autosave policy (pure timing, no flash)
 * ═══════════════════════════════════════════════════════════════════ */

static void test_autosave_settle_window(void)
{
    config_autosave_t a;
    config_autosave_init(&a, 2000);
    CHECK(!config_autosave_due(&a, 10000), "nothing pending must never be due");

    config_autosave_mark(&a, 10000);
    CHECK(!config_autosave_due(&a, 10000), "due immediately — the window does nothing");
    CHECK(!config_autosave_due(&a, 11999), "due 1 ms early");
    CHECK(config_autosave_due(&a, 12000), "not due at exactly the settle time");
    CHECK(config_autosave_due(&a, 99999), "not due long after");

    /* A second change restarts the window: a burst collapses to one write. */
    config_autosave_mark(&a, 11000);
    CHECK(!config_autosave_due(&a, 12000), "re-marking did not push the deadline out");
    CHECK(config_autosave_due(&a, 13000), "not due after the restarted window");

    config_autosave_done(&a);
    CHECK(!config_autosave_due(&a, 99999), "done() did not clear pending");
}

static void test_autosave_survives_tick_wrap(void)
{
    /* The FreeRTOS tick wraps every ~49 days at 1 kHz. A naive
     * `now >= marked + settle` compare stalls the save until the counter
     * catches up again. */
    config_autosave_t a;
    config_autosave_init(&a, 2000);
    config_autosave_mark(&a, 0xFFFFFF00u);           /* 256 ms before the wrap */
    CHECK(!config_autosave_due(&a, 0xFFFFFFF0u), "due too early across the wrap");
    CHECK(!config_autosave_due(&a, 0x000006C0u), "due at 1984 ms elapsed, before the window");
    CHECK(config_autosave_due(&a, 0x00000800u), "not due at 2304 ms elapsed, past the wrap");
}

/* ═══════════════════════════════════════════════════════════════════
 * 5. settings_store: capture / apply / write policy
 * ═══════════════════════════════════════════════════════════════════ */

static void test_live_settings_survive_a_power_cycle(void)
{
    fresh_device();
    power_cycle();
    settings_store_init();

    /* The user changes things. */
    scope_state_t *ss = scope_state_get();
    ss->ch1.vdiv_idx = 8;
    ss->ch2.coupling = COUPLING_AC;
    ss->timebase_idx = 15;
    ss->trigger.mode = TRIG_NORMAL;
    ss->trigger.level = -20;
    theme_set(THEME_NIGHT_RED);
    meter_layout = METER_LAYOUT_STATS;

    /* Button press: notices the change, but the window has not elapsed. */
    settings_store_note_change(1000);
    CHECK(!settings_store_service(1000), "wrote before the settle window elapsed");
    CHECK(model.programs == 0, "wrote to flash before settling");

    /* Next press, after the window. Pressing an unrelated button must NOT
     * push the deadline out again — only a new change does that. */
    uint32_t writes0 = settings_store_get_status()->writes;
    settings_store_note_change(4000);
    CHECK(settings_store_service(4000), "did not write after the settle window");
    CHECK(settings_store_get_status()->writes == writes0 + 1, "write not counted");

    power_cycle();
    settings_store_init();

    ss = scope_state_get();
    CHECK(ss->ch1.vdiv_idx == 8, "vdiv not restored (%u)", ss->ch1.vdiv_idx);
    CHECK(ss->ch2.coupling == COUPLING_AC, "coupling not restored");
    CHECK(ss->timebase_idx == 15, "timebase not restored");
    CHECK(ss->trigger.mode == TRIG_NORMAL, "trigger mode not restored");
    CHECK(ss->trigger.level == -20, "trigger level not restored");
    CHECK(theme_get_id() == THEME_NIGHT_RED, "theme not restored");
    CHECK(meter_layout == METER_LAYOUT_STATS, "meter layout not restored");
    CHECK(settings_store_get_status()->load_result == CONFIG_LOAD_OK,
          "expected a loaded record, got \"%s\"",
          config_load_result_name(settings_store_get_status()->load_result));
}

static void test_presses_that_change_nothing_never_write(void)
{
    fresh_device();
    power_cycle();
    settings_store_init();

    uint32_t programs = model.programs;
    uint32_t writes0  = settings_store_get_status()->writes;   /* status is cumulative */
    for (uint32_t t = 0; t < 100; t++) {
        settings_store_note_change(t * 1000u);
        (void)settings_store_service(t * 1000u);
    }
    CHECK(model.programs == programs, "idle presses wrote to flash (%u -> %u)",
          programs, model.programs);
    CHECK(model.erases == 0, "idle presses erased flash");
    CHECK(settings_store_get_status()->writes == writes0, "idle presses counted as writes");
}

static void test_a_change_undone_before_settling_costs_nothing(void)
{
    fresh_device();
    power_cycle();
    settings_store_init();
    uint32_t programs = model.programs;

    scope_state_t *ss = scope_state_get();
    uint8_t original = ss->ch1.vdiv_idx;

    ss->ch1.vdiv_idx = (uint8_t)(original + 1u);
    settings_store_note_change(1000);

    ss->ch1.vdiv_idx = original;              /* user changed their mind */
    settings_store_note_change(1500);

    CHECK(!settings_store_service(9000), "an undone change was still written");
    CHECK(!settings_store_flush(9000), "an undone change was still flushed");
    CHECK(model.programs == programs, "an undone change wrote to flash");
}

static void test_flush_ignores_the_settle_window(void)
{
    fresh_device();
    power_cycle();
    settings_store_init();

    scope_state_t *ss = scope_state_get();
    ss->timebase_idx = 3;

    /* This is the power-off / mode-change path: the change is 1 ms old and
     * must still reach flash. */
    CHECK(settings_store_flush(1), "flush did not write a fresh change");

    power_cycle();
    settings_store_init();
    CHECK(scope_state_get()->timebase_idx == 3, "flushed value did not survive");
}

static void test_corrupt_record_cannot_produce_an_out_of_range_index(void)
{
    fresh_device();
    power_cycle();

    /* A record that passes CRC32 and passes the config checksum, but whose
     * indices are nonsense — an older layout, or a bit flip the byte-sum
     * checksum happens not to catch. Applying it unclamped would index
     * vdiv_table[200] and theme[99]. */
    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.scope_ch1_vdiv      = 200;
    cfg.scope_ch2_vdiv      = 255;
    cfg.scope_timebase      = 99;
    cfg.scope_trigger_mode  = 77;
    cfg.scope_trigger_edge  = 5;
    cfg.scope_trigger_source = 9;
    cfg.scope_ch1_coupling  = 40;
    cfg.scope_ch1_probe     = 6;
    cfg.theme               = 99;
    cfg.math_op             = 88;
    cfg.meter_submode       = 200;
    cfg.meter_layout        = 60;
    cfg.scope_trigger_level = 30000;
    cfg.checksum = config_compute_checksum(&cfg);
    CHECK(config_validate(&cfg), "the forged record must be internally valid");

    uint8_t payload[sizeof cfg];
    memcpy(payload, &cfg, sizeof payload);
    forge_record(0, payload, (uint16_t)sizeof payload, crc32_of(payload, sizeof payload));

    settings_store_init();
    CHECK(settings_store_get_status()->load_result == CONFIG_LOAD_OK,
          "the record should load — it is valid, just insane");

    const scope_state_t *ss = scope_state_get();
    CHECK(ss->ch1.vdiv_idx < VDIV_COUNT, "ch1 vdiv index %u out of range", ss->ch1.vdiv_idx);
    CHECK(ss->ch2.vdiv_idx < VDIV_COUNT, "ch2 vdiv index %u out of range", ss->ch2.vdiv_idx);
    CHECK(ss->timebase_idx < TIMEBASE_COUNT, "timebase index %u out of range", ss->timebase_idx);
    CHECK(ss->trigger.mode < TRIG_COUNT, "trigger mode out of range");
    CHECK(ss->trigger.edge < TRIG_EDGE_COUNT, "trigger edge out of range");
    CHECK(ss->trigger.source < TRIG_SRC_COUNT, "trigger source out of range");
    CHECK(ss->ch1.coupling < COUPLING_COUNT, "coupling out of range");
    CHECK(ss->ch1.probe < PROBE_COUNT, "probe out of range");
    CHECK(theme_get_id() < THEME_COUNT, "theme id out of range");
    CHECK(math_op < MATH_COUNT, "math op out of range");
    CHECK(meter_submode < METER_SUBMODE_COUNT, "meter submode out of range");
    CHECK(meter_layout < METER_LAYOUT_COUNT, "meter layout out of range");
    CHECK(ss->trigger.level <= 103 && ss->trigger.level >= -103,
          "trigger level %d not clamped", ss->trigger.level);

    /* The tables really are indexable with what came out. */
    CHECK(vdiv_table[ss->ch1.vdiv_idx].label != NULL, "vdiv table entry is bogus");
    CHECK(timebase_table[ss->timebase_idx].label != NULL, "timebase table entry is bogus");
}

/* If the settings region cannot be written — not blank, flash failing, table
 * wrong — the store must not re-attempt the save on every press. Each attempt
 * scans the whole log in short SPI reads, so an unwritable device would spend
 * ~100 ms of SPI2 traffic per keypress achieving nothing. */
static void test_a_failing_write_is_not_retried_on_every_press(void)
{
    static const flash_region_t hostile[] = {
        [FLASH_REGION_SYSVOL]   = { "sysvol",   0x000000u, 0x010000u, FLASH_REGION_KIND_READONLY },
        [FLASH_REGION_USERVOL]  = { "uservol",  0x010000u, 0x010000u, FLASH_REGION_KIND_READONLY },
        [FLASH_REGION_USER_CAL] = { "usercal",  0x020000u, 0x010000u, FLASH_REGION_KIND_APPEND   },
        [FLASH_REGION_SETTINGS] = { "factory",  0x030000u, 0x010000u, FLASH_REGION_KIND_READONLY },
    };
    model_blank();
    config_persist_stats_reset();
    CHECK(flash_regions_init_table(&model_backend, hostile, 4) == FLASH_REGION_OK,
          "hostile table should be structurally valid");

    theme_init(THEME_DARK_BLUE);
    scope_state_init(scope_state_get());
    (void)settings_store_load_and_apply();

    uint32_t failures0 = settings_store_get_status()->write_failures;

    scope_state_get()->timebase_idx = 7;              /* one change */
    settings_store_note_change(1000);
    CHECK(!settings_store_service(9000), "the write should have failed");

    for (uint32_t t = 10; t < 60; t++) {             /* fifty more presses */
        settings_store_note_change(t * 1000u);
        CHECK(!settings_store_service(t * 1000u), "a blocked write reported success");
        CHECK(!settings_store_flush(t * 1000u), "a blocked flush reported success");
    }

    CHECK(settings_store_get_status()->write_failures == failures0 + 1,
          "one change should cost one failed attempt, not one per press (saw %u)",
          settings_store_get_status()->write_failures - failures0);

    /* A NEW change is worth another attempt. */
    scope_state_get()->timebase_idx = 8;
    settings_store_note_change(61000);
    CHECK(!settings_store_service(70000), "still unwritable");
    CHECK(settings_store_get_status()->write_failures == failures0 + 2,
          "a new change must re-arm the attempt");

    fresh_device();
}

static void test_capture_apply_round_trip_is_symmetric(void)
{
    /* Every field capture() writes must be one apply() restores; a field on
     * only one side is a setting that silently does not persist. */
    fresh_device();
    power_cycle();

    scope_state_t *ss = scope_state_get();
    ss->ch1.enabled = false;
    ss->ch1.vdiv_idx = 1;
    ss->ch1.coupling = COUPLING_GND;
    ss->ch1.probe = PROBE_10X;
    ss->ch1.bw_limit = true;
    ss->ch2.enabled = true;
    ss->ch2.vdiv_idx = 9;
    ss->ch2.coupling = COUPLING_AC;
    ss->ch2.probe = PROBE_10X;
    ss->ch2.bw_limit = true;
    ss->timebase_idx = 20;
    ss->trigger.mode = TRIG_SINGLE;
    ss->trigger.edge = TRIG_FALLING;
    ss->trigger.source = TRIG_SRC_CH2;
    ss->trigger.level = 42;
    theme_set(THEME_HIGH_CONTRAST);
    math_enabled = true;
    math_op = MATH_SUB;
    meter_submode = 7;
    meter_layout = METER_LAYOUT_FUSE;

    device_config_t cfg;
    config_init_defaults(&cfg);
    settings_store_capture(&cfg);

    /* Wipe live state, then put it back from the captured config only. */
    scope_state_init(scope_state_get());
    theme_init(THEME_DARK_BLUE);
    math_enabled = false; math_op = 0; meter_submode = 0; meter_layout = 0;

    settings_store_apply(&cfg);

    ss = scope_state_get();
    CHECK(ss->ch1.enabled == false && ss->ch2.enabled == true, "channel enables lost");
    CHECK(ss->ch1.vdiv_idx == 1 && ss->ch2.vdiv_idx == 9, "vdiv lost");
    CHECK(ss->ch1.coupling == COUPLING_GND && ss->ch2.coupling == COUPLING_AC, "coupling lost");
    CHECK(ss->ch1.probe == PROBE_10X && ss->ch2.probe == PROBE_10X, "probe lost");
    CHECK(ss->ch1.bw_limit && ss->ch2.bw_limit, "bw limit lost");
    CHECK(ss->timebase_idx == 20, "timebase lost");
    CHECK(ss->trigger.mode == TRIG_SINGLE, "trigger mode lost");
    CHECK(ss->trigger.edge == TRIG_FALLING, "trigger edge lost");
    CHECK(ss->trigger.source == TRIG_SRC_CH2, "trigger source lost");
    CHECK(ss->trigger.level == 42, "trigger level lost");
    CHECK(theme_get_id() == THEME_HIGH_CONTRAST, "theme lost");
    CHECK(math_enabled && math_op == MATH_SUB, "math settings lost");
    CHECK(meter_submode == 7, "meter submode lost");
    CHECK(meter_layout == METER_LAYOUT_FUSE, "meter layout lost");
}

/* ═══════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== settings persistence ===\n");
    printf("(device_config_t is %u bytes; record slot %u)\n",
           (unsigned)sizeof(device_config_t),
           (unsigned)(REC_HDR_SIZE + ((sizeof(device_config_t) + 3u) & ~3u)));

    run("first boot on blank flash yields defaults", test_first_boot_on_blank_flash_yields_defaults);
    run("save then power cycle round-trips", test_save_then_power_cycle_round_trips);
    run("newest record wins", test_newest_record_wins);
    run("saving unchanged settings is free", test_saving_the_same_settings_twice_is_free);

    run("torn record falls back to the previous one", test_torn_record_falls_back_to_the_previous_one);
    run("corrupt checksum falls back to defaults", test_corrupt_checksum_falls_back_to_defaults);
    run("a rejected record never lands in the caller's struct",
        test_a_rejected_record_never_lands_in_the_callers_struct);
    run("version mismatch falls back to defaults", test_version_mismatch_falls_back_to_defaults);
    run("wrong-size record falls back to defaults", test_wrong_size_record_falls_back_to_defaults);
    run("a destroyed log falls back to defaults", test_completely_corrupt_log_falls_back_to_defaults);

    run("the writer cannot reach a read-only region", test_writer_cannot_reach_a_readonly_region);
    run("normal operation never leaves the settings region",
        test_normal_operation_never_leaves_the_settings_region);
    run("no storage bound refuses rather than pretending",
        test_no_storage_bound_refuses_rather_than_pretending);

    run("autosave settle window", test_autosave_settle_window);
    run("autosave survives a tick wrap", test_autosave_survives_tick_wrap);

    run("live settings survive a power cycle", test_live_settings_survive_a_power_cycle);
    run("presses that change nothing never write", test_presses_that_change_nothing_never_write);
    run("a change undone before settling costs nothing",
        test_a_change_undone_before_settling_costs_nothing);
    run("flush ignores the settle window", test_flush_ignores_the_settle_window);
    run("a corrupt record cannot produce an out-of-range index",
        test_corrupt_record_cannot_produce_an_out_of_range_index);
    run("a failing write is not retried on every press",
        test_a_failing_write_is_not_retried_on_every_press);
    run("capture/apply round trip is symmetric", test_capture_apply_round_trip_is_symmetric);

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0) {
        printf("%d tests OK\n", tests_run);
    }
    free(model.mem);
    return tests_failed == 0 ? 0 : 1;
}
