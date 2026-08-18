# What stock does between "FPGA configured" and "two channels + meter working"

**Date:** 2026-08-17
**Binary:** `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`, link base `0x08007000`
(file offset = flash − 0x08007000). **Every address below is FLASH convention**, hand-verified
by disassembling the named range with `objdump -b binary -m armv7e-m -Mforce-thumb
--adjust-vma=0x08007000`. Where I quote an older doc that used the legacy base-`0x08000000`
convention, I say so and give the converted address.

**Why:** `docs/experiments/2026-08-17-03-warm-handoff-2x2.md` established that our runtime, not
our FPGA configuration, is the fault — stock had two channels and a live meter on the exact
FPGA SRAM image our firmware then inherited over a soft reset, and under our firmware CH2 and
the meter were both dead.

**Status of this document:** the investigation was cut short by a tool failure. Sections 1-6
are what I actually established from the binary. Section 8 lists what I did *not* get to.
Nothing here is inferred to fill a gap; gaps are marked as gaps.

---

## 0. Method note — one instrument correction up front

`reverse_engineering/analysis_v120/FPGA_TASK_ANALYSIS.md` and the CLAUDE.md summary describe a
"~40-entry FPGA command table (0x00-0x2C), dispatch table at 0x0804BE74", with
`0x01 = "Scope | Configure channel | Type 0 (CH1) / Type 1 (CH2)"` and
`0x0B-0x11 = "Scope | Channel/trigger/timebase"`. **That table is not an FPGA command table.
It is the LCD redraw dispatch table.** (HIGH — see §5.) The premise of the task I was given
("find the call sites for USART cmd 0x01 / 0x0B-0x11") therefore does not have an answer,
because those IDs never reach a wire.

---

## 1. The mode-state model

`ms` = the big state block based at `0x200000F8`. The mode byte is `ms[0xF68]` = `0x20001060`.

| state | meaning | how established |
|---|---|---|
| 1 | **METER / DMM** | the entry function for state 1 is the one that enables USART2 + resumes the two dvom tasks + raises PC11 |
| 2 | **SCOPE** | four independent supports, listed below |
| 3 | siggen | entry function clamps `ms[0xF6A]` from `ms[0xE59]`, tears the meter down |
| 5 | a scope sub-view | only reachable from state 2 (`0x0800E630`: `if (state==2) state=5`) |
| 0,4,6,7,8,9 | further sub-views / banks | not investigated |

Confidence that **state 2 = scope: HIGH**. Supports:
1. `0x0800A6D8`, `0x0800A8A0`, `0x0800AECE`, `0x0800B188` — every SPI3 scope-register post is
   guarded by `cmp <state>,#2 ; bne skip`.
2. `0x0803DA90`-`0x0803DAAA` — the UI task gives semaphore `0x20002D80` (fpga_sem1) whenever its
   queue is idle **and** `ms[0xF68]==2`, i.e. free-running acquisition only in that state.
3. Entering state 2 tears the meter down (§3).
4. `0x0800E630` promotes state 2 → state 5, a scope sub-view.

Mode-entry functions (reached only through the pointer table at `0x08046588`-`0x08046598`,
which is why a plain `bl` xref search finds no callers):

| function | action |
|---|---|
| `0x0800E2F4` | enter state **1** (meter) |
| `0x0800E3E4` | enter state **2** (scope) |
| `0x0800E4F4` | enter state **3** (siggen) |
| `0x0800E60C` | toggles `ms[0x48]`, only if state==2 |
| `0x0800E630` | state 2 → state 5 |

---

## 2. What stock sends on USART2 during scope entry and steady state

### **Nothing. USART2 is switched OFF.** (HIGH)

`0x0800E3E4` (enter scope), teardown arm for old-state 1, at `0x0800E41A`-`0x0800E49A`:

```
0800e41a  USART2->CTRL1 &= ~0x2000      ; UEN CLEARED   (r0 = 0x4000440C)
0800e42a  vTaskSuspend( *(void**)0x20002DA0 )   ; dvom_TX
0800e438  vTaskSuspend( *(void**)0x20002DA4 )   ; dvom_RX
0800e446  GPIOC->BRR (0x40011014) = 0x800       ; PC11 LOW
0800e454  xQueueReset( *(void**)0x20002D7C )    ; meter semaphore
0800e466  xQueueReset( *(void**)0x20002D74 )    ; usart_tx queue
0800e476  ms[0xF36] = 1 ; float sentinels ms[0xF48/F4C/F50] = 0x7FC00000 ; ms[0xF3C]=ms[0xF2D]=0
0800e4c4  ms[0xF68] = 2
0800e4e4  tail-call 0x08012908                  ; LCD redraw bank emitter
```

The mirror image exists at `0x0800E360`-`0x0800E3DE` (enter meter: UEN set, both dvom tasks
resumed, `GPIOC->BSRR (0x40011010) = 0x800` → PC11 HIGH, sentinels reseeded), and the same pair
exists in `master_init` at boot: meter-on at `0x0802DF8E`, meter-off at `0x0802E00A`. The June
2026 doc `meter_mode_command_table_2026_06_05.md` describes these same two blocks at
`0x08026F8E` / `0x0802700A` — those are **legacy** addresses; +0x7000 gives exactly the flash
addresses above, which I confirmed by byte-searching its quoted slices.

There is exactly one USART2 transmitter in the image: `dvom_tx_task` at `0x0803E3F4`, which
blocks on queue `0x20002D74` and builds the 10-byte frame
`AA 55 <param> <cmd> 00 00 00 00 00 <(cmd+param)&0xFF>`, then arms TDBEIE. I enumerated its
producers (`movw ...,#0x2d74` scan): `0x0800A3CA`, `0x0800ABA4`, `0x0800B30C`, `0x0800B8E4`,
`0x0800CB7A`, `0x0800D0CA`, `0x0800D2A2`, `0x0800D78A`, `0x0800D7BC`, `0x0800D8C0`, plus the two
queue-reset sites in the mode transitions and the creation site in master_init. I sampled four
of them (`0x0800A3CA`, `0x0800ABA4`, `0x0800B30C`, `0x0800B8E4`) — **every one posts a
`0x05xx`-family word** (`0x0508`, `0x0509`, `0x0500|n`), i.e. the DMM selector words that
`meter_mode_command_table_2026_06_05.md` already guards as meter commands.
**MEDIUM-HIGH**: I checked 4 of 10 producers, but the four are the ones reachable from the mode
menus, and the peripheral is disabled in scope mode anyway, which makes the point moot.

**Consequence for the task premise:** stock has no scope-mode USART2 traffic, no ordering, and
no delays to copy. Our `guest-coldtrace` family's `FPGA_USART_SILENT_SCOPE=1` is, on this axis,
*stock-faithful*.

### Where the "cmd 0x01 = configure channel" folklore came from

The 45 codes `0x00`-`0x2C` are single bytes posted to queue `0x20002D6C` (created 20×1 byte)
and consumed by the task at `0x0803DA50`:

```
0803da52  r5 = &queue 0x20002D6C
0803da56  r6 = 0x0804BE74            ; handler table
0803da5a  r7 = &sem  0x20002D80
0803da5e  r8 = 0x200000F8            ; ms base
0803da78  handler = ((void(**)())r6)[byte] ; blx handler
```

Handler `0x00` = `0x0800FD38` writes to `0x6001FFFE`/`0x60020000` — **the EXMC LCD command and
data registers** — setting a display window. Handler `0x02` = `0x080104EC` mallocs a
52×216×20 buffer and takes `0x20002D84`. Handler `0x03` = `0x08010D70` mallocs 301×201×2 and
calls `0x080165A8` (the 25 KB "zero-caller" function in CLAUDE.md — it now has a caller).
Handler `0x1A` = `0x08010A1C` early-outs unless `ms[0xF68]==1` and then draws strings.
**⇒ `0x0804BE74` is the LCD redraw dispatch table. HIGH.**

Per-mode redraw banks emitted by `0x08012908` (TBH at `0x08012926`, table at `0x0801292A`,
10 entries, states 0-9), for the record:

| state | bytes pushed to `0x20002D6C` |
|---|---|
| 0 | 00 01 0B 0C 0D 0E 0F 10 11 |
| 1 (meter) | 00 09 (07 or 0A) 1A 1B 1C 1D 1E |
| 2 (scope) | 02 03 04 05 06 08 09 (07 or 0A) |
| 3 | 00 08 09 (07\|0A) 16 17 18 19 |
| 4 | 00 1F 09 20 21 |
| 5 | 00 25 09 26 27 28 |
| 6 | 29 |
| 7 | 15 |
| 8 | 00 2C |
| 9 | 00 12 13 14 09 (07\|0A) |

The `07`/`0A` choice is `GPIOC->IDT bit 7` (PC7, probe-present): bit clear → `0x0A`, set → `0x07`.

---

## 3. Does stock's meter keep working in scope mode?

### **No. Stock tears the meter completely down on scope entry.** (HIGH)

Exactly the block quoted in §2: UEN cleared, both dvom tasks suspended, **PC11 driven LOW**,
meter semaphore and TX queue reset, meter accumulators reset to `0x7FC00000`. Re-entering meter
mode rebuilds it in the reverse order: UEN set → resume dvom_TX → resume dvom_RX → **PC11 HIGH**
→ reseed sentinels → redraw bank.

At boot the same choice is made once, from the *saved* mode `ms[0xF64]`
(`0x0802DF50`): saved mode 1 (or no valid save) → meter-on path; saved mode 2 or 3 → meter-off
path. So **a stock unit that was last used in scope mode boots with USART2 disabled and PC11
LOW**, and only a mode switch brings the meter up.

**This means our meter being dead in a scope-posture build is stock's own behaviour, not a
bug — but it also means the recipe to revive it is fully specified, and our build actively
does the opposite of it** (§6).

---

## 4. GPIO stock drives on the scope path, beyond the two range functions

`gpio_mux_portc_porte` (`0x080088A4`) and `gpio_mux_porta_portb` (`0x08008A58`) are the two
already-known 10-case relay tables. Everything below is *outside* them.

`master_init` tail, `0x0802E1F8`-`0x0802E276`, is stock's "apply saved state to hardware" block.
Register base note, verified: `r8` is loaded at `0x0802DD9E` with `0x40011014` = **GPIOC BRR**,
so `[r8]` = GPIOC BRR, `[r8,#0x3FC]` = `0x40011410` = **GPIOD BSRR**, `[r8,#0x400]` =
`0x40011414` = **GPIOD BRR**. (HIGH — the same code writes GPIOC BSRR explicitly via
`movw r1,#0x1000/movt #0x4001; str r0,[r1,#16]`, and the two forms agree.)

```
0802e16c  GPIOD->BSRR = 4                       ; PD2 HIGH (unconditional)
0802e1f8  bl 0x08012908                         ; LCD redraw bank
0802e202  r0 = ms[0x14]                         ; channel-enable mask
          ==1 -> GPIOC->BSRR=4 (PC2 H) ; GPIOC->BSRR=2 (PC1 H)
          ==3 -> GPIOC->BSRR=4 (PC2 H) ; GPIOC->BRR =2 (PC1 L)
          ==2 -> GPIOC->BRR =4 (PC2 L) ; GPIOC->BRR =2 (PC1 L)
          else -> neither pin touched
0802e23e  gpio_mux_portc_porte( ms[0x02] )      ; CH1 range: PC12/PE4/PE5/PE6
0802e246  gpio_mux_porta_portb( ms[0x03] )      ; CH2 range: PA15/PB11/PB10/PA10
0802e24e  ms[0x00] ? GPIOD->BSRR=0x1000 : GPIOD->BRR=0x1000     ; PD12
0802e262  ms[0x01] ? GPIOD->BSRR=0x2000 : GPIOD->BRR=0x2000     ; PD13
```

and slightly earlier, `0x0802DE66`-`0x0802DE8A`:

```
          gpio_init(GPIOC, {pins=0x0001, mode=INPUT,  pull=NONE})   ; PC0  data-ready
          gpio_init(GPIOC, {pins=0x0010, mode=OUTPUT, pp, 10MHz})   ; PC4
          ms[0x17]==2 ? GPIOC->BSRR=0x10 : GPIOC->BRR=0x10          ; PC4
```

### PC2 / PC1 = the channel-enable mask, in hardware

