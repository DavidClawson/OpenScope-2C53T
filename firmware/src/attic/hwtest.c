/*
 * Hardware test for AT32F403A on FNIRSI 2C53T (board V1.4)
 *
 * Confirmed pins:
 *   PC9  = Power hold (HIGH to keep device on)
 *   PB8  = LCD backlight (HIGH to enable)
 *   EXMC = LCD ST7789 parallel interface
 *
 * Uses AT32 HAL for clock init, bare registers for GPIO/EXMC/LCD
 */

#include "at32f403a_407.h"

/* LCD memory-mapped addresses (A17 selects command vs data) */
#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

/* EXMC registers */
#define EXMC_SNCTL0   (*(volatile uint32_t *)0xA0000000)
#define EXMC_SNTCFG0  (*(volatile uint32_t *)0xA0000004)
#define EXMC_SNWTCFG0 (*(volatile uint32_t *)0xA0000104)

/* Simple delay */
static void delay_ms(uint32_t ms) {
    /* Rough delay - works at any clock speed */
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;
        while (count--) __asm volatile("nop");
    }
}

/* Configure one GPIO pin: mode and config nibble */
static void gpio_pin_config(uint32_t base, uint8_t pin, uint8_t mode, uint8_t cnf) {
    volatile uint32_t *reg = (pin < 8) ?
        (volatile uint32_t *)(base + 0x00) :
        (volatile uint32_t *)(base + 0x04);
    uint8_t pos = (pin < 8) ? pin : (pin - 8);
    uint32_t val = *reg;
    val &= ~(0xF << (pos * 4));
    val |= ((mode | (cnf << 2)) << (pos * 4));
    *reg = val;
}

static void power_hold_init(void) {
    /* PC9 = push-pull output, set HIGH immediately */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_config(GPIOC_BASE, 9, 3, 0);  /* Push-pull 50MHz */
    GPIOC->scr = (1 << 9);  /* Set PC9 HIGH */
}

static void backlight_init(void) {
    /* PB8 = push-pull output, set HIGH */
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    gpio_pin_config(GPIOB_BASE, 8, 3, 0);  /* Push-pull 50MHz */
    GPIOB->scr = (1 << 8);  /* Set PB8 HIGH */
}

static void exmc_lcd_init(void) {
    /* Enable GPIOD, GPIOE, EXMC, and IOMUX (AFIO) clocks */
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);

    /* Configure EXMC pins as AF push-pull (50MHz) */
    /* PD0,1 = D2,D3 */
    gpio_pin_config(GPIOD_BASE, 0, 3, 2);
    gpio_pin_config(GPIOD_BASE, 1, 3, 2);
    /* PD4 = NOE, PD5 = NWE */
    gpio_pin_config(GPIOD_BASE, 4, 3, 2);
    gpio_pin_config(GPIOD_BASE, 5, 3, 2);
    /* PD7 = NE1 (chip select) */
    gpio_pin_config(GPIOD_BASE, 7, 3, 2);
    /* PD8,9,10 = D13,D14,D15 */
    gpio_pin_config(GPIOD_BASE, 8, 3, 2);
    gpio_pin_config(GPIOD_BASE, 9, 3, 2);
    gpio_pin_config(GPIOD_BASE, 10, 3, 2);
    /* PD11 = A16, PD12 = A17 (RS/DCX) */
    gpio_pin_config(GPIOD_BASE, 11, 3, 2);
    gpio_pin_config(GPIOD_BASE, 12, 3, 2);
    /* PD14 = D0, PD15 = D1 */
    gpio_pin_config(GPIOD_BASE, 14, 3, 2);
    gpio_pin_config(GPIOD_BASE, 15, 3, 2);

    /* PE7-15 = D4-D12 */
    for (int i = 7; i <= 15; i++) {
        gpio_pin_config(GPIOE_BASE, i, 3, 2);
    }

    /* Configure EXMC Bank 0 — original timing works at 240MHz */
    EXMC_SNCTL0  = 0x00005010;  /* 16-bit, write enable, extended mode */
    EXMC_SNTCFG0 = 0x02020424;  /* Read timing */
    EXMC_SNWTCFG0 = 0x00000202; /* Write timing */
    EXMC_SNCTL0 |= 0x0001;      /* Enable bank */
}

static void lcd_delay(void) {
    volatile uint32_t i = 100;
    while (i--) __asm volatile("nop");
}

static void lcd_cmd(uint8_t cmd) {
    LCD_CMD = cmd;
    lcd_delay();
}

static void lcd_data(uint8_t data) {
    LCD_DATA = data;
    lcd_delay();
}

static void lcd_data16(uint16_t data) {
    LCD_DATA = data;
}

static void lcd_init(void) {
    /* Software reset */
    lcd_cmd(0x01);
    delay_ms(200);

    /* Sleep out */
    lcd_cmd(0x11);
    delay_ms(200);

    /* Memory access control — landscape mode */
    lcd_cmd(0x36);
    delay_ms(1);
    lcd_data(0x60);  /* Row/Col exchange + Col order = landscape */
    delay_ms(10);

    /* Pixel format: 16-bit RGB565 */
    lcd_cmd(0x3A);
    delay_ms(1);
    lcd_data(0x55);
    delay_ms(10);

    /* Display on */
    lcd_cmd(0x29);
    delay_ms(50);
}

static void lcd_fill(uint16_t color) {
    /* Set window to full screen 320x240 */
    lcd_cmd(0x2A);
    lcd_data(0x00); lcd_data(0x00);
    lcd_data(0x01); lcd_data(0x3F);

    lcd_cmd(0x2B);
    lcd_data(0x00); lcd_data(0x00);
    lcd_data(0x00); lcd_data(0xEF);

    /* Write pixels */
    lcd_cmd(0x2C);
    for (uint32_t i = 0; i < 320 * 240; i++) {
        lcd_data16(color);
    }
}

/* External: AT32 system clock config from the HAL library */
extern void system_clock_config(void);

int main(void) {
    /* FIRST: Hold power on */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_config(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    /* Clock init to 240MHz */
    system_clock_config();

    /* Backlight on */
    backlight_init();

    /* LCD init */
    exmc_lcd_init();
    lcd_init();

    /* Color cycle test */
    while (1) {
        lcd_fill(0xF800);  /* Red */
        delay_ms(1000);
        lcd_fill(0x07E0);  /* Green */
        delay_ms(1000);
        lcd_fill(0x001F);  /* Blue */
        delay_ms(1000);
        lcd_fill(0xFFFF);  /* White */
        delay_ms(1000);
        lcd_fill(0x0000);  /* Black */
        delay_ms(1000);
    }
}
