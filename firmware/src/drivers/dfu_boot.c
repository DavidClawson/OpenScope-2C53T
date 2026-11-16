/*
 * OpenScope 2C53T - DFU Boot Entry
 *
 * Writes a magic word to a fixed RAM address, then resets.
 * The bootloader at 0x08000000 checks this same address on startup.
 *
 * Address 0x20037FE0 is chosen to be:
 *   - Below _estack (0x20037FF0) — not touched by initial SP
 *   - Well above BSS end (~0x20036914) — not zeroed by startup
 *   - In the stack growth area, but only used momentarily before reset
 */

#include "dfu_boot.h"
#include "at32f403a_407.h"

#define DFU_MAGIC_ADDR   ((volatile uint32_t *)0x20037FE0)
#define DFU_MAGIC_VALUE  0xDEADBEEF

/* Boot validation counter — must match bootloader's definition */
#define BOOT_COUNTER_ADDR    ((volatile uint32_t *)0x20037FDC)

/* Hand the chip to the bootloader at 0x08000000 WITHOUT a reset.
 *
 * On the 2C53T the supply is latched by PC9 (power hold), which the bootloader
 * drives high first thing. A system reset returns PC9 to an input, the latch
 * drops and the scope switches off unless POWER happens to be held (bench,
 * 2026-11-10): every `reboot bootloader` and every flash then needed a button
 * press. A direct jump keeps PC9 driven, so updates can run unattended.
 *
 * The chip is made quiet first, as the bootloader's own jump_to_app() does:
 * interrupts and SysTick off, all NVIC enables and pendings cleared, DMA
 * channels stopped (a running transfer would scribble on the bootloader's
 * RAM), and the USB peripheral reset so the host sees a disconnect and
 * re-enumerates the HID updater. Clocks are left as they are: the bootloader's
 * system_clock_config() starts with crm_reset(). We are in a FreeRTOS task
 * (thread mode, PSP), so CONTROL goes back to MSP before the new stack is set. */
static void dfu_jump_to_bootloader(void) __attribute__((noreturn));
static void dfu_jump_to_bootloader(void)
{
    const uint32_t base = 0x08000000u;
    uint32_t sp = *(const volatile uint32_t *)base;
    uint32_t pc = *(const volatile uint32_t *)(base + 4u);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    dma_channel_type *const ch[] = {
        DMA1_CHANNEL1, DMA1_CHANNEL2, DMA1_CHANNEL3, DMA1_CHANNEL4, DMA1_CHANNEL5,
        DMA1_CHANNEL6, DMA1_CHANNEL7, DMA2_CHANNEL1, DMA2_CHANNEL2, DMA2_CHANNEL3,
        DMA2_CHANNEL4, DMA2_CHANNEL5, DMA2_CHANNEL6, DMA2_CHANNEL7,
    };
    for (unsigned i = 0; i < sizeof(ch) / sizeof(ch[0]); i++) ch[i]->ctrl_bit.chen = 0;
    crm_periph_reset(CRM_USB_PERIPH_RESET, TRUE);
    crm_periph_reset(CRM_USB_PERIPH_RESET, FALSE);

    if ((sp & 0xFFF00000u) != 0x20000000u || pc < base || pc >= base + 0x4000u) {
        NVIC_SystemReset();      /* no sane bootloader there: the old way */
    }
    SCB->VTOR = base;
    __set_CONTROL(0);            /* privileged, MSP, no FP context */
    __ISB();
    __set_MSP(sp);
    __enable_irq();              /* as after reset; every NVIC line is off */
    ((void (*)(void))pc)();
    for (;;) { }
}

void dfu_request_reboot(void) {
    *DFU_MAGIC_ADDR = DFU_MAGIC_VALUE;
    __DSB();
    dfu_jump_to_bootloader();
}

void dfu_check_magic(void) {
}

void dfu_check_boot_button(void) {
}

/* Must match bootloader's definition */
#define BOOT_COUNTER_MAGIC   0xB0070000

void boot_validate(void) {
    /* Reset the boot attempt counter to zero while preserving the magic
     * marker. The bootloader checks (val & 0xFFFF0000) == BOOT_COUNTER_MAGIC
     * to recognize a valid counter.
     *
     * Previously this wrote plain 0, which broke the 3-strike system:
     * the bootloader wouldn't recognize 0 as a valid counter, so the
     * fail count never accumulated across crashes that happen AFTER
     * boot_validate() (e.g., FreeRTOS task crashes, watchdog resets).
     *
     * With the magic preserved, the flow is:
     *   Boot 1: bootloader writes MAGIC|1, app writes MAGIC|0 (success)
     *   Boot 2: bootloader writes MAGIC|1, app crashes → watchdog reset
     *   Boot 3: bootloader writes MAGIC|2, app crashes → watchdog reset
     *   Boot 4: bootloader sees count=3 ≥ BOOT_FAIL_MAX → safe mode */
    *BOOT_COUNTER_ADDR = BOOT_COUNTER_MAGIC;  /* count = 0, magic preserved */
    __DSB();
}
