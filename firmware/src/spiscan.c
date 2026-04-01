/*
 * SPI2 Scanner for FNIRSI 2C53T
 * Probes SPI2 (FPGA bulk data channel) to look for button state.
 * PB13=CLK, PB14=MISO (FPGA→MCU), PB15=MOSI (MCU→FPGA), PB6=CS (GPIO)
 *
 * From decompiled firmware:
 *   CS assert: GPIOB->BOP = 0x40 (set PB6)
 *   Wait SPI ready: while(!(SPI2_STAT & 0x01))
 *   Write/read: SPI2_DATA
 *   Wait done: while(SPI2_STAT & 0x02)
 *
 * Also monitors PB7 (PRM button / possible interrupt line) and PE2 (right arrow).
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"
#include <string.h>

extern void system_clock_config(void);

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

/* SPI2 registers */
#define SPI2_BASE_ADDR  0x40003800
#define SPI2_CTRL1  (*(volatile uint32_t *)(SPI2_BASE_ADDR + 0x00))
#define SPI2_CTRL2  (*(volatile uint32_t *)(SPI2_BASE_ADDR + 0x04))
#define SPI2_STS    (*(volatile uint32_t *)(SPI2_BASE_ADDR + 0x08))
#define SPI2_DT     (*(volatile uint32_t *)(SPI2_BASE_ADDR + 0x0C))

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

/* SPI2 functions */
static void spi2_init(void) {
    /* Enable SPI2 clock */
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);

    /* PB13=CLK (AF push-pull), PB15=MOSI (AF push-pull), PB14=MISO (input floating) */
    gpio_pin_cfg(GPIOB_BASE, 13, 3, 2);  /* CLK: AF push-pull 50MHz */
    gpio_pin_cfg(GPIOB_BASE, 15, 3, 2);  /* MOSI: AF push-pull 50MHz */
    gpio_pin_cfg(GPIOB_BASE, 14, 0, 1);  /* MISO: input floating */

    /* PB6 = CS (GPIO output, active polarity TBD) */
    gpio_pin_cfg(GPIOB_BASE, 6, 3, 0);   /* GPIO push-pull 50MHz */
    GPIOB->scr = (1 << 6);               /* CS high initially (deassert) */

    /* SPI2 config: Master, 8-bit, CLK/256 (slow for initial probing) */
    SPI2_CTRL1 = 0;
    SPI2_CTRL1 = (1 << 2)    /* MSTEN - master mode */
               | (7 << 3)    /* MDIV = /256 (120MHz/256 = ~469kHz) */
               | (0 << 0)    /* CLKPOL = 0 (idle low) */
               | (0 << 1)    /* CLKPHA = 0 (sample on first edge) */
               | (1 << 9)    /* SWCSN internal CS management */
               | (1 << 8);   /* SWCSEN */

    SPI2_CTRL1 |= (1 << 6);  /* SPIEN - enable SPI */
}

static uint8_t spi2_transfer(uint8_t tx) {
    /* Wait until TX buffer empty */
    while (!(SPI2_STS & (1 << 1))) {}  /* Wait TDBE */
    SPI2_DT = tx;
    /* Wait until RX buffer not empty */
    while (!(SPI2_STS & (1 << 0))) {}  /* Wait RDBF */
    return SPI2_DT & 0xFF;
}

static void cs_assert(void) {
    GPIOB->clr = (1 << 6);  /* CS low (try active-low first) */
}

