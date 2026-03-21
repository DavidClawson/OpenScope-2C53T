#ifndef DECODER_H
#define DECODER_H
#include <stdint.h>

#define DECODE_MAX_FRAMES 64
#define DECODE_MAX_DATA   256
#define DECODE_MAX_LABEL  16

typedef enum {
    DECODE_UART = 0,
    DECODE_I2C,
    DECODE_SPI,
    DECODE_CAN,
    DECODE_KLINE,
    DECODE_COUNT
} decoder_type_t;

/* A decoded frame (protocol-independent) */
typedef struct {
    uint32_t start_sample;   /* Sample index where frame starts */
    uint32_t end_sample;     /* Sample index where frame ends */
    uint8_t  data[16];       /* Decoded bytes */
    uint8_t  data_len;       /* Number of decoded bytes */
    uint8_t  flags;          /* Protocol-specific flags (error, ACK, etc.) */
    char     label[16];      /* Human-readable label ("0x48 W", "ACK", etc.) */
} decode_frame_t;

/* Decode result */
typedef struct {
    decode_frame_t frames[DECODE_MAX_FRAMES];
    uint16_t       num_frames;
    decoder_type_t type;
} decode_result_t;

/* Flag bits common across protocols */
#define DECODE_FLAG_ERROR   0x01
#define DECODE_FLAG_ACK     0x02
#define DECODE_FLAG_NAK     0x04

#endif /* DECODER_H */
