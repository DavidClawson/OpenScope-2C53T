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
#include "fpga_cal_table.h"
#include "meter_data.h"
#include "../ui/ui.h"
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
 * SPI3 register access.
 * AT32F403A SPI3 is at APB1 + 0x3C00 (same address as GD32's SPI2/STM32 SPI3).
 * PB3/PB4/PB5 map here after JTAG remap. SPI4 is a DIFFERENT peripheral
 * at APB1 + 0x4000 — do not confuse them.
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
    /* NOTE: byte[8] was previously 0xAA based on protocol doc, but the
     * stock frame builder does NOT set bytes[4-8] — they carry over from
     * command dispatchers (0 for simple commands). The 0xAA may have been
     * causing checksum validation failures on the FPGA side, explaining
     * zero echo frames. Now matches stock: bytes[4-8] = 0 for basic cmds. */
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
 * SPI3 IRQ Handler (stub)
 *
 * Stock firmware enables SPI3 IRQ #51 (NVIC_ISER1 bit 19).
 * We use polled SPI3, but enable the interrupt to match stock config.
 * This stub just clears any pending flags to prevent IRQ storms.
 * Compliance audit (2026-04-06): added to match stock init.
 * ═══════════════════════════════════════════════════════════════════ */

void SPI3_I2S3EXT_IRQHandler(void)
{
    /* Read STS and DR to clear any pending RXNE/TXE/OVR flags */
    volatile uint32_t sts = FPGA_SPI->sts;
    volatile uint32_t dr  = FPGA_SPI->dt;
    (void)sts;
    (void)dr;
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

        /* Build TX frame.
         * Stock firmware TX buffer retains bytes [4]-[8] from dispatch
         * handlers — for simple commands they're all 0 (BSS init).
         * We previously hardcoded byte[8]=0xAA based on protocol doc,
         * but this likely caused checksum failures (zero echo frames). */
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
 * Meter Poll Task
 *
 * The FPGA meter IC only emits a 12-byte data frame in response to a
 * "start measurement" command (0x00, 0x09). Without a continuous stream
 * of poll commands, the FPGA goes silent within ~5 frames and meter
 * readings freeze.
 *
 * Previously this poll lived inside draw_meter_screen() at meter_ui.c:768,
 * which tied the data acquisition cadence to the display refresh loop.
 * That worked by accident but coupled two unrelated concerns — if the UI
 * stopped redrawing (e.g. debug overlay, menu, backgrounded screen), data
 * flow stopped too.
 *
 * This task decouples the poll from the UI. It runs at ~4 Hz (matched to
 * the FPGA meter IC's natural ~3 Hz data cadence) and only polls while
 * the user is in meter mode.
 *
 * Root-cause analysis: reverse_engineering/analysis_v120/usart2_isr_state_machine.md
 */
static void fpga_meter_poll_task(void *pv)
{
    (void)pv;
    extern volatile device_mode_t current_mode;  /* from ui.h via main.c */

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));  /* ~4 Hz */
        if (fpga.initialized && current_mode == MODE_MULTIMETER) {
            fpga_send_cmd(0x00, 0x09);  /* Meter: start measurement */
        }
    }
}

/*
 * SPI3 Acquisition Task (fpga equivalent)
 * Waits on spi3_acq_queue for trigger events, then performs SPI3
 * transfers to read ADC sample data from FPGA.
 *
 * Stock firmware protocol (from fpga_task_annotated.c lines 775-925):
 *   1. Pre-acquisition CS transaction:
 *      CS_ASSERT → spi3_xfer(command_code) → CS_DEASSERT
 *      command_code = ~0x7F ^ voltage_range = 0x80 | (voltage_range & 0x7F)
 *      This tells the FPGA what acquisition to prepare.
 *
 *   2. Bulk data CS transaction (case 2 = normal scope):
 *      CS_ASSERT → spi3_xfer(0xFF) [discard echo] →
 *      512× { spi3_xfer(0xFF) [CH1], spi3_xfer(0xFF) [CH2] } →
 *      CS_DEASSERT
 *
 * Without step 1, the FPGA returns constant/empty data (all 0xFF or
 * all same value) because it hasn't been told what to acquire.
 */

