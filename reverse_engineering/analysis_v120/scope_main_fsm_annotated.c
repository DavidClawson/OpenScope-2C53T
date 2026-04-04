/**
 * scope_main_fsm — Annotated Decompilation
 * FNIRSI 2C53T V1.2.0 Firmware
 *
 * Address: 0x08019E98
 * Size: 13,276 bytes (largest function in firmware — 25% of all code with its callees)
 * Callers: 0 (FreeRTOS task entry — the 'osc' task)
 * Callees: 27 named functions
 *
 * This is THE central orchestrator for oscilloscope mode. It manages:
 *   - Acquisition control (arm, trigger, read, auto-range, auto-timebase)
 *   - DC offset calibration (per-channel, iterative convergence)
 *   - DAC trigger level computation (from calibration tables)
 *   - Waveform rendering pipeline (sample → display coordinate → LCD)
 *   - Sub-mode delegation (timebase, trigger, cursor, measure, display settings)
 *
 * All state field references use offsets from STATE_STRUCTURE.md.
 * Base address: 0x200000F8 (METER_STATE_BASE).
 *
 * Ghidra source: full_decompile.c lines 6595-9122
 */

#include <stdint.h>

/*============================================================================
 *  CALLEES — The 27 functions this FSM calls
 *============================================================================*/

/* Sub-mode handlers (called in groups when redraw_inhibit == 0) */
// FUN_0801e1e4  scope_mode_trigger       — trigger edge/level/source UI
// FUN_08020930  scope_mode_display_settings — display options UI (grid, persist, etc.)
// FUN_0801eaac  scope_mode_measure       — measurement readout (Vpp, Freq, etc.)
// FUN_0801f6f8  scope_mode_cursor        — cursor position/delta display
// FUN_0801d2ec  scope_mode_timebase      — timebase/sample rate display

/* Hardware config */
// FUN_080018a4  gpio_mux_portc_porte     — analog relay switching (PC12, PE4-6)
// FUN_08001a58  gpio_mux_porta_portb     — gain resistor switching (PA15, PA10, PB10)
// FUN_08001c60  siggen_configure         — signal generator reconfiguration

/* Display rendering */
// FUN_08029a70  scope_draw_measurements  — draw measurement values on screen
// FUN_08021de4  scope_draw_channel_info  — draw CH1/CH2 info labels
// FUN_08021b40  (display helper)

/* RTOS & math */
// FUN_0803acf0  xQueueGenericSend        — FreeRTOS queue send
// FUN_08034078  (RTOS state update)      — updates scope runtime state words
// FUN_080342f8  (RTOS config commit)     — commits config to hardware
// FUN_08033cfc  (framebuffer alloc)      — allocates display buffer regions
// FUN_080012bc  memset_zero              — zeroes memory (display buffer clear)

/* Floating-point & comparison */
// FUN_0803c8e0  fp_double_divide         — double-precision division
// FUN_0803edf0  fp_double_compare_eq     — double equality check
// FUN_0803ee0c  fp_double_compare_lt     — double less-than check
// FUN_0803ed70  fp_float_to_double       — float → double conversion
// FUN_0803e5da  fp_int_to_double         — int → double conversion
// FUN_0803e538  fp_double_multiply       — double multiplication
// FUN_0803e124  fp_double_subtract       — double subtraction
// FUN_0803f020  fp_double_add            — double addition
// FUN_08001642  fp_64bit_div             — 64-bit integer division
// FUN_08001732  fp_64bit_sdiv            — 64-bit signed division


/*============================================================================
 *  HIGH-LEVEL STRUCTURE
 *
 *  The function is organized as a multi-level state machine:
 *
 *  LEVEL 1: Entry guard
 *    if (system_mode != 0x02) return;  // Not in scope mode
 *
 *  LEVEL 2: Activity check
 *    if (scope_active_flag == 0) → handle idle/single-shot cases
 *
 *  LEVEL 3: Timebase branch
 *    if (timebase_index > 0x13) → SLOW TIMEBASE path (roll mode)
 *    else → NORMAL TIMEBASE path
 *
 *  LEVEL 4: Acquisition phase state machine (acquisition_phase)
 *    Phase 0: Idle — waiting for trigger
 *    Phase 1: Auto-range — scan samples for clipping
 *    Phase 2: Auto-timebase — measure frequency, set optimal timebase
 *    Phase 3: Stabilization — wait 100 cycles for hardware to settle
 *    Phase 0x11: Post-stabilization
 *    Phase 0xFF: Slow reconfigure — wait 200 cycles, then call siggen_configure
 *
 *  LEVEL 5: scope_sub_mode state machine (scope_sub_mode / +0x30)
 *    Sub-modes 0x30-0x39: DC offset calibration sequence
 *    Sub-modes 0x10-0x20: Voltage range reconfiguration
 *    Sub-mode 0x00: Normal operation
 *
 *  EXIT: Always sends command 3 to USART queue (scope heartbeat)
 *
 *============================================================================*/


