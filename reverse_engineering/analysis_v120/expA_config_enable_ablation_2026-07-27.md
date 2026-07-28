# Experiment A — CONFIG_ENABLE ablation on stock (2026-07-27)

## Question
Is the every-boot SSPI FPGA reconfiguration (the `0x05/0x12/0x15 → 0x3B + 115,638-byte
bitstream → 0x3A` handshake in stock master init) actually **required for the
oscilloscope**, or does the GW1N-UV2's non-volatile (autoloaded) design already service
scope capture? For months this was an *inference* ("the NV image alone services USART
meter traffic but evidently not scope capture" — CLAUDE.md), never directly tested.

## Method — single-byte ablation of stock V1.2.0
Patched exactly one byte in `APP_2C53T_V1.2.0_251015.bin` (sha256 `a17c5c35…`):

| item | value |
|------|-------|
| file offset | `0x26A42` (flash `0x0802DA42`) |
| instruction | `movs r0, #0x15` → `movs r0, #0x11` |
| bytes | `15 20` → `11 20` |
| effect | the **CONFIG_ENABLE** opcode (`0x15`) that engages Gowin EDIT_MODE becomes **READ_IDCODE** (`0x11`, side-effect-free) |
| patched sha256 | `1dcf7a58…` |

Rationale: with CONFIG_ENABLE never issued, the subsequent `0x3B` write-SRAM opcode is
**ignored by the FPGA** (exactly the wall our own firmware hits), so the FPGA retains its
autoloaded NV design in SRAM. Every other byte of stock is identical — power-hold (PC9),
buttons, meter pipeline, UI, and the post-`0x3A` scope register writes all run normally.
No control-flow or register-state surgery; no risk of blanking config SRAM (config mode is
never entered). Chosen after confirming stock **never gates boot on the config status
reply** (every `ldr r0,[r6,#4]` status read is immediately clobbered by the next
`movs r0,#0x40`), so the ablation cannot hang stock's init.

Flashed to the bench unit (native stock layout, app @ `0x08007000`) via the factory IAP
MSC channel. Device readout = stock's own LCD (app-side USB CDC is independently broken;
irrelevant here).

## Result (bench unit #1, 2026-07-27)
- **Boots normally** into stock UI (battery charge screen → POWER → full stock).
- **Multimeter: WORKS.** Read a Li-ion cell at **4.11 V DC**, no problem. → FPGA is alive
  and running its NV design; the NV design services the meter.
- **Oscilloscope: DEAD.** No trace at all — not even a flat/dancing baseline. Tapping the
  CH1 probe to the same 4.11 V cell produced nothing. AUTO (autoset) did nothing.

## Conclusion
**The every-boot SSPI reconfiguration IS load-bearing for the oscilloscope.** The GW1N-UV2
non-volatile design is **meter-only**; scope capture depends entirely on the ~115 KB
bitstream stock uploads at every boot. The meter serves as a perfect control proving the
FPGA is otherwise healthy and the one-byte patch broke nothing broadly — the *only* thing
removed was entry into config mode, and the scope died with it while the meter was
untouched.

This promotes a long-standing inference to an **experimental fact**, isolated to a single
causal variable (the `0x15` CONFIG_ENABLE opcode).

## Implication for the FPGA effort
There is **no shortcut** where the resident NV design already does scope — our focus on
getting CONFIG_ENABLE to *take* is correct and necessary. The problem is now fully
localized: on **unpatched stock** the identical `0x15 → 0x3B → 0x3A` sequence successfully
reconfigures the FPGA (scope works), while on **our firmware** it is refused (STATUS pinned
`0x00039020`, EDIT_MODE bit7 never engages). Same wire sequence, different outcome ⇒ the
determining factor is **FPGA state at the moment CONFIG_ENABLE is issued**, not the bytes.
Unpatched stock is now a known-good CONFIG_ENABLE **oracle** to diff against.

Leading hypothesis to test next: the FPGA only accepts CONFIG_ENABLE in a **window shortly
after power-on**, before it locks into user mode running the NV design. Stock issues `0x15`
very early in master init and hits the window; our firmware does extensive init first and
by the time it tries, the FPGA has settled into user mode (observed emitting `C8 10 00 01`
telemetry). Candidate experiment: issue CONFIG_ENABLE as the very first FPGA action after
power-on, before any other init.

