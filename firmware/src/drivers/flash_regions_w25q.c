/*
 * flash_regions_w25q.c — device binding for the region layer.
 *
 * Kept apart from flash_regions.c so the policy code stays free of firmware
 * dependencies and the host tests link the real thing rather than a copy.
 *
 * The three primitives map onto the existing flash_fs raw driver, which already
 * takes the filesystem mutex, so SPI2 access stays serialised against the USB
 * shell's flash commands.
 *
 * NOTE ON THE ESCAPE HATCH: flash_fs_raw_sector_erase() / _program() /
 * _write_block() remain reachable directly and are NOT policed by this layer.
 * They are maintainer bench diagnostics (`flash wtest`, which refuses any sector
 * that is not already blank). Every automated writer should go through
 * flash_regions_* instead; that is the whole point of the region table. If the
 * raw primitives should be locked down too, the hook is a
 * flash_regions_check_abs() call at the top of each public wrapper in
 * flash_fs.c — deliberately not done here, because it would change the
 * behaviour of a bench tool this task does not own.
 */

#include "flash_regions.h"
#include "flash_fs.h"

static int w25q_read(void *ctx, uint32_t addr, void *buf, uint32_t len)
{
    (void)ctx;
    return (flash_fs_raw_read_bytes(addr, buf, len) == FLASH_FS_OK) ? 0 : -1;
}

static int w25q_erase_sector(void *ctx, uint32_t addr)
{
    (void)ctx;
    return (flash_fs_raw_sector_erase(addr) == FLASH_FS_OK) ? 0 : -1;
}

/* The region layer only ever hands us a range inside one 256 B page, which is
 * the W25Q page-program contract. flash_fs_raw_program() would split pages
 * anyway; it does not erase, which is what we need. */
static int w25q_program(void *ctx, uint32_t addr, const void *data, uint32_t len)
{
    (void)ctx;
    return (flash_fs_raw_program(addr, data, len) == FLASH_FS_OK) ? 0 : -1;
}

static const flash_region_backend_t w25q_backend = {
    .read         = w25q_read,
    .erase_sector = w25q_erase_sector,
    .program      = w25q_program,
    .ctx          = NULL,
};

flash_region_status_t flash_regions_bind_w25q(void)
{
    return flash_regions_init(&w25q_backend);
}
