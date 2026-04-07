/*
 * OpenScope 2C53T - FPGA Communication Driver
 *
 * Dual-channel interface to the Gowin GW1N-UV2 FPGA:
 *   - USART2 (9600 baud): Command/control channel (PA2=TX, PA3=RX)
 *   - SPI3   (60MHz):     Bulk ADC data channel (PB3/4/5, PB6=CS)
 *
 * Control pins:
 *   - PC6:  FPGA SPI enable (must be HIGH for SPI3 to work)
 *   - PB11: FPGA active mode (must be HIGH during acquisition)
 *
 * Based on RE analysis of stock firmware:
 *   - reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md
 *   - reverse_engineering/analysis_v120/FPGA_BOOT_SEQUENCE.md
 */

#ifndef FPGA_H
#define FPGA_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

/* ═══════════════════════════════════════════════════════════════════
 * ADC Buffer Sizes
 * ═══════════════════════════════════════════════════════════════════ */

#define FPGA_ADC_BUF_SIZE     1024   /* Normal mode: 512 sample pairs */
#define FPGA_ROLL_BUF_SIZE    300    /* Roll mode: circular buffer */

/* ═══════════════════════════════════════════════════════════════════
 * USART Protocol Constants
 * ═══════════════════════════════════════════════════════════════════ */

#define FPGA_USART_BAUD       9600
#define FPGA_TX_FRAME_SIZE    10
#define FPGA_RX_FRAME_SIZE    12

/* RX frame headers */
#define FPGA_RX_DATA_HDR_0    0x5A   /* Data frame: 0x5A 0xA5 */
#define FPGA_RX_DATA_HDR_1    0xA5
#define FPGA_RX_ECHO_HDR_0    0xAA   /* Echo frame: 0xAA 0x55 */
#define FPGA_RX_ECHO_HDR_1    0x55

/* ═══════════════════════════════════════════════════════════════════
 * FPGA Command Codes (USART TX)
 * ═══════════════════════════════════════════════════════════════════ */

/* Boot sequence commands (sent during init, before SPI3 activation) */
#define FPGA_CMD_INIT_01      0x01   /* Channel init */
#define FPGA_CMD_INIT_02      0x02   /* Signal gen setup */
#define FPGA_CMD_INIT_06      0x06   /* Signal gen setup */
#define FPGA_CMD_INIT_07      0x07   /* Meter probe detect */
#define FPGA_CMD_INIT_08      0x08   /* Meter configure */

/* Runtime commands */
#define FPGA_CMD_RESET        0x00
#define FPGA_CMD_SCOPE_CH     0x01   /* Scope channel config */
#define FPGA_CMD_METER_START  0x09   /* Start meter measurement */
#define FPGA_CMD_METER_NOPROBE 0x0A  /* No probe detected */

/* Scope configuration commands (case 0 of mode init dispatcher FUN_0800b908).
 * Sent as a sequence when entering oscilloscope mode: 0x0B-0x11.
 * Each dispatches through the FPGA command table to a config writer that
 * encodes channel coupling/BW, voltage range, trigger, and timebase. */
#define FPGA_CMD_SCOPE_CFG_0B 0x0B   /* Scope config: CH1 coupling/range */
#define FPGA_CMD_SCOPE_CFG_0C 0x0C   /* Scope config: CH2 coupling/range */
#define FPGA_CMD_SCOPE_CFG_0D 0x0D   /* Scope config: trigger threshold */
#define FPGA_CMD_SCOPE_CFG_0E 0x0E   /* Scope config: trigger mode/edge */
#define FPGA_CMD_SCOPE_CFG_0F 0x0F   /* Scope config: timebase prescaler */
#define FPGA_CMD_SCOPE_CFG_10 0x10   /* Scope config: timebase period */
#define FPGA_CMD_SCOPE_CFG_11 0x11   /* Scope config: timebase mode */

/* Meter variant setup (system_mode 9: resistance) */
#define FPGA_CMD_METER_VAR_12 0x12   /* Meter variant config */
#define FPGA_CMD_METER_VAR_13 0x13   /* Meter variant config */
#define FPGA_CMD_METER_VAR_14 0x14   /* Meter variant config */

/* Channel gain/offset/coupling (sent at boot + runtime auto-range) */
#define FPGA_CMD_CH1_GAIN     0x1A   /* CH1 gain setting */
#define FPGA_CMD_CH1_OFFSET   0x1B   /* CH1 offset setting */
#define FPGA_CMD_CH2_GAIN     0x1C   /* CH2 gain setting */
#define FPGA_CMD_CH2_OFFSET   0x1D   /* CH2 offset setting */
#define FPGA_CMD_COUPLING     0x1E   /* Coupling / bandwidth limit */

