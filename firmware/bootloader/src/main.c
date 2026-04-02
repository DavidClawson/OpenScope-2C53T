/*
 * OpenScope 2C53T - USB HID In-Application Bootloader
 *
 * Permanent bootloader at 0x08000000. Never overwritten by user firmware.
 * Application lives at 0x08004000.
 *
 * Boot flow:
 *   1. PC9 HIGH (power hold) — must be first or device shuts off
 *   2. Check RAM magic at 0x20037FE0 → enter USB HID IAP
 *   3. Check flash upgrade flag → jump to app at 0x08004000
 *   4. Otherwise → enter USB HID IAP (no valid app)
 *
 * In bootloader mode:
 *   - LCD shows "Waiting for firmware..." status screen
 *   - POWER button (PC8) long-press boots into app / powers off
 *   - USB HID IAP accepts firmware from host tool
 *   - After successful flash, NVIC_SystemReset → clean boot into app
 */

#include "at32f403a_407.h"
#include "at32f403a_407_clock.h"
#include "usbd_core.h"
#include "hid_iap_class.h"
#include "hid_iap_desc.h"
#include "hid_iap_user.h"
#include "usbd_int.h"

/* RAM magic word: app writes this before NVIC_SystemReset() */
#define DFU_RAM_MAGIC_ADDR   ((volatile uint32_t *)0x20037FE0)
#define DFU_RAM_MAGIC_VALUE  0xDEADBEEF

/* LCD registers via EXMC */
#define LCD_CMD   (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA  (*(volatile uint16_t *)0x60020000)

usbd_core_type usb_core_dev;

/* ═══════════════════════════════════════════════════════════════════
 * Delay
 * ═══════════════════════════════════════════════════════════════════ */

static volatile uint32_t delay_tick;

static void delay_init(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = system_core_clock / 1000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk
                   | SysTick_CTRL_TICKINT_Msk;
}

void SysTick_Handler(void) { if (delay_tick > 0) delay_tick--; }

static void delay_ms(uint32_t ms)
{
    delay_tick = ms;
    while (delay_tick != 0);
}

void usb_delay_ms(uint32_t ms) { delay_ms(ms); }
void usb_delay_us(uint32_t us) { for (volatile uint32_t i = 0; i < us * 30; i++); }

/* ═══════════════════════════════════════════════════════════════════
 * Minimal LCD driver (direct register writes, no dependencies)
 * ═══════════════════════════════════════════════════════════════════ */

static void lcd_write_cmd(uint16_t cmd)  { LCD_CMD = cmd; }
static void lcd_write_data(uint16_t d)   { LCD_DATA = d; }

/* Configure GPIO pin: base=GPIO base addr, pin=0-15, mode=0-3, cnf=0-3 */
#define _GPIO_CFG(base, pin, mode, cnf) do { \
    volatile uint32_t *r = (pin < 8) ? \
        (volatile uint32_t *)(base + 0x00) : \
        (volatile uint32_t *)(base + 0x04); \
    uint8_t p = (pin < 8) ? pin : (pin - 8); \
    uint32_t v = *r; \
    v &= ~(0xFU << (p * 4)); \
    v |= (((mode) | ((cnf) << 2)) << (p * 4)); \
    *r = v; \
} while(0)

static void lcd_init(void)
{
    /* Enable GPIO clocks for EXMC pins */
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_XMC_PERIPH_CLOCK, TRUE);

    /* PD0,1,4,5,7,8,9,10,11,12,14,15 as AF push-pull (50MHz) */
    uint8_t pd_pins[] = {0,1,4,5,7,8,9,10,11,12,14,15};
    for (int i = 0; i < 12; i++)
        _GPIO_CFG(0x40011400, pd_pins[i], 3, 2);
    /* PE7-15 as AF push-pull */
    for (int i = 7; i <= 15; i++)
        _GPIO_CFG(0x40011800, i, 3, 2);

    /* EXMC config — proven working values from app */
    *(volatile uint32_t *)0xA0000000 = 0x00005010;  /* SNCTL0 */
    *(volatile uint32_t *)0xA0000004 = 0x02020424;  /* SNTCFG0 */
    *(volatile uint32_t *)0xA0000104 = 0x00000202;  /* SNWTCFG0 */
    *(volatile uint32_t *)0xA0000000 |= 0x0001;     /* Enable */
    delay_ms(50);

    /* ST7789V init sequence */
    lcd_write_cmd(0x01); delay_ms(200);   /* Software reset */
    lcd_write_cmd(0x11); delay_ms(200);   /* Sleep out */
    lcd_write_cmd(0x36); delay_ms(1);
    lcd_write_data(0xA0); delay_ms(10);   /* MADCTL landscape */
    lcd_write_cmd(0x3A); delay_ms(1);
    lcd_write_data(0x55); delay_ms(10);   /* 16-bit color */
    lcd_write_cmd(0x29); delay_ms(50);    /* Display on */
}

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x2 = x + w - 1, y2 = y + h - 1;
    lcd_write_cmd(0x2A);
    lcd_write_data(x >> 8); lcd_write_data(x & 0xFF);
    lcd_write_data(x2 >> 8); lcd_write_data(x2 & 0xFF);
    lcd_write_cmd(0x2B);
    lcd_write_data(y >> 8); lcd_write_data(y & 0xFF);
    lcd_write_data(y2 >> 8); lcd_write_data(y2 & 0xFF);
    lcd_write_cmd(0x2C);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    lcd_set_window(x, y, w, h);
    uint32_t n = (uint32_t)w * h;
    for (uint32_t i = 0; i < n; i++)
        lcd_write_data(color);
}

