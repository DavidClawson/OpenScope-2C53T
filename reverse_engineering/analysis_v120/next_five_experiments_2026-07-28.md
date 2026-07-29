# The next five experiments — FPGA config-entry wall

**Written 2026-07-28, after Exp O.** Planning document, not a result.

Assumes familiarity with `expE_swd_state_diff_2026-07-28.md` (Exps E–O).

---

## Where the search actually stands

Everything below is *excluded* by a bench measurement, not by argument:

| Excluded | By |
|---|---|
| Payload bytes | `0x4AD19` fix bench-tested; wall unchanged |
| Framing / CS polarity / trailing clocks | stock-faithful sequence; 256 trailing clocks |
| Narrow timing window | Exp B2 — stock delayed 5–10 s, still configured |
| Analog frontend posture | Exp C — range-select NOPed, stock still configured |
| Static MCU state (5 enumerables) | Exp F — fidelity build, wall held |
| Clock tree | Exp E — CRM regs byte-identical |
| FPGA state at the config instant | Exp K — stock reads `0x00039020`, bit-identical to ours |
| The SPI3 bus itself | Exp J — IDCODE/USERCODE/STATUS all answer and discriminate |
| Our sequence ever entering config | Exp M / Exp N — no config command moves STATUS by one bit |
| 20 conservative RECONFIG_N candidates | Exp O — 20/20 negative, anchored |

**The contradiction, precisely:** stock configures the part from a pre-state that is
measurably identical to ours, over a bus that demonstrably works, with no timing
window — and our identical command stream is inert. Exp N established that the SSPI
*read* path works while the SSPI *config-command* path is a no-op, which is exactly
the documented behaviour of a running, auto-booted GW1N.

---

## Two corrections that shape what follows

### 1. Exp K's conclusion is stated too strongly

Exp K concludes: *"the FPGA presents the same state to both firmwares, so FPGA state
is not the differentiator."*

What Exp K measured is **STATUS** — one 32-bit view. "Will the next config command be
honoured" is not necessarily exposed there; indeed `EDIT_MODE` reads 0 in *both* cases,
which is the very thing we are trying to explain. A part that was pulsed on RECONFIG_N
earlier and reloaded from NV flash would present the same STATUS while differing in
config-FSM state.

Supported: **"STATUS is not the differentiator."**
Not supported: "FPGA state is not the differentiator."

This matters because the stronger reading retires the RECONFIG_N hypothesis, and it
should not be retired. Exp O's own two disclosed blind spots mean it has not been
tested to exhaustion.

### 2. Exp B2 does not bound the reconfig→config-enable interval

B2 pushed stock's config-enable 5–10 s later via a code-cave busy loop and stock still
configured. That refutes a *narrow absolute window from power-on*. It does **not**
refute "stock asserts a trigger shortly before config-enable" — if the cave sits ahead
of the whole FPGA-init block, the trigger and the config-enable shift together and
their relative spacing is preserved.

So the trigger, if it is a pin, should be looked for **near config-enable in the
instruction stream**, not merely early in boot.

---

## What this session added

Ghidra was re-imported at the correct base (`0x08007000`) and 302/384 functions named,
which made a precise whole-image GPIO census possible for the first time.

**Finding: PC1 is missing from the Exp O candidate list.**

Stock drives PC1 and PC2 *as a pair*, same instruction pairs, in both arms of a mode
branch inside `master_init`:

```
0802c618  cmp   r0,#0x1
0802c622  bne   0x0802c662
0802c624  movs  r0,#0x4          ; PC2
0802c626  str.w r0,[r10,#0x10]   ; r10 = GPIOC, BSRR  -> SET
0802c62a  movs  r0,#0x2          ; PC1
0802c62c  str.w r0,[r10,#0x10]   ; SET
   ...
0802c654  str   r0,[r6,#0x0]     ; r6 = GPIOC BRR, 0x4 -> CLEAR PC2
0802c65c  str   r0,[r6,#0x0]     ;             0x2 -> CLEAR PC1
```

and again after config at `0802e21c/0802e220`.

