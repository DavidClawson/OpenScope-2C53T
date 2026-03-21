/*
 * Math Channel - Combine oscilloscope channels via arithmetic operations
 *
 * All computations use int32_t intermediates to avoid overflow.
 */

#include "math_channel.h"

/* Clamp int32_t to int16_t range */
static inline int16_t clamp16(int32_t val)
{
    if (val > 32767)  return 32767;
    if (val < -32768) return -32768;
    return (int16_t)val;
}

void math_channel_compute(const int16_t *ch_a, const int16_t *ch_b,
                          int16_t *output, uint16_t num_samples,
                          const math_config_t *cfg)
{
    uint16_t i;
    int32_t tmp;
    float scale = cfg->scale;
    int apply_scale = (scale != 1.0f);

    switch (cfg->operation) {
    case MATH_ADD:
        for (i = 0; i < num_samples; i++) {
            tmp = (int32_t)ch_a[i] + (int32_t)ch_b[i];
            if (apply_scale) tmp = (int32_t)(tmp * scale);
            output[i] = clamp16(tmp);
        }
        break;

    case MATH_SUB:
        for (i = 0; i < num_samples; i++) {
            tmp = (int32_t)ch_a[i] - (int32_t)ch_b[i];
            if (apply_scale) tmp = (int32_t)(tmp * scale);
            output[i] = clamp16(tmp);
        }
        break;

    case MATH_MUL:
        for (i = 0; i < num_samples; i++) {
            /* Fixed-point multiply: result scaled to int16 range */
            tmp = ((int32_t)ch_a[i] * (int32_t)ch_b[i]) / 32768;
            if (apply_scale) tmp = (int32_t)(tmp * scale);
            output[i] = clamp16(tmp);
        }
        break;

    case MATH_INV_A:
        for (i = 0; i < num_samples; i++) {
            /* Handle -32768 overflow: -(-32768) = 32768 > INT16_MAX */
            tmp = -(int32_t)ch_a[i];
            if (apply_scale) tmp = (int32_t)(tmp * scale);
            output[i] = clamp16(tmp);
        }
        break;

    case MATH_INV_B:
        for (i = 0; i < num_samples; i++) {
            tmp = -(int32_t)ch_b[i];
            if (apply_scale) tmp = (int32_t)(tmp * scale);
            output[i] = clamp16(tmp);
        }
        break;

    default:
        /* Unknown operation: zero output */
        for (i = 0; i < num_samples; i++) {
            output[i] = 0;
        }
        break;
    }
}

const char *math_channel_name(math_op_t op)
{
    static const char *names[] = {
        "A+B",
        "A-B",
        "A*B",
        "-A",
        "-B"
    };

    if (op < MATH_COUNT) {
        return names[op];
    }
    return "???";
}
