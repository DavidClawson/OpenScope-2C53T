// ============================================================================
// FNIRSI 2C53T V1.2.0 — Master Init Phase 2: Peripheral Configuration
// ============================================================================
//
// Function: FUN_08023A50 (system_init / master_init)
// Full range: 0x08023A50 - 0x080276F2 (~15.4 KB)
// THIS FILE: Phase 2 — 0x08025300 to 0x080276F2
//   Peripheral init (ADC, USART, SPI, Timers, DMA, NVIC, IWDG)
//   FreeRTOS queue/task creation
//   FPGA boot command sequence
//   SPI3 handshake
//   meter_state initialization
//
// Source binary: APP_2C53T_V1.2.0_251015.bin (base 0x08000000)
// Disassembled with Capstone (ARM Thumb mode)
//
// Register context entering Phase 2:
//   r5  = RCU_APB1EN (0x4002101C) [tracked as base for clock enables]
//   r6  = varies (reloaded frequently)
//   r7  = varies (timer/peripheral base)
//   r8  = varies (GPIO/NVIC base)
//   sb  = varies
//   sl  = varies (meter_state or EXMC LCD data)
//   fp  = varies (USART2 base or stack frame)
//   lr  = varies
//
// Key peripheral addresses:
//   ADC1:   0x40012400    USART2: 0x40004400    SPI3:   0x40003C00
//   TMR3:   0x40000400    TMR5:   0x40000C00    DMA1:   0x40020000
//   IWDG:   0x40003000    NVIC:   0xE000E100    SCB:    0xE000ED00
//
// ============================================================================


// ============================================================================
// PHASE 2A: ADC1 Configuration + Calibration (0x08025300 - 0x08025470)
// ============================================================================
//
// Context: Just finished VFP calibration math (float voltage scaling).
// r4 = 0x40012408 (ADC1_CTRL2)
// r6 = meter_state_base (0x200000F8)

void phase2a_adc1_config(void) {

    // --- VFP calibration pipeline (0x08025300 - 0x080253A0) ---
    // Floating-point math to compute calibration coefficients.
    // Uses s0, s2, s4 registers with vcvt, vmul, vdiv, vadd.
    // Loads channel gain/offset pairs from meter_state_base offsets:
    //   +0x2D8, +0x2EC, +0x300, +0x314, +0x328, +0x33C
    // These are 6 calibration table pairs (per-range ADC correction).
    // Result stored via VSTR to [r8] (likely a calibration output buffer).

    // --- RCU ADC clock configuration (0x080253A0) ---
    // r1 loaded with 0x40021004 (RCU_CFG0) [via r1 offset from base]
    RCU_CFG0 &= ~0x10000000;
    // Bit 28 = ADCPSC[2] (AT32-specific). Clears high prescaler bit.
    // Combined with bits 15:14 (ADCPSC[1:0]), this sets ADC clock divider.

    // --- ADC1 register configuration (0x080253B0 - 0x08025470) ---

    // ADC1_CTRL1 (0x40012404): Scan mode, analog watchdog
    ADC1_CTRL1 &= ~0xF0000;     // Clear AWDCH[3:0] — analog watchdog channel select
    ADC1_CTRL1 |= 0x100;        // Bit 8 = SCAN — enable scan mode (multiple channels)

    // ADC1_CTRL2 (0x40012408): Alignment, triggers, enables
    ADC1_CTRL2 |= 0x02;         // Bit 1 = CONT — continuous conversion mode
    ADC1_CTRL2 &= ~0x800;       // Bit 11 = ALIGN — 0 = right-aligned data

    // ADC1_OSQ3 (0x4001242C): Regular sequence register 3 (channels 1-6)
    ADC1_OSQ3 &= ~0xF00000;     // Clear SQ6[4:0] bits 23:20 — sequence position 6

    // ADC1_SPT2 (0x40012410): Sample time register 2 (channels 0-9)
    ADC1_SPT2 |= 0x38000000;    // Bits 29:27 = SMP9[2:0] = 0b111 = 239.5 cycles
                                 // Channel 9 = PB1 = battery voltage sense!
                                 // 239.5 cycles @ ~14MHz ADC clock = ~17us per sample

    // ADC1_OSQ2 (0x40012434): Regular sequence register 2 (channels 7-12)
    // r1 = 9 (channel number)
    ADC1_OSQ2 = BFI(ADC1_OSQ2, 9, 0, 5);  // SQ7[4:0] = 9 — battery channel in position 7

    // ADC1_CTRL2 continued:
    ADC1_CTRL2 &= ~0x2000000;   // Bit 25 = SWRCST — clear software start for regular channels
    ADC1_CTRL2 |= 0xE0000;      // Bits 19:17 = ETESEL[2:0] = 0b111 = SWSTART trigger
                                 // Software-triggered conversions
    ADC1_CTRL2 |= 0x100000;     // Bit 20 = ETERC — external trigger enable for regular channels

    // ADC1 Power-on and calibration sequence:
    ADC1_CTRL2 |= 0x100;        // Bit 8 = ADCAL (DMA request... or AT32 CAL trigger?)
                                 // Actually: Bit 0 on STM32 = ADON. This is AT32-specific.
    ADC1_CTRL2 |= 0x01;         // Bit 0 = ADON — turn on ADC
    ADC1_CTRL2 |= 0x08;         // Bit 3 = RSTCAL — reset calibration (AT32: OCTEFC init)

    // --- Wait for reset calibration to complete (0x08025422 - 0x0802543C) ---
    // Polls ADC1_CTRL2 bit 3 (RSTCAL) until it clears.
    // Pattern: ldr r0,[r4]; lsls r0,r0,#0x1c; bmi (loop if bit 3 still set)
    // Uses IT blocks for tight polling (4 checks per iteration).
    do {
        // wait
    } while (ADC1_CTRL2 & 0x08);  // Wait for RSTCAL to clear

    // Start actual calibration:
    ADC1_CTRL2 |= 0x04;         // Bit 2 = CAL — start calibration

    // --- Wait for calibration to complete (0x08025450 - 0x08025468) ---
    // Polls ADC1_CTRL2 bit 2 (CAL) until it clears.
    do {
        // wait
    } while (ADC1_CTRL2 & 0x04);  // Wait for CAL to clear

    // === ADC1 SUMMARY ===
    // Channel 9 (PB1) — battery voltage sense
    // 239.5 cycle sample time (~17us)
    // Right-aligned, scan mode, continuous conversion
    // Software-triggered (SWSTART)
    // Full reset + calibration cycle performed
    // NO DMA configured for ADC (polled/interrupt-driven)
}


