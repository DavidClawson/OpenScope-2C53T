#include "decode_can.h"
#include <string.h>

/*
 * CAN bus decoder: detects frames on a single-ended CAN signal.
 * Handles standard (11-bit) and extended (29-bit) IDs.
 * Implements bit de-stuffing per CAN 2.0 spec.
 */

static const char hex_chars[] = "0123456789ABCDEF";

/* CAN bit values: dominant=0, recessive=1 */
#define CAN_DOMINANT  0
#define CAN_RECESSIVE 1

/* Context struct for CAN bit reading with de-stuffing */
typedef struct {
    const int16_t *samples;
    uint32_t num_samples;
    float samples_per_bit;
    uint32_t frame_start;
    int16_t threshold;
    uint32_t raw_bit_idx;
    uint32_t consecutive;
    int last_bit;
} can_ctx_t;

static int ctx_read_bit(can_ctx_t *ctx)
{
    uint32_t pos = ctx->frame_start +
                   (uint32_t)((ctx->raw_bit_idx + 0.5f) * ctx->samples_per_bit);
    if (pos >= ctx->num_samples) return -1;
    int bit = (ctx->samples[pos] >= ctx->threshold) ? CAN_RECESSIVE : CAN_DOMINANT;
    return bit;
}

/* Read one bit with de-stuffing. Returns bit value or -1 on error. */
static int ctx_read_destuffed(can_ctx_t *ctx)
{
    int bit = ctx_read_bit(ctx);
    if (bit < 0) return -1;
    ctx->raw_bit_idx++;

    if (ctx->last_bit >= 0 && bit == ctx->last_bit) {
        ctx->consecutive++;
    } else {
        ctx->consecutive = 1;
    }
    ctx->last_bit = bit;

    if (ctx->consecutive > 5) {
        return -1; /* Stuff error */
    }

    /* After 5 consecutive, next must be a stuff bit - consume it */
    if (ctx->consecutive == 5) {
        int stuff = ctx_read_bit(ctx);
        if (stuff < 0) return -1;
        if (stuff == ctx->last_bit) return -1; /* Stuff error */
        ctx->raw_bit_idx++;
        ctx->consecutive = 1;
        ctx->last_bit = stuff;
    }

    return bit;
}

/* Read N bits without de-stuffing (for CRC delimiter, ACK, EOF) */
static int ctx_read_raw(can_ctx_t *ctx)
{
    int bit = ctx_read_bit(ctx);
    if (bit < 0) return -1;
    ctx->raw_bit_idx++;
    return bit;
}

/* Read multiple de-stuffed bits and assemble into a uint32_t (MSB first) */
static int ctx_read_field(can_ctx_t *ctx, uint32_t nbits, uint32_t *value)
{
    uint32_t v = 0;
    uint32_t i;
    for (i = 0; i < nbits; i++) {
        int bit = ctx_read_destuffed(ctx);
        if (bit < 0) return -1;
        v = (v << 1) | (uint32_t)bit;
    }
    *value = v;
    return 0;
}

