/*
 * OpenScope 2C53T - USB Configuration
 *
 * Required by AT32 USB middleware (usbd_core.h includes this).
 * Defines endpoint buffer layout and helper function declarations.
 */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#include <stddef.h>
#include "at32f403a_407.h"
#include "at32f403a_407_usb.h"

/* Maximum endpoint count */
#define USB_EPT_MAX_NUM                   8

/* Endpoint buffer addresses in USB packet memory (512 bytes total).
 * Each endpoint needs TX and RX buffer space. Layout:
 *   0x00-0x3F  reserved (packet memory table)
 *   0x40-0x7F  EPT0 TX (64 bytes)
 *   0x80-0xBF  EPT0 RX (64 bytes)
 *   0xC0-0xFF  EPT1 TX (64 bytes) - CDC bulk IN
 *   0x100-0x13F EPT1 RX (64 bytes) - CDC bulk OUT
 *   0x140-0x17F EPT2 TX (64 bytes) - CDC interrupt IN
 *   0x180-0x1BF EPT2 RX (64 bytes) - unused but allocated
 */
#define EPT0_TX_ADDR                      0x40
#define EPT0_RX_ADDR                      0x80
#define EPT1_TX_ADDR                      0xC0
#define EPT1_RX_ADDR                      0x100
#define EPT2_TX_ADDR                      0x140
#define EPT2_RX_ADDR                      0x180
#define EPT3_TX_ADDR                      0x00
#define EPT3_RX_ADDR                      0x00
#define EPT4_TX_ADDR                      0x00
#define EPT4_RX_ADDR                      0x00
#define EPT5_TX_ADDR                      0x00
#define EPT5_RX_ADDR                      0x00
#define EPT6_TX_ADDR                      0x00
#define EPT6_RX_ADDR                      0x00
#define EPT7_TX_ADDR                      0x00
#define EPT7_RX_ADDR                      0x00

/* WinUSB not needed */
#define USBD_SUPPORT_WINUSB               0

/* Delay helpers (implemented in usb_debug.c) */
void usb_delay_ms(uint32_t ms);
void usb_delay_us(uint32_t us);

#endif /* __USB_CONF_H */
