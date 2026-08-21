/*
 * fw_loader.h — firmware image loading over the CDC debug shell.
 *
 * WHY THIS EXISTS
 * ---------------
 * Every reflash of this firmware needs MENU+Power and the stock IAP channel.
 * That is fine as a recovery path and terrible as an iteration loop — and it
 * is the one thing keeping a unit from switching between this firmware and
 * another 0x08007000-linked image (stock, the 2C23T port) over nothing but
 * the USB cable. This module is the receiving half: the host streams an
 * image over the CDC shell, it is staged to free internal flash, CRC-checked,
 * and only then installed over the app slot by a RAM-resident copier.
 *
 * THE CONTRACT
 * ------------
 *   fwload <size> <crc32>   stage: shell switches the CDC RX stream to this
 *                           module until <size> raw bytes have arrived (or
 *                           it is aborted). Bytes are written to the staging
 *                           region as they arrive; nothing touches the app.
 *   fwstat                  report the state machine, byte count, CRC.
 *   fwapply                 install: only accepted when a staged image has
 *                           passed the size gate, the CRC check and a vector
 *                           table sanity check. Copies staged -> app slot in
 *                           a RAM function with IRQs off, verifies by
 *                           read-back, and SYSTEM-RESETS into the new image
 *                           (a cross-firmware jump without a reset was bench-
 *                           tried and half-bricks — see fw_loader.c).
 *
 * SAFETY POSTURE
 * --------------
 *   - Staging is physically separate from the app: a torn transfer leaves
 *     the running firmware untouched and the device fully alive. Abort and
 *     resend at will.
 *   - fwapply is the ONLY step that can brick the app slot, its inputs are
 *     CRC-verified at rest (staged flash, not a stream), and a failed apply
 *     still leaves the stock IAP (MENU+Power) as recovery.
 *   - The staging and app geometry are compile-time constants; nothing in
 *     the stream chooses addresses.
 *   - The staging region (0x080A0000+) caps images at ~382 KB, which fits
 *     the 2C23T-port image (~120 KB) but NOT this firmware's own ~600 KB
 *     image. Round-tripping THIS firmware over USB needs W25Q staging — a
 *     follow-up, not this module.
 */

#ifndef FW_LOADER_H
#define FW_LOADER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FW_LOADER_IDLE = 0,     /* nothing staged, nothing in flight            */
    FW_LOADER_RECEIVING,    /* fwload accepted, raw RX bytes being consumed */
    FW_LOADER_STAGED,       /* size + CRC + vector checks all passed        */
    FW_LOADER_ERROR,        /* see fw_loader_error(); fwload again to retry */
} fw_loader_state_t;

typedef enum {
    FW_LOADER_ERR_NONE = 0,
    FW_LOADER_ERR_SIZE,     /* size 0, odd, too small, or > staging region  */
    FW_LOADER_ERR_FLASH,    /* staging erase/program/verify failed          */
    FW_LOADER_ERR_CRC,      /* staged bytes' CRC != the one announced       */
    FW_LOADER_ERR_VECTOR,   /* staged image's SP/PC not app-slot shaped     */
    FW_LOADER_ERR_TIMEOUT,  /* RX went silent mid-transfer; re-run fwload   */
} fw_loader_error_t;

/* Shell command entry points (usb_debug.c). begin() validates and switches
 * the shell's RX routing; apply() never returns on success. */
bool fw_loader_begin(uint32_t size, uint32_t crc32);
void fw_loader_abort(void);
bool fw_loader_apply(void);

/* RX routing: while active(), the shell task must hand every received CDC
 * byte to feed() instead of the line editor. feed() is called on the shell
 * task, never from an ISR. */
bool fw_loader_active(void);
void fw_loader_feed(const uint8_t *data, uint16_t len);

/* Called once per shell-task loop iteration to age the RX-silence timeout. */
void fw_loader_poll(void);

fw_loader_state_t fw_loader_state(void);
fw_loader_error_t fw_loader_error(void);
uint32_t fw_loader_bytes(void);
uint32_t fw_loader_expected(void);
uint32_t fw_loader_crc_announced(void);

#endif /* FW_LOADER_H */