// ============================================================================
// PHASE 2B: EXTI3 + NVIC for USART2 RX (0x08025470 - 0x08025810)
// ============================================================================

void phase2b_exti_nvic_usart_rx(void) {

    // --- GPIO config for PA3 (USART2 RX) ---
    // At 0x08025470: gpio_init() calls to configure PA2 (TX) and PA3 (RX)
    // PA2: AF push-pull, 50MHz (USART2_TX)
    // PA3: Input floating (USART2_RX)

    // --- RCU clock enables (0x08025730-0x08025760) ---
    // Enable clocks needed for USART2 and EXTI

    // --- AFIO remap for JTAG → SPI3 (0x08025764 - 0x0802577A) ---
    AFIO_PCF0 &= ~0xF000;       // Clear SWJ_CFG[2:0] bits 15:12
    AFIO_PCF0 |= 0x2000;        // SWJ_CFG = 010 — JTAG-DP disabled, SW-DP enabled
                                 // Frees PB3 (JTCK/SPI3_SCK), PB4 (JTDI/SPI3_MISO),
                                 // PB5 (SPI3_MOSI) for SPI3 use

    // --- EXTI3 configuration (USART2 RX interrupt) ---
    // EXTI3 = PA3 (USART2 RX pin)
    EXTI_IMR &= ~0x08;          // Disable EXTI3 interrupt mask (clear first)
    EXTI_EMR &= ~0x08;          // Disable EXTI3 event mask
    EXTI_IMR |= 0x08;           // Re-enable EXTI3 interrupt mask (line 3)
    EXTI_RTSR &= ~0x08;         // Disable rising edge trigger on line 3
    EXTI_FTSR &= ~0x08;         // Disable falling edge trigger on line 3
    EXTI_RTSR |= 0x08;          // Enable rising edge trigger on EXTI3
    // EXTI3 triggers on rising edge of PA3 (USART2 RX start bit)

    // --- NVIC priority for IRQ9 (EXTI9_5) ---
    // At 0x080257D4: reads SCB_AIRCR to get priority group
    // Computes sub-priority and group priority
    // SCB_AIRCR bits [10:8] = PRIGROUP
    NVIC_IPR[9] = computed_priority;  // NVIC_IPR0+0x09 = 0xE000E409
                                       // IRQ9 = EXTI9_5 (or EXTI3 on AT32?)
    // IMPORTANT: IRQ9 on AT32F403A = EXTI line [9:5]
    // But EXTI3 needs IRQ9? Actually IRQ9 = EXTI5-9. EXTI3 = IRQ9 on STM32F1.
    // Wait — EXTI3 = IRQ9 on STM32F103, but on AT32F403A, EXTI3 = IRQ9 too.
    // Correction: EXTI0=IRQ6, EXTI1=IRQ7, EXTI2=IRQ8, EXTI3=IRQ9, EXTI4=IRQ10.

    NVIC_ISER0 = 0x200;         // Enable IRQ9 (bit 9) = EXTI3 interrupt
                                 // This enables USART2 RX byte reception via EXTI3!
                                 // The firmware uses EXTI3 (PA3 edge) instead of
                                 // USART2 RXNE interrupt — unusual but confirmed.

    // --- Enable USART2 clock (RCU_APB1EN) ---
    RCU_APB1EN |= 0x20000;      // Bit 17 = USART2EN — enable USART2 clock
    // Also: RCU_APB2EN |= 0x04 (GPIOA clock, already enabled)

    // --- GPIO config for PA2/PA3 ---
    // gpio_init(GPIOA, PA2, AF_PP_50MHz)   — USART2 TX
    // gpio_init(GPIOA, PA3, input_float)   — USART2 RX

    // --- NVIC priority for IRQ38 (USART2) ---
    NVIC_IPR[38] = computed_priority;  // 0xE000E426 = IPR[38/4] byte offset 38
                                        // IRQ38 = USART2 global interrupt

    NVIC_ISER1 = 0x40;          // Enable IRQ38 (bit 6 of ISER1, since 38-32=6)
                                 // = USART2 global interrupt
}


// ============================================================================
// PHASE 2C: TMR5 + USART2 Baud Rate (0x08025890 - 0x08025940)
// ============================================================================

