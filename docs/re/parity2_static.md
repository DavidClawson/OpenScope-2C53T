# Parity round 2, static RE: 20M limit (C13), zoom while paused (C14), display/sound/auto-off/USB sharing/logo (C15)

Scope: static reading of the stock V1.2.0 image only (`v120.bin`, linked at 0x08007000). Nothing here touched hardware, and
no OpenScope code is changed by this note. Everything below is what the next implementer needs, with stock addresses as evidence.

Tags: **[V]** read from code or data (address given), **[H]** hypothesis, **[L]** needs live stock (SWD) or a measurement.
`ms` = stock state struct at 0x200000F8 (`ms[n]` = 0x200000F8 + n). Method: Thumb-2 disassembly plus small scripts that resolve
movw/movt, `ldr =`, `adr`/`addw pc` references (stock builds most tables with `addw rX, pc, #imm`, which a plain
`ldr`-literal search misses; that is why the language table at 0x08014198 looked unreferenced).

Live cross-check: four SWD dumps of the stock state struct on unit #2 running stock V1.2.0 (2026-11-11, 4104 bytes from 0x200000F8 each)
agree with every field decoded below (ms[0x14] = 3 for two channels and 1 for CH1-only, ms[0x15] = 2 / 1, ms[0xF60..0xF65] =
2,100,1,0,0 = English, brightness 100, sound 1, startup off, auto-off off; ms[8..11] = 0).

## 0. Summary (what to do)

| Item | Finding | OpenScope action |
|---|---|---|
| C13 20M limit | The row exists in stock's string table and OK handler (ms[8]/ms[9] toggle) but is **unreachable**: the channel menu draws 4 rows and the down-key limit is row 3. The toggle writes no GPIO/FPGA word and no consumer was found. | Do not implement. Remove the "20M" row from the OpenScope channel menu (or label it "not in stock"). Keep the config byte. Close C13 as "dead in V1.2.0". One optional SWD test settles it for good. |
| C14 zoom paused | Stock keeps per-channel 1024-byte raw records and a captured-state tuple; on STOP any timebase/range/position change only **re-renders** (no re-arm). Horizontal factor = 1000 x display ns per px / record ns per sample (0x08030A70). Vertical is a closed formula. Interpolation kernel 0x0803D5D4 is fully decoded: a Q15 16-tap polyphase FIR. | Build a pure `paused_view` module (spec in section 3) and freeze the capture metadata at STOP. Horizontal zoom-out decimation details are [L]. |
| C15 brightness | PB8 = TMR10 CH1 PWM, ARR 99, 1 MHz count (10 kHz), duty = ms[0xF61] percent, 25..100 step 5, factory 100. | Implement exactly (section 4.2). |
| C15 sound | **TMR11 CH1 on PB9** (not TMR4). ARR 24, 50 kHz count (2 kHz tone). Key click writes C1DT = volume (0..10, factory 1). | Implement TMR11 PWM; resolves the TMR4/TMR11 dispute (section 4.3). |
| C15 auto-off | ms[0xF65] = 0/1/2/3 = off/15/30/60 min; idle counter ms[0xF6C] reset by every handled key; thresholds 900/1800/3600 ticks at 0x08040710..0x08040724; action = settings save (sig 0x55) then PC9 low. | Idle timer module; tick period [L] (assumed 1 Hz). |
| C15 channel enable | ms[0x14] bitmask; toggle at 0x0800BF7E..; GPIO effect PC1/PC2 per mask (matches `fpga_set_channel_mask`). | UI toggle in channel menu (section 4.1). Fixes the ch2.enabled persistence trap. |
| USB Sharing | Entered from the Settings page (ms[0xF68] = 4 is Settings itself, see the correction in section 4.5); the OK arm at 0x0800BF08 powers the USB core down/re-enumerates [H]; bring-up not traced; MSC device on the USB FS block (0x40005C00); shares FatFs volume "2:" (14 MB FAT12 at W25Q 0x200000..) [H]. | L-size, see section 4.5; **never expose a writable view of 0xD00000+**. |
| Boot logo | Boot opens `2:/LOGO/LOGO2C53T.jpg`; if it opens it is decoded at (0,0) with the backlight at 0, then the backlight is set to the stored brightness; otherwise an embedded image is drawn. | Read-only FAT12 + small JPEG decoder at boot (section 4.6). |

## 1. Shared facts decoded this pass (new or corrected)

ms fields, all [V] unless noted:

