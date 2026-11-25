/*
 * OpenScope 2C53T - power latch, boot classification and power-off.
 * See power.h for the hardware model and the stock addresses it follows.
 */

#include "power.h"

bool power_charge_req_valid(uint32_t ctrlsts, bool charge_req)
{
    return charge_req && !(ctrlsts & POWER_RSTF_POR);
}

power_boot_t power_boot_classify(uint32_t ctrlsts, bool power_held,
                                 bool charge_req, bool latched)
{
    /* Our own power-off: the charge screen, even if POWER is still down
     * from the hold that asked for it. It only counts without POR: on USB
     * power_off() ends in a software reset with the rail up, and the flags
     * were cleared on the boot that asked. POR means the rail went down
     * after the request (battery, or USB pulled during the wait), so the
     * request is stale, however well SRAM kept it, and the boot is judged
     * like any power-on below. Honouring it would put a POWER press on
     * battery onto the charge screen, which dies when POWER is released. */
    if (power_charge_req_valid(ctrlsts, charge_req))
        return POWER_BOOT_CHARGE;

    /* Someone is holding POWER: the user switched it on. */
    if (power_held)
        return POWER_BOOT_RUN;

    /* The factory IAP latched the power and jumped here after a flash. */
    if (latched)
        return POWER_BOOT_RUN;

    /* Alive at power-on with POWER up means external power did it: USB was
     * plugged into a switched-off unit. On battery this cannot happen for
     * long; with PC9 LOW the rail simply collapses, which is "off".
     *
     * The flags accumulate until cleared, and nothing before us clears them
     * (the factory IAP never writes RSTFC). So POR plus SW means a software
     * reset happened after power-on without our app running in between:
     * the factory IAP's only reset, after programming EOPB0 (0x08001954).
     * That one boots, as does a watchdog reset. */
    if ((ctrlsts & POWER_RSTF_POR) &&
        !(ctrlsts & (POWER_RSTF_SW | POWER_RSTF_WDT | POWER_RSTF_WWDT)))
        return POWER_BOOT_CHARGE;

    /* Pinhole, software reset, watchdog: boot as before. */
    return POWER_BOOT_RUN;
}

#ifndef POWER_HOST_TEST

#include "at32f403a_407.h"

extern void delay_ms(uint32_t ms);   /* main.c, busy-wait */

/* Request word for the next boot. .noinit is NOLOAD, so startup leaves it
 * alone and it survives a system reset. It sits far above the factory IAP's
 * RAM (its stack starts at 0x20001A08), the same reason g_fault survives.
 * Value plus complement, so power-on RAM noise cannot fake a request. */
#define POWER_REQ_CHARGE  0x43485247u   /* "CHRG" */
static volatile uint32_t g_power_req[2] __attribute__((section(".noinit")));

static bool after_poweroff;

static void pc9_output(void)
{
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 4)) | (0x3u << 4); /* PC9 PP 50MHz */
}

bool power_button_held(void)
{
    return (GPIOC->idt & (1u << 8)) == 0;
}

power_boot_t power_boot_decide(void)
{
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

#ifdef GUEST_BUILD
    /* PC9 an output (MODE != 00) and driven HIGH: the factory IAP's
     * upgrade-mode latch, still in place after its post-flash jump. */
    bool latched = ((GPIOC->cfghr >> 4) & 0x3u) != 0 &&
                   (GPIOC->odt & (1u << 9)) != 0;
#else
    bool latched = false;   /* our HID bootloader latches PC9 every boot */
#endif

    /* PC8 input with pull-up: CNF=10, MODE=00, ODT bit = pull-up. The
     * factory IAP already leaves it like this; set it anyway. */
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 0)) | (0x8u << 0);
    GPIOC->scr   = (1u << 8);
    for (volatile uint32_t i = 0; i < 2000u; i++) { }

    /* Two of three samples LOW = held. A misread is harmless: the charge
     * screen boots on a 300 ms hold anyway. */
    int lows = 0;
    for (int i = 0; i < 3; i++) {
        if (power_button_held()) lows++;
        for (volatile uint32_t d = 0; d < 200u; d++) { }
    }
    bool held = lows >= 2;

    uint32_t ctrlsts = CRM->ctrlsts;
    CRM->ctrlsts_bit.rstfc = 1;            /* clear for the next boot */

    bool req = (g_power_req[0] == POWER_REQ_CHARGE) &&
               (g_power_req[1] == ~POWER_REQ_CHARGE);
    g_power_req[0] = 0;
    g_power_req[1] = 0;

    power_boot_t mode = power_boot_classify(ctrlsts, held, req, latched);
    after_poweroff = (mode == POWER_BOOT_CHARGE) &&
                     power_charge_req_valid(ctrlsts, req);

    pc9_output();
    if (mode == POWER_BOOT_RUN)
        GPIOC->scr = (1u << 9);            /* latch the battery path */
    else
        GPIOC->clr = (1u << 9);            /* stock 0x0802AAB6: release it */
    return mode;
}

bool power_boot_after_poweroff(void)
{
    return after_poweroff;
}

void power_latch_on(void)
{
    pc9_output();
    GPIOC->scr = (1u << 9);
}

void power_off(void)
{
    g_power_req[0] = POWER_REQ_CHARGE;
    g_power_req[1] = ~POWER_REQ_CHARGE;
    __DSB();

    GPIOC->clr = (1u << 9);

    /* On battery the rail collapses well inside this wait. Still running
     * afterwards means USB is holding it up. delay_ms() busy-waits, so this
     * is safe from a task or before the scheduler alike. */
    for (int i = 0; i < 40; i++) {
        wdt_counter_reload();
        delay_ms(10);
    }

    __disable_irq();
    NVIC_SystemReset();                    /* stock 0x0802F314 */
    while (1) { }
}

#endif /* POWER_HOST_TEST */
