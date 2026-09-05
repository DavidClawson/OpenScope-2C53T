/*
 * Host tests for the CDC firmware loader's state machine.
 *
 * The RAM installer cannot run on a host and is compiled out
 * (FW_LOADER_HOST_TEST); what is tested is everything that decides WHETHER
 * it may run — the gates that stand between a byte stream and an erased
 * app slot:
 *
 *   1. Size gate: zero, odd, tiny and over-staging sizes are refused
 *      before a single byte is consumed.
 *   2. The happy path: a valid image streamed in awkward chunk sizes
 *      lands byte-exact in the staging region and reaches STAGED.
 *   3. CRC gate: one flipped bit anywhere = ERROR, never STAGED.
 *   4. Vector gate: right size and CRC but a non-app vector table (SP not
 *      in SRAM / PC out of slot / even PC) = ERROR.
 *   5. Timeout: a transfer that goes silent returns the shell to life as
 *      ERROR, and a fresh fwload recovers.
 *   6. apply() refuses anything not STAGED.
 *   7. No-buffer gate: until the shell task lends a scratch area, begin()
 *      and install_slot() refuse with ERR_NO_BUFFER and consume nothing.
 */

#include "../src/drivers/fw_loader.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Simulated 2 MB fwcache region (chip 0xD00000..0xF00000). */
uint8_t fw_loader_test_w25q[0x200000];

static int failures = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                       \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
            failures++;                                                       \
        }                                                                     \
    } while (0)

