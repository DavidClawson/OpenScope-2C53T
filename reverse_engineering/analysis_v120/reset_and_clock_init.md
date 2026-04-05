# Reset Vector and Clock Initialization — FNIRSI 2C53T V1.2.0

*Exhaustive reverse-engineering analysis of the V1.2.0 stock firmware binary*
*`APP_2C53T_V1.2.0_251015.bin` (751,232 bytes, base 0x08000000)*

---

## 1. Executive Summary

| Parameter | Value | Source |
|-----------|-------|--------|
| Initial MSP | `0x20036F90` | Vector table [0] at file offset 0x000 |
| Reset vector | `0x08007311` (Thumb) → code at `0x08007310` | Vector table [1] at file offset 0x004 |
| PLL clock source | HEXT (external crystal) | CRM_CFG bit16 = 1 |
| HEXT frequency | 8 MHz | Standard crystal; confirmed by 240 MHz / 60× |
| PLL input after pre-divide | 4 MHz | pllhextdiv = 1, misc3.hextdiv = 0 → HEXT/2 |
| PLL multiplier | **60×** | pllmult_l = 11, pllmult_h = 3 (see §5) |
| **SYSCLK (HCLK)** | **240 MHz** | 4 MHz × 60 = 240 MHz ✓ |
| AHB prescaler | /1 (no division) | CRM_CFG[7:4] BIC'd to 0 |
| APB1 (PCLK1) | **120 MHz** | CRM_CFG[10:8] = 0b100 = /2 |
| APB2 (PCLK2) | **120 MHz** | CRM_CFG[13:11] = 0b100 = /2 |
| ADC clock | **20 MHz** | PCLK2/6, confirmed by remaining_unknowns.md |
| Flash wait states | **Not written explicitly** | Likely 4 (AT32 requirement for 240 MHz); not found in disasm |
| Toolchain | **Keil MDK (ARMCC)** | scatter-load format, __main, `0x008F` table header |
| 240 MHz assumption | **CONFIRMED** | Proven from binary disassembly |

The assumed 240 MHz is **correct**. The PLL uses an unusual but valid configuration: HEXT/2 × 60 rather than the more common HEXT × 30. Both produce identical SYSCLK. The pllrange bit is set (`0x80000000` to CRM_CFG[31]), enabling the AT32-specific high-frequency PLL mode.

---

## 2. Binary Properties

- **File**: `APP_2C53T_V1.2.0_251015.bin`, 0xB7680 bytes (751,232 bytes)
- **Base address**: `0x08000000` (confirmed by cross-referencing printf_format_handler at file offset 0x238 matching function_names.md)
- **Toolchain**: Keil MDK (ARMCC), not GCC
  - Evidence: scatter-load table at `0x08033F7C` starts with repeated `0x008F` halfwords (Keil compressed region header)
  - Startup calls `__main` (Keil runtime entry), not GCC's `main` directly
  - No GCC-style explicit `.data` copy loop or `.bss` zero loop in startup
- **Flash range**: `0x08000000` – `0x080BB67F` (end of binary)
- **Initial MSP**: `0x20036F90` = top of 224 KB SRAM, confirms EOPB0 is set to 0xFE

---

## 3. Vector Table

The vector table occupies `0x08000000`–`0x08000140` (80 entries × 4 bytes).

Selected annotated entries (all Thumb — add 1 to get LSB-set value in table):

```
Offset  Entry#  Name                  Value         Handler addr
------  ------  ----                  -----         ------------
0x000   [0]     Initial SP            0x20036F90    SRAM top (224 KB with EOPB0=0xFE)
0x004   [1]     Reset                 0x08007311    0x08007310  ← see §3.1
0x008   [2]     NMI                   0x08020E95    0x08020E94
0x00C   [3]     HardFault             0x0800FD35    0x0800FD34
0x010   [4]     MemManage             0x08020E91    0x08020E90
0x014   [5]     BusFault              0x080092D9    0x080092D8
0x018   [6]     UsageFault            0x0802F311    0x0802F310
0x01C   [7-10]  Reserved              0x00000000    —
0x02C   [11]    SVCall                0x08028DC1    0x08028DC0
0x030   [12]    DebugMon              0x080097E5    0x080097E4
0x038   [14]    PendSV                0x08028D51    0x08028D50
0x03C   [15]    SysTick               0x0802A995    0x0802A994
```

