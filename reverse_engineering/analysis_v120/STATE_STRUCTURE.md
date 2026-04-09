# Global State Structure Map — 0x200000F8

The entire firmware revolves around a single large structure in SRAM at **0x200000F8**. In the decompiled code, register `r9` (or `sl`) holds this base address. When you see `*(byte *)(unaff_r9 + 0x2D)` in Ghidra output, that's `state[0x2D]` — the timebase index.

This document decodes every known offset. **71 functions** access this structure.

## How to Use This Document

When reading ANY decompiled function and you see an offset like `unaff_r9 + 0x14`, look it up here. The field name and purpose will tell you what the code is actually doing.

Absolute address = `0x200000F8 + offset`

---

## Quick Reference — Most Important Fields

| Offset | Abs Address | Name | Type | Refs | Purpose |
|--------|-------------|------|------|------|---------|
| +0x04 | 200000FC | `ch1_adc_offset` | int8 | **71** | CH1 signed ADC DC offset (calibration) |
| +0x05 | 200000FD | `ch2_adc_offset` | int8 | **51** | CH2 signed ADC DC offset (calibration) |
| +0x14 | 2000010C | `voltage_range` | uint8 | **58** | Voltage range setting (controls relays + gain) |
| +0x15 | 2000010D | `channel_config` | uint8 | **24** | Packed: hi nibble=active_ch, lo nibble=num_channels |
| +0x2D | 20000125 | `timebase_index` | uint8 | **63** | Timebase setting (0x00-0x13, index into speed table) |
| +0x30 | 20000128 | `scope_sub_mode` | uint8 | **37** | Scope sub-mode selector (scope_main_fsm exclusive) |
| +0xF68 | 20001060 | `system_mode` | uint8 | 7 | Overloaded mode / command-bank selector (scope code checks `0x02`, helper cluster also uses low-byte banks `0..9`) |

---

## Section 1: Core Configuration (offsets 0x00 – 0x0F)

These are the fundamental device configuration parameters, set during init and changed by mode switching.

| Offset | Abs Address | Name | Type | Refs | Accessors | Description |
|--------|-------------|------|------|------|-----------|-------------|
| +0x00 | 200000F8 | `ch1_enable` | uint8 | — | system_init | CH1 enabled flag (0=off, 1=on) |
| +0x01 | 200000F9 | `ch2_enable` | uint8 | — | system_init | CH2 enabled flag |
| +0x02 | 200000FA | `meter_function` | uint8 | **25** | scope_fsm, siggen, cursor, timebase, math | Meter function selector (DCV, ACV, Ohm, etc.) |
| +0x03 | 200000FB | `meter_range` | uint8 | **11** | scope_fsm, siggen, cursor, timebase | Meter range select |
| +0x04 | 200000FC | `ch1_adc_offset` | int8 | **71** | gpio_mux_portc, scope_fsm, cursor, timebase, display, math, trigger, spi3_acq | CH1 signed ADC DC offset. Used in VFP calibration: `(float)(raw - ch1_adc_offset)`. Loaded from cal table at init. XOR'd with 0x80 for cal_loader key. |
| +0x05 | 200000FD | `ch2_adc_offset` | int8 | **51** | gpio_mux_porta, scope_fsm, cursor, timebase, display | CH2 signed ADC DC offset. Same usage as +0x04 but for CH2. |
| +0x06 | 200000FE | — | — | — | — | (gap / padding) |
| +0x0A | 20000102 | `coupling_config` | uint16 | — | master_init | Coupling/BW limit bitfield (likely AC/DC + BW for both channels) |
| +0x0D | 20000105 | — | — | — | — | (gap) |

---

## Section 2: Acquisition Control (offsets 0x10 – 0x1F)

Controls how the FPGA acquires data — mode, channels, trigger, voltage range.

