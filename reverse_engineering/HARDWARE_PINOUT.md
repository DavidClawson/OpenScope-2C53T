# FNIRSI 2C53T Complete Hardware Pinout & Peripheral Reference

*Compiled from Ghidra decompilation of V1.2.0 firmware, hardware probing on 2C53T-V1.4 board, and custom firmware validation.*

---

## 1. MCU: Artery AT32F403A

| Parameter | Value |
|-----------|-------|
| Core | ARM Cortex-M4F (hardware FPU) |
| Clock | 240 MHz (PLL from external crystal) |
| Flash | 1 MB (0x08000000 - 0x080FFFFF) |
| SRAM | 224 KB (requires EOPB0 = 0xFE; default is 96 KB) |
| Package | LQFP-100 (markings sanded off by manufacturer) |
| Compatibility | Register-compatible with GD32F307 / STM32F103 at GPIO/EXMC level |
| SWD | PA13 (SWDIO), PA14 (SWCLK) -- JTAG disabled, SWD preserved |
| Supply | 3.3V from internal regulator, powered by Li-ion battery |

**EOPB0 note:** The AT32 defaults to 96KB SRAM. This firmware requires 224KB. EOPB0 must be programmed once via DFU option bytes before flashing firmware.

---

## 2. Pin Assignment Table -- By Port

### GPIOA (0x40010800)

| Pin | Function | Dir | Config | Speed | Evidence | Notes |
|-----|----------|-----|--------|-------|----------|-------|
| PA0 | Unknown | -- | -- | -- | No references found | |
| PA1 | Unknown | -- | -- | -- | No references found | |
| PA2 | USART2 TX | AF | AF push-pull | 50 MHz | `FPGA_BOOT_SEQUENCE.md` step 9; `usart_tx_frame_builder` | MCU to FPGA commands |
| PA3 | USART2 RX | Input | Floating | -- | `FPGA_BOOT_SEQUENCE.md` step 9; `usart2_isr` at 0x080277B4 | FPGA to MCU responses; same signal on debug UART RX pad |
| PA4 | Unknown | -- | -- | -- | | Possible DAC1 output |
| PA5 | Unknown | -- | -- | -- | | Possible DAC2 output |
| PA6 | Unknown | -- | -- | -- | No references found | |
| PA7 | Button matrix row | Input | Pull-up | -- | `peripheral_map.md`; `input_and_housekeeping` at 0x08039188 | CH2 button row pin |
| PA8 | Button matrix row | Input | Pull-up | -- | `peripheral_map.md`; `input_and_housekeeping` | Right button row pin |
| PA9 | USART1 TX | AF | -- | -- | Probed: dead (0 bytes) | Not used by stock firmware |
| PA10 | USART1 RX | Input | -- | -- | Probed: dead (0 bytes) | Not used by stock firmware |
| PA11 | USB D- | AF | -- | -- | `usb_endpoint_handler` at 0x080278E4 | USB FS device |
| PA12 | USB D+ | AF | -- | -- | `usb_endpoint_handler` at 0x080278E4 | USB FS device |
| PA13 | SWDIO | AF | -- | -- | JTAG disabled via AFIO remap; SWD preserved | Debug header |
| PA14 | SWCLK | AF | -- | -- | JTAG disabled via AFIO remap; SWD preserved | Debug header |
| PA15 | Analog MUX control | Output | Push-pull | 50 MHz | `gpio_mux_porta_portb` at 0x08001A58 | 10-mode analog routing |

### GPIOB (0x40010C00)

