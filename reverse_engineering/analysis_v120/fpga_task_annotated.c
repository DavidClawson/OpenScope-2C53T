/**
 * FPGA Task — Annotated Decompilation
 * FNIRSI 2C53T V1.2.0 Firmware
 *
 * Address range: 0x08036934 - 0x08039870
 * Total size: ~11.6 KB across 10 sub-functions
 *
 * This file contains the complete FPGA communication subsystem:
 *   - SPI flash bulk loader (filesystem access)
 *   - USART2 command dispatcher (FreeRTOS task loop)
 *   - Multimeter data processor (BCD decode + calibration)
 *   - Meter mode state machine (range/coupling/probe handling)
 *   - SPI3 ADC acquisition engine (9 capture modes, VFP calibration)
 *   - SPI3 peripheral initialization
 *   - Input scanning and housekeeping (15 buttons, watchdog, timers)
 *   - Probe change detection with auto power-off
 *   - USART TX frame builder (command serialization)
 *
 * Hardware interfaces:
 *   SPI3  (0x40003C00): PB3=SCK, PB4=MISO, PB5=MOSI, PB6=CS (GPIO)
 *   USART2 (0x40004400): PA2=TX, PA3=RX, 9600 baud, 10-byte TX / 12-byte RX
 *   GPIOB BOP/BCR: 0x40010C10/0x40010C14
 *   GPIOC IDR: 0x40011008
 *   TMR3 (0x40000400): drives USART2 exchange cycle
 *
 * Generated from Ghidra decompilation by reverse engineering analysis.
 * Original function: FUN_08036934 at 0x08036934
 */

#include <stdint.h>

/*============================================================================
 * HARDWARE REGISTER DEFINITIONS
 *============================================================================*/

/* SPI3 registers (base 0x40003C00) */
#define SPI3_CTL0       (*(volatile uint32_t *)0x40003C00)  /* Control register 0 */
#define SPI3_CTL1       (*(volatile uint32_t *)0x40003C04)  /* Control register 1 */
#define SPI3_STAT       (*(volatile uint32_t *)0x40003C08)  /* Status register */
#define SPI3_DATA       (*(volatile uint32_t *)0x40003C0C)  /* Data register */

/* SPI3_STAT bits */
#define SPI_STAT_TXE    (1 << 1)   /* TX buffer empty */
#define SPI_STAT_RXNE   (1 << 0)   /* RX buffer not empty */

/* USART2 registers (base 0x40004400) */
#define USART2_STS      (*(volatile uint32_t *)0x40004400)  /* Status register */
#define USART2_DATA     (*(volatile uint32_t *)0x40004404)  /* Data register */
#define USART2_CTRL1    (*(volatile uint32_t *)0x4000440C)  /* Control register 1 */

/* USART2_CTRL1 bits */
#define USART_CTRL1_RDBFIEN   (1 << 5)   /* RX buffer full interrupt enable */
#define USART_CTRL1_TDBEIEN   (1 << 7)   /* TX buffer empty interrupt enable */
#define USART_CTRL1_UEN       (1 << 13)  /* USART enable */

/* GPIO registers */
#define GPIOB_IDR       (*(volatile uint32_t *)0x40010C08)
#define GPIOB_BOP       (*(volatile uint32_t *)0x40010C10)  /* Bit set register */
#define GPIOB_BCR       (*(volatile uint32_t *)0x40010C14)  /* Bit clear register */
#define GPIOC_IDR       (*(volatile uint32_t *)0x40011008)

/* GPIO bit masks */
#define PB6_MASK        0x40       /* SPI3 chip select */
#define PB11_MASK       0x800      /* FPGA active mode signal */

/* Timer registers */
#define TMR3_CTL0       (*(volatile uint32_t *)0x40000400)
#define TIM5_CNT        (*(volatile uint32_t *)0x40000C24)

/* Watchdog */
#define IWDG_KR         (*(volatile uint32_t *)0x40003000)
#define IWDG_RELOAD_KEY 0xAAAA

/* Cortex-M ICSR for context switch */
#define SCB_ICSR        (*(volatile uint32_t *)0xE000ED04)
#define PENDSVSET       (1 << 28)

/*============================================================================
 * RAM ADDRESSES AND STRUCTURES
 *============================================================================*/

/* FreeRTOS queue handles (addresses of pointers to queue objects) */
#define USART_CMD_QUEUE_PTR    (*(void **)0x20002D6C)  /* USART command queue */
#define SECONDARY_CMD_QUEUE    (*(void **)0x20002D70)  /* Secondary command queue */
#define USART_TX_QUEUE         (*(void **)0x20002D74)  /* USART TX queue (2-byte items) */
#define SPI3_DATA_QUEUE        (*(void **)0x20002D78)  /* SPI3 data trigger queue */
#define METER_SEMAPHORE        (*(void **)0x20002D7C)  /* Meter RX complete semaphore */
#define FPGA_SEMAPHORE_1       (*(void **)0x20002D80)  /* Binary semaphore */
#define FPGA_SEMAPHORE_2       (*(void **)0x20002D84)  /* Binary semaphore */

/* USART TX/RX buffers */
#define USART_TX_BUF           ((volatile uint8_t *)0x20000005)  /* 10-byte TX frame */
#define USART_TX_INDEX         (*(volatile uint8_t *)0x2000000F) /* TX byte index (0-9) */
#define USART_RX_INDEX         (*(volatile uint8_t *)0x20004E10) /* RX byte index (0-11) */
#define USART_RX_BUF           ((volatile uint8_t *)0x20004E11)  /* 12-byte RX frame */

/* Global state structure base */
#define METER_STATE_BASE       ((volatile uint8_t *)0x200000F8)

