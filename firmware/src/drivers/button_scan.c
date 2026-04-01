/*
 * OpenScope 2C53T - Button Matrix Scan Driver
 *
 * Bidirectional 4x3 GPIO matrix + 3 passive buttons.
 * Extracted from stock firmware `input_and_housekeeping` at 0x08039188.
 * Hardware-confirmed on real FNIRSI 2C53T (2026-04-01).
 *
 * TMR3 ISR fires at 500Hz, scans all 15 buttons, debounces,
 * and sends confirmed presses to a FreeRTOS queue as button_id_t.
 *
 * See: reverse_engineering/analysis_v120/button_map_confirmed.md
 *      reverse_engineering/analysis_v120/button_scan_algorithm.md
 */

#include "button_scan.h"

/* ═══════════════════════════════════════════════════════════════════
 * Hardware-confirmed button bit → button_id_t mapping
 *
 * Bit indices from fulltest2.c hardware test (2026-04-01):
 *   0=POWER, 1=AUTO, 2=CH1, 3=MOVE, 4=SELECT, 5=TRIGGER,
 *   6=PRM(?), 7=CH2, 8=SAVE, 9=MENU, 10=UP, 11=DOWN,
 *   12=LEFT, 13=RIGHT, 14=PLAY/PAUSE
 * ═══════════════════════════════════════════════════════════════════ */

static const button_id_t bit_to_button[15] = {
    BTN_POWER,    /* bit 0:  PC8 passive */
    BTN_AUTO,     /* bit 1:  PE2 + PC10 */
    BTN_CH1,      /* bit 2:  PC5 + PC10 */
    BTN_MOVE,     /* bit 3:  PB0 + PA8 */
    BTN_SELECT,   /* bit 4:  PC5 + PA8 */
    BTN_TRIGGER,  /* bit 5:  PA7 + PA8 */
    BTN_PRM,      /* bit 6:  PB7 passive (active HIGH — detection WIP) */
    BTN_CH2,      /* bit 7:  PA7 + PE3 */
    BTN_SAVE,     /* bit 8:  PB0 + PE3 */
    BTN_MENU,     /* bit 9:  PE2 + PE3 */
    BTN_UP,       /* bit 10: PC13 passive */
    BTN_DOWN,     /* bit 11: PC5 + PE3 */
    BTN_LEFT,     /* bit 12: PA7 + PC10 */
    BTN_RIGHT,    /* bit 13: PE2 + PA8 */
    BTN_OK,       /* bit 14: PB0 + PC10 (PLAY/PAUSE = OK in our UI) */
};

/* ═══════════════════════════════════════════════════════════════════
 * State
 * ═══════════════════════════════════════════════════════════════════ */

#define DEBOUNCE_CONFIRM  70   /* 140ms at 500Hz — short press */
#define NUM_BUTTONS       15

static QueueHandle_t  s_button_queue = NULL;
static volatile uint16_t s_raw_state = 0;
static uint8_t s_debounce[NUM_BUTTONS] = {0};

/* ═══════════════════════════════════════════════════════════════════
 * GPIO helpers — reconfigure pins each scan phase
 * ═══════════════════════════════════════════════════════════════════ */

static void pin_input_pullup(gpio_type *port, uint16_t pin) {
    gpio_init_type cfg;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pin;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(port, &cfg);
}

static void pin_output_pp(gpio_type *port, uint16_t pin) {
    gpio_init_type cfg;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pin;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(port, &cfg);
}

/* ═══════════════════════════════════════════════════════════════════
 * Matrix scan — called from TMR3 ISR
 * ═══════════════════════════════════════════════════════════════════ */

