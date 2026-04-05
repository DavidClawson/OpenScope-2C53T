# The "301-byte per-channel meter cal blob" — Myth Busted

> **Update 2026-04-04 (later same day):** H1 below is also dead.
> See the "H1 Postmortem" section at the end. The decomp initially
> looked like FUN_080018A4 was a per-range DAC reference writer that
> would explain the low-Ω miss. After cross-checking against
> `STATE_STRUCTURE.md`, `function_names.md`, and the scope trigger
> analysis files, it turns out:
>
> 1. FUN_080018A4 is `gpio_mux_portc_porte` — a **scope** analog
>    frontend relay switcher that happens to also write DAC1
> 2. The DAC1 output is the **scope trigger comparator threshold**,
>    not a meter reference voltage
> 3. The cal tables at `0x20000358` are the 120-byte scope
>    gain/offset cal table, not meter calibration
> 4. Stock only calls FUN_080018A4 twice at boot, never at runtime
>    in meter mode — which is consistent with meter mode not
>    needing it, not with "we should port it"
>
> So both the 301-byte blob hypothesis AND the DAC reference
> hypothesis turned out to be wrong. The actual low-Ω cal source is
> now most likely the 411-byte FPGA internal cal exchange (cmds
> 0x3B/0x3A). See `fpga_411_byte_cal.md` for the current leading
> hypothesis.

**Status:** The claim in `CLAUDE.md` that stock firmware loads
"301-byte cal data from SPI flash per channel" for meter calibration
is **incorrect**. There is no such blob. This document explains
what the 301-byte region actually is, why the low-Ω 3.04× error
cannot be fixed by "loading cal data," and what the real next steps
are for closing that calibration gap.

## The conflicting sources

Three prior analysis files mention 301 bytes (`0x12D`) at
state offsets `0x356` and `0x483`:

| Source | Claim |
|---|---|
| `gap_functions_annotated.c:559` | "calibration_loader" — loads 301 bytes of per-channel calibration data |
| `scope_main_fsm_annotated.c:223` | `buf = &state[0x356 + ch * 0x12D]` — the **roll-mode sample buffer** (301 samples/ch) |
| `fpga_comms_deep_dive.c:430` | CH1/CH2 **acquisition buffers** at `state[0x356..0x482]` / `state[0x483..0x5AF]` |
| `core_subsystems_annotated.c:412` | Single-shot trigger copies `state[0x5B0+i] = state[0x356+i]` — i.e., roll buffer → display buffer |

All four references address the same RAM region. The interpretations
look contradictory ("calibration table" vs "roll buffer") but can be
reconciled: **the region is the oscilloscope roll-mode sample buffer**,
and the `calibration_loader` label in `gap_functions_annotated.c`
was a guess based on the only call site being at boot. The function
(`FUN_08001830`) loops 301 times writing to the destination with
`r2 = state[N] ^ 0x80` as a lookup key — that's consistent with
*pre-seeding the roll buffer with a midscale pattern*, not loading
cal data.

Either way, **the region is not meter calibration**. Meter readings
come from the FPGA meter IC as pre-scaled BCD over USART2 and never
touch this RAM region. The scope ADC path and the meter IC path are
completely separate.

## Why the low-Ω 3.04× error is NOT a missing cal load

The bench capture on 2026-04-04 showed:

- **Short (0 Ω)**: reads correctly (`00.16 Ω`)
- **147 Ω**: reads 3.04× low (shown as `48.36 Ω`, BCD `4836`)
- **3.3 kΩ, 10 kΩ, 100 kΩ**: BCD digits are within tolerance of the
  actual value — the visible errors in the bench log are
  decimal-position / unit-suffix decoder bugs (which have now been
  fixed by the `frame[6]` decoder table) not calibration drift

If the cause were a missing MCU-side cal blob, **all ranges would
be wrong**, not just low-Ω. The kΩ readings come back essentially
correct from the FPGA. This tells us the FPGA meter IC is already
well-calibrated for most ranges on its own.

## Three remaining hypotheses for the low-Ω miss

Each has a different test path:

### H1: DAC reference calibration is not being re-applied per meter range (leading hypothesis)

`FUN_080018a4` @ `0x080018A4` is not just a GPIO relay switcher — it
is the **DAC reference calibration writer**. Reading the full
function (decomp lines 2174-2257) reveals two phases:

**Phase 1 — GPIO mux (lines 2185-2234):** switch on `param_1`
(range index 0-9), set relay bits in `_DAT_40011010`,
`_DAT_40011810`, `_DAT_40011814` (PC/PE ports).

