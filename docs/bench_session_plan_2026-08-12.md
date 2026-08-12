# Bench session plan — 2026-08-12: DMM continuity, anchored

**Gear:** DMM with sharp probes, USB microscope. **No soldering, no iron, no
power applied for the continuity work.** Everything here uses hardware already on
hand (`bench-hardware-inventory`).

**Time:** ~90 min for Phases 0–3. Phase 4 is optional and needs the device powered.

---

## Why this session, and why it starts with a pinout check

The plan was going to open with "measure MODE0 and JTAGSEL_N". It shouldn't,
because **two of our own documents give different QN48 pin numbers, and the
disagreement lands on the one pin a load-bearing conclusion depends on.**

| Signal | Table A — `unmapped_mcu_fpga_pin_candidates.md` (cited "UG171E") | Table B — `R3_CAPTURE_ARMING_FROM_APICULA.md` (GW1N-2 chipdb) |
|---|---|---|
| TMS | 8 | **44** (IOT9B) |
| TCK | 9 | **45** (IOT9A) |
| TDI | 10 | **47** (IOT7B) |
| TDO | 11 | **48** (IOT7A) |
| RECONFIG_N | **48** | — (48 is TDO) |
| DONE | — | 37 (IOT18B) |
| READY | — | 38 (IOT18A) |
| MODE0 | 13 | — |
| JTAGSEL_N | 3 | — |
| pins 8–11 | JTAG TAP | ordinary left-edge GPIO (IOL11B/IOL12A/IOL12B/IOL17A) |

**The collision is pin 48.** `bench_session_plan_2026-07-30.md` says:

> *"RECONFIG_N is already answered — stop hunting it. maksidze checked QN48 pin 48
> (RECONFIG_N): always HIGH, no pulse. That confirms Exps G/O/Q were unwinnable by
> construction."*

If Table B is right, **pin 48 is TDO**, maksidze measured a JTAG output idling
high — which is unremarkable — and **RECONFIG_N has never been measured at all.**
The conclusion that closed the entire pulse-hunt would be resting on a
misidentified pin.

Neither table is trustworthy as written. Table A cites UG171E but is marked
"table unconfirmed, verify by continuity before wiring" in the same document.
Table B is explicit that its numbers come from a **proxy package**
(`GW1N-1P5C` / `QFN48XF`) and that "on the scope's package the numbers will
differ" — it only claims the *IOB locations* are fixed, and it specifically says
to re-check maksidze's assumption.

So the first job is not to measure a pin. **It is to earn the right to name one.**

This is the Exp J lesson applied to hardware: every long-lived error in this
project survived because no measurement had a known-correct answer. Phase 0 fixes
that by starting from connections we already know.

---

## Phase 0 — Anchor the package  ⏱ ~30 min  ← do not skip

**Goal:** a verified map from *this* chip's package pins to apicula IOB names,
built from connections whose answer we already know.

The four SSPI signals are the anchor. Their board routing is known (maksidze's
back-side SPI3 breakout pads: SCK / MOSI / MISO / CS) and their IOB identities are
known from the bitstream (apicula): **IOB5A = SCLK, IOB18B = SI, IOB5B = SO,
IOB18A = CS_N**.

1. **Photograph the FPGA under the microscope**, all four sides, with the pin-1
   marker visible. Record package type and pins per side. Save to
   `reverse_engineering/captures/2026-08-12/`.
2. **Buzz each known line to the FPGA package pin** it lands on. `§1` of
   `unmapped_mcu_fpga_pin_candidates.md` gives five **proven** MCU↔FPGA
   connections *with* the proxy-package pin numbers — so we have both the
   identity and the number to check against:

   | Board pad / MCU pin | FPGA IOB | Proxy pin (expected) | Measured |
   |---|---|---|---|
   | SPI3 SCK — PB3 | IOB5A | **16** | ? |
   | SPI3 MISO — PB4 | IOB5B (SO) | **17** | ? |
   | SPI3 CS — PB6 | IOB18A (CS_N) | **23** | ? |
   | SPI3 MOSI — PB5 | IOB18B (SI) | **24** | ? |
   | data-ready — PC0 | IOR13A | **32** | ? |

   PC0 is the fifth anchor and it is on the MCU side, so it can be buzzed from a
   known MCU pin even if it has no breakout pad.

3. **Derive the numbering.** Five known IOB↔pin pairs is more than enough to fix
   the package's origin and direction — and to detect a rotation or offset rather
   than assuming one.

   - **Read 16/17/23/24/32** → the proxy numbering *is* this package. Table B is
     authoritative, **pin 48 = TDO**, and RECONFIG_N has never been measured.
   - **Read a consistent offset/rotation** → apply it and derive every remaining
     signal.
   - **Read no consistent mapping** → the proxy package is genuinely different;
     fall back to identifying signals by trace and rail, not by number.

**Decision gate — record which table survived:**

- **Matches Table A** → Table B's proxy warning was the problem; maksidze measured
  RECONFIG_N correctly and that line stays closed. Proceed to Phase 1.
- **Matches Table B** → **pin 48 is TDO, RECONFIG_N was never measured**, and
  `bench_session_plan_2026-07-30.md`'s "stop hunting it" must be retracted along
  with the "Exps G/O/Q were unwinnable by construction" reading. Phase 2 becomes
  the priority of the session.
- **Matches neither** → the most likely outcome, honestly. Record the empirical
  map as the new ground truth and supersede both tables.

Whatever comes out, it is the first pinout claim in this project backed by a
measurement with a known-correct answer.

