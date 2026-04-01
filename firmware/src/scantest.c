/*
 * Peripheral Scanner for FNIRSI 2C53T
 * Scans ADC channels and I2C buses to find button inputs.
 * Displays all readings on LCD, highlights changes in red.
 *
 * AT32F403A ADC channels:
 *   ADC1 ch0=PA0, ch1=PA1, ch2=PA2, ch3=PA3, ch4=PA4, ch5=PA5, ch6=PA6, ch7=PA7
 *   ADC1 ch8=PB0, ch9=PB1, ch10=PC0, ch11=PC1, ch12=PC2, ch13=PC3, ch14=PC4, ch15=PC5
 *
 * AT32F403A I2C:
 *   I2C1: SCL=PB6, SDA=PB7
 *   I2C2: SCL=PB10, SDA=PB11
 */
#include "at32f403a_407.h"
#include "at32f403a_407_wdt.h"
#include <string.h>

extern void system_clock_config(void);

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

/* UART output on multiple USARTs so at least one hits the debug pads */
static void uart_tx_init(void) {
    /* USART1: PA9=TX (APB2 = 240MHz) */
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOA_BASE, 9, 3, 2);  /* AF push-pull 50MHz */
    USART1->baudr = 240000000 / 9600;
    USART1->ctrl1 = (1 << 3) | (1 << 13);  /* TE + UEN */

    /* USART3: PB10=TX (APB1 = 120MHz) — in case debug pad is here */
    crm_periph_clock_enable(CRM_USART3_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOB_BASE, 10, 3, 2);  /* AF push-pull 50MHz */
    USART3->baudr = 120000000 / 9600;
    USART3->ctrl1 = (1 << 3) | (1 << 13);  /* TE + UEN */
}

static void uart_putc(char c) {
    /* Send on both USART1 and USART3 */
    while (!(USART1->sts & (1 << 7))) {}  /* Wait TDE */
    USART1->dt = c;
    while (!(USART3->sts & (1 << 7))) {}
    USART3->dt = c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void uart_hex8(uint8_t val) {
    uart_putc("0123456789ABCDEF"[(val >> 4) & 0xF]);
    uart_putc("0123456789ABCDEF"[val & 0xF]);
}

static void uart_hex12(uint16_t val) {
    uart_putc("0123456789ABCDEF"[(val >> 8) & 0xF]);
    uart_putc("0123456789ABCDEF"[(val >> 4) & 0xF]);
    uart_putc("0123456789ABCDEF"[val & 0xF]);
}

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

static void lcd_cmd_wr(uint8_t cmd) {
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

/* Minimal 5x7 font */
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
    lcd_cmd_wr(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x + w - 1) >> 8); lcd_dat((x + w - 1) & 0xFF);
    lcd_cmd_wr(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y + h - 1) >> 8); lcd_dat((y + h - 1) & 0xFF);
    lcd_cmd_wr(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++)
        lcd_dat16(color);
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg, uint8_t scale) {
    lcd_set_window(x, y, 5*scale, 7*scale);
    for (int row = 0; row < 7; row++)
        for (int sr = 0; sr < scale; sr++)
            for (int col = 0; col < 5; col++)
                for (int sc = 0; sc < scale; sc++)
                    lcd_dat16((glyph[col] & (1 << (7-row))) ? fg : bg);
}

static void lcd_draw_char(uint16_t x, uint16_t y, uint8_t ch, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (ch >= '0' && ch <= '9') { lcd_draw_glyph(x, y, font5x7[ch - '0'], fg, bg, scale); return; }
    if (ch >= 'A' && ch <= 'F') { lcd_draw_glyph(x, y, font5x7[ch - 'A' + 10], fg, bg, scale); return; }
    lcd_fill(x, y, 5*scale, 7*scale, bg);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        lcd_draw_char(x, y, *s, fg, bg, scale);
        x += 6 * scale;
        s++;
    }
}

static void lcd_draw_hex12(uint16_t x, uint16_t y, uint16_t val, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[4];
    buf[0] = "0123456789ABCDEF"[(val >> 8) & 0xF];
    buf[1] = "0123456789ABCDEF"[(val >> 4) & 0xF];
    buf[2] = "0123456789ABCDEF"[val & 0xF];
    buf[3] = 0;
    lcd_draw_string(x, y, buf, fg, bg, scale);
}

static void lcd_draw_hex8(uint16_t x, uint16_t y, uint8_t val, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[3];
    buf[0] = "0123456789ABCDEF"[(val >> 4) & 0xF];
    buf[1] = "0123456789ABCDEF"[val & 0xF];
    buf[2] = 0;
    lcd_draw_string(x, y, buf, fg, bg, scale);
}

