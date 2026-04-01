# Peripheral Map

*Updated: 2026-03-31. MCU confirmed as Artery AT32F403A (register-compatible with GD32/STM32F1).*

## Memory-Mapped Peripherals Used by Firmware

| Address | Peripheral | Usage in Firmware |
|---|---|---|
| 0x40000400 | TIM3 | Periodic interrupt — likely button scan interval |
| 0x40003800 | SPI2 | **SPI flash** — Winbond W25Q128JV (PB13=CLK, PB14=MISO, PB15=MOSI, PB12=CS) |
| 0x40003C00 | **SPI3** | **FPGA bulk data channel** — ADC sample readout (**PB3=SCK, PB4=MISO, PB5=MOSI**, CS via GPIOB likely PB6) |
| 0x40004400 | USART2 | **FPGA command/control** — 10-byte TX frames, 2/10/12-byte RX frames (PA2=TX, PA3=RX) |
| 0x40005C00 | USB FS Device | USB mass storage mode ("USB Sharing") |
| 0x40006000 | CAN0 SRAM / USB SRAM | USB/CAN shared memory region |
| 0x40007400 | DAC | **Signal generator + calibration output** — dual 12-bit channels |
| 0x40010800 | GPIOA | Analog MUX control (PA15), button matrix (PA7, PA8) |
| 0x40010C00 | GPIOB | SPI CS (PB6=FPGA, PB12=flash), backlight (PB8), analog MUX (PB10, PB11), button (PB7) |
| 0x40011000 | GPIOC | Power hold (PC9), probe continuity (PC0), button matrix (PC5, PC10), analog front-end (PC12) |
| 0x40011400 | GPIOD | Signal routing (PD13), LCD EXMC data bus |
| 0x40011800 | GPIOE | Analog front-end (PE4/5/6), button matrix columns (PE2/3), LCD EXMC data bus |
| 0x40021000 | RCC/CRM | Clock configuration — FUN_0802a514 sets enable bits via (offset<<16 | bit) encoding |
| 0x60000000+ | EXMC Data Region | LCD ST7789V — 0x6001FFFE (command), 0x60020000 (data), A17 selects RS/DCX |

## Active Interrupt Handlers (V1.2.0)

| Vector | IRQ | Handler Address | Purpose |
|---|---|---|---|
| 15 | SysTick | 0x0802A995 | FreeRTOS system tick (1000Hz) |
| 25 | EXTI3 | 0x08009C11 | **Continuity buzzer detection** (added in V1.2.0) |
| 28 | DMA0_Ch1 | 0x08009671 | DMA transfer complete — LCD framebuffer blast (RAM 0x2000835C → EXMC 0x60020000) |
| 36 | USB_LP_CAN_RX0 | 0x0802E8E5 | USB device interrupt |
| 45 | TIM3 | 0x0802E71D | Periodic timer — possible button scan or measurement timing |
| 54 | USART2 | 0x0802E7B5 | **FPGA command interface** — TX byte pump + RX frame parser with echo/integrity validation |
| 59 | TIM8_BRK | 0x0802E78D | Timer 8 break — purpose unknown |

## Analog Front-End — GPIO MUX Routing

The firmware uses two GPIO multiplexing functions to select among 10 analog input configurations:

### FUN_080018a4 — Port C/E MUX (10 modes)

Controls PC12, PE4, PE5, PE6 for analog front-end relay/MUX switching. Each of the 10 cases sets specific GPIO output patterns on Port C (0x40011010/0x40011014) and Port E (0x40011810/0x40011814).

After switching, applies **floating-point ADC calibration**:
- Reads gain/offset from RAM lookup tables (selected by measurement mode)
- Formula: `result = ((gain - offset) / cal_constant) * (DAT_200000fc + 100) + offset`
- Writes 12-bit result to **DAC channel 2** (0x40007408)

### FUN_08001a58 — Port A/B MUX (10 modes)

Controls PA15, PB10, PB11 for analog range/MUX selection. Same 10-case structure as Port C/E MUX.

### ADC Calibration Tables (RAM)

Six table pairs (gain + offset), selected by mode:

| Condition | Gain Table | Offset Table |
|-----------|-----------|--------------|
| Mode ≥ 5 | 0x20000394 | 0x20000358 |
| Mode 4 or sub-mode 3 | 0x200003a8 | 0x2000036c |
| All other modes | 0x200003bc | 0x20000380 |

Port A/B uses a parallel set: 0x2000040c/0x200003d0, 0x20000420/0x200003e4, 0x20000434/0x200003f8.

These tables are likely loaded from SPI flash at boot (calibration data persisted across power cycles).

## SPI2 — SPI Flash Interface

| Register | Address | Purpose |
|----------|---------|---------|
| SPI2_STAT | 0x40003808 | Status: bit 1 = TX ready, bit 0 = RX ready |
| SPI2_DATA | 0x4000380C | Data TX/RX register |
| GPIOB BOP | 0x40010C10 | Chip select PB12 (flash) |

**spi2_transceive_byte** (FUN_0802f0c4): Polls TX ready, writes byte, polls RX ready, returns received byte.

**spi2_block_read** (FUN_0802f048): Standard SPI NOR flash read — command 0x03 + 3-byte address + bulk read. PB12 chip select.

This is SPI flash only (Winbond W25Q128JV). **NOT the FPGA data channel.**

## SPI3 — FPGA Bulk Data Interface (DISCOVERED 2026-03-31)

