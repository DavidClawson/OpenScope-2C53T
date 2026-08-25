# The flash was never deselected

*2026-08-25*

The SPI flash on this board could read but not write. Not "wrote the wrong
bytes" — every erase and every program returned an error and left the chip
untouched, while reads were flawless. That combination had been sitting under
three separate features (settings persistence, factory-cal backup, and the
USB-firmware-update PR #29) quietly not working, each blamed locally. Today it
turned out to be one bug in the flash driver, and it was a *timing* bug — the
kind that is invisible in the source and only shows up on a real chip.

## The starting point

The one thing owed on Stlkv's #29 was to run `flash wtest` on unit #1 — the
self-protecting write self-test that erases a blank sector, exercises both
write paths, and restores it. It failed:

```
wtest @0xD00000: sector blank, OK to proceed
ERR: write_block#1
ERR: restore erase
```

Same at every address tried. Stlkv had reported the identical signature on his
unit #2. So the first real result was a **cross-unit reproduction** — which
meant it was not a flaky chip.

## Ruling things out, one instrumented build at a time

The device runs read-protected, so there is no live debugger here (attaching
SWD kills the CPU — a saga from earlier in this project). Every step was a
build → flash → shell-command loop, each flash a MENU+Power dance. Worth it,
because each round killed a hypothesis:

- **The chip is alive and reads work.** JEDEC came back `EF 40 18` — a genuine
  Winbond W25Q128. Reads returned the real FAT12 boot sector at `0x0` and a
  JPEG at `0x100000`. Not a floating bus.
- **Not write protection.** Status registers showed block-protect bits clear,
  WPS clear, and the write-enable latch (WEL) *setting* after WREN.
- **Not the allocation.** `flash_fs_raw_write_block` mallocs a 4 KB RMW buffer;
  the heap had 14 KB free and the malloc succeeded.
- **The raw primitives work perfectly in isolation.** A step-resolved
  diagnostic that inlined WREN → erase → program with a status read between
  every step **passed** — erase accepted (BUSY rose), completed, sector read
  `0xFF`; program accepted, byte read back exactly. So the low-level driver was
  not broken… when driven a certain way.

That last point was the whole puzzle: the inline diagnostic worked every time,
the public `flash_fs_raw_sector_erase`/`write_block` failed every time, **on the
same address in the same session.** Same primitives, same mutex, same SPI
config (proven by forcing a re-init — it changed nothing). And no SPI transfer
timed out. The instrumented `raw_write_enable` was reading a *genuine* `0xFF`
from the status register: the flash simply wasn't driving MISO.

## The one difference

A genuine `0xFF` with no timeout means the flash didn't consider itself
selected during that read. But the CS pin was a proper output, idle high, and
`force_reinit` re-applied it with no effect. What was left?

The inline diagnostic and the real driver did the *same* WREN-then-read — except
the diagnostic put a **function call** between the WREN transaction's CS-deassert
and the read's CS-assert, and the real driver did them back-to-back. That
function call was worth a few tens of nanoseconds of extra CS-high time.

That is exactly **tSHSL** — the minimum time a W25Q needs CS held high to
register that it has been deselected between commands. Back-to-back with only a
few instructions between them, CS never stayed high long enough. The flash never
saw the deselect, treated the read as a continuation of the WREN frame, and left
MISO undriven. `raw_write_enable` read the floating `0xFF`, decided the bus was
dead, and aborted — so no erase, no program, ever.

The fix is one macro: hold CS high ~1 µs after every deassert.

```c
#define SPI_FLASH_CS_DEASSERT()                                         \
    do {                                                                \
        GPIOB->scr = GPIO_PINS_12;                                      \
        for (volatile uint32_t _cs_hi = 0; _cs_hi < 120u; _cs_hi++) {}  \
    } while (0)
```

`flash wtest` passes now: page-program in place, erase + read-modify-write,
sector restored to `0xFF`. A public erase/write round-trip reads back the exact
pattern written.

## Why it explains everything

- **Reads always worked** because a read is a *single* self-contained
  transaction — there is no back-to-back command framing to violate.
- **It reproduced on two different flash chips** — a genuine Winbond here, a
  Zbit ZB25VQ128 clone on Stlkv's unit — because it was our driver's timing,
  not the silicon. Stlkv's side-by-side register comparison had already
  narrowed it to "the driver or its bus setup, not the chip"; this is which.
- **Three unrelated features were broken by one line.** Settings persistence
  reported `writes ok: 0 ever`; the factory-cal backup path had nowhere to
  write; #29's `fwload` streaming failed at 512 B with
  `err=w25q erase/write/read`, while `fwswap` — read-only from the W25Q —
  always worked.

## The method note

This is another entry for the project's running theme: a *stable, plausible
wrong number* is the signature failure mode, and the way out is a measurement
with a known-correct answer. The inline diagnostic that "shouldn't" have
differed from the driver was the known-good reference; the entire diagnosis was
just chasing down why two byte-identical sequences behaved differently, until
the only remaining difference was the gap between them. The bug was in the
silence between two commands, which is precisely where source review does not
look.

Thanks to Stlkv (#29), whose independent second-unit reproduction and honest
"it's the driver, not the chip" is what turned a local annoyance into a
findable bug.
