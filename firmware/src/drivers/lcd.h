/*
 * FNIRSI 2C53T LCD Driver - ST7789V
 *
 * Extracted from firmware V1.2.0 (APP_2C53T_V1.2.0_251015.bin)
 * LCD Controller: ST7789V (positively identified from init sequence)
 * Interface: 16-bit parallel via GD32F307 EXMC (FSMC-compatible)
 * Resolution: 320x240 (landscape mode via MADCTL 0xA0)
 * Color format: RGB565 (16-bit)
 *
 * Address mapping (EXMC Bank 0, NE1):
 *   Command register: 0x6001FFFE  (A17 = 0 -> RS/DCX low = command)
 *   Data register:    0x60020000  (A17 = 1 -> RS/DCX high = data)
 *   RS pin is connected to EXMC address line A17
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/* ========================================================================
 * Display dimensions
 * ======================================================================== */
#define LCD_WIDTH       320
#define LCD_HEIGHT      240

/* ========================================================================
 * EXMC/FSMC memory-mapped LCD registers
 * ======================================================================== */
#define LCD_CMD_ADDR    ((volatile uint16_t *)0x6001FFFE)  /* A17=0: command */
#define LCD_DATA_ADDR   ((volatile uint16_t *)0x60020000)  /* A17=1: data    */

/* ========================================================================
 * EXMC/FSMC peripheral registers (GD32F307 / STM32F1xx compatible)
 * ======================================================================== */
#define EXMC_SNCTL0     (*(volatile uint32_t *)0xA0000000)  /* Bank 0 control */
#define EXMC_SNTCFG0    (*(volatile uint32_t *)0xA0000004)  /* Bank 0 read timing */
#define EXMC_SNWTCFG0   (*(volatile uint32_t *)0xA0000104)  /* Bank 0 write timing */

/* ========================================================================
 * GD32F307 peripheral registers used for LCD GPIO init
 * ======================================================================== */

/* RCU (Reset and Clock Unit) — only define if GD32 HAL isn't present */
#ifndef RCU_APB2EN
#define RCU_APB2EN      (*(volatile uint32_t *)0x40021018)
#endif
#ifndef RCU_AHBEN
#define RCU_AHBEN       (*(volatile uint32_t *)0x40021014)
#endif

/* RCU enable bits */
#ifndef RCU_APB2EN_PDEN
#define RCU_APB2EN_PDEN (1 << 5)   /* GPIOD clock enable */
#endif
#ifndef RCU_APB2EN_PEEN
#define RCU_APB2EN_PEEN (1 << 6)   /* GPIOE clock enable */
#endif
#ifndef RCU_AHBEN_EXMC
#define RCU_AHBEN_EXMC  (1 << 8)   /* EXMC clock enable  */
#endif

/* GPIO port D registers (0x40011400) */
#define GPIOD_CRL       (*(volatile uint32_t *)0x40011400)
#define GPIOD_CRH       (*(volatile uint32_t *)0x40011404)

/* GPIO port E registers (0x40011800) */
#define GPIOE_CRL       (*(volatile uint32_t *)0x40011800)
#define GPIOE_CRH       (*(volatile uint32_t *)0x40011804)

/* GPIO config: AF push-pull output, 50MHz */
#define GPIO_AF_PP_50MHZ  0xB  /* CNF=10 (AF push-pull), MODE=11 (50MHz) */

/* ========================================================================
 * ST7789V command definitions
 * ======================================================================== */
#define ST7789_NOP          0x00
#define ST7789_SWRESET      0x01
#define ST7789_SLPIN        0x10
#define ST7789_SLPOUT       0x11
#define ST7789_PTLON        0x12
#define ST7789_NORON        0x13
#define ST7789_INVOFF       0x20
#define ST7789_INVON        0x21
#define ST7789_DISPOFF      0x28
#define ST7789_DISPON       0x29
#define ST7789_CASET        0x2A  /* Column Address Set */
#define ST7789_RASET        0x2B  /* Row Address Set    */
#define ST7789_RAMWR        0x2C  /* Memory Write       */
#define ST7789_RAMRD        0x2E  /* Memory Read        */
#define ST7789_MADCTL       0x36  /* Memory Access Control */
#define ST7789_COLMOD       0x3A  /* Interface Pixel Format */
#define ST7789_PORCHCTRL    0xB2  /* Porch Control */
#define ST7789_GATECTRL     0xB7  /* Gate Control  */
#define ST7789_VCOMS        0xBB  /* VCOM Setting  */
#define ST7789_LCMCTRL      0xC0  /* LCM Control   */
#define ST7789_VDVVRHEN     0xC2  /* VDV and VRH Command Enable */
#define ST7789_VRHS         0xC3  /* VRH Set       */
#define ST7789_VDVS         0xC4  /* VDV Set       */
#define ST7789_FRCTRL2      0xC6  /* Frame Rate Control 2 */
#define ST7789_PWCTRL1      0xD0  /* Power Control 1 */
#define ST7789_PVGAMCTRL    0xE0  /* Positive Voltage Gamma Control */
#define ST7789_NVGAMCTRL    0xE1  /* Negative Voltage Gamma Control */

