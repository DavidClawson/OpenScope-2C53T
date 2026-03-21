#ifndef DECODE_UART_H
#define DECODE_UART_H

#include "decoder.h"

typedef struct {
    uint32_t baud_rate;      /* 300, 1200, 9600, 19200, 38400, 57600, 115200 */
    uint8_t  data_bits;      /* 7 or 8 */
    uint8_t  parity;         /* 0=none, 1=odd, 2=even */
    uint8_t  stop_bits;      /* 1 or 2 */
    int16_t  threshold;      /* Signal threshold for digital conversion */
} uart_config_t;

/* Decode UART from analog samples */
void decode_uart(const int16_t *samples, uint32_t num_samples,
                 float sample_rate, const uart_config_t *cfg,
                 decode_result_t *result);

/* Low-level helpers (used by K-Line and other UART-based protocol decoders) */

/* Decode a single UART byte starting at a falling edge (start bit).
 * Returns samples consumed, or 0 on error. */
uint32_t uart_decode_byte(const int16_t *samples, uint32_t num_samples,
                          float samples_per_bit, int16_t threshold,
                          uint8_t *out_byte);

/* Scan for UART bytes in a sample buffer. Returns number of bytes found. */
uint32_t uart_decode_bytes(const int16_t *samples, uint32_t num_samples,
                           float sample_rate, float baud_rate,
                           int16_t threshold,
                           uint8_t *raw_bytes, uint32_t *sample_positions,
                           uint32_t max_bytes);

#endif /* DECODE_UART_H */
