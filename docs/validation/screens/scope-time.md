# Screen pass: Scope, Time view

| | |
|---|---|
| **Image** | `coldtrace` or `coldtrace-meter`, plus the build line from Settings → About or the `version` shell command |
| **Tier** | 0 for §1–§4; 2 (a signal on CH1) for §5 |
| **Time** | ~20 min |
| **Theme** | Run once in Dark Blue. The theme sweep is its own page. |

Mark each row **✓** (as expected), **✗** (wrong: say what you saw), or **?** (works, but confusing:
say why). "Expect" is what's *right*. Where the code read predicts otherwise, the finding is
named (F-numbers, [findings.md](../findings.md)). Your result confirms it or retires it.

## 1. Arriving

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| ST-01 | Power on | Splash, then the Scope screen within ~2 s; a brief animated "DEMO" trace that disappears when real data arrives | | |
| ST-02 | Look at the info bar (bottom) | `CH1:<V/div> DC or AC`, trigger mode, `H=<time/div>`, `CH2:<V/div> DC or AC`. Readable, nothing overlapping | | |
| ST-03 | On a fresh/erased unit only | Usable V/div and time/div labels, not `--` (**F43** predicts `CH1:--` and `H=--`) | | |
| ST-04 | Look at the top-left | Active channel (`CH1`) and `SPI3:OK n` with n climbing | | |
| ST-05 | Watch it for 10 s with nothing connected | The two traces keep moving (noise), clearly more than a few frames per second | | |

## 2. Each button, once

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| ST-10 | CH1 | Popup `CH1 AC`; info bar shows AC. Again: `CH1 DC`. Never GND. | | |
| ST-11 | CH2 | Same for CH2 | | |
| ST-12 | SELECT | Popup `CH1 probe 10X`; V/div label ×10. Again: back to 1X | | |
| ST-13 | UP ×3, then DOWN ×3 | Popup and info bar step through V/div labels. `~` on provisional ranges, `--` on uncalibrated; never a blank or a stale label | | |
| ST-14 | RIGHT ×3, then LEFT ×3 | `H=` steps: `641us`, `1.28ms`, `2.56ms` … (codes 14+). The popup fits its box (**F20**). | | |
| ST-15 | MOVE ×4 | Popups in order: `UP/DN: Trig level` → `LT/RT: Position` → `UP/DN: V/div  LT/RT: Time`. Each fits its box (**F20**). | | |
| ST-16 | MOVE to Trig level, UP ×4 | Popup `Trig +20 code …`; a dotted level line moves up on CH1 | | |
| ST-17 | MOVE to Position, RIGHT ×3 | Popup `Trig at x=…`; the trigger position moves right | | |
| ST-18 | OK | `STOP` badge; the trace freezes. Press UP: it should stay frozen (**F06**). OK again: `RUN`. | | |
| ST-19 | TRIGGER ×4 | Cursors: vertical pair → horizontal pair → both → off. Each shows its readouts (dt, 1/dt, dV) legibly (**F16**). | | |
| ST-20 | With cursors on: UP, LEFT | UP moves the active cursor; LEFT/RIGHT switch cursors. Is the direction intuitive? | | |
| ST-21 | PRM ×5 | FFT → Split → Waterfall → X-Y → back to Time | | |
| ST-22 | SAVE | A clear message that saving isn't available, fitting its box (**F20**) | | |
| ST-23 | AUTO (nothing connected) | `MASK: teach 8`, then a refusal or a teach that never finishes. Whatever it does, it says so. AUTO again cancels. | | |
| ST-24 | MENU | Goes to Meter | | |

## 3. Reading the screen

| ID | Look at | Expect | Result | Note |
|---|---|---|---|---|
| ST-30 | Top-right badge in NORMAL with nothing connected | Says it's waiting, not `Trig'd` (**F05**) | | |
| ST-31 | Ground markers `1` and `2` on the left edge | Each at its own channel's 0 V, or absent (**F44**) | | |
| ST-32 | Labels inside the trace area (`CH1`, `SPI3:`, mode badge) | Still there after 5 s of live trace (**F17**) | | |
| ST-33 | Measurement badges (Freq, Vpp, Vrms, Duty, Per, CH2pp) | Readable; `--` when unavailable; values change with the signal (**F18**: they may only update on a button press) | | |
| ST-34 | Fonts | Nothing clipped or overlapping anywhere on the screen | | |

## 4. FFT views

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| ST-40 | PRM to FFT. SELECT ×5, UP, DOWN, LEFT, RIGHT | Window name cycles; reference level and zoom respond. The info bar shows FFT details. | | |
| ST-41 | In FFT, press OK | A visible STOP indication (**F19**) | | |
| ST-42 | In FFT, press CH1; then PRM back to Time | Coupling popup shown in FFT, not stale later in Time view (**F15**) | | |
| ST-43 | Time view: cursors on; PRM to FFT; press UP | UP changes the FFT reference level, not a hidden cursor (**F37**) | | |
| ST-44 | X-Y view, then back to Time | Time view unchanged: same trace position (**F38**) | | |

## 5. With a signal (tier 2): 1 kHz sine, 2 Vpp, on CH1

| ID | Do | Expect | Result | Note |
|---|---|---|---|---|
| ST-50 | Set CH1 to `643mV` (range 5), `H=641us` | A still, stable sine. Freq ≈ 1.00kHz, Vpp ≈ 2 V after any button press | | |
| ST-51 | The ▼ marker at the top | Sits on the trigger crossing | | |
| ST-52 | Trigger Mode → Normal (Settings), level above the signal | Trace holds; the badge says it's waiting | | |
| ST-53 | Level back inside the signal | Trace resumes | | |

**Report:** the issue form ("Test report"), or a comment on the tracking issue. Include the ID of
every ✗ and ?, a photo where it's visual, and the build line.
