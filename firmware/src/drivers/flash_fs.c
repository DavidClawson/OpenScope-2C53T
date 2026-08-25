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
#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Internal state
 * ═══════════════════════════════════════════════════════════════════ */

static SemaphoreHandle_t fs_mutex  = NULL;
static bool              fs_ready  = false;
static bool              raw_spi_ready = false;

#define SPI_FLASH_SPI       ((spi_type *)SPI2_BASE)
#define SPI_FLASH_SIZE      FLASH_FS_RAW_MAX_ADDR
#define SPI_FLASH_CS_ASSERT()   (GPIOB->clr = GPIO_PINS_12)
/* Deassert then hold CS high for >= tSHSL. A W25Q needs CS to rise for a
 * minimum time between commands to register deselection; back-to-back
 * transactions (WREN then RDSR/erase/program) with only a few instructions
 * of CS-high time violate this, and the flash mis-frames the second command
 * and leaves MISO undriven (reads 0xFF). Reads survived because a read is one
 * self-contained transaction; every WRITE path is WREN-then-command and was
 * failing at the RDSR inside raw_write_enable. ~1 us here, negligible vs a
 * page program or sector erase. */
#define SPI_FLASH_CS_DEASSERT()                                              \
    do {                                                                     \
        GPIOB->scr = GPIO_PINS_12;                                           \
        for (volatile uint32_t _cs_hi = 0; _cs_hi < 120u; _cs_hi++) { }      \
    } while (0)

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

static void flash_fs_raw_spi_init_once(void)
{
    if (raw_spi_ready) {
        return;
    }

    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);

    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);

    /* PB13 = SPI2_SCK, PB15 = SPI2_MOSI */
    gpio_cfg.gpio_pins = GPIO_PINS_13 | GPIO_PINS_15;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB14 = SPI2_MISO */
    gpio_cfg.gpio_pins = GPIO_PINS_14;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB12 = SPI flash CS */
    gpio_cfg.gpio_pins = GPIO_PINS_12;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);
    SPI_FLASH_CS_DEASSERT();

    /* SPI mode 0, master, software CS, conservative /16 clock. */
    SPI_FLASH_SPI->ctrl1 = (1U << 2)   /* master */
                         | (3U << 3)   /* /16 prescaler */
                         | (1U << 8)   /* internal CS high */
                         | (1U << 9);  /* software CS enable */
    SPI_FLASH_SPI->ctrl2 = 0;
    SPI_FLASH_SPI->ctrl1 |= (1U << 6); /* enable SPI */

    raw_spi_ready = true;
}

static uint8_t flash_fs_raw_spi_xfer(uint8_t tx)
{
    uint32_t timeout = 1000000;
    while (!(SPI_FLASH_SPI->sts & (1U << 1)) && --timeout) {}
    if (!timeout) {
        return 0xFF;
    }

    SPI_FLASH_SPI->dt = tx;

    timeout = 1000000;
    while (!(SPI_FLASH_SPI->sts & (1U << 0)) && --timeout) {}
    if (!timeout) {
        return 0xFF;
    }

    return (uint8_t)SPI_FLASH_SPI->dt;
}

/* Wait for the SPI peripheral to finish shifting (BSY=sts bit7 clear) before
 * raising CS. Reads tolerate an early CS edge (data already latched), but a
 * page-program/erase whose final byte is truncated by an early CS rise is
 * REJECTED by the W25Q (frame not a whole number of bytes) — that is why reads
 * worked but writes silently did nothing. Bounded so it can never hang. */
static void raw_spi_settle(void)
{
    uint32_t t = 100000u;
    while ((SPI_FLASH_SPI->sts & (1U << 7)) && --t) { }
}

/* Raw 0x03 read into a buffer, no mutex (caller holds it). Mirrors the stock
 * read primitive FUN_0802f048: CS↓, opcode 0x03 + 24-bit addr, stream bytes, CS↑. */
static void flash_fs_raw_read_nolock(uint32_t addr, uint8_t *buf, uint32_t len)
{
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(0x03);
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 16));
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 8));
    flash_fs_raw_spi_xfer((uint8_t)(addr));
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = flash_fs_raw_spi_xfer(0xFF);
    }
    SPI_FLASH_CS_DEASSERT();
}

/* ── Raw SPI-flash WRITE primitives — byte-faithful to the stock W25Q driver ──
 * Stock refs (V1.2.0): WREN FUN_0802f344, RDSR-wait FUN_0802f11c, 4KB sector
 * erase FUN_0802ee9c, page-program FUN_0802f36c, 256B page loop FUN_0802f2ac,
 * smart read-modify-write block FUN_0802f16c. Same SPI2 bus + PB12 CS as the
 * read path; CS↓ = GPIOB BRR (0x40010c14), CS↑ = GPIOB BSRR (0x40010c10). */
