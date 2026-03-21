#include "decode_i2c.h"
#include <string.h>

/*
 * I2C decoder: detects START/STOP conditions on SDA+SCL,
 * reads bits on SCL rising edges, decodes address and data bytes.
 */

static const char hex_chars[] = "0123456789ABCDEF";

static void format_hex(char *buf, uint8_t val)
{
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex_chars[(val >> 4) & 0x0F];
    buf[3] = hex_chars[val & 0x0F];
    buf[4] = '\0';
}

static void format_addr(char *buf, uint8_t addr_byte)
{
    uint8_t addr7 = addr_byte >> 1;
    uint8_t rw = addr_byte & 1;
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex_chars[(addr7 >> 4) & 0x0F];
    buf[3] = hex_chars[addr7 & 0x0F];
    buf[4] = ' ';
    buf[5] = rw ? 'R' : 'W';
    buf[6] = '\0';
}

void decode_i2c(const int16_t *sda_samples, const int16_t *scl_samples,
                uint32_t num_samples, float sample_rate,
                const i2c_config_t *cfg, decode_result_t *result)
{
    uint32_t i;
    int sda, sda_prev, scl, scl_prev;
    uint8_t bit_count = 0;
    uint8_t byte_val = 0;
    uint8_t in_frame = 0;
    uint8_t is_first_byte = 0;
    uint32_t byte_start = 0;

    (void)sample_rate;

    memset(result, 0, sizeof(*result));
    result->type = DECODE_I2C;

    if (!sda_samples || !scl_samples || num_samples < 2 || !cfg) {
        return;
    }

    sda_prev = (sda_samples[0] >= cfg->sda_threshold) ? 1 : 0;
    scl_prev = (scl_samples[0] >= cfg->scl_threshold) ? 1 : 0;

    for (i = 1; i < num_samples && result->num_frames < DECODE_MAX_FRAMES; i++) {
        sda = (sda_samples[i] >= cfg->sda_threshold) ? 1 : 0;
        scl = (scl_samples[i] >= cfg->scl_threshold) ? 1 : 0;

        /* START condition: SDA falls while SCL is high */
        if (sda_prev == 1 && sda == 0 && scl == 1 && scl_prev == 1) {
            if (in_frame) {
                /* Repeated START - emit a frame for it */
                decode_frame_t *f = &result->frames[result->num_frames];
                f->start_sample = i;
                f->end_sample = i;
                f->data_len = 0;
                f->flags = 0;
                memcpy(f->label, "Sr", 3);
                result->num_frames++;
                if (result->num_frames >= DECODE_MAX_FRAMES) break;
            }
            in_frame = 1;
            is_first_byte = 1;
            bit_count = 0;
            byte_val = 0;
            byte_start = i;

            sda_prev = sda;
            scl_prev = scl;
            continue;
        }

        /* STOP condition: SDA rises while SCL is high */
        if (sda_prev == 0 && sda == 1 && scl == 1 && scl_prev == 1) {
            in_frame = 0;
            bit_count = 0;

            sda_prev = sda;
            scl_prev = scl;
            continue;
        }

        /* Rising edge of SCL: sample data bit */
        if (scl_prev == 0 && scl == 1 && in_frame) {
            if (bit_count < 8) {
                /* Data bit - MSB first */
                byte_val = (byte_val << 1) | (uint8_t)sda;
                if (bit_count == 0) {
                    byte_start = i;
                }
                bit_count++;
            } else {
                /* 9th bit = ACK/NAK */
                decode_frame_t *f = &result->frames[result->num_frames];
                f->start_sample = byte_start;
                f->end_sample = i;
                f->data[0] = byte_val;
                f->data_len = 1;

                if (sda == 0) {
                    f->flags = DECODE_FLAG_ACK;
                } else {
                    f->flags = DECODE_FLAG_NAK;
                }

                if (is_first_byte) {
                    format_addr(f->label, byte_val);
                    is_first_byte = 0;
                } else {
                    format_hex(f->label, byte_val);
                }

                result->num_frames++;

                /* Also emit ACK/NAK frame */
                if (result->num_frames < DECODE_MAX_FRAMES) {
                    f = &result->frames[result->num_frames];
                    f->start_sample = i;
                    f->end_sample = i;
                    f->data_len = 0;
                    f->flags = (sda == 0) ? DECODE_FLAG_ACK : DECODE_FLAG_NAK;
                    memcpy(f->label, (sda == 0) ? "ACK" : "NAK", 4);
                    result->num_frames++;
                }

                /* Reset for next byte */
                bit_count = 0;
                byte_val = 0;
            }
        }

        sda_prev = sda;
        scl_prev = scl;
    }
}
