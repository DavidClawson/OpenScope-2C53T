/*
 * scopediag.c -- Standalone FPGA scope/acquisition diagnostic firmware
 * FNIRSI 2C53T / AT32F403A @ 240MHz
 *
 * Purpose:
 *   Provide a tiny, repeatable bench tool for answering one question:
 *   can we make the FPGA assert PC0 (data ready) and return nontrivial
 *   SPI3 bytes when we vary boot sequencing, H2 upload, scope params,
 *   and command-3 heartbeat behavior?
 *
 * Controls:
 *   UP   - next preset
 *   PRM  - run selected preset
 *   POWER - abort the active run
 *
 * Build:
 *   make -f Makefile.hwtest TEST=scopediag
 *   make -f Makefile.hwtest TEST=scopediag APP_SLOT=1
 *
 * Flash:
 *   make -f Makefile.hwtest TEST=scopediag flash
 *   make -f Makefile.hwtest TEST=scopediag APP_SLOT=1 flash
 */

#include "at32f403a_407.h"
#include "../drivers/fpga_cal_table.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef HWTEST_APP_SLOT
#include "dfu_boot.h"
#endif

extern void system_clock_config(void);

#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

#define FPGA_SPI       ((spi_type *)SPI3_BASE)

#define PB6_MASK       (1U << 6)   /* SPI3 CS */
#define PB11_MASK      (1U << 11)  /* FPGA active mode */
#define PC6_MASK       (1U << 6)   /* FPGA SPI enable */

#define SPI3_CS_ASSERT()    (GPIOB->clr = PB6_MASK)
#define SPI3_CS_DEASSERT()  (GPIOB->scr = PB6_MASK)

#define REGION_A_SIZE       87040u
#define RUN_WINDOW_MS       4000u

#define BG     0x0008
#define WHITE  0xFFFF
#define GREEN  0x07E0
#define CYAN   0x07FF
#define YELLO  0xFFE0
#define RED    0xF800
#define GRAY   0x4208
#define ORNG   0xFBE0

enum {
    H2_NONE = 0,
    H2_REGION_A = 1,
    H2_FULL = 2,
};

typedef enum {
    STATUS_IDLE = 0,
    STATUS_RUNNING,
    STATUS_PC0LOW,
    STATUS_NOLOW,
    STATUS_ABORT,
} diag_status_t;

typedef struct {
    uint8_t scope_params[8];   /* 0x01, 0x0B..0x11 */
    uint8_t run_mode;          /* 0x20 */
    uint8_t sample_depth;      /* 0x21 */
    uint8_t tb_prescaler;      /* 0x26 */
    uint8_t tb_period;         /* 0x27 */
    uint8_t tb_mode;           /* 0x28 */
    uint8_t trig_level_lsb;    /* 0x16 */
    uint8_t trig_level_msb;    /* 0x17 */
    uint8_t trig_mode_edge;    /* 0x18 */
    uint8_t trig_holdoff;      /* 0x19 */
    uint8_t range_params[5];   /* 0x1A..0x1E */
    uint8_t mode4_cfg;         /* 0x1F */
    uint8_t mode5_cfg;         /* 0x25 */
    uint8_t start_param;       /* 0x09 */
} scope_bank_t;

typedef struct {
    const char *name;
    uint8_t h2_mode;
    int8_t bank_index;         /* -1 = no scope config */
    bool heartbeat;
    uint16_t heartbeat_ms;
    bool meter_poll;
    uint16_t meter_poll_ms;
    bool runtime_cfg;
    bool meter_tail;
    bool mode_hint;
    bool trigger_cfg;
    bool range_cfg;
    bool mode4_seq;
    bool mode5_seq;
} preset_t;

typedef struct {
    uint32_t run_count;
    uint32_t tx_count;
    uint32_t rx_bytes;
    uint32_t data_frames;
    uint32_t echo_frames;
    uint32_t heartbeat_count;
    uint32_t pc0_low_samples;
    uint32_t h2_bytes_sent;
    uint8_t last_cmd_param;
    uint8_t last_cmd_code;
    uint8_t hs[8];
    uint8_t last_spi[8];
    uint8_t last_data[12];
    uint8_t last_echo[10];
    diag_status_t status;
} diag_state_t;

static diag_state_t diag_state;

static const scope_bank_t scope_banks[] = {
    /* Bank 0: all zeros (current firmware behavior) */
    {
        .scope_params = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .run_mode = 0x00,
        .sample_depth = 0x00,
        .tb_prescaler = 0x00,
        .tb_period = 0x00,
        .tb_mode = 0x00,
        .trig_level_lsb = 0x00,
        .trig_level_msb = 0x00,
        .trig_mode_edge = 0x00,
        .trig_holdoff = 0x00,
        .range_params = {0x00, 0x00, 0x00, 0x00, 0x00},
        .mode4_cfg = 0x00,
        .mode5_cfg = 0x00,
        .start_param = 0x00,
    },
    /* Bank 1: simple non-zero sweep */
    {
        .scope_params = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        .run_mode = 0x01,
        .sample_depth = 0x01,
        .tb_prescaler = 0x01,
        .tb_period = 0x01,
        .tb_mode = 0x01,
        .trig_level_lsb = 0x01,
        .trig_level_msb = 0x01,
        .trig_mode_edge = 0x01,
        .trig_holdoff = 0x01,
        .range_params = {0x01, 0x01, 0x01, 0x01, 0x01},
        .mode4_cfg = 0x01,
        .mode5_cfg = 0x01,
        .start_param = 0x01,
    },
    /* Bank 2: "basic scope" guess: CH1 on, CH2 off-ish, mid trigger, modest TB */
    {
        .scope_params = {0x01, 0x01, 0x00, 0x03, 0x80, 0x04, 0x02, 0x01},
        .run_mode = 0x03,      /* auto */
        .sample_depth = 0x02,
        .tb_prescaler = 0x04,
        .tb_period = 0x20,
        .tb_mode = 0x01,
        .trig_level_lsb = 0x80,
        .trig_level_msb = 0x00,
        .trig_mode_edge = 0x00,  /* CH1 rising */
        .trig_holdoff = 0x00,
        .range_params = {0x00, 0x00, 0x00, 0x00, 0x00},
        .mode4_cfg = 0x00,
        .mode5_cfg = 0x00,
        .start_param = 0x00,
    },
};

