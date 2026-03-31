/*
 * One-shot EOPB0 configuration to enable 224KB SRAM on AT32F403A
 * Flash this, let it run once, then flash the real firmware.
 */
#include "at32f403a_407.h"

extern void system_clock_config(void);

int main(void) {
    /* Power hold - PC9 */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xF << 4)) | (0x3 << 4);
    GPIOC->scr = (1 << 9);

    /* Backlight - PB8 */
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xF << 0)) | (0x3 << 0);
    GPIOB->scr = (1 << 8);

    /* Don't init PLL - run on internal 8MHz to be safe for flash ops */

    /* Check if EOPB0 already set */
    if (USD->eopb0 == 0x00FE) {
        /* Already configured - just spin with green screen or something */
        while(1);
    }

    /* Unlock flash */
    flash_unlock();

    /* Unlock USD (user system data / option bytes) */
    FLASH->usd_unlock = FLASH_UNLOCK_KEY1;
    FLASH->usd_unlock = FLASH_UNLOCK_KEY2;
    while(FLASH->ctrl_bit.usdulks == RESET);

    /* Erase USD (preserves FAP if already set) */
    flash_user_system_data_erase();

    /* Program EOPB0 = 0xFE (224KB SRAM mode) */
    FLASH->ctrl_bit.usdprgm = TRUE;
    USD->eopb0 = 0x00FE;
    /* Wait for completion */
    while(FLASH->sts_bit.obf);
    FLASH->ctrl_bit.usdprgm = FALSE;

    /* Reset to apply */
    nvic_system_reset();

    while(1);
}
