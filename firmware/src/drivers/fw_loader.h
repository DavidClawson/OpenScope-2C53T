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
} fw_loader_error_t;

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
