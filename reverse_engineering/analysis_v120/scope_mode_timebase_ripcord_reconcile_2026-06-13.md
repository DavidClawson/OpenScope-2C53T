# Reconcile: `scope_mode_timebase` 0x0801D2EC ↔ ripcord `FUN_080212ec` — clean VFP decode, no execution coverage

**Date:** 2026-06-13
**Source:** cross-walk with the ripcord warehouse (`stock_v120`) + its execution-verified
contract ledger (`build/contracts.sqlite`, 19 contracts) and the Renode oracle run log
(`notes/renode-at32-bringup.md`, Runs 5–8).
**Why this matters:** `0x0801D2EC` is booked in `coverage_ledger.csv` as **D3/R1/V0,
FPGA_BLOCKED** with the (correct) re-read that it is *"NOT a timebase config sub-handler —
it is the sample-buffer decimation/resampling engine."* This note **confirms that re-read
statically and resolves the VFP calibration math**, but it does **NOT** promote V — no
verified ripcord contract covers this address. The honest result: ripcord's clean decode
corroborates osc's own corrected understanding and ties the buffer offsets to contract
#19's roll-mode ring, but the Renode oracle never executed this function, so V stays **V0→V1**
(two static methods agree), not V2.

---

## 0. Address crosswalk

| | osc Ghidra project | ripcord warehouse |
|---|---|---|
| load base | `0x08000000` (flat app import) | `0x08004000` (runtime app-slot base) |
| this routine | `scope_mode_timebase @0x0801D2EC` | `FUN_080212ec @0x080212EC` (3752 B) |
| crosswalk | `0x0801D2EC + 0x4000 = 0x080212EC` | entry **exact match** |

**Not a mis-cut.** ripcord's `functions` row for the addr is `FUN_080212ec @0x080212EC`,
size 3752 B — the entry equals the runtime addr exactly. osc carved this at the correct
boundary; it is a genuine standalone function, single caller `FUN_08019e98` (`scope_main_fsm`,
the +0x4000 image of ripcord `0x0801de98`), callee set `{memset 0x080052bc, 0x08029de4,
0x0803bcfc, 0x0803c078, 0x0803e5d4}`. General rule holds: `ripcord_runtime = osc_project + 0x4000`.

---

## 1. CONFIRM — osc's corrected re-read is right; ripcord's clean decode corroborates it (static, V1)

osc already overturned the old "timebase config sub-handler" name (`function_names.md` line
130) and re-tagged it the **sample-buffer decimation/resampling engine**. ripcord's
disassembly + decompile of `FUN_080212ec` confirm that, register-exact. There are **no
`peripheral_xrefs`** on this function — every address is `movw/movt`-built into `r8`
(`0x200000F8`, the state base) or `r0` (`0x080465cc`, a flash LUT), which is exactly the
census-blind pattern contracts #16/#17 flagged. So this is a pure SRAM/flash DSP pass, no
MMIO; the "FPGA_BLOCKED" klass is a mislabel — nothing here touches the FPGA wire.

**Prologue (resolves the math, no corrupt context to fix here):**

```
0x080212F2:  movw  r8, #0xf8 ; movt r8,#0x2000   ; r8 = 0x200000F8  state base
0x080212FA:  ldrb  r0,[r8,#0x2c] ; beq exit       ; gate on state+0x2C (scope_active)
0x08021304:  ldrb  r0,[r8,#0x2e] ; beq exit       ; gate on state+0x2E
0x0802130E:  ldrb  r1,[r8,#2]                      ; r1 = state+0x02 (timebase/decim sel)
0x08021312:  movw  r0,#0x65cc ; movt r0,#0x804     ; r0 = 0x080465CC  flash 16-bit LUT
0x0802131A:  ldrh  r1,[r0, r1, lsl #1]             ; LUT[sel]  -> divisor for this timebase
0x0802131E:  ldrsb r2,[r8,#4]                      ; signed cal from state+0x04
0x08021340:  vmov.f32 s10, #28.0                   ; lower-rail constant 0x1c
            s0/s2/s8 = vldr [pc] cal constants     ; offset / bias / clamp-rail floats
```

