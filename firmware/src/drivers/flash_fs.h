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

#endif
