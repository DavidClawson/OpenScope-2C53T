# Bench Session Plan — 2026-07-30 (DMM first: FPGA config straps)

**Written 2026-07-29, ~01:00.** Pick-up-cold checklist. Read the two short
"why this changed" sections first — they reorder the plan away from what
`bench_day_spi3_esp32.md` says.

**Tonight's job is 20 minutes with a DMM and the USB microscope. No soldering.**

---

## Why the order changed

Three things came out of the 2026-07-29 review that move the priorities:

1. **RECONFIG_N is already answered — stop hunting it.**
   `analysis_v120/unmapped_mcu_fpga_pin_candidates.md:102` records that maksidze
   checked QN48 **pin 48 (RECONFIG_N): always HIGH, no pulse.** That confirms
   Exps G / O / Q were unwinnable by construction. Do not spend more bench time
   on MCU-driven RECONFIG_N pulses.

2. **The last unknown *physical* facts are MODE0 and JTAGSEL_N.**
   Same doc, lines 100–106: MODE0 (pin 13) and JTAGSEL_N (pin 3) routing are
   listed as open questions and have never been measured. MODE0 sets the
   config-mode group — which is one of the few remaining explanations for a part
   that decodes SSPI opcodes perfectly (Exp J) yet silently ignores every SSPI
   *config* command (Exp N). JTAGSEL_N decides whether the FT232H route works
   at all.

3. **The logic-analyzer plan is expensive and goes last.**
   The candidate pins (PB9, PC1, PC2, PC4, PD2, PD3, PB12) have **no breakout
   pads** — they are AT32F403A package pins at 0.5 mm pitch. Seven magnet-wire
   taps for a *diagnostic* is a bad trade. The SWD GPIO sampler (Task 2b below)
   gets the same boot-diff with zero soldering and covers all ~80 pins instead
   of 8.

## Also: skip the ESP32 group entirely

`bench_day_spi3_esp32.md` Step 5 was written 2026-06-19, **before Exp J**. Its
premise — "`id` → `01 20 68 1B` means the external master gets a response our
MCU never did" — is now known false: our MCU already gets the perfectly aligned
IDCODE. An external SSPI master will hit the identical wall. One less pad group
to solder, one less variable. That doc needs updating; not urgent.

---

## What is accessible on this board