/* Same CRC the loader and the host script use. */
static uint32_t crc32_ref(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

/* A minimal valid image: SRAM-shaped SP, odd (thumb) in-slot PC, noise body. */
static void make_image(uint8_t *img, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        img[i] = (uint8_t)(i * 2654435761u >> 24);
    uint32_t sp = 0x20030000u, pc = 0x08007311u;
    memcpy(img, &sp, 4);
    memcpy(img + 4, &pc, 4);
}

static void feed_in_chunks(const uint8_t *img, uint32_t size, uint16_t chunk)
{
    for (uint32_t off = 0; off < size; ) {
        uint16_t n = (uint16_t)((size - off) < chunk ? (size - off) : chunk);
        fw_loader_feed(img + off, n);
        off += n;
    }
}

static void test_size_gate(void)
{
    CHECK(!fw_loader_begin(0, 0x1234, 1), "size 0 must be refused");
    CHECK(!fw_loader_begin(8193, 0x1234, 1), "odd size must be refused");
    CHECK(!fw_loader_begin(4096, 0x1234, 1), "size < 8192 must be refused");
    CHECK(!fw_loader_begin(757762, 0x1234, 1), "size > app ceiling must be refused");
    CHECK(!fw_loader_begin(20000, 0x1234, 2), "slot 2 must be refused");
    CHECK(fw_loader_error() == FW_LOADER_ERR_SIZE, "err must be SIZE");
    CHECK(!fw_loader_active(), "a refused begin must not activate RX routing");
}

static void test_happy_path(void)
{
    enum { SZ = 20000 };
    static uint8_t img[SZ];
    make_image(img, SZ);
    uint32_t crc = crc32_ref(img, SZ);

    CHECK(fw_loader_begin(SZ, crc, 1), "begin must accept a sane image");
    CHECK(fw_loader_active(), "must be receiving");
    /* 63 is deliberately not a divisor of anything: pages fill unevenly. */
    feed_in_chunks(img, SZ, 63);
    CHECK(fw_loader_state() == FW_LOADER_STAGED,
          "valid stream must stage (state=%d err=%d)",
          fw_loader_state(), fw_loader_error());
    CHECK(!fw_loader_active(), "routing must return to the shell when done");
    CHECK(memcmp(&fw_loader_test_w25q[0x00E01000u - 0x00D00000u], img, SZ) == 0,
          "staged bytes must be byte-exact in slot B");
    CHECK(fw_loader_slot_size(1) == SZ, "slot B manifest must carry the size");
    CHECK(fw_loader_slot_crc(1) == crc, "slot B manifest must carry the crc");
    CHECK(fw_loader_slot_size(0) == 0, "slot A must stay empty");
    CHECK(fw_loader_apply(), "apply must accept a STAGED image (host no-op)");
    CHECK(fw_loader_install_slot(1), "fwswap of a valid cached slot must pass");
    CHECK(!fw_loader_install_slot(0), "fwswap of an empty slot must refuse");
}

static void test_crc_gate(void)
{
    enum { SZ = 16384 };
    static uint8_t img[SZ];
    make_image(img, SZ);
    uint32_t crc = crc32_ref(img, SZ);
    img[9000] ^= 0x40;   /* corrupt AFTER computing the announced CRC */

    CHECK(fw_loader_begin(SZ, crc, 0), "begin ok");
    feed_in_chunks(img, SZ, 512);
    CHECK(fw_loader_state() == FW_LOADER_ERROR &&
          fw_loader_error() == FW_LOADER_ERR_CRC,
          "one flipped bit must land in ERROR/CRC (state=%d err=%d)",
          fw_loader_state(), fw_loader_error());
    CHECK(!fw_loader_apply(), "apply must refuse a CRC-failed stage");
    CHECK(fw_loader_slot_size(0) == 0,
          "a CRC-failed intake must leave slot A without a manifest");
}

static void test_vector_gate(void)
{
    enum { SZ = 16384 };
    static uint8_t img[SZ];

    /* SP not in SRAM */
    make_image(img, SZ);
    uint32_t bad_sp = 0x08007000u;
    memcpy(img, &bad_sp, 4);
    CHECK(fw_loader_begin(SZ, crc32_ref(img, SZ), 1), "begin ok");
    feed_in_chunks(img, SZ, 2048);
    CHECK(fw_loader_error() == FW_LOADER_ERR_VECTOR, "flash-shaped SP must fail");

    /* PC below the app slot */
    make_image(img, SZ);
    uint32_t bad_pc = 0x08000101u;
    memcpy(img + 4, &bad_pc, 4);
    CHECK(fw_loader_begin(SZ, crc32_ref(img, SZ), 1), "begin ok");
    feed_in_chunks(img, SZ, 2048);
    CHECK(fw_loader_error() == FW_LOADER_ERR_VECTOR, "bootloader PC must fail");

    /* Even (non-thumb) PC */
    make_image(img, SZ);
    uint32_t even_pc = 0x08007310u;
    memcpy(img + 4, &even_pc, 4);
    CHECK(fw_loader_begin(SZ, crc32_ref(img, SZ), 1), "begin ok");
    feed_in_chunks(img, SZ, 2048);
    CHECK(fw_loader_error() == FW_LOADER_ERR_VECTOR, "even PC must fail");
}

static void test_timeout_recovers(void)
{
    enum { SZ = 16384 };
    static uint8_t img[SZ];
    make_image(img, SZ);
    uint32_t crc = crc32_ref(img, SZ);

    CHECK(fw_loader_begin(SZ, crc, 1), "begin ok");
    fw_loader_feed(img, 1000);          /* partial, then silence */
    for (int i = 0; i < 400; ++i)
        fw_loader_poll();
    CHECK(fw_loader_state() == FW_LOADER_ERROR &&
          fw_loader_error() == FW_LOADER_ERR_TIMEOUT,
          "silence must time out (state=%d err=%d)",
          fw_loader_state(), fw_loader_error());
    CHECK(!fw_loader_active(), "timeout must return routing to the shell");

    /* And a full retry succeeds from that state. */
    CHECK(fw_loader_begin(SZ, crc, 1), "retry begin ok");
    feed_in_chunks(img, SZ, 4096);
    CHECK(fw_loader_state() == FW_LOADER_STAGED, "retry must stage");
}

/* The loader owns no buffer; on target the shell task lends its bus scratch.
 * Before that has happened nothing may start — and a lend that is too small
 * must count as no lend at all. */
static void test_no_buffer_gate(void)
{
    static uint8_t tiny[FW_LOADER_SCRATCH_MIN - 1];

    CHECK(!fw_loader_begin(20000, 0x1234, 1), "begin without scratch must refuse");
    CHECK(fw_loader_error() == FW_LOADER_ERR_NO_BUFFER, "err must be NO_BUFFER");
    CHECK(!fw_loader_active(), "a refused begin must not activate RX routing");
    CHECK(!fw_loader_install_slot(0), "install without scratch must refuse");
    CHECK(fw_loader_error() == FW_LOADER_ERR_NO_BUFFER, "err must be NO_BUFFER");

    fw_loader_attach_scratch(tiny, sizeof(tiny));
    CHECK(!fw_loader_begin(20000, 0x1234, 1), "undersized scratch must count as none");
    CHECK(fw_loader_error() == FW_LOADER_ERR_NO_BUFFER, "err must be NO_BUFFER");
}

int main(void)
{
    /* Same shape as the target: one 2 KB arena lent by the shell task. */
    static uint8_t scratch[2048];

    test_no_buffer_gate();
    fw_loader_attach_scratch(scratch, sizeof(scratch));

    test_size_gate();
    test_happy_path();
    test_crc_gate();
    test_vector_gate();
    test_timeout_recovers();

    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    printf("all fw_loader tests passed\n");
    return 0;
}
