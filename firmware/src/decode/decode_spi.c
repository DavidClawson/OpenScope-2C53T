#include "decode_spi.h"
#include <string.h>

/*
 * SPI decoder: samples MOSI on active clock edges (per CPOL/CPHA),
 * while CS is asserted (low). Accumulates 8 bits per byte.
 */

static void format_hex_byte(char *buf, uint8_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(val >> 4) & 0x0F];
    buf[3] = hex[val & 0x0F];
    buf[4] = '\0';
}

void decode_spi(const int16_t *mosi_samples, const int16_t *clk_samples,
                const int16_t *cs_samples, uint32_t num_samples,
                float sample_rate, const spi_config_t *cfg,
                decode_result_t *result)
{
    uint32_t i;
    int clk, clk_prev, cs, cs_prev, mosi;
    uint8_t byte_val = 0;
    uint8_t bit_count = 0;
    uint32_t byte_start = 0;
    int cs_active = 0;

    (void)sample_rate;

    memset(result, 0, sizeof(*result));
    result->type = DECODE_SPI;

    if (!mosi_samples || !clk_samples || !cs_samples || num_samples < 2 || !cfg) {
        return;
    }

    /*
     * CPOL/CPHA determine which edge to sample on:
     * CPOL=0 CPHA=0: sample on rising edge  (idle low, leading edge)
     * CPOL=0 CPHA=1: sample on falling edge (idle low, trailing edge)
     * CPOL=1 CPHA=0: sample on falling edge (idle high, leading edge)
     * CPOL=1 CPHA=1: sample on rising edge  (idle high, trailing edge)
     *
     * Sample on rising edge when CPOL^CPHA == 0
     * Sample on falling edge when CPOL^CPHA == 1
     */
    int sample_on_rising = (cfg->cpol ^ cfg->cpha) == 0;

    clk_prev = (clk_samples[0] >= cfg->clk_threshold) ? 1 : 0;
    cs_prev = (cs_samples[0] >= cfg->cs_threshold) ? 1 : 0;
    cs_active = (cs_prev == 0);

    for (i = 1; i < num_samples && result->num_frames < DECODE_MAX_FRAMES; i++) {
        clk = (clk_samples[i] >= cfg->clk_threshold) ? 1 : 0;
        cs = (cs_samples[i] >= cfg->cs_threshold) ? 1 : 0;
        mosi = (mosi_samples[i] >= cfg->mosi_threshold) ? 1 : 0;

        /* CS just went active (high to low) */
        if (cs_prev == 1 && cs == 0) {
            cs_active = 1;
            bit_count = 0;
            byte_val = 0;
            byte_start = i;
        }

        /* CS just went inactive (low to high) - end any partial byte */
        if (cs_prev == 0 && cs == 1) {
            cs_active = 0;
            /* If we had bits accumulated, we could emit a partial frame,
             * but typically SPI transfers are byte-aligned */
            bit_count = 0;
            byte_val = 0;
        }

        if (cs_active) {
            int active_edge = 0;
            if (sample_on_rising && clk_prev == 0 && clk == 1) {
                active_edge = 1;
            } else if (!sample_on_rising && clk_prev == 1 && clk == 0) {
                active_edge = 1;
            }

            if (active_edge) {
                if (bit_count == 0) {
                    byte_start = i;
                }

                if (cfg->bit_order == 0) {
                    /* MSB first */
                    byte_val = (byte_val << 1) | (uint8_t)mosi;
                } else {
                    /* LSB first */
                    byte_val |= (uint8_t)(mosi << bit_count);
                }
                bit_count++;

                if (bit_count == 8) {
                    decode_frame_t *f = &result->frames[result->num_frames];
                    f->start_sample = byte_start;
                    f->end_sample = i;
                    f->data[0] = byte_val;
                    f->data_len = 1;
                    f->flags = 0;
                    format_hex_byte(f->label, byte_val);
                    result->num_frames++;
                    bit_count = 0;
                    byte_val = 0;
                }
            }
        }

        clk_prev = clk;
        cs_prev = cs;
    }
}
