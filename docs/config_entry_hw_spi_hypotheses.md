# Why does HW-SPI FPGA config fail on the 2C53T? — hypothesis board

- **Created:** 2026-08-25
- **Status:** OPEN. Living document — agents append findings under each hypothesis.
- **Owner question:** the bit-bang GPIO loader configures the GW1N cold and reliably
  (`CFG:0003F460 D1`); the *same payload* over the SPI3 peripheral hits the wall
  (`CFG:00039020 D0`, DONE_FINAL clear, edit-mode never engages, no error bits).
  Why?

This board exists so the search survives interruption. If you are picking this up
cold, read §1 (the reframe) first — it is the single most important thing here.

---

## 1. The reframe — the problem is smaller than it looks

There are **three** data points, not two:

| Path | Transport | Result |
|---|---|---|
| **Stock firmware** | HW SPI3 peripheral, gapless, /2 (60 MHz) | ✅ configures every boot |
| **Our bit-bang** (`coldtrace` / `FPGA_CONFIG_B`) | GPIO, slow, gapped | ✅ `CFG:0003F460 D1` |
| **Our HW-SPI** (`FPGA_CONFIG_A`) | HW SPI3 peripheral | ❌ the wall `00039020 D0` |

**Stock succeeds over the exact peripheral we are failing on.** Therefore the
"AF-drive vs GPIO-drive electrical character" leg of the transport bisect is
largely a red herring — AF drive works on this die. The real question is narrow:

> **What does *our* SPI3 config path do that *stock's* SPI3 config path does not?**

That is a software diff, not a hardware mystery. Every hypothesis below is a
candidate answer to that one question.

---

## 2. Ground truth (code, as of 2026-08-25)

- `spi3_xfer()` (fpga.c:374) — polled: wait TDBE → write DT → wait RDBF → read DT.
  One byte at a time, drains RX each byte.
- `fpga_spi3_config_sequence()` (fpga.c:4117) — the FAILING path (`FPGA_CONFIG_A`).
  Prelude at fpga.c:4283+ : bare CS pulse → `05 00` → `12 00` → `15 00`, each in
  its own CS-LOW frame (mode 0). `prelude_gap_ms` between frames.
- `fpga_bitbang_config_sequence()` (fpga.c:3890) — the WORKING path.
  `FPGA_CONFIG_B_FAITHFUL`: `100ms → 05 00 → 100ms → 12 00 → 100ms → 15 00` then
  `0x3B` + payload in one CS-LOW frame.
- `bb_cmd16()` (fpga.c:3881) — bit-bang command frame: **`bb_xfer(0)` (8 SCK edges
  with CS HIGH)** → CS LOW → 2 bytes → CS HIGH.
- SPI3 register setup (fpga.c:4852): CPHA=1, CPOL=1 (mode 3), MSTEN, MSB-first,
  software CS, /2, and **`CTRL2 = 0x03` (RXDMAEN+TXDMAEN) set — but we POLL.**
- Both paths: mode 3, MSB-first, PB3/4/5 + PB6 CS, PB4/MISO floating.

---

## 3. Hypotheses

Each: claim · why it's live · test (falsifier) · cost · status/findings.

### H1 — We reach CONFIG_ENABLE *too early* (POR/auto-boot settle window)
- **Claim:** the GW1N needs a fixed time after rail-up to finish auto-booting the
  NV design before it will accept SSPI config entry; our HW-SPI path fires `0x15`
  far sooner after cold rail-up than stock does.
- **Why live:** `POR(16)=1` with `DONE_FINAL(13)=0` reads like a part still early
  in POR. Stock runs ~15 KB of `master_init` before its `0x15`. **The working
  bit-bang path has 300 ms of built-in settle (3×100 ms) right before `0x15`; the
  lean HW-SPI path does not — an unisolated confound.** Exp B2 proved *late* is
  fine for stock; nobody has bounded *early* on our path.
- **Test (firmware, cheap):** add a fixed settle before the bare-CS pulse, swept
  `0 / 500 ms / 2 s / 5 s`, on a genuinely cold FPGA (unplug/replug).
  **Falsifier:** 5 s still → `00039020` kills it. **Win:** crosses the wall at any
  delay → transport innocent, ~10-line fix.
