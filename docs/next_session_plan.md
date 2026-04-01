# Next Session Plan: USB In-Application Bootloader

## Goal

Build a permanent USB bootloader so the case can be closed and firmware updated over USB-C without opening it. This is the critical path to end-user usability.

## Architecture

```
Flash Layout (AT32F403A, 1MB):
┌──────────────────────┐ 0x08000000
│  BOOTLOADER (8KB)    │  Permanent, never overwritten by user
│  - PC9 power hold    │
│  - RAM flag check    │
│  - USB HID IAP       │
│  - Flash programmer  │
├──────────────────────┤ 0x08001800
│  IAP Flag (2KB)      │  Magic word 0x41544B38 = valid app
├──────────────────────┤ 0x08002000
│  APPLICATION         │  Main firmware, VTOR offset = 0x2000
│  (~1016KB available) │
└──────────────────────┘ 0x080FFFFF
```

## Boot Flow

1. Power on → bootloader starts → PC9 HIGH (power hold)
2. Check RAM at 0x20037FF0: if magic word 0xDEADBEEF → enter USB DFU
3. Check flash at 0x08001800: if 0x41544B38 → jump to app at 0x08002000
4. Otherwise → enter USB DFU (no valid app, or update requested)

## Implementation

### Bootloader (`firmware/bootloader/`)

Based on ArteryTek's HID IAP example in the SDK:
- `middlewares/usbd_class/hid_iap/` — HID class with IAP protocol
- `middlewares/usbd_drivers/` — USB device core

Key adaptations:
- **USB clock:** Use HICK (internal 48MHz) since 240MHz has no /5 divider for USB
- **PC9 power hold:** Must be first instruction
- **App address:** 0x08002000 (not 0x08005000 as in the demo)
- **RAM flag:** Check 0x20037FF0 for 0xDEADBEEF before flash flag

USB HID protocol (from SDK):
- Device enumerates as HID (VID 0x2E3C, PID 0xAF01)
- 64-byte reports: START → ADDR → DATA (1KB chunks) → FINISH → JMP
- ~7.5KB total code, fits in 8KB with -Os

### App Changes

1. **Linker script:** FLASH origin = 0x08002000, LENGTH = 1016K
2. **VTOR:** `nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x2000)` at start of main()
3. **DFU request:** Write 0xDEADBEEF to 0x20037FF0, call NVIC_SystemReset()

### Host Tool (`scripts/hid_flash.py`)

~60-line Python script using `hidapi`:
```
./hid_flash.py firmware/build/firmware.bin
```
Sends START → ADDR → DATA chunks → FINISH → JMP. Progress bar.

### Development Workflow (end state)

```bash
# From terminal, case closed:
./scripts/hid_flash.py firmware/build/firmware.bin
# OR from device: Settings → Firmware Update → device enters bootloader
# Then: ./scripts/hid_flash.py firmware/build/firmware.bin
```

## What's Already Done

- AT32 USB middleware already in `at32f403a_lib/middlewares/`
- HID IAP class code ready in `middlewares/usbd_class/hid_iap/`
- Virtual MSC IAP example at `project/at_start_f403a/examples/usb_device/virtual_msc_iap/`
- Full analysis documented in this file

## What Was Tried (ROM Bootloader)

Software jump to the AT32 ROM bootloader failed because:
1. ROM bootloader checks BOOT0 pin — can't drive it HIGH from software
2. PC9 soft power latch drops on NVIC_SystemReset()
3. Flash erase + direct jump: bootloader started but USB didn't re-enumerate, device powered off

The in-application bootloader solves all of these: it handles PC9 itself, doesn't need BOOT0, and stays resident until explicitly jumping to the app.

## Estimated Effort

- Bootloader build system + main.c: 1 hour
- USB HID IAP integration: 2 hours
- Linker script changes for app: 30 min
- Python host tool: 30 min
- Testing: 1 hour
- **Total: ~5 hours**

## Files Referenced

- `at32f403a_lib/middlewares/usbd_drivers/` — USB device core
- `at32f403a_lib/middlewares/usbd_class/hid_iap/` — HID IAP class
- `at32f403a_lib/project/at_start_f403a/examples/usb_device/virtual_msc_iap/` — Full IAP example
- `at32f403a_lib/libraries/drivers/src/at32f403a_407_usb.c` — Low-level USB driver
