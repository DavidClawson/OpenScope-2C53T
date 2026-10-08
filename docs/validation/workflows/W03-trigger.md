# W03: Trigger level, edge, mode and a single shot

**Job:** "Make the trace start where I want" and "catch one event and keep it."

| | |
|---|---|
| **Tier** | 2: a function generator (sine, then a slow square or burst) |
| **Image** | either |
| **Time** | ~15 min |

| Step | Do | Correct result | Result | Note |
|---|---|---|---|---|
| 1 | 200 Hz sine, 2 Vpp on CH1, `643mV`, `H=1.28ms`; MOVE once (trigger level stage) | Popup `UP/DN: Trig level`; a dotted level line on CH1 | | |
| 2 | UP ×4, then DOWN ×8 | The line moves, the trigger point (▼) follows the crossing, and the trace stays stable while the level is inside the signal | | |
| 3 | Level above the top of the signal, mode **Auto** | The trace keeps updating (untriggered, may drift); the badge says untriggered | | |
| 4 | Same level, mode **Normal** (Settings → Trigger Mode) | The trace holds the last triggered capture; the badge says waiting | | |
| 5 | Level back inside | Triggered trace resumes | | |
| 6 | Trigger Edge: Rising, then Falling (Settings; **F14** says the row may be invisible) | The trace at the ▼ goes up through the level, then down | | |
| 7 | MOVE to Position stage, LEFT/RIGHT | The trigger point moves horizontally; the waveform follows | | |
| 8 | Mode **Single**; generator to a 1 Hz square | One capture, then it holds | | |
| 9 | Catch a second event | A clear way to re-arm. (**F32**: only changing the mode re-arms.) | | |
| 10 | OK (STOP) in Normal with a live signal; change the signal; press UP | The stopped trace does not change (**F06**) | | |

**Known limits:**
- The FPGA fires on either edge; Rising/Falling is applied by the MCU (EXP-55, EXP-73).
- Triggered updates are ~3–4/s (EXP-72).
