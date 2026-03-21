#ifndef MATH_CHANNEL_H
#define MATH_CHANNEL_H
#include <stdint.h>

typedef enum {
    MATH_ADD = 0,      /* A + B */
    MATH_SUB,          /* A - B */
    MATH_MUL,          /* A * B */
    MATH_INV_A,        /* -A (invert channel A) */
    MATH_INV_B,        /* -B (invert channel B) */
    MATH_COUNT
} math_op_t;

typedef struct {
    math_op_t operation;
    float     scale;        /* Output scaling factor (default 1.0) */
} math_config_t;

/* Compute math channel from two input buffers */
void math_channel_compute(const int16_t *ch_a, const int16_t *ch_b,
                          int16_t *output, uint16_t num_samples,
                          const math_config_t *cfg);

/* Get operation name string */
const char *math_channel_name(math_op_t op);

#endif