static const preset_t presets[] = {
    { "BASE",  H2_NONE,    -1, false,  0, false,   0, false, false, false, false, false, false, false },
    { "S0",    H2_NONE,     0, false,  0, false,   0, false, false, false, false, false, false, false },
    { "S0HB",  H2_NONE,     0, true,  25, false,   0, false, false, false, false, false, false, false },
    { "S1HB",  H2_NONE,     1, true,  25, false,   0, false, false, false, false, false, false, false },
    { "BASIC", H2_NONE,     2, true,  25, false,   0, true,  false, false, false, false, false, false },
    { "REGA",  H2_REGION_A, 2, true,  25, false,   0, true,  false, false, false, false, false, false },
    { "FULL",  H2_FULL,     2, true,  25, false,   0, true,  false, false, false, false, false, false },
    { "MTAIL", H2_REGION_A, 2, true,  25, false,   0, true,  true,  true,  false, false, false, false },
    { "TRIG",  H2_NONE,     2, true,  25, false,   0, true,  false, false, true,  false, false, false },
    { "RANG",  H2_NONE,     2, true,  25, false,   0, true,  false, false, true,  true,  false, false },
    { "M4TG",  H2_NONE,     2, true,  25, false,   0, false, false, false, true,  true,  true,  false },
    { "M5TG",  H2_NONE,     2, true,  25, false,   0, false, false, false, true,  true,  false, true  },
    { "F5TG",  H2_FULL,     2, true,  25, false,   0, false, true,  true,  true,  true,  false, true  },
    { "MPOL",  H2_FULL,    -1, false,  0, true,  250, false, true,  false, false, true,  false, false },
    { "MPBAS", H2_FULL,     2, false,  0, true,  250, false, true,  false, false, true,  false, false },
};

#define PRESET_COUNT ((uint8_t)(sizeof(presets) / sizeof(presets[0])))

static uint8_t selected_preset = 0;
static uint32_t global_run_counter = 0;

static uint8_t rx_buf[12];
static uint8_t rx_index = 0;
static uint8_t rx_target = 0;

typedef struct {
    bool stable;
    bool last_sample;
    uint8_t count;
} button_state_t;

static button_state_t btn_up;
static button_state_t btn_prm;
static button_state_t btn_power;

static void delay_ms(uint32_t ms)
{
    volatile uint32_t count;
    while (ms--) {
        count = system_core_clock / 10000;
        while (count--) __asm volatile("nop");
    }
}

static void lcd_bus_delay(void)
{
    volatile uint32_t i = 50;
    while (i--) __asm volatile("nop");
}

static void lcd_cmd_wr(uint8_t cmd) { LCD_CMD = cmd; lcd_bus_delay(); }
static void lcd_dat(uint8_t data)   { LCD_DATA = data; lcd_bus_delay(); }
static void lcd_dat16(uint16_t data){ LCD_DATA = data; }

static void gpio_pin_cfg(uint32_t base, uint8_t pin, uint8_t mode, uint8_t cnf)
{
    volatile uint32_t *reg = (pin < 8) ?
        (volatile uint32_t *)(base + 0x00) :
        (volatile uint32_t *)(base + 0x04);
    uint8_t pos = (pin < 8) ? pin : (pin - 8);
    uint32_t val = *reg;
    val &= ~(0xFU << (pos * 4));
    val |= (((mode) | ((cnf) << 2)) << (pos * 4));
    *reg = val;
}

static const uint8_t font_hex[][5] = {
    {0x7C,0x8A,0x92,0xA2,0x7C}, {0x00,0x42,0xFE,0x02,0x00},
    {0x46,0x8A,0x92,0x92,0x62}, {0x44,0x82,0x92,0x92,0x6C},
    {0x18,0x28,0x48,0xFE,0x08}, {0xE4,0xA2,0xA2,0xA2,0x9C},
    {0x3C,0x52,0x92,0x92,0x0C}, {0x80,0x8E,0x90,0xA0,0xC0},
    {0x6C,0x92,0x92,0x92,0x6C}, {0x60,0x92,0x92,0x94,0x78},
    {0x7C,0x90,0x90,0x90,0x7C}, {0xFE,0x92,0x92,0x92,0x6C},
    {0x7C,0x82,0x82,0x82,0x44}, {0xFE,0x82,0x82,0x82,0x7C},
    {0xFE,0x92,0x92,0x92,0x82}, {0xFE,0x90,0x90,0x90,0x80},
};

