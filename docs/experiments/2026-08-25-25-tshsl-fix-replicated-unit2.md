# EXP-25 — the tSHSL fix replicates on unit #2: wtest passes, fwload streams, on the Zbit clone

- **Date:** 2026-08-25
- **Unit:** bench unit #2 (Stlkv), W25Q socket populated with a Zbit ZB25VQ128 clone (id 5E17)
- **Build:** `make guest-coldtrace` at `6880b96` (= this branch merged with upstream
  `df96059`, carrying `479d120`), 598 948 B, crc32 `D5A4C58C`. Unmodified —
  no bench edits.
- **Status:** CONFIRMED

## 1. Problem

Upstream diagnosed every W25Q write failure as a tSHSL (CS-high deselect time)
violation in `flash_fs` and fixed it in `479d120`, bench-confirmed on unit #1's
genuine Winbond only. Unit #2 has shown the failure signature since 08-22
(`fwload` → `err=w25q erase/write/read` at 512 B; `flash wtest` →
`ERR: write_block#1`; `settings writes ok: 0`) on a different flash chip.
Does the same one-line timing fix cure the clone?

## 2. Hypothesis

If the root cause is the driver's CS-high timing (not unit #2's silicon):
`flash wtest` passes and a full `fwload` stream STAGEs on this unit with no
other change. If unit #2's failures were a second, chip-specific defect:
wtest keeps failing at `write_block#1` exactly as measured on 08-24.

## 3. Procedure

1. Stage the merged build into cache slot B from the running 2C23T port
   (its writer was never affected), `fwswap b` into it.
2. `flash wtest 0xFA0000 CONFIRM` — scratch region base, sector verified
   blank by the command itself.
3. The previously-failing path end-to-end: `python3 scripts/cdc_flash.py
   <the same 598 948 B image> --slot b --stage-only` — ~147 sectors of
   erase + program + per-chunk verify + full at-rest re-read, on THIS
   firmware's driver.
4. `fwswap a` home.

**Preconditions verified by readback** (not assumed):

| what | expected | measured |
|---|---|---|
| running build | merged branch build | `Build: Aug 25 2026 18:17:44` |
| slot table before test | A=port, B=this build | `A=131148/1624067E B=598948/D5A4C58C` |

## 4. Control

| control | expected | measured | passed? |
|---|---|---|---|
| pre-fix failure on THIS unit, same signature (08-24 session, PR #29 thread) | `fwload` dies at 512 B, wtest `ERR: write_block#1` | measured then, twice | yes (negative control, historical) |
| same-session known-good writer on the same chip (the port staging slot B) | STAGED | `STAGED slot=b size=598948` | yes |

The negative control is from a prior session (08-22/08-24), not re-run
tonight — re-demonstrating it would mean flashing a pre-fix build just to
watch it fail; the signature is recorded in the PR thread and EXPERIMENT-LOG.

## 5. Results

1. `flash wtest 0xFA0000 CONFIRM`: sector blank → **PASS page-program
   (in-place), PASS erase + read-modify-write, PASS sector restored** —
   first wtest pass ever recorded on this unit.
2. Full stream on this firmware: **598 948 B in 67.3 s (~9 KB/s),
   `fwload: STAGED slot=b 598948/598948 crc=D5A4C58C err=none`** — the
   path that died at byte 512 on 08-22 and 08-24 now completes, including
   the full at-rest CRC re-read. (The port streams the same image at
   ~19 KB/s; this firmware's intake erases lazily inline, hence slower.)
3. `settings` still shows `writes ok: 0` — **not evidence either way**: no
   save was attempted this boot (saves fire on button press / orderly
   power-off, and this session was shell-only). The underlying primitive
   is covered by wtest; the settings round-trip stays untested here.

## 6. Blind spots

1. Nothing here isolates tSHSL itself on this unit — no logic analyzer on
   CS. What is shown is fix-in → symptom-gone on the same chip that showed
   the symptom; the mechanism rests on upstream's unit #1 isolation.
2. One image size, one slot, one session; no endurance/erase-cycle claims.
3. The settings persistence round-trip (see 5.3).

## 7. Conclusion

- **Established:** upstream's `479d120` cures unit #2's W25Q write failures
  as-is — wtest passes and the fwload streaming half works on the Zbit
  clone. The failure was never this unit's silicon. PR #29's "Known limit"
  section is obsolete: both halves of the update path now work on both
  firmwares on both bench units.
- **Excluded:** a second, unit-#2-specific write defect underneath the
  tSHSL one (would have kept wtest failing).
- **NOT excluded (explicitly):** settings round-trip on this unit (untested
  tonight); long-term write endurance; anything about CS timing margins on
  other clone chips.
- **Follow-up:** drop the "in slot A always keep a bootable port image"
  bench rule to "keep it there anyway" — it is no longer load-bearing, both
  firmwares can now restage each other over the cable.