void phase2c_tmr5_usart2_init(void) {

    // --- TMR5 initialization via helper function ---
    // r7 = 0x40000C00 (TMR5 base)
    // Called: FUN_0802A430(timer_config_struct) — timer init helper
    // This function configures TMR5 for frequency measurement (input capture mode).
    // TMR5 is used by the FPGA subsystem for frequency counter and period measurement.

    // --- USART2 baud rate calculation (0x080258A0 - 0x080258E0) ---
    // Complex integer math to compute BAUDR value from APB1 clock frequency.
    //
    // Algorithm (reconstructed):
    //   apb1_freq = [sp+0x4c]   (loaded from clock config, likely 120MHz)
    //   baud_input = apb1_freq * 10
    //   mantissa = baud_input / 9600  (using multiply-shift: * 0x1B4E81B5 >> 10+32)
    //   fraction = mantissa % 10      (using multiply-shift: * 0xCCCCCCCD >> 3+32)
    //   if (fraction > 4): mantissa += 1
    //
    // This computes: BAUDR = APB1_CLK / (16 * 9600)
    //   At 120MHz APB1: 120000000 / (16 * 9600) = 781.25
    //   Mantissa = 781, Fraction = 4 (of 16) → BAUDR = 0x030D | (4 << 0)
    //
    // The PKHBT instruction packs mantissa (low half) with existing high bits.
    USART2_BAUDR = computed_baudr;  // 9600 baud at APB1 clock speed

    // --- USART2 control registers ---
    USART2_CTRL1 &= ~0x1000;    // Bit 12 = M (word length) — 0 = 8 data bits
    USART2_CTRL2 &= ~0x3000;    // Bits 13:12 = STOP[1:0] — 00 = 1 stop bit
    USART2_CTRL1 |= 0x08;       // Bit 3 = TE — transmitter enable
    USART2_CTRL1 |= 0x04;       // Bit 2 = RE — receiver enable
    USART2_CTRL1 |= 0x20;       // Bit 5 = RXNEIE — RX not empty interrupt enable
                                  // Generates IRQ38 when data received!
    USART2_CTRL1 &= ~0x2000;    // Bit 13 = UE — USART disabled (will enable later)
                                  // Wait — this CLEARS UE? Unusual order.
                                  // Actually: on STM32F1, bit 13 = UE. Clearing it here
                                  // means USART2 is configured but NOT yet enabled.
                                  // It gets enabled later at 0x08026F96 (|= 0x2000).

    // --- Enable USART2 related clocks ---
    RCU_APB1EN |= 0x20;         // Bit 5 = TIM5EN? or SPI2EN? — likely another peripheral
                                  // Actually: APB1EN bit 5 = TMR5 enable (not SPI2)

    // === USART2 SUMMARY ===
    // Baud: 9600
    // Word: 8-bit, 1 stop bit, no parity
    // TX enabled, RX enabled
    // RXNE interrupt enabled (IRQ38)
    // EXTI3 also configured on PA3 for edge detection
    // USART NOT yet enabled (UE cleared) — enabled after FPGA init
}


// ============================================================================
// PHASE 2D: DMA1 + EXMC LCD DMA (0x08025940 - 0x08025BF0)
// ============================================================================

void phase2d_dma_config(void) {

    // --- DMA clock enable ---
    RCU_AHBEN |= 0x02;          // Bit 1 = DMA1EN (or DMA2EN on some parts)
                                  // AT32: AHBEN bit 1 = DMA2EN, bit 0 = DMA1EN
                                  // Need to verify which DMA controller.

    // Additional APB1 clock enables:
    RCU_APB1EN |= 0x20000000;   // Bit 29 = DAC (DAC clock enable for signal generator)

    // GPIO configuration for DAC/signal gen output pins
    // gpio_init() calls for DAC output pins (PA4, PA5 = DAC ch1, ch2)

    // --- TMR5 configuration via FUN_0802A430 (second call) ---
    // At 0x08025980: configures TMR5 with updated parameters
    // This sets up TMR5 for the frequency measurement mode.

    // --- meter_state initialization (0x08025984 - 0x08025BA0) ---
    // lr = 0x200000F8 (meter_state_base)
    //
    // Complex floating-point calculations for meter calibration:
    //   - Loads calibration data from RAM (0x20000F54 etc.)
    //   - Multiplies by 200 (0xC8), converts to float
    //   - Computes scaling factors using VFP division
    //   - Stores result as "pixels per unit" scaling factor
    //
    // This is the meter display scaling: maps ADC readings to pixel positions
    // on the bargraph/waveform display.
    //
    // Also initializes SPI flash read of calibration data:
    //   FUN_0802D8B8 (spi_flash_read_data) called to load cal tables
    //   from W25Q128 flash into RAM

    // --- DMA channel configuration (0x08025BB0 - 0x08025BF0) ---
    // r4 = 0x40020444 (DMA1_C4CTRL) — DMA1 Channel 4
    // This is the LCD framebuffer DMA channel!
    //
    // DMA1 Channel 4 configuration:
    //   Base = DMA struct at r0
    //   Disables channel first (clears bit 0 of DMA_CHCTL)
    //   Sets transfer count = 0x80 (128 halfwords)
    //   Configures as memory-to-memory (or memory-to-peripheral)
    //   Source = framebuffer in RAM
    //   Dest = EXMC data address (0x60020000)
    //
    // The DMA is used to blast pixel data to the LCD via EXMC.
    // 16-bit transfers, incrementing source, fixed destination.

    // At 0x08025BBC-0x08025BF0: configures DMA1 Channel control register
    // r0 points to DMA channel base (from FUN_08039990 return value)
    // Offset 0x40 = DMA_CHCTL (channel control)
    // Offset 0x44 = DMA_CHCNT (channel count)
    // Offset 0x4C = DMA_CHMADDR (channel memory address)
    // Offset 0x50 = DMA_CHPADDR (channel peripheral address)
    //
    // DMA_CHCTL:
    //   &= ~1 (disable channel)
    //   r8 (=0) stored to offset 0x44 (count = 0 initially)
    //   r8 (=0) stored to offset 0x50 (peripheral addr = 0 initially)
    //   0x80 stored to offset 0x4C (memory addr or count = 128)
    //
    //   |= 0x9E00 — this sets:
    //     Bit 14 = MEM2MEM (memory to memory mode)
    //     Bit 13 = PL[1] = 1 (priority level high)
    //     Bit 12 = PL[0] = 0
    //     Bit 11 = MSIZE[1] = 1 (16-bit memory)
    //     Bit 10 = MSIZE[0] = 1
    //     Bit 9  = PSIZE[1] = 1 (16-bit peripheral)
    //     Bit 8  = PSIZE[0] = 0
    //     → Memory-to-memory, High priority, 16-bit mem, 16-bit periph
    //
    //   &= ~2 — clear bit 1 (TCIE = transfer complete interrupt disabled)
    //
    // Also: offset 0x60 register configured (another DMA channel?)
    //   &= ~2 — disable transfer complete interrupt

    // === DMA SUMMARY ===
    // DMA1 Channel 4: LCD framebuffer transfer
    // Memory-to-memory mode (RAM → EXMC address space)
    // 16-bit transfers, high priority
    // Transfer complete interrupt DISABLED (polled)
    // NOT used for SPI3 (SPI3 is polled for FPGA data)
    // NOT used for ADC (ADC is software-triggered, polled)
}


