# Reconcile: `scope_measurement_engine` 0x0801f6f8 ↔ ripcord `FUN_080236f8` — clean decode, no contract coverage (new_evidence: limited)

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`, 19 contracts) and the disassembler
(`scripts/analysis/disasm.py`).
**Why this matters:** `0x0801f6f8` is booked in `coverage_ledger.csv` as **D3/R1/V0,
FPGA_BLOCKED** — "the scope auto-measurement engine … scans the per-channel ADC sample
buffers (state[0x12d]-stride source ~300-sa…)". This reconcile asks whether ripcord's
execution oracle can lift that V0. **Answer: not directly.** No verified contract covers
this address — it is a pure-DSP consumer with zero MMIO, so the Renode oracle (which
verifies hardware-boundary transactions) has nothing to bite on here. What ripcord *does*
deliver: a clean, prologue-anchored decode that **confirms** the osc static read of the
buffer geometry and **execution-verifies the upstream producer** (contract #19) that fills
the very buffers this function consumes. Honest scope is stated in §4.

---

## 0. Address crosswalk (NOT a mis-cut)

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `scope_measurement_engine @0x0801f6f8` | `FUN_080236f8 @0x080236f8` |

`0x0801f6f8 + 0x4000 = 0x080236f8`, which is the **exact entry** of ripcord's
`FUN_080236f8` (size 4616 B). The `functions` row covering runtime addr `134362872`
starts at `134362872` — entry == addr, **so osc cut this correctly; it is a standalone
function, not an inner region.** Unlike the `acq_engine` reconcile, there is no mis-cut and
no corrupt-context artifact to repair: the warehouse decode opens with a complete prologue.
General rule still holds: `ripcord_runtime_addr = osc_project_addr + 0x4000`.

---

## 1. CORRECT — minor: resolve the osc decode against the clean prologue

osc had this at D3 (register/algorithm level) already, so there is little to correct. The
warehouse prologue confirms the frame and pins the constants osc inferred:

```
0x080236F8:  push.w {r4-r8, sb, sl, fp, lr}   ; 9-reg frame
0x080236FE:  vpush  {d8, d9, d10, d11}          ; PRESERVES callee-saved VFP regs across bl calls (NOT inline float — see §6)
0x08023704:  movw   r8, #0xf8
0x08023708:  movt   r8, #0x2000                  ; r8 = 0x200000F8  → global state-struct base
0x0802370C:  ldrb.w r4, [r8, #0x2d]              ; r4 = state[0x2D] = range_index
0x08023714:  ... rsb r0,r4,#0x1c ; smmul/.. mod-3 ; index = (0x1C - range_index) % 3
0x08023730:  movt   r1, #0x804
0x08023734:  ldrh.w r0, [r1, r0, lsl #1]         ; r1 = 0x0804BCE0 → flash u16 LUT (timebase/sample-rate)
```

| osc symbol / inference | warehouse value | meaning |
|---|---|---|
| state base | `0x200000F8` (`r8`) | global state-struct base (same base as acq engine §19) |
| `range_index` | `state[0x2D]` | timebase/range selector, drives the LUT index |
| sample-rate table | flash `0x0804BCE0` (u16[]) | indexed by `(0x1C − range_index) mod 3`; supplies the `sample_rate` term |
| per-channel stride | `0x12D` (301) | `smlabb r0,r8,#0x12d,lr` / inner `mla r0,r8,#0x12d` — confirms osc's "state[0x12d]-stride source" |
| source sample buffer | `state + 0x356`, byte reads | `ldrb [r0,#0x356]` per channel — the roll/sample buffer (contract #19 names `state+0x356` as the roll ring) |
| frequency divide | `0x3B9ACA00 / period` | `0x3B9ACA00 = 1_000_000_000`; freq = 1e9 / period_ns (Hz from a ns period) |
| rise/fall window | `0x2710 = 10000`, `[r6,#0x58]/#0x60` | 10000-unit span and start/width result slots |

**Ledger action:** osc note "state[0x12d]-stride source ~300-sa…" → **confirmed exact**:
stride is `0x12D = 301`, channel-major, sourced from `state+0x356`, gated by
`(state[0x15] & 0xF)` (active-channel low nibble) vs `state[0x15] >> 4` (channel count).
Keep D3. No corrupt context existed to fix — this is a clean correctly-cut function.

---

## 2. CONFIRM — independent agreement; what is and is NOT execution-verified

**No contract covers `0x080236f8`.** I checked all 19 rows in `build/contracts.sqlite`: the
two with a real `[addr_start,addr_end)` near acquisition are #18 (`fpga_bitstream_upload`,
134392260–134668495 — covers `0x0802A9C4`, the upload pump) and #19 (`acq_engine_runtime`,
134460500–134467592 = `0x0803B454`–`0x0803D008`). Runtime addr **134362872 is below both
ranges** → `contract_id = null`, `contract_verified = false`. The Renode oracle never ran
this function and could not: it has **zero MMIO** (`peripheral_xrefs` returns 0 rows), so
there is no hardware boundary for the oracle to verify.

| claim | osc decode | independent corroboration | tier |
|---|---|---|---|
| stride 301 (`0x12D`), source `state+0x356`, channel-major | yes | ripcord warehouse decode (static) | **static-inferred** (two static reads agree → V1) |
| sample_rate from flash LUT `0x0804BCE0[(0x1C−range)%3]` | yes | ripcord prologue (static) | **static-inferred** (V1) |
| freq = 1e9/period_ns | yes | ripcord `0x3B9ACA00` literal (static) | **static-inferred** (V1) |
| the buffers it consumes (`state+0x356`, `+0x5B0`) are real and filled by acquisition | implied | **contract #19, verified=1: acq engine writes `state+0x356`/`+0x483` (roll) and `+0x5B0`/`+0x9B0` (burst)** | **execution-verified — but of the PRODUCER, not this function** |

The only execution-verified fact in play is **upstream**: contract #19 (Runs 5–8) proved
the acquisition engine *writes* `state+0x356` and the burst buffers. That makes the
*existence and address* of this function's input buffers execution-verified. It does **not**
verify any computation `scope_measurement_engine` performs on them — the math here
(min/max/Vpp/Vrms, zero-crossing frequency, duty, rise/fall) is **static-inferred only**.

**Ledger action:** the input-buffer geometry (`state+0x356`, 301-stride) may move
**V0 → V1** on independent two-method static agreement, plus a footnote that the *producer*
of those buffers is execution-verified (#19). The measurement **algorithms stay V0** — no
execution touched them. Do **not** mark this row V2.

---

## 3. GAP — R-side corrections for `firmware/src/tasks/measurement.c`

`measurement.c::measurement_compute()` is the clean-room equivalent. It is algorithmically
in the same family but diverges from stock in concrete, reimplement-faithfully ways:

1. **Sample type / scale is wrong-shaped.** R uses `int16_t samples[]` with
   `VOLTAGE_SCALE = 3.3/32768` (a ±32k 16-bit signed model). Stock reads **8-bit unsigned
   bytes** (`ldrb`) from `state+0x356` with a 301-byte stride, post-cal already clamped to
   `[0,255]` by the acq engine (#19 cal tail). The measurement engine's input is the
   byte-domain ADC buffer, not a 16-bit signed array — the whole VOLTAGE_SCALE and
   smin/smax type need to be byte-domain.

2. **Frequency method differs.** R computes `crossings * sample_rate / num_samples`
   (zero-crossing count over the window). Stock computes a **period in nanoseconds then
   `frequency = 1e9 / period_ns`** (the `0x3B9ACA00` divide), where the period derives from
   the flash sample-rate LUT (`0x0804BCE0`) keyed on `range_index`, not from a passed-in
   `sample_rate` float. Reimplement the LUT-driven timebase path.

3. **No timebase LUT in R.** R takes `sample_rate` as a parameter; stock *derives* it
   internally: `idx = (0x1C − state[0x2D]) mod 3`, `rate = u16LUT[0x0804BCE0 + idx]`. The
   `(0x1C − range) mod 3` folding and the flash table are missing from the clean-room and
   must be ported (the table itself is a flash-resident constant array to extract).

4. **Channel iteration / gating absent.** Stock loops channels with low-nibble/high-nibble
   gating of `state[0x15]` (`& 0xF` active mask vs `>> 4` count) and channel-major 301
   stride. R's `measurement_compute` is single-buffer, single-channel — the multi-channel
   dispatch and the `state[0x15]` enable encoding are not modeled.

5. **Rise/fall uses a different reference span.** R interpolates 10%–90% of `(smax−smin)`.
   Stock carries a `0x2710` (10000) span and writes start/width to `[result+0x58]/[+0x60]`
   in a fixed-point form — verify the units (the `0x2710` suggests a 10000-tick or 0.01%
   granularity domain), not float seconds.

None of these are blockers; they are numerator-growth items so the clean-room produces
amplitude- and frequency-correct numbers matching stock.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0801f6f8` | note | "state[0x12d]-stride source ~300-sa…" | "stride CONFIRMED 0x12D=301, channel-major from state+0x356, gated by state[0x15] nibbles; sample_rate from flash LUT 0x0804BCE0[(0x1C−range_index)%3]; freq=1e9/period_ns (0x3B9ACA00). Clean correctly-cut fn (ripcord FUN_080236f8, no corrupt context). See scope_measurement_engine_ripcord_reconcile" | §1 |
| `0801f6f8` | V (input-buffer geometry only) | V0 | **V1** (two-method static agreement; PRODUCER of those buffers is execution-verified, contract #19) | §2 |
| `0801f6f8` | V (measurement algorithms) | V0 | **V0 (unchanged)** — no contract covers this addr; zero MMIO; oracle never ran it | §2 |
| `0801f6f8` | klass note | FPGA_BLOCKED | re-tag: this is **not** FPGA-blocked — it is a pure-DSP consumer; it is "oracle-inapplicable" (no MMIO to verify). Verification path is golden-vector replay against the acq buffers, not Renode MMIO. | §2 |

**Honest scope:** ripcord adds *modest* new evidence here, and the honest verdict is that
it does **not** unblock V2. The function has no hardware boundary, so the execution oracle
that lifted the acq-engine row cannot apply. What ripcord contributes: (a) a clean
prologue-anchored decode that confirms osc's static read and pins the exact constants
(`0x12D` stride, `0x0804BCE0` LUT, `0x3B9ACA00`/1e9 frequency divide); (b) execution-
verified provenance for the *upstream producer* of this function's input buffers (#19),
which raises confidence that the inputs are real without verifying the math; (c) five
concrete `measurement.c` reimplementation corrections. The correct verification route for
this row is **golden-vector replay** — feed a known sample buffer into an emulated
`FUN_080236f8` and diff the result struct — *not* the MMIO oracle. That re-tag (FPGA_BLOCKED
→ oracle-inapplicable / golden-vector) is itself the most useful output of this reconcile.

---

## 5. Recipe (per-function template, unchanged)

1. **crosswalk** osc addr `+0x4000` → ripcord runtime addr; confirm entry==addr (mis-cut check).
2. **clean decode**: `disasm.py` prologue + `decompiled_c` + `peripheral_xrefs` (here: 0 rows → pure DSP).
3. **execution evidence**: query `contracts.sqlite` for a `verified=1` contract covering the addr (here: none; nearest #18/#19 cover producers).
4. **diff vs R-side**: `firmware/src/tasks/measurement.c` → CONFIRM / CORRECT / GAP.
5. **score + write**: propose D/R/V deltas; be explicit when the oracle is inapplicable.

---

## 6. Verification (adversarial, 2026-06-13)

Re-ran every load-bearing check against the warehouse rather than trusting the draft.

**Crosswalk — HELD.** `scripts/query` over `functions` for runtime addr `134362872`
returns exactly one row: `FUN_080236f8 @0x80236f8`, size 4616, **entry == addr**. Not a
mis-cut. `crosswalk_ok = true`.

**Contract reality — HELD, and the draft's restraint is correct.** The draft cites no
covering contract (`contract_id: null`). Verified independently: contract **#19**
(`verified=1`) covers `0x803b454`–`0x803d008` (runtime `134460500`–`134467592`); runtime
addr `134362872` is **below** that range, so #19 does **not** cover this function. The
draft's use of #19 strictly as the *execution-verified PRODUCER* of the input buffers
(`state+0x356` roll ring, `+0x5B0` burst) is accurate — #19's evidence text literally names
roll rings `state+0x356/+0x483` and burst `state+0x5B0/+0x9B0`. No contract id was
hallucinated; no V2 was claimed.

**Instruction-level CONFIRMs — all verified present in the binary** (`disasm.py`):
`movw r8,#0xf8 / movt #0x2000` (state base 0x200000F8); `ldrb [r8,#0x2d]` (range_index);
`rsb r0,r4,#0x1c` + `smmul`/magic `0x55555556` mod-3 fold; `movw #0xbce0 / movt #0x804`
+ `ldrh [r1,r0,lsl #1]` (flash u16 LUT 0x0804BCE0); `movw #0x12d` + `smlabb`/`mla` (301
channel-major stride); many `ldrb [...,#0x356]` (byte-domain source); `ldrb [r8,#0x15]`
(channel gate); `movw #0x2710` + `strd [r6,#0x58]`/`[r6,#0x60]` (rise/fall fixed-point
window). Frequency divisor confirmed in `decompiled_c`:
`*(int *)(&DAT_20000148 + iVar11) = 1000000000 / (int)uVar21` — i.e. `1e9 / period`,
**integer** divide (literal 0x3B9ACA00 = decimal 1000000000).

**OVERTURNED — one overstatement (framing, not a V-score).** The note glossed the
`vpush {d8-d11}` prologue as "saves 4 VFP doubles -> heavy float math." Checked: the
function body contains **zero** VFP arithmetic instructions (no
`vadd/vsub/vmul/vdiv/vsqrt/vcvt/vcmp` — grep over the full 4616-byte body returns 0). The
VFP pushes preserve callee-saved registers across the `bl` calls (`0x8042124`, `0x80425da`);
the float math (Vrms/scale) is delegated to those callees, not done inline. This function is
integer-domain (71 integer `cmp` for min/max reductions, all-`ldrb` loads, integer
`1e9/period`). Corrected at §1 line. This does **not** change any tier — no claim rested on
inline float — but it tightens the byte-domain story (GAP #1) and removes a misleading cue.
The "auto-measurement engine (computes from samples)" claim itself stands as
**static-inferred**, consistent with 301-stride `ldrb` reads of the #19-verified
`state+0x356` ring + min/max + `1e9/period`. It updates rather than contradicts the
*unverified* hypothesis in contract #15 (measurements FPGA-computed over USART2): at least
the min/max/freq reductions are demonstrably MCU-side here. Left static; not promoted.

**FINAL approved scoring (unchanged from draft — it was already defensible):**
- **D3** — register/algorithm-level decode, constant-pinned by the ripcord prologue.
- **R1** — `measurement.c::measurement_compute` exists, diverges on the 5 concrete §3 items.
- **V1** for input-buffer geometry only (two-method static agreement + execution-verified
  PRODUCER #19); **V0** unchanged for the measurement algorithms (no covering contract, zero
  MMIO, oracle never ran this code). **NOT V2.** The FPGA_BLOCKED -> oracle-inapplicable /
  golden-vector re-tag stands.