static const uint8_t glyph_G[5] = {0x3C,0x42,0x52,0x52,0x34};
static const uint8_t glyph_H[5] = {0xFE,0x10,0x10,0x10,0xFE};
static const uint8_t glyph_I[5] = {0x00,0x82,0xFE,0x82,0x00};
static const uint8_t glyph_K[5] = {0xFE,0x10,0x28,0x44,0x82};
static const uint8_t glyph_L[5] = {0xFE,0x02,0x02,0x02,0x02};
static const uint8_t glyph_M[5] = {0xFE,0x40,0x20,0x40,0xFE};
static const uint8_t glyph_N[5] = {0xFE,0x20,0x10,0x08,0xFE};
static const uint8_t glyph_O[5] = {0x7C,0x82,0x82,0x82,0x7C};
static const uint8_t glyph_P[5] = {0xFE,0x90,0x90,0x90,0x60};
static const uint8_t glyph_Q[5] = {0x7C,0x82,0x8A,0x84,0x7A};
static const uint8_t glyph_R[5] = {0xFE,0x90,0x98,0x94,0x62};
static const uint8_t glyph_S[5] = {0x64,0x92,0x92,0x92,0x4C};
static const uint8_t glyph_T[5] = {0x80,0x80,0xFE,0x80,0x80};
static const uint8_t glyph_U[5] = {0xFC,0x02,0x02,0x02,0xFC};
static const uint8_t glyph_V[5] = {0xF8,0x04,0x02,0x04,0xF8};
static const uint8_t glyph_W[5] = {0xFE,0x04,0x18,0x04,0xFE};
static const uint8_t glyph_X[5] = {0xC6,0x28,0x10,0x28,0xC6};
static const uint8_t glyph_Y[5] = {0xE0,0x10,0x0E,0x10,0xE0};

static const uint8_t *glyph_for_char(char ch)
{
    if (ch >= '0' && ch <= '9') return font_hex[ch - '0'];
    if (ch >= 'A' && ch <= 'F') return font_hex[ch - 'A' + 10];
    switch (ch) {
    case 'G': return glyph_G;
    case 'H': return glyph_H;
    case 'I': return glyph_I;
    case 'K': return glyph_K;
    case 'L': return glyph_L;
    case 'M': return glyph_M;
    case 'N': return glyph_N;
    case 'O': return glyph_O;
    case 'P': return glyph_P;
    case 'Q': return glyph_Q;
    case 'R': return glyph_R;
    case 'S': return glyph_S;
    case 'T': return glyph_T;
    case 'U': return glyph_U;
    case 'V': return glyph_V;
    case 'W': return glyph_W;
    case 'X': return glyph_X;
    case 'Y': return glyph_Y;
    default:  return NULL;
    }
}

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    lcd_cmd_wr(0x2A);
    lcd_dat(x >> 8); lcd_dat(x & 0xFF);
    lcd_dat((x + w - 1) >> 8); lcd_dat((x + w - 1) & 0xFF);
    lcd_cmd_wr(0x2B);
    lcd_dat(y >> 8); lcd_dat(y & 0xFF);
    lcd_dat((y + h - 1) >> 8); lcd_dat((y + h - 1) & 0xFF);
    lcd_cmd_wr(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    lcd_set_window(x, y, w, h);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        lcd_dat16(color);
    }
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, const uint8_t *glyph,
                           uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (glyph == NULL) {
        lcd_fill(x, y, 5 * scale, 7 * scale, bg);
        return;
    }

    lcd_set_window(x, y, 5 * scale, 7 * scale);
    for (int row = 0; row < 7; row++) {
        for (int sr = 0; sr < scale; sr++) {
            for (int col = 0; col < 5; col++) {
                for (int sc = 0; sc < scale; sc++) {
                    uint16_t color = (glyph[col] & (1 << (7 - row))) ? fg : bg;
                    lcd_dat16(color);
                }
            }
        }
    }
}

static void lcd_draw_char(uint16_t x, uint16_t y, char ch,
                          uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (ch == ' ') {
        lcd_fill(x, y, 5 * scale, 7 * scale, bg);
        return;
    }
    if (ch == ':') {
        lcd_fill(x, y, 5 * scale, 7 * scale, bg);
        lcd_fill(x + 2 * scale, y + 1 * scale, scale, scale, fg);
        lcd_fill(x + 2 * scale, y + 5 * scale, scale, scale, fg);
        return;
    }
    if (ch == '-') {
        lcd_fill(x, y, 5 * scale, 7 * scale, bg);
        lcd_fill(x, y + 3 * scale, 5 * scale, scale, fg);
        return;
    }
    lcd_draw_glyph(x, y, glyph_for_char(ch), fg, bg, scale);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *s,
                            uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) {
        lcd_draw_char(x, y, *s, fg, bg, scale);
        x += 6 * scale;
        s++;
    }
}

static void lcd_draw_hex8(uint16_t x, uint16_t y, uint8_t val,
                          uint16_t fg, uint16_t bg, uint8_t scale)
{
    lcd_draw_char(x, y, "0123456789ABCDEF"[(val >> 4) & 0xF], fg, bg, scale);
    lcd_draw_char(x + 6 * scale, y, "0123456789ABCDEF"[val & 0xF], fg, bg, scale);
}

static void lcd_draw_hex16(uint16_t x, uint16_t y, uint16_t val,
                           uint16_t fg, uint16_t bg, uint8_t scale)
{
    lcd_draw_hex8(x, y, (uint8_t)(val >> 8), fg, bg, scale);
    lcd_draw_hex8(x + 12 * scale, y, (uint8_t)val, fg, bg, scale);
}

