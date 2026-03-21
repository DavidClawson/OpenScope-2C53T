/*
 * decode_kline.c — K-Line / KWP2000 protocol decoder
 *
 * Decodes ISO 9141-2 and ISO 14230 (KWP2000) automotive diagnostic
 * messages from analog oscilloscope samples.
 *
 * K-Line physical layer:
 *   - Single wire, open-drain with pull-up to 12V
 *   - 0V = dominant (logic 0), 12V = recessive (logic 1)
 *   - Standard UART 8N1 at 10400 baud
 *   - Half-duplex: tester request, then ECU response
 *
 * KWP2000 message format:
 *   [Format] [Target?] [Source?] [Length?] [SID] [Data...] [Checksum]
 *
 *   Format byte bits 7:6 (address mode):
 *     00 = no address info
 *     01 = CARB mode (exception format)
 *     10 = with address, length in format byte bits 5:0
 *     11 = with address, separate length byte follows
 *
 *   Checksum = sum of all preceding bytes mod 256
 */

#include "decode_kline.h"
#include <string.h>

/* Baud rates */
#define KLINE_BAUD_STANDARD  10400.0f
#define KLINE_BAUD_5INIT     5.0f

/* ISO 9141-2 sync/address bytes */
#define ISO_SYNC_BYTE        0x55
#define ISO_INIT_ADDR        0x33
#define ISO_INIT_ADDR_INV    0xCC

/* Fast init timing (in seconds) */
#define FAST_INIT_PULSE_MIN  0.020f   /* 20ms minimum */
#define FAST_INIT_PULSE_MAX  0.030f   /* 30ms maximum */

/* Maximum raw bytes we'll decode from the sample buffer */
#define MAX_RAW_BYTES        256

/* ------------------------------------------------------------------ */
/* External: UART byte decoder from decode_uart.c                     */
/* ------------------------------------------------------------------ */
extern uint32_t uart_decode_byte(const int16_t *samples, uint32_t num_samples,
                                 float samples_per_bit, int16_t threshold,
                                 uint8_t *out_byte);

extern uint32_t uart_decode_bytes(const int16_t *samples, uint32_t num_samples,
                                  float sample_rate, float baud_rate,
                                  int16_t threshold,
                                  uint8_t *raw_bytes, uint32_t *sample_positions,
                                  uint32_t max_bytes);

/* ------------------------------------------------------------------ */
/* Service ID name lookup                                              */
/* ------------------------------------------------------------------ */
const char *kline_service_name(uint8_t service_id)
{
    /* Handle high-range SIDs (0x81-0x83) directly first */
    switch (service_id) {
    case 0x81: return "StrtComm";    /* startCommunication */
    case 0x82: return "StopComm";    /* stopCommunication */
    case 0x83: return "AccTime";     /* accessTimingParameters */
    case 0xC1: return "StrtComm";    /* startCommunication positive response */
    case 0xC2: return "StopComm";    /* stopCommunication positive response */
    case 0xC3: return "AccTime";     /* accessTimingParameters positive response */
    default: break;
    }

    /* Positive response = request SID + 0x40 */
    uint8_t base_sid = service_id;
    if (service_id >= 0x40 && service_id != 0x7F)
        base_sid = service_id - 0x40;

    switch (base_sid) {
    case 0x01: return "StartDg";     /* startDiagnosticSession (old) */
    case 0x10: return "StartDg";     /* startDiagnosticSession */
    case 0x11: return "EcuRst";      /* ecuReset */
    case 0x14: return "ClrDTC";      /* clearDiagnosticInformation */
    case 0x17: return "RdDTCSt";     /* readDTCByStatus (old) */
    case 0x18: return "ReadDTC";     /* readDTCByStatus */
    case 0x1A: return "RdEcuId";     /* readEcuIdentification */
    case 0x21: return "RdLocal";     /* readDataByLocalIdentifier */
    case 0x22: return "RdById";      /* readDataByIdentifier */
    case 0x23: return "RdMemAd";     /* readMemoryByAddress */
    case 0x27: return "SecAccs";     /* securityAccess */
    case 0x2E: return "WrById";      /* writeDataByIdentifier */
    case 0x2F: return "IOCtrl";      /* inputOutputControlByIdentifier */
    case 0x30: return "IOCtrlL";     /* inputOutputControlByLocalIdentifier */
    case 0x31: return "RoutCtl";     /* routineControl */
    case 0x34: return "ReqDnld";     /* requestDownload */
    case 0x35: return "ReqUpld";     /* requestUpload */
    case 0x36: return "TxData";      /* transferData */
    case 0x37: return "TxExit";      /* requestTransferExit */
    case 0x3E: return "TstrPres";    /* testerPresent */
    case 0x7F: return "NegResp";     /* negativeResponse */
    default:   return "KWP";
    }
}

