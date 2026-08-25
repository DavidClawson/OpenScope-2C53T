/*
 * Host tests for the factory-cal backup CORE (src/util/cal_backup_core.c).
 *
 * The core is deliberately hardware-free: record build/parse/validate, page
 * classification, and the one decision that must never be wrong —
 * "may auto-restore overwrite the live page?". Everything the device does on
 * top (reading 0x08006000, W25Q region I/O, MCU-flash erase/program) lives in
 * cal_backup.c and is validated on the bench, not here.
 *
 * Build: gcc -o tests/test_cal_backup tests/test_cal_backup.c \
 *            src/util/cal_backup_core.c -Isrc/util
 * Run:   ./tests/test_cal_backup
 *
 * Also the target of a mutation check in scripts/test_cal_backup.py: each guard
 * in the core is deleted in turn and these tests are required to go red.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cal_backup_core.h"

static int tests_run = 0;
static int tests_failed = 0;
static int current_failed = 0;

#define CHECK(cond, ...) do {                       \
    if (!(cond)) {                                  \
        printf("  FAIL (line %d): ", __LINE__);     \
        printf(__VA_ARGS__);                        \
        printf("\n");                               \
        current_failed++;                           \
    }                                               \
} while (0)

#define RUN(fn) do {                                \
    current_failed = 0;                             \
    printf("%s\n", #fn);                            \
    fn();                                           \
    tests_run++;                                    \
    if (current_failed) { tests_failed++; printf("  -> FAILED\n"); }   \
    else { printf("  ok\n"); }                      \
} while (0)

/* A plausible 4 KB page: mostly 0xFF with a small run of "cal" values where the
 * factory table is known to sit (page offset 0x026 onward). */
static void make_page(uint8_t *page)
{
    memset(page, 0xFF, CAL_BACKUP_PAYLOAD_LEN);
    for (uint32_t i = 0; i < 240u; i++) {
        page[0x026u + i] = (uint8_t)(0x40u + (i & 0x3Fu));
    }
}

/* ── CRC32 matches the known IEEE-802.3 test vector ─────────────────── */
static void test_crc32_known_vector(void)
{
    /* CRC32("123456789") == 0xCBF43926 (reflected, standard). */
    uint32_t c = cal_backup_crc32("123456789", 9);
    CHECK(c == 0xCBF43926u, "crc32(\"123456789\")=0x%08X, expected 0xCBF43926", c);
}

/* ── build then parse round-trips and validates ─────────────────────── */
static void test_build_parse_roundtrip(void)
{
    static uint8_t page[CAL_BACKUP_PAYLOAD_LEN];
    static uint8_t rec[CAL_BACKUP_RECORD_LEN];
    make_page(page);

    uint32_t n = cal_backup_build_record(page, (uint16_t)CAL_BACKUP_PAYLOAD_LEN,
                                         (uint16_t)CAL_BACKUP_VERSION,
                                         CAL_BACKUP_SRC_ADDR, rec, sizeof rec);
    CHECK(n == CAL_BACKUP_RECORD_LEN, "record len %u, expected %u",
          n, (unsigned)CAL_BACKUP_RECORD_LEN);

    cal_backup_header_t h;
    const uint8_t *payload = NULL;
    cal_rec_status_t st = cal_backup_parse(rec, n, &h, &payload);
    CHECK(st == CAL_REC_OK, "parse -> %s", cal_rec_status_str(st));
    CHECK(h.magic == CAL_BACKUP_MAGIC, "magic wrong");
    CHECK(h.version == CAL_BACKUP_VERSION, "version wrong");
    CHECK(h.payload_len == CAL_BACKUP_PAYLOAD_LEN, "payload_len wrong");
    CHECK(h.src_addr == CAL_BACKUP_SRC_ADDR, "src_addr wrong");
    CHECK(payload != NULL && memcmp(payload, page, CAL_BACKUP_PAYLOAD_LEN) == 0,
          "payload not byte-identical");
}

/* ── build refuses bad arguments ────────────────────────────────────── */
static void test_build_bad_args(void)
{
    static uint8_t page[CAL_BACKUP_PAYLOAD_LEN];
    static uint8_t rec[CAL_BACKUP_RECORD_LEN];
    make_page(page);

    CHECK(cal_backup_build_record(NULL, 16, 1, 0, rec, sizeof rec) == 0, "null payload accepted");
    CHECK(cal_backup_build_record(page, 0, 1, 0, rec, sizeof rec) == 0, "zero len accepted");
    CHECK(cal_backup_build_record(page, (uint16_t)CAL_BACKUP_PAYLOAD_LEN, 1, 0, rec, 8) == 0,
          "undersized out accepted");
}