- **Blind spot:** must time from FPGA *rail-up*, not MCU boot.
- **Findings (2026-08-25, agent):** ⚠ **PREMISE INVERTED — H1 is effectively
  refuted by code inspection.** The failing Build A HW-SPI path DOES have ~300 ms
  pre-`0x15` settle (`prelude_gap_ms=100` ×3 real `systick_delay_ms`). The working
  `guest-coldtrace` uses `FPGA_CONFIG_B` *without* `FPGA_CONFIG_B_FAITHFUL` → the
  bit-bang `#else` branch with **~0 ms** inter-frame settle. The 3×100 ms bit-bang
  the premise cited is `guest-coldtrace-faithful`, a *different, non-default* build.
  Both run at the same early offset in `fpga_init`, so time-from-rail-up is ~equal.
  **More settle → failure, not success.** A `FPGA_HW_CONFIG_SETTLE_MS` knob +
  `guest-configA-settle SETTLE=` target was still written for one decisive
  multi-second shot (predicted: walls). Detail: `scratchpad/config_entry_findings_H1H2.md`.

### H2 — The framing rule imposed on HW-SPI is inverted (CS-high dummy asymmetry)
- **Claim:** the thing removed from the HW-SPI path as "harmful" is *present in the
  working bit-bang path*, so the CS-gating model may be backwards.
- **Why live:** HW-SPI comment (fpga.c:4195) removed CS-HIGH flush clocks as "stray
  edges that desync the parser" — but **`bb_cmd16` clocks 8 SCK edges with CS HIGH
  before every command frame** (fpga.c:3883) and *succeeds*. Asymmetric between
  success and failure, untested. Classic "excluded something the instrument
  couldn't isolate."
- **Test (firmware, cheap):** make the HW-SPI prelude mirror `bb_cmd16` exactly,
  incl. the CS-HIGH dummy byte before each frame. Also bench the existing
  `prelude_frame_mode` combined/merge variants on a cold part if not already done.
  **Falsifier:** adding dummy clocks changes nothing → model stands.
- **Findings (2026-08-25, agent):** ✅ **CONFIRMED and correctly stated — now the
  strongest firmware-only lead.** `bb_cmd16` (fpga.c:3881) clocks `bb_xfer(0)` = 8
  SCK edges with CS HIGH before every frame; the HW-SPI path deleted exactly this
  (fpga.c:4195 "removed entirely"). Knob `FPGA_HW_CS_DUMMY` written (inserts
  `spi3_xfer(0x00)` before each prelude `SPI3_CS_ASSERT` — exact mirror, since the
  peripheral clocks regardless of software CS) + `guest-configA-csdummy` target,
  default 0. `prelude_frame_mode` 1/2 have NO Makefile target (reachable only via
  boot opts / CDC shell) — flagged, not built. Detail:
  `scratchpad/config_entry_findings_H1H2.md`.
- **BENCH RESULT (EXP-29, 2026-08-25): ❌ REFUTED.** `guest-configA-csdummy` on a
  cold FPGA → `CFG:00039020 D0 E0 A0` — bit-identical wall. Re-adding the dummy on
  05/12/15 to match both working transports did NOT break config entry. **The
  doubly-converged static lead failed at the bench** — the strongest single lesson
  of this whole exercise: matching stock on the byte stream AND the CS/dummy
  framing is not sufficient. Blind spot: no LA confirmation the dummy edges reached
  the wire (code path executed; edges unmeasured). Knob left in place (default 0).
  See `docs/experiments/2026-08-25-29-hw-spi-cs-high-dummy.md`.

### H3 — Polled transfer ≠ stock's DMA-fed transfer
- **Claim:** stock feeds SPI3 via DMA; we set the DMA-enable bits (`CTRL2=0x03`) but
  drive DT by polling. A config command we *believe* went out may be mangled by a
  FIFO/timing quirk a DMA-paced stream avoids; reads survive because we anchor them.
- **Why live:** our own comment (fpga.c:4868) speculates the FPGA "may depend on
  seeing these DMA request signals as part of its SPI slave handshake" — set but
  never tested. Polled `TDBE→write→RDBF→read` per byte is a different cadence than a
  DMA burst.
- **Test (firmware, medium):** drive the prelude (and/or upload) via DMA exactly as
  stock does. **Win:** DMA configures where polled doesn't → transfer *mechanism*
  is the differentiator.
