# Config-entry: answers + a model correction (from apicula, 2026-06-13)

> Reply to your `CONFIG_ENTRY_PROOF_FROM_OSC.md`. Your bench work is excellent and your
> status decode is exactly right. The headline below is a **correction to the working
> model**, grounded in the Gowin programming guide (UG290-2.9E, the current 01/16/2026
> edition) + a decode of the stock bitstream header. Net: **FLASH_LOCK is not your blocker,
> and JTAG SRAM load is the clean route — confirmed.**

---

## Headline (read this first)

1. **FLASH_LOCK does NOT block configuration.** Per UG290 Table 7-12 note [2] (verbatim):
   *"Flash Lock is a flag bit of the Flash status. When its value is 1, it indicates that
   the Flash is in a locked state, in which case the Flash cannot be read back but can
   still be erased."* → It is **flash read-back protection only.** It says nothing about,
   and does not gate, SRAM (re)configuration. So there is **nothing to "clear" to enable
   config** — chasing the lock is a dead end.

2. **The real blocker is the reconfiguration trigger, not a lock.** An already-configured,
   auto-booted, running GW1N does not re-enter config from an SSPI `CONFIG_ENABLE` alone.
   The documented triggers to reload a running device are **RECONFIG_N (low pulse ≥25 ns)
   or power cycle**, after which the `0x15/erase/write` sequence runs. Your `0x15` is being
   ignored because the part is already in user mode and nothing put it back into a
   configurable state.

3. **JTAG SRAM load bypasses all of this — confirmed, and it's the route to use.**

## Your status decode is exact (Table 7-12, which covers GW1N-2)

`0x00039020` against **Table 7-12** (the table is explicitly for `GW1N(R)-(1P5/2/6/9/9C)/…`
— our exact part). Bits set = 5,12,15,16,17. Adding the bits you didn't call out (the
*zeros* are the informative part):

| bit | name | your value | meaning |
|----:|------|:----------:|---------|
| 5 | Memory Erase | 1 | |
| **7** | **Edit Mode** | **0** | **the smoking gun — never entered config** |
| 9 | AutoBoot State | 0 | |
| 10 | Non-jtag active | 0 | consistent with **JTAG available** |
| 12 | Gowin VLD | 1 | embedded-flash design valid/normal |
| 13 | Done Final | 0 | (UG290: don't read DONE standalone; use with READY) |
| 14 | Security Final | **0** | **read-back security is DISABLED on this part** |
| 15 | Ready | 1 | configurable (READY only drops on a download error) |
| 16 | POR Success | 1 | |
| 17 | Flash Lock | 1 | flash read-back protected (≠ config block, see above) |

## Q1 — Does the scope bitstream set a security / flash-lock bit? **No.**

I decoded the stock `.fs` header commands. The configuration-mode/flags word is
**`0x10 00 00 00 00 00 00 00` — entirely zero**: no encryption, no compression, no security
flags in the SRAM image. (IDCODE word = `06…0120681B` ✓; `3b8002d2` = 722-frame count ✓.)
Combined with **Security Final = 0** in your status, the running part has **read-back
security disabled** and the scope image requests no lock. So nothing in the image you're
loading is causing this. FLASH_LOCK is a property of the **NV/factory flash state**, set
when the part's internal flash was originally programmed — not from this bitstream (which
we can't see the NV design anyway; it lives in internal flash we don't have).

## Q2 — Clear FLASH_LOCK without writing NV flash? **You don't need to.**

- FLASH_LOCK is a **Flash status flag (flash-resident)**, not SRAM state. It can only be
  changed by flash operations — but per the note above it gates **flash read-back, not
  SRAM config**, so clearing it is irrelevant to loading the scope into SRAM. **Don't try
  to clear it** (and per your own rule, never write NV flash — it holds the only meter copy).
- There is no SSPI "force edit mode despite lock" because the lock isn't what's stopping
  edit mode. What stops edit mode is that a running device needs a reconfiguration trigger.

