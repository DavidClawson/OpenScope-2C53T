/**
 * Core Subsystem Annotations — FNIRSI 2C53T V1.2.0
 *
 * This file documents the 5 critical subsystems needed for core oscilloscope
 * functionality. Each section is based on deep analysis of the decompiled code,
 * cross-referenced with the state structure map (STATE_STRUCTURE.md) and
 * hardware pinout (HARDWARE_PINOUT.md).
 *
 * Contents:
 *   1. scope_mode_timebase (FUN_0801d2ec, 3752B) — FPGA timebase reconfiguration
 *   2. Waveform rendering (FUN_08030524 + FUN_08031f20, 10.7KB) — sample→pixel
 *   3. siggen_configure (FUN_08001c60, ~1.6KB) — analog frontend + signal gen
 *   4. Trigger system (FUN_0801e1e4 + detection logic) — end-to-end trigger
 *   5. Display task + render engine (FUN_08008154, 2.6KB) — mode dispatch + text
 *
 * Cross-references:
 *   - STATE_STRUCTURE.md for all state[+offset] references
 *   - scope_main_fsm_annotated.c for how these are called
 *   - gap_functions_annotated.c for the FPGA command builders
 *   - fpga_task_annotated.c for the SPI3/USART protocol layer
 */


/*============================================================================
 *
 *  1. SCOPE_MODE_TIMEBASE — FUN_0801d2ec (3752 bytes)
 *
 *  The bridge between the scope FSM and the FPGA. Called on every
 *  acquisition frame when new data is ready. Translates the current
 *  timebase_index into FPGA configuration and manages mode transitions.
 *
 *  Callers: scope_main_fsm (on new data), system_init
 *  Callees: 5 functions (xQueueGenericSend, gpio_mux, display helpers)
 *
 *============================================================================*/

/*
 * OVERVIEW:
 *
 * This function is the FPGA timebase reconfiguration engine. When called:
 *
 * 1. Reads timebase_index (state[0x2D]) — the current time/div setting (0-0x13)
 *
 * 2. Computes FPGA parameters from timebase_index:
 *    - SPI3 transfer mode (state[0x14]) — which acquisition mode to use
 *    - FPGA prescaler and period values
 *    - Sample buffer depth
 *
 * 3. Manages transitions between acquisition modes:
 *    - Normal mode (timebase 0x00-0x04): bulk SPI3 reads, 1024 samples
 *    - Fast mode (timebase 0x05-0x13): reduced sample count or ETS
 *    - Roll mode (timebase > 0x13): continuous circular buffer
 *
 * 4. Sends FPGA commands via the command builder pipeline:
 *    - Timebase prescaler (cmd 0x26)
 *    - Timebase period (cmd 0x27)
 *    - Timebase mode (cmd 0x28)
 *
 * 5. Updates display state for the timebase indicator
 *
 * KEY STATE FIELDS ACCESSED:
 *   state[0x04]  ch1_adc_offset (int8)  — for DAC trigger level
 *   state[0x05]  ch2_adc_offset (int8)  — for DAC trigger level
 *   state[0x14]  voltage_range (uint8)  — selects cal table tier
 *   state[0x15]  channel_config (uint8) — packed channel info
 *   state[0x16]  active_channel (uint8) — 0=CH1, 1=CH2
 *   state[0x17]  trigger_run_mode       — 0=auto, 1=normal, 2=single
 *   state[0x1A]  trigger_position (int16) — horizontal offset
 *   state[0x1C]  trigger_level (int16)  — for display
 *   state[0x2D]  timebase_index (uint8) — THE primary input
 *   state[0x2E]  scope_active_flag      — running state
 *   state[0xDB4] roll_write_ptr (uint16) — roll mode pointer
 *   state[0xDB6] roll_fill_level        — roll buffer depth
 *   state[0xDBC] scope_runtime_state    — display rendering state
 *   state[0xDBF] scope_trigger_state    — trigger status
 *   state[0xE08] meas_result_b (float)  — measurement output
 *   state[0xE0C] meas_result_c (float)  — measurement output
 *
 * TIMEBASE INDEX → FPGA MODE MAPPING:
 *
 *   Index | Time/div  | FPGA Mode | SPI3 Mode | Sample Count
 *   ------|-----------|-----------|-----------|-------------
 *   0x00  | 5s/div    | Slow      | Roll (3)  | 301 circular
 *   0x01  | 2s/div    | Slow      | Roll (3)  | 301 circular
 *   0x02  | 1s/div    | Slow      | Roll (3)  | 301 circular
 *   0x03  | 500ms/div | Slow      | Roll (3)  | 301 circular
 *   0x04  | 200ms/div | Medium    | Normal(2) | 1024
 *   0x05  | 100ms/div | Fast      | Normal(2) | 1024
 *   0x06  | 50ms/div  | Fast      | Normal(2) | 1024
 *   0x07  | 20ms/div  | Fast      | Normal(2) | 1024
 *   0x08  | 10ms/div  | Fast      | Normal(2) | 1024
 *   0x09  | 5ms/div   | Fast      | Normal(2) | 1024
 *   0x0A  | 2ms/div   | Fast      | Bulk (4)  | 1024
 *   0x0B  | 1ms/div   | Fast      | Bulk (4)  | 1024
 *   0x0C  | 500us/div | Fast      | Bulk (4)  | 1024
 *   0x0D  | 200us/div | Fast      | Bulk (4)  | 1024
 *   0x0E  | 100us/div | Fast      | Bulk (4)  | 1024
 *   0x0F  | 50us/div  | Fast      | Bulk (4)  | 1024
 *   0x10  | 20us/div  | Fast      | Bulk (4)  | 1024
 *   0x11  | 10us/div  | Fast      | Bulk (4)  | 1024
 *   0x12  | 5us/div   | Fast      | Bulk (4)  | 1024
 *   0x13  | 2us/div   | Fastest   | Bulk (4)  | 1024
 *
 * INTERNAL PROCESSING (from deep analysis):
 *
 *   This function does MORE than just send FPGA commands. It also:
 *
 *   1. SAMPLE RESAMPLING: For slow timebases (>1ms), it resamples the
 *      1024-byte ADC buffers down to 301 samples for the roll buffer.
 *      Uses FUN_080365d4 (resample/scale) to interpolate samples.
 *      Allocates a 1505-byte temporary buffer for the resampling.
 *
 *   2. ROLL MODE TRIGGER DETECTION: In slow mode, performs software
 *      trigger level detection on the resampled data (the FPGA doesn't
 *      trigger in roll mode). Uses the same threshold comparison as
 *      the SPI3 acquisition case 3.
 *
 *   3. VFP CALIBRATION: Applies per-sample calibration to resampled
 *      data. Output is clamped to [0x1C, 0xE4] (28-228 in ADC codes),
 *      which represents the usable display range.
 *
 *   4. DISPLAY UPDATE COORDINATION:
 *      - Calls FUN_08021de4 (scope_draw_channel_info) FIRST
 *      - Processes data
 *      - Calls FUN_08034078 (FPGA finalization) LAST
 *      Order matters — display must be set up before data processing.
 *
 * OUTPUT CLAMPING VALUES:
 *   The calibrated samples are clamped to [0x1C, 0xE4]:
 *     0x1C = 28  (min displayable value — bottom of waveform area)
 *     0xE4 = 228 (max displayable value — top of waveform area)
 *   This gives 200 discrete Y positions (228 - 28 = 200 pixels)
 *
 * CALIBRATION TABLE SELECTION (3 tiers based on timebase):
 *   - Fast (index >= 5):      cal_table[3]/cal_table[0] for CH1
 *   - Medium (index == 4 or voltage_range == 3): cal_table[4]/cal_table[1]
 *   - Slow (index < 4):       cal_table[5]/cal_table[2]
 *
 *   CH2 uses entries offset by +6 (i.e., cal_table[9]/cal_table[6], etc.)
 *
 * FPGA COMMAND SEQUENCE (sent via usart_tx_queue):
 *   1. Mode command to USART CMD queue (mode 2 = timebase change)
 *   2. Trigger command to SPI3 queue (arm new acquisition)
 *   3. GPIO relay adjustments if range changed
 */


