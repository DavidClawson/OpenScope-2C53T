/*
 * OpenScope 2C53T - FPGA Communication Driver
 *
 * Implements the complete FPGA interface as reverse-engineered from
 * the stock firmware's FPGA task (FUN_08036934, 11.6KB).
 *
 * Boot sequence follows FPGA_BOOT_SEQUENCE.md (53 steps):
 *   1. AFIO remap to free PB3/4/5 from JTAG
 *   2. USART2 init at 9600 baud
 *   3. Send boot commands (0x01, 0x02, 0x06, 0x07, 0x08)
 *   4. SPI3 init (Mode 3, /2 prescaler = 60MHz)
 *   5. PC6 HIGH (FPGA SPI enable)
 *   6. SysTick delays for FPGA timing
 *   7. SPI3 handshake (command 0x05)
 *   8. PB11 HIGH (FPGA active mode)
 *
 * Runtime architecture (3 FreeRTOS tasks):
 *   - fpga_usart_tx_task: Sends 10-byte command frames via USART2
 *   - fpga_usart_rx_task: Processes received meter/status data
 *   - fpga_acquisition_task: SPI3 bulk ADC data reads (9 modes)
 */

#include "fpga.h"
#include "meter_data.h"
#include "at32f403a_407.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════
 * Hardware Register Access
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * SPI3/SPI4 register access.
 * On AT32F403A, the peripheral at 0x40003C00 is called SPI4 in the HAL,
 * but maps to the same address as GD32's SPI3. We use the HAL's SPI3_BASE
 * (which is already defined) and access registers through typed pointers
 * to avoid macro redefinition warnings.
 */
#define FPGA_SPI       ((spi_type *)SPI3_BASE)

/* USART ctrl1 bit masks (AT32 HAL uses MAKE_VALUE macros, we need raw bits) */
#define USART_CTRL1_RDBFIEN   (1 << 5)   /* RX buffer full interrupt enable */
#define USART_CTRL1_TDBEIEN   (1 << 7)   /* TX buffer empty interrupt enable */

/* GPIO bit operations */
#define PB6_MASK        (1 << 6)   /* SPI3 CS */
#define PB11_MASK       (1 << 11)  /* FPGA active mode */
#define PC6_MASK        (1 << 6)   /* FPGA SPI enable */

/* CS control macros */
#define SPI3_CS_ASSERT()    (GPIOB->clr = PB6_MASK)   /* PB6 LOW */
#define SPI3_CS_DEASSERT()  (GPIOB->scr = PB6_MASK)   /* PB6 HIGH */

/* ═══════════════════════════════════════════════════════════════════
 * Global State
 * ═══════════════════════════════════════════════════════════════════ */

fpga_state_t fpga;

/* FreeRTOS handles */
static QueueHandle_t     usart_tx_queue  = NULL;  /* 2-byte items: cmd_hi|cmd_lo */
static QueueHandle_t     spi3_acq_queue  = NULL;  /* 1-byte trigger mode */
static SemaphoreHandle_t meter_sem       = NULL;  /* Signals meter RX frame ready */

static TaskHandle_t      acq_task_handle = NULL;
static TaskHandle_t      tx_task_handle  = NULL;
static TaskHandle_t      rx_task_handle  = NULL;

/* Track whether we've received at least one valid acquisition */
static volatile bool data_ready = false;

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Low-Level Transfer
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Full-duplex SPI3 byte exchange.
 * Sends tx_byte, returns the byte received simultaneously.
 * Matches stock firmware's optimized TXE/RXNE polling pattern.
 */
static uint8_t spi3_xfer(uint8_t tx_byte)
{
    volatile uint32_t timeout;

    /* Wait for TX buffer empty */
    timeout = 100000;
    while (!(FPGA_SPI->sts & SPI_I2S_TDBE_FLAG)) {
        if (--timeout == 0) return 0xFF;
    }
    FPGA_SPI->dt = tx_byte;

    /* Wait for RX buffer not empty */
    timeout = 100000;
    while (!(FPGA_SPI->sts & SPI_I2S_RDBF_FLAG)) {
        if (--timeout == 0) return 0xFF;
    }
    return (uint8_t)FPGA_SPI->dt;
}

/* ═══════════════════════════════════════════════════════════════════
 * USART2 Byte-Level TX (used during boot, before tasks are running)
 * ═══════════════════════════════════════════════════════════════════ */

static void usart2_send_byte(uint8_t b)
{
    volatile uint32_t timeout = 100000;
    while (!(USART2->sts & USART_TDBE_FLAG)) {
        if (--timeout == 0) return;
    }
    USART2->dt = b;
}

static void usart2_send_frame(const uint8_t *frame)
{
    for (int i = 0; i < FPGA_TX_FRAME_SIZE; i++) {
        usart2_send_byte(frame[i]);
    }
    /* Wait for transmit complete */
    volatile uint32_t timeout = 100000;
    while (!(USART2->sts & USART_TDC_FLAG)) {
        if (--timeout == 0) break;
    }
}

/*
 * Build and send a USART command frame (10 bytes).
 * Format: [0][1] [cmd_hi][cmd_lo] [0..0] [checksum]
 * Checksum = (cmd_hi + cmd_lo) & 0xFF
 */
static void usart2_send_cmd(uint8_t cmd_hi, uint8_t cmd_lo)
{
    uint8_t frame[FPGA_TX_FRAME_SIZE] = {0};
    frame[2] = cmd_hi;
    frame[3] = cmd_lo;
    /* Bytes [4]-[8] remain 0x00 — matches stock firmware exactly.
     * Stock firmware never writes to these bytes (BSS-initialized). */
    frame[9] = (cmd_lo + cmd_hi) & 0xFF;
    usart2_send_frame(frame);
}

