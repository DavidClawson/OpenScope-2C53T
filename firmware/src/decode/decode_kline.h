/*
 * decode_kline.h — K-Line / KWP2000 protocol decoder
 *
 * Decodes ISO 9141-2 and ISO 14230 (KWP2000) automotive diagnostics
 * from analog oscilloscope samples. K-Line is a single-wire half-duplex
 * UART-based protocol running at 10400 baud (8N1).
 */

#ifndef DECODE_KLINE_H
#define DECODE_KLINE_H

#include "decoder.h"

/* K-Line decoder configuration */
typedef struct {
    int16_t  threshold;        /* Signal threshold (typically 0 for centered signal) */
    uint8_t  protocol;         /* 0 = auto-detect, 1 = ISO 9141-2, 2 = KWP2000 */
    uint8_t  address_mode;     /* 0 = auto, 1 = with address, 2 = no address */
} kline_config_t;

/* K-Line specific frame flags */
#define KLINE_FLAG_INIT_5BAUD    0x01  /* 5-baud init sequence */
#define KLINE_FLAG_INIT_FAST     0x02  /* Fast init sequence */
#define KLINE_FLAG_REQUEST       0x04  /* Tester request */
#define KLINE_FLAG_RESPONSE      0x08  /* ECU response */
#define KLINE_FLAG_CHECKSUM_OK   0x10  /* Checksum valid */
#define KLINE_FLAG_CHECKSUM_ERR  0x20  /* Checksum mismatch */

/* KWP2000 Service IDs */
#define KWP_START_DIAG         0x10
#define KWP_CLEAR_DTC          0x14
#define KWP_READ_DTC           0x18
#define KWP_READ_DATA          0x21
#define KWP_TESTER_PRESENT     0x3E
#define KWP_START_COMM         0x81
#define KWP_STOP_COMM          0x82

/* Decode K-Line / KWP2000 from analog samples */
void decode_kline(const int16_t *samples, uint32_t num_samples,
                  float sample_rate, const kline_config_t *cfg,
                  decode_result_t *result);

/* Get human-readable name for a KWP2000 service ID */
const char *kline_service_name(uint8_t service_id);

#endif /* DECODE_KLINE_H */