static void cs_deassert(void) {
    GPIOB->scr = (1 << 6);  /* CS high */
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

    /* Init SPI2 */
    spi2_init();

    /* PB7 and PE2 as inputs for button monitoring */
    gpio_pin_cfg(GPIOB_BASE, 7, 0, 2);  /* PB7 input pull-down */
    GPIOB->clr = (1 << 7);
    gpio_pin_cfg(GPIOE_BASE, 2, 0, 2);  /* PE2 input pull-up */
    GPIOE->scr = (1 << 2);

    /* SPI probe: try reading data with CS asserted */
    uint8_t rx_data[32];
    uint8_t prev_data[32];
    memset(prev_data, 0xFF, 32);

    uint32_t loop = 0;
    uint8_t cs_polarity = 0;  /* 0 = active-low, 1 = active-high */

    while (1) {
        wdt_counter_reload();

        /* Every 10 seconds, toggle CS polarity to try both */
        if ((loop % 200) == 100) {
            cs_polarity = !cs_polarity;
        }

        /* Assert CS */
        if (cs_polarity) {
            GPIOB->scr = (1 << 6);  /* CS high (active-high) */
        } else {
            GPIOB->clr = (1 << 6);  /* CS low (active-low) */
        }

        /* Small delay for CS setup */
        volatile int d = 100; while(d--);

        /* Read 32 bytes via SPI (send 0x00 to clock data in) */
        for (int i = 0; i < 32; i++) {
            rx_data[i] = spi2_transfer(0x00);
        }

        /* Deassert CS */
        if (cs_polarity) {
            GPIOB->clr = (1 << 6);
        } else {
            GPIOB->scr = (1 << 6);
        }

        /* Display SPI data — 4 rows of 8 bytes */
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 8; col++) {
                int idx = row * 8 + col;
                uint16_t color = (rx_data[idx] != prev_data[idx]) ? 0xF800 : 0xFFFF;
                if (rx_data[idx] == 0x00) color = 0x4208;  /* Dim zeros */
                if (rx_data[idx] == 0xFF) color = 0x4208;  /* Dim FFs */
                lcd_hex8(5 + col * 38, 5 + row * 22, rx_data[idx], color, 0x0008, 2);
            }
        }
        memcpy(prev_data, rx_data, 32);

        /* Second read with 0xFF as TX data (some devices need this) */
        if (cs_polarity) {
            GPIOB->scr = (1 << 6);
        } else {
            GPIOB->clr = (1 << 6);
        }
        d = 100; while(d--);

        uint8_t rx_data2[16];
        for (int i = 0; i < 16; i++) {
            rx_data2[i] = spi2_transfer(0xFF);
        }

        if (cs_polarity) {
            GPIOB->clr = (1 << 6);
        } else {
            GPIOB->scr = (1 << 6);
        }

        /* Display second read */
        for (int col = 0; col < 8; col++) {
            lcd_hex8(5 + col * 38, 100, rx_data2[col], 0x07FF, 0x0008, 2);
        }
        for (int col = 0; col < 8; col++) {
            lcd_hex8(5 + col * 38, 122, rx_data2[8+col], 0x07FF, 0x0008, 2);
        }

        /* Also try reading with command bytes (from decompiled SPI protocol) */
        if (cs_polarity) {
            GPIOB->scr = (1 << 6);
        } else {
            GPIOB->clr = (1 << 6);
        }
        d = 100; while(d--);

        /* Send a read-like command, then read response */
        uint8_t cmd_byte = (loop / 20) & 0xFF;  /* Cycle through all command bytes */
        spi2_transfer(cmd_byte);
        uint8_t resp[8];
        for (int i = 0; i < 8; i++) resp[i] = spi2_transfer(0x00);

        if (cs_polarity) {
            GPIOB->clr = (1 << 6);
        } else {
            GPIOB->scr = (1 << 6);
        }

        /* Show command + response */
        lcd_hex8(5, 150, cmd_byte, 0x07E0, 0x0008, 2);
        for (int i = 0; i < 8; i++) {
            uint16_t c = (resp[i] == 0x00 || resp[i] == 0xFF) ? 0x4208 : 0xFFFF;
            lcd_hex8(35 + i * 35, 150, resp[i], c, 0x0008, 2);
        }

        /* Bottom: CS polarity, loop count, PB7, PE2 */
        lcd_hex8(5, 200, cs_polarity, 0xFFE0, 0x0008, 2);
        lcd_hex8(40, 200, (loop >> 8) & 0xFF, 0x8410, 0x0008, 2);
        lcd_hex8(65, 200, loop & 0xFF, 0x8410, 0x0008, 2);

        uint8_t pb7 = (GPIOB->idt & (1 << 7)) ? 1 : 0;
        uint8_t pe2 = (GPIOE->idt & (1 << 2)) ? 1 : 0;
        lcd_hex8(120, 200, pb7, pb7 ? 0xF800 : 0x07E0, 0x0008, 2);
        lcd_hex8(160, 200, pe2, pe2 ? 0x07E0 : 0xF800, 0x0008, 2);

        loop++;
        delay_ms(50);
    }
}