**Per-sample transform (unrolled 4-wide over a 0x12d=301-deep buffer, offsets +0x356..+0x359):**

```
gain  = LUT[state+0xDC1] / LUT[sel]                ; vdiv.f32 s12,s12,s4
raw   = (float)buf[+0x356+i]
out   = gain * (raw + s0 - cal) + s2 + cal + s6    ; cal = (float)state+0xDC3
out   = clamp(out, s8, 28.0)                        ; float rails
byte  = (int)out
buf[+0x356+i] = clamp_int(byte, 0x1c, 0xe4)         ; 28..228 hard rails
```

| claim | osc decode | ripcord decode | tier |
|---|---|---|---|
| function is a per-frame sample resample/normalize, not timebase config | re-read in ledger note | confirmed: VFP loop, no MMIO, writes back to sample buffer | static-inferred (both static) |
| operates on the `state+0x356` sample region (0x12d=301 deep) | — | `add r2,r8,r1; ldrb/strb [r2,#0x356..0x359]`, loop stride 4, exit `r1==0x12c` | static-inferred |
| timebase index `state+0x02` selects a 16-bit LUT at flash `0x080465CC` | — | `ldrh [0x080465CC, sel<<1]` used as the resample divisor | static-inferred |
| output hard-clamped to byte range **28..228** (`0x1c..0xe4`) | — | explicit `cmp #0xe4 / #0x1b` integer rails after the float store | static-inferred |
| pre-pass calls `memset` (the verified block-clear primitive) | — | callee `0x080052bc` = contract #1 `memset` (`verified=1`) | the *callee identity* is execution-verified (#1); its use here is static |

**Note the `state+0x356` tie:** contract #19 (`acq_engine_runtime`, verified=1) names
roll-mode rings at **`state+0x356`/`+0x483`**. This resample pass consumes the *same*
`+0x356` ring. That linkage is a real corroboration of the buffer map — but it is a
*static* join (same offset, two functions); the oracle executed the producer
(`acq_engine` mode-3 roll, Run 6), **not** this consumer.

---

## 2. CORRECT — what ripcord fixes vs. osc's prior state

1. **Name/klass:** kill the residual `scope_mode_timebase` / `protocol` / `FPGA_BLOCKED`
   framing. ripcord confirms zero peripheral access — it is a pure SRAM+flash DSP routine.
   Rename target: `scope_sample_resample` (or `scope_decimate_normalize`). klass → `dsp`,
   not `protocol`.
2. **No corrupt context to resolve.** Unlike the `acq_engine` mis-cut (osc `0x08037800`),
   this function has a real prologue, so there were no `unaff_*`/`in_ZR` artifacts to clean.
   ripcord adds the *symbol meanings* (LUT `0x080465CC`, rails 28..228, state offsets), not
   a decode rescue.
3. **The cal constants are float, mirroring the `acq_engine` calibration tail.** Same
   `(raw + offset − cal) * gain + bias` VFP shape contract #19 describes for the post-acq
   calibration — consistent firmware idiom, not a new mechanism.

---

## 3. GAP — R-side (`firmware/src/`) divergences

There is **no clean-room equivalent of this resample/normalize stage** in `firmware/src/`.
Concrete reimplementation items:

1. **Missing horizontal resample/decimation pass entirely.** `drivers/fpga.c`
   (case 2/3/4, ~line 1360) applies only integer `cal = raw + FPGA_ADC_OFFSET; clamp[0,255]`
   and stores to `fpga.ch1_buf/ch2_buf`. `ui/scope_ui.c` then renders that buffer directly
   (`lcd_set_pixel` per column). Stock inserts an intermediate **timebase-indexed VFP
   resample** (`gain = LUT[dc1]/LUT[sel]`) writing normalized bytes back to `state+0x356`
   *before* render. Reimplement this stage; without it horizontal scaling across timebase
   settings will be wrong.
2. **Clamp rails differ: stock uses `[28,228]`, R-side uses `[0,255]`.** The stock hard
   rails (`0x1c..0xe4`) reserve head/foot-room (likely graticule margin / off-screen
   guard). Adopt 28..228, not 0..255, to match stock pixel mapping.