/*============================================================================
 *
 *  2. WAVEFORM RENDERING — FUN_08030524 (CH1, 6632B) + FUN_08031f20 (CH2, 4110B)
 *
 *  These functions convert calibrated ADC samples into pixel data on the LCD.
 *  Coordinated by FUN_08032f6c (520B master renderer).
 *
 *============================================================================*/

/*
 * RENDERING ARCHITECTURE:
 *
 *   FUN_08032f6c (coordinator, 520B)
 *     ├── FUN_08030524 (CH1 renderer, 6632B)
 *     │   └── FUN_0800157c (memset_zero_fast)
 *     ├── FUN_08031f20 (CH2 renderer, 4110B)
 *     │   └── FUN_0800157c (memset_zero_fast)
 *     ├── FUN_0802d8b8 (pre-render setup)
 *     ├── FUN_08033cfc (framebuffer allocator)
 *     └── FUN_080012bc (memset_zero — buffer clear)
 *
 * DISPLAY VIEWPORT:
 *   The waveform is drawn into a rectangular region defined by:
 *     x_origin  = state[0xF78] @ 0x20001070  (51 refs)
 *     y_origin  = state[0xF80] @ 0x20001078  (41 refs)
 *     width     = state[0xF84] @ 0x2000107C  (40 refs)
 *     height    = state[0xF88] @ 0x20001080  (42 refs)
 *
 *   Default viewport: ~170 x 160 pixels (varies by mode)
 *
 * FRAMEBUFFER:
 *   Allocated at runtime by FUN_08033cfc (pool-based allocator)
 *   Base pointer stored at _DAT_20008358
 *   Format: RGB565 (16-bit per pixel, little-endian)
 *   Typical size: ~0x2134 bytes (8500 pixels)
 *
 * PIXEL COORDINATE MATH:
 *   For a sample value s (0-255, calibrated uint8):
 *
 *     y_pixel = y_origin + height - (s * height / 256)
 *     x_pixel = x_origin + (sample_index * width / total_samples)
 *
 *   Framebuffer address for pixel at (x, y):
 *     offset = ((y - y_origin) * width + (x - x_origin)) * 2
 *     pixel_addr = _DAT_20008358 + offset
 *
 *   Boundary clipping:
 *     x_origin <= x < x_origin + width
 *     y_origin <= y < y_origin + height
 *
 * COLOR CONSTANTS (RGB565):
 *   0xFB43 = RGB(255, 208, 24) — bright golden/orange — waveform trace
 *   0x18C3 = RGB(24, 24, 24) — dark gray — grid/graticule lines
 *
 * LINE DRAWING:
 *   Uses a Bresenham-variant algorithm (FUN_08019470, 1672B) with
 *   8-octant symmetry rendering. Called via FUN_08019c48 (trace box
 *   coordinator, 160B).
 *
 *   The algorithm:
 *     1. Computes error term: error = line_length * -2 + 3
 *     2. Iterates step_idx from 0 to line_length
 *     3. Uses an octant_mask byte to select which octants to draw:
 *        bit 0 = NE, bit 1 = ENE, bit 2 = NNW, bit 3 = NW,
 *        bit 4 = SW, bit 5 = SSW, bit 6 = SSE, bit 7 = SE
 *     4. Each pixel is bounds-checked against the viewport
 *     5. Writes RGB565 color to framebuffer
 *
 * CH1 vs CH2 DIFFERENCES:
 *   - CH1 (6632B) is larger because it includes additional preprocessing:
 *     calibration table lookups, extended scaling, and possibly FFT data prep
 *   - CH2 (4110B) reuses CH1's calibration infrastructure but with offset
 *     buffer addresses (+0x9B0 instead of +0x5B0)
 *   - Both use the same color constant (0xFB43) — the stock firmware uses
 *     the SAME color for both channels (your custom firmware uses different
 *     theme colors which is an improvement)
 *   - Both are called sequentially by the coordinator
 *
 * RENDERING PIPELINE:
 *   1. Coordinator allocates framebuffer
 *   2. Clear framebuffer region (memset_zero)
 *   3. CH1 renderer:
 *      a. Read from adc_buf_ch1 (state[0x5B0..0x9AF])
 *      b. Apply per-range calibration lookup (cal_table[range])
 *      c. Convert each sample → screen Y coordinate
 *      d. Draw lines between consecutive samples (Bresenham)
 *      e. Clip to viewport bounds
 *   4. CH2 renderer (same pipeline, different buffer)
 *   5. Compositor sends framebuffer to LCD via EXMC
 */