/* ADC1 registers (AT32 style) */
#define ADC1_BASE_ADDR  0x40012400
#define ADC1_STS    (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x00))  /* Status */
#define ADC1_CTRL1  (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x04))  /* Control 1 */
#define ADC1_CTRL2  (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x08))  /* Control 2 */
#define ADC1_SPT1   (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x0C))  /* Sample time ch10-17 */
#define ADC1_SPT2   (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x10))  /* Sample time ch0-9 */
#define ADC1_OSQ3   (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x34))  /* Regular seq reg 3 */
#define ADC1_OSQ1   (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x2C))  /* Regular seq reg 1 (length) */
#define ADC1_ODT    (*(volatile uint32_t *)(ADC1_BASE_ADDR + 0x4C))  /* Regular data */

static void adc_init(void) {
    /* Enable ADC1 clock (bit 9 of APB2EN) */
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);

    /* Configure ADC pins as analog input (mode=0, cnf=0) */
    /* PA0-PA7 = ADC ch0-7 */
    /* But skip PA2/PA3 (USART2 for FPGA) to avoid conflict */
    gpio_pin_cfg(GPIOA_BASE, 0, 0, 0);  /* ch0 */
    gpio_pin_cfg(GPIOA_BASE, 1, 0, 0);  /* ch1 */
    /* PA2 skip - FPGA TX */
    /* PA3 skip - FPGA RX */
    gpio_pin_cfg(GPIOA_BASE, 4, 0, 0);  /* ch4 */
    gpio_pin_cfg(GPIOA_BASE, 5, 0, 0);  /* ch5 */
    gpio_pin_cfg(GPIOA_BASE, 6, 0, 0);  /* ch6 */
    gpio_pin_cfg(GPIOA_BASE, 7, 0, 0);  /* ch7 */
    /* PB0=ch8, PB1=ch9 */
    gpio_pin_cfg(GPIOB_BASE, 0, 0, 0);  /* ch8 */
    gpio_pin_cfg(GPIOB_BASE, 1, 0, 0);  /* ch9 */
    /* PC0-PC5 = ch10-15 */
    gpio_pin_cfg(GPIOC_BASE, 0, 0, 0);  /* ch10 */
    gpio_pin_cfg(GPIOC_BASE, 1, 0, 0);  /* ch11 */
    gpio_pin_cfg(GPIOC_BASE, 2, 0, 0);  /* ch12 */
    gpio_pin_cfg(GPIOC_BASE, 3, 0, 0);  /* ch13 */
    gpio_pin_cfg(GPIOC_BASE, 4, 0, 0);  /* ch14 */
    gpio_pin_cfg(GPIOC_BASE, 5, 0, 0);  /* ch15 */

    /* ADC prescaler: PCLK2/8 (240MHz/8 = 30MHz, max is 28MHz but close enough) */
    /* CRM MISC2 register or ADC clock div — use /8 */
    crm_adc_clock_div_set(CRM_ADC_DIV_8);

    /* Reset ADC */
    ADC1_CTRL2 = 0;
    delay_ms(1);

    /* Power on ADC: set ADCEN (bit 0) */
    ADC1_CTRL2 = (1 << 0);
    delay_ms(2);

    /* Set sample time to max (239.5 cycles) for all channels */
    ADC1_SPT1 = 0x00FFFFFF;  /* ch10-17: all 7 = 239.5 cycles */
    ADC1_SPT2 = 0x3FFFFFFF;  /* ch0-9: all 7 = 239.5 cycles */

    /* Single conversion, 1 channel at a time */
    ADC1_OSQ1 = 0;  /* Length = 1 conversion */

    /* Calibrate */
    ADC1_CTRL2 |= (1 << 2);  /* ADCAL */
    while (ADC1_CTRL2 & (1 << 2)) {}  /* Wait for calibration */
}

static uint16_t adc_read(uint8_t channel) {
    /* Set channel in sequence register */
    ADC1_OSQ3 = channel & 0x1F;

    /* Clear end-of-conversion flag */
    ADC1_STS = 0;

    /* Start conversion: REXTTRIG + SWSTRT (software trigger) */
    ADC1_CTRL2 |= (1 << 20) | (7 << 17);  /* EXTTRGR = SWSTRT, EXTRGEN = enable */
    ADC1_CTRL2 |= (1 << 22);  /* OCSWTRG - start conversion */

    /* Wait for conversion complete (CCE flag = bit 1) */
    uint32_t timeout = 100000;
    while (!(ADC1_STS & (1 << 1)) && timeout--) {}

    return ADC1_ODT & 0xFFF;
}