- **Findings (2026-08-25, agent):** ❌ **REFUTED — de-prioritized.** Stock's FPGA
  config upload is **fully POLLED, not DMA** (`master_init_phase2.c:550-571`,
  `spi3_tx_rx` loop; its summary says verbatim "NO DMA — fully polled"). The
  wall-relevant prelude `05/12/15` is the same polled routine (phase2.c:506-531).
  **Register correction:** stock DOES set `CTRL2=0x03` (opcodes at flash `0x080265E8`
  `orr #2; orr #1` on CR2 `0x40003C04` — decompile mislabels them RXNEIE/TXEIE), so
  our `ctrl2=0x03` genuinely matches stock and **the "FPGA needs DMA request signals"
  speculation (fpga.c:4868) is MOOT** — CTRL2=0x03 asserts those requests whether or
  not a channel consumes them. Building a DMA upload would *diverge* from stock, not
  converge, and the wall is upstream of the payload anyway. Stock's real DMA serves
  USART2-RX (meter) + a mem-to-mem flash read, not the FPGA. Detail:
  `scratchpad/config_entry_findings_H3.md`.

### H4 — Stock decompile diff (the instrument, not a hypothesis) ★ do this first
- **Claim:** because stock succeeds over the same peripheral, an instruction-level
  diff of stock's SPI3 config path (`master_init` around `0x0802DA42`) vs our
  `fpga_spi3_config_sequence` holds transport constant and isolates only "how we
  drive it." Confirms/kills H1/H2/H3/H5 without touching the bench.
- **Extract from Ghidra / `decompiled_2C53T.c` / analysis_v120:**
  1. delay/settle between rail-up (or FPGA-power) and stock's `0x15` → **H1**
  2. stock's CS toggle pattern per command byte, dummy clocks included → **H2**
  3. DMA vs polled upload; SPI3 `CTRL2` / DMA channel setup → **H3**
  4. SPI3 idle levels / pin-AF-enable ordering before first frame → **H5**
  5. any SPI3 register write stock makes that we don't (catch-all)
