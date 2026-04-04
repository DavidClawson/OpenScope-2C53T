# Gap Functions -- Complete Inventory

## Summary

- Functions in full_decompile.c: 347
- Functions in function_map.txt: 292
- Gap functions in code region (full_decompile.c only): 9
- Ghidra false positives in data region (halt_baddata): 61
- Functions in init_function_decompile.txt only (not in either): 8
- **Total real gap functions: 17** (9 code-region + 8 init-only)
- By priority: P0=4, P1=5, P2=5, P3=3

Note: The "362 functions" count from COVERAGE.md includes both the 347 in full_decompile.c
and functions from the init decompilation. The 61 data-region entries are Ghidra artifacts
(halt_baddata at odd addresses in font/image data) and should not be counted as real functions.

## Reconciliation

| Source | Count | Notes |
|--------|-------|-------|
| function_map.txt | 292 | Call-graph traced functions |
| full_decompile.c | 347 | 292 mapped + 9 code-region gaps + 46 C runtime false positives + 0 overlap |
| init_function_decompile.txt | 8 unique | Called from FUN_08023A50 (system_init), not in full_decompile.c |
| **Total real functions** | **309** | 292 + 9 + 8 |
| Ghidra false positives | 61 | All `halt_baddata()` in data region >= 0x0803F268 |

## Gap Functions by Address Range

### Gap A: 0x08009B7E--0x0800BCD4 (Probe Detection / FPGA Queue Commands, 8.3 KB) -- P1/P2

These functions all call `FUN_0803acf0` (xQueueGenericSend) with different command byte
sequences. They are FPGA command builders -- each queues a specific command to the FPGA
task. The `uStack00000007` byte is the FPGA command code. Called indirectly from
scope_main_fsm, siggen_configure, meter handler, and FreeRTOS timer callback.

| Address | Size | Ghidra Name | Likely Purpose | Priority | Evidence |
|---------|------|-------------|----------------|----------|----------|
| 0800b900 | ~8 | FUN_0800b900 | Jump table dispatcher (meter/probe mode switch) | P1 | `UNRECOVERED_JUMPTABLE` -- dispatches to 0800ba06-0800bc98 |
| 0800ba06 | 102 | FUN_0800ba06 | FPGA command: set CH1+CH2 ranges (6 queue sends) | P1 | Sends cmd bytes 0x07/0x0A, 0x1A-0x1E via xQueueGenericSend |
| 0800bb10 | 84 | FUN_0800bb10 | FPGA command: set trigger config (5 queue sends) | P1 | Sends cmd bytes 0x07/0x0A, 0x16-0x19 via xQueueGenericSend |
| 0800bba6 | 24 | FUN_0800bba6 | FPGA command: set acquisition mode (2 queue sends) | P2 | Sends cmd bytes 0x20, 0x21 |
| 0800bc00 | 42 | FUN_0800bc00 | FPGA command: set timebase (3 queue sends) | P2 | Sends cmd bytes 0x26, 0x27, 0x28 |
| 0800bc98 | 36 | FUN_0800bc98 | FPGA command: set probe attenuation (1 queue send) | P2 | Sends cmd byte 0x07/0x0A conditional on param sign |

### Gap B: 0x08034DD0 (FreeRTOS Internals, near task/queue area) -- P3

| Address | Size | Ghidra Name | Likely Purpose | Priority | Evidence |
|---------|------|-------------|----------------|----------|----------|
| 08034dd0 | ~64 | FUN_08034dd0 | FreeRTOS queue peek / task ready check | P3 | Reads `_DAT_200062f0` (likely pxCurrentTCB or queue handle), returns 0/pointer |

### Gap C: 0x08036A4E--0x08039874 (FPGA Protocol / SPI3, 11.5 KB) -- P0

