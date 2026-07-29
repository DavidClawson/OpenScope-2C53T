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
#include "../ui/scope_state.h"
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
static volatile bool scope_reinit_pending = false;

/* ═══════════════════════════════════════════════════════════════════
 * Stock-State Bench Shadow
 * ═══════════════════════════════════════════════════════════════════ */

static void fpga_stock_shadow_write(uint8_t visible_state,
                                    uint8_t phase,
                                    uint8_t substate,
                                    uint8_t flags,
                                    uint8_t e1a,
                                    uint8_t e1b,
                                    uint8_t e1c,
                                    uint8_t e1d,
                                    uint8_t latch_355)
{
    fpga.stock_shadow.visible_state = visible_state;
    fpga.stock_shadow.phase = phase;
    fpga.stock_shadow.substate = substate;
    fpga.stock_shadow.flags = flags;
    fpga.stock_shadow.e1a = e1a;
    fpga.stock_shadow.e1b = e1b;
    fpga.stock_shadow.e1c = e1c;
    fpga.stock_shadow.e1d = e1d;
    fpga.stock_shadow.latch_355 = latch_355;
    memset((void *)fpga.stock_shadow.detail_bits, 0, sizeof(fpga.stock_shadow.detail_bits));
}

void fpga_stock_diag_set(uint8_t visible_state,
                         uint8_t phase,
                         uint8_t substate,
                         uint8_t flags,
                         uint8_t e1a,
                         uint8_t e1b,
                         uint8_t e1c,
                         uint8_t e1d,
                         uint8_t latch_355)
{
    fpga_stock_shadow_write(visible_state, phase, substate, flags,
                            e1a, e1b, e1c, e1d, latch_355);
}

void fpga_stock_diag_seed_base2(void)
{
    /* Conservative base-scope posture from the recovered compact owner:
     * visible state 2, no packed preset active, no staged right-panel handoff.
     * We intentionally leave no implied right-panel selection armed. */
    fpga_stock_shadow_write(2, 0, 0, 0, 0, 0, 0, 0, 0);
}

void fpga_stock_diag_reset(void)
{
    fpga_stock_diag_seed_base2();
}

void fpga_stock_diag_seed_state5(uint8_t e1b, uint8_t e1d)
{
    /* Stable right-panel editor posture. Use a small nonzero default entry
     * count in shell helpers so state-6 gating experiments have something to
     * work with, but keep the packed preset bytes inactive. */
    fpga_stock_shadow_write(5, 0, 0, 0, 0, e1b, 0, e1d, 0);
}

void fpga_stock_diag_seed_state6(uint8_t e1b, uint8_t e1d)
{
    /* Transient state above the right-panel editor. Stock only appears to
     * reach this when E1B is nonzero, so callers should seed it accordingly. */
    fpga_stock_shadow_write(6, 0, 0, 0, 0, e1b, 0, e1d, 0);
}

void fpga_stock_diag_seed_preset(uint8_t visible_state,
                                 uint8_t phase,
                                 uint8_t substate,
                                 uint8_t flags,
                                 uint8_t latch_355)
{
    fpga.stock_shadow.visible_state = visible_state;
    fpga.stock_shadow.phase = phase;
    fpga.stock_shadow.substate = substate;
    fpga.stock_shadow.flags = flags;
    fpga.stock_shadow.latch_355 = latch_355;
    memset((void *)fpga.stock_shadow.detail_bits, 0, sizeof(fpga.stock_shadow.detail_bits));
}

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

/* Double-buffered SPI3 pump (per GitHub issue #11, Lanchon).
 *
 * spi3_xfer()'s order — wait TDBE, write, wait RDBF, read — only queues
 * the NEXT tx byte after the current byte's RX has landed, by which point
 * the shift register has already drained. That underruns the transmitter
 * and clock-stretches the SPI bus between every byte; an interrupt landing
 * in the RDBF busy-wait widens the gap arbitrarily. Over the 115KB H2
 * upload that is tens of thousands of stalls.
 *
 * This pump primes the tx buffer one byte ahead: the moment TDBE frees it
 * writes tx[i], THEN blocks on RDBF for tx[i-1]. The shift register is
 * reloaded back-to-back so the clock runs continuously, and the pump
 * tolerates any interrupt shorter than one byte-time.
 *
 *   tx: bytes to send, or NULL to clock out 0xFF filler (read-only)
 *   rx: receive buffer, or NULL to discard (write-only)
 *   n:  byte count. Caller manages CS.
 *
 * Timeout-guarded like spi3_xfer so a misconfigured peripheral can't hang
 * the boot; on timeout the byte is treated as 0xFF and we keep going.
 */
static void spi3_pump(const uint8_t *tx, volatile uint8_t *rx, uint32_t n)
{
    if (n == 0)
        return;

    volatile uint32_t timeout;
    uint32_t i = 0;

    /* Prime the first byte. */
    timeout = 100000;
    while (!(FPGA_SPI->sts & SPI_I2S_TDBE_FLAG)) {
        if (--timeout == 0) break;
    }
    FPGA_SPI->dt = tx ? tx[0] : 0xFF;

    while (++i < n) {
        /* Queue the next tx byte the instant the buffer frees — BEFORE
         * blocking on RX. This is what keeps the shift register fed. */
        timeout = 100000;
        while (!(FPGA_SPI->sts & SPI_I2S_TDBE_FLAG)) {
            if (--timeout == 0) break;
        }
        FPGA_SPI->dt = tx ? tx[i] : 0xFF;

        /* Collect the previous byte's RX. */
        timeout = 100000;
        while (!(FPGA_SPI->sts & SPI_I2S_RDBF_FLAG)) {
            if (--timeout == 0) break;
        }
        uint8_t r = (timeout == 0) ? 0xFF : (uint8_t)FPGA_SPI->dt;
        if (rx)
            rx[i - 1] = r;
    }

    /* Drain the final byte's RX. */
    timeout = 100000;
    while (!(FPGA_SPI->sts & SPI_I2S_RDBF_FLAG)) {
        if (--timeout == 0) break;
    }
    uint8_t rlast = (timeout == 0) ? 0xFF : (uint8_t)FPGA_SPI->dt;
    if (rx)
        rx[n - 1] = rlast;
}

/* Set the SPI3 baud-rate divider (CTRL1 bits [5:3]) on the fly. Requires
 * toggling SPE off/on. br: 0=/2 (60MHz), 1=/4, 2=/8, 3=/16, 4=/32 ...
 *
 * Used to slow ONLY the 115KB 0x3B bitstream upload. Gowin SPI configuration
 * is clock-rate agnostic (the config logic samples on clock edges, not at a
 * fixed rate), so a slower upload is safe; the question under test is whether
 * our gapless 60MHz pump is marginal over 115K bytes (boot-to-boot 0x3A close
 * status varied F8/FC/00 — see SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md). */
/* TESTED 2026-06-12 (Unit 2): /16 produced IDENTICAL behavior to /2 —
 * close status still 00, buffers still empty. The gapless 60MHz pump is NOT
 * the marginal link. Reverted to /2 (stock-faithful); helper kept for future
 * sweeps. The config-completion gap is elsewhere (prelude/config-enter or a
 * runtime command we haven't replayed). */
#define SPI3_UPLOAD_BR  0u   /* /2 = 60MHz, matches stock */

/* ─── WARM-HANDOFF EXPERIMENT (2026-06-13) ──────────────────────────────
 * Set to 1 to build a "read-only" firmware for the stock→ours warm-handoff
 * test. The premise: stock firmware successfully configures the FPGA's scope
 * design at boot; the GW1N holds that SRAM config as long as power is
 * maintained (an MCU soft-reset does NOT power-cycle it, and RECONFIG_N stays
 * high). So if we boot stock (FPGA→scope-configured), then reflash to THIS
 * build via the stock IAP path WITHOUT cutting power, our firmware comes up in
 * front of an already-configured scope FPGA — letting us test our SPI3 read
 * path in isolation, before the FT232H/JTAG hardware arrives.
 *
 * When 1, fpga_init() sets up SPI3 + the two control pins (PC6 enable, PB11
 * active) to match stock's scope-run posture, but SKIPS everything that could
 * disturb the running config: the boot USART commands, the SSPI config
 * sequence (05/12/15/3B/3A), the meter-frontend relay routing, the meter USART
 * commands, and all auto-tasks. Read with the `spi3 acqread` shell command
 * (real 0x04/0x05 per-channel protocol). span>0 on CH1/CH2 = we read a live
 * waveform from a stock-configured FPGA = our downstream path works.
 *
 * Bench procedure: docs/fpga_warm_handoff_test.md. Revert to 0 for normal builds. */
#ifndef FPGA_WARM_HANDOFF_TEST
#define FPGA_WARM_HANDOFF_TEST  0
#endif

/* USART-silent scope-boot experiment (2026-06-13, stock-bringup-diff finding).
 * Stock holds USART2 UEN CLEAR through the entire scope boot and never runs the
 * dvom/meter tasks before the SPI3 config; our firmware enables USART2 + runs
 * dvom_TX/dvom_RX/meter_poll, which is the leading suspect for keeping the FPGA's
 * NV design owning the SSPI pins (prelude MISO 0x80 user-mode vs stock 0xFF
 * config-wait). When set: USART2 UEN never asserted, NVIC off, Step 3b + Step 8
 * USART traffic skipped, and NO auto-tasks created — the SPI3 wire stays quiet so
 * `fpga reinit` / `spi3 xfer` run unperturbed. Watch first 0x05 prelude MISO for
 * 0x80->0xFF and `0x11` IDCODE for 01 20 68 1B. See docs/fpga_stock_bringup_diff_plan.md. */
/* Experiment E: spin forever at the config-enable instant for an SWD state dump.
 * Build with `make guest-spin`. Never enable in a normal build — the device
 * parks in fpga_init and never reaches the scheduler (recover by reflashing). */
#ifndef FPGA_SPIN_AT_CONFIG_ENABLE
#define FPGA_SPIN_AT_CONFIG_ENABLE  0
#endif

