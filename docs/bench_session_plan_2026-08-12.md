# Next Session Plan — pick up cold

**Written 2026-08-11 at the end of a long session.** Assumes you remember nothing.
Read "State of the world" and "Do not re-chase" before running anything.

Supersedes `bench_session_plan_2026-07-30.md`, which is still worth reading for
the DMM/microscope task detail (Task 1 there) — that work is still outstanding.

---

## State of the world

**Repo.** `main` is current and is the default branch; everything merged
2026-08-11. Only two branches exist: `main` and `experiment/fpga-config-capture`
(kept as the sole copy of a June bench build, marked not-for-main). Tree clean.

**Device.** Bench unit #1 is running `OPENSCOPE_plainguest.bin` — a plain
`make guest` including the new fault handler. Fully usable; the meter works.

**Community.** Three replies posted 2026-08-11 and awaiting responses: PR #16
(re-review, one open item), #18 (the JTAG-premise correction to maksidze, plus
the 2C23T question), #22 (verification ask to michkovitalik). A public devlog
now exists at `docs/devlog/` — worth keeping to a cadence.

---

## The problem, in one paragraph

The oscilloscope needs the MCU to upload a 115,638-byte configuration to the
Gowin GW1N-UV2 at boot. Stock does this successfully every time; ours does not.
Measured: the FPGA **decodes our SSPI commands correctly** (IDCODE, USERCODE and
STATUS all answer, all distinct), reports **no errors at all** (no CRC, no
BAD_COMMAND, no ID_VERIFY, no TIMEOUT), and **not one config command moves the
status register by a single bit**. The bytes are silently discarded because the
part never enters configuration mode — the documented behaviour of a GW1N that
has already auto-booted its resident NV design. And the FPGA is in a
**measurably identical state** when stock does it and succeeds.

---

## Two rules that cost weeks to learn

### 1. Anchor every FPGA measurement

Read IDCODE (`0x11`) at `/256` and confirm `0x0120681B` **before believing any
other value from the same session**. If it fails, discard the reading — do not
report it. Three artifacts survived weeks without this: garbage reads at `/2`, a
floating MISO, and a byte-rotation in a script. A stable wrong number is
indistinguishable from a right one.

### 2. SWD kills this CPU — it is a halt-and-poke instrument only

`FLASH_OBR = 0x03FFFFFE` → flash read protection is **active**. On this part
family, bringing up the debug port **disables the flash array**, and the CPU
executes from flash. So attaching SWD locks the core up instantly
(`pc=0xFFFFFFFE`, `IPSR=3`). This is not a readout glitch — the core really is
dead.

**But the constraint is narrower than it sounds, and the ST-Link is one of the
better instruments on this bench.** RDP kills anything needing a *live CPU with a
debugger attached*. It does **not** touch the debug port's memory access: the host
can read and write any peripheral register while the core is dead, and the FPGA
neither knows nor cares why the CPU stopped.

`scripts/swd_fpga_status.sh` already works this way — it drives SPI3 entirely from
the host (writes `CTRL1`/`DT`, polls `STS`, toggles PB6/CS via `GPIOB` BSRR/BRR,
lines 78-92) with the core dead.

Consequences:
* **Read live firmware state from the LCD overlay, never over SWD.**
* SWD is good for: attach, accept the CPU dies, then drive peripherals / SPI3
  **from the host**. That is exactly how Exps E/K/L worked and they remain valid —
  peripherals keep whatever the firmware configured before the attach.
* **A host-driven pin sweep needs no reflashing at all** (see § 0b below). This is
  strictly more capable than the `guest-sweep` builds.
* **RTT can never work here** while RDP is set. Neither can any "sample pins live
  over SWD" scheme. Both need a running CPU with a debugger attached.

---

## Do not re-chase

Each of these is closed by a bench measurement, not by argument:

| Excluded | By |
|---|---|
| Bitstream payload bytes | `0x4AD19` fix; byte-exact vs the issue-#18 capture |
| Framing / CS polarity / trailing clocks | stock-faithful sequence; 256 trailing clocks |
| A narrow absolute timing window from power-on | Exp B2 (stock delayed 5-10 s, still configured) |
| Analog frontend posture | Exp C |
| Static MCU register state | Exp F (all five enumerables closed) |
| Clock tree | Exp E (CRM registers byte-identical) |
| The SPI3 bus itself | Exp J (IDCODE/USERCODE/STATUS answer and discriminate) |
| Our sequence ever entering config | Exp M / N |
| MCU-driven RECONFIG_N GPIO pulses | **maksidze measured QN48 pin 48: always HIGH, never pulses.** Exps G/O/Q were unwinnable by construction |
| 20 + 12 candidate pins, pulsed | Exp O / Q, transient-aware, zero anchor failures |
| The ESP32 SSPI route | mooted by Exp J — an external master hits the same wall |
| The user-mode-lockout / USART-silent theory | bench-refuted 2026-06-13 |
| **A "~55 s HardFault"** | **Does not exist.** Retracted 2026-08-11 — it was the debugger attach (see rule 2) |
| **Static MCU register state — for real this time** | **Exp R.** Exp F's exclusion was scoped to its method; Exp R closed the class it could not see (below) |
| Paired PC1 + PC2, held at stock's exact 2-bit code | **Exp R** — `guest-pc1` and `guest-fidelity2`, both `00039020` |
| A USART-borne "prepare for reconfig" command | **Exp R**, statically: stock clears UEN and never writes `0x40004404` before CONFIG_ENABLE |
| A fresh FPGA power-on reopening the config port | **Exp R** — first true FPGA power cycle in the project; status unchanged |

---

## Still genuinely open

1. ~~**Paired PC1 + PC2**~~ — **DONE 2026-08-11, negative (Exp R), and the framing
   was wrong.** `0x0802C608` is a 4-way selector driving a 2-bit code; every arm
   writes BSRR *or* BRR, never both, so **stock never pulses these pins**. The
   held code (`PC2 H, PC1 L`) was tested and the wall held.
2. **`POR`(16) SET with `DONE_FINAL`(13) CLEAR** — not what a part that has
   auto-booted its NV design should report. Unexplained since Exp J.
3. **MODE0 (QN48 pin 13) and JTAGSEL_N (pin 3)** — never measured. MODE0 sets the
   config-mode group. Caveat: a static strap is symmetric, so it cannot by itself
   explain why stock succeeds and we do not — but it is 20 minutes with a DMM and
   would force that contradiction into the open.
4. **AF-mode pins are invisible to every scan run so far** — a pin driven by a
   peripheral emits no GPIO store. `PB9` is AF-PP in stock and floating in ours.
5. **DMA-driven BSRR writes** leave no instruction to find. Only DMA1 Ch1 (LCD) is
   accounted for.
6. **`FUN_080165A8`** — 25,548 bytes, zero direct callers, larger than
   `master_init`, never examined.
7. **Whether STATUS is even the right detector** — there are three states
   (running-NV, in-config, configured) and ours distinguishes two.
8. **Does our firmware arrive too early?** Exp B2 established that *late* is fine;
   nobody has tested *earlier*. Stock does far more init before its handshake.

---

## Plan, in order

### 0. DONE — Experiment R (2026-08-11)

Item 1 below is complete and negative. Its real value was methodological: it
found that **Exp F's "static MCU state is EXCLUDED" was scoped to its method.**
That diff compared output LEVELS (ODT), where a pin stock drives LOW and a pin we
leave FLOATING both read 0. Diffing the CONFIG registers (CRL/CRH) instead
surfaced seven more pins — PC1, PA6, PC11, PD2, PD3, PD6, PD13 — all since
matched, singly (`guest-pc1`) and together (`guest-fidelity2`), both `00039020`.

The exclusion is now genuine. Two knobs remain in the tree for reuse:
`FPGA_FIDELITY_DRIVE_PC1`, `FPGA_FIDELITY_DRIVE_UNCOVERED`.

