# EXP-23 — config-transport bisect runbook (Tang Nano dev board)

- **Date:** 2026-08-25
- **Unit:** Tang Nano 9K (GW1NR-9, in transit — arriving today) + `bluepill_bis` rig; confirmation re-runs on 2C53T bench unit #1
- **Build:** rig `cd bluepill_bis && make` (commit at run time); firmware legs already exist as `make guest-*` targets
- **Status:** OPEN — ready to execute the hour the board arrives

This is a **runbook**, not a single experiment: it sequences BIS-1..BIS-4 from
[`docs/dev_plan_2026-08-21.md`](../dev_plan_2026-08-21.md) §"The config-transport
bisect" into an order you can run without re-deriving anything, with the exact
rig commands, the expected reply for each, and the falsifier for each. Fill in
the readback tables as you go. The prose backstory is in the devlog,
[The rig before the target](../devlog/2026-08-20-the-rig-before-the-target.md).

## 1. Problem

The 2C53T's stock firmware configures its Gowin GW1N-UV2 through the **hardware
SPI3 peripheral** — the very peripheral that silently discarded the same
115,638-byte payload for us for two months — while the loader that finally broke
the wall (`fpga_bitbang_config_sequence`, the maksidze/Stlkv 2C23T-V0.4
transplant) **bit-bangs GPIO**. Build A already excluded raw clock speed
(hardware-SPI at /256, byte-identical framing, still failed), and maksidze's /64
stock capture proved slow hardware SPI *can* configure a GW1N. So the deciding
factor is one of a small bundle that has never been separated:

1. the **V0.4 prelude reads** (`0x11`→`0x13`→`0x41` immediately before `0x12`/`0x15`) — BIS-1
2. the **`0x05` ERASE_SRAM** frame our path sends and V0.4 omits — BIS-2
3. the **transport waveform** — inter-byte gaps, CS setup/hold, clock idle, edge rates — BIS-3
4. the **part state** at config time — blank vs auto-booted-from-flash — BIS-4

The Tang Nano makes the config port **observable** on a part with the same
silicon config engine and the same embedded-NV auto-boot mechanism as the
scope's part, at zero risk to bench unit #1.

## 2. Hypotheses (each with its falsifier)

| # | Leg | If the deciding factor is this, we see… | …else we see |
|---|---|---|---|
| BIS-1 | `1` (HW + V0.4 reads) | `ST15 EDIT:1` (config entry engages) | `ST15 EDIT:0`, same wall |
| BIS-2 | `2` (HW, no `0x05`) | `ST15 EDIT:1` | `EDIT:0` |
| BIS-2× | `B` (BB **with** `0x05`) | bit-bang now **fails** (`EDIT:0`) ⇒ `0x05` is the whole story | `b`-like success ⇒ `0x05` not the cause |
| BIS-3 | waveform capture | gaps/edges differ and are the only thing left | — |
| BIS-4 | blank vs auto-booted × HW vs BB | HW works blank, fails auto-booted; BB works both ⇒ **mechanism** | any other pattern narrows differently |

**The discriminator is a *flip*, not an absolute value.** The 2C53T wall reads
`ST15 = 0x00039020, EDIT:0, DONE:0`; the Tang Nano is a different part and will
show a different baseline STATUS. What we are watching is whether **`EDIT`
(bit 7) or `DONE` (bit 13) ever change across legs** relative to that board's own
`a`/`b` poles (§4).

## 3. Pre-flight (do all of this before the first leg)

**Wiring** — four bus lines + GND, Blue Pill ↔ Tang Nano. **Buzz every line for
continuity before powering both boards.** The 20K map is in
[`bluepill_bis/README.md`](../../bluepill_bis/README.md); for the **9K**, take
the same four signals off its schematic (its SSPI pins are not the 20K's pin
numbers — do not copy the numbers, only the signal names):

| Blue Pill | signal | Tang Nano 9K |
|---|---|---|
| PB3 | SCLK | _(fill from 9K schematic)_ |
| PB5 | MOSI → SI/DIN | _(fill)_ |
| PB4 | MISO ← SO | _(fill)_ |
| PB6 | CS → SSPI_CS_N | _(fill)_ |
| GND | GND | GND |

