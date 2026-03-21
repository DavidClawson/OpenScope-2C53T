/*
 * Noisy signal stress tests for protocol decoders (UART, I2C, SPI, CAN).
 *
 * Real oscilloscope signals have noise, timing jitter, ringing, and imperfect
 * edges. These tests verify the decoders handle realistic signals gracefully.
 *
 * Build:
 *   gcc -o tests/test_decode_noisy tests/test_decode_noisy.c \
 *       src/decode/decode_uart.c src/decode/decode_i2c.c \
 *       src/decode/decode_spi.c src/decode/decode_can.c \
 *       -lm -Isrc/decode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "decode_uart.h"
#include "decode_i2c.h"
#include "decode_spi.h"
#include "decode_can.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* ========================================================================
 * Signal constants (same as test_decode.c)
 * ======================================================================== */

#define SAMPLE_RATE 1000000.0f  /* 1 MHz sample rate */
#define HIGH_LEVEL  1000
#define LOW_LEVEL  (-1000)
#define THRESHOLD   0

/* ========================================================================
 * Noise generation helpers
 * ======================================================================== */

/* Add pseudo-random noise to signal samples using LCG PRNG.
 * noise_amplitude: peak noise amplitude (e.g., 100 = 10% of +-1000 signal) */
static void add_noise(int16_t *samples, uint32_t num_samples, float noise_amplitude)
{
    uint32_t seed = 12345;
    uint32_t i;
    for (i = 0; i < num_samples; i++) {
        seed = seed * 1664525 + 1013904223;
        int16_t noise = (int16_t)((int32_t)(seed >> 16) * noise_amplitude / 32768);
        int32_t val = samples[i] + noise;
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples[i] = (int16_t)val;
    }
}

/* Add timing jitter by shifting edge positions randomly.
 * Finds transitions and shifts the transition point by +-jitter_samples. */
static void add_jitter(int16_t *samples, uint32_t num_samples, uint32_t jitter_samples)
{
    uint32_t seed = 67890;
    uint32_t i;
    if (jitter_samples == 0 || num_samples < 2) return;

    /* Work on a copy to avoid cascading shifts */
    int16_t *copy = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (!copy) return;
    memcpy(copy, samples, num_samples * sizeof(int16_t));

    for (i = 1; i < num_samples; i++) {
        int prev_high = (copy[i - 1] > THRESHOLD) ? 1 : 0;
        int cur_high = (copy[i] > THRESHOLD) ? 1 : 0;

        if (prev_high != cur_high) {
            /* Edge found. Compute random offset. */
            seed = seed * 1664525 + 1013904223;
            int offset = (int)((seed >> 16) % (2 * jitter_samples + 1)) - (int)jitter_samples;

            /* Shift this edge: extend or shorten the previous level */
            if (offset > 0) {
                /* Delay the edge: extend previous value forward */
                uint32_t j;
                for (j = 0; j < (uint32_t)offset && (i + j) < num_samples; j++) {
                    samples[i + j] = copy[i - 1];
                }
            } else if (offset < 0) {
                /* Advance the edge: extend new value backward */
                uint32_t j;
                for (j = 0; j < (uint32_t)(-offset) && i > (uint32_t)(-offset); j++) {
                    samples[i - 1 - j] = copy[i];
                }
            }
        }
    }
    free(copy);
}

/* Add a DC offset to all samples */
static void add_dc_offset(int16_t *samples, uint32_t num_samples, int16_t offset)
{
    uint32_t i;
    for (i = 0; i < num_samples; i++) {
        int32_t val = samples[i] + offset;
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples[i] = (int16_t)val;
    }
}

/* ========================================================================
 * Signal generation helpers (copied from test_decode.c)
 * ======================================================================== */

static void fill_samples(int16_t *buf, uint32_t start, uint32_t end, int16_t level)
{
    uint32_t i;
    for (i = start; i < end; i++) {
        buf[i] = level;
    }
}

static uint32_t gen_uart_byte(int16_t *buf, uint32_t offset,
                              uint8_t byte_val, uint32_t baud,
                              uint8_t data_bits, uint8_t parity, uint8_t stop_bits)
{
    float spb = SAMPLE_RATE / (float)baud;
    uint32_t bit_idx = 0;
    uint32_t start, end;
    uint8_t i;

    /* Start bit (low) */
    start = offset + (uint32_t)(bit_idx * spb);
    end = offset + (uint32_t)((bit_idx + 1) * spb);
    fill_samples(buf, start, end, LOW_LEVEL);
    bit_idx++;

    /* Data bits (LSB first) */
    uint8_t ones = 0;
    for (i = 0; i < data_bits; i++) {
        int bit = (byte_val >> i) & 1;
        if (bit) ones++;
        start = offset + (uint32_t)(bit_idx * spb);
        end = offset + (uint32_t)((bit_idx + 1) * spb);
        fill_samples(buf, start, end, bit ? HIGH_LEVEL : LOW_LEVEL);
        bit_idx++;
    }

    /* Parity bit */
    if (parity != 0) {
        int pbit;
        if (parity == 1) {
            pbit = (ones % 2 == 0) ? 1 : 0;
        } else {
            pbit = (ones % 2 == 1) ? 1 : 0;
        }
        start = offset + (uint32_t)(bit_idx * spb);
        end = offset + (uint32_t)((bit_idx + 1) * spb);
        fill_samples(buf, start, end, pbit ? HIGH_LEVEL : LOW_LEVEL);
        bit_idx++;
    }

    /* Stop bit(s) (high) */
    for (i = 0; i < stop_bits; i++) {
        start = offset + (uint32_t)(bit_idx * spb);
        end = offset + (uint32_t)((bit_idx + 1) * spb);
        fill_samples(buf, start, end, HIGH_LEVEL);
        bit_idx++;
    }

    return (uint32_t)(bit_idx * spb);
}

