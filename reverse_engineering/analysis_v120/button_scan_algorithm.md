# FNIRSI 2C53T Button Scanning Algorithm

Extracted from stock firmware V1.2.0 decompilation.
Function: `input_and_housekeeping` at 0x080390B8 (1342 bytes actual, starts at the `push` instruction).

## Summary

The buttons use a **bidirectional GPIO matrix scan** across 4 GPIO ports (GPIOA, GPIOB, GPIOC, GPIOE). Seven MCU pins serve dual roles -- each pin alternates between output-drive and input-read across two scan phases. This is NOT charlieplexing; it is a classic row/column matrix where rows and columns swap roles between phases.

**Why passive GPIO reads fail:** All 7 scan pins idle in their last-configured state. Without actively driving outputs LOW and reading inputs, the button connections are invisible.

## Pin Assignments

| Pin | Phase 1 Role | Phase 2 Role |
|-----|-------------|-------------|
| PA7 | Input (pull-up) | Output (push-pull, driven LOW) |
| PB0 | Input (pull-up) | Output (push-pull, driven LOW) |
| PC5 | Input (pull-up) | Output (push-pull, driven LOW) |
| PE2 | Input (pull-up) | Output (push-pull, driven LOW) |
| PA8 | Output (push-pull, driven LOW) | Input (pull-down) |
| PC10 | Output (push-pull, driven LOW) | Input (pull-down) |
| PE3 | Output (push-pull, driven LOW) | Input (pull-down) |

Additionally, three pins are read passively (no driving) before the matrix scan:
- **PC8** -- read at function entry (bit 8 of GPIOC_IDR)
- **PB7** -- read at function entry (bit 7 of GPIOB_IDR via 0x40010C08)
- **PC13** -- read at function entry (bit 13 of GPIOC_IDR)

## GPIO Register Addresses

| Register | Address | Purpose |
|----------|---------|---------|
| GPIOA base | 0x40010800 | PA7, PA8 |
| GPIOB base | 0x40010C00 | PB0, PB7 |
| GPIOC base | 0x40011000 | PC5, PC8, PC10, PC13 |
| GPIOE base | 0x40011800 | PE2, PE3 |
| GPIOA_IDR | 0x40010808 | Read PA pins |
| GPIOA_BOP | 0x40010810 | Set PA pins HIGH |
| GPIOA_BCR | 0x40010814 | Set PA pins LOW |
| GPIOB_IDR | 0x40010C08 | Read PB pins |
| GPIOB_BCR | 0x40010C14 | Set PB pins LOW |
| GPIOC_IDR | 0x40011008 | Read PC pins |
| GPIOC_BOP | 0x40011010 | Set PC pins HIGH |
| GPIOC_BCR | 0x40011014 | Set PC pins LOW |
| GPIOE_IDR | 0x40011808 | Read PE pins |
| GPIOE_BCR | 0x40011814 | Set PE pins LOW |

## gpio_init Wrapper (FUN_080302fc)

The stock firmware uses a wrapper around the GD32 `gpio_init` library function. It takes:
- `param1` = GPIO port base address (e.g., 0x40010800 for GPIOA)
- `param2` = pointer to 8-byte config struct:

```c
struct gpio_config {
    uint16_t pin_mask;    // offset 0: GPIO_PIN_x bitmask
    uint16_t reserved;    // offset 2
    uint8_t  unknown;     // offset 4
    uint8_t  sub_mode;    // offset 5: 0x18=pull-up input, 0x28=pull-down input
    uint8_t  main_mode;   // offset 6: 0x00=input w/pull, 0x10+=output
    uint8_t  speed;       // offset 7: output speed (1=10MHz, 2=2MHz, 3=50MHz)
};
```

Mode encoding:
- **Input pull-up**: main_mode=0, sub_mode=0x18 -> writes BOP (pull-up), CNF=10 MODE=00
- **Input pull-down**: main_mode=0, sub_mode=0x28 -> writes BCR (pull-down), CNF=10 MODE=00  
- **Output push-pull 10MHz**: main_mode=0x10, speed=1 -> CNF=00 MODE=01

