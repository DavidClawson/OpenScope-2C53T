# Experiment E — SWD register diff at the CONFIG_ENABLE instant (2026-07-28)

**Result: no single smoking gun, but two instrument-fidelity bugs found and three
new unrefuted off-SPI pin candidates surfaced (PC2, PB12, PB9).**

## Method

Both firmwares were patched to **park** (`b .`) at the instruction immediately before
`0x15` (CONFIG_ENABLE) is written to the SPI3 data register, with the `05`/`12`
prelude already clocked and CS held LOW. The MCU was then halted over SWD
(ST-Link/V2 + OpenOCD 0.12.0, `hla_swd`, `~/at32_attach.cfg`) and every plausibly
relevant peripheral register dumped via `scripts/swd_state_dump.sh`.

Stock spin image: `/tmp/APP_2C53T_expE_spin_stock.bin`
(sha256 `79c2d306899667391b93c7f17e2a912f6b63f663ec14aae23709b91c1abbc964`) — exactly
**4 bytes** differ from clean stock:

| Flash addr | File off | Clean | Spin | Meaning |
|---|---|---|---|---|
| `0x0802DA42` | `0x26A42` | `1520` (`movs r0,#0x15`) | `fee7` (`b .`) | park before CONFIG_ENABLE is clocked |
| `0x0802E5CC` | `0x275CC` | `0860` (`str r0,[r1]`) | `00bf` (`nop`) | suppress `0xCCCC`→`IWDG_KR` so the park survives |

Disassembly confirming both sites:

```
802da42:  2015    movs r0, #21      @ 0x15 = CONFIG_ENABLE   <-- spin lands here
802da44:  6070    str  r0, [r6, #4] @ ...write to SPI3 DT
...
802e5c2:  f64a 20aa  movw r0, #0xaaaa   @ IWDG_KR reload
802e5c6:  6008       str  r0, [r1, #0]
802e5c8:  f64c 40cc  movw r0, #0xcccc   @ IWDG_KR *start*
802e5cc:  6008       str  r0, [r1, #0]  <-- NOP'd
```

Guest spin image: `/tmp/OPENSCOPE_expE_spin_guest.bin` (`make guest-spin`,
`FPGA_SPIN_AT_CONFIG_ENABLE=1`), sha256 `380ae98cfb2cbb2f…`.

Raw dumps archived alongside this file: `swd_stock.txt`, `swd_ours.txt`,
`swd_chargemode_baseline.txt`.

## Gotchas discovered (procedural)

1. **`pc` is useless on this target.** Every halt reports
   `current mode: Handler HardFault`, `pc: 0xfffffffe`, IPSR=3 — the Cortex-M lockup
   signature — regardless of actual device state (seen identically on a powered-off
   device and on a correctly-parked one). This is the read-protection artifact
   `docs/SWD_GOLDEN_REFERENCE_2026_06_09.md` warns about. Core GP registers (`sp`,
   `r0`) do read plausibly, but **`pc` must not be trusted**.
   **Park must be confirmed from peripherals**, not `pc`.
2. **Park-confirmation criteria** (all must hold): SPI3 `CTRL1` bit6 (SPE) = 1;
   GPIOB `ODR` bit6 = 0 (PB6/CS asserted); GPIOB `ODR` bit11 = 1 (PB11 active mode);
   AFIO `MAPR` = `0x02000000` (SWJ_CFG=`010`, JTAG-DP off / SW-DP on — the remap that
   frees PB3/4/5). *Note: CLAUDE.md's `|0x2000` shorthand refers to the field, not the
   register value; the register reads `0x02000000`.*
3. **After an IAP flash the device reboots into charge-display mode, not a real
   power-on**, if left on USB. That path skips FPGA init entirely (SPI3 `CTRL1`=0,
   AFIO `MAPR`=0, USART2 dark, PC9 low). Press POWER to get a genuine boot.
   Baseline captured as `swd_chargemode_baseline.txt`.
4. **The spin image shrinks the MENU+Power upgrade-mode window** to the fraction of a
   second before the park. Our spin sits *after* the upgrade-mode button check so a
   window always exists — but a spin placed any earlier would cost upgrade-mode entry
   and force BOOT0 / ROM-DFU recovery. Place spins late.

## Findings

### Identical (excluded as factors)

- **Clock tree**: CRM `CTRL=0x03038f83`, `CFG=0xe02fa40a`, `AHBEN=0x00000116` — byte
  identical. SYSCLK/PLL/AHB conclusively not a factor.
- AFIO `MAPR = 0x02000000` in both — the JTAG→SWD remap is applied identically.
- Both have APB1EN SPI3EN (bit15) and USART2EN (bit17) set.
- SPI3 `CTRL2 = 0x00000003`, `STS = 0x00000002` in both.
- PB3 (SCK) and PB5 (MOSI) both AF-PP (`9`); PB6 (CS) both GPIO-PP (`1`).

### Tier 1 — differences on the SPI3 peripheral itself

| | stock | ours |
|---|---|---|
| SPI3 `CTRL1` | `0x00000347` → **BR=0 = /2** (60 MHz) | `0x0000037f` → **BR=7 = /256** (~470 kHz) |
| PB4 (MISO) `CNF` nibble | `8` = input **pull-up** (ODR bit4=1) | `4` = input **floating** |

Both decode otherwise identically: CPOL=1/CPHA=1 (mode 3), MSTR=1, 8-bit, MSB-first,
SSM=1.

- **The `/256` clock is self-inflicted** — it is the `cmd_br=7` set on 2026-07-27 to
  make status reads valid. It is **not** the original cause of the wall: the wall
  predates it, when `cmd_br` defaulted to 0 (`/2`), matching stock. But it is a live
  divergence and must be removed from the config frame.
- **PB4 floating is a genuine second instrument bug**, of the same family as the `/2`
  read bug found 2026-07-27. Stock's pull-up is exactly why an undriven MISO reads
  `0xFF` in the stock Saleae capture. Ours has no defined idle level, so **every
  status read this project has taken was made on a floating input.** `CFG:00039020`
  may still be real, but it was not measured under stock's electrical conditions.
  A floating MISO cannot gate FPGA config (it is an MCU input) — this is a
  *measurement* defect, not a candidate cause.

### Tier 2 — NEW unrefuted candidates (stock drives, we float)

Exp C (2026-07-27) ablated `PC12/PE4/PE5/PE6/PA15/PA10/PB10` and refuted the
frontend-strap theory. **None of the following were in that set**, so that refutation
does not cover them:

| Pin | stock | ours |
|---|---|---|
| **PC2** | output push-pull, driven **HIGH** | floating input |
| **PB12** | output push-pull, driven **HIGH** | floating input |
| **PB9** | AF push-pull | floating input |

PB12 is plausibly SPI2 NSS = SPI-flash CS (stock sets APB1EN bit14 SPI2EN and drives
PB13/14/15 AF-PP), which would make it benign — but that is unproven. PC2 is
undocumented and is the most interesting of the three.

### Tier 3 — already refuted or explained

- **USART2 `UE`**: stock `CTRL1=0x0000002c` (**UE clear**), ours `0x0000202c`
  (**UE set**). Ours also shows `STS=0xf8` (ORERR+IDLEF+RDBF) and `DT=0x5A` — we have
  already received meter traffic. Bench-refuted 2026-06-13 via
  `FPGA_USART_SILENT_SCOPE=1`; **do not re-chase.**
- **Analog frontend bank**: refuted by Exp C.
- GPIOD/GPIOE: EXMC LCD bus. Both AF-PP; stock uses `9` (10 MHz), ours `b` (50 MHz).
- APB2EN: stock `0x0070027d` vs ours `0x0040027d` — stock additionally enables
  bits 20/21 (TMR10/TMR11).
- APB1EN: stock `0x2082c0a0` vs ours `0x20828010` — stock adds TMR7 (5), TMR12 (7),
  SPI2 (14); ours has TMR6 (4) instead.

## Side result — a documented contradiction is settled

CLAUDE.md flagged an unresolved conflict between `stock_pre_fpga_gpio_state.md`
(static decode: USART2 **UE=0/dark** before the upload) and
`master_init_decode_diff_2026-06-13.md` (capture timeline: **UE live**).

This dump measures it directly at the CONFIG_ENABLE instant on real stock:
`USART2 CTRL1 = 0x0000002c` — RE, TE, RXNEIE set, **UE (bit13) CLEAR**, with
`BAUDR=0x30d4` = 9600 baud off a 120 MHz PCLK1.

**The static decode was right: UE is dark when CONFIG_ENABLE goes out.** This does not
revive the user-mode-lockout theory, which the June bench run already refuted.

## Next step

Build one **fidelity image** that matches stock on every cheaply enumerable difference
at once, and bench it in a single cycle:

1. SPI3 `BR = 0` (`/2`) for the prelude + config frame (move the `/256` read, if kept,
   outside the config frame).
