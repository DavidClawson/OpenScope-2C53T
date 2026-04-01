# Hardware Test Firmware Inventory

## Overview

Over the course of bringing up custom firmware on the FNIRSI 2C53T, 16 standalone test programs were written and flashed to investigate unknown hardware behavior. Each test targeted a specific hypothesis about how buttons, FPGA communication, or peripherals work. The approach was iterative: flash a test, observe results on the LCD or via UART, form a new hypothesis, write the next test.

The comprehensive FPGA task decompilation (April 2026) later explained why several tests produced unexpected results. This document records what each test tried, what it found, and what we now understand in hindsight.

## Build System

All tests are built via `firmware/Makefile.hwtest`:

```bash
make -f Makefile.hwtest TEST=hwtest       # Build (default target)
make -f Makefile.hwtest TEST=spi3test     # Build a specific test
make -f Makefile.hwtest TEST=fpgatalk flash  # Build and flash via DFU
```

Each test is a single `.c` file in `firmware/src/` compiled against the AT32F403A HAL library. The build links the AT32 clock init, GPIO, flash, USART, WDT, and XMC drivers. Output is a raw `.bin` flashed at `0x08000000` via `dfu-util`.

All tests share the same boilerplate: PC9 power hold, PB8 backlight enable, EXMC LCD init, and a minimal 5x7 bitmap font for hex display on the ST7789V screen.

## Test Timeline & Results

### Phase 1: Basic Bring-up

#### `hwtest.c` -- First sign of life
- **Goal:** Verify AT32F403A boots, LCD works via EXMC, power hold keeps device on.
- **Method:** Bare-register GPIO and EXMC configuration. Fill LCD with a solid color.
- **Result:** Success. Confirmed PC9 = power hold, PB8 = backlight, EXMC memory-mapped LCD at 0x6001FFFE/0x60020000.
- **Key finding:** IOMUX (AFIO) clock must be enabled for EXMC alternate function pins to work.

#### `ramtest.c` -- SWD-loaded RAM test
- **Goal:** Quick register-poke test loadable via SWD without reflashing.
- **Method:** No vector table or startup code. Raw register writes to GPIO and EXMC from RAM.
- **Result:** Useful for rapid iteration before DFU flashing was reliable. Confirmed register addresses.

#### `test_lcd_boot.c` -- Minimal LCD isolation test
- **Goal:** Isolate the LCD init path for Renode emulator debugging.
- **Method:** No FreeRTOS, no peripherals. Just clock init, EXMC init, ST7789V init, fill red, spin.
- **Result:** Confirmed LCD works independently. Used to debug Renode EXMC model.

#### `eopb0_setup.c` -- One-shot SRAM configuration
- **Goal:** Set EOPB0 option byte to 0xFE, enabling 224KB SRAM (default is 96KB).
- **Method:** Flash unlock, erase user system data, write EOPB0 = 0xFE, relock. Run once, then flash real firmware.
- **Result:** Success. Required for FreeRTOS heap (32KB) plus display buffers. Runs on internal 8MHz oscillator to avoid flash timing issues.

### Phase 2: Button Discovery

The device has 15 physical buttons. Only PB7 (PRM, active-high) and PE2 (right arrow, active-low) were initially found on MCU GPIO. The remaining 13 buttons were a mystery.

#### `btntest.c` -- GPIO register dump
- **Goal:** Read all GPIOA-GPIOE input data registers, display on LCD, press buttons and watch for bit changes.
- **Method:** Configured all non-EXMC pins as inputs with pull-ups. Continuous loop reading IDR registers.
- **Result:** Found PB7 and PE2. No other GPIO bits changed when pressing the remaining 13 buttons.
- **Hypothesis formed:** Buttons might be on an I2C touch controller or FPGA.