/* ------------------------------------------------------------------ */
/* Detect 5-baud initialization sequence                               */
/*                                                                     */
/* The tester sends address byte 0x33 at 5 baud = 200ms per bit.      */
/* Total frame (start + 8 data + stop) = 2000ms.                      */
/* Pattern: start(low 200ms), then 8 data bits of 0x33 = 0b00110011   */
/* Returns: number of samples consumed, or 0 if not detected.         */
/* ------------------------------------------------------------------ */
static uint32_t detect_5baud_init(const int16_t *samples, uint32_t num_samples,
                                  float sample_rate, int16_t threshold)
{
    float samples_per_bit_5baud = sample_rate / KLINE_BAUD_5INIT;
    uint32_t frame_samples = (uint32_t)(samples_per_bit_5baud * 10.0f);

    if (num_samples < frame_samples)
        return 0;

    /* Need a long low period (start bit at 5 baud = 200ms) */
    uint32_t min_low_samples = (uint32_t)(samples_per_bit_5baud * 0.8f);

    /* Count consecutive low samples */
    uint32_t low_count = 0;
    for (uint32_t i = 0; i < num_samples && i < min_low_samples + 100; i++) {
        if (samples[i] < threshold)
            low_count++;
        else
            break;
    }

    if (low_count < min_low_samples)
        return 0;

    /* Try to decode byte at 5 baud */
    uint8_t init_byte;
    uint32_t consumed = uart_decode_byte(samples, num_samples,
                                         samples_per_bit_5baud, threshold,
                                         &init_byte);
    if (consumed == 0)
        return 0;

    /* Should be 0x33 (ISO 9141-2 init address) */
    if (init_byte != ISO_INIT_ADDR)
        return 0;

    return consumed;
}

/* ------------------------------------------------------------------ */
/* Detect fast initialization sequence                                 */
/*                                                                     */
/* Tester pulls K-line low for ~25ms, then releases high for ~25ms.   */
/* Returns: number of samples consumed, or 0 if not detected.         */
/* ------------------------------------------------------------------ */
static uint32_t detect_fast_init(const int16_t *samples, uint32_t num_samples,
                                 float sample_rate, int16_t threshold)
{
    uint32_t min_pulse = (uint32_t)(FAST_INIT_PULSE_MIN * sample_rate);
    uint32_t max_pulse = (uint32_t)(FAST_INIT_PULSE_MAX * sample_rate);

    if (num_samples < min_pulse * 2)
        return 0;

    /* Count low samples (TiniL period) */
    uint32_t low_count = 0;
    for (uint32_t i = 0; i < num_samples && low_count < max_pulse + 1000; i++) {
        if (samples[i] < threshold)
            low_count++;
        else
            break;
    }

    if (low_count < min_pulse || low_count > max_pulse)
        return 0;

    /* Count high samples after (TiniH period) */
    uint32_t high_count = 0;
    uint32_t start_high = low_count;
    for (uint32_t i = start_high; i < num_samples && high_count < max_pulse + 1000; i++) {
        if (samples[i] >= threshold)
            high_count++;
        else
            break;
    }

    if (high_count < min_pulse || high_count > max_pulse)
        return 0;

    return low_count + high_count;
}