| Offset | Abs Address | Name | Type | Refs | Accessors | Description |
|--------|-------------|------|------|------|-----------|-------------|
| +0x10 | 20000108 | `meter_sub_mode` | uint8 | — | meter_data_processor | Meter sub-mode (0-7) for BCD result formatting |
| +0x14 | 2000010C | `voltage_range` | uint8 | **58** | gpio_mux_porta, gpio_mux_portc, scope_fsm, waveform_render, measurements, timebase, display_settings, channel_info, trigger | Voltage range index. Controls analog frontend relay states (PC12, PE4/PE5/PE6) and gain resistors (PA15, PA10, PB10). Values 0-3 select different V/div. When == 3: 100x probe mode. Accessed by 12+ functions. |
| +0x15 | 2000010D | `channel_config` | uint8 | **24** | siggen, scope_fsm, measure, cursor, trigger | **Packed byte:** high nibble (bits 7-4) = active channel index (0 or 1). Low nibble (bits 3-0) = number of enabled channels. Code uses `>> 4` and `& 0xF` to unpack. |
| +0x16 | 2000010E | `active_channel` | uint8 | **17** | siggen, scope_fsm, timebase, math, channel_info, spi3_acq | Active channel selector (0=CH1, 1=CH2). Used to index into per-channel calibration arrays: `state[active_channel * 0x12D + 0x356]` selects the right cal table. |
| +0x17 | 2000010F | `trigger_run_mode` | uint8 | **5** | scope_fsm, timebase, channel_info | Trigger run mode: 0=AUTO, 1=NORMAL, 2=SINGLE. When == 2, scope_main_fsm skips acquisition calls. |
| +0x18 | 20000110 | `trigger_edge` | uint8 | **3** | scope_fsm, timebase, channel_info | Trigger edge polarity (0=rising, 1=falling) |
| +0x1A | 20000112 | `trigger_position` | int16 | **11** | scope_fsm, timebase, math, channel_info | Signed trigger horizontal position offset. Used for pre/post-trigger display windowing. |
| +0x1C | 20000114 | `trigger_level` | int16 | **8** | siggen, scope_fsm, timebase, channel_info, spi3_acq | Trigger voltage level (signed 16-bit). In the SPI3 acquisition: `trigger_pos = (int16_t)voltage_range - trigger_level` before calibration. |

---

## Section 3: Scope Runtime State (offsets 0x20 – 0x3F)

Dynamic state that changes during oscilloscope operation.

| Offset | Abs Address | Name | Type | Refs | Accessors | Description |
|--------|-------------|------|------|------|-----------|-------------|
| +0x20 | 20000118 | `config_bitfield_a` | uint32 | **10** | FUN_08034078, FUN_080342f8 | Configuration bitfield written by usart_tx_config_writer. Bits encode AC/DC coupling, BW limit, trigger source per channel (see fpga_task_annotated.c types 0-2). |
| +0x24 | 2000011C | `config_bitfield_b` | uint32 | **10** | FUN_08034078, FUN_080342f8 | Second config bitfield (continuation of +0x20) |
| +0x28 | 20000120 | `button_repeat_timer` | uint8 | **2** | FUN_080342f8 | Button auto-repeat countdown |
| +0x2C | 20000124 | `redraw_inhibit` | uint8 | **5** | scope_fsm, timebase, tmr3_isr | When != 0, suppresses sub-handler calls (display redraw inhibit). Decremented by TMR3. |
| +0x2D | 20000125 | `timebase_index` | uint8 | **63** | gpio_mux_porta, gpio_mux_portc, scope_fsm, measurements, siggen, cursor, timebase, math, channel_info, waveform_render | **The 2nd most-referenced field.** Index into the timebase speed table (0x00-0x13 = 20 settings). Values > 0x13 enter "slow timebase" mode where roll/streaming is used. Determines: SPI3 acquisition mode, FPGA prescaler, sample depth, display rendering strategy. |
| +0x2E | 20000126 | `scope_active_flag` | uint8 | **21** | measurements, scope_fsm, cursor, timebase, channel_info | Scope activity flag. 0=idle, nonzero=active. scope_main_fsm checks this at entry and skips execution if 0. Also controls whether frequency measurement runs. |
| +0x2F | 20000127 | `acquisition_phase` | uint8 | **15** | siggen, scope_fsm | Acquisition phase state machine: 0x00=waiting for data, 0x01=data ready/process. scope_main_fsm switches on this to decide whether to run the waveform processing pipeline or wait. |
| +0x30 | 20000128 | `scope_sub_mode` | uint8 | **37** | scope_fsm (near-exclusive) | **Scope sub-mode selector.** Controls which scope operation mode is active. Almost exclusively accessed by scope_main_fsm — this is the state variable of its inner state machine. Values select between: normal view, FFT, XY mode, cursor mode, measurement mode, etc. |
| +0x31 | 20000129 | `siggen_config_byte` | uint8 | **1** | siggen_configure | Signal generator configuration (waveform type or output state) |
| +0x32 | 2000012A | `acquisition_counter` | uint8 | **12** | scope_fsm | Acquisition sample counter. Compared to 0x27 (39) in auto-range logic. When > 39, triggers auto-range evaluation. Reset on timebase change. |
| +0x33 | 2000012B | `cal_bypass` | uint8 | **5** | scope_fsm, spi3_acq | Calibration bypass flag. 0=normal (cal active), nonzero=skip calibration. SPI3 acquisition checks: `if (state[0x33] == 0)` before applying VFP cal. |
| +0x34 | 2000012C | `display_mode` | uint8 | **21** | waveform_render, scope_measure, display_settings, math, trigger | Current label retained, but newer trace shows it is likely a downstream render/overlay latch set and cleared by the broader scope controller before the later right-panel redraw/resource owner runs. Still affects what gets rendered, but probably is not a top-level owner byte by itself. |
| +0x35 | 2000012D | `display_sub_config` | uint8 | **3** | display_settings | Current label retained only provisionally. Recent raw trace shows this byte is compared against `F6B` and cleared when they match inside the downstream trigger-side posture cluster, so it may be a local preview/posture mirror rather than a simple display sub-setting. |
| +0x38 | 20000130 | `menu_state` | uint32 | **49** | display_settings + many unknown handlers | Menu/settings state. At 49 refs, this is heavily used — likely encodes which menu is open, which item is highlighted, scroll position, etc. |
| +0x3C | 20000134 | `measurement_config` | uint8 | **5** | scope_measure | Current label is now shaky. In the `0x080095AE..0x08009B1E` posture cluster this byte is mirrored against `F6B`, toggled between `0` and `F6B`, and gates pointer/list handling via `+0x40`, so it likely participates in a local preview/control posture beyond plain measurement config. |
| +0x3D | 20000135 | `frame_counter` | uint8 | **1** | — | Frame/update counter (wraps at 99) |
| +0x40 | 20000138 | `measurement_state` | uint32 | **25** | scope_measure + many unknowns | Current label is provisional. In the newer posture-cluster trace this field behaves like a pointer/list head with elements at offsets `+4`, `+8`, and `+16`, and is cleared to `0` when exhausted. That looks more structured than a plain scalar measurement selector. |