static void fpga_acquisition_task(void *pv)
{
    (void)pv;
    uint8_t trigger_byte;

    /* Backoff: after consecutive timeouts, wait before retrying */
    #define SPI3_BACKOFF_THRESHOLD  5
    #define SPI3_BACKOFF_MS         2000

    for (;;) {
        /* Wait for trigger from input/housekeeping or timer */
        xQueueReceive(spi3_acq_queue, &trigger_byte, portMAX_DELAY);

        if (!fpga.initialized) continue;

        /* Backoff: if we've timed out too many times, pause */
        if (fpga.spi3_timeout_count >= SPI3_BACKOFF_THRESHOLD) {
            fpga.spi3_timeout_count = 0;  /* Reset for next round */
            vTaskDelay(pdMS_TO_TICKS(SPI3_BACKOFF_MS));
            /* Drain any queued triggers that piled up during backoff */
            while (xQueueReceive(spi3_acq_queue, &trigger_byte, 0) == pdTRUE) {}
        }

        fpga.spi3_probing = true;

        /*
         * SPI3 Acquisition Protocol (stock firmware fpga_task_annotated.c):
         *
         * Transaction 1 — tell FPGA what to acquire:
         *   CS_ASSERT → spi3_xfer(command_code) → CS_DEASSERT
         *   command_code = 0x80 | (voltage_range & 0x7F)
         *
         * Transaction 2 — bulk data read:
         *   CS_ASSERT → spi3_xfer(0xFF) [discard] →
         *   512× { spi3_xfer(0xFF) [CH1], spi3_xfer(0xFF) [CH2] } →
         *   CS_DEASSERT
         *
         * Previously we skipped transaction 1 because "it didn't matter"
         * — but that was tested when PC6 was HIGH (FPGA SPI disabled).
         * Now that PC6 is LOW and compliance fixes are in, the FPGA may
         * need the command_code to arm its sample buffer.
         */

        /* Transaction 1: Pre-acquisition command */
        SPI3_CS_ASSERT();
        spi3_xfer(0x80);  /* command_code = 0x80 | voltage_range(0) */
        SPI3_CS_DEASSERT();

        /* Brief pause between transactions (stock firmware has a few cycles) */
        for (volatile int d = 0; d < 100; d++) {}

        /* Transaction 2: Bulk data read */
        SPI3_CS_ASSERT();

        /* First byte: 0xFF (stock firmware case 2), echo is discarded */
        uint8_t echo = spi3_xfer(0xFF);
        fpga.spi3_first_byte = echo;

        switch (trigger_byte) {

        case 3: /* FPGA_ACQ_NORMAL + 1: Normal scope, 1024 bytes interleaved */
        {
            /* Read 512 interleaved CH1/CH2 sample pairs (1024 bytes total).
             * Stock firmware: ms[0x5B0 + i] for even=CH1, odd=CH2.
             * We separate into ch1_buf/ch2_buf for cleaner rendering. */
            {
                uint8_t first_raw = 0;
                uint8_t varies = 0;

                for (int i = 0; i < 512; i++) {
                    uint8_t ch1_raw = spi3_xfer(0xFF);
                    uint8_t ch2_raw = spi3_xfer(0xFF);

                    /* Capture first 4 raw bytes for diagnostics */
                    if (i < 4) {
                        fpga.diag_ch1_raw[i] = ch1_raw;
                        fpga.diag_ch2_raw[i] = ch2_raw;
                    }

                    /* Track if data varies */
                    if (i == 0) first_raw = ch1_raw;
                    else if (ch1_raw != first_raw) varies = 1;

                    int16_t ch1_cal = (int16_t)ch1_raw + (int16_t)FPGA_ADC_OFFSET;
                    int16_t ch2_cal = (int16_t)ch2_raw + (int16_t)FPGA_ADC_OFFSET;

                    if (ch1_cal < 0) ch1_cal = 0;
                    if (ch1_cal > 255) ch1_cal = 255;
                    if (ch2_cal < 0) ch2_cal = 0;
                    if (ch2_cal > 255) ch2_cal = 255;

                    fpga.ch1_buf[i] = (uint8_t)ch1_cal;
                    fpga.ch2_buf[i] = (uint8_t)ch2_cal;
                }

                fpga.diag_data_varies = varies;
            }

            /* Always count as OK for now — we need to see what the FPGA
             * sends even if it's constant data. The display will show a
             * flat line for constant data, which is diagnostic info. */
            fpga.spi3_ok_count++;
            fpga.spi3_timeout_count = 0;
            data_ready = true;
            break;
        }

        case 4: /* FPGA_ACQ_DUAL + 1: Dual channel, 2048 bytes */
        {
            /* Stock firmware case 3: reads 0x800 bytes.
             * Same protocol — 0xFF command, then bulk read. */
            for (int i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
                uint8_t raw = spi3_xfer(0xFF);
                int16_t cal = (int16_t)raw + (int16_t)FPGA_ADC_OFFSET;
                if (cal < 0) cal = 0;
                if (cal > 255) cal = 255;
                fpga.ch1_buf[i] = (uint8_t)cal;
            }
            for (int i = 0; i < FPGA_ADC_BUF_SIZE; i++) {
                uint8_t raw = spi3_xfer(0xFF);
                int16_t cal = (int16_t)raw + (int16_t)FPGA_ADC_OFFSET;
                if (cal < 0) cal = 0;
                if (cal > 255) cal = 255;
                fpga.ch2_buf[i] = (uint8_t)cal;
            }

            fpga.spi3_ok_count++;
            fpga.spi3_timeout_count = 0;
            data_ready = true;
            break;
        }

        case 2: /* FPGA_ACQ_ROLL + 1: Roll mode */
        {
            /* Stock firmware case 1: reads 5 bytes for rolling display.
             * CS_ASSERT → 5× spi3_xfer(0xFF) → CS_DEASSERT
             * Bytes: ref, ch1_hi, ch1_lo, ch2_hi, ch2_lo */
            uint8_t roll_b1 = spi3_xfer(0xFF);
            uint8_t roll_b2 = spi3_xfer(0xFF);
            uint8_t roll_b3 = spi3_xfer(0xFF);
            uint8_t roll_b4 = spi3_xfer(0xFF);
            (void)roll_b1; (void)roll_b2; (void)roll_b3; (void)roll_b4;

            /* TODO: store roll samples into circular buffer properly */

            fpga.spi3_ok_count++;
            fpga.spi3_timeout_count = 0;
            data_ready = true;
            break;
        }

        default:
            break;
        }

        /* CS deassert */
        SPI3_CS_DEASSERT();
        fpga.spi3_probing = false;
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

    /* Disable JTAG and configure SPI3 pin mapping.
     *
     * AT32F403A has TWO remap systems:
     *   1. STM32-compatible: IOMUX->remap (offset 0x04) — same as GD32 AFIO_PCF0
     *      - Bits [26:24] swjtag_mux: 010 = disable JTAG, keep SWD
     *      - Bit [28] spi3_mux: 0 = PB3/PB4/PB5 (DEFAULT), 1 = PC10/PC11/PC12
     *   2. Extended GMUX: IOMUX->remap5/remap7 — AT32-specific
     *
     * The stock GD32 firmware uses system 1: writes AFIO_PCF0 bits [26:24]=010
     * and leaves bit 28=0 (SPI3 on PB3/PB4/PB5 by default). We must use the
     * same compatible register so both remap systems agree.
     */
    /* Read-modify-write the compatible remap register:
     * - Set bits [26:24] = 010 (JTAG off, SWD on)
     * - Clear bit [28] = 0 (SPI3 on PB3/PB4/PB5)
     * Stock firmware: (reg & ~0xF000) | 0x2000 at AFIO+0x08 per CLAUDE.md,
     * but the actual SWJ_CFG is at bits [26:24] of offset 0x04. */
    /* AT32F403A requires BOTH legacy remap AND GMUX configuration.
     * Unlike STM32F1, the AT32 GMUX system OVERRIDES the legacy remap.
     * GMUX=0000 (default) means SPI3 is NOT connected to any pins!
     *
     * Required settings:
     *   1. SWJTAG_GMUX_010: Disable JTAG-DP, keep SW-DP (frees PB3/PB4/PB5)
     *   2. SPI3_GMUX_0010:  Route SPI3 to PB3(SCK)/PB4(MISO)/PB5(MOSI)
     *
     * From AT32 example: spi/halfduplex_dma_jtagpin/src/main.c lines 173-174.
     * The AT32 HAL gpio_pin_remap_config() handles both legacy and GMUX regs.
     */
    /* AT32F403A pin remapping — need BOTH legacy AND GMUX for JTAG disable.
     *
     * The legacy SWJ_CFG in IOMUX->remap (offset 0x04) defaults to 000
     * (full JTAG enabled) on reset. PB3=JTDO, PB4=NJTRST in that state.
     * The GMUX SWJTAG in remap7 (offset 0x30) is a SEPARATE register.
     * Both must be set to free PB3/PB4 for SPI3 use.
     *
     * Legacy: SWJ_CFG bits [26:24] = 010 → JTAG off, SWD on
     *         Do NOT touch bit 28 (SPI3_MUX) — let GMUX handle SPI3 routing
     * GMUX:  SWJTAG = 010, SPI3 = 0010 (PB3/PB4/PB5)
     */
    /* Legacy JTAG disable — write-only bits, only modify SWJ_CFG [26:24] */
    {
        uint32_t remap = IOMUX->remap;
        remap &= ~(0x7u << 24);   /* Clear SWJ_CFG bits */
        remap |= (0x2u << 24);    /* Set SWJ_CFG = 010 (JTAG off, SWD on) */
        IOMUX->remap = remap;
    }

    /* GMUX remap (AT32-specific, controls actual pin mux).
     *
     * CRITICAL: Stock firmware NEVER writes SPI3_GMUX (remap5).
     * It leaves bits [27:24] at reset default 0000, which means
     * "use legacy remap path." The legacy remap + JTAG disable
     * is sufficient to route SPI3 to PB3/PB4/PB5.
     *
     * Our previous SPI3_GMUX_0010 call was WRONG — it actively
     * overrode the legacy path with a GMUX value that may resolve
     * pin routing differently on the AT32, causing MISO to be
     * disconnected. Discovered 2026-04-07 by comparing register
     * writes in stock master_init decompilation. */
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    /* SPI3_GMUX_0010 deliberately NOT called — match stock firmware */

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
     * Stock firmware sends ALL of 0x01-0x08 during boot.
     * Compliance audit (2026-04-06) found we were missing 0x03-0x05.
     * --------------------------------------------------------------- */
    systick_delay_ms(100);  /* Wait for FPGA power-up */

    usart2_send_cmd(0x00, FPGA_CMD_INIT_01);  /* 0x01: Channel init */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_02);  /* 0x02: Signal gen setup */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, 0x03);  /* 0x03: Trigger config */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, 0x04);  /* 0x04: Vertical scale */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, 0x05);  /* 0x05: Channel enable */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_06);  /* 0x06: Signal gen setup */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_07);  /* 0x07: Meter probe detect */
    systick_delay_ms(50);

    usart2_send_cmd(0x00, FPGA_CMD_INIT_08);  /* 0x08: Meter configure */
    systick_delay_ms(100);  /* Longer delay after last boot command */

    /* ---------------------------------------------------------------
     * Step 4: SPI3 peripheral init — Mode 3, Master, /2 prescaler
     *
     * AT32 SPI4 peripheral is at 0x40003C00 (same address as GD32 SPI3).
     * The AT32 HAL calls this SPI4, but we use direct register access
     * to match the stock firmware exactly.
     * --------------------------------------------------------------- */
    /* Enable SPI3 clock (bit 15 of APB1EN).
     * BUG FIX: was CRM_SPI4_PERIPH_CLOCK (bit 16) — wrong peripheral!
     * On AT32F403A, SPI3 (0x40003C00) and SPI4 (0x40004000) are separate.
     * PB3/PB4/PB5 map to SPI3, not SPI4. */
    crm_periph_clock_enable(CRM_SPI3_PERIPH_CLOCK, TRUE);

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
    GPIOC->scr = PC6_MASK;  /* PC6 HIGH — FPGA SPI enable (match stock) */

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
               /* BR[2:0] = 000 → /2 prescaler = 60MHz from 120MHz APB1.
                * Compliance audit (2026-04-06): stock uses /2 (60MHz),
                * confirmed by fpga_task_annotated.c, FPGA_TASK_ANALYSIS.md,
                * and remaining_unknowns.md. We had (1<<3) = /4 = 30MHz
                * which was WRONG — half the expected clock rate. */
               | (1 << 8)   /* SWCSIN (SSI) = 1 */
               | (1 << 9);  /* SWCSEN (SSM) = 1 */

    /* Stock firmware sets CTL1 |= 0x03 (RXDMAEN + TXDMAEN).
     * Compliance audit (2026-04-06): our previous comment that "DMA must
     * be DISABLED or the data register won't work" was WRONG. On AT32/STM32F1,
     * setting DMA enable bits without configuring DMA channels just causes
     * ignored DMA requests — polled DR access still works fine. The FPGA
     * may depend on seeing these DMA request signals as part of its SPI
     * slave handshake. Match stock exactly. */
    FPGA_SPI->ctrl2 = 0x03;  /* RXDMAEN + TXDMAEN (match stock) */

    /* Enable SPI */
    FPGA_SPI->ctrl1 |= (1 << 6) /* SPE */;

    /* Enable SPI3 interrupt in NVIC (stock enables IRQ #51).
     * We use polled transfers, but the stock firmware enables this and the
     * FPGA may expect it. Stub handler below clears any pending flags. */
    NVIC_EnableIRQ(SPI3_I2S3EXT_IRQn);

    /* Capture register state for diagnostics */
    fpga.diag_remap5 = IOMUX->remap;   /* STM32-compatible remap (offset 0x04) */
    fpga.diag_remap7 = IOMUX->remap5;  /* GMUX remap5 (spi3_gmux) */
    fpga.diag_spi_ctrl1 = FPGA_SPI->ctrl1;
    fpga.diag_spi_sts = FPGA_SPI->sts;

    /* ---------------------------------------------------------------
     * Step 5: SysTick delays (stock firmware phases 6)
     * The FPGA needs time after SPI3 activation before handshake.
     * Stock firmware has ~20ms of multi-phase SysTick delays here.
     * Compliance audit (2026-04-06): we had 10ms, stock has ~20ms.
     * --------------------------------------------------------------- */
    systick_delay_ms(10);
    systick_delay_ms(5);
    systick_delay_ms(5);

    /* ---------------------------------------------------------------
     * Step 6: SPI3 FPGA handshake
     * Unicorn trace (2026-04-06) shows EXACTLY this sequence:
     *   CS HIGH → send 0x00 flush
     *   CS LOW  → send 0x05, 0x00 (2 bytes only!)
     *   CS HIGH → send 0x00 flush
     *   [delay]
     *   CS LOW  → send 0x12, 0x00 (2 bytes)
     *   CS HIGH → send 0x00 flush
     *   [delay]
     *   CS LOW  → send 0x15, 0x00 (2 bytes)
     *   CS HIGH → send 0x00 flush
     *
     * CORRECTION: The earlier compliance audit agents said 4 bytes per
     * CS transaction. The Unicorn trace proves it's 2. Extra bytes
     * within CS could confuse the FPGA's SPI slave state machine.
     * --------------------------------------------------------------- */

    /* Flush: send dummy with CS high */
    SPI3_CS_DEASSERT();  /* Ensure CS is HIGH */
    fpga.init_hs[6] = spi3_xfer(0x00);  /* Dummy flush (CS high) */
    fpga.diag_spi_sts = FPGA_SPI->sts;  /* STS after first transfer */
    (void)FPGA_SPI->dt;  /* Discard any stale data */

    /* Handshake: command 0x05 — exactly 2 bytes within CS LOW */
    SPI3_CS_ASSERT();
    fpga.init_hs[0] = spi3_xfer(0x05);   /* FPGA command — config/ID query */
    fpga.init_hs[1] = spi3_xfer(0x00);   /* Parameter byte */
    SPI3_CS_DEASSERT();

    /* Flush with CS high */
    spi3_xfer(0x00);

    /* Post-handshake delay */
    systick_delay_ms(10);

    /* ---------------------------------------------------------------
     * Step 7: Additional SPI3 commands (0x12, 0x15)
     * Unicorn confirmed: 2 bytes per CS transaction + flush.
     * --------------------------------------------------------------- */
    SPI3_CS_ASSERT();
    fpga.init_hs[2] = spi3_xfer(0x12);
    fpga.init_hs[3] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    spi3_xfer(0x00);  /* flush */

    systick_delay_ms(5);

    SPI3_CS_ASSERT();
    fpga.init_hs[4] = spi3_xfer(0x15);
    fpga.init_hs[5] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    spi3_xfer(0x00);  /* flush */

    systick_delay_ms(5);

    /* ---------------------------------------------------------------
     * Step 7b: SPI3 bulk FPGA register-init table upload
     *
     * Stock firmware sends 115,638 bytes from flash (0x08051D19)
     * bracketed by SPI3 opcodes 0x3B (start) and 0x3A (end).
     * Structure: 544 sync-framed blocks (Region A, 87,040 bytes)
     * followed by dense coefficients (Region B, 28,598 bytes).
     * See: analysis_v120/h2_extracted/FINDINGS.md
     *
     * Previous attempts failed due to wrong SPI3 clock (SPI4 bit)
     * and DMA enabled during polled transfer. Both bugs are now fixed.
     *
     * The FPGA's SPI data interface appears to remain inactive until
     * this table is uploaded (bit-bang GPIO test confirmed MISO
     * stays idle-HIGH without it).
     * --------------------------------------------------------------- */
    SPI3_CS_ASSERT();
    spi3_xfer(0x3B);  /* Begin bulk upload opcode */
    spi3_xfer(0x00);  /* Flush byte (stock firmware sends this after opcode) */

    /* Stream the entire 115,638-byte table */
    for (uint32_t i = 0; i < FPGA_H2_CAL_TABLE_SIZE; i++) {
        spi3_xfer(fpga_h2_cal_table[i]);
        fpga.h2_bytes_sent = i + 1;
    }

    spi3_xfer(0x3A);  /* End bulk upload opcode */
    spi3_xfer(0x00);  /* Flush byte (stock firmware sends this after close) */
    SPI3_CS_DEASSERT();
    fpga.h2_upload_done = 1;

    systick_delay_ms(50);  /* Let FPGA process the table */

    /* PB11 HIGH (FPGA active mode) deferred to after all configuration.
     * Stock firmware sets PB11 in step 52, just before vTaskStartScheduler().
     * Compliance audit (2026-04-06): we were setting it too early — moved
     * to after analog frontend, meter commands, and gain/offset init. */

    /* ---------------------------------------------------------------
     * Step 8: Analog frontend + Meter IC activation
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

    /* PC11 — meter analog MUX enable.
     * Compliance audit (2026-04-06): was missing gpio_init() — PC11
     * defaults to floating input on reset, so the scr write was silently
     * ignored. The meter MUX was never actually enabled. */
    gpio_cfg.gpio_pins = GPIO_PINS_11;
    gpio_init(GPIOC, &gpio_cfg);
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

    /* ---------------------------------------------------------------
     * Step 9b: Set PB11 HIGH — FPGA active mode
     * Stock firmware does this in step 52, just before scheduler start,
     * AFTER all peripheral config, timer setup, and FPGA commands.
     * Compliance audit (2026-04-06): moved from before analog frontend
     * to after all configuration is complete.
     * --------------------------------------------------------------- */
    GPIOB->scr = PB11_MASK;  /* PB11 HIGH — FPGA active */

    /* ---------------------------------------------------------------
     * Step 10: Post-init SPI3 probe
     * --------------------------------------------------------------- */
    systick_delay_ms(100);  /* Give FPGA time to settle */

    /* Test 1: SPI peripheral probe */
    SPI3_CS_ASSERT();
    fpga.init_hs[6] = spi3_xfer(0xFF);  /* Post-init probe byte 1 */
    fpga.init_hs[7] = spi3_xfer(0xFF);  /* Post-init probe byte 2 */
    SPI3_CS_DEASSERT();

    systick_delay_ms(10);

    /* Bit-bang test REMOVED — it was disrupting the GMUX pin connection.
     * The GMUX fix (SPI3_GMUX_0010) was the real issue, not the protocol.
     * See project_spi3_miso_dead.md for the bit-bang test results. */

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
    xTaskCreate(fpga_usart_tx_task,    "dvom_TX",   64,  NULL, 2, &tx_task_handle);
    xTaskCreate(fpga_usart_rx_task,    "dvom_RX",   128, NULL, 3, &rx_task_handle);
    xTaskCreate(fpga_acquisition_task, "fpga",      256, NULL, 3, &acq_task_handle);
    xTaskCreate(fpga_meter_poll_task,  "meter_poll", 64, NULL, 2, NULL);

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

void fpga_send_raw_frame(const uint8_t *frame)
{
    usart2_send_frame(frame);
}