static void lcd_draw_hex32(uint16_t x, uint16_t y, uint32_t val,
                           uint16_t fg, uint16_t bg, uint8_t scale)
{
    lcd_draw_hex8(x, y,       (uint8_t)(val >> 24), fg, bg, scale);
    lcd_draw_hex8(x + 12*scale, y, (uint8_t)(val >> 16), fg, bg, scale);
    lcd_draw_hex8(x + 24*scale, y, (uint8_t)(val >> 8),  fg, bg, scale);
    lcd_draw_hex8(x + 36*scale, y, (uint8_t)val,         fg, bg, scale);
}

static void lcd_init_panel(void)
{
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};

    for (int i = 0; i < 12; i++) {
        gpio_pin_cfg(GPIOD_BASE, pd_pins[i], 3, 2);
    }
    for (int i = 7; i <= 15; i++) {
        gpio_pin_cfg(GPIOE_BASE, i, 3, 2);
    }

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
}

static void board_init(void)
{
    /* Power hold must be first. */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    gpio_pin_cfg(GPIOC_BASE, 9, 3, 0);
    GPIOC->scr = (1U << 9);

    system_clock_config();

#ifdef HWTEST_APP_SLOT
    /* Running as an application behind the HID bootloader. */
    SCB->VTOR = FLASH_BASE | 0x4000;
#endif

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);

    /* Backlight */
    gpio_pin_cfg(GPIOB_BASE, 8, 3, 0);
    GPIOB->scr = (1U << 8);

    lcd_init_panel();

    /* Passive buttons */
    gpio_pin_cfg(GPIOC_BASE, 8, 0, 2);   GPIOC->scr = (1U << 8);   /* POWER pull-up */
    gpio_pin_cfg(GPIOB_BASE, 7, 0, 2);   GPIOB->clr = (1U << 7);   /* PRM pull-down */
    gpio_pin_cfg(GPIOC_BASE, 13, 0, 2);  GPIOC->scr = (1U << 13);  /* UP pull-up */

    /* Match the working fpga.c remap path exactly.
     * On this AT32, forcing SPI3_GMUX_0010 can leave MISO disconnected. */
    {
        uint32_t remap = IOMUX->remap;
        remap &= ~(0x7u << 24);
        remap |= (0x2u << 24);    /* JTAG off, SWD on */
        IOMUX->remap = remap;
    }
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);

    gpio_pin_cfg(GPIOC_BASE, 0, 0, 1);   /* PC0 input floating */
    gpio_pin_cfg(GPIOC_BASE, 6, 3, 0);   /* PC6 output */
    gpio_pin_cfg(GPIOB_BASE, 11, 3, 0);  /* PB11 output */
    gpio_pin_cfg(GPIOB_BASE, 6, 3, 0);   /* PB6 CS output */
    gpio_pin_cfg(GPIOB_BASE, 3, 3, 2);   /* PB3 SCK mux */
    gpio_pin_cfg(GPIOB_BASE, 4, 0, 1);   /* PB4 MISO input floating */
    gpio_pin_cfg(GPIOB_BASE, 5, 3, 2);   /* PB5 MOSI mux */

    GPIOC->scr = PC6_MASK;
    GPIOB->clr = PB11_MASK;
    SPI3_CS_DEASSERT();

    /* USART2 */
    gpio_pin_cfg(GPIOA_BASE, 2, 3, 2);   /* TX AF PP */
    gpio_pin_cfg(GPIOA_BASE, 3, 0, 1);   /* RX input floating */

    USART2->ctrl1 = 0;
    USART2->ctrl2 = 0;
    USART2->ctrl3 = 0;
    USART2->baudr = (system_core_clock / 2) / 9600;
    USART2->ctrl1 = (1 << 13) | (1 << 3) | (1 << 2);

    /* SPI3 mode 3, /2, software NSS */
    FPGA_SPI->ctrl1 = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 8) | (1 << 9);
    FPGA_SPI->ctrl2 = 0x03;
    FPGA_SPI->ctrl1 |= (1 << 6);

    /* Scope-friendly analog defaults from RE notes. */
    gpio_pin_cfg(GPIOC_BASE, 11, 3, 0);  /* meter mux disable */
    gpio_pin_cfg(GPIOC_BASE, 12, 3, 0);
    gpio_pin_cfg(GPIOE_BASE, 4, 3, 0);
    gpio_pin_cfg(GPIOE_BASE, 5, 3, 0);
    gpio_pin_cfg(GPIOE_BASE, 6, 3, 0);
    gpio_pin_cfg(GPIOB_BASE, 9, 3, 0);
    gpio_pin_cfg(GPIOA_BASE, 6, 3, 0);
    gpio_pin_cfg(GPIOA_BASE, 10, 3, 0);
    gpio_pin_cfg(GPIOA_BASE, 15, 3, 0);
    gpio_pin_cfg(GPIOB_BASE, 10, 3, 0);

    GPIOC->clr = (1U << 11);   /* PC11 LOW */
    GPIOC->scr = (1U << 12);   /* PC12 HIGH */
    GPIOE->scr = (1U << 4);    /* PE4 HIGH */
    GPIOE->clr = (1U << 5);    /* PE5 LOW */
    GPIOE->scr = (1U << 6);    /* PE6 HIGH */
    GPIOB->scr = (1U << 9);    /* PB9 HIGH */
    GPIOA->scr = (1U << 6);    /* PA6 HIGH */
    GPIOA->scr = (1U << 10);   /* PA10 HIGH */
    GPIOA->scr = (1U << 15);   /* PA15 HIGH */
    GPIOB->clr = (1U << 10);   /* PB10 LOW */
}

