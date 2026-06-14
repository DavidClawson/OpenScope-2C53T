# wf5 resolve-squeeze — the hard pure-RE floor (2026-06-13)

16 agents on the 12 non-blocked unresolved-meaningful D3 fns. Each: deep static-equivalence
vs our source; promote to R2/V1 ONLY with cited register/byte matches; adversarial-verify every
promotion. Script: `scripts/wf_resolve_squeeze.js`.

## Result: ZERO new resolves. 18.6% (13/70) is a hard floor for pure RE.

Every remaining meaningful gain needs **build** (reconcile DIVERGENT / write ABSENT) or **bench**
(FPGA_BLOCKED). No byte-faithful equivalence was hiding in the unresolved set. The verifier even
demoted `0800b908` (stale R2/V1 -> R1/V0) and overturned 3 NA->ABSENT.

## Per-function disposition + concrete path-to-faithful (the actionable roadmap)

### `0x0801e1e4` scope_mode_trigger → DIVERGENT (R1/V0)
- **evidence:** Stock=256-pt FFT over SPI3 bufs 0x200006a8/0x20000aa8, offset-sub DAT_20000448/449, flash twiddle DAT_0806e0c0 + bitrev DAT_0806e4d0, mag=sqrt(re^2+im^2)->0x20000104+ch*4. Ours: 4096-pt, runtime twiddles, no flash-table match, fed by test_signal not FPGA. Not a register port.
- **to make faithful:** To reach R2/V1: match stock's 256-pt size, replicate flash twiddle table DAT_0806e0c0 and bitrev permutation DAT_0806e4d0, offset-subtract via DAT_20000448/449, write normalized 0x80-bin spectrum to display buffer; then feed from live FPGA SPI3 capture (blocked by config-entry wall) to verify equivalence.

### `0x08036084` spi_flash_fs_format → DIVERGENT (R2/V0)
- **evidence:** MISLABELED=screenshot BMP, not fs-format (confirmed). Stock: 54B header @offset0x36, compression=0 BI_RGB, filesize 0x25836, ppm 0x0b13, 3-band LCD readback (FUN_0800bcd4, PC7 0x40011000 gate), f_write FUN_0802e530. Ours diverges: 70B header (16B mask block), compression=3 BI_BITFIELDS (buf+114), filesize 153670, ppm=0, shadow-fb memcpy not LCD-band readback. Byte-faithful=false.
- **to make faithful:** To make V1 byte-faithful: use 54-byte header (no BITFIELDS mask block), set compression=0 (BI_RGB), filesize 0x25836=153654, ppm fields 0x0b13, replicate 3-band LCD readback with PC7-gated band selector and FatFs streaming. Currently a divergent BI_BITFIELDS one-shot; static equivalence not provable.

### `0x0802d1e8` spi_flash_write_sectors → NA_OURS (R0/VNA)
- **evidence:** Body is ChaN FatFs f_lseek (not a sector writer): signature char f(FIL*,FSIZE_t ofs); CLMT fast-seek build on ofs==0xffffffff (puVar9=param_1[10]); FAT-chain walk via FUN_0802ff18(get_fat); FS-type dispatch pcVar13[1]==2/3 (FAT32/exFAT); returns 9=FR_INVALID_OBJECT. No abs HW regs — pure FS-object library code we replace.
- **to make faithful:** N/A — third-party FatFs library function (misnamed in function_names.md as spi_flash_write_sectors; true identity = f_lseek). We legitimately substitute our own flash_fs wrapper; no register/byte-faithful reimpl required.
- **verifier overturned:** REFUTED on the OURS side; stock-side characterization confirmed accurate. Stock FUN_0802d1e8 is indeed ChaN FatFs f_lseek, NOT a sector writer: signature char f(FIL* param_1, FSIZE_t param_2); validate-FIL guard returns '\t'=9=FR_INVALID_OBJECT (lines 20738-43); CLMT fast-seek build branch on param_

### `0x080028e0` meter_data_process → DIVERGENT (R1/V0)
- **evidence:** Stock @080028e0 runs softfloat double pipeline (__aeabi_i2d 0803ed70/ui2d 0803e5da/pow 0803c8e0/dmul 0803e77c/dsub 0803eb94/dadd 0803e124/d2iz 0803df48): value=digits*pow(10,exp)-raw2*pow(10,dp); autorange loop vs DAT_08002c00/c08/c0c & 10.0 writing DAT_2000102f/2c; then DAT_20001025 FSM pushes 0x1b/0x1c/0x1e to _display_queue. Ours: single-float raw_bcd/10^(4-dec) + hardcoded 0.0304/0.001 factors, NO double subtract, NO threshold autorange-FSM, NO 0x1b/0x1c/0x1e display dispatch (meter_data.c:429-437 states we deliberately omit FW autorange).
- **to make faithful:** To reach R2/V1: reimplement the double-precision two-field pipeline value=digits*pow(10,exp)-raw2*pow(10,decpos); replicate the 4-iteration decade autorange that increments meter_decimal_pos against thresholds DAT_08002c00/c08/c0c/10.0 and sets range_index(1/2/3); and add the DAT_20001025 mode-FSM that emits display-queue events 0x1b/0x1c/0x1e per mode/probe_type. Currently we use a per-device-calibrated single-float shortcut instead.

### `0x0800b908` FUN_0800b908 → DIVERGENT (R1/V0)
- **evidence:** Cmd codes/order match (case1=00,09,07/0A,1A-1E; case9=00,12,13,14,09,probe; case8=00,2C; probe gate GPIOC 0x40011008 bit7 exact). BUT transport diverges: ours builds 10B frames via fpga_send_cmd, NOT stock's 1B codes to usart_cmd_queue 0x20002D6C via xQueueGenericSend(0x803acf0)+downstream [param,cmd] packer; keyed on submode not state[0x20000F68].
- **to make faithful:** To reach R2/V1: (1) drive mode init from state[0x20000F68] read (mode 0-9 TBH switch) not submode remap; (2) queue 1-byte codes to a usart_cmd_queue with a downstream dispatcher that packs [param,cmd] from live state (matching 0x0804BE74 table), instead of inline param building. (3) FPGA-side acceptance unverifiable (config-entry wall) — V1 blocked until acquisition chain lives.

### `0x08037800` fpga_spi3_transfer → FPGA_BLOCKED (R1/V0)
- **evidence:** Stock FUN_08037800 (full_decompile.c:28638) enters via code_r0x label with corrupt context (in_ZR, unaff_r7/r9, unaff_s16..s28). CS framing verified: writes _DAT_40010c10=0x40 / _DAT_40010c14=0x40 = GPIOB scr/clr PB6 — our SPI3_CS macros emit identical 0x40 to those addrs. But the float scale/cal body (modes 1-4, VectorSignedToFloat) is decode-corrupt + needs live ADC stream to verify.
- **to make faithful:** Re-decompile body with clean register context (Ghidra mis-split this as inner loop of spi3_acquisition_task 0x08037454; force function boundary or hand-decode the VFP block). Then bench-diff the per-sample calibration scaling (Y-transform scale_a/scale_b LUT, ADC offset -28, baseline subtraction) against a live capture once the FPGA config-entry wall is cleared. Equivalence is genuinely bench-gated, not statically verifiable.

### `0x0803bee0` dma1_configure → ABSENT (R0/V0)
- **evidence:** Stock fires DMA1-Ch1 mem->EXMC framebuffer blit: RCC AHBENR 0x40021014|=1 (DMA1 clk), DMA1 CNDTR=w*h@0x40020020, CMAR=fb@0x40020024, CPAR=0x60020000(LCD data)@0x40020028, CCR=0x4543@0x4002001c, NVIC ISER 0xe000e100=0x1000 (Ch1 IRQ), SCB AIRCR 0xe000ed0c sleep cfg. Our lcd.c writes pixels CPU-loop to 0x60020000, NO DMA1, no IRQ, no framebuffer alloc.
- **to make faithful:** Build a DMA1-Channel1 mem-to-EXMC blit: alloc RGB565 framebuffer, set LCD window/RAMWR, program DMA1 CNDTR=w*h, CMAR=fb, CPAR=0x60020000(LCD data, no incr), CCR=0x4543, AHBENR DMA1 clk, enable Ch1 IRQ (ISER bit12) for completion. Currently entirely absent; lcd.c is synchronous CPU writes.

### `0x08036848` spi_peripheral_init → DIVERGENT (R2/V0)
- **evidence:** Stock FUN_08036848 = StdPeriph SPI_Init clone; sole caller (full_decompile.c:21246) inits base 0x40003800=SPI2 (W25Q128 flash), cfg {dir0,master1,8bit,CPOL0,CPHA0,softNSS1,baud1}. Ours: flash_fs.c writes SPI2->ctrl1=(1<<2)master|(1<<8)SSI|(1<<9)SSM, CPOL/CPHA=0, ctrl2=0 — matches, BUT prescaler (3<<3)=/16 vs stock baud=1.
- **to make faithful:** Register bits (master, Mode0, soft-NSS, 8-bit, ctrl2=0) at SPI2 base already match stock byte-for-byte. Only divergence: clock prescaler — ours hardcodes /16 (conservative) vs stock param_2[6]=1 baud field. To make byte-faithful, decode stock baud=1 mapping and match CR1[5:3]; until then keep R2/V1 DIVERGENT, do not promote.
- **verifier overturned:** REFUTED the V1 claim; downgrade V1 -> V0. Verified both sides directly. Stock FUN_08036848(0x40003800,&cfg) is a StdPeriph-style SPI_Init clone on SPI2 base 0x40003800 (W25Q128 flash); the config struct local_2c..local_25 = {0,1,0,0,0,1,1,1}. Hand-decoding the bitfield writes against the AT32 CTRL1 

### `0x0802f2ac` spi2_page_write_loop → ABSENT (R0/V0)
- **evidence:** Stock=256B page-align loop (local_14=min(0x100-(addr&0xff),len)) -> FUN_0802f36c page-prog (WREN 0x06, op 0x02, 3 addr, RDSR 0x05) via byte prim FUN_0802f0c4 = I2C1 0x40003800/DR 0x4000380C, CS GPIOC PC12 BSRR 0x40010C14/BRR 0c10. Ours: no write code at all.
- **to make faithful:** Implement a flash page-program write loop in flash_fs.c: split at 256B page boundary, per page issue WREN(0x06)+PP(0x02)+3 addr bytes+data, then RDSR(0x05) busy-poll, over our real SPI2/W25Q128 path (CS PB12). Prior DIVERGENT/R1 was wrong: ours has no write primitive even as a register-level stub.

### `0x0802eee8` spi2_read_jedec_id → DIVERGENT (R1/V1)
- **evidence:** Stock @0802eee8: CS low GPIOB_BRR(0x40010c14)=0x1000(PB12), opcode 0x90 + 3 dummy addr (FUN_0802f0c4 0,0,0), read 2B (FUN_0802f0b8=0xFF), CS hi ODR(0x40010c10)=0x1000. Ours: same PB12 CS regs but opcode 0x9F, no addr, read 3B. Different SPI ID command/frame.
- **to make faithful:** To make register/byte-faithful: add a 0x90-variant (cmd 0x90 + 0x00,0x00,0x00 addr, read 2 bytes mfg+devID) matching stock frame, OR explicitly accept 0x9F-vs-0x90 divergence as a deliberate equivalent. Currently our 0x9F JEDEC read differs in opcode and byte count from stock 0x90 Read-Mfg/Device-ID.

### `0x08019e18` scope_set_ch_offset → STILL_PARTIAL (R1/V0)
- **evidence:** Mislabel confirmed (not ch_offset): stock body `_DAT_6001fffe=_DAT_2000834c; uRam60020000=(param_4+param_2)-1 &0xff` = LCD window end-coord, ports 0x6001fffe/0x60020000. Call site (v2.c:15826) FUN_08019e18(0,0,0x140,0xf0)=full 320x240 before a 76800-px FAT image blit. BUT Ghidra body is truncated (82B vs ~12B shown): full CASET/RASET seq not visible, can't certify byte-exact.
- **to make faithful:** Decode gap, not a code gap: re-disassemble all 82 bytes of FUN_08019e18 (Ghidra collapsed it) to expose the full CASET/RASET/RAMWR sequence and the x-window writes. Then compare byte-for-byte against lcd_set_window. Our register/formula match is already correct (y_end=y+h-1, ports 0x6001fffe/0x60020000); only the truncated stock body blocks promotion to R2/V1.

### `0x0802d1c4` spi_flash_status_check → NA_OURS (R0/V0)
- **evidence:** MISNAMED. Decompile (decompiled_2C53T_v2.c:20697): pure FatFs validate() helper. param_1=FIL/DIR obj; pcVar1=*param_1=parent FATFS; checks fs->fs_type(pcVar1[0])!=0, obj->id(param_1[1])==fs->id(pcVar1[6]), drive (byte)pcVar1[1]-1<3, else *param_1=0. ZERO hardware MMIO. Stock library code (ChaN FatFs), not a device protocol.
- **to make faithful:** None required — vanilla ChaN FatFs internal validate(). Not a register/protocol function; no constants to match. If FatFs is ever integrated, it ships this verbatim. NA_OURS skip is justified. Also note ledger name 'spi_flash_status_check' is wrong and should be renamed fatfs_validate_object.
- **verifier overturned:** REFUTED the RNA promotion — but not on the function-identity grounds. The stock-body identification is CORRECT: FUN_0802d1c4 (decompiled_2C53T_v2.c:20697) is the ChaN FatFs validate() helper, not spi_flash_status_check (a MEDIUM-confidence misnomer in function_names.md:165). The decompile is textboo
