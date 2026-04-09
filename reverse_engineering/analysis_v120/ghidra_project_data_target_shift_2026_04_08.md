# Ghidra Project Data-Target Shift 2026-04-08

## Summary

This pass resolves an important ambiguity in the current V1.2.0 workflow:

- how to map an **absolute flash literal inside app code** to the correct bytes
  inside the existing Ghidra project

The strongest result is:

- the current Ghidra project imported the app file as one flat block starting at
  `0x08000000`
- so for **data targets referenced by absolute literals inside app code**, the
  matching bytes in the project live at:
  - `project_addr = runtime_literal - 0x4000`

This is different from the earlier raw-app comparison rule in
[raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md),
which was focused on:

- comparing project/decompile **code locations** to the archived raw app file

So we now need two related but different rules:

1. **Code-location comparison**
   - raw app address = decompiled/project code address + `0x4000`
2. **Absolute data-literal target inside the current project**
   - project data address = runtime literal address - `0x4000`

That second rule is the new one.

Primary references:

- [raw_app_base_offset_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/raw_app_base_offset_2026_04_08.md)
- [key_task_dispatch_surface_contradiction_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/key_task_dispatch_surface_contradiction_2026_04_08.md)

## 1. The current Ghidra project really is one flat block at `0x08000000`

Headless `ListMemoryBlocks.java` now confirms the project view:

```text
ram | 08000000 | 080b767f | 0xB7680 | true | true | true | true | false | false
```

So there is:

- no separate app-slot block at `0x08004000`
- no overlay that remaps app literals onto a second initialized region

The imported program is simply one flat initialized block covering the file
length.

## 2. Project `0x08046520` matches raw file offset `0x46520`, not runtime `0x08046520`

I dumped the current project bytes at:

- `0x08046520..0x08046590`

and they came out as:

```text
08046520: F1 FF FF FF 20 00 00 00 00 00 00 00 00 00 00 00
08046530: 00 00 00 00 00 0F FF FF F4 00 00 00 00 00 00 00
08046540: 00 00 00 00 04 EF FF FF FF FF F3 00 00 BF FF FF
...
```

Those bytes match the archived raw app file at file offset:

- `0x46520`

not the bytes at the runtime-linked app address:

- runtime `0x08046520` -> raw file offset `0x42520`

So the current project address `0x08046520` is effectively:

- file offset `0x46520`
- runtime address `0x0804A520`

not the runtime literal `0x08046520`

## 3. Project `0x08042520` matches runtime literal `0x08046520`

I then dumped the project bytes at:

- `0x08042520..0x08042590`

and got:

```text
08042520: 98 FF FF FF DE 00 00 00 2F FE FF FF 72 03 00 00
08042530: DB F9 FF FF F6 0A 00 00 A1 E9 FF FF AF 71 00 00
08042540: CE 27 00 00 8B F0 FF FF 4D 08 00 00 4B FB FF FF
...
```

Those bytes match the archived raw app file at offset:

- `0x42520`

which is exactly the correct raw-file location for runtime-linked address:

- `0x08046520`

So within the current project:

- runtime literal `0x08046520`
- must be inspected at project address `0x08042520`

That is the concrete proof for the new rule:

- `project_addr = runtime_literal - 0x4000`

## 4. What this changes for the `key_task` dispatch surface

This explains one part of the earlier dispatcher confusion.

When raw `key_task` at runtime loads:

- `0x08046548`

the matching bytes in the current project are **not** at:

- project `0x08046548`

They are at:

- project `0x08042548`

That means some earlier table checks were mixing:

- runtime-linked literal addresses
with
- project storage addresses in a misbased import

However, the deeper contradiction still survives:

- the **correct** bytes for runtime `0x08046544/48`, now seen at project
  `0x08042544/48`, still look exidx-like rather than handler-like

So this new rule fixes the **location** mismatch, but not the **semantic**
contradiction.

## 5. Updated practical rules

For the current Ghidra project:

### A. Comparing code locations to the archived raw app

- `raw app address = project/decompile code address + 0x4000`

Use this when:

- checking disassembly against the archived app binary
- validating callsites and code slices

### B. Resolving an absolute flash literal from code to the right project bytes

- `project data address = runtime literal address - 0x4000`

Use this when:

- a decompiled function loads a literal flash address
- you want to inspect the corresponding bytes inside the current project

## What This Changes

The best next event-dispatch work should no longer ask:

- "what bytes are at project `0x08046544`?"

It should ask:

- "what bytes are at project `0x08042544`, which correspond to runtime literal
  `0x08046544`?"

That is the right surface for future Ghidra-side checks until the project is
re-imported at the correct app-slot base.

## Best Next Move

1. Apply the `-0x4000` rule to other flash-literal tables in the current
   project before trusting their bytes.
2. Keep the `key_task` contradiction alive, but relocate it to:
   - runtime `0x08046544/48`
   - project `0x08042544/48`
3. Re-evaluate earlier "invalid table" checks case by case, because some may
   have been looking at the wrong project storage addresses even when the raw
   app checks were fine.
