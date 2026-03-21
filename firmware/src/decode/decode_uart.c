#include "decode_uart.h"
#include <string.h>

/*
 * UART decoder: converts analog samples to digital, then decodes
 * asynchronous serial frames (start bit, data bits, optional parity, stop bit).
 */

/* Format a byte as hex into label buffer: "0x1A" */
static void format_hex_byte(char *buf, uint8_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(val >> 4) & 0x0F];
    buf[3] = hex[val & 0x0F];
    buf[4] = '\0';
}

/* Count number of set bits in a byte */
static uint8_t popcount8(uint8_t v)
{
    uint8_t c = 0;
    while (v) {
        c += v & 1;
        v >>= 1;
    }
    return c;
}

void decode_uart(const int16_t *samples, uint32_t num_samples,
                 float sample_rate, const uart_config_t *cfg,
                 decode_result_t *result)
{
    uint32_t i;
    float samples_per_bit;
    uint32_t half_bit;

    memset(result, 0, sizeof(*result));
    result->type = DECODE_UART;

    if (!samples || num_samples == 0 || !cfg || cfg->baud_rate == 0) {
        return;
    }

    samples_per_bit = sample_rate / (float)cfg->baud_rate;
    half_bit = (uint32_t)(samples_per_bit / 2.0f);

    /* UART idle is high (mark). Start bit is low (space). */
    i = 0;
    while (i < num_samples && result->num_frames < DECODE_MAX_FRAMES) {
        int16_t thresh = cfg->threshold;
        int cur_level = (samples[i] >= thresh) ? 1 : 0;

        /* Look for start bit: transition from high to low */
        if (cur_level == 0) {
            /* Possible start bit. Verify by sampling at center of start bit. */
            uint32_t start_sample = i;
            uint32_t center = i + half_bit;

            if (center >= num_samples) break;
            if (samples[center] >= thresh) {
                /* False start - not actually low at center */
                i++;
                continue;
            }

            /* Read data bits, sampling at center of each bit */
            uint8_t byte_val = 0;
            uint8_t bits_read = 0;
            uint8_t valid = 1;
            uint32_t bit_pos;

            for (bits_read = 0; bits_read < cfg->data_bits; bits_read++) {
                bit_pos = start_sample + (uint32_t)((1.5f + bits_read) * samples_per_bit);
                if (bit_pos >= num_samples) { valid = 0; break; }
                int bit_val = (samples[bit_pos] >= thresh) ? 1 : 0;
                byte_val |= (uint8_t)(bit_val << bits_read);  /* LSB first */
            }

            if (!valid) break;

            /* Check parity bit if configured */
            uint8_t parity_error = 0;
            uint8_t next_bit_offset = cfg->data_bits;

            if (cfg->parity != 0) {
                bit_pos = start_sample + (uint32_t)((1.5f + next_bit_offset) * samples_per_bit);
                if (bit_pos >= num_samples) break;
                int parity_bit = (samples[bit_pos] >= thresh) ? 1 : 0;
                uint8_t ones = popcount8(byte_val);

                if (cfg->parity == 1) {
                    /* Odd parity: total ones (data + parity) should be odd */
                    if (((ones + parity_bit) & 1) == 0) parity_error = 1;
                } else if (cfg->parity == 2) {
                    /* Even parity: total ones (data + parity) should be even */
                    if (((ones + parity_bit) & 1) != 0) parity_error = 1;
                }
                next_bit_offset++;
            }

            /* Check stop bit (should be high) */
            bit_pos = start_sample + (uint32_t)((1.5f + next_bit_offset) * samples_per_bit);
            uint8_t framing_error = 0;
            if (bit_pos < num_samples) {
                if (samples[bit_pos] < thresh) {
                    framing_error = 1;
                }
            }

            /* Calculate end position */
            uint32_t end_sample = start_sample +
                (uint32_t)((1 + cfg->data_bits +
                           (cfg->parity ? 1 : 0) +
                           cfg->stop_bits) * samples_per_bit);

            /* Store frame */
            decode_frame_t *f = &result->frames[result->num_frames];
            f->start_sample = start_sample;
            f->end_sample = (end_sample < num_samples) ? end_sample : num_samples - 1;
            f->data[0] = byte_val;
            f->data_len = 1;
            f->flags = 0;
            if (framing_error) f->flags |= DECODE_FLAG_ERROR;
            if (parity_error) f->flags |= DECODE_FLAG_ERROR;
            format_hex_byte(f->label, byte_val);

            result->num_frames++;

            /* Advance past this frame */
            i = (end_sample < num_samples) ? end_sample : num_samples;
        } else {
            i++;
        }
    }
}

/* ─── Low-level helpers for UART-based protocol decoders ────────── */

uint32_t uart_decode_byte(const int16_t *samples, uint32_t num_samples,
                          float samples_per_bit, int16_t threshold,
                          uint8_t *out_byte)
{
    uint32_t frame_len = (uint32_t)(samples_per_bit * 10.0f);
    if (num_samples < frame_len)
        return 0;

    /* Verify start bit */
    if (samples[0] >= threshold)
        return 0;

    /* Sample data bits at center of each bit period */
    uint8_t byte_val = 0;
    int bit;
    for (bit = 0; bit < 8; bit++) {
        uint32_t center = (uint32_t)(samples_per_bit * (1.5f + (float)bit));
        if (center >= num_samples)
            return 0;
        if (samples[center] >= threshold)
            byte_val |= (uint8_t)(1 << bit);  /* LSB first */
    }

    *out_byte = byte_val;
    return frame_len;
}

uint32_t uart_decode_bytes(const int16_t *samples, uint32_t num_samples,
                           float sample_rate, float baud_rate,
                           int16_t threshold,
                           uint8_t *raw_bytes, uint32_t *sample_positions,
                           uint32_t max_bytes)
{
    float samples_per_bit = sample_rate / baud_rate;
    uint32_t count = 0;
    uint32_t i = 0;

    while (i < num_samples && count < max_bytes) {
        if (samples[i] >= threshold) { i++; continue; }
        if (i > 0 && samples[i - 1] < threshold) { i++; continue; }

        uint8_t byte_val;
        uint32_t consumed = uart_decode_byte(samples + i, num_samples - i,
                                             samples_per_bit, threshold,
                                             &byte_val);
        if (consumed == 0) { i++; continue; }

        if (sample_positions)
            sample_positions[count] = i;
        raw_bytes[count] = byte_val;
        count++;
        i += consumed;
    }

    return count;
}
