/**
 * FPGA Communications Deep Dive — FNIRSI 2C53T V1.2.0
 *
 * This file documents the complete FPGA communication system at byte-level
 * detail, based on disassembly analysis of the stock firmware binary.
 * Created 2026-04-04 as a companion to core_subsystems_annotated.c.
 *
 * Contents:
 *   1. Mode Init Dispatcher (FUN_0800b908, 512B) — boot-time FPGA config
 *   2. USART2 ISR (0x080277B4, 304B) — complete RX state machine
 *   3. Meter Data Pipeline — BCD extraction through calibrated display
 *   4. All 9 SPI3 Acquisition Modes — byte-level protocol per mode
 *   5. Scope Trigger-to-Display Flow — end-to-end data path
 *
 * KEY CORRECTIONS TO PREVIOUS ANALYSIS:
 *   - The "5 command builder functions" are 10 cases of ONE switch in FUN_0800b908
 *   - FUN_0800b908 runs ONCE at boot, not at runtime
 *   - It queues 1-byte cmd codes to usart_cmd_queue, NOT [param,cmd] pairs
 *   - Parameter encoding happens downstream in dispatch handlers
 *   - Previous function names were wrong (see Section 1 corrections table)
 *   - 0x0803b09c is xSemaphoreGiveFromISR, NOT xQueueReceiveFromISR
 *
 * Cross-references:
 *   - core_subsystems_annotated.c — timebase, waveform, trigger, display
 *   - fpga_task_annotated.c — acquisition task framework
 *   - STATE_STRUCTURE.md — all state[+offset] references
 *   - gap_functions_annotated.c — TX task, config writer
 */


/*============================================================================
 *
 *  1. MODE INIT DISPATCHER — FUN_0800b908 (512 bytes)
 *
 *  ARCHITECTURAL CORRECTION: This is NOT 5 independent "command builder"
 *  functions. It is a SINGLE function with a 10-case TBH switch, called
 *  ONCE at boot from system_init (FUN_08023A50). Ghidra split the cases
 *  into separate functions because it couldn't recover the jump table.
 *
 *  Purpose: Initialize FPGA to the last-saved operating mode at power-on.
 *  Input: state[0xF68] = current_mode (0-9)
 *  Output: Sequence of 1-byte command codes queued to usart_cmd_queue
 *
 *============================================================================*/

/*
 * DATA FLOW:
 *
 *   FUN_0800b908 (boot, runs once)
 *       |
 *       | Queues 1-byte FPGA command codes
 *       v
 *   usart_cmd_queue (0x20002D6C, 20 x 1-byte items)
 *       |
 *       | usart_cmd_dispatcher dequeues, dispatches via table at 0x0804BE74
 *       v
 *   dispatch handler for each cmd code
 *       |
 *       | Handler calls usart_tx_config_writer to produce [param, cmd] pairs
 *       v
 *   usart_tx_queue (0x20002D74, 10 x 2-byte items)
 *       |
 *       | fpga_usart_tx_task dequeues 2-byte items
 *       v
 *   10-byte USART2 TX frame to FPGA
 *
 * CRITICAL: The parameter for each command is computed DOWNSTREAM by the
 * dispatch handler (reading current state), not by this boot init function.
 */

/*
 * PROBE DETECTION PATTERN (used in cases 1, 3, 9):
 *
 *   Read GPIOC_IDR (0x40011008) bit 7 (PC7 = probe detect pin)
 *   PC7 HIGH (probe connected): queue cmd 0x07
 *   PC7 LOW  (no probe):        queue cmd 0x0A
 */

/* ---- Case 0: Oscilloscope Mode (state[0xF68] == 0) ---- */
// Commands: 0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
// All queued to usart_cmd_queue with portMAX_DELAY
// 0x00 = reset/init
// 0x01 = configure channel
// 0x0B-0x11 = scope configuration sequence (7 commands)

/* ---- Case 1: Multimeter Basic (state[0xF68] == 1) ---- */
// Previously misnamed "fpga_cmd_set_channel_ranges" (FUN_0800ba06)
// Commands: 0x00, 0x09, [0x07/0x0A], 0x1A, 0x1B, 0x1C, 0x1D, 0x1E
// 0x09 = meter start measurement
// 0x07/0x0A = probe detect
// 0x1A-0x1E = CH1 gain, CH1 offset, CH2 gain, CH2 offset, coupling/BW

