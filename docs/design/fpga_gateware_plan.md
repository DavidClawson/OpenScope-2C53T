# FPGA Gateware Plan — Custom Bitstream & Trigger Engine

*2026-08-13. Distilled from a claude.ai design chat (David + Claude), cross-referenced
and corrected against this repo. Companion to the "Stretch track: custom Gowin
bitstreams" section of `docs/roadmap.md` and the `gw1n2-apicula` sibling repo
(`~/gw1n2-apicula`, GitHub `DavidClawson/gw1n2-apicula`). Supersedes the resource
survey in `fpga_future.md` for planning purposes (that doc's specs remain valid).*

**Context that makes this real:** config entry is solved (Build B bit-bang loader,
2026-08-13) — our firmware uploads arbitrary bitstreams to the GW1N-UV2 from a cold
boot, every boot. The stock bitstream is already unpacked to a fabric netlist
(`gw1n2-apicula/tools/m5/scope_unpacked.v`, ~108K lines) and partially reverse-
engineered. Incoming bench hardware: a signal generator and a **Sipeed Tang Nano 20K**
dev board (both expected ~late August 2026).

---

## 1. Open questions from the design chat — answered from the repo

The original notes listed these as unknowns. Most are already settled here:

| Question | Answer | Source |
|---|---|---|
| How many channels digitized? | **2** (CH1/CH2; per-channel 1026-byte SPI3 reads, opcodes 0x04/0x05; dual mode confirmed) | June #18 capture; warmtest bench |
| GW1N-2 exact resources | 2,304 LUT4 / 2,016 FF / 72 Kbit BSRAM / 1 rPLL / **no DSP** / 96 Kbit user flash | `fpga_future.md`; gw1n2-apicula |
| Where does sample memory live? | **BSRAM only** — netlist trace maps the ADC bus directly into the BRAMs; no external memory | `m_datapath.py` trace |
| Is the ADC bus DDR? | **YES — confirmed 2026-08-13**: 15 IDDRC + 1 ODDRC in the netlist (7 IDDR pairs + 1 single, all on east/west edge tiles = the ADC buses; effective sample rate is 2× the bus clock) | `m_utilization.py` |
| Is there a PLL, at what frequency? | Yes, the rPLL — configuration only partially decoded (Apicula gap); the fuzzing-oracle work targets exactly this | gw1n2-apicula workplan |
| Does acquisition stall during readout? | Effectively yes: engine starts **unarmed** after config, captures a buffer, halts until re-armed. Stock drains 04/05 pairs every ~29 ms (~34 Hz). Whether stock ping-pongs internally is answerable from the netlist | engine-arm saga; June capture |
| JTAG pads on the board? | **Yes** — four gold pads; DMM buzz-out vs MCU pins still pending. Less urgent now that SSPI config entry works; would still decouple "design correct?" from "config path correct?" during dev | CLAUDE.md next-steps |
| Stock utilization / headroom | **ANSWERED 2026-08-13** — see §8.1 results below. Headline: LUTs 45% (plenty), FFs ~82% (the scarce resource), **BSRAM 4/4 = zero headroom** | `m_utilization.py` |

## 2. Corrections to the design-chat notes

- **Netlist semantics are further along than the notes assume.** The notes warn that
  unpacking yields pin config but not semantics ("ball 42 = ADC bit 5"), calling that
  reconstruction "much harder." It's already done for the load-bearing paths: the
  `m_*.py` series traced pins, ADC→BRAM datapath, capture counter, SPI control
  register, readback mux, and the arm register — and the gate-level arm-cone sim
  (`m_simarm.py`) predicted the PB11∧PC6 arm condition that then worked on the bench.
- **Config-time bit-banging isn't merely "fine," it's currently load-bearing.**
  Hardware-SPI3 config failed (Build A); the bit-bang path is what broke the wall
  (Build B). Bisect (bit-bang vs AF pins vs no-0x05) still open.
- **Gowin EDA Education builds GW1N-2 today.** Authoring a replacement design does
  NOT wait on the Apicula chipdb. Apicula matters for unpacking (done) and eventual
  open-toolchain purity; the build path can be vendor tools from day one
  (install target: mars, the Linux bench box).

## 3. The Tang Nano 20K's role — testbench, not proxy

Board: Gowin **GW2AR-LV18QN88C8/I7** (Arora family) — 20,736 LUT4, 828 Kbit BSRAM,
48 DSP, 2 PLL, 8 MB die-stacked SDRAM, BL616 onboard JTAG/USB-UART, 27 MHz crystal +
MS5351 clock generator, HDMI/microSD/LCD headers.

**Wrong family, ~10× the fabric, different PLL/BSRAM/IO primitives, SDRAM the scope
doesn't have.** A green build on the Tang says nothing about fitting the GW1N-2 —
that is the one real failure mode to guard against.

What it IS good for, in order of value:
1. **Synthetic waveform playback into the trigger engine** (§5) — real-silicon
   validation with cycle-exact stimulus.
2. **GAO probing of internal signals** — on the target, fabric and I/O are too scarce
   to instrument; on the Tang you can watch FSM state, zone bits, counters live.
3. **MCU protocol bring-up** against a freely instrumentable design.
4. **Toolchain learning** — GAO, `.cst` syntax, primitive behavior, and the
   full open flow (mainline Apicula/nextpnr already supports GW2AR-18, so the
   yosys→nextpnr→openFPGALoader loop works on the Tang today, no vendor tools needed).

**Discipline that makes it transfer:**
- The trigger engine is **one module with a clean interface**. That module is the DUT;
  everything else on the Tang (playback engine, SDRAM buffers, debug infra) is
  scaffolding that never ships and lives in separate modules.
- Maintain a **parallel GW1N-2 project** (Gowin EDA) and synthesize the DUT against it
  on every meaningful change — continuous fit reports, not an integration-day surprise.
- Anything instantiating Gowin primitives directly (vs inferring) needs a per-target
  variant or guard (rPLL vs PLLVR, BSRAM modes, IDDR).

## 4. Trigger engine architecture

**One composable engine, not N independent trigger blocks.** On ~2,300 LUTs this is
the difference between three modes and ten.

```
       ┌─────────────┐
ADC ──▶│ comparator  │──▶ zone[1:0]   (below / mid / above)
       │  (hi + lo)  │
       └─────────────┘
              │
              ▼
       ┌─────────────┐
       │ zone→event  │──▶ event strobes
       │   encoder   │
       └─────────────┘
              │
        ┌─────┴─────┐
        ▼           ▼
  ┌──────────┐  ┌────────┐
  │ shared   │  │  FSM   │──▶ trigger
  │ counter  │◀▶│ (cfg   │
  └──────────┘  │  regs) │
                └────────┘
```

Two threshold registers give a 2-bit zone per sample; zone transitions become event
strobes; one shared counter measures durations; a small register-configured FSM
consumes events. Each mode is then a *configuration*:

| Mode | Expressed as |
|---|---|
| Edge | zone transition |
| Window | zone membership |
| Runt | entered mid zone, returned without reaching far zone |
| Pulse width | counter measures edge→edge vs min/max |
| Slew rate | counter measures low-exit → high-entry |
| Dropout | counter overflow with no event |
| Nth edge | counter counts events instead of cycles |

**Build order:**
1. **Pulse width** (~100–150 LUTs incl. counter). Highest user value and it forces the
   shared front end into existence. Size the counter for the slowest timebase
   (~20 bits to cover 10 ms at 100 MSPS).
2. Runt / window / dropout / slew (~30–80 LUTs each once the front end exists).
3. **Holdoff** — one more counter; makes repetitive complex waveforms usable.
4. Serial triggering: **decode is MCU work** (post-hoc, branchy); only *protocol
   triggering* (decide-before-store, e.g. "trigger on I²C addr 0x3C") belongs in
   fabric (~150–250 LUTs shift+compare). With 2 analog channels, two-wire protocols
   are possible in principle but signal conditioning is the constraint; UART/1-Wire
   triggering is the realistic first target.

**Memory trick for decode:** decoding needs the *thresholded* stream, not 8-bit
samples. A 1-bit digitized record alongside the analog one gives 8× record length in
the same BSRAM — short analog record for display, long 1-bit record for MCU parsing,
concurrently.

## 5. Test scaffolding — three legs, one stimulus set

One Python waveform generator emits **both** a cocotb stimulus array and a `.mem`
file, so identical golden cases run everywhere. Disagreement between legs is signal,
not ambiguity.

1. **cocotb/Verilator** (primary loop, seconds/iteration): sweep trigger levels
   across thousands of waveforms, assert the *exact sample* the trigger fires.
   Regression suite grows with each mode. This is where hysteresis and off-by-one
   bugs die.
2. **Tang playback**: a BSRAM playback engine (~100 lines of Verilog — counter
   walking a sample memory, driving exact ADC bus timing) wired to the DUT inside the
   fabric, patterns loaded over UART. Loop the buffer for continuous-trigger tests;
   the Tang's SDRAM is the escape hatch for long records (legitimate — scaffolding,
   not DUT).
