/**
 * Gap Functions — Annotated Decompilation
 * FNIRSI 2C53T V1.2.0 Firmware
 *
 * This file covers the 17 gap functions identified in gap_functions.md.
 * These were functions missed by the initial call-graph trace — either
 * sitting in address gaps between mapped functions, or called only from
 * the master init (FUN_08023A50) which itself wasn't in full_decompile.c.
 *
 * Organization:
 *   P0 — Critical for hardware bring-up (2 needing annotation)
 *   P1 — Channel/trigger/meter functionality (5 functions)
 *   P2 — Full functionality (5 functions)
 *   P3 — Low priority (3 functions)
 *
 * Cross-references:
 *   - FUN_08023A50 (system_init): Already annotated in master_init_phase1-4.c
 *   - FUN_08039734 (usart_tx_config_writer): Already annotated in fpga_task_annotated.c
 *   - FUN_08037800: NOT a separate function — inner loop of spi3_acquisition_task
 *
 * Conventions:
 *   - FUN_0803acf0 = xQueueGenericSend (FreeRTOS)
 *   - FUN_0803b1d8 = xQueueReceive (FreeRTOS)
 *   - FUN_0803a390 = vTaskDelay (FreeRTOS)
 *   - METER_STATE_BASE = 0x200000F8 (global state structure)
 *   - "unaff_r7" in SPI functions = pointer to SPI3_STAT (0x40003C08)
 *   - "unaff_r9" in SPI functions = pointer to METER_STATE_BASE (0x200000F8)
 */

#include <stdint.h>

/*============================================================================
 * HARDWARE REGISTER DEFINITIONS (supplement to fpga_task_annotated.c)
 *============================================================================*/

/* Timer 2 registers (base 0x40000000) */
#define TIM2_CTL0       (*(volatile uint32_t *)0x40000000)
#define TIM2_SMCFG      (*(volatile uint32_t *)0x40000008)
#define TIM2_INTF       (*(volatile uint32_t *)0x40000010)
#define TIM2_SWEVG      (*(volatile uint32_t *)0x40000014)
#define TIM2_CNT        (*(volatile uint32_t *)0x40000024)
#define TIM2_PSC        (*(volatile uint32_t *)0x40000028)
#define TIM2_CAR        (*(volatile uint32_t *)0x4000002C)

/* Timer 5 registers (base 0x40000C00) */
#define TIM5_CTL0       (*(volatile uint32_t *)0x40000C00)
#define TIM5_SMCFG      (*(volatile uint32_t *)0x40000C08)
#define TIM5_INTF       (*(volatile uint32_t *)0x40000C10)
#define TIM5_SWEVG      (*(volatile uint32_t *)0x40000C14)
#define TIM5_CNT        (*(volatile uint32_t *)0x40000C24)
#define TIM5_PSC        (*(volatile uint32_t *)0x40000C28)
#define TIM5_CAR        (*(volatile uint32_t *)0x40000C2C)

/* Clock control (RCU/CRM) */
#define RCU_APB1EN      (*(volatile uint32_t *)0x4002101C)
#define RCU_APB2EN      (*(volatile uint32_t *)0x40021018)

/* SPI3 registers (base 0x40003C00) — same as fpga_task_annotated.c */
#define SPI3_STAT       (*(volatile uint32_t *)0x40003C08)
#define SPI3_DATA       (*(volatile uint32_t *)0x40003C0C)
#define SPI_STAT_TXE    (1 << 1)
#define SPI_STAT_RXNE   (1 << 0)

/* GPIO */
#define GPIOB_BOP       (*(volatile uint32_t *)0x40010C10)  /* Bit set */
#define GPIOB_BCR       (*(volatile uint32_t *)0x40010C14)  /* Bit clear */
#define GPIOC_IDR       (*(volatile uint32_t *)0x40011008)
#define PB6_MASK        0x40  /* SPI3 chip select */

/* USART2 */
#define USART2_CTRL1    (*(volatile uint32_t *)0x4000440C)

/* Watchdog */
#define IWDG_KR         (*(volatile uint32_t *)0x40003000)

/* FreeRTOS queue handles */
#define USART_TX_QUEUE  (*(void **)0x20002D74)
#define SPI3_DATA_QUEUE (*(void **)0x20002D78)
#define USART_CMD_QUEUE (*(void **)0x20002D6C)

/* USART TX buffer */
#define USART_TX_BUF    ((volatile uint8_t *)0x20000005)
#define USART_TX_INDEX  (*(volatile uint8_t *)0x2000000F)

/* Global state structure */
#define STATE_BASE      ((volatile uint8_t *)0x200000F8)


/*============================================================================
 *
 *  P0 — CRITICAL PATH FUNCTIONS
 *
 *============================================================================*/