/* Tiny 5x7 ASCII font (printable chars 0x20-0x7E) */
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, /*   */ 0x00,0x00,0x5F,0x00,0x00, /* ! */
    0x00,0x07,0x00,0x07,0x00, /* " */ 0x14,0x7F,0x14,0x7F,0x14, /* # */
    0x24,0x2A,0x7F,0x2A,0x12, /* $ */ 0x23,0x13,0x08,0x64,0x62, /* % */
    0x36,0x49,0x56,0x20,0x50, /* & */ 0x00,0x08,0x07,0x03,0x00, /* ' */
    0x00,0x1C,0x22,0x41,0x00, /* ( */ 0x00,0x41,0x22,0x1C,0x00, /* ) */
    0x2A,0x1C,0x7F,0x1C,0x2A, /* * */ 0x08,0x08,0x3E,0x08,0x08, /* + */
    0x00,0x80,0x70,0x30,0x00, /* , */ 0x08,0x08,0x08,0x08,0x08, /* - */
    0x00,0x00,0x60,0x60,0x00, /* . */ 0x20,0x10,0x08,0x04,0x02, /* / */
    0x3E,0x51,0x49,0x45,0x3E, /* 0 */ 0x00,0x42,0x7F,0x40,0x00, /* 1 */
    0x72,0x49,0x49,0x49,0x46, /* 2 */ 0x21,0x41,0x49,0x4D,0x33, /* 3 */
    0x18,0x14,0x12,0x7F,0x10, /* 4 */ 0x27,0x45,0x45,0x45,0x39, /* 5 */
    0x3C,0x4A,0x49,0x49,0x31, /* 6 */ 0x41,0x21,0x11,0x09,0x07, /* 7 */
    0x36,0x49,0x49,0x49,0x36, /* 8 */ 0x46,0x49,0x49,0x29,0x1E, /* 9 */
    0x00,0x00,0x14,0x00,0x00, /* : */ 0x00,0x40,0x34,0x00,0x00, /* ; */
    0x00,0x08,0x14,0x22,0x41, /* < */ 0x14,0x14,0x14,0x14,0x14, /* = */
    0x00,0x41,0x22,0x14,0x08, /* > */ 0x02,0x01,0x59,0x09,0x06, /* ? */
    0x3E,0x41,0x5D,0x59,0x4E, /* @ */ 0x7C,0x12,0x11,0x12,0x7C, /* A */
    0x7F,0x49,0x49,0x49,0x36, /* B */ 0x3E,0x41,0x41,0x41,0x22, /* C */
    0x7F,0x41,0x41,0x41,0x3E, /* D */ 0x7F,0x49,0x49,0x49,0x41, /* E */
    0x7F,0x09,0x09,0x09,0x01, /* F */ 0x3E,0x41,0x41,0x51,0x73, /* G */
    0x7F,0x08,0x08,0x08,0x7F, /* H */ 0x00,0x41,0x7F,0x41,0x00, /* I */
    0x20,0x40,0x41,0x3F,0x01, /* J */ 0x7F,0x08,0x14,0x22,0x41, /* K */
    0x7F,0x40,0x40,0x40,0x40, /* L */ 0x7F,0x02,0x1C,0x02,0x7F, /* M */
    0x7F,0x04,0x08,0x10,0x7F, /* N */ 0x3E,0x41,0x41,0x41,0x3E, /* O */
    0x7F,0x09,0x09,0x09,0x06, /* P */ 0x3E,0x41,0x51,0x21,0x5E, /* Q */
    0x7F,0x09,0x19,0x29,0x46, /* R */ 0x26,0x49,0x49,0x49,0x32, /* S */
    0x03,0x01,0x7F,0x01,0x03, /* T */ 0x3F,0x40,0x40,0x40,0x3F, /* U */
    0x1F,0x20,0x40,0x20,0x1F, /* V */ 0x3F,0x40,0x38,0x40,0x3F, /* W */
    0x63,0x14,0x08,0x14,0x63, /* X */ 0x03,0x04,0x78,0x04,0x03, /* Y */
    0x61,0x59,0x49,0x4D,0x43, /* Z */ 0x00,0x7F,0x41,0x41,0x41, /* [ */
    0x02,0x04,0x08,0x10,0x20, /* \ */ 0x00,0x41,0x41,0x41,0x7F, /* ] */
    0x04,0x02,0x01,0x02,0x04, /* ^ */ 0x40,0x40,0x40,0x40,0x40, /* _ */
    0x00,0x03,0x07,0x08,0x00, /* ` */ 0x20,0x54,0x54,0x78,0x40, /* a */
    0x7F,0x28,0x44,0x44,0x38, /* b */ 0x38,0x44,0x44,0x44,0x28, /* c */
    0x38,0x44,0x44,0x28,0x7F, /* d */ 0x38,0x54,0x54,0x54,0x18, /* e */
    0x00,0x08,0x7E,0x09,0x02, /* f */ 0x18,0xA4,0xA4,0x9C,0x78, /* g */
    0x7F,0x08,0x04,0x04,0x78, /* h */ 0x00,0x44,0x7D,0x40,0x00, /* i */
    0x20,0x40,0x40,0x3D,0x00, /* j */ 0x7F,0x10,0x28,0x44,0x00, /* k */
    0x00,0x41,0x7F,0x40,0x00, /* l */ 0x7C,0x04,0x78,0x04,0x78, /* m */
    0x7C,0x08,0x04,0x04,0x78, /* n */ 0x38,0x44,0x44,0x44,0x38, /* o */
    0xFC,0x18,0x24,0x24,0x18, /* p */ 0x18,0x24,0x24,0x18,0xFC, /* q */
    0x7C,0x08,0x04,0x04,0x08, /* r */ 0x48,0x54,0x54,0x54,0x24, /* s */
    0x04,0x04,0x3F,0x44,0x24, /* t */ 0x3C,0x40,0x40,0x20,0x7C, /* u */
    0x1C,0x20,0x40,0x20,0x1C, /* v */ 0x3C,0x40,0x30,0x40,0x3C, /* w */
    0x44,0x28,0x10,0x28,0x44, /* x */ 0x4C,0x90,0x90,0x90,0x7C, /* y */
    0x44,0x64,0x54,0x4C,0x44, /* z */ 0x00,0x08,0x36,0x41,0x00, /* { */
    0x00,0x00,0x77,0x00,0x00, /* | */ 0x00,0x41,0x36,0x08,0x00, /* } */
    0x02,0x01,0x02,0x04,0x02, /* ~ */
};