`ms[0x14]` is the **channel-enable bitmask** (bit0 CH1, bit1 CH2; `.data` default `0x03`).
This is not my inference — it is already established in
`analysis_v120/spi3_runtime_dispatch_2026-08-15.md` (SPI3 op `0x02` transmits `ms[0x14]`) and
`analysis_v120/desk_sweep_2026-08-15.md` §2. What I add is that the same byte drives PC2/PC1,
with this mapping (**HIGH** — it is a direct read of the branch above):

| `ms[0x14]` | channels | PC2 | PC1 |
|---|---|---|---|
| 1 | CH1 only | HIGH | HIGH |
| 2 | CH2 only | LOW | LOW |
| 3 | both | HIGH | LOW |

which factors cleanly as **PC2 = "CH1 enabled" (active HIGH), PC1 = "CH2 enabled" (active LOW)**.
(That factoring is **MEDIUM** — it fits all three codes and the 4th arm leaving both pins
untouched, but two pins and three used codes cannot distinguish it from other encodings.)

Corroborating sites: `0x0800D996` reads `ms[0x14]`, loads `r4=0x40011410` (GPIOD BSRR) and
`r6=0x40011014` (GPIOC BRR), and writes `ms[0x14..0x15] = 0x0203`; `0x0800B770`-`0x0800B79C` is
the same handler family, which after the GPIO work posts redraw `0x02` to `0x20002D6C` and then
`0x01` to the SPI3 queue `0x20002D78`. Exp R (`expE_swd_state_diff_2026-07-28.md`) decoded the
identical 4-way selector at `0x0802C608` and found ~8 copies image-wide — Exp R correctly
called it "a functional selector, not a config strap", but did not identify *which* function.
**It is the channel selector.**

Stock's own posture at the CONFIG_ENABLE instant, read from `swd_stock.txt` (GPIOC ODT
`0x000063EC`): PC2 = 1, PC1 = 0 ⇒ mask 3 ⇒ **both channels enabled**. Ours (`swd_ours.txt`,
GPIOC CRL `0x41844444`): PC1 and PC2 nibbles are both `4` = **floating input**.

### PD12 / PD13 = per-channel AC/DC coupling

`ms[0x00]` → PD12, `ms[0x01]` → PD13, HIGH when the byte is nonzero. Independently derived in
`desk_sweep_2026-08-15.md` §4 (menu handler `0x0800C846`…`0x0800C93E`, 0=AC 1=DC, HIGH=DC); I
confirmed the boot-restore half at `0x0802E24E`/`0x0802E262` and saw the menu handler's
GPIOD BRR/BSRR writes at `0x0800C916`-`0x0800C93E`. **HIGH.**

### PD2 — driven HIGH, function unknown

`GPIOD->BSRR = 4` at `0x0802E16C` (end of master_init, unconditional) and again at
`0x0800B754` and `0x0800BD08` (both immediately before a scope redraw + a SPI3 reg-01 write).
I found no `GPIOD->BRR = 4` anywhere in the sites I looked at, but I did **not** do an
exhaustive scan. Function unknown. **MEDIUM** that stock holds PD2 HIGH in scope mode; **LOW**
on anything about what it does. Note `guest-fidelity2` (Exp R) drove PD2 **LOW**.

### PC4 — driven from `ms[0x17]`, function unknown

`ms[0x17]` is a 3-state UI value cycled `0→1→2→0` at `0x0800C5EE`; `.data` default is `1`.
`master_init` configures PC4 as an output and sets it HIGH iff `ms[0x17]==2`, LOW otherwise.
`ms[0x17]` also selects between `301` and `301<<16` written to `ms[0xDB4]` at `0x0800B734`
(301 = 0x12D = the scope trace width), so it is a scope-display-shape control of some kind.
**Guess (LOW, do not build on it):** trigger mode (auto/normal/single) or single-vs-dual trace.
Our firmware never drives PC4. Stock's Exp E dump shows PC4 still floating *at the
CONFIG_ENABLE instant*, which is consistent — it is configured ~9 KB of code later.

### PC11 — the meter enable, LOW in scope mode