/* ═══════════════════════════════════════════════════════════════════
 * SysTick Delay (pre-RTOS, matches stock firmware timing)
 * ═══════════════════════════════════════════════════════════════════ */

static void systick_delay_us(uint32_t us)
{
    /* Use SysTick for precise microsecond delays.
     * Stock firmware uses this between boot phases. */
    uint32_t ticks = (system_core_clock / 1000000) * us;
    SysTick->LOAD = ticks - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {}
    SysTick->CTRL = 0;
}

static void systick_delay_ms(uint32_t ms)
{
    while (ms--) {
        systick_delay_us(1000);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * USART2 IRQ Handler
 *
 * Called from the USART2 interrupt. Handles both TX and RX:
 *   TX: Pumps bytes from fpga.tx_frame[] (10 bytes, index in tx_index)
 *   RX: Assembles bytes into fpga.rx_buf[], validates frame headers
 * ═══════════════════════════════════════════════════════════════════ */

void USART2_IRQHandler(void)
{
    /* TX: send next byte from frame buffer */
    if ((USART2->ctrl1 & USART_CTRL1_TDBEIEN) && (USART2->sts & USART_TDBE_FLAG)) {
        if (fpga.tx_index < FPGA_TX_FRAME_SIZE) {
            USART2->dt = fpga.tx_frame[fpga.tx_index++];
        } else {
            /* All bytes sent — disable TX interrupt */
            USART2->ctrl1 &= ~USART_CTRL1_TDBEIEN;
        }
    }

    /* RX: receive and assemble frame */
    if (USART2->sts & USART_RDBF_FLAG) {
        fpga.rx_byte_count++;
        uint8_t byte = (uint8_t)USART2->dt;

        if (fpga.rx_index == 0) {
            /* Looking for frame header first byte */
            if (byte == FPGA_RX_DATA_HDR_0 || byte == FPGA_RX_ECHO_HDR_0) {
                fpga.rx_buf[0] = byte;
                fpga.rx_index = 1;
            }
        } else if (fpga.rx_index == 1) {
            /* Validate header second byte */
            if ((fpga.rx_buf[0] == FPGA_RX_DATA_HDR_0 && byte == FPGA_RX_DATA_HDR_1) ||
                (fpga.rx_buf[0] == FPGA_RX_ECHO_HDR_0 && byte == FPGA_RX_ECHO_HDR_1)) {
                fpga.rx_buf[1] = byte;
                fpga.rx_index = 2;
            } else {
                /* Invalid header — restart */
                fpga.rx_index = 0;
            }
        } else {
            fpga.rx_buf[fpga.rx_index++] = byte;

            /* Check for complete frame */
            if (fpga.rx_buf[0] == FPGA_RX_DATA_HDR_0 &&
                fpga.rx_index >= FPGA_RX_FRAME_SIZE) {
                /* Complete data frame (12 bytes): copy to stable buffer */
                memcpy((void *)fpga.rx_frame, (const void *)fpga.rx_buf,
                       FPGA_RX_FRAME_SIZE);
                fpga.rx_frame_valid = true;
                fpga.frame_count++;
                fpga.rx_index = 0;

                /* Signal meter processing task (only if RTOS is running) */
                if (meter_sem != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    xSemaphoreGiveFromISR(meter_sem, &xHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }

            } else if (fpga.rx_buf[0] == FPGA_RX_ECHO_HDR_0 &&
                       fpga.rx_index >= 10) {
                /* Complete echo frame (10 bytes): just acknowledge */
                fpga.echo_count++;
                fpga.rx_index = 0;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * FreeRTOS Tasks
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * USART TX Task (dvom_TX equivalent)
 * Receives 2-byte command items from usart_tx_queue, builds 10-byte
 * frames, and initiates interrupt-driven transmission.
 */
static void fpga_usart_tx_task(void *pv)
{
    (void)pv;
    uint16_t cmd_item;

    for (;;) {
        xQueueReceive(usart_tx_queue, &cmd_item, portMAX_DELAY);

        uint8_t cmd_lo = cmd_item & 0xFF;
        uint8_t cmd_hi = (cmd_item >> 8) & 0xFF;

        /* Build TX frame — bytes [4]-[8] stay 0x00 (matches stock firmware) */
        fpga.tx_count++;
        fpga.tx_index = 0;
        memset((void *)fpga.tx_frame, 0, FPGA_TX_FRAME_SIZE);
        fpga.tx_frame[2] = cmd_hi;
        fpga.tx_frame[3] = cmd_lo;
        fpga.tx_frame[9] = (cmd_lo + cmd_hi) & 0xFF;

        /* Enable TX interrupt — ISR pumps all 10 bytes */
        USART2->ctrl1 |= USART_CTRL1_TDBEIEN;

        /* Wait for transmission before accepting next command */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*
 * USART RX Processing Task (dvom_RX equivalent)
 * Wakes on meter_sem when a complete data frame arrives.
 * Parses BCD meter readings and updates the global meter_reading.
 *
 * After parsing, sends auto-range feedback commands (0x1B, 0x1C, 0x1E)
 * to keep the FPGA meter IC properly configured. Without these, the
 * meter IC operates with wrong gain/reference settings.
 * See: fpga_state_update (0x080028E0) in stock firmware.
 */
static void fpga_usart_rx_task(void *pv)
{
    (void)pv;

    for (;;) {
        /* Block until USART ISR signals a complete data frame */
        xSemaphoreTake(meter_sem, portMAX_DELAY);

        /* Parse the meter data from the RX frame.
         * meter_submode is the global from main.c (via ui.h extern). */
        extern volatile uint8_t meter_submode;
        meter_data_process_frame(fpga.rx_frame, meter_submode);

        /* Auto-range feedback commands (0x1B, 0x1C, 0x1E) DISABLED.
         *
         * 2026-04-04 findings: Sending these at runtime causes the FPGA
         * meter IC to auto-range internally, but the MCU's analog frontend
         * relays don't track the range changes. Result: correct readings
         * only in the ~2-10V sweet spot, wildly wrong outside it.
         *
         * With these disabled and boot commands 0x1A-0x1E (param=0), the
         * meter IC stays on a fixed 10V range: accurate 1-10V DCV readings,
         * BCD wraps above 10V. A relay click at ~0.7V suggests the FPGA
         * controls some analog switching internally.
         *
         * TODO: Implement MCU-side auto-ranging with relay switching:
         *   1. Detect BCD overflow (>9500) → send higher range params
         *   2. Detect BCD underflow (<100) → send lower range params
         *   3. Switch relays via gpio_mux_portc_porte/porta_portb
         *   4. Need to discover param values for 600mV, 60V, 600V ranges
         */
    }
}

/*
 * SPI3 Acquisition Task (fpga equivalent)
 * Waits on spi3_acq_queue for trigger events, then performs SPI3
 * transfers to read ADC sample data from FPGA.
 *
 * Initially implements modes 2 (normal) and 3 (dual channel),
 * which cover the primary oscilloscope use case.
 */
static void fpga_acquisition_task(void *pv)
{
    (void)pv;
    uint8_t trigger_byte;

    for (;;) {
        /* Wait for trigger from input/housekeeping or timer */
        xQueueReceive(spi3_acq_queue, &trigger_byte, portMAX_DELAY);

        if (!fpga.initialized) continue;

        /* CS assert */
        SPI3_CS_ASSERT();

        /* Send command byte (0xFF for bulk read) and get status */
        uint8_t status = spi3_xfer(0xFF);
        (void)status;

        switch (trigger_byte) {

        case 3: /* FPGA_ACQ_NORMAL + 1: Normal scope, 1024 bytes interleaved */
        {
            /*
             * Read 1024 bytes of interleaved CH1/CH2 data:
             *   Byte 0: CH1[0], Byte 1: CH2[0]
             *   Byte 2: CH1[1], Byte 3: CH2[1]
             *   ...
             * Store into separate channel buffers.
             */
            for (int i = 0; i < 512; i++) {
                uint8_t ch1_raw = spi3_xfer(0xFF);
                uint8_t ch2_raw = spi3_xfer(0xFF);

                /* Apply ADC offset calibration:
                 * calibrated = clamp(raw + FPGA_ADC_OFFSET, 0, 255)
                 *
                 * Full VFP calibration pipeline (gain, range scaling,
                 * probe compensation) will be added once basic data
                 * flow is confirmed working. */
                int16_t ch1_cal = (int16_t)ch1_raw + (int16_t)FPGA_ADC_OFFSET;
                int16_t ch2_cal = (int16_t)ch2_raw + (int16_t)FPGA_ADC_OFFSET;

                if (ch1_cal < 0) ch1_cal = 0;
                if (ch1_cal > 255) ch1_cal = 255;
                if (ch2_cal < 0) ch2_cal = 0;
                if (ch2_cal > 255) ch2_cal = 255;

                fpga.ch1_buf[i] = (uint8_t)ch1_cal;
                fpga.ch2_buf[i] = (uint8_t)ch2_cal;
            }

            fpga.frame_count++;
            data_ready = true;
            break;
        }

        case 4: /* FPGA_ACQ_DUAL + 1: Dual channel, 2048 bytes */
        {
            /* Read 1024 bytes for CH1 */
            for (int i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
                uint8_t raw = spi3_xfer(0xFF);
                int16_t cal = (int16_t)raw + (int16_t)FPGA_ADC_OFFSET;
                if (cal < 0) cal = 0;
                if (cal > 255) cal = 255;
                fpga.ch1_buf[i] = (uint8_t)cal;
            }
            /* Read 1024 bytes for CH2 */
            for (int i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
                uint8_t raw = spi3_xfer(0xFF);
                int16_t cal = (int16_t)raw + (int16_t)FPGA_ADC_OFFSET;
                if (cal < 0) cal = 0;
                if (cal > 255) cal = 255;
                fpga.ch2_buf[i] = (uint8_t)cal;
            }

            fpga.frame_count++;
            data_ready = true;
            break;
        }

        case 2: /* FPGA_ACQ_ROLL + 1: Roll mode */
        {
            /* Read 5 bytes: ref, ch1, ch2, ch2_extra, last */
            spi3_xfer(0xFF);  /* CH1 reference (discard) */
            uint8_t ch1_raw = spi3_xfer(0xFF);
            uint8_t ch2_raw = spi3_xfer(0xFF);
            spi3_xfer(0xFF);  /* CH2 extra */
            spi3_xfer(0xFF);  /* Last byte */

            /* Apply offset */
            int16_t ch1_cal = (int16_t)ch1_raw + (int16_t)FPGA_ADC_OFFSET;
            int16_t ch2_cal = (int16_t)ch2_raw + (int16_t)FPGA_ADC_OFFSET;
            if (ch1_cal < 0) ch1_cal = 0;
            if (ch1_cal > 255) ch1_cal = 255;
            if (ch2_cal < 0) ch2_cal = 0;
            if (ch2_cal > 255) ch2_cal = 255;

            /* Append to circular buffer */
            uint16_t idx = fpga.roll_write_idx;
            fpga.roll_ch1[idx] = (uint8_t)ch1_cal;
            fpga.roll_ch2[idx] = (uint8_t)ch2_cal;
            fpga.roll_write_idx = (idx + 1) % FPGA_ROLL_BUF_SIZE;
            if (fpga.roll_count < FPGA_ROLL_BUF_SIZE) {
                fpga.roll_count++;
            }

            data_ready = true;
            break;
        }

        default:
            /* Other modes (fast TB, meter ADC, siggen, cal, self-test)
             * will be implemented as each subsystem is wired up. */
            break;
        }

        /* CS deassert */
        SPI3_CS_DEASSERT();
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════ */

void fpga_init(void)
{
    /* Clear state */
    memset(&fpga, 0, sizeof(fpga));

    /* ---------------------------------------------------------------
     * Step 1: AFIO remap — disable JTAG-DP, keep SW-DP
     * This frees PB3/PB4/PB5 for SPI3 use.
     * Stock firmware: AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000
     * AT32 equivalent: IOMUX_REMAP6 SWJ_JTAG remap
     * --------------------------------------------------------------- */
    /* Enable IOMUX clock (should already be enabled from main.c) */
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);

    /* Disable JTAG, keep SWD: set SWJ_CFG bits [26:24] = 010 */
    IOMUX->remap6 = (IOMUX->remap6 & ~(0x7 << 24)) | (0x2 << 24);

    /* ---------------------------------------------------------------
     * Step 2: USART2 init — 9600 baud, 8N1, TX+RX with interrupts
     * --------------------------------------------------------------- */
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    /* PA2 = USART2_TX: AF push-pull, 50MHz */
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_pins = GPIO_PINS_2;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOA, &gpio_cfg);

    /* PA3 = USART2_RX: Input floating */
    gpio_cfg.gpio_pins = GPIO_PINS_3;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_cfg);

    /* USART2 config: 9600 baud, 8N1 */
    USART2->baudr = system_core_clock / 2 / FPGA_USART_BAUD;  /* APB1 = HCLK/2 */
    USART2->ctrl1 = 0;
    USART2->ctrl1 |= (1 << 2);   /* RE: Receiver enable */
    USART2->ctrl1 |= (1 << 3);   /* TE: Transmitter enable */
    USART2->ctrl1 |= (1 << 5);   /* RDBFIEN: RX interrupt enable */
    USART2->ctrl1 |= (1 << 13);  /* UEN: USART enable */

    /* Enable USART2 interrupt in NVIC */
    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_SetPriority(USART2_IRQn, 5);  /* Below FreeRTOS max syscall priority */

    /* ---------------------------------------------------------------
     * Step 3: Send FPGA boot commands via USART2
     * Stock firmware sends: 0x01, 0x02, 0x06, 0x07, 0x08
     * with delays between each for FPGA processing time.
     * --------------------------------------------------------------- */
    systick_delay_ms(100);  /* Wait for FPGA power-up */

    usart2_send_cmd(0x00, FPGA_CMD_INIT_01);
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_02);
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_06);
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_07);
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_08);
    systick_delay_ms(100);  /* Longer delay after last boot command */

    /* ---------------------------------------------------------------
     * Step 4: SPI3 peripheral init — Mode 3, Master, /2 prescaler
     *
     * AT32 SPI4 peripheral is at 0x40003C00 (same address as GD32 SPI3).
     * The AT32 HAL calls this SPI4, but we use direct register access
     * to match the stock firmware exactly.
     * --------------------------------------------------------------- */
    /* Enable SPI3/SPI4 clock (bit 15 of APB1EN in GD32 = SPI3) */
    /* AT32: CRM_SPI4_PERIPH_CLOCK maps to the same peripheral */
    crm_periph_clock_enable(CRM_SPI4_PERIPH_CLOCK, TRUE);

    /* PB3 = SPI3_SCK: AF push-pull, 50MHz */
    gpio_cfg.gpio_pins = GPIO_PINS_3;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB4 = SPI3_MISO: Input floating */
    gpio_cfg.gpio_pins = GPIO_PINS_4;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB5 = SPI3_MOSI: AF push-pull, 50MHz */
    gpio_cfg.gpio_pins = GPIO_PINS_5;
    gpio_cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB6 = SPI3_CS: GPIO output push-pull (software CS) */
    gpio_cfg.gpio_pins = GPIO_PINS_6;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    /* CS deassert (idle HIGH) */
    SPI3_CS_DEASSERT();

    /* PC6 = FPGA SPI enable: output push-pull, set HIGH */
    gpio_cfg.gpio_pins = GPIO_PINS_6;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOC, &gpio_cfg);
    GPIOC->scr = PC6_MASK;  /* PC6 HIGH — enable FPGA SPI */

    /* PB11 = FPGA active mode: output push-pull (set HIGH later) */
    gpio_cfg.gpio_pins = GPIO_PINS_11;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);

    /*
     * SPI3 register configuration (direct, matching stock firmware):
     *   CTRL1: Master, CPOL=1, CPHA=1, 8-bit, MSB first, SSM=1, SSI=1, /2
     *
     * Bit layout of SPI_CTRL1:
     *   [0]   CPHA   = 1 (Mode 3)
     *   [1]   CPOL   = 1 (Mode 3)
     *   [2]   MSTEN  = 1 (Master)
     *   [5:3] MDIV   = 000 (/2 prescaler → 60MHz from 120MHz APB1)
     *   [6]   SPIEN  = 0 (enable later)
     *   [7]   LTF    = 0 (MSB first)
     *   [8]   SWCSIN = 1 (SSI high)
     *   [9]   SWCSEN = 1 (SSM enable)
     *   [10]  RONLY  = 0 (full duplex)
     *   [11]  FBN    = 0 (8-bit)
     */
    FPGA_SPI->ctrl1 = (1 << 0)   /* CPHA = 1 */
               | (1 << 1)   /* CPOL = 1 */
               | (1 << 2)   /* MSTEN = 1 */
               | (0 << 3)   /* MDIV = /2 */
               | (1 << 8)   /* SWCSIN (SSI) = 1 */
               | (1 << 9);  /* SWCSEN (SSM) = 1 */

    /* Enable DMA/interrupt bits in CTRL2 (matches stock: CTL1 |= 0x03) */
    FPGA_SPI->ctrl2 |= 0x03;

    /* Enable SPI */
    FPGA_SPI->ctrl1 |= (1 << 6) /* SPE */;

    /* ---------------------------------------------------------------
     * Step 5: SysTick delays (stock firmware phases 6)
     * The FPGA needs time after SPI3 activation before handshake.
     * --------------------------------------------------------------- */
    systick_delay_ms(10);

    /* ---------------------------------------------------------------
     * Step 6: SPI3 FPGA handshake
     * Stock firmware: CS deassert, flush, CS assert, send 0x05, read
     * response, send 0x00, read, CS deassert, flush, delay.
     * --------------------------------------------------------------- */

    /* Flush: send dummy with CS high */
    spi3_xfer(0x00);
    (void)FPGA_SPI->dt;  /* Discard any stale data */

    /* Handshake: command 0x05 */
    SPI3_CS_ASSERT();
    spi3_xfer(0x05);   /* FPGA command — config/ID query */
    spi3_xfer(0x00);   /* Parameter byte */
    SPI3_CS_DEASSERT();

    /* Flush again */
    spi3_xfer(0x00);
    spi3_xfer(0x00);

    /* Post-handshake delay */
    systick_delay_ms(10);

    /* ---------------------------------------------------------------
     * Step 7: Additional SPI3 handshake commands
     * Stock firmware sends 0x12 and 0x15 queries.
     * --------------------------------------------------------------- */
    SPI3_CS_ASSERT();
    spi3_xfer(0x12);
    spi3_xfer(0x00);
    spi3_xfer(0x00);
    spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    systick_delay_ms(5);

    SPI3_CS_ASSERT();
    spi3_xfer(0x15);
    spi3_xfer(0x00);
    spi3_xfer(0x00);
    spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    systick_delay_ms(5);

    /* ---------------------------------------------------------------
     * Step 7b: SPI3 calibration data exchange
     * Stock firmware sends 411 bytes (137 entries x 3) of calibration
     * data to the FPGA via SPI3 commands 0x3B/0x3A.
     * Table extracted from V1.2.0 binary at 0x08051D19.
     * This configures the FPGA's internal ADC correction tables
     * and may activate the meter IC.
     * --------------------------------------------------------------- */
    {
        static const uint8_t fpga_cal_table[411] = {
            0x00,0x00,0x04,0x00,0x00,0x00,0x20,0x10,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0xEE,0xFF,0xFF,
            0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x20,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x08,0x00,0x20,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x42,0x40,0x00,0x00,0x01,0x00,0x00,0x00,0x24,
            0x00,0x26,0x00,0x00,0x00,0x02,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x46,0x00,0x10,0x00,0x80,0x02,0x00,0x8C,0x20,0x00,0x00,0x00,0x00,0x08,0x00,0x26,
            0x01,0x04,0x40,0x02,0x00,0x40,0x04,0x60,0x00,0x44,0x00,0x20,0x00,0x40,0x08,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x00,0x00,
            0x00,0x00,0x80,0x00,0x20,0x01,0x00,0x00,0x00,0x20,0x00,0x06,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x8C,0xFF,0xFF,
            0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x6D,0xF1,0xBC,0xE0,0x18,0x05,0x98,0xC0,0x00,0x00,0x00,0x90,
            0x00,0xB0,0x01,0x7C,0x4F,0xC0,0xF6,0x0C,0x0B,0x98,0xC1,0xC4,0x0D,0x83,0x11,0xE0,
            0x19,0x8E,0x60,0x42,0xC0,0xF6,0x9C,0xE0,0xC8,0x76,0x44,0x18,0x02,0xB2,0x7E,0xF9,
            0xAC,0x68,0xCA,0x34,0xA9,0x0C,0xA6,0x80,0x06,0xC0,0x00,0xA6,0x01,0xD2,0x49,0x8E,
            0x0E,0x00,0xBC,0x60,0x0C,0x60,0x18,0x00,0x1F,0x83,0xC3,0x00,0xC2,0x49,0x00,0x00,
            0x00,0x04,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x06,0x86,0x8B,0xCC,0x00,0x00,0x21,0x85,0x00,0x00,0x00,
            0x00,0x26,0xC0,0x08,0x02,0x00,0x20,0xEB,0x01,0x9F,0x59,0xAC,0x00,0x00,0x00,0x00,
            0x00,0x00,0x48,0x00,0x00,0x02,0x40,0x00,0x00,0x00,0x00,0x00,0x5E,0x29,0xFF,0xFF,
            0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x40,0x01,0x02,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x40,0x08,0x20,0x02,0x01,0x00,0x8A,0x06,0x00,0x22,0x00,0x80,0x20,0x42,0x00,
            0x01,0x00,0x00,0x04,0x00,0x28,0x0B,0x40,0x01,0x00,0x20,
        };

        /* Send 0x3B command header */
        SPI3_CS_ASSERT();
        spi3_xfer(0x3B);
        spi3_xfer(0x00);
        spi3_xfer(0x00);
        SPI3_CS_DEASSERT();

        systick_delay_ms(5);

        /* Send calibration data: 137 entries x 3 bytes each */
        SPI3_CS_ASSERT();
        spi3_xfer(0x3A);  /* Calibration data block command */
        for (int i = 0; i < 411; i++) {
            spi3_xfer(fpga_cal_table[i]);
        }
        SPI3_CS_DEASSERT();

        systick_delay_ms(10);
    }

    /* ---------------------------------------------------------------
     * Step 8: Set PB11 HIGH — FPGA active mode
     * Stock firmware does this in step 52, just before scheduler start.
     * --------------------------------------------------------------- */
    GPIOB->scr = PB11_MASK;  /* PB11 HIGH — FPGA active */

    /* ---------------------------------------------------------------
     * Step 9: Analog frontend + Meter IC activation
     * Stock firmware configures PB9 and PA6 as outputs during init
     * (discovered in master_init Phase 1 decompilation).
     * These pins may control the analog MUX or meter IC enable.
     * PC11 = meter analog MUX (from mode_switch decompilation).
     * --------------------------------------------------------------- */

    /* ---------------------------------------------------------------
     * Step 9: Analog frontend relay control
     * Decoded from stock firmware gpio_mux_portc_porte (FUN_080018A4).
     * These GPIO pins control physical relays that route the probe
     * signal to the meter IC's sigma-delta ADC.
     *
     * DC Voltage mode: PC12=HIGH, PE4=HIGH, PE5=LOW, PE6=HIGH
     * Without these, the meter IC has no analog input.
     * --------------------------------------------------------------- */

    /* Configure relay control pins as push-pull outputs */
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;

    /* PC12 — input routing relay */
    gpio_cfg.gpio_pins = GPIO_PINS_12;
    gpio_init(GPIOC, &gpio_cfg);

    /* PE4, PE5, PE6 — range/attenuation select */
    gpio_cfg.gpio_pins = GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6;
    gpio_init(GPIOE, &gpio_cfg);

    /* Set DC Voltage relay pattern */
    GPIOC->scr = (1U << 12);  /* PC12 HIGH — route probe to meter IC */
    GPIOE->scr = (1U << 4);   /* PE4 HIGH  — range select bit 0 */
    GPIOE->clr = (1U << 5);   /* PE5 LOW   — range select bit 1 */
    GPIOE->scr = (1U << 6);   /* PE6 HIGH  — attenuation/coupling */

    /* PB9, PA6 — additional analog frontend pins (from Phase 1 RE) */
    gpio_cfg.gpio_pins = GPIO_PINS_9;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1U << 9);

    gpio_cfg.gpio_pins = GPIO_PINS_6;
    gpio_init(GPIOA, &gpio_cfg);
    GPIOA->scr = (1U << 6);

    /* Gain resistor configuration — gpio_mux_porta_portb for DCV mode.
     * PA15, PA10 = gain select, PB10 = gain select, PB11 already set.
     * Without these, meter IC has wrong input gain → no measurement. */
    gpio_cfg.gpio_pins = GPIO_PINS_15 | GPIO_PINS_10;
    gpio_init(GPIOA, &gpio_cfg);
    GPIOA->scr = (1U << 15);  /* PA15 HIGH — gain bit */
    GPIOA->scr = (1U << 10);  /* PA10 HIGH — gain bit */

    gpio_cfg.gpio_pins = GPIO_PINS_10;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->clr = (1U << 10);  /* PB10 LOW — gain bit */

    /* PC11 — meter analog MUX enable */
    GPIOC->scr = (1U << 11);

    systick_delay_ms(50);  /* Let relays settle */

    /* Meter activation: cmd_hi=0x05 routes to meter IC subsystem!
     * Stock firmware TX queue items: 0x0508, 0x0509, 0x0507, 0x0514.
     * This was discovered by tracing direct TX queue writes in the binary. */
    usart2_send_cmd(0x05, 0x08);  /* Meter: configure */
    systick_delay_ms(10);
    usart2_send_cmd(0x05, 0x09);  /* Meter: start measurement */
    systick_delay_ms(10);

    /* Probe detect: read PC7 */
    if (GPIOC->idt & (1U << 7)) {
        usart2_send_cmd(0x05, 0x07);  /* Probe detected */
    } else {
        usart2_send_cmd(0x05, 0x0A);  /* No probe */
    }
    systick_delay_ms(10);

    usart2_send_cmd(0x05, 0x14);  /* Meter variant setup */
    systick_delay_ms(50);

    /* Meter channel gain/offset/coupling initialization (0x1A-0x1E).
     * Stock firmware meter_basic mode (case 1 in FUN_0800b908) sends these
     * at boot to configure the FPGA meter IC.
     *
     * Discovered 2026-04-04:
     *   param=0 → 10V range (1-10V accurate, BCD wraps at 10000 counts)
     *   param=1 → same as param=0 (no range change observed)
     *   Relay click heard at ~0.7V — FPGA controls some analog switching
     *   Below ~1V: readings incorrect (meter IC internal range mismatch)
     *   Above 10V: BCD wraps (11V→0.99, 12V→2, 13V→3)
     *
     * TODO: Find params for other ranges (600mV, 60V, 600V) to enable
     *       full auto-ranging. May require different command codes or
     *       MCU-side relay switching via gpio_mux functions. */
    usart2_send_cmd(0x00, FPGA_CMD_CH1_GAIN);    /* 0x1A: CH1 gain */
    systick_delay_ms(10);
    usart2_send_cmd(0x00, FPGA_CMD_CH1_OFFSET);  /* 0x1B: CH1 offset */
    systick_delay_ms(10);
    usart2_send_cmd(0x00, FPGA_CMD_CH2_GAIN);    /* 0x1C: CH2 gain */
    systick_delay_ms(10);
    usart2_send_cmd(0x00, FPGA_CMD_CH2_OFFSET);  /* 0x1D: CH2 offset */
    systick_delay_ms(10);
    usart2_send_cmd(0x00, FPGA_CMD_COUPLING);    /* 0x1E: coupling/BW */
    systick_delay_ms(50);

    fpga.initialized = true;
    fpga.acq_mode = FPGA_ACQ_NORMAL + 1;  /* Default to normal scope mode */
}

/* ═══════════════════════════════════════════════════════════════════
 * Task Creation
 * ═══════════════════════════════════════════════════════════════════ */

QueueHandle_t fpga_create_tasks(void)
{
    /* Only create FPGA tasks if init succeeded.
     * If fpga_init() failed or was skipped, the rest of the
     * firmware still works — just no scope/meter data. */
    if (!fpga.initialized) {
        return NULL;
    }

    /* Create queues */
    usart_tx_queue = xQueueCreate(10, sizeof(uint16_t));
    spi3_acq_queue = xQueueCreate(15, sizeof(uint8_t));
    meter_sem      = xSemaphoreCreateBinary();

    /* Create tasks (stack sizes and priorities match stock firmware) */
    xTaskCreate(fpga_usart_tx_task,    "dvom_TX", 64,  NULL, 2, &tx_task_handle);
    xTaskCreate(fpga_usart_rx_task,    "dvom_RX", 128, NULL, 3, &rx_task_handle);
    xTaskCreate(fpga_acquisition_task, "fpga",    256, NULL, 3, &acq_task_handle);

    return spi3_acq_queue;
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

BaseType_t fpga_send_cmd(uint8_t cmd_high, uint8_t cmd_low)
{
    if (!fpga.initialized) return pdFALSE;

    /* Use interrupt-driven TX via the dvom_TX task queue.
     * Non-blocking send — don't stall the calling task. */
    if (usart_tx_queue != NULL) {
        uint16_t item = ((uint16_t)cmd_high << 8) | cmd_low;
        return xQueueSend(usart_tx_queue, &item, 0);  /* non-blocking */
    }
    /* Fallback to polled if queue not created yet */
    usart2_send_cmd(cmd_high, cmd_low);
    return pdTRUE;
}

BaseType_t fpga_trigger_acquisition(uint8_t mode)
{
    if (spi3_acq_queue == NULL) return pdFALSE;
    return xQueueSend(spi3_acq_queue, &mode, 0);
}

bool fpga_data_ready(void)
{
    return data_ready;
}

const volatile uint8_t *fpga_get_ch1_buf(void)
{
    return fpga.initialized ? fpga.ch1_buf : NULL;
}

const volatile uint8_t *fpga_get_ch2_buf(void)
{
    return fpga.initialized ? fpga.ch2_buf : NULL;
}

void fpga_set_active(bool active)
{
    if (active) {
        GPIOB->scr = PB11_MASK;   /* PB11 HIGH */
    } else {
        GPIOB->clr = PB11_MASK;   /* PB11 LOW */
    }
    fpga.spi3_active = active;
}

void fpga_enter_scope_mode(void)
{
    if (!fpga.initialized) return;

    /* Stop DAC output if signal gen was running */
    {
        extern void dac_output_stop(void);
        extern bool dac_output_is_running(void);
        if (dac_output_is_running()) dac_output_stop();
    }

    /* Reset data_ready so display shows demo waveform until real data arrives */
    data_ready = false;

    /* Send scope init command sequence.
     * Stock firmware mode init dispatcher (FUN_0800b908, case 0) sends:
     *   0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
     *
     * In the stock firmware, these go through the dispatch table which reads
     * current scope state (coupling, V/div, trigger, timebase) and computes
     * the parameter byte for each. We send param=0 for now — the FPGA
     * will use default configuration. Once we see data flowing, we can
     * encode proper params from scope_state. */
    fpga_send_cmd(0x00, FPGA_CMD_RESET);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CH);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_0B);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_0C);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_0D);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_0E);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_0F);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_10);
    fpga_send_cmd(0x00, FPGA_CMD_SCOPE_CFG_11);

    /* Set acquisition mode for normal scope (1024 bytes interleaved) */
    fpga.acq_mode = FPGA_ACQ_NORMAL + 1;

    /* NOTE: Do NOT fire acquisition triggers here. The scope USART commands
     * need ~180ms to transmit at 9600 baud (9 cmds x 20ms each). If we
     * trigger SPI3 reads before the FPGA has processed them, spi3_xfer()
     * hangs in its polling loop and the watchdog resets the device.
     * Instead, the display task (scope_ui.c) fires the first trigger
     * after a short delay, once the FPGA has had time to configure. */
}