static void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = &font5x7[(c - 0x20) * 5];
    lcd_set_window(x, y, 5 * scale, 7 * scale);
    for (int row = 0; row < 7; row++)
        for (int sy = 0; sy < scale; sy++)
            for (int col = 0; col < 5; col++)
                for (int sx = 0; sx < scale; sx++)
                    lcd_write_data((glyph[col] & (1 << row)) ? fg : bg);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *s,
                            uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) {
        lcd_draw_char(x, y, *s++, fg, bg, scale);
        x += 6 * scale;
    }
}

static void lcd_draw_string_center(uint16_t cx, uint16_t y, const char *s,
                                   uint16_t fg, uint16_t bg, uint8_t scale)
{
    int len = 0;
    const char *p = s;
    while (*p++) len++;
    uint16_t x = cx - (len * 6 * scale) / 2;
    lcd_draw_string(x, y, s, fg, bg, scale);
}

/* Draw the bootloader status screen */
static void lcd_draw_bootloader_screen(void)
{
    /* Dark blue background */
    lcd_fill(0, 0, 320, 240, 0x0008);

    /* Title */
    lcd_draw_string_center(160, 30, "OPENSCOPE 2C53T", 0x07FF, 0x0008, 3);

    /* Status */
    lcd_draw_string_center(160, 90, "BOOTLOADER MODE", 0xFFFF, 0x0008, 2);

    lcd_draw_string_center(160, 130, "Waiting for firmware...", 0x07E0, 0x0008, 2);

    /* Instructions */
    lcd_draw_string_center(160, 180, "Run: make flash", 0xBDF7, 0x0008, 1);
    lcd_draw_string_center(160, 195, "Hold POWER to boot normally", 0xBDF7, 0x0008, 1);
}

