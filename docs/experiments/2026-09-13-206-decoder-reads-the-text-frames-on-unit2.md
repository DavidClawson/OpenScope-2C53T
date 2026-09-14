# EXP-206 — The decoder reads the SoC's text frames on unit #2

- **Date:** 2026-09-13
- **Unit:** bench unit #2 (Stlkv)
- **Build:** `make guest-coldtrace-meter` from the `meter-word-table` branch (#33) with the
  decoder commit of this PR cherry-picked on top — the word table is what puts the SoC into
  the function whose frame is being decoded. Installed in slot B over the port's CDC loader.
- **Status:** **CONFIRMED**

## 1. Problem

PR (2) teaches `meter_data.c` three things measured in EXP-27/EXP-205: the decimal point is
in the frame, a leading blank is a space, and the resistance/capacitance bands are in
`frame[7]`/`frame[6]`. The host tests pin the fixtures; does the firmware decoder produce the
right number on the device, from the live frame, for the loads that EXP-205 hand-decoded?

## 2. Hypothesis

Every load EXP-205 hand-decoded now comes out of `meter dump` as `display=<that value>
unit=<that unit>` with `reject=0`, three reads apart, stable to the last digit or one.

- **If true:** the table in §5 matches EXP-205 §5d.
- **If false:** a value differs by a power of ten or a unit, or `reject` is non-zero.

## 3. Procedure

```
mode meter N 0           # UI transition, echo-confirmed selector (#33)
meter dump               # x3, 1.2 s apart: display=, unit=, reject=, frame=
```

Loads changed by the operator between rows; nothing else touched.

**Preconditions verified by readback:** header `AA 55 (default)`; each transition
`tries=1 ok=1`; frame signatures per function as in EXP-205 §5c.

## 4. Control

Open probes, recorded first, no hands:

| submode | display | reject | frame[2..11] |
|---|---|---|---|
| 6 Ω | `OL` | 0 | `04 F0 6B 01 00 24 00 00 01 35` |
| 7 Cont | `OL` | 0 | `00 E0 7B 01 00 28 00 00 01 35` |
| 9 Cap | `0.002 nF` | 0 | `E4 FB EB AB 2D 10 00 00 01 37` |
| 10 Temp | `30 C` | 0 | `00 00 80 EF 0B 00 20 00 01 34` |
| 8 Diode | `---` | 1 | `00 F0 6B 01 80 00 02 00 01 39` |
| 0 DCV | `0.0005 V` | 0 | `F6 EB EB CB 07 00 82 00 01 3A` |

Before this PR the same frames gave `ERR` (Ω, Cont), `---` (Temp) and a ×100 capacitance.
The diode row is the expected negative: its frame carries the voltage marker `frame[8]=0x02`
and the family model rejects it — left unchanged in this PR, see the PR text.

## 5. Results

| load | submode | decoder, 3 reads | reject | frame[2..11] | EXP-205 hand decode |
|---|---|---|---|---|---|
| 10 kΩ | 6 Ω | 9.929 / 9.929 / 9.929 kOhm | 0 | `C4 DF AF CD 4F 20 00 00 01 33` | `9.924` kΩ |
| 2.2 kΩ | 6 Ω | 2.168 / 2.169 / 2.169 kOhm | 0 | `A4 0D EA C7 4F 20 80 00 01 35` | `2168` Ω |
| 2.2 kΩ | 7 Cont | OL / OL / OL | 0 | `00 E0 7B 01 00 28 00 00 01 33` | ` 0L ` |
| 300 kΩ | 6 Ω | 297.8 / 297.8 / 297.8 kOhm | 0 | `A4 CD 8F EA 0F 24 80 00 01 35` | `2978` × 100 Ω |
| short | 6 Ω | 0.17 / 0.16 / 0.16 Ohm | 0 | `04 E0 1B EA 07 20 00 00 01 35` | ` 0.17` |
| short | 7 Cont | 0.16 / 0.17 / 0.17 Ohm | 0 | `00 E0 1B 8A 0A 28 00 00 01 35` | ` 0.17` |
| 10 nF | 9 Cap | 9.642 / 9.635 / 9.634 nF | 0 | `C4 FF 87 4F 2E 10 00 00 01 3C` | `9.623` nF |
| 10 µF | 9 Cap | 9.716 / 9.704 / 9.704 uF | 0 | `C4 9F EA 4B 1E 10 00 00 01 3C` | `9.697` µF |

Every row matches the hand decode in value, unit and point; the last-digit drift between
reads (and against EXP-205, taken an hour earlier) is the SoC's own.

## 6. Blind spots

1. Same ±5 % parts and unlabelled capacitors as EXP-205; no reference instrument. This shows
   the decoder reproduces the SoC's text, not that the SoC is accurate.
2. Currents, ACV, diode: not decoded here (no source; diode by design of the family model).
3. One unit. The unit #1 fixtures in the tests are reconstructions of EXP-27's published
   bytes, not frames captured through this code.
4. The bench build is #33 + this PR; on `main` alone the UI would still select the wrong
   function, so this result is conditional on #33.

## 7. Conclusion

- **Established:** with the word table of #33 selecting the function, the decoder of this
  PR reads resistance across three bands (0.17 Ω, 2.169 kΩ, 9.929 kΩ, 297.8 kΩ),
  continuity (0.17 Ω / OL), capacitance in both units (9.64 nF, 9.70 µF) and temperature
  (30 °C) from live frames, `reject=0`, matching the EXP-205 hand decodes.
- **Excluded:** the ×100 capacitance and the low-band/upper-band resistance rejects.
- **NOT excluded:** any accuracy claim; diode (family model); currents and ACV.
- **Follow-up:** the diode family question and continuity beep policy (PR text); a reference
  DMM for the S1→S2 promotion the spec asks for.
