#ifndef FLASH_FS_H
#define FLASH_FS_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FLASH_FS_OK = 0,
    FLASH_FS_ERR_MUTEX,
    FLASH_FS_ERR_MOUNT,
    FLASH_FS_ERR_OPEN,
    FLASH_FS_ERR_WRITE,
    FLASH_FS_ERR_READ,
    FLASH_FS_ERR_RENAME,
} flash_fs_error_t;

/* Initialize filesystem with mutex protection. Call once from main(). */
flash_fs_error_t flash_fs_init(void);

/* Write data atomically: writes to .tmp then renames. Mutex-protected. */
flash_fs_error_t flash_fs_write_atomic(const char *path, const void *data, uint32_t len);

/* Read data. Mutex-protected. */
flash_fs_error_t flash_fs_read(const char *path, void *buf, uint32_t buf_size, uint32_t *bytes_read);

/* Delete a file. Mutex-protected. */
flash_fs_error_t flash_fs_delete(const char *path);

/* Check if filesystem is initialized */
bool flash_fs_is_ready(void);

/* ═══════════════════════════════════════════════════════════════════
 * Raw SPI flash access
 *
 * These helpers bypass the filesystem layer and talk directly to the
 * external W25Q128 over SPI2. They are read-only and intended for
 * diagnostics, recovery, and bring-up over the USB CDC shell.
 * ═══════════════════════════════════════════════════════════════════ */

#define FLASH_FS_RAW_MAX_ADDR  (16u * 1024u * 1024u)

flash_fs_error_t flash_fs_raw_read_jedec(uint8_t *manufacturer,
                                         uint8_t *memory_type,
                                         uint8_t *capacity);
flash_fs_error_t flash_fs_raw_read_bytes(uint32_t addr, void *buf, uint32_t len);

/* ── Raw WRITE primitives (faithful reimpl of the stock W25Q128 driver) ──
 * Byte-exact from stock: WREN/RDSR-wait/sector-erase/page-program/page-loop/
 * smart-block-write (FUN_0802f344/f11c/ee9c/f36c/f2ac/f16c). THESE MODIFY FLASH.
 * The W25Q128 holds UI assets + screenshots in two FAT volumes — writing outside
 * a known-erased/free region corrupts them. Mutex-protected; same SPI2 bus as reads. */

/* Erase one 4KB sector (addr masked down to the sector boundary). */
flash_fs_error_t flash_fs_raw_sector_erase(uint32_t addr);

/* Program `len` bytes at `addr`, splitting across 256B pages. The target range
 * MUST already be erased (0xFF) — use flash_fs_raw_write_block to erase-as-needed. */
flash_fs_error_t flash_fs_raw_program(uint32_t addr, const void *data, uint32_t len);

/* Smart write: read-modify-write at 4KB granularity, erasing a sector only when a
 * target byte isn't already 0xFF (stock FUN_0802f16c). Safe for arbitrary addr/len. */
flash_fs_error_t flash_fs_raw_write_block(uint32_t addr, const void *data, uint32_t len);

/* Diagnostic: out[0..2] = status registers SR1/SR2/SR3; out[3] = SR1 after a WREN
 * (bit1 = WEL should read 1). Reveals block-protect bits + whether write-enable latches. */
flash_fs_error_t flash_fs_raw_status_diag(uint8_t out[4]);

/* ═══════════════════════════════════════════════════════════════════
 * Factory calibration
 *
 * The stock firmware stores per-channel calibration as 301 bytes
 * in SPI flash, loaded at boot into the RAM block at
 * 0x20000358-0x20000434 in the stock layout. We mirror that here
 * so drivers can read it via a single pointer.
 *
 * Currently a stub: the real SPI flash driver is not yet wired up,
 * so flash_fs_load_factory_cal() will mark the block as "not loaded"
 * and leave the mirror zeroed. Meter/scope paths must fall back to
 * built-in defaults until this is populated.
 * ═══════════════════════════════════════════════════════════════════ */

#define FACTORY_CAL_CHANNEL_SIZE  301u
#define FACTORY_CAL_NUM_CHANNELS  2u

typedef struct {
    bool     loaded;              /* true once both channels read OK */
    uint8_t  ch1[FACTORY_CAL_CHANNEL_SIZE];
    uint8_t  ch2[FACTORY_CAL_CHANNEL_SIZE];
} factory_cal_t;

/* Attempt to load factory cal from flash into the RAM mirror.
 * Safe to call even if flash_fs is not yet backed by a real driver —
 * on failure the mirror is zeroed and `loaded` stays false. */
flash_fs_error_t flash_fs_load_factory_cal(void);

/* Read-only pointer to the cal mirror. Never NULL. */
const factory_cal_t *flash_fs_factory_cal(void);

#endif
