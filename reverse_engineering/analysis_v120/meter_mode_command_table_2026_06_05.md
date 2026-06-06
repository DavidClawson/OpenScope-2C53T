# Meter Mode Raw Command Table, 2026-06-05

This note records the local extraction of the eight-byte stock meter-mode raw
command table referenced by the `0x080BB3FC` notes.

The archived V1.2.0 app binary is loaded at app-slot VMA `0x08004000`, while the
current decompile notes use addresses as if the app started at `0x08000000`.
For literal data, `raw_app_base_offset_2026_04_08.md` says to inspect
`runtime_literal - 0x4000` in the current project/raw image. Applying that to
`0x080BB3FC` gives raw address `0x080B43FC`, file offset `0x000B43FC`.

Extraction from `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`:

```text
14 0c 17 0b 0a 12 11 10
```

These bytes are the low byte of raw UART words of the form `0x0500 | table[i]`.
They are not display/update selector bytes from queue `0x20002D6C`.
`scripts/test_stock_meter_literals.py` verifies these exact bytes at the
app-image address `0x080B43FC` (file offset `0x000B43FC`) while documenting the
runtime/app-slot literal as `0x080BB3FC`, so local selector policy fails the
software gate if the stock table evidence drifts.

The same guard now also verifies the selector consumer xrefs at
`0x080042E2..0x0800430A` and `0x080048BA..0x080048E2`.  These two stock code
sequences load the runtime `0x080BB3FC` table, read one byte indexed by
`DAT_20001025`, add `0x0500`, and store the raw UART halfword to `0x20002D54`
before queueing through the `0x20002D74` raw-word path.  This is selector-table
consumer evidence only; it still does not recover the analog mux bytes
`ms[0x02]`/`ms[0x03]` or any physical correction coefficient.

The selector adjuster guard now pins the paired prev/next handlers that own the
same digital selector step around those consumer xrefs:

```text
0x080041F8 selector_adjust_prev prologue:
  loads state base `0x200000F8`, reads `ms[0xF68]`, bounds it to 0..9,
  and dispatches through the mixed adjustment TBB table.

0x080042D4 selector_adjust_prev meter case:
  rejects the blocked `bRam20001055 & 0xF0 == 0xB0` state,
  decrements `DAT_20001025` with wrap over 0..7,
  loads `0x080BB3FC + DAT_20001025`,
  stages `0x0500 | table[selector]` at `0x20002D54`,
  enqueues the raw word through `0x20002D74`,
  queues display/update commands `0x1D` and `0x1B`,
  resets value/dirty state, and tail-calls `FUN_080028E0`.

0x080047CC selector_adjust_next prologue:
  same state-base and `ms[0xF68]` TBB owner for the positive adjustment side.

0x080048AC selector_adjust_next meter case:
  rejects the same blocked state, increments `DAT_20001025` with wrap over
  0..7, emits the same raw-word and `0x1D`/`0x1B` display-update sequence,
  resets value/dirty state, and tail-calls `FUN_080028E0`.
```

This is stronger stock evidence for the digital DMM selector state machine:
the stock UI/runtime adjustment owners decrement/increment the eight-entry
selector and emit the matching `0x05xx` raw word.  It is still deliberately
bounded evidence.  It does not recover a runtime analog range writer for
`ms[0x02]`/`ms[0x03]`, relay/AFE switching, factory calibration, or the
low-DCV `0.200 V` visual versus `0.4366 V` CDC blocker.

### Dynamic Raw-Word Helper Guard, 2026-06-06

The helper cluster immediately before the runtime mode-init dispatcher is now
binary-guarded as another digital command/state-machine path:

```text
0x08006060 selector_seed_state_pairs:
  after the `bRam20001055` blocker/lownibble gate, seeds explicit
  `DAT_20001025` / `DAT_2000102E` pairs: default `1/1`,
  `3/5`, `5/7`, `6/8`, and `7/10`.

0x080060CA selector_seed_emit_0501:
  stages raw word `0x0501` at `0x20002D54`, queues it through `0x20002D74`,
  then queues display/update bytes `0x1D` and `0x1B` through `0x20002D6C`.

0x08006120 dynamic_raw_word_gate_and_mask:
  runs only when `ms[0xF68] == 1`, rejects `bRam20001055 & 0xF0 == 0xB0`,
  reads `DAT_20001025`, requires selector mask `0xC6`, reads
  `DAT_2000102E`, and updates that selector-side state.

0x08006194 / 0x0800626A dynamic low-byte choices:
  selector-side families choose `0x0C/0x0D`, `0x0E/0x17`,
  `0x11/0x16`, or `0x10/0x15` depending on the `DAT_2000102E` side.

0x08006288 dynamic_raw_word_emit_tail:
  writes NaN display sentinels, calls `FUN_080028E0`, ORs the staged low byte
  with `0x0500`, queues the halfword through `0x20002D74`, then queues
  display/update byte `0x1B`.

0x080062F8 dynamic_helper_reverse_partner:
  shares the same outer `ms[0xF68]` gate, clears or updates state, and emits
  display/update bytes (`0x26`/`0x28` or `0x02`) instead of the dynamic
  `0x05xx` raw word.
```

The stock dynamic apply-pair boundary recovered from those low-byte choices is:

```text
ACV 0x0C/0x0D
DCA 0x17/0x0E
continuity 0x11/0x16
diode 0x10/0x15
```

The local transition plan may use apply words from this set only:
`0x050D`, `0x050E`, `0x0516`, and `0x0515`. Current local split modes share
the DCA pair, and no stock helper pair has been recovered for DCV, ACA,
resistance, capacitance, temperature, or microamp. If one of those modes needs
different behavior, recover a new stock low-byte pair or range writer first;
do not create a new apply word from decoded magnitude, unit text, or a single
bench reading.

This `dynamic raw-word helper guard` ties the selector bytes, display formatter
state, and `0x20002D74` raw-word queue together in stock code.  It strengthens
the digital DMM/FPGA command path model without upgrading the evidence to
analog mux/range proof: it still does not find a runtime writer for
`ms[0x02]`/`ms[0x03]`, does not prove relay/AFE settling, and does not provide
a factory calibration source for the low-DCV blocker.

### Meter Basic Raw-Word Queue Guard, 2026-06-06

The basic meter raw-word materializers are now binary-guarded separately from
the selector table and dynamic apply helper:

```text
0x080033CA meter_basic_configure_0508:
  materializes `0x0508` and stores it to the raw-word queue path.

0x08003BA4 meter_basic_start_0509:
  materializes `0x0509` and stores it to the raw-word queue path.

0x08005B7A meter_basic_variant_0514:
  materializes `0x0514`, clears the `bRam20001055` blocker byte, then follows
  the display/update tail.
```

This grounds the open firmware wake/start/probe tail as stock command
sequencing. It does not recover an analog range writer, low-DCV correction,
factory calibration coefficient, exact settle/discard count, or H2/SPI3 apply
effect. The probe word remains the stock GPIOC bit-7 branch (`0x0507` or
`0x050A`) guarded in `FUN_0800B908`.

The stock raw-word queue consumer is now binary-guarded as well. This
`dvom_TX raw-word consumer guard` proves that the `dvom_TX` task at
`0x080373F4` uses queue handle `0x20002D74`, blocks in `xQueueReceive`, then
formats the received halfword into the USART2 TX buffer:

```text
0x080373F4: task prologue, sets r6 = 0x20002D74
0x08037420: ldr r0, [r6, #0]
0x08037428: bl 0x0803B1D8              ; xQueueReceive(queue, &halfword, -1)
0x08037430: ldrh.w r0, [sp, #6]
0x08037434: strb.w r9, [r8]            ; clear tx index at 0x2000000F
0x08037438: lsrs r1, r0, #8
0x0803743A: strb r0, [r7, #3]          ; low byte into tx buffer
0x08037440: strb r1, [r7, #2]          ; high byte into tx buffer
0x08037442: strb r0, [r7, #9]          ; byte-sum/check byte
0x08037444: ldr r0, [r5, #0]
0x08037446: orr.w r0, r0, #0x80        ; USART2 CTRL1 TDBEIEN
0x0803744A: str r0, [r5, #0]
```

The guarded bytes are:

```text
0x080373F4:
  82 b0 44 f2 0c 45 42 f6 74 56 40 f2 05 07 40 f2
  0f 08 c4 f2 00 05 c2 f2 00 06 0d f1 06 04 c2 f2
  00 07 c2 f2 00 08 4f f0 00 09 00 bf 30 68 21 46
  4f f0 ff 32 03 f0 d6 fe 01 28 f7 d1 bd f8 06 00
  88 f8 00 90 01 0a f8 70 00 eb 10 20 b9 70 78 72
  28 68 40 f0 80 00 28 60 0a 20 02 f0 9f ff e5 e7
```

This proves that the guarded selector-table producers feed a real stock
USART2 command path, not the display queue. It is still digital command-path
evidence only: it does not recover the DMM-specific `ms[0x02]`/`ms[0x03]`
analog mux writers, relay/range timing, or calibration.

The stock transport transition is now binary-guarded too. The
saved mode-init restore path is stock-visible at `0x08026F50`: it copies the
saved `ms[0xF64]` byte into live `ms[0xF68]` before the restored state is
branched into the same boot transport paths. This
`meter transport transition guard` now covers the saved-state prelude plus the
two boot/config branches that enable/resume or disable/drain the DMM USART2
path:

```text
0x08026F50: read saved `ms[0xF64]`
0x08026F56: if nonzero, copy saved byte to live `ms[0xF68]`
0x08026F5A..0x08026F64: branch restored state 1/3/2 into boot transport paths
0x08026F80..0x08026F8C: if no saved state, read live `ms[0xF68]`,
                        set `ms[0xF69] = 1`, and reuse the same branch logic

0x08026F8E: USART2 CTRL1 |= 0x2000
0x08026F9E: load task handle 0x20002DA0, call vTaskResume
0x08026FAC: load task handle 0x20002DA4, call vTaskResume
0x08026FBA: prepare GPIOC BOP base
0x08026FC6: write 0x800 to GPIOC_BOP, setting PC11
0x08026FCE..0x08026FDA: reset max/min/avg sentinels to 0x7FC00000
0x08026FDE..0x08026FF6: reset selector/shadow/display state

0x0802700A: USART2 CTRL1 &= ~0x2000
0x0802701A: load task handle 0x20002DA0, call task suspend helper
0x08027028: load task handle 0x20002DA4, call task suspend helper
0x08027036..0x0802703A: write 0x800 to GPIOC clear register, clearing PC11
0x0802703E..0x0802704A: reset meter semaphore/queue 0x20002D7C
0x0802704E..0x0802705A: reset raw TX-word queue 0x20002D74
```

The guarded byte slices are:

```text
0x08026F50:
  9a f8 64 0f a0 b1 8a f8 68 0f 01 28 17 d0
  03 28 4c d0 02 28 51 d1 9a f8 54 03 00 07
  42 f6 50 50 c2 f2 00 00 0c bf 00 21 4f f4
  70 51 01 80 44 e0 9a f8 68 0f 01 21 8a f8
  69 1f 01 28 e7 d1

0x08026F8E:
  44 f2 0c 41 c4 f2 00 01 08 68 40 f4 00 50 08 60
  42 f6 a0 50 c2 f2 00 00 00 68 13 f0 32 fb
  42 f6 a4 50 c2 f2 00 00 00 68 13 f0 2b fb
  41 f2 00 01 4f f4 00 60 c4 f2 01 01 08 61
  00 20 c7 f6 c0 70 40 f2 01 11 ca f8 48 0f
  ca f8 4c 0f ca f8 50 0f 00 20 aa f8 35 1f
  ff 21 8a f8 5d 0f 8a f8 2d 0f 8a f8 2f 0f
  8a f8 38 1f aa f8 3c 0f

0x0802700A:
  44 f2 0c 41 c4 f2 00 01 08 68 20 f4 00 50 08 60
  42 f6 a0 50 c2 f2 00 00 00 68 13 f0 b2 fb
  42 f6 a4 50 c2 f2 00 00 00 68 13 f0 ab fb
  4f f4 00 60 c8 f8 00 00 42 f6 7c 50 c2 f2 00 00
  00 68 00 21 14 f0 ad f9 42 f6 74 50 c2 f2 00 00
  00 68 00 21 13 f0 ed fd
```

This is stock evidence for the transport side of DMM transitions: saved
mode-init restore, pause/drain via task suspension and queue reset, then resume
with USART2 and PC11 active.
It does not recover the exact local settle delay or frame-discard count, so the
open firmware must keep those constants documented as conservative local
policy until a stock runtime trace proves them. It also does not recover a
physical analog range writer for `ms[0x02]`/`ms[0x03]` or any factory
calibration coefficient.

### Meter Transport Operation Guard, 2026-06-06

The transport byte-slice guard is now paired with an operation-order guard. The
new check names the stock operations inside the already-guarded slices, so the
state-machine boundary is not just a blob comparison:

```text
boot enable/resume/reset:
  USART2 enable -> resume DVOM task 0x20002DA0 -> resume DVOM task 0x20002DA4
  -> set PC11 through GPIOC_BOP -> reset meter float sentinels
  -> reset selector/shadow bytes

boot disable/suspend/drain:
  USART2 disable -> suspend DVOM task 0x20002DA0 -> suspend DVOM task 0x20002DA4
  -> clear PC11 through GPIOC_BCR -> reset meter semaphore/queue 0x20002D7C
  -> reset raw TX-word queue 0x20002D74

runtime enable/resume tail:
  set mode-state 1 -> USART2 enable -> resume both DVOM tasks
  -> set PC11 -> reset selector/shadow bytes -> tail-call FUN_0800B908

runtime disable/suspend/drain:
  USART2 disable -> suspend both DVOM tasks -> clear PC11
  -> reset 0x20002D7C -> reset 0x20002D74 -> clear stale meter state
```

This strengthens the stock reset/resume/drain evidence used by the local DMM
transition model. It still does not recover exact settle/discard timing, an
analog `ms[0x02]`/`ms[0x03]` writer, H2/SPI3 acceptance, or a low-DCV
calibration source.

The runtime UI/mode-switch path carries the same transport shape and is now
covered by the `runtime mode-switch transport guard`.  This is distinct from
the boot/config branch above: `mode_switch_handler` dispatches on the live mode
state and runs the DMM entry/exit transition while the stock app is operating.
The guarded sites are:

```text
0x08007360: common enable/resume tail
            writes mode_state = 1 at `0x20001060`
            USART2 CTRL1 |= 0x2000
            resumes task handles `0x20002DA0` and `0x20002DA4`
            writes `0x800` to GPIOC_BOP at `0x40011010`, setting PC11
            resets max/min/avg sentinels and selector/display shadow bytes
            tail-calls `0x0800B908`

0x0800741A: meter-entry pause/drain case
            USART2 CTRL1 &= ~0x2000
            suspends task handles `0x20002DA0` and `0x20002DA4`
            writes `0x800` to GPIOC_BC/BRR at `0x40011014`, clearing PC11
            resets meter semaphore/queue `0x20002D7C`
            resets raw TX-word queue `0x20002D74`
            clears DMM selector/display shadow state before epilogue

0x080074BE: active/running epilogue
            writes mode_state = 2 at `0x20001060`
            clears transient display bytes
            optionally writes `0x3C00` to `0x20002D50`
            tail-calls `0x0800B908`
```

The guarded byte slices are:

```text
0x08007360:
  01 20 84 f8 68 0f 44 f2 0c 40 c4 f2 00 00 01 68
  41 f4 00 51 01 60 42 f6 a0 50 c2 f2 00 00 00 68
  33 f0 46 f9 42 f6 a4 50 c2 f2 00 00 00 68 33 f0
  3f f9 41 f2 10 00 c4 f2 01 00 4f f4 00 61 01 60
  00 20 c7 f6 c0 70 40 f2 01 11 c4 f8 48 0f c4 f8
  4c 0f c4 f8 50 0f 00 20 a4 f8 35 1f ff 21 84 f8
  5d 0f 84 f8 2f 0f 84 f8 38 1f a4 f8 3c 0f a4 f8
  2c 1f a4 f8 69 0f 84 f8 6b 0f bd e8 10 40 04 f0
  93 ba

0x0800741A:
  44 f2 0c 40 c4 f2 00 00 01 68 21 f4 00 51 01 60
  42 f6 a0 50 c2 f2 00 00 00 68 33 f0 aa f9 42 f6
  a4 50 c2 f2 00 00 00 68 33 f0 a3 f9 41 f2 14 00
  c4 f2 01 00 4f f4 00 61 01 60 42 f6 7c 50 c2 f2
  00 00 00 68 00 21 00 25 33 f0 a1 ff 42 f6 74 50
  c2 f2 00 00 00 68 00 21 33 f0 e1 fb 01 20 84 f8
  36 0f 00 20 c7 f6 c0 70 c4 f8 48 0f c4 f8 4c 0f
  c4 f8 50 0f a4 f8 3c 5f a4 f8 2d 5f c4 f8 30 5f
  0b e0

0x080074BE:
  94 f8 54 13 02 20 84 f8 68 0f 00 20 09 07 a4 f8
  69 0f 84 f8 6b 0f 06 d0 42 f6 50 50 c2 f2 00 00
  4f f4 70 51 01 80 bd e8 b0 40 04 f0 0e ba
```

This strengthens the state-machine evidence for pause/drain/resume in normal runtime transitions.
It still does not recover a DMM-specific `ms[0x02]`/
`ms[0x03]` analog range writer, exact settle/discard counts, or any factory
calibration acceptance/apply proof.

### Boot Mode-Init DMM Sequence Guard, 2026-06-06

`FUN_0800B908` is the stock mode-init dispatcher. It reads `ms[0xF68]`, loads
the USART command queue pointer at `0x20002D6C`, and queues one-byte command
codes with blocking `xQueueGenericSend` calls. It is boot-time command queue
and resume sequencing evidence, not a DMM calibration or range-writer proof.

`scripts/test_stock_meter_literals.py` now carries a `boot mode-init DMM sequence guard`
for the DMM-relevant arms, plus a command-bank guard that checks the
documented command-byte banks in order rather than leaving them as prose:

```text
0x0800B908 mode_init_dispatcher_tbh:
  reads `[0x200000f8 + 0xf68]`, bounds it to 0..9, loads queue `0x20002D6C`,
  and dispatches through the 10-case TBH table.

0x0800B9D6 meter_basic_boot_probe_prefix:
  queues `0x00`, `0x09`, then reads GPIOC IDR bit 7 and selects the
  `0x07`/`0x0A` command tail.

0x0800BA20 meter_basic_boot_range_tail:
  queues `0x1A`, `0x1B`, `0x1C`, `0x1D`, then final command `0x1E`.

0x0800BACE meter_extended_boot_probe_prefix:
  queues `0x00`, `0x08`, `0x09`, then the same `0x07`/`0x0A`
  probe-detect command.

0x0800BB2A meter_extended_boot_range_tail:
  queues `0x16`, `0x17`, `0x18`, then final command `0x19`.

0x0800BC32 meter_variant_boot_tail:
  queues `0x00`, `0x12`, `0x13`, `0x14`, `0x09`, then the same
  `0x07`/`0x0A` probe-detect command.
```

### Boot Mode-Init TBH State Map Guard, 2026-06-06

The dispatcher guard now decodes the Thumb `TBH [pc,r0,lsl#1]` table at
`0x0800B926` instead of documenting only the named arms. The source selector is
`ms[0xF68]` (`[0x200000F8 + 0xF68]`), bounded to states `0..9`; each target
queues one-byte commands through `0x20002D6C`.

```text
state 0 -> 0x0800B93E: 0x00, 0x01, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
state 1 -> 0x0800B9D6: 0x00, 0x09, 0x07/0x0A probe branch, 0x1A..0x1E
state 2 -> 0x0800BA6C: 0x02, 0x03, 0x04, 0x05, 0x06, 0x08
state 3 -> 0x0800BACE: 0x00, 0x08, 0x09, 0x07/0x0A probe branch, 0x16..0x19
state 4 -> 0x0800BB64: 0x00, 0x1F, 0x09, 0x20, 0x21
state 5 -> 0x0800BBBE: 0x00, 0x25, 0x09, 0x26, 0x27, 0x28
state 6 -> 0x0800BC2A: 0x29
state 7 -> 0x0800BC2E: 0x15
state 8 -> 0x0800BCA6: 0x00, 0x2C
state 9 -> 0x0800BC32: 0x00, 0x12, 0x13, 0x14, 0x09, 0x07/0x0A probe branch
```

This is stronger stock state-machine evidence than a loose list of command
bytes: it ties each command bank to the state byte that selects it. It remains a
narrow boundary. `ms[0xF68]` selects mode-init command banks; it is not DMM
`ms[0x02]`/`ms[0x03]` analog mux state, not raw `0x05xx` selector words, not a
factory coefficient, and not low-DCV calibration.

### Meter Probe Branch Guard, 2026-06-06

The `Meter Probe Branch Guard` now separates the PC7 command-tail branch from
range and calibration evidence. In all three guarded meter arms, stock loads
GPIOC IDR from `0x40011008` with:

```text
41 f2 08 00 c4 f2 01 00 00 68
```

The basic and extended arms at `0x0800B9D6` / `0x0800BACE` then use:

```text
07 21 00 06 58 bf 0a 21
```

The variant arm at `0x0800BC32` uses the same logic with `r0` as the command
register:

```text
00 06 4f f0 07 00 58 bf 0a 20
```

Disassembly shape:

```text
movs   #7
lsls   GPIOC_IDR, #24
it     pl
movpl  #10
```

Because `lsls #24` moves GPIOC bit 7 into the sign bit, PC7 high keeps `0x07`
and PC7 low selects `0x0A`. That is stock-visible `GPIOC bit 7` polarity for
the `0x07/0x0A` command tail. It is probe/tail sequencing only: not DMM runtime
range state, not a physical range/calibration source, not a low-DCV correction,
and not H2/SPI3 apply proof. The physical "probe present" label remains an
interpretation of the hardware line, not a decoder or range coefficient.

