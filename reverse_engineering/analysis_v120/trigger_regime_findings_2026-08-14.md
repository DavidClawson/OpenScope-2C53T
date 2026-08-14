# The "wedge" was never a wedge: trigger-regime readout findings (2026-08-14)

**Bench unit #1, `guest-coldtrace` @ commit 0727182, first fully remote bench session**
(CDC shell on `/dev/ttyACM0` + ESP32 siggen on `/dev/ttyUSB0`; the only hands-on steps
were IAP entry, power cycles, and probe re-wiring). All readings below went through
`spi3 opread` (new this session), which parks the acquisition task between CS frames
before touching the bus.

## Summary

The capture engine never wedges. What was diagnosed as a latched engine failure
requiring an FPGA power cycle is a **live, instantly-reversible readout regime**: our
0x04/0x05 read protocol only works while the engine free-runs (no trigger activity),
and returns stale buffers or unwritten-space `FF` the moment real trigger crossings
occur. Remove the stimulus (or move the trigger reference away) and full acquisition
resumes **the same instant, same boot, no reset of anything**.

This rewrites three standing claims and closes one open mystery, below.

## Refuted / corrected claims

1. **"The only recovery is a true FPGA power cycle; a pinhole reset does not fix it"**
   (fpga.c cadence note, 2026-08-14 morning) — **REFUTED the same day.** Measured: OK
   counter frozen at 27,362 with 100 kHz square attached; stimulus removed; OK resumed
   climbing at ~137/s within seconds, uptime continuous (same boot, no MCU or FPGA
   reset). The power cycles and resets of 2026-08-13/14 "worked" or "failed" according
   to what the input signal was doing at the time, not according to any reset.

2. **"30 ms probe cadence wedges the engine, 150 ms is safe"** (same note) —
   **CONFOUNDED, do not build on it.** Today a *deterministic* freeze-at-OK≈20
   reproduced at the 150 ms cadence twice in a row — with an active signal on CH1 —
   and full-speed ~137 reads/s ran clean for tens of thousands of frames with a quiet
   input. The controlling variable in every observation this project has recorded is
   **whether the input signal crosses the trigger reference**, not the read cadence.
   The cadence experiment needs re-running with signal state controlled before any
   cadence conclusion survives.

3. **"Cold-boot arm failed / the engine starts unarmed"** (this morning's working
   diagnosis) — **never true.** Boot arm works every time. The two "failed" cold boots
   both had an active ESP32 signal (1 kHz / 100 Hz sine) wired into CH1; the engine
   entered the triggered regime immediately and the readout+validity-gate combination
   *reported* it as dead. A cold boot with the input quiet (floating, or connected at
   a non-crossing level) arms and serves ~137 frames/s from the first second.

## The mechanism, as far as the bench can see it

Two regimes, flipped by comparator activity (input signal vs DAC1 trigger reference):

- **Free-run** (input never crosses the reference): continuous fresh captures, PC0
  paces reads at ~100–140/s, every frame full (1026/1026 non-FF) with live
  per-sample noise (span ~26–49). This is the only regime our current protocol
  handles, and the regime every successful run to date (2026-08-12/13 included) was
  measured in.
- **Triggered** (crossings occur): captures happen but the readout no longer streams.
  Consecutive reads under a 2 Hz square (levels 5 / 223 at the ADC) returned: a full
  constant-223 buffer, a full constant-5 buffer, **partial fills — 42/1026 and twice
  exactly 511/1026 real samples followed by `FF`**, and long runs of all-`FF`.

Decoded from that:

- **`FF` = unwritten buffer words. ⚠ CORRECTED THE SAME EVENING — this is WRONG;
  see the addendum below. `FF` = RAILED SAMPLES (ADC at 255; the 3 Vpp ESP32
  signal clips the range top). A cleared/unwritten buffer reads `0x00`.** The
  original (wrong) reasoning kept for the record: the readout serves samples up to the current fill
  point and floats/high past it. "All-FF" = armed with an empty (freshly cleared)
  buffer, not a dead bus and not a railed ADC. (The earlier all-FF panic reads were
  taken with a fast sine crossing the reference — permanently-just-cleared buffers.)