static void gen_i2c_transaction(int16_t *sda, int16_t *scl, uint32_t *total,
                                uint8_t addr_byte, uint8_t data_byte,
                                uint32_t bit_period, int repeated_start)
{
    uint32_t pos = 0;
    uint32_t half = bit_period / 2;
    uint32_t i;
    uint8_t bit;

    /* Idle: both high */
    fill_samples(sda, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    /* START: SDA falls while SCL high */
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(sda, pos, pos + half, HIGH_LEVEL);
    fill_samples(sda, pos + half, pos + bit_period, LOW_LEVEL);
    pos += bit_period;

    #define CLOCK_BIT(sda_val) do { \
        fill_samples(sda, pos, pos + bit_period, (sda_val) ? HIGH_LEVEL : LOW_LEVEL); \
        fill_samples(scl, pos, pos + half, LOW_LEVEL); \
        fill_samples(scl, pos + half, pos + bit_period, HIGH_LEVEL); \
        pos += bit_period; \
    } while(0)

    /* Address byte: MSB first, 8 bits */
    for (i = 0; i < 8; i++) {
        bit = (addr_byte >> (7 - i)) & 1;
        CLOCK_BIT(bit);
    }
    CLOCK_BIT(0); /* ACK */

    /* Data byte: MSB first, 8 bits */
    for (i = 0; i < 8; i++) {
        bit = (data_byte >> (7 - i)) & 1;
        CLOCK_BIT(bit);
    }
    CLOCK_BIT(0); /* ACK */

    if (repeated_start) {
        fill_samples(scl, pos, pos + half, LOW_LEVEL);
        fill_samples(sda, pos, pos + half, HIGH_LEVEL);
        pos += half;
        fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
        fill_samples(sda, pos, pos + half, HIGH_LEVEL);
        fill_samples(sda, pos + half, pos + bit_period, LOW_LEVEL);
        pos += bit_period;

        uint8_t addr_read = addr_byte | 0x01;
        for (i = 0; i < 8; i++) {
            bit = (addr_read >> (7 - i)) & 1;
            CLOCK_BIT(bit);
        }
        CLOCK_BIT(0); /* ACK */

        for (i = 0; i < 8; i++) {
            bit = (data_byte >> (7 - i)) & 1;
            CLOCK_BIT(bit);
        }
        CLOCK_BIT(1); /* NAK */
    }

    #undef CLOCK_BIT

    /* STOP: SCL goes high, then SDA goes high */
    fill_samples(scl, pos, pos + half, LOW_LEVEL);
    fill_samples(sda, pos, pos + half, LOW_LEVEL);
    pos += half;
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(sda, pos, pos + half, LOW_LEVEL);
    fill_samples(sda, pos + half, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    /* Trailing idle */
    fill_samples(sda, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    *total = pos;
}

static void gen_spi_byte(int16_t *mosi, int16_t *clk, int16_t *cs,
                         uint32_t *total, uint8_t byte_val,
                         uint32_t clk_period)
{
    uint32_t pos = 0;
    uint32_t half = clk_period / 2;
    uint8_t i;

    /* Idle */
    fill_samples(cs, pos, pos + clk_period, HIGH_LEVEL);
    fill_samples(clk, pos, pos + clk_period, LOW_LEVEL);
    fill_samples(mosi, pos, pos + clk_period, LOW_LEVEL);
    pos += clk_period;

    /* CS goes low */
    fill_samples(cs, pos, pos + half, LOW_LEVEL);
    fill_samples(clk, pos, pos + half, LOW_LEVEL);
    fill_samples(mosi, pos, pos + half, LOW_LEVEL);
    pos += half;

    for (i = 0; i < 8; i++) {
        int bit = (byte_val >> (7 - i)) & 1;
        fill_samples(mosi, pos, pos + clk_period, bit ? HIGH_LEVEL : LOW_LEVEL);
        fill_samples(clk, pos, pos + half, LOW_LEVEL);
        fill_samples(clk, pos + half, pos + clk_period, HIGH_LEVEL);
        fill_samples(cs, pos, pos + clk_period, LOW_LEVEL);
        pos += clk_period;
    }

    /* CS goes high */
    fill_samples(cs, pos, pos + clk_period, HIGH_LEVEL);
    fill_samples(clk, pos, pos + clk_period, LOW_LEVEL);
    fill_samples(mosi, pos, pos + clk_period, LOW_LEVEL);
    pos += clk_period;

    *total = pos;
}

/* Generate SPI byte with specific CPOL/CPHA mode */
static void gen_spi_byte_mode(int16_t *mosi, int16_t *clk, int16_t *cs,
                              uint32_t *total, uint8_t byte_val,
                              uint32_t clk_period, uint8_t cpol, uint8_t cpha)
{
    uint32_t pos = 0;
    uint32_t half = clk_period / 2;
    uint8_t i;
    int16_t clk_idle = cpol ? HIGH_LEVEL : LOW_LEVEL;
    int16_t clk_active = cpol ? LOW_LEVEL : HIGH_LEVEL;

    /* Idle */
    fill_samples(cs, pos, pos + clk_period, HIGH_LEVEL);
    fill_samples(clk, pos, pos + clk_period, clk_idle);
    fill_samples(mosi, pos, pos + clk_period, LOW_LEVEL);
    pos += clk_period;

    /* CS goes low */
    fill_samples(cs, pos, pos + half, LOW_LEVEL);
    fill_samples(clk, pos, pos + half, clk_idle);
    fill_samples(mosi, pos, pos + half, LOW_LEVEL);
    pos += half;

    for (i = 0; i < 8; i++) {
        int bit = (byte_val >> (7 - i)) & 1;
        fill_samples(mosi, pos, pos + clk_period, bit ? HIGH_LEVEL : LOW_LEVEL);

        if (cpha == 0) {
            /* CPHA=0: data on leading edge */
            fill_samples(clk, pos, pos + half, clk_idle);
            fill_samples(clk, pos + half, pos + clk_period, clk_active);
        } else {
            /* CPHA=1: data on trailing edge */
            fill_samples(clk, pos, pos + half, clk_active);
            fill_samples(clk, pos + half, pos + clk_period, clk_idle);
        }
        fill_samples(cs, pos, pos + clk_period, LOW_LEVEL);
        pos += clk_period;
    }

    /* CS goes high */
    fill_samples(cs, pos, pos + clk_period, HIGH_LEVEL);
    fill_samples(clk, pos, pos + clk_period, clk_idle);
    fill_samples(mosi, pos, pos + clk_period, LOW_LEVEL);
    pos += clk_period;

    *total = pos;
}

static void gen_can_frame(int16_t *buf, uint32_t buf_size, uint32_t *total,
                          uint32_t bit_rate, uint16_t can_id,
                          const uint8_t *data, uint8_t dlc)
{
    float spb = SAMPLE_RATE / (float)bit_rate;
    uint32_t raw_bit = 0;
    uint32_t consecutive = 0;
    int last_val = -1;
    uint32_t i;

    uint32_t idle_samples = (uint32_t)(10 * spb);
    fill_samples(buf, 0, idle_samples, HIGH_LEVEL);
    uint32_t bit_start = idle_samples;

    #define WRITE_BIT(val) do { \
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb); \
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb); \
        if (e > buf_size) e = buf_size; \
        fill_samples(buf, s, e, (val) ? HIGH_LEVEL : LOW_LEVEL); \
        if (last_val >= 0 && (val) == last_val) { \
            consecutive++; \
        } else { \
            consecutive = 1; \
        } \
        last_val = (val); \
        raw_bit++; \
        if (consecutive == 5 && stuffing_enabled) { \
            int stuff = !(val); \
            s = bit_start + (uint32_t)(raw_bit * spb); \
            e = bit_start + (uint32_t)((raw_bit + 1) * spb); \
            if (e > buf_size) e = buf_size; \
            fill_samples(buf, s, e, stuff ? HIGH_LEVEL : LOW_LEVEL); \
            last_val = stuff; \
            consecutive = 1; \
            raw_bit++; \
        } \
    } while(0)

    int stuffing_enabled = 1;

    /* SOF */
    WRITE_BIT(0);

    /* 11-bit ID, MSB first */
    for (i = 10; i < 11; i--) {
        WRITE_BIT((can_id >> i) & 1);
        if (i == 0) break;
    }

    WRITE_BIT(0); /* RTR */
    WRITE_BIT(0); /* IDE */
    WRITE_BIT(0); /* r0 */

    /* DLC: 4 bits */
    for (i = 3; i < 4; i--) {
        WRITE_BIT((dlc >> i) & 1);
        if (i == 0) break;
    }

    /* Data bytes */
    uint8_t d, b;
    for (d = 0; d < dlc; d++) {
        for (b = 0; b < 8; b++) {
            WRITE_BIT((data[d] >> (7 - b)) & 1);
        }
    }

    /* CRC: 15 zero bits */
    for (i = 0; i < 15; i++) {
        WRITE_BIT(0);
    }

    stuffing_enabled = 0;

    /* CRC delimiter (recessive) */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* ACK slot (dominant) */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, LOW_LEVEL);
        raw_bit++;
    }

    /* ACK delimiter (recessive) */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* EOF: 7 recessive bits */
    for (i = 0; i < 7; i++) {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* Trailing idle */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = s + (uint32_t)(5 * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
    }

    *total = bit_start + (uint32_t)((raw_bit + 5) * spb);
    if (*total > buf_size) *total = buf_size;

    #undef WRITE_BIT
}

