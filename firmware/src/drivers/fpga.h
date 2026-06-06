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
#include "fpga_meter_plan.h"
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
#define FPGA_TX_FRAME_HISTORY 16
#define FPGA_RX_FRAME_SIZE    12
#define FPGA_RX_FRAME_HISTORY 8
#define FPGA_METER_TRANSITION_HISTORY 4
#define FPGA_POST_H2_TRIGGER_HISTORY 5
#define FPGA_POST_H2_RX_HISTORY 8

/* RX frame headers */
#define FPGA_RX_DATA_HDR_0    0x5A   /* Data frame: 0x5A 0xA5 */
#define FPGA_RX_DATA_HDR_1    0xA5
#define FPGA_RX_ECHO_HDR_0    0xAA   /* Echo frame: 0xAA 0x55 */
#define FPGA_RX_ECHO_HDR_1    0x55
#define FPGA_RX_ECHO_FRAME_SIZE 10

/* ═══════════════════════════════════════════════════════════════════
 * FPGA Command Codes (USART TX)
 * ═══════════════════════════════════════════════════════════════════ */

/* Bytes 1/2/6/7/8 are stock post-H2 SPI3 trigger bytes, not USART commands. */

/* Runtime commands */
#define FPGA_CMD_RESET        0x00
#define FPGA_CMD_SCOPE_CH     0x01   /* Scope channel config */
#define FPGA_CMD_METER_START  0x09   /* Start meter measurement */
#define FPGA_CMD_METER_NOPROBE 0x0A  /* Meter PC7-low command tail */

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

/* Scope channel command family 0x1A..0x1E. Stock scope xrefs use these as
 * channel gain/offset/coupling commands. The DMM boot dispatcher also queues
 * the same byte values through 0x20002D6C, but that is a byte-dispatch surface,
 * not the raw 0x20002D74 DVOM wire queue. That proves command sequencing only;
 * it is not a recovered DMM range-selector, low-DCV calibration source, or
 * runtime ms[0x02]/ms[0x03] mux writer. */
#define FPGA_CMD_CH1_GAIN     0x1A   /* CH1 gain / stock command-bank byte */
#define FPGA_CMD_CH1_OFFSET   0x1B   /* CH1 offset / stock command-bank byte */
#define FPGA_CMD_CH2_GAIN     0x1C   /* CH2 gain / stock command-bank byte */
#define FPGA_CMD_CH2_OFFSET   0x1D   /* CH2 offset / stock command-bank byte */
#define FPGA_CMD_COUPLING     0x1E   /* Coupling/BW / stock command-bank byte */

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

/*
 * Diagnostic-only DMM waveform sampler trigger.
 *
 * Stock V1.2.0 uses public trigger byte 6 for SPI3 case 5
 * (`trigger_byte - 1 == FPGA_ACQ_METER_ADC`) during the post-H2 queue block at
 * 0x08026DCE..0x08026E2A. Keep the debug sampler on a private byte so it cannot
 * steal stock trigger semantics.
 */
#define FPGA_ACQ_DIAG_METER_ADC_TRIGGER 0xF0u

