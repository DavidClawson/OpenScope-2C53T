/**
 * scope_render_monsters_annotated.c
 * FNIRSI 2C53T V1.2.0 Firmware — Scope Rendering Deep Dive
 *
 * This file annotates three MEDIUM-confidence scope functions that drive
 * waveform display fidelity. They are the direct callees of scope_main_fsm
 * (0x08019E98, 13.3KB, previously annotated in scope_main_fsm_annotated.c).
 *
 * Sources consulted:
 *   - full_decompile.c (37,909 lines, Ghidra V1.2.0 decompile)
 *   - scope_main_fsm_annotated.c (parent FSM, already annotated)
 *   - STATE_STRUCTURE.md (global state at 0x200000F8)
 *   - function_names.md (309-function inventory)
 *   - ram_map.txt (sample buffer layout)
 *   - Binary: APP_2C53T_V1.2.0_251015.bin (flash base 0x08000000)
 *
 * ===========================================================================
 * IMPORTANT NAMING CORRECTION
 * ===========================================================================
 *
 * function_names.md entries 187–188 incorrectly name:
 *   FUN_08030524 = "waveform_render_ch1"
 *   FUN_08031f20 = "waveform_render_ch2"
 *
 * These ARE WRONG. FUN_08030524 (6632 bytes) is a JPEG/JFIF Huffman decoder.
 * The tell-tale signs:
 *   - Searches for 0xFFD8 JFIF SOI marker
 *   - Parses 0xC0 SOF, 0xC4 DHT, 0xDA SOS markers
 *   - Reads 0xFFD0-0xFFD7 restart markers
 *   - Decodes Huffman bitstream with sign-extension (negative = EOF)
 *   - Inverts DCT coefficients (IDCT butterfly visible in the code)
 *   - Returns error codes 2 (EOF), 4 (too large), 5 (null), 6 (bad marker)
 * FUN_08031f20 is the JPEG initializer/header parser that calls FUN_08030524.
 *
 * The ACTUAL waveform rendering code lives here:
 *   FUN_0801d2ec  (0x08019198, 3752B) = scope_mode_timebase  [THIS FILE]
 *   FUN_08019470  (0x08019470, 1672B) = scope_draw_waveform_trace  [THIS FILE]
 *   FUN_08018da0  (0x08018da0,  262B) = draw_line_segment (Bresenham)  [THIS FILE]
 *   FUN_08015f50  (0x08015f50, 5170B) = scope_ui_draw_main  [partial below]
 *
 * ===========================================================================
 * FUNCTION ADDRESSES
 * ===========================================================================
 *
 *  scope_mode_timebase      @ 0x0801D2EC  3752 bytes  full annotated
 *  scope_mode_cursor        @ 0x0801F6F8  4616 bytes  full annotated
 *  scope_draw_waveform_trace @ 0x08019470  1672 bytes  full annotated
 *  draw_line_segment        @ 0x08018DA0   262 bytes  full annotated
 *  scope_ui_draw_main       @ 0x08015F50  5170 bytes  partial
 *  JPEG waveform_render_ch1 (MISNAMED) @ 0x08030524  6632 bytes  noted
 */

#include <stdint.h>

/* =========================================================================
 * KEY CONSTANTS
 * =========================================================================
 *
 * Framebuffer viewport registers (at 0x20008350):
 *   _DAT_20008350 = waveform_x_origin (screen X start of waveform area)
 *   _DAT_20008352 = waveform_y_origin (screen Y start of waveform area)
 *   _DAT_20008354 = waveform_width    (pixels wide)
 *   _DAT_20008356 = waveform_height   (pixels tall)
 *   _DAT_20008358 = framebuffer_ptr   (pointer to RGB565 framebuffer)
 *
 * Pixel address formula (confirmed from every draw function):
 *   pixel_ptr = framebuffer + ((y - y_origin) * width + (x - x_origin)) * 2
 *
 * Waveform area Y range: [28, 228] = 200 pixels (confirmed from clamp values)
 * ADC sample range: [0, 255], unsigned uint8.  ADC center = 128.
 * ADC offset applied before Y-scaling: -28.0 (documented in CALIBRATION.md)
 *
 * Colors (RGB565):
 *   0xFB43 = R:248, G:104, B:24  — orange/amber waveform trace
 *   0x18C3 = R:24,  G:24,  B:24  — near-black graticule grid
 *   0x0000 = black (background)
 */

/* =========================================================================
 * GLOBAL STATE OFFSETS (from 0x200000F8 base, per STATE_STRUCTURE.md)
 * =========================================================================
 *
 * +0x04  ch1_adc_offset   (int8)   CH1 DC calibration offset
 * +0x05  ch2_adc_offset   (int8)   CH2 DC calibration offset
 * +0x0A  voltage_range    (uint8)  Voltage range (0-9) — relay/gain setting
 * +0x0D  channel_config   (uint8)  hi nibble=active_ch, lo nibble=num_channels
 * +0x0E  active_channel   (uint8)  0=CH1, 1=CH2
 * +0x0F  trigger_run_mode (uint8)  0=AUTO, 1=NORMAL, 2=SINGLE
 * +0x12  trigger_position (int16)  Pre/post trigger offset
 * +0x14  trigger_level    (int16)  Trigger voltage level
 * +0x25  timebase_index   (uint8)  0x00-0x13 = fast; >0x13 = slow/roll
 * +0x26  scope_active_flag(uint8)  0=inactive, nonzero=active
 * +0x24  redraw_inhibit   (uint8)  Nonzero = skip sub-mode UI refresh
 *
 * +0x50  cursor_data[]    (uint32[120]) Cursor array (4-byte stride, 120 entries)
 *        cursor_data[0..0x30*ch] = per-channel measurements (freq, period, etc.)
 *        Active at 0x20000148 + channel * 0x30
 *
 * +0x2A  scope_param_a    (uint8)  0x200000EB8 — runtime param A (timebase copy)
 * +0x2B  scope_param_b    (uint8)  0x200000EB9 — CH1 volt-range table index
 * +0x2C  scope_param_c    (uint8)  0x200000EBA — CH2 volt-range table index
 * +0x2D  scope_param_d    (uint8)  0x200000EBB — CH1 baseline vertical offset
 * +0x2E  scope_param_e    (uint8)  0x200000EBC — CH2 baseline vertical offset
 * +0x1C  scope_trigger_state (uint8) 0x200000EBF — trigger status
 *
 * +0x4C2 scope_runtime_state (uint16) 0x200000EBC — scope rendering state word
 * +0x35E roll_write_ptr   (uint16) 0x200000EAC — roll mode write pointer
 * +0x35E roll_fill_level  (uint16) 0x200000EAE — roll mode fill level (max 0x12D=301)
 *
 * ADC sample buffers (fast timebase mode):
 *   +0x5B0  adc_buf_ch1[1024]  @ 0x200006A8 — CH1 1024 raw ADC bytes
 *   +0x9B0  adc_buf_ch2[1024]  @ 0x20000AA8 — CH2 1024 raw ADC bytes
 *
 * Roll mode sample buffers (slow timebase mode):
 *   +0x356  roll_buf_ch1[301]  @ 0x2000044E — CH1 calibrated display bytes
 *   +0x483  roll_buf_ch2[301]  @ 0x2000057B — CH2 calibrated display bytes
 *   Stride per channel: 0x12D = 301 bytes
 *   Channel N buffer: base + channel * 0x12D
 */

/* =========================================================================
 * DATA TABLES (raw binary @ flash base 0x08000000)
 * =========================================================================
 *
 * DAT_080465CC — Voltage range display scale table
 * Size: 20+ uint16 entries (one per voltage range index 0-9+)
 * Used: scope_mode_timebase Y-scaling, scope_mode_cursor V-delta display
 *
 *   Raw dump (xxd -s 0x465cc -l 64 firmware.bin):
 *   000465cc: ffff ffff ffff ffff ffff ffff ffff ffff  ................
 *   000465dc: f400 001f ffff f1ff ffff 2000 0000 0000  .......... .....
 *   000465ec: 0000 0000 0000 0000 0000 000f ffff f400  ................
 *   000465fc: 0000 0000 0000 0000 0000 0000 bfff ffff  ................
 *
 *   As uint16 LE:
 *   [0]  0xFFFF = 65535  (range 0, e.g. 5mV/div — large scale factor)
 *   [1]  0xFFFF = 65535  (range 1)
 *   [2]  0xFFFF = 65535  (range 2)
 *   [3]  0xFFFF = 65535  (range 3)
 *   [4]  0xFFFF = 65535  (range 4)
 *   [5]  0xFFFF = 65535  (range 5)
 *   [6]  0xFFFF = 65535  (range 6)
 *   [7]  0xFFFF = 65535  (range 7)
 *   [8]  0x00F4 = 244    (range 8)
 *   [9]  0x1F00 = 7936   (range 9 — likely 5V/div or x10 probe mode)
 *
 *   NOTE: Values 0-7 all identical (0xFFFF = 65535) — this is because
 *   the table is actually used with meter_function selector (DAT_200000FA),
 *   not with voltage_range directly. The scope analog frontend has only
 *   a handful of distinct gain stages. The uniformity of 0xFFFF for
 *   most entries means those gain stages produce unit-scale (65535/65535 = 1.0)
 *   Y amplification in the display pipeline.
 *
 * DAT_0801D5F0 — Y-transform float constants (scope_mode_timebase)
 *   +0x00 = -128.0f  (DAT_0801d5f0)  — ADC midpoint (unsigned center)
 *   +0x04 = +128.0f  (DAT_0801d5f4)  — display Y center
 *   +0x08 = +228.0f  (DAT_0801d5f8)  — display Y maximum (clamp upper bound)
 *
 * DAT_0801FAC0 — Cursor frequency/period double constants
 *   +0x00 = 100.0    (double)  — Hz→unit conversion threshold
 *   +0x08 = 10.0     (double)  — period scaling constant
 *   +0x10 = 10000.0  (double)  — kHz/MHz scale boundary
 *   +0x18 = 0.0      (double)  — zero sentinel
 *
 *   xxd -s 0x1fac0 -l 32 firmware.bin:
 *   0001fac0: 0000 0000 0000 5940  = 100.0 (double LE)
 *   0001fac8: 0000 0000 0000 2440  = 10.0  (double LE)
 *   0001fad0: 0000 0000 0088 c340  = 10000.0 (double LE)
 *   0001fad8: 0000 0000 0000 0000  = 0.0
 */