/* MADCTL bits */
#define MADCTL_MY   0x80  /* Row Address Order     */
#define MADCTL_MX   0x40  /* Column Address Order  */
#define MADCTL_MV   0x20  /* Row/Column Exchange   */
#define MADCTL_ML   0x10  /* Vertical Refresh Order */
#define MADCTL_RGB  0x00  /* RGB color order       */
#define MADCTL_BGR  0x08  /* BGR color order       */

/* Firmware uses MADCTL = 0xA0 for landscape: MY=1, MV=1 -> 320x240 */
#define LCD_MADCTL_LANDSCAPE  (MADCTL_MY | MADCTL_MV | MADCTL_RGB)  /* 0xA0 */

/* ========================================================================
 * RGB565 color definitions
 *
 * Firmware-extracted colors with their purpose on the 2C53T oscilloscope:
 * ======================================================================== */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0  /* Channel 1 waveform color */
#define COLOR_CYAN      0x07FF  /* Channel 2 waveform color */
#define COLOR_MAGENTA   0xF81F

/* Colors extracted from firmware UI code */
#define COLOR_CH1           0xFFE0  /* Channel 1: Yellow */
#define COLOR_CH2           0x07FF  /* Channel 2: Cyan   */
#define COLOR_TRIGGER       0x07E0  /* Trigger indicator: Green */
#define COLOR_GRID          0x18C3  /* Grid lines: Dark gray */
#define COLOR_GRID_CENTER   0x3186  /* Center crosshair: Lighter gray */
#define COLOR_UI_TEXT       0x055F  /* UI text: Light blue */
#define COLOR_UI_BG         0x6BB0  /* UI background: Gray */
#define COLOR_DARK_GRAY     0x2104  /* Status bars, backgrounds */
#define COLOR_GRAY          0x8410  /* Secondary text */
#define COLOR_SELECTED_BG   0x2945  /* Selected menu item background */
#define COLOR_ORANGE        0xFCA0  /* Highlight/active item */

/* ========================================================================
 * RGB565 color helper macro
 * ======================================================================== */
#define RGB565(r, g, b) ((uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | ((b) >> 3)))

/* ========================================================================
 * Legacy 8x16 font (still used by lcd_draw_char / lcd_draw_string)
 *
 * For the new multi-size variable-width font system, see font.h
 * ======================================================================== */
#define FONT_WIDTH      8
#define FONT_HEIGHT     16

/* ========================================================================
 * Function prototypes
 * ======================================================================== */

/* Low-level initialization */
void lcd_gpio_init(void);       /* Configure EXMC GPIO pins (PD, PE) */
void lcd_fsmc_init(void);       /* Configure EXMC/FSMC bus timing    */

/* LCD register access (memory-mapped via EXMC) */
void lcd_write_cmd(uint8_t cmd);
void lcd_write_data(uint16_t data);
void lcd_write_data8(uint8_t data);
uint16_t lcd_read_data(void);

/* LCD controller initialization */
void lcd_init(void);            /* Full init: GPIO + FSMC + ST7789 sequence */
void lcd_reset(void);           /* Hardware reset via GPIO (if RST pin wired) */

/* Drawing window control */
void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/* Drawing primitives */
void lcd_set_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_clear(uint16_t color);

/* Text rendering (using embedded 8x16 font) */
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

/* Display control */
void lcd_set_brightness(uint8_t level);  /* If backlight PWM is available */
void lcd_display_on(void);
void lcd_display_off(void);
void lcd_set_orientation(uint8_t madctl);

/* Delay helper (platform-specific, must be provided) */
extern void delay_ms(uint32_t ms);

#endif /* LCD_H */
