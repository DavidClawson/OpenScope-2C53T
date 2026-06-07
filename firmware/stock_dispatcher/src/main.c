/*
 * High-flash launcher for the stock APP image.
 *
 * APP_2C53T_V1.2.0 is linked for 0x08007000 and writes VTOR=0x08007000 during
 * startup.  The low vector at 0x08000000 redirects here, this launcher keeps
 * the high HID recovery image reachable, then jumps through the stock vector
 * table at its linked address.
 */

#include <stdint.h>
#include "at32f403a_407.h"

#define FLASH_BASE_ADDRESS            0x08000000u
#define STOCK_APP_ADDRESS             0x08007000u
#define FLASH_APP_ADDRESS             0x08004000u
#define HIGH_BOOTLOADER_ADDRESS       0x080F0000u
#define FLASH_MAX_ADDRESS             0x08100000u
#define STOCK_TAIL_CHECK_ADDRESS      (STOCK_APP_ADDRESS + 0x00000800u)
#define STOCK_RUNTIME_TABLE_CHECK_ADDRESS (STOCK_APP_ADDRESS + 0x00033F7Cu)
#define STOCK_END_CHECK_ADDRESS       (STOCK_APP_ADDRESS + 0x000B7670u)
#define DFU_RAM_MAGIC_ADDR            ((volatile uint32_t *)0x20037FE0u)
#define DFU_RAM_MAGIC_VALUE           0xDEADBEEFu
#define BOOT_COUNTER_ADDR             ((volatile uint32_t *)0x20037FDCu)
#define BOOT_COUNTER_MAGIC            0xB0070000u
#define BOOT_COUNTER_MASK             0xFFFF0000u
#define BOOT_FAIL_MAX                 3u
#define IAP_UPGRADE_COMPLETE_FLAG     0x41544B38u
#define IAP_FLAG_ADDRESS              0x08003800u
#define STOCK_ALLOCATOR_CALLBACK_ADDR ((volatile uint32_t *)0x20001070u)
#define STOCK_ALLOCATOR_HEAP_BASE_ADDR ((volatile uint32_t *)0x20001078u)
#define STOCK_ALLOCATOR_BITMAP_ADDR   ((volatile uint32_t *)0x2000107Cu)
#define STOCK_ALLOCATOR_READY_ADDR    ((volatile uint32_t *)0x20001080u)
#define STOCK_ALLOCATOR_HEAP_BASE     0x20008000u
#define STOCK_ALLOCATOR_BITMAP_BASE   0x20005000u
#define STOCK_ALLOCATOR_BITMAP_SIZE   0x2BC0u

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
    if (rv < FLASH_BASE_ADDRESS || rv >= FLASH_MAX_ADDRESS) {
        return 0;
    }
    if (high_recovery && rv < HIGH_BOOTLOADER_ADDRESS) {
        return 0;
    }
    return 1;
}

static int stock_tail_valid(void)
{
    return (*(const uint32_t *)STOCK_TAIL_CHECK_ADDRESS == 0xF04FB430u) &&
           (*(const uint32_t *)STOCK_RUNTIME_TABLE_CHECK_ADDRESS == 0x008F008Fu) &&
           (*(const uint32_t *)STOCK_END_CHECK_ADDRESS == 0x3F027302u);
}

static int recovery_combo_pressed(void)
{
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 0)) | (0x8u << 0);
    GPIOC->scr = (1u << 8);
    GPIOB->cfglr = (GPIOB->cfglr & ~(0xFu << 28)) | (0x8u << 28);
    GPIOB->scr = (1u << 7);

    busy_delay(2400000u);
    return ((GPIOC->idt & (1u << 8)) == 0u) && ((GPIOB->idt & (1u << 7)) == 0u);
}

static void seed_stock_allocator_descriptor(void)
{
    /* Shifted stock reaches master init without the original launch context
     * that materializes this descriptor.  Seed only the descriptor; stock still
     * clears the allocation bitmap before it starts using it. */
    *STOCK_ALLOCATOR_CALLBACK_ADDR = 0u;
    *STOCK_ALLOCATOR_HEAP_BASE_ADDR = STOCK_ALLOCATOR_HEAP_BASE;
    *STOCK_ALLOCATOR_BITMAP_ADDR = STOCK_ALLOCATOR_BITMAP_BASE;
    *STOCK_ALLOCATOR_READY_ADDR = 0u;
    for (uint32_t i = 0; i < STOCK_ALLOCATOR_BITMAP_SIZE / sizeof(uint32_t); i++) {
        ((volatile uint32_t *)STOCK_ALLOCATOR_BITMAP_BASE)[i] = 0u;
    }
    __DSB();
}

static void jump_to_entry(uint32_t vtor,
                          uint32_t sp,
                          uint32_t entry,
                          int enable_irq,
                          uint32_t systick_ctrl_seed) __attribute__((noreturn));

static void jump_to_entry(uint32_t vtor,
                          uint32_t sp,
                          uint32_t entry,
                          int enable_irq,
                          uint32_t systick_ctrl_seed)
{
    __disable_irq();
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL = 0u;
    if (systick_ctrl_seed != 0u) {
        SysTick->CTRL = systick_ctrl_seed;
    }

    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    SCB->VTOR = vtor;
    if (enable_irq) {
        __enable_irq();
    }
    __set_MSP(sp);
    ((void (*)(void))entry)();

    while (1) {
        __NOP();
    }
}

static void jump_to_image(uint32_t address) __attribute__((noreturn));

static void jump_to_image(uint32_t address)
{
    jump_to_entry(address,
                  *(const uint32_t *)address,
                  *(const uint32_t *)(address + 4u),
                  1,
                  0u);
}

int main(void)
{
    int enter_recovery = 0;

    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFu << 4)) | (0x3u << 4);
    GPIOC->scr = (1u << 9);

    if (recovery_combo_pressed()) {
        enter_recovery = 1;
    }

    if (!enter_recovery && stock_tail_valid()) {
        /* Stock does not call OpenScope boot_validate(), and stock warm
         * power-key paths can preserve SRAM.  Do not let old OpenScope
         * recovery/failure words trap a valid stock image in HID recovery after
         * the user turns stock off and back on; POWER+PRM remains the explicit
         * recovery override. */
        *DFU_RAM_MAGIC_ADDR = 0u;
        *BOOT_COUNTER_ADDR = 0u;
        seed_stock_allocator_descriptor();
        jump_to_entry(STOCK_APP_ADDRESS,
                      *(const uint32_t *)STOCK_APP_ADDRESS,
                      *(const uint32_t *)(STOCK_APP_ADDRESS + 4u),
                      1,
                      4u);
    }

    if (!enter_recovery && *DFU_RAM_MAGIC_ADDR == DFU_RAM_MAGIC_VALUE) {
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
        *DFU_RAM_MAGIC_ADDR = DFU_RAM_MAGIC_VALUE;
        __DSB();
        jump_to_image(HIGH_BOOTLOADER_ADDRESS);
    }

    while (1) {
        __NOP();
    }
}
