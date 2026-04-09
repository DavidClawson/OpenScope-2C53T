# Scope Raw Command Recovery — 2026-04-08

## Why this pass mattered

The previous scope experiments assumed the stock firmware turned internal
scope setup commands into raw UART low bytes `0x0B..0x11` with only the
high-byte parameter missing.

That assumption now looks wrong.

Tracing the stock queue flow shows three distinct layers:

1. Mode/setup code sends **1-byte internal commands** to `usart_cmd_queue`
   at `0x20002D6C`.
2. The FPGA task consumes that queue and runs helper code that builds a
   **16-bit TX word** at `0x20002D54`.
3. `dvom_tx_task` dequeues that 16-bit word from `usart_tx_queue`
   (`0x20002D74`) and splits it into:
   - TX byte `[2]` = high byte
   - TX byte `[3]` = low byte

That means the internal command byte and the raw FPGA low byte are not
necessarily the same thing.

## Confirmed anchors

- `0x080373F4` (`dvom_tx_task`) dequeues a 16-bit word from
  `usart_tx_queue` and sends:
  - low byte -> TX `[3]`
  - high byte -> TX `[2]`
- `0x0800B908` is the boot/mode dispatcher that queues **internal**
  command bytes, not raw UART words.
- The real raw-word builders are the small runtime helpers that write to
  `0x20002D54` and then queue that halfword to `0x20002D74`.

## Recovered raw TX words

These are direct writes to `0x20002D54` recovered from the stock image:

- `0x02A0`
- `0x0501`
- `0x0503`
- `0x0508`
- `0x0509`
- `0x0514`

Those are much stronger candidates than our older guessed `00 0B..11`
frames because they come from the stock raw-word builders.

## Recovered scope-bank low bytes

One runtime helper at `0x08006172` chooses a raw low byte through a
range/channel branch and then sends `0x0500 | low_byte`.

Recovered low-byte set:

- CH1-flavored: `0x0C`, `0x0E`, `0x10`, `0x11`
- CH2-flavored: `0x0D`, `0x15`, `0x16`, `0x17`

This is the strongest evidence so far that our old
"internal scope commands 11..17 == raw UART low bytes 0x0B..0x11"
model was wrong.

## Important caveat

Another stock helper does a lookup through an address formed as:

- `movw r2, #0xB3FC`
- `movt r2, #0x080B`
- effective address `0x080BB3FC`

That address is **above the end of the downloaded vendor V1.2.0 file**
we have locally (`0x00000000..0x000B767F` when rebased to `0x08000000`).

So there are two plausible readings:

1. The website firmware image is missing a tail region that shipped
   devices may actually contain.
2. Our current address assumptions for that helper are stale somewhere
   upstream.

Either way, it means some of the scope lookup material we want may not be
recoverable from the downloaded app image alone.

## Best current hypothesis

The scope failure is now more consistent with a **raw-command mismatch**
than with a dead transport:

- meter-side USART traffic is alive on hardware
- scope heartbeat re-arm is alive
- but we were likely sending the wrong raw scope low bytes

The missing dynamic lookup at `0x080BB3FC` also gives real weight to the
"downloaded firmware is not the whole device image" hypothesis.

## Next bench sequence

Using the existing CDC shell, the best stock-derived raw sweep is:

### Minimum recovered set

```text
fpga scope wake
fpga diag clear
fpga frame 0x02 0xA0
fpga frame 0x05 0x01
fpga frame 0x05 0x03
fpga scope acqmode
fpga scope beat 10 50
status
fpga acq
spi3 read 32
```

### CH1-flavored recovered bank

```text
fpga scope wake
fpga diag clear
fpga frame 0x02 0xA0
fpga frame 0x05 0x01
fpga frame 0x05 0x0C
fpga frame 0x05 0x0E
fpga frame 0x05 0x11
fpga frame 0x05 0x10
fpga frame 0x05 0x03
fpga scope acqmode
fpga scope beat 10 50
status
fpga acq
spi3 read 32
```

### CH2-flavored recovered bank

```text
fpga scope wake
fpga diag clear
fpga frame 0x02 0xA0
fpga frame 0x05 0x01
fpga frame 0x05 0x0D
fpga frame 0x05 0x17
fpga frame 0x05 0x16
fpga frame 0x05 0x15
fpga frame 0x05 0x03
fpga scope acqmode
fpga scope beat 10 50
status
fpga acq
spi3 read 32
```

## Tooling

`scripts/scope_entry_sweep.py` now includes matching built-in candidates:

- `stock_raw_min`
- `stock_raw_ch1`
- `stock_raw_ch2`
- `stock_raw_aux`

`fpga frame` parses bytes with base-0 semantics, so raw values should be
sent as `0x..` tokens, not bare `0C`-style strings.

## Bench result on current board

The recovered raw-bank sweep was run live through the CDC shell with:

- `stock_raw_min`
- `stock_raw_ch1`
- `stock_raw_ch2`
- `stock_raw_aux`

All three transmitted correctly, including the previously untested
`0x02A0` frame.

Observed result stayed flat in every case:

- `RX bytes = 0`
- `Data frames = 0`
- `Echo frames = 0`
- `HS = FF FF FF FF FF FF FF FF`
- `fpga acq` remained all `FF`
- `spi3 read` remained all `E3`

So the recovered raw words are a better-grounded scope hypothesis, but
they are still **not sufficient** to wake scope traffic on the current
board.

That pushes the remaining suspicion toward one of:

1. missing dynamic lookup data outside the downloaded app image
2. a stock-only precondition before the raw scope bank
3. board-specific FPGA / flash contents that are not reproduced by the
   website image

## Stronger evidence that the vendor app image is incomplete

The downloaded vendor ZIP contains only:

- `APP_2C53T_V1.2.0_251015.bin`
- `Update Log.txt`

No sidecar table file, no bootloader image, and no second data blob.

But the app binary contains **many absolute flash references** built with
`movw`/`movt` pairs targeting `0x080Bxxxx`, including:

- `0x080BB3EC`
- `0x080BB3FC`
- `0x080BC18B`
- `0x080BC1A5`
- `0x080BC841`
- `0x080BE118`

The downloaded app file ends at:

- `0x080B7680`

So at least 63 distinct absolute flash references in the stock code point
past the end of the vendor-supplied image.

That is a strong indication that one of these is true:

1. the downloadable app image is truncated relative to what ships on the
   device
2. the shipping device contains additional programmed flash regions beyond
   the website app blob
3. the stock firmware depends on data written separately during factory
   programming

This is no longer just a vague hypothesis; the binary itself now points to
addresses that are not present in the vendor package we have.
