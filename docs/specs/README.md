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
| Live capture CH2 | S1 | — | TMR13/PA6 offset bring-up (`guest-coldtrace-ch2`), then re-run the attenuator ladder |
| Vertical scale | S3 | — | S4 blocked on a calibrated source (`SCOPE_CAL_SOURCE_SCALE`) — Help Wanted #3b |
| Horizontal scale | S3 | — | Codes 0x09–0x0C need a faster source; 0x06–0x08 need the narrow-field/roll-mode hypothesis tested |
| Freq badge | S3 | — | S4: refusal states could say *why* (`torn` vs `no peak`) |
| Auto-measurements (real units) | S1 | [auto-measurements](scope/auto-measurements.md) | S2: badge volts/seconds validated against a bench-driven signal |
| FFT + waterfall on live data | S0 | [fft-live](scope/fft-live.md) | S1: consume the live acq buffer in `guest-coldtrace` |
| Trigger level | S1 | — | S2: measure level-vs-ADC-code transfer on the bench |
| Cursors | S1 | *needed* | Units are fixed constants, not derived from the measured tables — same defect class the badges just escaped |
| Autofit vs. measured graticule | S1 | *needed* | Decision pending: the vertical graticule does not mean the volts/div the status bar prints |
| Math channels | S0 | — | After auto-measurements S2 (same input plumbing) |
| XY / roll / trend / mask | S0 | — | Unclaimed; each needs a spec before work starts |
| Protocol decoders | S0 | — | Needs a spec: capture-depth and sample-rate reality check first (132 host tests already exist) |
| Bode plot | S0 | — | Blocked on siggen/scope coexistence (shared DAC1) |

### Meter

| Feature | Stage | Spec | Next |
|---|---|---|---|
| Multimeter in the scope build | S1 (dead in coldtrace) | [meter-in-the-scope-build](meter/meter-in-the-scope-build.md) | S1-in-coldtrace: find what the FPGA config kills, revive USART2 data |
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
| Rendering path | S1 | — | Compositor + redraw gate landed 2026-08-19; S3 guard exists for the gate; graticule question above is the S4 item |
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

- **Trigger modes: auto / normal / single.** Wishlist Tier 1 **#1** — the
  single most-cited complaint about stock across the whole model family
  ("Normal trigger… mostly miss the trigger events"). We have a trigger
  *level*; we have no trigger *modes*. The flagship differentiator if done
  right, and the hardest: needs the engine's re-arm semantics understood.
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
| Reliable triggering (Normal/Single at all timebases) | Wishlist **T1 #1** — most-cited complaint family-wide | Gap list → trigger-modes spec. The flagship. |
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
| `docs/dev_plan_2026-08-13.md` | Session sequencing (partly stale) | Its lettered items become specs as they're picked up (D1 → module-loader) |
| `docs/roadmap.md` | Design sections only | Its status sections are superseded — see its header |