/*============================================================================
 *  SECTION 1: ENTRY GUARD
 *  Lines 6731-6733
 *============================================================================*/

void scope_main_fsm(void) {

    /* ---- Entry guard: only run in scope mode ---- */
    if (state[0xF68] != 0x02) {  /* system_mode: 0x02 = oscilloscope */
        return;
    }

    /* ---- Check if scope is active ---- */
    if (state[0x2E] == 0) {  /* scope_active_flag == 0 → scope inactive */
        goto handle_inactive;
    }


/*============================================================================
 *  SECTION 2: SINGLE-SHOT / RUN MODE CHECK
 *  Lines 6735-6745
 *============================================================================*/

    if (state[0x17] == 0x02) {  /* trigger_run_mode: 0x02 = SINGLE SHOT */
        goto handle_single_shot;  /* (Section 8) */
    }


/*============================================================================
 *  SECTION 3: SLOW TIMEBASE (ROLL MODE) PATH
 *  Lines 6736-6743
 *
 *  When timebase_index > 0x13, the scope is in roll/streaming mode.
 *  No trigger, no auto-range — just continuous rendering.
 *============================================================================*/

    if (state[0x2D] > 0x13) {  /* timebase_index > 19 = roll mode */
        if (state[0x2C] == 0) {  /* redraw_inhibit == 0 */
            scope_mode_trigger();
            scope_mode_display_settings();
            scope_mode_measure();
            scope_mode_cursor();
        }
        goto exit_epilog;
    }


/*============================================================================
 *  SECTION 4: NORMAL TIMEBASE — BUSY CHECK
 *  Lines 6745-6746
 *============================================================================*/

    /* Skip if acquisition is busy (double-buffer swap in progress) */
    if (state[0xDBA] != 0 || state[0xDBB] != 0) {
        goto exit_epilog;  /* acq_busy_a or acq_busy_b nonzero */
    }


/*============================================================================
 *  SECTION 5: CHECK FOR NEW DATA
 *  Lines 6747-6754
 *
 *  acq_frame_type == 1 means the SPI3 task has completed an acquisition
 *  and new ADC data is in the sample buffers.
 *============================================================================*/

    if (state[0xDB0] != 1) {  /* acq_frame_type: no new data */
        goto handle_no_data;   /* (Section 8b) */
    }

    /* New data available — run the processing pipeline */

    /* Always update timebase display */
    scope_mode_timebase();

    /* Update sub-mode UIs if redraw is not inhibited */
    if (state[0x2C] == 0) {  /* redraw_inhibit */
        scope_mode_trigger();
        scope_mode_display_settings();
        scope_mode_measure();
        scope_mode_cursor();
    }


/*============================================================================
 *  SECTION 6: ACQUISITION PHASE STATE MACHINE
 *  Lines 6755-7058
 *
 *  This is the heart of the oscilloscope's automatic operation.
 *  It runs after each acquisition to evaluate the signal and adjust settings.
 *
 *  State variable: acquisition_phase (state[0x2F])
 *============================================================================*/

    switch (state[0x2F]) {  /* acquisition_phase */

    case 0x00:  /* IDLE — waiting for trigger, no action needed */
        break;

    case 0x01:  /* AUTO-RANGE — check if signal is clipping */
        /*
         * Only runs after 40+ acquisitions (acquisition_counter > 0x27).
         *
         * Scans the roll mode buffer (301 samples per channel) looking
         * for clipping. Iterates through samples in groups of 16,
         * tracking min and max values.
         *
         * If max > 0xD9 (217) or min < 0x27 (39):
         *   → Signal is clipping. Check if we can increase the range.
         *   → If meter_function[channel] < 9: increment range and reconfigure
         *     relays via gpio_mux_portc_porte / gpio_mux_porta_portb
         *
         * The min/max algorithm is aggressively unrolled — it checks 16
         * consecutive bytes in a single loop iteration for speed.
         *
         * After processing all channels:
         *   acquisition_phase → 0x02 (proceed to auto-timebase)
         */
        if (state[0x32] > 0x27) {  /* acquisition_counter > 39 */
            /* Unpack channel_config */
            uint8_t active_ch = state[0x15] >> 4;
            uint8_t num_ch = state[0x15] & 0x0F;

            for (ch = active_ch; ch < num_ch; ch++) {
                uint8_t *buf = &state[0x356 + ch * 0x12D];  /* roll buffer */
                uint8_t min = buf[0], max = buf[0];

                /* Scan 301 samples in groups of 16 for min/max */
                for (int i = 0; i < 300; i += 16) {
                    for (int j = 1; j <= 15; j++) {
                        if (buf[i+j] > max) max = buf[i+j];
                        if (buf[i+j] < min) min = buf[i+j];
                    }
                }

                if (max > 0xD9 || min < 0x27) {
                    /* Signal is clipping — try to increase range */
                    if (state[0x02 + ch] < 9) {  /* meter_function < 9 */
                        state[0x02 + ch]++;       /* Increment range */
                        if (ch == 0)
                            gpio_mux_portc_porte(state[0x02]);
                        else
                            gpio_mux_porta_portb(state[0x03]);
                        /* Send range change command to USART queue */
                        uint8_t cmd = 4;
                        xQueueGenericSend(USART_CMD_QUEUE, &cmd, portMAX_DELAY);
                        state[0x32] = 0;  /* Reset counter */
                        /* Continue to next channel */
                    }
                }
            }
            state[0x2F] = 0x02;  /* → auto-timebase phase */
        }
        break;

    case 0x02:  /* AUTO-TIMEBASE — measure frequency, set optimal timebase */
        /*
         * Runs after 100+ acquisitions (acquisition_counter > 99).
         *
         * Reads the measured period from the cursor data array:
         *   period = cursor_data[active_channel * 0x30]
         * (This was computed by the frequency measurement subsystem.)
         *
         * Maps period (nanoseconds) to optimal timebase index via
         * a massive nested if-else chain (20 levels deep!):
         *
         *   Period (ns)      → Timebase Index
         *   ≥ 30,000,000     → 0  (slowest, 5s/div)
         *   ≥ 10,000,000     → 1
         *   ≥ 5,000,000      → 2
         *   ≥ 3,000,000      → 3
         *   ≥ 1,000,000      → 4
         *   ≥ 500,000        → 5
         *   ≥ 300,000        → 6
         *   ≥ 100,000        → 7
         *   ≥ 50,000         → 8
         *   ≥ 30,000         → 9
         *   ≥ 10,000         → 10
         *   ≥ 5,000          → 11
         *   ≥ 3,000          → 12
         *   ≥ 1,000          → 13
         *   ≥ 500            → 14
         *   ≥ 300            → 15
         *   ≥ 100            → 16  (0x10)
         *   ≥ 50             → 17  (0x11)
         *   ≥ 30             → 18  (0x12)
         *   < 30             → 19  (0x13, fastest, 5ns/div)
         *
         * After setting timebase_index:
         *   1. Send mode command (2) to USART queue → reconfigures FPGA
         *   2. Send trigger command (1) to SPI3 queue
         *   3. Set acquisition_phase = 0x03
         *   4. Clear PC4 via GPIOC_BCR (analog routing relay)
         *   5. Call scope_draw_measurements() to update display
         *
         *   6. COMPUTE DAC TRIGGER LEVEL for both channels:
         *      This is the most complex part. It reads from the
         *      calibration tables at state[+0x260..+0x33C] and
         *      computes a 12-bit DAC value for the trigger comparator.
         *
         *      The formula (for CH1):
         *        range_pair = cal_table[meter_function * 2]  (gain, offset)
         *        gain_diff = (float)(range_pair.high - range_pair.low)
         *        adc_scaled = (float)(ch1_adc_offset + 100)
         *        base = (float)(range_pair.low)
         *        dac_value = (gain_diff / DIVISOR) * adc_scaled + base
         *        DAC_CH1_DATA = dac_value & 0xFFF
         *        DAC_CH1_CTRL |= 1  (enable output)
         *
         *      Which calibration table entry depends on timebase_index:
         *        index < 5 && index == 4 or range == 3: cal[4]/cal[1]
         *        index < 5: cal[5]/cal[2]
         *        index >= 5: cal[3]/cal[0]
         *
         *      DAC registers:
         *        0x40007404 = DAC CH1 control
         *        0x40007408 = DAC CH1 12-bit data (right-aligned)
         *        0x40001C34 = DAC CH2 / Timer comparator
         */
        if (state[0x32] > 99) {
            /* Read period from cursor data */
            int32_t period = *(int32_t *)&state[0x50 + state[0x16] * 0x30];

            /* Map period to timebase index (20-level nested if-else) */
            state[0x2D] = period_to_timebase_index(period);

            /* Reconfigure FPGA */
            xQueueGenericSend(USART_CMD_QUEUE, &(uint8_t){2}, portMAX_DELAY);
            xQueueGenericSend(SPI3_DATA_QUEUE, &(uint8_t){1}, portMAX_DELAY);

            state[0x2F] = 0x03;  /* → stabilization phase */

            GPIOC_BCR = 0x10;  /* PC4 LOW — analog routing */
            scope_draw_measurements();

            /* Compute DAC trigger level for CH1 and CH2 */
            compute_dac_trigger_levels();
        }
        break;

    case 0x03:  /* STABILIZATION WAIT */
        /*
         * Wait for 100 more acquisitions for the hardware to settle
         * after a timebase or range change.
         */
        if (state[0x32] > 99) {
            state[0x32] = 0;
            state[0x2F] = 0x11;  /* → post-stabilization */
        }
        break;

    default:  /* SLOW RECONFIGURE (acquisition_phase = 0xFF or other) */
        /*
         * For phase 0xFF: wait 200 cycles then call siggen_configure().
         * For any other unknown phase: wait 40 cycles then call siggen_configure().
         * This is used after major mode changes that need the signal
         * generator to be reconfigured (e.g., switching frequency ranges).
         */
        if (state[0x2F] == 0xFF) {
            if (state[0x32] > 199) {
                state[0x32] = 0;
                siggen_configure();
            }
        } else if (state[0x32] > 0x27) {
            state[0x32] = 0;
            siggen_configure();
        }
        break;
    }


/*============================================================================
 *  SECTION 7: DC OFFSET CALIBRATION
 *  Lines 7059-7447+
 *
 *  When scope_sub_mode is in the range 0x30-0x39 (decimal '0'-'9'),
 *  the scope performs iterative DC offset calibration.
 *
 *  This is an automatic calibration that runs after 40+ acquisitions.
 *  It reads 500 ADC samples from the display buffer, averages them,
 *  and computes the deviation from the expected center value.
 *
 *  The algorithm adjusts the calibration table entries based on
 *  how far off the average is from the target:
 *
 *  STEP 1: Read 500 samples from adc_buf_ch1 (or ch2)
 *    - 25 iterations × 20 samples per iteration
 *    - Each sample is converted to float and summed
 *    - Address: 0x200000F8 + 0x6AA + offset (= adc_buf_ch1 + 2)
 *    - For CH2: base shifts by 0x400 (0x200004F8 base)
 *
 *  STEP 2: Compute normalized deviation
 *    - If adc_offset == -100: deviation = (sum / 500) - 28.0
 *    - Else: deviation = (sum / 500) + cal_constant
 *    - Convert to double for precision comparison
 *
 *  STEP 3: Graduated correction based on deviation magnitude
 *    The correction uses 4 thresholds with VFP comparison:
 *
 *    |deviation| ≤ 0: increment cal value by ±1 (fine correction)
 *    |deviation| ≤ 1.0: no correction needed (within tolerance)
 *    |deviation| ≤ 1.5: subtract deviation (medium correction)
 *    |deviation| > 1.5: subtract deviation × 8 (coarse correction)
 *
 *  STEP 4: Write corrected value to calibration table
 *    Table address: 0x200000F8 + ch * 0x78 + range * 2 + 0x288
 *    (For CH1: offset 0x288, for CH2: offset 0x2C4)
 *    These are the gain entries in the 12-entry calibration table.
 *
 *  STEP 5: Update DAC trigger level from corrected cal values
 *    Uses same formula as Section 6's DAC computation.
 *
 *  STEP 6: Iteration control
 *    scope_sub_mode encodes both channel and iteration count:
 *      Bits 3-0: voltage range index (0-9)
 *      Bits 6-4: iteration counter (0-7, wraps)
 *      Bit 7:    channel select (0=CH1, 1=CH2)
 *
 *    When low nibble reaches 10: next voltage range
 *    When all ranges done: check if CH2 needed
 *    Final state: scope_sub_mode = 0x20 or 0x00
 *
 *  After calibration completes:
 *    - Calls gpio_mux_portc_porte and gpio_mux_porta_portb to
 *      restore the relay/gain settings for the current range
 *    - Sets scope_sub_mode = 0x10 (post-calibration)
 *
 *============================================================================*/

    if (state[0x32] > 0x27 && state[0x30] != 0) {
        uint8_t sub = state[0x30];
        state[0x32] = 0;  /* Reset acquisition counter */

        if (sub >= 0x30 && sub <= 0x39) {
            /* DC offset calibration — iterative convergence */
            dc_offset_calibrate(sub);
        }
        else if (sub == 0x10) {
            /* Post-calibration: restore relay settings */
            gpio_mux_portc_porte(state[0x30] & 0x0F);
            gpio_mux_porta_portb(state[0x30] & 0x0F);
            state[0x30] = 0x00;  /* Return to normal operation */
        }
    }


/*============================================================================
 *  SECTION 8: SINGLE-SHOT & INACTIVE HANDLERS
 *  Lines 8087-8155
 *
 *  Handles the cases where:
 *    a) Scope is in single-shot mode (trigger_run_mode == 2)
 *    b) Scope is waiting for trigger
 *    c) Acquisition frame progression (frame_type 1→2→3→4→5→6→9)
 *
 *  acq_frame_type progression:
 *    0 = idle
 *    1 = data ready (normal path, Section 5)
 *    2 = first triggered frame processed
 *    3 = second frame processed → check GPIOC IDR bit 0
 *    4 = CH1 SPI3 read queued
 *    5 = CH2 SPI3 read queued
 *    6 = final → commits timebase change, sends config to RTOS
 *    9 = extended trigger mode
 *
 *  GPIOC_IDR bit 0 check (line 8089):
 *    Tests whether a probe is connected. When probe is present:
 *    - Sends SPI3 queue items (mode 4 = bulk CH1, mode 5 = bulk CH2)
 *    - These trigger the SPI3 acquisition task to read the next frame
 *
 *  Frame type 6 handling (line 8128):
 *    - Clears PC4 via GPIOC_BCR (relay control)
 *    - Sets roll_write_ptr = 0x12D0000 (initial value for fresh capture)
 *    - Calls scope_mode_timebase()
 *    - Clears scope_active_flag (stops further acquisition)
 *    - Sends command 2 to USART CMD queue
 *    - Calls RTOS state update (FUN_08034078)
 *============================================================================*/


/*============================================================================
 *  SECTION 9: WAVEFORM RENDERING PIPELINE
 *  Lines 8150-9112
 *
 *  The most complex display section. Runs when scope has valid data
 *  (after all the acquisition and calibration phases).
 *
 *  STEP 1: Load measurement results (lines 8150-8155)
 *    Reads state[0xDD4], [0xDD0], [0xDCC], [0xDC8] (scope_state_words)
 *    Checks meas_result_c (state[0xE0C]) and meas_result_b (state[0xE08])
 *    If no valid measurements, skip rendering
 *
 *  STEP 2: Compute display scaling
 *    Uses double-precision math to convert ADC sample values to
 *    pixel coordinates within the waveform viewport:
 *      x_scale = timebase_to_display_factor(timebase_index / 3)
 *      y_scale = computed from calibration tables
 *
 *  STEP 3: Waveform trace drawing (lines 8765+)
 *    Uses the waveform viewport (state[0xF78..0xF88]):
 *      waveform_x_origin, waveform_y_origin, waveform_width, waveform_height
 *
 *    For each channel:
 *      - Read sample from roll buffer (state[0x356+])
 *      - Apply calibration: calibrated = (raw + offset) / factor + center
 *      - Clamp to display range [0, 28.0]
 *      - Store to roll buffer at state[0x356]
 *      - Render using framebuffer functions (FUN_08033cfc → alloc,
 *        memset_zero → clear, then pixel writes)
 *
 *  STEP 4: Display buffer management
 *    The waveform display uses a column-based rendering approach:
 *    - Each column in the display area maps to one or more ADC samples
 *    - Column data is stored as 16-bit words in a display list at
 *      state[0xF84] (waveform_width)
 *    - Each word encodes the Y range (min→max) for that column
 *    - memset_zero is called to clear columns before drawing new data
 *
 *============================================================================*/


/*============================================================================
 *  SECTION 10: EXIT EPILOG
 *  Lines 9116-9121
 *
 *  Always executed at the end of scope_main_fsm, regardless of which
 *  path was taken through the state machine.
 *============================================================================*/

exit_epilog:
    if (state[0xF68] == 0x02) {  /* Still in scope mode */
        /* Send scope heartbeat command (3) to USART CMD queue.
         * This keeps the FPGA in scope mode and prevents it from
         * timing out to a default state. */
        uint8_t cmd = 3;
        xQueueGenericSend(USART_CMD_QUEUE, &cmd, portMAX_DELAY);
    }
    return;
}