Both are 3.3 V; power each from its own USB, common ground. UART console:
PA9/PA10 → the FT232H at 115200 8N1 (`--port /dev/ttyUSB0`; the FT232H is the
bench's UART, first used for this rig on 2026-08-20).

**MODE straps / config mode (the make-or-break precondition).** SSPI-slave must
actually be the mode the part is listening in, or every leg reads all-`FF` and
means nothing. The 9K auto-boots its embedded flash design on power-up; the SSPI
port is what we drive on top of that. On arrival, look up the GW1N-9 SSPI-slave
MODE value in UG290 and set the 9K's MODE straps (its user buttons / resistor
straps) accordingly. Verify by readback below — an all-`FF`/`OFF:-1` on `i` is
"the port is not listening," not "the FPGA refused config."

**Preconditions verified by readback** (not assumed — project law):

| what | how | expected | measured |
|---|---|---|---|
| rig alive | power rig, open console | `RIG:bluepill_bis BUILD:…` banner | |
| IDCODE anchors | `python3 host.py --port … i` | a **stable, non-FF** IDCODE, `OFF:00` | |
| set anchor | `--raw 'e <idcode>'` then `i` | `OFF:00` (aligned) | |
| bit-bang anchor | `--raw I` | same IDCODE, `OFF:0` | |
| MISO idle | `--raw 'm 1'` (default) | pull-up; `m 0` floats it as a control | |

> **Do not assume IDCODE `0x0120681B`.** That is the 2C53T's GW1N-2. The 9K is a
> different die and reports a different IDCODE — read it with `i`, set it with
> `e`, and only then is the anchor valid. Everything downstream is
> uninterpretable until `i` returns a stable value that `find_idcode` aligns at
> offset 0.

## 4. Controls (record FIRST, both poles, same session)

Establish the two poles on **this board** before trusting any middle leg. All
four legs below take **no payload** — the wall signature is payload-free.

| pole | command | expected on a part where transport matters | measured |
|---|---|---|---|
| **negative** (should fail) | `host.py … a` | `ST15 EDIT:0 DONE:0` | |
| **positive** (should configure) | `host.py … b` | `ST15 EDIT:1` (and/or the part leaves the wall) | |

> If `a` and `b` read **identical**, STOP — the board is not discriminating
> (wrong MODE strap, or SSPI not the boot path, or the 9K's auto-booted state
> refuses all SSPI entry the way the 2C53T did). That is a **VOID precondition,
> not a negative result.** Fix the strap/state (see BIS-4) before running BIS-1/2.

## 5. Procedure — the legs, cheapest-first

Each entry leg prints: `PRE11:` (pre-config IDCODE), `PRE41:… EDIT:x DONE:x`,
`ST15:xxxxxxxx EDIT:x DONE:x` (**the wall test, right after `0x15`**), `ID15:`
(post-`0x15` IDCODE — Exp L open/closed-port check), `ST3A:…`.

### Run 1 — BIS-2, the leading suspect (`2`)
```
python3 host.py --port /dev/ttyUSB0 2      # HW transport, 0x05 OMITTED, no reads
```
- **Why first:** single-variable against the `a` pole (only `0x05` differs), and
  the dev-plan's live suspect — maksidze's /64 stock capture already excluded
  clock rate, and the bit-bang loader that works also omits `0x05`.
- **`EDIT:1`** ⇒ our `0x05` was *poisoning* config entry, not inert. **Huge:** it
  hands us a hardware-SPI config path ~100× faster than bit-banging 115 KB.
- **`EDIT:0`** ⇒ `0x05` is not the cause; go to Run 2.

### Run 2 — BIS-1, prelude reads (`1`)
```
python3 host.py --port /dev/ttyUSB0 1      # HW transport + V0.4 reads 11/13/41 before 12/15
```
- **`EDIT:1`** ⇒ the config engine needs a read transaction to sync before it
  will accept CONFIG_ENABLE. Also a fast hardware-SPI path.
