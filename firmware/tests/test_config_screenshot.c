/*
 * OpenScope 2C53T - Tests for Config and Screenshot modules
 *
 * Build (native):
 *   gcc -o tests/test_config_screenshot tests/test_config_screenshot.c \
 *       src/util/config.c src/util/screenshot.c \
 *       -DFEATURE_SCREENSHOT -Isrc/util -lm
 *
 * Run:
 *   ./tests/test_config_screenshot
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "screenshot.h"

/* ========================================================================
 * Test helpers
 * ======================================================================== */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  TEST: %-50s ", name); } while (0)

#define PASS() \
    do { printf("[PASS]\n"); tests_passed++; } while (0)

#define FAIL(msg) \
    do { printf("[FAIL] %s\n", msg); tests_failed++; } while (0)

#define ASSERT_TRUE(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while (0)

#define ASSERT_FALSE(cond, msg) \
    do { if (cond) { FAIL(msg); return; } } while (0)

#define ASSERT_EQ(a, b, msg) \
    do { if ((a) != (b)) { FAIL(msg); return; } } while (0)

/* ========================================================================
 * Config tests
 * ======================================================================== */

static void test_config_defaults(void)
{
    TEST("config_init_defaults sets magic and version");
    device_config_t cfg;
    config_init_defaults(&cfg);
    ASSERT_EQ(cfg.magic, CONFIG_MAGIC, "magic mismatch");
    ASSERT_EQ(cfg.version, CONFIG_VERSION, "version mismatch");
    ASSERT_TRUE(cfg.scope_ch1_enabled, "ch1 should be enabled");
    ASSERT_TRUE(cfg.scope_ch2_enabled, "ch2 should be enabled");
    ASSERT_EQ(cfg.scope_ch1_vdiv, 3, "ch1 vdiv default");
    ASSERT_EQ(cfg.display_brightness, 80, "brightness default");
    PASS();
}

static void test_config_checksum_changes(void)
{
    TEST("checksum changes when data changes");
    device_config_t cfg;
    config_init_defaults(&cfg);
    uint32_t original = config_compute_checksum(&cfg);

    cfg.scope_ch1_vdiv = 7;
    uint32_t modified = config_compute_checksum(&cfg);

    ASSERT_TRUE(original != modified, "checksums should differ");
    PASS();
}

static void test_config_validate_valid(void)
{
    TEST("validate returns true for valid config");
    device_config_t cfg;
    config_init_defaults(&cfg);
    ASSERT_TRUE(config_validate(&cfg), "should validate");
    PASS();
}

static void test_config_validate_bad_magic(void)
{
    TEST("validate returns false for bad magic");
    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.magic = 0xDEADBEEF;
    /* Recompute checksum so only magic is wrong */
    cfg.checksum = config_compute_checksum(&cfg);
    ASSERT_FALSE(config_validate(&cfg), "should fail on bad magic");
    PASS();
}

static void test_config_validate_bad_checksum(void)
{
    TEST("validate returns false for corrupted checksum");
    device_config_t cfg;
    config_init_defaults(&cfg);
    cfg.checksum = cfg.checksum + 1; /* Corrupt it */
    ASSERT_FALSE(config_validate(&cfg), "should fail on bad checksum");
    PASS();
}

static void test_config_serialize_roundtrip(void)
{
    TEST("serialize/deserialize round-trip preserves data");
    device_config_t original, loaded;
    config_init_defaults(&original);
    original.siggen_frequency = 5000.0f;
    original.scope_trigger_level = -42;
    original.checksum = config_compute_checksum(&original);

    uint8_t buf[256];
    uint32_t written = config_serialize(&original, buf, sizeof(buf));
    ASSERT_TRUE(written > 0, "serialize should succeed");
    ASSERT_EQ(written, (uint32_t)sizeof(device_config_t), "size mismatch");

    bool ok = config_deserialize(&loaded, buf, written);
    ASSERT_TRUE(ok, "deserialize should succeed");
    ASSERT_EQ(loaded.magic, CONFIG_MAGIC, "magic");
    ASSERT_EQ(loaded.scope_trigger_level, -42, "trigger level");
    ASSERT_TRUE(loaded.siggen_frequency == 5000.0f, "frequency");
    PASS();
}

static void test_config_deserialize_bad_magic(void)
{
    TEST("deserialize rejects bad magic");
    device_config_t cfg;
    config_init_defaults(&cfg);

    uint8_t buf[256];
    config_serialize(&cfg, buf, sizeof(buf));

    /* Corrupt magic */
    buf[0] = 0xFF;

    device_config_t loaded;
    ASSERT_FALSE(config_deserialize(&loaded, buf, sizeof(buf)),
                 "should reject bad magic");
    PASS();
}

