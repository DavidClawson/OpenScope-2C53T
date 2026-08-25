/*
 * cal_backup.c — device glue for factory-cal self-protection. See cal_backup.h.
 *
 * All record/decision logic is in cal_backup_core.c (host-tested). This file is
 * only the three things the core cannot do: read the live MCU page, move the
 * record to/from the dedicated W25Q region, and — on restore only — erase and
 * program MCU-internal flash.
 *
 * MEMORY: RAM is nearly full (the image already sits within ~1 KB of the 224 KB
 * ceiling), so this holds NO 4 KB buffer. The 4 KB page is always streamed in
 * 256-byte chunks — CRC'd incrementally, copied chunk-at-a-time — so the only
 * working storage is one 256-byte stack buffer, safe even on the 2 KB usb_dbg
 * shell task. Every entry point runs on that single shell/boot thread.
 */

#include "cal_backup.h"

#include <string.h>

#include "flash_regions.h"

#ifndef EMULATOR_BUILD
#include "at32f403a_407.h"
#endif

#define CAL_BK_REGION   FLASH_REGION_FACTORY_CAL_BACKUP
#define CAL_BK_CHUNK    256u
#define CAL_BK_PAYLOAD_OFF  CAL_BACKUP_HEADER_LEN   /* payload follows the header */

/* ── live MCU page (read-only, streamed) ────────────────────────────── */

static void read_live_chunk(uint32_t off, uint8_t *dst, uint32_t n)
{
#ifndef EMULATOR_BUILD
    const volatile uint8_t *src = (const volatile uint8_t *)(CAL_BACKUP_SRC_ADDR + off);
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
#else
    (void)off;
    memset(dst, 0xFF, n);
#endif
}

/* One pass over the live page: its CRC32 and its class (blank/zeroed/programmed).
 * Classification combines per-chunk verdicts — any disagreement between chunks,
 * or any programmed chunk, makes the whole page programmed. */
static void live_scan(uint32_t *crc_out, cal_page_class_t *class_out)
{
    uint8_t buf[CAL_BK_CHUNK];
    uint32_t crc = CAL_BACKUP_CRC32_INIT;
    cal_page_class_t cls = CAL_PAGE_BLANK;
    bool first = true;

    for (uint32_t off = 0; off < CAL_BACKUP_PAYLOAD_LEN; off += CAL_BK_CHUNK) {
        read_live_chunk(off, buf, CAL_BK_CHUNK);
        crc = cal_backup_crc32_update(crc, buf, CAL_BK_CHUNK);
        cal_page_class_t c = cal_backup_classify_page(buf, CAL_BK_CHUNK);
        cls = first ? c : (cls == c ? cls : CAL_PAGE_PROGRAMMED);
        first = false;
    }
    if (crc_out != NULL)   { *crc_out = cal_backup_crc32_final(crc); }
    if (class_out != NULL) { *class_out = cls; }
}

/* ── W25Q backup region (streamed) ──────────────────────────────────── */

/* CRC32 of the payload as it is currently stored in the W25Q region. Returns
 * false on a region read error. */
static bool region_payload_crc(uint32_t *crc_out)
{
    uint8_t buf[CAL_BK_CHUNK];
    uint32_t crc = CAL_BACKUP_CRC32_INIT;
    for (uint32_t off = 0; off < CAL_BACKUP_PAYLOAD_LEN; off += CAL_BK_CHUNK) {
        if (flash_region_read(CAL_BK_REGION, CAL_BK_PAYLOAD_OFF + off,
                              buf, CAL_BK_CHUNK) != FLASH_REGION_OK) {
            return false;
        }
        crc = cal_backup_crc32_update(crc, buf, CAL_BK_CHUNK);
    }
    *crc_out = cal_backup_crc32_final(crc);
    return true;
}

/* Read and fully validate the stored backup: header well-formed AND the stored
 * payload's CRC matches the header. Fills *hdr on success. `present` (may be
 * NULL) reports whether the header bytes could be read at all. */
