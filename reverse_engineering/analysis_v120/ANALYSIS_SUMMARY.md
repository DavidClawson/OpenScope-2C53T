# FNIRSI 2C53T V1.2.0 Firmware Analysis Summary

Generated: 2026-03-31

## Decompilation Coverage

- **362 functions** decompiled (292 originally + 61 new from code gap analysis + 9 forced)
- **37,909 lines** of decompiled C
- Full hardware register access map, cross-reference map, RAM variable map

## Critical Finding: Button Input Mechanism

### GPIO IDR (Input) Reads in Entire Firmware

| Port | Address | Reads Found | Purpose |
|------|---------|-------------|---------|
| GPIOA IDR | 0x40010808 | **NONE** | - |
| GPIOB IDR | 0x40010C08 | **NONE** | - |
| GPIOC IDR | 0x40011008 | 7 reads | Probe continuity (PC0 bit check) |
| GPIOD IDR | 0x40011408 | **NONE** | - |
| GPIOE IDR | 0x40011808 | **NONE** | - |

**The firmware NEVER reads GPIO input for buttons on ports A, B, D, or E.**

### GPIO Output Writes

| Function | Port Pins | Purpose |
|----------|-----------|---------|
| FUN_08001a58 | PA15, PB10, PB11 | Analog MUX/range select (10 modes) |
| FUN_080018a4 | PC12, PE4, PE5, PE6, PE12 | Analog front-end control |
| FUN_08019e98 | PC various, PD various | Main UI/measurement function |
| FUN_08001c60 | PD13 | Signal routing |
| FUN_0802ef48 | PB12 (SPI CS) | SPI flash chip select |
| FUN_08037800 | PB6 | FPGA SPI chip select |
| SPI functions | PB12 | SPI flash operations |

### gpio_read_pin Function Calls

Only 3 total calls:
1. `FUN_080304e0(0x40011000, 0x80)` - Read PC7 (probe detection)
2. `FUN_0803683c(0x40003800, 2)` - Read SPI2 TXE flag
3. `FUN_0803683c(0x40003800, 1)` - Read SPI2 RXNE flag

### Implication

**The remaining 9 buttons (CH1, MOVE, SELECT, TRIGGER, MENU, Up, Left, Play/Pause, POWER) are NOT on MCU GPIO.** They must be read through:
1. **FPGA via USART2** (interrupt handler at 0x0802E7B5)
2. **I2C touch controller** (GT911/GT915 mentioned in hardware docs)
3. **Some other peripheral** (DMA, external interrupt)

## Buttons Confirmed on MCU GPIO (Matrix Scan)

| Button | Row Pin | Column Pin | Mechanism |
|--------|---------|------------|-----------|
| PRM | PB7 | - | Direct GPIO (active-low) |
| CH2 | PA7 | PE3 | Matrix |
| Down | PC5 | PE3 | Matrix |
| Right | PA8 | PE2 | Matrix |
| Auto | PC10 | PE2 | Matrix |
| Save | PB0 | PE3 | Matrix |

## Interrupt Vector Table (Active Handlers)

| IRQ | Address | Handler | Purpose |
|-----|---------|---------|---------|
| EXINT3 | 0x08009C11 | Continuity buzzer | EXTI line 3 |
| DMA1_Ch2 | 0x08009671 | DMA transfer complete | Likely USART2 RX |
| TMR3 | 0x0802E71D | Timer 3 overflow | Periodic task (button poll?) |
| USART2 | 0x0802E7B5 | USART2 RX | FPGA communication |
| USB_LP | 0x0802E8E5 | USB low-priority | USB device |
| TMR8_BRK | 0x0802E78D | Timer 8 break | Unknown |

## Key Peripheral Usage

- **DAC (0x40007400)**: Heavily used by measurement functions (signal generator output)
- **SPI2 (0x40003800)**: FPGA data channel (PB13=CLK, PB14=MISO, PB15=MOSI, PB6/PB12=CS)
- **USART2 (0x40004400)**: FPGA command/status channel
- **DMA1**: Data transfer (likely USART2 RX or ADC)
- **TMR3**: Periodic timing (possibly button scanning interval)

## Next Steps for Button Discovery

1. **Decompile USART2 interrupt handler** (0x0802E7B5) — may contain button data parsing from FPGA
2. **Decompile TMR3 handler** (0x0802E71D) — may trigger periodic FPGA queries for button state
3. **Decompile DMA1_Ch2 handler** (0x08009671) — DMA completion for FPGA RX data
4. **Investigate I2C touch controller** — GT911/GT915 uses I2C, could handle touch-based "buttons"
5. **Implement FPGA initialization sequence** — the FPGA may not report buttons until properly initialized
6. **Search binary for I2C address writes** — find touch controller I2C address (typically 0x5D or 0x14)

## Files Generated

- `full_decompile.c` — Complete 362-function decompilation
- `hardware_map.txt` — All peripheral register accesses
- `xref_map.txt` — Function cross-reference graph
- `ram_map.txt` — SRAM variable usage map