3. **Stock-netlist sim** (unique to this project): the same stimuli driven into the
   unpacked `scope_unpacked.v` gate-level sim — an executable spec of stock behavior
   to diff our replacement against.

Golden cases: clean sine; sine + 3-sample glitch; runt pulses near threshold; noisy
edges (hysteresis); pulse trains bracketing width thresholds; long quiet (dropout).

## 6. Dev cycle & first milestone

Fastest feedback first: **cocotb → Tang (CDC/timing/GAO) → target hardware last**,
with the parallel GW1N-2 fit report green before anything reaches the scope.

**First milestone: a protocol-compatible clone** — a design mimicking stock's runtime
interface closely enough that existing MCU firmware talks to it unmodified (0x04/0x05
reads, the five arm writes, the 0x03 status). This produces a working trace as a
checkpoint and cleanly separates "my FPGA design is right" from "my MCU side is
right." Extend only after that. (This is also the roadmap's step 2 — fork the working
front end rather than reinvent 250 MS/s sampling.)

## 7. Runtime protocol — once the bitstream is ours

- **Capability/version register at a fixed address** (magic + version + feature
  bitmap), from day one. The bitstream ships inside MCU firmware, but mismatched
  combos will happen via partial flashes and unknown builds; the MCU should detect
  what it's talking to and fail cleanly. Cheap now, painful to retrofit; slots into
  the module/plugin system.
- **Min/max peak-detect decimation in fabric** — the single biggest win. Sends
  display-ready column pairs instead of full records: transfer size collapses at slow
  timebases AND narrow glitches stay visible. This is the sharper version of the
  roadmap's "hardware decimation" timebase fix.
- **Sub-sample trigger interpolation** — small logic; kills the ±1-sample jitter that
  makes fast timebases look unstable.
- **Ping-pong buffering** if it fits in 72 Kbit — acquire into one buffer while the
  MCU drains the other; the difference between continuous update and going blind
  every readout. Stock's arrangement (whether it already ping-pongs) is readable from
  the netlist. ⚠ Utilization result (§8.1): stock already uses **all four BSRAM
  blocks**, so ping-pong can't be added with new memory — it means halving record
  depth or repartitioning what the four blocks hold (or discovering stock already
  ping-pongs, which the CH1/CH2 × 2-block split makes plausible).

## 8. Head start (before the hardware arrives)

1. **Stock utilization script** over `scope_unpacked.v` → exact LUT/FF/BSRAM headroom.
   **DONE 2026-08-13** — `gw1n2-apicula/tools/m_utilization.py`. Results:
   - **LUT4 sites: 1,047 / 2,304 (45.4%)** — 847 LUT4 + 192 ALU (carry) + 2
     RAM16SDP4. ~1,257 sites free: the trigger engine fits comfortably.
   - **FF sites: 1,416 / 1,728 (81.9%)** — registers, not LUTs, are the scarce
     resource. ~312 free against the conservative 1,728 budget (~600 if the
     budget is really 2,016 — datasheet check pending). Counter-heavy additions
     (pulse width + holdoff ≈ 40–60 FFs) fit; be deliberate, not anxious.
   - **BSRAM: 4 / 4 blocks — ZERO headroom.** Ping-pong buffering via *more*
     BSRAM is off the table; any record-depth or 1-bit-stream idea must
     restructure the existing four blocks, not add a fifth.
   - DDR confirmed: 15 IDDRC + 1 ODDRC (see §1). rPLL confirmed in use via
     clock-tap routing wires (`TRPLL0CLK*` at R10C10/C11) even though the
     chipdb can't decode the primitive itself yet.
   - 59 I/O pads (55 in / 3 out / 1 bidir); 1,296 wide-mux (MUX2) sites used.
2. **Gowin EDA Education on mars** — **ALREADY INSTALLED** (`~/gowin/
   V1.9.11.03_Education` + `gw1n2-apicula/tools/gowin-env.sh`), discovered
   2026-08-13. What remains is using it: the GW1N-2 parallel-fit project and the
   rPLL differential-fuzzing oracle (`gw1n2-apicula/docs/03-workplan.md`).
3. **Full-netlist sim harness** — extend `m_simarm.py` from the arm cone to the whole
   capture path (fake ADC in → 0x04/0x05 out): the executable spec of §5 leg 3.
   **STARTED 2026-08-13, core milestone hit** — `gw1n2-apicula/tools/m_capture.py`
   captures synthetic ADC stimulus end-to-end through the stock netlist into
   BSRAM_0 (512 samples then halt — reproducing the one-buffer pathology, and
   matching stock's per-channel read size). En route it found two systematic
   unpacker bugs that had silently degraded every previous sim (VCC-tied LSR
   nets holding cells in reset — the ADC front end included; BSRAM macro-internal
   pips never emitted), and decoded the capture memory geometry: 18-bit word =
   DDR sample pair in 9-bit lanes, linear address counter, **left-edge ADC bit
   order D7→D0 top-to-bottom**. Still open: lane bit-0 semantics, CH2, the SPI
   readout path (SPI decode isn't simulable — faked clock tree — so the harness
   forces the known run/done flop pair instead). Details:
   `gw1n2-apicula/docs/06-progress-log.md` § 2026-08-13.
4. **Python waveform generator** (§5) — pure software, needed by every leg.

## 9. Safety rule (from the roadmap, restated because it's absolute)

Experiment freely with SRAM config — a bad bitstream means no scope until reboot,
zero brick risk. **Never write the FPGA's NV flash.** It holds the meter design and
is the only copy.
