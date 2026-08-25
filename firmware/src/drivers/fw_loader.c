/*
 * fw_loader.c — see fw_loader.h for the contract and safety posture.
 *
 * GEOMETRY (v2 — W25Q staging)
 * ----------------------------
 *   app slot       0x08007000..0x080C0000   (740 KB ceiling: the 2C23T
 *                                            port keeps its FPGA bitstream
 *                                            store at 0x080C0000 — shared
 *                                            internal-flash map)
 *   cache slot A   W25Q 0x00D00000, 1 MB    (fwcache region)
 *   cache slot B   W25Q 0x00E00000, 1 MB
 *
 * Each slot is a manifest sector (magic 'FWC1', size, CRC-32 — written
 * LAST, so a torn transfer can never look valid) followed by image data.
 * The format is shared with the 2C23T port's fw_cache.c: either firmware
 * can fill a slot, and either can install from it. fwload streams into a
 * slot over CDC; fwswap installs from an already-filled slot with no host
 * transfer at all — which is what makes switching firmware a one-command
 * operation from both sides.
 *
 * The first revision staged to internal flash at 0x080A0000 and capped
 * images at 382 KB — below this firmware's own size, so it could never
 * round-trip itself. W25Q slots remove the cap (1 MB per slot) and
 * double as the persistent cache.
 *
 * The installer is RAM-resident and reads the W25Q over SPI2 directly
 * (polled; the code below IS the driver — the flash-resident one is
 * unreachable while the app slot is erased under it), programs the app
 * slot routing between the two internal-flash banks by address (this
 * image is 595 KB and crosses the 0x08080000 bank boundary — the first
 * revision drove bank-0 registers only, which silently drops bank-1
 * writes and would have failed verify on any image over ~468 KB), then
 * SYSTEM-RESETS into the new image. Never a jump: a cross-firmware jump
 * was bench-tried (2026-08-22) and half-bricks.
 */

#include "fw_loader.h"

#include <string.h>

#ifndef FW_LOADER_HOST_TEST
#include "flash_regions.h"
#endif

enum {
    FWL_APP_BASE    = 0x08007000u,
    FWL_APP_CEILING = 0x080C0000u,
    FWL_PAGE_SIZE   = 2048u,
    FWL_MIN_IMAGE   = 8192u,

    FWL_SLOT_SPAN   = 0x00100000u,
    FWL_SLOT_A      = 0x00D00000u,
    FWL_SLOT_B      = 0x00E00000u,
    FWL_SECTOR      = 4096u,
    FWL_DATA_OFF    = FWL_SECTOR,
    FWL_DATA_MAX    = FWL_APP_CEILING - FWL_APP_BASE,   /* 740 KB */

    /* See FW_LOADER_TIMEOUT_POLLS in the header — the shell task ages the
     * drain of an aborted image against the same number. */
    FWL_TIMEOUT_POLLS = FW_LOADER_TIMEOUT_POLLS,
};

#define FWL_MANIFEST_MAGIC 0x31435746u /* 'FWC1' little-endian */

typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t crc;
    uint32_t reserved;
} fwl_manifest_t;

static fw_loader_state_t fwl_state;
static fw_loader_error_t fwl_error;
static uint8_t  fwl_slot;         /* 0 = A, 1 = B */
static uint32_t fwl_expected;
static uint32_t fwl_crc_announced;
static uint32_t fwl_received;
static uint32_t fwl_running_crc;  /* pre-xorout */
static uint32_t fwl_erase_mark;
static uint32_t fwl_silence_polls;

/* RX chunk accumulator; also the CRC/verify read buffer and the
 * installer's transfer buffer (never active at the same time). 512 B on
 * purpose — this firmware runs within a few hundred bytes of the RAM
 * ceiling, so the installer works the 2 KB flash pages in four 512-byte
 * passes instead of holding a whole page. */
static uint8_t  fwl_buf[512];
static uint32_t fwl_buf_fill;

static uint32_t fwl_slot_base(uint8_t slot)
{
    return slot ? FWL_SLOT_B : FWL_SLOT_A;
}