static bool backup_is_valid(cal_backup_header_t *hdr, bool *present)
{
    uint8_t hbuf[CAL_BACKUP_HEADER_LEN];
    if (present != NULL) { *present = false; }
    if (flash_region_read(CAL_BK_REGION, 0, hbuf, sizeof hbuf) != FLASH_REGION_OK) {
        return false;
    }
    if (present != NULL) { *present = true; }

    cal_backup_header_t h;
    if (cal_backup_check_header(hbuf, sizeof hbuf, &h) != CAL_REC_OK) {
        return false;
    }
    if (h.payload_len != CAL_BACKUP_PAYLOAD_LEN) {
        return false;
    }
    uint32_t stored_crc = 0;
    if (!region_payload_crc(&stored_crc) || stored_crc != h.payload_crc) {
        return false;
    }
    if (hdr != NULL) { *hdr = h; }
    return true;
}

cal_bk_status_t cal_backup_store(void)
{
    uint32_t live_crc = 0;
    cal_page_class_t live_class = CAL_PAGE_BLANK;
    live_scan(&live_crc, &live_class);

    /* Never overwrite an existing backup with an empty capture. */
    if (live_class != CAL_PAGE_PROGRAMMED) {
        return CAL_BK_ERR_LIVE_BLANK;
    }

    uint8_t hbuf[CAL_BACKUP_HEADER_LEN];
    cal_backup_write_header(hbuf, (uint16_t)CAL_BACKUP_VERSION,
                            (uint16_t)CAL_BACKUP_PAYLOAD_LEN,
                            CAL_BACKUP_SRC_ADDR, live_crc);

    if (flash_region_reset(CAL_BK_REGION) != FLASH_REGION_OK) {
        return CAL_BK_ERR_REGION_IO;
    }
    /* Payload first, header last: a torn write then leaves the header absent or
     * mismatched and backup_is_valid() rejects it — never a header that claims a
     * payload that was not fully written. */
    uint8_t buf[CAL_BK_CHUNK];
    for (uint32_t off = 0; off < CAL_BACKUP_PAYLOAD_LEN; off += CAL_BK_CHUNK) {
        read_live_chunk(off, buf, CAL_BK_CHUNK);
        if (flash_region_write(CAL_BK_REGION, CAL_BK_PAYLOAD_OFF + off,
                               buf, CAL_BK_CHUNK) != FLASH_REGION_OK) {
            return CAL_BK_ERR_REGION_IO;
        }
    }
    if (flash_region_write(CAL_BK_REGION, 0, hbuf, sizeof hbuf) != FLASH_REGION_OK) {
        return CAL_BK_ERR_REGION_IO;
    }

    /* Verify by re-validating what actually landed. */
    cal_backup_header_t h;
    if (!backup_is_valid(&h, NULL) || h.payload_crc != live_crc) {
        return CAL_BK_ERR_VERIFY;
    }
    return CAL_BK_OK;
}

cal_bk_status_t cal_backup_status(cal_backup_report_t *r)
{
    if (r == NULL) {
        return CAL_BK_ERR_ARG;
    }
    memset(r, 0, sizeof(*r));

    live_scan(&r->live_crc, &r->live_class);

    /* Read the header for reporting even if the payload is corrupt, so status
     * can distinguish "no record" from "record present but invalid". */
    uint8_t hbuf[CAL_BACKUP_HEADER_LEN];
    bool present = (flash_region_read(CAL_BK_REGION, 0, hbuf, sizeof hbuf) == FLASH_REGION_OK);
    r->backup_present = present;

    cal_backup_header_t h;
    if (present) {
        r->backup_status = cal_backup_check_header(hbuf, sizeof hbuf, &h);
    } else {
        r->backup_status = CAL_REC_ERR_TRUNCATED;
    }

    if (r->backup_status == CAL_REC_OK) {
        r->backup_version     = h.version;
        r->backup_payload_crc = h.payload_crc;
        r->backup_src_addr    = h.src_addr;
        /* Fully validate (payload CRC) before claiming a match. */
        uint32_t stored_crc = 0;
        bool payload_ok = (h.payload_len == CAL_BACKUP_PAYLOAD_LEN) &&
                          region_payload_crc(&stored_crc) &&
                          stored_crc == h.payload_crc;
        if (!payload_ok) {
            r->backup_status = CAL_REC_ERR_PAYLOAD_CRC;
        } else {
            r->match = (h.payload_crc == r->live_crc);
        }
    }
    return CAL_BK_OK;
}

/* ── restore: W25Q -> MCU flash. The dangerous direction. ───────────── */

