# The acquisition record seam — root cause on paper

*Written 2026-09-12. **Analysis only. No hardware was used. No firmware was
changed.** Every claim below is either (a) read off the source, with file:line,
or (b) computed from `reverse_engineering/captures/exp22_frames_preguard.npz`,
the 80 frames EXP-22 saved on 2026-09-03. Nothing here is a new measurement,
and where a number would need the bench to settle it, it is labelled
**UNTESTED**.*

---

## 0. Summary for someone who reads one paragraph

The seam is not "stale data at the record edges". The acquisition record is a
**rotation of a live circular buffer**: the FPGA writes its 1024-sample capture
memory continuously, our read always starts at address 0 with no interlock, so
the record's chronological origin sits at the write pointer, not at index 0.
When the pointer happens to be near address 0 (which it usually is, because the
read is paced by the PC0 data-ready edge) the rotation is small and shows up as
a discontinuity near the record's edges — which is exactly the "head 32–96 /
tail 864–928" pattern EXP-22 reported. This is a **known-and-written-down**
defect: `fpga.c:3280-3306` already says stock's acquisition loop is *gate →
read → re-arm* and that "our acq task has always done the middle step only",
and predicts "a buffer being read while it is still being written". That
comment was written 2026-08-17 and never connected to EXP-22.

The one bench command that tests it, with no reflash and no new code:
**`fpga rearm on`**, then re-run the seam census.

---

## 1. What is established

### 1.1 From the source (verifiable without hardware)

**The read is one uninterrupted CS window, 1026 bytes, polled.**
`fpga_warmtest_read_channel()` (`firmware/src/drivers/fpga.c:3475-3504`)
asserts CS, transfers the opcode (`fpga.c:3478`), transfers exactly one dummy
byte (`fpga.c:3481`), then loops 1024 times (`fpga.c:3491`) applying
`FPGA_ADC_OFFSET` per sample, then deasserts CS (`fpga.c:3502`). There is no
chunking, no DMA, no mid-window CS pulse, and no second transaction. Byte
transfer is `spi3_xfer()` (`fpga.c:374-391`), a stalling two-poll loop.

**The two channels are two separate windows, back to back.**
`fpga.c:3600-3601` — `0x04` into the CH1 staging buffer, then `0x05` into CH2,
with nothing between them.

**The record that the UI and `spi3 frame` see is a byte-for-byte copy of what
that loop read.** Reads land in staging (`fpga.c:2910-2925`), and
`fpga_acq_frames_commit()` (`fpga.c:2928-2936`) `memcpy`s both channels into
the published buffers between two increments of `acq_frame_gen`. Nothing
between the SPI loop and the published buffer can reorder, resample or splice.

**Read pacing.** The task waits for a PC0 falling edge counted by `EXINT0_IRQHandler`
(`fpga.c:3441-3446`); the wait budget is `FPGA_AUTO_TRIG_WAIT_MS = 25`
(`fpga.c:171-173`) in AUTO and `FPGA_NORMAL_TRIG_WAIT_MS = 300` (`fpga.c:174-176`)
in NORMAL, polled at 1 ms granularity (`fpga.c:3568-3571`). On an AUTO timeout
the task reads the buffer anyway after `FPGA_AUTO_CADENCE_MS = 30`
(`fpga.c:168-170`, used at `fpga.c:3581`). After the pair it delays 10 ms
(`fpga.c:3657`).

**The interlock stock has and we do not.** `fpga.c:3280-3306` (comment above
`acq_rearm_enable`) records stock's loop as *gate on the timebase (op 0x01
blocks until enough samples have accumulated) → read the 0x04/0x05 pair →
re-arm by writing reg 0x01*, decoded twice independently. Our task does the
middle step only. `acq_rearm_enable` defaults **OFF** (`FPGA_ACQ_REARM_DEFAULT 0`,
`fpga.c:3307-3310`); when on, `fpga.c:3651-3652` writes reg 0x01 after each
pair. That comment already names the expected consequence: "That is a buffer
being read while it is still being written."

**Nothing can preempt the read for long.** Task priorities: `key` 4
(`main.c:1037`), acq `fpga` 3 (`fpga.c:5637`), `usb_dbg` 2
(`usb_debug.c:7506`), `display` 1 (`main.c:1036`). The only task above the acq
task is `vInputTask`, which blocks on a queue with a 100 ms timeout and does a
`health_checkin()` — microseconds. The TMR3 button-scan ISR is microseconds.