// ============================================================================
// PHASE 2E: FreeRTOS Queue + Task Creation (0x08025BFC - 0x08025D90)
// ============================================================================

void phase2e_rtos_setup(void) {

    // --- Queue creation (0x08025BFC - 0x08025C7A) ---
    // FUN_0803AB74 = xQueueGenericCreate

    usart_cmd_queue    = xQueueCreate(20, 1);   // 0x20002D6C — 20 × 1-byte (FPGA commands)
    button_event_queue = xQueueCreate(15, 1);   // 0x20002D70 — 15 × 1-byte (button events)
    usart_tx_queue     = xQueueCreate(10, 2);   // 0x20002D74 — 10 × 2-byte (USART TX frames)
    spi3_data_queue    = xQueueCreate(15, 1);   // 0x20002D78 — 15 × 1-byte (SPI3 triggers)
    meter_sem          = xQueueCreate(1, 0);    // 0x20002D7C — binary semaphore
    fpga_sem1          = xQueueCreate(1, 0);    // 0x20002D80 — binary semaphore
    fpga_sem2          = xQueueCreate(1, 0);    // 0x20002D84 — binary semaphore

    // --- Task creation (0x08025C80 - 0x08025D74) ---
    //
    // Format: xTaskCreate(entry, "name", stack_words, param, priority, &handle)
    //                                                                    ↓ stored at:
    xTaskCreate(0x080400B9, "Timer1",  10,   NULL, 10,   &timer1_handle);  // 0x20002D88
    xTaskCreate(0x080406C9, "Timer2",  1000, NULL, 1000, &timer2_handle);  // 0x20002D8C
    xTaskCreate(0x0803DA51, "display", 384,  NULL, 1,    &display_handle); // 0x20002D90
    xTaskCreate(0x08040009, "key",     128,  NULL, 4,    &key_handle);     // 0x20002D94
    xTaskCreate(0x0804009D, "osc",     256,  NULL, 2,    &osc_handle);     // 0x20002D98
    xTaskCreate(0x0803E455, "fpga",    128,  NULL, 3,    &fpga_handle);    // 0x20002D9C
    xTaskCreate(0x0803E3F5, "dvom_TX", 64,   NULL, 2,    &dvom_tx_handle); // 0x20002DA0
    xTaskCreate(0x0803DAC1, "dvom_RX", 128,  NULL, 3,    &dvom_rx_handle); // 0x20002DA4

    // --- Initial semaphore / task state ---
    xQueueGenericSend(fpga_sem1, NULL, 0);  // Give initial semaphore
    vTaskSuspend(meter_sem);                 // Suspend meter semaphore task
    vTaskSuspend(fpga_sem2);                 // Suspend FPGA sem2 task

    // Task priority summary:
    //   Priority 10:   Timer1 (software timer daemon, highest)
    //   Priority 1000: Timer2 (software timer daemon — likely xTimerCreateTimerTask)
    //   Priority 4:    key (button scanning)
    //   Priority 3:    fpga, dvom_RX (FPGA comm, meter receive)
    //   Priority 2:    osc, dvom_TX (oscilloscope acquisition, meter transmit)
    //   Priority 1:    display (lowest — UI rendering yields to everything)
}


// ============================================================================
// PHASE 2F: SPI Flash Read + meter_state Populate (0x08025D90 - 0x08026540)
// ============================================================================

void phase2f_spi_flash_meter_init(void) {

    // --- SPI flash read of saved settings (0x08025D90 - 0x08025E40) ---
    // sl = 0x200000F8 (meter_state_base)
    //
    // Reads saved instrument state from SPI flash (W25Q128).
    // Validates read data (checks for 0x55 magic byte).

    // Populate meter_state structure from flash data:
    meter_state[0x00] = flash_data[0];    // 0x200000F8: mode byte
    meter_state[0x01] = flash_data[1];    // 0x200000F9: sub-mode
    meter_state[0x02] = flash_data[2];    // 0x200000FA: meter mode (passed to meter_mode_select)
    meter_state[0x03] = flash_data[3..6]; // 0x200000FB: meter range + params (32-bit)
    meter_state[0x07] = flash_data[7];    // 0x200000FF: additional param
    meter_state[0x14] = flash_data[...];  // 0x2000010C: display/view mode
    meter_state[0x16] = flash_data[...];  // 0x2000010E: channel config
    meter_state[0x17] = flash_data[...];  // 0x2000010F: trigger mode
    meter_state[0x18] = flash_data[...];  // 0x20000110: timebase
    meter_state[0x1A] = flash_data[...];  // 0x20000112: trigger level (32-bit)
    meter_state[0x2D] = flash_data[...];  // 0x20000125: additional config

    // At 0x0802554A: calls meter helper functions with loaded values
    meter_mode_select(meter_state[0x02]);  // FUN_080018A4 — sets FPGA meter mode
    meter_range_select(meter_state[0x03]); // FUN_08001A58 — sets FPGA meter range

    // --- USART TX command sequence (0x08025D96 - 0x08026540) ---
    // This is the FPGA boot command sequence.
    // Sends initialization commands via USART2 to configure the FPGA.
    //
    // Format: 10-byte frames transmitted byte-by-byte.
    //   Frame: [cmd_id, param1, param2, ..., checksum]
    //   Each byte: wait for USART2_STS bit 7 (TXE), then write to USART2_DT
    //   After full frame: wait for USART2_STS bit 6 (TC)
    //
    // Commands sent during boot (from FPGA_BOOT_SEQUENCE.md):
    //   0x01: Set oscilloscope mode
    //   0x02: Set timebase
    //   0x03: Set trigger
    //   0x04: Set vertical scale
    //   0x05: Set channel enable
    //   0x06: Set coupling
    //   0x07: Set trigger edge
    //   0x08: Set trigger level
    //   Plus meter-mode commands if meter was last active mode
    //
    // Each command uses SysTick delay between transmissions.
    // SysTick LOAD = systick_reload * N, then polls COUNTFLAG.
}