/*============================================================================
 *
 *  KEY ALGORITHMS — DETAILED BREAKDOWN
 *
 *============================================================================*/


/*============================================================================
 *  ALGORITHM 1: Auto-Range (Phase 1)
 *
 *  Purpose: Automatically adjust voltage range to prevent clipping.
 *
 *  The algorithm scans all samples in the roll buffer (301 per channel)
 *  using a highly unrolled loop that processes 16 bytes at a time.
 *  It tracks the global min and max across the entire buffer.
 *
 *  Decision thresholds:
 *    max > 0xD9 (217/255 = 85%): signal too large → increase range
 *    min < 0x27 (39/255 = 15%):  signal too large → increase range
 *
 *  These thresholds provide ~15% headroom on each side of the ADC range.
 *  The "too large" case triggers a range increase by incrementing
 *  meter_function (state[0x02] for CH1, state[0x03] for CH2).
 *
 *  Range change actions:
 *    1. Increment meter_function for the affected channel
 *    2. Call gpio_mux to switch the analog frontend relays
 *    3. Send command 4 to USART queue (reconfigure FPGA)
 *    4. Reset acquisition counter
 *    5. Continue to next channel
 *
 *  Note: There is NO auto-range DOWN (reducing range when signal is small).
 *  The stock firmware only auto-ranges UP to prevent clipping.
 *  This is a deliberate design choice — auto-range down would cause
 *  oscillation when viewing signals near a range boundary.
 *============================================================================*/