/* ── CRC-32 (zlib), explicit chaining ───────────────────────────────── */
static uint32_t fwl_crc32_update(uint32_t crc, const uint8_t *p, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8u; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

/* ── W25Q access, swappable for the host test ───────────────────────── */
#ifdef FW_LOADER_HOST_TEST

/* The test provides a simulated fwcache region (2 MB at 0xD00000). */
extern uint8_t fw_loader_test_w25q[]; /* indexed by (addr - 0xD00000) */
#define FWL_W25Q(addr) (&fw_loader_test_w25q[(addr) - 0x00D00000u])

static int fwl_w25q_erase(uint32_t addr, uint32_t len)
{
    memset(FWL_W25Q(addr), 0xFF, len);
    return 0;
}

static int fwl_w25q_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    memcpy(FWL_W25Q(addr), data, len);
    return 0;
}

static int fwl_w25q_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    memcpy(dst, FWL_W25Q(addr), len);
    return 0;
}

#else /* target: everything goes through the audited flash_regions layer */

/* The regions layer ships host-tested but nothing in the firmware binds
 * it to the W25Q at boot yet — bind lazily on first use (idempotent). */
static bool fwl_regions_ready(void)
{
    if (!flash_regions_ready()) {
        (void)flash_regions_bind_w25q();
    }
    return flash_regions_ready();
}

static int fwl_w25q_erase(uint32_t addr, uint32_t len)
{
    if (!fwl_regions_ready()) {
        return -1;
    }
    return flash_regions_erase_abs(addr, len) == FLASH_REGION_OK ? 0 : -1;
}

static int fwl_w25q_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!fwl_regions_ready()) {
        return -1;
    }
    return flash_regions_write_abs(addr, data, len) == FLASH_REGION_OK ? 0 : -1;
}

static int fwl_w25q_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    const flash_region_t *r;
    if (!fwl_regions_ready()) {
        return -1;
    }
    r = flash_region_find(addr);
    if (r == NULL) {
        return -1;
    }
    /* flash_region_read is id+offset based; translate. */
    for (uint32_t i = 0; i < FLASH_REGION_COUNT; ++i) {
        if (&flash_region_table[i] == r) {
            return flash_region_read((flash_region_id_t)i, addr - r->start,
                                     dst, len) == FLASH_REGION_OK
                       ? 0
                       : -1;
        }
    }
    return -1;
}

#endif /* FW_LOADER_HOST_TEST */

/* ── State machine ──────────────────────────────────────────────────── */

static void fwl_fail(fw_loader_error_t err)
{
    fwl_state = FW_LOADER_ERROR;
    fwl_error = err;
}

bool fw_loader_begin(uint32_t size, uint32_t crc32, uint8_t slot)
{
    fwl_state = FW_LOADER_IDLE;
    fwl_error = FW_LOADER_ERR_NONE;
    fwl_received = 0;
    fwl_buf_fill = 0;
    fwl_silence_polls = 0;

    if (slot > 1u || size < FWL_MIN_IMAGE || size > FWL_DATA_MAX ||
        (size & 1u)) {
        fwl_fail(FW_LOADER_ERR_SIZE);
        return false;
    }
    fwl_slot = slot;
    fwl_expected = size;
    fwl_crc_announced = crc32;
    fwl_running_crc = 0xFFFFFFFFu;
    fwl_erase_mark = fwl_slot_base(slot); /* manifest sector erased too:
                                             a began transfer invalidates
                                             the slot until it re-STAGES */
    fwl_state = FW_LOADER_RECEIVING;
    return true;
}

void fw_loader_abort(void)
{
    if (fwl_state == FW_LOADER_RECEIVING) {
        fwl_fail(FW_LOADER_ERR_TIMEOUT);
    }
}

bool fw_loader_active(void)
{
    return fwl_state == FW_LOADER_RECEIVING;
}