/* ── parse rejects every form of corruption ─────────────────────────── */
static void test_parse_rejects_corruption(void)
{
    static uint8_t page[CAL_BACKUP_PAYLOAD_LEN];
    static uint8_t rec[CAL_BACKUP_RECORD_LEN];
    make_page(page);
    uint32_t n = cal_backup_build_record(page, (uint16_t)CAL_BACKUP_PAYLOAD_LEN,
                                         (uint16_t)CAL_BACKUP_VERSION,
                                         CAL_BACKUP_SRC_ADDR, rec, sizeof rec);

    /* truncated */
    CHECK(cal_backup_parse(rec, CAL_BACKUP_HEADER_LEN - 1u, NULL, NULL) == CAL_REC_ERR_TRUNCATED,
          "short buffer not rejected");

    /* blank region (all 0xFF) — the common "no backup yet" case */
    static uint8_t blank[CAL_BACKUP_RECORD_LEN];
    memset(blank, 0xFF, sizeof blank);
    CHECK(cal_backup_parse(blank, sizeof blank, NULL, NULL) == CAL_REC_ERR_MAGIC,
          "blank region not reported as bad magic");

    /* bad magic */
    static uint8_t r2[CAL_BACKUP_RECORD_LEN];
    memcpy(r2, rec, n);
    r2[0] ^= 0xFFu;
    CHECK(cal_backup_parse(r2, n, NULL, NULL) == CAL_REC_ERR_MAGIC, "magic corruption passed");

    /* header corruption (flip a byte inside the header, past magic) — caught by
     * header_crc before version/len are trusted */
    memcpy(r2, rec, n);
    r2[8] ^= 0x01u;   /* src_addr low byte */
    CHECK(cal_backup_parse(r2, n, NULL, NULL) == CAL_REC_ERR_HEADER_CRC, "header corruption passed");

    /* payload corruption — header still valid, payload_crc catches it */
    memcpy(r2, rec, n);
    r2[CAL_BACKUP_HEADER_LEN + 100u] ^= 0x80u;
    CHECK(cal_backup_parse(r2, n, NULL, NULL) == CAL_REC_ERR_PAYLOAD_CRC, "payload corruption passed");
}

/* ── an unknown future version is rejected, not misread ─────────────── */
static void test_parse_rejects_future_version(void)
{
    static uint8_t page[CAL_BACKUP_PAYLOAD_LEN];
    static uint8_t rec[CAL_BACKUP_RECORD_LEN];
    make_page(page);
    uint32_t n = cal_backup_build_record(page, (uint16_t)CAL_BACKUP_PAYLOAD_LEN,
                                         (uint16_t)(CAL_BACKUP_VERSION + 7u),
                                         CAL_BACKUP_SRC_ADDR, rec, sizeof rec);
    CHECK(cal_backup_parse(rec, n, NULL, NULL) == CAL_REC_ERR_VERSION,
          "future version accepted");
}

/* ── page classification ────────────────────────────────────────────── */
static void test_classify_page(void)
{
    static uint8_t page[CAL_BACKUP_PAYLOAD_LEN];

    memset(page, 0xFF, sizeof page);
    CHECK(cal_backup_classify_page(page, sizeof page) == CAL_PAGE_BLANK, "all-FF not BLANK");

    memset(page, 0x00, sizeof page);
    CHECK(cal_backup_classify_page(page, sizeof page) == CAL_PAGE_ZEROED, "all-00 not ZEROED");

    make_page(page);
    CHECK(cal_backup_classify_page(page, sizeof page) == CAL_PAGE_PROGRAMMED, "real page not PROGRAMMED");

    /* a single non-FF byte is enough to be PROGRAMMED */
    memset(page, 0xFF, sizeof page);
    page[2000] = 0x00;
    CHECK(cal_backup_classify_page(page, sizeof page) == CAL_PAGE_PROGRAMMED, "one byte not detected");
}

/* ── the load-bearing decision: never clobber a programmed page ─────── */
static void test_auto_restore_decision(void)
{
    /* No backup: never. */
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_BLANK, false) == false, "auto-restore with no backup");
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_ZEROED, false) == false, "auto-restore with no backup");
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_PROGRAMMED, false) == false, "auto-restore with no backup");

    /* Valid backup + empty page: yes. */
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_BLANK, true) == true, "should restore over blank");
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_ZEROED, true) == true, "should restore over zeroed");

    /* Valid backup + programmed page: NEVER — this is the whole safety story. */
    CHECK(cal_backup_should_auto_restore(CAL_PAGE_PROGRAMMED, true) == false,
          "auto-restore clobbered a programmed page");
}

int main(void)
{
    RUN(test_crc32_known_vector);
    RUN(test_build_parse_roundtrip);
    RUN(test_build_bad_args);
    RUN(test_parse_rejects_corruption);
    RUN(test_parse_rejects_future_version);
    RUN(test_classify_page);
    RUN(test_auto_restore_decision);

    printf("\n%d/%d test groups passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
