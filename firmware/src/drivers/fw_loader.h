/*
 * fw_loader.h — firmware image staging, caching, and installing over the
 * CDC debug shell.
 *
 * WHY THIS EXISTS
 * ---------------
 * Every reflash of this firmware used to need MENU+Power and the stock IAP
 * channel. This module makes the USB cable the whole story: an image is
 * streamed over the CDC shell into a cache slot on the W25Q (16 MB — two
 * 1 MB slots, so THIS firmware's own image fits, and so does the 2C23T
 * port's), verified at rest, and installed by a RAM-resident copier that
 * ends in a clean system reset. Both cache slots use the same manifest
 * format as the 2C23T port's fw_cache.c, so either firmware can install
 * an image the other one cached — switching between them is one command
 * (or one file drop) with no buttons and no cable fiddling.
 *
 * THE CONTRACT
 * ------------
 *   fwload <size> <crc32> [a|b]   stage: CDC RX is rerouted here until
 *                                 <size> raw bytes arrive; they stream
 *                                 into cache slot a/b (default b) on the
 *                                 W25Q via the audited flash_regions
 *                                 layer. On completion the slot is
 *                                 re-read, CRC-checked AT REST, vector-
 *                                 checked, and only then its manifest is
 *                                 written — a torn transfer can never
 *                                 look installable.
 *   fwstat                        state machine, byte count, slot table.
 *   fwapply                       install the just-staged slot.
 *   fwswap a|b                    install a previously cached slot — no
 *                                 host transfer at all.
 *
 * SAFETY POSTURE
 * --------------
 *   - Staging never touches the running app; abort/timeout just returns
 *     the shell. The manifest-last discipline means half-filled slots
 *     are invisible to every installer.
 *   - Install re-verifies the ENTIRE slot (manifest, full CRC, vector
 *     shape) at the moment of truth, copies with read-back verification,
 *     and SYSTEM-RESETS into the new image (a cross-firmware jump was
 *     bench-tried and half-bricks). Run it on USB power: PC9 drops
 *     during the reset, and the cable carries the rail — which fwapply
 *     guarantees by construction.
 *   - Nothing here can write below 0x08007000: the factory IAP
 *     bootloader is untouchable by construction, and MENU+Power remains
 *     the recovery path after ANY outcome.
 */

#ifndef FW_LOADER_H
#define FW_LOADER_H

#include <stdbool.h>
#include <stdint.h>

/* Shell-task loop iterations of RX silence before a transfer is declared dead
 * (~10 ms per idle iteration => ~3 s). Public because the shell task ages a
 * second deadline against it — the drain of an aborted image — and the two
 * must not drift apart. */
#define FW_LOADER_TIMEOUT_POLLS 300u

typedef enum {
    FW_LOADER_IDLE = 0,
    FW_LOADER_RECEIVING,
    FW_LOADER_STAGED,
    FW_LOADER_ERROR,
} fw_loader_state_t;

typedef enum {
    FW_LOADER_ERR_NONE = 0,
    FW_LOADER_ERR_SIZE,     /* size 0, odd, tiny, or over the app ceiling */
    FW_LOADER_ERR_FLASH,    /* W25Q erase/write/read failed               */
    FW_LOADER_ERR_CRC,      /* slot bytes' CRC != announced/manifest      */
    FW_LOADER_ERR_VECTOR,   /* image's SP/PC not app-slot shaped          */
    FW_LOADER_ERR_TIMEOUT,  /* RX went silent mid-transfer                */
    FW_LOADER_ERR_NO_IMAGE, /* slot holds no valid manifest               */
    FW_LOADER_ERR_NO_BUFFER,/* no scratch attached — a programming error   */
} fw_loader_error_t;

/* The loader owns no buffer. The shell task lends it one — at least
 * FW_LOADER_SCRATCH_MIN bytes — before the first fwload/fwswap; every entry
 * point below refuses with ERR_NO_BUFFER until that has happened. On target
 * the lender is usb_debug.c's shell_bus_scratch, the arena `spi3 read` /
 * `spi3 frame` already share: all of them run on the single shell task and
 * none can overlap a transfer (RX is routed here, so no other command runs)
 * or an install (interrupts are off). Lending rather than importing keeps
 * this module ignorant of USB, and the host test lends its own array.
 * len < FW_LOADER_SCRATCH_MIN detaches. */
