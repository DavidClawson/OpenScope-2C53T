/*
 * test_decode_kline.c — Tests for K-Line / KWP2000 protocol decoder
 *
 * Build:
 *   gcc -o tests/test_decode_kline tests/test_decode_kline.c \
 *       src/decode/decode_kline.c src/decode/decode_uart.c \
 *       -lm -Isrc/decode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "decode_kline.h"

#define SAMPLE_RATE  1000000.0f   /* 1 MHz */
#define BAUD_10400   10400.0f
#define BAUD_5       5.0f

/* Signal levels for K-Line (above/below threshold of 0) */
#define LEVEL_HIGH   1000    /* Idle / recessive (12V) */
#define LEVEL_LOW    (-1000) /* Dominant (0V) */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%s] ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg, ...) do { \
    printf("FAIL: " msg "\n", ##__VA_ARGS__); \
} while(0)

/* ------------------------------------------------------------------ */
/* Signal generation helpers                                           */
/* ------------------------------------------------------------------ */

/* Fill samples with idle (high) level */
static void fill_idle(int16_t *buf, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
        buf[i] = LEVEL_HIGH;
}

/* Generate one UART byte at given baud rate, return samples consumed */
static uint32_t gen_uart_byte(int16_t *buf, uint8_t byte_val,
                               float sample_rate, float baud_rate)
{
    float spb = sample_rate / baud_rate;
    uint32_t total = (uint32_t)(spb * 10.0f);  /* 1 start + 8 data + 1 stop */

    /* Start bit (low) */
    uint32_t start_end = (uint32_t)spb;
    for (uint32_t i = 0; i < start_end && i < total; i++)
        buf[i] = LEVEL_LOW;

    /* 8 data bits, LSB first */
    for (int bit = 0; bit < 8; bit++) {
        uint32_t bit_start = (uint32_t)(spb * (1.0f + (float)bit));
        uint32_t bit_end = (uint32_t)(spb * (2.0f + (float)bit));
        int16_t level = (byte_val & (1 << bit)) ? LEVEL_HIGH : LEVEL_LOW;
        for (uint32_t i = bit_start; i < bit_end && i < total; i++)
            buf[i] = level;
    }

    /* Stop bit (high) */
    uint32_t stop_start = (uint32_t)(spb * 9.0f);
    for (uint32_t i = stop_start; i < total; i++)
        buf[i] = LEVEL_HIGH;

    return total;
}

/* Generate a KWP2000 message as K-Line samples.
 * Returns total samples written starting at buf[offset]. */
static uint32_t gen_kwp_message(int16_t *buf, uint32_t offset,
                                 uint8_t format, uint8_t target, uint8_t source,
                                 const uint8_t *data, uint8_t data_len,
                                 float sample_rate)
{
    uint8_t msg[64];
    uint32_t msg_len = 0;

    uint8_t addr_mode = (format >> 6) & 0x03;

    /* Build message */
    msg[msg_len++] = format;

    if (addr_mode >= 1) {
        msg[msg_len++] = target;
        msg[msg_len++] = source;
    }

    /* If addr_mode == 3, separate length byte */
    if (addr_mode == 3) {
        msg[msg_len++] = data_len;
    }

    /* Data (including SID) */
    for (uint8_t i = 0; i < data_len; i++)
        msg[msg_len++] = data[i];

    /* Checksum */
    uint8_t cs = 0;
    for (uint32_t i = 0; i < msg_len; i++)
        cs += msg[i];
    msg[msg_len++] = cs;

    /* Generate UART samples for each byte */
    uint32_t pos = 0;
    for (uint32_t i = 0; i < msg_len; i++) {
        pos += gen_uart_byte(buf + offset + pos, msg[i],
                             sample_rate, BAUD_10400);
    }

    return pos;
}

/* Generate a fast init pulse (25ms low, 25ms high) */
static uint32_t gen_fast_init(int16_t *buf, uint32_t offset, float sample_rate)
{
    uint32_t pulse_samples = (uint32_t)(0.025f * sample_rate);  /* 25ms */
    uint32_t total = pulse_samples * 2;

    for (uint32_t i = 0; i < pulse_samples; i++)
        buf[offset + i] = LEVEL_LOW;
    for (uint32_t i = pulse_samples; i < total; i++)
        buf[offset + i] = LEVEL_HIGH;

    return total;
}