3. **The flash LUT `0x080465CC` (16-bit entries, indexed by `state+0x02` and `state+0xDC1`)
   is not represented.** It is the per-timebase sample-rate / points-per-division table.
   `drivers/fpga_cal_table.h` carries ADC cal, not this resample LUT — extract and add it.

---

## 4. Proposed `coverage_ledger.csv` deltas

| addr | field | from | to | basis |
|---|---|---|---|---|
| `0801d2ec` | name | `scope_mode_timebase` | `scope_sample_resample` | §1/§2 |
| `0801d2ec` | klass | `protocol` (FPGA_BLOCKED) | `dsp` — pure SRAM+flash, zero MMIO (ripcord: no peripheral_xrefs) | §1/§2 |
| `0801d2ec` | V | V0 | **V1** (two static methods agree: osc decode + ripcord clean decode; NOT execution-verified) | §1 |
| `0801d2ec` | note | "FPGA_BLOCKED … reads the two raw SPI3 [truncated]" | "ripcord FUN_080212ec, entry exact (not mis-cut), single caller scope_main_fsm. Per-frame VFP resample/normalize over state+0x356 ring (0x12d deep): out=gain·(raw+off−cal)+bias, gain=LUT[0x080465CC,sel], clamp byte [0x1c,0xe4]. No MMIO. Consumes the same +0x356 roll ring contract #19 (verified) produces. See scope_mode_timebase_ripcord_reconcile" | §1–§3 |

