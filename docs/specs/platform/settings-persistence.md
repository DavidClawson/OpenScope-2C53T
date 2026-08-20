# Spec: Settings persistence that actually persists

**Track:** platform
**Stage now:** S2 — repaired and commissioned 2026-08-20. The "open defect"
this spec shipped with lasted one bench session: writes were refused by a
build-time interlock (`SETTINGS_PERSIST_WRITES=0`, `config.h`) whose
supervised commissioning step had never been run. First record written,
restored and pushed to the FPGA across three consecutive power cycles; the
default is now 1 and every build persists. See the devlog,
`2026-08-20-the-interlock-nobody-threw.md`.
**Champion:** DavidClawson

## What it is

Change a setting, power off however you like, power on: the device is the way
you left it. Today that is true only sometimes, and — worse — we cannot yet
say *why* the failing case fails.

## Prior art

Wishlist Tier 1 #2's sting is exactly this: stock "reverts to Automatic mode
and… beeps at you" every mode re-entry. Every instrument owners like remembers
its state. Stock persists via a saved config in MCU internal flash
(`0x08006000`); ours appends records to the W25Q through the region layer.

## Our angle

The append-log design is right (wear-friendly, stock regions untouched). What
was missing is the project's own standard applied to itself: the store has
carried five diagnostic fields (`storage_bound`, `load_result`, `writes`,
`write_failures`, `changes_seen`) since it was written, and **no caller ever
read one** — the same shape as the `/2` SSPI reads and the unread `vdiv_table`.
The `settings` shell command (commit 5e659f2) finally wires the instrument.

## Hardware dependencies

None beyond the W25Q region layer, which is live. The blocker is diagnostic,
not hardware: the failure is invisible from the front panel because "write
never reached flash" and "load never came back" present identically (boot
lands on the arm block's 0x08 either way).

## Stage ladder

| To reach | Criterion (checkable) |
|---|---|
| ~~S2 (repair)~~ | **DONE 2026-08-20.** The diagnostic localised it in four halving steps (region blank → detection works → flush fires → `flash wtest` green → `config.h:204`). Root cause was neither half broken: a commissioning interlock nobody had thrown. Three consecutive restore passes on unit #1, matching the three consecutive failures. |
| S3 | Host test over the append-log encode/decode with corrupted-record and torn-write cases; on-device: a scripted `bench.py` change→cycle→verify loop as an acceptance test. Negative control: a build with the write path stubbed must fail the loop. |
| S4 | Kill the settle-window foot-gun: either write-through on every change (measure flash wear first) or make the 2 s window flush on *every* screen exit, so "a change carries only if a later button press follows" stops being a sentence users need to read. |

## Open questions

1. If the diagnosis says the *write* half fails only on some boundaries, is
   the MENU flush actually executing, or gated behind a condition that isn't
   met in scope mode? (`settings_store_flush` exists; who calls it, when?)
2. Should timebase persist at all when the persisted code is unmeasured?
   Current reconcile policy says no (pull down to 0x08). Revisit once codes
   0x09–0x0C get rates.
