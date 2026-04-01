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

/* Application starts at 0x08004000 (after 16KB bootloader region) */
#define FLASH_APP_ADDRESS                0x08004000

void iap_init(void);
iap_result_type iap_get_upgrade_flag(void);
void iap_loop(void);
void jump_to_app(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif
