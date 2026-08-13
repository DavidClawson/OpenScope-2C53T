# The wall came down, and it wasn't us who pushed it

**2026-08-12**

## The wall

From April to August this project had exactly one problem.

The FNIRSI 2C53T's oscilloscope front end is a Gowin GW1N-UV2. The part is
non-volatile — it holds a design across power cycles — but that resident design
is not enough: the stock firmware uploads a fresh 115,638-byte configuration over
SPI at every single boot, and without that upload the scope is stone dead.

We know that last part because we broke it on purpose. On 2026-07-27 we changed
**one byte** of the stock firmware — the `0x15` CONFIG_ENABLE opcode at file
offset `0x26A42`, flipped to `0x11` READ_IDCODE so the FPGA would keep its
resident design — and flashed it. Clean stock: scope works. One byte different:
no trace at all, meter still perfectly happy. Reflash clean stock: scope works
again. Same probe, same cell, A/B/A.

So we sent the same upload. The FPGA answered `0x00039020`. `DONE_FINAL` clear,
`SYSTEM_EDIT_MODE` clear, and — the part that made it maddening — **no error bits
at all.** No CRC failure, no bad-command, no ID-verify mismatch. The bytes weren't
being rejected. They were being silently ignored, because the part never entered
configuration mode in the first place.

That number is `0x00039020`, and from the moment we could read it properly — late
July, after the anchoring fix below — nothing we did moved it by a single bit. Not
one configuration command, across every framing, divisor, pin state and prelude we
could construct.

## Six things it wasn't

The record is worth keeping, because most of it is negative results, and a
properly measured negative result is the thing that stops the next person burning
a week on the same idea.