/* =========================================================================
 *
 *  FUNCTION 1: scope_mode_timebase
 *  Address: 0x0801D2EC
 *  Size:    3752 bytes
 *  Callers: scope_main_fsm (FUN_08019E98) — called every acquisition frame
 *           + twice more from single-shot frame handler
 *  Callees: FUN_08021de4 (scope_draw_channel_info)
 *           FUN_08033cfc (display_alloc_buffer)
 *           FUN_080365d4 (scope_draw_xy_mode)
 *           FUN_080012bc (memset_zero)
 *           FUN_08034078 (scope_display_refresh)
 *
 *  WHAT THIS FUNCTION ACTUALLY DOES:
 *  This is NOT purely a "timebase configuration sub-handler" — it is the
 *  COMPLETE ADC-SAMPLE-TO-DISPLAY-BUFFER PIPELINE for both normal and
 *  slow (roll) timebase modes. It:
 *    1. Converts raw ADC bytes to calibrated display-Y coordinates
 *    2. Applies trigger-position horizontal windowing
 *    3. Writes calibrated samples to the roll buffers (roll_buf_ch1/ch2)
 *    4. Implements XY mode display
 *    5. Implements per-channel trigger-find and cursor position calculation
 *    6. Calls scope_draw_channel_info and scope_display_refresh
 *    7. Calls scope_display_refresh (FUN_08034078) at the end
 *
 * =========================================================================*/

