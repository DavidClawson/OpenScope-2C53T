# FatFs Init Missing High Flash 2026-04-08

## Summary

The stock `fatfs_init()` path in the V1.2.0 decompile depends on multiple data pointers that are **outside the downloaded vendor app image**.

This is a strong explanation for two bench results:

1. the vendor app-slot flash black-screened instead of booting
2. direct xref tracing for `3:/System file/9999.bin` stayed empty

The external flash dump is real and readable, but the stock app still appears to rely on higher-flash data that is absent from the website download.

## Grounded Facts

The downloaded vendor app inside:

- `/Users/david/Desktop/osc/archive/2C53T_Firmware_V1.2.0.zip`
- file: `2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`

has length:

- `751232` bytes

If treated as an app-slot image at `0x08004000`, it ends at:

- `0x080B7680`

But `fatfs_init()` in:

- [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22738)

uses these data pointers:

- `0x080BBA1F`
- `0x080BB927`
- `0x080BBC58`
- `0x080BC1B4`
- `0x080BC18B`
- `0x080BBA22`
- `0x080BB92B`
- `0x080BC1A5`
- `0x080BC841`

Every one of those addresses is **past the end** of the downloaded app image.

## What `fatfs_init()` Does

Recovered flow from [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22738):

1. mount first volume with `fatfs_mount(0x20002DA8, 0x080BBA1F)`
2. open a file/object with `fatfs_open(..., 0x080BB927)`
3. run several `fatfs_read_write(...)` / `fatfs_operation()` calls using:
   - `0x080BBC58`
   - `0x080BC1B4`
   - `0x080BC18B`
4. mount second volume with `fatfs_mount(0x20003DDC, 0x080BBA22)`
5. open another file/object with `fatfs_open(..., 0x080BB92B)`
6. do another `fatfs_read_write(...)` with `0x080BC1A5`
7. do an early boot `fatfs_open_read(..., 0x080BC841, 0)` and set `DAT_20001066` depending on success

So boot-time filesystem setup is not using literal in-range strings alone. It is driven by higher-flash data records that are missing from the vendor app file we downloaded.

## Why This Matters For `9999.BIN`

The corrected literal string addresses are:

- `3:/System file/%d.jpg` at `0x080B9681`
- `3:/System file/9999.bin` at `0x080B9841`
- `2:/Screenshot simple file/%d.bin` at `0x080B9859`

Those strings are within the downloaded app image.

But Ghidra xref tracing against those corrected addresses still found no direct references:

- [flash_path_xrefs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/flash_path_xrefs_2026_04_08.txt#L1)

The best current explanation is:

- the literal strings live in the image,
- but the actual boot/runtime file-open flow reaches them through missing high-flash descriptor tables or indirection records,
- not via direct code references to the string addresses themselves.

So `9999.BIN` may still be real, but the code path that decides whether and how to open it is likely entangled with the missing `0x080BBxxx / 0x080BCxxx` data region.

## Connection To Bench Results

### Stock app black screen

When we flashed the downloaded vendor app into the app slot, the bootloader jumped into it, but the device went dark.

This result now makes more sense:

- the app binary itself can launch,
- but its boot-time filesystem init depends on higher-flash data that is absent from the website app image.

### External flash dump does not rescue the website app by itself

The board's W25Q128 contains:

- a valid `System file` directory with `160` JPEG assets
- an empty `9999.BIN`
- a second FAT volume at `0x200000`

So the external flash is present and readable.

But stock boot still may fail if the MCU-side high-flash descriptors that tell it *what* to open and *how* to interpret it are missing from the app image.

## Best Next Steps

1. Treat missing high-flash data as a top-tier explanation for stock-app boot failure.
2. Stop using direct xrefs to `0x080B9841` as the main `9999.BIN` strategy.
3. Trace the in-range callers of `fatfs_open_read`, `fatfs_open`, and related helpers that use formatted paths at runtime.
4. If another unit becomes available, compare external flash first, but also keep looking for any path to recover the missing MCU high-flash region indirectly.
