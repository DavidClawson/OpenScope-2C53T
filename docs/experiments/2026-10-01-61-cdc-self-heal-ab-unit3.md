# EXP-61 — CDC IN wedge (#39): does the self-heal turn it into a reconnect?

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build:** as EXP-60
- **Status:** **VOID (control failed: the wedge did not reproduce today, on either firmware)**

## 1. Problem
#39: long `flash dump` runs wedge the CDC shell (UI alive, port silent until replug). Is it a lost IN completion (endpoint NAK/disabled, g_tx_completed stuck at 0), and does the soft reconnect recover it without a human?

## 2. Hypothesis
If it is a lost completion, `usbstat` after a wedge shows `tx_completed=0` with EPT1 TX status NAK/disabled (not VALID), and with heal on the host sees one reconnect per wedge while the dump completes. If the endpoint is VALID at the stall, the host stopped reading and the wedge is host-side: the heal correctly does nothing.

## 3. Procedure
`bench_remote_protocol.py --only crumbs,soak --soak-mb 16`: heal **off** first, then heal **on**, 1 KB requests. Replug + reset if the heal-off run wedges (expected).

## 4. Control (record first)
| control | expected | measured | passed? |
|---|---|---|---|
| heal OFF reproduces the wedge through this exact path | ≥1 stall that needs a replug within 16 MB (4 wedges in 4 runs on 2026-10-01) | | |
| counters readable | `usbstat` answers before the run | | |

If heal-off does not wedge within the soak, the heal-on result is **VOID** (nothing to heal), not a success.

## 4b. What was run
| run | firmware | host pattern | result |
|---|---|---|---|
| 1 | this branch (crumbs v1, heal **off**) | `bench_remote_protocol.py` soak: 1 KB requests, CR only | 16 MB, 0 stalls, 25 KiB/s |
| 2 | this branch (crumbs v1, heal **off**) | `w25q_dump.py`: 4 KB requests, CRLF (last night's exact script) | 16 MB, 0 retries, 58 KiB/s |
| 3 | **release v0.4.0** (the firmware that wedged 4× last night) | same as run 2 | 16 MB, 0 retries, 61 KiB/s, scope mode (mode=0) |

## 5. Results
- 48 MB over the same path that wedged 4 times in ~20 MB last night (EXP-59 session: after ~1.0, 6.0, 8.1, 4.4 MB), with zero stalls today on both firmwares. The only stall counted today was the banner sent before the host opened the port (`ept1=0x00003031`: TX VALID, `host_slow=1`), classified correctly as "host not reading".
- The heal-ON run was not performed: with no wedge under heal OFF it would have been VOID.

## 6. Blind spots
- Whatever triggered last night's wedges was not present today and is not identified (candidates: operator button presses during the dump — true for the first wedge; thermal/USB-host state; a device uptime effect).
- The counters work; the heal path has never fired on hardware.

## 7. Conclusion
- **Established:** the #39 wedge is intermittent; it is not reproduced by the data path or the request pattern alone.
- **NOT established:** that this branch fixes or heals it. The watchdog stays as instrumentation (`usbstat` will show the endpoint state at the next wedge) and the heal stays opt-in-by-default but unvalidated.
- **Follow-up:** leave `usbstat` counters running in daily use; on the next wedge, read them before replugging.
- The stall snapshot is taken ≥1 s after the loss. A transient EPT state at the moment of loss is not seen.
- A heal that fires on a genuinely slow host would look like success here; host_slow counts are the check.
## 7. Conclusion