#ifndef FPGA_USART_SILENT_SCOPE
#define FPGA_USART_SILENT_SCOPE  0
#endif

/* Stock-fidelity config build (2026-07-28, Experiment F — follow-on to Exp E).
 * The Exp E SWD dump parked both stock and our firmware one instruction before
 * 0x15 reaches the SPI3 data register and diffed every peripheral register.
 * Clock tree came back byte-identical; what remained were five enumerable MCU
 * state differences. This flag closes ALL of them at once so a single bench
 * cycle can decide whether MCU-side state is the cause at all:
 *
 *   1. SPI3 CTRL1 BR — stock 0x347 (BR=0, /2), ours 0x37f (BR=7, /256).
 *      => cmd_br forced to 0. The /256 read clock moves OUT of the config
 *         frame and into the probe_edit STATUS read, which already runs at
 *         /256 in its own CS frame, so we keep a VALID readout.
 *   2. PB4 / SPI3_MISO — stock input PULL-UP, ours input FLOATING. This is
 *      why undriven MISO reads 0xFF in the stock capture; ours had no defined
 *      idle level, so every status read this project has taken was made under
 *      the wrong electrical conditions. Cannot itself gate config (it is an
 *      MCU input) — this is a measurement fix.
 *   3. PC2 and PB12 — stock drives both output push-pull HIGH; ours leaves
 *      them floating. NOT covered by the Exp C frontend ablation.
 *   4. USART2 UEN — stock CLEAR at the CONFIG_ENABLE instant (measured:
 *      CTRL1=0x0000002c). Paired with FPGA_USART_SILENT_SCOPE by the Makefile
 *      target. Already bench-refuted in isolation (2026-06-13); included only
 *      so this build is a clean superset.
 *   5. SPI2 peripheral clock — stock sets APB1EN bit14; ours does not.
 *
 * PB9 is deliberately NOT driven: stock has it AF push-pull, and an AF output
 * level is not reflected in ODR, so we cannot know what level to match. See
 * FPGA_FIDELITY_DRIVE_PB9 below.
 *
 * Build with `make guest-fidelity`. Full rationale + raw dumps:
 * reverse_engineering/analysis_v120/expE_swd_state_diff_2026-07-28.md */
#ifndef FPGA_STOCK_FIDELITY
#define FPGA_STOCK_FIDELITY  0
#endif

/* PB9 is AF push-pull in stock (likely a timer channel — PB8, the LCD
 * backlight, is AF-PP too and stock PWM-dims it). ODR does not reflect an AF
 * output level, so driving PB9 as a plain GPIO is a guess in BOTH directions.
 * Off by default; flip only to sweep it deliberately. */
#ifndef FPGA_FIDELITY_DRIVE_PB9
#define FPGA_FIDELITY_DRIVE_PB9  0
#endif

/* Boot-into-bus-released experiment (2026-06-14, experimental/esp32-bringup).
 * For the external-master bench rig: the MCU brings up SPI3/USART/control pins,
 * then — instead of running its own SSPI config upload — immediately hands the
 * bus to an external master (ESP32 on the SPI3 pads, or an FT232H on the JTAG
 * TAP pads @maksidze traced in #18). fpga_init() calls fpga_bus_release()
 * (tri-states PB3/PB5/PB6, stages PC6/PB11 HIGH, PC9 power-hold kept) and bails
 * before the meter frontend + meter USART traffic, so the FPGA's config port is
 * pristine for JTAG/SSPI with ZERO MCU interference. No auto-tasks created.
 * UNTESTED. See tools/esp32_sspi_bringup/README.md. Revert to 0 for normal builds. */
#ifndef FPGA_BUS_RELEASED_BOOT
#define FPGA_BUS_RELEASED_BOOT  0
#endif

static void spi3_set_br(uint32_t br)
{
    FPGA_SPI->ctrl1 &= ~(1u << 6);              /* SPE = 0 */
    FPGA_SPI->ctrl1 = (FPGA_SPI->ctrl1 & ~(7u << 3)) | ((br & 7u) << 3);
    FPGA_SPI->ctrl1 |= (1u << 6);               /* SPE = 1 */
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

static void fpga_scope_delay_ms(uint32_t ms)
{
    if (ms == 0) return;

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        systick_delay_ms(ms);
    }
}

static void fpga_timed_send_cmd(uint8_t cmd_hi, uint8_t cmd_lo, uint32_t delay_ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && usart_tx_queue != NULL) {
        uint16_t item = ((uint16_t)cmd_hi << 8) | cmd_lo;

        /* Scope reinit is a deliberate control path, so it's worth waiting
         * briefly for queue space instead of silently dropping commands. */
        (void)xQueueSend(usart_tx_queue, &item, pdMS_TO_TICKS(100));
    } else {
        usart2_send_cmd(cmd_hi, cmd_lo);
    }

    fpga_scope_delay_ms(delay_ms);
}

static void fpga_scope_select_timing(const scope_state_t *ss,
                                     uint8_t *run_mode,
                                     uint8_t *sample_depth,
                                     uint8_t *tb_prescaler,
                                     uint8_t *tb_period,
                                     uint8_t *tb_mode,
                                     uint8_t *acq_mode);
static void fpga_send_scope_range_block(const scope_state_t *ss);
static uint8_t fpga_scope_trigger_lsb(const scope_state_t *ss);
static uint8_t fpga_scope_trigger_mode_byte(const scope_state_t *ss);
static uint8_t fpga_scope_prefix_cmd(const scope_state_t *ss);

void fpga_wire_send_word(uint16_t word, uint32_t delay_ms)
{
    fpga_timed_send_cmd((uint8_t)(word >> 8), (uint8_t)(word & 0xFF), delay_ms);
}

static void fpga_wire_send_bank_words(uint8_t bank_mode)
{
    static const uint16_t ch1_words[] = { 0x050C, 0x050E, 0x0510, 0x0511 };
    static const uint16_t ch2_words[] = { 0x050D, 0x0517, 0x0516, 0x0515 };

    if (bank_mode == 0 || bank_mode == 2) {
        for (size_t i = 0; i < sizeof(ch1_words) / sizeof(ch1_words[0]); i++) {
            fpga_wire_send_word(ch1_words[i], 15);
        }
    }

    if (bank_mode == 1 || bank_mode == 2) {
        for (size_t i = 0; i < sizeof(ch2_words) / sizeof(ch2_words[0]); i++) {
            fpga_wire_send_word(ch2_words[i], 15);
        }
    }
}

static void fpga_send_scope_runtime_blocks(const scope_state_t *ss)
{
    uint8_t run_mode;
    uint8_t sample_depth;
    uint8_t tb_prescaler;
    uint8_t tb_period;
    uint8_t tb_mode;
    uint8_t acq_mode;
    uint8_t trigger_prefix;

    fpga_scope_select_timing(ss, &run_mode, &sample_depth,
                             &tb_prescaler, &tb_period, &tb_mode, &acq_mode);
    fpga.acq_mode = acq_mode;

    fpga_send_scope_range_block(ss);

    fpga_timed_send_cmd(run_mode, FPGA_CMD_FREQ_20, 15);
    fpga_timed_send_cmd(sample_depth, FPGA_CMD_FREQ_21, 15);
    fpga_timed_send_cmd(tb_prescaler, 0x26, 15);
    fpga_timed_send_cmd(tb_period, 0x27, 15);
    fpga_timed_send_cmd(tb_mode, 0x28, 20);

    trigger_prefix = fpga_scope_prefix_cmd(ss);
    fpga_timed_send_cmd(0x00, trigger_prefix, 15);
    fpga_timed_send_cmd(fpga_scope_trigger_lsb(ss), 0x16, 15);
    fpga_timed_send_cmd(0x00, 0x17, 15);
    fpga_timed_send_cmd(fpga_scope_trigger_mode_byte(ss), 0x18, 15);
    fpga_timed_send_cmd(0x00, 0x19, 20);
}

void fpga_wire_entry(uint8_t bank_mode)
{
    if (!fpga.initialized) return;

    fpga_wire_send_word(0x02A0, 20);
    fpga_wire_send_word(0x0501, 15);
    fpga_wire_send_bank_words(bank_mode);
    fpga_wire_send_word(0x0503, 20);
}

void fpga_wire_scope_sequence(uint8_t bank_mode)
{
    const scope_state_t *ss;

    if (!fpga.initialized) return;

    ss = scope_state_get();
    fpga_wire_entry(bank_mode);
    fpga_send_scope_runtime_blocks(ss);
}

/* ═══════════════════════════════════════════════════════════════════
 * Stock-State Bench Drivers
 * ═══════════════════════════════════════════════════════════════════ */

static void fpga_stock_shadow_clear_detail(void)
{
    memset((void *)fpga.stock_shadow.detail_bits, 0, sizeof(fpga.stock_shadow.detail_bits));
}

static bool fpga_stock_shadow_detail_nonzero(void)
{
    for (size_t i = 0; i < sizeof(fpga.stock_shadow.detail_bits); i++) {
        if (fpga.stock_shadow.detail_bits[i] != 0) return true;
    }
    return false;
}

static void fpga_stock_shadow_seed_detail_from_cursor(void)
{
    uint8_t max_items = fpga.stock_shadow.e1b;
    uint8_t index = fpga.stock_shadow.e1d;
    uint8_t byte_index;
    uint8_t bit_index;

    fpga_stock_shadow_clear_detail();
    if (max_items == 0) return;

    if (index >= max_items) index = (uint8_t)(max_items - 1);
    if (index > 47) index = 47;

    byte_index = index / 6;
    bit_index = index % 6;
    fpga.stock_shadow.detail_bits[byte_index] = (uint8_t)(1U << bit_index);
}

