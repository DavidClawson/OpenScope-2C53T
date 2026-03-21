# Project Roadmap

## Phase 1: Pre-Device (Can Do Now)

### Step 1: Get Renode Booting Past RTOS Idle

**Goal:** Get the firmware running far enough in Renode to see display output.

**What to do:**
- The firmware is stuck because the FPGA (USART2) and DVOM chip never respond
- Write a Renode Python script or C# peripheral that simulates USART2 responses:
  - Reply with `[0x5A, 0xA5]` ACK when the firmware sends a 10-byte command
  - Return valid 10-byte status frames with `[0xAA, 0x55]` header
- Add RCU register simulation so clock checks pass (partially done with Tag values)
- Monitor what address the firmware stalls at after each fix
- **Success criteria:** firmware gets past hardware init and into the main event loop

**Why first:** This is the single biggest unlock. Once the RTOS tasks are running, we can observe the display task drawing to the framebuffer, understand the menu system, and test button interactions — all without hardware.

### Step 2: Decompile the Missing Functions

**Goal:** Get coverage of the full firmware, especially the task entry points.

**What to do:**
- The Ghidra auto-analysis only found 292 functions. The FreeRTOS task mapper found that key functions (display task at 0x0803DA50, key task at 0x08040008, osc task at 0x0804009C, fpga task at 0x0803E454) were NOT in the decompiled output.
- Fix the Ghidra base address: re-import at 0x08007000 instead of 0x08000000 (correcting the offset we discovered)
- Manually define functions at the known task entry points
- Run the decompiler on these newly defined functions
- Apply all the FreeRTOS API names we've identified (19 functions)
- Apply the variable names we've mapped (framebuffer, viewport, etc.)
- **Goal output:** Updated `decompiled_2C53T.c` with 500+ named functions

**Why second:** The decompiled code is our primary reference. Having the actual task code decompiled (especially `display`, `osc`, and `fpga` tasks) is essential for everything that follows.

### Step 3: Set Up ARM Cross-Compilation Toolchain

**Goal:** Be able to compile C code for the GD32F307 and produce a flashable binary.

**What to do:**
- Install `arm-none-eabi-gcc` toolchain (`brew install --cask gcc-arm-embedded`)
- Download the GD32F30x firmware library from GigaDevice (HAL drivers, CMSIS headers, linker scripts)
- Create a minimal project structure:
  ```
  firmware/
  ├── src/
  │   ├── main.c           # Entry point
  │   ├── system_gd32f30x.c # System init
  │   └── startup.s        # Startup assembly
  ├── include/
  │   └── gd32f30x.h       # Register definitions
  ├── ld/
  │   └── gd32f307.ld      # Linker script (flash at 0x08007000, RAM at 0x20000000)
  ├── FreeRTOS/             # FreeRTOS kernel source
  └── Makefile
  ```
- Write a minimal `main.c` that just blinks or writes to a GPIO
- Compile and verify it produces a valid ARM binary
- Test it in Renode to confirm it boots
- **Do NOT flash to real device yet** — just verify it compiles and runs in emulator

**Why third:** Having the toolchain ready means the moment we understand a peripheral (from RE or from the real device), we can immediately write and test code for it.

### Step 4: Build the LCD Driver (From RE Data)

**Goal:** Write a standalone LCD initialization and drawing library in C.

**What to do:**
- From the decompiled code, extract the LCD initialization sequence (EXMC/FSMC setup, LCD controller init commands)
- Identify the LCD controller from the init sequence (ILI9341, ST7789, or similar)
- Write `lcd.c` / `lcd.h` with:
  - `lcd_init()` — configure EXMC, send init commands
  - `lcd_set_pixel(x, y, color)` — write RGB565 pixel
  - `lcd_fill_rect(x, y, w, h, color)` — fill rectangle
  - `lcd_draw_char()` / `lcd_draw_string()` — text rendering
- Test in Renode by hooking the EXMC memory writes → capture framebuffer → display in React frontend
- **This is where the emulator and real firmware converge** — same LCD code runs in both

**Why fourth:** The display is the most visible output. Getting "Hello World" on the LCD (even just in the emulator) is a huge motivational milestone.

---

## Phase 2: With Device (When It Arrives)

### Step 5: Hardware Teardown and Full Flash Dump

**Goal:** Get the complete flash contents and verify our chip identification.

**What to do:**
- Open the device (usually 4-6 screws on the back)
- Photograph the PCB, both sides — document every IC
- Read chip markings (even if sanded, sometimes readable at angle/under UV)
- Confirm GD32F307 (or identify the actual MCU if different)
- Identify the FPGA, ADC, LCD controller, flash chip, DVOM chip
- Connect via USB Sharing mode, copy everything from the internal filesystem
- If possible, find SWD test points on the PCB:
  - Connect a J-Link or ST-Link
  - Dump the full 1MB flash (this gets us the persistent string tables at 0x080BC000+ and the bootloader at 0x08000000-0x08007000)
  - Dump the external SPI flash (the system file filesystem)