static void test_config_deserialize_bad_checksum(void)
{
    TEST("deserialize rejects bad checksum");
    device_config_t cfg;
    config_init_defaults(&cfg);

    uint8_t buf[256];
    uint32_t written = config_serialize(&cfg, buf, sizeof(buf));

    /* Corrupt a data byte (not the checksum) */
    buf[8] ^= 0xFF;

    device_config_t loaded;
    ASSERT_FALSE(config_deserialize(&loaded, buf, written),
                 "should reject bad checksum");
    PASS();
}

static void test_config_serialize_buffer_too_small(void)
{
    TEST("serialize fails with too-small buffer");
    device_config_t cfg;
    config_init_defaults(&cfg);

    uint8_t buf[4];
    uint32_t written = config_serialize(&cfg, buf, sizeof(buf));
    ASSERT_EQ(written, 0u, "should return 0");
    PASS();
}

static void test_config_save_load_roundtrip(void)
{
    TEST("save/load round-trip via stub backend");
    device_config_t original, loaded;
    config_init_defaults(&original);
    original.auto_shutdown_mins = 15;
    original.checksum = config_compute_checksum(&original);

    ASSERT_TRUE(config_save(&original), "save should succeed");
    ASSERT_TRUE(config_load(&loaded), "load should succeed");
    ASSERT_EQ(loaded.auto_shutdown_mins, 15, "value preserved");
    ASSERT_TRUE(config_validate(&loaded), "loaded config valid");
    PASS();
}

/* ========================================================================
 * Screenshot tests
 * ======================================================================== */

static void test_screenshot_clear_red(void)
{
    TEST("clear with red fills all pixels with 0xF800");
    screenshot_init();
    screenshot_clear(0xF800);

    const uint8_t *fb = screenshot_get_framebuffer();
    ASSERT_TRUE(fb != NULL, "framebuffer should not be NULL");

    /* Check first, middle, and last pixel */
    /* 0xF800 LE: low byte = 0x00, high byte = 0xF8 */
    ASSERT_EQ(fb[0], 0x00, "pixel 0 low byte");
    ASSERT_EQ(fb[1], 0xF8, "pixel 0 high byte");

    uint32_t mid = (SCREENSHOT_SIZE / 2) & ~1u;
    ASSERT_EQ(fb[mid], 0x00, "mid pixel low byte");
    ASSERT_EQ(fb[mid + 1], 0xF8, "mid pixel high byte");

    uint32_t last = SCREENSHOT_SIZE - 2;
    ASSERT_EQ(fb[last], 0x00, "last pixel low byte");
    ASSERT_EQ(fb[last + 1], 0xF8, "last pixel high byte");
    PASS();
}

static void test_screenshot_set_pixel(void)
{
    TEST("set_pixel writes to correct position");
    screenshot_init();
    screenshot_clear(0x0000);

    /* Set pixel at (10, 5) to green (0x07E0) */
    screenshot_set_pixel(10, 5, 0x07E0);

    const uint8_t *fb = screenshot_get_framebuffer();
    uint32_t offset = (5 * SCREENSHOT_WIDTH + 10) * SCREENSHOT_BPP;
    ASSERT_EQ(fb[offset], 0xE0, "green low byte");
    ASSERT_EQ(fb[offset + 1], 0x07, "green high byte");

    /* Verify neighboring pixel is still black */
    uint32_t neighbor = (5 * SCREENSHOT_WIDTH + 11) * SCREENSHOT_BPP;
    ASSERT_EQ(fb[neighbor], 0x00, "neighbor should be black");
    PASS();
}

static void test_screenshot_fill_rect(void)
{
    TEST("fill_rect fills correct area");
    screenshot_init();
    screenshot_clear(0x0000);

    /* Fill a 3x2 rect at (100, 50) with blue (0x001F) */
    screenshot_fill_rect(100, 50, 3, 2, 0x001F);

    const uint8_t *fb = screenshot_get_framebuffer();

    /* Check all 6 pixels in the rect */
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            uint32_t offset = ((50 + row) * SCREENSHOT_WIDTH + (100 + col)) * SCREENSHOT_BPP;
            ASSERT_EQ(fb[offset], 0x1F, "blue low byte");
            ASSERT_EQ(fb[offset + 1], 0x00, "blue high byte");
        }
    }

    /* Check pixel just outside the rect */
    uint32_t outside = (50 * SCREENSHOT_WIDTH + 103) * SCREENSHOT_BPP;
    ASSERT_EQ(fb[outside], 0x00, "outside should be black");
    PASS();
}