static bool read_up_pressed(void)
{
    return (GPIOC->idt & (1U << 13)) == 0;
}

static bool read_prm_pressed(void)
{
    return (GPIOB->idt & (1U << 7)) != 0;
}

static bool read_power_pressed(void)
{
    return (GPIOC->idt & (1U << 8)) == 0;
}

static bool button_update(button_state_t *state, bool sample)
{
    bool pressed_event = false;

    if (sample == state->last_sample) {
        if (state->count < 3) state->count++;
    } else {
        state->last_sample = sample;
        state->count = 0;
    }

    if (state->count >= 2 && state->stable != sample) {
        state->stable = sample;
        if (sample) pressed_event = true;
    }

    return pressed_event;
}

static void rx_reset(void)
{
    rx_index = 0;
    rx_target = 0;
}

static void parse_rx_byte(uint8_t byte)
{
    diag_state.rx_bytes++;

    if (rx_index == 0) {
        if (byte == 0x5A || byte == 0xAA) {
            rx_buf[0] = byte;
            rx_index = 1;
        }
        return;
    }

    if (rx_index == 1) {
        if ((rx_buf[0] == 0x5A && byte == 0xA5) ||
            (rx_buf[0] == 0xAA && byte == 0x55)) {
            rx_buf[1] = byte;
            rx_target = (rx_buf[0] == 0x5A) ? 12 : 10;
            rx_index = 2;
        } else {
            rx_index = 0;
            rx_target = 0;
        }
        return;
    }

    rx_buf[rx_index++] = byte;
    if (rx_target != 0 && rx_index >= rx_target) {
        if (rx_buf[0] == 0x5A) {
            memcpy(diag_state.last_data, rx_buf, 12);
            diag_state.data_frames++;
        } else {
            memcpy(diag_state.last_echo, rx_buf, 10);
            diag_state.echo_frames++;
        }
        rx_index = 0;
        rx_target = 0;
    }
}

static void drain_usart_rx(void)
{
    while (USART2->sts & USART_RDBF_FLAG) {
        parse_rx_byte((uint8_t)USART2->dt);
    }
}

static void usart2_send_byte(uint8_t b)
{
    volatile uint32_t timeout = 200000;
    while (!(USART2->sts & USART_TDBE_FLAG)) {
        if (--timeout == 0) return;
    }
    USART2->dt = b;
}

static void fpga_send_frame(uint8_t param, uint8_t cmd)
{
    uint8_t frame[10] = {0};

    frame[2] = param;
    frame[3] = cmd;
    frame[9] = (uint8_t)(param + cmd);

    diag_state.last_cmd_param = param;
    diag_state.last_cmd_code = cmd;
    diag_state.tx_count++;

    for (int i = 0; i < 10; i++) {
        usart2_send_byte(frame[i]);
    }

    /* Wait for final byte to fully shift out. */
    {
        volatile uint32_t timeout = 200000;
        while (!(USART2->sts & USART_TDC_FLAG)) {
            if (--timeout == 0) break;
        }
    }
}

static uint8_t spi3_xfer(uint8_t tx)
{
    volatile uint32_t timeout = 200000;

    while (!(FPGA_SPI->sts & SPI_I2S_TDBE_FLAG)) {
        if (--timeout == 0) return 0xEE;
    }
    FPGA_SPI->dt = tx;

    timeout = 200000;
    while (!(FPGA_SPI->sts & SPI_I2S_RDBF_FLAG)) {
        if (--timeout == 0) return 0xEF;
    }
    return (uint8_t)FPGA_SPI->dt;
}

static void idle_ms(uint32_t ms)
{
    while (ms--) {
        drain_usart_rx();
        if (diag_state.status == STATUS_RUNNING &&
            ((GPIOC->idt & (1U << 0)) == 0)) {
            diag_state.pc0_low_samples++;
        }
        delay_ms(1);
    }
}

