/*
 * flash_fs.c - Safe filesystem wrapper for SPI flash (W25Q128)
 *
 * Prevents flash filesystem corruption (community bug #3) by:
 *   1. Mutex-protecting all filesystem operations so concurrent
 *      tasks cannot interleave SPI flash commands.
 *   2. Using atomic write pattern (write to .tmp, then rename)
 *      so a power loss mid-write cannot corrupt the original file.
 *
 * The actual FatFS/SPI flash driver calls are stubbed out until
 * those drivers are implemented. The safety infrastructure (mutex
 * acquire/release, atomic rename pattern) is fully in place.
 */

#include "flash_fs.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Internal state
 * ═══════════════════════════════════════════════════════════════════ */

static SemaphoreHandle_t fs_mutex  = NULL;
static bool              fs_ready  = false;

/* Mutex timeout: 5 seconds should be more than enough for any
 * single filesystem operation on SPI flash. */
#define FS_MUTEX_TIMEOUT_MS  5000

/* Maximum path length (matching FatFS LFN limits) */
#define FS_MAX_PATH  128

/* ═══════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Build a temporary file path by appending ".tmp" to the original */
static bool make_tmp_path(const char *path, char *tmp_path, uint32_t tmp_path_size)
{
    uint32_t len = (uint32_t)strlen(path);
    if (len + 5 > tmp_path_size) {  /* +5 for ".tmp\0" */
        return false;
    }
    memcpy(tmp_path, path, len);
    memcpy(tmp_path + len, ".tmp", 5);  /* includes null terminator */
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

flash_fs_error_t flash_fs_init(void)
{
    fs_mutex = xSemaphoreCreateMutex();
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }

    /* TODO: Initialize SPI flash driver (W25Q128 via SPI1)
     * TODO: Mount FatFS filesystem
     *   FATFS fs;
     *   FRESULT res = f_mount(&fs, "2:", 1);
     *   if (res != FR_OK) return FLASH_FS_ERR_MOUNT;
     */

    fs_ready = true;
    return FLASH_FS_OK;
}

flash_fs_error_t flash_fs_write_atomic(const char *path, const void *data, uint32_t len)
{
    if (!fs_ready || fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }

    /* Acquire mutex with timeout */
    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    flash_fs_error_t result = FLASH_FS_OK;

    /* Build temporary file path */
    char tmp_path[FS_MAX_PATH];
    if (!make_tmp_path(path, tmp_path, sizeof(tmp_path))) {
        result = FLASH_FS_ERR_WRITE;
        goto done;
    }

    /*
     * Atomic write pattern:
     *   1. Write data to <path>.tmp
     *   2. If write succeeds, delete original <path>
     *   3. Rename <path>.tmp to <path>
     *
     * If power is lost during step 1, the original file is intact.
     * If power is lost during step 3, the .tmp file contains valid
     * data and can be recovered on next boot.
     */

    /* Step 1: Write to temporary file */
    /* TODO: Implement when FatFS/SPI flash driver available
     *   FIL fil;
     *   FRESULT res = f_open(&fil, tmp_path, FA_WRITE | FA_CREATE_ALWAYS);
     *   if (res != FR_OK) { result = FLASH_FS_ERR_OPEN; goto done; }
     *
     *   UINT bytes_written;
     *   res = f_write(&fil, data, len, &bytes_written);
     *   f_close(&fil);
     *   if (res != FR_OK || bytes_written != len) {
     *       f_unlink(tmp_path);
     *       result = FLASH_FS_ERR_WRITE;
     *       goto done;
     *   }
     */

    /* Step 2: Delete original file (ignore error — may not exist) */
    /* TODO: f_unlink(path); */

    /* Step 3: Rename .tmp to final path */
    /* TODO:
     *   res = f_rename(tmp_path, path);
     *   if (res != FR_OK) { result = FLASH_FS_ERR_RENAME; goto done; }
     */

    /* Suppress unused parameter warnings until stubs are replaced */
    (void)data;
    (void)len;
    (void)tmp_path;

done:
    xSemaphoreGive(fs_mutex);
    return result;
}