/* ═══════════════════════════════════════════════════════════════════
 * Stock-State Bench Shadow
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t visible_state;  /* Stock DAT_20001060 / 0xF68 */
    uint8_t phase;          /* Stock DAT_20001061 / 0xF69 */
    uint8_t substate;       /* Stock DAT_20001062 / 0xF6A */
    uint8_t flags;          /* Stock DAT_20001063 / 0xF6B */
    uint8_t e1a;            /* Staged-detail latch */
    uint8_t e1b;            /* Panel count / non-empty gate */
    uint8_t e1c;            /* Panel handoff byte */
    uint8_t e1d;            /* Panel selection index */
    uint8_t latch_355;      /* Preset-consume latch */
    uint8_t detail_bits[8]; /* Bench shadow for stock +0xE12..+0xE19 bitmap */
} fpga_stock_shadow_t;

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
    volatile bool    bus_released;     /* SPI3 bus handed to external SSPI
                                        * master (ESP32) via fpga_bus_release();
                                        * acq task stays off the bus while set */
    volatile uint16_t frame_count;     /* Data frame counter (0x5A 0xA5) */
    volatile uint16_t echo_count;     /* Echo frame counter (0xAA 0x55) */
    volatile uint16_t tx_count;       /* TX commands sent */
    volatile uint16_t rx_byte_count;  /* Raw RX bytes received */
    volatile uint16_t rx_sync_data_start_count; /* Saw 0x5A as RX frame byte 0 */
    volatile uint16_t rx_sync_echo_start_count; /* Saw 0xAA as RX frame byte 0 */
    volatile uint16_t rx_sync_data_header_count; /* Completed 0x5A 0xA5 header */
    volatile uint16_t rx_sync_echo_header_count; /* Completed 0xAA 0x55 header */
    volatile uint16_t rx_sync_bad_second_count; /* Header byte 1 rejected */
    volatile uint16_t rx_sync_stray_count;      /* Byte ignored while unsynced */
    volatile uint16_t last_rx_frame_count; /* Data-frame count when latest frame arrived */
    volatile uint16_t last_rx_tx_count;    /* TX count visible at latest data-frame arrival */
    volatile uint16_t last_rx_echo_count;  /* Echo count visible at latest data-frame arrival */
    volatile uint16_t last_rx_mode_sequence_count; /* DMM selector sequence visible at RX arrival */
    volatile uint8_t  last_rx_mode_sequence_submode; /* DMM sequence submode visible at RX arrival */
    volatile uint8_t  last_rx_discard_remaining; /* Transition discard budget at RX arrival */
    volatile uint8_t  last_rx_transition_busy;   /* Meter transition gate state at RX arrival */
    volatile uint8_t  rx_frame_history[FPGA_RX_FRAME_HISTORY][FPGA_RX_FRAME_SIZE];
    volatile uint16_t rx_history_frame_count[FPGA_RX_FRAME_HISTORY];
    volatile uint16_t rx_history_tx_count[FPGA_RX_FRAME_HISTORY];
    volatile uint16_t rx_history_echo_count[FPGA_RX_FRAME_HISTORY];
    volatile uint16_t rx_history_sequence_count[FPGA_RX_FRAME_HISTORY];
    volatile uint8_t  rx_history_sequence_submode[FPGA_RX_FRAME_HISTORY];
    volatile uint8_t  rx_history_discard_remaining[FPGA_RX_FRAME_HISTORY];
    volatile uint8_t  rx_history_transition_busy[FPGA_RX_FRAME_HISTORY];
    volatile uint8_t  rx_frame_history_head;
    volatile uint8_t  rx_frame_history_count;
    volatile uint8_t  last_rx_echo_frame[FPGA_RX_ECHO_FRAME_SIZE];
    volatile uint8_t  tx_cmd_hi_history[16]; /* Last sent USART command high bytes */
    volatile uint8_t  tx_cmd_lo_history[16]; /* Last sent USART command low bytes */
    volatile uint8_t  tx_cmd_history_head;   /* Next history slot */
    volatile uint8_t  tx_cmd_history_count;  /* Valid history entries */
    volatile uint8_t  last_tx_frame[FPGA_TX_FRAME_SIZE]; /* Last full 10-byte USART frame sent */
    volatile uint8_t  tx_frame_history[FPGA_TX_FRAME_HISTORY][FPGA_TX_FRAME_SIZE];
    volatile uint16_t tx_frame_history_tx_count[FPGA_TX_FRAME_HISTORY];
    volatile uint8_t  tx_frame_history_head;
    volatile uint8_t  tx_frame_history_count;

    /* DMM mode-sequence diagnostics. Polling quickly overwrites tx_recent
     * with 0x0509, so keep the last explicit mode switch separately. */
    volatile uint16_t meter_mode_sequence_count;
    volatile uint8_t  meter_mode_sequence_submode;
    volatile uint16_t meter_mode_selector_word;
    volatile uint16_t meter_mode_apply_word;
    volatile uint16_t meter_mode_probe_word;
    volatile uint16_t meter_mode_start_word;
    volatile uint8_t  meter_transition_history_submode[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_selector[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_apply[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_probe[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_start[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_sequence_count[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_tx_before[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_tx_after[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_frame_before[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_frame_after[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_planned_gpio[FPGA_METER_TRANSITION_HISTORY];
    volatile uint16_t meter_transition_history_actual_gpio[FPGA_METER_TRANSITION_HISTORY];
    volatile uint8_t  meter_transition_history_head;
    volatile uint8_t  meter_transition_history_count;

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
    volatile uint8_t  init_hs[12];         /* Handshake response bytes (11 used + probe) */
    volatile uint32_t diag_remap5;         /* IOMUX remap5 (spi3_gmux) */
    volatile uint32_t diag_remap7;         /* IOMUX remap7 (swjtag_gmux) */
    volatile uint32_t diag_spi_ctrl1;      /* SPI3 CTRL1 after init */
    volatile uint32_t diag_spi_sts;        /* SPI3 STS after init */

    /* H2 SPI3 bitstream upload diagnostics. h2_bytes_sent/upload_done prove only
     * that local firmware streamed the stock 115638-byte table; the FPGA has no
     * recovered ACK/apply signal, so this is not proof the table was accepted,
     * applied, or a DMM calibration source. */
    volatile uint32_t h2_bytes_sent;       /* Bytes uploaded (should be 115638) */
    volatile uint8_t  h2_upload_done;      /* 1 = TX completed without error */
    volatile uint8_t  h2_close_status;     /* MISO byte after 0x3A close (stock: 0xF8) */
    volatile uint8_t  scope_status[4];     /* MISO from post-upload 0x03 read
                                            * (stock boot: 00 01 42 2E — issue-#18 capture) */
    volatile uint8_t  cfg_status_reg[4];   /* Gowin STATUS_REGISTER (opcode 0x41),
                                            * big-endian as clocked. The authoritative
                                            * config status. Bits (openFPGALoader
                                            * gowin.cpp): 0=CRC_ERROR 1=BAD_COMMAND
                                            * 2=ID_VERIFY_FAILED 13=DONE_FINAL 15=READY
                                            * 16=POR. All-0xFF = FPGA not driving MISO
                                            * (never entered config-receive). Separates
                                            * wire-problem from config-entry problem.
                                            * See sibling_loader_config_diff.md. */
    volatile uint8_t  edit_mode_status[4]; /* STATUS (0x41) read at /256 IMMEDIATELY
                                            * after 0x15 CONFIG_ENABLE (probe_edit knob).
                                            * SYSTEM_EDIT_MODE is bit 7 of the ASSEMBLED
                                            * word = bit 7 of [3], not of [0]. NOTE: as of
                                            * Exp I (2026-07-28) this value is known to be
                                            * a phase-slice of a free-running MISO pattern,
                                            * NOT a register read. Kept for continuity. */

    /* ── Exp J: ANCHORED opcode-discrimination probe (2026-07-28) ──────────
     * The first measurement in this project with a known correct answer.
     * Exp I showed every prior status read was a phase-slice of the free-running
     * 0xC8100001 pattern the FPGA emits from its running NV design; with no
     * ground truth, a stable wrong number was indistinguishable from a right one.
     *
     * Each probe clocks 8 bytes (not 4) so a repeating 4-byte pattern is visible
     * as data rather than inferred. All reads at /256 — the only valid SSPI read
     * clock (fpga.c:1564). Populated only when opts.probe_idcode = 1. */
    volatile uint8_t  probe_idcode[8];     /* 0x11 READ_IDCODE, before the prelude */
    volatile uint8_t  probe_noop[8];       /* 0x00 no-op — the control */
    volatile uint8_t  probe_status[8];     /* 0x41 STATUS */
    volatile uint8_t  probe_user[8];       /* 0x13 USERCODE */
    volatile uint8_t  probe_idcode_post[8];/* 0x11 again, AFTER 0x15 CONFIG_ENABLE */
    volatile int8_t   probe_id_bit;        /* bit offset 0..32 where 0x0120681B was
                                            * found in probe_idcode, else -1. Searched
                                            * across ALL bit offsets deliberately: every
                                            * artifact this project has hit (garbage at
                                            * /2, floating MISO, SWD byte-rotation) shows
                                            * up as a phase shift, so an exact-match test
                                            * would report absent when it is merely
                                            * misaligned. */
    volatile int8_t   probe_id_bit_post;   /* same search over probe_idcode_post */
    volatile uint8_t  probe_all_same;      /* 1 = 0x11 and 0x00 returned identical bytes
                                            * => the FPGA is not decoding opcodes */
    volatile uint8_t  probe_repeats;       /* 1 = first 4 bytes == last 4 bytes of the
                                            * 0x11 read => free-running fixed pattern */

    /* ── Post-0x3A anchor (Exp L follow-up, 2026-07-28) ────────────────────
     * IDCODE read at /256 immediately BEFORE cfg_status_reg, in the same window,
     * so the post-upload status is only believed on a validated read path.
     * Unlike the other probe_* fields this runs on EVERY build, because an
     * unanchored cfg_status_reg is exactly the kind of reading this project has
     * repeatedly mistaken for a measurement.
     *
     * It is also a direct configured/not-configured test. Exp L: once stock has
     * configured the FPGA successfully, the SSPI config port CLOSES and 0x11
     * returns zeros — the port belongs to the user design from then on. So:
     *   probe_id_bit_close >= 0  => config port still OPEN after our full
     *                               115,638-byte upload and 0x3A close, i.e. we
     *                               are definitively NOT configured.
     *   probe_id_bit_close == -1 => the port stopped answering. Consistent with
     *                               configuration having completed — but NOT
     *                               proof of it, since a dead bus reads the same.
     *                               Corroborate with DONE and a live trace. */
    volatile uint8_t  probe_idcode_close[8];
    volatile int8_t   probe_id_bit_close;

    /* ── Step-resolved config-status trace (2026-07-28) ────────────────────
     * The whole sequence measured one checkpoint at a time, each anchored on the
     * IDCODE so a reading is only kept if the read path proved itself first.
     * Captures sit at CS-frame BOUNDARIES only — never inside a frame, and never
     * between the 0x3B open and its data, which would break the upload frame.
     *
     *   0  pristine, before the 05 prelude
     *   1  after 05 00   (ERASE_SRAM)
     *   2  after 12 00   (INIT_ADDR)
     *   3  after 15 00   (CONFIG_ENABLE)
     *   4  after the full 115,638-byte upload, before 3A
     *   5  after 3A 00   (CONFIG_DISABLE / close)
     *
     * cfg_trace[i] is anchor-corrected. 0xFFFFFFFF means the anchor FAILED at
     * that checkpoint and the value is not to be believed — the same refusal the
     * SWD script makes, rather than storing a plausible-looking number.
     *
     * If all six are identical the part ignores every config command while still
     * answering reads, which localises the failure without further guessing. */
#define FPGA_CFG_TRACE_N 6
    volatile uint32_t cfg_trace[FPGA_CFG_TRACE_N];
    volatile int8_t   cfg_trace_anchor[FPGA_CFG_TRACE_N];

    /* ── RECONFIG_N candidate pin sweep (2026-07-28) ───────────────────────
     * Exp N showed no config command moves the STATUS register, which matches a
     * running GW1N refusing configuration until RECONFIG_N is pulsed. This
     * searches for that pin: for each candidate, pulse LOW->HIGH, send
     * CONFIG_ENABLE, and read the anchored STATUS. Anything other than the
     * baseline is a hit.
     *
     * Button-gated and run from the UI task, NOT from fpga_init: a bad pulse
     * during init would fail the boot, and three failed boots latch the
     * bootloader into SAFE MODE (seen twice on this unit already). Run from the
     * UI it is recoverable with a power-cycle, and repeatable without a reflash. */
    volatile uint8_t  sweep_state;      /* 0=idle 1=running 2=done */
    volatile uint8_t  sweep_tested;     /* candidates completed */
    volatile uint8_t  sweep_total;      /* candidates in the table */
    volatile uint8_t  sweep_hits;       /* count whose STATUS left the baseline */
    volatile uint8_t  sweep_anchor_fail;/* candidates whose IDCODE anchor failed —
                                         * NOT hits: an unvalidated read is discarded,
                                         * though a pin that CLOSES the config port
                                         * would also land here and is worth a look */
    volatile uint8_t  sweep_first_hit;  /* index into the candidate table, 0xFF = none */
    volatile uint32_t sweep_baseline;   /* anchored STATUS before the sweep */
    volatile uint32_t sweep_hit_status; /* anchored STATUS at the first hit */
    volatile uint8_t  sweep_hit_phase;  /* 0 = none, 1 = during the post-pulse
                                         * sampling window (the pulse ALONE did
                                         * something — a reconfiguration), 2 =
                                         * during the post-CONFIG_ENABLE window
                                         * (the pulse made 0x15 land). Exp O could
                                         * not distinguish these: it took a single
                                         * snapshot at a fixed +1ms and would have
                                         * missed a transient entirely. */

    /* Stock post-H2 SPI3 queue diagnostics. These counters prove only local
     * enqueue/execution of the stock trigger bytes; they are not FPGA ACK or
     * DMM calibration acceptance evidence. */
    volatile uint8_t  post_h2_spi3_boot_enqueued;
    volatile uint8_t  post_h2_spi3_boot_ok;
    volatile uint8_t  post_h2_spi3_boot_dropped;
    volatile uint8_t  post_h2_spi3_boot_mask;
    volatile uint8_t  post_h2_spi3_trigger[FPGA_POST_H2_TRIGGER_HISTORY];
    volatile uint8_t  post_h2_spi3_rx_len[FPGA_POST_H2_TRIGGER_HISTORY];
    volatile uint8_t  post_h2_spi3_rx[FPGA_POST_H2_TRIGGER_HISTORY][FPGA_POST_H2_RX_HISTORY];

    /* Experimental stock runtime shadow for scope-mode bench work.
     * These are NOT the original firmware RAM locations. They are a small
     * explicit mirror we can inspect and stage from the shell while testing
     * stock-like right-panel and packed-state flows. */
    volatile fpga_stock_shadow_t stock_shadow;

} fpga_state_t;

/* Global FPGA state (defined in fpga.c) */
extern fpga_state_t fpga;

/* Tunable knobs for the SPI3 config handshake (prelude + bitstream upload +
 * close + scope-config). Exposed so the debug shell can sweep them live via
 * `fpga reinit` without reflashing — chasing why our upload activates the
 * FPGA slave but never reaches stock's configured state (0x3A close F8,
 * 0x03 status 00 01 42 2E). See SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md. */
typedef struct {
    uint32_t upload_br;       /* SPI3 baud divider for the 0x3B bulk phase (0=/2) */
    uint32_t prelude_gap_ms;  /* gap after each 05/12/15 prelude frame (stock ~100) */
    uint32_t post_close_ms;   /* delay after 0x3A close before scope-config (stock ~600) */
    uint8_t  arm_pb11;        /* 1 = drive PB11 HIGH before the handshake */
    /* Optional FPGA reset pulse BEFORE the handshake — the step rosenrot00's
     * working 2C23T loader does (RESET LOW 10ms → HIGH 1ms) that our 2C53T
     * sequence lacks. The 2C53T reset line is unknown (PC8 is our POWER btn),
     * so this is sweepable: reset_port 1..5 = A..E, 0 = no pulse. */
    uint8_t  reset_port;      /* 0=none, 1=A,2=B,3=C,4=D,5=E */
    uint8_t  reset_pin;       /* 0..15 */
    uint16_t reset_low_ms;    /* LOW duration (rosenrot uses 10) */
    /* Prelude framing sweep — how the 05/12/15 CONFIG_ENABLE prelude is split
     * across chip-select frames relative to the 0x3B upload. Our baseline
     * (mode 0) is wire-faithful to the stock capture; modes 1/2 are deviations
     * to test whether the FPGA needs a different framing to enter SSPI receive
     * mode. See SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md / issue #18. */
    uint8_t  prelude_frame_mode; /* 0=split (stock: 05|12|15|3B), 1=combined
                                  *   (05 12 15 in one CS frame, then 3B),
                                  *   2=merge (05|12|15+3B share one CS frame) */
    uint32_t pre_upload_gap_ms;  /* delay between the prelude and the 0x3B open
                                  *   (stock ~8us → 0; lets the FPGA digest
                                  *   CONFIG_ENABLE before data starts) */
    uint32_t cmd_br;             /* SPI3 baud divider for the COMMAND phase —
                                  *   prelude (05/12/15), 0x3A close, 0x03 status
                                  *   reads (0=/2 default). The SSPI read path is
                                  *   clock-limited: IDCODE reads as garbage at /2
                                  *   but clean at /256, so the close/status bytes
                                  *   we sample at /2 may be unreliable and the
                                  *   CONFIG_ENABLE prelude may not land. Sweep
                                  *   this to send + read the handshake slowly. */
    /* Strap-hold sweep (2026-06-13 whole-binary GPIO audit). Port-D pins that
     * stock drives on scope-mode entry but our firmware never touches — held
     * (NOT pulsed) through the ENTIRE handshake, matching stock. PD2 is the
     * prime suspect (asserted beside fpga_queue sends in the decompile; the old
     * RECONFIG pulse-test doesn't clear a HELD level). PD12/PD13 are driven in
     * the meter_state channel loop. See unmapped_mcu_fpga_pin_candidates.md §4a. */
    uint8_t  strap_pd2;       /* PD2:       0=untouched, 1=hold HIGH, 2=hold LOW */
    uint8_t  strap_pd1213;    /* PD12+PD13: 0=untouched, 1=hold HIGH, 2=hold LOW */
    /* Trailing clocks after the last bitstream byte, before 0x3A close. Gowin
     * runs the CRC-check / DONE / wakeup on CCLK cycles AFTER the final config
     * byte; our sequence sent ZERO. rosenrot00's working 2C23T SPI loader clocks
     * ~200 dummy 0x00 here (fpga.c:342-344). Sweepable: 0 (stock-faithful per the
     * capture) / 64 / 200 / 512. See sibling_loader_config_diff.md. */
    uint16_t trailing_clocks;
    /* DIAGNOSTIC knobs (default 0 = sequence byte-unchanged). */
    uint8_t  probe_edit;      /* 1 = after 0x15 CONFIG_ENABLE, read STATUS(0x41) at
                               *     /256 into fpga.edit_mode_status[] — does bit7
                               *     SYSTEM_EDIT_MODE engage? (the precise wall test) */
    uint8_t  reload_3c;       /* 1 = send Gowin RELOAD (0x3C) at /256 before the
                               *     prelude — software reconfig trigger; does it
                               *     clear GWVLD/FLASH_LOCK and let CONFIG_ENABLE land? */
    uint8_t  cfg_trace;       /* 1 = capture the anchored step-resolved status trace
                               *     at all six CS-frame boundaries (see cfg_trace[]).
                               *     Adds read frames between the prelude steps, so
                               *     keep it off for stock-faithful runs. Requires
                               *     prelude_frame_mode 0 (split) for steps 1-3. */
    uint8_t  probe_idcode;    /* 1 = Exp J anchored opcode-discrimination probe: read
                               *     0x11/0x00/0x41/0x13 at /256 (8 bytes each) BEFORE
                               *     the prelude, and 0x11 again after CONFIG_ENABLE.
                               *     Known answer: IDCODE = 0x0120681B. Adds CS frames
                               *     ahead of the config attempt, so keep it off for
                               *     stock-faithful runs. */
    uint8_t  prelude_reads;   /* Build A (2026-08-13): 1 = insert the maksidze/Stlkv
                               *     V0.4 "richer prelude" reads 0x11 IDCODE / 0x13
                               *     USERCODE / 0x41 STATUS at /256 in their own CS
                               *     frames BETWEEN 0x05 ERASE_SRAM and 0x12 INIT_ADDR
                               *     — i.e. immediately before CONFIG_ENABLE, the
                               *     position Stlkv's working cold-start loader uses.
                               *     Stores into probe_idcode/_user/_status and sets
                               *     probe_id_bit (the anchor). Requires
                               *     prelude_frame_mode 0 (split). See issue #18. */
} fpga_cfg_seq_opts_t;

/* Run the full SPI3 config handshake. Returns the 0x3A close status byte
 * (stock: 0xF8). Fills fpga.init_hs[], fpga.h2_close_status, fpga.scope_status[]. */
uint8_t fpga_spi3_config_sequence(const fpga_cfg_seq_opts_t *opt);

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
 * Trigger a scope acquisition using the current scope-mode policy.
 * For bulk/dual captures this may queue more than one low-level SPI3 mode.
 */
BaseType_t fpga_trigger_scope_read(void);

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

extern volatile bool     fpga_meter_adc_sampler_enabled;
extern volatile bool     fpga_meter_adc_use_preacq;
extern volatile int16_t  fpga_meter_adc_selector_override;
extern volatile int16_t  fpga_meter_adc_preacq_override;
extern volatile uint32_t fpga_meter_adc_enqueue_attempts;
extern volatile uint32_t fpga_meter_adc_enqueue_success;
extern volatile uint32_t fpga_meter_adc_enqueue_drops;
extern volatile uint32_t fpga_meter_adc_samples;
extern volatile uint32_t fpga_meter_adc_ff_samples;
extern volatile uint32_t fpga_meter_adc_zero_samples;
extern volatile uint32_t fpga_meter_adc_transition_skips;
extern volatile uint32_t fpga_meter_adc_not_voltage_skips;
extern volatile uint32_t fpga_meter_adc_reset_generation;
extern volatile uint32_t fpga_meter_adc_last_reset_generation;
extern volatile uint8_t  fpga_meter_adc_last_preacq;
extern volatile uint8_t  fpga_meter_adc_last_preacq_rx;
extern volatile uint8_t  fpga_meter_adc_last_selector;
extern volatile uint8_t  fpga_meter_adc_last_sample;
extern volatile uint8_t  fpga_meter_adc_first_sample_after_reset;
extern volatile uint8_t  fpga_meter_adc_min_sample;
extern volatile uint8_t  fpga_meter_adc_max_sample;
extern volatile uint8_t  meter_frame_discard_count;
extern volatile uint32_t meter_transition_frame_skip_count;

void fpga_meter_adc_diag_reset(void);
bool fpga_meter_transition_busy(void);

/*
 * Set FPGA active mode (PB11).
 * Must be HIGH during oscilloscope/meter operation.
 */
void fpga_set_active(bool active);

/*
 * Release the SPI3 bus to an external SSPI master (ESP32) soldered to the
 * back-side SPI3 test pads. Tri-states PB3/PB5/PB6, keeps PC6/PB11 staged,
 * leaves PC9 power hold untouched. EXPERIMENTAL — see experimental/
 * esp32-bringup branch (tools/esp32_sspi_bringup/). Re-flash to undo.
 */
void fpga_bus_release(void);

/* RECONFIG_N candidate pin sweep. Pulses each candidate LOW->HIGH, sends
 * CONFIG_ENABLE, and reads the anchored STATUS, looking for any pin that makes
 * the part respond. Restores every pin's original config as it goes.
 * Blocking, ~400ms. Call from a task, never from an ISR or from fpga_init.
 * Results land in fpga.sweep_*. Built only under FPGA_PIN_SWEEP_BUILD. */
void fpga_reconfig_pin_sweep(void);
const char *fpga_sweep_pin_name(uint8_t idx);

/*
 * Enter oscilloscope mode: send FPGA scope configuration commands
 * (0x00, 0x01, 0x0B-0x11) and fire initial SPI3 acquisition triggers.
 *
 * Call at boot (device starts in scope mode) and when switching
 * from meter/siggen back to oscilloscope.
 */
void fpga_enter_scope_mode(void);

/*
 * Re-apply the current oscilloscope configuration to the FPGA and
 * analog frontend. Safe to call from the USB debug shell while the
 * device is already in scope mode.
 */
void fpga_scope_reinit(void);

/*
 * Queue a scope reinit to be serviced asynchronously from the display loop.
 * This is safe to invoke from the USB debug shell.
 */
void fpga_request_scope_reinit(void);

/*
 * Service deferred FPGA requests. Returns true if work was performed.
 * Call periodically from a non-USB task.
 */
bool fpga_service_requests(void);

/*
 * Enter signal generator mode: send FPGA siggen configuration commands
 * (0x02-0x06, 0x08) and switch analog MUX to route DAC output to BNC.
 *
 * Call when switching to signal generator mode.
 */
void fpga_enter_siggen_mode(void);

/*
 * Configure FPGA for a specific meter submode.
 * Sends the stock DMM raw UART word selected from the recovered
 * 0x080BB3FC meter-mode table, then re-arms measurement polling.
 *
 * Call when the meter submode changes (LEFT/RIGHT buttons).
 */
void fpga_set_meter_mode(uint8_t submode);

/*
 * Return the stock DMM raw-command selector that fpga_set_meter_mode() applies.
 * This is read-only debug metadata for USB harnesses and tests; it does not
 * touch GPIO or send FPGA commands.
 */
fpga_meter_selector_t fpga_meter_expected_selectors(uint8_t submode);

/*
 * Re-apply the known-good meter frontend baseline and meter command
 * sequence for the requested submode. Intended for live bench recovery
 * from the USB debug shell after scope experiments.
 */
void fpga_meter_reinit(uint8_t submode);

/*
 * Send the stock-like meter wake preamble, then re-apply the current
 * scope configuration. Intended for testing whether the FPGA needs a
 * meter-side wakeup before it will accept scope commands.
 */
void fpga_scope_wake(void);

/*
 * Recompute the current scope acquisition mode block (0x20/0x21) from
 * live UI state and send it to the FPGA. Mirrors stock internal cmd 2.
 */
void fpga_scope_refresh_acq_mode(void);

/*
 * Recompute the current scope timebase block (0x26/0x27/0x28), update the
 * local acquisition policy, and queue the next SPI read. Mirrors stock
 * internal cmd 3 as closely as our current architecture allows.
 */
void fpga_scope_heartbeat(void);

/*
 * Send a raw 10-byte USART2 frame to the FPGA (bypasses queue).
 * Used by the USB debug shell for interactive protocol exploration.
 * frame must point to exactly 10 bytes.
 */
void fpga_send_raw_frame(const uint8_t *frame);

/*
 * Reset the stock-state bench shadow to a conservative base-scope posture.
 */
void fpga_stock_diag_reset(void);

/*
 * Override the entire stock-state bench shadow.
 */
void fpga_stock_diag_set(uint8_t visible_state,
                         uint8_t phase,
                         uint8_t substate,
                         uint8_t flags,
                         uint8_t e1a,
                         uint8_t e1b,
                         uint8_t e1c,
                         uint8_t e1d,
                         uint8_t latch_355);

/*
 * Seed common stock-like scope postures for bench experiments.
 */
void fpga_stock_diag_seed_base2(void);
void fpga_stock_diag_seed_state5(uint8_t e1b, uint8_t e1d);
void fpga_stock_diag_seed_state6(uint8_t e1b, uint8_t e1d);
void fpga_stock_diag_seed_preset(uint8_t visible_state,
                                 uint8_t phase,
                                 uint8_t substate,
                                 uint8_t flags,
                                 uint8_t latch_355);

/*
 * Drive the recovered right-panel / packed-preset families against the current
 * stock shadow. These are best-effort bench stand-ins for the stock event
 * owners, not claims of exact wire-level equivalence.
 */
void fpga_stock_diag_prev(void);
void fpga_stock_diag_next(void);
void fpga_stock_diag_select(void);
void fpga_stock_diag_toggle(void);
void fpga_stock_diag_commit(void);
void fpga_stock_diag_consume(void);
void fpga_stock_diag_bridge_fixed(void);
void fpga_stock_diag_bridge_dynamic(uint8_t bank_mode);

/*
 * Bench helpers for candidate final 16-bit wire words recovered from the
 * stock TX-word queue path.
 *
 * bank_mode:
 *   0 = CH1 candidate bank
 *   1 = CH2 candidate bank
 *   2 = CH1 + CH2 candidate banks
 */
void fpga_wire_send_word(uint16_t word, uint32_t delay_ms);
void fpga_wire_entry(uint8_t bank_mode);
void fpga_wire_scope_sequence(uint8_t bank_mode);

/*
 * Re-enter the current clean-room scope bring-up path while preserving the
 * staged stock shadow. This is a conservative stand-in for the stock shared
 * bank-emitter re-entry used during scope-mode experiments.
 */
void fpga_stock_diag_reenter(void);

#endif /* FPGA_H */
