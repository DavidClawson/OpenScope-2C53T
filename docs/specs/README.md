# Feature specs — the forward half of the maturity matrix

The README's [Feature maturity](../../README.md#feature-maturity) table says
where each feature **stands**. This directory says what would **move each one
up** — and which features should exist that don't yet.

## How this works

A spec here is a **promotion ladder, not a wish**. This project's
characteristic failure is the stable, plausible, wrong number — labels never
derived from anything, "implemented and tested" claims for code with no call
sites. Prose aspirations rot into exactly that. A promotion criterion cannot:
it is a test that passes, a measurement written up in
[`docs/experiments/`](../experiments/), or a behaviour demonstrable on the
bench, and it is either satisfied or it isn't.

- One spec per feature, from [`TEMPLATE.md`](TEMPLATE.md), one page.
- Specs are proposed and reviewed by PR like code. Disagreeing with a
  promotion criterion is the intended form of design review.
- A feature **promotes only when its criterion is met**, and the README matrix
  row moves in the same commit as the evidence.
- Stage definitions (S0 Written → S1 Wired → S2 Measured → S3 Guarded →
  S4 Polished) live in the README and are not redefined here.

## Catalog

"Next" is the single move that spec names as the promotion criterion for the
next stage — the point of the whole exercise is that this column is always
concrete.

### Scope