| Group | Signals | Accessibility |
|---|---|---|
| SWD header | 3V3, SWDIO, GND, SWCLK (V1.4 silkscreen order) | **Already wired** |
| 5 gold pads by the FPGA | TCK / TDI / TDO / TMS + **3V3 (not GND!)** | Real solderable pads — the FT232H route |
| Back-side SPI3 pads | SCK / MOSI / MISO / CS + GND | Real pads (maksidze's breakout) |
| Everything else | PB9, PC1/PC2, PC4, PD2/PD3, PB12 … | **No pads.** 0.5 mm MCU package pins |

QN48 pin numbers (UG171E, via `unmapped_mcu_fpga_pin_candidates.md`):

| FPGA pin | QN48 | Status |
|---|---|---|
| JTAGSEL_N | **3** | ❓ unknown — measure tonight |
| TMS | 8 | table unconfirmed, verify by continuity before wiring |
| TCK | 9 | ″ |
| TDI | 10 | ″ |
| TDO | 11 | ″ |
| MODE0 | **13** | ❓ unknown — measure tonight |
| RECONFIG_N | 48 | ✅ always HIGH, no pulse (maksidze) |

> MODE1 / MODE2 are internally tied to GND on this part per UG171E — only MODE0
> is exposed, so MODE0's level is the whole config-mode question.

---

## TASK 1 — DMM + microscope: MODE0 and JTAGSEL_N  ⏱ ~20 min

**Goal:** determine whether QN48 pin 3 and pin 13 sit HIGH or LOW, and whether
they're strapped by a resistor to a rail or driven by something.

### Method

Do it in this order — identify cold, confirm hot.

1. **Power OFF, battery out.** Find the FPGA's pin-1 marker under the
   microscope (usually a dimple or an asymmetric corner pad). Count to pin 3 and
   pin 13. Photograph the package with the count marked — future you will thank
   you.
2. **Continuity (beep mode), still powered off.** For each of pin 3 and pin 13:
   - Beep to **GND**? → strapped low (direct or via a low-value resistor).
   - Beep to **3V3**? → strapped high.
   - Neither? → it goes through a pull resistor, or to a via/MCU pin. Follow the
     trace under the microscope and find the nearest passive; that's a much
     easier probe target than the QFN pin itself.
3. **Power ON, measure DC volts** on each pin to confirm the actual logic level.
   This is the number that matters.
4. **Optional bonus if the pins are easy to reach:** DONE and READY. A live DC
   read of DONE on a working stock boot would settle the standing
   `POR=1 / DONE_FINAL=0` anomaly, which is the single strangest unexplained
   fact in the whole investigation.

### ⚠ Safety

- The real risk is **slipping and shorting two adjacent 0.5 mm pins on a powered
  board.** That's why continuity-first / power-on-second. Use the sharpest
  probes you have, brace your hand, and go slowly under magnification.
- Probe the **nearest passive or via** rather than the QFN pin whenever you can.

### ⭐ Free high-value check while you're already tracing

While the DMM is out and the pads are identified, **buzz each of the 4 JTAG gold
pads against MCU pins.** You are asking: *does any MCU GPIO land on the FPGA's
TAP (TCK/TMS/TDI/TDO)?*

Almost certainly no — those pads are a factory programming point, and a
cost-reduced consumer device has no reason to wire them to the MCU. But it costs
four extra beeps, and a **yes is the entire project**: the MCU could bit-bang
Gowin JTAG in firmware (openFPGALoader's `gowin.cpp` has the sequences), config
entry stops mattering, and it ships over USB with no case opening. Highest
payoff-to-effort ratio on the whole board. Check it.

### Record

Write results straight into this file, or into
`analysis_v120/unmapped_mcu_fpga_pin_candidates.md` §"open questions":

```
MODE0    (QN48 pin 13):  level = ____ V   strapped via = ____
JTAGSEL_N(QN48 pin  3):  level = ____ V   strapped via = ____
DONE     (if reachable):  level = ____ V   (during a working stock boot)
```

### What the outcomes mean

| Result | So what |
|---|---|
| **JTAGSEL_N LOW** | TAP should be live → the FT232H route is viable. Proceed to Task 3. |
| **JTAGSEL_N HIGH** | Expect `openFPGALoader --detect` to fail. **That is itself the finding** — don't burn an evening soldering to a dead port. Cross-ref UG290 for whether it can be overridden. |
| **MODE0 LOW / HIGH** | Record it, then look up the mode encoding in UG290 / UG171E. If the strapped mode does not include SSPI slave configuration, that is a complete, mundane explanation for Exp N (SSPI reads work, SSPI config commands inert) — and it would end the investigation. |
| Either pin **MCU-driven** | Enormous — it means firmware can change config mode on demand, which would be the clean shipping answer. Trace it to which MCU pin. |

---

## TASK 2 — Software track (no bench time, can run in parallel)

### 2a. Move the debug shell onto RTT — ✅ **DONE 2026-07-29, untested on hardware**

`usb_debug.c` already implements a real command shell (`fpga busrelease`,
`fpga reinit`, `spi3 h2verify`). The shell was fine; the **transport** was dead —
USB CDC never enumerates on the bench unit (see `CLAUDE.local.md`). It now runs
over RTT on the already-soldered SWD wires as well.

**What was built**

* `firmware/src/drivers/rtt.{c,h}` — clean-room RTT control block. *Not*
  SEGGER's source: SEGGER licenses RTT for use with J-Link probes only, this
  project is GPLv3 and drives an ST-Link. Only the wire format is shared, which
  is all OpenOCD needs.
* `usb_debug.c` — output tees to both transports; the shell task polls both.
  A unit with working USB behaves exactly as before. RTT input is not echoed
  (telnet echoes locally); USB input still is.
* `main.c` — `rtt_init()` runs immediately after the early watchdog feed, so
  boot output is captured even if the device later hangs.
* `scripts/rtt_shell.sh` — host driver.

**Use it**

```bash
cd firmware && make guest          # flash as usual (IAP; ST-Link unplugged)
./scripts/rtt_shell.sh             # then: telnet localhost 9090
./scripts/rtt_shell.sh -c "fpga idcode"    # headless one-shot, for scripting
```

The `-c` form is the point of the whole exercise: a 50-pin sweep becomes a
host-side `for` loop against one running boot, instead of 50 reflash cycles.

**Verified on host, not yet on hardware.** The ring buffers were exercised
against a simulated host (11,200 bytes through the 2048-byte ring, byte-exact
across wraparound, both directions, plus the no-host-attached drop path). What
that cannot prove is the part that actually needs bench time:

1. **Does OpenOCD read SRAM on a *running* target through `hla_swd`?** Exp E's
   peripheral reads were taken **halted**. Same caveat as Task 2b, and the same
   validation applies — if `rtt_shell.sh` is silent, run `mdw 0x20007c08 4` on
   the running target and see whether the id string comes back.
2. `~/at32_attach.cfg` sets `adapter speed 100` (100 kHz), which makes an
   interactive console painful. The script overrides to 1000 kHz; if SWD gets
   flaky, `-s 100` puts it back.
3. **The ELF must match the flashed image.** The script reads the control-block
   address from `firmware/build/firmware.elf` with `nm` rather than scanning
   224 KB of SRAM. A stale ELF = a silent console, and that is the first thing
   to check.

### 2b. SWD GPIO sampler — the logic analyzer, without the soldering
Extend `scripts/swd_state_dump.sh` into a **running-target** logger: poll
`GPIOA/B/C/D/E_IDT` continuously over SWD while the device boots, log
transitions, and diff a stock boot against ours.

- Covers **all ~80 pins**, not 8 channels. No probes, no package-pin surgery.
- Expect ~1–3 kHz per full five-register sweep. Misses a 25 ns pulse; catches
  anything ms-scale — and everything we're hunting is ms-scale.
- **Validate first:** confirm OpenOCD `mdw` works on a *running* target here.
  Peripheral reads worked while halted in Exp E, but this target has an RDP
  artifact (`pc` reads `0xfffffffe`), so prove the reads are real before
  trusting a capture. Anchor it the same way Exp J anchors FPGA reads.

---

## TASK 3 — FT232H JTAG oracle (only if JTAGSEL_N says go)

Four magnet wires to the **5 gold pads** — real pads, straightforward under the
microscope. Same technique as the SWD wires.

- **Continuity-trace the pads before wiring.** The TCK/TDI/TDO/TMS table above
  is unconfirmed; a TDI/TDO swap looks exactly like "JTAG is disabled."
- **The 5th gold pad is 3V3, NOT GND.** Leave it unconnected. Ground comes from
  the SPI3 pad group / shared rail.
- **4.7 kΩ pulldown on TCK** (apicula's note).
- Share GND only. Never feed power into the board — it powers its own FPGA.

```bash
openFPGALoader -c ft232 --detect                  # GREEN LIGHT = 0x0120681B
openFPGALoader -c ft232 --freq 1000000 --detect   # if flaky, slow down
openFPGALoader -c ft232 -m --file-type bin fpga_bitstream/scope_bitstream_2c53t_v120.bin
```

### 🚫 SRAM ONLY — `-m`, never a flag with `flash` in it
No `-f` / `--write-flash` / `--external-flash` / `--user-flash`. The FPGA's
internal NV flash holds the **only copy of the stock meter design**, and
FLASH_LOCK prevents reading it back. There is no undo. An `-m` SRAM load is
fully reversible — power-cycle and the stock image reloads.

**If the scope comes alive on an SRAM load**, three things land at once: the
bitstream is validated, you get a working configured reference to A/B against,
and the entire scope datapath (0x04/0x05 reads, trigger, timebase, rendering)
unblocks and can proceed in parallel with the config-entry hunt.

---

## TASK 4 — Logic analyzer (last, if ever)

By the time Tasks 1–3 are done you'll know which **one or two** pins to tap, not
seven. One magnet wire is a very different job.

If you do run it:
- **1–4 MHz, not 24.** The boot is 10.5 s; at 8 ch × 2 MHz that's ~21 MB to
  disk, and it dodges the fx2lafw USB bandwidth ceiling. 1 µs resolution is
  ample for ms-scale events.
- **Do not re-capture SPI3.** maksidze's issue-#18 capture is complete and fully
  decoded (`captures/SPI3_STOCK_BOOT_CAPTURE_ANALYSIS.md`). Sampling SPI3 at
  60 MHz would need the /64 prescaler patch again anyway.
- Always burn one channel on **PB6 / CS** as a time reference — without it the
  capture can't be aligned to the config-enable instant.

---

## Standing "do not re-chase" list

- MCU-driven RECONFIG_N GPIO pulses (pin 48 confirmed never pulses)
- The user-mode-lockout / USART-silent theory (bench-refuted 2026-06-13)
- The frontend-strap theory (Exp C)
- A narrow absolute timing window from power-on (Exp B2)
- Static MCU register state (Exp F)
- Re-capturing SPI3
- The ESP32 SSPI route (mooted by Exp J)

## Still genuinely open

- **MODE0 / JTAGSEL_N straps** ← tonight
- `POR=1` with `DONE_FINAL=0` — not what an auto-booted part should report
- AF-mode pins invisible to every static scan (PB9 is AF-PP in stock, floating
  in ours)
- DMA-driven BSRR writes leave no instruction to find
- `FUN_080165A8` — 25,548 bytes, zero callers, never examined
- Paired PC1+PC2 drive (Exp Q pulsed PC1 singly)
- Whether rosenrot00's 2C23T FPGA has an NV image at all, and whether their
  board wires RECONFIG_N — **one question to them could end this**