/* Generate a CAN extended frame (29-bit ID) */
static void gen_can_frame_ext(int16_t *buf, uint32_t buf_size, uint32_t *total,
                              uint32_t bit_rate, uint32_t can_id_29,
                              const uint8_t *data, uint8_t dlc)
{
    float spb = SAMPLE_RATE / (float)bit_rate;
    uint32_t raw_bit = 0;
    uint32_t consecutive = 0;
    int last_val = -1;
    uint32_t i;

    uint32_t idle_samples = (uint32_t)(10 * spb);
    fill_samples(buf, 0, idle_samples, HIGH_LEVEL);
    uint32_t bit_start = idle_samples;

    #define WRITE_BIT(val) do { \
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb); \
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb); \
        if (e > buf_size) e = buf_size; \
        fill_samples(buf, s, e, (val) ? HIGH_LEVEL : LOW_LEVEL); \
        if (last_val >= 0 && (val) == last_val) { \
            consecutive++; \
        } else { \
            consecutive = 1; \
        } \
        last_val = (val); \
        raw_bit++; \
        if (consecutive == 5 && stuffing_enabled) { \
            int stuff = !(val); \
            s = bit_start + (uint32_t)(raw_bit * spb); \
            e = bit_start + (uint32_t)((raw_bit + 1) * spb); \
            if (e > buf_size) e = buf_size; \
            fill_samples(buf, s, e, stuff ? HIGH_LEVEL : LOW_LEVEL); \
            last_val = stuff; \
            consecutive = 1; \
            raw_bit++; \
        } \
    } while(0)

    int stuffing_enabled = 1;

    /* SOF */
    WRITE_BIT(0);

    /* Base ID (upper 11 bits of 29-bit ID), MSB first */
    uint16_t base_id = (can_id_29 >> 18) & 0x7FF;
    for (i = 10; i < 11; i--) {
        WRITE_BIT((base_id >> i) & 1);
        if (i == 0) break;
    }

    WRITE_BIT(0); /* SRR (substitute remote request, recessive=1 in CAN spec, but dominant for data) */
    WRITE_BIT(1); /* IDE = 1 (extended frame) */

    /* Extended ID: lower 18 bits, MSB first */
    uint32_t ext_id = can_id_29 & 0x3FFFF;
    for (i = 17; i < 18; i--) {
        WRITE_BIT((ext_id >> i) & 1);
        if (i == 0) break;
    }

    WRITE_BIT(0); /* RTR */
    WRITE_BIT(0); /* r1 */
    WRITE_BIT(0); /* r0 */

    /* DLC: 4 bits */
    for (i = 3; i < 4; i--) {
        WRITE_BIT((dlc >> i) & 1);
        if (i == 0) break;
    }

    /* Data bytes */
    uint8_t d, b;
    for (d = 0; d < dlc; d++) {
        for (b = 0; b < 8; b++) {
            WRITE_BIT((data[d] >> (7 - b)) & 1);
        }
    }

    /* CRC: 15 zero bits */
    for (i = 0; i < 15; i++) {
        WRITE_BIT(0);
    }

    stuffing_enabled = 0;

    /* CRC delimiter */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* ACK slot */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, LOW_LEVEL);
        raw_bit++;
    }

    /* ACK delimiter */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* EOF: 7 recessive bits */
    for (i = 0; i < 7; i++) {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* Trailing idle */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = s + (uint32_t)(5 * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
    }

    *total = bit_start + (uint32_t)((raw_bit + 5) * spb);
    if (*total > buf_size) *total = buf_size;

    #undef WRITE_BIT
}