**No V2 promotion.** No verified contract's `[addr_start,addr_end)` covers `134353644`.
The two verified contracts in the neighborhood — #18 (`fpga_bitstream_upload`,
`0x0802A9C4..0x0806E0CF`) and #19 (`acq_engine_runtime`, `0x0803B454..0x0803D008`) — both
sit at **higher** addresses and are **different functions**. The only execution-verified
fact touching this routine is the *identity of its callee* `memset` (contract #1); that
does not verify this function's behavior.

---

## 5. Honest scope

**`new_evidence` from execution is essentially nil for this function.** ripcord's Renode
oracle never ran `FUN_080212ec`; it stops at the acquisition task and the bitstream pump.
What ripcord genuinely contributes here is a **clean second static decode** that (a)
corroborates osc's own corrected re-read (resampler, not timebase config), (b) resolves the
exact VFP math, the `0x080465CC` LUT, and the `[28,228]` rails, and (c) statically links the
`state+0x356` buffer to contract #19's execution-verified roll ring. That is enough to fix
the name/klass and lift V0→V1, and it hands `firmware/src` three concrete gaps (missing
resample stage, wrong clamp rails, missing timebase LUT). It is **not** an execution
promotion, and it does not touch the FPGA secret (this function has no FPGA wire access at
all — the "FPGA_BLOCKED" tag was a misclassification, not a real blocker).

---

## 6. Recipe (per-function template)

1. **crosswalk** `+0x4000` → ripcord runtime addr; check `functions` entry == addr (mis-cut test).
2. **clean decode**: `disasm.py` prologue + `decompiled_c` + `peripheral_xrefs` (empty here ⇒ movw/movt SRAM/flash, no MMIO).
3. **execution evidence**: query `contracts.sqlite` for a `verified=1` contract covering the addr — **none here**; closest verified contracts are different functions at higher addresses.
4. **diff vs R-side**: `firmware/src/drivers/fpga.c` + `ui/scope_ui.c` — found the resample stage absent.
5. **score + write**: V0→V1 (static agreement), name/klass fix, 3 GAP items.

---

## Verification (adversarial, 2026-06-13)

Independent re-check against the ripcord warehouse + contract ledger, defaulting to
skepticism. **Nothing overturned.** The note is well-disciplined — it never claims a
contract covers this address, hedges every claim as static-inferred, and self-limits to V1.

**Checks run and results:**

1. **CROSSWALK — PASS.** `functions` query on `stock_v120` for runtime addr `134353644`
   (`0x80212ec`) returns exactly one row: `FUN_080212ec @0x80212ec, size 3752`. The entry
   equals the runtime addr; `0x0801d2ec + 0x4000 = 0x080212ec` holds. Not a mis-cut.

2. **CONTRACT REALITY — PASS (both contracts real & verified).**
   - Contract **#1** exists, `verified=1`, range `0x80052bc..0x8005300`, claim = memset/
     zero-fill primitive. Callee `0x080052bc` == addr_start exactly. The `calls` table shows
     `FUN_080212ec` really invokes it (sites `0x8021d74`, `0x8021f7c`; `bl #0x80052bc`
     confirmed in raw disasm). The note's framing — *callee identity execution-verified,
     use here static* — is exactly right.
   - Contract **#19** exists, `verified=1`, range `0x803b454..0x803d008`. Its evidence text
     genuinely names the roll-mode ring at `state+0x356`/`+0x483`. The note's
     `state+0x356` linkage is a real static offset-join; the oracle executed the *producer*
     (acq_engine mode-3 roll, Run 6), not this consumer — correctly labeled, not overstated.

3. **V-PROMOTION — no V2 claimed, nothing to demote.** The note explicitly refuses V2: no
   `verified=1` contract's `[addr_start,addr_end)` covers `134353644`; the two neighborhood
   verified contracts (#18, #19) are different functions at higher addresses. The only
   execution-verified fact touching this routine is its callee's *identity* (memset, #1),
   which does not verify this function's behavior. **Defensible ceiling = V1.**

4. **peripheral_xrefs — PASS.** Query confirms **zero** `peripheral_xrefs` for this function
   (both by `function_addr` and by `from_addr` range). Supports the `protocol/FPGA_BLOCKED →
   dsp` reclassification: no MMIO, pure SRAM+flash.

5. **Instruction-level spot-check (disasm.py) — PASS, register-exact.** Every load-bearing
   constant in §1 verified against raw Thumb:
   - `movw r8,#0xf8; movt r8,#0x2000` ⇒ state base `0x200000F8`. ✔
   - `ldrb r1,[r8,#2]` then `ldrh r1,[r0, r1, lsl #1]` ⇒ LUT indexed by **state+0x02**. ✔
     (the `[r8,#0x2c]`/`[r8,#0x2e]` loads are `beq` gates, not the index — note got this right)
   - `movw r0,#0x65cc; movt r0,#0x804` ⇒ flash LUT `0x080465CC`. ✔
   - gain: `ldrb r2,[r8,#0xdc1]; ldrh [r0,r2,lsl#1]` / `vdiv s12,s12,s4` ⇒
     `LUT[state+0xDC1] / LUT[sel]`. ✔
   - cal: `ldrsb r2,[r8,#4]` and `ldrsb r3,[r8,#0xdc3]` ⇒ signed `state+0x04` / `state+0xDC3`. ✔
   - clamp: `cmp #0xe4` (upper 228) and `cmp #0x1b → movs #0x1c` (lower 28) ⇒ hard rails
     `[0x1c,0xe4]`. ✔  Float lower rail `vmov.f32 s10,#28.0`. ✔
   - buffer: `add r2,r8,r1; ldrb/strb [r2,#0x356]`, 4-wide (`adds r1,#4`, `strb [r2,#0x359]`),
     loop exit `cmp r1,#0x12c; beq` ⇒ `state+0x356` ring, `0x12d=301` deep. ✔

**Minor labeling nit (non-substantive, not overturned):** §0 line 29 lists the callee set in
osc-project (+0x4000) address space (`0x08029de4` = ripcord `0x8025de4`+0x4000, etc.) while
listing `memset 0x080052bc` un-shifted. Internally harmless — memset sits below the app slot
and is shared — and the memset identity (the only load-bearing one) is correct. Left as-is.

**FINAL APPROVED: D3 / R1 / V1.** name `scope_sample_resample`, klass `dsp`. The V1 ceiling
is defensible and correct — two independent static decodes agree; no execution covers this
function. No CONFIRM claim was overstated; no body edit required.
