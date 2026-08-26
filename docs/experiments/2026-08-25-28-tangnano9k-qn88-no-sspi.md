# EXP-28 — the Tang Nano 9K (GW1NR-9C QN88) cannot be an SSPI config target

- **Date:** 2026-08-25
- **Hardware:** Tang Nano 9K (GW1NR-LV9QN88PC6/I5) + `bluepill_bis` rig + HiLetgo LA
- **Status:** CLOSED — the board is structurally unusable for the SSPI-transport
  bisect. Documented so the next person does not repeat the evening.

## 1. Problem

Run the config-transport bisect (BIS-1/2/4) against an observable, SSPI-capable
GW1N on the bench, at zero risk to 2C53T unit #1. The plan: put the 9K in
SSPI-slave mode and drive its config port from the Blue Pill rig.

## 2. What was built (all correct, all for nothing on this package)

- Located R19 (MODE0 pull-down, pin 88) by tracing the QN88 from pin 1 and
  cross-checking against UG803E; **lifted R19 → MODE0 floats high on its internal
  weak pull-up → MODE[2:0]=001 = SSPI-slave** (bench: pin 88 reads 1.8 V; the
  auto-boot LED goes dark = the part now waits for config instead of loading
  flash — the mode change genuinely took).
- Wired CS→pin55, SO→pin56 (clean header taps), and SCLK/SI to the onboard flash
  pads (U3 pin6/pin2) with strain-relieved magnet wire; flash ~CS confirmed high.

## 3. The measurement (rig is flawless, FPGA is silent)

HiLetgo capture of leg `i`, sigrok SPI decode (`cpol=1 cpha=1`):

- **MOSI (SI):** `00 11 00 … 41 00 …` — the `0x11`/`0x41` opcodes clocked out
  perfectly. SCLK toggling, CS framing correct.
- **MISO (SO):** `FF FF FF … FF` — the FPGA never responds.
- Bit-bang leg `I` fails identically → not an SPI mode/polarity issue.

## 4. Root cause (UG290 + UG803E, definitive)

UG290 §7.4.1: SSPI uses **dedicated** pins `SCLK, CLKHOLD_N, SSPI_CS_N, SI, SO` —
and these are **distinct from** the MSPI set `FASTRD_N, MCLK, MCS_N, MO, MI`. We
had wired SCLK→MCLK(59) and SI→MI(62): the wrong mode's pins entirely.

The real SSPI pins, from UG803E's QN88 column:

| SSPI signal | pin name | QN88 | QN88P |
|---|---|---|---|
| SSPI_CS_N | IOR14B | 55 | 55 |
| SO | IOR14A | 56 | 56 |
| **SCLK** | **IOL12A** | **— (unbonded)** | **—** |
| **SI/D2** | **IOR13B** | **— (unbonded)** | **—** |

**SCLK (IOL12A) and SI (IOR13B) are not bonded to a package pin on QN88/QN88P.**
They exist only on LQFP144 and larger. So SSPI-slave config is physically
impossible on this package: setting MODE=001 makes the config engine wait for
clock and data on pins that do not leave the die. `FF` forever — as observed.

This is consistent with the symptoms exactly: SO/CS worked (bonded), MODE0 took
effect (LED dark), the rig drove SSPI perfectly, and the part still could not
hear us, because two of its four SSPI pins are absent on this chip.

## 5. Blind spots / how the plan reached the bench anyway

The pre-bench research mapped SCLK/SI to MCLK/MI (flagging SI as "likely, not
confirmed") and **never checked whether the dedicated SSPI pins were bonded on
QN88.** A package-bonding check against UG803E would have killed the plan before
any soldering. Add "confirm the config pins are bonded on THIS package" to the
pre-flight for any future dev-board config work.

## 6. Conclusion

- **Established:** the Tang Nano 9K (QN88) cannot be configured via SSPI-slave —
  SCLK/SI unbonded. It is fine as a JTAG dev board; it is the wrong tool for an
  SSPI-transport bisect.
- **Consequence for the bisect:** BIS-1/BIS-2/BIS-3-gap run on the real 2C53T
  without a dev board (EXP-26, EXP-27). **BIS-4 (blank-vs-auto-booted state) is
  the only leg that needs an external SSPI target, and it needs a GW1N in an
  LQFP144+ package** — the 2C53T's own chip proves SSPI works on *some* GW1N
  package; QN88 is not it.
- The Blue Pill rig is validated and reusable against a correctly-bonded target.
