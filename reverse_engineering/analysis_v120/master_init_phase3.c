// =============================================================================
// FNIRSI 2C53T V1.2.0 — Master Init Phase 3: FreeRTOS Setup & FPGA Boot
// =============================================================================
//
// Source: APP_2C53T_V1.2.0_251015.bin
// Function: FUN_08023A50 (system_init), Phase 3 section
// Address range: 0x08025800 – 0x08026800 (4 KB)
// Disassembled with Capstone (ARM Thumb2)
//
// Phase 3 covers:
//   A. NVIC interrupt enable, USART2 clock, timer config   (0x08025800)
//   B. USART2 peripheral init (9600 baud for FPGA cmd)     (0x08025900)
//   C. Timer ISR config, DMA/SPI3/I2C clocks, NVIC         (0x08025A00)
//   D. SPI flash read of saved settings                     (0x08025B14)
//   E. DMA channel init, SPI3 RX buffer config              (0x08025BB4)
//   F. Queue creation (7 queues)                            (0x08025BFA)
//   G. Timer creation (2 software timers)                   (0x08025C80)
//   H. Task creation (6 tasks)                              (0x08025CBE)
//   I. Semaphore give/take to set initial states            (0x08025D76)
//   J. SPI flash config data unpack into meter_state        (0x08025D92)
//   K. Calibration table loading                            (0x080261A8)
//   L. GPIO config for SPI3 pins (PB3-PB6)                 (0x0802650A)
//   M. SPI3 peripheral init for FPGA ADC data               (0x080265CA)
//   N. SysTick delay, FPGA SPI3 handshake sequence          (0x08026638)
//
// Register conventions (inherited from Phase 1/2):
//   r5  = RCU base (0x40021018 = RCU_APB2EN)
//   fp  = USART2 base (0x40004400) or varies
//   sl  = meter_state base (0x200000F8) after 0x08025D94
//   r6  = SPI3_STAT base (0x40003C08) after SPI3 init
//   r4  = SPI flash config buffer (stack) / GPIO base
//   sb  = spi3_data_queue handle ptr (0x20002D78) in SPI3 section
//
// =============================================================================


// =============================================
// A. NVIC Interrupt Enable & Clock Gates
// Address: 0x08025800
// =============================================

