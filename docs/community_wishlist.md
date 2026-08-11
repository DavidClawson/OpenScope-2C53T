# Community Wishlist — Firmware Roadmap from Real Owner Demand

**What this is:** a prioritized, evidence-backed list of what FNIRSI 2C53T (and sibling) owners
actually complain about and ask for, mined from real YouTube/Reddit/forum discussion. Use it to
steer OpenScope firmware work toward fixes that owners will *notice and value*.

**Source:** `ytscan` market-research sweep, 2026-06-14 — four scans (`fnirsi-2c53t-pulse`,
`fnirsi-scopes-landscape`, `fnirsi-2c23t-sibling`, `fnirsi-device-landscape`) covering ~40 videos
and ~1,300 comments plus Reddit/HN/GitHub sources. Raw reports live outside the repo under
`~/Projects/RESEARCH/ytscan/reports/`.

**The strategic headline:** the most-cited complaints are *firmware-fixable*, they *recur across
the whole scope family* (2C53T / 2C23T / 2C53P / 2D15P — confirmed shared firmware), and FNIRSI
*demonstrably won't fix them* ("They will never change their firmware. They just put out a new
device."). That trifecta — fixable + cross-model + abandoned by the vendor — is the entire reason
an open firmware has a real audience.

---

## Tier 1 — "Ship this and win" (high recurrence, clearly firmware-fixable)

### 1. Reliable triggering ⭐ highest-signal technical bug
The single most damning and most technical complaint, and it spans models.
- 2C53T (@sadfur8728, +5): *"Normal trigger is only operable at 20ms and faster sweep rates,
  reverting to Auto mode at 50ms and up. And even at sweep rates where it's supposed to be working,
  it will mostly miss the trigger events… Workaround is to always use single sweep mode."*
- 2C23T (@pixlewing, +18): normal trigger fails on a 1PPS GPS pulse.
- DSO152: single-trigger captures only part of the waveform, fills the rest with noise.
- FNIRSI's *own* DSO152 tutorial ships a 4-step manual workaround for "unable to trigger."
- **Maps to:** the scope FSM / trigger-path work already underway. A trigger that *just works* in
  Normal and Single at all timebases is the flagship differentiator.

### 2. Manual DMM range lock (stop auto-reverting)
Most-requested DMM fix. The meter can't be pinned to a range and force-reverts to Auto every time
you leave and re-enter meter mode (and beeps continuity at you).
- @MortenHat (+10): *"reverts to Automatic mode and select continuity mode and possibly beep at you…
  the inability to manually select ranges (mV, V, uA, mA)."*
- @jgcheak, @jackflash6377, @miller-clem, @KentechInstruments echo it.
- **Type:** meter-mode FSM / state-persistence.

### 3. Correct Min/Max/Avg semantics
Implemented wrong; only works if you first hit Relative-to-null; Min stays stuck at 0V.
- @4NDR3SV asked FNIRSI directly: *"Why Min value doesn't work as expected in 2C53T **and** 2C23T,
  always shows 0.0 unless measuring a negative signal. Are you going to fix this?"* → **one fix
  ports across both models.**
- @dubmob151 (+3): *"They record the most negative values below zero as the min… didn't quite
  understand how it's supposed to be done."*
- Wish: reset min/mean/max without switching modes (e.g. capture a car-battery dip on cranking).

### 4. Honest resolution (drop padded trailing zeros)
The 20,000-count mV range is mostly fake — "maybe 200 real counts… incredibly scammy." A rival
(Zoyi ZT-703S) is praised specifically for *not* padding zeros.
- @sadfur8728 (+5), @pizzablender (+4), @wtmayhew (+6).
- **Type:** display formatting. Near-free credibility win.

### 5. Real, labeled, scalable FFT
Current FFT has no labels, no scale, wrong spacing — "pretty useless." The same rival *added* a
working FFT in a firmware update, proving it's deliverable on this hardware class.
- Kerry Wong (transcript), @OMNI_INFINITY, @lonebot2003.

### 6. Robust screenshot save / no FatFs corruption
- @johnchild61: *"won't store screen shots, just says failed to save… will have to return it."*
- @Alexion3000: *"the screenshot storage… keeps corrupting requiring you to reinstall the firmware
  over and over."*
- @yldrmyilmaz6264: accidentally deleted System File assets → bricked UI, no recovery path.
- **Maps to:** the current FatFs / screenshot-layer coverage work.

---

## Tier 2 — Meaningful; several map onto existing OpenScope work

- **Dual-channel 2nd-channel jitter/desync** (2C23T): when both channels show the same signal,
  only one locks/measures. Reportedly fixed by FNIRSI in v2.1.0 → firmware-tractable. (@bobisyouruncle1 +9)