#ifndef EMULATOR_BUILD
/* Erase the sectors covering the cal page, then program it from the W25Q backup
 * payload, streamed. Erases in 2 KB steps so the 4 KB page is fully cleared
 * whether the part's sector is 2 KB or 4 KB (a repeat erase of one 4 KB sector
 * is a harmless no-op). Returns false on any erase/program/region-read failure. */
static bool mcu_flash_restore_from_region(void)
{
    bool ok = true;
    flash_unlock();

    for (uint32_t off = 0; off < CAL_BACKUP_PAYLOAD_LEN; off += 2048u) {
        if (flash_sector_erase(CAL_BACKUP_SRC_ADDR + off) != FLASH_OPERATE_DONE) {
            ok = false;
            break;
        }
    }
    if (ok) {
        uint8_t buf[CAL_BK_CHUNK];
        for (uint32_t off = 0; off < CAL_BACKUP_PAYLOAD_LEN && ok; off += CAL_BK_CHUNK) {
            if (flash_region_read(CAL_BK_REGION, CAL_BK_PAYLOAD_OFF + off,
                                  buf, CAL_BK_CHUNK) != FLASH_REGION_OK) {
                ok = false;
                break;
            }
            for (uint32_t i = 0; i < CAL_BK_CHUNK; i += 4u) {
                uint32_t w = (uint32_t)buf[i] |
                             ((uint32_t)buf[i + 1] << 8) |
                             ((uint32_t)buf[i + 2] << 16) |
                             ((uint32_t)buf[i + 3] << 24);
                if (flash_word_program(CAL_BACKUP_SRC_ADDR + off + i, w) != FLASH_OPERATE_DONE) {
                    ok = false;
                    break;
                }
            }
        }
    }

    flash_lock();
    return ok;
}
#endif /* !EMULATOR_BUILD */

cal_bk_status_t cal_backup_restore(bool force)
{
    cal_backup_header_t h;
    if (!backup_is_valid(&h, NULL)) {
        return CAL_BK_ERR_NO_BACKUP;
    }
    if ((h.payload_len & 3u) != 0u) {   /* must be word-programmable */
        return CAL_BK_ERR_NO_BACKUP;
    }

    uint32_t live_crc = 0;
    cal_page_class_t live_class = CAL_PAGE_BLANK;
    live_scan(&live_crc, &live_class);

    /* Refuse to clobber a live page that still holds real content. */
    if (live_class == CAL_PAGE_PROGRAMMED && !force) {
        return CAL_BK_ERR_LIVE_PRECIOUS;
    }
    /* Already correct — skip a needless MCU-flash P/E cycle. */
    if (live_crc == h.payload_crc) {
        return CAL_BK_OK;
    }

#ifndef EMULATOR_BUILD
    if (!mcu_flash_restore_from_region()) {
        return CAL_BK_ERR_MCU_FLASH;
    }
    /* Verify the page now matches the backup. */
    live_scan(&live_crc, NULL);
    if (live_crc != h.payload_crc) {
        return CAL_BK_ERR_MCU_FLASH;
    }
    return CAL_BK_OK;
#else
    return CAL_BK_ERR_MCU_FLASH;
#endif
}

cal_bk_status_t cal_backup_maybe_auto_restore(void)
{
    cal_backup_header_t h;
    bool backup_valid = backup_is_valid(&h, NULL);

    cal_page_class_t live_class = CAL_PAGE_BLANK;
    live_scan(NULL, &live_class);

    if (!cal_backup_should_auto_restore(live_class, backup_valid)) {
        return CAL_BK_OK;   /* deliberate no-op */
    }
    return cal_backup_restore(false);
}

const char *cal_bk_status_str(cal_bk_status_t s)
{
    switch (s) {
    case CAL_BK_OK:               return "ok";
    case CAL_BK_ERR_LIVE_BLANK:   return "live page blank — nothing to back up";
    case CAL_BK_ERR_REGION_IO:    return "W25Q region io error";
    case CAL_BK_ERR_VERIFY:       return "backup readback/verify failed";
    case CAL_BK_ERR_NO_BACKUP:    return "no valid backup on W25Q";
    case CAL_BK_ERR_LIVE_PRECIOUS:return "refused: live page is programmed (use force)";
    case CAL_BK_ERR_MCU_FLASH:    return "MCU flash erase/program/verify failed";
    case CAL_BK_ERR_ARG:          return "bad argument";
    }
    return "unknown";
}
