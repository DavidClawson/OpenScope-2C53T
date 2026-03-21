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
 * Signal generation helpers
 * ======================================================================== */

#define SAMPLE_RATE 1000000.0f  /* 1 MHz sample rate */
#define HIGH_LEVEL  1000
#define LOW_LEVEL  (-1000)
#define THRESHOLD   0

/* Fill a range of samples with a level */
static void fill_samples(int16_t *buf, uint32_t start, uint32_t end, int16_t level)
{
    uint32_t i;
    for (i = start; i < end; i++) {
        buf[i] = level;
    }
}

/*
 * Generate UART samples for a single byte.
 * Returns number of samples written.
 */
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
            /* Odd parity: set parity bit so total ones is odd */
            pbit = (ones % 2 == 0) ? 1 : 0;
        } else {
            /* Even parity: set parity bit so total ones is even */
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

/*
 * Generate I2C signal: SDA and SCL for a complete transaction.
 * Transaction: START, address_byte + ACK, data_byte + ACK, STOP
 *
 * bit_period: samples per bit period
 */
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

    /* Helper macro: clock one bit on SDA */
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

    /* ACK (SDA low during 9th clock) */
    CLOCK_BIT(0);

    /* Data byte: MSB first, 8 bits */
    for (i = 0; i < 8; i++) {
        bit = (data_byte >> (7 - i)) & 1;
        CLOCK_BIT(bit);
    }

    /* ACK */
    CLOCK_BIT(0);

    if (repeated_start) {
        /* Repeated START: SDA goes high while SCL low, then falls while SCL high */
        fill_samples(scl, pos, pos + half, LOW_LEVEL);
        fill_samples(sda, pos, pos + half, HIGH_LEVEL);
        pos += half;
        fill_samples(scl, pos, pos + bit_period, HIGH_LEVEL);
        fill_samples(sda, pos, pos + half, HIGH_LEVEL);
        fill_samples(sda, pos + half, pos + bit_period, LOW_LEVEL);
        pos += bit_period;

        /* Second address byte (read) */
        uint8_t addr_read = addr_byte | 0x01;
        for (i = 0; i < 8; i++) {
            bit = (addr_read >> (7 - i)) & 1;
            CLOCK_BIT(bit);
        }

        /* ACK */
        CLOCK_BIT(0);

        /* Another data byte */
        for (i = 0; i < 8; i++) {
            bit = (data_byte >> (7 - i)) & 1;
            CLOCK_BIT(bit);
        }

        /* NAK (master NAK before STOP) */
        CLOCK_BIT(1);
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

/*
 * Generate SPI signals for one byte.
 * CPOL=0, CPHA=0, MSB first by default.
 */
static void gen_spi_byte(int16_t *mosi, int16_t *clk, int16_t *cs,
                         uint32_t *total, uint8_t byte_val,
                         uint32_t clk_period)
{
    uint32_t pos = 0;
    uint32_t half = clk_period / 2;
    uint8_t i;

    /* Idle: CS high, CLK low (CPOL=0) */
    fill_samples(cs, pos, pos + clk_period, HIGH_LEVEL);
    fill_samples(clk, pos, pos + clk_period, LOW_LEVEL);
    fill_samples(mosi, pos, pos + clk_period, LOW_LEVEL);
    pos += clk_period;

    /* CS goes low */
    fill_samples(cs, pos, pos + half, LOW_LEVEL);
    fill_samples(clk, pos, pos + half, LOW_LEVEL);
    fill_samples(mosi, pos, pos + half, LOW_LEVEL);
    pos += half;

    /* 8 bits, MSB first, CPOL=0 CPHA=0: data valid on rising edge */
    for (i = 0; i < 8; i++) {
        int bit = (byte_val >> (7 - i)) & 1;

        /* Setup data on falling/idle edge */
        fill_samples(mosi, pos, pos + clk_period, bit ? HIGH_LEVEL : LOW_LEVEL);

        /* Clock: low half, then high half (rising edge in middle) */
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

/*
 * Generate CAN standard frame samples.
 * Includes bit stuffing.
 */
static void gen_can_frame(int16_t *buf, uint32_t buf_size, uint32_t *total,
                          uint32_t bit_rate, uint16_t can_id,
                          const uint8_t *data, uint8_t dlc)
{
    float spb = SAMPLE_RATE / (float)bit_rate;
    uint32_t raw_bit = 0;
    uint32_t consecutive = 0;
    int last_val = -1;
    uint32_t i;

    /* Start with idle (recessive) */
    uint32_t idle_samples = (uint32_t)(10 * spb);
    fill_samples(buf, 0, idle_samples, HIGH_LEVEL);
    uint32_t bit_start = (uint32_t)(10 * spb); /* frame starts after idle */

    /* Helper: write one raw bit with stuffing */
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

    /* SOF (dominant) */
    WRITE_BIT(0);

    /* 11-bit ID, MSB first */
    for (i = 10; i < 11; i--) {
        WRITE_BIT((can_id >> i) & 1);
        if (i == 0) break;
    }

    /* RTR = 0 (data frame) */
    WRITE_BIT(0);

    /* IDE = 0 (standard frame) */
    WRITE_BIT(0);

    /* r0 = 0 (reserved) */
    WRITE_BIT(0);

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

    /* CRC: 15 bits - simplified, just write zeros for testing
     * (A real implementation would compute CRC-15) */
    /* Actually, let's compute the CRC properly so the decoder can verify later */
    /* For now, just write 15 zero bits - decoder doesn't verify CRC yet */
    for (i = 0; i < 15; i++) {
        WRITE_BIT(0);
    }

    /* Disable stuffing for remaining fields */
    stuffing_enabled = 0;

    /* CRC delimiter (recessive) */
    {
        uint32_t s = bit_start + (uint32_t)(raw_bit * spb);
        uint32_t e = bit_start + (uint32_t)((raw_bit + 1) * spb);
        if (e > buf_size) e = buf_size;
        fill_samples(buf, s, e, HIGH_LEVEL);
        raw_bit++;
    }

    /* ACK slot (dominant = ACK received) */
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

/* ========================================================================
 * Tests
 * ======================================================================== */

static void test_uart_hello(void)
{
    printf("Test: UART decode 'Hello' at 9600 baud\n");

    const char *msg = "Hello";
    uint32_t baud = 9600;
    float spb = SAMPLE_RATE / (float)baud;
    /* Each byte: 1 start + 8 data + 1 stop = 10 bits */
    uint32_t bytes_per_char = (uint32_t)(10 * spb) + 1;
    uint32_t total_size = (uint32_t)(spb * 2) + bytes_per_char * 5 + (uint32_t)(spb * 2);

    int16_t *samples = (int16_t *)calloc(total_size, sizeof(int16_t));

    /* Fill with idle (high) */
    fill_samples(samples, 0, total_size, HIGH_LEVEL);

    /* Generate each character */
    uint32_t offset = (uint32_t)(spb * 2); /* Start after some idle */
    uint32_t i;
    for (i = 0; i < 5; i++) {
        uint32_t written = gen_uart_byte(samples, offset, (uint8_t)msg[i],
                                         baud, 8, 0, 1);
        offset += written;
    }

    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = 8,
        .parity = 0,
        .stop_bits = 1,
        .threshold = THRESHOLD
    };
    decode_result_t result;
    decode_uart(samples, total_size, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_UART, "result type is UART");
    ASSERT(result.num_frames == 5, "decoded 5 frames");

    if (result.num_frames == 5) {
        ASSERT(result.frames[0].data[0] == 'H', "first byte is 'H'");
        ASSERT(result.frames[1].data[0] == 'e', "second byte is 'e'");
        ASSERT(result.frames[2].data[0] == 'l', "third byte is 'l'");
        ASSERT(result.frames[3].data[0] == 'l', "fourth byte is 'l'");
        ASSERT(result.frames[4].data[0] == 'o', "fifth byte is 'o'");

        /* Check labels */
        ASSERT(strcmp(result.frames[0].label, "0x48") == 0, "H label is 0x48");

        /* No errors */
        for (i = 0; i < 5; i++) {
            ASSERT((result.frames[i].flags & DECODE_FLAG_ERROR) == 0,
                   "no framing error");
        }
    }

    free(samples);
}

static void test_uart_framing_error(void)
{
    printf("Test: UART framing error detection\n");

    uint32_t baud = 9600;
    float spb = SAMPLE_RATE / (float)baud;
    uint32_t total_size = (uint32_t)(20 * spb);
    int16_t *samples = (int16_t *)calloc(total_size, sizeof(int16_t));

    /* Fill with idle */
    fill_samples(samples, 0, total_size, HIGH_LEVEL);

    /* Generate a byte but corrupt the stop bit */
    uint32_t offset = (uint32_t)(2 * spb);
    gen_uart_byte(samples, offset, 0x55, baud, 8, 0, 1);

    /* Corrupt stop bit: overwrite it with low */
    uint32_t stop_start = offset + (uint32_t)(9 * spb);
    uint32_t stop_end = offset + (uint32_t)(10 * spb);
    fill_samples(samples, stop_start, stop_end, LOW_LEVEL);

    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = 8,
        .parity = 0,
        .stop_bits = 1,
        .threshold = THRESHOLD
    };
    decode_result_t result;
    decode_uart(samples, total_size, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.num_frames >= 1, "decoded at least 1 frame");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data[0] == 0x55, "data byte is 0x55");
        ASSERT((result.frames[0].flags & DECODE_FLAG_ERROR) != 0,
               "framing error flag set");
    }

    free(samples);
}

static void test_i2c_basic(void)
{
    printf("Test: I2C address write + data byte\n");

    uint32_t bit_period = 100; /* 100 samples per bit */
    uint32_t buf_size = 50 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    /* Address 0x48, write (bit0=0), so address byte = 0x90 */
    /* Data byte: 0xA5 */
    gen_i2c_transaction(sda, scl, &total, 0x90, 0xA5, bit_period, 0);

    i2c_config_t cfg = {
        .sda_threshold = THRESHOLD,
        .scl_threshold = THRESHOLD
    };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_I2C, "result type is I2C");
    /* Expect: address frame, ACK, data frame, ACK */
    ASSERT(result.num_frames >= 4, "at least 4 frames (addr, ACK, data, ACK)");

    if (result.num_frames >= 4) {
        /* First frame: address byte 0x90 => addr 0x48 W */
        ASSERT(result.frames[0].data[0] == 0x90, "address byte is 0x90");
        ASSERT(strcmp(result.frames[0].label, "0x48 W") == 0,
               "address label is '0x48 W'");

        /* Second frame: ACK */
        ASSERT(strcmp(result.frames[1].label, "ACK") == 0, "ACK after address");

        /* Third frame: data byte 0xA5 */
        ASSERT(result.frames[2].data[0] == 0xA5, "data byte is 0xA5");
        ASSERT(strcmp(result.frames[2].label, "0xA5") == 0,
               "data label is '0xA5'");

        /* Fourth frame: ACK */
        ASSERT(strcmp(result.frames[3].label, "ACK") == 0, "ACK after data");
    }

    free(sda);
    free(scl);
}