/* ------------------------------------------------------------------ */
/* Parse a single KWP2000 message from raw bytes                       */
/*                                                                     */
/* Fills one decode_frame_t. Returns number of bytes consumed, or 0.  */
/* ------------------------------------------------------------------ */
static uint32_t parse_kwp_message(const uint8_t *bytes, uint32_t num_bytes,
                                  const uint32_t *sample_positions,
                                  decode_frame_t *frame)
{
    if (num_bytes < 2)
        return 0;

    uint8_t fmt = bytes[0];
    uint8_t addr_mode = (fmt >> 6) & 0x03;
    uint8_t has_addr = (addr_mode >= 2) ? 1 : ((addr_mode == 1) ? 1 : 0);
    uint32_t header_len;
    uint32_t data_len;

    switch (addr_mode) {
    case 0x00:
        /* No address, length in format byte bits 5:0 */
        data_len = fmt & 0x3F;
        header_len = 1;  /* format only */
        break;

    case 0x01:
        /* CARB mode: format + target + source, length in format bits 5:0 */
        data_len = fmt & 0x3F;
        header_len = 3;  /* format + target + source */
        break;

    case 0x02:
        /* With address, length in format byte bits 5:0 */
        data_len = fmt & 0x3F;
        header_len = 3;  /* format + target + source */
        break;

    case 0x03:
        /* With address, separate length byte */
        header_len = 4;  /* format + target + source + length */
        if (num_bytes < header_len)
            return 0;
        data_len = bytes[3];
        break;

    default:
        return 0;
    }

    if (num_bytes < header_len + data_len + 1)  /* +1 for checksum */
        return 0;

    uint32_t msg_len = header_len + data_len + 1;  /* total including checksum */

    /* Compute checksum: sum of all bytes except the checksum itself */
    uint8_t computed_cs = 0;
    for (uint32_t i = 0; i < msg_len - 1; i++)
        computed_cs += bytes[i];

    uint8_t received_cs = bytes[msg_len - 1];

    /* Fill frame */
    frame->start_sample = sample_positions[0];
    frame->end_sample = sample_positions[msg_len - 1];

    /* Copy raw bytes */
    uint8_t copy_len = (msg_len > DECODE_MAX_DATA) ? DECODE_MAX_DATA : (uint8_t)msg_len;
    memcpy(frame->data, bytes, copy_len);
    frame->data_len = copy_len;

    /* Determine service ID (first byte after header) */
    uint8_t sid = bytes[header_len];

    /* Set label from service name */
    const char *name = kline_service_name(sid);
    strncpy(frame->label, name, DECODE_MAX_LABEL - 1);
    frame->label[DECODE_MAX_LABEL - 1] = '\0';

    /* Set flags */
    frame->flags = 0;

    /* Checksum validity */
    if (computed_cs == received_cs)
        frame->flags |= KLINE_FLAG_CHECKSUM_OK;
    else
        frame->flags |= KLINE_FLAG_CHECKSUM_ERR;

    /* Request vs response heuristic:
     * - If target address is 0x01-0x7F (ECU range), it's a request from tester
     * - If positive response SID >= 0x40, it's a response
     * - Source 0xF1 = tester (ISO 14230 convention)
     */
    if (has_addr && header_len >= 3) {
        uint8_t target = bytes[1];
        uint8_t source = bytes[2];
        if (source == 0xF1 || (target >= 0x01 && target <= 0x7F && target != 0xF1))
            frame->flags |= KLINE_FLAG_REQUEST;
        else
            frame->flags |= KLINE_FLAG_RESPONSE;
    } else {
        /* Without address info, use SID to guess */
        if (sid >= 0x40 && sid != 0x7F)
            frame->flags |= KLINE_FLAG_RESPONSE;
        else
            frame->flags |= KLINE_FLAG_REQUEST;
    }

    return msg_len;
}