/*
 * meter_state field map (offsets from 0x200000F8):
 *
 * +0x04  : int8_t  ch1_dc_offset       - Channel 1 DC offset (signed)
 * +0x05  : int8_t  ch2_dc_offset       - Channel 2 DC offset (signed)
 * +0x10  : uint8_t meter_sub_mode      - Meter sub-mode (0-7) for result formatting
 * +0x14  : uint8_t spi3_transfer_mode  - 0=idle/dummy, nonzero=active transfer
 * +0x16  : uint8_t active_channel      - Active channel selector (0 or 1)
 * +0x17  : uint8_t hold_mode           - 0=running, 1=single, 2=hold
 * +0x18  : uint8_t trigger_mode        - Trigger configuration
 * +0x1A  : int16_t trigger_offset      - Signed trigger position offset
 * +0x1C  : int16_t voltage_range       - Voltage range encoding (signed)
 * +0x28  : uint8_t button_repeat_timer - Button repeat countdown
 * +0x2A  : uint8_t display_hold_timer  - Display hold countdown
 * +0x2C  : uint8_t generic_timer       - General purpose countdown
 * +0x2D  : uint8_t timebase_index      - Timebase setting (0-0x13, index into table)
 * +0x2E  : uint8_t freq_meas_enabled   - Frequency measurement active flag
 * +0x32  : uint8_t acquisition_timer   - Acquisition timing counter (0xFF = disabled)
 * +0x33  : uint8_t calibration_mode    - 0=normal, nonzero=calibrating
 * +0x3D  : uint8_t frame_counter       - Frame counter (wraps at 99)
 * +0x45  : uint8_t freq_cycle_counter  - Frequency measurement cycle counter
 * +0x46  : uint16_t spi3_status_word   - SPI3 status readback (16-bit assembled)
 * +0x50  : uint32_t ch1_period         - Channel 1 period (ns)
 * +0x54  : uint32_t ch1_frequency      - Channel 1 frequency (Hz)
 * +0x80  : uint32_t ch2_period         - Channel 2 period (ns)
 * +0x84  : uint32_t ch2_frequency      - Channel 2 frequency (Hz)
 * +0x230 : uint8_t trigger_state_prev  - Previous trigger state
 * +0x231 : uint8_t trigger_state_curr  - Current trigger state
 * +0x352 : uint8_t ch1_probe_type      - CH1 probe type (0=none, 0xFF=none)
 * +0x353 : uint8_t ch2_probe_type      - CH2 probe type
 * +0x356 : uint8_t roll_buf_ch1[0x12D] - Roll mode circular buffer CH1
 * +0x482 : uint8_t roll_calibrated_ch1 - Roll mode calibrated sample CH1
 * +0x483 : uint8_t roll_buf_ch2[0x12D] - Roll mode circular buffer CH2
 * +0x5AF : uint8_t roll_calibrated_ch2 - Roll mode calibrated sample CH2
 * +0x5B0 : uint8_t adc_buf_ch1[0x400]  - ADC sample buffer CH1 (1024 bytes)
 * +0x9B0 : uint8_t adc_buf_ch2[0x400]  - ADC sample buffer CH2 (1024 bytes)
 * +0xDB0 : uint8_t spi3_frame_count    - SPI3 frame counter
 * +0xDB1 : uint8_t prev_input_state    - Previous input scan state
 * +0xDB2 : uint8_t curr_input_state    - Current input scan state
 * +0xDB4 : uint16_t roll_read_ptr      - Roll mode read pointer
 * +0xDB6 : uint16_t roll_sample_count  - Roll mode sample count (max 0x12C=300)
 * +0xDB8 : uint16_t acq_sample_count   - Acquisition sample counter
 * +0xDBA : uint8_t timer_dba           - Countdown timer
 * +0xDBB : uint8_t timer_dbb           - Countdown timer
 * +0xE10 : uint8_t auto_power_mode     - Auto power-off mode (2+=enabled, 0xFF=disabled)
 * +0xE24 : uint8_t power_off_counter   - Power-off tick counter (threshold: 0x65=101)
 * +0xF2D : uint8_t meter_mode_state    - Meter operating mode (0-8)
 * +0xF2F : uint8_t meter_range_state   - Meter range state
 * +0xF30 : uint32_t meter_raw_value    - Raw meter measurement value
 * +0xF34 : uint8_t meter_result_class  - Result classification (1=normal,2=under,3=over,4=invalid)
 * +0xF35 : uint8_t meter_data_valid    - Meter data validity flag
 * +0xF36 : uint8_t meter_overload_flag - Overload detection state
 * +0xF37 : uint8_t meter_cal_coeff     - Calibration coefficient selector
 * +0xF38 : uint8_t meter_range_ff      - Range marker (0xFF = unset)
 * +0xF39 : uint8_t auto_range_flag     - Auto-range enable flag
 * +0xF3A : uint16_t meter_extra_data   - Extra meter data from RX frame bytes 10-11
 * +0xF3C : uint16_t exchange_lock      - USART exchange lock (0=unlocked)
 * +0xF48 : float   meas_ch1            - Measurement result CH1 (NaN = invalid)
 * +0xF4C : float   meas_ch2            - Measurement result CH2
 * +0xF50 : float   meas_ch3            - Measurement result CH3
 * +0xF5D : uint8_t continuity_buzzer   - Continuity buzzer state (0xB0/0xB1)
 * +0xF65 : uint8_t probe_change_type   - Probe change classification (0-3)
 * +0xF68 : uint8_t mode_state          - System mode state (1=active, 2-9=mode-specific)
 * +0xF69 : uint16_t mode_flags         - Mode transition flags
 * +0xF6B : uint8_t mode_flag_b         - Mode flag byte
 * +0xF6C : uint8_t probe_change_count  - Probe change event counter
 * +0xF6F : uint8_t input_state_prev    - Previous input state snapshot
 * +0xF70 : uint8_t input_state_curr    - Current input state
 */

/* Command dispatch table in flash */
#define USART_CMD_DISPATCH_TABLE  ((void (**)(void))0x0804BE74)

/* Timebase-to-sample-count lookup table in flash */
#define TIMEBASE_SAMPLE_TABLE     ((const uint8_t *)0x0804D833)

/* Button map table in flash */
#define BUTTON_MAP_TABLE          ((const uint8_t *)0x08046528)

/*============================================================================
 * EXTERNAL FUNCTION DECLARATIONS
 *============================================================================*/

/* FreeRTOS */
extern int  xQueueReceive(void *queue, void *item, uint32_t timeout);
extern int  xQueueGenericSend(void *queue, const void *item, uint32_t timeout, int type);
extern int  xQueueSemaphoreTake(void *semaphore, uint32_t timeout);
extern int  uxQueueMessagesWaiting(void *queue);
extern void vTaskDelay(uint32_t ticks);
extern void vTaskResume(void *task);

/* SPI flash */
extern void spi_flash_read(void *buf, uint32_t addr, uint32_t len);
extern void spi_flash_write_page(void *buf, uint32_t len);

/* Math (ARM EABI soft-float for double) */
extern double __aeabi_dcmpeq(double a, double b);
extern double __aeabi_dcmplt(double a, double b);
extern double __aeabi_dmul(double a, double b);
extern double __aeabi_ddiv(double a, double b);
extern int    __aeabi_d2iz(double a);

/* Application */
extern void measurement_calc(void);  /* 0x08034078 — waveform measurement dispatch */
extern void fpga_state_update(void); /* 0x080028E0 — update FPGA state after meter frame */
extern void gpio_init(void *port, void *config);

#define portMAX_DELAY  0xFFFFFFFF
#define pdTRUE         1


/*============================================================================
 * FUNCTION 1: spi_flash_loader
 * Address: 0x08036934 - 0x08036A4E  (282 bytes)
 *
 * Reads 4KB pages from external SPI flash (Winbond W25Q128JV) into a
 * context buffer. Supports two flash regions:
 *   Region 2: offset 0x200000 (2MB, likely user data / waveform storage)
 *   Region 3: raw page address (system data / calibration)
 * Also handles FAT32 filesystem signature writes when ctx->write_flag is set.
 *============================================================================*/
void spi_flash_loader(uint8_t *ctx) {
    /* ctx layout:
     *  [0]    = region type (2 or 3)
     *  [1]    = flash region selector
     *  [2]    = continuous mode flag (2 = read next page too)
     *  [3]    = pending flag (nonzero = read needed)
     *  [4]    = write_flag (1 = write-back needed)
     *  [0x1C] = total_pages
     *  [0x24] = start_page
     *  [0x30] = current page number
     *  [0x34] = buffer start (4KB page data follows)
     */

    if (ctx[3] == 0) goto check_write;

    uint8_t region = ctx[1];
    uint32_t page = *(uint32_t *)(ctx + 0x30);
    uint32_t flash_addr;

    if (region == 3) {
        flash_addr = page << 12;                      /* Region 3: direct page address */
    } else if (region == 2) {
        flash_addr = 0x200000 + (page << 12);        /* Region 2: 2MB offset */
    } else {
        goto done;
    }

    spi_flash_read(ctx + 0x34, flash_addr, 0x1000);  /* Read 4KB page */

    /* Check if more pages remain */
    uint32_t start_page = *(uint32_t *)(ctx + 0x24);
    uint32_t total_pages = *(uint32_t *)(ctx + 0x1C);
    uint32_t remaining = page - start_page;
    ctx[3] = 0;  /* Clear pending flag */
    if (remaining >= total_pages) goto check_write;

    /* Continuous mode: also read the next page */
    if (ctx[2] == 2) {
        uint32_t next_page = total_pages + page;
        if (region == 3)
            flash_addr = next_page << 12;
        else if (region == 2)
            flash_addr = 0x200000 + (next_page << 12);
        spi_flash_read(ctx + 0x34, flash_addr, 0x1000);
    }

check_write:
    /* FAT32 filesystem write-back (creates valid FAT boot sector) */
    if (ctx[0] == 3 && ctx[4] == 1) {
        spi_flash_write_page(ctx + 0x38, 0xFFC);

        /* Write FAT32 signatures */
        *(uint32_t *)(ctx + 0x34) = 0x41615252;       /* "RRaA" — FSInfo signature */
        *(uint16_t *)(ctx + 0x34 + 0x1FE) = 0xAA55;   /* Boot sector marker */
        *(uint32_t *)(ctx + 0x34 + 0x1E4) = 0x61417272; /* "rrAa" — FSInfo struct */

        ctx[4] = 0;  /* Clear write flag */
    }

done:
    return;
}


/*============================================================================
 * FUNCTION 2: usart_cmd_dispatcher
 * Address: 0x08036A50 - 0x08036ABC  (108 bytes)
 *
 * FreeRTOS task entry point for the USART command processing loop.
 * Receives command bytes from the USART command queue and dispatches
 * them through a function pointer table in flash (0x0804BE74).
 *
 * When idle, checks if the SPI3 queue is empty and the meter is in
 * mode 2 (active measurement); if so, sends a keepalive to prevent
 * the SPI3 acquisition task from starving.
 *============================================================================*/
