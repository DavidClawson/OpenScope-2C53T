# Entering DFU Mode (First-Time Firmware Flash)

The first time you flash custom firmware, you need to enter DFU (Device Firmware Upgrade) mode by pulling the BOOT0 pin high while resetting the MCU. After the initial flash installs the USB HID bootloader, **you'll never need to do this again** — all future updates go over USB-C with the case closed.

## What You Need

- Phillips screwdriver (to open the case)
- A short DuPont jumper wire (female-to-female or bare ends)
- USB-C cable connected to your computer
- A pin, plastic spudger, or small screwdriver (to press the pinhole reset button)

## Locating the Test Points

Open the case by removing the 4 Phillips screws on the back.

### 3.3V Source

The 3.3V source is on the SWD debug header, located next to the USB-C port. There are 5 labeled through-hole pads: **3V3**, **SWDIO**, **GND**, **SWCLK**, and one unlabeled. You may need to solder a pin header into the 3V3 pad for a reliable connection, or carefully hold a jumper wire against it.

<p align="center">
  <img src="images/3v3_source.jpeg" alt="3.3V source on SWD header near USB-C port" width="400">
</p>

### BOOT0 Pin

The BOOT0 pin is accessible on the MCU side of a pull-down resistor near the bottom edge of the main IC (AT32F403A). The resistor holds BOOT0 low during normal boot. You need to touch the **MCU-side pad** of this resistor — the side closest to the microcontroller.

<p align="center">
  <img src="images/boot0_resistor.jpg" alt="BOOT0 resistor location near MCU" width="400">
</p>

## Battery vs. USB Power

> **Note (unconfirmed):** You can likely perform this entire procedure **without the battery connected**. Unplug the JST battery connector, then plug in USB-C — the USB charge circuit appears to power the board independently. This gives you more room to work inside the case and avoids any risk to the battery.
>
> Evidence: with USB plugged in, the device stays powered even when the power kill switch is pressed — USB holds the rails up on its own. The ROM DFU bootloader is baked into the AT32 silicon and doesn't depend on any user firmware, so PC9 (power hold) shouldn't matter.
>
> If you can confirm this works, please let us know in [issue #1](https://github.com/DavidClawson/OpenScope-2C53T/issues/1)!

## Step-by-Step Procedure

1. **Open the case** and connect USB-C to your computer (see note above — you may be able to disconnect the battery first).

2. **Prepare the jumper wire.** Take a DuPont wire and connect one end to the **3V3 pad** on the SWD header (you may need to solder a pin into the through-hole for a solid connection).

3. **Touch the other end to BOOT0.** Carefully touch the free end of the jumper wire to the resistor pad on the MCU side (see the red arrow in the photo above). This pulls BOOT0 high (3.3V).

4. **While holding 3.3V on BOOT0, press and hold the reset button.** The pinhole reset button is accessible from the outside of the case, or you can press the NRST tactile switch on the PCB directly.

5. **Release the reset button, then release the 3.3V jumper.** The order matters — release reset first so the MCU samples BOOT0 = HIGH during startup.

6. **Verify DFU mode.** The device should enumerate as a USB DFU device:
   ```bash
   dfu-util -l
   # Should show: "AT32 Bootloader DFU" with ID 2e3c:df11
   ```

## First-Time Flash Commands

Once in DFU mode, run these commands:

```bash
# 1. Set EOPB0 for 224KB SRAM (one-time — the AT32 defaults to only 96KB)
dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D firmware/build/option_bytes48.bin

# 2. Flash the bootloader + application
cd firmware && make flash-all
```

After this, close the case. All future updates use the USB HID bootloader:
1. On the device: **Settings > Firmware Update**
2. On your computer: `cd firmware && make flash`

## Verifying DFU Enumeration

Once you've successfully entered DFU mode, the device shows up as a USB DFU device. Here's how to verify on each platform:

**macOS:**
```bash
# Check with dfu-util
dfu-util -l
# Expected output includes:
#   Found DFU: [2e3c:df11] ver=0200, devnum=XX, cfg=1, intf=0, path="X-X", alt=0, name="@Internal Flash  /0x08000000/0512*002Kg"

# Or check System Information → USB
# You should see "AT32 Bootloader DFU" listed
```

**Linux:**
```bash
dfu-util -l
# Same output as above

# Or check with lsusb:
lsusb | grep 2e3c
# Expected: Bus XXX Device XXX: ID 2e3c:df11
```

**Windows:**
- Open Device Manager — look for "AT32 Bootloader DFU" under USB devices
- You may need to install the WinUSB driver via [Zadig](https://zadig.akeo.ie/) for `dfu-util` to work

If you don't see the device, the BOOT0 jump didn't take. Try the procedure again — the timing can be finicky.

## Troubleshooting

- **Device doesn't enumerate as DFU:** Make sure you're touching the correct side of the resistor (MCU side, not ground side). Try again — the timing can be tricky.
- **`dfu-util` not found:** Install with `brew install dfu-util` (macOS) or `apt install dfu-util` (Linux).
- **Permission denied:** On Linux, you may need udev rules for the AT32 DFU device. Try running with `sudo` first.
- **Bricked after a bad flash:** You can always re-enter DFU mode with the BOOT0 procedure above. The ROM bootloader is permanent and cannot be overwritten.
