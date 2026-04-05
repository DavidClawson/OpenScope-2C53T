# Entering DFU Mode (First-Time Firmware Flash)

The first time you flash custom firmware, you need to enter the AT32's **ROM DFU mode** by pulling the BOOT0 pin high while resetting the MCU. After the initial flash installs the USB HID bootloader, **you'll never need to do this again** — all future updates go over USB-C with the case closed.

> **Two bootloaders, don't mix them up.** This device has two completely separate bootloaders:
>
> - **ROM DFU** — baked into AT32 silicon, entered only via BOOT0 + pinhole reset. LCD stays dark. Enumerates as `2e3c:df11`. **This is the only mode that can write option bytes** (needed once to set EOPB0 for 224KB SRAM).
> - **USB HID bootloader** — our custom bootloader at `0x08000000`, entered from **Settings → Firmware Update**. Shows "BOOTLOADER MODE" on the LCD. Used by `make flash` for all normal updates. **Cannot write option bytes.**
>
> If `dfu-util -l` shows "No DFU capable USB device available" while the scope is sitting on the "BOOTLOADER MODE" screen, that's why — you're in the wrong mode for what `dfu-util` expects.

## What You Need

- Phillips screwdriver (to open the case)
- A short DuPont jumper wire (female-to-female or bare ends)
- USB-C cable connected to your computer
- A pin, plastic spudger, or small screwdriver (to press the pinhole reset button)

## Locating the Test Points

Open the case by removing the 6 Phillips screws on the back.

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

Run `cd firmware && make` first — this generates `build/firmware.bin`, `build/bootloader.bin`, and the 48-byte `build/option_bytes48.bin` blob used in step 1 below.

Once in ROM DFU mode (confirmed by `dfu-util -l` showing `2e3c:df11` with alt interfaces 0 and 1):

```bash
cd firmware

# 1. Set EOPB0 = 0xFE → 224KB SRAM mode (one-time, AT32 defaults to 96KB)
dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D build/option_bytes48.bin

# 2. Pinhole reset to stay in DFU, then flash bootloader + application
make flash-all
```

Harmless warnings you can ignore during step 1:
- `Invalid DFU suffix signature` — our blob doesn't carry a CRC trailer; dfu-util just notes it.
- `Error sending dfu abort request` — AT32 ROM resets before acknowledging the session teardown.

The line that matters is `Download done. / File downloaded successfully`.

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

## Alternative Flashing Methods

The `dfu-util` + `make flash-all` path above is what I use and test on macOS. Users on other platforms have reported success with the alternatives below. I haven't personally verified these — they're documented here because they worked for real users, and they may be easier than fighting `dfu-util` on Windows or getting udev rules right on Linux.

### Windows: Artery ISP Programmer (community-contributed)

Artery ships a Windows GUI flasher that speaks the same ROM DFU protocol as `dfu-util`. A user on Windows 11 reported (see [#4](https://github.com/DavidClawson/OpenScope-2C53T/issues/4)) that after fighting WinUSB/`dfu-util` setup they flashed successfully using Artery's tool instead.

**Rough flow** (if you try this and it works, please post the exact steps on #4 so I can tighten this up):

1. Build the firmware on Windows (MSYS2/WSL) so you have `firmware/build/firmware.bin`, `firmware/build/bootloader.bin`, and `firmware/build/option_bytes48.bin`.
2. Put the device in **ROM DFU mode** using the BOOT0 + pinhole-reset procedure above. You're still entering the same mode — only the host-side tool changes.
3. Download **Artery ISP Programmer** from the ArteryTek website (search for "AT32 ISP Programmer").
4. In the tool, select the USB DFU interface, load each `.bin` at its flash address, and program. You'll need to program the option byte region (`0x1FFFF800`) from `option_bytes48.bin` as well — this is what `dfu-util -a 1 ... -s 0x1FFFF800` does in the command-line flow.
5. After the first successful flash, close the case. All future updates still go through `make flash` over USB-C via the HID bootloader — you're only using Artery ISP for the one-time setup.

If you hit this and it works, **please post your exact steps on [#4](https://github.com/DavidClawson/OpenScope-2C53T/issues/4)** — it'll help the next Windows user and let me turn this stub into a proper walkthrough.

### Linux: From-Scratch Recipe

Consolidated from [#2](https://github.com/DavidClawson/OpenScope-2C53T/issues/2), [#3](https://github.com/DavidClawson/OpenScope-2C53T/issues/3), and [#4](https://github.com/DavidClawson/OpenScope-2C53T/issues/4). I develop on macOS, so this path is community-tested rather than something I run on every release.

```bash
# 1. Install toolchain + DFU tool
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi dfu-util make git

# 2. Clone the repo and the two external libraries it depends on
git clone https://github.com/DavidClawson/OpenScope-2C53T.git
cd OpenScope-2C53T/firmware
git clone https://github.com/ArteryTek/AT32F403A_407_Firmware_Library.git at32f403a_lib
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS

# 3. Build — this is what generates build/option_bytes48.bin (48 bytes)
make
ls -l build/option_bytes48.bin   # sanity check: should exist and be 48 bytes

# 4. Enter ROM DFU mode (open case, BOOT0 jumper, pinhole reset — see above).
#    Verify:
dfu-util -l
# Expect: Found DFU: [2e3c:df11] ... alt=0 ... alt=1

# 5. One-time option-byte write (sets EOPB0 = 0xFE → 224KB SRAM mode)
dfu-util -a 1 -d 2e3c:df11 -s 0x1FFFF800 -D build/option_bytes48.bin
# Ignore: "Invalid DFU suffix signature" and "Error sending dfu abort request" — cosmetic.
# Look for:  "Download done." and "File downloaded successfully"

# 6. Pinhole reset (stays in DFU), then flash bootloader + app
make flash-all

# 7. Remove jumper, pinhole reset, close case. Done.
#    All future updates: Settings → Firmware Update on device, then `make flash` on host.
```

**Linux gotchas:**
- **Permission denied on `dfu-util -l`:** add a udev rule or run with `sudo` once to confirm it works. Minimal udev rule (save as `/etc/udev/rules.d/40-at32-dfu.rules`, then `sudo udevadm control --reload-rules`):
  ```
  SUBSYSTEM=="usb", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="df11", MODE="0666"
  ```
- **Build succeeds but `option_bytes48.bin` is missing:** you're on an old commit. This file is generated by a Makefile rule added [here](https://github.com/DavidClawson/OpenScope-2C53T/commit/e588e80) — `git pull` and rebuild.
- **ModemManager grabs the USB device:** on some distros, temporarily stop it with `sudo systemctl stop ModemManager` before flashing.

## Troubleshooting

- **Device doesn't enumerate as DFU:** Make sure you're touching the correct side of the resistor (MCU side, not ground side). Try again — the timing can be tricky.
- **`dfu-util` not found:** Install with `brew install dfu-util` (macOS) or `apt install dfu-util` (Linux).
- **Permission denied:** On Linux, you may need udev rules for the AT32 DFU device. Try running with `sudo` first.
- **Bricked after a bad flash:** You can always re-enter DFU mode with the BOOT0 procedure above. The ROM bootloader is permanent and cannot be overwritten.