In the disassembly, the full 32-bit word at struct offset 4 is:
- `0x01001800` = input pull-up (sub_mode=0x18, main_mode=0x00, speed=0x01)
- After `strh 0x1028, [sp+0x11]`: sub_mode=0x28, main_mode=0x10, speed=0x01 = output push-pull 10MHz

## Exact Scan Algorithm (Pseudocode)

```c
void input_and_housekeeping(void) {
    uint16_t button_events;   // at 0x20002D56 (fp register)
    uint8_t  input_state;     // at sp+0xb
    
    // ================================================================
    // STEP 0: Clear state
    // ================================================================
    button_events = 0;
    input_state = 0;
    
    // ================================================================
    // STEP 1: Passive reads (no GPIO reconfiguration needed)
    // These pins are always configured as inputs elsewhere
    // ================================================================
    if (!(GPIOC_IDR & (1 << 8)))    button_events |= 0x0001;  // PC8 LOW
    if (GPIOB_IDR & (1 << 7))       button_events |= 0x0040;  // PB7 HIGH (note: not inverted!)
    if (!(GPIOC_IDR & (1 << 13)))   button_events |= 0x0400;  // PC13 LOW
    
    // ================================================================
    // STEP 2: PHASE 1 -- Configure and scan first direction
    //   OUTPUTS (push-pull, driven LOW): PA8, PC10, PE3
    //   INPUTS  (pull-up):               PA7, PB0, PC5, PE2
    // ================================================================
    
    // Configure PA7 as input pull-up
    gpio_config_input_pullup(GPIOA, GPIO_PIN_7);     // 0x80
    // Configure PB0 as input pull-up  
    gpio_config_input_pullup(GPIOB, GPIO_PIN_0);     // 0x01
    // Configure PC5 as input pull-up
    gpio_config_input_pullup(GPIOC, GPIO_PIN_5);     // 0x20
    // Configure PE2 as input pull-up
    gpio_config_input_pullup(GPIOE, GPIO_PIN_2);     // 0x04
    
    // Configure PA8 as output push-pull 10MHz
    gpio_config_output_pp(GPIOA, GPIO_PIN_8);        // 0x100
    // Configure PC10 as output push-pull 10MHz
    gpio_config_output_pp(GPIOC, GPIO_PIN_10);       // 0x400
    // Configure PE3 as output push-pull 10MHz
    gpio_config_output_pp(GPIOE, GPIO_PIN_3);        // 0x08
    
    // Drive ALL outputs LOW simultaneously
    GPIOE_BCR = (1 << 3);     // PE3 = LOW
    GPIOA_BCR = (1 << 8);     // PA8 = LOW
    GPIOC_BCR = (1 << 10);    // PC10 = LOW
    
    // Read inputs (inverted: button press pulls input LOW through matrix)
    // A button connects one output line to one input line.
    // Output is LOW; input has pull-up. Press -> input goes LOW.
    input_state = 0;
    input_state |= (~GPIOA_IDR >> 7) & 0x01;   // PA7 LOW -> bit 0
    input_state |= (~GPIOC_IDR >> 4) & 0x02;   // PC5 LOW -> bit 1  (actually: 2 & ~(IDR>>4))
    input_state |= (~GPIOB_IDR << 2) & 0x04;   // PB0 LOW -> bit 2  (actually: 4 & ~(IDR<<2))  
    input_state |= (~GPIOE_IDR << 1) & 0x08;   // PE2 LOW -> bit 3  (actually: 8 & ~(IDR<<1))
    
    // Phase 1 tells us WHICH ROW has a pressed button.
    // Must be exactly ONE bit set (1, 2, 4, or 8) to proceed.
    if (input_state != 1 && input_state != 2 && 
        input_state != 4 && input_state != 8) {
        goto debounce;  // No button or multiple buttons
    }
    
    // ================================================================
    // STEP 3: PHASE 2 -- Swap directions and scan columns
    //   OUTPUTS (push-pull, driven LOW): PA7, PB0, PC5, PE2
    //   INPUTS  (pull-down):             PA8, PC10, PE3
    // ================================================================
    
    // Configure PA8 as input pull-up (actually BOP writes pull-up)
    gpio_config_input_pullup(GPIOA, GPIO_PIN_8);     // 0x100
    // Configure PC10 as input pull-up
    gpio_config_input_pullup(GPIOC, GPIO_PIN_10);    // 0x400
    // Configure PE3 as input pull-up
    gpio_config_input_pullup(GPIOE, GPIO_PIN_3);     // 0x08
    
    // Configure PA7 as output push-pull 10MHz
    gpio_config_output_pp(GPIOA, GPIO_PIN_7);        // 0x80
    // Configure PB0 as output push-pull 10MHz
    gpio_config_output_pp(GPIOB, GPIO_PIN_0);        // 0x01
    // Configure PC5 as output push-pull 10MHz
    gpio_config_output_pp(GPIOC, GPIO_PIN_5);        // 0x20
    // Configure PE2 as output push-pull 10MHz
    gpio_config_output_pp(GPIOE, GPIO_PIN_2);        // 0x04
    
    // Drive ALL outputs LOW simultaneously
    GPIOA_BCR = (1 << 7);     // PA7 = LOW
    GPIOC_BCR = (1 << 5);     // PC5 = LOW
    GPIOB_BCR = (1 << 0);     // PB0 = LOW
    GPIOE_BCR = (1 << 2);     // PE2 = LOW
    
    // Read column inputs and combine with row (input_state) to identify button
    // Phase 2 reads are ACTIVE-LOW (button press pulls column LOW through row)
    
    // Column 1: PE3 (GPIOE_IDR bit 3)
    if (!(GPIOE_IDR & (1 << 3))) {
        // PE3 is LOW = button detected on column 1
        switch (input_state) {
            case 1: button_events |= 0x0080; break;  // Row PA7 + Col PE3
            case 2: button_events |= 0x0800; break;  // Row PC5 + Col PE3
            case 4: button_events |= 0x0100; break;  // Row PB0 + Col PE3
            case 8: button_events |= 0x0200; break;  // Row PE2 + Col PE3
        }
    }
    
    // Column 2: PA8 (GPIOA_IDR bit 8)
    if (!(GPIOA_IDR & (1 << 8))) {
        // PA8 is LOW = button detected on column 2
        switch (input_state) {
            case 1: button_events |= 0x0020; break;  // Row PA7 + Col PA8
            case 2: button_events |= 0x0010; break;  // Row PC5 + Col PA8
            case 4: button_events |= 0x0008; break;  // Row PB0 + Col PA8
            case 8: button_events |= 0x2000; break;  // Row PE2 + Col PA8
        }
    }
    
    // Column 3: PC10 (GPIOC_IDR bit 10)
    if (!(GPIOC_IDR & (1 << 10))) {
        // PC10 is LOW = button detected on column 3
        switch (input_state) {
            case 1: button_events |= 0x1000; break;  // Row PA7 + Col PC10
            case 2: button_events |= 0x0004; break;  // Row PC5 + Col PC10
            case 4: button_events |= 0x4000; break;  // Row PB0 + Col PC10
            case 8: button_events |= 0x0002; break;  // Row PE2 + Col PC10
        }
    }
    
debounce:
    // ================================================================
    // STEP 4: Debounce 15 buttons with per-button counters
    // ================================================================
    uint16_t prev_events = *(uint16_t*)0x20002D50;  // previous scan result
    uint8_t *counters = (uint8_t*)0x20002D58;       // 15 debounce counters
    const uint8_t *btn_map = (const uint8_t*)0x08046528;  // button ID map table
    
    uint8_t event_count = 0;
    uint8_t button_id = 0;
    
    // Process 5 groups of 3 buttons each (15 total)
    // button_events bit assignments per group:
    //   Group 0 (offset 0):  bits 0, 1, 2   -> masks 0x01, 0x02, 0x04
    //   Group 1 (offset 3):  bits 3, 4, 5   -> masks 0x08, 0x10, 0x20
    //   Group 2 (offset 6):  bits 6, 7, 8   -> masks 0x40, 0x80, 0x100
    //   Group 3 (offset 9):  bits 9, 10, 11 -> masks 0x200, 0x400, 0x800
    //   Group 4 (offset 12): bits 12, 13, 14 -> masks 0x1000, 0x2000, 0x4000
    
    for (int group = 0; group < 15; group += 3) {
        for (int btn = 0; btn < 3; btn++) {
            int idx = group + btn;
            uint16_t mask;
            
            // btn 0 uses shift of 1, btn 1 uses shift of 2, btn 2 uses shift of 4
            if (btn == 0) mask = (1 << group);
            if (btn == 1) mask = (2 << group);
            if (btn == 2) mask = (4 << group);
            
            uint8_t counter = counters[idx];
            
            if (button_events & mask) {
                // Button is currently pressed
                if (counter != 0xFF) {
                    counter++;
                    counters[idx] = counter;
                }
                
                if (counter == 0x46) {  // 70 ticks = SHORT PRESS confirmed
                    event_count++;
                    button_id = btn_map[idx + 0x0F];  // short-press ID
                }
                
                if ((prev_events & mask) && counter == 0x48) {  // 72 ticks = LONG PRESS / REPEAT
                    event_count++;
                    button_id = btn_map[idx];  // long-press/repeat ID
                    counters[idx]--;  // decrement back to 0x47 to fire again next tick
                }
            } else {
                // Button is released
                if (counter >= 2 && counter <= 0x45) {
                    // Quick release (before short-press threshold)
                    event_count++;
                    button_id = btn_map[idx];  // release ID
                }
                counters[idx] = 0;  // reset counter
            }
        }
    }
    
    // ================================================================
    // STEP 5: Send confirmed button event to queue
    // ================================================================
    if ((uint8_t)event_count == 1 && button_id != 0) {
        xQueueGenericSend(*(QueueHandle_t*)0x20002D70, &button_id, 0);
    }
    
    // ================================================================
    // STEP 6: Watchdog, housekeeping timers, frequency measurement
    // (see full decompile for details)
    // ================================================================
    // ... IWDG feed, frame counter, auto-power-off, countdown timers,
    //     TIM5 frequency measurement, acquisition trigger monitoring ...
}
```

