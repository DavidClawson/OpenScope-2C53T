# FatFs Boot Follow-On Non-Gate 2026-04-08

Purpose:
- determine whether the Volume 1 root-directory normalization path is itself a
  hard boot gate
- identify the first concrete later consumer of the same descriptor family
- decide whether the stock-app black screen is more likely to come from
  `fatfs_init()` itself or from later descriptor-backed runtime paths

Key references:
- [master init call site](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L964)
- [fatfs_init @ 0x0802E7BC](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22735)
- [USB MSC bank-selection uses of `DAT_20001066`](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L14845)
- [FUN_08034878 @ 0x08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28279)
- [right_panel_resource_owner_map_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/right_panel_resource_owner_map_2026_04_08.md#L1)
- [volume1_root_directory_rewrite_path_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/volume1_root_directory_rewrite_path_2026_04_08.md#L1)

## 1. `fatfs_init()` Is Unconditionally Called During Boot

In the recovered master-init disassembly, stock does:

1. `FUN_0802EF48()`
2. `FUN_0802E7BC()`
3. immediately continues into timer/peripheral setup

See:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L964)

That matters because `fatfs_init()` returns `void`, and the call site does not
branch on any success/failure result. So the boot flow does **not** treat
`fatfs_init()` as an explicit success gate.

## 2. The Volume 1 Rewrite Path Is Conditional Inside `fatfs_init()`, Not At The Call Site

Inside [fatfs_init](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22738):

- mount `2:`
- `fatfs_open(..., 0x080BB927)` which is the current best match for `2:/`
- only if that open succeeds:
  - `fatfs_read_write(0x080BBC58)` -> `2:/LOGO`
  - `fatfs_read_write(0x080BC1B4)` -> `2:/Screenshot file`
  - `fatfs_read_write(0x080BC18B)` -> `2:/Screenshot simple file`
  - `fatfs_operation()`

Then it separately:

- mounts `3:`
- opens `3:/`
- conditionally runs `fatfs_read_write(0x080BC1A5)` for `3:/System file`

So the root-directory normalization family is real, but the boot code only
enters it if the relevant descriptor-backed root open succeeds. The outer boot
flow itself does not stop or branch on the result.

## 3. `DAT_20001066` Is Much Narrower Than A Boot-Screen Gate

`fatfs_init()` finishes with:

- `fatfs_open_read(..., 0x080BC841, 0)`
- `DAT_20001066 = 0` on success, else `1`

The important correction is that the known runtime uses of
`DAT_20001066` are the Mass Storage transfer handlers, not the later UI/render
stack.

The grounded uses in [decompiled_2C53T_v2.c](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L14845)
show:

- if `DAT_20001066 == 0`, SPI-flash reads/writes for LUN 0 apply `+0x200000`
- else they use the base address directly

That matches the older USB note in
[stock_iap_bootloader.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/stock_iap_bootloader.md#L225).

So `DAT_20001066` currently looks like:

- early bank/LUN selection for USB Mass Storage over SPI flash

not:

- the direct cause of the stock-app black screen

## 4. The First Concrete Later Consumer Is `FUN_08034878(0x080BC18B)`

Much later in the same boot path, stock does:

```c
DAT_20000F13 = FUN_08034878(0x080BC18B);
```

at:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5589)

This is the first clear later boot consumer of the same `2:/Screenshot simple file`
descriptor family that was normalized earlier in `fatfs_init()`.

And [FUN_08034878](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L28282)
is not a generic mount helper. It:

1. opens the descriptor family
2. iterates entries with `FUN_0802DF20(...)`
3. formats a per-index label/path via `FUN_0802912C(...)`
4. calls [FUN_0802EA08](/Users/david/Desktop/osc/reverse_engineering/decompiled_2C53T_v2.c#L22906)
5. seeds:
   - `DAT_20000F13` / panel entry count
   - `DAT_20000F09` / panel entry index

That ties the boot-time filesystem normalization path directly into the later
right-panel overlay/resource stack.

## 5. What `FUN_0802EA08()` Implies

`FUN_0802EA08()` defaults to `0x080BC1B4` and:

- opens `2:/Screenshot file`
- iterates entries
- compares names against the current formatted buffer

So the later consumer path looks like a descriptor-backed screenshot/preview
enumerator/validator, not a core LCD bring-up dependency.

That makes the current chain look like:

1. normalize the screenshot/logo roots in `fatfs_init()`
2. later enumerate and validate overlay/resource entries with `FUN_08034878()`
3. use that list in the right-panel resource family

## 6. Best Current Read

This is the useful correction:

- the Volume 1 root-directory rewrite path is real boot behavior
- but it is **not** currently the strongest direct boot gate
- `fatfs_init()` is called unconditionally and its result is not checked at the
  call site
- `DAT_20001066` looks like a USB Mass Storage bank-selection side effect
- the first strong later consumer of the same descriptor family is
  `FUN_08034878(0x080BC18B)`, which belongs to the right-panel
  overlay/resource stack

So the stock-app black screen is now more plausibly explained by:

1. missing high-flash descriptor families used by later consumers like
   `FUN_08034878()`
2. or a separate missing high-flash/UI descriptor gate elsewhere in boot

than by the `0x207000` root-directory rewrite itself.

## 7. Best Next Step

The sharpest follow-on trace is now:

1. identify the next boot/runtime consumer after the `FUN_08034878(0x080BC18B)`
   seeding step
2. check whether that later consumer reaches the LCD/resource path before the
   point where the flashed vendor app goes dark
3. keep `DAT_20001066` de-prioritized as a boot-screen theory unless a
   non-MSC use turns up

## 8. Immediate Post-Enumeration Boot Flow

One more useful correction from the master-init disassembly:

Immediately after:

```c
DAT_20000F13 = FUN_08034878(0x080BC18B);
```

the boot path does **not** jump straight into a resource consumer. It goes into:

- TIM5 setup
- TIM2 setup
- SysTick-based waits
- more peripheral/GPIO bring-up

See:
- [init_function_decompile.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/init_function_decompile.txt#L5589)

That means the boot path currently looks like:

1. early filesystem normalization in `fatfs_init()`
2. later boot-time descriptor enumeration via `FUN_08034878(0x080BC18B)`
3. more generic timer/peripheral bring-up
4. only later runtime/UI families consume the seeded overlay state

So we are likely approaching the end of what the downloaded app can explain with
plain code-flow tracing alone. The remaining uncertainty is shrinking toward:

- missing high-flash descriptor/data content
- or later runtime consumers that depend on that content

rather than a large undiscovered boot control branch.