void fpga_enter_siggen_mode(void)
{
    if (!fpga.initialized) return;

    /* Send signal generator init command sequence.
     * Stock firmware mode init dispatcher (FUN_0800b908, case 2) sends:
     *   0x02, 0x03, 0x04, 0x05, 0x06, 0x08
     * Then falls through to case 9 tail: 0x14, 0x09, [0x07/0x0A]
     *
     * 0x02-0x06 = siggen setup (freq, wave, amplitude, offset, duty)
     * 0x08 = meter configure range (shared)
     * 0x14 = meter variant setup
     * 0x09 = meter start measurement
     * 0x07/0x0A = probe detect */
    fpga_send_cmd(0x00, 0x02);  /* Siggen: frequency */
    fpga_send_cmd(0x00, 0x03);  /* Siggen: waveform */
    fpga_send_cmd(0x00, 0x04);  /* Siggen: amplitude */
    fpga_send_cmd(0x00, 0x05);  /* Siggen: offset */
    fpga_send_cmd(0x00, 0x06);  /* Siggen: duty cycle */
    fpga_send_cmd(0x00, 0x08);  /* Meter: configure range */

    /* Case 9 tail: meter variant + probe detect */
    fpga_send_cmd(0x00, 0x14);
    fpga_send_cmd(0x00, FPGA_CMD_METER_START);

    /* Probe detect: read PC7 */
    if (GPIOC->idt & (1U << 7)) {
        fpga_send_cmd(0x00, 0x07);  /* Probe detected */
    } else {
        fpga_send_cmd(0x00, FPGA_CMD_METER_NOPROBE);
    }

    /* Switch analog MUX for signal gen output.
     * In meter mode: PC12=HIGH routes probe→meter IC, PE4/5/6 set range.
     * For signal gen: try reversing PC12 to route DAC→BNC output.
     * PC11 LOW (meter MUX off — not measuring). */
    GPIOC->clr = (1U << 11);  /* PC11 LOW — meter MUX off */
    GPIOC->clr = (1U << 12);  /* PC12 LOW — try routing DAC to BNC */
    GPIOE->clr = (1U << 4);   /* PE4 LOW — clear range select */
    GPIOE->clr = (1U << 5);   /* PE5 LOW */
    GPIOE->clr = (1U << 6);   /* PE6 LOW */
}

