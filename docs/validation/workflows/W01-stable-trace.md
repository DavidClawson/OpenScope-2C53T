# W01: Get a stable trace of a known signal

**Job:** "I connected a signal; show it to me, still and readable, with its frequency and size."
The first thing anyone does with a scope.

| | |
|---|---|
| **Tier** | 2: a function generator (1 kHz sine and square, 2 Vpp) into CH1 with a BNC or probe |
| **Image** | either |
| **Time** | ~10 min |

Mark each step **✓**, **✗** (what you saw) or **?** (what confused you). Also note how
long each step took you without this page: that's the UX measurement.

| Step | Do | Correct result | Result | Note |
|---|---|---|---|---|
| 1 | Power on, connect the 1 kHz sine to CH1 | Within a few seconds, a moving trace on the top half | | |
| 2 | Find the V/div control without the manual | Did you guess UP/DOWN? How long? | | |
| 3 | Set CH1 to `643mV` (UP/DOWN) | The trace fills a good part of the top half | | |
| 4 | Find the time/div control and set `H=641us` | About 3 cycles across the screen | | |
| 5 | Watch 10 s | The trace stands still (triggered), with no jumping or tearing | | |
| 6 | Press any button, read the badges | Freq ≈ 1.00kHz (within 1 %), Vpp ≈ 2 V (within ~10 %), Per ≈ 1.00ms | | |
| 7 | Switch the generator to a 1 kHz square | Stable square; Duty ≈ 50 % | | |
| 8 | Generator to 100 Hz; adjust time/div until ~3 cycles show | Possible within the available steps; Freq ≈ 100 Hz | | |
| 9 | Disconnect the signal | Noise trace keeps updating smoothly (fast), badges go `--` within a few seconds | | |
| 10 | Reconnect | The stable trace returns without any button presses | | |

**Known limits** (not failures):
- Volt labels are only measured on ranges 5–7, and the timebase only from code 13 up.
- Badges refresh on button presses (F18).
- A triggered trace updates ~3–4 times a second; that's the FPGA design's hold (EXP-72).
