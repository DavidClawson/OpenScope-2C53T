/*
 * Endpoint-register value that clears one completion flag without erasing the
 * other one (#39, and the matching receive-side hang).
 *
 * TXTC and RXTC are write-0-to-clear, write-1-no-effect. Artery's
 * USB_CLEAR_TXTC / USB_CLEAR_RXTC (at32f403a_407_usb.h, lib v2.2.2) write the
 * OTHER flag back as it was read, so a completion the hardware sets between
 * that read and the write is cleared without ever being serviced. CDC IN and
 * OUT share EPT1: a lost IN completion leaves g_tx_completed at 0 (#39), a
 * lost OUT completion leaves the OUT endpoint NAKing for good. Writing the
 * other flag as 1 is what ST's PCD_CLEAR_RX_EP_CTR / PCD_CLEAR_TX_EP_CTR do.
 *
 * Plain value macros so the host test can drive them through a model of the
 * register. Needs USB_TXTC, USB_RXTC and USB_EPT_BIT_MASK defined first.
 */

#ifndef USB_EPT_FLAGS_H
#define USB_EPT_FLAGS_H

#define USB_EPT_VALUE_CLEARING_TXTC(read_value) \
    ((uint16_t)(((read_value) & 0xFF7F & USB_EPT_BIT_MASK) | USB_RXTC))
#define USB_EPT_VALUE_CLEARING_RXTC(read_value) \
    ((uint16_t)(((read_value) & 0x7FFF & USB_EPT_BIT_MASK) | USB_TXTC))

#endif /* USB_EPT_FLAGS_H */
