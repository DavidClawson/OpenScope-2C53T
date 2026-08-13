# Bench plan — next session (after the 2026-08-13 breakthrough)

Context: 2026-08-13 broke the config-entry wall AND reached **cold-boot-to-live-scope**
on our own firmware (`guest-coldtrace`; devlog `2026-08-13-cold-boot-to-scope.md`;
memory `config-entry-solved-buildB`). Config entry = bit-bang loader (Build B) not
hardware-SPI (Build A). Engine arm = the five writes with PB11(IOR1B)+PC6(IOB7B)
held. Live readout = the warmtest 0x04/0x05 pipeline. The scope now CAPTURES from
cold but has no timebase, so it only tracks slow signals; audio-band waveforms alias.

Bench aids ready: ESP32 signal generator on the LOLIN D32 PRO (`esp32_siggen/`,
gitignored) — serial-controllable (`sine/square/tri/saw/dc/amp` @ 115200 on
/dev/ttyUSB0), DAC1(GPIO25)→CH1, GND→ground. The agent can drive it live.

## 1. Timebase control — THE feature that makes it a usable scope ⭐
The blocker to displaying real waveforms: `guest-coldtrace` never sets the FPGA
sample rate, so each 1024-sample sweep is ~µs long, refreshed ~34 Hz. Slow signals
track as a moving level; >~15 Hz aliases.

