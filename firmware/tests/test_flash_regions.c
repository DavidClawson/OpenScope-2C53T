/*
 * Host tests for the W25Q region layer (src/drivers/flash_regions.c).
 *
 * Build: gcc -o tests/test_flash_regions tests/test_flash_regions.c \
 *            src/drivers/flash_regions.c -Isrc/drivers
 * Run:   ./tests/test_flash_regions
 *
 * These are not mocks. The backend below is a NOR flash model with real
 * semantics — erase sets 0xFF, program can only clear bits, page-program
 * cannot cross a 256 B page — and it counts and logs every erase and program.
 * That is what makes the negative tests meaningful: "the write was refused" is
 * asserted as ZERO erases, ZERO programs and a byte-identical chip image, not
 * merely as a non-zero return code. A guard that returned the right error while
 * still writing would fail here.
 *
 * The suite is also the target of a mutation check in
 * scripts/test_flash_regions.py, which disables each guard in turn and requires
 * these tests to go red. A test that has never been seen to fail is not
 * evidence.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "flash_regions.h"

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

#define CHECK_ST(expr, expect) do {                             \
    flash_region_status_t st_ = (expr);                         \
    if (st_ != (expect)) {                                      \
        printf("  FAIL (line %d): %s -> %s, expected %s\n",      \
               __LINE__, #expr, flash_region_strerror(st_),     \
               flash_region_strerror(expect));                  \
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
 * NOR flash model
 * ═══════════════════════════════════════════════════════════════════ */

#define MODEL_SIZE  FLASH_REGION_CHIP_SIZE

typedef struct {
    uint8_t *mem;
    uint32_t erases;
    uint32_t programs;
    uint32_t bytes_programmed;
    uint32_t reads;
    /* Fault injection: when set, program() reports success but changes
     * nothing — the exact "write silently did nothing" failure this project
     * has already been bitten by. */
    bool     drop_programs;
    /* Lowest and highest address ever modified, so a test can assert that a
     * refused operation touched nothing anywhere. */
    uint32_t touched_lo;
    uint32_t touched_hi;
} nor_model_t;

static nor_model_t model;

static void model_init(void)
{
    if (model.mem == NULL) {
        model.mem = malloc(MODEL_SIZE);
        if (model.mem == NULL) {
            printf("FATAL: cannot allocate %u byte flash model\n", (unsigned)MODEL_SIZE);
            exit(2);
        }
    }
    memset(model.mem, 0xFF, MODEL_SIZE);
    model.erases = model.programs = model.bytes_programmed = model.reads = 0;
    model.drop_programs = false;
    model.touched_lo = UINT32_MAX;
    model.touched_hi = 0;
}

static void model_touch_reset(void)
{
    model.touched_lo = UINT32_MAX;
    model.touched_hi = 0;
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
    if (addr % FLASH_REGION_SECTOR_SIZE) {
        printf("  MODEL VIOLATION: unaligned erase 0x%X\n", addr);
        exit(3);
    }
    if (addr >= MODEL_SIZE) {
        printf("  MODEL VIOLATION: erase out of range 0x%X\n", addr);
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
    model.bytes_programmed += len;
    if (model.drop_programs) {
        return 0;                       /* claims success, writes nothing */
    }
    model_touch(addr, len);
    for (uint32_t i = 0; i < len; i++) {
        model.mem[addr + i] &= src[i];  /* NOR: bits only go 1 -> 0 */
    }
    return 0;
}

static const flash_region_backend_t model_backend = {
    .read = model_read, .erase_sector = model_erase, .program = model_program, .ctx = NULL,
};

/* Snapshot / compare helpers so "nothing changed" is checked, not assumed. */
static uint8_t *snapshot(void)
{
    uint8_t *s = malloc(MODEL_SIZE);
    if (!s) { printf("FATAL: snapshot alloc\n"); exit(2); }
    memcpy(s, model.mem, MODEL_SIZE);
    return s;
}

static bool unchanged_since(uint8_t *snap)
{
    return memcmp(snap, model.mem, MODEL_SIZE) == 0;
}

static void fresh(void)
{
    model_init();
    flash_regions_stats_reset();
    if (flash_regions_init(&model_backend) != FLASH_REGION_OK) {
        printf("FATAL: flash_regions_init failed on the shipped table\n");
        exit(2);
    }
}

/* Convenience: the shipped table's regions. */
static const flash_region_t *R(flash_region_id_t id) { return flash_region_get(id); }

/* ═══════════════════════════════════════════════════════════════════
 * 1. Table integrity
 * ═══════════════════════════════════════════════════════════════════ */

static void test_shipped_table_is_valid(void)
{
    CHECK_ST(flash_regions_check_table(flash_region_table, FLASH_REGION_COUNT),
             FLASH_REGION_OK);

    for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
        const flash_region_t *r = &flash_region_table[i];
        CHECK(r->start % FLASH_REGION_SECTOR_SIZE == 0, "region %s start unaligned", r->name);
        CHECK(r->length % FLASH_REGION_SECTOR_SIZE == 0, "region %s length unaligned", r->name);
        CHECK(r->start + r->length <= FLASH_REGION_CHIP_SIZE, "region %s past chip end", r->name);
    }
}

