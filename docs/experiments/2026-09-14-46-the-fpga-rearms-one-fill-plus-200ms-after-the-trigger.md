# EXP-46 — PC0 marks the trigger, and the FPGA will not re-arm until one fill + ~200 ms later

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `16ed8cf` (`fpga postedge`, `fpga acqbr` knobs over the priming build)
- **Scripts:** `scripts/exp46_postedge_clock.py`, plus three shell sweeps (logs `exp46b/c/d.log`)
- **Status:** **CONFIRMED** — six predicted brackets, six hits

## 1. Problem

EXP-45 left two candidates for why the task's pair, issued the instant PC0
edges, never arms the next capture while a shell read seconds later always
does: the delay between the edge and the read, or the SPI clock (/2 vs /256).

## 2. Hypothesis

If it is the delay, NORMAL sustains itself once `fpga postedge` exceeds
some threshold and the clock knob changes nothing. If it is the clock,
`fpga acqbr 7` (/256) sustains NORMAL at post-edge 0. The threshold, if
any, either scales with the sample rate (a sample count) or does not (a
timer). Predictions for the later brackets are written in the run logs
before the arms were taken.

## 3. Procedure

JDS6600 201.2 Hz 3 Vpp → CH1, range 5, level 0. NORMAL arm = select mode,
one shell prime, then four `spi3 frame` records; "sustains" = generation
advancing with one edge per commit. AUTO arm = edges per pair over 6–8 s.

## 4. Control

Post-edge 0 / clock unset reproduced the freeze in every sweep; every knob
read back; AUTO control arm at 0.50 edges/pair before each comparison.

## 5. Results

**Sweep 1, 0x10 (fill 82 ms):**

| arm | NORMAL | edges / OK |
|---|---|---|
| postedge 0 / 5 / 20 / 100 ms | FROZEN | +0 |
| **postedge 300 ms** | **advancing (8–10/record)** | **+27 / +27** |
| acqbr 7 (/256), postedge 0 | FROZEN | +0 |
| acqbr 0 (/2), postedge 0 | FROZEN | +0 |
| AUTO, postedge 0 | — | 0.50/pair, 5.3 pairs/s |
| **AUTO, postedge 300** | — | **1.00/pair**, 3.3 pairs/s |

**Sweep 2, is it our 300 ms NORMAL constant?** AUTO with `fpga autowait 1000`:
postedge 0 / 150 / 260 → 0.50; **280 → 1.00** (3.5 pairs/s); 300 → 1.00;
400 → 1.00. The threshold is 260–280 ms with no 300 ms constant anywhere in
the loop → it is the FPGA.

**Sweep 3, does it scale?** NORMAL sustain vs post-edge at three rates:

| timebase | fill | 120 | 150 | 180 | 220 | 260 | 280 | 300 |
|---|---|---|---|---|---|---|---|---|
| 0x0F | 41 ms | ✗ | ✗ | ✗ | ✗ | **✓** | | |
| 0x10 | 82 ms | ✗ | ✗ | ✗ | ✗ | ✗ | **✓** | ✓ |
| 0x11 | 205 ms | ✗ | ✗ | ✗ | ✗ | ✗ | | ✗ |
| 0x0E | 21 ms | | | | | | | ✓ |

Neither a constant (0x11 fails at 300 where 0x0F passes at 260) nor a
multiple of the fill (0x0F passes at 6.3 fills, 0x10 at 3.4, 0x11 fails at
1.5). **One fill + ~190 ms** fits all four.

**Sweep 4, predictions from that model, written before the run:**

| timebase | fill | predicted frozen | predicted sustains |
|---|---|---|---|
| 0x11 | 205 | 380 (fill+175) → **frozen ✓** | 420 (fill+215) → **sustains ✓** (15/15) |
| 0x0E | 21 | 200 (fill+179) → **frozen ✓** | 230 (fill+209) → **sustains ✓** (24/24) |
| 0x12 | 410 | 560 (fill+150) → **frozen ✓** | 640 (fill+230) → **sustains ✓** (9/9) |

Bracket for the constant across all five rates: **180 ms < C ≤ 209 ms**.

## 6. Blind spots

- The constant's origin is unknown: ~190 ms is not a sample count at any
  rate we know (it does not scale) and not one of our loop delays. A
  fixed-clock counter in the design is the obvious shape; the netlist
  (`gw1n2-apicula`) could say.
- Cycle time is now ≈ fill + 200 ms + read, i.e. ~3.5 records/s at 0x10
  and ~1.4/s at 0x12. Stock's June capture shows 04/05 pairs every ~29 ms,
  so stock is not honouring this bracket — either it reads across the
  seam (its trace would carry it) or it drives the engine some other way.
  Not resolved here.
- Edge → read latency was inferred (pairs/s 3.5 at postedge 280 → the edge
  lands ~5 ms after the read), not timed.
- One drive frequency; the edge's own timing relative to the trigger
  crossing is not characterised.

## 7. Conclusion

- **Established:** PC0 announces the **trigger**, and the FPGA refuses to
  arm a new capture until **one 1024-sample fill plus ~200 ms** after it. A
  read inside that bracket is ignored (no capture, no next edge); a read
  after it arms, and the next edge follows within ~5 ms at this drive.
  This is the mechanism behind EXP-30's second deadlock, EXP-43/44's
  frozen NORMAL, and gated AUTO's exact 0.50 edges per pair.
- **Excluded:** the SPI clock; the inter-read gap; the reg 01 / reg 08
  writes; our own 300 ms NORMAL constant.
- **Shipped as default (`fpga_acq_post_edge_get`):** post-edge delay =
  fill + 230 ms derived from the timebase in force, 600 ms for unmeasured
  rates; AUTO fallback budget raised to 3 × fill + 230 so a fallback read
  cannot land inside the bracket. Both remain overridable.
- **Follow-up:** EXP-47 acceptance of those defaults on a fresh boot across
  0x0E/0x10/0x11/0x12, SINGLE included; then the stock cadence question.