/* Called from iap_loop before NVIC_SystemReset after successful flash */
void lcd_draw_reboot_message(void)
{
    lcd_fill(0, 0, 320, 240, 0x0008);
    lcd_draw_string_center(160, 80, "OPENSCOPE 2C53T", 0x07FF, 0x0008, 3);
    lcd_draw_string_center(160, 130, "Rebooting...", 0x07E0, 0x0008, 2);
}

#undef _GPIO_CFG

/* ═══════════════════════════════════════════════════════════════════
 * USB
 * ═══════════════════════════════════════════════════════════════════ */

static void usb_clock48m_hick(void)
{
    crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);
    crm_periph_clock_enable(CRM_ACC_PERIPH_CLOCK, TRUE);
    acc_write_c1(7980);
    acc_write_c2(8000);
    acc_write_c3(8020);
    acc_calibration_mode_enable(ACC_CAL_HICKTRIM, TRUE);
}

void USBFS_L_CAN1_RX0_IRQHandler(void)
{
    usbd_irq_handler(&usb_core_dev);
}

/* ═══════════════════════════════════════════════════════════════════
 * Fault handlers
 * ═══════════════════════════════════════════════════════════════════ */

void NMI_Handler(void) {}
void HardFault_Handler(void) { while(1); }
void MemManage_Handler(void) { while(1); }
void BusFault_Handler(void)  { while(1); }
void UsageFault_Handler(void){ while(1); }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * Boot checks
 * ═══════════════════════════════════════════════════════════════════ */

static int check_ram_magic(void)
{
    if (*DFU_RAM_MAGIC_ADDR == DFU_RAM_MAGIC_VALUE) {
        *DFU_RAM_MAGIC_ADDR = 0;
        return 1;
    }
    return 0;
}

/* Check POWER button (PC8) — active LOW, passive (no matrix scan) */
static int power_button_pressed(void)
{
    return !(GPIOC->idt & (1U << 8));
}

static void power_button_init(void)
{
    /* PC8 input with pull-up */
    uint32_t cfghr = GPIOC->cfghr;
    cfghr &= ~(0xFU << 0);
    cfghr |= (0x8U << 0);   /* CNF=10 (input pull-up/down), MODE=00 */
    GPIOC->cfghr = cfghr;
    GPIOC->scr = (1U << 8); /* Pull-up */
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void)
{
    int enter_bootloader = 0;

    /* ── Power hold: PC9 HIGH (MUST be first!) ── */
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    GPIOC->cfghr = (GPIOC->cfghr & ~(0xFU << 4)) | (0x3U << 4);
    GPIOC->scr = (1U << 9);

    /* ── Check entry conditions ── */

    /* 1. RAM magic word (app requested firmware update) */
    if (check_ram_magic()) {
        enter_bootloader = 1;
    }

    /* 2. Flash upgrade flag (valid app installed?) */
    if (!enter_bootloader) {
        iap_init();
        if (iap_get_upgrade_flag() == IAP_SUCCESS) {
            jump_to_app(FLASH_APP_ADDRESS);
        }
        enter_bootloader = 1;
    }

    /* ── Enter USB HID IAP bootloader ── */

    system_clock_config();
    delay_init();
    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
    iap_init();

    /* LCD backlight + display init */
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    GPIOB->cfghr = (GPIOB->cfghr & ~(0xFU << 0)) | (0x3U << 0);
    GPIOB->scr = (1U << 8);  /* PB8 backlight on */

    lcd_init();
    lcd_draw_bootloader_screen();

    /* POWER button for exit */
    power_button_init();

    /* USB init */
    usb_clock48m_hick();
    crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);
    nvic_irq_enable(USBFS_L_CAN1_RX0_IRQn, 0, 0);
    usbd_core_init(&usb_core_dev, USB, &hid_iap_class_handler, &hid_iap_desc_handler, 0);
    usbd_connect(&usb_core_dev);

    /* Main loop */
    uint32_t hold_count = 0;
    while (1) {
        iap_loop();
        wdt_counter_reload();

        /* POWER button: hold ~2 seconds to boot into app */
        if (power_button_pressed()) {
            hold_count++;
            if (hold_count > 2000) {
                lcd_fill(0, 130, 320, 14, 0x0008);
                lcd_draw_string_center(160, 130, "Booting...", 0xFFE0, 0x0008, 2);
                delay_ms(300);
                NVIC_SystemReset();
            }
        } else {
            hold_count = 0;
        }

        delay_ms(1);
    }
}
