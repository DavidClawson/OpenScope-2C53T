# W02: AC/DC coupling and the 10× probe

**Job:** "Look at a small ripple riding on a DC level" and "use my 10× probe and still read real volts."

| | |
|---|---|
| **Tier** | 2: a function generator with DC offset (or a battery for the DC part); a 1×/10× switchable probe |
| **Image** | either |
| **Time** | ~10 min |

| Step | Do | Correct result | Result | Note |
|---|---|---|---|---|
| 1 | 1 kHz sine, 0.5 Vpp, **+1 V offset** into CH1, DC coupling, `H=641us` | The sine sits above the centre; Vpp ≈ 0.5 V | | |
| 2 | CH1 (button) → AC | Popup `CH1 AC`; the sine re-centres; Vpp still ≈ 0.5 V | | |
| 3 | Offset 0 → +2 V on the generator, in AC | The trace jumps, then settles back to centre within ~a second | | |
| 4 | CH1 → DC | Popup `CH1 DC`; the trace moves with the offset again | | |
| 5 | Same on CH2 (signal moved to CH2) | Same behaviour | | |
| 6 | Coupling via Settings → Oscilloscope Settings | The Scope screen agrees with the menu | | |
| 7 | Probe switch at **10×**, signal 2 Vpp; SELECT on the scope → `10X` | Vpp ≈ 2 V again (not 0.2), V/div labels ×10 | | |
| 8 | Scope at `10X` but probe at **1×** | Vpp reads ×10 too high: proves the setting is applied | | |
| 9 | Power-cycle | Coupling and probe settings are remembered | | |

**Known limits:** there's no GND coupling, and the 20 MHz limit is `n/a` (EXP-70).
