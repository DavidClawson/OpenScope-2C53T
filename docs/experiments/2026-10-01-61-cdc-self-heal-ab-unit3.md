# EXP-61 — CDC IN wedge (#39): does the self-heal turn it into a reconnect?

- **Date:** 2026-10-01 (planned)
- **Unit:** unit #3
- **Build:** as EXP-60
- **Status:** OPEN

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

## 5. Results
## 6. Blind spots
- The stall snapshot is taken ≥1 s after the loss. A transient EPT state at the moment of loss is not seen.
- A heal that fires on a genuinely slow host would look like success here; host_slow counts are the check.
## 7. Conclusion
