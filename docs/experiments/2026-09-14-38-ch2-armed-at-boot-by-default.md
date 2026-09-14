# EXP-38 — CH2 armed at boot by default: does op05 come up live with nothing typed?

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `make guest-coldtrace` (FPGA_CH2_TRIGGER now defaults to 1) vs
  `make guest-coldtrace-noch2` (the control image, `-DFPGA_CH2_TRIGGER=0`),
  branch `bench/2026-09-14`; both 614,428 B (coincidence — the images differ in
  58,567 bytes and the ON image carries exactly one extra `bl` to
  `scope_trigger_ch2_init` / `scope_trigger_ch2_raw`, checked by objdump)
- **Script:** `scripts/exp38_ch2_boot_arm.py`
- **Status:** **CONFIRMED** — control passed, ON boot live, OFF boot dead as predicted (A/B, first boot after each flash)

## 1. Problem

CH2 was solved in two halves in August: the PC1/PC2 channel mask (EXP-01/07,
2026-08-17) and the TMR13/PA6 vertical-offset arm (EXP-07 confirmed the pin;
EXP-21 read an independent CH2 tone and drew a Lissajous). Yet every plain
`guest-coldtrace` note since — EXP-28 on 2026-09-12, EXP-29 on 2026-09-13 —
records "op05 all zeros". The arm was compiled in only behind
`FPGA_CH2_TRIGGER`, which only `guest-coldtrace-ch2` set. Does flipping that
default make CH2 live from a cold boot with no shell command sent?

## 2. Hypothesis

**If the default reaches the boot path:** on a fresh boot of the ON image,
before any `trig2` is sent, TMR13 `CTRL1.CEN`=1, `C1DT`=2544, PA6 is AF-PP, and
`op05` carries the CH2 drive (peak at the 2 kHz bin) while `op04` carries the
CH1 drive (1 kHz bin).

**If it does not** (block not compiled, or PA6 reclaimed as GPIO after the arm
by a later `gpio_init` in `fpga_init` — pre-run I believed there were three
such PA6 sites; they turned out to be PB6/PE6, see §5): `op05` span 0 on the
ON image, or `C1DT`=0 / PA6 nibble ≠ AF.

**OFF image prediction:** `CTRL1`=0, `C1DT`=0, PA6 = GPIO output, `op05` span 0
— the state every "CH2 dead" note was taken on.

## 3. Procedure

- Drive: JDS6600 (crystal-accurate in frequency), CH1 = 1 kHz, CH2 = 2 kHz,
  3 Vpp sine, both outputs on — EXP-21's rig. JDS6600 → scope CH1 (BNC) and
  scope CH2.
- Scope: `fpga scope vdiv 1 5`, `fpga scope vdiv 2 5` (range 5, 21–23 mV/count,
  the range the 2544 boot code was centred for), `fpga scope timebase 10`
  (12,490 S/s, non-tearing for `opread`). Expected bins: 1 kHz → 82.0,
  2 kHz → 164.0 of 1024.
- Readback (side-effect-free): `mem read 0x40001C00 1` (TMR13 CTRL1),
  `mem read 0x40001C34 1` (C1DT), `mem read 0x40010800 1` (GPIOA CFGLR, PA6 =
  bits 27:24). **The `trig2` shell handler calls `scope_trigger_ch2_init()`
  unconditionally before parsing its arguments**, so it is never sent in a
  boot phase before the op05 record; the register readback is the only
  precondition source.
- Records: 5 × `spi3 opread 04/05 1026 dump` per channel; mean, span, peak bin
  (peak *search*, `bench.peaks`).
- Flash path: factory IAP (`scripts/iap_flash.py flash <image>`), MENU + tap
  Power. An IAP flash on our images reboots straight into the app (Exp R), and
  an MCU reset zeroes TMR13, so an IAP reboot is a valid "cold" for the arm
  question (it is NOT an FPGA power cycle — see blind spots).

**Preconditions verified by readback** (control phase, image on the device at
session start — a coldtrace build with op05 zeros):

| what | expected | measured |
|---|---|---|
| TMR13 CTRL1 before any arm this run | — | `0x00000001` (CEN=1), C1DT=**0** |
| PA6 CFGLR nibble | — | `0x9` (AF-PP 10 MHz) |
| op04 baseline | 1 kHz tone | mean 85.4–85.8, span 151–152, peak bin 81 ×5 |
| op05 baseline | zeros | mean 0, span 0 ×5 |

The CEN=1 / C1DT=0 pre-state is explained: earlier in this session two
argument-less `trig2` probes were sent from the shell, and the handler inits
TMR13 before it parses. Duty 0 leaves CH2 at rail — op05 zeros — which is
also a reminder that "TMR13 running" is not "CH2 armed".

## 4. Control

Run first, same session, same path, on the image found on the device.

| control | expected | measured | passed? |
|---|---|---|---|
| (a) DAC1 sweep: `trig raw 1024 / 2048 / 3072` → op04 mean | monotonic | **6.6 → 85.2 → 206.8** | PASS |
| (b) runtime arm `trig2 raw 2544` → op05 | 2 kHz tone appears | readback C1DT=2544; op05 mean 134.0–134.3, span 154–155, **peak bin 163 ×5** | PASS |
| (b′) op04 unchanged by (b) | 1 kHz tone stays | mean 85.1–85.7, span 152–153, peak bin 81 ×5 | PASS |