| Field | Meaning | Evidence |
|---|---|---|
| ms[0..1] | coupling CH1/CH2, 1 = DC, drives PD12/PD13 immediately | OK handler row 2 at 0x0800C836 (flip, then GPIOD SCR/CLR 0x40011410/0x40011414 bit 12/13) |
| ms[2..3] | vertical range index per channel (0..9) | UP/DOWN handlers, dumps (6,6) |
| ms[4..5] | vertical position int8 (+-100) | dumps (194 = -62) |
| ms[6..7] | probe x1/x10 per channel | row 1 at 0x0800C82E, label table 0x0804C7BC |
| ms[8..9] | "20M limit" per channel, persisted | row 4 at 0x0800C826, restore 0x0802CE54/0x0802CE58 |
| ms[10..11] | per-channel ON/OFF whose menu label is "FFT:" (row 3) [label V, meaning H], persisted | row 3 at 0x0800C85E, painter 0x08013560 |
| ms[0x14] | **channel-enable mask** (bit0 CH1, bit1 CH2) | OK handler 0x0800BF7E: `tst (type-2)`, clear via `rsb`, set via `orr`; dumps 3 and 1 |
| ms[0x15] | enable-derived code {mask0:0, mask1:1, mask2:0x12, mask3:2} [H: layout/bank code] | 0x0800C90A tbb, 0x0802E186..0x0802E1E0; dumps |
| ms[0x16] | **trigger source channel** (0 = CH1, 1 = CH2), not RUN/STOP | toggled by the "Channel:" row 0x0800C874; used as `<<10` index into the records at 0x08021E2C |
| ms[0x17] | trigger mode 0 NORMAL / 1 AUTO / 2 SINGLE | already known |
| ms[0x18] | trigger edge | row 0x0800C894 (0/1 wrap) |
| ms[0x2D] | display timebase index (0..0x1C) | L/R handler stores it at 0x0800B3D2 |
| ms[0x2E] | **RUN flag** (1 running, 0 stopped) | RUN/STOP handler 0x0800BE5E (1 to 0) and 0x0800C252 (0 to 1) |
| ms[0xDC0] | timebase index **at capture** (updated by 0x08030A8C only while ms[0x2E]==1) | dumps: equals ms[0x2D] while running |
| ms[0xDBE] | channel mask at capture | 0x08030A90 |
| ms[0xDC1..2] | range index at capture per channel; ms[0xDC3..4] position at capture (int8) | 0x0802128C..0x021300 and dumps (-62,-70 = ms[4..5]) |
| ms[0xDBC] | trigger anchor T (low 15 bits) | 0x08021DE6 |
| ms[0xDB4/0xDB6] | u16 start column (0) and width (301) | 0x0800B744 stores 0x012D0000 |
| ms[0xE08] (u64) | display/record rate ratio x1000 (1000 when display rate = record rate) | writer 0x08030B42; dumps = 1000 |
| ms[0xE04] | interpolation factor 1000/ratio (u8), only updated while running and ratio <= ~992 | 0x08030B64..0x08030B6C |
| ms[0xDFC + 4*ch] | pointer to the interpolated/stopped copy for channel ch (0 when none) | 0x0800C252 frees it on RUN |
| ms+0x5B0 / ms+0x9B0 | raw 1024-byte records CH1 / CH2 (u8); with ms[0x14]!=3 and tb<4 the two halves interleave into one 2048-byte record (parity picks half, ms[0x350/0x351] trims) | 0x08021E2C..0x08021EC8 |
| ms+0x356 + ch*301 | 301-byte per-channel display array (u8 y, 28..228) | 0x0802203x, 0x080212B6 |
| ms[0xF60] | language (1 = Chinese, 2 = English) | OK at 0x0800BEDC stores row+1 |
| ms[0xF61] | brightness percent | section 4.2 |
| ms[0xF62] | sound volume 0..10 | section 4.3 |
| ms[0xF64] | startup mode (row index: off, scope, siggen, meter) | OK 0x0800C558 |
| ms[0xF65] | auto-off mode 0..3 | OK 0x0800C564 |
| ms[0xF68] | main state: 0 = home (mode selection), 1 = meter, 2 = scope, 3 = signal generator, 4 = **Settings**, 9 = Oscilloscope Settings page (corrected: an earlier version of this note read 4 as USB sharing) | home OK handler 0x0800BD90 (F68 = selection + 1), long BACK 0x0800D834, acquisition gate 0x08020EAC |
| ms[0xF69/0xF6A/0xF6B] | menu level, menu type (low nibble: 3 = CH1, 4 = CH2, 1 = "Sound and light" sub-page), selected row | painter 0x08013346 |
| ms[0xF6C] | u16 idle counter | section 4.4 |

Persisted record (restore 0x0802CE2C..0x0802CF40, writer 0x080293BC into flash page 0x08006000): record+0x2C = {lang, brightness, sound, ?},
+0x30 = {startup, auto-off, ms[8], ms[9]}, +0x34 = {ms[10], ms[11]}. Factory defaults (0x080294A0..0x0802957x): ms[0xF60..63] = 01 64 01 00
(Chinese, brightness 100, sound 1), ms[0xF64..65] = 0 0, ms[8..11] = 0, ms[0x14] = 3, ms[0x2D] = 13, ms[0x17] = 1 (AUTO), ms[0x16] = 0.
(The dumps show lang = 2: the unit was set to English.)

## 2. C13: 20M bandwidth limit