| Feature | Stage | Spec | Next |
|---|---|---|---|
| Cold-boot FPGA config | S2 | — | Regression guard on the config path; hardware-SPI gap stays a research thread, not a spec |
| Live capture CH1 | S2 | — | Guard: scripted capture acceptance in `bench.py` |
| Acquisition record integrity | **S2** (EXP-53/54, 2026-09-22) | *needed* | **Model rewritten (EXP-53/54):** the FPGA is a free-running triggered capture engine with a handshake — it captures 512 samples after a crossing of (reg-0x08 − 28) on *either* edge, holds the completed record until an opcode-04/05 read, strobes PC0 on each such read (6–9 µs after it), holds ~189 ms, then rolls again from the pointer. PC0 is a handover strobe, not a completion flag; the record is *complete and held* when the read finds it. The acq loop is now a poll (fill + 100 ms after a handover, then every 30 ms, stock's 29 ms cadence): the read that strobes is committed, reads that find the roll are discarded. Retired: the "arming read", the two-phase hold read (EXP-50), the latency-derived un-rotation (EXP-52, never ran), the "invalid head" (EXP-42) — the head was the seam, and the seam is the FPGA's pointer, which no MCU timestamp recovers. Committed records at the body floor at 0x10/0x11/0x12, 201.2 Hz and 50 Hz; strobe → commit 1 ms. S3: fold the EXP-54 arms into `exp22_stability.py`; the whole-record seam remains for FFT/measurement consumers (soft trigger covers the display) |
| Live capture CH2 | **S2** (EXP-38, 2026-09-14) | — | Armed at boot by default: fresh `guest-coldtrace` boot reads op05 = its own 2 kHz tone (bin 163 ×5) with op04 = 1 kHz (bin 81 ×5), no shell command; A/B against the `noch2` control image reproduces the old all-zeros. S3: re-run the CH2 attenuator ladder armed (the one-usable-tap result was unarmed) + one true-power-cycle repeat |
| Vertical scale | S3 | — | S4 blocked on a calibrated source (`SCOPE_CAL_SOURCE_SCALE`) — Help Wanted #3b |
| Horizontal scale | S3 | — | Codes 0x09–0x0C need a faster source; 0x06–0x08 need the narrow-field/roll-mode hypothesis tested |
| Freq badge | S3 | — | S4: refusal states could say *why* (`torn` vs `no peak`) |
| Auto-measurements (real units) | S1 | [auto-measurements](scope/auto-measurements.md) | S2: badge volts/seconds validated against a bench-driven signal |
| FFT + waterfall on live data | **S2 (record side complete, screen at 0x10 + refusal seen; 2026-09-22)** | [fft-live](scope/fft-live.md) | Bench: peak bin 328/164/82 exact at 0x10/0x0F/0x0E on the device's own record, 8 kHz fold at 1475 (= rate 12,497 S/s, the table's 0.06 %); header `LIVE CH1 pk 1.0kHz` on screen, `pk bin N` on an unmeasured code. Defect fixed in code: axis/dB labels were transparent grey on the bars, invisible — now opaque; screen check pending next flash. S3: the record carries one seam at the FPGA's pointer (EXP-53/54) → leakage; window/averaging by button; waterfall raster cost |
| Trigger level | S1 | [trigger-modes](scope/trigger-modes.md) | S2: measure level-vs-ADC-code transfer on the bench. ⚠ The register is real and bench-proven, but **no control reaches it** — `scope_adjust_trigger_level()` has zero callers, so the level is immutable at runtime and the on-screen marker never moves (spec D1) |
| Trigger modes (auto / normal / single) | **S1** (EXP-47 → EXP-54, 2026-09-22) | [trigger-modes](scope/trigger-modes.md) | S2 remaining: (a) level-vs-ADC-code transfer — **measured offset: the comparator sees the record + 28** (level 152 triggers on a peak of 133, level 54 does not on a floor of 33; EXP-53 50j, range 5 only); (c) edge polarity — **the FPGA triggers on either edge** (EXP-53 50j; whether reg 0x02 selects one is untested). EXP-54 on the poll loop: NORMAL at 0x0E/0x10/0x11/0x12 advancing, holds at +100, resumes at 0, SINGLE 3/3, AUTO edge-gated; **NORMAL on a 1 Hz / 4 Hz square now triggers** (it froze on the single-arming-read images because nothing read again after the crossing). Wishlist Tier 1 #1; milestone M2 |
| Cursors | S1 | *needed* | Units now derive from `scope_cal` / `scope_timebase` and refuse when the table has no entry (`scope_cursor.c`, host-tested with negative controls, 2026-09-12). **Not S2: unverified on the bench** — next is a cursor delta read against a known signal, and against the badges on the same capture |
| Autofit vs. measured graticule | S1 | *needed* | Decision pending: the vertical graticule does not mean the volts/div the status bar prints |
| Math channels | S0 | — | After auto-measurements S2 (same input plumbing) |
| XY / roll / trend / mask | S0 | — | Unclaimed; each needs a spec before work starts |
| Protocol decoders | S0 | — | Needs a spec: capture-depth and sample-rate reality check first (132 host tests already exist) |
| Bode plot | S0 | — | Blocked on siggen/scope coexistence (shared DAC1) |

### Meter

| Feature | Stage | Spec | Next |
|---|---|---|---|
| Multimeter in the scope build | **S1** (coldtrace, all submodes *accepted*) | [meter-in-the-scope-build](meter/meter-in-the-scope-build.md) | EXP-25/26/27/28 (2026-09-12): the `AA 55` TX header replicates on unit #1, commanded modes are acknowledged (`echo_frames` non-zero for the first time), 10 kΩ reads 9.775 kΩ, and the scope is undisturbed. **Not S2**: the 10 kΩ is a ±5% part, so it bounds the reading without being a reference. S2 needs a bench DMM across the same load. Word table + `AA 55` header landed (#33, `12c038c`); seven-segment decoder landed (#35, `e9891ae`), both host-tested and bench-exercised on unit #2 only. Next: run both on unit #1 with the eight untested words (capacitor, diode, thermocouple), then fold `guest-coldtrace-meter` into the default build |
| Manual range lock | S-none | — | Wishlist Tier 1 #2; spec after coexistence reaches S2 |
| DCV >10 V | S1 (known-wrong) | — | Decimal-latch bug documented since 2026-04-04; folds into the coexistence spec's S4 |
| Fuse current tester | S1 | — | Unvalidated against known loads |