static void fpga_stock_shadow_fill_detail(void)
{
    uint8_t max_items = fpga.stock_shadow.e1b;
    uint8_t remaining = (max_items > 48) ? 48 : max_items;

    fpga_stock_shadow_clear_detail();
    for (size_t i = 0; i < sizeof(fpga.stock_shadow.detail_bits) && remaining > 0; i++) {
        uint8_t bits = (remaining >= 6) ? 6 : remaining;
        fpga.stock_shadow.detail_bits[i] = (uint8_t)((1U << bits) - 1U);
        remaining = (remaining > bits) ? (uint8_t)(remaining - bits) : 0;
    }
}

static void fpga_stock_shadow_adjust(bool next)
{
    uint8_t limit = fpga.stock_shadow.e1b;

    if (!fpga.initialized) return;
    if (limit == 0) return;

    if (fpga.stock_shadow.e1d >= limit) {
        fpga.stock_shadow.e1d = (uint8_t)(limit - 1);
    } else if (next) {
        if ((uint8_t)(fpga.stock_shadow.e1d + 1U) < limit) {
            fpga.stock_shadow.e1d++;
        }
    } else if (fpga.stock_shadow.e1d > 0) {
        fpga.stock_shadow.e1d--;
    }

    if (fpga.stock_shadow.visible_state == 5 && fpga.stock_shadow.e1c == 0) {
        fpga_timed_send_cmd(0x00, 0x27, 15);
        fpga_timed_send_cmd(0x00, 0x28, 20);
    } else if (fpga.stock_shadow.visible_state == 6) {
        fpga_timed_send_cmd(0x00, 0x29, 20);
    }
}

void fpga_stock_diag_prev(void)
{
    fpga_stock_shadow_adjust(false);
}

void fpga_stock_diag_next(void)
{
    fpga_stock_shadow_adjust(true);
}

void fpga_stock_diag_select(void)
{
    if (!fpga.initialized) return;
    if (fpga.stock_shadow.visible_state != 5) return;
    if (fpga.stock_shadow.e1c != 0 || fpga.stock_shadow.e1b == 0) return;

    if (fpga.stock_shadow.e1a == 0) {
        fpga.stock_shadow.e1a = 1;
        fpga_stock_shadow_seed_detail_from_cursor();
    } else {
        fpga.stock_shadow.e1a = 0;
        fpga_stock_shadow_clear_detail();
    }

    fpga_timed_send_cmd(0x00, 0x28, 15);
    fpga_timed_send_cmd(0x00, 0x26, 20);
}

void fpga_stock_diag_toggle(void)
{
    if (!fpga.initialized) return;
    if (fpga.stock_shadow.visible_state != 5) return;
    if (fpga.stock_shadow.e1c != 0 || fpga.stock_shadow.e1b == 0) return;

    if (fpga.stock_shadow.e1a == 2) {
        fpga.stock_shadow.e1a = 1;
        fpga_stock_shadow_seed_detail_from_cursor();
    } else {
        fpga.stock_shadow.e1a = 2;
        fpga_stock_shadow_fill_detail();
    }

    fpga_timed_send_cmd(0x00, 0x26, 15);
    fpga_timed_send_cmd(0x00, 0x28, 20);
}

void fpga_stock_diag_commit(void)
{
    if (!fpga.initialized) return;

    if (fpga.stock_shadow.visible_state == 5 &&
        fpga.stock_shadow.e1c == 0 &&
        fpga.stock_shadow.e1a != 0 &&
        fpga_stock_shadow_detail_nonzero()) {
        fpga.stock_shadow.e1c = 2;
        fpga_timed_send_cmd(0x00, 0x2A, 20);
        return;
    }

    if (fpga.stock_shadow.visible_state == 5 && fpga.stock_shadow.e1c == 2) {
        fpga.stock_shadow.e1c = 1;
        fpga_timed_send_cmd(0x00, 0x2A, 20);
        return;
    }

    if (fpga.stock_shadow.visible_state == 5 && fpga.stock_shadow.e1c == 1) {
        fpga_timed_send_cmd(0x00, 0x2B, 20);
    }
}

void fpga_stock_diag_consume(void)
{
    if (!fpga.initialized) return;
    if (fpga.stock_shadow.visible_state != 9 || fpga.stock_shadow.latch_355 == 0) return;

    if (fpga.stock_shadow.substate == 2) {
        fpga_stock_diag_seed_base2();
        return;
    }

    fpga.stock_shadow.phase = 1;
    fpga.stock_shadow.substate = 2;
    fpga.stock_shadow.flags = 0;
    fpga_timed_send_cmd(0x00, 0x13, 15);
    fpga_timed_send_cmd(0x00, 0x14, 20);
}

void fpga_stock_diag_bridge_fixed(void)
{
    if (!fpga.initialized) return;

    /* Candidate downstream branch after the detailed sub2=5 path stages
     * 0x0501 at F69 and queues 0x13, then 0x14. We do not claim this is
     * exact stock control flow; it is a bounded bench probe of the fixed
     * 0x0501 materializer family around 0x08006060. */
    fpga_timed_send_cmd(0x00, 0x13, 15);
    fpga_timed_send_cmd(0x00, 0x14, 20);
    fpga_wire_send_word(0x0501, 15);
    fpga_timed_send_cmd(0x00, 0x1D, 15);
    fpga_timed_send_cmd(0x00, 0x1B, 20);
}

void fpga_stock_diag_bridge_dynamic(uint8_t bank_mode)
{
    if (!fpga.initialized) return;

    /* Alternate candidate downstream branch: after 0x13/0x14, land in the
     * dynamic 0x050x family materializer around 0x08006120 instead of the
     * fixed 0x0501 sibling. */
    fpga_timed_send_cmd(0x00, 0x13, 15);
    fpga_timed_send_cmd(0x00, 0x14, 20);
    fpga_wire_send_bank_words(bank_mode);
    fpga_timed_send_cmd(0x00, 0x1B, 20);
}

void fpga_stock_diag_reenter(void)
{
    if (!fpga.initialized) return;

    /* Conservative stock-ish bridging:
     * - visible state 6 enters the editor by collapsing to state 5
     * - visible state 9 can consume its packed preset before we re-enter the
     *   current clean-room scope configuration path
     */
    if (fpga.stock_shadow.visible_state == 9 && fpga.stock_shadow.latch_355 != 0) {
        fpga_stock_diag_consume();
    }

    if (fpga.stock_shadow.visible_state == 6 && fpga.stock_shadow.e1b != 0) {
        fpga.stock_shadow.visible_state = 5;
    }

    fpga_scope_reinit();
}

/* ═══════════════════════════════════════════════════════════════════
 * Scope Reinit Helpers
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t fpga_scope_channel_mask(const scope_state_t *ss)
{
    uint8_t mask = 0;

    if (ss->ch1.enabled) mask |= 0x01;
    if (ss->ch2.enabled) mask |= 0x02;

    return mask ? mask : 0x01;
}

static uint8_t fpga_scope_primary_range(const scope_state_t *ss)
{
    const channel_state_t *ch;

    if (ss->trigger.source == TRIG_SRC_CH2 && ss->ch2.enabled) {
        ch = &ss->ch2;
    } else if (ss->ch1.enabled) {
        ch = &ss->ch1;
    } else {
        ch = &ss->ch2;
    }

    return (ch->vdiv_idx < VDIV_COUNT) ? ch->vdiv_idx : (VDIV_COUNT - 1);
}

static void fpga_set_scope_frontend_range(uint8_t range_idx)
{
    /* Approximate gpio_mux_portc_porte / gpio_mux_porta_portb using the
     * reconstructed truth table from core_subsystems_annotated.c. This is
     * intentionally simple: we want a stable, obviously scope-like relay
     * state instead of inheriting meter or siggen leftovers. */
    switch (range_idx) {
    case 0:
    case 1:
        GPIOC->clr = (1U << 12);
        GPIOE->scr = (1U << 4);
        GPIOE->clr = (1U << 5);
        GPIOE->clr = (1U << 6);
        GPIOA->clr = (1U << 15);
        GPIOA->clr = (1U << 10);
        GPIOB->clr = (1U << 10);
        break;

    case 2:
    case 3:
    case 4:
        GPIOC->clr = (1U << 12);
        GPIOE->scr = (1U << 4);
        if ((range_idx & 1U) != 0) GPIOE->scr = (1U << 5);
        else                       GPIOE->clr = (1U << 5);
        GPIOE->clr = (1U << 6);
        GPIOA->scr = (1U << 15);
        GPIOA->scr = (1U << 10);
        GPIOB->clr = (1U << 10);
        break;

    case 5:
    case 6:
        GPIOC->clr = (1U << 12);
        GPIOE->clr = (1U << 4);
        if ((range_idx & 1U) != 0) GPIOE->scr = (1U << 5);
        else                       GPIOE->clr = (1U << 5);
        GPIOE->scr = (1U << 6);
        GPIOA->clr = (1U << 15);
        GPIOA->clr = (1U << 10);
        GPIOB->clr = (1U << 10);
        break;

    case 7:
        GPIOC->scr = (1U << 12);
        GPIOE->clr = (1U << 4);
        GPIOE->scr = (1U << 5);
        GPIOE->scr = (1U << 6);
        GPIOA->clr = (1U << 15);
        GPIOA->clr = (1U << 10);
        GPIOB->clr = (1U << 10);
        break;

    case 8:
        GPIOC->clr = (1U << 12);
        GPIOE->scr = (1U << 4);
        GPIOE->clr = (1U << 5);
        GPIOE->scr = (1U << 6);
        GPIOA->clr = (1U << 15);
        GPIOA->clr = (1U << 10);
        GPIOB->scr = (1U << 10);
        break;

    case 9:
    default:
        GPIOC->clr = (1U << 12);
        GPIOE->scr = (1U << 4);
        GPIOE->scr = (1U << 5);
        GPIOE->scr = (1U << 6);
        GPIOA->clr = (1U << 15);
        GPIOA->clr = (1U << 10);
        GPIOB->clr = (1U << 10);
        break;
    }

    /* Shared analog enables stay asserted in scope mode. */
    GPIOB->scr = (1U << 9);
    GPIOA->scr = (1U << 6);
}