- **`EDIT:0`** ⇒ neither prelude-shape variable is it; the difference is in the
  waveform (BIS-3) or the part state (BIS-4).

### Run 3 — BIS-2 cross-check (`B`)
```
python3 host.py --port /dev/ttyUSB0 B      # BIT-BANG transport WITH 0x05 inserted
```
- **`EDIT:0`/fails** (while `b` succeeded) ⇒ `0x05` confirmed as the whole story,
  independent of transport. Strong, because it isolates the same variable from
  the other direction.
- **succeeds like `b`** ⇒ `0x05` is not the discriminator on the bit-bang side
  either; weight shifts to the waveform.

### Run 4 — BIS-4, the state bisect (dev board only)
Manufacture both part states the 2C53T could never give us (Exp R: even a true
power cycle didn't reopen its port), using the 9K's onboard flash:
- **auto-booted state:** normal power-on (flash design loaded) — the state all
  four of our months of refusals ran against.
- **blank state:** erase the 9K's flash (openFPGALoader `-r`/erase, or its
  boot-mode strap) so nothing auto-boots, then power-on.

Run `a` (HW) and `b` (BB) against **each** state:

| state | `a` (HW) | `b` (BB) |
|---|---|---|
| auto-booted | | |
| blank | | |

- **HW works blank, fails auto-booted; BB works both** ⇒ a **mechanism**: the
  auto-booted config-FSM state is what closes the port to hardware-SPI framing,
  and bit-bang's waveform is what re-opens it. This is the single most
  explanatory outcome and directly informs whether config entry ever needs a
  RECONFIG_N assertion on the real board.

### Run 5 — full-config confirmation (only after a positive entry leg)
Any leg that shows `EDIT:1` gets a payload run to reach `DONE:1`:
```
python3 host.py --port /dev/ttyUSB0 2 --fs <9K_bitstream.fs>
```
- **Success signature:** `ST15 …/ST3A … DONE:1` **and** `ID15` goes to zeros or a
  mismatch — a configured Gowin **stops answering SSPI** (Exp L); the port
  closing is the positive proof, not a status bit you can keep reading.
- **⚠ Read §6 before trusting a hardware-SPI `DONE:1` here** — the rig streams
  the payload *gapped*, which is a confound for exactly the BIS-3 question.

## 6. Blind spots (what this rig **cannot** detect — read before interpreting)

Two hard limits of the Blue Pill rig, both found while preparing this runbook:

1. **The rig cannot reproduce gapless payload streaming.** The 115 KB payload is
   fed to the rig **over UART, one byte at a time** (`stream_payload` reads each
   byte with `uart_getc_timeout` then clocks it out), and the F103C8 has only
   20 KB RAM so it **cannot buffer the payload to stream it from memory.** At
   115200 baud each payload byte carries a **~87 µs** inter-byte gap — on
   **both** transports. So during the payload the rig is *always* gapped, while
   the real 2C53T firmware's hardware-SPI path streams the payload **gaplessly
   from the FIFO** (the BIS-3 devlog measured this on the *prelude* bytes, which
   the rig hard-codes and clocks back-to-back — not on the payload).
   **Consequence:** the entry legs (BIS-1/BIS-2, no payload) are clean and
   unconfounded — run and trust those. But a hardware-SPI **`DONE:1` with
   payload on the rig does NOT prove the real gapless firmware path would
   configure**; it is consistent with "gaps are what the engine needs," not a
   refutation of it. The definitive gapless-vs-gapped **payload** test must be
   run on the 2C53T (§7 follow-up).

2. **STM32F1-class, and a clone.** Sequence results (BIS-1/BIS-2/BIS-4) transfer
   to the AT32 directly; a **waveform** finding (BIS-3) is only "STM32F1-class"
   evidence, and the bench Blue Pill is a CS32-class clone
   (idcode `0x2ba01477`) — a positive BIS-3 must be confirmed at the 2C53T's own
   0.5 mm pins before it is believed.