void usart_cmd_dispatcher(void) {
    uint8_t cmd_byte;

    while (1) {
        /* Try non-blocking receive first */
        if (xQueueReceive(USART_CMD_QUEUE_PTR, &cmd_byte, 0) == pdTRUE) {
            /* Dispatch through command table */
            void (*handler)(void) = USART_CMD_DISPATCH_TABLE[cmd_byte];
            handler();
            continue;
        }

        /* Keepalive: if SPI3 queue empty AND meter in active mode, poke queue */
        if (uxQueueMessagesWaiting(METER_SEMAPHORE) == 0) {
            volatile uint8_t *ms = METER_STATE_BASE;
            if (ms[0xF68] == 2) {
                xQueueGenericSend(METER_SEMAPHORE, NULL, 0, 0);
            }
        }

        /* Block until next command arrives */
        if (xQueueReceive(USART_CMD_QUEUE_PTR, &cmd_byte, portMAX_DELAY) == pdTRUE) {
            void (*handler)(void) = USART_CMD_DISPATCH_TABLE[cmd_byte];
            handler();
        }
    }
}


/*============================================================================
 * FUNCTION 3: meter_data_processor
 * Address: 0x08036AC0 - 0x080371B0  (1776 bytes)
 *
 * Processes USART2 RX frames from FPGA for multimeter readings.
 * The FPGA sends 12-byte frames (0x5A 0xA5 header + 10 data bytes).
 * Bytes [2..7] contain BCD-encoded measurement digits.
 *
 * Processing pipeline:
 *   1. Wait on meter_semaphore (blocks until USART ISR signals frame ready)
 *   2. Extract nibble pairs from RX frame bytes
 *   3. Look up digits via command_lookup_table (maps raw nibbles to 0-9 + specials)
 *   4. Detect special patterns: OL (overload), blank, continuity, mode change
 *   5. Assemble 4-digit BCD value into integer
 *   6. Apply calibration: double-precision FP division/multiplication
 *   7. Detect polarity, apply range scaling, clamp
 *   8. Classify result: 1=normal, 2=underrange, 3=overrange, 4=invalid
 *   9. Format based on meter sub-mode (0-7): add decimal offset, etc.
 *
 * Uses VFP single-precision for final result and ARM EABI double-precision
 * soft-float calls for calibration math (__aeabi_ddiv, __aeabi_dmul, etc.)
 *
 * Special digit codes from lookup table:
 *   0x0A, 0x0B = "OL" (overload indication)
 *   0x0E       = special zero display
 *   0x10       = blank digit
 *   0x11       = partial blank
 *   0x12, 0x13 = mode indicators
 *   0xFF       = invalid/unrecognized nibble
 *============================================================================*/
void meter_data_processor(void) {
    volatile uint8_t *ms = METER_STATE_BASE;
    volatile uint8_t *rx = USART_RX_BUF;
    uint8_t prev_digit_count = 0;

    /* Preload VFP calibration constants from literal pool:
     *   d8, d9  = measurement reference values
     *   d11     = range constant
     *   d12     = overload threshold high
     *   d13     = overload threshold low
     */

    while (1) {
        /* Block until USART ISR signals a complete RX frame */
        xQueueSemaphoreTake(METER_SEMAPHORE, portMAX_DELAY);

        if (ms[0xF35] == 0) goto calibration_path;

        /*
         * BCD digit extraction from USART RX frame
         * Frame bytes [2..6] are packed nibble pairs.
         * Each pair is looked up to get a decimal digit (0-9) or special code.
         */
        uint8_t b2 = rx[2], b3 = rx[3], b4 = rx[4], b5 = rx[5], b6 = rx[6];

        /* Extract cross-byte nibble pairs and look up */
        uint8_t digit0 = /* lookup */((b2 & 0xF0) | (b3 & 0x0F));
        uint8_t digit1 = /* lookup */((b3 & 0xF0) | (b4 & 0x0F));
        uint8_t digit2 = /* lookup */((b4 & 0xF0) | (b5 & 0x0F));
        uint8_t digit3 = /* lookup */((b5 & 0xF0) | (b6 & 0x0F));

        /* --- Special value detection --- */

        if (digit0 == 0x0A && digit1 == 0x0B) {
            /* "OL" — overload indication */
            goto store_result;
        }

        if (digit0 == 0x10 && digit1 == 0x10) {
            /* Blank display (no measurement) */
            goto store_result;
        }

        if (digit0 == 0x10 && digit1 == 0x11) {
            /* Partial blank */
            goto store_result;
        }

        if (digit1 == 0x12 && digit2 == 0x0A) {
            /* Continuity test pattern */
            if (digit3 == 5 && ms[0xF5D] != 0xB0) {
                ms[0xF5D] = 0xB1;    /* Trigger continuity buzzer */
                ms[0xF2D] = 1;
            }
            goto store_result;
        }

        if (digit0 == 0xFF || digit1 == 0xFF || digit2 == 0xFF) {
            /* Invalid digits — skip */
            goto store_result;
        }

        /* --- Assemble 4-digit BCD value --- */
        int d0 = (digit0 >= 0x10) ? (digit0 - 0x10) : digit0;
        int d1 = (digit1 >= 0x10) ? (digit1 - 0x10) : digit1;
        int d2 = (digit2 >= 0x10) ? (digit2 - 0x10) : digit2;
        int d3 = (digit3 >= 0x10) ? (digit3 - 0x10) : digit3;
        int raw_value = d0 * 1000 + d1 * 100 + d2 * 10 + d3;

calibration_path:
        /*
         * Heavy floating-point calibration section.
         *
         * For each measurement channel (up to 4 iterations):
         *   1. Check polarity via __aeabi_dcmplt
         *   2. Convert calibration coefficient to double: __aeabi_ui2d(cal_coeff)
         *   3. Divide reference by coefficient: __aeabi_ddiv(d8_ref, divisor)
         *   4. Multiply by accumulated scale: __aeabi_dmul(quotient, d10_accum)
         *   5. Convert to int: __aeabi_d2iz(result)
         *
         * Digit count tracking: counts how many significant digits by
         * repeatedly multiplying by the reference until result == 0.
         * Stored as the number of decimal places for display formatting.
         *
         * Final classification stored at ms[0xF34]:
         *   1 = normal reading
         *   2 = underrange (value too small for current range)
         *   3 = overrange (exceeds display but not OL)
         *   4 = invalid measurement
         */

        /* [~150 lines of VFP + EABI double-precision math omitted for clarity] */

        /* Result formatting switch on meter_sub_mode (0-7):
         *   case 0: digit_count + 0x0A  (voltage DC)
         *   case 1: check polarity -> +2 (voltage AC)
         *   case 2: check flags -> +2 (current)
         *   case 3: 0xFF (invalid/unused)
         *   case 4: dual-channel conditional offset (resistance)
         *   case 5: digit_count + 2 (capacitance)
         *   case 6: digit_count + 0x0A (frequency)
         *   case 7: special handling (diode/continuity)
         */

store_result:
        ms[0xF30] = 0;  /* Clear raw value for next cycle */
    }
}


/*============================================================================
 * FUNCTION 4: meter_mode_handler
 * Address: 0x080371B0 - 0x080373A8  (504 bytes)
 *
 * Processes USART RX frame status bits to determine meter operating mode.
 * Reads bytes [6] and [7] of the USART RX frame for status/flag bits.
 *
 * Implements an 8-state machine (ms[0xF2D], values 0-8) that tracks:
 *   - AC/DC coupling mode
 *   - Auto-range vs manual range
 *   - Overload conditions
 *   - Probe detection
 *   - Range changes
 *
 * State transitions driven by individual bits in status_byte and flags_byte.
 *============================================================================*/