- **It wasn't the bytes.** @maksidze's logic-analyzer capture ([issue
  #18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18)) settled that in
  June — see below.
- **It wasn't timing.** The theory was that stock hit a narrow window shortly
  after power-on. We patched a busy-loop into a code cave in stock and pushed its
  config-enable 5–10 seconds later — visible as a splash-to-menu pause — and stock
  configured the FPGA anyway. There is no window.
- **It wasn't the analog front end.** Stock drives its relay/gain bank to a known
  range before the upload; our firmware left those pins floating, and that was the
  single biggest enumerable difference between the two. We NOPed stock's two
  range-select call sites so it uploaded with the bank floating exactly like ours.
  Stock still configured.
- **It wasn't MCU state.** We parked both firmwares one instruction before the
  `0x15` write, halted over SWD, and diffed every peripheral register. Clock tree
  byte-identical. We built a "fidelity" image matching stock on every difference
  we found — SPI divisor, MISO pull-up, three driven pins, USART enable — all at
  once. Wall held.
- **It wasn't a reset pin.** Gowin documents three ways to make a running,
  auto-booted part accept a new configuration: a RECONFIG_N pulse, a power cycle,
  or the `0x3C` RELOAD command. We refuted all three on this unit. Pin sweeps over
  twenty candidates, then twelve with transient-aware sampling: nothing. A genuine
  FPGA power cycle (which, it turns out, requires unplugging USB — neither the
  pinhole reset nor the power button drops the FPGA rail): still `0x00039020`.
  RELOAD: no change. Even PC9, the pin that [rosenrot00's independent 2C23T
  firmware](https://github.com/rosenrot00/OpenScope-2C23T) pulses before *its*
  bitstream — negative.
- **And it wasn't a lockout from user-mode traffic.** We tested that in June with
  a build that never enables USART2 at all. Identical wall. We then re-derived the
  same theory in July and had to be reminded by our own notes that it was already
  dead.

Somewhere in there we also learned that attaching a debugger to this device kills
it — flash read protection is set, and bringing up the debug port disables the
flash array out from under a CPU that executes from flash. Two months of
"mysterious hangs" were us, holding the probe.

## And several times, the instrument was the bug

The recurring failure mode of this project is not bad reasoning. It is
**confidently reading an instrument that cannot see the thing it is reporting on.**

- Every FPGA status read for six weeks was taken at a `/2` SPI divisor, where the
  driver's own comments say reads are garbage. A long-standing `0x8001C810` turned
  out to be `0x00039020` sampled one bit early. Another "free-running pattern" we
  had built a whole conclusion on was the same register, rotated.
- MISO was a floating input on our side and pulled up on stock's, so every read
  had been taken on a line with no defined idle level.
- An enumeration of "every pin where we differ from stock" compared output
  *levels* — which cannot distinguish a pin stock drives LOW from one we leave
  floating. Both read zero. Seven pins were hiding in that blind spot.

The fix that should have come first: **anchor every measurement.** Read IDCODE
before anything else and confirm it returns `0x0120681B`, a value we know
independently from the bitstream header. Three separate artifacts survived for
weeks purely because no measurement in the chain had a known-correct answer. A
stable wrong number is indistinguishable from a right one.

Once we did that, the picture got sharp and stayed sharp: the FPGA decodes SSPI
opcodes correctly, answers four different commands four different ways, and
ignores every configuration command we send.

## The people who actually broke it

Two contributors, neither of whom we have met.

**[@maksidze](https://github.com/maksidze)** patched the stock firmware's SPI
prescaler down to `/64` and captured an entire stock boot on a Saleae, then posted
it on [issue #18](https://github.com/DavidClawson/OpenScope-2C53T/issues/18). That
capture did two enormous things. First, it proved our payload was garbage: we had
been reading the bitstream out of the binary at file offset `0x51D19`, because we
had treated a *flash address* as a *file offset* and forgotten the app's
`0x08007000` link base. The real bitstream starts at `0x4AD19`. Every upload we
had ever sent, for months, was misaligned data. Second — after we fixed that and
the wall didn't budge — it eliminated the payload as a variable entirely, which is
what let us state the problem as config *entry* rather than config *content*.
maksidze also measured RECONFIG_N on the package pin for us, and later proposed
the single-variable test that closes out this entry.

**[@Stlkv](https://github.com/Stlkv)** did the thing that worked. On 2026-08-12 he
took maksidze's 2C23T-V0.4 loader — written for the sibling FNIRSI 2C23T, whose
scope works — and transplanted it onto the 2C53T's *own* pins (PB3/PB4/PB5, chip
select on PB6) with *our* corrected 115,638-byte payload. Status went
`0x00039020 → 0x0003F460`. **`DONE_FINAL` set.** Four cold boots out of four.
Live ADC baseline. His branch is
[`Stlkv/OpenScope-2C23T-2C53T-port`](https://github.com/Stlkv/OpenScope-2C23T-2C53T-port),
`2c53t-port`.

The difference between his loader and four months of our refusals was not the
bytes, not the payload, not the chip-select pin, not the MCU state, not the
timing, and not a reset. **His loader bit-bangs the SSPI handshake on GPIO. Ours
drove it through the SPI3 hardware peripheral.**

We had spent four months perfecting *what* to send. The answer was in *how*.

Credit is also due on the analysis side. A reply from the Apicula-side Gowin work
in June ([`docs/CONFIG_ENTRY_REPLY_FROM_APICULA.md`](../CONFIG_ENTRY_REPLY_FROM_APICULA.md))
confirmed our decode of `0x00039020` bit-for-bit against UG290, told us
`FLASH_LOCK` was a red herring with nothing to clear, and correctly named the
shape of the problem: a running auto-booted GW1N does not re-enter configuration
from CONFIG_ENABLE alone. We had that document sitting in the repo for two months
before we acted on it properly.

## Meanwhile, on our bench: the warm handoff

The same day, working from Stlkv's recipe on issue #18, we got our first live
trace under our own firmware — not from a cold boot, but by letting stock
configure the FPGA and then handing the still-configured part over to our code
with a reset.

That worked, and it taught us four things that all mattered the next day:

1. **The handoff method decides the engine state.** MENU+Power runs stock's
   upgrade-entry shutdown and leaves the capture engine *stopped*. The pinhole
   reset leaves it *free-running*. Same device, same firmware, completely
   different result — and we had been comparing across the two without noticing.
2. **The trigger DAC has to be re-armed.** A reset zeroes DAC1 (PA4), the CH1
   trigger comparator reference, and captures come back flat zero until you put
   it back at mid-scale.
3. **The analog front-end relay bank drops on MCU reset** and must be re-armed as
   outputs.
4. **PC12 is DC coupling** — HIGH passes DC — which we established with a live
   A/B, and which contradicted the relay table our own code was using.

The negative control passed: a true power cycle, with no stock configuration in
front of it, left the demo trace on screen. So the trace we were seeing really was
coming from the FPGA.

That gave us a two-minute development loop for everything on the scope side,
months ahead of when we would otherwise have had it. It was never shippable —
it needs stock flashed first and unbroken power — but it stopped being the plan
about eighteen hours later. See the next entry:
[**Cold boot to a live scope**](2026-08-13-cold-boot-to-scope.md).

## Postscript, 2026-08-13: it wasn't `0x05` either

When we reproduced Stlkv's result on our own bench, the comparison had a confound
in it. Our hardware-SPI sequence sends a `0x05` ERASE_SRAM frame in the prelude.
The V0.4 bit-bang loader doesn't. So "bit-bang versus hardware SPI" and "no `0x05`
versus `0x05`" were tangled together in every A/B we had.

maksidze proposed the obvious single-variable test on issue #18, and we ran it:
the hardware-SPI path with the `0x05` frame skipped. Result — `CFG:00039020`,
`DONE_FINAL` clear. **The wall, unchanged.** `0x05` is not load-bearing.

Which sharpens things rather than settling them, because of this table:

| path | result |
|---|---|
| stock, hardware SPI at `/64` (maksidze's capture) | **works** |
| ours, bit-bang, no `0x05` | **works** — `0x0003F460`, 4/4 cold boots |
| ours, hardware SPI, with `0x05` | fails — `0x00039020` |
| ours, hardware SPI, without `0x05` | fails — `0x00039020` |

Two of those four are hardware SPI and one of them works. So it is *not* "hardware
SPI can't do this", and it is not the prelude contents. Something in our own SPI3
setup or framing differs from stock's in a way we still cannot name. Config entry
is solved for shipping purposes — the bit-bang path configures reliably from cold —
but that last row is an open question, and it is the honest kind: we have a
working answer without a complete explanation.

## Footnote: a hypothesis graveyard, USB division

The FPGA wasn't the only place we got attached to a wrong idea. Our bench unit's
USB CDC debug shell stopped enumerating in July, and we produced two confident
theories about it, both dead:

- **Thermal.** The app clocks USB from the internal RC oscillator, so a warm-drift
  story was plausible, and one working enumeration had followed a cold boot. Then
  the device enumerated while hot, stayed up through 55,000 SPI reads, and the
  transition that restored it was a few-second unplug — far too short to cool
  anything. Also, a thermal fault severe enough to break USB would not leave the
  LCD, FreeRTOS, the meter and SPI3 all working perfectly at 240 MHz.
- **Reset type.** The idea that CDC survived a true power cycle but not a
  flash-reboot. Five flashes in one session killed it: a genuine power cycle
  produced CDC down, and a plain reboot produced CDC up.

What actually correlates, 5 for 5, is *which build is flashed* — specifically
which FPGA configuration path the image runs. That is a firmware bug in our own
boot path, not a host, cable, toolchain or temperature problem. The mechanism is
still unestablished, and we have deliberately deferred it: the builds the bench
needs are the ones where CDC works.

Two theories, both reasonable, both refuted by a cheap test. That has been the
shape of this entire project.