3. **A different die.** The 9K is GW1NR-9, not the scope's GW1N-UV2. The config
   *engine family* and the auto-boot *mechanism* are the shared subject; a
   flip here is a lead to confirm on the 2C53T, never a conclusion about it.

## 7. Conclusion (to fill in) and follow-ups already known

- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**

**Follow-ups regardless of outcome:**

- **The definitive gapless-payload test belongs on the 2C53T, and it needs one
  firmware knob that does not yet exist.** `fpga_bitbang_config_sequence` already
  has `FPGA_BB_HALF_DELAY` (add gaps to bit-bang). There is **no symmetric knob
  to insert inter-byte gaps into the hardware-SPI payload** in
  `fpga_spi3_config_sequence` — so "does gapping the fast path make it
  configure?" cannot be asked today. Appendix A is a ready, default-no-op sketch
  to add before that bench session; adding it touches the shipping config path,
  so it is deliberately left for a bench-validated change, not made here.
- **After config entry there is a second gate — the engine arm.** A part that
  reaches `DONE:1` on the 2C53T still starts **UNARMED** (one buffer then halt);
  arming needs the five control writes with **PB11 (IOR1B) HIGH ∧ PC6 (IOB7B)
  HIGH ∧ the SPI arm bit** (the #18 netlist result). This is **not testable on
  the Tang Nano** — it is a bare FPGA running its own flash design, with no
  2C53T capture engine — so arm stays a scope-side follow-up. Don't read a Tang
  Nano `DONE:1` as "the scope will now capture."
- **Secondary stake:** the USB-CDC-enumeration behaviour that tracks which
  config path a build uses (noted in the README's "sharp edges" and
  `dev_plan_2026-08-21.md`) — whichever suspect wins here is the leading
  candidate for that mechanism too.

## Appendix A — ready firmware gap knob (do NOT apply blind; bench-validate)

For the on-2C53T gapless-vs-gapped payload test only. Mirror of
`FPGA_BB_HALF_DELAY`, defaulting to 0 = byte-unchanged from the shipping path,
inserted into the `0x3B` upload loop of `fpga_spi3_config_sequence`:

```c
/* Inter-byte gap for the hardware-SPI 0x3B payload, to test the BIS-3
 * "gapless FIFO streaming is why hardware SPI fails" hypothesis on real
 * hardware. 0 = gapless (shipping behaviour, unchanged). */
#ifndef FPGA_HW_UPLOAD_GAP_NOPS
#define FPGA_HW_UPLOAD_GAP_NOPS 0
#endif
/* …inside the upload loop, after each spi3_xfer(payload_byte): */
#if FPGA_HW_UPLOAD_GAP_NOPS > 0
    for (volatile int _g = 0; _g < FPGA_HW_UPLOAD_GAP_NOPS; _g++) __asm__ volatile("nop");
#endif
```

Then a `guest-configA-gap` target (`-DFPGA_HW_UPLOAD_GAP_NOPS=<n>` sized to
~1.4 µs at 240 MHz). If a gapped hardware-SPI upload reaches `DONE:1` where the
gapless one does not, BIS-3 is confirmed on the real part — the outcome the rig
structurally cannot deliver (§6.1).

## Appendix B — rig quick reference

Legs: `i`/`I` IDCODE (hw/bb, **anchor first**) · `a` HW base · `1` BIS-1 · `2`
BIS-2 · `b` bit-bang V0.4 · `B` bit-bang+`0x05` · `p` arm-payload · `s` status.
Knobs: `e <idcode>` (per board — read with `i` first), `d <cmd_br> <upload_br>`
(BR dividers, default 7/7 = /256), `m <0|1>` MISO pull-up/float. Capture a leg
on the HiLetgo: `bluepill_bis/capture.sh <leg> <out.sr>` (keep the HW leg at
/256 — 24 MS/s does not resolve /2). Build verified clean 2026-08-25
(3436 B text). No rig code change was needed for BIS-1/BIS-2; the rig's limit is
the payload path (§6.1), which is a firmware-side test, not a rig fix.
