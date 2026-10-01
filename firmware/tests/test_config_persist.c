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
 *
 * THREE BUILDS OF THIS ONE FILE (all from the Makefile; one flash model)
 * ----------------------------------------------------------------------
 *   test_config_persist            -DSETTINGS_PERSIST_WRITES=1   must be green
 *   test_config_persist_nowrite    -DSETTINGS_PERSIST_WRITES=0   must be green
 *       The negative control (settings-persistence spec, S3): the write path
 *       stubbed by the real build-time switch. Runs ONLY the change -> power
 *       cycle -> verify loop and requires that nothing survives — and that the
 *       positive version of that test goes red. A loop that passes with the
 *       writes compiled out is not testing persistence.
 *   test_config_persist_known_defects  -DCONFIG_PERSIST_KNOWN_DEFECTS=1
 *       EXPECTED TO FAIL. Each test asserts the CORRECT behaviour for a power-
 *       cut case the firmware currently gets wrong, so the fix turning it green
 *       is the verification. Run by name (`make test-config-persist-known-
 *       defects`); deliberately not part of `make test-config-persist`, the
 *       same convention as test-meter-word-map.
 *
 * `make test-config-persist` builds and runs the first two.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "settings_store.h"
#include "flash_regions.h"
#include "scope_cal.h"
#include "scope_timebase.h"
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
static bool quiet_checks = false;   /* run_expect_red(): failures are the point */