static void test_screenshot_bmp_header(void)
{
    TEST("BMP header has correct signature, size, dimensions");
    screenshot_init();
    screenshot_clear(0x0000);

    uint8_t *bmp = (uint8_t *)malloc(BMP_FILE_SIZE);
    ASSERT_TRUE(bmp != NULL, "malloc failed");

    uint32_t size = screenshot_capture_bmp(bmp, BMP_FILE_SIZE);
    ASSERT_EQ(size, (uint32_t)BMP_FILE_SIZE, "BMP size");

    /* Signature */
    ASSERT_EQ(bmp[0], 'B', "BMP sig B");
    ASSERT_EQ(bmp[1], 'M', "BMP sig M");

    /* File size (LE) */
    uint32_t file_size = bmp[2] | (bmp[3] << 8) | (bmp[4] << 16) | (bmp[5] << 24);
    ASSERT_EQ(file_size, (uint32_t)BMP_FILE_SIZE, "file size in header");

    /* Pixel data offset */
    uint32_t px_offset = bmp[10] | (bmp[11] << 8) | (bmp[12] << 16) | (bmp[13] << 24);
    ASSERT_EQ(px_offset, (uint32_t)BMP_HEADER_SIZE, "pixel offset");

    /* Width */
    uint32_t width = bmp[18] | (bmp[19] << 8) | (bmp[20] << 16) | (bmp[21] << 24);
    ASSERT_EQ(width, (uint32_t)SCREENSHOT_WIDTH, "width");

    /* Height (negative for top-down, stored as 2's complement) */
    int32_t height;
    memcpy(&height, bmp + 22, 4);
    ASSERT_EQ(height, -(int32_t)SCREENSHOT_HEIGHT, "height (top-down)");

    /* BPP */
    uint16_t bpp = bmp[28] | (bmp[29] << 8);
    ASSERT_EQ(bpp, 16, "bits per pixel");

    /* Compression = BI_BITFIELDS (3) */
    uint32_t compression = bmp[30] | (bmp[31] << 8) | (bmp[32] << 16) | (bmp[33] << 24);
    ASSERT_EQ(compression, 3u, "BI_BITFIELDS compression");

    free(bmp);
    PASS();
}

static void test_screenshot_bmp_pixel_data(void)
{
    TEST("BMP pixel data matches shadow framebuffer");
    screenshot_init();
    screenshot_clear(0x0000);

    /* Set a known pixel */
    screenshot_set_pixel(0, 0, 0xF800); /* Red */
    screenshot_set_pixel(1, 0, 0x07E0); /* Green */

    uint8_t *bmp = (uint8_t *)malloc(BMP_FILE_SIZE);
    ASSERT_TRUE(bmp != NULL, "malloc failed");

    screenshot_capture_bmp(bmp, BMP_FILE_SIZE);

    /* Verify pixel data at correct offset */
    const uint8_t *fb = screenshot_get_framebuffer();
    ASSERT_EQ(memcmp(bmp + BMP_HEADER_SIZE, fb, SCREENSHOT_SIZE), 0,
              "pixel data should match framebuffer");

    /* Verify specific pixels */
    uint8_t *px = bmp + BMP_HEADER_SIZE;
    /* Pixel (0,0) = 0xF800 LE */
    ASSERT_EQ(px[0], 0x00, "red low");
    ASSERT_EQ(px[1], 0xF8, "red high");
    /* Pixel (1,0) = 0x07E0 LE */
    ASSERT_EQ(px[2], 0xE0, "green low");
    ASSERT_EQ(px[3], 0x07, "green high");

    free(bmp);
    PASS();
}

static void test_screenshot_bmp_too_small_buffer(void)
{
    TEST("BMP capture fails with too-small buffer");
    screenshot_init();

    uint8_t buf[64];
    uint32_t size = screenshot_capture_bmp(buf, sizeof(buf));
    ASSERT_EQ(size, 0u, "should return 0 for small buffer");
    PASS();
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
    printf("\n=== Config Tests ===\n");
    test_config_defaults();
    test_config_checksum_changes();
    test_config_validate_valid();
    test_config_validate_bad_magic();
    test_config_validate_bad_checksum();
    test_config_serialize_roundtrip();
    test_config_deserialize_bad_magic();
    test_config_deserialize_bad_checksum();
    test_config_serialize_buffer_too_small();
    test_config_save_load_roundtrip();

    printf("\n=== Screenshot Tests ===\n");
    test_screenshot_clear_red();
    test_screenshot_set_pixel();
    test_screenshot_fill_rect();
    test_screenshot_bmp_header();
    test_screenshot_bmp_pixel_data();
    test_screenshot_bmp_too_small_buffer();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
