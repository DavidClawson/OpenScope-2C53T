# Pin and peripheral corrections from a system map of unit #2 (2026-11-12)

A pin-by-pin map of a `2C53T-V1.4_20250507` board ("unit #2"), built from three sources:
the GPIO and peripheral setup in stock V1.2.0 (`APP_2C53T_V1.2.0_251015.bin`, linked at 0x08007000),
register dumps of stock running on the unit (read over SWD), and bench checks. This note lists only
the places where it disagrees with the docs in this repository, or fills a row they mark unknown.
Nothing here changes code.

Evidence tiers, strongest first:
- **B**: bench-verified on unit #2 (measured, or a build that depends on it works on the unit).
- **L**: live register dump of stock V1.2.0 running on unit #2.
- **U**: measured or traced by someone upstream (credited).
- **S**: static reading of stock V1.2.0 (addresses given).

Stock addresses are Thumb code addresses in the V1.2.0 image.

## Summary

| # | Pin / block | The docs say | The map says | Tier |
|---|---|---|---|---|
| 1 | LCD D/CX | PD12 = EXMC A17 | **PD11 = EXMC A16** | S + L |
| 2 | PD12 / PD13 | A17 / "signal routing" | CH1 / CH2 AC-DC coupling, high = DC | S + L |
| 3 | PD6 | unknown | **ST7789V RESX**, stock holds it high | S + L |
| 4 | PA2 / PA3 | FPGA command UART | meter SoC UART, across a π122U31 isolator | U + B |
| 5 | PB7 (PRM) | pull-up, active low | **pull-down, active high** | S + L + B |
| 6 | PC3 | unknown | meter SoC buzzer line, EXINT3 | B |
| 7 | PB8 / PB9 | PB8 plain GPIO; PB9 TMR4_CH4 | stock drives them from **TMR10_CH1 / TMR11_CH1** | S + L + B |
| 8 | PA0 / PA1, TMR2 / TMR5 | unknown / "FPGA timing" | external-clock **frequency counters** on PA0 / PA1 | S + L |
| 9 | Vector 59 (TMR8_BRK / TMR12) | unknown / "FatFs" / "PWM" | **TMR12 1 s tick that feeds the IWDG** | S |

## 1. LCD D/CX is PD11 (EXMC A16), not PD12

- `reverse_engineering/HARDWARE_PINOUT.md` (PD11 and PD12 rows, the EXMC pin table, section 5),
  `docs/design/peripheral_map.md` (EXMC row), and the `A17=0/1` comments in `firmware/src/drivers/lcd.h`
  and `lcd.c` name PD12 = A17 as the command/data line.
- With a 16-bit bank, the EXMC drives HADDR[25:1] on A[24:0]. The data address 0x60020000 has HADDR bit 17
  set, which comes out on **A16 = PD11**. The command address 0x6001FFFE has it clear.
- Stock configures PD11 in the same alternate-function group as the strobes (NOE/NWE/NE1: PD4/PD5/PD7,
  0x0802ABFE), and PD12 on its own as a plain output (0x0802C57C). **[S]**
- Live, stock running: PD11 alternate function, PD12 general-purpose output. **[L]**
- Why it has worked anyway: the LCD block in `firmware/src/main.c` sets PD11 *and* PD12 to alternate function,
  so A16 is there. `lcd_gpio_init()` in `lcd.c` sets PD12 but not PD11; nothing in the app calls it today.

## 2. PD12 / PD13 are the channels' AC/DC coupling

- `HARDWARE_PINOUT.md` lists PD12 as A17 and PD13 as "signal routing".
- Stock drives both as plain outputs from its coupling writer (0x08008C60): PD12 = CH1, PD13 = CH2, high = DC.
  **[S]** On the unlabelled board in #37 they lead to the KTY214S PhotoMOS parts. **[U]**
- `fpga.c` already drives them as outputs (`GPIOD->scr = (1U << 12) | (1U << 13)`, "both channels DC-coupled").
  The AF setting of PD12 in `main.c`'s LCD block is overwritten there, so it is only a wrong label. The AF
  configuration of PD12/PD13 seen on a unit at boot in #18 was this LCD block's.

## 3. PD6 is the ST7789V reset (RESX)

- `HARDWARE_PINOUT.md` lists PD6 as unknown, and `lcd.c`'s `lcd_reset()` has "TODO: Identify RST pin".
- Stock configures PD6 as an output on its own (0x0802ABEC), drives it high, low, high at boot
  (0x0802AC5A / 0x0802ACB2 / 0x0802AD0A), then leaves it high. **[S]**
- Live: stock has PD6 as a push-pull output, high (GPIOD CFGL nibble 6 = 0x1, ODT bit 6 = 1). OpenScope's
  default image leaves it a floating input. **[L]**
- A floating active-low reset input fits the white screens (panel back in its power-on state while the app runs) reported in the issue filed with this note. A continuity check
  PD6 (LQFP pin 87) to the LCD FPC's RESX pin would settle it; not done yet.