* PC1 is listed under **Unknown/Unresolved Pins** in `HARDWARE_PINOUT.md`
* our firmware **never** drives PC1
* Exp O's 20 candidates: PC6, PB11, PC11, PC2, PB12, PB9, PA6, PC12, PE4, PE5, PE6,
  PA15, PA10, PB10, PB0, PC5, PC10, PE2, PE3, PB7 — **PC1 is not among them**
* Exp F's "stock-fidelity" build drove PC2 and PB12 but **not** PC1

PC1 sits *inside* the window (`0x0802AA50 → 0x0802D63C`) that the candidate list was
derived from. It was missed because resolving it requires tracking a `movw`/`movt`
register pair, which the earlier byte-oriented scan does not do.

This is **not** a claim that PC1 does anything. It is a claim that a pin stock drives,
in lockstep with a pin already under suspicion, has never been tested.

**Also worth recording:** between `0x0802D63C` (last GPIO write before the SPI3 prelude)
and `0x0802DA42` (CONFIG_ENABLE) there are **no GPIO writes at all**. And the reset
handler at `0x08007310` is plain ARMCC `__main` startup — `mov sp`, two indirect `blx`,
`bx` — with no GPIO writes of its own. The feared pre-`master_init` pulse hole is
narrower than Exp O assumed, though the indirect `blx` targets are not yet followed.

**Tooling caveat, stated so it is not repeated:** the census script initially read
negative store displacements as unsigned, turning `[r1,#-0x4]` (BSRR) into `+0x4`
(LCKR). Fixed. Port and pin-mask attribution were unaffected; register attribution was
not. Any future scan must be validated against hand-read disassembly before its output
is trusted — this is the third instance in this project of a scan reporting confidently
wrong registers.

---

## The five

Ordered by expected information per unit of bench time.

### 1. Sweep v2 — transient-aware, with the missing candidates

**Hypothesis:** a pin drives RECONFIG_N, and Exp O missed it either because the
detector sampled once at +1 ms or because the pin was not on the list.

