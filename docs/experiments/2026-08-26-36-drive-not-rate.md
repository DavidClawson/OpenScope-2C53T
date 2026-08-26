# EXP-36 (H7 step 4) — rate excluded both ways; the differentiator is GPIO-vs-AF pin drive at matched rate

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Builds:** current-session `guest-bringup-bb` (AF via `fpga reinit`), then
  `make guest-bringup-bb-slow` (`FPGA_BB_HALF_DELAY=250` → bit-bang SCK ~470 kHz,
  `fpga configbb`).
- **Status:** CLOSED — the cleanest isolation yet. **Rate is excluded on both
  transports; GPIO drive configures where AF drive walls, at a matched ~470 kHz.**

## 1. Question

EXP-35 left GPIO-vs-AF drive as the last standing differentiator, but the two
working reference points (our bit-bang, stock AF) ran at different rates from the
failing one, so "drive" and "rate" were still entangled. Separate them: test AF
at bit-bang's rate, and bit-bang at AF's rate.

## 2. Procedure + Results

All reads anchored (IDCODE `0x0120681B` at /256 before each attempt). BR field:
0=/2, 5=/64, 7=/256.

**AF at three rates** (`fpga reinit <br> 100 600 pe k<br>`, `pe` reads EDIT_MODE
at /256 after 0x15):

| rate | command | post-0x15 EDIT_MODE | STATUS | verdict |
|---|---|---|---|---|
| /2 (60 MHz) | `reinit 0 … k0` | no | 00039020 | WALL |
| /64 (~1.9 MHz) | `reinit 5 … k5` | no | 00039020 | WALL |
| /256 (~470 kHz) | `reinit 7 … k7` (EXP-32) | no | 00039020 | WALL |

AF walls at **all three rates**, port OPEN after each (true refusal).

**Bit-bang slowed to AF's rate** (`guest-bringup-bb-slow`, `fpga configbb`):

```
anchor: IDCODE 0x0120681B, STATUS 00039020   (OPEN)
post-upload STATUS(0x41): 0003F460   DONE_FINAL(bit13)=YES -- CONFIG TOOK
```

`0003F460` is the documented success value. The slowed (~470 kHz) GPIO bit-bang
**configures** — same as the ~2 MHz bit-bang (EXP-32, 3/3).

## 3. The matrix

| path | drive | rate | result |
|---|---|---|---|
| stock | AF | /2 (60 MHz) | ✅ |
| our bit-bang | GPIO | ~2 MHz | ✅ |
| our bit-bang (this) | GPIO | ~470 kHz | ✅ DONE_FINAL 0003F460 |
| our AF | AF | /2 | ❌ 00039020 |
| our AF | AF | /64 (~1.9 MHz) | ❌ 00039020 |
| our AF | AF | /256 (~470 kHz) | ❌ 00039020 |

## 4. Conclusion

- **Rate is excluded on BOTH transports.** GPIO configures at 470 kHz and 2 MHz;
  AF walls at 470 kHz, 1.9 MHz and 60 MHz. There is no rate at which our AF works
  and none at which our GPIO fails (in the tested span).
- **At a matched ~470 kHz, GPIO drive configures and AF drive walls.** With bytes,
  framing, CS-dummy, registers, DMA-vs-polled, SPE-sequencing AND rate all held
  equal (EXP-29/31/32/33/34/35), the ONLY remaining difference between our
  configuring path and our walling path is **how PB3/4/5 are physically driven:
  GPIO push-pull (works) vs SPI3 alternate-function (walls).**

## 5. The paradox this does NOT dissolve

**Stock configures on AF drive** (its `0x3B` upload is hardware-SPI3, EXP-34), and
H6/Exp E found our AF pin+peripheral registers byte-identical to stock's. So "AF
drive cannot configure this GW1N" is FALSE in general — it is specifically OUR AF
that walls. The true unknown is therefore still **our-AF vs (stock-AF + our-GPIO)**,
now provably NOT rate. Either a hidden difference in stock's AF path that a
register snapshot missed, or a physical/analog property of how our SPI3 peripheral
drives the net that GPIO push-pull does not reproduce.

## 6. Next — the apples-to-apples LA capture

The perfect comparison is now available entirely on our own firmware: **our-AF at
/256 (walls) vs our-bit-bang at ~470 kHz (configures)** — same pins, same rate,
same FPGA state, differing ONLY in drive type. One LA window over both:

- edges differ (extra/missing SCK edge, phase, CS-to-edge, idle level) → we see
  exactly how AF drive departs from GPIO drive;
- edges bit-identical yet one configures → the difference is sub-decodable
  (analog slew/glitch) and escalates to a scope + stock-AF native capture.

Shipping unaffected — bit-bang coldtrace already cold-boots to a live scope; this
is understanding (and a possible faster hardware-SPI config path if the AF defect
is found and fixed).
