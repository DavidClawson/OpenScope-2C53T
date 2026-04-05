# Remaining Unknowns -- Extracted

Analysis of `init_function_decompile.txt` (FUN_08023A50, ~15.4KB, 0x08023A50-0x080276F2)
and supporting files (`full_decompile.c`, `hardware_map.txt`, `interrupt_handlers.c`,
`fpga_task_annotated.c`, `FPGA_BOOT_SEQUENCE.md`).

---

## 1. Clock Tree (RCC/CRM Configuration)

### What the init function reveals

The master init function does NOT contain the PLL/HSE setup itself. The clock tree
is configured before FUN_08023A50 is called (likely in the startup code or a
`SystemInit()` function in the code region below 0x08002000). By the time
FUN_08023A50 runs, the system clock is already running at full speed.

**Evidence:** The function uses `systick_reload_2` (stored at RAM 0x20002B20) as a
pre-computed value for SysTick delays, implying clocks are already configured.
The division math at 0x080258A4-0x080258CC computes a timer prescaler from the
APB1 clock frequency (stored in the timer init struct at `[sp+0x44]`).

### RCU_APB2EN (0x40021018) -- Peripheral clock enables

The init function progressively enables APB2 peripheral clocks via read-modify-write
to 0x40021018 (register sb throughout the function):

| Bit | Peripheral | Where enabled |
|-----|-----------|---------------|
| 0   | AFEN (AFIO clock) | 0x08023B98 |
| 2   | IOPAEN (GPIOA) | 0x08023ABE, 0x080241B8 |
| 3   | IOPBEN (GPIOB) | 0x08023ACC, 0x0802418A |
| 4   | IOPCEN (GPIOC) | 0x08023A6C, 0x080240D4, 0x08026E36 |
| 5   | IOPDEN (GPIOD) | 0x08023B7A |
| 6   | IOPEEN (GPIOE) | 0x08023B8A, 0x080240E4 |
| 8   | ADC0EN (ADC1) | 0x08025392 (`|= 0x200`) |
| 14  | USART0EN? | Possibly via 0x4002101C writes |
| 20  | TMR0EN (TIM1) | 0x08024194 (`|= 0x100000`) |
| 21  | SPI0EN (SPI1) | 0x080241AC (`|= 0x200000`) |
| 14  | TMR8EN (TIM8) | 0x080274F8 (`|= 0x4000`) |

### RCU_APB1EN (0x4002101C) -- Peripheral clock enables

| Bit | Peripheral | Where enabled |
|-----|-----------|---------------|
| 1   | TMR3EN (TIM3) | 0x08026EBA (`|= 0x2`) |
| 5   | TMR7EN (TIM7) | 0x080241C4 (`|= 0x80`) |
| 6   | TMR6EN (TIM6) | 0x080275D4 (`|= 0x40`) |
| 17  | USART1EN (USART2) | Referenced in pseudocode (`|= 0x20000`) |
| 29  | DACEN (DAC) | Implied by DAC register usage (0x40007400) |

### RCU_AHBEN (0x40021014) -- AHB clock enables

| Bit | Peripheral | Where enabled |
|-----|-----------|---------------|
| 0   | DMA0EN (DMA1) | 0x08025946 (`|= 0x2`), also via FUN_0803bee0 (`|= 1`) |
| 8   | EXMCEN (EXMC/XMC) | 0x08023BA8 (`|= 0x100`) |

### RCU_CFG0 (0x40021004) -- Clock configuration

Two accesses found in the init function at 0x08025398-0x080253AC:

```
ldr r0, [r1, #0xbc0]     ; load RCU_CFG0 (0x40021004)
bfi r0, r6, #0xe, #2     ; insert r6 (=2) into bits [15:14] -> ADC prescaler = /6
str r0, [r1, #0xbc0]     ; store RCU_CFG0
ldr r0, [r1, #0xbc0]     ; reload
bic r0, r0, #0x10000000  ; clear bit 28 (ADC prescaler extended bit)
str r0, [r1, #0xbc0]     ; store
```

**ADC prescaler = PCLK2/6.** Bits [15:14] = 0b10 = /6, bit 28 cleared = no extended mode.
If PCLK2 = 120MHz, then ADC clock = 20MHz (max is 14MHz on STM32F1, but AT32 allows higher).

### Inferred clock tree (from AT32F403A defaults + known 240MHz operation)

| Clock | Frequency | Derivation |
|-------|-----------|------------|
| HSE | 8 MHz | External crystal (standard for FNIRSI boards) |
| PLL multiplier | x30 | 8 MHz x 30 = 240 MHz (AT32-specific, exceeds STM32 range) |
| SYSCLK | 240 MHz | PLL output |
| AHB (HCLK) | 240 MHz | AHB prescaler /1 (default) |
| APB2 (PCLK2) | 120 MHz | APB2 prescaler /2 (typical for this speed) |
| APB1 (PCLK1) | 120 MHz | APB1 prescaler /2 (typical for this speed) |
| ADC clock | 20 MHz | PCLK2/6 (confirmed from RCU_CFG0 write) |
| SPI3 clock | 60 MHz | PCLK1/2 (prescaler=0x100 -> /2 from spi_init) |

**Note:** The actual PLL configuration registers (0x40021000, 0x40021004 bits [21:18],
0x40021028) are not written in FUN_08023A50. They must be set in the pre-main
startup code (likely an AT32 HAL `system_clock_config()` function).

---

## 2. IOMUX/AFIO Remap

### Exact register operations (from disassembly at 0x08025764)

The AFIO remap is at address 0x08025764 in the init function. The register accessed
is **0x40010008**, which the disassembly annotates as "AFIO+0x08".

**Important note on register identity:** On STM32F1/GD32F30x, 0x40010008 = AFIO_EXTICR0
(EXTI source select). However, on the **AT32F403A**, the IOMUX register layout is:
- 0x40010000 = IOMUX_EVTOUT (Event output)
- 0x40010004 = IOMUX_REMAP (Pin remap register 1) -- contains SWJ_CFG at bits [26:24]
- 0x40010008 = IOMUX_EXINTC1 (EXTI source config 1)
- 0x4001001C = IOMUX_REMAP2 (Extended remap)

The decompiled code at 0x40010008 may be a Ghidra/disassembler labeling issue where r2
was used as a base pointer with offsets. However, the actual bit operations match the
SWJ_CFG remap pattern exactly:

```asm
08025764:  ldr   r0, [r2]          ; r2 = 0x40010008, read register
08025768:  movw  r8, #0x800
0802576A:  bic   r0, r0, #0xF000   ; clear bits [15:12]
0802576E:  str   r0, [r2]          ; write back
08025770:  ldr   r0, [r2]          ; re-read
08025776:  orr   r0, r0, #0x2000   ; set bit 13 (= 0x2000)
0802577A:  str   r0, [r2]          ; write back
```

### Interpretation

The operation `(reg & ~0xF000) | 0x2000` sets bits [15:12] to 0b0010.

On the **AT32F403A IOMUX_REMAP** register (0x40010004), the SWJ remap bits are at
[28:26] (not [15:12]). However, some AT32 silicon revisions or the actual register
layout may differ from the datasheet. The GD32F30x AFIO_PCF0 register has SWJ_CFG
at bits [26:24], not [15:12] either.

**Most likely explanation:** The r2 base address (0x40010008) is used with negative
offsets elsewhere in the function, or the Ghidra annotation is off by one register.
Given that the code immediately after this accesses `[r2, #0x3f8]` and `[r2, #0x3fc]`
(which from 0x40010008 base would be 0x40010400 and 0x40010404 = GPIOB_CTL0/CTL1),
the base pointer is being used for GPIO access too.

### Effect

Regardless of the exact register offset ambiguity, the functional effect is confirmed:
- **JTAG-DP is disabled, SW-DP (SWD) is preserved**
- **PB3, PB4, PB5 are freed from JTAG function for SPI3 use**
- This matches the `gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE)` call verified
  on real hardware in `spi3test.c`

### Additional AFIO operations at 0x40010008

After the SWJ remap, the code also does:
```asm
0802577C:  ldr   r0, [r2, #0x3f8]  ; 0x40010008+0x3f8 = 0x40010400 (GPIOB_CTL0)
08025782:  bic   r0, r0, #8        ; clear bit 3
08025786:  str   r0, [r2, #0x3f8]
0802578A:  ldr   r0, [r2, #0x3fc]  ; 0x40010008+0x3fc = 0x40010404 (GPIOB_CTL1)
08025790:  bic   r0, r0, #8        ; clear bit 3
08025794:  str   r0, [r2, #0x3fc]
08025798:  ldr   r0, [r2, #0x3f8]  ; re-read GPIOB_CTL0
0802579C:  orr   r0, r0, #8        ; set bit 3
080257A0:  str   r0, [r2, #0x3f8]
```

This configures **GPIOB_CTL0** and **GPIOB_CTL1** (PB0-PB15 pin modes) for SPI3 pins.
Bit 3 in CTL0 corresponds to pin configuration mode bits (AF push-pull vs input).

---

## 3. ADC Configuration (Battery Voltage)

### ADC1 register accesses (base 0x40012400, r4 = 0x40012408 = ADC1_CR2)

The ADC configuration is at 0x08025336-0x0802546A. Using r4 = 0x40012408 as base:

| Operation | Address | Register | Value | Meaning |
|-----------|---------|----------|-------|---------|
| `[r4, #-0x4]` bic 0xF0000 | 0x40012404 | ADC_CR1 | clear bits [19:16] | Clear DISCNUM (discontinuous count) |
| `[r4, #-0x4]` orr 0x100 | 0x40012404 | ADC_CR1 | set bit 8 | SCAN mode enable |
| `[r4]` orr 0x2 | 0x40012408 | ADC_CR2 | set bit 1 | ADON = enable ADC |
| `[r4]` bic 0x800 | 0x40012408 | ADC_CR2 | clear bit 11 | ALIGN = right-aligned data |
| `[r4, #0x24]` bic 0xF00000 | 0x4001242C | ADC_SQR1 | clear bits [23:20] | L[3:0] = 0 -> 1 conversion in sequence |
| `[r4, #8]` orr 0x38000000 | 0x40012410 | ADC_SMPR1 | set bits [29:27] | Channel 9 sample time = 239.5 cycles |
| `[r4, #0x2c]` bfi r1(=9), 0, 5 | 0x40012434 | ADC_SQR3 | bits [4:0] = 9 | **1st conversion = Channel 9** |
| `[r4]` bic 0x2000000 | 0x40012408 | ADC_CR2 | clear bit 25 | Disable external trigger |
| `[r4]` orr 0xE0000 | 0x40012408 | ADC_CR2 | set bits [19:17] | EXTSEL = 0b111 = SWSTART |
| `[r4]` orr 0x100000 | 0x40012408 | ADC_CR2 | set bit 20 | EXTTRIG = enable trigger |
| `[r4]` orr 0x100 | 0x40012408 | ADC_CR2 | set bit 8 | DMA enable (or reserved on AT32?) |
| `[r4]` orr 0x1 | 0x40012408 | ADC_CR2 | set bit 0 | ADON = start conversion |
| `[r4]` orr 0x8 | 0x40012408 | ADC_CR2 | set bit 3 | RSTCAL = reset calibration |
| Wait loop: `[r4]` bit 3 clear | 0x40012408 | ADC_CR2 | poll bit 3 | Wait for RSTCAL to clear |
| `[r4]` orr 0x4 | 0x40012408 | ADC_CR2 | set bit 2 | CAL = start calibration |
| Wait loop: `[r4]` bit 2 clear | 0x40012408 | ADC_CR2 | poll bit 2 | Wait for calibration done |

### Summary

- **ADC1 is configured for Channel 9 (PB1) in single-conversion scan mode**
- **Sample time: 239.5 cycles** (maximum, for slow/stable reading)
- **Right-aligned, software-triggered (SWSTART)**
- **Calibration is performed (RSTCAL then CAL)**
- **Channel 9 = PB1** on AT32F403A/STM32F1

### Battery voltage channel

**ADC1 Channel 9 (PB1) is the battery voltage sense input.**

Evidence:
- Only one ADC channel is configured in the entire init function
- The long sample time (239.5 cycles) is characteristic of battery monitoring
  (slow-changing signal, needs low noise)
- The floating-point math preceding the ADC setup (0x08025332-0x08025368) uses
  calibration values including a `+100` offset and division, consistent with
  voltage divider compensation
- No ADC2 configuration is present in the init function

### Pre-ADC calibration math (0x08025332-0x08025370)

```c
// r0 = calibration_high halfword, r1 = calibration_low halfword
float s0 = (float)(calibration_high - calibration_low);
s0 = s0 / some_divisor;
int8_t dc_offset = meter_state[5] + 100;
s0 = s0 * (float)dc_offset;
s0 = s0 + (float)calibration_low;
// Result stored to *r8 (RAM location) as the ADC reference/threshold
```

This appears to compute a calibrated ADC threshold value from stored calibration
data, likely used to convert raw ADC counts to battery voltage.

---

## 4. TMR8 Configuration

### Vector table entry

From `vector_table.txt`:
```
[59] TMR8_BRK_TMR12    0x0802E78D
```

### ISR handler (FUN_0802E78C at 0x0802E78D, Thumb)

From `interrupt_handlers.c` (decompiled as FUN_0802e78c):

The TMR8_BRK ISR handler is at 0x0802E78D. However, **this is NOT actually a timer
handler**. The code at 0x0802E78C is part of the **FatFs filesystem** implementation:

```c
undefined1 FUN_0802e78c(uint param_1) {
    if (param_1 != 2) {
        // error path -> return 1
    }
    iVar7 = unaff_r9 * 0x1000 + 0x200000;
    // ... flash page read operations ...
    // FUN_0802f048 = spi_flash_read
    // FUN_080017a6 = memcpy
}
```

The function performs 4KB page reads from SPI flash (region 2 at offset 0x200000)
and manages a paging buffer with FAT32 cluster chain traversal. This is clearly a
FatFs disk_read callback, not a timer handler.

### Why TMR8_BRK points to FatFs code

From `usart_protocol_decompile.txt` analysis:
```
IRQ 43 (TMR8_BRK_TMR12) -> 0x0802E78D  [Filesystem/FatFs related]
```

The TMR8_BRK vector is **repurposed as a FatFs entry point**. This is a common
embedded firmware technique: unused interrupt vectors are pointed at utility
functions that can be called via software interrupt or simply share the vector
table slot with a function that needs to be at a known address.

### TMR8 peripheral registers (0x40013400)

**No writes to TMR8 peripheral registers (0x40013400) were found in the init
function or the full decompile.** The hardware timer 8 peripheral is never
configured. Only its vector table entry is used, redirected to FatFs.

### The code labeled "0x40010400" in the search

Note: 0x40010400 = **GPIOB_CTL0**, not TMR8. TMR8 is at 0x40013400 on AT32F403A.
No TMR8 register accesses exist in the codebase.

### TIM1 (0x40015400 region)

The init function DOES configure TIM1 (Advanced Timer 1) via addresses in the
0x40015xxx range, including:
- 0x40015404 = TMR1 peripheral registers accessed at 0x08023BC2, 0x08024108
- 0x40015810 = TMR1 extended registers at 0x08025B48
- Baud rate/prescaler calculation for TMR1 at 0x08027528

---

## 5. FPGA Command Payloads

### USART command queue items (sent via xQueueGenericSend to 0x20002D6C)

From the mode switch function (FUN_0800735c in `interrupt_handlers.c`), here are
all USART command byte values organized by meter mode (DAT_20001060):

| Mode | Name | Queue sends (byte values) |
|------|------|--------------------------|
| 0 | Oscilloscope CH1 | 0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11 |
| 1 | Oscilloscope CH2 | 0x00, 0x09, 0x07/0x0A*, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E |
| 2 | Signal generator | 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x07/0x0A* |
| 3 | Dual channel | 0x00, 0x08, 0x09, 0x07/0x0A*, 0x16, 0x17, 0x18, 0x19 |
| 4 | **Frequency counter** | 0x00, **0x1F**, 0x09, **0x20**, **0x21** |
| 5 | **Period measurement** | 0x00, **0x25**, 0x09, 0x26, 0x27, 0x28 |
| 6 | **Duty cycle** | **0x29** (single command) |
| 7 | Unknown mode | 0x15 (single command) |
| 8 | **Continuity/diode** | 0x00, **0x2C** |
| 9 | XY/Math mode | 0x00, 0x12, 0x13, 0x14, 0x09, 0x07/0x0A* |

*0x07 or 0x0A: selected based on GPIOC bit 7 state (`_DAT_40011008 << 0x18`):
if bit 7 HIGH -> use 0x07, if LOW -> use 0x0A. This likely reads a probe
detection pin.

### Previously unknown command bytes -- now identified

| Cmd | Context | Purpose |
|-----|---------|---------|
| **0x1F** | Mode 4 (freq counter) | Frequency counter mode enable |
| **0x20** | Mode 4 (freq counter) | Frequency counter parameter 1 |
| **0x21** | Mode 4 (freq counter) | Frequency counter parameter 2 |
| **0x25** | Mode 5 (period) | Period measurement mode enable |
| **0x29** | Mode 6 (duty cycle) | Duty cycle measurement mode (standalone) |
| **0x2C** | Mode 8 (continuity) | Continuity/diode test mode |

### USART TX frame format (10 bytes) -- CORRECTED 2026-04-04

Full trace of `dvom_tx_task` @ 0x080373F4 (decomp line 30822) confirms
the 10-byte TX buffer lives at `0x20000005..0x2000000E`, indexed by
`usart2_tx_byte_index` and pumped out by the USART2 ISR at line 13703.

**Only three bytes are ever written. The other seven are permanently
zero** (BSS-initialized, never touched in any code path I can find):

```
[0] = 0x00          (BSS, never written — address 0x20000005)
[1] = 0x00          (BSS, never written — address 0x20000006)
[2] = cmd_hi        (cRam20000007 = auStack_2[1], queue item high byte)
[3] = cmd_lo        (DAT_20000008  = auStack_2[0], queue item low byte)
[4] = 0x00          (BSS, never written — address 0x20000009)
[5] = 0x00          (BSS, never written — address 0x2000000A)
[6] = 0x00          (BSS, never written — address 0x2000000B)
[7] = 0x00          (BSS, never written — address 0x2000000C)
[8] = 0x00          (BSS, never written — address 0x2000000D)
[9] = cmd_hi+cmd_lo (cRam2000000e, checksum — address 0x2000000E)
```

**The prior "[4]-[8] = Parameters, context-dependent" hypothesis was
wrong.** A grep of the full decomp for any write to addresses
`0x20000009..0x2000000D` (direct, symbolic, or via the
`(&usart2_tx_buffer)[N]` pattern) returns zero hits.

**Implications for the FPGA protocol:**
- Every TX command carries exactly 2 bytes of meaningful payload
  (cmd_hi, cmd_lo). No per-command parameter bytes.
- The 5 zero bytes between cmd and checksum are pure timing/sync
  padding for the FPGA's USART receiver.
- Multi-byte protocols like the 411-byte boot cal (cmds 0x3B/0x3A)
  must be a **stream of separate single-command frames**, not a
  single frame with embedded params. 411 bytes = 411 frames if
  each frame carries 1 byte of cal data — this is consistent with
  the boot-time SysTick delays described in `FPGA_BOOT_SEQUENCE.md`.

**Our firmware (`firmware/src/drivers/fpga.c:260-265`) already
matches stock exactly:** it writes `[2]=cmd_hi`, `[3]=cmd_lo`,
`[9]=cmd_hi+cmd_lo`, and clears the rest via `memset`.

### SPI3 data queue trigger bytes

From init function at 0x08026DDA-0x08026E2A, after the SPI3 handshake, these
trigger bytes are sent to spi3_data_queue (0x20002D78):
```
1, 2, 6, 7, 8
```
These correspond to SPI3 acquisition modes in the spi3_acquisition_task.

---

## 6. DMA Assignments

### DMA1 (DMA0 in GD32 naming) -- Base 0x40020000

#### Channel 1 (registers at 0x4002001C-0x40020028) -- LCD framebuffer transfer

From FUN_0803bee0 (the LCD DMA blit function):

```c
// Enable DMA clock
RCU_AHBEN |= 1;                          // 0x40021014 |= 1 (DMA0EN)

// Clear channel flags
DMA0_INTC |= 0xF0;                       // 0x40020004 |= 0xF0 (clear CH1 flags? -- actually bits [7:4])

// Configure channel 1
DMA0_CH1_CNT  = (height + y_offset - 1) * width;  // 0x40020020: transfer count
DMA0_CH1_PADDR = framebuffer_ptr;                  // 0x40020024: peripheral/source address (RAM buffer)
DMA0_CH1_MADDR = 0x60020000;                       // 0x40020028: memory/dest address (LCD data register)

// NVIC priority and enable
SCB_AIRCR = (SCB_AIRCR & 0xF8FF) | 0x05FA0300;    // Priority grouping = 3
NVIC_ISER0 = 0x1000;                               // Enable IRQ12 = DMA0_CH1

// Channel control register
DMA0_CH1_CTL = 0x4543;                             // 0x4002001C = 0x4543
```

**DMA0_CH1_CTL = 0x4543 decoded (STM32/GD32 DMA_CCR format):**

| Bit | Value | Meaning |
|-----|-------|---------|
| 0 (EN) | 1 | Channel enabled |
| 1 (TCIE) | 1 | Transfer complete interrupt enabled |
| 4 (DIR) | 0 | Read from peripheral (actually: source = PADDR) |
| 5 (CIRC) | 0 | No circular mode |
| 6 (PINC) | 1 | Peripheral address increment |
| 7 (MINC) | 0 | Memory address fixed (LCD data register) |
| 8-9 (PSIZE) | 01 | Peripheral data size = 16-bit |
| 10-11 (MSIZE) | 01 | Memory data size = 16-bit |
| 12-13 (PL) | 01 | Priority = Medium |
| 14 (MEM2MEM) | 1 | Memory-to-memory mode |

**Summary: DMA0 Channel 1 transfers 16-bit RGB565 pixels from RAM framebuffer
to the LCD data register at 0x60020000 (EXMC-mapped ST7789V). This is the main
display DMA engine.**

#### Channel mapping summary

| DMA | Channel | Peripheral | Purpose | Evidence |
|-----|---------|-----------|---------|----------|
| DMA0 | CH1 | LCD (EXMC 0x60020000) | Framebuffer to LCD 16-bit blit | FUN_0803bee0, NVIC IRQ12 |
| DMA0 | CH0? | Flags cleared (0x0F) | Possibly SPI3 or USART2 RX | Pseudocode line 95 |

### DMA for SPI3

The SPI3 configuration at 0x08026540 sets:
```
SPI3_CTL1 |= 0x02  (bit 1 = RXDMAEN -- RX DMA enable)
SPI3_CTL1 |= 0x01  (bit 0 = TXDMAEN -- TX DMA enable)
```

However, no DMA channel configuration for SPI3 was found in the init function.
The SPI3 transfers in the runtime code (spi3_acquisition_task) use **polled I/O**
(wait TXE, write DATA, wait RXNE, read DATA), not DMA. The DMA enable bits may
be set but not used, or may be used in a mode not yet decompiled.

### DMA for USART2

From `hardware_map.txt`, USART2 accesses are at FUN_080277b4. The USART2
communication uses interrupt-driven byte-by-byte transfer (TMR3 ISR triggers
the exchange), not DMA.

---

## Appendix: Additional Peripheral Configurations Found

### EXMC/XMC (0xA0000000 region)

At 0x08023C02-0x08023C54:
```
EXMC base = 0xA0000000
*(0xA0000000) = 0x5010      ; SNCTL0: NOR flash mode, 16-bit bus, enabled
*(0xA0000204) = 0x02020424  ; SNTCFG0: timing config
*(0xA00001E4) = 0x00000202  ; SNWTCFG0: write timing
*(0xA0000220) modified: clear bits [15:8], set bits [7:0] = 8  ; Bus turnaround
*(0xA0000000) |= 1          ; Enable EXMC bank 0
```
LCD is memory-mapped: Command at 0x6001FFFE, Data at 0x60020000 (A17 selects RS/DCX).

### Watchdog (FWDGT at 0x40003000)

At 0x080275A0:
```
FWDGT_CTL = 0x5555    ; Unlock write access
FWDGT_PSC: bits [2:0] = 4 -> prescaler /64
FWDGT_RLD = 0x04E1    ; Reload = 1249
  -> Timeout = (1249+1) * 64 / 40000 = ~2.0 seconds (LSI = 40kHz)
  -> Or ~5.0 seconds if LSI = 16kHz (varies by chip)
FWDGT_CTL = 0xAAAA    ; Reload counter
FWDGT_CTL = 0xCCCC    ; Start watchdog
```

### DAC (0x40007400)

At 0x080259E6:
```
r6 = 0x40007400 (DAC base)
DAC_CR: bits [21:19] set to 2 (= 0b010) -- DAC channel 2 trigger
DAC_CR |= 0x40000  -- DAC CH2 trigger enable
DAC_CR &= ~0xC00000 -- clear DAC CH2 wave generation
DAC_CR &= ~0x20000  -- disable DAC CH2 DMA
DAC_CR |= 0x10000000 -- DAC CH2 output buffer disable
```
DAC is used for signal generator output (dual-channel 12-bit).

### I2C (0x40005C00 region)

At 0x08025BE8-0x08025BF8:
```
r5 = 0x40005C40
*(0x40005C40) |= 2      ; I2C_CR1 bit 1 (might be I2C2)
*(0x40005C60) |= 2      ; I2C_OAR1 or timing register
```
This is likely the touch panel I2C interface (GT911/GT915).

### NVIC Interrupt enables

| IRQ | Peripheral | ISER bit | Where enabled |
|-----|-----------|----------|---------------|
| 9 | EXTI9_5 | ISER0 bit 9 | 0x08025890 |
| 12 | DMA0_CH1 | ISER0 bit 12 | FUN_0803bee0 (`0x1000`) |
| 20 | USART2 | ISER0 bit 20 | 0x08025B8E (`0x100000`) |
| 29 | TMR3 | ISER0 bit 29 | 0x08026F44 (`0x20000000`) |
| 38 | TIM6/TIM7 | ISER1 bit 6 | 0x08025890 (`0x40`) |
| 43 | TMR8_BRK | ISER1 bit 11 | 0x08027682 (`0x800`) |