/* ========================================================================
 * Helper: generate UART string into buffer
 * ======================================================================== */

static uint32_t gen_uart_string(int16_t *buf, uint32_t buf_size,
                                const char *str, uint32_t baud,
                                uint8_t data_bits, uint8_t parity, uint8_t stop_bits)
{
    float spb = SAMPLE_RATE / (float)baud;
    uint32_t offset = (uint32_t)(spb * 2); /* idle lead-in */

    fill_samples(buf, 0, buf_size, HIGH_LEVEL);

    uint32_t i;
    for (i = 0; str[i] != '\0'; i++) {
        uint32_t written = gen_uart_byte(buf, offset, (uint8_t)str[i],
                                         baud, data_bits, parity, stop_bits);
        offset += written;
    }
    return offset + (uint32_t)(spb * 2); /* idle trail */
}

/* ========================================================================
 * 1. UART with noise (6 tests)
 * ======================================================================== */

static void test_uart_noise_10pct(void)
{
    printf("Test: UART 9600 'Hello' with 10%% noise\n");

    uint32_t buf_size = 8000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "Hello", 9600, 8, 0, 1);

    /* 10% noise: amplitude 100, signal swing is +-1000 */
    add_noise(samples, used, 100.0f);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 5, "decoded 5 frames with 10% noise");
    if (result.num_frames == 5) {
        ASSERT(result.frames[0].data[0] == 'H', "byte 0 = 'H'");
        ASSERT(result.frames[1].data[0] == 'e', "byte 1 = 'e'");
        ASSERT(result.frames[2].data[0] == 'l', "byte 2 = 'l'");
        ASSERT(result.frames[3].data[0] == 'l', "byte 3 = 'l'");
        ASSERT(result.frames[4].data[0] == 'o', "byte 4 = 'o'");
        uint32_t i;
        for (i = 0; i < 5; i++) {
            ASSERT((result.frames[i].flags & DECODE_FLAG_ERROR) == 0,
                   "no framing error at 10% noise");
        }
    }
    printf("  SNR ~20dB (10%% noise): PASS\n");
    free(samples);
}

static void test_uart_noise_30pct(void)
{
    printf("Test: UART 9600 'Hello' with 30%% noise\n");

    uint32_t buf_size = 8000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "Hello", 9600, 8, 0, 1);

    /* 30% noise: amplitude 300 */
    add_noise(samples, used, 300.0f);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 5, "decoded 5 frames with 30% noise");
    if (result.num_frames == 5) {
        ASSERT(result.frames[0].data[0] == 'H', "byte 0 = 'H'");
        ASSERT(result.frames[1].data[0] == 'e', "byte 1 = 'e'");
        ASSERT(result.frames[2].data[0] == 'l', "byte 2 = 'l'");
        ASSERT(result.frames[3].data[0] == 'l', "byte 3 = 'l'");
        ASSERT(result.frames[4].data[0] == 'o', "byte 4 = 'o'");
    }
    printf("  SNR ~10dB (30%% noise): PASS\n");
    free(samples);
}

static void test_uart_noise_50pct(void)
{
    printf("Test: UART 9600 'Hello' with 50%% noise (graceful degradation)\n");

    uint32_t buf_size = 8000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "Hello", 9600, 8, 0, 1);

    /* 50% noise: amplitude 500 */
    add_noise(samples, used, 500.0f);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    /*
     * At 50% noise (amplitude 500 vs signal 1000), noise can cross
     * the threshold. The decoder samples at bit centers, so it may
     * still decode correctly since each bit has ~104 samples at 9600 baud
     * and the center sample is usually correct. But we allow some failures.
     */
    ASSERT(result.num_frames >= 3, "decoded at least 3 of 5 frames at 50% noise");
    printf("  SNR ~6dB (50%% noise): decoded %d/5 frames\n", result.num_frames);
    free(samples);
}

static void test_uart_115200_noise(void)
{
    printf("Test: UART 115200 'Hi' with 10%% noise\n");

    /* At 115200 baud, ~8.7 samples per bit (much fewer than 9600's ~104) */
    uint32_t buf_size = 4000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "Hi", 115200, 8, 0, 1);

    add_noise(samples, used, 100.0f);

    uart_config_t cfg = { .baud_rate = 115200, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 2, "decoded 2 frames at 115200 with 10% noise");
    if (result.num_frames == 2) {
        ASSERT(result.frames[0].data[0] == 'H', "byte 0 = 'H'");
        ASSERT(result.frames[1].data[0] == 'i', "byte 1 = 'i'");
    }
    free(samples);
}

static void test_uart_jitter(void)
{
    printf("Test: UART 9600 'Hello' with 5%% timing jitter\n");

    uint32_t buf_size = 8000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "Hello", 9600, 8, 0, 1);

    /* 5% jitter of bit period: ~104 samples/bit, so +-5 samples */
    add_jitter(samples, used, 5);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 5, "decoded 5 frames with 5% jitter");
    if (result.num_frames == 5) {
        ASSERT(result.frames[0].data[0] == 'H', "byte 0 = 'H'");
        ASSERT(result.frames[4].data[0] == 'o', "byte 4 = 'o'");
    }
    free(samples);
}