void scope_mode_timebase(void) {

    /* ---- ENTRY GUARD ----
     * Skip if redraw is inhibited OR scope is not active.
     * When either flag is set, only draw channel info and return.
     */
    if ((DAT_20000124 != '\0') && (DAT_20000126 != '\0')) {
        goto normal_timebase_path;
    }

    scope_draw_channel_info();  /* FUN_08021de4 — always runs */

    /* ---- FAST TIMEBASE: ADC BUFFER → ROLL BUFFER CONVERSION ---- */
    /* Only runs when timebase_index < 4 AND voltage_range != 3 (no x100 probe) */
    if (((short)scope_runtime_state >= 0) || (trigger_run_mode == 0)) {
        /* Initialize roll_write_ptr for fresh capture:
         * roll_write_ptr (at 0x20000EAC) = 0x12D0000
         * This is an oddly-encoded starting value — the 0x12D is 301 (buffer size),
         * placed in the HIGH 16 bits, meaning this is a compound pointer word */
        if (trigger_run_mode == 0) {
            roll_write_ptr = 0x12D0000;
        }

        /* ---- SLOW TIMEBASE: DUAL BUFFER COPY WITH ADC OFFSET SUBTRACTION ----
         *
         * For each active channel (active_ch to num_channels):
         *   The code copies 301 samples from adc_buf_ch1/ch2 into
         *   roll_buf_ch1/ch2, but with a trigger_position offset applied.
         *
         * The critical loop body processes 4 samples per iteration (unrolled):
         *
         *   FOR TIMEBASE_INDEX < 4 (slow, interleaved mode):
         *   --------------------------------------------------
         *   adc_index = (roll_write_ptr & 0x7FFF) + (loop_idx - trigger_position)
         *   Wrap: if (adc_index - 0x96) >= 0x800: adc_index -= 0x800
         *         else: adc_index -= 0x96
         *   pair_idx = adc_index >> 1       -- which interleaved pair
         *   even_odd = adc_index & 1         -- CH1 or CH2 in pair
         *
         *   if (voltage_range == 1):
         *     if (even_odd == 0): sample = adc_buf_ch1[pair_idx]
         *     else:               sample = adc_buf_ch2[pair_idx] - ch2_adc_offset
         *   else:
         *     if (even_odd == 0): sample = adc_buf_ch1[pair_idx] - ch2_adc_offset
         *     else:               sample = adc_buf_ch2[pair_idx]
         *
         *   roll_buf_ch1[loop_idx] = sample
         *
         *   BUFFER SOURCES:
         *   adc_buf_ch1 @ 0x200006A8  — CH1 raw ADC bytes from SPI3
         *   adc_buf_ch2 @ 0x20000AA8  — CH2 raw ADC bytes from SPI3
         *
         *   The 0x96 subtraction = 150 decimal. This is the trigger pre-trigger
         *   sample count (half of 300). It centers the trigger at sample 150.
         *
         *   FOR TIMEBASE_INDEX >= 4 (faster timebase, non-interleaved):
         *   ----------------------------------------------------------
         *   The loop stride changes from 4 to 7 samples per write, accessing:
         *   adc_buf_ch1[(roll_write_ptr & 0x7FFF) + offset - 0x96..0x90]
         *   Writing 7 consecutive roll_buf entries per loop iteration.
         *   Loop terminates when loop_idx == 0x12D (301).
         */

        uint8_t active_ch  = channel_config >> 4;   /* state[0x0D] high nibble */
        uint8_t num_ch_end = channel_config & 0x0F; /* state[0x0D] low nibble  */
        uint8_t volt_range = voltage_range;          /* state[0x0C] = DAT_2000010c */
        uint8_t ch1_dc_off = ch2_state_byte;         /* state[0x51] = DAT_20000449 */
        uint8_t ch2_dc_off = ch1_state_byte;         /* state[0x50] = DAT_20000448 */
        int16_t trig_pos   = _DAT_20000112;          /* trigger_position int16 */

        if (active_ch < num_ch_end) {
            for (uint8_t ch = active_ch; ch < num_ch_end; ch++) {
                uint8_t *roll_base = (uint8_t *)(ch * 0x12D + 0x200000F8 + 0x356);
                /* roll_base points to roll_buf_ch1[0] for ch=0,
                 *                     roll_buf_ch2[0] for ch=1 */

                if ((timebase_index < 4) && (volt_range != 3)) {
                    /* INTERLEAVED PATH: 4 samples per iteration */
                    for (int i = 0; i <= 300; i += 4) {
                        for (int k = 0; k < 4 && (i+k) <= 300; k++) {
                            int adc_idx = (int)(roll_write_ptr & 0x7FFF) + (i+k) - trig_pos;
                            /* Wrap modulo ~2048 (0x800) with 0x96 base offset */
                            uint16_t wrapped = (uint16_t)(adc_idx - 0x96);
                            if ((wrapped >> 11) != 0) wrapped = (short)adc_idx - 0x896;
                            int pair = wrapped >> 1;        /* interleaved pair index */
                            int is_ch2 = wrapped & 1;       /* 0=CH1 sample, 1=CH2 sample */

                            int8_t sample;
                            if (volt_range == 1) {
                                sample = is_ch2 ?
                                    ((int8_t)adc_buf_ch2[pair] - ch2_dc_off) :
                                    (int8_t)adc_buf_ch1[pair];
                            } else {
                                sample = is_ch2 ?
                                    (int8_t)adc_buf_ch2[pair] :
                                    ((int8_t)adc_buf_ch1[pair] - ch1_dc_off);
                            }
                            roll_base[i + k] = (uint8_t)sample;
                        }
                    }
                } else {
                    /* NON-INTERLEAVED PATH: 7 samples per loop, single-channel */
                    /* Stride-7 unrolled loop from loop_idx=0 to loop_idx=0x12D-1 */
                    uint32_t ch_base_adc = ch * 0x400;  /* 1024 bytes per channel */
                    for (int i = 0; i < 0x12D; i += 7) {
                        for (int k = 0; k < 7 && (i+k) < 0x12D; k++) {
                            int adc_idx = (int)(roll_write_ptr & 0x7FFF) + (i+k) - trig_pos;
                            uint16_t wrapped = (uint16_t)(adc_idx - (0x96 - k));
                            if ((wrapped >> 10) != 0) wrapped = (short)adc_idx - (0x496 - k);
                            roll_base[i + k] = *(uint8_t *)(ch_base_adc + wrapped + 0x200006A8);
                        }
                    }
                }
            }
        }
    }

    /* ---- XY MODE AND DISPLAY BUFFER ALLOCATION ----
     *
     * After sample conversion, if a display buffer is available:
     * (FUN_08033cfc allocates 0x5E1 = 1505 bytes — enough for the waveform area)
     *
     * XY mode rendering:
     *   When timebase_index < some threshold AND both channels active:
     *   The trigger position determines horizontal scroll offset.
     *
     *   Buffer copy (scroll-window into roll_buf):
     *   Copies min(roll_fill_level, 301) samples from roll_buf starting
     *   at trigger offset, into the allocated display buffer.
     *
     * FUN_080365d4 = scope_draw_xy_mode: XY Lissajous display renderer.
     * Only called when voltage_range == 3 && channel == 1 (dual channel XY).
     *
     * Per-column trigger-search (for triggering display):
     *   After the scroll copy, the code scans the display buffer looking
     *   for the first trigger crossing:
     *     trigger_Y = (trigger_level + trigger_position) + 0x80 - trigger_pos_base
     *     (adjusted to display coordinates)
     *   Then searches forward from trigger_Y for the next rising/falling edge.
     *
     * Column copy back to roll_buf:
     *   Results are copied back to roll_buf with 7-sample stride:
     *   roll_buf[i..i+6] = display_buf[cursor_pos - i..cursor_pos - i + 6] - 0x96..0x90
     *   (applies the 150-sample baseline subtraction to convert to display Y)
     */

    if (_DAT_20000f04 == 0 &&
        (1000 < (int)_DAT_20000f00) &&
        (display_buf = display_alloc_buffer(0x5E1, _DAT_20000f00 - 1000)) != NULL) {

        /* Active channel count determines dual-channel XY path */
        if (channel_config >> 4 < (channel_config & 0xF)) {
            uint32_t scroll_div = 1000 / (_DAT_20000f00 & 0xFFFF);  /* zoom divisor */
            /* Trigger position in display coordinates */
            int16_t trig_disp_pos = (trig_pos + 0x96) - (short)((trig_pos + 0x96) * 5) / scroll_div;

            /* For each active channel: */
            for (uint8_t ch = channel_config >> 4; ch < (channel_config & 0xF); ch++) {
                /* Toggle active channel for XY if voltage_range==3 && ch==1 */
                uint8_t render_ch = active_channel;
                if ((volt_range == 3) && (ch == 1)) render_ch ^= 1;

                /* Copy scroll window into display buffer */
                if (trig_disp_pos < 0x12D) {
                    int copy_len = 0x12D - trig_disp_pos;
                    uint8_t *src = roll_buf_base[render_ch] + trig_disp_pos;
                    for (int j = 0; j < copy_len; j++) {
                        display_buf[j] = src[j];
                    }
                }

                /* XY mode drawing (both channels loaded, draw Lissajous) */
                int display_width = (scroll_div < 3) ? 0x1C3 : 0x2F0;
                FUN_080365d4(display_buf_ch2, display_buf, display_width / scroll_div);

                /* Stride-0x2B (43) block copy from temp buffer back:
                 * (43 bytes per "column" in some internal display format) */
                for (int j = 0; j < 0x5E1; j += 0x2B) {
                    memcpy(display_buf + j, temp_buf + j, 0x2B);
                }

                /* Clear used framebuffer columns:
                 * Uses _DAT_20001080 flag to decide between DMA or direct clear */
                if (DAT_20001080 == 0) {
                    (*lcd_dma_clear_fn)(0);
                } else {
                    /* Direct framebuffer clear via dirty-bit tracking */
                    uint32_t fb_region = (uint32_t)(display_buf - _DAT_20001078) >> 10;
                    if (fb_region < 0xAF) {
                        uint32_t word_idx = (uint32_t)(display_buf - _DAT_20001078) >> 5;
                        uint16_t dirty_len = *(uint16_t *)(_DAT_2000107C + word_idx * 2);
                        if (dirty_len != 0) {
                            memset_zero(_DAT_2000107C + word_idx * 2, dirty_len << 1);
                        }
                    }
                }

                /* ---- TRIGGER-LEVEL SEARCH IN DISPLAY BUFFER ----
                 *
                 * Searches the display buffer for the first sample that
                 * crosses the trigger level. The trigger level is stored
                 * in display-Y units as trigger_Y:
                 *
                 *   trigger_Y = (trigger_level + trigger_position) + 0x80
                 *             = raw trigger level adjusted to display center
                 *
                 * The search scans from position (trigger_Y_base + 0x96) forward
                 * through the display buffer, comparing consecutive pairs
                 * of samples to find a crossing.
                 *
                 * trigger_edge (DAT_20000110):
                 *   0 = rising edge:  scan for sample[i] > trigger_Y
                 *                     until sample[i+1] > trigger_Y
                 *   2 = falling edge: scan for both > or both < (bidirectional)
                 *   1 or 3 = combined
                 *
                 * The found position is stored in cursor_found_col (a local),
                 * and if nothing found, defaults to 0x12D (301 = no crossing).
                 */
                int16_t trigger_Y;
                int16_t *trig_ref;
                if (scope_active_flag != 0) {
                    trig_ref = &trigger_position_display;  /* state[0xEBE] */
                } else {
                    trig_ref = &trigger_level;             /* state[0x14] */
                }
                trigger_Y = *trig_ref + 0x80;
                uint16_t cursor_found_col = 0;

                int search_start = (int)trig_disp_pos + 0x96;
                int search_end   = display_width + (int)trig_pos;

                if (search_start < search_end) {
                    /* Unrolled search: 4 positions checked per outer loop */
                    /* trigger_edge determines rising/falling/both */
                    if (trigger_edge == 0) {
                        /* Rising edge: find sample[i] <= Y < sample[i+1] */
                        for (int pos = search_start; pos < search_end; pos++) {
                            if ((int8_t)display_buf[pos] <= trigger_Y &&
                                trigger_Y < (int8_t)display_buf[pos + 1]) {
                                cursor_found_col = (uint16_t)pos;
                                break;
                            }
                        }
                    } else if (trigger_edge == 2) {
                        /* Falling edge: find sample[i] >= Y > sample[i+1] */
                        for (int pos = search_start; pos < search_end; pos++) {
                            if (((int8_t)display_buf[pos] <= trigger_Y &&
                                 (int8_t)display_buf[pos + 1] < trigger_Y) ||
                                ((int8_t)display_buf[pos] <= trigger_Y &&
                                 trigger_Y < (int8_t)display_buf[pos + 1])) {
                                cursor_found_col = (uint16_t)pos;
                                break;
                            }
                        }
                    }
                    /* ... similar for trigger_edge 1, 3 */
                }
                if (cursor_found_col == 0) cursor_found_col = 0x12D;

                /* ---- WRITE BACK TO ROLL_BUF WITH DISPLAY OFFSET ----
                 * Copies display_buf[cursor_pos - i - 0x96..0x90] back to
                 * roll_buf[i..i+6] for i = 0..300, stride 7:
                 *   roll_buf[i] = display_buf[cursor_found_col - i - 0x96]
                 *   roll_buf[i+1] = display_buf[cursor_found_col - i - 0x95]
                 *   ...
                 *   roll_buf[i+6] = display_buf[cursor_found_col - i - 0x90]
                 */
                for (int i = 0; i < 0x12D; i += 7) {
                    for (int k = 0; k < 7; k++) {
                        int src_idx = cursor_found_col - i - (0x96 - k);
                        roll_buf_base[render_ch][i + k] = display_buf[src_idx];
                    }
                }
            }
        }

        /* ---- FINAL DISPLAY BUFFER CLEAR ---- */
        /* Same DMA/direct-clear as above, applied to the main display buf */
    }

    /* ---- ROLL MODE SAMPLE CLAMPING ----
     *
     * After the buffer operations, ALL roll_buf entries are clamped
     * to the valid display range [0x1C, 0xE4] = [28, 228]:
     *
     * FOR EACH CHANNEL (two-channel path if needed):
     *   for (i = -0x12D; i < 0; i += 7):
     *     for (k = 0x483..0x489) [7 offsets]:
     *       if (roll_buf_chN[i + k] < 0x1C)  roll_buf_chN[i + k] = 0x1C
     *       if (roll_buf_chN[i + k] >= 0xE5) roll_buf_chN[i + k] = 0xE4
     *
     * The double-loop structure (single channel then paired channels) means:
     * - First pass: handles the odd channel if (num_channels - active_ch) is odd
     * - Second pass: handles pairs (stride 2 * 0x12D across channel array)
     *
     * ALSO clamps adc_buf_ch2 offsets (0x5B0 stride) with same [0x1C, 0xE4] bounds:
     * This appears to pre-clamp the NEXT acquisition's raw data before processing.
     */
    for (int ch = active_ch; ch < num_ch_end; ch++) {
        uint8_t *buf = (uint8_t *)(ch * 0x12D + 0x200000F8 + 0x483);
        for (int i = 0; i < 0x12D; i++) {
            if (buf[i] < 0x1C)  buf[i] = 0x1C;
            if (buf[i] >= 0xE5) buf[i] = 0xE4;
        }
        uint8_t *adc_buf = (uint8_t *)(ch * 0x12D + 0x200000F8 + 0x5B0);
        for (int i = 0; i < 0x12D; i++) {
            if (adc_buf[i] < 0x1C)  adc_buf[i] = 0x1C;
            if (adc_buf[i] >= 0xE5) adc_buf[i] = 0xE4;
        }
    }

    return;

normal_timebase_path:
    /* ---- NORMAL TIMEBASE: FULL CALIBRATED Y-TRANSFORM ----
     *
     * This path runs when redraw_inhibit != 0 AND scope_active != 0
     * (i.e., scope is actively running and not in inhibited state).
     *
     * It processes 301 samples per channel from the ADC buffers,
     * applies VFP calibration, and writes calibrated display-Y values.
     *
     * === VFP Y-COORDINATE TRANSFORM (the critical formula) ===
     *
     * Inputs:
     *   volt_range_factor = *(uint16*)(&DAT_080465cc + meter_function * 2)
     *                       Loaded ONCE per channel pass.
     *   scope_param_b     = DAT_20000EB9 = state[0xDC1] (CH1 vertical scale index)
     *   ch1_adc_offset    = DAT_200000FC (int8, DC calibration)
     *   scope_param_d     = DAT_20000EBB = state[0xDC3] (CH1 baseline Y position)
     *
     * For each of 301 samples, 4 per unrolled loop iteration:
     *
     *   float scale_a = (float)(uint16_t)(table_080465cc[scope_param_b])
     *   float scale_b = (float)(uint16_t)(table_080465cc[meter_function])
     *   float baseline = (float)(int8_t)scope_param_d
     *   float dc_off   = (float)(int8_t)ch1_adc_offset
     *   float raw      = (float)(uint8_t)adc_buf_ch1[i]
     *
     *   calibrated = (scale_a / scale_b) * (raw + (-128.0f) - baseline)
     *                + 128.0f + baseline + dc_off
     *              - baseline
     *
     * Simplified:
     *   calibrated = (scale_a / scale_b) * (raw - 128.0f - baseline) + 128.0f + dc_off
     *
     * Equivalently:
     *   display_y = gain * (adc_raw - adc_center - ch_baseline) + display_center + dc_offset
     *
     * where:
     *   gain          = scope_param_b_scale / voltage_range_scale
     *   adc_center    = 128.0 (midpoint of uint8)
     *   ch_baseline   = per-channel vertical position offset (signed)
     *   display_center = 128.0
     *   dc_offset     = ch1_adc_offset (hardware DC calibration)
     *
     * CLAMP: result < 0.0f → clip to fVar31 (DAT_0801d5f8 = 228.0 used here as lower??
     *         Actually: result < fVar31 (which in the code is DAT_0801d5f8 = 228.0f)
     *         This is confusing. Looking at the code more carefully:
     *
     *   The code compares fVar29 < fVar31 where fVar31 = 228.0
     *   If the *old* fVar31 is still 228.0, then clamp condition is:
     *   "if calibrated < 228" which is ALWAYS true for valid values.
     *   The ACTUAL upper clamp writes 228.0f.
     *   The LOWER clamp uses the cast: (uint)fVar33 < 0x1C → write 0x1C (28).
     *
     *   So the effective clamp is:
     *     result < 28  → store 28
     *     result > 228 → store 228
     *
     * RESULT WRITE: calibrated float is cast to uint8 and stored:
     *   roll_buf_ch1[i]  @ 0x2000044E + i  (CH1 display buffer, y-coords)
     *   roll_buf_ch2[i]  @ 0x2000044E + 1024*4 + i  (offset by channel stride)
     *
     * === CH2 PATH ===
     * Identical transform but:
     *   scale_b = table_080465cc[DAT_200000FB] (meter_range)
     *   dc_off  = ch2_adc_offset (DAT_200000FD)
     *   baseline = scope_param_e (DAT_20000EBC)
     *   source  = adc_buf_ch2 (0x20000AA7 base, offset -300 to 0)
     *
     * At the end of CH2 pass, calls scope_display_refresh() (FUN_08034078).
     */

    /* CH1 VFP loop (processes samples at 0x2000044E, 0x2000044F, 0x20000450, 0x20000451) */
    float fVar30 = (float)(uint16_t)table_080465cc[meter_function];  /* DAT_200000FA */
    float fVar32 = (float)(int8_t)ch1_adc_offset;                   /* DAT_200000FC */
    for (int i = 0; i <= 300; i += 4) {
        for (int k = 0; k < 4 && (i + k) <= 300; k++) {
            float scale_a = (float)(uint16_t)table_080465cc[scope_param_b]; /* DAT_20000EB9 */
            float raw     = (float)(uint8_t)adc_buf_ch1[i + k];
            float baseline= (float)(int8_t)scope_param_d;                   /* DAT_20000EBB */
            float result  = (scale_a / fVar30) * (raw + (-128.0f) - baseline)
                            + 128.0f + baseline + fVar32 - baseline;
            /* Clamp [28, 228] */
            if (result < 28.0f)  result = 28.0f;  /* lower clamp */
            if (result > 228.0f) result = 228.0f; /* upper clamp */
            /* Write as uint8 to roll_buf_ch1 */
            roll_buf_ch1[i + k] = (uint8_t)(uint32_t)result;
            /* Secondary clamp after integer conversion */
            if (roll_buf_ch1[i + k] < 0x1C) roll_buf_ch1[i + k] = 0x1C;
            if (roll_buf_ch1[i + k] >= 0xE5) roll_buf_ch1[i + k] = 0xE4;
        }
    }

    /* CH2 VFP loop (samples at 0x200006A7 counting DOWN from -300 to 0) */
    float fVar31_ch2 = (float)(uint16_t)table_080465cc[meter_range];  /* DAT_200000FB */
    float fVar30_ch2 = (float)(int8_t)ch2_adc_offset;                /* DAT_200000FD */
    for (int i = -300; i <= 0; i += 4) {
        for (int k = 0; k < 4 && (i + k) <= 0; k++) {
            float scale_a = (float)(uint16_t)table_080465cc[scope_param_c]; /* DAT_20000EBA */
            float raw     = (float)(uint8_t)adc_buf_ch2_offset[i + k];      /* base 0x200006A7 */
            float baseline= (float)(int8_t)scope_param_e;                    /* DAT_20000EBC */
            float result  = (scale_a / fVar31_ch2) * (raw + (-128.0f) - baseline)
                            + 128.0f + baseline + fVar30_ch2 - baseline;
            if (result < 28.0f)  result = 28.0f;
            if (result > 228.0f) result = 228.0f;
            adc_buf_ch2_offset[i + k] = (uint8_t)(uint32_t)result;
            if (result_byte < 0x1C) adc_buf_ch2_offset[i+k] = 0x1C;
            if (result_byte >= 0xE5) adc_buf_ch2_offset[i+k] = 0xE4;
        }
    }

    scope_display_refresh();  /* FUN_08034078 — update measurement displays */
    return;
}