static void fpga_send_boot_sequence(void)
{
    static const uint8_t boot_cmds[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    idle_ms(100);
    for (unsigned i = 0; i < sizeof(boot_cmds); i++) {
        fpga_send_frame(0x00, boot_cmds[i]);
        idle_ms((boot_cmds[i] == 0x08) ? 100 : 50);
    }
}

static void fpga_do_handshake(void)
{
    SPI3_CS_DEASSERT();
    diag_state.hs[0] = spi3_xfer(0x00);

    SPI3_CS_ASSERT();
    diag_state.hs[1] = spi3_xfer(0x05);
    diag_state.hs[2] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    (void)spi3_xfer(0x00);
    idle_ms(10);

    SPI3_CS_ASSERT();
    diag_state.hs[3] = spi3_xfer(0x12);
    diag_state.hs[4] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    (void)spi3_xfer(0x00);
    idle_ms(5);

    SPI3_CS_ASSERT();
    diag_state.hs[5] = spi3_xfer(0x15);
    diag_state.hs[6] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    diag_state.hs[7] = spi3_xfer(0x00);
    idle_ms(5);
}

static void fpga_upload_h2(uint8_t mode)
{
    uint32_t count = 0;

    if (mode == H2_NONE) {
        diag_state.h2_bytes_sent = 0;
        return;
    }

    count = (mode == H2_REGION_A) ? REGION_A_SIZE : FPGA_H2_CAL_TABLE_SIZE;

    SPI3_CS_ASSERT();
    spi3_xfer(0x3B);
    spi3_xfer(0x00);
    for (uint32_t i = 0; i < count; i++) {
        spi3_xfer(fpga_h2_cal_table[i]);
        diag_state.h2_bytes_sent = i + 1;
    }
    spi3_xfer(0x3A);
    spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    idle_ms(50);
}

static void fpga_send_meter_tail(void)
{
    fpga_send_frame(0x05, 0x08);
    idle_ms(10);
    fpga_send_frame(0x05, 0x09);
    idle_ms(10);
    if (GPIOC->idt & (1U << 7)) {
        fpga_send_frame(0x05, 0x07);
    } else {
        fpga_send_frame(0x05, 0x0A);
    }
    idle_ms(10);
    fpga_send_frame(0x05, 0x14);
    idle_ms(20);
}

static void fpga_send_range_cfg(const scope_bank_t *bank)
{
    static const uint8_t cmds[] = {0x1A, 0x1B, 0x1C, 0x1D, 0x1E};

    for (unsigned i = 0; i < sizeof(cmds); i++) {
        fpga_send_frame(bank->range_params[i], cmds[i]);
        idle_ms(20);
    }
}

static void fpga_send_trigger_cfg(const scope_bank_t *bank)
{
    /* Best current guess for a basic CH1 auto-trigger setup. */
    fpga_send_frame(0x00, 0x07);
    idle_ms(20);
    fpga_send_frame(bank->trig_level_lsb, 0x16);
    idle_ms(20);
    fpga_send_frame(bank->trig_level_msb, 0x17);
    idle_ms(20);
    fpga_send_frame(bank->trig_mode_edge, 0x18);
    idle_ms(20);
    fpga_send_frame(bank->trig_holdoff, 0x19);
    idle_ms(25);
}

static void fpga_send_scope_bank(const scope_bank_t *bank)
{
    static const uint8_t cmds[] = {0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};

    fpga_send_frame(0x00, 0x00);  /* reset */
    idle_ms(25);

    for (unsigned i = 0; i < sizeof(cmds); i++) {
        fpga_send_frame(bank->scope_params[i], cmds[i]);
        idle_ms(25);
    }
}

static void fpga_send_runtime_cfg(const scope_bank_t *bank)
{
    fpga_send_frame(bank->run_mode, 0x20);
    idle_ms(20);
    fpga_send_frame(bank->sample_depth, 0x21);
    idle_ms(20);
    fpga_send_frame(bank->tb_prescaler, 0x26);
    idle_ms(20);
    fpga_send_frame(bank->tb_period, 0x27);
    idle_ms(20);
    fpga_send_frame(bank->tb_mode, 0x28);
    idle_ms(30);
}

static void fpga_send_mode4_seq(const scope_bank_t *bank)
{
    fpga_send_frame(0x00, 0x00);
    idle_ms(20);
    fpga_send_frame(bank->mode4_cfg, 0x1F);
    idle_ms(20);
    fpga_send_frame(bank->start_param, 0x09);
    idle_ms(20);
    fpga_send_frame(bank->run_mode, 0x20);
    idle_ms(20);
    fpga_send_frame(bank->sample_depth, 0x21);
    idle_ms(25);
}

static void fpga_send_mode5_seq(const scope_bank_t *bank)
{
    fpga_send_frame(0x00, 0x00);
    idle_ms(20);
    fpga_send_frame(bank->mode5_cfg, 0x25);
    idle_ms(20);
    fpga_send_frame(bank->start_param, 0x09);
    idle_ms(20);
    fpga_send_frame(bank->tb_prescaler, 0x26);
    idle_ms(20);
    fpga_send_frame(bank->tb_period, 0x27);
    idle_ms(20);
    fpga_send_frame(bank->tb_mode, 0x28);
    idle_ms(25);
}

static void fpga_capture_spi(uint8_t *buf, uint8_t len)
{
    SPI3_CS_ASSERT();
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = spi3_xfer(0x00);
    }
    SPI3_CS_DEASSERT();
}

static const char *status_name(diag_status_t status)
{
    switch (status) {
    case STATUS_IDLE:    return "IDLE";
    case STATUS_RUNNING: return "RUN";
    case STATUS_PC0LOW:  return "PC0LOW";
    case STATUS_NOLOW:   return "NOLOW";
    case STATUS_ABORT:   return "ABORT";
    default:             return "UNK";
    }
}