#define W25Q_CMD_WREN          0x06u
#define W25Q_CMD_RDSR          0x05u
#define W25Q_CMD_PAGE_PROGRAM  0x02u
#define W25Q_CMD_SECTOR_ERASE  0x20u
#define W25Q_PAGE_SIZE         256u
#define W25Q_SECTOR_SIZE       4096u
#define W25Q_STATUS_BUSY       0x01u
#define W25Q_STATUS_WEL        0x02u

/* The 4KB read-modify-write scratch (stock kept it permanently at RAM 0x200012c0).
 * We have only ~5KB of MSP-stack headroom over BSS, so a permanent 4KB static buffer
 * starves the boot stack and crashes the app — instead the public write_block wrapper
 * heap-allocates this on demand (it is never called at boot or in a hot loop). */

/* ── Write-path honesty (audit 2026-08-20, P0.4) ─────────────────────────
 * Until 2026-08-20 every function below was void and the public wrappers
 * returned FLASH_FS_OK unconditionally — a failed erase/program was reported
 * as success, and settings_store would then mark the save persisted and never
 * retry. The W25Q128JV has NO program/erase-fail bits in SR1 (that is other
 * vendors), so honesty comes from three checks instead:
 *   1. WEL actually latched after WREN (catches write-protect; a dead bus
 *      reading 0xFF sneaks past this one — see 3);
 *   2. the BUSY poll reached idle before its bounded guard expired;
 *   3. read-back verification in the public wrappers — the ground truth that
 *      catches everything, dead bus included. */

static bool raw_write_enable(void)   /* FUN_0802f344 + WEL confirm */
{
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(W25Q_CMD_WREN);
    raw_spi_settle();
    SPI_FLASH_CS_DEASSERT();

    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(W25Q_CMD_RDSR);
    uint8_t sr1 = flash_fs_raw_spi_xfer(0xFF);
    SPI_FLASH_CS_DEASSERT();
    /* 0xFF is either a dead bus (xfer timeout sentinel) or an SR1 with every
     * bit set including BUSY — refused either way. */
    return (sr1 != 0xFFu) && (sr1 & W25Q_STATUS_WEL) != 0u;
}

static bool raw_wait_busy(void)      /* FUN_0802f11c: poll RDSR until BUSY clears */
{
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(W25Q_CMD_RDSR);
    /* Bounded poll: flash_fs_raw_spi_xfer returns 0xFF on its OWN timeout, and
     * 0xFF & BUSY == BUSY, so an unbounded loop would spin forever on any SPI
     * fault and wedge the calling task. Guard covers a 4KB erase (~hundreds of
     * ms) with huge margin, then bails so the system can never hang here. */
    uint32_t guard = 20000000u;
    while ((flash_fs_raw_spi_xfer(0xFF) & W25Q_STATUS_BUSY) && --guard) { }
    SPI_FLASH_CS_DEASSERT();
    return guard != 0u;   /* false = still busy at guard expiry: erase/program
                             did not complete (or the bus is dead) */
}

static bool raw_sector_erase_nolock(uint32_t addr)   /* FUN_0802ee9c */
{
    if (!raw_write_enable()) {
        return false;
    }
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(W25Q_CMD_SECTOR_ERASE);
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 16));
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 8));
    flash_fs_raw_spi_xfer((uint8_t)(addr));
    raw_spi_settle();
    SPI_FLASH_CS_DEASSERT();
    return raw_wait_busy();
}

/* Program within a single 256B page (len ≤ 256, no page crossing). FUN_0802f36c. */
static bool raw_page_program_nolock(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!raw_write_enable()) {
        return false;
    }
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(W25Q_CMD_PAGE_PROGRAM);
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 16));
    flash_fs_raw_spi_xfer((uint8_t)(addr >> 8));
    flash_fs_raw_spi_xfer((uint8_t)(addr));
    for (uint32_t i = 0; i < len; i++) {
        flash_fs_raw_spi_xfer(data[i]);
    }
    raw_spi_settle();
    SPI_FLASH_CS_DEASSERT();
    return raw_wait_busy();
}

/* Read back [addr, addr+len) and compare. data == NULL means "expect erased"
 * (all 0xFF). Small stack chunks — this runs on tasks with tight stacks. */
static bool raw_verify_nolock(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t buf[32];
    uint32_t off = 0;
    while (off < len) {
        uint32_t n = len - off;
        if (n > sizeof(buf)) n = sizeof(buf);
        flash_fs_raw_read_nolock(addr + off, buf, n);
        for (uint32_t i = 0; i < n; i++) {
            uint8_t want = data ? data[off + i] : 0xFFu;
            if (buf[i] != want) {
                return false;
            }
        }
        off += n;
    }
    return true;
}