/* Frequency counter (system_mode 4) */
#define FPGA_CMD_FREQ_CFG     0x1F   /* Freq counter config */
#define FPGA_CMD_FREQ_20      0x20   /* Freq counter param */
#define FPGA_CMD_FREQ_21      0x21   /* Freq counter param */

/* Continuity/Diode (system_mode 8) */
#define FPGA_CMD_CONT_DIODE   0x2C   /* Continuity/diode mode */

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 Acquisition Modes (trigger_byte - 1)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    FPGA_ACQ_FAST_TB     = 0,  /* Fast timebase config only */
    FPGA_ACQ_ROLL        = 1,  /* Roll mode (circular buffer, 300 samples) */
    FPGA_ACQ_NORMAL      = 2,  /* Normal scope (1024 bytes, interleaved) */
    FPGA_ACQ_DUAL        = 3,  /* Dual channel (2048 bytes, split) */
    FPGA_ACQ_EXTENDED    = 4,  /* Extended command only */
    FPGA_ACQ_METER_ADC   = 5,  /* Meter ADC read */
    FPGA_ACQ_SIGGEN      = 6,  /* Signal gen feedback */
    FPGA_ACQ_CALIBRATE   = 7,  /* Calibration readback */
    FPGA_ACQ_SELF_TEST   = 8,  /* Self test */
} fpga_acq_mode_t;

/* ═══════════════════════════════════════════════════════════════════
 * FPGA State
 * ═══════════════════════════════════════════════════════════════════ */

/* ADC calibration constants (from stock firmware VFP registers) */
#define FPGA_ADC_OFFSET       (-28.0f)   /* s16: Hardware DC offset */
#define FPGA_ADC_MAX          255.0f     /* s24: Maximum clamp */
#define FPGA_ADC_MIN          0.0f       /* s26: Minimum clamp */

typedef struct {
    /* ADC sample buffers (written by acquisition task, read by display) */
    volatile uint8_t ch1_buf[FPGA_ADC_BUF_SIZE];
    volatile uint8_t ch2_buf[FPGA_ADC_BUF_SIZE];

    /* Roll mode circular buffers */
    volatile uint8_t roll_ch1[FPGA_ROLL_BUF_SIZE];
    volatile uint8_t roll_ch2[FPGA_ROLL_BUF_SIZE];
    volatile uint16_t roll_write_idx;
    volatile uint16_t roll_count;

    /* USART RX frame (latest complete frame from FPGA) */
    volatile uint8_t rx_frame[FPGA_RX_FRAME_SIZE];
    volatile bool    rx_frame_valid;

    /* USART TX frame buffer */
    volatile uint8_t tx_frame[FPGA_TX_FRAME_SIZE];
    volatile uint8_t tx_index;

    /* RX state machine */
    volatile uint8_t rx_buf[FPGA_RX_FRAME_SIZE];
    volatile uint8_t rx_index;

    /* Status */
    volatile bool    initialized;      /* Boot sequence complete */
    volatile bool    spi3_active;      /* SPI3 acquisition running */
    volatile uint16_t frame_count;     /* Data frame counter (0x5A 0xA5) */
    volatile uint16_t echo_count;     /* Echo frame counter (0xAA 0x55) */
    volatile uint16_t tx_count;       /* TX commands sent */
    volatile uint16_t rx_byte_count;  /* Raw RX bytes received */

    /* Acquisition mode (set by mode switch, read by acq task) */
    volatile uint8_t acq_mode;         /* fpga_acq_mode_t */

    /* SPI3 acquisition diagnostics */
    volatile uint16_t spi3_ok_count;       /* Successful acquisitions */
    volatile uint16_t spi3_timeout_count;  /* Consecutive timeouts (resets on success) */
    volatile uint16_t spi3_total_timeouts; /* Lifetime timeout count */
    volatile uint8_t  spi3_first_byte;     /* First byte from last probe */
    volatile bool     spi3_probing;        /* Currently attempting acquisition */

    /* Raw sample diagnostics (first 4 bytes of each channel, updated each read) */
    volatile uint8_t  diag_ch1_raw[4];     /* First 4 raw CH1 bytes (before cal) */
    volatile uint8_t  diag_ch2_raw[4];     /* First 4 raw CH2 bytes (before cal) */
    volatile uint8_t  diag_data_varies;    /* 1 if data varies within read, 0 if constant */

    /* Bit-bang GPIO test results (set once during init, never overwritten) */
    volatile uint8_t  bb_idle;             /* PB4 before CS assert (expect 1) */
    volatile uint8_t  bb_cs;              /* PB4 after CS assert (0 = FPGA responds!) */
    volatile uint8_t  bb_byte;            /* 8-bit MISO from manual SCK toggle */
    volatile uint8_t  bb_marker;          /* 0xBB = bit-bang test ran */

    /* Init handshake diagnostic (captured during fpga_init) */
    volatile uint8_t  init_hs[8];          /* Handshake response bytes */
    volatile uint32_t diag_remap5;         /* IOMUX remap5 (spi3_gmux) */
    volatile uint32_t diag_remap7;         /* IOMUX remap7 (swjtag_gmux) */
    volatile uint32_t diag_spi_ctrl1;      /* SPI3 CTRL1 after init */
    volatile uint32_t diag_spi_sts;        /* SPI3 STS after init */

} fpga_state_t;

