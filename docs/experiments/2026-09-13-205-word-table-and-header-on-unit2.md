# EXP-205 — The corrected word table under AA 55, on unit #2

- **Date:** 2026-09-13
- **Unit:** bench unit #2 (Stlkv)
- **Build:** `make guest-coldtrace-meter`, branch `meter-word-table` (this PR), staged into
  slot B over the port firmware's CDC loader and installed with `fwswap b`
- **Status:** **CONFIRMED** — with two defects found and fixed on the way, both in this branch

## 1. Problem

PR (1) of issue #15 turns the meter TX header on by default and replaces the word table.
Nothing on the bench had ever exercised a *UI-driven* transition with the header live:
EXP-25/26/27 commanded the SoC by hand (`usart tx`) after entering meter mode. Does
`mode meter <submode>` select the function it names, does the unobeyed keepalive stay
silent, and does every one of the eleven words get accepted?

## 2. Hypothesis

With the header on and the measured table in place, each `mode meter N` produces exactly
one accepted echo and the SoC's frame changes to the signature of function N; the 4 Hz
keepalive produces no echoes and no relay actuation.

- **If true:** `echo_valid` +1 per transition, `echo_bad` 0, no echo growth between
  transitions, data frames ~6/s, frame signature per function.
- **If false:** the echo does not move on a transition, or moves without one.

## 3. Procedure

```
meter hdr                # default must read AA 55
mode meter [N] 0         # UI transition path (fpga_set_meter_mode -> fpga_apply_meter_transition)
meter hdr / status       # echo ladder, tx_frames_recent, transition counters
meter dump               # frame, nibbles, raw_digits
meter hdr off            # negative control
```

Frames are hand-decoded from `nibbles=` (pre-lookup): bit 4 of a digit's nibble is the
seven-segment decimal point, lit on the digit it precedes; the glyph is the other seven bits.

**Preconditions verified by readback:**

| what | expected | measured |
|---|---|---|
| header default | `AA 55 (default)` | ✅ |
| scope mode, 10 s | TX 0, RX 0 on this flavour (`FPGA_USART_SILENT_SCOPE=1`) | TX 0, RX 0 — so the scope-side control is vacuous here, see §6 |
| keepalive on the wire | `00 00 05 09` | ✅ every frame in `tx_frames_recent` |

## 4. Control

Positive control, recorded first, on the **first** build of this branch (before the fixes
below): `usart tx 05 0C` by hand in meter mode → `echo_start 0→1, echo_valid 0→1,
echo_bad 0`, frame changed from the boot-time `E4 2E 63 25 07 00 00` to a DCV frame
(`f8=0x82`, reading 0.0003 V). The header and the RX echo path work on this build.

Negative control (final build): `meter hdr off`, then `mode meter 6 0` → the selector was
sent five times under `00 00`, `echo_valid` unchanged, `confirmed=0`, and the SoC's frame
stayed the boot-time text — the SoC does not obey `00 00`.

## 5. Results

### 5a. Defect 1 — the transition's selector was never accepted (fixed in this branch)