// ============================================================================
// PHASE 2G: SPI3 Init + FPGA Handshake (0x08026540 - 0x08026E00)
// ============================================================================

void phase2g_spi3_fpga_handshake(void) {

    // --- SPI3 GPIO configuration ---
    // r6 = 0x40003C08 (SPI3_STS)
    // r8 = 0x40003000 (IWDG_KR)

    // PB3 = SPI3_SCK  — AF push-pull, 50MHz
    // PB4 = SPI3_MISO — Input floating
    // PB5 = SPI3_MOSI — AF push-pull, 50MHz
    // PB6 = SPI3_CS   — GPIO output push-pull, software CS
    // (JTAG already remapped via AFIO_PCF0 |= 0x2000 above)

    // --- PC6 = FPGA SPI enable ---
    GPIOC_BSRR = (1 << 6);     // Set PC6 HIGH — enables FPGA SPI3 interface

    // --- SPI3 peripheral init via FUN_08036848 ---
    // This HAL function configures SPI3 with parameters from a struct:
    //   SPI3_CTRL1 settings (from struct at sp+0x40):
    //     Bit 15 = BIDIMODE = 0 (2-line unidirectional)
    //     Bit 14 = BIDIOE = 0
    //     Bit 13 = CRCEN = 0
    //     Bit 11 = DFF = 0 (8-bit data frame)
    //     Bit 10 = RXONLY = 0 (full duplex)
    //     Bit 9  = SSM = 1 (software slave management)
    //     Bit 8  = SSI = 1 (internal slave select)
    //     Bit 7  = LSBFIRST = 0 (MSB first)
    //     Bit 6  = SPE = 0 (not yet enabled)
    //     Bits 5:3 = BR[2:0] = prescaler
    //     Bit 2  = MSTR = 1 (master mode)
    //     Bit 1  = CPOL = 1 (clock polarity high when idle)
    //     Bit 0  = CPHA = 1 (data captured on 2nd edge)
    //   → SPI Mode 3 (CPOL=1, CPHA=1), Master, 8-bit, MSB first

    // --- SPI3 interrupt/DMA enables ---
    SPI3_CTRL2 |= 0x02;        // Bit 1 = TXEIE — TX buffer empty interrupt enable
    SPI3_CTRL2 |= 0x01;        // Bit 0 = RXNEIE — RX buffer not empty interrupt enable
    // Note: these enables may be for polling status bits, not actual interrupts.
    // The NVIC does NOT enable SPI3 interrupt in this init sequence.
    // So these flags are set but SPI3 IRQ is NOT enabled — polled mode only.

    SPI3_CTRL1 |= 0x40;        // Bit 6 = SPE — SPI enable!

    // --- PB11 = FPGA active mode ---
    // GPIOB_BSRR = (1 << 11);  // Set PB11 HIGH — tells FPGA to enter active mode
    // This is critical! Without PB11 HIGH, FPGA won't respond on SPI3.

    // --- SysTick delay (10ms) ---
    SysTick_LOAD = systick_reload * 10;  // ~10ms delay
    SysTick_VAL = 0;
    SysTick_CTRL |= 1;          // Enable SysTick
    while (!(SysTick_CTRL & 0x10000));  // Wait for COUNTFLAG

    // --- SPI3 FPGA handshake sequence (0x08026774 - 0x08026AE2) ---
    //
    // PB6 (CS) toggled LOW via GPIOB_BRR, then HIGH via GPIOB_BSRR
    // between transaction groups.
    //
    // SPI TX/RX pattern (repeated many times):
    //   1. Wait SPI3_STS bit 1 (TXE) — TX buffer empty
    //   2. Write byte to SPI3_DT
    //   3. Wait SPI3_STS bit 0 (RXNE) — RX buffer not empty
    //   4. Read response from SPI3_DT
    //
    // Transaction sequence (bytes sent → responses captured):
    //
    // Group 1 (CS LOW → CS HIGH):
    //   TX: 0x00  →  RX: FPGA status/ID byte
    //   TX: 0x05  →  RX: FPGA version/capability
    //   TX: 0x00  →  RX: (padding)
    //   TX: 0x00  →  RX: (padding)

    spi3_tx_rx(0x00);  // Query FPGA status
    spi3_tx_rx(0x05);  // Query FPGA version (cmd 0x05)
    spi3_tx_rx(0x00);  // Padding byte
    spi3_tx_rx(0x00);  // Padding byte

    // Group 2 (CS LOW → CS HIGH):
    //   TX: 0x12  →  RX: (response compared)
    //   TX: 0x00  →  RX: more FPGA data
    //   TX: 0x00  →  RX: more FPGA data
    //   TX: 0x00  →  RX: more FPGA data

    spi3_tx_rx(0x12);  // Extended query command
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);

    // Group 3 (CS LOW → CS HIGH):
    //   TX: 0x15  →  RX: response
    //   TX: 0x00  →  RX: data
    //   TX: 0x00  →  RX: data
    //   TX: 0x00  →  RX: data

    spi3_tx_rx(0x15);  // Another query
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);

    // --- SPI3 calibration data exchange (0x08026B00 - 0x08026CF2) ---
    // Sends calibration commands and receives calibration data from FPGA.
    //
    // TX: 0x3B  →  calibration-related
    // TX: 0x3A  →  calibration-related
    //
    // Loop at 0x08026B74: iterates through calibration data table,
    // sending 3 bytes per entry via SPI3, reading back 3 bytes.
    // Table at 0x0804D7C1 contains calibration coefficients.
    // The loop runs for ~0x89 (137) iterations, sending/receiving
    // calibration data in 3-byte frames.
    //
    // This calibration exchange initializes the FPGA's internal
    // ADC correction tables — the FPGA applies these corrections
    // to raw ADC samples before sending them via SPI3 bulk transfers.

    // Final handshake bytes:
    spi3_tx_rx(0x3B);  // Start calibration exchange
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);

    spi3_tx_rx(0x3A);  // Calibration data block
    spi3_tx_rx(0x00);
    spi3_tx_rx(0x00);

    // Post-handshake: reads spi3_data_queue handle from 0x20002D78
    // and signals readiness for FPGA data acquisition.

    // === SPI3 SUMMARY ===
    // Mode 3 (CPOL=1, CPHA=1), Master, 8-bit, MSB first
    // Software CS on PB6 (GPIO, not hardware NSS)
    // Clock: APB1/2 prescaler (likely 60MHz at 120MHz APB1)
    // PC6 HIGH enables FPGA SPI interface
    // PB11 HIGH enables FPGA active mode
    // NO DMA — fully polled SPI transfers
    // Handshake: queries FPGA ID (0x00, 0x05, 0x12, 0x15)
    // Calibration: 137 entries × 3 bytes exchanged via 0x3B/0x3A
}