#define CHECK(cond, ...) do {                                   \
    if (!(cond)) {                                              \
        if (!quiet_checks) {                                    \
            printf("  FAIL (line %d): ", __LINE__);             \
            printf(__VA_ARGS__);                                \
            printf("\n");                                       \
        }                                                       \
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

/* Run a test that MUST fail — the negative control's way of proving that a
 * positive test can go red. Its own CHECK output is suppressed (every line of
 * it would be an expected failure); what is reported is whether it failed. */
static void run_expect_red(const char *name, test_fn fn)
{
    current_failed = 0;
    tests_run++;
    quiet_checks = true;
    fn();
    quiet_checks = false;
    if (current_failed) {
        printf("ok    %s (red as required: %d failed check(s))\n", name, current_failed);
    } else {
        tests_failed++;
        printf("FAIL  %s (passed, but it must fail in this build)\n", name);
    }
    current_failed = 0;
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

/* Power-cut injection. Armed with a budget of programmed bytes and of sector
 * erases; the operation that would exceed either budget is the one the power
 * dies in. A program is cut byte-granular — the bytes before the cut land,
 * the rest stay as they were — and an erase is cut whole (it either completed
 * before the cut or never started). From then on the chip is unpowered: every
 * read, program and erase fails and changes nothing, so the code under test
 * unwinds through its own error paths and the flash image is frozen exactly as
 * the cut left it. power_cycle() restores power.
 *
 * This drives the REAL write path in the code's own order, so a torn record
 * here is whatever config_save() -> flash_region_append() actually leaves —
 * not the test's belief about the order. A real NOR tear is bit-granular;
 * the byte prefix is a representative subset of it. */
typedef struct {
    bool     armed;
    bool     dead;
    uint32_t program_bytes_left;
    uint32_t erases_left;
} power_cut_t;

static power_cut_t cut;

static void model_cut_power_after(uint32_t program_bytes, uint32_t erases)
{
    cut.armed = true;
    cut.dead = false;
    cut.program_bytes_left = program_bytes;
    cut.erases_left = erases;
}

static void model_restore_power(void)
{
    memset(&cut, 0, sizeof cut);
}

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
    model_restore_power();
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
    if (cut.dead) return -1;
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
    if (cut.dead) return -1;
    if (cut.armed) {
        if (cut.erases_left == 0) {
            cut.dead = true;                /* power dies before this erase */
            return -1;
        }
        cut.erases_left--;
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
    if (cut.dead) return -1;
    uint32_t landed = len;
    if (cut.armed && cut.program_bytes_left < len) {
        landed = cut.program_bytes_left;    /* power dies inside this program */
        cut.dead = true;
    }
    if (cut.armed) cut.program_bytes_left -= landed;
    model.programs++;
    model_touch(addr, len);
    for (uint32_t i = 0; i < landed; i++) {
        model.mem[addr + i] &= src[i];      /* NOR: bits only go 1 -> 0 */
    }
    return cut.dead ? -1 : 0;
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

/* A hostile table: the real geometry with the one region the settings writer
 * targets swapped for a read-only stand-in.
 *
 * Derived from flash_region_table[] rather than written out by hand. The
 * hand-written version indexed four entries by designated initializer and
 * passed a literal count of 4, so inserting a region mid-enum left a zeroed
 * hole at the inserted id and made the array longer than the count claimed:
 * the structural self-check then refused the table, and two tests that have
 * nothing to do with the new region failed for a reason neither of them is
 * about. FLASH_REGION_FWCACHE did exactly that. Copying the real table keeps
 * these fixtures honest whatever the enum grows next. */
static const flash_region_t *hostile_settings_readonly(void)
{
    static flash_region_t hostile[FLASH_REGION_COUNT];
    memcpy(hostile, flash_region_table, sizeof hostile);
    hostile[FLASH_REGION_SETTINGS].name = "factory";
    hostile[FLASH_REGION_SETTINGS].kind = FLASH_REGION_KIND_READONLY;
    return hostile;
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
    model_restore_power();
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

/* A DOCUMENTED FACT, NOT A VERDICT. The two places that define a "default"
 * scope state disagree on CH1 volts/div and on the timebase, although each
 * one's comment calls its value 2V/div and 50us/div:
 *   config_init_defaults()   ch1 vdiv index 3, timebase index 10
 *   scope_state_init()       ch1 vdiv index 8, timebase index 12
 * On the device scope_state_init() runs first and settings_store_init() then
 * applies the loaded config, or config.c's defaults, over it (main.c), so a
 * first boot comes up on 3 / 10 — and settings_store_apply()'s out-of-range
 * clamps fall back to 3 / 10 as well. Which pair is right is a question for
 * the scope front end, not for this suite. Pinned so that reconciling them is
 * a deliberate change: whoever does it updates this test. */
static void test_config_defaults_and_scope_state_init_disagree(void)
{
    device_config_t d;
    config_init_defaults(&d);
    scope_state_t s;
    memset(&s, 0, sizeof s);
    scope_state_init(&s);

    CHECK(d.scope_ch1_vdiv == 3u && d.scope_timebase == 10u,
          "config_init_defaults(): ch1 vdiv %u, timebase %u (pinned: 3, 10)",
          d.scope_ch1_vdiv, d.scope_timebase);
    CHECK(s.ch1.vdiv_idx == 8u && s.timebase_idx == 12u,
          "scope_state_init(): ch1 vdiv %u, timebase %u (pinned: 8, 12)",
          s.ch1.vdiv_idx, s.timebase_idx);

    /* What a user sees: a first boot on blank flash is config.c's pair. */
    fresh_device();
    power_cycle();
    settings_store_init();
    CHECK(scope_state_get()->ch1.vdiv_idx == d.scope_ch1_vdiv &&
          scope_state_get()->timebase_idx == d.scope_timebase,
          "a first boot came up on ch1 vdiv %u, timebase %u, not config_init_defaults()'s",
          scope_state_get()->ch1.vdiv_idx, scope_state_get()->timebase_idx);
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
    /* A table where the id config.c writes to (FLASH_REGION_SETTINGS) is
     * READ-ONLY and holds "factory calibration". */
    model_blank();
    /* irreplaceable content in a read-only region */
    memset(model.mem + settings_start(), 0x5A, 301);
    config_persist_stats_reset();
    model_counters_reset();
    CHECK(flash_regions_init_table(&model_backend, hostile_settings_readonly(),
                                   FLASH_REGION_COUNT) == FLASH_REGION_OK,
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
    CHECK(model.mem[settings_start()] == 0x5A &&
          model.mem[settings_start() + 300] == 0x5A,
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

    /*
     * A restored index is usable by whatever consumes it.
     *
     * This used to index vdiv_table / timebase_table, both of which were
     * removed on 2026-08-18 when volts/div and time/div became derived from
     * bench measurements rather than nominal strings. The intent is unchanged
     * — the consumers are. Both label calls must produce a NUL-terminated
     * string for any index the store can hand back, including the ranges and
     * codes that have no calibration (those legitimately return "--").
     */
    char lbl[16];
    memset(lbl, 0x7F, sizeof(lbl));
    scope_cal_range_label(1u, ss->ch1.vdiv_idx, lbl, sizeof(lbl));
    CHECK(memchr(lbl, '\0', sizeof(lbl)) != NULL,
          "restored ch1 vdiv index %u produced an unterminated label",
          ss->ch1.vdiv_idx);

    memset(lbl, 0x7F, sizeof(lbl));
    scope_cal_range_label(2u, ss->ch2.vdiv_idx, lbl, sizeof(lbl));
    CHECK(memchr(lbl, '\0', sizeof(lbl)) != NULL,
          "restored ch2 vdiv index %u produced an unterminated label",
          ss->ch2.vdiv_idx);

    memset(lbl, 0x7F, sizeof(lbl));
    scope_timebase_label(ss->timebase_idx, lbl, sizeof(lbl));
    CHECK(memchr(lbl, '\0', sizeof(lbl)) != NULL,
          "restored timebase code %u produced an unterminated label",
          ss->timebase_idx);
}

/* If the settings region cannot be written — not blank, flash failing, table
 * wrong — the store must not re-attempt the save on every press. Each attempt
 * scans the whole log in short SPI reads, so an unwritable device would spend
 * ~100 ms of SPI2 traffic per keypress achieving nothing. */
static void test_a_failing_write_is_not_retried_on_every_press(void)
{
    model_blank();
    config_persist_stats_reset();
    CHECK(flash_regions_init_table(&model_backend, hostile_settings_readonly(),
                                   FLASH_REGION_COUNT) == FLASH_REGION_OK,
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
 * Shared by sections 6-9
 * ═══════════════════════════════════════════════════════════════════ */

/* One record slot: header + config payload padded to 4 (64 B today). */
#define REC_SLOT  (REC_HDR_SIZE + ((uint32_t)(sizeof(device_config_t) + 3u) & ~3u))

/* Header layout (flash_regions.c, "Append log"): magic u16, len u16, crc u32.
 * A cut that lands at or after this many header bytes leaves a header whose
 * magic and length are both readable. */
#define HDR_MAGIC_AND_LEN_BYTES  4u

/* Distinctive ch1 volts/div codes, none of them the config default (3). */
#define MARK_A  1u      /* an older good record                       */
#define MARK_B  7u      /* the damaged / torn record: must never load */
#define MARK_C  4u      /* a newer good record                        */

/* Boot: RAM state gone, store re-initialised from whatever the chip holds. */
static void boot(void)
{
    power_cycle();
    settings_store_init();
}

static uint8_t default_ch1_vdiv(void)
{
    device_config_t d;
    config_init_defaults(&d);
    return d.scope_ch1_vdiv;
}

static bool slot_is_blank(uint32_t offset)
{
    const uint8_t *p = model.mem + settings_start() + offset;
    for (uint32_t i = 0; i < REC_SLOT; i++) {
        if (p[i] != 0xFFu) return false;
    }
    return true;
}

static uint32_t settings_capacity(void)
{
    return flash_region_get(FLASH_REGION_SETTINGS)->length / REC_SLOT;
}

/* A stamped, valid config told apart from the others by ch1 vdiv alone. */
static device_config_t marked_config(uint8_t ch1_vdiv)
{
    device_config_t c;
    config_init_defaults(&c);
    c.scope_ch1_vdiv = ch1_vdiv;
    c.checksum = config_compute_checksum(&c);
    return c;
}

static uint32_t forge_config(uint32_t offset, const device_config_t *c)
{
    uint8_t payload[sizeof *c];
    memcpy(payload, c, sizeof payload);
    return forge_record(offset, payload, (uint16_t)sizeof payload,
                        crc32_of(payload, sizeof payload));
}

/* ═══════════════════════════════════════════════════════════════════
 * 6. Damage anywhere in the log
 *
 * Section 2 damages only the NEWEST record. Here the damaged record B sits
 * either between two good ones ([A][B][C]) or at the end ([A][B]), for every
 * kind of damage the scanner distinguishes.
 *
 * THE INVARIANT, for every row: B is never applied, and the device boots with
 * CONFIG_LOAD_OK on a good record that is in the log (A, or C when there is
 * one). Two rows are held to more than that, because the firmware documents
 * more for them:
 *   - a CRC failure (payload bit flip) is SKIPPED by the length in the header
 *     (flash_regions.c, "Append log"; log_scan()), so mid-log the newest good
 *     record C must win, not A;
 *   - a record that passes CRC32 but fails the config's own magic / version /
 *     checksum: the region layer hands back the newest CRC-valid record and
 *     config.c validates only that one. Mid-log that is C, which loads. At the
 *     end of the log it is B itself, and config.h says what follows ("Anything
 *     that fails either check falls back to defaults"): defaults with
 *     CONFIG_LOAD_INVALID — accepted there, as is A, should the load path ever
 *     learn to fall back to an older record.
 *
 * ROWS THAT TODAY SIT ON A KNOWN DEFECT (#57), not on correct behaviour:
 * damaged magic, zero length, length over the max, plausible-but-wrong length.
 * log_scan() STOPS at each of them — an unparsable header, or a wrong length
 * that steps into the middle of the next record and finds no magic — so the
 * device boots on A, the newer good record C is unreachable, and (the worse
 * half of #57, section 9) every later save is refused. A is accepted for those
 * rows only because it is not B; a fix that reaches C stays green, and nothing
 * here should be read as saying A is the right answer mid-log.
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    DMG_PAYLOAD_BIT,        /* one payload bit flipped: CRC32 fails          */
    DMG_MAGIC,              /* header magic damaged                          */
    DMG_LEN_ZERO,           /* header length 0                               */
    DMG_LEN_OVER_MAX,       /* header length > FLASH_REGION_RECORD_MAX       */
    DMG_LEN_PLAUSIBLE,      /* header length sane, but not what was written  */
    DMG_CONFIG_CHECKSUM,    /* CRC32 good, config checksum bad               */
} damage_t;

static const struct {
    damage_t    kind;
    const char *name;
    bool        mid_needs_c;        /* [A][B][C]: documented to reach C        */
    bool        end_may_default;    /* [A][B]: documented fallback to defaults */
} DAMAGE_CASES[] = {
    { DMG_PAYLOAD_BIT,     "payload bit flip",           true,  false },
    { DMG_MAGIC,           "damaged magic",              false, false },  /* #57 */
    { DMG_LEN_ZERO,        "zero length",                false, false },  /* #57 */
    { DMG_LEN_OVER_MAX,    "length over the record max", false, false },  /* #57 */
    { DMG_LEN_PLAUSIBLE,   "plausible but wrong length", false, false },  /* #57 */
    { DMG_CONFIG_CHECKSUM, "CRC good, checksum bad",     true,  true  },
};

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

/* Write record B (ch1 vdiv = MARK_B) at `offset`, damaged as `kind`. Returns
 * the offset of the slot after it, as the undamaged record would occupy. */
static uint32_t forge_damaged(uint32_t offset, damage_t kind)
{
    device_config_t b = marked_config(MARK_B);
    if (kind == DMG_CONFIG_CHECKSUM) {
        b.meter_layout = 2;                 /* mutate AFTER checksumming */
    }
    uint32_t next = forge_config(offset, &b);
    uint8_t *rec = model.mem + settings_start() + offset;

    switch (kind) {
    case DMG_PAYLOAD_BIT:
        /* Not the vdiv byte: if B were applied anyway, MARK_B must show. */
        rec[REC_HDR_SIZE + offsetof(device_config_t, language)] ^= 0x04u;
        break;
    case DMG_MAGIC:
        rec[1] ^= 0x80u;
        break;
    case DMG_LEN_ZERO:
        put_le16(rec + 2, 0u);
        break;
    case DMG_LEN_OVER_MAX:
        put_le16(rec + 2, (uint16_t)(FLASH_REGION_RECORD_MAX + 1u));
        break;
    case DMG_LEN_PLAUSIBLE:
        put_le16(rec + 2, (uint16_t)(sizeof(device_config_t) - 4u));
        break;
    case DMG_CONFIG_CHECKSUM:
        break;
    }
    return next;
}

static void check_damage_case(unsigned row, bool mid_log)
{
    fresh_device();

    device_config_t a = marked_config(MARK_A);
    device_config_t c = marked_config(MARK_C);
    uint32_t off = forge_config(0, &a);
    off = forge_damaged(off, DAMAGE_CASES[row].kind);
    if (mid_log) {
        (void)forge_config(off, &c);
    }
    uint8_t *snap = snapshot();

    boot();

    const char *name  = DAMAGE_CASES[row].name;
    const char *where = mid_log ? "mid-log" : "end of log";
    const uint8_t got = scope_state_get()->ch1.vdiv_idx;
    const config_load_result_t res = settings_store_get_status()->load_result;
    const bool ok          = (res == CONFIG_LOAD_OK);
    const bool on_a        = ok && got == MARK_A;
    const bool on_c        = ok && mid_log && got == MARK_C;
    const bool on_defaults = res == CONFIG_LOAD_INVALID && got == default_ch1_vdiv();

    CHECK(got != MARK_B, "%s, %s: the damaged record was applied (vdiv %u)", name, where, got);
    if (mid_log && DAMAGE_CASES[row].mid_needs_c) {
        CHECK(on_c, "%s, %s: booted on vdiv %u (\"%s\"), expected the newer good record C "
              "(%u): the scan must step over B", name, where, got,
              config_load_result_name(res), MARK_C);
    } else if (!mid_log && DAMAGE_CASES[row].end_may_default) {
        CHECK(on_a || on_defaults, "%s, %s: booted on vdiv %u (\"%s\"), expected A (%u) "
              "or defaults (%u, \"%s\")", name, where, got, config_load_result_name(res),
              MARK_A, default_ch1_vdiv(), config_load_result_name(CONFIG_LOAD_INVALID));
    } else {
        CHECK(on_a || on_c, "%s, %s: booted on vdiv %u (\"%s\"), expected a good record "
              "in the log (A = %u, or C = %u mid-log) with \"%s\"", name, where, got,
              config_load_result_name(res), MARK_A, MARK_C,
              config_load_result_name(CONFIG_LOAD_OK));
    }
    CHECK(unchanged_since(snap), "%s, %s: booting modified the chip", name, where);
    free(snap);
}

static void test_damage_mid_log_is_never_applied(void)
{
    for (unsigned i = 0; i < sizeof DAMAGE_CASES / sizeof DAMAGE_CASES[0]; i++) {
        check_damage_case(i, true);
    }
}

static void test_damage_at_the_end_of_the_log_is_never_applied(void)
{
    for (unsigned i = 0; i < sizeof DAMAGE_CASES / sizeof DAMAGE_CASES[0]; i++) {
        check_damage_case(i, false);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * 7. Power cuts, injected into the real write path
 *
 * flash_region_append() programs the 8-byte header first and the payload
 * second, as two page programs. Every byte boundary of that sequence is a
 * place the power can die; these tests cut at each one in turn.
 * ═══════════════════════════════════════════════════════════════════ */

/* A fresh device that has saved MARK_A through the store, then a save of
 * MARK_B whose power dies after `budget` programmed bytes. Returns what the
 * cut save reported. The chip is left exactly as the cut left it. */
static bool save_cut_after(uint32_t budget)
{
    fresh_device();
    boot();
    scope_state_get()->ch1.vdiv_idx = MARK_A;
    CHECK(settings_store_flush(1000), "the good save before the cut failed");

    scope_state_get()->ch1.vdiv_idx = MARK_B;
    model_cut_power_after(budget, UINT32_MAX);
    return settings_store_flush(2000);
}

static void test_a_save_torn_at_any_byte_is_never_applied(void)
{
    for (uint32_t b = 1; b < REC_SLOT && !current_failed; b++) {
        bool wrote = save_cut_after(b);
        CHECK(!wrote, "cut after %u bytes: the torn save reported success", b);
        CHECK(!slot_is_blank(REC_SLOT),
              "cut after %u bytes: nothing landed — the cut did not tear the record", b);

        boot();
        uint8_t got = scope_state_get()->ch1.vdiv_idx;
        CHECK(got != MARK_B, "cut after %u bytes: the torn record was applied", b);
        CHECK(got == MARK_A, "cut after %u bytes: booted on vdiv %u, expected the previous "
              "good record (%u)", b, got, MARK_A);
        CHECK(settings_store_get_status()->load_result == CONFIG_LOAD_OK,
              "cut after %u bytes: load result \"%s\"", b,
              config_load_result_name(settings_store_get_status()->load_result));
    }
}

/* The guarantee config.h and flash_regions.c document: a torn record "of known
 * length whose CRC fails" is stepped over and the log keeps working. That holds
 * once the header's magic and length have landed — every cut from byte 4 to the
 * last payload byte. (Cuts inside bytes 0..3 are the known-defects build.) */
static void test_a_save_torn_after_its_length_does_not_stop_later_saves(void)
{
    for (uint32_t b = HDR_MAGIC_AND_LEN_BYTES; b < REC_SLOT && !current_failed; b++) {
        (void)save_cut_after(b);

        boot();
        scope_state_get()->ch1.vdiv_idx = MARK_C;
        CHECK(settings_store_flush(3000), "cut after %u bytes: the next save failed "
              "(last save status %d)", b, (int)config_persist_stats()->last_save_status);

        boot();
        CHECK(scope_state_get()->ch1.vdiv_idx == MARK_C,
              "cut after %u bytes: the save after the torn record did not survive (vdiv %u)",
              b, scope_state_get()->ch1.vdiv_idx);
    }
}

/* Fill the log with saves until it has no room for one more record. Each save
 * differs from the one before (else it is elided). Returns the last config
 * saved — the newest record in a full log. */
static device_config_t fill_log(void)
{
    device_config_t cfg;
    config_init_defaults(&cfg);
    for (uint32_t i = 0; i < settings_capacity(); i++) {
        cfg.scope_ch1_vdiv = (uint8_t)(i % VDIV_COUNT);
        cfg.scope_timebase = (uint8_t)(i % TIMEBASE_COUNT);
        cfg.checksum = config_compute_checksum(&cfg);   /* == the record's bytes */
        CHECK(config_save(&cfg), "fill save %u failed", i);
        if (current_failed) break;
    }
    uint32_t used = 0, free_bytes = 0;
    CHECK(flash_region_log_info(FLASH_REGION_SETTINGS, &used, &free_bytes, NULL) ==
          FLASH_REGION_OK, "log info failed");
    CHECK(free_bytes < REC_SLOT, "the log is not full after filling (%u bytes free)",
          free_bytes);
    CHECK(config_persist_stats()->compactions == 0, "filling the log compacted it early");
    return cfg;
}

/* The next save after a full log is the compaction: erase the region, re-append.
 * Power dies before the first sector erase. Nothing was lost — the full log is
 * still there — and the save after the reboot compacts properly. */
static void test_a_compaction_cut_before_its_erase_keeps_the_old_log(void)
{
    fresh_device();
    device_config_t last = fill_log();
    uint8_t *snap = snapshot();

    device_config_t next = marked_config(MARK_C);
    next.scope_timebase = 2;
    model_cut_power_after(UINT32_MAX, 0);
    CHECK(!config_save(&next), "the cut compaction reported success");
    CHECK(unchanged_since(snap), "a compaction cut before its first erase changed the chip");
    free(snap);

    power_cycle();
    device_config_t loaded;
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(memcmp(&loaded, &last, sizeof loaded) == 0,
          "the newest record of the full log did not load after the cut");

    uint32_t compactions0 = config_persist_stats()->compactions;
    CHECK(config_save(&next), "the save after the cut failed");
    CHECK(config_persist_stats()->compactions == compactions0 + 1,
          "the save after the cut did not compact");
    power_cycle();
    CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
    CHECK(loaded.scope_ch1_vdiv == MARK_C, "the save after the cut did not survive");
}

/* Power dies after the compaction erased its first sector. config.c says what
 * that costs: "A power cut between the reset and the re-append costs the
 * settings and the device comes up on defaults". Pinned: defaults, never a
 * half-erased record, and saving works again straight away. */
static void test_a_compaction_cut_mid_erase_boots_on_defaults_and_saves_again(void)
{
    fresh_device();
    (void)fill_log();

    device_config_t next = marked_config(MARK_B);
    model_cut_power_after(UINT32_MAX, 1);
    CHECK(!config_save(&next), "the cut compaction reported success");
    CHECK(slot_is_blank(0), "the first sector was not erased before the cut");

    boot();
    CHECK(settings_store_get_status()->load_result == CONFIG_LOAD_EMPTY,
          "expected an empty log after the cut, got \"%s\"",
          config_load_result_name(settings_store_get_status()->load_result));
    CHECK(scope_state_get()->ch1.vdiv_idx == default_ch1_vdiv(),
          "did not boot on defaults (vdiv %u)", scope_state_get()->ch1.vdiv_idx);

    scope_state_get()->ch1.vdiv_idx = MARK_C;
    CHECK(settings_store_flush(1000), "the save after the cut failed");
    boot();
    CHECK(scope_state_get()->ch1.vdiv_idx == MARK_C, "the save after the cut did not survive");
}

/* ═══════════════════════════════════════════════════════════════════
 * 8. Change -> power cycle -> verify, through the store, across compaction
 *
 * The S3 acceptance loop, on the host. Each cycle changes ONE setting through
 * live UI state, commits it the way the device does (alternately: a later
 * press after the settle window, and a flush at a mode change / power off),
 * power-cycles, and checks that the change came back and nothing else moved.
 * Every new value differs from both the current value and the boot default,
 * so with the writes stubbed out a cycle cannot pass by coincidence.
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    F_CH1_VDIV, F_CH2_VDIV, F_TIMEBASE, F_TRIG_LEVEL, F_TRIG_MODE,
    F_CH1_COUPLING, F_THEME, F_MATH_OP, F_METER_SUBMODE, F_METER_LAYOUT,
    F_COUNT
} field_t;

static int field_span(field_t f)
{
    switch (f) {
    case F_CH1_VDIV:      return VDIV_COUNT;
    case F_CH2_VDIV:      return VDIV_COUNT;
    case F_TIMEBASE:      return TIMEBASE_COUNT;
    case F_TRIG_LEVEL:    return 2 * 103 + 1;     /* code k <-> level k - 103 */
    case F_TRIG_MODE:     return TRIG_COUNT;
    case F_CH1_COUPLING:  return COUPLING_COUNT;
    case F_THEME:         return THEME_COUNT;
    case F_MATH_OP:       return MATH_COUNT;
    case F_METER_SUBMODE: return METER_SUBMODE_COUNT;
    case F_METER_LAYOUT:  return METER_LAYOUT_COUNT;
    case F_COUNT:         break;
    }
    return 0;
}

static int field_get(field_t f)
{
    const scope_state_t *ss = scope_state_get();
    switch (f) {
    case F_CH1_VDIV:      return ss->ch1.vdiv_idx;
    case F_CH2_VDIV:      return ss->ch2.vdiv_idx;
    case F_TIMEBASE:      return ss->timebase_idx;
    case F_TRIG_LEVEL:    return ss->trigger.level + 103;
    case F_TRIG_MODE:     return (int)ss->trigger.mode;
    case F_CH1_COUPLING:  return (int)ss->ch1.coupling;
    case F_THEME:         return (int)theme_get_id();
    case F_MATH_OP:       return math_op;
    case F_METER_SUBMODE: return meter_submode;
    case F_METER_LAYOUT:  return meter_layout;
    case F_COUNT:         break;
    }
    return -1;
}

static void field_set(field_t f, int v)
{
    scope_state_t *ss = scope_state_get();
    switch (f) {
    case F_CH1_VDIV:      ss->ch1.vdiv_idx = (uint8_t)v; break;
    case F_CH2_VDIV:      ss->ch2.vdiv_idx = (uint8_t)v; break;
    case F_TIMEBASE:      ss->timebase_idx = (uint8_t)v; break;
    case F_TRIG_LEVEL:    ss->trigger.level = (int16_t)(v - 103); break;
    case F_TRIG_MODE:     ss->trigger.mode = (trigger_mode_t)v; break;
    case F_CH1_COUPLING:  ss->ch1.coupling = (coupling_t)v; break;
    case F_THEME:         theme_set((theme_id_t)v); break;
    case F_MATH_OP:       math_op = (uint8_t)v; break;
    case F_METER_SUBMODE: meter_submode = (uint8_t)v; break;
    case F_METER_LAYOUT:  meter_layout = (uint8_t)v; break;
    case F_COUNT:         break;
    }
}

static void fields_read(int out[F_COUNT])
{
    for (int f = 0; f < F_COUNT; f++) out[f] = field_get((field_t)f);
}

/* A value for f that is neither its current nor its boot-default value. Every
 * field in the table has at least three values, so one always exists. */
static int next_value(field_t f, int current, int boot_default, uint32_t cycle)
{
    int span = field_span(f);
    for (int step = 1 + (int)(cycle % 5u); ; step++) {
        int v = (current + step) % span;
        if (v != current && v != boot_default) return v;
    }
}

typedef struct {
    uint32_t cycles;
    uint32_t commits_ok;        /* commits that reported a record written      */
    uint32_t survived;          /* cycles whose change came back after reboot  */
    uint32_t intact;            /* cycles where EVERY tracked setting came back */
    int32_t  first_bad;         /* first cycle not fully intact, or -1         */
    int32_t  first_compaction;  /* cycle whose commit compacted the log, or -1 */
    uint32_t compactions;
} cycle_result_t;

static cycle_result_t change_cycle_verify(uint32_t cycles)
{
    cycle_result_t r = { .cycles = cycles, .first_bad = -1, .first_compaction = -1 };
    int boot_default[F_COUNT], expect[F_COUNT], got[F_COUNT];

    boot();
    fields_read(boot_default);
    memcpy(expect, boot_default, sizeof expect);
    const uint32_t compactions0 = config_persist_stats()->compactions;
    uint32_t now = 1000u;

    for (uint32_t i = 0; i < cycles; i++) {
        field_t f = (field_t)(i % F_COUNT);
        int v = next_value(f, expect[f], boot_default[f], i);
        field_set(f, v);
        expect[f] = v;

        uint32_t before = config_persist_stats()->compactions;
        bool wrote;
        if (i & 1u) {
            wrote = settings_store_flush(now);              /* MENU / power off */
        } else {
            settings_store_note_change(now);                /* the change...    */
            settings_store_note_change(now + SETTINGS_STORE_SETTLE_MS);  /* a later press */
            wrote = settings_store_service(now + SETTINGS_STORE_SETTLE_MS);
        }
        now += 10u * SETTINGS_STORE_SETTLE_MS;
        if (wrote) r.commits_ok++;
        if (config_persist_stats()->compactions != before && r.first_compaction < 0) {
            r.first_compaction = (int32_t)i;
        }

        boot();
        fields_read(got);
        if (got[f] == v) r.survived++;
        if (memcmp(got, expect, sizeof got) == 0) {
            r.intact++;
        } else if (r.first_bad < 0) {
            r.first_bad = (int32_t)i;
        }
        /* Judge each cycle on its own change: carry on from what came back. */
        memcpy(expect, got, sizeof expect);
    }
    r.compactions = config_persist_stats()->compactions - compactions0;
    return r;
}

static void test_changes_survive_power_cycles_across_compaction(void)
{
    fresh_device();

    /* One record per cycle; the log holds settings_capacity() of them, so
     * this many cycles compacts it once and keeps going well past that. */
    const uint32_t cycles = settings_capacity() + 64u;
    cycle_result_t r = change_cycle_verify(cycles);

    CHECK(r.commits_ok == cycles, "%u of %u commits wrote a record", r.commits_ok, cycles);
    CHECK(r.survived == cycles, "%u of %u changes survived a power cycle", r.survived, cycles);
    CHECK(r.intact == cycles, "%u of %u cycles came back intact; first bad cycle %d",
          r.intact, cycles, (int)r.first_bad);
    CHECK(r.compactions >= 1, "the loop never compacted the log — it did not test the wrap");
    CHECK(r.first_compaction >= 0 && (uint32_t)r.first_compaction + 32u < cycles,
          "compaction at cycle %d leaves too few verified cycles after it",
          (int)r.first_compaction);
}

/* ── Negative control (built with SETTINGS_PERSIST_WRITES=0) ─────────── */

static void test_with_writes_stubbed_no_change_survives(void)
{
    fresh_device();
    uint8_t *snap = snapshot();

    const uint32_t cycles = 2u * F_COUNT;
    cycle_result_t r = change_cycle_verify(cycles);

    CHECK(r.commits_ok == 0, "%u commits claimed a write with writes compiled out",
          r.commits_ok);
    CHECK(r.survived == 0, "%u of %u changes survived with writes compiled out",
          r.survived, cycles);
    CHECK(r.intact == 0, "%u cycles came back intact with writes compiled out", r.intact);
    CHECK(unchanged_since(snap), "the chip changed with writes compiled out");

    /* config.h: a refusal we chose is not a failure we suffered. */
    const config_persist_stats_t *cs = config_persist_stats();
    CHECK(!cs->writes_enabled, "stats claim writes are enabled in a stubbed build");
    CHECK(cs->saves_disabled == cycles, "expected %u refused saves, counted %u",
          cycles, cs->saves_disabled);
    CHECK(cs->saves_failed == 0, "%u chosen refusals were counted as failures",
          cs->saves_failed);
    free(snap);
}

/* ═══════════════════════════════════════════════════════════════════
 * 9. KNOWN DEFECTS — built only with CONFIG_PERSIST_KNOWN_DEFECTS=1
 *
 * Each test asserts the behaviour the firmware's own comments promise, for a
 * case where it does not deliver it today. They are EXPECTED TO FAIL until
 * the firmware is fixed; do not "fix" them by weakening the assertion.
 * ═══════════════════════════════════════════════════════════════════ */

/* DEFECT: a power cut inside the 8-byte header program, before the magic and
 * length have both landed, stops every later save — permanently.
 *
 * config.h promises "a save interrupted by a power cut leaves a record of
 * known length whose CRC fails; the scanner steps over it". That is true only
 * once bytes 0..3 (magic, length) are down. With the cut earlier, log_scan()
 * (flash_regions.c, the magic / length checks) stops at the unparsable header
 * and reports next_offset = that slot; flash_region_append()'s blank-slot check
 * finds the bytes that did land and refuses with NEEDS_ERASE; config_save()
 * compacts only on FULL and deliberately not on NEEDS_ERASE — and FULL is now
 * unreachable, because the scan never gets past the torn header. Loads still
 * return the last good record, so nothing looks wrong at boot: every setting
 * change from then on is silently lost. */
static void test_a_save_torn_inside_its_magic_or_length_does_not_stop_later_saves(void)
{
    for (uint32_t b = 1; b < HDR_MAGIC_AND_LEN_BYTES; b++) {
        (void)save_cut_after(b);

        boot();
        CHECK(scope_state_get()->ch1.vdiv_idx == MARK_A,
              "cut after %u bytes: did not boot on the last good record", b);

        bool any_saved = false;
        for (uint8_t k = 0; k < 3; k++) {          /* three separate changes */
            scope_state_get()->ch1.vdiv_idx = (uint8_t)(MARK_C + k);
            any_saved |= settings_store_flush(3000u + k);
        }
        CHECK(any_saved, "cut after %u bytes: no later save succeeded (last save status "
              "%d = %s)", b, (int)config_persist_stats()->last_save_status,
              flash_region_strerror((flash_region_status_t)
                                    config_persist_stats()->last_save_status));

        boot();
        CHECK(scope_state_get()->ch1.vdiv_idx == MARK_C + 2u,
              "cut after %u bytes: the newest change did not survive a power cycle "
              "(booted on vdiv %u, the record from before the cut)",
              b, scope_state_get()->ch1.vdiv_idx);
    }
}

/* DEFECT: the same wedge from damage instead of a cut. One header anywhere in
 * the log that no longer parses (a flipped bit in the magic) hides every record
 * after it AND stops every later save, for the same reason as above. */
static void test_a_damaged_header_mid_log_does_not_stop_later_saves(void)
{
    fresh_device();
    device_config_t a = marked_config(MARK_A);
    device_config_t c = marked_config(MARK_C);
    uint32_t off = forge_config(0, &a);
    off = forge_damaged(off, DMG_MAGIC);
    (void)forge_config(off, &c);

    boot();
    scope_state_get()->ch1.vdiv_idx = 9;
    CHECK(settings_store_flush(1000), "a save after a damaged header failed (last save "
          "status %d = %s)", (int)config_persist_stats()->last_save_status,
          flash_region_strerror((flash_region_status_t)
                                config_persist_stats()->last_save_status));
    boot();
    CHECK(scope_state_get()->ch1.vdiv_idx == 9,
          "the save after a damaged header did not survive (booted on vdiv %u)",
          scope_state_get()->ch1.vdiv_idx);
}

/* DEFECT: a compaction cut after it erased some, not all, of the region later
 * resurrects the PRE-compaction settings as the newest record.
 *
 * The region is erased sector by sector from offset 0 (flash_regions.c
 * erase_checked()). Cut after sector 0: the device boots on defaults (the
 * documented cost) and new records fill sector 0 from offset 0. Once they fill
 * it, log_scan() walks straight on into sector 1, which still holds the old,
 * CRC-valid records, laid out on the same 64-byte grid — and the newest of
 * THOSE wins. The device boots on settings from before the compaction, over
 * 64 newer saves. (The next save then finds the log full and compacts, which
 * hides the symptom again until the next boot-before-save.) */
static void test_a_compaction_cut_mid_erase_never_resurrects_old_settings(void)
{
    fresh_device();
    device_config_t old_newest = fill_log();

    device_config_t next = marked_config(MARK_B);
    model_cut_power_after(UINT32_MAX, 1);
    CHECK(!config_save(&next), "the cut compaction reported success");
    power_cycle();

    const uint32_t per_sector = FLASH_REGION_SECTOR_SIZE / REC_SLOT;
    device_config_t cfg;
    config_init_defaults(&cfg);
    for (uint32_t i = 0; i < per_sector + 2u && !current_failed; i++) {
        cfg.scope_trigger_level = (int16_t)(i % 100u) - 50;    /* each save differs */
        cfg.scope_ch1_vdiv = (uint8_t)(i % 2u ? MARK_A : MARK_C);
        CHECK(config_save(&cfg), "save %u after the cut failed", i);

        power_cycle();
        device_config_t loaded;
        CHECK_LOAD(config_load_or_defaults(&loaded), CONFIG_LOAD_OK);
        CHECK(loaded.scope_trigger_level == cfg.scope_trigger_level &&
              loaded.scope_ch1_vdiv == cfg.scope_ch1_vdiv,
              "after %u saves since the cut, the boot loaded %s (vdiv %u, level %d) instead "
              "of the newest save (vdiv %u, level %d)", i + 1u,
              memcmp(&loaded, &old_newest, sizeof loaded) == 0
                  ? "the PRE-COMPACTION newest record" : "some other record",
              loaded.scope_ch1_vdiv, loaded.scope_trigger_level,
              cfg.scope_ch1_vdiv, cfg.scope_trigger_level);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════ */

#ifndef CONFIG_PERSIST_KNOWN_DEFECTS
#define CONFIG_PERSIST_KNOWN_DEFECTS 0
#endif
#if CONFIG_PERSIST_KNOWN_DEFECTS && !SETTINGS_PERSIST_WRITES
#error "the known-defects build needs the write path: build it with SETTINGS_PERSIST_WRITES=1"
#endif

enum { MODE_MAIN, MODE_NEGATIVE_CONTROL, MODE_KNOWN_DEFECTS };
#if CONFIG_PERSIST_KNOWN_DEFECTS
#define BUILD_MODE  MODE_KNOWN_DEFECTS
#elif !SETTINGS_PERSIST_WRITES
#define BUILD_MODE  MODE_NEGATIVE_CONTROL
#else
#define BUILD_MODE  MODE_MAIN
#endif

static void print_known_defects_banner(void)
{
    printf("\n");
    printf("=========================================================================\n");
    printf(" settings persistence -- KNOWN DEFECTS (power cuts)\n");
    printf("\n");
    printf(" THIS BUILD IS EXPECTED TO FAIL until the firmware is fixed.\n");
    printf(" A red result here is a DOCUMENTED, KNOWN DEFECT, not a broken build.\n");
    printf(" It is deliberately excluded from `make test-config-persist`.\n");
    printf(" Each test asserts the correct behaviour; see section 9 of\n");
    printf(" tests/test_config_persist.c for the mechanism behind each one.\n");
    printf("=========================================================================\n");
    printf("\n");
}

int main(void)
{
    if (BUILD_MODE == MODE_KNOWN_DEFECTS) {
        print_known_defects_banner();
    }
    printf("=== settings persistence (%s) ===\n",
           BUILD_MODE == MODE_MAIN             ? "SETTINGS_PERSIST_WRITES=1" :
           BUILD_MODE == MODE_NEGATIVE_CONTROL ? "negative control: SETTINGS_PERSIST_WRITES=0" :
                                                 "KNOWN DEFECTS: expected to fail");
    printf("(device_config_t is %u bytes; record slot %u)\n",
           (unsigned)sizeof(device_config_t), (unsigned)REC_SLOT);

    if (BUILD_MODE == MODE_MAIN) {
        run("first boot on blank flash yields defaults", test_first_boot_on_blank_flash_yields_defaults);
        run("config defaults and scope_state_init disagree (pinned fact)",
            test_config_defaults_and_scope_state_init_disagree);
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

        run("damage mid-log is never applied", test_damage_mid_log_is_never_applied);
        run("damage at the end of the log is never applied",
            test_damage_at_the_end_of_the_log_is_never_applied);
        run("a save torn at any byte is never applied", test_a_save_torn_at_any_byte_is_never_applied);
        run("a save torn after its length does not stop later saves",
            test_a_save_torn_after_its_length_does_not_stop_later_saves);
        run("a compaction cut before its erase keeps the old log",
            test_a_compaction_cut_before_its_erase_keeps_the_old_log);
        run("a compaction cut mid-erase boots on defaults and saves again",
            test_a_compaction_cut_mid_erase_boots_on_defaults_and_saves_again);
        run("changes survive power cycles across compaction",
            test_changes_survive_power_cycles_across_compaction);
    }

    if (BUILD_MODE == MODE_NEGATIVE_CONTROL) {
        run("with writes stubbed, no change survives a power cycle",
            test_with_writes_stubbed_no_change_survives);
        run_expect_red("with writes stubbed, \"changes survive power cycles across compaction\" goes red",
                       test_changes_survive_power_cycles_across_compaction);
    }

    if (BUILD_MODE == MODE_KNOWN_DEFECTS) {
        run("a save torn inside its magic or length does not stop later saves",
            test_a_save_torn_inside_its_magic_or_length_does_not_stop_later_saves);
        run("a damaged header mid-log does not stop later saves",
            test_a_damaged_header_mid_log_does_not_stop_later_saves);
        run("a compaction cut mid-erase never resurrects old settings",
            test_a_compaction_cut_mid_erase_never_resurrects_old_settings);
    }

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0) {
        printf("%d tests OK\n", tests_run);
    }
    free(model.mem);
    return tests_failed == 0 ? 0 : 1;
}