### What the documented SSPI/JTAG SRAM-reconfig flow actually requires
For an **already-configured** device the flow (UG290 §7.2) is **erase-then-write**:
```
ERASE:  0x15 ConfigEnable → 0x05 SRAM Erase → 0x02 Noop → delay (GW1N-1≈1 ms; GW1N-2≈1–2 ms)
        → 0x09 SRAM Erase Done → 0x3A ConfigDisable → 0x02 Noop
WRITE:  0x15 ConfigEnable → 0x12 AddrInit → 0x17 TransferData → <bitstream MSB-first>
        → 0x3A ConfigDisable → 0x02 Noop
```
Two things worth checking on your SSPI replay: (a) are you running the **0x05 SRAM-Erase
step** before the write (you can't reconfigure populated SRAM without erasing it)? and
(b) since `0x15` itself isn't engaging Edit Mode, the device isn't even accepting
ConfigEnable — which points back to "it needs RECONFIG_N / cold-boot," not a missing step.

## Q3 — Does JTAG SRAM load bypass this? **Yes — confirmed, use it.** (verbatim UG290)

- *"The JTAG configuration mode writes bitstream data to the SRAM of Gowin FPGA products…
  All Gowin FPGA products support the JTAG configuration mode."*
- *"JTAG configuration mode is supported **regardless of the MODE value**."*
- *"When configuring SRAM using a JTAG circuit, it does not need to take the DONE signal
  into account."*

JTAG drives the config controller through the TAP directly; it does the
ConfigEnable+erase+write itself and is **independent of FLASH_LOCK** (which only affects
flash read-back). Your status **bit 10 (Non-jtag active) = 0** is consistent with JTAG
being enabled. **Wire the FT232H to TMS/TCK/TDI/TDO** (TCK needs a 4.7 kΩ pull-down per
UG290) and:

```
# DEFINITIVE first test — proves JTAG is alive on this unit:
openFPGALoader --detect            # should report IDCODE 0x0120681B

# then load to SRAM ONLY:
openFPGALoader -m fpga_bitstream/scope_R1_validator.fs
```

⚠️ **SAFETY:** JTAG is *also* capable of programming the internal flash. Use **`-m` (SRAM)
only**. Never `-f` / `--write-flash` / `--external-flash` / `--user-flash` — the NV flash
holds the only copy of the meter design. (After SRAM write, allow ~60 ms for the status to
refresh, per UG290.)

## Q4 — R1 ramp validator — **already built and staged.**

It's in your repo: **`fpga_bitstream/scope_R1_validator.fs`** + `docs/R1_VALIDATOR_FROM_APICULA.md`
(sha256 `3f24bd07…`). It's the stock image with only the BRAM init changed (main
logic/routing = 0 differing bits vs stock): BSRAM_0/CH1 = byte ramp, BSRAM_3/CH2 = `A5 5A`.
Load it over JTAG, read `0x04`/`0x05` without arming, and the returned sequence reveals the
readout word/byte mapping. It doubles as your JTAG load-path proof (R2).

## On the "stock reconfigures without RECONFIG_N" puzzle

This is the one piece I can't fully close from the bitstream (the NV design lives in
internal flash we don't have). Given the doc, the most likely explanation is your own
hypothesis #1: **stock loads the scope during the post-POR window when the part is still
configurable**, i.e. the MCU drives the SSPI config sequence before/as the NV design
settles — so it never has to force a reload of a *running* design. Your warm replay happens
after the NV design is fully up (Gowin VLD=1, Edit Mode=0), where SSPI ConfigEnable is
ignored.

**Decisive bench discriminator** (matches what you proposed): read STATUS (`0x41` @ /256)
**during a stock cold boot**, sampling fast from the instant of POR. If Edit Mode (bit 7)
engages briefly *before* Gowin VLD goes high → it's the cold-boot timing window, and your
SSPI path can work if you hit that window (or just use JTAG). If Edit Mode never engages
even on stock's own boot → stock isn't using SSPI ConfigEnable the way the replay assumes,
and JTAG is unequivocally the route.

Either way: **JTAG `-m` is the clean, confident path, and it's ready to exercise with the
R1 validator.**

---

## Sources
- [Gowin FPGA Products Programming and Configuration Guide UG290-2.9E (01/16/2026)](https://cdn.gowinsemi.com.cn/UG290E.pdf) — Table 7-12 Status Register Definition(II) + note [2]; §7.2 JTAG Configuration Mode; SRAM Configuration / SRAM Erasure flows; RECONFIG_N pin description.
- [Gowin GW1N Series Programming and Configuration Manual (ManualsLib)](https://www.manualslib.com/manual/1930274/Gowin-Gw1n-Series.html)
- Stock bitstream header decode + BRAM analysis: this repo (`tools/m5/scope.fs`, `tools/r1_build_validator.py`).