| Pin | Function | Dir | Config | Speed | Evidence | Notes |
|-----|----------|-----|--------|-------|----------|-------|
| PB0 | Button matrix row | Input | Pull-up | -- | `peripheral_map.md` | Save button row pin |
| PB1 | Unknown | -- | -- | -- | No references found | |
| PB2 | Unknown (BOOT1) | -- | -- | -- | | |
| PB3 | SPI3 SCK | AF | AF push-pull | 50 MHz | `spi3_init_and_setup` at 0x08039050; hardware verified | JTAG pin (JTDO), freed by AFIO remap |
| PB4 | SPI3 MISO | Input | Floating | -- | `spi3_init_and_setup`; GPIO test confirmed physical FPGA connection | JTAG pin (JNTRST), freed by AFIO remap |
| PB5 | SPI3 MOSI | AF | AF push-pull | 50 MHz | `spi3_init_and_setup` | |
| PB6 | SPI3 CS (FPGA) | Output | Push-pull (GPIO) | 50 MHz | `spi3_init_and_setup`; 3 refs to GPIOB BOP/BCR in FPGA task | Active LOW; software-controlled |
| PB7 | Button (PRM) | Input | Pull-up | -- | `peripheral_map.md` | Direct GPIO, active-low |
| PB8 | LCD backlight | Output | Push-pull | 50 MHz | `main.c` line 299; `FPGA_BOOT_SEQUENCE.md` | HIGH = backlight on |
| PB9 | Unknown | -- | -- | -- | | |
| PB10 | Analog MUX control | Output | Push-pull | 50 MHz | `gpio_mux_porta_portb` at 0x08001A58 | 10-mode analog routing; USART3 TX probed dead |
| PB11 | FPGA active mode | Output | Push-pull | 50 MHz | `FPGA_BOOT_SEQUENCE.md` step 52; `FUN_08037800` | HIGH = FPGA active measurement mode; set via GPIOB_BOP = 0x800 |
| PB12 | SPI2 CS (flash) | Output | Push-pull | 50 MHz | `spi2_block_read` at 0x0802F048; hardware verified | Active LOW; Winbond W25Q128JV chip select |
| PB13 | SPI2 SCK | AF | AF push-pull | 50 MHz | `peripheral_map.md`; hardware verified | SPI flash clock |
| PB14 | SPI2 MISO | AF | Floating | -- | `spi2_transceive_byte` at 0x0802F0C4; hardware verified | SPI flash data in |
| PB15 | SPI2 MOSI | AF | AF push-pull | 50 MHz | `spi2_transceive_byte`; hardware verified | SPI flash data out |

### GPIOC (0x40011000)

| Pin | Function | Dir | Config | Speed | Evidence | Notes |
|-----|----------|-----|--------|-------|----------|-------|
| PC0 | Probe continuity | Input | Pull-up | -- | `peripheral_map.md` | |
| PC1 | Unknown | -- | -- | -- | | |
| PC2 | Unknown | -- | -- | -- | | |
| PC3 | Unknown | -- | -- | -- | | |
| PC4 | Unknown | -- | -- | -- | | |
| PC5 | Button matrix row | Input | Pull-up | -- | `peripheral_map.md`; `input_and_housekeeping` | Down button row pin |
| PC6 | FPGA SPI enable | Output | Push-pull | 50 MHz | `FPGA_BOOT_SEQUENCE.md` step 19; set via GPIOC_BOP = (1<<6) | Must be HIGH for SPI3 to communicate with FPGA |
| PC7 | Probe detect | Input | Pull-up | -- | `FPGA_TASK_ANALYSIS.md` USART cmd 0x07 | HIGH = probe detected |
| PC8 | Unknown | -- | -- | -- | | |
| PC9 | Power hold | Output | Push-pull | 50 MHz | `main.c` line 281; `FPGA_BOOT_SEQUENCE.md` step 2 | **Must be HIGH immediately at boot or device shuts off** |
| PC10 | Button matrix row | Input | Pull-up | -- | `peripheral_map.md`; `input_and_housekeeping` | Auto button row; also GPIOC IDR bit 10 scanned for Group 3 buttons |
| PC11 | Unknown | -- | -- | -- | | |
| PC12 | Analog front-end | Output | Push-pull | 50 MHz | `gpio_mux_portc_porte` at 0x080018A4 | Relay/MUX switching |
| PC13 | Unknown | -- | -- | -- | | |
| PC14 | Unknown | -- | -- | -- | | |
| PC15 | Unknown | -- | -- | -- | | |

**GPIOC IDR reads** (0x40011008): 5 functions read GPIOC IDR for probe detection and button scanning: `FUN_0800bc98`, `FUN_0800bc00`, `FUN_0800bba6`, `FUN_0800bb10`, `FUN_0800ba06`, plus `input_and_housekeeping`.

### GPIOD (0x40011400)