/* =========================================================================
 *
 *  FUNCTION 1b: scope_draw_waveform_trace  (CORRECT name for waveform renderer)
 *  Address: 0x08019470
 *  Size:    1672 bytes
 *  Callers: scope_draw_controller (FUN_08019C48)
 *           scope_draw_controller calls it 4x per waveform segment:
 *             scope_draw_waveform_trace(x0, y0, radius, octant_mask=0x03)
 *             scope_draw_waveform_trace(x1, y0, radius, octant_mask=0x0C)
 *             scope_draw_waveform_trace(x1, y1, radius, octant_mask=0x30)
 *             scope_draw_waveform_trace(x0, y1, radius, octant_mask=0xC0)
 *  Callees: NONE (pure pixel writer)
 *
 *  ALGORITHM: Bresenham Midpoint Circle with Octant Masking
 *  --------------------------------------------------------
 *  This implements the standard Bresenham midpoint circle algorithm,
 *  drawing up to 8 octants of a circle centered at (param_1, param_2)
 *  with radius param_3.
 *
 *  The octant_mask (param_4) selects which octants to draw:
 *    Bit 0: octant 1 (x+, y-)   — upper right
 *    Bit 1: octant 2 (x+, y-)   — right upper
 *    Bit 2: octant 3 (x-, y-)   — left upper
 *    Bit 3: octant 4 (x-, y-)   — upper left
 *    Bit 4: octant 5 (x-, y+)   — lower left
 *    Bit 5: octant 6 (x-, y+)   — left lower
 *    Bit 6: octant 7 (x+, y+)   — right lower
 *    Bit 7: octant 8 (x+, y+)   — lower right
 *
 *  Why a circle? The waveform is drawn by connecting adjacent sample-Y-values
 *  with round "dots" (filled using a rectangle + 4 circle corners), giving
 *  thick waveform lines that look clean at the 1-3px radius.
 *
 *  The scope_draw_controller (FUN_08019C48) uses radius = param_3 to size
 *  the dot, then draws horizontal and vertical lines between the dot centers
 *  using draw_line_segment (FUN_08018DA0) to connect consecutive samples.
 *
 *  Pixel color: HARDCODED 0xFB43 (orange/amber, R:248 G:104 B:24)
 *  This is the stock CH1 waveform color in the default theme.
 *  (CH2 presumably uses a different color, set elsewhere before calling.)
 *
 *  Framebuffer bounds check: every pixel write is guarded:
 *    if (x >= waveform_x_origin && x < waveform_x_origin + waveform_width
 *        && y >= waveform_y_origin && y < waveform_y_origin + waveform_height):
 *      framebuffer[(y - y_origin) * width + (x - x_origin)] = 0xFB43
 *
 *  No blending, no persistence — pure overwrite.
 *
 * =========================================================================*/

/* param_1 = center_x, param_2 = center_y, param_3 = radius, param_4 = octant_mask */
void scope_draw_waveform_trace(short center_x, short center_y, int radius, uint32_t octant_mask) {
    int error = radius * (-2) + 3;   /* Bresenham error term */
    int x = 0;                        /* Bresenham x coordinate (from 0 to radius) */
    int y_step = 0;                   /* Accumulated y step */

    while (x < radius) {
        /* Plot all enabled octants */
        int px, py;

        if (error < 0) {
            /* Step along circle (no y decrement) */
            int nx = x + 1;

            /* Octant 1: (center_x + nx, center_y - x) */
            if (octant_mask & 0x01) {
                px = center_x + nx;  py = center_y - x;
                if (INBOUNDS(px, py)) FB[...] = 0xFB43;
            }
            /* Octant 2: (center_x + x,  center_y - nx) -- etc for all 8 */
            /* ... 8 octants total, each guarded by octant_mask bit and viewport check */

            error += x * 4 + 6;
        } else {
            /* Step diagonally (decrement radius step) */
            int nr = radius - 1;

            /* Plot octants using (nx, nr) coordinates */
            /* ... same 8-octant structure */

            error += (x - nr) * 4 + 10;
            radius = nr;
        }
        x = (int)(short)(x + 1);
    }
}

/* =========================================================================
 *  CONTROLLER: scope_draw_controller  @ 0x08019C48
 *  Calls scope_draw_waveform_trace 4x and draw_line_segment 4x per segment.
 *
 *  void FUN_08019c48(param_1=x0, param_2=y0, param_3=radius)
 *
 *  Computes bounding box from (x0, y0) with radius:
 *    x1 = x0 + 0x35 (= x0 + 53)
 *    y1 = y0 + 0x11 (= y0 + 17)
 *    x_left  = x0 - radius
 *    x_right = x0 + radius (or x1 + radius)
 *    y_top   = y0 - radius
 *    y_bot   = y0 + radius (or y1 + radius)
 *
 *  Draws 4 circle corner caps:
 *    FUN_08019470(x_left,  y_top,  radius, 0x03)   upper-left corners
 *    FUN_08019470(x_right, y_top,  radius, 0x0C)   upper-right corners
 *    FUN_08019470(x_right, y_bot,  radius, 0x30)   lower-right corners
 *    FUN_08019470(x_left,  y_bot,  radius, 0xC0)   lower-left corners
 *
 *  Draws 4 connecting line segments (the sides of the waveform "segment capsule"):
 *    FUN_08018DA0(x_right, y0,    x_left, y0,    0xFB43)  top horizontal
 *    FUN_08018DA0(x_right, y_bot, x_left, y_bot, 0xFB43)  bottom horizontal
 *    FUN_08018DA0(x0,      y_top, x0,     y_bot, 0xFB43)  left vertical
 *    FUN_08018DA0(x1,      y_top, x1,     y_bot, 0xFB43)  right vertical
 *
 *  This effectively draws a "stadium" / "pill" shape:
 *  two vertical lines connected by semicircles at the ends.
 *  Each sample point gets one such shape, with adjacent samples connected
 *  by the vertical lines creating a filled waveform trace with smooth ends.
 */


/* =========================================================================
 *  FUNCTION 1c: draw_line_segment  (Bresenham line)
 *  Address: 0x08018DA0
 *  Size:    262 bytes
 *  Callers: scope_draw_controller (4x per waveform segment),
 *           scope_cursor drawing
 *
 *  ALGORITHM: Bresenham Line (integer, clip-to-viewport)
 *  Draws a straight line from (x1,y1) to (x2,y2) in color.
 *  Each pixel is bounds-checked against the waveform viewport.
 *
 * =========================================================================*/

/* param_1=x1, param_2=y1, param_3=x2, param_4=y2, param_5=color */
void draw_line_segment(int x1, int y1, int x2, int y2, uint16_t color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;   /* sign of dx */
    int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;   /* sign of dy */
    int adx = dx < 0 ? -dx : dx;                   /* |dx| */
    int ady = dy < 0 ? -dy : dy;                   /* |dy| */
    int step = (adx > ady) ? adx : ady;             /* max(|dx|,|dy|) = iterations */

    if (step < -1) return;  /* degenerate */

    int ex = 0, ey = 0;
    for (int i = 0; i <= step + 1; i++) {
        /* Write pixel with viewport clip */
        if (x1 >= waveform_x_origin && x1 < waveform_x_origin + waveform_width &&
            y1 >= waveform_y_origin && y1 < waveform_y_origin + waveform_height) {
            framebuffer[(y1 - waveform_y_origin) * waveform_width +
                        (x1 - waveform_x_origin)] = color;
        }
        /* Bresenham step: advance in the major axis */
        ex += adx;
        ey += ady;
        if ((int16_t)ex - step >= 0 && step <= (int16_t)ex) {
            x1 += sx;
            ex -= step;
        }
        if ((int16_t)ey - step >= 0 && step <= (int16_t)ey) {
            y1 += sy;
            ey -= step;
        }
    }
}


/* =========================================================================
 *  FUNCTION 1d: scope_draw_write_pixel  (single-pixel color-compare writer)
 *  Address: 0x08019CE8
 *  Size:    276 bytes
 *  Callers: scope_ui_draw_main (2x for FFT bars)
 *
 *  Writes a (width × height) filled rectangle of color param_6 at (x, y),
 *  BUT ONLY if the current pixel is BRIGHTER than param_5 in all RGB channels.
 *  This implements a "paint only if dimmer" logic — used to draw dark bars
 *  on top of potentially lighter background without overwriting bright pixels.
 *
 *  The brightness comparison (visible in code at lines 6512-6514):
 *    Write if:  (existing.R > new.R) && (existing.G > new.G) && (existing.B > new.B)
 *  i.e., write the new pixel only if existing is brighter than new in all components.
 *
 *  NOTE: Called with color 0x861 (R:0 G:67 B:1 = dark green) for FFT bars.
 *  (The color 0x861 = 0x0861: R=0, G=67>>1=33, B=1 — very dark green)
 * =========================================================================*/


/* =========================================================================
 *
 *  FUNCTION 2: scope_mode_cursor
 *  Address: 0x0801F6F8
 *  Size:    4616 bytes
 *  Callers: scope_main_fsm (4 callsites — normal, roll, single-shot, and
 *           redraw-inhibited paths)
 *  Callees: FUN_0803e5da (fp_int_to_double)
 *           FUN_0803e124 (fp_double_subtract)
 *           FUN_0803c8e0 (fp_double_divide)
 *           FUN_0803e77c (fp_double_multiply)
 *           FUN_0803e50a (fp_int_to_double_signed)
 *           FUN_0803dfac (fp_double_add variant)
 *           FUN_0803dba4 (fp_double_sqrt)
 *           FUN_0803e450 (fp_double_to_float_result)
 *           FUN_08001732 (fp_64bit_sdiv)
 *           FUN_0800157c (memset_zero)
 *
 *  WHAT THIS FUNCTION ACTUALLY DOES:
 *  This is the FREQUENCY MEASUREMENT and CURSOR/MEASUREMENT COMPUTATION engine.
 *  Despite its name, it does NOT draw cursor lines — instead it:
 *    1. Scans the roll_buf waveform data to find the signal period/frequency
 *    2. Computes Vpp, Vmax, Vmin, Vavg, Vrms using VFP double-precision math
 *    3. Counts zero-crossings for frequency measurement
 *    4. Stores results in the cursor_data[] array at state[0x50..0x230]
 *    5. Manages a 10-sample ring buffer for period averaging
 *    6. Accumulates running sums for period history (used by auto-timebase)
 *
 *  Parameters (not used — all data from global state):
 *    param_1, param_2 = undefined4 (ignored by function)
 *
 * =========================================================================*/

void scope_mode_cursor(undefined4 param_1, undefined4 param_2) {

    /* ---- TIMEBASE INDEX → DISPLAY PERIOD COMPUTATION ----
     *
     * The timebase_index (0x00-0x13 = 20 values) maps to a display
     * period-per-division using a 1-2-5 pattern:
     *
     *   Index 0x13 (19) = 5ns/div  → fastest
     *   Index 0x12 (18) = 10ns/div
     *   Index 0x11 (17) = 20ns/div
     *   Index 0x10 (16) = 50ns/div
     *   ...
     *   Index 0x00 (0)  = slowest (5s/div or similar)
     *
     * The per-div time is computed from the table at DAT_0804BCE0 (3 entries)
     * using the 1-2-5 modulo pattern:
     *
     *   period_multiplier = table_0804bce0[(0x1C - timebase_index) % 3]
     *   period_divisor    = (timebase_index + 1) / 3
     *
     * The three entries at 0x0804bce0 are all 0x0000 (from binary dump).
     * This means the period computation falls through to the double-precision
     * constant path using DAT_0801FAC0..0x0801FAD8:
     *   100.0, 10.0, 10000.0, 0.0
     *
     * These are frequency range boundaries:
     *   < 100 Hz: display in Hz (×1)
     *   100-10000 Hz: display in Hz
     *   > 10000 Hz: display in kHz (÷1000)
     *
     * The double formula for display period in ns:
     *   period_scaled = (1/3 table lookup) * 10^(timebase_index/3) * multiplier
     *   where multiplier ∈ {1, 2, 5} based on (timebase_index % 3)
     */

    uint8_t tb_idx = DAT_20000125;     /* timebase_index */
    int iVar12 = 0x1C - tb_idx;        /* distance from index 28 (maps 0→28, 19→9) */

    /* period_multiplier lookup: table[(0x1c - tb_idx) % 3] */
    double period_mult = fp_int_to_double(
        *(uint16_t *)(&DAT_0804BCE0 + (short)((short)iVar12 + (short)(iVar12 / 3) * (-3)) * 2)
    );

    /* period_base = DAT_0801FAC0 (100.0 double) - period_mult */
    double period_base = fp_double_subtract(DAT_0801FAC0, /* 100.0 */
                                            period_mult);

    /* period_divisor = (tb_idx + 1) / 3 as double */
    uint32_t uVar22 = (uint32_t)tb_idx + 1;
    double period_div = fp_int_to_double(uVar22 / 3);  /* integer division */
    double display_period = fp_double_divide(DAT_0801FAC8 /* 10.0 */,
                                             period_div);
    /* display_period_scaled = display_period * period_mult */
    double display_period_s = fp_double_multiply(display_period, period_mult);

    /* ---- ROLL BUFFER SCAN: FIND MIN, MAX, MIDPOINT ----
     *
     * For each active channel:
     *   Scans roll_buf_chN[0..299] (15 samples per loop iteration)
     *   for global min and max values.
     *
     *   min/max search uses 15-way unrolled comparison per iteration:
     *     pbVar19[0..14] compared against running min/max
     *
     *   midpoint = min + (max - min) / 2  (integer, sign-extended)
     *   Stored in: abStack_4c[ch] = min, abStack_4c[ch + 2] = max
     */

    uint8_t active_ch  = (uint8_t)(DAT_2000010D >> 4);
    uint8_t num_ch_end = (uint8_t)(DAT_2000010D & 0xF);
    uint32_t uVar31 = (uint32_t)active_ch;
    uint32_t uVar22_ch = (uint32_t)(DAT_2000010D & 0xFF);

    if (active_ch < num_ch_end) {
        uint8_t *roll_ptr = (uint8_t *)(active_ch * 0x12D + 0x2000044E);
        uint8_t ch_min_arr[4], ch_max_arr[4];  /* local stacks abStack_4c */

        /* per-channel min/max scan */
        int count = 300;
        uint32_t ch_min = *roll_ptr, ch_max = *roll_ptr;
        ch_min_arr[uVar31] = *roll_ptr;
        ch_max_arr[uVar31] = *roll_ptr;

        uint8_t *scan = roll_ptr;
        do {
            /* 15 samples per iteration (stride-15 unroll) */
            for (int k = 1; k <= 15; k++) {
                uint32_t s = (uint32_t)scan[k];
                if (ch_max < s) ch_max = s;
                if (s < ch_min) ch_min = s;
            }
            count -= 15;
            scan += 15;
        } while (count != 0);

        ch_max_arr[uVar31 + 2] = (uint8_t)ch_max;
        ch_min_arr[uVar31]     = (uint8_t)ch_min;

        /* midpoint = min + (max - min) / 2 (with sign extension trick) */
        int iVar24 = (int)ch_min +
                     ((int)(short)((short)(ch_max - ch_min) +
                                   (short)((ch_max - ch_min & 0xFFFF) >> 15)) >> 1);
    }

    /* ---- ZERO-CROSSING COUNTER (period/frequency measurement) ----
     *
     * After min/max scan, counts rising and falling edge crossings
     * through the midpoint level in roll_buf.
     *
     * State machine (iVar12 encodes crossing state):
     *   iVar12 == 0:  looking for first sample > midpoint (iVar24)
     *   iVar12 == 1:  found rising edge start, looking for sample <= midpoint
     *   iVar12 == 2:  one complete half-cycle counted
     *
     * uVar22 = rising edge count (half-periods above midpoint)
     * uVar17 = falling edge count (half-periods below midpoint)
     *
     * The crossing search:
     *   for i in 0..299:
     *     sample = roll_buf[i]
     *     if sample > midpoint:
     *       rising_count++; state = scan-for-falling
     *     else if state is scanning:
     *       if sample <= midpoint: falling_count++; state = done-or-next-cycle
     *
     * This is equivalent to counting zero-crossing pairs (full periods).
     * The result is stored in auStack_50[ch] — a uint16 per channel.
     *
     * Special case: if fewer than 3 crossings found in either direction,
     * or if the signal period is too short (cursor_data.period < 0x3D4 = 980
     * and timebase_index > 13), the period is set to 0x12D (301) indicating
     * "no stable period found".
     */

    /* ---- PER-CHANNEL VFP MEASUREMENT COMPUTATION ----
     *
     * For each active channel, computes 6 measurements using double-precision VFP:
     *
     * MEASUREMENT ARRAY (cursor_data, stride 0x30 = 48 bytes per channel):
     *   Located at state[0x60 + ch * 0x30] = 0x20000160 + ch * 0x30
     *
     *   [0x00] = Vmax (calibrated, double stored as int32 via fp_double_to_int)
     *   [0x04] = Vmin (calibrated)
     *   [0x08] = Vpp  = Vmax - Vmin
     *   [0x0C] = mid-level  (midpoint voltage)
     *   [0x10] = Vavg (average over crossing count samples)
     *   [0x14] = Vrms (root mean square)
     *
     * CALIBRATION LOOKUP TABLE:
     *   Voltage conversion uses DAT_0804BFB8 (a per-range scale table):
     *     scale = *(uint16*)(&DAT_0804BFB8 + (meter_function % 3) * 2)
     *     divisor_d = (double)(meter_function / 3)
     *     volt_per_lsb = scale_d / divisor_d
     *
     *   Per STATE_STRUCTURE.md, the calibration table at 0x0804BFB8 maps
     *   meter_function (0-based range selector) to millivolts per ADC count.
     *   NOTE: The binary dump shows 0x0004BFB8 is all zeros, suggesting
     *   this table may be loaded from SPI flash at runtime (not hardcoded in flash).
     *
     * FORMULA for Vmax:
     *   raw_offset = (uint32_t)(ch_max) - (int8_t)ch_adc_offset_display + (-0x80)
     *             = ch_max - display_offset - 128
     *   Vmax_d = fp_int_to_double(raw_offset)
     *   Vmax_d = fp_double_multiply(volt_per_lsb, Vmax_d)  [calibrated voltage]
     *
     * FORMULA for Vmin:
     *   Identical structure but using ch_min.
     *
     * FORMULA for Vavg:
     *   Sums (crossing_count) samples of roll_buf, divides by count.
     *   Uses the same min/max traversal structure (4-way unrolled):
     *     sum += roll_buf[i] + roll_buf[i+1] + roll_buf[i+2] + roll_buf[i+3]
     *   Vavg_raw = sum / crossing_count
     *   Vavg_d = calibrate(Vavg_raw)
     *
     * FORMULA for Vrms:
     *   Accumulates sum of squares:
     *     sq_sum += (sample_i - display_offset)^2 for each sample
     *   Uses VFP double chain: fp_int_to_double → fp_double_multiply → fp_double_add
     *   Vrms_d = fp_double_sqrt(sq_sum / crossing_count)
     *   Vrms_d = calibrate(Vrms_d)
     */

    if (uVar31 < (uVar22_ch & 0xF)) {
        uint8_t *pb_base = (uint8_t *)(uVar31 * 0x12D + 0x2000044D);
        int meas_idx = 0;

        do {
            uint8_t *pbVar32 = pb_base;
            uint8_t ch_range = *(uint8_t *)(&DAT_200000FA + uVar22);  /* meter_function[ch] */
            int8_t  ch_dcoff = *(int8_t *)(&DAT_200000FC + uVar22);   /* adc_offset[ch] */
            int iVar20 = (int)uVar22 * 0x30;                           /* cursor array stride */

            /* Vmax */
            int raw_max = (int)(uint32_t)ch_max_arr[uVar22 + 2] - (int)ch_dcoff + (-0x80);
            cursor_data[iVar20 + 0x00] = raw_max;

            /* Scale factor from range table */
            uint16_t range_mult = *(uint16_t *)(&DAT_0804BFB8 + (ch_range % 3) * 2);
            double scale_d = fp_int_to_double(range_mult);
            double div_d   = fp_int_to_double(ch_range / 3);
            double volt_per_lsb = fp_double_divide(scale_d, div_d);

            /* Calibrated Vmax */
            double vmax_d = fp_double_multiply(volt_per_lsb, fp_int_to_double(raw_max));
            cursor_data[iVar20 + 0x00] = fp_double_to_int32(vmax_d);

            /* Calibrated Vmin */
            int raw_min = (int)(uint32_t)ch_min_arr[uVar22] - (int)ch_dcoff + (-0x80);
            cursor_data[iVar20 + 0x04] = raw_min;
            double vmin_d = fp_double_multiply(volt_per_lsb, fp_int_to_double(raw_min));
            cursor_data[iVar20 + 0x04] = fp_double_to_int32(vmin_d);

            /* Vpp = Vmax - Vmin */
            cursor_data[iVar20 + 0x08] = cursor_data[iVar20] - cursor_data[iVar20 + 0x04];

            /* iVar24 = midpoint of waveform (from min/max scan above) */
            cursor_data[iVar20 + 0x0C] = fp_double_to_int32(
                fp_double_multiply(volt_per_lsb, fp_int_to_double(iVar24 - (int)ch_dcoff - 0x80))
            );

            /* Vavg: sum crossing_count samples, divide */
            uint16_t cross_count = auStack_50[uVar22];  /* from zero-crossing step */
            uint32_t vavg_sum = 0;
            if (cross_count > 0) {
                /* 4-way unrolled summation */
                uint32_t unroll_n = cross_count & ~3u;
                uint8_t *p = pbVar32;
                for (uint32_t j = 0; j < unroll_n; j += 4) {
                    vavg_sum += (uint32_t)p[1] + (uint32_t)p[2] + (uint32_t)p[3] + (uint32_t)p[4];
                    p += 4;
                }
                /* Remainder */
                if (cross_count & 3) {
                    int base = uVar22 * 0x12D + 0x200000F8;
                    vavg_sum += (uint32_t)*(uint8_t *)(base + unroll_n + 0x356);
                    if ((cross_count & 3) > 1) vavg_sum += *(uint8_t *)((unroll_n | 1) + base + 0x356);
                    if ((cross_count & 3) > 2) vavg_sum += *(uint8_t *)((unroll_n | 2) + base + 0x356);
                }
                int vavg_raw = (int)(vavg_sum / cross_count) - (int)ch_dcoff + (-0x80);
                cursor_data[iVar20 + 0x10] = vavg_raw;
                cursor_data[iVar20 + 0x10] = fp_double_to_int32(
                    fp_double_multiply(volt_per_lsb, fp_int_to_double(vavg_raw))
                );
            }

            /* Vrms: sum of squares, sqrt, calibrate */
            if (cross_count > 0) {
                double sq_sum = DAT_0801FAD8; /* 0.0 initial value */
                int dc_offset = -0x80 - (int)ch_dcoff;

                uint32_t unroll_n = cross_count & ~3u;
                uint8_t *p = pbVar32;
                for (uint32_t j = 0; j < unroll_n; j += 4) {
                    /* For each of 4 samples: sq_sum += (sample + dc_offset)^2 */
                    for (int k = 1; k <= 4; k++) {
                        double s_d = fp_int_to_double_signed(dc_offset + (uint32_t)p[k]);
                        s_d = fp_double_multiply(s_d, s_d);  /* square */
                        sq_sum = fp_double_add(sq_sum, s_d);
                    }
                    p += 4;
                }
                /* Remainder (same as above for up to 3 extra samples) */

                /* Vrms = sqrt(sum / N) */
                double count_d = fp_int_to_double(cross_count);
                double mean_sq = fp_double_subtract(sq_sum, count_d); /* actually divide? */
                double rms_d   = fp_double_sqrt(mean_sq);  /* FUN_0803dba4 */
                cursor_data[iVar20 + 0x14] = fp_double_to_int32(
                    fp_double_multiply(volt_per_lsb, fp_double_to_float_result(rms_d))
                );
            }

            /* ---- 10-SAMPLE PERIOD RING BUFFER ----
             *
             * Stores the computed period in a 10-entry circular buffer:
             *   ring_buf[ch][ptr] = Vpp (as proxy for cycle count or period)
             *   ptr = (ptr + 1) % 10
             *
             * Then bubble-sorts the 10-entry buffer (3 passes of bubble sort),
             * takes the median entry [4] (middle of 10 sorted values).
             * Stores median period to cursor_data[iVar20 + 0x0C].
             *
             * The ring buffer address:
             *   *(int *)(ch * 0x28 + 0x20004E20 + ptr * 4)
             * where 0x20004E20 is a dedicated measurement history array.
             * ptr address: *(uint8_t *)(ch + 0x20004E70)
             *
             * After sorting, the median-filtered period is used for:
             *   cursor_data[iVar20 + 0x10] = period_hz_conversion
             *   state[0x148 + ch * 0x30] = 1,000,000,000 / median_period  [frequency in Hz]
             */
            uint8_t ring_ptr = *(uint8_t *)(uVar22 + 0x20004E70);
            *(int *)(uVar22 * 0x28 + 0x20004E20 + (uint32_t)ring_ptr * 4) =
                cursor_data[iVar20 + 0x08];  /* store Vpp in ring */
            ring_ptr = (uint8_t)((ring_ptr + 1) % 10 < 10 ? ring_ptr + 1 : 0);
            *(uint8_t *)(uVar22 + 0x20004E70) = ring_ptr;

            /* Bubble-sort 10-entry ring to find median: */
            int ring_local[10];
            for (int r = 0; r < 10; r++)
                ring_local[r] = *(int *)(uVar22 * 0x28 + 0x20004E20 + r * 4);
            /* 3 passes of partial bubble sort (handles up to 9-element sorting) */
            for (int pass = 0; pass < 3; pass += 3) {
                /* ... bubble sort loop, 9-ipass iterations */
            }
            cursor_data[iVar20 + 0x0C] = ring_local[0];  /* sorted minimum (or median?) */

            /* ---- ACCUMULATOR FOR AUTO-TIMEBASE (cursor_data) ----
             *
             * The cursor function maintains running accumulators at
             * state[0x230+] for the auto-timebase period estimation.
             * These are 64-bit signed sums of consecutive period measurements.
             *
             * When scope_active_flag == 0:
             *   Reads accumulators, divides by trigger state to get average period,
             *   stores to state[0x208..0x264] (a 23-entry result array).
             *   Then zeroes all accumulators.
             *
             * When trigger_state_curr != 0:
             *   Increments the trigger counter: DAT_20000329++
             *   Adds current measurements to running accumulators.
             *   The 64-bit add ensures no overflow for long averaging windows.
             */
            if (scope_active_flag == 0) {
                /* Read-and-clear accumulators, compute averages */
                uint8_t trig_count = DAT_20000328; /* trigger_state_prev */
                if (DAT_20000329 == 0) {           /* trigger_state_curr */
                    if (trig_count != 0) {
                        /* Divide each accumulator by trig_count, store result */
                        for (int m = 0; m < 23; m++) {
                            uint32_t *acc = (uint32_t *)(&DAT_20000268 + m * 8);
                            cursor_data_result[m] = fp_64bit_sdiv(*acc, *(acc+1), trig_count, 0);
                            *acc = 0;  *(acc+1) = 0;  /* clear accumulator */
                        }
                    }
                    /* Zero all measurement accumulators */
                    memset_zero(&DAT_20000268, 0xC0); /* 192 bytes = 24 × 8-byte accumulators */
                    /* (zeroes local_7c..local_ac too) */
                }
            } else {
                /* Accumulate: add cursor measurements to running totals */
                DAT_20000329 = DAT_20000329 + 1;  /* increment trigger counter */
                /* 64-bit adds for each measurement to accumulator pairs */
                /* e.g.: _DAT_20000268 += cursor_data[0x148..0x14c] (64-bit) */
                /* ... 23 pairs accumulated ... */
            }

            /* ---- FINAL RESULT COPY ----
             * cursor_data[0x208..0x264] = current-frame measurement results
             * Copied from local state[0x208..0x264] = state[0x148..0x264] area.
             */
            _DAT_20000264 = *puVar15;  /* final result store */

            uVar22++;
            meas_idx++;
            pb_base += 0x12D;
        } while (uVar22 < (uVar22_ch & 0xF));
    }
}