void master_init_phase3(void) {

    // --- NVIC: Enable IRQ9 (EXTI9_5 or DMA CH0) ---
    // 0x08025800: movw r6, #0xE010; movt r6, #0xE000 -> r6 = SysTick base (0xE000E010)
    // 0x08025808: mov.w r0, #0x200
    // 0x08025810: str.w r0, [r6, #0xF0]   -> NVIC_ISER0 (0xE000E100) = 0x200 (bit 9)
    NVIC_ISER0 = 0x200;              // Enable IRQ #9

    // --- Enable USART2 clock (APB1) ---
    // 0x08025814: ldr r0, [r5, #4]        -> r5 = 0x40021018 (RCU_APB2EN), so [r5+4] = RCU_APB1EN
    // 0x08025818: orr r0, r0, #0x20000    -> bit 17 = USART2EN
    // 0x0802581C: str r0, [r5, #4]
    RCU_APB1EN |= (1 << 17);         // USART2 clock enable

    // --- Enable GPIOA clock ---
    // 0x0802581E: ldr r0, [r5]            -> RCU_APB2EN
    // 0x08025822: orr r0, r0, #4          -> bit 2 = IOPAEN
    // 0x08025826: str r0, [r5]
    RCU_APB2EN |= (1 << 2);          // GPIOA clock enable (ensure)

    // --- Redundant GPIOA enable ---
    // 0x08025828: ldr r0, [r5]; orr r0, #4; str r0, [r5]
    RCU_APB2EN |= (1 << 2);          // Same again (compiler artifact)

    // --- GPIO init call for USART2 TX/RX pins ---
    // 0x08025830: movw r0, #0x400; movt r0, #0x108
    //             -> GPIO config word: speed=50MHz, mode=AF push-pull
    // 0x08025838: strd sb, r0, [sp, #0x60] -> stack struct for gpio_init
    // 0x0802583E: bl 0x080302FC            -> gpio_init(GPIOA, {pin_config})
    gpio_init(GPIOA, pin_PA2_TX, AF_PUSH_PULL_50MHZ);   // PA2 = USART2_TX

    // 0x08025842-0x08025854: Second gpio_init call
    // 0x08025842: movw r0, #0x1800; movt r0, #0x100 -> input floating
    // 0x0802584E: movs r0, #8 -> pin 3 (PA3)
    // 0x08025854: bl 0x080302FC
    gpio_init(GPIOA, pin_PA3_RX, INPUT_FLOATING);        // PA3 = USART2_RX


// =============================================
// B. NVIC Priority + Timer5 Configuration
// Address: 0x08025858
// =============================================

    // --- Read NVIC priority group from SCB_AIRCR ---
    // 0x08025858: movw r0, #0xED0C; movt r0, #0xE000 -> SCB_AIRCR
    // 0x08025860: ldr r0, [r0]
    // 0x08025864: ubfx r0, r0, #8, #3    -> extract PRIGROUP (bits 10:8)
    // 0x08025868-0x0802588A: Calculate preemption/sub priority and store to NVIC IPR
    uint8_t prigroup = (SCB_AIRCR >> 8) & 0x7;
    // ... priority encoding stored to sb[0x1D] (NVIC_IPR for USART2 IRQ)

    // --- Enable NVIC IRQ #6 (DMA or EXTI) ---
    // 0x0802588E: movs r0, #0x40
    // 0x08025890: str.w r0, [r6, #0xF4]  -> NVIC_ISER1 (0xE000E104) = 0x40
    NVIC_ISER1 = 0x40;               // Enable IRQ #38 (bit 6 of ISER1)

    // --- Timer5 (TIM5) initialization ---
    // 0x08025894: movw r7, #0xC00; movt r7, #0x4000 -> r7 = 0x40000C00 = TIM5 base
    // 0x080258A0: bl 0x0802A430          -> timer_struct_init (zero out timer config)
    timer_struct_init(&timer_config);     // Clear timer config struct

    // --- Timer5 prescaler calculation from APB1 clock ---
    // 0x080258A4: ldr r0, [sp, #0x4C]    -> APB1 clock frequency
    // 0x080258A6-0x080258C8: Complex math:
    //   freq_10x = APB1_freq * 10
    //   prescaler = freq_10x / 4600 (magic constant 0x1B4E81B5)
    //   remainder = prescaler % 10
    //   if (remainder > 4) prescaler++
    //   result packed with PKHBT into timer config
    uint32_t prescaler = (APB1_freq * 10) / 4600;  // ~26 for 120MHz APB1
    if (prescaler % 10 > 4) prescaler = prescaler / 10 + 1;
    else prescaler = prescaler / 10;


// =============================================
// C. USART2 Peripheral Configuration
// Address: 0x080258E4
// =============================================

    // r4 = 0x40020444 (DMA interrupt control area)
    // fp = USART2 base (previously set, used with offsets)

    // --- USART2_CTL0: Clear word length bit ---
    // 0x080258EC: bic r0, r0, #0x1000  -> clear M bit (8-bit word)
    USART2_CTL0 &= ~(1 << 12);       // 8-bit data

    // --- USART2_CTL1: Clear stop bits ---
    // 0x080258FC: bic r0, r0, #0x3000  -> clear STOP bits (1 stop bit)
    USART2_CTL1 &= ~(3 << 12);       // 1 stop bit

    // --- USART2_CTL0: Configure bits ---
    // 0x0802590A: orr r0, r0, #8       -> TE (transmit enable)
    // 0x0802591A: orr r0, r0, #4       -> RE (receive enable)
    // 0x08025926: orr r0, r0, #0x20    -> RXNEIE (RX not empty interrupt enable)
    // 0x08025932: bic r0, r0, #0x2000  -> clear UE first
    USART2_CTL0 = (USART2_CTL0 & ~0x2000) | 0x2C;  // TE+RE+RXNEIE, UE cleared

    // --- Enable Timer5 clock ---
    // 0x0802593A: ldr r0, [r5, #4]; orr r0, #0x20; str -> RCU_APB1EN |= bit5
    RCU_APB1EN |= (1 << 5);          // TIM5EN (Timer5 clock enable)

    // --- Enable AFIO clock ---
    // 0x08025942-0x0802594A: Read 0x40020E14, set bit 1
    // This is at offset 0xBD0 from r4 base (0x40020444)
    // 0x40020E14 = AFIO_MAPR or related
    *(volatile uint32_t *)0x40020E14 |= 2;  // AFIO remap enable

    // --- Enable DMA clock ---
    // 0x0802594E: ldr r0, [r5, #4]; orr r0, #0x20000000; str
    RCU_APB1EN |= (1 << 29);         // DMA clock? (or AHB enable)

    // --- Enable GPIOA again + more clock enables ---
    // 0x08025956: ldr r0, [r5]; orr r0, #4; str
    RCU_APB2EN |= (1 << 2);          // IOPAEN

    // --- Second timer config struct + gpio_init ---
    // 0x0802595E-0x0802597A: Configure another timer (sl register = TIM base)
    // 0x0802597E-0x08025980: timer_struct_init + bl 0x0802A430
    timer_struct_init(&timer_config2);
    timer_init(TIM_base, &timer_config2);


// =============================================
// D. USART2 Baud Rate + Timer ISR Config
// Address: 0x08025984
// =============================================

    // --- meter_state base address loaded ---
    // 0x08025984: movw lr, #0xF8; movt lr, #0x2000 -> lr = 0x200000F8
    meter_state_t *meter = (meter_state_t *)0x200000F8;

    // --- Baud rate calculation for USART2 (targeting 9600 baud) ---
    // 0x0802598C: ldr.w r0, [lr, #0xE5C] -> meter[0xE5C] = clock divider
    // 0x08025990: movs r1, #0xC8          -> 200
    // 0x08025992: muls r0, r1, r0         -> freq * 200
    // 0x08025994-0x080259B6: VFP floating-point calculation:
    //   s0 = (float)(freq * 200)
    //   s4 = s0 * 10.0
    //   if (freq*200 < 4000) s0 = s4;
    //   s0 = s2 / s0
    //   result = (uint32_t)(s0 - 1.0)
    // This computes the USART BRR value for 9600 baud

    float baud_divisor = (float)timer_config_val / ((float)(clock_freq * 200));
    USART2_BRR = (uint32_t)(baud_divisor - 1.0f);

    // --- Timer prescaler for ~500Hz ISR (button scan) ---
    // 0x080259C2-0x080259D6: TIM_PSC (prescaler) and TIM_ARR (auto-reload)
    // movw r2, #0xFC2C; movt r2, #0xFFFF -> offset -0x3D4 from TIM base
    // movw r3, #0x1800; movt r3, #0x4000 -> r3 = 0x40001800 (TIM6 base?)
    // str r0, [r3, r2] -> TIM6_ARR = auto-reload value
    TIM6_ARR = prescaler;
    TIM6_PSC = baud_reg;

    // --- USART2 enable ---
    // 0x08025A00-0x08025A0A: Read CTL, set bit 0 (UE)
    USART2_CTL0 |= 1;                // UE = USART enable

    // --- DMA interrupt config ---
    // 0x08025A0C-0x08025A14: Clear DMA interrupt flags
    DMA_INTC &= ~0x70;               // Clear DMA channel flags

    // --- NVIC priority for DMA interrupt ---
    // 0x08025A18-0x08025A30: Set NVIC priority, insert into priority register
    NVIC_IPR[DMA_IRQ] = calculated_priority;


// =============================================
// E. USART2 Final Config + Timer Enable
// Address: 0x08025A32
// =============================================

    // --- USART2 control register fine-tuning ---
    // 0x08025A32-0x08025ABA: Series of bfi/orr/bic on USART registers
    // Sets: oversampling, parity, flow control, interrupt enables
    // Key settings:
    //   - DMAR (DMA receive) enable
    //   - IDLE interrupt enable
    //   - TX complete interrupt enable

    USART2_CTL0 |= (1 << 28);        // Enable something (possibly oversampling)
    USART2_CTL2 &= ~1;               // Disable CTS
    USART2_CTL2 = 0;                  // Clear flow control
    USART2_CTL2 = 0;                  // DMA regs zeroed
    USART2_CTL2 = 0;
    USART2_RTOR = 0x0F000;           // Receiver timeout

    // --- USART2 baud rate register ---
    // 0x08025ABC: movs r1, #0x64       -> 100
    // 0x08025ABE: str r1, [r4, #4]     -> BRR = 100 (but this may be timer)
    USART2_BRR = 0x64;               // Baud rate = APB1_CLK / (16 * 100) = 9600?

    // --- DMA Rx buffer config ---
    // 0x08025AC2: addw r0, lr, #0xE62  -> 0x200000F8 + 0xE62 = 0x20000F5A
    // 0x08025AC6: str r0, [r4, #0xC]   -> DMA memory address = 0x20000F5A
    DMA_CH_MADDR = &meter->usart_rx_buf;  // DMA receives into meter RX buffer

    // --- DMA channel interrupt config and enable ---
    // 0x08025AC8-0x08025ADE: Configure DMA channel
    // Interrupt priority = 6 (stored to DMA channel priority field)
    DMA_CH_CTRL |= (1 << 24);        // High priority
    // Priority field = 6
    DMA_CH_CTRL |= 1;                // Channel enable

    // --- Timer enable ---
    // 0x08025AE0-0x08025AEA: Enable timer counter
    // 0x08025AE0: ldr r0, [r4]; orr r0, #1; str -> TIMx_CR1 |= CEN
    TIMx_CR1 |= 1;                   // Counter enable

    // 0x08025AEC-0x08025AF6: Another register set
    // GPIOC port set (BOP register)
    GPIOC_BOP = (1 << 16);           // Some control pin manipulation

    // --- Timer7 enable ---
    // 0x08025AF8-0x08025B04: Enable another timer
    TIM7_CR1 |= 1;                   // Counter enable


// =============================================
// F. SPI Flash Config Read + Clock Enables
// Address: 0x08025B08
// =============================================

    // --- Enable I2C2 clock ---
    // 0x08025B08: ldr r0, [r5, #0x18]; orr r0, #0x2000000; str
    // r5+0x18 = 0x40021030 = RCU_APB1ENR2 or AHB enable
    RCU_AHB |= (1 << 25);            // I2C2 or DMA2 clock enable

    // --- Enable more peripheral clocks ---
    // 0x08025B14-0x08025B2A: Two more enables via r5+0x3C
    // 0x08025B14: ldr r0, [r5, #0x3C]; orr r0, #0x200; str -> enable bit 9
    // 0x08025B20: ldr r0, [r5, #0x3C]; orr r0, #0x100; str -> enable bit 8
    // These are likely SPI2/SPI3 or additional timer clocks
    RCU_PERIPH |= (1 << 9);          // SPI3 clock enable?
    RCU_PERIPH |= (1 << 8);          // Additional peripheral

    // --- Enable ADC clock ---
    // 0x08025B2C: ldr r0, [r5]; orr r0, #0x400000; str -> RCU_APB2EN bit 22
    RCU_APB2EN |= (1 << 22);         // ADC3 or similar

    // --- SPI flash USART baud rate (for SPI flash comm) ---
    // 0x08025B38-0x08025B4E: Write to USART5 registers
    // 0x08025B38: movw r0, #0x1F2C; str.w r0, [r2, #0x408]
    // r2 = 0x40015404 area -> USART5 registers for SPI flash
    USART5_BRR = 0x1F2C;             // SPI flash baud rate
    USART5_PSC = 0x1F40;             // Prescaler = 8000
    USART5_CR2 = 0x1F54;             // Guard time

    // --- USART5 enable (for SPI flash) ---
    // 0x08025B50-0x08025B6C: Enable USART5 TX and RX
    USART5_CTL0 |= 2;                // RE (receive enable)
    USART5_CTL0 |= 1;                // UE (USART enable)

    // --- Enable SPI3 clock (APB1) ---
    // 0x08025B70: ldr r0, [r5, #4]; orr r0, #0x800000; str
    RCU_APB1EN |= (1 << 23);         // SPI3EN (SPI3 peripheral clock)

    // --- NVIC priority for SPI3 ---
    // 0x08025B78-0x08025B8E: Configure NVIC for SPI3 IRQ
    // IRQ #20 (SPI3), bit 20 -> NVIC_ISER0 bit 20 = 0x100000
    NVIC_ISER0 = 0x100000;           // Enable SPI3 IRQ (#20)

    // --- Initialize scope/acq configuration struct ---
    // 0x08025B8A-0x08025BAE:
    // r7 = 0x20002B24 (acq_config base)
    // Store SPI3 DMA channel base (0x40005C00), display buffer ptrs
    acq_config_t *acq = (acq_config_t *)0x20002B24;
    acq->dma_base   = 0x40005C00;    // SPI3 DMA channel base
    acq->display_buf = 0x20000030;   // Display buffer A pointer
    acq->display_buf2 = 0x20000058;  // Display buffer B pointer
    acq->flag = 1;                   // [0x20002F48] = 1

    // --- SPI flash settings read ---
    // 0x08025BB4: bl 0x08039990      -> init_acq_config()
    //   This function at 0x08039990 initializes acquisition config:
    //   - Zeros out various fields
    //   - Sets up channel mapping tables (0x101, 0x202, 0x303, ... 0x707)
    //   - Configures default timebase, coupling, trigger settings
    init_acq_config(acq);

    // --- SPI flash read for saved settings ---
    // 0x08025BB8-0x08025BF0: Read saved device configuration from SPI flash
    // r0 = acq->dma_base (0x40005C00)
    // Configure DMA for SPI flash read:
    //   DMA_CTL: disable first (clear bit 0)
    //   DMA_CNT = 0
    //   DMA_PADDR = 0
    //   DMA_MADDR = 0x80 (128 bytes)
    //   DMA_CTL = 0x9E00 (mem-to-mem, 16-bit, high priority, increment)
    //   DMA_CTL &= ~2 (clear HTIE)
    //   DMA_INTF &= ~2 (clear half-transfer flag)

    // Then read actual config data:
    // SPI3_CTL1 |= 2   (enable RX DMA)
    // SPI3_CTL0 |= 2   (enable TX DMA? or similar)
    spi_flash_read_config(saved_config_buf, 128);


// =============================================
// G. FreeRTOS Queue Creation
// Address: 0x08025BFA
// =============================================

    // --- Queue 1: usart_cmd_queue ---
    // 0x08025BFA: movs r0, #0x14          -> queue_length = 20
    // 0x08025BFC: bl 0x0803AB74           -> xQueueGenericCreate(20, ?, ?)
    //   NOTE: r1 and r2 were NOT explicitly set before this call.
    //   r1 = 1 (item_size, from prior context), r2 = 0 (queue type = base)
    // 0x08025C00: movw r1, #0x2D6C; movt r1, #0x2000
    // 0x08025C08: str r0, [r1]
    *(uint32_t *)0x20002D6C = xQueueGenericCreate(
        20,     // uxQueueLength: 20 items
        1,      // uxItemSize: 1 byte per item  (inferred from r1 context)
        0       // ucQueueType: queueQUEUE_TYPE_BASE
    );  // -> usart_cmd_queue

    // --- Queue 2: secondary_cmd_queue (button events) ---
    // 0x08025C0A: movs r0, #0xF           -> queue_length = 15
    // 0x08025C0C: movs r1, #1             -> item_size = 1
    // 0x08025C0E: movs r2, #0             -> type = base queue
    // 0x08025C10: bl 0x0803AB74
    // 0x08025C14: movw r1, #0x2D70; movt r1, #0x2000; str r0, [r1]
    *(uint32_t *)0x20002D70 = xQueueGenericCreate(
        15,     // uxQueueLength: 15 items
        1,      // uxItemSize: 1 byte
        0       // ucQueueType: queueQUEUE_TYPE_BASE
    );  // -> secondary_cmd_queue (button events)

    // --- Queue 3: usart_tx_queue ---
    // 0x08025C1E: movs r0, #0xA           -> queue_length = 10
    // 0x08025C20: movs r1, #2             -> item_size = 2
    // 0x08025C22: movs r2, #0             -> type = base queue
    // 0x08025C24: bl 0x0803AB74
    // 0x08025C28: movw r1, #0x2D74; movt r1, #0x2000; str r0, [r1]
    *(uint32_t *)0x20002D74 = xQueueGenericCreate(
        10,     // uxQueueLength: 10 items
        2,      // uxItemSize: 2 bytes per item
        0       // ucQueueType: queueQUEUE_TYPE_BASE
    );  // -> usart_tx_queue

    // --- Queue 4: spi3_data_queue ---
    // 0x08025C32: movs r0, #0xF           -> queue_length = 15
    // 0x08025C34: movs r1, #1             -> item_size = 1
    // 0x08025C36: movs r2, #0             -> type = base queue
    // 0x08025C38: bl 0x0803AB74
    // 0x08025C3C: movw r1, #0x2D78; movt r1, #0x2000; str r0, [r1]
    *(uint32_t *)0x20002D78 = xQueueGenericCreate(
        15,     // uxQueueLength: 15 items
        1,      // uxItemSize: 1 byte
        0       // ucQueueType: queueQUEUE_TYPE_BASE
    );  // -> spi3_data_queue

    // --- Semaphore 1: meter_semaphore ---
    // 0x08025C46: movs r0, #1             -> queue_length = 1
    // 0x08025C48: movs r1, #0             -> item_size = 0
    // 0x08025C4A: movs r2, #3             -> type = 3 (queueQUEUE_TYPE_BINARY_SEMAPHORE)
    // 0x08025C4C: bl 0x0803AB74
    // 0x08025C50: movw r5, #0x2D7C; movt r5, #0x2000; str r0, [r5]
    *(uint32_t *)0x20002D7C = xQueueGenericCreate(
        1,      // uxQueueLength: 1
        0,      // uxItemSize: 0 (semaphore)
        3       // ucQueueType: queueQUEUE_TYPE_BINARY_SEMAPHORE
    );  // -> meter_semaphore       (r5 = &handle = 0x20002D7C)

    // --- Semaphore 2: fpga_semaphore1 ---
    // 0x08025C5A: movs r0, #1; movs r1, #0; movs r2, #3
    // 0x08025C60: bl 0x0803AB74
    // 0x08025C64: movw r6, #0x2D80; movt r6, #0x2000; str r0, [r6]
    *(uint32_t *)0x20002D80 = xQueueGenericCreate(
        1, 0, 3
    );  // -> fpga_semaphore1       (r6 = &handle = 0x20002D80)

    // --- Semaphore 3: fpga_semaphore2 ---
    // 0x08025C6E: movs r0, #1; movs r1, #0; movs r2, #3
    // 0x08025C74: bl 0x0803AB74
    // 0x08025C78: movw r8, #0x2D84; movt r8, #0x2000; str r0, [r8]
    *(uint32_t *)0x20002D84 = xQueueGenericCreate(
        1, 0, 3
    );  // -> fpga_semaphore2       (r8 = &handle = 0x20002D84)


// =============================================
// H. FreeRTOS Software Timer Creation
// Address: 0x08025C80
// =============================================

    // --- Timer1: Low-frequency housekeeping timer ---
    // 0x08025C80: movw r2, #0xB9; movt r2, #0x804 -> callback = 0x080400B9
    // 0x08025C88: addw r0, pc, #0xEB0             -> name = "Timer1" (at 0x08026B3C)
    // 0x08025C90: movs r1, #0xA                   -> period = 10 ticks (10 ms)
    // 0x08025C92: bl 0x0803BD88                   -> xTimerCreate_wrapper
    // 0x08025C96: movw r1, #0x2D88; movt r1, #0x2000; str r0, [r1]
    *(uint32_t *)0x20002D88 = xTimerCreate(
        "Timer1",           // pcTimerName
        10,                 // xTimerPeriod: 10 ticks (10 ms at 1kHz tick)
        pdTRUE,             // uxAutoReload: 1 (auto-reload, set inside wrapper)
        NULL,               // pvTimerID: NULL (set inside wrapper)
        0x080400B9          // pxCallbackFunction: Timer1 ISR callback
    );  // -> handle at 0x20002D88

    // --- Timer2: Periodic measurement/watchdog timer ---
    // 0x08025C9E: movw r2, #0x6C9; movt r2, #0x804 -> callback = 0x080406C9
    // 0x08025CA4: addw r0, pc, #0xE9C              -> name = "Timer2" (at 0x08026B44)
    // 0x08025CAC: mov.w r1, #0x3E8                 -> period = 1000 ticks (1 second)
    // 0x08025CB0: bl 0x0803BD88
    // 0x08025CB4: movw r1, #0x2D8C; movt r1, #0x2000; str r0, [r1]
    *(uint32_t *)0x20002D8C = xTimerCreate(
        "Timer2",           // pcTimerName
        1000,               // xTimerPeriod: 1000 ticks (1 second)
        pdTRUE,             // uxAutoReload
        NULL,               // pvTimerID
        0x080406C9          // pxCallbackFunction: Timer2 callback
    );  // -> handle at 0x20002D8C


// =============================================
// I. FreeRTOS Task Creation (6 tasks)
// Address: 0x08025CBE
// =============================================

    // xTaskCreate signature: (pxTaskCode, pcName, usStackDepth, pvParameters=NULL, uxPriority, pxCreatedTask)
    // In the binary: r0=taskCode, r1=name, r2=stackDepth (in words), r3=priority, [sp]=&handle
    // Note: pvParameters (NULL) is implicit in this wrapper at 0x0803B6A0

    // --- Task 1: "display" ---
    // 0x08025CBE: movw r7, #0x2D90; movt r7, #0x2000  -> handle location
    // 0x08025CC2: movw r0, #0xDA51; movt r0, #0x803    -> entry = 0x0803DA51
    // 0x08025CCE: addw r1, pc, #0xE7C                  -> name = "display"
    // 0x08025CD2: mov.w r2, #0x180                     -> stack = 384 words (1536 bytes)
    // 0x08025CD6: movs r3, #1                          -> priority = 1 (lowest)
    // 0x08025CD8: str r7, [sp]                         -> &handle on stack
    // 0x08025CDA: bl 0x0803B6A0
    xTaskCreate(
        0x0803DA51,         // display_task entry point
        "display",          // task name
        384,                // stack: 384 words = 1536 bytes
        NULL,               // parameters
        1,                  // priority: 1 (LOWEST of all tasks)
        &task_handle_display // -> 0x20002D90
    );

    // --- Task 2: "key" ---
    // 0x08025CDE: movw r7, #0x2D94; movt r7, #0x2000
    // 0x08025CE2: movw r0, #9; movt r0, #0x804         -> entry = 0x08040009
    // 0x08025CEE: addw r1, pc, #0xE64                  -> name = "key"
    // 0x08025CF2: movs r2, #0x80                       -> stack = 128 words (512 bytes)
    // 0x08025CF4: movs r3, #4                          -> priority = 4 (HIGHEST)
    // 0x08025CF8: bl 0x0803B6A0
    xTaskCreate(
        0x08040009,         // key_task entry point
        "key",              // task name
        128,                // stack: 128 words = 512 bytes
        NULL,               // parameters
        4,                  // priority: 4 (HIGHEST of all tasks)
        &task_handle_key    // -> 0x20002D94
    );

    // --- Task 3: "osc" ---
    // 0x08025CFC: movw r7, #0x2D98; movt r7, #0x2000
    // 0x08025D00: movw r0, #0x9D; movt r0, #0x804      -> entry = 0x0804009D
    // 0x08025D0C: addw r1, pc, #0xE48                  -> name = "osc"
    // 0x08025D10: mov.w r2, #0x100                     -> stack = 256 words (1024 bytes)
    // 0x08025D14: movs r3, #2                          -> priority = 2
    // 0x08025D18: bl 0x0803B6A0
    xTaskCreate(
        0x0804009D,         // osc_task entry point (oscilloscope acquisition)
        "osc",              // task name
        256,                // stack: 256 words = 1024 bytes
        NULL,               // parameters
        2,                  // priority: 2
        &task_handle_osc    // -> 0x20002D98
    );

    // --- Task 4: "fpga" ---
    // 0x08025D1C: movw r7, #0x2D9C; movt r7, #0x2000
    // 0x08025D20: movw r0, #0xE455; movt r0, #0x803    -> entry = 0x0803E455
    // 0x08025D2C: addw r1, pc, #0xE2C                  -> name = "fpga"
    // 0x08025D30: movs r2, #0x80                       -> stack = 128 words (512 bytes)
    // 0x08025D32: movs r3, #3                          -> priority = 3
    // 0x08025D36: bl 0x0803B6A0
    xTaskCreate(
        0x0803E455,         // fpga_task entry point
        "fpga",             // task name
        128,                // stack: 128 words = 512 bytes
        NULL,               // parameters
        3,                  // priority: 3
        &task_handle_fpga   // -> 0x20002D9C
    );

    // --- Task 5: "dvom_TX" ---
    // 0x08025D3A: movw r7, #0x2DA0; movt r7, #0x2000
    // 0x08025D3E: movw r0, #0xE3F5; movt r0, #0x803   -> entry = 0x0803E3F5
    // 0x08025D4A: addw r1, pc, #0xE18                  -> name = "dvom_TX"
    // 0x08025D4E: movs r2, #0x40                       -> stack = 64 words (256 bytes)
    // 0x08025D50: movs r3, #2                          -> priority = 2
    // 0x08025D54: bl 0x0803B6A0
    xTaskCreate(
        0x0803E3F5,         // dvom_tx_task entry point (meter USART transmit)
        "dvom_TX",          // task name
        64,                 // stack: 64 words = 256 bytes (smallest)
        NULL,               // parameters
        2,                  // priority: 2
        &task_handle_dvom_tx // -> 0x20002DA0
    );

    // --- Task 6: "dvom_RX" ---
    // 0x08025D58: movw r7, #0x2DA4; movt r7, #0x2000
    // 0x08025D5C: movw r0, #0xDAC1; movt r0, #0x803   -> entry = 0x0803DAC1
    // 0x08025D68: addw r1, pc, #0xE00                  -> name = "dvom_RX"
    // 0x08025D6C: movs r2, #0x80                       -> stack = 128 words (512 bytes)
    // 0x08025D6E: movs r3, #3                          -> priority = 3
    // 0x08025D72: bl 0x0803B6A0
    xTaskCreate(
        0x0803DAC1,         // dvom_rx_task entry point (meter USART receive)
        "dvom_RX",          // task name
        128,                // stack: 128 words = 512 bytes
        NULL,               // parameters
        3,                  // priority: 3
        &task_handle_dvom_rx // -> 0x20002DA4
    );


// =============================================
// J. Semaphore Initial State Setup
// Address: 0x08025D76
// =============================================

    // --- Give fpga_semaphore1 (make it available) ---
    // 0x08025D76: ldr r0, [r6]           -> r6 = 0x20002D80, load fpga_semaphore1 handle
    // 0x08025D78: movs r1, #0            -> pvItemToQueue = NULL
    // 0x08025D7A: movs r2, #0            -> xTicksToWait = 0
    // 0x08025D7C: bl 0x0803ACF0          -> xQueueGenericSend
    xQueueGenericSend(
        *(QueueHandle_t *)0x20002D80,   // fpga_semaphore1
        NULL,                            // no data (semaphore)
        0                                // no wait
    );  // xSemaphoreGive(fpga_semaphore1) — starts in "given" (available) state

    // --- Take meter_semaphore (start empty / unavailable) ---
    // 0x08025D80: ldr r0, [r5]           -> r5 = 0x20002D7C, load meter_semaphore handle
    // 0x08025D82: movs r1, #0            -> xTicksToWait = 0
    // 0x08025D84: bl 0x0803B3A8          -> xQueueReceive / xSemaphoreTake
    xSemaphoreTake(
        *(SemaphoreHandle_t *)0x20002D7C,  // meter_semaphore
        0                                   // non-blocking
    );  // Starts EMPTY — meter task must wait for data before proceeding

    // --- Take fpga_semaphore2 (start empty / unavailable) ---
    // 0x08025D88: ldr.w r0, [r8]         -> r8 = 0x20002D84, load fpga_semaphore2 handle
    // 0x08025D8C: movs r1, #0
    // 0x08025D8E: bl 0x0803B3A8
    xSemaphoreTake(
        *(SemaphoreHandle_t *)0x20002D84,  // fpga_semaphore2
        0                                   // non-blocking
    );  // Starts EMPTY — fpga task must wait for trigger before proceeding


// =============================================================================
// SEMAPHORE INITIAL STATE SUMMARY:
//   fpga_semaphore1 (0x20002D80): GIVEN    — immediately available
//   meter_semaphore (0x20002D7C): TAKEN    — starts unavailable (blocks meter)
//   fpga_semaphore2 (0x20002D84): TAKEN    — starts unavailable (blocks FPGA)
//
// This means:
//   - fpga_semaphore1 gates acquisition start: given = "go ahead and acquire"
//   - meter_semaphore gates meter data processing: blocked until USART RX data arrives
//   - fpga_semaphore2 gates secondary FPGA ops: blocked until trigger/event
// =============================================================================


// =============================================
// K. SPI Flash Config Data Unpack into meter_state
// Address: 0x08025D92
// =============================================

    // --- Read saved configuration from SPI flash buffer ---
    // r4 = pointer to stack buffer filled by spi_flash_read_config()
    // sl = 0x200000F8 (meter_state base)

    // 0x08025D92: ldr r0, [r4]           -> first word of config data
    // 0x08025D98: uxtb r1, r0            -> byte 0 = magic/version
    // 0x08025D9A: cmp r1, #0x55          -> magic = 0x55 = valid config
    // 0x08025DA0: beq -> proceed
    // 0x08025DA2: cmp r1, #0xAA          -> magic = 0xAA = factory reset
    // 0x08025DA4: bne -> 0x08026198 (use defaults)

    uint8_t magic = config_buf[0];
    if (magic == 0x55) {
        // Valid saved config — unpack all fields
    } else if (magic == 0xAA) {
        // Factory reset marker
        meter->mode_flag = 8;             // [0x20001060] = 8
        // Then fall through to unpack
    } else {
        // No valid config — skip to defaults
        meter->mode_flag = 8;
        meter->error_flag = 1;            // [0x20001058] = 1
        goto use_defaults;
    }

    // --- Unpack config bytes into meter_state fields ---
    // The config buffer is ~0x130 bytes. Each word is unpacked into
    // individual byte/halfword fields scattered across the meter_state structure.
    //
    // Key field mappings (config_buf offset -> meter_state offset from 0x200000F8):
    //
    //   config[0] bytes 1-3  -> meter[0x00..0x02]     = mode/range/coupling bytes
    //   config[1] (word)     -> meter[0x03..0x06]     = 4-byte settings block
    //   config[2] (word)     -> meter[0x07], [0x14], [0x16], [0x17]  = scattered
    //   config[3..4] (8B)    -> meter[0x18..0x1F]     = trigger/timebase config
    //   config[5..6] (8B)    -> meter[0x232..0x239]   = calibration offsets
    //   config[7] (word)     -> meter[0xE58..0xE61]   = ADC config
    //   config[8] (word)     -> meter[0xE5C]          = clock divisor
    //   config[0x2C] (word)  -> meter[0xF60]          = stored setting
    //   config[0x30] (word)  -> meter[0xF64]          = stored setting
    //
    //   config[0x38..0x48]   -> meter[0x260..0x268]   = oscilloscope calibration
    //                        -> meter[0x2D8..0x2E0]   = corresponding offsets
    //
    //   config[0x48..0x130]  -> meter[0x268..0x2D6]   = 50 pairs of 16-bit values
    //                           (scope channel gain/offset calibration, 25 per channel)
    //                        -> meter[0x2E0..0x352]   = matching offset table

    // This is a massive unroll of 50+ load-store pairs, each reading a word
    // from the config buffer, splitting it into low/high halfwords, and storing
    // them into the scope_cal[] and scope_offset[] arrays in meter_state.

    for (int i = 0; i < 50; i++) {
        uint32_t val = config_buf[0x48 + i*4];
        meter->scope_cal[i]    = (uint16_t)(val & 0xFFFF);
        meter->scope_offset[i] = (uint16_t)(val >> 16);
    }


// =============================================
// L. Calibration Table Initialization (Defaults)
// Address: 0x080261A8
// =============================================

    // If the SPI flash config had valid calibration data (check at 0x080261A8):
    //   r0 = meter[0x34E] (last calibration entry)
    //   if (r0 == 0xFFFF || r0 == 0) -> load hardcoded defaults

    // 0x080261A8: movw fp, #0x1018; movt fp, #0x4002 -> fp = RCU base area
    // 0x080261AC: movw r1, #0xFFFF
    // 0x080261B4: cmp r0, r1
    // 0x080261B6: beq -> load_defaults
    // 0x080261B8: cmp r0, #0
    // 0x080261BA: bne -> skip_to_gpio (0x0802650A)

    if (last_cal_entry == 0xFFFF || last_cal_entry == 0) {
        // --- Load hardcoded calibration defaults ---
        // 0x080261BE - 0x08026506: ~800 bytes of immediate constant loads
        // Stores hardcoded calibration values into meter_state:
        //
        // Oscilloscope channel calibration (scope_cal at meter+0x260):
        //   meter[0x260] = 0x0665_0667  (CH1 20V/div gain + offset)
        //   meter[0x264] = 0x065E_065B  (CH1 10V/div)
        //   meter[0x268] = 0x0669_065A  (CH1 5V/div)
        //   meter[0x26C] = 0x065A_065F  (CH1 2V/div)
        //   meter[0x270] = 0x0655_0657  (CH1 1V/div)
        //   meter[0x274] = 0x0663_0662  (CH1 500mV/div)
        //   meter[0x278] = 0x0659_0659  (CH1 200mV/div)
        //   meter[0x27C] = 0x0667_0657  (CH1 100mV/div)
        //   ... (continues for all voltage ranges, both channels)
        //
        // Meter calibration (scope_offset at meter+0x2D8):
        //   meter[0x2D8] = 0x064B_065D  (offset pair for 20V/div)
        //   meter[0x2DC] = 0x063C_063F  (offset pair for 10V/div)
        //   ...
        //
        // Meter range calibration constants (meter+0x29C onward):
        //   meter[0x29C] = 0x0CC4_0CCE  (meter cal constant)
        //   meter[0x2A0] = 0x0CBC       (meter cal constant)
        //   ... (about 40 calibration constants total)
        //
        // The values are ADC midpoint offsets (mostly ~0x650 = 1616 decimal)
        // and meter measurement scaling factors (mostly ~0xCB0 = 3248 decimal).

        load_hardcoded_calibration(meter);
    }


// =============================================
// M. GPIO Configuration for SPI3 (FPGA Data Bus)
// Address: 0x0802650A
// =============================================

    // --- Enable GPIOB clock (for SPI3 pins) ---
    // 0x0802650A: ldr.w r0, [fp, #4]; orr r0, #0x8000; str -> RCU_APB1EN bit 15
    // Actually fp = 0x40021018, fp+4 = RCU_APB1EN
    RCU_APB1EN |= (1 << 15);         // SPI3 or GPIOB related

    // --- Enable GPIOB APB2 clock ---
    // 0x08026518: ldr.w r0, [fp]; orr r0, #8; str -> RCU_APB2EN bit 3
    RCU_APB2EN |= (1 << 3);          // IOPBEN (GPIOB clock)
    RCU_APB2EN |= (1 << 3);          // Redundant enable

    // --- 4x gpio_init calls for SPI3 pins ---
    // r2 = 0x40010800 (GPIOB base)

    // PB3 = SPI3_SCK (AF push-pull, 50MHz)
    // 0x0802655E: movw r0, #0x1800; movt r0, #0x110
    //   -> pin config: mode=0x40 (pin 6?), speed=0x1800 | 0x110
    // 0x08026580: bl 0x080302FC
    gpio_init(GPIOB, PB3_SCK, {
        .mode  = GPIO_MODE_AF_PP,    // Alternate function push-pull
        .speed = GPIO_SPEED_50MHZ,
    });

    // PB4 = SPI3_MISO (input floating)
    // 0x08026584: movs r0, #8 (pin 4)
    // 0x08026590: strh.w r7=[0x818] -> config = input floating
    // 0x08026594: bl 0x080302FC
    gpio_init(GPIOB, PB4_MISO, {
        .mode = GPIO_MODE_INPUT,
        .pull = GPIO_NOPULL,          // Floating input
    });

    // PB5 = SPI3_MOSI (AF push-pull, 50MHz)
    // 0x08026598-0x080265A6: pin=0x10 (bit 5), config same as SCK
    gpio_init(GPIOB, PB5_MOSI, {
        .mode  = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_50MHZ,
    });

    // PB6 = SPI3_CS (GPIO output push-pull, 50MHz, software CS)
    // 0x080265AA-0x080265B6: pin=0x20 (bit 6), config=0x818 (output PP)
    gpio_init(GPIOB, PB6_CS, {
        .mode  = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_50MHZ,
    });

    // --- PC6 = FPGA SPI enable (set HIGH) ---
    // 0x080265BA-0x080265CA: Write 0x40 to GPIOC_BOP (bit 6)
    // r4 = 0x40011000 (GPIOC base), offset 0xFC10 from r4
    // Actually: 0x40011000 + 0xFFFFFC10 = 0x40010C10 = GPIOC_BOP
    GPIOC_BOP = (1 << 6);            // Set PC6 HIGH — enable FPGA SPI3


// =============================================
// N. SPI3 Peripheral Initialization
// Address: 0x080265CA
// =============================================

    // r8 = 0x40003000 (some periph base, but r8+0xC00 = 0x40003C00 = SPI3)

    // --- SPI3 init struct ---
    // 0x080265CE: mov.w r0, #0x100       -> prescaler = /4 (CLK/4)
    // 0x080265D2: str r0, [sp, #0x60]    -> config.prescaler
    // 0x080265D4: movw r0, #0x100; movt r0, #0x101
    //   -> config: CPOL=1(bit8), CPHA=1(bit0), 8-bit(bit8 of high)
    // 0x080265DC: str r0, [sp, #0x64]
    // 0x080265DE: add.w r0, r8, #0xC00   -> SPI3 base = 0x40003C00
    // 0x080265E4: bl 0x08036848          -> spi_init(SPI3, &config)
    spi_init(SPI3_BASE, {
        .prescaler = SPI_PSC_4,       // APB1_CLK / 4 = 30MHz (or /2 = 60MHz)
        .direction = SPI_DIRECTION_2LINES_FULLDUPLEX,
        .mode      = SPI_MODE_MASTER,
        .datasize  = SPI_DATASIZE_8BIT,
        .cpol      = SPI_CPOL_HIGH,   // CPOL = 1
        .cpha      = SPI_CPHA_2EDGE,  // CPHA = 1 (Mode 3)
        .nss       = SPI_NSS_SOFT,    // Software CS
        .firstbit  = SPI_FIRSTBIT_MSB,
    });

    // --- Enable SPI3 RX interrupt + TX interrupt ---
    // 0x080265E8: ldr r0, [r6, #-4]      -> r6 = 0x40003C08 (SPI3_STAT), so r6-4 = SPI3_CTL1
    // 0x080265EE: orr r0, #2; str        -> RXNEIE (RX not empty interrupt enable)
    // 0x080265F6: ldr; orr r0, #1; str   -> TXEIE (TX empty interrupt enable)
    SPI3_CTL1 |= (1 << 1);           // RXNEIE = RX buffer not empty interrupt
    SPI3_CTL1 |= (1 << 0);           // TXEIE = TX buffer empty interrupt

    // --- SPI3 enable ---
    // 0x08026604: ldr.w r0, [r8, #0xC00] -> SPI3_CTL0
    // 0x08026608: orr r0, #0x40; str     -> SPE (SPI enable)
    SPI3_CTL0 |= (1 << 6);           // SPE = SPI peripheral enable

    // --- Enable SPI3 DMA (for GPIOB) ---
    // 0x08026610-0x08026618: Another clock/peripheral enable via fp
    // fp = RCU area
    RCU_APB2EN |= (1 << 4);          // Additional enable (IOPDEN?)


// =============================================
// O. SysTick Delay (10ms) Before FPGA Handshake
// Address: 0x0802661C
// =============================================

    // --- Build a short SPI3 test transaction ---
    // 0x0802661C-0x08026634: Configure GPIO for a test sequence
    // Write to GPIOC registers:
    //   0x0802662A: bl 0x080302FC     -> gpio_init
    //   0x0802663C: str.w sb, [r4, #0x10] -> GPIOC_BOP = 0x40 (set PC6 again)

    GPIOC_BOP = (1 << 6);            // Ensure PC6 HIGH (FPGA SPI enable)

    // --- SysTick delay: 10ms ---
    // 0x08026638: movw r0, #0x2B1C; movt r0, #0x2000
    // 0x08026644: ldr r0, [r0]            -> load systick reload value
    // 0x0802664A: add.w r0, r0, r0, lsl #2 -> r0 = r0 * 5
    // 0x0802664E: lsls r0, r0, #1          -> r0 = r0 * 10 (total: reload * 10)
    // 0x08026654: str r0, [r5, #4]         -> SysTick_LOAD = reload * 10
    // 0x08026656: str r7, [r5, #8]         -> SysTick_VAL = 0
    // 0x08026658-0x08026686: Enable SysTick, poll COUNTFLAG
    //   SysTick_CTRL |= 1; while (!(SysTick_CTRL & 0x10000));
    uint32_t reload = *(uint32_t *)0x20002B1C;
    SysTick_LOAD = reload * 10;       // ~10ms delay
    SysTick_VAL  = 0;
    SysTick_CTRL |= 1;               // Enable SysTick
    while (!(SysTick_CTRL & 0x10000));  // Wait for COUNTFLAG
    SysTick_CTRL &= ~1;              // Disable SysTick
    SysTick_VAL = 0;                  // Clear counter

    // --- Second SysTick delay: 1ms ---
    // 0x08026688-0x080266DA: Same pattern with reload * 1
    SysTick_LOAD = reload * 1;        // ~1ms delay
    SysTick_VAL  = 0;
    SysTick_CTRL |= 1;
    while (!(SysTick_CTRL & 0x10000));
    SysTick_CTRL &= ~1;
    SysTick_VAL = 0;


// =============================================
// P. FPGA SPI3 Handshake Sequence
// Address: 0x080266DC
// =============================================

    // r6 = 0x40003C08 (SPI3_STAT register)
    // r6+4 = SPI3_DATA (0x40003C0C)
    // r4 = GPIO base (for CS control)
    // sb = 0x20002D78 (spi3_data_queue handle pointer)
    //
    // SPI3 handshake pattern (repeated):
    //   1. Wait SPI3_STAT bit 1 (TXE = TX empty)
    //   2. Write byte to SPI3_DATA
    //   3. Wait SPI3_STAT bit 0 (RXNE = RX not empty)
    //   4. Read byte from SPI3_DATA
    //
    // The ITTTT (if-then-then-then-then) blocks are unrolled busy-wait:
    //   ldr r0, [r6]; lsls r0, #0x1E; -> test bit 1 (TXE)
    //   itttt pl; ldr; lsls; ldr; lsls -> 3 extra checks before branch
    //   This gives 4 fast checks per loop iteration for low-latency polling.

    // --- Multi-step SysTick-timed SPI3 transaction loop ---
    // 0x080266DC-0x0802676E: Loop with SysTick delays between transactions
    // r1 = 0x32 (50), r2 = 0x64 (100) — loop counters or delay multipliers
    // ip = 0xFFFFFC10 -> offset to GPIOC BOP register
    //
    // This is a timed burst of SPI3 transactions with variable delays:
    //   for (count = 100; count > 0; count -= 50) {
    //       if (count >= 51) {
    //           SysTick_LOAD = (count - 50) * APB_freq * 50;
    //       } else {
    //           SysTick_LOAD = count * APB_freq;
    //       }
    //       // Wait for SysTick
    //       // Do SPI3 transaction
    //   }

    // --- FPGA SPI3 Handshake Transaction 1: Send 0x40, Send 0x00, Read response ---
    // 0x0802676E: movs r0, #0x40
    // 0x08026770: str.w r0, [r4, ip]    -> GPIOC_BOP? Or SPI3_DATA = 0x40
    // Wait TXE...
    // 0x0802678E: movs r0, #0
    // 0x08026790: str r0, [r6, #4]       -> SPI3_DATA = 0x00
    // Wait RXNE...
    // 0x080267AE: ldr r0, [r6, #4]       -> read SPI3_DATA (response byte)

    spi3_cs_assert();                 // CS LOW
    spi3_tx_rx(0x40);                 // Command: read FPGA status?
    uint8_t fpga_resp1 = spi3_tx_rx(0x00);  // Clock out response

    // --- Transaction 2: Send 0x40, Send 0x05, Read response ---
    // 0x080267B0: movs r0, #0x40
    // 0x080267B2: str.w r0, [r4, ip]
    // Wait TXE...
    // 0x080267D2: movs r0, #5
    // 0x080267D4: str r0, [r6, #4]       -> SPI3_DATA = 0x05
    // Wait RXNE...
    // 0x080267F2: ldr r0, [r6, #4]       -> read response

    spi3_tx_rx(0x40);                 // Re-send 0x40 (or CS re-assert)
    uint8_t fpga_resp2 = spi3_tx_rx(0x05);  // FPGA query command 0x05

    // Additional SPI3 handshake bytes follow...
    // The pattern continues with 0x00 data bytes to clock out more
    // FPGA identification/status bytes.

    // Full handshake sequence:
    //   TX: 0x40 0x00 -> RX: FPGA_ID byte 1
    //   TX: 0x40 0x05 -> RX: FPGA_ID byte 2
    //   TX: 0x40 0x00 -> RX: FPGA_ID byte 3
    //   TX: 0x40 0x00 -> RX: FPGA_ID byte 4
    // Compare responses to determine FPGA version/capabilities
    // Branch based on FPGA type (affects ADC format, sample sizes)

}   // End of master_init_phase3


