# FNIRSI 2C53T Function Names — Complete Inventory

## Statistics
- Total functions: 362
- Named (HIGH confidence): 138
- Named (MEDIUM confidence): 182
- Named (LOW confidence): 42
- Ghidra false positives (halt_baddata): 61 (all in data region 0x0803f3b9+)

## Subsystem Summary

| Subsystem | Function Count | Key Addresses |
|-----------|---------------|---------------|
| C runtime (printf, math, memset, memcpy, strcmp) | 56 | 0x08000238–0x080017a6 |
| Analog MUX / Signal routing | 3 | 0x080018a4, 0x08001a58, 0x08001c60 |
| Meter data processing | 2 | 0x080027e8, 0x080028e0 |
| Display/font rendering engine | 5 | 0x08008154, 0x08008b90, 0x08009014, 0x080096e8, 0x08009a94 |
| Probe detection / component test | 8 | 0x0800b900–0x0800bcd4 |
| Meter UI rendering | 4 | 0x0800bd84, 0x0800bde0, 0x0800e79c, 0x0800ec70 |
| Oscilloscope mode handlers | 20 | 0x08015cfc–0x08019e98 |
| Scope sub-modes (FFT, XY, etc.) | 7 | 0x0801d2ec–0x08021de4 |
| LCD misc / line drawing | 2 | 0x08022e14, 0x08019134 |
| Timer/USART ISRs | 2 | 0x0802771c, 0x080277b4 |
| USB endpoint handler | 1 | 0x080278e4 |
| File I/O (SPI flash FAT) | 8 | 0x08028314–0x0802a534 |
| SPI flash filesystem | 7 | 0x0802a664–0x0802c250 |
| SPI flash R/W core | 15 | 0x0802ce94–0x0802f3e4 |
| Flash memory controller (FMC) | 2 | 0x0802f3e4, 0x0802f5ec |
| SPI flash init | 1 | 0x0802ef48 |
| GPIO configuration | 2 | 0x080302fc, 0x080304e0 |
| Waveform rendering | 3 | 0x08030524, 0x08031f20, 0x08032f6c |
| Display/memory management | 5 | 0x08033174–0x08033cfc |
| Scope display refresh | 3 | 0x08034070–0x080342f8 |
| FreeRTOS kernel | ~40 | 0x08034828–0x0803be38 |
| DMA/LCD init | 1 | 0x0803bee0 |
| Soft-float math library | ~30 | 0x0803c046–0x0803e8d0 |
| FP comparison/conversion | ~20 | 0x0803e8d0–0x0803f1c8 |
| Ghidra false positives (data region) | 56 | 0x0803f3b9–0x080458eb |

## Function Table

| Address | Size | Proposed Name | Confidence | Evidence | Callers | Callees |
|---------|------|---------------|------------|----------|---------|---------|
| 08000238 | 2116 | `printf_format_handler` | HIGH | Handles %d, %x, %f, %e, %g, %s, %c, %p, %n, %o format specifiers; contains exponent formatting logic | 1 | 12 |
| 080002c8 | 36 | `libc_init_locale` | MEDIUM | Called at startup (0 callers in decompile = entry point), sets locale data via __locale_ptr | 0 | 4 |
| 08000370 | 38 | `snprintf_wrapper` | MEDIUM | Calls FUN_08001092 (vsnprintf_core) with format string table, 2 callers (render paths) | 2 | 2 |
| 080003b4 | 38 | `sprintf_to_buffer` | HIGH | Confirmed from v2 decompile; 4 callers including meter and scope display functions | 4 | 2 |
| 080003e0 | 44 | `printf_pad_left` | HIGH | Pads output with '0' or ' ' based on flags; called from all printf paths | 5 | 0 |
| 0800040c | 34 | `printf_pad_right` | HIGH | Pads trailing spaces for left-aligned output; called from all printf paths | 5 | 0 |
| 0800042e | 82 | `printf_output_string` | HIGH | Outputs string with padding; handles %s format; calls pad_left and pad_right | 1 | 2 |
| 080005b4 | 388 | `vprintf_core` | HIGH | Main printf dispatch loop — parses format string, handles width/precision, calls format_handler | 1 | 2 |
| 0800073c | 88 | `memcmp` | HIGH | Byte-by-byte comparison with word-aligned fast path; classic memcmp implementation | 2 | 0 |
| 08000794 | 20 | `memchr` | HIGH | Scans buffer for byte match, returns pointer or NULL; classic memchr | 1 | 0 |
| 080007a8 | 86 | `strncpy` | HIGH | Copies string with null-check and length limit; word-aligned fast path | 1 | 1 |
| 08000800 | 124 | `strcmp` | HIGH | Word-at-a-time string comparison with SIMD-like null detection (0x80808080 pattern) | 3 | 0 |
| 0800088e | 12 | `set_errno` | HIGH | Stores value via __errno_ptr; 6 callers from math functions | 6 | 1 |
| 0800089a | 18 | `printf_sign_extend_int` | HIGH | Sign-extends char/short for signed integer formatting | 1 | 0 |
| 080008ac | 18 | `printf_zero_extend_uint` | HIGH | Zero-extends char/short for unsigned integer formatting | 1 | 0 |
| 080008be | 178 | `printf_integer_output` | HIGH | Formats and outputs integer digits with padding and sign prefix | 1 | 2 |
| 08000970 | 432 | `printf_float_to_digits` | HIGH | Converts double to decimal digit string for %f/%e/%g; uses FP multiplication tables | 1 | 5 |
| 08001092 | 32 | `vsnprintf_core` | MEDIUM | Sets up printf state struct and calls vprintf_core | 2 | 1 |
| 080010b8 | 10 | `snprintf_putchar` | MEDIUM | Stores single char to buffer and advances pointer; printf output callback | 2 | 0 |
| 08001114 | 182 | `printf_wchar_output` | MEDIUM | Handles wide character (%ls/%lc) output with locale conversion | 1 | 3 |
| 080012bc | 68 | `memset_zero` | HIGH | Zeroes memory with word-aligned fast path (32 bytes at a time); 12 callers including all display functions | 12 | 0 |
| 08001300 | 4 | `get_impure_ptr` | HIGH | Returns pointer to C library _impure_data structure (newlib __getreent) | 3 | 0 |
| 08001308 | 4 | `get_errno_ptr` | HIGH | Returns pointer to errno variable | 1 | 0 |
| 08001310 | 138 | `udiv64_by_10` | HIGH | Divides 64-bit unsigned by 10 using multiply-shift algorithm; used by printf digit extraction | 2 | 0 |
| 0800139c | 112 | `printf_special_output` | HIGH | Outputs "inf", "INF", "nan", "NAN" for special float values | 1 | 2 |
| 08001448 | 224 | `printf_pow10_table_lookup` | MEDIUM | Computes power-of-10 from table for float formatting | 1 | 2 |
| 08001534 | 72 | `printf_wchar_encode` | MEDIUM | Encodes single wide character for printf output | 1 | 1 |
| 0800157c | 78 | `memset_zero_fast` | HIGH | Fast memory zeroing (32 bytes at a time); 7 callers including FPGA task, waveform, filesystem | 7 | 0 |
| 08001620 | 10 | `get_locale_ptr` | MEDIUM | Returns locale info pointer (impure_ptr + 4) | 1 | 1 |
| 08001642 | 240 | `udiv64` | HIGH | 64-bit unsigned division; called by scope, display refresh, waveform render | 3 | 0 |
| 08001732 | 116 | `sdiv64` | HIGH | 64-bit signed division; negates operands then calls udiv64 | 4 | 1 |
| 080017a6 | 238 | `memcpy` | HIGH | Optimized memory copy with alignment handling; 5 callers | 5 | 0 |
| 080018a4 | 422 | `gpio_mux_portc_porte` | HIGH | 10-way switch writing GPIOC/E BOP/BCR (0x40011010/0x40011810) for analog relay switching; accesses DAC calibration tables at 0x20000358–0x200003bc | 2 | 0 |
| 08001a58 | 506 | `gpio_mux_porta_portb` | HIGH | Writes GPIOA/B BOP/BCR (0x40010810/0x40010C10) for analog signal routing | 2 | 0 |
| 08001c60 | 1634 | `siggen_configure` | HIGH | Signal generator setup: configures DAC registers (0x40007404/0x40007408), calls both GPIO MUX functions, sends to FreeRTOS queue | 1 | 3 |
| 080027e8 | 248 | `lcd_read_image_data` | MEDIUM | Writes LCD command 0x2E (RAMRD) then reads RGB pixel data, converts to RGB565; references 0x60020000 (LCD data) and 0x6001FFFE (LCD cmd) | 0 | 1 |
| 080028e0 | 768 | `meter_data_process` | HIGH | Processes multimeter measurement data: floating-point math with range scaling, sets DAT_20001025-20001030 (meter state), sends queue messages (0x1B, 0x1C, 0x1E) | 0 | 9 |
| 08008154 | 2612 | `display_render_engine` | HIGH | Text layout and glyph rendering engine: measures text width, handles word-wrap (checks for 0x20 space), multi-line layout with alignment (left/center/right), calls glyph_render_single; 13 callers | 13 | 4 |
| 08008b90 | 414 | `glyph_render_single` | HIGH | Renders individual glyph bitmap to framebuffer with scaling; called only by display_render_engine | 1 | 0 |
| 08009014 | 322 | `meter_mode_init_ac_voltage` | MEDIUM | Meter mode handler: calls dma1_configure, FreeRTOS task setup (xTaskCreate via 0803b3a8), display_render, measurement_dispatch; AC voltage mode | 0 | 5 |
| 080096e8 | 222 | `meter_mode_init_dc_voltage` | MEDIUM | Similar structure to 08009014; different measurement parameters — DC voltage mode | 0 | 6 |
| 08009a94 | 234 | `meter_mode_init_resistance` | MEDIUM | Similar structure; resistance measurement mode | 0 | 6 |
| 0800b900 | 1 | `jump_table_dispatch` | LOW | Jump table trampoline — indirect call through recovered jump table | 0 | 0 |
| 0800ba06 | 102 | `probe_detect_handler_1` | MEDIUM | Reads GPIOC IDR (probe pin), sends multiple queue messages (0x07/0x0A, 0x1A-0x1E) via xQueueGenericSend | 0 | 1 |
| 0800bb10 | 84 | `probe_detect_handler_2` | MEDIUM | Similar to handler_1 but sends messages 0x16-0x19; different probe detection path | 0 | 1 |
| 0800bba6 | 24 | `probe_continuity_test` | MEDIUM | Sends queue messages 0x20, 0x21 — continuity beep test | 0 | 1 |
| 0800bc00 | 42 | `probe_component_test` | MEDIUM | Sends queue messages 0x26, 0x27, 0x28 — component identification test | 0 | 1 |
| 0800bc98 | 36 | `probe_detect_handler_3` | MEDIUM | Reads GPIOC IDR, sends single queue message (0x07 or 0x0A) | 0 | 1 |
| 0800bcd4 | 18 | `mode_dispatch_indirect` | MEDIUM | Indirect call through jump table at 0x0804C0CC + param*4; mode-specific handler dispatch | 1 | 0 |
| 0800bd84 | 90 | `meter_ui_draw_header` | MEDIUM | Calls measurement_dispatch + display_render_engine with fixed coordinates (0x34, 0, 0xD8, 0x14) — meter header bar | 0 | 2 |
| 0800bde0 | 530 | `meter_ui_draw_value` | MEDIUM | Formats meter reading with sprintf_to_buffer, calls display_render with large font, shows gain/offset text | 0 | 3 |
| 0800e79c | 490 | `meter_ui_draw_range_list` | MEDIUM | Draws 6 range items in vertical list (0x18, 0x3C, 0x60, 0x84, 0xA8, 0xCC Y-coordinates); highlights selected range | 0 | 3 |
| 0800ec70 | 1798 | `meter_ui_draw_bargraph` | MEDIUM | Large meter display with Bresenham line-drawing (fVar8/fVar20 delta stepping), bargraph visualization, format string display, unit suffix | 0 | 11 |
| 08015cfc | 90 | `scope_ui_draw_header_ch1` | MEDIUM | Scope CH1 header: measurement_dispatch + display_render with CH1 string table at 0x0804C3B4 | 0 | 2 |
| 08015d58 | 500 | `scope_ui_draw_range_list_ch1` | MEDIUM | Draws CH1 range selection list (6 items), highlights current; different string table from meter | 0 | 3 |
| 08015f50 | 5170 | `scope_ui_draw_main` | MEDIUM | Main oscilloscope screen drawing: switch on mode (DAT_20001062 & 0xF), renders graticule, waveform, measurements, cursor info; 5KB monster | 0 | 8 |
| 08017514 | 90 | `scope_ui_draw_header_ch2` | MEDIUM | Scope CH2 header: same pattern as CH1 header | 0 | 2 |
| 0801819c | 198 | `scope_ui_draw_trigger_info` | MEDIUM | Scope trigger info display: uses sprintf, measurement_dispatch, display_render | 0 | 3 |
| 08018324 | 726 | `scope_ui_draw_timebase_list` | MEDIUM | Draws timebase selection list with 6 items | 0 | 2 |
| 08018964 | 394 | `lcd_draw_pixel` | MEDIUM | Sets LCD pixel via coordinate math and framebuffer write; called by line-drawing and bargraph code | 2 | 1 |
| 08018af0 | 406 | `lcd_set_window` | MEDIUM | LCD window setup: calls FP conversion (0x0803c82e) and lcd_draw_pixel for rectangular region | 1 | 2 |
| 08018cb0 | 238 | `lcd_fill_color` | LOW | LCD color fill: called by lcd_draw_pixel, writes color values to framebuffer region | 1 | 0 |
| 08018da0 | 262 | `scope_draw_grid_horizontal` | LOW | Draws horizontal grid lines for oscilloscope graticule | 1 | 0 |
| 08018ea8 | 288 | `scope_draw_grid_vertical` | LOW | Draws vertical grid lines for oscilloscope graticule; 0 callers (jump table target) | 0 | 0 |
| 08018fcc | 348 | `scope_draw_grid_dots` | LOW | Draws dotted grid pattern for oscilloscope; 0 callers (jump table target) | 0 | 0 |
| 08019134 | 500 | `lcd_fill_rect_with_color` | MEDIUM | Fills rectangular area with background color; called by meter/scope range list drawers to clear regions; calls FP conversion (0x0803d540) | 2 | 1 |
| 08019330 | 318 | `scope_draw_cursor` | LOW | Draws scope cursor/crosshair marker; 0 callers (jump table or indirect) | 0 | 0 |
| 08019470 | 1672 | `scope_draw_waveform_trace` | LOW | Waveform trace rendering: complex coordinate calculations, likely Bresenham line between sample points; 1 caller (scope_draw_controller) | 1 | 0 |
| 08019af8 | 332 | `scope_draw_trigger_marker` | LOW | Draws trigger level marker on scope display; called by FUN_08021b40 | 1 | 0 |
| 08019c48 | 160 | `scope_draw_controller` | MEDIUM | Calls scope_draw_waveform_trace and scope_draw_grid_horizontal; orchestrates scope waveform area drawing | 0 | 2 |
| 08019ce8 | 276 | `scope_draw_fft_bars` | LOW | FFT spectrum bar rendering; called by scope_ui_draw_main | 1 | 0 |
| 08019e00 | 22 | `lcd_read_data_word` | MEDIUM | Reads from LCD data register 0x60020000; called by lcd_read_image_data | 1 | 0 |
| 08019e18 | 82 | `scope_set_ch_offset` | LOW | Sets channel vertical offset value; called from scope state handler (0802a534) | 1 | 0 |
| 08019e6c | 10 | `scope_get_ch_offset` | LOW | Returns channel offset; called from scope state handler | 1 | 0 |
| 08019e78 | 22 | `scope_set_ch_coupling` | LOW | Sets channel coupling (AC/DC/GND); called from scope state handler | 1 | 0 |
| 08019e98 | 13276 | `scope_main_fsm` | HIGH | Main oscilloscope state machine: 27 callees, accesses DAC (0x40007404), GPIOC/D BOP/BCR, calls siggen, FFT, all scope sub-handlers; reads GPIOC_IDR for button/probe state | 0 | 27 |
| 0801d2ec | 3752 | `scope_mode_timebase` | MEDIUM | Scope timebase configuration sub-handler: calls display_update, lcd_draw, scope_display_refresh; manages time/div settings | 1 | 5 |
| 0801e1e4 | 2232 | `scope_mode_trigger` | MEDIUM | Scope trigger configuration: calls USB related function (08028b80), display refresh; manages trigger level/slope/mode | 1 | 4 |
| 0801eaac | 1288 | `scope_mode_measure` | MEDIUM | Scope measurement display: calls memset_zero, display_update; shows Vpp, Freq, etc. | 1 | 2 |
| 0801efc0 | 1794 | `scope_mode_math` | MEDIUM | Scope math mode (CH1+CH2, CH1-CH2, FFT): calls FP operations, display routines | 1 | 9 |
| 0801f6f8 | 4616 | `scope_mode_cursor` | MEDIUM | Scope cursor measurement: complex FP math for delta-V, delta-T calculations; calls display routines and FP library | 1 | 10 |
| 08020930 | 4558 | `scope_mode_display_settings` | MEDIUM | Scope display settings: persistence, intensity, grid style; large switch with display calls | 1 | 2 |
| 08021b40 | 516 | `scope_draw_trigger_overlay` | MEDIUM | Draws trigger level overlay on scope screen; calls scope_draw_trigger_marker | 0 | 1 |
| 08021de4 | 1492 | `scope_draw_channel_info` | MEDIUM | Draws channel info text (V/div, coupling, bandwidth); called by scope FSM and timebase handler | 2 | 0 |
| 08022e14 | 2900 | `lcd_draw_bitmap_from_flash` | LOW | Large drawing function with no callees/callers (jump table target); likely draws UI bitmaps from SPI flash addresses | 0 | 0 |
| 0802771c | 110 | `tmr3_isr` | HIGH | Timer 3 ISR: reads TMR3_INTS (0x40000410), clears flag, sends queue message (0x03), triggers PendSV for context switch | 0 | 1 |
| 080277b4 | 304 | `usart2_isr` | HIGH | USART2 ISR: reads USART2_STAT/DATA (0x40004400/0x40004404), receives 12-byte frames with 0x5A/0xA5 headers, transmits 10-byte frames; sends queue message on complete frame | 0 | 1 |
| 080278e4 | 2566 | `usb_endpoint_handler` | HIGH | USB endpoint processing: accesses 0x40005C00 (USB registers) and 0x40006000 (packet buffer); handles EP0-EP7, SET/CLEAR operations, data stage management | 0 | 3 |
| 08028314 | 42 | `gpio_port_d_set_pin` | LOW | Writes to GPIOD_BOP (0x40011410); called by fs_close helper | 1 | 0 |
| 08028b80 | 1446 | `usb_class_request_handler` | MEDIUM | Handles USB class-specific requests; large switch/case on request type; called from scope_mode_trigger | 1 | 0 |
| 0802912c | 224 | `fs_format_filename` | LOW | Formats filesystem filename string; called from fs_init_sequence (08034878) | 1 | 0 |
| 0802920c | 1396 | `fs_read_file_data` | MEDIUM | Reads file data from SPI flash: calls spi_flash_block_read, spi_flash_write, fpga_frame_builder; filesystem data access | 2 | 3 |
| 0802985c | 92 | `fs_check_path` | LOW | Filesystem path validation; called from fs_init_sequence | 1 | 0 |
| 080298c0 | 384 | `meter_coord_transform` | MEDIUM | Coordinate transformation for meter bargraph: FP operations converting measurement to screen position | 1 | 5 |
| 08029a70 | 260 | `scope_draw_measurements` | MEDIUM | Renders measurement text on scope screen: calls FP formatting, font, color operations | 2 | 6 |
| 08029b80 | 276 | `fs_read_sector` | MEDIUM | Reads sector from SPI flash: calls spi_flash_block_read + memcmp for validation | 1 | 2 |
| 08029cc4 | 220 | `fs_write_entry` | MEDIUM | Writes filesystem directory entry: calls fs_read_file_data + fpga_frame_builder | 0 | 2 |
| 08029da0 | 106 | `fs_init_header` | LOW | Initializes filesystem header block; calls USB descriptor setup (08039b24) | 0 | 1 |
| 08029e0c | 620 | `fs_open_file` | MEDIUM | Opens file on SPI flash filesystem; reads directory, locates file entry, calls fs_read_file_data | 0 | 2 |
| 0802a078 | 578 | `fs_create_file` | MEDIUM | Creates new file entry; calls USB descriptor, fpga_frame_builder, USB endpoint setup | 0 | 3 |
| 0802a2d4 | 346 | `fs_extend_cluster` | MEDIUM | Extends file allocation: reads FAT, updates cluster chain; 4 callers from filesystem operations | 4 | 2 |
| 0802a514 | 30 | `gpio_port_b_configure_pin` | MEDIUM | Configures GPIOB pin mode; called during SPI flash init (0802ef48) | 1 | 0 |
| 0802a534 | 304 | `scope_state_handler` | MEDIUM | Scope state transition handler: calls scope_set_ch_offset, scope_draw, FreeRTOS operations; manages CH1/CH2/trigger state changes | 0 | 8 |
| 0802a664 | 5748 | `fs_fat_traverse` | MEDIUM | FAT filesystem cluster chain traversal: reads sectors, follows FAT links; 5.7KB | 2 | 5 |
| 0802bde4 | 1130 | `fs_write_data` | MEDIUM | Writes data to filesystem: calls spi_flash operations | 2 | 3 |
| 0802c250 | 3138 | `fs_directory_operations` | MEDIUM | Directory create/delete/enumerate: calls FAT traverse, flash read/write; large FS operation | 2 | 7 |
| 0802ce94 | 164 | `spi_flash_read_status` | MEDIUM | Reads SPI flash status/ID registers; called by 5 flash functions | 5 | 1 |
| 0802cf38 | 68 | `spi_flash_write_disable` | MEDIUM | Sends write disable command to SPI flash; calls spi_flash_write_page | 1 | 1 |
| 0802cf7c | 64 | `spi_flash_init_controller` | LOW | Initializes SPI2 flash controller; calls FMC operations, gpio_port_d_set | 0 | 3 |
| 0802cfbc | 88 | `spi_flash_chip_detect` | MEDIUM | Detects SPI flash chip (reads JEDEC ID); calls flash_transfer, FPGA task, fs_init | 1 | 3 |
| 0802d014 | 430 | `spi_flash_read_with_cache` | MEDIUM | Cached SPI flash read: checks if sector already in buffer, reads from flash if needed; 5 callers | 5 | 3 |
| 0802d1c4 | 36 | `spi_flash_status_check` | MEDIUM | Quick status register check; called from error handler path | 1 | 0 |
| 0802d1e8 | 842 | `spi_flash_write_sectors` | MEDIUM | Multi-sector write operation: handles sector boundaries, calls flash read/write primitives | 2 | 4 |
| 0802d534 | 572 | `spi_flash_fs_sync` | MEDIUM | Filesystem sync: flushes cache, updates FAT, writes dirty sectors; 10 callees including memcpy, memset_zero | 1 | 10 |
| 0802d774 | 150 | `spi_flash_cache_invalidate` | LOW | Invalidates flash cache entries; called from error recovery path | 1 | 0 |
| 0802d80c | 172 | `spi_flash_format_check` | LOW | Checks filesystem format/signature on flash; calls fs_read_sector | 1 | 1 |
| 0802d8b8 | 904 | `spi_flash_directory_scan` | MEDIUM | Scans directory entries on SPI flash; complex loop reading and comparing entries | 5 | 8 |
| 0802dc40 | 122 | `spi_flash_alloc_cluster` | MEDIUM | Allocates new cluster from FAT free list; 3 callers | 3 | 3 |
| 0802dcbc | 610 | `spi_flash_read_dir_entry` | MEDIUM | Reads single directory entry: flash read, FAT lookup, name/attribute extraction | 2 | 4 |
| 0802df20 | 524 | `spi_flash_delete_entry` | MEDIUM | Deletes filesystem entry: marks clusters free, updates FAT; calls flash read/write | 2 | 4 |
| 0802e12c | 1026 | `spi_flash_create_entry` | MEDIUM | Creates new filesystem entry: allocates clusters, writes directory entry, updates FAT | 2 | 8 |
| 0802e530 | 652 | `spi_flash_rename_entry` | MEDIUM | Renames filesystem entry: reads old, writes new name, updates directory | 2 | 5 |
| 0802e7bc | 330 | `spi_flash_fs_operations` | MEDIUM | High-level FS dispatcher: calls alloc, read_with_cache, directory_scan, fs_sync for various FS operations | 0 | 9 |
| 0802e908 | 254 | `spi_flash_error_handler` | LOW | Error recovery for flash operations; checks status, invalidates cache | 2 | 0 |
| 0802ea08 | 242 | `spi_flash_init_fs` | MEDIUM | Initializes filesystem on SPI flash: format check, reads boot sector, sets up FAT tables | 1 | 7 |
| 0802ee9c | 76 | `spi2_sector_erase` | HIGH | SPI flash sector erase (cmd 0x20): write-enable, assert CS (GPIOB_BCR = 0x1000), sends 3-byte address, deassert CS, wait-busy | 1 | 3 |
| 0802eee8 | 96 | `spi2_read_jedec_id` | HIGH | SPI flash JEDEC ID read (cmd 0x90): assert CS, sends 0x90 + 3 dummy bytes, reads 2 response bytes | 1 | 2 |
| 0802ef48 | 256 | `spi2_init` | HIGH | Full SPI2 initialization: configures GPIOB pins (PB12-CS, PB13-CLK, PB14-MISO, PB15-MOSI), inits SPI2 peripheral at 0x40003800, reads JEDEC ID | 0 | 6 |
| 0802f048 | 112 | `spi2_block_read` | HIGH | SPI flash block read (cmd 0x03): assert CS, sends 24-bit address, reads N bytes in loop via spi2_transceive, deassert CS | 15 | 1 |
| 0802f0b8 | 10 | `spi2_receive_byte` | HIGH | Receives one byte from SPI flash: calls spi2_transceive with 0xFF dummy | 1 | 1 |
| 0802f0c4 | 86 | `spi2_transceive_byte` | HIGH | SPI2 transceive: polls SPI2_STAT (0x4000380C) TXE/RXNE flags, writes/reads SPI2_DATA | 7 | 1 |
| 0802f11c | 78 | `spi2_wait_busy` | HIGH | Polls SPI flash status register (cmd 0x05) until WIP bit clears; CS toggle wrapper | 2 | 1 |
| 0802f16c | 320 | `spi_flash_write_block` | HIGH | Writes data to SPI flash with sector erase: handles 4KB sector boundaries, reads sector into RAM, erases, modifies, writes back | 16 | 3 |
| 0802f2ac | 152 | `spi2_page_write_loop` | HIGH | Page-aligned write loop: splits writes at 256-byte page boundaries | 1 | 1 |
| 0802f344 | 38 | `spi2_write_enable` | HIGH | SPI flash write enable (cmd 0x06): assert CS, send 0x06, deassert CS; PB12 for chip select | 2 | 1 |
| 0802f36c | 118 | `spi2_page_program` | HIGH | SPI flash page program (cmd 0x02): write-enable, assert CS, send address + data bytes, deassert, wait-busy | 1 | 3 |
| 0802f3e4 | 518 | `fmc_program_flash` | HIGH | Flash memory controller operations: writes FMC_KEY (0x40022008) = 0xCDEF89AB unlock sequence, checks FMC_STAT busy, programs internal flash; accesses 0x40022000+ | 1 | 0 |
| 0802f5ec | 236 | `fmc_erase_page` | MEDIUM | FMC page erase: sets erase bit, writes address, starts operation, waits for completion | 1 | 0 |
| 0802f6d8 | 1196 | `spi_flash_fat_update` | MEDIUM | Updates FAT table entries on SPI flash: reads FAT sector, modifies chain, writes back | 5 | 4 |
| 0802ff18 | 994 | `spi_flash_cluster_io` | MEDIUM | Cluster-level I/O: reads/writes data blocks at cluster addresses, handles cluster chain following | 11 | 2 |
| 080302fc | 482 | `gpio_pin_config` | MEDIUM | Generic GPIO pin configuration: writes CRL/CRH registers based on pin mode/speed parameters; address 0x40010C00 (GPIOB) passed as param | 1 | 0 |
| 080304e0 | 14 | `gpio_read_input` | MEDIUM | Reads GPIO input data register (IDR) for specified port | 1 | 0 |
| 080304f0 | 50 | `fs_close_file` | MEDIUM | Closes filesystem handle: flushes writes (spi_flash_read_dir_entry + spi_flash_write_sectors) | 0 | 2 |
| 08030524 | 6632 | `waveform_render_ch1` | MEDIUM | Channel 1 waveform rendering: 6.6KB, processes sample buffer, draws lines between points; calls memset_zero | 1 | 1 |
| 08031f20 | 4110 | `waveform_render_ch2` | MEDIUM | Channel 2 waveform rendering: similar structure to ch1; 4.1KB | 1 | 1 |
| 08032f6c | 520 | `measurement_dispatch` | HIGH | Measurement dispatch hub: called by 11 mode handlers, calls memset_zero, display_update, waveform render, flash cache read; coordinates rendering pipeline | 11 | 6 |
| 08033174 | 196 | `display_color_fill` | LOW | Fills display buffer region with solid color; no callers (indirect) | 0 | 0 |
| 0803340c | 222 | `font_get_glyph_index` | MEDIUM | Looks up glyph index in font table; called by display_render_engine | 1 | 0 |
| 080337a0 | 880 | `spi_flash_read_sector_cached` | MEDIUM | Cached sector read with LRU: checks cache first, reads from flash on miss; 6 callers | 6 | 3 |
| 08033c6c | 80 | `display_set_clip_region` | LOW | Sets display clipping rectangle; called by scope display handler | 1 | 0 |
| 08033cbc | 62 | `display_clear_region` | MEDIUM | Clears display region: calls memset_zero; 6 callers including scope and FS handlers | 6 | 1 |
| 08033cfc | 508 | `display_alloc_buffer` | HIGH | Display buffer memory allocator: called by 16 functions; manages display render buffer pool | 16 | 0 |
| 08034070 | 8 | `fs_close_helper` | LOW | Calls gpio_port_d_set_pin; tiny wrapper for filesystem close operation | 1 | 1 |
| 08034078 | 626 | `scope_display_refresh` | MEDIUM | Scope display refresh: calls measurement text render, channel info, grid drawing, FP operations; orchestrates scope screen update | 2 | 9 |
| 080342f8 | 1292 | `scope_display_full_redraw` | MEDIUM | Full scope screen redraw: calls all drawing functions (grid, channels, measurements, cursor); 0 callers (timer/indirect) | 0 | 9 |
| 08034828 | 78 | `freertos_task_idle_hook` | LOW | FreeRTOS idle task hook; no callers (called by scheduler), no callees | 0 | 0 |
| 08034878 | 318 | `fs_init_sequence` | MEDIUM | Filesystem initialization sequence: calls display_clear, format_filename, multiple flash operations, init_fs; boot-time FS setup | 0 | 9 |
| 080349b8 | 246 | `freertos_list_insert_sorted` | HIGH | FreeRTOS list insert (sorted by item value): walks list, inserts at correct position; called by delay/timer functions | 3 | 3 |
| 08034ab0 | 272 | `freertos_timer_command` | MEDIUM | Sends command to FreeRTOS timer daemon task; called from timer API | 1 | 4 |
| 08034bc0 | 98 | `freertos_timer_pend_call` | MEDIUM | Pends function call to timer daemon; calls timer list operations | 2 | 4 |
| 08034c24 | 90 | `freertos_timer_process_expired` | MEDIUM | Processes expired software timers; walks timer list, reloads auto-reload timers | 0 | 4 |
| 08034c80 | 66 | `freertos_task_set_timeout` | MEDIUM | Sets up task timeout block; called from xQueueReceive timeout path | 1 | 1 |
| 08034cc4 | 204 | `freertos_queue_copy_item` | MEDIUM | Copies item to/from queue buffer with overwrite support; manages queue head/tail pointers | 2 | 2 |
| 08034d90 | 24 | `freertos_timer_reload` | MEDIUM | Reloads timer with new period; wrapper for timer list operations | 1 | 1 |
| 08034da8 | 38 | `freertos_task_increment_tick` | MEDIUM | Increments tick counter for FreeRTOS task; called from tick hook | 1 | 0 |
| 08034dd0 | 38 | `freertos_port_yield_from_isr` | LOW | ARM port yield: stores registers, triggers PendSV; no callers in decompile (assembly-level) | 0 | 0 |
| 08034e10 | 186 | `freertos_timer_create_static` | MEDIUM | Creates FreeRTOS static timer; initializes timer control block | 1 | 0 |
| 08034f04 | 62 | `freertos_timer_start_from_isr` | MEDIUM | Starts timer from ISR context; calls timer command dispatch | 1 | 1 |
| 08034f44 | 274 | `freertos_timer_daemon_init` | MEDIUM | Initializes timer daemon task and timer lists; called during scheduler start | 1 | 2 |
| 08035058 | 118 | `freertos_timer_pend_callback` | MEDIUM | Pends callback to timer daemon; called from ISR context | 1 | 2 |
| 080350d0 | 138 | `freertos_timer_execute_callback` | MEDIUM | Executes pended timer callback function | 1 | 1 |
| 0803515c | 198 | `freertos_timer_switch_lists` | MEDIUM | Switches active/overflow timer lists at tick overflow; processes all expired timers | 2 | 0 |
| 08035224 | 134 | `freertos_queue_item_init` | MEDIUM | Initializes queue item control block for generic queue | 2 | 1 |
| 080352ac | 40 | `freertos_list_remove_head` | MEDIUM | Removes item from head of FreeRTOS list; called by queue and timer operations | 2 | 2 |
| 080352d4 | 46 | `freertos_queue_check_space` | MEDIUM | Checks if queue has space for new item; returns 0 if full | 1 | 2 |
| 08035304 | 40 | `freertos_port_disable_interrupts` | MEDIUM | Disables interrupts and stores priority mask; ARM Cortex-M basepri manipulation | 1 | 0 |
| 0803532c | 136 | `freertos_task_create_static` | MEDIUM | Creates FreeRTOS task with static memory: initializes TCB, sets up stack, inserts in ready list | 1 | 3 |
| 080353b4 | 324 | `freertos_scheduler_start` | HIGH | Starts FreeRTOS scheduler: creates timer daemon, idle task, enables tick interrupt, enters first task; 6 callees | 0 | 6 |
| 08035504 | 154 | `freertos_task_create` | MEDIUM | Dynamic task creation: allocates TCB + stack, calls static create, adds to ready list | 0 | 5 |
| 080355a0 | 60 | `freertos_critical_nesting_inc` | MEDIUM | Increments critical nesting counter; part of critical section management | 3 | 0 |
| 080355dc | 68 | `freertos_scheduler_suspend` | MEDIUM | Suspends FreeRTOS scheduler: increments uxSchedulerSuspended counter | 2 | 2 |
| 08035620 | 206 | `freertos_scheduler_resume` | MEDIUM | Resumes FreeRTOS scheduler: processes pending ready list, triggers context switch if needed | 1 | 3 |
| 08035730 | 106 | `freertos_task_get_current` | MEDIUM | Returns current task handle and validates scheduler is running | 1 | 0 |
| 080357b8 | 178 | `freertos_ready_list_add` | HIGH | Adds task to appropriate ready list by priority; manages pxReadyTasksLists array; 4 callers | 4 | 4 |
| 0803586c | 1016 | `spi_flash_cluster_chain_read` | MEDIUM | Reads chain of clusters from SPI flash; handles cluster boundary crossing, buffering | 4 | 2 |
| 08035c64 | 540 | `freertos_timer_daemon_task` | MEDIUM | FreeRTOS timer daemon task body: waits on timer queue, processes timer commands, calls expired timer callbacks | 3 | 4 |
| 08035e80 | 44 | `freertos_task_notify_check` | LOW | Checks task notification state; 1 caller from FreeRTOS scheduler | 1 | 0 |
| 08035eac | 40 | `freertos_timer_get_period` | LOW | Returns timer period and auto-reload flag | 1 | 0 |
| 08035ed4 | 432 | `spi_flash_fs_mount` | MEDIUM | Mounts SPI flash filesystem: reads boot sector, FAT, root directory; initializes FS state | 0 | 8 |
| 08036084 | 1360 | `spi_flash_fs_format` | MEDIUM | Formats SPI flash filesystem: writes boot sector, empty FAT, root directory; called at factory reset | 0 | 8 |
| 080365d4 | 588 | `scope_draw_xy_mode` | MEDIUM | XY mode display rendering for oscilloscope; called by scope FSM and timebase handler | 2 | 0 |
| 08036820 | 14 | `spi_peripheral_reset` | MEDIUM | Resets SPI peripheral; called during SPI2 init | 1 | 0 |
| 08036830 | 10 | `spi_peripheral_enable` | MEDIUM | Enables SPI peripheral; called during SPI2 init | 1 | 0 |
| 0803683c | 10 | `spi_check_flag` | HIGH | Checks SPI status register flag bit; called in transceive polling loop | 1 | 0 |
| 08036848 | 232 | `spi_peripheral_init` | MEDIUM | Full SPI peripheral initialization: configures mode, clock, data size, NSS; called for SPI2 (0x40003800) | 1 | 0 |
| 08036934 | 282 | `fs_flush_and_sync` | MEDIUM | Filesystem flush: writes dirty sectors to SPI flash, handles file allocation; writes "RRaA" and "rrAa" signatures (FAT32 FSInfo) | 4 | 2 |
| 08037400 | 1 | `fpga_usart_tx_task` | HIGH | FreeRTOS task: infinite loop calling xQueueReceive on USART2 TX queue, constructs 10-byte TX frame, enables USART TX interrupt; accesses 0x40000000 (TMR registers) | 0 | 0 |
| 08037800 | 472 | `fpga_spi3_transfer` | HIGH | FPGA SPI3 data transfer: writes GPIOB BOP/BCR (PB6 CS toggle, 0x40010C10/0x40010C14), complex multi-byte transfer protocol with floating-point ADC data conversion; accesses SPI3 at 0x40003C00 region | 0 | 2 |
| 08039874 | 284 | `usb_endpoint_configure` | MEDIUM | Configures USB endpoint registers: writes to 0x40005C00 + ep*4; sets NAK/STALL/VALID states | 1 | 0 |
| 08039990 | 402 | `usb_endpoint_init_all` | MEDIUM | Initializes all 8+8 USB endpoints (IN and OUT): sets EP addresses 0x00-0x07, sizes, and initial states | 1 | 0 |
| 08039b24 | 908 | `usb_descriptor_handler` | MEDIUM | Handles USB descriptor requests (GET_DESCRIPTOR): serves device, config, string descriptors from RAM tables; 3 callers | 3 | 0 |
| 08039eb4 | 368 | `fpga_frame_builder` | MEDIUM | Builds USB/FPGA protocol frame: assembles multi-byte response frame from device state; 5 callers from USB and FS operations | 5 | 0 |
| 0803a024 | 72 | `freertos_list_remove` | HIGH | Removes item from FreeRTOS doubly-linked list; updates prev/next pointers, decrements count; 13 callers | 13 | 0 |
| 0803a06c | 46 | `freertos_port_enter_critical` | HIGH | Enters critical section: raises basepri, increments nesting count; 3 callers | 3 | 0 |
| 0803a09c | 14 | `freertos_port_exit_critical` | HIGH | Exits critical section: decrements nesting, restores basepri if outermost; 2 callers | 2 | 0 |
| 0803a0ac | 106 | `freertos_list_insert_end` | HIGH | Inserts item at end of FreeRTOS list (FIFO order); used for ready lists | 4 | 0 |
| 0803a118 | 58 | `freertos_list_init` | HIGH | Initializes empty FreeRTOS list: sets end sentinel, zeroes count; 11 callers from task/queue/timer creation | 11 | 0 |
| 0803a154 | 14 | `freertos_port_save_context` | MEDIUM | Saves task context for ARM port; called from context switch | 1 | 0 |
| 0803a168 | 70 | `freertos_enter_critical_from_isr` | HIGH | Enter critical section: disables interrupts via basepri, returns previous mask; 16 callers from queue/task operations | 16 | 0 |
| 0803a1b0 | 46 | `freertos_exit_critical_from_isr` | HIGH | Exit critical section: restores interrupt mask from saved value; 16 callers | 16 | 0 |
| 0803a1e0 | 230 | `freertos_timer_process_commands` | MEDIUM | Processes timer command queue: start/stop/reset/delete timer operations | 3 | 3 |
| 0803a2c8 | 98 | `freertos_queue_peek_from_isr` | MEDIUM | Non-blocking queue peek from ISR: returns front item without removing | 2 | 0 |
| 0803a32c | 100 | `freertos_task_priority_set` | MEDIUM | Sets task priority: updates TCB, moves to appropriate ready list | 1 | 4 |
| 0803a390 | 110 | `freertos_task_delay` | HIGH | Delays current task for specified ticks: removes from ready list, inserts in delayed list, triggers context switch | 0 | 3 |
| 0803a400 | 36 | `freertos_task_save_priority` | MEDIUM | Saves current task priority for priority inheritance; called before blocking on queue/mutex | 4 | 0 |
| 0803a424 | 14 | `freertos_task_restore_priority` | MEDIUM | Restores task priority after mutex release | 1 | 0 |
| 0803a434 | 66 | `freertos_task_add_to_delayed` | MEDIUM | Adds task to delay list (sorted by wake time); handles overflow list | 3 | 2 |
| 0803a478 | 82 | `freertos_list_insert_sorted_wrap` | MEDIUM | Sorted insert with value wrapping for tick overflow; calls list_insert_sorted | 1 | 2 |
| 0803a4cc | 322 | `freertos_task_unblock_all` | MEDIUM | Unblocks all tasks waiting on event: walks waiting list, moves each to ready list | 1 | 2 |
| 0803a610 | 200 | `freertos_task_check_timeouts` | MEDIUM | Checks delayed task timeouts: moves tasks whose timeout expired to ready list | 0 | 5 |
| 0803a6d8 | 178 | `freertos_timer_start` | MEDIUM | Starts software timer: calculates expiry tick, inserts in timer list | 0 | 3 |
| 0803a78c | 374 | `freertos_systick_handler` | HIGH | SysTick ISR: increments tick count, checks delayed tasks, yields if higher-priority task ready | 0 | 6 |
| 0803a904 | 16 | `freertos_port_yield` | HIGH | Triggers PendSV for context switch: writes 0x10000000 to ICSR (0xE000ED04); 7 callers | 7 | 0 |
| 0803a914 | 190 | `freertos_port_pendsv_handler` | MEDIUM | PendSV handler: saves/restores task context on ARM Cortex-M; performs actual context switch | 2 | 0 |
| 0803a9d4 | 416 | `freertos_timer_delete` | MEDIUM | Deletes software timer: removes from list, frees memory if dynamic | 1 | 3 |
| 0803ab74 | 196 | `freertos_timer_reset` | MEDIUM | Resets timer: recalculates expiry time, re-inserts in timer list | 1 | 2 |
| 0803ac38 | 184 | `freertos_timer_stop` | MEDIUM | Stops running timer: removes from active timer list | 1 | 4 |
| 0803acf0 | 536 | `xQueueGenericSend` | HIGH | FreeRTOS xQueueGenericSend: copies item to queue, unblocks waiting tasks, handles timeout; 9 callers including ISR, scope, meter | 4 | 12 |
| 0803af08 | 354 | `xQueueGenericSendFromISR` | HIGH | FreeRTOS xQueueGenericSendFromISR: non-blocking queue send from interrupt context; called by TMR3 ISR and USART2 ISR | 2 | 3 |
| 0803b06c | 46 | `freertos_queue_notify_from_isr` | MEDIUM | Task notification from ISR; lightweight IPC mechanism | 0 | 2 |
| 0803b09c | 316 | `xQueueReceiveFromISR` | HIGH | FreeRTOS xQueueReceiveFromISR: non-blocking queue receive from ISR; called by USART2 ISR | 1 | 2 |
| 0803b1d8 | 462 | `xQueueReceive` | HIGH | FreeRTOS xQueueReceive: blocking queue receive with timeout; 12 callees; used by all task loops | 1 | 12 |
| 0803b3a8 | 548 | `xTaskCreate` | HIGH | FreeRTOS xTaskCreate: allocates TCB + stack, initializes task, adds to ready list; 15 callees; called by meter mode init functions | 3 | 15 |
| 0803b5cc | 210 | `freertos_task_remove_from_event` | MEDIUM | Removes task from event wait list and adds to ready list; 3 callers from queue operations | 3 | 3 |
| 0803b6a0 | 144 | `freertos_timer_daemon_process` | MEDIUM | Timer daemon processing loop: handles queued timer commands; called from timer daemon task | 2 | 4 |
| 0803b730 | 56 | `freertos_scheduler_running_check` | MEDIUM | Returns whether scheduler is running (for assertions); 4 callers | 4 | 0 |
| 0803b768 | 20 | `freertos_queue_get_count_from_isr` | LOW | Returns number of items in queue from ISR; called from scheduler suspend | 1 | 0 |
| 0803b77c | 440 | `freertos_task_yield_if_needed` | MEDIUM | Checks if context switch is needed after unblocking a task; called after queue operations | 1 | 3 |
| 0803b934 | 248 | `freertos_queue_copy_to_front` | MEDIUM | Copies item to front of queue (for urgent messages); manages queue head pointer | 1 | 2 |
| 0803ba2c | 286 | `freertos_task_check_delayed` | MEDIUM | Processes delayed task list: unblocks tasks whose delay has expired | 1 | 2 |
| 0803bb4c | 194 | `freertos_task_unblock` | MEDIUM | Unblocks specific task: removes from wait list, adds to ready list, checks if yield needed | 7 | 2 |
| 0803bc10 | 376 | `freertos_task_switch_context` | HIGH | FreeRTOS context switch: selects highest-priority ready task, updates pxCurrentTCB; 7 callers | 7 | 6 |
| 0803bd88 | 76 | `freertos_timer_expire_check` | MEDIUM | Checks if any timers expired; called from timer daemon | 0 | 2 |
| 0803bdd4 | 100 | `freertos_timer_startup` | MEDIUM | Timer subsystem startup: creates timer daemon task and command queue | 1 | 2 |
| 0803be38 | 168 | `freertos_tick_handler` | MEDIUM | Tick processing: increments counter, checks delayed tasks, sends to timer daemon | 3 | 3 |
| 0803bee0 | 358 | `dma1_configure` | HIGH | DMA1 channel configuration: writes DMA1 registers (0x40020000+), sets NVIC priority (0xE000ED0C), enables DMA channel, configures LCD transfer from RAM (0x20008358) to LCD (0x60020000) via EXMC; sets RCU clock enable (0x40021014) | 3 | 0 |
| 0803c046 | 228 | `fp_double_from_float` | MEDIUM | Converts single-precision float to double-precision representation; handles denormals and specials | 1 | 0 |
| 0803c12c | 696 | `fp_double_multiply` | MEDIUM | Double-precision floating-point multiplication: SIMD-like partial product accumulation | 1 | 0 |
| 0803c464 | 220 | `fp_bignum_subtract` | MEDIUM | Big-number subtraction for printf float formatting; handles carry propagation | 2 | 0 |
| 0803c540 | 42 | `fp_bignum_divide` | MEDIUM | Big-number division entry: dispatches to multiply or division core | 2 | 2 |
| 0803c56a | 42 | `fp_bignum_multiply` | MEDIUM | Big-number multiplication entry: dispatches to multiply core + subtract | 2 | 2 |
| 0803c594 | 580 | `fp_bignum_multiply_core` | MEDIUM | Big-number multiplication core: multi-word multiply with carry handling | 1 | 0 |
| 0803c7d8 | 48 | `fp_double_classify` | MEDIUM | Classifies double: returns category (zero, normal, inf, nan) based on exponent/mantissa | 2 | 0 |
| 0803c808 | 38 | `fp_float_abs` | MEDIUM | Returns absolute value of float (clears sign bit) | 1 | 0 |
| 0803c82e | 176 | `fp_float_to_int_round` | MEDIUM | Converts float to integer with rounding; calls set_errno on overflow | 1 | 2 |
| 0803c8e0 | 2916 | `fp_double_divide` | HIGH | Double-precision FP division: Newton-Raphson reciprocal approximation, multi-step refinement; 17 callees including special-value handling; 7 callers | 7 | 17 |
| 0803d540 | 1540 | `fp_float_divide` | MEDIUM | Single-precision FP division: similar to double version but 32-bit; calls set_errno, float_abs | 1 | 5 |
| 0803dba4 | 122 | `fp_double_compare_special` | MEDIUM | Double comparison with NaN handling; calls set_errno and dispatch function | 1 | 2 |
| 0803dc1e | 58 | `fp_double_set_errno` | MEDIUM | Sets errno for double-precision FP exceptions | 1 | 1 |
| 0803dc58 | 248 | `fp_double_add_subtract_core` | MEDIUM | Double-precision add/subtract with alignment shift; handles different exponents | 1 | 2 |
| 0803dd50 | 28 | `fp_double_negate_then_divide` | MEDIUM | Negates double then performs division; specialized wrapper | 1 | 1 |
| 0803ddb8 | 24 | `fp_double_abs_multiply` | LOW | Takes absolute value then multiplies; specialized operation | 1 | 1 |
| 0803ddd8 | 24 | `fp_double_negate_multiply` | LOW | Negates then multiplies; specialized operation | 1 | 1 |
| 0803ddf8 | 14 | `fp_float_negate` | MEDIUM | Negates float (flips sign bit) | 1 | 0 |
| 0803de24 | 10 | `fp_float_return_zero` | LOW | Returns floating-point zero | 1 | 0 |
| 0803de34 | 10 | `fp_float_return_one` | LOW | Returns floating-point 1.0 | 1 | 0 |
| 0803de44 | 14 | `isdigit` | HIGH | Tests if character is ASCII digit ('0'-'9'); called by printf parser | 1 | 0 |
| 0803de52 | 24 | `fp_double_check_nan` | MEDIUM | Tests if double is NaN; returns non-zero if NaN | 1 | 0 |
| 0803de6a | 110 | `fp_double_to_int` | MEDIUM | Converts double to integer with truncation; sets errno on overflow | 1 | 2 |
| 0803ded8 | 34 | `libc_init_ctype_table` | MEDIUM | Returns pointer to C library ctype classification table; called during locale init | 1 | 1 |
| 0803df04 | 34 | `libc_init_decimal_point` | MEDIUM | Returns pointer to locale decimal point string; called during locale init | 1 | 1 |
| 0803df30 | 6 | `fp_double_return_identity` | LOW | Returns its input unchanged; identity function for FP pipeline | 1 | 0 |
| 0803df48 | 98 | `fp_double_to_float` | HIGH | Converts double-precision to single-precision float: handles rounding, denormals; 4 callers from meter and scope display | 4 | 1 |
| 0803dfac | 316 | `fp_double_multiply_dispatch` | MEDIUM | Double multiply with special-value dispatch (inf*0=NaN, etc.); calls display_buffer_write for result output | 5 | 1 |
| 0803e124 | 542 | `fp_double_divide_dispatch` | MEDIUM | Double division with special-value dispatch (0/0=NaN, x/inf=0, etc.); calls display_buffer_write; 10 callers | 10 | 1 |
| 0803e450 | 92 | `fp_int_to_double` | MEDIUM | Converts 32-bit integer to double-precision float | 3 | 1 |
| 0803e50a | 46 | `fp_int_to_double_signed` | MEDIUM | Converts signed 32-bit integer to double | 5 | 0 |
| 0803e538 | 162 | `scope_set_sampling_params` | MEDIUM | Sets oscilloscope sampling parameters: timebase, trigger, mode; accesses scope state variables; 3 callers from scope FSM | 3 | 0 |
| 0803e5da | 38 | `fp_double_load_from_table` | MEDIUM | Loads double constant from table in ROM; 8 callers from FP operations | 8 | 0 |
| 0803e600 | 138 | `scope_set_measure_config` | LOW | Configures measurement parameters for scope; called from scope_draw_measurements | 1 | 0 |
| 0803e68c | 144 | `fp_double_add` | MEDIUM | Double-precision addition: aligns exponents, adds mantissas, normalizes; calls display_buffer_write | 2 | 1 |
| 0803e704 | 120 | `fp_double_subtract` | MEDIUM | Double-precision subtraction: negates second operand, calls add; calls display_buffer_write | 2 | 1 |
| 0803e77c | 312 | `fp_double_multiply_full` | MEDIUM | Full double multiply with normalization and rounding; calls display_buffer_write; 12 callers | 12 | 1 |
| 0803e8d0 | 152 | `display_buffer_write` | HIGH | Display rendering pipeline hub: accepts FP results, dispatches to screen buffer; 11 callers from all FP display operations; appears to be the final render-to-framebuffer step | 11 | 0 |
| 0803e978 | 108 | `fp_double_compare_ordered` | MEDIUM | Ordered double comparison: handles NaN, returns comparison result flags | 1 | 0 |
| 0803e9e4 | 22 | `fp_double_subtract_core` | MEDIUM | Core double subtraction with borrow handling; used by division algorithm | 1 | 0 |
| 0803e9fc | 354 | `fp_double_format_output` | MEDIUM | Formats double-precision result for display output; normalizes and scales; calls display_buffer_write | 2 | 1 |
| 0803eb94 | 472 | `fp_double_add_full` | MEDIUM | Full double addition with exponent alignment, mantissa add, normalize; calls display_buffer_write; 3 callers | 3 | 1 |
| 0803ed70 | 72 | `fp_float_to_double` | HIGH | Converts single-precision float to double-precision; 5 callers including meter display | 5 | 1 |
| 0803ede2 | 14 | `fp_double_add_wrapper_1` | LOW | Thin wrapper calling fp_double_add; called from scope_display_full_redraw | 1 | 1 |
| 0803edf0 | 14 | `fp_double_add_wrapper_2` | LOW | Thin wrapper calling fp_double_add; 2 callers | 2 | 1 |
| 0803edfe | 14 | `fp_double_subtract_wrapper_1` | LOW | Thin wrapper calling fp_double_subtract | 0 | 1 |
| 0803ee0c | 14 | `fp_double_subtract_wrapper_2` | LOW | Thin wrapper calling fp_double_subtract; 4 callers | 4 | 1 |
| 0803ee1a | 6 | `nop_stub` | HIGH | Empty function (just returns); possibly removed debug code or placeholder | 0 | 0 |
| 0803ee20 | 90 | `scope_read_adc_sample` | LOW | Reads ADC sample value; called by scope_main_fsm | 1 | 0 |
| 0803ee7a | 136 | `fp_float_to_double_norm` | MEDIUM | Float-to-double with normalization for denormals; helper for fp_float_to_double | 2 | 0 |
| 0803ef06 | 26 | `libc_init_atexit` | MEDIUM | Initializes C runtime atexit handler list; called during startup | 1 | 0 |
| 0803ef20 | 236 | `fp_float_format_special` | MEDIUM | Formats special float values (inf/nan) for display; calls fp_float_to_double_norm | 1 | 1 |
| 0803f020 | 184 | `scope_draw_trigger_line` | LOW | Draws trigger level horizontal line on scope display; calls display_buffer_write; 3 callers | 3 | 1 |
| 0803f0e4 | 116 | `scope_draw_measurement_box` | LOW | Draws measurement value box on scope screen; calls display_buffer_write | 1 | 1 |
| 0803f1c8 | 250 | `fp_double_round_to_int` | MEDIUM | Rounds double to nearest integer (banker's rounding); called by FP division | 1 | 0 |
| 0803f3b9 | 1 | `halt_baddata_0` | HIGH | Ghidra false positive — data region misidentified as code | 0 | 0 |
| 0803f439 | 1 | `halt_baddata_1` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0803f4cb | 1 | `halt_baddata_2` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0803f4fb | 1 | `halt_baddata_3` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0803f81f | 1 | `halt_baddata_4` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0803fa7b | 1 | `halt_baddata_5` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0803fcc7 | 1 | `halt_baddata_6` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080403af | 1 | `halt_baddata_7` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080404db | 1 | `halt_baddata_8` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080406fb | 1 | `halt_baddata_9` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08040763 | 1 | `halt_baddata_10` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080407ab | 1 | `halt_baddata_11` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080407df | 1 | `halt_baddata_12` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804088b | 1 | `halt_baddata_13` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08040e6f | 1 | `halt_baddata_14` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804169b | 1 | `halt_baddata_15` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080416d3 | 1 | `halt_baddata_16` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804172b | 1 | `halt_baddata_17` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080418ff | 1 | `halt_baddata_18` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08041cdf | 1 | `halt_baddata_19` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042153 | 1 | `halt_baddata_20` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042327 | 1 | `halt_baddata_21` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080424db | 1 | `halt_baddata_22` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804260b | 1 | `halt_baddata_23` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042657 | 1 | `halt_baddata_24` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080428bb | 1 | `halt_baddata_25` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042a43 | 1 | `halt_baddata_26` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042bab | 1 | `halt_baddata_27` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042c6f | 1 | `halt_baddata_28` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042da3 | 1 | `halt_baddata_29` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042ea7 | 1 | `halt_baddata_30` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042f1f | 1 | `halt_baddata_31` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08042f77 | 1 | `halt_baddata_32` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043047 | 1 | `halt_baddata_33` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043057 | 1 | `halt_baddata_34` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080432a7 | 1 | `halt_baddata_35` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080438a3 | 1 | `halt_baddata_36` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043a1b | 1 | `halt_baddata_37` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043a93 | 1 | `halt_baddata_38` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043b8b | 1 | `halt_baddata_39` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043bd7 | 1 | `halt_baddata_40` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08043e4f | 1 | `halt_baddata_41` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804404b | 1 | `halt_baddata_42` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044677 | 1 | `halt_baddata_43` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080446df | 1 | `halt_baddata_44` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044703 | 1 | `halt_baddata_45` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080447b3 | 1 | `halt_baddata_46` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080447cb | 1 | `halt_baddata_47` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044857 | 1 | `halt_baddata_48` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044d21 | 1 | `halt_baddata_49` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044e39 | 1 | `halt_baddata_50` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044e43 | 1 | `halt_baddata_51` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044e49 | 1 | `halt_baddata_52` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08044f73 | 1 | `halt_baddata_53` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080451cb | 1 | `halt_baddata_54` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08045653 | 1 | `halt_baddata_55` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08045677 | 1 | `halt_baddata_56` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 0804570f | 1 | `halt_baddata_57` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 08045713 | 1 | `halt_baddata_58` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080458e3 | 1 | `halt_baddata_59` | HIGH | Ghidra false positive — data region | 0 | 0 |
| 080458eb | 1 | `halt_baddata_60` | HIGH | Ghidra false positive — data region | 0 | 0 |

## Notes

### Naming Methodology
1. **HIGH confidence** register-based: Functions that directly access known peripheral registers (SPI2_DATA at 0x4000380C, USART2 at 0x40004400, GPIO BOP/BCR, LCD at 0x60020000, DMA at 0x40020000, FMC at 0x40022000) were identified by their register access patterns.
2. **HIGH confidence** structural: FreeRTOS kernel functions identified by their list manipulation patterns (doubly-linked lists with end sentinels), PendSV/SysTick access, basepri manipulation, and queue buffer management.
3. **MEDIUM confidence** call-graph: Functions identified by what calls them and what they call. For example, functions called only by scope_main_fsm that access display rendering are scope sub-modes.
4. **LOW confidence** positional/contextual: Functions in gaps between well-identified functions, with no register access or distinctive patterns, named by surrounding context.

### Key Corrections from Previous Analysis
- FUN_08008154 was previously named `adc_measurement_core` but is actually `display_render_engine` — a text layout engine (handles word-wrap, multi-line, alignment)
- FUN_080278e4 was previously named `fpga_data_task` but is actually `usb_endpoint_handler` — accesses USB registers at 0x40005C00
- FUN_0803acf0 was previously named `fpga_send_command` but is actually `xQueueGenericSend` — FreeRTOS kernel function
- FUN_08036934 in function_map.txt (282 bytes) is `fs_flush_and_sync`, NOT the real FPGA task. The real FPGA task is the ~11KB function starting at the same address in the gap area (documented in fpga_task_decompile.txt)
- FUN_0803e8d0 was previously named `display_buffer_write` — confirmed as the final rendering pipeline hub
- FUN_08037800 is `fpga_spi3_transfer` — accesses GPIOB BOP/BCR for PB6 CS, performs SPI3 data exchange with FP conversion

### Subsystem Architecture (from call graph)
```
scope_main_fsm (13.3KB)
  +-- scope_mode_timebase (3.7KB)
  +-- scope_mode_trigger (2.2KB)
  +-- scope_mode_measure (1.3KB)
  +-- scope_mode_math (1.8KB)
  +-- scope_mode_cursor (4.6KB)
  +-- scope_mode_display_settings (4.6KB)
  +-- siggen_configure (1.6KB)
  +-- scope_display_refresh (626B)
  +-- scope_draw_channel_info (1.5KB)

meter_data_process (768B)
  +-- fp_double_divide -> fp_double_multiply -> display_buffer_write
  +-- xQueueGenericSend (sends to display task)

measurement_dispatch (520B) -- central hub, 11 callers
  +-- waveform_render_ch1 (6.6KB)
  +-- waveform_render_ch2 (4.1KB)
  +-- spi_flash_read_with_cache
  +-- display_alloc_buffer

FreeRTOS kernel:
  xQueueGenericSend -> freertos_ready_list_add -> freertos_port_yield
  xQueueReceive -> freertos_task_add_to_delayed -> freertos_task_switch_context
  xTaskCreate -> freertos_task_create_static -> freertos_list_insert_sorted
```
