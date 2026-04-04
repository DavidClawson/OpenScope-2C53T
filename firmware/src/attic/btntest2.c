/*
 * Button Test v2 for FNIRSI 2C53T
 * Tests the non-EXMC pins on Port D and Port E that btntest.c skipped.
 * Also monitors known buttons PB7 and PE2.
 * Outputs results via USART2 TX (PA2) at 9600 baud for ESP32 bridge capture.
 *
 * Safe PD pins (not EXMC): PD2, PD3, PD6, PD13
 * Safe PE pins (not EXMC): PE0, PE1, PE2, PE3, PE4, PE5, PE6
 * Known: PB7 = PRM (active-high), PE2 = right arrow (active-low)
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"

extern void system_clock_config(void);

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

static void lcd_cmd_wr(uint8_t cmd) { LCD_CMD = cmd; lcd_bus_delay(); }
static void lcd_dat(uint8_t data) { LCD_DATA = data; lcd_bus_delay(); }
static void lcd_dat16(uint16_t data) { LCD_DATA = data; }

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

static const uint8_t font5x7[][5] = {
    {0x7C,0x8A,0x92,0xA2,0x7C}, {0x00,0x42,0xFE,0x02,0x00},
    {0x46,0x8A,0x92,0x92,0x62}, {0x44,0x82,0x92,0x92,0x6C},
    {0x18,0x28,0x48,0xFE,0x08}, {0xE4,0xA2,0xA2,0xA2,0x9C},
    {0x3C,0x52,0x92,0x92,0x0C}, {0x80,0x8E,0x90,0xA0,0xC0},
    {0x6C,0x92,0x92,0x92,0x6C}, {0x60,0x92,0x92,0x94,0x78},
    {0x7C,0x90,0x90,0x90,0x7C}, {0xFE,0x92,0x92,0x92,0x6C},
    {0x7C,0x82,0x82,0x82,0x44}, {0xFE,0x82,0x82,0x82,0x7C},
    {0xFE,0x92,0x92,0x92,0x82}, {0xFE,0x90,0x90,0x90,0x80},
};

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    lcd_cmd_wr(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x+w-1) >> 8); lcd_dat((x+w-1) & 0xFF);
    lcd_cmd_wr(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y+h-1) >> 8); lcd_dat((y+h-1) & 0xFF);
    lcd_cmd_wr(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) lcd_dat16(color);
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_set_window(x, y, 5*s, 7*s);
    for (int r = 0; r < 7; r++)
        for (int sr = 0; sr < s; sr++)
            for (int c = 0; c < 5; c++)
                for (int sc = 0; sc < s; sc++)
                    lcd_dat16((glyph[c] & (1<<(7-r))) ? fg : bg);
}

static void lcd_hex8(uint16_t x, uint16_t y, uint8_t val, uint16_t fg, uint16_t bg, uint8_t s) {
    lcd_draw_glyph(x, y, font5x7[(val >> 4) & 0xF], fg, bg, s);
    lcd_draw_glyph(x + 6*s, y, font5x7[val & 0xF], fg, bg, s);
}

/* UART output via USART2 TX (PA2) for ESP32 bridge */
static void uart_init(void) {
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);  /* PA2 = AF push-pull TX */
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = (1 << 3) | (1 << 13);  /* TE + UEN */
}