## Button Matrix Map

Based on the bit assignments from the Phase 2 column reads:

```
                Col PE3         Col PA8         Col PC10
               (bit group 0)   (bit group 1)   (bit group 2)
Row PA7  (1):   0x0080          0x0020          0x1000
Row PC5  (2):   0x0800          0x0010          0x0004
Row PB0  (4):   0x0100          0x0008          0x4000
Row PE2  (8):   0x0200          0x2000          0x0002
```

Plus 3 non-matrix (passive) buttons:
- PC8:  0x0001  (active LOW: bit set when PC8 is LOW)
- PB7:  0x0040  (active HIGH: bit set when PB7 is HIGH -- opposite polarity!)
- PC13: 0x0400  (active LOW: bit set when PC13 is LOW)

### Likely Physical Button Mapping

The button_map_table at 0x08046528 translates these 15 bit positions to logical button IDs. Based on the FNIRSI 2C53T physical layout (15 buttons: CH1, CH2, MOVE, SELECT, TRIGGER, PRM, AUTO, SAVE, MENU, UP, DOWN, LEFT, RIGHT, OK, POWER):

| Bit Position | Event Mask | Matrix Position | Likely Button |
|-------------|-----------|-----------------|---------------|
| 0 | 0x0001 | PC8 passive | Power button |
| 1 | 0x0002 | PE2 + PC10 | OK |
| 2 | 0x0004 | PC5 + PC10 | SELECT |
| 3 | 0x0008 | PB0 + PA8 | Arrow (DOWN?) |
| 4 | 0x0010 | PC5 + PA8 | Arrow (UP?) |
| 5 | 0x0020 | PA7 + PA8 | Arrow (LEFT?) |
| 6 | 0x0040 | PB7 passive | CH1 or CH2 |
| 7 | 0x0080 | PA7 + PE3 | CH1 or CH2 |
| 8 | 0x0100 | PB0 + PE3 | MOVE or PRM |
| 9 | 0x0200 | PE2 + PE3 | AUTO or SAVE |
| 10 | 0x0400 | PC13 passive | TRIGGER |
| 11 | 0x0800 | PC5 + PE3 | MENU or SAVE |
| 12 | 0x1000 | PA7 + PC10 | TRIGGER |
| 13 | 0x2000 | PE2 + PA8 | Arrow (RIGHT?) |
| 14 | 0x4000 | PB0 + PC10 | MENU |