2. PB4 (MISO) = input **pull-up**, not floating.
3. PC2 = output push-pull HIGH; PB12 = output push-pull HIGH; PB9 = AF push-pull.
4. USART2 `UE` = 0 until after config completes.
5. Enable SPI2 clock (APB1EN bit14).

Both outcomes are decisive:

- **Wall breaks** → bisect the five changes to the single responsible one.
- **Wall holds** → every enumerable MCU-state difference at the CONFIG_ENABLE instant
  is excluded. Combined with the already-excluded bytes, framing, prelude, trailing
  clocks, and timing (Exp A/B2/C), that makes the cause **not MCU-side**, and promotes
  the **FT232H JTAG oracle** (SRAM-load over the JTAG TAP pads — a port stock never
  uses) from "decisive next test" to "the only remaining test."

---

# Experiment F — stock-fidelity build (2026-07-28)

**Result: the wall HOLDS. Static MCU state is now excluded.**

`make guest-fidelity` closed all five enumerable differences at once (SPI3 BR
back to `/2`, PB4/MISO pulled up, PC2 + PB12 driven push-pull HIGH, USART2 UEN
clear, SPI2 clock enabled). Bench readout:

```
Magenta: S1:0347 ED:00039020 H2:y
Yellow:  CFG:8001C810 L0 H0
```

- `S1:0347` — BR=0 (`/2`) confirmed **on-device**; the fidelity change landed.
- `ED:00039020` — STATUS (0x41) at `/256` immediately after `0x15`.
  **Bit 7 SYSTEM_EDIT_MODE clear. Unchanged.**
- `CFG:8001C810` is expected garbage, not a regression: the post-close read
  inherits `cmd_br`, now `0` = `/2` = the known-garbage clock domain. `ED` is
  the trustworthy number.
- `L0 H0` — `FPGA_USART_SILENT_SCOPE` creates no acquisition tasks. Expected.

**Valuable side result:** `ED` was measured at `/256` with MISO **pulled up**,
i.e. under stock's actual electrical conditions, and returned the identical
`0x00039020` we had read on a floating line. That **retro-validates the refusal
signature** — `0x00039020` is genuine, not a floating-input artifact.

# Experiment G — RECONFIG_N pulse candidates (2026-07-28)

**Result: both candidates REFUTED. `ED:00039020` unchanged for both.**

| Build | Pulse | Outcome |
|---|---|---|
| `make guest-reconfig-pc2` | PC2 LOW 10 ms → HIGH before the prelude | no change |
| `make guest-reconfig-pb12` | PB12 LOW 10 ms → HIGH before the prelude | no change |

## Why a pulse was the right thing to try

Exp F's failure sent us back to `docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md`
(2026-06-13), which already named the mechanism and which we had not acted on:

> An already-configured, auto-booted, running GW1N does not re-enter config from
> an SSPI `CONFIG_ENABLE` alone. The documented triggers to reload a running
> device are **RECONFIG_N (low pulse >=25 ns) or power cycle**.

apicula also confirms our decode is exact (bits 5,12,15,16,17 set; **bit 7 Edit
Mode = 0 is "the smoking gun"**) and that **FLASH_LOCK is a red herring** —
flash read-back protection only, per UG290 Table 7-12 note [2]. Nothing to clear.

**Exp E is structurally blind to transitions.** A RECONFIG_N pulse issued before
the config-enable instant leaves the pin HIGH, indistinguishable from a pin
merely held HIGH. So Exp F driving PC2/PB12 statically HIGH was the wrong test
for a pulse — matching a steady state cannot reproduce an edge.

## CORRECTION to an overreach

It was claimed mid-session that PC2 and PB12 were "the complete list" of
candidates. **That is too strong.** They are the complete list of pins that
differ **statically** between stock and ours at the config-enable instant. A pin
that stock pulses LOW and then returns to a level our firmware also happens to
sit at is **invisible to a static diff** — the census would never flag it.
Refuting PC2 and PB12 closes the *static-difference* search, not the
pulsed-pin search.

## Unresolved tension worth stating plainly

apicula's model says a running, auto-booted GW1N needs a reconfig trigger. But
**Exp B2 (2026-07-27) delayed stock's config-enable by 5-10 s and stock still
configured successfully** — at which point the FPGA is unambiguously auto-booted
and in user mode. Stock therefore reconfigures a *running* part with no reset
pulse visible on the SPI3 lines. Either stock asserts a trigger on a pin the
issue-#18 capture never watched (it probed SPI3 only — it would never have seen
PC2, PB12, or anything else), or the trigger is not a GPIO at all.

## Next steps, in order

1. **Zero-bench, do this first: static hunt in the stock binary.** Search all of
   stock's init path before `0x0802DA42` for a GPIO `clr` (BRR) write followed
   by an `scr` (BSRR) write to the same pin — i.e. a pulse — on ANY port. This
   directly answers "does stock pulse a pin low before config-enable, and which
   one?" without guessing and without a bench cycle. It also covers the pulsed-
   pin blind spot the static diff cannot reach.
2. **Logic-analyzer sweep of stock's boot.** The HiLetgo 24 MHz 8-channel unit
   is adequate — a rosenrot-style reset pulse is ms-scale, not 25 ns. Sweep
   candidate pins 8 at a time across a stock power-on.
3. **FT232H JTAG oracle** (SRAM load over the JTAG TAP pads, a port stock never
   uses). apicula's recommendation in the same reply, and it bypasses the
   trigger problem entirely rather than solving it. **SRAM ONLY — never
   `--write-flash`; the NV flash holds the sole copy of the stock meter design.**

---

# Step 1 result — static hunt for a RECONFIG_N pulse (2026-07-28)

Tool: `scripts/find_gpio_pulses.py` (disassembles the stock image and reports
every store to a GPIO `ODR`/`BSRR(scr)`/`BRR(clr)` offset, dropping `sp`-relative
stack traffic).

## Negative result on the obvious idiom

Scanning the WHOLE image for a `str rX,[rY,#20]` (clr) followed within a few
instructions by `str rZ,[rY,#16]` (scr) on the **same base register** — the
textbook pulse — returns **56 candidates, and every one before the
config-enable instruction at `0x0802DA42` has `base=sp`**, i.e. stack-frame
stores, not GPIO at all. The first genuine register-based pair is `0x0802E136`,
which is *after* config-enable.

**So stock contains no same-register clr→scr pulse pair before CONFIG_ENABLE.**

## But the search is not closed — 5 real candidates remain

Restricting to master init (`0x08023A50`) through config-enable (`0x0802DA42`)
and dropping `sp`, there are **21 GPIO-shaped stores**, of which **five drive a
pin LOW**:

```
  0x080295DE  str r2,[sl,#20]   clr/BRR
  0x08029DCA  str r0,[r4,#20]   clr/BRR
  0x08029DEE  str r1,[r0,#20]   clr/BRR
  0x0802B2D0  str r0,[r6,#20]   clr/BRR
  0x0802B308  str r0,[r7,#20]   clr/BRR
```

with `scr` writes interleaved at `0x0802B284/2AA/346/412`, `0x0802C626/62C/642`
and a last one at `0x0802D63C` before config-enable.

These are pins stock drives LOW during init. **Which port and pin each targets is
unresolved** — the base registers (`sl`, `r4`, `r0`, `r6`, `r7`) are loaded
elsewhere, and resolving them needs dataflow, not a regex. That is a job for the
existing Ghidra project (`ghidra_project/`), not a disassembly scan.

Note the scan also cannot see:
- a reset performed via the **BSRR upper half** (`BSRR = 1 << (pin+16)`), which
  appears as a second `#16` store rather than a `#20` store;
- a pulse produced by a HAL call (`gpio_bits_reset()`) rather than inline stores;
- a write through an absolute literal address rather than a base register.

## Where this leaves the hunt

The five `clr/BRR` sites above are the concrete, bounded next target: resolve
each base register to a port/pin in Ghidra, and check whether any is set HIGH
again before `0x0802DA42`. Any that is = a RECONFIG_N candidate, and it would be
invisible to Exp E's static diff by construction.

Reproduce with:

```
scripts/find_gpio_pulses.py "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
```

## CORRECTION — three of the "five candidates" are not GPIO at all

Resolving the base registers from the disassembly (no Ghidra needed — they are
`movw`/`movt` immediate pairs) shows the scan produced **false positives**. Its
premise was "offset `+0x14` = GPIO BRR", but every peripheral has a `+0x14`
register, so timer writes matched too:

| site | base reg loaded as | effective | verdict |
|---|---|---|---|
| `0x08029DCA` | `movw #0x1400` + `movt #0x4000` | `0x40001400` | **TMR7** — not GPIO |
| `0x0802B2D0` | `movw #0x5404` + `movt #0x4001` | `0x40015404` | APB2 timer range — not GPIO |
| `0x0802B308` | `movw #0x1C00` + `movt #0x4000` | `0x40001C00` | **TMR13** — not GPIO |
| `0x080295DE` | `mov sl, r0` @ `0x08029406` | function argument | **unresolved** |
| `0x08029DEE` | `ldr r0,[r5,#0/#4]` | pointer from a struct | **unresolved** |