- **Without a re-arm, the last buffer is re-served stale.** With the reference parked
  at 4000 (above the signal's reach), the same constant frame returned indefinitely.
- **Exactly-511 fills happened twice** — suspicious of a 50% pre/post-trigger split
  or the 2×512 BSRAM half structure. Netlist question, do not guess further.
- The relationship is not a simple above/below-threshold rule: quiet input with ref
  mid (2048) and ref 0 behave differently from ref 4000 (healthy / stale-serve /
  armed-empty respectively). The comparator's polarity, hysteresis and what exactly
  re-arms a capture are **open items for the netlist**, which decodes this fabric
  (`gw1n2-apicula`, the R16C8/9 arm bank + readout mux work from the #18 sessions).

**Consequence for the firmware:** stock's runtime loop must be doing a per-capture
completion/re-arm step our `fpga_warmtest_acq_task` lacks. The June Saleae capture of
stock's runtime (04/05 pairs every ~29 ms) should be re-read for writes or framing
details between read pairs — that re-read is bench-free and is now the shortest path
to a scope that works on *triggered* signals, which is what a scope is for.

## Step 0b executed: the SPI3 read-opcode space is now mapped

`spi3 opsweep` ran clean (00..3F × 2048 B, zero canary failures, quiet input,
free-run regime):

| op (low 5 bits) | reply |
|---|---|
| 0x00 | zeros |
| 0x03 | constant = current ADC level (regime-dependent; stock's `00 01 42 2E` shape was a triggered-regime reading) |
| 0x04 / 0x05 | live CH1 / CH2 sample buffers |
| 0x09 | constant 0x03 (dynamic register, meaning unknown) |
| 0x0A | dynamic register tracking the **recent sample maximum** (read 0x89=137, then 0x65=101 = the concurrent noise-floor max; a prior square's 223 did **not** persist ⇒ windowed/decaying, not a since-boot latch) |
| 0x0B–0x1F | zeros |
| 0x20–0x3F | **exact aliases of 0x00–0x1F — the design decodes only the low 5 opcode bits** (verified: 23/24/25/29/2A ≡ 03/04/05/09/0A including live data) |

So the sweep is exhaustive by aliasing, and **no hidden ≥1024-word buffer opcode
exists in the free-run regime**. The BSRAM_1/2 hunt (bench plan Step 0b) is *not*
dead: that buffer pair sits on a gated clock whose enable cone contains the SPI
receiver, so it plausibly only fills in a triggered or timebase-configured regime —
re-sweep 0x06–0x1F under those regimes once the re-arm protocol is understood.
0x09/0x0A being min/max-shaped dynamic registers is consistent with BSRAM_1/2's
accumulate-in-place netlist signature; also unproven.

## Instrument notes (this project's recurring lesson, again)

- The freeze-at-OK≈20 signature was the **validity gate**, not the engine: in the
  triggered regime frames are constant or FF, `varies` is false, and only stray 0x80
  markers increment OK. OK≈20 at boot ≈ 200 ms of marker-flukes, not 20 real frames.
  The gate also *accepts* marker+FF frames — which is how the demo-trace latch turned
  off on garbage and rendered the "single flat blue line" (both channels constant
  227 after cal offset, drawn overlapping).
- `fpga.scope_status` (boot-time 0x03 read) is **all zeros on healthy armed boots
  too** — it is not an arm indicator. (The `status` shell command also wrongly prints
  "n/a (bit-bang path does not read it)" for it on coldtrace builds; stale
  suppression, minor, unfixed.)
- The acq task's defensive `scope_trigger_dac_raw(2048)` in the timeout branch
  silently fights any experiment that moves the trigger DAC — it did not contaminate
  today's readings (the branch doesn't run in the states we measured) but it will
  contaminate someone's, eventually.
- The ESP32 siggen's `dc <mV>` output level did not behave as commanded (square
  levels reached the ADC as 5/223, but `dc 0/1000/2500` all read ~82) — its DC path
  runs through the amplitude scaler; calibrate or fix `esp32_siggen.ino` before using
  DC levels quantitatively.

## Next steps, in value order

1. **Re-read the June stock runtime capture** for what stock does between 04/05 read
   pairs (writes? specific framing? PC0 edge discipline?) — bench-free, directly
   yields the triggered-readout protocol. `reverse_engineering/captures/`.
2. **Netlist session** on the trigger/readout cone: comparator input polarity, what
   clears/re-arms a capture, the fill-pointer/readout-mux interaction, why 511.
   (`gw1n2-apicula` tools, continuation of the M-series progress log.)
3. Implement the per-capture re-arm in `fpga_warmtest_acq_task`, then re-run the
   stimulus ladder — the acceptance test is a **stable rendered square wave**, the
   thing every prior "live trace" demo (noise + slow-level tracking) never was.
4. Re-run the cadence experiment with signal state controlled (only after 3).
5. Re-sweep opcodes 0x06–0x1F in the triggered regime (after 3; the sweep tool
   already exists).

---

# ADDENDUM — same evening: the trigger is DIGITAL, and triggered capture WORKS

Second half of the session, after the June-capture re-read and the edge-paced
build (commit 3b19014) went on the bench. Each item below is a live A/B on
bench unit #1, driven entirely over CDC + the ESP32.

## 1. Register 0x08 is a DIGITAL post-ADC trigger level — bench-proven both ways

With a quiet input (noise floor ~52–101, mean ~82) and the stock arm value
`08 AD` (level 173): **0 PC0 edges/s**. Write `08 37` (level 55, inside the
noise band): **4.6 edges/s — the noise itself starts triggering.** Restore
`08 AD`: 0 edges/s again. The trigger comparator lives in the fabric and
compares ADC codes; stock's arm sequence sets level 173.

**Corollary — DAC1 (PA4) is NOT the trigger comparator reference.** It is much
more likely the frontend's **vertical offset/bias**: the 2026-08-12 finding
("reset zeroes DAC1 → captures read flat" and `trig raw 4000` → all-FF frames)
stays factually intact but the mechanism is re-read as *signal pushed out of /
across the ADC window*, not a dead comparator. `scope_trigger_dac_raw()` and
the CLAUDE.md/CALIBRATION.md "trigger comparator DAC1" language need a
correction pass once this is netlist-confirmed.

## 2. FF disambiguated: railed samples, not unwritten words

1 Vpp sine (cannot rail) + trigger level 0x64 matched to it: **five
consecutive full frames of live data, zero FF bytes anywhere** — and one
all-`0x00` frame caught mid-cycle. So: cleared/unwritten buffer = `0x00`
(consistent with the sweep's zero-filled unused opcodes); `FF` = ADC railed at
255 (3 Vpp ESP32 output clips the current range). The main-text "FF =
unwritten words" claim is corrected accordingly. (Squares read high≈223 while
sine peaks read 255 — frontend bandwidth/overshoot question, unresolved,
minor.)

## 3. Triggered capture WORKS when the level matches the signal

Same test: full live frames on every read, means tracking the signal slice.
The "triggered regime collapse" of the morning was three stacked artifacts:
trigger level 173 vs a signal railing through it, ADC railing reading as FF,
and the validity gate rejecting constant frames. With the level matched and
railing avoided, **the scope captures triggered sweeps of a real signal on
open firmware.** Next feature step: wire the scope UI's trigger-level control
to an SPI `0x08` write (it currently only moves DAC1 = the offset).

## 4. Default capture rate measured: ~2.7 kS/s (fill ≈ 375 ms)

Trigger-frequency sweep with the PC0 edge counter: completions at ≤2.8 Hz,
zero at ≥3.0 Hz; edge rate saturates ~2.67/s regardless of trigger rate ⇒ the
1024-sample fill takes ~350–375 ms ⇒ **the free-running default sample clock
is ~2.7 kS/s**, not 250 MS/s. Stock's one 416 ms first-cycle outlier (win15)
is the same slow default fill.

## 5. THE new central question: what makes stock's engine 23× faster

After that first slow fill, stock sustains **18 ms cycles (~57 kS/s)** with
**byte-identical SPI input to ours** (MOSI pure opcode+FF, USART silent, same
five arm writes). Something outside the replayed SPI/USART traffic switches
stock's engine fast — and whatever it is, it is probably the timebase
mechanism (bench plan §1 / dev plan F4). Candidates, none tested: the analog
posture (stock boots DMM: PC11 high, meter relay bank, meter-mode DAC1 value),
some interaction of completed read-pairs at the right cadence, or fabric state
we have not identified. Netlist + a DMM-posture replay are the two obvious
attacks.

## 6. PC0 edge semantics (measured, this build)

Falling edges fire **once per completed capture** in the triggered regime and
**not at all** in the never-triggered state — yet never-triggered reads return
fresh varying noise every time. Interpretive question left open: the engine
may be a continuous circular pre-trigger writer (read = live stream; trigger =
freeze+complete), which would fit all of today's observations. Netlist item.

## Firmware state after this session

Commit 3b19014 (`guest-coldtrace`, flashed and validated on unit #1): EXINT0
PC0 edge counter (`status` → "PC0 edges"), edge-paced acquisition with probe
fallback, 3-byte read-header capture (`acq hdr` in `status`), gate honours
stock's b2==01 valid flag. The b2==01 flag has **never** been observed on our
engine in any regime — stock-state-specific, unexplained, tracked under item 5.

---

# ADDENDUM 2 — 2026-08-15: posture and wire-protocol EXCLUDED; the 23× is a regime, and it is not set by anything we replay

Next-day bench session, fully remote (CDC shell + ESP32), plus one flash cycle
(`guest-coldtrace-faithful`, David on IAP/power duty).

## 7. Detector-design note that un-broke the morning

A 20 Hz trigger square produces **zero** completions — §3's cutoff is a hard
"none above ~3 Hz" (a retrigger mid-fill restarts the capture), not a
saturation. The morning's "engine dead" scare was this, mis-read. Corollary
kept: a 20 Hz square is a clean *binary speed detector* — it can only produce
completions if the fill gets much faster (fill < 50 ms), so baseline 0 is the
expected negative.

## 8. DMM analog posture does NOT set the capture rate (A/B, controls passed)

PC11 HIGH, the full meter relay bank (PC12/PE4/PE5/PE6/PA15/PA10/PB10 in DCV
pattern), and both together: 20 Hz detector 0.00/s at every rung while a 2 Hz
control confirmed the trigger still crossed (1.75–2.50/s). Posture flips
tested both *after* arm and (via re-arm) *at* arm time; also PA2 driven
HIGH/LOW at arm (our silent build leaves the FPGA's USART RX floating — the
classic invisible-pin class). **All negative**: `0x03` byte1 stays 00, no
auto-refresh.

## 9. Stock's June capture, re-read at the window level — the 23× reframed

- Stock's **first** 04 read (win 13) is valid (`b2=01`) **0.6 ms after the
  arm write** — the buffer was already full from free-running during the
  600 ms post-close silence.
- The famous 416 ms gap sits between read 1 and read 2 (win 13→15); after
  that no gap exceeds 30 ms.
- The 31 invalid (`b2=00`) windows are **scattered** through 5.5–10.2 s, not
  clustered at the start: b2 is a *freshness* flag, and stock's buffer
  refreshes at ~26–45/s **with a probe that never triggers** (floating, level
  0xAD=173).
- CH2 (05) b2 is always 00 — the freshness flag lives on CH1 only. Stock CH1
  b0 ∈ {00×148, 80×26}; ours is always 80.

**So the "23×" is not a missing clock divider; stock's engine runs an
auto-refresh regime ours never enters.** Our engine refreshes the read buffer
only on a trigger; stock's refreshes continuously. The engines already
diverge at boot: stock's post-arm `0x03` reads `00 01 42 2E 2E`; ours reads
`00 00 <sample-ish>` in every state we can produce.

## 10. Faithful-boot experiment: wire protocol EXCLUDED (negative, decisive)

`make guest-coldtrace-faithful` made the bit-bang config+arm byte-exact to
stock's captured windows 0–13, closing all five wire deltas in one shot: (1)
no 0x41/0x11 reads between payload and 0x3A close, (2) 05 00 ERASE_SRAM
prelude present, (3) stock's empty-CS-pulse + 100 ms prelude spacing with no
prelude reads, (4) the lone 00-byte frame after close, (5) arm writes
back-to-back at /2 with the 0x03 read at /2. Flashed, genuine FPGA power
cycle, quiet input (stock-capture conditions):

- quiet-input PC0 rate **0.00/s** (no auto-refresh)
- live `0x03` = `00 00 5A …` (byte1 still 00)
- headers still sample-like (`80 00 4D`)

Side result: the faithful sequence configures and arms fine, warm AND cold —
including with 0x05 ERASE present, which closes half of the old Build-B
bisect (0x05 is harmless on the bit-bang path; whether its *absence* matters
was mooted).

**Conclusion: everything we replay — bytes, framing, order, spacing, posture
pins, PA2 — is now excluded as the regime switch.** What the capture could
not see (it had 8 channels: SPI ×4, USART ×2, PC6, PB11) and what we have
not simulated is where the answer lives. Also recorded: the June capture ran
/64-patched stock **in DMM mode** — stock's fast 04/05 cadence is its
DMM-mode acquisition; "scope mode" was never captured. The fast regime may be
the meter-ADC fabric path presenting on the same opcodes.

## Next attack: the netlist/sim session (bench exhausted for this question)

Sharp questions for `gw1n2-apicula` + the m_capture.py sim harness:
1. What drives byte1 of the opcode-0x03 response (stock 01, ours 00)?
2. What logic pulses PC0, and what re-starts a fill (auto-refresh vs
   triggered one-shot)?
3. What do arm regs 01 (=08) and 02 (=03) select — mode? decimation? Is
   there a divisor register the five writes never touch?
4. b2 freshness-flag mechanism on CH1 (and why ours reads sample-like bytes
   where stock has flags — response mux alignment?).

---

# ADDENDUM 3 — 2026-08-15: netlist session (gw1n2-apicula M12) — the 23x rate control is a real SPI-reachable counter on BSRAM_1/2

Since the bench excluded every MCU-side/wire hypothesis (Addendum 2), the
switch is in the fabric. Took it to the `gw1n2-apicula` unpacked netlist. Full
writeup: that repo's `docs/06-progress-log.md` § M12. Four new tools there
(`m_regime/m_readmux/m_countchk/m_divider/m_gate40`).

## Result — a programmable-rate write gate on the slow buffer, loadable over SPI

M11 had localized the only gated/slow storage to **BSRAM_1/2** (a both-channels,
1024-word, accumulate-in-place record — M9 — written on a GATED fabric clock
GB40, not the raw PLL clock the scope buffers use). M12 traced GB40's gate
(`R4C13_LUT4_7`) driver cone and it is **a counter/accumulator, not a bare
enable**:
- a self-feeding accumulator (`R13C5_DFFE_0/4/5`) = the divider element;
- gated by the BSRAM_1/2 address-counter phase (`R12C12 ← R8C9/R8C10`);
- **reachable from the SPI arm/receiver bank** (`R13C8 ← R14C8/R16C9`; `R14C8`
  is an arm flop, `R16C9` the SPI bit-counter).

A counter feeding a clock gate, with a load path from the SPI writes, is the
structural definition of a **programmable sample-rate divider** — and it sits
on the buffer pair **this firmware has never read** (only 0x04/0x05 =
BSRAM_0/3, the raw scope buffers). This is the strongest candidate yet for both
the timebase and stock's 23x auto-refresh: stock may be driving/ reading the
BSRAM_1/2 path (continuously written by its gated clock) while our engine only
uses the trigger-gated raw buffers.

## Two caveats that keep this honest
1. **Reachability ≠ located bit.** The cone is a counter (unlike the scope
   buffers' bare run&~done CEA), which is why this is more than M8's refuted
   "SPI reaches the cone" — but the actual divisor VALUE / which written
   register is not yet pinned.
2. **The sim can't run the divider**, for a now-precisely-located reason: the
   SPI-receiver/arm flops' CLEAR and clock nets are on the **long-wire (LW)
   branch network**, which is undriven in the unpacked netlist because
   apicula's chipdb has no `GW1N-2` segment model. Same dead-net class as the
   BSRAM pips, but not patchable from `db.nodes`. Fixing it (a
   `_segment_data['GW1N-2']` entry, modeled on GW1N-1/GW1NZ-1) is the apicula
   PR that would unblock BOTH the SPI address-decode sim and the divider sim.

## Bench action this hands us (cheap, next session)
Read-opcode sweep **targeting BSRAM_1/2**, on guest-coldtrace: the readout mux
(M9) shares SO across all four blocks under different selects, so some opcode
other than 0x04/0x05 should return the 1024-word accumulate record. If its
content auto-refreshes on a quiet input (unlike 0x04/0x05, which only refresh on
a trigger), that IS stock's fast regime, read directly. The opcode space is
low-5-bit (Addendum 1), and 0x06-0x1F were only swept in the never-triggered
regime — re-sweep them here watching for a long, trigger-independent reply.

---

# ADDENDUM 4 — 2026-08-15: BSRAM_1/2 opcode sweep (NEGATIVE) + the raw buffer free-runs and is directly pollable

Bench, guest-coldtrace-faithful, quiet input (ESP32 off, PC0 edges frozen at 0).
Ran the M12 read-opcode sweep. `bsram_sweep.py`.

## 1. NEGATIVE: no read opcode exposes BSRAM_1/2 in our engine state
Swept 0x06-0x1F (twice each, ~400 ms apart, looking for a long trigger-
independent reply). **Only 0x03/0x04/0x05 respond (0x09/0x0A too when a signal
is present); 0x06-0x1F are inert zeros.** So BSRAM_1/2 is NOT reachable by a
distinct read opcode. If the MCU reads it at all it must be through the SHARED
readout mux (M9) under a config-set SELECT (M12: the select nets are real but
route through the unpacker's dead F-lane class) — i.e. the same 0x04/0x05
opcode, mux-switched by a config bit we have not set, not a new opcode. The
"read BSRAM_1/2 directly" shortcut does not exist from firmware as-is.

## 2. POSITIVE and important: the raw buffer (BSRAM_0/3) free-runs and holds a COHERENT waveform, readable independent of any trigger
- With the input quiet and **PC0 frozen at 0** (no triggering), `0x04`/`0x05`
  content still CHANGES between reads. The raw capture buffer writes
  continuously regardless of trigger state.
- With a signal present, a direct `0x04` poll returns a **coherent waveform** —
  e.g. a 30 Hz sine shows ~13 clean cycles across the 1024-sample buffer, and
  the shape tracks the input. This is not scrambled live noise; it is the real
  captured trace.
- **This reframes the whole "engine only updates on trigger" problem.** Our
  acquisition task is EDGE-PACED (waits for a PC0/trigger-completion edge), so a
  quiet or non-triggering input makes it read ~never even though the buffer is
  full of fresh, coherent data. **A free-running / auto-trigger display is a
  firmware READ-PACING change** — poll 0x04/0x05 on a timer (stock's ~18-34 ms
  cadence) instead of waiting for PC0 — NOT a fabric change. This is the direct,
  shippable win from this session.

## 3. The 23x, re-stated precisely (and it is now clearly off-bench)
The slow ~2.7/s number was always the *trigger-completion* (PC0) cadence, never
the write rate. Two independent methods put our SAMPLE rate at low kS/s
(yesterday's trigger-frequency cutoff ~2.7 kS/s; today's 30 Hz→13-cycles/buffer
~2.4 kS/s), and higher frequencies visibly ALIAS in the buffer (10 Hz shows
more cycles than 40/80 Hz), confirming the low rate. Stock's 18 ms fresh-buffer
cadence implies ~57 kS/s ⇒ the ~23x is a genuine **sample-rate** gap on
identical SPI/USART. Per M8 the ADC clock is a bare PLL forward (no fabric
divider), so the difference is either the FPGA-internal PLL config (not
decodable by current apicula) or a raw-vs-accumulate BUFFER select (M9/M12
BSRAM_1/2, whose write clock IS counter-gated and SPI-reachable). Both are
netlist/sim questions; the bench has now given all it can on the rate itself.

## Net for next session
1. **Firmware (shippable): add an auto/free-run acquisition mode** that
   timer-polls 0x04/0x05 (~30 Hz) when not in triggered mode — gives a
   continuously-updating trace on any input, the thing the scope visibly lacks.
   The edge-paced path stays for triggered/normal mode.
2. **Netlist (the 23x): the readout-mux SELECT and the FPGA PLL** — needs the
   apicula GW1N-2 long-wire segment model (M12 blocker) before either simulates.