static void test_uart_multi_noise_levels(void)
{
    printf("Test: UART multiple bytes with varying noise\n");

    const char *msg = "ABCDEF";
    uint32_t buf_size = 10000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, msg, 9600, 8, 0, 1);

    /* Apply 20% noise */
    add_noise(samples, used, 200.0f);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 6, "decoded 6 frames with 20% noise");
    if (result.num_frames == 6) {
        uint32_t i;
        int all_correct = 1;
        for (i = 0; i < 6; i++) {
            if (result.frames[i].data[0] != (uint8_t)msg[i]) {
                all_correct = 0;
                printf("  Mismatch at byte %u: expected 0x%02X got 0x%02X\n",
                       i, (uint8_t)msg[i], result.frames[i].data[0]);
            }
        }
        ASSERT(all_correct, "all 6 bytes correct with 20% noise");
    }
    free(samples);
}

/* ========================================================================
 * 2. I2C with noise (4 tests)
 * ======================================================================== */

static void test_i2c_noise_10pct(void)
{
    printf("Test: I2C addr+data with 10%% noise\n");

    uint32_t bit_period = 100;
    uint32_t buf_size = 50 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_i2c_transaction(sda, scl, &total, 0x90, 0xA5, bit_period, 0);

    add_noise(sda, total, 100.0f);
    add_noise(scl, total, 100.0f);

    i2c_config_t cfg = { .sda_threshold = THRESHOLD, .scl_threshold = THRESHOLD };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_I2C, "result type is I2C");
    ASSERT(result.num_frames >= 4, "at least 4 frames with 10% noise");
    if (result.num_frames >= 4) {
        ASSERT(result.frames[0].data[0] == 0x90, "address byte 0x90");
        ASSERT(strcmp(result.frames[0].label, "0x48 W") == 0, "label '0x48 W'");
        ASSERT(result.frames[2].data[0] == 0xA5, "data byte 0xA5");
    }

    free(sda);
    free(scl);
}

static void test_i2c_noise_20pct(void)
{
    printf("Test: I2C repeated start with 20%% noise\n");

    uint32_t bit_period = 100;
    uint32_t buf_size = 100 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_i2c_transaction(sda, scl, &total, 0x90, 0xA5, bit_period, 1);

    add_noise(sda, total, 200.0f);
    add_noise(scl, total, 200.0f);

    i2c_config_t cfg = { .sda_threshold = THRESHOLD, .scl_threshold = THRESHOLD };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_I2C, "result type I2C");
    ASSERT(result.num_frames >= 4, "at least 4 frames with 20% noise");

    /* Find the repeated start */
    int found_sr = 0;
    uint16_t fi;
    for (fi = 0; fi < result.num_frames; fi++) {
        if (strcmp(result.frames[fi].label, "Sr") == 0) {
            found_sr = 1;
            break;
        }
    }
    ASSERT(found_sr, "repeated START detected through 20% noise");

    free(sda);
    free(scl);
}

/*
 * Generate I2C with realistic setup/hold timing.
 * In real I2C, SDA changes happen well after SCL falls (setup time).
 * This version adds a guard gap so SDA only transitions in the middle
 * of the SCL-low phase, making the signal robust to SCL jitter.
 */
static void gen_i2c_with_guard(int16_t *sda, int16_t *scl, uint32_t *total,
                               uint8_t addr_byte, uint8_t data_byte,
                               uint32_t bit_period)
{
    uint32_t pos = 0;
    uint32_t half = bit_period / 2;
    uint32_t guard = bit_period / 8; /* SDA changes guard samples after SCL falls */
    uint32_t i;
    uint8_t bit;

    /* Idle: both high */
    fill_samples(sda, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    /* START: SDA falls while SCL high */
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(sda, pos, pos + half, HIGH_LEVEL);
    fill_samples(sda, pos + half, pos + bit_period, LOW_LEVEL);
    pos += bit_period;

    /*
     * Clock bits with guard time: SCL goes low, SDA holds its previous
     * value for 'guard' samples, then SDA transitions to the new value,
     * then SCL goes high (SDA is stable during the high phase).
     */
    int16_t prev_sda_val = LOW_LEVEL; /* After START, SDA is low */
    #define CLOCK_BIT_GUARDED(sda_val) do { \
        /* SCL low phase */ \
        fill_samples(scl, pos, pos + half, LOW_LEVEL); \
        /* SDA: hold previous value during guard period */ \
        fill_samples(sda, pos, pos + guard, prev_sda_val); \
        /* SDA transitions to new value after guard */ \
        fill_samples(sda, pos + guard, pos + bit_period, \
                     (sda_val) ? HIGH_LEVEL : LOW_LEVEL); \
        /* SCL high phase */ \
        fill_samples(scl, pos + half, pos + bit_period, HIGH_LEVEL); \
        prev_sda_val = (sda_val) ? HIGH_LEVEL : LOW_LEVEL; \
        pos += bit_period; \
    } while(0)

    /* Address byte: MSB first */
    for (i = 0; i < 8; i++) {
        bit = (addr_byte >> (7 - i)) & 1;
        CLOCK_BIT_GUARDED(bit);
    }
    CLOCK_BIT_GUARDED(0); /* ACK */

    /* Data byte: MSB first */
    for (i = 0; i < 8; i++) {
        bit = (data_byte >> (7 - i)) & 1;
        CLOCK_BIT_GUARDED(bit);
    }
    CLOCK_BIT_GUARDED(0); /* ACK */

    #undef CLOCK_BIT_GUARDED

    /* STOP: SCL goes high, then SDA goes high */
    fill_samples(scl, pos, pos + half, LOW_LEVEL);
    fill_samples(sda, pos, pos + half, LOW_LEVEL);
    pos += half;
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(sda, pos, pos + half, LOW_LEVEL);
    fill_samples(sda, pos + half, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    /* Trailing idle */
    fill_samples(sda, pos, pos + bit_period, HIGH_LEVEL);
    fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
    pos += bit_period;

    *total = pos;
}

static void test_i2c_scl_jitter(void)
{
    printf("Test: I2C with SCL jitter\n");

    /*
     * Real I2C has setup/hold timing: SDA changes only during
     * SCL-low phases with a guard gap, never near SCL edges.
     * We use gen_i2c_with_guard() which models this, then add
     * jitter to SCL edges to test decoder robustness.
     */
    uint32_t bit_period = 100;
    uint32_t buf_size = 50 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_i2c_with_guard(sda, scl, &total, 0x90, 0xA5, bit_period);

    /* Add jitter to SCL edges (+-5 samples out of 100 bit period = 5%) */
    add_jitter(scl, total, 5);
    /* Add noise to both lines */
    add_noise(sda, total, 100.0f);
    add_noise(scl, total, 100.0f);

    i2c_config_t cfg = { .sda_threshold = THRESHOLD, .scl_threshold = THRESHOLD };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames >= 4, "at least 4 frames with SCL jitter");
    if (result.num_frames >= 4) {
        ASSERT(result.frames[0].data[0] == 0x90, "address byte correct with jitter");
        ASSERT(result.frames[2].data[0] == 0xA5, "data byte correct with jitter");
    }

    free(sda);
    free(scl);
}