#### `btntest2.c` -- Extended GPIO + UART output
- **Goal:** Test the non-EXMC pins on Port D (PD2, PD3, PD6, PD13) and Port E (PE0-PE6) that btntest.c may have misconfigured. Also output via USART2 TX (PA2) for ESP32 bridge capture.
- **Method:** Careful per-pin configuration avoiding EXMC conflicts. Known buttons PB7 and PE2 used as controls. Dual output: LCD display + serial.
- **Result:** Confirmed PB7 and PE2. Still no other GPIO changes on button press.
- **Hypothesis refined:** Buttons share EXMC data bus pins (PE7-PE15), time-multiplexed with LCD.

#### `matrix.c` -- EXMC bus button matrix scan
- **Goal:** Test the hypothesis that buttons are multiplexed on PE7-PE15 (LCD data bus). Temporarily disable EXMC, reconfigure PE7-15 as inputs, scan for button states, restore EXMC, display results.
- **Method:** EXMC disable, reconfigure pins, scan all row/column combinations, re-enable EXMC for display.
- **Result:** No button signals detected on PE7-15.
- **Hypothesis disproven:** Buttons are NOT on the LCD data bus.

#### `scantest.c` -- ADC + I2C peripheral scanner
- **Goal:** Scan ADC channels and I2C buses to find button inputs. Maybe buttons use a resistor ladder (ADC) or an I2C expander.
- **Method:** ADC1 channels 0-15 sampled continuously. I2C1 (PB6/PB7) and I2C2 (PB10/PB11) address scanned. UART output on both USART1 (PA9) and USART3 (PB10) to find debug pads.
- **Result:** No ADC channels responded to button presses. No I2C devices acknowledged on either bus.
- **Hypothesis disproven:** No resistor ladder, no I2C expander.

#### `spiscan.c` -- SPI2 button probe
- **Goal:** Check if FPGA sends button state over SPI2 (PB13/PB14/PB15), which the early decompilation suggested was the FPGA data channel.
- **Method:** Configure SPI2 as master, assert CS on PB6, clock out dummy bytes, read responses. Monitor PB7 and PE2 for cross-reference.
- **Result:** SPI2 returned only 0xFF. No button data.
- **Critical misidentification:** The decompiled firmware accesses at 0x40003800 were labeled "SPI2" but the FPGA bulk data interface is actually **SPI3** (0x40003C00) on PB3/PB4/PB5 (JTAG pins, remapped). This test was probing the wrong peripheral -- SPI2 is the W25Q128 SPI flash chip.

#### What we now know about buttons
The comprehensive decompilation of `input_and_housekeeping` (sub-function 8 of the FPGA task) revealed that buttons are **not** on FPGA, I2C, ADC, or the LCD bus. They are read from MCU GPIO via a **3-group multiplexed scan** of GPIOB and GPIOC pins:
- Group 1: GPIOC-derived (probe change/type detection)
- Group 2: PB8-derived (4 button clusters via bit masking)
- Group 3: GPIOC bit 10 (trigger, select, menu, OK via multiplexed reads)

The scanning uses specific GPIO output drive patterns and reads the resulting input states -- a technique our simple "configure as input and read" tests could not detect because they never drove the output side of the scan matrix.

### Phase 3: FPGA Communication

#### `uartsniff.c` -- USART discovery
- **Goal:** Initialize all 5 USARTs as RX at 9600 baud. See if the FPGA is transmitting on any of them.
- **Method:** USART1 (PA9/PA10), USART2 (PA2/PA3), USART3 (PB10/PB11), UART4 (PC10/PC11), UART5 (PC12/PD2) all configured for receive. Display byte counts and last received bytes on LCD.
- **Result:** USART2 (PA3 RX) received data from the FPGA. 12-byte frames with 0x5A 0xA5 header confirmed.
- **Key finding:** FPGA command channel is USART2 at 9600 baud. PA2 = TX to FPGA, PA3 = RX from FPGA.