GPIO bases are `0x40010800/0C00/1000/1400/1800`; none of the three resolved
sites is in that range.

**Net: the static scan finds NO confirmed GPIO LOW-drive in stock before
CONFIG_ENABLE.** Two sites remain genuinely unresolved because their base
arrives indirectly (a function parameter and a struct pointer) — those need real
dataflow analysis, which is what Ghidra is for.

**Lesson for the tool:** `find_gpio_pulses.py` must resolve the base register to
a GPIO port before reporting, rather than assuming the offset implies GPIO.
Until it does, treat its output as candidates to verify, not findings.

---

# Experiment H — SWD-driven status read on PARKED STOCK (2026-07-28)

First direct measurement of what the FPGA looks like **to stock** at the
CONFIG_ENABLE instant. Stock never reads the status itself (it ignores the
reply), so the only way to obtain it is to halt the MCU mid-sequence and clock
the transaction from the host. Tool: `scripts/swd_fpga_status.sh`.

Park verified from peripherals: `SPI3 CTRL1 = 0x00000347` (SPE=1, BR=0 = /2),
`GPIOB ODR = 0x00001910` (PB6/CS asserted LOW, PB11 HIGH).

## Headline result

```
STOCK, at the CONFIG_ENABLE instant:   00 03 90 20   = 0x00039020
```

**Identical to our firmware's `ED:00039020`.** The FPGA presents the *same
state* to stock as it does to us at the moment CONFIG_ENABLE goes out.

⇒ **FPGA state is NOT the differentiator.** Both firmwares face an identically
reporting part; stock's `0x15` leads to a working scope and ours does not.

## FALSE POSITIVE — corrected

The script initially reported `AFTER: 90 20 00 03` and printed
"EDIT_MODE ENGAGED". **That was wrong.** `90 20 00 03` is the same four bytes as
`00 03 90 20` rotated by two positions; the naive "bit 7 of the first byte"
check simply found `0x90` in slot 0. Same artifact class as `SS:20000390` in the
Exp A notes.

Control (device still parked, four consecutive reads, **nothing** sent between
them):

```
read1:  90 20 00 03
read2:  90 20 00 03
read3:  90 20 00 03
read4:  90 20 00 03
```

Stable. So the rotation is a **first-read-vs-subsequent phase offset** — the
first read follows the closing of stock's parked CS frame — and **`0x15`
changed nothing at all.**

**Instrument caveat to carry forward:** our `0x41` read framing has a two-byte
phase ambiguity between the first read and subsequent reads. Any status claim
must be checked against all four rotations of `{00,03,90,20}` before being
believed. `scripts/swd_fpga_status.sh` must be fixed to test rotations rather
than bit 7 of slot 0.

## The reframe this forces

We have now watched **stock's own bus, in stock's own FPGA state, receive
`0x15`, and NOT show SYSTEM_EDIT_MODE** — while stock, moments later in normal
operation, configures successfully and the scope works.

That undermines the framing this whole investigation has rested on since June:

> "CONFIG_ENABLE never engages SYSTEM_EDIT_MODE" = the wall

If stock reads `0x00039020` at the same instant and still succeeds, then
`0x00039020` is **compatible with successful configuration**, and
"EDIT_MODE never engages" may be a **measurement artifact rather than the
failure**. A plausible mechanism: reading STATUS requires its own CS frame, and
opening one may itself terminate the config session — i.e. the act of measuring
destroys what is being measured.

**Caveat, stated plainly:** the `0x15` we injected went out with ~100 µs
SWD-imposed inter-byte gaps versus ~17 µs native at /256 (and ~0.13 µs at /2),
with CS held LOW throughout. If the GW1N SSPI has a frame timeout, that alone
would void the injected-`0x15` half of this experiment. The **BEFORE** reading
is unaffected by this and stands on its own.

## What this means for the search

Excluded so far: bitstream bytes, framing, prelude, trailing clocks, timing
(Exp B2), analog frontend (Exp C), static MCU register state (Exp E/F), USART2
state, clock tree, the PC2/PB12 pulse candidates (Exp G) — and now **FPGA state
itself (Exp H)**.

Two live directions, in priority order:

1. **Stop trusting EDIT_MODE as the success signal.** Verify against DONE
   (bit 13) after a full upload instead, and confirm what stock's status reads
   at points where stock is *known* to have succeeded. If EDIT_MODE is
   unobservable by construction, every "wall" conclusion built on it needs
   re-examination.
2. **Wire-level comparison.** Everything state-shaped is now excluded, which
   leaves signal shape/timing. Capture our firmware's SPI3 lines with the
   HiLetgo 24 MHz analyser (slow the prescaler as maksidze did) and diff against
   the issue-#18 stock capture.

The FT232H JTAG oracle remains the guaranteed bypass — SRAM load only, **never
`--write-flash`.**

---

# Experiment I (2026-07-28) — the status register was never a status register

## What was run

Step 1 of the post-Exp-H plan: make the post-upload Gowin `0x41` STATUS read
valid. It had always run at `opt->cmd_br`, which the Exp F fidelity build sets
to 0 (`/2`) — the clock `fpga.c:1564` documents as producing garbage reads.
Forced it to `/256` unconditionally (`fpga.c` [6b]) and put a decoded DONE flag
and error nibble on the LCD overlay.

Prediction made before the flash: if the persistent `CFG:8001C810` was a
one-bit-early sample of `0x00039020`, then reading at `/256` would return
`00039020`.

## Bench result — bench unit #1, `make guest-fidelity` + the [6b] fix

```
Yellow:   CFG:00039020 D0 E0 L0 H0
Magenta:  S1:0347 ED:00039020 H2:Y
```

`S1:0347` = BR 0 = `/2` on the wire, so this is the Exp F condition with only
the read clock changed. Prediction confirmed exactly.

## The arithmetic

`0x8001C810` is `0x00039020` sampled one bit early:

```
0x00039020 = 0000 0000 0000 0011 1001 0000 0010 0000
prepend a 1, shift right one:
           = 1000 0000 0000 0001 1100 1000 0001 0000 = 0x8001C810
```

`0x8001C810` sets bits 4, 11 and 31, which are not defined bits in the Gowin
map at all — the giveaway that it was never a register value.

## The finding that matters

`0x00039020` is **not a register value either.** It is a bit-rotation of
`0xC8100001`:

```python
pat = 0xC8100001
rots = {((pat << i) | (pat >> (32 - i))) & 0xFFFFFFFF for i in range(32)}
0x00039020 in rots   # True
0x8001C810 in rots   # False  (a rotation PLUS a spurious leading bit)
```

`0xC8100001` is the free-running pattern the 2026-06-13 opcode-discrimination
probe recorded: opcodes `0x11` (IDCODE), `0x41` (STATUS), `0x00` (no-op) and
`0x13` (USERCODE) **all returned identical MISO `10 00 01 C8 10 00 01 C8`** —
the FPGA emitting a fixed 4-byte pattern from its running NV design and ignoring
MOSI entirely. `sibling_loader_config_diff.md:87` drew the right conclusion at
the time: *"The `0x41`='READY POR' decode was a spurious phase-slice of the
free-running stream (proven by `0x00` giving the same bytes)."*

Chance of a genuine status value landing on one of 32 rotations of that pattern
is ~7.5e-9. This is the stream, not a register.

**We have never successfully read the Gowin STATUS register.** Every status
number in this investigation — `ED`, `CFG`, the "refusal signature" — is a
phase-slice of a free-running pattern, at a phase set by the sampling clock.
`/2` and `/256` land on different rotations; that is the whole difference
between `8001C810` and `00039020`.

## Conclusions this retracts

- **Exp F's "RETRO-VALIDATES the refusal signature as genuine, not a
  floating-input artifact" — WITHDRAWN.** Pulling MISO up removed one artifact
  and left another. A stable reading is not a valid reading.
- **Exp H's "stock reads the same `0x00039020`, therefore FPGA state is not the
  differentiator" — UNSUPPORTED.** Both firmwares sample the same free-running
  stream, so of course they agree. The conclusion may still be true; the
  evidence for it is gone.
- **"SYSTEM_EDIT_MODE never engages" as the definition of the wall —
  UNSUPPORTED.** Bit 7 of a phase-slice means nothing. (Separately: bit 7 is
  bit 7 of the *assembled word*, i.e. `ED[3]`; this doc, the overlay comment and
  `scripts/swd_fpga_status.sh` all previously tested `ED[0]`/bit 31.)
- **apicula's confirmation does not rescue it.** They were asked what the bits
  mean and answered correctly. They were never in a position to judge whether
  the number was a real measurement.
