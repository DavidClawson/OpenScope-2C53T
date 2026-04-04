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