static void test_stock_volumes_are_read_only(void)
{
    fresh();
    /* Both stock FAT12 volumes, per w25q128_flash_map_2026-06-13.md. */
    CHECK(R(FLASH_REGION_SYSVOL)->kind == FLASH_REGION_KIND_READONLY, "sysvol must be RO");
    CHECK(R(FLASH_REGION_USERVOL)->kind == FLASH_REGION_KIND_READONLY, "uservol must be RO");
    CHECK(R(FLASH_REGION_SYSVOL)->start == 0x000000u, "sysvol must start at 0");
    CHECK(R(FLASH_REGION_SYSVOL)->length == 0x200000u, "sysvol must cover volume 3:");
    CHECK(R(FLASH_REGION_USERVOL)->start == 0x200000u, "uservol must start at 0x200000");
}

static void test_bad_tables_are_rejected(void)
{
    /* Overlap: an RW region straddling a read-only one is the single most
     * dangerous editing mistake, so it must not be representable. */
    static const flash_region_t overlapping[] = {
        { "ro", 0x000000u, 0x002000u, FLASH_REGION_KIND_READONLY },
        { "rw", 0x001000u, 0x002000u, FLASH_REGION_KIND_RW },
    };
    CHECK_ST(flash_regions_check_table(overlapping, 2), FLASH_REGION_ERR_TABLE);

    static const flash_region_t misaligned[] = {
        { "rw", 0x000800u, 0x001000u, FLASH_REGION_KIND_RW },
    };
    CHECK_ST(flash_regions_check_table(misaligned, 1), FLASH_REGION_ERR_TABLE);

    static const flash_region_t stub_len[] = {
        { "rw", 0x000000u, 0x000800u, FLASH_REGION_KIND_RW },
    };
    CHECK_ST(flash_regions_check_table(stub_len, 1), FLASH_REGION_ERR_TABLE);

    static const flash_region_t past_end[] = {
        { "rw", 0xFFF000u, 0x002000u, FLASH_REGION_KIND_RW },
    };
    CHECK_ST(flash_regions_check_table(past_end, 1), FLASH_REGION_ERR_TABLE);

    static const flash_region_t descending[] = {
        { "b", 0x002000u, 0x001000u, FLASH_REGION_KIND_RW },
        { "a", 0x000000u, 0x001000u, FLASH_REGION_KIND_RW },
    };
    CHECK_ST(flash_regions_check_table(descending, 2), FLASH_REGION_ERR_TABLE);

    CHECK_ST(flash_regions_check_table(NULL, 1), FLASH_REGION_ERR_TABLE);
    CHECK_ST(flash_regions_check_table(overlapping, 0), FLASH_REGION_ERR_TABLE);
}

static void test_bad_table_fails_closed(void)
{
    /* A rejected table must leave the layer dead, not half-armed: every write
     * path must refuse, rather than falling back to "no table means no
     * restrictions". */
    static const flash_region_t overlapping[] = {
        { "ro", 0x000000u, 0x002000u, FLASH_REGION_KIND_READONLY },
        { "rw", 0x001000u, 0x002000u, FLASH_REGION_KIND_RW },
    };
    model_init();
    uint8_t *snap = snapshot();

    CHECK_ST(flash_regions_init_table(&model_backend, overlapping, 2), FLASH_REGION_ERR_TABLE);
    CHECK(!flash_regions_ready(), "layer must not be ready after a bad table");

    uint8_t data[4] = { 1, 2, 3, 4 };
    CHECK_ST(flash_regions_write_abs(0x001000u, data, sizeof data), FLASH_REGION_ERR_NOT_INIT);
    CHECK_ST(flash_regions_erase_abs(0x001000u, 0x1000u), FLASH_REGION_ERR_NOT_INIT);
    CHECK_ST(flash_region_write(FLASH_REGION_SCRATCH, 0, data, sizeof data),
             FLASH_REGION_ERR_NOT_INIT);
    CHECK_ST(flash_region_erase(FLASH_REGION_SCRATCH, 0, 0x1000u), FLASH_REGION_ERR_NOT_INIT);
    CHECK_ST(flash_region_append(FLASH_REGION_SETTINGS, data, sizeof data),
             FLASH_REGION_ERR_NOT_INIT);
    CHECK(model.erases == 0 && model.programs == 0, "a dead layer must not touch flash");
    CHECK(unchanged_since(snap), "flash changed while the layer was uninitialised");
    free(snap);

    fresh();   /* restore a good table for later tests */
}