/* Global FPGA state (defined in fpga.c) */
extern fpga_state_t fpga;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Initialize FPGA communication hardware:
 *   - USART2 at 9600 baud (PA2=TX, PA3=RX)
 *   - SPI3 Mode 3 (PB3=SCK, PB4=MISO, PB5=MOSI, PB6=CS)
 *   - Control pins (PC6=SPI enable, PB11=active mode)
 *   - AFIO remap to free PB3/4/5 from JTAG
 *   - Send boot command sequence to FPGA
 *   - Perform SPI3 handshake
 *
 * Must be called after clock init and before FreeRTOS scheduler start.
 * Requires CRM_GPIOA/B/C and CRM_IOMUX clocks already enabled.
 */
void fpga_init(void);

/*
 * Create FreeRTOS tasks for FPGA communication:
 *   - USART TX task (dvom_TX): formats and sends command frames
 *   - USART RX task (dvom_RX): processes received data frames
 *   - Acquisition task (fpga): SPI3 ADC data reads
 *
 * Returns the acquisition task queue handle for triggering reads.
 * Must be called after fpga_init() and before vTaskStartScheduler().
 */
QueueHandle_t fpga_create_tasks(void);

/*
 * Send a command to the FPGA via USART2 TX queue.
 * cmd_high: command high byte (usually 0x00 for single-byte commands)
 * cmd_low:  command low byte (the actual command code)
 *
 * Non-blocking: returns pdTRUE on success, pdFALSE if queue full.
 */
BaseType_t fpga_send_cmd(uint8_t cmd_high, uint8_t cmd_low);

/*
 * Trigger an SPI3 acquisition cycle.
 * mode: acquisition mode byte (1-9, maps to fpga_acq_mode_t + 1)
 *
 * Call this from the input/housekeeping context to initiate a read.
 * For double-buffered operation, call twice back-to-back.
 */
BaseType_t fpga_trigger_acquisition(uint8_t mode);

/*
 * Check if valid ADC data is available.
 * Returns true after at least one successful SPI3 acquisition.
 */
bool fpga_data_ready(void);

/*
 * Get pointers to the ADC sample buffers.
 * Returns NULL if FPGA not initialized.
 */
const volatile uint8_t *fpga_get_ch1_buf(void);
const volatile uint8_t *fpga_get_ch2_buf(void);

/*
 * Set FPGA active mode (PB11).
 * Must be HIGH during oscilloscope/meter operation.
 */
void fpga_set_active(bool active);

/*
 * Enter oscilloscope mode: send FPGA scope configuration commands
 * (0x00, 0x01, 0x0B-0x11) and fire initial SPI3 acquisition triggers.
 *
 * Call at boot (device starts in scope mode) and when switching
 * from meter/siggen back to oscilloscope.
 */
void fpga_enter_scope_mode(void);

/*
 * Enter signal generator mode: send FPGA siggen configuration commands
 * (0x02-0x06, 0x08) and switch analog MUX to route DAC output to BNC.
 *
 * Call when switching to signal generator mode.
 */
void fpga_enter_siggen_mode(void);

/*
 * Configure FPGA for a specific meter submode.
 * Sends the appropriate FPGA init command sequence:
 *   - DCV/ACV (0,1): system_mode 1 → 0x00, 0x09, probe, 0x1A-0x1E
 *   - Resistance (6): system_mode 9 → 0x00, 0x12-0x14, 0x09, probe
 *   - Continuity (7): system_mode 8 → 0x00, 0x2C
 *   - Diode (8): system_mode 8 → 0x00, 0x2C
 *   - Frequency (5): system_mode 4 → 0x00, 0x1F, 0x09, 0x20, 0x21
 *
 * Call when the meter submode changes (LEFT/RIGHT buttons).
 */
void fpga_set_meter_mode(uint8_t submode);

#endif /* FPGA_H */