/*============================================================================
 * FUNCTION: fpga_usart_tx_task  (dvom_TX FreeRTOS task)
 * Address: 0x08037400
 * Size: ~60 bytes (Ghidra reports 1 due to computed entry)
 * Priority: P0
 *
 * This is the dvom_TX FreeRTOS task — the USART2 transmit side.
 * It runs as an infinite loop, blocking on the USART TX queue until
 * a 2-byte command item arrives. When received, it:
 *
 *   1. Resets the TX byte index to 0 (starting a new frame)
 *   2. Copies the command byte into TX buffer position [3] (the command slot)
 *   3. Copies the parameter byte into TX buffer position [2] (the param slot)
 *   4. Computes checksum: buf[9] = buf[2] + buf[3]
 *   5. Sets bit 7 of the TMR2 control register (0x40000000) — this is
 *      TMR2_CTL0 bit 7 = ARPE (auto-reload preload enable), but more
 *      likely this is used as a trigger signal to kick the USART TX
 *      exchange cycle via the timer interrupt chain
 *   6. Delays 10 ticks (~10ms) to allow the USART ISR to clock out
 *      all 10 bytes before accepting the next command
 *
 * The TX frame format (10 bytes, built elsewhere, modified here):
 *   [0] = 0x55  (header byte 1, set at init)
 *   [1] = 0xAA  (header byte 2, set at init)
 *   [2] = parameter byte  ← written here
 *   [3] = command byte    ← written here
 *   [4..8] = 0x00 (padding, set at init)
 *   [9] = checksum (buf[2] + buf[3])  ← computed here
 *
 * Queue item format: 2 bytes packed into a uint16_t
 *   Low byte  = command code (FPGA command 0x00-0x2C)
 *   High byte = parameter value
 *
 * IMPORTANT: This task is the ONLY writer to the USART TX buffer.
 * The TMR3 ISR reads from the buffer and clocks bytes out via USART2.
 * The 10ms delay ensures one complete frame is transmitted before
 * the next command overwrites the buffer.
 *
 * Callers: Created by system_init (FUN_08023A50) as FreeRTOS task
 * Callees: xQueueReceive, vTaskDelay
 *============================================================================*/
void fpga_usart_tx_task(void *pvParameters) {
    /* r6, r7 point to queue handle and TX buffer base via register setup
     * from the task creation context. Ghidra shows:
     *   unaff_r6 & 0xFFFF | 0x20000000 → pointer to USART_TX_QUEUE (0x20002D74)
     *   unaff_r7 & 0xFFFF | 0x20000000 → pointer to USART_TX_BUF area (0x20000005)
     */
    uint16_t queue_item;

    for (;;) {
        /* Block forever waiting for a command on the USART TX queue.
         * Queue items are 2 bytes: [cmd_byte, param_byte].
         * Returns pdTRUE (1) on success — loop retries on failure. */
        while (xQueueReceive(USART_TX_QUEUE, &queue_item, portMAX_DELAY) != pdTRUE);

        /* Reset TX byte index — the USART ISR uses this to track
         * which byte of the 10-byte frame to send next */
        USART_TX_INDEX = 0;  /* DAT_2000000f = 0 */

        /* Extract command and parameter from the queue item.
         * Ghidra shows byte manipulation of in_stack_00000004:
         *   buf[3] = low byte of queue_item   (command code)
         *   buf[2] = high byte of queue_item  (parameter)
         */
        uint8_t cmd_byte = (uint8_t)(queue_item & 0xFF);
        uint8_t param_byte = (uint8_t)(queue_item >> 8);

        USART_TX_BUF[3] = cmd_byte;    /* *(uVar3 + 3) = low byte */
        USART_TX_BUF[2] = param_byte;  /* *(uVar3 + 2) = high byte */

        /* Compute simple checksum: sum of cmd + param bytes */
        USART_TX_BUF[9] = cmd_byte + param_byte;  /* *(uVar3 + 9) */

        /* Kick the USART transmit cycle.
         * uRam40000000 |= 0x80 sets bit 7 of TMR2_CTL0.
         * This is the ARPE bit, but in context it triggers the
         * timer-driven USART TX sequence. TMR3 ISR will clock out
         * the 10 frame bytes one at a time. */
        TIM2_CTL0 |= 0x80;

        /* Wait 10 ticks for the frame to be fully transmitted.
         * At 9600 baud, 10 bytes = ~10.4ms. The 10-tick delay
         * (at 1000Hz tick rate = 10ms) is just enough. */
        vTaskDelay(10);
    }
}


/*============================================================================
 * FUNCTION: spi3_acquisition_inner_loop
 * Address: 0x08037800 (within spi3_acquisition_task at 0x08037454)
 * Size: 472 bytes (Ghidra entry point)
 * Priority: P0
 *
 * NOTE: This is NOT a separate function. It is the inner acquisition loop
 * of spi3_acquisition_task (FUN_08037454, 7164 bytes total). Ghidra created
 * a separate entry because it couldn't trace a computed branch back to the
 * outer function's switch statement.
 *
 * This code implements the per-trigger acquisition cycle:
 *
 * ENTRY (from spi3_acquisition_task's main loop):
 *   - A trigger event has been received via SPI3_DATA_QUEUE
 *   - The FPGA has indicated data is ready
 *
 * PHASE 1: Read 2-byte trigger position (lines 28638-28710)
 *   - Assert PB6 LOW (CS active): GPIOB_BOP = 0x40 [sic — BOP sets HIGH]
 *     Wait... looking more carefully:
 *       _DAT_40010c10 = 0x40  → GPIOB_BOP = PB6 → sets PB6 HIGH (CS deassert)
 *       Then delay(1)
 *       _DAT_40010c14 = 0x40  → GPIOB_BCR = PB6 → clears PB6 LOW (CS assert)
 *     So the sequence is: deassert CS briefly, delay, then assert CS.
 *     This is a CS toggle/reset before starting the transfer.
 *   - Send command byte 0x0A via SPI3, read response (trigger position MSB)
 *   - Send 0xFF dummy byte, read trigger position LSB
 *   - Combine: trigger_pos = (MSB << 8) | LSB → stored at state[0x46]
 *
 * PHASE 2: Wait for SPI3 data trigger (line 28716)
 *   - Deassert CS (PB6 HIGH via BOP)
 *   - Block on SPI3_DATA_QUEUE with portMAX_DELAY
 *   - This queue is signaled by the FPGA (via interrupt or polling)
 *     when ADC sample data is ready to be read
 *
 * PHASE 3: VFP Calibration of trigger position (lines 28721-28751)
 *   - Only if CH1 AND CH2 calibration data are valid:
 *       state[0x352] != 0 AND != 0xFF  (CH1 cal present)
 *       state[0x353] != 0 AND != 0xFF  (CH2 cal present)
 *       state[0x33] == 0               (cal not disabled)
 *   - Calibration formula (VFP floating-point):
 *       cal_factor = (float)(state[0x352]) + s16) / s28
 *       raw = (float)(trigger_pos - offset)
 *       offset_f = (float)(state[channel_base + 4])  (per-channel ADC offset)
 *       calibrated = cal_factor * raw + offset_f + s22
 *   - Result clamped to [0, s24] range (s24 = max display value, likely 255.0)
 *     If < 0: clamp to s26 (likely 0.0)
 *     If > s24: clamp to s24
 *
 * PHASE 4: Per-mode switch statement (lines 28778+)
 *   The queue item carries a mode byte that selects the acquisition type:
 *
 *   case 1: NORMAL ACQUISITION — Auto-range + single SPI3 read
 *     - Checks timebase against auto-range table at DAT_0804d833
 *     - If sample count < threshold: send current timebase index via SPI3
 *     - If sample count >= threshold AND timebase < 0x13: increment timebase
 *     - If timebase >= 0x13 (max): send fixed value 0x12
 *     - Sends via SPI3: wait TXE, write byte, wait RXNE, read response
 *
 *   case 2: DUAL CHANNEL ACQUISITION — Mode select
 *     - If state[0x14] == 0: send mode byte 3 (single-channel fast)
 *     - If state[0x14] != 0: send state[0x14] value (dual-channel mode)
 *
 *   case 3: ROLL MODE — Circular buffer with per-sample calibration
 *     - Manages a circular buffer at state[0xDB4..0xDB6]:
 *       state[0xDB4] = write position (16-bit)
 *       state[0xDB6] = buffer fill level (16-bit, max 0x12D = 301)
 *     - Decrements write position, increments fill level
 *     - When buffer is full (0x12D entries): shifts data left by 1
 *       Copies state[base+0x12D+n] ← state[base+0x12E+n] for CH1
 *       Copies state[base+0x25A+n] ← state[base+0x25B+n] for CH2
 *     - Reads 4 bytes from SPI3 (2x dummy 0xFF reads):
 *       Byte 1,2 = dummy reads (0xFF) → CH1 raw sample
 *       Byte 3,4 = dummy reads (0xFF) → CH2 raw sample
 *     - Stores raw bytes at state[0x482] (CH1) and state[0x5AF] (CH2)
 *     - Applies VFP calibration pipeline to BOTH channels:
 *       For each channel where cal data is valid (not 0, not 0xFF):
 *         If raw > 0xDC (220): apply full calibration
 *         cal_value = ((raw_cal + s20) - offset) / ((gain + s16) / s18) + s22 + offset
 *         Clamp to [s26, s24] range
 *     - Trigger detection: when buffer full AND state[0x17] != 1:
 *       Checks trigger crossing against thresholds at state[0x3EB]/[0x3EC]
 *       If triggered: copies buffer to secondary display buffer at state[0x5B0]
 *       Sends trigger command (byte 2) to USART CMD queue
 *
 *   case 4: BULK SPI3 READ — Full 1024-byte (512 pairs) acquisition
 *     - Sends initial 0xFF dummy read
 *     - Loops 1024 times (0x400), reading 2 bytes per iteration:
 *       Send 0xFF, read CH1 sample → state[0x5B0 + i*2]
 *       Send 0xFF, read CH2 sample → state[0x5B0 + i*2 + 1]
 *     - After reading all data:
 *       If calibration valid AND (dual mode OR timebase > 3):
 *         Applies unrolled VFP calibration to all 1024 samples, 8 at a time
 *         Each sample: calibrated = ((raw + s20) - offset) / cal_factor + s22 + offset
 *         Clamped to [s26, s24]
 *     - For triggered mode (state[0x17] == 2):
 *       Applies moving average filter with circular buffer wrapping
 *       Handles both normal (< 1024) and wrapped (>= 1024) index cases
 *
 * VFP register usage (callee-saved, set up by spi3_acquisition_task):
 *   s16 = CH1 gain calibration offset (from cal table)
 *   s18 = CH2 gain calibration factor
 *   s20 = ADC offset constant (-28.0)
 *   s22 = Display center offset (likely 128.0 for 8-bit centered display)
 *   s24 = Max clamp value (likely 255.0)
 *   s26 = Min clamp value (likely 0.0)
 *   s28 = Gain normalization factor
 *
 * SPI3 transfer pattern (appears 22+ times in this function):
 *   The Ghidra output shows this repeating pattern:
 *     (*unaff_r7 << 0x1e) < 0   →  check SPI3_STAT bit 1 (TXE)
 *     Three attempts (IT block optimization), then poll loop
 *     unaff_r7[1] = byte         →  write to SPI3_DATA (STAT+4)
 *     (*unaff_r7 & 1) == 0       →  check SPI3_STAT bit 0 (RXNE)
 *     Three attempts, then poll loop
 *     result = unaff_r7[1]       →  read from SPI3_DATA
 *
 *   In clean C this is just:
 *     while (!(SPI3_STAT & SPI_STAT_TXE));
 *     SPI3_DATA = tx_byte;
 *     while (!(SPI3_STAT & SPI_STAT_RXNE));
 *     rx_byte = SPI3_DATA;
 *============================================================================*/

/* This is documentation only — the actual code lives inside
 * spi3_acquisition_task. See fpga_task_annotated.c FUNCTION 6.
 *
 * Pseudo-code for the inner loop: */

void spi3_acquisition_inner_loop(uint8_t mode, uint8_t *state) {
    /* --- CS toggle: deassert then reassert --- */
    GPIOB_BOP = PB6_MASK;      /* PB6 HIGH = CS deassert */
    vTaskDelay(1);
    GPIOB_BCR = PB6_MASK;      /* PB6 LOW = CS assert */

    /* --- Read 2-byte trigger position via SPI3 --- */
    uint16_t trig_pos;
    trig_pos = spi3_xfer(0x0A);        /* Command byte → MSB response */
    trig_pos <<= 8;
    trig_pos |= spi3_xfer(0xFF);       /* Dummy → LSB response */
    state[0x46] = trig_pos;             /* Save trigger position */

    /* --- Block until FPGA signals data ready --- */
    GPIOB_BOP = PB6_MASK;              /* CS deassert */
    uint32_t queue_item;
    xQueueReceive(SPI3_DATA_QUEUE, &queue_item, portMAX_DELAY);

    /* --- Apply VFP calibration to trigger position (if cal valid) --- */
    uint8_t ch1_cal = state[0x352];
    uint8_t ch2_cal = state[0x353];
    if (ch1_cal != 0 && ch1_cal != 0xFF &&
        ch2_cal != 0 && ch2_cal != 0xFF &&
        state[0x33] == 0) {
        /* Full VFP calibration pipeline — see CALIBRATION.md */
        float gain = (float)ch1_cal;
        float cal_factor = (gain + VFP_S16) / VFP_S28;
        float raw = (float)(trig_pos - (int8_t)state[state[0x16] + 4]);
        float calibrated = cal_factor * raw + (float)(int8_t)state[state[0x16] + 4] + VFP_S22;
        /* Clamp to display range */
        if (calibrated > VFP_S24) calibrated = VFP_S24;
        if (calibrated < 0.0f)    calibrated = VFP_S26;
        trig_pos = (uint16_t)(int)calibrated;
    }

    /* --- Per-mode acquisition --- */
    switch (mode) {
    case 1: /* Normal: auto-range + single read */
        normal_acquisition(state);
        break;
    case 2: /* Dual: mode select */
        dual_channel_select(state);
        break;
    case 3: /* Roll: circular buffer + per-sample cal */
        roll_mode_acquisition(state);
        break;
    case 4: /* Bulk: full 1024-byte SPI3 read + batch cal */
        bulk_spi3_acquisition(state);
        break;
    }
}


/*============================================================================
 *
 *  P1 — CHANNEL / TRIGGER / METER FUNCTIONALITY
 *
 *============================================================================*/


/*============================================================================
 * FUNCTION: fpga_cmd_mode_dispatcher (jump table)
 * Address: 0x0800B900
 * Size: ~8 bytes (just the dispatcher)
 * Priority: P1
 *
 * This is a jump table dispatcher that Ghidra couldn't recover.
 * It dispatches to the FPGA command builder functions at
 * 0x0800BA06-0x0800BC98 based on the current meter/probe mode.
 *
 * The TBB (table branch byte) instruction at 0x0800B904 indexes into
 * a table of offsets. Each entry jumps to one of the command builders:
 *
 *   Index 0 → FUN_0800ba06 (fpga_cmd_set_channel_ranges)
 *   Index 1 → FUN_0800bb10 (fpga_cmd_set_trigger)
 *   Index 2 → FUN_0800bba6 (fpga_cmd_set_acquisition_mode)
 *   Index 3 → FUN_0800bc00 (fpga_cmd_set_timebase)
 *   Index 4 → FUN_0800bc98 (fpga_cmd_set_probe_atten)
 *   (possibly more entries)
 *
 * Called from: scope_main_fsm, siggen_configure, meter handler,
 *             FreeRTOS timer callbacks, and system_init
 *
 * The param_1 argument selects which entry in the jump table.
 *============================================================================*/
void fpga_cmd_mode_dispatcher(uint8_t mode_index) {
    /* TBB [PC, mode_index] — computed branch via byte offset table
     * Ghidra: WARNING: Could not recover jumptable at 0x0800b904 */

    /* Dispatches to one of the command builders below based on mode_index.
     * Each builder queues a sequence of FPGA commands to the USART TX queue.
     * The USART TX task (fpga_usart_tx_task at 0x08037400) then
     * serializes them into 10-byte frames and transmits via USART2. */
    switch (mode_index) {
    case 0: fpga_cmd_set_channel_ranges(); break;
    case 1: fpga_cmd_set_trigger();        break;
    case 2: fpga_cmd_set_acquisition_mode(); break;
    case 3: fpga_cmd_set_timebase();       break;
    case 4: fpga_cmd_set_probe_atten();    break;
    }
}


/*============================================================================
 * FUNCTION: fpga_cmd_set_channel_ranges
 * Address: 0x0800BA06
 * Size: 102 bytes
 * Priority: P1
 *
 * Queues 6 FPGA commands to configure CH1 and CH2 input ranges.
 * This is called when the user changes voltage range, coupling,
 * or bandwidth limit settings.
 *
 * Queue item format: Each command is a byte on the stack at offset 7.
 * The unaff_r5 register points to a structure containing the queue handle
 * and the parameter data. The stack variable uStack00000007 is the
 * FPGA command code.
 *
 * Command sequence:
 *   1. Prefix: 0x07 (if param negative) or 0x0A (if param >= 0)
 *      - This selects which channel bank the following commands affect
 *      - 0x07 = CH1 prefix, 0x0A = CH2 prefix
 *      - The sign test: *param_1 << 0x18 tests bit 7 of the first byte
 *   2. 0x1A = CH1 gain setting
 *   3. 0x1B = CH1 offset setting
 *   4. 0x1C = CH2 gain setting
 *   5. 0x1D = CH2 offset setting
 *   6. 0x1E = Coupling/BW limit (FINAL — sent with portMAX_DELAY)
 *
 * The last command uses portMAX_DELAY (0xFFFFFFFF) to block until the
 * queue accepts it. Earlier commands use a shorter timeout (0), meaning
 * they are fire-and-forget. This ensures the complete sequence is
 * committed atomically — the task won't continue until all 6 are queued.
 *
 * Callees: xQueueGenericSend (6 calls)
 *============================================================================*/
void fpga_cmd_set_channel_ranges(int8_t *channel_params) {
    uint8_t cmd;

    /* Select channel bank prefix based on sign of first parameter */
    if (channel_params[0] >= 0) {
        cmd = 0x0A;  /* CH2 bank (or positive range) */
    } else {
        cmd = 0x07;  /* CH1 bank (or negative range) */
    }
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* CH1 gain */
    cmd = 0x1A;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* CH1 offset */
    cmd = 0x1B;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* CH2 gain */
    cmd = 0x1C;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* CH2 offset */
    cmd = 0x1D;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Coupling / bandwidth limit — BLOCKING send */
    cmd = 0x1E;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, portMAX_DELAY, 0);
}


/*============================================================================
 * FUNCTION: fpga_cmd_set_trigger
 * Address: 0x0800BB10
 * Size: 84 bytes
 * Priority: P1
 *
 * Queues 5 FPGA commands to configure the trigger system.
 * Called when the user changes trigger level, edge, mode, or holdoff.
 *
 * Command sequence:
 *   1. Prefix: 0x07 or 0x0A (channel select, same logic as channel ranges)
 *   2. 0x16 = Trigger threshold LSB
 *   3. 0x17 = Trigger threshold MSB
 *   4. 0x18 = Trigger mode/edge selection
 *   5. 0x19 = Trigger holdoff (FINAL — blocking send)
 *
 * The trigger level is a 16-bit value split across commands 0x16/0x17.
 * The mode byte at 0x18 encodes edge polarity and trigger source.
 * Command 0x19 (holdoff) is the last in the sequence and blocks.
 *
 * Callees: xQueueGenericSend (5 calls)
 *============================================================================*/
void fpga_cmd_set_trigger(int8_t *trigger_params) {
    uint8_t cmd;

    /* Channel prefix */
    cmd = (trigger_params[0] >= 0) ? 0x0A : 0x07;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Trigger threshold LSB */
    cmd = 0x16;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Trigger threshold MSB */
    cmd = 0x17;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Trigger mode / edge */
    cmd = 0x18;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Trigger holdoff — BLOCKING */
    cmd = 0x19;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, portMAX_DELAY, 0);
}


/*============================================================================
 * FUNCTION: calibration_loader
 * Address: 0x08001830
 * Size: ~100 bytes
 * Priority: P1
 *
 * Loads 301 bytes (0x12D) of per-channel calibration data.
 * Called twice from system_init:
 *
 *   Call 1: calibration_loader(state + 0x356, 0x12D, state[4] ^ 0x80)
 *           → Loads CH1 calibration into state[0x356..0x482]
 *
 *   Call 2: calibration_loader(state + 0x483, 0x12D, state[5] ^ 0x80)
 *           → Loads CH2 calibration into state[0x483..0x5AF]
 *
 * From the disassembly at 0x080271A8:
 *   r0 = sl + 0x356  (destination buffer)
 *   r1 = 0x12D       (301 bytes)
 *   r2 = state[4] ^ 0x80  (XOR with 0x80 converts signed↔unsigned offset)
 *
 * The XOR 0x80 operation converts the stored signed ADC offset into an
 * unsigned lookup index. The calibration data is a 301-entry table that
 * maps raw ADC values to calibrated values, accounting for gain and
 * offset errors in the analog frontend.
 *
 * 301 = 0x12D bytes covers the usable ADC range. The data likely comes
 * from SPI flash (loaded during the FPGA calibration exchange at boot).
 *
 * Called from: system_init (FUN_08023A50) — init-only function
 *============================================================================*/
void calibration_loader(uint8_t *dest, uint16_t count, uint8_t offset_key) {
    /* Loads 'count' bytes of calibration data into 'dest'.
     * The offset_key selects which calibration set to load
     * (derived from the per-channel ADC offset XOR'd with 0x80).
     *
     * Internal implementation likely reads from a calibration table
     * in RAM that was previously loaded from SPI flash during the
     * 411-byte calibration exchange (commands 0x3B/0x3A). */
    for (uint16_t i = 0; i < count; i++) {
        dest[i] = cal_table_lookup(offset_key, i);
    }
}


/*============================================================================
 * FUNCTION: analog_input_init
 * Address: 0x08002C78
 * Size: ~200 bytes
 * Priority: P1
 *
 * Initializes the analog frontend based on probe detection state.
 * Called once from system_init at 0x080274EE.
 *
 * Context from the disassembly (0x080274E6):
 *   - Just before the call, the code checks GPIOC_IDR (0x40011008) bit 8
 *     (PC8 = POWER button, used here as a probe detect line?)
 *   - The branch at 0x080274E6 is BMI (branch if minus), meaning it
 *     checks the sign bit after shifting left by 0x17 (bit 8 position)
 *   - If PC8 is low: call FUN_08002c78
 *   - If PC8 is high: skip to alternate init path
 *
 * After this function returns, the init code:
 *   1. Enables USART clock: RCU_APB2EN |= 0x4000
 *   2. Enables GPIOA clock: RCU_APB2EN |= 0x4
 *   3. Configures GPIOA pin 9 (PA9) as alternate function push-pull
 *      (0x01080400 mode with pin 0x200) — this is likely USART1_TX
 *   4. Calls timer_init for ADC sampling timer setup
 *   5. Configures ADC prescaler based on a division/modulo calculation
 *
 * This function likely configures:
 *   - Input relay states (PC12, PE4/PE5/PE6) for the default range
 *   - Gain resistor selection (PA15, PA10, PB10)
 *   - AC/DC coupling relay position
 *   - Probe detection interrupt setup
 *
 * Called from: system_init (FUN_08023A50)
 *============================================================================*/
void analog_input_init(void) {
    /* Configures the analog input stage based on detected probe type
     * and saved settings. Sets relay states, gain paths, and coupling
     * mode to match the last-used configuration (loaded from SPI flash
     * settings during earlier init phases). */
}


/*============================================================================
 *
 *  P2 — FULL FUNCTIONALITY
 *
 *============================================================================*/


/*============================================================================
 * FUNCTION: fpga_cmd_set_acquisition_mode
 * Address: 0x0800BBA6
 * Size: 24 bytes
 * Priority: P2
 *
 * Queues 2 FPGA commands to set the acquisition run mode and sample depth.
 * Called when switching between single-shot, normal, and auto trigger modes.
 *
 * Command sequence:
 *   1. 0x20 = Run mode (0=stop, 1=single, 2=normal, 3=auto)
 *   2. 0x21 = Sample depth (FINAL — blocking send)
 *
 * This is the simplest command builder — only 2 commands, no channel
 * prefix needed because acquisition mode is global (not per-channel).
 *
 * Callees: xQueueGenericSend (2 calls)
 *============================================================================*/
void fpga_cmd_set_acquisition_mode(void) {
    uint8_t cmd;

    /* Run mode */
    cmd = 0x20;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Sample depth — BLOCKING */
    cmd = 0x21;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, portMAX_DELAY, 0);
}


/*============================================================================
 * FUNCTION: fpga_cmd_set_timebase
 * Address: 0x0800BC00
 * Size: 42 bytes
 * Priority: P2
 *
 * Queues 3 FPGA commands to configure the timebase (sample rate).
 * Called when the user rotates the timebase knob or changes time/div.
 *
 * Command sequence:
 *   1. 0x26 = Timebase prescaler (divides the FPGA's ADC sample clock)
 *   2. 0x27 = Timebase period (number of samples per trigger window)
 *   3. 0x28 = Timebase mode (FINAL — blocking send)
 *
 * The prescaler and period together define the effective sample rate.
 * The mode byte selects between normal, equivalent-time, and
 * interleaved sampling strategies in the FPGA.
 *
 * Callees: xQueueGenericSend (3 calls)
 *============================================================================*/
void fpga_cmd_set_timebase(void) {
    uint8_t cmd;

    /* Prescaler */
    cmd = 0x26;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Period */
    cmd = 0x27;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, 0, 0);

    /* Mode — BLOCKING */
    cmd = 0x28;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, portMAX_DELAY, 0);
}


/*============================================================================
 * FUNCTION: fpga_cmd_set_probe_atten
 * Address: 0x0800BC98
 * Size: 36 bytes
 * Priority: P2
 *
 * Queues 1 FPGA command to set the probe attenuation factor.
 * Called when the user switches between 1x and 10x probe settings.
 *
 * The channel prefix (0x07 or 0x0A) is the ONLY command sent.
 * The attenuation is encoded in the prefix choice:
 *   0x07 = 1x probe (or CH1 bank selection)
 *   0x0A = 10x probe (or CH2 bank selection)
 *
 * This is a single blocking send — the simplest possible command.
 *
 * Callees: xQueueGenericSend (1 call)
 *============================================================================*/
void fpga_cmd_set_probe_atten(int8_t *probe_params) {
    uint8_t cmd;

    /* Channel/probe prefix based on parameter sign */
    cmd = (probe_params[0] >= 0) ? 0x0A : 0x07;
    xQueueGenericSend(USART_TX_QUEUE, &cmd, portMAX_DELAY, 0);
}


/*============================================================================
 * FUNCTION: lcd_palette_init
 * Address: 0x080022DC
 * Size: ~200 bytes
 * Priority: P2
 *
 * Called twice from system_init (at 0x08024558 and 0x08024D48).
 * Likely initializes LCD color lookup tables or palette data.
 *
 * From the gap_functions.md description: "Called 2x during init with
 * RGB565 values derived from bit manipulation, near 0x0804CA24
 * (font/image data pointer)."
 *
 * The two calls may initialize:
 *   1. The foreground text color palette (theme-dependent)
 *   2. The background/grid color palette
 *
 * Or they may load two different display regions with initial content.
 * The proximity to font/image data (0x0804CA24) suggests this writes
 * pre-rendered UI elements or color gradient tables to display RAM.
 *
 * Called from: system_init (FUN_08023A50)
 *============================================================================*/
void lcd_palette_init(uint16_t *rgb565_data) {
    /* Initializes LCD color palette or pre-rendered display elements.
     * Exact implementation requires SPI flash dump or Ghidra analysis
     * of the function body (not in full_decompile.c, init-only). */
}


/*============================================================================
 * FUNCTION: lcd_window_setup
 * Address: 0x08022D40
 * Size: ~200 bytes
 * Priority: P2
 *
 * Called once from system_init at 0x08027288, immediately after
 * FUN_08022e14 (a 2.9KB UI helper function).
 *
 * Sets up the LCD display window/region configuration. This likely
 * sends the ST7789V window address commands:
 *   CASET (0x2A) — Column Address Set
 *   RASET (0x2B) — Row Address Set
 *   RAMWR (0x2C) — Memory Write
 *
 * This establishes the default drawing region (likely 0,0 → 319,239
 * for the full 320x240 display) before the first frame is rendered.
 *
 * Context: Called right after a UI drawing helper, suggesting it
 * finalizes the display configuration after initial UI content is
 * prepared in the framebuffer.
 *
 * Called from: system_init (FUN_08023A50)
 *============================================================================*/
void lcd_window_setup(void) {
    /* Sets LCD display window region via ST7789V commands.
     * Implementation in init-only code — body not in full_decompile.c. */
}


/*============================================================================
 *
 *  P3 — LOW PRIORITY
 *
 *============================================================================*/


/*============================================================================
 * FUNCTION: queue_peek_ready
 * Address: 0x08034DD0
 * Size: ~64 bytes
 * Priority: P3
 *
 * A small FreeRTOS utility that checks if a queue has items ready.
 *
 * From the Ghidra output:
 *   - Reads DAT_200062f0 (likely pxCurrentTCB or a queue handle)
 *   - Checks if the first word is 0 (queue empty)
 *   - If empty: sets *param_1 = 1 (true), returns 0 (NULL)
 *   - If not empty: sets *param_1 = 0 (false), returns the data
 *     from offset [3] of the queue structure (the first item)
 *
 * This is a non-blocking peek operation — it checks if data is
 * available without removing it from the queue. Used internally
 * by FreeRTOS or by application code that needs to poll.
 *
 * Equivalent to xQueuePeek with zero timeout, but returns the
 * item pointer directly instead of copying.
 *============================================================================*/
void *queue_peek_ready(uint32_t *is_empty) {
    volatile uint32_t *queue_state = (volatile uint32_t *)0x200062F0;

    /* Read the queue state pointer */
    uint32_t *queue_data = (uint32_t *)*queue_state;

    if (*queue_data == 0) {
        /* Queue is empty */
        *is_empty = 1;
        return NULL;
    } else {
        /* Queue has data — return pointer to first item */
        *is_empty = 0;
        return (void *)queue_data[3];
    }
}


/*============================================================================
 * FUNCTION: adc_prescaler_init
 * Address: 0x0800039C
 * Size: ~24 bytes
 * Priority: P3
 *
 * A small GPIO or clock initialization helper called once from
 * system_init at 0x080275A0.
 *
 * Context from the disassembly:
 *   - Called just after configuring the ADC prescaler division
 *     (a complex div/mod calculation involving 0x91A2B3C5 magic constant)
 *   - Called with two parameters loaded via ADR (PC-relative):
 *     r0 = address at PC+0x130
 *     r1 = address at PC+0x160
 *     These are likely pointers to configuration structures or
 *     GPIO init parameter blocks
 *   - Immediately after this call: IWDG is initialized
 *     (IWDG_KR = 0x5555, then prescaler = 4)
 *
 * Given its location (0x0800039C, very early in flash) and small size,
 * this is likely a simple register-write helper — possibly setting
 * the ADC clock prescaler or configuring the ADC's GPIO pins
 * (analog input mode for the relevant channels).
 *
 * Called from: system_init (FUN_08023A50)
 *============================================================================*/
void adc_prescaler_init(void *config_a, void *config_b) {
    /* Small init helper — likely configures ADC prescaler or
     * analog GPIO mode. Body requires disassembly of the 24 bytes
     * at 0x0800039C to fully decode. */
}


/*============================================================================
 * FUNCTION: probe_mode_init
 * Address: 0x0800B908
 * Size: ~512 bytes
 * Priority: P3
 *
 * Called once from system_init at 0x080271F8.
 * Located 8 bytes after FUN_0800b900 (the mode dispatcher jump table),
 * suggesting it's the default/init entry in that table.
 *
 * Context from the disassembly:
 *   - Called after calibration_loader (both CH1 and CH2 cal data loaded)
 *   - Called after checking state[0x23A] (some feature flag)
 *   - Followed by relay/GPIO configuration based on state[0x14]
 *     (acquisition mode: 1=scope1ch, 2=scope2ch, 3=meter)
 *
 * This function likely:
 *   1. Reads the saved probe type from SPI flash settings
 *   2. Sends the initial FPGA command sequence to set up the
 *      measurement mode (scope, meter, or siggen)
 *   3. Configures the analog frontend relays to match
 *   4. May send the full channel range + trigger + timebase
 *      command sequences to initialize the FPGA to the saved state
 *
 * At 512 bytes, this is substantial — it probably calls several of
 * the P1/P2 command builders to send the complete FPGA configuration
 * needed to restore the last-used operating mode.
 *
 * Called from: system_init (FUN_08023A50)
 *============================================================================*/
void probe_mode_init(void) {
    /* Initializes the complete measurement mode state:
     * - Probe type detection
     * - FPGA command sequence for saved mode
     * - Analog frontend relay configuration
     * - Gain resistor selection
     *
     * Implementation in init-only code — body at 0x0800B908.
     * May dispatch to the command builders (0x0800BA06-0x0800BC98). */
}


/*============================================================================
 *
 *  DISCREPANCY NOTE: FUN_08039734
 *
 * gap_functions.md calls this "timer_init_helper" (P1) because the init
 * disassembly shows it called with r0=TIM5_CTL0 (0x40000C00) and
 * r0=TIM2_CTL0 (0x40000000).
 *
 * fpga_task_annotated.c calls this "usart_tx_config_writer" at the
 * same address (0x08039734-0x08039870, 316 bytes) and describes it
 * as a 7-type command switch for scope/meter/siggen configuration.
 *
 * RESOLUTION: This appears to be a GENERIC configuration writer that
 * takes a base register pointer (r0) and a parameter block (sp+0x40).
 * When called from system_init with timer base addresses, it configures
 * timers. When called from the FPGA task context with a state structure
 * pointer, it writes USART TX configuration.
 *
 * The function uses a switch statement (TBB at 0x0803973E) with 7 cases.
 * Each case writes specific fields at offsets from the base pointer.
 * The interpretation of those offsets depends on what base is passed:
 *   - If base = 0x40000C00 (TIM5): offsets map to timer registers
 *   - If base = state_ptr: offsets map to USART TX frame fields
 *
 * This is a common embedded pattern — a single config-write function
 * reused for different peripherals by passing different base addresses.
 * The annotated version in fpga_task_annotated.c is correct for the
 * USART usage context. The init calls use it for timer configuration.
 *
 *============================================================================*/


/*============================================================================
 *
 *  SUMMARY: WHAT THIS TELLS US ABOUT THE FIRMWARE ARCHITECTURE
 *
 *  The gap functions reveal a clean layered architecture:
 *
 *  Layer 1: COMMAND BUILDERS (0x0800BA06-0x0800BC98)
 *    These are thin, type-safe wrappers around xQueueGenericSend.
 *    Each one knows the exact FPGA command sequence for its domain
 *    (channels, trigger, acquisition, timebase, probe). They all
 *    follow the same pattern:
 *      1. Optional channel prefix (0x07 or 0x0A)
 *      2. Sequential command bytes
 *      3. Last command uses portMAX_DELAY (blocking)
 *
 *  Layer 2: MODE DISPATCHER (0x0800B900)
 *    A jump table that selects which command builder to call based
 *    on the current operation. This provides a clean abstraction —
 *    callers just say "configure mode N" without knowing the FPGA
 *    command details.
 *
 *  Layer 3: USART TX TASK (0x08037400)
 *    The FreeRTOS task that dequeues commands and serializes them
 *    into 10-byte USART frames. It handles the physical layer:
 *    buffer management, checksums, and timing.
 *
 *  Layer 4: ACQUISITION ENGINE (0x08037454/0x08037800)
 *    The SPI3 data path that reads ADC samples from the FPGA.
 *    Multiple acquisition modes (normal, dual, roll, bulk) with
 *    per-sample VFP calibration and trigger detection.
 *
 *  The init-only functions (calibration_loader, analog_input_init,
 *  probe_mode_init) set up the initial state that these runtime
 *  layers operate on.
 *
 *============================================================================*/