/*============================================================================
 *
 *  3. SIGGEN_CONFIGURE — FUN_08001c60 (~1.6KB)
 *
 *  Configures both the signal generator DAC output AND the analog
 *  frontend relay/gain states. Called from scope_main_fsm during
 *  mode transitions and range changes.
 *
 *============================================================================*/

/*
 * DUAL PURPOSE:
 *   This function serves two distinct roles:
 *   1. Signal generator: configures DAC output waveform/frequency
 *   2. Analog frontend: sets relay states and gain resistors for scope input
 *
 *   It's called from scope_main_fsm (during phase 0xFF reconfigure),
 *   and from system_init (initial hardware setup).
 *
 * KEY STATE FIELDS:
 *   state[0x02]  meter_function (uint8) — selects voltage range for CH1
 *   state[0x03]  meter_range (uint8)    — selects voltage range for CH2
 *   state[0x04]  ch1_adc_offset (int8)  — signed DC offset
 *   state[0x05]  ch2_adc_offset (int8)  — signed DC offset
 *   state[0x14]  voltage_range (uint8)  — overall range setting
 *   state[0x15]  channel_config (uint8) — packed channel info
 *   state[0x16]  active_channel (uint8)
 *   state[0x2D]  timebase_index (uint8)
 *   state[0x2F]  acquisition_phase
 *   state[0x31]  siggen_config_byte
 *
 * CALLS:
 *   gpio_mux_portc_porte (FUN_080018a4) — relay switching
 *   gpio_mux_porta_portb (FUN_08001a58) — gain resistor switching
 *
 * ANALOG FRONTEND CONTROL:
 *   The analog frontend has two control functions:
 *
 *   gpio_mux_portc_porte (FUN_080018a4):
 *     Controls PC12 (input routing), PE4/PE5/PE6 (range/attenuation)
 *     Called with: meter_function index (voltage range)
 *     Sets relay states based on range selection
 *
 *   gpio_mux_porta_portb (FUN_08001a58):
 *     Controls PA15, PA10, PB10 (gain resistors)
 *     Called with: meter_function index
 *     Sets gain path based on range selection
 *
 * RELAY TRUTH TABLE (from CLAUDE.md + hardware analysis):
 *
 *   For DCV (DC Voltage) measurement:
 *     PC12 = HIGH (input routing to scope)
 *     PE4  = HIGH (range select A)
 *     PE5  = LOW  (range select B)
 *     PE6  = HIGH (range select C)
 *     PA15 = HIGH (gain select, DCV mode)
 *     PA10 = HIGH (gain select, DCV mode)
 *     PB10 = LOW  (gain select, DCV mode)
 *
 *   RELAY TRUTH TABLE (from siggen agent deep analysis):
 *
 *   gpio_mux_portc_porte (FUN_080018a4) — Relay control:
 *     Mode | PC12 | PE4  | PE5  | PE6  | Approx Range
 *     -----|------|------|------|------|-------------
 *     0-4  | LOW  | HIGH | var  | LOW  | ~50-200mV/div
 *     5-6  | LOW  | LOW  | var  | HIGH | ~200-500mV/div
 *     7    | HIGH | LOW  | HIGH | HIGH | ~500mV/div
 *     8-9  | LOW  | HIGH | var  | HIGH | ~1-5V/div
 *
 *   gpio_mux_porta_portb (FUN_08001a58) — Gain control:
 *     Mode | PA15 | PB10 | Approx Range
 *     -----|------|------|-------------
 *     0-1  | LOW  | var  | ~50mV (max gain)
 *     2-4  | HIGH | LOW  | ~100mV (with gain stage)
 *     5-7  | LOW  | LOW  | ~200-500mV
 *     8    | LOW  | HIGH | ~1V
 *     9    | LOW  | LOW  | ~2V (min gain)
 *
 *   AUTO-RANGE ALGORITHM (inside siggen_configure):
 *     1. Scan 300 ADC samples, find min/max per channel
 *     2. If max == 0xE4 (228) → overrange, increase range
 *     3. If min == 0x1C (28) → also overrange, increase range
 *     4. 4-bit FSM per channel (states 0x1-0xF) manages transitions
 *     5. AC coupling uses ±45mV window, DC uses ±90mV window
 *
 *   DAC CALIBRATION OUTPUT:
 *     Writes 12-bit value to DAC register 0x40007408:
 *     Formula: DAC = (range_width / cal_divisor) * correction + baseline
 *     Where range_width = cal_table[high] - cal_table[low]
 *
 *     The voltage_range field (state[0x14]) encodes:
 *       0 = 10mV/div (highest gain)
 *       1 = 20mV/div
 *       2 = 50mV/div
 *       3 = 100mV/div (special case — 100x probe mode)
 *       4-9 = progressively lower gain
 *
 *   AC/DC Coupling:
 *     Controlled via FPGA config bitfield (state[0x20]):
 *       Bit 1: CH1 AC/DC (0=DC, 1=AC)
 *       Bit 5: CH2 AC/DC (0=DC, 1=AC)
 *     The physical coupling relay is switched by FPGA, not MCU GPIO.
 *
 * DAC CONFIGURATION (Signal Generator):
 *   DAC registers:
 *     0x40007404 = DAC CH1 control (bit 0 = enable)
 *     0x40007408 = DAC CH1 data (12-bit right-aligned)
 *     0x40001C34 = DAC CH2 / Timer comparator
 *
 *   The signal generator output uses the same DAC that provides
 *   the trigger reference voltage. When in siggen mode, the DAC
 *   output is routed to the front panel BNC connector instead of
 *   the trigger comparator.
 */