- **Cost:** zero hardware.
- **Findings (2026-08-25, agent):** ✅ **DONE — converges on H2.** Decoded from
  stock machine code (flash `0x0802D5E8`–`0x0802DB28`; phase decompiles mislabel
  05/12/15 and were NOT trusted). Stock's per-command frame is: `~100ms SysTick →
  CS HIGH → clock 0x00 dummy (CS HIGH) → CS LOW → cmd → 0x00 → CS HIGH`, for EVERY
  frame (05@`0x0802D7D2`, 12@`0x0802D90A`, 15@`0x0802DA42`, 3B@`0x0802DB06`).
  Item-by-item: **(1) H1 settle — NO DIFFERENCE** (stock's ~100 ms SysTick waits ≈
  our `prelude_gap_ms`); **(2) H2 CS-high dummy — CONFIRMED DIFFERENCE, the
  standout** (both working transports clock it, the failing HW path removed exactly
  it); **(3) H3 DMA — REFUTED** (polled unrolled loop; `CTRL2=0x03` matched at
  `0x0802D5E8`); **(4) H5 — inconclusive here, low priority**; **(5) catch-all —
  NOTHING** (only CTRL1 elsewhere + CTRL2=0x03 matched; CS via PB6 matched; no
  hidden status write, no reset toggle, prelude bytes exactly 05·12·15·3B, 0x05
  present). Raw disasm: `scratchpad/stock_{pre_config,config_disasm,upload_loop}.txt`;
  detail: `scratchpad/config_entry_findings_H4.md`.

### H5 — Pin-mode-switch glitch at AF enable
- **Claim:** the bit-bang path *pre-loads* idle levels (SCK HIGH, CS HIGH, MOSI LOW)
  before enabling "to avoid a config glitch" (fpga.c:3904). If the HW-SPI path
  switches PB3/4/5 to AF or asserts SPE while SCK/CS idle is undefined, the first
  `0x15` frame can begin on a glitch edge — one stray edge eats the command; reads
  recover (we retry/anchor), a one-shot config command doesn't.
- **Test (firmware):** guarantee SCK-high / CS-high / MOSI-defined idle before SPE
  and before the first CS assertion, matching bit-bang. **Test (LA):** wire-diff
  bit-bang-success vs HW-SPI-fail at the `0x15` frame; look for a glitch at the mode
  switch and whether MISO/SO ACKs differently.
- **Findings (2026-08-25, agent):** **WEAK cause — deprioritized below H1/H2/H3.**
  Real infidelities vs bit-bang exist: PB3/PB5 go to AF (fpga.c:4657/4683) *before*
  CPOL is written (4852) and before SPE (4873); PB6/CS momentarily glitches LOW at
  `gpio_init` 4690 (ODR never pre-set) until 4693. Bit-bang avoids all of it by
  pre-writing ODR before flipping pins (fpga.c:3905-3915). **But every glitch window
  is timed wrong to hurt config entry:** the AF-before-CPOL window and CS-LOW blip
  both happen in `fpga_init`, long before the prelude clocks; the one genuine
  stray-edge source (`spi3_set_br` toggling SPE 0→1 *inside* an open CS frame,
  fpga.c:805-807) lands in the **upload**, downstream of `0x15` (Exp N: payload
  irrelevant); and the CONFIG_ENABLE frame itself (fpga.c:4337-4353) is clean
  CS_ASSERT→xfer→xfer→CS_DEASSERT with SPE stable. Fix written behind
  `FPGA_HW_IDLE_PRELOAD` + `guest-configA-idle` (idle pre-load + move SPE toggle out
  of open frames) — worth a control run, but static analysis predicts its falsifier
  (`00039020`). Only an LA showing a stray edge on the `0x15` frame revives it.
  Detail: `scratchpad/config_entry_findings_H5.md`.

---

## 4. Already excluded — do NOT re-chase

- Inter-byte gap (EXP-26)
- `0x05` ERASE_SRAM omission (EXP-27)
- Command **clock rate** — failed at BOTH /2 and /256, so "just slow the command
  down" is dead
- All static MCU pin state — CRL/CRH, output levels, pull-ups (Exps E/F/R)
- RECONFIG_N pin pulses (Exps G/O/Q/T), RELOAD `0x3C` (Exp S), power cycle (Exp R)
- USART-borne trigger (refuted statically, Exp R(e))
- PB4/MISO pull — floating in BOTH a success (bit-bang) and the fail (HW-SPI)

---

## 5. Recommended order — RESOLVED BY THE AGENT SWEEP (2026-08-25)

The four zero-hardware lanes ran and **converged on H2.** Standings:

| H | verdict | why |
|---|---|---|
| **H2** | ❌ **REFUTED AT BENCH (EXP-29)** | Doubly-converged static lead (H4 stock disasm + H1H2 our source). `guest-configA-csdummy` on a cold FPGA → `00039020 D0 A0`, bit-identical wall. Matching the CS/dummy framing is NOT sufficient. |
| H4 | ✅ done | The anchor diff. Named H2; catch-all found no *config-region* divergence — but did NOT cover SPI3 CTRL1 setup or the PB3/4/5 AF-pin config, which live outside `master_init`'s config sequence. **That is now the open gap.** |
| H1 | ❌ refuted | Settle is equal (H4) / inverted (H1H2) — failing path already has ~300 ms. |
| H3 | ❌ refuted | Stock's upload is polled too; `CTRL2=0x03` already matches; DMA-request speculation moot. |
| H5 | ⚠ weak | Glitches real but mistimed to affect the `0x15` frame; LA-only revival. |

### Where this leaves us (post-EXP-29)

**Every firmware-only "match stock's bytes / CS-framing" hypothesis is now dead**
(bytes, gap, 0x05, clock rate, DMA, CS-dummy). The wall is decided by something the
byte/CS stream does not capture. Two candidates remain, in cost order:

- **H6 (zero-hardware) — SPI3 CTRL1 + PB3/4/5 AF-pin config diff. ❌ REFUTED
  (agent, 2026-08-25).** Decoded the Exp E/R SWD dumps (live values at the
  CONFIG_ENABLE instant on BOTH firmwares) vs current code: **SPI3 CTRL1 identical
  in every mode/framing bit** (CPOL/CPHA/MSTEN/MSB/8-bit/SSM/SSI), only BR differs
  (excluded both ways); **PB3(SCK) & PB5(MOSI) both CRL nibble `0x9` = AF-PP 10 MHz
  in stock AND ours** (the "50 MHz" code comment was wrong — now fixed); **AFIO MAPR
  `0x02000000` both**. PB4/MISO pull differs but is already excluded (bit-bang works
  floating). **At CONFIG_ENABLE the SPI3 peripheral + pin config are PROVABLY
  IDENTICAL between the transport that succeeds (stock) and the one that fails
  (ours).** No knob possible. Detail: `scratchpad/config_entry_findings_H6.md`.
  (Open: PC6 not in the GPIOB-only dumps.)

- **H7 — LA wire-diff (the only remaining instrument).** Capture all three at the
  `0x15` frame on the same unit: stock-HW (success), our bit-bang (success), our HW
  (fail). Overlay SCK/CS/MOSI/**MISO**. It is the only instrument that can (a)
  confirm what actually reached the wire in EXP-29, (b) see intra-frame cadence
  (does our polled `spi3_xfer` stall between `0x15` and its `0x00`?), and (c) see
  whether the FPGA drives SO/ACK differently on a successful frame.

### The paradox that now defines the search (post-H6)

Register/byte/framing analysis is **exhausted** — 6 hypotheses excluded. The
remaining puzzle, stated exactly:

| path | boot history | transport/registers | result |
|---|---|---|---|
| stock HW-SPI | heavy (~15 KB init first) | = ours | ✅ |
| our bit-bang | lean (= our HW) | GPIO, slow | ✅ |
| **our HW-SPI** | lean | = stock's registers | ❌ |

- our HW vs **bit-bang**: same boot, same firmware — differ only in **transport** →
  transport matters *within our firmware*.
- our HW vs **stock**: same transport+registers — differ only in **boot history** →
  boot history matters.

The two successes each differ from the failure on a *different* axis, which is why
every single-axis "match stock" fix has walled. Clock **speed** is NOT it (stock
fast ✅, bit-bang slow ✅, our /2 AND /256 both ❌). The two live suspects the LA
can separate: **(a) intra-frame polled-cadence stall** unique to our
`spi3_xfer` peripheral path (bit-bang and stock's unrolled loop both avoid it), and
**(b) FPGA boot-history / receptivity state** (POR=1, DONE_FINAL=0 anomaly).

**Suspect (a) — intra-frame cadence — ❌ REFUTED AT BENCH (EXP-30, 2026-08-25).**
`guest-configA-gapless` clocked the mode-0 prelude frames back-to-back (no
mid-frame SCK idle, matching both working transports) → `CFG:00039020 D0 A0`,
bit-identical wall. Cadence excluded.

## 7. THE FIRMWARE SEARCH IS EXHAUSTED (2026-08-25)

Every firmware-reachable axis has been tried and walls:

| axis | excluded by |
|---|---|
| payload bytes | issue #18 (byte-exact vs stock) |
| inter-byte gap | EXP-26 |
| 0x05 ERASE_SRAM | EXP-27 |
| clock rate (/2 and /256) | 2026-06-12 sweep + this project |
| DMA vs polled | H3 (stock is polled too; CTRL2 matches) |
| CS-high dummy | EXP-29 |
| SPI3 CTRL1 + PB3/4/5 pin config | H6 (register-identical to stock) |
| intra-frame cadence | EXP-30 |

**No register snapshot or firmware knob can explain the wall.** The one remaining
suspect is **boot-history / FPGA-receptivity state** — what differs about the FPGA
between our lean boot and stock's ~15 KB init, or a co-signal we have never
watched. The only instrument that can see it is the **logic analyzer (H7)**.

### H7 — the logic-analyzer session (the path forward)

Not "confirm the gapless edges" — the real value is **catching a co-signal the
issue-#18 capture (SPI3-only) was blind to.** Plan:

- **Pads (known, back-side SPI3 test pads — maksidze issue #18):** SCK=PB3,
  MISO/SO=PB4, MOSI/SI=PB5, CS=PB6, + GND. Real solderable pads, not the FPGA legs.
- **Channels (HiLetgo 8ch, 24 MHz):** the 4 SPI3 lines + GND, plus spare channels
  on candidate off-SPI3 lines during a **stock** boot (PC6 SPI-enable, and the
  long-suspected RECONFIG_N-ish pins near config).
- **Captures to compare at the `0x15` frame, same unit, same rig:**
  1. **stock boot** (success) — the reference; maksidze's issue-#18 Saleae capture
     may already cover the SPI3 half.
  2. **our bit-bang** (`coldtrace`, success)
  3. **our HW-SPI** (`configA`, fail)
- **What to look for:** (a) does stock/bit-bang assert a pin our HW path doesn't;
  (b) does the FPGA drive SO/ACK on a successful `0x15` that stays silent on ours;
  (c) physically settle the `HARDWARE_PINOUT.md:54` 50 MHz vs SWD 10 MHz SCK
  contradiction.

**Coffin-nail firmware knobs still available (predicted to wall):**
`guest-configA-settle SETTLE=5000` (H1), `guest-configA-idle` (H5).

## 6. Bench discipline reminders

- Anchor every FPGA read: IDCODE `0x11` at /256 must read `0x0120681B` (`A0`).
- Never read STATUS `0x41` during capture.
- True power cycle = POWER → "Goodbye" → **unplug USB** → replug. IAP flash and
  POWER-with-USB do NOT drop the FPGA rail — every "did it configure?" read on a
  non-cold part is a lie (this is what nearly sank EXP-27).
- A negative is worthless without a same-session positive control through the same
  path.