Note: The exact button-to-position mapping requires either reading the table at 0x08046528 from the firmware binary, or empirical testing with the scan code running.

## TMR3 (Timer 3) Configuration

### Register Values

| Register | Address | Value | Meaning |
|----------|---------|-------|---------|
| TIM3_CR1 (CTL0) | 0x40000400 | bit 0 cleared then set | Up-counter, CMS=00, DIR=0 |
| TIM3_DIER | 0x40000414 | bit 0 set (UIE) | Update interrupt enable |
| TIM3_CNT | 0x40000424 | 0 (reset) | Counter cleared before start |
| TIM3_PSC | 0x40000428 | 11999 | Prescaler = APB1_CLK/10000 - 1 |
| TIM3_ARR | 0x4000042C | 19 (0x13) | Auto-reload = 20 - 1 |
| NVIC_ISER0 | 0xE000E100 | bit 29 set | TIM3 global IRQ enabled |
| NVIC priority | 0xE000E41D | configured | TIM3 interrupt priority set |

### Clock Calculation

```
APB1 clock = 120 MHz (AT32F403A at 240MHz, APB1 = HCLK/2)
PSC = 11999
ARR = 19

Timer frequency = APB1_CLK / ((PSC + 1) * (ARR + 1))
                = 120,000,000 / (12000 * 20)
                = 500 Hz  (2 ms period)
```