| Pin | Function | Dir | Config | Speed | Evidence | Notes |
|-----|----------|-----|--------|-------|----------|-------|
| PD0 | EXMC D2 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()`; `main.c` | LCD data bus |
| PD1 | EXMC D3 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PD2 | Unknown | -- | -- | -- | | |
| PD3 | Unknown | -- | -- | -- | | |
| PD4 | EXMC NOE | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD read strobe |
| PD5 | EXMC NWE | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD write strobe |
| PD6 | Unknown | -- | -- | -- | | |
| PD7 | EXMC NE1 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD chip select (Bank 0) |
| PD8 | EXMC D13 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PD9 | EXMC D14 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PD10 | EXMC D15 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PD11 | EXMC CLE / A16 | AF | AF push-pull | 50 MHz | `main.c` line 324 configures PD11 as AF-PP | LCD bus; may not be used for address |
| PD12 | EXMC A17 (RS/DCX) | AF | AF push-pull | 50 MHz | `lcd_gpio_init()`; address decode confirmed | A17=0: command, A17=1: data |
| PD13 | Signal routing | Output | Push-pull | 50 MHz | `hardware_map.txt` GPIOD BOP writes from `siggen_configure` | |
| PD14 | EXMC D0 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PD15 | EXMC D1 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |

### GPIOE (0x40011800)

| Pin | Function | Dir | Config | Speed | Evidence | Notes |
|-----|----------|-----|--------|-------|----------|-------|
| PE0 | Unknown | -- | -- | -- | | |
| PE1 | Unknown | -- | -- | -- | | |
| PE2 | Button matrix col | Output | Push-pull | 50 MHz | `peripheral_map.md` | Column for Right, Auto |
| PE3 | Button matrix col | Output | Push-pull | 50 MHz | `peripheral_map.md` | Column for CH2, Down, Save |
| PE4 | Analog front-end | Output | Push-pull | 50 MHz | `gpio_mux_portc_porte` at 0x080018A4 | Relay/MUX switching |
| PE5 | Analog front-end | Output | Push-pull | 50 MHz | `gpio_mux_portc_porte` | Relay/MUX switching |
| PE6 | Analog front-end | Output | Push-pull | 50 MHz | `gpio_mux_portc_porte` | Relay/MUX switching |
| PE7 | EXMC D4 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE8 | EXMC D5 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE9 | EXMC D6 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE10 | EXMC D7 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE11 | EXMC D8 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE12 | EXMC D9 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE13 | EXMC D10 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE14 | EXMC D11 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |
| PE15 | EXMC D12 | AF | AF push-pull | 50 MHz | `lcd_gpio_init()` | LCD data bus |

---

## 3. Pin Assignment Table -- By Function

### LCD (EXMC 16-bit parallel)

| Signal | Pin | Type |
|--------|-----|------|
| D0 | PD14 | AF push-pull |
| D1 | PD15 | AF push-pull |
| D2 | PD0 | AF push-pull |
| D3 | PD1 | AF push-pull |
| D4 | PE7 | AF push-pull |
| D5 | PE8 | AF push-pull |
| D6 | PE9 | AF push-pull |
| D7 | PE10 | AF push-pull |
| D8 | PE11 | AF push-pull |
| D9 | PE12 | AF push-pull |
| D10 | PE13 | AF push-pull |
| D11 | PE14 | AF push-pull |
| D12 | PE15 | AF push-pull |
| D13 | PD8 | AF push-pull |
| D14 | PD9 | AF push-pull |
| D15 | PD10 | AF push-pull |
| NWE (write strobe) | PD5 | AF push-pull |
| NOE (read strobe) | PD4 | AF push-pull |
| NE1 (chip select) | PD7 | AF push-pull |
| A17 (RS/DCX) | PD12 | AF push-pull |

**Total: 20 pins** (16 data + 4 control). PD11 is also configured as AF push-pull in the working firmware but its EXMC role (A16 or CLE) is unclear.

### FPGA SPI3 (bulk ADC data)

| Signal | Pin | Config | Evidence |
|--------|-----|--------|----------|
| SCK | PB3 | AF push-pull 50 MHz | `spi3_init_and_setup` at 0x08039050 |
| MISO | PB4 | Input floating | `spi3_init_and_setup`; hardware verified on board |
| MOSI | PB5 | AF push-pull 50 MHz | `spi3_init_and_setup` |
| CS | PB6 | GPIO output push-pull | Software CS, active LOW; `spi3_acquisition_task` |

PB3 and PB4 are JTAG pins by default. Requires AFIO remap: `AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000` (disable JTAG-DP, keep SW-DP). Source: `FPGA_BOOT_SEQUENCE.md` step 4.

### FPGA USART2 (command/control)

| Signal | Pin | Config | Evidence |
|--------|-----|--------|----------|
| TX (MCU to FPGA) | PA2 | AF push-pull | `FPGA_BOOT_SEQUENCE.md` step 9 |
| RX (FPGA to MCU) | PA3 | Input floating | `usart2_isr` at 0x080277B4; heartbeat captured on hardware |

### FPGA Control

| Signal | Pin | Config | Evidence |
|--------|-----|--------|----------|
| SPI enable | PC6 | GPIO output push-pull | `FPGA_BOOT_SEQUENCE.md` step 19; HIGH enables FPGA SPI3 |
| Active mode | PB11 | GPIO output push-pull | `FPGA_BOOT_SEQUENCE.md` step 52; HIGH during active measurement; GPIOB_BOP = 0x800 |

### SPI Flash (SPI2) -- Winbond W25Q128JV

| Signal | Pin | Config | Evidence |
|--------|-----|--------|----------|
| SCK | PB13 | AF push-pull 50 MHz | `peripheral_map.md`; hardware verified (JEDEC: EF 40 18) |
| MISO | PB14 | Input floating | `spi2_transceive_byte` at 0x0802F0C4 |
| MOSI | PB15 | AF push-pull 50 MHz | `spi2_transceive_byte` |
| CS | PB12 | GPIO output push-pull | Active LOW; `spi2_block_read` at 0x0802F048 |

### Analog Front-End MUX

Two 10-mode GPIO multiplexing functions control analog signal routing:

**Port A/B MUX** (`gpio_mux_porta_portb` at 0x08001A58):

| Pin | Function | Evidence |
|-----|----------|----------|
| PA15 | MUX control | Writes to GPIOA BOP/BCR (0x40010810/0x40010814) |
| PB10 | MUX control | Writes to GPIOB BOP/BCR (0x40010C10/0x40010C14) |
| PB11 | MUX control / FPGA active | Dual use: MUX routing + FPGA active mode |

**Port C/E MUX** (`gpio_mux_portc_porte` at 0x080018A4):

| Pin | Function | Evidence |
|-----|----------|----------|
| PC12 | Relay/MUX switching | Writes to GPIOC BOP/BCR (0x40011010/0x40011014) |
| PE4 | Relay/MUX switching | Writes to GPIOE BOP/BCR (0x40011810/0x40011814) |
| PE5 | Relay/MUX switching | Writes to GPIOE BOP/BCR |
| PE6 | Relay/MUX switching | Writes to GPIOE BOP/BCR |

After switching, both functions compute a floating-point calibration value and write a 12-bit result to DAC channel 2 (0x40007408). Calibration tables loaded from SPI flash at boot reside at 0x20000358-0x20000434.

### DAC (Signal Generator + Calibration)

| Signal | Pin | Evidence |
|--------|-----|----------|
| DAC CH1 output | PA4 (assumed) | `siggen_configure` at 0x08001C60; DAC_CTL at 0x40007404 |
| DAC CH2 output | PA5 (assumed) | `gpio_mux_portc_porte` writes calibration to 0x40007408 |

DAC register accesses confirmed: `DAC_CTL` (0x40007404) read/written by `siggen_configure`, both MUX functions, and `FUN_080018A4`. `DAC_CH2_DATA` (0x40007408) written after every analog MUX switch.

### Button Input

Three multiplexed scan groups from `input_and_housekeeping` at 0x08039188:

**Confirmed button-to-pin mapping** (from `peripheral_map.md` and FPGA task analysis):

| Button | Row Pin | Column Pin | Scan Group |
|--------|---------|------------|------------|
| PRM | PB7 | -- (direct) | -- |
| CH2 | PA7 | PE3 | Matrix |
| Down | PC5 | PE3 | Matrix |
| Save | PB0 | PE3 | Matrix |
| Right | PA8 | PE2 | Matrix |
| Auto | PC10 | PE2 | Matrix |

**GPIO scan groups** (from FPGA task analysis):

| Group | Source | Button Masks |
|-------|--------|--------------|
| 1 (GPIOC-derived) | GPIOC IDR | 0x0080 (CH1 probe), 0x0800 (CH2 probe), 0x0100 (probe type A), 0x0200 (probe type B) |
| 2 (PB8-derived) | GPIOB IDR | 0x0020, 0x0010, 0x0008 (button groups 1-3), 0x2000 (special) |
| 3 (GPIOC bit 10) | GPIOC IDR bit 10 | 0x1000 (Trigger), 0x0004 (Select), 0x4000 (Menu), 0x0002 (OK) |

**Debounce:** 70 ticks = short press confirmed; 72 ticks = long press. Button map table at 0x08046528 translates physical positions to logical IDs.

**Note:** All 15 buttons are on MCU GPIO (confirmed by `input_and_housekeeping` decompilation). Earlier hypothesis that 9 buttons were on FPGA/I2C was incorrect.

### USB

| Signal | Pin | Evidence |
|--------|-----|----------|
| USB D- | PA11 | `usb_endpoint_handler` at 0x080278E4 (USB FS Device at 0x40005C00) |
| USB D+ | PA12 | `usb_endpoint_handler` |

### Power Management

| Signal | Pin | Config | Evidence |
|--------|-----|--------|----------|
| Power hold | PC9 | Output push-pull | **First operation in main().** HIGH = keep device on. LOW = power off. |
| LCD backlight | PB8 | Output push-pull | HIGH = backlight on. Set during boot. |

### Debug / Programming

| Signal | Pin | Evidence |
|--------|-----|----------|
| SWDIO | PA13 | Debug header near USB-C port |
| SWCLK | PA14 | Debug header near USB-C port |
| Debug UART TX | Unknown MCU pin | Through-hole pad on PCB |
| Debug UART RX | Unknown MCU pin | Through-hole pad on PCB; carries same signal as PA3 (USART2 RX / FPGA heartbeat) |

---

## 4. Peripheral Configuration Reference

### EXMC/XMC (0xA0000000) -- LCD Bus Interface

```
SNCTL0  = 0x00005011 (with enable bit set)
  Bits [1:0]  = 01  NRBKEN: Bank enabled
  Bit  4      = 1   MWID[0]: 16-bit data bus width
  Bit  12     = 1   WREN: Write operations enabled
  Bit  14     = 1   EXTMOD: Extended mode (separate read/write timing)