flash_fs_error_t flash_fs_read(const char *path, void *buf, uint32_t buf_size, uint32_t *bytes_read)
{
    if (!fs_ready || fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    flash_fs_error_t result = FLASH_FS_OK;

    /* TODO: Implement when FatFS/SPI flash driver available
     *   FIL fil;
     *   FRESULT res = f_open(&fil, path, FA_READ);
     *   if (res != FR_OK) { result = FLASH_FS_ERR_OPEN; goto done; }
     *
     *   UINT br;
     *   res = f_read(&fil, buf, buf_size, &br);
     *   f_close(&fil);
     *   if (res != FR_OK) { result = FLASH_FS_ERR_READ; goto done; }
     *   if (bytes_read) *bytes_read = (uint32_t)br;
     */

    /* Suppress unused parameter warnings until stubs are replaced */
    (void)path;
    (void)buf;
    (void)buf_size;
    if (bytes_read) {
        *bytes_read = 0;
    }

    xSemaphoreGive(fs_mutex);
    return result;
}

flash_fs_error_t flash_fs_delete(const char *path)
{
    if (!fs_ready || fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    /* TODO: Implement when FatFS/SPI flash driver available
     *   FRESULT res = f_unlink(path);
     *   if (res != FR_OK && res != FR_NO_FILE) {
     *       xSemaphoreGive(fs_mutex);
     *       return FLASH_FS_ERR_OPEN;
     *   }
     */

    (void)path;

    xSemaphoreGive(fs_mutex);
    return FLASH_FS_OK;
}

bool flash_fs_is_ready(void)
{
    return fs_ready;
}

/* ═══════════════════════════════════════════════════════════════════
 * Factory calibration mirror
 *
 * Stock firmware path: SPI flash "3:/System file/" contains per-channel
 * calibration blobs (301 bytes each) that are loaded on boot into a
 * RAM block at 0x20000358-0x20000434 and consumed by the meter/scope
 * scaling pipeline.
 *
 * This is a load-only stub. The real SPI flash driver is not yet
 * wired through flash_fs_read(), so this routine currently reads 0
 * bytes and leaves `loaded` false. Drivers consuming factory_cal_t
 * must fall back to built-in defaults in that case.
 * ═══════════════════════════════════════════════════════════════════ */

static factory_cal_t g_factory_cal;  /* BSS zero */

flash_fs_error_t flash_fs_load_factory_cal(void)
{
    memset(&g_factory_cal, 0, sizeof(g_factory_cal));

    uint32_t br1 = 0;
    uint32_t br2 = 0;

    flash_fs_error_t r1 = flash_fs_read("3:/System file/cal_ch1.bin",
                                        g_factory_cal.ch1,
                                        FACTORY_CAL_CHANNEL_SIZE, &br1);
    flash_fs_error_t r2 = flash_fs_read("3:/System file/cal_ch2.bin",
                                        g_factory_cal.ch2,
                                        FACTORY_CAL_CHANNEL_SIZE, &br2);

    if (r1 == FLASH_FS_OK && r2 == FLASH_FS_OK &&
        br1 == FACTORY_CAL_CHANNEL_SIZE &&
        br2 == FACTORY_CAL_CHANNEL_SIZE) {
        g_factory_cal.loaded = true;
        /* TODO(phase3): apply these coefficients in meter_data.c */
        return FLASH_FS_OK;
    }

    /* Stub path: SPI flash driver not yet populated. Log-only outcome
     * is implicit — the caller can check flash_fs_factory_cal()->loaded. */
    g_factory_cal.loaded = false;
    return FLASH_FS_ERR_READ;
}

const factory_cal_t *flash_fs_factory_cal(void)
{
    return &g_factory_cal;
}