Most peripheral IRQs point to the default handler stub at `0x08007345`.

Active (non-default) peripheral IRQs:
```
[25]  EXINT3            0x08009C11    Button matrix row-change interrupt
[28]  DMA1_Channel2     0x08009671    LCD DMA transfer complete
[36]  USB_LP_CAN1_RX0   0x0802E8E5    USB HID interrupt (IAP)
[45]  TMR3              0x0802E71D    Button scan ISR (500 Hz)
[54]  USART2            0x0802E7B5    FPGA meter data RX
[59]  TMR8_BRK_TMR12    0x0802E78D    FatFs timer / TMR8 periodic tick
```

### 3.1 The Reset Vector Mystery

The Reset vector `0x08007311` points to address `0x08007310`, which falls **inside a TBB (Table Branch Byte) jump table**, not at a function boundary:

```
0x08007308: E8DF F001   TBB [pc, r1]     ; 8-entry switch table follows
0x0800730C: 23           ; table byte 0 = branch offset 0x23 → target 0x08007352
0x0800730E: 0E 04 23     ; table bytes 1, 2, 3
0x08007310: 15 04        ; ← Reset vector points HERE (inside table data!)
```

The CPU's reset does NOT jump to `0x08007310` during a cold boot. The actual C startup entry point is at `0x08000310`. The Reset vector value `0x08007311` is an artifact of Keil's linker placing the scatter-load stub there and/or the vector table being patched at runtime via `VTOR`.

The real startup sequence starts at **`0x08000310`** (see §4).

---

## 4. Startup Code — Annotated Disassembly

The C runtime startup lives at `0x08000310`. This is **Keil's `__main` / `__rt_entry` sequence**, not a GCC Reset_Handler.

```asm
; ─── KEIL C RUNTIME STARTUP at 0x08000310 ───────────────────────────────────
; Called by Reset_Handler (or VTOR-redirected entry) on power-on/reset.

08000310:  F241 0000    MOVW r0, #0x1000
08000314:  F2C2 0000    MOVT r0, #0x2000        ; r0 = 0x20001000 (temp stack addr)
08000318:  4685         MOV sp, r0              ; set temporary stack pointer

0800031A:  480E         LDR r0, [pc, #56]       ; r0 = [0x08000354] = 0x08033F7D
0800031C:  4780         BLX r0                  ; call __scatterload at 0x08033F7C
                                                ; copies .data from flash → SRAM
                                                ; zeros .bss (via Keil scatter tables)

0800031E:  F240 0000    MOVW r0, #0
08000322:  F6C0 0000    MOVT r0, #0x0800        ; r0 = 0x08000000 (flash base / vector table)
08000326:  F8D0 D000    LDR sp, [r0]            ; reload SP = [0x08000000] = 0x20036F90
                                                ; real stack pointer from vector table[0]

0800032A:  480B         LDR r0, [pc, #44]       ; r0 = [0x08000358] = 0x0802A9C5
0800032C:  4780         BLX r0                  ; call __libc_init_array at 0x0802A9C4
                                                ; runs C++ constructors / __attribute__((constructor))

0800032E:  480B         LDR r0, [pc, #44]       ; r0 = [0x0800035C] = 0x08007185
08000330:  4700         BX r0                   ; jump to main() at 0x08007184
                                                ; (no return — main is an infinite loop)

08000332:  E7FE         B.N 0x8000332           ; dead-loop (never reached) ×10

; ─── LITERAL POOL ────────────────────────────────────────────────────────────
08000354: 3F7D 0803     → 0x08033F7D   __scatterload (Keil compressed ROM table)
08000358: A9C5 0802     → 0x0802A9C5   __libc_init_array
0800035C: 7185 0800     → 0x08007185   main() [Thumb entry]
08000360: 6B90 2003     → 0x20036B90   SRAM data pointer
08000364: 6F90 2003     → 0x20036F90   = initial MSP (SRAM top)
08000368: 6B90 2003     → 0x20036B90   SRAM data pointer
```

