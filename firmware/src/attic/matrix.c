/*
 * Full Button Matrix Scanner for FNIRSI 2C53T
 * Temporarily disables EXMC, reconfigures PE7-15 as inputs,
 * scans all row/column combinations, restores EXMC, displays results.
 * Buttons share PE pins with LCD data bus (time-multiplexed).
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"
#include <string.h>

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

/* Row pin definitions — PA, PB, and PC */
typedef struct {
    uint8_t port;  /* 0=A, 1=B, 2=C */
    uint8_t pin;
} row_pin_t;

static const row_pin_t rows[] = {
    /* PA pins — NOW INCLUDING PA2, PA13, PA14, PA15 (JTAG disabled!) */
    {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7},
    {0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 14}, {0, 15},
    /* PB pins — NOW INCLUDING PB2, PB3, PB4 (JTAG disabled!) */
    /* Skip PB6=FPGA CS, PB7=PRM direct, PB8=backlight, PB10/11=mux, PB12-15=SPI/columns */
    {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 9},
    /* PC pins (skip PC9=power hold) */
    {2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5},
    {2, 6}, {2, 7}, {2, 8}, {2, 10}, {2, 11}, {2, 12},
    {2, 13}, {2, 14}, {2, 15},
};
#define NUM_ROWS (sizeof(rows)/sizeof(rows[0]))

static gpio_type* get_port(uint8_t p) {
    if (p == 0) return GPIOA;
    if (p == 1) return GPIOB;
    return GPIOC;
}

/*
 * Exhaustive scan: read ALL GPIO ports as columns.
 * For each row pin driven LOW, read PA, PB, PC, and PE0-6.
 * This catches cross-port AND same-port button connections.
 * col_a/b/c/e store which bits went LOW on each port.
 */
static void full_scan(uint16_t *col_a, uint16_t *col_b, uint16_t *col_c, uint8_t *col_e) {
    /* Configure PE0-PE6 as input pull-up */
    for (int i = 0; i <= 6; i++) {
        gpio_pin_cfg(GPIOE_BASE, i, 0, 2);
        GPIOE->scr = (1 << i);
    }

    volatile int d = 100; while(d--);

    /* Read baseline for all ports */
    uint16_t pa_base = GPIOA->idt & 0xFFFF;
    uint16_t pb_base = GPIOB->idt & 0xFFFF;
    uint16_t pc_base = GPIOC->idt & 0xFFFF;
    uint8_t  pe_base = GPIOE->idt & 0x7F;

    /* Scan each row */
    for (int r = 0; r < (int)NUM_ROWS; r++) {
        gpio_type *port = get_port(rows[r].port);
        uint16_t row_mask = (1 << rows[r].pin);

        /* Drive row LOW */
        gpio_pin_cfg((uint32_t)port, rows[r].pin, 3, 0);
        port->clr = row_mask;

        d = 30; while(d--);

        /* Read all ports */
        uint16_t pa = GPIOA->idt & 0xFFFF;
        uint16_t pb = GPIOB->idt & 0xFFFF;
        uint16_t pc = GPIOC->idt & 0xFFFF;
        uint8_t  pe = GPIOE->idt & 0x7F;

        /* Find bits that went LOW (mask out the driven row pin itself) */
        col_a[r] = (pa_base & ~pa);
        col_b[r] = (pb_base & ~pb);
        col_c[r] = (pc_base & ~pc);
        col_e[r] = (pe_base & ~pe);

        /* Mask out the row pin from its own port result */
        if (rows[r].port == 0) col_a[r] &= ~row_mask;
        else if (rows[r].port == 1) col_b[r] &= ~row_mask;
        else col_c[r] &= ~row_mask;

        /* Restore row to input pull-up */
        gpio_pin_cfg((uint32_t)port, rows[r].pin, 0, 2);
        port->scr = row_mask;
    }
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

    /* CRITICAL: Disable JTAG to free PA13, PA14, PA15, PB3, PB4 for GPIO use */
    gpio_pin_remap_config(SWJTAG_GMUX_100, TRUE);

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

    /* Configure all row pins as input pull-up initially */
    for (int r = 0; r < (int)NUM_ROWS; r++) {
        gpio_type *port = get_port(rows[r].port);
        gpio_pin_cfg((uint32_t)port, rows[r].pin, 0, 2);
        port->scr = (1 << rows[r].pin);
    }

    uint8_t col_results[NUM_ROWS];
    uint8_t display_row = 0;

    while (1) {
        wdt_counter_reload();

        /* Simple scan — no EXMC pause needed */
        memset(col_results, 0, sizeof(col_results));
        simple_scan(col_results);

        /* Display results */
        display_row = 0;
        for (int r = 0; r < (int)NUM_ROWS; r++) {
            if (col_results[r] == 0) continue;

            uint16_t y = 5 + display_row * 18;
            if (y > 220) break;

            /* Row: port letter + pin number (yellow) */
            uint8_t port_id = (rows[r].port == 0) ? 0xAA : (rows[r].port == 1) ? 0xBB : 0xCC;
            lcd_hex8(5, y, port_id, 0xFFE0, 0x0008, 2);
            lcd_hex8(30, y, rows[r].pin, 0xFFE0, 0x0008, 2);

            /* PE column hits (red) — low 7 bits */
            uint8_t pe_hits = col_results[r] & 0x7F;
            if (pe_hits) {
                lcd_hex8(65, y, 0xEE, 0xF800, 0x0008, 2);
                lcd_hex8(85, y, pe_hits, 0xF800, 0x0008, 2);
                /* Show first PE hit bit */
                for (int i = 0; i < 7; i++) {
                    if (pe_hits & (1 << i)) {
                        lcd_hex8(120, y, 0xE0 | i, 0xFFFF, 0xF800, 2);
                        break;
                    }
                }
            }

            /* PB column hits (green) — high 4 bits = PB12-15 */
            uint8_t pb_hits = (col_results[r] >> 4) & 0xF;
            if (pb_hits) {
                lcd_hex8(160, y, 0xBB, 0x07E0, 0x0008, 2);
                lcd_hex8(180, y, pb_hits, 0x07E0, 0x0008, 2);
                /* Show first PB hit */
                for (int i = 0; i < 4; i++) {
                    if (pb_hits & (1 << i)) {
                        lcd_hex8(215, y, 0xB0 | (i + 12), 0xFFFF, 0x07E0, 2);
                        break;
                    }
                }
            }

            display_row++;
        }

        /* Also check PB7 (PRM direct GPIO) */
        uint8_t pb7 = (GPIOB->idt & (1 << 7)) ? 1 : 0;
        if (pb7) {
            uint16_t y = 5 + display_row * 18;
            if (y <= 220) {
                lcd_hex8(5, y, 0xBB, 0xFFE0, 0x0008, 2);
                lcd_hex8(30, y, 0x07, 0xFFE0, 0x0008, 2);
                display_row++;
            }
        }

        /* Clear remaining rows */
        if (display_row == 0) {
            lcd_fill(5, 5, 310, 18, 0x0008);
        }
        for (int i = display_row; i < 12; i++) {
            lcd_fill(5, 5 + i * 18, 310, 18, 0x0008);
        }

        delay_ms(80);
    }
}