---

## Section 4: Scope Measurement Scratch (offsets 0x50 – 0x230)

This region is exclusively used by `scope_measurement_engine` (FUN_0801f6f8, 4616 bytes — previously labeled `scope_mode_cursor`; 2026-04-04 RE session reclassified it as a full Vpp/Vrms/Vmax/Vmin/Vavg/frequency/period measurement engine with 64-bit VFP accumulators. Cursor readout is one minor output). It stores the per-channel measurement accumulators and intermediate coordinates.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0x50 | 20000148 | `cursor_data[0]` | uint32 | 4 | Cursor position / measurement array |
| +0x54 | 2000014C | `cursor_data[1]` | uint32 | 6 | (4-byte stride, 120 entries) |
| +0x58–0x230 | ... | `measurement_scratch[2..119]` | uint32[] | 2-4 each | 120-entry array of 4-byte scope measurement values. Accessed exclusively by `scope_measurement_engine`. Stores Vpp/Vrms/freq/period accumulators, delta calculations, and readout values for both channels. |
| +0x230 | 20000328 | `trigger_state_prev` | uint8 | 1 | Previous trigger crossing state (for edge detection) |
| +0x231 | 20000329 | `trigger_state_curr` | uint8 | 2 | Current trigger crossing state |

---

## Section 5: Math Mode Data (offsets 0x232 – 0x25E)

