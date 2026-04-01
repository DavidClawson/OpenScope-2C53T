/*
 * OpenScope 2C53T Bootloader - USB configuration
 */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "at32f403a_407.h"
#include <stddef.h>

#define USB_EPT_MAX_NUM                   8
#define USB_EPT_AUTO_MALLOC_BUFFER

void usb_delay_ms(uint32_t ms);
void usb_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif
