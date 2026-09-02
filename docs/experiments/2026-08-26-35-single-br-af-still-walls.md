# EXP-35 (H7 step 3) — single-BR AF config still walls; the SPE-toggle sequencing is excluded

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-bringup-bb` (bus-released boot, CDC alive, `fpga reinit`
  drives the hardware-SPI path on demand). New `single_br` opt via the `sb`
  token: suppresses the three config-transaction `spi3_set_br()` calls
  (fpga.c:4280/4494/4501) so SPE is toggled ZERO times from `fpga_init` through
  the `0x3A` close.
- **Status:** CLOSED — negative. The pre-`0x15` SPE/BR toggle (EXP-34 item 1) is
  **excluded**.

## 1. Question

EXP-34 exonerated the config transaction at the byte/frame/pin-register level and
killed H3 (DMA). The one concrete *dynamic* difference it surfaced: our AF path
calls `spi3_set_br()` — which disables SPE, changes BR, re-enables SPE — at
fpga.c:4280, **before** the `05/12/15` prelude. Stock sets BR once at SPI3 init
and never touches it during config, so this pre-`0x15` SPE glitch is ours alone.
Does removing it let `0x15` engage EDIT_MODE?

## 2. Procedure

Power-cycle → `fpga busreacquire` (leaves SPI3 at /2, SPE on) → `fpga reinit 0
100 600 sb pe`, with NO intervening `/256` read so the config runs at a
guaranteed /2 (single_br skips the BR write, so the bus stays at whatever the
reacquire left). `pe` reads STATUS(0x41) at /256 immediately after `0x15` — the
EDIT_MODE(bit7) wall test — which is downstream of `0x15` and cannot retro-cause
the wall.

Anchor: a prior `spi3 gowin` read IDCODE `0x0120681B` / STATUS `00039020` at /256
→ port OPEN, cold, receptive.

## 3. Result

```
reinit: SINGLE-BR — no spi3_set_br/SPE toggle through the transaction
0x3A close:  00
0x41 STATUS: 00039020   flags: GWVLD READY POR
post-0x15 STATUS@/256: 00039020   EDIT_MODE(bit7)=no (0x15 did NOT engage)
acqread 0x04/0x05: 01 C8 10 free-running, span=200  (unconfigured, port open)
spi3 gowin AFTER: IDCODE 0x0120681B @/256, STATUS 00039020  (port OPEN — true refusal)
```

Reproduced twice (once after a `spi3 gowin` anchor that may have left /256; once
after a clean `busreacquire` guaranteeing /2). Bit-identical wall both times.
The post-config anchor reading `0x0120681B` proves the read instrument is valid
and the part is genuinely unconfigured — this is a real refusal, not the
port-closed-vs-dead-bus ambiguity.

## 4. Conclusion

- **Excluded:** the pre-`0x15` (and all in-transaction) `spi3_set_br()`/SPE
  toggle. With SPE untouched from `fpga_init` through the `0x3A` close — exactly
  stock's condition — the AF path walls identically (`00039020`, EDIT_MODE clear,
  port open).
- The config **transaction** is now exonerated on every firmware-controllable
  axis: bytes, payload, framing, `0x05`, CS-high dummy, write rate, DMA-vs-polled,
  pin registers (Exp E `9`-nibble), and now BR/SPE sequencing.
- **Bit-bang succeeds from this EXACT posture** (guest-bringup-bb, bus-released
  boot); AF fails from it, with identical bytes. So the differentiator has
  narrowed to the one axis nothing firmware-side can equalise: **GPIO push-pull
  drive of PB3/4/5 (bit-bang, works) vs SPI3 alternate-function drive (AF,
  walls)** — the physical character of the pins, not the traffic on them. Setup/
  hold timing is already excluded (the AF path walls at /256 too, where setup is
  ~1 µs).

## 5. Next

The only firmware-invisible discriminator left is the SCK/MOSI/CS **waveform**
itself. Decisive test = an LA side-by-side of the two paths' `0x15` frame on OUR
pins at a decodable rate (AF at /256 = 470 kHz; bit-bang slowed to match via
`FPGA_BB_HALF_DELAY`), diffing the actual edges of a GPIO-driven vs an AF-driven
`0x15`:

- edges bit-identical yet one configures → the difference is sub-sample
  (glitch/analog slew) and needs a scope;
- edges differ → we see exactly how AF drive departs from GPIO drive.

Then items 2/3 from EXP-34 (FPGA config-FSM state at `0x15`; uncontrolled
power/POR) remain, but they are now behind the physical-drive question, since
bit-bang and AF share the same FPGA state and posture and only the drive differs.

Shipping unaffected — bit-bang coldtrace already cold-boots to a live scope.