static void test_i2c_repeated_start(void)
{
    printf("Test: I2C repeated START detection\n");

    uint32_t bit_period = 100;
    uint32_t buf_size = 100 * bit_period;
    int16_t *sda = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *scl = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    /* Generate with repeated start */
    gen_i2c_transaction(sda, scl, &total, 0x90, 0xA5, bit_period, 1);

    i2c_config_t cfg = {
        .sda_threshold = THRESHOLD,
        .scl_threshold = THRESHOLD
    };
    decode_result_t result;
    decode_i2c(sda, scl, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_I2C, "result type is I2C");

    /* Look for "Sr" frame (repeated start) */
    int found_sr = 0;
    uint16_t fi;
    for (fi = 0; fi < result.num_frames; fi++) {
        if (strcmp(result.frames[fi].label, "Sr") == 0) {
            found_sr = 1;
            break;
        }
    }
    ASSERT(found_sr, "repeated START (Sr) detected");

    /* Look for a read address frame */
    int found_read = 0;
    for (fi = 0; fi < result.num_frames; fi++) {
        if (strcmp(result.frames[fi].label, "0x48 R") == 0) {
            found_read = 1;
            break;
        }
    }
    ASSERT(found_read, "read address (0x48 R) detected after repeated START");

    free(sda);
    free(scl);
}

