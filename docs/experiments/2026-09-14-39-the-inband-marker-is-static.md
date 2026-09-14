# EXP-39 — the in-band 0x80 "data-ready marker" is a static bank bit, not a gate

- **Date:** 2026-09-14
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace-noch2` (the EXP-38 control image — CH2 arm off is
  irrelevant here; the acq task and shell paths are the same in every coldtrace
  build), branch `bench/2026-09-14`
- **Status:** **REFUTED** as a data-ready source. Bench probe, shell-driven,
  no script file (raw log in the session scratchpad; numbers below are the
  complete record).

## 1. Problem

EXP-30 established that PC0 has never edged on this unit and proposed the
in-band `0x80` byte, clocked out during the read opcode, as "the FPGA
announcing data-ready" and the viable gate source for M3. Is that byte
actually a data-ready flag — does it change state with capture completion?

## 2. Hypothesis

If the byte flags "a capture completed since your last read", then at a
timebase whose capture period is far longer than the read cadence it must
read **not-ready on most reads**: at code `0x14` (500 S/s, 2.05 s per
1024-sample buffer) with reads ~60–100 ms apart, the marker should be set on
at most one read in twenty. If it is the same value on every read regardless
of timebase, cadence and re-arm, it carries no timing information and cannot
gate anything.

## 3. Procedure

- JDS6600 1 kHz 3 Vpp into CH1 (left from EXP-38); range 5.
- Acq-task view: `status` sampled 16× at ~60 ms spacing, reading the
  `acq hdr CH1/CH2` fields (the three bytes the acquisition task captured on
  its most recent 0x04 / 0x05 read) and `SPI3 OK` (advancing = reads are
  live), at timebase `0x14` then `0x10`.
- Shell view: `spi3 opread 04 1026` (stats, no dump) 20× consecutive at
  `0x14` (~100 ms spacing), then 8 alternating 04/05 pairs; repeat at `0x10`.
- Re-arm arm: `fpga rearm on` (readback confirmed "acq re-arm ON"), status
  sampled 20× at `0x14`, then `fpga rearm off`.

## 4. Control

The readback distinguishes values: in the same samples the CH1 header is
`80 00 00` and the CH2 header is `00 00 00`, and `SPI3 OK` advanced by 3–4
per sample (live reads). So the instrument reports the bit, and reads are
happening; it is the bit that does not move. There is no positive control
for a *transition* on our firmware — nothing we can do has ever made it
change — which is precisely the finding. The only place the bit has ever
been seen to change is stock's June capture (consecutive 0x04 reads
alternating `00`/`80`).

## 5. Results

| view | timebase | re-arm | reads | CH1 byte0 | CH2 byte0 |
|---|---|---|---|---|---|
| acq task (`status`) | `0x14` (2.05 s/buffer) | off | 16 samples, OK +53 | `80` ×16 | `00` ×16 |
| acq task | `0x10` (82 ms/buffer) | off | 16 samples, OK +57 | `80` ×16 | `00` ×16 |
| acq task | `0x14` | **on** (readback) | 20 samples, OK +53, timeouts 0 | `80` ×20 | `00` ×20 |
| shell `opread` | `0x14` | off | 20 consecutive 04 | `00` ×20 | — |
| shell `opread` | `0x14` | off | 8 pairs | `00` | `00` |
| shell `opread` | `0x10` | off | 12 consecutive 04 | `00` ×12 | — |
| shell `opread` | `0x10` | off | 8 pairs | `00` | `00` |

Side observation from the `0x14` shell run: across 1.5 s the op04 mean
drifted 83 → 76 and the span collapsed 152 → 66 → 3, i.e. the 1 kHz tone
(exactly 2× fs at 500 S/s) aliased to DC as the buffer progressively refilled
at the new rate — a direct, incidental view of the live rotating buffer that
EXP-29 measured.

## 6. Blind spots

- The shell's `opread` window never showed `80` even on 0x04, where the acq
  task always does. Either the shell path does not report the byte clocked
  during the opcode itself (its header is the bytes *after* the opcode), or
  the bit depends on something the acq task's cadence does that the shell's
  does not. Not resolved here; it does not affect the conclusion, because
  the acq-task view alone is 52 reads with no transition.
- Not tested on stock. Stock's alternation is from the June wire capture,
  not from this unit under stock today.

## 7. Conclusion

- **Established:** the first byte of a 0x04 read is `0x80` and of a 0x05
  read is `0x00` on every read, at capture periods of 82 ms and 2.05 s, with
  and without re-arm. It is the "stuck bank bit" the June capture already
  named, and it distinguishes the two read opcodes, not two capture states.
- **Excluded:** the in-band marker as a data-ready gate. EXP-30's candidate
  is withdrawn. The acq task's acceptance filter (`marker || varies`) still
  works, but only because the `varies` term carries it.
- **NOT excluded:** that stock's alternating bit reflects a bank swap our
  firmware never triggers. If the FPGA double-buffers, stock's `00/80`
  alternation on consecutive 0x04 reads says stock reads the two banks in
  turn; ours always reads the same bank — the one being written, hence the
  seam. What flips the bank is unknown; re-arm alone does not (this file).
- **Follow-up (EXP-40):** stock's op-01 handler is gated on
  `tbl[0x0804D833 + tb] + 0x32` elapsed since the last arm — a per-timebase
  **time** table (u8: 9 / 21 / 41 / 82 at codes 0x10–0x13, ≈ 1024/fs in
  10 ms units), not a status bit. EXP-29 re-armed and read ~30 ms later at
  every timebase, i.e. mid-capture. A settle of ≥ 1024/fs after the re-arm
  is the cheapest untested interlock and is the next experiment.