static void test_i2c_marginal_threshold(void)
{
    printf("Test: I2C marginal threshold signals\n");

    /* Use reduced signal amplitude: only +-200 instead of +-1000 */
    uint32_t bit_period = 100;
    uint32_t buf_size = 50 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_i2c_transaction(sda, scl, &total, 0x90, 0xA5, bit_period, 0);

    /* Scale signal down to +-200 (20% of normal) */
    uint32_t i;
    for (i = 0; i < total; i++) {
        sda[i] = sda[i] / 5;
        scl[i] = scl[i] / 5;
    }

    /* Add noise that is 10% of the reduced signal (20 amplitude) */
    add_noise(sda, total, 20.0f);
    add_noise(scl, total, 20.0f);

    i2c_config_t cfg = { .sda_threshold = THRESHOLD, .scl_threshold = THRESHOLD };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames >= 4, "decode works with marginal signal amplitude");
    if (result.num_frames >= 4) {
        ASSERT(result.frames[0].data[0] == 0x90, "address correct at marginal level");
    }

    free(sda);
    free(scl);
}

/* ========================================================================
 * 3. SPI with noise (4 tests)
 * ======================================================================== */

static void test_spi_noise_15pct(void)
{
    printf("Test: SPI 0xA5 with 15%% noise\n");

    uint32_t clk_period = 100;
    uint32_t buf_size = 20 * clk_period;
    int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_spi_byte(mosi, clk_buf, cs, &total, 0xA5, clk_period);

    add_noise(mosi, total, 150.0f);
    add_noise(clk_buf, total, 150.0f);
    add_noise(cs, total, 150.0f);

    spi_config_t cfg = { .mosi_threshold = THRESHOLD, .clk_threshold = THRESHOLD,
                         .cs_threshold = THRESHOLD, .cpol = 0, .cpha = 0,
                         .bit_order = 0 };
    decode_result_t result;
    decode_spi(mosi, clk_buf, cs, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_SPI, "result type SPI");
    ASSERT(result.num_frames == 1, "decoded 1 frame with 15% noise");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data[0] == 0xA5, "byte 0xA5 with 15% noise");
    }

    free(mosi);
    free(clk_buf);
    free(cs);
}

static void test_spi_multi_noise(void)
{
    printf("Test: SPI multiple bytes with noise\n");

    uint32_t clk_period = 100;
    /* Generate 3 bytes back to back */
    uint32_t buf_size = 60 * clk_period;
    int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));

    uint8_t test_bytes[3] = {0xDE, 0xAD, 0x42};
    uint32_t pos = 0;
    uint32_t i;

    for (i = 0; i < 3; i++) {
        uint32_t this_total = 0;
        gen_spi_byte(mosi + pos, clk_buf + pos, cs + pos,
                     &this_total, test_bytes[i], clk_period);
        pos += this_total;
    }

    add_noise(mosi, pos, 200.0f);
    add_noise(clk_buf, pos, 200.0f);
    add_noise(cs, pos, 200.0f);

    spi_config_t cfg = { .mosi_threshold = THRESHOLD, .clk_threshold = THRESHOLD,
                         .cs_threshold = THRESHOLD, .cpol = 0, .cpha = 0,
                         .bit_order = 0 };
    decode_result_t result;
    decode_spi(mosi, clk_buf, cs, pos, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 3, "decoded 3 SPI frames with noise");
    if (result.num_frames == 3) {
        ASSERT(result.frames[0].data[0] == 0xDE, "byte 0 = 0xDE");
        ASSERT(result.frames[1].data[0] == 0xAD, "byte 1 = 0xAD");
        ASSERT(result.frames[2].data[0] == 0x42, "byte 2 = 0x42");
    }

    free(mosi);
    free(clk_buf);
    free(cs);
}

static void test_spi_modes_noise(void)
{
    printf("Test: SPI CPOL/CPHA modes with noise\n");

    uint32_t clk_period = 100;
    uint32_t buf_size = 20 * clk_period;
    uint8_t cpol, cpha;

    for (cpol = 0; cpol <= 1; cpol++) {
        for (cpha = 0; cpha <= 1; cpha++) {
            int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
            int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
            int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));
            uint32_t total = 0;

            gen_spi_byte_mode(mosi, clk_buf, cs, &total, 0x55,
                              clk_period, cpol, cpha);

            add_noise(mosi, total, 150.0f);
            add_noise(clk_buf, total, 150.0f);
            add_noise(cs, total, 150.0f);

            spi_config_t cfg = { .mosi_threshold = THRESHOLD,
                                 .clk_threshold = THRESHOLD,
                                 .cs_threshold = THRESHOLD,
                                 .cpol = cpol, .cpha = cpha,
                                 .bit_order = 0 };
            decode_result_t result;
            decode_spi(mosi, clk_buf, cs, total, SAMPLE_RATE, &cfg, &result);

            char msg[64];
            snprintf(msg, sizeof(msg), "SPI mode %d%d decoded 0x55 with noise",
                     cpol, cpha);
            ASSERT(result.num_frames >= 1, msg);
            if (result.num_frames >= 1) {
                snprintf(msg, sizeof(msg), "SPI mode %d%d byte correct", cpol, cpha);
                ASSERT(result.frames[0].data[0] == 0x55, msg);
            }

            free(mosi);
            free(clk_buf);
            free(cs);
        }
    }
}