static bool fwl_commit_chunk(void)
{
    uint32_t addr = fwl_slot_base(fwl_slot) + FWL_DATA_OFF +
                    (fwl_received - fwl_buf_fill);
    while (fwl_erase_mark < addr + fwl_buf_fill) {
        if (fwl_w25q_erase(fwl_erase_mark, FWL_SECTOR) != 0) {
            return false;
        }
        fwl_erase_mark += FWL_SECTOR;
    }
    /* flash_regions verifies its own writes (ERR_VERIFY on mismatch). */
    return fwl_w25q_write(addr, fwl_buf, fwl_buf_fill) == 0;
}

/* Final gates, all against the slot AT REST: re-read CRC, then the vector
 * table's shape. Only then the manifest is written — the moment the slot
 * becomes installable by fwapply, fwswap, or the 2C23T port's SWAPx. */
static void fwl_finish(void)
{
    uint32_t data = fwl_slot_base(fwl_slot) + FWL_DATA_OFF;
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t off = 0; off < fwl_expected; off += sizeof(fwl_buf)) {
        uint32_t n = fwl_expected - off;
        if (n > sizeof(fwl_buf)) {
            n = sizeof(fwl_buf);
        }
        if (fwl_w25q_read(data + off, fwl_buf, n) != 0) {
            fwl_fail(FW_LOADER_ERR_FLASH);
            return;
        }
        crc = fwl_crc32_update(crc, fwl_buf, n);
    }
    if ((crc ^ 0xFFFFFFFFu) != fwl_crc_announced) {
        fwl_fail(FW_LOADER_ERR_CRC);
        return;
    }

    if (fwl_w25q_read(data, fwl_buf, 8) != 0) {
        fwl_fail(FW_LOADER_ERR_FLASH);
        return;
    }
    {
        uint32_t sp = (uint32_t)fwl_buf[0] | ((uint32_t)fwl_buf[1] << 8) |
                      ((uint32_t)fwl_buf[2] << 16) | ((uint32_t)fwl_buf[3] << 24);
        uint32_t pc = (uint32_t)fwl_buf[4] | ((uint32_t)fwl_buf[5] << 8) |
                      ((uint32_t)fwl_buf[6] << 16) | ((uint32_t)fwl_buf[7] << 24);
        if ((sp & 0xFFF00000u) != 0x20000000u ||
            pc < FWL_APP_BASE || pc >= FWL_APP_CEILING || (pc & 1u) == 0) {
            fwl_fail(FW_LOADER_ERR_VECTOR);
            return;
        }
    }

    {
        fwl_manifest_t *m = (fwl_manifest_t *)fwl_buf;
        memset(fwl_buf, 0xFF, FWL_SECTOR > sizeof(fwl_buf) ? sizeof(fwl_buf)
                                                           : FWL_SECTOR);
        m->magic = FWL_MANIFEST_MAGIC;
        m->size = fwl_expected;
        m->crc = fwl_crc_announced;
        m->reserved = 0;
        if (fwl_w25q_erase(fwl_slot_base(fwl_slot), FWL_SECTOR) != 0 ||
            fwl_w25q_write(fwl_slot_base(fwl_slot), fwl_buf,
                           sizeof(fwl_manifest_t)) != 0) {
            fwl_fail(FW_LOADER_ERR_FLASH);
            return;
        }
    }
    fwl_state = FW_LOADER_STAGED;
}

void fw_loader_feed(const uint8_t *data, uint16_t len)
{
    if (fwl_state != FW_LOADER_RECEIVING) {
        return;
    }
    fwl_silence_polls = 0;
    while (len > 0) {
        uint32_t room = sizeof(fwl_buf) - fwl_buf_fill;
        uint32_t take = len < room ? len : room;
        uint32_t remaining = fwl_expected - fwl_received;
        if (take > remaining) {
            take = remaining;
        }
        memcpy(&fwl_buf[fwl_buf_fill], data, take);
        fwl_running_crc = fwl_crc32_update(fwl_running_crc, data, take);
        fwl_buf_fill += take;
        fwl_received += take;
        data += take;
        len = (uint16_t)(len - take);

        if (fwl_buf_fill == sizeof(fwl_buf) ||
            fwl_received == fwl_expected) {
            if (!fwl_commit_chunk()) {
                fwl_fail(FW_LOADER_ERR_FLASH);
                return;
            }
            fwl_buf_fill = 0;
        }
        if (fwl_received == fwl_expected) {
            /* Cheap early reject before the full at-rest re-read: the
             * running CRC already disagrees => don't bother. */
            if ((fwl_running_crc ^ 0xFFFFFFFFu) != fwl_crc_announced) {
                fwl_fail(FW_LOADER_ERR_CRC);
                return;
            }
            fwl_finish();
            return;
        }
    }
}