/*============================================================================
 *  ALGORITHM 2: Auto-Timebase (Phase 2)
 *
 *  Purpose: Automatically set timebase to show ~2-3 signal periods.
 *
 *  Reads the measured period from the frequency measurement subsystem
 *  (stored at state[0x50 + channel * 0x30]). This value is in nanoseconds.
 *
 *  The mapping uses a hand-crafted 20-entry lookup implemented as a
 *  nested if-else chain. The thresholds follow a 1-3-5 pattern:
 *  30M, 10M, 5M, 3M, 1M, 500K, 300K, 100K, 50K, 30K, 10K, 5K, 3K,
 *  1K, 500, 300, 100, 50, 30 ns.
 *
 *  This gives roughly 3 decades per decade of the standard oscilloscope
 *  timebase sequence (1-2-5 or 1-3-5 depending on manufacturer).
 *
 *  After setting timebase_index, the FPGA is reconfigured and the
 *  DAC trigger level is recomputed from calibration tables.
 *============================================================================*/


/*============================================================================
 *  ALGORITHM 3: DC Offset Calibration (scope_sub_mode 0x30-0x39)
 *
 *  Purpose: Iteratively calibrate the DC offset of each ADC channel.
 *
 *  The algorithm averages 500 ADC samples and computes the deviation
 *  from the expected center value. It then applies a graduated
 *  correction to the calibration table entry:
 *
 *  Correction strategy (based on |deviation|):
 *    |dev| == 0: do nothing (perfect)
 *    0 < |dev| < 1.0: ±1 fine adjustment
 *    1.0 ≤ |dev| < 1.5: subtract deviation directly
 *    |dev| ≥ 1.5: subtract deviation × 8 (aggressive correction)
 *
 *  This creates a P-controller with three gain zones:
 *    - Dead zone (|dev| < 1.0): fine-grained ±1 nudging
 *    - Linear zone (1.0-1.5): direct correction
 *    - Aggressive zone (> 1.5): 8× amplified correction for fast convergence
 *
 *  The calibration iterates through all voltage ranges (0-9) for each
 *  channel. The scope_sub_mode byte encodes the state:
 *
 *    Bits 3-0: current voltage range (0-9)
 *    Bits 6-4: sub-iteration counter (0-7, wraps at 10 but uses BCD-like encoding)
 *    Bit 7:    channel select (0=CH1, 1=CH2)
 *
 *  When the low nibble reaches 10 (0xA), the counter wraps and either:
 *    a) Moves to the next voltage range (increment bits 3-0), or
 *    b) Advances to channel 2 (set bit 7), or
 *    c) Completes calibration (scope_sub_mode = 0x20)
 *
 *  Calibration table layout:
 *    The 12 calibration entries at state[0x260..0x33C] are organized as
 *    two sets of 6, with 0x78 (120) byte stride per channel:
 *      CH1 base: 0x200000F8 + 0x260 = 0x20000358
 *      CH2 base: 0x200000F8 + 0x260 + 0x78 = 0x200003D0
 *    Each entry contains gain/offset pairs as 16-bit values.
 *
 *  DAC trigger level update:
 *    After each calibration adjustment, the DAC trigger level is
 *    recomputed using the corrected calibration values. This ensures
 *    the hardware trigger comparator tracks the calibration changes.
 *
 *    DAC formula:
 *      dac_value = ((gain_high - gain_low) / divisor) * (adc_offset + 100) + gain_low
 *      DAC register = dac_value & 0xFFF (12-bit DAC)
 *
 *    The three calibration table selections (based on timebase_index):
 *      Fast (index ≥ 5):      cal[3]/cal[0] (CH1), cal[9]/cal[6] (CH2)
 *      Mid (index 4 or r==3): cal[4]/cal[1] (CH1), cal[10]/cal[7] (CH2)
 *      Slow (index < 4):      cal[5]/cal[2] (CH1), cal[11]/cal[8] (CH2)
 *============================================================================*/