// =============================================================================
// SUMMARY: COMPLETE ORDER OF OPERATIONS
// =============================================================================
//
// Phase 3 execution order within system_init() at 0x08023A50:
//
//  1. NVIC IRQ#9 enable (EXTI9_5 or DMA)
//  2. Enable USART2 clock (APB1 bit 17)
//  3. Configure PA2/PA3 as USART2 TX/RX (gpio_init x2)
//  4. Read NVIC priority group, set USART2 IRQ priority
//  5. Enable NVIC IRQ#38
//  6. Timer5 (TIM5) init + prescaler calculation from APB1 clock
//  7. USART2 peripheral config: 8N1, 9600 baud, TX+RX+RXNEIE
//  8. Enable Timer5 clock, AFIO remap, DMA clock, GPIOA clock
//  9. Timer6/7 configuration for periodic ISRs
// 10. USART2 baud rate register set
// 11. DMA channel config for USART2 RX
// 12. Timer enable (button scan 500Hz ISR)
// 13. Enable I2C2, SPI2/3, ADC3 clocks
// 14. SPI flash baud rate config (USART5 for SPI flash)
// 15. Enable SPI3 clock, NVIC for SPI3 IRQ
// 16. Initialize acq_config struct (0x20002B24)
// 17. init_acq_config() — zero and set default channel/trigger/timebase
// 18. SPI flash DMA read of saved config (~128 bytes)
// ------- QUEUES CREATED (BEFORE TASKS) -------
// 19. xQueueGenericCreate x4 (usart_cmd[20x1], button[15x1], usart_tx[10x2], spi3[15x1])
// 20. xQueueGenericCreate x3 (meter_sem[1x0], fpga_sem1[1x0], fpga_sem2[1x0]) — binary semaphores
// ------- TIMERS CREATED -------
// 21. xTimerCreate("Timer1", period=10, callback=0x080400B9)
// 22. xTimerCreate("Timer2", period=1000, callback=0x080406C9)
// ------- TASKS CREATED -------
// 23. xTaskCreate("display", 0x0803DA51, stack=384w, prio=1)
// 24. xTaskCreate("key",     0x08040009, stack=128w, prio=4)   <-- highest priority
// 25. xTaskCreate("osc",     0x0804009D, stack=256w, prio=2)
// 26. xTaskCreate("fpga",    0x0803E455, stack=128w, prio=3)
// 27. xTaskCreate("dvom_TX", 0x0803E3F5, stack=64w,  prio=2)  <-- smallest stack
// 28. xTaskCreate("dvom_RX", 0x0803DAC1, stack=128w, prio=3)
// ------- SEMAPHORE INITIAL STATES -------
// 29. xSemaphoreGive(fpga_semaphore1)    — start AVAILABLE
// 30. xSemaphoreTake(meter_semaphore, 0) — start UNAVAILABLE (blocks meter task)
// 31. xSemaphoreTake(fpga_semaphore2, 0) — start UNAVAILABLE (blocks FPGA task)
// ------- SPI FLASH CONFIG UNPACK -------
// 32. Unpack saved config into meter_state (0x200000F8) — modes, ranges, calibration
//     Magic byte check: 0x55=valid, 0xAA=factory reset, other=use defaults
// 33. If cal data invalid (0xFFFF or 0x0000): load hardcoded calibration constants
// ------- SPI3 GPIO + PERIPHERAL INIT -------
// 34. Configure PB3=SCK, PB4=MISO, PB5=MOSI (AF), PB6=CS (GPIO)
// 35. Set PC6 HIGH (FPGA SPI enable)
// 36. spi_init(SPI3): Mode 3 (CPOL=1,CPHA=1), Master, /4 prescaler, 8-bit, soft CS
// 37. Enable SPI3 RXNE + TXE interrupts
// 38. Enable SPI3 (SPE)
// ------- FPGA SPI3 HANDSHAKE -------
// 39. SysTick delay 10ms
// 40. SysTick delay 1ms
// 41. FPGA SPI3 handshake: send 0x40, 0x00, 0x05 sequence, read FPGA ID bytes
// 42. Branch based on FPGA response (determines ADC format, capabilities)
//
// ... Phase 4 continues with FPGA configuration loop, more SPI3 transactions,
//     DMA setup for bulk ADC transfers, watchdog init, and finally
//     tail-call to vTaskStartScheduler()
//


