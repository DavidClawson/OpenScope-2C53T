# Warm-Handoff FPGA Read Test (no hardware required)

**Goal:** test whether *our* firmware's SPI3 read path works against a
scope-configured FPGA — **before** the FT232H/JTAG hardware arrives — by letting
**stock** configure the FPGA, then handing off to our firmware **without cutting
power** so the FPGA keeps its config.

## Why this works

- Stock firmware successfully uploads the scope bitstream to the GW1N at boot
  (the `0x3B` SSPI sequence we can't make take). After that the FPGA is running
  the **scope** design.
- The GW1N holds its SRAM config **as long as VCC is maintained**. An MCU
  *soft-reset* (reflash + reboot) does **not** power-cycle the FPGA, and
  RECONFIG_N stays HIGH (maksidze confirmed). So the scope config survives a
  firmware swap.
- Our `FPGA_WARM_HANDOFF_TEST` build comes up **read-only**: it sets up SPI3 +
  PC6 (enable) + PB11 (active) to match stock's scope-run posture, but skips the
  SSPI config sequence, the boot/meter USART commands, the analog-frontend
  relays, and all auto-tasks — i.e. it touches nothing that could knock the
  FPGA out of scope mode. We then read manually with `spi3 acqread`.

If `spi3 acqread` returns live samples, **our downstream read path works** and
the *only* remaining problem is config-entry (which JTAG will solve). If it
doesn't, we learn whether the config survived the handoff vs. our read framing
is wrong — both actionable.

## What you need

- **Unit #2** (the "guest" unit: stock FNIRSI IAP bootloader at `0x08000000`,
  app at `0x08007000`). The handoff relies on the stock IAP drive for a
  no-power-loss reflash.
- The stock app binary: `archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin`.
- The serial debug shell (`scripts/serial_cmd.py` over the USB CDC port).

## Procedure

**1. Put stock on the guest unit (configures the FPGA).**
If the guest slot currently holds our firmware, flash stock back first:
```
! python3 scripts/iap_flash.py flash "archive/2C53T Firmware V1.2.0/APP_2C53T_V1.2.0_251015.bin"
```
(MENU+Power → upgrade mode, then detect → pick → flash. Needs sudo for the raw
disk write — run it yourself.)

**2. Boot stock, confirm the FPGA is configured.**
Let it boot fully (~5 s — stock uploads the bitstream at ~3.6 s automatically).
Enter **scope mode** and confirm a live trace. That proves the FPGA is now
running the scope design. **Leave the device powered on from here — do NOT
unplug or power-cycle.**

**3. Build the warm-handoff test firmware.**
```
cd firmware && make guest-warmtest
```

**4. Hand off to our firmware WITHOUT cutting power.**
With the device still on, enter stock upgrade mode (**MENU+Power** → the `IAP`
drive mounts — this is a *soft* reset, FPGA stays powered), then flash:
```
! python3 scripts/iap_flash.py flash firmware/build/firmware.bin
```
The device soft-reboots into our read-only test firmware. The FPGA never lost
power, so its scope config is (hopefully) intact.

**5. Read the FPGA.**
Open the shell and run:
```
spi3 acqread
```
Optionally feed a known signal into CH1 first (siggen or a DC source) for an
unambiguous result.

## Reading the result

`spi3 acqread` prints, per channel (0x04=CH1, 0x05=CH2): the 3 status bytes,
PC0 before/after, the first 16 samples, and `min/max/mean/span`.

| Result | Meaning | Next |
|---|---|---|
| **status `?? ?? 01`, span > 0, sane samples** | ✅ **WIN** — our read path works on a configured FPGA. Config-entry is the only remaining problem. | Rewrite `fpga_acquisition_task` from the old `0x80\|range` to the real `0x04`/`0x05` protocol so the scope UI shows the trace. |
| status sane (`?? ?? 01`) but **span = 0 / samples all `00`** | FPGA responds but buffer empty — configured, not capturing | May need a trigger/run nudge, or the design needs a USART arm we skipped. Try a single safe `spi3 xfer`/USART run cmd. |
| **status `FF FF FF`, samples all `FF`** | FPGA mute / high-Z | Run `spi3 gowin` (reads Gowin STATUS/IDCODE). If it shows configured → pins/CS issue. If `FF`/zero → the config did **not** survive the handoff (the reset disturbed it) — this experiment can't work and we wait for JTAG. |

`spi3 gowin` is the disambiguator: it tells us whether the FPGA is still
configured independent of the acq read framing.

## Notes / caveats

