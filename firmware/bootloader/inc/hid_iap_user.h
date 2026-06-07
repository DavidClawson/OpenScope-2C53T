/*
 * OpenScope 2C53T Bootloader - HID IAP user config
 * Adapted from ArteryTek SDK with our flash layout
 */

#ifndef __HID_IAP_USER_H
#define __HID_IAP_USER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hid_iap_class.h"

/* Application starts at 0x08004000.  The upper 64KB is reserved for a
 * relocated recovery bootloader, so HID app writes must stop before it. */
#define FLASH_APP_ADDRESS                0x08004000
#define FLASH_BASE_ADDRESS               0x08000000
#ifndef FLASH_APP_END_ADDRESS
#define FLASH_APP_END_ADDRESS            0x080F0000
#endif

#ifndef BOOTLOADER_BASE_ADDRESS
#define BOOTLOADER_BASE_ADDRESS          0x08000000
#endif

void iap_init(void);
iap_result_type iap_get_upgrade_flag(void);
void iap_loop(void);
void jump_to_app(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif
