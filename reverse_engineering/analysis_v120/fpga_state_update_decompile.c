/*
 * FNIRSI 2C53T V1.2.0 — Decompilation of two critical functions
 * Binary: APP_2C53T_V1.2.0_251015.bin (base 0x08000000)
 * Disassembled with Capstone (ARM Thumb-2)
 * Date: 2026-04-03
 *
 * Naming conventions:
 *   meter_state base = 0x200000F8 (r8 throughout function 1)
 *   Queue handles: see 0x20002D6C-0x20002D84 in CLAUDE.md
 */

/* ============================================================================
 * COMMON DEFINITIONS
 * ============================================================================ */

// meter_state struct base: 0x200000F8
// All offsets below are relative to this base (r8 = 0x200000F8)
//
// +0xF2D (0x20001025) = meter_display_value (uint16_t) — displayed reading
// +0xF2E (0x20001026) = meter_usart_cmd — FPGA command code to send
// +0xF2F (0x20001027) = meter_sub_mode (byte) — sub-mode within DC voltage etc.
// +0xF30 (0x20001028) = meter_raw_value (int32_t) — raw measurement from FPGA
// +0xF34 (0x2000102C) = meter_range_index (byte) — auto-range selection (0-4)
// +0xF36 (0x2000102E) = meter_probe_type (byte) — 1=DC, 2=AC, etc.
// +0xF37 (0x2000102F) = meter_decimal_pos (byte) — decimal point position
// +0xF38 (0x20001030) = meter_unit_index (byte) — index into unit string table
// +0xF3C (0x20001034) = meter_reading_lo (uint16_t) — low word of reading
// +0xF3D (0x20001035) = meter_data_valid (byte) — nonzero when USART data ready
// +0xF40 (0x20001038) = meter_digits_raw (int32_t) — raw digit value from BCD
// +0xF44 (0x2000103C) = meter_exponent (byte) — power-of-10 exponent
// +0xF48 (0x20001040) = meter_max_value (int32_t) — max hold
// +0xF4C (0x20001044) = meter_min_value (int32_t) — min hold
// +0xF50 (0x20001048) = meter_avg_accum (int32_t) — averaging accumulator
// +0xF68 (0x20001060) = mode_state (byte) — operating mode (1-9)
// +0xF69 (0x20001061) = mode_sub_counter (uint16_t) — transition counter
// +0xF6B (0x20001063) = mode_sub_flag (byte)
//
// +0x354 (0x2000044C) = scope_channel_flags (byte) — bit field, bit 3 relevant
// +0x355 (0x2000044D) = scope_misc_flag (byte)
//
// +0xE12 (0x20000F0A) = display_value_lo (int32_t)
// +0xE16 (0x20000F0E) = display_value_hi (int32_t)
// +0xE1A (0x20000F12) = display_sign (byte)
// +0xE1C (0x20000F14) = display_reading (uint16_t)

// Queue/semaphore handles
// 0x20002D6C = usart_cmd_queue
// 0x20002D74 = usart_tx_queue
// 0x20002D7C = meter_sem

// Task handles
// 0x20002DA0 = dvom_tx_task
// 0x20002DA4 = dvom_rx_task

// Register bases
// USART2_CTRL1 = 0x4000440C
// GPIOC_BRR    = 0x40011014
// I2C2/TMR11   = 0x40005C40 (uncertain; see remaining_unknowns.md)


/* ============================================================================
 * FUNCTION 1: fpga_state_update
 * Address: 0x080028E0 — 0x08002BE8  (776 bytes, much larger than estimated)
 *
 * Called by: meter_data_processor after processing each USART RX frame
 * Purpose:  Converts raw meter BCD data to a floating-point measurement value,
 *           performs auto-ranging, then dispatches the appropriate FPGA command
 *           byte via the USART command queue for the next measurement cycle.
 *
 * This is NOT a register-poking function — it's the meter's auto-range and
 * command dispatch engine. It:
 *   1. Converts raw BCD digits + exponent into a double-precision value
 *   2. Takes the absolute value and selects an appropriate range
 *   3. Based on meter_mode (0-7) and sub-mode, picks the FPGA command code
 *   4. Sends 3 command bytes (0x1B, 0x1C, 0x1E) to the USART command queue
 * ============================================================================ */

/*
 * Library calls used (all in the soft-float / math region 0x0803xxxx):
 *   0x0803ED70 = __aeabi_i2d   (int32 -> double)
 *   0x0803E5DA = __aeabi_ui2d  (uint32 -> double)
 *   0x0803C8E0 = pow           (double, double -> double)
 *   0x0803E77C = __aeabi_dmul  (double * double)
 *   0x0803EB94 = __aeabi_dsub  (double - double)
 *   0x0803EE0C = __aeabi_dcmplt(double < double) -> int
 *   0x0803E124 = __aeabi_dadd  (double + double)
 *   0x0803DF48 = __aeabi_d2iz  (double -> int32, truncate) [returns float in s0 too]
 *   0x0803ACF0 = xQueueGenericSend(queue, &item, timeout, copy_pos)
 */

void fpga_state_update(void)
{
    uint8_t *base = (uint8_t *)0x200000F8;  // r8 = meter_state base

    /* ---- Phase 1: Convert BCD to measurement value ---- */

    // 0x080028F2: Check if new data is available
    if (base[0xF3D] == 0) {             // meter_data_valid
        goto auto_range_and_dispatch;     // skip conversion, jump to 0x08002A9A
    }

    // 0x080028FC-0x0800292C: Convert digits to double
    //   value = (double)(int32_t)meter_digits_raw * pow(10.0, (double)meter_exponent)
    double raw_double = (double)(*(int32_t *)(base + 0xF40));  // meter_digits_raw
    double exponent   = (double)(base[0xF44]);                  // meter_exponent
    double scaled     = pow(10.0, exponent);                    // d8 = 10.0 (literal at PC+0x2DC = 1000.0... see note)
    // NOTE: d8 loaded from literal pool = 1000.0. The pow() call uses d8/1000 as base.
    // Actually: pow() takes (d8_const, exponent_as_double) then multiplies by raw_double.
    double value1 = raw_double * scaled;

    // 0x08002932-0x08002960: Same conversion for a second field
    //   value2 = (double)(int32_t)meter_raw_value[0xF30] * pow(10.0, (double)base[0xF37])
    double raw2    = (double)(*(int32_t *)(base + 0xF30));  // meter_raw_value
    double exp2    = (double)(base[0xF37]);                  // meter_decimal_pos
    double scaled2 = pow(10.0, exp2);
    double value2  = raw2 * scaled2;

    // 0x08002964-0x08002968: result = value1 - value2 (difference/delta mode?)
    double result = value1 - value2;   // __aeabi_dsub

    // 0x0800296C-0x08002998: Prepare for iterative rounding
    //   Take absolute value of result (clear sign bit in high word)
    //   Compare against thresholds: 0.0 and 10000.0
    double abs_result = fabs(result);   // done via BFI clearing bit 31
    uint8_t decimal_pos = 0;
    base[0xF37] = 0;                    // reset decimal position

    // 0x0800299E-0x08002A3A: Iterative decimal adjustment loop (unrolled 4x)
    //   While abs_result < 10000.0 and decimal_pos < 4:
    //     abs_result += 1000.0 (d8 literal)
    //     decimal_pos++
    //     store decimal_pos to base[0xF37]
    // This loop is unrolled — 4 iterations comparing against threshold (0x0803EE0C)
    // and adding d8 (1000.0) each time via __aeabi_dadd.

    // Iteration 1: (0x0800299E)
    if (__aeabi_dcmplt(abs_result, 10000.0) == 0) goto done_adjusting;
    decimal_pos = 1;
    base[0xF37] = 1;
    abs_result = __aeabi_dadd(result_with_sign, 1000.0);  // accumulate

    // Iteration 2: (0x080029CA)
    if (__aeabi_dcmplt(abs_result, 10000.0) == 0) goto done_adjusting;
    decimal_pos++;
    base[0xF37] = decimal_pos;
    abs_result = __aeabi_dadd(abs_result, 1000.0);

    // Iteration 3: (0x080029F6)
    if (__aeabi_dcmplt(abs_result, 10000.0) == 0) goto done_adjusting;
    decimal_pos++;
    base[0xF37] = decimal_pos;
    abs_result = __aeabi_dadd(abs_result, 1000.0);

    // Iteration 4: (0x08002A22)
    if (__aeabi_dcmplt(abs_result, 10000.0) == 0) goto done_adjusting;
    base[0xF37] = decimal_pos + 1;
    abs_result = __aeabi_dadd(abs_result, 1000.0);

done_adjusting:

    /* ---- Phase 2: Store integer result + auto-range ---- */

    // 0x08002A3E-0x08002A46: Convert final double to int32
    int32_t int_result = __aeabi_d2iz(abs_result);

    // 0x08002A4A-0x08002A96: Auto-range selection based on absolute value
    //   Compares |int_result| as float against thresholds
    //   Also checks current range_index to avoid unnecessary switching
    float abs_f = fabsf((float)int_result);
    uint8_t range = base[0xF34];            // meter_range_index
    *(int32_t *)(base + 0xF30) = int_result; // store raw value

    if (abs_f >= 1000.0f && range > 1) {
        base[0xF34] = 1;                    // switch to range 1
    } else if (abs_f >= 100.0f && range > 2) {
        base[0xF34] = 2;                    // switch to range 2
    } else if (abs_f >= 10.0f && range >= 4) {
        base[0xF34] = 3;                    // switch to range 3
    }
    // else: keep current range

auto_range_and_dispatch:

    /* ---- Phase 3: Command dispatch based on meter mode ---- */

    // 0x08002A9A-0x08002AAC: Load meter_mode and dispatch via TBB
    uint8_t meter_mode = base[0xF2D];       // 0-7: DCV, ACV, DCA, ACA, resistance, continuity, diode, freq
    uint32_t *queue_handles = (uint32_t *)0x20002D6C;  // r5

    if (meter_mode > 7) goto send_commands;

    /*
     * TBB dispatch table at 0x08002AB0 (8 entries):
     *   Case 0 (DCV):        0x08002AB8
     *   Case 1 (ACV):        0x08002AE6
     *   Case 2 (Resistance): 0x08002AFE  (note: actually labeled by function, see below)
     *   Case 3 (ACA?):       0x08002B0E
     *   Case 4 (DCA):        0x08002AD4
     *   Case 5 (Freq?):      0x08002B20
     *   Case 6 (Continuity): 0x08002B34
     *   Case 7 (Diode):      0x08002B44
     */

    switch (meter_mode) {

    case 0: /* DCV — DC Voltage */
        // 0x08002AB8: Sub-dispatch on meter_sub_mode (base[0xF2F])
        // Sub-TBB at 0x08002AC6 with 4 entries
        {
            uint8_t sub_mode = base[0xF2F];
            if (sub_mode > 3) goto send_all_commands;

            switch (sub_mode) {
            case 0:
                // 0x08002ACA: Initial/reset state
                base[0xF2E] = 0;       // meter_usart_cmd = 0
                // cmd_byte = 0xFF (sent to queue below? actually falls to send_all)
                base[0xF38] = 0xFF;    // meter_unit_index = 0xFF (no unit)
                // NOTE: actually stores cmd byte 0xFF to [0xF38] then falls to queue send
                goto send_all_commands;

            case 1:
                // 0x08002B7A: Same as case 2 in resistance mode
                {
                    uint8_t probe = base[0xF36];  // meter_probe_type
                    if (probe >= 1 && probe <= 2) {
                        base[0xF2E] = probe;      // cmd = probe_type
                    }
                    base[0xF38] = base[0xF37];    // unit_index = decimal_pos
                }
                goto send_all_commands;

            case 2:
                // 0x08002B8E: AC sub-mode within DCV?
                {
                    uint8_t probe = base[0xF36];
                    if (probe == 2) {
                        base[0xF2E] = 3;          // cmd = 3
                    }
                    base[0xF38] = base[0xF37] + 2;  // unit_index = decimal_pos + 2
                }
                goto send_all_commands;

            case 3:
                // Falls to 0x08002B98 area
                // Similar pattern
                break;
            }
        }
        break;

    case 1: /* ACV — AC Voltage */
        // 0x08002AE6: Check probe_type range
        {
            uint8_t probe = base[0xF36];
            if (probe >= 1 && probe <= 2) {        // valid range check (subs r1,r0,#1; cmp r1,#2; lo)
                base[0xF2E] = probe;               // cmd = probe_type
            }
            base[0xF38] = base[0xF37];            // unit_index = decimal_pos
        }
        break;

    case 2: /* Resistance (or similar) */
        // 0x08002AFE-0x08002B0C: Dispatches further on probe_type
        {
            uint8_t probe = base[0xF36];
            if (probe == 1) goto case2_probe1;
            if (probe == 2) {
                base[0xF2E] = 5;                   // cmd = 5
            } else {
                // unreachable or default
            }
            base[0xF38] = base[0xF37] + 2;        // unit_index = decimal_pos + 2
        }
        break;

    case2_probe1:
        // 0x08002B54: probe_type == 1
        base[0xF2E] = 4;                           // cmd = 4
        base[0xF38] = base[0xF37];                // unit_index = decimal_pos
        break;

    case 3: /* ACA? */
        // 0x08002B0E: cmd = 5
        base[0xF2E] = 5;
        base[0xF38] = base[0xF37] + 2;            // unit_index = decimal_pos + 2
        break;

    case 4: /* DCA */
        // 0x08002AD4: cmd = 6 + decimal_pos adjustment
        base[0xF2E] = 6;                           // meter_usart_cmd = 6
        base[0xF38] = base[0xF37] + 5;            // unit_index = decimal_pos + 5
        break;

    case 5: /* Frequency */
        // 0x08002B20: cmd = 7 + decimal_pos + 9
        base[0xF2E] = 7;
        base[0xF38] = base[0xF37] + 9;            // unit_index = decimal_pos + 9
        break;

    case 6: /* Continuity */
        // 0x08002B34: Check probe_type (1 or 2)
        {
            uint8_t probe = base[0xF36];
            if (probe == 1) {
                base[0xF2E] = 8;                   // cmd = 8
            } else if (probe == 2) {
                base[0xF2E] = 9;                   // cmd = 9
            }
            // else: no command update
            base[0xF38] = base[0xF37] + 10;       // unit_index = decimal_pos + 10
        }
        break;

    case 7: /* Diode */
        // 0x08002B44: Same structure as continuity but cmd 10/11
        {
            uint8_t probe = base[0xF36];
            if (probe == 1) {
                base[0xF2E] = 10;                  // cmd = 0x0A
            } else if (probe == 2) {
                base[0xF2E] = 11;                  // cmd = 0x0B
            }
            base[0xF38] = base[0xF37] + 10;       // unit_index = decimal_pos + 10
        }
        break;
    }

send_all_commands:
    // 0x08002BA6-0x08002BDE: Send 3 command bytes to USART command queue
    // Each is a single byte pushed to usart_cmd_queue with portMAX_DELAY (-1)
    {
        uint8_t cmd;

        // Command 0x1B — likely "set measurement mode" FPGA command
        cmd = 0x1B;
        xQueueGenericSend(*(uint32_t *)0x20002D6C, &cmd, portMAX_DELAY, 0);

        // Command 0x1C — likely "set range" FPGA command
        cmd = 0x1C;
        xQueueGenericSend(*(uint32_t *)0x20002D6C, &cmd, portMAX_DELAY, 0);

        // Command 0x1E — likely "trigger measurement" FPGA command
        cmd = 0x1E;
        xQueueGenericSend(*(uint32_t *)0x20002D6C, &cmd, portMAX_DELAY, 0);
    }

    // 0x08002BE2-0x08002BE8: Epilogue — restore FP regs, return
    return;
}

/*
 * SUMMARY — fpga_state_update:
 *   This function is the meter's brain. After the USART RX ISR delivers
 *   decoded BCD data, this function:
 *     1. Converts BCD digits + exponent to a double value
 *     2. Computes a delta (possibly for relative/delta measurement mode)
 *     3. Adjusts the decimal position iteratively (up to 4 shifts)
 *     4. Truncates to int32 and performs auto-ranging (3 thresholds: 1000/100/10)
 *     5. Based on meter_mode (0-7) and probe_type, selects the FPGA command code
 *     6. Queues 3 USART commands (0x1B, 0x1C, 0x1E) to configure the FPGA
 *        for the next measurement cycle
 *
 *   The 3 commands sent (0x1B=mode, 0x1C=range, 0x1E=trigger) match the
 *   FPGA command table documented in FPGA_PROTOCOL_COMPLETE.md.
 *
 *   NO direct GPIO writes or peripheral register pokes in this function.
 *   All FPGA communication goes through the USART command queue.
 */


/* ============================================================================
 * FUNCTION 2: mode_switch_handler
 * Address: 0x080073E4 — 0x080074F2  (270 bytes)
 *
 * Called periodically to handle operating mode transitions.
 * Uses TBB (table branch byte) to dispatch on mode_state (1-9).
 *
 * After the switch block, it transitions mode_state to 2 (active/running)
 * and optionally tail-calls 0x0800B908 (mode_post_transition).
 * ============================================================================ */

/*
 * Library/RTOS calls:
 *   0x0803A78C = vTaskSuspend(TaskHandle_t)
 *   0x0803B3A8 = xQueueGenericReset(QueueHandle_t, newQueue_flag)
 *   0x0803AC38 = xQueueReset(QueueHandle_t) [or xQueueReceive with timeout=0]
 *   0x0800B908 = mode_post_transition — reads mode_state, does further setup
 */

void mode_switch_handler(void)
{
    uint8_t *base = (uint8_t *)0x200000F8;  // r4 = meter_state base

    uint8_t mode = base[0xF68];             // mode_state
    if (mode < 1 || mode > 9) goto epilogue;

    /*
     * TBB dispatch table at 0x080073FC (9 entries, base = 0x080073FC):
     *   Case 1: offset=0x0F -> 0x0800741A  (METER MODE ENTRY: disable USART, suspend tasks, reset queues)
     *   Case 2: offset=0x5E -> 0x080074B8  (ACTIVE/RUNNING: no-op, falls to epilogue check)
     *   Case 3: offset=0x5E -> 0x080074B8  (same as case 2)
     *   Case 4: offset=0x5E -> 0x080074B8  (same as case 2)
     *   Case 5: offset=0x05 -> 0x08007406  (CLEAR DISPLAY: zero out display values)
     *   Case 6: offset=0x05 -> 0x08007406  (same as case 5)
     *   Case 7: offset=0x50 -> 0x0800749C  (ENABLE I2C/TMR11: set control bits)
     *   Case 8: offset=0x5E -> 0x080074B8  (same as case 2)
     *   Case 9: offset=0x78 -> 0x080074EC  (CLEAR FLAG: reset scope_misc_flag)
     */

    switch (mode) {

    case 5:  /* CLEAR DISPLAY VALUES */
    case 6:
        // 0x08007406-0x08007418
        // Zero out the display value registers — preparing for new mode
        *(uint16_t *)(base + 0xE1C) = 0;   // display_reading = 0
        *(uint32_t *)(base + 0xE12) = 0;   // display_value_lo = 0
        *(uint32_t *)(base + 0xE16) = 0;   // display_value_hi = 0
        base[0xE1A] = 0;                    // display_sign = 0
        break;

    case 1:  /* METER MODE ENTRY — full peripheral reconfiguration */
        // 0x0800741A-0x0800749A
        // This is the heavy case: transitioning INTO meter mode from scope or siggen.

        /* Step 1: Disable USART2 RX interrupt (bit 13 of CTRL1) */
        // 0x0800741A: r0 = 0x4000440C (USART2_CTRL1)
        // 0x08007422: r1 = *r0
        // 0x08007424: r1 &= ~0x2000  (clear RXNEIE — RX Not Empty Interrupt Enable)
        // 0x08007428: *r0 = r1
        volatile uint32_t *usart2_ctrl1 = (volatile uint32_t *)0x4000440C;
        *usart2_ctrl1 &= ~(1 << 13);       // Disable USART2 RXNE interrupt
        // EFFECT: Stops meter data from arriving via USART2 interrupts.
        //         The FPGA meter IC will still send data, but MCU ignores it.

        /* Step 2: Suspend dvom_tx_task and dvom_rx_task */
        // 0x0800742A: r0 = *(0x20002DA0)  -> dvom_tx_task handle
        // 0x08007434: vTaskSuspend(dvom_tx_task)
        vTaskSuspend(*(TaskHandle_t *)0x20002DA0);    // Suspend dvom_TX task

        // 0x08007438: r0 = *(0x20002DA4)  -> dvom_rx_task handle
        // 0x08007442: vTaskSuspend(dvom_rx_task)
        vTaskSuspend(*(TaskHandle_t *)0x20002DA4);    // Suspend dvom_RX task
        // EFFECT: Meter processing tasks frozen. No USART frames sent or parsed.

        /* Step 3: Clear PC11 via GPIOC_BRR */
        // 0x08007446: r0 = 0x40011014 (GPIOC_BRR — Bit Reset Register)
        // 0x0800744E: r1 = 0x800 (bit 11)
        // 0x08007452: *r0 = r1
        *(volatile uint32_t *)0x40011014 = (1 << 11);  // PC11 = LOW
        // EFFECT: Unknown pin function. NOT PB11 (FPGA active mode).
        //         PC11 might be a meter-specific enable or mux select.
        //         NOTE: This is GPIOC BRR, not GPIOB. PC11 is undocumented
        //         in the current pin map.
        //         *** NEW DISCOVERY: PC11 appears to be a meter-related control pin ***

        /* Step 4: Reset meter_sem (give semaphore with value 0) */
        // 0x08007454: r0 = *(0x20002D7C)  -> meter_sem handle
        // 0x0800745E: r1 = 0 (new_queue flag)
        // 0x08007462: xQueueGenericReset(meter_sem, 0)
        xQueueGenericReset(*(QueueHandle_t *)0x20002D7C, 0);
        // EFFECT: Clears any pending meter semaphore signals

        /* Step 5: Flush usart_tx_queue */
        // 0x08007466: r0 = *(0x20002D74)  -> usart_tx_queue handle
        // 0x08007470: r1 = 0
        // 0x08007472: xQueueReset(usart_tx_queue, 0)
        xQueueReset(*(QueueHandle_t *)0x20002D74);
        // EFFECT: Discards any queued USART TX commands

        /* Step 6: Initialize meter state variables */
        // 0x08007476-0x08007498
        base[0xF36] = 1;                    // meter_probe_type = 1 (DC default)

        // 0x0800747C: r0 = 0x7FC00000 (this is NaN / special float pattern)
        //   Actually: movs r0, #0; movt r0, #0x7FC0 => 0x7FC00000
        //   As float: NaN (quiet). As int32: 2,143,289,344
        //   Used as "invalid/uninitialized" sentinel value
        uint32_t sentinel = 0x7FC00000;
        *(uint32_t *)(base + 0xF48) = sentinel;  // meter_max_value = NaN sentinel
        *(uint32_t *)(base + 0xF4C) = sentinel;  // meter_min_value = NaN sentinel
        *(uint32_t *)(base + 0xF50) = sentinel;  // meter_avg_accum = NaN sentinel

        *(uint16_t *)(base + 0xF3C) = 0;    // meter_reading_lo = 0
        *(uint16_t *)(base + 0xF2D) = 0;    // meter_display_value = 0
        *(uint32_t *)(base + 0xF30) = 0;    // meter_raw_value = 0
        break;

    case 7:  /* ENABLE I2C2/TMR11 */
        // 0x0800749C-0x080074B2
        // Enable a peripheral at 0x40005C40 (likely I2C2 or TMR11 — see notes)
        {
            // 0x0800749C: r0 = 0x40005C40
            volatile uint32_t *periph = (volatile uint32_t *)0x40005C40;

            // 0x080074A4: r1 = *r0
            // 0x080074A6: r1 |= 2  (set bit 1)
            // 0x080074AA: *r0 = r1
            *periph |= 0x02;               // Enable bit at +0x00
            // If I2C_CR1: bit 1 = PE (Peripheral Enable)
            // If TMR_CR1: bit 1 = CEN? (unlikely, CEN is bit 0)

            // 0x080074AC: r1 = *(r0 + 0x20) = *(0x40005C60)
            // 0x080074AE: r1 |= 2
            // 0x080074B2: *(r0 + 0x20) = r1
            *(periph + 8) |= 0x02;         // Enable bit at +0x20
            // If I2C: +0x20 = I2C_OAR2, bit 1 = ENDUAL (dual address mode)
            // NOTE: This might enable I2C2 for touch panel communication,
            //       used when entering a mode that needs touch input.
        }
        break;

    case 2:  /* ACTIVE/RUNNING — no-op */
    case 3:
    case 4:
    case 8:
        // 0x080074B8: Falls through to epilogue
        break;

    case 9:  /* CLEAR SCOPE FLAG */
        // 0x080074EC-0x080074F2
        base[0x355] = 0;                    // scope_misc_flag = 0
        goto common_epilogue;               // jumps back to 0x080074BE
        break;
    }

    /* ---- Common epilogue (0x080074B4-0x080074E8) ---- */

    // 0x080074B4: Check if mode == 2
    mode = base[0xF68];
    if (mode == 2) return;                  // Already in active mode, done

common_epilogue:
    // 0x080074BE-0x080074E8: Transition to mode 2 (active/running)
    {
        uint8_t chan_flags = base[0x354];    // scope_channel_flags

        base[0xF68] = 2;                    // mode_state = 2 (ACTIVE)
        *(uint16_t *)(base + 0xF69) = 0;   // mode_sub_counter = 0
        base[0xF6B] = 0;                    // mode_sub_flag = 0

        // 0x080074CA: Check bit 3 of channel_flags
        if (chan_flags & 0x08) {             // lsls r1, r1, #0x1C tests bit 3
            // 0x080074D6: Write 0x3C00 to *(0x20002D50)
            *(uint16_t *)0x20002D50 = 0x3C00;
            // 0x20002D50 is NOT in the queue handle area (those start at 0x20002D6C).
            // This might be a timer reload value or acquisition parameter.
            // 0x3C00 = 15360 decimal.
        }

        // 0x080074E4: Tail-call to mode_post_transition
        // pop {r4, r5, r7, lr}; b 0x0800B908
        mode_post_transition();             // @ 0x0800B908
        // This function reads mode_state again and performs additional
        // setup based on the mode (switches up to case 9).
    }
}

