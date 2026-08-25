# EXP-27 — a transfer that goes silent >3 s arms the drain; both firmwares, both branches

- **Date:** 2026-08-25
- **Renumbered 2026-08-26:** was EXP-24 on this branch (and EXP-21 before
  `629c701`). `main` took 24 for the software display trigger the same day, so
  this one moved rather than upstream's. PR #29's comment of 2026-08-25 cites
  the old filename.
- **Unit:** bench unit #2 (Stlkv)
- **Build:** this firmware: `make guest-coldtrace` at `8c8778e` (+ EXP-26's
  bench-only watchdog edit, irrelevant here); 2C23T port: its commit `008e36e`,
  131 148 B, crc32 `1624067E`.
- **Status:** CONFIRMED

## 1. Problem

Commit `1136141` (and the port's `008e36e`) added a path that was reasoned
about but never run: a transfer killed by the RX-silence timeout now arms the
same drain the feed-path error arms, so a host that stalled >3 s mid-image and
then resumed must have the remainder swallowed, not parsed as command lines.
Does it, on hardware — and does an abandoned drain give the shell back?

## 2. Hypothesis

If the timeout arms the drain: after streaming a partial image and pausing
>3 s, the silent-death verdict appears; a resumed remainder produces NO shell
output (no `Unknown command`, no prompt noise); and `version` afterwards
answers normally. If the timeout does NOT arm it (the pre-fix behaviour), the
resumed bytes hit the line editor and the log fills with command-parse noise.
Separately: a drain left unfed for >3 s must print its stopped-waiting line
and free the shell.

## 3. Procedure

Naive-host script (streams regardless of errors, pyserial, 2048 B writes).
On the port (`/dev/cu.usbmodem00012`, new build installed and verified by
`fwstat` first): `fwload 131148 1624067E b`, stream 4096 B, pause 3.5 s,
stream the remaining 127 052 B, then `version`. Drain-expiry variant: stream
4096 B, pause 3.8 s, stream 4096 B more, abandon for 3.8 s, then `version`.
On this firmware (same procedure, `fwload 597464 61A13811 b`) the stall is
placed at **256 B — below the first 512 B commit**, so unit #2's known W25Q
write defect (`flash wtest` fails on this unit; see PR #29 "Known limit")
cannot fire first: the timeout is reached with the intake still healthy.

**Preconditions verified by readback** (not assumed):

| what | expected | measured |
|---|---|---|
| port build running | slot A image `1624067E` | `FWC A size=131148 crc=1624067e`, booted from it |
| this firmware running | bench build | `Build: Aug 25 2026 17:44:35` |

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| full `fwload`+`fwapply` through the same port intake, same session | STAGED, installs, reboots | staged 131 148 B in 6.7 s, `STAGED slot=a`, device rebooted into it | yes |
| shell echo path before each run | `version` banner | banner correct | yes |

On THIS firmware a full-transfer control is impossible on unit #2 (the W25Q
write defect kills any intake at 512 B) — its intake-arm and verdict lines
serve as the path-alive control instead, and the full-path control lives on
the port side only. Stated as a weakness, not hidden.

## 5. Results

Port (three-way PASS, script-checked):

1. Stall at 4096/131148 → `fwload: ERROR rx went silent; run fwload again`.
2. Resumed 127 052 B → **zero bytes of shell output** (captured: `''`).
3. `version` → normal banner.
4. Expiry variant: second stall → `fwload: stopped waiting for the aborted
   image; shell is listening again`; shell alive.

This firmware:

1. Stall at 256/597464 → `fwload: ERROR slot=b 256/597464 crc=61A13811
   err=rx went silent; run fwload again` (+ cache table line).
2. Resumed 64 KiB → zero bytes of shell output.
3. Abandoned drain → `fwload: stopped waiting for the aborted image; shell is
   listening again`; `version` answers.

## 6. Blind spots

1. The owed-count fix (`usb_debug.c:7033`, up to 63 stolen keystroke bytes)
   is NOT exercised: both hosts here write in 64-multiple chunks, so commits
   stay 64-aligned and the over-count path never runs. It needs a host with
   odd write granularity.
2. On this firmware only the first 256 B of intake ran — everything past the
   first W25Q commit (including drain arithmetic after a mid-image W25Q
   failure) is exercised on the port only, plus by `make test-fw-loader` on
   the host.
3. macOS CDC only; a different host CDC stack could batch the resumed bytes
   differently.

## 7. Conclusion

- **Established:** on both firmwares the silence-killed transfer arms the
  drain (resumed remainder swallowed, shell survives), and an abandoned drain
  frees the shell after one more silence period, on real hardware.
- **Excluded:** the pre-fix failure (resumed image parsed as command lines)
  for stalls >3 s, on these builds.
- **NOT excluded (explicitly):** the 63-byte owed over-count on odd-granularity
  hosts (fix code-reviewed + host-tested only); mid-image W25Q-failure drain
  arithmetic on this firmware on this unit.
- **Follow-up:** none blocking; an odd-granularity host run would close blind
  spot 1.
