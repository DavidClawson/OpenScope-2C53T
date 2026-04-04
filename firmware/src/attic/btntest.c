/*
 * Button mapping test for FNIRSI 2C53T
 * Reads all GPIO input registers and displays them on LCD.
 * Press each button and note which bit changes.
 */
#include "at32f403a_407.h"
#include <stdio.h>
#include <string.h>

extern void system_clock_config(void);

/* LCD addresses */
#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

static void delay_ms(uint32_t ms) {
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;
        while (count--) __asm volatile("nop");
    }
}

static void lcd_bus_delay(void) {
    volatile uint32_t i = 50;
    while (i--) __asm volatile("nop");
}

static void lcd_cmd(uint8_t cmd) {
    LCD_CMD = cmd;
    lcd_bus_delay();
}

static void lcd_dat(uint8_t data) {
    LCD_DATA = data;
    lcd_bus_delay();
}

static void lcd_dat16(uint16_t data) {
    LCD_DATA = data;
}

/* Simple 5x7 font for hex digits */
static const uint8_t font5x7[][5] = {
    {0x7C,0x8A,0x92,0xA2,0x7C}, /* 0 */
    {0x00,0x42,0xFE,0x02,0x00}, /* 1 */
    {0x46,0x8A,0x92,0x92,0x62}, /* 2 */
    {0x44,0x82,0x92,0x92,0x6C}, /* 3 */
    {0x18,0x28,0x48,0xFE,0x08}, /* 4 */
    {0xE4,0xA2,0xA2,0xA2,0x9C}, /* 5 */
    {0x3C,0x52,0x92,0x92,0x0C}, /* 6 */
    {0x80,0x8E,0x90,0xA0,0xC0}, /* 7 */
    {0x6C,0x92,0x92,0x92,0x6C}, /* 8 */
    {0x60,0x92,0x92,0x94,0x78}, /* 9 */
    {0x7C,0x90,0x90,0x90,0x7C}, /* A */
    {0xFE,0x92,0x92,0x92,0x6C}, /* B */
    {0x7C,0x82,0x82,0x82,0x44}, /* C */
    {0xFE,0x82,0x82,0x82,0x7C}, /* D */
    {0xFE,0x92,0x92,0x92,0x82}, /* E */
    {0xFE,0x90,0x90,0x90,0x80}, /* F */
};

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    lcd_cmd(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x + w - 1) >> 8); lcd_dat((x + w - 1) & 0xFF);
    lcd_cmd(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y + h - 1) >> 8); lcd_dat((y + h - 1) & 0xFF);
    lcd_cmd(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++)
        lcd_dat16(color);
}

static void lcd_draw_char(uint16_t x, uint16_t y, uint8_t ch, uint16_t fg, uint16_t bg, uint8_t scale) {
    int idx = -1;
    if (ch >= '0' && ch <= '9') idx = ch - '0';
    else if (ch >= 'A' && ch <= 'F') idx = ch - 'A' + 10;
    else if (ch >= 'a' && ch <= 'f') idx = ch - 'a' + 10;
    else if (ch == ' ') { lcd_fill(x, y, 5*scale, 7*scale, bg); return; }
    else if (ch == ':') {
        lcd_fill(x, y, 5*scale, 7*scale, bg);
        lcd_fill(x+2*scale, y+1*scale, scale, scale, fg);
        lcd_fill(x+2*scale, y+5*scale, scale, scale, fg);
        return;
    }
    else if (ch == 'P') { /* P */
        lcd_fill(x, y, 5*scale, 7*scale, bg);
        lcd_set_window(x, y, 5*scale, 7*scale);
        const uint8_t P[] = {0xFE,0x90,0x90,0x90,0x60};
        for (int row = 0; row < 7; row++)
            for (int sr = 0; sr < scale; sr++)
                for (int col = 0; col < 5; col++)
                    for (int sc = 0; sc < scale; sc++)
                        lcd_dat16((P[col] & (1 << (7-row))) ? fg : bg);
        return;
    }
    else { lcd_fill(x, y, 5*scale, 7*scale, bg); return; }

    lcd_set_window(x, y, 5*scale, 7*scale);
    for (int row = 0; row < 7; row++)
        for (int sr = 0; sr < scale; sr++)
            for (int col = 0; col < 5; col++)
                for (int sc = 0; sc < scale; sc++)
                    lcd_dat16((font5x7[idx][col] & (1 << (7-row))) ? fg : bg);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        lcd_draw_char(x, y, *s, fg, bg, scale);
        x += 6 * scale;
        s++;
    }
}

static void lcd_draw_hex16(uint16_t x, uint16_t y, uint16_t val, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[5];
    buf[0] = "0123456789ABCDEF"[(val >> 12) & 0xF];
    buf[1] = "0123456789ABCDEF"[(val >> 8) & 0xF];
    buf[2] = "0123456789ABCDEF"[(val >> 4) & 0xF];
    buf[3] = "0123456789ABCDEF"[val & 0xF];
    buf[4] = 0;
    lcd_draw_string(x, y, buf, fg, bg, scale);
}

