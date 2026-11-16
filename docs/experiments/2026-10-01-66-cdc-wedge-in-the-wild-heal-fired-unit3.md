# EXP-66 — #39 in the wild: a lost-completion wedge mid-`opread`, and the self-heal recovered the port

- **Date:** 2026-10-01, 16:49 (during EXP-64 run 2)
- **Unit:** unit #3 (V1.4)
- **Build:** `guest-coldtrace-crumbs` of PR #48's branch @ `f912890` (= #41 + evidence record + `cleared_at`), `Build: Oct  1 2026 14:36:14`
- **Status:** **CONFIRMED (one natural occurrence; the triggering stall's snapshot was overwritten — see §6)**
- **Note (2026-10-05, rebase onto 9170a50):** `FWL_INSTALL_CRUMBS`, `fwcrumb` and `make guest-coldtrace-crumbs` no longer exist on this branch; they were dropped in favour of main's c4e40bd (installer record in BPR DT1..5, read with `fwstat`), as agreed in #45. The "crumbs" build named here was guest-coldtrace with that installer trail on; the evidence record measured here does not depend on it. Commit ids in this file are the pre-rebase ones (see PR #48 for the current branch). The record is kept as it was measured.

## 1. Problem
#39: the CDC shell goes silent while the UI stays alive, and only replug + reset brought it back. PR #41 added a watchdog (two consecutive stalls with the IN endpoint not VALID and `g_tx_completed` still 0 → USB soft disconnect/reconnect) that EXP-61 could not exercise: the wedge did not reproduce on demand (48 MB clean). Does the heal fire on a real wedge, and does the port come back without a human?

## 2. Hypothesis
Not pre-registered: this event was not planned. Read against PR #41's design: if a wedge is a lost completion, the counters show stalls that are **not** host_slow, `consecutive` reaches 2, `heals` increments, the device drops off the bus for ~200 ms and re-enumerates with the same session number (no reset, uptime continuous). If instead the port had wedged without the heal firing, the host would have found an enumerated-but-silent device, as in EXP-59.

## 3. Procedure
None chosen. `verify_scope_cal.py --center-mid --ranges 5 6 7 8 9 --center-path opread` (EXP-64 run 2) was reading `spi3 opread` windows (1026-byte hex dumps) through the shell; the host is macOS, pyserial, the port held open for the whole run. The wedge happened on the first `opread` of range 8. Readback afterwards by hand: `openscope info`, `openscope shell usbstat`.

**Preconditions verified by readback**
| what | expected | measured |
|---|---|---|
| build / session | the EXP-65 session #2 build still running | `OpenScope Oct  1 2026 14:36:14`, `session #2`, uptime 7609.6 s at readback (booted ≈ 14:45, the `fwswap b` of EXP-65 step 6) |
| counters before the run | EXP-65 step 6 readback: `stalls=1 (host_slow=1) … heals=0` at 5.8 s; by 2887 s: `stalls=2 (host_slow=2) … heals=0` | — |

## 4. Control
The negative control is every prior hour of the same session: 2 stalls, both host_slow (banner stalls on a closed port), no heal, through EXP-63's five runs (~30 MB of hex dumps over the same path) and EXP-64 runs 1 and 1b. The heal had never fired in ~2 h of this session, or in EXP-61's 48 MB on this branch and on v0.4.0.

## 5. Results
Host side, 16:49:4x (`dumps/exp64_run2.log`):
```
bench: opread 04: asked for 1026 bytes, parsed 0 — window unusable
```
About 90 s later no `2e3c:5740` device was on the bus (`find_port`: ports seen = the Dot and a CH340 only); about 60 s after that it was back on the same node. Readback (`dumps/exp66_readback.txt`):
```
firmware  OpenScope Oct  1 2026 14:36:14
mode      scope
uptime    7609.6 s
usb       4 TX stalls, 1 self-heals

usb: session #2 dtr=1 (seen=1) heal=on
tx: stalls=4 (host_slow=2) consecutive=0 send_err=0 dropped_closed=1205 heals=1
last stall: tick=7574233 tx_completed=0 ept1=0x00003031 pending=0 cleared_at=7609577
proto rx: ok=30 bad_chk=0 bad_len=0 gap_timeouts=0
previous session #1: no stall pending (alive until 100521 ms; stalls=0 heals=0)
```
- `heals=1`: **the self-heal fired**, for the first time on hardware. `stalls=4`, `host_slow=2`: two stalls were lost completions (endpoint not VALID with `tx_completed=0`), consecutive, which is exactly the trigger. No reset: uptime 7609.6 s, `session #2` unchanged, `previous session` still EXP-65's #1.
- The device re-enumerated by itself; the next host open (`openscope info`, tick 7609577) completed a send (`cleared_at`). No replug, no reset, no button.
- `last stall: tick=7574233 ept1=0x00003031` (TX VALID → host_slow): this is **not** the wedge's stall. After the heal the host had abandoned the port (the script aborted and closed it), so the device's next write stalled as "nobody reading" and that snapshot replaced the lost-completion one. 7574.2 s after boot ≈ 16:51:1x, consistent with a write made after the reconnect. `dropped_closed=1205`: once a host had opened and closed the port after the heal, the device dropped 1205 queued lines (the rest of the opread dump and prompts) instead of stalling on each.
- Host-side consequence: `bench.Scope.opread` got an empty window and the script, which has no reconnect path, aborted (EXP-64 run 2 ends at r7).

## 6. Blind spots
- **The triggering stall's snapshot is gone.** The record keeps only the *last* stall; two host_slow stalls after the heal overwrote the lost-completion `ept1` value that the whole #39 investigation is after. What is left is the arithmetic (4 − 2 = 2 non-host_slow stalls) and `heals=1`. The firmware should keep the stall that fired the heal separately (follow-up, §7).
- One occurrence, one host, one unit; the wedge's cause is still unknown. That it struck during a hex dump at ~60 KiB/s, like the four EXP-59 wedges, is consistent with "heavy IN traffic" and proves nothing.
- Whether the heal was *necessary* (a lost completion that would never have completed) or a 2 s pause that would have resolved on its own cannot be told from this record; the pre-heal EXP-59 wedges never resolved in minutes, which is the only evidence that a stall of this kind is permanent.
- The ~90 s with no device on the bus is longer than a 200 ms heal: macOS's re-enumeration after a soft disconnect, or the host's own port teardown, or several heal cycles (only one counted) — not resolved; the timestamps are coarse (one `find_port` call).

## 7. Conclusion
- **Established:** a natural #39-class event (two consecutive lost-completion stalls on the CDC IN endpoint) occurred on unit #3 at 16:49 on 2026-10-01 during an `opread` dump; PR #41's self-heal fired once and the port came back without a reset or a replug; the evidence record survived and `usbstat` reports the event's counters.
- **Excluded:** that the heal path cannot fire on hardware; that a wedge of this kind resets the device (uptime continuous).
- **NOT excluded:** the wedge's mechanism; that the heal fired on a stall that would have resolved by itself; the exact dead time on the host side.
- **Follow-up:** (1) firmware: snapshot the heal-triggering stall into its own fields of the evidence record (`heal_stall_tick`, `heal_stall_ept`), never overwritten by later host_slow stalls — the fields that would have answered #39 today. **Done:** commit `170d9f3` (record 44 → 52 B, `usbstat` prints `this session heal: fired at <ms> on ept1=0x… (TX not VALID: lost completion)`), installed on unit #3 at 17:0x as `openscope-rp-evidence-crumbs-v3.bin`; the old build's record read as `none` after the install (layout changed, as EXP-65 found). (2) bench: `bench.Scope.opread` reopens the port once and retries an EMPTY window — **done**, commit on `bench/signal-source-abstraction`. (3) comment on #39 with this record (pending the operator's go).