First build, `mode meter`: TX count climbed, keepalive frames flowed, **`echo_valid` stayed
0**. `mode meter 6 0`: `tx_control_history` shows `AA 55 05 0B` leaving at tx=401 — the
selector was transmitted — and the transition history shows `data=558..558`: **not one
data frame arrived during the transition.** The frame afterwards was the boot-time
`E4 2E 63 25 07 00 00` (the SoC's power-on "Auto" text), not the DCV frame the SoC had been
sending a second earlier.

Reading: `fpga_meter_reset_transport()` drops PC11, and the SoC **restarts** — every
transition lands it back in its power-on state, and it is deaf while it boots. The selector
20 ms later goes into that silence. The same word typed by hand a second later was echoed
at once.

Fix: `fpga_meter_wait_for_soc()` waits for the first data frame after PC11 comes back (up
to 3 s, feeding unobeyed keepalive traffic every 250 ms), then +200 ms; the selector is
sent by `fpga_send_selector_confirmed()`, which re-sends until its echo arrives (≤5 tries,
300 ms each). Both are exported by `meter hdr` as `transition: wake_ms= tries= ok=`.

**Measured wake time: 1380 ms, on every one of 24 successful transitions**, boot→meter,
scope→meter and meter→meter alike. It is a constant, not a distribution.

### 5b. Defect 2 — one scope→meter entry in three had a dead transceiver (fixed)

With the wait in place, 2 of 6 and then 1 of 4 scope→meter entries still timed out with no
frame at all (`wake_ms=65535`, five unanswered tries). In that state:

| readback | failed entry | good entry |
|---|---|---|
| `USART2 CTRL1` | `0x20A0` — UEN=1, **TE=0, RE=0** | `0x202C` — TE=1, RE=1 |
| TX count | climbing (into a disabled transmitter) | climbing |
| RX bytes | frozen | climbing |
| `usart tx 05 0C` by hand | no echo | echo |

Mechanism (code reading, consistent with every observation): on a silent-scope build,
scope entry parks USART2 with `ctrl1 = 0`. The transition's `reset_transport()` reads
`ctrl1`, quiets the bus for 20 ms, then restores `ctrl1 | UEN | RDBFIEN`. The display task's
`fpga_set_meter_mux(true)` re-arms USART2 asynchronously when it notices the mode change;
when that lands inside the 20 ms window, the restore overwrites it with TE/RE clear. Fix:
the restore now sets TE and RE and enables the USART2 IRQ unconditionally — the transition
is about to talk to the SoC, so it owns the transceiver.

After the fix: **8/8 scope→meter entries confirmed on the first try, `CTRL1=0x202C` every
time, `wake_timeouts=0`, `unconfirmed=0`.**

### 5c. All eleven words accepted; frame signatures on open probes

| submode | word | echo Δ | tries | wake_ms | frame[2..11] | signature |
|---|---|---|---|---|---|---|
| 0 DCV | `050C` | +1 | 1 | 1380 | `F6 EB EB CB 07 00 82 00 00 FC` | f8=0x82, 0.0005 V |
| 1 ACV | `050D` | +1 | 1 | 1380 | `E5 EB EB EB 0B 00 82 00 00 00` | f8=0x82, "0000" |
| 2 DC mA | `0511` | +1 | 1 | 1380 | `F6 FB EB 8B 0F 01 01 00 01 0D` | f8 bit0, -00.03 mA |
| 3 DC A | `0510` | +1 | 1 | 1380 | `F6 EB EB AB 0D 00 81 00 01 0F` | f8=0x81, 0.002 A |
| 4 AC mA | `0516` | +1 | 1 | 1380 | `E5 FB EB EB 0F 01 01 00 00 00` | f8 bit0 |
| 5 AC A | `0515` | +1 | 1 | 1380 | `E5 EB EB 8B 0A 00 81 00 00 00` | f8=0x81 |
| 6 Ω | `050B` | +1 | 1 | 1380 | `04 F0 6B 01 00 24 00 00 01 0B` | f7=0x24, " 0L " |
| 7 Cont | `0517` | +1 | 1 | 1380 | `00 E0 7B 01 00 28 00 00 01 15` | f7=0x28, " 0L " |
| 8 Diode | `050E` | +1 | 1 | 1380 | `00 F0 6B 01 80 00 02 00 01 0B` | f8=0x02, " 0L " |
| 9 Cap | `050A` | +1 | 1 | 1380 | `E4 FB EB EB 2B 10 00 00 00 FE` | f7=0x10, f6=0x2_ (nF), "000.0" |
| 10 Temp | `0512` | +1 | 1 | 1380 | `00 00 A0 ED 0F 00 20 00 01 1F` | f8=0x20, "  28" (internal sensor, °C) |

Every word in the table is a word the SoC accepts and answers with a different function.
`echo_bad` stayed 0 throughout the session (final count: 24 valid echoes, 0 bad).

### 5d. Loads, hand-decoded (their decoder's output alongside, for PR (2))

| load | submode | frame[2..7] | 7-seg text | reading | their decoder |
|---|---|---|---|---|---|
| Li cell | 0 DCV | `86 FF EF A7 0D 00` f8=02 | `3.862` | 3.862 V | 3.862 V ✅ |
| Li cell | 8 Diode | `00 F0 6B 01 80 00` f8=02 | ` 0L ` | OL (above diode range) | `---` |
| Li cell | 1 ACV | `E5 EB EB EB 0B 00` f8=82 | `0000` | 0 (DC source) | `---`, reject=3 |
| 2.2 kΩ | 6 Ω | `A4 0D EA E7 4F 20` | `2168` | 2168 Ω | 2.168 kOhm ✅ |
| 2.2 kΩ | 7 Cont | `00 E0 7B 01 00 28` | ` 0L ` | open (above threshold) | `ERR` |
| 10 kΩ | 6 Ω | `C4 DF AF 4D 4E 20` | `9.924` | 9.924 kΩ | 9.924 kOhm ✅ |
| 300 kΩ | 6 Ω | `A4 CD 8F EA 0F 24` | `2978` | 297.8 kΩ (f7=0x24: ×100 Ω) | `---`, reject=4 |
| short | 6 Ω | `04 E0 1B 8A 0A 20` | ` 0.17` | 0.17 Ω | `ERR` |
| short | 7 Cont | `00 E0 1B 8A 0A 28` | ` 0.17` | 0.17 Ω | `ERR` |
| 10 nF | 9 Cap | `C4 FF A7 8D 2F 10` | `9.623` | 9.62 nF (f6=0x2_) | 962.3 nF ✗ (×100) |
| 10 µF | 9 Cap | `C4 FF C7 8F 1A 10` | `9.697` | 9.70 µF (f6=0x1_) | 969.7 nF ✗ (unit, ×100) |

Two things the table says about decoding, both for PR (2): the decimal point is in the
frame and it moves with the range (`2168` has none, `9.924` has one, both under
`f6 = 0x4_`); and the capacitance unit is the upper nibble of `f6`.

### 5e. Keepalive is silent; obeyed words are what click

Operator listening, labelled as such, with the relay pose read back over the shell before
and after each step:

| phase | what | clicks heard | our relay pose |
|---|---|---|---|
| 30 s in DCV, keepalive only (129 frames) | — | **0** | unchanged |
| scope → meter | entry | 3 | changed (scope pose → meter pose) |
| DC mA → DC A (2→3) | function change | **2** | **unchanged** |
| Cap → Temp (9→10) | function change | 0 | unchanged |

The 2→3 row is the interesting one: our MCU moved no pin, yet something clicked twice.
The SoC switches its own relays when a word is obeyed — consistent with the stock logger
result in #15 (stock's MCU moves no frontend pin across 40 function changes) — and it does
so only for functions that need it.

### 5f. Scope is undisturbed

SPI3 reads in scope mode: 15.3/s before a scope→meter→scope cycle, 14.2/s after, 0
timeouts, CH1 acquiring.

## 6. Blind spots

1. **The scope-side control is vacuous on this flavour.** With `FPGA_USART_SILENT_SCOPE=1`
   nothing is transmitted in scope mode, so "scope traffic causes no echoes" was not
   tested here. On a flavour that does drive USART2 in scope mode, the scope words go out
   unobeyed by construction (`fpga_timed_send_cmd`), but the SoC's reaction to an obeyed
   non-`0x05` frame remains unmeasured.
2. **No current source, no AC source, no thermocouple.** For submodes 1–5 and 10 only the
   word's acceptance and the frame signature are established; no value was checked.
3. **Loads are ±5 % parts and an unlabelled cell**; readings are physically sensible, not
   calibrated. The cell read 3.86 V, so it is not the alkaline AA it was assumed to be.
4. **Wake time is one unit's constant.** 1380 ms on 24 transitions here says nothing about
   unit #1 or about supply/temperature dependence; the 3 s timeout has 2.2× margin on
   this unit only.
5. **Relay clicks are ear-counted**, with pose readback as the only instrument. The 3-click
   entry was not decomposed into PC11 vs relay pose vs SoC.
6. **Only one obeyed word per transition was tested.** Whether the SoC accepts a second
   word without a PC11 restart (it did by hand, in EXP-27 and here) is not what the
   production path does now — the production path restarts the SoC every time.

## 7. Conclusion

- **Established:** with the header on and the measured table, `mode meter N` selects
  function N for all eleven submodes — one accepted echo per transition, zero bad echoes,
  frame signature per function, first-try confirmation on 24/24 transitions after the two
  fixes. The unobeyed keepalive produces no echoes and no relay actuation over 30 s.
  Header off → the SoC ignores the transition (negative control holds).
- **Established, new:** `fpga_meter_reset_transport()` restarts the SoC (PC11), which then
  needs 1380 ms before it hears anything. A transition that sends its selector 20 ms after
  PC11 selects nothing. This was true of every transition before this PR and was invisible
  because nothing was ever obeyed.
- **Established, new:** the transport reset could inherit a disabled transceiver from the
  scope-mode park and re-enable USART2 without TE/RE (`CTRL1=0x20A0`), about one entry in
  three from the shell. Fixed by making the reset own TE/RE/IRQ.
- **Excluded:** the keepalive as a source of relay actuation; scope disturbance from a
  meter cycle at the SPI3-rate level.
- **NOT excluded (explicitly):** the SoC's reaction to obeyed non-`0x05` words; the wake
  constant on other units; whether dropping PC11 on function changes is needed at all
  (stock does not, per the #15 logger) — that would remove the 1.4 s per change, and it is
  a transport-shape decision for the maintainer.
- **Follow-up:** PR (2) (decoder: frame decimal point, capacitance unit, both OL spellings,
  the `f7=0x24` band); PR (3) (the stock logger); EXP-26's "transmit stopped → 0 frames"
  arm was `mode scope`, which also drops PC11 — worth re-running with PC11 held high to
  separate "answers traffic" from "was powered".
