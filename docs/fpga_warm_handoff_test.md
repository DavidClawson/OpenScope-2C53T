# Warm-Handoff FPGA Read Test (no extra hardware required)

**Goal:** run **our firmware's full scope pipeline** (SPI3 reads → buffers →
scope UI) against a scope-configured FPGA, by letting **stock** configure the
FPGA at boot and then handing off to our firmware **without cutting power** so
the FPGA keeps its SRAM config. This sidesteps the config-entry wall entirely
and gives a ~2-min/iteration dev loop for real-waveform work.

> **REWORKED 2026-08-12.** The original June procedure read via the USB-CDC
> shell (dead on bench unit #1) and got exactly one buffer before capture went
> idle — see the RESULTS sections at the bottom. **Stlkv's port (issue #18,
> 2026-08-12) found the missing link: the MCU reset zeroes DAC1 (PA4, the
> trigger comparator's reference, `DHR12R1 @ 0x40007408`), so the FPGA never
> triggers a new capture and every read is flat zeros.** Restoring DAC1 to
> mid-scale gave live, continuously-updating waveforms with PC0 pulsing. The
> reworked `guest-warmtest` build arms DAC1 at init and replaces the shell
> readout with the LCD: a PC0-gated acquisition task speaks the real
> `0x04`/`0x05` protocol and feeds the scope UI directly.

## Why this works

- Stock firmware successfully uploads the scope bitstream to the GW1N at boot
  (the `0x3B` SSPI sequence we can't make take). After that the FPGA is running
  the **scope** design.
- The GW1N holds its SRAM config **as long as VCC is maintained**. An MCU
  *soft-reset* (reflash + reboot) does **not** power-cycle the FPGA, and
  RECONFIG_N stays HIGH (maksidze confirmed). So the scope config survives a
  firmware swap. (Corollary from Exp R: only unplugging USB after a POWER
  shutdown actually drops the FPGA rail.)
- Our `FPGA_WARM_HANDOFF_TEST` build comes up **read-only on the wire**:
  - SPI3 + PC6 (enable) + PB11 (active) match stock's scope-run posture;
    PB4/MISO gets stock's pull-up; PC0 (data-ready, active LOW) is configured
    input pull-up so an undriven line can't fake "ready".
  - **DAC1 armed to mid-scale (2048)** via the `scope_trigger` driver — the
    Stlkv fix. MCU-internal only; cannot touch the FPGA.
  - The SSPI config sequence, all USART2 traffic (UEN is never set —
    `FPGA_USART_SILENT_SCOPE` is paired by the Makefile target), the frontend
    relays, and every mode-entry/heartbeat transmit path are compiled out.
    (Before 2026-08-12 this build silently sent ~20 polled USART2 frames from
    `main()`'s pre-scheduler `fpga_enter_scope_mode()` call — fixed.)
  - One task runs: `fpga_warmtest_acq_task` — poll PC0 (active LOW, ~1 kHz,
    500 ms bound), then read CH1 (`0x04`) and CH2 (`0x05`) as single
    1026-byte CS-LOW windows (opcode + 2 status bytes + 1023 samples), apply
    the −28 ADC offset, fill `fpga.ch1_buf`/`ch2_buf`, set `data_ready`.

**⚠ Config-port quarantine.** Never add `0x11`/`0x41` (or any Gowin config
opcode) reads to this build, including the otherwise-mandatory IDCODE anchor:
a configured part's config port is closed (Exp L/M), reads return zeros, and
**touching the port desynchronises acquisition** — it would destroy the state
under test. The anchored-read rule applies to config-wall experiments, not
here. Related hazard: a *short* CS frame containing `0x05` is byte-identical
to the config prelude's ERASE_SRAM step; only the 1026-byte frame shape makes
it a CH2 read. Don't shorten the windows.

## What you need

- **Bench unit #1** (factory FNIRSI IAP bootloader at `0x08000000`, app slot
  `0x08007000`). The handoff relies on the factory IAP drive for a
  no-power-loss reflash. *(Historical note: the June runs called this "unit
  #2"; the standard bench loop since 2026-07-27 is `make guest` + factory IAP
  on bench unit #1. Do not run this on a unit with our HID bootloader — that
  flash path is not the no-power-loss one.)*
- The stock app binary: `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`.
- A signal source on the CH1 probe (device siggen output from *stock*, a
  probe-comp square wave, or any known signal). Feed it **before** the
  handoff so you can confirm stock's live trace first.
- USB attached and the battery charged. **Power must never drop between the
  stock boot and the end of the test** — a dropout reverts the FPGA to its NV
  design and all reads go dead (that reversion is also the negative control,
  step 6).

No USB shell, no logic analyzer, no case opening. The LCD is the readout.

## Procedure

**1. Flash stock (it will configure the FPGA at its next real boot).**
```
! python3 scripts/iap_flash.py flash "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
```
(MENU+Power → upgrade mode, then detect → pick → flash. Needs sudo for the raw
disk write — run it yourself.)

**2. Boot stock for real, confirm a live trace.**
⚠ **Stock reboots into charge-display mode after an IAP flash** (Exp R:
stock-only behavior) — that mode **skips FPGA init**. Press POWER for a real
boot. Let it boot fully (~5 s; the bitstream upload happens ~3.6 s in), enter
**scope mode**, and confirm a live trace on CH1 with your signal. That proves
the FPGA is running the scope design. **From here on: no unplugging, no
power-cycling, no POWER-button shutdown.**

**3. Build the warm-handoff firmware (on the host, any time).**
```
cd firmware && make guest-warmtest
```

**4. Hand off WITHOUT cutting power.**
With the device still on, enter upgrade mode (**MENU+Power** → the `IAP` drive
mounts — a *soft* reset; the FPGA stays powered), then:
```
! python3 scripts/iap_flash.py flash firmware/build/firmware.bin
```
Our builds **auto-boot straight into the app** after an IAP flash (Exp R) —
no POWER press, which is exactly the clean straight-through boot that
preserved the config in June's Round 3.

**5. Read the LCD.** The build boots directly into scope mode. Watch for:

| LCD observation | Meaning |
|---|---|
| **Synthetic demo trace disappears and a real waveform draws** | ✅ **WIN.** First real frame latched (`scope_ui` kills the demo permanently on genuine `data_ready` — and "genuine" is enforced: the acq task only accepts a frame carrying the `0x80` marker or non-constant CH1 data, so a dead bus reading flat `0xFF` cannot latch it). Our entire downstream path — PC0 gating, 0x04/0x05 reads, offset, buffers, rendering — works on our own firmware. |
| Waveform **updates continuously** (and overlay `OK:` counts up) | ✅✅ Full Stlkv-equivalent result: DAC1 restore re-armed free-running capture. |
| Demo trace disappears, **one frame, then frozen** (`OK:` stops) | Partial — June's one-buffer symptom persisting despite DAC1. Overlay `P0:` tells you whether PC0 ever pulses again. |
| Demo trace **never disappears**, overlay `TO:` climbing, `P0:H` constant | PC0 never arms — no capture behind the port. Config may not have survived, or capture needs a nudge we removed (see fallback knob below). |

Debug overlay (bottom strip, on in every build) fields that matter here:
`OK:` = accepted CH1+CH2 read pairs; `TO:` = 500 ms PC0 timeouts **plus
rejected frames** (frames failing the marker/non-constant validity gate); `1:`
= MISO byte clocked during the CH1 opcode (Stlkv observed `0x80` in the first
window after data-ready); `P0:` = live PC0 level (L = ready/active); `P6:`/`B:`
= PC6/PB11 (should be H/H); `L`/`H` = min/max over the CH1 buffer (span > 0 =
non-flat data). The config-sequence fields (`SS:`/`CL:`/`CFG:`/`ED:`)
legitimately read zero in this build and `H2:` reads `N` — the config sequence
never runs. Ignore the CFG line's `A0` subfield too: it is the zero-initialized
anchor slot, not a validated "config port open" reading (the overlay legend's
`A>=0` meaning does not apply here — no anchor read is ever taken).

Two render caveats while judging the table: (1) occasional single-frame
horizontal tearing in a live trace is a known unsynchronized-buffer race
between the acquisition and display tasks (inherited structure, cosmetic) —
not a capture glitch; (2) **do not press OK in scope mode** — it toggles
RUN/STOP, and STOP freezes the entire redraw including the overlay counters,
which looks exactly like the "one frame, then frozen" failure row. If you did,
press OK again.

**6. Negative control (run once, at the end).**
Hold POWER → "Goodbye" → **unplug USB** (device goes dark — this is the only
sequence that actually drops the FPGA rail) → replug → boot the same
`guest-warmtest` image cold. Expected: demo trace stays, `TO:` climbs, no
real frames — because the FPGA reverted to its NV design and there is no scope
capture to read. That proves the step-5 result came from the handoff, not
from the build somehow working against an unconfigured part.
(If the demo trace *does* disappear here, check `P0:` — an `L` reading means
something on the cold FPGA actively drives data-ready and the control is
inconclusive; the pull-up only defends against an undriven line. The `L`/`H`
span disambiguates: flat garbage vs real variation.)

## While the rig is warm (optional extras)

- **Record CH2.** Stlkv's open question (#18): in the inherited state both
  windows carried CH1 on his unit, CH2 never appeared. Note what our CH2
  trace shows with a signal on CH2 — any difference is a data point for him.
- **Siggen is safe to cycle past but useless**: `siggen_enable` is fully inert
  in this build (guarded — its disable path would otherwise Hi-Z PA4 and kill
  the trigger reference permanently). The acq task also re-arms DAC1 to
  mid-scale on every PC0 timeout as defense in depth.
- **Settings → "FPGA SPI Scanner" is guarded** (`fpga_scanner_run` no-ops in
  this build — it would otherwise sweep every quarantined config opcode and
  pulse PC6/PB11 LOW, ending the experiment in one press). **Settings →
  "Firmware Update" is NOT guarded** — it soft-resets the MCU mid-run; that's
  also how you *end* the session (it re-enters the flash path), just don't
  press it by accident.

## Fallback knob (only if PC0 never arms)

Stlkv's port also sent stock's five post-config SPI3 writes (`01 08 / 02 03 /
06 00 / 07 00 / 08 AD`; register `0x02` is an acquisition-mode selector where
`03` = run) and reported SPI3 writes safe against the live design. (Per the
ripcord session's stock decode, the `08 AD` payload is the *digital trigger
level* in offset-binary — `0xAD = 0x2D ^ 0x80` — a second trigger quantity
alongside the DAC1 analog reference; if DAC1 alone doesn't re-arm capture,
this byte is the other half of the trigger state.) The reworked build
deliberately sends nothing. If step 5 shows PC0 stuck HIGH,
the next build variant is those five writes (each in its own CS frame,
scope-engine opcodes only — extract from `fpga_spi3_config_sequence`, do NOT
call it, it would send the forbidden prelude). Not implemented until the
minimal build's bench result says it's needed.

## Notes / caveats

- Revert to a normal build any time with `make guest` (the flag defaults to 0;
  normal builds are unchanged).
- Nothing here is destructive — power-cycle and the FPGA reloads its NV design
  as always; reflash anything via the factory IAP.
- MENU still cycles UI modes, but every FPGA-side mode transition
  (`fpga_set_meter_mode`, `fpga_enter_siggen_mode`, `fpga_scope_reinit`,
  heartbeat) is compiled out. ⚠ The meter screen falls back to its built-in
  **DEMO readings** (canned plausible values — e.g. `13.82` V DCV, `4.700`
  kOhm, from `meter_modes[]` in `meter_ui.c`). These are FAKE; do not read
  them as evidence the meter path survived the handoff.
- Build serially (`make guest-warmtest`, no `-j`): every guest target's
  `clean all` prerequisite pair races under parallel make.
- This build must **never** be flashed as a daily driver: its meter is dead by
  design and its scope only works after a warm handoff.

---

## RESULTS (2026-06-13, Unit 2) — ✅ read path validated on real waveform data

*(Historical — pre-DAC1 rework. "Unit 2" here is the factory-IAP bench unit,
called bench unit #1 since July.)*

Three rounds. Headline: **our `0x04`/`0x05` read path reads genuine scope
samples from a stock-configured FPGA.** First time this firmware has ever pulled
real FPGA scope data.

**Round 1** (warm-test build, handoff *with* a charging-logo POWER press): first
`acqread` returned real, channel-distinct data — CH1 ≈173 / CH2 ≈82, a few LSB
noise (span 7/3). Every subsequent read all-zero. One buffer, then idle.

**Round 2** (drove PC6/PB11 HIGH + PC11 LOW at the very top of `main()` to
shrink the float window from ~2 s to the reset glitch; also POWER-press handoff):
*identical* — one real buffer (CH1 ≈173 span 4, CH2 ≈83 span 7), then zeros.
**The fast pin-restore changed nothing → the one-buffer limit is NOT the float
window.**

**Round 3 — the decisive one** (3 V p-p **sine** fed into CH1 on stock; handoff
booted **straight through with no POWER press = clean soft reset**): first
`acqread` CH1 returned a **smooth rising ramp** —
`A7 A7 AA AD B0 B1 B1 B3 B5 B8 BA BC BE BF C1 C4` (167→196), `span=81`,
min=133/max=214 — **the rising edge of the sine.** CH2 flat (span 3, nothing
connected). Unmistakably the captured waveform, read through our path.

### What this establishes

1. **The scope config survives a *clean* soft handoff** (no power-down). The
   SRAM bitstream is intact — we read its captured buffer. (The earlier
   POWER-press rounds were the disturbed ones; round 3's straight-through boot
   was clean.)
2. **Our `0x04`/`0x05` read framing is correct** — validated against a known
   waveform, not just noise.
3. The persistent **one-buffer limit is a run-state problem, not a read or
   config-survival problem.** The "keep capturing" state doesn't survive the MCU
   reset (and stock's siggen also stops at handoff, so there's no fresh signal
   to capture afterward). Re-arm attempts (stock's post-config SPI3 writes, the
   scope USART config, PB11/PC11 toggles) all failed to restart continuous
   capture.

> **⬤ 2026-08-12 REINTERPRETATION.** Point 3's "run-state problem" now has a
> concrete mechanism: **the MCU reset zeroed DAC1, the trigger comparator's
> reference (Stlkv, issue #18).** With the reference at 0 V the comparator
> never fires, so no new capture is ever triggered — which is why *no*
> pin/register re-arm could work while the one thing nobody re-armed was the
> DAC. The June "one buffer" was the capture stock had already taken before
> the handoff. This also retires the JTAG framing below: the warm handoff CAN
> yield continuous capture once DAC1 is restored.

### Implication for JTAG

*(Historical framing — superseded by the 2026-08-12 reinterpretation above,
and independently weakened by Exp J's finding that SSPI reaches the config
engine fine.)*

JTAG provides exactly what the warm-handoff can't: a **fresh** config
(run-state established) + free-running capture + **no MCU reset afterward**. With
the read path now validated, JTAG should yield **continuous** live traces. The
`fpga_acquisition_task` rewrite to `0x04`/`0x05` is the firmware follow-up,
validated and ready for that bench session.

---

## RESULTS (2026-06-13, Round 4) — `spi3 armtest`: MCU-pin re-arm falsified

*(Historical — the negative result stands, but its interpretation is updated
by the 2026-08-12 note above: the re-arm failures are all explained by the
zeroed trigger DAC, which none of these probes touched.)*

Tested the netlist-derived runtime-arm hypothesis from
`reverse_engineering/analysis_v120/mcu_fpga_boundary_reconcile_2026-06-13.md` §5:
the apicula trace says MISO (`SO.OEN`) is gated by a read-window counter that only
advances while the FPGA run/re-arm pad (`IOR1B`) is driven, and that re-arm runs
through an async-preset (`8× DFF.SET`) *pulse* path — so a held-HIGH level may
never restart capture after the first window. `IOR1B` was mapped (semantically,
not by board trace) to PB11 (#1) or PC6 (#2). New shell command `spi3 armtest
[pb11|pc6]` (in `usb_debug.c`): baseline acqread → pulse the run pin
HIGH→LOW→HIGH ×3 → re-issue the post-config control register
(`01 08 / 02 03 / 06 00 / 07 00 / 08 AD`) → acqread again.

Procedure was a **clean** warm handoff (stock → live scope trace → MENU+Power →
flash `guest-warmtest`, booted straight through, no POWER press).

- **Handoff confirmed good:** the PB11-run baseline read returned the stale
  one-shot buffer — CH1 `AE`(174) / CH2 `53`(83), matching Round 1's DC levels.
  Read path + `0x04`/`0x05` framing re-validated.
- **PB11 pulse + control-reg → no re-arm.** After-pulse read empty (span 0) on
  both channels; the single buffer was consumed and none regenerated.
- **PC6 pulse + control-reg → no re-arm.** After-pulse read empty on both
  channels (independent of the already-consumed baseline buffer).
- The read *window* still fires throughout (CH1 status `80 00 00`, PC0 1→0) —
  the readback path is alive; there is simply no fresh capture behind it.

**Conclusion:** continuous capture is **not re-armable from either MCU-reachable
dedicated control pin** (PB11, PC6) via an edge pulse + control-register
re-issue. Combined with the earlier PB11/PC11-toggle failures, MCU-side re-arm
after a soft reset does not restart capture. Per the boundary doc's decision
tree this points to `IOR1B` being an **unbonded top-edge IOT pad** (needs a board
trace) and/or the run-state being fundamentally non-re-establishable from the MCU
post-reset — i.e. **JTAG remains the unlock** (fresh config + no MCU reset).
`spi3 armtest` stays in the tree as a ready probe for the JTAG session (where a
fresh config means the baseline should free-run, not one-shot).

> Note (`spi3 gowin` in this state): once the scope **user design** is running it
> owns the SSPI port, so config-register reads (`0x11`/`0x41`) no longer reach the
> config engine — they returned all-`00` here, *unlike* the all-`FF`/valid-IDCODE
> seen when only the NV meter design is loaded. All-`00` on gowin is therefore a
> rough *positive* indicator that a user design has the port, not a failure.

---

## RESULTS (2026-08-12, bench unit #1) — ✅✅ LIVE SCOPE UNDER OPEN FIRMWARE

Six build iterations in one evening (~2 min/flash via factory IAP), ending in
a **live, continuously-updating, DC-responding trace rendered by our scope UI
from real FPGA capture data** — the first working oscilloscope under
OpenScope firmware. Negative control passed. Run log:

| Run | Build | Handoff | Result |
|---|---|---|---|
| 1 | v3 base (DAC1 arm, PC0 hard gate, no writes) | MENU+**Power** | ✗ `OK:0 TO:↑ P0:H` — demo trace stays |
| 2 | + five post-config writes + 0x03 read | MENU+**Power** | ✗ identical; `SS:` all **zeros** (actively driven — MISO is pulled up) |
| 3 | v3': writes removed, PC0 = hint + ~2 Hz probe reads | MENU+**pinhole** | ✅ **live frames** — `OK:` climbing 1-2/s, demo trace gone, noise trace responds to finger |
| 4 | v4: + frontend relay bank re-armed as outputs | MENU+pinhole | Path connected but **AC-coupled**: finger span ~22, battery = no DC response |
| 5 | v5: + SAVE toggles PC12 live | MENU+pinhole | **PC12 HIGH ⇒ DC passes** — battery steps the trace. Coupling relay found |
| 6 | v6: PC12 HIGH at boot | MENU+pinhole | ✅ DC response at boot, no button needed |
| NC | v6, after true power cycle (POWER→Goodbye→unplug→replug) | — | ✅ **demo trace stays, `OK:0`** — the handoff is the enabler; the build cannot fake success |

### What was established

1. **THE HANDOFF METHOD DECIDES THE ENGINE STATE.** MENU+Power lets stock's
   upgrade-entry code run before the reset — it stops the capture engine, and
   nothing MCU-side restarts it (runs 1-2; also explains June's one-buffer
   results and their failed re-arms). **MENU + pinhole reset (NRST) kills the
   MCU instantly and the engine keeps free-running** — Stlkv's recipe, now
   confirmed as the load-bearing detail on unit #1.
2. **DAC1 restore is necessary but the engine state is separate.** With the
   engine free-running + DAC1 at mid-scale (stock-faithful 0x3D bring-up:
   buffered, enable-last — the old D1BOFF here was a real bug, caught by the
   ripcord session's decode), capture just works.
3. **The five post-config writes are NOT needed** for inherited capture, and
   the 0x03 status read returned all zeros in the stopped-engine state
   (actively driven low — not floating). Left compiled out
   (`FPGA_WARMTEST_SEND_CFG_WRITES`).
4. **PC0 is not a spontaneous strobe when the engine is stopped** — a hard
   data-ready gate deadlocks (no read → no PC0 → no read). As a hint with
   ~2 Hz unconditional probe reads + the frame-validity gate, both engine
   states are handled.
5. **PC12 is the input-routing/coupling relay: HIGH passes DC, LOW is
   AC-coupled.** Bench-measured by live A/B on the SAVE toggle. The
   approximate relay truth table in `fpga_set_scope_frontend_range()` drives
   it LOW in every arm — wrong for DC scope work; overridden HIGH in the
   warmtest build pending a per-range re-derivation.
6. **The relay bank must be re-armed after any MCU reset** — the coils drop
   and the input path disconnects (run 3 vs 4). Warmtest configures the bank
   as outputs itself since it returns before fpga_init Step 9.

### Known cosmetics / next tuning (not failures)

- Full-screen repaint on every accepted frame → visible 2-3 Hz flash. The
  known "rendering pass" TODO (per-component dirty tracking), now with real
  data to justify it.
- ~½-1 s display latency ("buffered" feel) from the 2 Hz probe cadence.
  Tighten the probe interval / trust PC0 more when free-running.
- Baseline sits at ~55, not ~100/mid — missing per-range offset cal (known;
  the cal tables are placeholder full-scale linear).
- CH2 not yet examined. Ripcord contract 38/39 predicts CH2's trigger
  reference is **TMR13 CH1 PWM (C1DT @ 0x40001C34)** — never programmed by
  our firmware — so expect CH2 dead until a TMR13 bring-up (the "v7").