SNTCFG0 = 0x02020424 (read timing)
  Bits [3:0]   = 4   ADDSET: Address setup = 4 HCLK cycles
  Bits [7:4]   = 2   ADDHLD: Address hold = 2 HCLK cycles
  Bits [15:8]  = 4   DATAST: Data phase = 4 HCLK cycles (bits [11:8] = 4)
  Bits [19:16] = 2   BUSTURN: Bus turnaround = 2 HCLK cycles

SNWTCFG0 = 0x00000202 (write timing, used when EXTMOD=1)
  Bits [3:0]   = 2   ADDSET: Address setup = 2 HCLK cycles
  Bits [15:8]  = 2   DATAST: Data phase = 2 HCLK cycles
```

**Timing at 240 MHz** (HCLK cycle = 4.17 ns):
- Read cycle: (4 + 2 + 4) * 4.17 ns = ~42 ns
- Write cycle: (2 + 2) * 4.17 ns = ~17 ns

Source: firmware address 0x08023C02; confirmed working in custom firmware at 240 MHz.

### SPI3 (0x40003C00) -- FPGA Data Channel

```
CTL0 register configuration (from spi3_init_and_setup at 0x08039050):
  MSTR    = 1    Master mode
  CPOL    = 1    Clock polarity: idle HIGH
  CPHA    = 1    Clock phase: sample on falling edge (Mode 3)
  BR[2:0] = 000  Baud rate: fPCLK1 / 2 = 120 MHz / 2 = 60 MHz
  DFF     = 0    8-bit data frame
  LSBFIRST= 0    MSB first
  SSM     = 1    Software slave management
  SSI     = 1    Internal slave select HIGH
  SPE     = 1    SPI enabled (set after config)