## Positive control — PASSED (2026-07-27)
Reflashed the **clean, byte-identical** `APP_2C53T_V1.2.0_251015.bin` (sha256 `a17c5c35…`,
verified by the flasher) to the same unit, same probe, same Li-ion cell. **Scope trace
returned immediately** — finger-touch noise on CH1 *and* the 4.11 V DC level off the cell.

Full A/B/A on one bench unit, one session, one probe, one cell:

| firmware | delta vs stock | scope | meter |
|----------|----------------|-------|-------|
| clean stock | — | **works** | works |
| Exp-A stock | 1 byte (`0x15`→`0x11`, CONFIG_ENABLE ablated) | **dead, no trace** | works |
| clean stock (reflash) | — | **works again** | works |

Causal attribution is now unambiguous: the presence/absence of the scope trace is
controlled entirely by whether the FPGA is allowed to enter config mode and receive the
boot bitstream. Single-variable, reversible, reproduced.

## Experiment B — delay CONFIG_ENABLE to test the timing window (INCONCLUSIVE, 2026-07-27)
Attempted to push stock's config-enable/upload ~5 s later by ballooning the two
inter-command gap loops: `subs r2,#50` → `subs r2,#1` at file `0x2689A` and `0x269D2`.
**Null result** — pinhole reset reached the charge screen essentially instantly (no added
delay), and the scope worked. Root cause: that loop's `r2` is a **timeout guard on an
event-wait**, not a delay length. The loop (`movs r2,#100; movs r0,#50; … cmp uxth(r2),#51;
subs r2,#50; muls r3,r0; str [r5,#4]; spin on [r5]&0x10001==1`) spins until the SPI/timer
event fires and exits immediately regardless of `r2`; the actual dwell is set by the timer
reload `mem[0x00002b20]*r0` (r0=50), not by the iteration count. So the patch extended the
timeout, changed wall-clock by ~0, and did **not** move config-enable later. The "scope
works" here says nothing about the acceptance window. **Do not use `r2` as a delay knob.**

Watchdog note confirmed useful regardless: stock arms IWDG (`0x5555/0xAAAA/0xCCCC` key
sequence) only at `~0x0802E5A8`, *after* the FPGA config block — so a real added delay in
the config phase will not trip a boot-loop reset. A future proper delay is safe there.

## Experiment B2 — real delay before CONFIG_ENABLE: timing window REFUTED (2026-07-27)
Injected a **reliable CPU busy-loop** ahead of the config sequence (redirect at flash
`0x0802D7D2`, just before the `0x05` opcode send, → code cave at `0x0806F034`). Cave:
`push {r2,r3}; movw/movt r2,#0x20000000; 1: subs r2,#1; bne 1b; pop {r2,r3};`
+ the two displaced insns (`movs r0,#5; str r0,[r6,#4]`) + `b.w 0x0802D7D6`. 0x20000000
≈ 5.4e8 iterations of a 2-insn loop at 240 MHz ⇒ ~5–9 s. All branch encodings verified by
disassembling the patched image before flashing (`arm-none-eabi-as`/`ld`/`objdump`).

**Bench result:** the delay was real and visible — **~5–10 s pause from the FNIRSI splash to
the menu** (config-enable pushed that much later in boot). **Scope came up fully working,
trace alive and responding to input.** So the FPGA accepts CONFIG_ENABLE at least ~6–12 s
after power-on. **The narrow-post-power-on-window hypothesis is refuted.** (Corroborated
independently: our own firmware's on-demand `fpga reinit` is refused seconds-to-minutes
after boot — no timing that helps.)

Cosmetic side note: the first cave placement (`0x0806F034`, the only ≥48-byte `0xFF` run in
the image) turned out to be **live graphics data** — it distorted the stock battery-charge
icon. The delay still executed (same bytes run as code via the redirect and read as icon
data by the display path). For a clean image the cave must move to genuinely dead space
(true code-alignment `0x00` padding, or appended past the image end). Cosmetic only; scope
result unaffected.

