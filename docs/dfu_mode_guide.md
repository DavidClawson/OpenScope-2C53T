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

If the bootloader flash succeeds but the device afterwards comes up on the
`BOOTLOADER MODE` screen instead of the application, that still means the
one-time ROM DFU work succeeded. The application write is the second step of
`make flash-all` and ends by asking the ROM to leave DFU and jump to the app;
lines like `leave status=... state=...` or `device detached after leave: ...`
are the normal output of that step, not errors. Remove the BOOT0 jumper, reset
the device, and flash the application through the bootloader:

```bash
cd firmware
make flash
```

Only close the case after the application, not just the bootloader screen, has
booted. All future updates use the USB HID bootloader:
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

### Stock USB update channel — no case opening (macOS / Linux / Windows)

Separate from the ROM DFU path above: the device's **stock bootloader** also accepts firmware over USB-C with the case closed. Hold **MENU + tap Power** to enter upgrade mode (LCD shows "firmware upgrade") — the device mounts a FAT12 drive named `IAP`. This is the channel for restoring the original FNIRSI firmware, or flashing an image without the HID bootloader.

- **Windows:** drag-drop the `.bin` onto the `IAP` drive — this is the official FNIRSI update method and Windows' FAT driver handles the volume cleanly.
- **macOS:** do **not** drag-drop in Finder — macOS corrupts the write (the volume uses 2048-byte sectors and Finder adds AppleDouble `._` junk the bootloader misreads as firmware). Use the bundled flasher: `brew install mtools && python3 scripts/iap_flash.py` (auto-detects the device and images, SHA-verifies stock, shows progress). `python3 scripts/iap_flash.py guide` prints the full walkthrough.
- **Linux:** `scripts/iap_flash.py` works here too (or `mcopy` the `.bin` to the device, then `sync; sudo blockdev --flushbufs`).

A bad flash is never a brick — re-enter upgrade mode and reflash any image.

### Full factory restore (recover MENU+Power upgrade mode)

The stock device has **three** firmware layers, and which ones you have determines whether the MENU+Power upgrade disk works:

| Address | Layer | Notes |
|---------|-------|-------|
| (MCU silicon) | ROM DFU bootloader | Mask ROM, unerasable — BOOT0 + reset always reaches it |
| `0x08000000` | **Factory IAP bootloader** (28 KB) | Implements the MENU+Power `IAP` upgrade disk. **Not part of any `.bin` you can download** — it only ships pre-installed |
| `0x08007000` | Stock application | The `APP_2C53T_V*.bin` you can download from FNIRSI |

If you flashed an alternative bootloader (e.g. an older OpenScope HID bootloader, or a community switcher branch) directly to `0x08000000`, you **overwrote the factory IAP bootloader** and MENU+Power no longer mounts the `IAP` disk. Flashing the stock app to both `0x08000000` and `0x08007000` will boot, but it does **not** restore the upgrade disk — those are app fragments, not the IAP bootloader.

To fully restore factory state you need the factory IAP bootloader image. We've archived a dump from a V1.4 unit at [`archive/factory_iap_bootloader_2C53T.bin`](../archive/factory_iap_bootloader_2C53T.bin) (28,672 bytes, sha256 `0c9ec7d6…`). Restore over ROM DFU (open case, BOOT0 + pinhole reset — see top of this guide):

```bash
# Verify ROM DFU enumerated (2e3c:df11, alt 0 = Internal Flash)
dfu-util -l

# 1. Factory IAP bootloader → 0x08000000 (restores MENU+Power upgrade disk)
dfu-util -a 0 -d 2e3c:df11 -s 0x08000000 -D archive/factory_iap_bootloader_2C53T.bin

# 2. Stock application → 0x08007000
dfu-util -a 0 -d 2e3c:df11 -s 0x08007000 \
  -D "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"

# Remove BOOT0 jumper, pinhole reset → boots bone-stock; MENU+Power mounts IAP again.
```