The instrument can see the offset path move (a) and can see CH2 wake (b), so
an "op05 zeros" from a boot phase would be a real negative.

## 5. Results

### ON image (`guest-coldtrace`, FPGA_CH2_TRIGGER=1) — first boot after IAP flash

Registers read **before** `vdiv`/`timebase` and with no `trig2` ever sent:

| what | predicted | measured |
|---|---|---|
| TMR13 CTRL1 | CEN=1 | `0x00000001` |
| TMR13 C1DT | 2544 | **2544** |
| PA6 CFGLR nibble | AF-PP | `0x9` (AF-PP 10 MHz) |

| channel | mean | span | peak bin ×5 | expected bin |
|---|---|---|---|---|
| op04 (CH1, 1 kHz) | 85.2–85.5 | 151–153 | 81 ×5 | 82.0 |
| op05 (CH2, 2 kHz) | **134.0–134.4** | **153–155** | **163 ×5** | 164.0 |

Both channels live, different tones, no shell arm. Numbers match the
control-phase runtime arm to within a count (op05 mean 134.0–134.3 there),
i.e. the boot arm lands the same operating point `trig2 raw 2544` does.
→ **CONFIRMED** for the ON image.

### OFF image (`guest-coldtrace-noch2`) — first boot after IAP flash

| what | predicted | measured |
|---|---|---|
| TMR13 CTRL1 | 0 | `0x00000000` |
| TMR13 C1DT | 0 | 0 |
| PA6 CFGLR nibble | GPIO output | **`0x4` (floating input)** — prediction wrong in detail, see below |

| channel | mean | span | peak bin ×5 |
|---|---|---|---|
| op04 (CH1, 1 kHz) | 85.1–85.8 | 152–153 | 81 ×5 |
| op05 (CH2, 2 kHz) | **0.0** | **0** | 512 ×5 (no peak) |

CH1 identical to the ON image; CH2 exactly the "all zeros" of every earlier
plain-coldtrace note. Same drive, same range, same timebase, same session —
the single variable is the compiled-in boot arm.

**The PA6 detail matters.** I predicted the OFF image would leave PA6 as a
GPIO output because `fpga_scope_frontend_enables()` has always executed
`GPIOA->scr = (1U << 6)` "PA6 analog enable". It reads back as a **floating
input** — nothing in the OFF image ever configures PA6's mode (my earlier
grep for `GPIO_PINS_6` had matched the PB6 chip-select init, not PA6). On
this family ODR is ignored in floating-input mode, so that write has been a
no-op for the whole life of the coldtrace build. Fifth instance this year of
a control that looked correct because it was inert; it is now compiled out
alongside the arm and the comment says why.

## 6. Blind spots

- **Not an FPGA power cycle.** IAP reboot resets the MCU only; the FPGA keeps
  its configuration and its arm state. This test asks about the MCU-side arm
  (TMR13/PA6), which an MCU reset does zero, so the question is answerable —
  but it says nothing about CH2 across a true power cycle (POWER → Goodbye →
  unplug → replug). Worth one repeat on a true cold boot.
- **Channel mask not read back.** PC1/PC2 (mask 3) is applied by
  `fpga_set_scope_frontend_ranges()` at boot in both images; this test infers
  it from op05 carrying a *different* tone than op04, not from the pins.
- **Range 5 only.** The 2544 centring code is range-5-specific; other ranges
  boot off-centre until `fpga scope center ch2` runs (known, EXP-07).
- **Display not checked.** Everything here is `opread`; whether the UI draws
  the CH2 trace is a separate (weak, screen) observation, recorded if seen.

## 7. Conclusion

- **Established:** with `FPGA_CH2_TRIGGER` defaulting to 1, a plain
  `guest-coldtrace` boot arms TMR13 (CEN=1, C1DT=2544, PA6 AF-PP) before any
  shell command and CH2 is live: op05 carries its own 2 kHz tone at bin 163
  while op04 carries 1 kHz at bin 81, five of five records each, at the same
  operating point the runtime `trig2 raw 2544` reaches (mean 134). Both
  channels from a cold boot, no shell — the last CH2 gap in the default image
  is closed.
- **Established:** the OFF image reproduces the historical "CH2 all zeros"
  exactly, so those notes were the missing boot arm and nothing else.
- **Excluded:** PA6 being reclaimed as GPIO after the arm (the three later
  `GPIO_PINS_6` sites in `fpga_init` are PB6/PE6, not PA6); the old
  `GPIOA->scr` "PA6 enable" doing anything (floating input, ODR ignored).
- **NOT excluded (explicitly):** behaviour across a true FPGA power cycle
  (this was an MCU reset via IAP); centring at ranges other than 5; whether
  the UI draws CH2 (not looked at — `opread` only).
- **Follow-up:** re-run the CH2 attenuator ladder (EXP-06/08) now that the
  arm is default — the "only one usable CH2 tap" result was taken unarmed;
  one repeat of the ON phase after a true power cycle; fold
  `fpga scope center ch2` into boot if other ranges need it.