## Where this leaves the FPGA problem (2026-07-27)
Everything **firmware-reachable and timing-related is now excluded** as the reason our
CONFIG_ENABLE is refused while stock's identical `0x15→0x3B→0x3A` is accepted:
- bitstream bytes — byte-correct on the wire (proven, maksidze capture + our replay)
- framing / CS / prelude / trailing clocks — matched (proven)
- SPI3 wire sequence — replicated exactly (proven)
- narrow post-power-on timing window — **refuted (Exp B2)**

⇒ The blocker is a **signal/state difference**, not bytes or timing: something stock has in
the right state when it issues CONFIG_ENABLE that our firmware does not. Leading candidates:
1. **User-mode lockout** — on stock, CONFIG_ENABLE is issued in master init *before* any
   USART/user-mode FPGA traffic; on our firmware the FPGA has been servicing user-mode
   (meter) traffic and emitting `C8 10 00 01` telemetry by the time we try. Hypothesis: once
   the NV design is actively servicing user commands, the FPGA refuses config re-entry.
   (This is the USART-door idea, *inverted*: traffic LOCKS rather than opens.) Testable as a
   stock ablation — inject a USART/user-mode command before config-enable and see if it
   breaks; or, cleaner, on our firmware issue CONFIG_ENABLE before starting the meter task.
2. **A pin/strap not on the captured channels** — an FPGA mode/enable line stock drives that
   we don't (or drive wrong), invisible to the SPI-only Saleae capture.

## Experiment C — frontend-strap ablation: frontend posture REFUTED (2026-07-27)
Tested the #1 candidate from `stock_pre_fpga_gpio_state.md`: stock drives the entire
analog-frontend relay/gain bank (range-5 posture `PC12=L PE4=H PE5=H PE6=L | PA15=H PA10=H
PB10=H`, plus PB11) **before** it opens the `0x3B` upload; our firmware leaves that whole
bank **floating** during the upload. Hypothesis: the Gowin samples those pins as
straps/environment at config time.

**Method:** NOP'd stock's two range-select *call sites* at boot — `gpio_mux_portc_porte(5)`
@ `0x0802C546` and `gpio_mux_porta_portb(5)` @ `0x0802C54C` (each a 4-byte `bl` → `bf00
bf00`, file offsets `0x25546`/`0x2554C`). Deliberately NOP'd the **call sites, not the
helper bodies**, so mode-entry still re-drives the relays for the signal path later — making
the readout clean: a dead scope would mean config genuinely failed, not a stuck analog path.
Stock thus uploads the bitstream with the frontend bank floating, **exactly like our
firmware.** Single variable. Patched sha256 `f0e248ca…` (clean base `a17c5c35…`); both NOPs
verified by objdump before flashing. Flashed via factory IAP.