/*============================================================================
 *
 *  4. TRIGGER SYSTEM — Complete End-to-End
 *
 *  Configuration: FUN_0801e1e4 (scope_mode_trigger)
 *  FPGA commands: fpga_cmd_set_trigger (FUN_0800BB10)
 *  Detection: SPI3 acquisition case 3 (roll mode trigger)
 *
 *============================================================================*/

/*
 * TRIGGER CONFIGURATION FLOW:
 *
 *   User adjusts trigger → scope_mode_trigger UI handler
 *     → fpga_cmd_set_trigger (5 FPGA commands)
 *       → USART TX task (10-byte frames @ 9600 baud)
 *         → FPGA updates trigger registers
 *
 * FPGA TRIGGER COMMANDS:
 *   1. Channel prefix: 0x07 (CH1) or 0x0A (CH2)
 *   2. 0x16: Trigger threshold LSB
 *   3. 0x17: Trigger threshold MSB
 *   4. 0x18: Trigger mode/edge
 *      - Bits 0-1: edge (00/10=rising, 01/11=falling)
 *      - Bits 2-3: source (00=CH1, 01=CH2, 10=ext)
 *   5. 0x19: Trigger holdoff (BLOCKING send — ensures atomic commit)
 *
 * TRIGGER LEVEL ENCODING:
 *   Stored as int16 at state[0x1C]
 *   Split into LSB/MSB for FPGA commands 0x16/0x17
 *   FPGA reconstructs: threshold = (MSB << 8) | LSB
 *
 * SOFTWARE TRIGGER DETECTION (Roll Mode):
 *   In roll mode (case 3 of SPI3 acquisition), the MCU performs
 *   software trigger detection since the FPGA doesn't trigger in
 *   streaming mode.
 *
 *   Threshold bytes stored at computed offsets:
 *     ch_index = active_channel * 0x12D + trigger_position
 *     threshold_lo = state[ch_index + 0x3EB]
 *     threshold_hi = state[ch_index + 0x3EC]
 *
 *   Rising edge detection:
 *     Trigger when: sample >= threshold_lo AND sample < threshold_hi
 *     (Signal entering the threshold band from below)
 *
 *   Falling edge detection:
 *     Trigger when: sample < threshold_lo OR sample >= threshold_hi
 *     (Signal exiting the threshold band)
 *
 * TRIGGERED DATA CAPTURE:
 *   When trigger fires in SINGLE shot mode:
 *     1. Stop acquisition: scope_active_flag = 0
 *     2. Disable timer: TMR3_CTL0 &= ~0x01
 *     3. Queue trigger event (cmd 2) to USART CMD queue
 *     4. Copy roll buffers → display buffers:
 *        state[0x5B0+i] = state[0x356+i]  (CH1)
 *        state[0x9B0+i] = state[0x483+i]  (CH2)
 *
 *   In AUTO mode: uses entire buffer as triggered data,
 *   sets fill level = 0x12D (full 301-sample buffer)
 *
 * TRIGGER STATE VARIABLES:
 *   state[0x17] = trigger_run_mode (0=AUTO, 1=NORMAL, 2=SINGLE)
 *   state[0x18] = trigger_edge (0=rising, 1=falling)
 *   state[0x1A] = trigger_position (int16, horizontal offset)
 *   state[0x1C] = trigger_level (int16, voltage threshold)
 *   state[0x20] = config_bitfield_a (bits 9,11 = edge/source in FPGA config)
 *   state[0x230] = trigger_state_prev
 *   state[0x231] = trigger_state_curr
 *   state[0x46] = trigger_position_sample (from SPI3 readback)
 */


/*============================================================================
 *
 *  5. DISPLAY TASK + RENDER ENGINE
 *
 *  Display task entry: FreeRTOS task 'display' @ 0x0803DA51
 *  Render engine: FUN_08008154 (display_render_engine, 2612B)
 *  Scope UI: FUN_08015f50 (scope_ui_draw_main, 5170B)
 *
 *============================================================================*/

/*
 * DISPLAY MODE DISPATCH:
 *
 *   The display system uses two state variables for mode selection:
 *
 *   state[0xF60] = ui_state (52 refs!)
 *     Master UI state byte. Controls which screen is currently active.
 *     Accessed by 23 functions — the most widely-read display variable.
 *
 *   state[0xF68] = system_mode
 *     0x00 = boot/splash
 *     0x01 = multimeter
 *     0x02 = oscilloscope
 *     0x03 = signal generator
 *     0x04+ = settings, etc.
 *
 *   DAT_20001062 = display_refresh_mode (packed byte)
 *     Low nibble (bits 3-0): current display sub-mode
 *     Scope sub-modes:
 *       0 = Normal waveform (grid + channel traces)
 *       1 = FFT spectrum display
 *       2 = Multi-channel measurement mode
 *       3 = XY mode / other
 *
 * SCOPE UI DRAW (scope_ui_draw_main, 5170B):
 *
 *   Drawing sequence for normal waveform mode (sub-mode 0):
 *
 *   1. GRATICULE/GRID:
 *      Two nested loops draw grid dots in RGB565 0x18C3 (dark gray)
 *      Grid spans: x = 0x7B to 0x131, y = 0x3E to 0x5E (~176x32 pixels)
 *      Uses direct framebuffer writes with bounds checking
 *
 *   2. CHANNEL LABELS:
 *      "CH1" / "CH2" text rendered at fixed positions
 *      Label pointers: 0x080BC4F2 (CH1), 0x080BC5CF (CH2) — in SPI flash
 *      Color from theme table at 0x0804C180
 *
 *   3. WAVEFORM TRACES:
 *      Calls the waveform render coordinator (FUN_08032f6c)
 *      Which calls CH1 renderer (FUN_08030524) and CH2 renderer (FUN_08031f20)
 *
 *   4. MEASUREMENTS:
 *      Calls scope_draw_fft_bars (FUN_08019ce8, 276B) for spectral display
 *      Uses sprintf formatting for voltage/frequency/period readouts
 *
 *   5. UI ELEMENTS:
 *      Header bars: scope_ui_draw_header_ch1/ch2
 *      Range lists: scope_ui_draw_range_list_ch1
 *      Timebase list: scope_ui_draw_timebase_list
 *      Trigger info: scope_ui_draw_trigger_info
 *
 * DISPLAY RENDER ENGINE (FUN_08008154, 2612B):
 *
 *   A sophisticated TEXT LAYOUT ENGINE with:
 *
 *   - Word-wrap: Detects space (0x20) characters for line breaking
 *   - Multi-line: Vertical stacking with configurable line height
 *   - Alignment: Left (mode 1), center, right (mode 2)
 *   - Glyph rendering: Per-character bitmap rasterization
 *
 *   Parameters:
 *     param_1: X position (left edge)
 *     param_2: Y position (top edge)
 *     param_3: Max width (pixels)
 *     param_4: Line height
 *     param_5: Text/string data pointer
 *     param_6: Font metric table pointer (0x20001114 or 0x20001144)
 *     param_7: Font/format flags
 *     param_8: Alignment mode (0=no wrap, 1=left, 2=right)
 *
 *   Pipeline:
 *     1. Measure text: count glyphs, compute total width
 *     2. Allocate layout buffers (glyph pointers + metrics)
 *     3. Word-wrap: break at space characters when width exceeded
 *     4. Align: compute X offset based on alignment mode
 *     5. Render: call glyph_render_single (FUN_08008b90, 414B)
 *        for each character, writing RGB565 pixels to framebuffer
 *
 *   Callers (13 functions — used EVERYWHERE):
 *     - scope_ui_draw_header_ch1/ch2
 *     - scope_ui_draw_range_list_ch1
 *     - scope_ui_draw_timebase_list
 *     - scope_ui_draw_trigger_info
 *     - meter_ui_draw_header
 *     - meter_ui_draw_value
 *     - meter_ui_draw_range_list
 *     - meter_ui_draw_bargraph
 *     - scope_ui_draw_main (5170B monster)
 *
 * LCD INTERFACE:
 *   Command register: 0x6001FFFE (ST7789V command port)
 *   Data register:    0x60020000 (ST7789V data port)
 *   Framebuffer:      _DAT_20008358 (SRAM, ~8500 pixels @ 2B each)
 *   Color format:     RGB565 (16-bit: R5 G6 B5)
 *
 * REFRESH STRATEGY:
 *   - Full redraw: grid, waveforms, text rendered each frame
 *   - Partial update: measurement values updated more frequently
 *   - Timer-driven: TMR3 triggers refresh (~50-100 Hz estimated)
 *   - Event-driven: button/mode changes trigger immediate redraw
 *
 * DISPLAY FUNCTION CALL HIERARCHY:
 *
 *   display_task (FreeRTOS entry @ 0x0803DA51)
 *     └── mode dispatch on system_mode + ui_state
 *         ├── scope_ui_draw_main (5170B)
 *         │   ├── display_render_engine (2612B)
 *         │   │   ├── glyph_render_single (414B)
 *         │   │   └── framebuffer_alloc (FUN_08033cfc)
 *         │   ├── measurement_dispatch (520B)  [was "waveform_coordinator"]
 *         │   │   ├── jpeg_huffman_decode_1 (6632B)  [was waveform_render_ch1 — actually a JPEG decoder]
 *         │   │   └── jpeg_huffman_decode_2 (4110B)  [was waveform_render_ch2 — sibling JPEG decoder]
 *         │   ├── scope_capsule_draw (FUN_08019470 + FUN_08018DA0 + FUN_08015F50, amber 0xFB43)
 *         │   ├── scope_draw_fft_bars (276B)
 *         │   └── scope_ui_draw_* (headers, lists, trigger info)
 *         ├── meter_ui_draw_* (multimeter screens)
 *         ├── siggen_ui_draw_* (signal gen screens)
 *         └── settings_ui_draw_* (settings menus)
 */