The scan runs at **500 Hz**. With a debounce threshold of 0x46 (70) ticks, a button press is confirmed after **140 ms**. Long press/repeat fires at 0x48 (72) ticks = **144 ms**, then repeats every 2 ticks = **4 ms**.

### Variable ARR for Different Timebases

When the oscilloscope timebase index >= 0x14 (20), the ARR is loaded from a table at 0x080465A4:

```c
// Table at 0x080465A4, indexed by (timebase - 0x14), up to 8 entries
uint32_t arr_table[9];  // values unknown without firmware dump
// PSC stays at 11999
// TMR3 is enabled (CEN=1) only for these slow timebases
```

For timebases < 0x14, TMR3 is disabled (CEN=0). The slow timebases need TMR3 to periodically trigger the USART exchange and button scan.

## TIM3 ISR -> Button Scan Chain

### ISR: FUN_0802771c (Vector 45, IRQ 29)

```c
void TIM3_IRQHandler(void) {          // 0x0802771C, 110 bytes
    if (TIM3_INTF & 1) {              // Check update interrupt flag
        if (DAT_20000124 == 0) {      // System state check (scope active?)
            uint8_t cmd = 3;
            int woken = 0;
            xQueueSendFromISR(spi3_data_queue, &cmd, &woken);
            TIM3_INTF &= ~1;          // Clear interrupt flag
            if (woken) {
                SCB_ICSR = 0x10000000; // Trigger PendSV for context switch
                DSB(); ISB();
            }
        } else {
            TIM3_INTF &= ~1;          // Just clear flag
        }
    }
}
```

### Dispatch Chain

