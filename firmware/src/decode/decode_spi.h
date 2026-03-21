#ifndef DECODE_SPI_H
#define DECODE_SPI_H

#include "decoder.h"

typedef struct {
    int16_t mosi_threshold;
    int16_t clk_threshold;
    int16_t cs_threshold;
    uint8_t cpol;            /* Clock polarity: 0 or 1 */
    uint8_t cpha;            /* Clock phase: 0 or 1 */
    uint8_t bit_order;       /* 0=MSB first, 1=LSB first */
} spi_config_t;

void decode_spi(const int16_t *mosi_samples, const int16_t *clk_samples,
                const int16_t *cs_samples, uint32_t num_samples,
                float sample_rate, const spi_config_t *cfg,
                decode_result_t *result);

#endif /* DECODE_SPI_H */