void meter_mode_handler(void) {
    volatile uint8_t *ms = METER_STATE_BASE;
    volatile uint8_t *rx = USART_RX_BUF;

    uint8_t mode_state = ms[0xF2D];
    if (mode_state > 7) goto done;

    uint8_t status_byte = rx[7];   /* FPGA status flags */
    uint8_t flags_byte  = rx[6];   /* FPGA measurement flags */

    /* State machine (TBB switch on mode_state): */
    switch (mode_state) {

    case 0: /* IDLE / INIT */
        if (ms[0xF2F] == 0) {
            /* First entry after reset */
            ms[0xF2D] = 0;
            break;
        }
        /* Check status bits for AC mode, auto-range */
        if (status_byte & (1 << 5)) {
            /* Bit 5 set: enter AC mode path */
            ms[0xF36] = 0;             /* Clear overload flag */
            /* Extract bit 6 of flags_byte as cal_coeff */
            ms[0xF37] = (flags_byte >> 6) & 1;
            ms[0xF2F] = 3;             /* Advance to state 3 */
            break;
        }
        if (status_byte & (1 << 1)) {
            /* Bit 1: set overload flag = 1 */
            ms[0xF36] = 1;
            ms[0xF37] = (~status_byte) & 1;  /* Invert bit 0 */
            ms[0xF2F] = 1;
            break;
        }
        if (status_byte & (1 << 0)) {
            /* Bit 0: set overload flag = 2 */
            ms[0xF36] = 2;
            ms[0xF37] = 1;
            ms[0xF2F] = 1;
            break;
        }
        /* Normal: check bit 3 for auto-range trigger */
        if (status_byte & (1 << 3)) {
            /* Auto-range path */
            ms[0xF36] = 2;
            ms[0xF37] = (flags_byte >> 6) & 1;
            ms[0xF2F] = 2;
        } else {
            ms[0xF36] = 0;
        }
        break;

    case 1: /* POLARITY CHECK */
        if (status_byte & 1) {
            ms[0xF37] = 0;   /* Clear cal coefficient */
        } else {
            ms[0xF37] = 1;   /* Set cal coefficient */
        }
        break;

    case 2: /* OVERLOAD / RANGE CHANGE */
        if (ms[0xF36] == 1) {
            if (status_byte & 1) {
                /* Overload confirmed */
                ms[0xF37] = 0;
                ms[0xF2D] = 2;
            } else {
                ms[0xF37] = 1;
                ms[0xF2D] = 2;
            }
        } else {
            /* Check bit 3 for range change */
            if (status_byte & (1 << 3)) {
                /* Range change path */
            }
            ms[0xF2D] = 2;
        }
        break;

    case 3: /* AC/DC FLAG CHECK */
        if (status_byte & (1 << 2)) {
            /* AC mode detected */
            ms[0xF37] = 2;
            ms[0xF2D] = 3;
        } else {
            /* Check hold flag (flags_byte bit 6) */
            ms[0xF37] = (flags_byte >> 6) & 1;
            ms[0xF2D] = 3;
        }
        break;

    case 4: /* RANGE INDICATOR CHECK */
        if (flags_byte & (1 << 5)) {
            ms[0xF37] = 0;   /* Range indicator 1 */
            ms[0xF2D] = 4;
        } else if (flags_byte & (1 << 4)) {
            ms[0xF37] = 1;   /* Range indicator 2 */
            ms[0xF2D] = 4;
        } else {
            /* Check status_byte bit 0 for state selection */
            ms[0xF37] = (status_byte & 1) ? 2 : 3;
            ms[0xF2D] = 4;
        }
        break;

    case 5: /* AUTO-RANGE */
        if (ms[0xF39] == 0) {
            ms[0xF37] = 0;
        } else {
            ms[0xF37] = 4;
        }
        ms[0xF2D] = 5;
        break;

    case 6: /* STANDBY */
    case 7: /* STANDBY */
        /* Check flags_byte bit 4 for standby condition */
        if (!(flags_byte & (1 << 4))) {
            if (status_byte & 1) {
                ms[0xF37] = 2;
            } else {
                ms[0xF37] = 1;
            }
        } else {
            ms[0xF37] = 0;
        }
        break;
    }

    /* Post-processing: if overload flag == 2 and specific conditions met,
     * extract 16-bit value from RX frame bytes [10:11] (big-endian) */
    if (ms[0xF36] == 2) {
        uint8_t range = ms[0xF2F];
        uint8_t state = ms[0xF2D];
        if ((range ^ 1) == 0 || state == 1) {
            /* Extract big-endian 16-bit from rx[10..11] */
            uint16_t extra = (rx[10] << 8) | rx[11];
            *(uint16_t *)(ms + 0xF3A) = extra;
        }
    }

    /* Update state and signal processing task */
    fpga_state_update();

done:
    /* If mode requires semaphore signal, post to meter task */
    if (ms[0xF2D] <= 8) {
        /* Check if modes 0 or 8 (bits in mask 0x105) need semaphore */
        uint32_t mask = 1 << ms[0xF2D];
        if (mask & 0x105) {
            /* Signal meter_data_processor to wake up */
        }
    }
}


/*============================================================================
 * FUNCTION 4b: meter_result_enqueue
 * Address: 0x080373A8 - 0x08037428  (128 bytes)
 *
 * NOTE: This section contains the literal pool (FP constants) for
 * meter_data_processor and meter_mode_handler, followed by the
 * USART TX frame builder task entry.
 *============================================================================*/

/* --- Literal pool data (0x080373CC - 0x080373F0) ---
 * VFP double constants used by meter calibration:
 *   0x080373CC: 0x00000000 0x00000000  (0.0)
 *   0x080373D4: 0x00000000 0x00000000  (0.0)
 *   0x080373DC: 0x00000000 0x407EF000  (494.0 — likely cal reference)
 *   0x080373E4: 0x00000000 0x00000000  (0.0)
 *   0x080373EC: 0x00000000 0x43100000  (4503599627370496.0 — 2^52)
 */


/*============================================================================
 * FUNCTION 5: usart_tx_frame_builder (dvom_TX task)
 * Address: 0x080373F4 - 0x08037454  (96 bytes)
 *
 * FreeRTOS task that receives 2-byte command items from USART_TX_QUEUE,
 * formats them into the 10-byte USART TX frame, and initiates transmission
 * by enabling the TX interrupt.
 *
 * TX frame format (10 bytes at 0x20000005):
 *   [0],[1]: Header bytes (pre-set, likely 0x00 from BSS init)
 *   [2]: Command high byte (from queue item high byte)
 *   [3]: Command low byte / echo byte (from queue item low byte)
 *   [4]-[8]: Parameters (context-dependent, set by callers)
 *   [9]: Checksum = (cmd_high + cmd_low) & 0xFF
 *
 * After loading the frame, sets TDBEIEN bit in USART2_CTRL1 to trigger
 * the TX interrupt pump (usart2_irq_handler sends bytes 0-9 sequentially).
 * Then delays 10 ticks (10ms) before accepting next command.
 *============================================================================*/
void usart_tx_frame_builder(void) {
    volatile uint8_t *tx_buf = USART_TX_BUF;
    uint16_t cmd_item;

    while (1) {
        /* Block until command available (2-byte queue items) */
        xQueueReceive(USART_TX_QUEUE, &cmd_item, portMAX_DELAY);

        /* Reset TX byte index to 0 (ISR will pump from byte 0) */
        USART_TX_INDEX = 0;

        /* Fill TX frame fields */
        uint8_t cmd_low  = cmd_item & 0xFF;
        uint8_t cmd_high = (cmd_item >> 8) & 0xFF;
        tx_buf[3] = cmd_low;                          /* Echo byte */
        tx_buf[2] = cmd_high;                         /* Command high */
        tx_buf[9] = (cmd_item + cmd_high) & 0xFF;    /* Checksum */

        /* Enable TX interrupt — ISR will pump all 10 bytes */
        USART2_CTRL1 |= USART_CTRL1_TDBEIEN;

        /* Wait for transmission to complete before accepting next */
        vTaskDelay(10);
    }
}


