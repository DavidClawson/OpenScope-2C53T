/*
 * FPGA Command Sender for FNIRSI 2C53T
 * Sends 10-byte command frames on USART2 TX (PA2) at 9600 baud.
 * FPGA responses captured externally via logic analyzer on debug UART pads.
 * LCD shows what we're sending + button press reminder.
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

    /* USART2: TX only on PA2 at 9600 baud */
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);  /* PA2 = AF push-pull TX */
    USART2->baudr = 120000000 / 9600;
    USART2->ctrl1 = (1 << 3) | (1 << 13);  /* TE + UEN */

    /* Wait for FPGA to be ready */
    delay_ms(500);

    /* Send commands every 500ms, cycle through patterns */
    /* Phase 1 (0-9): No TX, just let FPGA send heartbeat (baseline) */
    /* Phase 2 (10-19): Send null commands */
    /* Phase 3 (20-29): Send mode commands */
    uint8_t tx_frame[10];
    uint32_t cmd_num = 0;

    while (1) {
        wdt_counter_reload();

        /* Build frame based on phase */
        for (int i = 0; i < 10; i++) tx_frame[i] = 0;

        if (cmd_num < 10) {
            /* Phase 1: Don't send anything — just baseline */
        } else if (cmd_num < 20) {
            /* Phase 2: Null command */
            tx_frame[9] = 0;  /* checksum = 0+0 */
        } else if (cmd_num < 30) {
            /* Phase 3: Various param_lo values */
            tx_frame[2] = (cmd_num - 20) & 0xFF;
            tx_frame[9] = tx_frame[2] + tx_frame[3];
        } else if (cmd_num < 40) {
            /* Phase 4: Various param_hi values */
            tx_frame[3] = (cmd_num - 30) & 0xFF;
            tx_frame[9] = tx_frame[2] + tx_frame[3];
        } else {
            /* Phase 5: Cycle through common FPGA commands from decompiled firmware */
            uint8_t idx = (cmd_num - 40) % 6;
            uint8_t cmds[][2] = {
                {0x20, 0x01},  /* Normal prefix, mode 1 (scope) */
                {0x20, 0x02},  /* Normal prefix, mode 2 (meter) */
                {0x20, 0x12},  /* Normal prefix, mode 0x12 (siggen?) */
                {0x21, 0x01},  /* Error recovery, mode 1 */
                {0x24, 0x01},  /* Special waveform mode */
                {0x00, 0x00},  /* Null */
            };
            tx_frame[2] = cmds[idx][0];
            tx_frame[3] = cmds[idx][1];
            tx_frame[9] = tx_frame[2] + tx_frame[3];
        }

        /* Display current command on LCD */
        uint16_t y = 10 + (cmd_num % 12) * 18;
        if ((cmd_num % 12) == 0) lcd_fill(0, 10, 320, 220, 0x0008);

        lcd_hex8(5, y, cmd_num & 0xFF, 0x8410, 0x0008, 2);
        for (int i = 0; i < 10; i++)
            lcd_hex8(40 + i * 28, y, tx_frame[i], 0x07E0, 0x0008, 2);

        /* Send (skip phase 1) */
        if (cmd_num >= 10) {
            for (int i = 0; i < 10; i++) {
                while (!(USART2->sts & (1 << 7))) {}
                USART2->dt = tx_frame[i];
            }
            while (!(USART2->sts & (1 << 6))) {}  /* Wait transmit complete */
        }

        cmd_num++;
        delay_ms(500);
    }
}