void fw_loader_poll(void)
{
    if (fwl_state != FW_LOADER_RECEIVING) {
        return;
    }
    if (++fwl_silence_polls >= FWL_TIMEOUT_POLLS) {
        fwl_fail(FW_LOADER_ERR_TIMEOUT);
    }
}

fw_loader_state_t fw_loader_state(void) { return fwl_state; }
fw_loader_error_t fw_loader_error(void) { return fwl_error; }
uint8_t  fw_loader_slot(void) { return fwl_slot; }
uint32_t fw_loader_bytes(void) { return fwl_received; }
uint32_t fw_loader_expected(void) { return fwl_expected; }
uint32_t fw_loader_crc_announced(void) { return fwl_crc_announced; }

static bool fwl_read_manifest(uint8_t slot, fwl_manifest_t *m)
{
    if (fwl_w25q_read(fwl_slot_base(slot), (uint8_t *)m, sizeof(*m)) != 0) {
        return false;
    }
    return m->magic == FWL_MANIFEST_MAGIC && m->size >= FWL_MIN_IMAGE &&
           m->size <= FWL_DATA_MAX && !(m->size & 1u);
}

uint32_t fw_loader_slot_size(uint8_t slot)
{
    fwl_manifest_t m;
    return (slot <= 1u && fwl_read_manifest(slot, &m)) ? m.size : 0u;
}

uint32_t fw_loader_slot_crc(uint8_t slot)
{
    fwl_manifest_t m;
    return (slot <= 1u && fwl_read_manifest(slot, &m)) ? m.crc : 0u;
}

/* ── The installer ──────────────────────────────────────────────────── */
#ifndef FW_LOADER_HOST_TEST

#define RF __attribute__((section(".data.ramfunc"), noinline, used))

/* SPI2 + GPIOB, raw (CS = PB12, same wiring the flash_fs driver uses). */
#define R_SPI2_STS  (*(volatile uint32_t *)0x40003808u)
#define R_SPI2_DT   (*(volatile uint32_t *)0x4000380Cu)
#define R_GPIOB_SCR (*(volatile uint32_t *)0x40010C10u)
#define R_GPIOB_CLR (*(volatile uint32_t *)0x40010C14u)

RF static uint8_t rf_spi2_xfer(uint8_t v)
{
    uint32_t t = 0x000FFFFFu;
    while (!(R_SPI2_STS & 0x2u) && --t) {
    }
    R_SPI2_DT = v;
    t = 0x000FFFFFu;
    while (!(R_SPI2_STS & 0x1u) && --t) {
    }
    return (uint8_t)R_SPI2_DT;
}

RF static void rf_w25q_read_raw(uint32_t addr, uint8_t *dst, uint32_t len)
{
    R_GPIOB_CLR = 1u << 12;
    (void)rf_spi2_xfer(0x03u);
    (void)rf_spi2_xfer((uint8_t)(addr >> 16));
    (void)rf_spi2_xfer((uint8_t)(addr >> 8));
    (void)rf_spi2_xfer((uint8_t)addr);
    for (uint32_t i = 0; i < len; ++i) {
        dst[i] = rf_spi2_xfer(0xFFu);
    }
    R_GPIOB_SCR = 1u << 12;
}

