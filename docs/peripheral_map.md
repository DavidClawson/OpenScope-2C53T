# Peripheral Map

## GD32F307 Memory-Mapped Peripherals Used by Firmware

| Address | Peripheral | Usage in Firmware |
|---|---|---|
| 0x40000400 | TIM3 | Timer interrupt (vector 45). Timing/delays/measurements |
| 0x40003800 | I2C1 / SPI2 | Touch panel communication (I2C protocol confirmed by register bit patterns) |
| 0x40005C00 | USB FS Device | USB mass storage mode ("USB Sharing") |
| 0x40006000 | CAN0 SRAM / USB SRAM | USB/CAN shared memory region |
| 0x40010C00 | GPIOB | Pin configuration (used by GPIO init function FUN_080302fc) |
| 0x40011000 | GPIOC | Pin reads (FUN_080304e0 reads pin 7) |
| 0x40021000 | RCU (RCC) | Clock configuration |
| 0xA0000000 | EXMC (FSMC) | LCD display parallel bus interface |
| 0x60000000+ | EXMC Data Region | LCD command/data writes (memory-mapped) |

## Active Interrupt Handlers (V1.2.0)

| Vector | IRQ | Handler Address | Purpose |
|---|---|---|---|
| 15 | SysTick | 0x0802A995 | System tick timer |
| 25 | EXTI3 | 0x08009C11 | **Continuity detection** (added in V1.2.0 for buzzer fix) |
| 28 | DMA1_Ch2 | 0x08009671 | DMA transfer (likely SPI↔memory for display or flash) |
| 36 | USB_LP_CAN_RX0 | 0x0802E8E5 | USB device interrupt |
| 45 | TIM3 | 0x0802E71D | Timer (measurements, delays, auto-shutdown countdown?) |
| 54 | USART2 | 0x0802E7B5 | UART communication (**possibly FPGA command interface**) |
| 59 | TIM8_BRK | 0x0802E78D | Timer 8 break (PWM for buzzer or signal generator) |

## Unused But Available Peripherals

These are built into the GD32F307 but have no interrupt handlers in the firmware:

| Peripheral | Potential Use |
|---|---|
| **CAN0 (0x40006400)** | Native CAN bus receive — protocol decode without external hardware |
| **CAN1 (0x40006800)** | Second CAN channel |
| **DAC (0x40007000)** | Likely used for signal generator but via polling, not interrupts |
| **ADC0 (0x40013000)** | Internal ADC (separate from FPGA's external ADC) |
| **SPI0 (0x40013800)** | SPI bus (likely used for external flash, no interrupt) |
| **TIM1, TIM2, TIM4-7** | Additional timers available for new features |
| **USART1, USART3, UART4, UART5** | Additional serial ports |
| **I2C2 (0x40005800)** | Second I2C bus |
| **DMA2** | Additional DMA channels |

## FPGA Communication

The FPGA communication method is not yet fully determined. Two candidates:

1. **USART2** — Has an active interrupt handler. References string addresses in the data section. Could be a serial command/response interface with the FPGA.

2. **GPIO bit-banging** — The 1013D/1014D used GPIO bit-banging (8-bit data bus + clock/read-write/command-data control lines on Port E). The 2C53T may use a similar approach through GPIOB/GPIOC.

The 1013D FPGA interface used:
- 8-bit data bus: PE0:7
- Clock: PE08
- Read/Write: PE09 (0=read, 1=write)
- Data/Command: PE10 (0=data, 1=command)

The 2C53T may use a different port but a similar protocol. Determining this is a key RE milestone.

## FPGA Commands (from 1013D — may be similar)

These were documented by pecostm32 for the 1013D. The 2C53T FPGA may use similar commands:

| Command | Function |
|---|---|
| 0x01 | Start/stop signal acquisition |
| 0x02 | Channel 1 enable |
| 0x03 | Channel 2 enable |
| 0x05 | Wait flag (lsb must become 1) |
| 0x06 | FPGA version check |
| 0x0D/0x0E | Timebase configuration |
| 0x15 | Trigger channel select |
| 0x16 | Trigger edge (0=rising, 1=falling) |
| 0x17 | 50% trigger setting |
| 0x1A | Trigger mode (0=auto, 1=normal/single) |
| 0x20-0x23 | Channel signal data readout |
| 0x32-0x37 | Channel voltage/coupling config |
| 0x38 | Display brightness |

**Warning:** These are from the 1013D. The 2C53T FPGA may use different commands. Do not assume compatibility.