- **The 2026-06-13 trailing-clock sweep (64/200/512 → "`0x41` stays
  80 01 C8 10") is unsupported as written** — an invalid but deterministic read
  shows "no change" whether or not anything changed. The conclusion may hold;
  it needs re-running against a valid readout.

Today's `D0 E0` is therefore NOT evidence that the config engine received
nothing. It is evidence that we still cannot see the config engine.

## The gap this exposes

Every measurement this project has taken of the FPGA has been **unanchored** —
we have never once read a value whose correct answer we knew in advance. That
is why three separate artifacts (garbage at `/2`, floating MISO, byte-rotation
in the SWD script) each survived for weeks: with no ground truth, a stable
wrong number is indistinguishable from a right one.

## Next: a known-answer test

Send `0x11` (READ_IDCODE) at `/256` and check for `0x0120681B` — a value we know
independently (Gowin GW1N-2 family; confirmed in the `.fs` preamble at file
offset `0x4AD19`).

- **IDCODE returns `0120681B`** → the FPGA *is* decoding SSPI opcodes. The June
  "not in config-receive mode" conclusion collapses, and with it the reasoning
  that sent this project toward JTAG.
- **All opcodes still return rotations of `C8100001` at `/256`** → the FPGA
  genuinely ignores SSPI while running its NV design. The June conclusion stands
  on a valid measurement for the first time, and JTAG is the route.

Either way it is the first anchored measurement in the investigation, and it
costs one build and one flash. Do this before any hardware step.

---

# Experiment J (2026-07-28) — the first anchored measurement: the FPGA answers

## What was run

`make guest-idcode` (fidelity image + `FPGA_IDCODE_PROBE=1`). On a pristine bus,
before the prelude, at `/256`, read four opcodes 8 bytes each and slide a 32-bit
window across all 33 bit alignments looking for the IDCODE `0x0120681B` — a value
known independently of any measurement we have taken (Gowin `.fs` preamble at
file offset `0x4AD19`). `0x11` is read again after CONFIG_ENABLE.

The bit-offset search and the 8-byte (rather than 4-byte) reads were deliberate:
every artifact this project has hit manifests as a phase shift, and 4 bytes
cannot distinguish "a register" from "a repeating pattern".

## Bench result — unit #1

```
ID:0120681B 0120681B     0x11 READ_IDCODE
NP:FFFFFFFF FFFFFFFF     0x00 no-op (control)
ST:00039020  US:00000000 0x41 STATUS, 0x13 USERCODE
ID@0 SM:N RP:Y PO@0
```

- `ID@0` — IDCODE found at bit offset **0**. Perfectly aligned, no phase shift.
- `SM:N` — `0x11` and `0x00` returned **different** bytes.
- Four opcodes, four distinct replies.
- `PO@0` — IDCODE still answers **after** CONFIG_ENABLE.

## Conclusion

**The FPGA decodes SSPI opcodes.** The bus, SPI mode (3), clock, CS framing and
wiring are all correct. MOSI is being read by the part.

### This refutes the conclusion that sent the project to JTAG

`sibling_loader_config_diff.md` (2026-06-13) concluded from `0x11`/`0x41`/`0x00`/
`0x13` all returning identical MISO that the FPGA "free-runs a fixed 4-byte
pattern and ignores MOSI" and is "not in SSPI config-receive mode", and that
"FT232H JTAG SRAM-load remains the route". Those reads were taken at `/2`
(`cmd_br` defaulted to 0 until 2026-07-27), where SSPI reads are garbage. At
`/256` the opcodes discriminate cleanly. **The JTAG rationale is substantially
weakened** — SSPI reaches the config engine fine.

### And it corrects Experiment I, from earlier the same night

Exp I concluded `0x00039020` was phase noise because it is a bit-rotation of
`0xC8100001`. The rotation is real — `0xC8100001` = `0x00039020` rotated left 15
— but **the causality was inverted.** `0x11` returning the known IDCODE proves
the `/256` read path is sound, and on that same validated path `0x41` returns
`0x00039020`. So `0x00039020` is the genuine register value and `0xC8100001` was
a misaligned `/2` read *of it*. June did not find a free-running pattern that
resembles the status register; June found the status register, mangled.

Reinstated: **Exp F's "retro-validates the refusal signature"** (it was right).
**Exp H's identical-status result** is meaningful again — its separate caveat
about SWD injected-byte timing is unaffected and still stands.

Still correct from Exp I: `0x8001C810` = `0x00039020` one bit early (it sets bits
4/11/31, undefined in the Gowin map); `/2` reads are invalid; and the
unanchored-measurement lesson, which is what produced this experiment.

## The wall, measured properly for the first time

```
SET   bit5  MEMORY_ERASE       clear bit7  SYSTEM_EDIT_MODE
SET   bit12 GOWIN_VLD          clear bit13 DONE_FINAL
SET   bit15 READY              clear bits 0-3  (no CRC_ERROR, BAD_COMMAND,
SET   bit16 POR                                 ID_VERIFY_FAILED, TIMEOUT)
SET   bit17 FLASH_LOCK
```

The part talks to us, reports **no errors at all**, and will not enter edit mode
on CONFIG_ENABLE. No CRC/BAD_COMMAND/ID_VERIFY means the bitstream bytes are not
being *rejected* — config entry never happens, so they are never parsed.

This is exactly apicula's answer (`docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md`,
2026-06-13): a running auto-booted GW1N re-enters configuration only via
RECONFIG_N (low pulse >= 25 ns) or a power cycle. We now have that on our own
measurement rather than on a correspondent's word.

## Open question

**DONE_FINAL is clear and POR is set** on a part that is supposedly running its
NV design and servicing USART meter traffic. That combination is not obviously
consistent, and it may be the thread that explains how stock succeeds. Worth
chasing before more pin hunting.

## Method change — now mandatory