/* ═══════════════════════════════════════════════════════════════════
 * 2. Read-only enforcement — the reason this layer exists
 * ═══════════════════════════════════════════════════════════════════ */

static void test_write_to_readonly_region_is_refused(void)
{
    fresh();
    /* Put plausible factory content in the System file volume so a stray write
     * would be visible as data loss rather than as a change to blank flash. */
    memset(model.mem + 0x007000u, 0x5A, 301);   /* stands in for cal_ch1.bin */
    uint8_t *snap = snapshot();

    uint8_t data[301];
    memset(data, 0x00, sizeof data);

    /* by region id */
    CHECK_ST(flash_region_write(FLASH_REGION_SYSVOL, 0x7000u, data, sizeof data),
             FLASH_REGION_ERR_READ_ONLY);
    /* by absolute address, straight at the cal blob */
    CHECK_ST(flash_regions_write_abs(0x007000u, data, sizeof data),
             FLASH_REGION_ERR_READ_ONLY);
    /* first and last byte of each read-only region */
    CHECK_ST(flash_regions_write_abs(0x000000u, data, 1), FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_write_abs(0x1FFFFFu, data, 1), FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_write_abs(0x200000u, data, 1), FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_write_abs(0xEFFFFFu, data, 1), FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_write_abs(0xFFF000u, data, 1), FLASH_REGION_ERR_READ_ONLY);

    CHECK(model.programs == 0, "%u programs issued against read-only flash", model.programs);
    CHECK(model.erases == 0, "%u erases issued against read-only flash", model.erases);
    CHECK(unchanged_since(snap), "read-only flash content changed");
    CHECK(flash_regions_stats()->writes_refused >= 7, "refusals not counted");
    free(snap);
}