/*
 * SUMMARY — mode_switch_handler:
 *
 * Mode state machine with 9 states. Key transitions:
 *
 *   Mode 1 (METER ENTRY):
 *     - Disables USART2 RXNE interrupt (stops meter data flow)
 *     - Suspends dvom_TX and dvom_RX FreeRTOS tasks
 *     - Clears PC11 LOW (*** NEW: undocumented meter control pin ***)
 *     - Resets meter_sem and flushes usart_tx_queue
 *     - Initializes meter state (probe=DC, max/min/avg=NaN sentinel)
 *
 *   Mode 5/6 (DISPLAY CLEAR):
 *     - Zeros all display value registers (clean slate for new mode)
 *
 *   Mode 7 (PERIPHERAL ENABLE):
 *     - Enables I2C2 or TMR11 peripheral (0x40005C40)
 *     - Sets bit 1 at both +0x00 and +0x20 offsets
 *
 *   Mode 9 (SCOPE FLAG CLEAR):
 *     - Clears scope_misc_flag
 *
 *   Modes 2/3/4/8: No-op (already running or intermediate states)
 *
 * After the switch, all non-mode-2 paths transition to mode_state=2 (ACTIVE)
 * and tail-call mode_post_transition @ 0x0800B908.
 *
 * KEY DISCOVERIES:
 *   1. PC11 is a meter-related control pin (cleared when entering meter mode via GPIOC_BRR)
 *   2. Task handles at 0x20002DA0 and 0x20002DA4 are dvom_TX and dvom_RX
 *   3. The NaN sentinel 0x7FC00000 is used to mark max/min/avg as uninitialized
 *   4. Mode transitions always converge to state 2 (ACTIVE)
 *   5. 0x20002D50 holds a 16-bit parameter (0x3C00) gated by scope channel flag bit 3
 */