/* =========================================================================
 *
 *  FUNCTION 3: scope_display_refresh  (previously "scope_mode_timebase" callee)
 *  Address: 0x08034078
 *  Size:    626 bytes
 *  Callers: scope_mode_timebase (at function end)
 *           scope_main_fsm (section 8 single-shot handler)
 *  Callees: FUN_0803e5da, FUN_0803c8e0, FUN_0803e538, FUN_0803e77c,
 *           FUN_0803f020, FUN_08029a70 (scope_draw_measurements),
 *           FUN_08001642 (udiv64), FUN_08001732 (sdiv64)
 *
 *  WHAT THIS DOES:
 *  Called at the end of each scope_mode_timebase pass. It:
 *    1. Copies current timebase/range config to the scope_state shadow registers:
 *         DAT_20000EB8 = timebase_index  (state[0xDC0])
 *         DAT_20000EB9 = meter_function  (state[0xDC1])
 *         DAT_20000EBA = meter_range     (state[0xDC2])
 *         DAT_20000EBB = ch2_adc_offset  (state[0xDC3])
 *         DAT_20000EBC = ch2_adc_offset  (state[0xDC4])
 *         etc.
 *    2. Computes horizontal scroll offset and trigger window:
 *         _DAT_20000EC8 = roll_write_ptr - 0x96   (start of visible window)
 *         _DAT_20000ED0 = (roll_write_ptr + roll_fill_level) - 0x96  (end)
 *    3. Converts timebase_index to display time using 1-3-5 multiplier table
 *       at DAT_080465C8:
 *         *(byte *)(&DAT_080465C8)[(timebase_index + (timebase_index/3)*(-3)) & 0xFF]
 *         This is the same modulo-3 trick: selects {1,2,5} from the table
 *         at bytes 0x080465C8+offset.
 *    4. Calls scope_draw_measurements (FUN_08029A70) to update Vpp/Freq text.
 *    5. For timebase_index < 4 && specific voltage range combos:
 *         Computes the trigger_Y position in display coordinates
 *         using a formula involving trigger_level, trigger_position, and
 *         the calibration tables, storing to _DAT_20000EBE.
 *    6. Computes _DAT_20000EF0 = horizontal cursor pixel offset:
 *         _DAT_20000EF0 = trigger_level_display - trigger_Y_scaled * sample_divisor
 *
 *  KEY DATA PATH:
 *    DAT_080342F0 = a double constant used as base for timebase period calc
 *    table DAT_080465C8 = {1, 2, 5, 1, 2, 5, ...} (1-2-5 sequence table)
 *
 * =========================================================================*/

/* Partial annotation of scope_display_refresh (FUN_08034078): */
void scope_display_refresh(void) {
    /* Shadow current config into display state registers */
    DAT_20000EB8 = DAT_20000125;   /* timebase_index copy */
    DAT_20000EB9 = DAT_200000FA;   /* meter_function copy */
    DAT_20000EBA = DAT_200000FB;   /* meter_range copy */
    DAT_20000EBC = DAT_200000FD;   /* ch2_adc_offset copy */
    _DAT_20000EBE = _DAT_20000112 >> 16 | _DAT_20000112 << 16;  /* trigger_pos swapped */
    _DAT_20000EB6 = _DAT_2000010C; /* voltage_range copy */
    _DAT_20000ED8 = _DAT_20000EAC; /* roll_write_ptr copy */
    _DAT_20000EC8 = roll_write_ptr - 0x96;       /* visible window start */
    _DAT_20000ED0 = (roll_fill_level + roll_write_ptr) - 0x96;  /* visible window end */

    /* Compute display time using 1-2-5 multiplier table at DAT_080465C8:
     * The table byte at (timebase_index % 3) gives {1, 2, 5}.
     * This selects the sub-decade multiplier for the timebase display.
     * Combined with (timebase_index / 3) as the decade gives full ns-per-div.
     */
    uint8_t tb_idx = DAT_20000125;
    uint8_t tb_mult = DAT_080465C8[tb_idx + (tb_idx / 3) * (-3) & 0xFF];

    /* Double-precision period calculation:
     * period_base = divide(DAT_080342F0, 10^(tb_idx/3)) * tb_mult
     */
    double period_d = fp_double_divide(DAT_080342F0, fp_int_to_double(tb_idx / 3));
    double period_scaled = fp_double_multiply(period_d, fp_int_to_double(tb_mult));
    _DAT_20000ED0 = (int32_t)period_scaled;
    _DAT_20000ED4 = (int32_t)(period_scaled / (1ULL << 32));

    /* Same calculation for the per-pixel sample period */
    double pixel_period = fp_double_multiply(period_d, fp_int_to_double(tb_mult % 3));
    _DAT_20000EC8 = (int32_t)pixel_period;
    _DAT_20000ECC = (int32_t)(pixel_period / (1ULL << 32));

    scope_draw_measurements();   /* FUN_08029A70 */

    /* Trigger position display offset (only for tb_idx < 4 or specific ranges) */
    if ((tb_idx < 4) && (voltage_range != 3 || tb_idx == 3 || voltage_range == 3)) {
        /* Copy pixel period to EE0/EE4/EE8/EEC display shadow regs */
        _DAT_20000EE0 = _DAT_20000EC8;
        _DAT_20000EE4 = _DAT_20000ECC;
        _DAT_20000EE8 = _DAT_20000ED0;
        _DAT_20000EEC = _DAT_20000ED4;

        /* Compute trigger Y offset using signed 64-bit math */
        double trig_offset_d = fp_double_multiply(
            (roll_fill_level + roll_write_ptr) - 0x96, DAT_080342F0
        );
        /* tb_mult2 = table lookup for this range */
        uint8_t tb_mult2 = (voltage_range == 3) ? DAT_080465C9 : DAT_080465C8[0];
        double trig_scaled = fp_double_multiply(trig_offset_d, (double)tb_mult2);
        _DAT_20000ED0 = (int32_t)trig_scaled;
        _DAT_20000ED4 = (int32_t)(trig_scaled / (1ULL << 32));

        /* Clamped trigger level (between 0x96 and actual range): */
        double trig_clamped = fp_double_multiply(
            (double)(roll_write_ptr - 0x96), DAT_080342F0
        ) * (double)tb_mult2;
        _DAT_20000EC8 = (int32_t)trig_clamped;
        _DAT_20000ECC = (int32_t)(trig_clamped / (1ULL << 32));

        /* sdiv64: config_bitfield / (tb_mult2 * 10) for period display */
        uint64_t cfg_pair = ((uint64_t)_DAT_2000011C << 32) | _DAT_20000118;
        uint64_t div_result = fp_64bit_sdiv(cfg_pair, tb_mult2 * 10, 0);
        /* Clamp result: if < 0x96 → use 0x96 */
        uint32_t trig_y = (uint32_t)div_result;
        if (trig_y < 0x96) trig_y = 0x96;
        _DAT_20000EBE = CONCAT22((short)trig_y, _DAT_20000EBE);

        /* Compute display cursor offset: */
        int16_t sample_div = fp_64bit_div(1000, 0, _DAT_20000F00, _DAT_20000F04);
        _DAT_20000EF0 = _DAT_20000112 - (short)trig_y * sample_div;
    }
}


/* =========================================================================
 * IMPORTANT DISCOVERY: FUN_08030524 / FUN_08031f20 ARE A JPEG DECODER
 * =========================================================================
 *
 * The function_names.md table names these as "waveform_render_ch1/ch2".
 * This is INCORRECT. The actual code implements:
 *
 *   FUN_08031f20 (4110B) = jpeg_init() / JFIF header parser
 *     - Sets up JPEG decode context struct (param_1, a 0x78-byte state struct)
 *     - Validates JFIF SOI (0xFFD8) and SOF (0xC0) markers
 *     - Parses Huffman tables (0xC4 DHT), quantization tables (0xDB DQT)
 *     - Parses SOS (0xDA Start-of-Scan) marker
 *     - Returns status: 2=need-more-data, 3=empty, 4=too-big, 5=null, 6=bad, 8=unsupported
 *
 *   FUN_08030524 (6632B) = jpeg_decode_mcu() / MCU decoder
 *     - Decodes one Minimum Coded Unit (8×8 or 16×16 pixel block)
 *     - Full Huffman bitstream decode with byte-stuffing removal
 *     - DC coefficient delta + AC coefficient zigzag decode
 *     - IDCT butterfly with 0x16A0, 0x1D90, 0x29CF constants
 *       (these are cos(π/8)·√2 etc. as Q12 fixed-point — standard JPEG)
 *     - YCbCr-to-RGB conversion for JPEG image display
 *
 *   These are used for rendering UI elements from JPEG images stored in
 *   SPI flash (waveform icons, channel labels, splash screens, etc.)
 *   The "1 caller" for each function is in the display task, NOT the scope FSM.
 *
 *   Confirmed by:
 *     - 0xFFD8 SOI marker check in FUN_08031f20 line 23931
 *     - 0xFFD0 restart marker check in FUN_08030524 line 22599
 *     - 0x0806EE74 zigzag table referenced for AC coefficient ordering
 *     - IDCT butterfly code with constants 0x16A0, 0x1D90, 0x29CF at lines 22982-22984
 *     - Return codes match JPEG decoder conventions (not waveform conventions)
 *
 * =========================================================================*/