**Read this before proposing anything:** that is the fourth time a stable-looking
measurement turned out to be blind to the thing it was being used to rule out
(after the `/2` reads, the floating MISO and the script's byte rotation). Before
running a new experiment, ask what its instrument *cannot* see.

### 0b. The instrument to use next: a HOST-DRIVEN pin sweep  ⏱ no reflash

Not a hypothesis — a better way to test the ones left. Boot normally so the
firmware configures clocks/SPI3/GPIO → attach SWD (CPU dies, peripherals keep
their state) → from OpenOCD, drive any pin's BSRR/BRR and read anchored STATUS
back over the same channel. Each candidate is a few TCL lines instead of a
build → flash → power-cycle → read cycle.

Reaches what the `guest-sweep` builds cannot:

* **arbitrary pulse shapes and hold durations**, no rebuild per variant
* **AF-mode pins** — reconfigure as GPIO from the host first, then drive.
  This is the standing blind spot: an AF output's level appears in no register
  we dump, so `FPGA_FIDELITY_DRIVE_PB9` is only ever a guess.
* **any pin at all**, including the 0.5 mm LQFP100 legs with no breakout pads,
  because we drive from inside the chip rather than probing from outside

Settle before the session, not during it:

1. **The watchdog.** With the core dead the IWDG keeps counting and will reset the
   MCU mid-sweep, resetting peripherals with it. Exp E's spin-park image NOPed the
   watchdog write for exactly this reason. Use a no-IWDG build, or stop it from
   the host first.
2. **Contention.** Same rule as the sweep builds: only pins stock itself drives.
   **Never PC9** — power hold.
3. **Anchor.** IDCODE `0x0120681B` before believing any status, as always.

Start from `scripts/swd_fpga_status.sh`; it already has the SPI3 host driver and
the anchor logic.

### 1. ~~Paired PC1 + PC2 — pulsed, then held~~  ✅ DONE, NEGATIVE

The cheapest untested thing, and the one concrete gap the Ghidra-side review
found in Exp Q.

Extend the existing sweep machinery in `firmware/src/drivers/fpga.c`
(`fpga_reconfig_pin_sweep`, button-gated on SAVE in scope mode, `make guest-sweep`):

* **Variant A** — pulse PC1 and PC2 **together** LOW→HIGH before CONFIG_ENABLE,
  with the existing transient-aware sampling (12 reads over ~120 ms, both after
  the pulse alone and after CONFIG_ENABLE).
* **Variant B** — **hold** both at stock's level from before the prelude through
  to after `0x3A`, rather than pulsing. Every experiment so far has tested edges
  or single-instant snapshots.

For B you need to know which arm of the branch at `0x0802C618` stock actually
takes — a runtime question. Either park stock at `0x0802C618` over SWD and read
`r0`, or infer it from the `.data` boot image.

Read the result from the LCD overlay. **Do not attach SWD to read it.**

### 2. Chase the responses  ⏱ zero bench — NOW THE TOP ITEM

maksidze's answer on the 2C23T could end this outright: if rosenrot00's board has
**no resident NV image** (config port open, waiting) while ours auto-boots one,
that is the whole difference and explains why the same byte sequence works there
and not here. Question already posted on #18.

**Second question posted 2026-08-11 — possibly the bigger one.** Exp N's `T0` is
captured *before any config command* and already reads `DONE_FINAL` clear with the
port answering, i.e. our part presents as **unconfigured** — while Exp L showed a
*configured* part stops answering SSPI entirely. **Yet our meter works.** If the
fabric serviced the meter it would have to be configured. So either `DONE`=0 means
something else for an NV-booted part, **or the meter was never the FPGA** — a
separate device on the USART2 line.

The whole "resident NV design is meter-only" model rests on Exp A (ablation kills
the scope, meter survives) — which supports "the meter was never the FPGA" exactly
as well, and the two were never distinguished. Asked maksidze where PA2/PA3
actually land, and whether there is a second IC near the input jacks.

**Keep the counter-argument in view:** Exp A *did* kill stock's scope, so stock's
scope genuinely needs the upload. A theory in which the config port is shut to
everyone still owes an explanation of how stock gets through it.

### 3. MODE0 / JTAGSEL_N with a DMM  ⏱ ~20 min, no soldering

Full procedure in `bench_session_plan_2026-07-30.md` Task 1 — continuity cold
first, then DC volts powered. While the meter is out, also buzz the four JTAG
gold pads against MCU pins: almost certainly not connected, but a *yes* would
mean the MCU could bit-bang Gowin JTAG in firmware and config entry stops
mattering entirely. Highest payoff-to-effort ratio on the board.

### 4. The RDP decision  ⏱ discussion, then a careful session

**⚠ CORRECTED 2026-08-11 — the case for this is much weaker than written below.**
This section claimed clearing RDP would unlock "RTT and the host-driven sweep
loop". **The sweep loop half is wrong: it works today, with RDP set** (§ 0b). The
debug port's memory access is unaffected by read protection; only a *live* CPU is.
So the remaining payoff is RTT — a convenience, not a capability we lack — set
against a mass erase that takes the factory IAP bootloader. **Do not do this
without a new reason.**

Clearing read protection would unlock RTT and the host-driven sweep loop — the
50-pins-in-one-boot workflow instead of 50 reflash cycles. But unprotecting
triggers a **mass erase by design**, which removes the **factory IAP bootloader**
this unit depends on, not just the app. Recovery needs ROM DFU via BOOT0 (case
open), and afterwards the flashing workflow changes because our bootloader uses a
different app slot.

**Before doing this, confirm we can restore the factory bootloader.** Do not
start it late at night.

### 5. If 1-3 come up empty

* **Pre-config delay test** — insert 1 s / 3 s / 5 s before the config sequence
  and re-run the Exp N step trace. Tests open question 8 directly.
* **Logic analyzer** — capture *our* SPI3 at `/64` and diff against maksidze's
  stock capture. Everything state-shaped is excluded; the wire itself is what is
  left. Use the existing back-side SPI3 pads. **Do not re-capture stock** — that
  capture is complete and decoded in `reverse_engineering/captures/`.
* **FT232H JTAG oracle** — only if JTAGSEL_N says the TAP is live. Its rationale
  is much weaker since Exp J, but it would still separate "is our bitstream good"
  from "can we enter config". **SRAM ONLY: `-m`, `--detect`, `--read-register`.
  Never a flag containing `flash`** — the FPGA's NV flash holds the only copy of
  the stock meter design.

---

## Bench procedure (this is easy to get wrong cold)

* **Always `make guest`**, never plain `make`, for this unit — it uses the
  factory bootloader with the app at `0x08007000`. Plain `make` hangs it in
  firmware-update mode.
* **Unplug the ST-Link before any IAP flash.** It browns out the USB-powered
  bootloader and causes bad writes. The flashing loop used all session refuses to
  flash while it sees `0483:3748`.
* Flash with `python3 scripts/iap_flash.py flash <image>` after MENU+Power puts
  the device in upgrade mode (`2e3c:5720`, mounts as `IAP`).
* **The charge-display-mode warning is STOCK-ONLY** (corrected 2026-08-11). Our
  images auto-boot straight into the app after an IAP flash; no POWER press is
  needed to get a real boot. Stock is what lands in charge mode.
* **To actually POWER-CYCLE THE FPGA** — nothing else in the loop does it. The
  pinhole reset resets the **MCU only**; the FPGA stays powered. A POWER-button
  shutdown with USB attached does not remove power either — the countdown releases
  the PC9 hold but VBUS keeps the rail up and the device hangs on "Goodbye". The
  only sequence that works:

      hold POWER -> "Goodbye" -> UNPLUG USB (device goes dark) -> replug

  Much of this project's testing has run on an FPGA that was never power-cycled.
  Exp R did it properly: status still `00039020`, so a fresh POR does not open the
  config port for us.
* Results come off the **LCD debug overlay** (`SCOPE_DEBUG_OVERLAY`, on by
  default in these builds). SWD is only for host-driven bus work.

### Regenerating the stock images (`/tmp` was cleared)

Clean stock is in the repo: `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`
(verified byte-identical to the `clean.bin` used all session).

The Exp E **spin-park** image is that file with **4 bytes** changed:

| file offset | clean | patched | meaning |
|---|---|---|---|
| `0x26A42` | `15 20` | `FE E7` | `movs r0,#0x15` → `b .` — parks one instruction before CONFIG_ENABLE hits SPI3 |
| `0x275CC` | `08 60` | `00 BF` | `str r0,[r1,#0]` → `nop` — stops the watchdog reset during the park |

Confirm the park from **peripherals**, never from `pc`: SPI3 `SPE=1`, `BR=0`,
PB6/CS LOW, PB11 HIGH, AFIO `MAPR=0x02000000`.

---

## Useful commands

```bash
# anchored FPGA status over SWD, on a PARKED image (aborts if IDCODE fails)
./scripts/swd_fpga_status.sh stock

# minimal OpenOCD target — no flash driver, no DBGMCU poke
openocd -f scripts/at32_minimal.cfg -c init -c targets -c shutdown

# whole-image GPIO pulse census of stock (resolves base registers properly)
./scripts/find_gpio_pulses.py "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"

# bench build variants that already exist
cd firmware
make guest            # normal, usable, meter works
make guest-idcode     # Exp J anchored opcode probe (ID@/PO@/CL@ on the overlay)
make guest-trace      # Exp N six-checkpoint anchored status trace
make guest-sweep      # Exp O/Q pin sweep, press SAVE in scope mode
make guest-fidelity   # Exp F stock-fidelity config frame
make guest-pc1        # Exp R fidelity + PC1 LOW (stock's full PC2:PC1 code)
make guest-fidelity2  # Exp R fidelity + PC1 PA6 PC11 PD2 PD3 PD6 PD13
```

---

## Method notes worth keeping

* **Run the control first.** The "~55 s HardFault" was a whole theory built on an
  observation only ever made *through* the debugger. The control — leave it alone
  for five minutes and watch — took five minutes and would have killed it at
  birth.
* **Check what is already known before running an experiment.** Two bench
  sessions went into hunting a RECONFIG_N pulse that a collaborator had measured
  as non-existent two months earlier, in a file in this repo.
* **Do not infer coverage from a changed-file list.** Two of six PR findings were
  fixed in files whose names pointed at a different finding.