```
TMR3 ISR (0x0802771C)
    |
    | sends cmd=3 to spi3_data_queue (0x20002D78)
    v
FPGA task main loop (0x08036A50)
    |
    | xQueueReceive(spi3_data_queue, &trigger_byte, ...)
    | trigger_byte == 3: dispatch through function table
    v
Function table at 0x08046544 [index 3]
    |
    v
probe_change_handler (0x080396C8)
    |
    | direct call
    v
input_and_housekeeping (0x080390B8)
    |
    | scans GPIO matrix, debounces
    | if confirmed press: xQueueSend to button_event_queue
    v
button_event_queue (0x20002D70)
    |
    v
"key" task (0x08040009, priority 4)
    |
    | dispatches button ID through handler table
    v
UI response
```

### Key Observations

1. **TMR3 ISR does NOT call input_and_housekeeping directly.** It sends a FreeRTOS queue message (value 3) from ISR context, which wakes the FPGA task.
2. The FPGA task dispatches through a function pointer table. Command 3 maps to `probe_change_handler`, which calls `input_and_housekeeping`.
3. The entire scan (Phase 1 + Phase 2 + debounce) runs in FPGA task context, NOT in ISR context.
4. Button events are sent to a separate "key" task via another FreeRTOS queue.

## Recommended Implementation for Test Firmware

### Bare-Metal (No FreeRTOS) Timer ISR Approach

