/*
 * cal_backup.h — factory-cal self-protection: back the MCU cal page up to the
 * external W25Q, and (deliberately, manually) restore it.
 *
 * WHY (2026-08-25)
 * ---------------------------------------------------------------------------
 * The per-device factory calibration is a 4 KB page in MCU-internal flash at
 * 0x08006000. It is NOT regenerable — see cal_backup_core.h and
 * factory_cal_truth_2026-08-14.md. Reflashing this device is the standing
 * threat to it, and we are about to reflash a great deal (the FPGA config-
 * transport bisect). The safe ordering is: capture the page to a place a
 * reflash cannot reach — the external W25Q — BEFORE the reflash spree.
 *
 * cal_dump.c already proved the CPU can READ its own protected flash (an
 * external debugger cannot, and merely attaching one kills this part). This
 * module adds the write side, split so the dangerous direction is opt-in:
 *
 *   cal_backup_store()   MCU page  -> W25Q   SAFE. Only writes the W25Q, which
 *                                            has a fixed, verified driver.
 *   cal_backup_status()  read-only inspection of both sides.
 *   cal_backup_restore() W25Q -> MCU page    DANGEROUS. Erases + programs MCU
 *                                            flash. Refuses to overwrite a
 *                                            programmed (precious) page unless
 *                                            explicitly forced.
 *
 * STORAGE — flagged for review. The backup lives in a DEDICATED W25Q region
 * (FLASH_REGION_FACTORY_CAL_BACKUP), not a FAT file and not MCU flash:
 *   - Not a FAT file: those live in stock-owned read-only volumes, and the
 *     project forbids inventing cal filenames (scripts/validate_dmm_goal.py).
 *   - Not MCU flash: the entire point is to survive an MCU-flash erase.
 *   - A dedicated region, not scratch/modules: the region layer's default-deny
 *     means a screenshot stream or a settings write can never reach it. This
 *     is data we cannot regenerate.
 * The region is carved from the tail of the general scratch region, inside the
 * 0xF00000+ area proven blank on bench unit #1 — the same basis as usercal,
 * settings, modules and scratch. See flash_regions.c.
 *
 * AUTO-RESTORE at boot is compiled OUT by default (CAL_BACKUP_AUTO_RESTORE=0).
 * Manual `cal restore` is the primary path. Even when enabled it only acts on a
 * BLANK/ZEROED page — cal_backup_should_auto_restore() never overwrites content.
 */

#ifndef CAL_BACKUP_H
#define CAL_BACKUP_H

#include <stdint.h>
#include <stdbool.h>

#include "cal_backup_core.h"

typedef enum {
    CAL_BK_OK = 0,
    CAL_BK_ERR_LIVE_BLANK,      /* nothing on the MCU page worth backing up      */
    CAL_BK_ERR_REGION_IO,       /* W25Q region read/write/erase failed           */
    CAL_BK_ERR_VERIFY,          /* stored record did not read back / re-validate */
    CAL_BK_ERR_NO_BACKUP,       /* no valid record in the W25Q region            */
    CAL_BK_ERR_LIVE_PRECIOUS,   /* restore refused: live page is programmed      */
    CAL_BK_ERR_MCU_FLASH,       /* MCU-flash erase/program/verify failed         */
    CAL_BK_ERR_ARG,             /* bad argument / not initialised                */
} cal_bk_status_t;

typedef struct {
    /* backup side (W25Q) */
    bool             backup_present;   /* a record parsed, whatever its verdict  */
    cal_rec_status_t backup_status;    /* CAL_REC_OK iff the record fully validates */
    uint16_t         backup_version;
    uint32_t         backup_payload_crc;
    uint32_t         backup_src_addr;
    /* live side (MCU 0x08006000) */
    cal_page_class_t live_class;
    uint32_t         live_crc;         /* CRC32 of the live 4 KB page             */
    /* relationship */
    bool             match;            /* live page byte-identical to the backup  */
} cal_backup_report_t;

/* Capture the live MCU page into the W25Q backup region. Refuses (with
 * CAL_BK_ERR_LIVE_BLANK) to overwrite an existing backup with a blank/zeroed
 * capture. Verifies the stored record by reading it back and re-validating. */
cal_bk_status_t cal_backup_store(void);

/* Fill *r from both sides. Read-only. Returns CAL_BK_OK unless a read failed. */
cal_bk_status_t cal_backup_status(cal_backup_report_t *r);

/* Restore the W25Q backup to MCU flash 0x08006000. Requires a valid backup.
 * With force==false, refuses if the live page is programmed (CAL_BK_ERR_LIVE_PRECIOUS).
 * Erases + programs MCU flash and verifies the readback. */
cal_bk_status_t cal_backup_restore(bool force);

/* Boot hook: restore ONLY if the live page is blank/zeroed AND a valid backup
 * exists (cal_backup_should_auto_restore). A no-op otherwise. Not called unless
 * CAL_BACKUP_AUTO_RESTORE is defined non-zero. */
cal_bk_status_t cal_backup_maybe_auto_restore(void);

const char *cal_bk_status_str(cal_bk_status_t s);

#endif /* CAL_BACKUP_H */