void decode_can(const int16_t *samples, uint32_t num_samples,
                float sample_rate, const can_config_t *cfg,
                decode_result_t *result)
{
    uint32_t i;
    float samples_per_bit;

    memset(result, 0, sizeof(*result));
    result->type = DECODE_CAN;

    if (!samples || num_samples == 0 || !cfg || cfg->bit_rate == 0) {
        return;
    }

    samples_per_bit = sample_rate / (float)cfg->bit_rate;

    /* Scan for SOF: dominant bit after idle (recessive) */
    i = 0;
    while (i < num_samples && result->num_frames < DECODE_MAX_FRAMES) {
        int level = (samples[i] >= cfg->threshold) ? CAN_RECESSIVE : CAN_DOMINANT;

        if (level == CAN_RECESSIVE) {
            i++;
            continue;
        }

        /* Found a dominant bit - potential SOF */
        uint32_t frame_start = i;

        can_ctx_t ctx;
        ctx.samples = samples;
        ctx.num_samples = num_samples;
        ctx.samples_per_bit = samples_per_bit;
        ctx.frame_start = frame_start;
        ctx.threshold = cfg->threshold;
        ctx.raw_bit_idx = 0;
        ctx.consecutive = 0;
        ctx.last_bit = -1;

        /* SOF bit (already know it's dominant) */
        int sof = ctx_read_destuffed(&ctx);
        if (sof < 0 || sof != CAN_DOMINANT) {
            i++;
            continue;
        }

        /* Read 11-bit ID */
        uint32_t id = 0;
        if (ctx_read_field(&ctx, 11, &id) < 0) {
            i++;
            continue;
        }

        /* RTR bit */
        int rtr = ctx_read_destuffed(&ctx);
        if (rtr < 0) { i++; continue; }

        /* IDE bit */
        int ide = ctx_read_destuffed(&ctx);
        if (ide < 0) { i++; continue; }

        uint32_t ext_id = 0;
        int ext_rtr = 0;

        if (ide == CAN_RECESSIVE) {
            /* Extended frame: read 18 more ID bits */
            if (ctx_read_field(&ctx, 18, &ext_id) < 0) {
                i++;
                continue;
            }
            id = (id << 18) | ext_id;

            /* Extended RTR */
            ext_rtr = ctx_read_destuffed(&ctx);
            if (ext_rtr < 0) { i++; continue; }

            /* r1 (reserved) */
            int r1 = ctx_read_destuffed(&ctx);
            if (r1 < 0) { i++; continue; }
            (void)r1;
        }

        /* r0 (reserved bit) */
        int r0 = ctx_read_destuffed(&ctx);
        if (r0 < 0) { i++; continue; }
        (void)r0;

        /* DLC - 4 bits */
        uint32_t dlc = 0;
        if (ctx_read_field(&ctx, 4, &dlc) < 0) {
            i++;
            continue;
        }
        if (dlc > 8) dlc = 8;

        /* Data bytes */
        uint8_t data_bytes[8];
        uint32_t d;
        int data_ok = 1;
        for (d = 0; d < dlc; d++) {
            uint32_t byte_val = 0;
            if (ctx_read_field(&ctx, 8, &byte_val) < 0) {
                data_ok = 0;
                break;
            }
            data_bytes[d] = (uint8_t)byte_val;
        }
        if (!data_ok) { i++; continue; }

        /* CRC - 15 bits (read but don't verify for now) */
        uint32_t crc = 0;
        if (ctx_read_field(&ctx, 15, &crc) < 0) {
            i++;
            continue;
        }
        (void)crc;

        /* CRC delimiter (1 recessive, no stuffing) */
        int crc_del = ctx_read_raw(&ctx);
        if (crc_del < 0) { i++; continue; }

        /* ACK slot (no stuffing) */
        int ack = ctx_read_raw(&ctx);
        if (ack < 0) { i++; continue; }

        /* ACK delimiter (no stuffing) */
        int ack_del = ctx_read_raw(&ctx);
        if (ack_del < 0) { i++; continue; }
        (void)ack_del;

        /* EOF: 7 recessive bits (no stuffing) */
        int eof_ok = 1;
        uint32_t e;
        for (e = 0; e < 7; e++) {
            int eb = ctx_read_raw(&ctx);
            if (eb < 0 || eb != CAN_RECESSIVE) {
                eof_ok = 0;
                break;
            }
        }

        /* Build the frame */
        decode_frame_t *f = &result->frames[result->num_frames];
        f->start_sample = frame_start;
        f->end_sample = frame_start +
            (uint32_t)(ctx.raw_bit_idx * samples_per_bit);
        if (f->end_sample >= num_samples) {
            f->end_sample = num_samples - 1;
        }

        /* Copy data */
        f->data_len = (uint8_t)dlc;
        for (d = 0; d < dlc && d < 16; d++) {
            f->data[d] = data_bytes[d];
        }

        f->flags = 0;
        if (!eof_ok) f->flags |= DECODE_FLAG_ERROR;

        /* Format label: "ID:0xHHH [D] HH HH..." */
        {
            char *p = f->label;
            char *end = f->label + sizeof(f->label) - 1;

            /* "ID:" prefix */
            if (p + 3 <= end) { *p++ = 'I'; *p++ = 'D'; *p++ = ':'; }

            /* Hex ID (compact) */
            if (ide == CAN_RECESSIVE) {
                /* Extended 29-bit: up to 8 hex digits, but truncate for label */
                if (p + 5 <= end) {
                    *p++ = hex_chars[(id >> 16) & 0x0F];
                    *p++ = hex_chars[(id >> 12) & 0x0F];
                    *p++ = hex_chars[(id >> 8) & 0x0F];
                    *p++ = hex_chars[(id >> 4) & 0x0F];
                    *p++ = hex_chars[id & 0x0F];
                }
            } else {
                /* Standard 11-bit: 3 hex digits */
                if (p + 3 <= end) {
                    *p++ = hex_chars[(id >> 8) & 0x0F];
                    *p++ = hex_chars[(id >> 4) & 0x0F];
                    *p++ = hex_chars[id & 0x0F];
                }
            }

            *p = '\0';
        }

        result->num_frames++;

        /* Advance past frame */
        i = f->end_sample + 1;
    }
}