```c
#include "at32f403a_gpio.h"
#include "at32f403a_tmr.h"

// Pin definitions
#define ROW_PA7   GPIO_PINS_7
#define ROW_PB0   GPIO_PINS_0
#define ROW_PC5   GPIO_PINS_5
#define ROW_PE2   GPIO_PINS_2
#define COL_PA8   GPIO_PINS_8
#define COL_PC10  GPIO_PINS_10
#define COL_PE3   GPIO_PINS_3

// Passive buttons
#define BTN_PC8   GPIO_PINS_8
#define BTN_PB7   GPIO_PINS_7
#define BTN_PC13  GPIO_PINS_13

// Debounce
#define DEBOUNCE_SHORT   70   // 0x46: confirm short press (140ms at 500Hz)
#define DEBOUNCE_LONG    72   // 0x48: long press / repeat
#define DEBOUNCE_MAX     0xFF

static volatile uint16_t g_button_events = 0;
static volatile uint16_t g_prev_events = 0;
static uint8_t g_debounce[15] = {0};
static volatile uint8_t g_last_button = 0;

// Button map table -- fill in from empirical testing or firmware dump
// btn_map[0..14] = long-press/release IDs
// btn_map[15..29] = short-press IDs
static const uint8_t btn_map[30] = {0}; // TODO: extract from 0x08046528

void configure_pin_input_pullup(gpio_type *port, uint16_t pin) {
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = pin;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(port, &gpio_cfg);
}

void configure_pin_output_pp(gpio_type *port, uint16_t pin) {
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = pin;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(port, &gpio_cfg);
}

void button_scan_init(void) {
    // Enable GPIO clocks (GPIOA, GPIOB, GPIOC, GPIOE)
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    
    // Configure passive button inputs
    configure_pin_input_pullup(GPIOC, BTN_PC8);
    configure_pin_input_pullup(GPIOB, BTN_PB7);
    configure_pin_input_pullup(GPIOC, BTN_PC13);
    
    // Configure TMR3: 500 Hz interrupt
    crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK, TRUE);
    
    tmr_base_init(TMR3, 19, 11999);  // ARR=19, PSC=11999 -> 500Hz
    tmr_cnt_dir_set(TMR3, TMR_COUNT_UP);
    tmr_interrupt_enable(TMR3, TMR_OVF_INT, TRUE);
    
    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
    nvic_irq_enable(TMR3_GLOBAL_IRQn, 5, 0);
    
    tmr_counter_enable(TMR3, TRUE);
}

void TMR3_GLOBAL_IRQHandler(void) {
    if (tmr_flag_get(TMR3, TMR_OVF_FLAG)) {
        tmr_flag_clear(TMR3, TMR_OVF_FLAG);
        
        uint16_t events = 0;
        uint8_t input_state = 0;
        
        // ---- Passive reads ----
        if (!(GPIOC->idt & (1 << 8)))   events |= 0x0001;  // PC8
        if (GPIOB->idt & (1 << 7))      events |= 0x0040;  // PB7 (active HIGH)
        if (!(GPIOC->idt & (1 << 13)))  events |= 0x0400;  // PC13
        
        // ---- Phase 1: Rows=input pull-up, Cols=output LOW ----
        configure_pin_input_pullup(GPIOA, ROW_PA7);
        configure_pin_input_pullup(GPIOB, ROW_PB0);
        configure_pin_input_pullup(GPIOC, ROW_PC5);
        configure_pin_input_pullup(GPIOE, ROW_PE2);
        
        configure_pin_output_pp(GPIOA, COL_PA8);
        configure_pin_output_pp(GPIOC, COL_PC10);
        configure_pin_output_pp(GPIOE, COL_PE3);
        
        GPIOA->clr = COL_PA8;     // PA8 LOW
        GPIOC->clr = COL_PC10;    // PC10 LOW
        GPIOE->clr = COL_PE3;     // PE3 LOW
        
        // Brief delay for GPIO to settle (a few NOPs may suffice)
        __NOP(); __NOP(); __NOP(); __NOP();
        
        // Read rows
        if (!(GPIOA->idt & (1 << 7)))  input_state |= 1;  // PA7 pulled LOW
        if (!(GPIOC->idt & (1 << 5)))  input_state |= 2;  // PC5 pulled LOW
        if (!(GPIOB->idt & (1 << 0)))  input_state |= 4;  // PB0 pulled LOW
        if (!(GPIOE->idt & (1 << 2)))  input_state |= 8;  // PE2 pulled LOW
        
        // Only proceed if exactly one row is active
        if (input_state == 1 || input_state == 2 || 
            input_state == 4 || input_state == 8) {
            
            // ---- Phase 2: Swap roles ----
            configure_pin_input_pullup(GPIOA, COL_PA8);
            configure_pin_input_pullup(GPIOC, COL_PC10);
            configure_pin_input_pullup(GPIOE, COL_PE3);
            
            configure_pin_output_pp(GPIOA, ROW_PA7);
            configure_pin_output_pp(GPIOB, ROW_PB0);
            configure_pin_output_pp(GPIOC, ROW_PC5);
            configure_pin_output_pp(GPIOE, ROW_PE2);
            
            GPIOA->clr = ROW_PA7;     // PA7 LOW
            GPIOB->clr = ROW_PB0;     // PB0 LOW
            GPIOC->clr = ROW_PC5;     // PC5 LOW
            GPIOE->clr = ROW_PE2;     // PE2 LOW
            
            __NOP(); __NOP(); __NOP(); __NOP();
            
            // Read columns, dispatch based on which row was active
            if (!(GPIOE->idt & (1 << 3))) {  // PE3 = column 1
                switch (input_state) {
                    case 1: events |= 0x0080; break;
                    case 2: events |= 0x0800; break;
                    case 4: events |= 0x0100; break;
                    case 8: events |= 0x0200; break;
                }
            }
            if (!(GPIOA->idt & (1 << 8))) {  // PA8 = column 2
                switch (input_state) {
                    case 1: events |= 0x0020; break;
                    case 2: events |= 0x0010; break;
                    case 4: events |= 0x0008; break;
                    case 8: events |= 0x2000; break;
                }
            }
            if (!(GPIOC->idt & (1 << 10))) {  // PC10 = column 3
                switch (input_state) {
                    case 1: events |= 0x1000; break;
                    case 2: events |= 0x0004; break;
                    case 4: events |= 0x4000; break;
                    case 8: events |= 0x0002; break;
                }
            }
        }
        
        // ---- Debounce ----
        uint8_t evt_count = 0;
        uint8_t btn_id = 0;
        
        for (int i = 0; i < 15; i++) {
            uint16_t mask = (1 << i);
            
            if (events & mask) {
                if (g_debounce[i] < DEBOUNCE_MAX) g_debounce[i]++;
                if (g_debounce[i] == DEBOUNCE_SHORT) {
                    evt_count++;
                    btn_id = btn_map[i + 15];  // short press ID
                }
                if ((g_prev_events & mask) && g_debounce[i] == DEBOUNCE_LONG) {
                    evt_count++;
                    btn_id = btn_map[i];  // long press / repeat ID
                    g_debounce[i]--;      // stay at 71 -> fires again next tick
                }
            } else {
                if (g_debounce[i] >= 2 && g_debounce[i] <= 0x45) {
                    evt_count++;
                    btn_id = btn_map[i];  // release event
                }
                g_debounce[i] = 0;
            }
        }
        
        g_prev_events = events;
        
        if (evt_count == 1 && btn_id != 0) {
            g_last_button = btn_id;  // store for main loop to consume
        }
    }
}
```