/* Split a write across 256B page boundaries. FUN_0802f2ac. Range must be erased. */
static bool raw_program_nolock(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t chunk = W25Q_PAGE_SIZE - (addr & 0xFF);
    if (len < chunk) chunk = len;
    while (len) {
        if (!raw_page_program_nolock(addr, data, chunk)) {
            return false;
        }
        addr += chunk;
        data += chunk;
        len  -= chunk;
        chunk = (len > W25Q_PAGE_SIZE) ? W25Q_PAGE_SIZE : len;
    }
    return true;
}

/* Smart write at 4KB-sector granularity (FUN_0802f16c): read the sector, and if
 * any target byte is not already 0xFF, erase + overlay + rewrite the full sector
 * (preserving the rest); otherwise program in place. Safe for arbitrary addr/len. */
static bool raw_write_block_nolock(uint32_t addr, const uint8_t *data, uint32_t len,
                                   uint8_t *sector_buf)
{
    uint32_t sector = addr >> 12;
    uint32_t off    = addr & 0xFFF;
    uint32_t chunk  = W25Q_SECTOR_SIZE - off;
    if (len < chunk) chunk = len;
    while (1) {
        flash_fs_raw_read_nolock(sector << 12, sector_buf, W25Q_SECTOR_SIZE);
        uint32_t i;
        for (i = 0; i < chunk && sector_buf[off + i] == 0xFF; i++) { }
        if (i < chunk) {                       /* needs erase */
            if (!raw_sector_erase_nolock(sector << 12)) {
                return false;
            }
            for (i = 0; i < chunk; i++) {
                sector_buf[off + i] = data[i];
            }
            /* Verify the WHOLE rewritten sector, not just the caller's bytes:
             * the RMW preserved region can fail to restore too, and a caller
             * has no way to notice that. */
            if (!raw_program_nolock(sector << 12, sector_buf, W25Q_SECTOR_SIZE) ||
                !raw_verify_nolock(sector << 12, sector_buf, W25Q_SECTOR_SIZE)) {
                return false;
            }
        } else {                                /* already erased: program in place */
            if (!raw_program_nolock((sector << 12) | off, data, chunk) ||
                !raw_verify_nolock((sector << 12) | off, data, chunk)) {
                return false;
            }
        }
        if (chunk == len) break;
        sector += 1;
        off = 0;
        data += chunk;
        len  -= chunk;
        chunk = (len > W25Q_SECTOR_SIZE) ? W25Q_SECTOR_SIZE : len;
    }
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

flash_fs_error_t flash_fs_raw_read_jedec(uint8_t *manufacturer,
                                         uint8_t *memory_type,
                                         uint8_t *capacity)
{
    if (manufacturer == NULL || memory_type == NULL || capacity == NULL) {
        return FLASH_FS_ERR_READ;
    }
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }

    flash_fs_raw_spi_init_once();

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(0x9F);
    *manufacturer = flash_fs_raw_spi_xfer(0xFF);
    *memory_type  = flash_fs_raw_spi_xfer(0xFF);
    *capacity     = flash_fs_raw_spi_xfer(0xFF);
    SPI_FLASH_CS_DEASSERT();

    xSemaphoreGive(fs_mutex);
    return FLASH_FS_OK;
}

flash_fs_error_t flash_fs_raw_read_bytes(uint32_t addr, void *buf, uint32_t len)
{
    uint8_t *out = (uint8_t *)buf;

    if (buf == NULL) {
        return FLASH_FS_ERR_READ;
    }
    if (len == 0) {
        return FLASH_FS_OK;
    }
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }
    if (addr >= SPI_FLASH_SIZE || len > (SPI_FLASH_SIZE - addr)) {
        return FLASH_FS_ERR_READ;
    }

    flash_fs_raw_spi_init_once();

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    flash_fs_raw_read_nolock(addr, out, len);

    xSemaphoreGive(fs_mutex);
    return FLASH_FS_OK;
}

/* ── Public WRITE wrappers (mutex + init + bounds; call the nolock cores) ── */

flash_fs_error_t flash_fs_raw_sector_erase(uint32_t addr)
{
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }
    if (addr >= SPI_FLASH_SIZE) {
        return FLASH_FS_ERR_WRITE;
    }

    flash_fs_raw_spi_init_once();

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    uint32_t base = addr & ~(W25Q_SECTOR_SIZE - 1u);
    flash_fs_error_t err = FLASH_FS_OK;
    if (!raw_sector_erase_nolock(base)) {
        err = FLASH_FS_ERR_WRITE;
    } else if (!raw_verify_nolock(base, NULL, W25Q_SECTOR_SIZE)) {
        err = FLASH_FS_ERR_VERIFY;   /* "erased" but not reading 0xFF */
    }

    xSemaphoreGive(fs_mutex);
    return err;
}