// ============================================================================
// PHASE 2H: TMR3 Button Scan Timer (0x08026E00 - 0x08026F50)
// ============================================================================

void phase2h_tmr3_button_scan(void) {

    // r5 = 0x40000400 (TMR3_CTRL1)
    // r7 = 0x40000414 (TMR3_SWEVG)
    // r6 = 0xE000E409 (NVIC IPR base for priority config)

    // --- TMR3 period/prescaler (button scan at 500Hz) ---
    // At 0x08026EA0: TMR3 configuration
    //
    // Baud rate / prescaler calculation:
    //   Uses APB1 clock frequency from clock config struct
    //   Prescaler = (APB1_freq >> 13) + adj   (divides by 8192)
    //   Period = 0x13 (19)
    //   → Effective frequency = APB1_CLK / ((prescaler+1) * (period+1))
    //   At 120MHz APB1: 120000000 / (14648 * 20) ≈ 410 Hz
    //   But with APB1 timer doubling: 240MHz / (14648 * 20) ≈ 819 Hz
    //   The exact rate depends on the prescaler computation.
    //   Target is 500Hz (2ms period) for button matrix scanning.

    TMR3_PR = 0x13;             // Period = 19 (auto-reload value)
                                 // ISR fires every 20 timer ticks
    TMR3_DIV = prescaler;       // Prescaler computed from APB1 clock
                                 // Aims for 500Hz interrupt rate

    // --- Generate update event to load prescaler ---
    TMR3_SWEVG |= 0x01;        // Bit 0 = UG (update generation)
                                 // Forces prescaler and period to load immediately

    // --- TMR3 control register ---
    TMR3_CTRL1 &= ~0x70;       // Clear bits 6:4 = CMS[1:0]+DIR
                                 // CMS=00 (edge-aligned), DIR=0 (upcounting)

    // --- TMR3 interrupt enable ---
    TMR3_IDEN |= 0x01;         // Bit 0 = UIE — update interrupt enable
                                 // Generates interrupt on counter overflow
                                 // This is the button scan ISR trigger!

    // --- NVIC priority for TMR3 (IRQ29) ---
    // Reads SCB_AIRCR for priority grouping
    // Computes priority value with group/sub
    NVIC_IPR[29] = priority;    // 0xE000E41D = IPR[29]
                                 // IRQ29 = TIM3 global interrupt

    // --- Enable TMR3 interrupt in NVIC ---
    NVIC_ISER0 = 0x20000000;   // Bit 29 = IRQ29 = TIM3 global interrupt

    // --- TMR3 NOT started yet ---
    TMR3_CTRL1 &= ~0x01;       // Bit 0 = CEN — counter disable
                                 // Timer configured but NOT running!
                                 // It gets enabled later based on mode.

    // === TMR3 SUMMARY ===
    // Purpose: Button matrix scan ISR at ~500Hz
    // Period: 19 (auto-reload), Prescaler: computed for 500Hz
    // Mode: Edge-aligned, upcounting
    // Update interrupt enabled (UIE), routed to NVIC IRQ29
    // Timer configured but NOT started (CEN=0)
    // Started conditionally based on operating mode
}


// ============================================================================
// PHASE 2I: USART2 Enable + Task Resume (0x08026F50 - 0x08027080)
// ============================================================================