static void uart_putc(char c) {
    while (!(USART2->sts & (1 << 7))) {}
    USART2->dt = c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void uart_hex8(uint8_t val) {
    uart_putc("0123456789ABCDEF"[(val >> 4) & 0xF]);
    uart_putc("0123456789ABCDEF"[val & 0xF]);
}

int main(void) {
    /* Power hold */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    system_clock_config();

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1 << 8);

    /* LCD init */
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

    lcd_cmd_wr(0x01); delay_ms(200);
    lcd_cmd_wr(0x11); delay_ms(200);
    lcd_cmd_wr(0x36); lcd_bus_delay(); lcd_dat(0xA0); delay_ms(10);
    lcd_cmd_wr(0x3A); lcd_bus_delay(); lcd_dat(0x55); delay_ms(10);
    lcd_cmd_wr(0x29); delay_ms(50);

    /* Watchdog */
    wdt_register_write_enable(TRUE);
    wdt_divider_set(WDT_CLK_DIV_64);
    wdt_reload_value_set(1875);
    wdt_counter_reload();
    wdt_enable();

    lcd_fill(0, 0, 320, 240, 0x0008);

    /* UART init */
    uart_init();
    uart_puts("\r\n=== BTNTEST2 ===\r\n");

    /* Configure ONLY the non-EXMC pins as input pull-up */
    /* PD non-EXMC: 2, 3, 6, 13 */
    gpio_pin_cfg(GPIOD_BASE, 2,  0, 2); GPIOD->scr = (1 << 2);   /* pull-up */
    gpio_pin_cfg(GPIOD_BASE, 3,  0, 2); GPIOD->scr = (1 << 3);
    gpio_pin_cfg(GPIOD_BASE, 6,  0, 2); GPIOD->scr = (1 << 6);
    gpio_pin_cfg(GPIOD_BASE, 13, 0, 2); GPIOD->scr = (1 << 13);

    /* PE non-EXMC: 0, 1, 2, 3, 4, 5, 6 */
    gpio_pin_cfg(GPIOE_BASE, 0, 0, 2); GPIOE->scr = (1 << 0);
    gpio_pin_cfg(GPIOE_BASE, 1, 0, 2); GPIOE->scr = (1 << 1);
    gpio_pin_cfg(GPIOE_BASE, 2, 0, 2); GPIOE->scr = (1 << 2);
    gpio_pin_cfg(GPIOE_BASE, 3, 0, 2); GPIOE->scr = (1 << 3);
    gpio_pin_cfg(GPIOE_BASE, 4, 0, 2); GPIOE->scr = (1 << 4);
    gpio_pin_cfg(GPIOE_BASE, 5, 0, 2); GPIOE->scr = (1 << 5);
    gpio_pin_cfg(GPIOE_BASE, 6, 0, 2); GPIOE->scr = (1 << 6);

    /* PB7 as input pull-down (active-high) */
    gpio_pin_cfg(GPIOB_BASE, 7, 0, 2); GPIOB->clr = (1 << 7);

    /* Also configure remaining PA, PB, PC pins as input pull-up for completeness */
    /* PA: skip PA2 (UART TX), PA13/14 (SWD) */
    for (int i = 0; i <= 15; i++) {
        if (i == 2 || i == 13 || i == 14) continue;
        gpio_pin_cfg(GPIOA_BASE, i, 0, 2);
        GPIOA->scr = (1 << i);  /* pull-up */
    }
    /* PB: skip PB8 (backlight) */
    for (int i = 0; i <= 15; i++) {
        if (i == 7 || i == 8) continue;
        gpio_pin_cfg(GPIOB_BASE, i, 0, 2);
        GPIOB->scr = (1 << i);
    }
    /* PC: skip PC9 (power hold) */
    for (int i = 0; i <= 15; i++) {
        if (i == 9) continue;
        gpio_pin_cfg(GPIOC_BASE, i, 0, 2);
        GPIOC->scr = (1 << i);
    }

    /* Display layout:
     * Row 0: PD non-EXMC (2,3,6,13)
     * Row 1: PE non-EXMC (0-6)
     * Row 2: PA full
     * Row 3: PB full
     * Row 4: PC full
     * Each pin shows as a colored block: green=high, dark=low, red=changed
     */

    /* Pin labels on LCD */
    uint16_t prev_pd = 0xFFFF, prev_pe = 0xFFFF;
    uint16_t prev_pa = 0xFFFF, prev_pb = 0xFFFF, prev_pc = 0xFFFF;

    /* Initial baseline read */
    uint16_t base_pd = GPIOD->idt & 0xFFFF;
    uint16_t base_pe = GPIOE->idt & 0x7F;  /* only bits 0-6 */
    uint16_t base_pa = GPIOA->idt & 0xFFFF;
    uint16_t base_pb = GPIOB->idt & 0xFFFF;
    uint16_t base_pc = GPIOC->idt & 0xFFFF;

    uart_puts("BASE PD:"); uart_hex8(base_pd >> 8); uart_hex8(base_pd); uart_puts("\r\n");
    uart_puts("BASE PE:"); uart_hex8(base_pe >> 8); uart_hex8(base_pe); uart_puts("\r\n");
    uart_puts("BASE PA:"); uart_hex8(base_pa >> 8); uart_hex8(base_pa); uart_puts("\r\n");
    uart_puts("BASE PB:"); uart_hex8(base_pb >> 8); uart_hex8(base_pb); uart_puts("\r\n");
    uart_puts("BASE PC:"); uart_hex8(base_pc >> 8); uart_hex8(base_pc); uart_puts("\r\n\r\n");

    uint32_t loop = 0;
    while (1) {
        wdt_counter_reload();

        uint16_t pd = GPIOD->idt & 0xFFFF;
        uint16_t pe = GPIOE->idt & 0x7F;
        uint16_t pa = GPIOA->idt & 0xFFFF;
        uint16_t pb = GPIOB->idt & 0xFFFF;
        uint16_t pc = GPIOC->idt & 0xFFFF;

        /* Check for ANY change and report via UART */
        if (pd != prev_pd || pe != prev_pe || pa != prev_pa || pb != prev_pb || pc != prev_pc) {
            /* Report which bits changed */
            uint16_t pd_diff = pd ^ prev_pd;
            uint16_t pe_diff = pe ^ prev_pe;
            uint16_t pa_diff = pa ^ prev_pa;
            uint16_t pb_diff = pb ^ prev_pb;
            uint16_t pc_diff = pc ^ prev_pc;

            if (pd_diff) { uart_puts("PD:"); uart_hex8(pd >> 8); uart_hex8(pd); uart_puts(" d:"); uart_hex8(pd_diff >> 8); uart_hex8(pd_diff); uart_puts(" "); }
            if (pe_diff) { uart_puts("PE:"); uart_hex8(pe >> 8); uart_hex8(pe); uart_puts(" d:"); uart_hex8(pe_diff >> 8); uart_hex8(pe_diff); uart_puts(" "); }
            if (pa_diff) { uart_puts("PA:"); uart_hex8(pa >> 8); uart_hex8(pa); uart_puts(" d:"); uart_hex8(pa_diff >> 8); uart_hex8(pa_diff); uart_puts(" "); }
            if (pb_diff) { uart_puts("PB:"); uart_hex8(pb >> 8); uart_hex8(pb); uart_puts(" d:"); uart_hex8(pb_diff >> 8); uart_hex8(pb_diff); uart_puts(" "); }
            if (pc_diff) { uart_puts("PC:"); uart_hex8(pc >> 8); uart_hex8(pc); uart_puts(" d:"); uart_hex8(pc_diff >> 8); uart_hex8(pc_diff); uart_puts(" "); }
            uart_puts("\r\n");
        }

        /* LCD: show PE0-PE6 as big colored blocks */
        for (int i = 0; i <= 6; i++) {
            uint8_t val = (pe >> i) & 1;
            uint8_t changed = ((pe ^ base_pe) >> i) & 1;
            uint16_t color = changed ? 0xF800 : (val ? 0x07E0 : 0x2104);
            lcd_fill(5 + i * 44, 5, 40, 30, color);
            lcd_hex8(15 + i * 44, 12, i, 0xFFFF, color, 2);
        }

        /* PD non-EXMC pins */
        uint8_t pd_safe[] = {2, 3, 6, 13};
        for (int i = 0; i < 4; i++) {
            uint8_t pin = pd_safe[i];
            uint8_t val = (pd >> pin) & 1;
            uint8_t changed = ((pd ^ base_pd) >> pin) & 1;
            uint16_t color = changed ? 0xF800 : (val ? 0x07E0 : 0x2104);
            lcd_fill(5 + i * 78, 40, 74, 25, color);
            lcd_hex8(20 + i * 78, 47, pin, 0xFFFF, color, 2);
        }

        /* PB7 indicator */
        uint8_t pb7 = (pb >> 7) & 1;
        lcd_fill(5, 70, 100, 25, pb7 ? 0xF800 : 0x2104);
        lcd_hex8(10, 77, 0xB7, 0xFFFF, pb7 ? 0xF800 : 0x2104, 2);

        /* Full port hex values */
        uint16_t y = 100;
        lcd_hex8(5,   y,    0xAA, 0xFFE0, 0x0008, 2);  /* PA label */
        lcd_hex8(35,  y,    (pa >> 8) & 0xFF, (pa != prev_pa) ? 0xF800 : 0xFFFF, 0x0008, 2);
        lcd_hex8(60,  y,    pa & 0xFF, (pa != prev_pa) ? 0xF800 : 0xFFFF, 0x0008, 2);

        lcd_hex8(5,   y+20, 0xBB, 0x07E0, 0x0008, 2);  /* PB label */
        lcd_hex8(35,  y+20, (pb >> 8) & 0xFF, (pb != prev_pb) ? 0xF800 : 0xFFFF, 0x0008, 2);
        lcd_hex8(60,  y+20, pb & 0xFF, (pb != prev_pb) ? 0xF800 : 0xFFFF, 0x0008, 2);

        lcd_hex8(5,   y+40, 0xCC, 0x07FF, 0x0008, 2);  /* PC label */
        lcd_hex8(35,  y+40, (pc >> 8) & 0xFF, (pc != prev_pc) ? 0xF800 : 0xFFFF, 0x0008, 2);
        lcd_hex8(60,  y+40, pc & 0xFF, (pc != prev_pc) ? 0xF800 : 0xFFFF, 0x0008, 2);

        lcd_hex8(160, y,    0xDD, 0xF81F, 0x0008, 2);  /* PD label */
        lcd_hex8(190, y,    (pd >> 8) & 0xFF, (pd != prev_pd) ? 0xF800 : 0xFFFF, 0x0008, 2);
        lcd_hex8(215, y,    pd & 0xFF, (pd != prev_pd) ? 0xF800 : 0xFFFF, 0x0008, 2);

        lcd_hex8(160, y+20, 0xEE, 0xFBE0, 0x0008, 2);  /* PE label */
        lcd_hex8(190, y+20, (pe >> 8) & 0xFF, (pe != prev_pe) ? 0xF800 : 0xFFFF, 0x0008, 2);
        lcd_hex8(215, y+20, pe & 0xFF, (pe != prev_pe) ? 0xF800 : 0xFFFF, 0x0008, 2);

        prev_pd = pd;
        prev_pe = pe;
        prev_pa = pa;
        prev_pb = pb;
        prev_pc = pc;

        loop++;
        delay_ms(30);
    }
}