static uint8_t fpga_probe_cmd_byte(void)
{
    return (GPIOC->idt & (1U << 7)) ? 0x07 : FPGA_CMD_METER_NOPROBE;
}

static void fpga_set_meter_frontend_baseline(void)
{
    /* Restore the known-good meter posture from boot init so shell-driven
     * meter recovery doesn't depend on whichever scope range ran last. */
    GPIOB->scr = PB11_MASK;   /* FPGA active */
    GPIOC->scr = PC6_MASK;    /* SPI path enabled */
    GPIOC->scr = (1U << 11);  /* Meter MUX on */
    GPIOC->scr = (1U << 12);  /* Route probe to meter path */

    GPIOE->scr = (1U << 4);
    GPIOE->clr = (1U << 5);
    GPIOE->scr = (1U << 6);

    GPIOB->scr = (1U << 9);
    GPIOA->scr = (1U << 6);
    GPIOA->scr = (1U << 15);
    GPIOA->scr = (1U << 10);
    GPIOB->clr = (1U << 10);
}

static void fpga_send_meter_wake_preamble(void)
{
    uint8_t probe_cmd = fpga_probe_cmd_byte();

    fpga_set_meter_frontend_baseline();
    fpga_scope_delay_ms(20);

    /* Boot-time meter bring-up uses cmd_hi=0x05 for this block. Keep that
     * path available as a live experiment before scope mode re-entry. */
    fpga_timed_send_cmd(0x05, 0x08, 10);
    fpga_timed_send_cmd(0x05, FPGA_CMD_METER_START, 10);
    fpga_timed_send_cmd(0x05, probe_cmd, 10);
    fpga_timed_send_cmd(0x05, FPGA_CMD_METER_VAR_14, 20);
}

static uint8_t fpga_scope_trigger_lsb(const scope_state_t *ss)
{
    int level = 128 - ss->trigger.level;

    if (level < 0) level = 0;
    if (level > 255) level = 255;

    return (uint8_t)level;
}

static uint8_t fpga_scope_trigger_mode_byte(const scope_state_t *ss)
{
    uint8_t mode = 0;

    if (ss->trigger.source == TRIG_SRC_CH2) mode |= 0x01;
    if (ss->trigger.edge == TRIG_FALLING)   mode |= 0x80;

    switch (ss->trigger.mode) {
    case TRIG_SINGLE: mode |= 0x20; break;
    case TRIG_NORMAL: mode |= 0x10; break;
    case TRIG_AUTO:
    default:          mode |= 0x00; break;
    }

    return mode;
}

static uint8_t fpga_scope_prefix_cmd(const scope_state_t *ss)
{
    return (ss->trigger.source == TRIG_SRC_CH2) ? 0x0A : 0x07;
}

static uint8_t fpga_scope_gain_param(const channel_state_t *ch)
{
    uint8_t param = ch->vdiv_idx & 0x0F;

    if (ch->probe == PROBE_10X) param |= 0x10;
    if (!ch->enabled)           param |= 0x80;

    return param;
}

static uint8_t fpga_scope_offset_param(const channel_state_t *ch)
{
    int offset = 128 - ch->position;

    if (offset < 0)   offset = 0;
    if (offset > 255) offset = 255;

    return (uint8_t)offset;
}

static uint8_t fpga_scope_coupling_param(const scope_state_t *ss)
{
    uint8_t param = 0;

    param |= (uint8_t)(ss->ch1.coupling & 0x03);
    param |= (uint8_t)((ss->ch2.coupling & 0x03) << 2);

    if (ss->ch1.bw_limit) param |= 0x10;
    if (ss->ch2.bw_limit) param |= 0x20;

    return param;
}

static void fpga_send_scope_range_block(const scope_state_t *ss)
{
    /* Stock range/coupling updates dispatch a channel-bank prefix followed by
     * 0x1A..0x1E. We still do not have the original state-packer that filled
     * bytes[4..8], so keep this as a best-effort projection of live UI state
     * into the single-byte hi params our current queue transport supports. */
    fpga_timed_send_cmd(0x00, fpga_scope_prefix_cmd(ss), 10);
    fpga_timed_send_cmd(fpga_scope_gain_param(&ss->ch1), FPGA_CMD_CH1_GAIN, 10);
    fpga_timed_send_cmd(fpga_scope_offset_param(&ss->ch1), FPGA_CMD_CH1_OFFSET, 10);
    fpga_timed_send_cmd(fpga_scope_gain_param(&ss->ch2), FPGA_CMD_CH2_GAIN, 10);
    fpga_timed_send_cmd(fpga_scope_offset_param(&ss->ch2), FPGA_CMD_CH2_OFFSET, 10);
    fpga_timed_send_cmd(fpga_scope_coupling_param(ss), FPGA_CMD_COUPLING, 20);
}

static void fpga_scope_select_timing(const scope_state_t *ss,
                                     uint8_t *run_mode,
                                     uint8_t *sample_depth,
                                     uint8_t *tb_prescaler,
                                     uint8_t *tb_period,
                                     uint8_t *tb_mode,
                                     uint8_t *acq_mode)
{
    if (!ss->running) {
        *run_mode = 0x00;
    } else {
        switch (ss->trigger.mode) {
        case TRIG_SINGLE: *run_mode = 0x01; break;
        case TRIG_NORMAL: *run_mode = 0x02; break;
        case TRIG_AUTO:
        default:          *run_mode = 0x03; break;
        }
    }

    if (ss->timebase_idx <= 3) {
        *sample_depth = 0x01;
        *tb_prescaler = 0x20;
        *tb_period    = 0x80;
        *tb_mode      = 0x00;
        *acq_mode     = FPGA_ACQ_ROLL + 1;
    } else if (ss->timebase_idx <= 9) {
        *sample_depth = 0x02;
        *tb_prescaler = 0x08;
        *tb_period    = 0x40;
        *tb_mode      = 0x01;
        *acq_mode     = FPGA_ACQ_NORMAL + 1;
    } else {
        *sample_depth = 0x02;
        *tb_prescaler = 0x04;
        *tb_period    = 0x20;
        *tb_mode      = 0x01;
        *acq_mode     = FPGA_ACQ_DUAL + 1;
    }
}

static void fpga_send_scope_sequence(const scope_state_t *ss)
{
    uint8_t run_mode;
    uint8_t sample_depth;
    uint8_t tb_prescaler;
    uint8_t tb_period;
    uint8_t tb_mode;
    uint8_t acq_mode;
    uint8_t trigger_prefix;

    fpga_scope_select_timing(ss, &run_mode, &sample_depth,
                             &tb_prescaler, &tb_period, &tb_mode, &acq_mode);

    fpga.acq_mode = acq_mode;

    /* Scope entry block. The 0x0B..0x11 bytes are still partly guessed, so
     * keep the empirically least-bad bank-2 defaults and pair them with
     * explicit trigger/timebase commands derived from live UI state. */
    fpga_timed_send_cmd(0x00, FPGA_CMD_RESET, 20);
    fpga_timed_send_cmd(fpga_scope_channel_mask(ss), FPGA_CMD_SCOPE_CH, 15);
    fpga_timed_send_cmd(0x01, FPGA_CMD_SCOPE_CFG_0B, 15);
    fpga_timed_send_cmd(ss->ch2.enabled ? 0x01 : 0x00, FPGA_CMD_SCOPE_CFG_0C, 15);
    fpga_timed_send_cmd(0x03, FPGA_CMD_SCOPE_CFG_0D, 15);
    fpga_timed_send_cmd(0x80, FPGA_CMD_SCOPE_CFG_0E, 15);
    fpga_timed_send_cmd(0x04, FPGA_CMD_SCOPE_CFG_0F, 15);
    fpga_timed_send_cmd(0x02, FPGA_CMD_SCOPE_CFG_10, 15);
    fpga_timed_send_cmd(0x01, FPGA_CMD_SCOPE_CFG_11, 20);

    /* Stock scope setup also pushes a channel range/coupling block via
     * 0x07/0x0A + 0x1A..0x1E when the frontend changes. Re-apply that here
     * so scope entry does not rely only on local relay writes. */
    fpga_send_scope_range_block(ss);

    /* Runtime acquisition and timebase config. */
    fpga_timed_send_cmd(run_mode, FPGA_CMD_FREQ_20, 15);
    fpga_timed_send_cmd(sample_depth, FPGA_CMD_FREQ_21, 15);
    fpga_timed_send_cmd(tb_prescaler, 0x26, 15);
    fpga_timed_send_cmd(tb_period, 0x27, 15);
    fpga_timed_send_cmd(tb_mode, 0x28, 20);

    /* Scope trigger block follows the stock runtime trigger builder:
     * channel prefix (0x07/0x0A), then 0x16..0x19. */
    trigger_prefix = fpga_scope_prefix_cmd(ss);
    fpga_timed_send_cmd(0x00, trigger_prefix, 15);
    fpga_timed_send_cmd(fpga_scope_trigger_lsb(ss), 0x16, 15);
    fpga_timed_send_cmd(0x00, 0x17, 15);
    fpga_timed_send_cmd(fpga_scope_trigger_mode_byte(ss), 0x18, 15);
    fpga_timed_send_cmd(0x00, 0x19, 20);
}

static void fpga_scope_select_runtime(const scope_state_t *ss,
                                      uint8_t *run_mode,
                                      uint8_t *sample_depth,
                                      uint8_t *tb_prescaler,
                                      uint8_t *tb_period,
                                      uint8_t *tb_mode,
                                      uint8_t *acq_mode)
{
    fpga_scope_select_timing(ss, run_mode, sample_depth,
                             tb_prescaler, tb_period, tb_mode, acq_mode);
    fpga.acq_mode = *acq_mode;
}