void phase2i_usart2_enable_tasks(void) {

    // --- Conditional USART2 enable based on mode ---
    // At 0x08026F50: checks meter_state byte at offset 0xF64
    // (from sl base = 0x200000F8, so address 0x2000105C)
    // This byte indicates whether meter mode was active at last shutdown.

    // If meter mode was active at shutdown:
    if (meter_state[0xF64] != 0) {
        // Loop: sends FPGA commands to restore meter state
        // Iterates through saved meter configuration,
        // sending USART commands to FPGA to restore:
        //   - Meter function (V/A/Ohm/Cap/Freq/etc.)
        //   - Meter range (auto/manual + specific range)
        //   - Display mode
        // Uses FUN_08026F5E loop with USART TX

        // Enable USART2:
        USART2_CTRL1 |= 0x2000; // Bit 13 = UE — USART enable!
                                  // NOW the USART2 is fully operational.
                                  // FPGA can start sending meter data back.

        // Resume dvom_TX task:
        vTaskResume(dvom_tx_handle);  // 0x20002DA0
        // Resume dvom_RX task:
        vTaskResume(dvom_rx_handle);  // 0x20002DA4
    }

    // --- Alternative path: oscilloscope mode ---
    // If NOT in meter mode:
    if (meter_state[0xF64] == 0) {
        // Disable USART2 (keep it off for scope mode):
        USART2_CTRL1 &= ~0x2000;  // Bit 13 = UE — USART disable
                                    // In scope mode, USART2 is not needed
                                    // until user switches to meter mode.

        // Resume oscilloscope tasks instead:
        vTaskResume(dvom_tx_handle);  // Still resume task handles
        vTaskResume(dvom_rx_handle);
    }

    // === USART2 ENABLE SUMMARY ===
    // USART2 is conditionally enabled based on last-used mode.
    // In meter mode: USART2 enabled for FPGA meter data
    // In scope mode: USART2 disabled (SPI3 used instead)
    // Tasks are resumed regardless of mode.
}


// ============================================================================
// PHASE 2J: TMR3 Start + TMR5 Freq Measurement (0x08027080 - 0x080271A0)
// ============================================================================

void phase2j_tmr3_start_tmr5_freq(void) {

    // --- TMR3 conditional start ---
    // Based on meter_state mode byte:

    // Reconfigure TMR3 period for specific mode:
    TMR3_PR = mode_specific_period;   // Depends on operating mode
    TMR3_DIV = recomputed_prescaler;  // Adjusted for mode

    TMR3_SWEVG |= 0x01;        // Generate update event
    TMR3_CNT = 0;               // Reset counter to 0
    TMR3_CTRL1 |= 0x01;        // Bit 0 = CEN — START the timer!
                                 // Button scanning begins!

    // Alternative: if in certain modes, timer stays stopped:
    TMR3_CTRL1 &= ~0x01;       // CEN = 0 — keep timer stopped

    // --- TMR5 for frequency measurement (0x08027176) ---
    // r7 = 0x40000C00 (TMR5_CTRL1)
    //
    // TMR5 is used for frequency counter mode.
    // Configured based on meter_state[0x14] (meter function):
    //   - If function <= 3: configure for frequency measurement
    //   - If function > 3: skip (not freq/period mode)

    // TMR5 configuration depends on specific meter function.
    // Period and prescaler set based on expected frequency range.
}


// ============================================================================
// PHASE 2K: IWDG Watchdog Init (0x080275A0 - 0x080275D0)
// ============================================================================

void phase2k_iwdg_init(void) {

    // r1 = 0x40003000 (IWDG_KR)

    // --- IWDG (Independent Watchdog) configuration ---
    IWDG_KR  = 0x5555;         // Unlock write access to PR and RLR registers

    IWDG_PR  = BFI(IWDG_PR, 4, 0, 3);
                                 // PR[2:0] = 4 → prescaler = /64
                                 // IWDG clock = LSI (~40kHz) / 64 = 625 Hz

    IWDG_RLR = 0x04E1;         // Reload value = 1249
                                 // Timeout = 1249 / 625 = ~2.0 seconds
                                 // (Note: previous analysis said ~5sec, but
                                 //  recalculating: 40kHz/64 = 625Hz,
                                 //  1249/625 = 2.0s. LSI variance matters.)

    IWDG_KR  = 0xAAAA;         // Reload counter (feed the dog)
    IWDG_KR  = 0xCCCC;         // Start watchdog!

    // === IWDG SUMMARY ===
    // Prescaler: /64 (from ~40kHz LSI)
    // Reload: 1249 (0x4E1)
    // Timeout: ~2 seconds (with LSI at 40kHz)
    // Must be fed regularly or MCU resets!
    // Fed every 11 calls to input_and_housekeeping (~50ms interval)
}


// ============================================================================
// PHASE 2L: Final Timer + NVIC + Scheduler Launch (0x080275D0 - 0x080276F2)
// ============================================================================

void phase2l_final_init_and_launch(void) {

    // --- TMR6/TMR7 configuration ---
    // Additional timer configuration for software timer tasks.
    // These timers support FreeRTOS software timer callbacks.

    // TMR6 clock enable:
    RCU_APB1EN |= 0x40;        // Bit 6 = TMR6EN — enable TMR6 clock

    // TMR7 NVIC priority and enable:
    NVIC_IPR[43] = priority;    // 0xE000E42B = IPR[43]
                                 // IRQ43 = TIM7 global interrupt
                                 // (Repurposed for FatFs on AT32F403A)

    NVIC_ISER1 = 0x800;        // Bit 11 = IRQ43 (32+11=43) = TIM7
                                 // Enable TIM7 interrupt

    // --- Start timer ---
    TMR_CTRL1 |= 0x01;         // CEN — enable counter

    // --- Configure FreeRTOS software timer handles ---
    // Calls timer configuration functions:
    //   FUN_0803B768 (xTimerCreate?) — create software timer
    //   FUN_0803BE38 (xTimerStart?) — start software timer
    // Applied to Timer1 and Timer2 task handles

    // --- Launch FreeRTOS scheduler ---
    // Function epilogue: pop {r4-r11, lr}
    // Tail-calls into FUN_0803A6D8 (vTaskStartScheduler)
    // This NEVER returns — FreeRTOS takes over!
    vTaskStartScheduler();
}


