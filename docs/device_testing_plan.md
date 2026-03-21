# Device Testing Plan

What to do when the FNIRSI 2C53T arrives, step by step.

## What to Buy

### SWD Debug Probe (Required)

An **ST-Link V2 clone** is the cheapest and most reliable option for SWD debugging GD32/STM32 chips.

**Recommended options (any of these will work):**

| Option | Price | Notes |
|---|---|---|
| [WWZMDiB ST-Link V2](https://www.amazon.com/WWZMDiB-Programming-Downloader-resettable-Protection/dp/B0BDDWRK4C) | ~$8 | Simple USB dongle, includes ribbon cable |
| [Mini ST-LINK/V2](https://www.amazon.com/ST-LINK-circuit-debugger-programmer-XYGStudy/dp/B00PVJ8Q4C) | ~$10 | Compact form factor |
| [Waveshare ST-Link/V2](https://www.amazon.com/Waveshare-ST-LINK-V2-STMICROELECTRONICS-rogrammer/dp/B00MXXWQKY) | ~$15 | More established brand |

Any ST-Link V2 clone with SWD support will work. They all use the same protocol. You **cannot brick the scope** with an ST-Link — it's read-only unless you explicitly write.

### Also Useful

| Item | Price | Purpose |
|---|---|---|
| Dupont jumper wires (female-female) | ~$5 | Connect ST-Link to SWD pads |
| Fine-tip soldering iron + solder | — | If you want to solder header pins to the SWD pads |
| Pogo pin test clips | ~$8 | Connect to SWD pads without soldering |
| USB logic analyzer (8ch) | ~$10 | Capture FPGA UART/SPI traffic |

## Software to Install

```bash
# OpenOCD - talks to the ST-Link
brew install openocd

# Optional: STM32CubeProgrammer (free from ST) - GUI for flash read/write
# Download from: https://www.st.com/en/development-tools/stm32cubeprog.html
```

## Step-by-Step Plan

### Day 1: Initial Testing (Don't Open It Yet)

1. **Power on, basic functionality test**
   - Verify oscilloscope, multimeter, and signal generator all work
   - Take screenshots of each mode (SAVE button saves BMP to internal storage)
   - Note the firmware version (MENU → About)

2. **USB filesystem dump**
   - Connect via USB, enter USB Sharing mode (MENU → USB Sharing)
   - Copy EVERYTHING from the USB drive to your computer:
     ```bash
     mkdir -p ~/Desktop/osc/device_dump/usb_sharing
     cp -r /Volumes/FNIRSI/* ~/Desktop/osc/device_dump/usb_sharing/
     ```
   - This gets us the internal filesystem: screenshots, logo, possibly config files

3. **Test firmware update mechanism (with stock firmware)**
   - Download a different firmware version from FNIRSI website
   - Flash it using the standard process (MENU + Power → USB → copy .bin)
   - Verify it works
   - Flash back to V1.2.0
   - This confirms the update mechanism is safe and reversible

### Day 2: Teardown

4. **Open the device**
   - Remove back screws (likely 4-6 Phillips screws)
   - Carefully separate the case halves
   - **Do NOT disconnect any ribbon cables yet** — just open enough to see the PCB

5. **Photograph everything**
   - Take clear photos of both sides of the PCB
   - Get close-ups of every IC (even if markings are sanded)
   - Photograph the SWD pads (labeled SWCLK, GND, SWDIO, 3V3)
   - Look for any other test points or unpopulated headers
   - Save photos to `~/Desktop/osc/device_dump/pcb_photos/`

6. **Identify chips** (from markings or package type)
   - MCU (large QFP — should be GD32F307)
   - FPGA (another large IC)
   - ADC (likely near the BNC connectors)
   - LCD controller (on the display flex cable or a chip near the display connector)
   - SPI flash (small 8-pin SOIC)
   - DVOM chip (near the banana jack area)
   - Signal relays (HFD4/3-LS — already identified)

### Day 3: SWD Connection and Flash Dump

7. **Connect ST-Link to SWD pads**

   Wire the ST-Link to the scope's SWD pads:
   ```
   ST-Link          Scope PCB
   ────────         ─────────
   SWCLK    ───→    SWCLK
   SWDIO    ───→    SWDIO
   GND      ───→    GND
   3.3V     ───→    3V3 (optional — scope has its own power)
   ```

   Options for connecting:
   - **Solder header pins** to the pads (most reliable)
   - **Pogo pins** or **test clips** (no soldering, good for one-time dump)
   - **Hold wires against pads** with tape (unreliable but works in a pinch)

   **Important:** The scope should be powered ON via its own battery/USB when connecting. The ST-Link's 3.3V can optionally power the MCU, but the scope has other components that need their own power.

8. **Verify SWD connection**
   ```bash
   # Test that OpenOCD can see the chip
   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
     -c "init; echo \"Connected!\"; exit"
   ```

   If it says "Connected!" — you're in. If it fails, try:
   - `stm32f3x.cfg` instead (GD32F307 is Cortex-M4 like F3)
   - Check wiring
   - Make sure the scope is powered on

9. **Dump the full flash (1MB)**
   ```bash
   mkdir -p ~/Desktop/osc/device_dump/flash

   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "
     init
     halt
     flash banks
     dump_image ~/Desktop/osc/device_dump/flash/full_flash_1MB.bin 0x08000000 0x100000
     echo \"Flash dump complete!\"
     resume
     exit
   "
   ```

   This dumps the entire 1MB flash including:
   - Bootloader (0x08000000 - 0x08007000)
   - Main firmware (0x08007000 - 0x080B7680)
   - Persistent string tables (0x080BC000+)
   - Calibration data
   - Any other data regions

10. **Dump RAM snapshot (optional but useful)**
    ```bash
    openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "
      init
      halt
      dump_image ~/Desktop/osc/device_dump/flash/ram_snapshot.bin 0x20000000 0x40000
      resume
      exit
    "
    ```

11. **Read the chip ID**
    ```bash
    openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "
      init
      halt
      mdw 0xE0042000 1
      mdw 0x1FFFF7E0 4
      resume
      exit
    "
    ```
    - `0xE0042000` = MCU device ID register
    - `0x1FFFF7E0` = Unique chip ID (serial number) + flash size

### Day 4: FPGA Protocol Capture

12. **Connect logic analyzer to USART2 lines**
    - Find the USART2 TX/RX traces on the PCB (between MCU and DVOM/FPGA chip)
    - Connect logic analyzer channels to TX, RX, and GND
    - May need to follow PCB traces from the MCU's USART2 pins

13. **Capture boot sequence**
    ```bash
    # Using sigrok-cli with a cheap USB logic analyzer
    sigrok-cli --driver fx2lafw --config samplerate=1000000 \
      --channels D0=TX,D1=RX --time 10s -o boot_capture.sr
    ```
    - Power cycle the scope while capturing
    - Open in PulseView, add UART decoder

14. **Capture during operation**
    - Capture while changing timebase
    - Capture while switching oscilloscope → multimeter → signal generator
    - Capture during signal acquisition
    - Save all captures for analysis

### Day 5: First Custom Firmware

15. **Backup verification**
    - Compare the flash dump with the firmware update .bin file
    - Verify the dump contains valid ARM code
    - Check that the bootloader region (0x00-0x7000) makes sense
    - Extract the persistent string tables from the dump

16. **Flash custom firmware (THE BIG MOMENT)**

    **Option A: Via USB update (safer)**
    - Copy our `firmware/build/firmware.bin` to the USB drive in update mode
    - This only replaces the firmware region, leaving bootloader and calibration intact

    **Option B: Via SWD (more control)**
    ```bash
    openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "
      init
      halt
      flash write_image erase firmware/build/firmware.bin 0x08007000
      reset
      exit
    "
    ```
    - Note: flash at 0x08007000 (after bootloader), NOT 0x08000000

17. **Verify custom firmware runs**
    - The display should show "OpenScope 2C53T" (or whatever our test firmware does)
    - If nothing happens, connect via SWD and check PC register

18. **Flash back to stock**
    ```bash
    # Restore from our full flash dump
    openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "
      init
      halt
      flash write_image erase ~/Desktop/osc/device_dump/flash/full_flash_1MB.bin 0x08000000
      reset
      exit
    "
    ```
    - Or use the USB update mechanism with the stock .bin file

## Safety Notes

- **You CANNOT brick the device via SWD** — SWD always works even if the firmware is garbage. You can always reflash.
- **The USB firmware update is also safe** — the bootloader (first 28KB) is separate and isn't overwritten by firmware updates.
- **Always dump the flash BEFORE writing anything** — this is your insurance policy.
- **Calibration data** is stored somewhere in flash (likely near the end). The flash dump preserves it. Don't erase the entire flash without restoring from the dump.
- **The battery** will keep the device powered during testing. Keep it charged.

## Checklist

```
Before opening:
[ ] Verify all features work
[ ] Dump USB filesystem
[ ] Test firmware update with stock firmware
[ ] Note firmware version

After opening:
[ ] Photograph PCB (both sides, all ICs, SWD pads)
[ ] Identify all chips
[ ] Connect ST-Link to SWD
[ ] Verify SWD connection with OpenOCD
[ ] Dump full 1MB flash
[ ] Read chip ID register
[ ] Save everything to device_dump/

Protocol capture:
[ ] Connect logic analyzer to USART2
[ ] Capture boot sequence
[ ] Capture mode changes
[ ] Capture signal acquisition

Custom firmware:
[ ] Verify flash dump integrity
[ ] Flash test firmware
[ ] Verify it runs
[ ] Flash back to stock
[ ] Celebrate!
```
