/*
 * cal_backup_core.h — pure record/decision logic for factory-cal self-protection.
 *
 * WHAT THIS IS
 * ---------------------------------------------------------------------------
 * The per-device factory calibration lives in MCU-internal flash at 0x08006000
 * (a 4 KB page — see cal_dump.h and factory_cal_truth_2026-08-14.md). It is
 * NOT regenerable: every unit's page differs and there is no factory source we
 * can re-download. Our own reflashing is the standing hazard to it.
 *
 * This module is the HARDWARE-FREE half of the backup mechanism: it builds and
 * validates a versioned, CRC-protected backup record wrapping the raw page, and
 * it answers the one decision that must never be gotten wrong — "is it safe to
 * write this backup back over the live page?". Keeping it free of the AT32 HAL,
 * the W25Q driver and the volatile 0x08006000 pointer is what lets it be unit-
 * tested on the host (tests/test_cal_backup.c), where the recurring project
 * failure mode — a plausible, stable, wrong number — is cheap to catch.
 *
 * The device glue (reading the live page, storing to the W25Q, erasing and
 * programming MCU flash on restore) is in cal_backup.c.
 *
 * RECORD LAYOUT (little-endian, 4-byte aligned)
 *   offset  size  field
 *   0       4     magic         CAL_BACKUP_MAGIC
 *   4       2     version       CAL_BACKUP_VERSION
 *   6       2     payload_len   bytes of payload following the header
 *   8       4     src_addr      provenance: 0x08006000 (informational)
 *   12      4     payload_crc   CRC32 of the payload
 *   16      4     header_crc    CRC32 of bytes [0,16)
 *   20      ...   payload       payload_len bytes (the raw cal page)
 */

#ifndef CAL_BACKUP_CORE_H
#define CAL_BACKUP_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CAL_BACKUP_MAGIC        0x4C414342u   /* 'B','C','A','L' little-endian */
#define CAL_BACKUP_VERSION      1u

/* The MCU-internal saved-config page this module protects. */
#define CAL_BACKUP_SRC_ADDR     0x08006000u
#define CAL_BACKUP_PAYLOAD_LEN  0x1000u        /* 4 KB — the whole page, verbatim */

#define CAL_BACKUP_HEADER_LEN   20u
#define CAL_BACKUP_RECORD_LEN   (CAL_BACKUP_HEADER_LEN + CAL_BACKUP_PAYLOAD_LEN)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_len;
    uint32_t src_addr;
    uint32_t payload_crc;
    uint32_t header_crc;
} cal_backup_header_t;

/* Result of parsing/validating a stored record. */
typedef enum {
    CAL_REC_OK = 0,
    CAL_REC_ERR_TRUNCATED,      /* fewer than a header's worth of bytes         */
    CAL_REC_ERR_MAGIC,          /* magic mismatch (blank/foreign/erased slot)   */
    CAL_REC_ERR_VERSION,        /* version this build does not understand       */
    CAL_REC_ERR_LEN,            /* payload_len is 0 or exceeds the buffer        */
    CAL_REC_ERR_HEADER_CRC,     /* header corrupt                                */
    CAL_REC_ERR_PAYLOAD_CRC,    /* payload corrupt                               */
} cal_rec_status_t;

/* How the live 4 KB page reads, for the restore decision. */
typedef enum {
    CAL_PAGE_BLANK = 0,         /* every byte 0xFF — erased, nothing there       */
    CAL_PAGE_ZEROED,            /* every byte 0x00 — wiped, nothing there        */
    CAL_PAGE_PROGRAMMED,        /* has real content — treat as precious          */
} cal_page_class_t;

/* CRC32/IEEE-802.3, reflected, init 0xFFFFFFFF, final XOR 0xFFFFFFFF — the same
 * parameters flash_regions.c and cal_dump.c use, so a value computed here, on
 * the device, or by an external tool all agree. */
uint32_t cal_backup_crc32(const void *data, uint32_t len);

/* Incremental form, so the device can CRC the 4 KB page in small chunks without
 * a 4 KB RAM buffer (RAM is nearly full). Seed with CAL_BACKUP_CRC32_INIT, fold
 * each chunk with _update, finalise with _final:
 *     uint32_t c = CAL_BACKUP_CRC32_INIT;
 *     c = cal_backup_crc32_update(c, chunk, n);   // repeat
 *     uint32_t crc = cal_backup_crc32_final(c);
 * cal_backup_crc32() is exactly init/update/final over one buffer. */
#define CAL_BACKUP_CRC32_INIT  0xFFFFFFFFu
uint32_t cal_backup_crc32_update(uint32_t crc, const void *data, uint32_t len);
uint32_t cal_backup_crc32_final(uint32_t crc);

/* Serialise the 20-byte record header (including header_crc) into out. The
 * payload CRC is supplied by the caller (who may have streamed it). */
void cal_backup_write_header(uint8_t out[CAL_BACKUP_HEADER_LEN],
                             uint16_t version, uint16_t payload_len,
                             uint32_t src_addr, uint32_t payload_crc);

/* Validate the header alone: magic, header_crc, version, payload_len != 0. Does
 * NOT check the payload (the device verifies payload_crc by streaming). `rec`
 * must hold at least CAL_BACKUP_HEADER_LEN bytes. Fills *hdr_out on OK. */
cal_rec_status_t cal_backup_check_header(const uint8_t *rec, uint32_t rec_len,
                                         cal_backup_header_t *hdr_out);

/* Build a record for `payload` into `out`. Returns the total record length, or
 * 0 on bad argument (null, zero length, out too small). */
uint32_t cal_backup_build_record(const void *payload, uint16_t payload_len,
                                 uint16_t version, uint32_t src_addr,
                                 uint8_t *out, uint32_t out_cap);

/* Parse and fully validate `rec`. On CAL_REC_OK, *hdr_out is filled and
 * *payload_out points at the payload inside `rec`. Any out pointer may be NULL. */
cal_rec_status_t cal_backup_parse(const uint8_t *rec, uint32_t rec_len,
                                  cal_backup_header_t *hdr_out,
                                  const uint8_t **payload_out);

/* Classify a page already copied into RAM. */
cal_page_class_t cal_backup_classify_page(const uint8_t *page, uint32_t len);

/* THE load-bearing decision. Auto-restore is permitted ONLY when a valid backup
 * exists AND the live page carries nothing worth keeping. A programmed page is
 * NEVER overwritten automatically — recovering a suspect page is a human call. */
bool cal_backup_should_auto_restore(cal_page_class_t live, bool backup_valid);

/* Human-readable strings for logs/shell. Never NULL. */
const char *cal_rec_status_str(cal_rec_status_t s);
const char *cal_page_class_str(cal_page_class_t c);

#endif /* CAL_BACKUP_CORE_H */