**Method.** Extend `make guest-sweep`:
* after each pulse, sample the anchored STATUS **~20 times over ~200 ms** and flag any
  deviation at any point (closes Exp O's disclosed gap (a))
* add the unmapped pins: **PC1** first, then PC3, PC4, PD2, PD3, PD6, PE0, PE1,
  PA0, PA1, PA4, PA5
* add a **paired PC1+PC2** case — stock never drives one without the other, so testing
  them singly may not reproduce the condition
* keep the existing hardware-risk exclusions (PC9, PC8, SPI3, SWD, USART2, EXMC, matrix)

**Positive result proves:** the trigger is a GPIO, and which one. Search over.
**Negative result proves:** no single conservative-list pin, pulsed, produces a STATUS
deviation within 200 ms. It does *not* exclude a pin that must be *held* rather than
pulsed, nor a pin neither firmware drives.

**Cost:** one build change, one bench boot. Do this first.

---

### 2. Hold, don't pulse — reproduce stock's PC1/PC2 level

**Hypothesis:** the relevant difference is a *level held across* the config sequence,
not an edge. Every experiment so far has tested edges (Exp G, Exp O) or static snapshots
at one instant (Exp E, Exp F).

**Method.** Determine which arm of the `0802c618` branch stock actually takes — this is
a runtime question, so either park at `0802c618` over SWD and read `r0`, or take it from
experiment 3 below. Then build a fidelity image that drives **PC1 and PC2 together to
stock's level** from before the SPI3 prelude through to after `0x3A`, and re-run.

**Positive result proves:** Exp F's "all five enumerables closed" conclusion was
incomplete, and static MCU state is back in play as the differentiator.
**Negative result proves:** PC1/PC2 level is not the mechanism — and, usefully, it
repairs the fidelity build so the "static state excluded" claim finally rests on an
image that is actually faithful.

**Cost:** one SWD park + one build + one boot.

---

### 3. Execution-ordered peripheral trace to the CONFIG_ENABLE instant

**Hypothesis:** none — this is instrument-building. It is the measurement the project
has never had.

**Method.** Take the existing Unicorn harness (it produced
`APP_2C53T_V1.2.0_251015_unicorn_trace.csv`, 159 events) and run it to `0x0802DA42`.
Two things make this newly feasible:
* the harness's pcs are in the **legacy `0x08000000` base** — it inherited the same
  off-by-`0x7000` bug the Ghidra project had. Emulating code whose literal-pool pointers
  are all `0x7000` off is a plausible reason it dies at legacy `0x08024412`
* `master_init` is now a named, mostly-disassembled 15 KB function

Then run the same harness against our firmware and **diff the ordered event streams**.

**What it delivers that nothing else does:**
* execution **order**, not address order — my static census cannot tell a pulse from
  two arms of a branch, and mis-reported four "pulse candidates" for exactly that reason
* resolves the `(reg)` values the static census could not — roughly half the GPIO writes
  in `master_init` have their value in a register
* answers which arm of `0802c618` runs, feeding experiment 2 directly

**Cost:** zero bench. Pure host work.

---

### 4. Physically trace RECONFIG_N

**Hypothesis:** the entire pin hunt may be misdirected. Nobody has established that
RECONFIG_N is wired to the MCU at all.

**Method.** Get the GW1N-UV2 package pinout, locate RECONFIG_N on the board, and ohm it
out: to an MCU pin, to a pull-up, to a supervisor, or to nothing.

**Why it ranks this high:** it converts a search over ~50 candidate pins into a single
measurement. And the negative case is *more* valuable than the positive one — if
RECONFIG_N goes to an RC or a supervisor rather than the MCU, then no firmware-side pin
experiment can ever succeed, Exps G/O were unwinnable by construction, and the whole
approach needs to change.

**Positive result proves:** exactly which pin to drive.
**Negative result proves:** the mechanism is not MCU-driven RECONFIG_N, which redirects
everything.

**Cost:** case open, fine-pitch probing, multimeter. Moderate, but bounded and one-off.

---

### 5. FT232H JTAG oracle

**Hypothesis:** decouple "is our bitstream correct" from "can we enter config over
SSPI" — two questions currently entangled in every experiment.

**Method.** Load the extracted bitstream into FPGA SRAM over the JTAG TAP pads, a port
stock never touches.

**If the scope comes alive:** the bitstream, its extraction, the framing, and everything
downstream are proven correct, and the problem collapses to a single well-posed
question — SSPI config entry. It also gives a *working scope on our firmware*, which has
independent value and would let the rest of the project proceed in parallel.
**If it does not:** our understanding of the bitstream or of what the design needs is
wrong, and the SSPI wall was never the only problem.

**Cost:** highest of the five — hardware, wiring, toolchain. But it is the only item
that can produce a working instrument regardless of which way it resolves.

---

## Blind spots that remain after all five

Stated explicitly so they are not silently assumed closed:

1. **Peripheral-driven pins.** A pin in alternate-function mode is driven by a timer or
   other peripheral and produces *no* GPIO store — invisible to every scan run to date,
   including this session's. Exp E noted PB9 as `AF-PP` in stock and floating in ours.
   The AF-decode arm of the census returned nothing usable because nearly all `CRL`/`CRH`
   writes are read-modify-write with the value in a register.
2. **DMA-driven GPIO.** A DMA channel writing `BSRR` produces pulses with no instruction
   anywhere. Only DMA1 Ch1 (LCD) is currently accounted for.
3. **The indirect `blx` targets in the reset stub** are not yet followed.
4. **~18% of the code region is still not disassembled**, and 82 functions remain
   unnamed — including `FUN_080165a8` at 25,548 bytes with zero direct callers, which is
   larger than `master_init` and has never been examined.
5. **Whether STATUS is even the right detector.** Exp L found that a *fully configured*
   part stops answering SSPI entirely. A part that is *in config mode* must still answer.
   These are different states and the sweep detector should distinguish all three, not
   two.

---

## Recommended order

**3 → 1 → 2 → 4 → 5.**

Experiment 3 costs no bench time and feeds 1 and 2 with the branch answer and the
unresolved register values. Run it first even though it proves nothing by itself.