void fpga_scope_refresh_acq_mode(void)
{
    const scope_state_t *ss;
    uint8_t run_mode;
    uint8_t sample_depth;
    uint8_t tb_prescaler;
    uint8_t tb_period;
    uint8_t tb_mode;
    uint8_t acq_mode;

    if (!fpga.initialized) return;

    ss = scope_state_get();
    fpga_scope_select_runtime(ss, &run_mode, &sample_depth,
                              &tb_prescaler, &tb_period, &tb_mode, &acq_mode);

    fpga_timed_send_cmd(run_mode, FPGA_CMD_FREQ_20, 15);
    fpga_timed_send_cmd(sample_depth, FPGA_CMD_FREQ_21, 20);
}

void fpga_scope_heartbeat(void)
{
    const scope_state_t *ss;
    uint8_t run_mode;
    uint8_t sample_depth;
    uint8_t tb_prescaler;
    uint8_t tb_period;
    uint8_t tb_mode;
    uint8_t acq_mode;

    if (!fpga.initialized) return;

    ss = scope_state_get();
    fpga_scope_select_runtime(ss, &run_mode, &sample_depth,
                              &tb_prescaler, &tb_period, &tb_mode, &acq_mode);

    /* Stock cmd 3 re-applies timebase state, then re-arms acquisition. */
    fpga_timed_send_cmd(tb_prescaler, 0x26, 15);
    fpga_timed_send_cmd(tb_period, 0x27, 15);
    fpga_timed_send_cmd(tb_mode, 0x28, 20);
    (void)fpga_trigger_scope_read();
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

        /* Bus handed to an external SSPI master (fpga_bus_release) — stay
         * off SPI3 entirely to avoid contention with the ESP32. */
        if (fpga.bus_released) continue;

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
        {
            const scope_state_t *ss = scope_state_get();
            uint8_t voltage_range = fpga_scope_primary_range(ss) & 0x7F;
            spi3_xfer(0x80 | voltage_range);
        }
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
 * SPI3 FPGA config handshake (shared by fpga_init and `fpga reinit`)
 *
 * Sequence is the stock-captured order (issue-#18 Saleae capture):
 *   [PB11 HIGH, 1ms] CS↑00 | CS↓ 05 00 CS↑00 | gap | CS↓ 12 00 CS↑00 | gap
 *   | CS↓ 15 00 CS↑00 | CS↓ 3B <115638-byte bitstream> CS↑00
 *   | CS↓ 3A <close> CS↑00 | CS↓00 CS↑00 | [post_close delay]
 *   | CS↓ 01 08 CS↑ | 02 03 | 06 00 | 07 00 | 08 AD | CS↓ 03 <status×4> CS↑
 * ═══════════════════════════════════════════════════════════════════ */
uint8_t fpga_spi3_config_sequence(const fpga_cfg_seq_opts_t *opt)
{
    gpio_init_type gpio_cfg;
    gpio_default_para_init(&gpio_cfg);

    /* Keep the acquisition task off the SPI3 bus during the handshake.
     * No-op pre-RTOS; essential when replayed live via `fpga reinit`. */
    int sched_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    if (sched_running && acq_task_handle) vTaskSuspend(acq_task_handle);

    (void)FPGA_SPI->dt;                  /* Discard any stale RX data */
    fpga.diag_spi_sts = FPGA_SPI->sts;   /* STS before handshake */

    /* Optional FPGA reset pulse (rosenrot00's working 2C23T loader does this;
     * our 2C53T sequence lacks it). Configure the chosen pin as push-pull
     * output, drive LOW for reset_low_ms, then HIGH 1ms before the handshake. */
    if (opt->reset_port >= 1 && opt->reset_port <= 5) {
        gpio_type *rport = (gpio_type *)((const gpio_type *[]){
            GPIOA, GPIOB, GPIOC, GPIOD, GPIOE }[opt->reset_port - 1]);
        uint32_t rmask = (1u << opt->reset_pin);
        gpio_cfg.gpio_pins = rmask;
        gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
        gpio_init(rport, &gpio_cfg);
        rport->clr = rmask;                 /* RESET LOW */
        fpga_scope_delay_ms(opt->reset_low_ms ? opt->reset_low_ms : 10);
        rport->scr = rmask;                 /* RESET HIGH */
        fpga_scope_delay_ms(1);
    }

    /* Strap-hold (2026-06-13 GPIO-audit lead): drive Port-D pins that stock
     * asserts on scope-mode entry but our firmware never touches, HELD through
     * the entire handshake (prelude→0x3B→0x3A→status). NOT a pulse — a held
     * level, matching stock. PD2 is the prime config-entry-lever candidate.
     * See unmapped_mcu_fpga_pin_candidates.md §4a. */
    if (opt->strap_pd2) {
        gpio_cfg.gpio_pins = (1u << 2);                    /* PD2 */
        gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
        gpio_init(GPIOD, &gpio_cfg);
        if (opt->strap_pd2 == 1) GPIOD->scr = (1u << 2);   /* hold HIGH (stock) */
        else                     GPIOD->clr = (1u << 2);   /* hold LOW */
    }
    if (opt->strap_pd1213) {
        gpio_cfg.gpio_pins = (1u << 12) | (1u << 13);      /* PD12, PD13 */
        gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
        gpio_init(GPIOD, &gpio_cfg);
        if (opt->strap_pd1213 == 1) GPIOD->scr = (3u << 12);  /* hold HIGH */
        else                        GPIOD->clr = (3u << 12);  /* hold LOW */
    }

    if (opt->arm_pb11) {
        /* PB11 HIGH ~1ms before the CS pulse — stock raises it 1.0ms before
         * the bare CS pulse and holds it HIGH through the upload (capture). */
        gpio_cfg.gpio_pins = GPIO_PINS_11;
        gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
        gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
        gpio_init(GPIOB, &gpio_cfg);
        GPIOB->scr = PB11_MASK;
        fpga_scope_delay_ms(1);
    }

    /* WIRE-EXACT to the issue-#18 stock capture. The capture clocks NOTHING
     * while CS is high; every byte sits inside a CS-LOW frame. Our previous
     * version clocked flush 0x00 bytes with CS HIGH between frames — 8 stray
     * SCK edges per gap that, if the GW1N SSPI shift register isn't strictly
     * CS-gated, desync the command parser so CONFIG_ENABLE (0x15) never lands.
     * Removed entirely below. See SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md. */

    /* Command-phase clock. The SSPI read path is clock-limited (IDCODE reads
     * garbage at /2, clean at /256), so the prelude/close/status all run at
     * opt->cmd_br here; only the bulk 0x3B payload switches to opt->upload_br
     * and then returns to cmd_br for the close/status reads. Restored to /2 at
     * function exit. */
    spi3_set_br(opt->cmd_br);

    /* [0a] DIAGNOSTIC (reload_3c): Gowin SSPI RELOAD (0x3C) — software reconfig
     * trigger, sent at /256 in its own CS frame before the prelude, with a settle
     * delay. Tests whether it knocks the running NV design back toward config
     * (clears GWVLD/FLASH_LOCK so CONFIG_ENABLE can engage SYSTEM_EDIT_MODE). */
    if (opt->reload_3c) {
        spi3_set_br(7);                  /* /256 */
        SPI3_CS_ASSERT();
        spi3_xfer(0x3C);
        spi3_xfer(0x00);
        SPI3_CS_DEASSERT();
        spi3_set_br(opt->cmd_br);
        fpga_scope_delay_ms(50);
    }

    /* [0] bare CS pulse — CS low→high with ZERO clocks (stock t=3.6082).
     * A CS assertion is the SSPI frame-sync that resets the command FSM. */
    SPI3_CS_DEASSERT();
    (void)FPGA_SPI->dt;                  /* drain stale RX without clocking */
    SPI3_CS_ASSERT();
    for (volatile int d = 0; d < 50; d++) { __asm__ volatile("nop"); }
    SPI3_CS_DEASSERT();
    fpga_scope_delay_ms(opt->prelude_gap_ms);   /* stock waits ~100ms → 0x05 */

    /* [1-3] CONFIG_ENABLE prelude — 05 00 / 12 00 / 15 00.
     * Framing per opt->prelude_frame_mode (sweep knob; 0 = stock-faithful):
     *   0 split    : CS↓05 00↑ | CS↓12 00↑ | CS↓15 00↑   (then 3B in its own frame)
     *   1 combined : CS↓05 00 12 00 15 00↑                (then 3B in its own frame)
     *   2 merge    : CS↓05 00↑ | CS↓12 00↑ | CS↓15 00 3B <table>↑  (15 shares upload frame)
     * init_hs[] capture indices are identical across all three. */
    if (opt->prelude_frame_mode == 1) {
        SPI3_CS_ASSERT();
        fpga.init_hs[1] = spi3_xfer(0x05);
        fpga.init_hs[2] = spi3_xfer(0x00);
        fpga.init_hs[4] = spi3_xfer(0x12);
        fpga.init_hs[5] = spi3_xfer(0x00);
        fpga.init_hs[7] = spi3_xfer(0x15);
        fpga.init_hs[8] = spi3_xfer(0x00);
        SPI3_CS_DEASSERT();
    } else {
        /* [1] 05 00 */
        SPI3_CS_ASSERT();
        fpga.init_hs[1] = spi3_xfer(0x05);
        fpga.init_hs[2] = spi3_xfer(0x00);
        SPI3_CS_DEASSERT();
        fpga_scope_delay_ms(opt->prelude_gap_ms);

        /* [2] 12 00 */
        SPI3_CS_ASSERT();
        fpga.init_hs[4] = spi3_xfer(0x12);
        fpga.init_hs[5] = spi3_xfer(0x00);
        SPI3_CS_DEASSERT();
        fpga_scope_delay_ms(opt->prelude_gap_ms);

        /* [3] 15 00 — own frame (mode 0) or held LOW into the upload (mode 2) */
        SPI3_CS_ASSERT();
#if FPGA_SPIN_AT_CONFIG_ENABLE
        /* Experiment E (2026-07-27): park forever with the bus in EXACTLY the
         * state stock has at flash 0x0802DA42 — prelude 05/12 already sent, CS
         * LOW, 0x15 not yet clocked — so SWD can dump the full peripheral state
         * and diff it against the stock spin image (which has `b .` patched in
         * at that same instruction). Feed the IWDG in the loop: our firmware
         * starts it late, but it survives a soft reset from a previous boot and
         * must not reset us mid-dump. */
        for (;;) { wdt_counter_reload(); }
#endif
        fpga.init_hs[7] = spi3_xfer(0x15);
        fpga.init_hs[8] = spi3_xfer(0x00);
        if (opt->prelude_frame_mode != 2) SPI3_CS_DEASSERT();
    }

    /* [3b] DIAGNOSTIC (probe_edit): read STATUS(0x41) at /256 IMMEDIATELY after
     * CONFIG_ENABLE — does 0x15 engage SYSTEM_EDIT_MODE (bit7)? Reads are only
     * valid at slow clock. Skipped in merge mode (CS held LOW into the upload).
     * Default off so the real attempt stays byte-unchanged. */
    if (opt->probe_edit && opt->prelude_frame_mode != 2) {
        spi3_set_br(7);                  /* /256 — valid SSPI read clock */
        spi3_xfer(0x00);                 /* bare clock, CS HIGH (frame) */
        SPI3_CS_ASSERT();
        spi3_xfer(0x41);
        spi3_xfer(0x00); spi3_xfer(0x00); spi3_xfer(0x00);
        for (unsigned i = 0; i < 4; i++)
            fpga.edit_mode_status[i] = spi3_xfer(0x00);
        SPI3_CS_DEASSERT();
        spi3_set_br(opt->cmd_br);        /* restore command clock */
    }

    /* Optional digest gap between CONFIG_ENABLE and the data stream (stock ~8µs
     * → default 0). Skipped in merge mode, where CS stays LOW into the upload. */
    if (opt->prelude_frame_mode != 2)
        fpga_scope_delay_ms(opt->pre_upload_gap_ms);

    /* [4] bitstream upload — 0x3B + full table. mode 2 continues the CS frame
     * opened by 15 00; modes 0/1 open a fresh CS frame here. */
    if (opt->prelude_frame_mode != 2) SPI3_CS_ASSERT();
    fpga.init_hs[10] = spi3_xfer(0x3B);  /* open upload */
    spi3_set_br(opt->upload_br);
    spi3_pump(fpga_h2_cal_table, NULL, FPGA_H2_CAL_TABLE_SIZE);
    spi3_set_br(opt->cmd_br);            /* back to command clock for close/status */
    /* Trailing clocks: Gowin runs the CRC-check / DONE / wakeup on CCLK cycles
     * AFTER the last config byte. Our sequence sent none; rosenrot00's working
     * 2C23T SPI loader clocks ~200 dummy 0x00 here. Stay inside the upload CS
     * frame. Default 0 = stock-faithful; sweep via `fpga reinit tcN`. */
    for (uint32_t i = 0; i < opt->trailing_clocks; i++)
        spi3_xfer(0x00);
    SPI3_CS_DEASSERT();
    fpga.h2_bytes_sent = FPGA_H2_CAL_TABLE_SIZE;
    fpga.h2_upload_done = 1;

    /* [5] 3A 00 — close/commit in its own CS-LOW frame. Stock → 0xF8. */
    SPI3_CS_ASSERT();
    spi3_xfer(0x3A);
    fpga.h2_close_status = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    /* [6] single 0x00 byte, CS LOW (stock flush frame at t=4.4484). */
    SPI3_CS_ASSERT();
    spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    /* [6b] Gowin STATUS_REGISTER read (opcode 0x41) — the authoritative config
     * status, which our sequence never read. Framed like rosenrot00's working
     * read_register32(0x41000000): a bare dummy byte (CS HIGH), then CS LOW,
     * opcode 0x41 + 3 pad bytes, then clock 4 bytes back. Decoded by the shell.
     * All-0xFF = FPGA not driving MISO (never entered config-receive) → config-
     * entry wall; CRC_ERROR/ID_VERIFY_FAILED set = bytes reached the engine →
     * wire/content problem. See sibling_loader_config_diff.md. */
    spi3_xfer(0x00);                          /* bare clock, CS HIGH (frame) */
    SPI3_CS_ASSERT();
    spi3_xfer(0x41);
    spi3_xfer(0x00); spi3_xfer(0x00); spi3_xfer(0x00);
    for (unsigned i = 0; i < 4; i++)
        fpga.cfg_status_reg[i] = spi3_xfer(0x00);
    SPI3_CS_DEASSERT();

    /* Step 7c: post-upload scope config (5 register writes + status read). */
    fpga_scope_delay_ms(opt->post_close_ms);

    static const uint8_t scope_cfg[][2] = {
        { 0x01, 0x08 }, { 0x02, 0x03 }, { 0x06, 0x00 },
        { 0x07, 0x00 }, { 0x08, 0xAD },
    };
    for (unsigned i = 0; i < sizeof(scope_cfg) / sizeof(scope_cfg[0]); i++) {
        SPI3_CS_ASSERT();
        spi3_xfer(scope_cfg[i][0]);
        spi3_xfer(scope_cfg[i][1]);
        SPI3_CS_DEASSERT();
    }

    SPI3_CS_ASSERT();
    spi3_xfer(0x03);                          /* status read */
    for (unsigned i = 0; i < 4; i++)
        fpga.scope_status[i] = spi3_xfer(0xFF);
    SPI3_CS_DEASSERT();

    spi3_set_br(0);                      /* restore /2 (60MHz) for normal acq */

    if (sched_running && acq_task_handle) vTaskResume(acq_task_handle);

    return fpga.h2_close_status;
}

/* ═══════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════ */

void fpga_init(void)
{
    /* Clear state */
    memset(&fpga, 0, sizeof(fpga));
    fpga_stock_diag_reset();

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

    /* GMUX remap — AT32-specific pin routing fabric.
     *
     * CRITICAL: on AT32 the GMUX overrides the legacy remap and its
     * SPI3 default routes SPI3 to PC10/11/12, NOT PB3/4/5. The legacy
     * SWJ_CFG=010 write above frees the JTAG pins but does NOT by itself
     * connect SPI3 to them. We MUST call SPI3_GMUX_0010 to route
     * SPI3 → PB3(SCK)/PB4(MISO)/PB5(MOSI)/PB6.
     *
     * Do NOT remove this based on "the stock decompilation never writes
     * SPI3_GMUX." Stock is a GD32 binary; the AT32 GMUX register block
     * does not exist in its world, so it CANNOT contain such a write —
     * its absence proves nothing about the AT32's needs. Bench-confirmed
     * 2026-04-06: SCK does not toggle on PB3 without this call. The HAL's
     * own JTAG-pin SPI3 example writes both SWJTAG_GMUX_010 and
     * SPI3_GMUX_0010. See memory feedback_at32_gmux + GitHub issue #11. */
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    gpio_pin_remap_config(SPI3_GMUX_0010, TRUE);  /* route SPI3 → PB3/PB4/PB5/PB6 */

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
#if FPGA_USART_SILENT_SCOPE
    /* Experiment: leave USART2 fully DISABLED (UEN clear) like stock's scope boot.
     * No RE/TE/RDBFIEN/UEN, no NVIC — zero USART2 traffic on PA2/PA3. */
    (void)0;
#else
    USART2->ctrl1 |= (1 << 2);   /* RE: Receiver enable */
    USART2->ctrl1 |= (1 << 3);   /* TE: Transmitter enable */
    USART2->ctrl1 |= (1 << 5);   /* RDBFIEN: RX interrupt enable */
    USART2->ctrl1 |= (1 << 13);  /* UEN: USART enable */

    /* Enable USART2 interrupt in NVIC */
    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_SetPriority(USART2_IRQn, 5);  /* Below FreeRTOS max syscall priority */
#endif

    /* ---------------------------------------------------------------
     * Step 3: Wait for FPGA to finish booting
     *
     * The stock firmware does ~2-3 seconds of LCD init, boot screen
     * animation (including a power-button-release wait loop), SPI flash
     * reads, and timer/FreeRTOS setup BEFORE touching SPI3. During all
     * that time, the FPGA is loading its bitstream from internal flash
     * and initializing its SPI slave.
     *
     * Our custom firmware reaches this point much faster (~500ms after
     * power-on). If we start SPI3 before the FPGA finishes booting,
     * the SPI slave won't be active yet → MISO stuck at 0xFF.
     *
     * Add an explicit delay to match the stock firmware's implicit
     * boot time. Try 2000ms as a conservative starting point.
     * --------------------------------------------------------------- */
#if FPGA_WARM_HANDOFF_TEST
    /* FPGA is already configured by stock — don't wait 2s (that long float
     * is what stopped capture in round 1). PC6/PB11/PC11 were driven to scope
     * posture at the very top of main(); just settle briefly. */
    systick_delay_ms(50);
#else
    systick_delay_ms(2000);
#endif

    /* ---------------------------------------------------------------
     * Step 3b: USART boot commands — sent BEFORE the SPI3 phase
     *
     * Stock-validated order: master init Phase 4 (inline USART cmds at
     * 0x08025D96) precedes the SPI3 phase (0x08026540). Moved here
     * 2026-06-10 after the framed-upload-only experiment left PC0
     * unarmed; the prior after-upload order came from the debunked
     * FUN_08027a50 reading (see docs/fpga_bitstream_replay_plan.md).
     * --------------------------------------------------------------- */
#if !FPGA_WARM_HANDOFF_TEST && !FPGA_USART_SILENT_SCOPE
    usart2_send_cmd(0x00, FPGA_CMD_INIT_01);  /* 0x01: Channel init */
    systick_delay_ms(50);
    usart2_send_cmd(0x00, FPGA_CMD_INIT_02);  /* 0x02: Signal gen setup */
    systick_delay_ms(50);
    usart2_send_cmd(0x00, FPGA_CMD_INIT_06);  /* 0x06: Signal gen setup */
    systick_delay_ms(50);
    usart2_send_cmd(0x00, FPGA_CMD_INIT_07);  /* 0x07: Meter probe detect */
    systick_delay_ms(50);
    usart2_send_cmd(0x00, FPGA_CMD_INIT_08);  /* 0x08: Meter configure */
    systick_delay_ms(100);
#endif  /* skip: would disturb stock-loaded config / perturb config-entry */

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

    /* PB4 = SPI3_MISO.
     * Exp E (2026-07-28) measured stock as input PULL-UP here (GPIOB CRL pin4
     * nibble 8 = CNF 10, with ODR bit4 set) while ours was input FLOATING
     * (nibble 4 = CNF 01). Stock's pull-up is exactly why an undriven MISO
     * reads 0xFF in the issue-#18 capture; a floating input has no defined
     * idle level, which makes every status byte we sample suspect. */
    gpio_cfg.gpio_pins = GPIO_PINS_4;
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
#if FPGA_STOCK_FIDELITY
    gpio_cfg.gpio_pull = GPIO_PULL_UP;
#else
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
#endif
    gpio_init(GPIOB, &gpio_cfg);
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;   /* restore shared struct default */

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

#if FPGA_STOCK_FIDELITY
    /* Experiment F: the two pins stock drives that Exp C never covered.
     * At the CONFIG_ENABLE instant the Exp E dump measured stock as
     *   PC2  : GPIOC CRL nibble 1 (output push-pull 10MHz), ODR bit2  = 1 -> HIGH
     *   PB12 : GPIOB CRH nibble 1 (output push-pull 10MHz), ODR bit12 = 1 -> HIGH
     * and our firmware as floating input (nibble 4) on both. The Exp C ablation
     * covered only the analog-frontend bank (PC12/PE4/PE5/PE6/PA15/PA10/PB10),
     * so neither of these is refuted. PB12 is plausibly SPI2 NSS = SPI-flash CS
     * (stock enables SPI2 and drives PB13/14/15 AF-PP) and therefore probably
     * benign, but that is unproven and it costs nothing to match. */
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;

    gpio_cfg.gpio_pins = GPIO_PINS_2;
    gpio_init(GPIOC, &gpio_cfg);
    GPIOC->scr = (1U << 2);    /* PC2 HIGH */

    gpio_cfg.gpio_pins = GPIO_PINS_12;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1U << 12);   /* PB12 HIGH */

#if FPGA_FIDELITY_DRIVE_PB9
    /* Stock has PB9 AF push-pull; level unknown (AF output is not in ODR).
     * Driving it HIGH as GPIO is a guess — sweep knob only. */
    gpio_cfg.gpio_pins = GPIO_PINS_9;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = (1U << 9);
#endif

    /* Stock sets APB1EN bit14 (SPI2) before the config sequence; we never did.
     * The peripheral itself is unused by us — this only matches the clock-gate
     * state so the enumerable diff closes to zero. */
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);
#endif  /* FPGA_STOCK_FIDELITY */

    /* PB11 = FPGA active mode — DO NOT configure as output yet!
     *
     * Stock firmware sets PB11 HIGH in "step 52" (just before
     * vTaskStartScheduler), but critically does NOT configure PB11
     * as a GPIO output before SPI3 init. On reset, PB11 defaults
     * to floating input. If the FPGA has an internal pull-up on its
     * PB11-connected pin, floating = HIGH = active mode.
     *
     * Previously we configured PB11 as output push-pull here, which
     * drives it LOW (GPIO output default). If the FPGA gates its
     * SPI slave interface on PB11, this would explain MISO stuck at
     * 0xFF and zero USART echo frames.
     *
     * PB11 gpio_init + set HIGH is deferred to Step 9b below. */

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
     * Step 5: SysTick delay
     * Stock firmware has ~100ms delay after SPI3 enable before
     * handshake. Previous value was 20ms total — too short.
     * --------------------------------------------------------------- */
    systick_delay_ms(100);

    /* ---------------------------------------------------------------
     * Step 6: SPI3 FPGA handshake — CS-framed commands (stock-faithful)
     *
     * GROUND-TRUTHED 2026-06-10 from arm-none-eabi-objdump of the raw
     * stock V1.2.0 binary. The sequence lives in master init
     * (FUN_08023A50) at 0x0802676E–0x08026D8x; the bulk-upload loop at
     * 0x08026B28 occurs exactly once in the image. Register tracking:
     *   r4 = 0x40011000 (GPIOC base), lr = 0xFFFFFC10, ip = 0xFFFFFC14
     *   [r4,lr] = 0x40010C10 = GPIOB_BSRR → PB6 HIGH = CS DEASSERT
     *   [r4,ip] = 0x40010C14 = GPIOB_BRR  → PB6 LOW  = CS ASSERT
     *
     * This INVERTS the CS polarity claimed by
     * SPI3_HANDSHAKE_BYTE_ACCURATE.md (which also mis-attributed the
     * code to FUN_08027a50 — that doc's warmup bursts and held-low CS
     * are not in the stock image). Stock frames EACH command in its own
     * CS-LOW window and clocks one dummy byte with CS HIGH in between:
     *
     *   CS↑ 00 | CS↓ 05 00 CS↑ 00 | ~100ms
     *          | CS↓ 12 00 CS↑ 00 | ~100ms
     *          | CS↓ 15 00 CS↑ 00 |
     *          | CS↓ 3B <115,638-byte bitstream> CS↑ 00
     *          | CS↓ 3A 00 CS↑ 00 | CS↓ 00 CS↑ 00
     *
     * The H2 blob is the Gowin FPGA bitstream, not a cal table — same
     * 0x3B/0x3A bracket, size ±1 byte, and 160-byte frame structure as
     * the working rosenrot00/OpenScope-2C23T HW4 loader. See
     * analysis_v120/h2_extracted/h2_is_gowin_bitstream_2c23t_evidence.md
     * and docs/fpga_bitstream_replay_plan.md.
     *
     * Every stock byte is a full-duplex polled exchange (wait TXE →
     * write → wait RXNE → read); spi3_xfer matches that exactly, and
     * spi3_pump is the gap-free equivalent for the bulk stream.
     * --------------------------------------------------------------- */

    /* The full PB11-arm → prelude → bitstream upload → close → scope-config
     * handshake now lives in fpga_spi3_config_sequence() so the debug shell
     * (`fpga reinit`) can replay it on demand for fast iteration without a
     * reflash. Parameters let us sweep the variables under investigation. */
#if FPGA_BUS_RELEASED_BOOT
    /* Boot-into-bus-released: hand SPI3 to an external master and stay off it.
     * Do NOT run the MCU's SSPI config upload — leave the FPGA's config port
     * pristine for an FT232H (JTAG TAP) or ESP32 (SSPI pads). fpga_bus_release()
     * tri-states PB3/PB5/PB6, stages PC6/PB11 HIGH, keeps PC9 power-hold, and the
     * acq task is gated off via fpga.bus_released. Mark init done and bail before
     * the meter-frontend routing + meter USART traffic (keep the wire quiet). */
    fpga.initialized = true;
    fpga_bus_release();
    return;
#elif FPGA_WARM_HANDOFF_TEST
    /* Warm-handoff test: the FPGA was already configured (scope) by stock
     * before the no-power-loss handoff. Do NOT run the SSPI config sequence —
     * it would attempt a reconfig the running design ignores anyway, and we
     * keep the wire quiet to be safe. Just arm PB11 (active mode) to match
     * stock's scope-run posture (PC6 is already HIGH from above), mark init
     * done, and bail before the meter-frontend routing + meter USART commands.
     * Read with `spi3 acqread`. */
    gpio_cfg.gpio_pins = GPIO_PINS_11;
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = PB11_MASK;   /* PB11 HIGH — FPGA active (match stock scope) */
    fpga.initialized = true;
    return;
#else
    fpga_spi3_config_sequence(&(fpga_cfg_seq_opts_t){
        .upload_br      = SPI3_UPLOAD_BR,
        .prelude_gap_ms = 100,
        .post_close_ms  = 600,
        .arm_pb11       = 1,
        /* 2026-07-27: the boot path had never set these two, so both took the
         * zero default — and both defaults were wrong.
         *
         * cmd_br = 7 (/256): the SSPI READ path is clock-limited (this file,
         * L1564: "IDCODE reads garbage at /2, clean at /256"). At the previous
         * default of 0 (/2, 60MHz) every status read — 0x3A close, the 0x03
         * scope status, and the 0x41 STATUS_REGISTER — was clocked in the known-
         * garbage domain, which is exactly what the bench showed (CL=FF instead
         * of stock's F8; SS/CFG returning rotations of a repeating 00 01 C8 10
         * pattern). Only the bulk 0x3B payload uses upload_br, so this does not
         * slow the 115KB upload.
         *
         * KNOWN TRADEOFF: stock clocks its prelude writes at /2, so /256 is a
         * deliberate divergence on the WRITE side. It is currently the right
         * call because stock never gates on the config status reply (it ignores
         * it entirely) whereas we depend on reading it, and switching br mid-CS-
         * frame would glitch the frame. Revisit if write timing is ever
         * implicated. Bench-checked 2026-07-27: prelude at /256 changed nothing
         * behaviourally, and turned CFG from garbage (8001C810) into the real
         * value (00039020).
         *
         * trailing_clocks: left at the stock-faithful 0. Gowin runs CRC-check /
         * DONE / wakeup on CCLK cycles after the last config byte and
         * rosenrot00's 2C23T loader clocks ~200 dummy bytes, so 256 was tried
         * on the bench 2026-07-27 — it did NOT produce DONE (status unchanged at
         * 00039020). Reverted rather than left as untested drift. */
#if FPGA_STOCK_FIDELITY
        /* Experiment F (2026-07-28): close the KNOWN TRADEOFF above. Exp E
         * measured stock's SPI3 CTRL1 as 0x347 (BR=0, /2) at the CONFIG_ENABLE
         * instant against our 0x37f (BR=7, /256), so the config frame now runs
         * at stock's /2 — and we do NOT lose the readout, because probe_edit
         * takes the 0x41 STATUS read at /256 in its own CS frame afterwards.
         * That is the best of both: stock-faithful writes, valid reads. */
        .cmd_br         = 0,
        .probe_edit     = 1,
#else
        .cmd_br         = 7,
#endif
    });
#endif

    /* USART boot commands (0x01,0x02,0x06,0x07,0x08) now sent in
     * Step 3b, BEFORE the SPI3 phase — stock-validated Phase 4 order. */

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
#if !FPGA_USART_SILENT_SCOPE
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
#endif  /* !FPGA_USART_SILENT_SCOPE — keep the wire quiet for the config test */

    /* Step 9b removed: PB11 is now armed immediately before the SPI3
     * handshake (stock-captured order, issue-#18 capture). */

    /* ---------------------------------------------------------------
     * Step 10: Post-init SPI3 probe
     * --------------------------------------------------------------- */
    systick_delay_ms(100);  /* Give FPGA time to settle */

    /* Test 1: SPI peripheral probe */
    SPI3_CS_ASSERT();
    fpga.init_hs[11] = spi3_xfer(0xFF);  /* Post-init probe byte */
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

#if FPGA_BUS_RELEASED_BOOT
    /* Bus-released boot: create NO auto-tasks — the MCU has handed SPI3 to an
     * external master and must not drive the bus or send USART traffic. */
    (void)tx_task_handle; (void)rx_task_handle; (void)acq_task_handle;
#elif FPGA_WARM_HANDOFF_TEST
    /* Warm-handoff test: create NO auto-tasks. The acquisition task uses the
     * old 0x80|range read and would both disturb the FPGA and compete with the
     * manual `spi3 acqread` probe; the meter poll/USART tasks could switch the
     * FPGA out of scope mode. We drive everything from the debug shell. */
    (void)tx_task_handle; (void)rx_task_handle; (void)acq_task_handle;
#elif FPGA_USART_SILENT_SCOPE
    /* USART-silent scope test: create NO USART/meter tasks (dvom_TX/dvom_RX/
     * meter_poll) — they are the ongoing PA2/PA3 traffic we're eliminating — and
     * no acquisition task (it would compete on the SPI3 bus). Drive `fpga reinit`
     * + `spi3 xfer` from the debug shell on a quiet wire. */
    (void)tx_task_handle; (void)rx_task_handle; (void)acq_task_handle;
#else
    /* Create tasks (stack sizes and priorities match stock firmware) */
    xTaskCreate(fpga_usart_tx_task,    "dvom_TX",   64,  NULL, 2, &tx_task_handle);
    xTaskCreate(fpga_usart_rx_task,    "dvom_RX",   128, NULL, 3, &rx_task_handle);
    xTaskCreate(fpga_acquisition_task, "fpga",      256, NULL, 3, &acq_task_handle);
    xTaskCreate(fpga_meter_poll_task,  "meter_poll", 64, NULL, 2, NULL);
#endif

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

BaseType_t fpga_trigger_scope_read(void)
{
    BaseType_t ok;

    if (spi3_acq_queue == NULL) return pdFALSE;

    if (fpga.acq_mode == (FPGA_ACQ_DUAL + 1)) {
        /* Stock firmware queues two back-to-back reads for the bulk/dual
         * acquisition path. Keep the first half explicit so the second
         * transfer has fresh data to consume. */
        ok = xQueueSend(spi3_acq_queue, &(uint8_t){FPGA_ACQ_NORMAL + 1}, 0);
        if (ok != pdTRUE) return ok;
        return xQueueSend(spi3_acq_queue, &(uint8_t){FPGA_ACQ_DUAL + 1}, 0);
    }

    return xQueueSend(spi3_acq_queue, (const void *)&fpga.acq_mode, 0);
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

/* ═══════════════════════════════════════════════════════════════════
 * SPI3 bus release — hand the bus to an external SSPI master (ESP32)
 * ═══════════════════════════════════════════════════════════════════
 *
 * EXPERIMENTAL — UNTESTED ON HARDWARE (2026-06-14). See the experimental/
 * esp32-bringup branch README (tools/esp32_sspi_bringup/).
 *
 * Purpose: make the MCU let go of the SPI3 lines (PB3 SCK, PB5 MOSI, PB6 CS)
 * so an external 3.3 V SPI master soldered to the back-side SPI3 test pads
 * (maksidze's #18 pad map) can drive the Gowin SSPI config interface itself —
 * at a controlled slow clock — WITHOUT bus contention, and without the
 * build → flash → pinhole-reset loop. The MCU keeps the board alive (PC9
 * power hold is untouched) and stages the FPGA enables, then yields the bus.
 *
 * Why this lets the ESP32 win the bus:
 *   - SPI3 is point-to-point (one master). Two masters driving SCK/MOSI/CS at
 *     once = contention = garbage. So we MUST tri-state our driven lines.
 *   - PB4 (MISO) is FPGA→MCU; it's already an input — the ESP32 also only
 *     reads it, so no contention there. Left as-is.
 *   - PB3 (SCK) and PB5 (MOSI) were AF push-pull outputs; PB6 (CS) was a GPIO
 *     output. All three become floating inputs (true Hi-Z) here.
 *
 * Staging held for the FPGA (not on the ESP32's 4 pads, so the MCU owns them):
 *   - PC6 = HIGH  (FPGA SPI enable)
 *   - PB11 = HIGH (FPGA active mode)
 *
 * The acquisition task checks fpga.bus_released and stays off the bus while
 * this is set (see fpga_acquisition_task). Re-flash to undo (no un-release
 * command on purpose — the bench operator power-cycles or reflashes). */
void fpga_bus_release(void)
{
    gpio_init_type gpio_cfg;

    /* Latch the flag first so the acq task bails before re-touching SPI3. */
    fpga.bus_released = true;
    fpga.spi3_active = false;

    /* Disable the SPI3 peripheral so it stops driving SCK/MOSI. */
    FPGA_SPI->ctrl1 &= ~(1u << 6);   /* SPE = 0 */

    /* Tri-state the MCU-driven SPI3 lines: PB3 (SCK), PB5 (MOSI), PB6 (CS).
     * Floating input = Hi-Z, so the ESP32 owns these nets. */
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_mode = GPIO_MODE_INPUT;
    gpio_cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_cfg.gpio_pins = GPIO_PINS_3 | GPIO_PINS_5 | GPIO_PINS_6;
    gpio_init(GPIOB, &gpio_cfg);

    /* PB4 (MISO) is already a floating input — leave it; both we and the
     * ESP32 only ever read it, the FPGA drives it. */

    /* Stage the FPGA enables the MCU still owns (not on the ESP32 pads). */
    gpio_default_para_init(&gpio_cfg);
    gpio_cfg.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_cfg.gpio_pins = GPIO_PINS_11;          /* PB11 = active mode */
    gpio_init(GPIOB, &gpio_cfg);
    GPIOB->scr = PB11_MASK;                      /* PB11 HIGH */

    gpio_cfg.gpio_pins = GPIO_PINS_6;            /* PC6 = SPI enable */
    gpio_init(GPIOC, &gpio_cfg);
    GPIOC->scr = PC6_MASK;                       /* PC6 HIGH */
}

void fpga_scope_reinit(void)
{
    const scope_state_t *ss;

    if (!fpga.initialized) return;

    ss = scope_state_get();

    /* Reset data_ready so display shows demo waveform until real data arrives */
    data_ready = false;

    /* Exit meter posture explicitly before sending scope-side commands. */
    GPIOC->clr = (1U << 11);  /* PC11 LOW — meter MUX off */
    GPIOB->scr = PB11_MASK;   /* PB11 HIGH — FPGA active */

    fpga_set_scope_frontend_range(fpga_scope_primary_range(ss));
    fpga_scope_delay_ms(10);
    fpga_send_scope_sequence(ss);
}

void fpga_request_scope_reinit(void)
{
    scope_reinit_pending = true;
}

bool fpga_service_requests(void)
{
    extern volatile device_mode_t current_mode;

    if (!scope_reinit_pending || !fpga.initialized) return false;
    if (current_mode != MODE_OSCILLOSCOPE) return false;

    scope_reinit_pending = false;
    fpga_scope_reinit();
    return true;
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

    fpga_stock_diag_seed_base2();
    fpga_scope_reinit();

    /* NOTE: Do NOT fire acquisition triggers here. The scope USART commands
     * take a few hundred milliseconds, and the display task already waits
     * before kicking the first acquisition. */
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
    if (fpga_probe_cmd_byte() == 0x07) {
        fpga_send_cmd(0x00, 0x07);  /* PC7 HIGH: probe detected */
    } else {
        fpga_send_cmd(0x00, FPGA_CMD_METER_NOPROBE);
    }
}

static void fpga_send_meter_mode_sequence(uint8_t submode)
{
    if (submode >= METER_SUBMODE_COUNT) submode = 0;

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

void fpga_set_meter_mode(uint8_t submode)
{
    if (!fpga.initialized) return;

    /* Stop DAC output if signal gen was running */
    {
        extern void dac_output_stop(void);
        extern bool dac_output_is_running(void);
        if (dac_output_is_running()) dac_output_stop();
    }

    fpga_set_meter_frontend_baseline();
    fpga_scope_delay_ms(10);
    fpga_send_meter_mode_sequence(submode);
}

void fpga_meter_reinit(uint8_t submode)
{
    if (!fpga.initialized) return;

    fpga_send_meter_wake_preamble();
    fpga_scope_delay_ms(10);
    fpga_send_meter_mode_sequence(submode);
    fpga_timed_send_cmd(0x00, FPGA_CMD_METER_START, 20);
}

void fpga_scope_wake(void)
{
    if (!fpga.initialized) return;

    fpga_send_meter_wake_preamble();
    fpga_scope_delay_ms(10);
    fpga_scope_reinit();
}

void fpga_send_raw_frame(const uint8_t *frame)
{
    usart2_send_frame(frame);
}