This was tested end-to-end on a unit whose factory bootloader had been erased months earlier: it boots stock V1.2.0, shows a live trace, and MENU+Power restores the `IAP` upgrade disk. If the device drops off DFU mid-write, **hold the POWER button during the flash** (PC9 isn't asserted in ROM DFU on some units) and retry.

> The factory IAP bootloader is FNIRSI's proprietary code, archived here solely for device recovery. If FNIRSI requests removal, we'll comply.

### Windows: Artery ISP Programmer (community-contributed)

Artery ships a Windows GUI flasher that speaks the same ROM DFU protocol as `dfu-util`, and it handles the option bytes through a menu rather than a binary blob — which makes it the easier route on Windows than fighting WinUSB and `dfu-util`.

The walkthrough below comes from **[@baraa1936 in #20](https://github.com/DavidClawson/OpenScope-2C53T/issues/20)**, who did this on real hardware and documented every dialog with screenshots. **[Go there for the screenshots](https://github.com/DavidClawson/OpenScope-2C53T/issues/20)** — they make each step unambiguous in a way prose can't. Earlier reports of the same approach are in [#4](https://github.com/DavidClawson/OpenScope-2C53T/issues/4).

> **Not verified by the maintainer** — I don't have a Windows machine. This worked for the contributor who wrote it. If a step is wrong or a dialog has changed, please say so on [#20](https://github.com/DavidClawson/OpenScope-2C53T/issues/20).

**Before you start:** build the firmware (MSYS2 or WSL) so you have `firmware/build/bootloader.bin` and `firmware/build/firmware.bin`, and put the device in **ROM DFU mode** using the BOOT0 + pinhole-reset procedure above. Install the DFU driver if Windows hasn't already.

1. Download **[Artery ISP Programmer](https://www.arterychip.com/file/download/1764)** and run it.
2. Set **Language → English**, and confirm **Port Type** is **USB DFU**. Click through **Next**.
3. Choose **Edit User system data**, then **Next**.
4. Set **EOPB0** to **224KB SRAM**, then **Apply to device**.
   This is the one-time option-byte write — the same thing `dfu-util -a 1 -s 0x1FFFF800` does in the command-line flow. Without it the MCU comes up in 96 KB SRAM mode and the firmware won't run.
5. **The device will disconnect.** Re-enter ROM DFU mode (BOOT0 + pinhole reset), then click **Back** in the tool.
6. Switch the mode selector from **Edit User system data** to **Download to device**, and add both images at their correct addresses:

   | File | Address |
   |---|---|
   | `bootloader.bin` | `0x08000000` |
   | `firmware.bin` | `0x08004000` |

7. **Next**, then **OK**, and wait for it to finish.
8. **Expected at this point: the device shows the bootloader screen and will *not* boot the application.** This is correct behaviour, not a failed flash — see below.
9. Finish with `make flash` from a machine with the toolchain. The contributor used a Raspberry Pi; WSL or MSYS2 should also work, though neither has been confirmed for this final step.

**Why step 8 happens.** Our bootloader only jumps to the application after the app has been *validated* — it checks a boot-handshake flag before handing over. Writing `firmware.bin` directly with Artery ISP puts the bytes in flash but never sets that flag, so the bootloader correctly refuses to jump and stays on its own screen. `make flash` goes through the HID update path, which sets the flag and blesses the app. That last step is doing the validation, not re-flashing for nothing.

After this one-time setup, close the case — all future updates go through **Settings → Firmware Update** on the device followed by `make flash` on the host.

*Thanks to [@baraa1936](https://github.com/baraa1936) for writing this up and testing it on hardware.*

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
- **Permission denied on `dfu-util`, `make flash`, or the USB debug shell:**
  install the repo's udev rule or run with `sudo` once to confirm it is only a
  permission problem:
  ```bash
  sudo cp udev/60-openscope-2c53t.rules /etc/udev/rules.d/
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  ```
  Then reconnect the device or enter the bootloader again. The rule covers all
  three OpenScope USB interfaces:
  ```
  # OpenScope/FNIRSI 2C53T Artery AT32 bootloader and debug interfaces
  SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="af01", MODE="0660", GROUP="plugdev", TAG+="uaccess"
  SUBSYSTEM=="usb", ATTR{idVendor}=="2e3c", ATTR{idProduct}=="df11", MODE="0666", GROUP="plugdev", TAG+="uaccess"
  SUBSYSTEM=="tty", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="5740", MODE="0660", GROUP="dialout", TAG+="uaccess"
  ```
  `af01` is the custom HID bootloader used by `make flash`, `df11` is the ROM
  DFU bootloader, and `5740` is the AT32 virtual COM port used by the debug
  shell.
- **Build succeeds but `option_bytes48.bin` is missing:** you're on an old commit. This file is generated by a Makefile rule added [here](https://github.com/DavidClawson/OpenScope-2C53T/commit/e588e80) — `git pull` and rebuild.
- **ModemManager grabs the USB device:** on some distros, temporarily stop it with `sudo systemctl stop ModemManager` before flashing.

## Troubleshooting

- **Device doesn't enumerate as DFU:** Make sure you're touching the correct side of the resistor (MCU side, not ground side). Try again — the timing can be tricky.
- **`dfu-util` not found:** Install with `brew install dfu-util` (macOS) or `apt install dfu-util` (Linux).
- **Permission denied:** On Linux, install the udev rule above for the ROM DFU,
  HID bootloader, and CDC debug-shell interfaces. Try running with `sudo` once
  to confirm it is only a permission problem.
- **`option_bytes48.bin` is not 48 bytes:** the file must be exactly 48 bytes or
  the ROM DFU rejects the write with "address out of range". Confirm with
  `wc -c firmware/build/option_bytes48.bin`. This used to go wrong on Linux,
  where `/bin/sh` is often `dash` and its `printf` does not understand `\xHH`
  escapes — the blob came out 192 bytes. The Makefile now uses octal escapes,
  which are portable, so a fresh build should be correct on any shell.
- **`SET_ADDRESS not correctly executed` for option bytes or internal flash:**
  the ROM DFU can be stuck in a protected/error state. No external programmer is
  required for this recovery path. With the device in ROM DFU, run:
  ```bash
  dfu-util -a 0 -d 2e3c:df11 -s :unprotect:force:will-reset -D /dev/null
  ```
  It is normal for `dfu-util` to complain about the final state or for the
  device to disappear from USB. Keep BOOT0 high, press pinhole reset or
  unplug/replug USB, verify `dfu-util -l` again, then retry the option-byte and
  flash commands.

  **This erases the whole internal flash — use it only when nothing else works.**
  As well as the application, it destroys anything else you have installed,
  including the archived stock V1.2.0 image and the high recovery bootloader if
  you set up the stock/OpenScope switcher (`scripts/switch_firmware.py`). After
  an unprotect you are back to a bare chip and must redo first-time setup, and
  you will need to re-archive a stock image before the switcher works again.
- **Device shows only `BOOTLOADER MODE` after first-time setup:** ROM DFU
  installed the USB HID bootloader, but the application did not boot. Leave
  BOOT0 disconnected and run `cd firmware && make flash` while the bootloader
  screen is visible.
- **Bricked after a bad flash:** You can always re-enter DFU mode with the BOOT0 procedure above. The ROM bootloader is permanent and cannot be overwritten.