/* ---- Case 2: Signal Generator (state[0xF68] == 2) ---- */
// Commands: 0x02, 0x03, 0x04, 0x05, 0x06, 0x08
// Then falls through to case 9 tail: 0x14, 0x09, [0x07/0x0A]
// 0x02-0x06 = siggen setup (freq, wave, amplitude, offset, duty)
// 0x08 = meter configure range

/* ---- Case 3: Multimeter Extended (state[0xF68] == 3) ---- */
// Previously misnamed "fpga_cmd_set_trigger_config" (FUN_0800bb10)
// Commands: 0x00, 0x08, 0x09, [0x07/0x0A], 0x16, 0x17, 0x18, 0x19
// 0x08 = meter configure range
// 0x16-0x19 = trigger threshold LSB/MSB, mode/edge, holdoff

/* ---- Case 4: Mode 4 (state[0xF68] == 4) ---- */
// Previously misnamed "fpga_cmd_set_acquisition_mode" (FUN_0800bba6)
// Commands: 0x00, 0x1F, 0x09, 0x20, 0x21
// GPIOC read occurs but value is discarded
// 0x1F = mode 4 config, 0x20/0x21 = acquisition run/depth

/* ---- Case 5: Mode 5 (state[0xF68] == 5) ---- */
// Previously misnamed "fpga_cmd_set_timebase" (FUN_0800bc00)
// Commands: 0x00, 0x25, 0x09, 0x26, 0x27, 0x28
// 0x25 = mode 5 config
// 0x26-0x28 = timebase prescaler/period/mode

/* ---- Case 6: Standalone (state[0xF68] == 6) ---- */
// Single command: 0x29

/* ---- Case 7: Standalone (state[0xF68] == 7) ---- */
// Single command: 0x15

/* ---- Case 8: Mode 8 (state[0xF68] == 8) ---- */
// Commands: 0x00, 0x2C

/* ---- Case 9: Multimeter Variant (state[0xF68] == 9) ---- */
// Previously misnamed "fpga_cmd_set_probe_atten" (FUN_0800bc98)
// Commands: 0x00, 0x12, 0x13, 0x14, 0x09, [0x07/0x0A]
// 0x12-0x14 = meter variant setup
// Case 2 falls through to this tail at 0xBC6E

/*
 * NAMING CORRECTIONS TABLE:
 *
 * Old Name (from gap_functions.md)     | Actual Purpose
 * -------------------------------------|--------------------------------
 * fpga_cmd_set_channel_ranges          | Meter basic mode init (case 1)
 * fpga_cmd_set_trigger_config          | Meter extended mode init (case 3)
 * fpga_cmd_set_acquisition_mode        | Mode 4 init (freq counter?)
 * fpga_cmd_set_timebase                | Mode 5 init
 * fpga_cmd_set_probe_atten             | Meter variant mode init (case 9)
 *
 * ALL sends use portMAX_DELAY (blocking), not just the final one.
 * The function is boot-time only — runtime mode changes use different paths.
 */


/*============================================================================
 *
 *  2. USART2 ISR — 0x080277B4 (304 bytes)
 *
 *  Complete interrupt service routine for FPGA ↔ MCU serial communication.
 *  Handles both TX (pumping bytes from tx_buffer) and RX (assembling frames
 *  with sync validation).
 *
 *============================================================================*/

/*
 * MEMORY LAYOUT:
 *
 * Address      Name              Type     Purpose
 * 0x40004400   USART2_STS        reg      Status (bit 5=RDBF, bit 7=TDBE)
 * 0x40004404   USART2_DATA       reg      Data register (R=RX, W=TX)
 * 0x4000440C   USART2_CTRL1      reg      Control (bit 5=RDBFIEN, bit 7=TDBEIEN)
 * 0x20000005   tx_buffer[10]     RAM      10-byte TX frame
 * 0x2000000F   tx_index          uint8    TX byte position (0-9, 10=complete)
 * 0x20004E10   rx_index          uint8    RX byte position (0-11)
 * 0x20004E11   rx_buffer[12]     RAM      12-byte RX frame
 * 0x20001034   exchange_lock     uint16   state[+0xF3C], nonzero=suppress notify
 * 0x20002D7C   meter_sem         ptr      Binary semaphore → dvom_RX task
 */