### 1.2 From the EXP-22 frames (computation on data already in the repo)

Method: model each record as a sinusoid and compute the **circular** second-order
recurrence residual `x[k+1] − 2cos(ω)·x[k] + x[k−1]`, which is ~0 for a
contiguous sinusoid and spikes at a single sample where the record is spliced.
Outliers scored by robust z (MAD), threshold z ≥ 12. Circular, so the wrap
1023→0→1 is examined too — the earlier analyzer
(`scripts/exp22_seam_analysis.py`) used 64-sample blocks on a linear record and
therefore could neither localise a seam better than ±32 samples nor see one at
the wrap at all.

Results over 80 frames × 2 channels:

| Observation | Value |
|---|---|
| Frames with at least one CH1 seam | **27 / 80** |
| Frames with exactly **one** circular seam | 12 |
| Frames with exactly **two** circular seams | 15 |
| Distinct seam indices seen (both channels) | 0, 1, 7, 13, 42, 53, 55, 88, 89, 94, 95, 98, 100, 101, 102, 107, 108, 880, 881, 886, 919, 925, 930, 931, 937, 938 |
| Non-trivial seams (index > 3) inside the wrapped arc [880,1023] ∪ [0,108] | **44 of 44** |

**(E1) The seam index lives in a narrow wrapped arc around index 0.** 44/44
non-trivial seams fall in an arc of width ~253 of 1024 (24.7%). Under a uniform
null the probability is 0.247⁴⁴ ≈ 10⁻²⁷. Nothing ever lands in [109, 879].
EXP-22's "head 32–96" and "tail 864–928" are the two halves of **one** arc
straddling index 0.

**(E2) Single-seam frames are exact rotations of one contiguous capture.**
Rolling such a record so the seam lands at index 0 and then testing it as a
*linear* (non-circular) sinusoid gives a max residual z of **3.3–6.8** — i.e.
clean, below the z ≥ 12 detection threshold and comparable to a seam-free frame.
Seven frames tested (six at 200 Hz, one at 1 kHz). A rotation means the record
contains all 1024 samples of one capture, in order, starting at the wrong index.

**(E3) The rotation is invisible at 500 Hz / 1 kHz / 2 kHz and glaring at
200 Hz — and that is arithmetic, not luck.** A rotation's discontinuity is a
time jump of exactly one buffer depth, 1024 samples, so its phase size is
`frac(1024·f/fs)`. De-rotated records give the period directly: 62.50 samples at
200 Hz, 25.00 at 500 Hz, 12.50 at 1 kHz. So 1024 samples is 16.384 periods at
200 Hz (0.384 cycle ⇒ 24 samples of apparent jump, very visible) but 40.96 and
81.92 periods at 500 Hz and 1 kHz (0.04 and 0.08 cycle ⇒ ~1 sample, invisible).
That fully explains the otherwise odd per-scenario detection rate:
200 Hz **8/8**, 500 Hz scenarios 0–6/8, 1 kHz **1/8**, 2 kHz **0/8**.
**The corollary matters: every 500 Hz, 1 kHz and 2 kHz record in this data set
may be rotated too, and the test used cannot see it.**

**(E4) Two-seam frames are a genuine splice, and the gap is NOT one buffer
depth.** Measuring the fundamental's phase on either side of the seam (3-sample
guard bands, period taken from the majority run so the seam cannot bias it):

| drive | n | \|phase jump\| | sd |
|---|---|---|---|
| 500 Hz | 26 | **0.479 cycle** | 0.003 |
| 200 Hz | 4 | **0.198 cycle** | 0.010 |

Forcing the gap to 1024 samples (back-to-back laps, i.e. a pure rotation) leaves
an rms residual of **0.434 cycle**; the best-fit gap leaves **0.020 cycle**.
A 1024-sample gap is refuted by a factor of 22.

**(E5) The gap is only determined modulo the drive frequencies.** 200 Hz and
500 Hz are both harmonics of 100 Hz, so the joint constraint has an alias lattice
of `fs/100 Hz` ≈ 125 samples. The gap `C` satisfies **C ≡ 12.5 (mod 125.0)
samples** — every candidate fits equally well (rms 0.0199 for all of them):

| C (samples) | C − 1024 | in ms at 12,500 S/s |
|---|---|---|
| 387.5 | −636.5 | −50.9 |
| 512.5 | −511.5 | −40.9 |
| 887.5 | −136.5 | −10.9 |
| **1012.5** | **−11.5** | **−0.92** |
| 1137.5 | +113.5 | +9.1 |

**EXP-22's "~33 ms age gap ≈ one acq read cadence" is not established by the
data.** 33 ms is 412 samples, which is not on the lattice; 387.5 (31.0 ms) and
512.5 (41.0 ms) are. The phase method cannot distinguish them, and with a single
frequency family it never could. See §7.

**(E6) The seam boundary is anchored in TIME, not in address — but only in the
two-seam frames.** Comparing CH1's seam index with CH2's in the same frame:

| frame class | n | CH2 index − CH1 index |
|---|---|---|
| two-seam (splice) | 14 | **+6.1**, sd 0.9, range +5…+8, **all positive** |
| one-seam (pure rotation) | 6 | **0**, exactly, every time |

This split is the strongest single structural result in the data. A rotation is
a property of the *address space* — both channels wrap at the same address,
consistent with one write-address counter driving both channel memories — so the
displacement is 0. A splice boundary is "where the writer had reached **when we
read that address**", and CH2 is read one channel-read-period later, so its
boundary has moved on. Taking the fill rate at code 0x10 (≈12,500 S/s), +6.1
samples implies **one channel read ≈ 490 µs, ≈ 480 ns/byte** — about 3.6× the
133 ns/byte wire time at SPI /2, which is the right order for the polled
`spi3_xfer` loop plus the per-sample offset/clamp. *That derived number is an
inference from the seam data, not a timing measurement — **UNTESTED**.*

**(E7) The splice is not an MCU RAM race.** `exp22_stability.py:231` asserts
`coherent == 1` on every frame and run 1 passed that check, so every saved frame
was copied between two equal even values of `acq_frame_gen` — i.e. no
`fpga_acq_frames_commit()` memcpy overlapped the shell's copy. The splice
therefore exists in the bytes that came off SPI3.

**(E8) The gap is the same on both channels.** In every two-seam frame CH1's and
CH2's phase jumps agree to within 0.06 cycle (e.g. amp_4Vpp fr1: −0.483 /
−0.480). Whatever separates the two runs is one physical interval, not a
per-channel artefact.

**(E9) Amplitude-independent.** Seams appear at 1 Vpp through 4 Vpp with the same
phase magnitude; the run-1 "amplitude dependence" EXP-22 noticed was, as it
suspected, sampling luck.

**(E10) By-product, not load-bearing.** The de-rotated records give periods of
exactly 62.50 / 25.00 / 12.50 samples at 200 / 500 / 1000 Hz, implying
**fs ≈ 12,500 S/s at code 0x10** against `scope_timebase.c`'s measured 12,490.0 —
a 0.08% difference, and it lands on the round ladder value CLAUDE.md deliberately
declined to write into the table. This assumes the JDS6600's frequency is right,
which has never been checked against a reference here, so it is a corroborating
coincidence and nothing more. It is **not** evidence for editing the table.

---

## 2. The readout path, end to end

```
FPGA capture memory  ──0x04/0x05, one 1026-byte CS window each, /2, polled──►
    fpga_warmtest_read_channel()            fpga.c:3475   (2 bytes dropped, 1024 kept)
      └─► acq_stage_ch1 / acq_stage_ch2     fpga.c:2910   (pvPortMalloc, fpga.c:5616)
            └─► gate: marker || varies, and spi3_hw_timeouts unchanged   fpga.c:3627
                  └─► fpga_acq_frames_commit()  fpga.c:2928  memcpy under acq_frame_gen
                        └─► fpga.ch1_buf / ch2_buf   fpga.h:157-158  (FPGA_ADC_BUF_SIZE 1024, fpga.h:30)
                              ├─► scope_ui renderer, trigger search from SEAM_GUARD=128  scope_ui.c:852
                              ├─► `spi3 frame`  usb_debug.c:3562-3595   (RAM only, generation-checked)
                              └─► `spi3 read`   usb_debug.c:3497-3537   (RAM only, snapshot-then-print)
```

Two shell paths do **not** go through the acq task and read SPI3 directly:
`spi3 opread` (`usb_debug.c:5691-5729`) and `spi3 acqread`
(`usb_debug.c:5630-5658`). Both are separately mis-framed — see §6.

### Where 32–96 and 864–928 come from arithmetically

They do not come from 1026 − 1024. They are the write pointer. The read is
started by a PC0 data-ready edge; the edge poll has 1 ms granularity
(`fpga.c:3568-3571`) plus scheduling latency, so by the time address 0 is
clocked out the writer has advanced by *latency × fs* samples. At 12,500 S/s,
1 ms is 12.5 samples, so a 0–8 ms latency puts the pointer anywhere in
[0, ~100] — the head cluster. The tail cluster is the same thing on the other
side: an AUTO free-run read (`fpga.c:3573-3581`) that fires shortly **before**
the fill completes leaves the pointer at [880, 1023]. The arc is centred on 0
because the read is paced to the fill boundary; it is wide because the pacing is
tick-granular.

The seam is at a *sample* index, not a *byte* index, so the 1026/1024 header
question is orthogonal — see §4 row (e).

---

## 3. Hypothesis table

"Distinguishing prediction" means: something this hypothesis predicts that no
other surviving hypothesis on this list predicts. A hypothesis with none is
marked as such and is useless until someone finds one.

| # | Hypothesis | Status | Distinguishing prediction |
|---|---|---|---|
| **H1** | **Live circular capture memory, read from a fixed address with no interlock.** Record = rotation by the write pointer W; a second seam appears whenever consecutive laps are not back-to-back. | **LEADING** — fits E1, E2, E3, E6, E8, and matches `fpga.c:3280-3306` | W is a *time* quantity: CH2's boundary must trail CH1's by exactly one channel-read period (E6, +6.1 at 12.5 kS/s), and that displacement must scale **linearly with fs** across timebase codes — ≈24 samples at code 0x0E — while the *read* duration in seconds stays put. Nothing else on this list predicts a channel-to-channel displacement that is zero for rotations and non-zero for splices in the same data set. |
| **H1b** | H1 plus dead time / overlap between laps, which is what makes two-seam frames two-seam. | Required by E4 (1024 refuted at 22:1) | The gap C is a property of the **FPGA's capture cycle**, so in SAMPLES it should be roughly timebase-independent; in SECONDS it should scale by 1/fs. The competing "gap is an MCU interval" reading predicts the opposite. One ramp-drive session (§7 step 4) measures C absolutely and settles it. |
| **H2** | The FPGA's fill itself stalls mid-capture (writes 0..W−1, waits, resumes). | Not excluded | The seam would be baked into the memory, so **reading the same static buffer twice must reproduce the same S with the same bytes**. Under H1 a second read of an unchanged buffer has a *different* S (or none). Directly testable — §7 step 5. |
| **H3** | MCU preempted mid-read; the FPGA keeps writing while our clock is stopped. | **Strongly disfavoured** | Would require a task above priority 3 to run for milliseconds. The only one is `key` (`main.c:1037`), which blocks on a queue and does a health check-in. Distinguishing prediction: hold a button down through a capture run and the seam rate must jump. If it doesn't, H3 is dead. |
| **H4** | Readout address counter not reset per window, so the record is rotated by a *stale read pointer* rather than the write pointer. | Alive, and **indistinguishable from H1 on rotation alone** | H4 is address-anchored, so it predicts CH2's rotation index = CH1's **always**, including in two-seam frames. E6 shows +6.1 in the two-seam frames, so H4 cannot be the whole story — but it could co-exist and would explain the rotations. Distinguishing prediction: under H4 the rotation offset should be *sticky* across consecutive reads (it advances only by what the previous read left behind); under H1 it re-randomises with the read latency. |
| (a) | Rolling/circular buffer read across the write pointer | This is H1. Confirmed in its rotation form (E2). | — |
| (b) | Read split into chunks, engine advances between chunks | **REFUTED as written** | The read is one CS window with no chunking (`fpga.c:3475-3504`). The only way to chunk it is preemption, which is H3. |
| (c) | Off-by-N in unpacking ("our reads are off by one byte") | **REFUTED as the cause** | A constant shift moves the whole record; it cannot create an *internal* discontinuity, and it certainly cannot create one whose index varies frame to frame across a 250-sample arc. A real off-by-N does exist elsewhere — §6 — but it is not this. |
| (d) | CH1 and CH2 read at different times, so they are not contemporaneous | **True, and harmless here** | The two windows *are* ~490 µs apart, but when the memory is static both windows return the same time span, so a clean record's CH1/CH2 alignment is correct. The +6.1 of E6 is where the *splice* lands, not a sample skew. No prediction that bears on the seam. |
| (e) | Header/status bytes in the 1026 vs 1024 difference mis-stripped | **REFUTED as the cause** | Constant shift again. The acq path drops exactly 2 (`fpga.c:3478,3481`), which is what stock's decoded handler does. |
| (f) | MCU RAM race between the acq commit and the shell snapshot | **REFUTED** | E7: `coherent == 1` was asserted on every saved frame (`exp22_stability.py:231`). |
| (g) | Ping-pong of two capture memories, source switching mid-read | **REFUTED** | Predicts a **fixed** switch index. Observed S spans 7…938. |