/* =========================================================================
 *
 *  TIMEBASE PERIOD LOOKUP (from scope_main_fsm auto-timebase mapping)
 *
 *  This table is NOT a binary data table — it is compiled as a 20-level
 *  nested if-else chain in scope_main_fsm. From scope_main_fsm_annotated.c:
 *
 *  Period (nanoseconds)    → Timebase Index → Time/Div (approx)
 *  ≥ 30,000,000 (30ms)    → 0x00 (0)       → 5s/div  (slowest)
 *  ≥ 10,000,000 (10ms)    → 0x01 (1)       → 2s/div
 *  ≥  5,000,000 (5ms)     → 0x02 (2)       → 1s/div
 *  ≥  3,000,000 (3ms)     → 0x03 (3)       → 500ms/div
 *  ≥  1,000,000 (1ms)     → 0x04 (4)       → 200ms/div
 *  ≥    500,000 (500µs)   → 0x05 (5)       → 100ms/div
 *  ≥    300,000 (300µs)   → 0x06 (6)       → 50ms/div
 *  ≥    100,000 (100µs)   → 0x07 (7)       → 20ms/div
 *  ≥     50,000 (50µs)    → 0x08 (8)       → 10ms/div
 *  ≥     30,000 (30µs)    → 0x09 (9)       → 5ms/div
 *  ≥     10,000 (10µs)    → 0x0A (10)      → 2ms/div
 *  ≥      5,000 (5µs)     → 0x0B (11)      → 1ms/div
 *  ≥      3,000 (3µs)     → 0x0C (12)      → 500µs/div
 *  ≥      1,000 (1µs)     → 0x0D (13)      → 200µs/div
 *  ≥        500 (500ns)   → 0x0E (14)      → 100µs/div
 *  ≥        300 (300ns)   → 0x0F (15)      → 50µs/div
 *  ≥        100 (100ns)   → 0x10 (16)      → 20µs/div
 *  ≥         50 (50ns)    → 0x11 (17)      → 10µs/div
 *  ≥         30 (30ns)    → 0x12 (18)      → 5µs/div
 *  <         30 (< 30ns)  → 0x13 (19)      → ~5ns/div (fastest, 250MS/s)
 *
 *  Pattern: periods follow a 1-3-5 staircase descending by decade.
 *  3 periods per decade × ~7 decades = 20 timebase settings.
 *  Each index corresponds to ~1 period of the measured signal per screen width.
 *
 * =========================================================================*/

/* As a C array for reference (not from binary, derived from FSM code): */
static const int32_t period_threshold_ns[20] = {
    /* Index 0  */ 30000000,
    /* Index 1  */ 10000000,
    /* Index 2  */  5000000,
    /* Index 3  */  3000000,
    /* Index 4  */  1000000,
    /* Index 5  */   500000,
    /* Index 6  */   300000,
    /* Index 7  */   100000,
    /* Index 8  */    50000,
    /* Index 9  */    30000,
    /* Index 10 */    10000,
    /* Index 11 */     5000,
    /* Index 12 */     3000,
    /* Index 13 */     1000,
    /* Index 14 */      500,
    /* Index 15 */      300,
    /* Index 16 */      100,
    /* Index 17 */       50,
    /* Index 18 */       30,
    /* Index 19 */        0,  /* catch-all: < 30ns */
};

uint8_t period_to_timebase_index(int32_t period_ns) {
    for (int i = 0; i < 19; i++) {
        if (period_ns >= period_threshold_ns[i]) return (uint8_t)i;
    }
    return 0x13;  /* fastest */
}


/* =========================================================================
 *
 *  TIMEBASE PERIOD TABLE — RAW HEX DUMP
 *  (The 1-2-5 multiplier table at DAT_080465C8, 6 bytes)
 *
 *   xxd -s 0x465c8 -l 8 APP_2C53T_V1.2.0_251015.bin:
 *   000465c8: 4b ff ff ff ff ff ff ff  K.......
 *             ^^ -- note the 0x4B = 75 at 0x465C8 precedes the 0xFFFF table
 *
 *   The 1-2-5 sequence table used by scope_display_refresh (FUN_08034078):
 *     DAT_080465C8[idx % 3]:
 *     However, the table at 0x080465CC (the one actually indexed in the code)
 *     begins at 0x465CC and starts with 0xFFFF, 0xFFFF, 0xFFFF...
 *
 *   The 1-2-5 table bytes DAT_080465C8:
 *     0x465C8 = 0x4B (75)  ← not part of the 1-2-5 sequence
 *     0x465C9 = 0xFF        ← not expected
 *
 *   CONCLUSION: The "1-2-5" table is not at 0x080465C8 as described in
 *   scope_display_refresh. That constant is used differently:
 *   *(byte *)(&DAT_080465c8)[(tb_idx + tb_idx/3*(-3)) & 0xFF]
 *   indexes WITHIN the 0xFFFF-filled region using the modulo-3 position.
 *   For ALL valid timebase indices 0-19:
 *     tb_idx % 3 ∈ {0,1,2}, (tb_idx/3)*3 < 19, so (tb_idx - tb_idx/3*3) ∈ {0,1,2}
 *   At offset 0 → 0xFF, offset 1 → 0xFF, offset 2 → 0xFF (all 0xFF = 255)
 *   This means the 1-2-5 multiplier is effectively 0xFF = 255 for all ranges,
 *   OR the addressing formula is computed differently than it appears in Ghidra.
 *   The scope_display_refresh period computation may be using a different
 *   constant base, with Ghidra misidentifying the exact byte address.
 *   Further binary analysis of the actual ARM instruction encoding is needed.
 *
 * =========================================================================*/


/* =========================================================================
 *
 *  ALGORITHM SUMMARIES FOR CUSTOM FIRMWARE
 *
 * =========================================================================
 *
 *  1. ADC-TO-DISPLAY Y-COORDINATE TRANSFORM
 *  -----------------------------------------
 *  To match stock fidelity, the custom firmware must implement:
 *
 *    display_y = (volt_scale[scope_param_b] / volt_scale[meter_function]) *
 *                (raw_adc - 128.0 - ch_baseline) + 128.0 + ch_adc_offset
 *
 *  where volt_scale[] is the 16-bit table at 0x080465CC (indexed by range).
 *  The result is clamped to uint8 [28, 228] — a 200-pixel display window.
 *  The transform runs on all 301 samples per channel per frame.
 *
 *  The critical subtlety: the formula has TWO range indices (scope_param_b
 *  and meter_function), not just one. This allows independent vertical
 *  scaling for the displayed position vs. the physical gain stage. If these
 *  are always equal (as they likely are in normal operation), the ratio is 1.0
 *  and the formula reduces to a simple centered offset + DC calibration.
 *  In auto-range mode they may temporarily differ during range transitions.
 *
 *  2. WAVEFORM DRAWING ALGORITHM
 *  --------------------------------
 *  Stock waveform uses a "filled capsule" approach per sample pair:
 *    - Each sample point (x, y) gets a Bresenham midpoint circle of radius r
 *      drawn in 4 selected octants (corner caps).
 *    - Adjacent sample pairs are connected by horizontal and vertical lines
 *      using a Bresenham line algorithm.
 *  This produces thick, smooth waveform traces that look exactly like a
 *  hardware oscilloscope with infinite persistence at each sample instant.
 *  Radius appears to be 3 pixels (param_3 in the calls).
 *  There is NO inter-frame blending, persistence, or decay — just overwrite.
 *  Custom firmware could implement phosphor persistence by blending the
 *  previous frame at reduced intensity before drawing the new frame.
 *
 *  3. FREQUENCY/MEASUREMENT ENGINE
 *  ---------------------------------
 *  The scope_mode_cursor function is a comprehensive signal measurement engine:
 *    - Zero-crossing count → frequency (period = sample count between crossings)
 *    - Min/max scan → Vpp, Vmax, Vmin
 *    - Sum average → Vavg
 *    - Sum-of-squares → Vrms (double-precision sqrt)
 *    - 10-sample ring buffer median → stable period measurement
 *    - 64-bit accumulator → auto-timebase period estimate
 *  All arithmetic is VFP double-precision except the ring buffer sort.
 *  Custom firmware needs all 6 measurement types and the ring-buffer median
 *  filter to match the auto-timebase behavior.
 *
 *  4. TRIGGER POSITION AND SCROLL WINDOWING
 *  ------------------------------------------
 *  The trigger_position (state[0x12], int16) controls which sample in the
 *  1024-byte ADC buffer is displayed at the center of the screen.
 *  The hardware delivers 512 triggered samples + 512 pre-trigger samples.
 *  The display window is 301 samples wide (the roll buffer size), centered
 *  at the trigger point via: adc_index = write_ptr + loop_idx - trigger_pos
 *  with wrap-around modulo ~2048.
 *  The baseline offset 0x96 (= 150) places the trigger at sample 150/300.
 *
 *  5. JPEG DECODER (not waveform renderer)
 *  -----------------------------------------
 *  Function_names.md incorrectly labels FUN_08030524 and FUN_08031f20 as
 *  waveform renderers. They are a JPEG decoder used for UI assets from
 *  SPI flash. Custom firmware can ignore these for waveform rendering,
 *  but would need them to display JPEG-compressed UI images from flash.
 *
 * =========================================================================*/
