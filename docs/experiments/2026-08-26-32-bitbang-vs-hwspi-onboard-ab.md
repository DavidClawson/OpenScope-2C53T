# EXP-32 (H7 step 1) — bit-bang vs hardware-SPI, on-board A/B from one build

- **Date:** 2026-08-26
- **Unit:** 2C53T bench unit #1
- **Build:** `make guest-bringup-bb` (guest-bringup + `FPGA_CONFIG_B` so the
  bit-bang loader is COMPILED but not run at boot — `FPGA_BUS_RELEASED_BOOT` takes
  the early-return branch first). New shell verb `fpga configbb` fires bit-bang on
  demand; `fpga reinit … k<br>` fires hardware-SPI on demand. Same build, same boot
  path, identical off-SPI pin state → the cleanest A/B we can make.
- **Status:** Two results, one of them a CORRECTION of a mistake made this session.

## 1. Result A — bit-bang config SUCCEEDS from this build (3/3)

Power-cycle → anchor open (IDCODE `0x0120681B`, STATUS `00039020`) → `fpga configbb`:

```
post-upload STATUS(0x41): 0003F460   DONE_FINAL(bit13)=YES -- CONFIG TOOK
```

`0x0003F460` is the documented success value (`00039020 → 0003F460`). Reproduced
3/3, including two LA-captured runs. **So the FPGA is configurable by the MCU on
PB3/4/5/6 with our 115,638-byte payload — the shippable coldtrace path — and it
works when driven from the bus-released `guest-bringup` posture too.** Off-SPI pin
posture is therefore NOT load-bearing for a successful config (bit-bang succeeds
from both coldtrace's full warm posture and guest-bringup's bus-released posture).

## 2. Result B (CORRECTION) — write clock rate is NOT the differentiator

**The mistake:** in the first pass I ran `fpga configbb` (bit-bang, which SUCCEEDS
and CLOSES the port) and then, *without a power cycle*, ran `fpga reinit 0 100 600
pe k0` (hardware-SPI at /2). It returned all-00 (IDCODE silent, `0x80` status
marker), which I over-read as "/2 hardware-SPI config closed the port = write rate
was the wall." It was not: the port was **already closed by the preceding
bit-bang**. Classic un-isolated A/B.

**The reproduce (proper isolation) refutes it:**

```
power-cycle → anchor OPEN (0x0120681B) → fpga reinit 0 100 600 pe k0
  0x41 STATUS:        00039020
  post-0x15 STATUS:   00039020   EDIT_MODE(bit7)=no
  acqread:            01 C8 10, span=200 (free-running = unconfigured, port OPEN)
```

Hardware-SPI config at /2 (stock's write rate) **walls exactly like /256**. The
port stays OPEN, EDIT_MODE never engages. **Write clock rate is excluded.** (The
`guest-coldtrace-hwspi` confirmation build was made for the write-rate premise and
is now moot; kept in-tree, not flashed.)

Bench-discipline note: the reproduce is mandatory precisely because of the
project's recurring "stable plausible wrong result" failure mode. It caught this
one before it entered the record as a breakthrough.

## 3. Standings after this session

| path | drive | mode | rate | framing | result |
|---|---|---|---|---|---|
| **stock** | AF hardware-SPI | 3 | /2 (60 MHz) | 05/12/15 + upload | ✅ |
| **our bit-bang** | GPIO | 3 | ~2 MHz | V0.4 reads, no-05 | ✅ |
| **our hardware-SPI** | AF | 3 | /2 **and** /256 | 05/12/15 + upload | ❌ |

Excluded as the bit-bang-success-vs-HWSPI-failure differentiator: bytes (issue
#18), 0x05 (EXP-27), inter-byte gap (EXP-26), intra-frame cadence (EXP-30),
CS-high dummy (EXP-29), SPI3 CTRL1 + pin config (H6), **write clock rate (this,
both /2 and /256)**, and — from EXP-31's wire capture — every SPI3 transport
defect (the bytes are byte-perfect on the physical wire). Drive strength matches
(`STRONGER` on both). SPI mode matches (3 on both).

## 4. The reframe

There are **TWO independent working config paths** (stock-AF, our-bitbang-GPIO)
and one failing path (our-AF-hardware-SPI). The bit-bang's success is a *separate*
working mechanism — it does not, by itself, explain why our AF path fails, because
**stock's AF path succeeds**. So the sharp remaining question is narrower than
"bit-bang vs hardware-SPI":

> **What does STOCK's AF hardware-SPI do that OUR AF hardware-SPI does not?**

H6 found our SPI3 peripheral + pin registers byte-identical to stock at the
CONFIG_ENABLE instant. If that snapshot is complete, our AF path should behave like
stock's — but it walls. So either (a) the difference is DYNAMIC (sequencing/timing
of the AF transfers, invisible to a static register snapshot — e.g. stock streams
the upload via DMA with no inter-byte idle, where we poll DT with gaps; H3 touched
this but not on a validated readout), or (b) H6's snapshot missed something.

**Crucially, shipping is unaffected:** the bit-bang path (coldtrace) already
cold-boots to a live scope and is the shippable config. The AF-hardware-SPI wall is
now an *understanding* question (and a potential faster-config path), not a blocker.

## 5. Tooling added this session (all in-tree)

- `fpga_spi3_bus_reacquire()` + `fpga busreacquire` (EXP-31): take the bus back
  after a bus-released boot.
- `fpga configbb` + `guest-bringup-bb`: fire bit-bang config on demand from a build
  where hardware-SPI is also on-demand — the on-board A/B rig.
- `FPGA_CONFIG_A_HWSPI` + `guest-coldtrace-hwspi`: hardware-SPI /2 config inside the
  live-readout pipeline (built for the write-rate premise; premise refuted).

## 6. Next (candidates, cheapest first)

1. **DMA vs polled, on a validated readout.** Stock sets CTRL2=0x03 (RXDMAEN+
   TXDMAEN) and *may actually stream via DMA*; we poll DT (gapped). Re-run H3
   properly: drive the upload via DMA and read the wall anchored. This is the
   leading "dynamic AF difference" candidate.
2. **`FPGA_CONFIG_B_FAITHFUL`** (already in-tree): bit-bang with stock's EXACT
   framing (WITH 0x05, 100 ms gaps, no reads). If it still succeeds, framing is
   fully excluded and only GPIO-vs-AF electrical drive remains; if it fails, the
   V0.4 no-05/reads framing matters after all.
3. **H7 part 2** (the co-signal / stock-boot LA) drops in priority: bit-bang
   succeeds with no special off-SPI co-signal, so a co-signal is not required for
   config; stock may use one but it is not the only door.