// =============================================================================
// TASK PRIORITY MAP (ascending priority, lower number = lower priority):
// =============================================================================
//
//   Priority 1: display    (0x0803DA51) — LCD rendering, lowest priority
//   Priority 2: osc        (0x0804009D) — oscilloscope acquisition processing
//   Priority 2: dvom_TX    (0x0803E3F5) — meter USART transmit commands
//   Priority 3: fpga       (0x0803E455) — FPGA SPI3/USART coordination
//   Priority 3: dvom_RX    (0x0803DAC1) — meter USART receive processing
//   Priority 4: key        (0x08040009) — button input scanning (HIGHEST)
//
// Software timers (run in timer daemon task):
//   Timer1: 10ms period, callback 0x080400B9  (housekeeping/watchdog feed)
//   Timer2: 1s period,   callback 0x080406C9  (battery monitor/auto power-off)
//


// =============================================================================
// QUEUE/SEMAPHORE MAP:
// =============================================================================
//
//   Handle Addr   Type     Length  ItemSize  Name              Purpose
//   -----------   ----     ------  --------  ----              -------
//   0x20002D6C    Queue    20      1 byte    usart_cmd_queue   FPGA commands to send
//   0x20002D70    Queue    15      1 byte    secondary_cmd     Button events
//   0x20002D74    Queue    10      2 bytes   usart_tx_queue    USART TX frame IDs
//   0x20002D78    Queue    15      1 byte    spi3_data_queue   SPI3 data triggers
//   0x20002D7C    BinSem   1       0         meter_semaphore   Gates meter data processing
//   0x20002D80    BinSem   1       0         fpga_semaphore1   Gates acquisition start
//   0x20002D84    BinSem   1       0         fpga_semaphore2   Gates FPGA secondary ops
//
// Task handle storage:
//   0x20002D88    Timer1 handle
//   0x20002D8C    Timer2 handle
//   0x20002D90    display task handle
//   0x20002D94    key task handle
//   0x20002D98    osc task handle
//   0x20002D9C    fpga task handle
//   0x20002DA0    dvom_TX task handle
//   0x20002DA4    dvom_RX task handle
//


// =============================================================================
// KEY FUNCTION ADDRESSES (FreeRTOS API in this binary):
// =============================================================================
//
//   0x0803AB74   xQueueGenericCreate(uxQueueLength, uxItemSize, ucQueueType)
//   0x0803ACF0   xQueueGenericSend(xQueue, pvItemToQueue, xTicksToWait)
//   0x0803B3A8   xQueueReceive / xSemaphoreTake(xQueue, xTicksToWait)
//   0x0803B6A0   xTaskCreate(pxTaskCode, pcName, usStackDepth, uxPriority, pxHandle)
//   0x0803BD88   xTimerCreate(pcTimerName, xTimerPeriod, pxCallbackFunction)
//                [wrapper: auto-reload=1, pvTimerID=NULL hardcoded]
//   0x080302FC   gpio_init(GPIOx, pin_config_struct)
//   0x0802A430   timer_struct_init(timer_config_struct)
//   0x08036848   spi_init(SPIx, spi_config_struct)
//   0x08039990   init_acq_config(acq_config)
//   0x08032F6C   spi_flash_read(address, buffer, length)