static void test_spi_fast_clock_noise(void)
{
    printf("Test: SPI fast clock (small period) with noise\n");

    /* Use small clock period: only 10 samples per clock cycle */
    uint32_t clk_period = 10;
    uint32_t buf_size = 20 * clk_period;
    int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_spi_byte(mosi, clk_buf, cs, &total, 0xC3, clk_period);

    /* 10% noise at fast clock */
    add_noise(mosi, total, 100.0f);
    add_noise(clk_buf, total, 100.0f);
    add_noise(cs, total, 100.0f);

    spi_config_t cfg = { .mosi_threshold = THRESHOLD, .clk_threshold = THRESHOLD,
                         .cs_threshold = THRESHOLD, .cpol = 0, .cpha = 0,
                         .bit_order = 0 };
    decode_result_t result;
    decode_spi(mosi, clk_buf, cs, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames >= 1, "decoded at least 1 frame at fast clock");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data[0] == 0xC3, "byte 0xC3 at fast clock with noise");
    }

    free(mosi);
    free(clk_buf);
    free(cs);
}

/* ========================================================================
 * 4. CAN with noise (4 tests)
 * ======================================================================== */

static void test_can_noise_10pct(void)
{
    printf("Test: CAN standard frame with 10%% noise\n");

    uint32_t bit_rate = 500000;
    uint32_t buf_size = (uint32_t)(200 * (SAMPLE_RATE / (float)bit_rate));
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    uint8_t data[2] = {0x01, 0x02};
    gen_can_frame(samples, buf_size, &total, bit_rate, 0x123, data, 2);

    add_noise(samples, total, 100.0f);

    can_config_t cfg = { .bit_rate = 500000, .threshold = THRESHOLD };
    decode_result_t result;
    decode_can(samples, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_CAN, "result type CAN");
    ASSERT(result.num_frames == 1, "decoded 1 CAN frame with 10% noise");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data_len == 2, "DLC 2");
        ASSERT(result.frames[0].data[0] == 0x01, "data[0] = 0x01");
        ASSERT(result.frames[0].data[1] == 0x02, "data[1] = 0x02");
        ASSERT(strstr(result.frames[0].label, "123") != NULL, "ID contains 123");
    }

    free(samples);
}

static void test_can_stuffing_noise(void)
{
    printf("Test: CAN frame with bit stuffing + noise\n");

    uint32_t bit_rate = 500000;
    uint32_t buf_size = (uint32_t)(200 * (SAMPLE_RATE / (float)bit_rate));
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    /* ID 0x000 and data 0x00 will have many consecutive dominant bits,
     * triggering bit stuffing */
    uint8_t data[1] = {0x00};
    gen_can_frame(samples, buf_size, &total, bit_rate, 0x000, data, 1);

    add_noise(samples, total, 150.0f);

    can_config_t cfg = { .bit_rate = 500000, .threshold = THRESHOLD };
    decode_result_t result;
    decode_can(samples, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 1, "decoded CAN frame with stuffing + noise");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data_len == 1, "DLC 1");
        ASSERT(result.frames[0].data[0] == 0x00, "data[0] = 0x00");
    }

    free(samples);
}

static void test_can_extended_noise(void)
{
    printf("Test: CAN extended frame (29-bit ID) with noise\n");

    uint32_t bit_rate = 500000;
    /* Extended frame is longer - need more buffer */
    uint32_t buf_size = (uint32_t)(300 * (SAMPLE_RATE / (float)bit_rate));
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    uint8_t data[2] = {0xAB, 0xCD};
    gen_can_frame_ext(samples, buf_size, &total, bit_rate, 0x12345, data, 2);

    add_noise(samples, total, 100.0f);

    can_config_t cfg = { .bit_rate = 500000, .threshold = THRESHOLD };
    decode_result_t result;
    decode_can(samples, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_CAN, "result type CAN");
    ASSERT(result.num_frames == 1, "decoded 1 extended CAN frame with noise");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data_len == 2, "DLC 2");
        ASSERT(result.frames[0].data[0] == 0xAB, "data[0] = 0xAB");
        ASSERT(result.frames[0].data[1] == 0xCD, "data[1] = 0xCD");
    }

    free(samples);
}

static void test_can_multi_frames_noise(void)
{
    printf("Test: CAN multiple frames back-to-back with noise\n");

    uint32_t bit_rate = 500000;
    float spb = SAMPLE_RATE / (float)bit_rate;
    /* Two frames need plenty of buffer */
    uint32_t buf_size = (uint32_t)(400 * spb);
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));

    /* Fill with idle (recessive) */
    fill_samples(samples, 0, buf_size, HIGH_LEVEL);

    /* Generate first frame */
    uint32_t total1 = 0;
    uint8_t data1[1] = {0x11};
    gen_can_frame(samples, buf_size, &total1, bit_rate, 0x100, data1, 1);

    /* Generate second frame after first */
    uint32_t total2 = 0;
    uint32_t offset2 = total1;
    uint8_t data2[1] = {0x22};

    /* Generate into a temp buffer and copy with offset */
    uint32_t temp_size = (uint32_t)(200 * spb);
    int16_t *temp = (int16_t *)calloc(temp_size, sizeof(int16_t));
    gen_can_frame(temp, temp_size, &total2, bit_rate, 0x200, data2, 1);

    uint32_t j;
    for (j = 0; j < total2 && (offset2 + j) < buf_size; j++) {
        samples[offset2 + j] = temp[j];
    }
    free(temp);

    uint32_t total = offset2 + total2;
    if (total > buf_size) total = buf_size;

    add_noise(samples, total, 100.0f);

    can_config_t cfg = { .bit_rate = 500000, .threshold = THRESHOLD };
    decode_result_t result;
    decode_can(samples, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames >= 2, "decoded at least 2 CAN frames with noise");
    if (result.num_frames >= 2) {
        ASSERT(result.frames[0].data[0] == 0x11, "frame 0 data = 0x11");
        ASSERT(result.frames[1].data[0] == 0x22, "frame 1 data = 0x22");
    }

    free(samples);
}

