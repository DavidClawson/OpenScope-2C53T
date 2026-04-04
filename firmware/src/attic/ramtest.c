/*
 * RAM-loaded hardware test — loaded and executed via SWD/OpenOCD
 * No vector table needed, no startup code, just raw register pokes
 */
#include <stdint.h>

#define RCC_APB2ENR  (*(volatile uint32_t *)0x40021018)
#define RCC_AHBENR   (*(volatile uint32_t *)0x40021014)

#define GPIOA_CRL    (*(volatile uint32_t *)0x40010800)
#define GPIOA_CRH    (*(volatile uint32_t *)0x40010804)
#define GPIOA_ODR    (*(volatile uint32_t *)0x4001080C)
#define GPIOB_CRL    (*(volatile uint32_t *)0x40010C00)
#define GPIOB_CRH    (*(volatile uint32_t *)0x40010C04)
#define GPIOB_ODR    (*(volatile uint32_t *)0x40010C0C)
#define GPIOC_CRL    (*(volatile uint32_t *)0x40011000)
#define GPIOC_CRH    (*(volatile uint32_t *)0x40011004)
#define GPIOC_ODR    (*(volatile uint32_t *)0x4001100C)
#define GPIOD_CRL    (*(volatile uint32_t *)0x40011400)
#define GPIOD_CRH    (*(volatile uint32_t *)0x40011404)
#define GPIOD_ODR    (*(volatile uint32_t *)0x4001140C)
#define GPIOE_CRL    (*(volatile uint32_t *)0x40011800)
#define GPIOE_CRH    (*(volatile uint32_t *)0x40011804)
#define GPIOE_ODR    (*(volatile uint32_t *)0x4001180C)

#define EXMC_SNCTL0  (*(volatile uint32_t *)0xA0000000)
#define EXMC_SNTCFG0 (*(volatile uint32_t *)0xA0000004)

#define LCD_CMD      (*(volatile uint16_t *)0x6001FFFE)
#define LCD_DATA     (*(volatile uint16_t *)0x60020000)

static void delay(volatile uint32_t n) { while(n--) __asm volatile("nop"); }

void ramtest_main(void) {
    /* Step 1: Enable ALL GPIO clocks + AFIO */
    RCC_APB2ENR |= 0x7D;  /* AFIOEN + IOPAEN + IOPBEN + IOPCEN + IOPDEN + IOPEEN */
    delay(10000);

    /* Step 2: Set ALL port B and C pins as push-pull output 50MHz, all HIGH */
    /* This brute-forces the power latch and backlight */
    GPIOB_CRL = 0x33333333;  /* All low pins: push-pull 50MHz */
    GPIOB_CRH = 0x33333333;
    GPIOB_ODR = 0xFFFF;

    GPIOC_CRL = 0x33333333;
    GPIOC_CRH = 0x33333333;
    GPIOC_ODR = 0xFFFF;

    /* PA8 and PA15 high too */
    GPIOA_CRH = (GPIOA_CRH & 0x0FFFFFF0) | 0x30000003;
    GPIOA_ODR |= (1 << 8) | (1 << 15);

    delay(100000);

    /* Step 3: Enable EXMC clock */
    RCC_AHBENR |= (1 << 8);
    delay(10000);

    /* Step 4: Configure EXMC data/control pins as AF push-pull */
    /* PD0,1,4,5,7,8,9,10,11,12,14,15 */
    GPIOD_CRL = 0xB8BB44BB;  /* PD0=AF, PD1=AF, PD2=input, PD3=input, PD4=AF, PD5=AF, PD6=input, PD7=AF */
    GPIOD_CRH = 0xBB3BBBBB;  /* PD8-12=AF, PD13=input, PD14=AF, PD15=AF */

    /* PE7-15 = AF push-pull */
    GPIOE_CRL = (GPIOE_CRL & 0x0FFFFFFF) | 0xB0000000;  /* PE7 */
    GPIOE_CRH = 0xBBBBBBBB;  /* PE8-15 all AF */

    delay(10000);

    /* Step 5: Configure EXMC bank 0 - NOR/SRAM, 16-bit */
    EXMC_SNCTL0 = (1 << 0)   /* Bank enable */
                | (1 << 6)   /* 16-bit data width */
                | (1 << 12); /* NRWTPOL */

    EXMC_SNTCFG0 = (15 << 0)     /* Address setup: 15 cycles */
                 | (15 << 8)     /* Address hold: 15 cycles */
                 | (60 << 16);   /* Data setup: 60 cycles (very slow, safe) */

    delay(100000);

    /* Step 6: Try LCD software reset */
    LCD_CMD = 0x01;
    delay(1000000);  /* Long delay after reset */

    /* Step 7: Sleep out */
    LCD_CMD = 0x11;
    delay(1000000);

    /* Step 8: Pixel format 16-bit */
    LCD_CMD = 0x3A;
    LCD_DATA = 0x55;
    delay(100000);

    /* Step 9: Display ON */
    LCD_CMD = 0x29;
    delay(100000);

    /* Step 10: Set window to full screen */
    LCD_CMD = 0x2A;
    LCD_DATA = 0x00; LCD_DATA = 0x00;
    LCD_DATA = 0x01; LCD_DATA = 0x3F;

    LCD_CMD = 0x2B;
    LCD_DATA = 0x00; LCD_DATA = 0x00;
    LCD_DATA = 0x00; LCD_DATA = 0xEF;

    /* Step 11: Write pixels — solid green */
    LCD_CMD = 0x2C;
    for (uint32_t i = 0; i < 320 * 240; i++) {
        LCD_DATA = 0x07E0;  /* Green in RGB565 */
    }

    /* Done — spin with a marker value in r0 so we can see success in GDB */
    volatile uint32_t marker = 0xDEADBEEF;
    while (1) { delay(100000); marker++; }
}
