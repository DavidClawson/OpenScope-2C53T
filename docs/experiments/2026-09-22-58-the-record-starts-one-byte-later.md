# EXP-58 — the record starts one byte later: "sample 0" was a stale byte

- **Date:** 2026-09-22
- **Unit:** bench unit #1
- **Build:** measured on v15 (`d0ce0bb`, Build Sep 22 2026 19:13:27) through `spi3 opread`; fix in v16
- **Status:** **CONFIRMED** on the wire and on v16 (Build Sep 22 2026 20:15:11)

## 1. Problem
Sample 0 of a committed record is often a stray value (13/20 raw records at 201 Hz / 0x10;
25/204 across the EXP-53/55/56 captures). It defeated two seam finders and one test during
the un-rotation work. Is it a framing error in our read, or real FPGA behaviour? The read
discards two bytes, as stock's decoded dispatch does (changed from three on 2026-08-15).

## 2. Hypothesis
Reading past the 1024th sample exposes where the FPGA's 1024-byte blocks begin and end.
- A (bad read of a real address): the stray byte belongs to the record and the block
  boundary sits between wire bytes 1 and 2.
- B (stale first byte): wire byte 2 is the tail of a previous block and the record is wire
  3..1026; then the byte at wire 2 + 1024n continues from its LEFT neighbour and breaks
  into its right one.
- Falsifier for B: bytes 1026 / 2050 break from the left, or show no boundary at all.

## 3. Procedure
JDS6600 CH1: 201.2 Hz sine, 2.0 Vpp, 0 V offset; range 5; timebase 0x10; level 0.
`spi3 opread 04 <len> dump` prints wire bytes 0–2 (`s=`) and the rest as a dump; the acq
task is parked for the read.
- Pass 1: 20 reads of 1,043 wire bytes in AUTO (acq task running between reads, so these
  mostly catch the rolling buffer).
- Pass 2 (the test): 12 reads of 2,063 wire bytes, each taken after `trigmode single` had
  committed one record, so the acq loop has stopped reading and the FPGA holds a completed
  record for our read. PC0 strobes counted across each read.

## 4. Control
An interior byte (wire 700) scored with the same left/right extrapolation test.

| control | expected | measured | passed? |
|---|---|---|---|
| wire 700 fits both neighbours | residual small both sides | L 0–4, R 0–2, 12/12 | yes |
| one handover strobe per held read | 1 | 1 on 12/12 | yes |

## 5. Results
Pass 1 (rolling): past wire 1026 the waveform continues (the buffer is being written as we
read); two reads showed a step between wire 1026 and 1027 — 1024 bytes after the stray
step between wire 2 and 3.

Pass 2 (held), residual of the boundary byte against extrapolation from each side:

| boundary byte | from the LEFT | into the RIGHT |
|---|---|---|
| wire 1026 | 0–2 on 12/12 | 2–35 (≥ 8 on 11/12) |
| wire 2050 | 0–3 on 12/12 | 17–24 on 12/12 |
| wire 2 | (no left data: the dummy) | 1–49 (≥ 13 on 9/12) |

Big steps in every held read sit at wire 3, 1027 and 2051 (plus the FPGA's seam 18–62
bytes in, as before). Log: `reverse_engineering/captures/exp58/exp58_held.log`; bytes:
`exp58_held.npy`, `exp58_wire.npy`.

## 6. Blind spots
- The content of the second pass differs from the first (the FPGA refills), so the block
  boundary is located by continuity, not by a byte-for-byte repeat.
- Why the first byte is stale at our timing but stock's code discards two is not
  explained (stock's clock divider or inter-byte timing may add the byte of latency).
- CH1 only; op 05 uses the same framing and is assumed to behave the same.
- The shell tools that read windows (`spi3 opread`, canaries) keep their own framing.

## 7. Conclusion
- **Established:** each 1024-byte block ends at wire 2 + 1024n; the record is wire
  3..1026. The acquisition read kept wire 2..1025: one stale byte at "sample 0" and the
  true last sample never read. Hypothesis B.
- **Excluded:** A (the stray byte being a real sample of the record).
- **NOT excluded:** a timing-dependent latency that would differ at another SPI divider.
- **Fix (v16):** discard three bytes and read 1024 (a 1,027-byte window).
- **Predictions for v16, written before the run:** (1) stray-sample-0 rate in committed
  records at 201 Hz / 0x10 (unrotate off) ≤ 1/20, against 13/20 on v15 with the same
  metric; (2) raw seam index = v15's minus one in distribution (seams at 4–59, not 5–60);
  (3) `exp22_stability.py --trigger-only` 17/17.

## 8. v16 acceptance (run after the predictions above)
| prediction | result | verdict |
|---|---|---|
| stray sample 0 ≤ 1/20 (v15: 13/20, same metric) | **0/20** | met |
| raw seams shift by one sample | v16 seams 0–55, v15 1–60, unpaired records | **inconclusive** — per-record seam spread exceeds one sample, so unpaired sets cannot show a one-sample shift |
| `exp22_stability.py --trigger-only` 17/17 | **17/17** | met |

A first acceptance attempt stalled in NORMAL with zero strobes. Cause: the fresh boot left
CH1 uncentred (29..81), so the trigger crossing (record 100) sat above the signal and NORMAL
correctly held; the script had skipped centring. Rerun after a host centre (DAC1 2500).
Logs: `v16_acceptance.log`, `v16_regression.log`, frames `v16_regression_frames.npz`.