### Signal generator

| Feature | Stage | Spec | Next |
|---|---|---|---|
| DDS output | S1 | *needed* | S2: characterise output against our own now-calibrated scope — and resolve the DAC1/PA4 conflict that makes it inert in `guest-coldtrace` |
| Sweep / arb / modulation | S-none | — | Ideas catalogued in `docs/ideas/feature_catalog.md`; spec when claimed |

### Platform

| Feature | Stage | Spec | Next |
|---|---|---|---|
| Settings persistence | S2 (commissioned 2026-08-20) | [settings-persistence](platform/settings-persistence.md) | S3: the "bug" was an unthrown build interlock (`c57394c`); next is a power-cycle regression check in the bench script + surfacing `saves_failed` in the UI. Audit P0.4 (silent W25Q write failure) is the open honesty gap |
| Screenshot capture | S1 | — | S2: pull a BMP off the device and look at it (note audit P3: BTN_SAVE currently shows "SAVED #n" without writing anything) |
| Structural hardening | plan exists | [audit 2026-08-20](../structural_audit_2026-08-20.md) | P0 ladder: SPI3 timeout-as-success, W25Q silent-success, bus ownership, torn capture buffers, pre-scheduler queue overflow |
| Rendering path | S3 | — | Display stability guarded on hardware by EXP-22 (`scripts/exp22_stability.py`, 11/11, negative control); graticule question above is the S4 item |
| USB CDC shell | S1 (build-dependent) | — | Enumeration correlates exactly with which FPGA config path the build runs — replicated on a second unit (PR #13); mechanism unestablished — research thread |
| PC export / remote view | S-none | — | Issue #10 ask; wishlist Appendix B has the format lead |

### Modules

| Feature | Stage | Spec | Next |
|---|---|---|---|
| Module loader | S0 | [module-loader](modules/module-loader.md) | S0+ guard: schema validator over the 17 existing procedure files, then embed-and-render one on-device |
| Seed content (17 procedures, 4 trades) | content exists | — | Grows by contribution; `CONTRIBUTING.md` green-lights it |

## The gap list — standard-instrument table stakes we haven't written

Listed so their absence is a published fact, and each is claimable. Every one
needs a spec before code.

- ~~**Trigger modes: auto / normal / single.**~~ **CLAIMED** — spec written,
  [trigger-modes](scope/trigger-modes.md). Wishlist Tier 1 **#1**. The spec's
  finding: what exists is a hardware level register with no control surface, a
  software *display* trigger in the renderer, and a one-field acquisition wait
  policy — the mode, edge, source and level controls a user can press reach the
  fabric nowhere.
- **Pre-trigger capture / horizontal position.** Unknown whether the FPGA's
  ring buffer supports it — a research question before a spec.
- **Acquisition averaging / high-res mode.**
- **Per-channel coupling UI.** PC12 is bench-measured (HIGH=DC); there is no
  user control surface for it.
- **Probe 1x/10x setting.** Pure UI once vertical cal is trusted.
- **Waveform save/recall + CSV export.** Wishlist Tier 2; pairs with the
  PC-link ask (#10).
- **Self-test / user calibration mode.** Design sketch already in
  `docs/roadmap.md` § "User calibration mode"; blocked on a trusted source.

## Community-demand audit — every known ask, mapped

The point of this table is that nothing the community has asked for is
unrepresented: each ask either has a home above, or is explicitly triaged out
with a reason. Sources: [`docs/community_wishlist.md`](../community_wishlist.md)
(evidence-backed, ~1,300 comments mined 2026-06-14) and the GitHub issues.

| Ask | Source | Where it lands |
|---|---|---|
| Reliable triggering (Normal/Single at all timebases) | Wishlist **T1 #1** — most-cited complaint family-wide | [trigger-modes](scope/trigger-modes.md), S0. The flagship. Its S2 criterion answers the complaint with a number: capture success rate in Normal, per measured timebase code. |
| Manual DMM range lock, no auto-revert | Wishlist T1 #2 | Meter table; spec after coexistence S2 |
| Correct Min/Max/Avg semantics | Wishlist T1 #3 | [auto-measurements](scope/auto-measurements.md) |
| Honest resolution (no padded zeros) | Wishlist T1 #4 | House style already (measure-or-refuse); enforced per-badge in auto-measurements S2 |
| Real, labeled, scalable FFT | Wishlist T1 #5 | [fft-live](scope/fft-live.md) |
| Robust screenshot save | Wishlist T1 #6 | Platform table (S2: pull a BMP and look) |
| Scope reads ~4% low vs. true input | Wishlist T2 | Vertical scale row — this is exactly the `SCOPE_CAL_SOURCE_SCALE` absolute-scale constant |
| CH2 desync / second-channel jitter | Wishlist T2 | Live capture CH2 row (TMR13/PA6 bring-up) |
| Meter autorange / decimal instability | Wishlist T2 | Meter coexistence spec S4 + DCV >10 V row |
| X-Y default-state bug; pan/zoom on frozen capture | Wishlist T2 | XY row — carry both into its spec when claimed |
| CSV / waveform export, PC streaming, CAN decode | Wishlist T2 + issue #10 | Gap list (export) + protocol-decoders row + PC-link row |
| Cursor fine/coarse acceleration | Wishlist T2 | Cursors spec (with the units fix) |
| Sans-serif meter font (+28, top UI gripe) | Wishlist T3 | Already shipped — our font system is SF Pro/Menlo. Say so in comparison docs (#12). |
| µF not mF; suppress junk decimals; dBu/dBV | Wishlist T3 | Polish bundle — S4 items on meter/auto-measurements |
| Visual continuity indicator; pitched diode beep | Wishlist T3 | Polish bundle; buzzer is mapped (PB9, TMR4_CH4 — issue #25), so the beep is pure firmware |
| Siggen frequency persists across menu exit | Wishlist T3 | [settings-persistence](platform/settings-persistence.md) — same store |
| Get multimeter working | Issue #15 | [meter-in-the-scope-build](meter/meter-in-the-scope-build.md) |
| Stock-vs-OpenScope comparison | Issue #12 | Not a feature — the README matrix + this catalog largely *are* the honest answer; `docs/stock_vs_openscope.md` needs a refresh against them |
| PC remote view/control | Issue #10 | Platform table (PC export / remote view) |
| 2C53P / sibling support | Issue #21 (closed) | Out of scope for specs; cross-model porting notes live in the wishlist appendix |

Asks that are **hardware-limited** (siggen 3 Vpp/50 kHz cap, shared grounds,
20 mV/div floor, no pF range, 125 MS/s per channel with both on) are triaged
out deliberately — see the wishlist's "Explicitly hardware-limited" section.
Specs must not promise around them.

The wishlist's own "Suggested build order" (triggering → range lock → honest
counts → min/max → screenshots → FFT/XY → export → polish) is the demand-side
ordering; the catalog's "Next" column is the readiness-side one. Where they
disagree, readiness wins the session but demand wins the quarter.

## Relationship to the other planning docs

| Doc | Role | Overlap policy |
|---|---|---|
| `README.md` § Feature maturity | **Status of record** | Specs must agree with it; promotions move both in one commit |
| `docs/community_wishlist.md` | Demand evidence | Cited from specs' Prior art; never restated |
| `docs/ideas/feature_catalog.md` | Brainstorm pool | A feature graduates from there to here by getting a spec |
| `docs/dev_plan_2026-09-22.md` | Session sequencing (current) | Its numbered items name the catalog rows they promote; the 08-13 and 09-12 plans are superseded and kept for the record |
| `docs/roadmap.md` | Design sections only | Its status sections are superseded — see its header |