static void test_spi_byte(void)
{
    printf("Test: SPI decode 0xA5\n");

    uint32_t clk_period = 100;
    uint32_t buf_size = 20 * clk_period;
    int16_t *mosi = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *clk_buf = (int16_t *)calloc(buf_size, sizeof(int16_t));
    int16_t *cs = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    gen_spi_byte(mosi, clk_buf, cs, &total, 0xA5, clk_period);

    spi_config_t cfg = {
        .mosi_threshold = THRESHOLD,
        .clk_threshold = THRESHOLD,
        .cs_threshold = THRESHOLD,
        .cpol = 0,
        .cpha = 0,
        .bit_order = 0 /* MSB first */
    };
    decode_result_t result;
    decode_spi(mosi, clk_buf, cs, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_SPI, "result type is SPI");
    ASSERT(result.num_frames == 1, "decoded 1 frame");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data[0] == 0xA5, "decoded byte is 0xA5");
        ASSERT(strcmp(result.frames[0].label, "0xA5") == 0, "label is '0xA5'");
    }

    free(mosi);
    free(clk_buf);
    free(cs);
}

static void test_can_standard(void)
{
    printf("Test: CAN standard frame ID=0x123 DLC=2 data=[0x01,0x02]\n");

    uint32_t bit_rate = 500000;
    uint32_t buf_size = (uint32_t)(200 * (SAMPLE_RATE / (float)bit_rate));
    int16_t *samples = (int16_t *)calloc(buf_size, sizeof(int16_t));
    uint32_t total = 0;

    uint8_t data[2] = {0x01, 0x02};
    gen_can_frame(samples, buf_size, &total, bit_rate, 0x123, data, 2);

    can_config_t cfg = {
        .bit_rate = 500000,
        .threshold = THRESHOLD
    };
    decode_result_t result;
    decode_can(samples, total, SAMPLE_RATE, &cfg, &result);

    ASSERT(result.type == DECODE_CAN, "result type is CAN");
    ASSERT(result.num_frames == 1, "decoded 1 CAN frame");
    if (result.num_frames >= 1) {
        ASSERT(result.frames[0].data_len == 2, "DLC is 2");
        ASSERT(result.frames[0].data[0] == 0x01, "data[0] is 0x01");
        ASSERT(result.frames[0].data[1] == 0x02, "data[1] is 0x02");
        /* Label should contain "ID:" and "123" */
        ASSERT(strstr(result.frames[0].label, "ID:") != NULL, "label has ID prefix");
        ASSERT(strstr(result.frames[0].label, "123") != NULL, "label contains 123");
    }

    free(samples);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
    printf("=== Protocol Decoder Tests ===\n\n");

    test_uart_hello();
    test_uart_framing_error();
    test_i2c_basic();
    test_i2c_repeated_start();
    test_spi_byte();
    test_can_standard();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