/*============================================================================
 * FUNCTION 6: spi3_acquisition_task
 * Address: 0x08037428 - 0x08039050  (7208 bytes)
 *
 * THE MAIN SPI3 ADC DATA ACQUISITION ENGINE.
 * This is the heart of the oscilloscope/meter data pipeline.
 *
 * Waits on spi3_data_queue for trigger events, then performs SPI3 transfers
 * to read ADC sample data from the FPGA. Nine acquisition modes are
 * supported, selected by the trigger byte value (0-8).
 *
 * Register allocation:
 *   r7  = &SPI3_STAT (0x40003C08) — used as base for SPI3 register access
 *         [r7+0] = SPI3_STAT, [r7+4] = SPI3_DATA, [r7-8] = SPI3_CTL0
 *   sb  = &meter_state (0x200000F8)
 *   r4  = spi3_data_queue handle pointer
 *   r5  = stack pointer to trigger byte
 *
 * VFP register allocation (preloaded once at entry):
 *   s16 = -28.0         (CH1 ADC offset constant)
 *   s18 = cal_gain_ch1  (from literal pool, calibration gain CH1)
 *   s20 = cal_offset_ch2 (calibration offset CH2)
 *   s22 = cal_bias       (calibration bias / DC offset)
 *   s24 = max_clamp      (maximum calibrated value, typically 255.0)
 *   s26 = min_clamp      (minimum calibrated value, typically 0.0)
 *   s28 = range_divisor  (ADC range divisor for calibration)
 *
 * SPI3 transfer protocol:
 *   1. CS_ASSERT:  GPIOB_BCR = PB6_MASK  (PB6 LOW)
 *   2. Wait TXE:   poll SPI3_STAT bit 1
 *   3. Write:       SPI3_DATA = byte
 *   4. Wait RXNE:  poll SPI3_STAT bit 0
 *   5. Read:        byte = SPI3_DATA
 *   6. CS_DEASSERT: GPIOB_BOP = PB6_MASK (PB6 HIGH)
 *
 * The TXE/RXNE polling uses an optimized pattern: checks 3 times in
 * conditional IT blocks before falling into a polling loop, reducing
 * latency for the common fast-response case.
 *============================================================================*/
void spi3_acquisition_task(void) {
    volatile uint8_t *ms = METER_STATE_BASE;
    uint8_t trigger_byte;

    /* Preload VFP calibration constants */
    float s16_offset = -28.0f;  /* ADC zero offset */
    /* s18, s20, s22, s24, s26, s28 loaded from literal pool at 0x08037674+ */

    while (1) {
        /* Wait for trigger from queue */
        xQueueReceive(SPI3_DATA_QUEUE, &trigger_byte, portMAX_DELAY);

        /* --- Pre-acquisition setup --- */

        /* Check if probe calibration is needed */
        uint8_t ch1_probe = ms[0x352];
        int16_t voltage_range = *(int16_t *)(ms + 0x1C);
        int16_t command_code = ~0x7F ^ voltage_range;  /* Transform for FPGA */

        /* If both probes valid and not in calibration mode, apply probe cal */
        if (ch1_probe != 0 && ch1_probe != 0xFF) {
            uint8_t ch2_probe = ms[0x353];
            if (ch2_probe != 0 && ch2_probe != 0xFF) {
                if (ms[0x33] == 0) {
                    /* Apply VFP probe calibration */
                    /* calibrated = ((float)probe + s16) / s28
                     *            * (float)(end - start) + (float)start + s22 */
                    goto calibrated_acquisition;
                }
            }
        }

        /* --- CS ASSERT --- */
        GPIOB_BCR = PB6_MASK;   /* PB6 LOW — select FPGA */

        /* Send command byte, read response */
        spi3_xfer(command_code);

        /* --- CS DEASSERT --- */
        GPIOB_BOP = PB6_MASK;   /* PB6 HIGH — deselect */

        /*
         * MAIN ACQUISITION SWITCH (TBH at 0x08037536)
         * Branches on (trigger_byte - 1), cases 0-8:
         */
        switch (trigger_byte - 1) {

        case 0: /* SCOPE_FAST_TIMEBASE — Fast timebase oscilloscope capture */
            /*
             * Used for fast timebases (< 4, i.e., fastest sweep speeds).
             * Reads timebase_sample_count from lookup table.
             * Checks if enough samples accumulated (count + 0x32 threshold).
             *
             * SPI3 protocol: sends timebase command, reads back status,
             * then CS_DEASSERT → read SPI3_DATA (status byte, discarded).
             *
             * No ADC data is read in this mode — it merely configures the
             * FPGA timebase and waits for acquisition to complete.
             */
            {
                uint8_t tb_idx = ms[0x2D];
                uint8_t sample_threshold = TIMEBASE_SAMPLE_TABLE[tb_idx] + 0x32;
                uint16_t acq_count = *(uint16_t *)(ms + 0xDB8);

                if (sample_threshold <= acq_count) {
                    /* Acquisition complete — will be handled elsewhere */
                    goto end_acquisition;
                }

                /* Send timebase config to FPGA */
                spi3_xfer(tb_idx);
            }
            break;

        case 1: /* SCOPE_ROLL_MODE — Rolling/streaming oscilloscope mode */
            /*
             * Continuous scrolling display mode.
             * Manages circular buffers at ms+0x356 (CH1) and ms+0x483 (CH2).
             * Maximum 300 (0x12C) samples in the circular buffer.
             *
             * Protocol:
             *   1. Decrement roll_read_ptr if nonzero
             *   2. Increment roll_sample_count (capped at 300)
             *   3. Shift circular buffer contents down by 1
             *   4. Read new samples via SPI3:
             *      - Send 0xFF, read back CH1 raw byte
             *      - Repeat for 3 more bytes (interleaved CH1/CH2)
             *   5. Apply VFP calibration to raw bytes
             *   6. Store calibrated values in roll buffer
             *   7. When buffer full (count==0x12D), trigger display update:
             *      - Check trigger conditions
             *      - Disable TMR3 (stop USART polling)
             *      - Send command 2 to USART cmd queue
             *      - Call measurement_calc()
             *      - Copy circular buffer to main ADC buffers
             */
            {
                uint16_t read_ptr = *(uint16_t *)(ms + 0xDB4);
                if (read_ptr > 0) {
                    *(uint16_t *)(ms + 0xDB4) = read_ptr - 1;
                }

                uint16_t sample_count = *(uint16_t *)(ms + 0xDB6);
                if (sample_count <= 0x12C) {
                    *(uint16_t *)(ms + 0xDB6) = sample_count + 1;
                    if (sample_count == 0) {
                        /* First sample — skip shift */
                        goto roll_read_samples;
                    }
                }

                /* Shift existing samples in circular buffer */
                /* [shift loop omitted] */

roll_read_samples:
                /* Read 4 SPI3 bytes (2 per channel) */
                GPIOB_BCR = PB6_MASK;           /* CS ASSERT */
                uint8_t raw_ch1_hi = spi3_xfer(0xFF);
                uint8_t raw_ch1_lo = spi3_xfer(0xFF);
                uint8_t raw_ch2_hi = spi3_xfer(0xFF);
                uint8_t raw_ch2_lo = spi3_xfer(0xFF);
                uint8_t raw_last   = spi3_xfer(0xFF);
                GPIOB_BOP = PB6_MASK;           /* CS DEASSERT */

                /* Store last byte and apply probe calibration if needed */
                ms[0x5AF] = raw_last;

                /* VFP calibration for roll mode:
                 * if (ch1_probe > 0xDC):
                 *   cal = ((float)ch1_probe + s16_offset) / s18_gain
                 *   result = ((float)raw_ch2 + s20_offset - (float)dc_offset)
                 *          / cal + s22_bias + (float)dc_offset
                 *   clamp to [s26_min, s24_max]
                 * Repeat for ch2_probe with different buffer offset
                 */

                /* When 300 samples collected, check trigger and update display */
                if (*(uint16_t *)(ms + 0xDB6) == 0x12D) {
                    /* Check hold_mode and trigger_mode */
                    /* Disable TMR3, queue command 2, call measurement_calc() */
                    /* Copy circular buffer to main ADC buffer */
                }
            }
            break;

        case 2: /* SCOPE_NORMAL — Standard oscilloscope acquisition */
            /*
             * Reads 0x400 (1024) raw bytes from SPI3 into CH1 buffer.
             * Data format: interleaved CH1/CH2 bytes (even=CH1, odd=CH2).
             *
             * SPI3 protocol per sample pair:
             *   CS_ASSERT
             *   spi3_xfer(0xFF)  → discard (command echo)
             *   spi3_xfer(0xFF)  → CH1 raw byte → ms[0x5B0 + offset]
             *   spi3_xfer(0xFF)  → CH2 raw byte → ms[0x5B0 + offset + 1]
             *   CS_DEASSERT
             *
             * Actually reads in a tight loop without per-sample CS toggle:
             *   Initial CS_ASSERT, send 0xFF command
             *   Then alternating SPI3 writes of 0xFF and reads of data
             *   Stores even-indexed bytes to buffer
             *   Counter increments by 2, total 0x400 bytes = 512 sample pairs
             *
             * After 0x400 bytes: branches to calibration post-processing
             * (at 0x08037D9C) which applies VFP calibration per-sample.
             */
            {
                /* Send initial command (0xFF = read all) */
                spi3_xfer(0xFF);

                /* Bulk read loop: 1024 bytes interleaved */
                for (int i = 0; i < 0x400; i += 2) {
                    spi3_xfer(0xFF);  /* Clock out data */
                    uint8_t ch1_byte = SPI3_DATA;
                    ms[0x5B0 + i] = ch1_byte;

                    spi3_xfer(0xFF);
                    uint8_t ch2_byte = SPI3_DATA;
                    ms[0x5B0 + i + 1] = ch2_byte;
                }
                /* Fall through to calibration post-processing */
            }
            break;

        case 3: /* SCOPE_DUAL_CHANNEL — Dual-channel with calibration */
            /*
             * Reads 0x800 (2048) bytes for dual-channel capture.
             * Same SPI3 protocol as case 2 but double the data.
             * CH1 buffer: ms+0x5B0 (0x400 bytes)
             * CH2 buffer: ms+0x9B0 (0x400 bytes)
             *
             * Initial command: 0xFF
             * Stores interleaved: even bytes to CH1 buf, odd to CH2 buf.
             * After complete, falls to calibration at 0x0803806C.
             */
            {
                spi3_xfer(0xFF);

                for (int i = 0x400; i < 0x800; i += 2) {
                    spi3_xfer(0xFF);
                    ms[0x5B0 + i] = SPI3_DATA;

                    spi3_xfer(0xFF);
                    ms[0x5B0 + i + 1] = SPI3_DATA;
                }
            }
            break;

        case 4: /* SCOPE_EXTENDED — Extended acquisition (XY or deep memory) */
            /*
             * Sends specific command byte (from r2) rather than 0xFF.
             * Reads response and stores to trigger status (ms+0x46).
             *
             * SPI3 protocol:
             *   spi3_xfer(command)  → read response (discarded)
             *
             * After one exchange, CS_DEASSERT, done.
             */
            {
                spi3_xfer(command_code);
            }
            break;

        case 5: /* METER_ADC_READ — Multimeter ADC readback */
            /*
             * Reads ADC value for multimeter mode.
             * Sends the active_channel byte (ms[0x16]) as SPI3 command.
             *
             * SPI3 protocol:
             *   spi3_xfer(active_channel)  → read meter ADC value
             *   CS_DEASSERT, done.
             */
            {
                uint8_t channel = ms[0x16];
                spi3_xfer(channel);
            }
            break;

        case 6: /* SIGGEN_FEEDBACK — Signal generator feedback monitoring */
            /*
             * Reads signal generator output feedback.
             * Sends trigger_mode (ms[0x18]) as SPI3 command.
             *
             * SPI3 protocol:
             *   spi3_xfer(trigger_mode)  → read feedback value
             *   CS_DEASSERT, done.
             */
            {
                uint8_t trig_mode = ms[0x18];
                spi3_xfer(trig_mode);
            }
            break;

        case 7: /* CALIBRATION — Internal calibration readback */
            /*
             * Multi-phase SPI3 exchange for reading FPGA internal status/ID.
             *
             * Protocol:
             *   Phase 1: spi3_xfer(command) → read high byte → ms[0x46]
             *            CS_DEASSERT, vTaskDelay(1), CS_ASSERT
             *   Phase 2: spi3_xfer(0x0A) → read status
             *            spi3_xfer(0xFF) → read low byte #1
             *            spi3_xfer(0xFF) → read low byte #2
             *
             * Assembles 16-bit result: ms[0x46] = (high << 8) | low
             * Also reads ms[0xDB0] (frame counter) and increments it.
             *
             * This appears to be the FPGA version/status readback
             * or an internal self-test sequence.
             */
            {
                spi3_xfer(command_code);
                uint8_t high_byte = SPI3_DATA;
                *(uint16_t *)(ms + 0x46) = high_byte;

                GPIOB_BOP = PB6_MASK;  /* CS DEASSERT */
                vTaskDelay(1);
                GPIOB_BCR = PB6_MASK;  /* CS ASSERT */

                spi3_xfer(0x0A);       /* Sub-command */
                spi3_xfer(0xFF);
                spi3_xfer(0xFF);

                uint8_t low_byte = SPI3_DATA;
                *(uint16_t *)(ms + 0x46) = (high_byte << 8) | low_byte;

                uint8_t frame = ms[0xDB0] + 1;
                /* ... store frame count ... */
            }
            break;

        case 8: /* SELF_TEST — FPGA communication self-test */
            /*
             * Similar to case 7 but with a different command sequence.
             * Sends the command, reads response bytes, and verifies
             * communication integrity. Used during boot validation.
             */
            {
                spi3_xfer(command_code);
                spi3_xfer(0xFF);
                spi3_xfer(0xFF);
            }
            break;
        }

calibrated_acquisition:
        /*
         * POST-ACQUISITION CALIBRATION (0x08037D9C+)
         *
         * Applied after cases 2 and 3 (normal and dual-channel scope).
         * Processes raw samples with VFP single-precision calibration.
         *
         * Conditions for VFP calibration path:
         *   - spi3_transfer_mode (ms[0x14]) must not be 3
         *   - timebase_index (ms[0x2D]) must be < 4
         *   - calibration_mode (ms[0x33]) must be 0 (not calibrating)
         *   - Both probe types (ms[0x352], ms[0x353]) must be valid (1-0xFE)
         *
         * Per-sample calibration formula (VFP, from disassembly at 0x08037918):
         *
         *   float raw_f = (float)(uint8_t)raw_sample;
         *   float normalized = (raw_f + s16_offset) / s28_divisor;
         *   int8_t dc_offset = *(int8_t *)(ms + 4);  // CH1 DC offset
         *   float range = (float)((int16_t)voltage_range - dc_offset);
         *   float result = normalized * range + (float)dc_offset + s22_bias;
         *
         *   // Clamp to valid range
         *   if (result > s24_max) result = s24_max;  // typically 255.0
         *   if (result < 0.0)     result = s26_min;  // typically 0.0
         *
         *   calibrated_sample = (uint8_t)(int)result;
         *
         * This transforms raw 8-bit ADC values (0-255) from the FPGA into
         * calibrated display-ready values, accounting for:
         *   - ADC zero offset (s16 = -28.0 for this hardware)
         *   - Probe gain scaling (s28)
         *   - Vertical position offset
         *   - Per-channel DC coupling offset
         *   - Display range mapping
         */

end_acquisition:
        /* CS DEASSERT (if not already done) */
        GPIOB_BOP = PB6_MASK;
    }
}