Used by `scope_mode_math` (FUN_0801efc0) for FFT, add/subtract, multiply operations.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0x232 | 2000032A | `math_result_a` | int16 | 2 | Math operation result A |
| +0x234 | 2000032C | `math_result_b` | int16 | 2 | Math operation result B |
| +0x236 | 2000032E | `math_result_c` | int16 | 2 | Math operation result C |
| +0x238 | 20000330 | `math_result_d` | int16 | 2 | Math operation result D |
| +0x23A | 20000332 | `math_mode_select` | uint8 | **7** | Math mode selector (add, sub, mult, FFT). Also checked during probe_mode_init. |
| +0x23C | 20000334 | `math_config` | uint32 | 4 | Math configuration parameters |
| +0x240 | 20000338 | `math_state_a` | uint32 | 6 | Math processing state |
| +0x244 | 2000033C | `math_state_b` | uint32 | 6 | Math processing state |
| +0x248 | 20000340 | `fft_data[0]` | uint32 | 8 | FFT computation data |
| +0x24C | 20000344 | `fft_data[1]` | uint32 | 8 | |
| +0x250 | 20000348 | `fft_data[2]` | uint32 | 8 | |
| +0x254 | 2000034C | `fft_peak` | int16 | 2 | FFT peak value |
| +0x258 | 20000350 | `ch1_cal_present` | uint8 | 3 | CH1 calibration data present flag (0=no, 1=yes) |
| +0x259 | 20000351 | `ch2_cal_present` | uint8 | 3 | CH2 calibration data present flag |
| +0x25A | 20000352 | `ch1_cal_gain` | uint8 | 4 | CH1 calibration gain byte. 0=invalid, 0xFF=invalid. Used in VFP pipeline: `if (ch1_cal_gain != 0 && ch1_cal_gain != 0xFF)` |
| +0x25B | 20000353 | `ch2_cal_gain` | uint8 | 3 | CH2 calibration gain byte. Same validation as CH1. |
| +0x25C | 20000354 | `cal_extra_a` | uint8 | 2 | Additional calibration parameter |
| +0x25D | 20000355 | `cal_extra_b` | uint8 | 2 | Additional calibration parameter |
| +0x25E | 20000356 | `cal_extra_c` | uint8 | 3 | Additional calibration parameter |

---

## Section 6: Calibration Tables (offsets 0x260 – 0x33C)

Per-channel gain/offset calibration tables loaded from SPI flash at boot. Used by `scope_main_fsm` and `gpio_mux_porta_portb` to compute voltage scaling.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0x260 | 20000358 | `cal_table[0]` | struct (20B) | 9 | Calibration entry 0: gain(4B) + offset(4B) + params(12B) |
| +0x274 | 2000036C | `cal_table[1]` | struct (20B) | 10 | Calibration entry 1 |
| +0x288 | 20000380 | `cal_table[2]` | struct (20B) | 10 | Calibration entry 2 |
| +0x29C | 20000394 | `cal_table[3]` | struct (20B) | 8 | Calibration entry 3 |
| +0x2B0 | 200003A8 | `cal_table[4]` | struct (20B) | 9 | Calibration entry 4 |
| +0x2C4 | 200003BC | `cal_table[5]` | struct (20B) | 9 | Calibration entry 5 |

These 6 entries (stride = 0x14 = 20 bytes each, at addresses 0x20000358–0x20000434) are the **gain/offset calibration pairs** documented in CLAUDE.md. Loaded from SPI flash at boot. `scope_main_fsm` indexes into this table based on the current voltage range to apply the correct gain and offset.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0x2D8 | 200003D0 | `cal_ext_table[0]` | struct (20B) | 9 | Extended calibration (entries 6-11) |
| +0x2EC | 200003E4 | `cal_ext_table[1]` | struct (20B) | 9 | |
| +0x300 | 200003F8 | `cal_ext_table[2]` | struct (20B) | **16** | Most-accessed extended cal entry |
| +0x314 | 2000040C | `cal_ext_table[3]` | struct (20B) | 9 | |
| +0x328 | 20000420 | `cal_ext_table[4]` | struct (20B) | 9 | |
| +0x33C | 20000434 | `cal_ext_table[5]` | struct (20B) | **14** | |

---

## Section 7: Channel State (offsets 0x350 – 0x353)

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0x350 | 20000448 | `ch1_state_byte` | uint8 | **23** | CH1 display state. Accessed by scope_fsm, timebase, channel_info, trigger, and 17 waveform render callsites. Controls how CH1 waveform is drawn. |
| +0x351 | 20000449 | `ch2_state_byte` | uint8 | **24** | CH2 display state. Same pattern as above but for CH2. |
| +0x352 | 2000044A | `ch1_probe_detect` | uint8 | **3** | CH1 probe type detected. 0=none, 0xFF=none, other=probe present. Used in SPI3 VFP cal: `if (ch1_probe_detect != 0 && ch1_probe_detect != 0xFF)` |
| +0x353 | 2000044B | `ch2_probe_detect` | uint8 | **2** | CH2 probe type detected. Same validation. |

---

## Section 8: Roll Mode Buffers (offsets 0x356 – 0x5AF)

Circular buffers for roll/streaming mode acquisition. Two 301-byte buffers (one per channel), plus per-sample calibrated output bytes.

| Offset | Abs Address | Name | Type | Size | Description |
|--------|-------------|------|------|------|-------------|
| +0x356 | 2000044E | `roll_buf_ch1[]` | uint8[] | 301 (0x12D) | CH1 roll mode circular buffer. Shift-and-append: new sample goes at end, older samples shift left by 1. |
| +0x483 | 2000057B | `roll_buf_ch2[]` | uint8[] | 301 (0x12D) | CH2 roll mode circular buffer. |
| +0x38A | 20000482 | `roll_cal_ch1` | uint8 | — | Last calibrated CH1 sample (roll mode) |
| +0x4B7 | 200005AF | `roll_cal_ch2` | uint8 | — | Last calibrated CH2 sample (roll mode) |

The roll buffer addressing pattern in code:
```c
/* channel_index * 0x12D + 0x2000044E selects CH1 or CH2 buffer */
uint8_t *buf = (uint8_t *)(channel * 0x12D + 0x2000044E);
```

---

## Section 9: ADC Sample Buffers (offsets 0x5B0 – 0xDAF)

Main waveform display buffers. Filled by SPI3 bulk acquisition, read by display rendering.

| Offset | Abs Address | Name | Type | Size | Description |
|--------|-------------|------|------|------|-------------|
| +0x5B0 | 200006A8 | `adc_buf_ch1[]` | uint8[] | 1024 (0x400) | **CH1 display sample buffer.** Normal mode: 512 samples (interleaved read fills both CH1+CH2). Bulk mode: 1024 samples. Referenced by 28+ waveform render callsites. |
| +0x9B0 | 20000AA8 | `adc_buf_ch2[]` | uint8[] | 1024 (0x400) | **CH2 display sample buffer.** Same as CH1 but for channel 2. Referenced by 29+ callsites. |

---

## Section 10: Acquisition Runtime (offsets 0xDB0 – 0xEFF)

Dynamic state managed by the SPI3 acquisition task and TMR3 ISR.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xDB0 | 20000EA8 | `acq_frame_type` | uint8 | **14** | Acquisition frame type / trigger state. scope_main_fsm checks `== 1` to decide if new data is ready. Set by SPI3 task after a complete acquisition. |
| +0xDB2 | 20000EAA | `input_change_curr` | uint8 | 2 | Current input change event flag |
| +0xDB4 | 20000EAC | `roll_write_ptr` | uint16 | **20** | Roll mode write pointer. 16-bit counter for circular buffer position. |
| +0xDB6 | 20000EAE | `roll_fill_level` | uint16 | **17** | Roll mode fill level. Max = 0x12D (301). When full, triggers shift operation. |
| +0xDB8 | 20000EB0 | `acq_sample_counter` | uint16 | — | Current acquisition sample count (for auto-range comparison) |
| +0xDBA | 20000EB2 | `acq_busy_a` | uint8 | — | Acquisition busy flag A. scope_main_fsm skips if nonzero. |
| +0xDBB | 20000EB3 | `acq_busy_b` | uint8 | — | Acquisition busy flag B. Same — skips if nonzero. |
| +0xDBC | 20000EB4 | `scope_runtime_state` | uint16 | **32** | Scope runtime state word. Heavily accessed by scope_fsm, timebase, channel_info, trigger. Likely encodes current waveform rendering state. |
| +0xDBE | 20000EB6 | `scope_meas_select` | uint8 | **7** | Which measurement is selected for display (Vpp, Freq, etc.) |
| +0xDBF | 20000EB7 | `scope_trigger_state` | uint8 | **18** | Trigger detection state. Accessed by scope_fsm and many unknown display functions. |
| +0xDC0 | 20000EB8 | `scope_param_a` | uint8 | **9** | Scope configuration parameter A |
| +0xDC1 | 20000EB9 | `scope_param_b` | uint8 | **7** | Scope configuration parameter B |
| +0xDC2 | 20000EBA | `scope_param_c` | uint8 | **5** | |
| +0xDC3 | 20000EBB | `scope_param_d` | uint8 | **4** | |
| +0xDC4 | 20000EBC | `scope_param_e` | uint8 | **5** | |
| +0xDC6 | 20000EBE | `scope_display_flag` | uint8 | **4** | Display update flag |
| +0xDC8 | 20000EC0 | `scope_state_word_a` | uint32 | **5** | |
| +0xDD0 | 20000EC8 | `scope_state_word_b` | uint32 | **9** | |
| +0xDD4 | 20000ECC | `scope_state_word_c` | uint32 | **9** | |
| +0xDD8 | 20000ED0 | `scope_state_word_d` | uint32 | **9** | |
| +0xDDC | 20000ED4 | `scope_state_word_e` | uint32 | **9** | |
| +0xDE0 | 20000ED8 | `scope_threshold` | uint8 | **6** | Threshold value (auto-range or trigger) |
| +0xDE8 | 20000EE0 | `scope_state_word_f` | uint32 | **5** | |
| +0xDEC | 20000EE4 | `scope_state_word_g` | uint32 | **5** | |
| +0xDF8 | 20000EF0 | `scope_state_final` | uint32 | **3** | |
| +0xE04 | 20000EFC | `meas_result_a` | float | **4** | Measurement result (Vpp, RMS, etc.) |
| +0xE08 | 20000F00 | `meas_result_b` | float | **15** | Measurement result (heavily used) |
| +0xE0C | 20000F04 | `meas_result_c` | float | **15** | Measurement result (heavily used) |

---

## Section 11: Power & System (offsets 0xE10 – 0xE30)

Update 2026-04-08:
- the old power-management labels in this subsection are no longer trustworthy
  for `+0xE10..+0xE1C`
- recent objdump and forced decompiles show this byte family participating in a
  right-panel redraw / file-resource overlay path rather than plain power state
  handling
- see
  [scope_cluster_control_bytes_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_cluster_control_bytes_2026_04_08.md)
  and
  [scope_state_commit_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_commit_bridge_2026_04_08.md)

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xE10 | 20000F08 | `panel_overlay_state` | uint8 | **3** | Transient redraw / overlay branch selector. The `0x08015780..0x08015A14` ladder branches on `1 / 2 / 3 / 0xFF`, and the decompiled key-loop slice at `0x08039008` clears the `2 / 3` family back to `0` before normal dispatch. When comparing that loop against the archived raw app image, use the app-slot correction from [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md): raw `0x0803D008` corresponds to decompiled `0x08039008`. Not a stable top-level mode byte. |
| +0xE11 | 20000F09 | `panel_entry_index` | uint8 | **4** | Current resource/list index for the same overlay path. `FUN_08034878` writes it as `last_item + 1` or `1` by default, and the redraw ladder reads it while formatting right-panel content. |
| +0xE1B | 20000F13 | `panel_entry_count` | uint8 | **1** | Entry count / append cursor for the overlay list state. The special `*(base + 0xE10) == 1` path stores the `FUN_08034878(...)` return byte here, and the success path appends `+0xE11` into `+0xE25[]` using this byte as an index. |
| +0xE1C | 20000F14 | `panel_subview_state` | uint8 | **2** | Right-panel subview / toggle byte. Raw code repeatedly tests and writes `0 / 1 / 2`, and `FUN_0801819C` renders different right-panel strings/colors from this value. |

---

## Section 12: Meter State (offsets 0xF2D – 0xF40)

Separate from scope state — this is the multimeter's operating state.

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xF2D | 20001025 | `meter_mode` | uint8 | **9** | Meter operating mode (0-8): DCV, ACV, Ohm, Cont, Diode, Cap, Freq, Period, Duty |
| +0xF2E | 20001026 | `meter_display_state` | uint8 | **10** | Meter display state machine |
| +0xF2F | 20001027 | `meter_range` | uint8 | **2** | Current meter range setting |
| +0xF30 | 20001028 | `meter_raw_value` | uint32 | **5** | Raw meter measurement value (from BCD decode) |
| +0xF34 | 2000102C | `meter_result_class` | uint8 | **3** | Result class: 1=normal, 2=underrange, 3=overrange, 4=invalid |
| +0xF35 | 2000102D | `meter_data_valid` | uint8 | **1** | Data validity flag |
| +0xF36 | 2000102E | `meter_overload` | uint8 | **7** | Overload detection state |
| +0xF37 | 2000102F | `meter_cal_coeff` | uint8 | **14** | Calibration coefficient selector (indexes into cal table) |
| +0xF38 | 20001030 | `meter_range_marker` | uint8 | **8** | Range marker (0xFF = unset) |
| +0xF3C | 20001034 | `usart_exchange_lock` | uint16 | **2** | USART exchange lock (nonzero = locked) |

---

## Section 13: Display Task State (offsets 0xF59 – 0xF84)

Used by `lcd_draw_bitmap_from_flash` (FUN_08022e14) for drawing UI assets from SPI flash.

Update 2026-04-08:
- boot restore code at `0x08026F50` reads `base + 0xF64` and can copy that byte
  into `DAT_20001060`, so this subsection now needs a focused re-audit before
  `+0xF64` is trusted as pure display state. See
  [mode_selector_writer_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/mode_selector_writer_map_2026_04_08.md).

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xF59 | 20000F51 | `draw_bitmap_flag` | uint8 | **2** | Bitmap drawing active flag |
| +0xF60 | 20000F58 | `draw_x_pos` | uint8 | **2** | Bitmap draw X position |
| +0xF61 | 20000F59 | `draw_y_pos` | uint8 | **9** | Bitmap draw Y position (heavily accessed) |
| +0xF62 | 20000F5A | `draw_width` | uint16 | **6** | Bitmap draw width |
| +0xF64 | 20000F5C | `draw_height` | uint16 | **6** | Bitmap draw height |
| +0xF66–0xF84 | ... | `draw_palette[]` | uint16[] | 3-6 ea. | RGB565 color palette entries for bitmap rendering (16 entries) |

---

## Section 14: System Mode & UI (offsets 0xF60 – 0xF78)

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xF60 | 20001058 | `ui_state` | uint8 | **52** | **UI state byte** — heavily accessed by display functions (23 accessor functions). Controls which screen/mode the UI is currently rendering. |
| +0xF68 | 20001060 | `system_mode` | uint8 | **7** | **Overloaded mode / command-bank selector byte.** scope-side code checks `== 0x02` in some paths, but the same low byte is also reused by the helper cluster around `0x08006418` to select command banks `0..9`. See [enclosing_helper_cluster_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/enclosing_helper_cluster_2026_04_08.md). |
| +0xF69 | 20001061 | `mode_transition_flags` | uint16 | **15** | Packed mode-transition / phase gate. Reader-side code treats value `2` as a special draw-state condition, and raw objdump now shows concrete decrement/increment/reset writers at `0x08003036`, `0x08004236`, and `0x08004806`. See [scope_state_commit_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_commit_bridge_2026_04_08.md) and [packed_scope_state_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_writers_2026_04_08.md). |
| +0xF6A | 20001062 | `scope_ui_state_flags` | uint8 | **9** | Packed scope UI / subview selector byte. Scope-side draw paths split high/low nibbles for list selection and `scope_ui_draw_main` switches on `& 0xF`. Also used by the helper cluster's `9 -> 2` collapse check. Current exact-byte xrefs show reads plus a boot/config restore write at `0x08027006`; runtime mutation may happen through a wider packed-state helper rather than a simple standalone `strb`. See [scope_low_byte_2_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_low_byte_2_path_2026_04_08.md) and [scope_state_commit_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_commit_bridge_2026_04_08.md). |
| +0xF6B | 20001063 | `mode_flags` | uint8 | **15** | Packed display / marker mode byte. Nearby scope-panel helpers branch on `== 3` and decode its high bits into marker/highlight positions, and raw objdump now shows real normalization/step writers at `0x080030F6`, `0x0800311E`, `0x0800440E`, and `0x0800450C`. See [scope_state_commit_bridge_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/scope_state_commit_bridge_2026_04_08.md) and [packed_scope_state_writers_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/packed_scope_state_writers_2026_04_08.md). |

---

## Section 15: Waveform Display State (offset 0xF78+)

| Offset | Abs Address | Name | Type | Refs | Description |
|--------|-------------|------|------|------|-------------|
| +0xF78 | 20001070 | `waveform_x_origin` | uint16 | **51** | Waveform X origin for rendering. Accessed by 27 functions including scope_fsm, display_render_engine, all waveform renderers. |
| +0xF80 | 20001078 | `waveform_y_origin` | uint16 | **41** | Waveform Y origin |
| +0xF84 | 2000107C | `waveform_width` | uint16 | **40** | Waveform display width (pixels) |
| +0xF88 | 20001080 | `waveform_height` | uint16 | **42** | Waveform display height (pixels) |