**Anchor every FPGA measurement: read `0x11` at `/256` and confirm `0x0120681B`
before trusting anything else in the same session.** Three artifacts (garbage at
`/2`, floating MISO, the SWD script's byte rotation) each survived for weeks for
one reason — no measurement had a known correct answer, so a stable wrong number
was indistinguishable from a right one.

## Re-run backlog, now that readout is valid

1. `scripts/swd_fpga_status.sh` — add the IDCODE anchor, then re-run Exp H on
   stock. Also fix its SYSTEM_EDIT_MODE test: bit 7 of the ASSEMBLED word is
   byte[3], not byte[0] (it currently tests bit 31).
2. The 2026-06-13 trailing-clock sweep (64/200/512) — "no change" was measured
   through an invalid read.
3. The `0x3C` RELOAD test (`reload_3c`) — same.
4. With a live status readout, a RECONFIG_N hunt can now be run as a *search*
   rather than a guess: pulse a candidate pin, read STATUS, look for bit 7.

## Bench note

The first flash of this image came up with a dark screen and had to be recovered
via upgrade mode. Reflashing the identical image booted fine and produced the
result above, so the probe was not the cause. This unit has now shown an
unexplained early-boot hang twice (see also the splash-hang noted 2026-07-27).
It is intermittent, unexplained, and worth keeping in view — an early-boot fault
that corrupts a bench result would be easy to mistake for a real finding.

---

# Experiment K (2026-07-28) — Exp H redone, anchored: stock and ours are identical

Re-ran the SWD status read on parked stock with `scripts/swd_fpga_status.sh`
rewritten to anchor on the known IDCODE first (commit 2d43d34).

```
park       : SPI3 CTRL1 = 0x00000347  SPE=1 BR=0 ; PB6/CS=0 ; PB11=1
ANCHOR OK  : IDCODE raw 0120681B0120681B — found at bit offset 0
STATUS     : raw 0003902000039020 -> 0x00039020
             EDIT_MODE(bit7)=0  DONE(bit13)=0  ERR(bits0-3)=0x0
after injected CONFIG_ENABLE (see caveat): unchanged, re-anchor still offset 0
```

**Stock reads `0x00039020` at the CONFIG_ENABLE instant — bit-identical to our
firmware.** No errors, DONE clear, SYSTEM_EDIT_MODE clear. Exp H's conclusion is
reinstated on a validated channel: **the FPGA presents the same state to both
firmwares, so FPGA state is not the differentiator.**

Raw dump archived as `swd_fpgastatus_stock_anchored.txt`.

Note the IDCODE came back at offset 0 with no phase correction needed, and the
old 4-byte read's "90 20 00 03" rotation did not recur — consistent with that
having been a first-read framing artifact of the previous script.

The injected-`0x15` half again showed no change, but it still carries the timing
caveat (~100us SWD inter-byte gaps vs ~17us native) and should not be leaned on.

## Where this leaves the search

Both firmwares stand at the same instruction, with the same peripheral state
(Exp E: clock tree byte-identical, Exp F: all five enumerables closed), facing an
FPGA in the same state (Exp K), on a bus that demonstrably works (Exp J). Stock
then succeeds and we do not. **The divergence must therefore be in what happens
after that instant, at native speed** — the `0x15` -> `0x3B` -> 115,638 bytes ->
`0x3A` execution itself.

## Next: measure what SUCCESS looks like

The reference measurement this project has never taken is **the status of an FPGA
that has been configured successfully.** It does not need a spin patch:

1. Flash CLEAN stock. Boot fully. Confirm the scope actually works (trace on
   screen) — that is proof configuration completed.
2. Attach SWD, halt, and run the anchored read.

If DONE(bit13) is SET there, we finally know the success signature, and our
firmware's post-upload `D0` becomes a meaningful contrast. If DONE is CLEAR on a
demonstrably working scope, then DONE is not the success signal on this board and
every conclusion drawn from it — including today's — needs rethinking.

Either result is worth more than another pin experiment. Do this before parking
stock at later instructions (post-`0x15` at `0x0802DA62`, post-`0x3A`), because it
tells us which bit to even look at.

Disassembly reference for later parks (r6 = SPI3+0x08, so [r6,#0]=STS, [r6,#4]=DT):
```
802da42: 2015       movs r0,#0x15
802da44: 6070       str  r0,[r6,#4]   <- CONFIG_ENABLE hits the wire
802da48..802da60:   wait RDBF (bit0)
802da62: 6870       ldr  r0,[r6,#4]   <- 0x15 byte fully clocked; current park is before 802da44
802da64..802da7c:   wait TDBE (bit1)
802da7e: 2000       movs r0,#0        <- the dummy 0x00 follows
```
CAVEAT for a post-`0x15` park: reading STATUS requires opening a new CS frame,
which may itself end an active config session — so a clear EDIT_MODE there would
be ambiguous. DONE after `0x3A` is the more robust target, since it reflects a
completed configuration rather than a transient session flag.

---

# Experiment L (2026-07-28) — a configured FPGA stops answering SSPI

## What was run

Flashed CLEAN stock (`/tmp/clean.bin`, verified byte-identical to
`archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`), booted fully,
**confirmed the scope was drawing a trace** — proof that configuration completed
— then halted over SWD and ran the anchored read. No spin patch needed.

## Result — the anchor failed, and the script aborted

```
park       : SPI3 CTRL1 = 0x00000347  SPE=1 BR=0 ; PB6/CS=1 ; PB11=1
IDCODE raw : 0000000000000000
!! IDCODE NOT FOUND at any bit alignment. Aborting.
```

The FPGA does not answer `0x11` at all once it is configured and running the
scope design. Compare Exp J/K, where the same read on the same board returned
`0120681B` cleanly. The idle level flipped too: Exp J's `0x00` no-op control read
`FF...` on a pulled-up MISO, whereas this reads as actively driven low.

**Reading: the SSPI configuration port closes once configuration completes and
the user design takes over the bus.** That is consistent with the known protocol
— post-config, PB4/MISO carries ADC sample data for the `0x04`/`0x05` per-channel
reads, so the config engine is no longer the thing on the other end of the wire.

The success signature (DONE bit 13) was therefore NOT obtained; it is unreadable
at this point in the boot. The prediction going in was `DONE=1`; it was neither
confirmed nor refuted.

**Process note: this is the first time the anchoring discipline actively
prevented a false reading.** The old script would have printed
`STATUS 0x00000000  EDIT_MODE=0  DONE=0` — a plausible-looking, entirely
meaningless line that we would have spent time interpreting.

## What it does give us: a binary discriminator

| state | `0x11` READ_IDCODE | config port |
|---|---|---|
| stock, parked pre-config (Exp K) | `0120681B` | **open** |
| ours, post-upload (Exp I, via a valid `0x41` read) | STATUS `0x00039020` | **open** |
| stock, configured, scope working (Exp L) | zeros | **closed** |

Our firmware's post-upload `0x41` returning a valid `0x00039020` is itself
evidence that the config port is still open after our entire 115,638-byte
upload — i.e. we are definitively not configured. The contrast with stock is
already established; it is simply not yet IDCODE-anchored on our side.

Caveat: "no IDCODE" is not by itself proof of "configured" — a broken bus reads
the same way. It is only interpretable here because the scope was visibly
working at the moment of the halt.

## Next

1. **Cheap and airtight:** add an IDCODE anchor to our firmware's post-`0x3A`
   read (one call in the existing `probe_idcode` block). If IDCODE still answers
   after the full upload and close, "not configured" is proven on a validated
   channel rather than inferred from a status value.
2. **The real success signature:** park stock immediately after its `0x3A` close
   and read IDCODE + STATUS there. That is the only window in which a
   successfully-configured part might still answer the config engine. Requires
   locating the `0x3A` write in the disassembly and a new spin patch.

---

# Experiment M (2026-07-28) — anchored proof that we never enter config

## Result — bench unit #1, `make guest-idcode` + the post-`0x3A` anchor

```
Magenta: ID@0 SM:N RP:Y PO@0 CL@0
Yellow:  ST:00039020  US:00000000
```

| checkpoint | reading | config port |
|---|---|---|
| pre-prelude, pristine bus | `ID@0`, `SM:N`, `RP:Y` | **open** |
| after CONFIG_ENABLE (`0x15`) | `PO@0` | **open** |
| after the full 115,638-byte upload and `0x3A` close | **`CL@0`** | **open** |

Exp L established that a successfully configured part stops answering `0x11` —
the SSPI config port closes and the pins pass to the user design. Ours answers it
perfectly aligned at all three checkpoints.

**We are definitively not configured**, proven on a read path validated against a
known answer at every step rather than inferred from a status value.

And the status register never moves: `ST` (pre-prelude) = `ED` (post-CONFIG_ENABLE,
Exp F/I) = `CFG` (post-close, Exp I) = `0x00039020`, with the error nibble clear
throughout.

## What this pins down

No CRC_ERROR, no BAD_COMMAND, no ID_VERIFY_FAILED, no TIMEOUT — yet nothing is
configured. Combined with the port staying open, the bytes are being **silently
discarded, not rejected**: they were never treated as configuration data, because
the part never entered config mode. That is a different failure from "our
bitstream is wrong" or "our framing is wrong", both of which would produce an
error bit, and it is consistent with everything since Exp J.

The part decodes *reads* (IDCODE, USERCODE, STATUS all discriminate correctly)
but does not act on the config-mode *commands*. `0x15` in particular changes
nothing observable.

## Known defect in this build's overlay

The Exp J overlay block replaces the whole overlay and early-returns, so the
`CFG:... D E A` line never renders in `guest-idcode` — the `A` anchor added in
79ce306 was not visible. No information was lost (`CL@` is the same measurement
from the same read) but the two should not silently shadow each other.

## Next: use the instrument to search, not to confirm

Every remaining question is now cheap to ask, because STATUS and the open/closed
test are both trustworthy and both on-screen.

1. **Step-resolved status trace.** Read the anchored STATUS after each of `0x05`,
   `0x12`, `0x15`, the `0x3B` open, mid-upload, post-upload, post-`0x3A`. If it
   never moves at any step, the part is ignoring every config command while still
   answering reads — which localises the problem precisely and cheaply.
2. **Command-order variants.** Gowin's documented SSPI flow is CONFIG_ENABLE
   first, then ERASE_SRAM, then write. Stock's captured order is `05` -> `12` ->
   `15`, i.e. CONFIG_ENABLE last. Worth one build to try the documented order and
   watch STATUS.
3. **RECONFIG_N hunt as a search rather than a guess.** Exp G "refuted" PC2 and
   PB12 by watching a number we could not read. With live status, pulse a
   candidate and look for bit 7 immediately.
4. The `DONE=0` + `POR=1` + `MEMORY_ERASE=1` combination on a part supposedly
   running its NV design remains unexplained and may be the thread that matters.

---

# Experiment N (2026-07-28) — step-resolved: not one config command has any effect

## Result — bench unit #1, `make guest-trace`

```
T0:00039020 T1:00039020      pristine        / after 05 ERASE_SRAM
T2:00039020 T3:00039020      after 12        / after 15 CONFIG_ENABLE
T4:00039020 T5:00039020      after upload    / after 3A close
A:000000 MV:0 H2:Y
```

`A:000000` — the IDCODE anchor succeeded at every one of the six checkpoints, so
all six are measurements rather than placeholders. `MV:0` — **not one of them
differs from the pristine baseline.**

## What it establishes

The Gowin STATUS register does not change by a single bit across:

| command | effect on STATUS |
|---|---|
| `0x05` ERASE_SRAM | none |
| `0x12` INIT_ADDR | none |
| `0x15` CONFIG_ENABLE | none |
| `0x3B` + 115,638-byte bitstream | none |
| `0x3A` CONFIG_DISABLE | none |

while, in the same windows and on the same wire, every READ command answers
correctly — IDCODE aligned at offset 0, USERCODE `0x00000000`, STATUS a coherent
value, and the three discriminating cleanly from one another (Exp J).

**The SSPI read path works; the SSPI config-command path is inert.** This is not
a late-stage rejection (no CRC_ERROR, no ID_VERIFY_FAILED), not a partial load,
and not a framing problem. The part is servicing reads and treating every config
command as a no-op.

That asymmetry is exactly the documented behaviour of a running, auto-booted
GW1N: read commands remain available at all times, configuration commands are
refused until RECONFIG_N is pulsed or the part is power-cycled
(`docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md`, UG290). We now have that from our own
instrument at every step of our own sequence.

## The remaining contradiction, stated precisely

Stock configures this part successfully **from the same pre-state** (Exp K:
identical STATUS, identical FPGA state at the CONFIG_ENABLE instant), with the
same peripheral state (Exp E: clock tree byte-identical; Exp F: all five
enumerables closed), over the same working bus (Exp J), and with no narrow timing
window (Exp B2: a 5-10 s delay changed nothing). Yet stock's config commands take
and ours do not.

Everything reachable from the MCU's SPI3 pins is now excluded. Whatever stock
does to make the part accept configuration is **not on the SPI3 bus** — which is
consistent with the issue-#18 capture never having watched anything else.

## Next: the RECONFIG_N hunt becomes a search, not a guess

Exp G "refuted" PC2 and PB12 one pin per flash, while watching a status value we
could not actually read. Both premises are now fixed: the status is trustworthy,
and `MV` is a one-glance detector for "did anything change at all".

A single boot can therefore sweep many candidate pins — pulse a pin LOW->HIGH,
send `0x15`, read the anchored STATUS, and record whether anything moved — where
Exp G managed two pins in two bench cycles.

**Hardware-risk constraint on any such sweep.** Driving a pin that something else
is already driving is contention. The sweep must be restricted to pins stock
itself configures as outputs (per `stock_pre_fpga_gpio_state.md`) plus genuinely
unmapped pins, and must exclude:
  * `PC9`  power hold — the device dies instantly
  * `PC8`  power button
  * `PB3/4/5/6` SPI3 (the bus under test)
  * `PA13/PA14` SWD
  * `PA2/PA3` USART2
  * the EXMC/LCD bus
  * button-matrix inputs and any FPGA-driven pin (contention)

---

# Experiment O (2026-07-28) — conservative RECONFIG_N pin sweep: 20/20 negative

## Result — bench unit #1, `make guest-sweep`, SAVE in scope mode

```
SWEEP:DONE 20/20
BASE:00039020
HIT:-- 000000
H:0 AF:0
```

20 candidates pulsed LOW->HIGH (10ms) immediately before CONFIG_ENABLE, each
followed by an anchored STATUS read. **Zero hits. Zero anchor failures.**

`AF:0` matters: every one of the 20 readings passed its IDCODE anchor, so the
config port stayed open and answering throughout and this is a genuine negative
rather than 20 failed measurements. `BASE:00039020` matches every other reading.

Candidates (all pins stock drives as outputs per `stock_pre_fpga_gpio_state.md`,
ordered by prior): PC6, PB11, PC11, PC2, PB12, PB9, PA6, PC12, PE4, PE5, PE6,
PA15, PA10, PB10, PB0, PC5, PC10, PE2, PE3, PB7.

This also re-runs Exp G (PC2, PB12) with a status readout that actually works,
and extends Exp C's static-posture ablation of the frontend bank into a
transition test. Both stay refuted.

## KNOWN BLIND SPOT — do not over-read this result

The detector takes **one status snapshot at a fixed +1ms after the pulse**. If
pulsing the real RECONFIG_N causes the part to reconfigure from its NV flash,
that is a TRANSIENT: the status changes and then settles, plausibly back to a
value indistinguishable from the baseline by the time we sample.

So what Exp O establishes is: **no candidate produced a PERSISTENT status change
at +1ms.** That is weaker than "no candidate is RECONFIG_N", and the gap should
be closed before the conservative pin hypothesis is retired.

Fix: sample the status repeatedly after each pulse (e.g. ~20 reads over 200ms)
and flag ANY deviation at any point, rather than one snapshot at one delay.

## Second gap — the candidate list has a hole by construction

The list came from `stock_pre_fpga_gpio_state.md`, which enumerates the window
`0x0802AA50 -> 0x0802D63C` — master-init entry to the first SPI3 byte. Anything
stock does BEFORE master init is outside it. That is not hypothetical: the same
doc notes PC9's power-hold is asserted by the reset/clock-init stub at
`0x08000000`, before master init runs.

An early hardware-init pulse is exactly where a RECONFIG_N assertion would
sensibly live, and we have never scanned there. `scripts/find_gpio_pulses.py`
starts at `MASTER_INIT_ADDR` and inherits the same hole.

## Next, in order

1. **Zero bench time:** rescan for GPIO pulses across the whole image up to
   CONFIG_ENABLE, including the reset/clock-init stub and everything before
   master init. Fix the script's base-register resolution first — the 2026-07-27
   run reported 5 candidates of which 3 turned out to be timers.
2. **Sweep v2:** transient-aware sampling, closing the blind spot above.
3. **Broad sweep:** pins that NEITHER firmware drives, accepting the contention
   risk that the conservative list was chosen to avoid.

---

# Experiment P (2026-07-28) — static GPIO pulse scan, whole image

Zero bench time. `scripts/find_gpio_pulses.py` rewritten and run over clean stock
across the ENTIRE image up to CONFIG_ENABLE (`0x08007000..0x0802DA42`), reset stub
included. Full output: `stock_gpio_pulse_scan_2026-07-28.txt`.

## Script fixes (the previous version could not have found this)

1. **Scan window.** It started at MASTER_INIT and so could never see anything
   before it. Now the whole image.
2. **Base-register resolution.** It reported any store to `+0x0C/+0x10/+0x14` as
   GPIO without resolving the base — every peripheral has those offsets, which is
   why the 2026-07-27 run gave 5 candidates of which 3 were timers. Now resolved
   by backward dataflow (literal pool read out of the binary, movw/movt pairs,
   register moves, constant adds), and unresolvable stores are reported
   separately rather than dropped or counted.
3. **movt handling — the bug that made the first corrected run report ZERO hits.**
   Walking backwards, `movt` is met BEFORE the `movw` that supplies the low half.
   Treating it as an opaque write aborted resolution on every movw/movt-built
   base, which is most of them.
4. **Pin masks.** The stored value is now resolved too, so a LOW and a HIGH can be
   paired into a pulse. Without the mask the register alone says nothing.

**Validated against a known answer before the output was believed:** the scan
finds `0x0802AAB6  GPIOC clr/BRR  pin 9` — the PC9 power-hold write documented in
`stock_pre_fpga_gpio_state.md`. Same discipline as the IDCODE anchor.

## Result — 103 GPIO level writes; pins driven both LOW and HIGH

```
PA15  PB10 PB11  PC1 PC2 PC4 PC9 PC11  PD12  PE4 PE5 PE6
```

**Not covered by the Exp O sweep: PC1, PC4, PC9, PD12.**

| pin | standing |
|---|---|
| **PC1** | **entirely new** — in no pinout doc, in neither sweep, never considered |
| PC4  | hunted 2026-06 (commit 3c53e53, "negative") under the unreadable-status regime |
| PD12 | `strap_pd1213` tested it as a HELD level, never as a pulse |
| PC9  | power hold — stays excluded; driving it low kills the device |

Conversely, Exp O spent 12 of its 20 slots on pins stock never drives LOW at all
(PA6, PA10, PB0, PB7, PB9, PB12, PC5, PC6, PC10, PC12, PE2, PE3). Defensible a
priori, but the scan says they were never pulse candidates.

## Null result worth recording

**There is no GPIO level write in the reset/clock-init stub** (`0x08007000` to
`0x08007238`); the earliest in the whole image is `0x080088E4`. The gap that
motivated this scan turned out to be empty. The missed pins were hiding in helper
functions instead — which is exactly why a static-window enumeration
(`stock_pre_fpga_gpio_state.md`, master-init entry to first SPI3 byte) did not
list them.

## Caveat — address order is not execution order

Pairs are reported in ADDRESS order. A LOW at `0x0800DFCC` paired with a HIGH at
`0x08023758` spans different functions and is almost certainly not a runtime
pulse. **The reliable output of this scan is the PIN SET, not the pairings.**
Each candidate still has to be confirmed on the bench.

## Next

Sweep v2: add **PC1, PC4, PD12**, drop the 12 pins stock never drives LOW, and
fix the Exp O blind spot by sampling the status repeatedly after each pulse
(~20 reads over 200ms, flagging any deviation) instead of one snapshot at +1ms.

---

# Experiment Q (2026-07-29) — sweep v2: pin-pulse theory effectively dead

```
SWEEP:DONE 12/12
BASE:00039020
HIT:--/- H:0 AF:0
```

12 candidates from the Exp P static scan — every pin stock drives both LOW and
HIGH before CONFIG_ENABLE, minus PC9 (power hold) — each pulsed LOW->HIGH and
then watched across two ~120ms sampling windows: one after the pulse alone, one
after CONFIG_ENABLE. **No deviation from baseline at any sample, on any pin, in
either phase.** No anchor failures, so all 12 are genuine measurements.

This closes the Exp O blind spot (a single snapshot at +1ms, which could miss a
transient reconfiguration) and covers PC1, PC4, PD12, PD13 — pins Exp O never
tested. PC1 in particular appears in no pinout doc and had never been considered
before the corrected scan surfaced it.

A prior run of the v1 image also reproduced Exp O exactly (20/20, no hits,
AF:0, same baseline), so the negative is stable across builds.

**Verdict: no MCU-driven GPIO pulse unlocks config entry.** Combined with Exp C
(frontend posture), Exp E/F (static MCU state), Exp G (PC2/PB12 pulses) and Exp P
(the pin set is complete for pins stock drives at all), the "stock pulses
something" theory is out of room on the MCU side.

## Back to an anomaly we noted and walked past

The status we read at every checkpoint, on both firmwares, decodes as:

```
SET   MEMORY_ERASE(5)  GOWIN_VLD(12)  READY(15)  POR(16)  FLASH_LOCK(17)
clear SYSTEM_EDIT_MODE(7)  DONE_FINAL(13)  and all four error bits
```

**POR set and DONE_FINAL clear is not what a part that has auto-booted its NV
design should report.** DONE goes HIGH when configuration completes. A GW1N
running a resident design ought to show DONE set; this one shows POR — the
power-on-reset state — and no DONE at all.

That was flagged as an open question in Exp J and never chased. It now looks
important, because it suggests the part may not have finished (or begun) loading
a design at the moment we talk to it.

Which raises a hypothesis this project has never tested: **our firmware may reach
the FPGA handshake TOO EARLY.** Exp B2 established that LATE is fine — a 5-10s
delay inserted into stock changed nothing, so there is no narrow window closing
behind us. But nothing has ever tested the other direction. Stock performs a very
large amount of init before its handshake (LCD/EXMC, FatFs, SPI flash, DAC/ADC,
USART2, the frontend relays); our firmware is far leaner and reaches the same
point much sooner after power-on. If the GW1N is still in its own power-up
sequence when our CONFIG_ENABLE arrives, ignoring it is exactly the observed
behaviour — reads answered, config commands inert, POR set, DONE clear.

## Next, cheapest first

1. **Pre-config delay sweep.** Insert a large delay (1s, 3s, 5s) before the whole
   sequence and re-run the Exp N step trace. One build, one flash, one button.
   Directly tests the too-early hypothesis and is motivated by the POR/DONE
   anomaly rather than by guessing.
2. **Park stock AFTER its 0x3A close** and read the anchored status/IDCODE there.
   We have measured stock only BEFORE its config commands. We have never measured
   stock where it SUCCEEDS, so we still do not know what success looks like on
   this board (Exp L could not get it — the port had already closed by then).
3. **Logic analyzer.** Everything state-shaped is now excluded; what remains is
   the wire itself. Capture our SPI3 at /64 and diff against the issue-#18 stock
   capture.

---

# Corrections from the Ghidra-side review (2026-07-29)

`next_five_experiments_2026-07-28.md` (parallel Ghidra-side effort) raises two
corrections to this document. Both are right and are adopted.

## 1. Exp K's conclusion was stated too strongly

Written: *"the FPGA presents the same state to both firmwares, so FPGA state is
not the differentiator."*

Exp K measured **STATUS** — one 32-bit view. "Will the next config command be
honoured" need not be exposed there; `EDIT_MODE` reads 0 in both cases, which is
the very thing under investigation. A part pulsed on RECONFIG_N earlier and
reloaded from NV flash would present an identical STATUS while differing in
config-FSM state.

Supported: **"STATUS is not the differentiator."**
Not supported: "FPGA state is not the differentiator."

This matters because the stronger reading retires the RECONFIG_N hypothesis, and
Exp O/Q's own disclosed blind spots mean it has not been tested to exhaustion.
Corrected in `CLAUDE.md` too.

## 2. Exp B2 does not bound the trigger→config-enable interval

B2 pushed stock's config-enable 5-10s later via a code-cave busy loop and stock
still configured. That refutes a narrow **absolute** window from power-on. It does
not refute "stock asserts a trigger shortly before config-enable": the cave sits
ahead of the whole FPGA-init block, so trigger and config-enable shift **together**
and their relative spacing is preserved.

So a pin trigger should be looked for **near config-enable in the instruction
stream**. Both scans agree there are no GPIO writes at all between `0x0802D63C`
(last GPIO write before the prelude) and `0x0802DA42` (CONFIG_ENABLE) — which
constrains, but does not close, that search.

## 3. A gap in Exp Q

**Stock drives PC1 and PC2 as a PAIR** — same instruction pair, both arms of the
mode branch at `0x0802C618`, verified here against the disassembly:

```
802c624: movs  r0,#4          ; PC2
802c626: str.w r0,[sl,#0x10]  ; sl = GPIOC BSRR -> SET
802c62a: movs  r0,#2          ; PC1
802c62c: str.w r0,[sl,#0x10]  ; SET
```

Exp Q pulsed PC1 **singly**. Stock never drives one without the other, so a
single-pin test may not reproduce the condition. **The paired case has never been
run.** (PC1 is `Unknown` in `HARDWARE_PINOUT.md:73`; our firmware never drives it.)

## 4. Blind spots that list adds to ours

Recorded because they are not closed by anything we have run:

* **Peripheral-driven (AF-mode) pins produce no GPIO store at all** and are
  invisible to every scan run to date, including Exp P. Exp E already noted
  **PB9 is AF-PP in stock and floating in ours.**
* **DMA-driven BSRR writes** produce pulses with no instruction anywhere. Only
  DMA1 Ch1 (LCD) is accounted for.
* **`FUN_080165A8`: 25,548 bytes, zero direct callers, larger than `master_init`,
  never examined.**
* **STATUS may be the wrong detector.** Exp L found a fully configured part stops
  answering SSPI; a part *in config mode* must still answer. That is three states
  — running-NV, in-config, configured — and our detector distinguishes two.
* The indirect `blx` targets in the reset stub are still not followed.

## Where we differ on ordering

That document recommends its experiment 3 (Unicorn execution-ordered trace)
first, as "zero bench cost". It is zero *bench* cost but not zero cost: the
harness has never run past legacy `0x08024412`, and repairing an emulator enough
to trace a 15KB function is a substantial job.

Higher value per unit of effort, in our view:

1. **Paired PC1+PC2 pulse, and PC1/PC2 held at stock's level** — one build each,
   and the concrete gap in Exp Q.
2. **Their experiment 4 — physically ohm out RECONFIG_N.** This is the one to
   elevate. It converts a search over ~50 candidate pins into a single
   measurement, and its NEGATIVE case is the more valuable: if RECONFIG_N lands on
   an RC or a supervisor rather than the MCU, then Exps G/O/Q were unwinnable by
   construction and no firmware-side pin experiment can ever succeed.
3. **The pre-config delay test** (Exp Q's closing note) — still cheap, still
   untested, and motivated by the unexplained POR-set/DONE-clear reading.

---

## Experiment R (2026-08-11) — the pins a LEVEL diff could not see

**Motivation.** The session plan's next item was a "paired PC1+PC2 pulse", on the
grounds that Exp P listed PC1/PC2 as pins stock drives both LOW and HIGH, and
Exp Q had only ever pulsed PC1 singly.

**The premise did not survive the disassembly.** `0x0802C608` is a 4-way selector
that drives a 2-bit code onto PC2:PC1 —

```
[r9,#20]==0                    neither pin touched
        ==1  (0x0802C624)  BSRR=4, BSRR=2   -> PC2 H, PC1 H
        ==2  (0x0802C64A)  BRR =4, BRR =2   -> PC2 L, PC1 L
        ==3  (0x0802C632)  BSRR=4, BRR =2   -> PC2 H, PC1 L
```

Both writes in the `==1` arm target BSRR. **No arm produces a low-then-high edge:
stock never pulses these pins.** Exp P flagged them as "driven both ways" only by
aggregating across different arms of one switch. The same PC2-then-PC1 pattern
appears at ~8 sites across the image (`0x0800C99E`, `0x0800D9D6`, `0x0800DFB8`,
`0x08023758`, `0x08023C54`, `0x08023CB8`, `0x08023E16`, `0x0802C626`), which is
the signature of a functional selector, not a config strap — RECONFIG_N would be
driven once, in init.

**The runtime unknown was already on disk.** The plan proposed parking stock at
`0x0802C618` over SWD to read `r0` — which rule 2 makes impossible anyway. But the
Exp E spin-park is at `0x0802DA42`, *after* this code, so `swd_stock.txt` already
records the outcome: ODT bit2 set, bit1 clear ⇒ the `==3` arm ⇒ **PC2 HIGH,
PC1 LOW** at the CONFIG_ENABLE instant.

### The real finding: Exp F's exclusion was scoped to its method

Chasing why PC1 had never been a candidate surfaced a whole blind spot. The Exp E
enumeration compared output **levels** (ODT). At the CONFIG_ENABLE instant:

| | PC1 CRL nibble | PC1 ODT bit1 |
|---|---|---|
| stock | `0x1` — output push-pull 10MHz, driven LOW | 0 |
| ours  | `0x4` — floating input                     | 0 |

**A driven-low output and a floating input report the same bit.** Every pin in
that class was invisible. Re-diffing the same two dumps on the CONFIG registers
(CRL/CRH) instead of ODT gives ~20 differences; after removing pins with known
functions the open set was **PC1, PA6, PC11, PD2, PD3, PD6, PD13**.

So Exp F's "all five enumerables closed ⇒ static MCU state is EXCLUDED" was true
only of what a level-based diff could reach. Same failure mode as the `/2` reads,
the floating MISO and the script's byte rotation: a measurement that could not
have seen the thing it was used to rule out.

### Results — both NEGATIVE

`make guest-pc1` and `make guest-fidelity2`, layered on FPGA_STOCK_FIDELITY (left
byte-identical to Exp F so the comparison stays valid).

| build | pins driven at stock's levels | result |
|---|---|---|
| `guest-pc1` | PC1 LOW (with Exp F's PC2 HIGH = stock's full 2-bit code) | `S1:0347 ED:00039020` |
| `guest-fidelity2` | + PA6 L, PC11 L, PD2 L, PD13 L, PD3 H, PD6 H | `S1:0347 ED:00039020` |

`S1:0347` confirms BR=/2 on-device in both, so the fidelity base was intact and
these are clean negatives. **The config-register class is now closed. Static MCU
state is genuinely excluded** — this time by a diff that could see it.

### Side result: a true FPGA power cycle does NOT open the config port

The `guest-fidelity2` reading was re-taken after real power removal, which on this
unit requires a specific sequence — worth recording, because nothing else achieves
it:

> hold POWER → "Goodbye" → **unplug USB (device goes dark)** → replug → boots

The pinhole reset resets the MCU only; the FPGA stays powered. A POWER-button
shutdown with USB attached also does **not** remove power — VBUS keeps the rail up
and the device hangs on "Goodbye". Only the dark moment after unplugging drops FPGA
power. Much of this project's testing has therefore run on an FPGA that had not
been power-cycled in a long time.

Status after a genuinely cold FPGA start: **`00039020`, unchanged.** Combined with
Exp B2 (stock delayed 5–10 s still configured), both halves of apicula's answer sit
awkwardly with the evidence: it is not a post-power-on window, because stock does
not need one and we do not benefit from one.

**Correction to bench procedure:** the standing note that "after an IAP flash the
device reboots into charge-display mode, press POWER for a real boot" is
**stock-only**. Our builds auto-boot straight into the app after flashing. No
reading in this project is invalidated by it, but the instruction is wrong as
written for our images.

### Also refuted, statically: a USART-borne config trigger

The Ghidra-side correction left "a trigger asserted NEAR config-enable in the
instruction stream" unrefuted (Exp B2 shifted trigger and config-enable together,
preserving spacing, so it bounds only an ABSOLUTE window). Every GPIO form of such
a trigger is now excluded, so the remaining channel to the FPGA is USART2.

Stock's USART2 setup at `0x0802C8D2`–`0x0802C936` (`fp` = `0x4000440C` = CTRL1):

```
[fp,#-4] BAUDR   baud (9600)
[fp]     CTRL1   bic #0x1000    clear DBN0
[fp,#4]  CTRL2   bic #0x3000    clear STOP
[fp]     CTRL1   orr #8         TEN
[fp]     CTRL1   orr #4         REN
[fp]     CTRL1   orr #0x20      RDBFIE
[fp]     CTRL1   bic #0x2000    clear UEN
```

Final `CTRL1 = 0x2C`, matching the Exp E dump. **There is no access to `[fp,#-8]`
= `0x40004404` = the data register**, and UEN is explicitly cleared, so no USART
byte can leave the MCU before CONFIG_ENABLE. A USART "prepare for reconfiguration"
command is refuted. (This also independently re-confirms the UE=0 question settled
by Exp E.)

### Open thread raised but not tested: is the meter even the FPGA?

Exp N's `T0` checkpoint is captured **before any config command is sent**
(`fpga.c:2046`) and reads `00039020` — `DONE_FINAL` clear, config port answering.
Per Exp L, a *successfully configured* part stops answering SSPI entirely. So at
T0 our FPGA presents as unconfigured — yet our meter works (DCV and resistance both
accurate).

If the FPGA fabric serviced the meter it would have to be configured. Either the
meter is **not the FPGA** (a separate device on the USART2 line), or `DONE_FINAL`=0
does not mean unconfigured for an NV-booted part.

This matters because CLAUDE.md's framing — "the resident NV design is meter-only" —
rests on Exp A showing the meter survives ablation of the config upload, which
equally supports "the meter was never the FPGA". If there is no resident design,
apicula's "a running auto-booted part will not re-enter config" may simply not
apply to this board. That is the question already posted on issue #18.

**Caveat against over-reading it:** Exp A also showed ablation kills the scope, so
stock's scope genuinely depends on the SSPI upload. Any theory in which the config
port is closed to *everyone* has to explain how stock succeeds through it.

---

## Experiment S (2026-08-11) — Gowin RELOAD (`0x3C`): NEGATIVE, and it closes the last documented route

**Build:** `make guest-reload` = the Exp F fidelity base (`FPGA_STOCK_FIDELITY=1`,
`FPGA_USART_SILENT_SCOPE=1`) plus `FPGA_RELOAD_3C_BUILD=1`. The send path is at
`fpga.c` `[0a]`: `0x3C 0x00` at `/256` in its own CS frame, before the prelude,
followed by a 50 ms settle. The `0x3C` frame is therefore the **only** difference
from the Exp F/R images.

**Result:**

```
ED:00039020   S1:0347   H2:Y
```

`S1:0347` confirms the fidelity base was intact (SPI3 CTRL1 BR=0, stock's `/2`),
so this is a clean single-variable test. `ED` is bit-identical to every reading
since June: bits 5 (MEMORY_ERASE), 12 (GOWIN_VLD), 15 (READY), 16 (POR),
17 (FLASH_LOCK). **SYSTEM_EDIT_MODE (bit 7) still clear. No error bits.**

RELOAD joins `0x05`, `0x12`, `0x15`, the 115,638-byte `0x3B` upload and `0x3A` on
the list of config commands that do not move the status register by a single bit
(Exp N), while every *read* opcode answers correctly on the same bus (Exp J).

### Why this one mattered

Gowin documents exactly three ways to make a running, auto-booted part accept a
new configuration. As of tonight all three are refuted on this unit:

| Route | Closed by |
|---|---|
| RECONFIG_N low pulse (≥25 ns) | Exps G/O/Q — 20 + 12 candidate pins, transient-aware, zero anchor failures. Plus maksidze's measurement that QN48 pin 48 sits always HIGH and never pulses |
| Power cycle | **Exp R** — first genuine FPGA power cycle in the project (hold POWER → "Goodbye" → unplug USB → replug). Status unchanged |
| **RELOAD command** | **Exp S — this experiment** |

This had never been run on a validated readout. The knob and its send code have
existed since June, but the only way to reach them was the debug shell, which
needs USB CDC (never enumerated on this unit) or RTT (impossible while RDP is
set). There was no make target. It sat in the re-run backlog for that reason
alone, alongside the trailing-clock sweep and Exp H on stock.

### What it leaves

A sharper contradiction than we had this morning. Every documented route into
configuration fails for us, while stock traverses one of them at every boot and
its scope demonstrably depends on doing so (Exp A). Something in the model is
wrong, and the candidates are no longer MCU-side — that class is closed by
Exps E/F/R.

The two live ones, in order:

1. **Is the meter even the FPGA?** (§ "Open thread" above, posted on issue #18.)
   If there is no resident NV design on our part, apicula's "a running auto-booted
   part will not re-enter config" may not describe this board at all, and the
   whole framing of the wall changes.
2. **Is RECONFIG_N even connected to the MCU?** Asked on #18 2026-08-11. If pin 48
   has no trace back to the MCU — strapped, or pulled up with nothing driving it —
   then no firmware change could ever have worked, and Exps G/O/Q were unwinnable
   by construction rather than merely unlucky.

### Method note

The bench also produced a reminder worth recording: **a visible scope trace is
not evidence of success.** `scope_ui.c:334` renders real ADC samples when
`fpga_data_ready()`, and falls back to a synthetic square wave (line 379) when no
data has ever arrived. Since the FPGA never configures, the demo trace renders on
every boot. There is a latch at `scope_ui.c:337` — the first real sample disables
the fallback permanently for that boot — so the *disappearance* of the demo trace
is a genuine success indicator. Its presence is not a failure indicator; it is
the default state.