/*============================================================================
 *
 *  CROSS-CUTTING INSIGHTS — What These 5 Subsystems Tell Us Together
 *
 *============================================================================*/

/*
 * DATA FLOW SUMMARY (ADC to Pixel):
 *
 *   FPGA ADC → SPI3 → adc_buf_ch1/ch2 → VFP calibration →
 *   scope capsule-draw pipeline (FUN_08019470 + FUN_08018DA0 + FUN_08015F50,
 *   filled Bresenham capsule, amber 0xFB43) → framebuffer → LCD via EXMC
 *
 *   NOTE: FUN_08030524 / FUN_08031f20, previously labeled waveform_render_ch1/ch2
 *   in function_names.md, are actually JPEG Huffman decoders for UI assets
 *   (2026-04-04 correction). They play no part in the scope waveform path.
 *
 *   The scope_mode_timebase function controls the INPUT side (how fast
 *   and how many samples). The capsule-draw pipeline controls the OUTPUT
 *   side (how samples map to pixels). The scope_main_fsm coordinates
 *   both sides.
 *
 * TRIGGER FLOW (button press to triggered display):
 *
 *   Button → scope_mode_trigger → fpga_cmd_set_trigger →
 *   USART TX task → FPGA registers → trigger detection →
 *   data capture → display buffer → waveform render → LCD
 *
 * MODE TRANSITION FLOW:
 *
 *   Button → key task → mode_dispatch → system_mode update →
 *   siggen_configure (reconfigure analog frontend) →
 *   scope_mode_timebase (reconfigure FPGA) →
 *   scope_ui_draw_main (render new mode) → LCD
 *
 * CALIBRATION FLOW:
 *
 *   scope_main_fsm DC cal (sub_mode 0x30-0x39) →
 *   reads 500 ADC samples → averages → graduated correction →
 *   updates cal_table entries → recomputes DAC trigger level →
 *   siggen_configure (apply new relay settings)
 *
 * KEY DESIGN PATTERNS IN THE STOCK FIRMWARE:
 *
 *   1. QUEUE-BASED DECOUPLING: Commands flow through FreeRTOS queues.
 *      The scope FSM never talks to hardware directly — it queues
 *      commands that other tasks (USART TX, SPI3) execute.
 *
 *   2. STATE STRUCTURE AS GLOBAL BUS: The ~4KB state structure at
 *      0x200000F8 is the shared communication medium. All 71 functions
 *      read/write it. There is no message-passing for configuration —
 *      everything is shared state.
 *
 *   3. TIMER-DRIVEN EVERYTHING: TMR3 ISR drives the entire system:
 *      USART exchange, button scanning, acquisition timing. The tasks
 *      mostly block on queues waiting for TMR3 to feed them.
 *
 *   4. SINGLE-COLOR WAVEFORM: The stock firmware uses the same color
 *      (0xFB43) for both channels. Your custom firmware's theme system
 *      with separate channel colors is a real improvement.
 *
 *   5. FULL REDRAW PER FRAME: The display task does full redraws rather
 *      than incremental updates. This is simpler but slower. Your
 *      custom firmware could optimize by tracking dirty regions.
 */