These 4 fields define the waveform rendering viewport rectangle. They're among the most-referenced fields in the entire structure.

---

## Structure Size Estimate

| Region | Start Offset | End Offset | Size | Purpose |
|--------|-------------|------------|------|---------|
| Core config | 0x000 | 0x03F | 64B | Channel, trigger, mode, timing |
| Cursor data | 0x050 | 0x231 | 482B | Cursor positions and measurements |
| Math data | 0x232 | 0x25F | 46B | FFT and math mode |
| Cal tables | 0x260 | 0x34F | 240B | 12 gain/offset calibration entries |
| Channel state | 0x350 | 0x355 | 6B | Probe detect, channel state |
| Roll buffers | 0x356 | 0x5AF | 602B | 2x 301-byte circular buffers |
| ADC CH1 buf | 0x5B0 | 0x9AF | 1024B | CH1 sample buffer |
| ADC CH2 buf | 0x9B0 | 0xDAF | 1024B | CH2 sample buffer |
| Acq runtime | 0xDB0 | 0xE0F | 96B | Acquisition counters, state |
| Power/system | 0xE10 | 0xE30 | 33B | Power management |
| Meter state | 0xF2D | 0xF40 | 20B | Multimeter operating state |
| Display state | 0xF58 | 0xF90 | 57B | UI, bitmap, waveform viewport |
| **TOTAL** | 0x000 | 0xF90 | **~3984B** | Full structure |

The structure spans approximately **4KB** of SRAM starting at 0x200000F8, ending around 0x20001088.

---

## Cross-Reference: Which Functions Touch What

| Function | Name | Key Offsets Accessed |
|----------|------|---------------------|
| FUN_08019e98 | `scope_main_fsm` | +0x04, +0x05, +0x14, +0x2C, +0x2D, +0x2E, +0x2F, +0x30, +0x32, +0x260-0x33C (cal tables), +0xDB0, +0xDBA-DBB, +0xDBC-0xE0C |
| FUN_0801d2ec | `scope_mode_timebase` | +0x04, +0x05, +0x14, +0x15, +0x16, +0x17, +0x1A, +0x1C, +0x2D, +0x2E, +0xDB4-DB6 |
| FUN_0801f6f8 | `scope_measurement_engine` | +0x04, +0x05, +0x15, +0x2D, +0x2E, +0x50-0x230 (measurement scratch array) |
| FUN_08020930 | `scope_mode_display_settings` | +0x14, +0x34, +0x35, +0x38 (menu_state) |
| FUN_08001c60 | `siggen_configure` | +0x02, +0x03, +0x04, +0x05, +0x14, +0x15, +0x16, +0x2D, +0x2F, +0x31 |
| FUN_080018a4 | `gpio_mux_portc_porte` | +0x04, +0x14, +0x2D, +0x260-0x2C4 (cal tables) |
| FUN_08001a58 | `gpio_mux_porta_portb` | +0x05, +0x14, +0x15, +0x16, +0x2D |
| FUN_08037800 | `spi3_acquisition_inner` | +0x04, +0x14, +0x16, +0x1C, +0x33, +0x352-353, +0x5B0-0xDAF (ADC buffers), +0xDB0-DB8 |
| FUN_08029a70 | `scope_draw_measurements` | +0x14, +0x2D, +0x350-351, +0xDBE, +0xE08-E0C |
| FUN_08034078 | (RTOS helper) | +0x02, +0x03, +0x05, +0x14, +0x15, +0x1A, +0x20, +0x24, +0xDB4-EF0 |

---

## Notes

1. **Offsets are stable across firmware versions** — the same structure layout appears in V1.0.3 through V1.2.0, suggesting it's defined as a single C struct compiled from the original source.

2. **The `sl` register convention** — The AT32 Cortex-M4 calling convention uses r10 (sl) as a scratch register, but the stock firmware uses it as a persistent pointer to this structure across many functions. This is likely a global variable whose address is loaded into sl at function entry.

3. **Packed fields** — Several bytes pack multiple fields:
   - +0x15 (`channel_config`): hi nibble = active channel, lo nibble = channel count
   - +0x20-0x24 (`config_bitfields`): per-bit configuration flags

4. **Unknown offsets** — There are still gaps, particularly in the 0x06-0x0F range, 0x3E-0x4F range, and 0xE30-0xF2C range. These likely contain less-frequently-accessed parameters (screenshot config, language, backlight timeout, etc.)
