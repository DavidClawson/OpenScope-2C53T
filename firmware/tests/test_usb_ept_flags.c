/*
 * Host test for the endpoint flag-clear fix (src/drivers/usb_ept_flags.h).
 *
 * Models the AT32/STM32 USB endpoint register's write semantics:
 *   TXTC (bit 7), RXTC (bit 15): write 0 clears, write 1 has no effect;
 *   TXSTS, TXDTS, RXSTS, RXDTS:  write 1 toggles, write 0 has no effect;
 *   SETUPTC (bit 11):            read-only;
 *   the rest:                    plain read/write.
 * The race under test: the ISR reads the register to clear one completion
 * flag, the hardware sets the OTHER flag before the write lands. The vendor
 * expression must lose that flag in this model (the negative control); the
 * fixed one must keep it and leave every status and toggle bit untouched.
 */

#include <stdint.h>
#include <stdio.h>

/* Values from at32f403a_407_usb.h (lib v2.2.2), lines 88-103. */
#define USB_EPTADDR         0x000Fu
#define USB_TXSTS           0x0030u
#define USB_TXDTS           0x0040u
#define USB_TXTC            0x0080u
#define USB_EXF             0x0100u
#define USB_TRANS_TYPE      0x0600u
#define USB_SETUPTC         0x0800u
#define USB_RXSTS           0x3000u
#define USB_RXDTS           0x4000u
#define USB_RXTC            0x8000u
#define USB_EPT_BIT_MASK    (USB_TXTC | USB_SETUPTC | USB_EPTADDR | USB_EXF | USB_RXTC | USB_TRANS_TYPE)
#define USB_TX_NAK          0x0020u
#define USB_TX_VALID        0x0030u
#define USB_RX_NAK          0x2000u
#define USB_RX_VALID        0x3000u

#include "../src/drivers/usb_ept_flags.h"

#define VENDOR_VALUE_CLEARING_TXTC(read_value) ((uint16_t)((read_value) & 0xFF7F & USB_EPT_BIT_MASK))
#define VENDOR_VALUE_CLEARING_RXTC(read_value) ((uint16_t)((read_value) & 0x7FFF & USB_EPT_BIT_MASK))

#define CLEAR_ON_ZERO_BITS  (USB_TXTC | USB_RXTC)
#define TOGGLE_BITS         (USB_TXSTS | USB_TXDTS | USB_RXSTS | USB_RXDTS)
#define READ_ONLY_BITS      USB_SETUPTC

static int checks_run;
static int checks_failed;

static void check(int condition, const char *description)
{
    checks_run++;

    if (!condition) {
        checks_failed++;
        printf("  FAIL: %s\n", description);
    }
}

static uint16_t register_after_write(uint16_t register_value, uint16_t written_value)
{
    uint16_t clear_on_zero = register_value & written_value & CLEAR_ON_ZERO_BITS;
    uint16_t toggled = (register_value ^ written_value) & TOGGLE_BITS;
    uint16_t read_only = register_value & READ_ONLY_BITS;
    uint16_t plain = written_value & (uint16_t)~(CLEAR_ON_ZERO_BITS | TOGGLE_BITS | READ_ONLY_BITS);
    return clear_on_zero | toggled | read_only | plain;
}

/* EPT1 as CDC leaves it: bulk, address 1, IN just completed (TX NAK, TXTC
 * set), OUT armed (RX VALID), some data toggles set. */
static const uint16_t endpoint_after_in_completion = USB_TXTC | USB_TX_NAK | USB_TXDTS | USB_RX_VALID | 0x0001u;

static void test_clearing_txtc_keeps_an_rxtc_that_lands_mid_write(void)
{
    uint16_t read_value = endpoint_after_in_completion;
    /* The host's OUT packet completes between the read and the write: the
     * hardware sets RXTC and moves RX VALID -> NAK. */
    uint16_t register_at_write = (uint16_t)((read_value | USB_RXTC) & ~USB_RXSTS) | USB_RX_NAK;

    uint16_t vendor_result = register_after_write(register_at_write, VENDOR_VALUE_CLEARING_TXTC(read_value));
    check((vendor_result & USB_RXTC) == 0, "control: the vendor TXTC clear loses the OUT completion in this model");

    uint16_t fixed_result = register_after_write(register_at_write, USB_EPT_VALUE_CLEARING_TXTC(read_value));
    check((fixed_result & USB_RXTC) != 0, "fixed TXTC clear keeps the OUT completion");
    check((fixed_result & USB_TXTC) == 0, "fixed TXTC clear still clears TXTC");
    check((fixed_result & TOGGLE_BITS) == (register_at_write & TOGGLE_BITS), "fixed TXTC clear toggles no status bit");
    check((fixed_result & USB_EPTADDR) == 1, "endpoint address kept");
}

static void test_clearing_rxtc_keeps_a_txtc_that_lands_mid_write(void)
{
    uint16_t read_value = USB_RXTC | USB_RX_NAK | USB_TX_VALID | 0x0001u;
    /* The IN packet goes out between the read and the write: the hardware
     * sets TXTC and moves TX VALID -> NAK (the #39 lost completion). */
    uint16_t register_at_write = (uint16_t)((read_value | USB_TXTC) & ~USB_TXSTS) | USB_TX_NAK;

    uint16_t vendor_result = register_after_write(register_at_write, VENDOR_VALUE_CLEARING_RXTC(read_value));
    check((vendor_result & USB_TXTC) == 0, "control: the vendor RXTC clear loses the IN completion in this model");

    uint16_t fixed_result = register_after_write(register_at_write, USB_EPT_VALUE_CLEARING_RXTC(read_value));
    check((fixed_result & USB_TXTC) != 0, "fixed RXTC clear keeps the IN completion");
    check((fixed_result & USB_RXTC) == 0, "fixed RXTC clear still clears RXTC");
    check((fixed_result & TOGGLE_BITS) == (register_at_write & TOGGLE_BITS), "fixed RXTC clear toggles no status bit");
}

static void test_without_a_race_the_fix_matches_the_vendor_clear(void)
{
    uint16_t register_values[] = {
        endpoint_after_in_completion,
        USB_RXTC | USB_RX_NAK | USB_TX_NAK | USB_RXDTS | 0x0001u,
        USB_TXTC | USB_RXTC | USB_TX_NAK | USB_RX_NAK | USB_EXF | 0x0201u,
    };

    for (unsigned index = 0; index < sizeof(register_values) / sizeof(register_values[0]); index++) {
        uint16_t register_value = register_values[index];
        check(register_after_write(register_value, USB_EPT_VALUE_CLEARING_TXTC(register_value)) ==
              register_after_write(register_value, VENDOR_VALUE_CLEARING_TXTC(register_value)),
              "TXTC clear with no race: same register as the vendor macro");
        check(register_after_write(register_value, USB_EPT_VALUE_CLEARING_RXTC(register_value)) ==
              register_after_write(register_value, VENDOR_VALUE_CLEARING_RXTC(register_value)),
              "RXTC clear with no race: same register as the vendor macro");
    }
}

int main(void)
{
    test_clearing_txtc_keeps_an_rxtc_that_lands_mid_write();
    test_clearing_rxtc_keeps_a_txtc_that_lands_mid_write();
    test_without_a_race_the_fix_matches_the_vendor_clear();
    printf("test_usb_ept_flags\n%d/%d checks passed\n", checks_run - checks_failed, checks_run);
    return checks_failed == 0 ? 0 : 1;
}