---

## 4. What the existing data already rules in or out

The single most useful thing found here is that **the answer was already latent
in `exp22_frames_preguard.npz`** and needed no bench time:

**Ruled IN**
- The record is a rotation of a live buffer (E2, exact; seven frames come out
  clean after de-rotation).
- The rotation index lives in a narrow arc around 0 (E1, 44/44).
- The splice boundary is time-anchored, +6.1 samples per channel-read (E6).
- Consecutive laps are sometimes not back-to-back (E4; 1024 refuted at 22:1).

**Ruled OUT**
- A fixed off-by-N or header mis-strip as the cause (c, e).
- An MCU RAM race (E7).
- A fixed ping-pong switch point (g).
- "One acq read cadence ≈ 33 ms" as an *established* gap: 412 samples is not on
  the alias lattice (E5). It is not refuted either — 387.5 samples (31.0 ms) is
  a candidate. The claim as written in
  `docs/experiments/2026-09-03-22-display-stability.md` and the devlog was a
  plausible reading of a modular measurement, not a determination.
- "Stale head". In the W-small frames the head run is the **newer** data and the
  long body is the older lap. The word "stale" points the reader at the wrong
  end of the record.

**Ruled NEITHER — and this is the important correction to EXP-22**
- EXP-22 reported seams in 27 of 80 frames. That is the **detection** rate, not
  the occurrence rate. E3 shows the rotation is phase-invisible whenever
  `1024·f/fs` is near an integer, which is true for every 500 Hz, 1 kHz and
  2 kHz scenario in the set. The honest statement is: **the rotation may be
  present in all 80 frames and the instrument could only see it at 200 Hz.**
  This is the project's recurring failure shape — a measurement used to bound
  something it could not detect — and it appears here in our own EXP-22 numbers.

---

## 5. The single most likely root cause

> **The acquisition read is not interlocked with the FPGA's capture cycle.**
> The capture memory is written continuously; we clock it out starting at
> address 0 regardless of where the writer is; so the record's time origin is
> the write pointer. Usually that is a harmless rotation by a few tens of
> samples — harmless to Vpp, Vrms and FFT *magnitude*, fatal to anything that
> assumes index 0 is t=0, and visible on screen as a step near the edges.
> Occasionally the two laps either side of the pointer are not back-to-back, and
> then it is a real splice with a real gap.

Stock does not have this problem because stock closes the loop: gate on reg
0x01, read the pair, re-arm reg 0x01. We implemented the middle step only, and
our own source says so at `fpga.c:3280-3306`, including the sentence "That is a
buffer being read while it is still being written."

Confidence: **high that the record is a rotation of a live buffer** (E2 is an
exact structural test, not a correlation, and E6's zero-vs-+6 split is
independent corroboration). **Medium that the fix is the missing re-arm** — the
re-arm writes reg 0x01 *after* the read, which is two of stock's three steps;
the *gate before the read* is the part that actually guarantees a static buffer,
and our task substitutes a PC0 edge wait for it. **Low on the absolute size of
the splice gap** — it is determined only modulo 125 samples and nothing in the
existing data breaks the tie.

---

## 6. Secondary defects found while tracing the path

None of these cause the seam. All are checkable from source, and all are of the
shape this project keeps paying for.