/*============================================================================
 * FUNCTION 7: spi3_init_and_setup
 * Address: 0x08039050 - 0x08039188  (312 bytes)
 *
 * Initializes SPI3 peripheral and GPIO pin configuration for FPGA
 * data interface. Called once during startup, before the acquisition
 * task enters its main loop.
 *
 * Also performs the initial SPI3 handshake with the FPGA to verify
 * communication.
 *============================================================================*/
void spi3_init_and_setup(void) {
    uint8_t mode;
    xQueueReceive(SPI3_DATA_QUEUE, &mode, portMAX_DELAY);

    /*
     * GPIO configuration for SPI3 (remapped from JTAG):
     * NOTE: AFIO remap must already be done (AFIO_PCF0 bit 25 = SW-DP only)
     * which frees PB3/PB4/PB5 from JTAG use.
     */

    /* PB3 = SPI3_SCK:  Alternate function push-pull, 50MHz */
    /* PB4 = SPI3_MISO: Input floating */
    /* PB5 = SPI3_MOSI: Alternate function push-pull, 50MHz */
    /* PB6 = SPI3_CS:   General purpose output push-pull */
    gpio_init(/* GPIOB, PB3 AF_PP 50MHz */);
    gpio_init(/* GPIOB, PB4 floating input */);
    gpio_init(/* GPIOB, PB5 AF_PP 50MHz */);
    gpio_init(/* GPIOB, PB6 GPIO output */);

    /* CS deassert (idle HIGH) */
    GPIOB_BOP = PB6_MASK;

    /*
     * SPI3 configuration:
     *   - Master mode
     *   - Clock polarity: CPOL=1 (idle HIGH)       ← MODE 3
     *   - Clock phase: CPHA=1 (sample on 2nd edge)  ← MODE 3
     *   - Data frame: 8-bit
     *   - Bit order: MSB first
     *   - NSS management: Software (SSM=1, SSI=1)
     *   - Baud rate prescaler: /2 (120MHz / 2 = 60MHz SPI clock)
     *   - Full duplex
     */

    /* SPI3_CTL0 and CTL1 configuration via spi_init(0x40003C00, config) */
    /* Post-init: enable DMA requests and SPI peripheral */
    SPI3_CTL1 |= 0x02;   /* TXDMAEN or RXNEIE */
    SPI3_CTL1 |= 0x01;   /* RXDMAEN or TXEIE */
    SPI3_CTL0 |= 0x40;   /* SPE — SPI enable */

    /* Initial handshake: dummy transfer */
    GPIOB_BCR = PB6_MASK;  /* CS ASSERT */
    spi3_xfer(0x00);        /* Send dummy byte */
    GPIOB_BOP = PB6_MASK;  /* CS DEASSERT */

    vTaskDelay(10);         /* Wait for FPGA to stabilize */

    /* Signal readiness via semaphore */
    xQueueSemaphoreTake(FPGA_SEMAPHORE_1, portMAX_DELAY);
}