static void render_screen(void)
{
    const preset_t *preset = &presets[selected_preset];
    const scope_bank_t *bank = (preset->bank_index >= 0) ? &scope_banks[preset->bank_index] : NULL;
    uint16_t y = 4;
    uint16_t status_color = GRAY;

    switch (diag_state.status) {
    case STATUS_RUNNING: status_color = YELLO; break;
    case STATUS_PC0LOW:  status_color = GREEN; break;
    case STATUS_NOLOW:   status_color = RED;   break;
    case STATUS_ABORT:   status_color = ORNG;  break;
    default:             status_color = GRAY;  break;
    }

    lcd_fill(0, 0, 320, 240, BG);

    lcd_draw_string(4, y, "SCOPEDIAG", ORNG, BG, 2);
    y += 18;

    lcd_draw_string(4, y, "P", WHITE, BG, 2);
    lcd_draw_hex8(16, y, selected_preset, CYAN, BG, 2);
    lcd_draw_string(44, y, preset->name, WHITE, BG, 2);
    y += 18;

    lcd_draw_string(4, y, "H2", WHITE, BG, 1);
    lcd_draw_hex8(22, y, preset->h2_mode, CYAN, BG, 1);
    lcd_draw_string(46, y, "HB", WHITE, BG, 1);
    lcd_draw_hex16(64, y, preset->heartbeat_ms, YELLO, BG, 1);
    lcd_draw_string(108, y, "RT", WHITE, BG, 1);
    lcd_draw_hex8(126, y, preset->runtime_cfg ? 1 : 0, CYAN, BG, 1);
    lcd_draw_string(150, y, "MT", WHITE, BG, 1);
    lcd_draw_hex8(168, y, preset->meter_tail ? 1 : 0, CYAN, BG, 1);
    lcd_draw_string(192, y, "MH", WHITE, BG, 1);
    lcd_draw_hex8(210, y, preset->mode_hint ? 1 : 0, CYAN, BG, 1);
    y += 12;

    lcd_draw_string(4, y, "MP", WHITE, BG, 1);
    lcd_draw_hex16(22, y, preset->meter_poll_ms, CYAN, BG, 1);
    y += 12;

    lcd_draw_string(4, y, "UP NEXT PRM RUN", GRAY, BG, 1);
    y += 12;
#ifdef HWTEST_APP_SLOT
    lcd_draw_string(4, y, "PWR+UP BOOT", GRAY, BG, 1);
    y += 12;
#endif
    lcd_draw_string(4, y, "POWER ABORT", GRAY, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "STATUS", WHITE, BG, 1);
    lcd_draw_string(52, y, status_name(diag_state.status), status_color, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "RUN", WHITE, BG, 1);
    lcd_draw_hex16(28, y, (uint16_t)diag_state.run_count, CYAN, BG, 1);
    lcd_draw_string(72, y, "LOW", WHITE, BG, 1);
    lcd_draw_hex16(96, y, (uint16_t)diag_state.pc0_low_samples, GREEN, BG, 1);
    lcd_draw_string(140, y, "HB", WHITE, BG, 1);
    lcd_draw_hex16(158, y, (uint16_t)diag_state.heartbeat_count, YELLO, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "PC0", WHITE, BG, 1);
    lcd_draw_char(28, y, (GPIOC->idt & (1U << 0)) ? 'H' : 'L', (GPIOC->idt & (1U << 0)) ? GRAY : GREEN, BG, 1);
    lcd_draw_string(48, y, "PC6", WHITE, BG, 1);
    lcd_draw_char(72, y, (GPIOC->odt & (1U << 6)) ? 'H' : 'L', CYAN, BG, 1);
    lcd_draw_string(92, y, "PB11", WHITE, BG, 1);
    lcd_draw_char(128, y, (GPIOB->odt & (1U << 11)) ? 'H' : 'L', CYAN, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "TX", WHITE, BG, 1);
    lcd_draw_hex16(22, y, (uint16_t)diag_state.tx_count, CYAN, BG, 1);
    lcd_draw_string(48, y, "RX", WHITE, BG, 1);
    lcd_draw_hex16(66, y, (uint16_t)diag_state.rx_bytes, ORNG, BG, 1);
    lcd_draw_string(108, y, "DF", WHITE, BG, 1);
    lcd_draw_hex16(126, y, (uint16_t)diag_state.data_frames, GREEN, BG, 1);
    lcd_draw_string(168, y, "EF", WHITE, BG, 1);
    lcd_draw_hex16(186, y, (uint16_t)diag_state.echo_frames, YELLO, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "H2", WHITE, BG, 1);
    lcd_draw_hex32(22, y, diag_state.h2_bytes_sent, (diag_state.h2_bytes_sent != 0) ? CYAN : GRAY, BG, 1);
    y += 14;

    lcd_draw_string(4, y, "HS", WHITE, BG, 1);
    for (int i = 0; i < 8; i++) {
        lcd_draw_hex8(22 + i * 18, y, diag_state.hs[i], CYAN, BG, 1);
    }
    y += 14;

    lcd_draw_string(4, y, "SPI", WHITE, BG, 1);
    for (int i = 0; i < 8; i++) {
        lcd_draw_hex8(28 + i * 18, y, diag_state.last_spi[i],
                      (diag_state.last_spi[i] != 0x00 && diag_state.last_spi[i] != 0xFF) ? GREEN : GRAY,
                      BG, 1);
    }
    y += 14;

    lcd_draw_string(4, y, "DAT", WHITE, BG, 1);
    for (int i = 0; i < 8; i++) {
        lcd_draw_hex8(28 + i * 18, y, diag_state.last_data[i], CYAN, BG, 1);
    }
    y += 14;

    lcd_draw_string(4, y, "ECH", WHITE, BG, 1);
    for (int i = 0; i < 8; i++) {
        lcd_draw_hex8(28 + i * 18, y, diag_state.last_echo[i], YELLO, BG, 1);
    }
    y += 14;

    lcd_draw_string(4, y, "CMD", WHITE, BG, 1);
    lcd_draw_hex8(28, y, diag_state.last_cmd_param, CYAN, BG, 1);
    lcd_draw_hex8(46, y, diag_state.last_cmd_code, CYAN, BG, 1);
    if (bank != NULL) {
        for (int i = 0; i < 8; i++) {
            lcd_draw_hex8(82 + i * 18, y, bank->scope_params[i], WHITE, BG, 1);
        }
    } else {
        lcd_draw_string(82, y, "NO CFG", GRAY, BG, 1);
    }
}

