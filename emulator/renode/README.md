# FNIRSI 2C53T - Renode Emulation

Renode-based emulation of the FNIRSI 2C53T oscilloscope's GD32F307VET6 MCU.

## Prerequisites

### Renode Installation (macOS)

Renode v1.16.1 is installed at `/Applications/Renode.app`.

To install manually:
1. Download the macOS ARM64 portable DMG from:
   https://github.com/renode/renode/releases/download/v1.16.1/renode-1.16.1-dotnet.osx-arm64-portable.dmg
2. Open the DMG and drag `Renode.app` to `/Applications/`
3. Verify: `/Applications/Renode.app/Contents/MacOS/renode --version`

### Firmware

The firmware binary must be present at:
```
../../2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin
```
(relative to this directory)

## Files

- `gd32f307_2c53t.repl` - Platform description defining the GD32F307 hardware:
  CPU (Cortex-M4F @ 120MHz), 1MB flash, 256KB RAM, GPIO A-E, USART2 (FPGA),
  TIM2-4, I2C1 (touch), SPI1 (flash), EXMC/FSMC (LCD), DMA, and more.
- `run_2c53t.resc` - Renode script that loads the platform and firmware, then starts emulation.

## Running

```bash
# From this directory:
/Applications/Renode.app/Contents/MacOS/renode run_2c53t.resc

# Or launch Renode first, then in the Renode monitor:
(monitor) include @run_2c53t.resc
```

## What to Expect

The emulation will boot the firmware. Since many peripherals (FPGA via USART2,
LCD via EXMC, touch via I2C, ADC) are stubbed as memory regions, the firmware
will likely stall waiting for hardware responses. This is expected and useful for:

- Tracing early initialization code paths
- Observing register access patterns
- Understanding the boot sequence before peripheral communication begins
- Identifying which peripherals the firmware probes first

## Debugging Tips

In the Renode monitor:

```
# Pause emulation
pause

# Read CPU registers
cpu PC           # Program counter
cpu SP           # Stack pointer

# Read memory
sysbus ReadDoubleWord 0x08000000   # Reset vector table
sysbus ReadDoubleWord 0x40021000   # RCU_CTL register

# Set logging on a peripheral
logLevel 0 usart2    # Verbose logging for FPGA UART

# Step execution
cpu Step 1

# Resume
start
```

## Hardware Map

| Peripheral   | Address      | Model / Notes                    |
|-------------|-------------|----------------------------------|
| Flash       | 0x08000000  | 1MB (also mirrored at 0x00000000)|
| SRAM        | 0x20000000  | 256KB                            |
| GPIOA       | 0x40010800  | STM32F1 GPIO model               |
| GPIOB       | 0x40010C00  | STM32F1 GPIO model               |
| GPIOC       | 0x40011000  | STM32F1 GPIO model               |
| GPIOD       | 0x40011400  | STM32F1 GPIO model               |
| GPIOE       | 0x40011800  | STM32F1 GPIO model               |
| USART2      | 0x40004400  | FPGA communication (IRQ 38)      |
| TIM3        | 0x40000400  | Timer (IRQ 29)                   |
| I2C1        | 0x40005400  | Touch panel (GT911)              |
| SPI1        | 0x40013000  | External flash (W25Q64)          |
| USB         | 0x40005C00  | USB device registers             |
| RCU/RCC     | 0x40021000  | Clock control (stubbed)          |
| EXMC (LCD)  | 0x60000000  | LCD data via parallel bus         |
| EXMC ctrl   | 0xA0000000  | FSMC/EXMC control registers      |
| NVIC        | 0xE000E000  | Cortex-M4 interrupt controller   |