static void gpio_pin_cfg(uint32_t base, uint8_t pin, uint8_t mode, uint8_t cnf) {
    volatile uint32_t *reg = (pin < 8) ?
        (volatile uint32_t *)(base + 0x00) :
        (volatile uint32_t *)(base + 0x04);
    uint8_t pos = (pin < 8) ? pin : (pin - 8);
    uint32_t val = *reg;
    val &= ~(0xFU << (pos * 4));
    val |= (((mode) | ((cnf) << 2)) << (pos * 4));
    *reg = val;
}

int main(void) {
    /* Power hold */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    /* Clock */
    system_clock_config();

    /* Enable all GPIO clocks + IOMUX + XMC */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1 << 8);

    /* Configure port A, B, C as input pull-up.
     * Do NOT touch port D or E — they're EXMC (LCD bus).
     * Do NOT touch PB8 (backlight) or PC9 (power hold).
     * Configure BOTH lower and upper halves of each port. */

    /* Configure as input with PULL-DOWN (active-high buttons).
     * Config = 0x8 (input pull-up/down), then CLR register = pull-down. */

    /* PA0-PA15 as input pull-down */
    GPIOA->cfglr = 0x88888888;
    GPIOA->cfghr = 0x88888888;
    GPIOA->clr = 0xFFFF;  /* Pull-down all */

    /* PB0-PB7 + PB9-PB15 as input pull-down (skip PB8 = backlight) */
    GPIOB->cfglr = 0x88888888;
    GPIOB->cfghr = (GPIOB->cfghr & 0x0000000F) | 0x88888880;
    GPIOB->clr = 0xFEFF;  /* Pull-down all except PB8 */

    /* PC0-PC8 + PC10-PC15 as input pull-down (skip PC9 = power hold) */
    GPIOC->cfglr = 0x88888888;
    GPIOC->cfghr = (GPIOC->cfghr & 0x000000F0) | 0x88888808;
    GPIOC->clr = 0xFDFF;  /* Pull-down all except PC9 */

    /* LCD init - proven sequence */
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
    for (int i = 0; i < 12; i++)
        gpio_pin_cfg(GPIOD_BASE, pd_pins[i], 3, 2);
    for (int i = 7; i <= 15; i++)
        gpio_pin_cfg(GPIOE_BASE, i, 3, 2);

    *(volatile uint32_t *)0xA0000000 = 0x00005010;
    *(volatile uint32_t *)0xA0000004 = 0x02020424;
    *(volatile uint32_t *)0xA0000104 = 0x00000202;
    *(volatile uint32_t *)0xA0000000 |= 0x0001;
    delay_ms(50);

    lcd_cmd(0x01); delay_ms(200);
    lcd_cmd(0x11); delay_ms(200);
    lcd_cmd(0x36); lcd_bus_delay(); lcd_dat(0xA0); delay_ms(10);
    lcd_cmd(0x3A); lcd_bus_delay(); lcd_dat(0x55); delay_ms(10);
    lcd_cmd(0x29); delay_ms(50);

    /* Clear screen - dark blue */
    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Title */
    lcd_draw_string(10, 5, "BUTTON TEST", 0xFFFF, 0x0008, 3);

    /* Labels */
    uint16_t y = 40;
    lcd_draw_string(10, y,      "PA:", 0xFFE0, 0x0008, 2);
    lcd_draw_string(10, y + 25, "PB:", 0x07E0, 0x0008, 2);
    lcd_draw_string(10, y + 50, "PC:", 0x07FF, 0x0008, 2);
    lcd_draw_string(10, y + 75, "PD:", 0xF81F, 0x0008, 2);
    lcd_draw_string(10, y + 100,"PE:", 0xFBE0, 0x0008, 2);

    lcd_draw_string(10, y + 140, "Press buttons", 0xFFFF, 0x0008, 2);

    /* Store previous values to highlight changes */
    uint16_t prev_a = 0xFFFF, prev_b = 0xFFFF, prev_c = 0xFFFF;
    uint16_t prev_d = 0xFFFF, prev_e = 0xFFFF;

    while (1) {
        uint16_t pa = GPIOA->idt & 0xFFFF;
        uint16_t pb = GPIOB->idt & 0xFFFF;
        uint16_t pc = GPIOC->idt & 0xFFFF;
        uint16_t pd = GPIOD->idt & 0xFFFF;
        uint16_t pe = GPIOE->idt & 0xFFFF;

        /* Only redraw if changed */
        if (pa != prev_a) {
            lcd_draw_hex16(50, y, pa, (pa != prev_a) ? 0xF800 : 0xFFE0, 0x0008, 2);
            prev_a = pa;
        }
        if (pb != prev_b) {
            lcd_draw_hex16(50, y + 25, pb, (pb != prev_b) ? 0xF800 : 0x07E0, 0x0008, 2);
            prev_b = pb;
        }
        if (pc != prev_c) {
            lcd_draw_hex16(50, y + 50, pc, (pc != prev_c) ? 0xF800 : 0x07FF, 0x0008, 2);
            prev_c = pc;
        }
        if (pd != prev_d) {
            lcd_draw_hex16(50, y + 75, pd, (pd != prev_d) ? 0xF800 : 0xF81F, 0x0008, 2);
            prev_d = pd;
        }
        if (pe != prev_e) {
            lcd_draw_hex16(50, y + 100, pe, (pe != prev_e) ? 0xF800 : 0xFBE0, 0x0008, 2);
            prev_e = pe;
        }

        delay_ms(50);
    }
}
