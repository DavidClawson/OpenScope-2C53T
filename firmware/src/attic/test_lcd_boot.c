/*
 * Minimal LCD boot test — isolates Renode EXMC issue
 * No FreeRTOS, no FFT, no signal gen. Just:
 *   1. Set SystemCoreClock
 *   2. Init GPIO for EXMC
 *   3. Init EXMC/FSMC
 *   4. Init ST7789V
 *   5. Fill screen with red
 *   6. Spin forever
 *
 * Build: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 \
 *        -mfloat-abi=hard -DEMULATOR_BUILD -Os -nostartfiles \
 *        -Tld/gd32f307.ld -o build/test_lcd.elf src/test_lcd_boot.c \
 *        src/drivers/lcd.c -Isrc/drivers -Iinclude \
 *        -Igd32f30x_lib/Firmware/CMSIS \
 *        -Igd32f30x_lib/Firmware/CMSIS/GD/GD32F30x/Include
 */

#include <stdint.h>

/* Memory-mapped LCD addresses */
#define LCD_CMD_ADDR   ((volatile uint16_t *)0x6001FFFE)
#define LCD_DATA_ADDR  ((volatile uint16_t *)0x60020000)

/* EXMC registers */
#define EXMC_SNCTL0    (*(volatile uint32_t *)0xA0000000)
#define EXMC_SNTCFG0   (*(volatile uint32_t *)0xA0000004)
#define EXMC_SNWTCFG0  (*(volatile uint32_t *)0xA0000104)

/* GPIO registers */
#define GPIOD_CRL      (*(volatile uint32_t *)0x40011400)
#define GPIOD_CRH      (*(volatile uint32_t *)0x40011404)
#define GPIOE_CRL      (*(volatile uint32_t *)0x40011800)
#define GPIOE_CRH      (*(volatile uint32_t *)0x40011804)

/* Marker in SRAM so we can check progress from Renode */
#define MARKER_ADDR    ((volatile uint32_t *)0x2003FF00)

uint32_t SystemCoreClock = 120000000;

void delay_ms(uint32_t ms)
{
    volatile uint32_t count;
    while (ms--) {
        count = 120;  /* Reduced for emulator speed */
        while (count--) {
            __asm volatile("nop");
        }
    }
}

void SystemInit(void)
{
    /* Stub */
}

/* Minimal EXMC GPIO setup */
static void init_gpio(void)
{
    MARKER_ADDR[0] = 0x01;  /* Stage 1: GPIO init */

    /* Enable clocks directly */
    *(volatile uint32_t *)0x40021014 = 0x00000114;  /* AHBEN: SRAM + EXMC */
    *(volatile uint32_t *)0x40021018 = 0x0000FFFD;  /* APB2EN: all GPIO */

    /* PORTD: D0-D1(PD14-15), D2-D3(PD0-1), A16/17(PD11-12), NE1(PD7), NWE(PD5), NOE(PD4) */
    GPIOD_CRL = 0xB4BB44BB;  /* PD0,1=AF_PP, PD4,5=AF_PP, PD7=AF_PP */
    GPIOD_CRH = 0xBB444BBB;  /* PD8-10=AF_PP, PD11-12=AF_PP, PD14-15=AF_PP */

    /* PORTE: D4-D11 (PE7-14) and D12 (PE15) */
    GPIOE_CRL = 0xB4444444;  /* PE7=AF_PP */
    GPIOE_CRH = 0x4BBBBBBB;  /* PE8-14=AF_PP */

    MARKER_ADDR[1] = 0x02;  /* Stage 2: GPIO done */
}

/* Minimal EXMC/FSMC setup */
static void init_exmc(void)
{
    MARKER_ADDR[2] = 0x03;  /* Stage 3: EXMC init */

    EXMC_SNCTL0  = 0x00005010;  /* 16-bit, SRAM mode, write enable */
    EXMC_SNTCFG0 = 0x02020424;  /* Read timing */
    EXMC_SNWTCFG0= 0x00000202;  /* Write timing */
    EXMC_SNCTL0 |= 0x0001;      /* Enable bank */

    MARKER_ADDR[3] = 0x04;  /* Stage 4: EXMC done */
}

/* Write a command to the LCD */
static void lcd_cmd(uint8_t cmd)
{
    *LCD_CMD_ADDR = (uint16_t)cmd;
}

/* Write data to the LCD */
static void lcd_dat(uint16_t data)
{
    *LCD_DATA_ADDR = data;
}

/* Write 8-bit data */
static void lcd_dat8(uint8_t data)
{
    *LCD_DATA_ADDR = (uint16_t)data;
}

int main(void)
{
    MARKER_ADDR[4] = 0xAA;  /* Marker: main entered */

    init_gpio();
    init_exmc();

    MARKER_ADDR[5] = 0xBB;  /* Marker: about to write LCD */

    /* Minimal LCD init: just enough to write pixels */
    lcd_cmd(0x11);       /* Sleep out */
    delay_ms(5);         /* Short delay (reduced for emu) */

    lcd_cmd(0x3A);       /* COLMOD */
    lcd_dat8(0x55);      /* RGB565 */

    lcd_cmd(0x36);       /* MADCTL */
    lcd_dat8(0xA0);      /* Landscape */

    lcd_cmd(0x29);       /* Display on */

    MARKER_ADDR[6] = 0xCC;  /* Marker: LCD init done */

    /* Set window to full screen */
    lcd_cmd(0x2A);       /* CASET */
    lcd_dat8(0x00); lcd_dat8(0x00);  /* Start col = 0 */
    lcd_dat8(0x01); lcd_dat8(0x3F);  /* End col = 319 */

    lcd_cmd(0x2B);       /* RASET */
    lcd_dat8(0x00); lcd_dat8(0x00);  /* Start row = 0 */
    lcd_dat8(0x00); lcd_dat8(0xEF);  /* End row = 239 */

    lcd_cmd(0x2C);       /* RAMWR */

    MARKER_ADDR[7] = 0xDD;  /* Marker: about to write pixels */

    /* Fill screen with red (0xF800) */
    uint32_t i;
    for (i = 0; i < 320 * 240; i++) {
        lcd_dat(0xF800);
    }

    MARKER_ADDR[8] = 0xEE;  /* Marker: pixels done! */

    /* Spin forever */
    while (1) {
        __asm volatile("nop");
    }
}

/* Minimal vector table */
extern uint32_t _estack;
void Reset_Handler(void) __attribute__((alias("main")));

__attribute__((section(".isr_vector")))
uint32_t *vectors[] = {
    (uint32_t *)&_estack,
    (uint32_t *)main,
};