/*============================================================================
 * FUNCTION 8: input_and_housekeeping
 * Address: 0x08039188 - 0x080396C6  (1342 bytes)
 *
 * Comprehensive input scanning, button debouncing, timer management,
 * watchdog feeding, and frequency measurement.
 *
 * Called periodically (likely from TMR3 ISR context or a timer task).
 *
 * Responsibilities:
 *   1. GPIO input scanning (GPIOB, GPIOC)
 *   2. 15-button debounce with configurable thresholds
 *   3. Button event queueing to FreeRTOS
 *   4. Watchdog (IWDG) management
 *   5. Frame counter and housekeeping timers
 *   6. Auto power-off timer
 *   7. Frequency/period measurement via TIM5
 *   8. Trigger/acquisition readiness monitoring
 *============================================================================*/
void input_and_housekeeping(void) {
    volatile uint8_t *ms = METER_STATE_BASE;

    /*
     * INPUT SCANNING
     *
     * Build input_state byte from GPIO reads:
     *   bit 0: ~GPIOC_IDR[7]  (PC7 = probe detect, active low)
     *   bit 1: GPIOB_IDR[5]   (PB5 state)
     *   bit 2: ~special_reg   (derived from GPIOB CTL register)
     *   bit 3: ~GPIOC_IDR[3]  (PC3/PC4 state, active low)
     *
     * Three TBB switch tables decode the GPIO bits into button events:
     *
     * Group 1 (TBB at 0x080392B0, GPIOC-derived):
     *   case 0: button_events |= 0x80    (CH1 probe change)
     *   case 1: button_events |= 0x800   (CH2 probe change)
     *   case 2: button_events |= 0x100   (probe type A)
     *   case 3: button_events |= 0x200   (probe type B)
     *
     * Group 2 (TBB at 0x080392F8, PB8-derived):
     *   case 0: button_events |= 0x20    (button group 1)
     *   case 1: button_events |= 0x10    (button group 2)
     *   case 2: button_events |= 0x08    (button group 3)
     *   case 3: button_events |= 0x2000  (special button)
     *
     * Group 3 (TBB at 0x0803933E, GPIOC IDR bit 10):
     *   case 0: button_events |= 0x1000  (trigger button)
     *   case 1: button_events |= 0x04    (select button)
     *   case 2: button_events |= 0x4000  (menu button)
     *   case 3: button_events |= 0x02    (OK button)
     */
    uint16_t button_events = 0;
    /* [GPIO reads and TBB dispatch build button_events] */

    /*
     * BUTTON DEBOUNCE
     *
     * For each of 15 buttons (5 groups x 3):
     *   - Debounce counters at 0x20002D58+
     *   - Press threshold:  0x46 (70 ticks) — confirm press
     *   - Release threshold: 0x48 (72 ticks) — detect long press
     *   - On confirmed short press: look up logical ID from button_map_table
     *   - On long press: different logical ID from same table
     *   - On release (2-0x45 range): emit press event
     *
     * Button map table at 0x08046528 maps physical position to logical button ID.
     * Logical IDs correspond to: CH1, CH2, MOVE, SELECT, TRIGGER, PRM,
     *   AUTO, SAVE, MENU, UP, DOWN, LEFT, RIGHT, OK, POWER
     */
    uint8_t event_count = 0;
    uint8_t button_id = 0;

    /* [debounce loop processes all 15 buttons] */

    /* If exactly one button event, queue it */
    if (event_count == 1 && button_id != 0) {
        xQueueGenericSend(SECONDARY_CMD_QUEUE, &button_id, 0, 0);
    }

    /*
     * WATCHDOG MANAGEMENT
     *
     * IWDG reload register at 0x40015434 checked.
     * Counter incremented each call.
     * When counter >= 11 (0x0B), reload watchdog (IWDG_KR = 0xAAAA).
     * This gives ~50ms between watchdog kicks at typical call rate.
     */
    /* [watchdog logic] */

    /*
     * HOUSEKEEPING TIMERS
     */
    ms[0x3D]++;                            /* Frame counter (wraps at 99) */
    if (ms[0x3D] > 99) ms[0x3D] = 0;

    /* Auto power-off management */
    if (ms[0xE10] >= 2 && ms[0xE10] != 0xFF) {
        ms[0xE24]++;                       /* Power-off tick counter */
        if (ms[0xE24] >= 0x65) {           /* 101 ticks threshold */
            ms[0xE10] = 0;                 /* Disable auto-off */
        }
    } else {
        ms[0xE24] = 0;
    }

    /* Countdown timers */
    if (ms[0x32] != 0xFF) ms[0x32]++;     /* Acquisition timer */
    if (ms[0x2A] != 0) ms[0x2A]--;        /* Display hold countdown */
    if (ms[0x28] != 0) ms[0x28]--;        /* Button repeat countdown */
    if (ms[0x2C] != 0) ms[0x2C]--;        /* Generic countdown */
    if (ms[0xDBB] != 0) ms[0xDBB]--;      /* Timer B */
    if (ms[0xDBA] != 0) ms[0xDBA]--;      /* Timer A */

    /*
     * FREQUENCY MEASUREMENT (via TIM5)
     *
     * Cycle counter (ms[0x45]) increments each call.
     * At count 0: reset TIM5 counter and control register.
     * At count 0x33 (51): read TIM5 capture values for both channels.
     *
     * Frequency calculation:
     *   period = (float)TIM5_count * 2.0
     *   frequency = 1,000,000,000 / (int)period    (in Hz)
     *
     * Results stored:
     *   ms[0x50] = CH1 period (nanoseconds)
     *   ms[0x54] = CH1 frequency (Hz)
     *   ms[0x80] = CH2 period
     *   ms[0x84] = CH2 frequency
     *
     * Also copies trigger state: ms[0x230] = ms[0x231]; ms[0x231] = 0
     */
    ms[0x45]++;
    if (ms[0x45] == 0) {
        TIM5_CNT = 0;
        /* Reset TIM5 control */
    }
    if (ms[0x45] == 0x33) {
        if (ms[0x2E] != 0) {  /* Frequency measurement enabled */
            /* Read TIM5 capture registers */
            /* Calculate frequency and period */
            /* Store results */
        }
        ms[0x45] = 0;
    }

    /*
     * TRIGGER / ACQUISITION READINESS
     *
     * Checks GPIOC_IDR bit 0: FPGA data-ready signal (active LOW).
     *
     * If data ready AND not in hold mode AND valid timebase:
     *   - Increment acquisition sample counter (ms[0xDB8])
     *   - Check against threshold from timebase table + 0x32
     *   - When threshold reached: send TWO items to SPI3 data queue
     *     (double-buffered acquisition: triggers two back-to-back reads)
     *
     * If NOT data ready: reset acquisition counter to 0.
     */
    if (!(GPIOC_IDR & 1)) {
        /* FPGA data ready (PC0 LOW) */
        if (ms[0x17] != 2) {  /* Not in hold mode */
            if (ms[0x2D] <= 0x13) {  /* Valid timebase index */
                uint16_t count = *(uint16_t *)(ms + 0xDB8);
                count++;
                *(uint16_t *)(ms + 0xDB8) = count;

                uint8_t threshold = TIMEBASE_SAMPLE_TABLE[ms[0x2D]] + 0x32;
                if (threshold == count) {
                    /* Acquisition complete — trigger SPI3 reads */
                    uint8_t trigger = 1;
                    xQueueGenericSend(SPI3_DATA_QUEUE, &trigger, portMAX_DELAY, 0);
                    xQueueGenericSend(SPI3_DATA_QUEUE, &trigger, portMAX_DELAY, 0);
                }
            }
        }
    } else {
        *(uint16_t *)(ms + 0xDB8) = 0;  /* Reset acquisition counter */
    }
}