**Phase 2 — DAC reference cal (lines 2236-2254):**
```c
if (device_mode < 5) {
    // Scope mode — use scope cal tables
    puVar1 = &DAT_200003BC + param_1 * 2;  // or 0x200003A8 in calibration
    puVar2 = &DAT_20000380 + param_1 * 2;  // or 0x2000036C
} else {
    // Meter mode — use meter cal tables
    puVar1 = &DAT_20000394 + param_1 * 2;
    puVar2 = &DAT_20000358 + param_1 * 2;
}
range_width = *puVar1 - *puVar2;
baseline    = *puVar2;
DAC_DAT1    = (range_width / DAT_08001A54) * (system_state + 100) + baseline;
DAC_CR     |= 1;  // Enable DAC output
```

This writes a per-range DAC value to `0x40007408` (DAC1 data) that
biases the analog frontend used by BOTH the scope ADC path and the
FPGA meter IC. **If this is not re-written when switching between
Ω ranges, the analog reference stays at whatever was set at boot,
and the FPGA meter IC measures against a wrong bias.**

**Meter cal table locations (confirmed from line 2247-2249):**
- `0x20000358` — puVar2 base (baseline values), 2 bytes × N entries
- `0x20000394` — puVar1 base (upper-range values), 2 bytes × N entries
- Stride: 2 bytes per entry (short), param_1 ∈ {0..9} so up to 20
  bytes per table = 40 bytes total for meter cal

These are the **real** "factory cal" bytes CLAUDE.md was alluding
to when it said "6 gain/offset pairs at 0x20000358-0x20000434".
The original note conflated two adjacent cal regions and got the
size wrong — the meter portion is ~40 bytes, and the 0x20000434
endpoint is actually a different table used in scope auto-range
(see decomp lines 7402-7408).

**How it would normally get called for meter mode:** at boot, the
master init calls `FUN_080018a4(meter_state[2])` with the saved
meter mode index (per `init_function_decompile.txt:232`). **After
boot, call sites for FUN_080018a4 only appear in scope auto-range
FSM code (lines 2528, 7421, 7845, 8604).** I could not find any
runtime re-invocation of this function when the user changes meter
submode. Either:
  (a) stock firmware only supports the meter range that was active
      at boot (unlikely — meter UI clearly switches modes), OR
  (b) there's a different call path for meter mode re-calibration
      that uses a different function, OR
  (c) `DAT_20000128 & 0xf` at line 7421 is actually the meter range
      variable in some contexts, and the scope-auto-range code path
      is shared with meter range changes via a common range-index
      byte

**Our firmware**: `fpga.c:737-779` hardcodes the DCV GPIO pattern
for all meter modes and never writes to `0x40007408` (the DAC1
data register) from meter code at all. Our DAC handling is in the
signal generator path only. So we're running with either
(a) whatever DAC value was set by our boot code, or (b) the DAC
disabled entirely. Either way, the analog reference is not
range-adjusted.

**Test (requires a stock unit)**: bench the same 147 Ω resistor on
a stock 2C53T and confirm it reads correctly. If stock reads 147 Ω
cleanly, H1 is almost certainly the mechanism.

**Decomp target (no hardware needed)**: find what sets `DAT_20000128`
and confirm whether bit 0-3 of that byte is the meter range index in
meter mode. If so, we have a direct mapping from meter submode →
FUN_080018a4 param.