static void run_preset(uint8_t preset_index)
{
    const preset_t *preset = &presets[preset_index];
    const scope_bank_t *bank = (preset->bank_index >= 0) ? &scope_banks[preset->bank_index] : NULL;
    uint32_t elapsed = 0;
    uint32_t hb_timer = 0;
    uint32_t meter_poll_timer = 0;
    uint32_t redraw_timer = 0;
    bool saw_pc0_low = false;
    bool prev_pc0_low = false;
    bool aborted = false;

    memset(&diag_state, 0, sizeof(diag_state));
    global_run_counter++;
    diag_state.run_count = global_run_counter;
    diag_state.status = STATUS_RUNNING;
    render_screen();

    /* Reset local parser + drain stale RX bytes. */
    rx_reset();
    while (USART2->sts & USART_RDBF_FLAG) {
        (void)USART2->dt;
    }

    /* Re-establish key control pins. */
    GPIOB->clr = PB11_MASK;
    GPIOC->scr = PC6_MASK;
    SPI3_CS_DEASSERT();
    idle_ms(20);

    fpga_send_boot_sequence();
    fpga_do_handshake();
    fpga_upload_h2(preset->h2_mode);

    if (preset->meter_tail) {
        fpga_send_meter_tail();
    }

    if (bank != NULL && preset->range_cfg) {
        fpga_send_range_cfg(bank);
    }

    GPIOB->scr = PB11_MASK;
    idle_ms(30);

    if (preset->mode_hint) {
        fpga_send_frame(0x05, 0x01);
        idle_ms(20);
    }

    if (bank != NULL) {
        fpga_send_scope_bank(bank);
        if (preset->trigger_cfg) {
            fpga_send_trigger_cfg(bank);
        }
        if (preset->runtime_cfg) {
            fpga_send_runtime_cfg(bank);
        }
        if (preset->mode4_seq) {
            fpga_send_mode4_seq(bank);
        }
        if (preset->mode5_seq) {
            fpga_send_mode5_seq(bank);
        }
    }

    idle_ms(100);

    while (elapsed < RUN_WINDOW_MS) {
        bool pc0_low = ((GPIOC->idt & (1U << 0)) == 0);

        drain_usart_rx();

        if (pc0_low) {
            diag_state.pc0_low_samples++;
            saw_pc0_low = true;
            if (!prev_pc0_low) {
                fpga_capture_spi(diag_state.last_spi, 8);
            }
        }
        prev_pc0_low = pc0_low;

        if (preset->heartbeat) {
            hb_timer++;
            if (hb_timer >= preset->heartbeat_ms) {
                hb_timer = 0;
                fpga_send_frame(0x00, 0x03);
                diag_state.heartbeat_count++;
            }
        }

        if (preset->meter_poll) {
            meter_poll_timer++;
            if (meter_poll_timer >= preset->meter_poll_ms) {
                meter_poll_timer = 0;
                fpga_send_frame(0x00, 0x09);
            }
        }

        redraw_timer++;
        if (redraw_timer >= 100) {
            redraw_timer = 0;
            render_screen();
        }

        if (button_update(&btn_power, read_power_pressed())) {
            aborted = true;
            break;
        }

        delay_ms(1);
        elapsed++;
    }

    drain_usart_rx();
    if (aborted) {
        diag_state.status = STATUS_ABORT;
    } else if (saw_pc0_low || diag_state.pc0_low_samples != 0) {
        diag_state.status = STATUS_PC0LOW;
    } else {
        diag_state.status = STATUS_NOLOW;
    }
    render_screen();
}

int main(void)
{
#ifdef HWTEST_APP_SLOT
    uint8_t boot_combo_ticks = 0;
    bool boot_combo_active = false;
#endif

    board_init();
    memset(&diag_state, 0, sizeof(diag_state));
    diag_state.status = STATUS_IDLE;

#ifdef HWTEST_APP_SLOT
    /* Tell the permanent HID bootloader this app started cleanly. */
    boot_validate();
#endif

    render_screen();

    while (1) {
#ifdef HWTEST_APP_SLOT
        boot_combo_active = read_power_pressed() && read_up_pressed();
        if (boot_combo_active) {
            if (boot_combo_ticks < 20) boot_combo_ticks++;
            if (boot_combo_ticks >= 15) {
                dfu_request_reboot();
            }
        } else {
            boot_combo_ticks = 0;
        }
#endif

#ifdef HWTEST_APP_SLOT
        if (!boot_combo_active && button_update(&btn_up, read_up_pressed())) {
#else
        if (button_update(&btn_up, read_up_pressed())) {
#endif
            selected_preset++;
            if (selected_preset >= PRESET_COUNT) selected_preset = 0;
            memset(&diag_state, 0, sizeof(diag_state));
            diag_state.status = STATUS_IDLE;
            render_screen();
        }

        if (button_update(&btn_prm, read_prm_pressed())) {
            run_preset(selected_preset);
        }

        /* Keep power button state fresh even while idle. */
        (void)button_update(&btn_power, read_power_pressed());

        delay_ms(20);
    }
}
