/*
 * fw_loader.c — see fw_loader.h for the contract and safety posture.
 *
 * GEOMETRY
 * --------
 *   app slot     0x08007000            (bank 1; what fwapply rewrites)
 *   staging      0x080A0000..0x080FF800 (bank 2; what fwload writes)
 *
 * The staging window was chosen to be inert for BOTH firmwares this loader
 * is meant to swap between: this one keeps nothing above 0x080A0000, and
 * the 2C23T port keeps its bitstream store below (0x08080000..0x0809FFFF)
 * and its settings page above (0x080FF800). A staged image sits between
 * the two and clobbers neither.
 *
 * Staging lives in flash BANK 2 while code executes from bank 1, so the
 * incoming stream is programmed without stalling the CPU — USB stays
 * serviced during the transfer. The app slot is bank 1, which is why the
 * final copy runs from RAM with interrupts off (fw_ram_install below,
 * adapted from the 2C23T port's field-proven installer, GPL v3 both ways).
 *
 * This file deliberately prints nothing and knows nothing about USB: it is
 * a pure state machine fed bytes by the shell task, which keeps it host-
 * testable (tests/test_fw_loader.c builds it with FW_LOADER_HOST_TEST and
 * a simulated flash array).
 */

#include "fw_loader.h"

#include <string.h>

enum {
    FWL_APP_BASE    = 0x08007000u,
    FWL_STAGE_BASE  = 0x080A0000u,
    FWL_STAGE_END   = 0x080FF800u,
    FWL_STAGE_CAP   = FWL_STAGE_END - FWL_STAGE_BASE,
    FWL_PAGE_SIZE   = 2048u,
    FWL_MIN_IMAGE   = 8192u,
    /* Shell-task loop iterations of RX silence before a transfer is declared
     * dead. The idle loop sleeps 10 ms, so ~300 iterations ~= 3 s. */
    FWL_TIMEOUT_POLLS = 300u,
};

static fw_loader_state_t fwl_state;
static fw_loader_error_t fwl_error;
static uint32_t fwl_expected;
static uint32_t fwl_crc_announced;
static uint32_t fwl_received;
static uint32_t fwl_silence_polls;

/* RX bytes accumulate in a small chunk buffer and are programmed as they
 * arrive; pages are erased lazily by a watermark when the stream first
 * reaches them. 512 B (not a full page) on purpose: this firmware runs
 * within ~2 KB of the RAM ceiling and a page-sized buffer did not fit —
 * and none is needed, since erase and program are separate operations.
 * The installer needs no buffer at all: its source is bank-2 flash,
 * readable while bank 1 programs. */
enum { FWL_CHUNK_SIZE = 512u };
static uint8_t  fwl_chunk[FWL_CHUNK_SIZE];
static uint32_t fwl_chunk_fill;
static uint32_t fwl_erase_mark;   /* next un-erased staging address */

/* ── CRC-32 (reflected, poly 0xEDB88320, init/xorout 0xFFFFFFFF) ─────────
 * Matches zlib and the host script's zlib.crc32(). */
static uint32_t fwl_crc32(uint32_t crc_in, const volatile uint8_t *p, uint32_t len)
{
    uint32_t crc = crc_in ^ 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8u; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ── Flash access, swappable for the host test ──────────────────────────── */
#ifdef FW_LOADER_HOST_TEST

/* 1 MB simulated part, indexed by (addr - 0x08000000). The test provides it. */
extern uint8_t fw_loader_test_flash[];
#define FWL_MEM(addr) (&fw_loader_test_flash[(addr) - 0x08000000u])

static bool fwl_erase_page(uint32_t addr)
{
    memset(FWL_MEM(addr), 0xFF, FWL_PAGE_SIZE);
    return true;
}

static bool fwl_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    memcpy(FWL_MEM(addr), data, len);
    return true;
}

static const volatile uint8_t *fwl_read_ptr(uint32_t addr)
{
    return FWL_MEM(addr);
}

#else /* target */

/* Bank-2 flash controller registers (AT32F403A, 1 MB part: bank 2 is
 * 0x08080000+ and has its own KEYR/STS/CTRL/ADDR block at +0x44..+0x54).
 * Raw pointers rather than HAL: this must stay identical in shape to the
 * bank-1 sequence inside fw_ram_install below, where no HAL can be used. */