/* Erase + program + verify the app slot from a W25Q source, routing
 * between the two internal-flash banks by address, then SYSRESETREQ.
 * Bank 0 regs at +0x0C/+0x10/+0x14 (KEYR +0x04), bank 1 at +0x4C/+0x50/
 * +0x54 (KEYR2 +0x44). */
RF static void fwl_ram_install(uint32_t src, uint32_t size)
{
    uint8_t *page = fwl_buf;
    uint32_t end = FWL_APP_BASE +
                   ((size + (FWL_PAGE_SIZE - 1u)) & ~(FWL_PAGE_SIZE - 1u));

    __asm__ volatile("cpsid i" ::: "memory");

    /* Power hold (PC9): keep the rail up through whatever follows. */
    *(volatile uint32_t *)0x40021018u |= (1u << 4);
    *(volatile uint32_t *)0x40011010u = 1u << 9;

    /* Reclaim SPI2 from whatever the RTOS had in flight. */
    {
        uint32_t t = 0x000FFFFFu;
        while ((R_SPI2_STS & 0x80u) && --t) {
        }
        R_GPIOB_SCR = 1u << 12;
    }

    if (size == 0 || (size & 1u) || end > FWL_APP_CEILING) {
        goto dead;
    }

    for (uint32_t addr = FWL_APP_BASE; addr < end; addr += FWL_PAGE_SIZE) {
        /* Feed the FWDGT (cmd register, reload key). This firmware runs the
         * watchdog with a hard 3.0 s timeout (watchdog.c), normally fed by
         * the 500 ms health task — which died with `cpsid i` above. The
         * install is 10-20 s, so without a feed the dog fires mid-erase and,
         * the vector page going first, resets into an unbootable slot.
         * One feed per 2 KB page keeps us far under 3 s. RAM-safe: a single
         * volatile MMIO store, no flash, no libcalls; feeding a never-armed
         * FWDGT is a no-op. Found by DavidClawson in review. The bench
         * round-trips on unit #2 never tripped it for two reasons, neither
         * of them margin: GUEST_BUILD skips watchdog_init() (main.c), and
         * the 2C23T port arms no watchdog at all. Release builds arm it. */
        *(volatile uint32_t *)0x40003000u = 0x0000AAAAu;

        uint8_t bank1 = addr >= 0x08080000u;
        volatile uint32_t *sts =
            (volatile uint32_t *)(bank1 ? 0x4002204Cu : 0x4002200Cu);
        volatile uint32_t *ctrl =
            (volatile uint32_t *)(bank1 ? 0x40022050u : 0x40022010u);
        volatile uint32_t *fadr =
            (volatile uint32_t *)(bank1 ? 0x40022054u : 0x40022014u);
        volatile uint32_t *keyr =
            (volatile uint32_t *)(bank1 ? 0x40022044u : 0x40022004u);
        uint32_t off = addr - FWL_APP_BASE;
        uint32_t t;

        if (*ctrl & (1u << 7)) {
            *keyr = 0x45670123u;
            *keyr = 0xCDEF89ABu;
        }
        if (*ctrl & (1u << 7)) {
            goto dead;
        }

        /* Erase the whole 2 KB page... */
        t = 0x00FFFFFFu;
        while ((*sts & 1u) && --t) {
        }
        if (!t) {
            goto dead;
        }
        *sts = (1u << 5) | (1u << 2) | (1u << 4);
        *ctrl |= 1u << 1;
        *fadr = addr;
        *ctrl |= 1u << 6;
        t = 0x00FFFFFFu;
        while ((*sts & 1u) && --t) {
        }
        *ctrl &= ~(1u << 1);
        if (!t || (*sts & ((1u << 2) | (1u << 4)))) {
            goto dead;
        }

        /* ...then program and verify it in 512-byte passes (RAM budget). */
        for (uint32_t sub = 0; sub < FWL_PAGE_SIZE; sub += sizeof(fwl_buf)) {
            uint32_t poff = off + sub;
            uint32_t n = poff < size ? size - poff : 0;
            if (n > sizeof(fwl_buf)) {
                n = sizeof(fwl_buf);
            }
            if (n == 0) {
                break; /* rest of the page stays erased (0xFF) */
            }
            rf_w25q_read_raw(src + poff, page, sizeof(fwl_buf));
            for (uint32_t i = n; i < sizeof(fwl_buf); ++i) {
                page[i] = 0xFFu;
            }

            for (uint32_t i = 0; i < sizeof(fwl_buf); i += 2u) {
                uint16_t v = (uint16_t)page[i] | ((uint16_t)page[i + 1u] << 8);
                t = 0x00FFFFFFu;
                while ((*sts & 1u) && --t) {
                }
                if (!t) {
                    goto dead;
                }
                *sts = (1u << 5) | (1u << 2) | (1u << 4);
                *ctrl |= 1u;
                *(volatile uint16_t *)(addr + sub + i) = v;
                t = 0x00FFFFFFu;
                while ((*sts & 1u) && --t) {
                }
                *ctrl &= ~1u;
                if (!t || (*sts & ((1u << 2) | (1u << 4)))) {
                    goto dead;
                }
            }

            for (uint32_t i = 0; i < sizeof(fwl_buf); ++i) {
                if (*(volatile uint8_t *)(addr + sub + i) != page[i]) {
                    goto dead;
                }
            }
        }
    }

    *(volatile uint32_t *)0xE000ED0Cu = 0x05FA0004u; /* SYSRESETREQ */

dead:
    /* Unreachable on success. Rail held; recovery = MENU+Power IAP, which
     * this code cannot touch — it writes only 0x08007000 upward. */
    while (1) {
    }
}