---

## Phase 1 — The five gold pads  ⏱ ~20 min

Assumed to be TCK / TDI / TDO / TMS + 3V3. Assumed. Never verified.

1. **Buzz each of the five pads to a package pin.** Using the Phase 0 map, name
   what each one actually is. Confirm or refute the TCK/TDI/TDO/TMS + 3V3 reading,
   and confirm the fifth pad really is **3V3 and not GND** (the July doc flags
   this explicitly — getting it wrong damages the FT232H or the FPGA).
2. **Then the question worth the whole session: do any of the five reach an MCU
   pin?** Buzz each pad against every plausible MCU GPIO. Start with the ones our
   own analysis has flagged as unexplained and never assigned: **PC1, PC2, PC4,
   PB9, PB12, PD2, PD3, PD6, PD12, PD13, PA6, PC11, PC3, PC14, PC15**
   (`HARDWARE_PINOUT.md` lists PC1/PC2/PC3/PC4/PC8/PC11/PC13/PC14/PC15 and PB9 as
   Unknown).

**Why this is the prize:** if the Gowin TAP is wired to MCU GPIOs, we can bit-bang
JTAG **in firmware**. JTAG SRAM load is documented to work *regardless of MODE*
and needs no DONE handshake (apicula, quoting UG290 verbatim) — so config entry
stops mattering entirely, and unlike the FT232H route it stays inside the
**no-case-crack shipping constraint** after this one-time measurement.

A *no* here is also worth having: it makes FT232H-on-the-pads the only JTAG route,
which is a dev unblock and explicitly **not** a shipping answer.

---

## Phase 2 — RECONFIG_N, properly this time  ⏱ ~15 min

Using the Phase 0 map, find the real RECONFIG_N pin and answer, in order:

1. **Where does it go?** Continuity to **MCU PC9** first — that is the open
   question from Exp T and issue #18. Then to every other MCU pin. Then to a
   pull-up/rail, a test point, or nothing.
2. **Is it strapped?** Resistance to 3V3 and to GND. A hard tie versus a pull-up
   changes what is possible.
3. If it reaches no MCU pin and is simply pulled high, **the MCU-driven pulse
   route is dead by construction** — Exps G/O/Q/T were unwinnable, and we should
   say so once, with a measurement behind it, and never re-run that class again.

**This is what decides the Exp T follow-up.** A *yes* on PC9 makes the position
variant (pulse at stock's `master_init` +0x66 rather than pre-prelude) worth a
flash. A *no* closes PC9 and the whole pulse family permanently.

---

## Phase 3 — MODE0 and JTAGSEL_N  ⏱ ~15 min

The original goal of the session; still worth doing, now with correct pin numbers.

- **MODE0** — level (HIGH/LOW), and whether strapped by resistor or driven.
  MODE1/MODE2 are internally tied to GND on this part per UG171E, so MODE0's level
  is the entire config-mode question. It is one of the few remaining explanations
  for a part that decodes SSPI opcodes perfectly (Exp J) yet silently discards
  every SSPI *config* command with no error bits (Exp N).
- **JTAGSEL_N** — level and strap. Decides whether the FT232H route works at all.

---

## Phase 4 — DONE and READY  ⏱ optional, needs power

Table B puts DONE at IOT18B and READY at IOT18A. We currently have **no way to
observe DONE** — Exp L established that a configured part stops answering SSPI, so
the success signature is unreadable over the config port.

1. Continuity from DONE to any MCU pin. (PC8 is the natural suspect — the 2C23T
   HW3 loader reads PC8 back as its ready signal — but our SWD dumps show PC8 as
   input-with-pull-up in *both* firmwares, consistent with the POWER button and
   not obviously a DONE line. Measure, don't assume.)
2. If it reaches nothing, measure DONE's **level** with a probe on a powered
   board: after a clean stock boot with a live trace (configured) versus after our
   firmware boots (not configured). Two states, known-correct answers on both
   sides — a proper anchored measurement, and it would finally give us a direct
   success indicator.

---

## Recording

- Photos → `reverse_engineering/captures/2026-08-12/`
- Results table → new `analysis_v120/qn48_pinout_measured_2026-08-12.md`, written
  as **the** pinout of record, superseding both Table A and Table B
- Update `HARDWARE_PINOUT.md` for any MCU pin that gains a real function
- **If Phase 0 refutes pin 48 = RECONFIG_N**, retract the claim in
  `bench_session_plan_2026-07-30.md` and the "unwinnable by construction" reading
  in the Exp G/O/Q chain in `CLAUDE.md` — in place, not by adding a footnote
- Post the outcome to issue #18 either way; maksidze supplied the pin-48 reading
  in good faith and should get the correction from us, not discover it

## Safety

- **Device off and USB unplugged** for all continuity work. Continuity mode
  injects a test current; do not buzz a powered board.
- 0.5 mm pitch — verify probe placement under the microscope before trusting a
  beep. A slipped probe bridging two pins reads as continuity that isn't there.
- Confirm the fifth gold pad is 3V3 before any future FT232H wiring. Nothing in
  this session applies power to the pads.
- Phase 4 is the only powered step, and it is probe-only — no injection.

## What this session cannot do

It cannot tell us why stock's SSPI config commands take and ours do not. It maps
the board. Every remaining hypothesis — MODE0 gating, a JTAG bit-bang route, a
real RECONFIG_N path, DONE observability — needs these numbers to be right first,
and right now we do not know that any of them are.