**Firmware target**: dump SPI flash from a stock unit and extract the
20-40 bytes of meter cal at the equivalent of `0x20000358`/`0x20000394`
(they're loaded from flash at boot into these RAM addresses). Then
port the DAC cal formula from FUN_080018a4. This is the concrete
fix path.

### H2: The FPGA needs a boot-time cal exchange we aren't sending

`CLAUDE.md` mentions a 411-byte cal exchange (137 entries × 3 bytes)
sent via commands `0x3B`/`0x3A` during boot from a table at
`0x08051D19` in the V1.2.0 binary. This is **FPGA-side** cal — it
configures the FPGA meter IC's internal correction tables.

Our boot sequence (`fpga.c`, the `usart2_send_cmd(0x00, FPGA_CMD_INIT_*)`
calls) does **not** send this exchange. If the FPGA has sensible
defaults for most ranges but relies on the 411-byte table for low-Ω
correction, that would produce exactly the symptom we see.

**Test**: extract the 411-byte table from the stock binary, replay it
over USART2 at boot via cmds 0x3B/0x3A, and re-bench.

**Decomp target**: trace where `FPGA_CMD_INIT_3B` / `0x3B` is sent,
and how the 411 bytes are paged out via the 2-byte TX frame payload.
Given our confirmed TX frame layout (only cmd_hi/cmd_lo carry
payload), sending 411 bytes requires 411 separate 10-byte frames,
~43 seconds at 9600 baud. That's consistent with the long "boot
pause" we observe on stock.

### H3: Stock firmware applies an MCU-side post-decode scale factor

Even after decoding BCD to digits, stock might multiply by a
per-range coefficient before displaying. 4836 × some factor ≈ 147
would need factor ≈ 0.0304 (1/32.9). That's an awkward number and
doesn't match any common ADC / voltage-divider ratio.

**Why this is least likely**: the kΩ readings are correct without
any MCU-side scaling in our code. If stock scaled low-Ω by 0.0304,
it would also have to scale kΩ by ~1.0 — possible but adds
complexity that no other meter IC in this class uses.

**Test**: trace the stock firmware's display path for Ω mode and see
if any float multiplication happens between the BCD decoder and the
LCD string formatter.

## Recommended order of attack (ORIGINAL — SUPERSEDED)

> See "H1 Postmortem" below. After further decomp cross-checking,
> H1 turned out to be wrong. H2 is now the leading hypothesis.

1. ~~**H1 first (cheapest)**: Find the stock Ω-mode GPIO mux table.~~
   **Ruled out.** FUN_080018A4 is `gpio_mux_portc_porte` (scope
   relay + scope trigger DAC). Its meter-mode branch writes the
   scope trigger comparator, which has no role in meter accuracy.
   Stock does not re-call it at runtime in meter mode.

2. **H2 (now the leading hypothesis)**: Extract the 411-byte table
   and add the boot-time cal exchange. This is FPGA-side cal that
   directly configures the meter IC's internal correction tables.

3. **H3**: Trace the stock display path only if H2 fails.

## H1 Postmortem — Why I was wrong

**What I thought**: FUN_080018A4 writes DAC1 (0x40007408) as a
per-range reference voltage for the analog frontend, and its
internal `device_mode >= 5` branch (meter mode) uses meter-specific
cal tables at `0x20000358`/`0x20000394`. Our firmware never writes
DAC1 in meter mode, so the reference is wrong → low-Ω reads 3.04× low.

**What's actually true** (cross-checked against `STATE_STRUCTURE.md`,
`function_names.md`, `scope_main_fsm_annotated.c`, and
`fpga_comms_deep_dive.c`):

1. **FUN_080018A4 is named `gpio_mux_portc_porte`** — the scope
   analog frontend relay switcher (PC12, PE4, PE5, PE6) plus trigger
   comparator DAC writer.

2. **DAC1 output is the scope trigger comparator threshold**, not
   a meter reference. Multiple cross-refs confirm:
   - `scope_main_fsm_annotated.c:297` — "computes a 12-bit DAC value
     for the trigger comparator"
   - `scope_main_fsm_annotated.c:641` — "the hardware trigger
     comparator tracks the calibration changes"
   - `fpga_comms_deep_dive.c:665` — "compute DAC trigger level"

3. **The cal region at 0x20000358 is the 120-byte scope
   gain/offset cal table** (6 entries × 20 bytes). Per
   `STATE_STRUCTURE.md:138`: "Calibration entry 0: gain(4B) +
   offset(4B) + params(12B)". Used by `scope_main_fsm`, indexed by
   voltage range. It's shared between FUN_080018A4's scope branch
   and its meter branch via different internal offsets, but both
   branches are reading from the *scope* cal table.

4. **There is no runtime re-call of FUN_080018A4 for meter mode.**
   The two calls at init lines 2817 and 5566 both pass `state[2]`
   (saved meter mode) unconditionally. After boot, it's only called
   from scope auto-range paths (lines 2528, 7421, 7845, 8604). If
   the DAC reference were load-bearing for meter accuracy, stock
   would re-call it when the meter submode changed. It doesn't.

**Why the kΩ-works-but-low-Ω-fails pattern made me suspect DAC**:
I reasoned that a single boot-time DAC value could affect low-Ω
more than kΩ if kΩ measurements were differential (cancelling
reference errors) and low-Ω used the reference directly. That's
plausible in the abstract, but it required the DAC to actually be
routed to the meter frontend — which it isn't. DAC1 (PA4) only
drives: (a) the scope trigger comparator, and (b) the siggen BNC
output in siggen mode. Neither affects meter readings.

**Lesson for future decomp work**: when a function has a complex
internal branch that looks relevant ("device_mode >= 5 → meter"),
verify what the function's OUTPUTS physically connect to before
assuming the branch is load-bearing. In this case, the `else` branch
exists because the same function preloads some shared state, but
the downstream consumer (trigger comparator) is only active in
scope mode.

## Corrections needed to other docs

- **`CLAUDE.md`**: (1) Remove the claim that "301-byte cal data
  loaded from SPI flash per channel." (2) Remove the recently-added
  "Meter DAC reference calibration" note — that was H1 and is wrong.
- **`gap_functions_annotated.c:530`**: The "calibration_loader" label
  on FUN_08001830 is speculative. Rename to `roll_buffer_preload` or
  similar, and note the uncertainty.
- **`meter_frame_capture_log.md`**: The "factory cal load" note in
  the Calibration Issue section should point to H2 (411-byte FPGA
  exchange) rather than a non-existent 301-byte MCU blob or
  DAC reference cal.