#define FW_LOADER_SCRATCH_MIN 512u
void fw_loader_attach_scratch(uint8_t *buf, uint32_t len);

/* Shell entry points (usb_debug.c). */
bool fw_loader_begin(uint32_t size, uint32_t crc32, uint8_t slot);
void fw_loader_abort(void);
bool fw_loader_apply(void);              /* install the just-staged slot  */
bool fw_loader_install_slot(uint8_t slot); /* fwswap: install from cache  */

/* RX routing: while active(), the shell task hands every received CDC
 * byte to feed() instead of the line editor (shell-task context only). */
bool fw_loader_active(void);
void fw_loader_feed(const uint8_t *data, uint16_t len);

/* Called once per shell-task loop to age the RX-silence timeout. */
void fw_loader_poll(void);

/* ── Install breadcrumbs (2026-09-30, after EXP-57) ─────────────────────
 * EXP-57: `fwapply` hung on unit #1 inside the RAM installer, which has
 * eight exits into a silent spin and reported none of them. The installer
 * now keeps a record in five AT32 backup registers (BPR DT1..DT5, 16 bits
 * each), which survive a system or pinhole reset while the board stays
 * powered (NOT verified to survive the board going fully dark -- that
 * depends on VBAT wiring, unknown):
 *   DT1  FWL_BC_MAGIC once an install has started
 *   DT2  stage / exit code (below); bit 8 set if the SPI2 reclaim timed out
 *   DT3  high half of the flash page being worked on (updated every page)
 *   DT4  low half of it
 *   DT5  low 16 bits of that bank's flash STS register at the exit
 * and, on a failure exit, blinks the backlight `code` times, pauses, and
 * repeats -- a readout that needs nothing to survive. The app captures and
 * clears the record once at boot; `fwstat` prints it. */
#define FWL_BC_MAGIC 0xF1A5u
enum {
    FWL_BC_NONE        = 0x00,
    FWL_BC_BAD_SIZE    = 0x01,  /* size 0, odd, or past the app ceiling      */
    FWL_BC_UNLOCK      = 0x02,  /* flash bank stayed locked after the keys   */
    FWL_BC_ERASE_BUSY  = 0x03,  /* busy never cleared before the erase       */
    FWL_BC_ERASE_ERR   = 0x04,  /* erase timed out or raised PRGMERR/EPPERR  */
    FWL_BC_PROG_BUSY   = 0x05,  /* busy never cleared before a halfword      */
    FWL_BC_PROG_ERR    = 0x06,  /* program timed out or raised an error      */
    FWL_BC_VERIFY      = 0x07,  /* read-back differs from what was written   */
    FWL_BC_STARTED     = 0x10,  /* running (still set = died without an exit)*/
    FWL_BC_DONE        = 0xAA,  /* every page verified; reset issued         */
};
#define FWL_BC_SPI2_STALL 0x0100u

typedef struct {
    bool     present;   /* magic found                                 */
    uint16_t code;      /* FWL_BC_* (low byte)                         */
    bool     spi2_stall;
    uint32_t addr;      /* page address at the exit                    */
    uint16_t sts;       /* flash STS low half at the exit              */
} fwl_breadcrumb_t;

/* Pure: decode the five backup words (host-tested). */
fwl_breadcrumb_t fw_loader_breadcrumb_decode(const uint16_t w[5]);
const char *fw_loader_breadcrumb_name(uint16_t code);

/* Target: read the backup registers once, keep the result, clear them. */
void fw_loader_breadcrumb_capture(void);
const fwl_breadcrumb_t *fw_loader_breadcrumb_last(void);

fw_loader_state_t fw_loader_state(void);
fw_loader_error_t fw_loader_error(void);
uint8_t  fw_loader_slot(void);
uint32_t fw_loader_bytes(void);
uint32_t fw_loader_expected(void);
uint32_t fw_loader_crc_announced(void);
/* Cache slot probes (0 = no valid manifest). */
uint32_t fw_loader_slot_size(uint8_t slot);
uint32_t fw_loader_slot_crc(uint8_t slot);

#endif /* FW_LOADER_H */
