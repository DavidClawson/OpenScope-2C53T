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
    /* Wait for TX buffer empty */
    while (!(FPGA_SPI->sts & SPI_I2S_TDBE_FLAG)) {}
    FPGA_SPI->dt = tx_byte;

    /* Wait for RX buffer not empty */
    while (!(FPGA_SPI->sts & SPI_I2S_RDBF_FLAG)) {}
    return (uint8_t)FPGA_SPI->dt;
}

/* ═══════════════════════════════════════════════════════════════════
 * USART2 Byte-Level TX (used during boot, before tasks are running)
 * ═══════════════════════════════════════════════════════════════════ */

static void usart2_send_byte(uint8_t b)
{
    while (!(USART2->sts & USART_TDBE_FLAG)) {}
    USART2->dt = b;
}

static void usart2_send_frame(const uint8_t *frame)
{
    for (int i = 0; i < FPGA_TX_FRAME_SIZE; i++) {
        usart2_send_byte(frame[i]);
    }
    /* Wait for transmit complete */
    while (!(USART2->sts & USART_TDC_FLAG)) {}
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
    frame[9] = (cmd_hi + cmd_lo) & 0xFF;
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
                fpga.rx_index = 0;

                /* Signal meter processing task */
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                if (meter_sem != NULL) {
                    xSemaphoreGiveFromISR(meter_sem, &xHigherPriorityTaskWoken);
                }
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

            } else if (fpga.rx_buf[0] == FPGA_RX_ECHO_HDR_0 &&
                       fpga.rx_index >= 10) {
                /* Complete echo frame (10 bytes): just acknowledge */
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

        /* Build TX frame */
        fpga.tx_index = 0;
        memset((void *)fpga.tx_frame, 0, FPGA_TX_FRAME_SIZE);
        fpga.tx_frame[2] = cmd_hi;
        fpga.tx_frame[3] = cmd_lo;
        fpga.tx_frame[9] = (cmd_hi + cmd_lo) & 0xFF;

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
     * Step 7: Additional SPI3 configuration
     * Stock firmware sends command 0x12 and sets up ADC parameters.
     * --------------------------------------------------------------- */
    SPI3_CS_ASSERT();
    spi3_xfer(0x12);  /* ADC/trigger config command */
    SPI3_CS_DEASSERT();

    systick_delay_ms(5);

    /* ---------------------------------------------------------------
     * Step 8: Set PB11 HIGH — FPGA active mode
     * Stock firmware does this in step 52, just before scheduler start.
     * --------------------------------------------------------------- */
    GPIOB->scr = PB11_MASK;  /* PB11 HIGH — FPGA active */

    fpga.initialized = true;
    fpga.acq_mode = FPGA_ACQ_NORMAL + 1;  /* Default to normal scope mode */
}

/* ═══════════════════════════════════════════════════════════════════
 * Task Creation
 * ═══════════════════════════════════════════════════════════════════ */

QueueHandle_t fpga_create_tasks(void)
{
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
    if (usart_tx_queue == NULL) return pdFALSE;
    uint16_t item = ((uint16_t)cmd_high << 8) | cmd_low;
    return xQueueSend(usart_tx_queue, &item, 0);
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
