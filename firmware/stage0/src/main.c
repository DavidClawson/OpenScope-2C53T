/*
 * Tiny low-flash stage0 for the stock/OpenScope switcher layout.
 *
 * The MCU always fetches its initial vector table from 0x08000000.  Therefore
 * a byte-for-byte untouched stock image cannot coexist with PC-only recovery.
 * This file keeps the unavoidable low-flash resident code to one 2KB sector and
 * moves the full USB HID recovery bootloader to 0x080F0000.
 */

#include <stdint.h>
#include "at32f403a_407.h"

#define FLASH_APP_ADDRESS             0x08004000u
#define HIGH_BOOTLOADER_ADDRESS       0x080F0000u
#define FLASH_MAX_ADDRESS             0x08100000u
#define DFU_RAM_MAGIC_ADDR            ((volatile uint32_t *)0x20037FE0u)
#define DFU_RAM_MAGIC_VALUE           0xDEADBEEFu
#define BOOT_COUNTER_ADDR             ((volatile uint32_t *)0x20037FDCu)
#define BOOT_COUNTER_MAGIC            0xB0070000u
#define BOOT_COUNTER_MASK             0xFFFF0000u
#define BOOT_FAIL_MAX                 3u
#define IAP_UPGRADE_COMPLETE_FLAG     0x41544F4Bu
#define IAP_FLAG_ADDRESS              0x08003800u

static void busy_delay(uint32_t loops)
{
    while (loops-- != 0u) {
        __NOP();
    }
}

static int valid_vector(uint32_t address, int high_recovery)
{
    uint32_t sp = *(const uint32_t *)address;
    uint32_t rv = *(const uint32_t *)(address + 4u);

    if ((sp & 0xFFF00000u) != 0x20000000u) {
        return 0;
    }
    if (rv < 0x08000000u || rv >= FLASH_MAX_ADDRESS) {
        return 0;
    }
    if (high_recovery && rv < HIGH_BOOTLOADER_ADDRESS) {
        return 0;
    }
    return 1;
}

static void jump_to_image(uint32_t address)
{
    uint32_t sp = *(const uint32_t *)address;
    uint32_t rv = *(const uint32_t *)(address + 4u);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    SCB->VTOR = address;
    __set_MSP(sp);
    ((void (*)(void))rv)();
}

static int recovery_combo_pressed(void)
{
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    /* PC8 POWER and PB7 PRM are active-low passive buttons. */
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 0)) | (0x8u << 0);
    GPIOC->scr = (1u << 8);
    GPIOB->cfglr = (GPIOB->cfglr & ~(0xFu << 28)) | (0x8u << 28);
    GPIOB->scr = (1u << 7);

    busy_delay(2400000u);
    return ((GPIOC->idt & (1u << 8)) == 0u) && ((GPIOB->idt & (1u << 7)) == 0u);
}

int main(void)
{
    int enter_recovery = 0;

    /* PC9 power hold must be asserted before any longer decision path. */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 4)) | (0x3u << 4);
    GPIOC->scr = (1u << 9);

    if (*DFU_RAM_MAGIC_ADDR == DFU_RAM_MAGIC_VALUE) {
        *DFU_RAM_MAGIC_ADDR = 0u;
        enter_recovery = 1;
    }

    if (!enter_recovery) {
        uint32_t boot_val = *BOOT_COUNTER_ADDR;
        if ((boot_val & BOOT_COUNTER_MASK) == BOOT_COUNTER_MAGIC &&
            (boot_val & 0xFFFFu) >= BOOT_FAIL_MAX) {
            *BOOT_COUNTER_ADDR = 0u;
            enter_recovery = 1;
        }
    }

    if (!enter_recovery && recovery_combo_pressed()) {
        enter_recovery = 1;
    }

    if (!enter_recovery &&
        *(const uint32_t *)IAP_FLAG_ADDRESS == IAP_UPGRADE_COMPLETE_FLAG &&
        valid_vector(FLASH_APP_ADDRESS, 0)) {
        uint32_t boot_val = *BOOT_COUNTER_ADDR;
        uint16_t fail_count = 0u;
        if ((boot_val & BOOT_COUNTER_MASK) == BOOT_COUNTER_MAGIC) {
            fail_count = (uint16_t)(boot_val & 0xFFFFu);
        }
        *BOOT_COUNTER_ADDR = BOOT_COUNTER_MAGIC | (uint32_t)(fail_count + 1u);
        jump_to_image(FLASH_APP_ADDRESS);
    }

    if (valid_vector(HIGH_BOOTLOADER_ADDRESS, 1)) {
        jump_to_image(HIGH_BOOTLOADER_ADDRESS);
    }

    while (1) {
        __NOP();
    }
}