static void test_erase_of_readonly_region_is_refused(void)
{
    fresh();
    memset(model.mem + 0x000000u, 0xA5, FLASH_REGION_SECTOR_SIZE);
    uint8_t *snap = snapshot();

    CHECK_ST(flash_region_erase(FLASH_REGION_SYSVOL, 0, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_region_reset(FLASH_REGION_SYSVOL), FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_erase_abs(0x000000u, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_regions_erase_abs(0x1FF000u, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_region_reset(FLASH_REGION_USERVOL), FLASH_REGION_ERR_READ_ONLY);

    CHECK(model.erases == 0, "%u erases reached read-only flash", model.erases);
    CHECK(unchanged_since(snap), "read-only sector was erased");
    free(snap);
}

static void test_reads_from_readonly_regions_still_work(void)
{
    fresh();
    memset(model.mem + 0x007000u, 0x42, 301);

    uint8_t buf[301];
    CHECK_ST(flash_region_read(FLASH_REGION_SYSVOL, 0x7000u, buf, sizeof buf), FLASH_REGION_OK);
    CHECK(buf[0] == 0x42 && buf[300] == 0x42, "read-only region did not read back");

    /* Reads are harmless and must never be blocked; only writes are policed. */
    CHECK(model.programs == 0 && model.erases == 0, "a read modified flash");
}

/* ═══════════════════════════════════════════════════════════════════
 * 3. Bounds — fail closed, never partial
 * ═══════════════════════════════════════════════════════════════════ */

static void test_write_crossing_into_readonly_neighbour_fails_closed(void)
{
    fresh();
    const flash_region_t *scratch = R(FLASH_REGION_SCRATCH);
    uint8_t *snap = snapshot();

    /* Start 8 bytes before the end of scratch and run 64 bytes: the tail lands
     * in the read-only tail sector. Nothing at all may be written — not even
     * the 8 bytes that are legal. */
    uint8_t data[64];
    memset(data, 0x11, sizeof data);
    uint32_t addr = scratch->start + scratch->length - 8u;

    CHECK_ST(flash_regions_write_abs(addr, data, sizeof data), FLASH_REGION_ERR_READ_ONLY);
    CHECK(model.programs == 0, "partial write leaked %u programs", model.programs);
    CHECK(unchanged_since(snap), "a boundary-crossing write modified flash");

    /* Same shape via the region-relative API: past the end of the region. */
    CHECK_ST(flash_region_write(FLASH_REGION_SCRATCH, scratch->length - 8u, data, sizeof data),
             FLASH_REGION_ERR_BOUNDS);
    CHECK(model.programs == 0, "region-relative overrun still programmed");
    CHECK(unchanged_since(snap), "region-relative overrun modified flash");
    free(snap);
}

static void test_write_spanning_two_writable_regions_is_refused(void)
{
    fresh();
    const flash_region_t *modules = R(FLASH_REGION_MODULES);
    uint8_t *snap = snapshot();

    /* modules ends where scratch begins; both are writable. Crossing is still
     * refused — the boundary is the thing being protected, not the permission. */
    CHECK(modules->start + modules->length == R(FLASH_REGION_SCRATCH)->start,
          "test assumes modules abuts scratch");

    uint8_t data[16];
    memset(data, 0x33, sizeof data);
    CHECK_ST(flash_regions_write_abs(modules->start + modules->length - 8u, data, sizeof data),
             FLASH_REGION_ERR_BOUNDS);
    CHECK(model.programs == 0, "cross-region write programmed flash");
    CHECK(unchanged_since(snap), "cross-region write modified flash");
    free(snap);
}

static void test_unmapped_addresses_are_denied_by_default(void)
{
    /* Default deny: build a table with a hole and confirm the hole is not
     * writable. The shipped table happens to be contiguous, so this property
     * needs its own table to be tested at all. */
    static const flash_region_t holed[] = {
        { "rw_lo", 0x000000u, 0x001000u, FLASH_REGION_KIND_RW },
        /* 0x001000-0x001FFF unmapped */
        { "rw_hi", 0x002000u, 0x001000u, FLASH_REGION_KIND_RW },
    };
    model_init();
    flash_regions_stats_reset();
    CHECK_ST(flash_regions_init_table(&model_backend, holed, 2), FLASH_REGION_OK);
    uint8_t *snap = snapshot();

    uint8_t data[4] = { 9, 9, 9, 9 };
    CHECK_ST(flash_regions_write_abs(0x001000u, data, sizeof data), FLASH_REGION_ERR_UNMAPPED);
    CHECK_ST(flash_regions_write_abs(0x001FFCu, data, sizeof data), FLASH_REGION_ERR_UNMAPPED);
    CHECK_ST(flash_regions_erase_abs(0x001000u, 0x1000u), FLASH_REGION_ERR_UNMAPPED);
    /* A range starting in a mapped region but running into the hole. */
    CHECK_ST(flash_regions_write_abs(0x000FFEu, data, sizeof data), FLASH_REGION_ERR_BOUNDS);
    CHECK(model.programs == 0 && model.erases == 0, "unmapped space was written");
    CHECK(unchanged_since(snap), "unmapped space changed");
    free(snap);

    /* The region's own last 4 bytes (0xFFC..0xFFF) are legal: the boundary is
     * checked exactly, not conservatively, so the guard is not just "refuse
     * everything near the edge". */
    CHECK_ST(flash_regions_write_abs(0x000FFCu, data, sizeof data), FLASH_REGION_OK);
    CHECK(model.mem[0x000FFF] == 9, "the in-bounds edge write did not land");
    CHECK(model.mem[0x001000] == 0xFF, "the in-bounds edge write leaked into the hole");

    fresh();
}

static void test_erase_alignment_is_enforced(void)
{
    fresh();
    uint8_t *snap = snapshot();

    CHECK_ST(flash_region_erase(FLASH_REGION_SCRATCH, 0x800u, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_ALIGN);
    CHECK_ST(flash_region_erase(FLASH_REGION_SCRATCH, 0, 0x800u), FLASH_REGION_ERR_ALIGN);
    CHECK_ST(flash_regions_erase_abs(R(FLASH_REGION_SCRATCH)->start + 1u, 0x1000u),
             FLASH_REGION_ERR_ALIGN);
    /* Past the end of the region, aligned but too long. */
    CHECK_ST(flash_region_erase(FLASH_REGION_SCRATCH, 0,
                                R(FLASH_REGION_SCRATCH)->length + FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_BOUNDS);

    CHECK(model.erases == 0, "misaligned erase reached the chip");
    CHECK(unchanged_since(snap), "misaligned erase modified flash");
    free(snap);
}

static void test_zero_length_and_null_are_rejected(void)
{
    fresh();
    uint8_t data[4] = { 0 };
    CHECK_ST(flash_region_write(FLASH_REGION_SCRATCH, 0, NULL, 4), FLASH_REGION_ERR_BAD_ARG);
    CHECK_ST(flash_region_write(FLASH_REGION_SCRATCH, 0, data, 0), FLASH_REGION_ERR_BAD_ARG);
    CHECK_ST(flash_regions_write_abs(R(FLASH_REGION_SCRATCH)->start, NULL, 4),
             FLASH_REGION_ERR_BAD_ARG);
    CHECK_ST(flash_region_write((flash_region_id_t)FLASH_REGION_COUNT, 0, data, 4),
             FLASH_REGION_ERR_BAD_ARG);
    CHECK_ST(flash_region_read((flash_region_id_t)99, 0, data, 4), FLASH_REGION_ERR_BAD_ARG);
    CHECK(model.programs == 0 && model.erases == 0, "bad arguments reached the chip");
}

/* Exhaustive-ish sweep: for a large set of deterministic pseudo-random ranges,
 * any range that touches a read-only region must be refused and must leave the
 * chip byte-identical. This is the property statement of requirement 1; the
 * targeted tests above are the readable examples of it. */
static void test_random_ranges_never_touch_readonly(void)
{
    fresh();
    /* Programmed content everywhere read-only, so any stray program or erase
     * shows up as a difference. */
    for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
        const flash_region_t *r = &flash_region_table[i];
        if (r->kind == FLASH_REGION_KIND_READONLY) {
            memset(model.mem + r->start, 0x5A, r->length);
        }
    }
    uint8_t *snap = snapshot();

    uint8_t data[256];
    memset(data, 0x00, sizeof data);

    uint32_t rng = 0x13579BDFu;
    uint32_t refused_ro = 0, allowed = 0;
    for (int iter = 0; iter < 20000; iter++) {
        rng = rng * 1664525u + 1013904223u;
        uint32_t addr = rng % FLASH_REGION_CHIP_SIZE;
        rng = rng * 1664525u + 1013904223u;
        uint32_t len = 1u + (rng % sizeof data);

        /* Independent oracle: does [addr, addr+len) intersect any RO region? */
        bool touches_ro = false;
        if (len <= FLASH_REGION_CHIP_SIZE - addr) {
            for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
                const flash_region_t *r = &flash_region_table[i];
                if (r->kind != FLASH_REGION_KIND_READONLY) continue;
                if (addr < r->start + r->length && r->start < addr + len) {
                    touches_ro = true;
                    break;
                }
            }
        }

        flash_region_status_t st = flash_regions_write_abs(addr, data, len);
        if (touches_ro) {
            if (st != FLASH_REGION_ERR_READ_ONLY) {
                CHECK(false, "addr 0x%X len %u touches read-only but returned %s",
                      addr, len, flash_region_strerror(st));
                break;
            }
            refused_ro++;
        } else if (st == FLASH_REGION_OK) {
            allowed++;
        }
    }
    CHECK(refused_ro > 1000, "sweep only exercised %u read-only refusals", refused_ro);
    CHECK(allowed > 0, "sweep never exercised an allowed write");

    /* Every read-only byte must be exactly as it started. */
    for (uint32_t i = 0; i < FLASH_REGION_COUNT; i++) {
        const flash_region_t *r = &flash_region_table[i];
        if (r->kind != FLASH_REGION_KIND_READONLY) continue;
        CHECK(memcmp(snap + r->start, model.mem + r->start, r->length) == 0,
              "read-only region %s was modified during the sweep", r->name);
    }
    free(snap);
}

/* ═══════════════════════════════════════════════════════════════════
 * 4. Write behaviour in writable regions
 * ═══════════════════════════════════════════════════════════════════ */

static void test_write_and_readback(void)
{
    fresh();
    uint8_t data[600];
    for (uint32_t i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 7u);

    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0x40u, data, sizeof data), FLASH_REGION_OK);
    CHECK(model.programs >= 3, "600 bytes at offset 0x40 should span 3 pages, saw %u",
          model.programs);

    uint8_t back[600];
    CHECK_ST(flash_region_read(FLASH_REGION_MODULES, 0x40u, back, sizeof back), FLASH_REGION_OK);
    CHECK(memcmp(back, data, sizeof data) == 0, "readback mismatch");
    CHECK(flash_regions_stats()->writes_programmed == 1, "write not counted");
    CHECK(model.erases == 0, "a plain write into blank flash must not erase");
}

static void test_identical_write_is_elided(void)
{
    fresh();
    uint8_t data[128];
    memset(data, 0xC3, sizeof data);

    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, data, sizeof data), FLASH_REGION_OK);
    uint32_t programs_after_first = model.programs;
    uint32_t erases_after_first = model.erases;

    for (int i = 0; i < 5; i++) {
        CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, data, sizeof data), FLASH_REGION_OK);
    }
    CHECK(model.programs == programs_after_first, "repeat write programmed again (%u -> %u)",
          programs_after_first, model.programs);
    CHECK(model.erases == erases_after_first, "repeat write erased");
    CHECK(flash_regions_stats()->writes_elided == 5, "elisions not counted: %u",
          flash_regions_stats()->writes_elided);
}