#### `fpgaprobe.c` -- USB peripheral probe (dead end)
- **Goal:** Test whether the FPGA communicates via the USB peripheral hardware. The decompiled firmware accesses USB endpoint registers at 0x40005C00 and packet buffer SRAM at 0x40006000.
- **Method:** Read USB registers raw, enable USB clock, dump packet buffer, read PA11/PA12 as GPIO, try register patterns from decompiled code.
- **Result:** USB registers were at default/reset values. No FPGA traffic on USB.
- **In hindsight:** The 0x40005C00 accesses in the decompile turned out to be I2C2 register accesses (touch panel), not USB. Ghidra labeled the address range ambiguously.

#### `fpgacmd.c` -- USART command sender (TX only)
- **Goal:** Send 10-byte command frames to FPGA via USART2 TX (PA2). Capture responses externally via logic analyzer on debug UART pads.
- **Method:** Format frames per the decompiled protocol. LCD shows what was sent + button press reminder.
- **Result:** Commands sent successfully. FPGA responded with ACK frames on PA3. Confirmed the 10-byte TX / 12-byte RX frame format.

#### `fpgatalk.c` -- Bidirectional USART communication
- **Goal:** Send commands AND receive/display responses. Highlight bytes that change to detect button data in FPGA responses.
- **Method:** Full bidirectional USART2 at 9600 baud. Parse RX frames, display all bytes on LCD, color-code changes.
- **Result:** Confirmed bidirectional USART protocol. Meter data (BCD-encoded readings) visible in RX frames. No button data in USART frames -- buttons come from GPIO, not FPGA.

#### `fpgainit.c` -- FPGA initialization + response monitor
- **Goal:** Send the oscilloscope mode initialization command sequence to FPGA, then monitor response frames for button/state data.
- **Method:** TX frames with AA 55 header (corrected from earlier attempts). Monitor 12-byte RX frames with 5A A5 header.
- **Result:** FPGA responded to mode commands. Meter data frames decoded. Confirmed the USART protocol works bidirectionally.
- **Key finding:** The USART channel carries commands and meter data, NOT oscilloscope ADC samples. ADC data comes via a separate high-speed interface.

#### `spi3test.c` -- SPI3 FPGA ADC interface (the critical test)
- **Goal:** Read ADC sample data from FPGA via SPI3. Based on the corrected identification that FPGA bulk data uses SPI3 (0x40003C00) on PB3/PB4/PB5 (JTAG pins, remapped), not SPI2.
- **Method:** JTAG-to-SWD remap to free PB3/PB4/PB5. SPI3 Mode 3 (CPOL=1, CPHA=1), master, software CS on PB6. PC6 set HIGH (SPI enable). Send command bytes per decompiled protocol, read responses. Also configured correct 3-byte USART command format (high, low, checksum).
- **V1 result:** SPI3 operational (clock toggling confirmed on logic analyzer), but all reads returned 0xFF.
- **V2 result:** Switched to Mode 3 (from Mode 0). Still 0xFF.
- **Result:** SPI3 hardware is connected (PB4 MISO physically wired to FPGA, confirmed by GPIO toggle test), peripheral is configured correctly, but FPGA does not respond with data.

#### Why SPI3 returned 0xFF -- the 4 missing pieces
The comprehensive FPGA task decompilation (`FPGA_TASK_ANALYSIS.md`) explained the failures:

1. **PB11 not set HIGH.** The FPGA requires PB11 = HIGH to enter "active measurement mode." Our test never set this pin.
2. **USART boot sequence not sent.** The stock firmware sends a 53-step initialization sequence of USART commands (documented in `FPGA_BOOT_SEQUENCE.md`) before any SPI3 data transfers. Our test jumped straight to SPI3.
3. **Wrong trigger mechanism.** SPI3 reads are initiated by FreeRTOS queue events from the `input_and_housekeeping` timer task, not by polling. The FPGA expects the MCU to request data at specific intervals driven by TMR3.
4. **Missing SysTick delays.** The boot sequence has specific timing requirements between initialization phases. Our test did not replicate these delays.

