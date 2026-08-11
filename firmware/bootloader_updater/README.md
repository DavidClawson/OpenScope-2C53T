# Bootloader Updater App

This directory builds a temporary app-slot image that updates the custom HID
bootloader.

Why this shape:

- Existing and older HID bootloaders only accept app-slot writes at
  `0x08004000`.
- The updater is therefore linked as a normal app, embeds `bootloader.bin`, and
  runs from app flash.
- Once started, it validates the embedded payload, erases
  `0x08000000..0x08003FFF`, programs the new bootloader, verifies the bytes, and
  resets into HID IAP.

This does **not** relax the normal HID flash path to arbitrary addresses. Direct
bootloader-region writes remain guarded by ROM DFU/SWD or by this explicit
updater image.

Build:

```sh
make -C firmware/bootloader
make -C firmware/bootloader_updater
```

Dry-run preflight:

```sh
python3 scripts/update_bootloader_via_hid.py --image-only
```

Live update flow:

```sh
python3 scripts/update_bootloader_via_hid.py
```

The live command requires HID IAP mode and an interactive
`UPDATE BOOTLOADER` confirmation. Power loss during the erase/program window can
still require ROM DFU or SWD recovery.