- **Output:** Complete chip manifest, PCB photos, full flash dump, filesystem contents

**Why fifth:** This is the first thing to do when the device arrives. Everything we assumed from firmware analysis gets confirmed or corrected.

### Step 6: Capture Real FPGA Protocol Traffic

**Goal:** Record the actual USART2 and SPI2 communication between MCU and FPGA.

**What to do:**
- Connect a logic analyzer (or a second scope!) to the USART2 TX/RX lines on the PCB
- Power on the device and capture the entire boot sequence
- Record traffic during:
  - Boot/initialization
  - Changing timebase
  - Changing trigger settings
  - Switching between oscilloscope/multimeter/signal generator modes
  - Capturing a signal
- Decode the USART frames using our known protocol format (10-byte frames, 0x5A/0xA5 ACK, 0xAA/0x55 data)
- Also capture SPI2 traffic during acquisition to understand sample data format
- Cross-reference with the decompiled fpga task code
- **Output:** Complete FPGA command reference document, similar to pecostm32's `FPGA explained.txt`

**Why sixth:** This is the Rosetta Stone. Once we know every FPGA command and its effect, we can write firmware that fully controls the analog frontend.

### Step 7: Flash a Minimal Custom Firmware

**Goal:** Prove we can write code that runs on the real device.

**What to do:**
- **FIRST: Make a complete backup** of the device's flash via SWD
- Write a minimal firmware that:
  - Initializes the clock system (copy from decompiled startup code)
  - Initializes the LCD (using the driver from Step 4)
  - Displays "OpenScope 2C53T" on the screen
  - Blinks an LED or toggles a GPIO (if there's a visible one)
- Flash it via the firmware update mechanism (MENU + Power, copy to USB drive)
- Verify it boots and displays the text
- Flash back to stock firmware to confirm the update mechanism is fully reversible
- **Output:** Confirmed ability to write and deploy custom firmware

**Why seventh:** This is the "we can actually do this" milestone. Everything before this is analysis; this is creation.

---

## Phase 3: Building Features

### Step 8: Implement Core Scope Functionality

**Goal:** Custom firmware that captures and displays waveforms.

**What to do:**
- Port the FPGA command interface using the protocol captured in Step 6
- Implement basic waveform capture: configure timebase → send FPGA command → read samples via SPI2 → display on LCD
- Implement trigger: level, edge, auto/normal/single modes
- Implement basic UI: timebase/volts-per-div adjustment with buttons
- Use FreeRTOS with the same task structure (display, osc, key, fpga)
- **Success criteria:** capture and display a real signal from the scope probes

### Step 9: First Protocol Decoder (UART)

**Goal:** Prove the protocol decode overlay concept.

**What to do:**
- Implement UART decoder: auto-detect baud rate, detect start/stop bits, extract bytes
- Draw decoded bytes as text overlay on the waveform (e.g., `0x48 'H'` above the corresponding bits)
- Add a "Protocol" menu option to select the decoder
- Store decoder settings in the existing configuration system
- **This validates the architecture** for all future protocol decoders

### Step 10: USB Streaming Mode

**Goal:** Stream sample data to PC for use with sigrok/PulseView.

**What to do:**
- Add USB CDC (virtual serial port) mode alongside existing USB mass storage
- When activated: stream raw samples from FPGA through USB to PC
- Write a basic sigrok hardware driver (`libsigrok`) for the 2C53T
- Test with PulseView — live waveform display on PC with full protocol decode library
- **This turns the $70 device into a PC-connected instrument**

---

## Decision Points

At each phase boundary, evaluate:

1. **After Phase 1:** Is the RE data sufficient, or do we need to wait for hardware? (Likely: proceed with what we have, hardware fills in gaps)

2. **After Step 7:** Go public with the repo? At this point you'd have confirmed the firmware update works, the device is hackable, and you have a working custom firmware. This is when the project becomes interesting to others.

3. **After Step 9:** Release the first "useful" firmware build? A scope with UART decode would already be more capable than the stock firmware.

4. **CAN bus timing:** If the GD32F307's CAN pins are accessible (discovered in Step 5), the automotive modules become much easier. If not, CAN decode from the analog waveform is still feasible but harder.

## Estimated Effort

| Step | Without device | With device | Effort |
|---|---|---|---|
| 1. Renode boot | Yes | — | 1-2 sessions |
| 2. Full decompilation | Yes | — | 1 session |
| 3. Toolchain setup | Yes | — | 1 session |
| 4. LCD driver | Yes (test in emulator) | — | 1-2 sessions |
| 5. Teardown + dump | — | Yes | 1 session |
| 6. FPGA protocol capture | — | Yes | 1-2 sessions |
| 7. First custom flash | — | Yes | 1 session |
| 8. Core scope | — | Yes | 3-5 sessions |
| 9. UART decoder | — | Yes (or emulator) | 1-2 sessions |
| 10. USB streaming | — | Yes | 2-3 sessions |