### Important Notes for Implementation

1. **GPIOE must be clocked.** The stock firmware enables GPIOA/B/C/D/E clocks. If GPIOE clock is not enabled, PE2/PE3 reads will return 0.

2. **GPIO reconfiguration in ISR is intentional.** The stock firmware reconfigures GPIO modes every scan cycle. This is necessary because the bidirectional scan requires pins to swap between input and output roles. The AT32 GPIO configuration is fast (~20ns per pin).

3. **No delay between drive and read in stock firmware.** The stock firmware has no explicit delay between driving outputs LOW and reading inputs. The gpio_init calls themselves provide enough delay (~100ns each for 4 calls). In the recommended implementation, a few NOPs are added as a safety margin.

4. **Phase 1 uses input pull-UP (BOP), not pull-down.** The stock firmware writes to BOP to enable pull-ups on the row pins. When a button is pressed, the output (LOW) overcomes the pull-up, pulling the input LOW.

5. **Phase 2 also uses input pull-UP** (same BOP write pattern). Despite the earlier analysis suggesting pull-down, re-examination of the gpio_init wrapper (FUN_080302fc) shows that sub_mode 0x18 writes BOP = pull-up.

6. **The button_map_table at 0x08046528 is required.** Without it, you get raw bit positions (0-14) but not logical button IDs. To extract it, either dump the firmware binary from 0x08046528 (30 bytes), or do empirical mapping by pressing each button and noting which bit position lights up.

7. **Pin conflicts with SPI3:** PB0 is used for button scanning AND might overlap with other functions. Verify PB0 is not needed for SPI3 or other peripherals during button scanning.

## Extracting the Button Map Table

The table at 0x08046528 has 30 bytes (15 entries for long-press/release + 15 entries for short-press). To read it from the stock firmware binary:

```bash
# If you have the firmware binary:
xxd -s $((0x08046528 - 0x08000000)) -l 30 APP_2C53T_V120.bin
```

Update 2026-04-08:
- The downloaded vendor app image `APP_2C53T_V1.2.0_251015.bin` does **not**
  appear to contain a usable table at this location. A raw dump from that file is
  mostly zeros/sentinel-like bytes (`00 ... 0f ff ff f4 ... 04 ef`), and the
  adjacent `key_task` dispatch table at `0x08046544` also does not look like a
  valid Thumb function-pointer table.
- So for this specific vendor download, "just dump `0x08046528`" is **not**
  enough to recover the logical key mapping. A real stock dump or runtime trace
  is likely required.

Alternatively, run the scan algorithm with debug output and press each button to map bit positions to physical buttons empirically.

## References

- Decompilation: `reverse_engineering/analysis_v120/fpga_task_decompile.txt` lines 4899-5537
- gpio_init wrapper: `reverse_engineering/analysis_v120/full_decompile.c` line 22337
- TMR3 init: `reverse_engineering/analysis_v120/init_function_decompile.txt` lines 5228-5286
- TMR3 ISR: `reverse_engineering/analysis_v120/full_decompile.c` line 13772
- FPGA task analysis: `reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md`
