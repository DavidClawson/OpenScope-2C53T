/*
 * OpenScope 2C53T Bootloader - AT32 peripheral config
 * Minimal set: only CRM, GPIO, FLASH, USB, ACC, CRC, MISC
 */

#ifndef __AT32F403A_407_CONF_H
#define __AT32F403A_407_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined HEXT_VALUE
#define HEXT_VALUE               ((uint32_t)8000000)
#endif

#define HEXT_STARTUP_TIMEOUT     ((uint16_t)0x3000)
#define HICK_VALUE               ((uint32_t)8000000)
#define LEXT_VALUE               ((uint32_t)32768)

/* Only enable modules the bootloader actually uses */
#define CRM_MODULE_ENABLED
#define GPIO_MODULE_ENABLED
#define FLASH_MODULE_ENABLED
#define CRC_MODULE_ENABLED
#define USB_MODULE_ENABLED
#define ACC_MODULE_ENABLED
#define WDT_MODULE_ENABLED
#define MISC_MODULE_ENABLED

#ifdef CRM_MODULE_ENABLED
#include "at32f403a_407_crm.h"
#endif
#ifdef GPIO_MODULE_ENABLED
#include "at32f403a_407_gpio.h"
#endif
#ifdef FLASH_MODULE_ENABLED
#include "at32f403a_407_flash.h"
#endif
#ifdef CRC_MODULE_ENABLED
#include "at32f403a_407_crc.h"
#endif
#ifdef USB_MODULE_ENABLED
#include "at32f403a_407_usb.h"
#endif
#ifdef ACC_MODULE_ENABLED
#include "at32f403a_407_acc.h"
#endif
#ifdef WDT_MODULE_ENABLED
#include "at32f403a_407_wdt.h"
#endif
#ifdef MISC_MODULE_ENABLED
#include "at32f403a_407_misc.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