static void test_write_needing_bit_set_is_refused_not_erased(void)
{
    fresh();
    uint8_t zeros[64];
    memset(zeros, 0x00, sizeof zeros);
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, zeros, sizeof zeros), FLASH_REGION_OK);

    uint8_t *snap = snapshot();
    uint32_t erases_before = model.erases;

    /* 0x00 -> 0xFF needs bits to go 0 -> 1, which only an erase can do. The
     * layer must refuse rather than quietly erasing 4 KB. */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof ones);
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, ones, sizeof ones),
             FLASH_REGION_ERR_NEEDS_ERASE);
    CHECK(model.erases == erases_before, "refused write still erased %u sectors",
          model.erases - erases_before);
    CHECK(unchanged_since(snap), "refused write modified flash");

    /* Partial overlap: only one byte needs a set bit — still all-or-nothing. */
    uint8_t mixed[64];
    memset(mixed, 0x00, sizeof mixed);
    mixed[63] = 0x01;
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, mixed, sizeof mixed),
             FLASH_REGION_ERR_NEEDS_ERASE);
    CHECK(unchanged_since(snap), "partially-incompatible write modified flash");
    free(snap);

    /* After an explicit erase the same write goes through. */
    CHECK_ST(flash_region_erase(FLASH_REGION_MODULES, 0, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_OK);
    CHECK(model.erases == erases_before + 1, "explicit erase did not happen");
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, ones, sizeof ones), FLASH_REGION_OK);
}

static void test_erase_of_blank_sector_is_elided(void)
{
    fresh();
    CHECK_ST(flash_region_erase(FLASH_REGION_MODULES, 0, 4u * FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_OK);
    CHECK(model.erases == 0, "blank sectors were erased anyway (%u)", model.erases);
    CHECK(flash_regions_stats()->erases_elided == 4, "elided erases not counted");

    uint8_t data[4] = { 1, 2, 3, 4 };
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0x2000u, data, sizeof data),
             FLASH_REGION_OK);
    CHECK_ST(flash_region_erase(FLASH_REGION_MODULES, 0, 4u * FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_OK);
    CHECK(model.erases == 1, "only the dirty sector should be erased, saw %u", model.erases);
}

static void test_erase_stays_inside_its_region(void)
{
    fresh();
    const flash_region_t *cal = R(FLASH_REGION_USER_CAL);
    /* Mark the neighbouring region so an over-erase is detectable. */
    uint8_t marker[16];
    memset(marker, 0x5C, sizeof marker);
    CHECK_ST(flash_region_write(FLASH_REGION_SETTINGS, 0, marker, sizeof marker), FLASH_REGION_OK);
    /* Dirty the cal region so the reset has real work to do (blank sectors are
     * elided, and an elided erase would prove nothing). */
    uint8_t junk[8];
    memset(junk, 0x00, sizeof junk);
    CHECK_ST(flash_region_write(FLASH_REGION_USER_CAL, cal->length - 8u, junk, sizeof junk),
             FLASH_REGION_OK);

    model_touch_reset();
    CHECK_ST(flash_region_reset(FLASH_REGION_USER_CAL), FLASH_REGION_OK);
    CHECK(model.erases > 0, "reset did not erase anything");

    uint8_t back[16];
    CHECK_ST(flash_region_read(FLASH_REGION_SETTINGS, 0, back, sizeof back), FLASH_REGION_OK);
    CHECK(memcmp(back, marker, sizeof marker) == 0, "reset spilled into the next region");
    CHECK(model.touched_hi <= cal->start + cal->length,
          "erase touched 0x%X, past the end of usercal", model.touched_hi);
}

static void test_verify_catches_a_silent_write_failure(void)
{
    fresh();
    uint8_t data[32];
    memset(data, 0x77, sizeof data);

    model.drop_programs = true;     /* backend claims success, writes nothing */
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, data, sizeof data),
             FLASH_REGION_ERR_VERIFY);
    CHECK(flash_regions_stats()->verify_failures == 1, "verify failure not counted");
    CHECK(flash_regions_stats()->writes_programmed == 0, "failed write counted as success");

    model.drop_programs = false;
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, data, sizeof data), FLASH_REGION_OK);
}

/* A backend whose reads fail: the layer must surface FLASH_REGION_ERR_IO rather
 * than treating unreadable flash as blank and programming over it. */
static int failing_read(void *ctx, uint32_t addr, void *buf, uint32_t len)
{
    (void)ctx; (void)addr; (void)buf; (void)len;
    return -1;
}

static void test_io_errors_propagate(void)
{
    model_init();
    flash_regions_stats_reset();
    static flash_region_backend_t broken;
    broken = model_backend;
    broken.read = failing_read;
    CHECK_ST(flash_regions_init(&broken), FLASH_REGION_OK);

    uint8_t data[16];
    memset(data, 0x5E, sizeof data);
    CHECK_ST(flash_region_write(FLASH_REGION_MODULES, 0, data, sizeof data), FLASH_REGION_ERR_IO);
    CHECK_ST(flash_region_erase(FLASH_REGION_MODULES, 0, FLASH_REGION_SECTOR_SIZE),
             FLASH_REGION_ERR_IO);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, data, sizeof data), FLASH_REGION_ERR_IO);
    CHECK(model.programs == 0 && model.erases == 0, "unreadable flash was written anyway");
    CHECK(flash_regions_stats()->io_failures >= 3, "io failures not counted");

    fresh();
}

/* ═══════════════════════════════════════════════════════════════════
 * 5. Append log
 * ═══════════════════════════════════════════════════════════════════ */

static void test_append_and_read_latest(void)
{
    fresh();
    const char *v1 = "cal-v1";
    const char *v2 = "cal-version-two";
    const char *v3 = "3";

    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, v1, (uint32_t)strlen(v1)),
             FLASH_REGION_OK);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, v2, (uint32_t)strlen(v2)),
             FLASH_REGION_OK);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, v3, (uint32_t)strlen(v3)),
             FLASH_REGION_OK);

    uint8_t buf[64];
    uint32_t len = 0;
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_OK);
    CHECK(len == strlen(v3) && memcmp(buf, v3, len) == 0, "latest record is wrong");

    uint32_t used = 0, freeb = 0, records = 0;
    CHECK_ST(flash_region_log_info(FLASH_REGION_USER_CAL, &used, &freeb, &records),
             FLASH_REGION_OK);
    CHECK(records == 3, "expected 3 valid records, saw %u", records);
    CHECK(used + freeb == R(FLASH_REGION_USER_CAL)->length, "log accounting is wrong");
    CHECK(model.erases == 0, "appending must never erase");
}

/* Records are not page-aligned, so a payload can straddle a 256 B page boundary.
 * The model aborts the whole run on a page-crossing program, so this test fails
 * loudly rather than subtly if the splitting is ever dropped. */
static void test_append_record_crossing_a_page_boundary(void)
{
    fresh();
    uint8_t a[200], b[200];
    memset(a, 0x1A, sizeof a);
    memset(b, 0x2B, sizeof b);

    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, a, sizeof a), FLASH_REGION_OK);
    /* second record: header at 208, payload 216..415 — spans pages 0 and 1 */
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, b, sizeof b), FLASH_REGION_OK);

    uint8_t back[200];
    uint32_t len = 0;
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, back, sizeof back, &len),
             FLASH_REGION_OK);
    CHECK(len == sizeof b && memcmp(back, b, len) == 0, "page-straddling record is wrong");
}

static void test_append_is_elided_when_unchanged(void)
{
    fresh();
    uint8_t payload[40];
    memset(payload, 0x21, sizeof payload);

    CHECK_ST(flash_region_append(FLASH_REGION_SETTINGS, payload, sizeof payload), FLASH_REGION_OK);
    uint32_t programs = model.programs;

    /* The hot-value case: saving the same settings 50 times must cost nothing.
     * This is what keeps a frequently-written value off the erase path. */
    for (int i = 0; i < 50; i++) {
        CHECK_ST(flash_region_append(FLASH_REGION_SETTINGS, payload, sizeof payload),
                 FLASH_REGION_OK);
    }
    CHECK(model.programs == programs, "unchanged appends programmed flash (%u -> %u)",
          programs, model.programs);
    CHECK(model.erases == 0, "unchanged appends erased flash");

    uint32_t records = 0;
    CHECK_ST(flash_region_log_info(FLASH_REGION_SETTINGS, NULL, NULL, &records), FLASH_REGION_OK);
    CHECK(records == 1, "expected 1 record after 51 identical appends, saw %u", records);

    /* A changed value does get written. */
    payload[0] = 0x22;
    CHECK_ST(flash_region_append(FLASH_REGION_SETTINGS, payload, sizeof payload), FLASH_REGION_OK);
    CHECK(model.programs > programs, "a changed append did not write");
}

static void test_append_survives_a_torn_record(void)
{
    fresh();
    const char *a = "first";
    const char *b = "second";
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, a, (uint32_t)strlen(a)), FLASH_REGION_OK);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, b, (uint32_t)strlen(b)), FLASH_REGION_OK);

    /* Corrupt the newest record's payload the way a power cut mid-program
     * would: bits cleared, header intact. */
    uint32_t base = R(FLASH_REGION_USER_CAL)->start;
    uint32_t second_off = 8u + ((strlen(a) + 3u) & ~3u);
    model.mem[base + second_off + 8u] = 0x00;

    uint8_t buf[64];
    uint32_t len = 0;
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_OK);
    CHECK(len == strlen(a) && memcmp(buf, a, len) == 0,
          "a torn newest record must fall back to the previous good one");

    /* And the log keeps working: a new append lands after the damaged record. */
    const char *c = "third";
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, c, (uint32_t)strlen(c)), FLASH_REGION_OK);
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_OK);
    CHECK(len == strlen(c) && memcmp(buf, c, len) == 0, "append after a torn record failed");
}