#define FWL_STS2   (*(volatile uint32_t *)0x4002204Cu)
#define FWL_CTRL2  (*(volatile uint32_t *)0x40022050u)
#define FWL_ADDR2  (*(volatile uint32_t *)0x40022054u)
#define FWL_KEYR2  (*(volatile uint32_t *)0x40022044u)

#define FWL_STS_BSY   (1u << 0)
#define FWL_STS_PRGMERR (1u << 2)
#define FWL_STS_WRPRTERR (1u << 4)
#define FWL_STS_ODF   (1u << 5)
#define FWL_CTRL_PG   (1u << 0)
#define FWL_CTRL_PER  (1u << 1)
#define FWL_CTRL_STRT (1u << 6)
#define FWL_CTRL_LOCK (1u << 7)

static bool fwl_wait_bank2(void)
{
    uint32_t timeout = 0x00FFFFFFu;
    while ((FWL_STS2 & FWL_STS_BSY) && --timeout) {
    }
    return timeout != 0;
}

static bool fwl_unlock_bank2(void)
{
    if (FWL_CTRL2 & FWL_CTRL_LOCK) {
        FWL_KEYR2 = 0x45670123u;
        FWL_KEYR2 = 0xCDEF89ABu;
    }
    return (FWL_CTRL2 & FWL_CTRL_LOCK) == 0;
}

static bool fwl_erase_page(uint32_t addr)
{
    if (!fwl_wait_bank2() || !fwl_unlock_bank2()) {
        return false;
    }
    FWL_STS2 = FWL_STS_ODF | FWL_STS_PRGMERR | FWL_STS_WRPRTERR;
    FWL_CTRL2 |= FWL_CTRL_PER;
    FWL_ADDR2 = addr;
    FWL_CTRL2 |= FWL_CTRL_STRT;
    bool ok = fwl_wait_bank2();
    FWL_CTRL2 &= ~FWL_CTRL_PER;
    if (!ok || (FWL_STS2 & (FWL_STS_PRGMERR | FWL_STS_WRPRTERR))) {
        return false;
    }
    return true;
}

static bool fwl_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!fwl_wait_bank2() || !fwl_unlock_bank2()) {
        return false;
    }
    for (uint32_t off = 0; off < len; off += 2u) {
        uint16_t v = (uint16_t)data[off] |
                     ((uint16_t)(off + 1u < len ? data[off + 1u] : 0xFFu) << 8);
        FWL_STS2 = FWL_STS_ODF | FWL_STS_PRGMERR | FWL_STS_WRPRTERR;
        FWL_CTRL2 |= FWL_CTRL_PG;
        *(volatile uint16_t *)(addr + off) = v;
        bool ok = fwl_wait_bank2();
        FWL_CTRL2 &= ~FWL_CTRL_PG;
        if (!ok || (FWL_STS2 & (FWL_STS_PRGMERR | FWL_STS_WRPRTERR))) {
            return false;
        }
    }
    return true;
}

static const volatile uint8_t *fwl_read_ptr(uint32_t addr)
{
    return (const volatile uint8_t *)addr;
}

#endif /* FW_LOADER_HOST_TEST */

/* ── State machine ──────────────────────────────────────────────────────── */

static void fwl_fail(fw_loader_error_t err)
{
    fwl_state = FW_LOADER_ERROR;
    fwl_error = err;
}