## Lessons Learned

**What worked well:**
- The incremental test approach correctly identified USART2 as the FPGA command channel and confirmed the pin assignments (PA2/PA3).
- Hardware probing with a logic analyzer was essential for verifying SPI3 clock activity and MISO connectivity.
- Each test built on previous findings, narrowing the search space systematically.
- The `eopb0_setup.c` one-shot utility solved a critical bring-up blocker (96KB vs 224KB SRAM).

**What led us astray:**
- Early decompilation misidentified SPI2 (0x40003800) as the FPGA data channel. It was actually SPI3 (0x40003C00). The `spiscan.c` test wasted time probing the SPI flash chip.
- The assumption that buttons must be on FPGA/I2C/ADC was wrong. The 3-group GPIO scan technique is invisible to passive input reading -- you must actively drive outputs to read the scan matrix.
- The USB peripheral probe (`fpgaprobe.c`) was based on a Ghidra labeling ambiguity (0x40005C00 is I2C2 on AT32, not USB endpoints).
- Testing SPI3 in isolation (without the full USART boot sequence and control pins) was doomed to fail. The FPGA is a state machine that requires proper initialization before it will respond on SPI3.

**What we'd do differently:**
- Decompile the full FPGA task FIRST before writing hardware tests. The 12,700-line decompilation answered every open question.
- Use a logic analyzer from day one to capture the stock firmware's SPI3 and USART2 traffic as a reference waveform, rather than trying to reproduce the protocol from incomplete decompilation.
- Test control pins (PC6, PB11) systematically. A simple "toggle every unused GPIO and see if SPI3 behavior changes" test would have found PB11 much faster.

## What's Next

A corrected SPI3 test firmware should incorporate all four missing pieces:

1. Set PB11 HIGH before any SPI3 communication
2. Send the full 53-step USART boot command sequence (from `FPGA_BOOT_SEQUENCE.md`)
3. Use a timer (TMR3) to trigger SPI3 reads at the correct interval, via queue events
4. Include proper SysTick delays between boot phases
5. Apply ADC offset correction of -28.0 LSBs to raw samples
6. Send TWO queue items per acquisition trigger (double-buffer scheme)

Additionally, a button scanning test should implement the 3-group GPIO multiplexed scan from the `input_and_housekeeping` decompilation to finally map all 15 physical buttons.

## File Reference

| File | Phase | Purpose |
|------|-------|---------|
| `firmware/src/hwtest.c` | 1 | First boot: power hold, LCD, EXMC |
| `firmware/src/ramtest.c` | 1 | SWD-loaded RAM test (no flash) |
| `firmware/src/test_lcd_boot.c` | 1 | Minimal LCD for Renode debugging |
| `firmware/src/eopb0_setup.c` | 1 | One-shot 224KB SRAM enable |
| `firmware/src/btntest.c` | 2 | GPIO register dump for buttons |
| `firmware/src/btntest2.c` | 2 | Extended GPIO + UART output |
| `firmware/src/matrix.c` | 2 | EXMC bus matrix scan |
| `firmware/src/scantest.c` | 2 | ADC + I2C peripheral scanner |
| `firmware/src/spiscan.c` | 2 | SPI2 button probe (wrong SPI) |
| `firmware/src/uartsniff.c` | 3 | All-USART RX sniffer |
| `firmware/src/fpgaprobe.c` | 3 | USB peripheral probe (dead end) |
| `firmware/src/fpgacmd.c` | 3 | USART TX command sender |
| `firmware/src/fpgatalk.c` | 3 | Bidirectional USART monitor |
| `firmware/src/fpgainit.c` | 3 | FPGA init + response monitor |
| `firmware/src/spi3test.c` | 3 | SPI3 ADC interface test |
| `firmware/Makefile.hwtest` | -- | Build system for all tests |