Written only by the four meter transitions (`0x0802DFC6` boot-on, `0x0802E036`-ish boot-off,
`0x0800E39A` runtime-on, `0x0800E44E`/`0x0800E55E`/`0x0800D8A8` runtime-off), always in lockstep
with USART2 UEN and the two dvom tasks. Stock CRH nibble for PC11 = `1` (output push-pull);
ODT bit 11 = 0 at the CONFIG_ENABLE instant, i.e. **LOW at boot before the meter is enabled**.
**HIGH** that PC11 is exclusively a meter-transition line. **Its physical function is still
unproven** — "meter MUX" is a project label, not a measured fact.

---

## 5. Corrections this produces to existing project docs

1. **`0x0804BE74` is the LCD redraw dispatch table, not an FPGA command table.** (HIGH)
   `FPGA_TASK_ANALYSIS.md`'s "~40 FPGA commands 0x00-0x2C" and CLAUDE.md's repetition of it are
   wrong; the mapping "cmd 0x01 = Scope | Configure channel | Type 0/1" describes a redraw ID.
   Queue `0x20002D6C` is the redraw queue, not `usart_cmd`.
2. Consequently **there is no USART2 "scope channel configure" command to send.** The channel
   selector is `ms[0x14]` → SPI3 op `0x02` **and** GPIO PC2/PC1.
3. `dmm_mode_state_f68_boundary_2026_06_06.md` and `meter_mode_command_table_2026_06_05.md`
   addresses are **legacy base-0x08000000**; add 0x7000. I verified this by byte-searching their
   quoted slices (`0x08026F50` → flash `0x0802DF50`, etc.).
4. `ms[0xF68]` is the **UI mode** state (1 meter / 2 scope / 3 siggen / others = sub-views), not
   a DMM-submode byte.

---

## 6. What our `fpga.c` demonstrably does not do

Read against `firmware/src/drivers/fpga.c` and `firmware/src/main.c` on branch
`bench/2026-08-17`:

| stock does | we do | where ours is |
|---|---|---|
| PC2/PC1 driven as the channel mask; stock's own state at handoff is PC2 H / PC1 L (both channels) | **never driven in any runtime build** — left floating inputs. The only writes are inside the `FPGA_STOCK_FIDELITY` config block (`fpga.c:4329` PC2 HIGH, `fpga.c:4382` PC1 LOW), which is not compiled into the warm-handoff or coldtrace builds | missing |
| PC11 HIGH in meter mode | **explicitly driven LOW** at `main.c:767` (`GPIOC->clr = 1u<<11` in the `FPGA_WARM_HANDOFF_TEST` block, commented "PC11 LOW (scope)") and again at `fpga.c:4420`, `fpga.c:5188`, `fpga.c:5281`, `fpga.c:262`, `fpga.c:1778/1782` | actively wrong for the meter |
| PD12/PD13 = per-channel coupling | `fpga.c:4696` drives both HIGH (DC) in one path; not tied to any per-channel state | partial |
| PD2 HIGH | Exp R build drove it LOW; runtime builds do not drive it | missing |
| PC4 from `ms[0x17]` | never driven | missing |
| PA3 (USART2 RX) input **pull-up** (stock GPIOA CRL `0x29008944`, nibble3 = 8, ODR bit3 = 1) | input **floating** (`fpga.c:4197-4201`; our GPIOA CRL `0x84404944`, nibble3 = 4) | divergence, same class as the PB4/MISO bug |
| TX frame header `AA 55` (from the Keil `.data` image) | `00 00` (`usart2_send_cmd`, `fpga.c:1022`) | divergence, previously documented in `usart_boot_frames_exact.md`, still unfixed |
| meter bring-up order: UEN → resume dvom_TX → resume dvom_RX → PC11 HIGH → reseed sentinels | no equivalent ordered sequence; `fpga_usart_scope_enable()` (`fpga.c:2931`) sets CTRL1 + NVIC only | missing PC11 |

---

## 7. The two failures, separately