## 4. PA2 / PA3: the meter SoC's UART, behind an isolator

- `HARDWARE_PINOUT.md` (PA2/PA3 rows, "FPGA USART2" section, the USART2 section) and `peripheral_map.md`
  (USART2 rows) describe USART2 as the FPGA command interface.
- @saulvalenzuela23 traced it in #37: the meter SoC's pins 35/36 (RXD/TXD) go to the isolated side of a
  π122U31 digital isolator; the MCU side is PA2/PA3. **[U]** The frames are the SoC's `AA 55` / `5A A5`
  protocol from #15 and #33.
- The meter's COM is isolated from USB ground on unit #2: with the unit off and USB unplugged, a Fluke
  reads open (OL, above ~40 MΩ) from COM to the USB-C shell and from COM to the CH1 BNC shell. **[B]**
- The π122U31 has two channels and the UART uses both, so PC11 (the SoC power/enable line) must cross the
  barrier by another route. Not traced.

## 5. PB7 (PRM) is active high with a pull-down

- `HARDWARE_PINOUT.md` lists PB7 as pull-up, active low.
- Stock's gpio init gives PB7 a pull-down (0x0802AB04). **[S]** The live dump agrees. **[L]**
- `firmware/src/drivers/button_scan.c` already reads it active high with a pull-down, and the bootloader's
  opposite reading is #80. **[B]**

## 6. PC3 is the meter SoC's buzzer line (EXINT3)

- `HARDWARE_PINOUT.md` lists PC3 as unknown; its vector table gives EXTI3 as "continuity buzzer detection".
- The SoC's BUZ output (pin 28) drives an EL3H7 optocoupler through 1 kΩ (#37). Its output is PC3. Unit #2,
  meter in Auto: 4480 edges/s on PC3 with the leads shorted (a 2.24 kHz tone), none with the leads open;
  PD3 and PE3 stay quiet. **[B]** Details in the PC3 hardware-observation issue filed with this note.
- Stock's EXINT3 handler (0x08009C10) sets the buzzer duty (TMR11 C1DT = 10) whatever the volume setting.
  **[S]**

## 7. PB8 / PB9: stock uses TMR10_CH1 and TMR11_CH1

- `HARDWARE_PINOUT.md` lists PB8 as a plain push-pull output and PB9 as TMR4_CH4 (#25).
- Stock writes the backlight to TMR10 C1DT (period 99, 10 kHz; 0x0800B448, 0x0800BA14) and the buzzer to
  TMR11 C1DT (period 24, 2 kHz; 0x08040038, 0x08009C10). V1.2.0 has no reference to TMR4's register block.
  **[S]**
- Live, stock running: TMR10 and TMR11 counting (CTRL1 = 1), TMR4 idle. OpenScope, dumped the same way the
  same day, had TMR10 and TMR11 stopped, so these are stock's own. **[L]**
- A build of ours drives the backlight from TMR10_CH1 and the buzzer from TMR11_CH1 on unit #2: the backlight
  dims, key clicks sound. **[B]** TMR4_CH3/CH4 are also legal on PB8/PB9 under the default mux, so #25's
  driver works electrically; stock just doesn't use it. Details for #71 are in a comment there.

## 8. TMR2 / TMR5 are frequency counters on PA0 / PA1

- `HARDWARE_PINOUT.md` lists PA0/PA1 as unknown and TMR2/TMR5 as "FPGA timing", values not extracted.
- Stock sets TMR2 and TMR5 up as external-clock counters with their inputs on PA0 and PA1, and the key task
  (0x080400B8) reads and zeroes both. **[S]** Live, stock running: both counting (CTRL1 = 0x401). **[L]**
  That is a hardware frequency counter per channel; the source of the edges (probably a comparator per
  channel) is not traced.

## 9. Vector 59 is TMR12, and it feeds the watchdog

- `HARDWARE_PINOUT.md` gives vector 59 (TIM8_BRK, 0x0802E78D) as unknown, `peripheral_map.md` as
  "repurposed for FatFs", `docs/re/function_map.md` as "PWM for buzzer or signal generator".
- On the AT32F403A this vector is shared by TMR8_BRK and TMR12. Stock runs TMR12 as a 1 s tick (period 9999)
  and its handler (0x0802E78C) is where the IWDG is reloaded. **[S]**
- The IWDG is started at 0x0802E5CC with prescaler /64 and reload 0x4E1, about 2 s on the nominal 40 kHz LICK.
  `HARDWARE_PINOUT.md`'s IWDG section says ~5 s, fed from the housekeeping loop; this reading disagrees. **[S]**

## Not settled

- PD6 → FPC RESX and PC11's route across the isolation barrier: continuity checks still to do.
- The edge source on PA0/PA1.
- The live register dumps of stock were taken after starting stock from OpenScope through a small launcher.
  Clock-gate and AFIO remap bits in them may be OpenScope's leftovers, so this note doesn't use them. The rows
  above use GPIO configuration and timer enables, compared with a dump of OpenScope taken the same way.