| Address | Size | Ghidra Name | Likely Purpose | Priority | Evidence |
|---------|------|-------------|----------------|----------|----------|
| 08037400 | ~60 | FUN_08037400 | **fpga_usart_tx_task** (dvom_TX FreeRTOS task) — infinite loop on USART TX queue, builds 10-byte TX frames, kicks TMR2 for transmission | P0 | Calls `xQueueReceive` on USART_TX_QUEUE (0x20002D74), writes TX buf at 0x20000005, sets TMR2_CTL0 bit 7, delays 10 ticks. **ANNOTATED in gap_functions_annotated.c** |
| 08037800 | 472 | FUN_08037800 | **NOT a separate function** — inner loop of spi3_acquisition_task (FUN_08037454). Ghidra split due to unresolved computed branch. Contains per-mode SPI3 acquisition: cases 1-4 = normal/dual/roll/bulk with VFP calibration pipeline | P0 | Writes `0x40010c10`/`0x40010c14` (GPIOB BOP/BCR for PB6 CS), SPI data register polling, float math, `VectorSignedToFloat`, calls `xQueueReceive` on `DAT_20002d78`. **ANNOTATED in gap_functions_annotated.c** |

### Init-Only Functions (from FUN_08023A50 system_init, not in full_decompile.c)

These are called from the master init function (0x08023A50--0x080276F2, ~15.4 KB) which
itself is not in full_decompile.c. They were discovered through manual disassembly of the
boot sequence.

#### FUN_08023A50: The Master Init (P0)

| Address | ~Size | Ghidra Name | Likely Purpose | Priority | Evidence |
|---------|-------|-------------|----------------|----------|----------|
| 08023A50 | ~15400 | FUN_08023A50 | system_init -- master peripheral + RTOS init | P0 | Configures SPI3, USART, GPIO, DMA, timers, NVIC; creates all FreeRTOS tasks; FPGA handshake. THE most important function for hardware bring-up. |

#### Sub-functions called by system_init

| Address | ~Size | Ghidra Name | Likely Purpose | Priority | Evidence |
|---------|-------|-------------|----------------|----------|----------|
| 0800039c | ~24 | FUN_0800039c | GPIO or clock init helper | P1 | Called 1x from system_init near GPIO config section, adjacent to FUN_080003b4 (sprintf_to_buffer) |
| 08001830 | ~100 | FUN_08001830 | Calibration data loader (per-channel) | P1 | Called 2x with buffer at sl+0x356 and sl+0x483 (CH1/CH2 cal offsets), r1=0x12D (301 bytes), r2=byte XOR 0x80 |
| 080022dc | ~200 | FUN_080022dc | LCD/display init helper (color LUT or palette) | P2 | Called 2x during init with RGB565 values derived from bit manipulation, near `0x0804CA24` (font/image data pointer) |
| 08002c78 | ~200 | FUN_08002c78 | GPIOC probe detect / analog input init | P1 | Called 1x after reading GPIOC_ISTAT (0x40011008) bit 8 check, immediately before RCU_APB2EN enables |
| 0800b908 | ~512 | FUN_0800b908 | Probe/meter mode init (FPGA command sequence) | P1 | Called 1x during init, 8 bytes after FUN_0800b900 (jump table), in probe detection gap region |
| 08022d40 | ~200 | FUN_08022d40 | LCD window setup or display region config | P2 | Called 1x just before FUN_08022e14 (2.9KB UI helper), between UI/display gap functions |
| 08039734 | 316 | FUN_08039734 | **Generic config writer** (`usart_tx_config_writer` in fpga_task_annotated.c). DUAL-PURPOSE: when called from system_init with timer base addresses (TIM5/TIM2), configures timers. When called from FPGA task with state pointer, writes USART TX config. Uses 7-type TBB switch. | P1 | Called 2x from init: r0=TIM5_CTL0 (0x40000C00), r0=TIM2_CTL0 (0x40000000). **Already annotated in fpga_task_annotated.c lines 1382-1493.** Discrepancy resolved in gap_functions_annotated.c. |

## FPGA Command Code Map (Extracted from Gap Functions)

The gap functions at 0x0800ba06--0x0800bc98 reveal previously unknown FPGA command codes
sent via `xQueueGenericSend` to the FPGA task queue:

| Cmd Byte | Source Function | Likely Meaning |
|----------|----------------|----------------|
| 0x07 | FUN_0800ba06, FUN_0800bb10, FUN_0800bc98 | Channel range prefix (when param < 0) |
| 0x0A | FUN_0800ba06, FUN_0800bb10, FUN_0800bc98 | Channel range prefix (when param >= 0) |
| 0x16 | FUN_0800bb10 | Trigger: threshold LSB |
| 0x17 | FUN_0800bb10 | Trigger: threshold MSB |
| 0x18 | FUN_0800bb10 | Trigger: mode/edge |
| 0x19 | FUN_0800bb10 | Trigger: holdoff (final, blocking) |
| 0x1A | FUN_0800ba06 | Channel: CH1 gain |
| 0x1B | FUN_0800ba06 | Channel: CH1 offset |
| 0x1C | FUN_0800ba06 | Channel: CH2 gain |
| 0x1D | FUN_0800ba06 | Channel: CH2 offset |
| 0x1E | FUN_0800ba06 | Channel: coupling/BW limit (final, blocking) |
| 0x20 | FUN_0800bba6 | Acquisition: run mode |
| 0x21 | FUN_0800bba6 | Acquisition: sample depth (final, blocking) |
| 0x26 | FUN_0800bc00 | Timebase: prescaler |
| 0x27 | FUN_0800bc00 | Timebase: period |
| 0x28 | FUN_0800bc00 | Timebase: mode (final, blocking) |

"Final, blocking" means the last call in a sequence uses `xQueueGenericSend(..., 0xffffffff)`
(portMAX_DELAY) to block until the queue accepts the item.

## Data Region False Positives (61 entries, all SKIP)

All 61 functions at addresses >= 0x0803F268 are Ghidra artifacts where the disassembler
attempted to interpret font data, image data, or lookup tables as ARM Thumb instructions.
Every one contains only `halt_baddata()`. These are NOT real functions.

Addresses: 0803f3b9, 0803f439, 0803f4cb, 0803f4fb, 0803f81f, 0803fa7b, 0803fcc7,
080403af, 080404db, 080406fb, 08040763, 080407ab, 080407df, 0804088b, 08040e6f,
0804169b, 080416d3, 0804172b, 080418ff, 08041cdf, 08042153, 08042327, 080424db,
0804260b, 08042657, 080428bb, 08042a43, 08042bab, 08042c6f, 08042da3, 08042ea7,
08042f1f, 08042f77, 08043047, 08043057, 080432a7, 080438a3, 08043a1b, 08043a93,
08043b8b, 08043bd7, 08043e4f, 0804404b, 08044677, 080446df, 08044703, 080447b3,
080447cb, 08044857, 08044d21, 08044e39, 08044e43, 08044e49, 08044f73, 080451cb,
08045653, 08045677, 0804570f, 08045713, 080458e3, 080458eb

## Priority Summary

### P0 -- Critical for hardware bring-up (4 functions)
- **FUN_08023A50** (system_init): Master init, all peripheral config, FPGA handshake
- **FUN_08037400** (FPGA command RX loop): Infinite-loop task receiving FPGA responses
- **FUN_08037800** (FPGA SPI3 transfer): SPI3 CS control, ADC data read, sample processing with float math
- **FUN_08039734** (timer init): TIM2/TIM5 config called from system_init (needed for FPGA timing)

### P1 -- Needed for channel/trigger/meter functionality (5 functions)
- **FUN_0800b900** (mode dispatcher): Jump table for meter/probe modes
- **FUN_0800ba06** (FPGA cmd: channel ranges): Sets CH1/CH2 gain, offset, coupling
- **FUN_0800bb10** (FPGA cmd: trigger): Sets trigger threshold, mode, holdoff
- **FUN_08001830** (calibration loader): Loads 301-byte CH1/CH2 calibration data
- **FUN_08002c78** (analog input init): GPIOC probe detect, pre-clock-enable init

### P2 -- Needed for full functionality (5 functions)
- **FUN_0800bba6** (FPGA cmd: acquisition mode): Run mode + sample depth
- **FUN_0800bc00** (FPGA cmd: timebase): Prescaler, period, mode
- **FUN_0800bc98** (FPGA cmd: probe atten): Probe attenuation setting
- **FUN_080022dc** (LCD init helper): Color/palette init during boot
- **FUN_08022d40** (display region config): LCD window setup

### P3 -- Low priority / can skip (3 functions)
- **FUN_08034dd0** (RTOS internal): Queue peek helper
- **FUN_0800039c** (init helper): Small GPIO/clock helper near boot code
- **FUN_0800b908** (probe init): Probe mode init sequence in system_init
