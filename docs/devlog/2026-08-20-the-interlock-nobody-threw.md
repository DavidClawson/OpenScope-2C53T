# The interlock nobody threw

*2026-08-20*

Yesterday ended with a three-time-refuted mystery: change the timebase, hit a
documented flush point, power-cycle — and the boot reconcile said `pulled`
every time, never `pushed`. The settings store's five diagnostic fields had
never been read by anything, so the session ended the only honest way it
could: with a shell command to read them, and no diagnosis.

Tonight the diagnosis took four steps, each one cutting the space in half.

## Four halvings

1. **`settings` at boot:** `storage bound: YES`, `boot load: no saved record`.
   The region layer binds; the loader scans and finds nothing. So either
   writes never happen, or they land where the loader doesn't look.
2. **`flash read 0xF10000`:** the settings region is 100% `0xFF`. Virgin.
   No record has *ever* existed — the loader was telling the truth, and every
   theory about the load half died at once.
3. **One button press, then MENU:** `changes_seen: 1` — detection works.
   `write_failures: 1` — the flush fired at the mode switch exactly as
   designed, called `config_save()`, and was refused.
4. **`flash wtest 0xFA0000`:** 3/3 PASS. The W25Q write primitive is fine.

What's left after four halvings is one line: `config.h:168`,
`#define SETTINGS_PERSIST_WRITES 0`.

## It wasn't a bug. It was an interlock — a good one — with no calendar entry

The persistence code was the first thing in this project's history to write
the external flash at runtime. Its author (2026-08-13) reasoned carefully
about the exposure — factory cal is address-enforced read-only 14 MB away;
the real risk was the region map having been derived from one unit — and
gated every write behind a build flag, off by default, with a supervised
commissioning procedure written into the Makefile under `guest-persist`:
dump the region, confirm blank, flash, round-trip a setting, dump again.

Then `guest-coldtrace` became the standard bench build, and the commissioning
step was never run. For a week, every flush on every build refused —
silently, by design — while the README matrix said settings persistence was
"real, to the external flash." **That row is the maturity table's first
overstatement.** Every false claim the table had caught before understated
the project; this one flattered it, and it got there the same way: written
from the code's intent, not from a record in flash.

## The conflation the code itself warned about

The sharpest part. `config.h:162`, written with the interlock:

> A refusal we chose is not a failure we suffered, and any diagnostic that
> conflates them is lying in the way this project keeps getting burned by.

The store's `write_failures` counter conflates exactly that: `write_now()`
counts a disabled-build refusal as a failure. The counters that distinguish
them — `saves_disabled` vs `saves_failed`, and a `writes_enabled` bool put
there "so a reader never has to guess which build produced them" — existed,
unprinted, one struct away. Yesterday's `settings` command read the wrong
struct, and the first live reading blamed "the write path" when the truth
was "the build you flashed refuses on purpose." The command now prints both.

## Commissioning

`make guest-persist` (= coldtrace + the flag), then the Makefile's own
procedure, run for the first time:

- Region confirmed blank; scope still comes up configured and capturing.
- Timebase → `0x0D`, MENU: `writes ok: 1`, and at `0xF10000` the **first
  settings record in the project's history** — 56 bytes, `2CSO` magic,
  `0x0D` sitting at payload offset 8.
- Power cycle: `boot load: loaded`, display **and** FPGA register both at
  `0x0D`, 123,663 S/s in force. The reconcile finally printed `pushed`.
- Twice more: three consecutive passes, matching the three consecutive
  failures. The record count stayed at exactly one — each power-off flush
  captured an image byte-identical to flash and elided the write, which is
  the wear design working on its first day in service.

The flip condition the author set — "change this default only once that has
passed on hardware" — is met, so `SETTINGS_PERSIST_WRITES` now defaults
to 1. And yesterday's other observation dissolves with it: the five blank
`--/div` presses at boot were this same fault seen from the front. The
device booted into the arm block's unmeasured `0x08` because nothing could
remember better; now it boots into the last measured code you chose.

## Lessons

- **An interlock with no commissioning date is a latent outage.** The flag
  did its safety job perfectly and then kept doing it after the danger had
  passed, because "flip it once the bench check passes" had no owner and no
  reminder. If a gate needs a ceremony to open, schedule the ceremony when
  you build the gate.
- **The warning in the code was about the code.** The author who wrote
  "any diagnostic that conflates them is lying" also provided the fields to
  avoid it — and the diagnostic written a week later conflated them anyway,
  because it read the nearer struct. A warning comment is not a guard; the
  guard is printing `writes_enabled` where the failure count appears.
- **Four reads beat three theories.** Yesterday's three power-cycle tests
  consumed a bench session and produced one bit ("still broken"). Tonight's
  four instrument readings each eliminated half the hypothesis space and
  the whole diagnosis fit in twenty minutes. The instrument existed all
  along; it just had no caller — the same shape as the `/2` SSPI reads, the
  floating MISO, and the unread status fields this very story started with.