/* I2C scanner — try to address every device 0x01-0x7F */
#define I2C1_BASE_ADDR 0x40005400
#define I2C2_BASE_ADDR 0x40005800

typedef struct {
    volatile uint32_t ctrl1;    /* 0x00 */
    volatile uint32_t ctrl2;    /* 0x04 */
    volatile uint32_t oaddr1;   /* 0x08 */
    volatile uint32_t oaddr2;   /* 0x0C */
    volatile uint32_t dt;       /* 0x10 */
    volatile uint32_t sts1;     /* 0x14 */
    volatile uint32_t sts2;     /* 0x18 */
    volatile uint32_t clkctrl;  /* 0x1C */
    volatile uint32_t tmrise;   /* 0x20 */
} i2c_regs_t;

static uint8_t i2c_scan_addr(i2c_regs_t *i2c, uint8_t addr) {
    /* Generate START */
    i2c->ctrl1 |= (1 << 8);  /* GENSTART */

    uint32_t timeout = 10000;
    while (!(i2c->sts1 & (1 << 0)) && timeout--) {}  /* Wait SB */
    if (!timeout) return 0;

    /* Send address (write) */
    i2c->dt = (addr << 1);

    timeout = 10000;
    while (timeout--) {
        uint32_t sts1 = i2c->sts1;
        if (sts1 & (1 << 1)) {  /* ADDR set = ACK received */
            (void)i2c->sts2;  /* Clear ADDR by reading sts2 */
            i2c->ctrl1 |= (1 << 9);  /* GENSTOP */
            return 1;  /* Device found! */
        }
        if (sts1 & (1 << 10)) {  /* ACKFAIL = NACK */
            i2c->sts1 &= ~(1 << 10);  /* Clear AF */
            i2c->ctrl1 |= (1 << 9);  /* GENSTOP */
            return 0;
        }
    }
    i2c->ctrl1 |= (1 << 9);  /* GENSTOP */
    return 0;
}

static void my_i2c_init(i2c_regs_t *i2c) {
    /* Reset */
    i2c->ctrl1 = (1 << 15);  /* RESET */
    i2c->ctrl1 = 0;

    /* APB1 clock = 120MHz, set FREQ field */
    i2c->ctrl2 = 120;  /* APB1 freq in MHz */

    /* 100kHz standard mode: CCR = APB1 / (2 * 100000) = 600 */
    i2c->clkctrl = 600;

    /* Rise time: 1000ns / (1/120MHz) + 1 = 121 */
    i2c->tmrise = 121;

    /* Enable */
    i2c->ctrl1 = (1 << 0);  /* I2CEN */
}

