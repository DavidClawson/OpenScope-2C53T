/*
 * cal_backup_core.c — see cal_backup_core.h. Hardware-free; host-tested.
 */

#include "cal_backup_core.h"

#include <string.h>

/* ── CRC32 (IEEE-802.3, reflected). Bitwise: no 1 KB table, imperceptible on
 * 4 KB at 240 MHz, and identical to flash_regions.c / cal_dump.c. ── */
uint32_t cal_backup_crc32_update(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

uint32_t cal_backup_crc32_final(uint32_t crc)
{
    return ~crc;
}

uint32_t cal_backup_crc32(const void *data, uint32_t len)
{
    return cal_backup_crc32_final(
        cal_backup_crc32_update(CAL_BACKUP_CRC32_INIT, data, len));
}

/* Little-endian pack/unpack helpers keep the on-flash layout host-independent. */
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void cal_backup_write_header(uint8_t out[CAL_BACKUP_HEADER_LEN],
                             uint16_t version, uint16_t payload_len,
                             uint32_t src_addr, uint32_t payload_crc)
{
    put_u32(out + 0, CAL_BACKUP_MAGIC);
    put_u16(out + 4, version);
    put_u16(out + 6, payload_len);
    put_u32(out + 8, src_addr);
    put_u32(out + 12, payload_crc);
    /* header_crc covers the 16 bytes above; write it last. */
    put_u32(out + 16, cal_backup_crc32(out, 16u));
}

cal_rec_status_t cal_backup_check_header(const uint8_t *rec, uint32_t rec_len,
                                         cal_backup_header_t *hdr_out)
{
    if (rec == NULL || rec_len < CAL_BACKUP_HEADER_LEN) {
        return CAL_REC_ERR_TRUNCATED;
    }

    cal_backup_header_t h;
    h.magic       = get_u32(rec + 0);
    h.version     = get_u16(rec + 4);
    h.payload_len = get_u16(rec + 6);
    h.src_addr    = get_u32(rec + 8);
    h.payload_crc = get_u32(rec + 12);
    h.header_crc  = get_u32(rec + 16);

    if (h.magic != CAL_BACKUP_MAGIC) {
        return CAL_REC_ERR_MAGIC;
    }
    if (cal_backup_crc32(rec, 16u) != h.header_crc) {
        return CAL_REC_ERR_HEADER_CRC;
    }
    if (h.version != CAL_BACKUP_VERSION) {
        return CAL_REC_ERR_VERSION;
    }
    if (h.payload_len == 0u) {
        return CAL_REC_ERR_LEN;
    }

    if (hdr_out != NULL) {
        *hdr_out = h;
    }
    return CAL_REC_OK;
}

uint32_t cal_backup_build_record(const void *payload, uint16_t payload_len,
                                 uint16_t version, uint32_t src_addr,
                                 uint8_t *out, uint32_t out_cap)
{
    if (payload == NULL || out == NULL || payload_len == 0u) {
        return 0u;
    }
    uint32_t total = (uint32_t)CAL_BACKUP_HEADER_LEN + payload_len;
    if (out_cap < total) {
        return 0u;
    }

    cal_backup_write_header(out, version, payload_len, src_addr,
                            cal_backup_crc32(payload, payload_len));
    memcpy(out + CAL_BACKUP_HEADER_LEN, payload, payload_len);
    return total;
}

cal_rec_status_t cal_backup_parse(const uint8_t *rec, uint32_t rec_len,
                                  cal_backup_header_t *hdr_out,
                                  const uint8_t **payload_out)
{
    cal_backup_header_t h;
    cal_rec_status_t st = cal_backup_check_header(rec, rec_len, &h);
    if (st != CAL_REC_OK) {
        return st;
    }
    if ((uint32_t)CAL_BACKUP_HEADER_LEN + h.payload_len > rec_len) {
        return CAL_REC_ERR_LEN;
    }
    if (cal_backup_crc32(rec + CAL_BACKUP_HEADER_LEN, h.payload_len) != h.payload_crc) {
        return CAL_REC_ERR_PAYLOAD_CRC;
    }

    if (hdr_out != NULL) {
        *hdr_out = h;
    }
    if (payload_out != NULL) {
        *payload_out = rec + CAL_BACKUP_HEADER_LEN;
    }
    return CAL_REC_OK;
}

cal_page_class_t cal_backup_classify_page(const uint8_t *page, uint32_t len)
{
    if (page == NULL || len == 0u) {
        return CAL_PAGE_BLANK;
    }
    bool all_ff = true;
    bool all_00 = true;
    for (uint32_t i = 0; i < len; i++) {
        if (page[i] != 0xFFu) { all_ff = false; }
        if (page[i] != 0x00u) { all_00 = false; }
        if (!all_ff && !all_00) { return CAL_PAGE_PROGRAMMED; }
    }
    if (all_ff) { return CAL_PAGE_BLANK; }
    if (all_00) { return CAL_PAGE_ZEROED; }
    return CAL_PAGE_PROGRAMMED;
}

bool cal_backup_should_auto_restore(cal_page_class_t live, bool backup_valid)
{
    if (!backup_valid) {
        return false;
    }
    /* Only when the live page holds nothing. A programmed page is precious and
     * is never touched without a human. */
    return live == CAL_PAGE_BLANK || live == CAL_PAGE_ZEROED;
}

const char *cal_rec_status_str(cal_rec_status_t s)
{
    switch (s) {
    case CAL_REC_OK:              return "ok";
    case CAL_REC_ERR_TRUNCATED:   return "truncated";
    case CAL_REC_ERR_MAGIC:       return "no backup (bad magic)";
    case CAL_REC_ERR_VERSION:     return "unknown version";
    case CAL_REC_ERR_LEN:         return "bad length";
    case CAL_REC_ERR_HEADER_CRC:  return "header crc fail";
    case CAL_REC_ERR_PAYLOAD_CRC: return "payload crc fail";
    }
    return "unknown";
}

const char *cal_page_class_str(cal_page_class_t c)
{
    switch (c) {
    case CAL_PAGE_BLANK:      return "blank (0xFF)";
    case CAL_PAGE_ZEROED:     return "zeroed (0x00)";
    case CAL_PAGE_PROGRAMMED: return "programmed";
    }
    return "unknown";
}