// ============================================================================
// COMPLETE NVIC INTERRUPT MAP (from this init function)
// ============================================================================
//
// IRQ# | Vector  | Peripheral   | Priority | ISER Bit  | Purpose
// -----|---------|-------------|----------|-----------|---------------------------
//   9  | 0x0064  | EXTI3       | custom   | ISER0[9]  | USART2 RX edge detect (PA3)
//  29  | 0x00B4  | TIM3        | custom   | ISER0[29] | Button matrix scan @ 500Hz
//  38  | 0x00D8  | USART2      | custom   | ISER1[6]  | USART2 TX/RX interrupt
//  43  | 0x00EC  | TIM7        | custom   | ISER1[11] | FatFs / software timer
//  20  | 0x0090  | EXTI15_10?  | —        | ISER0[20] | (seen at 0x08025B8E)
//
// Note: NVIC priority uses SCB_AIRCR PRIGROUP to split group/sub.
// All priorities computed dynamically from PRIGROUP setting.
//
// IRQs NOT enabled in this init:
//   - SPI3 (IRQ51) — polled, no interrupt
//   - DMA channels — transfer complete interrupt disabled
//   - ADC1 (IRQ18) — polled/software-triggered
//   - TMR5 (IRQ50) — configured but interrupt not enabled here


// ============================================================================
// COMPLETE PERIPHERAL CONFIGURATION SUMMARY
// ============================================================================
//
// ADC1:
//   - Channel 9 (PB1) = battery voltage
//   - 239.5 cycle sample time = ~17us
//   - Right-aligned, scan mode, continuous conversion
//   - Software-triggered (ETESEL = SWSTART)
//   - Full calibration cycle (RSTCAL + CAL)
//   - NO DMA — polled reads from ADC1_ODT
//
// USART2 (PA2 TX, PA3 RX):
//   - 9600 baud, 8N1 (8 data, no parity, 1 stop)
//   - TX + RX enabled
//   - RXNE interrupt enabled (IRQ38)
//   - EXTI3 also enabled on RX pin (edge detection)
//   - UE (USART enable) set conditionally by mode:
//     * Meter mode: enabled at init
//     * Scope mode: disabled at init, enabled on mode switch
//
// SPI3 (PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS):
//   - Mode 3: CPOL=1, CPHA=1
//   - Master, 8-bit, MSB first
//   - Software slave management (SSM=1, SSI=1)
//   - Software CS via PB6 GPIO
//   - Clock: APB1 prescaler (likely /2 = 60MHz)
//   - TXEIE + RXNEIE set but NO NVIC interrupt (polled)
//   - PC6 HIGH = FPGA SPI enable
//   - PB11 HIGH = FPGA active mode
//
// TMR3 (button scan):
//   - Period: 19, Prescaler: ~APB1/8192
//   - Target: 500Hz interrupt rate
//   - Update interrupt enabled (UIE), NVIC IRQ29
//   - Initially stopped (CEN=0), started by mode logic
//
// TMR5 (frequency measurement):
//   - Configured via FUN_0802A430 helper
//   - Used for freq counter / period measurement in meter mode
//   - Input capture mode on FPGA signal
//
// DMA1:
//   - Channel 4: LCD framebuffer → EXMC (memory-to-memory)
//   - 16-bit transfers, high priority
//   - Transfer complete interrupt DISABLED
//   - NOT used for SPI3 or ADC
//
// IWDG:
//   - Prescaler: /64, Reload: 1249
//   - Timeout: ~2 seconds
//   - Must be fed every ~50ms
//
// FreeRTOS:
//   - 7 queues (20×1, 15×1, 10×2, 15×1, 1×0, 1×0, 1×0)
//   - 8 tasks (Timer1, Timer2, display, key, osc, fpga, dvom_TX, dvom_RX)
//   - Scheduler launched at end of init (never returns)


// ============================================================================
// KEY FINDINGS FOR CUSTOM FIRMWARE
// ============================================================================
//
// 1. EXTI3 on PA3: Stock firmware uses BOTH EXTI3 edge interrupt AND USART2
//    RXNE interrupt for receiving FPGA data. Our firmware only uses USART2
//    RXNE. The EXTI3 may provide faster initial byte detection.
//
// 2. USART2 conditional enable: Stock firmware only enables USART2 (UE bit)
//    when in meter mode. In scope mode, USART2 is disabled to save power
//    and prevent spurious interrupts. Our firmware may want to replicate this.
//
// 3. ADC1 calibration: Stock firmware performs full RSTCAL + CAL sequence.
//    This is important for accurate battery voltage readings. Our firmware
//    should do this at boot.
//
// 4. TMR3 initially stopped: The button scan timer is configured but NOT
//    started during init. It's started conditionally based on the operating
//    mode. This means button scanning doesn't begin until the mode logic
//    decides it's needed.
//
// 5. SPI3 calibration exchange: 137 entries × 3 bytes of calibration data
//    are exchanged with the FPGA during handshake. Table at 0x0804D7C1.
//    This is separate from the USART command sequence and provides per-ADC
//    correction factors that the FPGA applies to raw samples.
//
// 6. DMA for LCD only: DMA is configured ONLY for LCD pixel blitting
//    (memory-to-memory via EXMC). SPI3 and ADC are fully polled.
//    This means SPI3 data transfer from FPGA blocks the CPU.
//
// 7. NVIC IRQ20 (EXTI15_10): Enabled at 0x08025B8E with value 0x100000
//    (bit 20). This could be for EXTI10 (PC10 button column) or another
//    edge-triggered GPIO. Needs investigation.
//
// 8. RCU_APB1EN bit 29 (DAC): DAC clock is enabled during init, confirming
//    the signal generator uses the built-in DAC (PA4/PA5 outputs).
//
// 9. Timer priority: Timer2 has priority 1000, which seems abnormally high.
//    This might be the FreeRTOS timer daemon task with configMAX_PRIORITIES.
//    Timer1 at priority 10 is likely a periodic housekeeping timer.