#endif /* !FW_LOADER_HOST_TEST */

/* Install from a slot: full at-rest verification first (manifest, CRC of
 * every byte, vector shape), then the RAM installer. Shared by fwapply
 * (install what was just staged) and fwswap (install a cached image with
 * no transfer). Returns only on refusal. */
bool fw_loader_install_slot(uint8_t slot)
{
    fwl_manifest_t m;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t data;

    if (slot > 1u || !fwl_read_manifest(slot, &m)) {
        fwl_fail(FW_LOADER_ERR_NO_IMAGE);
        return false;
    }
    data = fwl_slot_base(slot) + FWL_DATA_OFF;

    for (uint32_t off = 0; off < m.size; off += sizeof(fwl_buf)) {
        uint32_t n = m.size - off;
        if (n > sizeof(fwl_buf)) {
            n = sizeof(fwl_buf);
        }
        if (fwl_w25q_read(data + off, fwl_buf, n) != 0) {
            fwl_fail(FW_LOADER_ERR_FLASH);
            return false;
        }
        crc = fwl_crc32_update(crc, fwl_buf, n);
    }
    if ((crc ^ 0xFFFFFFFFu) != m.crc) {
        fwl_fail(FW_LOADER_ERR_CRC);
        return false;
    }
    if (fwl_w25q_read(data, fwl_buf, 8) != 0) {
        fwl_fail(FW_LOADER_ERR_FLASH);
        return false;
    }
    {
        uint32_t sp = (uint32_t)fwl_buf[0] | ((uint32_t)fwl_buf[1] << 8) |
                      ((uint32_t)fwl_buf[2] << 16) | ((uint32_t)fwl_buf[3] << 24);
        uint32_t pc = (uint32_t)fwl_buf[4] | ((uint32_t)fwl_buf[5] << 8) |
                      ((uint32_t)fwl_buf[6] << 16) | ((uint32_t)fwl_buf[7] << 24);
        if ((sp & 0xFFF00000u) != 0x20000000u ||
            pc < FWL_APP_BASE || pc >= FWL_APP_CEILING || (pc & 1u) == 0) {
            fwl_fail(FW_LOADER_ERR_VECTOR);
            return false;
        }
    }
#ifndef FW_LOADER_HOST_TEST
    fwl_ram_install(data, m.size);
    /* not reached */
#endif
    return true;
}

bool fw_loader_apply(void)
{
    if (fwl_state != FW_LOADER_STAGED) {
        return false;
    }
    return fw_loader_install_slot(fwl_slot);
}