- This is **guest-unit (#2) only** — it needs the stock IAP no-power-loss
  reflash path. Don't try it on the HID-bootloader unit.
- The scope UI will look idle in this build (no acquisition task by design) —
  that's expected; read via the shell.
- Revert to a normal build any time with `make guest` (the flag defaults to 0;
  normal builds are unchanged).
- If acqread fails, it is **not** destructive — power-cycle and the FPGA reloads
  its NV meter design as always.

---

## RESULTS (2026-06-13, Unit 2) — ✅ read path validated on real waveform data

Three rounds. Headline: **our `0x04`/`0x05` read path reads genuine scope
samples from a stock-configured FPGA.** First time this firmware has ever pulled
real FPGA scope data.

**Round 1** (warm-test build, handoff *with* a charging-logo POWER press): first
`acqread` returned real, channel-distinct data — CH1 ≈173 / CH2 ≈82, a few LSB
noise (span 7/3). Every subsequent read all-zero. One buffer, then idle.

**Round 2** (drove PC6/PB11 HIGH + PC11 LOW at the very top of `main()` to
shrink the float window from ~2 s to the reset glitch; also POWER-press handoff):
*identical* — one real buffer (CH1 ≈173 span 4, CH2 ≈83 span 7), then zeros.
**The fast pin-restore changed nothing → the one-buffer limit is NOT the float
window.**

**Round 3 — the decisive one** (3 V p-p **sine** fed into CH1 on stock; handoff
booted **straight through with no POWER press = clean soft reset**): first
`acqread` CH1 returned a **smooth rising ramp** —
`A7 A7 AA AD B0 B1 B1 B3 B5 B8 BA BC BE BF C1 C4` (167→196), `span=81`,
min=133/max=214 — **the rising edge of the sine.** CH2 flat (span 3, nothing
connected). Unmistakably the captured waveform, read through our path.

### What this establishes

1. **The scope config survives a *clean* soft handoff** (no power-down). The
   SRAM bitstream is intact — we read its captured buffer. (The earlier
   POWER-press rounds were the disturbed ones; round 3's straight-through boot
   was clean.)
2. **Our `0x04`/`0x05` read framing is correct** — validated against a known
   waveform, not just noise.
3. The persistent **one-buffer limit is a run-state problem, not a read or
   config-survival problem.** The "keep capturing" state doesn't survive the MCU
   reset (and stock's siggen also stops at handoff, so there's no fresh signal
   to capture afterward). Re-arm attempts (stock's post-config SPI3 writes, the
   scope USART config, PB11/PC11 toggles) all failed to restart continuous
   capture.

### Implication for JTAG

JTAG provides exactly what the warm-handoff can't: a **fresh** config
(run-state established) + free-running capture + **no MCU reset afterward**. With
the read path now validated, JTAG should yield **continuous** live traces. The
`fpga_acquisition_task` rewrite to `0x04`/`0x05` is the firmware follow-up,
validated and ready for that bench session.

---

## RESULTS (2026-06-13, Round 4) — `spi3 armtest`: MCU-pin re-arm falsified

Tested the netlist-derived runtime-arm hypothesis from
`reverse_engineering/analysis_v120/mcu_fpga_boundary_reconcile_2026-06-13.md` §5:
the apicula trace says MISO (`SO.OEN`) is gated by a read-window counter that only
advances while the FPGA run/re-arm pad (`IOR1B`) is driven, and that re-arm runs
through an async-preset (`8× DFF.SET`) *pulse* path — so a held-HIGH level may
never restart capture after the first window. `IOR1B` was mapped (semantically,
not by board trace) to PB11 (#1) or PC6 (#2). New shell command `spi3 armtest
[pb11|pc6]` (in `usb_debug.c`): baseline acqread → pulse the run pin
HIGH→LOW→HIGH ×3 → re-issue the post-config control register
(`01 08 / 02 03 / 06 00 / 07 00 / 08 AD`) → acqread again.

Procedure was a **clean** warm handoff (stock → live scope trace → MENU+Power →
flash `guest-warmtest`, booted straight through, no POWER press).

- **Handoff confirmed good:** the PB11-run baseline read returned the stale
  one-shot buffer — CH1 `AE`(174) / CH2 `53`(83), matching Round 1's DC levels.
  Read path + `0x04`/`0x05` framing re-validated.
- **PB11 pulse + control-reg → no re-arm.** After-pulse read empty (span 0) on
  both channels; the single buffer was consumed and none regenerated.
- **PC6 pulse + control-reg → no re-arm.** After-pulse read empty on both
  channels (independent of the already-consumed baseline buffer).
- The read *window* still fires throughout (CH1 status `80 00 00`, PC0 1→0) —
  the readback path is alive; there is simply no fresh capture behind it.

**Conclusion:** continuous capture is **not re-armable from either MCU-reachable
dedicated control pin** (PB11, PC6) via an edge pulse + control-register
re-issue. Combined with the earlier PB11/PC11-toggle failures, MCU-side re-arm
after a soft reset does not restart capture. Per the boundary doc's decision
tree this points to `IOR1B` being an **unbonded top-edge IOT pad** (needs a board
trace) and/or the run-state being fundamentally non-re-establishable from the MCU
post-reset — i.e. **JTAG remains the unlock** (fresh config + no MCU reset).
`spi3 armtest` stays in the tree as a ready probe for the JTAG session (where a
fresh config means the baseline should free-run, not one-shot).

> Note (`spi3 gowin` in this state): once the scope **user design** is running it
> owns the SSPI port, so config-register reads (`0x11`/`0x41`) no longer reach the
> config engine — they returned all-`00` here, *unlike* the all-`FF`/valid-IDCODE
> seen when only the NV meter design is loaded. All-`00` on gowin is therefore a
> rough *positive* indicator that a user design has the port, not a failure.
