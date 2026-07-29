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