/* Generate 5-baud init byte 0x33 */
static uint32_t gen_5baud_init(int16_t *buf, uint32_t offset, float sample_rate)
{
    return gen_uart_byte(buf + offset, 0x33, sample_rate, BAUD_5);
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_tester_present(void)
{
    TEST(tester_present);

    /* TesterPresent: format=0x80|0x01=0x81 (addr mode 10, len=1),
     * but using mode 11 with address: format=0xC0|0x00, target=0x33, source=0xF1 */
    /* Actually: format byte 0xC0 means addr_mode=3, len in separate byte.
     * For SID only (1 byte data): format=0xC0, target=0x33, source=0xF1, len=1, SID=0x3E */

    int16_t samples[20000];
    fill_idle(samples, 20000);

    /* Idle gap then message */
    uint32_t offset = 500;
    uint8_t data[] = { 0x3E };  /* TesterPresent SID */
    uint32_t consumed = gen_kwp_message(samples, offset,
                                         0xC1, 0x33, 0xF1,
                                         data, 1, SAMPLE_RATE);
    (void)consumed;

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 20000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        return;
    }

    decode_frame_t *f = &result.frames[0];
    if (strcmp(f->label, "TstrPres") != 0) {
        FAIL("expected label 'TstrPres', got '%s'", f->label);
        return;
    }
    if (!(f->flags & KLINE_FLAG_CHECKSUM_OK)) {
        FAIL("expected CHECKSUM_OK flag");
        return;
    }
    if (!(f->flags & KLINE_FLAG_REQUEST)) {
        FAIL("expected REQUEST flag (source=0xF1 is tester)");
        return;
    }

    PASS();
}

static void test_read_dtc(void)
{
    TEST(read_dtc);

    int16_t samples[20000];
    fill_idle(samples, 20000);

    /* ReadDTCByStatus request: SID=0x18 */
    uint8_t data[] = { 0x18, 0x00, 0xFF, 0x00 };
    gen_kwp_message(samples, 500, 0xC4, 0x33, 0xF1,
                    data, 4, SAMPLE_RATE);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 20000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        return;
    }

    if (strcmp(result.frames[0].label, "ReadDTC") != 0) {
        FAIL("expected 'ReadDTC', got '%s'", result.frames[0].label);
        return;
    }

    PASS();
}

static void test_start_diagnostic(void)
{
    TEST(start_diagnostic);

    int16_t samples[20000];
    fill_idle(samples, 20000);

    /* StartDiagnosticSession: SID=0x10, sub=0x01 */
    uint8_t data[] = { 0x10, 0x01 };
    gen_kwp_message(samples, 500, 0xC2, 0x33, 0xF1,
                    data, 2, SAMPLE_RATE);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 20000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        return;
    }

    if (strcmp(result.frames[0].label, "StartDg") != 0) {
        FAIL("expected 'StartDg', got '%s'", result.frames[0].label);
        return;
    }

    PASS();
}

static void test_checksum_valid(void)
{
    TEST(checksum_valid);

    int16_t samples[20000];
    fill_idle(samples, 20000);

    uint8_t data[] = { 0x3E };
    gen_kwp_message(samples, 500, 0xC1, 0x33, 0xF1,
                    data, 1, SAMPLE_RATE);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 20000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        return;
    }
    if (!(result.frames[0].flags & KLINE_FLAG_CHECKSUM_OK)) {
        FAIL("expected CHECKSUM_OK flag");
        return;
    }
    if (result.frames[0].flags & KLINE_FLAG_CHECKSUM_ERR) {
        FAIL("should NOT have CHECKSUM_ERR flag");
        return;
    }

    PASS();
}

static void test_checksum_error(void)
{
    TEST(checksum_error);

    int16_t samples[20000];
    fill_idle(samples, 20000);

    /* Generate a valid message first */
    uint8_t data[] = { 0x3E };
    gen_kwp_message(samples, 500, 0xC1, 0x33, 0xF1,
                    data, 1, SAMPLE_RATE);

    /* Corrupt the checksum byte (last byte of message).
     * The message is: format(0xC1) target(0x33) source(0xF1) len(0x01) SID(0x3E) CS
     * = 6 bytes. Overwrite the last byte's UART representation with wrong value. */
    float spb = SAMPLE_RATE / BAUD_10400;
    uint32_t byte_period = (uint32_t)(spb * 10.0f);
    /* Checksum byte starts at offset 500 + 5*byte_period */
    uint32_t cs_offset = 500 + 5 * byte_period;
    /* Overwrite with a different byte to corrupt checksum */
    gen_uart_byte(samples + cs_offset, 0x00, SAMPLE_RATE, BAUD_10400);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 20000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        return;
    }
    if (!(result.frames[0].flags & KLINE_FLAG_CHECKSUM_ERR)) {
        FAIL("expected CHECKSUM_ERR flag, flags=0x%04X", result.frames[0].flags);
        return;
    }

    PASS();
}

static void test_fast_init(void)
{
    TEST(fast_init);

    /* Need enough room for fast init pulse (50ms = 50000 samples) + message */
    uint32_t buf_size = 100000;
    int16_t *samples = malloc(buf_size * sizeof(int16_t));
    fill_idle(samples, buf_size);

    /* Fast init pulse at start */
    uint32_t init_end = gen_fast_init(samples, 0, SAMPLE_RATE);

    /* Add some idle gap then a StartCommunication response */
    uint32_t gap = 500;
    uint8_t data[] = { 0xC1 };  /* StartCommunication positive response (0x81+0x40) */
    gen_kwp_message(samples, init_end + gap, 0xC1, 0xF1, 0x33,
                    data, 1, SAMPLE_RATE);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, buf_size, SAMPLE_RATE, &cfg, &result);

    /* Should have at least 2 frames: fast init + message */
    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        free(samples);
        return;
    }

    /* First frame should be fast init */
    if (!(result.frames[0].flags & KLINE_FLAG_INIT_FAST)) {
        FAIL("expected INIT_FAST flag on first frame, flags=0x%04X", result.frames[0].flags);
        free(samples);
        return;
    }

    free(samples);
    PASS();
}