flash_fs_error_t flash_fs_raw_program(uint32_t addr, const void *data, uint32_t len)
{
    if (data == NULL) {
        return FLASH_FS_ERR_WRITE;
    }
    if (len == 0) {
        return FLASH_FS_OK;
    }
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }
    if (addr >= SPI_FLASH_SIZE || len > (SPI_FLASH_SIZE - addr)) {
        return FLASH_FS_ERR_WRITE;
    }

    flash_fs_raw_spi_init_once();

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }

    flash_fs_error_t err = FLASH_FS_OK;
    if (!raw_program_nolock(addr, (const uint8_t *)data, len)) {
        err = FLASH_FS_ERR_WRITE;
    } else if (!raw_verify_nolock(addr, (const uint8_t *)data, len)) {
        err = FLASH_FS_ERR_VERIFY;
    }

    xSemaphoreGive(fs_mutex);
    return err;
}

flash_fs_error_t flash_fs_raw_status_diag(uint8_t out[4])
{
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }
    flash_fs_raw_spi_init_once();
    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return FLASH_FS_ERR_MUTEX;
    }
    /* SR1 (0x05), SR2 (0x35), SR3 (0x15). */
    static const uint8_t ops[3] = { 0x05, 0x35, 0x15 };
    for (int i = 0; i < 3; i++) {
        SPI_FLASH_CS_ASSERT();
        flash_fs_raw_spi_xfer(ops[i]);
        out[i] = flash_fs_raw_spi_xfer(0xFF);
        SPI_FLASH_CS_DEASSERT();
    }
    /* WREN, then re-read SR1 — bit1 (WEL) must become 1 if write-enable latches. */
    (void)raw_write_enable();
    SPI_FLASH_CS_ASSERT();
    flash_fs_raw_spi_xfer(0x05);
    out[3] = flash_fs_raw_spi_xfer(0xFF);
    SPI_FLASH_CS_DEASSERT();

    xSemaphoreGive(fs_mutex);
    return FLASH_FS_OK;
}

flash_fs_error_t flash_fs_raw_write_block(uint32_t addr, const void *data, uint32_t len)
{
    if (data == NULL) {
        return FLASH_FS_ERR_WRITE;
    }
    if (len == 0) {
        return FLASH_FS_OK;
    }
    if (fs_mutex == NULL) {
        return FLASH_FS_ERR_MUTEX;
    }
    if (addr >= SPI_FLASH_SIZE || len > (SPI_FLASH_SIZE - addr)) {
        return FLASH_FS_ERR_WRITE;
    }

    flash_fs_raw_spi_init_once();

    /* 4KB RMW scratch from the FreeRTOS heap (no permanent BSS cost). */
    uint8_t *sector_buf = (uint8_t *)pvPortMalloc(W25Q_SECTOR_SIZE);
    if (sector_buf == NULL) {
        return FLASH_FS_ERR_WRITE;
    }

    if (xSemaphoreTake(fs_mutex, pdMS_TO_TICKS(FS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        vPortFree(sector_buf);
        return FLASH_FS_ERR_MUTEX;
    }

    bool ok = raw_write_block_nolock(addr, (const uint8_t *)data, len, sector_buf);

    xSemaphoreGive(fs_mutex);
    vPortFree(sector_buf);
    /* Verification runs inside the nolock core (whole rewritten sector on the
     * RMW path), so a false here is already "did not stick", write or verify. */
    return ok ? FLASH_FS_OK : FLASH_FS_ERR_WRITE;
}

/* ═══════════════════════════════════════════════════════════════════
 * Factory calibration mirror
 *
 * Stock-grounding boundary: the obvious W25Q lead on the bench unit,
 * "3:/System file/9999.BIN", is a zero-byte placeholder (cluster 0,
 * size 0), and the older "301-byte per-channel meter cal blob" label
 * was reclassified as oscilloscope roll-buffer state. No stock xref or
 * W25Q file currently proves a host-readable DMM factory-calibration
 * blob, and H2/SPI3 replay proves byte count only, not FPGA acceptance
 * or DMM correction.
 *
 * Keep this mirror as a fail-closed placeholder for a future recovered
 * source. Until that source is proven, do not probe invented filenames
 * and do not let meter/scope code consume arbitrary flash bytes as
 * calibration.
 * ═══════════════════════════════════════════════════════════════════ */

static factory_cal_t g_factory_cal;  /* BSS zero */

flash_fs_error_t flash_fs_load_factory_cal(void)
{
    memset(&g_factory_cal, 0, sizeof(g_factory_cal));
    g_factory_cal.loaded = false;
    return FLASH_FS_ERR_READ;
}

const factory_cal_t *flash_fs_factory_cal(void)
{
    return &g_factory_cal;
}