| Register | Address | Purpose |
|----------|---------|---------|
| SPI3_CTL0 | 0x40003C00 | Control register (clock, mode, enable) |
| SPI3_STAT | 0x40003C08 | Status — polled in FPGA task (FUN_08036934) |
| SPI3_DATA | 0x40003C0C | Data TX/RX register |
| GPIOB BOP | 0x40010C10 | Chip select (likely PB6) — 3 references in FPGA task |

### Pin Mapping

| Signal | Pin | Notes |
|--------|-----|-------|
| SPI3_SCK | **PB3** | JTAG pin (JTDO) by default — requires IOMUX remap to use as SPI3 |
| SPI3_MISO | **PB4** | JTAG pin (JNTRST) by default — requires IOMUX remap |
| SPI3_MOSI | **PB5** | Normal GPIO |
| SPI3_CS | **PB6 (GPIO)** | Software-controlled via GPIOB_BOP |

### Discovery Method

Found by tracing FreeRTOS `xQueueReceive` callers to FUN_08036934, then disassembling MOVW/MOVT instruction pairs to reconstruct the peripheral address `0x40003C08` (SPI3_STAT). This was missed in earlier analysis because:
1. Ghidra decompiler showed the address as SPI2 (0x40003800) due to Ghidra's GD32 processor definition not distinguishing SPI2/SPI3
2. Hardware probing only tested SPI2 (0x40003800)
3. PB3/PB4 are JTAG pins by default, not suspected as SPI

## DAC — Signal Generator + Calibration

| Register | Address | Purpose |
|----------|---------|---------|
| DAC_CTL | 0x40007404 | Control (bit 0 = enable) |
| DAC_CH2_DATA | 0x40007408 | Channel 2 12-bit output (lower 12 bits) |

DAC is used for:
1. **Calibration offset output** — GPIO MUX functions write calibrated values to DAC CH2 after analog input switching
2. **Signal generator output** — FUN_08001c60 configures waveform generation

## Signal Generator Architecture

**FUN_08001c60** (siggen_configure, 1634 bytes):
- Manages **6 output channels** (DAT_2000010d high nibble = channel count)
- 300-byte waveform descriptor per channel at RAM base 0x2000044E (stride 0x12D)
- Computes min/max across 16 waveform samples per channel
- 6 operating modes (switch on DAT_20000127 nibble-packed mode bits):
  - Case 1: Frequency stepping (up to 9 steps via DAT_200000FA[channel])
  - Case 2: Power mode with FPGA register control
  - Case 3: Frequency sweep with target calculation
  - Case 4-5: Sweep confirmation and modulation
  - Case 0xF: Finalization with measurement output
- Calls GPIO MUX functions for analog routing
- Writes to DAC (0x40007408) and GPIO control registers (0x40011410)

## Multimeter DVOM Architecture

### Mode Dispatch

Function pointer table at **0x0804C0CC** — indexed by meter sub-mode to select the handler function.

### FPGA Command Sequences by Meter Mode

| Function | Commands Sent | Likely Mode |
|----------|--------------|-------------|
| FUN_0800ba06 | 0x07/0x0A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E | Oscilloscope-style meter (AC voltage?) |
| FUN_0800bb10 | 0x07/0x0A, 0x16, 0x17, 0x18, 0x19 | DC voltage/current meter |
| FUN_0800bba6 | 0x20, 0x21 | Component tester |
| FUN_0800bc00 | 0x26, 0x27, 0x28 | Unknown meter sub-mode |
| FUN_0800bc98 | 0x07/0x0A | Single-command mode (probe detect?) |

All use `fpga_send_command` (FUN_0803acf0). Command 0x07 vs 0x0A is selected based on a sign bit in *param_1.

### Display Pipeline

Meter readings rendered via 6-position vertical layout:
- Y positions: 0x18, 0x3C, 0x60, 0x84, 0xA8, 0xCC
- Data fetched from table at **0x0804C5E8** indexed by `DAT_20001058` (mode) × 0x34 stride
- Scaling/formatting in FUN_0800ec70 (1,798 bytes of float math)
- Display callback at 0x20001114

## Button Input

### Confirmed MCU GPIO Buttons (6 of 15)

| Button | Row Pin | Column Pin | Mechanism |
|--------|---------|------------|-----------|
| PRM | PB7 | — | Direct GPIO (active-low) |
| CH2 | PA7 | PE3 | Matrix scan |
| Down | PC5 | PE3 | Matrix scan |
| Right | PA8 | PE2 | Matrix scan |
| Auto | PC10 | PE2 | Matrix scan |
| Save | PB0 | PE3 | Matrix scan |

### Missing Buttons (9 of 15)

CH1, MOVE, SELECT, TRIGGER, MENU, Up, Left, Play/Pause, POWER — **NOT on MCU GPIO**. Firmware has ZERO reads from GPIOA/B/D/E IDR registers. Must come through FPGA USART2 responses, I2C touch controller, or other peripheral.

## Unused But Available Peripherals

| Peripheral | Potential Use |
|---|---|
| **CAN0 (0x40006400)** | Native CAN bus — protocol decode without external hardware |
| **CAN1 (0x40006800)** | Second CAN channel |
| **ADC0 (0x40013000)** | Internal ADC (separate from FPGA's external ADC) |
| **SPI1 (0x40013000)** | High-speed SPI (APB2, no interrupt handler) |
| **TIM1, TIM2, TIM4-7** | Additional timers |
| **USART1, USART3, UART4, UART5** | Additional serial ports |
| **I2C1 (0x40005400)** | I2C — possibly used for GT911/GT915 touch controller (needs investigation) |
| **I2C2 (0x40005800)** | Second I2C bus |
| **DMA2** | Additional DMA channels |
