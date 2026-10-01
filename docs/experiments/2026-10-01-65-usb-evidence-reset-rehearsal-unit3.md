# EXP-65 — #39 evidence record: does `.noinit` report the previous session correctly across a pinhole reset, and stay silent after a power cycle?

- **Date:** 2026-10-01
- **Unit:** unit #3 (V1.4, Quero)
- **Build:** `make guest-coldtrace-crumbs` at commit `8e692ed` (PR #48: #41 + the evidence record), banner `OpenScope Oct  1 2026 13:45:50`, image `openscope-rp-evidence-crumbs.bin` sha256 `aa6d7089…`, installed over USB with `cdc_flash.py` (PR #46); steps 5–6 on v2 = the same plus `cleared_at=` in `usbstat` (+4 B `.bss`), banner `Oct  1 2026 14:36:14`, `openscope-rp-evidence-crumbs-v2.bin` sha256 `ecf15ceb…`
- **Status:** **CONFIRMED** (the qualitative predictions and both controls; one numeric estimate from PR #48's description missed and is explained post hoc, §5)

## 1. Problem
When the next #39 wedge is recovered with the pinhole reset, will `usbstat` in the new session say what the old one was doing at the stall — and will a power cycle never produce a fake "previous session"? PR #48 had shown survival through one `fwswap b` (SYSRESETREQ issued by the RAM installer, booting back through the factory IAP) only; the recovery everyone uses is the pinhole reset, and nobody had shown the trust rule (magic + FNV-1a) rejecting cold SRAM on this part.

## 2. Hypothesis
With no host having opened the port since enumeration (DTR never raised), the boot banner stalls (host_slow, EPT1 TX VALID) and nothing completes afterwards, so the record is left PENDING. A pinhole reset then starts session N+1, whose `usbstat` prints `previous session #N: STALL PENDING at <tick> ms (tx_completed=0 ept1=0x…3x dtr=0), alive until <later tick>`. PR #48's description had estimated the stall tick at ~600 ms; no stall count was predicted.

- If `.noinit` is cleared by the reset path (factory IAP at 0x08000000 runs first, then the startup code), we see `previous session: none` or a wrong session number.
- If the alive tick is not maintained, `alive until` ≤ the stall tick.
- **Clean control:** same reset after a CDC write has completed since the last stall (the host read a reply) → `previous session #N: no stall pending`. If this also says PENDING, the record reports the procedure, not the port state, and the rehearsal is void.
- **Cold-power control:** no VBUS and PC9 LOW for ~10 s, power on, replug → `previous session: none (cold power-up …)` and `session #1`. If the record validates, the trust rule is not doing its job (or SRAM is retained by the rail), and a fake wedge after a power-up is possible.

## 3. Procedure
Host: macOS, `tools/openscope_host` (`openscope info|shell usbstat`), then `scripts/bench_usb_evidence.py --log dumps/exp65_usbstat.log` (holds the port open with DTR asserted, reconnects after each event and sends `usbstat`). Timestamps below are the script's wall clock. **The version that ran differs from the committed one:** its `wait_for_port` probe-opened and closed the port as soon as it re-enumerated (so the firmware saw DTR 1→0 before the banner); the committed script only lists the port, so a post-event session is expected to show `stalls=1 (host_slow=1) dropped_closed=0` instead of `dropped_closed=1 stalls=0` — run as step 4: confirmed.

1. Rehearsal: all host processes closed (no DTR since enumeration). Operator: pinhole reset, wait ≥3 s, pinhole reset again. Then `openscope info` + `openscope shell usbstat` by hand (output in `dumps/exp65_first_read.txt`; wall time not logged, implied ≈14:01:56).
2. Clean control: `bench_usb_evidence.py` holding the port. Session #4's PENDING had already been cleared by the step-1 readout (a completed reply; `pending=0` at uptime 242.8 s) and again by the holder's own `usbstat` at 14:05:57. Operator: one pinhole reset. Script reconnects and reads `usbstat`.
3. Cold-power control: script still holding the port. Operator (as instructed; reported done, not independently observed): unplug USB, hold POWER (3 s, "Goodbye!", PC9 LOW), 10 s off, POWER on from battery, replug USB. Order differs from the skill's "POWER → Goodbye → unplug": same end state (no VBUS, PC9 LOW), the rails drop at the POWER release instead of at the unplug. Script reconnects and reads `usbstat`; then `fwcrumb` by hand.
4. Committed script (`--once`), started 14:31:35 in the session after the cold cycle. Operator: one pinhole reset (14:40:29). Script reconnects and reads `usbstat`.
5. Install v2 (`cdc_flash.py`: fwload + fwapply, SYSRESETREQ from the RAM installer), then `usbstat` by hand. The only difference to the running build is +4 B of `.bss` (`s_usb.pending_cleared_tick`).
6. On v2: `fwswap b` (reinstalls the same image, SYSRESETREQ), port closed until 5.0 s after it is listed again (so the banner stalls), then `openscope shell usbstat` by hand.

**Preconditions verified by readback**
| what | expected | measured |
|---|---|---|
| build on the device | `OpenScope Oct  1 2026 13:45:50` | `openscope info`: same |
| record live before step 1 | session counter advanced since install (`fwswap b` gave #2) | step 1 reports #3 and #4 |
| EPT1 decode | `USB_TXSTS = 0x30`, `USB_TX_VALID = 0x30` (at32f403a_407_usb.h) | `0x3031 & 0x30 = 0x30` → TX VALID; bits 13:12 = RX VALID; bit 0 = address 1 |
| PENDING clear before step 2 | a completed CDC write since the last stall | `pending=0` in the step-1 and 14:05:57 replies (0 by construction in any delivered reply; the completion itself is the clear) |
| BPR crumb trail present before step 3 | stage-8 trail from `fwswap b` (crumbs build), no `fwcrumb clear` since | **not read** — see §5/§6 |

## 4. Control (recorded in the same session, same path)
| control | expected | measured | passed? |
|---|---|---|---|
| clean: reset with a completed write since the last stall | `previous session #4: no stall pending`; alive tick ≥ 242.8 s (step-1 uptime) + 137.4 s (holder start 14:05:54.8 → loss 14:08:12.1) = 380 s; counters equal to #4's own live `usbstat` | `previous session #4: no stall pending (alive until 618818 ms; stalls=2 heals=0)`: 618.8 s ≥ 380 s ✓ (upper bound not measured); `stalls=2 heals=0` = #4's live line at 14:05:57 ✓ | yes |
| cold power: no VBUS, POWER off ~10 s, on, replug | `previous session: none`, `session #1` | `usb: session #1 … previous session: none (cold power-up, or its record did not validate)` | yes |

## 5. Results
Rehearsal (step 1), read in session #4 at uptime 242.8 s (`dumps/exp65_first_read.txt`):
```
usb: session #4 dtr=1 (seen=1) heal=on
tx: stalls=2 (host_slow=2) consecutive=0 send_err=0 dropped_closed=0 heals=0
last stall: tick=2826 tx_completed=0 ept1=0x00003031 pending=0
proto rx: ok=4 bad_chk=0 bad_len=0 gap_timeouts=0
previous session #3: STALL PENDING at 1824 ms (tx_completed=0 ept1=0x00003031 dtr=0), alive until 5226 ms; stalls=1 host_slow=1 heals=0; rx ok=0 bad_chk=0 bad_len=0 gap=0
```
- Session #3 = between the two resets: lived 5.2 s (operator waited "≥3 s"), one stall at 1824 ms with the TX packet still VALID and DTR 0 (never raised in that session: no host had the port open), the shell loop alive 3.4 s past the stall. That is the host-slow signature, correctly distinguishable from the lost-completion one (`ept1` NAK/disabled) that #39 is about.
- PR #48's description had estimated `STALL PENDING at ~600 ms`; the measured 1824 ms, and session #4's stall count (not predicted), are explained post hoc by the code:
  - `vUsbDebugTask` (task `usb_dbg`) waits 50 idle passes of 10 ms (≈500 ms) after the device is CONFIGURED, then writes the banner; the first 64-byte chunk is armed at once (`g_tx_completed` is 1 after SET_CONFIGURATION) and stays TX VALID; the second waits up to 999 × `vTaskDelay(1)` and the stall is stamped after that: stall ≈ write start + ~1000. So the banner write started at ≈825 ms (derived, not measured): ≈500 ms of settle explained by the code plus ≈0.3 s of boot-to-CONFIGURED enumeration, which is host timing.
  - session #4 counts **2** stalls (last at 2826) where #3 counted 1: `shell_send_banner()` writes the `previous session … STALL PENDING` line **before** the banner when there is one to report, and on a port no host has opened since enumeration each write costs its own stall (a port that was opened and then closed drops writes instead — see the `dropped_closed=1` below). #3 had a clean predecessor, so it only wrote the banner. #4's first stall tick is not recorded (only the last is kept); if its first write started at the same ≈825 ms as #3's, its stalls fall at ≈1824 and ≈2824; the recorded 2826 is consistent with that (1002 after #3's) — an inference, not a check.

Clean control (step 2), script log (`dumps/exp65_usbstat.log` lines 10–19, `last stall`/`proto rx` lines and prompts elided):
```
14:08:12.131 port lost (SerialException); waiting for re-enumeration
14:08:14.787 port back after 2.7 s; settling 1.5 s
14:08:19.189 usbstat after event #1:
  usb: session #5 dtr=1 (seen=1) heal=on
  tx: stalls=0 (host_slow=0) consecutive=0 send_err=0 dropped_closed=1 heals=0
  …
  previous session #4: no stall pending (alive until 618818 ms; stalls=2 heals=0)
```
- `dropped_closed=1, stalls=0` in session #5 (and again in session #1 below): the script version that ran **probed** the port (open + close) as soon as it re-enumerated, so the firmware saw DTR 1→0 before the banner and dropped it (PR #41 drop-when-closed) instead of stalling 1 s on it. An incidental, unplanned confirmation of that path on two fresh sessions; the probe is removed from the committed script because an observer must not perturb the port state it is there to record. These current-session counters are produced by the tool's timing; only the `previous session` line is the evidence.

Cold-power control (step 3), script log (lines 30–39, same elision):
```
14:08:32.539 port lost (SerialException); waiting for re-enumeration
14:09:04.786 port back after 32.2 s; settling 1.5 s
14:09:09.168 usbstat after event #2:
  usb: session #1 dtr=1 (seen=1) heal=on
  tx: stalls=0 (host_slow=0) consecutive=0 send_err=0 dropped_closed=1 heals=0
  …
  previous session: none (cold power-up, or its record did not validate)
```
- 32 s between the unplug and the port returning (hold + ~10 s off + boot + replug, per the instruction; the off time was not measured). The counter restarted at #1: the record did not validate after the rails dropped.
- `fwcrumb` afterwards: `no trail (DT11=0x0000) - no install ran since the last clear/power loss`. A stage-8 trail was *expected* beforehand (PR #48's `fwswap b` ran this crumbs build's RAM installer, which writes DT11=0xFC57 on entry, and nothing but `fwcrumb clear` erases it), but DT11 was **not read before the cycle**, so this is consistent with the power cycle clearing the BPR trail, not a measurement of it. BPR survival across resets is EXP-62's result (SYSRESETREQ and pinhole), not re-read here.

Committed script (step 4), log lines 59–71 (same elision), session #1 = the one started by the cold cycle:
```
14:40:29.829 port lost (SerialException); waiting for re-enumeration
14:40:32.688 port back after 2.9 s; settling 1.5 s
14:40:37.081   < 
14:40:37.081   < +----------------------------------+
14:40:37.081   < |  OpenScope 2C53T Debusbstat
14:40:37.081   < usb: session #2 dtr=1 (seen=1) heal=on
14:40:37.081   < tx: stalls=1 (host_slow=1) consecutive=0 send_err=0 dropped_closed=0 heals=0
14:40:37.081   < last stall: tick=1822 tx_completed=0 ept1=0x00003031 pending=0
14:40:37.081   < previous session #1: no stall pending (alive until 1889612 ms; stalls=0 heals=0)
```
- As stated in §3 before the run: `stalls=1 (host_slow=1) dropped_closed=0`, stall tick 1822 (1824 in session #3, 1823 in step 6: the banner timing is stable to ±1 ms on this host). Session #1 lived 1889.6 s: 14:40:29.8 − 1889.6 s = 14:09:00.2 for its boot, 4.6 s before the port was listed at 14:09:04.8 ✓.
- The reply starts with `\r\n\r\n+----…+\r\n|  OpenScope 2C53T Deb` = exactly 64 bytes, then the `usbstat` echo. That is the banner's first chunk, armed at boot and delivered the moment the host opened the port; the second chunk is the one that stalled and the rest of the banner was never sent (`cdc_send_bytes` returns on the stall). Direct observation of the mechanism assumed in §5 above.

v2 install (step 5), by hand right after `cdc_flash.py` reported the new build (`dumps/exp65_first_read.txt`):
```
usb: session #1 dtr=1 (seen=1) heal=on
tx: stalls=0 (host_slow=0) consecutive=0 send_err=0 dropped_closed=0 heals=0
last stall: tick=0 tx_completed=0 ept1=0x00000000 pending=0 cleared_at=0
previous session: none (cold power-up, or its record did not validate)
```
- **Not predicted:** the previous session (#2 of the old build, reset by the installer's SYSRESETREQ) reads as `none`. Cause, from the ELF: `.noinit` is placed right after `.bss` (`g_usb_ev` = `_ebss` = 0x20037508 in v2), so the 4 B added to `.bss` moved the record by 4 B and the new build validated 44 B that start at the old record's `seq` field. The trust rule did its job (no fake session), but the record is tied to the build's RAM layout: **a firmware update loses it**, and so does `g_fault` (same section). `stalls=0` because `cdc_flash.py` opened the port before the banner's second chunk timed out.

`cleared_at` (step 6), v2, `fwswap b` then port closed for 5.0 s after it was listed:
```
usb: session #2 dtr=1 (seen=1) heal=on
tx: stalls=1 (host_slow=1) consecutive=0 send_err=0 dropped_closed=0 heals=0
last stall: tick=1823 tx_completed=0 ept1=0x00003031 pending=0 cleared_at=5773
proto rx: ok=1 bad_chk=0 bad_len=0 gap_timeouts=0
previous session #1: no stall pending (alive until 100521 ms; stalls=0 heals=0)
```
- `pending=0` as always over CDC, and `cleared_at=5773`: the IN path was dead from 1823 ms until the host's open at ≈5.7 s (5.0 s after listing + boot-to-listing). The field says what `pending=` cannot. `previous session #1` = the v2 install session (≈100 s), so on the same build the record survives the installer's SYSRESETREQ too (as PR #48's `fwswap b` run).

Raw material: `dumps/exp65_usbstat.log` (holder) and `dumps/exp65_first_read.txt` (the by-hand outputs: step 1, `fwcrumb`, steps 5–6) in the bench workspace, not in the repo.

## 6. Blind spots
- The rehearsal stall is a **host_slow** stall (nobody had opened the port). The lost-completion stall (#39 proper: `ept1` TX NAK/disabled, `tx_completed=0`) has still never been seen through this record; EXP-61 could not reproduce the wedge. What is shown is that the record survives and reports the stall snapshot; what the snapshot will say at a real wedge is not.
- `alive until` is the shell loop's tick, updated once per pass; a wedge that blocks the loop itself (not only the IN path) would show `alive` ≈ `stall_tick`, and this run never exercised that reading.
- The cold-power control shows the record *failed to validate*; it does not distinguish "SRAM lost" from "SRAM kept but the IAP/startup touched those 44 bytes". Either way the trust rule gave the safe answer, which is all it claims. A power interruption shorter than the ~10 s used here (quick replug, brown-out dip) might keep SRAM, validate the record and continue `seq`, reading as a reset; not tested.
- The BPR half of the power-cycle statement rests on an expected-but-unread DT11 (§5).
- One unit, one host (macOS). The 2.7 s re-enumeration and the ≈825 ms banner time are host/OS dependent, and so are the record's absolute ticks (`stall_tick` = banner time + ~1 s) and the boot stall counts (session #5 shows 0 stalls only because of the probe); the meaning of the fields (PENDING, EPT1 TX VALID vs NAK, DTR, alive past the stall) is not.
- A reset that lands between an update's first field store and its seal reads as "none" and restarts at session #1. `usb_ev_alive` does one such update per ~10 ms loop pass; the window is a few hundred cycles unless the `usb_dbg` task (priority 2) is preempted mid-update. The three resets here did not hit it; nothing here bounds how often it would.
- The record's address is wherever `.bss` ends, so "previous session" only means anything between two boots of the same build (step 5). A record left by another build is rejected, never misread, but the evidence of the session before an update is gone.
- Steps 2 and 3 were done by the operator from a written instruction; "Goodbye!" on screen, the dark screen and the off time were reported done, not observed by the instrument. Whether the unit powered on from battery or only at the replug does not change the reading (`none`, `#1` either way).

## 7. Conclusion
- **Established:** across the pinhole reset the factory IAP + startup leave the 44-byte `.noinit` record intact (sessions #3→#4→#5 chained, counters and ticks consistent with the operator's timing); `usbstat` reports the previous session's stall snapshot, endpoint state, DTR and alive tick; the same reset after a completed write reports "no stall pending"; 1/1 power-off of ~10 s (unmeasured) with no VBUS gave "none" and restarted at session #1; on v2 the record also survives the installer's SYSRESETREQ, and `cleared_at` reports when a stalled IN path came back (1823 → 5773 ms). The record tells "nobody was reading the port" from "nothing wrong" and from "cold power-up". The banner's first 64-byte chunk is delivered whenever the host first opens the port; the rest is lost to the stall.
- **Excluded:** that the pinhole reset path clears `.noinit` (SYSRESETREQ was already shown in PR #48).
- **NOT excluded (explicitly):** that the lost-completion wedge of #39 leaves a readable snapshot (never observed); that other reset paths (watchdog, brown-out, `fault.c` handler) preserve the record; that a short power interruption keeps SRAM and continues `seq`; that the BPR trail was present before the power cycle; that a record survives a firmware update (shown NOT to, when `.bss` changes size).
- **Follow-up:** leave this build in daily use; at the next natural wedge, press the pinhole reset *without* replugging first and read `usbstat` (`openscope shell usbstat`) — the `previous session` line is the evidence #39 lacks. Give `.noinit` a fixed address so the record (and `g_fault`) survive an update: a small window just below the DFU magic at 0x20037FE0 (`_estack` moved down by its size), after checking that no bootloader on the reset path (factory IAP: SP 0x20001A08, fine; the HID bootloader of the full path: unknown) runs its stack through it. Next cold-power run: read `fwcrumb` right before the power-off. PR #48's description is updated from the "~600 ms" estimate to the measured 1824 ms.