Findings.
- The channel-settings menu is one painter, 0x08013338 (menu types 3 and 4 enter at 0x08013368). Its labels come from a language-grouped table,
  stride 5 words, base 0x08014198 (reached with `addw r1, pc, #0xCC0` at 0x080134D6; selected by `ms[0xF60] != 1` times 20 bytes):
  index 0 "Channel:", 1 "Probe:", 2 "Coupling:", 3 "FFT:", 4 "20M Limit:" (English row at 0x080141AC..0x080141BC; Chinese at 0x08014198..).
  The ON/OFF text pair is at 0x080141CC (`addw` at 0x08013564), the toggle icons 79/80.jpg at 0x080141C0.
- The painter loops `sl = 0..3` (0x08013378 init, 0x080133B2 `cmp sl,#4`): **four rows only**. Its per-row value dispatcher (`tbb` at 0x08013558,
  bytes 07 12 1A 02) maps row 0 to ms[0x14] bit (channel on/off), row 1 to ms[ch+5] (probe X1/X10), row 2 to ms[ch-1] (AC/DC), row 3 to ms[ch+9] (OFF/ON, label "FFT:").
- Row-advance handler (0x0800AC9E, inside the key-slot-11 handler): `cmp row, table[type]` with the byte table at 0x0800B1F0 = {1,1,1,3,3,2,3,8}; for types 3/4 the maximum row is **3**.
  So row 4 (the 20M row) can never be selected. Extra detail at 0x0800ACB6..0x0800ACC4: in the CH menus the row only advances if that channel's ms[0x14] bit is set, so a disabled channel's menu
  has the single row "Channel: OFF".
- The OK handler still has a row-4 case: 0x0800BF74 `tbh` entry 4 to 0x0800C826 (`ldrb [ctx+5]`, then the common flip at 0x0800C864: `!x`), i.e. it
  toggles ms[type+5] = ms[8]/ms[9]. Unlike coupling (row 2), which writes PD12/PD13 inline, this case writes no GPIO, no FPGA word and posts nothing
  except the generic redraw (0x0800CB1E..0x0800CB44). The only direct accesses to ms[8]/ms[9] in the whole image are the settings save/restore
  (0x0802956E..0x08029648, 0x0802CE54..0x0802CE60). Channel-indexed readers (`ldrb [base+ch]`) cannot be excluded by static search, but none was found at
  the places that drive the front end (relay/GPIO writes sit behind ms[2..3], ms[0x14], ms[0..1]).
- The manual (Chinese/English/RU/PT) never mentions a bandwidth limit; its spec says "50 MHz (dual channel independent 50 MHz)".
- Corrections to earlier notes: "20M Limit" is not a drawn row in V1.2.0 and the old "USART bit" theory has nothing to attach to; row 3 on screen is "FFT:",
  not 20M.

Verdict [V for reachability, H for "no hardware effect"]: V1.2.0 ships a dead 20M-limit entry. A user cannot toggle it, and toggling it in code does not
reach the front end.