- **On-screen scope voltage reads ~4% low while the trace itself is correct** (2C23T @WA_AM: input
  5.000V, scope prints 4.800V). → **maps onto the scope gain/offset cal-table work.**
- **Meter autorange / decimal-point instability** across both models → *same class* as the
  frame[6] dp-jitter / >10V DCV problem already being chased on the 2C53T.
- **X-Y mode**: broken by default (works only if you manually enable both channels — a default-state
  bug). Plus a recurring wish: pan/zoom on a *frozen* capture.
- **No CSV / waveform export, no PC streaming** — only BMP screenshots over USB mass-storage. Asked
  across every model. Automotive users specifically want **CAN/LIN decode** (`fnirsi can bus` is a
  real search query). See the export-format lead in the appendix.
- **Clunky, button-heavy UI / cursor overshoot**: a single press moves 1px, hold "races away too
  quickly." Owners ask for acceleration / fine-coarse on the arrow keys (@olepigeon: "essentially
  adding a mouse scroll wheel").

---

## Tier 3 — Low-effort goodwill wins

- **Sans-serif meter font** — the *highest-voted single UI gripe* (@juststeve5542, +28: *"that
  horrible low-res Times New Roman… try Arial!"*). OpenScope already has a multi-size font system.
- **Capacitance in µF, not mF** (@olepigeon, Branchus) — unit display toggle.
- **Visual continuity indicator** on-screen (accessibility for hard-of-hearing) — asked twice.
- **Pitched diode beep** (different tone for Schottky vs silicon) — community loved the idea.
- **dBu (0.775V) / dBV (1.0V)** readouts for audio work (@RBBlackstone).
- **Suppress meaningless decimal places** on the frequency readout (@Alexion3000).
- **Signal-gen frequency persists** instead of resetting to 1kHz on menu exit (@jjptech).

---

## Explicitly hardware-limited (don't chase as firmware fixes)

For triage clarity — these came up but are not firmware-tractable on this hardware:
- Signal generator capped at **3 Vpp** and **50 kHz** (DAC-via-MCU architecture; note the cheaper
  2C23T actually does 2 MHz on its siggen — a deliberate platform trade).
- **250 MS/s shared** across both channels → 125 MS/s each; aliasing above ~30 MHz with both on.
- **Non-isolated (shared) channel grounds.**
- **No pF-range capacitance**; **20 mV/div** vertical floor (front-end gain).
- Recessed BNCs reject standard probes; fragile BNC solder joints; non-replaceable LiPo.

---

## Suggested build order (impact × effort)

1. Triggering (Normal + Single, all timebases) — highest impact, aligns with current FSM work.
2. Manual DMM range lock + no auto-revert on mode switch.
3. Honest count display (drop padded zeros) — cheap, high credibility.
4. Min/Max/Avg correct semantics + resettable.
5. Robust screenshot/FatFs (already in flight).
6. Real FFT; X-Y default-enable fix.
7. CSV/waveform export (flagship differentiator; see decoder lead below).
8. Polish bundle: sans font, µF, cursor acceleration, continuity indicator, diode-beep pitch.

---

## Appendix A — Cross-model porting

The 2C53T, 2C23T, 2C53P, and 2D15P share a firmware base (the Min/Max bug spans all of them per
@4NDR3SV; the 2C53T exposes XY/FFT/Persistence/Math that the 2C23T gates off on the *same*
platform). Implication: fixes and features built for OpenScope-2C53T are largely portable across
the family — and `rosenrot00/OpenScope-2C23T` already exists as the sibling effort.

## Appendix B — Export-format lead (for the CSV/PC-export feature)

**`RafaelDiasCampos/FNIRSI-2C53T-Decoder`** (GitHub, ⭐1, updated 2026-05-31) — a third-party Python
decoder for the **2C53T's exported `.bin` waveform files** (voltage conversion, timebase
reconstruction, FFT, JSON export). He's also an OpenScope stargazer. Useful both to understand the
on-device save format and as a reference/collaborator for a CSV-export feature. Existing 1013D/1014D
export tooling (`jesuslg123/fnirsi-wave-explorer`, `CrizPi/...wav-analyzer`) shows this is a durable
community demand across the whole FNIRSI scope line.

## Appendix C — Competitive / narrative context

- Most-named cross-shopped rivals: **OWON HDS200 series** (HDS2102S — has a *separate* siggen chip,
  the thing FNIRSI's MCU-synth siggen can't match) and **Zoyi/Zotek ZT-703S** (honest DMM counts +
  a working FFT added in firmware).
- Pervasive "FNIRSI won't fix their firmware / toys disguised as instruments" sentiment — this is
  both validation and the marketing narrative for OpenScope. Several owners literally assume the
  vendor will never patch; a credible open alternative has an open door.