/* ------------------------------------------------------------------ */
/* Main K-Line decoder entry point                                     */
/* ------------------------------------------------------------------ */
void decode_kline(const int16_t *samples, uint32_t num_samples,
                  float sample_rate, const kline_config_t *cfg,
                  decode_result_t *result)
{
    memset(result, 0, sizeof(*result));

    if (num_samples == 0 || samples == NULL || cfg == NULL)
        return;

    int16_t threshold = cfg->threshold;
    uint32_t pos = 0;
    uint16_t init_flags = 0;

    /* --------------------------------------------------------------- */
    /* Phase 1: Check for initialization sequences at start of capture */
    /* --------------------------------------------------------------- */

    /* Skip leading idle (high) */
    while (pos < num_samples && samples[pos] >= threshold)
        pos++;

    if (pos < num_samples) {
        /* Try 5-baud init detection */
        uint32_t consumed = detect_5baud_init(samples + pos, num_samples - pos,
                                              sample_rate, threshold);
        if (consumed > 0) {
            /* Found 5-baud init — create a frame for it */
            if (result->num_frames < DECODE_MAX_FRAMES) {
                decode_frame_t *f = &result->frames[result->num_frames];
                f->start_sample = pos;
                f->end_sample = pos + consumed;
                f->data[0] = ISO_INIT_ADDR;
                f->data_len = 1;
                strncpy(f->label, "5BdInit", DECODE_MAX_LABEL - 1);
                f->flags = KLINE_FLAG_INIT_5BAUD;
                result->num_frames++;
            }
            init_flags |= KLINE_FLAG_INIT_5BAUD;
            pos += consumed;
        } else {
            /* Try fast init detection */
            consumed = detect_fast_init(samples + pos, num_samples - pos,
                                        sample_rate, threshold);
            if (consumed > 0) {
                if (result->num_frames < DECODE_MAX_FRAMES) {
                    decode_frame_t *f = &result->frames[result->num_frames];
                    f->start_sample = pos;
                    f->end_sample = pos + consumed;
                    f->data_len = 0;
                    strncpy(f->label, "FastInit", DECODE_MAX_LABEL - 1);
                    f->flags = KLINE_FLAG_INIT_FAST;
                    result->num_frames++;
                }
                init_flags |= KLINE_FLAG_INIT_FAST;
                pos += consumed;
            }
        }
    }

    /* --------------------------------------------------------------- */
    /* Phase 2: Decode UART bytes at 10400 baud                        */
    /* --------------------------------------------------------------- */
    uint8_t  raw_bytes[MAX_RAW_BYTES];
    uint32_t sample_pos[MAX_RAW_BYTES];

    uint32_t byte_count = uart_decode_bytes(samples + pos, num_samples - pos,
                                            sample_rate, KLINE_BAUD_STANDARD,
                                            threshold,
                                            raw_bytes, sample_pos,
                                            MAX_RAW_BYTES);

    /* Adjust sample positions relative to full buffer */
    for (uint32_t i = 0; i < byte_count; i++)
        sample_pos[i] += pos;

    /* --------------------------------------------------------------- */
    /* Phase 3: Parse KWP2000 messages from decoded bytes              */
    /* --------------------------------------------------------------- */
    uint32_t byte_idx = 0;

    while (byte_idx < byte_count && result->num_frames < DECODE_MAX_FRAMES) {
        decode_frame_t *f = &result->frames[result->num_frames];
        memset(f, 0, sizeof(*f));

        uint32_t consumed = parse_kwp_message(raw_bytes + byte_idx,
                                              byte_count - byte_idx,
                                              sample_pos + byte_idx,
                                              f);
        if (consumed == 0) {
            /* Can't parse a message here — skip one byte and try again */
            byte_idx++;
            continue;
        }

        /* Propagate init flags to all messages in this capture */
        f->flags |= init_flags;

        result->num_frames++;
        byte_idx += consumed;
    }
}