static void test_append_full_refuses_and_preserves(void)
{
    fresh();
    uint8_t payload[248];              /* 256 bytes per record with the header */
    const uint32_t region_len = R(FLASH_REGION_USER_CAL)->length;
    const uint32_t capacity = region_len / 256u;

    uint32_t written = 0;
    for (uint32_t i = 0; i < capacity + 4u; i++) {
        memset(payload, (uint8_t)(i + 1u), sizeof payload);
        flash_region_status_t st = flash_region_append(FLASH_REGION_USER_CAL, payload,
                                                       sizeof payload);
        if (st == FLASH_REGION_OK) {
            written++;
            continue;
        }
        CHECK_ST(st, FLASH_REGION_ERR_FULL);
        break;
    }
    CHECK(written == capacity, "expected %u records to fit, %u did", capacity, written);

    /* Full means full: further appends stay refused and change nothing. */
    uint8_t *snap = snapshot();
    memset(payload, 0xEE, sizeof payload);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, payload, sizeof payload),
             FLASH_REGION_ERR_FULL);
    CHECK(unchanged_since(snap), "a full-log append modified flash");
    free(snap);

    /* The newest record is still readable — a full log is not a lost log. */
    uint8_t buf[248];
    uint32_t len = 0;
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_OK);
    CHECK(len == sizeof payload && buf[0] == (uint8_t)written, "newest record lost when full");

    /* Explicit reset is the only way out, and then it works again. */
    CHECK_ST(flash_region_reset(FLASH_REGION_USER_CAL), FLASH_REGION_OK);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, payload, sizeof payload),
             FLASH_REGION_OK);
}

static void test_append_rejects_non_append_regions(void)
{
    fresh();
    uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t *snap = snapshot();

    CHECK_ST(flash_region_append(FLASH_REGION_SYSVOL, payload, sizeof payload),
             FLASH_REGION_ERR_READ_ONLY);
    CHECK_ST(flash_region_append(FLASH_REGION_MODULES, payload, sizeof payload),
             FLASH_REGION_ERR_BAD_ARG);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, payload, FLASH_REGION_RECORD_MAX + 1u),
             FLASH_REGION_ERR_BAD_ARG);
    CHECK(unchanged_since(snap), "a refused append modified flash");
    free(snap);
}

static void test_read_latest_on_empty_log(void)
{
    fresh();
    uint8_t buf[16];
    uint32_t len = 0;
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_ERR_NOT_FOUND);

    /* A record that does not fit the caller's buffer is an error, not a
     * truncated read. */
    uint8_t big[64];
    memset(big, 0x31, sizeof big);
    CHECK_ST(flash_region_append(FLASH_REGION_USER_CAL, big, sizeof big), FLASH_REGION_OK);
    CHECK_ST(flash_region_read_latest(FLASH_REGION_USER_CAL, buf, sizeof buf, &len),
             FLASH_REGION_ERR_BOUNDS);
}

/* ═══════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== W25Q region layer ===\n");

    run("shipped region table is valid", test_shipped_table_is_valid);
    run("stock volumes are read-only", test_stock_volumes_are_read_only);
    run("malformed tables are rejected", test_bad_tables_are_rejected);
    run("a rejected table fails closed", test_bad_table_fails_closed);

    run("write to a read-only region is refused", test_write_to_readonly_region_is_refused);
    run("erase of a read-only region is refused", test_erase_of_readonly_region_is_refused);
    run("reads from read-only regions still work", test_reads_from_readonly_regions_still_work);

    run("write crossing into read-only fails closed",
        test_write_crossing_into_readonly_neighbour_fails_closed);
    run("write spanning two writable regions is refused",
        test_write_spanning_two_writable_regions_is_refused);
    run("unmapped addresses are denied by default", test_unmapped_addresses_are_denied_by_default);
    run("erase alignment is enforced", test_erase_alignment_is_enforced);
    run("zero length and null are rejected", test_zero_length_and_null_are_rejected);
    run("20k random ranges never touch read-only", test_random_ranges_never_touch_readonly);

    run("write and readback", test_write_and_readback);
    run("identical write is elided", test_identical_write_is_elided);
    run("write needing a bit set is refused, not erased",
        test_write_needing_bit_set_is_refused_not_erased);
    run("erase of a blank sector is elided", test_erase_of_blank_sector_is_elided);
    run("erase stays inside its region", test_erase_stays_inside_its_region);
    run("verify catches a silent write failure", test_verify_catches_a_silent_write_failure);
    run("io errors propagate", test_io_errors_propagate);

    run("append and read latest", test_append_and_read_latest);
    run("append record crossing a page boundary", test_append_record_crossing_a_page_boundary);
    run("append is elided when unchanged", test_append_is_elided_when_unchanged);
    run("append survives a torn record", test_append_survives_a_torn_record);
    run("a full log refuses and preserves", test_append_full_refuses_and_preserves);
    run("append rejects non-append regions", test_append_rejects_non_append_regions);
    run("read_latest on an empty log", test_read_latest_on_empty_log);

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0) {
        printf("%d tests OK\n", tests_run);
    }
    free(model.mem);
    return tests_failed == 0 ? 0 : 1;
}