/*
 * RX STATE MACHINE:
 *
 * rx_index=0:  IDLE. Store byte. Accept only 0x5A or 0xAA as frame start.
 *              Anything else → reset to 0.
 *
 * rx_index=1:  HEADER VALIDATION.
 *              0x5A+0xA5 → valid data frame header, continue
 *              0xAA+0x55 → valid echo frame header, continue
 *              Anything else → reset to 0
 *
 * rx_index=2..8:  BODY ACCUMULATION. Store byte, increment, no validation.
 *
 * rx_index=9:  ECHO FRAME CHECK (10 bytes accumulated).
 *              If rx_buffer[0]==0xAA:
 *                Validate: rx[1]==0x55 AND rx[3]==tx_buffer[3] AND rx[7]==0xAA
 *                All pass → echo accepted, rx_index reset (silently consumed)
 *                Fail → silently discard, continue accumulating
 *
 * rx_index=11: DATA FRAME COMPLETE (12 bytes).
 *              Reset rx_index=0 unconditionally.
 *              Three gates: tx_index==10 AND exchange_lock==0 AND valid header
 *              All pass → xSemaphoreGiveFromISR(meter_sem)
 *                       → PendSV if higher-priority task woken
 *
 * TX PATH (simpler):
 *   Read tx_index, write tx_buffer[tx_index] to USART2_DATA, increment.
 *   When tx_index reaches 10: clear TDBEIEN (disable TX interrupt).
 */

/*
 * TX FRAME FORMAT (MCU → FPGA, 10 bytes):
 *
 * [0] 0x55  (header, fixed at init)
 * [1] 0xAA  (header, fixed at init)
 * [2] param (written per-command by fpga_usart_tx_task)
 * [3] cmd   (FPGA command code 0x00-0x2C)
 * [4..8] 0x00 (padding, fixed at init)
 * [9] checksum = (param + cmd) & 0xFF
 *
 * RX DATA FRAME (FPGA → MCU, 12 bytes):
 *
 * [0]  0x5A  (sync)
 * [1]  0xA5  (sync)
 * [2]  BCD data byte 0  ┐
 * [3]  BCD data byte 1  │ Cross-byte nibble pairs
 * [4]  BCD data byte 2  │ for 4-digit meter reading
 * [5]  BCD data byte 3  │
 * [6]  BCD data byte 4  ┘ Also: flag bits (hold, range)
 * [7]  Status byte (AC/DC, range, polarity, overload)
 * [8-10] Additional data
 * [11] Rolling counter (F0→F1→F2→F3 observed on hardware)
 *
 * RX ECHO FRAME (FPGA → MCU, 10 bytes):
 *
 * [0] 0xAA  (sync)
 * [1] 0x55  (sync)
 * [2] FPGA status
 * [3] Echo of tx_buffer[3] (command byte)
 * [4-6] Response data
 * [7] 0xAA  (integrity marker, MUST match)
 * [8-9] Response data
 *
 * STATUS BYTE (rx[7]) BIT FIELDS:
 *   Bit 0: Polarity / calibration coefficient select
 *   Bit 2: AC flag
 *   Bit 3: Range change indicator
 *
 * FLAGS BYTE (rx[6]) BIT FIELDS:
 *   Bit 4: Range 2 indicator
 *   Bit 5: Range 1 indicator
 *   Bit 6: Hold flag
 */

/*
 * ERROR HANDLING: NONE for hardware errors.
 *   No overrun (ORE) check
 *   No framing error (FE) check
 *   No timeout mechanism
 *   Recovery only via sync byte rejection on next frame
 *
 * SHARED BUFFER WARNING:
 *   rx_buffer at 0x20004E11 is shared between ISR (write) and
 *   meter_data_processor task (read). No double-buffering.
 *   At 9600 baud, inter-frame time is ~12.5ms (12 bytes x 10 bits / 9600),
 *   giving sufficient time for the priority-3 task to process before overwrite.
 *
 * NAMING CORRECTION:
 *   function_names.md lists 0x0803b09c as "xQueueReceiveFromISR"
 *   but it's actually xSemaphoreGiveFromISR (which is xQueueGenericSendFromISR
 *   internally). The ISR is SENDING a notification, not receiving.
 */


/*============================================================================
 *
 *  3. METER DATA PIPELINE — Complete Path from USART RX to Display
 *
 *  Explains the 3.7x calibration error in custom firmware.
 *
 *============================================================================*/