The same binary guard checks the complete direct callsite set for this dispatcher.
The only direct `BL` instructions to `FUN_0800B908` in the stock APP image are:

```text
0x08002DAA
0x080051D6
0x0800533A
0x08005572
0x080271F8
```

Those are command-dispatch entry evidence only. They do not add a recovered
DMM runtime analog range writer for `ms[0x02]`/`ms[0x03]`.

This guard is useful because local meter bring-up and mode-resume comments use
these command families, and future DMM changes must not replace them with
observed-value guesses. The boundary is just as important: `FUN_0800B908`
queues command bytes to `0x20002D6C`; parameter materialization and raw
`0x05xx` word emission happen downstream. It does not identify the missing
runtime writer for `ms[0x02]`/`ms[0x03]`, does not prove exact settle/discard
counts, and does not explain the `0.200 V` visual versus `0.4366 V` CDC low-DCV
blocker.

The guarded command-byte banks are stock command queue evidence:
`0x00/0x09/(0x07|0x0A)`, `0x1A..0x1E`, `0x00/0x08/0x09/(0x07|0x0A)`,
`0x16..0x19`, and `0x00/0x12/0x13/0x14/0x09/(0x07|0x0A)`. They are not
raw selector words, not mux bytes, and not factory calibration coefficients.

### Runtime Mode-Init Dispatcher Caller Guard, 2026-06-06

The adjacent runtime helper pair at `0x08006418` and `0x08006548` is now
binary-guarded as the stock runtime entry path into `FUN_0800B908`. These
helpers read the mode-init state byte at `ms[0xF68]`, dispatch through TBB
tables, mutate `ms[0xF68]` plus nearby latch/progress bytes, and tail-call
`FUN_0800B908`. That makes them runtime mode-init dispatcher caller evidence:
they select which already-guarded command bank will be queued to `0x20002D6C`.

Guarded slices:

```text
0x08006418 runtime_mode_init_forward_dispatcher:
  reads `[0x200000f8 + 0xf68]`, subtracts one, bounds the state to 1..9,
  and dispatches through the forward helper TBB table.

0x0800644E runtime_mode_init_forward_state2_seed:
  writes `_ms[0xF68] = 0x00050109`, sets `ms[0x355] = 1`,
  and tail-calls `FUN_0800B908`.

0x080064E0 runtime_mode_init_forward_latch_collapse_to_state2:
  when `ms[0xF6A] == 5`, clears `ms[0x355]`, writes `ms[0xF68] = 2`,
  clears `ms[0xF69]`/`ms[0xF6B]`, and tail-calls `FUN_0800B908`.

0x08006548 runtime_mode_init_reverse_dispatcher:
  reads the same `[0x200000f8 + 0xf68]` byte and dispatches through the
  reverse helper TBB table.

0x08006578 runtime_mode_init_reverse_state2_seed:
  writes `_ms[0xF68] = 0x00020109`, sets `ms[0x355] = 1`,
  and tail-calls `FUN_0800B908`.

0x08006592 runtime_mode_init_reverse_state_clear_to_state2:
  writes `ms[0xF68] = 2`, clears `ms[0xE1C]`, `ms[0xE12]`,
  `ms[0xE16]`, and `ms[0xE1A]`, then tail-calls `FUN_0800B908`.

0x080065B2 runtime_mode_init_reverse_state5_seed:
  writes `ms[0xF68] = 5` and tail-calls `FUN_0800B908`.
```

This replaces the looser old "runtime command-bank emitter" wording in the
April notes with a narrower claim: the runtime helpers choose/normalize
mode-init state, and `FUN_0800B908` performs the byte-command queueing. It is
still not a DMM calibration source, not a runtime analog range writer for
`ms[0x02]`/`ms[0x03]`, and not proof of exact settle/discard timing.

The same script now also carries a selector state writer guard for the stock
digital DMM state machine. These sites prove stock RAM coupling around
`DAT_20001025` (`0x20001025`, selector), `DAT_2000102E` (`0x2000102e`, mode/range
shadow), `DAT_2000102F` (`0x2000102f`, display decimal shift), and
`DAT_20001027` (`0x20001027`, formatter substate). They are not analog
mux/range writers:

```text
0x08026FDE: init/reset clears selector/shadow state, including
            `strb.w r0, [sl, #0xf2d]` and `strb.w r0, [sl, #0xf2f]`
0x08036D14: RX classifier special branch writes `DAT_20001025 = 8`
0x08036D50: RX classifier B0/B1 branch writes `DAT_20001025 = 1`
0x08037220: RX branch writes `DAT_2000102E = 0`
0x080372E0: RX branch writes `DAT_2000102E = 0`,
            `DAT_2000102F = frame-derived bit`, `DAT_20001027 = 3`
0x08037328: RX branch writes `DAT_2000102E = 1`
0x08037338: RX branch writes `DAT_2000102E = 2`,
            `DAT_2000102F = 1 & ~frame_flag`, `DAT_20001027 = 1`
0x080373A8: RX branch writes `DAT_2000102E = 2`,
            `DAT_2000102F = frame-derived bit`, `DAT_20001027 = 2`
```

`FUN_080028E0` then reads `DAT_20001025` at `0x08002A9A` and dispatches the
formatter/unit cases listed below. This selector state writer guard proves the
digital stock DMM FSM around selector and formatter shadow bytes; it still does
not recover `ms[0x02]`/`ms[0x03]`, the analog frontend range writer, or a
factory calibration coefficient.

Important boundary for current ranges: the guarded `DAT_2000102e` sites are
variant shadow writer evidence, not a recovered current range writer. Stock
`FUN_080028E0` case 2 reads `DAT_2000102e` to choose current display unit index
`4` or `3`, so the current formatter reads `DAT_2000102e`; no inspected stock
path yet proves the physical current range source, a uA path, or an AC A
runtime writer.

The current formatter variant guard pins that display-only branch directly to
stock bytes:

```text
0x08002AFE: 98 f8 36 0f 01 28 26 d0 02 28 57 d1 03 20 00 e0
            05 20 98 f8 37 1f 88 f8 2e 0f 88 1c 88 f8 38 0f
            FUN_080028E0 case 2 reads DAT_2000102e; variant 2 selects
            DAT_20001026 = 3 and DAT_20001030 = DAT_2000102f + 2.
0x08002B54: 98 f8 37 1f 04 20 88 f8 2e 0f 88 f8 38 1f 2a e0
            Variant 1 branch target writes DAT_20001026 = 4 and
            DAT_20001030 = DAT_2000102f.
```

This is formatter/unit-state evidence only. It is deliberately not accepted as
a physical current range writer, current-jack safety proof, or factory
calibration coefficient.

Local port:

| Stock meter mode | Low byte | Raw word |
|---:|---:|---:|
| 0 | `0x14` | `0x0514` |
| 1 | `0x0C` | `0x050C` |
| 2 | `0x17` | `0x0517` |
| 3 | `0x0B` | `0x050B` |
| 4 | `0x0A` | `0x050A` |
| 5 | `0x12` | `0x0512` |
| 6 | `0x11` | `0x0511` |
| 7 | `0x10` | `0x0510` |

## Local Porting Map

The open firmware exposes eleven UI submodes, while the stock table has eight
stock slots. The current port therefore maps local UI state onto the recovered
stock slots below. This is a porting map, not proof that stock has eleven
separate modes.

| Local UI submode | Meaning | Stock slot | Selector | Extra apply word | Parser frame family |
|---:|---|---:|---:|---:|---|
| 0 | DC voltage | 0 | `0x0514` | none | voltage |
| 1 | AC voltage | 1 | `0x050C` | `0x050D` | voltage |
| 2 | DC current, small range | 2 | `0x0517` | `0x050E` | current |
| 3 | DC current, A range | 2 | `0x0517` | `0x050E` | current |
| 4 | AC current, small range | 3 | `0x050B` | none | current |
| 5 | AC current, A range | 3 | `0x050B` | none | current |
| 6 | resistance | 4 | `0x050A` | none | resistance |
| 7 | continuity | 6 | `0x0511` | `0x0516` | continuity |
| 8 | diode | 7 | `0x0510` | `0x0515` | diode |
| 9 | capacitance | 5 | `0x0512` | none | extended |
| 10 | temperature | 5 | `0x0512` | none | extended |

Production transition code sends `plan.selector_word` as the authoritative
stock command, and the USB/debug `expected selector` metadata now derives its
low byte from that same word. Do not rebuild the debug range byte from a second
selector-table lookup; that would let diagnostics drift from the actual
transition command.

The runtime tail is also modeled through the same transition plan. Valid local
submodes carry the stock-family start word `0x0509`, grounded at `0x08003BA4`,
plus the probe-detect tail (`0x0507` or `0x050A`, selected from live GPIOC bit
7); invalid submodes carry no selector, no apply word, no probe-detect tail,
and no start word. This keeps the production USART2 sequence and debug mirrors
tied to one software state-machine object. It is still command-sequence
evidence only: it does not recover exact settle/discard counts, a DMM-specific
`ms[0x02]`/`ms[0x03]` runtime analog range writer, or the low-DCV
frontend/calibration gap.

The live validation helper is intentionally narrower than the software state
matrix. On an energized voltage source it may switch only DCV (`mode meter 0 0`)
and ACV (`mode meter 1 0`) before returning to DCV; passive modes and current
modes are reported as not probed and rely on the parser/transition property
tests for stale/wrong-family rejection. Current live probing requires an
explicit current jack, series circuit, and load-limited source setup before the
helper may grow any current-mode command.

## Per-Submode Evidence Matrix

The table below separates stock-disassembly evidence from local policy. `High`
means the selector/display path is directly recovered from stock code or literal
tables; `Medium` means the stock slot is recovered but the local eleven-submode
split projects onto fewer stock slots; `Low` marks behavior that remains a local
conservative transition policy until stock runtime state writes or bench traces
prove the exact rule.

| Local submode | Stock selector evidence | Port C/E mux evidence | Port A/B mux evidence | Transition evidence | Confidence / gap |
|---:|---|---|---|---|---|
| 0 DCV | stock slot `0`, selector `0x0514`; stock case 0/DCV formatter in `full_decompile.c` around `0x080028E0` | projects stock slot `0` into `gpio_mux_portc_porte`; function recovered at `0x080018A4` | projects stock slot `0` into `gpio_mux_porta_portb`; function recovered at `0x08001A58` and writes PA15/PA10/PB10/PB11 | local reset + 20 ms settle + selector + 2-frame discard; stock proves command pacing/filtering, not exact constants | High selector; Medium mux projection; Low exact settle/discard proof |
| 1 ACV | stock slot `1`, selector `0x050C`, apply `0x050D`; ACV case at `0x08037228` reads `frame[7].0` | projects stock slot `1` | projects stock slot `1` | same local transition policy plus apply word | High selector/formatter; Medium mux; Low exact settle/discard proof |
| 2 DC mA | stock slot `2`, selector `0x0517`, apply `0x050E`; stock current formatter evidence distinguishes DCA unit indices | projects stock slot `2` | projects stock slot `2` | same local transition policy | Medium: stock current slot recovered, local small-current split not separately selector-proven |
| 3 DC A | same stock slot `2`, selector `0x0517`, apply `0x050E` | projects stock slot `2` | projects stock slot `2` | same local transition policy | Medium/Low: local A-range policy over shared stock slot; runtime range writer still missing |
| 4 AC mA | stock slot `3`, selector `0x050B`; stock case 3 writes ACA display/unit state | projects stock slot `3` | projects stock slot `3` | same local transition policy | Medium: ACA-like slot recovered |
| 5 AC A | same stock slot `3`, selector `0x050B` | projects stock slot `3` | projects stock slot `3` | same local transition policy | Low/Medium: AC A is local policy until stock A-range ACA evidence appears |
| 6 resistance | stock slot `4`, selector `0x050A`; stock case 4 formatter/unit state | projects stock slot `4` | projects stock slot `4` | same local transition policy | High selector; Medium mux |
| 7 continuity | stock slot `6`, selector `0x0511`, apply `0x0516`; continuity segment marker path is parser-visible | projects stock slot `6` | projects stock slot `6` | same local transition policy plus apply word | High selector; Medium mux |
| 8 diode | stock slot `7`, selector `0x0510`, apply `0x0515` | projects stock slot `7` | projects stock slot `7` | same local transition policy plus apply word | High selector; Medium mux |
| 9 capacitance | stock slot `5`, selector `0x0512`; stock case 5 has capacitance-like `digit_count + 2` formatting | projects stock slot `5` | projects stock slot `5` | same local transition policy | Medium: extended slot recovered; cap/temp split not separately selector-proven |
| 10 temperature | same stock slot `5`, selector `0x0512`; stock mode-5 path has Fahrenheit conversion clue and adjacent `32.0f` literal | projects stock slot `5` | projects stock slot `5` | same local transition policy | Medium/Low: conversion clue exists, separate selector not proven |

## Current Range Evidence Boundary

Stock selector slots 2 and 3 are the only current slots recovered from the
eight-byte command table:

| Stock slot | Selector | Stock formatter evidence | Local use |
|---:|---:|---|---|
| 2 | `0x0517` | `full_decompile.c` case 2 writes display unit state `4` for one DCA range and `3` for another, keyed by `DAT_2000102e`. The unit lookup boundary guard proves `0x0804C40C` is not a recovered stock unit string table, so mA/A suffix text remains local/inferred. | local DC current small range and DC A range |
| 3 | `0x050B` | `full_decompile.c` case 3 writes display unit state `5`; suffix text for that unit index is local/inferred because the downloaded APP image has a zero-filled lookup region rather than recovered stock strings. | local AC current small range; AC A is only local policy until proven |

No inspected stock path proves a separate uA selector, and no inspected AC path
proves an A-range ACA formatter. The stock evidence so far distinguishes DC
current ranges through frame/display unit state, not through additional
command-table slots. Until the runtime writer for the stock range state is
recovered or bench-proven, uA is unresolved and unexposed in the local UI, while
AC A remains parser/UI policy on top of the recovered ACA current slot. The
UI/submode surface guard now makes that absence explicit: there is no recovered
uA local submode, the UI mode table remains 11 entries, and autoscan must not
add a hidden microamp candidate without new stock/runtime evidence.

The open-firmware state-machine model now carries a separate logical DMM
function matrix so the "all modes" requirement includes the unresolved
microamp families instead of forgetting them. Guard label: logical DMM function matrix.
The logical functions are: DCV, ACV, DC uA,
DC mA, DC A, AC uA, AC mA, AC A, resistance, continuity, diode, capacitance,
and temperature. DC uA and AC uA map to `FPGA_METER_INVALID_LOCAL_SUBMODE`;
every other logical function maps to the current 0..10 local submode table above.
`test_fpga_meter_plan` guards the complete 13-function matrix, and
`test_meter_auto` guards that the unresolved uA entries cannot become autoscan
candidates or score as current readings. Changing that requires new stock
selector/formatter/mux evidence or safe current-jack, series-load-limited live
traces.

## Voltage Frame-Family Marker Boundary

Low-DCV live and synthetic failure frames use both `frame[8]=0x80` and
`frame[8]=0x82` forms. In both, bit 7 is the stock range/status input that
selects stock decimal class `4`; in the `0x82` form the low seven bits also
carry the common DCV/voltage marker `0x02`. Therefore both `0x80` and `0x82`
with `frame[9]=0x00` must be treated as voltage-family payloads for wrong-mode
rejection unless a later stock xref proves another family owns that bit.

This is a frame-metadata rule, not a value-recognition rule. The decoder must not
infer voltage/current/passive family from the BCD count looking plausible.

## Observed Frame-Family Classification Boundary

The stock-visible cross-family markers recovered so far are deliberately narrow:

- voltage-family metadata in `frame[8]`/`frame[9]`, including the low-DCV
  `0x80`/`0x82` class-4 forms;
- the continuity segment marker (`raw_digits[1] == 0x12`,
  `raw_digits[2] == 0x0A`, `raw_digits[3] == 5`).

No inspected stock xref yet proves a current, resistance, diode, capacitance, or
temperature frame-family marker that can be applied independently of the active
DMM transition plan. The local parser therefore treats an unclassified normal
digit frame as belonging to the active local transition plan only after the
frontend/mode transition has selected that submode. This is not proof that the
frame physically originated from that family; it is a conservative parser
boundary until stock metadata or a repeatable live trace recovers more markers.

The C property test
`unclassified_normal_frames_follow_active_family_only` guards this unresolved
boundary across all eleven local submodes. It prevents future work from adding
a synthetic "current-looking", "resistance-looking", diode, or extended-family
classifier based on BCD magnitude, `frame[6]` folklore, or convenient unit text.
Wrong-family rejection is currently executable for marker-visible foreign frames
(voltage and continuity). The current/resistance/diode/extended cross-family
marker gap remains open and must be solved from stock frame metadata, stock
xrefs, or safe live traces rather than guessed in the decoder.

The production state-machine model now keeps that distinction explicit:
`fpga_meter_frame_family_is_recovered()` means the active transition plan has a
stock selector/formatter family for the selected local submode, while
`fpga_meter_frame_family_has_stock_marker()` means a foreign RX frame can be
recognized without trusting the active plan. The marker-visible set is only
voltage and continuity. Current, resistance, diode, and extended families are
active-plan-only until new stock metadata or safe live traces prove their
independent frame markers. `test_fpga_meter_plan` guards this boundary with
`frame_family_marker_visibility_documents_observed_gaps`.

## Magnitude-Derived Range Feedback Boundary

Older bring-up comments suggested MCU-side auto-ranging by watching for BCD
overflow/underflow and then sending higher or lower range commands. That path is
now explicitly rejected for the DMM goal. Stock-visible DCV scaling is
metadata-driven (`frame[8].7`, `frame[3].4`, `frame[4].4`, `frame[5].4`, plus
the `frame[2].3` raw extension) and active-state driven, not a value-shape classifier.
The low-DCV blocker proves why this matters: the frame
`5A A5 44 8E EF E7 07 24 80 00 01 89` decodes stock-consistently to about
`0.4366 V` while the visual source/load display showed `0.200 V`; changing
range commands based on the decoded number would hide the missing
frontend/H2/acceptance evidence instead of recovering it.

The production RX path therefore keeps range feedback disabled unless future
stock xrefs or safe live traces recover an actual frontend/range command path.
`scripts/validate_dmm_goal.py` now rejects the stale BCD-overflow/underflow TODO
phrasing in firmware drivers/UI, so future work cannot reintroduce
magnitude-derived relay/range control as an attractive shortcut.

## Extended Slot 5 Evidence Boundary

Stock slot 5 is solidly recovered as selector `0x0512`. The meaning of the
local capacitance and temperature split is narrower:

- `fpga_task_annotated.c` records the stock result-formatting switch case 5 as
  `digit_count + 2` for capacitance-like formatting.
- `meter_math_pipeline_annotated.c` contains a mode-5 conversion path using
  `value = value * 9 / 5 + 32` when the flag at `+0xF39` is set, with a
  Fahrenheit `32.0f` literal nearby.
- The display formatter dispatch guard binary-pins `FUN_080028E0` switch
  slices at `0x08002AA0`, `0x08002B20`, and `0x08002B34`: stock writes
  `DAT_20001026` unit index `7` and `DAT_20001030 = DAT_2000102f + 9` for
  mode 5, and `DAT_20001026` unit indices 8/9/10/11 with
  `DAT_20001030 = DAT_2000102f + 10`, i.e. format offsets +9/+10, for
  modes 6/7.
- The local port maps both capacitance and temperature to stock slot 5 and
  parses both as the extended frame family.

That is evidence for a shared extended slot, not proof that stock exposes
separate capacitance and temperature selector modes matching the open
firmware's eleven UI submodes.

## Transition Timing Evidence Boundary

Stock evidence supports command pacing and frame filtering:

- `fpga_task_annotated.c` shows the TX interrupt enabled, followed by a 10-tick
  delay before accepting the next command.
- `full_decompile.c` and `usart2_isr_state_machine.md` show USART2 framing that
  accepts `0x5A/0xA5` data frames, validates `0xAA/0x55` echo frames, and drops
  invalid echo/data sequences.
- `meter_math_pipeline_annotated.c` marks `{any, 0x13, 0x14, 1..3}` as pending
  auto-range/mode-transition data.

No inspected stock path proves a fixed "discard exactly N frames" or "settle
exactly 20 ms" window after every mode switch. The open firmware's current
two-frame discard plus 20 ms settle is a conservative local transition policy
that should be replaced only when a stock path or repeatable bench capture
proves the exact rule. The transition settle/discard policy guard now pins this
as a uniform local settle/discard policy for all valid local submodes; invalid
submodes emit no settle/discard, no selector, and no apply word. Exact stock
settle/discard counts remain open.

The open firmware now routes both `fpga_set_meter_mode()` and
`fpga_meter_reinit()` through one `fpga_apply_meter_transition()` production
path: invalidate the active reading, reset/drain the meter transport, optionally
send the wake preamble used by reinit, apply the table-driven frontend mux
state, wait the local settle window, send selector/apply/probe/start words from
the transition plan, and arm the planned discard budget. This unifies the
software state machine around the stock-visible transport shape; it is not proof
of exact stock settle counts, the unrecovered runtime `ms[0x02]`/`ms[0x03]`
analog writer, or any factory meter coefficient.

## GPIO Mux Evidence Boundary

Stock master init calls the two GPIO mux functions from saved meter state:

```text
0x08025544: ldrb r0, [r4, #2]  ; ms[0x02]
0x08025546: bl   0x080018a4    ; gpio_mux_portc_porte
0x0802554a: ldrb r0, [r4, #3]  ; ms[0x03]
0x0802554c: bl   0x08001a58    ; gpio_mux_porta_portb
```

The same pair repeats after the later probe/attenuation restore block at
`0x0802723e..0x0802724a`. `gpio_mux_portc_porte` controls PC12 and PE4/PE5/PE6;
`gpio_mux_porta_portb` controls PA15, PA10, PB10, and PB11: the decompile at
`0x08001a58..0x08001bba` writes GPIOA bit `0x8000`, GPIOA bit `0x400`, GPIOB bit
`0x400`, and GPIOB bit `0x800` through the BOP/BCR registers. The current
open-firmware transition plan now represents these as separate
`portc_porte_mux` and `porta_portb_mux` fields instead of treating the stock
function/range selectors as one generic mux.

`scripts/test_stock_meter_literals.py` binary-guards both saved-state apply
sites so this evidence cannot drift silently:

```text
0x08025544: a0 78 dc f7 ad f9 e0 78 dc f7 84 fa
0x0802723e: 9a f8 02 00 da f7 2f fb 9a f8 03 00 da f7 05 fc
```

The persistent saved-config unpack path is also now covered by the
`saved-config meter-state unpack guard`.  This is the stock writer that feeds
the saved-state apply sites above:

```text
0x08025D92: ldr r0, [r4]           ; r4 = 0x08006000 persistent config
0x08025D98: uxtb r1, r0
0x08025D9A: cmp r1, #0x55          ; normal valid signature
0x08025DA2: cmp r1, #0xAA          ; valid signature saved from meter mode
0x08025DA8: movs r1, #8
0x08025DAA: strb.w r1, [sl,#0xf68] ; if 0xAA, set mode_state = 8
0x08025DAE: lsrs r1, r0, #8
0x08025DB0: lsrs r2, r0, #16
0x08025DB2: lsrs r0, r0, #24
0x08025DB4: strb.w r1, [sl]        ; saved byte[1] -> ms[0x00]
0x08025DB8: strb.w r2, [sl,#1]     ; saved byte[2] -> ms[0x01]
0x08025DBC: strb.w r0, [sl,#2]     ; saved byte[3] -> ms[0x02]
0x08025DC0: ldr r0, [r4,#4]
0x08025DC2: str.w r0, [sl,#3]      ; saved word[1] -> ms[0x03..0x06]
```

The guarded bytes are:

```text
0x08025D92:
  20 68 40 f2 f8 0a c1 b2 55 29 c2 f2 00 0a 05 d0
  aa 29 40 f0 f8 81 08 21 8a f8 68 1f 01 0a 02 0c
  00 0e 8a f8 00 10 8a f8 01 20 8a f8 02 00 60 68
  ca f8 03 00
```

These bytes prove the persistent saved-config writer plus the two
boot/saved-state apply sequences only. They still do not prove a runtime DMM
writer that changes `ms[0x02]`/`ms[0x03]` while the user switches local DMM
ranges.

The narrower `saved-config live mux-store guard` names the direct live RAM
stores explicitly, so this evidence cannot be blurred into a guessed runtime
DMM range writer:

```text
0x08025D94 saved_config_live_mux_store:
  40 f2 f8 0a c1 b2 55 29 c2 f2 00 0a 05 d0 aa 29
  40 f0 f8 81 08 21 8a f8 68 1f 01 0a 02 0c 00 0e
  8a f8 00 10 8a f8 01 20 8a f8 02 00 60 68 ca f8
  03 00
```

This slice materializes `sl = 0x200000f8`, checks signatures `0x55`/`0xAA`,
writes `ms[0x02]` at `0x08025DBC`, then writes `ms[0x03..0x06]` at
`0x08025DC2`.  It is stock evidence for saved-config unpack and for
boot/restore direct live mux stores, not a runtime DMM range writer.

The paired persistent pack path is now guarded as well. The
`saved-config meter-state pack guard` covers `FUN_080223BC` (`0x080223BC`),
which allocates a 512-byte save buffer and, when called with signature `0x55`,
reads the current meter-state bytes from `0x200000F8`:

```text
0x0802241A: ldrb r2, [r7,#0]       ; ms[0x00]
0x08022426: ldrb r6, [r7,#2]       ; ms[0x02]
0x08022448: ldrb r3, [r7,#1]       ; ms[0x01]
0x0802258A: orr.w r4, ip, lr       ; signature plus ms[0x00]
0x08022594: str.w r6, [sl]         ; persistent word 0
0x08022598: ldr.w r6, [r7,#3]      ; ms[0x03..0x06]
0x0802259E: str.w r6, [sl,#4]      ; persistent word 1
```

The guarded stock byte slices are:

```text
0x08022410 saved_config_meter_state_pack_reads:
  39 7e 97 f8 2d 90 05 91 79 8b 3a 78 06 91 4f ea
  09 21 07 91 b9 8b be 78 09 04 0d 91 b7 f8 34 12
  4f ea 02 2c 09 04 03 91 b7 f8 32 12 32 06 0a 92
  fa 7d 04 91 b7 f8 36 12 7b 78 3d 7d bc 7d 12 06
  00 91 b7 f8 38 12 1e 04

0x080224A0 saved_config_meter_state_default_seed:
  00 21 c0 f2 05 51 4c f6 32 62 c7 e9 00 12 03 22
  3a 75 4f f4 80 72 fa 82

0x0802258A saved_config_meter_state_pack_writes:
  4c ea 0e 04 26 43 0a 9c 26 43 ca f8 00 60 d7 f8
  03 60 09 9c ca f8 04 60 fe 79 26 43 32 43 08 9e
  32 43 ca f8 08 20 07 9a 05 9e 32 43
```

The default seed branch at `0x080224A0` writes `0x05050000` to `[r7]`, so stock
defaults `ms[0x02] = 5` and `ms[0x03] = 5` before later default fields are
filled. This is a saved-config pack/default guard: it proves persistence layout
and default mux-state bytes, but still not a runtime DMM range writer.

The raw direct-BL sweep for this packer is now guarded too. It found four
BL-shaped stock hits to `FUN_080223BC`: `0x08002F8C`, `0x08002FE2`,
`0x08005B4A`, and `0x0803972E`. The classification matters more than the raw
count: a sweep over the whole image sees literal pools and branch islands, so
these are not automatically executable DMM range writers.

The first executable-looking call is a housekeeping threshold path near the end
of the stock `FUN_08002C78` region. It reaches `FUN_080223BC(0x55)` at
`0x08002F8C` after the surrounding counter/state checks:

```text
0x08002F80 housekeeping threshold saved-config pack caller:
  6d af 7b e7 09 29 de d1 e7 e7 55 20 1f f0 16 fa
```

The next raw hit, `0x08002FE2`, sits inside the post-function literal/data
region between the `0x08002F8C` call tail and the next function prologue. It is
guarded as direct-BL-shaped bytes, not as a classified executable DMM caller:

```text
0x08002F90 post-function literal/data region with BL-shaped bytes:
  cd cc cc cc cc cc 4c 3f 3d 0a d7 a3 70 3d 10 40
  d7 a3 70 3d 0a d7 0f 40 5c 8f c2 f5 28 5c 0f 40
  f6 28 5c 8f c2 f5 0e 40 8f c2 f5 28 5c 8f 0e 40
  52 b8 1e 85 eb 51 0e 40 29 5c 8f c2 f5 28 0e 40
  ec 51 b8 1e 85 eb 0d 40 71 3d 0a d7 a3 70 0d 40
  55 20 1f f0 eb f9 00 00
```

The `0x08005B4A` hit is likewise guarded as direct-BL-shaped bytes in a branch
island immediately before the selector seed function at `0x08005B50`:

```text
0x08005B40 branch island before selector seed:
  bd e8 f0 41 35 f0 d4 b8 00 20 1c f0 37 fc 00 00
  2d e9 f0 43 81 b0 40 f2 f8 05
```

Finally, `probe_change_handler` (`0x080396C8..0x08039734`) increments the
auto-power-off/probe-change counter at `ms[0xF6C]`, checks threshold constants
`0x0384`, `0x0708`, and `0x0E10`, then calls `FUN_080223BC(0x55)` at
`0x0803972E` only on the controlled shutdown/config-save path:

```text
0x080396F4 saved-config pack caller guard:
  a0 b1 b1 f8 6c 2f 03 28 02 f1 01 02 a1 f8 6c 2f
  08 d0 02 28 0b d0 01 28 08 d1 90 b2 b0 f5 61 7f
  04 d9 09 e0 90 b2 b0 f5 61 6f 05 d8 80 bd 90 b2
  b0 f5 e1 6f 98 bf 80 bd 55 20 e8 f7 45 fe 00 00
```

This `saved-config pack caller guard` makes the boundary explicit: the packer
is stock evidence for persistence, housekeeping save, and power-off save
behavior, not normal runtime DMM range switching. The missing runtime DMM
evidence remains a writer or trace that ties live DMM selector/range transitions
to `ms[0x02]` and `ms[0x03]`, or proves that those bytes are not the runtime
DMM range source. In short: none of the guarded `FUN_080223BC` direct-BL-shaped
hits is recovered as a normal runtime DMM range writer.

### USART TX Config Writer Meter-Case Guard, 2026-06-06

`FUN_08039734` has a seven-arm `TBB` dispatch that older notes describe as
`usart_tx_config_writer`. One arm is meter-case-shaped (`cmd_type == 4`) and
writes parameter bits into config words, but the visible direct callers in
`FUN_08023A50` pass timer base addresses (`TIM5_CTL0` and `TIM2_CTL0`) during
master init. Treat this as a separate FPGA config bitfield path, not as a
recovered normal DMM runtime range source.

The binary guard covers the dispatch prologue, the `0x080397C8` meter-case arm,
the common epilogue that ORs the `0x0100 update mask`, and the complete direct callsite set.
The only recovered direct `BL` instructions to `FUN_08039734` are the two init
callers below (`0x080272D4` and `0x08027344`); no runtime DMM caller is recovered:

```text
0x08039734 writer_tbb_prologue:
  10 b5 0a 78 06 2a 88 bf 10 bd df e8 02 f0
  04 98 1c 98 43 98 6c 00

0x080397C8 meter_case_bitfield_body:
  02 46 91 f8 01 c0 52 f8 20 3f 0c f0 01 0c
  23 f4 00 73 43 ea 4c 23 13 60 91 f8 01 c0
  d2 f8 00 e0 cc f3 54 03 63 f3 cb 2e c2 f8
  00 e0 91 f8 02 c0 50 f8 1c 3f 4f f4 80 74
  6c f3 01 03 03 60 c9 78 03 68 09 01 5f fa
  81 fc 23 f0 f0 0e 6f f0 0c 03 22 e0

0x08039860 writer_common_update_mask_commit:
  4e ea 0c 01 01 60 01 68 19 40 01 60
  10 68 20 43 10 60 10 bd

0x080272CC tim5_init_config_writer_call:
  4f f4 80 30 10 90 38 46 12 f0 2e fa
  b8 68 05 21 61 f3 06 10 b8 60

0x08027338 tim2_init_config_writer_call:
  02 20 c0 f2 01 00 10 90 4f f0 80 40 12 f0 f6 f9
```

Decoded from the stock disassembly, the `cmd_type == 4` arm writes
`params[1].0` into `[r0+0x20]` bit 9, `params[1].1` into `[r0+0x20]` bit 11,
`params[2]` low two bits into `[r0+0x1C]` bits 0..1, and `params[3]` shifted
into `[r0+0x1C]` bits 4..7, then commits update mask `0x0100`. That is useful
stock hardware/config shape evidence. It still does not explain why the
low-DCV live frame `5A A5 44 8E EF E7 07 24 80 00 01 89` reports stock math
`0.4366 V` while the visible source is `0.200 V`, and it is not a license to
add a multiplier, current/voltage magnitude classifier, or guessed factory
coefficient.

Open gap: find the DMM-owned runtime caller or trace that feeds this bitfield
path from the eight-entry selector table, or prove that DMM runtime mode/range
selection uses another writer entirely. Until that exists, `USART TX config writer meter-case guard` is evidence for a separate FPGA config bitfield path; visible direct callers are TIM5/TIM2 init, not normal DMM runtime range switching.

The unresolved part is the live/runtime writer for `ms[0x03]` during local
small-current versus A-range operation. Until that is recovered or bench-proven,
local submodes 2/3 and 4/5 share the same recovered stock current slot and are
split only by parser/UI range state. Treat current readings that still look like
voltage payloads as a frontend activation failure, not a decimal decoder issue.

### Mux Writer Xref Audit, 2026-06-06

The text decompile/xref pass found no DMM-specific runtime writer that maps the
eight recovered DMM selector words directly to unique `ms[0x02]` and `ms[0x03]`
bytes. A halfword-aligned binary sweep found all 16 direct `BL` callsites to
the two mux writers, and `scripts/test_stock_meter_literals.py` now carries a
mux callsite guard so this list cannot silently shrink back to a partial
function-map view. The guard independently scans the whole APP image for direct
Thumb `BL` callers to `0x080018A4` and `0x08001A58`, then requires the scanned
callers to match the documented list exactly:

```text
gpio_mux_portc_porte target 0x080018A4:
  0x080020B2: ff f7 f7 fb
  0x080031E8: fe f7 5c fb
  0x080039A2: fd f7 7f ff
  0x0801A53E: e7 f7 b1 f9
  0x0801C7CC: e5 f7 6a f8
  0x0801D094: e4 f7 06 fc
  0x08025546: dc f7 ad f9
  0x08027242: da f7 2f fb

gpio_mux_porta_portb target 0x08001A58:
  0x08001F06: ff f7 a7 fd
  0x08003644: fe f7 08 fa
  0x08003E3A: fd f7 0d fe
  0x0801A534: e7 f7 90 fa
  0x0801C7D8: e5 f7 3e f9
  0x0801D0A0: e4 f7 da fc
  0x0802554C: dc f7 84 fa
  0x0802724A: da f7 05 fc
```

The evidence splits like this:

| Evidence | Stock offsets / files | Classification |
|---|---|---|
| `gpio_mux_portc_porte` body | `FUN_080018a4`, `full_decompile.c:2206..2295`; 10-way `param_1` switch writing GPIOC/E plus DAC calibration tables | hardware writer, stock-proven |
| `gpio_mux_porta_portb` body | `FUN_08001a58`, `full_decompile.c:2300..2365`; 10-way `param_1` switch writing GPIOA/B pins including PB11 | hardware writer, stock-proven |
| saved-state restore/apply | `0x08025544..0x0802554c` and `0x0802723e..0x0802724a` load saved `ms[0x02]`/`ms[0x03]` then call both mux writers | DMM-relevant boot/saved-state evidence |
| direct decompile callers | `function_names.md` lists `FUN_08001c60` as `siggen_configure` and `FUN_08019e98` as `scope_main_fsm`; `function_map_complete.txt` lists only callers `08001c60,08019e98` for both mux writers even though the binary guard proves more BL sites | decompile/function-map limitation; not enough for DMM selector proof |
| `FUN_08001c60` scope/siggen channel setup | `0x08001F06` / `0x080020B2`; `full_decompile.c:2564..2574` increments `(&DAT_200000fa)[uVar20]`, then calls `FUN_080018a4(DAT_200000fa)` for channel 0 or `FUN_08001a58(DAT_200000fb)` for channel 1 and queues command `4` | scope/siggen auto-range path; not DMM |
| scope/preset mux owner handlers | `0x08003148` and `0x08003900` wrap the callsites `0x080031E8` / `0x08003644` and `0x080039A2` / `0x08003E3A`; binary disassembly shows they increment/decrement `(&DAT_200000fa)[DAT_2000044c >> 7]`, call `FUN_080018a4` for channel 0 or `FUN_08001a58` for channel 1, then queue command `4` | scope/preset UI mux owners; not DMM runtime range proof |
| scope UI mux-LUT consumer | `0x080151B0..0x080151F2` in `FUN_08015f50`/`scope_ui_draw_main`; reads `DAT_2000010e`, loads `(&DAT_200000fa)[idx]`, derives the modulo-3 scale index, then reads `DAT_0804bfb8` | scope render/scale consumer; not a DMM range writer |
| scope main auto-range write | `0x0801A534` / `0x0801A53E`; `full_decompile.c:6880..6999` scans sample buffers, enters range selection, then reuses `DAT_200000fa/DAT_200000fb` for DAC/calibration recompute; `full_decompile.c:8744..8752` performs the actual mux call after `(&DAT_200000fa)[uVar70] = bVar37 + 1` | oscilloscope acquisition path; not DMM |
| explicit scope-submode mux calls | `0x0801C7B8..0x0801C7D8` and `0x0801D088..0x0801D0A0` (`full_decompile.c:7564..7565` and `7988..7989`) read `DAT_20000128`/`state[0x30]`, mask `& 0x0f`, and call both mux writers; `scope_main_fsm_annotated.c` names that byte as scope sub-mode | scope runtime reconfiguration, not DMM |
| DAC1 writes | `FUN_080018a4` at `0x080018A4..0x08001A52` and inline recomputes at `full_decompile.c:2603..2624`, `6960..7020`, `7771..7990` write `0x40007408` from scope calibration tables | scope trigger/comparator threshold; not DMM calibration |
| waveform calibration/render use | `full_decompile.c:8611..8624`, `9840..9971` index `DAT_080465cc` and calibration deltas through current and saved `DAT_200000fa/DAT_200000fb` | scope display/calibration path, not DMM selector proof |

### Mux Writer Literal-Pointer Negative Guard, 2026-06-06

The direct `BL` sweep above is now paired with a whole-APP literal-pointer
sweep. `scripts/test_stock_meter_literals.py` scans for both even and Thumb-bit
32-bit forms of the two mux writer targets:

```text
gpio_mux_portc_porte: 0x080018A4 and 0x080018A5
gpio_mux_porta_portb: 0x08001A58 and 0x08001A59
```

The current V1.2.0 APP image has no static 32-bit literal/function-pointer refs
to either mux writer target. This matters because the stock firmware does have
real `ldr`/`blx` dispatch surfaces elsewhere; without this negative guard, a
future pass could claim an unverified hidden function-pointer table as the
missing DMM range path. The boundary remains narrow: this does not prove there
is no computed or state-mediated path to the mux writers. Any computed or
state-mediated path still requires a new trace, xref owner, or binary guard
before being used as DMM runtime range evidence.

A computed or state-mediated path still requires a new trace before it can be
used as DMM runtime range evidence.

### Mux Writer Body Guard, 2026-06-06

`scripts/test_stock_meter_literals.py` now also binary-guards representative
slices inside the two mux writer bodies, not only their callsites:

```text
gpio_mux_portc_porte / FUN_080018a4:
  0x080018A4 switch prologue: 09 28 00 f2 88 80 df e8 ...
  0x080018C4 gpio_pc12_pe_write_block: GPIOC/E BOP/BCR writes for PC12 and PE pins
  0x080019BA scope_calibration_table_select: indexes scope calibration tables
  0x08001A20 DAC1/scope calibration tail: updates 0x40007408/0x40007404

gpio_mux_porta_portb / FUN_08001a58:
  0x08001A58 switch prologue: 09 28 00 f2 bb 80 df e8 ...
  0x08001A78 gpio_pa15_pb11_pb10_write_block: GPIOA/B writes for PA15/PB11/PB10
  0x08001B82 gpio_high_modes_write_block: higher mux modes writing PA/B pins
  0x08001BD4 scope_calibration_table_select: indexes scope calibration tables
  0x08001C3A DAC1/scope calibration tail: updates scope DAC state
```

This mux writer body guard proves that the functions behind `ms[0x02]` and
`ms[0x03]` really are 10-way GPIO hardware writers with scope-calibration/DAC1
tails. It deliberately does not prove that any inspected DMM runtime branch
writes those bytes during local range switching, and it does not turn the DAC1
tail into a DMM calibration coefficient.

### Stock Mux Arm Truth Table Guard, 2026-06-06

The mux writers themselves have ten switch arms (`0..9`) even though the
currently recovered DMM selector table exposes only eight stock slots. The open
firmware now keeps a pure model helper,
`fpga_meter_mux_gpio_state_for_stock_mux_arms(portc_porte_mux,
porta_portb_mux, out)`, so the whole switch-arm truth table is unit-tested
separately from local UI submode policy.

Offsets/evidence:

- `FUN_080018a4` / `0x080018A4` (`gpio_mux_portc_porte`) projects the
  `ms[0x02]` arm into PC12/PE4/PE5/PE6.
- `FUN_08001a58` / `0x08001A58` (`gpio_mux_porta_portb`) projects the
  `ms[0x03]` arm into PA15/PA10/PB10/PB11.
- `scripts/test_stock_meter_literals.py` binary-guards both switch prologues
  and representative GPIO write blocks; `test_fpga_meter_plan` now guards the
  resulting 10-arm GPIO model.

When the same arm number is applied to both mux writers, the projected GPIO
levels are:

| Stock mux arm | PC12 | PE4 | PE5 | PE6 | PA15 | PA10 | PB10 | PB11 | PB9 | PA6 | Boundary |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 0 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 1 | 0 | 0 | Used by local DCV projection |
| 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | Used by local ACV projection |
| 2 | 1 | 1 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | Shared local DC-current projection |
| 3 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | Shared local AC-current projection |
| 4 | 1 | 1 | 1 | 0 | 1 | 0 | 1 | 1 | 0 | 0 | Used by local resistance projection |
| 5 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | Shared local capacitance/temperature projection |
| 6 | 0 | 1 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | Used by local continuity projection |
| 7 | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 0 | 0 | 0 | Used by local diode projection |
| 8 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | Recovered stock switch arm, not mapped to a local DMM selector |
| 9 | 0 | 1 | 1 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | Recovered stock switch arm, not mapped to a local DMM selector |

Arms `8` and `9` are important negative/unfinished evidence. They prove the
stock mux writers have more hardware states than the recovered DMM selector
slots, but they do not by themselves identify a low-DCV range, current jack
range, capacitance path, or temperature path. A future physical correction must
recover a stock DMM-owned writer or trace that selects a specific arm before
using either state in production.

### Mux GPIO State Projection Guard, 2026-06-06

The open firmware now exposes a pure `fpga_meter_mux_gpio_state_for_submode`
model so the stock mux projection can be unit-tested without touching hardware.
The model starts from the same baseline used by
`fpga_set_meter_frontend_for_submode`: PC12 high, PE4 high, PE5 low, PE6 high,
PA15 high, PA10 high, PB10 low, PB11 high, with PB9 and PA6 kept low by the
auxiliary AFE boundary below. It then applies the 10-way switch bodies from
`FUN_080018a4` (`0x080018A4..0x080019B6`) and `FUN_08001a58`
(`0x08001A58..0x08001BAA`).

Final projected levels for the local DMM submodes are:

| Local submode | Stock slot | PC12 | PE4 | PE5 | PE6 | PA15 | PA10 | PB10 | PB11 | PB9 | PA6 | Evidence/gap |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 0 DCV | 0 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 1 | 0 | 0 | Stock slot 0 projection |
| 1 ACV | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | Stock slot 1 projection |
| 2 DC current small | 2 | 1 | 1 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | Shares recovered stock slot 2 |
| 3 DC current A | 2 | 1 | 1 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | Local split, no extra stock slot recovered |
| 4 AC current small | 3 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | Shares recovered stock slot 3 |
| 5 AC current A | 3 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | Local split, no extra stock slot recovered |
| 6 Resistance | 4 | 1 | 1 | 1 | 0 | 1 | 0 | 1 | 1 | 0 | 0 | Stock slot 4 projection |
| 7 Continuity | 6 | 0 | 1 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | Stock slot 6 projection |
| 8 Diode | 7 | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 0 | 0 | 0 | Stock slot 7 projection |
| 9 Capacitance | 5 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | Shares recovered stock slot 5 |
| 10 Temperature | 5 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | Local split, no extra stock slot recovered |

This is a mux GPIO state projection guard: it makes the current stock-slot
projection explicit and testable, and the production DMM frontend apply path now
uses this same `fpga_meter_mux_gpio_state_for_submode` model before writing GPIO
BOP/BCR registers. That removes the earlier duplicated switch body in
`fpga.c`, so the tested state-machine table and hardware-facing writes cannot
silently diverge.

The projection still remains a policy boundary. No currently recovered DMM
branch proves a separate runtime writer for small/A current,
capacitance/temperature, or a low-DCV physical range. A future change must
update the stock-offset evidence, this table, and the unit tests together
instead of reacting to a surprising reading with decoder-side coefficients.

### Auxiliary AFE PB9/PA6 Guard, 2026-06-06

The stock master-init sequence leaves PB9 and PA6 configured as outputs only,
but the current stock xrefs show no recovered stock BOP/BCR level write for
either pin. The guarded stock bytes are:

```text
0x080241D4 auxiliary AFE PB9 output config; 0x080241E2 PA6 output config:
  4f f4 00 70 18 90 28 46 21 46 0c f0 8d f8
  40 20 18 90 40 f6 00 00 c4 f2 01 00 21 46 0c f0 84 f8
```

This starts with the PB9 pin mask `0x0200`, calls `gpio_pin_config`
(`FUN_080302FC`) on the GPIOB base saved in `r5`, then loads the PA6 pin mask
`0x0040`, materializes GPIOA base `0x40010800`, and calls the same GPIO
configuration helper. In `full_decompile.c`, the currently recovered direct
level-write forms are absent:

```text
_DAT_40010c10 = 0x200  ; PB9 set high
_DAT_40010c14 = 0x200  ; PB9 set low
_DAT_40010810 = 0x40   ; PA6 set high
_DAT_40010814 = 0x40   ; PA6 set low
```

So this is configuration evidence only. The open firmware now applies PB9/PA6
through the same tested mux GPIO state model as the recovered stock slot pins,
and that table keeps them in the stock-reset/output-low state instead of
asserting the old bench-inferred high level. If a later stock trace proves
mode-specific PB9/PA6 levels, that evidence must update the table-driven
transition plan and this guard together.

### Runtime Mux-State Writer Guard, 2026-06-06

The text decompile currently exposes only two runtime writes to the
`DAT_200000fa`/`DAT_200000fb` mux-state pair, and
`scripts/test_stock_meter_literals.py` now binary-guards both:

```text
0x08001EE8: `FUN_08001c60` increments `(&DAT_200000fa)[uVar20]`,
            calls `FUN_080018a4(DAT_200000fa)` or
            `FUN_08001a58(DAT_200000fb)`, then queues command `4`
0x0801A526: `FUN_08019e98` writes `(&DAT_200000fa)[uVar70] = bVar37 + 1`,
            calls `FUN_080018a4`/`FUN_08001a58`, then queues command `4`
```

This is negative DMM evidence. Both guarded writer branches are scope/siggen
autorange/frontend paths in the current decompile context. They prove that the
stock firmware mutates the mux-state pair at runtime, but they do not recover a
DMM-mode runtime writer for `ms[0x02]`/`ms[0x03]`. A future DMM correction must
find a DMM-owned writer or a stock runtime trace; it must not reuse these
scope/siggen autorange branches as a meter range proof.

This means the open firmware can legitimately project the recovered stock DMM
slots into the two mux bytes for fail-closed local operation, but it must keep
that projection marked as a local policy. The complete direct mux callsite list
does not recover runtime DMM `ms[0x02]`/`ms[0x03]` writers: no inspected
callsite maps the eight DMM selector slots to analog mux bytes. Until either
Ghidra data-xrefs recover the DMM saved-state writers or a stock-runtime trace
records `0x200000fa/0x200000fb` while switching DMM modes, scope/siggen mux callers are not DMM runtime range proof.
Do not treat scope auto-range writes as evidence for DMM current/voltage range
decoding, and do not repair a surprising DMM reading by adding a numeric
coefficient on top of those scope paths.

The `mux-state RAM-map boundary` is now guarded too.  The stock V1.2.0
`ram_map.txt` function-level refs for the shared mux-state bytes are:

```text
DAT_200000fa (25 refs):
  FUN_08034078@08034078
  FUN_08001c60@08001c60
  FUN_08019e98@08019e98
  FUN_0801f6f8@0801f6f8
  FUN_0801d2ec@0801d2ec
  FUN_0801efc0@0801efc0
  unknown@080151c2

DAT_200000fb (11 refs):
  FUN_08034078@08034078
  FUN_08001c60@08001c60
  FUN_08019e98@08019e98
  FUN_0801f6f8@0801f6f8
  FUN_0801d2ec@0801d2ec
```

Every listed function is already classified in this note as a scope/siggen
writer, scope/preset owner, scope snapshot path, scope UI/LUT consumer, or
scope measurement/math consumer.  The guard is negative evidence: the current
RAM-map surface still does not expose a DMM-owned runtime writer for
`ms[0x02]`/`ms[0x03]`.  If future RE finds one, it must update the xref map,
binary guard, and local range policy together rather than citing a stale
unclassified RAM-map line.

The `mux-state full-decompile surface guard` now pins the text-decompile
surface too, not only the RAM-map function-level surface. It verifies all 26
`DAT_200000fa` references and all 10 `DAT_200000fb` references currently
visible in `full_decompile.c` and classifies the only decompile-visible indexed
writes to the shared mux-state pair:

```text
DAT_200000fa full-decompile refs: 26
  full_decompile.c:2566  (&DAT_200000fa)[uVar20] = bVar2 + 1;
    target: DAT_200000fa/DAT_200000fb selected by uVar20
    classification: scope/siggen autorange increment in FUN_08001c60
  full_decompile.c:8745  (&DAT_200000fa)[uVar70] = bVar37 + 1;
    target: DAT_200000fa/DAT_200000fb selected by uVar70
    classification: scope_main_fsm autorange increment in FUN_08019e98

DAT_200000fb full-decompile refs: 10
  no literal direct assignment line such as DAT_200000fb = ...
```

The rest of the guarded text refs are function calls, scope scale-table
consumers, or snapshot copies such as `DAT_20000eb9 = DAT_200000fa` and
`_DAT_20000eba = _DAT_200000fb`. The two indexed writes above can update
`DAT_200000fb` when the index selects the second byte; they are already kept as
scope/siggen evidence because the same branches call `FUN_08001a58(DAT_200000fb)`.
`DAT_200000fb` only lacks a literal direct assignment in the recovered text
surface. This narrows the DMM gap rather than solving it: if a later decompile
or stock trace exposes a new literal or aliased write to either byte, that new
write must be classified as DMM-owned, scope-owned, saved-state, or non-code
before local mux/range policy changes.

The `mux-state pair-write context guard` also pins the branch/call context
around those two aliased writes so the classification is not derived from the
assignment line alone:

```text
full_decompile.c:2564..2573
  bVar2 = (&DAT_200000fa)[uVar20];
  if (bVar2 < 9) {
    (&DAT_200000fa)[uVar20] = bVar2 + 1;
    if (uVar20 == 0) FUN_080018a4(DAT_200000fa);
    else             FUN_08001a58(DAT_200000fb);
    local_31 = 4;

full_decompile.c:8745..8753
  (&DAT_200000fa)[uVar70] = bVar37 + 1;
  if (uVar70 == 0) FUN_080018a4(DAT_200000fa);
  else             FUN_08001a58(DAT_200000fb);
  local_6b = 4;
  FUN_0803acf0(_DAT_20002d6c, &local_6b, 0xffffffff);
```

Both contexts prove the same index-selected pair write is immediately followed
by the matching Port C/E or Port A/B mux writer and command `4` enqueue. That is
why these are kept as scope/siggen autorange paths and negative DMM evidence,
not as hidden DMM range switching.

The scope-submode mux call guard now pins the two explicit scope reconfiguration
sites that were previously only present in the broad direct-BL list. Both sites
read `DAT_20000128` / `state[0x30]`, mask the low nibble, and feed that scope
sub-mode byte to both mux writers:

```text
0x0801C7B8 scope_submode_post_calibration_mux_restore:
  1e f0 9a fa 40 f2 f8 04 c2 f2 00 04 94 f8 30 00
  00 f0 0f 00 e5 f7 6a f8 94 f8 30 00 00 f0 0f 00
  e5 f7 3e f9 ff f7 58 bb

0x0801D088 scope_submode_runtime_mux_restore:
  00 0a 4e 46 96 f8 30 00 00 f0 0f 00 e4 f7 06 fc
  96 f8 30 00 00 f0 0f 00 e4 f7 da fc 96 f8 32 00
  b2 46 ff 28
```

That makes these callsites negative DMM evidence: they are scope runtime
reconfiguration paths, not DMM selector-table consumers, not DMM selector-word
emitters, and not the missing runtime writer for `ms[0x02]`/`ms[0x03]`.

The same boundary applies to DAC1 (`0x40007408`). Stock DAC1 writes are real
and table-backed, but current xrefs tie them to the scope trigger/comparator
path. They are not a recovered meter reference or low-DCV correction source.

### Scope Snapshot Consumer Guard, 2026-06-06

`FUN_08034078` is another easy place to draw the wrong conclusion. The stock
decompile copies the current scope/mux state into the `DAT_20000eb8..` snapshot
block before scope measurement/display math:

```text
full_decompile.c:26144  DAT_20000eb8 = DAT_20000125;
full_decompile.c:26145  DAT_20000eb9 = DAT_200000fa;
full_decompile.c:26155  _DAT_20000eba = _DAT_200000fb;
```

Those snapshot bytes are later consumed by scope scale/table paths such as
`full_decompile.c:8613`, `9087`, and `9847..10066`, where
`DAT_080465cc` is indexed by the saved mux state. The function is named
`scope_display_refresh` in `function_names.md`, is called from `scope_main_fsm`
and scope render paths, and does not call `FUN_080018a4` or `FUN_08001a58`.

`scripts/test_stock_meter_literals.py` binary-guards the opening snapshot block:

```text
0x08034078:
  2d e9 f0 4f 81 b0 2d ed 04 8b 40 f2 f8 05 c2 f2
  00 05 95 f8 2d 00 4a f6 ab 27 ca f6 aa 27 a0 fb
  07 12 a9 78 85 f8 c0 0d 85 f8 c1 1d d5 f8 1a 10
  b5 f8 b4 0d 4f ea 31 41 c5 f8 c6 1d a9 8a 6b 79
  a5 f8 be 1d b5 f8 b6 1d a5 f8 e0 0d
```

This is a scope snapshot consumer guard. It proves that stock reads the current
mux-state pair into a measurement/display snapshot, then uses that snapshot in
scope math. It is explicitly a consumer/snapshot path, not a DMM mux writer,
not a DMM mode/range transition, and not a factory meter calibration source.

### Scope/Preset Mux Owner Guard, 2026-06-06

The remaining early mux callsites from the direct BL sweep are now classified
from stock binary disassembly instead of left as possible DMM evidence. The two
paired handlers are:

```text
0x08003148: scope/preset mux increment handler
0x08003900: scope/preset mux decrement handler
```

Both handlers read `DAT_20001060` (`[base+0xf68]`) for a UI/state switch and
use `DAT_2000044c` (`[base+0x354]`) as the channel selector. The low nibble
selects an action, while the sign/high bit selects which mux-state byte is
edited:

```text
0x080031B6: add.w r0, r5, r0, lsr #7
0x080031BA: ldrb.w r1, [r0, #2]!
0x080031C4: adds r1, #1
0x080031C6: strb r1, [r0, #0]
...
0x080031DE: cmp.w r0, #-1
0x080031E2: ble.w 0x08003642
0x080031E6: ldrb r0, [r5, #2]
0x080031E8: bl 0x080018a4

0x08003642: ldrb r0, [r5, #3]
0x08003644: bl 0x08001a58
0x08003658: movs r1, #4
0x08003664: bl 0x0803acf0
```

The decrement-side handler mirrors the same ownership shape:

```text
0x08003970: add.w r0, r6, r0, lsr #7
0x08003974: ldrb.w r1, [r0, #2]!
0x0800397E: subs r1, #1
0x08003980: strb r1, [r0, #0]
...
0x08003998: cmp.w r0, #-1
0x0800399C: ble.w 0x08003E38
0x080039A0: ldrb r0, [r6, #2]
0x080039A2: bl 0x080018a4

0x08003E38: ldrb r0, [r6, #3]
0x08003E3A: bl 0x08001a58
0x08003E4E: movs r1, #4
0x08003E5A: bl 0x0803acf0
```

`scripts/test_stock_meter_literals.py` carries a scope/preset mux owner guard
for the two prologues and all four mux branches:

```text
0x08003148 increment prologue:
  f0 b5 81 b0 2d ed 02 8b 40 f2 f8 05 c2 f2 00 05
  95 f8 68 0f 01 38 08 28 00 f2 b3 83 df e8 10 f0
0x080031B6 increment Port C/E branch:
  05 eb d0 10 10 f8 02 1f ... a8 78 fe f7 5c fb
0x08003642 increment Port A/B branch:
  e8 78 fe f7 08 fa ... 04 21 38 68 21 70 ... 37 f0 44 fb

0x08003900 decrement prologue:
  f0 b5 81 b0 2d ed 02 8b 40 f2 f8 06 c2 f2 00 06
  96 f8 68 0f 01 38 08 28 00 f2 4b 84 df e8 10 f0
0x08003970 decrement Port C/E branch:
  06 eb d0 10 10 f8 02 1f ... b0 78 fd f7 7f ff
0x08003E38 decrement Port A/B branch:
  f0 78 fd f7 0d fe ... 04 21 28 68 21 70 ... 36 f0 49 ff
```

This resolves the "additional early UI/scope-owner mux sites" row from the
previous audit: these are stock scope/preset UI mux owners. They prove another
runtime owner of the shared mux-state pair, but they are not tied to the
eight-entry DMM selector table and are not DMM runtime range proof.

### Scope UI Mux-LUT Consumer Guard, 2026-06-06

The `ram_map.txt` entry `unknown@080151c2` for `DAT_200000fa` resolves into
a pre-`FUN_08015f50` stock instruction slice at `0x080151B0`. That slice reads
`DAT_2000010e` as the active scope channel/index, loads
`(&DAT_200000fa)[idx]`, and uses `DAT_0804bfb8` as a scope scale table:

```text
0x080151B0: movw r8,#0xf8
0x080151B4: movt r8,#0x2000
0x080151B8: ldrb.w r0,[r8,#22]      ; DAT_2000010e channel/index
0x080151C0: add r0,r8
0x080151C2: ldrb r1,[r0,#2]         ; (&DAT_200000fa)[idx]
0x080151E4: movw r1,#0xbfb8
0x080151E8: movt r1,#0x804          ; DAT_0804bfb8
0x080151EE: ldrh.w r0,[r1,r0,lsl #1]
```

`scripts/test_stock_meter_literals.py` binary-guards that mux-LUT consumer:

```text
0x080151B0:
  40 f2 f8 08 c2 f2 00 08 98 f8 16 00 4a f6 ab 23
  40 44 81 78 ca f6 aa 23 ca b2 a2 fb 03 23 b8 f9
  1c 20 90 f9 04 00 5c 08 10 1a 00 ee 10 0a a4 eb
  84 00 08 44 4b f6 b8 71 c0 b2 c0 f6 04 01 31 f8
  10 00
```

This is scope render/scale math. It consumes `DAT_200000fa` and
`DAT_0804bfb8`, but it performs no mux writer call, no DMM selector-table
transition, and no meter calibration. Do not use this xref to justify a DMM
range correction.

### Scope Mux-State Consumer Guard, 2026-06-06

The remaining RAM-map consumers for `DAT_200000fa`/`DAT_200000fb` are now
classified and binary-guarded too. `ram_map.txt` lists `FUN_0801d2ec`
(`0x0801D2EC`), `FUN_0801efc0` (`0x0801EFC0`), and `FUN_0801f6f8`
(`0x0801F6F8`) as additional refs to the mux-state pair.
Those functions are scope timebase, scope math, and scope measurement-engine
paths, not DMM runtime range owners.

The `scope measurement-engine mux-pointer consumer context guard` pins
`full_decompile.c:11411..11491` inside `FUN_0801f6f8` and
forbids local pointer-write forms such as `*pbVar19 =` after
`pbVar19 = &DAT_200000fa + uVar22`. This matters because the later alias reads
no longer contain the literal `DAT_200000fa` symbol. The guard keeps this block
classified as read-only scope measurement/scale math, not as a hidden DMM mux
writer:

```text
full_decompile.c:11411  pbVar19 = &DAT_200000fa + uVar22;
full_decompile.c:11412  bVar3 = *pbVar19;
full_decompile.c:11418  FUN_0803e5da(*(undefined2 *)(&DAT_0804bfb8 + ...), ...)
full_decompile.c:11435  uVar31 = *pbVar19 / 3;
full_decompile.c:11438  uVar6 = *(undefined2 *)(&DAT_0804bfb8 + ...);
full_decompile.c:11488  bVar3 = *pbVar19;
full_decompile.c:11491  uVar6 = *(undefined2 *)(&DAT_0804bfb8 + ...);
```

`scripts/test_stock_meter_literals.py` carries the `scope mux-state consumer
guard` for these sites:

```text
0x0801D2EC scope_timebase_ch1_mux_scale_consumer:
  loads base `0x200000f8`, reads `DAT_200000fa`, then indexes
  `DAT_080465cc` for scope scale math.

0x0801D8B8 scope_timebase_ch2_mux_scale_consumer:
  reads `DAT_200000fb` and `DAT_200000fd`, then indexes the same scope
  scale table.

0x0801F51E / 0x0801F5FC scope_math_delta_ch1/ch2_mux_scale_consumer:
  `FUN_0801efc0` reads `DAT_2000010e`, `(&DAT_200000fa)[idx]`, and
  `(&DAT_200000fc)[idx]` while formatting scope/math deltas.

0x0801FD66 scope_measurement_engine_mux_scale_consumer:
  `FUN_0801f6f8` reads the selected mux-state byte and offset byte while
  computing scope measurement scale/position values.
```

These remaining RAM-map consumers are useful negative evidence. They consume
the shared scope mux-state bytes and scope scale tables, but they do not call
`FUN_080018a4` or `FUN_08001a58`, do not emit DMM selector words, and are not
DMM range proof. The unresolved DMM path is still a runtime writer or trace that
ties DMM selector/range transitions to `ms[0x02]` and `ms[0x03]`.
Put tersely for future audits: these scope consumers are not DMM range proof.

### Watchdog Reload State Boundary, 2026-06-06

The `0x08039038` hit in `fpga_task_decompile.txt` is also negative DMM
evidence. It is easy to confuse the immediate offset with the recovered mux
state bytes, but the base register is `r7 = 0x200000f8`, so `[r7,#0xf62]` is
absolute `0x2000105a` (`DAT_2000105a`), not `DAT_200000fa`, `DAT_200000fb`,
`ms[0x02]`, or `ms[0x03]`.
In stock terms this is `meter_state + 0xf62`; it is not DMM ms[0x02]/ms[0x03].

```text
0x08039024: movt r7, #0x2000       ; r7 = 0x200000f8 = meter_state
0x0803902c: movt sb, #0x4001       ; sb = 0x40015434 = IWDG_RLR
0x08039038: ldrb.w r0, [r7,#0xf62] ; DAT_2000105a
0x08039040: strne.w r0, [sb]       ; write watchdog reload register
0x08039044: strh.w r5, [r7,#0xf6c] ; clear adjacent housekeeping counter
```

The same byte is used during init:

```text
0x08027372: ldrb.w r0, [sl,#0xf62]
0x08027386: strne r0, [0x40015404,#0x30] ; IWDG reload register alias
```

The RAM map and full decompile agree that `DAT_2000105a` is a display/UI state
byte, not an analog mux/range byte:

```text
ram_map.txt: 0x2000105A DAT_2000105a (1 refs): FUN_08015f50@08015f50
full_decompile.c:4733  uVar15 = FUN_0803e5da(DAT_2000105a);
```

`scripts/test_stock_meter_literals.py` carries the
`watchdog reload state boundary guard` for `0x08027372` and `0x08039008`,
plus the RAM-map and full-decompile anchors above. If a future pass wants to
use this state area for DMM behavior, it must first recover a different stock
writer/consumer tied to meter selector/range transitions; this block itself is
watchdog/UI housekeeping, not DMM `ms[0x02]`/`ms[0x03]` range proof.