1. **`spi3 opread ... dump` prints inside the CS window.** `usb_debug.c:5698`
   asserts CS, and the `if (dump)` block at `usb_debug.c:5709-5713` calls
   `usb_debug_printf` **inside the sample loop**, before CS is released at
   `usb_debug.c:5715`. `usb_send_bytes()` waits for each 64-byte USB packet and
   will `vTaskDelay(1)` (`usb_debug.c:158-180`). A dumped 1026-byte window is
   therefore clocked out over tens to hundreds of milliseconds with CS held low.
   This is the **same bug** that was found and fixed in `spi3 read` on
   2026-08-19 — and the comment recording that fix
   (`usb_debug.c:3510-3527`) says "against 7 of 12 for `spi3 opread`, which
   already snapshots". **`spi3 opread` does not snapshot when dumping**; it
   snapshots only `first16`. `bench.py`'s `opread()` always passes `dump`
   (`scripts/bench.py:455`). EXP-17's "6/12 opread records torn" and CLAUDE.md's
   "opread clocks at /256 and takes ~35 ms" both attach to this; the baud is
   whatever `spi3_set_br` last left, which after the arm block is /2
   (`fpga.c:4202`), so the time is USB print time, not clock time.
2. **Three readers, three different byte strips.** The acq task drops 2 and
   keeps 1024 (`fpga.c:3478,3481,3491`) — stock's shape. `spi3_opread_window`
   drops 3 (`usb_debug.c:5699-5701`) and then clocks `len` more, so
   `spi3 opread 04 1026` is a **1029-byte** window whose payload starts one
   sample late. `cmd_spi3_acqread_one` drops 3 and keeps 1023
   (`usb_debug.c:5639-5641`) — the pre-`ce22b49` framing that was already
   corrected everywhere else. And `bench.py` then drops `STOCK_HEADER_DROP = 2`
   more (`scripts/bench.py:121,455,461`), on the assumption that the dump
   includes the opcode echo — it does not. Net: **a `bench.py` opread array
   starts 3 samples later than the acq task's record and runs 3 samples past the
   end of the 1024-sample buffer.** The BLIND SPOT note at `bench.py:114-120`
   flags the uncertainty; the arithmetic resolves it from source. UNTESTED on
   the wire, but it does not need the wire — it is three constants in two files.
3. **`SEAM_GUARD = 128` (`scope_ui.c:852`) is well sized for what the data
   shows** — the largest head-side seam index observed is 108 — but it is sized
   against a *detected* distribution that E3 shows is incomplete. It also does
   nothing for the tail arc; it works today only because the drawn window
   [off, off+320] with off ≥ 129 ends well short of 880.

---

## 7. Bench procedure — one session, discriminates the survivors

Uses only commands that exist today (`usb_debug.c` command table,
`usb_debug.c:7029-7180`). **No firmware change is required for steps 0–3, 5 and
6.** Step 4 needs a ramp source, not a code change.

Build: `guest-coldtrace-ch2` (what EXP-22 used; `firmware/Makefile:978`).
Signal path: generator → CH1 **and** CH2, as in EXP-22.

### Step 0 — record the state you are about to perturb
```
status
fpga scope timebase
fpga rearm
fpga scope softtrig
```
`status` prints `PC0 edges`, `SPI3 OK`, `SPI3 timeouts` and the last acq headers
(`usb_debug.c:659-677`). Take `PC0 edges` twice, ten seconds apart, to get the
data-ready rate — the model in §5 says that rate is what the seam arc is
centred on.

### Step 1 — make the rotation visible before measuring anything
Drive **201.2 Hz** at code 0x10, not 500 Hz. The criterion is
`frac(1024·f/fs) ≈ 0.5`, which maximises the rotation's phase size; at 500 Hz it
is 0.04 and the rotation is invisible (E3). Then:
```
fpga scope timebase 10
spi3 frame            # x64, scripted
```
Run the records through the circular-recurrence detector (§1.2; the script that
produced every number in this document is reproduced in §9).

- **Prediction, H1/H4:** ~every record carries exactly one circular seam, index
  varying inside an arc around 0, and de-rotating by that index yields a clean
  linear sinusoid.
- **If instead most records are clean at 201.2 Hz**, the rotation is not
  universal and E3's corollary is wrong — say so loudly, because then §5 is
  wrong too.

### Step 2 — replicate the zero-vs-+6 split (H1 vs H4)
Same 64 frames, compare CH1's seam index with CH2's.
- **H1:** one-seam frames give Δ = 0, two-seam frames give Δ = +6 ± 1.
- **H4 alone:** Δ = 0 in *every* frame.

### Step 3 — timebase scaling of Δ (H1's quantitative claim)
```
fpga scope timebase 0e
```
and drive 804.8 Hz (4× the step-1 tone, preserving cycles per record). Capture
64 more `spi3 frame`.
- **H1:** Δ scales with fs — **predict Δ ≈ 24 samples** at code 0x0E, because the
  read duration is fixed in seconds. This is a sharp number; if Δ stays near 6
  the time-anchoring story is wrong.