/*
 * STOCK FIRMWARE PIPELINE:
 *
 *   USART2 ISR → rx_buffer[12]
 *       |
 *       v  (meter_sem wakes dvom_RX task)
 *   meter_data_processor (0x08036AC0, 1776B)
 *       | BCD extraction via cross-byte nibble pairs
 *       | 7-segment lookup table → digits 0-9
 *       | Double-precision calibration math
 *       v
 *   meter_mode_handler (0x080371B0, 504B)
 *       | 8-state FSM parsing frame[6]/[7] status bits
 *       | Sets decimal position, range, AC/DC, polarity
 *       v
 *   fpga_state_update (0x080028E0, 768B)
 *       | Auto-range: compares |value| against 10/100/1000
 *       | Sends FPGA commands 0x1B, 0x1C, 0x1E for range feedback
 *       v
 *   Display task reads formatted result
 *
 *
 * CUSTOM FIRMWARE PIPELINE (MISSING COMPONENTS):
 *
 *   USART2 ISR → fpga.rx_frame
 *       |
 *       v  (meter_sem wakes task)
 *   meter_data_process_frame()
 *       | BCD extraction (same as stock)      ← OK
 *       | Static decimal position lookup      ← WRONG (should be dynamic from FSM)
 *       | raw_bcd / pow(10, 4-decimal_pos)    ← MISSING calibration division
 *       v
 *   Display (no auto-range feedback)          ← MISSING 0x1B/0x1C/0x1E commands
 */

/*
 * BCD DIGIT EXTRACTION (same in both stock and custom):
 *
 *   digit0 = lookup[(frame[2] & 0xF0) | (frame[3] & 0x0F)]
 *   digit1 = lookup[(frame[3] & 0xF0) | (frame[4] & 0x0F)]
 *   digit2 = lookup[(frame[4] & 0xF0) | (frame[5] & 0x0F)]
 *   digit3 = lookup[(frame[5] & 0xF0) | (frame[6] & 0x0F)]
 *
 *   raw_bcd = d0*1000 + d1*100 + d2*10 + d3
 *
 * The nibble pairs encode 7-segment LCD drive signals from the meter IC.
 * Lookup table maps to 0-9 or special codes (0x0A=O, 0x0B=L, 0x10=blank, 0xFF=invalid).
 */

/*
 * STOCK FIRMWARE CALIBRATION (what custom firmware is missing):
 *
 * Step 1: Convert BCD to double with exponent
 *   value = (double)meter_digits_raw * pow(10.0, (double)meter_exponent)
 *
 * Step 2: Iterative decimal adjustment (up to 4x)
 *   While |value| < 10000.0 and iterations < 4:
 *       value *= 10.0
 *       decimal_pos++
 *
 * Step 3: Apply factory calibration coefficients
 *   Coefficients stored at state[+0x29C..+0x34E], loaded from SPI flash at boot
 *   Factory defaults: ~0x0CB0-0x0CCE range (approximately 3248-3278 decimal)
 *   Reference constant: 494.0 (literal pool at 0x080373DC)
 *
 * Step 4: Auto-range selection
 *   |value| >= 1000.0 → range = 1
 *   |value| >= 100.0  → range = 2
 *   |value| >= 10.0   → range = 3
 *   Sends FPGA commands 0x1B, 0x1C, 0x1E to configure next measurement cycle
 *
 * THE 3.7x ERROR EXPLAINED:
 *   For a 1.5V battery on DCV (submode 0):
 *   - Meter IC outputs raw BCD count ≈ 5600
 *   - Custom firmware: 5600 / 1000.0 = 5.6V (wrong — treats as direct voltage)
 *   - Stock firmware: 5600 / calibration_coeff ≈ 5600 / 3.73 ≈ 1501 → 1.501V
 *   - The ~3.73 factor = calibration coefficient / reference = ~3264 / ~875
 *
 * METER MODE HANDLER FSM (0x080371B0, 504B):
 *   8 states: IDLE → POLARITY → OVERLOAD → AC/DC → RANGE → AUTO_RANGE → STANDBY → STANDBY
 *   Reads frame[6] bits 4-6 and frame[7] bits 0-3
 *   Dynamically sets: meter_decimal_pos (state[+0xF37]), meter_probe_type (state[+0xF36])
 *   These feed into the calibration pipeline above
 *   Custom firmware has NONE of this — uses static lookup table for decimal position
 *
 * TO FIX THE CUSTOM FIRMWARE:
 *   1. Implement meter_mode_handler FSM (parse frame[6]/[7] status bits)
 *   2. Use dynamic decimal position from FSM instead of static lookup
 *   3. Apply calibration coefficients (load from SPI flash or use factory defaults)
 *   4. Add auto-range feedback loop (send cmds 0x1B/0x1C/0x1E after each measurement)
 */


/*============================================================================
 *
 *  4. ALL 9 SPI3 ACQUISITION MODES — Byte-Level Protocol
 *
 *  The FPGA task (FUN_08037454) blocks on spi3_data_queue and dispatches
 *  to one of 9 modes based on the trigger byte value (1-9).
 *
 *  Pre-acquisition common preamble (all modes):
 *    1. xQueueReceive(spi3_data_queue, &trigger_byte, portMAX_DELAY)
 *    2. Compute command_code = ~0x7F ^ voltage_range (transforms state[0x1C])
 *    3. Optional probe calibration check → VFP pre-cal path
 *    4. CS Assert: GPIOB_BCR = 0x40 (PB6 LOW)
 *    5. SPI3 exchange: send command_code, discard response
 *    6. CS Deassert: GPIOB_BOP = 0x40 (PB6 HIGH)
 *
 *  VFP registers (loaded once at task entry from literal pool 0x08037674):
 *    s16 = calibration gain
 *    s18 = calibration offset (= -28.0)
 *    s20 = DC bias correction
 *    s22 = max clamp (255.0)
 *    s24 = min clamp (0.0)
 *    s26 = range divisor
 *
 *  Calibration formula (modes 2, 3, 4, 5):
 *    probe_gain = ((float)probe_type + (-28.0)) / gain_divisor
 *    result = ((float)raw_byte + offset - (float)dc_offset) / probe_gain + bias + dc_offset
 *    clamp to [0, 255]
 *
 *============================================================================*/

/*
 * MODE 0 (trigger_byte=1): FAST TIMEBASE CONFIGURATION
 *
 * Purpose: Configure FPGA sampling rate for fast sweeps. Does NOT read ADC data.
 * SPI3 data bytes: 0
 * Action: Reads timebase_index from state[0x2D], looks up threshold from
 *         TIMEBASE_SAMPLE_TABLE[idx] + 0x32, compares against acq_sample_count
 *         at state[0xDB8]. If threshold not reached, sends tb_idx (+1 if <0x13,
 *         capped at 0x12) as SPI3 command. If reached, resets counter.
 * Buffers: None
 * Signals: None
 */

/*
 * MODE 1 (trigger_byte=2): IDLE / SPI3 TRANSFER MODE CHECK
 *
 * Purpose: Conditional SPI3 exchange based on spi3_transfer_mode (state[0x14]).
 * SPI3 data bytes: 0
 * Action: If state[0x14]==0: send 0x03 (keepalive). Otherwise: preamble only.
 * Buffers: None
 * Signals: None
 */

/*
 * MODE 2 (trigger_byte=3): ROLL MODE — 300-Sample Circular Buffer
 *
 * Purpose: Continuous scrolling display for slow timebases (5s-500ms/div).
 * SPI3 data bytes: 5 per trigger (2 calibrated samples stored)
 * Action:
 *   1. Manage circular buffer pointers (roll_read_ptr at state[0xDB4],
 *      roll_sample_count at state[0xDB6], cap at 300)
 *   2. Shift entire buffer left by 1 position (O(n) memmove — expensive!)
 *      CH1: state[0x356..0x482] (301 bytes)
 *      CH2: state[0x483..0x5AF] (301 bytes)
 *   3. CS assert, read 5 bytes via SPI3 (all 0xFF commands):
 *      Bytes stored to state[0x482] (CH1 calibrated) and state[0x5AF] (CH2 calibrated)
 *   4. Apply VFP calibration if probe_type > 0xDC
 *
 * SOFTWARE TRIGGER (when roll_sample_count reaches 301):
 *   Compares samples against threshold band at state[0x3EB]/[0x3EC]
 *   Rising: sample >= threshold_lo AND sample < threshold_hi
 *   Falling: sample < threshold_lo OR sample >= threshold_hi
 *   On trigger: disable TMR3, send cmd 2 to display queue, copy circular → main buffers
 *
 * Buffer addresses: 0x2000044E-0x2000057A (CH1), 0x2000057B-0x200006A7 (CH2)
 */

/*
 * MODE 3 (trigger_byte=4): NORMAL SCOPE — 1024 Bytes Interleaved
 *
 * Purpose: Standard oscilloscope acquisition (timebases 200ms-5us/div).
 * SPI3 data bytes: 1024 (512 CH1 + 512 CH2, interleaved)
 * Action:
 *   1. Send 0xFF, discard response (command acknowledge)
 *   2. Tight loop reading 1024 bytes into state[0x5B0]:
 *      for (i=0; i<0x400; i+=2):
 *        write 0xFF → read even byte (CH1)
 *        write 0xFF → read odd byte  (CH2)
 *   3. VFP calibration (conditional on probe validity + mode):
 *      Processes 8 samples at a time (unrolled), uses ch1_probe for gain
 *   4. 5-POINT GLITCH FILTER (if hold_mode==2):
 *      Scans for pattern: |diff[0]|<8, |diff[1]|>10, |diff[2]|>10, |diff[3]|<8, |diff[4]|<8
 *      Replaces sharp transitions with midpoint of neighbors (edge sharpening)
 *
 * Buffer: state[0x5B0] = 0x200006A8 (1024 bytes, interleaved CH1/CH2)
 * CS held asserted for entire 1024-byte bulk transfer (not toggled per sample).
 */

/*
 * MODE 4 (trigger_byte=5): DUAL CHANNEL — 1024 Bytes CH2
 *
 * Purpose: Second half of double-buffered dual-channel acquisition.
 * SPI3 data bytes: 1024
 * Action:
 *   1. Send 0xFF, discard (acknowledge)
 *   2. Read 1024 bytes into state[0x9B0] (CH2 buffer)
 *   3. VFP calibration using ch2_probe (state[0x353]) for gain
 *   4. Same 5-point glitch filter as Mode 3
 *   5. Increment spi3_frame_count (state[0xDB0])
 *
 * PAIRING: Modes 3+4 form a double-buffer pair. The trigger mechanism sends
 * TWO queue items. Mode 3 fills CH1 (1024B interleaved), Mode 4 fills CH2
 * (1024B). Together = 2048 bytes of dual-channel data.
 *
 * Buffer: state[0x9B0] = 0x20000AA8 (1024 bytes)
 */

/*
 * MODE 5 (trigger_byte=6): METER ADC READ
 *
 * Purpose: Secondary meter data path via SPI3 (separate from USART BCD path).
 * SPI3 data bytes: 0 (1 command/response exchange only)
 * Action: Send active_channel (state[0x16], 0 or 1) as SPI3 byte, discard response.
 * NOTE: This is NOT the primary meter data path. Primary = USART2 BCD digits.
 *       This may read raw analog values for calibration or auxiliary measurement.
 */

/*
 * MODE 6 (trigger_byte=7): SIGNAL GENERATOR FEEDBACK
 *
 * Purpose: Read siggen output feedback from FPGA.
 * SPI3 data bytes: 0
 * Action: Send trigger_mode (state[0x18]) as SPI3 byte, discard response.
 */

/*
 * MODE 7 (trigger_byte=8): STATUS EXCHANGE
 *
 * Purpose: Simple one-byte command/status exchange.
 * SPI3 data bytes: 1
 * Action: Send transformed voltage range command, read status byte.
 */

/*
 * MODE 8 (trigger_byte=9): CALIBRATION — 2-Phase 16-bit Readback
 *
 * Purpose: Read FPGA internal calibration/status register.
 * SPI3 data bytes: 4 (across 2 phases)
 * Action:
 *   Phase 1: Send command → read high byte → store to state[0x46]
 *   CS Deassert + vTaskDelay(1) + CS Assert (required for FPGA latching)
 *   Phase 2: Send 0x0A (readback sub-cmd) → read status (discard)
 *            Send 0xFF → read data (discard)
 *            Send 0xFF → read low byte
 *   Assemble: state[0x46] = (high << 8) | low   (16-bit value at 0x2000013E)
 *
 * Increment spi3_frame_count afterward.
 */

/*
 * SUMMARY TABLE:
 *
 * Mode  trigger  Name              SPI3 Bytes  Buffer Written       VFP Cal  Post-Processing
 * ────  ───────  ────              ──────────  ──────────────       ───────  ───────────────
 *  0      1      Fast TB config       0        None                 No       Reset acq counter
 *  1      2      Idle/config          0        None                 No       None
 *  2      3      Roll mode            5        state[0x356,0x483]   Yes*     Shift buf, sw trigger
 *  3      4      Normal scope      1024        state[0x5B0]         Yes      5-pt glitch filter
 *  4      5      Dual CH2          1024        state[0x9B0]         Yes      5-pt glitch filter
 *  5      6      Meter ADC            0        None                 No       None
 *  6      7      Siggen feedback      0        None                 No       None
 *  7      8      Status exchange      1        None                 No       None
 *  8      9      Calibration          4        state[0x46] (16-bit) No       Frame count++
 *
 *  * Roll mode VFP cal only if probe_type > 0xDC
 */


/*============================================================================
 *
 *  5. SCOPE TRIGGER-TO-DISPLAY FLOW — End-to-End Data Path
 *
 *  Traces the complete lifecycle of oscilloscope data from FPGA sampling
 *  through to pixels on the LCD.
 *
 *============================================================================*/

/*
 * PATH A: NORMAL CONTINUOUS ACQUISITION (auto trigger, normal timebase)
 *
 * STARTUP (from system_init):
 *   1. fpga_sem1 given (available), fpga_sem2 taken (blocked)
 *   2. Boot commands 0x01-0x08 sent via usart_tx_queue
 *   3. Mode command sent to usart_cmd_queue → dispatches to set_acquisition_mode
 *   4. Trigger byte (1) sent to spi3_data_queue → wakes spi3_acquisition_task
 *
 * STEADY-STATE CYCLE (repeating):
 *
 *   ┌─ spi3_acq: blocks on spi3_data_queue ←──────────────────────────────┐
 *   │                                                                       │
 *   │  (trigger byte arrives)                                               │
 *   │                                                                       │
 *   ├─ spi3_acq: CS assert, bulk SPI3 read (1024B), VFP calibrate          │
 *   │            Store to state[0x5B0] / state[0x9B0]                       │
 *   │            Set state[0xDB0] = 1 (NEW DATA flag)                       │
 *   │                                                                       │
 *   ├─ display: calls scope_main_fsm()                                      │
 *   │    Checks: scope_active, trigger_mode, timebase, busy flags           │
 *   │    Checks: state[0xDB0] == 1 → NEW DATA available                    │
 *   │    Calls scope_mode_timebase() → updates FPGA config                  │
 *   │    Runs acquisition_phase state machine (auto-range, auto-timebase)   │
 *   │    Calls waveform_render_ch1/ch2 → Bresenham line draw → LCD          │
 *   │                                                                       │
 *   ├─ display: EXIT EPILOG sends cmd 3 (heartbeat) to usart_cmd_queue      │
 *   │                                                                       │
 *   ├─ usart_cmd_disp: receives cmd 3, dispatches via table[3]              │
 *   │                  → queues FPGA commands to usart_tx_queue             │
 *   │                  → queues next trigger byte to spi3_data_queue ───────┘
 *   │
 *   └─ dvom_TX: receives [param,cmd], builds 10-byte frame, USART2 TX
 *               10.4ms per frame at 9600 baud, then vTaskDelay(10)
 *
 * THE CRITICAL LOOP CLOSURE:
 *   scope_main_fsm exit epilog ALWAYS sends cmd 3 heartbeat.
 *   This eventually re-arms the next SPI3 acquisition.
 *   If the display task stops calling scope_main_fsm, the heartbeat stops,
 *   the FPGA starves, and ALL data flow halts.
 *   (This explains the "display must continuously redraw" lesson in CLAUDE.md)
 */

/*
 * PATH B: USER CHANGES VOLTAGE RANGE
 *
 * Button press → TMR3 ISR debounce (140ms) → button_event_queue
 *   → display task key handler → scope channel handler
 *   → state[0x02] updated (new voltage range)
 *   → gpio_mux_portc_porte(): switches relays (PC12, PE4/5/6)
 *   → gpio_mux_porta_portb(): switches gain resistors (PA15, PA10, PB10)
 *   → Queues 6 FPGA commands: 0x07/0x0A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E
 *   → dvom_TX transmits all 6 (~63ms total)
 *   → FPGA applies new gain/offset immediately
 *   → scope_main_fsm recomputes DAC trigger level from cal tables
 *   → Next SPI3 data reflects new range
 *
 * KEY: Acquisition does NOT stop/restart. FPGA applies settings on the fly.
 * Relays settle ~1ms, next ADC batch already has new range.
 */