/*============================================================================
 *  ALGORITHM 4: DAC Trigger Level Computation
 *
 *  The oscilloscope uses the MCU's DAC to set a voltage that the FPGA
 *  compares against the ADC input for trigger detection. This voltage
 *  must be precisely calibrated.
 *
 *  Hardware:
 *    DAC CH1 control: 0x40007404 (bit 0 = enable)
 *    DAC CH1 data:    0x40007408 (12-bit right-aligned)
 *    DAC CH2/Timer:   0x40001C34
 *
 *  Formula:
 *    Given calibration pair (low, high) from the cal table:
 *      range = (float)(high - low)
 *      scaled_offset = (float)(ch_adc_offset + 100)
 *      dac_value = (range / DIVISOR) * scaled_offset + (float)low
 *
 *    Where DIVISOR is a float constant at 0x0801B748 or 0x0801CD38
 *    (likely 200.0 — normalizing the ±100 offset range).
 *
 *  This is called:
 *    - During auto-timebase (Phase 2)
 *    - After DC offset calibration adjustments
 *    - After range changes
 *============================================================================*/


/*============================================================================
 *
 *  STATE TRANSITIONS SUMMARY
 *
 *  Normal acquisition flow:
 *    acquisition_phase: 0 → 1 → 2 → 3 → 0x11 → (stable running)
 *                       idle  auto   auto   stab
 *                             range  time   wait
 *
 *  Range change during auto-range:
 *    Stays in phase 1, increments meter_function, resets counter
 *
 *  Mode change (via external command):
 *    acquisition_phase → 0xFF → (wait 200 cycles) → siggen_configure
 *
 *  Single-shot:
 *    Bypasses the acquisition_phase machine entirely.
 *    Frame type progression: 1→2→3→4→5→6 then stops.
 *
 *  scope_sub_mode transitions:
 *    0x00 (normal) → 0x30 (start cal) → 0x31...0x39 → 0x30 (next range)
 *                                     → 0x10 (restore relays)
 *                                     → 0x20 (CH2 if needed)
 *                                     → 0x00 (done)
 *
 *============================================================================*/
