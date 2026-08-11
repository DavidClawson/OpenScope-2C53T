# The FPGA was listening the whole time

**2026-07-28 / 29**

## Background

The 2C53T's oscilloscope path runs through a **Gowin GW1N-UV2 FPGA**. The stock
firmware uploads a 115,638-byte FPGA configuration over SPI at every boot. If you
stop it doing that — we tested this by changing a single byte in the stock
firmware — the multimeter still works but the scope is stone dead.

Our replacement firmware sends the same bytes in the same order and the FPGA does
nothing. That has been the blocking problem on this project since roughly June.

## What we thought we knew

Back in June, a probe read four different SPI commands and got **identical**
replies to all of them, including a repeating 4-byte pattern. The conclusion was
reasonable: the FPGA isn't parsing our commands at all, it's just free-running a
fixed pattern and ignoring us. That conclusion is what sent the project toward
building a JTAG programming rig as the way forward.

A status register we could read seemed to confirm it, returning a stable
`0x8001C810` every time. Stable numbers feel like real numbers.

## What actually happened

The SPI clock was wrong.

Reads from this FPGA are only valid at a slow clock (~470 kHz). We were reading
at 60 MHz, where the data arrives after the sampling edge and you latch the
*previous* bit. Every status value this project had recorded was garbage — but
*consistent* garbage, which is far more dangerous than obviously-wrong garbage.

`0x8001C810` turned out to be a real value read one bit early. And the "free-
running pattern" was the status register itself, read misaligned.

## The fix that mattered wasn't a code change

We had never once read a value whose correct answer we knew in advance.

So we added one: the FPGA has an **IDCODE** register whose value we know
independently (`0x0120681B`, from the Gowin bitstream header). Read that first.
If it doesn't come back correct, throw the whole measurement away.

The first time we ran it:

```
0x11 READ_IDCODE  ->  0120681B 0120681B   the known answer, exactly
0x00 no-op        ->  FF...               idle line
0x13 USERCODE     ->  00000000
0x41 STATUS       ->  00039020
```

Four commands, four different answers. **The FPGA decodes our commands perfectly.**
It always had. The bus, the wiring, the SPI mode, the framing — all correct.

That refuted the June conclusion, and with it the main argument for the JTAG
route.

## So where does it actually fail?

With a trustworthy instrument we could finally ask precise questions. We read the
FPGA's status at all six steps of the configuration sequence:

```
before        after ERASE   after INIT   after CONFIG_ENABLE   after 115KB   after close
00039020      00039020      00039020     00039020              00039020      00039020
```

**Not one configuration command changes the register by a single bit** — while
every *read* command in the same window answers correctly.

And there are no error bits. No CRC error, no bad-command, no ID-verify failure.
The bytes aren't being rejected; they're being **silently discarded**, because the
FPGA never enters configuration mode in the first place.

That matches the documented behaviour of a Gowin part that has already booted its
own resident design: it will answer queries all day, and refuse to be
reconfigured until it sees a reset pulse or a power cycle.

## What we ruled out after that

Since then, and all measured rather than argued: the bitstream contents, the
framing, the timing, the analog front-end state, the MCU's register state, the
clock tree, and every GPIO pin the stock firmware pulses. We also confirmed that
the stock firmware's FPGA sees **exactly the same state** ours does at the moment
of the critical command — and stock succeeds anyway.

That contradiction is where the investigation currently sits.

## The lesson, which cost six weeks

A measurement with no known-correct answer isn't a measurement. Three separate
artifacts survived for weeks in this project — a wrong clock, a floating input,
and a byte-rotation in a script — purely because nothing ever checked itself.

Every FPGA read now starts by confirming the IDCODE. If that fails, the tool
refuses to print anything rather than hand you a plausible number.

Full technical write-ups are in
[`reverse_engineering/analysis_v120/`](../../reverse_engineering/analysis_v120/),
particularly `expE_swd_state_diff_2026-07-28.md`.