/*
 * PATH C: SINGLE-SHOT TRIGGER
 *
 * ARMING:
 *   Button → state[0x17] = 2 (SINGLE mode)
 *   → fpga_cmd_set_trigger(): 5 cmds (0x07/0x0A, 0x16, 0x17, 0x18, 0x19)
 *   → FPGA configures trigger registers (threshold, edge, source)
 *   → state[0x2E] = 1 (scope active), trigger byte → spi3_data_queue
 *
 * TRIGGER DETECTION:
 *   Fast/normal timebases (index 0x04-0x13): HARDWARE in FPGA
 *     FPGA captures data when signal crosses threshold, signals via SPI3 queue
 *   Roll/slow timebases (index 0x00-0x03): SOFTWARE in MCU
 *     spi3_acq mode 2 (roll) compares samples against state[0x3EB]/[0x3EC]
 *
 * CAPTURE & FREEZE (6-frame progression):
 *   Frame 1: data ready, process samples
 *   Frame 3: check probe, queue bulk reads (modes 4+5 for CH1+CH2)
 *   Frame 6: FREEZE — state[0x2E]=0 (inactive), PC4 LOW, final render
 *   After freeze: spi3_acq blocks forever (nothing queued), display frozen
 */

/*
 * PATH D: USER CHANGES TIME/DIV
 *
 * Button → state[0x2D] updated (new timebase_index, 0x00-0x13)
 *   → scope_mode_timebase() determines new acquisition mode:
 *       Index 0x00-0x03: Roll mode (SPI3 mode 2, circular buffer)
 *       Index 0x04:      Medium (SPI3 mode 3, 1024 interleaved)
 *       Index 0x05-0x09: Fast (SPI3 mode 3, 1024 interleaved)
 *       Index 0x0A-0x13: Bulk (SPI3 mode 4, 1024 bulk)
 *   → Sends 3 FPGA timebase commands: 0x26 (prescaler), 0x27 (period), 0x28 (mode)
 *   → Sends cmd 2 → usart_cmd_queue (reconfigure acquisition mode)
 *   → Arms new trigger byte → spi3_data_queue
 *   → Resets acquisition_counter and acquisition_phase for fresh auto-range/TB eval
 *   → Selects calibration table tier:
 *       Fast (>=5):   cal[3]/cal[0]
 *       Medium (4):   cal[4]/cal[1]
 *       Slow (<4):    cal[5]/cal[2]
 */

/*
 * ACQUISITION PHASE STATE MACHINE (within scope_main_fsm):
 *
 * Phase 0x00 (IDLE):     Just rendering, no analysis
 * Phase 0x01 (AUTO-RANGE): After 40 acqs, scan 301 samples for clipping
 *                          max>217 or min<39 → increment range, reconfigure relays
 *                          → send cmd 4 → usart_cmd_queue
 * Phase 0x02 (AUTO-TB):  After 100 acqs, measure period → map to timebase index
 *                          → send cmd 2, arm trigger, compute DAC trigger level
 * Phase 0x03 (STABILIZE): Wait 100 more acqs for hardware settle
 * Phase 0x11 (STABLE):   Normal running state
 * Phase 0xFF (RECONFIG):  Wait 200 cycles, then call siggen_configure()
 *
 * Normal flow: 0x00 → 0x01 → 0x02 → 0x03 → 0x11
 */

/*
 * TIMING SUMMARY:
 *
 * USART2 frame:    10 bytes @ 9600 baud = 10.4ms per command
 * SPI3 bulk read:  1024 bytes @ 60MHz polled = ~170us (with overhead)
 * VFP calibration: 1024 float ops = ~100us
 * Display redraw:  ~5-10ms (full waveform + text + measurements)
 * Button debounce: 70 ticks @ 500Hz = 140ms
 * TMR3 ISR:        2ms period (500Hz)
 * Auto-range eval: every 40 acquisitions
 * Auto-TB eval:    every 100 acquisitions
 * Hardware settle: 100 acquisition cycles after range/TB change
 *
 * SEMAPHORE USAGE:
 *   fpga_sem1 (0x20002D80): Gates init→acquisition transition. Given at init.
 *   fpga_sem2 (0x20002D84): Secondary FPGA ops. Taken at init (blocks).
 *   meter_sem (0x20002D7C): USART RX frame ready → dvom_RX task.
 */