**Bench result (unit #1):** **scope ALIVE, trace responds** to finger/probe input; **meter
reads the cell correctly.** Floating the frontend bank through config changed nothing.
⇒ **The analog-frontend posture is NOT the config blocker.** This eliminates the single
largest enumerable stock-vs-ours difference. Fits the mechanism: those pins drive signal
relays / gain switches, not FPGA config-logic straps. (Battery icon also rendered clean,
confirming the B2 glitch was purely the earlier cave placement.)

Candidate list after Exp C — remaining leading hypothesis is **#1 user-mode lockout**
(pre-config user-mode FPGA traffic locks out config re-entry); frontend-strap (#2) is now
**refuted**.

## ⚠️ RETRACTED — the "wall broken" claim below is FALSE. Read this box first.
**Everything in the next section that reads as a breakthrough was an artifact of clocking the
SSPI status reads at /2, which `fpga.c:1564` documents as the garbage domain
("IDCODE reads garbage at /2, clean at /256"). The boot path never set `cmd_br`, so it took
the 0 default = /2.** Once `cmd_br=7` (/256) was set and the reads became valid, the true
status is **`CFG:00039020`** — the long-documented refusal signature (CLAUDE.md: "STATUS
pinned 0x00039020, EDIT_MODE bit7 never engages"; low byte `0x20` ⇒ bit 7 clear ⇒
**SYSTEM_EDIT_MODE never engaged**). `SS:20000390` is the same 4 bytes rotated, confirming one
real value read at different phases.

**Conclusions that DO survive:**
- The `0x4AD19` payload fix did **not** break the config-entry wall. The wall is unchanged.
- `trailing_clocks=256` did **not** produce DONE. (Tested; leave it or revert — not the fix.)
- **All status reads taken at /2 anywhere in this project are garbage** and any conclusion
  drawn from them must be re-checked. `H2:Y` is only our own "we transmitted" flag and is NOT
  FPGA confirmation. `PC0:H` and a `FF` prelude are likewise NOT proof of config entry.
- The acquisition transport genuinely works (OK +2/refresh, TO:0) but returns constant data,
  consistent with the fabric never going live.
- Useful new instrument: the LCD overlay now shows `SS`/`CL`/`CFG`/`L`/`H`, so the bench is
  fully diagnosable **without USB** — `make guest` + factory IAP + LCD is a complete loop.

*Process note: the breakthrough was called twice on indicators that could not distinguish
"FPGA responding" from "MISO floating / invalid clock domain." Require `CL=F8` and a /256
clock before believing any config-status claim.*

## [RETRACTED — see box above] CONFIG-ENTRY WALL BROKEN — first bench test of the 0x4AD19 payload fix (2026-07-27)
The June 2026 root-cause fix (bitstream re-extracted from **file offset 0x4AD19**, correcting
the 0x7000 link-base bug that had us replaying garbage for months) had **never been bench
tested** — it was wrongly considered blocked behind the app-side USB CDC failure. It is not:
`make guest` + the **factory IAP** channel flashes fine, and `scope_ui.c`'s
`SCOPE_DEBUG_OVERLAY` prints the needed diagnostics **on the LCD**, so no USB is required.

Built + flashed our firmware (`firmware.bin`, 490,920 B, sha256 `1e673e6e…`; bitstream table
verified **byte-exact** vs the archive at 0x4AD19, sha256 `5a0e7338…`).

**Bench result — the documented PASS criteria are met:**

| signal | old (pre-fix) | now | meaning |
|---|---|---|---|
| prelude MISO `G1/G2/G3` | `0x80`/`0x00` (user mode) | **`FF FF FF`** | **config-wait float** |
| `H2` upload flag | — | **`Y`** | bitstream upload completed |
| `PC0` | L | **`H`** | **FPGA armed/active** |

`master_init_decode_diff_2026-06-13.md` defined PASS as *"prelude MISO 0xFF (config-wait
float) … with PC0 going active."* Both hold. ⇒ **Our CONFIG_ENABLE is now ACCEPTED; the FPGA
enters config mode and takes the bitstream.** The months-long "config-entry refused" wall was
the **wrong-offset payload**, exactly as the 2026-06-12 analysis predicted. The
STATUS-pinned-`0x00039020` / EDIT_MODE-never-engages signature is gone.

### CFG STATUS_REGISTER readout — the pre-registered discriminator (2026-07-27)
`fpga.cfg_status_reg[]` (Gowin `STATUS_REGISTER`, opcode `0x41`, captured at `fpga.c:1692`)
had never been surfaced anywhere. Added to the LCD overlay and read on the bench:

```
CFG:8001C810      SS:100001C8   CL:FF   R:C800C800   L0 H172   OK+2/refresh  TO:0
```

The criterion was written into `fpga.c` long before this test: *"All-0xFF = FPGA not driving
MISO (never entered config-receive) → config-entry wall; CRC_ERROR/ID_VERIFY_FAILED set =
bytes reached the engine."* **CFG is NOT all-FF ⇒ bytes ARE reaching the Gowin config
engine.** First time observed in this project.

**Tentative bit decode** (GW1N status register; VERIFY against the sibling `gw1n2-apicula`
repo before relying on it) of `0x0001C810` — bits 4, 11, 14, 15, 16 set:
- **bit 0 CRC_ERROR = 0**, **bit 2 ID_VERIFY_FAILED = 0**, **bit 3 TIMEOUT = 0** ⇒ the
  bitstream was **accepted, not rejected** (so the 0x4AD19 payload is good on the wire)
- bit 14 READY, bit 15 POR, bit 16 FLASH_LOCK, bit 11 BYPASS set ⇒ FPGA alive/responding
- **bit 12 DONE_FINAL = 0** ⇒ **configuration started but never COMPLETED**

⇒ New, much more specific problem statement: **we get into config mode and the engine accepts
our frames without CRC/ID error, but DONE is never asserted**, so the scope fabric never goes
live. That fully explains the downstream symptoms (constant samples, flat trace, probe has no
effect) without invoking any analog/cal theory.

**Caveat on read framing:** `SS`, `CFG` and `1:` share recurring bytes (`00 01 C8 10`) at
different rotations — e.g. `SS = 10 00 01 C8` and `CFG = 80 | 01 C8 10` are consistent with a
repeating 4-byte pattern `00 01 C8 10` sampled at different phases. So our post-upload reads
are likely **byte-phase misaligned** (a stray leading byte / wrong dummy count). `CL:FF` where
stock returns `F8` points the same way. Fix the read framing before trusting any exact bit
decode. Note this does NOT undermine the headline result — a floating MISO cannot produce a
structured repeating pattern.

**Next actions (firmware-only, no new hardware):**
1. Fix post-upload read framing (leading-byte/dummy-count phase) so `CL` returns `F8` and the
   `0x03` status returns stock's `00 01 42 2E`; then re-read CFG for a trustworthy decode.
2. Chase **DONE**: Gowin asserts DONE only after the full frame count + trailing CCLK cycles.
   Our `trailing_clocks` default is **0**; rosenrot00's working 2C23T loader clocks ~200 dummy
   bytes after the last config byte. **Sweep `trailing_clocks` (200/500/1000) — top suspect.**
3. Verify the status-register bit map against `DavidClawson/gw1n2-apicula`.

**Remaining symptom (a NEW, different wall):** the scope renders a **flat fixed line**
refreshing ~2×/s, not responding to input. Note this differs from Exp A (stock, config
ablated) which showed **no trace at all** — so this is not a config failure. Next question is
whether real samples are arriving (`OK:`/`TO:` counters, `1:` first data byte) or whether the
UI is drawing a no-data baseline. Prime suspects, both already catalogued as **high-severity**
divergences in `master_init_decode_diff_2026-06-13.md`: (1) the 40-entry per-range scope/meter
gain-offset cal-table restore — our `flash_fs_load_factory_cal()` is still a **no-op stub**;
(2) the cal-derived **DAC1 scope trigger comparator** value stock writes (`0x40007408` low-12
+ `0x40007404 |= 1` in `FUN_080018A4`).

## Cold-boot control — "FPGA must be virgin" REFUTED (2026-07-27)
Concern: every run of our firmware this session followed a **soft reset** from a stock image
that had already configured the FPGA, so the FPGA was never virgin when our CONFIG_ENABLE
went out. (A pinhole reset is NOT a clean control either — NRST releases PC9 power-hold, so
whether FPGA power actually drops is ambiguous.)

**Test:** deliberate full power-off (POWER held through the countdown), ~10 s for rails to
bleed down, then cold power-on into our firmware — FPGA fresh from NV autoload, exactly the
state stock sees at its own cold boot.

**Result: unchanged — `CFG:00039020`, flat trace.** ⇒ Our firmware fails even on a virgin
FPGA. The "FPGA only accepts config once per power cycle / must be virgin" theory is
**refuted**, and the confound is removed: stock and our firmware now demonstrably start from
the *same* FPGA state and get *different* outcomes. The stock-vs-ours comparison is airtight.

Corollary: the planned "patch stock to configure twice in one boot" experiment is **no longer
needed** to answer this question — the cold-boot control answers it more cheaply.

## Recommended next steps
- **FT232H JTAG oracle** (hardware): SRAM-load the scope bitstream over the JTAG TAP pads
  maksidze exposed — a port stock never uses — to cleanly separate "bitstream good" from
  "SSPI config-entry broken." Decisive; needs bench wiring.
- **User-mode-lockout test**: quickest firmware-side probe once the USB CDC shell is
  restored — bring up CONFIG_ENABLE as the very first FPGA action, before the meter/USART
  task runs. If it then takes, the lockout theory is confirmed.
- Unpatched stock remains the known-good CONFIG_ENABLE oracle for any further ablation.