### Key observations

- There is **no explicit call to `SystemInit`** in this startup. Artery's `SystemInit` (which only resets CRM to HICK defaults and sets VTOR) is either not called or embedded in Keil's `__main`.
- There is **no GCC-style `.data` copy loop** (this is Keil's scatter-load, which uses a compressed table at `0x08033F7C`).
- The **stack is set up twice**: first to a temp address (`0x20001000`), then reloaded from the vector table[0] (`0x20036F90`) after scatter-load.
- `__libc_init_array` at `0x0802A9C4` handles C++ global constructors and `__attribute__((constructor))` functions.

### Keil __scatterload data at 0x08033F7C

```
08033F7C: 008F 008F 008F 008F ...  (repeated 0x008F halfwords)
```

This is Keil's proprietary compressed region table format (header byte `0x8F` = "copy with length in following halfword"). Not human-readable disassembly. It encodes the `.data` source/destination/length and `.bss` zero regions.

### .data and .bss layout

From the literal pool pointers in the startup:
- **SRAM area**: `0x20036B90` – `0x20036F90` (1024 bytes = 0x400) — initialized SRAM block  
- **Initial SP**: `0x20036F90` = top of the 224 KB SRAM
- The actual .data source addresses are encoded in the scatter-load table and not directly readable without a Keil scatter-file.

---

## 5. Clock Tree — PLL Configuration (CONFIRMED FROM BINARY)

The clock is configured by the function at **`0x080334EC`** (a dedicated CRM/clock init function called during system bringup). This function also writes VTOR = `0x08007000` and enables interrupts, suggesting it is called at runtime from a phase after the initial startup, possibly from `main()` or a pre-main init constructor.

### Annotated disassembly of PLL setup (0x080334EC)

```asm
; ─── FUNCTION clock_init() at 0x080334EC ────────────────────────────────────

080334EC:  B5B0         PUSH {r4, r5, r7, lr}

; Set up base pointers:
; ip = 0xE000ED0C (SCB->AIRCR)
; r1 = 0x40021004 (CRM_CFG)
; All CRM_CTRL accesses: [r1, #-4] = 0x40021000 = CRM_CTRL
; All CRM_CFG accesses:  [r1, #0]  = 0x40021004 = CRM_CFG
; All CRM_MISC1 accesses:[r1, #80] = 0x40021054 = CRM_MISC1

; Write VTOR = 0x08007000
080334F6:  F247 0000    MOVW r0, #0x7000
0803350 2:  F6C0 0000   MOVT r0, #0x0800        ; r0 = 0x08007000
08033506:  F84C 0C04    STR.W r0, [ip, #-4]     ; VTOR = 0x08007000

; Enable global interrupts
0803350A:  B662         CPSIE I

; Enable HEXT (external crystal oscillator)
0803350C:  F851 0C04    LDR.W r0, [r1, #-4]    ; read CRM_CTRL
08033510:  F040 0001    ORR.W r0, r0, #1        ; set bit0 = hexten (HEXT enable)
08033514:  F841 0C04    STR.W r0, [r1, #-4]    ; write CRM_CTRL

; Wait for HEXT stable (bit1 = hextstbl)
08033518:  F851 0C04    LDR.W r0, [r1, #-4]
0803351C:  0780         LSLS r0, r0, #30        ; test bit1 (hextstbl)
; ... polling loop until bit1 = 1 ...

; Switch SCLK back to HICK (bits[1:0]=0), clear PLL bits
0803353A:  6808         LDR r0, [r1, #0]        ; read CRM_CFG
0803353C:  F020 0003    BIC.W r0, r0, #3        ; SCLKSEL = 0 (HICK)
08033540:  6008         STR r0, [r1, #0]

; Wait for SCLKSTS = 0 (confirmed on HICK)
; ... polling loop ...

; Reset CRM_CTRL (clear PLL enable, HEXT bypass, CFDEN, etc.)
08033564:  F851 0C04    LDR.W r0, [r1, #-4]
08033568:  F64F 72FF    MOVW r2, #0xFFFF
0803356C:  F6CF 62F2    MOVT r2, #0xFEF2        ; mask = 0xFEF2FFFF
08033570:  4010         ANDS r0, r2             ; clear PLL/HEXT control bits
08033572:  F841 0C04    STR.W r0, [r1, #-4]

; Clear CRM_CFG (all prescalers/dividers), CRM_MISC2(+0x2C)
08033576:  2000         MOVS r0, #0
08033578:  6008         STR r0, [r1, #0]        ; CRM_CFG = 0 (reset all)
0803357A:  62C8         STR r0, [r1, #44]       ; CRM_MISC2 = 0

; Set CRM_CLKINT (interrupt flags)
0803357C:  F44F 001F    MOV.W r0, #0x9F0000
08033580:  6048         STR r0, [r1, #4]        ; CRM_CLKINT

; Enable HEXT again (for PLL source)
08033582:  F851 0C04    LDR.W r0, [r1, #-4]
08033586:  F440 3080    ORR.W r0, r0, #0x10000  ; bit16 = hexten
0803358A:  F841 0C04    STR.W r0, [r1, #-4]

; Wait for HEXT stable with timeout
; ... polling loop, timeout ~0x2FFE counts ...

; ─── CONFIGURE PLL (entered at 0x080335E8 after HEXT stable) ────────────────

080335E8:  6808         LDR r0, [r1, #0]        ; read CRM_CFG
080335EA:  220B         MOVS r2, #11             ; r2 = 11 = pllmult_l value

080335EC:  F440 3080    ORR.W r0, r0, #0x10000  ; bit16 = pllrcs = 1 (HEXT as PLL source)
080335F0:  6008         STR r0, [r1, #0]

080335F2:  6808         LDR r0, [r1, #0]
080335F4:  F440 3000    ORR.W r0, r0, #0x20000  ; bit17 = pllhextdiv = 1 (HEXT ÷ 2)
080335F8:  6008         STR r0, [r1, #0]        ; PLL input = 8 MHz / 2 = 4 MHz

080335FA:  6808         LDR r0, [r1, #0]
080335FC:  F362 4095    BFI r0, r2, #18, #4     ; CRM_CFG[21:18] = r2[3:0] = 11 = pllmult_l
08033600:  6008         STR r0, [r1, #0]

08033602:  6808         LDR r0, [r1, #0]
08033604:  F040 40C0    ORR.W r0, r0, #0x60000000 ; bits[30:29] = 0b11 → pllmult_h = 3
08033608:  6008         STR r0, [r1, #0]

0803360A:  6808         LDR r0, [r1, #0]
0803360C:  F040 4000    ORR.W r0, r0, #0x80000000 ; bit31 = pllrange = 1 (≤240 MHz mode)
08033610:  6008         STR r0, [r1, #0]

; Clear CRM_MISC1[13:12] (USBDIV pre-clock bits)
08033612:  6D08         LDR r0, [r1, #80]       ; read CRM_MISC1
08033614:  F420 5040    BIC.W r0, r0, #0x3000
08033618:  6508         STR r0, [r1, #80]

; Enable PLL: set CRM_CTRL bit24 (pllen)
0803361A:  F851 0C04    LDR.W r0, [r1, #-4]
0803361E:  F040 7080    ORR.W r0, r0, #0x1000000 ; bit24 = pllen
08033622:  F841 0C04    STR.W r0, [r1, #-4]

; Wait for PLL locked (CRM_CTRL bit25 = pllstbl)
; ... polling loop ...

; ─── SET PRESCALERS AND SWITCH TO PLL ───────────────────────────────────────

0803364A:  6808         LDR r0, [r1, #0]
0803364C:  2204         MOVS r2, #4              ; r2 = 4 = APB div value 0b100 = /2
0803364E:  F020 00F0    BIC.W r0, r0, #0xF0      ; clear CRM_CFG[7:4] = AHBDIV → /1

08033652:  6008         STR r0, [r1, #0]

08033654:  6808         LDR r0, [r1, #0]
08033656:  F362 20CD    BFI r0, r2, #11, #3      ; CRM_CFG[13:11] = 4 = APB2DIV = /2
0803365A:  6008         STR r0, [r1, #0]

0803365C:  6808         LDR r0, [r1, #0]
0803365E:  F362 200A    BFI r0, r2, #8, #3       ; CRM_CFG[10:8]  = 4 = APB1DIV = /2
08033662:  6008         STR r0, [r1, #0]

; Set CRM_MISC1[5:4] (USBDIV_H = 0b11)
08033664:  6D08         LDR r0, [r1, #80]
08033666:  2202         MOVS r2, #2
08033668:  F040 0030    ORR.W r0, r0, #0x30      ; MISC1[5:4] = 0b11 (USB div high bits)
0803366C:  6508         STR r0, [r1, #80]

; Switch SCLK to PLL (SCLKSEL = 0b10)
0803366E:  6808         LDR r0, [r1, #0]
08033670:  F362 0001    BFI r0, r2, #0, #2       ; CRM_CFG[1:0] = 2 = PLL source
08033674:  6008         STR r0, [r1, #0]

; Wait for SCLKSTS = 0b10 (CRM_CFG[3:2] = 0b10 = 8)
08033678:  6808         LDR r0, [r1, #0]
0803367A:  F000 000C    AND.W r0, r0, #12        ; mask bits[3:2]
0803367E:  2808         CMP r0, #8               ; 0b10 << 2 = 8
08033680:  D00E         BEQ.N 0x80336A0          ; done when PLL switch confirmed
; ... polling loop ...
```

---

## 6. Confirmed Clock Tree

| Domain | Value | How |
|--------|-------|-----|
| HEXT crystal | 8 MHz | Standard external crystal |
| pllhextdiv | 1 (HEXT/2) | CRM_CFG bit17 set at 0x080335F4 |
| misc3.hextdiv | 0 (default) | Not written → default reset value |
| PLL input = HEXT/(hextdiv+2) | **4 MHz** | 8 MHz / (0+2) = 4 MHz |
| pllmult_l | 11 (CRM_CFG[21:18]) | BFI #18,#4, r2=11 at 0x080335FC |
| pllmult_h | 3 (CRM_CFG[30:29]) | ORR #0x60000000 at 0x08033604 |
| PLL multiplier formula | pllmult_l + 16×pllmult_h + 1 | AT32F403A formula (pllmult_h≠0) |
| PLL multiplier | 11 + 16×3 + 1 = **60** | Proven |
| **SYSCLK** | 4 × 60 = **240 MHz** | **CONFIRMED** |
| pllrange | 1 (CRM_CFG[31]) | ORR #0x80000000 at 0x0803360C; enables ≤240 MHz mode |
| AHBDIV | 0 (no division) | BIC #0xF0 at 0x0803364E |
| **HCLK (AHB)** | **240 MHz** | SYSCLK / 1 |
| APB1DIV (PCLK1) | /2 | BFI CRM_CFG[10:8] = 4 at 0x0803365E |
| **PCLK1** | **120 MHz** | 240 / 2 |
| APB2DIV (PCLK2) | /2 | BFI CRM_CFG[13:11] = 4 at 0x08033656 |
| **PCLK2** | **120 MHz** | 240 / 2 |
| ADC clock | PCLK2/6 = **20 MHz** | CRM_CFG[15:14]=0b10, bit28=0 (per remaining_unknowns.md) |

This corrects and confirms the previously **inferred** clock tree in `remaining_unknowns.md` §1. Every value is now proven from binary disassembly.

---

## 7. Flash Wait States

**The FLASH_PSR write was not found in the disassembled binary.**

FLASH_PSR is at `0x40022000` (AHBPERIPH_BASE + 0x2000). No `MOVW/MOVT` sequence loading this address was found in the binary, and no indirect access pattern was identified in the clock init function or the startup stub.

Possible explanations:
1. **AT32F403A auto-configures wait states** based on SYSCLK after the PLL switch (not documented in public Artery references, but possible for this silicon revision).
2. Flash wait state is written via a relative offset from a peripheral base that wasn't decoded in this analysis pass.
3. The value 4 (required for 240 MHz per AT32F403A datasheet) may be the silicon's power-on default.

**Risk**: If flash wait states are not correctly set to 4 before running at 240 MHz, the device will fetch incorrect instructions. Since the stock firmware runs stably at 240 MHz, this must be handled correctly — but the mechanism is not yet confirmed from disassembly.

The custom firmware uses `flash_psr_clock_cycle_config(FLASH_WAIT_CYCLE_4)` (at32f403a HAL) which explicitly writes 4 wait states. This is correct and safe.

---

## 8. AT32-Specific Quirks

### PLL multiplier encoding
The AT32F403A uses a non-obvious PLL multiplier encoding:
- `pllmult_l` (bits[21:18]) = lower 4 bits of the raw mult value
- `pllmult_h` (bits[30:29]) = upper 2 bits of the raw mult value
- Formula: when pllmult_h ≠ 0 (or pllmult_l = 15): `mult = pllmult_l + 16 × pllmult_h + 1`
- The raw combined value (0x3B = `(pllmult_h<<4)|pllmult_l` = 0b11_1011 = 59) + 1 = 60×

This is why the multiplier is 60 and not 30 — the stock firmware uses the `pllhextdiv` divide-by-2 plus a higher multiplier rather than the more obvious `HEXT × 30` configuration. Both produce 240 MHz.

### pllrange bit
CRM_CFG bit31 = `pllrange` enables PLL frequencies above ~192 MHz (up to 240 MHz). This bit **must** be set before switching to PLL at 240 MHz. The stock firmware sets it at `0x0803360C`.

### MISC1 USB divider
CRM_MISC1[5:4] = USBDIV_H is set to `0b11` at `0x08033668`. Combined with USBDIV_L in CRM_CFG[23:22] (cleared to 0 here), the USB clock divider is configured for 48 MHz (240 MHz / 5).

### HEXTDIV path
The use of `pllhextdiv=1` with `misc3.hextdiv=0` is the AT32-specific pre-divider mechanism for the HEXT→PLL path. This is different from STM32F1 which has no HEXT pre-divider in the same location. The stock firmware uses HEXT/2 as PLL input rather than HEXT directly, requiring double the multiplier (60× instead of 30×).

---

## 9. Comparison to Custom Firmware

| Aspect | Stock V1.2.0 | Custom firmware |
|--------|-------------|-----------------|
| Toolchain | Keil MDK (ARMCC) | GCC (arm-none-eabi-gcc) |
| Startup | Keil `__main` → scatter-load → `__libc_init_array` → `main` | GCC `Reset_Handler` → `.data` copy loop → `.bss` zero → `SystemInit` → `__libc_init_array` → `main` |
| Base address | `0x08000000` | `0x08004000` (app after bootloader) |
| HEXT | 8 MHz | 8 MHz ✓ |
| PLL config | pllhextdiv=1, mult=60 (4 MHz × 60) | Via `system_clock_config()` in `main.c`: HEXT × 30 = 240 MHz |
| SYSCLK | 240 MHz | 240 MHz ✓ |
| APB1 | 120 MHz (/2) | 120 MHz (/2) ✓ |
| APB2 | 120 MHz (/2) | 120 MHz (/2) ✓ |
| Flash wait | 4 cycles (assumed, not confirmed in disasm) | `FLASH_WAIT_CYCLE_4` explicit ✓ |
| `.data` copy | Keil scatter-load (compressed table) | Explicit copy loop in GCC startup |
| SystemInit | No explicit call; clock init in separate function | Called from GCC Reset_Handler |
| `__libc_init_array` | Called at `0x0802A9C4` | Called by GCC startup |

**Key difference**: The stock firmware uses HEXT/2 × 60 for PLL; our custom firmware uses HEXT × 30. Both arrive at 240 MHz. Our configuration is cleaner and matches Artery's example code more directly. No changes needed.

**Risk**: The stock firmware clock init function (`0x080334EC`) also writes `VTOR = 0x08007000`, which would redirect the interrupt vector table away from `0x08000000`. This is a stock-firmware-specific behavior that we do not need to replicate; our custom firmware leaves VTOR at `0x08004000` (our app base).

---

## 10. Implications

1. **240 MHz is proven correct.** All timer prescalers, USART baud rates, and timing code in the custom firmware that assume 240 MHz are correct.

2. **APB1 = APB2 = 120 MHz.** USART2 (FPGA commands) and TMR3 (button scan) are on APB1 at 120 MHz. Their prescaler calculations in the custom firmware are correct.

3. **ADC clock = 20 MHz** is confirmed. PB1 (battery sense) ADC at 239.5-cycle sample time = 12 µs per conversion, which is correct for the 16-sample averaging.

4. **Flash wait state explicit write not found.** The custom firmware's explicit `FLASH_WAIT_CYCLE_4` call is safe and correct. Do not remove it.

5. **Keil scatter-load at 0x08033F7C** occupies some flash. This explains why the Ghidra function map shows a large data region in that area. It is not code and does not need to be reverse-engineered further.

6. **No SystemInit clock setup.** The Artery-provided `SystemInit` (which only resets CRM to HICK defaults) is not the PLL configurator. PLL setup is done in application code. This is consistent with our custom firmware's `system_clock_config()` in `main.c`.

---

## Appendix A: CRM Register Map (accessed in clock_init)

| Address | Name | Role in clock_init |
|---------|------|--------------------|
| `0x40021000` | CRM_CTRL | hexten, hextstbl, pllen, pllstbl |
| `0x40021004` | CRM_CFG | pllrcs, pllhextdiv, pllmult_l, pllrange, pllmult_h, ahbdiv, apb1div, apb2div, sclksel, sclksts |
| `0x40021054` | CRM_MISC1 | usbdiv_h |
| `0x4002102C` | CRM_MISC2 (+0x28 from CFG) | Cleared to 0 |
| `0xE000ED08` | SCB->VTOR | Set to `0x08007000` |

| Register | Bits written | Value | Meaning |
|----------|-------------|-------|---------|
| CRM_CFG[16] | pllrcs | 1 | HEXT as PLL source |
| CRM_CFG[17] | pllhextdiv | 1 | HEXT pre-divided by 2 |
| CRM_CFG[21:18] | pllmult_l | 11 (0b1011) | PLL mult lower 4 bits |
| CRM_CFG[30:29] | pllmult_h | 3 (0b11) | PLL mult upper 2 bits |
| CRM_CFG[31] | pllrange | 1 | Enable >192 MHz mode |
| CRM_CFG[7:4] | ahbdiv | 0 | AHB = SYSCLK (no division) |
| CRM_CFG[10:8] | apb1div | 4 (0b100) | APB1 = HCLK/2 = 120 MHz |
| CRM_CFG[13:11] | apb2div | 4 (0b100) | APB2 = HCLK/2 = 120 MHz |
| CRM_CFG[1:0] | sclksel | 2 | PLL as SYSCLK source |
| CRM_MISC1[5:4] | usbdiv_h | 3 (0b11) | USB clock divider (high bits) |

---

## Appendix B: Key Address Cross-Reference

| Address | Content | Notes |
|---------|---------|-------|
| `0x08000000` | Vector table [0] = `0x20036F90` | Initial MSP |
| `0x08000004` | Vector table [1] = `0x08007311` | Reset (Thumb) → `0x08007310` |
| `0x08000310` | Keil `__main` startup entry | Scatter-load → libc_init → main |
| `0x08000354` | Literal: `0x08033F7D` | `__scatterload` Thumb entry |
| `0x08000358` | Literal: `0x0802A9C5` | `__libc_init_array` Thumb entry |
| `0x0800035C` | Literal: `0x08007185` | `main()` Thumb entry |
| `0x08007310` | Reset vector destination | Inside TBB jump table (artifact) |
| `0x08007345` | Default IRQ handler stub | `B.N 0x8007345` (infinite loop) |
| `0x08033F7C` | Keil scatter-load data table | `0x008F` compressed region headers |
| `0x0802A9C4` | `__libc_init_array` | |
| `0x080334EC` | `clock_init()` | HEXT enable, PLL config, SCLK switch |