OpenScope must: show no 20M row (today's "n/a" row can go); keep `bw` in config v3 untouched; do not compile in any bandwidth GPIO. Record "C13: stock has no
reachable 20M limit; parity = absent" in the backlog.

Only live SWD can settle: write ms[8] = 1 (0x20000100) on a running stock unit, then diff GPIOA..E ODR, SPI3 traffic and TMR/DAC registers; set ms[10] = 1 to learn
what the "FFT" per-channel flag does. If nothing moves, close for good. (PLAN's S4 "zl" step: not needed.)

## 3. C14: zoom while paused

What stock does with the record [V].
1. Capture (RUN): while ms[0x2E] == 1 the function 0x08030A70 copies ms[0x2D] to ms[0xDC0] and ms[0x14] to ms[0xDBE]; the acquisition function 0x08020E98 fills the raw
   records (ms+0x5B0 CH1, ms+0x9B0 CH2, 1024 u8 each; at tb < 4 with one channel the two halves form one 2048-sample interleaved record, parity = which half,
   ms[0x350/0x351] one-sided trims, `cmp ms[0x2D],#4` at 0x08021DFC/0x08021E7E). Anchor T = ms[0xDBC] & 0x7FFF.
2. STOP: RUN/STOP handler 0x0800BE5E sets ms[0x2E] = 0 and calls 0x0803B078. The record and the captured tuple (ms[0xDC0], ms[0xDBE], ms[0xDC1..4]) stay frozen.
3. Any timebase change while stopped (L/R handler 0x0800B3CC, stores ms[0x2D]) skips the FPGA re-arm (the `ms[0x2E] != 0` test at 0x0800B6AA fails), takes the common tail
   0x0800B748: `bl 0x08030A70` (recompute ratio, see below), `bl 0x0803B2F8`, queue message 2 (redraw) and tail-calls the cursor engine 0x08025FC0.
   The vertical range/position keys do the same (`ms[0x2E]` checks at 0x0800A6D8/0x0800A830). Nothing re-arms; the unit "zooms" the frozen record.
4. Horizontal ratio, exact (0x08030A70..0x08030B6C): `ratio_x1000 = 1000 * (display_ns_per_px) / (record_ns_per_sample)`, stored as u64 at ms[0xE08].
   - display: M[tb%3] x 10^(3 + tb/3) with M = {1,2,5} (table 0x080466C8) is div_ns x 100; 25 px per div, so ns/px = div_ns/25;
   - record: period table u64 at 0x080465E0 in 0.4 ns units (10,10,10,10,20,50,...), x2 when captured tb < 4 and captured mask == 3 (0x08030AE8..0x08030B14);
   - check against the dumps: tb 5, mask 3: 500 ns/div = 20 ns/px, record 20 ns, ratio 1000 (dump: ms[0xE08] = 1000); tb 3 one channel: 100 ns/div = 4 ns/px, record 4 ns, 1000; tb 4, mask 3: 1000 (dump). Computed, no dump: tb 0 one channel: 10 ns/div = 0.4 ns/px against a 4 ns record gives 100 (factor 10); tb 0 with both channels the record is 8 ns (the x2 above), so 50 (factor 20); tb 3 with both channels 500 (factor 2).
   - While running, `ms[0xE04] = 1000 / ratio` (interpolation factor: 10, 5, 2 for tb 0, 1, 2 with one channel; 20, 10, 4, 2 for tb 0..3 with both channels) is stored if (ratio >> 3) <= 124, i.e. ratio <= 999 (0x08030B4E..0x08030B6C); otherwise the old value stays (the cal3 dump shows a stale 2 at tb 5). While stopped it is left alone.
5. Stopped re-render (0x08021178..0x08021348, per column `r6` = 0..300, per channel):
   - source sample: direct path `idx = (T & 0x7FFF) + col - ms[0xDC8] - 150`, wrapped at 1024 (or 2048 merged) plus `ch << 10` (0x08021232..0x08021282); with an interpolated copy (ms[0xDFC+4ch] != 0)
     the byte is read from that copy instead (0x08021224..0x0802122E);
   - **vertical re-scale, closed form** (0x080212BC..0x08021348): `y = clamp(128 + pos_now + k * (raw - 128 - pos_cap), 28, 228)` with `k = V[range_cap] / V[range_now]`,
     `V = {10,20,50,100,200,500,1000,2000,5000,10000}` (u16 table 0x080465CC, mV/div), `pos_cap` = ms[0xDC3+ch], `pos_now` = ms[4+ch];
     the float constants are 128.0 (0x43000000), -128.0, 228.0 (0x43640000), 28.0;
   - the column limit is clipped to `301 - ms[0xDE0]` and to `ms[0xE08] * width / 1000 * (ms[0xE04] or 1)` (0x08021178..0x080211D4).
6. Interpolation kernel (V1.1.0 "sinusoidal interpolation"), `0x0803D5D4(src_u8, dst_u8, n_in, ratio)`, fully decoded: for each input position `n` in 0 .. n_in-16 and phase `p` in 0 .. ratio-1,
   `out[n*ratio + p] = clamp((sum_{k=0..15} c[S(ratio)+p][k] * in[n+k]) >> 15, 0, 255)` with signed Q15 coefficients (64-bit accumulate),
   so the output sits between taps 7 and 8. Coefficient table: 0x080466E0, 64 bytes per phase (16 x i32). Set start `S(r) = sum of {2,4,5,8,10,20,25,40,50,80} for each threshold below r`
   (cascade at 0x0803D5DC..0x0803D620), so ratios 2,4,5,8,10,20,25,40,50,80 exist; phase 0 is the identity (c[7] = 32768). Half-sample set (ratio 2, set 1):
   -112 189 -424 883 -1683 3105 -6197 20469 | 20861 -6567 3436 -1957 1093 -564 261 -125, DC gain 32668/32768. This is a windowed sinc. (The same function is used on the live
   path for tb 0..2, see call sites 0x080218D8, 0x08021C92, 0x08022024, 0x0802211C.)
7. Stop copy for the trigger search: 0x08021EF0 copies 25 samples s[T-7 .. T+17], runs the kernel with ratio ms-derived, then re-finds the crossing against `ms[0xDC6] ^ 0x80`
   with a 2-point test (0x08021F02..0x08021F48). (Matches ADDENDUM_A.)

OpenScope spec (new pure module `src/dsp/paused_view.c/.h`, host-tested; the UI only calls it):
- At STOP, freeze `{tb_cap, mask_cap, merged?, range_cap[ch], pos_cap[ch], anchor T, hpos_cap}` next to the frozen record (1024 per channel; merged 2048). While stopped, tb/range/position changes **must not re-arm
  or reconfigure the FPGA**; they only change display parameters and request a redraw. (This also removes the AUTO free-run incoherence on a stopped frame.)
- `paused_ratio_x1000(tb_now, tb_cap, mask_cap)` = `1000 * (div_ns[tb_now]/25) / (period_ns[tb_cap] * ((tb_cap<4 && mask_cap==3) ? 2 : 1))`, using OpenScope's measured rate table (stock's 0.4 ns table gives the same values; for the 0x05 / 0x0B / 0x0C entries the stock table is the reference).
- `paused_y(raw, range_cap, range_now, pos_cap, pos_now)` follows the closed form above, clamped to 28..228. The 128 is **stock's** raw baseline; OpenScope's 0 V is record 100 + pos with the calibration page (HEAD 113ce11), so the centre constant must come from OpenScope's own record/cal baseline and must not be copied. Use scope_cal for volts; this is only the stored-trace display mapping.
- Horizontal: for display ratio R < 1000 (zoomed in beyond the record) use the 16-tap polyphase interpolator. Generate the taps in code (windowed sinc, 16 taps, Q15; compare
  to the half-sample set above in a unit test, tolerance a few LSB); do not need to copy stock's table. Factors needed: 10, 5, 2 with one channel and 20, 10, 4, 2 with both (all in stock's set), plus any ratio a paused zoom-in reaches. For R >= 1000 (zoomed out)
  [H] pick or max/min by position; stock's exact zoom-out sampling is [L] (the loop at 0x08021178 is a per-column time to index map; no peak detect exists in stock). Clip to the record and leave the rest of the trace blank like stock (`301 - ms[0xDE0]` limit).
- Tests: ratio table against the dump values (1000 at tb 5/mask 3, tb 3/mask 1, tb 4/mask 3) and the computed ones (100 at tb 0/mask 1, 50 at tb 0/mask 3, 500 at tb 3/mask 3); y-map identities (same range and position gives identity, 2x range ratio doubles the offset from centre, clamp at 28/228); kernel: DC gain, phase-0 identity, symmetry of the half-sample set, impulse response vs the stock half-sample taps. Mutation target: the k = V[cap]/V[now] orientation.
- Size: M (as in PLAN); the display plumbing is the larger half.

Only live SWD can settle: after STOP, step the timebase +-1..+4 and read ms[0xDC0], ms[0xE08], ms[0xE04], ms[0xDE0], ms[0xDC8], ms[0xDFC..0xE03] and the display arrays ms+0x356..+0x5B0 (the horizontal zoom-out sampling rule and the
interpolated-copy layout); repeat with a range change. Also confirm ms[0xDE0] (clip) and the use of ms[0xDC8] (hpos).

## 4. C15: channel enable, brightness, sound, auto-off, USB sharing, boot logo

### 4.1 Channel enable (UI for D4)  [V]
OK handler row 0 (0x0800BF7E): `type` = ms[0xF6A]&15 (3 = CH1, 4 = CH2), bit = type-2. If the bit is set, clear it (`and` with `1-type`), flip the stored positions (`eor 0x80` into the 301-byte display rows and the position
bytes), else set it; then `bl 0x08030A70`, set ms[0xDBA] = 10 if ms[0x2D] <= 3, and apply per-mask GPIO at 0x0800C980..0x0800C9B2 (identical copy at 0x0802E202..0x0802E23E):
set PD2 (0x40011410 bit 2), then mask 1: PC2=1, PC1=1; mask 2: PC2=0, PC1=0; mask 3: PC2=1, PC1=0; mask 0: untouched. This equals OpenScope's own finding in `fpga.c:3369-3373`
(`fpga_set_channel_mask`). Extra: ms[0x354] low 7 bits are forced/cleared (0x0800C9B8..0x0800C9C8), ms[0x15] gets the layout code.
OpenScope: add a "Channel: ON/OFF" row (per channel) to the channel menu that sets `ch.enabled` and calls `fpga_set_channel_mask`; refuse to disable the last channel (stock allows mask 0, which blanks the screen);
persist; on boot re-apply the mask (fixes the persistence trap). Keep 250 MS/s interleave gated by `mask != 3 && tb < 4` as is.

### 4.2 Brightness (PB8)  [V]
- Timer: TMR10 (base 0x40015000). Init in the mode-setup block at 0x0802B0CE..0x0802B2F0: PSC = clock/1e6 - 1 (0x0802B1F4..0x0802B210; `clock` is the AHB field of the clock struct filled by 0x08031430, sp+0x44 = offset 4, not the APB2 timer clock; with AHB 240 MHz and APB2 /2 the timer clock is also 240 MHz, so 239, i.e. 1 MHz count), ARR = 99
  (`str 0x63` at 0x0802B20C), CCMR1 OC1M = 0b111 (PWM mode 2), CCER = CC1E | CC1P (inverted polarity, so the pin is high while CNT < CCR; duty = CCR %), CR2.OIS1 set, EGR UG,
  BDT.MOE (0x40015044 bit 15), CEN; initial C1DT = 100 (0x0802B272). PB8 and PB9 are configured by the same struct (pins 0x100 then 0x200, AF push-pull, 0x0802B0F2..0x0802B1DE);
  APB2 clocks for TMR10/TMR11 are bits 20 and 21 of the APB2 enable register (0x0802B194, 0x0802B1AC).
  OpenScope's timer clock is 240 MHz (sclk 240 MHz, APB2 /2 means 2x timer clock; `at32f403a_407_clock.c:62-79`), so use PSC 239.
- Level: `C1DT = ms[0xF61]`, percent, **25..100 step 5**, factory 100. Right: `ms < 100 -> ms += 5` (0x0800BA14..0x0800BA28), left: `ms >= 26 -> ms -= 5` (0x0800B448..0x0800B458); both write 0x40015034 immediately. It is re-applied after every mode switch (0x0802E1FC).
- Sequencing: at boot `C1DT = 0` (r4 = 0 at 0x0802B442), the splash/logo is drawn, then `C1DT = ms[0xF61]` (0x0802C064..0x0802C06C). At power-off the settings saver first writes C1DT = 0 (0x080293EE).
- The edits gate on menu type 1 = the "Sound and light" sub-page; row 0 edits sound, row 1 edits brightness (0x0800BADC vs 0x0800BA10; on-screen order not checked).
OpenScope: new `backlight_pwm` driver (PB8 as TMR10_CH1 AF-PP, `tmr_base_init(TMR10, 99, 239)`, PWM A with normal polarity is equivalent to stock's PWM B + inverted polarity, MOE on); keep today's "hold PB8 low until the first frame"
(main.c:969-1051) and then set the stored level; `lcd_set_brightness(uint8_t pct)` maps to C1DT; store in settings; Settings row 25..100 step 5; clamp. Host test: step law (never below 25 or above 100), duty = CCR/(ARR+1).
Open [L]: PB8 is TMR10_CH1 only if the default pin mux applies (stock sets no remap); the bench will show it by the backlight reacting.

### 4.3 Sound (buzzer): TMR11 CH1 on PB9, **not TMR4_CH4**  [V]
- The buzzer compare register is 0x40015434 = TMR11 + 0x34 = C1DT (siggen/meter notes and the meter checker agree). TMR11 init at 0x0802B27x..0x0802B2E4 (`r6 = 0x40015404`): ARR 24 (`str 24, [r6,#0x28]` at 0x0802B29E),
  PSC = AHB clock / 50 000 - 1 (0x0802B1F6..0x0802B298: `clk/16 * 0x0A7C5AC5 >> 32 >> 7 = clk/50000`, so 4799 at 240 MHz), PWM mode 2, CC1E|CC1P, MOE, CEN, C1DT = 0. So a **2 kHz** tone (50 kHz count / 25) with duty = C1DT/25 (0..40 %).
  PB9 comes from the same AF-PP struct as PB8.
- No code in the image touches TMR4 (zero movw/movt hits in 0x40000800..0x40000860). PB9 is also TIM4_CH4 under the default mux, which is why the DOOM port works with TMR4; stock uses TMR11. The siggen note "PB9 / TMR4_CH4 [C]" and the HARDWARE_PINOUT line are therefore not what stock does (both are electrically possible).
- Volume: `ms[0xF62]` 0..10 (right: `ms <= 9 -> +1` 0x0800BADC; left: down to 0), factory 1. It is used directly as C1DT, so volume 10 = 40 % duty.
- Triggers [V]: (a) every key event handled by the key task (0x08040038): `if ms[0xF62] != 0: C1DT = ms[0xF62]` and the idle counter ms[0xF6C] = 0; (b) the meter SoC's buzzer line via EXTI3 (0x08009C10..0x08009C64): writes the constant **10** to C1DT (not scaled by the volume,
  ignores volume 0), only in Auto and Continuity (meter-parity checker); (c) a mode-change/boot block at 0x0802E370..0x0802E3E4 sets C1DT = volume (if non-zero), runs a SysTick delay and clears C1DT (length depends on 0x20002B20, [L]).
- Length: the counter 0x20002D67 is incremented once per call of the key-scan function 0x080400B8 (0x08040508..0x08040532, reached on every call; same function as the 70/72-count key counters at 0x08040490/0x080404A8) and C1DT is cleared when it reaches 11 (0x08040526..0x0804052C, duplicate at 0x08029D20..0x08029D28). The EXTI3 beep also zeroes it (0x08009C66), so both beeps last 11 scan ticks.
  The scan period is not proven, but the same function points to **10 ms** [H]: every 51 calls (counter ms[0x45], 0x080405C2..0x08040658) it zeroes TMR2/TMR5 CNT (0x40000024/0x40000C24), and 50 calls later, while RUN, stores `ms[0x50] = 2 * TMR2.CNT`, `ms[0x54] = 1e9 / ms[0x50]` (same for TMR5 into ms[0x80]/ms[0x84]). A period in ns from `1e9 / x` means x is in Hz, so the 50-call gate is 0.5 s and one call is 10 ms (20 ms if the counters run with an ETR /2 prescaler; 1 ms would need /10, which the ETR does not offer). At 10 ms the key counters read naturally: 70 ticks = 0.7 s long press, release at 2..69 ticks = short press. So the click is probably **~110 ms**, which agrees with `BUZZER_SOC_TICK_MS_DEFAULT 10` in `continuity_buzzer.c` (11 x 10 ms). Note that OpenScope's `key_model.h` reads stock's 70 as a 140 ms press confirm at 2 ms per tick; if 10 ms holds, stock's 70 is the long-press threshold (0.7 s). Measure PB9 on the bench before changing either.
OpenScope: `sound_pwm` driver (TMR11 CH1, PSC 4799, ARR 24, PWM A, normal polarity), `sound_click()` called from the input task for each key event when volume > 0, `sound_set_volume(0..10)`, continuity beep through the same driver at constant duty 10; replace the GPIO square wave in `continuity_buzzer.c` once the bench confirms. Persist volume, factory 1.

### 4.4 Auto-shutdown  [V except the tick]
- Setting: ms[0xF65] = settings row (0x0800C564): 0 off, 1 = 15 min, 2 = 30 min, 3 = 60 min; factory 0.
- Counter: u16 ms[0xF6C]. Handler 0x080406C8 (which also calls the power-key routine 0x08009C78) runs periodically; if ms[0xF65] != 0 it increments ms[0xF6C] and compares: mode 1 `> 900` (0x08040710), mode 2 `> 1800` (0x08040724), mode 3 `> 3600` (0x0804071A).
  The counter is reset to 0 by every key event processed (0x08040044) and at the start of the settings saver (0x080293F8). No other writer (so signal activity, USB and charging do not keep the unit awake; check live).
- Action: 0x0804072C `movs r0,#0x55; bl 0x080293BC` = settings save with signature 0x55 (the saver zeroes the backlight, rewrites the page, then drops PC9, 0x08029D3A) = power off.
- The handler's period is unknown: 900/1800/3600 = 15/30/60 min only if it runs at 1 Hz [H]. Supporting hint: the same handler (0x080406D6..0x080406F0) latches ms[0xF70] to ms[0xF6F] and ms[0xDB2] to ms[0xDB1] and zeroes both, a per-interval rate snapshot (ms[0xDB2] is the acquisition counter that scope-parity Q9 wants counted over 10 s), which fits a slow (1 s) interval. The handler has no direct caller or pointer in the image (probably a registered callback), so it was not traced to its timer. [L]
OpenScope: `autooff` pure module (`autooff_tick(st, dt_s, key_event)`, `autooff_due()`), settings field, row in Settings (Off/15/30/60 min), on due: flush settings, backlight off, drop PC9 (stock's own sequence). Test: reset on key, thresholds, off = never, saturation of the u16. Add a warning popup before power-off (beyond stock, optional).

### 4.5 USB Sharing (mass storage)  [V for mechanism, H for the shared volume]
- Correction: ms[0xF68] = 4 is the Settings page, not a USB Sharing state. The home page's OK sets F68 = selection + 1 and Settings is item 3; long BACK from 4 highlights "Settings"; the settings OK/BACK handlers act on F68 = 4. 0x0800BF08 sits inside the OK handler's settings arm, so USB Sharing is a row of Settings [V for 4 = Settings, H for where USB Sharing lives].
- Entry: settings item "USB Sharing", in the OK handler's settings arm (0x0800BF08, where `ms[0xF68] = 4` is the Settings state already), then `0x40005C40 |= 2` and `0x40005C60 |= 2` (AT32F403A USB FS block at 0x40005C00; 0x40005C40 is CNTR in the F1-style map and bit 1 is PDWN, so this step probably powers the USB core **down** to force re-enumeration [H]), redraw message. The MSC bring-up itself happens elsewhere (not traced; the device core sits at 0x08030500..0x08031300). State 4 (Settings) is polled by the key handlers (0x0800C58E). The device exposes a mass-storage interface: descriptor bytes
  `09 02 .. 01 01 04 C0 32 | 09 04 00 00 02 08 06 50` (class 8, SCSI transparent, bulk-only) and two bulk endpoints (0x81, 0x01) are visible in the compressed .data at 0x080BE4F0 (partly packed, [H] for the rest). SCSI handler not traced.
- What it shares: stock's FatFs mounts "2:" and "3:" at boot (0x080357BC..0x080358EA: `f_mount`, `f_mkfs` if the result is 13 = FR_NO_FILESYSTEM, then `f_mkdir` for `2:/LOGO` (0x080BBC58), `2:/Screenshot file` (0x080BC1B4), `2:/Screenshot simple file` (0x080BC18B), `3:/System file` (0x080BC1A5),
  and a probe for `3:/System file/9999.bin` that sets ms[0xF6E]). `flash_regions.c` already documents the two FAT12 volumes: "3:" = W25Q 0x000000..0x1FFFFF (UI JPEGs), "2:" = 0x200000..0xFFFFFF. The manual's logo recipe (copy `LOGO2C53T.jpg` into the drive's `LOGO` folder) shows the host sees "2:". [H] that "3:" is hidden.
- Hazard for OpenScope: the stock FAT of "2:" nominally covers 0xD00000..0xFFFFFF, where OpenScope keeps fwcache/usercal/settings/modules/scratch/cal backup (`flash_regions.c:72-92`). A writable share would let a host overwrite them.
OpenScope proposal (size L): (1) a **read-only** MSC mode first (`usb share` shell command and a Settings entry), LUN capacity = the whole "2:" volume but reads beyond 0xD00000 return zeros and all writes are refused; (2) writable only inside 0x200000..0xCFFFFF after an explicit command, via the region layer (`FLASH_REGION_USERVOL` is READONLY today; needs a new write class), FAT left intact; (3) stop CDC while sharing (stock exposes only MSC), restore after eject; (4) tests: sector math, write clamp, zero fill. Do not format or re-create FAT; if the stock FAT is missing, do nothing.
[L]/host-only on stock (no SWD needed): `lsusb -v`, `dd`, `fsck.fat -n` on a unit in USB Sharing give descriptors, LUN count, capacity and the FAT layout.

### 4.6 Boot logo  [V]
- Boot (0x0802B420..0x0802B456): `f_open("2:/LOGO/LOGO2C53T.jpg")` (string 0x080BC66B). If it succeeds (result 0 at 0x0802B44C, `beq 0x0802C04A`): C1DT = 0 (backlight off), `bl 0x08039F6C(path, x=0, y=0)` decodes and draws the JPEG (same routine as the `3:/System file/%d.jpg` assets), `C1DT = ms[0xF61]`, then a SysTick-based hold (0x0802C0B8..0x0802C124).
  If it fails the embedded logo (u16 width/height header at [r8], pixel writes through the LCD bus, 0x0802B45A..) is drawn.
- Format: not enforced in code that was read; the manual says to check name, pixel size and format. [H] full-screen 320x240 baseline JPEG (the decoder looks like a TJpgDec-style routine reading through FatFs).
OpenScope proposal (size M after the read-only FAT12 reader exists): at boot, after the splash, try to read `2:/LOGO/LOGO2C53T.jpg` through a read-only FAT12 reader on the user volume (never mount read-write, never format), decode with a small baseline decoder (tjpgd or picojpeg, check licence), draw with the backlight still off, then raise the backlight; any failure keeps the built-in splash. Shell `logo info` to report found/size/decode result.

## 5. What only live stock (SWD) can settle
1. C13: ms[8] (0x20000100) = 1, ms[9] (0x20000101) = 1, ms[10] (0x20000102) = 1 each in turn; diff GPIOA..E ODR (0x4001080C, 0x40010C0C, 0x4001100C, 0x4001140C, 0x4001180C), SPI3 TX words, DAC and TMR registers. Settles "20M limit has no effect" and the "FFT" per-channel flag.
2. C14: STOP then timebase +-1..4 and range steps; dump ms[0xDC0], ms[0xDBE], ms[0xDC1..4], ms[0xDC8], ms[0xDE0], ms[0xE04], ms[0xE08], ms[0xDFC..0xE03] and ms+0x356..+0x5B0 before and after each step (zoom-out sampling rule, interpolated-copy size, `ms[0xDC8]`).
3. TMR10 and TMR11 registers (0x40015000..0x40015048, 0x40015400..0x40015448) and GPIOB CFGHR/mux (PB8/PB9 function): confirms PSC 239 / ARR 99 and PSC 4799 / ARR 24, the polarity bits and the pin mux, with brightness 25 and 100 and with volume 1 and 10.
4. Auto-off: read ms[0xF6C] twice 10 s apart (tick period), with USB power connected and not; check whether anything other than keys resets it.
5. Click length: read 0x20002D67 and 0x40015434 at a fixed rate around a key press (or time the scan tick with the DWT cycle counter); read 0x20002B20 (SysTick ticks per unit) for the boot beep.
6. USB Sharing: none needed for descriptors (host side), SWD only to find which FatFs drive the MSC LUN maps to (breakpoint in the SCSI READ(10) path) if the host view is ambiguous.

## 6. Notes the checker should know
- `ms[0x16]` is the trigger channel, not a RUN flag: the PLAN/scope notes' "ms[0x2E] != 0 = Await Trigger" is consistent with ms[0x2E] being RUN.
- Stock's continuity/Auto beep ignores the Sound setting (constant 10); key clicks scale with it.
- Buzzer facts for `continuity_buzzer.c`: TMR11 CH1 (PSC 4799, ARR 24 at 240 MHz), pulse = 11 key-scan ticks (probably ~110 ms at a 10 ms scan, [H], see section 4.3).
- All four reference dumps were taken with ms[0xF68] = 2 (scope); no dump of the paused or settings states exists, so all zoom-paused statements about ms[0xE08..] while stopped come from code, not data.