/* Helper: send probe detect command (shared by meter modes) */
static void fpga_send_probe_detect(void)
{
    if (GPIOC->idt & (1U << 7)) {
        fpga_send_cmd(0x00, 0x07);  /* PC7 HIGH: probe detected */
    } else {
        fpga_send_cmd(0x00, FPGA_CMD_METER_NOPROBE);
    }
}

void fpga_set_meter_mode(uint8_t submode)
{
    if (!fpga.initialized) return;

    /* Stop DAC output if signal gen was running */
    {
        extern void dac_output_stop(void);
        extern bool dac_output_is_running(void);
        if (dac_output_is_running()) dac_output_stop();
    }

    /* Ensure meter MUX is enabled */
    GPIOC->scr = (1U << 11);  /* PC11 HIGH — meter MUX on */

    /* Send mode-specific FPGA command sequence.
     * Mapping from RE analysis of mode init dispatcher (FUN_0800b908):
     *
     * Submodes 0-4 (DCV, ACV, DCA, ACA, unused) → system_mode 1 (basic meter)
     * Submode 5 (Frequency)                      → system_mode 4 (freq counter)
     * Submode 6 (Resistance)                     → system_mode 9 (meter variant)
     * Submode 7 (Continuity)                     → system_mode 8 (cont/diode)
     * Submode 8 (Diode)                          → system_mode 8 (cont/diode)
     * Submode 9 (Capacitance)                    → system_mode 3 (extended meter)
     */
    switch (submode) {

    case 0: /* DCV */
    case 1: /* ACV */
    case 2: /* DCA */
    case 3: /* ACA */
    case 4: /* (unused) */
    default:
        /* System mode 1: basic meter.
         * Commands: 0x00, 0x09, probe, 0x1A-0x1E */
        fpga_send_cmd(0x00, FPGA_CMD_RESET);
        fpga_send_cmd(0x00, FPGA_CMD_METER_START);
        fpga_send_probe_detect();
        fpga_send_cmd(0x00, FPGA_CMD_CH1_GAIN);
        fpga_send_cmd(0x00, FPGA_CMD_CH1_OFFSET);
        fpga_send_cmd(0x00, FPGA_CMD_CH2_GAIN);
        fpga_send_cmd(0x00, FPGA_CMD_CH2_OFFSET);
        fpga_send_cmd(0x00, FPGA_CMD_COUPLING);
        break;

    case 5: /* Frequency */
        /* System mode 4: frequency counter.
         * Commands: 0x00, 0x1F, 0x09, 0x20, 0x21 */
        fpga_send_cmd(0x00, FPGA_CMD_RESET);
        fpga_send_cmd(0x00, FPGA_CMD_FREQ_CFG);
        fpga_send_cmd(0x00, FPGA_CMD_METER_START);
        fpga_send_cmd(0x00, FPGA_CMD_FREQ_20);
        fpga_send_cmd(0x00, FPGA_CMD_FREQ_21);
        break;

    case 6: /* Resistance */
        /* System mode 9: meter variant.
         * Commands: 0x00, 0x12, 0x13, 0x14, 0x09, probe */
        fpga_send_cmd(0x00, FPGA_CMD_RESET);
        fpga_send_cmd(0x00, FPGA_CMD_METER_VAR_12);
        fpga_send_cmd(0x00, FPGA_CMD_METER_VAR_13);
        fpga_send_cmd(0x00, FPGA_CMD_METER_VAR_14);
        fpga_send_cmd(0x00, FPGA_CMD_METER_START);
        fpga_send_probe_detect();
        break;

    case 7: /* Continuity */
    case 8: /* Diode */
        /* System mode 8: continuity/diode.
         * Commands: 0x00, 0x2C */
        fpga_send_cmd(0x00, FPGA_CMD_RESET);
        fpga_send_cmd(0x00, FPGA_CMD_CONT_DIODE);
        break;

    case 9: /* Capacitance */
        /* System mode 3: extended meter.
         * Commands: 0x00, 0x08, 0x09, probe, 0x16-0x19 */
        fpga_send_cmd(0x00, FPGA_CMD_RESET);
        fpga_send_cmd(0x00, 0x08);
        fpga_send_cmd(0x00, FPGA_CMD_METER_START);
        fpga_send_probe_detect();
        fpga_send_cmd(0x00, 0x16);
        fpga_send_cmd(0x00, 0x17);
        fpga_send_cmd(0x00, 0x18);
        fpga_send_cmd(0x00, 0x19);
        break;
    }
}