**CH2.** The strongest candidate is **PC2/PC1**. Stock holds PC2 HIGH / PC1 LOW (mask 3, both
channels); an MCU reset returns both to floating inputs, and our firmware never re-drives them,
so stock's channel-enable posture is exactly the part of stock's working state that a warm
handoff does *not* preserve — which is EXP-03's own blind spot #1. It also fits the two things
EXP-03 measured that a simple "CH2 is off" model does not explain: op04 and op05 both carrying
the **CH1** tone, and the CH1 attenuator bank moving **both** buffers. Those are the signature
of CH1 being routed into both converters, i.e. of the part being in single-channel mode, not of
CH2 being dead.
**Confidence that PC2/PC1 is the channel mask: HIGH. Confidence that it is *the* CH2 gate on
the bench: MEDIUM — untested.** Note `spi3_runtime_dispatch_2026-08-15.md` lists "PC1/PC2" among
pins "previously swept and also negative"; that refers to the config-entry-era sweeps (Exp G/Q,
pulses, before configuration), not to holding the static mask on a configured part while
reading op05. `desk_sweep_2026-08-15.md` §7 item 2 proposed exactly this test on 2026-08-15 and
it does not appear to have been run.

**Meter.** `PC11`. Stock raises it in the same breath as USART2 UEN and the dvom task resume;
our warm-handoff build drives it LOW on purpose. EXP-02's signature — TDC set, and
RDBF/ORERR/FERR/NERR *all* never set — is a line that is never driven at all, not a garbled
one, which is what a disabled/disconnected transmitter looks like. **Confidence that PC11 is
the meter enable in stock's software: HIGH. Confidence that raising it revives RX: MEDIUM** —
its physical function is unproven, and PA3 being a floating input rather than stock's pull-up
is a second, independent defect on the same wire.

---

## 8. What I did **not** get to

- I did not trace where `ms[0x14]` is written from the UI, so I have not proven that the user's
  CH1/CH2 on-screen toggle is what moves PC2/PC1 (I only saw `0x0800D996` writing
  `ms[0x14..0x15] = 0x0203` and the `0x0800B770` family posting SPI3 `0x02` afterwards).
- I did not determine what PC4 or PD2 physically do.
- I did not verify PC11's physical function (mux? power? AFE enable?) — only its software role.
- I did not check the remaining 6 of 10 `0x20002D74` producers.
- I did not look at the `osc` task or at how stock paces its 04/05 read pairs at runtime
  (`desk_sweep_2026-08-15.md` §3/§6 already covers the re-arm and the 28.9 ms cadence).
- I did not audit whether anything else in stock's scope path touches GPIO between the redraw
  bank emit and the acquisition loop.

## 9. Bench tests this produces

Ordered by cost. Both are single-variable and both have a real negative control.

1. **CH2 / channel mask (one build, three readings).** In the warm-handoff or coldtrace build,
   configure PC1 and PC2 as output push-pull, then A/B/A:
   - `PC2 HIGH, PC1 LOW` (mask 3, stock's dual posture) → read op04 and op05;
   - `PC2 HIGH, PC1 HIGH` (mask 1, CH1 only) → read both again;
   - back to mask 3.
   Also set CH2's coupling (PD13 HIGH) and a CH2 range ≤ 4 so `gpio_mux_porta_portb` leaves
   PA15 (CH2 input connect) HIGH — `desk_sweep_2026-08-15.md` §4 notes our relay table
   disconnects the CH2 input at range 8, which is the state CH2 keeps getting tested in.
   **Predicts:** under mask 3 the CH2-jack tone appears in op05 and *only* op05; under mask 1
   both buffers show the CH1 tone again. If mask 3 changes nothing, PC2/PC1 is refuted as the
   CH2 gate and the analog side (or the netlist's two hard-wired ADC buses) is next.
2. **Meter (no rebuild if a `gpio set` shell exists).** With USART2 already brought up
   (`guest-warmtest-usart`), drive **PC11 HIGH** (configure GPIOC pin 11 as output push-pull
   first — a `scr` write to a floating input does nothing), then re-send the same three
   stock-proven meter frames EXP-02 used and watch `rx_bytes`. Negative control: PC11 back LOW,
   same frames, expect silence again. While there, set PA3 to input pull-up to match stock.
   **Predicts:** RX bytes with PC11 HIGH, none with it LOW.

Test 2 is the cheaper of the two and is close to a pure instrument fix: EXP-02 is recorded as
VOID precisely because its positive control failed, and PC11 is the most likely reason that
control could not have passed.