bool fw_loader_begin(uint32_t size, uint32_t crc32)
{
    /* A transfer in flight is replaced, not refused: the timeout path and a
     * host retry look identical from here, and the staged region is about
     * to be rewritten anyway. */
    fwl_state = FW_LOADER_IDLE;
    fwl_error = FW_LOADER_ERR_NONE;
    fwl_received = 0;
    fwl_chunk_fill = 0;
    fwl_erase_mark = FWL_STAGE_BASE;
    fwl_silence_polls = 0;

    if (size < FWL_MIN_IMAGE || size > FWL_STAGE_CAP || (size & 1u)) {
        fwl_fail(FW_LOADER_ERR_SIZE);
        return false;
    }
    fwl_expected = size;
    fwl_crc_announced = crc32;
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

/* Commit the RAM chunk buffer to the staging region: erase any page the
 * chunk reaches into (watermark — each page is erased exactly once, when
 * the stream first arrives at it), program, verify by read-back (the
 * settings store's discipline, applied here). */
static bool fwl_commit_chunk(void)
{
    uint32_t addr = FWL_STAGE_BASE + (fwl_received - fwl_chunk_fill);
    while (fwl_erase_mark < addr + fwl_chunk_fill) {
        if (!fwl_erase_page(fwl_erase_mark)) {
            return false;
        }
        fwl_erase_mark += FWL_PAGE_SIZE;
    }
    if (!fwl_program(addr, fwl_chunk, fwl_chunk_fill)) {
        return false;
    }
    const volatile uint8_t *rb = fwl_read_ptr(addr);
    for (uint32_t i = 0; i < fwl_chunk_fill; ++i) {
        if (rb[i] != fwl_chunk[i]) {
            return false;
        }
    }
    fwl_chunk_fill = 0;
    return true;
}

/* Final gate: CRC of the staged bytes AT REST, then a vector-table shape
 * check. Both run against staged flash, not the stream — what is checked
 * is exactly what fwapply will copy. */
static void fwl_finish(void)
{
    uint32_t crc = fwl_crc32(0, fwl_read_ptr(FWL_STAGE_BASE), fwl_expected);
    if (crc != fwl_crc_announced) {
        fwl_fail(FW_LOADER_ERR_CRC);
        return;
    }
    const volatile uint8_t *v = fwl_read_ptr(FWL_STAGE_BASE);
    uint32_t sp = (uint32_t)v[0] | ((uint32_t)v[1] << 8) |
                  ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
    uint32_t pc = (uint32_t)v[4] | ((uint32_t)v[5] << 8) |
                  ((uint32_t)v[6] << 16) | ((uint32_t)v[7] << 24);
    if ((sp & 0xFFF00000u) != 0x20000000u ||
        pc < FWL_APP_BASE || pc >= FWL_STAGE_BASE || (pc & 1u) == 0) {
        fwl_fail(FW_LOADER_ERR_VECTOR);
        return;
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
        uint32_t room = FWL_CHUNK_SIZE - fwl_chunk_fill;
        uint32_t take = len < room ? len : room;
        uint32_t remaining = fwl_expected - fwl_received;
        if (take > remaining) {
            take = remaining;
        }
        memcpy(&fwl_chunk[fwl_chunk_fill], data, take);
        fwl_chunk_fill += take;
        fwl_received += take;
        data += take;
        len = (uint16_t)(len - take);

        if (fwl_chunk_fill == FWL_CHUNK_SIZE ||
            fwl_received == fwl_expected) {
            if (!fwl_commit_chunk()) {
                fwl_fail(FW_LOADER_ERR_FLASH);
                return;
            }
        }
        if (fwl_received == fwl_expected) {
            fwl_finish();
            return; /* trailing bytes, if any, go back to the shell */
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
uint32_t fw_loader_bytes(void) { return fwl_received; }
uint32_t fw_loader_expected(void) { return fwl_expected; }
uint32_t fw_loader_crc_announced(void) { return fwl_crc_announced; }

/* ── The installer ──────────────────────────────────────────────────────
 * Adapted from the 2C23T port's fw_ram_install (Stlkv/OpenScope-2C23T-
 * 2C53T-port, fw_update.c), which has performed every field reflash of
 * that firmware since 2026-08. Differences here: source is bank-2 staging
 * (memory-mapped, readable while bank 1 programs), the power-hold pin is
 * this board's PC9, and the erase bound is the staging base rather than a
 * fixed slot size.
 *
 * Runs from RAM with IRQs off. After the first erase there is no way back:
 * on any failure past that point it parks in a dead loop with the power
 * rail held, and recovery is the stock IAP (MENU+Power) — which this
 * sequence can never touch, since it writes only 0x08007000 upward. */
#ifndef FW_LOADER_HOST_TEST

__attribute__((section(".data.ramfunc"), noinline, used))
static void fwl_ram_install(uint32_t src, uint32_t dst, uint32_t size)
{
    volatile uint32_t *sts  = (volatile uint32_t *)0x4002200Cu;
    volatile uint32_t *ctrl = (volatile uint32_t *)0x40022010u;
    volatile uint32_t *fadr = (volatile uint32_t *)0x40022014u;
    volatile uint32_t *keyr = (volatile uint32_t *)0x40022004u;
    uint32_t end = dst + ((size + (FWL_PAGE_SIZE - 1u)) & ~(FWL_PAGE_SIZE - 1u));
    uint32_t timeout;

    __asm__ volatile("cpsid i" ::: "memory");

    /* Keep the power rail held no matter what happens below (PC9). */
    *(volatile uint32_t *)0x40021018u |= (1u << 4);            /* GPIOC clk */
    *(volatile uint32_t *)0x40011010u = (1u << 9);             /* BSRR PC9  */

    if (dst != FWL_APP_BASE || size == 0 || (size & 1u) ||
        end > FWL_STAGE_BASE) {
        goto dead;
    }
    if (*ctrl & FWL_CTRL_LOCK) {
        *keyr = 0x45670123u;
        *keyr = 0xCDEF89ABu;
    }
    if (*ctrl & FWL_CTRL_LOCK) {
        goto dead;
    }

    /* No copy buffer anywhere below: src is bank-2 flash, which stays
     * readable while bank 1 is being erased and programmed. */
    for (uint32_t addr = dst; addr < end; addr += FWL_PAGE_SIZE) {
        timeout = 0x00FFFFFFu;
        while ((*sts & FWL_STS_BSY) && --timeout) {
        }
        if (!timeout) {
            goto dead;
        }
        *sts = FWL_STS_ODF | FWL_STS_PRGMERR | FWL_STS_WRPRTERR;
        *ctrl |= FWL_CTRL_PER;
        *fadr = addr;
        *ctrl |= FWL_CTRL_STRT;
        timeout = 0x00FFFFFFu;
        while ((*sts & FWL_STS_BSY) && --timeout) {
        }
        *ctrl &= ~FWL_CTRL_PER;
        if (!timeout || (*sts & (FWL_STS_PRGMERR | FWL_STS_WRPRTERR))) {
            goto dead;
        }
    }

    for (uint32_t off = 0; off < size; off += 2u) {
        uint16_t v = *(volatile uint8_t *)(src + off) |
                     ((uint16_t)*(volatile uint8_t *)(src + off + 1u) << 8);
        timeout = 0x00FFFFFFu;
        while ((*sts & FWL_STS_BSY) && --timeout) {
        }
        if (!timeout) {
            goto dead;
        }
        *sts = FWL_STS_ODF | FWL_STS_PRGMERR | FWL_STS_WRPRTERR;
        *ctrl |= FWL_CTRL_PG;
        *(volatile uint16_t *)(dst + off) = v;
        timeout = 0x00FFFFFFu;
        while ((*sts & FWL_STS_BSY) && --timeout) {
        }
        *ctrl &= ~FWL_CTRL_PG;
        if (!timeout || (*sts & (FWL_STS_PRGMERR | FWL_STS_WRPRTERR))) {
            goto dead;
        }
    }

    for (uint32_t off = 0; off < size; ++off) {
        if (*(volatile uint8_t *)(dst + off) !=
            *(volatile uint8_t *)(src + off)) {
            goto dead;
        }
    }

    /* Success: hand the CPU to the new image the way our own startup would
     * receive it — SysTick off, every IRQ disabled and pending-cleared,
     * VTOR at the new table. */
    *(volatile uint32_t *)0xE000E010u = 0;
    for (uint32_t i = 0; i < 8u; ++i) {
        *(volatile uint32_t *)(0xE000E180u + i * 4u) = 0xFFFFFFFFu;
        *(volatile uint32_t *)(0xE000E280u + i * 4u) = 0xFFFFFFFFu;
    }
    *(volatile uint32_t *)0xE000ED08u = dst;
    __asm__ volatile(
        "dsb\n"
        "isb\n"
        "ldr r0, [%0]\n"
        "ldr r1, [%0, #4]\n"
        "msr msp, r0\n"
        "bx r1\n"
        :
        : "r"(dst)
        : "r0", "r1", "memory");

dead:
    /* Unreachable on success. The rail stays held so the unit does not
     * power off with a half-written slot; recovery is MENU+Power (IAP). */
    while (1) {
    }
}

#endif /* !FW_LOADER_HOST_TEST */

bool fw_loader_apply(void)
{
    if (fwl_state != FW_LOADER_STAGED) {
        return false;
    }
    /* Re-check the CRC at the moment of truth — the staged region has been
     * sitting in flash since fwload finished, and this is the last cheap
     * instant to notice anything disturbed it. */
    if (fwl_crc32(0, fwl_read_ptr(FWL_STAGE_BASE), fwl_expected) !=
        fwl_crc_announced) {
        fwl_fail(FW_LOADER_ERR_CRC);
        return false;
    }
#ifndef FW_LOADER_HOST_TEST
    fwl_ram_install(FWL_STAGE_BASE, FWL_APP_BASE, fwl_expected);
    /* not reached */
#endif
    return true;
}