int main(void) {
    /* Power hold */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1 << 9);

    /* Clock - 240MHz */
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

    /* Watchdog — 4 second timeout */
    wdt_register_write_enable(TRUE);
    wdt_divider_set(WDT_CLK_DIV_64);
    wdt_reload_value_set(1875);
    wdt_counter_reload();
    wdt_enable();

    /* Clear screen */
    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Title */
    lcd_draw_string(10, 2, "ADC SCAN", 0xFFFF, 0x0008, 2);

    /* ADC channel labels - 2 columns of 8 */
    uint16_t y_start = 20;
    uint8_t row_h = 14;
    const char *ch_names[] = {
        "0 A0", "1 A1", "2   ", "3   ",
        "4 A4", "5 A5", "6 A6", "7 A7",
        "8 B0", "9 B1", "A C0", "B C1",
        "C C2", "D C3", "E C4", "F C5"
    };
    for (int i = 0; i < 16; i++) {
        int col = i / 8;
        int row = i % 8;
        uint16_t x = 5 + col * 160;
        uint16_t y = y_start + row * row_h;
        lcd_draw_string(x, y, ch_names[i], 0x8410, 0x0008, 1);
    }

    /* I2C section at bottom */
    uint16_t iy = y_start + 8 * row_h + 4;
    lcd_draw_string(5, iy, "I2C1", 0x07E0, 0x0008, 2);
    lcd_draw_string(5, iy + 18, "I2C2", 0x07FF, 0x0008, 2);

    /* Init UART output */
    uart_tx_init();
    uart_puts("\r\n=== SCANTEST ===\r\n");

    /* Init ADC */
    adc_init();

    /* Init I2C1 and I2C2 */
    crm_periph_clock_enable(CRM_I2C1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_I2C2_PERIPH_CLOCK, TRUE);

    /* I2C1: PB6=SCL, PB7=SDA — open drain AF */
    gpio_pin_cfg(GPIOB_BASE, 6, 3, 3);  /* AF open-drain, 50MHz */
    gpio_pin_cfg(GPIOB_BASE, 7, 3, 3);

    /* I2C2: PB10=SCL, PB11=SDA — open drain AF */
    gpio_pin_cfg(GPIOB_BASE, 10, 3, 3);
    gpio_pin_cfg(GPIOB_BASE, 11, 3, 3);

    i2c_regs_t *i2c1 = (i2c_regs_t *)I2C1_BASE_ADDR;
    i2c_regs_t *i2c2 = (i2c_regs_t *)I2C2_BASE_ADDR;
    my_i2c_init(i2c1);
    my_i2c_init(i2c2);

    /* Scan I2C once at startup */
    uint8_t i2c1_found[8];
    uint8_t i2c2_found[8];
    uint8_t i2c1_count = 0, i2c2_count = 0;
    memset(i2c1_found, 0, sizeof(i2c1_found));
    memset(i2c2_found, 0, sizeof(i2c2_found));

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        wdt_counter_reload();
        if (i2c_scan_addr(i2c1, addr) && i2c1_count < 8) {
            i2c1_found[i2c1_count++] = addr;
        }
    }
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        wdt_counter_reload();
        if (i2c_scan_addr(i2c2, addr) && i2c2_count < 8) {
            i2c2_found[i2c2_count++] = addr;
        }
    }

    /* Display I2C results */
    uart_puts("I2C1:");
    for (int i = 0; i < i2c1_count; i++) {
        lcd_draw_hex8(60 + i * 20, iy, i2c1_found[i], 0x07E0, 0x0008, 2);
        uart_putc(' '); uart_hex8(i2c1_found[i]);
    }
    if (i2c1_count == 0) {
        lcd_draw_string(60, iy, "----", 0x4208, 0x0008, 2);
        uart_puts(" none");
    }
    uart_puts("\r\n");

    uart_puts("I2C2:");
    for (int i = 0; i < i2c2_count; i++) {
        lcd_draw_hex8(60 + i * 20, iy + 18, i2c2_found[i], 0x07FF, 0x0008, 2);
        uart_putc(' '); uart_hex8(i2c2_found[i]);
    }
    if (i2c2_count == 0) {
        lcd_draw_string(60, iy + 18, "----", 0x4208, 0x0008, 2);
        uart_puts(" none");
    }
    uart_puts("\r\n\r\n");

    /* Store previous ADC values */
    uint16_t prev_adc[16];
    memset(prev_adc, 0xFF, sizeof(prev_adc));

    /* Skip channels 2,3 (FPGA USART) */
    uint8_t scan_channels[] = {0,1,4,5,6,7,8,9,10,11,12,13,14,15};
    int num_channels = sizeof(scan_channels);

    uint32_t loop = 0;
    while (1) {
        wdt_counter_reload();

        uint8_t any_changed = 0;

        /* Read all ADC channels */
        for (int i = 0; i < num_channels; i++) {
            uint8_t ch = scan_channels[i];
            uint16_t val = adc_read(ch);

            /* Only redraw if changed significantly (noise filter: ±16) */
            int diff = (int)val - (int)prev_adc[ch];
            if (diff < 0) diff = -diff;
            if (diff > 16 || prev_adc[ch] == 0xFFFF) {
                int col = ch / 8;
                int row = ch % 8;
                uint16_t x = 35 + col * 160;
                uint16_t y = y_start + row * row_h;
                uint16_t color = (prev_adc[ch] != 0xFFFF) ? 0xF800 : 0xFFFF;  /* Red if changed */
                lcd_draw_hex12(x, y, val, color, 0x0008, 1);

                /* Also draw a tiny bar graph */
                uint16_t bar_w = (val * 80) / 4096;
                lcd_fill(x + 22, y, 80, row_h - 2, 0x0008);  /* Clear bar area */
                if (bar_w > 0)
                    lcd_fill(x + 22, y, bar_w, row_h - 2, color);

                if (prev_adc[ch] != 0xFFFF) any_changed = 1;
                prev_adc[ch] = val;
            }
        }

        /* Output via UART every 10th loop or when something changes */
        if (any_changed || (loop % 20) == 0) {
            uart_puts("ADC:");
            for (int i = 0; i < num_channels; i++) {
                uint8_t ch = scan_channels[i];
                uart_putc(' ');
                uart_hex12(prev_adc[ch]);
            }
            uart_puts("\r\n");
        }

        loop++;
        delay_ms(50);
    }
}
