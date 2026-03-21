#ifndef DECODE_I2C_H
#define DECODE_I2C_H

#include "decoder.h"

typedef struct {
    int16_t sda_threshold;
    int16_t scl_threshold;
} i2c_config_t;

void decode_i2c(const int16_t *sda_samples, const int16_t *scl_samples,
                uint32_t num_samples, float sample_rate,
                const i2c_config_t *cfg, decode_result_t *result);

#endif /* DECODE_I2C_H */