CTL1 register post-init:
  Bit 0 (RXDMAEN/TXEIE) = 1
  Bit 1 (TXDMAEN/RXNEIE) = 1
```

**Clock derivation:** APB1 clock = 120 MHz (HCLK/2). SPI3 prescaler = /2. SPI3 SCK = 60 MHz.

**Transfer protocol:**
1. Assert CS: GPIOB_BCR = 0x40 (PB6 LOW)
2. Write command byte to SPI3_DATA
3. Poll SPI3_STAT for TX empty + RX not empty
4. Read response from SPI3_DATA
5. For bulk reads: write 0xFF, read data byte (repeat)
6. Deassert CS: GPIOB_BOP = 0x40 (PB6 HIGH)

Source: `spi3_acquisition_task` at 0x08037454 and `spi3_init_and_setup` at 0x08039050.

### SPI2 (0x40003800) -- SPI Flash (W25Q128JV)

| Register | Address | Purpose |
|----------|---------|---------|
| SPI2_STAT | 0x40003808 | Status: bit 1 = TX ready, bit 0 = RX ready |
| SPI2_DATA | 0x4000380C | Data TX/RX register |

**Transceive** (`spi2_transceive_byte` at 0x0802F0C4): Poll TX ready, write byte, poll RX ready, return received byte.

**Block read** (`spi2_block_read` at 0x0802F048): Standard SPI NOR flash read -- command 0x03 + 3-byte address + bulk data. PB12 chip select.

**Flash contents:** FAT filesystem with UI assets, system files, calibration data, and unit format strings. Paths: `2:/Screenshot file/`, `3:/System file/`.

### USART2 (0x40004400) -- FPGA Command Interface

```
Baud rate:  9600 (confirmed on hardware via logic analyzer)
Data bits:  8
Stop bits:  1
Parity:     None
Mode:       TX + RX enabled
```

| Register | Address | Usage |
|----------|---------|-------|
| USART2_STAT | 0x40004400 | Status: TXE, RXNE flags |
| USART2_TDATA | 0x40004404 | Transmit data register |
| USART2_RDATA | 0x4000440C | Receive data register |

**TX frame** (MCU to FPGA, 10 bytes):
```
[0][1]: Header bytes
[2]:    Command high byte
[3]:    Command low byte (echoed back by FPGA for validation)
[4-8]:  Parameters (5 bytes, meaning varies by command)
[9]:    Checksum = (byte[2] + byte[3]) & 0xFF
```

**RX frame types** (FPGA to MCU):
- Data frame: `5A A5` header + 10 data bytes = 12 bytes total
- Echo frame: `AA 55` header + 8 bytes = 10 bytes total (rx[3] must match tx[3], rx[7] must be 0xAA)

Source: `usart2_isr` at 0x080277B4; `usart_tx_frame_builder` at 0x080373F4.

### TMR3 (0x40000400) -- USART Exchange + Button Scan Timer

```
IRQ:       45 (TIM3 global)
Handler:   tmr3_isr at 0x0802771C
Purpose:   Periodic interrupt drives USART2 byte exchange and button scan trigger
```

TMR3 ISR reads `+0x10` (status register) and writes to clear flags, then calls USART2 handler for byte-by-byte TX/RX pumping. Also triggers `input_and_housekeeping` for button scanning.

Source: `hardware_map.txt` TMR3 entries; `tmr3_isr` at 0x0802771C.

### TIM2 (0x40000000) and TIM5 (0x40000C00) -- FPGA Timing

Configured by `timer_init_helper` at 0x08039734 (called twice from `system_init`):
- First call: r0 = TIM5_CTL0 (0x40000C00)
- Second call: r0 = TIM2_CTL0 (0x40000000)

Prescaler and period loaded from stack variables set during init. Exact values not yet extracted.

Source: `gap_functions.md` P0 entry for FUN_08039734.

### DMA1 (0x40020000) -- LCD Transfer + USART2 RX

| Channel | Purpose | Evidence |
|---------|---------|----------|
| Ch0 | Cleared during init | `FPGA_BOOT_SEQUENCE.md` step 8 |
| Ch1 | LCD framebuffer blast (RAM 0x2000835C to EXMC 0x60020000) | DMA0_Ch1 ISR at 0x08009671 (vector 28) |

**DMA1 configuration** (`dma1_configure` at 0x0803BEE0): Extensive register manipulation at 0x4002001C (DMA channel config) with multiple read-modify-write sequences. Enables RCC clock at 0x40021014 for DMA.

Source: `hardware_map.txt` DMA1 entries.

### DAC (0x40007400) -- Signal Generator + Calibration

| Register | Address | Purpose |
|----------|---------|---------|
| DAC_CTL | 0x40007404 | Control: bit 0 = enable |
| DAC_CH2_DATA | 0x40007408 | Channel 2 12-bit output (lower 12 bits) |

**Uses:**
1. **Calibration offset** -- both `gpio_mux_portc_porte` and `gpio_mux_porta_portb` write computed calibration values to DAC CH2 after every analog input switch. Formula: `result = ((gain - offset) / cal_constant) * (DAT_200000fc + 100) + offset`
2. **Signal generator** -- `siggen_configure` at 0x08001C60 manages 6 output channels with 300-byte waveform descriptors, frequency stepping, sweep, and modulation modes.

Calibration table locations in RAM:

| Mode | Gain Table | Offset Table |
|------|-----------|--------------|
| Mode >= 5 | 0x20000394 | 0x20000358 |
| Mode 4 or sub-mode 3 | 0x200003A8 | 0x2000036C |
| Other modes | 0x200003BC | 0x20000380 |

Port A/B parallel tables: 0x2000040C/0x200003D0, 0x20000420/0x200003E4, 0x20000434/0x200003F8.

Source: `peripheral_map.md` Analog Front-End section; `hardware_map.txt` DAC entries.

### IWDG -- Independent Watchdog

**Timeout:** ~5 seconds.
**Feed rate:** Every 11 calls to `input_and_housekeeping` (~50 ms between feeds given TMR3 call rate).
**Feed function:** Called from within `input_and_housekeeping` at 0x08039188.

Source: `FPGA_TASK_ANALYSIS.md` Key Discovery #7.

---

## 5. EXMC LCD Interface Detail

### Address Decoding

The LCD is on EXMC Bank 0 (NE1). Address line A17 (PD12) selects command vs. data:

| Address | A17 | Function |
|---------|-----|----------|
| 0x6001FFFE | 0 | Command register (RS/DCX LOW) |
| 0x60020000 | 1 | Data register (RS/DCX HIGH) |

**How it works:** EXMC Bank 0 maps to 0x60000000-0x63FFFFFF. The base address for NE1 is 0x60000000. A17 is bit 17 of the address. To write data (A17=1), address = 0x60000000 + (1 << 17) = 0x60020000. For command (A17=0), any address with A17=0 works; 0x6001FFFE is chosen (likely legacy convention from the original firmware).

### Timing Parameters

At 240 MHz HCLK (4.17 ns per cycle):

| Parameter | Read | Write |
|-----------|------|-------|
| Address setup | 4 cycles (16.7 ns) | 2 cycles (8.3 ns) |
| Address hold | 2 cycles (8.3 ns) | 0 |
| Data phase | 4 cycles (16.7 ns) | 2 cycles (8.3 ns) |
| Bus turnaround | 2 cycles (8.3 ns) | 0 |
| **Total cycle** | **~50 ns** | **~17 ns** |

These timings are well within ST7789V specifications (minimum write cycle = 66 ns at 3.3V, but the display is tolerant of faster writes in practice).

### LCD Init Sequence

Extracted from firmware at 0x08023D66 (confirmed working in custom firmware):

```
1. Software reset    (0x01)      -- 200 ms delay
2. Sleep out         (0x11)      -- 200 ms delay
3. MADCTL            (0x36) 0xA0 -- Landscape mode, RGB order, column/row swap
4. Pixel format      (0x3A) 0x55 -- 16-bit RGB565
5. Display on        (0x29)      -- 50 ms delay
```

The stock firmware also configures porch settings, gamma curves, and other ST7789V registers (see `lcd.c` for the full sequence from address 0x08023D66-0x08023F5A). The minimal 5-step sequence above is sufficient for operation.

### DMA LCD Transfer

DMA channel 1 performs bulk framebuffer writes:
- Source: RAM at 0x2000835C
- Destination: EXMC data register at 0x60020000
- ISR: Vector 28, handler at 0x08009671

---

## 6. Analog Front-End

### GPIO MUX Routing

The oscilloscope input passes through an analog front-end controlled by 10-mode GPIO multiplexing. Two functions work in tandem:

**`gpio_mux_portc_porte`** (0x080018A4, 422 bytes): 10-way switch writing to GPIOC and GPIOE BOP/BCR registers. Controls PC12, PE4, PE5, PE6 for relay and MUX IC switching. After switching, computes a DAC calibration value and writes to DAC CH2.

**`gpio_mux_porta_portb`** (0x08001A58, 506 bytes): 10-way switch writing to GPIOA and GPIOB BOP/BCR registers. Controls PA15, PB10, PB11 for analog range/gain selection.

Both functions are called by `siggen_configure` (0x08001C60) and `scope_main_fsm` (0x08019E98).

### Probe Detection

GPIOC IDR (0x40011008) is read by 5 probe detection functions:

| Function | Address | What It Reads |
|----------|---------|---------------|
| `probe_detect_handler_1` | 0x0800BA06 | GPIOC IDR for CH1+CH2 probe state |
| `probe_detect_handler_2` | 0x0800BB10 | GPIOC IDR for trigger probe state |
| `probe_detect_handler_3` | 0x0800BC98 | GPIOC IDR single probe detect |
| `probe_change_handler` | 0x080396C8 | Probe connect/disconnect with auto power-off |
| `probe_component_test` | 0x0800BC00 | Component identification test |

**Probe detect pin:** PC7 (inferred from USART command 0x07 = "probe detected" when PC7 HIGH).

### AC/DC Coupling

Controlled via USART2 command to FPGA. Config writer types 0 and 1 (scope channel config) include bit fields:
- `config[0x20]` bit 1 = CH1 AC/DC coupling
- `config[0x20]` bit 5 = CH2 AC/DC coupling

The physical AC/DC relay switching is performed by the FPGA based on these commands.

### Voltage Range Selection

`config[0x18]` in the USART command frame encodes the voltage range:
- Bits [1:0] = range low (2 bits)
- Bits [15:12] = range high (4 bits)

The FPGA adjusts its ADC gain/attenuation accordingly. The MCU-side MUX functions also switch analog front-end relays to match.

---

## 7. Board Features

### SWD Debug Header

**Location:** Near USB-C port on the PCB.
**Pinout:**

| Pad | Signal |
|-----|--------|
| 1 | SWDIO (PA13) |
| 2 | SWCLK (PA14) |
| 3 | GND |

SWD is available because JTAG is disabled via AFIO remap (`AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000`), which frees PB3/PB4 for SPI3 while preserving SWD on PA13/PA14.

### UART Debug Pads

**Location:** Through-hole pads on PCB (not near any connector).
**Signals:** RX, TX, GND.
**Note:** The RX debug pad carries the same FPGA heartbeat signal as PA3 (USART2 RX). The MCU pins driving the debug TX pad have not been identified (not PA9/PA10, which were probed dead).

### FPGA Programming Header

**Location:** On PCB near FPGA (Gowin GW1N-UV2).
**Pinout:**

| Pad | Signal |
|-----|--------|
| M0 | Mode select 0 |
| M1 | Mode select 1 |
| M2 | Mode select 2 |
| M3 | Mode select 3 |
| GND | Ground |
| VDD | 3.3V supply |
| VPP | Programming voltage |

The Gowin GW1N-UV2 is non-volatile (retains bitstream across power cycles). Programming requires a Gowin-compatible programmer connected to this header.

### BOOT0 Resistor Pad

**Location:** Inside case, accessible when opened.
**Procedure:** Hold BOOT0 pad to 3V3, press pinhole reset, release BOOT0. Device enumerates as "AT32 Bootloader DFU" (VID:PID 2E3C:DF11).

### Pinhole Reset (NRST)

**Location:** Side of case, accessible with a pin/paperclip.
**Function:** Connects to MCU NRST pin. Resets the processor.

### Battery Connector

Li-ion battery powers the device. Battery voltage ADC channel has not yet been identified in the firmware (likely ADC1 or ADC2, configured in `system_init` at 0x08023A50).

---

## Appendix: Interrupt Vector Table

| Vector | IRQ # | Handler Address | Purpose |
|--------|-------|-----------------|---------|
| 15 | SysTick | 0x0802A995 | FreeRTOS system tick (1000 Hz) |
| 25 | EXTI3 | 0x08009C11 | Continuity buzzer detection (reads `ms+0xF5D` for state 0xB0/0xB1) |
| 28 | DMA0_Ch1 | 0x08009671 | DMA transfer complete: LCD framebuffer blast (0x2000835C to 0x60020000) |
| 36 | USB_LP_CAN_RX0 | 0x0802E8E5 | USB device interrupt |
| 45 | TIM3 | 0x0802771D | Periodic timer: drives USART2 exchange + button scan trigger |
| 54 | USART2 | 0x080277B5 | FPGA command interface: TX byte pump + RX frame parser with echo validation |
| 59 | TIM8_BRK | 0x0802E78D | Unknown purpose |

## Appendix: AFIO/IOMUX Remap Configuration

```
AFIO_PCF0 = (AFIO_PCF0 & ~0xF000) | 0x2000
```

This sets the SWJ_CFG field (bits [26:24] in the actual register, represented as bits [15:12] in the PCF0 encoding) to `010`:
- **JTAG-DP disabled** -- frees PA15, PB3, PB4 for GPIO/AF use
- **SW-DP enabled** -- preserves PA13 (SWDIO) and PA14 (SWCLK) for debugging

**Freed pins:**
| Pin | Default (JTAG) | After Remap |
|-----|---------------|-------------|
| PA15 | JTDI | GPIO: Analog MUX control |
| PB3 | JTDO/TRACESWO | SPI3 SCK |
| PB4 | JNTRST | SPI3 MISO |

## Appendix: Unknown/Unresolved Pins

The following MCU pins have no identified function from decompilation or hardware probing:

**GPIOA:** PA0, PA1, PA4, PA5, PA6 (PA4/PA5 may be DAC outputs)
**GPIOB:** PB1, PB2, PB9
**GPIOC:** PC1, PC2, PC3, PC4, PC8, PC11, PC13, PC14, PC15
**GPIOD:** PD2, PD3, PD6
**GPIOE:** PE0, PE1

Some of these may be connected to the analog front-end but controlled indirectly through the 10-mode MUX functions. Others may be genuinely unused.