static uint16_t matrix_scan(void) {
    uint16_t events = 0;
    uint8_t row = 0;

    /* ── Passive reads ──────────────────────────────────────── */
    if (!(GPIOC->idt & (1 << 8)))   events |= 0x0001;  /* PC8: POWER */
    if (GPIOB->idt & (1 << 7))      events |= 0x0040;  /* PB7: PRM (active HIGH) */
    if (!(GPIOC->idt & (1 << 13)))  events |= 0x0400;  /* PC13: UP */

    /* ── Phase 1: Cols = output LOW, Rows = input pull-up ──── */
    pin_input_pullup(GPIOA, GPIO_PINS_7);
    pin_input_pullup(GPIOB, GPIO_PINS_0);
    pin_input_pullup(GPIOC, GPIO_PINS_5);
    pin_input_pullup(GPIOE, GPIO_PINS_2);

    pin_output_pp(GPIOA, GPIO_PINS_8);
    pin_output_pp(GPIOC, GPIO_PINS_10);
    pin_output_pp(GPIOE, GPIO_PINS_3);

    GPIOA->clr = (1 << 8);
    GPIOC->clr = (1 << 10);
    GPIOE->clr = (1 << 3);

    __NOP(); __NOP(); __NOP(); __NOP();

    if (!(GPIOA->idt & (1 << 7)))  row |= 1;
    if (!(GPIOC->idt & (1 << 5)))  row |= 2;
    if (!(GPIOB->idt & (1 << 0)))  row |= 4;
    if (!(GPIOE->idt & (1 << 2)))  row |= 8;

    /* Must be exactly one row active */
    if (row != 1 && row != 2 && row != 4 && row != 8)
        return events;

    /* ── Phase 2: Swap — Rows = output LOW, Cols = input pull-up */
    pin_input_pullup(GPIOA, GPIO_PINS_8);
    pin_input_pullup(GPIOC, GPIO_PINS_10);
    pin_input_pullup(GPIOE, GPIO_PINS_3);

    pin_output_pp(GPIOA, GPIO_PINS_7);
    pin_output_pp(GPIOB, GPIO_PINS_0);
    pin_output_pp(GPIOC, GPIO_PINS_5);
    pin_output_pp(GPIOE, GPIO_PINS_2);

    GPIOA->clr = (1 << 7);
    GPIOB->clr = (1 << 0);
    GPIOC->clr = (1 << 5);
    GPIOE->clr = (1 << 2);

    __NOP(); __NOP(); __NOP(); __NOP();

    /* Read columns, combine with row */
    if (!(GPIOE->idt & (1 << 3))) {      /* PE3 = col 1 */
        switch (row) {
            case 1: events |= 0x0080; break;  /* CH2 */
            case 2: events |= 0x0800; break;  /* DOWN */
            case 4: events |= 0x0100; break;  /* SAVE */
            case 8: events |= 0x0200; break;  /* MENU */
        }
    }
    if (!(GPIOA->idt & (1 << 8))) {      /* PA8 = col 2 */
        switch (row) {
            case 1: events |= 0x0020; break;  /* TRIGGER */
            case 2: events |= 0x0010; break;  /* SELECT */
            case 4: events |= 0x0008; break;  /* MOVE */
            case 8: events |= 0x2000; break;  /* RIGHT */
        }
    }
    if (!(GPIOC->idt & (1 << 10))) {     /* PC10 = col 3 */
        switch (row) {
            case 1: events |= 0x1000; break;  /* LEFT */
            case 2: events |= 0x0004; break;  /* CH1 */
            case 4: events |= 0x4000; break;  /* OK/PLAY */
            case 8: events |= 0x0002; break;  /* AUTO */
        }
    }

    return events;
}

/* ═══════════════════════════════════════════════════════════════════
 * TMR3 ISR — 500Hz scan + debounce
 * ═══════════════════════════════════════════════════════════════════ */

void TMR3_GLOBAL_IRQHandler(void) {
    if (!(TMR3->ists & 0x01)) return;
    TMR3->ists &= ~0x01;

    uint16_t events = matrix_scan();
    s_raw_state = events;

    /* Debounce: confirm on rising edge at tick 70 */
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (i == 6) continue;  /* skip PRM (bit 6) — always HIGH, WIP */

        uint16_t mask = (1 << i);
        if (events & mask) {
            if (s_debounce[i] < 0xFF) s_debounce[i]++;
            if (s_debounce[i] == DEBOUNCE_CONFIRM) {
                /* Confirmed press — send to queue from ISR */
                button_id_t btn = bit_to_button[i];
                BaseType_t woken = pdFALSE;
                xQueueSendFromISR(s_button_queue, &btn, &woken);
                portYIELD_FROM_ISR(woken);
            }
        } else {
            s_debounce[i] = 0;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

void button_scan_init(QueueHandle_t button_queue) {
    s_button_queue = button_queue;

    /* Ensure all GPIO clocks are enabled (A, B, C, E used by matrix) */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);

    /* Configure passive button pins as inputs */
    pin_input_pullup(GPIOC, GPIO_PINS_8);   /* PC8: POWER */
    pin_input_pullup(GPIOB, GPIO_PINS_7);   /* PB7: PRM */
    pin_input_pullup(GPIOC, GPIO_PINS_13);  /* PC13: UP */

    /* Configure TMR3: 500Hz (PSC=11999, ARR=19, APB1=120MHz) */
    crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK, TRUE);

    TMR3->ctrl1 = 0;
    TMR3->div   = 11999;
    TMR3->pr    = 19;
    TMR3->cval  = 0;
    TMR3->ists  = 0;
    TMR3->iden  = (1 << 0);  /* Update interrupt enable */

    nvic_irq_enable(TMR3_GLOBAL_IRQn, 5, 0);

    TMR3->ctrl1 |= (1 << 0);  /* Counter enable */
}

uint16_t button_scan_get_raw(void) {
    return s_raw_state;
}