- Also re-measure the arc width: it should scale the same way.

### Step 4 — the absolute gap, with no modular arithmetic (settles E5)
Switch the generator to a **ramp / sawtooth, ~5 Hz, 2 Vpp** into CH1. At
12.5 kS/s one ramp period is 2500 samples, longer than the record, so the
instantaneous level is a monotone clock: every sample's value encodes its
absolute position in time, and the gap at a seam can be read off as a level step
divided by the ramp slope. Capture 64 × `spi3 frame`.
- **Rotation:** the level step at the seam corresponds to exactly −1024 samples.
- **Splice:** it gives C directly, in absolute samples, settling 1012.5 vs 1137.5
  vs 387.5 in one shot — and therefore settling whether the gap is an FPGA
  capture-cycle property or an MCU-interval one.
- This is the step that makes the "one read cadence" claim either true or false
  instead of unfalsifiable. Nothing else in this plan can do that: a periodic
  drive can never resolve a gap beyond its own period.

### Step 5 — is the seam in the memory or in the reading? (H2)
With the ramp still connected, repeat `spi3 acqread` ten times back to back and
compare the printed `first16` and `min/max/mean/span`
(`usb_debug.c:5652-5658`).
- **H1/H4:** the sixteen bytes change every time — the buffer is live.
- **H2:** the buffer is static between fills, so consecutive reads inside one
  dwell return identical `first16`, and if a stall is baked into the memory the
  seam survives re-reading unchanged.
- Caveat, stated in advance: `spi3 acqread` uses the wrong strip (§6.2). For a
  *comparison between two of its own reads* that does not matter.

### Step 6 — THE TEST. Close the loop stock closes.
```
fpga rearm on
```
then repeat step 1 exactly (201.2 Hz, code 0x10, 64 × `spi3 frame`), then
```
fpga rearm off
```
and repeat again — **A/B/A**, one boot, one probe, one signal, per the discipline
`fpga.c:3299-3306` set out when the toggle was added.
- **Prediction if §5 is right:** with the re-arm on, the write pointer is
  re-synchronised to our read cadence, so the seam-index distribution collapses
  (a single repeated index, or no seams at all), and the two-seam class
  disappears or becomes rare.
- **If nothing changes**, the interlock we are missing is stock's *first* step —
  gating on reg 0x01 before the read — not the re-arm after it, and the fix is a
  firmware change rather than a toggle.
- Either outcome is worth the session: this is the only step that can turn the
  root cause into a fix on the same evening.

### Step 7 — negative control for H3 (two minutes)
Repeat step 1 while pressing a front-panel button continuously. `vInputTask`
(priority 4, `main.c:1037`) then actually runs work instead of blocking.
- **H3:** seam rate rises sharply.
- **H1:** no change.

### Acceptance
`scripts/exp22_stability.py` remains the acceptance vehicle for the *display*;
this session's acceptance is different and should be stated as a number before
it runs: **the seam-index spread at 201.2 Hz, code 0x10, must fall below 4
samples p95 for the seam to be considered closed**, because that is the point at
which a full-buffer FFT sees one contiguous record.

---

## 8. Blind spots — what this paper analysis could not have detected

1. **No timing was measured.** Every duration here (490 µs per channel read,
   480 ns/byte, the PC0 latency that sets the arc width) is *inferred from seam
   positions*, i.e. from the very thing being explained. Circular reasoning is
   avoided only in the sense that the inference is quantitative and falsifiable
   (step 3); it is not independent evidence.
2. **The gap C is undetermined.** Only C mod 125.0 samples is fixed by the data,
   because EXP-22 used 200/500/1000/2000 Hz, all harmonics of 100 Hz. Any
   statement of the form "the stale data is one X old" — including the one in
   EXP-22 and the one you might be tempted to write from §5 — is a choice among
   ten equally-fitting candidates until step 4 runs.
3. **The detector is blind wherever `1024·f/fs` is near an integer.** That is
   E3, and it means the 27/80 occurrence rate is a floor, not an estimate. It
   also means that if the bench re-runs at 500 Hz the answer will look better
   than it is. **Do not re-run this at 500 Hz.**
4. **Sinusoid-only.** The recurrence detector assumes a single tone; the square
   wave scenario saturates it (every edge is an outlier) and was excluded.
   Nothing here says how the seam behaves on a non-sinusoidal input.
