#ifndef DECODE_CAN_H
#define DECODE_CAN_H

#include "decoder.h"

typedef struct {
    uint32_t bit_rate;       /* 125000, 250000, 500000, 1000000 */
    int16_t  threshold;      /* Differential threshold */
} can_config_t;

void decode_can(const int16_t *samples, uint32_t num_samples,
                float sample_rate, const can_config_t *cfg,
                decode_result_t *result);

#endif /* DECODE_CAN_H */