static void test_5baud_init(void)
{
    TEST(5baud_init);

    /* 5-baud init: 0x33 at 5 baud = 2 seconds = 2,000,000 samples at 1MHz */
    /* Use a lower effective sample rate to keep buffer manageable */
    float test_sr = 50000.0f;  /* 50kHz — still 9.6 samples/bit at 5 baud nope...
                                  at 5 baud: 50000/5 = 10000 samples per bit
                                  10 bits = 100000 samples. */
    uint32_t buf_size = 150000;
    int16_t *samples = malloc(buf_size * sizeof(int16_t));
    fill_idle(samples, buf_size);

    /* Generate 5-baud init */
    gen_5baud_init(samples, 0, test_sr);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, buf_size, test_sr, &cfg, &result);

    if (result.num_frames < 1) {
        FAIL("expected at least 1 frame, got %d", result.num_frames);
        free(samples);
        return;
    }

    if (!(result.frames[0].flags & KLINE_FLAG_INIT_5BAUD)) {
        FAIL("expected INIT_5BAUD flag, flags=0x%04X", result.frames[0].flags);
        free(samples);
        return;
    }

    if (strcmp(result.frames[0].label, "5BdInit") != 0) {
        FAIL("expected label '5BdInit', got '%s'", result.frames[0].label);
        free(samples);
        return;
    }

    free(samples);
    PASS();
}

static void test_service_names(void)
{
    TEST(service_names);

    struct { uint8_t sid; const char *expected; } cases[] = {
        { 0x10, "StartDg" },
        { 0x14, "ClrDTC" },
        { 0x18, "ReadDTC" },
        { 0x21, "RdLocal" },
        { 0x3E, "TstrPres" },
        { 0x81, "StrtComm" },
        { 0x82, "StopComm" },
        { 0x7F, "NegResp" },
        /* Positive responses (SID + 0x40) should map back */
        { 0x50, "StartDg" },    /* 0x10 + 0x40 */
        { 0x58, "ReadDTC" },    /* 0x18 + 0x40 */
        { 0x7E, "TstrPres" },   /* 0x3E + 0x40 */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *got = kline_service_name(cases[i].sid);
        if (strcmp(got, cases[i].expected) != 0) {
            FAIL("SID 0x%02X: expected '%s', got '%s'",
                 cases[i].sid, cases[i].expected, got);
            return;
        }
    }

    PASS();
}

static void test_multiple_messages(void)
{
    TEST(multiple_messages);

    int16_t samples[40000];
    fill_idle(samples, 40000);

    /* Message 1: TesterPresent request */
    uint8_t data1[] = { 0x3E };
    uint32_t len1 = gen_kwp_message(samples, 500, 0xC1, 0x33, 0xF1,
                                     data1, 1, SAMPLE_RATE);

    /* Gap between messages (idle) */
    uint32_t gap = 2000;

    /* Message 2: TesterPresent positive response (SID=0x7E) */
    uint8_t data2[] = { 0x7E };
    gen_kwp_message(samples, 500 + len1 + gap, 0xC1, 0xF1, 0x33,
                    data2, 1, SAMPLE_RATE);

    kline_config_t cfg = { .threshold = 0, .protocol = 0, .address_mode = 0 };
    decode_result_t result;
    decode_kline(samples, 40000, SAMPLE_RATE, &cfg, &result);

    if (result.num_frames < 2) {
        FAIL("expected at least 2 frames, got %d", result.num_frames);
        return;
    }

    /* First should be request */
    if (!(result.frames[0].flags & KLINE_FLAG_REQUEST)) {
        FAIL("frame 0: expected REQUEST flag");
        return;
    }
    if (strcmp(result.frames[0].label, "TstrPres") != 0) {
        FAIL("frame 0: expected 'TstrPres', got '%s'", result.frames[0].label);
        return;
    }

    /* Second should be response */
    if (!(result.frames[1].flags & KLINE_FLAG_RESPONSE)) {
        FAIL("frame 1: expected RESPONSE flag, flags=0x%04X", result.frames[1].flags);
        return;
    }
    if (strcmp(result.frames[1].label, "TstrPres") != 0) {
        FAIL("frame 1: expected 'TstrPres', got '%s'", result.frames[1].label);
        return;
    }

    PASS();
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("K-Line / KWP2000 Decoder Tests\n");
    printf("==============================\n");

    test_tester_present();
    test_read_dtc();
    test_start_diagnostic();
    test_checksum_valid();
    test_checksum_error();
    test_fast_init();
    test_5baud_init();
    test_service_names();
    test_multiple_messages();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