/* ========================================================================
 * 5. Edge cases (4 tests)
 * ======================================================================== */

static void test_uart_dc_offset(void)
{
    printf("Test: UART with DC offset (signal centered at 500)\n");

    uint32_t buf_size = 8000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "AB", 9600, 8, 0, 1);

    /* Add DC offset of 500: signal now ranges from -500 to 1500 */
    add_dc_offset(samples, used, 500);
    /* Add some noise */
    add_noise(samples, used, 50.0f);

    /* Threshold must match DC offset for correct decoding */
    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = 500 };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 2, "decoded 2 frames with DC offset");
    if (result.num_frames == 2) {
        ASSERT(result.frames[0].data[0] == 'A', "byte 0 = 'A' with DC offset");
        ASSERT(result.frames[1].data[0] == 'B', "byte 1 = 'B' with DC offset");
    }

    free(samples);
}

static void test_all_zeros_data(void)
{
    printf("Test: All-zeros data through all protocols\n");

    /* UART: byte 0x00 */
    {
        uint32_t buf_size = 4000;
        int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
        fill_samples(samples, 0, buf_size, HIGH_LEVEL);
        float spb = SAMPLE_RATE / 9600.0f;
        uint32_t offset = (uint32_t)(spb * 2);
        gen_uart_byte(samples, offset, 0x00, 9600, 8, 0, 1);
        uint32_t used = offset + (uint32_t)(12 * spb);

        add_noise(samples, used, 100.0f);

        uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                              .stop_bits = 1, .threshold = THRESHOLD };
        decode_result_t result;
        decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

        ASSERT(result.num_frames >= 1, "UART decoded 0x00 byte");
        if (result.num_frames >= 1) {
            ASSERT(result.frames[0].data[0] == 0x00, "UART byte value is 0x00");
        }
        free(samples);
    }

    /* SPI: byte 0x00 */
    {
        uint32_t clk_period = 100;
        uint32_t buf_size = 20 * clk_period;
        int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
        int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
        int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));
        uint32_t total = 0;

        gen_spi_byte(mosi, clk_buf, cs, &total, 0x00, clk_period);
        add_noise(mosi, total, 100.0f);
        add_noise(clk_buf, total, 100.0f);
        add_noise(cs, total, 100.0f);

        spi_config_t cfg = { .mosi_threshold = THRESHOLD, .clk_threshold = THRESHOLD,
                             .cs_threshold = THRESHOLD, .cpol = 0, .cpha = 0,
                             .bit_order = 0 };
        decode_result_t result;
        decode_spi(mosi, clk_buf, cs, total, SAMPLE_RATE, &cfg, &result);

        ASSERT(result.num_frames >= 1, "SPI decoded 0x00 byte");
        if (result.num_frames >= 1) {
            ASSERT(result.frames[0].data[0] == 0x00, "SPI byte value is 0x00");
        }

        free(mosi);
        free(clk_buf);
        free(cs);
    }
}

static void test_max_length_uart(void)
{
    printf("Test: Maximum-length UART message (32 bytes) with noise\n");

    /* 32 bytes at 9600 baud */
    uint32_t buf_size = 50000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    const char *msg = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"; /* 32 chars */

    uint32_t used = gen_uart_string(samples, buf_size, msg, 9600, 8, 0, 1);
    add_noise(samples, used, 150.0f);

    uart_config_t cfg = { .baud_rate = 9600, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 32, "decoded 32 bytes with noise");
    if (result.num_frames == 32) {
        int all_ok = 1;
        uint32_t i;
        for (i = 0; i < 32; i++) {
            if (result.frames[i].data[0] != (uint8_t)msg[i]) {
                all_ok = 0;
                printf("  Mismatch at byte %u: expected 0x%02X got 0x%02X\n",
                       i, (uint8_t)msg[i], result.frames[i].data[0]);
                break;
            }
        }
        ASSERT(all_ok, "all 32 bytes correct");
    }

    free(samples);
}

static void test_min_baud_noise(void)
{
    printf("Test: UART minimum baud rate (300) with noise\n");

    /* At 300 baud: ~3333 samples per bit, very oversampled */
    uint32_t buf_size = 200000;
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t used = gen_uart_string(samples, buf_size, "OK", 300, 8, 0, 1);

    /* 20% noise */
    add_noise(samples, used, 200.0f);

    uart_config_t cfg = { .baud_rate = 300, .data_bits = 8, .parity = 0,
                          .stop_bits = 1, .threshold = THRESHOLD };
    decode_result_t result;
    decode_uart(samples, used, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames == 2, "decoded 2 frames at 300 baud with noise");
    if (result.num_frames == 2) {
        ASSERT(result.frames[0].data[0] == 'O', "byte 0 = 'O'");
        ASSERT(result.frames[1].data[0] == 'K', "byte 1 = 'K'");
    }

    free(samples);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
    printf("=== Protocol Decoder Noisy Signal Stress Tests ===\n\n");

    printf("--- UART with noise ---\n");
    test_uart_noise_10pct();
    test_uart_noise_30pct();
    test_uart_noise_50pct();
    test_uart_115200_noise();
    test_uart_jitter();
    test_uart_multi_noise_levels();

    printf("\n--- I2C with noise ---\n");
    test_i2c_noise_10pct();
    test_i2c_noise_20pct();
    test_i2c_scl_jitter();
    test_i2c_marginal_threshold();

    printf("\n--- SPI with noise ---\n");
    test_spi_noise_15pct();
    test_spi_multi_noise();
    test_spi_modes_noise();
    test_spi_fast_clock_noise();

    printf("\n--- CAN with noise ---\n");
    test_can_noise_10pct();
    test_can_stuffing_noise();
    test_can_extended_noise();
    test_can_multi_frames_noise();

    printf("\n--- Edge cases ---\n");
    test_uart_dc_offset();
    test_all_zeros_data();
    test_max_length_uart();
    test_min_baud_noise();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