**What already exists (don't rebuild it):**
- `timebase_table[]` (`scope_state.c`) — 21 timebases, 5 ns → 20 ms, with labels.
- `scope_adjust_timebase()` + button handling — the UI selector works.
- `fpga_scope_select_timing()` (`fpga.c:1142`) — maps a timebase to
  `{run_mode, sample_depth, tb_prescaler, tb_period, tb_mode, acq_mode}` and
  `fpga_send_scope_sequence()` sends the `0x0F/0x10/0x11` commands.

**What's wrong / missing (the actual work):**
- The mapping is only **3 coarse buckets** (idx ≤3, ≤9, >9) with **guessed**
  prescaler/period values (0x20/0x80, 0x08/0x40, 0x04/0x20). 21 UI timebases → 3
  real configs. Not per-timebase, not stock's values.
- None of it is wired into the cold-boot (`guest-coldtrace`) path, and **it has
  never been validated on hardware** — the scope was dead until 2026-08-13.

**Step 0 — PROVE THE MECHANISM (the load-bearing unknown, do this first).**
We do not actually know that `0x0F/0x10/0x11` change the effective sample rate.

**⚠ NETLIST ANSWER, 2026-08-13 (`gw1n2-apicula/tools/m_timebase.py`, progress log
M8) — THERE IS NO RATE CONTROL IN THE CAPTURE PATH, so the expected bench result
is "NO CHANGE".** Structurally, on all four BSRAMs: `WREA` tied VCC; **every**
address-counter clock-enable tied VCC (CH1 8/8, CH2 8/8, BSRAM_1/2 10/10); `CEA`
is a bare arm gate (CH1 `R13C2_LUT4_3` INIT `0x3300` = run & ~done; CH2
`R11C17_LUT4_1` INIT `0x0f00`, same shape). A sample pair is written on *every*
capture clock and the address advances *every* clock ⇒ no counter, no comparator,
no divisor register exists to program. And the ADC clock output is a **plain
forward** (the design's one ODDR has constant data pins `D0=VCC/D1=VSS`), so
there is no fabric clock divider either. Remaining blind spot: the rPLL is not
decoded by the chipdb — no fabric net drives PLL inputs, but that evidence is
weaker than the two above.

**⚠ THAT NETLIST ANSWER IS NOW PARTLY WITHDRAWN, 2026-08-13 (`m_clocktree.py`,
progress log M10). The three M8 checks are all still true, but every one of them
sits on the DATA/ENABLE side — none looks at the CLOCK. A gated or divided clock
changes the sample rate with `WREA` and every `CE` still tied to VCC, so M8's
evidence was structurally blind to it** (same failure mode as the `/2` reads and
the floating MISO). Walking the tree from the top: this design has **six global
clock sources, and THREE ARE GENERATED IN THE FABRIC** — `LUT4 R4C13_LUT4_7`
(INIT `0xffc0` = `I3 | (I1 & I2)`), `LUT4 R4C13_LUT4_1` (INIT `0xfccc` =
`I1 | (I2 & I3)`), and `DFFRE R16C17_DFFE_0`; the other three are SPI SCLK
(pad `R19C5_IA`, on a spine) and two PLL outputs. The two LUTs are a clock-gate
pair — `passthrough | (gate & enable)` — fed by flip-flop state plus `GB70`, the
biggest clock net in the design. That is what a programmable divider looks like
built from fabric. **Unresolved:** whether either fabric clock reaches the
capture BSRAMs, which needs the spine → `GB{n}0` mapping (apicula has no GW1N-2
`tap_start`; do NOT assume index mod 8). So the "NO CHANGE" prediction below is
now a genuinely open coin-flip, not a near-certainty — which makes Step 0 more
worth running, not less.

So **run Step 0 as a falsification test, not an open question**, and keep it
cheap: with the ESP32 driving a known 1 kHz square from the coldtrace path, send
2–3 *wildly* different `tb_prescaler`/`tb_period` values.
  - **No change (PREDICTED)** → the timebase is MCU-side. Go straight to the real
    candidates: read pacing (ripcord: stock's per-timebase pacer is TMR3, a
    9-entry 1-2-5 PR table) and roll mode, i.e. rewrite Steps 1–3 around *when we
    re-arm and read*, not around FPGA rate registers. Also suspect the
    **BSRAM_1/2 pair** — 10-bit address counter on a *different* clock spine
    (GB40 vs the scope buffers' GB20), sharing one enable flop; a slower,
    deeper, separately-clocked buffer is exactly a roll-path shape, and it has
    never been identified (old notes guessed "DMM?").
  - Stretches → the netlist reading is wrong somewhere; stop and reconcile before
    building on either model.

**Step 0b — SPI READ-OPCODE SWEEP (new, 2026-08-13; cheap and possibly decisive).**
Follow-up netlist work (progress log M9) identified what BSRAM_1/2 are, and they
may hold the slow timebase. Unlike the scope buffers (BSRAM_0=CH1, BSRAM_3=CH2,
fed *exclusively* through the IDDR/DDR path, one channel each, 8-bit address),
**BSRAM_1+2 are fed from the same ADC pins through a separate non-DDR (SDR) path,
see BOTH channels, sit behind deep combinational logic (so they store something
computed, not raw samples), share one enable flop and one 10-bit address counter
(= a single 1024-word record two blocks wide), on a different clock spine (GB40
vs the scope buffers' GB20), with verified read-modify-write feedback (BSRAM_1's
DO1 routes back into its own DIA8/DIA9 = accumulate-in-place).** That is the
shape of a decimated / min-max peak-detect **roll buffer** — i.e. plausibly the
slow-timebase path, in a buffer we have never read. All four blocks feed the same
readout mux (→ SPI SO + DRDY), so the MCU *can* read it, under some opcode other
than `0x04`/`0x05`.

Test: with `guest-coldtrace` running, sweep SPI3 read opcodes beyond the known
`0x03/0x04/0x05` and log reply length + content; look for one returning ≥1024
words whose content tracks a slow input from the ESP32. Minutes on the existing
rig, and a hit would hand us the slow timebase directly.

**"Not simulable" was true for the wrong reason — and may be fixable
(2026-08-13, M10).** The harness caveat is that `m_simarm` force-drives *every*
GB tap from one common `clk`; SPI SCLK turns out to enter the design as a
**dedicated clock input on its own spine** (pad `R19C5_IA` → BLBDCLK3 →
SPINE16/24), so that force is exactly what overwrites the SPI domain. Driving
the SCLK spine from the SCLK pad in the testbench should restore SPI address
decode in sim — which would move this sweep (and Stlkv's arm-address sweep) off
the bench entirely. Gated on resolving spine → `GB{n}0`; see M10.

**Step 1 — wire the config into the cold-boot path.** Integrate the timebase block
(`fpga_send_scope_sequence`) after config+arm in the `guest-coldtrace` path, coexisting
with the 0x04/0x05 readout. (Mind the trigger-byte space — see the PR #13 review note
about `acq_mode` values colliding with roll/fast-TB triggers.)

**Step 2 — real per-timebase values.** Replace the 3-bucket placeholders. Sources:
stock's `0x26/0x27/0x28` timebase block (`fpga.c:576`), ripcord's TMR3 table, and the
June Saleae capture. Resolve the **TMR3 conflict** (stock's acquisition pacer vs our
500 Hz button scan) if Step 0 shows MCU pacing matters. Handle **roll mode** for the
slow end (stock scrolls slow timebases; `FPGA_ROLL_BUF_SIZE=300`).

**Step 3 — wire the buttons.** `scope_adjust_timebase` → re-send config live; watch the
ESP32 wave stretch/compress as you step the timebase.

**Step 4 — calibrate (horizontal).** With the ESP32 at known frequencies, verify the
on-screen period matches the timebase label. First real horizontal calibration.

Deliverable: `guest-coldtrace`-derived build showing a **stable multi-cycle sine** whose
period reads correctly across timebase steps. Bench aid: the ESP32 siggen (agent can
drive it live).

## 2. Fold cold-boot-to-scope into the DEFAULT boot path (shippability)
`guest-coldtrace` is a special build. Make cold config + arm + readout the default
so a normal `make guest` boots to a live scope — the shippable path (no case-crack).
- Reconcile with the meter path (config runs, THEN meter USART init) and the mode
  boot default (currently MODE_MULTIMETER).
- Keep the warm-handoff build around for A/B.
Deliverable: `make guest` cold-boots to scope; update CLAUDE.md.

## 3. Config-entry bisect — which single variable broke the wall
Build B differs from Build A in TWO ways (bit-bang-vs-AF AND no-0x05 ERASE). Isolate:
- Build B + re-add `0x05` ERASE → still works? (isolates the 0x05 variable)
- Build A (hardware-SPI) + inter-byte gaps / GPIO-mode PB3/4/5 → does AF ever work?
Why it matters: hardware-SPI (if it can be made to work) is ~faster than bit-bang
and cleaner. If bit-bang is truly required, document why (GW1N config-engine timing).

## 4. CH2 cleanup + trigger source
David observed CH1/CH2 "triggering each other" and CH2 coupling CH1's signal.
- Bring up the CH2 trigger reference: `guest-warmtest-ch2` (TMR13 CH1 PWM on PA6 —
  decoded, UNCONFIRMED; watch for CH2 responding, and for PA6/frontend conflict).
- Per-channel trigger source select (stop the cross-triggering).
- Confirm PA6 = TMR13_CH1 on the bench (graduate the HARDWARE_PINOUT entry).

## 5. Real calibration (now that we can inject known signals)
Placeholder cal (base=0, upper=4095) makes vertical/trigger rough. With the ESP32
producing known DC levels and amplitudes, derive real per-range vertical scale +
offset and the trigger-DAC cal. Start with DCV points, then Vpp.

## 6. PR #13 (Komzpa) — follow through on the review
4 blockers filed (SPI3_GMUX revert, trigger-byte interception, H2 SHA guard, unit-
table retraction) + should-fixes. Options: (a) wait for Komzpa's fixes, (b) push the
mechanical B1–B4 fixes to his branch ourselves (offered in the review). Re-review,
merge when green. Hardware-check the merged build (task #5 from last session).

## 7. USB data out / PC remote (issue #10)
The app's USB CDC doesn't enumerate on unit #1 (suspected HICK clock drift). Fixing
it unlocks scope-data-to-host (the debug shell already has trace/screenshot cmds)
and answers issue #10. Path: copy how stock clocks USB (48 MHz divider at 240 MHz
SYSCLK) into our app. The ESP32 could also serve as a bridge later.

## 8. Community / housekeeping
- Watch #18 for Stlkv/maksidze reactions to the breakthrough; answer the netlist
  follow-ups Stlkv asked for (single-write arm-address sweep, runtime read framing).
- Repo at 93⭐ — expect a bump after the cold-boot post.
- Commit + push the devlog entry and this plan.
- Consider a short show-and-tell video (cold boot → live trace) now that it's real.

## Priority order
1 (timebase) → 2 (default boot) → then 4 (CH2) / 5 (cal) / 3 (bisect) as they unblock
UI work, with 6 (PR #13) and 7 (USB) interleaved. Item 1 is the one that turns
"captures" into "usable."
