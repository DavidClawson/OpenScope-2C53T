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