5. **One build, one unit, one session.** All 80 frames came from bench unit #1
   on 2026-09-03 under `guest-coldtrace-ch2`. Rotation behaviour could differ
   under `fpga rearm on`, at other timebase codes, in NORMAL trigger mode, or on
   a second unit. None of that is sampled.
6. **The FPGA side is a black box here.** "Circular capture memory" is a model
   that fits the MCU-visible data. Whether the fabric actually implements a
   circular BSRAM, a linear fill with restart, or something else is a netlist
   question — `fpga-arm-register-netlist` and `gw1n2-apicula` are the places to
   settle it, and doing so would make step 4 unnecessary.
7. **Nothing here was executed on hardware.** No command in §7 has been run. The
   predictions are predictions.
8. **The `spi3 opread` dump defect (§6.1) is read off the source, not observed.**
   It says a dumped window *must* be smeared; it does not say by how much, and
   it does not prove that any specific past result was wrong. It does mean every
   past `bench.py` opread record deserves re-examination before it is cited
   again.
9. **The de-rotation test (E2) was applied to seven frames**, six of them from
   one scenario. It is an exact structural test and it passed cleanly, but it is
   seven frames.

---

## 9. Reproducing every number in this document

No hardware. From the repo root, with numpy:

```python
import numpy as np, math
D  = np.load('reverse_engineering/captures/exp22_frames_preguard.npz')
# scenario -> (drive Hz, fs from scope_timebase.c)
SC = {"baseline_500Hz_2Vpp":(500,12490.),"freq_200Hz":(200,12490.),
      "freq_1kHz":(1000,12490.),"amp_1Vpp":(500,12490.),"amp_4Vpp":(500,12490.),
      "phase_45":(500,12490.),"phase_90":(500,12490.),"phase_180":(500,12490.),
      "NEGCTL_free-run":(500,12490.),"tb_0x0E_2kHz":(2000,49930.1)}

def est_f(x, fs, f0):                       # |DFT| peak, 3201-point grid, +-4%
    x = x.astype(float); x -= x.mean(); k = np.arange(len(x))
    return max((abs(np.sum(x*np.exp(-2j*np.pi*f*k/fs))), f)
               for f in np.linspace(f0*.96, f0*1.04, 3201))[1]

def seams(x, f, fs, zthr=12.):               # CIRCULAR recurrence residual
    x = x.astype(float); x = x - x.mean()
    c = 2*math.cos(2*math.pi*f/fs)
    a = np.abs(np.roll(x,-1) - c*x + np.roll(x,1))
    med = np.median(a); mad = np.median(np.abs(a-med)) + 1e-9
    z = (a-med)/(1.4826*mad); n = len(z); out = []
    for k in np.argsort(-z):
        if z[k] < zthr: break
        if any(min(abs(int(k)-p), n-abs(int(k)-p)) <= 3 for p in out): continue
        out.append(int(k))
    return sorted(out)
```

- **§1.2 census (E1):** `seams()` over all 80 frames × 2 channels.
- **E2:** for a one-seam frame, `np.roll(x, -S)` then the *linear* residual
  `x[2:] - 2cos(w)x[1:-1] + x[:-2]`; max z came out 3.3–6.8.
- **E4/E5:** phase of the fundamental over each run with 3-sample guards,
  referred to the seam index; then scan C minimising
  `Σ ((C·ν + Δφ/2π + ½) mod 1 − ½)²`.
- **E6:** compare the >3 seam index of CH1 and CH2 per frame.

---

## 10. What to change in the record

- `docs/experiments/2026-09-03-22-display-stability.md` and
  `docs/devlog/2026-09-03-the-trigger-was-reading-the-seam.md` describe the
  defect as a "stale head" of data "about one read cadence old". Both halves are
  unsupported: the head run is the newer data, and the age is determined only
  modulo 125 samples. Suggest a correction note rather than an edit, in this
  repo's usual style.
- The MEMORY note `acq-record-edge-seam` should gain: *the record is a rotation
  of a live buffer; the rotation is phase-invisible at 500 Hz/1 kHz/2 kHz, so
  the 27/80 rate is a detection floor; the interlock stock has and we do not is
  already written down at `fpga.c:3280-3306`.*
- `bench.py`'s `STOCK_HEADER_DROP` and the `spi3 opread` dump loop (§6) want
  their own small issue; they are not this bug and should not be fixed inside a
  seam commit.
