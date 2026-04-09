# External Flash Runtime Path Map 2026-04-08

## Summary

The external-flash runtime path is starting to separate into two different buckets:

1. an **early boot probe** that sets `DAT_20001066` and appears to choose which SPI-flash bank/LUN behavior to expose
2. a **formatted-path resource/metadata loader** built around `DAT_20000F09`, `FUN_08034878()`, and `FUN_08035ED4()`

This makes `9999.BIN` look less like the immediate boot blocker and more like a separate, still-unresolved resource/config path.

## 1. Early Boot Probe: `0x080BC841`

In [fatfs_init @ 0x0802E7BC](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22738), stock does:

```c
cVar1 = fatfs_open_read(uVar5, 0x80bc841, 0);
if (*pcVar3 == '\0') {
    DAT_20001066 = 0;
    FUN_0802d014(uVar5);
}
else {
    DAT_20001066 = 1;
}
```

The important follow-on is that `DAT_20001066` is already documented in:

- [stock_iap_bootloader.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/stock_iap_bootloader.md#L225)

as the SPI-flash bank selector used by the USB MSC path:

- if `DAT_20001066 == 0`, a `+0x200000` offset is applied
- otherwise base flash is used directly

That means this early `0x080BC841` file-open is probably **not** a scope config file. It is more likely a volume/layout probe, bank-selection marker, or filesystem-state check.

This is a good reason to stop treating the boot-time `DAT_20001066` path as evidence that `9999.BIN` is boot-critical.

## 2. Formatted Path Loader: `0x080BC859`

The more interesting runtime path is:

- [FUN_08035ED4](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L29627)

which does:

```c
FUN_08000370(&string_format_buffer, 0x80bc859, DAT_20000f09);
local_11 = fatfs_open_read(local_10, &string_format_buffer, 10);
```

Then, on success, it reads a fixed-size payload:

- `0x25A` bytes into `0x2000044E`
- then reads another `5` bytes of small config/state data

And on failure it calls:

- `FUN_0802E12C()`

which `function_names.md` currently labels:

- `spi_flash_create_entry`

So this path looks much more like:

- "open per-index metadata record"
- "if missing/bad, create or repair it"

than anything tied to the empty `9999.BIN` entry.

## 3. Enumerator / Repair Driver: `FUN_08034878`

The companion path is:

- [FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)

It:

1. opens a higher-flash-backed descriptor list (`param_1`, often `0x80BC18B`)
2. iterates entries with `FUN_0802DF20`
3. converts each entry name with `FUN_0802912C` (`fs_format_filename`)
4. formats one path via `0x80BCAE5`
5. calls `FUN_0802EA08()`
6. on failure, formats another path via `0x80BC859` and calls `FUN_0802E12C()`

It also writes:

- `DAT_20000F09 = last_item + 1` or `1`

which matches the current RAM-map interpretation in:

- [STATE_STRUCTURE.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/STATE_STRUCTURE.md#L252)

So the `0x80BC859` family is strongly tied to:

- `panel_entry_index`
- resource list enumeration
- metadata creation/repair

not obviously to a one-off `9999.BIN` blob.

## 4. Missing High Flash Still Matters

The big problem remains:

- `0x080BC841`
- `0x080BC859`
- `0x080BC18B`
- `0x080BC1A5`
- `0x080BC1B4`
- `0x080BB927`
- `0x080BB92B`
- `0x080BBA1F`
- `0x080BBA22`

are all **outside the downloaded V1.2.0 vendor app image**.

That means we can map the *call structure* from the in-range code, but the actual filename/descriptor payloads for these paths are still absent from the website firmware image.

This is consistent with:

- the stock app black-screening when flashed into our app slot
- the corrected literal `9999.BIN` string still having no direct xrefs

## 5. Current Best Interpretation of `9999.BIN`

Given all of the above:

- `9999.BIN` still exists as a literal path string in the image
- the board's external flash contains an empty placeholder entry for it
- but the best-grounded runtime loaders we can currently trace are centered on missing high-flash descriptors and per-index formatted paths, not direct use of `9999.BIN`

So the best current read is:

- `9999.BIN` is still a real lead,
- but it is probably **not** the first or simplest explanation for the stock-app boot failure,
- and it is probably **not** the same thing as the `DAT_20001066` boot probe.

## Best Next Steps

1. Trace the in-range callers of `FUN_08035ED4()` and `FUN_08034878()` to identify which UI/runtime branches consume the per-index metadata path.
2. Treat `0x080BC859` and `0x080BC841` as distinct missing-descriptor families with different roles.
3. Keep `9999.BIN` on the board as a likely placeholder/stub, but stop assuming it is the direct boot-critical file without stronger evidence.
