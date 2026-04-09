# Unicorn Stock Flash Trace Second Pass 2026-04-08

Purpose:
- extend the software-only SPI2 flash tracer far beyond the original LCD-init
  stall
- answer whether `master_init()` itself reaches real W25Q128 block reads
- test whether direct `fatfs_init()` emulation is a cleaner path to filesystem
  traffic

Artifacts:
- tracer:
  [unicorn_stock_flash_trace.py](/Users/david/Desktop/osc/scripts/unicorn_stock_flash_trace.py)
- latest master-init trace:
  [unicorn_stock_flash_trace_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_2026_04_08.txt)
- direct fatfs-init trace:
  [unicorn_fatfs_init_trace_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_2026_04_08.txt)
- seeded direct fatfs-init trace:
  [unicorn_fatfs_init_trace_seeded_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_seeded_2026_04_08.txt)
- seeded direct fatfs-init trace with synthetic high-flash FS strings:
  [unicorn_fatfs_init_trace_seeded_fs_2026_04_08.txt](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_fatfs_init_trace_seeded_fs_2026_04_08.txt)

Update:
- the follow-up ablation in
  [unicorn_stock_flash_trace_third_pass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_third_pass_2026_04_08.md#L1)
  shows that the root + directory-root descriptor families matter more than the
  guessed `0x080BC841 = "3:/System file/9999.bin"` probe text itself

## What changed

The tracer now has enough board-specific emulation to move far beyond the first
pass:
- skips two large LCD/image blit families in early init
- fixes `ADC1_CTRL2` readback so stock calibration/conversion waits complete
- models `SPI3_STAT` and `SPI3_DATA` well enough to get through the early FPGA
  handshake path
- returns an idle-high `PC8` on `GPIOC_ISTAT` so the boot path does not spin in
  the passive-key wait loop
- records `entry`, `last_pc`, `last_lr`, and any Unicorn run error
- supports arbitrary function entry via `--entry`

## New master-init result

With the second-pass model, stock `master_init()` now reaches much deeper boot
code:
- final trace stop:
  `stop_pc = 0x08033EC4`
- no Unicorn fault in the successful long run
- only one SPI2 flash transaction still appears:
  `0x90 00 00 00 FF FF`

That means:
- the software-only boot path is now materially stronger than the first pass
- but `master_init()` still does not hit any real `0x03` / `0x0B` W25Q128 block
  reads in this harness before it moves into later generic setup / memory work

Practical read:
- the early boot path really does perform the legacy flash-ID probe
- deeper filesystem/resource traversal likely happens later than the currently
  traced init surface, not during the narrow SPI2-ID stage

## Direct fatfs_init result

Directly entering `fatfs_init()` is not yet viable as a shortcut.

The direct run dies almost immediately:
- `entry = 0x0802E7BD`
- `last_pc = 0x08033D16`
- `last_lr = 0x0802E7C9`
- `run_error = Invalid instruction (UC_ERR_INSN_INVALID)`

The crash site is revealing:
- `0x08033CFC` is the stock allocator wrapper
- at `0x08033D10..0x08033D16`, it checks the RAM block at `0x20001070`
- with our zeroed RAM, the callback pointer at `0x20001070` is null
- stock then executes `blx r1` with `r1 = 0`, which explains the immediate
  failure

So the blocker for direct `fatfs_init()` is not SPI2. It is missing RAM-side
allocator/bootstrap state.

## Seeded fatfs_init result

Seeding a synthetic display-buffer allocator block at `0x20001070` is enough to
make direct `fatfs_init()` run to completion.

Seeded run:
- `entry = 0x0802E7BD`
- `seed_display_alloc = True`
- `stop_pc = 0x08FFFFF0`
- `last_pc = 0x0802E904`
- `transactions = 0`

This is a strong result:
- the direct function no longer crashes on allocator bootstrap
- but it still performs **zero** SPI2 flash transactions against the real dump

The best current read is:
- direct `fatfs_init()` is now blocked by stock descriptor/resource state before
  any actual W25Q128 read is attempted
- that lines up with the earlier high-flash audit: the key path strings and
  descriptors used by `fatfs_init()` sit beyond the end of the downloaded app

## Seeded high-flash FS result

Injecting a minimal synthetic high-flash descriptor/string table changes the
behavior dramatically.

Synthetic table used:
- `0x080BBA1F = "2:"`
- `0x080BBA22 = "3:"`
- `0x080BB927 = "2:/"`
- `0x080BB92B = "3:/"`
- `0x080BBC58 = "2:/LOGO"`
- `0x080BC18B = "2:/Screenshot simple file"`
- `0x080BC1A5 = "3:/System file"`
- `0x080BC1B4 = "2:/Screenshot file"`
- `0x080BC841 = "3:/System file/9999.bin"`

With that seed in place, direct `fatfs_init()` produces real SPI2 traffic:
- `14` block reads with opcode `0x03`
- `1` status read with opcode `0x05`
- `2` write-enables with opcode `0x06`
- `1` sector erase with opcode `0x20`

First observed block reads:
- `0x200000`
- `0x205000`
- `0x206000`
- `0x207000`

Observed erase command:
- `0x20 20 70 00` -> sector erase at `0x207000`

This is the strongest software-only result so far because it means:
- the W25Q128 model is good enough to exercise real filesystem traffic
- the missing high-flash descriptor strings are not a side detail; they are
  an actual gating factor for stock filesystem behavior
- the stock-app black-screen result is now even more consistent with missing
  MCU-side high-flash descriptor content rather than a dead external flash

## What this means

The current best software-only picture is:
- `master_init()` alone is not enough to reach real filesystem reads in this
  harness
- direct `fatfs_init()` needed seeded allocator RAM state first, and once that
  was provided it still performed no SPI2 reads
- that shifts the next gate from allocator bootstrap to missing descriptor /
  resource state, most likely the high-flash-backed path pointers already
  catalogued in
  [fatfs_init_missing_high_flash_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/fatfs_init_missing_high_flash_2026_04_08.md#L1)
- once a minimal synthetic high-flash descriptor/string table is injected,
  direct `fatfs_init()` immediately reaches real SPI2 read/erase traffic on the
  dumped flash image

This is still useful because it narrowed the next step sharply:
- the next gap was descriptor/resource state, not the SPI2 flash model
- the third pass now refines that further: the important synthetic families are
  the drive roots and directory roots, while `0x080BC841` is less semantically
  decisive than first assumed

## Best next step

Best software-only next move:
- follow the corrected third-pass priority:
  [unicorn_stock_flash_trace_third_pass_2026_04_08.md](/Users/david/Desktop/osc/reverse_engineering/analysis_v120/unicorn_stock_flash_trace_third_pass_2026_04_08.md#L149)
- specifically, trace why the `0x080BBC58` / `2:/LOGO` family diverts control
  flow and inspect what metadata stock is trying to synthesize at `0x207000`