/*============================================================================
 * FUNCTION 9: probe_change_handler
 * Address: 0x080396C8 - 0x08039734  (108 bytes)
 *
 * Detects probe connection/disconnection events and manages auto power-off
 * timers. Thresholds are based on the type of change detected:
 *
 *   change == 3: 0x0E10 ticks (3600 = ~1 hour)    — full probe swap
 *   change == 2: 0x0708 ticks (1800 = ~30 minutes) — probe type change
 *   change == 1: 0x0384 ticks (900 = ~15 minutes)  — probe disconnect
 *
 * When the threshold is exceeded, triggers system power-off with code 0x55.
 *============================================================================*/
void probe_change_handler(void) {
    volatile uint8_t *ms = METER_STATE_BASE;

    /* Call input_handler to update probe/GPIO state */
    input_and_housekeeping();

    /* Save previous state, clear change flags */
    ms[0xF6F] = ms[0xF70];
    ms[0xF70] = 0;
    ms[0xDB1] = ms[0xDB2];
    ms[0xDB2] = 0;

    uint8_t change_type = ms[0xF65];
    if (change_type == 0) return;

    /* Increment change counter */
    ms[0xF6C]++;

    /* Check auto power-off thresholds */
    uint16_t counter = ms[0xF6C];  /* Simplified — actual uses wider counter */
    if (change_type == 3 && counter > 0x0E10) goto power_off;
    if (change_type == 2 && counter > 0x0708) goto power_off;
    if (change_type == 1 && counter > 0x0384) goto power_off;
    return;

power_off:
    /* system_reset_or_power_off(0x55) — trigger controlled shutdown */
    return;
}


/*============================================================================
 * FUNCTION 10: usart_tx_config_writer
 * Address: 0x08039734 - 0x08039870  (316 bytes)
 *
 * Writes FPGA configuration parameters into the USART TX frame based on
 * command type. Called by scope/meter/siggen mode handlers to set up
 * the FPGA for the desired operating mode.
 *
 * 7 command types (TBB switch at 0x0803973E):
 *
 *   Type 0: SCOPE CH1 CONFIGURATION
 *     - AC/DC coupling:    config[0x20] bit 1 = params[1] bit 0
 *     - Bandwidth limit:   config[0x20] bit 3 = params[1] bit 1
 *     - Voltage range low:  config[0x18] = (params[2] & 3)
 *     - Voltage range high: config[0x18] |= (params[3] & 0xF) << 12
 *     - Channel mask = 0x01
 *
 *   Type 1: SCOPE CH2 CONFIGURATION
 *     - AC/DC coupling:    config[0x20] bit 5 = params[1] bit 0
 *     - Bandwidth limit:   config[0x20] bit 7 = params[1] bit 1
 *     - Voltage range:     same structure, different bit positions
 *     - Channel mask = 0x10
 *
 *   Type 2: TRIGGER CONFIGURATION
 *     - Trigger edge:      config[0x20] bit 9  = params[1] bit 0
 *     - Trigger source:    config[0x20] bit 11 = params[1] bit 1
 *     - Trigger level:     config[0x1C] = level encoding
 *     - Channel mask = 0x10
 *
 *   Type 3: TIMEBASE CONFIGURATION
 *     - Direct register writes for sample rate / timebase
 *
 *   Type 4: METER RANGE CONFIGURATION
 *     - Meter measurement range selection
 *
 *   Type 5: SIGNAL GENERATOR FREQUENCY
 *     - Frequency register configuration for DAC output
 *
 *   Type 6: SIGNAL GENERATOR WAVEFORM
 *     - Waveform type selection (sine, square, triangle, etc.)
 *============================================================================*/
void usart_tx_config_writer(uint8_t *config_base, uint8_t *params) {
    uint8_t cmd_type = params[0];
    if (cmd_type > 6) return;

    switch (cmd_type) {

    case 0: /* Scope CH1 config */
        /* config_base[0x20].bit1 = params[1].bit0  (AC/DC) */
        if (params[1] & 1)
            config_base[0x20] |= (1 << 1);
        else
            config_base[0x20] &= ~(1 << 1);

        /* config_base[0x20].bit3 = params[1].bit1  (BW limit) */
        if (params[1] & 2)
            config_base[0x20] |= (1 << 3);
        else
            config_base[0x20] &= ~(1 << 3);

        /* Voltage range encoding */
        config_base[0x18] = (params[2] & 3);
        config_base[0x18] |= (params[3] & 0xF) << 12;
        /* channel_mask = 0x01 */
        break;

    case 1: /* Scope CH2 config */
        if (params[1] & 1)
            config_base[0x20] |= (1 << 5);
        else
            config_base[0x20] &= ~(1 << 5);

        if (params[1] & 2)
            config_base[0x20] |= (1 << 7);
        else
            config_base[0x20] &= ~(1 << 7);

        config_base[0x18] = (params[2] & 3) << 8;
        config_base[0x18] |= (params[3] & 0xF) << 12;
        /* channel_mask = 0x10 */
        break;

    case 2: /* Trigger config */
        if (params[1] & 1)
            config_base[0x20] |= (1 << 9);
        else
            config_base[0x20] &= ~(1 << 9);

        if (params[1] & 2)
            config_base[0x20] |= (1 << 11);
        else
            config_base[0x20] &= ~(1 << 11);

        /* config_base[0x1C] = trigger level */
        /* channel_mask = 0x10 */
        break;

    case 3: /* Timebase config */
        /* Direct register writes */
        break;

    case 4: /* Meter range config */
        /* Range selection */
        break;

    case 5: /* Siggen frequency */
        /* Frequency registers */
        break;

    case 6: /* Siggen waveform */
        /* Waveform type */
        break;
    }
}


/*============================================================================
 * HELPER: spi3_xfer (inlined throughout the firmware)
 *
 * Performs a single SPI3 full-duplex byte transfer.
 * This is NOT a standalone function in the firmware — it is inlined at
 * 22+ locations. The pattern is always:
 *
 *   1. Poll SPI3_STAT bit 1 (TXE) until set — TX buffer empty
 *      (optimized: 3 checks in IT block, then poll loop)
 *   2. Write data byte to SPI3_DATA
 *   3. Poll SPI3_STAT bit 0 (RXNE) until set — RX data available
 *      (same optimized pattern)
 *   4. Read received byte from SPI3_DATA
 *
 * Assembly pattern (at every SPI3 transfer site):
 *   ldr  r3, [r7]           ; r7 = &SPI3_STAT
 *   lsls r3, r3, #0x1e      ; check bit 1 (TXE)
 *   itttt pl                 ; if not set, try 2 more times
 *   ldrpl r3, [r7]
 *   lslspl.w r3, r3, #0x1e
 *   ldrpl r3, [r7]
 *   lslspl.w r3, r3, #0x1e
 *   bmi  tx_ready
 *   ; fall to polling loop
 *   ...
 *   str  rN, [r7, #4]       ; write byte to SPI3_DATA (SPI3_STAT + 4)
 *   ; then same pattern for RXNE (bit 0, lsls #0x1f)
 *   ...
 *   ldr  rN, [r7, #4]       ; read received byte from SPI3_DATA
 *============================================================================*/
static inline uint8_t spi3_xfer(uint8_t tx_byte) {
    /* Wait for TXE */
    while (!(SPI3_STAT & SPI_STAT_TXE));
    SPI3_DATA = tx_byte;
    /* Wait for RXNE */
    while (!(SPI3_STAT & SPI_STAT_RXNE));
    return (uint8_t)SPI3_DATA;
}
