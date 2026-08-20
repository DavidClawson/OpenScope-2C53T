# Spec: <feature name>

> Copy this file to `docs/specs/<track>/<feature>.md`. One page. Delete the
> guidance quotes. A spec here is a **promotion ladder, not a wish** — the load-
> bearing section is the stage ladder, and every criterion in it must be
> checkable: a test that passes, a measurement with a writeup in
> `docs/experiments/`, or a behaviour demonstrable on the bench. Stage
> definitions (S0–S4) are in the README, [Feature maturity](../../README.md#feature-maturity).

**Track:** scope | meter | siggen | platform | modules
**Stage now:** S-none | S0 | S1 | S2 | S3 | S4 — must agree with the README matrix
**Champion:** who is driving it (blank = unclaimed)

## What it is

> One paragraph, user-visible behaviour. What does the person holding the
> device get that they don't have today?

## Prior art

> What stock does. What standard bench/handheld scopes do. What the sibling
> firmwares (rosenrot00, Stlkv's port) do. What owners actually ask for —
> cite `docs/community_wishlist.md` tiers where they apply. This section keeps
> us honest about table stakes vs. novelty.

## Our angle

> Why the open-firmware version is *better*, not just present: the USB CDC
> shell and `bench.py` scripting, 224KB SRAM, measured-or-refuse honesty,
> module packs. If there is no angle, say "parity" — that is a fine answer.

## Hardware dependencies

> Which measured facts this needs (cite the experiment doc), and which open
> unknowns block it. Be specific: "needs a sample rate" means "only meaningful
> on the 8 measured timebase codes; shows `--` on the rest".

## Stage ladder

> The heart. One row per promotion this feature still has ahead of it.
> Each criterion must be falsifiable. "Works well" is not a criterion;
> "Vrms of a known 1 kHz sine matches the commanded amplitude within the cal
> table's stated uncertainty, written up in docs/experiments/" is.

| To reach | Criterion (checkable) |
|---|---|
| S1 | … |
| S2 | … |
| S3 | … |
| S4 | … |

## Open questions

> Decisions a reviewer can actually weigh in on, with the options named.
